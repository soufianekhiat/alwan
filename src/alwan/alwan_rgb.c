/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* struct alwan_ctx is defined in alwan_internal.h */

/* ----------------------------------------------------------------
 * Helper: Convert xyY to XYZ (Y=1)
 * ---------------------------------------------------------------- */
static void xy_to_XYZ(alwan_scalar x, alwan_scalar y, alwan_vec3 *XYZ) {
    /* Avoid division by zero */
    if (y < ALWAN_EPSILON) {
        XYZ->v[0] = XYZ->v[1] = XYZ->v[2] = ALWAN_LITERAL(0.0);
        return;
    }

    alwan_scalar Y = ALWAN_LITERAL(1.0);
    XYZ->v[0] = (x / y) * Y;  /* X */
    XYZ->v[1] = Y;             /* Y */
    XYZ->v[2] = ((ALWAN_LITERAL(1.0) - x - y) / y) * Y;  /* Z */
}

/* ----------------------------------------------------------------
 * RGB Matrix Derivation
 * ---------------------------------------------------------------- */

int alwan_rgb_derive_matrices(alwan_mat3x3 *rgb_to_xyz,
                               alwan_mat3x3 *xyz_to_rgb,
                               alwan_rgb_space_desc const *desc) {
    if (!desc || !rgb_to_xyz || !xyz_to_rgb) {
        return ALWAN_E_INVALID;
    }

    /* Convert primaries from xy to XYZ (with Y=1) */
    alwan_vec3 R_XYZ, G_XYZ, B_XYZ, W_XYZ;
    xy_to_XYZ(desc->primaries_xy[0], desc->primaries_xy[1], &R_XYZ);
    xy_to_XYZ(desc->primaries_xy[2], desc->primaries_xy[3], &G_XYZ);
    xy_to_XYZ(desc->primaries_xy[4], desc->primaries_xy[5], &B_XYZ);
    xy_to_XYZ(desc->white_xy[0], desc->white_xy[1], &W_XYZ);

    /* Form matrix M from primaries (columns are R, G, B) */
    alwan_mat3x3 M = {{
        R_XYZ.v[0], G_XYZ.v[0], B_XYZ.v[0],  /* Row 0: X */
        R_XYZ.v[1], G_XYZ.v[1], B_XYZ.v[1],  /* Row 1: Y */
        R_XYZ.v[2], G_XYZ.v[2], B_XYZ.v[2]   /* Row 2: Z */
    }};

    /* Compute M^-1 */
    alwan_mat3x3 M_inv;
    int status = alwan_mat3_inv(&M_inv, &M);
    if (status != ALWAN_OK) {
        return status;  /* Singular matrix */
    }

    /* Solve for scale factors: S = M^-1 * W */
    alwan_vec3 S;
    alwan_mat3_mulv(&S, &M_inv, &W_XYZ);

    /* RGB->XYZ = M * diag(S)
     * This is equivalent to scaling each column of M by corresponding S component */
    rgb_to_xyz->m[0] = M.m[0] * S.v[0];  /* R column scaled */
    rgb_to_xyz->m[1] = M.m[1] * S.v[1];  /* G column scaled */
    rgb_to_xyz->m[2] = M.m[2] * S.v[2];  /* B column scaled */
    rgb_to_xyz->m[3] = M.m[3] * S.v[0];
    rgb_to_xyz->m[4] = M.m[4] * S.v[1];
    rgb_to_xyz->m[5] = M.m[5] * S.v[2];
    rgb_to_xyz->m[6] = M.m[6] * S.v[0];
    rgb_to_xyz->m[7] = M.m[7] * S.v[1];
    rgb_to_xyz->m[8] = M.m[8] * S.v[2];

    /* XYZ->RGB = inverse of RGB->XYZ */
    return alwan_mat3_inv(xyz_to_rgb, rgb_to_xyz);
}

/* ----------------------------------------------------------------
 * RGB <-> XYZ Direct Conversion
 * ---------------------------------------------------------------- */

int alwan_rgb_to_xyz(alwan_xyz *xyz,
                     alwan_rgb_space_desc const *space,
                     alwan_rgb const *rgb) {
    if (!space || !rgb || !xyz) {
        return ALWAN_E_INVALID;
    }

    /* Derive conversion matrices */
    alwan_mat3x3 rgb_to_xyz_mat, xyz_to_rgb_mat;
    int status = alwan_rgb_derive_matrices(&rgb_to_xyz_mat, &xyz_to_rgb_mat, space);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Apply RGB -> XYZ matrix */
    alwan_vec3 vec_in, vec_out;
    ALWAN_MEMCPY(&vec_in, rgb, sizeof(alwan_vec3));
    alwan_mat3_mulv(&vec_out, &rgb_to_xyz_mat, &vec_in);
    ALWAN_MEMCPY(xyz, &vec_out, sizeof(alwan_vec3));

    return ALWAN_OK;
}

int alwan_xyz_to_rgb(alwan_rgb *rgb,
                     alwan_rgb_space_desc const *space,
                     alwan_xyz const *xyz) {
    if (!space || !xyz || !rgb) {
        return ALWAN_E_INVALID;
    }

    /* Derive conversion matrices */
    alwan_mat3x3 rgb_to_xyz_mat, xyz_to_rgb_mat;
    int status = alwan_rgb_derive_matrices(&rgb_to_xyz_mat, &xyz_to_rgb_mat, space);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Apply XYZ -> RGB matrix */
    alwan_vec3 vec_in, vec_out;
    ALWAN_MEMCPY(&vec_in, xyz, sizeof(alwan_vec3));
    alwan_mat3_mulv(&vec_out, &xyz_to_rgb_mat, &vec_in);
    ALWAN_MEMCPY(rgb, &vec_out, sizeof(alwan_vec3));

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * sRGB Transfer Functions
 * ---------------------------------------------------------------- */

/* sRGB OETF: linear -> encoded
 * Formula: V' = 12.92 * V              if V <= 0.0031308
 *               1.055 * V^(1/2.4) - 0.055   otherwise */
static alwan_scalar srgb_oetf_scalar(alwan_scalar linear) {
    if (linear <= ALWAN_LITERAL(0.0031308)) {
        return ALWAN_LITERAL(12.92) * linear;
    } else {
        return ALWAN_LITERAL(1.055) * ALWAN_POW(linear, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4))
               - ALWAN_LITERAL(0.055);
    }
}

/* sRGB EOTF: encoded -> linear
 * Formula: V = V' / 12.92              if V' <= 0.04045
 *               ((V' + 0.055) / 1.055)^2.4  otherwise */
static alwan_scalar srgb_eotf_scalar(alwan_scalar encoded) {
    if (encoded <= ALWAN_LITERAL(0.04045)) {
        return encoded / ALWAN_LITERAL(12.92);
    } else {
        return ALWAN_POW((encoded + ALWAN_LITERAL(0.055)) / ALWAN_LITERAL(1.055),
                         ALWAN_LITERAL(2.4));
    }
}

/* ----------------------------------------------------------------
 * BT.2020 Transfer Functions
 * ---------------------------------------------------------------- */

/* BT.2020 OETF: linear -> encoded
 * Formula: E' = 4.5 * E                       if E < 0.018
 *               1.099 * E^0.45 - 0.099        otherwise
 * This is the 10-bit system variant (beta=0.018, alpha=1.099) */
static alwan_scalar bt2020_oetf_scalar(alwan_scalar linear) {
    alwan_scalar const beta = ALWAN_LITERAL(0.018);
    alwan_scalar const alpha = ALWAN_LITERAL(1.099);

    if (linear < beta) {
        return ALWAN_LITERAL(4.5) * linear;
    } else {
        return alpha * ALWAN_POW(linear, ALWAN_LITERAL(0.45)) - (alpha - ALWAN_LITERAL(1.0));
    }
}

/* BT.2020 EOTF: encoded -> linear
 * Inverse of the OETF above */
static alwan_scalar bt2020_eotf_scalar(alwan_scalar encoded) {
    alwan_scalar const beta = ALWAN_LITERAL(0.018);
    alwan_scalar const alpha = ALWAN_LITERAL(1.099);
    alwan_scalar const threshold = ALWAN_LITERAL(4.5) * beta;  /* 0.081 */

    if (encoded < threshold) {
        return encoded / ALWAN_LITERAL(4.5);
    } else {
        return ALWAN_POW((encoded + (alpha - ALWAN_LITERAL(1.0))) / alpha,
                         ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.45));
    }
}

/* ----------------------------------------------------------------
 * PQ (ST.2084) Transfer Functions - HDR10/HDR10+
 * ---------------------------------------------------------------- */

/* PQ OETF: linear (0-10000 cd/m²) -> PQ code values
 * Normalizes input to [0,1] assuming 10000 cd/m² peak, then applies PQ curve */
static alwan_scalar pq_oetf_scalar(alwan_scalar linear) {
    /* PQ constants (computed at compile time) */
    alwan_scalar const m1 = ALWAN_LITERAL(2610.0) / ALWAN_LITERAL(16384.0);
    alwan_scalar const m2 = ALWAN_LITERAL(2523.0) / ALWAN_LITERAL(32.0);
    alwan_scalar const c1 = ALWAN_LITERAL(3424.0) / ALWAN_LITERAL(4096.0);
    alwan_scalar const c2 = ALWAN_LITERAL(2413.0) / ALWAN_LITERAL(128.0);
    alwan_scalar const c3 = ALWAN_LITERAL(2392.0) / ALWAN_LITERAL(128.0);

    /* Normalize to [0,1] assuming 10000 cd/m² peak */
    alwan_scalar Y = linear / ALWAN_LITERAL(10000.0);
    if (Y < ALWAN_LITERAL(0.0)) Y = ALWAN_LITERAL(0.0);

    alwan_scalar Y_pow_m1 = ALWAN_POW(Y, m1);
    alwan_scalar numerator = c1 + c2 * Y_pow_m1;
    alwan_scalar denominator = ALWAN_LITERAL(1.0) + c3 * Y_pow_m1;

    return ALWAN_POW(numerator / denominator, m2);
}

