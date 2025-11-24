/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ATD95 Color Vision Model
 * Based on Guth's ATD (1995)
 *
 * References:
 * - Guth, S. L. (1995). Further applications of the ATD model for color vision.
 *   Proc. SPIE 2414, Device-Independent Color Imaging II.
 * - Fairchild, M. D. (2013). Color Appearance Models (3rd ed.). Wiley.
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <math.h>

/* ----------------------------------------------------------------
 * ATD95 Constants
 * ---------------------------------------------------------------- */

/* XYZ to LMS transformation matrix coefficients */
#define ATD95_LMS_L_X ALWAN_LITERAL(0.2435)
#define ATD95_LMS_L_Y ALWAN_LITERAL(0.8524)
#define ATD95_LMS_L_Z ALWAN_LITERAL(-0.0516)

#define ATD95_LMS_M_X ALWAN_LITERAL(-0.3954)
#define ATD95_LMS_M_Y ALWAN_LITERAL(1.1642)
#define ATD95_LMS_M_Z ALWAN_LITERAL(0.0837)

#define ATD95_LMS_S_Y ALWAN_LITERAL(0.04)
#define ATD95_LMS_S_Z ALWAN_LITERAL(0.6225)

/* Cone response parameters */
#define ATD95_L_SCALE ALWAN_LITERAL(0.66)
#define ATD95_L_OFFSET ALWAN_LITERAL(0.024)
#define ATD95_L_EXP ALWAN_LITERAL(0.7)

#define ATD95_M_OFFSET ALWAN_LITERAL(0.036)
#define ATD95_M_EXP ALWAN_LITERAL(0.7)

#define ATD95_S_SCALE ALWAN_LITERAL(0.43)
#define ATD95_S_OFFSET ALWAN_LITERAL(0.31)
#define ATD95_S_EXP ALWAN_LITERAL(0.7)

/* Retinal illuminance conversion */
#define ATD95_RETINAL_SCALE ALWAN_LITERAL(18.0)
#define ATD95_RETINAL_EXP ALWAN_LITERAL(0.8)

/* Opponent response coefficients */
#define ATD95_A1_L ALWAN_LITERAL(3.57)
#define ATD95_A1_M ALWAN_LITERAL(2.64)

#define ATD95_T1_L ALWAN_LITERAL(7.18)
#define ATD95_T1_M ALWAN_LITERAL(-6.21)

#define ATD95_D1_L ALWAN_LITERAL(-0.7)
#define ATD95_D1_M ALWAN_LITERAL(0.085)

#define ATD95_A2_SCALE ALWAN_LITERAL(0.09)
#define ATD95_T2_T ALWAN_LITERAL(0.43)
#define ATD95_T2_D ALWAN_LITERAL(0.76)

/* Final response normalization */
#define ATD95_FINAL_NORM ALWAN_LITERAL(200.0)

/* ----------------------------------------------------------------
 * Helper Functions
 * ---------------------------------------------------------------- */

/* Convert XYZ luminance to retinal illuminance */
static alwan_scalar xyz_to_retinal_illuminance(alwan_scalar Y, alwan_scalar Y_0) {
    return ATD95_RETINAL_SCALE * ALWAN_POW(Y_0 * Y / ALWAN_LITERAL(100.0), ATD95_RETINAL_EXP);
}

/* Transform retinal XYZ to LMS cone responses */
static void retinal_xyz_to_lms(
    alwan_scalar X_r,
    alwan_scalar Y_r,
    alwan_scalar Z_r,
    alwan_scalar *L,
    alwan_scalar *M,
    alwan_scalar *S
) {
    /* L cone response */
    alwan_scalar L_linear = ATD95_LMS_L_X * X_r + ATD95_LMS_L_Y * Y_r + ATD95_LMS_L_Z * Z_r;
    *L = ATD95_L_SCALE * ALWAN_POW(ALWAN_FABS(L_linear), ATD95_L_EXP) + ATD95_L_OFFSET;
    if (L_linear < ALWAN_LITERAL(0.0)) *L = -(*L);

    /* M cone response */
    alwan_scalar M_linear = ATD95_LMS_M_X * X_r + ATD95_LMS_M_Y * Y_r + ATD95_LMS_M_Z * Z_r;
    *M = ALWAN_POW(ALWAN_FABS(M_linear), ATD95_M_EXP) + ATD95_M_OFFSET;
    if (M_linear < ALWAN_LITERAL(0.0)) *M = -(*M);

    /* S cone response */
    alwan_scalar S_linear = ATD95_LMS_S_Y * Y_r + ATD95_LMS_S_Z * Z_r;
    *S = ATD95_S_SCALE * ALWAN_POW(ALWAN_FABS(S_linear), ATD95_S_EXP) + ATD95_S_OFFSET;
    if (S_linear < ALWAN_LITERAL(0.0)) *S = -(*S);
}

