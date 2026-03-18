/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only CAM18sl Color Appearance Model for self-luminous stimuli (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Hermans et al. (2018)
 */

/* ================================================================
 * CAM18sl Correlates
 * ================================================================ */

typedef struct {
    ALWAN_CORE_T Q;   /* Brightness */
    ALWAN_CORE_T C;   /* Colorfulness */
    ALWAN_CORE_T h;   /* Hue angle (degrees) */
    ALWAN_CORE_T M;   /* Colorfulness (same as C for unrelated colors) */
    ALWAN_CORE_T a;   /* Red-green */
    ALWAN_CORE_T b;   /* Yellow-blue */
} ALWAN_CORE_FNLIT(alwan_cam18sl_v_correlates);

/* ================================================================
 * CAM18sl Forward Transform
 *
 * XYZ -> CAM18sl correlates for self-luminous stimuli
 *
 * Parameters:
 *   xyz  - CIE XYZ tristimulus values (absolute, in cd/m2)
 *   Y_b  - background luminance (cd/m2)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_cam18sl_v_correlates) ALWAN_CORE_FNVLIT(alwan_cam18sl_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_T Y_b) {
    ALWAN_CORE_FNLIT(alwan_cam18sl_v_correlates) result;

    /* Step 1: XYZ to LMS cone response using Wd matrix
     * Matrix from Hermans et al. (2018), Table 1 */
    ALWAN_CORE_T L = ALWAN_CORE_LITERAL( 0.401288) * xyz.x
                   + ALWAN_CORE_LITERAL( 0.650173) * xyz.y
                   + ALWAN_CORE_LITERAL(-0.051461) * xyz.z;
    ALWAN_CORE_T M = ALWAN_CORE_LITERAL(-0.250268) * xyz.x
                   + ALWAN_CORE_LITERAL( 1.204414) * xyz.y
                   + ALWAN_CORE_LITERAL( 0.045854) * xyz.z;
    ALWAN_CORE_T S = ALWAN_CORE_LITERAL(-0.002079) * xyz.x
                   + ALWAN_CORE_LITERAL( 0.048952) * xyz.y
                   + ALWAN_CORE_LITERAL( 0.953127) * xyz.z;

    /* Step 2: Nonlinear compression (power function)
     * For self-luminous context: adapted response */
    ALWAN_CORE_T p = ALWAN_CORE_LITERAL(0.58);
    ALWAN_CORE_T L_safe = ALWAN_CORE_SELECT(L < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, L);
    ALWAN_CORE_T M_safe = ALWAN_CORE_SELECT(M < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, M);
    ALWAN_CORE_T S_safe = ALWAN_CORE_SELECT(S < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, S);

    ALWAN_CORE_T La = ALWAN_CORE_POW(L_safe, p);
    ALWAN_CORE_T Ma = ALWAN_CORE_POW(M_safe, p);
    ALWAN_CORE_T Sa = ALWAN_CORE_POW(S_safe, p);

    /* Step 3: Opponent channels */
    result.a = La - ALWAN_CORE_LITERAL(12.0) / ALWAN_CORE_LITERAL(11.0) * Ma
             + Sa / ALWAN_CORE_LITERAL(11.0);
    result.b = (La + Ma - ALWAN_CORE_LITERAL(2.0) * Sa) / ALWAN_CORE_LITERAL(9.0);

    /* Step 4: Hue angle */
    result.h = ALWAN_CORE_ATAN2(result.b, result.a) * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    result.h = ALWAN_CORE_SELECT(result.h < ALWAN_CORE_ZERO,
                            result.h + ALWAN_CORE_LITERAL(360.0), result.h);

    /* Step 5: Brightness
     * Helmholtz-Kohlrausch brightness adjustment for self-luminous stimuli */
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(2.0) * La + Ma
                   + ALWAN_CORE_LITERAL(0.05) * Sa;
    ALWAN_CORE_T Y_b_safe = ALWAN_CORE_SELECT(Y_b < ALWAN_CORE_LITERAL(1e-10),
                                          ALWAN_CORE_LITERAL(1e-10), Y_b);
    result.Q = ALWAN_CORE_LITERAL(0.937) * ALWAN_CORE_POW(A / ALWAN_CORE_LITERAL(3.05),
        ALWAN_CORE_LITERAL(0.335) * ALWAN_CORE_POW(Y_b_safe / ALWAN_CORE_LITERAL(100.0),
        ALWAN_CORE_LITERAL(0.14)));

    /* Step 6: Colorfulness */
    result.C = ALWAN_CORE_SQRT(result.a * result.a + result.b * result.b);
    result.M = result.C;  /* For unrelated colors, M == C */

    return result;
}