/* PQ EOTF: PQ code values -> linear (0-10000 cd/m²) */
static alwan_scalar pq_eotf_scalar(alwan_scalar encoded) {
    /* PQ constants (computed at compile time) */
    alwan_scalar const m1 = ALWAN_LITERAL(2610.0) / ALWAN_LITERAL(16384.0);
    alwan_scalar const m2 = ALWAN_LITERAL(2523.0) / ALWAN_LITERAL(32.0);
    alwan_scalar const c1 = ALWAN_LITERAL(3424.0) / ALWAN_LITERAL(4096.0);
    alwan_scalar const c2 = ALWAN_LITERAL(2413.0) / ALWAN_LITERAL(128.0);
    alwan_scalar const c3 = ALWAN_LITERAL(2392.0) / ALWAN_LITERAL(128.0);
    alwan_scalar const m1_inv = ALWAN_LITERAL(1.0) / m1;
    alwan_scalar const m2_inv = ALWAN_LITERAL(1.0) / m2;

    if (encoded < ALWAN_LITERAL(0.0)) encoded = ALWAN_LITERAL(0.0);

    alwan_scalar E_pow_m2_inv = ALWAN_POW(encoded, m2_inv);
    alwan_scalar numerator = E_pow_m2_inv - c1;
    if (numerator < ALWAN_LITERAL(0.0)) numerator = ALWAN_LITERAL(0.0);

    alwan_scalar denominator = c2 - c3 * E_pow_m2_inv;
    alwan_scalar Y = ALWAN_POW(numerator / denominator, m1_inv);

    /* Scale back to cd/m² (0-10000 range) */
    return Y * ALWAN_LITERAL(10000.0);
}

/* ----------------------------------------------------------------
 * HLG (Hybrid Log-Gamma) Transfer Functions - BT.2100
 * ---------------------------------------------------------------- */

/* HLG OETF: scene-referred linear -> HLG signal */
static alwan_scalar hlg_oetf_scalar( alwan_scalar linear )
{
	/* HLG constants (computed at compile time) */
	alwan_scalar const a = ALWAN_LITERAL( 0.17883277 );
	alwan_scalar const b = ALWAN_LITERAL( 1.0 ) - ALWAN_LITERAL( 4.0 ) * a;  /* 1 - 4*a */
	alwan_scalar const c = ALWAN_LITERAL( 0.5 ) - a * ALWAN_LN( ALWAN_LITERAL( 4.0 ) * a );  /* 0.5 - a*ln(4*a) */

    if (linear < ALWAN_LITERAL(0.0)) linear = ALWAN_LITERAL(0.0);

    if (linear <= ALWAN_LITERAL(1.0) / ALWAN_LITERAL(12.0)) {
        return ALWAN_SQRT(ALWAN_LITERAL(3.0) * linear);
    } else {
        return a * ALWAN_LN(ALWAN_LITERAL(12.0) * linear - b) + c;
    }
}

/* HLG EOTF: HLG signal -> display-referred linear
 * Note: Simplified EOTF without system gamma, assumes gamma=1.2 */
static alwan_scalar hlg_eotf_scalar(alwan_scalar encoded) {
    /* HLG constants (computed at compile time) */
    alwan_scalar const a = ALWAN_LITERAL(0.17883277);
    alwan_scalar const b = ALWAN_LITERAL(1.0) - ALWAN_LITERAL(4.0)*a;  /* 1 - 4*a */
    alwan_scalar const c = ALWAN_LITERAL(0.5) - a*ALWAN_LN(ALWAN_LITERAL(4.0)*a);  /* 0.5 - a*ln(4*a) */

    if (encoded < ALWAN_LITERAL(0.0)) encoded = ALWAN_LITERAL(0.0);

    alwan_scalar linear;
    if (encoded <= ALWAN_LITERAL(0.5)) {
        linear = (encoded * encoded) / ALWAN_LITERAL(3.0);
    } else {
        linear = (ALWAN_EXP((encoded - c) / a) + b) / ALWAN_LITERAL(12.0);
    }

    /* Apply system gamma 1.2 */
    return ALWAN_POW(linear, ALWAN_LITERAL(1.2));
}

/* ----------------------------------------------------------------
 * BT.1886 EOTF - Reference monitor transfer function
 * ---------------------------------------------------------------- */

/* BT.1886 EOTF: assumes gamma 2.4, black level 0 */
static alwan_scalar bt1886_eotf_scalar(alwan_scalar encoded) {
    if (encoded < ALWAN_LITERAL(0.0)) encoded = ALWAN_LITERAL(0.0);
    return ALWAN_POW(encoded, ALWAN_LITERAL(2.4));
}

/* ----------------------------------------------------------------
 * ACESproxy Transfer Functions
 * ---------------------------------------------------------------- */

/* ACESproxy encode: ACES linear -> ACESproxy (10-bit or 12-bit log encoding) */
static alwan_scalar acesproxy_oetf_scalar(alwan_scalar linear) {
    /* ACESproxy constants for 10-bit encoding */
    alwan_scalar const mid_gray_in = ALWAN_LITERAL(0.18);
    alwan_scalar const mid_code_value = ALWAN_LITERAL(425.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const steps_per_stop = ALWAN_LITERAL(50.0) / ALWAN_LITERAL(1023.0);

    if (linear <= ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(64.0) / ALWAN_LITERAL(1023.0);  /* Minimum code value */
    }

    alwan_scalar log_val = ALWAN_LN(linear / mid_gray_in) / ALWAN_LN(ALWAN_LITERAL(2.0));
    return mid_code_value + steps_per_stop * log_val;
}

/* ACESproxy decode: ACESproxy -> ACES linear */
static alwan_scalar acesproxy_eotf_scalar(alwan_scalar encoded) {
    alwan_scalar const mid_gray_in = ALWAN_LITERAL(0.18);
    alwan_scalar const mid_code_value = ALWAN_LITERAL(425.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const steps_per_stop = ALWAN_LITERAL(50.0) / ALWAN_LITERAL(1023.0);

    alwan_scalar log_val = (encoded - mid_code_value) / steps_per_stop;
    return mid_gray_in * ALWAN_POW(ALWAN_LITERAL(2.0), log_val);
}

/* ----------------------------------------------------------------
 * Extended Transfer Functions
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * Legal Range / Full Range Conversion Helpers (10-bit)
 * ---------------------------------------------------------------- */

/* Legal range to full range conversion (10-bit)
 * Legal range: 64-940 (0.062561094819159 - 0.919165268851521)
 * Full range: 0-1023 (0.0 - 1.0) */
static alwan_scalar legal_to_full_10bit(alwan_scalar legal) {
    return (legal - ALWAN_LITERAL(0.062561094819159)) / ALWAN_LITERAL(0.856304985337243);
}

static alwan_scalar full_to_legal_10bit(alwan_scalar full) {
    return full * ALWAN_LITERAL(0.856304985337243) + ALWAN_LITERAL(0.062561094819159);
}

/* ----------------------------------------------------------------
 * Sony S-Log Family
 * ---------------------------------------------------------------- */

/* S-Log OETF: linear -> S-Log
 * Formula from colour-science (Sony S-Log specification)
 * Reference: SonyCorporation2012a
 * Note: Scene-referred (in_reflection=True), normalized code values (legal range) */
static alwan_scalar slog_oetf_scalar(alwan_scalar linear) {
    /* Step 1: Scale input (scene-referred) */
    alwan_scalar x = linear / ALWAN_LITERAL(0.9);

    /* Step 2: Apply log curve (full range) */
    alwan_scalar y_full;
    if (x >= ALWAN_LITERAL(0.0)) {
        y_full = ALWAN_LITERAL(0.432699) * ALWAN_LOG10(x + ALWAN_LITERAL(0.037584)) + ALWAN_LITERAL(0.616596) + ALWAN_LITERAL(0.03);
    } else {
        y_full = x * ALWAN_LITERAL(5.0) + ALWAN_LITERAL(0.030001222851889303);
    }

    /* Step 3: Convert to legal range (normalized code values) */
    return full_to_legal_10bit(y_full);
}

/* S-Log EOTF: S-Log -> linear
 * Formula from colour-science (Sony S-Log specification)
 * Reference: SonyCorporation2012a
 * Note: Scene-referred (out_reflection=True), expects legal range input */
static alwan_scalar slog_eotf_scalar(alwan_scalar encoded) {
    /* Step 1: Convert from legal range to full range */
    alwan_scalar y_full = legal_to_full_10bit(encoded);

    /* Threshold: encoded value for linear=0.0 (full range) */
    alwan_scalar const threshold = ALWAN_LITERAL(0.030001222851889303);

    /* Step 2: Inverse log curve */
    alwan_scalar x;
    if (y_full >= threshold) {
        x = ALWAN_POW(ALWAN_LITERAL(10.0), (y_full - ALWAN_LITERAL(0.646596)) / ALWAN_LITERAL(0.432699)) - ALWAN_LITERAL(0.037584);
    } else {
        x = (y_full - ALWAN_LITERAL(0.030001222851889303)) / ALWAN_LITERAL(5.0);
    }

    /* Step 3: Scale output (scene-referred) */
    return x * ALWAN_LITERAL(0.9);
}

/* S-Log2 OETF: linear -> S-Log2
 * Formula from colour-science: S-Log2 is S-Log with 155/219 scaling
 * Reference: SonyCorporation2012a */
static alwan_scalar slog2_oetf_scalar(alwan_scalar linear) {
    /* Scale input: S-Log2 = S-Log(x * 155/219) */
    alwan_scalar scaled = linear * ALWAN_LITERAL(155.0) / ALWAN_LITERAL(219.0);
    return slog_oetf_scalar(scaled);
}

static alwan_scalar slog2_eotf_scalar(alwan_scalar encoded) {
    /* Decode with S-Log, then scale output: x = S-Log_decode(y) * 219/155 */
    alwan_scalar decoded = slog_eotf_scalar(encoded);
    return decoded * ALWAN_LITERAL(219.0) / ALWAN_LITERAL(155.0);
}

/* S-Log3 OETF: linear -> S-Log3
 * Formula from colour-science (Sony S-Log3 specification)
 * Outputs normalized code values (legal range)
 * Reference: SonyCorporation2012a */
static alwan_scalar slog3_oetf_scalar(alwan_scalar linear) {
    if (linear < ALWAN_LITERAL(0.0)) {
        linear = ALWAN_LITERAL(0.0);
    }

    alwan_scalar y;
    if (linear >= ALWAN_LITERAL(0.01125000)) {
        /* Logarithmic region: note denominator is (0.18 + 0.01) = 0.19, NOT 0.01125 */
        y = (ALWAN_LITERAL(420.0) + ALWAN_LOG10((linear + ALWAN_LITERAL(0.01)) / ALWAN_LITERAL(0.19)) * ALWAN_LITERAL(261.5)) / ALWAN_LITERAL(1023.0);
    } else {
        /* Linear region */
        y = (linear * (ALWAN_LITERAL(171.2102946929) - ALWAN_LITERAL(95.0)) / ALWAN_LITERAL(0.01125000) + ALWAN_LITERAL(95.0)) / ALWAN_LITERAL(1023.0);
    }

    /* Return normalized code values (legal range) */
    return y;
}

/* S-Log3 EOTF: S-Log3 -> linear
 * Expects normalized code values (legal range) as input */
static alwan_scalar slog3_eotf_scalar(alwan_scalar encoded) {
    /* Input is normalized code values (legal range) */
    alwan_scalar code = encoded * ALWAN_LITERAL(1023.0);

    if (code >= ALWAN_LITERAL(171.2102946929)) {
        /* Logarithmic region: note the multiplier is 0.19, not 0.01125 */
        return (ALWAN_POW(ALWAN_LITERAL(10.0), ((code - ALWAN_LITERAL(420.0)) / ALWAN_LITERAL(261.5))) * ALWAN_LITERAL(0.19)) - ALWAN_LITERAL(0.01);
    } else {
        /* Linear region */
        return ((code - ALWAN_LITERAL(95.0)) / (ALWAN_LITERAL(171.2102946929) - ALWAN_LITERAL(95.0))) * ALWAN_LITERAL(0.01125000);
    }
}

/* ----------------------------------------------------------------
 * Canon C-Log Family
 * ---------------------------------------------------------------- */

/* C-Log OETF: linear -> C-Log (v1.2)
 * Formula from colour-science (Canon C-Log v1.2 specification)
 * Reference: CanonInc2012, CanonInc2016 */
static alwan_scalar clog_oetf_scalar(alwan_scalar linear) {
    /* Constants for C-Log v1.2 */
    alwan_scalar const a = ALWAN_LITERAL(0.45310179);
    alwan_scalar const k = ALWAN_LITERAL(10.1596);
    alwan_scalar const offset = ALWAN_LITERAL(0.12512248);

    if (linear < ALWAN_LITERAL(0.0)) {
        linear = ALWAN_LITERAL(0.0);
    }

    /* Scene-referred scaling */
    alwan_scalar x = linear / ALWAN_LITERAL(0.9);

    /* Encoding: a * log10(k * x + 1) + offset */
    return a * ALWAN_LOG10(k * x + ALWAN_LITERAL(1.0)) + offset;
}

static alwan_scalar clog_eotf_scalar(alwan_scalar encoded) {
    /* Constants for C-Log v1.2 */
    alwan_scalar const a = ALWAN_LITERAL(0.45310179);
    alwan_scalar const k = ALWAN_LITERAL(10.1596);
    alwan_scalar const offset = ALWAN_LITERAL(0.12512248);

    /* Decoding: (10^((encoded - offset) / a) - 1) / k */
    alwan_scalar x = (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - offset) / a) - ALWAN_LITERAL(1.0)) / k;

    /* Scene-referred scaling */
    return x * ALWAN_LITERAL(0.9);
}

