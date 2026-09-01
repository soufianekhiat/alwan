/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Nayatani (1995) Color Appearance Model (parameterized).
 * No include guard - included twice via setup/teardown.
 *
 * Validated bit-for-bit against colour-science `colour.XYZ_to_Nayatani95`
 * (see alwan_dev tests). Constants are the published model coefficients
 * (CIE 1994 XYZ->RGB matrix, Nayatani beta-functions, chromatic-strength
 * Fourier coefficients).
 *
 * References:
 * - Nayatani, Y., Sobagaki, H., & Yano, K. H. T. (1995). Lightness dependency
 *   of chroma scales of a nonlinear color-appearance model and its latest
 *   formulation. Color Research & Application, 20(3), 156-167.
 * - Fairchild, M. D. (2013). Color Appearance Models (3rd ed.). Wiley.
 */

/* ----------------------------------------------------------------
 * Nayatani95 Output Type (value-returning, native ranges)
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T L_star_N;   /* normalised achromatic lightness */
    ALWAN_CORE_T C;          /* chroma */
    ALWAN_CORE_T theta;      /* hue angle (radians) */
    ALWAN_CORE_T S;          /* saturation */
    ALWAN_CORE_T B_r;        /* brightness */
    ALWAN_CORE_T L_star_P;   /* achromatic lightness */
} ALWAN_CORE_FNLIT(alwan_nayatani95_v_correlates);

