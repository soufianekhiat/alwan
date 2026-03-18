/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only CAM20u Color Appearance Model for unrelated color (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Kim & Park (2020) "Color appearance model for unrelated color"
 */

/* ================================================================
 * CAM20u Correlates
 * ================================================================ */

typedef struct {
    ALWAN_CORE_T Q;   /* Brightness */
    ALWAN_CORE_T M;   /* Colorfulness */
    ALWAN_CORE_T h;   /* Hue angle (degrees) */
    ALWAN_CORE_T C;   /* Chroma */
    ALWAN_CORE_T s;   /* Saturation */
    ALWAN_CORE_T a;   /* Red-green */
    ALWAN_CORE_T b;   /* Yellow-blue */
} ALWAN_CORE_FNLIT(alwan_cam20u_v_correlates);

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

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_cam20u_v_correlates) ALWAN_CORE_FNVLIT(alwan_cam20u_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_T Y_b,
    ALWAN_CORE_T L_a) {
    ALWAN_CORE_FNLIT(alwan_cam20u_v_correlates) result;

    /* Step 1: XYZ to Hunt-Pointer-Estevez (HPE) cone response
     * Uses the sharpened cone response from CAM16 */
    ALWAN_CORE_T L = ALWAN_CORE_LITERAL( 0.401288) * xyz.x
                   + ALWAN_CORE_LITERAL( 0.650173) * xyz.y
                   + ALWAN_CORE_LITERAL(-0.051461) * xyz.z;
    ALWAN_CORE_T M = ALWAN_CORE_LITERAL(-0.250268) * xyz.x
                   + ALWAN_CORE_LITERAL( 1.204414) * xyz.y
                   + ALWAN_CORE_LITERAL( 0.045854) * xyz.z;
    ALWAN_CORE_T S = ALWAN_CORE_LITERAL(-0.002079) * xyz.x
                   + ALWAN_CORE_LITERAL( 0.048952) * xyz.y
                   + ALWAN_CORE_LITERAL( 0.953127) * xyz.z;

    /* Step 2: Luminance adaptation factor (Hunt model) */
    ALWAN_CORE_T k = ALWAN_CORE_ONE / (ALWAN_CORE_LITERAL(5.0) * L_a + ALWAN_CORE_ONE);
    ALWAN_CORE_T FL = ALWAN_CORE_LITERAL(0.2) * k * k * k * k * (ALWAN_CORE_LITERAL(5.0) * L_a)
                    + ALWAN_CORE_LITERAL(0.1) * ALWAN_CORE_POW(ALWAN_CORE_ONE - k * k * k * k, ALWAN_CORE_LITERAL(2.0))
                    * ALWAN_CORE_POW(ALWAN_CORE_LITERAL(5.0) * L_a, ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(3.0));

    /* Step 3: Nonlinear cone response (adapted, power-function compression) */
    ALWAN_CORE_T p = ALWAN_CORE_LITERAL(0.42);
    ALWAN_CORE_T L_abs = ALWAN_CORE_ABS(L);
    ALWAN_CORE_T M_abs = ALWAN_CORE_ABS(M);
    ALWAN_CORE_T S_abs = ALWAN_CORE_ABS(S);

    ALWAN_CORE_T L_safe = ALWAN_CORE_SELECT(L_abs < ALWAN_CORE_LITERAL(1e-12),
                                        ALWAN_CORE_LITERAL(1e-12), L_abs);
    ALWAN_CORE_T M_safe = ALWAN_CORE_SELECT(M_abs < ALWAN_CORE_LITERAL(1e-12),
                                        ALWAN_CORE_LITERAL(1e-12), M_abs);
    ALWAN_CORE_T S_safe = ALWAN_CORE_SELECT(S_abs < ALWAN_CORE_LITERAL(1e-12),
                                        ALWAN_CORE_LITERAL(1e-12), S_abs);

    /* Naka-Rushton-type response compression */
    ALWAN_CORE_T tmp_L = ALWAN_CORE_POW(FL * L_safe / ALWAN_CORE_LITERAL(100.0), p);
    ALWAN_CORE_T tmp_M = ALWAN_CORE_POW(FL * M_safe / ALWAN_CORE_LITERAL(100.0), p);
    ALWAN_CORE_T tmp_S = ALWAN_CORE_POW(FL * S_safe / ALWAN_CORE_LITERAL(100.0), p);

    ALWAN_CORE_T La = ALWAN_CORE_LITERAL(400.0) * tmp_L / (tmp_L + ALWAN_CORE_LITERAL(27.13)) + ALWAN_CORE_LITERAL(0.1);
    ALWAN_CORE_T Ma_val = ALWAN_CORE_LITERAL(400.0) * tmp_M / (tmp_M + ALWAN_CORE_LITERAL(27.13)) + ALWAN_CORE_LITERAL(0.1);
    ALWAN_CORE_T Sa = ALWAN_CORE_LITERAL(400.0) * tmp_S / (tmp_S + ALWAN_CORE_LITERAL(27.13)) + ALWAN_CORE_LITERAL(0.1);

    /* Step 4: Opponent channels */
    result.a = La - ALWAN_CORE_LITERAL(12.0) / ALWAN_CORE_LITERAL(11.0) * Ma_val
             + Sa / ALWAN_CORE_LITERAL(11.0);
    result.b = (La + Ma_val - ALWAN_CORE_LITERAL(2.0) * Sa) / ALWAN_CORE_LITERAL(9.0);

    /* Step 5: Hue angle */
    result.h = ALWAN_CORE_ATAN2(result.b, result.a) * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    result.h = ALWAN_CORE_SELECT(result.h < ALWAN_CORE_ZERO,
                            result.h + ALWAN_CORE_LITERAL(360.0), result.h);

    /* Step 6: Achromatic response */
    ALWAN_CORE_T A = (ALWAN_CORE_LITERAL(2.0) * La + Ma_val
                    + ALWAN_CORE_LITERAL(0.05) * Sa - ALWAN_CORE_LITERAL(0.305));

    /* Step 7: Brightness */
    ALWAN_CORE_T Y_b_safe = ALWAN_CORE_SELECT(Y_b < ALWAN_CORE_LITERAL(1e-10),
                                          ALWAN_CORE_LITERAL(1e-10), Y_b);
    ALWAN_CORE_T n = Y_b_safe / ALWAN_CORE_LITERAL(100.0);
    ALWAN_CORE_T z = ALWAN_CORE_LITERAL(1.48) + ALWAN_CORE_SQRT(n);
    ALWAN_CORE_T A_safe = ALWAN_CORE_SELECT(A < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, A);
    result.Q = ALWAN_CORE_LITERAL(11.0) * ALWAN_CORE_POW(A_safe / ALWAN_CORE_LITERAL(100.0), z);

    /* Step 8: Colorfulness and Chroma */
    ALWAN_CORE_T et = ALWAN_CORE_LITERAL(0.25) * (ALWAN_CORE_COS(result.h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0) + ALWAN_CORE_LITERAL(2.0)) + ALWAN_CORE_LITERAL(3.8));
    result.M = ALWAN_CORE_SQRT(result.a * result.a + result.b * result.b) * et;
    ALWAN_CORE_T Q_safe = ALWAN_CORE_SELECT(result.Q < ALWAN_CORE_LITERAL(1e-10),
                                        ALWAN_CORE_LITERAL(1e-10), result.Q);
    result.C = result.M / Q_safe * ALWAN_CORE_LITERAL(100.0);

    /* Step 9: Saturation */
    result.s = ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_SQRT(result.M / Q_safe);

    return result;
}

