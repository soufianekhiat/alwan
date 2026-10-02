/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Three more methods of alwan_denoise_{T}: the bilateral filter (Tomasi and Manduchi 1998)
 * twice, as OpenCV and as scikit-image compute it, and the local adaptive Wiener filter
 * (Lee 1980) as scipy.signal.wiener computes it.
 *
 * ALWAN_DENOISE_BILATERAL_OPENCV is a port of cv::bilateralFilter (OpenCV 5.0.0,
 * modules/imgproc/src/bilateral_filter.dispatch.cpp and bilateral_filter.simd.hpp), kept to
 * its float arithmetic and order of operations so the result is OpenCV's value for value with
 * IPP off, on one or three channels of 8-bit or float data:
 *
 *   - 8 bits: the colour weights a 256 x cn table, filled by OpenCV's polynomial v_exp in
 *     fours below its last four entries and by expf above; the window the pixels within a
 *     radius of round(1.5 sigma_space) (or diameter / 2), BORDER_REFLECT_101; the AVX2
 *     kernel's blocks of 32 pixels add with fused multiply-adds, the scalar tail without, and
 *     a 13-pixel window (radius 2) is summed in the order OpenCV's unrolled case loads it;
 *   - floats: the colour weight read from a 4096 x cn bin table over the image's range by
 *     linear interpolation, written one way in the AVX2 blocks of 16 pixels (a fused
 *     multiply-add on the upper entry) and another in the scalar tail; the centre pixel is
 *     left out of the window and added with weight 1 at the end, as OpenCV does.
 *
 * The OpenCV code carries this notice:
 *
 *   License Agreement For Open Source Computer Vision Library
 *
 *   Copyright (C) 2000-2008, 2018, Intel Corporation, all rights reserved.
 *   Copyright (C) 2009, Willow Garage Inc., all rights reserved.
 *   Copyright (C) 2014-2015, Itseez Inc., all rights reserved.
 *   Copyright (C) 2025, Advanced Micro Devices, all rights reserved.
 *   Third party copyrights are property of their respective owners.
 *
 *   Redistribution and use in source and binary forms, with or without modification,
 *   are permitted provided that the following conditions are met:
 *
 *     * Redistribution's of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *     * Redistribution's in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 *
 *     * The name of the copyright holders may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *   This software is provided by the copyright holders and contributors "as is" and
 *   any express or implied warranties, including, but not limited to, the implied
 *   warranties of merchantability and fitness for a particular purpose are disclaimed.
 *   In no event shall the Intel Corporation or contributors be liable for any direct,
 *   indirect, incidental, special, exemplary, or consequential damages
 *   (including, but not limited to, procurement of substitute goods or services;
 *   loss of use, data, or profits; or business interruption) however caused
 *   and on any theory of liability, whether in contract, strict liability,
 *   or tort (including negligence or otherwise) arising in any way out of
 *   the use of this software, even if advised of the possibility of such damage.
 *
 * ALWAN_DENOISE_BILATERAL_SKIMAGE follows skimage.restoration.denoise_bilateral
 * (scikit-image 0.26.0, restoration/_denoise.py and _denoise_cy.pyx, BSD-3-Clause, Copyright
 * 2009-2022 the scikit-image team): a Euclidean colour distance over the channels read from a
 * `bins` table spanning [0, max), a spatial Gaussian read from a table, any border mode. Two
 * of its details are kept because they set its values: the spatial table is built over
 * arange(-win // 2, win // 2 + 1), one sample wider than the window, and read with the
 * window's width, so the weights are not the radially symmetric Gaussian; and the colour
 * table index is (bins / channels, an integer division) / max x distance, truncated. Two are
 * not: an image with a negative value comes back shifted by its minimum in scikit-image, and
 * here in place; and an 8-bit image is filtered on v / 255 with that image's maximum, where
 * scikit-image takes the maximum of the 8-bit codes, so its colour table spans 0..255 for
 * data in 0..1.
 *
 * ALWAN_DENOISE_WIENER_LOCAL is scipy.signal.wiener: the local mean and variance over a
 * size x size window (zero outside the image, the window sums added row by row from 0 as
 * scipy's direct correlation adds them), the noise power the mean of the local variances
 * unless given, and each pixel pulled to its local mean by noise / variance, the mean where
 * the variance is below the noise. scipy picks an FFT for the window sums on most images, so
 * its own result differs from its direct form by the FFT's rounding; this is the direct form.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------------------------ */
/* Shared                                                                                */
/* ------------------------------------------------------------------------------------ */

static long dbl_reflect101(long p, long n) {
    if (n == 1) return 0;
    while (p < 0 || p >= n) {
        if (p < 0) p = -p;
        if (p >= n) p = 2 * n - 2 - p;
    }
    return p;
}

/* cvRound on a double: ties to even (SSE2 cvtsd2si). */
static long dbl_round_even(double v) {
    double r = ALWAN_FLOOR_F64(v), d = v - r;
    long i = (long)r;
    if (d > 0.5 || (d == 0.5 && (i & 1))) i++;
    return i;
}

static unsigned char dbl_sat_u8(long v) {
    return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
}

/* OpenCV's v_exp_default_32f (a Cephes polynomial) at its SSE3 baseline, where v_fma is a
 * multiply then an add. Below about -87 it returns 0, not a subnormal. */
static float dbl_v_exp(float x0) {
    float x = x0, e, xx, y, scale;
    int mm;
    unsigned int bits;
    if (x < -88.3762626647949f) x = -88.3762626647949f;
    if (x > 89.f) x = 89.f;
    e = x * 1.44269504088896341f + 0.5f;
    mm = (int)ALWAN_FLOOR_F32(e);
    e = (float)mm;
    bits = (unsigned int)(mm + 0x7f) << 23;
    memcpy(&scale, &bits, sizeof(scale));
    x = e * -6.93359375E-1f + x;
    x = e * 2.12194440E-4f + x;
    xx = x * x;
    y = x * 1.9875691500E-4f + 1.3981999507E-3f;
    y = y * x + 8.3334519073E-3f;
    y = y * x + 4.1665795894E-2f;
    y = y * x + 1.6666665459E-1f;
    y = y * x + 5.0000001201E-1f;
    y = y * xx + x;
    y = y + 1.f;
    return y * scale;
}

static int dbl_finite(double v) {
    return v == v && v > -DBL_MAX && v < DBL_MAX;
}

/* ------------------------------------------------------------------------------------ */
/* cv::bilateralFilter                                                                   */
/* ------------------------------------------------------------------------------------ */

/* The window: offsets and spatial weights in OpenCV's row-major order. with_centre is 1 for
 * the 8-bit kernel, 0 for the float one, which leaves the centre out. */
static int dbl_cv_window(int radius, int with_centre, double gsc, int **dy, int **dx, float **sw) {
    int d = 2 * radius + 1, i, j, maxk = 0;
    *dy = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)d * (size_t)d, sizeof(int)), 16);
    *dx = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)d * (size_t)d, sizeof(int)), 16);
    *sw = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)d * (size_t)d, sizeof(float)), 16);
    if (!*dy || !*dx || !*sw) return -1;
    for (i = -radius; i <= radius; i++) {
        for (j = -radius; j <= radius; j++) {
            double r = ALWAN_SQRT((double)i * i + (double)j * j);
            if (r > radius) continue;
            if (!with_centre && i == 0 && j == 0) continue;
            (*sw)[maxk] = (float)ALWAN_EXP(r * r * gsc);
            (*dy)[maxk] = i;
            (*dx)[maxk++] = j;
        }
    }
    return maxk;
}

