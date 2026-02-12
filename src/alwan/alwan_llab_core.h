/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only LLAB Color Appearance Model
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Luo, Lo & Kuo (1996)
 * "The LLAB(l:c) colour model"
 */

#ifndef ALWAN_LLAB_CORE_H
#define ALWAN_LLAB_CORE_H

#include "alwan_platform.h"
#include "alwan_types.h"

/* ----------------------------------------------------------------
 * LLAB Output Type
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_scalar L, Ch, h, s;
} alwan_llab_v_correlates;

/* ----------------------------------------------------------------
 * LLAB Transformation Matrices
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const LLAB_XYZ_TO_RGB[9] = {
#include "data/matrices/llab_xyz_to_rgb.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const LLAB_RGB_TO_XYZ[9] = {
#include "data/matrices/llab_rgb_to_xyz.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * LLAB Constants
 * ---------------------------------------------------------------- */

static alwan_scalar const LLAB_V_EPSILON = ALWAN_LITERAL(0.008856);
static alwan_scalar const LLAB_V_COEF_116 = ALWAN_LITERAL(116.0);
static alwan_scalar const LLAB_V_COEF_16 = ALWAN_LITERAL(16.0);
static alwan_scalar const LLAB_V_COEF_500 = ALWAN_LITERAL(500.0);
static alwan_scalar const LLAB_V_COEF_200 = ALWAN_LITERAL(200.0);
static alwan_scalar const LLAB_V_CHROMA_SCALE = ALWAN_LITERAL(25.0);
static alwan_scalar const LLAB_V_CHROMA_CONST = ALWAN_LITERAL(0.05);
static alwan_scalar const LLAB_V_BETA_EXP = ALWAN_LITERAL(0.0834);

