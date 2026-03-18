/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only CAM20u Color Appearance Model for unrelated color
 * Reference: Kim & Park (2020) "Color appearance model for unrelated color"
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_CAM20U_CORE_H
#define ALWAN_CAM20U_CORE_H

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
#include "alwan_cam20u_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_cam20u_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* GPU backends - original code */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

typedef struct {
    alwan_scalar Q;
    alwan_scalar M;
    alwan_scalar h;
    alwan_scalar C;
    alwan_scalar s;
    alwan_scalar a;
    alwan_scalar b;
} alwan_cam20u_v_correlates;

ALWAN_INLINE alwan_cam20u_v_correlates alwan_cam20u_forward_v(
    alwan_xyz xyz, alwan_scalar Y_b, alwan_scalar L_a) {
    alwan_cam20u_v_correlates result;
    alwan_scalar L = ALWAN_LITERAL( 0.401288) * xyz.x + ALWAN_LITERAL( 0.650173) * xyz.y + ALWAN_LITERAL(-0.051461) * xyz.z;
    alwan_scalar M = ALWAN_LITERAL(-0.250268) * xyz.x + ALWAN_LITERAL( 1.204414) * xyz.y + ALWAN_LITERAL( 0.045854) * xyz.z;
    alwan_scalar S = ALWAN_LITERAL(-0.002079) * xyz.x + ALWAN_LITERAL( 0.048952) * xyz.y + ALWAN_LITERAL( 0.953127) * xyz.z;
    alwan_scalar k = ALWAN_ONE / (ALWAN_LITERAL(5.0) * L_a + ALWAN_ONE);
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k * k * k * k * (ALWAN_LITERAL(5.0) * L_a)
                    + ALWAN_LITERAL(0.1) * ALWAN_POW(ALWAN_ONE - k * k * k * k, ALWAN_LITERAL(2.0))
                    * ALWAN_POW(ALWAN_LITERAL(5.0) * L_a, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
    alwan_scalar p = ALWAN_LITERAL(0.42);
    alwan_scalar L_abs = ALWAN_ABS(L); alwan_scalar M_abs = ALWAN_ABS(M); alwan_scalar S_abs = ALWAN_ABS(S);
    alwan_scalar L_safe = ALWAN_SELECT(L_abs < ALWAN_LITERAL(1e-12), ALWAN_LITERAL(1e-12), L_abs);
    alwan_scalar M_safe = ALWAN_SELECT(M_abs < ALWAN_LITERAL(1e-12), ALWAN_LITERAL(1e-12), M_abs);
    alwan_scalar S_safe = ALWAN_SELECT(S_abs < ALWAN_LITERAL(1e-12), ALWAN_LITERAL(1e-12), S_abs);
    alwan_scalar tmp_L = ALWAN_POW(FL * L_safe / ALWAN_LITERAL(100.0), p);
    alwan_scalar tmp_M = ALWAN_POW(FL * M_safe / ALWAN_LITERAL(100.0), p);
    alwan_scalar tmp_S = ALWAN_POW(FL * S_safe / ALWAN_LITERAL(100.0), p);
    alwan_scalar La = ALWAN_LITERAL(400.0) * tmp_L / (tmp_L + ALWAN_LITERAL(27.13)) + ALWAN_LITERAL(0.1);
    alwan_scalar Ma_val = ALWAN_LITERAL(400.0) * tmp_M / (tmp_M + ALWAN_LITERAL(27.13)) + ALWAN_LITERAL(0.1);
    alwan_scalar Sa = ALWAN_LITERAL(400.0) * tmp_S / (tmp_S + ALWAN_LITERAL(27.13)) + ALWAN_LITERAL(0.1);
    result.a = La - ALWAN_LITERAL(12.0) / ALWAN_LITERAL(11.0) * Ma_val + Sa / ALWAN_LITERAL(11.0);
    result.b = (La + Ma_val - ALWAN_LITERAL(2.0) * Sa) / ALWAN_LITERAL(9.0);
    result.h = ALWAN_ATAN2(result.b, result.a) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    result.h = ALWAN_SELECT(result.h < ALWAN_ZERO, result.h + ALWAN_LITERAL(360.0), result.h);
    alwan_scalar A = (ALWAN_LITERAL(2.0) * La + Ma_val + ALWAN_LITERAL(0.05) * Sa - ALWAN_LITERAL(0.305));
    alwan_scalar Y_b_safe = ALWAN_SELECT(Y_b < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), Y_b);
    alwan_scalar n = Y_b_safe / ALWAN_LITERAL(100.0);
    alwan_scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);
    alwan_scalar A_safe = ALWAN_SELECT(A < ALWAN_ZERO, ALWAN_ZERO, A);
    result.Q = ALWAN_LITERAL(11.0) * ALWAN_POW(A_safe / ALWAN_LITERAL(100.0), z);
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(result.h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));
    result.M = ALWAN_SQRT(result.a * result.a + result.b * result.b) * et;
    alwan_scalar Q_safe = ALWAN_SELECT(result.Q < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), result.Q);
    result.C = result.M / Q_safe * ALWAN_LITERAL(100.0);
    result.s = ALWAN_LITERAL(100.0) * ALWAN_SQRT(result.M / Q_safe);
    return result;
}

