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
#include "alwan_llab_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_llab_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

typedef struct {
    alwan_scalar L, Ch, h, s;
} alwan_llab_v_correlates;

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 LLAB_XYZ_TO_RGB = {{
#include "../data/matrices/llab_xyz_to_rgb.csv"
}};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 LLAB_RGB_TO_XYZ = {{
#include "../data/matrices/llab_rgb_to_xyz.csv"
}};
ALWAN_DIAG_POP

static const alwan_scalar LLAB_V_EPSILON = ALWAN_LITERAL(0.008856);
static const alwan_scalar LLAB_V_COEF_116 = ALWAN_LITERAL(116.0);
static const alwan_scalar LLAB_V_COEF_16 = ALWAN_LITERAL(16.0);
static const alwan_scalar LLAB_V_COEF_500 = ALWAN_LITERAL(500.0);
static const alwan_scalar LLAB_V_COEF_200 = ALWAN_LITERAL(200.0);
static const alwan_scalar LLAB_V_CHROMA_SCALE = ALWAN_LITERAL(25.0);
static const alwan_scalar LLAB_V_CHROMA_CONST = ALWAN_LITERAL(0.05);
static const alwan_scalar LLAB_V_BETA_EXP = ALWAN_LITERAL(0.0834);

ALWAN_INLINE alwan_scalar llab_f_v(alwan_scalar t, alwan_scalar F_S) {
    alwan_scalar power_result = ALWAN_POW(t, ALWAN_LITERAL(1.0) / F_S);
    alwan_scalar slope = (ALWAN_POW(LLAB_V_EPSILON, ALWAN_LITERAL(1.0) / F_S) -
                          LLAB_V_COEF_16 / LLAB_V_COEF_116) / LLAB_V_EPSILON;
    alwan_scalar linear_result = slope * t + LLAB_V_COEF_16 / LLAB_V_COEF_116;
    return ALWAN_SELECT(t > LLAB_V_EPSILON, power_result, linear_result);
}

ALWAN_INLINE alwan_llab_v_correlates alwan_llab_forward_v(
    alwan_xyz xyz, alwan_xyz xyz_0, alwan_xyz xyz_r,
    alwan_scalar Y_b, alwan_scalar D, alwan_scalar F_S, alwan_scalar F_L) {
    alwan_llab_v_correlates result;
    alwan_scalar Y_sample = xyz.y;
    alwan_scalar Y_s_inv = ALWAN_SELECT(ALWAN_ABS(Y_sample) > ALWAN_EPSILON, ALWAN_LITERAL(1.0) / Y_sample, ALWAN_LITERAL(1.0));
    alwan_vec3 xyz_norm = {{xyz.x * Y_s_inv, ALWAN_LITERAL(1.0), xyz.z * Y_s_inv}};
    alwan_vec3 rgb_v = alwan_mat3_mulv_v(LLAB_XYZ_TO_RGB, xyz_norm);
    alwan_scalar R = rgb_v.v[0]; alwan_scalar G = rgb_v.v[1]; alwan_scalar B = rgb_v.v[2];
    alwan_scalar Y0_inv = ALWAN_SELECT(ALWAN_ABS(xyz_0.y) > ALWAN_EPSILON, ALWAN_LITERAL(1.0) / xyz_0.y, ALWAN_LITERAL(1.0));
    alwan_vec3 xyz_0_norm = {{xyz_0.x * Y0_inv, ALWAN_LITERAL(1.0), xyz_0.z * Y0_inv}};
    alwan_vec3 rgb_0_v = alwan_mat3_mulv_v(LLAB_XYZ_TO_RGB, xyz_0_norm);
    alwan_scalar R_0 = rgb_0_v.v[0]; alwan_scalar G_0 = rgb_0_v.v[1]; alwan_scalar B_0 = rgb_0_v.v[2];
    alwan_scalar Yr_inv = ALWAN_SELECT(ALWAN_ABS(xyz_r.y) > ALWAN_EPSILON, ALWAN_LITERAL(1.0) / xyz_r.y, ALWAN_LITERAL(1.0));
    alwan_vec3 xyz_r_norm = {{xyz_r.x * Yr_inv, ALWAN_LITERAL(1.0), xyz_r.z * Yr_inv}};
    alwan_vec3 rgb_r_v = alwan_mat3_mulv_v(LLAB_XYZ_TO_RGB, xyz_r_norm);
    alwan_scalar R_r = rgb_r_v.v[0]; alwan_scalar G_r = rgb_r_v.v[1]; alwan_scalar B_r = rgb_r_v.v[2];
    alwan_scalar R_adapted = (D * (R_r / R_0) + (ALWAN_LITERAL(1.0) - D)) * R;
    alwan_scalar G_adapted = (D * (G_r / G_0) + (ALWAN_LITERAL(1.0) - D)) * G;
    alwan_scalar beta = ALWAN_POW(B_0 / B_r, LLAB_V_BETA_EXP);
    alwan_scalar B_adapted = (D * B_r / ALWAN_POW(B_0, beta) + (ALWAN_LITERAL(1.0) - D)) * ALWAN_POW(B, beta);
    alwan_vec3 adapted_v = {{R_adapted * Y_sample, G_adapted * Y_sample, B_adapted * Y_sample}};
    alwan_vec3 xyz_adapted_v = alwan_mat3_mulv_v(LLAB_RGB_TO_XYZ, adapted_v);
    alwan_scalar X_adapted = xyz_adapted_v.v[0]; alwan_scalar Y_adapted = xyz_adapted_v.v[1]; alwan_scalar Z_adapted = xyz_adapted_v.v[2];
    alwan_scalar f_X = llab_f_v(X_adapted / ALWAN_LITERAL(95.05), F_S);
    alwan_scalar f_Y = llab_f_v(Y_adapted / ALWAN_LITERAL(100.0), F_S);
    alwan_scalar f_Z = llab_f_v(Z_adapted / ALWAN_LITERAL(108.88), F_S);
    alwan_scalar z = ALWAN_LITERAL(1.0) + F_L * ALWAN_SQRT(Y_b / ALWAN_LITERAL(100.0));
    alwan_scalar L_L = LLAB_V_COEF_116 * ALWAN_POW(f_Y, z) - LLAB_V_COEF_16;
    alwan_scalar a_L = LLAB_V_COEF_500 * (f_X - f_Y);
    alwan_scalar b_L = LLAB_V_COEF_200 * (f_Y - f_Z);
    alwan_scalar c = ALWAN_SQRT(a_L * a_L + b_L * b_L);
    alwan_scalar Ch_L = LLAB_V_CHROMA_SCALE * ALWAN_LN(ALWAN_LITERAL(1.0) + LLAB_V_CHROMA_CONST * c);
    alwan_scalar h_rad = ALWAN_ATAN2(b_L, a_L);
    alwan_scalar h_deg = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    alwan_scalar h_L = ALWAN_SELECT(h_deg < ALWAN_LITERAL(0.0), h_deg + ALWAN_LITERAL(360.0), h_deg);
    alwan_scalar s_L = ALWAN_SELECT(L_L > ALWAN_LITERAL(0.0), Ch_L / L_L, ALWAN_LITERAL(0.0));
    result.L = L_L; result.Ch = Ch_L; result.h = h_L; result.s = s_L;
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_LLAB_CORE_H */
