/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map sRGB Convenience and Batch Delta E - True SIMD vectorized
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_colorspace_core.h"
#include "../core/alwan_oklab_core.h"
#include "../core/alwan_rgb_core.h"

/* ----------------------------------------------------------------
 * sRGB <-> XYZ D65 matrices (BT.709 primaries) - dual precision
 * ---------------------------------------------------------------- */

/* sRGB / BT.709 NPM (Normalized Primary Matrix) and its inverse.
 * These are computed from exact BT.709 primaries + D65 white point via gendata
 * (colour-science rgb_spaces.py), NOT the IEC 61966-2-1 rounded constants.
 *
 * Difference vs IEC 61966-2-1 published matrix (4 decimal places):
 *   IEC:   0.4124, 0.3576, 0.1805, ...
 *   ours:  0.41239080, 0.35758434, 0.18048079, ...
 *   delta: ~1e-5 per cell (5th decimal place)
 *
 * This produces a ~0.007 difference in Lab a* for saturated colors compared
 * to implementations that use the rounded IEC constants (e.g. colour-science).
 * Both are valid — ours is more precise, IEC is the published standard. */
ALWAN_DIAG_PUSH
ALWAN_CONSTEXPR alwan_mat3x3_f64 SRGB_TO_XYZ = {{
#include "../data/matrices/aces_rec709_to_xyz.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3_f64 XYZ_TO_SRGB = {{
#include "../data/matrices/aces_xyz_to_rec709.csv"
}};
ALWAN_DIAG_POP

/* f32 variants */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3_f32 SRGB_TO_XYZ_f32 = {{
#include "../data/matrices/aces_rec709_to_xyz.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3_f32 XYZ_TO_SRGB_f32 = {{
#include "../data/matrices/aces_xyz_to_rec709.csv"
}};
ALWAN_DIAG_POP

/* f64 variants */
ALWAN_DIAG_PUSH
ALWAN_CONSTEXPR alwan_mat3x3_f64 SRGB_TO_XYZ_f64 = {{
#include "../data/matrices/aces_rec709_to_xyz.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3_f64 XYZ_TO_SRGB_f64 = {{
#include "../data/matrices/aces_xyz_to_rec709.csv"
}};
ALWAN_DIAG_POP

/* D65 white point (Y=1 normalized) - compile-time + dual precision */
ALWAN_DIAG_PUSH
static alwan_f64 const D65_WP_Y1[] = {
#include "../data/white_d65_xyz_y1.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static float const D65_WP_Y1_f32[] = {
#include "../data/white_d65_xyz_y1.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
static double const D65_WP_Y1_f64[] = {
#include "../data/white_d65_xyz_y1.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Single-pixel sRGB Convenience Conversions
 * Used by _map_interleave_ex macros; composite of EOTF/OETF + matrix + core
 * ---------------------------------------------------------------- */

int alwan_srgb_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_rgb_f64 const *rgb) {
    if (!rgb || !xyz) return ALWAN_E_INVALID;
    alwan_vec3_f64 v = {{alwan_srgb_eotf_f64(rgb->r), alwan_srgb_eotf_f64(rgb->g), alwan_srgb_eotf_f64(rgb->b)}};
    alwan_vec3_f64 r = alwan_mat3_mulv_f64_v(SRGB_TO_XYZ, v);
    xyz->x = r.v[0]; xyz->y = r.v[1]; xyz->z = r.v[2];
    return ALWAN_OK;
}

int alwan_xyz_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_xyz_f64 const *xyz) {
    if (!xyz || !rgb) return ALWAN_E_INVALID;
    alwan_vec3_f64 v = {{xyz->x, xyz->y, xyz->z}};
    alwan_vec3_f64 lin = alwan_mat3_mulv_f64_v(XYZ_TO_SRGB, v);
    rgb->r = alwan_srgb_oetf_f64(lin.v[0]); rgb->g = alwan_srgb_oetf_f64(lin.v[1]); rgb->b = alwan_srgb_oetf_f64(lin.v[2]);
    return ALWAN_OK;
}

int alwan_srgb_to_lab_f64(alwan_lab_f64 *lab, alwan_rgb_f64 const *rgb) {
    if (!rgb || !lab) return ALWAN_E_INVALID;
    alwan_vec3_f64 v = {{alwan_srgb_eotf_f64(rgb->r), alwan_srgb_eotf_f64(rgb->g), alwan_srgb_eotf_f64(rgb->b)}};
    alwan_vec3_f64 xyz = alwan_mat3_mulv_f64_v(SRGB_TO_XYZ, v);
    alwan_xyz_f64 wp = {D65_WP_Y1[0], D65_WP_Y1[1], D65_WP_Y1[2]};
    alwan_xyz_f64 xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
    *lab = alwan_xyz_to_lab_f64_v(xyz_s, wp);
    return ALWAN_OK;
}

