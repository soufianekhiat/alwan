/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only RGB Transfer Functions (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Uses ALWAN_CORE_* macros (set by alwan_core_f32_setup.h or _f64_setup.h).
 */

/* ================================================================
 * xy chromaticity to XYZ (Y=1)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_xy_to_xyz)(ALWAN_CORE_T x, ALWAN_CORE_T y) {
    ALWAN_CORE_VEC3 result;
    ALWAN_CORE_T abs_y = ALWAN_CORE_ABS(y);
    ALWAN_CORE_T safe_y = ALWAN_CORE_SELECT(abs_y < ALWAN_CORE_EPSILON, ALWAN_CORE_ONE, y);
    ALWAN_CORE_T Y = ALWAN_CORE_ONE;

    result.v[0] = ALWAN_CORE_SELECT(abs_y < ALWAN_CORE_EPSILON, ALWAN_CORE_ZERO, (x / safe_y) * Y);
    result.v[1] = ALWAN_CORE_SELECT(abs_y < ALWAN_CORE_EPSILON, ALWAN_CORE_ZERO, Y);
    result.v[2] = ALWAN_CORE_SELECT(abs_y < ALWAN_CORE_EPSILON, ALWAN_CORE_ZERO,
                               ((ALWAN_CORE_ONE - x - y) / safe_y) * Y);
    return result;
}

/* ================================================================
 * RGB Matrix Derivation (value-returning)
 *
 * Computes RGB<->XYZ matrices from xy chromaticity primaries
 * and white point using the standard IEC 61966-2-1 method.
 * ================================================================ */

typedef struct {
    ALWAN_CORE_MAT3X3 rgb_to_xyz;
    ALWAN_CORE_MAT3X3 xyz_to_rgb;
} ALWAN_CORE_FNLIT(alwan_rgb_matrices);

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_rgb_matrices) ALWAN_CORE_FNVLIT(alwan_rgb_derive_matrices)(
    ALWAN_CORE_T rx, ALWAN_CORE_T ry,
    ALWAN_CORE_T gx, ALWAN_CORE_T gy,
    ALWAN_CORE_T bx, ALWAN_CORE_T by,
    ALWAN_CORE_T wx, ALWAN_CORE_T wy)
{
    ALWAN_CORE_FNLIT(alwan_rgb_matrices) result;

    /* Build chromaticity matrix P directly from (x, y, 1-x-y).
     * Unlike the XYZ-with-Y=1 approach (x/y, 1, (1-x-y)/y), this
     * avoids division by y and handles primaries with y=0
     * (e.g. DCDM XYZ, Max RGB, Xtreme RGB). */
    ALWAN_CORE_MAT3X3 M;
    M.m[0] = rx; M.m[1] = gx; M.m[2] = bx;
    M.m[3] = ry; M.m[4] = gy; M.m[5] = by;
    M.m[6] = ALWAN_CORE_ONE - rx - ry;
    M.m[7] = ALWAN_CORE_ONE - gx - gy;
    M.m[8] = ALWAN_CORE_ONE - bx - by;

    /* White point XYZ (Y=1) -- white point always has y > 0 */
    ALWAN_CORE_VEC3 W_XYZ = ALWAN_CORE_FNVLIT(alwan_xy_to_xyz)(wx, wy);

    /* Solve for scale factors: S = M^-1 * W */
    ALWAN_CORE_MAT3X3 M_inv = ALWAN_CORE_FNVLIT(alwan_mat3_inv)(M);
    ALWAN_CORE_VEC3 S = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M_inv, W_XYZ);

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
    result.xyz_to_rgb = ALWAN_CORE_FNVLIT(alwan_mat3_inv)(result.rgb_to_xyz);

    return result;
}

/* ================================================================
 * Legal Range / Full Range Conversion (10-bit)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_legal_to_full_10bit)(ALWAN_CORE_T legal) {
    return (legal - ALWAN_CORE_LITERAL(0.062561094819159)) / ALWAN_CORE_LITERAL(0.856304985337243);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_full_to_legal_10bit)(ALWAN_CORE_T full) {
    return full * ALWAN_CORE_LITERAL(0.856304985337243) + ALWAN_CORE_LITERAL(0.062561094819159);
}

/* ================================================================
 * ACESproxy Transfer Functions
 * Reference: Academy S-2013-001
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_acesproxy_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T mid_gray_in     = ALWAN_CORE_LITERAL(0.18);
    ALWAN_CORE_T mid_code_value  = ALWAN_CORE_LITERAL(425.0) / ALWAN_CORE_LITERAL(1023.0);
    ALWAN_CORE_T steps_per_stop  = ALWAN_CORE_LITERAL(50.0) / ALWAN_CORE_LITERAL(1023.0);
    ALWAN_CORE_T min_code        = ALWAN_CORE_LITERAL(64.0) / ALWAN_CORE_LITERAL(1023.0);

    ALWAN_CORE_T log_val    = ALWAN_CORE_LN(linear / mid_gray_in) / ALWAN_CORE_LN(ALWAN_CORE_LITERAL(2.0));
    ALWAN_CORE_T log_result = mid_code_value + steps_per_stop * log_val;
    return ALWAN_CORE_SELECT(linear <= ALWAN_CORE_ZERO, min_code, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_acesproxy_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T mid_gray_in     = ALWAN_CORE_LITERAL(0.18);
    ALWAN_CORE_T mid_code_value  = ALWAN_CORE_LITERAL(425.0) / ALWAN_CORE_LITERAL(1023.0);
    ALWAN_CORE_T steps_per_stop  = ALWAN_CORE_LITERAL(50.0) / ALWAN_CORE_LITERAL(1023.0);

    ALWAN_CORE_T log_val = (encoded - mid_code_value) / steps_per_stop;
    return mid_gray_in * ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), log_val);
}

/* ================================================================
 * ACEScc Transfer Functions (SMPTE ST 2065-5)
 * Reference: Academy S-2014-003
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_acescc_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T min_cutoff = ALWAN_CORE_LITERAL(0.00003051757812);  /* 2^-15 */
    ALWAN_CORE_T log2_e     = ALWAN_CORE_LITERAL(1.4426950408889634);

    ALWAN_CORE_T neg_result  = (ALWAN_CORE_LITERAL(-16.0) + ALWAN_CORE_LITERAL(9.72)) / ALWAN_CORE_LITERAL(17.52);
    ALWAN_CORE_T val         = ALWAN_CORE_LITERAL(0.0000152587890625) + linear * ALWAN_CORE_LITERAL(0.5);
    ALWAN_CORE_T low_result  = (ALWAN_CORE_LN(val) * log2_e + ALWAN_CORE_LITERAL(9.72)) / ALWAN_CORE_LITERAL(17.52);
    ALWAN_CORE_T high_result = (ALWAN_CORE_LN(linear) * log2_e + ALWAN_CORE_LITERAL(9.72)) / ALWAN_CORE_LITERAL(17.52);

    return ALWAN_CORE_SELECT(linear <= ALWAN_CORE_ZERO, neg_result,
           ALWAN_CORE_SELECT(linear < min_cutoff, low_result, high_result));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_acescc_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T log2_e     = ALWAN_CORE_LITERAL(1.4426950408889634);
    ALWAN_CORE_T neg_cutoff = (ALWAN_CORE_LITERAL(9.72) - ALWAN_CORE_LITERAL(15.0)) / ALWAN_CORE_LITERAL(17.52);
    ALWAN_CORE_T max_cutoff = (ALWAN_CORE_LN(ALWAN_CORE_LITERAL(65504.0)) * log2_e + ALWAN_CORE_LITERAL(9.72)) / ALWAN_CORE_LITERAL(17.52);

    ALWAN_CORE_T neg_result = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), encoded * ALWAN_CORE_LITERAL(17.52) - ALWAN_CORE_LITERAL(9.72)) - ALWAN_CORE_LITERAL(0.0000152587890625)) * ALWAN_CORE_LITERAL(2.0);
    ALWAN_CORE_T mid_result = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), encoded * ALWAN_CORE_LITERAL(17.52) - ALWAN_CORE_LITERAL(9.72));
    ALWAN_CORE_T max_result = ALWAN_CORE_LITERAL(65504.0);

    return ALWAN_CORE_SELECT(encoded < neg_cutoff, neg_result,
           ALWAN_CORE_SELECT(encoded < max_cutoff, mid_result, max_result));
}