/* C-Log2 OETF: linear -> C-Log2 (v1.2)
 * Formula from colour-science (Canon C-Log2 v1.2 specification)
 * Reference: CanonInc2012, CanonInc2016 */
static alwan_scalar clog2_oetf_scalar(alwan_scalar linear) {
    /* Constants for C-Log2 v1.2 */
    alwan_scalar const a = ALWAN_LITERAL(0.24136077);
    alwan_scalar const k = ALWAN_LITERAL(87.09937546);
    alwan_scalar const offset = ALWAN_LITERAL(0.092864125);

    if (linear < ALWAN_LITERAL(0.0)) {
        linear = ALWAN_LITERAL(0.0);
    }

    /* Scene-referred scaling */
    alwan_scalar x = linear / ALWAN_LITERAL(0.9);

    /* Encoding: a * log10(k * x + 1) + offset */
    return a * ALWAN_LOG10(k * x + ALWAN_LITERAL(1.0)) + offset;
}

static alwan_scalar clog2_eotf_scalar(alwan_scalar encoded) {
    /* Constants for C-Log2 v1.2 */
    alwan_scalar const a = ALWAN_LITERAL(0.24136077);
    alwan_scalar const k = ALWAN_LITERAL(87.09937546);
    alwan_scalar const offset = ALWAN_LITERAL(0.092864125);

    /* Decoding: (10^((encoded - offset) / a) - 1) / k */
    alwan_scalar x = (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - offset) / a) - ALWAN_LITERAL(1.0)) / k;

    /* Scene-referred scaling */
    return x * ALWAN_LITERAL(0.9);
}

/* C-Log3 OETF: linear -> C-Log3
 * Formula from colour-science (Canon C-Log3 v1.2 specification)
 * Reference: CanonInc2012, CanonInc2016
 * Note: Scene-referred (in_reflection=True), operates in legal range internally */
static alwan_scalar clog3_oetf_scalar(alwan_scalar linear) {
    /* Step 1: Scale input (scene-referred) */
    alwan_scalar x = linear / ALWAN_LITERAL(0.9);

    /* Step 2: Apply 3-region piecewise formula */
    /* Linear thresholds for 3-region piecewise formula */
    /* x-value thresholds (before scene-referred scaling): ±0.014 */
    alwan_scalar const x_threshold_low = ALWAN_LITERAL(-0.014);
    alwan_scalar const x_threshold_high = ALWAN_LITERAL(0.014);

    alwan_scalar y;

    if (x < x_threshold_low) {
        /* Negative region */
        y = ALWAN_LITERAL(-0.36726845) * ALWAN_LOG10(-x * ALWAN_LITERAL(14.98325) + ALWAN_LITERAL(1.0)) + ALWAN_LITERAL(0.12783901);
    } else if (x <= x_threshold_high) {
        /* Linear region */
        y = ALWAN_LITERAL(1.9754798) * x + ALWAN_LITERAL(0.12512219);
    } else {
        /* Positive logarithmic region */
        y = ALWAN_LITERAL(0.36726845) * ALWAN_LOG10(x * ALWAN_LITERAL(14.98325) + ALWAN_LITERAL(1.0)) + ALWAN_LITERAL(0.12240537);
    }

    /* Return normalized code values (legal range) */
    return y;
}

static alwan_scalar clog3_eotf_scalar(alwan_scalar encoded) {
    /* Input is normalized code values (legal range) */

    /* Apply inverse 3-region piecewise formula */
    alwan_scalar x;

    alwan_scalar const threshold_low = ALWAN_LITERAL(0.097465473);
    alwan_scalar const threshold_high = ALWAN_LITERAL(0.15277891);

    if (encoded < threshold_low) {
        /* Negative region */
        x = -(ALWAN_POW(ALWAN_LITERAL(10.0), (ALWAN_LITERAL(0.12783901) - encoded) / ALWAN_LITERAL(0.36726845)) - ALWAN_LITERAL(1.0)) / ALWAN_LITERAL(14.98325);
    } else if (encoded <= threshold_high) {
        /* Linear region */
        x = (encoded - ALWAN_LITERAL(0.12512219)) / ALWAN_LITERAL(1.9754798);
    } else {
        /* Positive logarithmic region */
        x = (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - ALWAN_LITERAL(0.12240537)) / ALWAN_LITERAL(0.36726845)) - ALWAN_LITERAL(1.0)) / ALWAN_LITERAL(14.98325);
    }

    /* Scale output (scene-referred) */
    return x * ALWAN_LITERAL(0.9);
}

/* ----------------------------------------------------------------
 * Panasonic V-Log
 * ---------------------------------------------------------------- */

/* V-Log OETF: linear -> V-Log
 * Formula from colour-science (Panasonic V-Log specification)
 * Reference: PanasonicCorporation2014
 * Outputs normalized code values (legal range) */
static alwan_scalar vlog_oetf_scalar(alwan_scalar linear) {
    /* Constants */
    alwan_scalar const cut1 = ALWAN_LITERAL(0.01);    /* Linear threshold */
    alwan_scalar const b = ALWAN_LITERAL(0.00873);
    alwan_scalar const c = ALWAN_LITERAL(0.241514);
    alwan_scalar const d = ALWAN_LITERAL(0.598206);

    /* Apply 2-region piecewise formula */
    alwan_scalar y;

    if (linear <= cut1) {
        /* Linear region (use <= to match decoding threshold and ensure continuity) */
        y = ALWAN_LITERAL(5.6) * linear + ALWAN_LITERAL(0.125);
    } else {
        /* Logarithmic region */
        y = c * ALWAN_LOG10(linear + b) + d;
    }

    /* Return normalized code values (legal range) */
    return y;
}

static alwan_scalar vlog_eotf_scalar(alwan_scalar encoded) {
    /* Constants */
    alwan_scalar const cut2 = ALWAN_LITERAL(0.181);   /* Encoded threshold */
    alwan_scalar const b = ALWAN_LITERAL(0.00873);
    alwan_scalar const c = ALWAN_LITERAL(0.241514);
    alwan_scalar const d = ALWAN_LITERAL(0.598206);

    /* Input is normalized code values (legal range) */

    /* Apply inverse 2-region piecewise formula */
    alwan_scalar linear;

    if (encoded <= cut2) {
        /* Linear region (use <= to match encoding threshold and ensure continuity) */
        linear = (encoded - ALWAN_LITERAL(0.125)) / ALWAN_LITERAL(5.6);
    } else {
        /* Logarithmic region */
        linear = ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - d) / c) - b;
    }

    return linear;
}

/* ----------------------------------------------------------------
 * ARRI LogC Family
 * ---------------------------------------------------------------- */

/* LogC3 OETF: linear -> LogC3 (EI 800)
 * Formula from ARRI documentation */
static alwan_scalar logc3_oetf_scalar(alwan_scalar linear) {
    alwan_scalar const a = ALWAN_LITERAL(5.555556);
    alwan_scalar const b = ALWAN_LITERAL(0.052272);
    alwan_scalar const c = ALWAN_LITERAL(0.247190);
    alwan_scalar const d = ALWAN_LITERAL(0.385537);
    alwan_scalar const e = ALWAN_LITERAL(5.367655);
    alwan_scalar const f = ALWAN_LITERAL(0.092809);

    if (linear > ALWAN_LITERAL(0.010591)) {
        return c * ALWAN_LOG10(a * linear + b) + d;
    } else {
        return e * linear + f;
    }
}

