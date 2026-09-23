/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The domain transform: Gastal and Oliveira, "Domain Transform for Edge-Aware Image and
 * Video Processing", SIGGRAPH 2011. A 2D edge-aware filter made of 1D passes along rows and
 * columns, each in a transformed coordinate where neighbouring pixels are
 *
 *     ct(x + 1) - ct(x) = 1 + (sigma_s / sigma_r) sum_c |I_c(x + 1) - I_c(x)|
 *
 * apart, so an edge in the guide I stretches the line and a plain 1D filter along it
 * stops at the edge. Iterations k = 1..N alternate horizontal and vertical passes with
 * sigma_H,k = sigma_s sqrt3 2^(N - k) / sqrt(4^N - 1), so the N passes add up to sigma_s.
 * Cost is linear in the pixels, independent of sigma_s.
 *
 *   ALWAN_DT_NC  normalized convolution: a box of radius sqrt3 sigma_H,k in ct (Eq. 11)
 *   ALWAN_DT_RF  recursive filtering: J[n] = (1 - a^d) I[n] + a^d J[n - 1], forward then
 *                backward, a = exp(-sqrt2 / sigma_H,k), d the ct distance (Eq. 21)
 *
 * The reference is OpenCV's ximgproc::dtFilter (opencv_contrib, BSD-3-Clause), and this
 * follows it: the colour distance is the L1 sum over the guide's channels; NC's window
 * holds the pixels whose ct lies in [ct - r, ct + r), r = sqrt3 sigma_H,k, which OpenCV
 * writes as 3 sigma_s 2^(N - k) / sqrt(4^N - 1); and ct is accumulated in float, as
 * OpenCV accumulates it, because a pixel's membership in a window is decided by it. The
 * data themselves are filtered in double. OpenCV's third mode, interpolated convolution,
 * is not provided.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>

#define ALWAN_DT_NC 0
#define ALWAN_DT_RF 2

static int alwan_dt_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

/* One NC pass over `rows` lines of `cols` pixels: src rows, the lines' ct (cols + 1 floats
 * a line, the last FLT_MAX), writing each line transposed into dst (cols rows of `rows`). */
static void alwan_dt_nc_pass(double *dst, double const *src, float const *idist, size_t rows, size_t cols,
                             size_t ch, float radius, double *isum) {
    size_t i, j, c;
    for (i = 0; i < rows; i++) {
        double const *line = src + i * cols * ch;
        float const *id = idist + i * (cols + 1);
        size_t lb = 0, rb = 0;
        for (c = 0; c < ch; c++) isum[c] = 0.0;
        for (j = 0; j < cols; j++) for (c = 0; c < ch; c++) isum[(j + 1) * ch + c] = isum[j * ch + c] + line[j * ch + c];
        for (j = 0; j < cols; j++) {
            float const cur = id[j];
            while (id[lb] < cur - radius) lb++;
            while (id[rb + 1] < cur + radius) rb++;
            for (c = 0; c < ch; c++) {
                dst[(j * rows + i) * ch + c] = (isum[(rb + 1) * ch + c] - isum[lb * ch + c]) / (double)(rb + 1 - lb);
            }
        }
    }
}

