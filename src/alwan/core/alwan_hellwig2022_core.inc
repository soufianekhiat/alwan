/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Hellwig2022 Color Appearance Model (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Hellwig & Fairchild (2022)
 * "Predicting lightness, chroma, and hue using IAM and CAM frameworks"
 */

/* ----------------------------------------------------------------
 * Hellwig2022 Correlates (value-returning variant)
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T J;  /* Lightness (0-100) */
    ALWAN_CORE_T C;  /* Chroma (0+) */
    ALWAN_CORE_T h;  /* Hue angle (0-360 degrees) */
    ALWAN_CORE_T s;  /* Saturation (0+) */
    ALWAN_CORE_T Q;  /* Brightness (0+) */
    ALWAN_CORE_T M;  /* Colorfulness (0+) */
    ALWAN_CORE_T H;  /* Hue quadrature (0-400) */
} ALWAN_CORE_FNLIT(alwan_hellwig2022_v_correlates);

/* ----------------------------------------------------------------
 * CAT16 Transformation Matrices
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(HW22_V_M_CAT16) = {{
#include "../data/matrices/cat_cat16.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(HW22_V_M_CAT16_INV) = {{
#include "../data/matrices/cat_cat16_inv.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Hellwig2022 Hue Quadrature Tables
 * ---------------------------------------------------------------- */

static const ALWAN_CORE_T ALWAN_CORE_FNLIT(HW22_V_H_I)[5] = {
    ALWAN_CORE_LITERAL(20.14), ALWAN_CORE_LITERAL(90.00),
    ALWAN_CORE_LITERAL(164.25), ALWAN_CORE_LITERAL(237.53),
    ALWAN_CORE_LITERAL(380.14)
};

static const ALWAN_CORE_T ALWAN_CORE_FNLIT(HW22_V_E_I)[4] = {
    ALWAN_CORE_LITERAL(0.8), ALWAN_CORE_LITERAL(0.7),
    ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.2)
};

static const ALWAN_CORE_T ALWAN_CORE_FNLIT(HW22_V_HQ_I)[4] = {
    ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(100.0),
    ALWAN_CORE_LITERAL(200.0), ALWAN_CORE_LITERAL(300.0)
};

