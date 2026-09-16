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

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_math_core.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_cam_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_cam_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* GPU backends - original code */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

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

/* Hunt-Pointer-Estevez (HPE) matrix for CIECAM02 opponent signals */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 CAM_M_HPE = {{
#include "../data/matrices/hpe.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CAM_M_HPE_INV = {{
#include "../data/matrices/hpe_inv.csv"
}};
/* CAT02 matrix for CIECAM02 chromatic adaptation */
ALWAN_CONSTEXPR alwan_mat3x3 CAM_M_CAT02 = {{
#include "../data/matrices/cat_cat02.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CAM_M_CAT02_INV = {{
#include "../data/matrices/cat_cat02_inv.csv"
}};
/* Precomputed M_HPE * M_CAT02^-1 for CIECAM02 (matches colour-science) */
ALWAN_CONSTEXPR alwan_mat3x3 CAM_M_HPE_CAT02_INV = {{
#include "../data/matrices/hpe_cat02_inv.csv"
}};
/* Inverse: M_CAT02 * M_HPE^-1 */
ALWAN_CONSTEXPR alwan_mat3x3 CAM_M_CAT02_HPE_INV = {{
#include "../data/matrices/hpe_cat02_inv_inv.csv"
}};
/* CAT16 matrix for CAM16 XYZ <-> LMS */
ALWAN_CONSTEXPR alwan_mat3x3 CAM_M_CAT16 = {{
#include "../data/matrices/cat_cat16.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CAM_M_CAT16_INV = {{
#include "../data/matrices/cat_cat16_inv.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Hue Quadrature Data (shared by CIECAM02 and CAM16)
 * ---------------------------------------------------------------- */

ALWAN_CONSTEXPR alwan_scalar CAM_HQ_h_i[5] = {
    ALWAN_LITERAL(20.14), ALWAN_LITERAL(90.00),
    ALWAN_LITERAL(164.25), ALWAN_LITERAL(237.53),
    ALWAN_LITERAL(380.14)
};

ALWAN_CONSTEXPR alwan_scalar CAM_HQ_e_i[5] = {
    ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.7),
    ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.2),
    ALWAN_LITERAL(0.8)  /* wraparound: e_i[4] == e_i[0] */
};

ALWAN_CONSTEXPR alwan_scalar CAM_HQ_H_i[5] = {
    ALWAN_LITERAL(0.0), ALWAN_LITERAL(100.0),
    ALWAN_LITERAL(200.0), ALWAN_LITERAL(300.0),
    ALWAN_LITERAL(400.0)
};

ALWAN_INLINE alwan_scalar cam_compute_D_v(
    alwan_scalar F, alwan_scalar La, alwan_scalar discount_illuminant) {
    alwan_scalar D_computed = F * (ALWAN_ONE - ALWAN_ONE / ALWAN_LITERAL(3.6) *
                     ALWAN_EXP((-La - ALWAN_LITERAL(42.0)) / ALWAN_LITERAL(92.0)));
    alwan_scalar D_clamped = alwan_clamp(D_computed, ALWAN_ZERO, ALWAN_ONE);
    return ALWAN_SELECT(discount_illuminant > ALWAN_LITERAL(0.5), ALWAN_ONE, D_clamped);
}

ALWAN_INLINE alwan_scalar cam_post_adaptation_nonlinear_v(alwan_scalar x) {
    alwan_scalar x_abs = ALWAN_ABS(x);
    alwan_scalar x_pow = ALWAN_POW(x_abs, ALWAN_LITERAL(0.42));
    alwan_scalar magnitude = ALWAN_LITERAL(400.0) * x_pow / (ALWAN_LITERAL(27.13) + x_pow) + ALWAN_LITERAL(0.1);
    return ALWAN_SELECT(x < ALWAN_ZERO, -magnitude, magnitude);
}

ALWAN_INLINE alwan_scalar cam_post_adaptation_nonlinear_inv_v(alwan_scalar x) {
    alwan_scalar y = x - ALWAN_LITERAL(0.1);
    alwan_scalar y_abs = ALWAN_ABS(y);
    alwan_scalar magnitude = ALWAN_POW(
        (ALWAN_LITERAL(27.13) * y_abs) / (ALWAN_LITERAL(400.0) - y_abs),
        ALWAN_ONE / ALWAN_LITERAL(0.42));
    return ALWAN_SELECT(y < ALWAN_ZERO, -magnitude, magnitude);
}

typedef struct {
    alwan_scalar D, FL, n, Nbb, Ncb, z, A_w;
} alwan_cam_derived_params;

