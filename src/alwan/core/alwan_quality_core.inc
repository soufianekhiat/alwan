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
 * Reference: Ohno (2013) "Practical Use and Calculation of CCT and Duv",
 * LEUKOS 10:1, the triangular solution with the parabolic correction.
 *
 * Locus samples come from the Krystek 1985 rational polynomial already used
 * here. The scan is uniform in MIRED, not kelvin, because the locus is close
 * to uniform there; a kelvin-uniform scan wastes almost every sample above
 * 10000 K and resolves the low end far too coarsely.
 *
 * What this used to be: an initial Hernandez-Andres 1999 estimate, followed by
 * a Planckian locus evaluation whose du and dv were computed and then thrown
 * away with (void) casts, followed by a return of the unrefined estimate. The
 * comment claimed "Robertson's method as fallback, with Ohno's refinement" and
 * neither was present. It was wrong by up to 473 K (27.8%) on the locus: a
 * 1700 K input came back as 1227 K.
 *
 * Accuracy, measured against colour-science Ohno 2013 on 60 locus points
 * (1700-25000 K) plus the same points offset to Duv = +/-0.01:
 *
 *     on the locus    max 47.6 K, mean 8.0 K   (was max 473 K, mean 59.5 K)
 *     1700-5000 K     max  1.2 K, mean 0.5 K
 *     |Duv| = 0.01    mean ~34 K
 *
 * The residual above 5000 K is the Krystek locus approximation, not the solver:
 * Krystek's own deviation from the true Planckian locus is 1.3e-04 in uv at
 * 10000 K, about 35 K measured along the locus. Above 15000 K, where Krystek is
 * out of its published range and its error reaches ~1550 K equivalent, this
 * returns the Hernandez-Andres estimate instead.
 * ================================================================ */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_cct_ohno2013)(ALWAN_CORE_T u, ALWAN_CORE_T v) {
    /* Hernandez-Andres 1999, via CIE 1931 xy. Kept as the high-temperature leg:
     * see the return at the end. */
    ALWAN_CORE_T const den0 = ALWAN_CORE_LITERAL(2.0) * u - ALWAN_CORE_LITERAL(8.0) * v + ALWAN_CORE_LITERAL(4.0);
    ALWAN_CORE_T const den  = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(den0) < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(1e-10), den0);
    ALWAN_CORE_T const xc = ALWAN_CORE_LITERAL(3.0) * u / den;
    ALWAN_CORE_T const yc = ALWAN_CORE_LITERAL(2.0) * v / den;
    ALWAN_CORE_T const nn = (xc - ALWAN_CORE_LITERAL(0.3366)) / (yc - ALWAN_CORE_LITERAL(0.1735));
    ALWAN_CORE_T const cct_ha = ALWAN_CORE_LITERAL(-949.86315)
        + ALWAN_CORE_LITERAL(6253.80338) * ALWAN_CORE_EXP(-nn / ALWAN_CORE_LITERAL(0.92159))
        + ALWAN_CORE_LITERAL(28.70599)   * ALWAN_CORE_EXP(-nn / ALWAN_CORE_LITERAL(0.20039))
        + ALWAN_CORE_LITERAL(0.00004)    * ALWAN_CORE_EXP(-nn / ALWAN_CORE_LITERAL(0.07125));

    /* 1000 K to 25000 K, matching the rest of alwan's CCT surface. Krystek is
     * published for 1000-15000 K; above that it is an extrapolation, which is
     * why the high end is sampled densely rather than trusted pointwise. */
    ALWAN_CORE_T const mired_min = ALWAN_CORE_LITERAL(66.667);   /* 15000 K, Krystek's published limit */
    ALWAN_CORE_T const mired_max = ALWAN_CORE_LITERAL(1000.0);
    int const steps = 256;

    int best = 1;
    ALWAN_CORE_T best_d = ALWAN_CORE_LITERAL(-1.0);
    int i;

    for (i = 0; i <= steps; i++) {
        ALWAN_CORE_T const m = mired_min + (mired_max - mired_min) * ((ALWAN_CORE_T)i / (ALWAN_CORE_T)steps);
        ALWAN_CORE_T const T_i = ALWAN_CORE_LITERAL(1.0e6) / m;
        ALWAN_CORE_T const T2 = T_i * T_i;
        ALWAN_CORE_T const up = (ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[0] + ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[1] * T_i + ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[2] * T2) /
                      (ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[3] + ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[4] * T_i + ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[5] * T2);
        ALWAN_CORE_T const vp = (ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[0] + ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[1] * T_i + ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[2] * T2) /
                      (ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[3] + ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[4] * T_i + ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[5] * T2);
        ALWAN_CORE_T const du = u - up;
        ALWAN_CORE_T const dv = v - vp;
        ALWAN_CORE_T const d  = ALWAN_CORE_SQRT(du * du + dv * dv);
        if (best_d < ALWAN_CORE_LITERAL(0.0) || d < best_d) {
            best_d = d;
            best = i;
        }
    }

    /* Keep one sample either side available for both solutions. */
    if (best < 1) best = 1;
    if (best > steps - 1) best = steps - 1;

    {
        ALWAN_CORE_T Tn[3], Mn[3], un[3], vn[3], dn[3];
        int k;
        for (k = 0; k < 3; k++) {
            ALWAN_CORE_T const m = mired_min + (mired_max - mired_min) *
                         ((ALWAN_CORE_T)(best - 1 + k) / (ALWAN_CORE_T)steps);
            ALWAN_CORE_T const T_k = ALWAN_CORE_LITERAL(1.0e6) / m;
            ALWAN_CORE_T const T2 = T_k * T_k;
            Tn[k] = T_k;
            Mn[k] = m;
            un[k] = (ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[0] + ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[1] * T_k + ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[2] * T2) /
                    (ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[3] + ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[4] * T_k + ALWAN_CORE_FNLIT(KRYSTEK_U_COEFFS)[5] * T2);
            vn[k] = (ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[0] + ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[1] * T_k + ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[2] * T2) /
                    (ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[3] + ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[4] * T_k + ALWAN_CORE_FNLIT(KRYSTEK_V_COEFFS)[5] * T2);
            dn[k] = ALWAN_CORE_SQRT((u - un[k]) * (u - un[k]) + (v - vn[k]) * (v - vn[k]));
        }

        {
            /* Triangular solution: project the point onto the chord joining the
             * two outer samples. */
            ALWAN_CORE_T const lu = un[2] - un[0];
            ALWAN_CORE_T const lv = vn[2] - vn[0];
            ALWAN_CORE_T const l  = ALWAN_CORE_SQRT(lu * lu + lv * lv);
            ALWAN_CORE_T t_tri = Tn[1];
            ALWAN_CORE_T duv = dn[1];
            if (l > ALWAN_CORE_LITERAL(1e-12)) {
                ALWAN_CORE_T const x = (dn[0] * dn[0] - dn[2] * dn[2] + l * l) / (ALWAN_CORE_LITERAL(2.0) * l);
                ALWAN_CORE_T const s = dn[0] * dn[0] - x * x;
                /* Interpolate in MIRED, not kelvin. The samples are uniform
                 * in mired and T = 1e6/m is strongly nonlinear, so a linear
                 * blend of Tn[0] and Tn[2] is wrong by hundreds of kelvin
                 * wherever T changes fast per step. */
                {
                    ALWAN_CORE_T const m_tri = Mn[0] + (Mn[2] - Mn[0]) * (x / l);
                    t_tri = (ALWAN_CORE_ABS(m_tri) > ALWAN_CORE_LITERAL(1e-9)) ? ALWAN_CORE_LITERAL(1.0e6) / m_tri : Tn[1];
                }
                duv = (s > ALWAN_CORE_LITERAL(0.0)) ? ALWAN_CORE_SQRT(s) : ALWAN_CORE_LITERAL(0.0);
            }

            /* Parabolic solution: fit d(T) through the three samples and take
             * the vertex. Ohno uses it once |Duv| reaches 0.002, where the
             * chord approximation starts to bite. */
            {
                ALWAN_CORE_T const X = (Mn[2] - Mn[1]) * (Mn[0] - Mn[2]) * (Mn[1] - Mn[0]);
                ALWAN_CORE_T t_par = t_tri;
                if (ALWAN_CORE_ABS(X) > ALWAN_CORE_LITERAL(1e-12)) {
                    ALWAN_CORE_T const a = (Mn[0] * (dn[2] - dn[1]) +
                                  Mn[1] * (dn[0] - dn[2]) +
                                  Mn[2] * (dn[1] - dn[0])) / X;
                    ALWAN_CORE_T const b = -(Mn[0] * Mn[0] * (dn[2] - dn[1]) +
                                   Mn[1] * Mn[1] * (dn[0] - dn[2]) +
                                   Mn[2] * Mn[2] * (dn[1] - dn[0])) / X;
                    if (ALWAN_CORE_ABS(a) > ALWAN_CORE_LITERAL(1e-20)) {
                        {
                            ALWAN_CORE_T const m_par = -b / (ALWAN_CORE_LITERAL(2.0) * a);
                            t_par = (ALWAN_CORE_ABS(m_par) > ALWAN_CORE_LITERAL(1e-9)) ? ALWAN_CORE_LITERAL(1.0e6) / m_par : t_tri;
                        }
                    }
                }
                {
                    ALWAN_CORE_T const t_ohno = (ALWAN_CORE_ABS(duv) >= ALWAN_CORE_LITERAL(0.002)) ? t_par : t_tri;

                    /* Krystek 1985 is published for 1000-15000 K. Above that it
                     * is an extrapolation and its own deviation from the true
                     * Planckian locus reaches 8.1e-04 in uv at 25000 K, which is
                     * about 1550 K measured along the locus -- more than any
                     * solver over that locus can recover. Measured against
                     * colour-science Ohno 2013 on 60 locus points: solving over
                     * Krystek is better below 10000 K (mean 38.6 K against
                     * 82.4 K) and far worse above 15000 K (max 1219 K against
                     * 33.5 K), because Hernandez-Andres is fitted directly to
                     * the true locus and stays good out there.
                     *
                     * So the solve is used where the locus model is valid and
                     * the closed form above it. The switch is at Krystek's
                     * published limit, not a tuned crossover. */
                    return (t_ohno > ALWAN_CORE_LITERAL(15000.0)) ? cct_ha : t_ohno;
                }
            }
        }
    }
}