static void dbl_free3(void *a, void *b, void *c) {
    if (a) ALWAN_FREE(a);
    if (b) ALWAN_FREE(b);
    if (c) ALWAN_FREE(c);
}

static alwan_status dbl_cv_8u(unsigned char *out, size_t out_rs, unsigned char const *src, size_t src_rs, int cn, int w,
                              int h, long diameter, double sigma_color, double sigma_space) {
    static int const order13[13] = { 0, 12, 1, 2, 3, 9, 10, 11, 4, 5, 6, 7, 8 };
    float gcc = (float)(-0.5 / (sigma_color * sigma_color));
    float gsc = (float)(-0.5 / (sigma_space * sigma_space));
    int radius, maxk, i, x, y, tw, th, xv;
    int *dy = 0, *dx = 0;
    float *sw = 0, *cw;
    unsigned char *tmp;
    if (sigma_color <= 1e-6 || sigma_space <= 1e-6) {
        for (y = 0; y < h; y++) memmove(out + (size_t)y * out_rs, src + (size_t)y * src_rs, (size_t)w * cn);
        return ALWAN_OK;
    }
    radius = diameter <= 0 ? (int)dbl_round_even(sigma_space * 1.5) : (int)(diameter / 2);
    if (radius < 1) radius = 1;
    tw = w + 2 * radius;
    th = h + 2 * radius;
    cw = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)256 * (size_t)cn, sizeof(float)), 16);
    tmp = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size((size_t)tw * (size_t)th, (size_t)cn), 16);
    maxk = dbl_cv_window(radius, 1, (double)gsc, &dy, &dx, &sw);
    if (!cw || !tmp || maxk < 0) {
        dbl_free3(cw, tmp, dy);
        dbl_free3(dx, sw, 0);
        return ALWAN_E_NOMEM;
    }
    /* the baseline (SSE3) table fill: v_exp in fours, the last four by expf */
    for (i = 0; i < 256 * cn - 4; i++) cw[i] = dbl_v_exp(((float)i * (float)i) * gcc);
    for (; i < 256 * cn; i++) cw[i] = ALWAN_EXP_F32((float)(i * i) * gcc);
    for (y = 0; y < th; y++) {
        long sy = dbl_reflect101(y - radius, h);
        for (x = 0; x < tw; x++) {
            long sx = dbl_reflect101(x - radius, w);
            int c;
            for (c = 0; c < cn; c++) tmp[((size_t)y * tw + x) * cn + c] = src[(size_t)sy * src_rs + (size_t)sx * cn + c];
        }
    }
    xv = w - w % 32; /* the AVX2 kernel's blocks of 32 pixels */
    for (y = 0; y < h; y++) {
        unsigned char *drow = out + (size_t)y * out_rs;
        for (x = 0; x < w; x++) {
            unsigned char const *c0 = tmp + ((size_t)(y + radius) * tw + (x + radius)) * cn;
            int const fused = x < xv;
            int kk;
            if (cn == 1) {
                int v0 = c0[0];
                float wsum = 0.f, sum = 0.f;
                for (kk = 0; kk < maxk; kk++) {
                    int const k = (fused && maxk == 13) ? order13[kk] : kk;
                    int v = c0[(long)dy[k] * tw + dx[k]];
                    int ad = v > v0 ? v - v0 : v0 - v;
                    float wk = sw[k] * cw[ad];
                    wsum += wk;
                    if (fused) sum = ALWAN_FMA_CR_F32((float)v, wk, sum);
                    else sum += (float)v * wk;
                }
                drow[x] = dbl_sat_u8(dbl_round_even((double)(sum / wsum)));
            } else {
                float wsum = 0.f, sb = 0.f, sg = 0.f, sr = 0.f, inv;
                for (kk = 0; kk < maxk; kk++) {
                    unsigned char const *p = c0 + ((long)dy[kk] * tw + dx[kk]) * 3;
                    int b = p[0], g = p[1], r = p[2];
                    int diff = (b > c0[0] ? b - c0[0] : c0[0] - b) + (g > c0[1] ? g - c0[1] : c0[1] - g) +
                               (r > c0[2] ? r - c0[2] : c0[2] - r);
                    float wk = sw[kk] * cw[diff];
                    wsum += wk;
                    if (fused) {
                        sb = ALWAN_FMA_CR_F32((float)b, wk, sb);
                        sg = ALWAN_FMA_CR_F32((float)g, wk, sg);
                        sr = ALWAN_FMA_CR_F32((float)r, wk, sr);
                    } else {
                        sb += (float)b * wk;
                        sg += (float)g * wk;
                        sr += (float)r * wk;
                    }
                }
                inv = 1.f / wsum;
                drow[x * 3 + 0] = dbl_sat_u8(dbl_round_even((double)(inv * sb)));
                drow[x * 3 + 1] = dbl_sat_u8(dbl_round_even((double)(inv * sg)));
                drow[x * 3 + 2] = dbl_sat_u8(dbl_round_even((double)(inv * sr)));
            }
        }
    }
    dbl_free3(cw, tmp, dy);
    dbl_free3(dx, sw, 0);
    return ALWAN_OK;
}

