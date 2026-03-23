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

    alwan_scalar luma = ALWAN_LUMA_KR_BT709 * rgb.v[0]
                      + ALWAN_LUMA_KG_BT709 * rgb.v[1]
                      + ALWAN_LUMA_KB_BT709 * rgb.v[2];

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

    alwan_scalar luma = ALWAN_LUMA_KR_BT709 * rgb.v[0]
                      + ALWAN_LUMA_KG_BT709 * rgb.v[1]
                      + ALWAN_LUMA_KB_BT709 * rgb.v[2];

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

/* ================================================================
 * Khronos PBR Neutral Tone Mapping
 *
 * Released May 2024 for e-commerce / product visualization.
 * Colors below ~0.76 linear pass through; above it, a soft
 * rolloff compresses highlights with slight desaturation.
 *
 * Reference: https://github.com/KhronosGroup/ToneMapping/tree/main/PBR_Neutral
 * ================================================================ */

ALWAN_INLINE alwan_vec3 alwan_khronos_pbr_neutral_v(alwan_vec3 color) {
    alwan_vec3 result;
    alwan_scalar const start_compression = ALWAN_LITERAL(0.8) - ALWAN_LITERAL(0.04);
    alwan_scalar const desaturation = ALWAN_LITERAL(0.15);

    /* Compute offset from min channel */
    alwan_scalar x = color.v[0];
    x = ALWAN_SELECT(color.v[1] < x, color.v[1], x);
    x = ALWAN_SELECT(color.v[2] < x, color.v[2], x);
    alwan_scalar offset = ALWAN_SELECT(x < ALWAN_LITERAL(0.08),
                                        x - ALWAN_LITERAL(6.25) * x * x,
                                        ALWAN_LITERAL(0.04));
    color.v[0] -= offset;
    color.v[1] -= offset;
    color.v[2] -= offset;

    /* Find peak channel */
    alwan_scalar peak = color.v[0];
    peak = ALWAN_SELECT(color.v[1] > peak, color.v[1], peak);
    peak = ALWAN_SELECT(color.v[2] > peak, color.v[2], peak);

    /* Below start_compression: pass-through */
    if (peak < start_compression) {
        return color;
    }

    /* Soft rolloff */
    {
        alwan_scalar const d = ALWAN_LITERAL(1.0) - start_compression;
        alwan_scalar new_peak = ALWAN_LITERAL(1.0) - d * d / (peak + d - start_compression);
        alwan_scalar scale = new_peak / peak;
        color.v[0] *= scale;
        color.v[1] *= scale;
        color.v[2] *= scale;

        /* Desaturation */
        alwan_scalar g = ALWAN_LITERAL(1.0) - ALWAN_LITERAL(1.0) /
                         (desaturation * (peak - new_peak) + ALWAN_LITERAL(1.0));
        result.v[0] = color.v[0] + (new_peak - color.v[0]) * g;
        result.v[1] = color.v[1] + (new_peak - color.v[1]) * g;
        result.v[2] = color.v[2] + (new_peak - color.v[2]) * g;
    }

    return result;
}