ALWAN_INLINE alwan_xyz alwan_cam20u_inverse_v(
    alwan_cam20u_v_correlates correlates, alwan_scalar Y_b, alwan_scalar L_a) {
    alwan_xyz result;
    alwan_scalar Y_b_safe = ALWAN_SELECT(Y_b < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), Y_b);
    alwan_scalar n = Y_b_safe / ALWAN_LITERAL(100.0);
    alwan_scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);
    alwan_scalar safe_z = ALWAN_SELECT(z < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), z);
    alwan_scalar A = ALWAN_LITERAL(100.0) * ALWAN_POW(correlates.Q / ALWAN_LITERAL(11.0), ALWAN_ONE / safe_z);
    alwan_scalar h_rad = correlates.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h_rad + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));
    alwan_scalar et_safe = ALWAN_SELECT(et < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), et);
    alwan_scalar t = correlates.M / et_safe;
    alwan_scalar a = t * ALWAN_COS(h_rad);
    alwan_scalar b = t * ALWAN_SIN(h_rad);
    alwan_scalar A_full = A + ALWAN_LITERAL(0.305);
    alwan_scalar c_a = ALWAN_LITERAL(11.0) / ALWAN_LITERAL(23.0);
    alwan_scalar c_b = ALWAN_LITERAL(315.0) / ALWAN_LITERAL(23.0);
    alwan_scalar Sa = (A_full - c_a * a - c_b * b) / ALWAN_LITERAL(3.05);
    alwan_scalar Ma_val = Sa - (ALWAN_LITERAL(11.0) / ALWAN_LITERAL(23.0)) * (a - ALWAN_LITERAL(9.0) * b);
    alwan_scalar La = ALWAN_LITERAL(9.0) * b - Ma_val + ALWAN_LITERAL(2.0) * Sa;
    alwan_scalar k = ALWAN_ONE / (ALWAN_LITERAL(5.0) * L_a + ALWAN_ONE);
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k * k * k * k * (ALWAN_LITERAL(5.0) * L_a)
                    + ALWAN_LITERAL(0.1) * ALWAN_POW(ALWAN_ONE - k * k * k * k, ALWAN_LITERAL(2.0))
                    * ALWAN_POW(ALWAN_LITERAL(5.0) * L_a, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
    alwan_scalar p = ALWAN_LITERAL(0.42);
    alwan_scalar inv_p = ALWAN_ONE / p;
    alwan_scalar La_m = La - ALWAN_LITERAL(0.1);
    alwan_scalar Ma_m = Ma_val - ALWAN_LITERAL(0.1);
    alwan_scalar Sa_m = Sa - ALWAN_LITERAL(0.1);
    alwan_scalar denom_L = ALWAN_SELECT(ALWAN_ABS(ALWAN_LITERAL(400.0) - La_m) < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), ALWAN_LITERAL(400.0) - La_m);
    alwan_scalar denom_M = ALWAN_SELECT(ALWAN_ABS(ALWAN_LITERAL(400.0) - Ma_m) < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), ALWAN_LITERAL(400.0) - Ma_m);
    alwan_scalar denom_S = ALWAN_SELECT(ALWAN_ABS(ALWAN_LITERAL(400.0) - Sa_m) < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), ALWAN_LITERAL(400.0) - Sa_m);
    alwan_scalar tmp_L = ALWAN_LITERAL(27.13) * La_m / denom_L;
    alwan_scalar tmp_M = ALWAN_LITERAL(27.13) * Ma_m / denom_M;
    alwan_scalar tmp_S = ALWAN_LITERAL(27.13) * Sa_m / denom_S;
    alwan_scalar tmp_L_safe = ALWAN_SELECT(tmp_L < ALWAN_ZERO, ALWAN_ZERO, tmp_L);
    alwan_scalar tmp_M_safe = ALWAN_SELECT(tmp_M < ALWAN_ZERO, ALWAN_ZERO, tmp_M);
    alwan_scalar tmp_S_safe = ALWAN_SELECT(tmp_S < ALWAN_ZERO, ALWAN_ZERO, tmp_S);
    alwan_scalar FL_safe = ALWAN_SELECT(FL < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), FL);
    alwan_scalar Lc = (ALWAN_LITERAL(100.0) / FL_safe) * ALWAN_POW(tmp_L_safe, inv_p);
    alwan_scalar Mc = (ALWAN_LITERAL(100.0) / FL_safe) * ALWAN_POW(tmp_M_safe, inv_p);
    alwan_scalar Sc = (ALWAN_LITERAL(100.0) / FL_safe) * ALWAN_POW(tmp_S_safe, inv_p);
    result.x = ALWAN_LITERAL( 1.86206787) * Lc + ALWAN_LITERAL(-1.01125463) * Mc + ALWAN_LITERAL( 0.14918677) * Sc;
    result.y = ALWAN_LITERAL( 0.38752654) * Lc + ALWAN_LITERAL( 0.62144744) * Mc + ALWAN_LITERAL(-0.00897398) * Sc;
    result.z = ALWAN_LITERAL(-0.01584150) * Lc + ALWAN_LITERAL(-0.03412294) * Mc + ALWAN_LITERAL( 1.04996444) * Sc;
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_CAM20U_CORE_H */
