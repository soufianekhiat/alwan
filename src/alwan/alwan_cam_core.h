/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only CIECAM02 & CAM16 Color Appearance Models
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: CIE 159:2004 (CIECAM02), Li et al. (2017) (CAM16)
 *
 * The _v() functions take all viewing condition parameters as direct
 * scalar arguments (F, c, Nc, D, FL, n, Nbb, Ncb, z, A_w, La, Y_b, Y_w)
 * instead of structs with enums. The .c wrapper computes these derived
 * values and calls the _v() function.
 */

#ifndef ALWAN_CAM_CORE_H
#define ALWAN_CAM_CORE_H

#include "alwan_platform.h"
#include "alwan_types.h"

/* ----------------------------------------------------------------
 * CIECAM02 / CAM16 Correlates (value-returning variants)
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_scalar J, C, h, s, Q, M, H;
} alwan_ciecam02_v_correlates;

typedef struct {
    alwan_scalar J, C, h, s, Q, M, H;
} alwan_cam16_v_correlates;

/* ----------------------------------------------------------------
 * Transformation Matrices
 * ---------------------------------------------------------------- */

/* Hunt-Pointer-Estevez (HPE) matrix for CIECAM02 XYZ <-> LMS */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const CAM_M_HPE[9] = {
#include "data/matrices/hpe.csv"
};
static alwan_scalar const CAM_M_HPE_INV[9] = {
#include "data/matrices/hpe_inv.csv"
};
/* CAT16 matrix for CAM16 XYZ <-> LMS */
static alwan_scalar const CAM_M_CAT16[9] = {
#include "data/matrices/cat_cat16.csv"
};
static alwan_scalar const CAM_M_CAT16_INV[9] = {
#include "data/matrices/cat_cat16_inv.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Hue Quadrature Data (shared by CIECAM02 and CAM16)
 * ---------------------------------------------------------------- */

static alwan_scalar const CAM_HQ_h_i[5] = {
    ALWAN_LITERAL(20.14), ALWAN_LITERAL(90.00),
    ALWAN_LITERAL(164.25), ALWAN_LITERAL(237.53),
    ALWAN_LITERAL(380.14)
};

static alwan_scalar const CAM_HQ_e_i[5] = {
    ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.7),
    ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.2),
    ALWAN_LITERAL(0.8)  /* wraparound: e_i[4] == e_i[0] */
};

static alwan_scalar const CAM_HQ_H_i[5] = {
    ALWAN_LITERAL(0.0), ALWAN_LITERAL(100.0),
    ALWAN_LITERAL(200.0), ALWAN_LITERAL(300.0),
    ALWAN_LITERAL(400.0)
};

/* ----------------------------------------------------------------
 * Helper: Compute degree of adaptation D
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_scalar cam_compute_D_v(
    alwan_scalar F,
    alwan_scalar La,
    alwan_scalar discount_illuminant
) {
    alwan_scalar D_computed = F * (ALWAN_ONE - ALWAN_ONE / ALWAN_LITERAL(3.6) *
                     ALWAN_EXP((-La - ALWAN_LITERAL(42.0)) / ALWAN_LITERAL(92.0)));
    alwan_scalar D_clamped = alwan_clamp(D_computed, ALWAN_ZERO, ALWAN_ONE);
    return ALWAN_SELECT(discount_illuminant > ALWAN_LITERAL(0.5), ALWAN_ONE, D_clamped);
}

/* ----------------------------------------------------------------
 * Helper: Post-adaptation nonlinear response compression
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_scalar cam_post_adaptation_nonlinear_v(alwan_scalar x) {
    alwan_scalar x_abs = ALWAN_ABS(x);
    alwan_scalar x_pow = ALWAN_POW(x_abs, ALWAN_LITERAL(0.42));
    alwan_scalar magnitude = ALWAN_LITERAL(400.0) * x_pow / (ALWAN_LITERAL(27.13) + x_pow) + ALWAN_LITERAL(0.1);
    return ALWAN_SELECT(x < ALWAN_ZERO, -magnitude, magnitude);
}

/* ----------------------------------------------------------------
 * Helper: Inverse post-adaptation nonlinear response
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_scalar cam_post_adaptation_nonlinear_inv_v(alwan_scalar x) {
    alwan_scalar y = x - ALWAN_LITERAL(0.1);
    alwan_scalar y_abs = ALWAN_ABS(y);
    alwan_scalar magnitude = ALWAN_POW(
        (ALWAN_LITERAL(27.13) * y_abs) / (ALWAN_LITERAL(400.0) - y_abs),
        ALWAN_ONE / ALWAN_LITERAL(0.42));
    return ALWAN_SELECT(y < ALWAN_ZERO, -magnitude, magnitude);
}

/* ----------------------------------------------------------------
 * Derived viewing condition parameters (value-returning)
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_scalar D, FL, n, Nbb, Ncb, z, A_w;
} alwan_cam_derived_params;

/* Compute all derived viewing condition parameters from scalars.
 * M_cat must point to a 9-element row-major 3x3 matrix
 * (HPE for CIECAM02, CAT16 for CAM16). */
