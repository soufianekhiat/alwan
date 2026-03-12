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

/* ================================================================
 * xy chromaticity to XYZ (Y=1)
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

/* ================================================================
 * RGB Matrix Derivation (value-returning)
 *
 * Computes RGB<->XYZ matrices from xy chromaticity primaries
 * and white point using the standard IEC 61966-2-1 method.
 * ================================================================ */

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

    /* Build chromaticity matrix P directly from (x, y, 1-x-y).
     * Unlike the XYZ-with-Y=1 approach (x/y, 1, (1-x-y)/y), this
     * avoids division by y and handles primaries with y=0
     * (e.g. DCDM XYZ, Max RGB, Xtreme RGB). */
    alwan_mat3x3 M;
    M.m[0] = rx; M.m[1] = gx; M.m[2] = bx;
    M.m[3] = ry; M.m[4] = gy; M.m[5] = by;
    M.m[6] = ALWAN_ONE - rx - ry;
    M.m[7] = ALWAN_ONE - gx - gy;
    M.m[8] = ALWAN_ONE - bx - by;

    /* White point XYZ (Y=1) -- white point always has y > 0 */
    alwan_vec3 W_XYZ = alwan_xy_to_xyz_v(wx, wy);

    /* Solve for scale factors: S = M^-1 * W */
    alwan_mat3x3 M_inv = alwan_mat3_inv_v(M);
    alwan_vec3 S = alwan_mat3_mulv_v(M_inv, W_XYZ);

    /* RGB->XYZ = M * diag(S) -- scale each column by corresponding S */
    result.rgb_to_xyz.m[0] = M.m[0] * S.v[0];
    result.rgb_to_xyz.m[1] = M.m[1] * S.v[1];
    result.rgb_to_xyz.m[2] = M.m[2] * S.v[2];
    result.rgb_to_xyz.m[3] = M.m[3] * S.v[0];
    result.rgb_to_xyz.m[4] = M.m[4] * S.v[1];
    result.rgb_to_xyz.m[5] = M.m[5] * S.v[2];
    result.rgb_to_xyz.m[6] = M.m[6] * S.v[0];
    result.rgb_to_xyz.m[7] = M.m[7] * S.v[1];
    result.rgb_to_xyz.m[8] = M.m[8] * S.v[2];

    /* XYZ->RGB = inverse of RGB->XYZ */
    result.xyz_to_rgb = alwan_mat3_inv_v(result.rgb_to_xyz);

    return result;
}

/* ================================================================
 * Legal Range / Full Range Conversion (10-bit)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_legal_to_full_10bit(alwan_scalar legal) {
    return (legal - ALWAN_LITERAL(0.062561094819159)) / ALWAN_LITERAL(0.856304985337243);
}

ALWAN_INLINE alwan_scalar alwan_full_to_legal_10bit(alwan_scalar full) {
    return full * ALWAN_LITERAL(0.856304985337243) + ALWAN_LITERAL(0.062561094819159);
}

/* ================================================================
 * ACESproxy Transfer Functions
 * Reference: Academy S-2013-001
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_acesproxy_oetf(alwan_scalar linear) {
    alwan_scalar mid_gray_in     = ALWAN_LITERAL(0.18);
    alwan_scalar mid_code_value  = ALWAN_LITERAL(425.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar steps_per_stop  = ALWAN_LITERAL(50.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar min_code        = ALWAN_LITERAL(64.0) / ALWAN_LITERAL(1023.0);

    alwan_scalar log_val    = ALWAN_LN(linear / mid_gray_in) / ALWAN_LN(ALWAN_LITERAL(2.0));
    alwan_scalar log_result = mid_code_value + steps_per_stop * log_val;
    return ALWAN_SELECT(linear <= ALWAN_ZERO, min_code, log_result);
}

ALWAN_INLINE alwan_scalar alwan_acesproxy_eotf(alwan_scalar encoded) {
    alwan_scalar mid_gray_in     = ALWAN_LITERAL(0.18);
    alwan_scalar mid_code_value  = ALWAN_LITERAL(425.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar steps_per_stop  = ALWAN_LITERAL(50.0) / ALWAN_LITERAL(1023.0);

    alwan_scalar log_val = (encoded - mid_code_value) / steps_per_stop;
    return mid_gray_in * ALWAN_POW(ALWAN_LITERAL(2.0), log_val);
}

/* ================================================================
 * ACEScc Transfer Functions (SMPTE ST 2065-5)
 * Reference: Academy S-2014-003
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_acescc_oetf(alwan_scalar linear) {
    alwan_scalar min_cutoff = ALWAN_LITERAL(0.00003051757812);  /* 2^-15 */
    alwan_scalar log2_e     = ALWAN_LITERAL(1.4426950408889634);

    alwan_scalar neg_result  = (ALWAN_LITERAL(-16.0) + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);
    alwan_scalar val         = ALWAN_LITERAL(0.0000152587890625) + linear * ALWAN_LITERAL(0.5);
    alwan_scalar low_result  = (ALWAN_LN(val) * log2_e + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);
    alwan_scalar high_result = (ALWAN_LN(linear) * log2_e + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);

    return ALWAN_SELECT(linear <= ALWAN_ZERO, neg_result,
           ALWAN_SELECT(linear < min_cutoff, low_result, high_result));
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

