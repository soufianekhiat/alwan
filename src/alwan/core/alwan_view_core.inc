/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only View Transform curves (parameterized)
 * No include guard - included twice via setup/teardown.
 */

/* ================================================================
 * ACES Filmic Tone-Map (Hill / Narkowicz approximation)
 * f(x) = (x*(a*x+b)) / (x*(c*x+d)+e)
 * Coefficients: Narkowicz, "ACES Filmic Tone Mapping Curve"
 *   https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
 * Not equivalent to the full ACES Reference Rendering Transform (RRT+ODT).
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_aces_tonemap)(ALWAN_CORE_T x) {
    const ALWAN_CORE_T a = ALWAN_CORE_LITERAL(2.51);
    const ALWAN_CORE_T b = ALWAN_CORE_LITERAL(0.03);
    const ALWAN_CORE_T c = ALWAN_CORE_LITERAL(2.43);
    const ALWAN_CORE_T d = ALWAN_CORE_LITERAL(0.59);
    const ALWAN_CORE_T e = ALWAN_CORE_LITERAL(0.14);

    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

/* ================================================================
 * AgX Log-space encoding
 * Converts linear value to normalized [0,1] log2 range
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_agx_log_encode)(ALWAN_CORE_T x,
                                                   ALWAN_CORE_T min_ev,
                                                   ALWAN_CORE_T max_ev) {
    ALWAN_CORE_T clamped = ALWAN_CORE_SELECT(x < ALWAN_CORE_LITERAL(1e-10),
                                         ALWAN_CORE_LITERAL(1e-10), x);
    ALWAN_CORE_T log_x = ALWAN_CORE_LOG2(clamped);
    ALWAN_CORE_T normalized = (log_x - min_ev) / (max_ev - min_ev);
    return ALWAN_CORE_SATURATE(normalized);
}

/* ================================================================
 * AgX Base Curve - 6th-order polynomial
 * Input: normalized [0,1] from log encoding
 * Output: display [0,1]
 *
 * PROVENANCE IS INCOMPLETE, AND THE MISSING PIECE CHANGES THE ANSWER.
 *
 * The comment here used to say the coefficients were fitted to "the Blender 4.0
 * AgX sigmoid", without recording the log range the fit assumed. That range is
 * not a detail: this curve is fed by alwan_agx_log_encode above, which takes
 * min_ev and max_ev FROM THE CALLER, and the choice decides whether the output
 * is AgX or something else entirely. Measured against the AgX this library
 * actually ships, over a neutral ramp from 2^-10 to 2^4 scene linear:
 *
 *   log2 range used     worst |this curve - ALWAN_VIEW_AGX_BLENDER|
 *   -12.474 .. +4.026   0.311      <- AgX's OWN documented range
 *   -12.0   .. +4.0     0.291
 *   -10.0   .. +6.5     0.088
 *    -8.0   .. +5.0     0.040      <- roughly what it was fitted for
 *   -16.0   .. +6.0     0.344
 *
 * So it is not a bad curve, it is an UNLABELLED one. Drive it with AgX's own
 * range and it misses by nearly a third of the display range: 18% grey lands at
 * 0.497 where Blender AgX puts it at 0.180 and AgX original at 0.214.
 *
 * NOTHING IN THIS LIBRARY CALLS IT. alwan_agx_curve_v appears exactly once in
 * each core twin, its own definition; every shipped AgX path goes through
 * alwan_view_transform_apply_{T} instead, and ALWAN_VIEW_AGX_BLENDER is a baked
 * 57^3 LUT rather than any polynomial. It is reachable only by a shader author
 * including this header, which is why the measurement above is written down
 * rather than the function quietly left alone.
 *
 * NOT TESTABLE as "the Blender AgX sigmoid" until the fit is regenerated with
 * its range recorded. If you need AgX, call the view transform. References:
 *   https://github.com/EaryChow/AgX          (the Blender 4.0 implementation)
 *   https://github.com/sobotka/AgX           (Troy Sobotka's original)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_agx_curve)(ALWAN_CORE_T t) {
    ALWAN_CORE_T t2 = t * t;
    ALWAN_CORE_T t3 = t2 * t;
    ALWAN_CORE_T t4 = t2 * t2;
    ALWAN_CORE_T t5 = t4 * t;
    ALWAN_CORE_T t6 = t3 * t3;

    return ALWAN_CORE_SATURATE(
        ALWAN_CORE_LITERAL( 15.5)     * t6 +
        ALWAN_CORE_LITERAL(-40.14)    * t5 +
        ALWAN_CORE_LITERAL( 31.96)    * t4 +
        ALWAN_CORE_LITERAL( -6.868)   * t3 +
        ALWAN_CORE_LITERAL(  0.4298)  * t2 +
        ALWAN_CORE_LITERAL(  0.1191)  * t +
        ALWAN_CORE_LITERAL( -0.00232));
}

/* ================================================================
 * ASC CDL (Color Decision List) -- ASC-CDL v1.2
 * Standard formula: out = clamp01(in * slope + offset) ^ power
 * Then saturation around BT.709 luma.
 * Used by AgX Punchy and Golden looks (sobotka/AgX config.ocio).
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_cdl_apply)(
    ALWAN_CORE_VEC3 rgb,
    ALWAN_CORE_T slope_r, ALWAN_CORE_T slope_g, ALWAN_CORE_T slope_b,
    ALWAN_CORE_T offset_r, ALWAN_CORE_T offset_g, ALWAN_CORE_T offset_b,
    ALWAN_CORE_T power_r, ALWAN_CORE_T power_g, ALWAN_CORE_T power_b,
    ALWAN_CORE_T sat) {
    ALWAN_CORE_VEC3 result;
    /* Slope + Offset + Clamp */
    ALWAN_CORE_T r = ALWAN_CORE_SATURATE(rgb.v[0] * slope_r + offset_r);
    ALWAN_CORE_T g = ALWAN_CORE_SATURATE(rgb.v[1] * slope_g + offset_g);
    ALWAN_CORE_T b = ALWAN_CORE_SATURATE(rgb.v[2] * slope_b + offset_b);
    /* Power */
    r = ALWAN_CORE_POW(r, power_r);
    g = ALWAN_CORE_POW(g, power_g);
    b = ALWAN_CORE_POW(b, power_b);
    /* Saturation around BT.709 luma */
    ALWAN_CORE_T luma = ALWAN_CORE_LUMA_KR_BT709 * r
                      + ALWAN_CORE_LUMA_KG_BT709 * g
                      + ALWAN_CORE_LUMA_KB_BT709 * b;
    result.v[0] = ALWAN_CORE_SATURATE(luma + sat * (r - luma));
    result.v[1] = ALWAN_CORE_SATURATE(luma + sat * (g - luma));
    result.v[2] = ALWAN_CORE_SATURATE(luma + sat * (b - luma));
    return result;
}

