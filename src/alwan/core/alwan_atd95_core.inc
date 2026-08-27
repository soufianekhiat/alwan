/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ATD95 Color Vision Model (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * References:
 * - Guth, S. L. (1995). Further applications of the ATD model for color vision.
 *   Proc. SPIE 2414, Device-Independent Color Imaging II.
 * - Fairchild, M. D. (2013). Color Appearance Models (3rd ed.). Wiley.
 */

/* ----------------------------------------------------------------
 * ATD95 Output Type
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T H, C, Br;
    ALWAN_CORE_T A_1, T_1, D_1;
    ALWAN_CORE_T A_2, T_2, D_2;
} ALWAN_CORE_FNLIT(alwan_atd95_v_correlates);

/* ----------------------------------------------------------------
 * ATD95 Constants
 * ---------------------------------------------------------------- */

/* XYZ to LMS transformation matrix coefficients */
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_LMS_L_X) = ALWAN_CORE_LITERAL(0.2435);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_LMS_L_Y) = ALWAN_CORE_LITERAL(0.8524);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_LMS_L_Z) = ALWAN_CORE_LITERAL(-0.0516);

static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_LMS_M_X) = ALWAN_CORE_LITERAL(-0.3954);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_LMS_M_Y) = ALWAN_CORE_LITERAL(1.1642);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_LMS_M_Z) = ALWAN_CORE_LITERAL(0.0837);

static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_LMS_S_Y) = ALWAN_CORE_LITERAL(0.04);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_LMS_S_Z) = ALWAN_CORE_LITERAL(0.6225);

/* Cone response parameters */
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_L_SCALE)  = ALWAN_CORE_LITERAL(0.66);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_L_OFFSET) = ALWAN_CORE_LITERAL(0.024);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_L_EXP)    = ALWAN_CORE_LITERAL(0.7);

static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_M_OFFSET) = ALWAN_CORE_LITERAL(0.036);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_M_EXP)    = ALWAN_CORE_LITERAL(0.7);

static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_S_SCALE)  = ALWAN_CORE_LITERAL(0.43);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_S_OFFSET) = ALWAN_CORE_LITERAL(0.31);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_S_EXP)    = ALWAN_CORE_LITERAL(0.7);

/* Retinal illuminance conversion */
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_RETINAL_SCALE) = ALWAN_CORE_LITERAL(18.0);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_RETINAL_EXP)   = ALWAN_CORE_LITERAL(0.8);

/* Opponent response coefficients */
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_A1_L) = ALWAN_CORE_LITERAL(3.57);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_A1_M) = ALWAN_CORE_LITERAL(2.64);

static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_T1_L) = ALWAN_CORE_LITERAL(7.18);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_T1_M) = ALWAN_CORE_LITERAL(-6.21);

static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_D1_L) = ALWAN_CORE_LITERAL(-0.7);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_D1_M) = ALWAN_CORE_LITERAL(0.085);

static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_A2_SCALE) = ALWAN_CORE_LITERAL(0.09);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_T2_T)     = ALWAN_CORE_LITERAL(0.43);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ATD95_V_T2_D)     = ALWAN_CORE_LITERAL(0.76);

/* ----------------------------------------------------------------
 * Helper Functions
 * ---------------------------------------------------------------- */