ALWAN_INLINE alwan_cam_derived_params cam_compute_derived_params_v(
    alwan_scalar F, alwan_scalar La, alwan_scalar Yb, alwan_scalar Yw,
    alwan_scalar discount_illuminant, alwan_xyz white_xyz,
    alwan_mat3x3 M_cat, alwan_mat3x3 M_cat_inv, alwan_mat3x3 M_hpe, int use_hpe) {
    alwan_cam_derived_params p;
    p.D = cam_compute_D_v(F, La, discount_illuminant);
    alwan_scalar k = ALWAN_ONE / (ALWAN_LITERAL(5.0) * La + ALWAN_ONE);
    alwan_scalar k4 = k * k * k * k;
    p.FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * La) +
           ALWAN_LITERAL(0.1) * (ALWAN_ONE - k4) * (ALWAN_ONE - k4) *
           ALWAN_POW(ALWAN_LITERAL(5.0) * La, ALWAN_ONE / ALWAN_LITERAL(3.0));
    p.n   = Yb / Yw;
    p.Nbb = ALWAN_LITERAL(0.725) * ALWAN_POW(ALWAN_ONE / p.n, ALWAN_LITERAL(0.2));
    p.Ncb = p.Nbb;
    p.z   = ALWAN_LITERAL(1.48) + ALWAN_SQRT(p.n);
    alwan_vec3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    alwan_vec3 lms_w = alwan_mat3_mulv_v(M_cat, white_v);
    alwan_scalar D_R_w = p.D * (Yw / lms_w.v[0]) + ALWAN_ONE - p.D;
    alwan_scalar D_G_w = p.D * (Yw / lms_w.v[1]) + ALWAN_ONE - p.D;
    alwan_scalar D_B_w = p.D * (Yw / lms_w.v[2]) + ALWAN_ONE - p.D;
    alwan_vec3 rgb_cw = {{lms_w.v[0] * D_R_w, lms_w.v[1] * D_G_w, lms_w.v[2] * D_B_w}};
    alwan_vec3 hpe_w = rgb_cw;
    if (use_hpe) {
        alwan_vec3 xyz_cw = alwan_mat3_mulv_v(M_cat_inv, rgb_cw);
        hpe_w = alwan_mat3_mulv_v(M_hpe, xyz_cw);
    }
    alwan_scalar R_aw = cam_post_adaptation_nonlinear_v(p.FL * hpe_w.v[0] / ALWAN_LITERAL(100.0));
    alwan_scalar G_aw = cam_post_adaptation_nonlinear_v(p.FL * hpe_w.v[1] / ALWAN_LITERAL(100.0));
    alwan_scalar B_aw = cam_post_adaptation_nonlinear_v(p.FL * hpe_w.v[2] / ALWAN_LITERAL(100.0));
    p.A_w = (ALWAN_LITERAL(2.0) * R_aw + G_aw +
             ALWAN_LITERAL(0.05) * B_aw - ALWAN_LITERAL(0.305)) * p.Nbb;
    return p;
}

ALWAN_INLINE alwan_scalar cam_hue_to_quadrature_v(alwan_scalar h) {
    /* Normalize h to [0, 360) */
    alwan_scalar h_norm = ALWAN_FMOD(h, ALWAN_LITERAL(360.0));
    h_norm = ALWAN_SELECT(h_norm < ALWAN_ZERO, h_norm + ALWAN_LITERAL(360.0), h_norm);

    /* CIE 159:2004 interpolates on (h - h_i) / e_i. This divided by e_i until
     * 2026-09-16, when it multiplied instead, and the two end sectors were a
     * single wrapped sector rather than the two the standard gives. Nothing
     * caught it: suites 13 and 14 load seven correlates per colour and compare
     * three, so H had never been checked against anything.
     *
     * Below the first tabulated hue, H runs from 385.9 to 400 on its own
     * coefficients; at and above the last, the sector closes on 360 with
     * e = 0.856 and a span of 85.9. The two meet: H(360) = 385.9 = H(0), and
     * H(20.14) = 400, which is the same red the table opens at. */
    alwan_scalar t_low = h_norm / ALWAN_LITERAL(0.856);
    alwan_scalar H_low = ALWAN_LITERAL(385.9) +
        (ALWAN_LITERAL(14.1) * t_low) /
        (t_low + (ALWAN_LITERAL(20.14) - h_norm) / ALWAN_LITERAL(0.8));

    /* Sector 0: h_i[0]=20.14 .. h_i[1]=90.00, e_i[0]=0.8, e_i[1]=0.7 */
    alwan_scalar d_0 = (h_norm - CAM_HQ_h_i[0]) / CAM_HQ_e_i[0];
    alwan_scalar H_0 = CAM_HQ_H_i[0] + (ALWAN_LITERAL(100.0) * d_0) /
                        (d_0 + (CAM_HQ_h_i[1] - h_norm) / CAM_HQ_e_i[1]);

    /* Sector 1: h_i[1]=90.00 .. h_i[2]=164.25, e_i[1]=0.7, e_i[2]=1.0 */
    alwan_scalar d_1 = (h_norm - CAM_HQ_h_i[1]) / CAM_HQ_e_i[1];
    alwan_scalar H_1 = CAM_HQ_H_i[1] + (ALWAN_LITERAL(100.0) * d_1) /
                        (d_1 + (CAM_HQ_h_i[2] - h_norm) / CAM_HQ_e_i[2]);

    /* Sector 2: h_i[2]=164.25 .. h_i[3]=237.53, e_i[2]=1.0, e_i[3]=1.2 */
    alwan_scalar d_2 = (h_norm - CAM_HQ_h_i[2]) / CAM_HQ_e_i[2];
    alwan_scalar H_2 = CAM_HQ_H_i[2] + (ALWAN_LITERAL(100.0) * d_2) /
                        (d_2 + (CAM_HQ_h_i[3] - h_norm) / CAM_HQ_e_i[3]);

    /* Sector 3: h_i[3]=237.53 .. 360, e_i[3]=1.2, closing on 0.856 */
    alwan_scalar d_3 = (h_norm - CAM_HQ_h_i[3]) / CAM_HQ_e_i[3];
    alwan_scalar H_3 = CAM_HQ_H_i[3] + (ALWAN_LITERAL(85.9) * d_3) /
                        (d_3 + (ALWAN_LITERAL(360.0) - h_norm) / ALWAN_LITERAL(0.856));

    /* Nested ALWAN_SELECT to choose the correct sector */
    alwan_scalar H_val = ALWAN_SELECT(h_norm < CAM_HQ_h_i[0], H_low,
                         ALWAN_SELECT(h_norm < CAM_HQ_h_i[1], H_0,
                         ALWAN_SELECT(h_norm < CAM_HQ_h_i[2], H_1,
                         ALWAN_SELECT(h_norm < CAM_HQ_h_i[3], H_2,
                                                           H_3))));

    return H_val;
}

