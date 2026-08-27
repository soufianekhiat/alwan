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

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_atd95_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_atd95_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* GPU backends - original code */
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
ALWAN_INLINE alwan_scalar atd95_spow_response_v(alwan_scalar linear, alwan_scalar scale, alwan_scalar offset, alwan_scalar exponent) {
    alwan_scalar scaled = scale * linear;
    alwan_scalar mag = ALWAN_POW(ALWAN_ABS(scaled), exponent);
    alwan_scalar signed_pow = ALWAN_SELECT(scaled < ALWAN_ZERO, -mag, mag);
    return signed_pow + offset;
}

ALWAN_INLINE alwan_scalar atd95_final_response_v(alwan_scalar value) {
    return value / (ALWAN_LITERAL(200.0) + ALWAN_ABS(value));
}

ALWAN_INLINE alwan_atd95_v_correlates alwan_atd95_forward_v(
    alwan_xyz xyz, alwan_xyz white_xyz,
    alwan_scalar Y_0, alwan_scalar k1, alwan_scalar k2, alwan_scalar sigma) {
    alwan_atd95_v_correlates result;
    alwan_scalar X_r  = ATD95_V_RETINAL_SCALE * ALWAN_POW(Y_0 * xyz.x / ALWAN_LITERAL(100.0), ATD95_V_RETINAL_EXP);
    alwan_scalar Y_r  = ATD95_V_RETINAL_SCALE * ALWAN_POW(Y_0 * xyz.y / ALWAN_LITERAL(100.0), ATD95_V_RETINAL_EXP);
    alwan_scalar Z_r  = ATD95_V_RETINAL_SCALE * ALWAN_POW(Y_0 * xyz.z / ALWAN_LITERAL(100.0), ATD95_V_RETINAL_EXP);
    alwan_scalar X_0r = ATD95_V_RETINAL_SCALE * ALWAN_POW(Y_0 * white_xyz.x / ALWAN_LITERAL(100.0), ATD95_V_RETINAL_EXP);
    alwan_scalar Y_0r = ATD95_V_RETINAL_SCALE * ALWAN_POW(Y_0 * white_xyz.y / ALWAN_LITERAL(100.0), ATD95_V_RETINAL_EXP);
    alwan_scalar Z_0r = ATD95_V_RETINAL_SCALE * ALWAN_POW(Y_0 * white_xyz.z / ALWAN_LITERAL(100.0), ATD95_V_RETINAL_EXP);
    alwan_scalar L_linear = ATD95_V_LMS_L_X * X_r + ATD95_V_LMS_L_Y * Y_r + ATD95_V_LMS_L_Z * Z_r;
    alwan_scalar M_linear = ATD95_V_LMS_M_X * X_r + ATD95_V_LMS_M_Y * Y_r + ATD95_V_LMS_M_Z * Z_r;
    alwan_scalar S_linear = ATD95_V_LMS_S_Y * Y_r + ATD95_V_LMS_S_Z * Z_r;
    alwan_scalar L = atd95_spow_response_v(L_linear, ATD95_V_L_SCALE, ATD95_V_L_OFFSET, ATD95_V_L_EXP);
    alwan_scalar M = atd95_spow_response_v(M_linear, ALWAN_ONE, ATD95_V_M_OFFSET, ATD95_V_M_EXP);
    alwan_scalar S = atd95_spow_response_v(S_linear, ATD95_V_S_SCALE, ATD95_V_S_OFFSET, ATD95_V_S_EXP);
    alwan_scalar L0_linear = ATD95_V_LMS_L_X * X_0r + ATD95_V_LMS_L_Y * Y_0r + ATD95_V_LMS_L_Z * Z_0r;
    alwan_scalar M0_linear = ATD95_V_LMS_M_X * X_0r + ATD95_V_LMS_M_Y * Y_0r + ATD95_V_LMS_M_Z * Z_0r;
    alwan_scalar S0_linear = ATD95_V_LMS_S_Y * Y_0r + ATD95_V_LMS_S_Z * Z_0r;
    alwan_scalar L_0 = atd95_spow_response_v(L0_linear, ATD95_V_L_SCALE, ATD95_V_L_OFFSET, ATD95_V_L_EXP);
    alwan_scalar M_0 = atd95_spow_response_v(M0_linear, ALWAN_ONE, ATD95_V_M_OFFSET, ATD95_V_M_EXP);
    alwan_scalar S_0 = atd95_spow_response_v(S0_linear, ATD95_V_S_SCALE, ATD95_V_S_OFFSET, ATD95_V_S_EXP);
    ALWAN_UNUSED(L_0); ALWAN_UNUSED(M_0); ALWAN_UNUSED(S_0);
    alwan_scalar X_ar = k1 * X_r + k2 * X_0r;
    alwan_scalar Y_ar = k1 * Y_r + k2 * Y_0r;
    alwan_scalar Z_ar = k1 * Z_r + k2 * Z_0r;
    alwan_scalar La_linear = ATD95_V_LMS_L_X * X_ar + ATD95_V_LMS_L_Y * Y_ar + ATD95_V_LMS_L_Z * Z_ar;
    alwan_scalar Ma_linear = ATD95_V_LMS_M_X * X_ar + ATD95_V_LMS_M_Y * Y_ar + ATD95_V_LMS_M_Z * Z_ar;
    alwan_scalar Sa_linear = ATD95_V_LMS_S_Y * Y_ar + ATD95_V_LMS_S_Z * Z_ar;
    alwan_scalar L_a = atd95_spow_response_v(La_linear, ATD95_V_L_SCALE, ATD95_V_L_OFFSET, ATD95_V_L_EXP);
    alwan_scalar M_a = atd95_spow_response_v(Ma_linear, ALWAN_ONE, ATD95_V_M_OFFSET, ATD95_V_M_EXP);
    alwan_scalar S_a = atd95_spow_response_v(Sa_linear, ATD95_V_S_SCALE, ATD95_V_S_OFFSET, ATD95_V_S_EXP);
    alwan_scalar L_g = L * (sigma / (sigma + L_a));
    alwan_scalar M_g = M * (sigma / (sigma + M_a));
    alwan_scalar S_g = S * (sigma / (sigma + S_a));
    alwan_scalar A_1i = ATD95_V_A1_L * L_g + ATD95_V_A1_M * M_g;
    alwan_scalar T_1i = ATD95_V_T1_L * L_g + ATD95_V_T1_M * M_g;
    alwan_scalar D_1i = ATD95_V_D1_L * L_g + ATD95_V_D1_M * M_g + S_g;
    alwan_scalar A_2i = ATD95_V_A2_SCALE * A_1i;
    alwan_scalar T_2i = ATD95_V_T2_T * T_1i + ATD95_V_T2_D * D_1i;
    alwan_scalar D_2i = D_1i;
    alwan_scalar A_1 = atd95_final_response_v(A_1i);
    alwan_scalar T_1 = atd95_final_response_v(T_1i);
    alwan_scalar D_1 = atd95_final_response_v(D_1i);
    alwan_scalar A_2 = atd95_final_response_v(A_2i);
    alwan_scalar T_2 = atd95_final_response_v(T_2i);
    alwan_scalar D_2 = atd95_final_response_v(D_2i);
    alwan_scalar Br = ALWAN_SQRT(A_1 * A_1 + T_1 * T_1 + D_1 * D_1);
    alwan_scalar abs_A2 = ALWAN_ABS(A_2);
    alwan_scalar C = ALWAN_SELECT(abs_A2 > ALWAN_LITERAL(1e-10),
                                  ALWAN_SQRT(T_2 * T_2 + D_2 * D_2) / A_2, ALWAN_ZERO);
    /* Hue.
     *
     * ATD95 defines H as the plain RATIO T_2 / D_2, not an angle. It is not
     * bounded to [0, 360) and the reference implementation does not wrap it.
     * Until 2026-08-27 this returned degrees(atan2(T_2, D_2)) wrapped into
     * [0, 360), which is a different quantity with a different range. */
    alwan_scalar H = ALWAN_SELECT(ALWAN_ABS(D_2) > ALWAN_LITERAL(1e-12),
                                  T_2 / ALWAN_SELECT(ALWAN_ABS(D_2) > ALWAN_LITERAL(1e-12), D_2, ALWAN_ONE),
                                  ALWAN_ZERO);
    result.H = H; result.C = C; result.Br = Br;
    result.A_1 = A_1; result.T_1 = T_1; result.D_1 = D_1;
    result.A_2 = A_2; result.T_2 = T_2; result.D_2 = D_2;
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_ATD95_CORE_H */
