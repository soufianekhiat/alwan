/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only View Transform curves
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_VIEW_CORE_H
#define ALWAN_VIEW_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

/* ================================================================
 * ACES Filmic Tone-Map (Hill approximation)
 * f(x) = (x*(a*x+b)) / (x*(c*x+d)+e)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_aces_tonemap_v(alwan_scalar x) {
    alwan_scalar const a = ALWAN_LITERAL(2.51);
    alwan_scalar const b = ALWAN_LITERAL(0.03);
    alwan_scalar const c = ALWAN_LITERAL(2.43);
    alwan_scalar const d = ALWAN_LITERAL(0.59);
    alwan_scalar const e = ALWAN_LITERAL(0.14);

    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

/* ================================================================
 * AgX Log-space encoding
 * Converts linear value to normalized [0,1] log2 range
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_agx_log_encode_v(alwan_scalar x,
                                                   alwan_scalar min_ev,
                                                   alwan_scalar max_ev) {
    alwan_scalar clamped = ALWAN_SELECT(x < ALWAN_LITERAL(1e-10),
                                         ALWAN_LITERAL(1e-10), x);
    alwan_scalar log_x = ALWAN_LOG2(clamped);
    alwan_scalar normalized = (log_x - min_ev) / (max_ev - min_ev);
    return alwan_saturate(normalized);
}

/* ================================================================
 * AgX Base Curve - 6th-order polynomial (Blender 4.0 reference)
 * Input: normalized [0,1] from log encoding
 * Output: display [0,1]
 *
 * Coefficients derived from fitting the Blender 4.0 AgX sigmoid
 * using a 6th-order polynomial for improved highlight rolloff
 * and shadow detail compared to the simpler cubic approximation.
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_agx_curve_v(alwan_scalar t) {
    alwan_scalar t2 = t * t;
    alwan_scalar t3 = t2 * t;
    alwan_scalar t4 = t2 * t2;
    alwan_scalar t5 = t4 * t;
    alwan_scalar t6 = t3 * t3;

    return alwan_saturate(
        ALWAN_LITERAL( 15.5)     * t6 +
        ALWAN_LITERAL(-40.14)    * t5 +
        ALWAN_LITERAL( 31.96)    * t4 +
        ALWAN_LITERAL( -6.868)   * t3 +
        ALWAN_LITERAL(  0.4298)  * t2 +
        ALWAN_LITERAL(  0.1191)  * t +
        ALWAN_LITERAL( -0.00232));
}

/* ================================================================
 * AgX Punchy Grade
 * Applies contrast + saturation boost on top of base curve output
 * ================================================================ */

ALWAN_INLINE alwan_vec3 alwan_agx_punchy_grade_v(alwan_vec3 rgb) {
    alwan_vec3 result;
    alwan_scalar const contrast   = ALWAN_LITERAL(1.15);
    alwan_scalar const saturation = ALWAN_LITERAL(1.2);
    alwan_scalar const mid_gray   = ALWAN_LITERAL(0.18);

    alwan_scalar luma = ALWAN_LITERAL(0.2126) * rgb.v[0]
                      + ALWAN_LITERAL(0.7152) * rgb.v[1]
                      + ALWAN_LITERAL(0.0722) * rgb.v[2];

    result.v[0] = alwan_saturate(mid_gray + (luma + (rgb.v[0] - luma) * saturation - mid_gray) * contrast);
    result.v[1] = alwan_saturate(mid_gray + (luma + (rgb.v[1] - luma) * saturation - mid_gray) * contrast);
    result.v[2] = alwan_saturate(mid_gray + (luma + (rgb.v[2] - luma) * saturation - mid_gray) * contrast);

    return result;
}

/* ================================================================
 * AgX Golden Grade
 *
 * Look transform that warms highlights and cools shadows,
 * inspired by the Blender AgX "Golden" look.
 * ================================================================ */

ALWAN_INLINE alwan_vec3 alwan_agx_golden_grade_v(alwan_vec3 rgb) {
    alwan_vec3 result;

    alwan_scalar luma = ALWAN_LITERAL(0.2126) * rgb.v[0]
                      + ALWAN_LITERAL(0.7152) * rgb.v[1]
                      + ALWAN_LITERAL(0.0722) * rgb.v[2];

    /* Golden: warm highlights, cool shadows */
    /* Highlights: shift toward warm (increase R, slight G, decrease B) */
    /* Shadows: shift toward cool (decrease R, slight G, increase B) */
    alwan_scalar warm_factor = alwan_saturate(luma);  /* 0 in shadows, 1 in highlights */

    /* Base saturation boost */
    alwan_scalar saturation = ALWAN_LITERAL(1.3);
    alwan_scalar contrast   = ALWAN_LITERAL(1.1);
    alwan_scalar mid_gray   = ALWAN_LITERAL(0.18);

    /* Apply saturation around luma */
    alwan_scalar r_sat = luma + (rgb.v[0] - luma) * saturation;
    alwan_scalar g_sat = luma + (rgb.v[1] - luma) * saturation;
    alwan_scalar b_sat = luma + (rgb.v[2] - luma) * saturation;

    /* Apply warm/cool color shift */
    alwan_scalar warm_r = ALWAN_LITERAL(0.02) * warm_factor;
    alwan_scalar cool_b = ALWAN_LITERAL(0.02) * (ALWAN_ONE - warm_factor);

    r_sat = r_sat + warm_r - cool_b * ALWAN_LITERAL(0.5);
    b_sat = b_sat + cool_b - warm_r * ALWAN_LITERAL(0.5);

    /* Apply contrast */
    result.v[0] = alwan_saturate(mid_gray + (r_sat - mid_gray) * contrast);
    result.v[1] = alwan_saturate(mid_gray + (g_sat - mid_gray) * contrast);
    result.v[2] = alwan_saturate(mid_gray + (b_sat - mid_gray) * contrast);

    return result;
}

#endif /* ALWAN_VIEW_CORE_H */