static alwan_scalar logc3_eotf_scalar(alwan_scalar encoded) {
    alwan_scalar const a = ALWAN_LITERAL(5.555556);
    alwan_scalar const b = ALWAN_LITERAL(0.052272);
    alwan_scalar const c = ALWAN_LITERAL(0.247190);
    alwan_scalar const d = ALWAN_LITERAL(0.385537);
    alwan_scalar const e = ALWAN_LITERAL(5.367655);
    alwan_scalar const f = ALWAN_LITERAL(0.092809);

    alwan_scalar threshold = e * ALWAN_LITERAL(0.010591) + f;

    if (encoded < threshold) {
        return (encoded - f) / e;
    } else {
        return (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - d) / c) - b) / a;
    }
}

/* LogC4 OETF: linear -> LogC4
 * Formula from ARRI documentation */
static alwan_scalar logc4_oetf_scalar(alwan_scalar linear) {
    alwan_scalar const a = ALWAN_LITERAL(14.98325);
    alwan_scalar const b = ALWAN_LITERAL(0.005494072);
    alwan_scalar const c = ALWAN_LITERAL(0.0647954);
    alwan_scalar const d = ALWAN_LITERAL(0.0075);
    alwan_scalar const e = ALWAN_LITERAL(0.000000089);
    alwan_scalar const f = ALWAN_LITERAL(0.0);

    if (linear < ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }

    alwan_scalar t = (linear + e) / (linear + d);
    return (c * ALWAN_LOG10(a * t + b)) + f;
}

static alwan_scalar logc4_eotf_scalar(alwan_scalar encoded) {
    alwan_scalar const a = ALWAN_LITERAL(14.98325);
    alwan_scalar const b = ALWAN_LITERAL(0.005494072);
    alwan_scalar const c = ALWAN_LITERAL(0.0647954);
    alwan_scalar const d = ALWAN_LITERAL(0.0075);
    alwan_scalar const e = ALWAN_LITERAL(0.000000089);
    alwan_scalar const f = ALWAN_LITERAL(0.0);

    alwan_scalar s = ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - f) / c);
    return (d * (s - b) - e * (a * s - b)) / (a * s - b - s + ALWAN_LITERAL(1.0));
}

/* ----------------------------------------------------------------
 * RED Log Family
 * ---------------------------------------------------------------- */

/* REDLog OETF: linear -> REDLog
 * Formula from RED documentation */
static alwan_scalar redlog_oetf_scalar(alwan_scalar linear) {
    if (linear < ALWAN_LITERAL(0.0)) {
        linear = ALWAN_LITERAL(0.0);
    }

    return (ALWAN_LOG10(linear * ALWAN_LITERAL(0.9) + ALWAN_LITERAL(0.1)) + ALWAN_LITERAL(3.0)) / ALWAN_LITERAL(3.0);
}

static alwan_scalar redlog_eotf_scalar(alwan_scalar encoded) {
    return (ALWAN_POW(ALWAN_LITERAL(10.0), encoded * ALWAN_LITERAL(3.0) - ALWAN_LITERAL(3.0)) - ALWAN_LITERAL(0.1)) / ALWAN_LITERAL(0.9);
}

/* REDLogFilm OETF: linear -> REDLogFilm
 * Formula from RED documentation */
static alwan_scalar redlogfilm_oetf_scalar(alwan_scalar linear) {
    if (linear < ALWAN_LITERAL(0.0)) {
        linear = ALWAN_LITERAL(0.0);
    }

    return (ALWAN_LOG10(linear * ALWAN_LITERAL(0.8) + ALWAN_LITERAL(0.1)) + ALWAN_LITERAL(3.0)) / ALWAN_LITERAL(3.0);
}

static alwan_scalar redlogfilm_eotf_scalar(alwan_scalar encoded) {
    return (ALWAN_POW(ALWAN_LITERAL(10.0), encoded * ALWAN_LITERAL(3.0) - ALWAN_LITERAL(3.0)) - ALWAN_LITERAL(0.1)) / ALWAN_LITERAL(0.8);
}

/* Log3G10 OETF: linear -> Log3G10
 * Formula from RED whitepaper (915-0187 Rev-C)
 * Constants: a=0.224282, b=155.975327, c=0.01, g=15.1927 */
static alwan_scalar log3g10_oetf_scalar(alwan_scalar linear) {
    alwan_scalar const a = ALWAN_LITERAL(0.224282);
    alwan_scalar const b = ALWAN_LITERAL(155.975327);
    alwan_scalar const c = ALWAN_LITERAL(0.01);
    alwan_scalar const g = ALWAN_LITERAL(15.1927);

    alwan_scalar x = linear + c;
    if (x < ALWAN_LITERAL(0.0)) {
        return x * g;  /* Linear segment for negative values */
    }
    return a * ALWAN_LOG10(x * b + ALWAN_LITERAL(1.0));
}

/* Log3G10 EOTF: Log3G10 -> linear
 * Inverse of OETF formula from RED whitepaper (915-0187 Rev-C)
 * Constants: a=0.224282, b=155.975327, c=0.01, g=15.1927 */
static alwan_scalar log3g10_eotf_scalar(alwan_scalar encoded) {
    alwan_scalar const a = ALWAN_LITERAL(0.224282);
    alwan_scalar const b = ALWAN_LITERAL(155.975327);
    alwan_scalar const c = ALWAN_LITERAL(0.01);
    alwan_scalar const g = ALWAN_LITERAL(15.1927);

    if (encoded < ALWAN_LITERAL(0.0)) {
        return (encoded / g) - c;  /* Inverse of linear segment */
    }
    return (ALWAN_POW(ALWAN_LITERAL(10.0), encoded / a) - ALWAN_LITERAL(1.0)) / b - c;
}

/* ----------------------------------------------------------------
 * Blackmagic Film Gen 5
 * ---------------------------------------------------------------- */

/* Blackmagic Film Gen 5 OETF: linear -> BMDFilm
 * Formula from Blackmagic documentation */
static alwan_scalar bmdfilm_oetf_scalar(alwan_scalar linear) {
    if (linear < ALWAN_LITERAL(0.005)) {
        return linear * ALWAN_LITERAL(8.283605932);
    } else {
        return ALWAN_LITERAL(0.5) * ALWAN_LN(linear + ALWAN_LITERAL(0.006)) / ALWAN_LN(ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(0.584);
    }
}

static alwan_scalar bmdfilm_eotf_scalar(alwan_scalar encoded) {
    alwan_scalar threshold = ALWAN_LITERAL(0.005) * ALWAN_LITERAL(8.283605932);

    if (encoded < threshold) {
        return encoded / ALWAN_LITERAL(8.283605932);
    } else {
        return ALWAN_POW(ALWAN_LITERAL(2.0), (encoded - ALWAN_LITERAL(0.584)) / ALWAN_LITERAL(0.5)) - ALWAN_LITERAL(0.006);
    }
}

/* ----------------------------------------------------------------
 * Blackmagic Film Gen 4 (Broadcast Film Gen 4)
 * Formula reverse-engineered from LUTs
 * ---------------------------------------------------------------- */

/* Constants for Blackmagic Film Gen 4 */
static alwan_scalar const BMDFILM4_A = ALWAN_LITERAL(5.2212906000378565);
static alwan_scalar const BMDFILM4_B = ALWAN_LITERAL(-0.00007134598996420424);
static alwan_scalar const BMDFILM4_C = ALWAN_LITERAL(0.03630411093543444);
static alwan_scalar const BMDFILM4_D = ALWAN_LITERAL(0.21566456116952773);
static alwan_scalar const BMDFILM4_E = ALWAN_LITERAL(0.7133134738229736);
static alwan_scalar const BMDFILM4_LIN_CUT = ALWAN_LITERAL(0.00500072683168086);
static alwan_scalar const BMDFILM4_LOG_CUT = ALWAN_LITERAL(0.026038902009648163);

/* Blackmagic Film Gen 4 OETF: linear -> BMDFilm4 */
static alwan_scalar bmdfilm4_oetf_scalar(alwan_scalar linear) {
    if (linear <= BMDFILM4_LIN_CUT) {
        return linear * BMDFILM4_A + BMDFILM4_B;
    } else {
        return ALWAN_LN(linear + BMDFILM4_C) * BMDFILM4_D + BMDFILM4_E;
    }
}

/* Blackmagic Film Gen 4 EOTF: BMDFilm4 -> linear */
static alwan_scalar bmdfilm4_eotf_scalar(alwan_scalar encoded) {
    if (encoded <= BMDFILM4_LOG_CUT) {
        return (encoded - BMDFILM4_B) / BMDFILM4_A;
    } else {
        return ALWAN_EXP((encoded - BMDFILM4_E) / BMDFILM4_D) - BMDFILM4_C;
    }
}

/* ----------------------------------------------------------------
 * Filmlight T-Log / E-Log
 * ---------------------------------------------------------------- */

/* T-Log OETF: linear -> T-Log
 * Formula from Filmlight documentation */
static alwan_scalar tlog_oetf_scalar(alwan_scalar linear) {
    alwan_scalar const a = ALWAN_LITERAL(0.01);
    alwan_scalar const b = ALWAN_LITERAL(0.0);
    alwan_scalar const c = ALWAN_LITERAL(0.33);
    alwan_scalar const d = ALWAN_LITERAL(0.02);

    if (linear <= a) {
        return linear / a * d;
    } else {
        return c * ALWAN_LN((linear + b) / a) / ALWAN_LN(ALWAN_LITERAL(10.0)) + d;
    }
}

static alwan_scalar tlog_eotf_scalar(alwan_scalar encoded) {
    alwan_scalar const a = ALWAN_LITERAL(0.01);
    alwan_scalar const b = ALWAN_LITERAL(0.0);
    alwan_scalar const c = ALWAN_LITERAL(0.33);
    alwan_scalar const d = ALWAN_LITERAL(0.02);

    if (encoded <= d) {
        return encoded / d * a;
    } else {
        return ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - d) / c) * a - b;
    }
}

/* E-Log OETF: linear -> E-Log (Filmlight)
 * Formula from Filmlight documentation */
static alwan_scalar elog_oetf_scalar(alwan_scalar linear) {
    if (linear < ALWAN_LITERAL(0.0)) {
        linear = ALWAN_LITERAL(0.0);
    }

    return ALWAN_LN(linear + ALWAN_LITERAL(1.0)) / ALWAN_LN(ALWAN_LITERAL(10.0)) * ALWAN_LITERAL(0.4) + ALWAN_LITERAL(0.6);
}

