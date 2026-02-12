/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ATD95 Color Vision Model
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * References:
 * - Guth, S. L. (1995). Further applications of the ATD model for color vision.
 *   Proc. SPIE 2414, Device-Independent Color Imaging II.
 * - Fairchild, M. D. (2013). Color Appearance Models (3rd ed.). Wiley.
 */

#ifndef ALWAN_ATD95_CORE_H
#define ALWAN_ATD95_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

/* ----------------------------------------------------------------
 * ATD95 Output Type
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_scalar H, C, Br;
    alwan_scalar A_1, T_1, D_1;
    alwan_scalar A_2, T_2, D_2;
} alwan_atd95_v_correlates;

/* ----------------------------------------------------------------
 * ATD95 Constants
 * ---------------------------------------------------------------- */

/* XYZ to LMS transformation matrix coefficients */
static alwan_scalar const ATD95_V_LMS_L_X = ALWAN_LITERAL(0.2435);
static alwan_scalar const ATD95_V_LMS_L_Y = ALWAN_LITERAL(0.8524);
static alwan_scalar const ATD95_V_LMS_L_Z = ALWAN_LITERAL(-0.0516);

static alwan_scalar const ATD95_V_LMS_M_X = ALWAN_LITERAL(-0.3954);
static alwan_scalar const ATD95_V_LMS_M_Y = ALWAN_LITERAL(1.1642);
static alwan_scalar const ATD95_V_LMS_M_Z = ALWAN_LITERAL(0.0837);

static alwan_scalar const ATD95_V_LMS_S_Y = ALWAN_LITERAL(0.04);
static alwan_scalar const ATD95_V_LMS_S_Z = ALWAN_LITERAL(0.6225);

/* Cone response parameters */
static alwan_scalar const ATD95_V_L_SCALE  = ALWAN_LITERAL(0.66);
static alwan_scalar const ATD95_V_L_OFFSET = ALWAN_LITERAL(0.024);
static alwan_scalar const ATD95_V_L_EXP    = ALWAN_LITERAL(0.7);

static alwan_scalar const ATD95_V_M_OFFSET = ALWAN_LITERAL(0.036);
static alwan_scalar const ATD95_V_M_EXP    = ALWAN_LITERAL(0.7);

static alwan_scalar const ATD95_V_S_SCALE  = ALWAN_LITERAL(0.43);
static alwan_scalar const ATD95_V_S_OFFSET = ALWAN_LITERAL(0.31);
static alwan_scalar const ATD95_V_S_EXP    = ALWAN_LITERAL(0.7);

/* Retinal illuminance conversion */
static alwan_scalar const ATD95_V_RETINAL_SCALE = ALWAN_LITERAL(18.0);
static alwan_scalar const ATD95_V_RETINAL_EXP   = ALWAN_LITERAL(0.8);

/* Opponent response coefficients */
static alwan_scalar const ATD95_V_A1_L = ALWAN_LITERAL(3.57);
static alwan_scalar const ATD95_V_A1_M = ALWAN_LITERAL(2.64);

static alwan_scalar const ATD95_V_T1_L = ALWAN_LITERAL(7.18);
static alwan_scalar const ATD95_V_T1_M = ALWAN_LITERAL(-6.21);

static alwan_scalar const ATD95_V_D1_L = ALWAN_LITERAL(-0.7);
static alwan_scalar const ATD95_V_D1_M = ALWAN_LITERAL(0.085);

static alwan_scalar const ATD95_V_A2_SCALE = ALWAN_LITERAL(0.09);
static alwan_scalar const ATD95_V_T2_T     = ALWAN_LITERAL(0.43);
static alwan_scalar const ATD95_V_T2_D     = ALWAN_LITERAL(0.76);

/* ----------------------------------------------------------------
 * Helper Functions
 * ---------------------------------------------------------------- */

/* Sign-preserving power response for cone channels */
ALWAN_INLINE alwan_scalar atd95_spow_response_v(alwan_scalar linear, alwan_scalar scale, alwan_scalar offset, alwan_scalar exponent) {
    alwan_scalar abs_lin = ALWAN_ABS(linear);
    alwan_scalar result = scale * ALWAN_POW(abs_lin, exponent) + offset;
    return ALWAN_SELECT(linear < ALWAN_ZERO, -result, result);
}