/* The float kernel on a packed float image (in and out may not overlap). */
static alwan_status dbl_cv_32f(float *dst, float const *src, int cn, int w, int h, long diameter, double sigma_color,
                               double sigma_space) {
    double gcc = -0.5 / (sigma_color * sigma_color);
    double gsc = -0.5 / (sigma_space * sigma_space);
    int const nbins_per = 1 << 12;
    int radius, maxk, i, x, y, tw, th, xv, nbins;
    double mn, mx;
    float len, scale_index, last = 1.f;
    int *dy = 0, *dx = 0;
    float *sw = 0, *lut, *tmp;
    size_t const n = (size_t)w * (size_t)h * (size_t)cn;
    size_t k;
    if (sigma_color <= 1e-6 || sigma_space <= 1e-6) {
        memcpy(dst, src, n * sizeof(float));
        return ALWAN_OK;
    }
    radius = diameter <= 0 ? (int)dbl_round_even(sigma_space * 1.5) : (int)(diameter / 2);
    if (radius < 1) radius = 1;
    mn = mx = src[0];
    for (k = 1; k < n; k++) {
        if (src[k] < mn) mn = src[k];
        if (src[k] > mx) mx = src[k];
    }
    if (ALWAN_ABS_F64(mn - mx) < FLT_EPSILON) {
        memcpy(dst, src, n * sizeof(float));
        return ALWAN_OK;
    }
    tw = w + 2 * radius;
    th = h + 2 * radius;
    len = (float)(mx - mn) * (float)cn;
    nbins = nbins_per * cn;
    scale_index = (float)nbins / len;
    lut = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)nbins + 2, sizeof(float)), 16);
    tmp = (float *)ALWAN_ALLOC(alwan_safe_array_size(alwan_safe_array_size((size_t)tw, (size_t)th), (size_t)cn * sizeof(float)), 16);
    maxk = dbl_cv_window(radius, 0, gsc, &dy, &dx, &sw);
    if (!lut || !tmp || maxk < 0) {
        dbl_free3(lut, tmp, dy);
        dbl_free3(dx, sw, 0);
        return ALWAN_E_NOMEM;
    }
    for (i = 0; i < nbins + 2; i++) {
        if (last > 0.f) {
            double v = (double)((float)i / scale_index);
            lut[i] = (float)ALWAN_EXP(v * v * gcc);
            last = lut[i];
        } else {
            lut[i] = 0.f;
        }
    }
    for (y = 0; y < th; y++) {
        long sy = dbl_reflect101(y - radius, h);
        for (x = 0; x < tw; x++) {
            long sx = dbl_reflect101(x - radius, w);
            int c;
            for (c = 0; c < cn; c++) tmp[((size_t)y * tw + x) * cn + c] = src[((size_t)sy * w + sx) * cn + c];
        }
    }
    xv = w - w % 16; /* the AVX2 blocks of 32 then 16 pixels */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            float const *c0 = tmp + ((size_t)(y + radius) * tw + (x + radius)) * cn;
            int const vec = x < xv;
            int kk;
            if (cn == 1) {
                float const rv = c0[0];
                float wsum = 0.f, sum = 0.f;
                for (kk = 0; kk < maxk; kk++) {
                    float const v = c0[(long)dy[kk] * tw + dx[kk]];
                    float a = ALWAN_ABS_F32(v - rv) * scale_index, wk;
                    int idx;
                    if (vec) {
                        idx = (int)a;
                        if (idx > nbins) idx = nbins;
                        a -= (float)idx;
                        wk = sw[kk] * ALWAN_FMA_CR_F32(lut[idx + 1], a, lut[idx] * (1.f - a));
                        wsum += wk;
                        sum = ALWAN_FMA_CR_F32(v, wk, sum);
                    } else {
                        if (a > (float)nbins) a = (float)nbins;
                        idx = (int)ALWAN_FLOOR_F32(a);
                        a -= (float)idx;
                        wk = sw[kk] * (lut[idx] + a * (lut[idx + 1] - lut[idx]));
                        wsum += wk;
                        sum += v * wk;
                    }
                }
                dst[(size_t)y * w + x] = (sum + rv) / (wsum + 1.f);
            } else {
                float wsum = 0.f, sb = 0.f, sg = 0.f, sr = 0.f, inv;
                for (kk = 0; kk < maxk; kk++) {
                    float const *p = c0 + ((long)dy[kk] * tw + dx[kk]) * 3;
                    float const b = p[0], g = p[1], r = p[2];
                    float a = (ALWAN_ABS_F32(b - c0[0]) + ALWAN_ABS_F32(g - c0[1]) + ALWAN_ABS_F32(r - c0[2])) * scale_index;
                    float wk;
                    int idx;
                    if (vec) {
                        idx = (int)a;
                        if (idx > nbins) idx = nbins;
                        a -= (float)idx;
                        wk = sw[kk] * ALWAN_FMA_CR_F32(lut[idx + 1], a, lut[idx] * (1.f - a));
                        wsum += wk;
                        sb = ALWAN_FMA_CR_F32(b, wk, sb);
                        sg = ALWAN_FMA_CR_F32(g, wk, sg);
                        sr = ALWAN_FMA_CR_F32(r, wk, sr);
                    } else {
                        if (a > (float)nbins) a = (float)nbins;
                        idx = (int)ALWAN_FLOOR_F32(a);
                        a -= (float)idx;
                        wk = sw[kk] * (lut[idx] + a * (lut[idx + 1] - lut[idx]));
                        wsum += wk;
                        sb += b * wk;
                        sg += g * wk;
                        sr += r * wk;
                    }
                }
                inv = 1.f / (wsum + 1.f);
                dst[((size_t)y * w + x) * 3 + 0] = (sb + c0[0]) * inv;
                dst[((size_t)y * w + x) * 3 + 1] = (sg + c0[1]) * inv;
                dst[((size_t)y * w + x) * 3 + 2] = (sr + c0[2]) * inv;
            }
        }
    }
    dbl_free3(lut, tmp, dy);
    dbl_free3(dx, sw, 0);
    return ALWAN_OK;
}

