/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Exposure fusion: Mertens, Kautz and Van Reeth, "Exposure Fusion", Pacific Graphics 2007.
 * A bracket of exposures is blended straight into one picture, without building a radiance
 * map or tone mapping one: every pixel of every exposure gets a weight
 *
 *     W = C^wc * S^ws * E^we + 1e-12,
 *     C = |Laplacian of the grey image|           (contrast)
 *     S = sqrt(sum_c (I_c - mean_c I)^2)          (saturation)
 *     E = prod_c exp(-(I_c - 0.5)^2 / 0.08)       (well-exposedness, sigma 0.2)
 *
 * the weights are normalised to sum to 1 at each pixel, and the exposures' Laplacian
 * pyramids are blended with the Gaussian pyramids of their weights, which hides the seams
 * a per-pixel blend would show.
 *
 * The reference is OpenCV's cv::MergeMertens (modules/photo/src/merge.cpp, Apache-2.0),
 * and this follows it: grey = 0.299 c0 + 0.587 c1 + 0.114 c2 (the first channel red), the
 * Laplacian is the 4-neighbour kernel with BORDER_REFLECT_101, S is the root of the sum of
 * squared deviations (the paper's standard deviation without the division by the channel
 * count), the pyramid has floor(log2(min(w, h))) levels below the image, pyrDown is the
 * 5 x 5 kernel [1 4 6 4 1] / 16 with BORDER_REFLECT_101 keeping even samples, and pyrUp is
 * that kernel times 4 on the zero-stuffed grid of twice the size, BORDER_REFLECT_101 on that
 * grid, cropped to the finer level. OpenCV scales its input by 1 / 255 first; alwan takes
 * values already in 0..1. OpenCV computes in float; this computes in double.
 *
 * One deliberate difference: where every exposure's weight is the 1e-12 floor (contrast or
 * saturation exactly zero in all of them, as in a flat neutral region), the weights tie and
 * the fusion is the plain average. OpenCV's float mean makes a neutral pixel's saturation
 * about 1e-8 instead of 0, which outweighs the floor and lets rounding pick the blend; this
 * computes the saturation from pairwise channel differences, exactly 0 for a neutral pixel.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

#define ALWAN_EF_MAX_LEVELS 64

static double const alwan_ef_k[5] = { 1.0 / 16.0, 4.0 / 16.0, 6.0 / 16.0, 4.0 / 16.0, 1.0 / 16.0 };

static int alwan_ef_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

static long alwan_ef_r101(long i, long n) {
    if (n == 1) return 0;
    while (i < 0 || i >= n) {
        if (i < 0) i = -i;
        if (i >= n) i = 2 * n - 2 - i;
    }
    return i;
}

/* dst ((sw + 1) / 2 x (sh + 1) / 2) from src (sw x sh); tmp dw x sh. */
static void alwan_ef_down(double *dst, double const *src, long sw, long sh, double *tmp) {
    long const dw = (sw + 1) / 2, dh = (sh + 1) / 2;
    long x, y, j;
    for (y = 0; y < sh; y++) {
        for (x = 0; x < dw; x++) {
            double s = 0.0;
            for (j = -2; j <= 2; j++) s += alwan_ef_k[j + 2] * src[y * sw + alwan_ef_r101(2 * x + j, sw)];
            tmp[y * dw + x] = s;
        }
    }
    for (y = 0; y < dh; y++) {
        for (x = 0; x < dw; x++) {
            double s = 0.0;
            for (j = -2; j <= 2; j++) s += alwan_ef_k[j + 2] * tmp[alwan_ef_r101(2 * y + j, sh) * dw + x];
            dst[y * dw + x] = s;
        }
    }
}

/* dst (dw x dh, dw <= 2 sw, dh <= 2 sh) from src (sw x sh): the zero-stuffed 2 sw x 2 sh
 * grid filtered by 2 x the kernel along each axis, BORDER_REFLECT_101 on that grid, the
 * top-left dw x dh kept. tmp dw x sh. */