ALWAN_INLINE alwan_cam_derived_params cam_compute_derived_params_v(
    alwan_scalar F,
    alwan_scalar La,
    alwan_scalar Yb,
    alwan_scalar Yw,
    alwan_scalar discount_illuminant,
    alwan_xyz white_xyz,
    alwan_scalar const *M_cat
) {
    alwan_cam_derived_params p;

    /* Degree of adaptation D */
    p.D = cam_compute_D_v(F, La, discount_illuminant);

    /* Luminance level adaptation factor FL */
    alwan_scalar k = ALWAN_ONE / (ALWAN_LITERAL(5.0) * La + ALWAN_ONE);
    alwan_scalar k4 = k * k * k * k;
    p.FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * La) +
           ALWAN_LITERAL(0.1) * (ALWAN_ONE - k4) * (ALWAN_ONE - k4) *
           ALWAN_POW(ALWAN_LITERAL(5.0) * La, ALWAN_ONE / ALWAN_LITERAL(3.0));

    /* Background parameters */
    p.n   = Yb / Yw;
    p.Nbb = ALWAN_LITERAL(0.725) * ALWAN_POW(ALWAN_ONE / p.n, ALWAN_LITERAL(0.2));
    p.Ncb = p.Nbb;
    p.z   = ALWAN_LITERAL(1.48) + ALWAN_SQRT(p.n);

    /* Achromatic response of the white point A_w */
    alwan_scalar R_w = M_cat[0] * white_xyz.x + M_cat[1] * white_xyz.y + M_cat[2] * white_xyz.z;
    alwan_scalar G_w = M_cat[3] * white_xyz.x + M_cat[4] * white_xyz.y + M_cat[5] * white_xyz.z;
    alwan_scalar B_w = M_cat[6] * white_xyz.x + M_cat[7] * white_xyz.y + M_cat[8] * white_xyz.z;

    alwan_scalar D_R_w = p.D * (Yw / R_w) + ALWAN_ONE - p.D;
    alwan_scalar D_G_w = p.D * (Yw / G_w) + ALWAN_ONE - p.D;
    alwan_scalar D_B_w = p.D * (Yw / B_w) + ALWAN_ONE - p.D;

    alwan_scalar R_cw = R_w * D_R_w;
    alwan_scalar G_cw = G_w * D_G_w;
    alwan_scalar B_cw = B_w * D_B_w;

    alwan_scalar R_aw = cam_post_adaptation_nonlinear_v(p.FL * R_cw / ALWAN_LITERAL(100.0));
    alwan_scalar G_aw = cam_post_adaptation_nonlinear_v(p.FL * G_cw / ALWAN_LITERAL(100.0));
    alwan_scalar B_aw = cam_post_adaptation_nonlinear_v(p.FL * B_cw / ALWAN_LITERAL(100.0));

    p.A_w = (ALWAN_LITERAL(2.0) * R_aw + G_aw +
             ALWAN_LITERAL(0.05) * B_aw - ALWAN_LITERAL(0.305)) * p.Nbb;

    return p;
}