/* ------------------------------------------------------------------------------------ */
/* skimage.restoration.denoise_bilateral                                                 */
/* ------------------------------------------------------------------------------------ */

/* scikit-image's coord_map, the mode letter from the alwan border. */
static long dbl_sk_map(long n, long c, int mode) {
    long const cmax = n - 1;
    switch (mode) {
    case ALWAN_FILTER_BORDER_REFLECT: /* 'symmetric' */
        if (c < 0) c = -c - 1;
        if (c > cmax) return (c / n) % 2 != 0 ? cmax - c % n : c % n;
        return c;
    case ALWAN_FILTER_BORDER_WRAP:
        if (c < 0) return cmax - ((-c - 1) % n);
        if (c > cmax) return c % n;
        return c;
    case ALWAN_FILTER_BORDER_MIRROR: /* 'reflect' */
        if (n == 1) return 0;
        if (c < 0) return ((-c / cmax) % 2 != 0) ? cmax - (-c % cmax) : (-c % cmax);
        if (c > cmax) return ((c / cmax) % 2 != 0) ? cmax - (c % cmax) : (c % cmax);
        return c;
    default: /* NEAREST, 'edge' */
        if (c < 0) return 0;
        if (c > cmax) return cmax;
        return c;
    }
}

/* numpy's pairwise sum of a contiguous run, in the element type. */
static double dbl_pw_d(double const *a, size_t n) {
    if (n < 8) {
        double r = 0.0;
        size_t i;
        for (i = 0; i < n; i++) r += a[i];
        return r;
    }
    if (n <= 128) {
        double r[8], res;
        size_t i, j;
        for (j = 0; j < 8; j++) r[j] = a[j];
        for (i = 8; i < n - (n % 8); i += 8)
            for (j = 0; j < 8; j++) r[j] += a[i + j];
        res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; i++) res += a[i];
        return res;
    } else {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return dbl_pw_d(a, n2) + dbl_pw_d(a + n2, n - n2);
    }
}

static float dbl_pw_f(float const *a, size_t n) {
    if (n < 8) {
        float r = 0.0f;
        size_t i;
        for (i = 0; i < n; i++) r += a[i];
        return r;
    }
    if (n <= 128) {
        float r[8], res;
        size_t i, j;
        for (j = 0; j < 8; j++) r[j] = a[j];
        for (i = 8; i < n - (n % 8); i += 8)
            for (j = 0; j < 8; j++) r[j] += a[i + j];
        res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; i++) res += a[i];
        return res;
    } else {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return dbl_pw_f(a, n2) + dbl_pw_f(a + n2, n - n2);
    }
}

/* numpy's std of the whole image: the pairwise mean, then the pairwise mean of the squared
 * differences, in the image's type. tmp holds n values. */
static double dbl_std_d(double const *a, double *tmp, size_t n) {
    double const m = dbl_pw_d(a, n) / (double)n;
    size_t i;
    for (i = 0; i < n; i++) {
        double const t = a[i] - m;
        tmp[i] = t * t;
    }
    return ALWAN_SQRT(dbl_pw_d(tmp, n) / (double)n);
}

