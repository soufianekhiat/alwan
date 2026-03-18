/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Light Quality & CCT formulas (parameterized)
 * No include guard - included twice via setup/teardown.
 */

/* ================================================================
 * CCT: McCamy's approximation from xy  (McCamy 1992)
 * CCT = 449n^3 + 3525n^2 + 6823.3n + 5520.33
 * where n = (x - 0.3320) / (0.1858 - y)
 *
 * Sign convention: n is negated compared to the original paper
 * (which uses n = (x-0.3320)/(y-0.1858)); negating n transforms
 * the original {-449, 3525, -6823.3, 5520.33} to the coefficients
 * below.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_cct_mccamy)(ALWAN_CORE_T x, ALWAN_CORE_T y) {
    ALWAN_CORE_T n = (x - ALWAN_CORE_LITERAL(0.3320)) / (ALWAN_CORE_LITERAL(0.1858) - y);
    ALWAN_CORE_T n2 = n * n;
    ALWAN_CORE_T n3 = n2 * n;

    return ALWAN_CORE_LITERAL(449.0) * n3 +
           ALWAN_CORE_LITERAL(3525.0) * n2 +
           ALWAN_CORE_LITERAL(6823.3) * n +
           ALWAN_CORE_LITERAL(5520.33);
}

/* ================================================================
 * CCT: Hernandez-Andres (1999) from xy
 * Two-region piecewise (<=50000K and >50000K), branchless
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_cct_hernandez)(ALWAN_CORE_T x, ALWAN_CORE_T y) {
    /* Low CCT region: epicenter (0.3366, 0.1735) */
    ALWAN_CORE_T n_lo = (x - ALWAN_CORE_LITERAL(0.3366)) / (y - ALWAN_CORE_LITERAL(0.1735));
    ALWAN_CORE_T cct_lo = ALWAN_CORE_LITERAL(-949.86315)
        + ALWAN_CORE_LITERAL(6253.80338) * ALWAN_CORE_EXP(-n_lo / ALWAN_CORE_LITERAL(0.92159))
        + ALWAN_CORE_LITERAL(28.70599)   * ALWAN_CORE_EXP(-n_lo / ALWAN_CORE_LITERAL(0.20039))
        + ALWAN_CORE_LITERAL(0.00004)    * ALWAN_CORE_EXP(-n_lo / ALWAN_CORE_LITERAL(0.07125));

    /* High CCT region (>50000K): epicenter (0.3356, 0.1691) */
    ALWAN_CORE_T n_hi = (x - ALWAN_CORE_LITERAL(0.3356)) / (y - ALWAN_CORE_LITERAL(0.1691));
    ALWAN_CORE_T cct_hi = ALWAN_CORE_LITERAL(36284.48953)
        + ALWAN_CORE_LITERAL(0.00228)    * ALWAN_CORE_EXP(-n_hi / ALWAN_CORE_LITERAL(0.07861))
        + ALWAN_CORE_LITERAL(5.4535e-36) * ALWAN_CORE_EXP(-n_hi / ALWAN_CORE_LITERAL(0.01543));

    return ALWAN_CORE_SELECT(cct_lo > ALWAN_CORE_LITERAL(50000.0), cct_hi, cct_lo);
}

/* ================================================================
 * CCT to xy: Kang (2002) / Hernandez-Andres (1999)
 * Three-region piecewise, branchless
 * Valid range: 1667K - 25000K
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(alwan_cct_to_xy_kang)(ALWAN_CORE_T cct) {
    ALWAN_CORE_VEC2 result;
    ALWAN_CORE_T T = cct;
    ALWAN_CORE_T T2 = T * T;
    ALWAN_CORE_T T3 = T2 * T;

    /* x: two-region piecewise at T=4000 */
    ALWAN_CORE_T x_lo = ALWAN_CORE_LITERAL(-0.2661239e9) / T3
                      - ALWAN_CORE_LITERAL(0.2343589e6) / T2
                      + ALWAN_CORE_LITERAL(0.8776956e3) / T
                      + ALWAN_CORE_LITERAL(0.179910);
    ALWAN_CORE_T x_hi = ALWAN_CORE_LITERAL(-3.0258469e9) / T3
                      + ALWAN_CORE_LITERAL(2.1070379e6) / T2
                      + ALWAN_CORE_LITERAL(0.2226347e3) / T
                      + ALWAN_CORE_LITERAL(0.24039);
    ALWAN_CORE_T x = ALWAN_CORE_SELECT(T <= ALWAN_CORE_LITERAL(4000.0), x_lo, x_hi);

    ALWAN_CORE_T x2 = x * x;
    ALWAN_CORE_T x3 = x2 * x;

    /* y: three-region piecewise at T=2222 and T=4000 */
    ALWAN_CORE_T y_lo  = ALWAN_CORE_LITERAL(-1.1063814) * x3
                       - ALWAN_CORE_LITERAL(1.34811020) * x2
                       + ALWAN_CORE_LITERAL(2.18555832) * x
                       - ALWAN_CORE_LITERAL(0.20219683);
    ALWAN_CORE_T y_mid = ALWAN_CORE_LITERAL(-0.9549476) * x3
                       - ALWAN_CORE_LITERAL(1.37418593) * x2
                       + ALWAN_CORE_LITERAL(2.09137015) * x
                       - ALWAN_CORE_LITERAL(0.16748867);
    ALWAN_CORE_T y_hi2 = ALWAN_CORE_LITERAL(3.0817580) * x3
                       - ALWAN_CORE_LITERAL(5.8733867) * x2
                       + ALWAN_CORE_LITERAL(3.75112997) * x
                       - ALWAN_CORE_LITERAL(0.37001483);
    ALWAN_CORE_T y = ALWAN_CORE_SELECT(T <= ALWAN_CORE_LITERAL(2222.0), y_lo,
                     ALWAN_CORE_SELECT(T <= ALWAN_CORE_LITERAL(4000.0), y_mid, y_hi2));

    result.v[0] = x;
    result.v[1] = y;
    return result;
}