/* ----------------------------------------------------------------
 * Helper: Hue angle to hue quadrature H (branchless, unrolled)
 *
 * h must be in [0, 360). Uses nested ALWAN_SELECT for sector
 * selection across 5 sectors.
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_scalar cam_hue_to_quadrature_v(alwan_scalar h) {
    /* Normalize h to [0, 360) */
    alwan_scalar h_norm = ALWAN_FMOD(h, ALWAN_LITERAL(360.0));
    h_norm = ALWAN_SELECT(h_norm < ALWAN_ZERO, h_norm + ALWAN_LITERAL(360.0), h_norm);

    /* Wrap: if h' < 20.14, shift it up by 360 to match the table range */
    alwan_scalar hp = ALWAN_SELECT(h_norm < ALWAN_LITERAL(20.14),
                                   h_norm + ALWAN_LITERAL(360.0), h_norm);

    /* Compute H for each of the 4 possible sectors */
    /* Sector 0: h_i[0]=20.14 .. h_i[1]=90.00, e_i[0]=0.8, e_i[1]=0.7 */
    alwan_scalar h_diff_0 = hp - CAM_HQ_h_i[0];
    alwan_scalar hi_diff_0 = CAM_HQ_h_i[1] - CAM_HQ_h_i[0];
    alwan_scalar H_0 = CAM_HQ_H_i[0] + (ALWAN_LITERAL(100.0) * h_diff_0 * CAM_HQ_e_i[0]) /
                        (h_diff_0 * CAM_HQ_e_i[0] + (hi_diff_0 - h_diff_0) * CAM_HQ_e_i[1]);

    /* Sector 1: h_i[1]=90.00 .. h_i[2]=164.25, e_i[1]=0.7, e_i[2]=1.0 */
    alwan_scalar h_diff_1 = hp - CAM_HQ_h_i[1];
    alwan_scalar hi_diff_1 = CAM_HQ_h_i[2] - CAM_HQ_h_i[1];
    alwan_scalar H_1 = CAM_HQ_H_i[1] + (ALWAN_LITERAL(100.0) * h_diff_1 * CAM_HQ_e_i[1]) /
                        (h_diff_1 * CAM_HQ_e_i[1] + (hi_diff_1 - h_diff_1) * CAM_HQ_e_i[2]);

    /* Sector 2: h_i[2]=164.25 .. h_i[3]=237.53, e_i[2]=1.0, e_i[3]=1.2 */
    alwan_scalar h_diff_2 = hp - CAM_HQ_h_i[2];
    alwan_scalar hi_diff_2 = CAM_HQ_h_i[3] - CAM_HQ_h_i[2];
    alwan_scalar H_2 = CAM_HQ_H_i[2] + (ALWAN_LITERAL(100.0) * h_diff_2 * CAM_HQ_e_i[2]) /
                        (h_diff_2 * CAM_HQ_e_i[2] + (hi_diff_2 - h_diff_2) * CAM_HQ_e_i[3]);

    /* Sector 3: h_i[3]=237.53 .. h_i[4]=380.14, e_i[3]=1.2, e_i[4]=0.8 */
    alwan_scalar h_diff_3 = hp - CAM_HQ_h_i[3];
    alwan_scalar hi_diff_3 = CAM_HQ_h_i[4] - CAM_HQ_h_i[3];
    alwan_scalar H_3 = CAM_HQ_H_i[3] + (ALWAN_LITERAL(100.0) * h_diff_3 * CAM_HQ_e_i[3]) /
                        (h_diff_3 * CAM_HQ_e_i[3] + (hi_diff_3 - h_diff_3) * CAM_HQ_e_i[4]);

    /* Nested ALWAN_SELECT to choose the correct sector */
    alwan_scalar H_val = ALWAN_SELECT(hp < CAM_HQ_h_i[1], H_0,
                         ALWAN_SELECT(hp < CAM_HQ_h_i[2], H_1,
                         ALWAN_SELECT(hp < CAM_HQ_h_i[3], H_2,
                                                           H_3)));

    return H_val;
}

/* ----------------------------------------------------------------
 * Helper: Inverse opponent colour dimensions
 *
 * Given t, h (radians), Nc, Ncb, et, A, Nbb, P_3, compute a and b.
 * Uses ALWAN_SELECT instead of if/else for branchless path.
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_vec2 cam_opponent_colour_inverse_v(
    alwan_scalar t,
    alwan_scalar h_rad,
    alwan_scalar Nc,
    alwan_scalar Ncb,
    alwan_scalar et,
    alwan_scalar A,
    alwan_scalar Nbb
) {
    alwan_vec2 result;

    alwan_scalar sin_h = ALWAN_SIN(h_rad);
    alwan_scalar cos_h = ALWAN_COS(h_rad);

    alwan_scalar P_1 = (ALWAN_LITERAL(50000.0) / ALWAN_LITERAL(13.0)) * Nc * Ncb * et / t;
    alwan_scalar P_2 = A / Nbb + ALWAN_LITERAL(0.305);
    alwan_scalar P_3 = ALWAN_LITERAL(21.0) / ALWAN_LITERAL(20.0);

    alwan_scalar numerator = P_2 * (ALWAN_LITERAL(2.0) + P_3) * (ALWAN_LITERAL(460.0) / ALWAN_LITERAL(1403.0));

    /* Case 1: |sin(h)| >= |cos(h)| -- solve for b first */
    alwan_scalar P_4 = P_1 / sin_h;
    alwan_scalar denom_b = P_4 +
                          (ALWAN_LITERAL(2.0) + P_3) * (ALWAN_LITERAL(220.0) / ALWAN_LITERAL(1403.0)) * (cos_h / sin_h) -
                          (ALWAN_LITERAL(27.0) / ALWAN_LITERAL(1403.0)) +
                          P_3 * (ALWAN_LITERAL(6300.0) / ALWAN_LITERAL(1403.0));
    alwan_scalar b_case1 = numerator / denom_b;
    alwan_scalar a_case1 = b_case1 * (cos_h / sin_h);

    /* Case 2: |sin(h)| < |cos(h)| -- solve for a first */
    alwan_scalar P_5 = P_1 / cos_h;
    alwan_scalar denom_a = P_5 +
                          (ALWAN_LITERAL(2.0) + P_3) * (ALWAN_LITERAL(220.0) / ALWAN_LITERAL(1403.0)) -
                          ((ALWAN_LITERAL(27.0) / ALWAN_LITERAL(1403.0)) - P_3 * (ALWAN_LITERAL(6300.0) / ALWAN_LITERAL(1403.0))) * (sin_h / cos_h);
    alwan_scalar a_case2 = numerator / denom_a;
    alwan_scalar b_case2 = a_case2 * (sin_h / cos_h);

    alwan_scalar use_sin_path = ALWAN_ABS(sin_h) >= ALWAN_ABS(cos_h);
    result.v[0] = ALWAN_SELECT(use_sin_path, a_case1, a_case2);
    result.v[1] = ALWAN_SELECT(use_sin_path, b_case1, b_case2);

    return result;
}

