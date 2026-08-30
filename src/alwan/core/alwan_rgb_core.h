/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only RGB Transfer Functions
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Basic transfer functions (sRGB, BT.2020, PQ, HLG, BT.1886, CIE Lab f)
 * are in alwan_core.h. This header adds extended / camera-manufacturer
 * transfer functions.
 */

#ifndef ALWAN_RGB_CORE_H
#define ALWAN_RGB_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_core.h"
#include "alwan_math_core.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_rgb_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_rgb_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_INLINE alwan_vec3 alwan_xy_to_xyz_v(alwan_scalar x, alwan_scalar y) {
    alwan_vec3 result;
    alwan_scalar abs_y = ALWAN_ABS(y);
    alwan_scalar safe_y = ALWAN_SELECT(abs_y < ALWAN_EPSILON, ALWAN_ONE, y);
    alwan_scalar Y = ALWAN_ONE;

    result.v[0] = ALWAN_SELECT(abs_y < ALWAN_EPSILON, ALWAN_ZERO, (x / safe_y) * Y);
    result.v[1] = ALWAN_SELECT(abs_y < ALWAN_EPSILON, ALWAN_ZERO, Y);
    result.v[2] = ALWAN_SELECT(abs_y < ALWAN_EPSILON, ALWAN_ZERO,
                               ((ALWAN_ONE - x - y) / safe_y) * Y);
    return result;
}

typedef struct {
    alwan_mat3x3 rgb_to_xyz;
    alwan_mat3x3 xyz_to_rgb;
} alwan_rgb_matrices;

ALWAN_INLINE alwan_rgb_matrices alwan_rgb_derive_matrices_v(
    alwan_scalar rx, alwan_scalar ry,
    alwan_scalar gx, alwan_scalar gy,
    alwan_scalar bx, alwan_scalar by,
    alwan_scalar wx, alwan_scalar wy)
{
    alwan_rgb_matrices result;

    alwan_mat3x3 M;
    M.m[0] = rx; M.m[1] = gx; M.m[2] = bx;
    M.m[3] = ry; M.m[4] = gy; M.m[5] = by;
    M.m[6] = ALWAN_ONE - rx - ry;
    M.m[7] = ALWAN_ONE - gx - gy;
    M.m[8] = ALWAN_ONE - bx - by;

    alwan_vec3 W_XYZ = alwan_xy_to_xyz_v(wx, wy);

    alwan_mat3x3 M_inv = alwan_mat3_inv_v(M);
    alwan_vec3 S = alwan_mat3_mulv_v(M_inv, W_XYZ);

    result.rgb_to_xyz.m[0] = M.m[0] * S.v[0];
    result.rgb_to_xyz.m[1] = M.m[1] * S.v[1];
    result.rgb_to_xyz.m[2] = M.m[2] * S.v[2];
    result.rgb_to_xyz.m[3] = M.m[3] * S.v[0];
    result.rgb_to_xyz.m[4] = M.m[4] * S.v[1];
    result.rgb_to_xyz.m[5] = M.m[5] * S.v[2];
    result.rgb_to_xyz.m[6] = M.m[6] * S.v[0];
    result.rgb_to_xyz.m[7] = M.m[7] * S.v[1];
    result.rgb_to_xyz.m[8] = M.m[8] * S.v[2];

    result.xyz_to_rgb = alwan_mat3_inv_v(result.rgb_to_xyz);

    return result;
}

/* Legal 10-bit SDI range: luma code values [64, 940] -> [0.0, 1.0]
 * per SMPTE ST 274:2008 / ITU-R BT.709-6 Annex B.
 * 64/1023 ~ 0.062561, (940-64)/1023 ~ 0.856305 */
ALWAN_INLINE alwan_scalar alwan_legal_to_full_10bit(alwan_scalar legal) {
    return (legal - ALWAN_LITERAL(0.062561094819159)) / ALWAN_LITERAL(0.856304985337243);
}

ALWAN_INLINE alwan_scalar alwan_full_to_legal_10bit(alwan_scalar full) {
    return full * ALWAN_LITERAL(0.856304985337243) + ALWAN_LITERAL(0.062561094819159);
}

/* ACESproxy -- ACES S-2013-001 "ACESproxy -- An Integer Log Encoding of ACES
 * Image Data".
 *
 * The spec's final step rounds the code value to an integer. That step is left
 * to the caller, so this stays a continuous float transfer function and the
 * EOTF remains an exact inverse of the OETF. Everything else is normative and
 * applied here: the 2^-9.72 floor, the [CV_min, CV_max] clamp, and
 * mid_log_offset = 2.5. 10-bit constants.
 *
 * The visible consequence, which is deliberate: at linear 0.18 the code value
 * is 426.30344, so this returns 426.30344 / 1023 = 0.4167189 where a rounding
 * implementation (colour-science, and any integer ACESproxy encoder) returns
 * 426 / 1023 = 0.4164223. The gap is 2.97e-04 and is bounded by half a code
 * value across the whole range. Round the result yourself if you need bit-exact
 * ACESproxy code values. Do not quantise inside the OETF: that turns the curve
 * into a staircase and breaks EOTF(OETF(x)) == x for the float and GPU paths.
 */
ALWAN_INLINE alwan_scalar alwan_acesproxy_oetf(alwan_scalar lin) {
    alwan_scalar cv_min         = ALWAN_LITERAL(64.0);
    alwan_scalar cv_max         = ALWAN_LITERAL(940.0);
    alwan_scalar mid_cv_offset  = ALWAN_LITERAL(425.0);
    alwan_scalar mid_log_offset = ALWAN_LITERAL(2.5);
    alwan_scalar steps_per_stop = ALWAN_LITERAL(50.0);
    alwan_scalar lin_cut        = ALWAN_LITERAL(0.0011857371917920374);  /* 2^-9.72 */
    
    alwan_scalar arg    = ALWAN_SELECT(lin <= ALWAN_ZERO, ALWAN_LITERAL(1e-10), lin);
    alwan_scalar cv_raw = ALWAN_ROUND(
        (ALWAN_LOG2(arg) + mid_log_offset) * steps_per_stop + mid_cv_offset);
    alwan_scalar cv_lo  = ALWAN_SELECT(cv_raw < cv_min, cv_min, cv_raw);
    alwan_scalar cv     = ALWAN_SELECT(cv_lo > cv_max, cv_max, cv_lo);
    return ALWAN_SELECT(lin > lin_cut, cv, cv_min) / ALWAN_LITERAL(1023.0);
}

ALWAN_INLINE alwan_scalar alwan_acesproxy_eotf(alwan_scalar encoded) {
    alwan_scalar mid_cv_offset  = ALWAN_LITERAL(425.0);
    alwan_scalar mid_log_offset = ALWAN_LITERAL(2.5);
    alwan_scalar steps_per_stop = ALWAN_LITERAL(50.0);
    
    alwan_scalar cv = encoded * ALWAN_LITERAL(1023.0);
    return ALWAN_POW(ALWAN_LITERAL(2.0), (cv - mid_cv_offset) / steps_per_stop - mid_log_offset);
}