/* ================================================================
 * ACEScct Transfer Functions (SMPTE ST 2065-5)
 * Reference: Academy S-2016-001
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_acescct_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T cut   = ALWAN_CORE_LITERAL(0.0078125);  /* 2^-7 */
    ALWAN_CORE_T A     = ALWAN_CORE_LITERAL(10.5402377416545);
    ALWAN_CORE_T B     = ALWAN_CORE_LITERAL(0.0729055341958355);
    ALWAN_CORE_T log2_e = ALWAN_CORE_LITERAL(1.4426950408889634);

    ALWAN_CORE_T linear_result = A * linear + B;
    ALWAN_CORE_T log_result    = (ALWAN_CORE_LN(linear) * log2_e + ALWAN_CORE_LITERAL(9.72)) / ALWAN_CORE_LITERAL(17.52);
    return ALWAN_CORE_SELECT(linear <= cut, linear_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_acescct_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T cut_enc = ALWAN_CORE_LITERAL(0.155251141552511);
    ALWAN_CORE_T A       = ALWAN_CORE_LITERAL(10.5402377416545);
    ALWAN_CORE_T B       = ALWAN_CORE_LITERAL(0.0729055341958355);

    ALWAN_CORE_T linear_result = (encoded - B) / A;
    ALWAN_CORE_T log_result    = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), encoded * ALWAN_CORE_LITERAL(17.52) - ALWAN_CORE_LITERAL(9.72));
    return ALWAN_CORE_SELECT(encoded <= cut_enc, linear_result, log_result);
}

/* ================================================================
 * Sony S-Log Family
 * Reference: SonyCorporation2012a
 * ================================================================ */

/* S-Log OETF: linear -> S-Log (scene-referred, legal range output) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_slog_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T x = linear / ALWAN_CORE_LITERAL(0.9);

    ALWAN_CORE_T log_result    = ALWAN_CORE_LITERAL(0.432699) * ALWAN_CORE_LOG10(x + ALWAN_CORE_LITERAL(0.037584)) + ALWAN_CORE_LITERAL(0.616596) + ALWAN_CORE_LITERAL(0.03);
    ALWAN_CORE_T linear_result = x * ALWAN_CORE_LITERAL(5.0) + ALWAN_CORE_LITERAL(0.030001222851889303);
    ALWAN_CORE_T y_full = ALWAN_CORE_SELECT(x >= ALWAN_CORE_ZERO, log_result, linear_result);

    return ALWAN_CORE_FNLIT(alwan_full_to_legal_10bit)(y_full);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_slog_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T y_full    = ALWAN_CORE_FNLIT(alwan_legal_to_full_10bit)(encoded);
    ALWAN_CORE_T threshold = ALWAN_CORE_LITERAL(0.030001222851889303);

    ALWAN_CORE_T log_result    = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), (y_full - ALWAN_CORE_LITERAL(0.646596)) / ALWAN_CORE_LITERAL(0.432699)) - ALWAN_CORE_LITERAL(0.037584);
    ALWAN_CORE_T linear_result = (y_full - ALWAN_CORE_LITERAL(0.030001222851889303)) / ALWAN_CORE_LITERAL(5.0);
    ALWAN_CORE_T x = ALWAN_CORE_SELECT(y_full >= threshold, log_result, linear_result);

    return x * ALWAN_CORE_LITERAL(0.9);
}

/* S-Log2: S-Log with 155/219 scaling */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_slog2_oetf)(ALWAN_CORE_T linear) {
    return ALWAN_CORE_FNLIT(alwan_slog_oetf)(linear * ALWAN_CORE_LITERAL(155.0) / ALWAN_CORE_LITERAL(219.0));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_slog2_eotf)(ALWAN_CORE_T encoded) {
    return ALWAN_CORE_FNLIT(alwan_slog_eotf)(encoded) * ALWAN_CORE_LITERAL(219.0) / ALWAN_CORE_LITERAL(155.0);
}

/* S-Log3 OETF: linear -> S-Log3 (legal range output) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_slog3_oetf)(ALWAN_CORE_T linear) {
    /* No pre-clamp: negative inputs use the linear segment (linear < 0.01125000),
     * giving a negative encoded output that callers can clip as needed. */
    ALWAN_CORE_T log_result    = (ALWAN_CORE_LITERAL(420.0) + ALWAN_CORE_LOG10((linear + ALWAN_CORE_LITERAL(0.01)) / ALWAN_CORE_LITERAL(0.19)) * ALWAN_CORE_LITERAL(261.5)) / ALWAN_CORE_LITERAL(1023.0);
    ALWAN_CORE_T linear_result = (linear * (ALWAN_CORE_LITERAL(171.2102946929) - ALWAN_CORE_LITERAL(95.0)) / ALWAN_CORE_LITERAL(0.01125000) + ALWAN_CORE_LITERAL(95.0)) / ALWAN_CORE_LITERAL(1023.0);

    return ALWAN_CORE_SELECT(linear >= ALWAN_CORE_LITERAL(0.01125000), log_result, linear_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_slog3_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T code = encoded * ALWAN_CORE_LITERAL(1023.0);

    ALWAN_CORE_T log_result    = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), ((code - ALWAN_CORE_LITERAL(420.0)) / ALWAN_CORE_LITERAL(261.5))) * ALWAN_CORE_LITERAL(0.19)) - ALWAN_CORE_LITERAL(0.01);
    ALWAN_CORE_T linear_result = ((code - ALWAN_CORE_LITERAL(95.0)) / (ALWAN_CORE_LITERAL(171.2102946929) - ALWAN_CORE_LITERAL(95.0))) * ALWAN_CORE_LITERAL(0.01125000);
    return ALWAN_CORE_SELECT(code >= ALWAN_CORE_LITERAL(171.2102946929), log_result, linear_result);
}

