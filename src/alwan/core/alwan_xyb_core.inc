/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only XYB (JPEG XL) (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: JPEG XL white paper; constants as ColorAide carries them.
 */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(XYB_LRGB_TO_LMS) = {{
#include "../data/matrices/xyb_lrgb_to_lms.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(XYB_LMS_TO_LRGB) = {{
#include "../data/matrices/xyb_lms_to_lrgb.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(XYB_XYZ_TO_LMS) = {{
#include "../data/matrices/xyb_xyz_to_lms.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(XYB_LMS_TO_XYZ) = {{
#include "../data/matrices/xyb_lms_to_xyz.csv"
}};
/* the bias, and its cube root */
ALWAN_CONSTEXPR ALWAN_CORE_T ALWAN_CORE_FNLIT(XYB_BIAS)[2] = {
#include "../data/xyb_bias.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * LMS -> XYB: biased cube root, then X = (L' - M') / 2,
 * Y = (L' + M') / 2, B = S' - Y
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYB ALWAN_CORE_FNVLIT(alwan_xyb_from_lms)(ALWAN_CORE_VEC3 lms) {
    ALWAN_CORE_XYB result;
    ALWAN_CORE_T l_ = ALWAN_CORE_CBRT(lms.v[0] + ALWAN_CORE_FNLIT(XYB_BIAS)[0]) - ALWAN_CORE_FNLIT(XYB_BIAS)[1];
    ALWAN_CORE_T m_ = ALWAN_CORE_CBRT(lms.v[1] + ALWAN_CORE_FNLIT(XYB_BIAS)[0]) - ALWAN_CORE_FNLIT(XYB_BIAS)[1];
    ALWAN_CORE_T s_ = ALWAN_CORE_CBRT(lms.v[2] + ALWAN_CORE_FNLIT(XYB_BIAS)[0]) - ALWAN_CORE_FNLIT(XYB_BIAS)[1];
    result.x = ALWAN_CORE_LITERAL(0.5) * l_ - ALWAN_CORE_LITERAL(0.5) * m_;
    result.y = ALWAN_CORE_LITERAL(0.5) * l_ + ALWAN_CORE_LITERAL(0.5) * m_;
    result.b = s_ - result.y;
    return result;
}

/* ----------------------------------------------------------------
 * XYB -> LMS: the inverse of the above
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_xyb_to_lms)(ALWAN_CORE_XYB xyb) {
    ALWAN_CORE_VEC3 lms;
    ALWAN_CORE_T l_ = xyb.x + xyb.y + ALWAN_CORE_FNLIT(XYB_BIAS)[1];
    ALWAN_CORE_T m_ = xyb.y - xyb.x + ALWAN_CORE_FNLIT(XYB_BIAS)[1];
    ALWAN_CORE_T s_ = xyb.b + xyb.y + ALWAN_CORE_FNLIT(XYB_BIAS)[1];
    lms.v[0] = l_ * l_ * l_ - ALWAN_CORE_FNLIT(XYB_BIAS)[0];
    lms.v[1] = m_ * m_ * m_ - ALWAN_CORE_FNLIT(XYB_BIAS)[0];
    lms.v[2] = s_ * s_ * s_ - ALWAN_CORE_FNLIT(XYB_BIAS)[0];
    return lms;
}

/* ----------------------------------------------------------------
 * Linear sRGB <-> XYB
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYB ALWAN_CORE_FNVLIT(alwan_linear_srgb_to_xyb)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_VEC3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    return ALWAN_CORE_FNVLIT(alwan_xyb_from_lms)(ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(XYB_LRGB_TO_LMS), rgb_v));
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_xyb_to_linear_srgb)(ALWAN_CORE_XYB xyb) {
    ALWAN_CORE_RGB result;
    ALWAN_CORE_VEC3 rgb_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(XYB_LMS_TO_LRGB), ALWAN_CORE_FNVLIT(alwan_xyb_to_lms)(xyb));
    result.r = rgb_v.v[0];
    result.g = rgb_v.v[1];
    result.b = rgb_v.v[2];
    return result;
}

/* ----------------------------------------------------------------
 * XYZ (D65, Y = 1) <-> XYB, through linear sRGB folded into one matrix
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYB ALWAN_CORE_FNVLIT(alwan_xyz_to_xyb)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_VEC3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    return ALWAN_CORE_FNVLIT(alwan_xyb_from_lms)(ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(XYB_XYZ_TO_LMS), xyz_v));
}

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_xyb_to_xyz)(ALWAN_CORE_XYB xyb) {
    ALWAN_CORE_XYZ result;
    ALWAN_CORE_VEC3 xyz_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(XYB_LMS_TO_XYZ), ALWAN_CORE_FNVLIT(alwan_xyb_to_lms)(xyb));
    result.x = xyz_v.v[0];
    result.y = xyz_v.v[1];
    result.z = xyz_v.v[2];
    return result;
}