/* ACEScc -- ACES S-2014-003 "ACEScc -- A Logarithmic Encoding of ACES Data for use within Color Grading Systems" */
ALWAN_INLINE alwan_scalar alwan_acescc_oetf(alwan_scalar lin) {
    alwan_scalar min_cutoff = ALWAN_LITERAL(0.00003051757812);
    alwan_scalar log2_e     = ALWAN_LITERAL(1.4426950408889634);

    alwan_scalar neg_result  = (ALWAN_LITERAL(-16.0) + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);
    alwan_scalar val         = ALWAN_LITERAL(0.0000152587890625) + lin * ALWAN_LITERAL(0.5);
    alwan_scalar low_result  = (ALWAN_LN(val) * log2_e + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);
    alwan_scalar high_result = (ALWAN_LN(lin) * log2_e + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);

    return ALWAN_SELECT(lin <= ALWAN_ZERO, neg_result,
           ALWAN_SELECT(lin < min_cutoff, low_result, high_result));
}

ALWAN_INLINE alwan_scalar alwan_acescc_eotf(alwan_scalar encoded) {
    alwan_scalar log2_e     = ALWAN_LITERAL(1.4426950408889634);
    alwan_scalar neg_cutoff = (ALWAN_LITERAL(9.72) - ALWAN_LITERAL(15.0)) / ALWAN_LITERAL(17.52);
    alwan_scalar max_cutoff = (ALWAN_LN(ALWAN_LITERAL(65504.0)) * log2_e + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);

    alwan_scalar neg_result = (ALWAN_POW(ALWAN_LITERAL(2.0), encoded * ALWAN_LITERAL(17.52) - ALWAN_LITERAL(9.72)) - ALWAN_LITERAL(0.0000152587890625)) * ALWAN_LITERAL(2.0);
    alwan_scalar mid_result = ALWAN_POW(ALWAN_LITERAL(2.0), encoded * ALWAN_LITERAL(17.52) - ALWAN_LITERAL(9.72));
    alwan_scalar max_result = ALWAN_LITERAL(65504.0);

    return ALWAN_SELECT(encoded < neg_cutoff, neg_result,
           ALWAN_SELECT(encoded < max_cutoff, mid_result, max_result));
}

/* ACEScct -- ACES S-2016-001 "ACEScct -- A Quasi-Logarithmic Encoding of ACES Data for use within Color Grading Systems" */
ALWAN_INLINE alwan_scalar alwan_acescct_oetf(alwan_scalar lin) {
    alwan_scalar cut   = ALWAN_LITERAL(0.0078125);
    alwan_scalar A     = ALWAN_LITERAL(10.5402377416545);
    alwan_scalar B     = ALWAN_LITERAL(0.0729055341958355);
    alwan_scalar log2_e = ALWAN_LITERAL(1.4426950408889634);

    alwan_scalar linear_result = A * lin + B;
    alwan_scalar log_result    = (ALWAN_LN(lin) * log2_e + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);
    return ALWAN_SELECT(lin <= cut, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_acescct_eotf(alwan_scalar encoded) {
    alwan_scalar cut_enc = ALWAN_LITERAL(0.155251141552511);
    alwan_scalar A       = ALWAN_LITERAL(10.5402377416545);
    alwan_scalar B       = ALWAN_LITERAL(0.0729055341958355);

    alwan_scalar linear_result = (encoded - B) / A;
    alwan_scalar log_result    = ALWAN_POW(ALWAN_LITERAL(2.0), encoded * ALWAN_LITERAL(17.52) - ALWAN_LITERAL(9.72));
    return ALWAN_SELECT(encoded <= cut_enc, linear_result, log_result);
}

/* S-Log -- Sony "S-Log: A new LUT for digital production mastering and interchange applications" (2009) */
ALWAN_INLINE alwan_scalar alwan_slog_oetf(alwan_scalar lin) {
    alwan_scalar x = lin / ALWAN_LITERAL(0.9);
    alwan_scalar log_result    = ALWAN_LITERAL(0.432699) * ALWAN_LOG10(x + ALWAN_LITERAL(0.037584)) + ALWAN_LITERAL(0.616596) + ALWAN_LITERAL(0.03);
    alwan_scalar linear_result = x * ALWAN_LITERAL(5.0) + ALWAN_LITERAL(0.030001222851889303);
    alwan_scalar y_full = ALWAN_SELECT(x >= ALWAN_ZERO, log_result, linear_result);
    return alwan_full_to_legal_10bit(y_full);
}

ALWAN_INLINE alwan_scalar alwan_slog_eotf(alwan_scalar encoded) {
    alwan_scalar y_full    = alwan_legal_to_full_10bit(encoded);
    alwan_scalar threshold = ALWAN_LITERAL(0.030001222851889303);
    alwan_scalar log_result    = ALWAN_POW(ALWAN_LITERAL(10.0), (y_full - ALWAN_LITERAL(0.646596)) / ALWAN_LITERAL(0.432699)) - ALWAN_LITERAL(0.037584);
    alwan_scalar linear_result = (y_full - ALWAN_LITERAL(0.030001222851889303)) / ALWAN_LITERAL(5.0);
    alwan_scalar x = ALWAN_SELECT(y_full >= threshold, log_result, linear_result);
    return x * ALWAN_LITERAL(0.9);
}

/* S-Log2 -- Sony "S-Log2 Technical Paper" (2012); same form as S-Log with different signal scaling */
ALWAN_INLINE alwan_scalar alwan_slog2_oetf(alwan_scalar lin) {
    return alwan_slog_oetf(lin * ALWAN_LITERAL(155.0) / ALWAN_LITERAL(219.0));
}

ALWAN_INLINE alwan_scalar alwan_slog2_eotf(alwan_scalar encoded) {
    return alwan_slog_eotf(encoded) * ALWAN_LITERAL(219.0) / ALWAN_LITERAL(155.0);
}

/* S-Log3 -- Sony "Technical Summary for S-Gamut3.Cine/S-Log3 and S-Gamut3/S-Log3" (2014) */
ALWAN_INLINE alwan_scalar alwan_slog3_oetf(alwan_scalar lin) {
    /* No pre-clamp: negative inputs use the linear segment (L < 0.01125000),
     * giving a negative encoded output that callers can clip as needed. */
    alwan_scalar log_result    = (ALWAN_LITERAL(420.0) + ALWAN_LOG10((lin + ALWAN_LITERAL(0.01)) / ALWAN_LITERAL(0.19)) * ALWAN_LITERAL(261.5)) / ALWAN_LITERAL(1023.0);
    alwan_scalar linear_result = (lin * (ALWAN_LITERAL(171.2102946929) - ALWAN_LITERAL(95.0)) / ALWAN_LITERAL(0.01125000) + ALWAN_LITERAL(95.0)) / ALWAN_LITERAL(1023.0);
    return ALWAN_SELECT(lin >= ALWAN_LITERAL(0.01125000), log_result, linear_result);
}

ALWAN_INLINE alwan_scalar alwan_slog3_eotf(alwan_scalar encoded) {
    alwan_scalar code = encoded * ALWAN_LITERAL(1023.0);
    alwan_scalar log_result    = (ALWAN_POW(ALWAN_LITERAL(10.0), ((code - ALWAN_LITERAL(420.0)) / ALWAN_LITERAL(261.5))) * ALWAN_LITERAL(0.19)) - ALWAN_LITERAL(0.01);
    alwan_scalar linear_result = ((code - ALWAN_LITERAL(95.0)) / (ALWAN_LITERAL(171.2102946929) - ALWAN_LITERAL(95.0))) * ALWAN_LITERAL(0.01125000);
    return ALWAN_SELECT(code >= ALWAN_LITERAL(171.2102946929), log_result, linear_result);
}

/* C-Log -- Canon "EOS C300 Instruction Manual" / "Canon Log Transfer Characteristic" (2012) */
ALWAN_INLINE alwan_scalar alwan_clog_oetf(alwan_scalar lin) {
    alwan_scalar a = ALWAN_LITERAL(0.45310179);
    alwan_scalar k = ALWAN_LITERAL(10.1596);
    alwan_scalar offset = ALWAN_LITERAL(0.12512248);
    alwan_scalar L = ALWAN_SELECT(lin < ALWAN_ZERO, ALWAN_ZERO, lin);
    alwan_scalar x = L / ALWAN_LITERAL(0.9);
    return a * ALWAN_LOG10(k * x + ALWAN_ONE) + offset;
}

ALWAN_INLINE alwan_scalar alwan_clog_eotf(alwan_scalar encoded) {
    alwan_scalar a = ALWAN_LITERAL(0.45310179);
    alwan_scalar k = ALWAN_LITERAL(10.1596);
    alwan_scalar offset = ALWAN_LITERAL(0.12512248);
    alwan_scalar x = (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - offset) / a) - ALWAN_ONE) / k;
    return x * ALWAN_LITERAL(0.9);
}

