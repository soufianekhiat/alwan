/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The fast global smoother: Min, Choi, Lu, Ham, Sohn and Do, "Fast Global Image
 * Smoothing Based on Weighted Least Squares", IEEE TIP 2014. The weighted least squares
 * smoother
 *
 *     minimise  sum_p (u_p - f_p)^2 + lambda sum_p sum_{q in N(p)} w_pq (u_p - u_q)^2,
 *     w_pq = exp(-|g_p - g_q| / sigma_color),
 *
 * with the 2D problem split into 1D problems along every row and then every column. A 1D
 * problem is tridiagonal,
 *
 *     (1 + lambda (w_l + w_r)) u_x - lambda w_l u_{x-1} - lambda w_r u_{x+1} = f_x,
 *
 * and is solved exactly by the Thomas algorithm in linear time. Iterations repeat the row
 * and column passes on the last result, lambda multiplied by lambda_attenuation after
 * each, which removes the streaks a single pair leaves.
 *
 * The reference is OpenCV's ximgproc::fastGlobalSmootherFilter (opencv_contrib,
 * BSD-3-Clause), and this follows it: |g_p - g_q| is the Euclidean distance over the
 * guide's channels, the edge weight between x and x + 1 is stored at x (0 past the last
 * pixel, so the ends see one neighbour), lambda is attenuated geometrically rather than on
 * the paper's 4^(T - t) schedule, and each channel is filtered alone. OpenCV takes an
 * 8-bit guide and sigma_color in its 0..255 units and solves in float; alwan takes the
 * guide in any units, with sigma_color in the same units, and solves in double.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>

static int alwan_fgs_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

/* One tridiagonal solve along `n` samples `step` doubles apart: wt[i * wstep] is the
 * weight between samples i and i + 1 (0 at the end), scratch holds n doubles. */
static void alwan_fgs_solve(double *x, size_t step, double const *wt, size_t wstep, size_t n, double lambda,
                            double *scratch) {
    double cprev = -lambda * wt[0];
    size_t i;
    scratch[0] = cprev / (1.0 - cprev);
    x[0] = x[0] / (1.0 - cprev);
    for (i = 1; i < n; i++) {
        double const ccur = -lambda * wt[i * wstep];
        double const denom = (1.0 - cprev - ccur) - scratch[i - 1] * cprev;
        scratch[i] = ccur / denom;
        x[i * step] = (x[i * step] - x[(i - 1) * step] * cprev) / denom;
        cprev = ccur;
    }
    for (i = n - 1; i-- > 0;) x[i * step] -= scratch[i] * x[(i + 1) * step];
}

alwan_status alwan__fgs_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t sch,
                            void const *guide, size_t guide_row_stride, size_t gch, size_t w, size_t h,
                            double lambda, double sigma_color, double attenuation, size_t iterations, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    double *data, *wh, *wv, *scratch;
    size_t x, y, c, k;
    if (!out || !src || !guide) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || sch == 0 || sch > 4 || gch == 0 || gch > 4) return ALWAN_E_INVALID;
    if (!alwan_fgs_finite(lambda) || !alwan_fgs_finite(sigma_color) || !alwan_fgs_finite(attenuation)) return ALWAN_E_INVALID;
    if (src_row_stride / elem / sch < w || guide_row_stride / elem / gch < w || out_row_stride / elem / sch < w) {
        return ALWAN_E_INVALID;
    }
    if (!(lambda >= 0.0) || !(sigma_color > 0.0) || !(attenuation >= 0.0) || iterations == 0 || iterations > 100 || n / w != h) {
        return ALWAN_E_RANGE;
    }

    data = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, (sch + 2) * sizeof(double)), sizeof(double));
    scratch = (double *)ALWAN_ALLOC(alwan_safe_array_size(w > h ? w : h, sizeof(double)), sizeof(double));
    if (!data || !scratch) {
        if (data) ALWAN_FREE(data);
        if (scratch) ALWAN_FREE(scratch);
        return ALWAN_E_NOMEM;
    }
    wh = data + n * sch; /* wh[y w + x]: between (x, y) and (x + 1, y) */
    wv = wh + n;         /* wv[y w + x]: between (x, y) and (x, y + 1) */

    for (y = 0; y < h; y++) {
        char const *srow = (char const *)src + y * src_row_stride;
        for (x = 0; x < w * sch; x++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)srow)[x] : (double)((alwan_f64 const *)srow)[x];
            if (!alwan_fgs_finite(v)) goto invalid;
            data[(x % sch) * n + y * w + x / sch] = v; /* planar: one channel after another */
        }
    }
    for (y = 0; y < h; y++) {
        char const *g0 = (char const *)guide + y * guide_row_stride;
        char const *g1 = (char const *)guide + (y + 1 < h ? y + 1 : y) * guide_row_stride;
        for (x = 0; x < w; x++) {
            double dh = 0.0, dv = 0.0;
            for (c = 0; c < gch; c++) {
                double const a = is_f32 ? (double)((alwan_f32 const *)g0)[x * gch + c] : ((alwan_f64 const *)g0)[x * gch + c];
                double const r = x + 1 < w ? (is_f32 ? (double)((alwan_f32 const *)g0)[(x + 1) * gch + c] : ((alwan_f64 const *)g0)[(x + 1) * gch + c]) : a;
                double const b = is_f32 ? (double)((alwan_f32 const *)g1)[x * gch + c] : ((alwan_f64 const *)g1)[x * gch + c];
                if (!alwan_fgs_finite(a)) goto invalid;
                dh += (a - r) * (a - r);
                dv += (a - b) * (a - b);
            }
            wh[y * w + x] = x + 1 < w ? exp(-sqrt(dh) / sigma_color) : 0.0;
            wv[y * w + x] = y + 1 < h ? exp(-sqrt(dv) / sigma_color) : 0.0;
        }
    }

    for (c = 0; c < sch; c++) {
        double *plane = data + c * n;
        double lam = lambda;
        for (k = 0; k < iterations; k++) {
            for (y = 0; y < h; y++) alwan_fgs_solve(plane + y * w, 1, wh + y * w, 1, w, lam, scratch);
            for (x = 0; x < w; x++) alwan_fgs_solve(plane + x, w, wv + x, w, h, lam, scratch);
            lam *= attenuation;
        }
    }

    for (y = 0; y < h; y++) {
        char *orow = (char *)out + y * out_row_stride;
        for (x = 0; x < w * sch; x++) {
            double const v = data[(x % sch) * n + y * w + x / sch];
            if (is_f32) ((alwan_f32 *)orow)[x] = (alwan_f32)v;
            else ((alwan_f64 *)orow)[x] = (alwan_f64)v;
        }
    }
    ALWAN_FREE(data);
    ALWAN_FREE(scratch);
    return ALWAN_OK;
invalid:
    ALWAN_FREE(data);
    ALWAN_FREE(scratch);
    return ALWAN_E_INVALID;
}