/* ================================================================
 * Canon C-Log Family
 * Reference: CanonInc2012, CanonInc2016
 * ================================================================ */

/* C-Log v1.2 */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_clog_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.45310179);
    ALWAN_CORE_T k = ALWAN_CORE_LITERAL(10.1596);
    ALWAN_CORE_T offset = ALWAN_CORE_LITERAL(0.12512248);

    ALWAN_CORE_T L = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    ALWAN_CORE_T x = L / ALWAN_CORE_LITERAL(0.9);

    return a * ALWAN_CORE_LOG10(k * x + ALWAN_CORE_ONE) + offset;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_clog_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.45310179);
    ALWAN_CORE_T k = ALWAN_CORE_LITERAL(10.1596);
    ALWAN_CORE_T offset = ALWAN_CORE_LITERAL(0.12512248);

    ALWAN_CORE_T x = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), (encoded - offset) / a) - ALWAN_CORE_ONE) / k;
    return x * ALWAN_CORE_LITERAL(0.9);
}

/* C-Log2 v1.2 */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_clog2_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.24136077);
    ALWAN_CORE_T k = ALWAN_CORE_LITERAL(87.09937546);
    ALWAN_CORE_T offset = ALWAN_CORE_LITERAL(0.092864125);

    ALWAN_CORE_T L = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    ALWAN_CORE_T x = L / ALWAN_CORE_LITERAL(0.9);

    return a * ALWAN_CORE_LOG10(k * x + ALWAN_CORE_ONE) + offset;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_clog2_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.24136077);
    ALWAN_CORE_T k = ALWAN_CORE_LITERAL(87.09937546);
    ALWAN_CORE_T offset = ALWAN_CORE_LITERAL(0.092864125);

    ALWAN_CORE_T x = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), (encoded - offset) / a) - ALWAN_CORE_ONE) / k;
    return x * ALWAN_CORE_LITERAL(0.9);
}

/* C-Log3 (3-region piecewise) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_clog3_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T x = linear / ALWAN_CORE_LITERAL(0.9);

    ALWAN_CORE_T x_threshold_low  = ALWAN_CORE_LITERAL(-0.014);
    ALWAN_CORE_T x_threshold_high = ALWAN_CORE_LITERAL(0.014);

    ALWAN_CORE_T neg_result    = ALWAN_CORE_LITERAL(-0.36726845) * ALWAN_CORE_LOG10(-x * ALWAN_CORE_LITERAL(14.98325) + ALWAN_CORE_ONE) + ALWAN_CORE_LITERAL(0.12783901);
    ALWAN_CORE_T linear_result = ALWAN_CORE_LITERAL(1.9754798) * x + ALWAN_CORE_LITERAL(0.12512219);
    ALWAN_CORE_T pos_result    = ALWAN_CORE_LITERAL(0.36726845) * ALWAN_CORE_LOG10(x * ALWAN_CORE_LITERAL(14.98325) + ALWAN_CORE_ONE) + ALWAN_CORE_LITERAL(0.12240537);

    return ALWAN_CORE_SELECT(x < x_threshold_low, neg_result,
           ALWAN_CORE_SELECT(x <= x_threshold_high, linear_result, pos_result));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_clog3_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T threshold_low  = ALWAN_CORE_LITERAL(0.097465473);
    ALWAN_CORE_T threshold_high = ALWAN_CORE_LITERAL(0.15277891);

    ALWAN_CORE_T neg_result    = -(ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), (ALWAN_CORE_LITERAL(0.12783901) - encoded) / ALWAN_CORE_LITERAL(0.36726845)) - ALWAN_CORE_ONE) / ALWAN_CORE_LITERAL(14.98325);
    ALWAN_CORE_T linear_result = (encoded - ALWAN_CORE_LITERAL(0.12512219)) / ALWAN_CORE_LITERAL(1.9754798);
    ALWAN_CORE_T pos_result    = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), (encoded - ALWAN_CORE_LITERAL(0.12240537)) / ALWAN_CORE_LITERAL(0.36726845)) - ALWAN_CORE_ONE) / ALWAN_CORE_LITERAL(14.98325);

    ALWAN_CORE_T x = ALWAN_CORE_SELECT(encoded < threshold_low, neg_result,
                     ALWAN_CORE_SELECT(encoded <= threshold_high, linear_result, pos_result));

    return x * ALWAN_CORE_LITERAL(0.9);
}

/* ================================================================
 * Panasonic V-Log
 * Reference: PanasonicCorporation2014
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_vlog_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T cut1 = ALWAN_CORE_LITERAL(0.01);
    ALWAN_CORE_T b = ALWAN_CORE_LITERAL(0.00873);
    ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.241514);
    ALWAN_CORE_T d = ALWAN_CORE_LITERAL(0.598206);

    ALWAN_CORE_T linear_result = ALWAN_CORE_LITERAL(5.6) * linear + ALWAN_CORE_LITERAL(0.125);
    ALWAN_CORE_T log_result    = c * ALWAN_CORE_LOG10(linear + b) + d;

    return ALWAN_CORE_SELECT(linear <= cut1, linear_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_vlog_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T cut2 = ALWAN_CORE_LITERAL(0.181);
    ALWAN_CORE_T b = ALWAN_CORE_LITERAL(0.00873);
    ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.241514);
    ALWAN_CORE_T d = ALWAN_CORE_LITERAL(0.598206);

    ALWAN_CORE_T linear_result = (encoded - ALWAN_CORE_LITERAL(0.125)) / ALWAN_CORE_LITERAL(5.6);
    ALWAN_CORE_T log_result    = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), (encoded - d) / c) - b;

    return ALWAN_CORE_SELECT(encoded <= cut2, linear_result, log_result);
}

/* ================================================================
 * ARRI LogC Family
 * ================================================================ */