/* Final response function (normalization) */
static alwan_scalar final_response(alwan_scalar value) {
    return value / (ATD95_FINAL_NORM + ALWAN_FABS(value));
}

/* ----------------------------------------------------------------
 * ATD95 Forward Transform
 * ---------------------------------------------------------------- */

int alwan_atd95_forward(
    alwan_vec3 const *xyz,
    alwan_atd95_viewing_conditions const *vc,
    alwan_atd95_correlates *out
) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Step 1: Convert XYZ to retinal illuminance for both sample and white */
    alwan_scalar X_r = xyz_to_retinal_illuminance(xyz->v[0], vc->Y_0);
    alwan_scalar Y_r = xyz_to_retinal_illuminance(xyz->v[1], vc->Y_0);
    alwan_scalar Z_r = xyz_to_retinal_illuminance(xyz->v[2], vc->Y_0);

    alwan_scalar X_0r = xyz_to_retinal_illuminance(vc->white_xyz.v[0], vc->Y_0);
    alwan_scalar Y_0r = xyz_to_retinal_illuminance(vc->white_xyz.v[1], vc->Y_0);
    alwan_scalar Z_0r = xyz_to_retinal_illuminance(vc->white_xyz.v[2], vc->Y_0);

    /* Step 2: Transform to LMS cone responses */
    alwan_scalar L, M, S, L_0, M_0, S_0;
    retinal_xyz_to_lms(X_r, Y_r, Z_r, &L, &M, &S);
    retinal_xyz_to_lms(X_0r, Y_0r, Z_0r, &L_0, &M_0, &S_0);

    /* Step 3: Compute adapted responses using k1, k2, and sigma */
    /* Adapted illuminance for gain control */
    alwan_scalar X_ar = vc->k1 * X_r + vc->k2 * X_0r;
    alwan_scalar Y_ar = vc->k1 * Y_r + vc->k2 * Y_0r;
    alwan_scalar Z_ar = vc->k1 * Z_r + vc->k2 * Z_0r;

    alwan_scalar L_a, M_a, S_a;
    retinal_xyz_to_lms(X_ar, Y_ar, Z_ar, &L_a, &M_a, &S_a);

    /* Apply gain normalization with sigma */
    alwan_scalar sigma = vc->sigma;
    alwan_scalar L_g = L * (sigma / (sigma + L_a));
    alwan_scalar M_g = M * (sigma / (sigma + M_a));
    alwan_scalar S_g = S * (sigma / (sigma + S_a));

    /* Step 4: Compute opponent color dimensions */
    alwan_scalar A_1i = ATD95_A1_L * L_g + ATD95_A1_M * M_g;
    alwan_scalar T_1i = ATD95_T1_L * L_g + ATD95_T1_M * M_g;
    alwan_scalar D_1i = ATD95_D1_L * L_g + ATD95_D1_M * M_g + S_g;

    alwan_scalar A_2i = ATD95_A2_SCALE * A_1i;
    alwan_scalar T_2i = ATD95_T2_T * T_1i + ATD95_T2_D * D_1i;
    alwan_scalar D_2i = D_1i;

    /* Apply final response function */
    alwan_scalar A_1 = final_response(A_1i);
    alwan_scalar T_1 = final_response(T_1i);
    alwan_scalar D_1 = final_response(D_1i);

    alwan_scalar A_2 = final_response(A_2i);
    alwan_scalar T_2 = final_response(T_2i);
    alwan_scalar D_2 = final_response(D_2i);

    /* Step 5: Compute appearance correlates */

    /* Brightness */
    alwan_scalar Br = ALWAN_SQRT(A_1 * A_1 + T_1 * T_1 + D_1 * D_1);

    /* Saturation correlate */
    alwan_scalar C = ALWAN_LITERAL(0.0);
    if (ALWAN_FABS(A_2) > ALWAN_LITERAL(1e-10)) {
        C = ALWAN_SQRT(T_2 * T_2 + D_2 * D_2) / A_2;
    }

    /* Hue angle */
    alwan_scalar H = ALWAN_ATAN2(T_2, D_2) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    if (H < ALWAN_LITERAL(0.0)) {
        H += ALWAN_LITERAL(360.0);
    }

    /* Output correlates */
    out->H = H;
    out->C = C;
    out->Br = Br;
    out->A_1 = A_1;
    out->T_1 = T_1;
    out->D_1 = D_1;
    out->A_2 = A_2;
    out->T_2 = T_2;
    out->D_2 = D_2;

    return ALWAN_OK;
}