static float dbl_std_f(float const *a, float *tmp, size_t n) {
    float const m = dbl_pw_f(a, n) / (float)n;
    size_t i;
    for (i = 0; i < n; i++) {
        float const t = a[i] - m;
        tmp[i] = t * t;
    }
    return ALWAN_SQRT_F32(dbl_pw_f(tmp, n) / (float)n);
}

/* scikit-image's filter on a packed image in double (img and out may not overlap). */
static alwan_status dbl_sk_d(double *out, double *img, size_t dims, size_t rows, size_t cols, size_t win,
                             double sigma_color, double sigma_spatial, size_t bins, int mode, double cval) {
    size_t const n = rows * cols * dims, g = win + 1;
    long const ext = (long)((win - 1) / 2);
    double mn, mx, step, dist_scale;
    double *clut, *rlut, *vals;
    size_t i;
    long r, c;
    if (sigma_color == 0.0) {
        double *t = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
        if (!t) return ALWAN_E_NOMEM;
        sigma_color = dbl_std_d(img, t, n);
        ALWAN_FREE(t);
    }
    mn = mx = img[0];
    for (i = 1; i < n; i++) {
        if (img[i] < mn) mn = img[i];
        if (img[i] > mx) mx = img[i];
    }
    if (mn == mx) {
        memcpy(out, img, n * sizeof(double));
        return ALWAN_OK;
    }
    clut = (double *)ALWAN_ALLOC(alwan_safe_array_size(bins, sizeof(double)), sizeof(double));
    rlut = (double *)ALWAN_ALLOC(alwan_safe_array_size(g * g, sizeof(double)), sizeof(double));
    vals = (double *)ALWAN_ALLOC(alwan_safe_array_size(dims, sizeof(double)), sizeof(double));
    if (!clut || !rlut || !vals) {
        dbl_free3(clut, rlut, vals);
        return ALWAN_E_NOMEM;
    }
    /* colour table: np.linspace(0, max, bins, endpoint=False), exp(-0.5 (v^2 / s^2)) */
    step = mx / (double)bins;
    {
        double const s2 = sigma_color * sigma_color;
        for (i = 0; i < bins; i++) {
            double const v = (double)i * step;
            clut[i] = ALWAN_EXP(-0.5 * ((v * v) / s2));
        }
    }
    /* spatial table over arange(-win // 2, win // 2 + 1), win + 1 samples a side */
    {
        long const lo = -(long)((win + 1) / 2); /* Python's -win // 2 */
        double const s2 = sigma_spatial * sigma_spatial;
        size_t a, b;
        for (a = 0; a < g; a++) {
            for (b = 0; b < g; b++) {
                double const ra = (double)(lo + (long)a), cb = (double)(lo + (long)b);
                double const d = ALWAN_SQRT(ra * ra + cb * cb);
                rlut[a * g + b] = ALWAN_EXP(-0.5 * ((d * d) / s2));
            }
        }
    }
    if (mn < 0.0) {
        for (i = 0; i < n; i++) img[i] -= mn;
        mx -= mn;
    }
    dist_scale = (double)(bins / dims) / mx;
    for (r = 0; r < (long)rows; r++) {
        for (c = 0; c < (long)cols; c++) {
            double const *centre = img + ((size_t)r * cols + (size_t)c) * dims;
            double tw = 0.0;
            double *tv = out + ((size_t)r * cols + (size_t)c) * dims;
            long wr, wc;
            size_t d;
            for (d = 0; d < dims; d++) tv[d] = 0.0;
            for (wr = -ext; wr <= ext; wr++) {
                long const rr = wr + r, kr = wr + ext;
                for (wc = -ext; wc <= ext; wc++) {
                    long const cc = wc + c, kc = wc + ext;
                    double dist = 0.0, wgt;
                    size_t bin;
                    for (d = 0; d < dims; d++) {
                        double v, t;
                        if (mode == ALWAN_FILTER_BORDER_CONSTANT && (rr < 0 || rr >= (long)rows || cc < 0 || cc >= (long)cols))
                            v = cval;
                        else
                            v = img[((size_t)dbl_sk_map((long)rows, rr, mode) * cols + (size_t)dbl_sk_map((long)cols, cc, mode)) * dims + d];
                        vals[d] = v;
                        t = centre[d] - v;
                        dist += t * t;
                    }
                    dist = ALWAN_SQRT(dist);
                    {
                        double const f = dist * dist_scale;
                        bin = f >= (double)(bins - 1) ? bins - 1 : (size_t)f;
                    }
                    wgt = rlut[(size_t)kr * win + (size_t)kc] * clut[bin];
                    for (d = 0; d < dims; d++) tv[d] += vals[d] * wgt;
                    tw += wgt;
                }
            }
            for (d = 0; d < dims; d++) tv[d] = tv[d] / tw;
        }
    }
    if (mn < 0.0) {
        for (i = 0; i < n; i++) {
            out[i] += mn;
            img[i] += mn;
        }
    }
    dbl_free3(clut, rlut, vals);
    return ALWAN_OK;
}

/* The same in float, as scikit-image runs a float32 image: every sum and product in float.
 * Its tables come from numpy's float32 exp, which is neither the C runtime's nor correctly
 * rounded; these are exp in double rounded to float, so a table entry can differ by one
 * float step. */
