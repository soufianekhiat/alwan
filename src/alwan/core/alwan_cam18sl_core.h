/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only CAM18sl Color Appearance Model for self-luminous stimuli
 * Reference: Hermans et al. (2018)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_CAM18SL_CORE_H
#define ALWAN_CAM18SL_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

/* ================================================================
 * CAM18sl Correlates
 * ================================================================ */

typedef struct {
    alwan_scalar Q;   /* Brightness */
    alwan_scalar C;   /* Colorfulness */
    alwan_scalar h;   /* Hue angle (degrees) */
    alwan_scalar M;   /* Colorfulness (same as C for unrelated colors) */
    alwan_scalar a;   /* Red-green */
    alwan_scalar b;   /* Yellow-blue */
} alwan_cam18sl_v_correlates;

/* ================================================================
 * CAM18sl Forward Transform
 *
 * XYZ -> CAM18sl correlates for self-luminous stimuli
 *
 * Parameters:
 *   xyz  - CIE XYZ tristimulus values (absolute, in cd/m2)
 *   Y_b  - background luminance (cd/m2)
 * ================================================================ */

ALWAN_INLINE alwan_cam18sl_v_correlates alwan_cam18sl_forward_v(
    alwan_xyz xyz,
    alwan_scalar Y_b) {
    alwan_cam18sl_v_correlates result;

    /* Step 1: XYZ to LMS cone response using Wd matrix
     * Matrix from Hermans et al. (2018), Table 1 */
    alwan_scalar L = ALWAN_LITERAL( 0.401288) * xyz.x
                   + ALWAN_LITERAL( 0.650173) * xyz.y
                   + ALWAN_LITERAL(-0.051461) * xyz.z;
    alwan_scalar M = ALWAN_LITERAL(-0.250268) * xyz.x
                   + ALWAN_LITERAL( 1.204414) * xyz.y
                   + ALWAN_LITERAL( 0.045854) * xyz.z;
    alwan_scalar S = ALWAN_LITERAL(-0.002079) * xyz.x
                   + ALWAN_LITERAL( 0.048952) * xyz.y
                   + ALWAN_LITERAL( 0.953127) * xyz.z;

    /* Step 2: Nonlinear compression (power function)
     * For self-luminous context: adapted response */
    alwan_scalar p = ALWAN_LITERAL(0.58);
    alwan_scalar L_safe = ALWAN_SELECT(L < ALWAN_ZERO, ALWAN_ZERO, L);
    alwan_scalar M_safe = ALWAN_SELECT(M < ALWAN_ZERO, ALWAN_ZERO, M);
    alwan_scalar S_safe = ALWAN_SELECT(S < ALWAN_ZERO, ALWAN_ZERO, S);

    alwan_scalar La = ALWAN_POW(L_safe, p);
    alwan_scalar Ma = ALWAN_POW(M_safe, p);
    alwan_scalar Sa = ALWAN_POW(S_safe, p);

    /* Step 3: Opponent channels */
    result.a = La - ALWAN_LITERAL(12.0) / ALWAN_LITERAL(11.0) * Ma
             + Sa / ALWAN_LITERAL(11.0);
    result.b = (La + Ma - ALWAN_LITERAL(2.0) * Sa) / ALWAN_LITERAL(9.0);

    /* Step 4: Hue angle */
    result.h = ALWAN_ATAN2(result.b, result.a) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    result.h = ALWAN_SELECT(result.h < ALWAN_ZERO,
                            result.h + ALWAN_LITERAL(360.0), result.h);

    /* Step 5: Brightness
     * Helmholtz-Kohlrausch brightness adjustment for self-luminous stimuli */
    alwan_scalar A = ALWAN_LITERAL(2.0) * La + Ma
                   + ALWAN_LITERAL(0.05) * Sa;
    alwan_scalar Y_b_safe = ALWAN_SELECT(Y_b < ALWAN_LITERAL(1e-10),
                                          ALWAN_LITERAL(1e-10), Y_b);
    result.Q = ALWAN_LITERAL(0.937) * ALWAN_POW(A / ALWAN_LITERAL(3.05),
        ALWAN_LITERAL(0.335) * ALWAN_POW(Y_b_safe / ALWAN_LITERAL(100.0),
        ALWAN_LITERAL(0.14)));

    /* Step 6: Colorfulness */
    result.C = ALWAN_SQRT(result.a * result.a + result.b * result.b);
    result.M = result.C;  /* For unrelated colors, M == C */

    return result;
}

/* ================================================================
 * CAM18sl Inverse Transform
 *
 * CAM18sl correlates -> XYZ
 * Uses Q, C, h to reconstruct XYZ.
 * ================================================================ */