int alwan_lab_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_lab_f64 const *lab) {
    if (!lab || !rgb) return ALWAN_E_INVALID;
    alwan_xyz_f64 wp = {D65_WP_Y1[0], D65_WP_Y1[1], D65_WP_Y1[2]};
    alwan_xyz_f64 xyz = alwan_lab_to_xyz_f64_v(*lab, wp);
    alwan_vec3_f64 v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3_f64 lin = alwan_mat3_mulv_f64_v(XYZ_TO_SRGB, v);
    rgb->r = alwan_srgb_oetf_f64(lin.v[0]); rgb->g = alwan_srgb_oetf_f64(lin.v[1]); rgb->b = alwan_srgb_oetf_f64(lin.v[2]);
    return ALWAN_OK;
}

int alwan_srgb_to_oklab_f64(alwan_oklab_f64 *oklab, alwan_rgb_f64 const *rgb) {
    if (!rgb || !oklab) return ALWAN_E_INVALID;
    alwan_vec3_f64 v = {{alwan_srgb_eotf_f64(rgb->r), alwan_srgb_eotf_f64(rgb->g), alwan_srgb_eotf_f64(rgb->b)}};
    alwan_vec3_f64 xyz = alwan_mat3_mulv_f64_v(SRGB_TO_XYZ, v);
    alwan_xyz_f64 xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
    *oklab = alwan_xyz_to_oklab_f64_v(xyz_s);
    return ALWAN_OK;
}

int alwan_oklab_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_oklab_f64 const *oklab) {
    if (!oklab || !rgb) return ALWAN_E_INVALID;
    alwan_xyz_f64 xyz = alwan_oklab_to_xyz_f64_v(*oklab);
    alwan_vec3_f64 v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3_f64 lin = alwan_mat3_mulv_f64_v(XYZ_TO_SRGB, v);
    rgb->r = alwan_srgb_oetf_f64(lin.v[0]); rgb->g = alwan_srgb_oetf_f64(lin.v[1]); rgb->b = alwan_srgb_oetf_f64(lin.v[2]);
    return ALWAN_OK;
}

/* ================================================================
 * Dual-precision SIMD kernels + typed wrappers
 * ================================================================ */

/* === f32 pass === */
#define ALWAN_MAP_F32
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_rgb_map_kernels.inc"
#include "alwan_map_simd_undef.h"

/* === f64 pass === */
#define ALWAN_MAP_F64
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_rgb_map_kernels.inc"
#include "alwan_map_simd_undef.h"

/* ================================================================
 * Backward-compatible kernel aliases (unsuffixed -> compile-time selected)
 * ================================================================ */

#define alwan__srgb_to_xyz_kernel       alwan__srgb_to_xyz_kernel_f64
#define alwan__xyz_to_srgb_kernel       alwan__xyz_to_srgb_kernel_f64
#define alwan__srgb_to_lab_kernel       alwan__srgb_to_lab_kernel_f64
#define alwan__lab_to_srgb_kernel       alwan__lab_to_srgb_kernel_f64
#define alwan__srgb_to_oklab_kernel     alwan__srgb_to_oklab_kernel_f64
#define alwan__oklab_to_srgb_kernel     alwan__oklab_to_srgb_kernel_f64

/* ================================================================
 * _ex Interleave Variants (dual-dispatch: F64 -> f64 pipeline, else f32)
 * ================================================================ */

ALWAN_EX_DELEGATE_DUAL(alwan_srgb_to_xyz_map_interleave_ex,
                       alwan_srgb_to_xyz_f32_map_interleave,
                       alwan_srgb_to_xyz_f64_map_interleave)

ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_srgb_map_interleave_ex,
                       alwan_xyz_to_srgb_f32_map_interleave,
                       alwan_xyz_to_srgb_f64_map_interleave)

ALWAN_EX_DELEGATE_DUAL(alwan_srgb_to_lab_map_interleave_ex,
                       alwan_srgb_to_lab_f32_map_interleave,
                       alwan_srgb_to_lab_f64_map_interleave)

ALWAN_EX_DELEGATE_DUAL(alwan_lab_to_srgb_map_interleave_ex,
                       alwan_lab_to_srgb_f32_map_interleave,
                       alwan_lab_to_srgb_f64_map_interleave)