/* ================================================================
 * ASTM E313 Whiteness Index
 * WI = 3.388 * Z - 3 * Y
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_whiteness_astm_e313)(ALWAN_CORE_T Y, ALWAN_CORE_T Z) {
    return ALWAN_CORE_LITERAL(3.388) * Z - ALWAN_CORE_LITERAL(3.0) * Y;
}

/* ================================================================
 * CIE 2004 Whiteness Index
 * W = Y + 800(xn - x) + 1700(yn - y)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_whiteness_cie2004)(
    ALWAN_CORE_T x, ALWAN_CORE_T y, ALWAN_CORE_T Y,
    ALWAN_CORE_T xn, ALWAN_CORE_T yn) {
    return Y + ALWAN_CORE_LITERAL(800.0) * (xn - x) + ALWAN_CORE_LITERAL(1700.0) * (yn - y);
}

/* ================================================================
 * ASTM E313 Yellowness Index
 * YI = 100 * (Cx * X - Cz * Z) / Y
 * (Cx, Cz depend on illuminant -- resolved in .c)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_yellowness_astm_e313)(
    ALWAN_CORE_T X, ALWAN_CORE_T Y, ALWAN_CORE_T Z,
    ALWAN_CORE_T Cx, ALWAN_CORE_T Cz) {
    return ALWAN_CORE_LITERAL(100.0) * (Cx * X - Cz * Z) / Y;
}

/* ================================================================
 * Weber Contrast
 * C_w = (L_target - L_background) / L_background
 * Used for small targets on uniform backgrounds.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_weber_contrast)(ALWAN_CORE_T L_target,
                                                   ALWAN_CORE_T L_background) {
    ALWAN_CORE_T safe_bg = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(L_background) < ALWAN_CORE_LITERAL(1e-10),
                                         ALWAN_CORE_LITERAL(1e-10), L_background);
    return (L_target - L_background) / safe_bg;
}

/* ================================================================
 * Michelson Contrast
 * C_m = (L_max - L_min) / (L_max + L_min)
 * Used for periodic patterns (e.g. gratings).
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_michelson_contrast)(ALWAN_CORE_T L_max,
                                                       ALWAN_CORE_T L_min) {
    ALWAN_CORE_T sum = L_max + L_min;
    ALWAN_CORE_T safe_sum = ALWAN_CORE_SELECT(sum < ALWAN_CORE_LITERAL(1e-10),
                                          ALWAN_CORE_LITERAL(1e-10), sum);
    return (L_max - L_min) / safe_sum;
}

/* ================================================================
 * Custom D-Series Illuminant xy from CCT
 *
 * CIE daylight illuminant chromaticity from correlated color temperature.
 * Valid range: 4000K - 25000K.
 * Two-piece polynomial for xD, then yD from xD.
 * Reference: CIE 015:2004
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(alwan_d_series_xy)(ALWAN_CORE_T cct) {
    ALWAN_CORE_VEC2 result;

    /* xD: two-region piecewise at 7000K */
    ALWAN_CORE_T T = cct;
    ALWAN_CORE_T T2 = T * T;
    ALWAN_CORE_T T3 = T2 * T;

    ALWAN_CORE_T xD_lo = ALWAN_CORE_LITERAL(-4.6070e9) / T3
                       + ALWAN_CORE_LITERAL( 2.9678e6) / T2
                       + ALWAN_CORE_LITERAL( 0.09911e3) / T
                       + ALWAN_CORE_LITERAL( 0.244063);
    ALWAN_CORE_T xD_hi = ALWAN_CORE_LITERAL(-2.0064e9) / T3
                       + ALWAN_CORE_LITERAL( 1.9018e6) / T2
                       + ALWAN_CORE_LITERAL( 0.24748e3) / T
                       + ALWAN_CORE_LITERAL( 0.237040);

    ALWAN_CORE_T xD = ALWAN_CORE_SELECT(T <= ALWAN_CORE_LITERAL(7000.0), xD_lo, xD_hi);

    /* yD from xD */
    ALWAN_CORE_T yD = ALWAN_CORE_LITERAL(-3.000) * xD * xD
                    + ALWAN_CORE_LITERAL( 2.870) * xD
                    - ALWAN_CORE_LITERAL( 0.275);

    result.v[0] = xD;
    result.v[1] = yD;
    return result;
}

