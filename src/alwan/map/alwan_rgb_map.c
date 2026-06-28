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

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static float const D65_WP_Y1_f32[] = {
#include "../data/white_d65_xyz_y1.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
static double const D65_WP_Y1_f64[] = {
#include "../data/white_d65_xyz_y1.csv"
};
ALWAN_DIAG_POP
#endif

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

#if ALWAN_WITH_F32
/* === f32 pass === */
#define ALWAN_MAP_F32
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_rgb_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

#if ALWAN_WITH_F64
/* === f64 pass === */
#define ALWAN_MAP_F64
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_rgb_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

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

/* ----------------------------------------------------------------
 * Image color-space conversion — descriptor-precision templated.
 *
 * The f32 and f64 public entries are PROPER instantiations of
 * alwan_image_convert_impl.inc: each builds its combined matrix natively
 * from its own descriptor precision (the f32 entry does NOT widen the
 * descriptor to f64). Pixel DATA precision is governed by
 * alwan_pixel_format, NOT the f32/f64 suffix — the per-pixel pipeline runs
 * in f64 lanes for both passes.
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "../core/alwan_core_f32_setup.h"
#include "alwan_image_convert_impl.inc"
#include "../core/alwan_core_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "../core/alwan_core_f64_setup.h"
#include "alwan_image_convert_impl.inc"
#include "../core/alwan_core_teardown.h"
#endif
