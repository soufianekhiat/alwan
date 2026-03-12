/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Chromatic Adaptation Transform (CAT) implementation
 *
 * This .c wrapper resolves the CAT enum to matrices loaded from
 * embedded global data and delegates the math to the _v() core
 * functions defined in alwan_cat_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_cat_core.h"
#include <string.h>

/* ----------------------------------------------------------------
 * CAT Matrix Accessors
 * Load cone-response matrices from embedded global data arrays.
 * ---------------------------------------------------------------- */

/* Bradford CAT matrix (most common, used in ICC profiles)
 * Data defined in alwan_data.c */
static void get_bradford_matrix(alwan_mat3x3 *out) {
    memcpy(out->m, g_cat_bradford, sizeof(g_cat_bradford));
}

/* CAT02 matrix (from CIECAM02)
 * Data defined in alwan_data.c */
static void get_cat02_matrix(alwan_mat3x3 *out) {
    memcpy(out->m, g_cat_cat02, sizeof(g_cat_cat02));
}

/* CAT16 matrix (from CAM16)
 * Data defined in alwan_data.c */
static void get_cat16_matrix(alwan_mat3x3 *out) {
    memcpy(out->m, g_cat_cat16, sizeof(g_cat_cat16));
}

/* ----------------------------------------------------------------
 * Extended CAT Matrix Accessors
 * ---------------------------------------------------------------- */

/* Sharp CAT matrix
 * Data defined in alwan_data.c */
static void get_sharp_matrix(alwan_mat3x3 *out) {
    memcpy(out->m, g_cat_sharp, sizeof(g_cat_sharp));
}

/* Fairchild 1990 CAT matrix
 * Data defined in alwan_data.c */
static void get_fairchild_matrix(alwan_mat3x3 *out) {
    memcpy(out->m, g_cat_fairchild, sizeof(g_cat_fairchild));
}

/* CMCCAT97 matrix
 * Data defined in alwan_data.c */
static void get_cmccat97_matrix(alwan_mat3x3 *out) {
    memcpy(out->m, g_cat_cmccat97, sizeof(g_cat_cmccat97));
}

/* CMCCAT2000 matrix
 * Data defined in alwan_data.c */
static void get_cmccat2000_matrix(alwan_mat3x3 *out) {
    memcpy(out->m, g_cat_cmccat2000, sizeof(g_cat_cmccat2000));
}

/* CAT02 Brill 2008 variant matrix
 * Data defined in alwan_data.c */
static void get_cat02_brill_2008_matrix(alwan_mat3x3 *out) {
    memcpy(out->m, g_cat_cat02_brill_2008, sizeof(g_cat_cat02_brill_2008));
}

/* Bianco 2010 CAT matrix
 * Data defined in alwan_data.c */
static void get_bianco_2010_matrix(alwan_mat3x3 *out) {
    memcpy(out->m, g_cat_bianco_2010, sizeof(g_cat_bianco_2010));
}

/* Bianco PC 2010 CAT matrix
 * Data defined in alwan_data.c */
static void get_bianco_pc_2010_matrix(alwan_mat3x3 *out) {
    memcpy(out->m, g_cat_bianco_pc_2010, sizeof(g_cat_bianco_pc_2010));
}

/* ----------------------------------------------------------------
 * CAT Matrix Computation
 * Resolves the CAT enum to a cone-response matrix M, then
 * calls the core value-returning functions.
 * ---------------------------------------------------------------- */