ALWAN_INLINE alwan_vec2 cam_opponent_colour_inverse_v(
    alwan_scalar t, alwan_scalar h_rad, alwan_scalar Nc, alwan_scalar Ncb,
    alwan_scalar et, alwan_scalar A, alwan_scalar Nbb) {
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

    int use_sin_path = ALWAN_ABS(sin_h) >= ALWAN_ABS(cos_h);
    result.v[0] = ALWAN_SELECT(use_sin_path, a_case1, a_case2);
    result.v[1] = ALWAN_SELECT(use_sin_path, b_case1, b_case2);

    return result;
}

ALWAN_INLINE alwan_ciecam02_v_correlates alwan_ciecam02_forward_v(
    alwan_xyz xyz, alwan_xyz white_xyz,
    alwan_scalar F, alwan_scalar c, alwan_scalar Nc, alwan_scalar D,
    alwan_scalar FL, alwan_scalar n, alwan_scalar Nbb, alwan_scalar Ncb,
    alwan_scalar z, alwan_scalar A_w) {
    alwan_ciecam02_v_correlates result;
    ALWAN_UNUSED(F);
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 rgb_cat = alwan_mat3_mulv_v(CAM_M_CAT02, xyz_v);
    alwan_vec3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    alwan_vec3 rgb_cat_w = alwan_mat3_mulv_v(CAM_M_CAT02, white_v);
    alwan_scalar D_R = D * (white_xyz.y / rgb_cat_w.v[0]) + ALWAN_ONE - D;
    alwan_scalar D_G = D * (white_xyz.y / rgb_cat_w.v[1]) + ALWAN_ONE - D;
    alwan_scalar D_B = D * (white_xyz.y / rgb_cat_w.v[2]) + ALWAN_ONE - D;
    alwan_vec3 rgb_c = {{rgb_cat.v[0] * D_R, rgb_cat.v[1] * D_G, rgb_cat.v[2] * D_B}};
    alwan_vec3 hpe = alwan_mat3_mulv_v(CAM_M_HPE_CAT02_INV, rgb_c);
    alwan_scalar R_a = cam_post_adaptation_nonlinear_v(FL * hpe.v[0] / ALWAN_LITERAL(100.0));
    alwan_scalar G_a = cam_post_adaptation_nonlinear_v(FL * hpe.v[1] / ALWAN_LITERAL(100.0));
    alwan_scalar B_a = cam_post_adaptation_nonlinear_v(FL * hpe.v[2] / ALWAN_LITERAL(100.0));
    alwan_scalar A = (ALWAN_LITERAL(2.0) * R_a + G_a +
                ALWAN_LITERAL(0.05) * B_a - ALWAN_LITERAL(0.305)) * Nbb;
    alwan_scalar a = R_a - ALWAN_LITERAL(12.0) * G_a / ALWAN_LITERAL(11.0) + B_a / ALWAN_LITERAL(11.0);
    alwan_scalar b = (R_a + G_a - ALWAN_LITERAL(2.0) * B_a) / ALWAN_LITERAL(9.0);
    alwan_scalar h_rad = ALWAN_ATAN2(b, a);
    alwan_scalar h = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    h = ALWAN_SELECT(h < ALWAN_ZERO, h + ALWAN_LITERAL(360.0), h);
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));
    alwan_scalar H = cam_hue_to_quadrature_v(h);
    alwan_scalar J = ALWAN_LITERAL(100.0) * ALWAN_POW(A / A_w, c * z);
    alwan_scalar Q = (ALWAN_LITERAL(4.0) / c) * ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               (A_w + ALWAN_LITERAL(4.0)) * ALWAN_POW(FL, ALWAN_LITERAL(0.25));
    alwan_scalar t_val = (ALWAN_LITERAL(50000.0) / ALWAN_LITERAL(13.0) * Nc * Ncb * et *
                ALWAN_SQRT(a * a + b * b)) /
               (R_a + G_a + ALWAN_LITERAL(21.0) / ALWAN_LITERAL(20.0) * B_a);
    alwan_scalar C_val = ALWAN_POW(t_val, ALWAN_LITERAL(0.9)) *
               ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73));
    alwan_scalar M = C_val * ALWAN_POW(FL, ALWAN_LITERAL(0.25));
    alwan_scalar s = ALWAN_LITERAL(100.0) * ALWAN_SQRT(M / Q);
    result.J = J; result.C = C_val; result.h = h; result.s = s;
    result.Q = Q; result.M = M; result.H = H;
    return result;
}