/* Sign-preserving power response for cone channels.
 *
 * ATD95 applies the channel gain BEFORE the 0.7 power and adds the offset
 * AFTER it: spow(scale * x, 0.7) + offset, where spow is sign-preserving.
 *
 * Two things were wrong here until 2026-08-27. The gain sat outside the power,
 * scale * |x|^0.7 + offset, so the L channel was scaled by 0.66 where the model
 * scales by 0.66^0.7 = 0.7492 and S by 0.43 instead of 0.43^0.7 = 0.5533. And
 * the whole expression including the offset was negated for negative input, so
 * the offset came back with the wrong sign there; only the power is
 * sign-preserving, the offset is not. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(atd95_spow_response)(ALWAN_CORE_T linear, ALWAN_CORE_T scale, ALWAN_CORE_T offset, ALWAN_CORE_T exponent) {
    ALWAN_CORE_T scaled = scale * linear;
    ALWAN_CORE_T mag = ALWAN_CORE_POW(ALWAN_CORE_ABS(scaled), exponent);
    ALWAN_CORE_T signed_pow = ALWAN_CORE_SELECT(scaled < ALWAN_CORE_ZERO, -mag, mag);
    return signed_pow + offset;
}

/* Final response normalization: value / (200 + |value|) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(atd95_final_response)(ALWAN_CORE_T value) {
    return value / (ALWAN_CORE_LITERAL(200.0) + ALWAN_CORE_ABS(value));
}

/* ----------------------------------------------------------------
 * ATD95 Forward Transform (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_atd95_v_correlates) ALWAN_CORE_FNVLIT(alwan_atd95_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_XYZ white_xyz,
    ALWAN_CORE_T Y_0,
    ALWAN_CORE_T k1,
    ALWAN_CORE_T k2,
    ALWAN_CORE_T sigma
) {
    ALWAN_CORE_FNLIT(alwan_atd95_v_correlates) result;

    /* Step 1: Convert XYZ to retinal illuminance for sample and white */
    ALWAN_CORE_T X_r  = ALWAN_CORE_FNLIT(ATD95_V_RETINAL_SCALE) * ALWAN_CORE_POW(Y_0 * xyz.x / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_FNLIT(ATD95_V_RETINAL_EXP));
    ALWAN_CORE_T Y_r  = ALWAN_CORE_FNLIT(ATD95_V_RETINAL_SCALE) * ALWAN_CORE_POW(Y_0 * xyz.y / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_FNLIT(ATD95_V_RETINAL_EXP));
    ALWAN_CORE_T Z_r  = ALWAN_CORE_FNLIT(ATD95_V_RETINAL_SCALE) * ALWAN_CORE_POW(Y_0 * xyz.z / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_FNLIT(ATD95_V_RETINAL_EXP));

    ALWAN_CORE_T X_0r = ALWAN_CORE_FNLIT(ATD95_V_RETINAL_SCALE) * ALWAN_CORE_POW(Y_0 * white_xyz.x / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_FNLIT(ATD95_V_RETINAL_EXP));
    ALWAN_CORE_T Y_0r = ALWAN_CORE_FNLIT(ATD95_V_RETINAL_SCALE) * ALWAN_CORE_POW(Y_0 * white_xyz.y / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_FNLIT(ATD95_V_RETINAL_EXP));
    ALWAN_CORE_T Z_0r = ALWAN_CORE_FNLIT(ATD95_V_RETINAL_SCALE) * ALWAN_CORE_POW(Y_0 * white_xyz.z / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_FNLIT(ATD95_V_RETINAL_EXP));

    /* Step 2: Transform retinal XYZ to LMS cone responses (sample) */
    ALWAN_CORE_T L_linear = ALWAN_CORE_FNLIT(ATD95_V_LMS_L_X) * X_r + ALWAN_CORE_FNLIT(ATD95_V_LMS_L_Y) * Y_r + ALWAN_CORE_FNLIT(ATD95_V_LMS_L_Z) * Z_r;
    ALWAN_CORE_T M_linear = ALWAN_CORE_FNLIT(ATD95_V_LMS_M_X) * X_r + ALWAN_CORE_FNLIT(ATD95_V_LMS_M_Y) * Y_r + ALWAN_CORE_FNLIT(ATD95_V_LMS_M_Z) * Z_r;
    ALWAN_CORE_T S_linear = ALWAN_CORE_FNLIT(ATD95_V_LMS_S_Y) * Y_r + ALWAN_CORE_FNLIT(ATD95_V_LMS_S_Z) * Z_r;

    ALWAN_CORE_T L = ALWAN_CORE_FNVLIT(atd95_spow_response)(L_linear, ALWAN_CORE_FNLIT(ATD95_V_L_SCALE), ALWAN_CORE_FNLIT(ATD95_V_L_OFFSET), ALWAN_CORE_FNLIT(ATD95_V_L_EXP));
    ALWAN_CORE_T M = ALWAN_CORE_FNVLIT(atd95_spow_response)(M_linear, ALWAN_CORE_ONE,       ALWAN_CORE_FNLIT(ATD95_V_M_OFFSET), ALWAN_CORE_FNLIT(ATD95_V_M_EXP));
    ALWAN_CORE_T S = ALWAN_CORE_FNVLIT(atd95_spow_response)(S_linear, ALWAN_CORE_FNLIT(ATD95_V_S_SCALE), ALWAN_CORE_FNLIT(ATD95_V_S_OFFSET), ALWAN_CORE_FNLIT(ATD95_V_S_EXP));

    /* Step 2b: Transform retinal XYZ to LMS cone responses (white) */
    ALWAN_CORE_T L0_linear = ALWAN_CORE_FNLIT(ATD95_V_LMS_L_X) * X_0r + ALWAN_CORE_FNLIT(ATD95_V_LMS_L_Y) * Y_0r + ALWAN_CORE_FNLIT(ATD95_V_LMS_L_Z) * Z_0r;
    ALWAN_CORE_T M0_linear = ALWAN_CORE_FNLIT(ATD95_V_LMS_M_X) * X_0r + ALWAN_CORE_FNLIT(ATD95_V_LMS_M_Y) * Y_0r + ALWAN_CORE_FNLIT(ATD95_V_LMS_M_Z) * Z_0r;
    ALWAN_CORE_T S0_linear = ALWAN_CORE_FNLIT(ATD95_V_LMS_S_Y) * Y_0r + ALWAN_CORE_FNLIT(ATD95_V_LMS_S_Z) * Z_0r;

    ALWAN_CORE_T L_0 = ALWAN_CORE_FNVLIT(atd95_spow_response)(L0_linear, ALWAN_CORE_FNLIT(ATD95_V_L_SCALE), ALWAN_CORE_FNLIT(ATD95_V_L_OFFSET), ALWAN_CORE_FNLIT(ATD95_V_L_EXP));
    ALWAN_CORE_T M_0 = ALWAN_CORE_FNVLIT(atd95_spow_response)(M0_linear, ALWAN_CORE_ONE,       ALWAN_CORE_FNLIT(ATD95_V_M_OFFSET), ALWAN_CORE_FNLIT(ATD95_V_M_EXP));
    ALWAN_CORE_T S_0 = ALWAN_CORE_FNVLIT(atd95_spow_response)(S0_linear, ALWAN_CORE_FNLIT(ATD95_V_S_SCALE), ALWAN_CORE_FNLIT(ATD95_V_S_OFFSET), ALWAN_CORE_FNLIT(ATD95_V_S_EXP));
    /* White point adapted responses reserved for full adaptation model */
    ALWAN_UNUSED(L_0); ALWAN_UNUSED(M_0); ALWAN_UNUSED(S_0);

    /* Step 3: Compute adapted responses using k1, k2 */
    ALWAN_CORE_T X_ar = k1 * X_r + k2 * X_0r;
    ALWAN_CORE_T Y_ar = k1 * Y_r + k2 * Y_0r;
    ALWAN_CORE_T Z_ar = k1 * Z_r + k2 * Z_0r;

    /* Transform adapted retinal XYZ to LMS */
    ALWAN_CORE_T La_linear = ALWAN_CORE_FNLIT(ATD95_V_LMS_L_X) * X_ar + ALWAN_CORE_FNLIT(ATD95_V_LMS_L_Y) * Y_ar + ALWAN_CORE_FNLIT(ATD95_V_LMS_L_Z) * Z_ar;
    ALWAN_CORE_T Ma_linear = ALWAN_CORE_FNLIT(ATD95_V_LMS_M_X) * X_ar + ALWAN_CORE_FNLIT(ATD95_V_LMS_M_Y) * Y_ar + ALWAN_CORE_FNLIT(ATD95_V_LMS_M_Z) * Z_ar;
    ALWAN_CORE_T Sa_linear = ALWAN_CORE_FNLIT(ATD95_V_LMS_S_Y) * Y_ar + ALWAN_CORE_FNLIT(ATD95_V_LMS_S_Z) * Z_ar;

    ALWAN_CORE_T L_a = ALWAN_CORE_FNVLIT(atd95_spow_response)(La_linear, ALWAN_CORE_FNLIT(ATD95_V_L_SCALE), ALWAN_CORE_FNLIT(ATD95_V_L_OFFSET), ALWAN_CORE_FNLIT(ATD95_V_L_EXP));
    ALWAN_CORE_T M_a = ALWAN_CORE_FNVLIT(atd95_spow_response)(Ma_linear, ALWAN_CORE_ONE,       ALWAN_CORE_FNLIT(ATD95_V_M_OFFSET), ALWAN_CORE_FNLIT(ATD95_V_M_EXP));
    ALWAN_CORE_T S_a = ALWAN_CORE_FNVLIT(atd95_spow_response)(Sa_linear, ALWAN_CORE_FNLIT(ATD95_V_S_SCALE), ALWAN_CORE_FNLIT(ATD95_V_S_OFFSET), ALWAN_CORE_FNLIT(ATD95_V_S_EXP));

    /* Step 4: Apply gain normalization with sigma */
    ALWAN_CORE_T L_g = L * (sigma / (sigma + L_a));
    ALWAN_CORE_T M_g = M * (sigma / (sigma + M_a));
    ALWAN_CORE_T S_g = S * (sigma / (sigma + S_a));

    /* Step 5: Compute opponent color dimensions */
    ALWAN_CORE_T A_1i = ALWAN_CORE_FNLIT(ATD95_V_A1_L) * L_g + ALWAN_CORE_FNLIT(ATD95_V_A1_M) * M_g;
    ALWAN_CORE_T T_1i = ALWAN_CORE_FNLIT(ATD95_V_T1_L) * L_g + ALWAN_CORE_FNLIT(ATD95_V_T1_M) * M_g;
    ALWAN_CORE_T D_1i = ALWAN_CORE_FNLIT(ATD95_V_D1_L) * L_g + ALWAN_CORE_FNLIT(ATD95_V_D1_M) * M_g + S_g;

    ALWAN_CORE_T A_2i = ALWAN_CORE_FNLIT(ATD95_V_A2_SCALE) * A_1i;
    ALWAN_CORE_T T_2i = ALWAN_CORE_FNLIT(ATD95_V_T2_T) * T_1i + ALWAN_CORE_FNLIT(ATD95_V_T2_D) * D_1i;
    ALWAN_CORE_T D_2i = D_1i;

    /* Step 6: Apply final response function */
    ALWAN_CORE_T A_1 = ALWAN_CORE_FNVLIT(atd95_final_response)(A_1i);
    ALWAN_CORE_T T_1 = ALWAN_CORE_FNVLIT(atd95_final_response)(T_1i);
    ALWAN_CORE_T D_1 = ALWAN_CORE_FNVLIT(atd95_final_response)(D_1i);

    ALWAN_CORE_T A_2 = ALWAN_CORE_FNVLIT(atd95_final_response)(A_2i);
    ALWAN_CORE_T T_2 = ALWAN_CORE_FNVLIT(atd95_final_response)(T_2i);
    ALWAN_CORE_T D_2 = ALWAN_CORE_FNVLIT(atd95_final_response)(D_2i);

    /* Step 7: Compute appearance correlates */

    /* Brightness */
    ALWAN_CORE_T Br = ALWAN_CORE_SQRT(A_1 * A_1 + T_1 * T_1 + D_1 * D_1);

    /* Saturation */
    ALWAN_CORE_T abs_A2 = ALWAN_CORE_ABS(A_2);
    ALWAN_CORE_T C = ALWAN_CORE_SELECT(abs_A2 > ALWAN_CORE_LITERAL(1e-10),
                                  ALWAN_CORE_SQRT(T_2 * T_2 + D_2 * D_2) / A_2,
                                  ALWAN_CORE_ZERO);

    /* Hue.
     *
     * ATD95 defines H as the plain RATIO T_2 / D_2, not an angle. It is not
     * bounded to [0, 360) and the reference implementation does not wrap it.
     * Until 2026-08-27 this returned degrees(atan2(T_2, D_2)) wrapped into
     * [0, 360), which is a different quantity with a different range.
     *
     * D_2 is a final_response output, value / (200 + |value|), so it is zero
     * only when its input is exactly zero; guard rather than divide. */
    ALWAN_CORE_T H = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(D_2) > ALWAN_CORE_LITERAL(1e-12),
                                       T_2 / ALWAN_CORE_SELECT(ALWAN_CORE_ABS(D_2) > ALWAN_CORE_LITERAL(1e-12), D_2, ALWAN_CORE_ONE),
                                       ALWAN_CORE_ZERO);

    /* Output correlates */
    result.H   = H;
    result.C   = C;
    result.Br  = Br;
    result.A_1 = A_1;
    result.T_1 = T_1;
    result.D_1 = D_1;
    result.A_2 = A_2;
    result.T_2 = T_2;
    result.D_2 = D_2;

    return result;
}