/* ================================================================
 * AgX Punchy Grade -- CDL from sobotka/AgX config.ocio
 * CDLTransform {power: [1.35, 1.35, 1.35], sat: 1.4}
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_agx_punchy_grade)(ALWAN_CORE_VEC3 rgb) {
    return ALWAN_CORE_FNVLIT(alwan_cdl_apply)(rgb,
        ALWAN_CORE_ONE, ALWAN_CORE_ONE, ALWAN_CORE_ONE,           /* slope = 1 */
        ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ZERO,       /* offset = 0 */
        ALWAN_CORE_LITERAL(1.35), ALWAN_CORE_LITERAL(1.35), ALWAN_CORE_LITERAL(1.35), /* power */
        ALWAN_CORE_LITERAL(1.4));                                  /* saturation */
}

/* ================================================================
 * AgX Golden Grade -- CDL from sobotka/AgX config.ocio
 * CDLTransform {slope: [1, 0.9, 0.5], power: [0.8, 0.8, 0.8], sat: 1.3}
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_agx_golden_grade)(ALWAN_CORE_VEC3 rgb) {
    return ALWAN_CORE_FNVLIT(alwan_cdl_apply)(rgb,
        ALWAN_CORE_ONE, ALWAN_CORE_LITERAL(0.9), ALWAN_CORE_LITERAL(0.5), /* slope */
        ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ZERO,               /* offset = 0 */
        ALWAN_CORE_LITERAL(0.8), ALWAN_CORE_LITERAL(0.8), ALWAN_CORE_LITERAL(0.8), /* power */
        ALWAN_CORE_LITERAL(1.3));                                          /* saturation */
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

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_khronos_pbr_neutral)(ALWAN_CORE_VEC3 color) {
    ALWAN_CORE_VEC3 result;
    const ALWAN_CORE_T start_compression = ALWAN_CORE_LITERAL(0.8) - ALWAN_CORE_LITERAL(0.04);
    const ALWAN_CORE_T desaturation = ALWAN_CORE_LITERAL(0.15);

    /* Compute offset from min channel */
    ALWAN_CORE_T x = color.v[0];
    x = ALWAN_CORE_SELECT(color.v[1] < x, color.v[1], x);
    x = ALWAN_CORE_SELECT(color.v[2] < x, color.v[2], x);
    ALWAN_CORE_T offset = ALWAN_CORE_SELECT(x < ALWAN_CORE_LITERAL(0.08),
                                        x - ALWAN_CORE_LITERAL(6.25) * x * x,
                                        ALWAN_CORE_LITERAL(0.04));
    color.v[0] -= offset;
    color.v[1] -= offset;
    color.v[2] -= offset;

    /* Find peak channel */
    ALWAN_CORE_T peak = color.v[0];
    peak = ALWAN_CORE_SELECT(color.v[1] > peak, color.v[1], peak);
    peak = ALWAN_CORE_SELECT(color.v[2] > peak, color.v[2], peak);

    /* Soft rolloff -- branchless: when peak < start_compression, new_peak=peak -> scale=1, g=0 */
    const ALWAN_CORE_T d = ALWAN_CORE_LITERAL(1.0) - start_compression;
    ALWAN_CORE_T safe_peak = ALWAN_CORE_SELECT(peak < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(1e-10), peak);
    ALWAN_CORE_T new_peak = ALWAN_CORE_SELECT(
        peak < start_compression,
        peak,
        ALWAN_CORE_LITERAL(1.0) - d * d / (safe_peak + d - start_compression));
    ALWAN_CORE_T scale = new_peak / safe_peak;
    color.v[0] *= scale;
    color.v[1] *= scale;
    color.v[2] *= scale;

    /* Desaturation -- g=0 when pass-through (safe_peak == new_peak) */
    ALWAN_CORE_T g = ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_LITERAL(1.0) /
                     (desaturation * (safe_peak - new_peak) + ALWAN_CORE_LITERAL(1.0));
    result.v[0] = color.v[0] + (new_peak - color.v[0]) * g;
    result.v[1] = color.v[1] + (new_peak - color.v[1]) * g;
    result.v[2] = color.v[2] + (new_peak - color.v[2]) * g;
    return result;
}

