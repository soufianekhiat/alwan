/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_math_core.h"
#include <string.h>

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_math_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_math_impl.inc"
#include "alwan_api_teardown.h"

/* ================================================================
 * Advanced Mathematical & Utility Functions
 * ================================================================ */

/* Find interval index for x in sorted array x_in */
static size_t find_interval(alwan_f64 const *x_in, size_t count, alwan_f64 x) {
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

/* Lanczos kernel */
static alwan_f64 lanczos_kernel(alwan_f64 x, int a) {
    return alwan_lanczos_kernel_f64_v(x, (alwan_f64)a);
}

/* ----------------------------------------------------------------
 * Advanced Interpolation Methods
 * ---------------------------------------------------------------- */

int alwan_interpolate(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                       alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                       alwan_interp_method method) {
    if (!x_in || !y_in || !x_out || !y_out || count_in < 2 || count_out == 0) {
        return ALWAN_E_RANGE;
    }

    for (size_t i = 0; i < count_out; i++) {
        alwan_f64 x = x_out[i];

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
        alwan_f64 x0 = x_in[idx];
        alwan_f64 x1 = x_in[idx + 1];
        alwan_f64 t = (x - x0) / (x1 - x0);

        switch (method) {
            case ALWAN_INTERP_LINEAR: {
                /* Linear interpolation */
                y_out[i] = y_in[idx] * (ALWAN_LITERAL(1.0) - t) + y_in[idx + 1] * t;
                break;
            }

            case ALWAN_INTERP_CUBIC: {
                /* Catmull-Rom cubic spline */
                alwan_f64 y0 = (idx > 0) ? y_in[idx - 1] : y_in[idx];
                alwan_f64 y1 = y_in[idx];
                alwan_f64 y2 = y_in[idx + 1];
                alwan_f64 y3 = (idx + 2 < count_in) ? y_in[idx + 2] : y_in[idx + 1];

                y_out[i] = alwan_catmull_rom_f64_v(y0, y1, y2, y3, t);
                break;
            }

            case ALWAN_INTERP_LANCZOS: {
                /* Lanczos interpolation (a=3) */
                const int a = 3;
                alwan_f64 sum = 0.0;
                alwan_f64 weight_sum = 0.0;

                for (int j = -a + 1; j <= a; j++) {
                    int k = (int)idx + j;
                    if (k < 0 || k >= (int)count_in) continue;

                    alwan_f64 dx = x - x_in[k];
                    alwan_f64 normalized_dx = dx / (x1 - x0);
                    alwan_f64 weight = lanczos_kernel(normalized_dx, a);

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
                    alwan_f64 y0 = (idx > 0) ? y_in[idx - 1] : y_in[idx];
                    alwan_f64 y1 = y_in[idx];
                    alwan_f64 y2 = y_in[idx + 1];
                    alwan_f64 y3 = (idx + 2 < count_in) ? y_in[idx + 2] : y_in[idx + 1];

                    y_out[i] = alwan_catmull_rom_f64_v(y0, y1, y2, y3, t);
                } else {
                    /* Sprague coefficients for 6 points */
                    alwan_f64 y[6];
                    for (int j = 0; j < 6; j++) {
                        int k = (int)idx + j - 2;
                        if (k < 0) k = 0;
                        if (k >= (int)count_in) k = (int)count_in - 1;
                        y[j] = y_in[k];
                    }

                    /* Sprague formula */
                    alwan_f64 t2 = t * t;
                    alwan_f64 t3 = t2 * t;
                    alwan_f64 t4 = t3 * t;
                    alwan_f64 t5 = t4 * t;

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
                alwan_f64 y[4];
                alwan_f64 x_vals[4];

                int start = (int)idx - 1;
                if (start < 0) start = 0;
                if (start + 4 > (int)count_in) start = (int)count_in - 4;

                for (int j = 0; j < 4; j++) {
                    y[j] = y_in[start + j];
                    x_vals[j] = x_in[start + j];
                }

                alwan_f64 result = 0.0;
                for (int j = 0; j < 4; j++) {
                    alwan_f64 term = y[j];
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
                    y_out[i] = y_in[idx] * (ALWAN_LITERAL(1.0) - t) + y_in[idx + 1] * t;
                } else {
                    /* Calculate slopes */
                    alwan_f64 m[5];
                    for (int j = -2; j <= 2; j++) {
                        int k = (int)idx + j;
                        if (k < 0) k = 0;
                        if (k >= (int)count_in - 1) k = (int)count_in - 2;
                        m[j + 2] = (y_in[k + 1] - y_in[k]) / (x_in[k + 1] - x_in[k]);
                    }

                    /* Akima weights */
                    alwan_f64 w1 = ALWAN_ABS(m[3] - m[2]);
                    alwan_f64 w2 = ALWAN_ABS(m[1] - m[0]);
                    alwan_f64 slope = (w1 + w2 > 0.0) ?
                        ((w1 * m[1] + w2 * m[3]) / (w1 + w2)) :
                        (ALWAN_LITERAL(0.5) * (m[1] + m[3]));

                    /* Hermite interpolation */
                    alwan_f64 y1 = y_in[idx];
                    alwan_f64 y2 = y_in[idx + 1];
                    alwan_f64 dx = x1 - x0;

                    alwan_f64 h00 = (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(2.0) * t) * (ALWAN_LITERAL(1.0) - t) * (ALWAN_LITERAL(1.0) - t);
                    alwan_f64 h10 = t * (ALWAN_LITERAL(1.0) - t) * (ALWAN_LITERAL(1.0) - t);
                    alwan_f64 h01 = t * t * (ALWAN_LITERAL(3.0) - ALWAN_LITERAL(2.0) * t);
                    alwan_f64 h11 = t * t * (t - ALWAN_LITERAL(1.0));

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

int alwan_extrapolate(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                       alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                       alwan_extrap_method method) {
    if (!x_in || !y_in || !x_out || !y_out || count_in < 2 || count_out == 0) {
        return ALWAN_E_RANGE;
    }

    for (size_t i = 0; i < count_out; i++) {
        alwan_f64 x = x_out[i];
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
            alwan_f64 t = (x - x_in[idx]) / (x_in[idx + 1] - x_in[idx]);
            y_out[i] = y_in[idx] * (ALWAN_LITERAL(1.0) - t) + y_in[idx + 1] * t;
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
                    alwan_f64 slope = (y_in[1] - y_in[0]) / (x_in[1] - x_in[0]);
                    y_out[i] = y_in[0] + slope * (x - x_in[0]);
                } else {
                    alwan_f64 slope = (y_in[count_in - 1] - y_in[count_in - 2]) /
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
                        alwan_f64 slope = (y_in[1] - y_in[0]) / (x_in[1] - x_in[0]);
                        y_out[i] = y_in[0] + slope * (x - x_in[0]);
                    } else {
                        alwan_f64 slope = (y_in[count_in - 1] - y_in[count_in - 2]) /
                                            (x_in[count_in - 1] - x_in[count_in - 2]);
                        y_out[i] = y_in[count_in - 1] + slope * (x - x_in[count_in - 1]);
                    }
                } else {
                    /* Use 3 boundary points for quadratic fit */
                    alwan_f64 x0, x1, x2, y0, y1, y2;
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
                        alwan_f64 ratio = y_in[1] / y_in[0];
                        alwan_f64 lambda = ALWAN_LN(ratio) / (x_in[1] - x_in[0]);
                        y_out[i] = y_in[0] * ALWAN_EXP(lambda * (x - x_in[0]));
                    } else {
                        y_out[i] = y_in[0];
                    }
                } else {
                    if (y_in[count_in - 1] > 0.0 && y_in[count_in - 2] > 0.0) {
                        alwan_f64 ratio = y_in[count_in - 1] / y_in[count_in - 2];
                        alwan_f64 lambda = ALWAN_LN(ratio) / (x_in[count_in - 1] - x_in[count_in - 2]);
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
                alwan_f64 x_reflected;

                if (use_left) {
                    alwan_f64 dist = x_in[0] - x;
                    x_reflected = x_in[0] + dist;
                } else {
                    alwan_f64 dist = x - x_in[count_in - 1];
                    x_reflected = x_in[count_in - 1] - dist;
                }

                /* Clamp to valid range */
                if (x_reflected < x_in[0]) x_reflected = x_in[0];
                if (x_reflected > x_in[count_in - 1]) x_reflected = x_in[count_in - 1];

                /* Interpolate at reflected position */
                size_t idx = find_interval(x_in, count_in, x_reflected);
                alwan_f64 t = (x_reflected - x_in[idx]) / (x_in[idx + 1] - x_in[idx]);
                y_out[i] = y_in[idx] * (ALWAN_LITERAL(1.0) - t) + y_in[idx + 1] * t;
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

/* CCT helpers */
static alwan_f64 compute_duv(alwan_f64 x, alwan_f64 y, alwan_f64 cct) {
    return alwan_compute_duv_f64_v(x, y, cct);
}

int alwan_cct_duv_optimize(alwan_f64 *cct_out, alwan_f64 *duv_out, alwan_vec2 const *xy) {
    if (!xy || !cct_out || !duv_out) {
        return ALWAN_E_RANGE;
    }

    alwan_f64 x = xy->v[0];
    alwan_f64 y = xy->v[1];

    /* Initial CCT estimate using McCamy's formula */
    alwan_f64 cct = alwan_mccamy_cct_f64_v(x, y);

    /* Clamp to reasonable range */
    if (cct < ALWAN_LITERAL(1000.0)) cct = ALWAN_LITERAL(1000.0);
    if (cct > ALWAN_LITERAL(25000.0)) cct = ALWAN_LITERAL(25000.0);

    /* Newton-Raphson iteration to find minimum Duv (not root!) */
    /* We're finding where d(Duv^2)/d(CCT) = 0 for robustness */
    const int max_iter = 20;
    const alwan_f64 tol = ALWAN_LITERAL(0.01);
    const alwan_f64 h = ALWAN_LITERAL(1.0); /* Finite difference step */

    for (int iter = 0; iter < max_iter; iter++) {
        /* Compute Duv^2 at three points for first and second derivatives */
        alwan_f64 duv_minus = compute_duv(x, y, cct - h);
        alwan_f64 duv_current = compute_duv(x, y, cct);
        alwan_f64 duv_plus = compute_duv(x, y, cct + h);

        /* Use Duv^2 for better numerical stability */
        alwan_f64 f_minus = duv_minus * duv_minus;
        alwan_f64 f_current = duv_current * duv_current;
        alwan_f64 f_plus = duv_plus * duv_plus;

        /* First derivative (central difference) */
        alwan_f64 first_deriv = (f_plus - f_minus) / (ALWAN_LITERAL(2.0) * h);

        /* Second derivative */
        alwan_f64 second_deriv = (f_plus - ALWAN_LITERAL(2.0) * f_current + f_minus) / (h * h);

        /* Check for convergence or numerical issues */
        if (ALWAN_ABS(first_deriv) < ALWAN_LITERAL(1e-10)) break;
        if (ALWAN_ABS(second_deriv) < ALWAN_LITERAL(1e-10)) break;

        /* Newton step for minimization: x_new = x - f'(x)/f''(x) */
        alwan_f64 delta = -first_deriv / second_deriv;

        cct += delta;

        /* Clamp */
        if (cct < ALWAN_LITERAL(1000.0)) cct = ALWAN_LITERAL(1000.0);
        if (cct > ALWAN_LITERAL(25000.0)) cct = ALWAN_LITERAL(25000.0);

        /* Check convergence */
        if (ALWAN_ABS(delta) < tol) break;
    }

    *cct_out = cct;
    *duv_out = compute_duv(x, y, cct);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Tristimulus Optimization
 * ---------------------------------------------------------------- */

int alwan_optimize_spectrum_for_xyz(alwan_spd *spd_out,
                                      alwan_xyz const *target_xyz,
                                      alwan_observer_type observer,
                                      alwan_ctx *ctx) {
    (void)observer; /* Observer type parameter reserved for future CMF selection */

    if (!target_xyz || !ctx || !spd_out || !spd_out->values) {
        return ALWAN_E_RANGE;
    }

    /* Simple optimization using Gaussian basis functions */
    /* This is a simplified approach - a full implementation would use
     * more sophisticated optimization (e.g., quasi-Newton methods) */

    const size_t num_gaussians = 7;
    const alwan_f64 centers[7] = {
        ALWAN_LITERAL(420.0), ALWAN_LITERAL(470.0), ALWAN_LITERAL(520.0),
        ALWAN_LITERAL(570.0), ALWAN_LITERAL(600.0), ALWAN_LITERAL(630.0),
        ALWAN_LITERAL(680.0)
    };
    const alwan_f64 widths[7] = {
        ALWAN_LITERAL(40.0), ALWAN_LITERAL(40.0), ALWAN_LITERAL(40.0),
        ALWAN_LITERAL(40.0), ALWAN_LITERAL(40.0), ALWAN_LITERAL(40.0),
        ALWAN_LITERAL(40.0)
    };

    /* Initialize SPD range */
    spd_out->wavelength_min = ALWAN_LITERAL(380.0);
    spd_out->wavelength_max = ALWAN_LITERAL(780.0);
    spd_out->count = 81; /* 380-780nm in 5nm steps */

    /* Simple initial guess: equal weight Gaussians scaled by luminance */
    alwan_f64 weights[7];
    alwan_f64 luminance = target_xyz->y; /* Y component */
    for (size_t i = 0; i < num_gaussians; i++) {
        weights[i] = luminance / num_gaussians;
    }

    /* Build SPD from Gaussian basis */
    alwan_f64 wavelength_step = (spd_out->wavelength_max - spd_out->wavelength_min) / (spd_out->count - 1);
    for (size_t i = 0; i < spd_out->count; i++) {
        alwan_f64 lambda = spd_out->wavelength_min + i * wavelength_step;
        alwan_f64 value = 0.0;

        for (size_t j = 0; j < num_gaussians; j++) {
            alwan_f64 dx = (lambda - centers[j]) / widths[j];
            value += weights[j] * ALWAN_EXP(ALWAN_LITERAL(-0.5) * dx * dx);
        }

        spd_out->values[i] = (value > ALWAN_LITERAL(0.0)) ? value : ALWAN_LITERAL(0.0);
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

alwan_f64 alwan_table_interp_1d(alwan_f64 const *table, size_t size,
                                    alwan_f64 x, alwan_interp_method method) {
    if (!table || size < 2) {
        return 0.0;
    }

    /* Clamp x to [0, 1] */
    if (x <= 0.0) return table[0];
    if (x >= 1.0) return table[size - 1];

    /* Map x to table index */
    alwan_f64 pos = x * (size - 1);
    size_t idx = (size_t)pos;
    alwan_f64 t = pos - idx;

    if (idx >= size - 1) {
        return table[size - 1];
    }

    switch (method) {
        case ALWAN_INTERP_LINEAR: {
            return table[idx] * (ALWAN_LITERAL(1.0) - t) + table[idx + 1] * t;
        }

        case ALWAN_INTERP_CUBIC: {
            /* Catmull-Rom cubic */
            alwan_f64 y0 = (idx > 0) ? table[idx - 1] : table[idx];
            alwan_f64 y1 = table[idx];
            alwan_f64 y2 = table[idx + 1];
            alwan_f64 y3 = (idx + 2 < size) ? table[idx + 2] : table[idx + 1];

            return alwan_catmull_rom_f64_v(y0, y1, y2, y3, t);
        }

        default:
            return table[idx] * (ALWAN_LITERAL(1.0) - t) + table[idx + 1] * t;
    }
}

int alwan_table_interp_3d_trilinear(alwan_rgb *rgb_out,
                                     alwan_f64 const *table, size_t const sizes[3],
                                     alwan_rgb const *rgb_in) {
    if (!table || !sizes || !rgb_in || !rgb_out) {
        return ALWAN_E_RANGE;
    }

    /* Clamp input to [0, 1] */
    alwan_f64 r = rgb_in->r;
    alwan_f64 g = rgb_in->g;
    alwan_f64 b = rgb_in->b;

    if (r < 0.0) r = 0.0; if (r > 1.0) r = 1.0;
    if (g < 0.0) g = 0.0; if (g > 1.0) g = 1.0;
    if (b < 0.0) b = 0.0; if (b > 1.0) b = 1.0;

    /* Map to table indices */
    alwan_f64 r_pos = r * (sizes[0] - 1);
    alwan_f64 g_pos = g * (sizes[1] - 1);
    alwan_f64 b_pos = b * (sizes[2] - 1);

    size_t r_idx = (size_t)r_pos;
    size_t g_idx = (size_t)g_pos;
    size_t b_idx = (size_t)b_pos;

    alwan_f64 r_frac = r_pos - r_idx;
    alwan_f64 g_frac = g_pos - g_idx;
    alwan_f64 b_frac = b_pos - b_idx;

    /* Clamp indices */
    if (r_idx >= sizes[0] - 1) { r_idx = sizes[0] - 2; r_frac = 1.0; }
    if (g_idx >= sizes[1] - 1) { g_idx = sizes[1] - 2; g_frac = 1.0; }
    if (b_idx >= sizes[2] - 1) { b_idx = sizes[2] - 2; b_frac = 1.0; }

    /* Get 8 corner values */
    size_t stride_g = sizes[0] * 3;
    size_t stride_b = sizes[0] * sizes[1] * 3;

    #define GET_VALUE(ri, gi, bi, c) table[((bi) * stride_b + (gi) * stride_g + (ri) * 3 + (c))]

    alwan_f64 c000[3], c001[3], c010[3], c011[3];
    alwan_f64 c100[3], c101[3], c110[3], c111[3];

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
        alwan_f64 c00 = c000[c] * (ALWAN_LITERAL(1.0) - r_frac) + c100[c] * r_frac;
        alwan_f64 c01 = c001[c] * (ALWAN_LITERAL(1.0) - r_frac) + c101[c] * r_frac;
        alwan_f64 c10 = c010[c] * (ALWAN_LITERAL(1.0) - r_frac) + c110[c] * r_frac;
        alwan_f64 c11 = c011[c] * (ALWAN_LITERAL(1.0) - r_frac) + c111[c] * r_frac;

        alwan_f64 c0 = c00 * (ALWAN_LITERAL(1.0) - g_frac) + c10 * g_frac;
        alwan_f64 c1 = c01 * (ALWAN_LITERAL(1.0) - g_frac) + c11 * g_frac;

        alwan_f64 result = c0 * (ALWAN_LITERAL(1.0) - b_frac) + c1 * b_frac;

        if (c == 0) rgb_out->r = result;
        else if (c == 1) rgb_out->g = result;
        else rgb_out->b = result;
    }

    return ALWAN_OK;
}

int alwan_table_interp_3d_tetrahedral(alwan_rgb *rgb_out,
                                       alwan_f64 const *table, size_t const sizes[3],
                                       alwan_rgb const *rgb_in) {
    if (!table || !sizes || !rgb_in || !rgb_out) {
        return ALWAN_E_RANGE;
    }

    /* Clamp input to [0, 1] */
    alwan_f64 r = rgb_in->r;
    alwan_f64 g = rgb_in->g;
    alwan_f64 b = rgb_in->b;

    if (r < 0.0) r = 0.0; if (r > 1.0) r = 1.0;
    if (g < 0.0) g = 0.0; if (g > 1.0) g = 1.0;
    if (b < 0.0) b = 0.0; if (b > 1.0) b = 1.0;

    /* Map to table indices */
    alwan_f64 r_pos = r * (sizes[0] - 1);
    alwan_f64 g_pos = g * (sizes[1] - 1);
    alwan_f64 b_pos = b * (sizes[2] - 1);

    size_t r_idx = (size_t)r_pos;
    size_t g_idx = (size_t)g_pos;
    size_t b_idx = (size_t)b_pos;

    alwan_f64 r_frac = r_pos - r_idx;
    alwan_f64 g_frac = g_pos - g_idx;
    alwan_f64 b_frac = b_pos - b_idx;

    /* Clamp indices */
    if (r_idx >= sizes[0] - 1) { r_idx = sizes[0] - 2; r_frac = 1.0; }
    if (g_idx >= sizes[1] - 1) { g_idx = sizes[1] - 2; g_frac = 1.0; }
    if (b_idx >= sizes[2] - 1) { b_idx = sizes[2] - 2; b_frac = 1.0; }

    /* Get cube corner values */
    size_t stride_g = sizes[0] * 3;
    size_t stride_b = sizes[0] * sizes[1] * 3;

    #define GET_VALUE(ri, gi, bi, c) table[((bi) * stride_b + (gi) * stride_g + (ri) * 3 + (c))]

    alwan_f64 result[3] = {0.0, 0.0, 0.0};

    /* Tetrahedral interpolation - determine which tetrahedron we're in */
    for (int c = 0; c < 3; c++) {
        alwan_f64 c000 = GET_VALUE(r_idx, g_idx, b_idx, c);
        alwan_f64 c111 = GET_VALUE(r_idx + 1, g_idx + 1, b_idx + 1, c);

        if (r_frac > g_frac) {
            if (g_frac > b_frac) {
                /* Tetrahedron 1: r > g > b */
                alwan_f64 c100 = GET_VALUE(r_idx + 1, g_idx, b_idx, c);
                alwan_f64 c110 = GET_VALUE(r_idx + 1, g_idx + 1, b_idx, c);
                result[c] = c000 + (c100 - c000) * r_frac +
                           (c110 - c100) * g_frac + (c111 - c110) * b_frac;
            } else if (r_frac > b_frac) {
                /* Tetrahedron 2: r > b > g */
                alwan_f64 c100 = GET_VALUE(r_idx + 1, g_idx, b_idx, c);
                alwan_f64 c101 = GET_VALUE(r_idx + 1, g_idx, b_idx + 1, c);
                result[c] = c000 + (c100 - c000) * r_frac +
                           (c101 - c100) * b_frac + (c111 - c101) * g_frac;
            } else {
                /* Tetrahedron 3: b > r > g */
                alwan_f64 c001 = GET_VALUE(r_idx, g_idx, b_idx + 1, c);
                alwan_f64 c101 = GET_VALUE(r_idx + 1, g_idx, b_idx + 1, c);
                result[c] = c000 + (c001 - c000) * b_frac +
                           (c101 - c001) * r_frac + (c111 - c101) * g_frac;
            }
        } else {
            if (b_frac > g_frac) {
                /* Tetrahedron 4: b > g > r */
                alwan_f64 c001 = GET_VALUE(r_idx, g_idx, b_idx + 1, c);
                alwan_f64 c011 = GET_VALUE(r_idx, g_idx + 1, b_idx + 1, c);
                result[c] = c000 + (c001 - c000) * b_frac +
                           (c011 - c001) * g_frac + (c111 - c011) * r_frac;
            } else if (b_frac > r_frac) {
                /* Tetrahedron 5: g > b > r */
                alwan_f64 c010 = GET_VALUE(r_idx, g_idx + 1, b_idx, c);
                alwan_f64 c011 = GET_VALUE(r_idx, g_idx + 1, b_idx + 1, c);
                result[c] = c000 + (c010 - c000) * g_frac +
                           (c011 - c010) * b_frac + (c111 - c011) * r_frac;
            } else {
                /* Tetrahedron 6: g > r > b */
                alwan_f64 c010 = GET_VALUE(r_idx, g_idx + 1, b_idx, c);
                alwan_f64 c110 = GET_VALUE(r_idx + 1, g_idx + 1, b_idx, c);
                result[c] = c000 + (c010 - c000) * g_frac +
                           (c110 - c010) * r_frac + (c111 - c110) * b_frac;
            }
        }
    }

    #undef GET_VALUE

    rgb_out->r = result[0];
    rgb_out->g = result[1];
    rgb_out->b = result[2];

    return ALWAN_OK;
}