static alwan_scalar elog_eotf_scalar(alwan_scalar encoded) {
    return ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - ALWAN_LITERAL(0.6)) / ALWAN_LITERAL(0.4)) - ALWAN_LITERAL(1.0);
}

/* ----------------------------------------------------------------
 * GoPro Protune
 * ---------------------------------------------------------------- */

/* Protune OETF: linear -> Protune
 * Formula from GoPro documentation */
static alwan_scalar protune_oetf_scalar(alwan_scalar linear) {
    if (linear < ALWAN_LITERAL(0.0)) {
        linear = ALWAN_LITERAL(0.0);
    }

    return ALWAN_LN(linear * ALWAN_LITERAL(112.0) + ALWAN_LITERAL(1.0)) / ALWAN_LN(ALWAN_LITERAL(113.0));
}

static alwan_scalar protune_eotf_scalar(alwan_scalar encoded) {
    return (ALWAN_POW(ALWAN_LITERAL(113.0), encoded) - ALWAN_LITERAL(1.0)) / ALWAN_LITERAL(112.0);
}

/* ----------------------------------------------------------------
 * Standard Gamma Variants
 * ---------------------------------------------------------------- */

/* Gamma 2.2 */
static alwan_scalar gamma22_oetf_scalar(alwan_scalar linear) {
    if (linear < ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }
    return ALWAN_POW(linear, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.2));
}

static alwan_scalar gamma22_eotf_scalar(alwan_scalar encoded) {
    if (encoded < ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }
    return ALWAN_POW(encoded, ALWAN_LITERAL(2.2));
}

/* Gamma 2.4 */
static alwan_scalar gamma24_oetf_scalar(alwan_scalar linear) {
    if (linear < ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }
    return ALWAN_POW(linear, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));
}

static alwan_scalar gamma24_eotf_scalar(alwan_scalar encoded) {
    if (encoded < ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }
    return ALWAN_POW(encoded, ALWAN_LITERAL(2.4));
}

/* Gamma 2.6 */
static alwan_scalar gamma26_oetf_scalar(alwan_scalar linear) {
    if (linear < ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }
    return ALWAN_POW(linear, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.6));
}

static alwan_scalar gamma26_eotf_scalar(alwan_scalar encoded) {
    if (encoded < ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }
    return ALWAN_POW(encoded, ALWAN_LITERAL(2.6));
}

/* Gamma 2.8 */
static alwan_scalar gamma28_oetf_scalar(alwan_scalar linear) {
    if (linear < ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }
    return ALWAN_POW(linear, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.8));
}

static alwan_scalar gamma28_eotf_scalar(alwan_scalar encoded) {
    if (encoded < ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }
    return ALWAN_POW(encoded, ALWAN_LITERAL(2.8));
}

/* ----------------------------------------------------------------
 * Nikon N-Log
 * ---------------------------------------------------------------- */

/* N-Log OETF: linear -> N-Log
 * Formula from Nikon documentation */
static alwan_scalar nlog_oetf_scalar(alwan_scalar linear) {
    alwan_scalar const cut = ALWAN_LITERAL(0.00570);

    if (linear < cut) {
        return ALWAN_LITERAL(150.0) * linear + ALWAN_LITERAL(0.11241);
    } else {
        return (ALWAN_LITERAL(0.36) * ALWAN_LN(linear + ALWAN_LITERAL(0.10033)) / ALWAN_LN(ALWAN_LITERAL(10.0))) + ALWAN_LITERAL(0.71722);
    }
}

static alwan_scalar nlog_eotf_scalar(alwan_scalar encoded) {
    alwan_scalar const cut = ALWAN_LITERAL(0.00570);
    alwan_scalar threshold = ALWAN_LITERAL(150.0) * cut + ALWAN_LITERAL(0.11241);

    if (encoded < threshold) {
        return (encoded - ALWAN_LITERAL(0.11241)) / ALWAN_LITERAL(150.0);
    } else {
        return ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - ALWAN_LITERAL(0.71722)) / ALWAN_LITERAL(0.36)) - ALWAN_LITERAL(0.10033);
    }
}
/* Cineon / DPX film log encoding
 * Reference: Sony Imageworks (2012)
 * black_offset = 10^((95 - 685) / 300) */
static alwan_scalar const CINEON_BLACK_OFFSET = ALWAN_LITERAL(0.010797751623277);

static alwan_scalar cineon_oetf_scalar(alwan_scalar linear) {
    /* y = (685 + 300 * log10(x * (1 - black_offset) + black_offset)) / 1023 */
    alwan_scalar x = linear;
    if (x < ALWAN_LITERAL(0.0)) x = ALWAN_LITERAL(0.0);

    alwan_scalar arg = x * (ALWAN_LITERAL(1.0) - CINEON_BLACK_OFFSET) + CINEON_BLACK_OFFSET;
    if (arg <= ALWAN_LITERAL(0.0)) arg = ALWAN_LITERAL(1e-10);

    return (ALWAN_LITERAL(685.0) + ALWAN_LITERAL(300.0) * ALWAN_LOG10(arg)) / ALWAN_LITERAL(1023.0);
}

static alwan_scalar cineon_eotf_scalar(alwan_scalar encoded) {
    /* x = (10^((1023 * y - 685) / 300) - black_offset) / (1 - black_offset) */
    alwan_scalar y = encoded;
    alwan_scalar exponent = (ALWAN_LITERAL(1023.0) * y - ALWAN_LITERAL(685.0)) / ALWAN_LITERAL(300.0);
    alwan_scalar x = (ALWAN_POW(ALWAN_LITERAL(10.0), exponent) - CINEON_BLACK_OFFSET) /
                     (ALWAN_LITERAL(1.0) - CINEON_BLACK_OFFSET);
    return x;
}

/* Apple Log (iPhone 15 Pro+)
 * Reference: Apple Log Profile White Paper
 * Uses BT.2020 primaries with Apple-defined log curve
 * Constants from Apple specification */
static alwan_scalar const APPLE_LOG_R0    = ALWAN_LITERAL(-0.05641088);
static alwan_scalar const APPLE_LOG_RT    = ALWAN_LITERAL(0.01);
static alwan_scalar const APPLE_LOG_C     = ALWAN_LITERAL(47.28711236);
static alwan_scalar const APPLE_LOG_BETA  = ALWAN_LITERAL(0.00964052);
static alwan_scalar const APPLE_LOG_GAMMA = ALWAN_LITERAL(0.08550479);
static alwan_scalar const APPLE_LOG_DELTA = ALWAN_LITERAL(0.69336945);
/* Pt = c * (Rt - R0)^2 = 47.28711236 * (0.01 - (-0.05641088))^2 */
static alwan_scalar const APPLE_LOG_PT    = ALWAN_LITERAL(0.20855531595464202);

static alwan_scalar apple_log_oetf_scalar(alwan_scalar linear) {
    /* Encoding: Linear L to Apple Log V
     * V = 0, when L < R0
     * V = c * (L - R0)^2, when R0 <= L < Rt
     * V = gamma * log2(L + beta) + delta, when L >= Rt */
    if (linear < APPLE_LOG_R0) {
        return ALWAN_LITERAL(0.0);
    } else if (linear < APPLE_LOG_RT) {
        alwan_scalar diff = linear - APPLE_LOG_R0;
        return APPLE_LOG_C * diff * diff;
    } else {
        return APPLE_LOG_GAMMA * ALWAN_LOG2(linear + APPLE_LOG_BETA) + APPLE_LOG_DELTA;
    }
}

static alwan_scalar apple_log_eotf_scalar(alwan_scalar encoded) {
    /* Decoding: Apple Log V to Linear L
     * L = R0, when V < 0
     * L = sqrt(V/c) + R0, when 0 <= V < Pt
     * L = 2^((V - delta)/gamma) - beta, when V >= Pt */
    if (encoded < ALWAN_LITERAL(0.0)) {
        return APPLE_LOG_R0;
    } else if (encoded < APPLE_LOG_PT) {
        return ALWAN_SQRT(encoded / APPLE_LOG_C) + APPLE_LOG_R0;
    } else {
        return ALWAN_POW(ALWAN_LITERAL(2.0), (encoded - APPLE_LOG_DELTA) / APPLE_LOG_GAMMA) - APPLE_LOG_BETA;
    }
}

/* ----------------------------------------------------------------
 * Fujifilm F-Log
 * Reference: Fujifilm F-Log Data Sheet
 * Gamut: BT.2020 primaries (F-Gamut)
 * ---------------------------------------------------------------- */
static alwan_scalar const FLOG_CUT1 = ALWAN_LITERAL(0.00089);
static alwan_scalar const FLOG_CUT2 = ALWAN_LITERAL(0.100537775223865);
static alwan_scalar const FLOG_A = ALWAN_LITERAL(0.555556);
static alwan_scalar const FLOG_B = ALWAN_LITERAL(0.009468);
static alwan_scalar const FLOG_C = ALWAN_LITERAL(0.344676);
static alwan_scalar const FLOG_D = ALWAN_LITERAL(0.790453);
static alwan_scalar const FLOG_E = ALWAN_LITERAL(8.735631);
static alwan_scalar const FLOG_F = ALWAN_LITERAL(0.092864);

static alwan_scalar flog_oetf_scalar(alwan_scalar linear) {
    /* F-Log encoding: Linear L to F-Log V
     * V = e * L + f, when L < cut1
     * V = c * log10(a * L + b) + d, when L >= cut1 */
    if (linear < FLOG_CUT1) {
        return FLOG_E * linear + FLOG_F;
    } else {
        return FLOG_C * ALWAN_LOG10(FLOG_A * linear + FLOG_B) + FLOG_D;
    }
}

static alwan_scalar flog_eotf_scalar(alwan_scalar encoded) {
    /* F-Log decoding: F-Log V to Linear L
     * L = (V - f) / e, when V < cut2
     * L = (10^((V - d) / c) - b) / a, when V >= cut2 */
    if (encoded < FLOG_CUT2) {
        return (encoded - FLOG_F) / FLOG_E;
    } else {
        return (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - FLOG_D) / FLOG_C) - FLOG_B) / FLOG_A;
    }
}

/* ----------------------------------------------------------------
 * Fujifilm F-Log2
 * Reference: Fujifilm F-Log2 Data Sheet
 * Gamut: BT.2020 primaries (F-Gamut)
 * ---------------------------------------------------------------- */
