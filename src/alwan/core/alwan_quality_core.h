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

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_quality_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_quality_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_cct_mccamy_v(alwan_scalar x, alwan_scalar y) {
    alwan_scalar n = (x - ALWAN_LITERAL(0.3320)) / (ALWAN_LITERAL(0.1858) - y);
    alwan_scalar n2 = n * n;
    alwan_scalar n3 = n2 * n;
    return ALWAN_LITERAL(449.0) * n3 +
           ALWAN_LITERAL(3525.0) * n2 +
           ALWAN_LITERAL(6823.3) * n +
           ALWAN_LITERAL(5520.33);
}

ALWAN_INLINE alwan_scalar alwan_cct_hernandez_v(alwan_scalar x, alwan_scalar y) {
    alwan_scalar n_lo = (x - ALWAN_LITERAL(0.3366)) / (y - ALWAN_LITERAL(0.1735));
    alwan_scalar cct_lo = ALWAN_LITERAL(-949.86315)
        + ALWAN_LITERAL(6253.80338) * ALWAN_EXP(-n_lo / ALWAN_LITERAL(0.92159))
        + ALWAN_LITERAL(28.70599)   * ALWAN_EXP(-n_lo / ALWAN_LITERAL(0.20039))
        + ALWAN_LITERAL(0.00004)    * ALWAN_EXP(-n_lo / ALWAN_LITERAL(0.07125));
    alwan_scalar n_hi = (x - ALWAN_LITERAL(0.3356)) / (y - ALWAN_LITERAL(0.1691));
    alwan_scalar cct_hi = ALWAN_LITERAL(36284.48953)
        + ALWAN_LITERAL(0.00228)    * ALWAN_EXP(-n_hi / ALWAN_LITERAL(0.07861))
        + ALWAN_LITERAL(5.4535e-36) * ALWAN_EXP(-n_hi / ALWAN_LITERAL(0.01543));
    return ALWAN_SELECT(cct_lo > ALWAN_LITERAL(50000.0), cct_hi, cct_lo);
}

ALWAN_INLINE alwan_vec2 alwan_cct_to_xy_kang_v(alwan_scalar cct) {
    alwan_vec2 result;
    alwan_scalar T = cct;
    alwan_scalar T2 = T * T;
    alwan_scalar T3 = T2 * T;
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
    alwan_scalar y_lo  = ALWAN_LITERAL(-1.1063814) * x3 - ALWAN_LITERAL(1.34811020) * x2
                       + ALWAN_LITERAL(2.18555832) * x - ALWAN_LITERAL(0.20219683);
    alwan_scalar y_mid = ALWAN_LITERAL(-0.9549476) * x3 - ALWAN_LITERAL(1.37418593) * x2
                       + ALWAN_LITERAL(2.09137015) * x - ALWAN_LITERAL(0.16748867);
    alwan_scalar y_hi2 = ALWAN_LITERAL(3.0817580) * x3 - ALWAN_LITERAL(5.8733867) * x2
                       + ALWAN_LITERAL(3.75112997) * x - ALWAN_LITERAL(0.37001483);
    alwan_scalar y = ALWAN_SELECT(T <= ALWAN_LITERAL(2222.0), y_lo,
                     ALWAN_SELECT(T <= ALWAN_LITERAL(4000.0), y_mid, y_hi2));
    result.v[0] = x;
    result.v[1] = y;
    return result;
}

ALWAN_INLINE alwan_scalar alwan_whiteness_astm_e313_v(alwan_scalar Y, alwan_scalar Z) {
    return ALWAN_LITERAL(3.388) * Z - ALWAN_LITERAL(3.0) * Y;
}

ALWAN_INLINE alwan_scalar alwan_whiteness_cie2004_v(
    alwan_scalar x, alwan_scalar y, alwan_scalar Y,
    alwan_scalar xn, alwan_scalar yn) {
    return Y + ALWAN_LITERAL(800.0) * (xn - x) + ALWAN_LITERAL(1700.0) * (yn - y);
}

ALWAN_INLINE alwan_scalar alwan_yellowness_astm_e313_v(
    alwan_scalar X, alwan_scalar Y, alwan_scalar Z,
    alwan_scalar Cx, alwan_scalar Cz) {
    return ALWAN_LITERAL(100.0) * (Cx * X - Cz * Z) / Y;
}