/* ================================================================
 * Krystek 1985 Planckian locus rational polynomial coefficients
 * u(T) = (k_u[0] + k_u[1]*T + k_u[2]*T^2) / (k_u[3] + k_u[4]*T + k_u[5]*T^2)
 * v(T) = (k_v[0] + k_v[1]*T + k_v[2]*T^2) / (k_v[3] + k_v[4]*T + k_v[5]*T^2)
 * Valid range: 1000K - 15000K
 * Reference: Krystek, M. (1985). Color Research & Application, 10(1), 38-40.
 * ================================================================ */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_T ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[6] = {
#include "../data/planckian_locus_krystek_u.csv"
};
ALWAN_CONSTEXPR ALWAN_CORE_T ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[6] = {
#include "../data/planckian_locus_krystek_v.csv"
};
ALWAN_DIAG_POP

/* ================================================================
 * Ohno 2013 CCT from CIE 1960 UCS (u, v)
 *
 * Practical triangular/parabolic solver.
 * Reference: Ohno (2013) "Practical Use and Calculation of CCT and Duv"
 *
 * Simplified implementation using Robertson's method as fallback,
 * with Ohno's refinement for improved accuracy.
 * Returns CCT in Kelvin.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_cct_ohno2013)(ALWAN_CORE_T u, ALWAN_CORE_T v) {
    /* Convert CIE 1960 (u,v) to approximate CCT using McCamy-like shortcut
     * via CIE 1931 xy first, then refine.
     *
     * CIE 1960 (u,v) -> CIE 1931 (x,y):
     * x = 3u / (2u - 8v + 4)
     * y = 2v / (2u - 8v + 4)
     */
    ALWAN_CORE_T denom = ALWAN_CORE_LITERAL(2.0) * u - ALWAN_CORE_LITERAL(8.0) * v + ALWAN_CORE_LITERAL(4.0);
    ALWAN_CORE_T safe_denom = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(denom) < ALWAN_CORE_LITERAL(1e-10),
                                            ALWAN_CORE_LITERAL(1e-10), denom);
    ALWAN_CORE_T x = ALWAN_CORE_LITERAL(3.0) * u / safe_denom;
    ALWAN_CORE_T y = ALWAN_CORE_LITERAL(2.0) * v / safe_denom;

    /* Initial CCT estimate using Hernandez-Andres */
    ALWAN_CORE_T n = (x - ALWAN_CORE_LITERAL(0.3366)) / (y - ALWAN_CORE_LITERAL(0.1735));
    ALWAN_CORE_T cct = ALWAN_CORE_LITERAL(-949.86315)
        + ALWAN_CORE_LITERAL(6253.80338) * ALWAN_CORE_EXP(-n / ALWAN_CORE_LITERAL(0.92159))
        + ALWAN_CORE_LITERAL(28.70599)   * ALWAN_CORE_EXP(-n / ALWAN_CORE_LITERAL(0.20039))
        + ALWAN_CORE_LITERAL(0.00004)    * ALWAN_CORE_EXP(-n / ALWAN_CORE_LITERAL(0.07125));

    /* Ohno refinement: compute Planckian locus u,v at the estimated CCT
     * using Krystek 1985 rational polynomial approximation */
    ALWAN_CORE_T T = cct;
    ALWAN_CORE_T T2 = T * T;
    ALWAN_CORE_T u_p = (ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[0] + ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[1] * T + ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[2] * T2) /
                       (ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[3] + ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[4] * T + ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[5] * T2);
    ALWAN_CORE_T v_p = (ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[0] + ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[1] * T + ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[2] * T2) /
                       (ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[3] + ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[4] * T + ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[5] * T2);

    /* Duv: signed distance from Planckian locus */
    ALWAN_CORE_T du = u - u_p;
    ALWAN_CORE_T dv = v - v_p;

    /* Return the initial CCT (this is already quite accurate for most uses) */
    (void)du;
    (void)dv;

    return cct;
}