static alwan_status dbl_sk_f(float *out, float *img, size_t dims, size_t rows, size_t cols, size_t win,
                             double sigma_color_in, double sigma_spatial, size_t bins, int mode, double cval) {
    size_t const n = rows * cols * dims, g = win + 1;
    long const ext = (long)((win - 1) / 2);
    float mn, mx, step, dist_scale, sigma_color = (float)sigma_color_in, fcval = (float)cval;
    float *clut, *rlut, *vals;
    size_t i;
    long r, c;
    if (sigma_color_in == 0.0) {
        float *t = (float *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(float)), sizeof(float));
        if (!t) return ALWAN_E_NOMEM;
        sigma_color = dbl_std_f(img, t, n);
        ALWAN_FREE(t);
    }
    mn = mx = img[0];
    for (i = 1; i < n; i++) {
        if (img[i] < mn) mn = img[i];
        if (img[i] > mx) mx = img[i];
    }
    if (mn == mx) {
        memcpy(out, img, n * sizeof(float));
        return ALWAN_OK;
    }
    clut = (float *)ALWAN_ALLOC(alwan_safe_array_size(bins, sizeof(float)), sizeof(float));
    rlut = (float *)ALWAN_ALLOC(alwan_safe_array_size(g * g, sizeof(float)), sizeof(float));
    vals = (float *)ALWAN_ALLOC(alwan_safe_array_size(dims, sizeof(float)), sizeof(float));
    if (!clut || !rlut || !vals) {
        dbl_free3(clut, rlut, vals);
        return ALWAN_E_NOMEM;
    }
    /* linspace in float32: step = max / bins, y = i * step; then v^2 / (float)s^2 */
    step = mx / (float)bins;
    {
        float const s2 = sigma_color_in == 0.0 ? sigma_color * sigma_color : (float)(sigma_color_in * sigma_color_in);
        for (i = 0; i < bins; i++) {
            float const v = (float)i * step;
            float const a = -0.5f * ((v * v) / s2);
            clut[i] = (float)ALWAN_EXP((double)a);
        }
    }
    {
        long const lo = -(long)((win + 1) / 2);
        double const s2 = sigma_spatial * sigma_spatial;
        size_t a, b;
        for (a = 0; a < g; a++) {
            for (b = 0; b < g; b++) {
                double const ra = (double)(lo + (long)a), cb = (double)(lo + (long)b);
                double const d = ALWAN_SQRT(ra * ra + cb * cb);
                rlut[a * g + b] = (float)ALWAN_EXP((double)(float)(-0.5 * ((d * d) / s2)));
            }
        }
    }
    if (mn < 0.0f) {
        for (i = 0; i < n; i++) img[i] -= mn;
        mx -= mn;
    }
    dist_scale = (float)((double)(bins / dims) / (double)mx);
    for (r = 0; r < (long)rows; r++) {
        for (c = 0; c < (long)cols; c++) {
            float const *centre = img + ((size_t)r * cols + (size_t)c) * dims;
            float tw = 0.0f;
            float *tv = out + ((size_t)r * cols + (size_t)c) * dims;
            long wr, wc;
            size_t d;
            for (d = 0; d < dims; d++) tv[d] = 0.0f;
            for (wr = -ext; wr <= ext; wr++) {
                long const rr = wr + r, kr = wr + ext;
                for (wc = -ext; wc <= ext; wc++) {
                    long const cc = wc + c, kc = wc + ext;
                    float dist = 0.0f, wgt, f;
                    size_t bin;
                    for (d = 0; d < dims; d++) {
                        float v, t;
                        if (mode == ALWAN_FILTER_BORDER_CONSTANT && (rr < 0 || rr >= (long)rows || cc < 0 || cc >= (long)cols))
                            v = fcval;
                        else
                            v = img[((size_t)dbl_sk_map((long)rows, rr, mode) * cols + (size_t)dbl_sk_map((long)cols, cc, mode)) * dims + d];
                        vals[d] = v;
                        t = centre[d] - v;
                        dist += t * t;
                    }
                    dist = (float)ALWAN_SQRT((double)dist);
                    f = dist * dist_scale;
                    bin = (double)f >= (double)(bins - 1) ? bins - 1 : (size_t)f;
                    wgt = rlut[(size_t)kr * win + (size_t)kc] * clut[bin];
                    for (d = 0; d < dims; d++) tv[d] += vals[d] * wgt;
                    tw += wgt;
                }
            }
            for (d = 0; d < dims; d++) tv[d] = tv[d] / tw;
        }
    }
    if (mn < 0.0f) {
        for (i = 0; i < n; i++) {
            out[i] += mn;
            img[i] += mn;
        }
    }
    dbl_free3(clut, rlut, vals);
    return ALWAN_OK;
}

/* ------------------------------------------------------------------------------------ */
/* scipy.signal.wiener                                                                   */
/* ------------------------------------------------------------------------------------ */

/* One channel of doubles; sq holds the squares as scipy forms them (in float for a float32
 * image). out may not overlap. */