/* ================================================================
 * Reinhard Extended Tone Mapping (Luminance-based)
 *
 * From "Photographic Tone Reproduction for Digital Images"
 * (Reinhard et al., SIGGRAPH 2002), Equation 4.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_reinhard_extended)(ALWAN_CORE_T v,
                                                     ALWAN_CORE_T max_white) {
    ALWAN_CORE_T ww = max_white * max_white;
    return (v * (ALWAN_CORE_ONE + v / ww)) / (ALWAN_CORE_ONE + v);
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_reinhard_extended_luma)(ALWAN_CORE_VEC3 rgb,
                                                        ALWAN_CORE_T max_white) {
    ALWAN_CORE_VEC3 result;
    ALWAN_CORE_T l_old = ALWAN_CORE_LUMA_KR_BT709 * rgb.v[0]
                       + ALWAN_CORE_LUMA_KG_BT709 * rgb.v[1]
                       + ALWAN_CORE_LUMA_KB_BT709 * rgb.v[2];

    /* Branchless: clamp to safe value, zero scale when l_old is black */
    ALWAN_CORE_T safe_l = ALWAN_CORE_SELECT(l_old < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(1e-10), l_old);
    ALWAN_CORE_T l_new = ALWAN_CORE_FNVLIT(alwan_reinhard_extended)(safe_l, max_white);
    ALWAN_CORE_T scale = ALWAN_CORE_SELECT(l_old < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_ZERO, l_new / safe_l);

    result.v[0] = rgb.v[0] * scale;
    result.v[1] = rgb.v[1] * scale;
    result.v[2] = rgb.v[2] * scale;
    return result;
}

