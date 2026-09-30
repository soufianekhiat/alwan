/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Non-photorealistic filters on the domain transform.
 *
 * A port of OpenCV 5.0.0's photo module (modules/photo/src/npr.cpp and npr.hpp: the
 * Domain_Filter class behind cv::edgePreservingFilter, cv::detailEnhance, cv::stylization
 * and cv::pencilSketch), after E. S. L. Gastal and M. M. Oliveira, "Domain Transform for
 * Edge-Aware Image and Video Processing", SIGGRAPH 2011. OpenCV is Copyright the OpenCV
 * authors (npr.cpp: Copyright (C) 2013, OpenCV Foundation), Apache License 2.0
 * (https://github.com/opencv/opencv); its notice is kept here and the licence travels with
 * the table this file reads, data/opencv/LICENSE-opencv.txt.
 *
 * The OpenCV calls those functions make are reproduced from the same sources, each as the
 * x64 build computes it with IPP off:
 *   - convertTo 8U to 32F by the float 1/255, and back by 255 with round-half-even and
 *     saturation (convertScaleAbs takes the absolute value first);
 *   - cvtColor BGR2Lab on float (the int16 table alwan_decolor reads), Lab2BGR on float
 *     (color_lab.cpp's Lab2RGBfloat: blocks of eight pixels on the SSE baseline, whose
 *     reciprocal constants and unfused multiply-adds differ from the scalar tail's
 *     divisions; the sRGB curve a cubic spline over 1024 intervals, the coefficients and
 *     the nine matrix entries read from data/opencv/lab2srgb_float.csv), and BGR2YCrCb /
 *     YCrCb2BGR on float (dispatched to AVX2: fused multiply-adds in blocks of eight, the
 *     tail unfused);
 *   - Sobel 3 x 3 on float, BORDER_REFLECT_101: the [1 2 1] pass sums (a + c) + 2b on the
 *     vector blocks of eight and (a + 2b) + c on the tail (SymmColumnSmallFilter's scalar
 *     code);
 *   - magnitude, sqrt(fma(x, x, y y)) when there are sixteen or more values, the scalar
 *     sqrt(x x + y y) otherwise;
 *   - the Domain_Filter itself, scalar C++ in single precision: std::pow(float, float) is
 *     powf, the recursive filter's feedback exp and the iteration's sigma are in double.
 *
 * OpenCV works on BGR; alwan takes RGB and hands the port BGR, since the normalised
 * convolution's index arithmetic is not symmetric in the channels.
 *
 * ALWAN_STYLIZE_OIL_PAINTING ports opencv_contrib 5.0.0's xphoto oilPainting
 * (modules/xphoto/src/oilpainting.cpp, after the histogram method in G. J. Holzmann,
 * "Beyond Photography: The Digital Darkroom", 1988). opencv_contrib is Copyright the
 * OpenCV authors, Apache License 2.0 (https://github.com/opencv/opencv_contrib), licence
 * text in data/opencv/LICENSE-opencv_contrib.txt. Its luminance is cvtColor BGR2GRAY on 8
 * bits, ((b 3735 + g 19235 + r 9798 + 2^14) >> 15), quantised by cvRound(l / dynRatio);
 * each output pixel is the mean colour of the (2 size + 1)^2 window's most frequent
 * quantised luminance, the window's histogram and float sums slid along the row. The mean
 * is the float sum times 1 / count in double, rounded to float; three channels round it to
 * 8 bits half to even, one channel truncates it (OpenCV's static_cast). OpenCV then
 * requantises a copy of its result by dynRatio after handing the result back, which has
 * no effect, and nor does it here.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>

/* Lab2RGBfloat for blueIdx 0: the nine coefficients C0..C8, then the sRGB spline, four
 * coefficients per interval of 1024. */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static float const st_lab_tab[9 + 4 * 1024] = {
#include "../data/opencv/lab2srgb_float.csv"
};
ALWAN_DIAG_POP

#define ST_ITERS 3

/* ------------------------------------------------------------------------------------ */
/* convertTo / convertScaleAbs to 8 bits: y = v * 255 in float, cvRound (ties to even),   */
/* saturated. cvRound of NaN, of an infinity or of anything from 2^31 up is INT_MIN (the     */
/* SSE conversion's overflow value), which saturates to 0, not 255.                       */
/* ------------------------------------------------------------------------------------ */

static unsigned char st_to_u8(float v, int absolute) {
    float y = v * 255.0f;
    int i;
    float fr;
    if (absolute && y < 0.0f) y = -y;
    if (!(y > 0.0f) || !(y < 2147483648.0f)) return 0;
    if (y >= 255.0f) return 255;
    i = (int)y;
    fr = y - (float)i;
    if (fr > 0.5f || (fr == 0.5f && (i & 1))) i++;
    return (unsigned char)i;
}

static float st_fabs(float v) {
    return v < 0.0f ? -v : v;
}

/* ------------------------------------------------------------------------------------ */
/* Domain_Filter                                                                          */
/* ------------------------------------------------------------------------------------ */

typedef struct {
    int h, w;
    float *horiz, *vert;    /* h x w: 1 + (sigma_s / sigma_r) * the L1 neighbour distance */
    float *ct_h, *ct_v;     /* h x w: their running sums along the rows and the columns */
} st_domain;

static void st_domain_free(st_domain *d) {
    free(d->horiz); free(d->vert); free(d->ct_h); free(d->ct_v);
    memset(d, 0, sizeof *d);
}

/* Domain_Filter::init: img is h x w x c. */
static int st_init(st_domain *d, float const *img, int h, int w, int c, int cumulative, float ss, float sr) {
    size_t const n = (size_t)h * (size_t)w;
    float const k = ss / sr;
    int i, j, ch;
    memset(d, 0, sizeof *d);
    d->h = h; d->w = w;
    d->horiz = (float *)calloc(n, sizeof(float));
    d->vert = (float *)calloc(n, sizeof(float));
    if (!d->horiz || !d->vert) return 0;
    /* distx, disty accumulated channel by channel into zeros, the first column / row 0 */
    for (i = 0; i < h; i++) {
        for (j = 0; j < w - 1; j++) {
            float const *p = img + ((size_t)i * (size_t)w + (size_t)j) * (size_t)c;
            float *dst = d->horiz + (size_t)i * (size_t)w + (size_t)j + 1;
            for (ch = 0; ch < c; ch++) *dst = *dst + st_fabs(p[c + ch] - p[ch]);
        }
    }
    for (i = 0; i < h - 1; i++) {
        for (j = 0; j < w; j++) {
            float const *p = img + ((size_t)i * (size_t)w + (size_t)j) * (size_t)c;
            float const *q = p + (size_t)w * (size_t)c;
            float *dst = d->vert + (size_t)(i + 1) * (size_t)w + (size_t)j;
            for (ch = 0; ch < c; ch++) *dst = *dst + st_fabs(q[ch] - p[ch]);
        }
    }
    for (i = 0; (size_t)i < n; i++) {
        float const tx = d->horiz[i] * k;
        float const ty = d->vert[i] * k;
        d->horiz[i] = 1.0f + tx;
        d->vert[i] = 1.0f + ty;
    }
    if (cumulative) {
        d->ct_h = (float *)malloc(n * sizeof(float));
        d->ct_v = (float *)malloc(n * sizeof(float));
        if (!d->ct_h || !d->ct_v) return 0;
        for (i = 0; i < h; i++) {
            float *row = d->ct_h + (size_t)i * (size_t)w;
            float const *hz = d->horiz + (size_t)i * (size_t)w;
            row[0] = hz[0];
            for (j = 1; j < w; j++) row[j] = hz[j] + row[j - 1];
        }
        for (j = 0; j < w; j++) {
            d->ct_v[j] = d->vert[j];
            for (i = 1; i < h; i++)
                d->ct_v[(size_t)i * (size_t)w + (size_t)j] = d->vert[(size_t)i * (size_t)w + (size_t)j] +
                                                            d->ct_v[(size_t)(i - 1) * (size_t)w + (size_t)j];
        }
    }
    return 1;
}

/* sigma_h of iteration it, as filter() and pencil_sketch() compute it in double. */
static float st_sigma_h(float ss, int it) {
    double const two_k = (double)(1 << (ST_ITERS - (it + 1)));
    return (float)((double)ss * ALWAN_SQRT_F64(3.0) * two_k / ALWAN_SQRT_F64(63.0));
}

static void st_transpose(float *dst, float const *src, int h, int w, int c) {
    int i, j, ch;
    for (i = 0; i < h; i++)
        for (j = 0; j < w; j++)
            for (ch = 0; ch < c; ch++)
                dst[((size_t)j * (size_t)h + (size_t)i) * (size_t)c + (size_t)ch] =
                    src[((size_t)i * (size_t)w + (size_t)j) * (size_t)c + (size_t)ch];
}

/* compute_Rfilter: the recursive filter along the rows of O (h x w x c), both ways. */
static void st_rfilter(float *o, float const *hz, int h, int w, int c, float sigma_h, float *v) {
    float const a = (float)ALWAN_EXP_F64((-1.0 * ALWAN_SQRT_F64(2.0)) / (double)sigma_h);
    size_t const n = (size_t)h * (size_t)w;
    size_t idx;
    int i, j, ch;
    for (idx = 0; idx < n; idx++) v[idx] = ALWAN_POW_F32(a, hz[idx]);
    for (i = 0; i < h; i++) {
        float *row = o + (size_t)i * (size_t)w * (size_t)c;
        float const *vr = v + (size_t)i * (size_t)w;
        for (j = 1; j < w; j++)
            for (ch = 0; ch < c; ch++) {
                float const cur = row[j * c + ch];
                row[j * c + ch] = cur + (row[(j - 1) * c + ch] - cur) * vr[j];
            }
    }
    for (i = 0; i < h; i++) {
        float *row = o + (size_t)i * (size_t)w * (size_t)c;
        float const *vr = v + (size_t)i * (size_t)w;
        for (j = w - 2; j >= 0; j--)
            for (ch = 0; ch < c; ch++) {
                float const cur = row[j * c + ch];
                row[j * c + ch] = cur + (row[(j + 1) * c + ch] - cur) * vr[j + 1];
            }
    }
}

/* compute_boxfilter: for each pixel, the first positions along its row whose domain
 * coordinate passes hz - radius and hz + radius, plus one, as floats. dom is w + 1
 * scratch; tl, tu are w scratch. */
static void st_box_idx(float *lower, float *upper, float const *hz, int h, int w, float radius,
                       float *dom, float *tl, float *tu) {
    int i, j, k;
    int temp = 0;
    for (i = 0; i < h; i++) {
        float const *hr = hz + (size_t)i * (size_t)w;
        float lo0, up0;
        for (j = 0; j < w; j++) dom[j] = hr[j];
        dom[w] = FLT_MAX;   /* OpenCV's infinity; only ever compared with a finite position */
        for (j = 0; j < w; j++) { tl[j] = 0.0f; tu[j] = 0.0f; }
        lo0 = hr[0] - radius;
        up0 = hr[0] + radius;
        for (j = 0; j < w; j++)
            if (dom[j] > lo0) { tl[0] = (float)j; break; }
        for (j = 0; j < w; j++)
            if (dom[j] > up0) { tu[0] = (float)j; break; }
        temp = 0;
        for (j = 1; j < w; j++) {
            float const lo = hr[j] - radius;
            float const up = hr[j] + radius;
            int count = 0;
            for (k = (int)tl[j - 1]; k < w + 1; k++) {
                if (dom[k] > lo) { temp = count; break; }
                count++;
            }
            tl[j] = tl[j - 1] + (float)temp;
            count = 0;
            for (k = (int)tu[j - 1]; k < w + 1; k++) {
                if (dom[k] > up) { temp = count; break; }
                count++;
            }
            tu[j] = tu[j - 1] + (float)temp;
        }
        for (j = 0; j < w; j++) {
            lower[(size_t)i * (size_t)w + (size_t)j] = tl[j] + 1.0f;
            upper[(size_t)i * (size_t)w + (size_t)j] = tu[j] + 1.0f;
        }
    }
}

typedef struct {
    float *lower, *upper, *box, *dom, *tl, *tu;
} st_nc_scratch;

static size_t st_at(int i, int w, int j) {
    return (size_t)i * (size_t)w + (size_t)j;
}

/* compute_NCfilter on O (h x w x 3), in place: the box filter of radius in the domain,
 * from a running sum along the row, indexed as OpenCV decodes its flat indices. */
static void st_ncfilter(float *o, float const *hz, int h, int w, float radius, st_nc_scratch const *s, float *fin) {
    int const rowlen = (w + 1) * 3;
    float const big_h = (float)(h * (w + 1));
    int i, j, ch;
    st_box_idx(s->lower, s->upper, hz, h, w, radius, s->dom, s->tl, s->tu);
    for (i = 0; i < h; i++) {
        float *b = s->box + (size_t)i * (size_t)rowlen;
        float const *orow = o + (size_t)i * (size_t)w * 3;
        b[0] = b[1] = b[2] = 0.0f;
        b[3] = orow[0]; b[4] = orow[1]; b[5] = orow[2];
        for (j = 2; j < w + 1; j++)
            for (ch = 0; ch < 3; ch++) b[j * 3 + ch] = orow[(j - 1) * 3 + ch] + b[(j - 1) * 3 + ch];
    }
    for (ch = 0; ch < 3; ch++) {
        float const t1 = ((float)(ch + 1) - 1.0f) * big_h;
        int const hh = h * (w + 1);
        for (i = 0; i < h; i++) {
            float const ind = (float)i + 1.0f;
            for (j = 0; j < w; j++) {
                size_t const at = st_at(i, w, j);
                float const lw = s->lower[at], up = s->upper[at];
                float const a = (t1 + (lw - 1.0f) * (float)h) + ind;
                float const bb = (t1 + (up - 1.0f) * (float)h) + ind;
                int r, rem, q, p, r1, rem1, q1, p1;
                float v0 = 0.0f, v1 = 0.0f;
                r = (int)bb / hh;
                rem = (int)bb - r * hh;
                q = rem / h;
                p = rem - q * h;
                if (q == 0) { p = h; q = w; r = r - 1; }
                if (p == 0) { p = h; q = q - 1; }
                r1 = (int)a / hh;
                rem1 = (int)a - r1 * hh;
                q1 = rem1 / h;
                p1 = rem1 - q1 * h;
                if (p1 == 0) { p1 = h; q1 = q1 - 1; }
                /* These leave the table when a whole row's domain is shorter than the
                 * radius (a flat row under about 91 pixels at the defaults): the first
                 * pixel's upper index stays 1, OpenCV reads one float past its buffer and
                 * divides by upper - lower = 0. Whatever it read, the pixel is infinite
                 * or NaN, the next pass spreads that through the running sums, and the
                 * 8-bit conversion makes every such pixel 0 (st_to_u8). alwan reads 0
                 * there and reaches the same non-finite values. */
                if (p >= 1 && p <= h && q >= 0 && q <= w && r >= 0 && r <= 2)
                    v0 = s->box[(size_t)(p - 1) * (size_t)rowlen + (size_t)q * 3 + (size_t)(2 - r)];
                if (p1 >= 1 && p1 <= h && q1 >= 0 && q1 <= w && r1 >= 0 && r1 <= 2)
                    v1 = s->box[(size_t)(p1 - 1) * (size_t)rowlen + (size_t)q1 * 3 + (size_t)(2 - r1)];
                fin[at * 3 + (size_t)(2 - ch)] = (v0 - v1) / (up - lw);
            }
        }
    }
    memcpy(o, fin, (size_t)h * (size_t)w * 3 * sizeof(float));
}

/* Domain_Filter::filter: RECURS_FILTER (1) on c channels or NORMCONV_FILTER (2) on 3.
 * o holds the image in and the result out. */
static int st_filter(float *o, int h, int w, int c, float ss, float sr, int flags) {
    st_domain d;
    size_t const n = (size_t)h * (size_t)w;
    size_t const m = (size_t)(h > w ? h : w);
    float *ot = (float *)malloc(n * (size_t)c * sizeof(float));
    float *vt = (float *)malloc(n * sizeof(float));
    float *scratch = (float *)malloc(n * sizeof(float));
    int ok, it;
    memset(&d, 0, sizeof d);
    ok = ot && vt && scratch && st_init(&d, o, h, w, c, flags == 2, ss, sr);
    if (ok && flags == 1) {
        st_transpose(vt, d.vert, h, w, 1);
        for (it = 0; it < ST_ITERS; it++) {
            float const sh = st_sigma_h(ss, it);
            st_rfilter(o, d.horiz, h, w, c, sh, scratch);
            st_transpose(ot, o, h, w, c);
            st_rfilter(ot, vt, w, h, c, sh, scratch);
            st_transpose(o, ot, w, h, c);
        }
    } else if (ok) {
        st_nc_scratch s;
        float *fin = (float *)malloc(n * 3 * sizeof(float));
        s.lower = (float *)malloc(n * sizeof(float));
        s.upper = (float *)malloc(n * sizeof(float));
        s.box = (float *)malloc((size_t)h * (size_t)(w + 1) * 3 * sizeof(float) +
                                (size_t)w * (size_t)(h + 1) * 3 * sizeof(float));
        s.dom = (float *)malloc((m + 1) * sizeof(float));
        s.tl = (float *)malloc(m * sizeof(float));
        s.tu = (float *)malloc(m * sizeof(float));
        ok = fin && s.lower && s.upper && s.box && s.dom && s.tl && s.tu;
        if (ok) {
            st_transpose(vt, d.ct_v, h, w, 1);
            for (it = 0; it < ST_ITERS; it++) {
                float const radius = (float)ALWAN_SQRT_F64(3.0) * st_sigma_h(ss, it);
                st_ncfilter(o, d.ct_h, h, w, radius, &s, fin);
                st_transpose(ot, o, h, w, 3);
                st_ncfilter(ot, vt, w, h, radius, &s, fin);
                st_transpose(o, ot, w, h, 3);
            }
        }
        free(fin); free(s.lower); free(s.upper); free(s.box); free(s.dom); free(s.tl); free(s.tu);
    }
    st_domain_free(&d);
    free(ot); free(vt); free(scratch);
    return ok;
}

/* ------------------------------------------------------------------------------------ */
/* Colour conversions on float BGR rows                                                   */
/* ------------------------------------------------------------------------------------ */

static float st_spline(float x) {
    int ix = (int)x;
    float const *t;
    if (ix < 0) ix = 0;
    if (ix > 1023) ix = 1023;
    x -= (float)ix;
    t = st_lab_tab + 9 + ix * 4;
    return ((t[3] * x + t[2]) * x + t[1]) * x + t[0];
}

static float st_clip01(float v) {
    return v < 0.0f ? 0.0f : (v <= 1.0f ? v : 1.0f);
}

/* Lab2RGBfloat on one row of n pixels, Lab in, BGR out (may alias). */
static void st_lab_to_bgr_row(float *dst, float const *src, int n) {
    float const *c = st_lab_tab;
    float const lthresh = 8.0f;
    float const fthresh = 6.0f / 29.0f;
    int const nv = n / 8 * 8;
    int i, k;
    for (i = 0; i < n; i++) {
        float const li = src[i * 3], ai = src[i * 3 + 1], bi = src[i * 3 + 2];
        float y, fy, fxz[2], x, z, rgb[3];
        if (i < nv) {
            /* the SSE baseline's blocks of eight: reciprocal constants, unfused */
            float const ylo = li * (1.f / 903.3f);
            float const fylo = 7.787f * ylo + 16.0f / 116.0f;
            float const fyhi = (li + 16.0f) * (1.f / 116.0f);
            float const yhi = fyhi * fyhi * fyhi;
            int const lo = li <= lthresh;
            y = lo ? ylo : yhi;
            fy = lo ? fylo : fyhi;
            fxz[0] = ai * (1.f / 500.f) + fy;
            fxz[1] = bi * (-1.f / 200.f) + fy;
            for (k = 0; k < 2; k++) {
                float const f = fxz[k];
                fxz[k] = f <= fthresh ? (f - 16.0f / 116.0f) * (1.f / 7.787f) : f * f * f;
            }
            x = fxz[0]; z = fxz[1];
            rgb[0] = c[0] * x + (c[1] * y + c[2] * z);
            rgb[1] = c[3] * x + (c[4] * y + c[5] * z);
            rgb[2] = c[6] * x + (c[7] * y + c[8] * z);
        } else {
            if (li <= lthresh) {
                y = li / 903.3f;
                fy = 7.787f * y + 16.0f / 116.0f;
            } else {
                fy = (li + 16.0f) / 116.0f;
                y = fy * fy * fy;
            }
            fxz[0] = ai / 500.0f + fy;
            fxz[1] = fy - bi / 200.0f;
            for (k = 0; k < 2; k++) {
                float const f = fxz[k];
                fxz[k] = f <= fthresh ? (f - 16.0f / 116.0f) / 7.787f : f * f * f;
            }
            x = fxz[0]; z = fxz[1];
            rgb[0] = c[0] * x + c[1] * y + c[2] * z;
            rgb[1] = c[3] * x + c[4] * y + c[5] * z;
            rgb[2] = c[6] * x + c[7] * y + c[8] * z;
        }
        for (k = 0; k < 3; k++) dst[i * 3 + k] = st_spline(st_clip01(rgb[k]) * 1024.0f);
    }
}

#define ST_R2Y 0.299f
#define ST_G2Y 0.587f
#define ST_B2Y 0.114f
#define ST_YCR 0.713f
#define ST_YCB 0.564f
#define ST_CR2R 1.403f
#define ST_CR2G (-0.714f)
#define ST_CB2G (-0.344f)
#define ST_CB2B 1.773f

/* RGB2YCrCb_f<float> for BGR: fused blocks of eight (AVX2), the tail unfused. */
static void st_bgr_to_ycrcb_row(float *dst, float const *src, int n) {
    int const nv = n / 8 * 8;
    int i;
    for (i = 0; i < n; i++) {
        float const b = src[i * 3], g = src[i * 3 + 1], r = src[i * 3 + 2];
        float y;
        if (i < nv) {
            y = ALWAN_FMA_CR_F32(b, ST_B2Y, ALWAN_FMA_CR_F32(g, ST_G2Y, r * ST_R2Y));
            dst[i * 3 + 1] = ALWAN_FMA_CR_F32(r - y, ST_YCR, 0.5f);
            dst[i * 3 + 2] = ALWAN_FMA_CR_F32(b - y, ST_YCB, 0.5f);
        } else {
            y = b * ST_B2Y + g * ST_G2Y + r * ST_R2Y;
            dst[i * 3 + 1] = (r - y) * ST_YCR + 0.5f;
            dst[i * 3 + 2] = (b - y) * ST_YCB + 0.5f;
        }
        dst[i * 3] = y;
    }
}

/* YCrCb2RGB_f<float> to BGR. */
static void st_ycrcb_to_bgr_row(float *dst, float const *src, int n) {
    int const nv = n / 8 * 8;
    int i;
    for (i = 0; i < n; i++) {
        float const y = src[i * 3];
        float const cr = src[i * 3 + 1] - 0.5f, cb = src[i * 3 + 2] - 0.5f;
        if (i < nv) {
            dst[i * 3] = ALWAN_FMA_CR_F32(cb, ST_CB2B, y);
            dst[i * 3 + 1] = ALWAN_FMA_CR_F32(cr, ST_CR2G, ALWAN_FMA_CR_F32(cb, ST_CB2G, y));
            dst[i * 3 + 2] = ALWAN_FMA_CR_F32(cr, ST_CR2R, y);
        } else {
            dst[i * 3] = y + cb * ST_CB2B;
            dst[i * 3 + 1] = y + cb * ST_CB2G + cr * ST_CR2G;
            dst[i * 3 + 2] = y + cr * ST_CR2R;
        }
    }
}

/* ------------------------------------------------------------------------------------ */
/* Sobel 3 x 3 and magnitude, for stylization's find_magnitude                            */
/* ------------------------------------------------------------------------------------ */

static int st_reflect101(int p, int n) {
    if (n == 1) return 0;
    while (p < 0 || p >= n) p = p < 0 ? -p : 2 * n - 2 - p;
    return p;
}

/* [1 2 1] as SymmColumnSmallVec_32f and SymmColumnSmallFilter compute it. */
static float st_smooth(float a, float b, float c, int vec) {
    return vec ? (a + c) + b * 2.0f : (a + b * 2.0f) + c;
}

/* One plane (h x w, stride 3) to its Sobel dx and dy. */
static void st_sobel(float *gx, float *gy, float const *plane, int h, int w) {
    int const nv = w / 8 * 8;
    int i, j;
    for (i = 0; i < h; i++) {
        int const rows[3] = {st_reflect101(i - 1, h), i, st_reflect101(i + 1, h)};
        for (j = 0; j < w; j++) {
            int const jl = st_reflect101(j - 1, w), jr = st_reflect101(j + 1, w);
            float d[3], s[3];
            int k;
            for (k = 0; k < 3; k++) {
                float const *row = plane + (size_t)rows[k] * (size_t)w * 3;
                d[k] = row[jr * 3] - row[jl * 3];
                s[k] = st_smooth(row[jl * 3], row[j * 3], row[jr * 3], j < nv);
            }
            gx[(size_t)i * (size_t)w + (size_t)j] = st_smooth(d[0], d[1], d[2], j < nv) + 0.0f;
            gy[(size_t)i * (size_t)w + (size_t)j] = (s[2] - s[0]) + 0.0f;
        }
    }
}

/* Domain_Filter::find_magnitude on a BGR image: 1 - the sum of the planes' magnitudes. */
static int st_magnitude(float *mag, float const *img, int h, int w) {
    size_t const n = (size_t)h * (size_t)w;
    float *gx = (float *)malloc(n * sizeof(float));
    float *gy = (float *)malloc(n * sizeof(float));
    size_t i;
    int ch;
    if (!gx || !gy) { free(gx); free(gy); return 0; }
    for (ch = 0; ch < 3; ch++) {
        st_sobel(gx, gy, img + ch, h, w);
        for (i = 0; i < n; i++) {
            float const x = gx[i], y = gy[i];
            float const m = n >= 16 ? ALWAN_SQRT_F32(ALWAN_FMA_CR_F32(x, x, y * y))
                                    : ALWAN_SQRT_F32(x * x + y * y);
            mag[i] = ch == 0 ? m : mag[i] + m;
        }
    }
    for (i = 0; i < n; i++) mag[i] = 1.0f - mag[i];
    free(gx); free(gy);
    return 1;
}

/* ------------------------------------------------------------------------------------ */
/* xphoto::oilPainting (ParallelOilPainting)                                              */
/* ------------------------------------------------------------------------------------ */

/* saturate_cast<uchar>(float): cvRound, ties to even, INT_MIN (NaN, an infinity, 2^31 and
 * up) saturating to 0 */
static unsigned char st_round_u8(float y) {
    int i;
    float fr;
    if (!(y > -2147483648.0f) || !(y < 2147483648.0f)) return 0;
    if (y <= 0.5f) return 0;
    if (y >= 255.0f) return 255;
    i = (int)y;
    fr = y - (float)i;
    if (fr > 0.5f || (fr == 0.5f && (i & 1))) i++;
    return (unsigned char)i;
}

static void st_oil_add(int *hist, float (*mean)[3], int k, unsigned char const *px, size_t ch, int sign) {
    hist[k] += sign;
    if (ch == 1) {
        if (sign > 0) mean[k][0] += (float)px[0]; else mean[k][0] -= (float)px[0];
    } else if (sign > 0) {
        mean[k][0] += (float)px[2];
        mean[k][1] += (float)px[1];
        mean[k][2] += (float)px[0];
    } else {
        mean[k][0] -= (float)px[2];
        mean[k][1] -= (float)px[1];
        mean[k][2] -= (float)px[0];
    }
}

/* src and out are w x h, ch 1, 3 or 4 (the fourth copied through), strides in bytes; out
 * must not overlap src (the caller copies). mean[] holds B, G, R as OpenCV's Vec3f does. */
static alwan_status st_oil(unsigned char *out, size_t out_rs, unsigned char const *src, size_t src_rs,
                           int w, int h, size_t ch, int halfsize, int dyn) {
    size_t const n = (size_t)w * (size_t)h;
    unsigned char *lum = (unsigned char *)malloc(n);
    int hist[256];
    float mean[256][3];
    double const dratio = 1 / (double)dyn;
    int x, y, yy, xx, i;
    if (!lum) return ALWAN_E_NOMEM;

    for (y = 0; y < h; y++) {
        unsigned char const *s = src + (size_t)y * src_rs;
        for (x = 0; x < w; x++) {
            int l;
            double v, f;
            long r;
            if (ch == 1) {
                l = s[x];
            } else {
                int const rr = s[(size_t)x * ch], g = s[(size_t)x * ch + 1], b = s[(size_t)x * ch + 2];
                l = (b * 3735 + g * 19235 + rr * 9798 + (1 << 14)) >> 15;
            }
            /* saturate_cast<uchar>(cvRound(l * dratio)) */
            v = (double)l * dratio;
            f = ALWAN_FLOOR_F64(v);
            r = (long)f;
            if (v - f > 0.5 || (v - f == 0.5 && (r & 1))) r++;
            lum[(size_t)y * (size_t)w + (size_t)x] = (unsigned char)(r > 255 ? 255 : r);
        }
    }

    for (y = 0; y < h; y++) {
        unsigned char *d = out + (size_t)y * out_rs;
        for (x = 0; x < w; x++) {
            int pos = 0;
            if (x == 0) {
                memset(hist, 0, sizeof hist);
                memset(mean, 0, sizeof mean);
                for (yy = -halfsize; yy <= halfsize; yy++) {
                    if (y + yy >= 0 && y + yy < h) {
                        unsigned char const *vp = src + (size_t)(y + yy) * src_rs;
                        unsigned char const *uc = lum + (size_t)(y + yy) * (size_t)w;
                        for (xx = 0; xx <= halfsize && x + xx < w; xx++)
                            st_oil_add(hist, mean, uc[x + xx], vp + (size_t)(x + xx) * ch, ch, 1);
                    }
                }
            } else {
                for (yy = -halfsize; yy <= halfsize; yy++) {
                    if (y + yy >= 0 && y + yy < h) {
                        unsigned char const *vp = src + (size_t)(y + yy) * src_rs;
                        unsigned char const *uc = lum + (size_t)(y + yy) * (size_t)w;
                        xx = x - halfsize - 1;
                        if (xx >= 0 && xx < w) st_oil_add(hist, mean, uc[xx], vp + (size_t)xx * ch, ch, -1);
                        xx = x + halfsize;
                        if (xx >= 0 && xx < w) st_oil_add(hist, mean, uc[xx], vp + (size_t)xx * ch, ch, 1);
                    }
                }
            }
            /* std::max_element: the first of the largest counts */
            for (i = 1; i < 256; i++)
                if (hist[i] > hist[pos]) pos = i;
            {
                /* Vec3f / int: each sum times 1. / count in double, saturate_cast<float> */
                double const inv = 1. / hist[pos];
                if (ch == 1) {
                    float const m = (float)((double)mean[pos][0] * inv);
                    d[x] = (unsigned char)m; /* static_cast<uint8_t>: truncation */
                } else {
                    d[(size_t)x * ch + 2] = st_round_u8((float)((double)mean[pos][0] * inv));
                    d[(size_t)x * ch + 1] = st_round_u8((float)((double)mean[pos][1] * inv));
                    d[(size_t)x * ch] = st_round_u8((float)((double)mean[pos][2] * inv));
                    if (ch == 4) d[(size_t)x * 4 + 3] = src[(size_t)y * src_rs + (size_t)x * 4 + 3];
                }
            }
        }
    }
    free(lum);
    return ALWAN_OK;
}

/* ------------------------------------------------------------------------------------ */
/* The API                                                                                */
/* ------------------------------------------------------------------------------------ */

static int st_param(double *out, double v, double def) {
    if (!(v == v) || v < 0.0 || v > 1e30) return 0;
    *out = v == 0.0 ? def : v;
    return 1;
}

alwan_status alwan_stylize(unsigned char *out, size_t out_row_stride,
                           unsigned char const *src, size_t src_row_stride,
                           size_t width, size_t height, size_t channels,
                           alwan_stylize_method method, alwan_stylize_params const *params) {
    alwan_stylize_params p;
    double ss, sr, shade;
    double const def_ss = method == ALWAN_STYLIZE_DETAIL_ENHANCE ? 10.0 : 60.0;
    double const def_sr = method == ALWAN_STYLIZE_EDGE_PRESERVING_RECURSIVE ||
                          method == ALWAN_STYLIZE_EDGE_PRESERVING_NORMCONV ? 0.4 :
                          method == ALWAN_STYLIZE_DETAIL_ENHANCE ? 0.15 :
                          method == ALWAN_STYLIZE_STYLIZATION ? 0.45 : 0.07;
    size_t const out_ch = method == ALWAN_STYLIZE_PENCIL_SKETCH_GREY ? 1 : channels;
    int h, w;
    size_t n, i, x, y;
    float *img, *work = NULL;
    alwan_status st = ALWAN_OK;

    if (!out || !src) return ALWAN_E_INVALID;
    if (method == ALWAN_STYLIZE_OIL_PAINTING) {
        size_t sz, dyn;
        unsigned char *copy;
        if (channels != 1 && channels != 3 && channels != 4) return ALWAN_E_INVALID;
        if (width < 1 || height < 1 || width > 46340 || height > 46340) return ALWAN_E_INVALID;
        if (src_row_stride < width * channels || out_row_stride < width * channels) return ALWAN_E_INVALID;
        if (params) p = *params; else memset(&p, 0, sizeof p);
        sz = p.oil_size == 0 ? 10 : p.oil_size;
        dyn = p.oil_dyn_ratio == 0 ? 1 : p.oil_dyn_ratio;
        if (sz > 46340 || dyn > 127) return ALWAN_E_INVALID;
        /* the window reads the source while the result is written: work from a copy */
        copy = (unsigned char *)malloc(width * height * channels);
        if (!copy) return ALWAN_E_NOMEM;
        for (y = 0; y < height; y++) memcpy(copy + y * width * channels, src + y * src_row_stride, width * channels);
        st = st_oil(out, out_row_stride, copy, width * channels, (int)width, (int)height, channels, (int)sz, (int)dyn);
        free(copy);
        return st;
    }
    if (channels != 3 && channels != 4) return ALWAN_E_INVALID;
    if (width < 2 || height < 2 || width > 46340 || height > 46340) return ALWAN_E_INVALID;
    if (method < ALWAN_STYLIZE_EDGE_PRESERVING_RECURSIVE || method > ALWAN_STYLIZE_PENCIL_SKETCH_COLOR)
        return ALWAN_E_INVALID;
    if (src_row_stride < width * channels || out_row_stride < width * out_ch) return ALWAN_E_INVALID;
    if (params) p = *params; else memset(&p, 0, sizeof p);
    if (!st_param(&ss, p.sigma_s, def_ss) || !st_param(&sr, p.sigma_r, def_sr) ||
        !st_param(&shade, p.shade_factor, 0.02))
        return ALWAN_E_INVALID;

    h = (int)height;
    w = (int)width;
    n = width * height;
    img = (float *)malloc(n * 3 * sizeof(float));
    if (!img) return ALWAN_E_NOMEM;
    /* convertTo(CV_32FC3, 1.0 / 255.0), in BGR order */
    for (y = 0; y < height; y++) {
        unsigned char const *s = src + y * src_row_stride;
        float *d = img + y * width * 3;
        for (x = 0; x < width; x++) {
            float const inv = (float)(1.0 / 255.0);
            d[x * 3] = (float)s[x * channels + 2] * inv;
            d[x * 3 + 1] = (float)s[x * channels + 1] * inv;
            d[x * 3 + 2] = (float)s[x * channels] * inv;
        }
    }

    switch (method) {
    case ALWAN_STYLIZE_EDGE_PRESERVING_RECURSIVE:
    case ALWAN_STYLIZE_EDGE_PRESERVING_NORMCONV:
        if (!st_filter(img, h, w, 3, (float)ss, (float)sr,
                       method == ALWAN_STYLIZE_EDGE_PRESERVING_RECURSIVE ? 1 : 2))
            st = ALWAN_E_NOMEM;
        break;
    case ALWAN_STYLIZE_DETAIL_ENHANCE: {
        float *l = (float *)malloc(n * sizeof(float));
        float *res = (float *)malloc(n * sizeof(float));
        if (!l || !res) { st = ALWAN_E_NOMEM; free(l); free(res); break; }
        for (i = 0; i < n; i++) {
            float lab[3];
            alwan__cv_rgb_to_lab_f32(lab, img[i * 3 + 2], img[i * 3 + 1], img[i * 3]);
            img[i * 3] = lab[0]; img[i * 3 + 1] = lab[1]; img[i * 3 + 2] = lab[2];
            l[i] = lab[0] * (float)(1.0 / 255.0);
            res[i] = l[i];
        }
        if (!st_filter(res, h, w, 1, (float)ss, (float)sr, 1)) {
            st = ALWAN_E_NOMEM;
        } else {
            for (i = 0; i < n; i++) {
                float detail = l[i] - res[i];
                detail = detail * 3.0f;
                img[i * 3] = (res[i] + detail) * 255.0f;
            }
            for (y = 0; y < height; y++) st_lab_to_bgr_row(img + y * width * 3, img + y * width * 3, w);
        }
        free(l); free(res);
        break;
    }
    case ALWAN_STYLIZE_STYLIZATION: {
        float *mag = (float *)malloc(n * sizeof(float));
        if (!mag) { st = ALWAN_E_NOMEM; break; }
        if (!st_filter(img, h, w, 3, (float)ss, (float)sr, 2) || !st_magnitude(mag, img, h, w)) {
            st = ALWAN_E_NOMEM;
        } else {
            for (i = 0; i < n; i++) {
                img[i * 3] = img[i * 3] * mag[i];
                img[i * 3 + 1] = img[i * 3 + 1] * mag[i];
                img[i * 3 + 2] = img[i * 3 + 2] * mag[i];
            }
        }
        free(mag);
        break;
    }
    default: {   /* the pencil sketch: the box filter's widths at the first iteration only */
        st_domain d;
        memset(&d, 0, sizeof d);
        size_t const m = (size_t)(h > w ? h : w);
        float *pen = (float *)malloc(n * sizeof(float));
        float *lo = (float *)malloc(n * sizeof(float));
        float *up = (float *)malloc(n * sizeof(float));
        float *vt = (float *)malloc(n * sizeof(float));
        float *dom = (float *)malloc((m + 1) * sizeof(float));
        float *tl = (float *)malloc(m * sizeof(float));
        float *tu = (float *)malloc(m * sizeof(float));
        work = (float *)malloc(n * 3 * sizeof(float));
        if (!pen || !lo || !up || !vt || !dom || !tl || !tu || !work ||
            !st_init(&d, img, h, w, 3, 1, (float)ss, (float)sr)) {
            st = ALWAN_E_NOMEM;
        } else {
            float const radius = (float)ALWAN_SQRT_F64(3.0) * st_sigma_h((float)ss, 0);
            float const sf = (float)shade;
            st_box_idx(lo, up, d.ct_h, h, w, radius, dom, tl, tu);
            for (i = 0; i < n; i++) pen[i] = up[i] - lo[i];
            st_transpose(vt, d.ct_v, h, w, 1);
            st_box_idx(lo, up, vt, w, h, radius, dom, tl, tu);
            for (y = 0; y < height; y++)
                for (x = 0; x < width; x++) {
                    size_t const t = x * height + y;
                    size_t const at = y * width + x;
                    pen[at] = sf * (pen[at] + (up[t] - lo[t]));
                }
            for (y = 0; y < height; y++) st_bgr_to_ycrcb_row(work + y * width * 3, img + y * width * 3, w);
            for (i = 0; i < n; i++) work[i * 3] = pen[i];
            for (y = 0; y < height; y++) st_ycrcb_to_bgr_row(img + y * width * 3, work + y * width * 3, w);
            if (method == ALWAN_STYLIZE_PENCIL_SKETCH_GREY)
                for (i = 0; i < n; i++) img[i * 3] = pen[i];
        }
        st_domain_free(&d);
        free(pen); free(lo); free(up); free(vt); free(dom); free(tl); free(tu);
        break;
    }
    }

    if (st == ALWAN_OK) {
        int const absolute = method == ALWAN_STYLIZE_EDGE_PRESERVING_RECURSIVE ||
                             method == ALWAN_STYLIZE_EDGE_PRESERVING_NORMCONV;
        for (y = 0; y < height; y++) {
            float const *s = img + y * width * 3;
            unsigned char *d = out + y * out_row_stride;
            unsigned char const *a = src + y * src_row_stride;
            for (x = 0; x < width; x++) {
                if (out_ch == 1) {
                    d[x] = st_to_u8(s[x * 3], 0);
                    continue;
                }
                d[x * out_ch] = st_to_u8(s[x * 3 + 2], absolute);
                d[x * out_ch + 1] = st_to_u8(s[x * 3 + 1], absolute);
                d[x * out_ch + 2] = st_to_u8(s[x * 3], absolute);
                if (out_ch == 4) d[x * 4 + 3] = a[x * 4 + 3];
            }
        }
    }
    free(img);
    free(work);
    return st;
}