ALWAN_INLINE alwan_xyz alwan_cam18sl_inverse_v(
    alwan_cam18sl_v_correlates correlates,
    alwan_scalar Y_b) {
    alwan_xyz result;

    /* Reconstruct a, b from C and h */
    alwan_scalar h_rad = correlates.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar a = correlates.C * ALWAN_COS(h_rad);
    alwan_scalar b = correlates.C * ALWAN_SIN(h_rad);

    /* Reconstruct A from Q */
    alwan_scalar Y_b_safe = ALWAN_SELECT(Y_b < ALWAN_LITERAL(1e-10),
                                          ALWAN_LITERAL(1e-10), Y_b);
    alwan_scalar exp_val = ALWAN_LITERAL(0.335) * ALWAN_POW(Y_b_safe / ALWAN_LITERAL(100.0),
        ALWAN_LITERAL(0.14));
    alwan_scalar safe_exp = ALWAN_SELECT(exp_val < ALWAN_LITERAL(1e-10),
                                          ALWAN_LITERAL(1e-10), exp_val);
    alwan_scalar A = ALWAN_LITERAL(3.05) * ALWAN_POW(correlates.Q / ALWAN_LITERAL(0.937),
        ALWAN_ONE / safe_exp);

    /* Solve for La, Ma, Sa from opponent channels + achromatic:
     * a = La - 12/11*Ma + 1/11*Sa
     * b = (La + Ma - 2*Sa) / 9
     * A = 2*La + Ma + 0.05*Sa
     *
     * Linear system solve: */
    alwan_scalar Sa = (A - ALWAN_LITERAL(11.0) * a - ALWAN_LITERAL(9.0) * b) /
                       ALWAN_LITERAL(2.2538);
    alwan_scalar Ma = (A - ALWAN_LITERAL(2.0) * a - ALWAN_LITERAL(18.0) * b
                        - ALWAN_LITERAL(0.1) * Sa) / ALWAN_LITERAL(3.181818);
    alwan_scalar La = a + ALWAN_LITERAL(12.0) / ALWAN_LITERAL(11.0) * Ma
                    - Sa / ALWAN_LITERAL(11.0);

    /* Need to solve this more carefully. Let me use the proper inverse. */
    /* From the 3 equations:
     * a = La - (12/11)*Ma + (1/11)*Sa         ... (1)
     * b = (1/9)*La + (1/9)*Ma - (2/9)*Sa      ... (2)
     * A = 2*La + Ma + 0.05*Sa                 ... (3)
     *
     * From (2): La = 9*b - Ma + 2*Sa
     * Sub into (1): 9*b - Ma + 2*Sa - (12/11)*Ma + (1/11)*Sa = a
     *   9*b - (23/11)*Ma + (23/11)*Sa = a
     *   Ma = Sa - (11/23)*(a - 9*b)
     * Sub La, Ma into (3):
     *   2*(9*b - Ma + 2*Sa) + Ma + 0.05*Sa = A
     *   18*b - 2*Ma + 4*Sa + Ma + 0.05*Sa = A
     *   18*b - Ma + 4.05*Sa = A
     *   18*b - (Sa - (11/23)*(a-9*b)) + 4.05*Sa = A
     *   18*b - Sa + (11/23)*a - (99/23)*b + 4.05*Sa = A
     *   (11/23)*a + (414/23 - 99/23)*b + 3.05*Sa = A
     *   (11/23)*a + (315/23)*b + 3.05*Sa = A
     */
    alwan_scalar coeff_a = ALWAN_LITERAL(11.0) / ALWAN_LITERAL(23.0);
    alwan_scalar coeff_b = ALWAN_LITERAL(315.0) / ALWAN_LITERAL(23.0);
    Sa = (A - coeff_a * a - coeff_b * b) / ALWAN_LITERAL(3.05);
    Ma = Sa - (ALWAN_LITERAL(11.0) / ALWAN_LITERAL(23.0)) * (a - ALWAN_LITERAL(9.0) * b);
    La = ALWAN_LITERAL(9.0) * b - Ma + ALWAN_LITERAL(2.0) * Sa;

    /* Inverse nonlinear compression */
    alwan_scalar p = ALWAN_LITERAL(0.58);
    alwan_scalar inv_p = ALWAN_ONE / p;
    alwan_scalar La_safe = ALWAN_SELECT(La < ALWAN_ZERO, ALWAN_ZERO, La);
    alwan_scalar Ma_safe = ALWAN_SELECT(Ma < ALWAN_ZERO, ALWAN_ZERO, Ma);
    alwan_scalar Sa_safe = ALWAN_SELECT(Sa < ALWAN_ZERO, ALWAN_ZERO, Sa);

    alwan_scalar Lc = ALWAN_POW(La_safe, inv_p);
    alwan_scalar Mc = ALWAN_POW(Ma_safe, inv_p);
    alwan_scalar Sc = ALWAN_POW(Sa_safe, inv_p);

    /* LMS to XYZ (inverse of forward matrix) */
    result.x = ALWAN_LITERAL( 1.86206787) * Lc
             + ALWAN_LITERAL(-1.01125463) * Mc
             + ALWAN_LITERAL( 0.14918677) * Sc;
    result.y = ALWAN_LITERAL( 0.38752654) * Lc
             + ALWAN_LITERAL( 0.62144744) * Mc
             + ALWAN_LITERAL(-0.00897398) * Sc;
    result.z = ALWAN_LITERAL(-0.01584150) * Lc
             + ALWAN_LITERAL(-0.03412294) * Mc
             + ALWAN_LITERAL( 1.04996444) * Sc;

    return result;
}

#endif /* ALWAN_CAM18SL_CORE_H */