ALWAN_INLINE alwan_scalar alwan_weber_contrast_v(alwan_scalar L_target,
                                                   alwan_scalar L_background) {
    alwan_scalar safe_bg = ALWAN_SELECT(ALWAN_ABS(L_background) < ALWAN_LITERAL(1e-10),
                                         ALWAN_LITERAL(1e-10), L_background);
    return (L_target - L_background) / safe_bg;
}

ALWAN_INLINE alwan_scalar alwan_michelson_contrast_v(alwan_scalar L_max,
                                                       alwan_scalar L_min) {
    alwan_scalar sum = L_max + L_min;
    alwan_scalar safe_sum = ALWAN_SELECT(sum < ALWAN_LITERAL(1e-10),
                                          ALWAN_LITERAL(1e-10), sum);
    return (L_max - L_min) / safe_sum;
}

ALWAN_INLINE alwan_vec2 alwan_d_series_xy_v(alwan_scalar cct) {
    alwan_vec2 result;
    alwan_scalar T = cct;
    alwan_scalar T2 = T * T;
    alwan_scalar T3 = T2 * T;
    alwan_scalar xD_lo = ALWAN_LITERAL(-4.6070e9) / T3
                       + ALWAN_LITERAL( 2.9678e6) / T2
                       + ALWAN_LITERAL( 0.09911e3) / T
                       + ALWAN_LITERAL( 0.244063);
    alwan_scalar xD_hi = ALWAN_LITERAL(-2.0064e9) / T3
                       + ALWAN_LITERAL( 1.9018e6) / T2
                       + ALWAN_LITERAL( 0.24748e3) / T
                       + ALWAN_LITERAL( 0.237040);
    alwan_scalar xD = ALWAN_SELECT(T <= ALWAN_LITERAL(7000.0), xD_lo, xD_hi);
    alwan_scalar yD = ALWAN_LITERAL(-3.000) * xD * xD
                    + ALWAN_LITERAL( 2.870) * xD
                    - ALWAN_LITERAL( 0.275);
    result.v[0] = xD;
    result.v[1] = yD;
    return result;
}

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_scalar KRYSTEK_U_COEFFS[6] = {
#include "../data/planckian_locus_krystek_u.csv"
};
ALWAN_CONSTEXPR alwan_scalar KRYSTEK_V_COEFFS[6] = {
#include "../data/planckian_locus_krystek_v.csv"
};
ALWAN_DIAG_POP

ALWAN_INLINE alwan_scalar alwan_cct_ohno2013_v(alwan_scalar u, alwan_scalar v) {
    alwan_scalar denom = ALWAN_LITERAL(2.0) * u - ALWAN_LITERAL(8.0) * v + ALWAN_LITERAL(4.0);
    alwan_scalar safe_denom = ALWAN_SELECT(ALWAN_ABS(denom) < ALWAN_LITERAL(1e-10),
                                            ALWAN_LITERAL(1e-10), denom);
    alwan_scalar x = ALWAN_LITERAL(3.0) * u / safe_denom;
    alwan_scalar y = ALWAN_LITERAL(2.0) * v / safe_denom;
    alwan_scalar n = (x - ALWAN_LITERAL(0.3366)) / (y - ALWAN_LITERAL(0.1735));
    alwan_scalar cct = ALWAN_LITERAL(-949.86315)
        + ALWAN_LITERAL(6253.80338) * ALWAN_EXP(-n / ALWAN_LITERAL(0.92159))
        + ALWAN_LITERAL(28.70599)   * ALWAN_EXP(-n / ALWAN_LITERAL(0.20039))
        + ALWAN_LITERAL(0.00004)    * ALWAN_EXP(-n / ALWAN_LITERAL(0.07125));
    alwan_scalar T = cct;
    alwan_scalar T2 = T * T;
    alwan_scalar u_p = (KRYSTEK_U_COEFFS[0] + KRYSTEK_U_COEFFS[1] * T + KRYSTEK_U_COEFFS[2] * T2) /
                       (KRYSTEK_U_COEFFS[3] + KRYSTEK_U_COEFFS[4] * T + KRYSTEK_U_COEFFS[5] * T2);
    alwan_scalar v_p = (KRYSTEK_V_COEFFS[0] + KRYSTEK_V_COEFFS[1] * T + KRYSTEK_V_COEFFS[2] * T2) /
                       (KRYSTEK_V_COEFFS[3] + KRYSTEK_V_COEFFS[4] * T + KRYSTEK_V_COEFFS[5] * T2);
    alwan_scalar du = u - u_p;
    alwan_scalar dv = v - v_p;
    (void)du;
    (void)dv;
    return cct;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_QUALITY_CORE_H */
