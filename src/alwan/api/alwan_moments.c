/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Image moments, as scikit-image's measure.moments, moments_central, moments_normalized,
 * moments_hu and inertia_tensor(_eigvals), which suite 252 holds these to.
 *
 * Moments about a given centre are summed as scikit-image's einsum forms them: first down
 * each column against the rows' powers, then along the rows against the columns' powers, the
 * powers taken by pow() as numpy's power does. Summation order inside numpy's einsum is its
 * own, so these agree to rounding. Central moments about the centroid are formed from the
 * raw ones as scikit-image forms them: its closed forms up to order 3, operation for
 * operation, and the binomial expansion above. Hu's invariants are scikit-image's Cython
 * expressions in their order; the inertia tensor's eigenvalues are in closed form where
 * scikit-image calls LAPACK.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

#define ALWAN_MOMENTS_MAX_ORDER 16u

static double alwan_mom_read(void const *p, size_t rs, size_t x, size_t y, int kind) {
    char const *row = (char const *)p + y * rs;
    return kind == 0 ? ((alwan_f64 const *)row)[x] : kind == 1 ? (double)((alwan_f32 const *)row)[x] : (double)((unsigned char const *)row)[x];
}

/* sum_rc v[r][c] (r sp0 - c0)^p (c sp1 - c1)^q for every p, q <= order */
static alwan_status alwan_mom_about(double *mu, size_t order, void const *src, size_t rs, size_t w, size_t h, double c0, double c1, double sp0,
                                    double sp1, int kind) {
    size_t const o1 = order + 1;
    double *A, *pc;
    size_t x, y, pq, q;
    A = (double *)ALWAN_ALLOC(alwan_safe_array_size(o1 * w + o1 * (w > h ? w : h), sizeof(double)), 64);
    if (!A) return ALWAN_E_NOMEM;
    pc = A + o1 * w;
    memset(A, 0, o1 * w * sizeof(double));
    /* the rows' powers, then A[p][c] = sum_r v[r][c] dr^p */
    for (y = 0; y < h; y++) {
        double const d = (double)y * sp0 - c0;
        for (pq = 0; pq < o1; pq++) pc[pq] = ALWAN_POWI_F64(d, pq);
        for (x = 0; x < w; x++) {
            double const v = alwan_mom_read(src, rs, x, y, kind);
            for (pq = 0; pq < o1; pq++) A[pq * w + x] += v * pc[pq];
        }
    }
    for (pq = 0; pq < o1 * o1; pq++) mu[pq] = 0.0;
    for (x = 0; x < w; x++) {
        double const d = (double)x * sp1 - c1;
        for (q = 0; q < o1; q++) pc[q] = ALWAN_POWI_F64(d, q);
        for (pq = 0; pq < o1; pq++)
            for (q = 0; q < o1; q++) mu[pq * o1 + q] += A[pq * w + x] * pc[q];
    }
    ALWAN_FREE(A);
    return ALWAN_OK;
}

static double alwan_mom_comb(size_t n, size_t k) {
    double r = 1.0;
    size_t i;
    if (k > n - k) k = n - k;
    for (i = 1; i <= k; i++) r = r * (double)(n - k + i) / (double)i;
    return ALWAN_FLOOR_F64(r + 0.5);
}