ALWAN_EX_DELEGATE_DUAL(alwan_srgb_to_oklab_map_interleave_ex,
                       alwan_srgb_to_oklab_f32_map_interleave,
                       alwan_srgb_to_oklab_f64_map_interleave)

ALWAN_EX_DELEGATE_DUAL(alwan_oklab_to_srgb_map_interleave_ex,
                       alwan_oklab_to_srgb_f32_map_interleave,
                       alwan_oklab_to_srgb_f64_map_interleave)

/* ----------------------------------------------------------------
 * Batch Delta E Computations
 * ---------------------------------------------------------------- */

int alwan_delta_e_76_f64_batch(alwan_f64 *delta_e_out,
                           alwan_f64 const *lab1_in, size_t in1_stride,
                           alwan_f64 const *lab2_in, size_t in2_stride,
                           size_t count) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *in1_ptr = (alwan_f64 const *)((char const *)lab1_in + i * in1_stride);
        alwan_f64 const *in2_ptr = (alwan_f64 const *)((char const *)lab2_in + i * in2_stride);
        alwan_lab_f64 lab1 = {in1_ptr[0], in1_ptr[1], in1_ptr[2]};
        alwan_lab_f64 lab2 = {in2_ptr[0], in2_ptr[1], in2_ptr[2]};
        delta_e_out[i] = alwan_delta_e_76_f64(&lab1, &lab2);
    }

    return ALWAN_OK;
}

int alwan_delta_e_2000_f64_batch(alwan_f64 *delta_e_out,
                             alwan_f64 const *lab1_in, size_t in1_stride,
                             alwan_f64 const *lab2_in, size_t in2_stride,
                             size_t count) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *in1_ptr = (alwan_f64 const *)((char const *)lab1_in + i * in1_stride);
        alwan_f64 const *in2_ptr = (alwan_f64 const *)((char const *)lab2_in + i * in2_stride);
        alwan_lab_f64 lab1 = {in1_ptr[0], in1_ptr[1], in1_ptr[2]};
        alwan_lab_f64 lab2 = {in2_ptr[0], in2_ptr[1], in2_ptr[2]};
        delta_e_out[i] = alwan_delta_e_2000_f64(&lab1, &lab2);
    }

    return ALWAN_OK;
}

int alwan_delta_e_94_f64_batch(alwan_f64 *delta_e_out,
                           alwan_f64 const *lab1_in, size_t in1_stride,
                           alwan_f64 const *lab2_in, size_t in2_stride,
                           size_t count) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *in1_ptr = (alwan_f64 const *)((char const *)lab1_in + i * in1_stride);
        alwan_f64 const *in2_ptr = (alwan_f64 const *)((char const *)lab2_in + i * in2_stride);
        alwan_lab_f64 lab1 = {in1_ptr[0], in1_ptr[1], in1_ptr[2]};
        alwan_lab_f64 lab2 = {in2_ptr[0], in2_ptr[1], in2_ptr[2]};
        delta_e_out[i] = alwan_delta_e_94_f64(&lab1, &lab2);
    }

    return ALWAN_OK;
}

int alwan_delta_e_cmc_f64_batch(alwan_f64 *delta_e_out,
                            alwan_f64 const *lab1_in, size_t in1_stride,
                            alwan_f64 const *lab2_in, size_t in2_stride,
                            size_t count,
                            alwan_f64 l,
                            alwan_f64 c) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *in1_ptr = (alwan_f64 const *)((char const *)lab1_in + i * in1_stride);
        alwan_f64 const *in2_ptr = (alwan_f64 const *)((char const *)lab2_in + i * in2_stride);
        alwan_lab_f64 lab1 = {in1_ptr[0], in1_ptr[1], in1_ptr[2]};
        alwan_lab_f64 lab2 = {in2_ptr[0], in2_ptr[1], in2_ptr[2]};
        alwan_delta_e_cmc_params_f64 cmc_p; cmc_p.l = (double)l; cmc_p.c = (double)c;
        delta_e_out[i] = alwan_delta_e_cmc_f64(&lab1, &lab2, &cmc_p);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Image Color Space Conversion
 * ---------------------------------------------------------------- */

/* alwan__resolve_eotf / alwan__resolve_oetf are now in alwan_internal.h */

/* Pixel stride in bytes for a 3-channel pixel of the given format. */
static size_t alwan__pixel_stride(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return 3 * sizeof(uint8_t);
    case ALWAN_PIXEL_U16: return 3 * sizeof(uint16_t);
    case ALWAN_PIXEL_F16: return 3 * sizeof(uint16_t);
    case ALWAN_PIXEL_F32: return 3 * sizeof(float);
    case ALWAN_PIXEL_F64: return 3 * sizeof(double);
    default:              return 0;
    }
}

