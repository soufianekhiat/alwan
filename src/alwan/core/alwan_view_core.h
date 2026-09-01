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

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_view_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_view_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_aces_tonemap_v(alwan_scalar x) {
    const alwan_scalar a = ALWAN_LITERAL(2.51);
    const alwan_scalar b = ALWAN_LITERAL(0.03);
    const alwan_scalar c = ALWAN_LITERAL(2.43);
    const alwan_scalar d = ALWAN_LITERAL(0.59);
    const alwan_scalar e = ALWAN_LITERAL(0.14);

    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

ALWAN_INLINE alwan_scalar alwan_agx_log_encode_v(alwan_scalar x,
                                                   alwan_scalar min_ev,
                                                   alwan_scalar max_ev) {
    alwan_scalar clamped = ALWAN_SELECT(x < ALWAN_LITERAL(1e-10),
                                         ALWAN_LITERAL(1e-10), x);
    alwan_scalar log_x = ALWAN_LOG2(clamped);
    alwan_scalar normalized = (log_x - min_ev) / (max_ev - min_ev);
    return alwan_saturate(normalized);
}

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

/* ASC CDL: slope, offset, power, then saturation around BT.709 luma. The GPU
 * mirror of the .inc definition; both graders below call it, so without it this
 * core names an undefined function and cannot compile as a shader. */
ALWAN_INLINE alwan_vec3 alwan_cdl_apply_v(
    alwan_vec3 rgb,
    alwan_scalar slope_r, alwan_scalar slope_g, alwan_scalar slope_b,
    alwan_scalar offset_r, alwan_scalar offset_g, alwan_scalar offset_b,
    alwan_scalar power_r, alwan_scalar power_g, alwan_scalar power_b,
    alwan_scalar sat) {
    alwan_vec3 result;
    /* Slope + Offset + Clamp */
    alwan_scalar r = alwan_saturate(rgb.v[0] * slope_r + offset_r);
    alwan_scalar g = alwan_saturate(rgb.v[1] * slope_g + offset_g);
    alwan_scalar b = alwan_saturate(rgb.v[2] * slope_b + offset_b);
    /* Power */
    r = ALWAN_POW(r, power_r);
    g = ALWAN_POW(g, power_g);
    b = ALWAN_POW(b, power_b);
    /* Saturation around BT.709 luma */
    alwan_scalar luma = ALWAN_LUMA_KR_BT709 * r
                      + ALWAN_LUMA_KG_BT709 * g
                      + ALWAN_LUMA_KB_BT709 * b;
    result.v[0] = alwan_saturate(luma + sat * (r - luma));
    result.v[1] = alwan_saturate(luma + sat * (g - luma));
    result.v[2] = alwan_saturate(luma + sat * (b - luma));
    return result;
}

ALWAN_INLINE alwan_vec3 alwan_agx_punchy_grade_v(alwan_vec3 rgb) {
    return alwan_cdl_apply_v(rgb,
        ALWAN_ONE, ALWAN_ONE, ALWAN_ONE,           /* slope = 1 */
        ALWAN_ZERO, ALWAN_ZERO, ALWAN_ZERO,       /* offset = 0 */
        ALWAN_LITERAL(1.35), ALWAN_LITERAL(1.35), ALWAN_LITERAL(1.35), /* power */
        ALWAN_LITERAL(1.4));                                  /* saturation */
}

ALWAN_INLINE alwan_vec3 alwan_agx_golden_grade_v(alwan_vec3 rgb) {
    return alwan_cdl_apply_v(rgb,
        ALWAN_ONE, ALWAN_LITERAL(0.9), ALWAN_LITERAL(0.5), /* slope */
        ALWAN_ZERO, ALWAN_ZERO, ALWAN_ZERO,               /* offset = 0 */
        ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.8), /* power */
        ALWAN_LITERAL(1.3));                                          /* saturation */
}

