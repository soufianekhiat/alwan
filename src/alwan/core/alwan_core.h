/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only branchless transfer functions
 * Uses ALWAN_SELECT for cross-platform (C/HLSL/Halide) compatibility.
 */

#ifndef ALWAN_CORE_H
#define ALWAN_CORE_H

#include "../alwan_platform.h"

/* ================================================================
 * sRGB Transfer Functions
 * ================================================================ */

/* sRGB OETF: linear -> encoded
 * V' = 12.92 * V              if V <= 0.0031308
 *      1.055 * V^(1/2.4) - 0.055  otherwise */
ALWAN_INLINE alwan_scalar alwan_srgb_oetf(alwan_scalar linear) {
    alwan_scalar linear_result = ALWAN_LITERAL(12.92) * linear;
    alwan_scalar gamma_result  = ALWAN_LITERAL(1.055)
        * ALWAN_POW(linear, ALWAN_ONE / ALWAN_LITERAL(2.4)) - ALWAN_LITERAL(0.055);
    return ALWAN_SELECT(linear <= ALWAN_LITERAL(0.0031308),
                        linear_result, gamma_result);
}

/* sRGB EOTF: encoded -> linear
 * V = V' / 12.92              if V' <= 0.04045
 *     ((V' + 0.055) / 1.055)^2.4  otherwise */
ALWAN_INLINE alwan_scalar alwan_srgb_eotf(alwan_scalar encoded) {
    alwan_scalar linear_result = encoded / ALWAN_LITERAL(12.92);
    alwan_scalar gamma_result  = ALWAN_POW(
        (encoded + ALWAN_LITERAL(0.055)) / ALWAN_LITERAL(1.055),
        ALWAN_LITERAL(2.4));
    return ALWAN_SELECT(encoded <= ALWAN_LITERAL(0.04045),
                        linear_result, gamma_result);
}

/* ================================================================
 * BT.2020 Transfer Functions
 * ================================================================ */

/* BT.2020 OETF: linear -> encoded (10-bit system) */
ALWAN_INLINE alwan_scalar alwan_bt2020_oetf(alwan_scalar linear) {
    alwan_scalar linear_result = ALWAN_LITERAL(4.5) * linear;
    alwan_scalar gamma_result  = ALWAN_LITERAL(1.099)
        * ALWAN_POW(linear, ALWAN_LITERAL(0.45)) - ALWAN_LITERAL(0.099);
    return ALWAN_SELECT(linear < ALWAN_LITERAL(0.018),
                        linear_result, gamma_result);
}

/* BT.2020 EOTF: encoded -> linear */
ALWAN_INLINE alwan_scalar alwan_bt2020_eotf(alwan_scalar encoded) {
    alwan_scalar threshold = ALWAN_LITERAL(4.5) * ALWAN_LITERAL(0.018);
    alwan_scalar linear_result = encoded / ALWAN_LITERAL(4.5);
    alwan_scalar gamma_result  = ALWAN_POW(
        (encoded + ALWAN_LITERAL(0.099)) / ALWAN_LITERAL(1.099),
        ALWAN_ONE / ALWAN_LITERAL(0.45));
    return ALWAN_SELECT(encoded < threshold,
                        linear_result, gamma_result);
}

/* ================================================================
 * PQ (ST.2084) Transfer Functions
 * ================================================================ */

/* PQ OETF: linear (0-10000 cd/m2) -> PQ code values */
ALWAN_INLINE alwan_scalar alwan_pq_oetf(alwan_scalar linear) {
    alwan_scalar m1 = ALWAN_LITERAL(2610.0) / ALWAN_LITERAL(16384.0);
    alwan_scalar m2 = ALWAN_LITERAL(2523.0) / ALWAN_LITERAL(32.0);
    alwan_scalar c1 = ALWAN_LITERAL(3424.0) / ALWAN_LITERAL(4096.0);
    alwan_scalar c2 = ALWAN_LITERAL(2413.0) / ALWAN_LITERAL(128.0);
    alwan_scalar c3 = ALWAN_LITERAL(2392.0) / ALWAN_LITERAL(128.0);

    alwan_scalar Y = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO,
                                  linear / ALWAN_LITERAL(10000.0));
    alwan_scalar Y_pow_m1 = ALWAN_POW(Y, m1);
    alwan_scalar numerator = c1 + c2 * Y_pow_m1;
    alwan_scalar denominator = ALWAN_ONE + c3 * Y_pow_m1;
    return ALWAN_POW(numerator / denominator, m2);
}