/* ----------------------------------------------------------------
 * Helper: LLAB f(t) with F_S induction factor
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_scalar llab_f_v(alwan_scalar t, alwan_scalar F_S) {
    alwan_scalar power_result = ALWAN_POW(t, ALWAN_LITERAL(1.0) / F_S);
    alwan_scalar slope = (ALWAN_POW(LLAB_V_EPSILON, ALWAN_LITERAL(1.0) / F_S) -
                          LLAB_V_COEF_16 / LLAB_V_COEF_116) / LLAB_V_EPSILON;
    alwan_scalar linear_result = slope * t + LLAB_V_COEF_16 / LLAB_V_COEF_116;
    return ALWAN_SELECT(t > LLAB_V_EPSILON, power_result, linear_result);
}

/* ----------------------------------------------------------------
 * LLAB Forward Transform (value-returning)
 *
 * Parameters:
 *   xyz   - Input XYZ tristimulus values
 *   xyz_0 - Test illuminant white point (XYZ)
 *   xyz_r - Reference illuminant white point (XYZ)
 *   Y_b   - Background luminance factor (%)
 *   D     - Discounting-the-Illuminant factor
 *   F_S   - Surround induction factor
 *   F_L   - Lightness induction factor
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_llab_v_correlates alwan_llab_forward_v(
    alwan_xyz xyz,
    alwan_xyz xyz_0,
    alwan_xyz xyz_r,
    alwan_scalar Y_b,
    alwan_scalar D,
    alwan_scalar F_S,
    alwan_scalar F_L
) {
    alwan_llab_v_correlates result;

    /* Step 1: Convert XYZ to RGB (cone responses) */
    alwan_scalar R = LLAB_XYZ_TO_RGB[0] * xyz.x + LLAB_XYZ_TO_RGB[1] * xyz.y + LLAB_XYZ_TO_RGB[2] * xyz.z;
    alwan_scalar G = LLAB_XYZ_TO_RGB[3] * xyz.x + LLAB_XYZ_TO_RGB[4] * xyz.y + LLAB_XYZ_TO_RGB[5] * xyz.z;
    alwan_scalar B = LLAB_XYZ_TO_RGB[6] * xyz.x + LLAB_XYZ_TO_RGB[7] * xyz.y + LLAB_XYZ_TO_RGB[8] * xyz.z;

    /* Convert test illuminant to RGB */
    alwan_scalar R_0 = LLAB_XYZ_TO_RGB[0] * xyz_0.x + LLAB_XYZ_TO_RGB[1] * xyz_0.y + LLAB_XYZ_TO_RGB[2] * xyz_0.z;
    alwan_scalar G_0 = LLAB_XYZ_TO_RGB[3] * xyz_0.x + LLAB_XYZ_TO_RGB[4] * xyz_0.y + LLAB_XYZ_TO_RGB[5] * xyz_0.z;
    alwan_scalar B_0 = LLAB_XYZ_TO_RGB[6] * xyz_0.x + LLAB_XYZ_TO_RGB[7] * xyz_0.y + LLAB_XYZ_TO_RGB[8] * xyz_0.z;

    /* Convert reference illuminant to RGB */
    alwan_scalar R_r = LLAB_XYZ_TO_RGB[0] * xyz_r.x + LLAB_XYZ_TO_RGB[1] * xyz_r.y + LLAB_XYZ_TO_RGB[2] * xyz_r.z;
    alwan_scalar G_r = LLAB_XYZ_TO_RGB[3] * xyz_r.x + LLAB_XYZ_TO_RGB[4] * xyz_r.y + LLAB_XYZ_TO_RGB[5] * xyz_r.z;
    alwan_scalar B_r = LLAB_XYZ_TO_RGB[6] * xyz_r.x + LLAB_XYZ_TO_RGB[7] * xyz_r.y + LLAB_XYZ_TO_RGB[8] * xyz_r.z;

    /* Step 2: BFD chromatic adaptation */

    /* R channel: linear adaptation */
    alwan_scalar ratio_R = R_r / R_0;
    alwan_scalar R_adapted = (D * ratio_R + (ALWAN_LITERAL(1.0) - D)) * R;

    /* G channel: linear adaptation */
    alwan_scalar ratio_G = G_r / G_0;
    alwan_scalar G_adapted = (D * ratio_G + (ALWAN_LITERAL(1.0) - D)) * G;

    /* B channel: nonlinear adaptation with power function */
    alwan_scalar beta = ALWAN_POW(B_0 / B_r, LLAB_V_BETA_EXP);
    alwan_scalar B_adapted_factor = D * ALWAN_POW(B_r / B_0, beta) + (ALWAN_LITERAL(1.0) - D);
    alwan_scalar B_adapted = B_adapted_factor * ALWAN_POW(B, beta);

    /* Step 3: Convert adapted RGB back to XYZ */
    alwan_scalar X_adapted = LLAB_RGB_TO_XYZ[0] * R_adapted + LLAB_RGB_TO_XYZ[1] * G_adapted + LLAB_RGB_TO_XYZ[2] * B_adapted;
    alwan_scalar Y_adapted = LLAB_RGB_TO_XYZ[3] * R_adapted + LLAB_RGB_TO_XYZ[4] * G_adapted + LLAB_RGB_TO_XYZ[5] * B_adapted;
    alwan_scalar Z_adapted = LLAB_RGB_TO_XYZ[6] * R_adapted + LLAB_RGB_TO_XYZ[7] * G_adapted + LLAB_RGB_TO_XYZ[8] * B_adapted;

    /* Step 4: Compute Lab coordinates with F_S induction factor */
    /* Reference white for Lab calculation (use reference condition illuminant) */
    alwan_scalar X_n = xyz_r.x;
    alwan_scalar Y_n = xyz_r.y;
    alwan_scalar Z_n = xyz_r.z;

    alwan_scalar f_X = llab_f_v(X_adapted / X_n, F_S);
    alwan_scalar f_Y = llab_f_v(Y_adapted / Y_n, F_S);
    alwan_scalar f_Z = llab_f_v(Z_adapted / Z_n, F_S);

    /* Compute z factor for lightness modulation */
    alwan_scalar z = ALWAN_LITERAL(1.0) + F_L * ALWAN_SQRT(Y_b / ALWAN_LITERAL(100.0));

    /* LLAB lightness with z modulation */
    alwan_scalar L_L = LLAB_V_COEF_116 * ALWAN_POW(f_Y, z) - LLAB_V_COEF_16;

    /* LLAB opponent dimensions */
    alwan_scalar a_L = LLAB_V_COEF_500 * (f_X - f_Y);
    alwan_scalar b_L = LLAB_V_COEF_200 * (f_Y - f_Z);

    /* Step 5: Compute appearance correlates */

    /* Chroma */
    alwan_scalar c = ALWAN_SQRT(a_L * a_L + b_L * b_L);
    alwan_scalar Ch_L = LLAB_V_CHROMA_SCALE *
                        ALWAN_LN(ALWAN_LITERAL(1.0) + LLAB_V_CHROMA_CONST * c);

    /* Hue angle */
    alwan_scalar h_rad = ALWAN_ATAN2(b_L, a_L);
    alwan_scalar h_deg = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    alwan_scalar h_L = ALWAN_SELECT(h_deg < ALWAN_LITERAL(0.0), h_deg + ALWAN_LITERAL(360.0), h_deg);

    /* Saturation */
    alwan_scalar s_L = ALWAN_SELECT(L_L > ALWAN_LITERAL(0.0), Ch_L / L_L, ALWAN_LITERAL(0.0));

    /* Output correlates */
    result.L = L_L;
    result.Ch = Ch_L;
    result.h = h_L;
    result.s = s_L;

    return result;
}

#endif /* ALWAN_LLAB_CORE_H */