/* scikit-image's moments_raw_to_central, 2-D */
static void alwan_mom_raw_to_central(double *mc, double const *m, size_t order) {
    size_t const o1 = order + 1;
#define M(i, j) m[(i) * o1 + (j)]
#define MC(i, j) mc[(i) * o1 + (j)]
    size_t p, q, i, j;
    for (p = 0; p < o1 * o1; p++) mc[p] = 0.0;
    if (order < 4) {
        double const cx = M(1 < o1 ? 1 : 0, 0) / M(0, 0), cy = o1 > 1 ? M(0, 1) / M(0, 0) : 0.0;
        MC(0, 0) = M(0, 0);
        if (order > 1) {
            MC(1, 1) = M(1, 1) - cx * M(0, 1);
            MC(2, 0) = M(2, 0) - cx * M(1, 0);
            MC(0, 2) = M(0, 2) - cy * M(0, 1);
        }
        if (order > 2) {
            MC(2, 1) = M(2, 1) - 2 * cx * M(1, 1) - cy * M(2, 0) + ALWAN_POWI_F64(cx, 2) * M(0, 1) + cy * cx * M(1, 0);
            MC(1, 2) = M(1, 2) - 2 * cy * M(1, 1) - cx * M(0, 2) + 2 * cy * cx * M(0, 1);
            MC(3, 0) = M(3, 0) - 3 * cx * M(2, 0) + 2 * ALWAN_POWI_F64(cx, 2) * M(1, 0);
            MC(0, 3) = M(0, 3) - 3 * cy * M(0, 2) + 2 * ALWAN_POWI_F64(cy, 2) * M(0, 1);
        }
        return;
    }
    {
        double const c0 = M(1, 0) / M(0, 0), c1 = M(0, 1) / M(0, 0);
        for (p = 0; p <= order; p++)
            for (q = 0; q <= order; q++) {
                if (p + q > order) continue;
                for (i = 0; i <= p; i++) {
                    double const term1 = alwan_mom_comb(p, i) * ALWAN_POWI_F64(-c0, (long)(p - i));
                    for (j = 0; j <= q; j++) {
                        double const term2 = alwan_mom_comb(q, j) * ALWAN_POWI_F64(-c1, (long)(q - j));
                        MC(p, q) += term1 * term2 * M(i, j);
                    }
                }
            }
    }
#undef M
#undef MC
}

static alwan_status alwan_mom_run(double *mu, size_t order, void const *src, size_t rs, size_t w, size_t h, alwan_moments_method method,
                                  alwan_moments_params const *params, int kind) {
    alwan_moments_params const zero = {0, {0.0, 0.0}, {0.0, 0.0}};
    alwan_moments_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    double const sp0 = p->spacing[0] == 0.0 ? 1.0 : p->spacing[0], sp1 = p->spacing[1] == 0.0 ? 1.0 : p->spacing[1];
    if (!mu || !src || w == 0 || h == 0 || rs / elem < w) return ALWAN_E_INVALID;
    if (order > ALWAN_MOMENTS_MAX_ORDER || (unsigned)method > (unsigned)ALWAN_MOMENTS_CENTRAL) return ALWAN_E_INVALID;
    if (!(sp0 > 0.0) || !(sp1 > 0.0)) return ALWAN_E_INVALID;
    if (method == ALWAN_MOMENTS_RAW) return alwan_mom_about(mu, order, src, rs, w, h, 0.0, 0.0, sp0, sp1, kind);
    if (p->center_given) return alwan_mom_about(mu, order, src, rs, w, h, p->center[0], p->center[1], sp0, sp1, kind);
    {
        /* the raw moments to order max(order, 1) for the centroid, then to central */
        size_t const o = order < 1 ? 1 : order, o1 = o + 1;
        double raw[(ALWAN_MOMENTS_MAX_ORDER + 1) * (ALWAN_MOMENTS_MAX_ORDER + 1)], cen[(ALWAN_MOMENTS_MAX_ORDER + 1) * (ALWAN_MOMENTS_MAX_ORDER + 1)];
        size_t i, j;
        alwan_status const st = alwan_mom_about(raw, o, src, rs, w, h, 0.0, 0.0, sp0, sp1, kind);
        if (st != ALWAN_OK) return st;
        alwan_mom_raw_to_central(cen, raw, o);
        for (i = 0; i <= order; i++)
            for (j = 0; j <= order; j++) mu[i * (order + 1) + j] = cen[i * o1 + j];
        return ALWAN_OK;
    }
}

