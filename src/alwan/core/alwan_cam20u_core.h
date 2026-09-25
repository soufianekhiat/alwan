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

/* ================================================================
 * CAM20u Correlates
 * ================================================================ */

typedef struct {
    alwan_scalar Q;   /* Brightness */
    alwan_scalar M;   /* Colorfulness */
    alwan_scalar h;   /* Hue angle (degrees) */
    alwan_scalar C;   /* Chroma */
    alwan_scalar s;   /* Saturation */
    alwan_scalar a;   /* Red-green */
    alwan_scalar b;   /* Yellow-blue */
} alwan_cam20u_v_correlates;

/* ================================================================
 * CAM20u Forward Transform
 *
 * XYZ -> CAM20u correlates for unrelated color stimuli
 *
 * Parameters:
 *   xyz  - CIE XYZ tristimulus values (absolute, in cd/m2)
 *   Y_b  - background luminance (cd/m2)
 *   L_a  - adapting luminance (cd/m2)
 * ================================================================ */

ALWAN_INLINE alwan_cam20u_v_correlates alwan_cam20u_forward_v(
    alwan_xyz xyz,
    alwan_scalar Y_b,
    alwan_scalar L_a) {
    alwan_cam20u_v_correlates result;

    /* Step 1: XYZ to Hunt-Pointer-Estevez (HPE) cone response
     * Uses the sharpened cone response from CAM16 */
    alwan_scalar L = ALWAN_LITERAL( 0.401288) * xyz.x
                   + ALWAN_LITERAL( 0.650173) * xyz.y
                   + ALWAN_LITERAL(-0.051461) * xyz.z;
    alwan_scalar M = ALWAN_LITERAL(-0.250268) * xyz.x
                   + ALWAN_LITERAL( 1.204414) * xyz.y
                   + ALWAN_LITERAL( 0.045854) * xyz.z;
    alwan_scalar S = ALWAN_LITERAL(-0.002079) * xyz.x
                   + ALWAN_LITERAL( 0.048952) * xyz.y
                   + ALWAN_LITERAL( 0.953127) * xyz.z;

    /* Step 2: Luminance adaptation factor (Hunt model) */
    alwan_scalar k = ALWAN_ONE / (ALWAN_LITERAL(5.0) * L_a + ALWAN_ONE);
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k * k * k * k * (ALWAN_LITERAL(5.0) * L_a)
                    + ALWAN_LITERAL(0.1) * ALWAN_POW(ALWAN_ONE - k * k * k * k, ALWAN_LITERAL(2.0))
                    * ALWAN_POW(ALWAN_LITERAL(5.0) * L_a, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    /* Step 3: Nonlinear cone response (adapted, power-function compression) */
    alwan_scalar p = ALWAN_LITERAL(0.42);
    alwan_scalar L_abs = ALWAN_ABS(L);
    alwan_scalar M_abs = ALWAN_ABS(M);
    alwan_scalar S_abs = ALWAN_ABS(S);

    /* Naka-Rushton-type response compression, keeping the sign (CAM16's form) */
    alwan_scalar tmp_L = ALWAN_POW(FL * L_abs / ALWAN_LITERAL(100.0), p);
    alwan_scalar tmp_M = ALWAN_POW(FL * M_abs / ALWAN_LITERAL(100.0), p);
    alwan_scalar tmp_S = ALWAN_POW(FL * S_abs / ALWAN_LITERAL(100.0), p);

    alwan_scalar cL = ALWAN_LITERAL(400.0) * tmp_L / (tmp_L + ALWAN_LITERAL(27.13));
    alwan_scalar cM = ALWAN_LITERAL(400.0) * tmp_M / (tmp_M + ALWAN_LITERAL(27.13));
    alwan_scalar cS = ALWAN_LITERAL(400.0) * tmp_S / (tmp_S + ALWAN_LITERAL(27.13));
    alwan_scalar La = ALWAN_SELECT(L < ALWAN_ZERO, -cL, cL) + ALWAN_LITERAL(0.1);
    alwan_scalar Ma_val = ALWAN_SELECT(M < ALWAN_ZERO, -cM, cM) + ALWAN_LITERAL(0.1);
    alwan_scalar Sa = ALWAN_SELECT(S < ALWAN_ZERO, -cS, cS) + ALWAN_LITERAL(0.1);

    /* Step 4: Opponent channels */
    result.a = La - ALWAN_LITERAL(12.0) / ALWAN_LITERAL(11.0) * Ma_val
             + Sa / ALWAN_LITERAL(11.0);
    result.b = (La + Ma_val - ALWAN_LITERAL(2.0) * Sa) / ALWAN_LITERAL(9.0);

    /* Step 5: Hue angle */
    result.h = ALWAN_ATAN2(result.b, result.a) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    result.h = ALWAN_SELECT(result.h < ALWAN_ZERO,
                            result.h + ALWAN_LITERAL(360.0), result.h);

    /* Step 6: Achromatic response */
    alwan_scalar A = (ALWAN_LITERAL(2.0) * La + Ma_val
                    + ALWAN_LITERAL(0.05) * Sa - ALWAN_LITERAL(0.305));

    /* Step 7: Brightness */
    alwan_scalar Y_b_safe = ALWAN_SELECT(Y_b < ALWAN_LITERAL(1e-10),
                                          ALWAN_LITERAL(1e-10), Y_b);
    alwan_scalar n = Y_b_safe / ALWAN_LITERAL(100.0);
    alwan_scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);
    alwan_scalar A_abs = ALWAN_ABS(A);
    alwan_scalar Q_abs = ALWAN_LITERAL(11.0) * ALWAN_POW(A_abs / ALWAN_LITERAL(100.0), z);
    result.Q = ALWAN_SELECT(A < ALWAN_ZERO, -Q_abs, Q_abs);

    /* Step 8: Colorfulness and Chroma */
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(result.h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));
    result.M = ALWAN_SQRT(result.a * result.a + result.b * result.b) * et;
    alwan_scalar Q_safe = ALWAN_SELECT(result.Q < ALWAN_LITERAL(1e-10),
                                        ALWAN_LITERAL(1e-10), result.Q);
    result.C = result.M / Q_safe * ALWAN_LITERAL(100.0);

    /* Step 9: Saturation */
    result.s = ALWAN_LITERAL(100.0) * ALWAN_SQRT(result.M / Q_safe);

    return result;
}

