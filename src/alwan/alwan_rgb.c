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

/* Internal context structure (needed for runtime_data_root access) */
struct alwan_ctx {
    alwan_alloc_fn alloc_fn;
    alwan_free_fn  free_fn;
    char *runtime_data_root;
    uint32_t flags;
};

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

int alwan_rgb_derive_matrices(alwan_rgb_space_desc const *desc,
                               alwan_mat3x3 *rgb_to_xyz,
                               alwan_mat3x3 *xyz_to_rgb) {
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
    int status = alwan_mat3_inv(&M, &M_inv);
    if (status != ALWAN_OK) {
        return status;  /* Singular matrix */
    }

    /* Solve for scale factors: S = M^-1 * W */
    alwan_vec3 S;
    alwan_mat3_mulv(&M_inv, &W_XYZ, &S);

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
    return alwan_mat3_inv(rgb_to_xyz, xyz_to_rgb);
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
	alwan_scalar const c = ALWAN_LITERAL( 0.5 ) - a * ALWAN_LOG( ALWAN_LITERAL( 4.0 ) * a );  /* 0.5 - a*ln(4*a) */

    if (linear < ALWAN_LITERAL(0.0)) linear = ALWAN_LITERAL(0.0);

    if (linear <= ALWAN_LITERAL(1.0) / ALWAN_LITERAL(12.0)) {
        return ALWAN_SQRT(ALWAN_LITERAL(3.0) * linear);
    } else {
        return a * ALWAN_LOG(ALWAN_LITERAL(12.0) * linear - b) + c;
    }
}

/* HLG EOTF: HLG signal -> display-referred linear
 * Note: Simplified EOTF without system gamma, assumes gamma=1.2 */
