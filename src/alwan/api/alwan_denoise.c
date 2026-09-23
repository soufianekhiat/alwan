/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Total-variation denoising: Chambolle, "An Algorithm for Total Variation Minimization
 * and Applications", J. Math. Imaging and Vision 20, 2004, for the Rudin, Osher and Fatemi
 * model
 *
 *     minimise  sum (u - f)^2 / 2 + weight TV(u),   TV(u) = sum |grad u|,
 *
 * by fixed-point iterations on the dual field p (one vector a pixel):
 *
 *     u = f - div p,    g = grad u (forward differences, 0 past the last row / column),
 *     p <- (p - tau g) / (1 + tau |g| / weight),   tau = 1/4,
 *
 * stopping when the energy E = (sum |div p|^2 + weight sum |g|) / pixels changes by less
 * than eps times its first value, or after max_iterations. Flat regions go flat and edges
 * stay sharp; a larger weight removes more.
 *
 * The reference is scikit-image's denoise_tv_chambolle (restoration/_denoise.py, BSD-3),
 * and this follows it step for step, channel by channel (channel_axis set): its u is the
 * one formed at the top of the last pass, before that pass updates p, so a run that ends
 * on max_iterations returns the u of the previous p. scikit-image sums the energy with
 * numpy's pairwise summation and this with a plain loop, so a run whose stopping test is
 * within rounding of eps can stop one iteration apart.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

static int alwan_dn_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

/* One channel, w x h, in place in `u` (holding f on entry); scratch 5 w h doubles. */
static void alwan_tv_plane(double *u, size_t w, size_t h, double weight, double eps, size_t max_iter, double *scratch) {
    size_t const n = w * h;
    double *f = scratch, *py = f + n, *px = py + n, *gy = px + n, *gx = gy + n;
    double e_init = 0.0, e_prev = 0.0;
    double const tau = 0.25;
    size_t i, x, y, p;
    memcpy(f, u, n * sizeof(double));
    memset(py, 0, 4 * n * sizeof(double));
    for (i = 0; i < max_iter; i++) {
        double e = 0.0, tv = 0.0;
        if (i > 0) {
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    size_t const q = y * w + x;
                    double d = -(py[q] + px[q]);
                    if (y > 0) d += py[q - w];
                    if (x > 0) d += px[q - 1];
                    u[q] = f[q] + d;
                    e += d * d;
                }
            }
        }
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                size_t const q = y * w + x;
                double nrm;
                gy[q] = y + 1 < h ? u[q + w] - u[q] : 0.0;
                gx[q] = x + 1 < w ? u[q + 1] - u[q] : 0.0;
                nrm = sqrt(gy[q] * gy[q] + gx[q] * gx[q]);
                tv += nrm;
            }
        }
        e = (e + weight * tv) / (double)n;
        for (p = 0; p < n; p++) {
            double const nrm = sqrt(gy[p] * gy[p] + gx[p] * gx[p]) * (tau / weight) + 1.0;
            py[p] = (py[p] - tau * gy[p]) / nrm;
            px[p] = (px[p] - tau * gx[p]) / nrm;
        }
        if (i == 0) {
            e_init = e;
            e_prev = e;
        } else {
            if (fabs(e_prev - e) < eps * e_init) break;
            e_prev = e;
        }
    }
}

static alwan_status alwan_tv_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t ch,
                                 size_t w, size_t h, double weight, double eps, size_t max_iter, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    double *data, *scratch;
    size_t x, y, c;
    if (!out || !src) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_row_stride / elem / ch < w || out_row_stride / elem / ch < w) return ALWAN_E_INVALID;
    if (!alwan_dn_finite(weight) || !alwan_dn_finite(eps)) return ALWAN_E_INVALID;
    if (!(weight > 0.0) || !(eps >= 0.0) || max_iter == 0 || max_iter > 100000 || n / w != h) return ALWAN_E_RANGE;

    data = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, ch * sizeof(double)), sizeof(double));
    scratch = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 5 * sizeof(double)), sizeof(double));
    if (!data || !scratch) {
        if (data) ALWAN_FREE(data);
        if (scratch) ALWAN_FREE(scratch);
        return ALWAN_E_NOMEM;
    }
    for (y = 0; y < h; y++) {
        char const *srow = (char const *)src + y * src_row_stride;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double const v = is_f32 ? (double)((alwan_f32 const *)srow)[x * ch + c] : ((alwan_f64 const *)srow)[x * ch + c];
                if (!alwan_dn_finite(v)) {
                    ALWAN_FREE(data);
                    ALWAN_FREE(scratch);
                    return ALWAN_E_INVALID;
                }
                data[c * n + y * w + x] = v;
            }
        }
    }
    for (c = 0; c < ch; c++) alwan_tv_plane(data + c * n, w, h, weight, eps, max_iter, scratch);
    for (y = 0; y < h; y++) {
        char *orow = (char *)out + y * out_row_stride;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double const v = data[c * n + y * w + x];
                if (is_f32) ((alwan_f32 *)orow)[x * ch + c] = (alwan_f32)v;
                else ((alwan_f64 *)orow)[x * ch + c] = v;
            }
        }
    }
    ALWAN_FREE(data);
    ALWAN_FREE(scratch);
    return ALWAN_OK;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_denoise_tv_chambolle_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src,
                                            size_t src_row_stride, size_t channels, size_t width, size_t height,
                                            alwan_f64 weight, alwan_f64 eps, size_t max_iterations) {
    return alwan_tv_run(out, out_row_stride, src, src_row_stride, channels, width, height, (double)weight, (double)eps,
                        max_iterations, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_denoise_tv_chambolle_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src,
                                            size_t src_row_stride, size_t channels, size_t width, size_t height,
                                            alwan_f32 weight, alwan_f32 eps, size_t max_iterations) {
    return alwan_tv_run(out, out_row_stride, src, src_row_stride, channels, width, height, (double)weight, (double)eps,
                        max_iterations, 1);
}
#endif