/* ================================================================
 * Reinhard Extended Tone Mapping (Luminance-based)
 *
 * From "Photographic Tone Reproduction for Digital Images"
 * (Reinhard et al., SIGGRAPH 2002), Equation 4.
 *
 * Extends the basic Reinhard operator C/(1+C) with a white point
 * parameter that controls the luminance level that maps to pure
 * white. As max_white -> infinity, reduces to basic Reinhard.
 *
 * This luminance-based variant applies the curve to BT.709 luminance
 * and scales all channels proportionally, preserving hue and
 * saturation better than the per-channel form.
 *
 * References:
 *   Paper:  https://dl.acm.org/doi/10.1145/566654.566575
 *   PDF:    https://www.cs.utah.edu/docs/techreports/2002/pdf/UUCS-02-001.pdf
 *   Review: https://64.github.io/tonemapping/
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_reinhard_extended_v(alwan_scalar v,
                                                     alwan_scalar max_white) {
    alwan_scalar ww = max_white * max_white;
    return (v * (ALWAN_ONE + v / ww)) / (ALWAN_ONE + v);
}

ALWAN_INLINE alwan_vec3 alwan_reinhard_extended_luma_v(alwan_vec3 rgb,
                                                        alwan_scalar max_white) {
    alwan_vec3 result;
    alwan_scalar l_old = ALWAN_LUMA_KR_BT709 * rgb.v[0]
                       + ALWAN_LUMA_KG_BT709 * rgb.v[1]
                       + ALWAN_LUMA_KB_BT709 * rgb.v[2];

    if (l_old < ALWAN_LITERAL(1e-10)) {
        result.v[0] = ALWAN_ZERO;
        result.v[1] = ALWAN_ZERO;
        result.v[2] = ALWAN_ZERO;
        return result;
    }

    alwan_scalar l_new = alwan_reinhard_extended_v(l_old, max_white);
    alwan_scalar scale = l_new / l_old;

    result.v[0] = rgb.v[0] * scale;
    result.v[1] = rgb.v[1] * scale;
    result.v[2] = rgb.v[2] * scale;
    return result;
}

/* ================================================================
 * Uchimura / Gran Turismo Tone Mapping
 *
 * Three-segment piecewise curve (toe + linear + shoulder) with
 * smooth blending, designed for HDR display mapping in Gran Turismo
 * Sport / GT7.
 *
 * Parameters:
 *   P  = 1.0   Maximum display brightness (peak value)
 *   a  = 1.0   Contrast (slope of linear section)
 *   m  = 0.22  Linear section start
 *   l  = 0.4   Linear section length
 *   c  = 1.33  Black tightness (toe curvature)
 *   b  = 0.0   Pedestal (black level lift)
 *
 * References:
 *   Presentation: "HDR Theory and Practice" (Uchimura, CEDEC 2017)
 *   SIGGRAPH:     https://www.polyphony.co.jp/publications/sa2018/
 *   Desmos:       https://www.desmos.com/calculator/gslcdxvipg
 *   GLSL impl:    https://github.com/dmnsgn/glsl-tone-map/blob/main/uchimura.glsl
 *   Shadertoy:    https://www.shadertoy.com/view/Xstyzn
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_uchimura_v(alwan_scalar x,
                                             alwan_scalar P,
                                             alwan_scalar a,
                                             alwan_scalar m,
                                             alwan_scalar l,
                                             alwan_scalar c,
                                             alwan_scalar b) {
    /* Derived constants */
    alwan_scalar l0 = ((P - m) * l) / a;
    alwan_scalar S0 = m + l0;
    alwan_scalar S1 = m + a * l0;
    alwan_scalar C2 = (a * P) / (P - S1);
    alwan_scalar CP = -C2 / P;

    /* Smoothstep weights: 3t^2 - 2t^3 */
    alwan_scalar t_toe = alwan_saturate((x < m) ? (ALWAN_ONE - x / m) : ALWAN_ZERO);
    alwan_scalar w0 = t_toe * t_toe * (ALWAN_LITERAL(3.0) - ALWAN_LITERAL(2.0) * t_toe);
    alwan_scalar w2 = (x > S0) ? ALWAN_ONE : ALWAN_ZERO;
    alwan_scalar w1 = ALWAN_ONE - w0 - w2;

    /* Toe: power curve + pedestal */
    alwan_scalar T = m * ALWAN_POW(x / m, c) + b;

    /* Linear section */
    alwan_scalar L = m + a * (x - m);

    /* Shoulder: exponential asymptote to P */
    alwan_scalar S = P - (P - S1) * ALWAN_EXP(CP * (x - S0));

    return T * w0 + L * w1 + S * w2;
}