/* ================================================================
 * ACEScct Transfer Functions (SMPTE ST 2065-5)
 * Reference: Academy S-2016-001
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_acescct_oetf(alwan_scalar linear) {
    alwan_scalar cut   = ALWAN_LITERAL(0.0078125);  /* 2^-7 */
    alwan_scalar A     = ALWAN_LITERAL(10.5402377416545);
    alwan_scalar B     = ALWAN_LITERAL(0.0729055341958355);
    alwan_scalar log2_e = ALWAN_LITERAL(1.4426950408889634);

    alwan_scalar linear_result = A * linear + B;
    alwan_scalar log_result    = (ALWAN_LN(linear) * log2_e + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);
    return ALWAN_SELECT(linear <= cut, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_acescct_eotf(alwan_scalar encoded) {
    alwan_scalar cut_enc = ALWAN_LITERAL(0.155251141552511);
    alwan_scalar A       = ALWAN_LITERAL(10.5402377416545);
    alwan_scalar B       = ALWAN_LITERAL(0.0729055341958355);

    alwan_scalar linear_result = (encoded - B) / A;
    alwan_scalar log_result    = ALWAN_POW(ALWAN_LITERAL(2.0), encoded * ALWAN_LITERAL(17.52) - ALWAN_LITERAL(9.72));
    return ALWAN_SELECT(encoded <= cut_enc, linear_result, log_result);
}

/* ================================================================
 * Sony S-Log Family
 * Reference: SonyCorporation2012a
 * ================================================================ */

/* S-Log OETF: linear -> S-Log (scene-referred, legal range output) */
ALWAN_INLINE alwan_scalar alwan_slog_oetf(alwan_scalar linear) {
    alwan_scalar x = linear / ALWAN_LITERAL(0.9);

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

/* S-Log2: S-Log with 155/219 scaling */
ALWAN_INLINE alwan_scalar alwan_slog2_oetf(alwan_scalar linear) {
    return alwan_slog_oetf(linear * ALWAN_LITERAL(155.0) / ALWAN_LITERAL(219.0));
}

ALWAN_INLINE alwan_scalar alwan_slog2_eotf(alwan_scalar encoded) {
    return alwan_slog_eotf(encoded) * ALWAN_LITERAL(219.0) / ALWAN_LITERAL(155.0);
}

/* S-Log3 OETF: linear -> S-Log3 (legal range output) */
ALWAN_INLINE alwan_scalar alwan_slog3_oetf(alwan_scalar linear) {
    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);

    alwan_scalar log_result    = (ALWAN_LITERAL(420.0) + ALWAN_LOG10((L + ALWAN_LITERAL(0.01)) / ALWAN_LITERAL(0.19)) * ALWAN_LITERAL(261.5)) / ALWAN_LITERAL(1023.0);
    alwan_scalar linear_result = (L * (ALWAN_LITERAL(171.2102946929) - ALWAN_LITERAL(95.0)) / ALWAN_LITERAL(0.01125000) + ALWAN_LITERAL(95.0)) / ALWAN_LITERAL(1023.0);

    return ALWAN_SELECT(L >= ALWAN_LITERAL(0.01125000), log_result, linear_result);
}

ALWAN_INLINE alwan_scalar alwan_slog3_eotf(alwan_scalar encoded) {
    alwan_scalar code = encoded * ALWAN_LITERAL(1023.0);

    alwan_scalar log_result    = (ALWAN_POW(ALWAN_LITERAL(10.0), ((code - ALWAN_LITERAL(420.0)) / ALWAN_LITERAL(261.5))) * ALWAN_LITERAL(0.19)) - ALWAN_LITERAL(0.01);
    alwan_scalar linear_result = ((code - ALWAN_LITERAL(95.0)) / (ALWAN_LITERAL(171.2102946929) - ALWAN_LITERAL(95.0))) * ALWAN_LITERAL(0.01125000);
    return ALWAN_SELECT(code >= ALWAN_LITERAL(171.2102946929), log_result, linear_result);
}

/* ================================================================
 * Canon C-Log Family
 * Reference: CanonInc2012, CanonInc2016
 * ================================================================ */

/* C-Log v1.2 */
ALWAN_INLINE alwan_scalar alwan_clog_oetf(alwan_scalar linear) {
    alwan_scalar a = ALWAN_LITERAL(0.45310179);
    alwan_scalar k = ALWAN_LITERAL(10.1596);
    alwan_scalar offset = ALWAN_LITERAL(0.12512248);

    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
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

/* C-Log2 v1.2 */
ALWAN_INLINE alwan_scalar alwan_clog2_oetf(alwan_scalar linear) {
    alwan_scalar a = ALWAN_LITERAL(0.24136077);
    alwan_scalar k = ALWAN_LITERAL(87.09937546);
    alwan_scalar offset = ALWAN_LITERAL(0.092864125);

    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
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

/* C-Log3 (3-region piecewise) */
ALWAN_INLINE alwan_scalar alwan_clog3_oetf(alwan_scalar linear) {
    alwan_scalar x = linear / ALWAN_LITERAL(0.9);

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

/* ================================================================
 * Panasonic V-Log
 * Reference: PanasonicCorporation2014
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_vlog_oetf(alwan_scalar linear) {
    alwan_scalar cut1 = ALWAN_LITERAL(0.01);
    alwan_scalar b = ALWAN_LITERAL(0.00873);
    alwan_scalar c = ALWAN_LITERAL(0.241514);
    alwan_scalar d = ALWAN_LITERAL(0.598206);

    alwan_scalar linear_result = ALWAN_LITERAL(5.6) * linear + ALWAN_LITERAL(0.125);
    alwan_scalar log_result    = c * ALWAN_LOG10(linear + b) + d;

    return ALWAN_SELECT(linear <= cut1, linear_result, log_result);
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

/* ================================================================
 * ARRI LogC Family
 * ================================================================ */

/* LogC3 (EI 800) */
ALWAN_INLINE alwan_scalar alwan_logc3_oetf(alwan_scalar linear) {
    alwan_scalar a = ALWAN_LITERAL(5.555556);
    alwan_scalar b = ALWAN_LITERAL(0.052272);
    alwan_scalar c = ALWAN_LITERAL(0.247190);
    alwan_scalar d = ALWAN_LITERAL(0.385537);
    alwan_scalar e = ALWAN_LITERAL(5.367655);
    alwan_scalar f = ALWAN_LITERAL(0.092809);

    alwan_scalar log_result    = c * ALWAN_LOG10(a * linear + b) + d;
    alwan_scalar linear_result = e * linear + f;
    return ALWAN_SELECT(linear > ALWAN_LITERAL(0.010591), log_result, linear_result);
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

/* LogC4 */
ALWAN_INLINE alwan_scalar alwan_logc4_oetf(alwan_scalar linear) {
    alwan_scalar a = ALWAN_LITERAL(14.98325);
    alwan_scalar b = ALWAN_LITERAL(0.005494072);
    alwan_scalar c = ALWAN_LITERAL(0.0647954);
    alwan_scalar d = ALWAN_LITERAL(0.0075);
    alwan_scalar e = ALWAN_LITERAL(0.000000089);
    alwan_scalar f = ALWAN_LITERAL(0.0);

    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
    alwan_scalar t = (L + e) / (L + d);
    return (c * ALWAN_LOG10(a * t + b)) + f;
}

ALWAN_INLINE alwan_scalar alwan_logc4_eotf(alwan_scalar encoded) {
    alwan_scalar a = ALWAN_LITERAL(14.98325);
    alwan_scalar b = ALWAN_LITERAL(0.005494072);
    alwan_scalar c = ALWAN_LITERAL(0.0647954);
    alwan_scalar d = ALWAN_LITERAL(0.0075);
    alwan_scalar e = ALWAN_LITERAL(0.000000089);
    alwan_scalar f = ALWAN_LITERAL(0.0);

    alwan_scalar s = ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - f) / c);
    return (d * (s - b) - e * (a * s - b)) / (a * s - b - s + ALWAN_ONE);
}

/* ================================================================
 * RED Log Family
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_redlog_oetf(alwan_scalar linear) {
    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
    return (ALWAN_LOG10(L * ALWAN_LITERAL(0.9) + ALWAN_LITERAL(0.1)) + ALWAN_LITERAL(3.0)) / ALWAN_LITERAL(3.0);
}

ALWAN_INLINE alwan_scalar alwan_redlog_eotf(alwan_scalar encoded) {
    return (ALWAN_POW(ALWAN_LITERAL(10.0), encoded * ALWAN_LITERAL(3.0) - ALWAN_LITERAL(3.0)) - ALWAN_LITERAL(0.1)) / ALWAN_LITERAL(0.9);
}

ALWAN_INLINE alwan_scalar alwan_redlogfilm_oetf(alwan_scalar linear) {
    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
    return (ALWAN_LOG10(L * ALWAN_LITERAL(0.8) + ALWAN_LITERAL(0.1)) + ALWAN_LITERAL(3.0)) / ALWAN_LITERAL(3.0);
}

ALWAN_INLINE alwan_scalar alwan_redlogfilm_eotf(alwan_scalar encoded) {
    return (ALWAN_POW(ALWAN_LITERAL(10.0), encoded * ALWAN_LITERAL(3.0) - ALWAN_LITERAL(3.0)) - ALWAN_LITERAL(0.1)) / ALWAN_LITERAL(0.8);
}

/* Log3G10 (RED whitepaper 915-0187 Rev-C) */
ALWAN_INLINE alwan_scalar alwan_log3g10_oetf(alwan_scalar linear) {
    alwan_scalar a = ALWAN_LITERAL(0.224282);
    alwan_scalar b = ALWAN_LITERAL(155.975327);
    alwan_scalar c = ALWAN_LITERAL(0.01);
    alwan_scalar g = ALWAN_LITERAL(15.1927);

    alwan_scalar x = linear + c;
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

/* ================================================================
 * Blackmagic Film Gen 5
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_bmdfilm_oetf(alwan_scalar linear) {
    alwan_scalar linear_result = linear * ALWAN_LITERAL(8.283605932);
    alwan_scalar log_result    = ALWAN_LITERAL(0.5) * ALWAN_LN(linear + ALWAN_LITERAL(0.006)) / ALWAN_LN(ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(0.584);
    return ALWAN_SELECT(linear < ALWAN_LITERAL(0.005), linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_bmdfilm_eotf(alwan_scalar encoded) {
    alwan_scalar threshold = ALWAN_LITERAL(0.005) * ALWAN_LITERAL(8.283605932);

    alwan_scalar linear_result = encoded / ALWAN_LITERAL(8.283605932);
    alwan_scalar log_result    = ALWAN_POW(ALWAN_LITERAL(2.0), (encoded - ALWAN_LITERAL(0.584)) / ALWAN_LITERAL(0.5)) - ALWAN_LITERAL(0.006);
    return ALWAN_SELECT(encoded < threshold, linear_result, log_result);
}

/* ================================================================
 * Blackmagic Film Gen 4
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_bmdfilm4_oetf(alwan_scalar linear) {
    alwan_scalar A       = ALWAN_LITERAL(5.2212906000378565);
    alwan_scalar B       = ALWAN_LITERAL(-0.00007134598996420424);
    alwan_scalar C       = ALWAN_LITERAL(0.03630411093543444);
    alwan_scalar D       = ALWAN_LITERAL(0.21566456116952773);
    alwan_scalar E       = ALWAN_LITERAL(0.7133134738229736);
    alwan_scalar lin_cut = ALWAN_LITERAL(0.00500072683168086);

    alwan_scalar linear_result = linear * A + B;
    alwan_scalar log_result    = ALWAN_LN(linear + C) * D + E;
    return ALWAN_SELECT(linear <= lin_cut, linear_result, log_result);
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

/* ================================================================
 * Filmlight T-Log / E-Log
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_tlog_oetf(alwan_scalar linear) {
    alwan_scalar a = ALWAN_LITERAL(0.01);
    alwan_scalar b = ALWAN_LITERAL(0.0);
    alwan_scalar c = ALWAN_LITERAL(0.33);
    alwan_scalar d = ALWAN_LITERAL(0.02);

    alwan_scalar linear_result = linear / a * d;
    alwan_scalar log_result    = c * ALWAN_LN((linear + b) / a) / ALWAN_LN(ALWAN_LITERAL(10.0)) + d;
    return ALWAN_SELECT(linear <= a, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_tlog_eotf(alwan_scalar encoded) {
    alwan_scalar a = ALWAN_LITERAL(0.01);
    alwan_scalar b = ALWAN_LITERAL(0.0);
    alwan_scalar c = ALWAN_LITERAL(0.33);
    alwan_scalar d = ALWAN_LITERAL(0.02);

    alwan_scalar linear_result = encoded / d * a;
    alwan_scalar log_result    = ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - d) / c) * a - b;
    return ALWAN_SELECT(encoded <= d, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_elog_oetf(alwan_scalar linear) {
    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
    return ALWAN_LN(L + ALWAN_ONE) / ALWAN_LN(ALWAN_LITERAL(10.0)) * ALWAN_LITERAL(0.4) + ALWAN_LITERAL(0.6);
}

ALWAN_INLINE alwan_scalar alwan_elog_eotf(alwan_scalar encoded) {
    return ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - ALWAN_LITERAL(0.6)) / ALWAN_LITERAL(0.4)) - ALWAN_ONE;
}

/* ================================================================
 * GoPro Protune
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_protune_oetf(alwan_scalar linear) {
    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
    return ALWAN_LN(L * ALWAN_LITERAL(112.0) + ALWAN_ONE) / ALWAN_LN(ALWAN_LITERAL(113.0));
}

ALWAN_INLINE alwan_scalar alwan_protune_eotf(alwan_scalar encoded) {
    return (ALWAN_POW(ALWAN_LITERAL(113.0), encoded) - ALWAN_ONE) / ALWAN_LITERAL(112.0);
}

/* ================================================================
 * Standard Gamma Variants
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_gamma22_oetf(alwan_scalar linear) {
    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
    return ALWAN_POW(L, ALWAN_ONE / ALWAN_LITERAL(2.2));
}

ALWAN_INLINE alwan_scalar alwan_gamma22_eotf(alwan_scalar encoded) {
    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(E, ALWAN_LITERAL(2.2));
}

ALWAN_INLINE alwan_scalar alwan_gamma24_oetf(alwan_scalar linear) {
    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
    return ALWAN_POW(L, ALWAN_ONE / ALWAN_LITERAL(2.4));
}

ALWAN_INLINE alwan_scalar alwan_gamma24_eotf(alwan_scalar encoded) {
    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(E, ALWAN_LITERAL(2.4));
}

ALWAN_INLINE alwan_scalar alwan_gamma26_oetf(alwan_scalar linear) {
    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
    return ALWAN_POW(L, ALWAN_ONE / ALWAN_LITERAL(2.6));
}

ALWAN_INLINE alwan_scalar alwan_gamma26_eotf(alwan_scalar encoded) {
    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(E, ALWAN_LITERAL(2.6));
}

ALWAN_INLINE alwan_scalar alwan_gamma28_oetf(alwan_scalar linear) {
    alwan_scalar L = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
    return ALWAN_POW(L, ALWAN_ONE / ALWAN_LITERAL(2.8));
}

ALWAN_INLINE alwan_scalar alwan_gamma28_eotf(alwan_scalar encoded) {
    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(E, ALWAN_LITERAL(2.8));
}

/* ================================================================
 * Nikon N-Log
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_nlog_oetf(alwan_scalar linear) {
    alwan_scalar cut = ALWAN_LITERAL(0.00570);

    alwan_scalar linear_result = ALWAN_LITERAL(150.0) * linear + ALWAN_LITERAL(0.11241);
    alwan_scalar log_result    = (ALWAN_LITERAL(0.36) * ALWAN_LN(linear + ALWAN_LITERAL(0.10033)) / ALWAN_LN(ALWAN_LITERAL(10.0))) + ALWAN_LITERAL(0.71722);
    return ALWAN_SELECT(linear < cut, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_nlog_eotf(alwan_scalar encoded) {
    alwan_scalar cut       = ALWAN_LITERAL(0.00570);
    alwan_scalar threshold = ALWAN_LITERAL(150.0) * cut + ALWAN_LITERAL(0.11241);

    alwan_scalar linear_result = (encoded - ALWAN_LITERAL(0.11241)) / ALWAN_LITERAL(150.0);
    alwan_scalar log_result    = ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - ALWAN_LITERAL(0.71722)) / ALWAN_LITERAL(0.36)) - ALWAN_LITERAL(0.10033);
    return ALWAN_SELECT(encoded < threshold, linear_result, log_result);
}

/* ================================================================
 * Cineon / DPX Film Log
 * Reference: Sony Imageworks (2012)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_cineon_oetf(alwan_scalar linear) {
    alwan_scalar black_offset = ALWAN_LITERAL(0.010797751623277);
    alwan_scalar x = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);

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

/* ================================================================
 * Apple Log (iPhone 15 Pro+)
 * Reference: Apple Log Profile White Paper
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_apple_log_oetf(alwan_scalar linear) {
    alwan_scalar R0    = ALWAN_LITERAL(-0.05641088);
    alwan_scalar Rt    = ALWAN_LITERAL(0.01);
    alwan_scalar C     = ALWAN_LITERAL(47.28711236);
    alwan_scalar beta  = ALWAN_LITERAL(0.00964052);
    alwan_scalar gamma = ALWAN_LITERAL(0.08550479);
    alwan_scalar delta = ALWAN_LITERAL(0.69336945);

    alwan_scalar diff = linear - R0;
    alwan_scalar quad_result = C * diff * diff;
    alwan_scalar log_result  = gamma * ALWAN_LOG2(linear + beta) + delta;

    return ALWAN_SELECT(linear < R0, ALWAN_ZERO,
           ALWAN_SELECT(linear < Rt, quad_result, log_result));
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

/* ================================================================
 * Fujifilm F-Log
 * Reference: Fujifilm F-Log Data Sheet
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_flog_oetf(alwan_scalar linear) {
    alwan_scalar cut1 = ALWAN_LITERAL(0.00089);
    alwan_scalar A = ALWAN_LITERAL(0.555556);
    alwan_scalar B = ALWAN_LITERAL(0.009468);
    alwan_scalar C = ALWAN_LITERAL(0.344676);
    alwan_scalar D = ALWAN_LITERAL(0.790453);
    alwan_scalar E = ALWAN_LITERAL(8.735631);
    alwan_scalar F = ALWAN_LITERAL(0.092864);

    alwan_scalar linear_result = E * linear + F;
    alwan_scalar log_result    = C * ALWAN_LOG10(A * linear + B) + D;
    return ALWAN_SELECT(linear < cut1, linear_result, log_result);
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

/* ================================================================
 * Fujifilm F-Log2
 * Reference: Fujifilm F-Log2 Data Sheet
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_flog2_oetf(alwan_scalar linear) {
    alwan_scalar cut1 = ALWAN_LITERAL(0.000889);
    alwan_scalar A = ALWAN_LITERAL(5.555556);
    alwan_scalar B = ALWAN_LITERAL(0.064829);
    alwan_scalar C = ALWAN_LITERAL(0.245281);
    alwan_scalar D = ALWAN_LITERAL(0.384316);
    alwan_scalar E = ALWAN_LITERAL(8.799461);
    alwan_scalar F = ALWAN_LITERAL(0.092864);

    alwan_scalar linear_result = E * linear + F;
    alwan_scalar log_result    = C * ALWAN_LOG10(A * linear + B) + D;
    return ALWAN_SELECT(linear < cut1, linear_result, log_result);
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

/* ================================================================
 * Leica L-Log
 * Reference: Leica L-Log Reference Manual
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_llog_oetf(alwan_scalar linear) {
    alwan_scalar cut = ALWAN_LITERAL(0.01);
    alwan_scalar A = ALWAN_LITERAL(5.555556);
    alwan_scalar B = ALWAN_LITERAL(0.064);
    alwan_scalar C = ALWAN_LITERAL(0.241514);
    alwan_scalar D = ALWAN_LITERAL(0.598206);
    alwan_scalar E = ALWAN_LITERAL(5.6);
    alwan_scalar F = ALWAN_LITERAL(0.069);

    alwan_scalar linear_result = E * linear + F;
    alwan_scalar log_result    = C * ALWAN_LOG10(A * linear + B) + D;
    return ALWAN_SELECT(linear < cut, linear_result, log_result);
}

ALWAN_INLINE alwan_scalar alwan_llog_eotf(alwan_scalar encoded) {
    alwan_scalar cut_enc = ALWAN_LITERAL(0.125);
    alwan_scalar A = ALWAN_LITERAL(5.555556);
    alwan_scalar B = ALWAN_LITERAL(0.064);
    alwan_scalar C = ALWAN_LITERAL(0.241514);
    alwan_scalar D = ALWAN_LITERAL(0.598206);
    alwan_scalar E = ALWAN_LITERAL(5.6);
    alwan_scalar F = ALWAN_LITERAL(0.069);

    alwan_scalar linear_result = (encoded - F) / E;
    alwan_scalar log_result    = (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - D) / C) - B) / A;
    return ALWAN_SELECT(encoded < cut_enc, linear_result, log_result);
}

/* ================================================================
 * DJI D-Log
 * Reference: DJI Cinema Color System Technical Whitepaper
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_dlog_oetf(alwan_scalar linear) {
    alwan_scalar cut       = ALWAN_LITERAL(0.0078);
    alwan_scalar lin_slope = ALWAN_LITERAL(6.025);
    alwan_scalar lin_off   = ALWAN_LITERAL(0.0929);
    alwan_scalar log_mult  = ALWAN_LITERAL(0.256663);
    alwan_scalar log_off   = ALWAN_LITERAL(0.584555);
    alwan_scalar log_scale = ALWAN_LITERAL(0.9892);
    alwan_scalar log_bias  = ALWAN_LITERAL(0.0108);

    alwan_scalar linear_result = lin_slope * linear + lin_off;
    alwan_scalar log_result    = ALWAN_LOG10(linear * log_scale + log_bias) * log_mult + log_off;
    return ALWAN_SELECT(linear <= cut, linear_result, log_result);
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

/* ================================================================
 * DCDM (Digital Cinema Distribution Master) - SMPTE ST 428-1
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_dcdm_oetf(alwan_scalar linear) {
    alwan_scalar scale     = ALWAN_LITERAL(0.9165552797403094);   /* 48/52.37 */
    alwan_scalar inv_gamma = ALWAN_LITERAL(0.38461538461538464);  /* 1/2.6 */

    alwan_scalar L = ALWAN_SELECT(linear <= ALWAN_ZERO, ALWAN_ZERO, linear);
    return ALWAN_POW(L * scale, inv_gamma);
}

ALWAN_INLINE alwan_scalar alwan_dcdm_eotf(alwan_scalar encoded) {
    alwan_scalar inv_scale = ALWAN_LITERAL(1.0910416666666667);  /* 52.37/48 */
    alwan_scalar gamma     = ALWAN_LITERAL(2.6);

    alwan_scalar E = ALWAN_SELECT(encoded <= ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(E, gamma) * inv_scale;
}

/* ================================================================
 * Linear / Identity
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_linear_identity(alwan_scalar v) {
    return v;
}

/* ================================================================
 * ADX (Academy Density Exchange) - SMPTE ST 2065-3
 * ================================================================ */

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

/* ================================================================
 * Arbitrary Gamma Transfer Function
 *
 * Simple power-law gamma encoding/decoding with arbitrary exponent.
 * OETF: linear -> encoded = pow(max(0, linear), 1/gamma)
 * EOTF: encoded -> linear = pow(max(0, encoded), gamma)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_gamma_oetf_v(alwan_scalar linear, alwan_scalar gamma) {
    alwan_scalar safe = ALWAN_SELECT(linear < ALWAN_ZERO, ALWAN_ZERO, linear);
    return ALWAN_POW(safe, ALWAN_ONE / gamma);
}

ALWAN_INLINE alwan_scalar alwan_gamma_eotf_v(alwan_scalar encoded, alwan_scalar gamma) {
    alwan_scalar safe = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    return ALWAN_POW(safe, gamma);
}

/* ================================================================
 * DICOM GSDF (Grayscale Standard Display Function)
 *
 * DICOM Part 14: maps between luminance (cd/m2) and JND index.
 * Used for medical imaging display calibration.
 *
 * OETF: luminance -> JND index
 * EOTF: JND index -> luminance
 *
 * Polynomial coefficients from DICOM PS3.14 Table B-1:
 * log10(L) = a + b*log10(j) + c*(log10(j))^2 + d*(log10(j))^3 + ...
 * where j = JND index, L = luminance
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_dicom_gsdf_eotf_v(alwan_scalar jnd) {
    /* JND index to luminance (cd/m2)
     * DICOM PS3.14 Barten model: rational polynomial in ln(jnd)
     * log10(L) = (a + c*ln(j) + e*ln(j)^2 + g*ln(j)^3 + m*ln(j)^4)
     *          / (1 + b*ln(j) + d*ln(j)^2 + f*ln(j)^3 + h*ln(j)^4 + k*ln(j)^5)
     */
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
    /* Luminance (cd/m2) to JND index */
    /* Coefficients from DICOM PS3.14 inverse */
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

#endif /* ALWAN_RGB_CORE_H */
