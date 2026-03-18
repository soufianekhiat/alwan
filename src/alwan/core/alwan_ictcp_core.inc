/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ICtCp Color Space (ITU-R BT.2100 HDR) (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: ITU-R Recommendation BT.2100-3 (02/2025)
 */

/* ----------------------------------------------------------------
 * ICtCp Transformation Matrices
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ICTCP_RGB_TO_LMS) = {{
#include "../data/ictcp_rgb_to_lms.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ICTCP_LMS_TO_RGB) = {{
#include "../data/ictcp_lms_to_rgb.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ICTCP_LMS_P_TO_ICTCP_PQ) = {{
#include "../data/ictcp_lms_p_to_ictcp_pq.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ICTCP_ICTCP_TO_LMS_P_PQ) = {{
#include "../data/ictcp_ictcp_to_lms_p_pq.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ICTCP_LMS_P_TO_ICTCP_HLG) = {{
#include "../data/ictcp_lms_p_to_ictcp_hlg.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ICTCP_ICTCP_TO_LMS_P_HLG) = {{
#include "../data/ictcp_ictcp_to_lms_p_hlg.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ICTCP_XYZ_TO_BT2020) = {{
#include "../data/ictcp_xyz_to_bt2020.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ICTCP_BT2020_TO_XYZ) = {{
#include "../data/ictcp_bt2020_to_xyz.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * HLG Inverse OETF (branchless)
 * Mathematical inverse of HLG OETF, NOT the same as HLG EOTF
 * (HLG EOTF includes system gamma 1.2)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_hlg_inverse_oetf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.17883277);
    ALWAN_CORE_T b = ALWAN_CORE_ONE - ALWAN_CORE_LITERAL(4.0) * a;
    ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.5) - a * ALWAN_CORE_LN(ALWAN_CORE_LITERAL(4.0) * a);

    ALWAN_CORE_T E = ALWAN_CORE_SELECT(encoded < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, encoded);
    ALWAN_CORE_T linear_lo = (E * E) / ALWAN_CORE_LITERAL(3.0);
    ALWAN_CORE_T linear_hi = (ALWAN_CORE_EXP((E - c) / a) + b) / ALWAN_CORE_LITERAL(12.0);
    return ALWAN_CORE_SELECT(E <= ALWAN_CORE_LITERAL(0.5), linear_lo, linear_hi);
}

/* ----------------------------------------------------------------
 * BT.2020 RGB -> ICtCp (PQ) (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_ICTCP ALWAN_CORE_FNVLIT(alwan_rgb_to_ictcp_pq)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_ICTCP result;

    /* Step 1: BT.2020 RGB (linear) -> LMS */
    ALWAN_CORE_VEC3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ICTCP_RGB_TO_LMS), rgb_v);
    ALWAN_CORE_T l = lms.v[0];
    ALWAN_CORE_T m = lms.v[1];
    ALWAN_CORE_T s = lms.v[2];

    /* Step 2: LMS -> LMS' via PQ OETF */
    ALWAN_CORE_T lp = ALWAN_CORE_FNLIT(alwan_pq_oetf)(l);
    ALWAN_CORE_T mp = ALWAN_CORE_FNLIT(alwan_pq_oetf)(m);
    ALWAN_CORE_T sp = ALWAN_CORE_FNLIT(alwan_pq_oetf)(s);

    /* Step 3: LMS' -> ICtCp via PQ matrix */
    ALWAN_CORE_VEC3 lms_p = {{lp, mp, sp}};
    ALWAN_CORE_VEC3 ictcp = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ICTCP_LMS_P_TO_ICTCP_PQ), lms_p);
    result.I  = ictcp.v[0];
    result.Ct = ictcp.v[1];
    result.Cp = ictcp.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * ICtCp (PQ) -> BT.2020 RGB (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_ictcp_pq_to_rgb)(ALWAN_CORE_ICTCP ictcp) {
    ALWAN_CORE_RGB result;

    /* Step 1: ICtCp -> LMS' via PQ inverse matrix */
    ALWAN_CORE_VEC3 ictcp_v = {{ictcp.I, ictcp.Ct, ictcp.Cp}};
    ALWAN_CORE_VEC3 lms_p = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ICTCP_ICTCP_TO_LMS_P_PQ), ictcp_v);
    ALWAN_CORE_T lp = lms_p.v[0];
    ALWAN_CORE_T mp = lms_p.v[1];
    ALWAN_CORE_T sp = lms_p.v[2];

    /* Step 2: LMS' -> LMS via PQ EOTF */
    ALWAN_CORE_T l = ALWAN_CORE_FNLIT(alwan_pq_eotf)(lp);
    ALWAN_CORE_T m = ALWAN_CORE_FNLIT(alwan_pq_eotf)(mp);
    ALWAN_CORE_T s = ALWAN_CORE_FNLIT(alwan_pq_eotf)(sp);

    /* Step 3: LMS -> BT.2020 RGB (linear) */
    ALWAN_CORE_VEC3 lms_v = {{l, m, s}};
    ALWAN_CORE_VEC3 rgb = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ICTCP_LMS_TO_RGB), lms_v);
    result.r = rgb.v[0];
    result.g = rgb.v[1];
    result.b = rgb.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * BT.2020 RGB -> ICtCp (HLG) (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_ICTCP ALWAN_CORE_FNVLIT(alwan_rgb_to_ictcp_hlg)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_ICTCP result;

    /* Step 1: BT.2020 RGB (linear) -> LMS */
    ALWAN_CORE_VEC3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ICTCP_RGB_TO_LMS), rgb_v);
    ALWAN_CORE_T l = lms.v[0];
    ALWAN_CORE_T m = lms.v[1];
    ALWAN_CORE_T s = lms.v[2];

    /* Step 2: LMS -> LMS' via HLG OETF */
    ALWAN_CORE_T lp = ALWAN_CORE_FNLIT(alwan_hlg_oetf)(l);
    ALWAN_CORE_T mp = ALWAN_CORE_FNLIT(alwan_hlg_oetf)(m);
    ALWAN_CORE_T sp = ALWAN_CORE_FNLIT(alwan_hlg_oetf)(s);

    /* Step 3: LMS' -> ICtCp via HLG matrix */
    ALWAN_CORE_VEC3 lms_p = {{lp, mp, sp}};
    ALWAN_CORE_VEC3 ictcp = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ICTCP_LMS_P_TO_ICTCP_HLG), lms_p);
    result.I  = ictcp.v[0];
    result.Ct = ictcp.v[1];
    result.Cp = ictcp.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * ICtCp (HLG) -> BT.2020 RGB (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_ictcp_hlg_to_rgb)(ALWAN_CORE_ICTCP ictcp) {
    ALWAN_CORE_RGB result;

    /* Step 1: ICtCp -> LMS' via HLG inverse matrix */
    ALWAN_CORE_VEC3 ictcp_v = {{ictcp.I, ictcp.Ct, ictcp.Cp}};
    ALWAN_CORE_VEC3 lms_p = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ICTCP_ICTCP_TO_LMS_P_HLG), ictcp_v);
    ALWAN_CORE_T lp = lms_p.v[0];
    ALWAN_CORE_T mp = lms_p.v[1];
    ALWAN_CORE_T sp = lms_p.v[2];

    /* Step 2: LMS' -> LMS via HLG inverse OETF (NOT EOTF - no system gamma) */
    ALWAN_CORE_T l = ALWAN_CORE_FNVLIT(alwan_hlg_inverse_oetf)(lp);
    ALWAN_CORE_T m = ALWAN_CORE_FNVLIT(alwan_hlg_inverse_oetf)(mp);
    ALWAN_CORE_T s = ALWAN_CORE_FNVLIT(alwan_hlg_inverse_oetf)(sp);

    /* Step 3: LMS -> BT.2020 RGB (linear) */
    ALWAN_CORE_VEC3 lms_v = {{l, m, s}};
    ALWAN_CORE_VEC3 rgb = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ICTCP_LMS_TO_RGB), lms_v);
    result.r = rgb.v[0];
    result.g = rgb.v[1];
    result.b = rgb.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * XYZ -> ICtCp (PQ) (value-returning, via BT.2020 RGB)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_ICTCP ALWAN_CORE_FNVLIT(alwan_xyz_to_ictcp_pq)(ALWAN_CORE_XYZ xyz) {
    /* Step 1: XYZ (D65) -> BT.2020 RGB (linear) */
    ALWAN_CORE_VEC3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    ALWAN_CORE_VEC3 rgb_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ICTCP_XYZ_TO_BT2020), xyz_v);
    ALWAN_CORE_RGB rgb;
    rgb.r = rgb_v.v[0];
    rgb.g = rgb_v.v[1];
    rgb.b = rgb_v.v[2];

    /* Step 2: BT.2020 RGB -> ICtCp (PQ) */
    return ALWAN_CORE_FNVLIT(alwan_rgb_to_ictcp_pq)(rgb);
}

