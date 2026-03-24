/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Unrolled Matrix Operations (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Uses ALWAN_CORE_* macros (set by alwan_core_f32_setup.h or _f64_setup.h).
 */

/* ================================================================
 * Identity Matrix
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FNVLIT(alwan_mat3_identity)(void) {
    ALWAN_CORE_MAT3X3 out;
    out.m[0] = ALWAN_CORE_LITERAL(1.0);
    out.m[1] = ALWAN_CORE_LITERAL(0.0);
    out.m[2] = ALWAN_CORE_LITERAL(0.0);
    out.m[3] = ALWAN_CORE_LITERAL(0.0);
    out.m[4] = ALWAN_CORE_LITERAL(1.0);
    out.m[5] = ALWAN_CORE_LITERAL(0.0);
    out.m[6] = ALWAN_CORE_LITERAL(0.0);
    out.m[7] = ALWAN_CORE_LITERAL(0.0);
    out.m[8] = ALWAN_CORE_LITERAL(1.0);
    return out;
}

/* ================================================================
 * Determinant (Sarrus' Rule)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_mat3_det)(ALWAN_CORE_MAT3X3 m) {
    return m.m[0] * (m.m[4] * m.m[8] - m.m[5] * m.m[7]) -
           m.m[1] * (m.m[3] * m.m[8] - m.m[5] * m.m[6]) +
           m.m[2] * (m.m[3] * m.m[7] - m.m[4] * m.m[6]);
}

/* ================================================================
 * Matrix-Vector Multiplication (Unrolled)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_MAT3X3 m, ALWAN_CORE_VEC3 v) {
    ALWAN_CORE_VEC3 out;
    out.v[0] = m.m[0] * v.v[0] + m.m[1] * v.v[1] + m.m[2] * v.v[2];
    out.v[1] = m.m[3] * v.v[0] + m.m[4] * v.v[1] + m.m[5] * v.v[2];
    out.v[2] = m.m[6] * v.v[0] + m.m[7] * v.v[1] + m.m[8] * v.v[2];
    return out;
}

/* ================================================================
 * Matrix-Matrix Multiplication (Unrolled)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FNVLIT(alwan_mat3_mul)(ALWAN_CORE_MAT3X3 a, ALWAN_CORE_MAT3X3 b) {
    ALWAN_CORE_MAT3X3 out;
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

ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FNVLIT(alwan_mat3_inv)(ALWAN_CORE_MAT3X3 m) {
    /* Cofactors (unrolled 2x2 determinants) */
    ALWAN_CORE_T c00 = m.m[4] * m.m[8] - m.m[5] * m.m[7];
    ALWAN_CORE_T c01 = m.m[5] * m.m[6] - m.m[3] * m.m[8];
    ALWAN_CORE_T c02 = m.m[3] * m.m[7] - m.m[4] * m.m[6];
    ALWAN_CORE_T c10 = m.m[2] * m.m[7] - m.m[1] * m.m[8];
    ALWAN_CORE_T c11 = m.m[0] * m.m[8] - m.m[2] * m.m[6];
    ALWAN_CORE_T c12 = m.m[1] * m.m[6] - m.m[0] * m.m[7];
    ALWAN_CORE_T c20 = m.m[1] * m.m[5] - m.m[2] * m.m[4];
    ALWAN_CORE_T c21 = m.m[2] * m.m[3] - m.m[0] * m.m[5];
    ALWAN_CORE_T c22 = m.m[0] * m.m[4] - m.m[1] * m.m[3];

    /* Determinant via reuse */
    ALWAN_CORE_T det = ALWAN_CORE_FNVLIT(alwan_mat3_det)(m);

    /* Guard: if singular, return identity */
    ALWAN_CORE_T inv_det = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(det) < ALWAN_CORE_EPSILON,
                                              ALWAN_CORE_LITERAL(0.0),
                                              ALWAN_CORE_LITERAL(1.0) / det);
    ALWAN_CORE_T is_singular = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(det) < ALWAN_CORE_EPSILON,
                                                  ALWAN_CORE_LITERAL(1.0),
                                                  ALWAN_CORE_LITERAL(0.0));

    /* Adjugate (transposed cofactor matrix) / det */
    ALWAN_CORE_MAT3X3 out;
    out.m[0] = c00 * inv_det + is_singular * ALWAN_CORE_LITERAL(1.0);
    out.m[1] = c10 * inv_det;
    out.m[2] = c20 * inv_det;
    out.m[3] = c01 * inv_det;
    out.m[4] = c11 * inv_det + is_singular * ALWAN_CORE_LITERAL(1.0);
    out.m[5] = c21 * inv_det;
    out.m[6] = c02 * inv_det;
    out.m[7] = c12 * inv_det;
    out.m[8] = c22 * inv_det + is_singular * ALWAN_CORE_LITERAL(1.0);
    return out;
}