/* C-Log2 -- Canon "Canon Log 2 Transfer Characteristic" (2015); extended dynamic range variant */
ALWAN_INLINE alwan_scalar alwan_clog2_oetf(alwan_scalar lin) {
    alwan_scalar a = ALWAN_LITERAL(0.24136077);
    alwan_scalar k = ALWAN_LITERAL(87.09937546);
    alwan_scalar offset = ALWAN_LITERAL(0.092864125);
    alwan_scalar L = ALWAN_SELECT(lin < ALWAN_ZERO, ALWAN_ZERO, lin);
    alwan_scalar x = L / ALWAN_LITERAL(0.9);
    return a * ALWAN_LOG10(k * x + ALWAN_ONE) + offset;
}

ALWAN_INLINE alwan_scalar alwan_clog2_eotf(alwan_scalar encoded) {
    alwan_scalar a = ALWAN_LITERAL(0.24136077);
    alwan_scalar k = ALWAN_LITERAL(87.09937546);
    alwan_scalar offset = ALWAN_LITERAL(0.092864125);
    alwan_scalar x = (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - offset) / a) - ALWAN_ONE) / k;
    return x * ALWAN_LITERAL(0.9);
}

/* C-Log3 -- Canon "Canon Log 3 Transfer Characteristic" (2018); symmetric around 0 for negative values */
ALWAN_INLINE alwan_scalar alwan_clog3_oetf(alwan_scalar lin) {
    alwan_scalar x = lin / ALWAN_LITERAL(0.9);
    alwan_scalar x_threshold_low  = ALWAN_LITERAL(-0.014);
    alwan_scalar x_threshold_high = ALWAN_LITERAL(0.014);
    alwan_scalar neg_result    = ALWAN_LITERAL(-0.36726845) * ALWAN_LOG10(-x * ALWAN_LITERAL(14.98325) + ALWAN_ONE) + ALWAN_LITERAL(0.12783901);
    alwan_scalar linear_result = ALWAN_LITERAL(1.9754798) * x + ALWAN_LITERAL(0.12512219);
    alwan_scalar pos_result    = ALWAN_LITERAL(0.36726845) * ALWAN_LOG10(x * ALWAN_LITERAL(14.98325) + ALWAN_ONE) + ALWAN_LITERAL(0.12240537);
    return ALWAN_SELECT(x < x_threshold_low, neg_result,
           ALWAN_SELECT(x <= x_threshold_high, linear_result, pos_result));
}

ALWAN_INLINE alwan_scalar alwan_clog3_eotf(alwan_scalar encoded) {
    alwan_scalar threshold_low  = ALWAN_LITERAL(0.097465473);
    alwan_scalar threshold_high = ALWAN_LITERAL(0.15277891);
    alwan_scalar neg_result    = -(ALWAN_POW(ALWAN_LITERAL(10.0), (ALWAN_LITERAL(0.12783901) - encoded) / ALWAN_LITERAL(0.36726845)) - ALWAN_ONE) / ALWAN_LITERAL(14.98325);
    alwan_scalar linear_result = (encoded - ALWAN_LITERAL(0.12512219)) / ALWAN_LITERAL(1.9754798);
    alwan_scalar pos_result    = (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - ALWAN_LITERAL(0.12240537)) / ALWAN_LITERAL(0.36726845)) - ALWAN_ONE) / ALWAN_LITERAL(14.98325);
    alwan_scalar x = ALWAN_SELECT(encoded < threshold_low, neg_result,
                     ALWAN_SELECT(encoded <= threshold_high, linear_result, pos_result));
    return x * ALWAN_LITERAL(0.9);
}

/* V-Log -- Panasonic "V-Log/V-Gamut Reference Manual" Rev 1.0 (2014) */
ALWAN_INLINE alwan_scalar alwan_vlog_oetf(alwan_scalar lin) {
    alwan_scalar cut1 = ALWAN_LITERAL(0.01);
    alwan_scalar b = ALWAN_LITERAL(0.00873);
    alwan_scalar c = ALWAN_LITERAL(0.241514);
    alwan_scalar d = ALWAN_LITERAL(0.598206);
    alwan_scalar linear_result = ALWAN_LITERAL(5.6) * lin + ALWAN_LITERAL(0.125);
    alwan_scalar log_result    = c * ALWAN_LOG10(lin + b) + d;
    /* Panasonic's spec splits at `linear < cut1`, so the cut point itself
     * belongs to the log segment; the two segments are not exactly
     * continuous there. */
    return ALWAN_SELECT(lin < cut1, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_vlog_eotf(alwan_scalar encoded) {
    alwan_scalar cut2 = ALWAN_LITERAL(0.181);
    alwan_scalar b = ALWAN_LITERAL(0.00873);
    alwan_scalar c = ALWAN_LITERAL(0.241514);
    alwan_scalar d = ALWAN_LITERAL(0.598206);
    alwan_scalar linear_result = (encoded - ALWAN_LITERAL(0.125)) / ALWAN_LITERAL(5.6);
    alwan_scalar log_result    = ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - d) / c) - b;
    return ALWAN_SELECT(encoded <= cut2, linear_result, log_result);
}

/* LogC3 -- ARRI "LogC Curve -- Usage in VFX" (2012); EI 800 nominal exposure index parameters */
ALWAN_INLINE alwan_scalar alwan_logc3_oetf(alwan_scalar lin) {
    alwan_scalar a = ALWAN_LITERAL(5.555556);
    alwan_scalar b = ALWAN_LITERAL(0.052272);
    alwan_scalar c = ALWAN_LITERAL(0.247190);
    alwan_scalar d = ALWAN_LITERAL(0.385537);
    alwan_scalar e = ALWAN_LITERAL(5.367655);
    alwan_scalar f = ALWAN_LITERAL(0.092809);
    alwan_scalar log_result    = c * ALWAN_LOG10(a * lin + b) + d;
    alwan_scalar linear_result = e * lin + f;
    return ALWAN_SELECT(lin > ALWAN_LITERAL(0.010591), log_result, linear_result);
}

