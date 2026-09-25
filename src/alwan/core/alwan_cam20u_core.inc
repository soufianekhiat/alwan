/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only CAM20u Color Appearance Model for unrelated color (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * NOT the published CAM20u. The model of that name is Gao, Li, Luo, Pointer et al. (2021),
 * "CAM20u: an extension of CAM16 for predicting the color appearance of unrelated colors",
 * Color Res. Appl. 46(3); an earlier comment here cited a "Kim & Park (2020)" that does not
 * exist. What follows is alwan's own CAM16-shaped model for unrelated colours, with no
 * published definition or open implementation to check it against. Its forward and inverse
 * are exact inverses of each other (suite 260): the cone compression and the brightness
 * power keep their sign, as CAM16's do, so a stimulus with a negative cone signal comes back.
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

    /* Naka-Rushton-type response compression, keeping the sign (CAM16's form) */
    ALWAN_CORE_T tmp_L = ALWAN_CORE_POW(FL * L_abs / ALWAN_CORE_LITERAL(100.0), p);
    ALWAN_CORE_T tmp_M = ALWAN_CORE_POW(FL * M_abs / ALWAN_CORE_LITERAL(100.0), p);
    ALWAN_CORE_T tmp_S = ALWAN_CORE_POW(FL * S_abs / ALWAN_CORE_LITERAL(100.0), p);

    ALWAN_CORE_T cL = ALWAN_CORE_LITERAL(400.0) * tmp_L / (tmp_L + ALWAN_CORE_LITERAL(27.13));
    ALWAN_CORE_T cM = ALWAN_CORE_LITERAL(400.0) * tmp_M / (tmp_M + ALWAN_CORE_LITERAL(27.13));
    ALWAN_CORE_T cS = ALWAN_CORE_LITERAL(400.0) * tmp_S / (tmp_S + ALWAN_CORE_LITERAL(27.13));
    ALWAN_CORE_T La = ALWAN_CORE_SELECT(L < ALWAN_CORE_ZERO, -cL, cL) + ALWAN_CORE_LITERAL(0.1);
    ALWAN_CORE_T Ma_val = ALWAN_CORE_SELECT(M < ALWAN_CORE_ZERO, -cM, cM) + ALWAN_CORE_LITERAL(0.1);
    ALWAN_CORE_T Sa = ALWAN_CORE_SELECT(S < ALWAN_CORE_ZERO, -cS, cS) + ALWAN_CORE_LITERAL(0.1);

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
    ALWAN_CORE_T A_abs = ALWAN_CORE_ABS(A);
    ALWAN_CORE_T Q_abs = ALWAN_CORE_LITERAL(11.0) * ALWAN_CORE_POW(A_abs / ALWAN_CORE_LITERAL(100.0), z);
    result.Q = ALWAN_CORE_SELECT(A < ALWAN_CORE_ZERO, -Q_abs, Q_abs);

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
    ALWAN_CORE_T Q_abs = ALWAN_CORE_ABS(correlates.Q);
    ALWAN_CORE_T A_abs = ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_POW(Q_abs / ALWAN_CORE_LITERAL(11.0),
        ALWAN_CORE_ONE / safe_z);
    ALWAN_CORE_T A = ALWAN_CORE_SELECT(correlates.Q < ALWAN_CORE_ZERO, -A_abs, A_abs);

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
    ALWAN_CORE_T La_s = La - ALWAN_CORE_LITERAL(0.1);
    ALWAN_CORE_T Ma_s = Ma_val - ALWAN_CORE_LITERAL(0.1);
    ALWAN_CORE_T Sa_s = Sa - ALWAN_CORE_LITERAL(0.1);
    ALWAN_CORE_T La_m = ALWAN_CORE_ABS(La_s);
    ALWAN_CORE_T Ma_m = ALWAN_CORE_ABS(Ma_s);
    ALWAN_CORE_T Sa_m = ALWAN_CORE_ABS(Sa_s);

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
    ALWAN_CORE_T Lc_abs = (ALWAN_CORE_LITERAL(100.0) / FL_safe) * ALWAN_CORE_POW(tmp_L_safe, inv_p);
    ALWAN_CORE_T Mc_abs = (ALWAN_CORE_LITERAL(100.0) / FL_safe) * ALWAN_CORE_POW(tmp_M_safe, inv_p);
    ALWAN_CORE_T Sc_abs = (ALWAN_CORE_LITERAL(100.0) / FL_safe) * ALWAN_CORE_POW(tmp_S_safe, inv_p);
    ALWAN_CORE_T Lc = ALWAN_CORE_SELECT(La_s < ALWAN_CORE_ZERO, -Lc_abs, Lc_abs);
    ALWAN_CORE_T Mc = ALWAN_CORE_SELECT(Ma_s < ALWAN_CORE_ZERO, -Mc_abs, Mc_abs);
    ALWAN_CORE_T Sc = ALWAN_CORE_SELECT(Sa_s < ALWAN_CORE_ZERO, -Sc_abs, Sc_abs);

    /* LMS to XYZ: the forward CAM16 matrix inverted exactly (Cramer), not its
     * eight-digit printed inverse, so the round trip closes to rounding. */
    ALWAN_CORE_T m00 = ALWAN_CORE_LITERAL( 0.401288), m01 = ALWAN_CORE_LITERAL( 0.650173), m02 = ALWAN_CORE_LITERAL(-0.051461);
    ALWAN_CORE_T m10 = ALWAN_CORE_LITERAL(-0.250268), m11 = ALWAN_CORE_LITERAL( 1.204414), m12 = ALWAN_CORE_LITERAL( 0.045854);
    ALWAN_CORE_T m20 = ALWAN_CORE_LITERAL(-0.002079), m21 = ALWAN_CORE_LITERAL( 0.048952), m22 = ALWAN_CORE_LITERAL( 0.953127);
    ALWAN_CORE_T c00 = m11 * m22 - m12 * m21, c01 = m02 * m21 - m01 * m22, c02 = m01 * m12 - m02 * m11;
    ALWAN_CORE_T c10 = m12 * m20 - m10 * m22, c11 = m00 * m22 - m02 * m20, c12 = m02 * m10 - m00 * m12;
    ALWAN_CORE_T c20 = m10 * m21 - m11 * m20, c21 = m01 * m20 - m00 * m21, c22 = m00 * m11 - m01 * m10;
    ALWAN_CORE_T det = m00 * c00 + m01 * c10 + m02 * c20;
    result.x = (c00 * Lc + c01 * Mc + c02 * Sc) / det;
    result.y = (c10 * Lc + c11 * Mc + c12 * Sc) / det;
    result.z = (c20 * Lc + c21 * Mc + c22 * Sc) / det;

    return result;
}