ALWAN_INLINE alwan_vec3 alwan_khronos_pbr_neutral_v(alwan_vec3 color) {
    alwan_vec3 result;
    const alwan_scalar start_compression = ALWAN_LITERAL(0.8) - ALWAN_LITERAL(0.04);
    const alwan_scalar desaturation = ALWAN_LITERAL(0.15);

    alwan_scalar x = color.v[0];
    x = ALWAN_SELECT(color.v[1] < x, color.v[1], x);
    x = ALWAN_SELECT(color.v[2] < x, color.v[2], x);
    alwan_scalar offset = ALWAN_SELECT(x < ALWAN_LITERAL(0.08),
                                        x - ALWAN_LITERAL(6.25) * x * x,
                                        ALWAN_LITERAL(0.04));
    color.v[0] -= offset;
    color.v[1] -= offset;
    color.v[2] -= offset;

    alwan_scalar peak = color.v[0];
    peak = ALWAN_SELECT(color.v[1] > peak, color.v[1], peak);
    peak = ALWAN_SELECT(color.v[2] > peak, color.v[2], peak);

    const alwan_scalar d = ALWAN_LITERAL(1.0) - start_compression;
    alwan_scalar safe_peak = ALWAN_SELECT(peak < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), peak);
    alwan_scalar new_peak = ALWAN_SELECT(
        peak < start_compression,
        peak,
        ALWAN_LITERAL(1.0) - d * d / (safe_peak + d - start_compression));
    alwan_scalar scale = new_peak / safe_peak;
    color.v[0] *= scale;
    color.v[1] *= scale;
    color.v[2] *= scale;

    alwan_scalar g = ALWAN_LITERAL(1.0) - ALWAN_LITERAL(1.0) /
                     (desaturation * (safe_peak - new_peak) + ALWAN_LITERAL(1.0));
    result.v[0] = color.v[0] + (new_peak - color.v[0]) * g;
    result.v[1] = color.v[1] + (new_peak - color.v[1]) * g;
    result.v[2] = color.v[2] + (new_peak - color.v[2]) * g;
    return result;
}

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

    alwan_scalar safe_l = ALWAN_SELECT(l_old < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), l_old);
    alwan_scalar l_new = alwan_reinhard_extended_v(safe_l, max_white);
    alwan_scalar scale = ALWAN_SELECT(l_old < ALWAN_LITERAL(1e-10), ALWAN_ZERO, l_new / safe_l);

    result.v[0] = rgb.v[0] * scale;
    result.v[1] = rgb.v[1] * scale;
    result.v[2] = rgb.v[2] * scale;
    return result;
}

ALWAN_INLINE alwan_scalar alwan_uchimura_v(alwan_scalar x,
                                             alwan_scalar P,
                                             alwan_scalar a,
                                             alwan_scalar m,
                                             alwan_scalar l,
                                             alwan_scalar c,
                                             alwan_scalar b) {
    alwan_scalar l0 = ((P - m) * l) / a;
    alwan_scalar S0 = m + l0;
    alwan_scalar S1 = m + a * l0;
    alwan_scalar C2 = (a * P) / (P - S1);
    alwan_scalar CP = -C2 / P;

    alwan_scalar t_toe = alwan_saturate(ALWAN_SELECT(x < m, ALWAN_ONE - x / m, ALWAN_ZERO));
    alwan_scalar w0 = t_toe * t_toe * (ALWAN_LITERAL(3.0) - ALWAN_LITERAL(2.0) * t_toe);
    alwan_scalar w2 = ALWAN_SELECT(x > S0, ALWAN_ONE, ALWAN_ZERO);
    alwan_scalar w1 = ALWAN_ONE - w0 - w2;

    alwan_scalar T = m * ALWAN_POW(x / m, c) + b;
    alwan_scalar L = m + a * (x - m);
    alwan_scalar S = P - (P - S1) * ALWAN_EXP(CP * (x - S0));

    return T * w0 + L * w1 + S * w2;
}

