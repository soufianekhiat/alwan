/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * 3x3 Matrix Operations
 * Matrix layout (row-major): [m00 m01 m02 m10 m11 m12 m20 m21 m22]
 * Index: m->m[row*3 + col]
 * ---------------------------------------------------------------- */

void alwan_mat3_identity(alwan_mat3x3 *out) {
    memset(out->m, 0, sizeof(out->m));
    out->m[0] = out->m[4] = out->m[8] = ALWAN_LITERAL(1.0);
}

void alwan_mat3_mul(alwan_mat3x3 const *a, alwan_mat3x3 const *b, alwan_mat3x3 *out) {
    alwan_mat3x3 tmp;

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            alwan_scalar sum = 0;
            for (int k = 0; k < 3; k++) {
                sum += a->m[row * 3 + k] * b->m[k * 3 + col];
            }
            tmp.m[row * 3 + col] = sum;
        }
    }

    memcpy(out, &tmp, sizeof(alwan_mat3x3));
}

void alwan_mat3_mulv(alwan_mat3x3 const *m, alwan_vec3 const *v, alwan_vec3 *out) {
    alwan_vec3 tmp;

    for (int row = 0; row < 3; row++) {
        tmp.v[row] = m->m[row * 3 + 0] * v->v[0] +
                     m->m[row * 3 + 1] * v->v[1] +
                     m->m[row * 3 + 2] * v->v[2];
    }

    memcpy(out, &tmp, sizeof(alwan_vec3));
}

/* ----------------------------------------------------------------
 * 3x3 Matrix Inversion using Partial-Pivot Gaussian Elimination
 * ---------------------------------------------------------------- */

int alwan_mat3_inv(alwan_mat3x3 const *m, alwan_mat3x3 *out) {
    /* Create augmented matrix [M | I] */
    alwan_scalar aug[3][6];

    /* Initialize with input matrix on left, identity on right */
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            aug[row][col] = m->m[row * 3 + col];
        }
        aug[row][3] = (row == 0) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(0.0);
        aug[row][4] = (row == 1) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(0.0);
        aug[row][5] = (row == 2) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(0.0);
    }

    /* Forward elimination with partial pivoting */
    for (int col = 0; col < 3; col++) {
        /* Find pivot row */
        int pivot_row = col;
        alwan_scalar max_val = ALWAN_FABS(aug[col][col]);

        for (int row = col + 1; row < 3; row++) {
            alwan_scalar val = ALWAN_FABS(aug[row][col]);
            if (val > max_val) {
                max_val = val;
                pivot_row = row;
            }
        }

        /* Check for singularity */
        if (max_val < ALWAN_EPSILON) {
            return ALWAN_E_RANGE;  /* Matrix is singular or near-singular */
        }

        /* Swap rows if needed */
        if (pivot_row != col) {
            for (int k = 0; k < 6; k++) {
                alwan_scalar tmp = aug[col][k];
                aug[col][k] = aug[pivot_row][k];
                aug[pivot_row][k] = tmp;
            }
        }

        /* Scale pivot row */
        alwan_scalar pivot = aug[col][col];
        for (int k = 0; k < 6; k++) {
            aug[col][k] /= pivot;
        }

        /* Eliminate column in rows below */
        for (int row = col + 1; row < 3; row++) {
            alwan_scalar factor = aug[row][col];
            for (int k = 0; k < 6; k++) {
                aug[row][k] -= factor * aug[col][k];
            }
        }
    }

    /* Back substitution */
    for (int col = 2; col >= 0; col--) {
        for (int row = col - 1; row >= 0; row--) {
            alwan_scalar factor = aug[row][col];
            for (int k = 0; k < 6; k++) {
                aug[row][k] -= factor * aug[col][k];
            }
        }
    }

    /* Extract inverse from right side of augmented matrix */
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            out->m[row * 3 + col] = aug[row][col + 3];
        }
    }

    return ALWAN_OK;
}

/* ================================================================
 * Advanced Mathematical & Utility Functions
 * ================================================================ */

/* Find interval index for x in sorted array x_in */
static size_t find_interval(alwan_scalar const *x_in, size_t count, alwan_scalar x) {
    if (x <= x_in[0]) return 0;
    if (x >= x_in[count - 1]) return count - 2;

    /* Binary search */
    size_t left = 0, right = count - 1;
    while (right - left > 1) {
        size_t mid = (left + right) / 2;
        if (x_in[mid] <= x) {
            left = mid;
        } else {
            right = mid;
        }
    }
    return left;
}

/* Lanczos windowed sinc kernel */
static alwan_scalar lanczos_kernel(alwan_scalar x, int a) {
    if (x == 0.0) return 1.0;
    if (ALWAN_FABS(x) >= a) return 0.0;

    alwan_scalar pi_x = ALWAN_PI * x;
    alwan_scalar pi_x_a = pi_x / a;
    return (ALWAN_SIN(pi_x) / pi_x) * (ALWAN_SIN(pi_x_a) / pi_x_a);
}

/* ----------------------------------------------------------------
 * Advanced Interpolation Methods
 * ---------------------------------------------------------------- */