static alwan_scalar const FLOG2_CUT1 = ALWAN_LITERAL(0.000889);
static alwan_scalar const FLOG2_CUT2 = ALWAN_LITERAL(0.100686685370811);
static alwan_scalar const FLOG2_A = ALWAN_LITERAL(5.555556);
static alwan_scalar const FLOG2_B = ALWAN_LITERAL(0.064829);
static alwan_scalar const FLOG2_C = ALWAN_LITERAL(0.245281);
static alwan_scalar const FLOG2_D = ALWAN_LITERAL(0.384316);
static alwan_scalar const FLOG2_E = ALWAN_LITERAL(8.799461);
static alwan_scalar const FLOG2_F = ALWAN_LITERAL(0.092864);

static alwan_scalar flog2_oetf_scalar(alwan_scalar linear) {
    /* F-Log2 encoding: Linear L to F-Log2 V
     * V = e * L + f, when L < cut1
     * V = c * log10(a * L + b) + d, when L >= cut1 */
    if (linear < FLOG2_CUT1) {
        return FLOG2_E * linear + FLOG2_F;
    } else {
        return FLOG2_C * ALWAN_LOG10(FLOG2_A * linear + FLOG2_B) + FLOG2_D;
    }
}

static alwan_scalar flog2_eotf_scalar(alwan_scalar encoded) {
    /* F-Log2 decoding: F-Log2 V to Linear L
     * L = (V - f) / e, when V < cut2
     * L = (10^((V - d) / c) - b) / a, when V >= cut2 */
    if (encoded < FLOG2_CUT2) {
        return (encoded - FLOG2_F) / FLOG2_E;
    } else {
        return (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - FLOG2_D) / FLOG2_C) - FLOG2_B) / FLOG2_A;
    }
}

/* ----------------------------------------------------------------
 * Leica L-Log
 * Reference: Leica L-Log Reference Manual
 * Gamut: BT.2020 primaries (L-Gamut)
 * Similar to Panasonic V-Log structure
 * ---------------------------------------------------------------- */
static alwan_scalar const LLOG_CUT = ALWAN_LITERAL(0.01);
static alwan_scalar const LLOG_CUT_ENC = ALWAN_LITERAL(0.125);  /* Encoded cut point */
static alwan_scalar const LLOG_A = ALWAN_LITERAL(5.555556);     /* d coefficient for log */
static alwan_scalar const LLOG_B = ALWAN_LITERAL(0.064);        /* e coefficient for log */
static alwan_scalar const LLOG_C = ALWAN_LITERAL(0.241514);     /* c coefficient for log */
static alwan_scalar const LLOG_D = ALWAN_LITERAL(0.598206);     /* f coefficient for log */
static alwan_scalar const LLOG_E = ALWAN_LITERAL(5.6);          /* Linear slope */
static alwan_scalar const LLOG_F = ALWAN_LITERAL(0.069);        /* Linear offset */

static alwan_scalar llog_oetf_scalar(alwan_scalar linear) {
    /* L-Log encoding: Linear to L-Log
     * For linear < cut: V = E * linear + F (linear region)
     * For linear >= cut: V = C * log10(A * linear + B) + D (log region) */
    if (linear < LLOG_CUT) {
        return LLOG_E * linear + LLOG_F;
    } else {
        return LLOG_C * ALWAN_LOG10(LLOG_A * linear + LLOG_B) + LLOG_D;
    }
}

static alwan_scalar llog_eotf_scalar(alwan_scalar encoded) {
    /* L-Log decoding: L-Log to Linear
     * For encoded < cut_enc: linear = (V - F) / E
     * For encoded >= cut_enc: linear = (10^((V - D) / C) - B) / A */
    if (encoded < LLOG_CUT_ENC) {
        return (encoded - LLOG_F) / LLOG_E;
    } else {
        return (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - LLOG_D) / LLOG_C) - LLOG_B) / LLOG_A;
    }
}

/* ----------------------------------------------------------------
 * DJI D-Log Transfer Function
 * Reference: DJI Cinema Color System Technical Whitepaper
 * ---------------------------------------------------------------- */
static alwan_scalar const DLOG_CUT = ALWAN_LITERAL(0.0078);
static alwan_scalar const DLOG_CUT_ENC = ALWAN_LITERAL(0.14);
static alwan_scalar const DLOG_LIN_SLOPE = ALWAN_LITERAL(6.025);
static alwan_scalar const DLOG_LIN_OFFSET = ALWAN_LITERAL(0.0929);
static alwan_scalar const DLOG_LOG_MULT = ALWAN_LITERAL(0.256663);
static alwan_scalar const DLOG_LOG_OFFSET = ALWAN_LITERAL(0.584555);
static alwan_scalar const DLOG_LOG_SCALE = ALWAN_LITERAL(0.9892);
static alwan_scalar const DLOG_LOG_BIAS = ALWAN_LITERAL(0.0108);

static alwan_scalar dlog_oetf_scalar(alwan_scalar linear) {
    /* DJI D-Log OETF:
     * For linear <= 0.0078: y = 6.025 * x + 0.0929
     * For linear > 0.0078: y = log10(x * 0.9892 + 0.0108) * 0.256663 + 0.584555 */
    if (linear <= DLOG_CUT) {
        return DLOG_LIN_SLOPE * linear + DLOG_LIN_OFFSET;
    } else {
        return ALWAN_LOG10(linear * DLOG_LOG_SCALE + DLOG_LOG_BIAS) * DLOG_LOG_MULT + DLOG_LOG_OFFSET;
    }
}

static alwan_scalar dlog_eotf_scalar(alwan_scalar encoded) {
    /* DJI D-Log EOTF:
     * For encoded <= 0.14: x = (y - 0.0929) / 6.025
     * For encoded > 0.14: x = (10^(3.89616 * y - 2.27752) - 0.0108) / 0.9892
     * Note: 3.89616 = 1/0.256663, 2.27752 = 0.584555/0.256663 */
    if (encoded <= DLOG_CUT_ENC) {
        return (encoded - DLOG_LIN_OFFSET) / DLOG_LIN_SLOPE;
    } else {
        return (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - DLOG_LOG_OFFSET) / DLOG_LOG_MULT) - DLOG_LOG_BIAS) / DLOG_LOG_SCALE;
    }
}

/* DCDM (Digital Cinema Distribution Master)
 * Reference: SMPTE ST 428-1
 * Gamma 2.6 encoding with 48/52.37 luminance scaling
 * Applied to CIE XYZ values (not RGB) */
static alwan_scalar const DCDM_SCALE = ALWAN_LITERAL(0.9165552797403094);  /* 48/52.37 */
static alwan_scalar const DCDM_INV_SCALE = ALWAN_LITERAL(1.0910416666666667);  /* 52.37/48 */
static alwan_scalar const DCDM_GAMMA = ALWAN_LITERAL(2.6);
static alwan_scalar const DCDM_INV_GAMMA = ALWAN_LITERAL(0.38461538461538464);  /* 1/2.6 */

static alwan_scalar dcdm_oetf_scalar(alwan_scalar linear) {
    /* DCDM encoding: X' = (X * 48/52.37)^(1/2.6) */
    if (linear <= ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }
    return ALWAN_POW(linear * DCDM_SCALE, DCDM_INV_GAMMA);
}

static alwan_scalar dcdm_eotf_scalar(alwan_scalar encoded) {
    /* DCDM decoding: X = (X'^2.6) * 52.37/48 */
    if (encoded <= ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }
    return ALWAN_POW(encoded, DCDM_GAMMA) * DCDM_INV_SCALE;
}

/* Linear / Identity */
static alwan_scalar linear_identity_scalar(alwan_scalar v) {
    return v;
}

/* ACEScc constants (SMPTE ST 2065-5)
 * ACEScc is a logarithmic encoding for color grading
 * Reference: Academy S-2014-003 */
static alwan_scalar const ACESCC_MIN_CUTOFF = ALWAN_LITERAL(0.00003051757812);  /* 2^-15 */
static alwan_scalar const ACESCC_LOG2_E = ALWAN_LITERAL(1.4426950408889634);  /* 1/ln(2) */

static alwan_scalar acescc_oetf_scalar(alwan_scalar linear) {
    /* ACEScc encoding:
     * if x < 0: (log2(2^-16) + 9.72) / 17.52
     * if x < 2^-15: (log2(2^-16 + x*0.5) + 9.72) / 17.52
     * else: (log2(x) + 9.72) / 17.52 */
    if (linear <= ALWAN_LITERAL(0.0)) {
        return (ALWAN_LITERAL(-16.0) + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);  /* -0.3584474886 */
    } else if (linear < ACESCC_MIN_CUTOFF) {
        alwan_scalar val = ALWAN_LITERAL(0.0000152587890625) + linear * ALWAN_LITERAL(0.5);  /* 2^-16 + x*0.5 */
        return (ALWAN_LN(val) * ACESCC_LOG2_E + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);
    } else {
        return (ALWAN_LN(linear) * ACESCC_LOG2_E + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);
    }
}

static alwan_scalar acescc_eotf_scalar(alwan_scalar encoded) {
    /* ACEScc decoding:
     * if encoded < (9.72-15)/17.52: (2^(encoded*17.52-9.72) - 2^-16)*2
     * if encoded < (log2(65504)+9.72)/17.52: 2^(encoded*17.52-9.72)
     * else: 65504 */
    alwan_scalar const neg_cutoff = (ALWAN_LITERAL(9.72) - ALWAN_LITERAL(15.0)) / ALWAN_LITERAL(17.52);
    alwan_scalar const max_cutoff = (ALWAN_LN(ALWAN_LITERAL(65504.0)) * ACESCC_LOG2_E + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);

    if (encoded < neg_cutoff) {
        return (ALWAN_POW(ALWAN_LITERAL(2.0), encoded * ALWAN_LITERAL(17.52) - ALWAN_LITERAL(9.72)) - ALWAN_LITERAL(0.0000152587890625)) * ALWAN_LITERAL(2.0);
    } else if (encoded < max_cutoff) {
        return ALWAN_POW(ALWAN_LITERAL(2.0), encoded * ALWAN_LITERAL(17.52) - ALWAN_LITERAL(9.72));
    } else {
        return ALWAN_LITERAL(65504.0);
    }
}

/* ACEScct constants (SMPTE ST 2065-5)
 * ACEScct is ACEScc with a toe for better shadow handling
 * Reference: Academy S-2016-001 */
static alwan_scalar const ACESCCT_CUT = ALWAN_LITERAL(0.0078125);  /* 2^-7 */
static alwan_scalar const ACESCCT_A = ALWAN_LITERAL(10.5402377416545);
static alwan_scalar const ACESCCT_B = ALWAN_LITERAL(0.0729055341958355);