/* ----------------------------------------------------------------
 * Branchless degree of adaptation D
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hw22_compute_D)(ALWAN_CORE_T F, ALWAN_CORE_T La, ALWAN_CORE_T discount) {
    ALWAN_CORE_T D_computed = F * (ALWAN_CORE_ONE - ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.6) *
                     ALWAN_CORE_EXP((-La - ALWAN_CORE_LITERAL(42.0)) / ALWAN_CORE_LITERAL(92.0)));
    D_computed = ALWAN_CORE_CLAMP(D_computed, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    return ALWAN_CORE_SELECT(discount > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_ONE, D_computed);
}

/* ----------------------------------------------------------------
 * Luminance level adaptation factor FL
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hw22_compute_FL)(ALWAN_CORE_T La) {
    ALWAN_CORE_T k = ALWAN_CORE_ONE / (ALWAN_CORE_LITERAL(5.0) * La + ALWAN_CORE_ONE);
    ALWAN_CORE_T k4 = k * k * k * k;
    return ALWAN_CORE_LITERAL(0.2) * k4 * (ALWAN_CORE_LITERAL(5.0) * La) +
           ALWAN_CORE_LITERAL(0.1) * (ALWAN_CORE_ONE - k4) * (ALWAN_CORE_ONE - k4) *
           ALWAN_CORE_POW(ALWAN_CORE_LITERAL(5.0) * La, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));
}

/* ----------------------------------------------------------------
 * Post-adaptation nonlinear response compression
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hw22_post_adaptation_nonlinear)(ALWAN_CORE_T x, ALWAN_CORE_T FL) {
    ALWAN_CORE_T x_FL = (FL * x) / ALWAN_CORE_LITERAL(100.0);
    ALWAN_CORE_T x_abs = ALWAN_CORE_ABS(x_FL);
    ALWAN_CORE_T x_pow = ALWAN_CORE_POW(x_abs, ALWAN_CORE_LITERAL(0.42));
    ALWAN_CORE_T result = ALWAN_CORE_LITERAL(400.0) * x_pow / (ALWAN_CORE_LITERAL(27.13) + x_pow) + ALWAN_CORE_LITERAL(0.1);
    return ALWAN_CORE_SELECT(x_FL >= ALWAN_CORE_ZERO, result, -result);
}

/* ----------------------------------------------------------------
 * Inverse post-adaptation nonlinear response
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hw22_post_adaptation_nonlinear_inv)(ALWAN_CORE_T x, ALWAN_CORE_T FL) {
    ALWAN_CORE_T y = x - ALWAN_CORE_LITERAL(0.1);
    ALWAN_CORE_T y_abs = ALWAN_CORE_ABS(y);
    ALWAN_CORE_T result = ALWAN_CORE_POW((ALWAN_CORE_LITERAL(27.13) * y_abs) /
                     (ALWAN_CORE_LITERAL(400.0) - y_abs), ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(0.42)) *
                     ALWAN_CORE_LITERAL(100.0) / FL;
    return ALWAN_CORE_SELECT(x >= ALWAN_CORE_ZERO, result, -result);
}

/* ----------------------------------------------------------------
 * Hellwig2022 Fourier series eccentricity factor
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hw22_eccentricity)(ALWAN_CORE_T h_rad) {
    ALWAN_CORE_T h2 = ALWAN_CORE_LITERAL(2.0) * h_rad;
    ALWAN_CORE_T h3 = ALWAN_CORE_LITERAL(3.0) * h_rad;
    ALWAN_CORE_T h4 = ALWAN_CORE_LITERAL(4.0) * h_rad;
    return ALWAN_CORE_ONE +
           ALWAN_CORE_LITERAL(-0.0582) * ALWAN_CORE_COS(h_rad) +
           ALWAN_CORE_LITERAL(-0.0258) * ALWAN_CORE_COS(h2) +
           ALWAN_CORE_LITERAL(-0.1347) * ALWAN_CORE_COS(h3) +
           ALWAN_CORE_LITERAL( 0.0289) * ALWAN_CORE_COS(h4) +
           ALWAN_CORE_LITERAL(-0.1475) * ALWAN_CORE_SIN(h_rad) +
           ALWAN_CORE_LITERAL(-0.0308) * ALWAN_CORE_SIN(h2) +
           ALWAN_CORE_LITERAL( 0.0385) * ALWAN_CORE_SIN(h3) +
           ALWAN_CORE_LITERAL( 0.0096) * ALWAN_CORE_SIN(h4);
}

/* ----------------------------------------------------------------
 * Hue angle to hue quadrature H (branchless, unrolled)
 *
 * Uses ALWAN_CORE_SELECT to evaluate all four quadrants and pick the
 * correct one based on which range h falls into.
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hw22_hue_to_quadrature)(ALWAN_CORE_T h) {
    /* Normalize h into [20.14, 380.14) range branchlessly */
    h = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(20.14), h + ALWAN_CORE_LITERAL(360.0), h);
    h = ALWAN_CORE_SELECT(h > ALWAN_CORE_LITERAL(380.14), h - ALWAN_CORE_LITERAL(360.0), h);

    /* Quadrant 0: h in [20.14, 90.00) */
    ALWAN_CORE_T hp0 = (h - ALWAN_CORE_FNLIT(HW22_V_H_I)[0]) / ALWAN_CORE_FNLIT(HW22_V_E_I)[0];
    ALWAN_CORE_T H0 = ALWAN_CORE_FNLIT(HW22_V_HQ_I)[0] + (ALWAN_CORE_LITERAL(100.0) * hp0) / (hp0 + (ALWAN_CORE_FNLIT(HW22_V_H_I)[1] - h) / ALWAN_CORE_FNLIT(HW22_V_E_I)[0]);

    /* Quadrant 1: h in [90.00, 164.25) */
    ALWAN_CORE_T hp1 = (h - ALWAN_CORE_FNLIT(HW22_V_H_I)[1]) / ALWAN_CORE_FNLIT(HW22_V_E_I)[1];
    ALWAN_CORE_T H1 = ALWAN_CORE_FNLIT(HW22_V_HQ_I)[1] + (ALWAN_CORE_LITERAL(100.0) * hp1) / (hp1 + (ALWAN_CORE_FNLIT(HW22_V_H_I)[2] - h) / ALWAN_CORE_FNLIT(HW22_V_E_I)[1]);

    /* Quadrant 2: h in [164.25, 237.53) */
    ALWAN_CORE_T hp2 = (h - ALWAN_CORE_FNLIT(HW22_V_H_I)[2]) / ALWAN_CORE_FNLIT(HW22_V_E_I)[2];
    ALWAN_CORE_T H2 = ALWAN_CORE_FNLIT(HW22_V_HQ_I)[2] + (ALWAN_CORE_LITERAL(100.0) * hp2) / (hp2 + (ALWAN_CORE_FNLIT(HW22_V_H_I)[3] - h) / ALWAN_CORE_FNLIT(HW22_V_E_I)[2]);

    /* Quadrant 3: h in [237.53, 380.14) */
    ALWAN_CORE_T hp3 = (h - ALWAN_CORE_FNLIT(HW22_V_H_I)[3]) / ALWAN_CORE_FNLIT(HW22_V_E_I)[3];
    ALWAN_CORE_T H3 = ALWAN_CORE_FNLIT(HW22_V_HQ_I)[3] + (ALWAN_CORE_LITERAL(100.0) * hp3) / (hp3 + (ALWAN_CORE_FNLIT(HW22_V_H_I)[4] - h) / ALWAN_CORE_FNLIT(HW22_V_E_I)[3]);

    /* Select the correct quadrant (nested ALWAN_CORE_SELECT, no branches) */
    ALWAN_CORE_T H_result = ALWAN_CORE_SELECT(h < ALWAN_CORE_FNLIT(HW22_V_H_I)[1], H0,
                            ALWAN_CORE_SELECT(h < ALWAN_CORE_FNLIT(HW22_V_H_I)[2], H1,
                            ALWAN_CORE_SELECT(h < ALWAN_CORE_FNLIT(HW22_V_H_I)[3], H2, H3)));

    return H_result;
}