/* LogC3 (EI 800) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_logc3_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(5.555556);
    ALWAN_CORE_T b = ALWAN_CORE_LITERAL(0.052272);
    ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.247190);
    ALWAN_CORE_T d = ALWAN_CORE_LITERAL(0.385537);
    ALWAN_CORE_T e = ALWAN_CORE_LITERAL(5.367655);
    ALWAN_CORE_T f = ALWAN_CORE_LITERAL(0.092809);

    ALWAN_CORE_T log_result    = c * ALWAN_CORE_LOG10(a * linear + b) + d;
    ALWAN_CORE_T linear_result = e * linear + f;
    return ALWAN_CORE_SELECT(linear > ALWAN_CORE_LITERAL(0.010591), log_result, linear_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_logc3_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(5.555556);
    ALWAN_CORE_T b = ALWAN_CORE_LITERAL(0.052272);
    ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.247190);
    ALWAN_CORE_T d = ALWAN_CORE_LITERAL(0.385537);
    ALWAN_CORE_T e = ALWAN_CORE_LITERAL(5.367655);
    ALWAN_CORE_T f = ALWAN_CORE_LITERAL(0.092809);

    ALWAN_CORE_T threshold = e * ALWAN_CORE_LITERAL(0.010591) + f;

    ALWAN_CORE_T linear_result = (encoded - f) / e;
    ALWAN_CORE_T log_result    = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), (encoded - d) / c) - b) / a;
    return ALWAN_CORE_SELECT(encoded < threshold, linear_result, log_result);
}

/* LogC4 -- ARRI LogC4 (Cooper 2022) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_logc4_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(2231.8263090676883);
    ALWAN_CORE_T b = ALWAN_CORE_LITERAL(0.9071358748778103);
    ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.09286412512218964);
    ALWAN_CORE_T s = ALWAN_CORE_LITERAL(0.1135972086105891);
    ALWAN_CORE_T t = ALWAN_CORE_LITERAL(-0.01805699611991131);

    ALWAN_CORE_T log_val = (ALWAN_CORE_LOG2(a * linear + ALWAN_CORE_LITERAL(64.0)) - ALWAN_CORE_LITERAL(6.0))
                         / ALWAN_CORE_LITERAL(14.0) * b + c;
    ALWAN_CORE_T lin_val = (linear - t) / s;
    return ALWAN_CORE_SELECT(linear < t, lin_val, log_val);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_logc4_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(2231.8263090676883);
    ALWAN_CORE_T b = ALWAN_CORE_LITERAL(0.9071358748778103);
    ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.09286412512218964);
    ALWAN_CORE_T s = ALWAN_CORE_LITERAL(0.1135972086105891);
    ALWAN_CORE_T t = ALWAN_CORE_LITERAL(-0.01805699611991131);

    ALWAN_CORE_T scene = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0),
        (encoded - c) / b * ALWAN_CORE_LITERAL(14.0) + ALWAN_CORE_LITERAL(6.0)) - ALWAN_CORE_LITERAL(64.0)) / a;
    ALWAN_CORE_T lin_val = encoded * s + t;
    return ALWAN_CORE_SELECT(encoded < ALWAN_CORE_ZERO, lin_val, scene);
}

/* ================================================================
 * RED Log Family
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_redlog_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T L = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    return (ALWAN_CORE_LOG10(L * ALWAN_CORE_LITERAL(0.9) + ALWAN_CORE_LITERAL(0.1)) + ALWAN_CORE_LITERAL(3.0)) / ALWAN_CORE_LITERAL(3.0);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_redlog_eotf)(ALWAN_CORE_T encoded) {
    return (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), encoded * ALWAN_CORE_LITERAL(3.0) - ALWAN_CORE_LITERAL(3.0)) - ALWAN_CORE_LITERAL(0.1)) / ALWAN_CORE_LITERAL(0.9);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_redlogfilm_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T L = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    return (ALWAN_CORE_LOG10(L * ALWAN_CORE_LITERAL(0.8) + ALWAN_CORE_LITERAL(0.1)) + ALWAN_CORE_LITERAL(3.0)) / ALWAN_CORE_LITERAL(3.0);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_redlogfilm_eotf)(ALWAN_CORE_T encoded) {
    return (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), encoded * ALWAN_CORE_LITERAL(3.0) - ALWAN_CORE_LITERAL(3.0)) - ALWAN_CORE_LITERAL(0.1)) / ALWAN_CORE_LITERAL(0.8);
}

/* Log3G10 (RED whitepaper 915-0187 Rev-C) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_log3g10_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.224282);
    ALWAN_CORE_T b = ALWAN_CORE_LITERAL(155.975327);
    ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.01);
    ALWAN_CORE_T g = ALWAN_CORE_LITERAL(15.1927);

    ALWAN_CORE_T x = linear + c;
    ALWAN_CORE_T linear_result = x * g;
    ALWAN_CORE_T log_result    = a * ALWAN_CORE_LOG10(x * b + ALWAN_CORE_ONE);
    return ALWAN_CORE_SELECT(x < ALWAN_CORE_ZERO, linear_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_log3g10_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.224282);
    ALWAN_CORE_T b = ALWAN_CORE_LITERAL(155.975327);
    ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.01);
    ALWAN_CORE_T g = ALWAN_CORE_LITERAL(15.1927);

    ALWAN_CORE_T linear_result = (encoded / g) - c;
    ALWAN_CORE_T log_result    = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), encoded / a) - ALWAN_CORE_ONE) / b - c;
    return ALWAN_CORE_SELECT(encoded < ALWAN_CORE_ZERO, linear_result, log_result);
}

/* ================================================================
 * Blackmagic Film Gen 5
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_bmdfilm_oetf)(ALWAN_CORE_T linear) {
    /* A*ln(x+B)+C  for x >= LIN_CUT;  D*x+E  for x < LIN_CUT */
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(0.08692876065491224);
    ALWAN_CORE_T B = ALWAN_CORE_LITERAL(0.005494072432257808);
    ALWAN_CORE_T C = ALWAN_CORE_LITERAL(0.5300133392291939);
    ALWAN_CORE_T D = ALWAN_CORE_LITERAL(8.283605932402494);
    ALWAN_CORE_T E = ALWAN_CORE_LITERAL(0.09246575342465753);
    ALWAN_CORE_T linear_result = D * linear + E;
    ALWAN_CORE_T log_result    = A * ALWAN_CORE_LN(linear + B) + C;
    return ALWAN_CORE_SELECT(linear < ALWAN_CORE_LITERAL(0.005), linear_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_bmdfilm_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(0.08692876065491224);
    ALWAN_CORE_T B = ALWAN_CORE_LITERAL(0.005494072432257808);
    ALWAN_CORE_T C = ALWAN_CORE_LITERAL(0.5300133392291939);
    ALWAN_CORE_T D = ALWAN_CORE_LITERAL(8.283605932402494);
    ALWAN_CORE_T E = ALWAN_CORE_LITERAL(0.09246575342465753);
    ALWAN_CORE_T threshold     = D * ALWAN_CORE_LITERAL(0.005) + E;
    ALWAN_CORE_T linear_result = (encoded - E) / D;
    ALWAN_CORE_T log_result    = ALWAN_CORE_EXP((encoded - C) / A) - B;
    return ALWAN_CORE_SELECT(encoded < threshold, linear_result, log_result);
}