ALWAN_INLINE alwan_xyz alwan_ciecam02_inverse_v(
    alwan_scalar J, alwan_scalar C, alwan_scalar h, alwan_xyz white_xyz,
    alwan_scalar F, alwan_scalar c, alwan_scalar Nc, alwan_scalar D,
    alwan_scalar FL, alwan_scalar n, alwan_scalar Nbb, alwan_scalar Ncb,
    alwan_scalar z, alwan_scalar A_w) {
    alwan_xyz result;
    ALWAN_UNUSED(F);
    alwan_scalar A = A_w * ALWAN_POW(J / ALWAN_LITERAL(100.0), ALWAN_ONE / (c * z));
    alwan_scalar t_val = ALWAN_POW(
        C / (ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
             ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73))),
        ALWAN_ONE / ALWAN_LITERAL(0.9));
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));
    alwan_scalar h_rad = h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_vec2 ab = cam_opponent_colour_inverse_v(t_val, h_rad, Nc, Ncb, et, A, Nbb);
    alwan_scalar a = ab.v[0];
    alwan_scalar b = ab.v[1];
    alwan_scalar P_2 = A / Nbb + ALWAN_LITERAL(0.305);
    alwan_scalar R_a = (ALWAN_LITERAL(460.0) * P_2 + ALWAN_LITERAL(451.0) * a + ALWAN_LITERAL(288.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar G_a = (ALWAN_LITERAL(460.0) * P_2 - ALWAN_LITERAL(891.0) * a - ALWAN_LITERAL(261.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar B_a = (ALWAN_LITERAL(460.0) * P_2 - ALWAN_LITERAL(220.0) * a - ALWAN_LITERAL(6300.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar R_hpe = ALWAN_LITERAL(100.0) / FL * cam_post_adaptation_nonlinear_inv_v(R_a);
    alwan_scalar G_hpe = ALWAN_LITERAL(100.0) / FL * cam_post_adaptation_nonlinear_inv_v(G_a);
    alwan_scalar B_hpe = ALWAN_LITERAL(100.0) / FL * cam_post_adaptation_nonlinear_inv_v(B_a);
    alwan_vec3 hpe_v = {{R_hpe, G_hpe, B_hpe}};
    alwan_vec3 rgb_c = alwan_mat3_mulv_v(CAM_M_CAT02_HPE_INV, hpe_v);
    alwan_vec3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    alwan_vec3 rgb_cat_w = alwan_mat3_mulv_v(CAM_M_CAT02, white_v);
    alwan_scalar D_R = D * (white_xyz.y / rgb_cat_w.v[0]) + ALWAN_ONE - D;
    alwan_scalar D_G = D * (white_xyz.y / rgb_cat_w.v[1]) + ALWAN_ONE - D;
    alwan_scalar D_B = D * (white_xyz.y / rgb_cat_w.v[2]) + ALWAN_ONE - D;
    alwan_vec3 rgb_cat = {{rgb_c.v[0] / D_R, rgb_c.v[1] / D_G, rgb_c.v[2] / D_B}};
    alwan_vec3 xyz_out = alwan_mat3_mulv_v(CAM_M_CAT02_INV, rgb_cat);
    result.x = xyz_out.v[0]; result.y = xyz_out.v[1]; result.z = xyz_out.v[2];
    return result;
}

ALWAN_INLINE alwan_cam16_v_correlates alwan_cam16_forward_v(
    alwan_xyz xyz, alwan_xyz white_xyz,
    alwan_scalar F, alwan_scalar c, alwan_scalar Nc, alwan_scalar D,
    alwan_scalar FL, alwan_scalar n, alwan_scalar Nbb, alwan_scalar Ncb,
    alwan_scalar z, alwan_scalar A_w) {
    alwan_cam16_v_correlates result;
    ALWAN_UNUSED(F); /* F already folded into pre-computed D */

    /* Step 1: XYZ -> LMS via CAT16 matrix (stimulus) */
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 lms = alwan_mat3_mulv_v(CAM_M_CAT16, xyz_v);
    alwan_scalar R = lms.v[0];
    alwan_scalar G = lms.v[1];
    alwan_scalar B = lms.v[2];

    /* Step 2: XYZ -> LMS via CAT16 matrix (white point) */
    alwan_vec3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    alwan_vec3 lms_w = alwan_mat3_mulv_v(CAM_M_CAT16, white_v);
    alwan_scalar R_w = lms_w.v[0];
    alwan_scalar G_w = lms_w.v[1];
    alwan_scalar B_w = lms_w.v[2];

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

    /* Step 8: Eccentricity et (match colour-science: radians(h) + 2) */
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
    alwan_scalar C_val = ALWAN_POW(t_val, ALWAN_LITERAL(0.9)) *
               ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73));

    /* Step 13: Colorfulness M and saturation s */
    alwan_scalar M = C_val * ALWAN_POW(FL, ALWAN_LITERAL(0.25));
    alwan_scalar s = ALWAN_LITERAL(100.0) * ALWAN_SQRT(M / Q);

    result.J = J;
    result.C = C_val;
    result.h = h;
    result.s = s;
    result.Q = Q;
    result.M = M;
    result.H = H;

    return result;
}

ALWAN_INLINE alwan_xyz alwan_cam16_inverse_v(
    alwan_scalar J, alwan_scalar C, alwan_scalar h, alwan_xyz white_xyz,
    alwan_scalar F, alwan_scalar c, alwan_scalar Nc, alwan_scalar D,
    alwan_scalar FL, alwan_scalar n, alwan_scalar Nbb, alwan_scalar Ncb,
    alwan_scalar z, alwan_scalar A_w) {
    alwan_xyz result;
    ALWAN_UNUSED(F); /* F already folded into pre-computed D */

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
    alwan_vec3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    alwan_vec3 lms_w = alwan_mat3_mulv_v(CAM_M_CAT16, white_v);
    alwan_scalar R_w = lms_w.v[0];
    alwan_scalar G_w = lms_w.v[1];
    alwan_scalar B_w = lms_w.v[2];

    alwan_scalar D_R = D * (white_xyz.y / R_w) + ALWAN_ONE - D;
    alwan_scalar D_G = D * (white_xyz.y / G_w) + ALWAN_ONE - D;
    alwan_scalar D_B = D * (white_xyz.y / B_w) + ALWAN_ONE - D;

    alwan_scalar R = R_c / D_R;
    alwan_scalar G = G_c / D_G;
    alwan_scalar B = B_c / D_B;

    /* Step 8: LMS -> XYZ via inverse CAT16 matrix */
    alwan_vec3 lms_inv = {{R, G, B}};
    alwan_vec3 xyz_out = alwan_mat3_mulv_v(CAM_M_CAT16_INV, lms_inv);
    result.x = xyz_out.v[0];
    result.y = xyz_out.v[1];
    result.z = xyz_out.v[2];

    return result;
}

ALWAN_INLINE alwan_cam_jab alwan_cam16_to_ucs_v(
    alwan_scalar J, alwan_scalar M, alwan_scalar h) {
    alwan_cam_jab result;
    alwan_scalar J_prime = ALWAN_LITERAL(1.7) * J / (ALWAN_ONE + ALWAN_LITERAL(0.007) * J);
    alwan_scalar M_prime = ALWAN_ONE / ALWAN_LITERAL(0.0228) * ALWAN_LN(ALWAN_ONE + ALWAN_LITERAL(0.0228) * M);
    alwan_scalar h_rad = h * ALWAN_PI / ALWAN_LITERAL(180.0);
    result.J = J_prime;
    result.a = M_prime * ALWAN_COS(h_rad);
    result.b = M_prime * ALWAN_SIN(h_rad);
    return result;
}

ALWAN_INLINE alwan_cam16_v_correlates alwan_cam16_from_ucs_v(
    alwan_scalar J_prime, alwan_scalar a_prime, alwan_scalar b_prime) {
    alwan_cam16_v_correlates result;
    alwan_scalar J = J_prime / (ALWAN_LITERAL(1.7) - ALWAN_LITERAL(0.007) * J_prime);
    alwan_scalar M_prime = ALWAN_SQRT(a_prime * a_prime + b_prime * b_prime);
    alwan_scalar M = (ALWAN_EXP(M_prime * ALWAN_LITERAL(0.0228)) - ALWAN_ONE) / ALWAN_LITERAL(0.0228);
    alwan_scalar h_rad = ALWAN_ATAN2(b_prime, a_prime);
    alwan_scalar h = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    h = ALWAN_SELECT(h < ALWAN_ZERO, h + ALWAN_LITERAL(360.0), h);
    result.J = J; result.C = ALWAN_ZERO; result.h = h; result.s = ALWAN_ZERO;
    result.Q = ALWAN_ZERO; result.M = M; result.H = ALWAN_ZERO;
    return result;
}

/* ================================================================
 * CIE 248:2022 post-adaptation compression
 *
 * CAM16 compresses with a bare power curve, which climbs without bound and
 * turns very steep near zero. CIE 248:2022 keeps that curve between q_L and
 * q_U and replaces it outside them: a ray through the origin below q_L, and
 * the curve's own tangent above q_U. Between the two joins it is the same
 * curve CAM16 uses, less the 0.1 the caller adds back, so the two models
 * agree exactly on any signal that lands inside the window.
 *
 * f_q takes the unscaled signal q and the luminance factor separately,
 * because the thresholds are on q itself, not on the scaled argument the
 * CAM16 helper takes.
 * ================================================================ */

ALWAN_INLINE alwan_scalar cam_ciecam16_f_q_v(alwan_scalar FL, alwan_scalar q) {
    alwan_scalar s = ALWAN_POW(FL * q / ALWAN_LITERAL(100.0), ALWAN_LITERAL(0.42));
    return ALWAN_LITERAL(400.0) * s / (ALWAN_LITERAL(27.13) + s);
}

/* Slope of f_q at q. The 1.68 folds in both the 0.42 exponent and the 1/100. */
ALWAN_INLINE alwan_scalar cam_ciecam16_d_f_q_v(alwan_scalar FL, alwan_scalar q) {
    alwan_scalar u = FL * q / ALWAN_LITERAL(100.0);
    alwan_scalar den = ALWAN_LITERAL(27.13) + ALWAN_POW(u, ALWAN_LITERAL(0.42));
    return (ALWAN_LITERAL(1.68) * ALWAN_LITERAL(27.13) * FL *
            ALWAN_POW(u, -ALWAN_LITERAL(0.58))) / (den * den);
}

ALWAN_INLINE alwan_scalar cam_ciecam16_f_e_v(alwan_scalar q, alwan_scalar FL) {
    alwan_scalar q_L = ALWAN_LITERAL(0.26);
    alwan_scalar q_U = ALWAN_LITERAL(150.0);
    alwan_scalar f_L = cam_ciecam16_f_q_v(FL, q_L);
    alwan_scalar f_U = cam_ciecam16_f_q_v(FL, q_U);
    /* The middle branch raises q to a fractional power, so it is evaluated on a
     * clamped q: below the join the result is discarded, and a negative base
     * would otherwise make it a NaN first. */
    alwan_scalar q_mid = ALWAN_SELECT(q < q_L, q_L, q);
    alwan_scalar mid = cam_ciecam16_f_q_v(FL, q_mid);
    alwan_scalar low = f_L * q / q_L;
    alwan_scalar high = f_U + cam_ciecam16_d_f_q_v(FL, q_U) * (q - q_U);
    return ALWAN_SELECT(q > q_U, high, ALWAN_SELECT(q < q_L, low, mid));
}

ALWAN_INLINE alwan_scalar cam_ciecam16_f_e_inv_v(alwan_scalar a, alwan_scalar FL) {
    alwan_scalar q_L = ALWAN_LITERAL(0.26);
    alwan_scalar q_U = ALWAN_LITERAL(150.0);
    alwan_scalar f_L = cam_ciecam16_f_q_v(FL, q_L);
    alwan_scalar f_U = cam_ciecam16_f_q_v(FL, q_U);
    /* Same guard as the forward: the middle branch is evaluated on a clamped
     * value so the discarded branches cannot produce a NaN. */
    alwan_scalar a_mid = ALWAN_SELECT(a < f_L, f_L, ALWAN_SELECT(a > f_U, f_U, a));
    alwan_scalar mid = ALWAN_LITERAL(100.0) / FL * ALWAN_POW(
        (ALWAN_LITERAL(27.13) * a_mid) / (ALWAN_LITERAL(400.0) - a_mid),
        ALWAN_ONE / ALWAN_LITERAL(0.42));
    alwan_scalar low = q_L * a / f_L;
    alwan_scalar high = q_U + (a - f_U) / cam_ciecam16_d_f_q_v(FL, q_U);
    return ALWAN_SELECT(a > f_U, high, ALWAN_SELECT(a < f_L, low, mid));
}

/* ================================================================
 * CIECAM16 derived viewing parameters
 *
 * Differs from cam_compute_derived_params in exactly two places, both of
 * them the model's own: the adaptation term divides by a fixed 100 where
 * CAM16 divides by the white's own Y, and the white's achromatic response
 * goes through the CIE 248:2022 compression. n keeps Yw, as the standard
 * has it. There is no HPE branch: CIECAM16 works in CAT16 space throughout.
 * ================================================================ */

ALWAN_INLINE alwan_cam_derived_params cam_compute_derived_params_ciecam16_v(
    alwan_scalar F,
    alwan_scalar La,
    alwan_scalar Yb,
    alwan_scalar Yw,
    alwan_scalar discount_illuminant,
    alwan_xyz white_xyz,
    alwan_mat3x3 M_cat
) {
    alwan_cam_derived_params p;

    p.D = cam_compute_D_v(F, La, discount_illuminant);

    alwan_scalar k = ALWAN_ONE / (ALWAN_LITERAL(5.0) * La + ALWAN_ONE);
    alwan_scalar k4 = k * k * k * k;
    p.FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * La) +
           ALWAN_LITERAL(0.1) * (ALWAN_ONE - k4) * (ALWAN_ONE - k4) *
           ALWAN_POW(ALWAN_LITERAL(5.0) * La, ALWAN_ONE / ALWAN_LITERAL(3.0));

    p.n   = Yb / Yw;
    p.Nbb = ALWAN_LITERAL(0.725) * ALWAN_POW(ALWAN_ONE / p.n, ALWAN_LITERAL(0.2));
    p.Ncb = p.Nbb;
    p.z   = ALWAN_LITERAL(1.48) + ALWAN_SQRT(p.n);

    alwan_vec3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    alwan_vec3 lms_w = alwan_mat3_mulv_v(M_cat, white_v);

    alwan_scalar D_R_w = p.D * (ALWAN_LITERAL(100.0) / lms_w.v[0]) + ALWAN_ONE - p.D;
    alwan_scalar D_G_w = p.D * (ALWAN_LITERAL(100.0) / lms_w.v[1]) + ALWAN_ONE - p.D;
    alwan_scalar D_B_w = p.D * (ALWAN_LITERAL(100.0) / lms_w.v[2]) + ALWAN_ONE - p.D;

    alwan_scalar R_aw = cam_ciecam16_f_e_v(lms_w.v[0] * D_R_w, p.FL) + ALWAN_LITERAL(0.1);
    alwan_scalar G_aw = cam_ciecam16_f_e_v(lms_w.v[1] * D_G_w, p.FL) + ALWAN_LITERAL(0.1);
    alwan_scalar B_aw = cam_ciecam16_f_e_v(lms_w.v[2] * D_B_w, p.FL) + ALWAN_LITERAL(0.1);

    p.A_w = (ALWAN_LITERAL(2.0) * R_aw + G_aw +
             ALWAN_LITERAL(0.05) * B_aw - ALWAN_LITERAL(0.305)) * p.Nbb;

    return p;
}

/* ================================================================
 * CIECAM16 Forward Transform (CIE 248:2022, value-returning)
 *
 * The correlates are CAM16's, so the CAM16 correlate struct is reused.
 * ================================================================ */

ALWAN_INLINE alwan_cam16_v_correlates alwan_ciecam16_forward_v(
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
    ALWAN_UNUSED(F); /* F already folded into pre-computed D */
    ALWAN_UNUSED(white_xyz); /* the adaptation term is fixed at 100, not the white's Y */

    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 lms = alwan_mat3_mulv_v(CAM_M_CAT16, xyz_v);
    alwan_scalar R = lms.v[0];
    alwan_scalar G = lms.v[1];
    alwan_scalar B = lms.v[2];

    alwan_vec3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    alwan_vec3 lms_w = alwan_mat3_mulv_v(CAM_M_CAT16, white_v);
    alwan_scalar R_w = lms_w.v[0];
    alwan_scalar G_w = lms_w.v[1];
    alwan_scalar B_w = lms_w.v[2];

    /* Chromatic adaptation against a fixed 100, which is where CIECAM16 parts
     * company with CAM16 whenever the white is not on the Y = 100 scale. */
    alwan_scalar D_R = D * (ALWAN_LITERAL(100.0) / R_w) + ALWAN_ONE - D;
    alwan_scalar D_G = D * (ALWAN_LITERAL(100.0) / G_w) + ALWAN_ONE - D;
    alwan_scalar D_B = D * (ALWAN_LITERAL(100.0) / B_w) + ALWAN_ONE - D;

    alwan_scalar R_c = R * D_R;
    alwan_scalar G_c = G * D_G;
    alwan_scalar B_c = B * D_B;

    alwan_scalar R_a = cam_ciecam16_f_e_v(R_c, FL) + ALWAN_LITERAL(0.1);
    alwan_scalar G_a = cam_ciecam16_f_e_v(G_c, FL) + ALWAN_LITERAL(0.1);
    alwan_scalar B_a = cam_ciecam16_f_e_v(B_c, FL) + ALWAN_LITERAL(0.1);

    alwan_scalar A = (ALWAN_LITERAL(2.0) * R_a + G_a +
                ALWAN_LITERAL(0.05) * B_a - ALWAN_LITERAL(0.305)) * Nbb;

    alwan_scalar a = R_a - ALWAN_LITERAL(12.0) * G_a / ALWAN_LITERAL(11.0) +
               B_a / ALWAN_LITERAL(11.0);
    alwan_scalar b = (R_a + G_a - ALWAN_LITERAL(2.0) * B_a) / ALWAN_LITERAL(9.0);

    alwan_scalar h_rad = ALWAN_ATAN2(b, a);
    alwan_scalar h = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    h = ALWAN_SELECT(h < ALWAN_ZERO, h + ALWAN_LITERAL(360.0), h);

    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));

    alwan_scalar H = cam_hue_to_quadrature_v(h);

    alwan_scalar J = ALWAN_LITERAL(100.0) * ALWAN_POW(A / A_w, c * z);

    alwan_scalar Q = (ALWAN_LITERAL(4.0) / c) * ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               (A_w + ALWAN_LITERAL(4.0)) * ALWAN_POW(FL, ALWAN_LITERAL(0.25));

    alwan_scalar t_val = (ALWAN_LITERAL(50000.0) / ALWAN_LITERAL(13.0) * Nc * Ncb * et *
                ALWAN_SQRT(a * a + b * b)) /
               (R_a + G_a + ALWAN_LITERAL(21.0) / ALWAN_LITERAL(20.0) * B_a);
    alwan_scalar C_val = ALWAN_POW(t_val, ALWAN_LITERAL(0.9)) *
               ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73));

    alwan_scalar M = C_val * ALWAN_POW(FL, ALWAN_LITERAL(0.25));
    alwan_scalar s = ALWAN_LITERAL(100.0) * ALWAN_SQRT(M / Q);

    result.J = J;
    result.C = C_val;
    result.h = h;
    result.s = s;
    result.Q = Q;
    result.M = M;
    result.H = H;

    return result;
}

