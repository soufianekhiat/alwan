/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Kim, Weyrich and Kautz (2009) Color Appearance Model
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Kim, M. H., Weyrich, T., & Kautz, J. (2009). Modeling Human Color
 * Perception under Extended Luminance Levels. ACM Transactions on Graphics, 28(3).
 *
 * The _v() functions take all viewing condition parameters as direct scalar
 * arguments instead of structs with enums. The .c wrapper resolves enums to scalars.
 */

#ifndef ALWAN_KIM2009_CORE_H
#define ALWAN_KIM2009_CORE_H

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
#include "alwan_kim2009_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_kim2009_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

typedef struct {
    alwan_scalar J;  /* Lightness */
    alwan_scalar C;  /* Chroma */
    alwan_scalar h;  /* Hue angle (degrees) */
} alwan_kim2009_v_correlates;

#define KIM2009_V_N_C  ALWAN_LITERAL(0.57)
#define KIM2009_V_N_Q  ALWAN_LITERAL(0.1308)
#define KIM2009_V_A_K  ALWAN_LITERAL(456.5)
#define KIM2009_V_N_K  ALWAN_LITERAL(0.62)
#define KIM2009_V_A_M  ALWAN_LITERAL(0.11)
#define KIM2009_V_B_M  ALWAN_LITERAL(0.61)
#define KIM2009_V_A_J  ALWAN_LITERAL(0.89)
#define KIM2009_V_B_J  ALWAN_LITERAL(0.24)
#define KIM2009_V_N_J  ALWAN_LITERAL(3.65)
#define KIM2009_V_O_J  ALWAN_LITERAL(0.65)
#define KIM2009_V_W_L    ALWAN_LITERAL(40.0)
#define KIM2009_V_W_M    ALWAN_LITERAL(20.0)
#define KIM2009_V_W_S    ALWAN_LITERAL(1.0)
#define KIM2009_V_W_SUM  ALWAN_LITERAL(61.0)

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 KIM2009_V_M_CAT02 = {{
#include "../data/matrices/cat_cat02.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 KIM2009_V_M_CAT02_INV = {{
#include "../data/matrices/cat_cat02_inv.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 KIM2009_V_M_HPE = {{
#include "../data/matrices/hpe.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 KIM2009_V_M_HPE_INV = {{
#include "../data/matrices/hpe_inv.csv"
}};
ALWAN_DIAG_POP

/* Sign-preserving power, sign(x) * |x|^p. See the note in the .inc: the cone
 * responses can be negative and a plain pow() of that is NaN, which the forward
 * transform then returned alongside ALWAN_OK. */
