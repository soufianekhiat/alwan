/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_distance_transform_{T} and _u8: for every feature pixel, its distance to the nearest
 * background pixel. A pixel is background when every channel equals params->background; the
 * image's border is not background. As scipy.ndimage's distance transforms, which suite 236
 * holds this to.
 *
 * ALWAN_DISTANCE_EUCLIDEAN, scipy's distance_transform_edt: exact, by Felzenszwalb and
 * Huttenlocher's lower envelope of parabolas ("Distance Transforms of Sampled Functions",
 * Theory of Computing 8, 2012), one pass down the columns and one along the rows, on squared
 * distances in the pixel's own width and height (sampling). The squared distances are exact
 * sums of squares, so with unit sampling the result is the correctly rounded square root.
 *
 * ALWAN_DISTANCE_CITYBLOCK and ALWAN_DISTANCE_CHESSBOARD, scipy's distance_transform_cdt with
 * the taxicab and chessboard metrics: a forward and a backward chamfer pass over the 4 and
 * the 8 neighbours, exact for these two metrics, in pixels (sampling does not apply).
 *
 * With no background pixel the distances are +infinity. (scipy's EDT then returns values
 * measured from a point outside the image and its CDT returns -1.) With signed_distance, a
 * background pixel takes minus its distance to the nearest feature pixel: a signed distance
 * field, positive inside.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>

static int alwan_dt_is_background(void const *src, size_t src_rs, size_t ch, size_t x, size_t y, double bg, int kind) {
    char const *row = (char const *)src + y * src_rs;
    size_t c;
    for (c = 0; c < ch; c++) {
        double const v = kind == 0 ? ((alwan_f64 const *)row)[x * ch + c]
                         : kind == 1 ? (double)((alwan_f32 const *)row)[x * ch + c]
                                     : (double)((unsigned char const *)row)[x * ch + c];
        if (v != bg) return 0;
    }
    return 1;
}

/* One line of the lower envelope: d[i] = min over q of (s (i - q))^2 + f[q], f infinite where
 * there is no site; v, z scratch of n and n + 1. */
static void alwan_dt_line(double *d, double const *f, size_t n, double s, long *v, double *z) {
    long k = -1;
    size_t q, i;
    double const s2 = s * s;
    for (q = 0; q < n; q++) {
        double const fq = f[q] + s2 * (double)q * (double)q;
        if (f[q] == HUGE_VAL) continue;
        for (;;) {
            double t;
            if (k < 0) {
                k = 0;
                v[0] = (long)q;
                z[0] = -HUGE_VAL;
                z[1] = HUGE_VAL;
                break;
            }
            /* where the parabola of q overtakes the last one kept */
            t = (fq - (f[v[k]] + s2 * (double)v[k] * (double)v[k])) / (2.0 * s2 * ((double)q - (double)v[k]));
            if (t <= z[k]) {
                k--;
                continue;
            }
            k++;
            v[k] = (long)q;
            z[k] = t;
            z[k + 1] = HUGE_VAL;
            break;
        }
    }
    if (k < 0) {
        for (i = 0; i < n; i++) d[i] = HUGE_VAL;
        return;
    }
    {
        long j = 0;
        for (i = 0; i < n; i++) {
            double dq;
            while (z[j + 1] < (double)i) j++;
            dq = (double)i - (double)v[j];
            d[i] = s2 * dq * dq + f[v[j]];
        }
    }
}

/* The Euclidean distance of every pixel with site[p] == 0 to the nearest site, into out. */
static alwan_status alwan_dt_edt(double *out, size_t out_rs, unsigned char const *site, size_t w, size_t h, double sx, double sy) {
    size_t const m = w > h ? w : h;
    double *g = (double *)ALWAN_ALLOC(alwan_safe_array_size(w * h + 3 * m + 1, sizeof(double)), sizeof(double));
    long *v = (long *)ALWAN_ALLOC(alwan_safe_array_size(m, sizeof(long)), sizeof(long));
    double *f, *d, *z;
    size_t x, y;
    if (!g || !v) {
        ALWAN_FREE(g);
        ALWAN_FREE(v);
        return ALWAN_E_NOMEM;
    }
    f = g + w * h;
    d = f + m;
    z = d + m;
    /* down the columns */
    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) f[y] = site[y * w + x] ? 0.0 : HUGE_VAL;
        alwan_dt_line(d, f, h, sy, v, z);
        for (y = 0; y < h; y++) g[y * w + x] = d[y];
    }
    /* along the rows */
    for (y = 0; y < h; y++) {
        double *orow = (double *)((char *)out + y * out_rs);
        alwan_dt_line(d, g + y * w, w, sx, v, z);
        for (x = 0; x < w; x++) orow[x] = d[x] == HUGE_VAL ? HUGE_VAL : sqrt(d[x]);
    }
    ALWAN_FREE(g);
    ALWAN_FREE(v);
    return ALWAN_OK;
}

