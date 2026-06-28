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
 *
 * The templated alwan_cat_matrix path selects its matrix directly from the
 * native-precision data twin (g_cat_NAME_f32 / g_cat_NAME_f64) inside
 * alwan_cat_impl.inc. Only the f64-only Zhai 2018 path below needs an
 * accessor, so just CAT02 and CAT16 are kept here (reading the _f64 twin).
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F64
/* CAT02 matrix (from CIECAM02)
 * Data defined in alwan_data.c */
static void get_cat02_matrix(alwan_mat3x3_f64 *out) {
    memcpy(out->m, g_cat_cat02_f64, sizeof(g_cat_cat02_f64));
}

/* CAT16 matrix (from CAM16)
 * Data defined in alwan_data.c */
static void get_cat16_matrix(alwan_mat3x3_f64 *out) {
    memcpy(out->m, g_cat_cat16_f64, sizeof(g_cat_cat16_f64));
}
#endif /* ALWAN_WITH_F64 */

/* ----------------------------------------------------------------
 * CAT Matrix Computation
 * Resolves the CAT enum to a cone-response matrix M, then
 * calls the core value-returning functions.
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_cat_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_cat_impl.inc"
#include "alwan_api_teardown.h"
#endif

/* ----------------------------------------------------------------
 * Bulk Chromatic Adaptation
 * Computes the adaptation matrix once, then applies it to every
 * element in a stride-based buffer.  This loop stays in the .c
 * file because it is a bulk operation that cannot live in a
 * header-only core.
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F64
int alwan_xyz_adapt_f64(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f64 const *src_white_xyz, alwan_xyz_f64 const *dst_white_xyz, alwan_cat_method method) {
    if (!xyz_in || !xyz_out || !src_white_xyz || !dst_white_xyz) {
        return ALWAN_E_INVALID;
    }

    /* Compute adaptation matrix once */
    alwan_mat3x3_f64 cat_mat;
    int status = alwan_cat_matrix_f64(&cat_mat, src_white_xyz, dst_white_xyz, method);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Apply to all colors (strides are in bytes) */
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *in_ptr = (alwan_f64 const *)((char const *)xyz_in + i * in_stride);
        alwan_f64 *out_ptr = (alwan_f64 *)((char *)xyz_out + i * out_stride);

        /* Build an alwan_xyz_f64 from the strided input */
        alwan_xyz_f64 xyz_input;
        xyz_input.x = in_ptr[0];
        xyz_input.y = in_ptr[1];
        xyz_input.z = in_ptr[2];

        /* Delegate to core single-color adaptation */
        alwan_xyz_f64 xyz_adapted = alwan_cat_adapt_f64_v(cat_mat, xyz_input);

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

int alwan_cat_zhai2018_f64(alwan_xyz_f64 *xyz_out,
                       alwan_xyz_f64 const *xyz_in,
                       alwan_xyz_f64 const *xyz_src,
                       alwan_xyz_f64 const *xyz_dst,
                       alwan_f64 D_src,
                       alwan_f64 D_dst,
                       alwan_xyz_f64 const *xyz_baseline,
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
    alwan_xyz_f64 baseline_default = { ALWAN_LITERAL(100.0), ALWAN_LITERAL(100.0), ALWAN_LITERAL(100.0) };
    alwan_xyz_f64 const *xyz_o = xyz_baseline ? xyz_baseline : &baseline_default;

    /* Get the CAT matrix M for the chosen method */
    alwan_mat3x3_f64 M;
    if (transform == ALWAN_CAT_CAT02) {
        get_cat02_matrix(&M);
    } else {
        get_cat16_matrix(&M);
    }

    /* Compute M inverse */
    alwan_mat3x3_f64 M_inv;
    int status = alwan_mat3_inv_f64(&M_inv, &M);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Delegate to core two-step adaptation */
    *xyz_out = alwan_cat_zhai2018_f64_v(M, M_inv, *xyz_in, *xyz_src, *xyz_dst,
                                    D_src, D_dst, *xyz_o);

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */
