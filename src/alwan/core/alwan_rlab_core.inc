/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only RLAB Color Appearance Model (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Fairchild (1993, 1996)
 * "RLAB: a color appearance space for color reproduction"
 * "Refinement of the RLAB color space"
 */

/* ----------------------------------------------------------------
 * RLAB Correlates (suffixed per precision)
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T L, a, b, h, C, s;
} ALWAN_CORE_FNLIT(alwan_rlab_v_correlates);

/* ----------------------------------------------------------------
 * RLAB Constants
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(RLAB_M_HPE) = {{
#include "../data/matrices/hpe.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(RLAB_M_HPE_INV) = {{
#include "../data/matrices/hpe_inv.csv"
}};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(RLAB_M_RLAB) = {{
#include "../data/matrices/rlab_m.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(RLAB_M_RLAB_INV) = {{
#include "../data/matrices/rlab_m_inv.csv"
}};
ALWAN_DIAG_POP

/* ================================================================
 * RLAB Forward: XYZ -> Correlates
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_rlab_v_correlates) ALWAN_CORE_FNVLIT(alwan_rlab_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_XYZ xyz_w,
    ALWAN_CORE_XYZ xyz_n,
    ALWAN_CORE_T sigma,
    ALWAN_CORE_T D)
{
    ALWAN_CORE_FNLIT(alwan_rlab_v_correlates) result;

    /* Step 1: XYZ -> LMS using HPE matrix */
    ALWAN_CORE_VEC3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    ALWAN_CORE_VEC3 lms_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(RLAB_M_HPE), xyz_v);
    ALWAN_CORE_T lms_0 = lms_v.v[0]; ALWAN_CORE_T lms_1 = lms_v.v[1]; ALWAN_CORE_T lms_2 = lms_v.v[2];

    /* White point XYZ -> LMS_w */
    ALWAN_CORE_VEC3 xyz_w_v = {{xyz_w.x, xyz_w.y, xyz_w.z}};
    ALWAN_CORE_VEC3 lms_w_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(RLAB_M_HPE), xyz_w_v);
    ALWAN_CORE_T lms_w0 = lms_w_v.v[0]; ALWAN_CORE_T lms_w1 = lms_w_v.v[1]; ALWAN_CORE_T lms_w2 = lms_w_v.v[2];

    /* Step 2: Chromatic adaptation */
    ALWAN_CORE_T Y_Yw = xyz_n.y / xyz_w.y;

    ALWAN_CORE_T adapt_0 = ALWAN_CORE_SELECT(lms_w0 > ALWAN_CORE_LITERAL(1e-10),
        (D * (xyz_n.y / lms_w0) + ALWAN_CORE_LITERAL(1.0) - D) / Y_Yw,
        ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T lms_a0 = lms_0 * adapt_0;

    ALWAN_CORE_T adapt_1 = ALWAN_CORE_SELECT(lms_w1 > ALWAN_CORE_LITERAL(1e-10),
        (D * (xyz_n.y / lms_w1) + ALWAN_CORE_LITERAL(1.0) - D) / Y_Yw,
        ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T lms_a1 = lms_1 * adapt_1;

    ALWAN_CORE_T adapt_2 = ALWAN_CORE_SELECT(lms_w2 > ALWAN_CORE_LITERAL(1e-10),
        (D * (xyz_n.y / lms_w2) + ALWAN_CORE_LITERAL(1.0) - D) / Y_Yw,
        ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T lms_a2 = lms_2 * adapt_2;

    /* Step 3: LMS_adapted -> RLAB reference space */
    ALWAN_CORE_VEC3 lms_a_v = {{lms_a0, lms_a1, lms_a2}};
    ALWAN_CORE_VEC3 xyz_ref_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(RLAB_M_RLAB), lms_a_v);
    ALWAN_CORE_T xyz_ref_0 = xyz_ref_v.v[0]; ALWAN_CORE_T xyz_ref_1 = xyz_ref_v.v[1]; ALWAN_CORE_T xyz_ref_2 = xyz_ref_v.v[2];

    /* Clamp negatives */
    xyz_ref_0 = ALWAN_CORE_SELECT(xyz_ref_0 < ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0), xyz_ref_0);
    xyz_ref_1 = ALWAN_CORE_SELECT(xyz_ref_1 < ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0), xyz_ref_1);
    xyz_ref_2 = ALWAN_CORE_SELECT(xyz_ref_2 < ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0), xyz_ref_2);

    /* Step 4: Apply power nonlinearity */
    ALWAN_CORE_T xyz_ref_sigma_0 = ALWAN_CORE_POW(xyz_ref_0, sigma);
    ALWAN_CORE_T xyz_ref_sigma_1 = ALWAN_CORE_POW(xyz_ref_1, sigma);
    ALWAN_CORE_T xyz_ref_sigma_2 = ALWAN_CORE_POW(xyz_ref_2, sigma);

    /* Step 5: Compute L */
    result.L = ALWAN_CORE_LITERAL(100.0) * xyz_ref_sigma_1;

    /* Step 6: Compute opponent dimensions a, b */
    result.a = ALWAN_CORE_LITERAL(430.0) * (xyz_ref_sigma_0 - xyz_ref_sigma_1);
    result.b = ALWAN_CORE_LITERAL(170.0) * (xyz_ref_sigma_1 - xyz_ref_sigma_2);

    /* Step 7: Hue angle h (degrees, [0, 360)) */
    ALWAN_CORE_T h_deg = ALWAN_CORE_ATAN2(result.b, result.a) * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    result.h = ALWAN_CORE_SELECT(h_deg < ALWAN_CORE_LITERAL(0.0), h_deg + ALWAN_CORE_LITERAL(360.0), h_deg);

    /* Step 8: Chroma C */
    result.C = ALWAN_CORE_SQRT(result.a * result.a + result.b * result.b);

    /* Step 9: Saturation s */
    result.s = ALWAN_CORE_SELECT(result.L > ALWAN_CORE_LITERAL(1e-10), result.C / result.L, ALWAN_CORE_LITERAL(0.0));

    return result;
}

