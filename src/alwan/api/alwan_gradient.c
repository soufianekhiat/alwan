/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_gradient_{T}: the image gradient by a small derivative kernel, as scikit-image's
 * filters.sobel, scharr, prewitt, farid and roberts compute it, which suite 225 holds
 * this to.
 *
 * Each derivative is a separable kernel, the derivative [1, 0, -1] (Farid's 5 taps) along
 * one axis times a smoothing along the other, applied as scipy's ndimage.convolve applies
 * it: the kernel flipped, its non-zero weights taken in raster order and summed in double,
 * the image extended by scipy's "reflect" (the edge sample repeated). The magnitude is
 * sqrt(d0^2 + d1^2) / sqrt(2), the derivative down the rows first, in the data's
 * precision. Roberts is the two 2 x 2 diagonal differences, its magnitude divided by
 * sqrt(2) in double and rounded back, as scikit-image's in-place division does.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

static double alwan_gr_r(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

static size_t alwan_gr_reflect(long i, size_t n) {
    long const p = 2 * (long)n;
    long m = i % p;
    if (m < 0) m += p;
    return (size_t)(m >= (long)n ? p - 1 - m : m);
}

/* scipy's convolve of the w x h plane img by the kh x kw kernel k into out. */
static void alwan_gr_convolve(double *out, double const *img, size_t w, size_t h, double const *k, size_t kh, size_t kw, int f32) {
    long const cy = (long)(kh / 2) - (kh % 2 == 0), cx = (long)(kw / 2) - (kw % 2 == 0);
    size_t x, y, a, b;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double t = 0.0;
            for (a = 0; a < kh; a++) {
                size_t const sy = alwan_gr_reflect((long)y + (long)a - cy, h);
                for (b = 0; b < kw; b++) {
                    double const wt = k[(kh - 1 - a) * kw + (kw - 1 - b)];   /* flipped */
                    if (fabs(wt) <= DBL_EPSILON) continue;
                    t += img[sy * w + alwan_gr_reflect((long)x + (long)b - cx, w)] * wt;
                }
            }
            out[y * w + x] = alwan_gr_r(t, f32);
        }
    }
}

static alwan_status alwan_gr_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_gradient_method method, alwan_gradient_params const *params, int f32) {
    /* Farid and Simoncelli (2004), "Differentiation of discrete multidimensional signals",
     * the 5-tap interpolator and derivative, as scikit-image carries them */
    static double const farid_smooth[5] = { 0.0376593171958126, 0.249153396177344, 0.426374573253687, 0.249153396177344,
                                            0.0376593171958126 };
    static double const farid_edge[5] = { 0.109603762960254, 0.276690988455557, 0.0, -0.276690988455557, -0.109603762960254 };
    static double const edge3[3] = { 1.0, 0.0, -1.0 };
    alwan_gradient_params const zero = { 0 };
    alwan_gradient_params const *p = params ? params : &zero;
    size_t const elem = f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    double smooth[5], edge[5], k0[25], k1[25];
    size_t taps = 3, i, j, x, y, c;
    double *buf, *img, *d0, *d1;
    if (!out || !src || w == 0 || h == 0 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_GRADIENT_ROBERTS || p->component < 0 || p->component > 2) return ALWAN_E_INVALID;
    switch (method) {
    case ALWAN_GRADIENT_SOBEL:
        smooth[0] = 0.25, smooth[1] = 0.5, smooth[2] = 0.25;
        break;
    case ALWAN_GRADIENT_SCHARR:
        smooth[0] = 0.1875, smooth[1] = 0.625, smooth[2] = 0.1875;
        break;
    case ALWAN_GRADIENT_PREWITT:
        smooth[0] = smooth[1] = smooth[2] = 1.0 / 3.0;
        break;
    case ALWAN_GRADIENT_FARID:
        taps = 5;
        memcpy(smooth, farid_smooth, sizeof(farid_smooth));
        break;
    default:
        break;
    }
    if (method == ALWAN_GRADIENT_ROBERTS) {
        /* ROBERTS_PD [[1, 0], [0, -1]] and ROBERTS_ND [[0, 1], [-1, 0]] */
        k0[0] = 1.0, k0[1] = 0.0, k0[2] = 0.0, k0[3] = -1.0;
        k1[0] = 0.0, k1[1] = 1.0, k1[2] = -1.0, k1[3] = 0.0;
        taps = 2;
    } else {
        memcpy(edge, taps == 5 ? farid_edge : edge3, taps * sizeof(double));
        for (i = 0; i < taps; i++)
            for (j = 0; j < taps; j++) {
                k0[i * taps + j] = edge[i] * smooth[j];   /* derivative down the rows (axis 0) */
                k1[i * taps + j] = edge[j] * smooth[i];   /* across the columns (axis 1) */
            }
    }
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 3 * sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    img = buf;
    d0 = img + n;
    d1 = d0 + n;
    for (c = 0; c < ch; c++) {
        for (y = 0; y < h; y++) {
            char const *row = (char const *)src + y * src_rs;
            for (x = 0; x < w; x++) {
                double const v = f32 ? (double)((alwan_f32 const *)row)[x * ch + c] : ((alwan_f64 const *)row)[x * ch + c];
                if (!(v - v == 0.0)) {
                    ALWAN_FREE(buf);
                    return ALWAN_E_INVALID;
                }
                img[y * w + x] = v;
            }
        }
        if (p->component != 2) alwan_gr_convolve(d0, img, w, h, k0, taps, taps, f32);
        if (p->component != 1) alwan_gr_convolve(d1, img, w, h, k1, taps, taps, f32);
        if (p->component == 0) {
            for (i = 0; i < n; i++) {
                double s;
                if (method == ALWAN_GRADIENT_ROBERTS) {
                    s = alwan_gr_r(alwan_gr_r(d0[i] * d0[i], f32) + alwan_gr_r(d1[i] * d1[i], f32), f32);
                    d0[i] = alwan_gr_r(alwan_gr_r(sqrt(s), f32) / sqrt(2.0), f32);
                } else {
                    s = alwan_gr_r(0.0 + alwan_gr_r(d0[i] * d0[i], f32), f32);
                    s = alwan_gr_r(s + alwan_gr_r(d1[i] * d1[i], f32), f32);
                    d0[i] = alwan_gr_r(alwan_gr_r(sqrt(s), f32) / alwan_gr_r(sqrt(2.0), f32), f32);
                }
            }
        } else if (p->component == 2) {
            memcpy(d0, d1, n * sizeof(double));
        }
        for (y = 0; y < h; y++) {
            char *row = (char *)out + y * out_rs;
            for (x = 0; x < w; x++) {
                if (f32) ((alwan_f32 *)row)[x * ch + c] = (alwan_f32)d0[y * w + x];
                else ((alwan_f64 *)row)[x * ch + c] = d0[y * w + x];
            }
        }
    }
    ALWAN_FREE(buf);
    return ALWAN_OK;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_gradient_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels,
                                size_t width, size_t height, alwan_gradient_method method, alwan_gradient_params const *params) {
    return alwan_gr_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_gradient_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels,
                                size_t width, size_t height, alwan_gradient_method method, alwan_gradient_params const *params) {
    return alwan_gr_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