/* ================================================================
 * CIECAM02 Forward Transform (value-returning)
 *
 * All viewing condition parameters are pre-computed scalars:
 *   F, c, Nc  - surround parameters
 *   D         - degree of adaptation
 *   FL        - luminance level adaptation factor
 *   n         - Y_b / Y_w
 *   Nbb, Ncb  - background/chromatic induction factors
 *   z         - base exponential nonlinearity
 *   A_w       - achromatic response of the white point
 *   La        - adapting luminance (cd/m^2)
 *   Y_b       - background luminance factor
 *   Y_w       - reference white Y
 * ================================================================ */

ALWAN_INLINE alwan_ciecam02_v_correlates alwan_ciecam02_forward_v(
    alwan_xyz xyz,
    alwan_xyz white_xyz,
    alwan_scalar F,
    alwan_scalar c,
    alwan_scalar Nc,
    alwan_scalar D,
    alwan_scalar FL,
    alwan_scalar n,
    alwan_scalar Nbb,
    alwan_scalar Ncb,
    alwan_scalar z,
    alwan_scalar A_w
) {
    alwan_ciecam02_v_correlates result;
    (void)F; /* F already folded into pre-computed D */

    /* Step 1: XYZ -> LMS via HPE matrix (stimulus) */
    alwan_scalar R = CAM_M_HPE[0] * xyz.x + CAM_M_HPE[1] * xyz.y + CAM_M_HPE[2] * xyz.z;
    alwan_scalar G = CAM_M_HPE[3] * xyz.x + CAM_M_HPE[4] * xyz.y + CAM_M_HPE[5] * xyz.z;
    alwan_scalar B = CAM_M_HPE[6] * xyz.x + CAM_M_HPE[7] * xyz.y + CAM_M_HPE[8] * xyz.z;

    /* Step 2: XYZ -> LMS via HPE matrix (white point) */
    alwan_scalar R_w = CAM_M_HPE[0] * white_xyz.x + CAM_M_HPE[1] * white_xyz.y + CAM_M_HPE[2] * white_xyz.z;
    alwan_scalar G_w = CAM_M_HPE[3] * white_xyz.x + CAM_M_HPE[4] * white_xyz.y + CAM_M_HPE[5] * white_xyz.z;
    alwan_scalar B_w = CAM_M_HPE[6] * white_xyz.x + CAM_M_HPE[7] * white_xyz.y + CAM_M_HPE[8] * white_xyz.z;

    /* Step 3: Chromatic adaptation */
    alwan_scalar D_R = D * (white_xyz.y / R_w) + ALWAN_ONE - D;
    alwan_scalar D_G = D * (white_xyz.y / G_w) + ALWAN_ONE - D;
    alwan_scalar D_B = D * (white_xyz.y / B_w) + ALWAN_ONE - D;

    alwan_scalar R_c = R * D_R;
    alwan_scalar G_c = G * D_G;
    alwan_scalar B_c = B * D_B;

    /* Step 4: Post-adaptation nonlinear response compression */
    alwan_scalar R_a = cam_post_adaptation_nonlinear_v(FL * R_c / ALWAN_LITERAL(100.0));
    alwan_scalar G_a = cam_post_adaptation_nonlinear_v(FL * G_c / ALWAN_LITERAL(100.0));
    alwan_scalar B_a = cam_post_adaptation_nonlinear_v(FL * B_c / ALWAN_LITERAL(100.0));

    /* Step 5: Achromatic response A */
    alwan_scalar A = (ALWAN_LITERAL(2.0) * R_a + G_a +
                ALWAN_LITERAL(0.05) * B_a - ALWAN_LITERAL(0.305)) * Nbb;

    /* Step 6: Opponent colour dimensions */
    alwan_scalar a = R_a - ALWAN_LITERAL(12.0) * G_a / ALWAN_LITERAL(11.0) +
               B_a / ALWAN_LITERAL(11.0);
    alwan_scalar b = (R_a + G_a - ALWAN_LITERAL(2.0) * B_a) / ALWAN_LITERAL(9.0);

    /* Step 7: Hue angle h (degrees) */
    alwan_scalar h_rad = ALWAN_ATAN2(b, a);
    alwan_scalar h = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    h = ALWAN_SELECT(h < ALWAN_ZERO, h + ALWAN_LITERAL(360.0), h);

    /* Step 8: Eccentricity et */
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));

    /* Step 9: Hue quadrature H */
    alwan_scalar H = cam_hue_to_quadrature_v(h);

    /* Step 10: Lightness J */
    alwan_scalar J = ALWAN_LITERAL(100.0) * ALWAN_POW(A / A_w, c * z);

    /* Step 11: Brightness Q */
    alwan_scalar Q = (ALWAN_LITERAL(4.0) / c) * ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               (A_w + ALWAN_LITERAL(4.0)) * ALWAN_POW(FL, ALWAN_LITERAL(0.25));

    /* Step 12: Chroma C */
    alwan_scalar t_val = (ALWAN_LITERAL(50000.0) / ALWAN_LITERAL(13.0) * Nc * Ncb * et *
                ALWAN_SQRT(a * a + b * b)) /
               (R_a + G_a + ALWAN_LITERAL(21.0) / ALWAN_LITERAL(20.0) * B_a);
    alwan_scalar C = ALWAN_POW(t_val, ALWAN_LITERAL(0.9)) *
               ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73));

    /* Step 13: Colorfulness M and saturation s */
    alwan_scalar M = C * ALWAN_POW(FL, ALWAN_LITERAL(0.25));
    alwan_scalar s = ALWAN_LITERAL(100.0) * ALWAN_SQRT(M / Q);

    result.J = J;
    result.C = C;
    result.h = h;
    result.s = s;
    result.Q = Q;
    result.M = M;
    result.H = H;

    return result;
}