ALWAN_INLINE alwan_scalar alwan_uchimura_default_v(alwan_scalar x) {
    return alwan_uchimura_v(x,
        ALWAN_ONE, ALWAN_ONE, ALWAN_LITERAL(0.22),
        ALWAN_LITERAL(0.4), ALWAN_LITERAL(1.33), ALWAN_ZERO);
}

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

    alwan_scalar pow_hdr_a   = ALWAN_POW(hdr_max, contrast);
    alwan_scalar pow_hdr_ad  = ALWAN_POW(hdr_max, contrast * shoulder);
    alwan_scalar pow_mid_a   = ALWAN_POW(mid_in,  contrast);
    alwan_scalar pow_mid_ad  = ALWAN_POW(mid_in,  contrast * shoulder);

    alwan_scalar b = (-pow_mid_a + pow_hdr_a * mid_out)
                   / ((pow_hdr_ad - pow_mid_ad) * mid_out);
    alwan_scalar c = (pow_hdr_ad * pow_mid_a - pow_hdr_a * pow_mid_ad * mid_out)
                   / ((pow_hdr_ad - pow_mid_ad) * mid_out);

    alwan_scalar peak = alwan_max3(rgb.v[0], rgb.v[1], rgb.v[2]);

    alwan_scalar safe_peak = ALWAN_SELECT(peak < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), peak);
    alwan_scalar new_peak = alwan_lottes_curve_v(safe_peak, contrast, shoulder, b, c);
    alwan_scalar scale = ALWAN_SELECT(peak < ALWAN_LITERAL(1e-10), ALWAN_ZERO, new_peak / safe_peak);

    result.v[0] = rgb.v[0] * scale;
    result.v[1] = rgb.v[1] * scale;
    result.v[2] = rgb.v[2] * scale;
    return result;
}

ALWAN_INLINE alwan_vec3 alwan_lottes_default_v(alwan_vec3 rgb) {
    return alwan_lottes_v(rgb,
        ALWAN_LITERAL(1.6), ALWAN_LITERAL(0.977),
        ALWAN_LITERAL(8.0), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.267));
}

ALWAN_INLINE alwan_scalar alwan_tony_curve_v(alwan_scalar v) {
    return ALWAN_ONE - ALWAN_EXP(-v);
}

ALWAN_INLINE alwan_vec3 alwan_tony_mcmapface_v(alwan_vec3 col) {
    alwan_vec3 result;

    alwan_scalar luma = ALWAN_LUMA_KR_BT709 * col.v[0]
                      + ALWAN_LUMA_KG_BT709 * col.v[1]
                      + ALWAN_LUMA_KB_BT709 * col.v[2];

    alwan_scalar cb = col.v[2] - luma;
    alwan_scalar cr = col.v[0] - luma;

    alwan_scalar chroma_len = ALWAN_SQRT(cb * cb + cr * cr);
    alwan_scalar bt = alwan_tony_curve_v(chroma_len * ALWAN_LITERAL(2.4));
    alwan_scalar desat = alwan_max(
        (bt - ALWAN_LITERAL(0.7)) * ALWAN_LITERAL(0.8), ALWAN_ZERO);
    desat = desat * desat;

    alwan_scalar desat_r = col.v[0] + (luma - col.v[0]) * desat;
    alwan_scalar desat_g = col.v[1] + (luma - col.v[1]) * desat;
    alwan_scalar desat_b = col.v[2] + (luma - col.v[2]) * desat;

    alwan_scalar tm_luma = alwan_tony_curve_v(luma);

    alwan_scalar luma_scale = ALWAN_SELECT(luma > ALWAN_LITERAL(1e-5),
                                           tm_luma / luma, ALWAN_ZERO);

    alwan_scalar tm0_r = col.v[0] * luma_scale;
    alwan_scalar tm0_g = col.v[1] * luma_scale;
    alwan_scalar tm0_b = col.v[2] * luma_scale;

    alwan_scalar tm1_r = alwan_tony_curve_v(desat_r);
    alwan_scalar tm1_g = alwan_tony_curve_v(desat_g);
    alwan_scalar tm1_b = alwan_tony_curve_v(desat_b);

    alwan_scalar bt2 = bt * bt;
    alwan_scalar final_mult = ALWAN_LITERAL(0.97); /* Stachowiak 2023 "Somewhat Boring Display Transform" -- empirical rolloff */

    result.v[0] = (tm0_r + (tm1_r - tm0_r) * bt2) * final_mult;
    result.v[1] = (tm0_g + (tm1_g - tm0_g) * bt2) * final_mult;
    result.v[2] = (tm0_b + (tm1_b - tm0_b) * bt2) * final_mult;

    return result;
}

#endif

#endif /* ALWAN_VIEW_CORE_H */