int alwan_cat_matrix(alwan_mat3x3 *out,
                     alwan_xyz const *src_white_xyz,
                     alwan_xyz const *dst_white_xyz,
                     alwan_cat_method method) {
    if (!src_white_xyz || !dst_white_xyz || !out) {
        return ALWAN_E_INVALID;
    }

    /* Validate white points (Y should be close to 1.0, and all components > 0) */
    if (src_white_xyz->y < ALWAN_EPSILON || dst_white_xyz->y < ALWAN_EPSILON) {
        return ALWAN_E_INVALID;
    }

    /* Handle XYZ scaling separately (simplest case -- no cone-response matrix) */
    if (method == ALWAN_CAT_XYZ_SCALING) {
        *out = alwan_cat_xyz_scaling_v(*src_white_xyz, *dst_white_xyz);
        return ALWAN_OK;
    }

    /* Get the CAT matrix M for the chosen method */
    alwan_mat3x3 M;
    switch (method) {
        case ALWAN_CAT_BRADFORD:
            get_bradford_matrix(&M);
            break;
        case ALWAN_CAT_CAT02:
            get_cat02_matrix(&M);
            break;
        case ALWAN_CAT_CAT16:
            get_cat16_matrix(&M);
            break;

        /* Extended CAT methods */
        case ALWAN_CAT_SHARP:
            get_sharp_matrix(&M);
            break;
        case ALWAN_CAT_FAIRCHILD:
            get_fairchild_matrix(&M);
            break;
        case ALWAN_CAT_CMCCAT97:
            get_cmccat97_matrix(&M);
            break;
        case ALWAN_CAT_CMCCAT2000:
            get_cmccat2000_matrix(&M);
            break;
        case ALWAN_CAT_CAT02_BRILL_2008:
            get_cat02_brill_2008_matrix(&M);
            break;
        case ALWAN_CAT_BIANCO_2010:
            get_bianco_2010_matrix(&M);
            break;
        case ALWAN_CAT_BIANCO_PC_2010:
            get_bianco_pc_2010_matrix(&M);
            break;

        default:
            return ALWAN_E_INVALID;
    }

    /* Compute M^-1 */
    alwan_mat3x3 M_inv;
    int status = alwan_mat3_inv(&M_inv, &M);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Delegate to core: M_adapt = M^-1 * diag(rgb_dst / rgb_src) * M */
    *out = alwan_cat_matrix_v(M, M_inv, *src_white_xyz, *dst_white_xyz);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Bulk Chromatic Adaptation
 * Computes the adaptation matrix once, then applies it to every
 * element in a stride-based buffer.  This loop stays in the .c
 * file because it is a bulk operation that cannot live in a
 * header-only core.
 * ---------------------------------------------------------------- */

int alwan_xyz_adapt(alwan_scalar *xyz_out,
                    alwan_xyz const *src_white_xyz,
                    alwan_xyz const *dst_white_xyz,
                    alwan_cat_method method,
                    alwan_scalar const *xyz_in,
                    size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !xyz_out || !src_white_xyz || !dst_white_xyz) {
        return ALWAN_E_INVALID;
    }

    /* Compute adaptation matrix once */
    alwan_mat3x3 cat_mat;
    int status = alwan_cat_matrix(&cat_mat, src_white_xyz, dst_white_xyz, method);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Apply to all colors (strides are in bytes) */
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);

        /* Build an alwan_xyz from the strided input */
        alwan_xyz xyz_input;
        xyz_input.x = in_ptr[0];
        xyz_input.y = in_ptr[1];
        xyz_input.z = in_ptr[2];

        /* Delegate to core single-color adaptation */
        alwan_xyz xyz_adapted = alwan_cat_adapt_v(cat_mat, xyz_input);

        /* Store output color */
        out_ptr[0] = xyz_adapted.x;
        out_ptr[1] = xyz_adapted.y;
        out_ptr[2] = xyz_adapted.z;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Zhai & Luo 2018 Two-Step Chromatic Adaptation
 * Reference: Zhai, Luo (2018) "Study of chromatic adaptation via
 *            neutral white matches on different viewing media"
 *            Optics Express, 26(6), 7724.
 * ---------------------------------------------------------------- */

int alwan_cat_zhai2018(alwan_xyz *xyz_out,
                       alwan_xyz const *xyz_in,
                       alwan_xyz const *xyz_src,
                       alwan_xyz const *xyz_dst,
                       alwan_scalar D_src,
                       alwan_scalar D_dst,
                       alwan_xyz const *xyz_baseline,
                       alwan_cat_method transform) {
    if (!xyz_in || !xyz_src || !xyz_dst || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    /* Validate degree of adaptation [0, 1] */
    if (D_src < ALWAN_LITERAL(0.0) || D_src > ALWAN_LITERAL(1.0) ||
        D_dst < ALWAN_LITERAL(0.0) || D_dst > ALWAN_LITERAL(1.0)) {
        return ALWAN_E_RANGE;
    }

    /* Only CAT02 and CAT16 are supported for Zhai 2018 */
    if (transform != ALWAN_CAT_CAT02 && transform != ALWAN_CAT_CAT16) {
        return ALWAN_E_INVALID;
    }

    /* Default baseline illuminant is equal-energy white (E) */
    alwan_xyz baseline_default = { ALWAN_LITERAL(100.0), ALWAN_LITERAL(100.0), ALWAN_LITERAL(100.0) };
    alwan_xyz const *xyz_o = xyz_baseline ? xyz_baseline : &baseline_default;

    /* Get the CAT matrix M for the chosen method */
    alwan_mat3x3 M;
    if (transform == ALWAN_CAT_CAT02) {
        get_cat02_matrix(&M);
    } else {
        get_cat16_matrix(&M);
    }

    /* Compute M inverse */
    alwan_mat3x3 M_inv;
    int status = alwan_mat3_inv(&M_inv, &M);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Delegate to core two-step adaptation */
    *xyz_out = alwan_cat_zhai2018_v(M, M_inv, *xyz_in, *xyz_src, *xyz_dst,
                                    D_src, D_dst, *xyz_o);

    return ALWAN_OK;
}