/* ================================================================
 * Lanczos Windowed Sinc Kernel
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_lanczos_kernel)(ALWAN_CORE_T x, ALWAN_CORE_T a) {
    ALWAN_CORE_T x_abs = ALWAN_CORE_ABS(x);
    ALWAN_CORE_T pi_x = ALWAN_CORE_PI * x;
    ALWAN_CORE_T pi_x_a = pi_x / a;

    /* sinc(x) * sinc(x/a) */
    ALWAN_CORE_T sinc_val = (ALWAN_CORE_SIN(pi_x) / pi_x) * (ALWAN_CORE_SIN(pi_x_a) / pi_x_a);

    /* x == 0 => 1.0, |x| >= a => 0.0, else sinc */
    ALWAN_CORE_T result = ALWAN_CORE_SELECT(x_abs < ALWAN_CORE_EPSILON, ALWAN_CORE_ONE,
                          ALWAN_CORE_SELECT(x_abs >= a, ALWAN_CORE_ZERO, sinc_val));
    return result;
}

/* ================================================================
 * Catmull-Rom Cubic Interpolation (4-point)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_catmull_rom)(
    ALWAN_CORE_T y0, ALWAN_CORE_T y1, ALWAN_CORE_T y2, ALWAN_CORE_T y3,
    ALWAN_CORE_T t)
{
    ALWAN_CORE_T t2 = t * t;
    ALWAN_CORE_T t3 = t2 * t;

    return ALWAN_CORE_LITERAL(0.5) * (
        (ALWAN_CORE_LITERAL(2.0) * y1) +
        (-y0 + y2) * t +
        (ALWAN_CORE_LITERAL(2.0) * y0 - ALWAN_CORE_LITERAL(5.0) * y1 + ALWAN_CORE_LITERAL(4.0) * y2 - y3) * t2 +
        (-y0 + ALWAN_CORE_LITERAL(3.0) * y1 - ALWAN_CORE_LITERAL(3.0) * y2 + y3) * t3);
}

/* ================================================================
 * CCT / Planckian Locus Helpers
 * ================================================================ */