static alwan_status dbl_wiener_plane(double *out, double const *im, double const *sq, size_t w, size_t h, size_t k,
                                     double noise_in) {
    long const half = (long)(k / 2);
    double const lsize = (double)(k * k);
    size_t const n = w * h;
    double *mean = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    double *var = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    double noise;
    long x, y;
    size_t i;
    if (!mean || !var) {
        dbl_free3(mean, var, 0);
        return ALWAN_E_NOMEM;
    }
    for (y = 0; y < (long)h; y++) {
        for (x = 0; x < (long)w; x++) {
            double s1 = 0.0, s2 = 0.0, m;
            long a, b;
            for (a = -half; a <= half; a++) {
                long const yy = y + a;
                for (b = -half; b <= half; b++) {
                    long const xx = x + b;
                    if (yy < 0 || yy >= (long)h || xx < 0 || xx >= (long)w) {
                        s1 += 0.0;
                        s2 += 0.0;
                    } else {
                        s1 += im[(size_t)yy * w + (size_t)xx];
                        s2 += sq[(size_t)yy * w + (size_t)xx];
                    }
                }
            }
            m = s1 / lsize;
            mean[(size_t)y * w + (size_t)x] = m;
            var[(size_t)y * w + (size_t)x] = s2 / lsize - m * m;
        }
    }
    noise = noise_in > 0.0 ? noise_in : dbl_pw_d(var, n) / (double)n;
    for (i = 0; i < n; i++) {
        double res = im[i] - mean[i];
        res *= (1.0 - noise / var[i]);
        res += mean[i];
        out[i] = var[i] < noise ? mean[i] : res;
    }
    dbl_free3(mean, var, 0);
    return ALWAN_OK;
}

/* ------------------------------------------------------------------------------------ */
/* Entry points (kind 0 f64, 1 f32, 2 u8)                                                */
/* ------------------------------------------------------------------------------------ */

static int dbl_check(void const *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                     size_t esz) {
    if (!out || !src || w == 0 || h == 0 || ch == 0 || ch > 4) return 0;
    if (w > (size_t)INT_MAX / 8 || h > (size_t)INT_MAX / 8) return 0;
    if (src_rs / esz / ch < w || out_rs / esz / ch < w) return 0;
    return 1;
}

static size_t dbl_esz(int kind) {
    return kind == 0 ? sizeof(double) : kind == 1 ? sizeof(float) : 1;
}

/* Read the image into a packed double buffer, refusing a non-finite value. */
static alwan_status dbl_read_d(double *buf, void const *src, size_t src_rs, size_t ch, size_t w, size_t h, int kind,
                               double scale) {
    size_t x, y;
    for (y = 0; y < h; y++) {
        unsigned char const *row = (unsigned char const *)src + y * src_rs;
        for (x = 0; x < w * ch; x++) {
            double v = kind == 0 ? ((double const *)row)[x] : kind == 1 ? (double)((float const *)row)[x] : (double)row[x] * scale;
            if (!dbl_finite(v)) return ALWAN_E_INVALID;
            buf[y * w * ch + x] = v;
        }
    }
    return ALWAN_OK;
}

static void dbl_write_d(void *out, size_t out_rs, double const *buf, size_t ch, size_t w, size_t h, int kind, double scale) {
    size_t x, y;
    for (y = 0; y < h; y++) {
        unsigned char *row = (unsigned char *)out + y * out_rs;
        for (x = 0; x < w * ch; x++) {
            double const v = buf[y * w * ch + x];
            if (kind == 0) ((double *)row)[x] = v;
            else if (kind == 1) ((float *)row)[x] = (float)v;
            else row[x] = dbl_sat_u8(dbl_round_even(v * scale));
        }
    }
}

alwan_status alwan__denoise_bilateral_opencv(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w,
                                             size_t h, size_t diameter, double sigma_color, double sigma_space, int kind) {
    size_t const n = w * h * ch;
    float *a, *b;
    alwan_status st;
    size_t x, y;
    if (!dbl_check(out, out_rs, src, src_rs, ch, w, h, dbl_esz(kind))) return ALWAN_E_INVALID;
    if (ch != 1 && ch != 3) return ALWAN_E_INVALID;
    if (!dbl_finite(sigma_color) || !dbl_finite(sigma_space) || diameter > 4096) return ALWAN_E_RANGE;
    if (kind == 2) {
        unsigned char *tmp = 0;
        unsigned char const *s8 = (unsigned char const *)src;
        size_t srs = src_rs;
        if (out == src) { /* OpenCV refuses in place; filter a copy */
            tmp = (unsigned char *)ALWAN_ALLOC(n, 16);
            if (!tmp) return ALWAN_E_NOMEM;
            for (y = 0; y < h; y++) memcpy(tmp + y * w * ch, s8 + y * src_rs, w * ch);
            s8 = tmp;
            srs = w * ch;
        }
        st = dbl_cv_8u((unsigned char *)out, out_rs, s8, srs, (int)ch, (int)w, (int)h, (long)diameter, sigma_color,
                       sigma_space);
        if (tmp) ALWAN_FREE(tmp);
        return st;
    }
    a = (float *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(float)), 16);
    b = (float *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(float)), 16);
    if (!a || !b) {
        dbl_free3(a, b, 0);
        return ALWAN_E_NOMEM;
    }
    for (y = 0; y < h; y++) {
        unsigned char const *row = (unsigned char const *)src + y * src_rs;
        for (x = 0; x < w * ch; x++) {
            double const v = kind == 0 ? ((double const *)row)[x] : (double)((float const *)row)[x];
            if (!dbl_finite(v)) {
                dbl_free3(a, b, 0);
                return ALWAN_E_INVALID;
            }
            a[y * w * ch + x] = (float)v;
        }
    }
    st = dbl_cv_32f(b, a, (int)ch, (int)w, (int)h, (long)diameter, sigma_color, sigma_space);
    if (st == ALWAN_OK) {
        for (y = 0; y < h; y++) {
            unsigned char *row = (unsigned char *)out + y * out_rs;
            for (x = 0; x < w * ch; x++) {
                if (kind == 0) ((double *)row)[x] = (double)b[y * w * ch + x];
                else ((float *)row)[x] = b[y * w * ch + x];
            }
        }
    }
    dbl_free3(a, b, 0);
    return st;
}

