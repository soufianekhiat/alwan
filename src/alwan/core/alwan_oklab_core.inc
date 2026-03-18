/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Oklab & Oklch Color Spaces (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Bjorn Ottosson (2020)
 */

/* ----------------------------------------------------------------
 * Oklab Transformation Matrices (suffixed per precision)
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(OKLAB_M1) = {{
#include "../data/matrices/oklab_m1.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(OKLAB_M2) = {{
#include "../data/matrices/oklab_m2.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(OKLAB_M1_INV) = {{
#include "../data/matrices/oklab_m1_inv.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(OKLAB_M2_INV) = {{
#include "../data/matrices/oklab_m2_inv.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * XYZ -> Oklab (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_OKLAB ALWAN_CORE_FNVLIT(alwan_xyz_to_oklab)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_OKLAB result;

    /* Step 1: XYZ -> LMS via M1 matrix */
    ALWAN_CORE_VEC3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    ALWAN_CORE_VEC3 lms_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(OKLAB_M1), xyz_v);
    ALWAN_CORE_T l = lms_v.v[0];
    ALWAN_CORE_T m = lms_v.v[1];
    ALWAN_CORE_T s = lms_v.v[2];

    /* Step 2: LMS -> LMS' via cube root */
    ALWAN_CORE_T l_ = ALWAN_CORE_CBRT(l);
    ALWAN_CORE_T m_ = ALWAN_CORE_CBRT(m);
    ALWAN_CORE_T s_ = ALWAN_CORE_CBRT(s);

    /* Step 3: LMS' -> Lab via M2 matrix */
    ALWAN_CORE_VEC3 lms_p_v = {{l_, m_, s_}};
    ALWAN_CORE_VEC3 lab_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(OKLAB_M2), lms_p_v);
    result.L = lab_v.v[0];
    result.a = lab_v.v[1];
    result.b = lab_v.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * Oklab -> XYZ (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_oklab_to_xyz)(ALWAN_CORE_OKLAB oklab) {
    ALWAN_CORE_XYZ result;

    /* Step 1: Lab -> LMS' via M2_INV matrix */
    ALWAN_CORE_VEC3 lab_v = {{oklab.L, oklab.a, oklab.b}};
    ALWAN_CORE_VEC3 lms_p_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(OKLAB_M2_INV), lab_v);
    ALWAN_CORE_T l_ = lms_p_v.v[0];
    ALWAN_CORE_T m_ = lms_p_v.v[1];
    ALWAN_CORE_T s_ = lms_p_v.v[2];

    /* Step 2: LMS' -> LMS via cube */
    ALWAN_CORE_T l = l_ * l_ * l_;
    ALWAN_CORE_T m = m_ * m_ * m_;
    ALWAN_CORE_T s = s_ * s_ * s_;

    /* Step 3: LMS -> XYZ via M1_INV matrix */
    ALWAN_CORE_VEC3 lms_v = {{l, m, s}};
    ALWAN_CORE_VEC3 xyz_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(OKLAB_M1_INV), lms_v);
    result.x = xyz_v.v[0];
    result.y = xyz_v.v[1];
    result.z = xyz_v.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * Oklab -> Oklch (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_OKLCH ALWAN_CORE_FNVLIT(alwan_oklab_to_oklch)(ALWAN_CORE_OKLAB oklab) {
    ALWAN_CORE_OKLCH result;
    result.L = oklab.L;
    result.C = ALWAN_CORE_SQRT(oklab.a * oklab.a + oklab.b * oklab.b);
    result.h = ALWAN_CORE_ATAN2(oklab.b, oklab.a);
    return result;
}

/* ----------------------------------------------------------------
 * Oklch -> Oklab (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_OKLAB ALWAN_CORE_FNVLIT(alwan_oklch_to_oklab)(ALWAN_CORE_OKLCH oklch) {
    ALWAN_CORE_OKLAB result;
    result.L = oklch.L;
    result.a = oklch.C * ALWAN_CORE_COS(oklch.h);
    result.b = oklch.C * ALWAN_CORE_SIN(oklch.h);
    return result;
}
