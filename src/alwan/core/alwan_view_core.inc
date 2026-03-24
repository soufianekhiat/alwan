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
    ALWAN_CORE_T const a = ALWAN_CORE_LITERAL(2.51);
    ALWAN_CORE_T const b = ALWAN_CORE_LITERAL(0.03);
    ALWAN_CORE_T const c = ALWAN_CORE_LITERAL(2.43);
    ALWAN_CORE_T const d = ALWAN_CORE_LITERAL(0.59);
    ALWAN_CORE_T const e = ALWAN_CORE_LITERAL(0.14);

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
 * AgX Base Curve - 6th-order polynomial (Blender 4.0 reference)
 * Input: normalized [0,1] from log encoding
 * Output: display [0,1]
 *
 * Coefficients derived from fitting the Blender 4.0 AgX sigmoid
 * using a 6th-order polynomial for improved highlight rolloff
 * and shadow detail compared to the simpler cubic approximation.
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
 * AgX Punchy Grade
 * Applies contrast + saturation boost on top of base curve output
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_agx_punchy_grade)(ALWAN_CORE_VEC3 rgb) {
    ALWAN_CORE_VEC3 result;
    ALWAN_CORE_T const contrast   = ALWAN_CORE_LITERAL(1.15);
    ALWAN_CORE_T const saturation = ALWAN_CORE_LITERAL(1.2);
    ALWAN_CORE_T const mid_gray   = ALWAN_CORE_LITERAL(0.18);

    /* BT.709 luma coefficients — ITU-R BT.709-6 Table 3 */
    ALWAN_CORE_T luma = ALWAN_CORE_LITERAL(0.2126) * rgb.v[0]
                      + ALWAN_CORE_LITERAL(0.7152) * rgb.v[1]
                      + ALWAN_CORE_LITERAL(0.0722) * rgb.v[2];

    result.v[0] = ALWAN_CORE_SATURATE(mid_gray + (luma + (rgb.v[0] - luma) * saturation - mid_gray) * contrast);
    result.v[1] = ALWAN_CORE_SATURATE(mid_gray + (luma + (rgb.v[1] - luma) * saturation - mid_gray) * contrast);
    result.v[2] = ALWAN_CORE_SATURATE(mid_gray + (luma + (rgb.v[2] - luma) * saturation - mid_gray) * contrast);

    return result;
}