int alwan_interpolate(alwan_scalar const *x_in, alwan_scalar const *y_in, size_t count_in,
                       alwan_scalar const *x_out, alwan_scalar *y_out, size_t count_out,
                       alwan_interp_method method) {
    if (!x_in || !y_in || !x_out || !y_out || count_in < 2 || count_out == 0) {
        return ALWAN_E_RANGE;
    }

    for (size_t i = 0; i < count_out; i++) {
        alwan_scalar x = x_out[i];

        /* Handle boundary cases */
        if (x <= x_in[0]) {
            y_out[i] = y_in[0];
            continue;
        }
        if (x >= x_in[count_in - 1]) {
            y_out[i] = y_in[count_in - 1];
            continue;
        }

        size_t idx = find_interval(x_in, count_in, x);
        alwan_scalar x0 = x_in[idx];
        alwan_scalar x1 = x_in[idx + 1];
        alwan_scalar t = (x - x0) / (x1 - x0);

        switch (method) {
            case ALWAN_INTERP_LINEAR: {
                /* Linear interpolation */
                y_out[i] = y_in[idx] * (1.0 - t) + y_in[idx + 1] * t;
                break;
            }

            case ALWAN_INTERP_CUBIC: {
                /* Catmull-Rom cubic spline */
                alwan_scalar y0 = (idx > 0) ? y_in[idx - 1] : y_in[idx];
                alwan_scalar y1 = y_in[idx];
                alwan_scalar y2 = y_in[idx + 1];
                alwan_scalar y3 = (idx + 2 < count_in) ? y_in[idx + 2] : y_in[idx + 1];

                alwan_scalar t2 = t * t;
                alwan_scalar t3 = t2 * t;

                y_out[i] = ALWAN_LITERAL(0.5) * (
                    (ALWAN_LITERAL(2.0) * y1) +
                    (-y0 + y2) * t +
                    (ALWAN_LITERAL(2.0) * y0 - ALWAN_LITERAL(5.0) * y1 + ALWAN_LITERAL(4.0) * y2 - y3) * t2 +
                    (-y0 + ALWAN_LITERAL(3.0) * y1 - ALWAN_LITERAL(3.0) * y2 + y3) * t3
                );
                break;
            }

            case ALWAN_INTERP_LANCZOS: {
                /* Lanczos interpolation (a=3) */
                const int a = 3;
                alwan_scalar sum = 0.0;
                alwan_scalar weight_sum = 0.0;

                for (int j = -a + 1; j <= a; j++) {
                    int k = (int)idx + j;
                    if (k < 0 || k >= (int)count_in) continue;

                    alwan_scalar dx = x - x_in[k];
                    alwan_scalar normalized_dx = dx / (x1 - x0);
                    alwan_scalar weight = lanczos_kernel(normalized_dx, a);

                    sum += y_in[k] * weight;
                    weight_sum += weight;
                }

                y_out[i] = (weight_sum > 0.0) ? (sum / weight_sum) : y_in[idx];
                break;
            }

            case ALWAN_INTERP_SPRAGUE: {
                /* Sprague 5th order interpolation (for smooth spectra) */
                if (count_in < 6) {
                    /* Fall back to cubic */
                    alwan_scalar y0 = (idx > 0) ? y_in[idx - 1] : y_in[idx];
                    alwan_scalar y1 = y_in[idx];
                    alwan_scalar y2 = y_in[idx + 1];
                    alwan_scalar y3 = (idx + 2 < count_in) ? y_in[idx + 2] : y_in[idx + 1];

                    alwan_scalar t2 = t * t;
                    alwan_scalar t3 = t2 * t;

                    y_out[i] = ALWAN_LITERAL(0.5) * (
                        (ALWAN_LITERAL(2.0) * y1) +
                        (-y0 + y2) * t +
                        (ALWAN_LITERAL(2.0) * y0 - ALWAN_LITERAL(5.0) * y1 + ALWAN_LITERAL(4.0) * y2 - y3) * t2 +
                        (-y0 + ALWAN_LITERAL(3.0) * y1 - ALWAN_LITERAL(3.0) * y2 + y3) * t3
                    );
                } else {
                    /* Sprague coefficients for 6 points */
                    alwan_scalar y[6];
                    for (int j = 0; j < 6; j++) {
                        int k = (int)idx + j - 2;
                        if (k < 0) k = 0;
                        if (k >= (int)count_in) k = (int)count_in - 1;
                        y[j] = y_in[k];
                    }

                    /* Sprague formula */
                    alwan_scalar t2 = t * t;
                    alwan_scalar t3 = t2 * t;
                    alwan_scalar t4 = t3 * t;
                    alwan_scalar t5 = t4 * t;

                    y_out[i] =
                        y[2] +
                        ALWAN_LITERAL(0.5) * t * (y[3] - y[1]) +
                        ALWAN_LITERAL(0.5) * t2 * (y[3] - ALWAN_LITERAL(2.0) * y[2] + y[1]) +
                        (ALWAN_LITERAL(1.0)/ALWAN_LITERAL(6.0)) * t3 * (y[4] - ALWAN_LITERAL(3.0) * y[3] + ALWAN_LITERAL(3.0) * y[1] - y[0]) +
                        (ALWAN_LITERAL(1.0)/ALWAN_LITERAL(24.0)) * t4 * (y[4] - ALWAN_LITERAL(4.0) * y[3] + ALWAN_LITERAL(6.0) * y[2] - ALWAN_LITERAL(4.0) * y[1] + y[0]) +
                        (ALWAN_LITERAL(1.0)/ALWAN_LITERAL(120.0)) * t5 * (y[5] - ALWAN_LITERAL(5.0) * y[4] + ALWAN_LITERAL(10.0) * y[3] - ALWAN_LITERAL(10.0) * y[2] + ALWAN_LITERAL(5.0) * y[1] - y[0]);
                }
                break;
            }

            case ALWAN_INTERP_LAGRANGE: {
                /* Lagrange polynomial interpolation (3rd order) */
                alwan_scalar y[4];
                alwan_scalar x_vals[4];

                int start = (int)idx - 1;
                if (start < 0) start = 0;
                if (start + 4 > (int)count_in) start = (int)count_in - 4;

                for (int j = 0; j < 4; j++) {
                    y[j] = y_in[start + j];
                    x_vals[j] = x_in[start + j];
                }

                alwan_scalar result = 0.0;
                for (int j = 0; j < 4; j++) {
                    alwan_scalar term = y[j];
                    for (int k = 0; k < 4; k++) {
                        if (j != k) {
                            term *= (x - x_vals[k]) / (x_vals[j] - x_vals[k]);
                        }
                    }
                    result += term;
                }

                y_out[i] = result;
                break;
            }

            case ALWAN_INTERP_AKIMA: {
                /* Akima spline (non-overshooting) */
                if (count_in < 4) {
                    /* Fall back to linear */
                    y_out[i] = y_in[idx] * (1.0 - t) + y_in[idx + 1] * t;
                } else {
                    /* Calculate slopes */
                    alwan_scalar m[5];
                    for (int j = -2; j <= 2; j++) {
                        int k = (int)idx + j;
                        if (k < 0) k = 0;
                        if (k >= (int)count_in - 1) k = (int)count_in - 2;
                        m[j + 2] = (y_in[k + 1] - y_in[k]) / (x_in[k + 1] - x_in[k]);
                    }

                    /* Akima weights */
                    alwan_scalar w1 = ALWAN_FABS(m[3] - m[2]);
                    alwan_scalar w2 = ALWAN_FABS(m[1] - m[0]);
                    alwan_scalar slope = (w1 + w2 > 0.0) ?
                        ((w1 * m[1] + w2 * m[3]) / (w1 + w2)) :
                        (ALWAN_LITERAL(0.5) * (m[1] + m[3]));

                    /* Hermite interpolation */
                    alwan_scalar y1 = y_in[idx];
                    alwan_scalar y2 = y_in[idx + 1];
                    alwan_scalar dx = x1 - x0;

                    alwan_scalar h00 = (1.0 + ALWAN_LITERAL(2.0) * t) * (1.0 - t) * (1.0 - t);
                    alwan_scalar h10 = t * (1.0 - t) * (1.0 - t);
                    alwan_scalar h01 = t * t * (ALWAN_LITERAL(3.0) - ALWAN_LITERAL(2.0) * t);
                    alwan_scalar h11 = t * t * (t - 1.0);

                    y_out[i] = h00 * y1 + h10 * dx * slope + h01 * y2 + h11 * dx * slope;
                }
                break;
            }

            default:
                return ALWAN_E_RANGE;
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Enhanced Extrapolation Methods
 * ---------------------------------------------------------------- */

int alwan_extrapolate(alwan_scalar const *x_in, alwan_scalar const *y_in, size_t count_in,
                       alwan_scalar const *x_out, alwan_scalar *y_out, size_t count_out,
                       alwan_extrap_method method) {
    if (!x_in || !y_in || !x_out || !y_out || count_in < 2 || count_out == 0) {
        return ALWAN_E_RANGE;
    }

    for (size_t i = 0; i < count_out; i++) {
        alwan_scalar x = x_out[i];
        int extrapolating = 0;
        int use_left = 0;

        if (x < x_in[0]) {
            extrapolating = 1;
            use_left = 1;
        } else if (x > x_in[count_in - 1]) {
            extrapolating = 1;
            use_left = 0;
        }

        if (!extrapolating) {
            /* Within bounds, use linear interpolation */
            size_t idx = find_interval(x_in, count_in, x);
            alwan_scalar t = (x - x_in[idx]) / (x_in[idx + 1] - x_in[idx]);
            y_out[i] = y_in[idx] * (1.0 - t) + y_in[idx + 1] * t;
            continue;
        }

        switch (method) {
            case ALWAN_EXTRAP_CONSTANT: {
                /* Use boundary value */
                y_out[i] = use_left ? y_in[0] : y_in[count_in - 1];
                break;
            }

            case ALWAN_EXTRAP_LINEAR: {
                /* Linear extrapolation */
                if (use_left) {
                    alwan_scalar slope = (y_in[1] - y_in[0]) / (x_in[1] - x_in[0]);
                    y_out[i] = y_in[0] + slope * (x - x_in[0]);
                } else {
                    alwan_scalar slope = (y_in[count_in - 1] - y_in[count_in - 2]) /
                                        (x_in[count_in - 1] - x_in[count_in - 2]);
                    y_out[i] = y_in[count_in - 1] + slope * (x - x_in[count_in - 1]);
                }
                break;
            }

            case ALWAN_EXTRAP_POLYNOMIAL: {
                /* Polynomial extrapolation (3-point) */
                if (count_in < 3) {
                    /* Fall back to linear */
                    if (use_left) {
                        alwan_scalar slope = (y_in[1] - y_in[0]) / (x_in[1] - x_in[0]);
                        y_out[i] = y_in[0] + slope * (x - x_in[0]);
                    } else {
                        alwan_scalar slope = (y_in[count_in - 1] - y_in[count_in - 2]) /
                                            (x_in[count_in - 1] - x_in[count_in - 2]);
                        y_out[i] = y_in[count_in - 1] + slope * (x - x_in[count_in - 1]);
                    }
                } else {
                    /* Use 3 boundary points for quadratic fit */
                    alwan_scalar x0, x1, x2, y0, y1, y2;
                    if (use_left) {
                        x0 = x_in[0]; y0 = y_in[0];
                        x1 = x_in[1]; y1 = y_in[1];
                        x2 = x_in[2]; y2 = y_in[2];
                    } else {
                        x0 = x_in[count_in - 3]; y0 = y_in[count_in - 3];
                        x1 = x_in[count_in - 2]; y1 = y_in[count_in - 2];
                        x2 = x_in[count_in - 1]; y2 = y_in[count_in - 1];
                    }

                    /* Lagrange interpolation formula */
                    y_out[i] = y0 * ((x - x1) * (x - x2)) / ((x0 - x1) * (x0 - x2)) +
                               y1 * ((x - x0) * (x - x2)) / ((x1 - x0) * (x1 - x2)) +
                               y2 * ((x - x0) * (x - x1)) / ((x2 - x0) * (x2 - x1));
                }
                break;
            }

            case ALWAN_EXTRAP_EXPONENTIAL: {
                /* Exponential decay (for SPDs) */
                if (use_left) {
                    if (y_in[0] > 0.0 && y_in[1] > 0.0) {
                        alwan_scalar ratio = y_in[1] / y_in[0];
                        alwan_scalar lambda = ALWAN_LOG(ratio) / (x_in[1] - x_in[0]);
                        y_out[i] = y_in[0] * ALWAN_EXP(lambda * (x - x_in[0]));
                    } else {
                        y_out[i] = y_in[0];
                    }
                } else {
                    if (y_in[count_in - 1] > 0.0 && y_in[count_in - 2] > 0.0) {
                        alwan_scalar ratio = y_in[count_in - 1] / y_in[count_in - 2];
                        alwan_scalar lambda = ALWAN_LOG(ratio) / (x_in[count_in - 1] - x_in[count_in - 2]);
                        y_out[i] = y_in[count_in - 1] * ALWAN_EXP(lambda * (x - x_in[count_in - 1]));
                    } else {
                        y_out[i] = y_in[count_in - 1];
                    }
                }

                /* Clamp to non-negative */
                if (y_out[i] < 0.0) y_out[i] = 0.0;
                break;
            }

            case ALWAN_EXTRAP_REFLECT: {
                /* Reflective boundary */
                alwan_scalar x_reflected;

                if (use_left) {
                    alwan_scalar dist = x_in[0] - x;
                    x_reflected = x_in[0] + dist;
                } else {
                    alwan_scalar dist = x - x_in[count_in - 1];
                    x_reflected = x_in[count_in - 1] - dist;
                }

                /* Clamp to valid range */
                if (x_reflected < x_in[0]) x_reflected = x_in[0];
                if (x_reflected > x_in[count_in - 1]) x_reflected = x_in[count_in - 1];

                /* Interpolate at reflected position */
                size_t idx = find_interval(x_in, count_in, x_reflected);
                alwan_scalar t = (x_reflected - x_in[idx]) / (x_in[idx + 1] - x_in[idx]);
                y_out[i] = y_in[idx] * (1.0 - t) + y_in[idx + 1] * t;
                break;
            }

            case ALWAN_EXTRAP_NATURAL: {
                /* Natural neighbor extrapolation (nearest boundary value) */
                y_out[i] = use_left ? y_in[0] : y_in[count_in - 1];
                break;
            }

            default:
                return ALWAN_E_RANGE;
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CCT and Duv Optimization
 * ---------------------------------------------------------------- */

/* Planckian locus (x, y) from CCT using Hernández-Andrés 1999 approximation */
static void cct_to_xy_planckian(alwan_scalar cct, alwan_scalar *x_out, alwan_scalar *y_out) {
    alwan_scalar T = cct;
    alwan_scalar T2 = T * T;
    alwan_scalar T3 = T2 * T;

    alwan_scalar x;
    if (T <= ALWAN_LITERAL(4000.0)) {
        x = ALWAN_LITERAL(-0.2661239e9) / T3 - ALWAN_LITERAL(0.2343589e6) / T2 + ALWAN_LITERAL(0.8776956e3) / T + ALWAN_LITERAL(0.179910);
    } else {
        x = ALWAN_LITERAL(-3.0258469e9) / T3 + ALWAN_LITERAL(2.1070379e6) / T2 + ALWAN_LITERAL(0.2226347e3) / T + ALWAN_LITERAL(0.240390);
    }

    alwan_scalar x2 = x * x;
    alwan_scalar x3 = x2 * x;

    alwan_scalar y;
    if (T <= ALWAN_LITERAL(2222.0)) {
        y = ALWAN_LITERAL(-1.1063814) * x3 - ALWAN_LITERAL(1.34811020) * x2 + ALWAN_LITERAL(2.18555832) * x - ALWAN_LITERAL(0.20219683);
    } else if (T <= ALWAN_LITERAL(4000.0)) {
        y = ALWAN_LITERAL(-0.9549476) * x3 - ALWAN_LITERAL(1.37418593) * x2 + ALWAN_LITERAL(2.09137015) * x - ALWAN_LITERAL(0.16748867);
    } else {
        y = ALWAN_LITERAL(3.0817580) * x3 - ALWAN_LITERAL(5.87338670) * x2 + ALWAN_LITERAL(3.75112997) * x - ALWAN_LITERAL(0.37001483);
    }

    *x_out = x;
    *y_out = y;
}

/* Distance from point to Planckian locus (Duv) */
static alwan_scalar compute_duv(alwan_scalar x, alwan_scalar y, alwan_scalar cct) {
    alwan_scalar x_p, y_p;
    cct_to_xy_planckian(cct, &x_p, &y_p);

    /* Convert to uv for perceptually uniform distance */
    alwan_scalar u = ALWAN_LITERAL(4.0) * x / (ALWAN_LITERAL(-2.0) * x + ALWAN_LITERAL(12.0) * y + ALWAN_LITERAL(3.0));
    alwan_scalar v = ALWAN_LITERAL(6.0) * y / (ALWAN_LITERAL(-2.0) * x + ALWAN_LITERAL(12.0) * y + ALWAN_LITERAL(3.0));

    alwan_scalar u_p = ALWAN_LITERAL(4.0) * x_p / (ALWAN_LITERAL(-2.0) * x_p + ALWAN_LITERAL(12.0) * y_p + ALWAN_LITERAL(3.0));
    alwan_scalar v_p = ALWAN_LITERAL(6.0) * y_p / (ALWAN_LITERAL(-2.0) * x_p + ALWAN_LITERAL(12.0) * y_p + ALWAN_LITERAL(3.0));

    alwan_scalar du = u - u_p;
    alwan_scalar dv = v - v_p;

    return ALWAN_SQRT(du * du + dv * dv);
}

/* McCamy's formula for initial CCT estimate */
static alwan_scalar mccamy_cct_estimate(alwan_scalar x, alwan_scalar y) {
    alwan_scalar n = (x - ALWAN_LITERAL(0.3320)) / (ALWAN_LITERAL(0.1858) - y);
    return ALWAN_LITERAL(449.0) * n * n * n + ALWAN_LITERAL(3525.0) * n * n + ALWAN_LITERAL(6823.3) * n + ALWAN_LITERAL(5520.33);
}

int alwan_cct_duv_optimize(alwan_vec2 const *xy, alwan_scalar *cct_out, alwan_scalar *duv_out) {
    if (!xy || !cct_out || !duv_out) {
        return ALWAN_E_RANGE;
    }

    alwan_scalar x = xy->v[0];
    alwan_scalar y = xy->v[1];

    /* Initial CCT estimate using McCamy's formula */
    alwan_scalar cct = mccamy_cct_estimate(x, y);

    /* Clamp to reasonable range */
    if (cct < ALWAN_LITERAL(1000.0)) cct = ALWAN_LITERAL(1000.0);
    if (cct > ALWAN_LITERAL(25000.0)) cct = ALWAN_LITERAL(25000.0);

    /* Newton-Raphson iteration to find minimum Duv (not root!) */
    /* We're finding where d(Duv²)/d(CCT) = 0 for robustness */
    const int max_iter = 20;
    const alwan_scalar tol = ALWAN_LITERAL(0.01);
    const alwan_scalar h = ALWAN_LITERAL(1.0); /* Finite difference step */

    for (int iter = 0; iter < max_iter; iter++) {
        /* Compute Duv² at three points for first and second derivatives */
        alwan_scalar duv_minus = compute_duv(x, y, cct - h);
        alwan_scalar duv_current = compute_duv(x, y, cct);
        alwan_scalar duv_plus = compute_duv(x, y, cct + h);

        /* Use Duv² for better numerical stability */
        alwan_scalar f_minus = duv_minus * duv_minus;
        alwan_scalar f_current = duv_current * duv_current;
        alwan_scalar f_plus = duv_plus * duv_plus;

        /* First derivative (central difference) */
        alwan_scalar first_deriv = (f_plus - f_minus) / (ALWAN_LITERAL(2.0) * h);

        /* Second derivative */
        alwan_scalar second_deriv = (f_plus - ALWAN_LITERAL(2.0) * f_current + f_minus) / (h * h);

        /* Check for convergence or numerical issues */
        if (ALWAN_FABS(first_deriv) < ALWAN_LITERAL(1e-10)) break;
        if (ALWAN_FABS(second_deriv) < ALWAN_LITERAL(1e-10)) break;

        /* Newton step for minimization: x_new = x - f'(x)/f''(x) */
        alwan_scalar delta = -first_deriv / second_deriv;

        cct += delta;

        /* Clamp */
        if (cct < ALWAN_LITERAL(1000.0)) cct = ALWAN_LITERAL(1000.0);
        if (cct > ALWAN_LITERAL(25000.0)) cct = ALWAN_LITERAL(25000.0);

        /* Check convergence */
        if (ALWAN_FABS(delta) < tol) break;
    }

    *cct_out = cct;
    *duv_out = compute_duv(x, y, cct);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Tristimulus Optimization
 * ---------------------------------------------------------------- */

int alwan_optimize_spectrum_for_xyz(alwan_vec3 const *target_xyz,
                                      alwan_observer_type observer,
                                      alwan_ctx *ctx,
                                      alwan_spd *spd_out) {
    (void)observer; /* TODO: Use observer type for CMF selection */

    if (!target_xyz || !ctx || !spd_out || !spd_out->values) {
        return ALWAN_E_RANGE;
    }

    /* Simple optimization using Gaussian basis functions */
    /* This is a simplified approach - a full implementation would use
     * more sophisticated optimization (e.g., quasi-Newton methods) */

    const size_t num_gaussians = 7;
    const alwan_scalar centers[7] = {
        ALWAN_LITERAL(420.0), ALWAN_LITERAL(470.0), ALWAN_LITERAL(520.0),
        ALWAN_LITERAL(570.0), ALWAN_LITERAL(600.0), ALWAN_LITERAL(630.0),
        ALWAN_LITERAL(680.0)
    };
    const alwan_scalar widths[7] = {
        ALWAN_LITERAL(40.0), ALWAN_LITERAL(40.0), ALWAN_LITERAL(40.0),
        ALWAN_LITERAL(40.0), ALWAN_LITERAL(40.0), ALWAN_LITERAL(40.0),
        ALWAN_LITERAL(40.0)
    };

    /* Initialize SPD range */
    spd_out->wavelength_min = ALWAN_LITERAL(380.0);
    spd_out->wavelength_max = ALWAN_LITERAL(780.0);
    spd_out->count = 81; /* 380-780nm in 5nm steps */

    /* Simple initial guess: equal weight Gaussians scaled by luminance */
    alwan_scalar weights[7];
    alwan_scalar luminance = target_xyz->v[1]; /* Y component */
    for (size_t i = 0; i < num_gaussians; i++) {
        weights[i] = luminance / num_gaussians;
    }

    /* Build SPD from Gaussian basis */
    alwan_scalar wavelength_step = (spd_out->wavelength_max - spd_out->wavelength_min) / (spd_out->count - 1);
    for (size_t i = 0; i < spd_out->count; i++) {
        alwan_scalar lambda = spd_out->wavelength_min + i * wavelength_step;
        alwan_scalar value = 0.0;

        for (size_t j = 0; j < num_gaussians; j++) {
            alwan_scalar dx = (lambda - centers[j]) / widths[j];
            value += weights[j] * ALWAN_EXP(ALWAN_LITERAL(-0.5) * dx * dx);
        }

        spd_out->values[i] = (value > 0.0) ? value : 0.0;
    }

    /* Note: A complete implementation would:
     * 1. Use proper CMF convolution to compute XYZ from SPD
     * 2. Implement iterative optimization (gradient descent or quasi-Newton)
     * 3. Add constraints (non-negative, smoothness)
     * 4. Handle multiple observer types
     * This simplified version provides a reasonable starting point.
     */

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Table Interpolation Utilities
 * ---------------------------------------------------------------- */

alwan_scalar alwan_table_interp_1d(alwan_scalar const *table, size_t size,
                                    alwan_scalar x, alwan_interp_method method) {
    if (!table || size < 2) {
        return 0.0;
    }

    /* Clamp x to [0, 1] */
    if (x <= 0.0) return table[0];
    if (x >= 1.0) return table[size - 1];

    /* Map x to table index */
    alwan_scalar pos = x * (size - 1);
    size_t idx = (size_t)pos;
    alwan_scalar t = pos - idx;

    if (idx >= size - 1) {
        return table[size - 1];
    }

    switch (method) {
        case ALWAN_INTERP_LINEAR: {
            return table[idx] * (1.0 - t) + table[idx + 1] * t;
        }

        case ALWAN_INTERP_CUBIC: {
            /* Catmull-Rom cubic */
            alwan_scalar y0 = (idx > 0) ? table[idx - 1] : table[idx];
            alwan_scalar y1 = table[idx];
            alwan_scalar y2 = table[idx + 1];
            alwan_scalar y3 = (idx + 2 < size) ? table[idx + 2] : table[idx + 1];

            alwan_scalar t2 = t * t;
            alwan_scalar t3 = t2 * t;

            return ALWAN_LITERAL(0.5) * (
                (ALWAN_LITERAL(2.0) * y1) +
                (-y0 + y2) * t +
                (ALWAN_LITERAL(2.0) * y0 - ALWAN_LITERAL(5.0) * y1 + ALWAN_LITERAL(4.0) * y2 - y3) * t2 +
                (-y0 + ALWAN_LITERAL(3.0) * y1 - ALWAN_LITERAL(3.0) * y2 + y3) * t3
            );
        }

        default:
            return table[idx] * (1.0 - t) + table[idx + 1] * t;
    }
}

int alwan_table_interp_3d_trilinear(alwan_scalar const *table, size_t const sizes[3],
                                     alwan_vec3 const *rgb_in, alwan_vec3 *rgb_out) {
    if (!table || !sizes || !rgb_in || !rgb_out) {
        return ALWAN_E_RANGE;
    }

    /* Clamp input to [0, 1] */
    alwan_scalar r = rgb_in->v[0];
    alwan_scalar g = rgb_in->v[1];
    alwan_scalar b = rgb_in->v[2];

    if (r < 0.0) r = 0.0; if (r > 1.0) r = 1.0;
    if (g < 0.0) g = 0.0; if (g > 1.0) g = 1.0;
    if (b < 0.0) b = 0.0; if (b > 1.0) b = 1.0;

    /* Map to table indices */
    alwan_scalar r_pos = r * (sizes[0] - 1);
    alwan_scalar g_pos = g * (sizes[1] - 1);
    alwan_scalar b_pos = b * (sizes[2] - 1);

    size_t r_idx = (size_t)r_pos;
    size_t g_idx = (size_t)g_pos;
    size_t b_idx = (size_t)b_pos;

    alwan_scalar r_frac = r_pos - r_idx;
    alwan_scalar g_frac = g_pos - g_idx;
    alwan_scalar b_frac = b_pos - b_idx;

    /* Clamp indices */
    if (r_idx >= sizes[0] - 1) { r_idx = sizes[0] - 2; r_frac = 1.0; }
    if (g_idx >= sizes[1] - 1) { g_idx = sizes[1] - 2; g_frac = 1.0; }
    if (b_idx >= sizes[2] - 1) { b_idx = sizes[2] - 2; b_frac = 1.0; }

    /* Get 8 corner values */
    size_t stride_g = sizes[0] * 3;
    size_t stride_b = sizes[0] * sizes[1] * 3;

    #define GET_VALUE(ri, gi, bi, c) table[((bi) * stride_b + (gi) * stride_g + (ri) * 3 + (c))]

    alwan_scalar c000[3], c001[3], c010[3], c011[3];
    alwan_scalar c100[3], c101[3], c110[3], c111[3];

    for (int c = 0; c < 3; c++) {
        c000[c] = GET_VALUE(r_idx, g_idx, b_idx, c);
        c001[c] = GET_VALUE(r_idx, g_idx, b_idx + 1, c);
        c010[c] = GET_VALUE(r_idx, g_idx + 1, b_idx, c);
        c011[c] = GET_VALUE(r_idx, g_idx + 1, b_idx + 1, c);
        c100[c] = GET_VALUE(r_idx + 1, g_idx, b_idx, c);
        c101[c] = GET_VALUE(r_idx + 1, g_idx, b_idx + 1, c);
        c110[c] = GET_VALUE(r_idx + 1, g_idx + 1, b_idx, c);
        c111[c] = GET_VALUE(r_idx + 1, g_idx + 1, b_idx + 1, c);
    }

    #undef GET_VALUE

    /* Trilinear interpolation */
    for (int c = 0; c < 3; c++) {
        alwan_scalar c00 = c000[c] * (1.0 - r_frac) + c100[c] * r_frac;
        alwan_scalar c01 = c001[c] * (1.0 - r_frac) + c101[c] * r_frac;
        alwan_scalar c10 = c010[c] * (1.0 - r_frac) + c110[c] * r_frac;
        alwan_scalar c11 = c011[c] * (1.0 - r_frac) + c111[c] * r_frac;

        alwan_scalar c0 = c00 * (1.0 - g_frac) + c10 * g_frac;
        alwan_scalar c1 = c01 * (1.0 - g_frac) + c11 * g_frac;

        alwan_scalar result = c0 * (1.0 - b_frac) + c1 * b_frac;

        if (c == 0) rgb_out->v[0] = result;
        else if (c == 1) rgb_out->v[1] = result;
        else rgb_out->v[2] = result;
    }

    return ALWAN_OK;
}

int alwan_table_interp_3d_tetrahedral(alwan_scalar const *table, size_t const sizes[3],
                                       alwan_vec3 const *rgb_in, alwan_vec3 *rgb_out) {
    if (!table || !sizes || !rgb_in || !rgb_out) {
        return ALWAN_E_RANGE;
    }

    /* Clamp input to [0, 1] */
    alwan_scalar r = rgb_in->v[0];
    alwan_scalar g = rgb_in->v[1];
    alwan_scalar b = rgb_in->v[2];

    if (r < 0.0) r = 0.0; if (r > 1.0) r = 1.0;
    if (g < 0.0) g = 0.0; if (g > 1.0) g = 1.0;
    if (b < 0.0) b = 0.0; if (b > 1.0) b = 1.0;

    /* Map to table indices */
    alwan_scalar r_pos = r * (sizes[0] - 1);
    alwan_scalar g_pos = g * (sizes[1] - 1);
    alwan_scalar b_pos = b * (sizes[2] - 1);

    size_t r_idx = (size_t)r_pos;
    size_t g_idx = (size_t)g_pos;
    size_t b_idx = (size_t)b_pos;

    alwan_scalar r_frac = r_pos - r_idx;
    alwan_scalar g_frac = g_pos - g_idx;
    alwan_scalar b_frac = b_pos - b_idx;

    /* Clamp indices */
    if (r_idx >= sizes[0] - 1) { r_idx = sizes[0] - 2; r_frac = 1.0; }
    if (g_idx >= sizes[1] - 1) { g_idx = sizes[1] - 2; g_frac = 1.0; }
    if (b_idx >= sizes[2] - 1) { b_idx = sizes[2] - 2; b_frac = 1.0; }

    /* Get cube corner values */
    size_t stride_g = sizes[0] * 3;
    size_t stride_b = sizes[0] * sizes[1] * 3;

    #define GET_VALUE(ri, gi, bi, c) table[((bi) * stride_b + (gi) * stride_g + (ri) * 3 + (c))]

    alwan_scalar result[3] = {0.0, 0.0, 0.0};

    /* Tetrahedral interpolation - determine which tetrahedron we're in */
    for (int c = 0; c < 3; c++) {
        alwan_scalar c000 = GET_VALUE(r_idx, g_idx, b_idx, c);
        alwan_scalar c111 = GET_VALUE(r_idx + 1, g_idx + 1, b_idx + 1, c);

        if (r_frac > g_frac) {
            if (g_frac > b_frac) {
                /* Tetrahedron 1: r > g > b */
                alwan_scalar c100 = GET_VALUE(r_idx + 1, g_idx, b_idx, c);
                alwan_scalar c110 = GET_VALUE(r_idx + 1, g_idx + 1, b_idx, c);
                result[c] = c000 + (c100 - c000) * r_frac +
                           (c110 - c100) * g_frac + (c111 - c110) * b_frac;
            } else if (r_frac > b_frac) {
                /* Tetrahedron 2: r > b > g */
                alwan_scalar c100 = GET_VALUE(r_idx + 1, g_idx, b_idx, c);
                alwan_scalar c101 = GET_VALUE(r_idx + 1, g_idx, b_idx + 1, c);
                result[c] = c000 + (c100 - c000) * r_frac +
                           (c101 - c100) * b_frac + (c111 - c101) * g_frac;
            } else {
                /* Tetrahedron 3: b > r > g */
                alwan_scalar c001 = GET_VALUE(r_idx, g_idx, b_idx + 1, c);
                alwan_scalar c101 = GET_VALUE(r_idx + 1, g_idx, b_idx + 1, c);
                result[c] = c000 + (c001 - c000) * b_frac +
                           (c101 - c001) * r_frac + (c111 - c101) * g_frac;
            }
        } else {
            if (b_frac > g_frac) {
                /* Tetrahedron 4: b > g > r */
                alwan_scalar c001 = GET_VALUE(r_idx, g_idx, b_idx + 1, c);
                alwan_scalar c011 = GET_VALUE(r_idx, g_idx + 1, b_idx + 1, c);
                result[c] = c000 + (c001 - c000) * b_frac +
                           (c011 - c001) * g_frac + (c111 - c011) * r_frac;
            } else if (b_frac > r_frac) {
                /* Tetrahedron 5: g > b > r */
                alwan_scalar c010 = GET_VALUE(r_idx, g_idx + 1, b_idx, c);
                alwan_scalar c011 = GET_VALUE(r_idx, g_idx + 1, b_idx + 1, c);
                result[c] = c000 + (c010 - c000) * g_frac +
                           (c011 - c010) * b_frac + (c111 - c011) * r_frac;
            } else {
                /* Tetrahedron 6: g > r > b */
                alwan_scalar c010 = GET_VALUE(r_idx, g_idx + 1, b_idx, c);
                alwan_scalar c110 = GET_VALUE(r_idx + 1, g_idx + 1, b_idx, c);
                result[c] = c000 + (c010 - c000) * g_frac +
                           (c110 - c010) * r_frac + (c111 - c110) * b_frac;
            }
        }
    }

    #undef GET_VALUE

    rgb_out->v[0] = result[0];
    rgb_out->v[1] = result[1];
    rgb_out->v[2] = result[2];

    return ALWAN_OK;
}