/* ================================================================
 * Uchimura / Gran Turismo Tone Mapping
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_uchimura)(ALWAN_CORE_T x,
                                             ALWAN_CORE_T P,
                                             ALWAN_CORE_T a,
                                             ALWAN_CORE_T m,
                                             ALWAN_CORE_T l,
                                             ALWAN_CORE_T c,
                                             ALWAN_CORE_T b) {
    /* Derived constants */
    ALWAN_CORE_T l0 = ((P - m) * l) / a;
    ALWAN_CORE_T S0 = m + l0;
    ALWAN_CORE_T S1 = m + a * l0;
    ALWAN_CORE_T C2 = (a * P) / (P - S1);
    ALWAN_CORE_T CP = -C2 / P;

    /* Smoothstep weights: 3t^2 - 2t^3 */
    ALWAN_CORE_T t_toe = ALWAN_CORE_SATURATE(ALWAN_CORE_SELECT(x < m, ALWAN_CORE_ONE - x / m, ALWAN_CORE_ZERO));
    ALWAN_CORE_T w0 = t_toe * t_toe * (ALWAN_CORE_LITERAL(3.0) - ALWAN_CORE_LITERAL(2.0) * t_toe);
    ALWAN_CORE_T w2 = ALWAN_CORE_SELECT(x > S0, ALWAN_CORE_ONE, ALWAN_CORE_ZERO);
    ALWAN_CORE_T w1 = ALWAN_CORE_ONE - w0 - w2;

    /* Toe: power curve + pedestal */
    ALWAN_CORE_T T = m * ALWAN_CORE_POW(x / m, c) + b;

    /* Linear section */
    ALWAN_CORE_T L = m + a * (x - m);

    /* Shoulder: exponential asymptote to P */
    ALWAN_CORE_T S = P - (P - S1) * ALWAN_CORE_EXP(CP * (x - S0));

    return T * w0 + L * w1 + S * w2;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_uchimura_default)(ALWAN_CORE_T x) {
    return ALWAN_CORE_FNVLIT(alwan_uchimura)(x,
        ALWAN_CORE_ONE,                  /* P = 1.0  */
        ALWAN_CORE_ONE,                  /* a = 1.0  */
        ALWAN_CORE_LITERAL(0.22),        /* m = 0.22 */
        ALWAN_CORE_LITERAL(0.4),         /* l = 0.4  */
        ALWAN_CORE_LITERAL(1.33),        /* c = 1.33 */
        ALWAN_CORE_ZERO);                /* b = 0.0  */
}

/* ================================================================
 * Lottes tone mapping: T. Lottes, "Advanced Techniques and Optimization of
 * HDR Color Pipelines", GDC 2016
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_lottes_curve)(ALWAN_CORE_T x,
                                                 ALWAN_CORE_T a,
                                                 ALWAN_CORE_T d,
                                                 ALWAN_CORE_T b,
                                                 ALWAN_CORE_T c) {
    ALWAN_CORE_T xp = ALWAN_CORE_POW(x, a);
    return xp / (ALWAN_CORE_POW(x, a * d) * b + c);
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_lottes)(ALWAN_CORE_VEC3 rgb,
                                         ALWAN_CORE_T contrast,
                                         ALWAN_CORE_T shoulder,
                                         ALWAN_CORE_T hdr_max,
                                         ALWAN_CORE_T mid_in,
                                         ALWAN_CORE_T mid_out) {
    ALWAN_CORE_VEC3 result;

    /* Precompute b and c from constraints */
    ALWAN_CORE_T pow_hdr_a   = ALWAN_CORE_POW(hdr_max, contrast);
    ALWAN_CORE_T pow_hdr_ad  = ALWAN_CORE_POW(hdr_max, contrast * shoulder);
    ALWAN_CORE_T pow_mid_a   = ALWAN_CORE_POW(mid_in,  contrast);
    ALWAN_CORE_T pow_mid_ad  = ALWAN_CORE_POW(mid_in,  contrast * shoulder);

    ALWAN_CORE_T b = (-pow_mid_a + pow_hdr_a * mid_out)
                   / ((pow_hdr_ad - pow_mid_ad) * mid_out);
    ALWAN_CORE_T c = (pow_hdr_ad * pow_mid_a - pow_hdr_a * pow_mid_ad * mid_out)
                   / ((pow_hdr_ad - pow_mid_ad) * mid_out);

    /* Per channel, as Lottes's curve and the GLSL it is known by (dmnsgn glsl-tone-map
     * lottes.glsl): these constants are that shader's, and they put 0.18 at 0.267 exactly.
     * This once applied the curve to the largest channel and scaled the others by its ratio,
     * which kept greys and moved colours by up to 0.78. */
    result.v[0] = ALWAN_CORE_FNVLIT(alwan_lottes_curve)(rgb.v[0], contrast, shoulder, b, c);
    result.v[1] = ALWAN_CORE_FNVLIT(alwan_lottes_curve)(rgb.v[1], contrast, shoulder, b, c);
    result.v[2] = ALWAN_CORE_FNVLIT(alwan_lottes_curve)(rgb.v[2], contrast, shoulder, b, c);
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_lottes_default)(ALWAN_CORE_VEC3 rgb) {
    return ALWAN_CORE_FNVLIT(alwan_lottes)(rgb,
        ALWAN_CORE_LITERAL(1.6),         /* contrast  */
        ALWAN_CORE_LITERAL(0.977),       /* shoulder  */
        ALWAN_CORE_LITERAL(8.0),         /* hdr_max   */
        ALWAN_CORE_LITERAL(0.18),        /* mid_in    */
        ALWAN_CORE_LITERAL(0.267));      /* mid_out   */
}