/* Planckian locus xy from CCT (Hernandez-Andres 1999 approximation, branchless) */
ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(alwan_cct_to_xy_planckian)(ALWAN_CORE_T cct) {
    ALWAN_CORE_VEC2 result;
    ALWAN_CORE_T T = cct;
    ALWAN_CORE_T T2 = T * T;
    ALWAN_CORE_T T3 = T2 * T;

    /* x: two-region piecewise at T=4000 */
    ALWAN_CORE_T x_lo = ALWAN_CORE_LITERAL(-0.2661239e9) / T3 - ALWAN_CORE_LITERAL(0.2343589e6) / T2 +
                        ALWAN_CORE_LITERAL(0.8776956e3) / T + ALWAN_CORE_LITERAL(0.179910);
    ALWAN_CORE_T x_hi = ALWAN_CORE_LITERAL(-3.0258469e9) / T3 + ALWAN_CORE_LITERAL(2.1070379e6) / T2 +
                        ALWAN_CORE_LITERAL(0.2226347e3) / T + ALWAN_CORE_LITERAL(0.240390);
    ALWAN_CORE_T x = ALWAN_CORE_SELECT(T <= ALWAN_CORE_LITERAL(4000.0), x_lo, x_hi);

    ALWAN_CORE_T x2 = x * x;
    ALWAN_CORE_T x3 = x2 * x;

    /* y: three-region piecewise at T=2222 and T=4000 */
    ALWAN_CORE_T y_lo  = ALWAN_CORE_LITERAL(-1.1063814) * x3 - ALWAN_CORE_LITERAL(1.34811020) * x2 +
                         ALWAN_CORE_LITERAL(2.18555832) * x - ALWAN_CORE_LITERAL(0.20219683);
    ALWAN_CORE_T y_mid = ALWAN_CORE_LITERAL(-0.9549476) * x3 - ALWAN_CORE_LITERAL(1.37418593) * x2 +
                         ALWAN_CORE_LITERAL(2.09137015) * x - ALWAN_CORE_LITERAL(0.16748867);
    ALWAN_CORE_T y_hi2 = ALWAN_CORE_LITERAL(3.0817580) * x3 - ALWAN_CORE_LITERAL(5.87338670) * x2 +
                         ALWAN_CORE_LITERAL(3.75112997) * x - ALWAN_CORE_LITERAL(0.37001483);
    ALWAN_CORE_T y = ALWAN_CORE_SELECT(T <= ALWAN_CORE_LITERAL(2222.0), y_lo,
                     ALWAN_CORE_SELECT(T <= ALWAN_CORE_LITERAL(4000.0), y_mid, y_hi2));

    result.v[0] = x;
    result.v[1] = y;
    return result;
}

/* Duv: distance from xy point to Planckian locus at given CCT */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_compute_duv)(ALWAN_CORE_T x, ALWAN_CORE_T y, ALWAN_CORE_T cct) {
    ALWAN_CORE_VEC2 xy_p = ALWAN_CORE_FNVLIT(alwan_cct_to_xy_planckian)(cct);
    ALWAN_CORE_T x_p = xy_p.v[0];
    ALWAN_CORE_T y_p = xy_p.v[1];

    /* Convert to CIE 1960 UCS (u, v) */
    ALWAN_CORE_T denom   = ALWAN_CORE_LITERAL(-2.0) * x   + ALWAN_CORE_LITERAL(12.0) * y   + ALWAN_CORE_LITERAL(3.0);
    ALWAN_CORE_T denom_p = ALWAN_CORE_LITERAL(-2.0) * x_p + ALWAN_CORE_LITERAL(12.0) * y_p + ALWAN_CORE_LITERAL(3.0);

    ALWAN_CORE_T u   = ALWAN_CORE_LITERAL(4.0) * x   / denom;
    ALWAN_CORE_T v   = ALWAN_CORE_LITERAL(6.0) * y   / denom;
    ALWAN_CORE_T u_p = ALWAN_CORE_LITERAL(4.0) * x_p / denom_p;
    ALWAN_CORE_T v_p = ALWAN_CORE_LITERAL(6.0) * y_p / denom_p;

    ALWAN_CORE_T du = u - u_p;
    ALWAN_CORE_T dv = v - v_p;

    return ALWAN_CORE_SQRT(du * du + dv * dv);
}

/* McCamy's CCT estimate from xy chromaticity.
 * Reference: McCamy (1992) "Correlated color temperature as an explicit function
 *   of chromaticity coordinates" Color Res. Appl. 17(2):142-144.
 * Coefficients: n = (x-0.3320)/(0.1858-y); CCT = 449n^3 + 3525n^2 + 6823.3n + 5520.33.
 * Valid approximately 2856–7000 K (incandescent to daylight). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_mccamy_cct)(ALWAN_CORE_T x, ALWAN_CORE_T y) {
    ALWAN_CORE_T n = (x - ALWAN_CORE_LITERAL(0.3320)) / (ALWAN_CORE_LITERAL(0.1858) - y);
    return ALWAN_CORE_LITERAL(449.0) * n * n * n + ALWAN_CORE_LITERAL(3525.0) * n * n +
           ALWAN_CORE_LITERAL(6823.3) * n + ALWAN_CORE_LITERAL(5520.33);
}
