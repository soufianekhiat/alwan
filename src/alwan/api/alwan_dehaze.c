/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Haze removal by the dark channel prior: He, Sun and Tang, "Single Image Haze Removal
 * Using Dark Channel Prior", CVPR 2009 and IEEE TPAMI 33(12), 2011, with the transmission
 * refined by the guided filter as the same authors do in "Guided Image Filtering" (TPAMI
 * 2013, section 4.3) in place of the original soft matting.
 *
 * A hazy image is I = J t + A (1 - t): scene radiance J attenuated by the transmission t
 * and veiled by the airlight A. The prior: in most patches of a haze-free outdoor image
 * some channel is near zero, so the patch minimum of I / A measures the haze.
 *
 *   dark(x)  = min over the patch around x of min over channels of I_c
 *   A        = among the brightest `top_fraction` of dark, the pixel whose mean of the
 *              three channels is largest (Section 4.4 of the TPAMI paper)
 *   t(x)     = 1 - omega min over the patch of min over channels of I_c / A_c
 *   t        refined by the guided filter with I as the colour guide
 *   J        = (I - A) / max(t, t0) + A
 *
 * The model is linear in radiance, so I should be linear light. Patches are clipped at
 * the image border rather than padded. There is no reference implementation to hold this
 * to: suite 192 hazes a scene that satisfies the prior with a known A and t and measures
 * how much of each the method recovers, and checks that a haze-free image passes through.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>

void alwan_dehaze_params_init(alwan_dehaze_params *params) {
    if (!params) return;
    params->patch_radius = 7;           /* the paper's 15 x 15 patch */
    params->omega = 0.95;
    params->t0 = 0.1;
    params->top_fraction = 0.001;
    params->guide_radius = 30;
    params->guide_eps = 1e-3;
}

/* A separable minimum over a (2 r + 1)^2 window clipped at the border. tmp: w * h. */
static void alwan_dh_min_filter(double *dst, double const *src, size_t w, size_t h, size_t r, double *tmp) {
    size_t x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            size_t const x0 = x > r ? x - r : 0, x1 = x + r < w ? x + r : w - 1;
            double m = DBL_MAX;
            size_t k;
            for (k = x0; k <= x1; k++) if (src[y * w + k] < m) m = src[y * w + k];
            tmp[y * w + x] = m;
        }
    }
    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            size_t const y0 = y > r ? y - r : 0, y1 = y + r < h ? y + r : h - 1;
            double m = DBL_MAX;
            size_t k;
            for (k = y0; k <= y1; k++) if (tmp[k * w + x] < m) m = tmp[k * w + x];
            dst[y * w + x] = m;
        }
    }
}

typedef struct {
    double v;
    size_t i;
} alwan_dh_rank;

static int alwan_dh_cmp_desc(void const *a, void const *b) {
    alwan_dh_rank const *x = (alwan_dh_rank const *)a, *y = (alwan_dh_rank const *)b;
    if (x->v != y->v) return x->v > y->v ? -1 : 1;
    return x->i < y->i ? -1 : x->i > y->i;
}

static int alwan_dh_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