/* ================================================================
 * AgX Golden Grade
 *
 * Look transform that warms highlights and cools shadows,
 * inspired by the Blender AgX "Golden" look.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_agx_golden_grade)(ALWAN_CORE_VEC3 rgb) {
    ALWAN_CORE_VEC3 result;

    /* BT.709 luma coefficients — ITU-R BT.709-6 Table 3 */
    ALWAN_CORE_T luma = ALWAN_CORE_LITERAL(0.2126) * rgb.v[0]
                      + ALWAN_CORE_LITERAL(0.7152) * rgb.v[1]
                      + ALWAN_CORE_LITERAL(0.0722) * rgb.v[2];

    /* Golden: warm highlights, cool shadows */
    /* Highlights: shift toward warm (increase R, slight G, decrease B) */
    /* Shadows: shift toward cool (decrease R, slight G, increase B) */
    ALWAN_CORE_T warm_factor = ALWAN_CORE_SATURATE(luma);  /* 0 in shadows, 1 in highlights */

    /* Base saturation boost */
    ALWAN_CORE_T saturation = ALWAN_CORE_LITERAL(1.3);
    ALWAN_CORE_T contrast   = ALWAN_CORE_LITERAL(1.1);
    ALWAN_CORE_T mid_gray   = ALWAN_CORE_LITERAL(0.18);

    /* Apply saturation around luma */
    ALWAN_CORE_T r_sat = luma + (rgb.v[0] - luma) * saturation;
    ALWAN_CORE_T g_sat = luma + (rgb.v[1] - luma) * saturation;
    ALWAN_CORE_T b_sat = luma + (rgb.v[2] - luma) * saturation;

    /* Apply warm/cool color shift */
    ALWAN_CORE_T warm_r = ALWAN_CORE_LITERAL(0.02) * warm_factor;
    ALWAN_CORE_T cool_b = ALWAN_CORE_LITERAL(0.02) * (ALWAN_CORE_ONE - warm_factor);

    r_sat = r_sat + warm_r - cool_b * ALWAN_CORE_LITERAL(0.5);
    b_sat = b_sat + cool_b - warm_r * ALWAN_CORE_LITERAL(0.5);

    /* Apply contrast */
    result.v[0] = ALWAN_CORE_SATURATE(mid_gray + (r_sat - mid_gray) * contrast);
    result.v[1] = ALWAN_CORE_SATURATE(mid_gray + (g_sat - mid_gray) * contrast);
    result.v[2] = ALWAN_CORE_SATURATE(mid_gray + (b_sat - mid_gray) * contrast);

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

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_khronos_pbr_neutral)(ALWAN_CORE_VEC3 color) {
    ALWAN_CORE_VEC3 result;
    ALWAN_CORE_T const start_compression = ALWAN_CORE_LITERAL(0.8) - ALWAN_CORE_LITERAL(0.04);
    ALWAN_CORE_T const desaturation = ALWAN_CORE_LITERAL(0.15);

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

    /* Below start_compression: pass-through */
    if (peak < start_compression) {
        return color;
    }

    /* Soft rolloff */
    {
        ALWAN_CORE_T const d = ALWAN_CORE_LITERAL(1.0) - start_compression;
        ALWAN_CORE_T new_peak = ALWAN_CORE_LITERAL(1.0) - d * d / (peak + d - start_compression);
        ALWAN_CORE_T scale = new_peak / peak;
        color.v[0] *= scale;
        color.v[1] *= scale;
        color.v[2] *= scale;

        /* Desaturation */
        ALWAN_CORE_T g = ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_LITERAL(1.0) /
                         (desaturation * (peak - new_peak) + ALWAN_CORE_LITERAL(1.0));
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
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_reinhard_extended)(ALWAN_CORE_T v,
                                                     ALWAN_CORE_T max_white) {
    ALWAN_CORE_T ww = max_white * max_white;
    return (v * (ALWAN_CORE_ONE + v / ww)) / (ALWAN_CORE_ONE + v);
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_reinhard_extended_luma)(ALWAN_CORE_VEC3 rgb,
                                                        ALWAN_CORE_T max_white) {
    ALWAN_CORE_VEC3 result;
    ALWAN_CORE_T l_old = ALWAN_CORE_LITERAL(0.2126) * rgb.v[0]
                       + ALWAN_CORE_LITERAL(0.7152) * rgb.v[1]
                       + ALWAN_CORE_LITERAL(0.0722) * rgb.v[2];

    if (l_old < ALWAN_CORE_LITERAL(1e-10)) {
        result.v[0] = ALWAN_CORE_ZERO;
        result.v[1] = ALWAN_CORE_ZERO;
        result.v[2] = ALWAN_CORE_ZERO;
        return result;
    }

    ALWAN_CORE_T l_new = ALWAN_CORE_FNVLIT(alwan_reinhard_extended)(l_old, max_white);
    ALWAN_CORE_T scale = l_new / l_old;

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
    ALWAN_CORE_T t_toe = ALWAN_CORE_SATURATE((x < m) ? (ALWAN_CORE_ONE - x / m) : ALWAN_CORE_ZERO);
    ALWAN_CORE_T w0 = t_toe * t_toe * (ALWAN_CORE_LITERAL(3.0) - ALWAN_CORE_LITERAL(2.0) * t_toe);
    ALWAN_CORE_T w2 = (x > S0) ? ALWAN_CORE_ONE : ALWAN_CORE_ZERO;
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
 * Lottes / AMD Cauldron Tone Mapping
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

    /* Apply curve to peak channel, scale others proportionally */
    ALWAN_CORE_T peak = ALWAN_CORE_MAX3(rgb.v[0], rgb.v[1], rgb.v[2]);

    if (peak < ALWAN_CORE_LITERAL(1e-10)) {
        result.v[0] = ALWAN_CORE_ZERO;
        result.v[1] = ALWAN_CORE_ZERO;
        result.v[2] = ALWAN_CORE_ZERO;
        return result;
    }

    ALWAN_CORE_T new_peak = ALWAN_CORE_FNVLIT(alwan_lottes_curve)(peak, contrast, shoulder, b, c);
    ALWAN_CORE_T scale = new_peak / peak;

    result.v[0] = rgb.v[0] * scale;
    result.v[1] = rgb.v[1] * scale;
    result.v[2] = rgb.v[2] * scale;
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

/* ================================================================
 * "Somewhat Boring Display Transform" (Stachowiak, 2023)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_tony_curve)(ALWAN_CORE_T v) {
    return ALWAN_CORE_ONE - ALWAN_CORE_EXP(-v);
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_tony_mcmapface)(ALWAN_CORE_VEC3 col) {
    ALWAN_CORE_VEC3 result;

    /* BT.709 luma */
    ALWAN_CORE_T luma = ALWAN_CORE_LITERAL(0.2126) * col.v[0]
                      + ALWAN_CORE_LITERAL(0.7152) * col.v[1]
                      + ALWAN_CORE_LITERAL(0.0722) * col.v[2];

    /* BT.709 Cb, Cr (simplified, not full-range) */
    ALWAN_CORE_T cb = col.v[2] - luma;
    ALWAN_CORE_T cr = col.v[0] - luma;

    /* Chroma magnitude drives desaturation */
    ALWAN_CORE_T chroma_len = ALWAN_CORE_SQRT(cb * cb + cr * cr);
    ALWAN_CORE_T bt = ALWAN_CORE_FNVLIT(alwan_tony_curve)(chroma_len * ALWAN_CORE_LITERAL(2.4));
    ALWAN_CORE_T desat = ALWAN_CORE_MAX(
        (bt - ALWAN_CORE_LITERAL(0.7)) * ALWAN_CORE_LITERAL(0.8), ALWAN_CORE_ZERO);
    desat = desat * desat;

    /* Desaturate toward luma */
    ALWAN_CORE_T desat_r = col.v[0] + (luma - col.v[0]) * desat;
    ALWAN_CORE_T desat_g = col.v[1] + (luma - col.v[1]) * desat;
    ALWAN_CORE_T desat_b = col.v[2] + (luma - col.v[2]) * desat;

    /* Tone-map luminance */
    ALWAN_CORE_T tm_luma = ALWAN_CORE_FNVLIT(alwan_tony_curve)(luma);

    /* Luminance-preserving scale of original color */
    ALWAN_CORE_T orig_luma = ALWAN_CORE_LITERAL(0.2126) * col.v[0]
                           + ALWAN_CORE_LITERAL(0.7152) * col.v[1]
                           + ALWAN_CORE_LITERAL(0.0722) * col.v[2];
    ALWAN_CORE_T luma_scale = (orig_luma > ALWAN_CORE_LITERAL(1e-5))
                            ? (tm_luma / orig_luma) : ALWAN_CORE_ZERO;

    ALWAN_CORE_T tm0_r = col.v[0] * luma_scale;
    ALWAN_CORE_T tm0_g = col.v[1] * luma_scale;
    ALWAN_CORE_T tm0_b = col.v[2] * luma_scale;

    /* Per-channel tone map of desaturated color */
    ALWAN_CORE_T tm1_r = ALWAN_CORE_FNVLIT(alwan_tony_curve)(desat_r);
    ALWAN_CORE_T tm1_g = ALWAN_CORE_FNVLIT(alwan_tony_curve)(desat_g);
    ALWAN_CORE_T tm1_b = ALWAN_CORE_FNVLIT(alwan_tony_curve)(desat_b);

    /* Blend between luminance-preserving and per-channel based on bt */
    ALWAN_CORE_T bt2 = bt * bt;
    ALWAN_CORE_T final_mult = ALWAN_CORE_LITERAL(0.97);

    result.v[0] = (tm0_r + (tm1_r - tm0_r) * bt2) * final_mult;
    result.v[1] = (tm0_g + (tm1_g - tm0_g) * bt2) * final_mult;
    result.v[2] = (tm0_b + (tm1_b - tm0_b) * bt2) * final_mult;

    return result;
}