/* ----------------------------------------------------------------
 * ICtCp (PQ) -> XYZ (value-returning, via BT.2020 RGB)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_ictcp_pq_to_xyz)(ALWAN_CORE_ICTCP ictcp) {
    ALWAN_CORE_XYZ result;

    /* Step 1: ICtCp (PQ) -> BT.2020 RGB (linear) */
    ALWAN_CORE_RGB rgb = ALWAN_CORE_FNVLIT(alwan_ictcp_pq_to_rgb)(ictcp);

    /* Step 2: BT.2020 RGB -> XYZ (D65) */
    ALWAN_CORE_VEC3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    ALWAN_CORE_VEC3 xyz = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ICTCP_BT2020_TO_XYZ), rgb_v);
    result.x = xyz.v[0];
    result.y = xyz.v[1];
    result.z = xyz.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * XYZ -> ICtCp (HLG) (value-returning, via BT.2020 RGB)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_ICTCP ALWAN_CORE_FNVLIT(alwan_xyz_to_ictcp_hlg)(ALWAN_CORE_XYZ xyz) {
    /* Step 1: XYZ (D65) -> BT.2020 RGB (linear) */
    ALWAN_CORE_VEC3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    ALWAN_CORE_VEC3 rgb_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ICTCP_XYZ_TO_BT2020), xyz_v);
    ALWAN_CORE_RGB rgb;
    rgb.r = rgb_v.v[0];
    rgb.g = rgb_v.v[1];
    rgb.b = rgb_v.v[2];

    /* Step 2: BT.2020 RGB -> ICtCp (HLG) */
    return ALWAN_CORE_FNVLIT(alwan_rgb_to_ictcp_hlg)(rgb);
}

/* ----------------------------------------------------------------
 * ICtCp (HLG) -> XYZ (value-returning, via BT.2020 RGB)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_ictcp_hlg_to_xyz)(ALWAN_CORE_ICTCP ictcp) {
    ALWAN_CORE_XYZ result;

    /* Step 1: ICtCp (HLG) -> BT.2020 RGB (linear) */
    ALWAN_CORE_RGB rgb = ALWAN_CORE_FNVLIT(alwan_ictcp_hlg_to_rgb)(ictcp);

    /* Step 2: BT.2020 RGB -> XYZ (D65) */
    ALWAN_CORE_VEC3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    ALWAN_CORE_VEC3 xyz = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ICTCP_BT2020_TO_XYZ), rgb_v);
    result.x = xyz.v[0];
    result.y = xyz.v[1];
    result.z = xyz.v[2];

    return result;
}
