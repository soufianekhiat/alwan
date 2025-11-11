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