ALWAN_INLINE alwan_scalar alwan_uchimura_default_v(alwan_scalar x) {
    return alwan_uchimura_v(x,
        ALWAN_ONE,                  /* P = 1.0  */
        ALWAN_ONE,                  /* a = 1.0  */
        ALWAN_LITERAL(0.22),        /* m = 0.22 */
        ALWAN_LITERAL(0.4),         /* l = 0.4  */
        ALWAN_LITERAL(1.33),        /* c = 1.33 */
        ALWAN_ZERO);                /* b = 0.0  */
}

/* ================================================================
 * Lottes / AMD Cauldron Tone Mapping
 *
 * Parametric rational curve: pow(x,a) / (pow(x,a*d) * b + c)
 * where b and c are derived from the constraint that the curve
 * passes through (midIn, midOut) and approaches 1.0 at hdrMax.
 *
 * Applied to the peak channel to preserve hue, with optional
 * desaturation/crosstalk for highlight rolloff.
 *
 * References:
 *   GDC 2016:  https://gpuopen.com/wp-content/uploads/2016/03/GdcVdrLottes.pdf
 *   Follow-up: https://gpuopen.com/learn/vdr-follow-up-tonemapping-for-hdr-signals/
 *   Cauldron:  https://github.com/GPUOpen-LibrariesAndSDKs/Cauldron
 *   GLSL impl: https://github.com/dmnsgn/glsl-tone-map/blob/main/lottes.glsl
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_lottes_curve_v(alwan_scalar x,
                                                 alwan_scalar a,
                                                 alwan_scalar d,
                                                 alwan_scalar b,
                                                 alwan_scalar c) {
    alwan_scalar xp = ALWAN_POW(x, a);
    return xp / (ALWAN_POW(x, a * d) * b + c);
}

ALWAN_INLINE alwan_vec3 alwan_lottes_v(alwan_vec3 rgb,
                                         alwan_scalar contrast,
                                         alwan_scalar shoulder,
                                         alwan_scalar hdr_max,
                                         alwan_scalar mid_in,
                                         alwan_scalar mid_out) {
    alwan_vec3 result;

    /* Precompute b and c from constraints */
    alwan_scalar pow_hdr_a   = ALWAN_POW(hdr_max, contrast);
    alwan_scalar pow_hdr_ad  = ALWAN_POW(hdr_max, contrast * shoulder);
    alwan_scalar pow_mid_a   = ALWAN_POW(mid_in,  contrast);
    alwan_scalar pow_mid_ad  = ALWAN_POW(mid_in,  contrast * shoulder);

    alwan_scalar b = (-pow_mid_a + pow_hdr_a * mid_out)
                   / ((pow_hdr_ad - pow_mid_ad) * mid_out);
    alwan_scalar c = (pow_hdr_ad * pow_mid_a - pow_hdr_a * pow_mid_ad * mid_out)
                   / ((pow_hdr_ad - pow_mid_ad) * mid_out);

    /* Apply curve to peak channel, scale others proportionally */
    alwan_scalar peak = alwan_max3(rgb.v[0], rgb.v[1], rgb.v[2]);

    if (peak < ALWAN_LITERAL(1e-10)) {
        result.v[0] = ALWAN_ZERO;
        result.v[1] = ALWAN_ZERO;
        result.v[2] = ALWAN_ZERO;
        return result;
    }

    alwan_scalar new_peak = alwan_lottes_curve_v(peak, contrast, shoulder, b, c);
    alwan_scalar scale = new_peak / peak;

    result.v[0] = rgb.v[0] * scale;
    result.v[1] = rgb.v[1] * scale;
    result.v[2] = rgb.v[2] * scale;
    return result;
}

ALWAN_INLINE alwan_vec3 alwan_lottes_default_v(alwan_vec3 rgb) {
    return alwan_lottes_v(rgb,
        ALWAN_LITERAL(1.6),         /* contrast  */
        ALWAN_LITERAL(0.977),       /* shoulder  */
        ALWAN_LITERAL(8.0),         /* hdr_max   */
        ALWAN_LITERAL(0.18),        /* mid_in    */
        ALWAN_LITERAL(0.267));      /* mid_out   */
}