/* Pixel stride in bytes for a 4-channel (RGBA) pixel of the given format. */
static size_t alwan__pixel_stride4(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return 4 * sizeof(uint8_t);
    case ALWAN_PIXEL_U16: return 4 * sizeof(uint16_t);
    case ALWAN_PIXEL_F16: return 4 * sizeof(uint16_t);
    case ALWAN_PIXEL_F32: return 4 * sizeof(float);
    case ALWAN_PIXEL_F64: return 4 * sizeof(double);
    default:              return 0;
    }
}

int alwan_image_convert_f64(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_ctx *ctx) {

    if (!dst || !src || !src_space || !dst_space || width == 0 || height == 0) {
        return ALWAN_E_INVALID;
    }

    /* Resolve pixel strides from format */
    size_t const src_px = alwan__pixel_stride(src_fmt);
    size_t const dst_px = alwan__pixel_stride(dst_fmt);
    if (src_px == 0 || dst_px == 0) return ALWAN_E_INVALID;

    /* Derive conversion matrices */
    alwan_mat3x3_f64 src_to_xyz, xyz_to_src;
    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
    } else {
        int status = alwan_rgb_derive_matrices_f64(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    alwan_mat3x3_f64 dst_to_xyz, xyz_to_dst;
    if (dst_space->has_matrices) {
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f64(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Check if chromatic adaptation is needed */
    alwan_f64 const tol = ALWAN_LITERAL(1e-6);
    alwan_f64 dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_f64 dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_cat = (ALWAN_ABS(dx) > tol || ALWAN_ABS(dy) > tol);

    /* Precompute a single combined matrix: xyz_to_dst * [cat *] src_to_xyz
     * This reduces per-pixel work to one mat3 multiply. */
    alwan_mat3x3_f64 combined;
    if (need_cat && ctx) {
        alwan_xyy_f64 src_xyy, dst_xyy;
        alwan_xyz_f64 src_wp, dst_wp;
        src_xyy.x = src_space->white_xy[0];
        src_xyy.y = src_space->white_xy[1];
        src_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz_f64(&src_wp, &src_xyy);

        dst_xyy.x = dst_space->white_xy[0];
        dst_xyy.y = dst_space->white_xy[1];
        dst_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz_f64(&dst_wp, &dst_xyy);

        alwan_mat3x3_f64 cat;
        int status = alwan_cat_matrix_f64(&cat, &src_wp, &dst_wp, ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        /* combined = xyz_to_dst * cat * src_to_xyz */
        alwan_mat3x3_f64 tmp = alwan_mat3_mul_f64_v(cat, src_to_xyz);
        combined = alwan_mat3_mul_f64_v(xyz_to_dst, tmp);
    } else {
        /* combined = xyz_to_dst * src_to_xyz */
        combined = alwan_mat3_mul_f64_v(xyz_to_dst, src_to_xyz);
    }

    /* Resolve transfer function pointers */
    alwan_tf_fn_f64 eotf_fn = alwan__resolve_eotf_f64(src_space->eotf);
    alwan_tf_fn_f64 oetf_fn = alwan__resolve_oetf_f64(dst_space->oetf);
    if (!eotf_fn || !oetf_fn) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    /* SIMD fast path: resolve SIMD transfer function pairs */
    {
        typedef alwan_simd (*simd_tf_fn)(alwan_simd);
        simd_tf_fn eotf_simd = NULL, oetf_simd = NULL;

        switch (src_space->eotf) {
        case ALWAN_TF_SRGB:   eotf_simd = alwan__srgb_eotf_simd;  break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084: eotf_simd = alwan__pq_eotf_simd;    break;
        case ALWAN_TF_HLG:    eotf_simd = alwan__hlg_eotf_full_simd; break;
        case ALWAN_TF_LINEAR: eotf_simd = NULL;                      break; /* identity -- handle below */
        default: break;
        }

        switch (dst_space->oetf) {
        case ALWAN_TF_SRGB:   oetf_simd = alwan__srgb_oetf_simd;  break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084: oetf_simd = alwan__pq_oetf_simd;    break;
        case ALWAN_TF_HLG:    oetf_simd = alwan__hlg_oetf_simd;   break;
        case ALWAN_TF_LINEAR: oetf_simd = NULL;                    break; /* identity -- handle below */
        default: break;
        }

        /* Use SIMD path if both TFs are known (including LINEAR=identity) */
        if (eotf_simd || src_space->eotf == ALWAN_TF_LINEAR) {
            if (oetf_simd || dst_space->oetf == ALWAN_TF_LINEAR) {
                int const eotf_is_linear = (src_space->eotf == ALWAN_TF_LINEAR);
                int const oetf_is_linear = (dst_space->oetf == ALWAN_TF_LINEAR);
                size_t const W = ALWAN_SIMD_WIDTH;

                for (size_t y = 0; y < height; y++) {
                    char const *src_row = (char const *)src + y * src_row_stride;
                    char       *dst_row = (char       *)dst + y * dst_row_stride;
                    size_t processed = 0;

                    while (processed < width) {
                        size_t tile = width - processed;
                        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
                        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];

                        alwan__load_tile_typed_3(c0, c1, c2, src_row, src_fmt, processed, src_px, tile);

                        /* SIMD: EOTF -> mat3 -> OETF */
                        {
                            size_t j = 0;
                            for (; j + W <= tile; j += W) {
                                alwan_simd vr = alwan_simd_load(&c0[j]);
                                alwan_simd vg = alwan_simd_load(&c1[j]);
                                alwan_simd vb = alwan_simd_load(&c2[j]);

                                /* EOTF: encoded -> linear */
                                if (!eotf_is_linear) {
                                    vr = eotf_simd(vr);
                                    vg = eotf_simd(vg);
                                    vb = eotf_simd(vb);
                                }

                                /* Combined matrix multiply */
                                alwan_simd dr, dg, db;
                                alwan__mat3_mul_simd(&dr, &dg, &db, &combined, vr, vg, vb);

                                /* OETF: linear -> encoded */
                                if (!oetf_is_linear) {
                                    dr = oetf_simd(dr);
                                    dg = oetf_simd(dg);
                                    db = oetf_simd(db);
                                }

                                alwan_simd_store(&c0[j], dr);
                                alwan_simd_store(&c1[j], dg);
                                alwan_simd_store(&c2[j], db);
                            }
                            /* Scalar tail within tile */
                            for (; j < tile; j++) {
                                alwan_f64 r = (alwan_f64)c0[j];
                                alwan_f64 g = (alwan_f64)c1[j];
                                alwan_f64 b = (alwan_f64)c2[j];
                                if (!eotf_is_linear) {
                                    r = eotf_fn(r); g = eotf_fn(g); b = eotf_fn(b);
                                }
                                alwan_vec3_f64 lin = {{r, g, b}};
                                alwan_vec3_f64 dst_lin = alwan_mat3_mulv_f64_v(combined, lin);
                                if (!oetf_is_linear) {
                                    c0[j] = (alwan_simd_lane)oetf_fn(dst_lin.v[0]);
                                    c1[j] = (alwan_simd_lane)oetf_fn(dst_lin.v[1]);
                                    c2[j] = (alwan_simd_lane)oetf_fn(dst_lin.v[2]);
                                } else {
                                    c0[j] = (alwan_simd_lane)dst_lin.v[0];
                                    c1[j] = (alwan_simd_lane)dst_lin.v[1];
                                    c2[j] = (alwan_simd_lane)dst_lin.v[2];
                                }
                            }
                        }

                        alwan__store_tile_typed_3(dst_row, dst_fmt, processed, dst_px, c0, c1, c2, tile);
                        processed += tile;
                    }
                }
                return ALWAN_OK;
            }
        }
    }
#endif /* ALWAN_SIMD_WIDTH > 1 */

    /* Scalar fallback */
    for (size_t y = 0; y < height; y++) {
        char const *src_row = (char const *)src + y * src_row_stride;
        char       *dst_row = (char       *)dst + y * dst_row_stride;

        for (size_t x = 0; x < width; x++) {
            /* Load 3-channel pixel with format conversion */
            alwan_f64 rgb[3];
            alwan__load3_typed(rgb, src_row + x * src_px, src_fmt);

            /* Source EOTF: encoded -> linear */
            alwan_vec3_f64 lin = {{eotf_fn(rgb[0]), eotf_fn(rgb[1]), eotf_fn(rgb[2])}};

            /* Combined matrix: src linear RGB -> dst linear RGB */
            alwan_vec3_f64 dst_lin = alwan_mat3_mulv_f64_v(combined, lin);

            /* Destination OETF: linear -> encoded */
            alwan_f64 out[3] = {
                oetf_fn(dst_lin.v[0]),
                oetf_fn(dst_lin.v[1]),
                oetf_fn(dst_lin.v[2])
            };

            /* Store with format conversion */
            alwan__store3_typed(dst_row + x * dst_px, out, dst_fmt);
        }
    }

    return ALWAN_OK;
}

int alwan_image_convert_rgba_f64(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_alpha_mode alpha_mode, alwan_ctx *ctx) {

    if (!dst || !src || !src_space || !dst_space || width == 0 || height == 0) {
        return ALWAN_E_INVALID;
    }

    /* Resolve 4-channel pixel strides */
    size_t const src_px = alwan__pixel_stride4(src_fmt);
    size_t const dst_px = alwan__pixel_stride4(dst_fmt);
    if (src_px == 0 || dst_px == 0) return ALWAN_E_INVALID;

    /* Element size for alpha channel offset (4th channel at index 3) */
    size_t const src_elem = alwan__fmt_size(src_fmt);
    size_t const dst_elem = alwan__fmt_size(dst_fmt);

    /* Derive conversion matrices */
    alwan_mat3x3_f64 src_to_xyz, xyz_to_src;
    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
    } else {
        int status = alwan_rgb_derive_matrices_f64(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    alwan_mat3x3_f64 dst_to_xyz, xyz_to_dst;
    if (dst_space->has_matrices) {
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f64(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Check if chromatic adaptation is needed */
    alwan_f64 const tol = ALWAN_LITERAL(1e-6);
    alwan_f64 dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_f64 dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_cat = (ALWAN_ABS(dx) > tol || ALWAN_ABS(dy) > tol);

    /* Precompute combined matrix: xyz_to_dst * [cat *] src_to_xyz */
    alwan_mat3x3_f64 combined;
    if (need_cat && ctx) {
        alwan_xyy_f64 src_xyy, dst_xyy;
        alwan_xyz_f64 src_wp, dst_wp;
        src_xyy.x = src_space->white_xy[0];
        src_xyy.y = src_space->white_xy[1];
        src_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz_f64(&src_wp, &src_xyy);

        dst_xyy.x = dst_space->white_xy[0];
        dst_xyy.y = dst_space->white_xy[1];
        dst_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz_f64(&dst_wp, &dst_xyy);

        alwan_mat3x3_f64 cat;
        int status = alwan_cat_matrix_f64(&cat, &src_wp, &dst_wp, ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        alwan_mat3x3_f64 tmp = alwan_mat3_mul_f64_v(cat, src_to_xyz);
        combined = alwan_mat3_mul_f64_v(xyz_to_dst, tmp);
    } else {
        combined = alwan_mat3_mul_f64_v(xyz_to_dst, src_to_xyz);
    }

    /* Resolve transfer function pointers */
    alwan_tf_fn_f64 eotf_fn = alwan__resolve_eotf_f64(src_space->eotf);
    alwan_tf_fn_f64 oetf_fn = alwan__resolve_oetf_f64(dst_space->oetf);
    if (!eotf_fn || !oetf_fn) return ALWAN_E_INVALID;

    int const premul = (alpha_mode == ALWAN_ALPHA_PREMULTIPLIED);

#if ALWAN_SIMD_WIDTH > 1
    /* SIMD fast path: resolve SIMD transfer function pairs */
    {
        typedef alwan_simd (*simd_tf_fn)(alwan_simd);
        simd_tf_fn eotf_simd = NULL, oetf_simd = NULL;

        switch (src_space->eotf) {
        case ALWAN_TF_SRGB:   eotf_simd = alwan__srgb_eotf_simd;  break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084: eotf_simd = alwan__pq_eotf_simd;    break;
        case ALWAN_TF_HLG:    eotf_simd = alwan__hlg_eotf_full_simd; break;
        case ALWAN_TF_LINEAR: eotf_simd = NULL;                      break;
        default: break;
        }

        switch (dst_space->oetf) {
        case ALWAN_TF_SRGB:   oetf_simd = alwan__srgb_oetf_simd;  break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084: oetf_simd = alwan__pq_oetf_simd;    break;
        case ALWAN_TF_HLG:    oetf_simd = alwan__hlg_oetf_simd;   break;
        case ALWAN_TF_LINEAR: oetf_simd = NULL;                    break;
        default: break;
        }

        if (eotf_simd || src_space->eotf == ALWAN_TF_LINEAR) {
            if (oetf_simd || dst_space->oetf == ALWAN_TF_LINEAR) {
                int const eotf_is_linear = (src_space->eotf == ALWAN_TF_LINEAR);
                int const oetf_is_linear = (dst_space->oetf == ALWAN_TF_LINEAR);
                size_t const W = ALWAN_SIMD_WIDTH;

                for (size_t y = 0; y < height; y++) {
                    char const *src_row = (char const *)src + y * src_row_stride;
                    char       *dst_row = (char       *)dst + y * dst_row_stride;
                    size_t processed = 0;

                    while (processed < width) {
                        size_t tile = width - processed;
                        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
                        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS];
                        ALWAN_ALIGN(32) alwan_simd_lane c2[ALWAN_TILE_PIXELS], c3[ALWAN_TILE_PIXELS];

                        alwan__load_tile_typed_3(c0, c1, c2, src_row, src_fmt, processed, src_px, tile);

                        /* Load alpha (channel 3) */
                        for (size_t j = 0; j < tile; j++) {
                            char const *ap = src_row + (processed + j) * src_px + 3 * src_elem;
                            c3[j] = (alwan_simd_lane)alwan__load1_typed(ap, src_fmt);
                        }

                        /* SIMD: [unpremul] EOTF -> mat3 -> OETF [repremul] */
                        {
                            size_t j = 0;
                            for (; j + W <= tile; j += W) {
                                alwan_simd vr = alwan_simd_load(&c0[j]);
                                alwan_simd vg = alwan_simd_load(&c1[j]);
                                alwan_simd vb = alwan_simd_load(&c2[j]);

                                if (premul) {
                                    alwan_simd va      = alwan_simd_load(&c3[j]);
                                    alwan_simd zero    = alwan_simd_zero();
                                    alwan_simd_mask nz = alwan_simd_cmpgt(va, zero);
                                    alwan_simd sa      = alwan_simd_select(nz, va, alwan_simd_set1((alwan_simd_lane)1.0));
                                    alwan_simd inv_a   = alwan_simd_div(alwan_simd_set1((alwan_simd_lane)1.0), sa);
                                    vr = alwan_simd_mul(vr, inv_a);
                                    vg = alwan_simd_mul(vg, inv_a);
                                    vb = alwan_simd_mul(vb, inv_a);
                                }

                                if (!eotf_is_linear) {
                                    vr = eotf_simd(vr);
                                    vg = eotf_simd(vg);
                                    vb = eotf_simd(vb);
                                }

                                alwan_simd dr, dg, db;
                                alwan__mat3_mul_simd(&dr, &dg, &db, &combined, vr, vg, vb);

                                if (!oetf_is_linear) {
                                    dr = oetf_simd(dr);
                                    dg = oetf_simd(dg);
                                    db = oetf_simd(db);
                                }

                                if (premul) {
                                    alwan_simd va = alwan_simd_load(&c3[j]);
                                    dr = alwan_simd_mul(dr, va);
                                    dg = alwan_simd_mul(dg, va);
                                    db = alwan_simd_mul(db, va);
                                }

                                alwan_simd_store(&c0[j], dr);
                                alwan_simd_store(&c1[j], dg);
                                alwan_simd_store(&c2[j], db);
                            }
                            /* Scalar tail within tile */
                            for (; j < tile; j++) {
                                alwan_f64 r = (alwan_f64)c0[j];
                                alwan_f64 g = (alwan_f64)c1[j];
                                alwan_f64 b = (alwan_f64)c2[j];
                                alwan_f64 a = (alwan_f64)c3[j];
                                if (premul && a > ALWAN_LITERAL(0.0)) {
                                    alwan_f64 inv_a = ALWAN_LITERAL(1.0) / a;
                                    r *= inv_a; g *= inv_a; b *= inv_a;
                                }
                                if (!eotf_is_linear) { r = eotf_fn(r); g = eotf_fn(g); b = eotf_fn(b); }
                                alwan_vec3_f64 lin = {{r, g, b}};
                                alwan_vec3_f64 dst_lin = alwan_mat3_mulv_f64_v(combined, lin);
                                if (!oetf_is_linear) {
                                    c0[j] = (alwan_simd_lane)oetf_fn(dst_lin.v[0]);
                                    c1[j] = (alwan_simd_lane)oetf_fn(dst_lin.v[1]);
                                    c2[j] = (alwan_simd_lane)oetf_fn(dst_lin.v[2]);
                                } else {
                                    c0[j] = (alwan_simd_lane)dst_lin.v[0];
                                    c1[j] = (alwan_simd_lane)dst_lin.v[1];
                                    c2[j] = (alwan_simd_lane)dst_lin.v[2];
                                }
                                if (premul) {
                                    c0[j] *= (alwan_simd_lane)a;
                                    c1[j] *= (alwan_simd_lane)a;
                                    c2[j] *= (alwan_simd_lane)a;
                                }
                            }
                        }

                        alwan__store_tile_typed_3(dst_row, dst_fmt, processed, dst_px, c0, c1, c2, tile);

                        /* Store alpha (channel 3) */
                        for (size_t j = 0; j < tile; j++) {
                            char *ap = dst_row + (processed + j) * dst_px + 3 * dst_elem;
                            alwan__store1_typed(ap, (alwan_f64)c3[j], dst_fmt);
                        }

                        processed += tile;
                    }
                }
                return ALWAN_OK;
            }
        }
    }
#endif /* ALWAN_SIMD_WIDTH > 1 */

    /* Process image row by row */
    for (size_t y = 0; y < height; y++) {
        char const *src_row = (char const *)src + y * src_row_stride;
        char       *dst_row = (char       *)dst + y * dst_row_stride;

        for (size_t x = 0; x < width; x++) {
            char const *src_pixel = src_row + x * src_px;
            char       *dst_pixel = dst_row + x * dst_px;

            /* Load RGB channels (first 3 elements of 4-channel pixel) */
            alwan_f64 rgb[3];
            alwan__load3_typed(rgb, src_pixel, src_fmt);

            /* Load alpha channel (4th element) */
            alwan_f64 alpha = alwan__load1_typed(src_pixel + 3 * src_elem, src_fmt);

            /* Unpremultiply if needed */
            if (premul && alpha > ALWAN_LITERAL(0.0)) {
                alwan_f64 inv_a = ALWAN_LITERAL(1.0) / alpha;
                rgb[0] *= inv_a;
                rgb[1] *= inv_a;
                rgb[2] *= inv_a;
            }

            /* Source EOTF: encoded -> linear */
            alwan_vec3_f64 lin = {{eotf_fn(rgb[0]), eotf_fn(rgb[1]), eotf_fn(rgb[2])}};

            /* Combined matrix: src linear RGB -> dst linear RGB */
            alwan_vec3_f64 dst_lin = alwan_mat3_mulv_f64_v(combined, lin);

            /* Destination OETF: linear -> encoded */
            alwan_f64 out[3] = {
                oetf_fn(dst_lin.v[0]),
                oetf_fn(dst_lin.v[1]),
                oetf_fn(dst_lin.v[2])
            };

            /* Repremultiply if needed */
            if (premul) {
                out[0] *= alpha;
                out[1] *= alpha;
                out[2] *= alpha;
            }

            /* Store RGB + alpha with format conversion */
            alwan__store3_typed(dst_pixel, out, dst_fmt);
            alwan__store1_typed(dst_pixel + 3 * dst_elem, alpha, dst_fmt);
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * f32 wrappers — widen f32 descriptors to f64 and delegate.
 *
 * The image pixel data is in a typed pixel_format and is not affected
 * by the descriptor precision.
 * ---------------------------------------------------------------- */

static void image_map_widen_desc_32(alwan_rgb_space_desc_f64 *out, alwan_rgb_space_desc_f32 const *in) {
    for (int j = 0; j < 6; j++) out->primaries_xy[j] = (double)in->primaries_xy[j];
    out->white_xy[0] = (double)in->white_xy[0];
    out->white_xy[1] = (double)in->white_xy[1];
    out->oetf = in->oetf;
    out->eotf = in->eotf;
    for (int j = 0; j < 9; j++) {
        out->rgb_to_xyz.m[j] = (double)in->rgb_to_xyz.m[j];
        out->xyz_to_rgb.m[j] = (double)in->xyz_to_rgb.m[j];
    }
    out->has_matrices = in->has_matrices;
}

int alwan_image_convert_f32(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_ctx *ctx) {
    if (!src_space || !dst_space) return ALWAN_E_INVALID;
    alwan_rgb_space_desc_f64 s, d;
    image_map_widen_desc_32(&s, src_space);
    image_map_widen_desc_32(&d, dst_space);
    return alwan_image_convert_f64(dst, dst_row_stride, src, src_row_stride, width, height, dst_fmt, src_fmt, &s, &d, ctx);
}

int alwan_image_convert_rgba_f32(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_alpha_mode alpha_mode, alwan_ctx *ctx) {
    if (!src_space || !dst_space) return ALWAN_E_INVALID;
    alwan_rgb_space_desc_f64 s, d;
    image_map_widen_desc_32(&s, src_space);
    image_map_widen_desc_32(&d, dst_space);
    return alwan_image_convert_rgba_f64(dst, dst_row_stride, src, src_row_stride, width, height, dst_fmt, src_fmt, &s, &d, alpha_mode, ctx);
}
