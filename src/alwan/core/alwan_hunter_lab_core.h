/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Hunter Lab Color Space
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Hunter (1948), ASTM D 1535
 */

#ifndef ALWAN_HUNTER_LAB_CORE_H
#define ALWAN_HUNTER_LAB_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

/* ----------------------------------------------------------------
 * Hunter Lab Constants
 * ---------------------------------------------------------------- */

/* Ka and Kb calculation constants (for reference white normalization) */
#define ALWAN_HUNTER_KA_XN_REF  ALWAN_LITERAL(98.043)
#define ALWAN_HUNTER_KB_ZN_REF  ALWAN_LITERAL(118.115)

/* D65 Illuminant coefficients (precomputed) */
#define ALWAN_HUNTER_KA_D65  ALWAN_LITERAL(172.30)
#define ALWAN_HUNTER_KB_D65  ALWAN_LITERAL(67.20)

/* D65 reference white (Y=100 scale) for Hunter Lab */
#define ALWAN_HUNTER_D65_XN  ALWAN_LITERAL(95.02)
#define ALWAN_HUNTER_D65_YN  ALWAN_LITERAL(100.0)
#define ALWAN_HUNTER_D65_ZN  ALWAN_LITERAL(108.82)

/* ----------------------------------------------------------------
 * Helper: Calculate Ka and Kb for custom illuminant (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_vec2 alwan_hunter_coefficients_v(alwan_xyz xyz_n) {
    alwan_vec2 result;
    /* Ka = 175 * sqrt(Xn / 98.043) */
    result.v[0] = ALWAN_LITERAL(175.0) * ALWAN_SQRT(xyz_n.x / ALWAN_HUNTER_KA_XN_REF);
    /* Kb = 70 * sqrt(Zn / 118.115) */
    result.v[1] = ALWAN_LITERAL(70.0) * ALWAN_SQRT(xyz_n.z / ALWAN_HUNTER_KB_ZN_REF);
    return result;
}

/* ----------------------------------------------------------------
 * XYZ -> Hunter Lab, D65 (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_hunter_lab alwan_xyz_to_hunter_lab_v(alwan_xyz xyz) {
    alwan_hunter_lab result;

    alwan_scalar y_ratio = xyz.y / ALWAN_HUNTER_D65_YN;
    alwan_scalar sqrt_y_ratio = ALWAN_SQRT(y_ratio);

    /* Division-by-zero guard */
    alwan_scalar safe = ALWAN_SELECT(sqrt_y_ratio < ALWAN_LITERAL(1e-10),
                                     ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

    result.L = ALWAN_LITERAL(100.0) * sqrt_y_ratio * safe;

    alwan_scalar x_ratio = xyz.x / ALWAN_HUNTER_D65_XN;
    alwan_scalar z_ratio = xyz.z / ALWAN_HUNTER_D65_ZN;

    /* Use safe to zero out when sqrt_y_ratio is too small */
    alwan_scalar inv_sqrt = ALWAN_SELECT(sqrt_y_ratio < ALWAN_LITERAL(1e-10),
                                         ALWAN_LITERAL(0.0),
                                         ALWAN_LITERAL(1.0) / sqrt_y_ratio);
    result.a = ALWAN_HUNTER_KA_D65 * (x_ratio - y_ratio) * inv_sqrt * safe;
    result.b = ALWAN_HUNTER_KB_D65 * (y_ratio - z_ratio) * inv_sqrt * safe;

    return result;
}

/* ----------------------------------------------------------------
 * Hunter Lab -> XYZ, D65 (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_xyz alwan_hunter_lab_to_xyz_v(alwan_hunter_lab hl) {
    alwan_xyz result;

    alwan_scalar l_norm = hl.L / ALWAN_LITERAL(100.0);

    result.y = l_norm * l_norm * ALWAN_HUNTER_D65_YN;

    alwan_scalar a_term = (hl.a / ALWAN_HUNTER_KA_D65) * l_norm;
    result.x = (a_term + l_norm * l_norm) * ALWAN_HUNTER_D65_XN;

    alwan_scalar b_term = (hl.b / ALWAN_HUNTER_KB_D65) * l_norm;
    result.z = -(b_term - l_norm * l_norm) * ALWAN_HUNTER_D65_ZN;

    return result;
}

/* ----------------------------------------------------------------
 * XYZ -> Hunter Lab, custom illuminant (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_hunter_lab alwan_xyz_to_hunter_lab_custom_v(alwan_xyz xyz, alwan_xyz xyz_n) {
    alwan_hunter_lab result;

    alwan_vec2 coeffs = alwan_hunter_coefficients_v(xyz_n);
    alwan_scalar ka = coeffs.v[0];
    alwan_scalar kb = coeffs.v[1];

    alwan_scalar y_ratio = xyz.y / xyz_n.y;
    alwan_scalar sqrt_y_ratio = ALWAN_SQRT(y_ratio);

    /* Division-by-zero guard */
    alwan_scalar inv_sqrt = ALWAN_SELECT(sqrt_y_ratio < ALWAN_LITERAL(1e-10),
                                         ALWAN_LITERAL(0.0),
                                         ALWAN_LITERAL(1.0) / sqrt_y_ratio);
    alwan_scalar safe = ALWAN_SELECT(sqrt_y_ratio < ALWAN_LITERAL(1e-10),
                                     ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

    result.L = ALWAN_LITERAL(10.0) * sqrt_y_ratio * safe;

    alwan_scalar x_ratio = xyz.x / xyz_n.x;
    alwan_scalar z_ratio = xyz.z / xyz_n.z;

    result.a = ka * (x_ratio - y_ratio) * inv_sqrt * safe;
    result.b = kb * (y_ratio - z_ratio) * inv_sqrt * safe;

    return result;
}

/* ----------------------------------------------------------------
 * Hunter Lab -> XYZ, custom illuminant (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_xyz alwan_hunter_lab_to_xyz_custom_v(alwan_hunter_lab hl, alwan_xyz xyz_n) {
    alwan_xyz result;

    alwan_vec2 coeffs = alwan_hunter_coefficients_v(xyz_n);
    alwan_scalar ka = coeffs.v[0];
    alwan_scalar kb = coeffs.v[1];

    alwan_scalar l_norm = hl.L / ALWAN_LITERAL(10.0);

    result.y = l_norm * l_norm * xyz_n.y;

    alwan_scalar a_term = (hl.a / ka) * l_norm;
    result.x = (a_term + l_norm * l_norm) * xyz_n.x;

    alwan_scalar b_term = (hl.b / kb) * l_norm;
    result.z = -(b_term - l_norm * l_norm) * xyz_n.z;

    return result;
}

#endif /* ALWAN_HUNTER_LAB_CORE_H */
