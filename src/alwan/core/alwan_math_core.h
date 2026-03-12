/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Unrolled Matrix Operations
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_MATH_CORE_H
#define ALWAN_MATH_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

/* ================================================================
 * Identity Matrix
 * ================================================================ */

ALWAN_INLINE alwan_mat3x3 alwan_mat3_identity_v(void) {
    alwan_mat3x3 out;
    out.m[0] = ALWAN_LITERAL(1.0);
    out.m[1] = ALWAN_LITERAL(0.0);
    out.m[2] = ALWAN_LITERAL(0.0);
    out.m[3] = ALWAN_LITERAL(0.0);
    out.m[4] = ALWAN_LITERAL(1.0);
    out.m[5] = ALWAN_LITERAL(0.0);
    out.m[6] = ALWAN_LITERAL(0.0);
    out.m[7] = ALWAN_LITERAL(0.0);
    out.m[8] = ALWAN_LITERAL(1.0);
    return out;
}

/* ================================================================
 * Determinant (Sarrus' Rule)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_mat3_det_v(alwan_mat3x3 m) {
    return m.m[0] * (m.m[4] * m.m[8] - m.m[5] * m.m[7]) -
           m.m[1] * (m.m[3] * m.m[8] - m.m[5] * m.m[6]) +
           m.m[2] * (m.m[3] * m.m[7] - m.m[4] * m.m[6]);
}

/* ================================================================
 * Matrix-Vector Multiplication (Unrolled)
 * ================================================================ */

ALWAN_INLINE alwan_vec3 alwan_mat3_mulv_v(alwan_mat3x3 m, alwan_vec3 v) {
    alwan_vec3 out;
    out.v[0] = m.m[0] * v.v[0] + m.m[1] * v.v[1] + m.m[2] * v.v[2];
    out.v[1] = m.m[3] * v.v[0] + m.m[4] * v.v[1] + m.m[5] * v.v[2];
    out.v[2] = m.m[6] * v.v[0] + m.m[7] * v.v[1] + m.m[8] * v.v[2];
    return out;
}

/* ================================================================
 * Matrix-Matrix Multiplication (Unrolled)
 * ================================================================ */

ALWAN_INLINE alwan_mat3x3 alwan_mat3_mul_v(alwan_mat3x3 a, alwan_mat3x3 b) {
    alwan_mat3x3 out;
    /* Row 0 */
    out.m[0] = a.m[0] * b.m[0] + a.m[1] * b.m[3] + a.m[2] * b.m[6];
    out.m[1] = a.m[0] * b.m[1] + a.m[1] * b.m[4] + a.m[2] * b.m[7];
    out.m[2] = a.m[0] * b.m[2] + a.m[1] * b.m[5] + a.m[2] * b.m[8];
    /* Row 1 */
    out.m[3] = a.m[3] * b.m[0] + a.m[4] * b.m[3] + a.m[5] * b.m[6];
    out.m[4] = a.m[3] * b.m[1] + a.m[4] * b.m[4] + a.m[5] * b.m[7];
    out.m[5] = a.m[3] * b.m[2] + a.m[4] * b.m[5] + a.m[5] * b.m[8];
    /* Row 2 */
    out.m[6] = a.m[6] * b.m[0] + a.m[7] * b.m[3] + a.m[8] * b.m[6];
    out.m[7] = a.m[6] * b.m[1] + a.m[7] * b.m[4] + a.m[8] * b.m[7];
    out.m[8] = a.m[6] * b.m[2] + a.m[7] * b.m[5] + a.m[8] * b.m[8];
    return out;
}

/* ================================================================
 * Matrix Inverse (Cofactor/Adjugate method, unrolled)
 * Returns identity on singular input (det ~= 0).
 * ================================================================ */

