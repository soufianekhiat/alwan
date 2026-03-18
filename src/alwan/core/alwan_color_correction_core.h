/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Color Correction (Lift/Gamma/Gain, Color Matrix)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_COLOR_CORRECTION_CORE_H
#define ALWAN_COLOR_CORRECTION_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_math_core.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_color_correction_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_color_correction_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_INLINE alwan_rgb alwan_lgg_apply_v(alwan_rgb rgb, alwan_rgb lift, alwan_rgb gamma, alwan_rgb gain) {
    alwan_rgb result;
    alwan_scalar lifted_r = ALWAN_SELECT(rgb.r + lift.r < ALWAN_LITERAL(0.0),
                                         ALWAN_LITERAL(0.0), rgb.r + lift.r);
    alwan_scalar gamma_safe_r = ALWAN_SELECT(gamma.r <= ALWAN_LITERAL(0.0001),
                                             ALWAN_LITERAL(0.0001), gamma.r);
    alwan_scalar gamma_corrected_r = ALWAN_POW(lifted_r, ALWAN_LITERAL(1.0) / gamma_safe_r);
    result.r = gamma_corrected_r * gain.r;
    alwan_scalar lifted_g = ALWAN_SELECT(rgb.g + lift.g < ALWAN_LITERAL(0.0),
                                         ALWAN_LITERAL(0.0), rgb.g + lift.g);
    alwan_scalar gamma_safe_g = ALWAN_SELECT(gamma.g <= ALWAN_LITERAL(0.0001),
                                             ALWAN_LITERAL(0.0001), gamma.g);
    alwan_scalar gamma_corrected_g = ALWAN_POW(lifted_g, ALWAN_LITERAL(1.0) / gamma_safe_g);
    result.g = gamma_corrected_g * gain.g;
    alwan_scalar lifted_b = ALWAN_SELECT(rgb.b + lift.b < ALWAN_LITERAL(0.0),
                                         ALWAN_LITERAL(0.0), rgb.b + lift.b);
    alwan_scalar gamma_safe_b = ALWAN_SELECT(gamma.b <= ALWAN_LITERAL(0.0001),
                                             ALWAN_LITERAL(0.0001), gamma.b);
    alwan_scalar gamma_corrected_b = ALWAN_POW(lifted_b, ALWAN_LITERAL(1.0) / gamma_safe_b);
    result.b = gamma_corrected_b * gain.b;
    return result;
}

ALWAN_INLINE alwan_rgb alwan_color_matrix_apply_v(alwan_rgb rgb, alwan_mat3x3 matrix) {
    alwan_rgb result;
    result.r = matrix.m[0] * rgb.r + matrix.m[1] * rgb.g + matrix.m[2] * rgb.b;
    result.g = matrix.m[3] * rgb.r + matrix.m[4] * rgb.g + matrix.m[5] * rgb.b;
    result.b = matrix.m[6] * rgb.r + matrix.m[7] * rgb.g + matrix.m[8] * rgb.b;
    return result;
}

ALWAN_INLINE alwan_rgb alwan_printer_lights_apply_v(alwan_rgb rgb,
                                                      alwan_scalar red_lights,
                                                      alwan_scalar green_lights,
                                                      alwan_scalar blue_lights) {
    alwan_rgb result;
    alwan_scalar default_lights = ALWAN_LITERAL(25.0);
    alwan_scalar log_step = ALWAN_LITERAL(0.025);
    alwan_scalar ln10 = ALWAN_LITERAL(2.302585092994046);
    alwan_scalar red_exposure   = (default_lights - red_lights)   * log_step;
    alwan_scalar green_exposure = (default_lights - green_lights) * log_step;
    alwan_scalar blue_exposure  = (default_lights - blue_lights)  * log_step;
    result.r = rgb.r * ALWAN_EXP(red_exposure   * ln10);
    result.g = rgb.g * ALWAN_EXP(green_exposure * ln10);
    result.b = rgb.b * ALWAN_EXP(blue_exposure  * ln10);
    return result;
}

ALWAN_INLINE alwan_rgb alwan_white_balance_apply_v(alwan_rgb rgb, alwan_rgb multipliers) {
    alwan_rgb result;
    result.r = rgb.r * multipliers.r;
    result.g = rgb.g * multipliers.g;
    result.b = rgb.b * multipliers.b;
    return result;
}

ALWAN_INLINE alwan_rgb alwan_white_balance_from_gray_v(alwan_rgb measured_gray) {
    alwan_rgb result;
    alwan_scalar r = measured_gray.r;
    alwan_scalar g = measured_gray.g;
    alwan_scalar b = measured_gray.b;
    alwan_scalar min_rg = ALWAN_SELECT(r < g, r, g);
    alwan_scalar min_val = ALWAN_SELECT(min_rg < b, min_rg, b);
    alwan_scalar valid = ALWAN_SELECT(min_val <= ALWAN_ZERO, ALWAN_ZERO, ALWAN_ONE);
    result.r = ALWAN_SELECT(valid > ALWAN_LITERAL(0.5), min_val / r, ALWAN_ONE);
    result.g = ALWAN_SELECT(valid > ALWAN_LITERAL(0.5), min_val / g, ALWAN_ONE);
    result.b = ALWAN_SELECT(valid > ALWAN_LITERAL(0.5), min_val / b, ALWAN_ONE);
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_COLOR_CORRECTION_CORE_H */
