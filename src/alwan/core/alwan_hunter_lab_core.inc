/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Hunter Lab Color Space (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Hunter (1948), ASTM D 1535
 */

/* ----------------------------------------------------------------
 * Hunter Lab Constants (precision-specific)
 * ---------------------------------------------------------------- */

#define ALWAN_HUNTER_KA_XN_REF_  ALWAN_CORE_LITERAL(98.043)
#define ALWAN_HUNTER_KB_ZN_REF_  ALWAN_CORE_LITERAL(118.115)
#define ALWAN_HUNTER_KA_D65_     ALWAN_CORE_LITERAL(172.30)
#define ALWAN_HUNTER_KB_D65_     ALWAN_CORE_LITERAL(67.20)
#define ALWAN_HUNTER_D65_XN_     ALWAN_CORE_LITERAL(95.02)
#define ALWAN_HUNTER_D65_YN_     ALWAN_CORE_LITERAL(100.0)
#define ALWAN_HUNTER_D65_ZN_     ALWAN_CORE_LITERAL(108.82)

/* ----------------------------------------------------------------
 * Calculate Ka and Kb for custom illuminant (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(alwan_hunter_coefficients)(ALWAN_CORE_XYZ xyz_n) {
    ALWAN_CORE_VEC2 result;
    /* Ka = 175 * sqrt(Xn / 98.043) */
    result.v[0] = ALWAN_CORE_LITERAL(175.0) * ALWAN_CORE_SQRT(xyz_n.x / ALWAN_HUNTER_KA_XN_REF_);
    /* Kb = 70 * sqrt(Zn / 118.115) */
    result.v[1] = ALWAN_CORE_LITERAL(70.0) * ALWAN_CORE_SQRT(xyz_n.z / ALWAN_HUNTER_KB_ZN_REF_);
    return result;
}

/* ----------------------------------------------------------------
 * XYZ -> Hunter Lab, D65 (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_HUNTER_LAB ALWAN_CORE_FNVLIT(alwan_xyz_to_hunter_lab)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_HUNTER_LAB result;

    ALWAN_CORE_T y_ratio = xyz.y / ALWAN_HUNTER_D65_YN_;
    ALWAN_CORE_T sqrt_y_ratio = ALWAN_CORE_SQRT(y_ratio);

    /* Division-by-zero guard */
    ALWAN_CORE_T safe = ALWAN_CORE_SELECT(sqrt_y_ratio < ALWAN_CORE_LITERAL(1e-10),
                                     ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));

    result.L = ALWAN_CORE_LITERAL(100.0) * sqrt_y_ratio * safe;

    ALWAN_CORE_T x_ratio = xyz.x / ALWAN_HUNTER_D65_XN_;
    ALWAN_CORE_T z_ratio = xyz.z / ALWAN_HUNTER_D65_ZN_;

    /* Use safe to zero out when sqrt_y_ratio is too small */
    ALWAN_CORE_T inv_sqrt = ALWAN_CORE_SELECT(sqrt_y_ratio < ALWAN_CORE_LITERAL(1e-10),
                                         ALWAN_CORE_LITERAL(0.0),
                                         ALWAN_CORE_LITERAL(1.0) / sqrt_y_ratio);
    result.a = ALWAN_HUNTER_KA_D65_ * (x_ratio - y_ratio) * inv_sqrt * safe;
    result.b = ALWAN_HUNTER_KB_D65_ * (y_ratio - z_ratio) * inv_sqrt * safe;

    return result;
}

/* ----------------------------------------------------------------
 * Hunter Lab -> XYZ, D65 (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_hunter_lab_to_xyz)(ALWAN_CORE_HUNTER_LAB hl) {
    ALWAN_CORE_XYZ result;

    ALWAN_CORE_T l_norm = hl.L / ALWAN_CORE_LITERAL(100.0);

    result.y = l_norm * l_norm * ALWAN_HUNTER_D65_YN_;

    ALWAN_CORE_T a_term = (hl.a / ALWAN_HUNTER_KA_D65_) * l_norm;
    result.x = (a_term + l_norm * l_norm) * ALWAN_HUNTER_D65_XN_;

    ALWAN_CORE_T b_term = (hl.b / ALWAN_HUNTER_KB_D65_) * l_norm;
    result.z = -(b_term - l_norm * l_norm) * ALWAN_HUNTER_D65_ZN_;

    return result;
}

/* ----------------------------------------------------------------
 * XYZ -> Hunter Lab, custom illuminant (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_HUNTER_LAB ALWAN_CORE_FNVLIT(alwan_xyz_to_hunter_lab_custom)(ALWAN_CORE_XYZ xyz, ALWAN_CORE_XYZ xyz_n) {
    ALWAN_CORE_HUNTER_LAB result;

    ALWAN_CORE_VEC2 coeffs = ALWAN_CORE_FNVLIT(alwan_hunter_coefficients)(xyz_n);
    ALWAN_CORE_T ka = coeffs.v[0];
    ALWAN_CORE_T kb = coeffs.v[1];

    ALWAN_CORE_T y_ratio = xyz.y / xyz_n.y;
    ALWAN_CORE_T sqrt_y_ratio = ALWAN_CORE_SQRT(y_ratio);

    /* Division-by-zero guard */
    ALWAN_CORE_T inv_sqrt = ALWAN_CORE_SELECT(sqrt_y_ratio < ALWAN_CORE_LITERAL(1e-10),
                                         ALWAN_CORE_LITERAL(0.0),
                                         ALWAN_CORE_LITERAL(1.0) / sqrt_y_ratio);
    ALWAN_CORE_T safe = ALWAN_CORE_SELECT(sqrt_y_ratio < ALWAN_CORE_LITERAL(1e-10),
                                     ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));

    result.L = ALWAN_CORE_LITERAL(10.0) * sqrt_y_ratio * safe;

    ALWAN_CORE_T x_ratio = xyz.x / xyz_n.x;
    ALWAN_CORE_T z_ratio = xyz.z / xyz_n.z;

    result.a = ka * (x_ratio - y_ratio) * inv_sqrt * safe;
    result.b = kb * (y_ratio - z_ratio) * inv_sqrt * safe;

    return result;
}

/* ----------------------------------------------------------------
 * Hunter Lab -> XYZ, custom illuminant (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_hunter_lab_to_xyz_custom)(ALWAN_CORE_HUNTER_LAB hl, ALWAN_CORE_XYZ xyz_n) {
    ALWAN_CORE_XYZ result;

    ALWAN_CORE_VEC2 coeffs = ALWAN_CORE_FNVLIT(alwan_hunter_coefficients)(xyz_n);
    ALWAN_CORE_T ka = coeffs.v[0];
    ALWAN_CORE_T kb = coeffs.v[1];

    ALWAN_CORE_T l_norm = hl.L / ALWAN_CORE_LITERAL(10.0);

    result.y = l_norm * l_norm * xyz_n.y;

    ALWAN_CORE_T a_term = (hl.a / ka) * l_norm;
    result.x = (a_term + l_norm * l_norm) * xyz_n.x;

    ALWAN_CORE_T b_term = (hl.b / kb) * l_norm;
    result.z = -(b_term - l_norm * l_norm) * xyz_n.z;

    return result;
}

#undef ALWAN_HUNTER_KA_XN_REF_
#undef ALWAN_HUNTER_KB_ZN_REF_
#undef ALWAN_HUNTER_KA_D65_
#undef ALWAN_HUNTER_KB_D65_
#undef ALWAN_HUNTER_D65_XN_
#undef ALWAN_HUNTER_D65_YN_
#undef ALWAN_HUNTER_D65_ZN_
