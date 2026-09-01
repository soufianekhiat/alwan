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
    ALWAN_CORE_T D,
    ALWAN_CORE_T Y_n)
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

    /* Step 2: Chromatic adaptation.
     *
     * RLAB (Fairchild 1996):
     *     l_E  = 3 * LMS_n / sum(LMS_n)
     *     p    = (1 + Y_n^(1/3) + l_E) / (1 + Y_n^(1/3) + 1/l_E)
     *     LMS_a = (p + D * (1 - p)) / LMS_n
     *
     * Two things were wrong until 2026-08-27.
     *
     * The normalisation divided by Y_n/Y_w instead of by LMS_n, so the
     * reference space came out scaled by the white's Y rather than normalised
     * to 1. On the documented Y=100 domain that multiplied L, a, b and C by
     * Y_w^sigma: the reference white returned L = 740.57 instead of 100, which
     * is exactly 100^(1/2.3) too large for an Average surround.
     *
     * The incomplete-adaptation term p was absent; adaptation was blended
     * linearly toward "none" with (1 - D). Note that at D = 1, which is the
     * Hard Copy default, p cancels out of (p + D*(1-p)) completely and only the
     * normalisation matters, so the default path is now exact.
     *
     * Y_n, the ABSOLUTE adapting luminance in cd/m^2, is not in
     * alwan_rlab_viewing_conditions, so p is evaluated at the RLAB reference
     * value of 318.31 cd/m^2 used in Fairchild's worked examples. That is
     * correct at D = 1 and an approximation below it, where the true answer
     * depends on Y_n. Adding the field is an ABI change and is tracked in
     * before_public.md. */
        /* A zero Y_n means "unset"; fall back to the RLAB reference viewing
     * condition rather than producing a cube root of zero. */
    const ALWAN_CORE_T Y_n_used = ALWAN_CORE_SELECT(Y_n > ALWAN_CORE_LITERAL(0.0), Y_n, ALWAN_CORE_LITERAL(318.31));
    const ALWAN_CORE_T y_n_cbrt = ALWAN_CORE_POW(Y_n_used, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));
    const ALWAN_CORE_T lms_w_sum = lms_w0 + lms_w1 + lms_w2;
    const ALWAN_CORE_T lms_w_sum_safe = ALWAN_CORE_SELECT(
        ALWAN_CORE_ABS(lms_w_sum) > ALWAN_CORE_LITERAL(1e-10), lms_w_sum, ALWAN_CORE_ONE);

    const ALWAN_CORE_T l_E0 = ALWAN_CORE_LITERAL(3.0) * lms_w0 / lms_w_sum_safe;
    const ALWAN_CORE_T l_E1 = ALWAN_CORE_LITERAL(3.0) * lms_w1 / lms_w_sum_safe;
    const ALWAN_CORE_T l_E2 = ALWAN_CORE_LITERAL(3.0) * lms_w2 / lms_w_sum_safe;

    const ALWAN_CORE_T p0 = (ALWAN_CORE_ONE + y_n_cbrt + l_E0) /
                            (ALWAN_CORE_ONE + y_n_cbrt + ALWAN_CORE_ONE / l_E0);
    const ALWAN_CORE_T p1 = (ALWAN_CORE_ONE + y_n_cbrt + l_E1) /
                            (ALWAN_CORE_ONE + y_n_cbrt + ALWAN_CORE_ONE / l_E1);
    const ALWAN_CORE_T p2 = (ALWAN_CORE_ONE + y_n_cbrt + l_E2) /
                            (ALWAN_CORE_ONE + y_n_cbrt + ALWAN_CORE_ONE / l_E2);

    ALWAN_CORE_T adapt_0 = ALWAN_CORE_SELECT(lms_w0 > ALWAN_CORE_LITERAL(1e-10),
        (p0 + D * (ALWAN_CORE_ONE - p0)) / lms_w0, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T lms_a0 = lms_0 * adapt_0;

    ALWAN_CORE_T adapt_1 = ALWAN_CORE_SELECT(lms_w1 > ALWAN_CORE_LITERAL(1e-10),
        (p1 + D * (ALWAN_CORE_ONE - p1)) / lms_w1, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T lms_a1 = lms_1 * adapt_1;

    ALWAN_CORE_T adapt_2 = ALWAN_CORE_SELECT(lms_w2 > ALWAN_CORE_LITERAL(1e-10),
        (p2 + D * (ALWAN_CORE_ONE - p2)) / lms_w2, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T lms_a2 = lms_2 * adapt_2;

    ALWAN_UNUSED(xyz_n);

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
    ALWAN_CORE_T D,
    ALWAN_CORE_T Y_n)
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

    /* Step 5: Inverse chromatic adaptation.
     *
     * Must stay the exact inverse of the forward: LMS = LMS_a / adapt, with the
     * same adapt = (p + D*(1 - p)) / LMS_n. When only the forward was corrected
     * on 2026-08-27 the pair disagreed by the white's Y and the round-trip test
     * in 92_rlab_f32 failed with 0.5 coming back as 50, which is how the
     * omission was caught. */
        /* A zero Y_n means "unset"; fall back to the RLAB reference viewing
     * condition rather than producing a cube root of zero. */
    const ALWAN_CORE_T Y_n_used = ALWAN_CORE_SELECT(Y_n > ALWAN_CORE_LITERAL(0.0), Y_n, ALWAN_CORE_LITERAL(318.31));
    const ALWAN_CORE_T y_n_cbrt = ALWAN_CORE_POW(Y_n_used, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));
    const ALWAN_CORE_T lms_w_sum = lms_w0 + lms_w1 + lms_w2;
    const ALWAN_CORE_T lms_w_sum_safe = ALWAN_CORE_SELECT(
        ALWAN_CORE_ABS(lms_w_sum) > ALWAN_CORE_LITERAL(1e-10), lms_w_sum, ALWAN_CORE_ONE);

    const ALWAN_CORE_T l_E0 = ALWAN_CORE_LITERAL(3.0) * lms_w0 / lms_w_sum_safe;
    const ALWAN_CORE_T l_E1 = ALWAN_CORE_LITERAL(3.0) * lms_w1 / lms_w_sum_safe;
    const ALWAN_CORE_T l_E2 = ALWAN_CORE_LITERAL(3.0) * lms_w2 / lms_w_sum_safe;

    const ALWAN_CORE_T p0 = (ALWAN_CORE_ONE + y_n_cbrt + l_E0) /
                            (ALWAN_CORE_ONE + y_n_cbrt + ALWAN_CORE_ONE / l_E0);
    const ALWAN_CORE_T p1 = (ALWAN_CORE_ONE + y_n_cbrt + l_E1) /
                            (ALWAN_CORE_ONE + y_n_cbrt + ALWAN_CORE_ONE / l_E1);
    const ALWAN_CORE_T p2 = (ALWAN_CORE_ONE + y_n_cbrt + l_E2) /
                            (ALWAN_CORE_ONE + y_n_cbrt + ALWAN_CORE_ONE / l_E2);

    ALWAN_CORE_T adapt_0 = ALWAN_CORE_SELECT(lms_w0 > ALWAN_CORE_LITERAL(1e-10),
        (p0 + D * (ALWAN_CORE_ONE - p0)) / lms_w0, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T lms_0 = lms_a0 / adapt_0;

    ALWAN_CORE_T adapt_1 = ALWAN_CORE_SELECT(lms_w1 > ALWAN_CORE_LITERAL(1e-10),
        (p1 + D * (ALWAN_CORE_ONE - p1)) / lms_w1, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T lms_1 = lms_a1 / adapt_1;

    ALWAN_CORE_T adapt_2 = ALWAN_CORE_SELECT(lms_w2 > ALWAN_CORE_LITERAL(1e-10),
        (p2 + D * (ALWAN_CORE_ONE - p2)) / lms_w2, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T lms_2 = lms_a2 / adapt_2;

    ALWAN_UNUSED(xyz_n);

    /* Step 6: LMS -> XYZ using HPE inverse */
    ALWAN_CORE_VEC3 lms_v = {{lms_0, lms_1, lms_2}};
    ALWAN_CORE_VEC3 xyz_out_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(RLAB_M_HPE_INV), lms_v);
    result.x = xyz_out_v.v[0]; result.y = xyz_out_v.v[1]; result.z = xyz_out_v.v[2];

    return result;
}
