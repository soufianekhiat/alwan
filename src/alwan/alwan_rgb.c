/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Helper: Convert xyY to XYZ (Y=1)
 * ---------------------------------------------------------------- */
static void xy_to_XYZ(Scalar x, Scalar y, alwan_vec3 *XYZ) {
    /* Avoid division by zero */
    if (y < ALWAN_EPSILON) {
        XYZ->v[0] = XYZ->v[1] = XYZ->v[2] = ALWAN_LITERAL(0.0);
        return;
    }

    Scalar Y = ALWAN_LITERAL(1.0);
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

    /* RGB→XYZ = M * diag(S)
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

    /* XYZ→RGB = inverse of RGB→XYZ */
    return alwan_mat3_inv(rgb_to_xyz, xyz_to_rgb);
}

/* ----------------------------------------------------------------
 * sRGB Transfer Functions
 * ---------------------------------------------------------------- */

/* sRGB OETF: linear → encoded
 * Formula: V' = 12.92 * V              if V <= 0.0031308
 *               1.055 * V^(1/2.4) - 0.055   otherwise */
static Scalar srgb_oetf_scalar(Scalar linear) {
    if (linear <= ALWAN_LITERAL(0.0031308)) {
        return ALWAN_LITERAL(12.92) * linear;
    } else {
        return ALWAN_LITERAL(1.055) * ALWAN_POW(linear, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4))
               - ALWAN_LITERAL(0.055);
    }
}

/* sRGB EOTF: encoded → linear
 * Formula: V = V' / 12.92              if V' <= 0.04045
 *               ((V' + 0.055) / 1.055)^2.4  otherwise */
static Scalar srgb_eotf_scalar(Scalar encoded) {
    if (encoded <= ALWAN_LITERAL(0.04045)) {
        return encoded / ALWAN_LITERAL(12.92);
    } else {
        return ALWAN_POW((encoded + ALWAN_LITERAL(0.055)) / ALWAN_LITERAL(1.055),
                         ALWAN_LITERAL(2.4));
    }
}

/* ----------------------------------------------------------------
 * PQ (ST.2084) Transfer Functions - HDR10/HDR10+
 * ---------------------------------------------------------------- */

/* PQ OETF: linear (0-10000 cd/m²) → PQ code values
 * Normalizes input to [0,1] assuming 10000 cd/m² peak, then applies PQ curve */
static Scalar pq_oetf_scalar(Scalar linear) {
    /* PQ constants */
    Scalar const m1 = ALWAN_LITERAL(0.1593017578125);      /* 2610/16384 */
    Scalar const m2 = ALWAN_LITERAL(78.84375);              /* 2523/32 */
    Scalar const c1 = ALWAN_LITERAL(0.8359375);             /* 3424/4096 */
    Scalar const c2 = ALWAN_LITERAL(18.8515625);            /* 2413/128 */
    Scalar const c3 = ALWAN_LITERAL(18.6875);               /* 2392/128 */

    /* Normalize to [0,1] assuming 10000 cd/m² peak */
    Scalar Y = linear / ALWAN_LITERAL(10000.0);
    if (Y < ALWAN_LITERAL(0.0)) Y = ALWAN_LITERAL(0.0);

    Scalar Y_pow_m1 = ALWAN_POW(Y, m1);
    Scalar numerator = c1 + c2 * Y_pow_m1;
    Scalar denominator = ALWAN_LITERAL(1.0) + c3 * Y_pow_m1;

    return ALWAN_POW(numerator / denominator, m2);
}

/* PQ EOTF: PQ code values → linear (0-10000 cd/m²) */
static Scalar pq_eotf_scalar(Scalar encoded) {
    /* PQ constants */
    Scalar const m1 = ALWAN_LITERAL(0.1593017578125);
    Scalar const m2 = ALWAN_LITERAL(78.84375);
    Scalar const c1 = ALWAN_LITERAL(0.8359375);
    Scalar const c2 = ALWAN_LITERAL(18.8515625);
    Scalar const c3 = ALWAN_LITERAL(18.6875);
    Scalar const m1_inv = ALWAN_LITERAL(1.0) / m1;
    Scalar const m2_inv = ALWAN_LITERAL(1.0) / m2;

    if (encoded < ALWAN_LITERAL(0.0)) encoded = ALWAN_LITERAL(0.0);

    Scalar E_pow_m2_inv = ALWAN_POW(encoded, m2_inv);
    Scalar numerator = E_pow_m2_inv - c1;
    if (numerator < ALWAN_LITERAL(0.0)) numerator = ALWAN_LITERAL(0.0);

    Scalar denominator = c2 - c3 * E_pow_m2_inv;
    Scalar Y = ALWAN_POW(numerator / denominator, m1_inv);

    /* Scale back to cd/m² (0-10000 range) */
    return Y * ALWAN_LITERAL(10000.0);
}

/* ----------------------------------------------------------------
 * HLG (Hybrid Log-Gamma) Transfer Functions - BT.2100
 * ---------------------------------------------------------------- */

/* HLG OETF: scene-referred linear → HLG signal */
static Scalar hlg_oetf_scalar(Scalar linear) {
    Scalar const a = ALWAN_LITERAL(0.17883277);
    Scalar const b = ALWAN_LITERAL(0.28466892);  /* 1 - 4*a */
    Scalar const c = ALWAN_LITERAL(0.55991073);  /* 0.5 - a*ln(4*a) */

    if (linear < ALWAN_LITERAL(0.0)) linear = ALWAN_LITERAL(0.0);

    if (linear <= ALWAN_LITERAL(1.0) / ALWAN_LITERAL(12.0)) {
        return ALWAN_SQRT(ALWAN_LITERAL(3.0) * linear);
    } else {
        return a * ALWAN_LOG(ALWAN_LITERAL(12.0) * linear - b) + c;
    }
}