/* beta_1(x) = (6.469 + 6.362 x^0.4495) / (6.469 + x^0.4495) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(nayatani95_beta_1)(ALWAN_CORE_T x) {
    ALWAN_CORE_T xp = ALWAN_CORE_POW(x, ALWAN_CORE_LITERAL(0.4495));
    return (ALWAN_CORE_LITERAL(6.469) + ALWAN_CORE_LITERAL(6.362) * xp)
         / (ALWAN_CORE_LITERAL(6.469) + xp);
}

/* beta_2(x) = 0.7844 (8.414 + 8.091 x^0.5128) / (8.414 + x^0.5128) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(nayatani95_beta_2)(ALWAN_CORE_T x) {
    ALWAN_CORE_T xp = ALWAN_CORE_POW(x, ALWAN_CORE_LITERAL(0.5128));
    return ALWAN_CORE_LITERAL(0.7844) * (ALWAN_CORE_LITERAL(8.414) + ALWAN_CORE_LITERAL(8.091) * xp)
         / (ALWAN_CORE_LITERAL(8.414) + xp);
}

/* ----------------------------------------------------------------
 * Nayatani95 Forward Transform (value-returning)
 *   xyz, white_xyz : CIE XYZ tristimulus, [0, 100] domain
 *   Y_o            : luminance factor of achromatic background
 *   E_o            : illuminance of the viewing field (lux)
 *   E_or           : normalising illuminance (lux)
 *   (noise term n is fixed at 1.0, the model default)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_nayatani95_v_correlates) ALWAN_CORE_FNVLIT(alwan_nayatani95_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_XYZ white_xyz,
    ALWAN_CORE_T Y_o,
    ALWAN_CORE_T E_o,
    ALWAN_CORE_T E_or
) {
    ALWAN_CORE_FNLIT(alwan_nayatani95_v_correlates) r;
    const ALWAN_CORE_T n = ALWAN_CORE_ONE;
    const ALWAN_CORE_T PI = ALWAN_CORE_PI;
    const ALWAN_CORE_T c20 = ALWAN_CORE_LITERAL(20.0);
    const ALWAN_CORE_T twothirds = ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0);
    const ALWAN_CORE_T onethird  = ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0);

    /* Intermediate xi/eta/zeta from the reference-white chromaticity. */
    ALWAN_CORE_T sum = white_xyz.x + white_xyz.y + white_xyz.z;
    ALWAN_CORE_T xo = white_xyz.x / sum;
    ALWAN_CORE_T yo = white_xyz.y / sum;
    ALWAN_CORE_T xi   = (ALWAN_CORE_LITERAL(0.48105) * xo + ALWAN_CORE_LITERAL(0.78841) * yo - ALWAN_CORE_LITERAL(0.08081)) / yo;
    ALWAN_CORE_T eta  = (ALWAN_CORE_LITERAL(-0.27200) * xo + ALWAN_CORE_LITERAL(1.11962) * yo + ALWAN_CORE_LITERAL(0.04570)) / yo;
    ALWAN_CORE_T zeta = (ALWAN_CORE_LITERAL(0.91822) * (ALWAN_CORE_ONE - xo - yo)) / yo;

    /* Adapting field cone responses and their exponential factors. */
    ALWAN_CORE_T L_or = Y_o * E_or / (ALWAN_CORE_LITERAL(100.0) * PI);
    ALWAN_CORE_T k    = Y_o * E_o  / (ALWAN_CORE_LITERAL(100.0) * PI);
    ALWAN_CORE_T bR_o = ALWAN_CORE_FNVLIT(nayatani95_beta_1)(k * xi);
    ALWAN_CORE_T bG_o = ALWAN_CORE_FNVLIT(nayatani95_beta_1)(k * eta);
    ALWAN_CORE_T bB_o = ALWAN_CORE_FNVLIT(nayatani95_beta_2)(k * zeta);
    ALWAN_CORE_T bL_or = ALWAN_CORE_FNVLIT(nayatani95_beta_1)(L_or);

    /* Stimulus cone responses (CIE 1994 XYZ->RGB matrix). */
    ALWAN_CORE_T R =  ALWAN_CORE_LITERAL(0.40024) * xyz.x + ALWAN_CORE_LITERAL(0.70760) * xyz.y - ALWAN_CORE_LITERAL(0.08081) * xyz.z;
    ALWAN_CORE_T G = ALWAN_CORE_LITERAL(-0.22630) * xyz.x + ALWAN_CORE_LITERAL(1.16532) * xyz.y + ALWAN_CORE_LITERAL(0.04570) * xyz.z;
    ALWAN_CORE_T B =                                                                              ALWAN_CORE_LITERAL(0.91822) * xyz.z;

    ALWAN_CORE_T eR = ALWAN_CORE_SELECT(R >= c20 * xi,  ALWAN_CORE_LITERAL(1.758), ALWAN_CORE_ONE);
    ALWAN_CORE_T eG = ALWAN_CORE_SELECT(G >= c20 * eta, ALWAN_CORE_LITERAL(1.758), ALWAN_CORE_ONE);

    ALWAN_CORE_T lR = ALWAN_CORE_LOG10((R + n) / (c20 * xi   + n));
    ALWAN_CORE_T lG = ALWAN_CORE_LOG10((G + n) / (c20 * eta  + n));
    ALWAN_CORE_T lB = ALWAN_CORE_LOG10((B + n) / (c20 * zeta + n));

    /* Achromatic response Q, tritanopic t, protanopic p. */
    ALWAN_CORE_T Q = (twothirds * bR_o * eR * lR + onethird * bG_o * eG * lG)
                   * (ALWAN_CORE_LITERAL(41.69) / bL_or);
    ALWAN_CORE_T t = bR_o * lR
                   - (ALWAN_CORE_LITERAL(12.0) / ALWAN_CORE_LITERAL(11.0)) * bG_o * lG
                   + (ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(11.0)) * bB_o * lB;
    ALWAN_CORE_T p = (ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(9.0)) * bR_o * lR
                   + (ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(9.0)) * bG_o * lG
                   - (ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(9.0)) * bB_o * lB;

    /* Brightness and ideal-white brightness. */
    ALWAN_CORE_T B_r = (ALWAN_CORE_LITERAL(50.0) / bL_or) * (twothirds * bR_o + onethird * bG_o) + Q;
    ALWAN_CORE_T B_rw = (twothirds * bR_o * ALWAN_CORE_LITERAL(1.758) * ALWAN_CORE_LOG10((ALWAN_CORE_LITERAL(100.0) * xi  + n) / (c20 * xi  + n))
                       + onethird  * bG_o * ALWAN_CORE_LITERAL(1.758) * ALWAN_CORE_LOG10((ALWAN_CORE_LITERAL(100.0) * eta + n) / (c20 * eta + n)))
                       * (ALWAN_CORE_LITERAL(41.69) / bL_or)
                       + (ALWAN_CORE_LITERAL(50.0) / bL_or) * twothirds * bR_o
                       + (ALWAN_CORE_LITERAL(50.0) / bL_or) * onethird  * bG_o;

    ALWAN_CORE_T L_star_P = Q + ALWAN_CORE_LITERAL(50.0);
    ALWAN_CORE_T L_star_N = ALWAN_CORE_LITERAL(100.0) * B_r / B_rw;

    /* Hue angle (degrees -> [0,360) -> radians). */
    ALWAN_CORE_T theta_deg = ALWAN_CORE_ATAN2(p, t) * ALWAN_CORE_LITERAL(180.0) / PI;
    theta_deg = ALWAN_CORE_FMOD(theta_deg, ALWAN_CORE_LITERAL(360.0));
    theta_deg = ALWAN_CORE_SELECT(theta_deg < ALWAN_CORE_ZERO, theta_deg + ALWAN_CORE_LITERAL(360.0), theta_deg);
    ALWAN_CORE_T th = theta_deg * PI / ALWAN_CORE_LITERAL(180.0);

    /* Chromatic-strength function E_s(theta). */
    ALWAN_CORE_T E_s = ALWAN_CORE_LITERAL(0.9394)
        - ALWAN_CORE_LITERAL(0.2478) * ALWAN_CORE_SIN(th)
        - ALWAN_CORE_LITERAL(0.0743) * ALWAN_CORE_SIN(ALWAN_CORE_LITERAL(2.0) * th)
        + ALWAN_CORE_LITERAL(0.0666) * ALWAN_CORE_SIN(ALWAN_CORE_LITERAL(3.0) * th)
        - ALWAN_CORE_LITERAL(0.0186) * ALWAN_CORE_SIN(ALWAN_CORE_LITERAL(4.0) * th)
        - ALWAN_CORE_LITERAL(0.0055) * ALWAN_CORE_COS(th)
        - ALWAN_CORE_LITERAL(0.0521) * ALWAN_CORE_COS(ALWAN_CORE_LITERAL(2.0) * th)
        - ALWAN_CORE_LITERAL(0.0573) * ALWAN_CORE_COS(ALWAN_CORE_LITERAL(3.0) * th)
        - ALWAN_CORE_LITERAL(0.0061) * ALWAN_CORE_COS(ALWAN_CORE_LITERAL(4.0) * th);

    /* Saturation and chroma. */
    ALWAN_CORE_T S_RG = (ALWAN_CORE_LITERAL(488.93) / bL_or) * E_s * t;
    ALWAN_CORE_T S_YB = (ALWAN_CORE_LITERAL(488.93) / bL_or) * E_s * p;
    ALWAN_CORE_T S = ALWAN_CORE_SQRT(S_RG * S_RG + S_YB * S_YB);

    /* C = spow(L_star_P / 50, 0.7) * S  (signed power, avoid pow(neg)). */
    ALWAN_CORE_T ratio = L_star_P / ALWAN_CORE_LITERAL(50.0);
    ALWAN_CORE_T pc = ALWAN_CORE_POW(ALWAN_CORE_ABS(ratio), ALWAN_CORE_LITERAL(0.7));
    ALWAN_CORE_T C = ALWAN_CORE_SELECT(ratio < ALWAN_CORE_ZERO, -pc, pc) * S;

    r.L_star_N = L_star_N;
    r.C        = C;
    r.theta    = th;
    r.S        = S;
    r.B_r      = B_r;
    r.L_star_P = L_star_P;
    return r;
}
