/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * RGB Color Spaces, Transfer Functions, and Matrix Derivation
 *
 * Transfer functions in alwan_rgb_core.h and alwan_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_rgb_core.h"
#include "../core/alwan_math_core.h"
#include "../map/alwan_map_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* struct alwan_ctx is defined in alwan_internal.h */

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_rgb_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_rgb_impl.inc"
#include "alwan_api_teardown.h"
#endif

/* ----------------------------------------------------------------
 * RGB Matrix Derivation
 * ---------------------------------------------------------------- */

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE
 * (gamut volume/coverage f32 facades derive matrices in f64). */
#if ALWAN_WITH_F64_FACADE
alwan_status alwan_rgb_derive_matrices_f64(alwan_mat3x3_f64 *rgb_to_xyz,
                               alwan_mat3x3_f64 *xyz_to_rgb,
                               alwan_rgb_space_desc_f64 const *desc) {
    if (!desc || !rgb_to_xyz || !xyz_to_rgb) {
        return ALWAN_E_INVALID;
    }

    alwan_rgb_matrices_f64 m = alwan_rgb_derive_matrices_f64_v(
        desc->primaries_xy[0], desc->primaries_xy[1],
        desc->primaries_xy[2], desc->primaries_xy[3],
        desc->primaries_xy[4], desc->primaries_xy[5],
        desc->white_xy[0], desc->white_xy[1]);

    *rgb_to_xyz = m.rgb_to_xyz;
    *xyz_to_rgb = m.xyz_to_rgb;

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F32
alwan_status alwan_rgb_derive_matrices_f32(alwan_mat3x3_f32 *rgb_to_xyz,
                               alwan_mat3x3_f32 *xyz_to_rgb,
                               alwan_rgb_space_desc_f32 const *desc) {
    if (!desc || !rgb_to_xyz || !xyz_to_rgb) return ALWAN_E_INVALID;

    alwan_rgb_matrices_f32 m = alwan_rgb_derive_matrices_f32_v(
        desc->primaries_xy[0], desc->primaries_xy[1],
        desc->primaries_xy[2], desc->primaries_xy[3],
        desc->primaries_xy[4], desc->primaries_xy[5],
        desc->white_xy[0], desc->white_xy[1]);

    *rgb_to_xyz = m.rgb_to_xyz;
    *xyz_to_rgb = m.xyz_to_rgb;
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */

/* ----------------------------------------------------------------
 * RGB <-> XYZ Direct Conversion
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F64
alwan_status alwan_rgb_to_xyz_f64(alwan_xyz_f64 *xyz,
                     alwan_rgb_space_desc_f64 const *space,
                     alwan_rgb_f64 const *rgb) {
    if (!space || !rgb || !xyz) {
        return ALWAN_E_INVALID;
    }

    alwan_mat3x3_f64 const *mat;
    alwan_mat3x3_f64 derived_rgb_to_xyz, derived_xyz_to_rgb;

    if (space->has_matrices) {
        mat = &space->rgb_to_xyz;
    } else {
        int status = alwan_rgb_derive_matrices_f64(&derived_rgb_to_xyz, &derived_xyz_to_rgb, space);
        if (status != ALWAN_OK) return status;
        mat = &derived_rgb_to_xyz;
    }

    /* Apply RGB -> XYZ matrix */
    alwan_vec3_f64 vec_in, vec_out;
    ALWAN_MEMCPY(&vec_in, rgb, sizeof(alwan_vec3_f64));
    alwan_mat3_mulv_f64(&vec_out, mat, &vec_in);
    ALWAN_MEMCPY(xyz, &vec_out, sizeof(alwan_vec3_f64));

    return ALWAN_OK;
}

alwan_status alwan_xyz_to_rgb_f64(alwan_rgb_f64 *rgb,
                     alwan_rgb_space_desc_f64 const *space,
                     alwan_xyz_f64 const *xyz) {
    if (!space || !xyz || !rgb) {
        return ALWAN_E_INVALID;
    }

    alwan_mat3x3_f64 const *mat;
    alwan_mat3x3_f64 derived_rgb_to_xyz, derived_xyz_to_rgb;

    if (space->has_matrices) {
        mat = &space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f64(&derived_rgb_to_xyz, &derived_xyz_to_rgb, space);
        if (status != ALWAN_OK) return status;
        mat = &derived_xyz_to_rgb;
    }

    /* Apply XYZ -> RGB matrix */
    alwan_vec3_f64 vec_in, vec_out;
    ALWAN_MEMCPY(&vec_in, xyz, sizeof(alwan_vec3_f64));
    alwan_mat3_mulv_f64(&vec_out, mat, &vec_in);
    ALWAN_MEMCPY(rgb, &vec_out, sizeof(alwan_vec3_f64));

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */

/* Native f32: mirrors alwan_rgb_to_xyz_f64 / alwan_xyz_to_rgb_f64 exactly using
 * f32 building blocks (no widen-to-f64 facade). Uses precomputed matrices when
 * present, else derives them in f32 via alwan_rgb_derive_matrices_f32. */
#if ALWAN_WITH_F32
alwan_status alwan_rgb_to_xyz_f32(alwan_xyz_f32 *xyz,
                     alwan_rgb_space_desc_f32 const *space,
                     alwan_rgb_f32 const *rgb) {
    if (!space || !rgb || !xyz) {
        return ALWAN_E_INVALID;
    }

    alwan_mat3x3_f32 const *mat;
    alwan_mat3x3_f32 derived_rgb_to_xyz, derived_xyz_to_rgb;

    if (space->has_matrices) {
        mat = &space->rgb_to_xyz;
    } else {
        int status = alwan_rgb_derive_matrices_f32(&derived_rgb_to_xyz, &derived_xyz_to_rgb, space);
        if (status != ALWAN_OK) return status;
        mat = &derived_rgb_to_xyz;
    }

    /* Apply RGB -> XYZ matrix */
    alwan_vec3_f32 vec_in, vec_out;
    ALWAN_MEMCPY(&vec_in, rgb, sizeof(alwan_vec3_f32));
    alwan_mat3_mulv_f32(&vec_out, mat, &vec_in);
    ALWAN_MEMCPY(xyz, &vec_out, sizeof(alwan_vec3_f32));

    return ALWAN_OK;
}

alwan_status alwan_xyz_to_rgb_f32(alwan_rgb_f32 *rgb,
                     alwan_rgb_space_desc_f32 const *space,
                     alwan_xyz_f32 const *xyz) {
    if (!space || !xyz || !rgb) {
        return ALWAN_E_INVALID;
    }

    alwan_mat3x3_f32 const *mat;
    alwan_mat3x3_f32 derived_rgb_to_xyz, derived_xyz_to_rgb;

    if (space->has_matrices) {
        mat = &space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f32(&derived_rgb_to_xyz, &derived_xyz_to_rgb, space);
        if (status != ALWAN_OK) return status;
        mat = &derived_xyz_to_rgb;
    }

    /* Apply XYZ -> RGB matrix */
    alwan_vec3_f32 vec_in, vec_out;
    ALWAN_MEMCPY(&vec_in, xyz, sizeof(alwan_vec3_f32));
    alwan_mat3_mulv_f32(&vec_out, mat, &vec_in);
    ALWAN_MEMCPY(rgb, &vec_out, sizeof(alwan_vec3_f32));

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */

/* ----------------------------------------------------------------
 * Transfer Function API
 * ---------------------------------------------------------------- */

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE
 * (referenced by the always-compiled alwan_aces2_output_transform_custom_f64). */
#if ALWAN_WITH_F64_FACADE
alwan_status alwan_oetf_apply_f64(alwan_f64 *encoded_out, size_t out_stride, alwan_f64 const *linear_in, size_t in_stride, size_t count, alwan_transfer_function tf) {
    if (!linear_in || !encoded_out) {
        return ALWAN_E_INVALID;
    }

    /* Select transfer function */
    alwan_tf_fn_f64 oetf_fn = alwan__resolve_oetf_f64(tf);
    if (!oetf_fn) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    if (in_stride == sizeof(double) && out_stride == sizeof(double)) {
        typedef alwan_simd (*simd_tf_fn)(alwan_simd);
        simd_tf_fn oetf_simd = NULL;
        switch (tf) {
        case ALWAN_TF_SRGB:   oetf_simd = alwan__srgb_oetf_simd;  break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084: oetf_simd = alwan__pq_oetf_simd;    break;
        case ALWAN_TF_HLG:    oetf_simd = alwan__hlg_oetf_simd;   break;
        default: break;
        }
        if (oetf_simd) {
            alwan_simd_lane const *in  = (alwan_simd_lane const *)linear_in;
            alwan_simd_lane       *out = (alwan_simd_lane       *)encoded_out;
            size_t const W = ALWAN_SIMD_WIDTH;
            size_t i = 0;
            for (; i + W <= count; i += W)
                alwan_simd_storeu(&out[i], oetf_simd(alwan_simd_loadu(&in[i])));
            for (; i < count; i++)
                out[i] = (alwan_simd_lane)oetf_fn((alwan_f64)in[i]);
            return ALWAN_OK;
        }
    }
#endif

    /* Apply transfer function to array */
    for (size_t i = 0; i < count; i++) {
        double const *in_ptr = (double const *)((char const *)linear_in + i * in_stride);
        double *out_ptr = (double *)((char *)encoded_out + i * out_stride);
        *out_ptr = oetf_fn(*in_ptr);
    }

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F32
alwan_status alwan_oetf_apply_f32(alwan_f32 *encoded_out, size_t out_stride, alwan_f32 const *linear_in, size_t in_stride, size_t count, alwan_transfer_function tf) {
    if (!linear_in || !encoded_out) return ALWAN_E_INVALID;

    alwan_tf_fn_f32 oetf_fn = alwan__resolve_oetf_f32(tf);
    if (!oetf_fn) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        float const *in_ptr = (float const *)((char const *)linear_in + i * in_stride);
        float *out_ptr = (float *)((char *)encoded_out + i * out_stride);
        *out_ptr = oetf_fn(*in_ptr);
    }
    return ALWAN_OK;
}

alwan_status alwan_eotf_apply_f32(alwan_f32 *linear_out, size_t out_stride, alwan_f32 const *encoded_in, size_t in_stride, size_t count, alwan_transfer_function tf) {
    if (!encoded_in || !linear_out) return ALWAN_E_INVALID;

    alwan_tf_fn_f32 eotf_fn = alwan__resolve_eotf_f32(tf);
    if (!eotf_fn) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        float const *in_ptr = (float const *)((char const *)encoded_in + i * in_stride);
        float *out_ptr = (float *)((char *)linear_out + i * out_stride);
        *out_ptr = eotf_fn(*in_ptr);
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE
 * (the aces2 inverse f32 facade applies the display EOTF in f64). */
#if ALWAN_WITH_F64_FACADE
alwan_status alwan_eotf_apply_f64(alwan_f64 *linear_out, size_t out_stride, alwan_f64 const *encoded_in, size_t in_stride, size_t count, alwan_transfer_function tf) {
    if (!encoded_in || !linear_out) {
        return ALWAN_E_INVALID;
    }

    /* Select transfer function */
    alwan_tf_fn_f64 eotf_fn = alwan__resolve_eotf_f64(tf);
    if (!eotf_fn) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    if (in_stride == sizeof(double) && out_stride == sizeof(double)) {
        typedef alwan_simd (*simd_tf_fn)(alwan_simd);
        simd_tf_fn eotf_simd = NULL;
        switch (tf) {
        case ALWAN_TF_SRGB:   eotf_simd = alwan__srgb_eotf_simd;  break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084: eotf_simd = alwan__pq_eotf_simd;    break;
        case ALWAN_TF_HLG:    eotf_simd = alwan__hlg_eotf_full_simd; break;
        default: break;
        }
        if (eotf_simd) {
            alwan_simd_lane const *in  = (alwan_simd_lane const *)encoded_in;
            alwan_simd_lane       *out = (alwan_simd_lane       *)linear_out;
            size_t const W = ALWAN_SIMD_WIDTH;
            size_t i = 0;
            for (; i + W <= count; i += W)
                alwan_simd_storeu(&out[i], eotf_simd(alwan_simd_loadu(&in[i])));
            for (; i < count; i++)
                out[i] = (alwan_simd_lane)eotf_fn((alwan_f64)in[i]);
            return ALWAN_OK;
        }
    }
#endif

    /* Apply transfer function to array */
    for (size_t i = 0; i < count; i++) {
        double const *in_ptr = (double const *)((char const *)encoded_in + i * in_stride);
        double *out_ptr = (double *)((char *)linear_out + i * out_stride);
        *out_ptr = eotf_fn(*in_ptr);
    }

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64_FACADE */

/* ----------------------------------------------------------------
 * M11: RGB <-> RGB Conversion
 * ---------------------------------------------------------------- */

/* Convert RGB color from one color space to another.
 * Native f32: mirrors alwan_rgb_convert_f64's algorithm exactly using f32
 * building blocks (no widen-to-f64 facade). */
#if ALWAN_WITH_F32
alwan_status alwan_rgb_convert_f32(alwan_rgb_f32 *dst_rgb, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_rgb_f32 const *src_rgb, alwan_ctx *ctx) {
    if (!src_space || !dst_space || !src_rgb || !dst_rgb) {
        return ALWAN_E_INVALID;
    }

    /* Get conversion matrices for both spaces */
    alwan_mat3x3_f32 src_to_xyz, xyz_to_src;
    alwan_mat3x3_f32 dst_to_xyz, xyz_to_dst;

    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
        xyz_to_src = src_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f32(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    if (dst_space->has_matrices) {
        dst_to_xyz = dst_space->rgb_to_xyz;
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f32(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Convert source RGB to XYZ */
    alwan_vec3_f32 vec_in;
    ALWAN_MEMCPY(&vec_in, src_rgb, sizeof(alwan_vec3_f32));
    alwan_vec3_f32 xyz;
    alwan_mat3_mulv_f32(&xyz, &src_to_xyz, &vec_in);

    /* Check if chromatic adaptation is needed */
    alwan_f32 const tolerance = 1e-6f;
    alwan_f32 dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_f32 dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_adaptation = (ALWAN_ABS_F32(dx) > tolerance || ALWAN_ABS_F32(dy) > tolerance);

    int status;
    if (need_adaptation && ctx) {
        /* Perform chromatic adaptation using Bradford CAT */
        alwan_xyy_f32 src_white_xyy, dst_white_xyy;
        alwan_xyz_f32 src_white_xyz, dst_white_xyz;

        /* Convert xy to XYZ (using Y=1.0) */
        src_white_xyy.x = src_space->white_xy[0];
        src_white_xyy.y = src_space->white_xy[1];
        src_white_xyy.Y = 1.0f;
        alwan_xyy_to_xyz_f32(&src_white_xyz, &src_white_xyy);

        dst_white_xyy.x = dst_space->white_xy[0];
        dst_white_xyy.y = dst_space->white_xy[1];
        dst_white_xyy.Y = 1.0f;
        alwan_xyy_to_xyz_f32(&dst_white_xyz, &dst_white_xyy);

        /* Compute and apply CAT matrix */
        alwan_mat3x3_f32 cat_matrix;
        status = alwan_cat_matrix_f32(&cat_matrix, &src_white_xyz, &dst_white_xyz,
                                  ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        alwan_vec3_f32 xyz_adapted;
        alwan_mat3_mulv_f32(&xyz_adapted, &cat_matrix, &xyz);
        xyz = xyz_adapted;
    }

    /* Convert adapted XYZ to destination RGB */
    alwan_vec3_f32 vec_out;
    alwan_mat3_mulv_f32(&vec_out, &xyz_to_dst, &xyz);
    ALWAN_MEMCPY(dst_rgb, &vec_out, sizeof(alwan_vec3_f32));

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */

#if ALWAN_WITH_F64
alwan_status alwan_rgb_convert_f64(alwan_rgb_f64 *dst_rgb, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_rgb_f64 const *src_rgb, alwan_ctx *ctx) {
    if (!src_space || !dst_space || !src_rgb || !dst_rgb) {
        return ALWAN_E_INVALID;
    }

    /* Get conversion matrices for both spaces */
    alwan_mat3x3_f64 src_to_xyz, xyz_to_src;
    alwan_mat3x3_f64 dst_to_xyz, xyz_to_dst;

    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
        xyz_to_src = src_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f64(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    if (dst_space->has_matrices) {
        dst_to_xyz = dst_space->rgb_to_xyz;
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f64(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Convert source RGB to XYZ */
    alwan_vec3_f64 vec_in;
    ALWAN_MEMCPY(&vec_in, src_rgb, sizeof(alwan_vec3_f64));
    alwan_vec3_f64 xyz;
    alwan_mat3_mulv_f64(&xyz, &src_to_xyz, &vec_in);

    /* Check if chromatic adaptation is needed */
    alwan_f64 const tolerance = ALWAN_LITERAL(1e-6);
    alwan_f64 dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_f64 dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_adaptation = (ALWAN_ABS(dx) > tolerance || ALWAN_ABS(dy) > tolerance);

    int status;
    if (need_adaptation && ctx) {
        /* Perform chromatic adaptation using Bradford CAT */
        alwan_xyy_f64 src_white_xyy, dst_white_xyy;
        alwan_xyz_f64 src_white_xyz, dst_white_xyz;

        /* Convert xy to XYZ (using Y=1.0) */
        src_white_xyy.x = src_space->white_xy[0];
        src_white_xyy.y = src_space->white_xy[1];
        src_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz_f64(&src_white_xyz, &src_white_xyy);

        dst_white_xyy.x = dst_space->white_xy[0];
        dst_white_xyy.y = dst_space->white_xy[1];
        dst_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz_f64(&dst_white_xyz, &dst_white_xyy);

        /* Compute and apply CAT matrix */
        alwan_mat3x3_f64 cat_matrix;
        status = alwan_cat_matrix_f64(&cat_matrix, &src_white_xyz, &dst_white_xyz,
                                  ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        alwan_vec3_f64 xyz_adapted;
        alwan_mat3_mulv_f64(&xyz_adapted, &cat_matrix, &xyz);
        xyz = xyz_adapted;
    }

    /* Convert adapted XYZ to destination RGB */
    alwan_vec3_f64 vec_out;
    alwan_mat3_mulv_f64(&vec_out, &xyz_to_dst, &xyz);
    ALWAN_MEMCPY(dst_rgb, &vec_out, sizeof(alwan_vec3_f64));

    return ALWAN_OK;
}

/* Map RGB color space conversion */
alwan_status alwan_rgb_convert_map_interleave_f64(alwan_rgb_f64 *dst_rgb, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_rgb_f64 const *src_rgb, size_t count, alwan_ctx *ctx) {
    if (!src_space || !dst_space || !src_rgb || !dst_rgb || count == 0) {
        return ALWAN_E_INVALID;
    }

    /* Get conversion matrices once for all colors */
    alwan_mat3x3_f64 src_to_xyz, xyz_to_src;
    alwan_mat3x3_f64 dst_to_xyz, xyz_to_dst;

    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
        xyz_to_src = src_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f64(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    if (dst_space->has_matrices) {
        dst_to_xyz = dst_space->rgb_to_xyz;
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f64(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Check if chromatic adaptation is needed */
    alwan_f64 const tolerance = ALWAN_LITERAL(1e-6);
    alwan_f64 dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_f64 dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_adaptation = (ALWAN_ABS(dx) > tolerance || ALWAN_ABS(dy) > tolerance);

    /* Precompute adaptation matrix if needed */
    alwan_mat3x3_f64 cat_matrix;
    if (need_adaptation && ctx) {
        alwan_xyy_f64 src_white_xyy, dst_white_xyy;
        alwan_xyz_f64 src_white_xyz, dst_white_xyz;

        /* Convert xy to XYZ (using Y=1.0) */
        src_white_xyy.x = src_space->white_xy[0];
        src_white_xyy.y = src_space->white_xy[1];
        src_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz_f64(&src_white_xyz, &src_white_xyy);

        dst_white_xyy.x = dst_space->white_xy[0];
        dst_white_xyy.y = dst_space->white_xy[1];
        dst_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz_f64(&dst_white_xyz, &dst_white_xyy);

        /* Compute CAT matrix once */
        int status = alwan_cat_matrix_f64(&cat_matrix, &src_white_xyz, &dst_white_xyz,
                                      ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;
    }

    /* Convert all colors */
    for (size_t i = 0; i < count; i++) {
        /* Convert source RGB to XYZ */
        alwan_vec3_f64 xyz, vec_in;
        ALWAN_MEMCPY(&vec_in, &src_rgb[i], sizeof(alwan_vec3_f64));
        alwan_mat3_mulv_f64(&xyz, &src_to_xyz, &vec_in);

        /* Apply chromatic adaptation if needed */
        if (need_adaptation && ctx) {
            alwan_vec3_f64 xyz_adapted;
            alwan_mat3_mulv_f64(&xyz_adapted, &cat_matrix, &xyz);
            xyz = xyz_adapted;
        }

        /* Convert adapted XYZ to destination RGB */
        alwan_vec3_f64 vec_out;
        alwan_mat3_mulv_f64(&vec_out, &xyz_to_dst, &xyz);
        ALWAN_MEMCPY(&dst_rgb[i], &vec_out, sizeof(alwan_vec3_f64));
    }

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */

/* Map RGB color space conversion.
 * Native f32 (HOT PATH): mirrors alwan_rgb_convert_map_interleave_f64's algorithm
 * exactly using f32 building blocks. Matrices/CAT precomputed once; per-pixel
 * matrix-vector products in f32. No f64 scratch, no heap allocation. */
#if ALWAN_WITH_F32
alwan_status alwan_rgb_convert_map_interleave_f32(alwan_rgb_f32 *dst_rgb, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_rgb_f32 const *src_rgb, size_t count, alwan_ctx *ctx) {
    if (!src_space || !dst_space || !src_rgb || !dst_rgb || count == 0) {
        return ALWAN_E_INVALID;
    }

    /* Get conversion matrices once for all colors */
    alwan_mat3x3_f32 src_to_xyz, xyz_to_src;
    alwan_mat3x3_f32 dst_to_xyz, xyz_to_dst;

    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
        xyz_to_src = src_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f32(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    if (dst_space->has_matrices) {
        dst_to_xyz = dst_space->rgb_to_xyz;
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f32(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Check if chromatic adaptation is needed */
    alwan_f32 const tolerance = 1e-6f;
    alwan_f32 dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_f32 dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_adaptation = (ALWAN_ABS_F32(dx) > tolerance || ALWAN_ABS_F32(dy) > tolerance);

    /* Precompute adaptation matrix if needed */
    alwan_mat3x3_f32 cat_matrix;
    if (need_adaptation && ctx) {
        alwan_xyy_f32 src_white_xyy, dst_white_xyy;
        alwan_xyz_f32 src_white_xyz, dst_white_xyz;

        /* Convert xy to XYZ (using Y=1.0) */
        src_white_xyy.x = src_space->white_xy[0];
        src_white_xyy.y = src_space->white_xy[1];
        src_white_xyy.Y = 1.0f;
        alwan_xyy_to_xyz_f32(&src_white_xyz, &src_white_xyy);

        dst_white_xyy.x = dst_space->white_xy[0];
        dst_white_xyy.y = dst_space->white_xy[1];
        dst_white_xyy.Y = 1.0f;
        alwan_xyy_to_xyz_f32(&dst_white_xyz, &dst_white_xyy);

        /* Compute CAT matrix once */
        int status = alwan_cat_matrix_f32(&cat_matrix, &src_white_xyz, &dst_white_xyz,
                                      ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;
    }

    /* Convert all colors */
    for (size_t i = 0; i < count; i++) {
        /* Convert source RGB to XYZ */
        alwan_vec3_f32 xyz, vec_in;
        ALWAN_MEMCPY(&vec_in, &src_rgb[i], sizeof(alwan_vec3_f32));
        alwan_mat3_mulv_f32(&xyz, &src_to_xyz, &vec_in);

        /* Apply chromatic adaptation if needed */
        if (need_adaptation && ctx) {
            alwan_vec3_f32 xyz_adapted;
            alwan_mat3_mulv_f32(&xyz_adapted, &cat_matrix, &xyz);
            xyz = xyz_adapted;
        }

        /* Convert adapted XYZ to destination RGB */
        alwan_vec3_f32 vec_out;
        alwan_mat3_mulv_f32(&vec_out, &xyz_to_dst, &xyz);
        ALWAN_MEMCPY(&dst_rgb[i], &vec_out, sizeof(alwan_vec3_f32));
    }

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */

/* ----------------------------------------------------------------
 * RGB Space Descriptor Helper
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * Embedded RGB Space Data
 * ---------------------------------------------------------------- */
#include "../alwan_rgb_embedded.h"
#include "../alwan_rgb_matrices_embedded.h"

/* ----------------------------------------------------------------
 * Transfer Function Lookup Table
 * Maps each alwan_rgb_space enum index to its {oetf, eotf} pair.
 * Order MUST match the alwan_rgb_space enum.
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_transfer_function oetf;
    alwan_transfer_function eotf;
} alwan_tf_pair;

static alwan_tf_pair const g_rgb_space_tf[] = {
    /* [0]  SRGB                     */ { ALWAN_TF_SRGB,     ALWAN_TF_SRGB     },
    /* [1]  BT709                    */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [2]  DISPLAY_P3               */ { ALWAN_TF_SRGB,     ALWAN_TF_SRGB     },
    /* [3]  BT2020                   */ { ALWAN_TF_BT2020,   ALWAN_TF_BT2020   },
    /* [4]  ACES2065_1               */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [5]  ACESCG                   */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [6]  ACESPROXY                */ { ALWAN_TF_ACESPROXY,ALWAN_TF_ACESPROXY},
    /* [7]  ACESCC                   */ { ALWAN_TF_ACESCC,   ALWAN_TF_ACESCC   },
    /* [8]  ACESCCT                  */ { ALWAN_TF_ACESCCT,  ALWAN_TF_ACESCCT  },
    /* [9]  ARRI_WIDE_GAMUT_3        */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [10] ARRI_WIDE_GAMUT_4        */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [11] ARRI_LOGC3               */ { ALWAN_TF_LOGC3,    ALWAN_TF_LOGC3    },
    /* [12] ARRI_LOGC4               */ { ALWAN_TF_LOGC4,    ALWAN_TF_LOGC4    },
    /* [13] REDCOLOR                 */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [14] REDCOLOR2                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [15] REDCOLOR3                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [16] REDCOLOR4                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [17] DRAGONCOLOR              */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [18] DRAGONCOLOR2             */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [19] REDLOG                   */ { ALWAN_TF_REDLOG,   ALWAN_TF_REDLOG   },
    /* [20] VENICE_S_GAMUT3          */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [21] VENICE_S_GAMUT3_CINE     */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [22] S_LOG                    */ { ALWAN_TF_SLOG,     ALWAN_TF_SLOG     },
    /* [23] S_LOG2                   */ { ALWAN_TF_SLOG2,    ALWAN_TF_SLOG2    },
    /* [24] S_LOG3                   */ { ALWAN_TF_SLOG3,    ALWAN_TF_SLOG3    },
    /* [25] CIE_RGB                  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [26] ADOBE_WIDE_GAMUT_RGB     */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [27] ROMM_RGB                 */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [28] RIMM_RGB                 */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [29] ERIMM_RGB                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [30] FILMLIGHT_E_GAMUT        */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [31] FILMLIGHT_T_LOG          */ { ALWAN_TF_TLOG,     ALWAN_TF_TLOG     },
    /* [32] F_GAMUT                  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [33] FUJIFILM_F_LOG           */ { ALWAN_TF_FLOG,     ALWAN_TF_FLOG     },
    /* [34] N_GAMUT                  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [35] N_LOG                    */ { ALWAN_TF_NLOG,     ALWAN_TF_NLOG     },
    /* [36] DJI_D_GAMUT             */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [37] PROTUNE_NATIVE           */ { ALWAN_TF_PROTUNE,  ALWAN_TF_PROTUNE  },
    /* [38] ITU_R_BT470_525          */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [39] ITU_R_BT470_625          */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [40] SMPTE_240M               */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [41] SMPTE_C                  */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [42] DCDM_XYZ                 */ { ALWAN_TF_DCDM,     ALWAN_TF_DCDM     },
    /* [43] BEST_RGB                 */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [44] BETA_RGB                 */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [45] DON_RGB_4                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [46] EKTA_SPACE_PS5           */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [47] MAX_RGB                  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [48] RUSSELL_RGB              */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [49] SHARP_RGB                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [50] ECI_RGB_V2               */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [51] ADOBE_RGB_1998           */ { ALWAN_TF_GAMMA22,  ALWAN_TF_GAMMA22  },
    /* [52] PROPHOTO_RGB             */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [53] DAVINCI_WIDE_GAMUT       */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [54] DAVINCI_INTERMEDIATE     */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [55] BLACKMAGIC_WIDE_GAMUT    */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [56] BLACKMAGIC_FILM          */ { ALWAN_TF_BMDFILM4, ALWAN_TF_BMDFILM4 },
    /* [57] BLACKMAGIC_FILM_GEN5     */ { ALWAN_TF_BMDFILM,  ALWAN_TF_BMDFILM  },
    /* [58] V_GAMUT                  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [59] V_LOG                    */ { ALWAN_TF_VLOG,     ALWAN_TF_VLOG     },
    /* [60] S_GAMUT                  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [61] S_GAMUT3                 */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [62] S_GAMUT3_CINE            */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [63] CINEMA_GAMUT             */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [64] CANON_LOG                */ { ALWAN_TF_CLOG,     ALWAN_TF_CLOG     },
    /* [65] REDWIDEGAMUTRGB          */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [66] DCI_P3                   */ { ALWAN_TF_GAMMA26,  ALWAN_TF_GAMMA26  },
    /* [67] DCI_P3_P                 */ { ALWAN_TF_GAMMA26,  ALWAN_TF_GAMMA26  },
    /* [68] P3_D65                   */ { ALWAN_TF_SRGB,     ALWAN_TF_SRGB     },
    /* [69] NTSC_1953                */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [70] NTSC_1987                */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [71] PAL_SECAM                */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [72] EBU_TECH_3213_E          */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [73] APPLE_RGB                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [74] COLORMATCH_RGB           */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [75] ALEXA_WIDE_GAMUT         */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [76] P3_D60                   */ { ALWAN_TF_GAMMA26,  ALWAN_TF_GAMMA26  },
    /* [77] XTREME_RGB               */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [78] LINEAR_REC709            */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [79] LINEAR_REC2020           */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [80] LINEAR_ADOBE_RGB_1998    */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [81] LINEAR_P3_D65            */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [82] LINEAR_DISPLAY_P3        */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [83] LINEAR_PROPHOTO_RGB      */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [84] LINEAR_DCI_P3            */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [85] LINEAR_ADOBE_WIDE_GAMUT  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [86] LINEAR_APPLE_RGB         */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [87] LINEAR_COLORMATCH_RGB    */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [88] LINEAR_P3_D60            */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [89] LINEAR_BT470_525         */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [90] LINEAR_BT470_625         */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [91] LINEAR_SMPTE_240M        */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [92] ITU_T_H273_22_UNSPEC     */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [93] ITU_T_H273_GENERIC_FILM  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [94] PLASA_ANSI_E154          */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [95] GAMMA22_REC709           */ { ALWAN_TF_GAMMA22,  ALWAN_TF_GAMMA22  },
    /* [96] GAMMA22_ADOBE_RGB        */ { ALWAN_TF_GAMMA22,  ALWAN_TF_GAMMA22  },
    /* [97] GAMMA22_P3_D65           */ { ALWAN_TF_GAMMA22,  ALWAN_TF_GAMMA22  },
    /* [98] GAMMA22_AP1              */ { ALWAN_TF_GAMMA22,  ALWAN_TF_GAMMA22  },
    /* [99] GAMMA18_REC709           */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* ColorInterop Display Color Spaces */
    /* [100] REC1886_REC709          */ { ALWAN_TF_BT709,    ALWAN_TF_BT1886   },
    /* [101] REC2100_PQ              */ { ALWAN_TF_PQ,       ALWAN_TF_PQ       },
    /* [102] REC2100_HLG             */ { ALWAN_TF_HLG,      ALWAN_TF_HLG      },
    /* [103] DISPLAY_P3_HDR          */ { ALWAN_TF_PQ,       ALWAN_TF_PQ       },
};

/* The descriptor lookups below bounds-check `space` once and then subscript four
 * separate tables with it: primaries, transfer functions, and matrices, in two
 * precisions. Every one of them is asserted against alwan_rgb_space here or at its
 * own definition, so the single check is sound. Comparing the tables against each
 * other instead would let all of them drift away from the enum together. */
_Static_assert(
    sizeof(g_rgb_space_tf) / sizeof(g_rgb_space_tf[0]) == (size_t)ALWAN_RGB_SPACE_COUNT,
    "g_rgb_space_tf[] must have one row per alwan_rgb_space"
);

/* ----------------------------------------------------------------
 * Get RGB space descriptor by enum
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F32
alwan_status alwan_rgb_get_space_descriptor_f32(alwan_rgb_space_desc_f32 *desc, alwan_rgb_space space, alwan_ctx *ctx) {
    if (!desc) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_EMBED_DATA
    /* Embedded data mode - read native f32 tables directly (no f64->f32
     * double-rounding). Mirrors alwan_rgb_get_space_descriptor_f64 exactly. */
    (void)ctx;  /* Unused in embedded mode */

    /* Bounds check: ensure enum value is within valid array range */
    if (space < 0 || (size_t)space >= (size_t)ALWAN_RGB_SPACE_COUNT) {
        return ALWAN_E_INVALID;
    }

    /* Direct lookup using enum as index - data format: [rx, ry, gx, gy, bx, by, wx, wy] */
    float const *rgb_data = g_rgb_space_data_f32[space];

    /* Copy primaries (first 6 values) and white point (last 2 values) */
    for (int j = 0; j < 6; j++) {
        desc->primaries_xy[j] = rgb_data[j];
    }
    desc->white_xy[0] = rgb_data[6];
    desc->white_xy[1] = rgb_data[7];

    /* Look up transfer functions from the TF table */
    desc->oetf = g_rgb_space_tf[space].oetf;
    desc->eotf = g_rgb_space_tf[space].eotf;

    /* Load precomputed matrices */
    float const *mat_data = g_rgb_space_matrices_f32[space];
    for (int j = 0; j < 9; j++) {
        desc->rgb_to_xyz.m[j] = mat_data[j];
        desc->xyz_to_rgb.m[j] = mat_data[j + 9];
    }
    desc->has_matrices = 1;

    return ALWAN_OK;

#else
    #error "Only ALWAN_EMBED_DATA mode is supported. Runtime CSV loading has been removed."
#endif /* ALWAN_EMBED_DATA */
}
#endif /* ALWAN_WITH_F32 */

#if ALWAN_WITH_F64
alwan_status alwan_rgb_get_space_descriptor_f64(alwan_rgb_space_desc_f64 *desc, alwan_rgb_space space, alwan_ctx *ctx) {
    if (!desc) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_EMBED_DATA
    /* Embedded data mode - direct array indexing (enum values map to array indices) */
    (void)ctx;  /* Unused in embedded mode */

    /* Bounds check: ensure enum value is within valid array range */
    if (space < 0 || (size_t)space >= (size_t)ALWAN_RGB_SPACE_COUNT) {
        return ALWAN_E_INVALID;
    }

    /* Direct lookup using enum as index - data format: [rx, ry, gx, gy, bx, by, wx, wy] */
    alwan_f64 const *rgb_data = g_rgb_space_data[space];

    /* Copy primaries (first 6 values) and white point (last 2 values) */
    for (int j = 0; j < 6; j++) {
        desc->primaries_xy[j] = rgb_data[j];
    }
    desc->white_xy[0] = rgb_data[6];
    desc->white_xy[1] = rgb_data[7];

    /* Look up transfer functions from the TF table */
    desc->oetf = g_rgb_space_tf[space].oetf;
    desc->eotf = g_rgb_space_tf[space].eotf;

    /* Load precomputed matrices */
    alwan_f64 const *mat_data = g_rgb_space_matrices[space];
    for (int j = 0; j < 9; j++) {
        desc->rgb_to_xyz.m[j] = mat_data[j];
        desc->xyz_to_rgb.m[j] = mat_data[j + 9];
    }
    desc->has_matrices = 1;

    return ALWAN_OK;

#else
    #error "Only ALWAN_EMBED_DATA mode is supported. Runtime CSV loading has been removed."
#endif /* ALWAN_EMBED_DATA */
}
#endif /* ALWAN_WITH_F64 */

/* ----------------------------------------------------------------
 * Arbitrary Gamma OETF/EOTF
 *
 * Templatized in alwan_rgb_impl.inc.
 * ---------------------------------------------------------------- */