static alwan_scalar acescct_oetf_scalar(alwan_scalar linear) {
    /* ACEScct encoding:
     * if x <= 0.0078125: 10.5402377416545 * x + 0.0729055341958355
     * else: (log2(x) + 9.72) / 17.52 */
    if (linear <= ACESCCT_CUT) {
        return ACESCCT_A * linear + ACESCCT_B;
    } else {
        return (ALWAN_LN(linear) * ACESCC_LOG2_E + ALWAN_LITERAL(9.72)) / ALWAN_LITERAL(17.52);
    }
}

static alwan_scalar acescct_eotf_scalar(alwan_scalar encoded) {
    /* ACEScct decoding:
     * if encoded <= 0.155251141552511: (encoded - 0.0729055341958355) / 10.5402377416545
     * else: 2^(encoded*17.52-9.72) */
    alwan_scalar const cut = ALWAN_LITERAL(0.155251141552511);  /* ACEScct value at linear 0.0078125 */

    if (encoded <= cut) {
        return (encoded - ACESCCT_B) / ACESCCT_A;
    } else {
        return ALWAN_POW(ALWAN_LITERAL(2.0), encoded * ALWAN_LITERAL(17.52) - ALWAN_LITERAL(9.72));
    }
}

/* ADX constants (SMPTE ST 2065-3)
 * Academy Density Exchange encoding for film scanning
 * CV = (density + 0.5) * scale, then normalized to [0,1] */
static alwan_scalar const ADX_REF_PT = ALWAN_LITERAL(0.5);
static alwan_scalar const ADX10_SCALE = ALWAN_LITERAL(400.0);
static alwan_scalar const ADX16_SCALE = ALWAN_LITERAL(25600.0);
static alwan_scalar const ADX10_NORM = ALWAN_LITERAL(1023.0);
static alwan_scalar const ADX16_NORM = ALWAN_LITERAL(65535.0);

static alwan_scalar adx10_oetf_scalar(alwan_scalar density) {
    /* ADX10 encoding: CV = clamp((density + 0.5) * 400, 0, 1023) / 1023 */
    alwan_scalar cv = (density + ADX_REF_PT) * ADX10_SCALE;
    if (cv < ALWAN_LITERAL(0.0)) cv = ALWAN_LITERAL(0.0);
    if (cv > ADX10_NORM) cv = ADX10_NORM;
    return cv / ADX10_NORM;
}

static alwan_scalar adx10_eotf_scalar(alwan_scalar encoded) {
    /* ADX10 decoding: density = encoded * 1023 / 400 - 0.5 */
    return encoded * ADX10_NORM / ADX10_SCALE - ADX_REF_PT;
}

static alwan_scalar adx16_oetf_scalar(alwan_scalar density) {
    /* ADX16 encoding: CV = clamp((density + 0.5) * 25600, 0, 65535) / 65535 */
    alwan_scalar cv = (density + ADX_REF_PT) * ADX16_SCALE;
    if (cv < ALWAN_LITERAL(0.0)) cv = ALWAN_LITERAL(0.0);
    if (cv > ADX16_NORM) cv = ADX16_NORM;
    return cv / ADX16_NORM;
}

static alwan_scalar adx16_eotf_scalar(alwan_scalar encoded) {
    /* ADX16 decoding: density = encoded * 65535 / 25600 - 0.5 */
    return encoded * ADX16_NORM / ADX16_SCALE - ADX_REF_PT;
}


/* ----------------------------------------------------------------
 * Transfer Function API
 * ---------------------------------------------------------------- */

int alwan_oetf_apply(alwan_scalar *encoded,
                     alwan_transfer_function tf,
                     alwan_scalar const *linear, size_t count, size_t in_stride,
                     size_t out_stride) {
    if (!linear || !encoded) {
        return ALWAN_E_INVALID;
    }

    /* Select transfer function */
    alwan_scalar (*oetf_fn)(alwan_scalar) = NULL;

    switch (tf) {
        case ALWAN_TF_SRGB:
            oetf_fn = srgb_oetf_scalar;
            break;
        case ALWAN_TF_BT709:
        case ALWAN_TF_BT2020:
            oetf_fn = bt2020_oetf_scalar;  /* BT.709 and BT.2020 use same transfer function */
            break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084:
            oetf_fn = pq_oetf_scalar;
            break;
        case ALWAN_TF_HLG:
            oetf_fn = hlg_oetf_scalar;
            break;
        case ALWAN_TF_ACESPROXY:
            oetf_fn = acesproxy_oetf_scalar;
            break;
        case ALWAN_TF_BT1886:
            /* BT.1886 EOTF is gamma 2.4, so OETF is gamma 1/2.4 */
            oetf_fn = gamma24_oetf_scalar;
            break;

        /* Extended Transfer Functions */
        case ALWAN_TF_SLOG:
            oetf_fn = slog_oetf_scalar;
            break;
        case ALWAN_TF_SLOG2:
            oetf_fn = slog2_oetf_scalar;
            break;
        case ALWAN_TF_SLOG3:
            oetf_fn = slog3_oetf_scalar;
            break;
        case ALWAN_TF_CLOG:
            oetf_fn = clog_oetf_scalar;
            break;
        case ALWAN_TF_CLOG2:
            oetf_fn = clog2_oetf_scalar;
            break;
        case ALWAN_TF_CLOG3:
            oetf_fn = clog3_oetf_scalar;
            break;
        case ALWAN_TF_VLOG:
            oetf_fn = vlog_oetf_scalar;
            break;
        case ALWAN_TF_LOGC3:
            oetf_fn = logc3_oetf_scalar;
            break;
        case ALWAN_TF_LOGC4:
            oetf_fn = logc4_oetf_scalar;
            break;
        case ALWAN_TF_REDLOG:
            oetf_fn = redlog_oetf_scalar;
            break;
        case ALWAN_TF_REDLOGFILM:
            oetf_fn = redlogfilm_oetf_scalar;
            break;
        case ALWAN_TF_LOG3G10:
            oetf_fn = log3g10_oetf_scalar;
            break;
        case ALWAN_TF_BMDFILM:
            oetf_fn = bmdfilm_oetf_scalar;
            break;
        case ALWAN_TF_BMDFILM4:
            oetf_fn = bmdfilm4_oetf_scalar;
            break;
        case ALWAN_TF_TLOG:
            oetf_fn = tlog_oetf_scalar;
            break;
        case ALWAN_TF_ELOG:
            oetf_fn = elog_oetf_scalar;
            break;
        case ALWAN_TF_PROTUNE:
            oetf_fn = protune_oetf_scalar;
            break;
        case ALWAN_TF_GAMMA22:
            oetf_fn = gamma22_oetf_scalar;
            break;
        case ALWAN_TF_GAMMA24:
            oetf_fn = gamma24_oetf_scalar;
            break;
        case ALWAN_TF_GAMMA26:
            oetf_fn = gamma26_oetf_scalar;
            break;
        case ALWAN_TF_GAMMA28:
            oetf_fn = gamma28_oetf_scalar;
            break;
        case ALWAN_TF_NLOG:
            oetf_fn = nlog_oetf_scalar;
            break;

        case ALWAN_TF_CINEON:
            oetf_fn = cineon_oetf_scalar;
            break;

        case ALWAN_TF_APPLE_LOG:
            oetf_fn = apple_log_oetf_scalar;
            break;

        case ALWAN_TF_FLOG:
            oetf_fn = flog_oetf_scalar;
            break;
        case ALWAN_TF_FLOG2:
            oetf_fn = flog2_oetf_scalar;
            break;

        case ALWAN_TF_LLOG:
            oetf_fn = llog_oetf_scalar;
            break;

        case ALWAN_TF_DLOG:
            oetf_fn = dlog_oetf_scalar;
            break;

        case ALWAN_TF_DCDM:
            oetf_fn = dcdm_oetf_scalar;
            break;

        case ALWAN_TF_LINEAR:
            oetf_fn = linear_identity_scalar;
            break;

        case ALWAN_TF_ACESCC:
            oetf_fn = acescc_oetf_scalar;
            break;

        case ALWAN_TF_ACESCCT:
            oetf_fn = acescct_oetf_scalar;
            break;

        case ALWAN_TF_ADX10:
            oetf_fn = adx10_oetf_scalar;
            break;

        case ALWAN_TF_ADX16:
            oetf_fn = adx16_oetf_scalar;
            break;

        default:
            return ALWAN_E_INVALID;
    }

    /* Apply transfer function to array */
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)linear + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)encoded + i * out_stride);
        *out_ptr = oetf_fn(*in_ptr);
    }

    return ALWAN_OK;
}