ALWAN_INLINE alwan_scalar alwan_logc3_eotf(alwan_scalar encoded) {
    alwan_scalar a = ALWAN_LITERAL(5.555556);
    alwan_scalar b = ALWAN_LITERAL(0.052272);
    alwan_scalar c = ALWAN_LITERAL(0.247190);
    alwan_scalar d = ALWAN_LITERAL(0.385537);
    alwan_scalar e = ALWAN_LITERAL(5.367655);
    alwan_scalar f = ALWAN_LITERAL(0.092809);
    alwan_scalar threshold = e * ALWAN_LITERAL(0.010591) + f;
    alwan_scalar linear_result = (encoded - f) / e;
    alwan_scalar log_result    = (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - d) / c) - b) / a;
    return ALWAN_SELECT(encoded < threshold, linear_result, log_result);
}

/* LogC4 -- ARRI LogC4 (Cooper 2022, ARRI LogC4 Specification)
 * Formula: (log2(a*x + 64) - 6) / 14 * b + c  for x >= t
 *          (x - t) / s                          for x < t  */
ALWAN_INLINE alwan_scalar alwan_logc4_oetf(alwan_scalar lin) {
    alwan_scalar a = ALWAN_LITERAL(2231.8263090676883);
    alwan_scalar b = ALWAN_LITERAL(0.9071358748778103);
    alwan_scalar c = ALWAN_LITERAL(0.09286412512218964);
    alwan_scalar s = ALWAN_LITERAL(0.1135972086105891);
    alwan_scalar t = ALWAN_LITERAL(-0.01805699611991131);
    alwan_scalar log_val = (ALWAN_LOG2(a * lin + ALWAN_LITERAL(64.0)) - ALWAN_LITERAL(6.0))
                         / ALWAN_LITERAL(14.0) * b + c;
    alwan_scalar lin_val = (lin - t) / s;
    return ALWAN_SELECT(lin < t, lin_val, log_val);
}

ALWAN_INLINE alwan_scalar alwan_logc4_eotf(alwan_scalar encoded) {
    alwan_scalar a = ALWAN_LITERAL(2231.8263090676883);
    alwan_scalar b = ALWAN_LITERAL(0.9071358748778103);
    alwan_scalar c = ALWAN_LITERAL(0.09286412512218964);
    alwan_scalar s = ALWAN_LITERAL(0.1135972086105891);
    alwan_scalar t = ALWAN_LITERAL(-0.01805699611991131);
    alwan_scalar scene = (ALWAN_POW(ALWAN_LITERAL(2.0),
        (encoded - c) / b * ALWAN_LITERAL(14.0) + ALWAN_LITERAL(6.0)) - ALWAN_LITERAL(64.0)) / a;
    alwan_scalar lin_val = encoded * s + t;
    return ALWAN_SELECT(encoded < ALWAN_ZERO, lin_val, scene);
}

/* REDLog -- RED Digital Cinema, matching the Sony Imageworks OCIO reference
 * implementation: (1023 + 511 * log10(x * (1 - bo) + bo)) / 1023 with a black
 * offset of 10^(-1023/511). */
ALWAN_INLINE alwan_scalar alwan_redlog_oetf(alwan_scalar lin) {
    alwan_scalar black_offset = ALWAN_LITERAL(0.009955040995908344);
    alwan_scalar L = ALWAN_SELECT(lin < ALWAN_ZERO, ALWAN_ZERO, lin);
    alwan_scalar arg_raw = L * (ALWAN_ONE - black_offset) + black_offset;
    alwan_scalar arg = ALWAN_SELECT(arg_raw <= ALWAN_ZERO, ALWAN_LITERAL(1e-10), arg_raw);
    return (ALWAN_LITERAL(1023.0) + ALWAN_LITERAL(511.0) * ALWAN_LOG10(arg)) / ALWAN_LITERAL(1023.0);
}

ALWAN_INLINE alwan_scalar alwan_redlog_eotf(alwan_scalar encoded) {
    alwan_scalar black_offset = ALWAN_LITERAL(0.009955040995908344);
    alwan_scalar exponent = (ALWAN_LITERAL(1023.0) * encoded - ALWAN_LITERAL(1023.0)) / ALWAN_LITERAL(511.0);
    return (ALWAN_POW(ALWAN_LITERAL(10.0), exponent) - black_offset) / (ALWAN_ONE - black_offset);
}

/* REDLogFilm -- RED Digital Cinema. Identical to the Cineon encoding; the
 * constants are repeated rather than delegated so each GPU backend stays
 * self contained. Keep in step with alwan_cineon_oetf / _eotf. */
ALWAN_INLINE alwan_scalar alwan_redlogfilm_oetf(alwan_scalar lin) {
    alwan_scalar black_offset = ALWAN_LITERAL(0.010797751623277);
    alwan_scalar L = ALWAN_SELECT(lin < ALWAN_ZERO, ALWAN_ZERO, lin);
    alwan_scalar arg_raw = L * (ALWAN_ONE - black_offset) + black_offset;
    alwan_scalar arg = ALWAN_SELECT(arg_raw <= ALWAN_ZERO, ALWAN_LITERAL(1e-10), arg_raw);
    return (ALWAN_LITERAL(685.0) + ALWAN_LITERAL(300.0) * ALWAN_LOG10(arg)) / ALWAN_LITERAL(1023.0);
}

ALWAN_INLINE alwan_scalar alwan_redlogfilm_eotf(alwan_scalar encoded) {
    alwan_scalar black_offset = ALWAN_LITERAL(0.010797751623277);
    alwan_scalar exponent = (ALWAN_LITERAL(1023.0) * encoded - ALWAN_LITERAL(685.0)) / ALWAN_LITERAL(300.0);
    return (ALWAN_POW(ALWAN_LITERAL(10.0), exponent) - black_offset) / (ALWAN_ONE - black_offset);
}

