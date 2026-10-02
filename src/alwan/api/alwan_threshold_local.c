/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_threshold_local_{T}: a threshold for every pixel from its neighbourhood, as
 * scikit-image's filters.threshold_local (gaussian, mean, median), threshold_niblack and
 * threshold_sauvola compute it, which suite 222 holds this to.
 *
 *   GAUSSIAN  scipy's gaussian_filter, sigma (block_size - 1) / 6, truncated at 4 sigma,
 *             minus offset
 *   MEAN      scipy's uniform_filter over block_size x block_size, minus offset
 *   MEDIAN    scipy's median_filter over block_size x block_size, minus offset
 *   NIBLACK   m - k s
 *   SAUVOLA   m (1 + k (s / r - 1))
 *
 * The first three filter the columns, then the rows, each pass stored in the data's
 * precision, with scipy's "reflect" edge (d c b a | a b c d). The gaussian correlates as
 * scipy's symmetric kernel does, outer pairs first; the mean is scipy's running sum. m and
 * s, the mean and standard deviation over the window, come from scikit-image's integral
 * images: the image padded by numpy's "reflect" (d c b | a b c d), w / 2 + 1 above and to
 * the left and w / 2 below and to the right, summed down the columns and then along the
 * rows in double, the window's sum the four-corner combination in scikit-image's order.
 * On float32 data every operation outside the double sums, and every constant, is rounded
 * to float as numpy rounds it.
 *
 * WOLF and NICK port OpenCV's ximgproc niBlackThreshold (modules/ximgproc/src/
 * niblack_thresholding.cpp, opencv_contrib; Copyright (C) 2014, Beat Kueng, Lukas Vogel,
 * Morten Lysgaard; the 3-clause licence below). The mean and the mean of squares are
 * OpenCV's boxFilter and sqrBoxFilter to float32 with BORDER_REPLICATE: row sums in double
 * (direct for a 3 or 5 wide box, else a running sum; squares always running), then running
 * column sums in double, each output (float)(sum * (1 / block_size^2)). Then, in float32:
 * variance = sqmean - mean^2, s = sqrt(variance) (NaN where that rounds negative, as
 * OpenCV's), WOLF m - k (m - min - s (m - min) / max s), NICK m + k sqrt(variance + sqmean).
 * OpenCV evaluates those through its matrix expressions, whose vectorised paths may fuse a
 * multiply and an add; the order here is the plain one, and suite 292 measures the residual.
 * For a float32 image and a block of 3 or 5 OpenCV's boxFilter takes another summation
 * path; the sums are exact in double for ordinary data, so the means are the same.
 *
 *   Redistribution and use in source and binary forms, with or without modification, are
 *   permitted provided that the following conditions are met:
 *   * Redistribution's of source code must retain the above copyright notice, this list of
 *     conditions and the following disclaimer.
 *   * Redistribution's in binary form must reproduce the above copyright notice, this list
 *     of conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *   * The name of the copyright holders may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *   This software is provided by the copyright holders and contributors "as is" and any
 *   express or implied warranties, including, but not limited to, the implied warranties of
 *   merchantability and fitness for a particular purpose are disclaimed. In no event shall
 *   the Intel Corporation or contributors be liable for any direct, indirect, incidental,
 *   special, exemplary, or consequential damages (including, but not limited to,
 *   procurement of substitute goods or services; loss of use, data, or profits; or business
 *   interruption) however caused and on any theory of liability, whether in contract,
 *   strict liability, or tort (including negligence or otherwise) arising in any way out of
 *   the use of this software, even if advised of the possibility of such damage.
 *
 * BRADLEY is Bradley and Roth 2007, "Adaptive Thresholding using the Integral Image": the
 * image's integral (summed down the columns, then along the rows, in double, a zero row and
 * column first), the window's sum ((I[y2][x2] - I[y1][x2]) - I[y2][x1]) + I[y1][x1] over the
 * block clipped to the image, the threshold sum / count * (1 - k).
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double alwan_tl_r(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

/* scipy's "reflect": period 2 n, d c b a | a b c d | d c b a */
static size_t alwan_tl_scipy_reflect(long i, size_t n) {
    long const p = 2 * (long)n;
    long m = i % p;
    if (m < 0) m += p;
    return (size_t)(m >= (long)n ? p - 1 - m : m);
}

/* numpy's pad "reflect": period 2 (n - 1), d c b | a b c d | c b a */
static size_t alwan_tl_numpy_reflect(long i, size_t n) {
    long p, m;
    if (n == 1) return 0;
    p = 2 * ((long)n - 1);
    m = i % p;
    if (m < 0) m += p;
    return (size_t)(m >= (long)n ? p - m : m);
}

/* One pass of scipy's gaussian (kind 0) or uniform (1) filter along lines: count lines of
 * len samples, sample (l, i) at in[l * lstep + i * istep]. ext holds len + 2 half + 1. */
static void alwan_tl_pass(double *out, double const *in, size_t count, size_t len, size_t lstep, size_t istep, int kind,
                          double const *wts, size_t half, int f32, double *ext) {
    size_t l, i;
    long j;
    for (l = 0; l < count; l++) {
        for (j = -(long)half; j < (long)(len + half); j++)
            ext[j + (long)half] = in[l * lstep + alwan_tl_scipy_reflect(j, len) * istep];
        if (kind == 0) {
            for (i = 0; i < len; i++) {
                double const *c = ext + half + i;
                double acc = c[0] * wts[half];
                for (j = -(long)half; j < 0; j++) acc += (c[j] + c[-j]) * wts[(long)half + j];
                out[l * lstep + i * istep] = alwan_tl_r(acc, f32);
            }
        } else {
            size_t const size = 2 * half + 1;
            double t = 0.0;
            for (i = 0; i < size; i++) t += ext[i];
            out[l * lstep] = alwan_tl_r(t / (double)size, f32);
            for (i = 1; i < len; i++) {
                t += ext[i + size - 1] - ext[i - 1];
                out[l * lstep + i * istep] = alwan_tl_r(t / (double)size, f32);
            }
        }
    }
}

/* numpy's pairwise sum, for the kernel's normalisation (fewer than 129 terms here, never
 * more than the eight-accumulator block). */
static double alwan_tl_sum(double const *a, size_t n) {
    double r[8], res = 0.0;
    size_t i, j;
    if (n < 8) {
        for (i = 0; i < n; i++) res += a[i];
        return res;
    }
    if (n > 128) {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan_tl_sum(a, n2) + alwan_tl_sum(a + n2, n - n2);
    }
    for (j = 0; j < 8; j++) r[j] = a[j];
    for (i = 8; i < n - (n % 8); i += 8)
        for (j = 0; j < 8; j++) r[j] += a[i + j];
    res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    for (; i < n; i++) res += a[i];
    return res;
}

static int alwan_tl_cmp(void const *a, void const *b) {
    double const x = *(double const *)a, y = *(double const *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

/* OpenCV's boxFilter (sq 0) or sqrBoxFilter (sq 1) of one channel to float32, normalised,
 * BORDER_REPLICATE; img holds float32 values. rs: (h + b - 1) * w doubles, ext: w + b. */
static void alwan_tl_cv_box(double *out, double const *img, size_t w, size_t h, size_t b, int sq, double *rs, double *ext) {
    size_t const half = b / 2, rows = h + b - 1;
    double const scale = 1.0 / (double)(b * b);
    size_t y, x, i;
    for (y = 0; y < rows; y++) {
        /* padded row y is source row y - half, clamped (BORDER_REPLICATE); likewise columns */
        size_t const sy = y < half ? 0 : y - half >= h ? h - 1 : y - half;
        double const *row = img + sy * w;
        double *d = rs + y * w;
        for (i = 0; i < w + b - 1; i++) ext[i] = row[i < half ? 0 : i - half >= w ? w - 1 : i - half];
        if (!sq && b == 3) {
            for (x = 0; x < w; x++) d[x] = ext[x] + ext[x + 1] + ext[x + 2];
        } else if (!sq && b == 5) {
            for (x = 0; x < w; x++) d[x] = ext[x] + ext[x + 1] + ext[x + 2] + ext[x + 3] + ext[x + 4];
        } else {
            double acc = 0.0;
            for (i = 0; i < b; i++) acc += sq ? ext[i] * ext[i] : ext[i];
            d[0] = acc;
            for (x = 0; x + 1 < w; x++) {
                if (sq) acc += ext[x + b] * ext[x + b] - ext[x] * ext[x];
                else acc += ext[x + b] - ext[x];
                d[x + 1] = acc;
            }
        }
    }
    for (x = 0; x < w; x++) {
        double sum = 0.0;
        for (i = 0; i + 1 < b; i++) sum += rs[i * w + x];
        for (y = 0; y < h; y++) {
            double const s0 = sum + rs[(y + b - 1) * w + x];
            out[y * w + x] = (double)(float)(s0 * scale);
            sum = s0 - rs[y * w + x];
        }
    }
}

static alwan_status alwan_tl_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_threshold_local_method method, alwan_threshold_local_params const *params, int f32) {
    alwan_threshold_local_params const zero = { 0 };
    alwan_threshold_local_params const *p = params ? params : &zero;
    size_t const elem = f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const b = p->block_size ? p->block_size
                     : method == ALWAN_THRESHOLD_LOCAL_BRADLEY ? ((w / 8) < 3 ? 3 : (w / 8) | 1)
                                                               : 15,
                 half = b / 2;
    size_t const n = w * h, pw = w + b, ph = h + b;
    double const k = alwan_tl_r(p->k != 0.0 ? p->k : 0.2, f32);
    float const kcv = (float)(p->k != 0.0 ? p->k : method == ALWAN_THRESHOLD_LOCAL_WOLF ? 0.5 : -0.1);
    double const r = alwan_tl_r(p->r != 0.0 ? p->r : 1.0, f32);
    double const offset = alwan_tl_r(p->offset, f32);
    double const sigma = p->sigma > 0.0 ? p->sigma : (double)(b - 1) / 6.0;
    size_t const lw = (size_t)(4.0 * sigma + 0.5);
    size_t const ext_len = (w > h ? w : h) + 2 * (lw > half ? lw : half) + 2;
    double *buf, *img, *tmp, *res, *ext, *wts, *ia = NULL, *ib = NULL;
    size_t x, y, c, i;
    alwan_status st = ALWAN_OK;
    if (!out || !src || w == 0 || h == 0 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_THRESHOLD_LOCAL_BRADLEY) return ALWAN_E_INVALID;
    if ((method == ALWAN_THRESHOLD_LOCAL_WOLF || method == ALWAN_THRESHOLD_LOCAL_NICK) && b == 1) return ALWAN_E_RANGE;
    if (b % 2 == 0 || b > 1023 || !(p->sigma >= 0.0) || p->sigma > 256.0 || !(p->k == p->k) || !(p->r == p->r) ||
        !(p->offset == p->offset))
        return ALWAN_E_RANGE;
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(3 * n + ext_len + 2 * lw + 1 + b * b, sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    img = buf;
    tmp = img + n;
    res = tmp + n;
    ext = res + n;
    wts = ext + ext_len;
    if (method == ALWAN_THRESHOLD_LOCAL_WOLF || method == ALWAN_THRESHOLD_LOCAL_NICK) {
        /* ia: the box sums' rows, (h + b - 1) x w, and their padded line; ib: mean and sqmean */
        ia = (double *)ALWAN_ALLOC(alwan_safe_array_size((h + b - 1) * w + w + b + 2 * n, sizeof(double)), sizeof(double));
        if (!ia) {
            ALWAN_FREE(buf);
            return ALWAN_E_NOMEM;
        }
        ib = ia + (h + b - 1) * w + w + b;
    } else if (method == ALWAN_THRESHOLD_LOCAL_BRADLEY) {
        ia = (double *)ALWAN_ALLOC(alwan_safe_array_size((w + 1) * (h + 1), sizeof(double)), sizeof(double));
        if (!ia) {
            ALWAN_FREE(buf);
            return ALWAN_E_NOMEM;
        }
    } else if (method == ALWAN_THRESHOLD_LOCAL_NIBLACK || method == ALWAN_THRESHOLD_LOCAL_SAUVOLA) {
        ia = (double *)ALWAN_ALLOC(alwan_safe_array_size(pw * ph, 2 * sizeof(double)), sizeof(double));
        if (!ia) {
            ALWAN_FREE(buf);
            return ALWAN_E_NOMEM;
        }
        ib = ia + pw * ph;
    }
    if (method == ALWAN_THRESHOLD_LOCAL_GAUSSIAN && sigma > 1e-15) {
        /* scipy's _gaussian_kernel1d: exp(-0.5 / sigma^2 * x^2), normalised by numpy's sum */
        double const a = -0.5 / (sigma * sigma);
        double s;
        for (i = 0; i <= 2 * lw; i++) {
            long const xi = (long)i - (long)lw;
            wts[i] = ALWAN_EXP_F64(a * (double)(xi * xi));
        }
        s = alwan_tl_sum(wts, 2 * lw + 1);
        for (i = 0; i <= 2 * lw; i++) wts[i] = wts[i] / s;
    }
    for (c = 0; c < ch && st == ALWAN_OK; c++) {
        for (y = 0; y < h; y++) {
            char const *row = (char const *)src + y * src_rs;
            for (x = 0; x < w; x++) {
                double const v = f32 ? (double)((alwan_f32 const *)row)[x * ch + c] : ((alwan_f64 const *)row)[x * ch + c];
                if (!(v - v == 0.0)) {
                    st = ALWAN_E_INVALID;
                    break;
                }
                img[y * w + x] = v;
            }
        }
        if (st != ALWAN_OK) break;
        switch (method) {
        case ALWAN_THRESHOLD_LOCAL_GAUSSIAN:
            if (sigma > 1e-15) {
                alwan_tl_pass(tmp, img, w, h, 1, w, 0, wts, lw, f32, ext);   /* axis 0: down the columns */
                alwan_tl_pass(res, tmp, h, w, w, 1, 0, wts, lw, f32, ext);   /* axis 1: along the rows */
            } else {
                memcpy(res, img, n * sizeof(double));
            }
            for (i = 0; i < n; i++) res[i] = alwan_tl_r(res[i] - offset, f32);
            break;
        case ALWAN_THRESHOLD_LOCAL_MEAN:
            alwan_tl_pass(tmp, img, w, h, 1, w, 1, NULL, half, f32, ext);
            alwan_tl_pass(res, tmp, h, w, w, 1, 1, NULL, half, f32, ext);
            for (i = 0; i < n; i++) res[i] = alwan_tl_r(res[i] - offset, f32);
            break;
        case ALWAN_THRESHOLD_LOCAL_MEDIAN: {
            double *win = wts;   /* b * b entries follow the kernel */
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    size_t m = 0, u, v;
                    for (u = 0; u < b; u++) {
                        size_t const sy = alwan_tl_scipy_reflect((long)y + (long)u - (long)half, h);
                        for (v = 0; v < b; v++) win[m++] = img[sy * w + alwan_tl_scipy_reflect((long)x + (long)v - (long)half, w)];
                    }
                    qsort(win, m, sizeof(double), alwan_tl_cmp);
                    res[y * w + x] = alwan_tl_r(alwan_tl_r(win[m / 2], f32) - offset, f32);
                }
            }
            break;
        }
        case ALWAN_THRESHOLD_LOCAL_WOLF:
        case ALWAN_THRESHOLD_LOCAL_NICK: {
            double *mean = ib, *sqm = ib + n, *ext2 = ia + (h + b - 1) * w;
            for (i = 0; i < n; i++) img[i] = (double)(float)img[i];   /* OpenCV's float32 image */
            alwan_tl_cv_box(mean, img, w, h, b, 0, ia, ext2);
            alwan_tl_cv_box(sqm, img, w, h, b, 1, ia, ext2);
            for (i = 0; i < n; i++) {
                float const m = (float)mean[i];
                float const var = (float)sqm[i] - m * m;
                tmp[i] = (double)var;                                   /* the variance */
                res[i] = (double)(float)ALWAN_SQRT_F64((double)var);   /* s: NaN where var < 0, as OpenCV's */
            }
            if (method == ALWAN_THRESHOLD_LOCAL_WOLF) {
                float smin = (float)img[0], sdmax = 0.0f;
                int first = 1;
                for (i = 1; i < n; i++)
                    if ((float)img[i] < smin) smin = (float)img[i];
                for (i = 0; i < n; i++) {
                    float const sd = (float)res[i];
                    if (sd == sd && (first || sd > sdmax)) sdmax = sd, first = 0;
                }
                for (i = 0; i < n; i++) {
                    float const m = (float)mean[i], sd = (float)res[i];
                    float const a = m - smin;
                    float const xs = (sd * a) / sdmax;
                    res[i] = (double)(m - kcv * (a - xs));
                }
            } else {
                for (i = 0; i < n; i++) {
                    float const m = (float)mean[i];
                    float const root = (float)ALWAN_SQRT_F64((double)((float)tmp[i] + (float)sqm[i]));
                    res[i] = (double)(m + kcv * root);
                }
            }
            break;
        }
        case ALWAN_THRESHOLD_LOCAL_BRADLEY: {
            double const frac = 1.0 - (p->k != 0.0 ? p->k : 0.15);
            size_t const W1 = w + 1;
            memset(ia, 0, W1 * sizeof(double));
            for (y = 0; y < h; y++) {
                ia[(y + 1) * W1] = 0.0;
                for (x = 0; x < w; x++) ia[(y + 1) * W1 + x + 1] = ia[y * W1 + x + 1] + img[y * w + x];
            }
            for (y = 1; y <= h; y++)
                for (x = 1; x <= w; x++) ia[y * W1 + x] += ia[y * W1 + x - 1];
            for (y = 0; y < h; y++) {
                size_t const y1 = y > half ? y - half : 0, y2 = y + half + 1 < h ? y + half + 1 : h;
                for (x = 0; x < w; x++) {
                    size_t const x1 = x > half ? x - half : 0, x2 = x + half + 1 < w ? x + half + 1 : w;
                    double const sum = ((ia[y2 * W1 + x2] - ia[y1 * W1 + x2]) - ia[y2 * W1 + x1]) + ia[y1 * W1 + x1];
                    double const count = (double)((y2 - y1) * (x2 - x1));
                    res[y * w + x] = alwan_tl_r(sum / count * frac, f32);
                }
            }
            break;
        }
        default: {   /* NIBLACK, SAUVOLA */
            double const area = alwan_tl_r((double)(b * b), f32);
            long const top = (long)half + 1;
            for (y = 0; y < ph; y++) {
                size_t const sy = alwan_tl_numpy_reflect((long)y - top, h);
                for (x = 0; x < pw; x++) {
                    double const v = img[sy * w + alwan_tl_numpy_reflect((long)x - top, w)];
                    ia[y * pw + x] = v;
                    ib[y * pw + x] = alwan_tl_r(v * v, f32);
                }
            }
            for (y = 1; y < ph; y++)
                for (x = 0; x < pw; x++) {
                    ia[y * pw + x] += ia[(y - 1) * pw + x];
                    ib[y * pw + x] += ib[(y - 1) * pw + x];
                }
            for (y = 0; y < ph; y++)
                for (x = 1; x < pw; x++) {
                    ia[y * pw + x] += ia[y * pw + x - 1];
                    ib[y * pw + x] += ib[y * pw + x - 1];
                }
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    size_t const o00 = y * pw + x, o01 = o00 + b, o10 = (y + b) * pw + x, o11 = o10 + b;
                    double m = ((ia[o00] - ia[o01]) - ia[o10]) + ia[o11];
                    double g2 = ((ib[o00] - ib[o01]) - ib[o10]) + ib[o11];
                    double s, t;
                    m = alwan_tl_r(alwan_tl_r(m, f32) / area, f32);
                    g2 = alwan_tl_r(alwan_tl_r(g2, f32) / area, f32);
                    s = alwan_tl_r(g2 - alwan_tl_r(m * m, f32), f32);
                    s = alwan_tl_r(ALWAN_SQRT_F64(s > 0.0 ? s : 0.0), f32);
                    if (method == ALWAN_THRESHOLD_LOCAL_NIBLACK) {
                        t = alwan_tl_r(m - alwan_tl_r(k * s, f32), f32);
                    } else {
                        t = alwan_tl_r(alwan_tl_r(s / r, f32) - 1.0, f32);
                        t = alwan_tl_r(1.0 + alwan_tl_r(k * t, f32), f32);
                        t = alwan_tl_r(m * t, f32);
                    }
                    res[y * w + x] = t;
                }
            }
            break;
        }
        }
        for (y = 0; y < h; y++) {
            char *row = (char *)out + y * out_rs;
            for (x = 0; x < w; x++) {
                if (f32) ((alwan_f32 *)row)[x * ch + c] = (alwan_f32)res[y * w + x];
                else ((alwan_f64 *)row)[x * ch + c] = res[y * w + x];
            }
        }
    }
    ALWAN_FREE(ia);
    ALWAN_FREE(buf);
    return st;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_threshold_local_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height, alwan_threshold_local_method method,
                                       alwan_threshold_local_params const *params) {
    return alwan_tl_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_threshold_local_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height, alwan_threshold_local_method method,
                                       alwan_threshold_local_params const *params) {
    return alwan_tl_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
