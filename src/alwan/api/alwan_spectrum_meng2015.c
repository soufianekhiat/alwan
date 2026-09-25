/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_xyz_to_spectrum_meng2015_{T}: the smoothest non-negative reflectance with a given
 * XYZ (Meng, Simon, Hanika and Dachsbacher, "Physically Meaningful Rendering using
 * Tristimulus Colours", Computer Graphics Forum 34(4), 2015): R minimising
 *
 *   sum_i (R[i + 1] - R[i])^2   subject to   A R = XYZ,  R >= 0,
 *
 * A the observer's functions times the illuminant, summed over the samples and scaled so a
 * perfect reflector has Y = 1 (colour's sd_to_XYZ_integration). colour solves this with
 * SLSQP from a spectrum of ones; alwan solves it exactly as the convex quadratic programme it
 * is: the equality-constrained minimum when it is already non-negative, otherwise a feasible
 * start by non-negative least squares (Lawson and Hanson) and a primal active-set method
 * (Nocedal and Wright, algorithm 16.3) to the optimum, its KKT conditions met. The
 * non-negativity binds on saturated colours. colour's bound of 1000 on each sample never
 * does for an XYZ on the Y = 1 scale and is left out.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

/* Solve A x = b (n x n, row-major, destroyed) by LU with partial pivoting; 0 if singular. */
static int alwan_mg_solve(double *A, double *b, size_t n) {
    size_t i, j, k;
    for (k = 0; k < n; k++) {
        size_t p = k;
        double best = ALWAN_ABS_F64(A[k * n + k]);
        for (i = k + 1; i < n; i++)
            if (ALWAN_ABS_F64(A[i * n + k]) > best) best = ALWAN_ABS_F64(A[i * n + k]), p = i;
        if (!(best > 1e-300)) return 0;
        if (p != k) {
            for (j = 0; j < n; j++) {
                double const t = A[k * n + j];
                A[k * n + j] = A[p * n + j], A[p * n + j] = t;
            }
            {
                double const t = b[k];
                b[k] = b[p], b[p] = t;
            }
        }
        for (i = k + 1; i < n; i++) {
            double const l = A[i * n + k] / A[k * n + k];
            for (j = k + 1; j < n; j++) A[i * n + j] -= l * A[k * n + j];
            b[i] -= l * b[k];
        }
    }
    for (k = n; k-- > 0;) {
        double s = b[k];
        for (j = k + 1; j < n; j++) s -= A[k * n + j] * b[j];
        b[k] = s / A[k * n + k];
    }
    return 1;
}

/* The equality-constrained step on the free samples: minimise (1/2) p'Hp + g'p subject to
 * A_F p_F = 0 and p = 0 off F, H = 2 D'D; p into p, the multipliers into lam. K is scratch
 * of (n + 3)^2 + (n + 3). */
static int alwan_mg_eqp(double *p, double *lam, double const *A, double const *g, unsigned char const *fixed, size_t n, double *K) {
    size_t F[4096], nf = 0, i, j, m;
    double *rhs;
    for (i = 0; i < n; i++)
        if (!fixed[i]) F[nf++] = i;
    m = nf + 3;
    rhs = K + m * m;
    memset(K, 0, m * m * sizeof(double));
    for (i = 0; i < nf; i++) {
        size_t const a = F[i];
        for (j = 0; j < nf; j++) {
            size_t const b = F[j];
            /* H = 2 D'D: 2 (d_a) on the diagonal, -2 between neighbours */
            double h = 0.0;
            if (a == b) h = 2.0 * ((a > 0) + (a + 1 < n));
            else if (a + 1 == b || b + 1 == a) h = -2.0;
            K[i * m + j] = h;
        }
        for (j = 0; j < 3; j++) K[i * m + nf + j] = K[(nf + j) * m + i] = A[j * n + a];
        rhs[i] = -g[a];
    }
    for (j = 0; j < 3; j++) rhs[nf + j] = 0.0;
    if (!alwan_mg_solve(K, rhs, m)) return 0;
    for (i = 0; i < n; i++) p[i] = 0.0;
    for (i = 0; i < nf; i++) p[F[i]] = rhs[i];
    for (j = 0; j < 3; j++) lam[j] = rhs[nf + j];
    return 1;
}

/* Lawson and Hanson's non-negative least squares, min |A x - b| subject to x >= 0, A 3 x n. */
static void alwan_mg_nnls(double *x, double const *A, double const *b, size_t n, unsigned char *passive, double *z) {
    size_t i, it;
    memset(x, 0, n * sizeof(double));
    memset(passive, 0, n);
    for (it = 0; it < 3 * n + 30; it++) {
        double w_best = 0.0, r[3];
        size_t t = n, k;
        for (k = 0; k < 3; k++) {
            r[k] = b[k];
            for (i = 0; i < n; i++) r[k] -= A[k * n + i] * x[i];
        }
        for (i = 0; i < n; i++) {
            double w = 0.0;
            if (passive[i]) continue;
            for (k = 0; k < 3; k++) w += A[k * n + i] * r[k];
            if (w > w_best) w_best = w, t = i;
        }
        if (t == n || w_best <= 1e-15) break;
        passive[t] = 1;
        for (;;) {
            /* least squares on the passive set, by its normal equations */
            size_t P[4096], np = 0, a, c;
            double N[16 * 16], y[16], alpha = 1.0;
            int neg = 0;
            for (i = 0; i < n; i++)
                if (passive[i]) P[np++] = i;
            if (np > 16) break;   /* at most 3 columns matter for 3 rows; never reached from a sound start */
            for (a = 0; a < np; a++) {
                y[a] = 0.0;
                for (k = 0; k < 3; k++) y[a] += A[k * n + P[a]] * b[k];
                for (c = 0; c < np; c++) {
                    double s = 0.0;
                    for (k = 0; k < 3; k++) s += A[k * n + P[a]] * A[k * n + P[c]];
                    N[a * np + c] = s;
                }
            }
            if (!alwan_mg_solve(N, y, np)) {
                passive[t] = 0;
                break;
            }
            for (i = 0; i < n; i++) z[i] = 0.0;
            for (a = 0; a < np; a++) {
                z[P[a]] = y[a];
                if (y[a] <= 0.0) neg = 1;
            }
            if (!neg) {
                memcpy(x, z, n * sizeof(double));
                break;
            }
            for (a = 0; a < np; a++)
                if (z[P[a]] <= 0.0) {
                    double const q = x[P[a]] / (x[P[a]] - z[P[a]]);
                    if (q < alpha) alpha = q;
                }
            for (i = 0; i < n; i++) {
                x[i] += alpha * (z[i] - x[i]);
                if (passive[i] && x[i] <= 1e-300) passive[i] = 0, x[i] = 0.0;
            }
        }
    }
}

static alwan_status alwan_mg_solve_qp(double *R, double const *A, double const *xyz, size_t n) {
    double *K = (double *)ALWAN_ALLOC(alwan_safe_array_size((n + 3) * (n + 3) + (n + 3) + 4 * n, sizeof(double)), sizeof(double));
    unsigned char *fixed = (unsigned char *)ALWAN_ALLOC(2 * n, 1);
    double *p, *g, *z, lam[3], scale = 0.0;
    size_t i, it;
    alwan_status st = ALWAN_OK;
    if (!K || !fixed) {
        ALWAN_FREE(K);
        ALWAN_FREE(fixed);
        return ALWAN_E_NOMEM;
    }
    p = K + (n + 3) * (n + 3) + (n + 3);
    g = p + n;
    z = g + n;
    for (i = 0; i < 3; i++) scale += ALWAN_ABS_F64(xyz[i]);
    /* the equality-constrained minimum: from R = 0 with nothing fixed, one step lands on it */
    memset(fixed, 0, n);
    memset(R, 0, n * sizeof(double));
    {
        /* A R = xyz with p = R: solve the KKT system with the constraint's right side */
        size_t const m = n + 3;
        double *rhs = K + m * m;
        size_t j;
        memset(K, 0, m * m * sizeof(double));
        for (i = 0; i < n; i++) {
            K[i * m + i] = 2.0 * ((i > 0) + (i + 1 < n));
            if (i > 0) K[i * m + i - 1] = -2.0;
            if (i + 1 < n) K[i * m + i + 1] = -2.0;
            for (j = 0; j < 3; j++) K[i * m + n + j] = K[(n + j) * m + i] = A[j * n + i];
            rhs[i] = 0.0;
        }
        for (j = 0; j < 3; j++) rhs[n + j] = xyz[j];
        if (!alwan_mg_solve(K, rhs, m)) st = ALWAN_E_RANGE;
        else memcpy(R, rhs, n * sizeof(double));
    }
    if (st == ALWAN_OK) {
        int feasible = 1;
        for (i = 0; i < n; i++)
            if (R[i] < 0.0) feasible = 0;
        if (!feasible) {
            /* a feasible start: the non-negative least-squares solution, exact when the XYZ can
             * be reached at all */
            double res = 0.0;
            size_t k;
            alwan_mg_nnls(R, A, xyz, n, fixed + n, z);
            for (k = 0; k < 3; k++) {
                double s = xyz[k];
                for (i = 0; i < n; i++) s -= A[k * n + i] * R[i];
                res += ALWAN_ABS_F64(s);
            }
            if (!(res <= 1e-9 * (scale > 0.0 ? scale : 1.0))) st = ALWAN_E_RANGE;
            for (i = 0; i < n; i++) fixed[i] = (unsigned char)(R[i] <= 0.0);
            /* the primal active-set iterations */
            for (it = 0; st == ALWAN_OK && it < 20 * n + 100; it++) {
                double pmax = 0.0, rmax = 0.0;
                for (i = 0; i < n; i++) g[i] = 2.0 * (((i > 0) + (i + 1 < n)) * R[i] - (i > 0 ? R[i - 1] : 0.0) - (i + 1 < n ? R[i + 1] : 0.0));
                if (!alwan_mg_eqp(p, lam, A, g, fixed, n, K)) {
                    st = ALWAN_E_RANGE;
                    break;
                }
                for (i = 0; i < n; i++) {
                    if (ALWAN_ABS_F64(p[i]) > pmax) pmax = ALWAN_ABS_F64(p[i]);
                    if (ALWAN_ABS_F64(R[i]) > rmax) rmax = ALWAN_ABS_F64(R[i]);
                }
                if (pmax <= 1e-13 * (rmax > 1e-300 ? rmax : 1.0)) {
                    /* stationary on the working set: the multipliers of the fixed samples */
                    size_t worst = n;
                    double mu_min = 0.0;
                    for (i = 0; i < n; i++)
                        if (fixed[i]) {
                            double const mu = g[i] + A[i] * lam[0] + A[n + i] * lam[1] + A[2 * n + i] * lam[2];
                            if (mu < mu_min) mu_min = mu, worst = i;
                        }
                    if (worst == n || mu_min > -1e-12) break;   /* the KKT conditions hold */
                    fixed[worst] = 0;
                } else {
                    double alpha = 1.0;
                    size_t block = n;
                    for (i = 0; i < n; i++)
                        if (!fixed[i] && p[i] < 0.0) {
                            double const q = -R[i] / p[i];
                            if (q < alpha) alpha = q, block = i;
                        }
                    for (i = 0; i < n; i++) R[i] += alpha * p[i];
                    if (block != n) R[block] = 0.0, fixed[block] = 1;
                }
            }
            for (i = 0; i < n; i++)
                if (R[i] < 0.0) R[i] = 0.0;   /* rounding on the rails */
        }
    }
    ALWAN_FREE(K);
    ALWAN_FREE(fixed);
    return st;
}

/* The cell of a sample at fractional position t of an SPD with count samples: 1 and
 * *j = floor(t) when 0 <= t < count, else 0 (NaN and inf included), so the cast is defined. */
static int alwan_mg_cell(double t, size_t count, size_t *j) {
    if (!(t >= 0.0 && t < (double)count)) return 0;
    *j = (size_t)t;
    return 1;
}

static alwan_status alwan_mg_run(double *R, size_t *n_out, double *wl0, double *wl1, double const *xyz, alwan_meng2015_params const *params,
                                 alwan_ctx *ctx) {
    alwan_meng2015_params const zero = { 0 };
    alwan_meng2015_params const *p = params ? params : &zero;
    double const w0 = p->wavelength_min == 0.0 ? 360.0 : p->wavelength_min;
    double const w1 = p->wavelength_max == 0.0 ? 780.0 : p->wavelength_max;
    double const dw = p->interval == 0.0 ? 5.0 : p->interval;
    alwan_spd_f64 xb, yb, zb, d65;
    alwan_spd_f64 const *ill;
    double *A, ky = 0.0;
    size_t n, i, k;
    alwan_status st;
    memset(&xb, 0, sizeof(xb)), memset(&yb, 0, sizeof(yb)), memset(&zb, 0, sizeof(zb)), memset(&d65, 0, sizeof(d65));
    if (!(w0 >= 360.0) || !(w1 <= 830.0) || !(dw > 0.0) || !(w1 > w0)) return ALWAN_E_RANGE;
    n = (size_t)ALWAN_FLOOR_F64((w1 - w0) / dw + 0.5) + 1;
    if (n < 4 || n > 4096 || ALWAN_ABS_F64(w0 + (double)(n - 1) * dw - w1) > 1e-9) return ALWAN_E_RANGE;
    for (k = 0; k < 3; k++)
        if (!(xyz[k] - xyz[k] == 0.0)) return ALWAN_E_INVALID;
    st = alwan_spd_observer_f64(&xb, &yb, &zb, p->observer, ctx);
    if (st != ALWAN_OK) return st;
    ill = p->illuminant;
    if (ill && (!ill->values || ill->count < 2 || !(ill->wavelength_min - ill->wavelength_min == 0.0) ||
                !(ill->wavelength_max - ill->wavelength_max == 0.0) || !(ill->wavelength_max > ill->wavelength_min))) {
        alwan_spd_destroy_f64(&xb, ctx), alwan_spd_destroy_f64(&yb, ctx), alwan_spd_destroy_f64(&zb, ctx);
        return ALWAN_E_INVALID;
    }
    if (!ill) {
        st = alwan_spd_illuminant_f64(&d65, ALWAN_ILLUMINANT_D65, ctx);
        ill = &d65;
    }
    A = st == ALWAN_OK ? (double *)ALWAN_ALLOC(alwan_safe_array_size(3 * n, sizeof(double)), sizeof(double)) : NULL;
    if (st == ALWAN_OK && !A) st = ALWAN_E_NOMEM;
    if (st == ALWAN_OK) {
        /* the functions and the illuminant at each sample, linear between their own samples */
        alwan_spd_f64 const *f[3] = { &xb, &yb, &zb };
        for (i = 0; i < n; i++) {
            double const w = w0 + (double)i * dw;
            double s, fv[3];
            double const t = (w - ill->wavelength_min) / ((ill->wavelength_max - ill->wavelength_min) / (double)(ill->count - 1));
            size_t j = 0;
            if (!alwan_mg_cell(t, ill->count, &j)) s = 0.0;
            else if (j + 1 >= ill->count) s = ill->values[ill->count - 1];
            else s = ill->values[j] + (t - (double)j) * (ill->values[j + 1] - ill->values[j]);
            for (k = 0; k < 3; k++) {
                double const u = (w - f[k]->wavelength_min) / ((f[k]->wavelength_max - f[k]->wavelength_min) / (double)(f[k]->count - 1));
                size_t q = 0;
                if (!alwan_mg_cell(u + 1e-9, f[k]->count, &q)) fv[k] = 0.0;
                else if (q + 1 >= f[k]->count || ALWAN_ABS_F64(u - (double)q) < 1e-9) fv[k] = f[k]->values[q];
                else fv[k] = f[k]->values[q] + (u - (double)q) * (f[k]->values[q + 1] - f[k]->values[q]);
                A[k * n + i] = s * fv[k];
            }
            ky += s * fv[1];
        }
        if (!(ky > 0.0)) st = ALWAN_E_RANGE;
        else
            for (i = 0; i < 3 * n; i++) A[i] /= ky;   /* a perfect reflector has Y = 1 */
    }
    if (st == ALWAN_OK) st = alwan_mg_solve_qp(R, A, xyz, n);
    ALWAN_FREE(A);
    alwan_spd_destroy_f64(&xb, ctx);
    alwan_spd_destroy_f64(&yb, ctx);
    alwan_spd_destroy_f64(&zb, ctx);
    if (d65.values) alwan_spd_destroy_f64(&d65, ctx);
    *n_out = n, *wl0 = w0, *wl1 = w1;
    return st;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_xyz_to_spectrum_meng2015_f64(alwan_spd_f64 *out_spd, alwan_xyz_f64 const *xyz, alwan_meng2015_params const *params,
                                               alwan_ctx *ctx) {
    double R[4096], w0, w1, in[3];
    size_t n = 0, i;
    alwan_status st;
    if (!out_spd || !xyz) return ALWAN_E_INVALID;
    in[0] = (double)xyz->x, in[1] = (double)xyz->y, in[2] = (double)xyz->z;
    st = alwan_mg_run(R, &n, &w0, &w1, in, params, ctx);
    if (st != ALWAN_OK) return st;
    st = alwan_spd_create_f64(out_spd, w0, w1, n, ctx);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < n; i++) out_spd->values[i] = R[i];
    return ALWAN_OK;
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_xyz_to_spectrum_meng2015_f32(alwan_spd_f32 *out_spd, alwan_xyz_f32 const *xyz, alwan_meng2015_params const *params,
                                               alwan_ctx *ctx) {
    double R[4096], w0, w1, in[3];
    size_t n = 0, i;
    alwan_status st;
    if (!out_spd || !xyz) return ALWAN_E_INVALID;
    in[0] = (double)xyz->x, in[1] = (double)xyz->y, in[2] = (double)xyz->z;
    st = alwan_mg_run(R, &n, &w0, &w1, in, params, ctx);
    if (st != ALWAN_OK) return st;
    st = alwan_spd_create_f32(out_spd, (alwan_f32)w0, (alwan_f32)w1, n, ctx);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < n; i++) out_spd->values[i] = (alwan_f32)R[i];
    return ALWAN_OK;
}
#endif