static alwan_status alwan_dh_run(void *out, size_t out_row_stride, void *t_out, size_t t_row_stride,
                                 double airlight_out[3], void const *rgb, size_t row_stride, size_t w, size_t h,
                                 alwan_dehaze_params const *p, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    double *pool, *img, *dark, *t, *tr, *tmp;
    alwan_dh_rank *rank;
    double A[3];
    size_t i, x, y, ntop, best;
    int c;
    alwan_status st;
    alwan_dehaze_params defaults;
    if (!p) {
        alwan_dehaze_params_init(&defaults);
        p = &defaults;
    }
    if (!out || !rgb) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || row_stride / elem / 3 < w || out_row_stride / elem / 3 < w) return ALWAN_E_INVALID;
    if (t_out && t_row_stride / elem < w) return ALWAN_E_INVALID;
    if (!(p->omega > 0.0 && p->omega <= 1.0) || !(p->t0 > 0.0 && p->t0 <= 1.0)) return ALWAN_E_INVALID;
    if (!(p->top_fraction > 0.0 && p->top_fraction <= 1.0) || !(p->guide_eps >= 0.0) || !alwan_dh_finite(p->guide_eps)) {
        return ALWAN_E_INVALID;
    }
    if (n / w != h) return ALWAN_E_RANGE;
    pool = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 8 * sizeof(double)), sizeof(double));
    rank = (alwan_dh_rank *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof *rank), sizeof(double));
    if (!pool || !rank) {
        if (pool) ALWAN_FREE(pool);
        if (rank) ALWAN_FREE(rank);
        return ALWAN_E_NOMEM;
    }
    img = pool; dark = img + 3 * n; t = dark + n; tr = t + n; tmp = tr + n;
    for (y = 0; y < h; y++) {
        char const *row = (char const *)rgb + y * row_stride;
        for (x = 0; x < 3 * w; x++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)row)[x] : (double)((alwan_f64 const *)row)[x];
            if (!alwan_dh_finite(v)) { st = ALWAN_E_INVALID; goto done; }
            img[y * 3 * w + x] = v;
        }
    }

    /* the dark channel, then the airlight from its brightest pixels */
    for (i = 0; i < n; i++) {
        double const *q = img + 3 * i;
        tmp[i] = q[0] < q[1] ? (q[0] < q[2] ? q[0] : q[2]) : (q[1] < q[2] ? q[1] : q[2]);
    }
    alwan_dh_min_filter(dark, tmp, w, h, p->patch_radius, t);
    for (i = 0; i < n; i++) { rank[i].v = dark[i]; rank[i].i = i; }
    qsort(rank, n, sizeof *rank, alwan_dh_cmp_desc);
    ntop = (size_t)ALWAN_CEIL_F64(p->top_fraction * (double)n);
    if (ntop < 1) ntop = 1;
    if (ntop > n) ntop = n;
    best = rank[0].i;
    for (i = 0; i < ntop; i++) {
        double const *q = img + 3 * rank[i].i, *b = img + 3 * best;
        if (q[0] + q[1] + q[2] > b[0] + b[1] + b[2]) best = rank[i].i;
    }
    for (c = 0; c < 3; c++) {
        A[c] = img[3 * best + (size_t)c];
        if (!(A[c] > 0.0)) { st = ALWAN_E_RANGE; goto done; }
    }

    /* the transmission, then its refinement by the guided filter */
    for (i = 0; i < n; i++) {
        double const *q = img + 3 * i;
        double const r = q[0] / A[0], g = q[1] / A[1], b = q[2] / A[2];
        tmp[i] = r < g ? (r < b ? r : b) : (g < b ? g : b);
    }
    alwan_dh_min_filter(t, tmp, w, h, p->patch_radius, tr);
    for (i = 0; i < n; i++) t[i] = 1.0 - p->omega * t[i];
    if (p->guide_radius > 0) {
        st = alwan__gf_run(tr, w * sizeof(double), t, w * sizeof(double), 1, img, 3 * w * sizeof(double), 3, w, h,
                           p->guide_radius, p->guide_eps, 0);
        if (st != ALWAN_OK) goto done;
    } else {
        for (i = 0; i < n; i++) tr[i] = t[i];
    }

    /* the radiance */
    for (y = 0; y < h; y++) {
        char *orow = (char *)out + y * out_row_stride;
        char *trow = t_out ? (char *)t_out + y * t_row_stride : NULL;
        for (x = 0; x < w; x++) {
            double const tt = tr[y * w + x], tc = tt > p->t0 ? tt : p->t0;
            for (c = 0; c < 3; c++) {
                double const v = (img[3 * (y * w + x) + (size_t)c] - A[c]) / tc + A[c];
                if (is_f32) ((alwan_f32 *)orow)[3 * x + (size_t)c] = (alwan_f32)v;
                else ((alwan_f64 *)orow)[3 * x + (size_t)c] = (alwan_f64)v;
            }
            if (trow) {
                if (is_f32) ((alwan_f32 *)trow)[x] = (alwan_f32)tt;
                else ((alwan_f64 *)trow)[x] = (alwan_f64)tt;
            }
        }
    }
    if (airlight_out) for (c = 0; c < 3; c++) airlight_out[c] = A[c];
    st = ALWAN_OK;
done:
    ALWAN_FREE(pool);
    ALWAN_FREE(rank);
    return st;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_dehaze_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 *transmission_out, size_t t_row_stride,
                              alwan_f64 airlight_out[3], alwan_f64 const *rgb, size_t row_stride, size_t width,
                              size_t height, alwan_dehaze_method method, alwan_dehaze_params const *params) {
    double a[3];
    alwan_status st;
    if (method != ALWAN_DEHAZE_DARK_CHANNEL) return ALWAN_E_INVALID;
    st = alwan_dh_run(out, out_row_stride, transmission_out, t_row_stride, a, rgb, row_stride, width, height, params,
                      0);
    if (st == ALWAN_OK && airlight_out) { airlight_out[0] = a[0]; airlight_out[1] = a[1]; airlight_out[2] = a[2]; }
    return st;
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_dehaze_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 *transmission_out, size_t t_row_stride,
                              alwan_f32 airlight_out[3], alwan_f32 const *rgb, size_t row_stride, size_t width,
                              size_t height, alwan_dehaze_method method, alwan_dehaze_params const *params) {
    double a[3];
    alwan_status st;
    if (method != ALWAN_DEHAZE_DARK_CHANNEL) return ALWAN_E_INVALID;
    st = alwan_dh_run(out, out_row_stride, transmission_out, t_row_stride, a, rgb, row_stride, width, height, params,
                      1);
    if (st == ALWAN_OK && airlight_out) {
        airlight_out[0] = (alwan_f32)a[0]; airlight_out[1] = (alwan_f32)a[1]; airlight_out[2] = (alwan_f32)a[2];
    }
    return st;
}
#endif