int alwan_eotf_apply(alwan_scalar *linear,
                     alwan_transfer_function tf,
                     alwan_scalar const *encoded, size_t count, size_t in_stride,
                     size_t out_stride) {
    if (!encoded || !linear) {
        return ALWAN_E_INVALID;
    }

    /* Select transfer function */
    alwan_scalar (*eotf_fn)(alwan_scalar) = NULL;

    switch (tf) {
        case ALWAN_TF_SRGB:
            eotf_fn = srgb_eotf_scalar;
            break;
        case ALWAN_TF_BT709:
        case ALWAN_TF_BT2020:
            eotf_fn = bt2020_eotf_scalar;  /* BT.709 and BT.2020 use same transfer function */
            break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084:
            eotf_fn = pq_eotf_scalar;
            break;
        case ALWAN_TF_HLG:
            eotf_fn = hlg_eotf_scalar;
            break;
        case ALWAN_TF_BT1886:
            eotf_fn = bt1886_eotf_scalar;
            break;
        case ALWAN_TF_ACESPROXY:
            eotf_fn = acesproxy_eotf_scalar;
            break;

        /* Extended Transfer Functions */
        case ALWAN_TF_SLOG:
            eotf_fn = slog_eotf_scalar;
            break;
        case ALWAN_TF_SLOG2:
            eotf_fn = slog2_eotf_scalar;
            break;
        case ALWAN_TF_SLOG3:
            eotf_fn = slog3_eotf_scalar;
            break;
        case ALWAN_TF_CLOG:
            eotf_fn = clog_eotf_scalar;
            break;
        case ALWAN_TF_CLOG2:
            eotf_fn = clog2_eotf_scalar;
            break;
        case ALWAN_TF_CLOG3:
            eotf_fn = clog3_eotf_scalar;
            break;
        case ALWAN_TF_VLOG:
            eotf_fn = vlog_eotf_scalar;
            break;
        case ALWAN_TF_LOGC3:
            eotf_fn = logc3_eotf_scalar;
            break;
        case ALWAN_TF_LOGC4:
            eotf_fn = logc4_eotf_scalar;
            break;
        case ALWAN_TF_REDLOG:
            eotf_fn = redlog_eotf_scalar;
            break;
        case ALWAN_TF_REDLOGFILM:
            eotf_fn = redlogfilm_eotf_scalar;
            break;
        case ALWAN_TF_LOG3G10:
            eotf_fn = log3g10_eotf_scalar;
            break;
        case ALWAN_TF_BMDFILM:
            eotf_fn = bmdfilm_eotf_scalar;
            break;
        case ALWAN_TF_BMDFILM4:
            eotf_fn = bmdfilm4_eotf_scalar;
            break;
        case ALWAN_TF_TLOG:
            eotf_fn = tlog_eotf_scalar;
            break;
        case ALWAN_TF_ELOG:
            eotf_fn = elog_eotf_scalar;
            break;
        case ALWAN_TF_PROTUNE:
            eotf_fn = protune_eotf_scalar;
            break;
        case ALWAN_TF_GAMMA22:
            eotf_fn = gamma22_eotf_scalar;
            break;
        case ALWAN_TF_GAMMA24:
            eotf_fn = gamma24_eotf_scalar;
            break;
        case ALWAN_TF_GAMMA26:
            eotf_fn = gamma26_eotf_scalar;
            break;
        case ALWAN_TF_GAMMA28:
            eotf_fn = gamma28_eotf_scalar;
            break;
        case ALWAN_TF_NLOG:
            eotf_fn = nlog_eotf_scalar;
            break;

        case ALWAN_TF_CINEON:
            eotf_fn = cineon_eotf_scalar;
            break;

        case ALWAN_TF_APPLE_LOG:
            eotf_fn = apple_log_eotf_scalar;
            break;

        case ALWAN_TF_FLOG:
            eotf_fn = flog_eotf_scalar;
            break;
        case ALWAN_TF_FLOG2:
            eotf_fn = flog2_eotf_scalar;
            break;

        case ALWAN_TF_LLOG:
            eotf_fn = llog_eotf_scalar;
            break;

        case ALWAN_TF_DLOG:
            eotf_fn = dlog_eotf_scalar;
            break;

        case ALWAN_TF_DCDM:
            eotf_fn = dcdm_eotf_scalar;
            break;

        case ALWAN_TF_LINEAR:
            eotf_fn = linear_identity_scalar;
            break;

        case ALWAN_TF_ACESCC:
            eotf_fn = acescc_eotf_scalar;
            break;

        case ALWAN_TF_ACESCCT:
            eotf_fn = acescct_eotf_scalar;
            break;

        case ALWAN_TF_ADX10:
            eotf_fn = adx10_eotf_scalar;
            break;

        case ALWAN_TF_ADX16:
            eotf_fn = adx16_eotf_scalar;
            break;

        default:
            return ALWAN_E_INVALID;
    }

    /* Apply transfer function to array */
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)encoded + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)linear + i * out_stride);
        *out_ptr = eotf_fn(*in_ptr);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * M11: RGB <-> RGB Conversion
 * ---------------------------------------------------------------- */

/* Convert RGB color from one color space to another */
int alwan_rgb_convert(alwan_rgb *dst_rgb,
                      alwan_ctx *ctx,
                      alwan_rgb_space_desc const *src_space,
                      alwan_rgb_space_desc const *dst_space,
                      alwan_rgb const *src_rgb) {
    if (!src_space || !dst_space || !src_rgb || !dst_rgb) {
        return ALWAN_E_INVALID;
    }

    /* Derive conversion matrices for both spaces */
    alwan_mat3x3 src_to_xyz, xyz_to_src;
    alwan_mat3x3 dst_to_xyz, xyz_to_dst;

    int status = alwan_rgb_derive_matrices(&src_to_xyz, &xyz_to_src, src_space);
    if (status != ALWAN_OK) return status;

    status = alwan_rgb_derive_matrices(&dst_to_xyz, &xyz_to_dst, dst_space);
    if (status != ALWAN_OK) return status;

    /* Convert source RGB to XYZ */
    alwan_vec3 xyz, vec_in;
    ALWAN_MEMCPY(&vec_in, src_rgb, sizeof(alwan_vec3));
    alwan_mat3_mulv(&xyz, &src_to_xyz, &vec_in);

    /* Check if chromatic adaptation is needed */
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
    alwan_scalar dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_scalar dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_adaptation = (ALWAN_ABS(dx) > tolerance || ALWAN_ABS(dy) > tolerance);

    if (need_adaptation && ctx) {
        /* Perform chromatic adaptation using Bradford CAT */
        alwan_xyy src_white_xyy, dst_white_xyy;
        alwan_xyz src_white_xyz, dst_white_xyz;

        /* Convert xy to XYZ (using Y=1.0) */
        src_white_xyy.x = src_space->white_xy[0];
        src_white_xyy.y = src_space->white_xy[1];
        src_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&src_white_xyz, &src_white_xyy);

        dst_white_xyy.x = dst_space->white_xy[0];
        dst_white_xyy.y = dst_space->white_xy[1];
        dst_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&dst_white_xyz, &dst_white_xyy);

        /* Compute and apply CAT matrix */
        alwan_mat3x3 cat_matrix;
        status = alwan_cat_matrix(&cat_matrix, &src_white_xyz, &dst_white_xyz,
                                  ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        alwan_vec3 xyz_adapted;
        alwan_mat3_mulv(&xyz_adapted, &cat_matrix, &xyz);
        xyz = xyz_adapted;
    }

    /* Convert adapted XYZ to destination RGB */
    alwan_vec3 vec_out;
    alwan_mat3_mulv(&vec_out, &xyz_to_dst, &xyz);
    ALWAN_MEMCPY(dst_rgb, &vec_out, sizeof(alwan_vec3));

    return ALWAN_OK;
}

/* Bulk RGB color space conversion */
int alwan_rgb_convert_bulk(alwan_rgb *dst_rgb,
                            alwan_ctx *ctx,
                            alwan_rgb_space_desc const *src_space,
                            alwan_rgb_space_desc const *dst_space,
                            alwan_rgb const *src_rgb,
                            size_t count) {
    if (!src_space || !dst_space || !src_rgb || !dst_rgb || count == 0) {
        return ALWAN_E_INVALID;
    }

    /* Derive conversion matrices once for all colors */
    alwan_mat3x3 src_to_xyz, xyz_to_src;
    alwan_mat3x3 dst_to_xyz, xyz_to_dst;

    int status = alwan_rgb_derive_matrices(&src_to_xyz, &xyz_to_src, src_space);
    if (status != ALWAN_OK) return status;

    status = alwan_rgb_derive_matrices(&dst_to_xyz, &xyz_to_dst, dst_space);
    if (status != ALWAN_OK) return status;

    /* Check if chromatic adaptation is needed */
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
    alwan_scalar dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_scalar dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_adaptation = (ALWAN_ABS(dx) > tolerance || ALWAN_ABS(dy) > tolerance);

    /* Precompute adaptation matrix if needed */
    alwan_mat3x3 cat_matrix;
    if (need_adaptation && ctx) {
        alwan_xyy src_white_xyy, dst_white_xyy;
        alwan_xyz src_white_xyz, dst_white_xyz;

        /* Convert xy to XYZ (using Y=1.0) */
        src_white_xyy.x = src_space->white_xy[0];
        src_white_xyy.y = src_space->white_xy[1];
        src_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&src_white_xyz, &src_white_xyy);

        dst_white_xyy.x = dst_space->white_xy[0];
        dst_white_xyy.y = dst_space->white_xy[1];
        dst_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&dst_white_xyz, &dst_white_xyy);

        /* Compute CAT matrix once */
        status = alwan_cat_matrix(&cat_matrix, &src_white_xyz, &dst_white_xyz,
                                  ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;
    }

    /* Convert all colors */
    for (size_t i = 0; i < count; i++) {
        /* Convert source RGB to XYZ */
        alwan_vec3 xyz, vec_in;
        ALWAN_MEMCPY(&vec_in, &src_rgb[i], sizeof(alwan_vec3));
        alwan_mat3_mulv(&xyz, &src_to_xyz, &vec_in);

        /* Apply chromatic adaptation if needed */
        if (need_adaptation && ctx) {
            alwan_vec3 xyz_adapted;
            alwan_mat3_mulv(&xyz_adapted, &cat_matrix, &xyz);
            xyz = xyz_adapted;
        }

        /* Convert adapted XYZ to destination RGB */
        alwan_vec3 vec_out;
        alwan_mat3_mulv(&vec_out, &xyz_to_dst, &xyz);
        ALWAN_MEMCPY(&dst_rgb[i], &vec_out, sizeof(alwan_vec3));
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB Space Descriptor Helper
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * Embedded RGB Space Data
 * ---------------------------------------------------------------- */
#include "alwan_rgb_embedded.h"

/* ----------------------------------------------------------------
 * Get RGB space descriptor by enum
 * ---------------------------------------------------------------- */

int alwan_rgb_get_space_descriptor(alwan_rgb_space_desc *desc, alwan_ctx *ctx, alwan_rgb_space space) {
    if (!desc) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_EMBED_DATA
    /* Embedded data mode - direct array indexing (enum values map to array indices) */
    (void)ctx;  /* Unused in embedded mode */

    /* Bounds check: ensure enum value is within valid array range */
    if (space < 0 || (size_t)space >= g_rgb_space_data_count) {
        return ALWAN_E_INVALID;
    }

    /* Direct lookup using enum as index - data format: [rx, ry, gx, gy, bx, by, wx, wy] */
    alwan_scalar const *rgb_data = g_rgb_space_data[space];

    /* Copy primaries (first 6 values) and white point (last 2 values) */
    for (int j = 0; j < 6; j++) {
        desc->primaries_xy[j] = rgb_data[j];
    }
    desc->white_xy[0] = rgb_data[6];
    desc->white_xy[1] = rgb_data[7];

    /* Set transfer functions to linear (identity) by default */
    desc->oetf = ALWAN_TF_LINEAR;
    desc->eotf = ALWAN_TF_LINEAR;

    return ALWAN_OK;

#else
    #error "Only ALWAN_EMBED_DATA mode is supported. Runtime CSV loading has been removed."
#endif /* ALWAN_EMBED_DATA */
}