ALWAN_INLINE alwan_scalar kim2009_spow_v(alwan_scalar x, alwan_scalar p) {
    alwan_scalar m = ALWAN_POW(ALWAN_ABS(x), p);
    return ALWAN_SELECT(x < ALWAN_ZERO, -m, m);
}
ALWAN_INLINE alwan_kim2009_v_correlates alwan_kim2009_forward_v(
    alwan_xyz xyz, alwan_scalar white_x, alwan_scalar white_y,
    alwan_scalar white_z, alwan_scalar La, alwan_scalar D, alwan_scalar media_E) {
    alwan_kim2009_v_correlates result;
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 rgb_v = alwan_mat3_mulv_v(KIM2009_V_M_CAT02, xyz_v);
    alwan_scalar R = rgb_v.v[0]; alwan_scalar G = rgb_v.v[1]; alwan_scalar B = rgb_v.v[2];
    alwan_vec3 white_v = {{white_x, white_y, white_z}};
    alwan_vec3 rgbw_v = alwan_mat3_mulv_v(KIM2009_V_M_CAT02, white_v);
    alwan_scalar R_w = rgbw_v.v[0]; alwan_scalar G_w = rgbw_v.v[1]; alwan_scalar B_w = rgbw_v.v[2];
    alwan_scalar fac_R = D * (white_y / R_w) + (ALWAN_ONE - D);
    alwan_scalar fac_G = D * (white_y / G_w) + (ALWAN_ONE - D);
    alwan_scalar fac_B = D * (white_y / B_w) + (ALWAN_ONE - D);
    alwan_scalar R_c = R * fac_R; alwan_scalar G_c = G * fac_G; alwan_scalar B_c = B * fac_B;
    alwan_scalar R_wc = R_w * fac_R; alwan_scalar G_wc = G_w * fac_G; alwan_scalar B_wc = B_w * fac_B;
    alwan_vec3 rgbc_v = {{R_c, G_c, B_c}};
    alwan_vec3 xyzc_v = alwan_mat3_mulv_v(KIM2009_V_M_CAT02_INV, rgbc_v);
    alwan_scalar Xc = xyzc_v.v[0]; alwan_scalar Yc = xyzc_v.v[1]; alwan_scalar Zc = xyzc_v.v[2];
    alwan_vec3 rgbwc_v = {{R_wc, G_wc, B_wc}};
    alwan_vec3 xyzwc_v = alwan_mat3_mulv_v(KIM2009_V_M_CAT02_INV, rgbwc_v);
    alwan_scalar Xwc = xyzwc_v.v[0]; alwan_scalar Ywc = xyzwc_v.v[1]; alwan_scalar Zwc = xyzwc_v.v[2];
    alwan_vec3 xyzc_in = {{Xc, Yc, Zc}};
    alwan_vec3 lms_v = alwan_mat3_mulv_v(KIM2009_V_M_HPE, xyzc_in);
    alwan_scalar L = lms_v.v[0]; alwan_scalar M = lms_v.v[1]; alwan_scalar S = lms_v.v[2];
    alwan_vec3 xyzwc_in = {{Xwc, Ywc, Zwc}};
    alwan_vec3 lmsw_v = alwan_mat3_mulv_v(KIM2009_V_M_HPE, xyzwc_in);
    alwan_scalar L_w = lmsw_v.v[0]; alwan_scalar M_w = lmsw_v.v[1]; alwan_scalar S_w = lmsw_v.v[2];
    alwan_scalar La_nc = kim2009_spow_v(La, KIM2009_V_N_C);
    alwan_scalar L_nc = kim2009_spow_v(L, KIM2009_V_N_C); alwan_scalar M_nc = kim2009_spow_v(M, KIM2009_V_N_C); alwan_scalar S_nc = kim2009_spow_v(S, KIM2009_V_N_C);
    alwan_scalar L_w_nc = kim2009_spow_v(L_w, KIM2009_V_N_C); alwan_scalar M_w_nc = kim2009_spow_v(M_w, KIM2009_V_N_C); alwan_scalar S_w_nc = kim2009_spow_v(S_w, KIM2009_V_N_C);
    alwan_scalar Lp = L_nc / (L_nc + La_nc); alwan_scalar Mp = M_nc / (M_nc + La_nc); alwan_scalar Sp = S_nc / (S_nc + La_nc);
    alwan_scalar Lp_w = L_w_nc / (L_w_nc + La_nc); alwan_scalar Mp_w = M_w_nc / (M_w_nc + La_nc); alwan_scalar Sp_w = S_w_nc / (S_w_nc + La_nc);
    alwan_scalar A = (KIM2009_V_W_L * Lp + KIM2009_V_W_M * Mp + KIM2009_V_W_S * Sp) / KIM2009_V_W_SUM;
    alwan_scalar A_w = (KIM2009_V_W_L * Lp_w + KIM2009_V_W_M * Mp_w + KIM2009_V_W_S * Sp_w) / KIM2009_V_W_SUM;
    alwan_scalar A_A_w = A / A_w;
    alwan_scalar o_j_n_j = ALWAN_POW(KIM2009_V_O_J, KIM2009_V_N_J);
    alwan_scalar num_jp = -(A_A_w - KIM2009_V_B_J) * o_j_n_j;
    alwan_scalar den_jp = A_A_w - KIM2009_V_B_J - KIM2009_V_A_J;
    alwan_scalar J_p = kim2009_spow_v(num_jp / den_jp, ALWAN_ONE / KIM2009_V_N_J);
    alwan_scalar J_val = ALWAN_LITERAL(100.0) * (media_E * (J_p - ALWAN_ONE) + ALWAN_ONE);
    alwan_scalar opp_a = (ALWAN_LITERAL(11.0) * Lp - ALWAN_LITERAL(12.0) * Mp + ALWAN_ONE * Sp) / ALWAN_LITERAL(11.0);
    alwan_scalar opp_b = (ALWAN_ONE * Lp + ALWAN_ONE * Mp - ALWAN_LITERAL(2.0) * Sp) / ALWAN_LITERAL(9.0);
    alwan_scalar C_val = KIM2009_V_A_K * ALWAN_POW(ALWAN_SQRT(opp_a * opp_a + opp_b * opp_b), KIM2009_V_N_K);
    alwan_scalar h_rad = ALWAN_ATAN2(opp_b, opp_a);
    alwan_scalar h_deg = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    h_deg = ALWAN_SELECT(h_deg < ALWAN_ZERO, h_deg + ALWAN_LITERAL(360.0), h_deg);
    result.J = J_val; result.C = C_val; result.h = h_deg;
    return result;
}