/* ================================================================
 * Blackmagic Film Gen 4
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_bmdfilm4_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T A       = ALWAN_CORE_LITERAL(5.2212906000378565);
    ALWAN_CORE_T B       = ALWAN_CORE_LITERAL(-0.00007134598996420424);
    ALWAN_CORE_T C       = ALWAN_CORE_LITERAL(0.03630411093543444);
    ALWAN_CORE_T D       = ALWAN_CORE_LITERAL(0.21566456116952773);
    ALWAN_CORE_T E       = ALWAN_CORE_LITERAL(0.7133134738229736);
    ALWAN_CORE_T lin_cut = ALWAN_CORE_LITERAL(0.00500072683168086);

    ALWAN_CORE_T linear_result = linear * A + B;
    ALWAN_CORE_T log_result    = ALWAN_CORE_LN(linear + C) * D + E;
    return ALWAN_CORE_SELECT(linear <= lin_cut, linear_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_bmdfilm4_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T A       = ALWAN_CORE_LITERAL(5.2212906000378565);
    ALWAN_CORE_T B       = ALWAN_CORE_LITERAL(-0.00007134598996420424);
    ALWAN_CORE_T D       = ALWAN_CORE_LITERAL(0.21566456116952773);
    ALWAN_CORE_T C       = ALWAN_CORE_LITERAL(0.03630411093543444);
    ALWAN_CORE_T E       = ALWAN_CORE_LITERAL(0.7133134738229736);
    ALWAN_CORE_T log_cut = ALWAN_CORE_LITERAL(0.026038902009648163);

    ALWAN_CORE_T linear_result = (encoded - B) / A;
    ALWAN_CORE_T log_result    = ALWAN_CORE_EXP((encoded - E) / D) - C;
    return ALWAN_CORE_SELECT(encoded <= log_cut, linear_result, log_result);
}

/* ================================================================
 * FilmLight T-Log, Olympus OM-Log400 (E-Log)
 * ================================================================ */

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
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_tlog_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(0.55201265686066547);
    ALWAN_CORE_T B = ALWAN_CORE_LITERAL(0.092329025965773526);
    ALWAN_CORE_T C = ALWAN_CORE_LITERAL(0.0057048244042473785);
    ALWAN_CORE_T G = ALWAN_CORE_LITERAL(16.184376489665897);
    ALWAN_CORE_T o = ALWAN_CORE_LITERAL(0.075);

    ALWAN_CORE_T linear_result = linear * G + o;
    ALWAN_CORE_T log_result    = ALWAN_CORE_LN(linear + C) * B + A;
    return ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, linear_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_tlog_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(0.55201265686066547);
    ALWAN_CORE_T B = ALWAN_CORE_LITERAL(0.092329025965773526);
    ALWAN_CORE_T C = ALWAN_CORE_LITERAL(0.0057048244042473785);
    ALWAN_CORE_T G = ALWAN_CORE_LITERAL(16.184376489665897);
    ALWAN_CORE_T o = ALWAN_CORE_LITERAL(0.075);

    ALWAN_CORE_T linear_result = (encoded - o) / G;
    ALWAN_CORE_T log_result    = ALWAN_CORE_EXP((encoded - A) / B) - C;
    return ALWAN_CORE_SELECT(encoded < o, linear_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_elog_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T L = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    return ALWAN_CORE_LN(L + ALWAN_CORE_ONE) / ALWAN_CORE_LN(ALWAN_CORE_LITERAL(10.0)) * ALWAN_CORE_LITERAL(0.4) + ALWAN_CORE_LITERAL(0.6);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_elog_eotf)(ALWAN_CORE_T encoded) {
    return ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), (encoded - ALWAN_CORE_LITERAL(0.6)) / ALWAN_CORE_LITERAL(0.4)) - ALWAN_CORE_ONE;
}

/* ================================================================
 * GoPro Protune
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_protune_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T L = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    return ALWAN_CORE_LN(L * ALWAN_CORE_LITERAL(112.0) + ALWAN_CORE_ONE) / ALWAN_CORE_LN(ALWAN_CORE_LITERAL(113.0));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_protune_eotf)(ALWAN_CORE_T encoded) {
    return (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(113.0), encoded) - ALWAN_CORE_ONE) / ALWAN_CORE_LITERAL(112.0);
}

/* ================================================================
 * Standard Gamma Variants
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_gamma22_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T L = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    return ALWAN_CORE_POW(L, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(2.2));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_gamma22_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T E = ALWAN_CORE_SELECT(encoded < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, encoded);
    return ALWAN_CORE_POW(E, ALWAN_CORE_LITERAL(2.2));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_gamma24_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T L = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    return ALWAN_CORE_POW(L, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(2.4));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_gamma24_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T E = ALWAN_CORE_SELECT(encoded < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, encoded);
    return ALWAN_CORE_POW(E, ALWAN_CORE_LITERAL(2.4));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_gamma26_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T L = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    return ALWAN_CORE_POW(L, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(2.6));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_gamma26_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T E = ALWAN_CORE_SELECT(encoded < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, encoded);
    return ALWAN_CORE_POW(E, ALWAN_CORE_LITERAL(2.6));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_gamma28_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T L = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    return ALWAN_CORE_POW(L, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(2.8));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_gamma28_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T E = ALWAN_CORE_SELECT(encoded < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, encoded);
    return ALWAN_CORE_POW(E, ALWAN_CORE_LITERAL(2.8));
}

/* ================================================================
 * Nikon N-Log
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_nlog_oetf)(ALWAN_CORE_T linear) {
    /* a*(y+b)^(1/3)  for y < cut1;  c*ln(y)+d  for y >= cut1
     * Signed cube root: follows colour-science spow convention so that
     * linear < -b gives a negative result (clipped to 0 by callers). */
    ALWAN_CORE_T a    = ALWAN_CORE_LITERAL(0.635386119257087);
    ALWAN_CORE_T b    = ALWAN_CORE_LITERAL(0.0075);
    ALWAN_CORE_T c    = ALWAN_CORE_LITERAL(0.1466275659824047);
    ALWAN_CORE_T d    = ALWAN_CORE_LITERAL(0.6050830889540567);
    ALWAN_CORE_T cut1 = ALWAN_CORE_LITERAL(0.328);
    ALWAN_CORE_T arg      = linear + b;
    ALWAN_CORE_T abs_arg  = ALWAN_CORE_SELECT(arg < ALWAN_CORE_ZERO, -arg, arg);
    ALWAN_CORE_T sign_arg = ALWAN_CORE_SELECT(arg < ALWAN_CORE_ZERO, -ALWAN_CORE_ONE, ALWAN_CORE_ONE);
    ALWAN_CORE_T linear_result = a * sign_arg * ALWAN_CORE_POW(abs_arg, ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(3.0));
    ALWAN_CORE_T log_result    = c * ALWAN_CORE_LN(linear) + d;
    return ALWAN_CORE_SELECT(linear < cut1, linear_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_nlog_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T a    = ALWAN_CORE_LITERAL(0.635386119257087);
    ALWAN_CORE_T b    = ALWAN_CORE_LITERAL(0.0075);
    ALWAN_CORE_T c    = ALWAN_CORE_LITERAL(0.1466275659824047);
    ALWAN_CORE_T d    = ALWAN_CORE_LITERAL(0.6050830889540567);
    ALWAN_CORE_T cut2 = ALWAN_CORE_LITERAL(0.4418377321603128);
    ALWAN_CORE_T linear_result = ALWAN_CORE_POW(encoded / a, ALWAN_CORE_LITERAL(3.0)) - b;
    ALWAN_CORE_T log_result    = ALWAN_CORE_EXP((encoded - d) / c);
    return ALWAN_CORE_SELECT(encoded < cut2, linear_result, log_result);
}