/* The city-block or chessboard distance to the nearest site, by two chamfer passes. */
static alwan_status alwan_dt_chamfer(double *out, size_t out_rs, unsigned char const *site, size_t w, size_t h, int eight) {
    double *d = (double *)ALWAN_ALLOC(alwan_safe_array_size(w * h, sizeof(double)), sizeof(double));
    size_t x, y;
    if (!d) return ALWAN_E_NOMEM;
    for (x = 0; x < w * h; x++) d[x] = site[x] ? 0.0 : HUGE_VAL;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            double *p = d + y * w + x, b = *p;
            if (x > 0 && p[-1] + 1.0 < b) b = p[-1] + 1.0;
            if (y > 0) {
                if (p[-(long)w] + 1.0 < b) b = p[-(long)w] + 1.0;
                if (eight && x > 0 && p[-(long)w - 1] + 1.0 < b) b = p[-(long)w - 1] + 1.0;
                if (eight && x + 1 < w && p[-(long)w + 1] + 1.0 < b) b = p[-(long)w + 1] + 1.0;
            }
            *p = b;
        }
    for (y = h; y-- > 0;)
        for (x = w; x-- > 0;) {
            double *p = d + y * w + x, b = *p;
            if (x + 1 < w && p[1] + 1.0 < b) b = p[1] + 1.0;
            if (y + 1 < h) {
                if (p[w] + 1.0 < b) b = p[w] + 1.0;
                if (eight && x + 1 < w && p[w + 1] + 1.0 < b) b = p[w + 1] + 1.0;
                if (eight && x > 0 && p[w - 1] + 1.0 < b) b = p[w - 1] + 1.0;
            }
            *p = b;
        }
    for (y = 0; y < h; y++) {
        double *orow = (double *)((char *)out + y * out_rs);
        for (x = 0; x < w; x++) orow[x] = d[y * w + x];
    }
    ALWAN_FREE(d);
    return ALWAN_OK;
}

static alwan_status alwan_dt_run(double *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_distance_method method, alwan_distance_params const *params, int kind /* 0 f64, 1 f32, 2 u8 */) {
    alwan_distance_params const zero = { 0 };
    alwan_distance_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const n = w * h;
    double const sx = p->sampling[0] == 0.0 ? 1.0 : p->sampling[0], sy = p->sampling[1] == 0.0 ? 1.0 : p->sampling[1];
    unsigned char *site;
    double *neg = NULL;
    size_t x, y;
    alwan_status st;
    if (!out || !src || w == 0 || h == 0 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / sizeof(double) < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_DISTANCE_CHESSBOARD) return ALWAN_E_INVALID;
    if (!(p->background == p->background)) return ALWAN_E_INVALID;
    if (!(sx > 0.0) || !(sy > 0.0) || sx - sx != 0.0 || sy - sy != 0.0) return ALWAN_E_RANGE;
    site = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size(n, 1), 1);
    if (!site) return ALWAN_E_NOMEM;
    if (kind != 2)
        for (y = 0; y < h; y++)
            for (x = 0; x < w * ch; x++) {
                double const v = kind == 0 ? ((alwan_f64 const *)((char const *)src + y * src_rs))[x]
                                           : (double)((alwan_f32 const *)((char const *)src + y * src_rs))[x];
                if (!(v - v == 0.0)) {
                    ALWAN_FREE(site);
                    return ALWAN_E_INVALID;
                }
            }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) site[y * w + x] = (unsigned char)alwan_dt_is_background(src, src_rs, ch, x, y, p->background, kind);
    /* the distance of every feature pixel to the background */
    st = method == ALWAN_DISTANCE_EUCLIDEAN ? alwan_dt_edt(out, out_rs, site, w, h, sx, sy)
                                            : alwan_dt_chamfer(out, out_rs, site, w, h, method == ALWAN_DISTANCE_CHESSBOARD);
    if (st == ALWAN_OK && p->signed_distance) {
        /* and of every background pixel to the features, negated */
        neg = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
        if (!neg) {
            st = ALWAN_E_NOMEM;
        } else {
            for (x = 0; x < n; x++) site[x] = (unsigned char)!site[x];
            st = method == ALWAN_DISTANCE_EUCLIDEAN ? alwan_dt_edt(neg, w * sizeof(double), site, w, h, sx, sy)
                                                    : alwan_dt_chamfer(neg, w * sizeof(double), site, w, h, method == ALWAN_DISTANCE_CHESSBOARD);
            if (st == ALWAN_OK)
                for (y = 0; y < h; y++) {
                    double *orow = (double *)((char *)out + y * out_rs);
                    for (x = 0; x < w; x++)
                        if (!site[y * w + x]) orow[x] = -neg[y * w + x];   /* a background pixel */
                }
            ALWAN_FREE(neg);
        }
    }
    ALWAN_FREE(site);
    return st;
}

alwan_status alwan_distance_transform_u8(double *out, size_t out_row_stride, unsigned char const *src, size_t src_row_stride,
                                         size_t channels, size_t width, size_t height, alwan_distance_method method,
                                         alwan_distance_params const *params) {
    return alwan_dt_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_distance_transform_f64(double *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                                          size_t channels, size_t width, size_t height, alwan_distance_method method,
                                          alwan_distance_params const *params) {
    return alwan_dt_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_distance_transform_f32(double *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                                          size_t channels, size_t width, size_t height, alwan_distance_method method,
                                          alwan_distance_params const *params) {
    return alwan_dt_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