/* ================================================================
 * CAM20u Inverse Transform
 *
 * CAM20u correlates -> XYZ
 * Uses Q, M, h to reconstruct XYZ.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_cam20u_inverse)(
    ALWAN_CORE_FNLIT(alwan_cam20u_v_correlates) correlates,
    ALWAN_CORE_T Y_b,
    ALWAN_CORE_T L_a) {
    ALWAN_CORE_XYZ result;

    /* Reconstruct achromatic response from Q */
    ALWAN_CORE_T Y_b_safe = ALWAN_CORE_SELECT(Y_b < ALWAN_CORE_LITERAL(1e-10),
                                          ALWAN_CORE_LITERAL(1e-10), Y_b);
    ALWAN_CORE_T n = Y_b_safe / ALWAN_CORE_LITERAL(100.0);
    ALWAN_CORE_T z = ALWAN_CORE_LITERAL(1.48) + ALWAN_CORE_SQRT(n);
    ALWAN_CORE_T safe_z = ALWAN_CORE_SELECT(z < ALWAN_CORE_LITERAL(1e-10),
                                        ALWAN_CORE_LITERAL(1e-10), z);
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_POW(correlates.Q / ALWAN_CORE_LITERAL(11.0),
        ALWAN_CORE_ONE / safe_z);

    /* Reconstruct a, b from M and h */
    ALWAN_CORE_T h_rad = correlates.h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_T et = ALWAN_CORE_LITERAL(0.25) * (ALWAN_CORE_COS(h_rad + ALWAN_CORE_LITERAL(2.0)) + ALWAN_CORE_LITERAL(3.8));
    ALWAN_CORE_T et_safe = ALWAN_CORE_SELECT(et < ALWAN_CORE_LITERAL(1e-10),
                                         ALWAN_CORE_LITERAL(1e-10), et);
    ALWAN_CORE_T t = correlates.M / et_safe;
    ALWAN_CORE_T a = t * ALWAN_CORE_COS(h_rad);
    ALWAN_CORE_T b = t * ALWAN_CORE_SIN(h_rad);

    /* Reconstruct La, Ma, Sa from a, b, A
     * a = La - (12/11)*Ma + (1/11)*Sa
     * b = (1/9)*La + (1/9)*Ma - (2/9)*Sa
     * A+0.305 = 2*La + Ma + 0.05*Sa
     */
    ALWAN_CORE_T A_full = A + ALWAN_CORE_LITERAL(0.305);

    /* Solving the linear system using Cramer's rule or substitution */
    ALWAN_CORE_T c_a = ALWAN_CORE_LITERAL(11.0) / ALWAN_CORE_LITERAL(23.0);
    ALWAN_CORE_T c_b = ALWAN_CORE_LITERAL(315.0) / ALWAN_CORE_LITERAL(23.0);
    ALWAN_CORE_T Sa = (A_full - c_a * a - c_b * b) / ALWAN_CORE_LITERAL(3.05);
    ALWAN_CORE_T Ma_val = Sa - (ALWAN_CORE_LITERAL(11.0) / ALWAN_CORE_LITERAL(23.0)) * (a - ALWAN_CORE_LITERAL(9.0) * b);
    ALWAN_CORE_T La = ALWAN_CORE_LITERAL(9.0) * b - Ma_val + ALWAN_CORE_LITERAL(2.0) * Sa;

    /* Inverse Naka-Rushton: adapted -> cone response */
    ALWAN_CORE_T k = ALWAN_CORE_ONE / (ALWAN_CORE_LITERAL(5.0) * L_a + ALWAN_CORE_ONE);
    ALWAN_CORE_T FL = ALWAN_CORE_LITERAL(0.2) * k * k * k * k * (ALWAN_CORE_LITERAL(5.0) * L_a)
                    + ALWAN_CORE_LITERAL(0.1) * ALWAN_CORE_POW(ALWAN_CORE_ONE - k * k * k * k, ALWAN_CORE_LITERAL(2.0))
                    * ALWAN_CORE_POW(ALWAN_CORE_LITERAL(5.0) * L_a, ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(3.0));

    ALWAN_CORE_T p = ALWAN_CORE_LITERAL(0.42);
    ALWAN_CORE_T inv_p = ALWAN_CORE_ONE / p;

    /* Inverse of: adapted = 400 * tmp / (tmp + 27.13) + 0.1
     * => tmp = 27.13 * (adapted - 0.1) / (400 - (adapted - 0.1)) */
    ALWAN_CORE_T La_m = La - ALWAN_CORE_LITERAL(0.1);
    ALWAN_CORE_T Ma_m = Ma_val - ALWAN_CORE_LITERAL(0.1);
    ALWAN_CORE_T Sa_m = Sa - ALWAN_CORE_LITERAL(0.1);

    ALWAN_CORE_T denom_L = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(ALWAN_CORE_LITERAL(400.0) - La_m) < ALWAN_CORE_LITERAL(1e-10),
                                         ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(400.0) - La_m);
    ALWAN_CORE_T denom_M = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(ALWAN_CORE_LITERAL(400.0) - Ma_m) < ALWAN_CORE_LITERAL(1e-10),
                                         ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(400.0) - Ma_m);
    ALWAN_CORE_T denom_S = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(ALWAN_CORE_LITERAL(400.0) - Sa_m) < ALWAN_CORE_LITERAL(1e-10),
                                         ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(400.0) - Sa_m);

    ALWAN_CORE_T tmp_L = ALWAN_CORE_LITERAL(27.13) * La_m / denom_L;
    ALWAN_CORE_T tmp_M = ALWAN_CORE_LITERAL(27.13) * Ma_m / denom_M;
    ALWAN_CORE_T tmp_S = ALWAN_CORE_LITERAL(27.13) * Sa_m / denom_S;

    ALWAN_CORE_T tmp_L_safe = ALWAN_CORE_SELECT(tmp_L < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, tmp_L);
    ALWAN_CORE_T tmp_M_safe = ALWAN_CORE_SELECT(tmp_M < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, tmp_M);
    ALWAN_CORE_T tmp_S_safe = ALWAN_CORE_SELECT(tmp_S < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, tmp_S);

    ALWAN_CORE_T FL_safe = ALWAN_CORE_SELECT(FL < ALWAN_CORE_LITERAL(1e-10),
                                         ALWAN_CORE_LITERAL(1e-10), FL);
    ALWAN_CORE_T Lc = (ALWAN_CORE_LITERAL(100.0) / FL_safe) * ALWAN_CORE_POW(tmp_L_safe, inv_p);
    ALWAN_CORE_T Mc = (ALWAN_CORE_LITERAL(100.0) / FL_safe) * ALWAN_CORE_POW(tmp_M_safe, inv_p);
    ALWAN_CORE_T Sc = (ALWAN_CORE_LITERAL(100.0) / FL_safe) * ALWAN_CORE_POW(tmp_S_safe, inv_p);

    /* LMS to XYZ (inverse of HPE / CAM16 matrix) */
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
