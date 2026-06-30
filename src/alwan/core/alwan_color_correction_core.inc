/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Color Correction (Lift/Gamma/Gain, Color Matrix, parameterized)
 * No include guard - included twice via setup/teardown.
 */

/* ================================================================
 * Lift/Gamma/Gain (LGG)
 * Formula per channel: ((rgb + lift) ^ (1/gamma)) * gain
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_lgg_apply)(ALWAN_CORE_RGB rgb, ALWAN_CORE_RGB lift, ALWAN_CORE_RGB gamma, ALWAN_CORE_RGB gain) {
    ALWAN_CORE_RGB result;

    /* R channel */
    ALWAN_CORE_T lifted_r = ALWAN_CORE_SELECT(rgb.r + lift.r < ALWAN_CORE_LITERAL(0.0),
                                         ALWAN_CORE_LITERAL(0.0),
                                         rgb.r + lift.r);
    ALWAN_CORE_T gamma_safe_r = ALWAN_CORE_SELECT(gamma.r <= ALWAN_CORE_LITERAL(0.0001),
                                             ALWAN_CORE_LITERAL(0.0001),
                                             gamma.r);
    ALWAN_CORE_T gamma_corrected_r = ALWAN_CORE_POW(lifted_r, ALWAN_CORE_LITERAL(1.0) / gamma_safe_r);
    result.r = gamma_corrected_r * gain.r;

    /* G channel */
    ALWAN_CORE_T lifted_g = ALWAN_CORE_SELECT(rgb.g + lift.g < ALWAN_CORE_LITERAL(0.0),
                                         ALWAN_CORE_LITERAL(0.0),
                                         rgb.g + lift.g);
    ALWAN_CORE_T gamma_safe_g = ALWAN_CORE_SELECT(gamma.g <= ALWAN_CORE_LITERAL(0.0001),
                                             ALWAN_CORE_LITERAL(0.0001),
                                             gamma.g);
    ALWAN_CORE_T gamma_corrected_g = ALWAN_CORE_POW(lifted_g, ALWAN_CORE_LITERAL(1.0) / gamma_safe_g);
    result.g = gamma_corrected_g * gain.g;

    /* B channel */
    ALWAN_CORE_T lifted_b = ALWAN_CORE_SELECT(rgb.b + lift.b < ALWAN_CORE_LITERAL(0.0),
                                         ALWAN_CORE_LITERAL(0.0),
                                         rgb.b + lift.b);
    ALWAN_CORE_T gamma_safe_b = ALWAN_CORE_SELECT(gamma.b <= ALWAN_CORE_LITERAL(0.0001),
                                             ALWAN_CORE_LITERAL(0.0001),
                                             gamma.b);
    ALWAN_CORE_T gamma_corrected_b = ALWAN_CORE_POW(lifted_b, ALWAN_CORE_LITERAL(1.0) / gamma_safe_b);
    result.b = gamma_corrected_b * gain.b;

    return result;
}

/* ================================================================
 * Color Matrix Application
 * Result = Matrix * RGB (treating RGB as column vector)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_color_matrix_apply)(ALWAN_CORE_RGB rgb, ALWAN_CORE_MAT3X3 matrix) {
    ALWAN_CORE_RGB result;
    result.r = matrix.m[0] * rgb.r + matrix.m[1] * rgb.g + matrix.m[2] * rgb.b;
    result.g = matrix.m[3] * rgb.r + matrix.m[4] * rgb.g + matrix.m[5] * rgb.b;
    result.b = matrix.m[6] * rgb.r + matrix.m[7] * rgb.g + matrix.m[8] * rgb.b;
    return result;
}

/* ================================================================
 * Printer Lights
 * Film-printing convention: 25 lights = neutral (no change).
 * Each light unit = 0.025 log10 exposure change (1 stop ~ 40 lights).
 * Formula: output = input * 10^((25 - lights) * 0.025)
 * Source: traditional photochemical film printing; see Kodak Reference Manual
 *   for Cineon (1992) and ISO 5-2:2001 "Photography -- Density measurements".
 * 25 neutral lights and 0.025 log step are industry convention, not a standard.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_printer_lights_apply)(ALWAN_CORE_RGB rgb,
                                                      ALWAN_CORE_T red_lights,
                                                      ALWAN_CORE_T green_lights,
                                                      ALWAN_CORE_T blue_lights) {
    ALWAN_CORE_RGB result;

    ALWAN_CORE_T default_lights = ALWAN_CORE_LITERAL(25.0);
    ALWAN_CORE_T log_step = ALWAN_CORE_LITERAL(0.025);
    ALWAN_CORE_T ln10 = ALWAN_CORE_LITERAL(2.302585092994046);

    ALWAN_CORE_T red_exposure   = (default_lights - red_lights)   * log_step;
    ALWAN_CORE_T green_exposure = (default_lights - green_lights) * log_step;
    ALWAN_CORE_T blue_exposure  = (default_lights - blue_lights)  * log_step;

    result.r = rgb.r * ALWAN_CORE_EXP(red_exposure   * ln10);
    result.g = rgb.g * ALWAN_CORE_EXP(green_exposure * ln10);
    result.b = rgb.b * ALWAN_CORE_EXP(blue_exposure  * ln10);

    return result;
}

/* ================================================================
 * White Balance
 * ================================================================ */

/* Apply white balance multipliers: output = rgb * multipliers */
ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_white_balance_apply)(ALWAN_CORE_RGB rgb, ALWAN_CORE_RGB multipliers) {
    ALWAN_CORE_RGB result;
    result.r = rgb.r * multipliers.r;
    result.g = rgb.g * multipliers.g;
    result.b = rgb.b * multipliers.b;
    return result;
}

/* Compute white balance multipliers from measured gray patch (branchless).
 * Normalizes each channel relative to the minimum channel.
 * Returns identity {1,1,1} if any channel <= 0. */
ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_white_balance_from_gray)(ALWAN_CORE_RGB measured_gray) {
    ALWAN_CORE_RGB result;

    ALWAN_CORE_T r = measured_gray.r;
    ALWAN_CORE_T g = measured_gray.g;
    ALWAN_CORE_T b = measured_gray.b;

    /* Branchless min of three */
    ALWAN_CORE_T min_rg = ALWAN_CORE_SELECT(r < g, r, g);
    ALWAN_CORE_T min_val = ALWAN_CORE_SELECT(min_rg < b, min_rg, b);

    /* Guard: if min_val <= 0, return identity multipliers */
    ALWAN_CORE_T valid = ALWAN_CORE_SELECT(min_val <= ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);

    result.r = ALWAN_CORE_SELECT(valid > ALWAN_CORE_LITERAL(0.5), min_val / r, ALWAN_CORE_ONE);
    result.g = ALWAN_CORE_SELECT(valid > ALWAN_CORE_LITERAL(0.5), min_val / g, ALWAN_CORE_ONE);
    result.b = ALWAN_CORE_SELECT(valid > ALWAN_CORE_LITERAL(0.5), min_val / b, ALWAN_CORE_ONE);

    return result;
}