alwan_status alwan__dt_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t sch,
                           void const *guide, size_t guide_row_stride, size_t gch, size_t w, size_t h,
                           double sigma_s, double sigma_r, int mode, size_t iterations, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    float const ss = (float)sigma_s, sr = (float)sigma_r;
    double *data, *tmp, *isum, *ah, *av;
    float *g, *idh, *idv;
    size_t x, y, c, k;
    if (!out || !src || !guide) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || sch == 0 || sch > 4 || gch == 0 || gch > 4) return ALWAN_E_INVALID;
    if (mode != ALWAN_DT_NC && mode != ALWAN_DT_RF) return ALWAN_E_INVALID;
    if (!alwan_dt_finite(sigma_s) || !alwan_dt_finite(sigma_r)) return ALWAN_E_INVALID;
    if (src_row_stride / elem / sch < w || guide_row_stride / elem / gch < w || out_row_stride / elem / sch < w) {
        return ALWAN_E_INVALID;
    }
    if (!(sigma_s >= 1.0) || !(sigma_r >= 0.01) || iterations == 0 || iterations > 30 || n / w != h) return ALWAN_E_RANGE;

    data = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, (2 * sch + 2) * sizeof(double)), sizeof(double));
    g = (float *)ALWAN_ALLOC(alwan_safe_array_size(n + w + h + 2, (gch + 2) * sizeof(float)), sizeof(float));
    isum = (double *)ALWAN_ALLOC(alwan_safe_array_size((w > h ? w : h) + 1, sch * sizeof(double)), sizeof(double));
    if (!data || !g || !isum) {
        if (data) ALWAN_FREE(data);
        if (g) ALWAN_FREE(g);
        if (isum) ALWAN_FREE(isum);
        return ALWAN_E_NOMEM;
    }
    tmp = data + n * sch;
    ah = tmp + n * sch;      /* RF: n doubles, (w - 1) a row */
    av = ah + n;             /* RF: n doubles, w a row pair */
    idh = g + n * gch;       /* NC: h lines of w + 1 */
    idv = idh + h * (w + 1); /* NC: w lines of h + 1 */

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            char const *srow = (char const *)src + y * src_row_stride;
            char const *grow = (char const *)guide + y * guide_row_stride;
            for (c = 0; c < sch; c++) {
                double const v = is_f32 ? (double)((alwan_f32 const *)srow)[x * sch + c] : (double)((alwan_f64 const *)srow)[x * sch + c];
                if (!alwan_dt_finite(v)) goto invalid;
                data[(y * w + x) * sch + c] = v;
            }
            for (c = 0; c < gch; c++) {
                double const v = is_f32 ? (double)((alwan_f32 const *)grow)[x * gch + c] : (double)((alwan_f64 const *)grow)[x * gch + c];
                if (!alwan_dt_finite(v)) goto invalid;
                g[(y * w + x) * gch + c] = (float)v;
            }
        }
    }

    if (mode == ALWAN_DT_NC) {
        /* ct along rows and along columns, accumulated in float */
        for (y = 0; y < h; y++) {
            float cur = 0.0f;
            idh[y * (w + 1)] = 0.0f;
            for (x = 1; x < w; x++) {
                float d = 0.0f;
                for (c = 0; c < gch; c++) d += fabsf(g[(y * w + x - 1) * gch + c] - g[(y * w + x) * gch + c]);
                cur += (float)(1.0f + ss / sr * d);
                idh[y * (w + 1) + x] = cur;
            }
            idh[y * (w + 1) + w] = FLT_MAX;
        }
        for (x = 0; x < w; x++) {
            float cur = 0.0f;
            idv[x * (h + 1)] = 0.0f;
            for (y = 1; y < h; y++) {
                float d = 0.0f;
                for (c = 0; c < gch; c++) d += fabsf(g[((y - 1) * w + x) * gch + c] - g[(y * w + x) * gch + c]);
                cur += (float)(1.0f + ss / sr * d);
                idv[x * (h + 1) + y] = cur;
            }
            idv[x * (h + 1) + h] = FLT_MAX;
        }
        for (k = 1; k <= iterations; k++) {
            double const sh = (double)ss * pow(2.0, (double)(iterations - k)) / sqrt(pow(4.0, (double)iterations) - 1.0);
            float const radius = (float)(3.0 * sh);
            alwan_dt_nc_pass(tmp, data, idh, h, w, sch, radius, isum);    /* rows, into columns-as-rows */
            alwan_dt_nc_pass(data, tmp, idv, w, h, sch, radius, isum);    /* columns, back */
        }
    } else {
        double const sh1 = (double)ss * pow(2.0, (double)(iterations - 1)) / sqrt(pow(4.0, (double)iterations) - 1.0);
        float const alpha1 = (float)exp(-sqrt(2.0 / 3.0) / sh1);
        float const lna = logf(alpha1);
        for (y = 0; y < h; y++) {
            for (x = 0; x + 1 < w; x++) {
                float d = 0.0f;
                for (c = 0; c < gch; c++) d += fabsf(g[(y * w + x) * gch + c] - g[(y * w + x + 1) * gch + c]);
                ah[y * w + x] = exp((double)(lna * (float)(1.0f + ss / sr * d)));
            }
        }
        for (y = 0; y + 1 < h; y++) {
            for (x = 0; x < w; x++) {
                float d = 0.0f;
                for (c = 0; c < gch; c++) d += fabsf(g[(y * w + x) * gch + c] - g[((y + 1) * w + x) * gch + c]);
                av[y * w + x] = exp((double)(lna * (float)(1.0f + ss / sr * d)));
            }
        }
        for (k = 1; k <= iterations; k++) {
            if (k > 1) {   /* a^d for sigma_H,k is the last iteration's squared */
                for (x = 0; x < n; x++) { ah[x] *= ah[x]; av[x] *= av[x]; }
            }
            for (y = 0; y < h; y++) {
                double *line = data + y * w * sch;
                double const *a = ah + y * w;
                for (x = 1; x < w; x++) for (c = 0; c < sch; c++) line[x * sch + c] += a[x - 1] * (line[(x - 1) * sch + c] - line[x * sch + c]);
                for (x = w - 1; x-- > 0;) for (c = 0; c < sch; c++) line[x * sch + c] += a[x] * (line[(x + 1) * sch + c] - line[x * sch + c]);
            }
            for (y = 1; y < h; y++) {
                for (x = 0; x < w; x++) {
                    double const a = av[(y - 1) * w + x];
                    for (c = 0; c < sch; c++) data[(y * w + x) * sch + c] += a * (data[((y - 1) * w + x) * sch + c] - data[(y * w + x) * sch + c]);
                }
            }
            for (y = h - 1; y-- > 0;) {
                for (x = 0; x < w; x++) {
                    double const a = av[y * w + x];
                    for (c = 0; c < sch; c++) data[(y * w + x) * sch + c] += a * (data[((y + 1) * w + x) * sch + c] - data[(y * w + x) * sch + c]);
                }
            }
        }
    }

    for (y = 0; y < h; y++) {
        char *orow = (char *)out + y * out_row_stride;
        for (x = 0; x < w * sch; x++) {
            if (is_f32) ((alwan_f32 *)orow)[x] = (alwan_f32)data[y * w * sch + x];
            else ((alwan_f64 *)orow)[x] = (alwan_f64)data[y * w * sch + x];
        }
    }
    ALWAN_FREE(data);
    ALWAN_FREE(g);
    ALWAN_FREE(isum);
    return ALWAN_OK;
invalid:
    ALWAN_FREE(data);
    ALWAN_FREE(g);
    ALWAN_FREE(isum);
    return ALWAN_E_INVALID;
}