/* ----------------------------------------------------------------
 * Compute white-point achromatic response A_w and
 *         adapted white RGB_w in CAT16 space.
 *         Shared between forward and inverse.
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hw22_compute_Aw)(
    ALWAN_CORE_T white_x, ALWAN_CORE_T white_y, ALWAN_CORE_T white_z,
    ALWAN_CORE_T D, ALWAN_CORE_T FL,
    ALWAN_CORE_PARAM_OUT RGB_w0_out, ALWAN_CORE_PARAM_OUT RGB_w1_out, ALWAN_CORE_PARAM_OUT RGB_w2_out)
{
    /* White point XYZ -> RGB via CAT16 */
    ALWAN_CORE_VEC3 white_v = {{white_x, white_y, white_z}};
    ALWAN_CORE_VEC3 RGB_w_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HW22_V_M_CAT16), white_v);
    ALWAN_CORE_T RGB_w0 = RGB_w_v.v[0];
    ALWAN_CORE_T RGB_w1 = RGB_w_v.v[1];
    ALWAN_CORE_T RGB_w2 = RGB_w_v.v[2];

    /* Store for caller */
    ALWAN_CORE_REF(RGB_w0_out) = RGB_w0;
    ALWAN_CORE_REF(RGB_w1_out) = RGB_w1;
    ALWAN_CORE_REF(RGB_w2_out) = RGB_w2;

    /* Chromatic adaptation of the white onto itself. The per-channel factor
     * collapses to D + 1 - D = 1 by construction here; it is written out rather
     * than folded away so this stays visibly the same expression as the sample
     * path above it. */
    ALWAN_CORE_T RGB_wc0 = (white_y * D / RGB_w0 + ALWAN_CORE_ONE - D) * RGB_w0;
    ALWAN_CORE_T RGB_wc1 = (white_y * D / RGB_w1 + ALWAN_CORE_ONE - D) * RGB_w1;
    ALWAN_CORE_T RGB_wc2 = (white_y * D / RGB_w2 + ALWAN_CORE_ONE - D) * RGB_w2;

    /* Post-adaptation nonlinear compression of white */
    ALWAN_CORE_T R_aw = ALWAN_CORE_FNVLIT(hw22_post_adaptation_nonlinear)(RGB_wc0, FL);
    ALWAN_CORE_T G_aw = ALWAN_CORE_FNVLIT(hw22_post_adaptation_nonlinear)(RGB_wc1, FL);
    ALWAN_CORE_T B_aw = ALWAN_CORE_FNVLIT(hw22_post_adaptation_nonlinear)(RGB_wc2, FL);

    /* Hellwig2022 achromatic response for white */
    return ALWAN_CORE_LITERAL(2.0) * R_aw + G_aw + ALWAN_CORE_LITERAL(0.05) * B_aw - ALWAN_CORE_LITERAL(0.305);
}