ALWAN_INLINE alwan_mat3x3 alwan_mat3_inv_v(alwan_mat3x3 m) {
    /* Cofactors (unrolled 2x2 determinants) */
    alwan_scalar c00 = m.m[4] * m.m[8] - m.m[5] * m.m[7];
    alwan_scalar c01 = m.m[5] * m.m[6] - m.m[3] * m.m[8];
    alwan_scalar c02 = m.m[3] * m.m[7] - m.m[4] * m.m[6];
    alwan_scalar c10 = m.m[2] * m.m[7] - m.m[1] * m.m[8];
    alwan_scalar c11 = m.m[0] * m.m[8] - m.m[2] * m.m[6];
    alwan_scalar c12 = m.m[1] * m.m[6] - m.m[0] * m.m[7];
    alwan_scalar c20 = m.m[1] * m.m[5] - m.m[2] * m.m[4];
    alwan_scalar c21 = m.m[2] * m.m[3] - m.m[0] * m.m[5];
    alwan_scalar c22 = m.m[0] * m.m[4] - m.m[1] * m.m[3];

    /* Determinant via reuse of alwan_mat3_det_v */
    alwan_scalar det = alwan_mat3_det_v(m);

    /* Guard: if singular, return identity */
    alwan_scalar inv_det = ALWAN_SELECT(ALWAN_ABS(det) < ALWAN_EPSILON,
                                        ALWAN_LITERAL(0.0),
                                        ALWAN_LITERAL(1.0) / det);
    alwan_scalar is_singular = ALWAN_SELECT(ALWAN_ABS(det) < ALWAN_EPSILON,
                                            ALWAN_LITERAL(1.0),
                                            ALWAN_LITERAL(0.0));

    /* Adjugate (transposed cofactor matrix) / det */
    alwan_mat3x3 out;
    out.m[0] = c00 * inv_det + is_singular * ALWAN_LITERAL(1.0);
    out.m[1] = c10 * inv_det;
    out.m[2] = c20 * inv_det;
    out.m[3] = c01 * inv_det;
    out.m[4] = c11 * inv_det + is_singular * ALWAN_LITERAL(1.0);
    out.m[5] = c21 * inv_det;
    out.m[6] = c02 * inv_det;
    out.m[7] = c12 * inv_det;
    out.m[8] = c22 * inv_det + is_singular * ALWAN_LITERAL(1.0);
    return out;
}

/* ================================================================
 * Lanczos Windowed Sinc Kernel
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_lanczos_kernel_v(alwan_scalar x, alwan_scalar a) {
    alwan_scalar x_abs = ALWAN_ABS(x);
    alwan_scalar pi_x = ALWAN_PI * x;
    alwan_scalar pi_x_a = pi_x / a;

    /* sinc(x) * sinc(x/a) */
    alwan_scalar sinc_val = (ALWAN_SIN(pi_x) / pi_x) * (ALWAN_SIN(pi_x_a) / pi_x_a);

    /* x == 0 => 1.0, |x| >= a => 0.0, else sinc */
    alwan_scalar result = ALWAN_SELECT(x_abs < ALWAN_EPSILON, ALWAN_ONE,
                          ALWAN_SELECT(x_abs >= a, ALWAN_ZERO, sinc_val));
    return result;
}

/* ================================================================
 * Catmull-Rom Cubic Interpolation (4-point)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_catmull_rom_v(
    alwan_scalar y0, alwan_scalar y1, alwan_scalar y2, alwan_scalar y3,
    alwan_scalar t)
{
    alwan_scalar t2 = t * t;
    alwan_scalar t3 = t2 * t;

    return ALWAN_LITERAL(0.5) * (
        (ALWAN_LITERAL(2.0) * y1) +
        (-y0 + y2) * t +
        (ALWAN_LITERAL(2.0) * y0 - ALWAN_LITERAL(5.0) * y1 + ALWAN_LITERAL(4.0) * y2 - y3) * t2 +
        (-y0 + ALWAN_LITERAL(3.0) * y1 - ALWAN_LITERAL(3.0) * y2 + y3) * t3);
}

/* ================================================================
 * CCT / Planckian Locus Helpers
 * ================================================================ */

