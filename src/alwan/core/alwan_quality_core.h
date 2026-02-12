/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Light Quality & CCT formulas
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_QUALITY_CORE_H
#define ALWAN_QUALITY_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

/* ================================================================
 * CCT: McCamy's approximation from xy
 * CCT = 437n^3 + 3601n^2 + 6861n + 5517
 * where n = (x - 0.3320) / (0.1858 - y)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_cct_mccamy_v(alwan_scalar x, alwan_scalar y) {
    alwan_scalar n = (x - ALWAN_LITERAL(0.3320)) / (ALWAN_LITERAL(0.1858) - y);
    alwan_scalar n2 = n * n;
    alwan_scalar n3 = n2 * n;

    return ALWAN_LITERAL(437.0) * n3 +
           ALWAN_LITERAL(3601.0) * n2 +
           ALWAN_LITERAL(6861.0) * n +
           ALWAN_LITERAL(5517.0);
}

/* ================================================================
 * CCT: Hernandez-Andres (1999) from xy
 * Two-region piecewise (<=50000K and >50000K), branchless
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_cct_hernandez_v(alwan_scalar x, alwan_scalar y) {
    /* Low CCT region: epicenter (0.3366, 0.1735) */
    alwan_scalar n_lo = (x - ALWAN_LITERAL(0.3366)) / (y - ALWAN_LITERAL(0.1735));
    alwan_scalar cct_lo = ALWAN_LITERAL(-949.86315)
        + ALWAN_LITERAL(6253.80338) * ALWAN_EXP(-n_lo / ALWAN_LITERAL(0.92159))
        + ALWAN_LITERAL(28.70599)   * ALWAN_EXP(-n_lo / ALWAN_LITERAL(0.20039))
        + ALWAN_LITERAL(0.00004)    * ALWAN_EXP(-n_lo / ALWAN_LITERAL(0.07125));

    /* High CCT region (>50000K): epicenter (0.3356, 0.1691) */
    alwan_scalar n_hi = (x - ALWAN_LITERAL(0.3356)) / (y - ALWAN_LITERAL(0.1691));
    alwan_scalar cct_hi = ALWAN_LITERAL(36284.48953)
        + ALWAN_LITERAL(0.00228)    * ALWAN_EXP(-n_hi / ALWAN_LITERAL(0.07861))
        + ALWAN_LITERAL(5.4535e-36) * ALWAN_EXP(-n_hi / ALWAN_LITERAL(0.01543));

    return ALWAN_SELECT(cct_lo > ALWAN_LITERAL(50000.0), cct_hi, cct_lo);
}

/* ================================================================
 * CCT to xy: Kang (2002) / Hernandez-Andres (1999)
 * Three-region piecewise, branchless
 * Valid range: 1667K - 25000K
 * ================================================================ */

ALWAN_INLINE alwan_vec2 alwan_cct_to_xy_kang_v(alwan_scalar cct) {
    alwan_vec2 result;
    alwan_scalar T = cct;
    alwan_scalar T2 = T * T;
    alwan_scalar T3 = T2 * T;

    /* x: two-region piecewise at T=4000 */
    alwan_scalar x_lo = ALWAN_LITERAL(-0.2661239e9) / T3
                      - ALWAN_LITERAL(0.2343589e6) / T2
                      + ALWAN_LITERAL(0.8776956e3) / T
                      + ALWAN_LITERAL(0.179910);
    alwan_scalar x_hi = ALWAN_LITERAL(-3.0258469e9) / T3
                      + ALWAN_LITERAL(2.1070379e6) / T2
                      + ALWAN_LITERAL(0.2226347e3) / T
                      + ALWAN_LITERAL(0.24039);
    alwan_scalar x = ALWAN_SELECT(T <= ALWAN_LITERAL(4000.0), x_lo, x_hi);

    alwan_scalar x2 = x * x;
    alwan_scalar x3 = x2 * x;

    /* y: three-region piecewise at T=2222 and T=4000 */
    alwan_scalar y_lo  = ALWAN_LITERAL(-1.1063814) * x3
                       - ALWAN_LITERAL(1.34811020) * x2
                       + ALWAN_LITERAL(2.18555832) * x
                       - ALWAN_LITERAL(0.20219683);
    alwan_scalar y_mid = ALWAN_LITERAL(-0.9549476) * x3
                       - ALWAN_LITERAL(1.37418593) * x2
                       + ALWAN_LITERAL(2.09137015) * x
                       - ALWAN_LITERAL(0.16748867);
    alwan_scalar y_hi2 = ALWAN_LITERAL(3.0817580) * x3
                       - ALWAN_LITERAL(5.8733867) * x2
                       + ALWAN_LITERAL(3.75112997) * x
                       - ALWAN_LITERAL(0.37001483);
    alwan_scalar y = ALWAN_SELECT(T <= ALWAN_LITERAL(2222.0), y_lo,
                     ALWAN_SELECT(T <= ALWAN_LITERAL(4000.0), y_mid, y_hi2));

    result.v[0] = x;
    result.v[1] = y;
    return result;
}

/* ================================================================
 * ASTM E313 Whiteness Index
 * WI = 3.388 * Z - 3 * Y
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_whiteness_astm_e313_v(alwan_scalar Y, alwan_scalar Z) {
    return ALWAN_LITERAL(3.388) * Z - ALWAN_LITERAL(3.0) * Y;
}

/* ================================================================
 * CIE 2004 Whiteness Index
 * W = Y + 800(xn - x) + 1700(yn - y)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_whiteness_cie2004_v(
    alwan_scalar x, alwan_scalar y, alwan_scalar Y,
    alwan_scalar xn, alwan_scalar yn) {
    return Y + ALWAN_LITERAL(800.0) * (xn - x) + ALWAN_LITERAL(1700.0) * (yn - y);
}

/* ================================================================
 * ASTM E313 Yellowness Index
 * YI = 100 * (Cx * X - Cz * Z) / Y
 * (Cx, Cz depend on illuminant — resolved in .c)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_yellowness_astm_e313_v(
    alwan_scalar X, alwan_scalar Y, alwan_scalar Z,
    alwan_scalar Cx, alwan_scalar Cz) {
    return ALWAN_LITERAL(100.0) * (Cx * X - Cz * Z) / Y;
}

#endif /* ALWAN_QUALITY_CORE_H */