alwan_status alwan__denoise_bilateral_skimage(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w,
                                              size_t h, size_t win, double sigma_color, double sigma_spatial, size_t bins,
                                              int border, double cval, int kind) {
    size_t const n = w * h * ch;
    alwan_status st;
    if (!dbl_check(out, out_rs, src, src_rs, ch, w, h, dbl_esz(kind))) return ALWAN_E_INVALID;
    if (border < 0 || border > ALWAN_FILTER_BORDER_WRAP) return ALWAN_E_INVALID;
    if (!dbl_finite(sigma_color) || sigma_color < 0.0 || !dbl_finite(sigma_spatial) || sigma_spatial <= 0.0 ||
        !dbl_finite(cval))
        return ALWAN_E_RANGE;
    if (win == 0) {
        double const c = ALWAN_CEIL(3.0 * sigma_spatial);
        if (c > 4096.0) return ALWAN_E_RANGE;
        win = 2 * (size_t)c + 1;
        if (win < 5) win = 5;
    }
    if (win > 8193 || bins == 0 || bins > 100000000 || bins < ch) return ALWAN_E_RANGE;
    if (kind == 1) {
        float *a = (float *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(float)), 16);
        float *b = (float *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(float)), 16);
        size_t x, y;
        if (!a || !b) {
            dbl_free3(a, b, 0);
            return ALWAN_E_NOMEM;
        }
        for (y = 0; y < h; y++) {
            float const *row = (float const *)((unsigned char const *)src + y * src_rs);
            for (x = 0; x < w * ch; x++) {
                if (!dbl_finite((double)row[x])) {
                    dbl_free3(a, b, 0);
                    return ALWAN_E_INVALID;
                }
                a[y * w * ch + x] = row[x];
            }
        }
        st = dbl_sk_f(b, a, ch, h, w, win, sigma_color, sigma_spatial, bins, border, cval);
        if (st == ALWAN_OK) {
            for (y = 0; y < h; y++) {
                float *row = (float *)((unsigned char *)out + y * out_rs);
                for (x = 0; x < w * ch; x++) row[x] = b[y * w * ch + x];
            }
        }
        dbl_free3(a, b, 0);
        return st;
    } else {
        double *a = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), 16);
        double *b = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), 16);
        if (!a || !b) {
            dbl_free3(a, b, 0);
            return ALWAN_E_NOMEM;
        }
        st = dbl_read_d(a, src, src_rs, ch, w, h, kind, 1.0 / 255.0);
        if (st == ALWAN_OK) st = dbl_sk_d(b, a, ch, h, w, win, sigma_color, sigma_spatial, bins, border, cval);
        if (st == ALWAN_OK) dbl_write_d(out, out_rs, b, ch, w, h, kind, 255.0);
        dbl_free3(a, b, 0);
        return st;
    }
}

alwan_status alwan__denoise_wiener(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                   size_t size, double noise, int kind) {
    size_t const np = w * h;
    double *im, *sq, *res;
    alwan_status st = ALWAN_OK;
    size_t c, x, y;
    if (!dbl_check(out, out_rs, src, src_rs, ch, w, h, dbl_esz(kind))) return ALWAN_E_INVALID;
    if (size == 0) size = 3;
    if (!(size & 1u) || size > 255 || !dbl_finite(noise) || noise < 0.0) return ALWAN_E_RANGE;
    im = (double *)ALWAN_ALLOC(alwan_safe_array_size(np, sizeof(double)), 16);
    sq = (double *)ALWAN_ALLOC(alwan_safe_array_size(np, sizeof(double)), 16);
    res = (double *)ALWAN_ALLOC(alwan_safe_array_size(alwan_safe_array_size(np, ch), sizeof(double)), 16);
    if (!im || !sq || !res) {
        dbl_free3(im, sq, res);
        return ALWAN_E_NOMEM;
    }
    for (c = 0; c < ch && st == ALWAN_OK; c++) {
        for (y = 0; y < h; y++) {
            unsigned char const *row = (unsigned char const *)src + y * src_rs;
            for (x = 0; x < w; x++) {
                double v;
                if (kind == 0) {
                    v = ((double const *)row)[x * ch + c];
                    sq[y * w + x] = v * v;
                } else if (kind == 1) {
                    float const f = ((float const *)row)[x * ch + c];
                    v = (double)f;
                    sq[y * w + x] = (double)(f * f); /* numpy squares a float32 image in float32 */
                } else {
                    v = (double)row[x * ch + c];
                    sq[y * w + x] = v * v;
                }
                if (!dbl_finite(v)) {
                    st = ALWAN_E_INVALID;
                    break;
                }
                im[y * w + x] = v;
            }
            if (st != ALWAN_OK) break;
        }
        if (st != ALWAN_OK) break;
        {
            double *plane = (double *)ALWAN_ALLOC(alwan_safe_array_size(np, sizeof(double)), 16);
            if (!plane) {
                st = ALWAN_E_NOMEM;
                break;
            }
            st = dbl_wiener_plane(plane, im, sq, w, h, size, noise);
            for (x = 0; x < np && st == ALWAN_OK; x++) res[x * ch + c] = plane[x];
            ALWAN_FREE(plane);
        }
    }
    if (st == ALWAN_OK) dbl_write_d(out, out_rs, res, ch, w, h, kind, 1.0);
    dbl_free3(im, sq, res);
    return st;
}