/* ================================================================
 * CAM20u Inverse Transform
 *
 * CAM20u correlates -> XYZ
 * Uses Q, M, h to reconstruct XYZ.
 * ================================================================ */

ALWAN_INLINE alwan_xyz alwan_cam20u_inverse_v(
    alwan_cam20u_v_correlates correlates,
    alwan_scalar Y_b,
    alwan_scalar L_a) {
    alwan_xyz result;

    /* Reconstruct achromatic response from Q */
    alwan_scalar Y_b_safe = ALWAN_SELECT(Y_b < ALWAN_LITERAL(1e-10),
                                          ALWAN_LITERAL(1e-10), Y_b);
    alwan_scalar n = Y_b_safe / ALWAN_LITERAL(100.0);
    alwan_scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);
    alwan_scalar safe_z = ALWAN_SELECT(z < ALWAN_LITERAL(1e-10),
                                        ALWAN_LITERAL(1e-10), z);
    alwan_scalar Q_abs = ALWAN_ABS(correlates.Q);
    alwan_scalar A_abs = ALWAN_LITERAL(100.0) * ALWAN_POW(Q_abs / ALWAN_LITERAL(11.0),
        ALWAN_ONE / safe_z);
    alwan_scalar A = ALWAN_SELECT(correlates.Q < ALWAN_ZERO, -A_abs, A_abs);

    /* Reconstruct a, b from M and h */
    alwan_scalar h_rad = correlates.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h_rad + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));
    alwan_scalar et_safe = ALWAN_SELECT(et < ALWAN_LITERAL(1e-10),
                                         ALWAN_LITERAL(1e-10), et);
    alwan_scalar t = correlates.M / et_safe;
    alwan_scalar a = t * ALWAN_COS(h_rad);
    alwan_scalar b = t * ALWAN_SIN(h_rad);

    /* Reconstruct La, Ma, Sa from a, b, A
     * a = La - (12/11)*Ma + (1/11)*Sa
     * b = (1/9)*La + (1/9)*Ma - (2/9)*Sa
     * A+0.305 = 2*La + Ma + 0.05*Sa
     */
    alwan_scalar A_full = A + ALWAN_LITERAL(0.305);

    /* Solving the linear system using Cramer's rule or substitution */
    alwan_scalar c_a = ALWAN_LITERAL(11.0) / ALWAN_LITERAL(23.0);
    alwan_scalar c_b = ALWAN_LITERAL(315.0) / ALWAN_LITERAL(23.0);
    alwan_scalar Sa = (A_full - c_a * a - c_b * b) / ALWAN_LITERAL(3.05);
    alwan_scalar Ma_val = Sa - (ALWAN_LITERAL(11.0) / ALWAN_LITERAL(23.0)) * (a - ALWAN_LITERAL(9.0) * b);
    alwan_scalar La = ALWAN_LITERAL(9.0) * b - Ma_val + ALWAN_LITERAL(2.0) * Sa;

    /* Inverse Naka-Rushton: adapted -> cone response */
    alwan_scalar k = ALWAN_ONE / (ALWAN_LITERAL(5.0) * L_a + ALWAN_ONE);
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k * k * k * k * (ALWAN_LITERAL(5.0) * L_a)
                    + ALWAN_LITERAL(0.1) * ALWAN_POW(ALWAN_ONE - k * k * k * k, ALWAN_LITERAL(2.0))
                    * ALWAN_POW(ALWAN_LITERAL(5.0) * L_a, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    alwan_scalar p = ALWAN_LITERAL(0.42);
    alwan_scalar inv_p = ALWAN_ONE / p;

    /* Inverse of: adapted = 400 * tmp / (tmp + 27.13) + 0.1
     * => tmp = 27.13 * (adapted - 0.1) / (400 - (adapted - 0.1)) */
    alwan_scalar La_s = La - ALWAN_LITERAL(0.1);
    alwan_scalar Ma_s = Ma_val - ALWAN_LITERAL(0.1);
    alwan_scalar Sa_s = Sa - ALWAN_LITERAL(0.1);
    alwan_scalar La_m = ALWAN_ABS(La_s);
    alwan_scalar Ma_m = ALWAN_ABS(Ma_s);
    alwan_scalar Sa_m = ALWAN_ABS(Sa_s);

    alwan_scalar denom_L = ALWAN_SELECT(ALWAN_ABS(ALWAN_LITERAL(400.0) - La_m) < ALWAN_LITERAL(1e-10),
                                         ALWAN_LITERAL(1e-10), ALWAN_LITERAL(400.0) - La_m);
    alwan_scalar denom_M = ALWAN_SELECT(ALWAN_ABS(ALWAN_LITERAL(400.0) - Ma_m) < ALWAN_LITERAL(1e-10),
                                         ALWAN_LITERAL(1e-10), ALWAN_LITERAL(400.0) - Ma_m);
    alwan_scalar denom_S = ALWAN_SELECT(ALWAN_ABS(ALWAN_LITERAL(400.0) - Sa_m) < ALWAN_LITERAL(1e-10),
                                         ALWAN_LITERAL(1e-10), ALWAN_LITERAL(400.0) - Sa_m);

    alwan_scalar tmp_L = ALWAN_LITERAL(27.13) * La_m / denom_L;
    alwan_scalar tmp_M = ALWAN_LITERAL(27.13) * Ma_m / denom_M;
    alwan_scalar tmp_S = ALWAN_LITERAL(27.13) * Sa_m / denom_S;

    alwan_scalar tmp_L_safe = ALWAN_SELECT(tmp_L < ALWAN_ZERO, ALWAN_ZERO, tmp_L);
    alwan_scalar tmp_M_safe = ALWAN_SELECT(tmp_M < ALWAN_ZERO, ALWAN_ZERO, tmp_M);
    alwan_scalar tmp_S_safe = ALWAN_SELECT(tmp_S < ALWAN_ZERO, ALWAN_ZERO, tmp_S);

    alwan_scalar FL_safe = ALWAN_SELECT(FL < ALWAN_LITERAL(1e-10),
                                         ALWAN_LITERAL(1e-10), FL);
    alwan_scalar Lc_abs = (ALWAN_LITERAL(100.0) / FL_safe) * ALWAN_POW(tmp_L_safe, inv_p);
    alwan_scalar Mc_abs = (ALWAN_LITERAL(100.0) / FL_safe) * ALWAN_POW(tmp_M_safe, inv_p);
    alwan_scalar Sc_abs = (ALWAN_LITERAL(100.0) / FL_safe) * ALWAN_POW(tmp_S_safe, inv_p);
    alwan_scalar Lc = ALWAN_SELECT(La_s < ALWAN_ZERO, -Lc_abs, Lc_abs);
    alwan_scalar Mc = ALWAN_SELECT(Ma_s < ALWAN_ZERO, -Mc_abs, Mc_abs);
    alwan_scalar Sc = ALWAN_SELECT(Sa_s < ALWAN_ZERO, -Sc_abs, Sc_abs);

    /* LMS to XYZ: the forward CAM16 matrix inverted exactly (Cramer), not its
     * eight-digit printed inverse, so the round trip closes to rounding. */
    alwan_scalar m00 = ALWAN_LITERAL( 0.401288), m01 = ALWAN_LITERAL( 0.650173), m02 = ALWAN_LITERAL(-0.051461);
    alwan_scalar m10 = ALWAN_LITERAL(-0.250268), m11 = ALWAN_LITERAL( 1.204414), m12 = ALWAN_LITERAL( 0.045854);
    alwan_scalar m20 = ALWAN_LITERAL(-0.002079), m21 = ALWAN_LITERAL( 0.048952), m22 = ALWAN_LITERAL( 0.953127);
    alwan_scalar c00 = m11 * m22 - m12 * m21, c01 = m02 * m21 - m01 * m22, c02 = m01 * m12 - m02 * m11;
    alwan_scalar c10 = m12 * m20 - m10 * m22, c11 = m00 * m22 - m02 * m20, c12 = m02 * m10 - m00 * m12;
    alwan_scalar c20 = m10 * m21 - m11 * m20, c21 = m01 * m20 - m00 * m21, c22 = m00 * m11 - m01 * m10;
    alwan_scalar det = m00 * c00 + m01 * c10 + m02 * c20;
    result.x = (c00 * Lc + c01 * Mc + c02 * Sc) / det;
    result.y = (c10 * Lc + c11 * Mc + c12 * Sc) / det;
    result.z = (c20 * Lc + c21 * Mc + c22 * Sc) / det;

    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_CAM20U_CORE_H */