/* ================================================================
 * RLAB Inverse: Correlates -> XYZ
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_rlab_inverse)(
    ALWAN_CORE_FNLIT(alwan_rlab_v_correlates) correlates,
    ALWAN_CORE_XYZ xyz_w,
    ALWAN_CORE_XYZ xyz_n,
    ALWAN_CORE_T sigma,
    ALWAN_CORE_T D)
{
    ALWAN_CORE_XYZ result;

    /* Step 1: Recover xyz_ref_sigma from L, a, b */
    ALWAN_CORE_T xyz_ref_sigma_1 = correlates.L / ALWAN_CORE_LITERAL(100.0);
    ALWAN_CORE_T xyz_ref_sigma_0 = xyz_ref_sigma_1 + correlates.a / ALWAN_CORE_LITERAL(430.0);
    ALWAN_CORE_T xyz_ref_sigma_2 = xyz_ref_sigma_1 - correlates.b / ALWAN_CORE_LITERAL(170.0);

    /* Step 2: Apply inverse power nonlinearity */
    ALWAN_CORE_T inv_sigma = ALWAN_CORE_LITERAL(1.0) / sigma;
    ALWAN_CORE_T xyz_ref_0 = ALWAN_CORE_POW(ALWAN_CORE_ABS(xyz_ref_sigma_0), inv_sigma);
    ALWAN_CORE_T xyz_ref_1 = ALWAN_CORE_POW(ALWAN_CORE_ABS(xyz_ref_sigma_1), inv_sigma);
    ALWAN_CORE_T xyz_ref_2 = ALWAN_CORE_POW(ALWAN_CORE_ABS(xyz_ref_sigma_2), inv_sigma);

    /* Step 3: RLAB_INV matrix * xyz_ref -> lms_adapted */
    ALWAN_CORE_VEC3 xyz_ref_v = {{xyz_ref_0, xyz_ref_1, xyz_ref_2}};
    ALWAN_CORE_VEC3 lms_a_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(RLAB_M_RLAB_INV), xyz_ref_v);
    ALWAN_CORE_T lms_a0 = lms_a_v.v[0]; ALWAN_CORE_T lms_a1 = lms_a_v.v[1]; ALWAN_CORE_T lms_a2 = lms_a_v.v[2];

    /* Step 4: White point XYZ -> LMS_w */
    ALWAN_CORE_VEC3 xyz_w_v = {{xyz_w.x, xyz_w.y, xyz_w.z}};
    ALWAN_CORE_VEC3 lms_w_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(RLAB_M_HPE), xyz_w_v);
    ALWAN_CORE_T lms_w0 = lms_w_v.v[0]; ALWAN_CORE_T lms_w1 = lms_w_v.v[1]; ALWAN_CORE_T lms_w2 = lms_w_v.v[2];

    /* Step 5: Inverse chromatic adaptation */
    ALWAN_CORE_T Y_Yw = xyz_n.y / xyz_w.y;

    ALWAN_CORE_T adapt_0 = ALWAN_CORE_SELECT(lms_w0 > ALWAN_CORE_LITERAL(1e-10),
        (D * (xyz_n.y / lms_w0) + ALWAN_CORE_LITERAL(1.0) - D) / Y_Yw,
        ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T lms_0 = lms_a0 / adapt_0;

    ALWAN_CORE_T adapt_1 = ALWAN_CORE_SELECT(lms_w1 > ALWAN_CORE_LITERAL(1e-10),
        (D * (xyz_n.y / lms_w1) + ALWAN_CORE_LITERAL(1.0) - D) / Y_Yw,
        ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T lms_1 = lms_a1 / adapt_1;

    ALWAN_CORE_T adapt_2 = ALWAN_CORE_SELECT(lms_w2 > ALWAN_CORE_LITERAL(1e-10),
        (D * (xyz_n.y / lms_w2) + ALWAN_CORE_LITERAL(1.0) - D) / Y_Yw,
        ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T lms_2 = lms_a2 / adapt_2;

    /* Step 6: LMS -> XYZ using HPE inverse */
    ALWAN_CORE_VEC3 lms_v = {{lms_0, lms_1, lms_2}};
    ALWAN_CORE_VEC3 xyz_out_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(RLAB_M_HPE_INV), lms_v);
    result.x = xyz_out_v.v[0]; result.y = xyz_out_v.v[1]; result.z = xyz_out_v.v[2];

    return result;
}