ALWAN_INLINE alwan_xyz alwan_kim2009_inverse_v(
    alwan_kim2009_v_correlates correlates, alwan_scalar white_x, alwan_scalar white_y,
    alwan_scalar white_z, alwan_scalar La, alwan_scalar D, alwan_scalar media_E) {
    alwan_xyz result;
    alwan_vec3 white_v = {{white_x, white_y, white_z}};
    alwan_vec3 rgbw_v = alwan_mat3_mulv_v(KIM2009_V_M_CAT02, white_v);
    alwan_scalar R_w = rgbw_v.v[0]; alwan_scalar G_w = rgbw_v.v[1]; alwan_scalar B_w = rgbw_v.v[2];
    alwan_scalar fac_R = D * (white_y / R_w) + (ALWAN_ONE - D);
    alwan_scalar fac_G = D * (white_y / G_w) + (ALWAN_ONE - D);
    alwan_scalar fac_B = D * (white_y / B_w) + (ALWAN_ONE - D);
    alwan_scalar R_wc = R_w * fac_R; alwan_scalar G_wc = G_w * fac_G; alwan_scalar B_wc = B_w * fac_B;
    alwan_vec3 rgbwc_v = {{R_wc, G_wc, B_wc}};
    alwan_vec3 xyzwc_v = alwan_mat3_mulv_v(KIM2009_V_M_CAT02_INV, rgbwc_v);
    alwan_scalar Xwc = xyzwc_v.v[0]; alwan_scalar Ywc = xyzwc_v.v[1]; alwan_scalar Zwc = xyzwc_v.v[2];
    alwan_vec3 xyzwc_in = {{Xwc, Ywc, Zwc}};
    alwan_vec3 lmsw_v = alwan_mat3_mulv_v(KIM2009_V_M_HPE, xyzwc_in);
    alwan_scalar L_w = lmsw_v.v[0]; alwan_scalar M_w = lmsw_v.v[1]; alwan_scalar S_w = lmsw_v.v[2];
    alwan_scalar La_nc = kim2009_spow_v(La, KIM2009_V_N_C);
    alwan_scalar L_w_nc = kim2009_spow_v(L_w, KIM2009_V_N_C); alwan_scalar M_w_nc = kim2009_spow_v(M_w, KIM2009_V_N_C); alwan_scalar S_w_nc = kim2009_spow_v(S_w, KIM2009_V_N_C);
    alwan_scalar Lp_w = L_w_nc / (L_w_nc + La_nc); alwan_scalar Mp_w = M_w_nc / (M_w_nc + La_nc); alwan_scalar Sp_w = S_w_nc / (S_w_nc + La_nc);
    alwan_scalar A_w = (KIM2009_V_W_L * Lp_w + KIM2009_V_W_M * Mp_w + KIM2009_V_W_S * Sp_w) / KIM2009_V_W_SUM;
    alwan_scalar J_p = (correlates.J / ALWAN_LITERAL(100.0) - ALWAN_ONE) / media_E + ALWAN_ONE;
    alwan_scalar J_p_nj = kim2009_spow_v(J_p, KIM2009_V_N_J);
    alwan_scalar o_j_nj = ALWAN_POW(KIM2009_V_O_J, KIM2009_V_N_J);
    alwan_scalar A_val = A_w * ((KIM2009_V_A_J * J_p_nj) / (J_p_nj + o_j_nj) + KIM2009_V_B_J);
    alwan_scalar hr = correlates.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar C_a_k = ALWAN_POW(correlates.C / KIM2009_V_A_K, ALWAN_ONE / KIM2009_V_N_K);
    alwan_scalar opp_a = ALWAN_COS(hr) * C_a_k; alwan_scalar opp_b = ALWAN_SIN(hr) * C_a_k;
    alwan_scalar Lp = A_val + (ALWAN_LITERAL(451.0) / ALWAN_LITERAL(1403.0)) * opp_a + (ALWAN_LITERAL(288.0) / ALWAN_LITERAL(1403.0)) * opp_b;
    alwan_scalar Mp = A_val - (ALWAN_LITERAL(891.0) / ALWAN_LITERAL(1403.0)) * opp_a - (ALWAN_LITERAL(261.0) / ALWAN_LITERAL(1403.0)) * opp_b;
    alwan_scalar Sp = A_val - (ALWAN_LITERAL(220.0) / ALWAN_LITERAL(1403.0)) * opp_a - (ALWAN_LITERAL(6300.0) / ALWAN_LITERAL(1403.0)) * opp_b;
    alwan_scalar inv_nc = ALWAN_ONE / KIM2009_V_N_C;
    alwan_scalar den_L = Lp - ALWAN_ONE; alwan_scalar ratio_L = -La_nc * Lp / den_L;
    ratio_L = ALWAN_SELECT(ALWAN_ABS(den_L) < ALWAN_EPSILON, ALWAN_ZERO, ratio_L);
    ratio_L = ALWAN_SELECT(ratio_L < ALWAN_ZERO, ALWAN_ZERO, ratio_L);
    alwan_scalar Lv = ALWAN_POW(ratio_L, inv_nc);
    Lv = ALWAN_SELECT(ALWAN_ABS(den_L) < ALWAN_EPSILON, ALWAN_ZERO, Lv);
    alwan_scalar den_M = Mp - ALWAN_ONE; alwan_scalar ratio_M = -La_nc * Mp / den_M;
    ratio_M = ALWAN_SELECT(ALWAN_ABS(den_M) < ALWAN_EPSILON, ALWAN_ZERO, ratio_M);
    ratio_M = ALWAN_SELECT(ratio_M < ALWAN_ZERO, ALWAN_ZERO, ratio_M);
    alwan_scalar Mv = ALWAN_POW(ratio_M, inv_nc);
    Mv = ALWAN_SELECT(ALWAN_ABS(den_M) < ALWAN_EPSILON, ALWAN_ZERO, Mv);
    alwan_scalar den_S = Sp - ALWAN_ONE; alwan_scalar ratio_S = -La_nc * Sp / den_S;
    ratio_S = ALWAN_SELECT(ALWAN_ABS(den_S) < ALWAN_EPSILON, ALWAN_ZERO, ratio_S);
    ratio_S = ALWAN_SELECT(ratio_S < ALWAN_ZERO, ALWAN_ZERO, ratio_S);
    alwan_scalar Sv = ALWAN_POW(ratio_S, inv_nc);
    Sv = ALWAN_SELECT(ALWAN_ABS(den_S) < ALWAN_EPSILON, ALWAN_ZERO, Sv);
    alwan_vec3 lms_v = {{Lv, Mv, Sv}};
    alwan_vec3 xyzc_v = alwan_mat3_mulv_v(KIM2009_V_M_HPE_INV, lms_v);
    alwan_scalar Xc = xyzc_v.v[0]; alwan_scalar Yc = xyzc_v.v[1]; alwan_scalar Zc = xyzc_v.v[2];
    alwan_vec3 xyzc_in = {{Xc, Yc, Zc}};
    alwan_vec3 rgbc_v = alwan_mat3_mulv_v(KIM2009_V_M_CAT02, xyzc_in);
    alwan_scalar R_c = rgbc_v.v[0]; alwan_scalar G_c = rgbc_v.v[1]; alwan_scalar B_c = rgbc_v.v[2];
    alwan_scalar R_out = R_c / fac_R; alwan_scalar G_out = G_c / fac_G; alwan_scalar B_out = B_c / fac_B;
    alwan_vec3 rgb_out_v = {{R_out, G_out, B_out}};
    alwan_vec3 xyz_out_v = alwan_mat3_mulv_v(KIM2009_V_M_CAT02_INV, rgb_out_v);
    result.x = xyz_out_v.v[0]; result.y = xyz_out_v.v[1]; result.z = xyz_out_v.v[2];
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_KIM2009_CORE_H */