/* HLG EOTF: HLG signal → display-referred linear
 * Note: Simplified EOTF without system gamma, assumes gamma=1.2 */
static Scalar hlg_eotf_scalar(Scalar encoded) {
    Scalar const a = ALWAN_LITERAL(0.17883277);
    Scalar const b = ALWAN_LITERAL(0.28466892);
    Scalar const c = ALWAN_LITERAL(0.55991073);

    if (encoded < ALWAN_LITERAL(0.0)) encoded = ALWAN_LITERAL(0.0);

    Scalar linear;
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
static Scalar bt1886_eotf_scalar(Scalar encoded) {
    if (encoded < ALWAN_LITERAL(0.0)) encoded = ALWAN_LITERAL(0.0);
    return ALWAN_POW(encoded, ALWAN_LITERAL(2.4));
}

/* ----------------------------------------------------------------
 * ACESproxy Transfer Functions
 * ---------------------------------------------------------------- */

/* ACESproxy encode: ACES linear → ACESproxy (10-bit or 12-bit log encoding) */
static Scalar acesproxy_oetf_scalar(Scalar linear) {
    /* ACESproxy constants for 10-bit encoding */
    Scalar const mid_gray_in = ALWAN_LITERAL(0.18);
    Scalar const mid_code_value = ALWAN_LITERAL(425.0) / ALWAN_LITERAL(1023.0);
    Scalar const steps_per_stop = ALWAN_LITERAL(50.0) / ALWAN_LITERAL(1023.0);

    if (linear <= ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(64.0) / ALWAN_LITERAL(1023.0);  /* Minimum code value */
    }

    Scalar log_val = ALWAN_LOG(linear / mid_gray_in) / ALWAN_LOG(ALWAN_LITERAL(2.0));
    return mid_code_value + steps_per_stop * log_val;
}

/* ACESproxy decode: ACESproxy → ACES linear */
static Scalar acesproxy_eotf_scalar(Scalar encoded) {
    Scalar const mid_gray_in = ALWAN_LITERAL(0.18);
    Scalar const mid_code_value = ALWAN_LITERAL(425.0) / ALWAN_LITERAL(1023.0);
    Scalar const steps_per_stop = ALWAN_LITERAL(50.0) / ALWAN_LITERAL(1023.0);

    Scalar log_val = (encoded - mid_code_value) / steps_per_stop;
    return mid_gray_in * ALWAN_POW(ALWAN_LITERAL(2.0), log_val);
}

/* ----------------------------------------------------------------
 * Transfer Function API
 * ---------------------------------------------------------------- */

int alwan_oetf_apply(char const *name,
                     Scalar const *linear, size_t count, size_t in_stride,
                     Scalar *encoded, size_t out_stride) {
    if (!name || !linear || !encoded) {
        return ALWAN_E_INVALID;
    }

    /* Select transfer function */
    Scalar (*oetf_fn)(Scalar) = NULL;

    if (strcmp(name, "srgb") == 0) {
        oetf_fn = srgb_oetf_scalar;
    } else if (strcmp(name, "pq") == 0 || strcmp(name, "st2084") == 0) {
        oetf_fn = pq_oetf_scalar;
    } else if (strcmp(name, "hlg") == 0) {
        oetf_fn = hlg_oetf_scalar;
    } else if (strcmp(name, "acesproxy") == 0) {
        oetf_fn = acesproxy_oetf_scalar;
    } else {
        return ALWAN_E_INVALID;  /* Unknown transfer function */
    }

    /* Apply transfer function to array */
    for (size_t i = 0; i < count; i++) {
        Scalar const *in_ptr = (Scalar const *)((char const *)linear + i * in_stride);
        Scalar *out_ptr = (Scalar *)((char *)encoded + i * out_stride);
        *out_ptr = oetf_fn(*in_ptr);
    }

    return ALWAN_OK;
}

int alwan_eotf_apply(char const *name,
                     Scalar const *encoded, size_t count, size_t in_stride,
                     Scalar *linear, size_t out_stride) {
    if (!name || !encoded || !linear) {
        return ALWAN_E_INVALID;
    }

    /* Select transfer function */
    Scalar (*eotf_fn)(Scalar) = NULL;

    if (strcmp(name, "srgb") == 0) {
        eotf_fn = srgb_eotf_scalar;
    } else if (strcmp(name, "pq") == 0 || strcmp(name, "st2084") == 0) {
        eotf_fn = pq_eotf_scalar;
    } else if (strcmp(name, "hlg") == 0) {
        eotf_fn = hlg_eotf_scalar;
    } else if (strcmp(name, "bt1886") == 0) {
        eotf_fn = bt1886_eotf_scalar;
    } else if (strcmp(name, "acesproxy") == 0) {
        eotf_fn = acesproxy_eotf_scalar;
    } else {
        return ALWAN_E_INVALID;  /* Unknown transfer function */
    }

    /* Apply transfer function to array */
    for (size_t i = 0; i < count; i++) {
        Scalar const *in_ptr = (Scalar const *)((char const *)encoded + i * in_stride);
        Scalar *out_ptr = (Scalar *)((char *)linear + i * out_stride);
        *out_ptr = eotf_fn(*in_ptr);
    }

    return ALWAN_OK;
}