/* ================================================================
 * CIECAM02 Inverse Transform (value-returning)
 * ================================================================ */

ALWAN_INLINE alwan_xyz alwan_ciecam02_inverse_v(
    alwan_scalar J,
    alwan_scalar C,
    alwan_scalar h,
    alwan_xyz white_xyz,
    alwan_scalar F,
    alwan_scalar c,
    alwan_scalar Nc,
    alwan_scalar D,
    alwan_scalar FL,
    alwan_scalar n,
    alwan_scalar Nbb,
    alwan_scalar Ncb,
    alwan_scalar z,
    alwan_scalar A_w
) {
    alwan_xyz result;
    (void)F; /* F already folded into pre-computed D */

    /* Step 1: Compute achromatic response A from J */
    alwan_scalar A = A_w * ALWAN_POW(J / ALWAN_LITERAL(100.0), ALWAN_ONE / (c * z));

    /* Step 2: Compute t from C */
    alwan_scalar t_val = ALWAN_POW(
        C / (ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
             ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73))),
        ALWAN_ONE / ALWAN_LITERAL(0.9));

    /* Step 3: Eccentricity et */
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));

    /* Step 4: Compute opponent dimensions a, b via helper */
    alwan_scalar h_rad = h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_vec2 ab = cam_opponent_colour_inverse_v(t_val, h_rad, Nc, Ncb, et, A, Nbb);
    alwan_scalar a = ab.v[0];
    alwan_scalar b = ab.v[1];

    /* Step 5: Compute adapted signals RGB_a from a, b, A */
    alwan_scalar P_2 = A / Nbb + ALWAN_LITERAL(0.305);
    alwan_scalar R_a = (ALWAN_LITERAL(460.0) * P_2 + ALWAN_LITERAL(451.0) * a + ALWAN_LITERAL(288.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar G_a = (ALWAN_LITERAL(460.0) * P_2 - ALWAN_LITERAL(891.0) * a - ALWAN_LITERAL(261.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar B_a = (ALWAN_LITERAL(460.0) * P_2 - ALWAN_LITERAL(220.0) * a - ALWAN_LITERAL(6300.0) * b) / ALWAN_LITERAL(1403.0);

    /* Step 6: Inverse post-adaptation nonlinearity */
    alwan_scalar R_c = ALWAN_LITERAL(100.0) / FL * cam_post_adaptation_nonlinear_inv_v(R_a);
    alwan_scalar G_c = ALWAN_LITERAL(100.0) / FL * cam_post_adaptation_nonlinear_inv_v(G_a);
    alwan_scalar B_c = ALWAN_LITERAL(100.0) / FL * cam_post_adaptation_nonlinear_inv_v(B_a);

    /* Step 7: Inverse chromatic adaptation */
    alwan_scalar R_w = CAM_M_HPE[0] * white_xyz.x + CAM_M_HPE[1] * white_xyz.y + CAM_M_HPE[2] * white_xyz.z;
    alwan_scalar G_w = CAM_M_HPE[3] * white_xyz.x + CAM_M_HPE[4] * white_xyz.y + CAM_M_HPE[5] * white_xyz.z;
    alwan_scalar B_w = CAM_M_HPE[6] * white_xyz.x + CAM_M_HPE[7] * white_xyz.y + CAM_M_HPE[8] * white_xyz.z;

    alwan_scalar D_R = D * (white_xyz.y / R_w) + ALWAN_ONE - D;
    alwan_scalar D_G = D * (white_xyz.y / G_w) + ALWAN_ONE - D;
    alwan_scalar D_B = D * (white_xyz.y / B_w) + ALWAN_ONE - D;

    alwan_scalar R = R_c / D_R;
    alwan_scalar G = G_c / D_G;
    alwan_scalar B = B_c / D_B;

    /* Step 8: LMS -> XYZ via inverse HPE matrix */
    result.x = CAM_M_HPE_INV[0] * R + CAM_M_HPE_INV[1] * G + CAM_M_HPE_INV[2] * B;
    result.y = CAM_M_HPE_INV[3] * R + CAM_M_HPE_INV[4] * G + CAM_M_HPE_INV[5] * B;
    result.z = CAM_M_HPE_INV[6] * R + CAM_M_HPE_INV[7] * G + CAM_M_HPE_INV[8] * B;

    return result;
}

/* ================================================================
 * CAM16 Forward Transform (value-returning)
 *
 * Same parameter convention as CIECAM02 but uses CAT16 matrix.
 * ================================================================ */

ALWAN_INLINE alwan_cam16_v_correlates alwan_cam16_forward_v(
    alwan_xyz xyz,
    alwan_xyz white_xyz,
    alwan_scalar F,
    alwan_scalar c,
    alwan_scalar Nc,
    alwan_scalar D,
    alwan_scalar FL,
    alwan_scalar n,
    alwan_scalar Nbb,
    alwan_scalar Ncb,
    alwan_scalar z,
    alwan_scalar A_w
) {
    alwan_cam16_v_correlates result;
    (void)F; /* F already folded into pre-computed D */

    /* Step 1: XYZ -> LMS via CAT16 matrix (stimulus) */
    alwan_scalar R = CAM_M_CAT16[0] * xyz.x + CAM_M_CAT16[1] * xyz.y + CAM_M_CAT16[2] * xyz.z;
    alwan_scalar G = CAM_M_CAT16[3] * xyz.x + CAM_M_CAT16[4] * xyz.y + CAM_M_CAT16[5] * xyz.z;
    alwan_scalar B = CAM_M_CAT16[6] * xyz.x + CAM_M_CAT16[7] * xyz.y + CAM_M_CAT16[8] * xyz.z;

    /* Step 2: XYZ -> LMS via CAT16 matrix (white point) */
    alwan_scalar R_w = CAM_M_CAT16[0] * white_xyz.x + CAM_M_CAT16[1] * white_xyz.y + CAM_M_CAT16[2] * white_xyz.z;
    alwan_scalar G_w = CAM_M_CAT16[3] * white_xyz.x + CAM_M_CAT16[4] * white_xyz.y + CAM_M_CAT16[5] * white_xyz.z;
    alwan_scalar B_w = CAM_M_CAT16[6] * white_xyz.x + CAM_M_CAT16[7] * white_xyz.y + CAM_M_CAT16[8] * white_xyz.z;

    /* Step 3: Chromatic adaptation */
    alwan_scalar D_R = D * (white_xyz.y / R_w) + ALWAN_ONE - D;
    alwan_scalar D_G = D * (white_xyz.y / G_w) + ALWAN_ONE - D;
    alwan_scalar D_B = D * (white_xyz.y / B_w) + ALWAN_ONE - D;

    alwan_scalar R_c = R * D_R;
    alwan_scalar G_c = G * D_G;
    alwan_scalar B_c = B * D_B;

    /* Step 4: Post-adaptation nonlinear response compression */
    alwan_scalar R_a = cam_post_adaptation_nonlinear_v(FL * R_c / ALWAN_LITERAL(100.0));
    alwan_scalar G_a = cam_post_adaptation_nonlinear_v(FL * G_c / ALWAN_LITERAL(100.0));
    alwan_scalar B_a = cam_post_adaptation_nonlinear_v(FL * B_c / ALWAN_LITERAL(100.0));

    /* Step 5: Achromatic response A */
    alwan_scalar A = (ALWAN_LITERAL(2.0) * R_a + G_a +
                ALWAN_LITERAL(0.05) * B_a - ALWAN_LITERAL(0.305)) * Nbb;

    /* Step 6: Opponent colour dimensions */
    alwan_scalar a = R_a - ALWAN_LITERAL(12.0) * G_a / ALWAN_LITERAL(11.0) +
               B_a / ALWAN_LITERAL(11.0);
    alwan_scalar b = (R_a + G_a - ALWAN_LITERAL(2.0) * B_a) / ALWAN_LITERAL(9.0);

    /* Step 7: Hue angle h (degrees) */
    alwan_scalar h_rad = ALWAN_ATAN2(b, a);
    alwan_scalar h = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    h = ALWAN_SELECT(h < ALWAN_ZERO, h + ALWAN_LITERAL(360.0), h);

    /* Step 8: Eccentricity et */
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));

    /* Step 9: Hue quadrature H */
    alwan_scalar H = cam_hue_to_quadrature_v(h);

    /* Step 10: Lightness J */
    alwan_scalar J = ALWAN_LITERAL(100.0) * ALWAN_POW(A / A_w, c * z);

    /* Step 11: Brightness Q */
    alwan_scalar Q = (ALWAN_LITERAL(4.0) / c) * ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               (A_w + ALWAN_LITERAL(4.0)) * ALWAN_POW(FL, ALWAN_LITERAL(0.25));

    /* Step 12: Chroma C */
    alwan_scalar t_val = (ALWAN_LITERAL(50000.0) / ALWAN_LITERAL(13.0) * Nc * Ncb * et *
                ALWAN_SQRT(a * a + b * b)) /
               (R_a + G_a + ALWAN_LITERAL(21.0) / ALWAN_LITERAL(20.0) * B_a);
    alwan_scalar C = ALWAN_POW(t_val, ALWAN_LITERAL(0.9)) *
               ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73));

    /* Step 13: Colorfulness M and saturation s */
    alwan_scalar M = C * ALWAN_POW(FL, ALWAN_LITERAL(0.25));
    alwan_scalar s = ALWAN_LITERAL(100.0) * ALWAN_SQRT(M / Q);

    result.J = J;
    result.C = C;
    result.h = h;
    result.s = s;
    result.Q = Q;
    result.M = M;
    result.H = H;

    return result;
}