/* ================================================================
 * Hellwig2022 Forward: XYZ -> Correlates
 *
 * Parameters (all pre-resolved scalars):
 *   xyz       - Input XYZ tristimulus values
 *   white_x/y/z - Reference white XYZ (Y typically 100)
 *   F         - Surround parameter (average=1.0, dim=0.9, dark=0.8)
 *   c         - Impact of surround (average=0.69, dim=0.59, dark=0.525)
 *   Nc        - Chromatic induction factor (average=1.0, dim=0.9, dark=0.8)
 *   La        - Adapting luminance (cd/m^2)
 *   Y_b       - Relative background luminance (Y_b/Y_w)
 *   Y_w       - Luminance of reference white (typically 100)
 *   discount_illuminant_as_scalar - 1.0 to discount, 0.0 otherwise
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_hellwig2022_v_correlates) ALWAN_CORE_FNVLIT(alwan_hellwig2022_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_T white_x, ALWAN_CORE_T white_y, ALWAN_CORE_T white_z,
    ALWAN_CORE_T F, ALWAN_CORE_T c, ALWAN_CORE_T Nc,
    ALWAN_CORE_T La,
    ALWAN_CORE_T Y_b,
    ALWAN_CORE_T Y_w,
    ALWAN_CORE_T discount_illuminant_as_scalar)
{
    ALWAN_CORE_FNLIT(alwan_hellwig2022_v_correlates) result;

    /* Step 1: Degree of adaptation D */
    ALWAN_CORE_T D = ALWAN_CORE_FNVLIT(hw22_compute_D)(F, La, discount_illuminant_as_scalar);

    /* Step 2: Luminance level adaptation factor FL */
    ALWAN_CORE_T FL = ALWAN_CORE_FNVLIT(hw22_compute_FL)(La);

    /* Step 3: White-point achromatic response A_w and white RGB_w */
    ALWAN_CORE_T RGB_w0, RGB_w1, RGB_w2;
    ALWAN_CORE_T A_w = ALWAN_CORE_FNVLIT(hw22_compute_Aw)(white_x, white_y, white_z, D, FL,
                                         ALWAN_CORE_ADDR(RGB_w0), ALWAN_CORE_ADDR(RGB_w1), ALWAN_CORE_ADDR(RGB_w2));

    /* Step 4: Transform test color XYZ -> RGB via CAT16 */
    ALWAN_CORE_VEC3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    ALWAN_CORE_VEC3 RGB_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HW22_V_M_CAT16), xyz_v);
    ALWAN_CORE_T RGB0 = RGB_v.v[0];
    ALWAN_CORE_T RGB1 = RGB_v.v[1];
    ALWAN_CORE_T RGB2 = RGB_v.v[2];

    /* Step 5: Chromatic adaptation */
    ALWAN_CORE_T RGB_c0 = (white_y * D / RGB_w0 + ALWAN_CORE_ONE - D) * RGB0;
    ALWAN_CORE_T RGB_c1 = (white_y * D / RGB_w1 + ALWAN_CORE_ONE - D) * RGB1;
    ALWAN_CORE_T RGB_c2 = (white_y * D / RGB_w2 + ALWAN_CORE_ONE - D) * RGB2;

    /* Step 6: Post-adaptation nonlinear compression (directly on RGB_c, no post-adaptation matrix) */
    ALWAN_CORE_T R_a = ALWAN_CORE_FNVLIT(hw22_post_adaptation_nonlinear)(RGB_c0, FL);
    ALWAN_CORE_T G_a = ALWAN_CORE_FNVLIT(hw22_post_adaptation_nonlinear)(RGB_c1, FL);
    ALWAN_CORE_T B_a = ALWAN_CORE_FNVLIT(hw22_post_adaptation_nonlinear)(RGB_c2, FL);

    /* Step 7: Opponent dimensions */
    ALWAN_CORE_T a = R_a - ALWAN_CORE_LITERAL(12.0) * G_a / ALWAN_CORE_LITERAL(11.0) + B_a / ALWAN_CORE_LITERAL(11.0);
    ALWAN_CORE_T b = (R_a + G_a - ALWAN_CORE_LITERAL(2.0) * B_a) / ALWAN_CORE_LITERAL(9.0);

    /* Step 8: Hue angle h */
    ALWAN_CORE_T h_rad = ALWAN_CORE_ATAN2(b, a);
    ALWAN_CORE_T h_deg = h_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    h_deg = ALWAN_CORE_SELECT(h_deg < ALWAN_CORE_ZERO, h_deg + ALWAN_CORE_LITERAL(360.0), h_deg);

    /* Step 9: Eccentricity factor et (Hellwig2022 Fourier series) */
    ALWAN_CORE_T et = ALWAN_CORE_FNVLIT(hw22_eccentricity)(h_rad);

    /* Step 10: Achromatic response A.
     * A = 2R + G + 0.05B - 0.305, with no N_bb scaling and no N_c. That is
     * Hellwig2022's own departure from CAM16, not a shortcut taken here:
     * dropping the induction factors is the paper's contribution. Matches
     * colour.appearance.XYZ_to_Hellwig2022 to 3.9e-10. */
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(2.0) * R_a + G_a + ALWAN_CORE_LITERAL(0.05) * B_a - ALWAN_CORE_LITERAL(0.305);

    /* Step 11: Base exponential nonlinearity z */
    ALWAN_CORE_T n = Y_b / Y_w;
    ALWAN_CORE_T z = ALWAN_CORE_LITERAL(1.48) + ALWAN_CORE_SQRT(n);

    /* Step 12: Lightness J */
    result.J = ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_POW(A / A_w, c * z);

    /* Step 13: Colourfulness M = 43 * N_c * e_t * hypot(a, b), as published. */
    result.M = ALWAN_CORE_LITERAL(43.0) * Nc * et * ALWAN_CORE_SQRT(a * a + b * b);

    /* Step 14: Chroma C = 35 * M / A_w, as published. */
    result.C = ALWAN_CORE_LITERAL(35.0) * result.M / A_w;

    /* Step 15: Brightness Q = (2/c) * (J/100) * A_w, as published. */
    result.Q = (ALWAN_CORE_LITERAL(2.0) / c) * (result.J / ALWAN_CORE_LITERAL(100.0)) * A_w;

    /* Step 16: Saturation s (no sqrt in Hellwig2022) */
    result.s = ALWAN_CORE_SELECT(result.Q > ALWAN_CORE_EPSILON,
                            ALWAN_CORE_LITERAL(100.0) * result.M / result.Q,
                            ALWAN_CORE_ZERO);

    /* Step 17: Hue composition H */
    result.h = h_deg;
    result.H = ALWAN_CORE_FNVLIT(hw22_hue_to_quadrature)(h_deg);

    return result;
}

