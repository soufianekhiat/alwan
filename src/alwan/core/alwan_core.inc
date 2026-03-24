/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only branchless transfer functions (parameterized)
 * No include guard - included twice via setup/teardown.
 */

/* ================================================================
 * sRGB Transfer Functions
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_srgb_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T linear_result = ALWAN_CORE_LITERAL(12.92) * linear;
    ALWAN_CORE_T gamma_result  = ALWAN_CORE_LITERAL(1.055)
        * ALWAN_CORE_POW(linear, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(2.4)) - ALWAN_CORE_LITERAL(0.055);
    return ALWAN_CORE_SELECT(linear <= ALWAN_CORE_LITERAL(0.0031308),
                              linear_result, gamma_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_srgb_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T linear_result = encoded / ALWAN_CORE_LITERAL(12.92);
    ALWAN_CORE_T gamma_result  = ALWAN_CORE_POW(
        (encoded + ALWAN_CORE_LITERAL(0.055)) / ALWAN_CORE_LITERAL(1.055),
        ALWAN_CORE_LITERAL(2.4));
    return ALWAN_CORE_SELECT(encoded <= ALWAN_CORE_LITERAL(0.04045),
                              linear_result, gamma_result);
}

/* ================================================================
 * BT.2020 Transfer Functions
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_bt2020_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T linear_result = ALWAN_CORE_LITERAL(4.5) * linear;
    ALWAN_CORE_T gamma_result  = ALWAN_CORE_LITERAL(1.099)
        * ALWAN_CORE_POW(linear, ALWAN_CORE_LITERAL(0.45)) - ALWAN_CORE_LITERAL(0.099);
    return ALWAN_CORE_SELECT(linear < ALWAN_CORE_LITERAL(0.018),
                              linear_result, gamma_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_bt2020_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T threshold = ALWAN_CORE_LITERAL(4.5) * ALWAN_CORE_LITERAL(0.018);
    ALWAN_CORE_T linear_result = encoded / ALWAN_CORE_LITERAL(4.5);
    ALWAN_CORE_T gamma_result  = ALWAN_CORE_POW(
        (encoded + ALWAN_CORE_LITERAL(0.099)) / ALWAN_CORE_LITERAL(1.099),
        ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(0.45));
    return ALWAN_CORE_SELECT(encoded < threshold,
                              linear_result, gamma_result);
}

/* ================================================================
 * PQ (ST.2084) Transfer Functions
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_pq_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T m1 = ALWAN_CORE_LITERAL(2610.0) / ALWAN_CORE_LITERAL(16384.0);
    ALWAN_CORE_T m2 = ALWAN_CORE_LITERAL(2523.0) / ALWAN_CORE_LITERAL(32.0);
    ALWAN_CORE_T c1 = ALWAN_CORE_LITERAL(3424.0) / ALWAN_CORE_LITERAL(4096.0);
    ALWAN_CORE_T c2 = ALWAN_CORE_LITERAL(2413.0) / ALWAN_CORE_LITERAL(128.0);
    ALWAN_CORE_T c3 = ALWAN_CORE_LITERAL(2392.0) / ALWAN_CORE_LITERAL(128.0);

    ALWAN_CORE_T Y = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO,
                                        linear / ALWAN_CORE_LITERAL(10000.0));
    ALWAN_CORE_T Y_pow_m1 = ALWAN_CORE_POW(Y, m1);
    ALWAN_CORE_T numerator = c1 + c2 * Y_pow_m1;
    ALWAN_CORE_T denominator = ALWAN_CORE_ONE + c3 * Y_pow_m1;
    return ALWAN_CORE_POW(numerator / denominator, m2);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_pq_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T m1 = ALWAN_CORE_LITERAL(2610.0) / ALWAN_CORE_LITERAL(16384.0);
    ALWAN_CORE_T m2 = ALWAN_CORE_LITERAL(2523.0) / ALWAN_CORE_LITERAL(32.0);
    ALWAN_CORE_T c1 = ALWAN_CORE_LITERAL(3424.0) / ALWAN_CORE_LITERAL(4096.0);
    ALWAN_CORE_T c2 = ALWAN_CORE_LITERAL(2413.0) / ALWAN_CORE_LITERAL(128.0);
    ALWAN_CORE_T c3 = ALWAN_CORE_LITERAL(2392.0) / ALWAN_CORE_LITERAL(128.0);
    ALWAN_CORE_T m1_inv = ALWAN_CORE_ONE / m1;
    ALWAN_CORE_T m2_inv = ALWAN_CORE_ONE / m2;

    ALWAN_CORE_T E = ALWAN_CORE_SELECT(encoded < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, encoded);
    ALWAN_CORE_T E_pow_m2_inv = ALWAN_CORE_POW(E, m2_inv);
    ALWAN_CORE_T numerator = ALWAN_CORE_SELECT(E_pow_m2_inv - c1 < ALWAN_CORE_ZERO,
                                                ALWAN_CORE_ZERO, E_pow_m2_inv - c1);
    ALWAN_CORE_T denominator = c2 - c3 * E_pow_m2_inv;
    ALWAN_CORE_T Y = ALWAN_CORE_POW(numerator / denominator, m1_inv);
    return Y * ALWAN_CORE_LITERAL(10000.0);
}

/* ================================================================
 * HLG (Hybrid Log-Gamma) Transfer Functions
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_hlg_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.17883277);
    ALWAN_CORE_T b = ALWAN_CORE_ONE - ALWAN_CORE_LITERAL(4.0) * a;
    ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.5) - a * ALWAN_CORE_LN(ALWAN_CORE_LITERAL(4.0) * a);

    ALWAN_CORE_T L = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    ALWAN_CORE_T sqrt_result = ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(3.0) * L);
    ALWAN_CORE_T log_result  = a * ALWAN_CORE_LN(ALWAN_CORE_LITERAL(12.0) * L - b) + c;
    return ALWAN_CORE_SELECT(L <= ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(12.0),
                              sqrt_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_hlg_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.17883277);
    ALWAN_CORE_T b = ALWAN_CORE_ONE - ALWAN_CORE_LITERAL(4.0) * a;
    ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.5) - a * ALWAN_CORE_LN(ALWAN_CORE_LITERAL(4.0) * a);

    ALWAN_CORE_T E = ALWAN_CORE_SELECT(encoded < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, encoded);
    ALWAN_CORE_T linear_lo = (E * E) / ALWAN_CORE_LITERAL(3.0);
    ALWAN_CORE_T linear_hi = (ALWAN_CORE_EXP((E - c) / a) + b) / ALWAN_CORE_LITERAL(12.0);
    ALWAN_CORE_T linear = ALWAN_CORE_SELECT(E <= ALWAN_CORE_LITERAL(0.5),
                                             linear_lo, linear_hi);
    return ALWAN_CORE_POW(linear, ALWAN_CORE_LITERAL(1.2));
}

/* ================================================================
 * BT.1886 EOTF
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_bt1886_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T E = ALWAN_CORE_SELECT(encoded < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, encoded);
    return ALWAN_CORE_POW(E, ALWAN_CORE_LITERAL(2.4));
}

/* ================================================================
 * CIE Lab f(t) and f_inv(t) Functions
 * Reference: CIE 15:2004 "Colorimetry" 3rd edition, eq. (A1-A4).
 *   f(t) = t^(1/3) if t > (6/29)^3, else (29/6)^2 * t/3 + 16/116
 * Threshold delta = 6/29 is exact; (6/29)^3 ≈ 0.008856, 16/116 ≈ 0.13793.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_lab_f)(ALWAN_CORE_T t) {
    ALWAN_CORE_T delta = ALWAN_CORE_LITERAL(6.0) / ALWAN_CORE_LITERAL(29.0);
    ALWAN_CORE_T delta3 = delta * delta * delta;
    ALWAN_CORE_T kappa = ALWAN_CORE_ONE / (ALWAN_CORE_LITERAL(3.0) * delta * delta);
    ALWAN_CORE_T offset = ALWAN_CORE_LITERAL(16.0) / ALWAN_CORE_LITERAL(116.0);

    ALWAN_CORE_T cbrt_result = ALWAN_CORE_POW(t, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));
    ALWAN_CORE_T linear_result = kappa * t + offset;
    return ALWAN_CORE_SELECT(t > delta3, cbrt_result, linear_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_lab_f_inv)(ALWAN_CORE_T t) {
    ALWAN_CORE_T delta = ALWAN_CORE_LITERAL(6.0) / ALWAN_CORE_LITERAL(29.0);
    ALWAN_CORE_T kappa = ALWAN_CORE_LITERAL(3.0) * delta * delta;
    ALWAN_CORE_T offset = ALWAN_CORE_LITERAL(16.0) / ALWAN_CORE_LITERAL(116.0);

    ALWAN_CORE_T cube_result = t * t * t;
    ALWAN_CORE_T linear_result = kappa * (t - offset);
    return ALWAN_CORE_SELECT(t > delta, cube_result, linear_result);
}