/* Planckian locus xy from CCT (Hernandez-Andres 1999 approximation, branchless) */
ALWAN_INLINE alwan_vec2 alwan_cct_to_xy_planckian_v(alwan_scalar cct) {
    alwan_vec2 result;
    alwan_scalar T = cct;
    alwan_scalar T2 = T * T;
    alwan_scalar T3 = T2 * T;

    /* x: two-region piecewise at T=4000 */
    alwan_scalar x_lo = ALWAN_LITERAL(-0.2661239e9) / T3 - ALWAN_LITERAL(0.2343589e6) / T2 +
                        ALWAN_LITERAL(0.8776956e3) / T + ALWAN_LITERAL(0.179910);
    alwan_scalar x_hi = ALWAN_LITERAL(-3.0258469e9) / T3 + ALWAN_LITERAL(2.1070379e6) / T2 +
                        ALWAN_LITERAL(0.2226347e3) / T + ALWAN_LITERAL(0.240390);
    alwan_scalar x = ALWAN_SELECT(T <= ALWAN_LITERAL(4000.0), x_lo, x_hi);

    alwan_scalar x2 = x * x;
    alwan_scalar x3 = x2 * x;

    /* y: three-region piecewise at T=2222 and T=4000 */
    alwan_scalar y_lo  = ALWAN_LITERAL(-1.1063814) * x3 - ALWAN_LITERAL(1.34811020) * x2 +
                         ALWAN_LITERAL(2.18555832) * x - ALWAN_LITERAL(0.20219683);
    alwan_scalar y_mid = ALWAN_LITERAL(-0.9549476) * x3 - ALWAN_LITERAL(1.37418593) * x2 +
                         ALWAN_LITERAL(2.09137015) * x - ALWAN_LITERAL(0.16748867);
    alwan_scalar y_hi2 = ALWAN_LITERAL(3.0817580) * x3 - ALWAN_LITERAL(5.87338670) * x2 +
                         ALWAN_LITERAL(3.75112997) * x - ALWAN_LITERAL(0.37001483);
    alwan_scalar y = ALWAN_SELECT(T <= ALWAN_LITERAL(2222.0), y_lo,
                     ALWAN_SELECT(T <= ALWAN_LITERAL(4000.0), y_mid, y_hi2));

    result.v[0] = x;
    result.v[1] = y;
    return result;
}

/* Duv: distance from xy point to Planckian locus at given CCT */
ALWAN_INLINE alwan_scalar alwan_compute_duv_v(alwan_scalar x, alwan_scalar y, alwan_scalar cct) {
    alwan_vec2 xy_p = alwan_cct_to_xy_planckian_v(cct);
    alwan_scalar x_p = xy_p.v[0];
    alwan_scalar y_p = xy_p.v[1];

    /* Convert to CIE 1960 UCS (u, v) */
    alwan_scalar denom   = ALWAN_LITERAL(-2.0) * x   + ALWAN_LITERAL(12.0) * y   + ALWAN_LITERAL(3.0);
    alwan_scalar denom_p = ALWAN_LITERAL(-2.0) * x_p + ALWAN_LITERAL(12.0) * y_p + ALWAN_LITERAL(3.0);

    alwan_scalar u   = ALWAN_LITERAL(4.0) * x   / denom;
    alwan_scalar v   = ALWAN_LITERAL(6.0) * y   / denom;
    alwan_scalar u_p = ALWAN_LITERAL(4.0) * x_p / denom_p;
    alwan_scalar v_p = ALWAN_LITERAL(6.0) * y_p / denom_p;

    alwan_scalar du = u - u_p;
    alwan_scalar dv = v - v_p;

    return ALWAN_SQRT(du * du + dv * dv);
}

/* McCamy's CCT estimate from xy chromaticity */
ALWAN_INLINE alwan_scalar alwan_mccamy_cct_v(alwan_scalar x, alwan_scalar y) {
    alwan_scalar n = (x - ALWAN_LITERAL(0.3320)) / (ALWAN_LITERAL(0.1858) - y);
    return ALWAN_LITERAL(449.0) * n * n * n + ALWAN_LITERAL(3525.0) * n * n +
           ALWAN_LITERAL(6823.3) * n + ALWAN_LITERAL(5520.33);
}

#endif /* ALWAN_MATH_CORE_H */