/* ================================================================
 * Cineon / DPX Film Log
 * Reference: Sony Imageworks (2012)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_cineon_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T black_offset = ALWAN_CORE_LITERAL(0.010797751623277);
    ALWAN_CORE_T x = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);

    ALWAN_CORE_T arg_raw = x * (ALWAN_CORE_ONE - black_offset) + black_offset;
    ALWAN_CORE_T arg = ALWAN_CORE_SELECT(arg_raw <= ALWAN_CORE_ZERO, ALWAN_CORE_LITERAL(1e-10), arg_raw);

    return (ALWAN_CORE_LITERAL(685.0) + ALWAN_CORE_LITERAL(300.0) * ALWAN_CORE_LOG10(arg)) / ALWAN_CORE_LITERAL(1023.0);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_cineon_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T black_offset = ALWAN_CORE_LITERAL(0.010797751623277);
    ALWAN_CORE_T exponent = (ALWAN_CORE_LITERAL(1023.0) * encoded - ALWAN_CORE_LITERAL(685.0)) / ALWAN_CORE_LITERAL(300.0);
    return (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), exponent) - black_offset) /
           (ALWAN_CORE_ONE - black_offset);
}

/* ================================================================
 * Apple Log (iPhone 15 Pro+)
 * Reference: Apple Log Profile White Paper
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_apple_log_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T R0    = ALWAN_CORE_LITERAL(-0.05641088);
    ALWAN_CORE_T Rt    = ALWAN_CORE_LITERAL(0.01);
    ALWAN_CORE_T C     = ALWAN_CORE_LITERAL(47.28711236);
    ALWAN_CORE_T beta  = ALWAN_CORE_LITERAL(0.00964052);
    ALWAN_CORE_T gamma = ALWAN_CORE_LITERAL(0.08550479);
    ALWAN_CORE_T delta = ALWAN_CORE_LITERAL(0.69336945);

    ALWAN_CORE_T diff = linear - R0;
    ALWAN_CORE_T quad_result = C * diff * diff;
    ALWAN_CORE_T log_result  = gamma * ALWAN_CORE_LOG2(linear + beta) + delta;

    return ALWAN_CORE_SELECT(linear < R0, ALWAN_CORE_ZERO,
           ALWAN_CORE_SELECT(linear < Rt, quad_result, log_result));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_apple_log_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T R0    = ALWAN_CORE_LITERAL(-0.05641088);
    ALWAN_CORE_T C     = ALWAN_CORE_LITERAL(47.28711236);
    ALWAN_CORE_T beta  = ALWAN_CORE_LITERAL(0.00964052);
    ALWAN_CORE_T gamma = ALWAN_CORE_LITERAL(0.08550479);
    ALWAN_CORE_T delta = ALWAN_CORE_LITERAL(0.69336945);
    ALWAN_CORE_T Pt    = ALWAN_CORE_LITERAL(0.20855531595464202);

    ALWAN_CORE_T sqrt_result = ALWAN_CORE_SQRT(encoded / C) + R0;
    ALWAN_CORE_T pow_result  = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), (encoded - delta) / gamma) - beta;

    return ALWAN_CORE_SELECT(encoded < ALWAN_CORE_ZERO, R0,
           ALWAN_CORE_SELECT(encoded < Pt, sqrt_result, pow_result));
}

/* ================================================================
 * Fujifilm F-Log
 * Reference: Fujifilm F-Log Data Sheet
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_flog_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T cut1 = ALWAN_CORE_LITERAL(0.00089);
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(0.555556);
    ALWAN_CORE_T B = ALWAN_CORE_LITERAL(0.009468);
    ALWAN_CORE_T C = ALWAN_CORE_LITERAL(0.344676);
    ALWAN_CORE_T D = ALWAN_CORE_LITERAL(0.790453);
    ALWAN_CORE_T E = ALWAN_CORE_LITERAL(8.735631);
    ALWAN_CORE_T F = ALWAN_CORE_LITERAL(0.092864);

    ALWAN_CORE_T linear_result = E * linear + F;
    ALWAN_CORE_T log_result    = C * ALWAN_CORE_LOG10(A * linear + B) + D;
    return ALWAN_CORE_SELECT(linear < cut1, linear_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_flog_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T cut2 = ALWAN_CORE_LITERAL(0.100537775223865);
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(0.555556);
    ALWAN_CORE_T B = ALWAN_CORE_LITERAL(0.009468);
    ALWAN_CORE_T C = ALWAN_CORE_LITERAL(0.344676);
    ALWAN_CORE_T D = ALWAN_CORE_LITERAL(0.790453);
    ALWAN_CORE_T E = ALWAN_CORE_LITERAL(8.735631);
    ALWAN_CORE_T F = ALWAN_CORE_LITERAL(0.092864);

    ALWAN_CORE_T linear_result = (encoded - F) / E;
    ALWAN_CORE_T log_result    = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), (encoded - D) / C) - B) / A;
    return ALWAN_CORE_SELECT(encoded < cut2, linear_result, log_result);
}

/* ================================================================
 * Fujifilm F-Log2
 * Reference: Fujifilm F-Log2 Data Sheet
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_flog2_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T cut1 = ALWAN_CORE_LITERAL(0.000889);
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(5.555556);
    ALWAN_CORE_T B = ALWAN_CORE_LITERAL(0.064829);
    ALWAN_CORE_T C = ALWAN_CORE_LITERAL(0.245281);
    ALWAN_CORE_T D = ALWAN_CORE_LITERAL(0.384316);
    ALWAN_CORE_T E = ALWAN_CORE_LITERAL(8.799461);
    ALWAN_CORE_T F = ALWAN_CORE_LITERAL(0.092864);

    ALWAN_CORE_T linear_result = E * linear + F;
    ALWAN_CORE_T log_result    = C * ALWAN_CORE_LOG10(A * linear + B) + D;
    return ALWAN_CORE_SELECT(linear < cut1, linear_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_flog2_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T cut2 = ALWAN_CORE_LITERAL(0.100686685370811);
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(5.555556);
    ALWAN_CORE_T B = ALWAN_CORE_LITERAL(0.064829);
    ALWAN_CORE_T C = ALWAN_CORE_LITERAL(0.245281);
    ALWAN_CORE_T D = ALWAN_CORE_LITERAL(0.384316);
    ALWAN_CORE_T E = ALWAN_CORE_LITERAL(8.799461);
    ALWAN_CORE_T F = ALWAN_CORE_LITERAL(0.092864);

    ALWAN_CORE_T linear_result = (encoded - F) / E;
    ALWAN_CORE_T log_result    = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), (encoded - D) / C) - B) / A;
    return ALWAN_CORE_SELECT(encoded < cut2, linear_result, log_result);
}

/* ================================================================
 * Leica L-Log
 * Reference: Leica L-Log Reference Manual
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_llog_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T cut = ALWAN_CORE_LITERAL(0.01);
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(5.555556);
    ALWAN_CORE_T B = ALWAN_CORE_LITERAL(0.064);
    ALWAN_CORE_T C = ALWAN_CORE_LITERAL(0.241514);
    ALWAN_CORE_T D = ALWAN_CORE_LITERAL(0.598206);
    ALWAN_CORE_T E = ALWAN_CORE_LITERAL(5.6);
    ALWAN_CORE_T F = ALWAN_CORE_LITERAL(0.069);

    ALWAN_CORE_T linear_result = E * linear + F;
    ALWAN_CORE_T log_result    = C * ALWAN_CORE_LOG10(A * linear + B) + D;
    return ALWAN_CORE_SELECT(linear < cut, linear_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_llog_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T cut_enc = ALWAN_CORE_LITERAL(0.125);
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(5.555556);
    ALWAN_CORE_T B = ALWAN_CORE_LITERAL(0.064);
    ALWAN_CORE_T C = ALWAN_CORE_LITERAL(0.241514);
    ALWAN_CORE_T D = ALWAN_CORE_LITERAL(0.598206);
    ALWAN_CORE_T E = ALWAN_CORE_LITERAL(5.6);
    ALWAN_CORE_T F = ALWAN_CORE_LITERAL(0.069);

    ALWAN_CORE_T linear_result = (encoded - F) / E;
    ALWAN_CORE_T log_result    = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), (encoded - D) / C) - B) / A;
    return ALWAN_CORE_SELECT(encoded < cut_enc, linear_result, log_result);
}

/* ================================================================
 * DJI D-Log
 * Reference: DJI Cinema Color System Technical Whitepaper
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_dlog_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T cut       = ALWAN_CORE_LITERAL(0.0078);
    ALWAN_CORE_T lin_slope = ALWAN_CORE_LITERAL(6.025);
    ALWAN_CORE_T lin_off   = ALWAN_CORE_LITERAL(0.0929);
    ALWAN_CORE_T log_mult  = ALWAN_CORE_LITERAL(0.256663);
    ALWAN_CORE_T log_off   = ALWAN_CORE_LITERAL(0.584555);
    ALWAN_CORE_T log_scale = ALWAN_CORE_LITERAL(0.9892);
    ALWAN_CORE_T log_bias  = ALWAN_CORE_LITERAL(0.0108);

    ALWAN_CORE_T linear_result = lin_slope * linear + lin_off;
    ALWAN_CORE_T log_result    = ALWAN_CORE_LOG10(linear * log_scale + log_bias) * log_mult + log_off;
    return ALWAN_CORE_SELECT(linear <= cut, linear_result, log_result);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_dlog_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T cut_enc   = ALWAN_CORE_LITERAL(0.14);
    ALWAN_CORE_T lin_slope = ALWAN_CORE_LITERAL(6.025);
    ALWAN_CORE_T lin_off   = ALWAN_CORE_LITERAL(0.0929);
    ALWAN_CORE_T log_mult  = ALWAN_CORE_LITERAL(0.256663);
    ALWAN_CORE_T log_off   = ALWAN_CORE_LITERAL(0.584555);
    ALWAN_CORE_T log_scale = ALWAN_CORE_LITERAL(0.9892);
    ALWAN_CORE_T log_bias  = ALWAN_CORE_LITERAL(0.0108);

    ALWAN_CORE_T linear_result = (encoded - lin_off) / lin_slope;
    ALWAN_CORE_T log_result    = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), (encoded - log_off) / log_mult) - log_bias) / log_scale;
    return ALWAN_CORE_SELECT(encoded <= cut_enc, linear_result, log_result);
}

/* ================================================================
 * DCDM (Digital Cinema Distribution Master) - SMPTE ST 428-1
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_dcdm_oetf)(ALWAN_CORE_T linear) {
    ALWAN_CORE_T scale     = ALWAN_CORE_LITERAL(0.9165552797403094);   /* 48/52.37 */
    ALWAN_CORE_T inv_gamma = ALWAN_CORE_LITERAL(0.38461538461538464);  /* 1/2.6 */

    ALWAN_CORE_T L = ALWAN_CORE_SELECT(linear <= ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    return ALWAN_CORE_POW(L * scale, inv_gamma);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_dcdm_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T inv_scale = ALWAN_CORE_LITERAL(1.0910416666666667);  /* 52.37/48 */
    ALWAN_CORE_T gamma     = ALWAN_CORE_LITERAL(2.6);

    ALWAN_CORE_T E = ALWAN_CORE_SELECT(encoded <= ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, encoded);
    return ALWAN_CORE_POW(E, gamma) * inv_scale;
}