/* ================================================================
 * CAM16 Inverse Transform (value-returning)
 * ================================================================ */

ALWAN_INLINE alwan_xyz alwan_cam16_inverse_v(
    alwan_scalar J,
    alwan_scalar C,
    alwan_scalar h,
    alwan_xyz white_xyz,
    alwan_scalar F,
    alwan_scalar c,
    alwan_scalar Nc,
    alwan_scalar D,
    alwan_scalar FL,
    alwan_scalar n,
    alwan_scalar Nbb,
    alwan_scalar Ncb,
    alwan_scalar z,
    alwan_scalar A_w
) {
    alwan_xyz result;
    (void)F; /* F already folded into pre-computed D */

    /* Step 1: Compute achromatic response A from J */
    alwan_scalar A = A_w * ALWAN_POW(J / ALWAN_LITERAL(100.0), ALWAN_ONE / (c * z));

    /* Step 2: Compute t from C */
    alwan_scalar t_val = ALWAN_POW(
        C / (ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
             ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73))),
        ALWAN_ONE / ALWAN_LITERAL(0.9));

    /* Step 3: Eccentricity et */
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));

    /* Step 4: Compute opponent dimensions a, b via helper */
    alwan_scalar h_rad = h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_vec2 ab = cam_opponent_colour_inverse_v(t_val, h_rad, Nc, Ncb, et, A, Nbb);
    alwan_scalar a = ab.v[0];
    alwan_scalar b = ab.v[1];

    /* Step 5: Compute adapted signals RGB_a from a, b, A */
    alwan_scalar P_2 = A / Nbb + ALWAN_LITERAL(0.305);
    alwan_scalar R_a = (ALWAN_LITERAL(460.0) * P_2 + ALWAN_LITERAL(451.0) * a + ALWAN_LITERAL(288.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar G_a = (ALWAN_LITERAL(460.0) * P_2 - ALWAN_LITERAL(891.0) * a - ALWAN_LITERAL(261.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar B_a = (ALWAN_LITERAL(460.0) * P_2 - ALWAN_LITERAL(220.0) * a - ALWAN_LITERAL(6300.0) * b) / ALWAN_LITERAL(1403.0);

    /* Step 6: Inverse post-adaptation nonlinearity */
    alwan_scalar R_c = ALWAN_LITERAL(100.0) / FL * cam_post_adaptation_nonlinear_inv_v(R_a);
    alwan_scalar G_c = ALWAN_LITERAL(100.0) / FL * cam_post_adaptation_nonlinear_inv_v(G_a);
    alwan_scalar B_c = ALWAN_LITERAL(100.0) / FL * cam_post_adaptation_nonlinear_inv_v(B_a);

    /* Step 7: Inverse chromatic adaptation */
    alwan_scalar R_w = CAM_M_CAT16[0] * white_xyz.x + CAM_M_CAT16[1] * white_xyz.y + CAM_M_CAT16[2] * white_xyz.z;
    alwan_scalar G_w = CAM_M_CAT16[3] * white_xyz.x + CAM_M_CAT16[4] * white_xyz.y + CAM_M_CAT16[5] * white_xyz.z;
    alwan_scalar B_w = CAM_M_CAT16[6] * white_xyz.x + CAM_M_CAT16[7] * white_xyz.y + CAM_M_CAT16[8] * white_xyz.z;

    alwan_scalar D_R = D * (white_xyz.y / R_w) + ALWAN_ONE - D;
    alwan_scalar D_G = D * (white_xyz.y / G_w) + ALWAN_ONE - D;
    alwan_scalar D_B = D * (white_xyz.y / B_w) + ALWAN_ONE - D;

    alwan_scalar R = R_c / D_R;
    alwan_scalar G = G_c / D_G;
    alwan_scalar B = B_c / D_B;

    /* Step 8: LMS -> XYZ via inverse CAT16 matrix */
    result.x = CAM_M_CAT16_INV[0] * R + CAM_M_CAT16_INV[1] * G + CAM_M_CAT16_INV[2] * B;
    result.y = CAM_M_CAT16_INV[3] * R + CAM_M_CAT16_INV[4] * G + CAM_M_CAT16_INV[5] * B;
    result.z = CAM_M_CAT16_INV[6] * R + CAM_M_CAT16_INV[7] * G + CAM_M_CAT16_INV[8] * B;

    return result;
}

/* ================================================================
 * CAM16-UCS Forward Transform (JMh -> Jab) (value-returning)
 * ================================================================ */

ALWAN_INLINE alwan_cam_jab alwan_cam16_to_ucs_v(
    alwan_scalar J,
    alwan_scalar M,
    alwan_scalar h
) {
    alwan_cam_jab result;

    alwan_scalar J_prime = ALWAN_LITERAL(1.7) * J / (ALWAN_ONE + ALWAN_LITERAL(0.007) * J);
    alwan_scalar M_prime = ALWAN_ONE / ALWAN_LITERAL(0.0228) * ALWAN_LN(ALWAN_ONE + ALWAN_LITERAL(0.0228) * M);

    alwan_scalar h_rad = h * ALWAN_PI / ALWAN_LITERAL(180.0);
    result.J = J_prime;
    result.a = M_prime * ALWAN_COS(h_rad);
    result.b = M_prime * ALWAN_SIN(h_rad);

    return result;
}

/* ================================================================
 * CAM16-UCS Inverse Transform (Jab -> JMh) (value-returning)
 * Returns a cam16_v_correlates with only J, M, h populated;
 * C, s, Q, H are set to zero.
 * ================================================================ */

ALWAN_INLINE alwan_cam16_v_correlates alwan_cam16_from_ucs_v(
    alwan_scalar J_prime,
    alwan_scalar a_prime,
    alwan_scalar b_prime
) {
    alwan_cam16_v_correlates result;

    alwan_scalar J = J_prime / (ALWAN_LITERAL(1.7) - ALWAN_LITERAL(0.007) * J_prime);
    alwan_scalar M_prime = ALWAN_SQRT(a_prime * a_prime + b_prime * b_prime);
    alwan_scalar M = (ALWAN_EXP(M_prime * ALWAN_LITERAL(0.0228)) - ALWAN_ONE) / ALWAN_LITERAL(0.0228);
    alwan_scalar h_rad = ALWAN_ATAN2(b_prime, a_prime);
    alwan_scalar h = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    h = ALWAN_SELECT(h < ALWAN_ZERO, h + ALWAN_LITERAL(360.0), h);

    result.J = J;
    result.C = ALWAN_ZERO;
    result.h = h;
    result.s = ALWAN_ZERO;
    result.Q = ALWAN_ZERO;
    result.M = M;
    result.H = ALWAN_ZERO;

    return result;
}

#endif /* ALWAN_CAM_CORE_H */
