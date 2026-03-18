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
    alwan_scalar h_norm = ALWAN_FMOD(h, ALWAN_LITERAL(360.0));
    h_norm = ALWAN_SELECT(h_norm < ALWAN_ZERO, h_norm + ALWAN_LITERAL(360.0), h_norm);
    alwan_scalar hp = ALWAN_SELECT(h_norm < ALWAN_LITERAL(20.14),
                                   h_norm + ALWAN_LITERAL(360.0), h_norm);
    alwan_scalar h_diff_0 = hp - CAM_HQ_h_i[0];
    alwan_scalar hi_diff_0 = CAM_HQ_h_i[1] - CAM_HQ_h_i[0];
    alwan_scalar H_0 = CAM_HQ_H_i[0] + (ALWAN_LITERAL(100.0) * h_diff_0 * CAM_HQ_e_i[0]) /
                        (h_diff_0 * CAM_HQ_e_i[0] + (hi_diff_0 - h_diff_0) * CAM_HQ_e_i[1]);
    alwan_scalar h_diff_1 = hp - CAM_HQ_h_i[1];
    alwan_scalar hi_diff_1 = CAM_HQ_h_i[2] - CAM_HQ_h_i[1];
    alwan_scalar H_1 = CAM_HQ_H_i[1] + (ALWAN_LITERAL(100.0) * h_diff_1 * CAM_HQ_e_i[1]) /
                        (h_diff_1 * CAM_HQ_e_i[1] + (hi_diff_1 - h_diff_1) * CAM_HQ_e_i[2]);
    alwan_scalar h_diff_2 = hp - CAM_HQ_h_i[2];
    alwan_scalar hi_diff_2 = CAM_HQ_h_i[3] - CAM_HQ_h_i[2];
    alwan_scalar H_2 = CAM_HQ_H_i[2] + (ALWAN_LITERAL(100.0) * h_diff_2 * CAM_HQ_e_i[2]) /
                        (h_diff_2 * CAM_HQ_e_i[2] + (hi_diff_2 - h_diff_2) * CAM_HQ_e_i[3]);
    alwan_scalar h_diff_3 = hp - CAM_HQ_h_i[3];
    alwan_scalar hi_diff_3 = CAM_HQ_h_i[4] - CAM_HQ_h_i[3];
    alwan_scalar H_3 = CAM_HQ_H_i[3] + (ALWAN_LITERAL(100.0) * h_diff_3 * CAM_HQ_e_i[3]) /
                        (h_diff_3 * CAM_HQ_e_i[3] + (hi_diff_3 - h_diff_3) * CAM_HQ_e_i[4]);
    alwan_scalar H_val = ALWAN_SELECT(hp < CAM_HQ_h_i[1], H_0,
                         ALWAN_SELECT(hp < CAM_HQ_h_i[2], H_1,
                         ALWAN_SELECT(hp < CAM_HQ_h_i[3], H_2, H_3)));
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
    alwan_scalar P_4 = P_1 / sin_h;
    alwan_scalar denom_b = P_4 +
                          (ALWAN_LITERAL(2.0) + P_3) * (ALWAN_LITERAL(220.0) / ALWAN_LITERAL(1403.0)) * (cos_h / sin_h) -
                          (ALWAN_LITERAL(27.0) / ALWAN_LITERAL(1403.0)) +
                          P_3 * (ALWAN_LITERAL(6300.0) / ALWAN_LITERAL(1403.0));
    alwan_scalar b_case1 = numerator / denom_b;
    alwan_scalar a_case1 = b_case1 * (cos_h / sin_h);
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
    ALWAN_UNUSED(F);
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 lms = alwan_mat3_mulv_v(CAM_M_CAT16, xyz_v);
    alwan_vec3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    alwan_vec3 lms_w = alwan_mat3_mulv_v(CAM_M_CAT16, white_v);
    alwan_scalar D_R = D * (white_xyz.y / lms_w.v[0]) + ALWAN_ONE - D;
    alwan_scalar D_G = D * (white_xyz.y / lms_w.v[1]) + ALWAN_ONE - D;
    alwan_scalar D_B = D * (white_xyz.y / lms_w.v[2]) + ALWAN_ONE - D;
    alwan_scalar R_c = lms.v[0] * D_R;
    alwan_scalar G_c = lms.v[1] * D_G;
    alwan_scalar B_c = lms.v[2] * D_B;
    alwan_scalar R_a = cam_post_adaptation_nonlinear_v(FL * R_c / ALWAN_LITERAL(100.0));
    alwan_scalar G_a = cam_post_adaptation_nonlinear_v(FL * G_c / ALWAN_LITERAL(100.0));
    alwan_scalar B_a = cam_post_adaptation_nonlinear_v(FL * B_c / ALWAN_LITERAL(100.0));
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

ALWAN_INLINE alwan_xyz alwan_cam16_inverse_v(
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
    alwan_scalar R_c = ALWAN_LITERAL(100.0) / FL * cam_post_adaptation_nonlinear_inv_v(R_a);
    alwan_scalar G_c = ALWAN_LITERAL(100.0) / FL * cam_post_adaptation_nonlinear_inv_v(G_a);
    alwan_scalar B_c = ALWAN_LITERAL(100.0) / FL * cam_post_adaptation_nonlinear_inv_v(B_a);
    alwan_vec3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    alwan_vec3 lms_w = alwan_mat3_mulv_v(CAM_M_CAT16, white_v);
    alwan_scalar D_R = D * (white_xyz.y / lms_w.v[0]) + ALWAN_ONE - D;
    alwan_scalar D_G = D * (white_xyz.y / lms_w.v[1]) + ALWAN_ONE - D;
    alwan_scalar D_B = D * (white_xyz.y / lms_w.v[2]) + ALWAN_ONE - D;
    alwan_vec3 lms_inv = {{R_c / D_R, G_c / D_G, B_c / D_B}};
    alwan_vec3 xyz_out = alwan_mat3_mulv_v(CAM_M_CAT16_INV, lms_inv);
    result.x = xyz_out.v[0]; result.y = xyz_out.v[1]; result.z = xyz_out.v[2];
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

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_CAM_CORE_H */