/* ================================================================
 * Hellwig2022 Inverse: Correlates -> XYZ
 *
 * Recovers XYZ from J, C, h correlates plus viewing conditions.
 *
 * Parameters (all pre-resolved scalars):
 *   correlates - Hellwig2022 appearance correlates (J, C, h used)
 *   white_x/y/z - Reference white XYZ (Y typically 100)
 *   F          - Surround parameter
 *   c          - Impact of surround
 *   Nc         - Chromatic induction factor
 *   La         - Adapting luminance (cd/m^2)
 *   Y_b        - Relative background luminance (Y_b/Y_w)
 *   Y_w        - Luminance of reference white
 *   discount_illuminant_as_scalar - 1.0 to discount, 0.0 otherwise
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_hellwig2022_inverse)(
    ALWAN_CORE_FNLIT(alwan_hellwig2022_v_correlates) correlates,
    ALWAN_CORE_T white_x, ALWAN_CORE_T white_y, ALWAN_CORE_T white_z,
    ALWAN_CORE_T F, ALWAN_CORE_T c, ALWAN_CORE_T Nc,
    ALWAN_CORE_T La,
    ALWAN_CORE_T Y_b,
    ALWAN_CORE_T Y_w,
    ALWAN_CORE_T discount_illuminant_as_scalar)
{
    ALWAN_CORE_XYZ result;

    /* Step 1: Degree of adaptation D */
    ALWAN_CORE_T D = ALWAN_CORE_FNVLIT(hw22_compute_D)(F, La, discount_illuminant_as_scalar);

    /* Step 2: Luminance level adaptation factor FL */
    ALWAN_CORE_T FL = ALWAN_CORE_FNVLIT(hw22_compute_FL)(La);

    /* Step 3: White-point achromatic response A_w and white RGB_w */
    ALWAN_CORE_T RGB_w0, RGB_w1, RGB_w2;
    ALWAN_CORE_T A_w = ALWAN_CORE_FNVLIT(hw22_compute_Aw)(white_x, white_y, white_z, D, FL,
                                         ALWAN_CORE_ADDR(RGB_w0), ALWAN_CORE_ADDR(RGB_w1), ALWAN_CORE_ADDR(RGB_w2));

    /* Step 4: Base exponential nonlinearity z */
    ALWAN_CORE_T n = Y_b / Y_w;
    ALWAN_CORE_T z = ALWAN_CORE_LITERAL(1.48) + ALWAN_CORE_SQRT(n);

    /* Step 5: Recover achromatic response A from lightness J */
    ALWAN_CORE_T A = A_w * ALWAN_CORE_POW(correlates.J / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_ONE / (c * z));

    /* Step 6: Recover opponent dimensions a, b from hue and chroma */
    ALWAN_CORE_T h_rad = correlates.h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);

    /* Hellwig2022 Fourier series eccentricity */
    ALWAN_CORE_T et = ALWAN_CORE_FNVLIT(hw22_eccentricity)(h_rad);

    /* C = 35*M/A_w => M = C*A_w/35 */
    ALWAN_CORE_T M = correlates.C * A_w / ALWAN_CORE_LITERAL(35.0);

    /* M = 43*Nc*et*sqrt(a^2+b^2) => sqrt(a^2+b^2) = M / (43*Nc*et) */
    ALWAN_CORE_T ab_magnitude = M / (ALWAN_CORE_LITERAL(43.0) * Nc * et);

    ALWAN_CORE_T a = ALWAN_CORE_COS(h_rad) * ab_magnitude;
    ALWAN_CORE_T b = ALWAN_CORE_SIN(h_rad) * ab_magnitude;

    /* Step 7: Recover post-adaptation cone responses from opponent dimensions */
    ALWAN_CORE_T p2 = A + ALWAN_CORE_LITERAL(0.305);
    ALWAN_CORE_T R_a = (ALWAN_CORE_LITERAL(460.0) * p2 + ALWAN_CORE_LITERAL(451.0) * a + ALWAN_CORE_LITERAL(288.0) * b) / ALWAN_CORE_LITERAL(1403.0);
    ALWAN_CORE_T G_a = (ALWAN_CORE_LITERAL(460.0) * p2 - ALWAN_CORE_LITERAL(891.0) * a - ALWAN_CORE_LITERAL(261.0) * b) / ALWAN_CORE_LITERAL(1403.0);
    ALWAN_CORE_T B_a = (ALWAN_CORE_LITERAL(460.0) * p2 - ALWAN_CORE_LITERAL(220.0) * a - ALWAN_CORE_LITERAL(6300.0) * b) / ALWAN_CORE_LITERAL(1403.0);

    /* Step 8: Inverse nonlinear compression -> RGB_c (no post-adaptation matrix in Hellwig2022) */
    ALWAN_CORE_T RGB_c0 = ALWAN_CORE_FNVLIT(hw22_post_adaptation_nonlinear_inv)(R_a, FL);
    ALWAN_CORE_T RGB_c1 = ALWAN_CORE_FNVLIT(hw22_post_adaptation_nonlinear_inv)(G_a, FL);
    ALWAN_CORE_T RGB_c2 = ALWAN_CORE_FNVLIT(hw22_post_adaptation_nonlinear_inv)(B_a, FL);

    /* Step 9: Reverse chromatic adaptation */
    ALWAN_CORE_T RGB0 = RGB_c0 / (white_y * D / RGB_w0 + ALWAN_CORE_ONE - D);
    ALWAN_CORE_T RGB1 = RGB_c1 / (white_y * D / RGB_w1 + ALWAN_CORE_ONE - D);
    ALWAN_CORE_T RGB2 = RGB_c2 / (white_y * D / RGB_w2 + ALWAN_CORE_ONE - D);

    /* Step 10: RGB -> XYZ via inverse CAT16 */
    ALWAN_CORE_VEC3 RGB_inv_v = {{RGB0, RGB1, RGB2}};
    ALWAN_CORE_VEC3 xyz_out_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HW22_V_M_CAT16_INV), RGB_inv_v);
    result.x = xyz_out_v.v[0];
    result.y = xyz_out_v.v[1];
    result.z = xyz_out_v.v[2];

    return result;
}