/* ================================================================
 * CAM18sl Inverse Transform
 *
 * CAM18sl correlates -> XYZ
 * Uses Q, C, h to reconstruct XYZ.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_cam18sl_inverse)(
    ALWAN_CORE_FNLIT(alwan_cam18sl_v_correlates) correlates,
    ALWAN_CORE_T Y_b) {
    ALWAN_CORE_XYZ result;

    /* Reconstruct a, b from C and h */
    ALWAN_CORE_T h_rad = correlates.h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_T a = correlates.C * ALWAN_CORE_COS(h_rad);
    ALWAN_CORE_T b = correlates.C * ALWAN_CORE_SIN(h_rad);

    /* Reconstruct A from Q */
    ALWAN_CORE_T Y_b_safe = ALWAN_CORE_SELECT(Y_b < ALWAN_CORE_LITERAL(1e-10),
                                          ALWAN_CORE_LITERAL(1e-10), Y_b);
    ALWAN_CORE_T exp_val = ALWAN_CORE_LITERAL(0.335) * ALWAN_CORE_POW(Y_b_safe / ALWAN_CORE_LITERAL(100.0),
        ALWAN_CORE_LITERAL(0.14));
    ALWAN_CORE_T safe_exp = ALWAN_CORE_SELECT(exp_val < ALWAN_CORE_LITERAL(1e-10),
                                          ALWAN_CORE_LITERAL(1e-10), exp_val);
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(3.05) * ALWAN_CORE_POW(correlates.Q / ALWAN_CORE_LITERAL(0.937),
        ALWAN_CORE_ONE / safe_exp);

    /* Solve for La, Ma, Sa from opponent channels + achromatic:
     * a = La - 12/11*Ma + 1/11*Sa
     * b = (La + Ma - 2*Sa) / 9
     * A = 2*La + Ma + 0.05*Sa
     *
     * Linear system solve: */
    ALWAN_CORE_T Sa = (A - ALWAN_CORE_LITERAL(11.0) * a - ALWAN_CORE_LITERAL(9.0) * b) /
                       ALWAN_CORE_LITERAL(2.2538);
    ALWAN_CORE_T Ma = (A - ALWAN_CORE_LITERAL(2.0) * a - ALWAN_CORE_LITERAL(18.0) * b
                        - ALWAN_CORE_LITERAL(0.1) * Sa) / ALWAN_CORE_LITERAL(3.181818);
    ALWAN_CORE_T La = a + ALWAN_CORE_LITERAL(12.0) / ALWAN_CORE_LITERAL(11.0) * Ma
                    - Sa / ALWAN_CORE_LITERAL(11.0);

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
    ALWAN_CORE_T coeff_a = ALWAN_CORE_LITERAL(11.0) / ALWAN_CORE_LITERAL(23.0);
    ALWAN_CORE_T coeff_b = ALWAN_CORE_LITERAL(315.0) / ALWAN_CORE_LITERAL(23.0);
    Sa = (A - coeff_a * a - coeff_b * b) / ALWAN_CORE_LITERAL(3.05);
    Ma = Sa - (ALWAN_CORE_LITERAL(11.0) / ALWAN_CORE_LITERAL(23.0)) * (a - ALWAN_CORE_LITERAL(9.0) * b);
    La = ALWAN_CORE_LITERAL(9.0) * b - Ma + ALWAN_CORE_LITERAL(2.0) * Sa;

    /* Inverse nonlinear compression */
    ALWAN_CORE_T p = ALWAN_CORE_LITERAL(0.58);
    ALWAN_CORE_T inv_p = ALWAN_CORE_ONE / p;
    ALWAN_CORE_T La_safe = ALWAN_CORE_SELECT(La < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, La);
    ALWAN_CORE_T Ma_safe = ALWAN_CORE_SELECT(Ma < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, Ma);
    ALWAN_CORE_T Sa_safe = ALWAN_CORE_SELECT(Sa < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, Sa);

    ALWAN_CORE_T Lc = ALWAN_CORE_POW(La_safe, inv_p);
    ALWAN_CORE_T Mc = ALWAN_CORE_POW(Ma_safe, inv_p);
    ALWAN_CORE_T Sc = ALWAN_CORE_POW(Sa_safe, inv_p);

    /* LMS to XYZ (inverse of forward matrix) */
    result.x = ALWAN_CORE_LITERAL( 1.86206787) * Lc
             + ALWAN_CORE_LITERAL(-1.01125463) * Mc
             + ALWAN_CORE_LITERAL( 0.14918677) * Sc;
    result.y = ALWAN_CORE_LITERAL( 0.38752654) * Lc
             + ALWAN_CORE_LITERAL( 0.62144744) * Mc
             + ALWAN_CORE_LITERAL(-0.00897398) * Sc;
    result.z = ALWAN_CORE_LITERAL(-0.01584150) * Lc
             + ALWAN_CORE_LITERAL(-0.03412294) * Mc
             + ALWAN_CORE_LITERAL( 1.04996444) * Sc;

    return result;
}
