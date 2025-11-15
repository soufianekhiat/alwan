/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

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
    // TODO: create alwan_mat3_mul_diag() function
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
 * Transfer Function API
 * ---------------------------------------------------------------- */

int alwan_oetf_apply(char const *name,
                     alwan_scalar const *linear, size_t count, size_t in_stride,
                     alwan_scalar *encoded, size_t out_stride) {
    if (!name || !linear || !encoded) {
        return ALWAN_E_INVALID;
    }

    /* Select transfer function */
    alwan_scalar (*oetf_fn)(alwan_scalar) = NULL;

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
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)linear + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)encoded + i * out_stride);
        *out_ptr = oetf_fn(*in_ptr);
    }

    return ALWAN_OK;
}

int alwan_eotf_apply(char const *name,
                     alwan_scalar const *encoded, size_t count, size_t in_stride,
                     alwan_scalar *linear, size_t out_stride) {
    if (!name || !encoded || !linear) {
        return ALWAN_E_INVALID;
    }

    /* Select transfer function */
    alwan_scalar (*eotf_fn)(alwan_scalar) = NULL;

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
                      alwan_vec3 const *src_rgb,
                      alwan_vec3 *dst_rgb) {
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
    alwan_mat3_mulv(&src_to_xyz, src_rgb, &xyz);

    /* Check if chromatic adaptation is needed */
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
    alwan_scalar dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_scalar dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_adaptation = (ALWAN_FABS(dx) > tolerance || ALWAN_FABS(dy) > tolerance);

    if (need_adaptation && ctx) {
        /* Perform chromatic adaptation using Bradford CAT */
        alwan_vec3 src_white_xyy, src_white_xyz;
        alwan_vec3 dst_white_xyy, dst_white_xyz;

        /* Convert xy to XYZ (using Y=1.0) */
        src_white_xyy.v[0] = src_space->white_xy[0];
        src_white_xyy.v[1] = src_space->white_xy[1];
        src_white_xyy.v[2] = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&src_white_xyy, &src_white_xyz);

        dst_white_xyy.v[0] = dst_space->white_xy[0];
        dst_white_xyy.v[1] = dst_space->white_xy[1];
        dst_white_xyy.v[2] = ALWAN_LITERAL(1.0);
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
    alwan_mat3_mulv(&xyz_to_dst, &xyz, dst_rgb);

    return ALWAN_OK;
}

/* Bulk RGB color space conversion */
int alwan_rgb_convert_bulk(alwan_ctx *ctx,
                            alwan_rgb_space_desc const *src_space,
                            alwan_rgb_space_desc const *dst_space,
                            alwan_vec3 const *src_rgb,
                            alwan_vec3 *dst_rgb,
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
        alwan_vec3 src_white_xyy, src_white_xyz;
        alwan_vec3 dst_white_xyy, dst_white_xyz;

        /* Convert xy to XYZ (using Y=1.0) */
        src_white_xyy.v[0] = src_space->white_xy[0];
        src_white_xyy.v[1] = src_space->white_xy[1];
        src_white_xyy.v[2] = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&src_white_xyy, &src_white_xyz);

        dst_white_xyy.v[0] = dst_space->white_xy[0];
        dst_white_xyy.v[1] = dst_space->white_xy[1];
        dst_white_xyy.v[2] = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&dst_white_xyy, &dst_white_xyz);

        /* Compute CAT matrix once */
        status = alwan_cat_matrix(&src_white_xyz, &dst_white_xyz,
                                  ALWAN_CAT_BRADFORD, &cat_matrix);
        if (status != ALWAN_OK) return status;
    }

    /* Convert all colors */
    for (size_t i = 0; i < count; i++) {
        /* Convert source RGB to XYZ */
        alwan_vec3 xyz;
        alwan_mat3_mulv(&src_to_xyz, &src_rgb[i], &xyz);

        /* Apply chromatic adaptation if needed */
        if (need_adaptation && ctx) {
            alwan_vec3 xyz_adapted;
            alwan_mat3_mulv(&cat_matrix, &xyz, &xyz_adapted);
            xyz = xyz_adapted;
        }

        /* Convert adapted XYZ to destination RGB */
        alwan_mat3_mulv(&xyz_to_dst, &xyz, &dst_rgb[i]);
    }

    return ALWAN_OK;
}