/* ================================================================
 * Linear / Identity
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_linear_identity)(ALWAN_CORE_T v) {
    return v;
}

/* ================================================================
 * ADX (Academy Density Exchange) - SMPTE ST 2065-3
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_adx10_oetf)(ALWAN_CORE_T density) {
    ALWAN_CORE_T ref_pt = ALWAN_CORE_LITERAL(0.5);
    ALWAN_CORE_T scale  = ALWAN_CORE_LITERAL(400.0);
    ALWAN_CORE_T norm   = ALWAN_CORE_LITERAL(1023.0);

    ALWAN_CORE_T cv_raw = (density + ref_pt) * scale;
    ALWAN_CORE_T cv = ALWAN_CORE_SELECT(cv_raw < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO,
                      ALWAN_CORE_SELECT(cv_raw > norm, norm, cv_raw));
    return cv / norm;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_adx10_eotf)(ALWAN_CORE_T encoded) {
    return encoded * ALWAN_CORE_LITERAL(1023.0) / ALWAN_CORE_LITERAL(400.0) - ALWAN_CORE_LITERAL(0.5);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_adx16_oetf)(ALWAN_CORE_T density) {
    ALWAN_CORE_T ref_pt = ALWAN_CORE_LITERAL(0.5);
    ALWAN_CORE_T scale  = ALWAN_CORE_LITERAL(25600.0);
    ALWAN_CORE_T norm   = ALWAN_CORE_LITERAL(65535.0);

    ALWAN_CORE_T cv_raw = (density + ref_pt) * scale;
    ALWAN_CORE_T cv = ALWAN_CORE_SELECT(cv_raw < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO,
                      ALWAN_CORE_SELECT(cv_raw > norm, norm, cv_raw));
    return cv / norm;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_adx16_eotf)(ALWAN_CORE_T encoded) {
    return encoded * ALWAN_CORE_LITERAL(65535.0) / ALWAN_CORE_LITERAL(25600.0) - ALWAN_CORE_LITERAL(0.5);
}

/* ================================================================
 * Arbitrary Gamma Transfer Function
 *
 * Simple power-law gamma encoding/decoding with arbitrary exponent.
 * OETF: linear -> encoded = pow(max(0, linear), 1/gamma)
 * EOTF: encoded -> linear = pow(max(0, encoded), gamma)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_gamma_oetf)(ALWAN_CORE_T linear, ALWAN_CORE_T gamma) {
    ALWAN_CORE_T safe = ALWAN_CORE_SELECT(linear < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, linear);
    return ALWAN_CORE_POW(safe, ALWAN_CORE_ONE / gamma);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_gamma_eotf)(ALWAN_CORE_T encoded, ALWAN_CORE_T gamma) {
    ALWAN_CORE_T safe = ALWAN_CORE_SELECT(encoded < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, encoded);
    return ALWAN_CORE_POW(safe, gamma);
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

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_dicom_gsdf_eotf)(ALWAN_CORE_T jnd) {
    /* JND index to luminance (cd/m2)
     * DICOM PS3.14 Barten model: rational polynomial in ln(jnd)
     * log10(L) = (a + c*ln(j) + e*ln(j)^2 + g*ln(j)^3 + m*ln(j)^4)
     *          / (1 + b*ln(j) + d*ln(j)^2 + f*ln(j)^3 + h*ln(j)^4 + k*ln(j)^5)
     */
    ALWAN_CORE_T const a = ALWAN_CORE_LITERAL(-1.3011877);
    ALWAN_CORE_T const b = ALWAN_CORE_LITERAL(-2.5840191e-2);
    ALWAN_CORE_T const c = ALWAN_CORE_LITERAL( 8.0242636e-2);
    ALWAN_CORE_T const d = ALWAN_CORE_LITERAL(-1.0320229e-1);
    ALWAN_CORE_T const e = ALWAN_CORE_LITERAL( 1.3646699e-1);
    ALWAN_CORE_T const f = ALWAN_CORE_LITERAL( 2.8745620e-2);
    ALWAN_CORE_T const g = ALWAN_CORE_LITERAL(-2.5468404e-2);
    ALWAN_CORE_T const h = ALWAN_CORE_LITERAL(-3.1978977e-3);
    ALWAN_CORE_T const k = ALWAN_CORE_LITERAL( 1.2992634e-4);
    ALWAN_CORE_T const m = ALWAN_CORE_LITERAL( 1.3635334e-3);

    ALWAN_CORE_T safe_jnd = ALWAN_CORE_SELECT(jnd < ALWAN_CORE_LITERAL(1.0),
                                          ALWAN_CORE_LITERAL(1.0), jnd);
    ALWAN_CORE_T lj = ALWAN_CORE_LN(safe_jnd);
    ALWAN_CORE_T lj2 = lj * lj;
    ALWAN_CORE_T lj3 = lj2 * lj;
    ALWAN_CORE_T lj4 = lj2 * lj2;
    ALWAN_CORE_T lj5 = lj4 * lj;

    ALWAN_CORE_T numer = a + c * lj + e * lj2 + g * lj3 + m * lj4;
    ALWAN_CORE_T denom = ALWAN_CORE_ONE + b * lj + d * lj2 + f * lj3
                        + h * lj4 + k * lj5;
    ALWAN_CORE_T safe_denom = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(denom) < ALWAN_CORE_LITERAL(1e-10),
                                            ALWAN_CORE_LITERAL(1e-10), denom);

    ALWAN_CORE_T log_L = numer / safe_denom;

    return ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), log_L);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_dicom_gsdf_oetf)(ALWAN_CORE_T luminance) {
    /* Luminance (cd/m2) to JND index */
    /* Coefficients from DICOM PS3.14 inverse */
    ALWAN_CORE_T const a = ALWAN_CORE_LITERAL( 71.498068);
    ALWAN_CORE_T const b = ALWAN_CORE_LITERAL( 94.593053);
    ALWAN_CORE_T const c = ALWAN_CORE_LITERAL( 41.912053);
    ALWAN_CORE_T const d = ALWAN_CORE_LITERAL(  9.8247004);
    ALWAN_CORE_T const e = ALWAN_CORE_LITERAL(  0.28175407);
    ALWAN_CORE_T const f = ALWAN_CORE_LITERAL( -1.1878455);
    ALWAN_CORE_T const g = ALWAN_CORE_LITERAL( -0.18014349);
    ALWAN_CORE_T const h = ALWAN_CORE_LITERAL(  0.14710899);
    ALWAN_CORE_T const k = ALWAN_CORE_LITERAL( -0.017046845);

    ALWAN_CORE_T safe_L = ALWAN_CORE_SELECT(luminance < ALWAN_CORE_LITERAL(0.05),
                                        ALWAN_CORE_LITERAL(0.05), luminance);
    ALWAN_CORE_T ll = ALWAN_CORE_LOG10(safe_L);
    ALWAN_CORE_T ll2 = ll * ll;
    ALWAN_CORE_T ll3 = ll2 * ll;
    ALWAN_CORE_T ll4 = ll2 * ll2;

    ALWAN_CORE_T jnd = a + b * ll + c * ll2 + d * ll3
                      + e * ll4 + f * ll2 * ll3
                      + g * ll3 * ll3 + h * ll3 * ll4
                      + k * ll4 * ll4;

    return ALWAN_CORE_SELECT(jnd < ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0), jnd);
}
