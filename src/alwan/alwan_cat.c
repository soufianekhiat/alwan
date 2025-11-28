/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Chromatic Adaptation Transform (CAT) implementation
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * CAT Matrix Accessors
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
 * CAT Implementation
 * ---------------------------------------------------------------- */

int alwan_cat_matrix(alwan_xyz const *src_white_xyz,
                     alwan_xyz const *dst_white_xyz,
                     alwan_cat_method method,
                     alwan_mat3x3 *out) {
    if (!src_white_xyz || !dst_white_xyz || !out) {
        return ALWAN_E_INVALID;
    }

    /* Validate white points (Y should be close to 1.0, and all components > 0) */
    if (src_white_xyz->y < ALWAN_EPSILON || dst_white_xyz->y < ALWAN_EPSILON) {
        return ALWAN_E_INVALID;
    }

    /* Handle XYZ scaling separately (simplest case) */
    if (method == ALWAN_CAT_XYZ_SCALING) {
        /* XYZ scaling: diagonal matrix of ratios */
        alwan_scalar const sx = dst_white_xyz->x / src_white_xyz->x;
        alwan_scalar const sy = dst_white_xyz->y / src_white_xyz->y;
        alwan_scalar const sz = dst_white_xyz->z / src_white_xyz->z;

        out->m[0] = sx;                 out->m[1] = ALWAN_LITERAL(0.0); out->m[2] = ALWAN_LITERAL(0.0);
        out->m[3] = ALWAN_LITERAL(0.0); out->m[4] = sy;                 out->m[5] = ALWAN_LITERAL(0.0);
        out->m[6] = ALWAN_LITERAL(0.0); out->m[7] = ALWAN_LITERAL(0.0); out->m[8] = sz;

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

    /* Transform white points to cone response space */
    alwan_vec3 rgb_src, rgb_dst;
    alwan_mat3_mulv(&M, (alwan_vec3 const *)src_white_xyz, &rgb_src);
    alwan_mat3_mulv(&M, (alwan_vec3 const *)dst_white_xyz, &rgb_dst);

    /* Compute diagonal scaling matrix D = diag(rgb_dst ./ rgb_src) */
    alwan_mat3x3 D;
    memset(&D, 0, sizeof(D));
    D.m[0] = rgb_dst.v[0] / rgb_src.v[0];  /* D[0,0] */
    D.m[4] = rgb_dst.v[1] / rgb_src.v[1];  /* D[1,1] */
    D.m[8] = rgb_dst.v[2] / rgb_src.v[2];  /* D[2,2] */

    /* Compute M^-1 */
    alwan_mat3x3 M_inv;
    int status = alwan_mat3_inv(&M, &M_inv);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Compute adaptation matrix: M_adapt = M^-1 * D * M */
    alwan_mat3x3 temp;
    alwan_mat3_mul(&D, &M, &temp);        /* temp = D * M */
    alwan_mat3_mul(&M_inv, &temp, out);   /* out = M^-1 * temp */

    return ALWAN_OK;
}

int alwan_xyz_adapt(alwan_scalar const *xyz_in, size_t count, size_t in_stride,
                    alwan_xyz const *src_white_xyz,
                    alwan_xyz const *dst_white_xyz,
                    alwan_cat_method method,
                    alwan_scalar *xyz_out, size_t out_stride) {
    if (!xyz_in || !xyz_out || !src_white_xyz || !dst_white_xyz) {
        return ALWAN_E_INVALID;
    }

    /* Compute adaptation matrix once */
    alwan_mat3x3 cat_mat;
    int status = alwan_cat_matrix(src_white_xyz, dst_white_xyz, method, &cat_mat);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Apply to all colors */
    for (size_t i = 0; i < count; i++) {
        alwan_vec3 xyz_input, xyz_adapted;

        /* Load input color */
        xyz_input.v[0] = xyz_in[i * in_stride + 0];
        xyz_input.v[1] = xyz_in[i * in_stride + 1];
        xyz_input.v[2] = xyz_in[i * in_stride + 2];

        /* Apply adaptation matrix */
        alwan_mat3_mulv(&cat_mat, &xyz_input, &xyz_adapted);

        /* Store output color */
        xyz_out[i * out_stride + 0] = xyz_adapted.v[0];
        xyz_out[i * out_stride + 1] = xyz_adapted.v[1];
        xyz_out[i * out_stride + 2] = xyz_adapted.v[2];
    }

    return ALWAN_OK;
}