static alwan_scalar hlg_eotf_scalar(alwan_scalar encoded) {
    /* HLG constants (computed at compile time) */
    alwan_scalar const a = ALWAN_LITERAL(0.17883277);
    alwan_scalar const b = ALWAN_LITERAL(1.0) - ALWAN_LITERAL(4.0)*a;  /* 1 - 4*a */
    alwan_scalar const c = ALWAN_LITERAL(0.5) - a*ALWAN_LOG(ALWAN_LITERAL(4.0)*a);  /* 0.5 - a*ln(4*a) */

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

    alwan_scalar log_val = ALWAN_LOG(linear / mid_gray_in) / ALWAN_LOG(ALWAN_LITERAL(2.0));
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
 * Formula from RED documentation */
static alwan_scalar log3g10_oetf_scalar(alwan_scalar linear) {
    if (linear < ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    } else if (linear <= ALWAN_LITERAL(0.0)) {
        return linear * ALWAN_LITERAL(15.1927);
    } else {
        return ALWAN_LITERAL(0.224282) * ALWAN_LOG10(linear * ALWAN_LITERAL(155.975327) + ALWAN_LITERAL(1.0)) + ALWAN_LITERAL(0.01);
    }
}

static alwan_scalar log3g10_eotf_scalar(alwan_scalar encoded) {
    if (encoded <= ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    } else if (encoded <= ALWAN_LITERAL(0.0)) {
        return encoded / ALWAN_LITERAL(15.1927);
    } else {
        return (ALWAN_POW(ALWAN_LITERAL(10.0), (encoded - ALWAN_LITERAL(0.01)) / ALWAN_LITERAL(0.224282)) - ALWAN_LITERAL(1.0)) / ALWAN_LITERAL(155.975327);
    }
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
        return ALWAN_LITERAL(0.5) * ALWAN_LOG(linear + ALWAN_LITERAL(0.006)) / ALWAN_LOG(ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(0.584);
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
        return c * ALWAN_LOG((linear + b) / a) / ALWAN_LOG(ALWAN_LITERAL(10.0)) + d;
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

    return ALWAN_LOG(linear + ALWAN_LITERAL(1.0)) / ALWAN_LOG(ALWAN_LITERAL(10.0)) * ALWAN_LITERAL(0.4) + ALWAN_LITERAL(0.6);
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

    return ALWAN_LOG(linear * ALWAN_LITERAL(112.0) + ALWAN_LITERAL(1.0)) / ALWAN_LOG(ALWAN_LITERAL(113.0));
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
        return (ALWAN_LITERAL(0.36) * ALWAN_LOG(linear + ALWAN_LITERAL(0.10033)) / ALWAN_LOG(ALWAN_LITERAL(10.0))) + ALWAN_LITERAL(0.71722);
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

/* ----------------------------------------------------------------
 * Transfer Function API
 * ---------------------------------------------------------------- */

int alwan_oetf_apply(alwan_transfer_function tf,
                     alwan_scalar const *linear, size_t count, size_t in_stride,
                     alwan_scalar *encoded, size_t out_stride) {
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
            /* BT.1886 only has EOTF, no OETF */
            return ALWAN_E_INVALID;

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

int alwan_eotf_apply(alwan_transfer_function tf,
                     alwan_scalar const *encoded, size_t count, size_t in_stride,
                     alwan_scalar *linear, size_t out_stride) {
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
int alwan_rgb_convert(alwan_ctx *ctx,
                      alwan_rgb_space_desc const *src_space,
                      alwan_rgb_space_desc const *dst_space,
                      alwan_rgb const *src_rgb,
                      alwan_rgb *dst_rgb) {
    if (!src_space || !dst_space || !src_rgb || !dst_rgb) {
        return ALWAN_E_INVALID;
    }

    /* Derive conversion matrices for both spaces */
    alwan_mat3x3 src_to_xyz, xyz_to_src;
    alwan_mat3x3 dst_to_xyz, xyz_to_dst;

    int status = alwan_rgb_derive_matrices(src_space, &src_to_xyz, &xyz_to_src);
    if (status != ALWAN_OK) return status;

    status = alwan_rgb_derive_matrices(dst_space, &dst_to_xyz, &xyz_to_dst);
    if (status != ALWAN_OK) return status;

    /* Convert source RGB to XYZ */
    alwan_vec3 xyz;
    alwan_mat3_mulv(&src_to_xyz, (alwan_vec3 const *)src_rgb, &xyz);

    /* Check if chromatic adaptation is needed */
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
    alwan_scalar dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_scalar dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_adaptation = (ALWAN_FABS(dx) > tolerance || ALWAN_FABS(dy) > tolerance);

    if (need_adaptation && ctx) {
        /* Perform chromatic adaptation using Bradford CAT */
        alwan_xyy src_white_xyy, dst_white_xyy;
        alwan_xyz src_white_xyz, dst_white_xyz;

        /* Convert xy to XYZ (using Y=1.0) */
        src_white_xyy.x = src_space->white_xy[0];
        src_white_xyy.y = src_space->white_xy[1];
        src_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&src_white_xyy, &src_white_xyz);

        dst_white_xyy.x = dst_space->white_xy[0];
        dst_white_xyy.y = dst_space->white_xy[1];
        dst_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&dst_white_xyy, &dst_white_xyz);

        /* Compute and apply CAT matrix */
        alwan_mat3x3 cat_matrix;
        status = alwan_cat_matrix(&src_white_xyz, &dst_white_xyz,
                                  ALWAN_CAT_BRADFORD, &cat_matrix);
        if (status != ALWAN_OK) return status;

        alwan_vec3 xyz_adapted;
        alwan_mat3_mulv(&cat_matrix, &xyz, &xyz_adapted);
        xyz = xyz_adapted;
    }

    /* Convert adapted XYZ to destination RGB */
    alwan_mat3_mulv(&xyz_to_dst, &xyz, (alwan_vec3 *)dst_rgb);

    return ALWAN_OK;
}

/* Bulk RGB color space conversion */
int alwan_rgb_convert_bulk(alwan_ctx *ctx,
                            alwan_rgb_space_desc const *src_space,
                            alwan_rgb_space_desc const *dst_space,
                            alwan_rgb const *src_rgb,
                            alwan_rgb *dst_rgb,
                            size_t count) {
    if (!src_space || !dst_space || !src_rgb || !dst_rgb || count == 0) {
        return ALWAN_E_INVALID;
    }

    /* Derive conversion matrices once for all colors */
    alwan_mat3x3 src_to_xyz, xyz_to_src;
    alwan_mat3x3 dst_to_xyz, xyz_to_dst;

    int status = alwan_rgb_derive_matrices(src_space, &src_to_xyz, &xyz_to_src);
    if (status != ALWAN_OK) return status;

    status = alwan_rgb_derive_matrices(dst_space, &dst_to_xyz, &xyz_to_dst);
    if (status != ALWAN_OK) return status;

    /* Check if chromatic adaptation is needed */
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
    alwan_scalar dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_scalar dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_adaptation = (ALWAN_FABS(dx) > tolerance || ALWAN_FABS(dy) > tolerance);

    /* Precompute adaptation matrix if needed */
    alwan_mat3x3 cat_matrix;
    if (need_adaptation && ctx) {
        alwan_xyy src_white_xyy, dst_white_xyy;
        alwan_xyz src_white_xyz, dst_white_xyz;

        /* Convert xy to XYZ (using Y=1.0) */
        src_white_xyy.x = src_space->white_xy[0];
        src_white_xyy.y = src_space->white_xy[1];
        src_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&src_white_xyy, &src_white_xyz);

        dst_white_xyy.x = dst_space->white_xy[0];
        dst_white_xyy.y = dst_space->white_xy[1];
        dst_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&dst_white_xyy, &dst_white_xyz);

        /* Compute CAT matrix once */
        status = alwan_cat_matrix((alwan_xyz const *)&src_white_xyz, (alwan_xyz const *)&dst_white_xyz,
                                  ALWAN_CAT_BRADFORD, &cat_matrix);
        if (status != ALWAN_OK) return status;
    }

    /* Convert all colors */
    for (size_t i = 0; i < count; i++) {
        /* Convert source RGB to XYZ */
        alwan_vec3 xyz;
        alwan_mat3_mulv(&src_to_xyz, (alwan_vec3 const *)&src_rgb[i], &xyz);

        /* Apply chromatic adaptation if needed */
        if (need_adaptation && ctx) {
            alwan_vec3 xyz_adapted;
            alwan_mat3_mulv(&cat_matrix, &xyz, &xyz_adapted);
            xyz = xyz_adapted;
        }

        /* Convert adapted XYZ to destination RGB */
        alwan_mat3_mulv(&xyz_to_dst, &xyz, (alwan_vec3 *)&dst_rgb[i]);
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

int alwan_rgb_get_space_descriptor(alwan_ctx *ctx, alwan_rgb_space space, alwan_rgb_space_desc *desc) {
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