/* ================================================================
 * "Somewhat Boring Display Transform" (Stachowiak, 2023)
 *
 * Analytical tone mapper by Tomasz Stachowiak (h3r2tic), designed
 * as a practical replacement for per-channel Reinhard. Works in
 * BT.709 YCbCr space: compresses luminance with 1-exp(-v), then
 * selectively desaturates bright chromatic content to avoid hue
 * shifts from clipping.
 *
 * This is the analytical companion to the LUT-based Tony McMapface.
 * Used by the Bevy game engine as its default tone mapper.
 *
 * References:
 *   Tony McMapface repo: https://github.com/h3r2tic/tony-mc-mapface
 *   Bevy engine impl:    https://github.com/bevyengine/bevy (tonemapping_shared.wgsl)
 *   Alex Tardif review:  https://alextardif.com/Tonemapping.html
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_tony_curve_v(alwan_scalar v) {
    return ALWAN_ONE - ALWAN_EXP(-v);
}

ALWAN_INLINE alwan_vec3 alwan_tony_mcmapface_v(alwan_vec3 col) {
    alwan_vec3 result;

    /* BT.709 luma */
    alwan_scalar luma = ALWAN_LUMA_KR_BT709 * col.v[0]
                      + ALWAN_LUMA_KG_BT709 * col.v[1]
                      + ALWAN_LUMA_KB_BT709 * col.v[2];

    /* BT.709 Cb, Cr (simplified, not full-range) */
    alwan_scalar cb = col.v[2] - luma;
    alwan_scalar cr = col.v[0] - luma;

    /* Chroma magnitude drives desaturation */
    alwan_scalar chroma_len = ALWAN_SQRT(cb * cb + cr * cr);
    alwan_scalar bt = alwan_tony_curve_v(chroma_len * ALWAN_LITERAL(2.4));
    alwan_scalar desat = alwan_max(
        (bt - ALWAN_LITERAL(0.7)) * ALWAN_LITERAL(0.8), ALWAN_ZERO);
    desat = desat * desat;

    /* Desaturate toward luma */
    alwan_scalar desat_r = col.v[0] + (luma - col.v[0]) * desat;
    alwan_scalar desat_g = col.v[1] + (luma - col.v[1]) * desat;
    alwan_scalar desat_b = col.v[2] + (luma - col.v[2]) * desat;

    /* Tone-map luminance */
    alwan_scalar tm_luma = alwan_tony_curve_v(luma);

    /* Luminance-preserving scale of original color */
    alwan_scalar orig_luma = ALWAN_LUMA_KR_BT709 * col.v[0]
                           + ALWAN_LUMA_KG_BT709 * col.v[1]
                           + ALWAN_LUMA_KB_BT709 * col.v[2];
    alwan_scalar luma_scale = (orig_luma > ALWAN_LITERAL(1e-5))
                            ? (tm_luma / orig_luma) : ALWAN_ZERO;

    alwan_scalar tm0_r = col.v[0] * luma_scale;
    alwan_scalar tm0_g = col.v[1] * luma_scale;
    alwan_scalar tm0_b = col.v[2] * luma_scale;

    /* Per-channel tone map of desaturated color */
    alwan_scalar tm1_r = alwan_tony_curve_v(desat_r);
    alwan_scalar tm1_g = alwan_tony_curve_v(desat_g);
    alwan_scalar tm1_b = alwan_tony_curve_v(desat_b);

    /* Blend between luminance-preserving and per-channel based on bt */
    alwan_scalar bt2 = bt * bt;
    alwan_scalar final_mult = ALWAN_LITERAL(0.97);

    result.v[0] = (tm0_r + (tm1_r - tm0_r) * bt2) * final_mult;
    result.v[1] = (tm0_g + (tm1_g - tm0_g) * bt2) * final_mult;
    result.v[2] = (tm0_b + (tm1_b - tm0_b) * bt2) * final_mult;

    return result;
}

#endif /* ALWAN_VIEW_CORE_H */