/* Final response normalization: value / (200 + |value|) */
ALWAN_INLINE alwan_scalar atd95_final_response_v(alwan_scalar value) {
    return value / (ALWAN_LITERAL(200.0) + ALWAN_ABS(value));
}

/* ----------------------------------------------------------------
 * ATD95 Forward Transform (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_atd95_v_correlates alwan_atd95_forward_v(
    alwan_xyz xyz,
    alwan_xyz white_xyz,
    alwan_scalar Y_0,
    alwan_scalar k1,
    alwan_scalar k2,
    alwan_scalar sigma
) {
    alwan_atd95_v_correlates result;

    /* Step 1: Convert XYZ to retinal illuminance for sample and white */
    alwan_scalar X_r  = ATD95_V_RETINAL_SCALE * ALWAN_POW(Y_0 * xyz.x / ALWAN_LITERAL(100.0), ATD95_V_RETINAL_EXP);
    alwan_scalar Y_r  = ATD95_V_RETINAL_SCALE * ALWAN_POW(Y_0 * xyz.y / ALWAN_LITERAL(100.0), ATD95_V_RETINAL_EXP);
    alwan_scalar Z_r  = ATD95_V_RETINAL_SCALE * ALWAN_POW(Y_0 * xyz.z / ALWAN_LITERAL(100.0), ATD95_V_RETINAL_EXP);

    alwan_scalar X_0r = ATD95_V_RETINAL_SCALE * ALWAN_POW(Y_0 * white_xyz.x / ALWAN_LITERAL(100.0), ATD95_V_RETINAL_EXP);
    alwan_scalar Y_0r = ATD95_V_RETINAL_SCALE * ALWAN_POW(Y_0 * white_xyz.y / ALWAN_LITERAL(100.0), ATD95_V_RETINAL_EXP);
    alwan_scalar Z_0r = ATD95_V_RETINAL_SCALE * ALWAN_POW(Y_0 * white_xyz.z / ALWAN_LITERAL(100.0), ATD95_V_RETINAL_EXP);

    /* Step 2: Transform retinal XYZ to LMS cone responses (sample) */
    alwan_scalar L_linear = ATD95_V_LMS_L_X * X_r + ATD95_V_LMS_L_Y * Y_r + ATD95_V_LMS_L_Z * Z_r;
    alwan_scalar M_linear = ATD95_V_LMS_M_X * X_r + ATD95_V_LMS_M_Y * Y_r + ATD95_V_LMS_M_Z * Z_r;
    alwan_scalar S_linear = ATD95_V_LMS_S_Y * Y_r + ATD95_V_LMS_S_Z * Z_r;

    alwan_scalar L = atd95_spow_response_v(L_linear, ATD95_V_L_SCALE, ATD95_V_L_OFFSET, ATD95_V_L_EXP);
    alwan_scalar M = atd95_spow_response_v(M_linear, ALWAN_ONE,       ATD95_V_M_OFFSET, ATD95_V_M_EXP);
    alwan_scalar S = atd95_spow_response_v(S_linear, ATD95_V_S_SCALE, ATD95_V_S_OFFSET, ATD95_V_S_EXP);

    /* Step 2b: Transform retinal XYZ to LMS cone responses (white) */
    alwan_scalar L0_linear = ATD95_V_LMS_L_X * X_0r + ATD95_V_LMS_L_Y * Y_0r + ATD95_V_LMS_L_Z * Z_0r;
    alwan_scalar M0_linear = ATD95_V_LMS_M_X * X_0r + ATD95_V_LMS_M_Y * Y_0r + ATD95_V_LMS_M_Z * Z_0r;
    alwan_scalar S0_linear = ATD95_V_LMS_S_Y * Y_0r + ATD95_V_LMS_S_Z * Z_0r;

    alwan_scalar L_0 = atd95_spow_response_v(L0_linear, ATD95_V_L_SCALE, ATD95_V_L_OFFSET, ATD95_V_L_EXP);
    alwan_scalar M_0 = atd95_spow_response_v(M0_linear, ALWAN_ONE,       ATD95_V_M_OFFSET, ATD95_V_M_EXP);
    alwan_scalar S_0 = atd95_spow_response_v(S0_linear, ATD95_V_S_SCALE, ATD95_V_S_OFFSET, ATD95_V_S_EXP);
    /* White point adapted responses reserved for full adaptation model */
    (void)L_0; (void)M_0; (void)S_0;

    /* Step 3: Compute adapted responses using k1, k2 */
    alwan_scalar X_ar = k1 * X_r + k2 * X_0r;
    alwan_scalar Y_ar = k1 * Y_r + k2 * Y_0r;
    alwan_scalar Z_ar = k1 * Z_r + k2 * Z_0r;

    /* Transform adapted retinal XYZ to LMS */
    alwan_scalar La_linear = ATD95_V_LMS_L_X * X_ar + ATD95_V_LMS_L_Y * Y_ar + ATD95_V_LMS_L_Z * Z_ar;
    alwan_scalar Ma_linear = ATD95_V_LMS_M_X * X_ar + ATD95_V_LMS_M_Y * Y_ar + ATD95_V_LMS_M_Z * Z_ar;
    alwan_scalar Sa_linear = ATD95_V_LMS_S_Y * Y_ar + ATD95_V_LMS_S_Z * Z_ar;

    alwan_scalar L_a = atd95_spow_response_v(La_linear, ATD95_V_L_SCALE, ATD95_V_L_OFFSET, ATD95_V_L_EXP);
    alwan_scalar M_a = atd95_spow_response_v(Ma_linear, ALWAN_ONE,       ATD95_V_M_OFFSET, ATD95_V_M_EXP);
    alwan_scalar S_a = atd95_spow_response_v(Sa_linear, ATD95_V_S_SCALE, ATD95_V_S_OFFSET, ATD95_V_S_EXP);

    /* Step 4: Apply gain normalization with sigma */
    alwan_scalar L_g = L * (sigma / (sigma + L_a));
    alwan_scalar M_g = M * (sigma / (sigma + M_a));
    alwan_scalar S_g = S * (sigma / (sigma + S_a));

    /* Step 5: Compute opponent color dimensions */
    alwan_scalar A_1i = ATD95_V_A1_L * L_g + ATD95_V_A1_M * M_g;
    alwan_scalar T_1i = ATD95_V_T1_L * L_g + ATD95_V_T1_M * M_g;
    alwan_scalar D_1i = ATD95_V_D1_L * L_g + ATD95_V_D1_M * M_g + S_g;

    alwan_scalar A_2i = ATD95_V_A2_SCALE * A_1i;
    alwan_scalar T_2i = ATD95_V_T2_T * T_1i + ATD95_V_T2_D * D_1i;
    alwan_scalar D_2i = D_1i;

    /* Step 6: Apply final response function */
    alwan_scalar A_1 = atd95_final_response_v(A_1i);
    alwan_scalar T_1 = atd95_final_response_v(T_1i);
    alwan_scalar D_1 = atd95_final_response_v(D_1i);

    alwan_scalar A_2 = atd95_final_response_v(A_2i);
    alwan_scalar T_2 = atd95_final_response_v(T_2i);
    alwan_scalar D_2 = atd95_final_response_v(D_2i);

    /* Step 7: Compute appearance correlates */

    /* Brightness */
    alwan_scalar Br = ALWAN_SQRT(A_1 * A_1 + T_1 * T_1 + D_1 * D_1);

    /* Saturation */
    alwan_scalar abs_A2 = ALWAN_ABS(A_2);
    alwan_scalar C = ALWAN_SELECT(abs_A2 > ALWAN_LITERAL(1e-10),
                                  ALWAN_SQRT(T_2 * T_2 + D_2 * D_2) / A_2,
                                  ALWAN_ZERO);

    /* Hue angle */
    alwan_scalar H = ALWAN_ATAN2(T_2, D_2) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    H = ALWAN_SELECT(H < ALWAN_ZERO, H + ALWAN_LITERAL(360.0), H);

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

#endif /* ALWAN_ATD95_CORE_H */