/* Log3G10 -- RED Digital Cinema "IPP2 Image Processing Pipeline" (2017) */
ALWAN_INLINE alwan_scalar alwan_log3g10_oetf(alwan_scalar lin) {
    alwan_scalar a = ALWAN_LITERAL(0.224282);
    alwan_scalar b = ALWAN_LITERAL(155.975327);
    alwan_scalar c = ALWAN_LITERAL(0.01);
    alwan_scalar g = ALWAN_LITERAL(15.1927);
    alwan_scalar x = lin + c;
    alwan_scalar linear_result = x * g;
    alwan_scalar log_result    = a * ALWAN_LOG10(x * b + ALWAN_ONE);
    return ALWAN_SELECT(x < ALWAN_ZERO, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_log3g10_eotf(alwan_scalar encoded) {
    alwan_scalar a = ALWAN_LITERAL(0.224282);
    alwan_scalar b = ALWAN_LITERAL(155.975327);
    alwan_scalar c = ALWAN_LITERAL(0.01);
    alwan_scalar g = ALWAN_LITERAL(15.1927);
    alwan_scalar linear_result = (encoded / g) - c;
    alwan_scalar log_result    = (ALWAN_POW(ALWAN_LITERAL(10.0), encoded / a) - ALWAN_ONE) / b - c;
    return ALWAN_SELECT(encoded < ALWAN_ZERO, linear_result, log_result);
}

/* BMDFilm (Gen5) -- Blackmagic Design "Blackmagic RAW 3.0 SDK" color science documentation */
ALWAN_INLINE alwan_scalar alwan_bmdfilm_oetf(alwan_scalar lin) {
    /* A*ln(x+B)+C  for x >= LIN_CUT;  D*x+E  for x < LIN_CUT */
    alwan_scalar A = ALWAN_LITERAL(0.08692876065491224);
    alwan_scalar B = ALWAN_LITERAL(0.005494072432257808);
    alwan_scalar C = ALWAN_LITERAL(0.5300133392291939);
    alwan_scalar D = ALWAN_LITERAL(8.283605932402494);
    alwan_scalar E = ALWAN_LITERAL(0.09246575342465753);
    alwan_scalar linear_result = D * lin + E;
    alwan_scalar log_result    = A * ALWAN_LN(lin + B) + C;
    return ALWAN_SELECT(lin < ALWAN_LITERAL(0.005), linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_bmdfilm_eotf(alwan_scalar encoded) {
    alwan_scalar A = ALWAN_LITERAL(0.08692876065491224);
    alwan_scalar B = ALWAN_LITERAL(0.005494072432257808);
    alwan_scalar C = ALWAN_LITERAL(0.5300133392291939);
    alwan_scalar D = ALWAN_LITERAL(8.283605932402494);
    alwan_scalar E = ALWAN_LITERAL(0.09246575342465753);
    alwan_scalar threshold     = D * ALWAN_LITERAL(0.005) + E;
    alwan_scalar linear_result = (encoded - E) / D;
    alwan_scalar log_result    = ALWAN_EXP((encoded - C) / A) - B;
    return ALWAN_SELECT(encoded < threshold, linear_result, log_result);
}

/* BMDFilm Gen4 -- Blackmagic Design Generation 4 Film color science (URSA Mini Pro 4.6K) */
ALWAN_INLINE alwan_scalar alwan_bmdfilm4_oetf(alwan_scalar lin) {
    alwan_scalar A       = ALWAN_LITERAL(5.2212906000378565);
    alwan_scalar B       = ALWAN_LITERAL(-0.00007134598996420424);
    alwan_scalar C       = ALWAN_LITERAL(0.03630411093543444);
    alwan_scalar D       = ALWAN_LITERAL(0.21566456116952773);
    alwan_scalar E       = ALWAN_LITERAL(0.7133134738229736);
    alwan_scalar lin_cut = ALWAN_LITERAL(0.00500072683168086);
    alwan_scalar linear_result = lin * A + B;
    alwan_scalar log_result    = ALWAN_LN(lin + C) * D + E;
    return ALWAN_SELECT(lin <= lin_cut, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_bmdfilm4_eotf(alwan_scalar encoded) {
    alwan_scalar A       = ALWAN_LITERAL(5.2212906000378565);
    alwan_scalar B       = ALWAN_LITERAL(-0.00007134598996420424);
    alwan_scalar D       = ALWAN_LITERAL(0.21566456116952773);
    alwan_scalar C       = ALWAN_LITERAL(0.03630411093543444);
    alwan_scalar E       = ALWAN_LITERAL(0.7133134738229736);
    alwan_scalar log_cut = ALWAN_LITERAL(0.026038902009648163);
    alwan_scalar linear_result = (encoded - B) / A;
    alwan_scalar log_result    = ALWAN_EXP((encoded - E) / D) - C;
    return ALWAN_SELECT(encoded <= log_cut, linear_result, log_result);
}

/* T-Log -- FilmLight "T-Log" transfer characteristic (Baselight / Truelight
 * colour space definition). Pairs with FilmLight E-Gamut primaries.
 * Constants folded from the reference parameterisation w = 128, g = 16,
 * o = 0.075:
 *     b  = 1 / (0.7107 + 1.2359 * ln(w * g))
 *     gs = g / (1 - o)                      C = b / gs
 *     a  = 1 - b * ln(w + C)                y0 = a + b * ln(C)
 *     s  = (1 - o) / (1 - y0)
 *     A  = 1 + (a - 1) * s   B = b * s      G = gs * s
 * The curve carries a black offset: T-Log(0) = o = 0.075, and is linear for
 * negative scene values. */
ALWAN_INLINE alwan_scalar alwan_tlog_oetf(alwan_scalar lin) {
    alwan_scalar A = ALWAN_LITERAL(0.55201265686066547);
    alwan_scalar B = ALWAN_LITERAL(0.092329025965773526);
    alwan_scalar C = ALWAN_LITERAL(0.0057048244042473785);
    alwan_scalar G = ALWAN_LITERAL(16.184376489665897);
    alwan_scalar o = ALWAN_LITERAL(0.075);
    alwan_scalar linear_result = lin * G + o;
    alwan_scalar log_result    = ALWAN_LN(lin + C) * B + A;
    return ALWAN_SELECT(lin < ALWAN_ZERO, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_tlog_eotf(alwan_scalar encoded) {
    alwan_scalar A = ALWAN_LITERAL(0.55201265686066547);
    alwan_scalar B = ALWAN_LITERAL(0.092329025965773526);
    alwan_scalar C = ALWAN_LITERAL(0.0057048244042473785);
    alwan_scalar G = ALWAN_LITERAL(16.184376489665897);
    alwan_scalar o = ALWAN_LITERAL(0.075);
    alwan_scalar linear_result = (encoded - o) / G;
    alwan_scalar log_result    = ALWAN_EXP((encoded - A) / B) - C;
    return ALWAN_SELECT(encoded < o, linear_result, log_result);
}

/* E-Log -- Olympus/OM System "OM-Log400 Transfer Characteristic" specification */
ALWAN_INLINE alwan_scalar alwan_elog_oetf(alwan_scalar lin) {
    alwan_scalar L = ALWAN_SELECT(lin < ALWAN_ZERO, ALWAN_ZERO, lin);
    return ALWAN_LN(L + ALWAN_ONE) / ALWAN_LN(ALWAN_LITERAL(10.0)) * ALWAN_LITERAL(0.4) + ALWAN_LITERAL(0.6);
}

ALWAN_INLINE alwan_scalar alwan_elog_eotf(alwan_scalar encoded) {
    return ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - ALWAN_LITERAL(0.6)) / ALWAN_LITERAL(0.4)) - ALWAN_ONE;
}

/* Protune -- GoPro "Protune Flat Color Profile" / CineForm SDK color science documentation */
ALWAN_INLINE alwan_scalar alwan_protune_oetf(alwan_scalar lin) {
    alwan_scalar L = ALWAN_SELECT(lin < ALWAN_ZERO, ALWAN_ZERO, lin);
    return ALWAN_LN(L * ALWAN_LITERAL(112.0) + ALWAN_ONE) / ALWAN_LN(ALWAN_LITERAL(113.0));
}

ALWAN_INLINE alwan_scalar alwan_protune_eotf(alwan_scalar encoded) {
    return (ALWAN_POW(ALWAN_LITERAL(113.0), encoded) - ALWAN_ONE) / ALWAN_LITERAL(112.0);
}

ALWAN_INLINE alwan_scalar alwan_gamma22_oetf(alwan_scalar lin) {
    alwan_scalar L = ALWAN_SELECT(lin < ALWAN_ZERO, ALWAN_ZERO, lin);
    return ALWAN_POW(L, ALWAN_ONE / ALWAN_LITERAL(2.2));
}

ALWAN_INLINE alwan_scalar alwan_gamma22_eotf(alwan_scalar encoded) {
    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(E, ALWAN_LITERAL(2.2));
}

ALWAN_INLINE alwan_scalar alwan_gamma24_oetf(alwan_scalar lin) {
    alwan_scalar L = ALWAN_SELECT(lin < ALWAN_ZERO, ALWAN_ZERO, lin);
    return ALWAN_POW(L, ALWAN_ONE / ALWAN_LITERAL(2.4));
}

ALWAN_INLINE alwan_scalar alwan_gamma24_eotf(alwan_scalar encoded) {
    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(E, ALWAN_LITERAL(2.4));
}

ALWAN_INLINE alwan_scalar alwan_gamma26_oetf(alwan_scalar lin) {
    alwan_scalar L = ALWAN_SELECT(lin < ALWAN_ZERO, ALWAN_ZERO, lin);
    return ALWAN_POW(L, ALWAN_ONE / ALWAN_LITERAL(2.6));
}

ALWAN_INLINE alwan_scalar alwan_gamma26_eotf(alwan_scalar encoded) {
    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(E, ALWAN_LITERAL(2.6));
}

ALWAN_INLINE alwan_scalar alwan_gamma28_oetf(alwan_scalar lin) {
    alwan_scalar L = ALWAN_SELECT(lin < ALWAN_ZERO, ALWAN_ZERO, lin);
    return ALWAN_POW(L, ALWAN_ONE / ALWAN_LITERAL(2.8));
}

ALWAN_INLINE alwan_scalar alwan_gamma28_eotf(alwan_scalar encoded) {
    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(E, ALWAN_LITERAL(2.8));
}

/* N-Log -- Nikon "N-Log Specification Document" Ver. 1.0.0 (2018) */
ALWAN_INLINE alwan_scalar alwan_nlog_oetf(alwan_scalar lin) {
    /* Nikon N-Log: a*(y+b)^(1/3)  for y < cut1;  c*ln(y)+d  for y >= cut1
     * Source: Nikon "N-Log Specification" (2018)
     * Signed cube root: follows colour-science spow convention so that
     * linear < -b gives a negative result (clipped to 0 by callers). */
    alwan_scalar a    = ALWAN_LITERAL(0.635386119257087);
    alwan_scalar b    = ALWAN_LITERAL(0.0075);
    alwan_scalar c    = ALWAN_LITERAL(0.1466275659824047);
    alwan_scalar d    = ALWAN_LITERAL(0.6050830889540567);
    alwan_scalar cut1 = ALWAN_LITERAL(0.328);
    alwan_scalar arg      = lin + b;
    alwan_scalar abs_arg  = ALWAN_SELECT(arg < ALWAN_ZERO, -arg, arg);
    alwan_scalar sign_arg = ALWAN_SELECT(arg < ALWAN_ZERO, -ALWAN_ONE, ALWAN_ONE);
    alwan_scalar linear_result = a * sign_arg * ALWAN_POW(abs_arg, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
    alwan_scalar log_result    = c * ALWAN_LN(lin) + d;
    return ALWAN_SELECT(lin < cut1, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_nlog_eotf(alwan_scalar encoded) {
    alwan_scalar a    = ALWAN_LITERAL(0.635386119257087);
    alwan_scalar b    = ALWAN_LITERAL(0.0075);
    alwan_scalar c    = ALWAN_LITERAL(0.1466275659824047);
    alwan_scalar d    = ALWAN_LITERAL(0.6050830889540567);
    alwan_scalar cut2 = ALWAN_LITERAL(0.4418377321603128);
    alwan_scalar linear_result = ALWAN_POW(encoded / a, ALWAN_LITERAL(3.0)) - b;
    alwan_scalar log_result    = ALWAN_EXP((encoded - d) / c);
    return ALWAN_SELECT(encoded < cut2, linear_result, log_result);
}

/* Cineon -- Kodak "Reference Manual for Cineon Digital Film System" (1992);
 * log encoding used in Kodak Cineon film scanner/recorder */
ALWAN_INLINE alwan_scalar alwan_cineon_oetf(alwan_scalar lin) {
    alwan_scalar black_offset = ALWAN_LITERAL(0.010797751623277);
    alwan_scalar x = ALWAN_SELECT(lin < ALWAN_ZERO, ALWAN_ZERO, lin);
    alwan_scalar arg_raw = x * (ALWAN_ONE - black_offset) + black_offset;
    alwan_scalar arg = ALWAN_SELECT(arg_raw <= ALWAN_ZERO, ALWAN_LITERAL(1e-10), arg_raw);
    return (ALWAN_LITERAL(685.0) + ALWAN_LITERAL(300.0) * ALWAN_LOG10(arg)) / ALWAN_LITERAL(1023.0);
}

ALWAN_INLINE alwan_scalar alwan_cineon_eotf(alwan_scalar encoded) {
    alwan_scalar black_offset = ALWAN_LITERAL(0.010797751623277);
    alwan_scalar exponent = (ALWAN_LITERAL(1023.0) * encoded - ALWAN_LITERAL(685.0)) / ALWAN_LITERAL(300.0);
    return (ALWAN_POW(ALWAN_LITERAL(10.0), exponent) - black_offset) /
           (ALWAN_ONE - black_offset);
}

/* Apple Log -- Apple "Apple Log Profile White Paper" (2023); for ProRes RAW and iPhone ProRes */
ALWAN_INLINE alwan_scalar alwan_apple_log_oetf(alwan_scalar lin) {
    alwan_scalar R0    = ALWAN_LITERAL(-0.05641088);
    alwan_scalar Rt    = ALWAN_LITERAL(0.01);
    alwan_scalar C     = ALWAN_LITERAL(47.28711236);
    alwan_scalar beta  = ALWAN_LITERAL(0.00964052);
    alwan_scalar gamma = ALWAN_LITERAL(0.08550479);
    alwan_scalar delta = ALWAN_LITERAL(0.69336945);
    alwan_scalar diff = lin - R0;
    alwan_scalar quad_result = C * diff * diff;
    alwan_scalar log_result  = gamma * ALWAN_LOG2(lin + beta) + delta;
    return ALWAN_SELECT(lin < R0, ALWAN_ZERO,
           ALWAN_SELECT(lin < Rt, quad_result, log_result));
}

ALWAN_INLINE alwan_scalar alwan_apple_log_eotf(alwan_scalar encoded) {
    alwan_scalar R0    = ALWAN_LITERAL(-0.05641088);
    alwan_scalar C     = ALWAN_LITERAL(47.28711236);
    alwan_scalar beta  = ALWAN_LITERAL(0.00964052);
    alwan_scalar gamma = ALWAN_LITERAL(0.08550479);
    alwan_scalar delta = ALWAN_LITERAL(0.69336945);
    alwan_scalar Pt    = ALWAN_LITERAL(0.20855531595464202);
    alwan_scalar sqrt_result = ALWAN_SQRT(encoded / C) + R0;
    alwan_scalar pow_result  = ALWAN_POW(ALWAN_LITERAL(2.0), (encoded - delta) / gamma) - beta;
    return ALWAN_SELECT(encoded < ALWAN_ZERO, R0,
           ALWAN_SELECT(encoded < Pt, sqrt_result, pow_result));
}

/* F-Log -- Fujifilm "F-Log Data Sheet" Rev 1.0 (2013) */
ALWAN_INLINE alwan_scalar alwan_flog_oetf(alwan_scalar lin) {
    alwan_scalar cut1 = ALWAN_LITERAL(0.00089);
    alwan_scalar A = ALWAN_LITERAL(0.555556);
    alwan_scalar B = ALWAN_LITERAL(0.009468);
    alwan_scalar C = ALWAN_LITERAL(0.344676);
    alwan_scalar D = ALWAN_LITERAL(0.790453);
    alwan_scalar E = ALWAN_LITERAL(8.735631);
    alwan_scalar F = ALWAN_LITERAL(0.092864);
    alwan_scalar linear_result = E * lin + F;
    alwan_scalar log_result    = C * ALWAN_LOG10(A * lin + B) + D;
    return ALWAN_SELECT(lin < cut1, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_flog_eotf(alwan_scalar encoded) {
    alwan_scalar cut2 = ALWAN_LITERAL(0.100537775223865);
    alwan_scalar A = ALWAN_LITERAL(0.555556);
    alwan_scalar B = ALWAN_LITERAL(0.009468);
    alwan_scalar C = ALWAN_LITERAL(0.344676);
    alwan_scalar D = ALWAN_LITERAL(0.790453);
    alwan_scalar E = ALWAN_LITERAL(8.735631);
    alwan_scalar F = ALWAN_LITERAL(0.092864);
    alwan_scalar linear_result = (encoded - F) / E;
    alwan_scalar log_result    = (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - D) / C) - B) / A;
    return ALWAN_SELECT(encoded < cut2, linear_result, log_result);
}

/* F-Log2 -- Fujifilm "F-Log2 Data Sheet" Rev 1.0 (2019); extended range for GFX and X-T series */
ALWAN_INLINE alwan_scalar alwan_flog2_oetf(alwan_scalar lin) {
    alwan_scalar cut1 = ALWAN_LITERAL(0.000889);
    alwan_scalar A = ALWAN_LITERAL(5.555556);
    alwan_scalar B = ALWAN_LITERAL(0.064829);
    alwan_scalar C = ALWAN_LITERAL(0.245281);
    alwan_scalar D = ALWAN_LITERAL(0.384316);
    alwan_scalar E = ALWAN_LITERAL(8.799461);
    alwan_scalar F = ALWAN_LITERAL(0.092864);
    alwan_scalar linear_result = E * lin + F;
    alwan_scalar log_result    = C * ALWAN_LOG10(A * lin + B) + D;
    return ALWAN_SELECT(lin < cut1, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_flog2_eotf(alwan_scalar encoded) {
    alwan_scalar cut2 = ALWAN_LITERAL(0.100686685370811);
    alwan_scalar A = ALWAN_LITERAL(5.555556);
    alwan_scalar B = ALWAN_LITERAL(0.064829);
    alwan_scalar C = ALWAN_LITERAL(0.245281);
    alwan_scalar D = ALWAN_LITERAL(0.384316);
    alwan_scalar E = ALWAN_LITERAL(8.799461);
    alwan_scalar F = ALWAN_LITERAL(0.092864);
    alwan_scalar linear_result = (encoded - F) / E;
    alwan_scalar log_result    = (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - D) / C) - B) / A;
    return ALWAN_SELECT(encoded < cut2, linear_result, log_result);
}

/* L-Log -- Leica "L-Log Reference Manual" (2022).
 *   LSR <= cut1 : A * LSR + B
 *   else        : C * log10(D * LSR + E) + F
 * cut2 = A * cut1 + B = 0.138 is the matching encoded-side split. */
ALWAN_INLINE alwan_scalar alwan_llog_oetf(alwan_scalar lin) {
    alwan_scalar cut1 = ALWAN_LITERAL(0.006);
    alwan_scalar A = ALWAN_LITERAL(8.0);
    alwan_scalar B = ALWAN_LITERAL(0.09);
    alwan_scalar C = ALWAN_LITERAL(0.27);
    alwan_scalar D = ALWAN_LITERAL(1.3);
    alwan_scalar E = ALWAN_LITERAL(0.0115);
    alwan_scalar F = ALWAN_LITERAL(0.6);
    alwan_scalar linear_result = A * lin + B;
    alwan_scalar log_result    = C * ALWAN_LOG10(D * lin + E) + F;
    return ALWAN_SELECT(lin <= cut1, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_llog_eotf(alwan_scalar encoded) {
    alwan_scalar cut2 = ALWAN_LITERAL(0.138);
    alwan_scalar A = ALWAN_LITERAL(8.0);
    alwan_scalar B = ALWAN_LITERAL(0.09);
    alwan_scalar C = ALWAN_LITERAL(0.27);
    alwan_scalar D = ALWAN_LITERAL(1.3);
    alwan_scalar E = ALWAN_LITERAL(0.0115);
    alwan_scalar F = ALWAN_LITERAL(0.6);
    alwan_scalar linear_result = (encoded - B) / A;
    alwan_scalar log_result    = (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - F) / C) - E) / D;
    return ALWAN_SELECT(encoded <= cut2, linear_result, log_result);
}

/* D-Log -- DJI "D-Log Color Transformation -- User Guide" Rev 1.0; for Zenmuse X and Inspire */
ALWAN_INLINE alwan_scalar alwan_dlog_oetf(alwan_scalar lin) {
    alwan_scalar cut       = ALWAN_LITERAL(0.0078);
    alwan_scalar lin_slope = ALWAN_LITERAL(6.025);
    alwan_scalar lin_off   = ALWAN_LITERAL(0.0929);
    alwan_scalar log_mult  = ALWAN_LITERAL(0.256663);
    alwan_scalar log_off   = ALWAN_LITERAL(0.584555);
    alwan_scalar log_scale = ALWAN_LITERAL(0.9892);
    alwan_scalar log_bias  = ALWAN_LITERAL(0.0108);
    alwan_scalar linear_result = lin_slope * lin + lin_off;
    alwan_scalar log_result    = ALWAN_LOG10(lin * log_scale + log_bias) * log_mult + log_off;
    return ALWAN_SELECT(lin <= cut, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_dlog_eotf(alwan_scalar encoded) {
    alwan_scalar cut_enc   = ALWAN_LITERAL(0.14);
    alwan_scalar lin_slope = ALWAN_LITERAL(6.025);
    alwan_scalar lin_off   = ALWAN_LITERAL(0.0929);
    alwan_scalar log_mult  = ALWAN_LITERAL(0.256663);
    alwan_scalar log_off   = ALWAN_LITERAL(0.584555);
    alwan_scalar log_scale = ALWAN_LITERAL(0.9892);
    alwan_scalar log_bias  = ALWAN_LITERAL(0.0108);
    alwan_scalar linear_result = (encoded - lin_off) / lin_slope;
    alwan_scalar log_result    = (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - log_off) / log_mult) - log_bias) / log_scale;
    return ALWAN_SELECT(encoded <= cut_enc, linear_result, log_result);
}

/* DCDM -- SMPTE ST 428-1:2019 "D-Cinema Distribution Master -- Image Characteristics"; gamma 2.6 */
ALWAN_INLINE alwan_scalar alwan_dcdm_oetf(alwan_scalar lin) {
    alwan_scalar scale     = ALWAN_LITERAL(0.9165552797403094);
    alwan_scalar inv_gamma = ALWAN_LITERAL(0.38461538461538464);
    alwan_scalar L = ALWAN_SELECT(lin <= ALWAN_ZERO, ALWAN_ZERO, lin);
    return ALWAN_POW(L * scale, inv_gamma);
}

ALWAN_INLINE alwan_scalar alwan_dcdm_eotf(alwan_scalar encoded) {
    alwan_scalar inv_scale = ALWAN_LITERAL(1.0910416666666667);
    alwan_scalar gamma     = ALWAN_LITERAL(2.6);
    alwan_scalar E = ALWAN_SELECT(encoded <= ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(E, gamma) * inv_scale;
}

ALWAN_INLINE alwan_scalar alwan_linear_identity(alwan_scalar v) {
    return v;
}

/* ADX10/ADX16 -- ACES S-2008-001 "Academy Density Exchange Encoding (ADX)" */
ALWAN_INLINE alwan_scalar alwan_adx10_oetf(alwan_scalar density) {
    alwan_scalar ref_pt = ALWAN_LITERAL(0.5);
    alwan_scalar scale  = ALWAN_LITERAL(400.0);
    alwan_scalar norm   = ALWAN_LITERAL(1023.0);
    alwan_scalar cv_raw = (density + ref_pt) * scale;
    alwan_scalar cv = ALWAN_SELECT(cv_raw < ALWAN_ZERO, ALWAN_ZERO,
                      ALWAN_SELECT(cv_raw > norm, norm, cv_raw));
    return cv / norm;
}

ALWAN_INLINE alwan_scalar alwan_adx10_eotf(alwan_scalar encoded) {
    return encoded * ALWAN_LITERAL(1023.0) / ALWAN_LITERAL(400.0) - ALWAN_LITERAL(0.5);
}

ALWAN_INLINE alwan_scalar alwan_adx16_oetf(alwan_scalar density) {
    alwan_scalar ref_pt = ALWAN_LITERAL(0.5);
    alwan_scalar scale  = ALWAN_LITERAL(25600.0);
    alwan_scalar norm   = ALWAN_LITERAL(65535.0);
    alwan_scalar cv_raw = (density + ref_pt) * scale;
    alwan_scalar cv = ALWAN_SELECT(cv_raw < ALWAN_ZERO, ALWAN_ZERO,
                      ALWAN_SELECT(cv_raw > norm, norm, cv_raw));
    return cv / norm;
}

ALWAN_INLINE alwan_scalar alwan_adx16_eotf(alwan_scalar encoded) {
    return encoded * ALWAN_LITERAL(65535.0) / ALWAN_LITERAL(25600.0) - ALWAN_LITERAL(0.5);
}

ALWAN_INLINE alwan_scalar alwan_gamma_oetf_v(alwan_scalar lin, alwan_scalar gamma) {
    alwan_scalar safe = ALWAN_SELECT(lin < ALWAN_ZERO, ALWAN_ZERO, lin);
    return ALWAN_POW(safe, ALWAN_ONE / gamma);
}

ALWAN_INLINE alwan_scalar alwan_gamma_eotf_v(alwan_scalar encoded, alwan_scalar gamma) {
    alwan_scalar safe = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(safe, gamma);
}

ALWAN_INLINE alwan_scalar alwan_dicom_gsdf_eotf_v(alwan_scalar jnd) {
    alwan_scalar const a = ALWAN_LITERAL(-1.3011877);
    alwan_scalar const b = ALWAN_LITERAL(-2.5840191e-2);
    alwan_scalar const c = ALWAN_LITERAL( 8.0242636e-2);
    alwan_scalar const d = ALWAN_LITERAL(-1.0320229e-1);
    alwan_scalar const e = ALWAN_LITERAL( 1.3646699e-1);
    alwan_scalar const f = ALWAN_LITERAL( 2.8745620e-2);
    alwan_scalar const g = ALWAN_LITERAL(-2.5468404e-2);
    alwan_scalar const h = ALWAN_LITERAL(-3.1978977e-3);
    alwan_scalar const k = ALWAN_LITERAL( 1.2992634e-4);
    alwan_scalar const m = ALWAN_LITERAL( 1.3635334e-3);
    alwan_scalar safe_jnd = ALWAN_SELECT(jnd < ALWAN_LITERAL(1.0),
                                          ALWAN_LITERAL(1.0), jnd);
    alwan_scalar lj = ALWAN_LN(safe_jnd);
    alwan_scalar lj2 = lj * lj;
    alwan_scalar lj3 = lj2 * lj;
    alwan_scalar lj4 = lj2 * lj2;
    alwan_scalar lj5 = lj4 * lj;
    alwan_scalar numer = a + c * lj + e * lj2 + g * lj3 + m * lj4;
    alwan_scalar denom = ALWAN_ONE + b * lj + d * lj2 + f * lj3
                        + h * lj4 + k * lj5;
    alwan_scalar safe_denom = ALWAN_SELECT(ALWAN_ABS(denom) < ALWAN_LITERAL(1e-10),
                                            ALWAN_LITERAL(1e-10), denom);
    alwan_scalar log_L = numer / safe_denom;
    return ALWAN_POW(ALWAN_LITERAL(10.0), log_L);
}

ALWAN_INLINE alwan_scalar alwan_dicom_gsdf_oetf_v(alwan_scalar luminance) {
    alwan_scalar const a = ALWAN_LITERAL( 71.498068);
    alwan_scalar const b = ALWAN_LITERAL( 94.593053);
    alwan_scalar const c = ALWAN_LITERAL( 41.912053);
    alwan_scalar const d = ALWAN_LITERAL(  9.8247004);
    alwan_scalar const e = ALWAN_LITERAL(  0.28175407);
    alwan_scalar const f = ALWAN_LITERAL( -1.1878455);
    alwan_scalar const g = ALWAN_LITERAL( -0.18014349);
    alwan_scalar const h = ALWAN_LITERAL(  0.14710899);
    alwan_scalar const k = ALWAN_LITERAL( -0.017046845);
    alwan_scalar safe_L = ALWAN_SELECT(luminance < ALWAN_LITERAL(0.05),
                                        ALWAN_LITERAL(0.05), luminance);
    alwan_scalar ll = ALWAN_LOG10(safe_L);
    alwan_scalar ll2 = ll * ll;
    alwan_scalar ll3 = ll2 * ll;
    alwan_scalar ll4 = ll2 * ll2;
    alwan_scalar jnd = a + b * ll + c * ll2 + d * ll3
                      + e * ll4 + f * ll2 * ll3
                      + g * ll3 * ll3 + h * ll3 * ll4
                      + k * ll4 * ll4;
    return ALWAN_SELECT(jnd < ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), jnd);
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_RGB_CORE_H */