/* ================================================================
 * CIECAM16 Inverse Transform (value-returning)
 * ================================================================ */

ALWAN_INLINE alwan_xyz alwan_ciecam16_inverse_v(
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
    ALWAN_UNUSED(F); /* F already folded into pre-computed D */

    alwan_scalar A = A_w * ALWAN_POW(J / ALWAN_LITERAL(100.0), ALWAN_ONE / (c * z));

    alwan_scalar t_val = ALWAN_POW(
        C / (ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
             ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73))),
        ALWAN_ONE / ALWAN_LITERAL(0.9));

    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));

    alwan_scalar h_rad = h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_vec2 ab = cam_opponent_colour_inverse_v(t_val, h_rad, Nc, Ncb, et, A, Nbb);
    alwan_scalar a = ab.v[0];
    alwan_scalar b = ab.v[1];

    alwan_scalar P_2 = A / Nbb + ALWAN_LITERAL(0.305);
    alwan_scalar R_a = (ALWAN_LITERAL(460.0) * P_2 + ALWAN_LITERAL(451.0) * a + ALWAN_LITERAL(288.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar G_a = (ALWAN_LITERAL(460.0) * P_2 - ALWAN_LITERAL(891.0) * a - ALWAN_LITERAL(261.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar B_a = (ALWAN_LITERAL(460.0) * P_2 - ALWAN_LITERAL(220.0) * a - ALWAN_LITERAL(6300.0) * b) / ALWAN_LITERAL(1403.0);

    /* The inverse compression already returns the unscaled signal, so there is
     * no 100/FL here, unlike the CAM16 inverse. */
    alwan_scalar R_c = cam_ciecam16_f_e_inv_v(R_a - ALWAN_LITERAL(0.1), FL);
    alwan_scalar G_c = cam_ciecam16_f_e_inv_v(G_a - ALWAN_LITERAL(0.1), FL);
    alwan_scalar B_c = cam_ciecam16_f_e_inv_v(B_a - ALWAN_LITERAL(0.1), FL);

    alwan_vec3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    alwan_vec3 lms_w = alwan_mat3_mulv_v(CAM_M_CAT16, white_v);
    alwan_scalar R_w = lms_w.v[0];
    alwan_scalar G_w = lms_w.v[1];
    alwan_scalar B_w = lms_w.v[2];

    alwan_scalar D_R = D * (ALWAN_LITERAL(100.0) / R_w) + ALWAN_ONE - D;
    alwan_scalar D_G = D * (ALWAN_LITERAL(100.0) / G_w) + ALWAN_ONE - D;
    alwan_scalar D_B = D * (ALWAN_LITERAL(100.0) / B_w) + ALWAN_ONE - D;

    alwan_scalar R = R_c / D_R;
    alwan_scalar G = G_c / D_G;
    alwan_scalar B = B_c / D_B;

    alwan_vec3 lms_inv = {{R, G, B}};
    alwan_vec3 xyz_out = alwan_mat3_mulv_v(CAM_M_CAT16_INV, lms_inv);
    result.x = xyz_out.v[0];
    result.y = xyz_out.v[1];
    result.z = xyz_out.v[2];

    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_CAM_CORE_H */