static void alwan_ef_up(double *dst, long dw, long dh, double const *src, long sw, long sh, double *tmp) {
    long const gw = 2 * sw, gh = 2 * sh;
    long x, y, j;
    for (y = 0; y < sh; y++) {
        for (x = 0; x < dw; x++) {
            double s = 0.0;
            for (j = -2; j <= 2; j++) {
                long const g = alwan_ef_r101(x + j, gw);
                if ((g & 1) == 0) s += 2.0 * alwan_ef_k[j + 2] * src[y * sw + g / 2];
            }
            tmp[y * dw + x] = s;
        }
    }
    for (y = 0; y < dh; y++) {
        for (x = 0; x < dw; x++) {
            double s = 0.0;
            for (j = -2; j <= 2; j++) {
                long const g = alwan_ef_r101(y + j, gh);
                if ((g & 1) == 0) s += 2.0 * alwan_ef_k[j + 2] * tmp[(g / 2) * dw + x];
            }
            dst[y * dw + x] = s;
        }
    }
}

static alwan_status alwan_ef_run(void *out, size_t out_row_stride, void const *const *images, size_t row_stride,
                                 size_t count, size_t ch, size_t w, size_t h, double wc, double ws, double we,
                                 int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    long lw[ALWAN_EF_MAX_LEVELS], lh[ALWAN_EF_MAX_LEVELS];
    size_t off[ALWAN_EF_MAX_LEVELS], total;
    int levels, l;
    size_t i, c, p, x, y;
    double *wts, *wsum, *res, *ipyr, *wpyr, *tmp, *up;
    if (!out || !images) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || count == 0 || (ch != 1 && ch != 3)) return ALWAN_E_INVALID;
    if (row_stride / elem / ch < w || out_row_stride / elem / ch < w) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) if (!images[i]) return ALWAN_E_INVALID;
    if (!alwan_ef_finite(wc) || !alwan_ef_finite(ws) || !alwan_ef_finite(we)) return ALWAN_E_INVALID;
    if (!(wc >= 0.0) || !(ws >= 0.0) || !(we >= 0.0) || count > 1024 || n / w != h) return ALWAN_E_RANGE;

    {
        size_t m = w < h ? w : h;
        levels = 0;
        while (m >= 2) { m >>= 1; levels++; }
    }
    lw[0] = (long)w; lh[0] = (long)h; off[0] = 0; total = n;
    for (l = 1; l <= levels; l++) {
        lw[l] = (lw[l - 1] + 1) / 2;
        lh[l] = (lh[l - 1] + 1) / 2;
        off[l] = total;
        total += (size_t)lw[l] * (size_t)lh[l];
    }

    wts = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, (count + 1) * sizeof(double)), sizeof(double));
    res = (double *)ALWAN_ALLOC(alwan_safe_array_size(total, (2 * ch + 1) * sizeof(double)), sizeof(double));
    tmp = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(double)), sizeof(double));
    if (!wts || !res || !tmp) {
        if (wts) ALWAN_FREE(wts);
        if (res) ALWAN_FREE(res);
        if (tmp) ALWAN_FREE(tmp);
        return ALWAN_E_NOMEM;
    }
    wsum = wts + count * n;
    ipyr = res + ch * total;
    wpyr = ipyr + ch * total;
    up = tmp + n;

    /* weights */
    for (p = 0; p < n; p++) wsum[p] = 0.0;
    for (i = 0; i < count; i++) {
        double *wt = wts + i * n;
        double *grey = ipyr; /* scratch: n doubles */
        for (y = 0; y < h; y++) {
            char const *row = (char const *)images[i] + y * row_stride;
            for (x = 0; x < w; x++) {
                double v[3], sat = 0.0, ex = 1.0;
                for (c = 0; c < ch; c++) {
                    v[c] = is_f32 ? (double)((alwan_f32 const *)row)[x * ch + c] : ((alwan_f64 const *)row)[x * ch + c];
                    if (!alwan_ef_finite(v[c])) goto invalid;
                    ex *= exp(-(v[c] - 0.5) * (v[c] - 0.5) / 0.08);
                }
                if (ch == 3) {
                    /* sum_c (v_c - mean)^2 as pairwise differences: exactly 0 when the
                     * channels are equal, where (v + v + v) / 3 need not give back v */
                    sat = ((v[0] - v[1]) * (v[0] - v[1]) + (v[1] - v[2]) * (v[1] - v[2]) + (v[2] - v[0]) * (v[2] - v[0])) / 3.0;
                }
                grey[y * w + x] = ch == 3 ? 0.299 * v[0] + 0.587 * v[1] + 0.114 * v[2] : v[0];
                wt[y * w + x] = ch == 3 ? pow(sqrt(sat), ws) * pow(ex, we) : pow(ex, we);
            }
        }
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                double const g = grey[y * w + x];
                double const lap = grey[alwan_ef_r101((long)y - 1, (long)h) * w + x] + grey[alwan_ef_r101((long)y + 1, (long)h) * w + x]
                                 + grey[y * w + alwan_ef_r101((long)x - 1, (long)w)] + grey[y * w + alwan_ef_r101((long)x + 1, (long)w)] - 4.0 * g;
                double const wv = pow(fabs(lap), wc) * wt[y * w + x] + 1e-12;
                wt[y * w + x] = wv;
                wsum[y * w + x] += wv;
            }
        }
    }

    /* blend the Laplacian pyramids */
    memset(res, 0, ch * total * sizeof(double));
    for (i = 0; i < count; i++) {
        double const *wt = wts + i * n;
        for (p = 0; p < n; p++) wpyr[p] = wt[p] / wsum[p];
        for (l = 1; l <= levels; l++) alwan_ef_down(wpyr + off[l], wpyr + off[l - 1], lw[l - 1], lh[l - 1], tmp);
        for (c = 0; c < ch; c++) {
            double *ip = ipyr + c * total;
            double *rp = res + c * total;
            for (y = 0; y < h; y++) {
                char const *row = (char const *)images[i] + y * row_stride;
                for (x = 0; x < w; x++) {
                    ip[y * w + x] = is_f32 ? (double)((alwan_f32 const *)row)[x * ch + c] : ((alwan_f64 const *)row)[x * ch + c];
                }
            }
            for (l = 1; l <= levels; l++) alwan_ef_down(ip + off[l], ip + off[l - 1], lw[l - 1], lh[l - 1], tmp);
            for (l = 0; l < levels; l++) {
                size_t const nl = (size_t)lw[l] * (size_t)lh[l];
                alwan_ef_up(up, lw[l], lh[l], ip + off[l + 1], lw[l + 1], lh[l + 1], tmp);
                for (p = 0; p < nl; p++) ip[off[l] + p] -= up[p];
            }
            for (p = 0; p < total; p++) rp[p] += ip[p] * wpyr[p];
        }
    }
    for (c = 0; c < ch; c++) {
        double *rp = res + c * total;
        for (l = levels; l > 0; l--) {
            size_t const nl = (size_t)lw[l - 1] * (size_t)lh[l - 1];
            alwan_ef_up(up, lw[l - 1], lh[l - 1], rp + off[l], lw[l], lh[l], tmp);
            for (p = 0; p < nl; p++) rp[off[l - 1] + p] += up[p];
        }
    }

    for (y = 0; y < h; y++) {
        char *orow = (char *)out + y * out_row_stride;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double const v = res[c * total + y * w + x];
                if (is_f32) ((alwan_f32 *)orow)[x * ch + c] = (alwan_f32)v;
                else ((alwan_f64 *)orow)[x * ch + c] = v;
            }
        }
    }
    ALWAN_FREE(wts);
    ALWAN_FREE(res);
    ALWAN_FREE(tmp);
    return ALWAN_OK;
invalid:
    ALWAN_FREE(wts);
    ALWAN_FREE(res);
    ALWAN_FREE(tmp);
    return ALWAN_E_INVALID;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_exposure_fusion_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *const *images,
                                       size_t image_row_stride, size_t image_count, size_t channels, size_t width,
                                       size_t height, alwan_f64 contrast_weight, alwan_f64 saturation_weight,
                                       alwan_f64 exposure_weight) {
    return alwan_ef_run(out, out_row_stride, (void const *const *)images, image_row_stride, image_count, channels, width,
                        height, (double)contrast_weight, (double)saturation_weight, (double)exposure_weight, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_exposure_fusion_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *const *images,
                                       size_t image_row_stride, size_t image_count, size_t channels, size_t width,
                                       size_t height, alwan_f32 contrast_weight, alwan_f32 saturation_weight,
                                       alwan_f32 exposure_weight) {
    return alwan_ef_run(out, out_row_stride, (void const *const *)images, image_row_stride, image_count, channels, width,
                        height, (double)contrast_weight, (double)saturation_weight, (double)exposure_weight, 1);
}
#endif
