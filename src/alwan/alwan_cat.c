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

/* ----------------------------------------------------------------
 * Zhai & Luo 2018 Two-Step Chromatic Adaptation
 * Reference: Zhai, Luo (2018) "Study of chromatic adaptation via
 *            neutral white matches on different viewing media"
 *            Optics Express, 26(6), 7724.
 * ---------------------------------------------------------------- */

int alwan_cat_zhai2018(alwan_xyz const *xyz_in,
                       alwan_xyz const *xyz_src,
                       alwan_xyz const *xyz_dst,
                       alwan_scalar D_src,
                       alwan_scalar D_dst,
                       alwan_xyz const *xyz_baseline,
                       alwan_cat_method transform,
                       alwan_xyz *xyz_out) {
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
        memcpy(M.m, g_cat_cat02, sizeof(g_cat_cat02));
    } else {
        memcpy(M.m, g_cat_cat16, sizeof(g_cat_cat16));
    }

    /* Compute M inverse */
    alwan_mat3x3 M_inv;
    int status = alwan_mat3_inv(&M, &M_inv);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Transform XYZ to RGB (cone responses) */
    alwan_vec3 rgb_in, rgb_src, rgb_dst, rgb_o;
    alwan_mat3_mulv(&M, (alwan_vec3 const *)xyz_in, &rgb_in);
    alwan_mat3_mulv(&M, (alwan_vec3 const *)xyz_src, &rgb_src);
    alwan_mat3_mulv(&M, (alwan_vec3 const *)xyz_dst, &rgb_dst);
    alwan_mat3_mulv(&M, (alwan_vec3 const *)xyz_o, &rgb_o);

    /* Compute D_RGB factors for source and destination
     * D_RGB = D * (Y_w / Y_o) * (RGB_o / RGB_w) + 1 - D
     * Simplified when Y_w = Y_o (both = 100): D_RGB = D * (RGB_o / RGB_w) + 1 - D */
    alwan_vec3 D_rgb_src, D_rgb_dst;
    for (int i = 0; i < 3; i++) {
        D_rgb_src.v[i] = D_src * (rgb_o.v[i] / rgb_src.v[i]) + (ALWAN_LITERAL(1.0) - D_src);
        D_rgb_dst.v[i] = D_dst * (rgb_o.v[i] / rgb_dst.v[i]) + (ALWAN_LITERAL(1.0) - D_dst);
    }

    /* Apply two-step adaptation: RGB_d = (D_RGB_src / D_RGB_dst) * RGB_in */
    alwan_vec3 rgb_adapted;
    for (int i = 0; i < 3; i++) {
        rgb_adapted.v[i] = (D_rgb_src.v[i] / D_rgb_dst.v[i]) * rgb_in.v[i];
    }

    /* Transform back to XYZ */
    alwan_mat3_mulv(&M_inv, &rgb_adapted, (alwan_vec3 *)xyz_out);

    return ALWAN_OK;
}