/* PQ EOTF: PQ code values -> linear (0-10000 cd/m2) */
ALWAN_INLINE alwan_scalar alwan_pq_eotf(alwan_scalar encoded) {
    alwan_scalar m1 = ALWAN_LITERAL(2610.0) / ALWAN_LITERAL(16384.0);
    alwan_scalar m2 = ALWAN_LITERAL(2523.0) / ALWAN_LITERAL(32.0);
    alwan_scalar c1 = ALWAN_LITERAL(3424.0) / ALWAN_LITERAL(4096.0);
    alwan_scalar c2 = ALWAN_LITERAL(2413.0) / ALWAN_LITERAL(128.0);
    alwan_scalar c3 = ALWAN_LITERAL(2392.0) / ALWAN_LITERAL(128.0);
    alwan_scalar m1_inv = ALWAN_ONE / m1;
    alwan_scalar m2_inv = ALWAN_ONE / m2;

    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    alwan_scalar E_pow_m2_inv = ALWAN_POW(E, m2_inv);
    alwan_scalar numerator = ALWAN_SELECT(E_pow_m2_inv - c1 < ALWAN_ZERO,
                                          ALWAN_ZERO, E_pow_m2_inv - c1);
    alwan_scalar denominator = c2 - c3 * E_pow_m2_inv;
    alwan_scalar Y = ALWAN_POW(numerator / denominator, m1_inv);
    return Y * ALWAN_LITERAL(10000.0);
}

/* ================================================================
 * HLG (Hybrid Log-Gamma) Transfer Functions
 * ================================================================ */

/* HLG OETF: scene-referred linear -> HLG signal */
ALWAN_INLINE alwan_scalar alwan_hlg_oetf(alwan_scalar linear) {
    alwan_scalar a = ALWAN_LITERAL(0.17883277);
    alwan_scalar b = ALWAN_ONE - ALWAN_LITERAL(4.0) * a;
    alwan_scalar c = ALWAN_LITERAL(0.5) - a * ALWAN_LN(ALWAN_LITERAL(4.0) * a);

    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
    alwan_scalar sqrt_result = ALWAN_SQRT(ALWAN_LITERAL(3.0) * L);
    alwan_scalar log_result  = a * ALWAN_LN(ALWAN_LITERAL(12.0) * L - b) + c;
    return ALWAN_SELECT(L <= ALWAN_ONE / ALWAN_LITERAL(12.0),
                        sqrt_result, log_result);
}

/* HLG EOTF: HLG signal -> display-referred linear (with system gamma 1.2) */
ALWAN_INLINE alwan_scalar alwan_hlg_eotf(alwan_scalar encoded) {
    alwan_scalar a = ALWAN_LITERAL(0.17883277);
    alwan_scalar b = ALWAN_ONE - ALWAN_LITERAL(4.0) * a;
    alwan_scalar c = ALWAN_LITERAL(0.5) - a * ALWAN_LN(ALWAN_LITERAL(4.0) * a);

    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    alwan_scalar linear_lo = (E * E) / ALWAN_LITERAL(3.0);
    alwan_scalar linear_hi = (ALWAN_EXP((E - c) / a) + b) / ALWAN_LITERAL(12.0);
    alwan_scalar linear = ALWAN_SELECT(E <= ALWAN_LITERAL(0.5),
                                       linear_lo, linear_hi);
    return ALWAN_POW(linear, ALWAN_LITERAL(1.2));
}

/* ================================================================
 * BT.1886 EOTF
 * ================================================================ */

/* BT.1886 EOTF: assumes gamma 2.4, black level 0 */
ALWAN_INLINE alwan_scalar alwan_bt1886_eotf(alwan_scalar encoded) {
    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(E, ALWAN_LITERAL(2.4));
}

/* ================================================================
 * CIE Lab f(t) and f_inv(t) Functions
 * ================================================================ */

/* CIE Lab f(t) function with delta = 6/29 */
ALWAN_INLINE alwan_scalar alwan_lab_f(alwan_scalar t) {
    alwan_scalar delta = ALWAN_LITERAL(6.0) / ALWAN_LITERAL(29.0);
    alwan_scalar delta3 = delta * delta * delta;
    alwan_scalar kappa = ALWAN_ONE / (ALWAN_LITERAL(3.0) * delta * delta);
    alwan_scalar offset = ALWAN_LITERAL(16.0) / ALWAN_LITERAL(116.0);

    alwan_scalar cbrt_result = ALWAN_POW(t, ALWAN_ONE / ALWAN_LITERAL(3.0));
    alwan_scalar linear_result = kappa * t + offset;
    return ALWAN_SELECT(t > delta3, cbrt_result, linear_result);
}

/* Inverse of CIE Lab f(t) function */
ALWAN_INLINE alwan_scalar alwan_lab_f_inv(alwan_scalar t) {
    alwan_scalar delta = ALWAN_LITERAL(6.0) / ALWAN_LITERAL(29.0);
    alwan_scalar kappa = ALWAN_LITERAL(3.0) * delta * delta;
    alwan_scalar offset = ALWAN_LITERAL(16.0) / ALWAN_LITERAL(116.0);

    alwan_scalar cube_result = t * t * t;
    alwan_scalar linear_result = kappa * (t - offset);
    return ALWAN_SELECT(t > delta, cube_result, linear_result);
}

#endif /* ALWAN_CORE_H */