alwan_status alwan_moments_u8(double *mu, size_t order, unsigned char const *src, size_t row_stride, size_t width, size_t height,
                              alwan_moments_method method, alwan_moments_params const *params) {
    return alwan_mom_run(mu, order, src, row_stride, width, height, method, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_moments_f64(double *mu, size_t order, alwan_f64 const *src, size_t row_stride, size_t width, size_t height,
                               alwan_moments_method method, alwan_moments_params const *params) {
    return alwan_mom_run(mu, order, src, row_stride, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_moments_f32(double *mu, size_t order, alwan_f32 const *src, size_t row_stride, size_t width, size_t height,
                               alwan_moments_method method, alwan_moments_params const *params) {
    return alwan_mom_run(mu, order, src, row_stride, width, height, method, params, 1);
}
#endif

/* mu0^(twice / 2): libm's pow in ordinary builds, bit for bit. The deterministic build's pow
 * takes a positive base only, so there the power is split into an integer power and, for an
 * odd twice, one square root: exact in sign for an even power of a negative mass, NaN for an
 * odd one, as pow gives. */
static double alwan_mom_pow_half(double x, size_t twice) {
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
    double const r = ALWAN_POWI_F64(x, (long)(twice / 2));
    return (twice & 1u) ? r * ALWAN_SQRT_F64(x) : r;
#else
    return ALWAN_POW_F64(x, (double)twice / 2.0);
#endif
}

alwan_status alwan_moments_normalized(double *nu, double const *mu, size_t order, double const *spacing) {
    size_t const o1 = order + 1;
    double scale = 1.0, mu0;
    size_t p, q;
    if (!nu || !mu || order > ALWAN_MOMENTS_MAX_ORDER) return ALWAN_E_INVALID;
    if (spacing) scale = spacing[0] < spacing[1] ? spacing[0] : spacing[1];
    mu0 = mu[0];
    for (p = 0; p < o1; p++)
        for (q = 0; q < o1; q++) {
            size_t const s = p + q;
            nu[p * o1 + q] = s < 2 ? NAN : (mu[p * o1 + q] / ALWAN_POWI_F64(scale, (long)s)) / alwan_mom_pow_half(mu0, s + 2);
        }
    return ALWAN_OK;
}

alwan_status alwan_moments_hu(double *hu, double const *nu, size_t order) {
    size_t const o1 = order + 1;
    double t0, t1, q0, q1, n4, s, d;
    if (!hu || !nu || order < 3 || order > ALWAN_MOMENTS_MAX_ORDER) return ALWAN_E_INVALID;
#define NU(i, j) nu[(i) * o1 + (j)]
    t0 = NU(3, 0) + NU(1, 2);
    t1 = NU(2, 1) + NU(0, 3);
    q0 = t0 * t0;
    q1 = t1 * t1;
    n4 = 4 * NU(1, 1);
    s = NU(2, 0) + NU(0, 2);
    d = NU(2, 0) - NU(0, 2);
    hu[0] = s;
    hu[1] = d * d + n4 * NU(1, 1);
    hu[3] = q0 + q1;
    hu[5] = d * (q0 - q1) + n4 * t0 * t1;
    t0 *= q0 - 3 * q1;
    t1 *= 3 * q0 - q1;
    q0 = NU(3, 0) - 3 * NU(1, 2);
    q1 = 3 * NU(2, 1) - NU(0, 3);
    hu[2] = q0 * q0 + q1 * q1;
    hu[4] = q0 * t0 + q1 * t1;
    hu[6] = q1 * t0 - q0 * t1;
#undef NU
    return ALWAN_OK;
}

alwan_status alwan_inertia_tensor(double *tensor, double *eigvals, double const *mu, size_t order) {
    size_t const o1 = order + 1;
    double mu0, s, t00, t11, t01;
    if (!mu || (!tensor && !eigvals) || order < 2 || order > ALWAN_MOMENTS_MAX_ORDER) return ALWAN_E_INVALID;
    mu0 = mu[0];
    s = mu[2 * o1 + 0] + mu[0 * o1 + 2];
    t00 = (s - mu[2 * o1 + 0]) / mu0;
    t11 = (s - mu[0 * o1 + 2]) / mu0;
    t01 = -mu[1 * o1 + 1] / mu0;
    if (tensor) {
        tensor[0] = t00, tensor[1] = t01;
        tensor[2] = t01, tensor[3] = t11;
    }
    if (eigvals) {
        double const half = (t00 + t11) / 2.0, root = ALWAN_HYPOT_F64((t00 - t11) / 2.0, t01);
        double l1 = half + root, l2 = half - root;
        if (l1 < 0.0) l1 = 0.0;
        if (l2 < 0.0) l2 = 0.0;
        eigvals[0] = l1;
        eigvals[1] = l2;
    }
    return ALWAN_OK;
}
