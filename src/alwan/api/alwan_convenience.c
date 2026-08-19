/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M9: Convenience Color Model Conversions
 * HSV, HSL, CMY, CMYK, YCbCr, YcCbcCrc
 * See alwan_convenience_core.h
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_core.h"
#include "../core/alwan_convenience_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_convenience_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_convenience_impl.inc"
#include "alwan_api_teardown.h"
#endif

/* ----------------------------------------------------------------
 * CMY <-> CMYK
 * ---------------------------------------------------------------- */

int alwan_cmy_to_cmyk_f32(alwan_cmyk_f32 *cmyk_out, alwan_cmy_f32 const *cmy) {
    if (!cmyk_out || !cmy) return ALWAN_E_INVALID;
    *cmyk_out = alwan_cmy_to_cmyk_f32_v(*cmy);
    return ALWAN_OK;
}

int alwan_cmy_to_cmyk_f64(alwan_cmyk_f64 *cmyk_out, alwan_cmy_f64 const *cmy) {
    if (!cmyk_out || !cmy) return ALWAN_E_INVALID;
    *cmyk_out = alwan_cmy_to_cmyk_f64_v(*cmy);
    return ALWAN_OK;
}

int alwan_cmyk_to_cmy_f32(alwan_cmy_f32 *cmy_out, alwan_cmyk_f32 const *cmyk) {
    if (!cmy_out || !cmyk) return ALWAN_E_INVALID;
    *cmy_out = alwan_cmyk_to_cmy_f32_v(*cmyk);
    return ALWAN_OK;
}

int alwan_cmyk_to_cmy_f64(alwan_cmy_f64 *cmy_out, alwan_cmyk_f64 const *cmyk) {
    if (!cmy_out || !cmyk) return ALWAN_E_INVALID;
    *cmy_out = alwan_cmyk_to_cmy_f64_v(*cmyk);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YCbCr
 * The .c wrapper resolves the enum to kr/kb.
 * ---------------------------------------------------------------- */

/* YCbCr coefficients resolved via alwan__get_ycbcr_coeffs() in alwan_internal.h */

int alwan_rgb_to_ycbcr_f64(alwan_ycbcr_f64 *ycbcr_out, alwan_rgb_f64 const *rgb, alwan_ycbcr_standard standard) {
    if (!rgb || !ycbcr_out) return ALWAN_E_INVALID;
    alwan_f64 kr, kb;
    alwan__get_ycbcr_coeffs(standard, &kr, &kb);
    *ycbcr_out = alwan_rgb_to_ycbcr_kr_kb_f64_v(*rgb, kr, kb);
    ALWAN_NORM_YCBCR(ycbcr_out);
    return ALWAN_OK;
}

int alwan_rgb_to_ycbcr_f32(alwan_ycbcr_f32 *ycbcr_out, alwan_rgb_f32 const *rgb, alwan_ycbcr_standard standard) {
    if (!rgb || !ycbcr_out) return ALWAN_E_INVALID;
    alwan_f64 kr, kb;
    alwan__get_ycbcr_coeffs(standard, &kr, &kb);
    alwan_rgb_f32 rgb_f32 = *rgb;
    *ycbcr_out = alwan_rgb_to_ycbcr_kr_kb_f32_v(rgb_f32, (alwan_f32)kr, (alwan_f32)kb);
    ALWAN_NORM_YCBCR(ycbcr_out);
    return ALWAN_OK;
}

int alwan_ycbcr_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_ycbcr_f64 const *ycbcr, alwan_ycbcr_standard standard) {
    if (!ycbcr || !rgb_out) return ALWAN_E_INVALID;
    alwan_ycbcr_f64 tmp = *ycbcr;
    ALWAN_DENORM_YCBCR(&tmp);
    alwan_f64 kr, kb;
    alwan__get_ycbcr_coeffs(standard, &kr, &kb);
    *rgb_out = alwan_ycbcr_to_rgb_kr_kb_f64_v(tmp, kr, kb);
    return ALWAN_OK;
}

int alwan_ycbcr_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_ycbcr_f32 const *ycbcr, alwan_ycbcr_standard standard) {
    if (!ycbcr || !rgb_out) return ALWAN_E_INVALID;
    alwan_ycbcr_f32 tmp = *ycbcr;
    ALWAN_DENORM_YCBCR(&tmp);
    alwan_f64 kr, kb;
    alwan__get_ycbcr_coeffs(standard, &kr, &kb);
    *rgb_out = alwan_ycbcr_to_rgb_kr_kb_f32_v(tmp, (alwan_f32)kr, (alwan_f32)kb);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HWB
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hwb_f32(alwan_hwb_f32 *hwb_out, alwan_rgb_f32 const *rgb) {
    if (!rgb || !hwb_out) return ALWAN_E_INVALID;
    *hwb_out = alwan_rgb_to_hwb_f32_v(*rgb);
    return ALWAN_OK;
}

int alwan_rgb_to_hwb_f64(alwan_hwb_f64 *hwb_out, alwan_rgb_f64 const *rgb) {
    if (!rgb || !hwb_out) return ALWAN_E_INVALID;
    *hwb_out = alwan_rgb_to_hwb_f64_v(*rgb);
    return ALWAN_OK;
}

int alwan_hwb_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hwb_f32 const *hwb) {
    if (!rgb_out || !hwb) return ALWAN_E_INVALID;
    *rgb_out = alwan_hwb_to_rgb_f32_v(*hwb);
    return ALWAN_OK;
}

int alwan_hwb_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hwb_f64 const *hwb) {
    if (!rgb_out || !hwb) return ALWAN_E_INVALID;
    *rgb_out = alwan_hwb_to_rgb_f64_v(*hwb);
    return ALWAN_OK;
}

int alwan_hsv_to_hwb_f32(alwan_hwb_f32 *hwb_out, alwan_hsv_f32 const *hsv) {
    if (!hsv || !hwb_out) return ALWAN_E_INVALID;
    *hwb_out = alwan_hsv_to_hwb_f32_v(*hsv);
    return ALWAN_OK;
}

int alwan_hsv_to_hwb_f64(alwan_hwb_f64 *hwb_out, alwan_hsv_f64 const *hsv) {
    if (!hsv || !hwb_out) return ALWAN_E_INVALID;
    *hwb_out = alwan_hsv_to_hwb_f64_v(*hsv);
    return ALWAN_OK;
}

int alwan_hwb_to_hsv_f32(alwan_hsv_f32 *hsv_out, alwan_hwb_f32 const *hwb) {
    if (!hsv_out || !hwb) return ALWAN_E_INVALID;
    *hsv_out = alwan_hwb_to_hsv_f32_v(*hwb);
    return ALWAN_OK;
}

int alwan_hwb_to_hsv_f64(alwan_hsv_f64 *hsv_out, alwan_hwb_f64 const *hwb) {
    if (!hsv_out || !hwb) return ALWAN_E_INVALID;
    *hsv_out = alwan_hwb_to_hsv_f64_v(*hwb);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Relative Luminance
 * ---------------------------------------------------------------- */

int alwan_relative_luminance_f64(alwan_f64 *Y_out,
                             alwan_rgb_f64 const *rgb,
                             alwan_luma_standard standard) {
    if (!rgb || !Y_out) return ALWAN_E_INVALID;
    alwan_f64 kr, kg, kb;
    alwan__get_luma_coeffs(standard, &kr, &kg, &kb);
    *Y_out = alwan_relative_luminance_f64_v(*rgb, kr, kg, kb);
    return ALWAN_OK;
}

int alwan_relative_luminance_f32(alwan_f32 *Y_out,
                             alwan_rgb_f32 const *rgb,
                             alwan_luma_standard standard) {
    if (!rgb || !Y_out) return ALWAN_E_INVALID;
    alwan_f64 kr, kg, kb;
    alwan__get_luma_coeffs(standard, &kr, &kg, &kb);
    *Y_out = alwan_relative_luminance_f32_v(*rgb, (alwan_f32)kr, (alwan_f32)kg, (alwan_f32)kb);
    return ALWAN_OK;
}

int alwan_relative_luminance_kr_kb_f64(alwan_f64 *Y_out,
                                   alwan_rgb_f64 const *rgb,
                                   alwan_f64 kr, alwan_f64 kb) {
    if (!rgb || !Y_out) return ALWAN_E_INVALID;
    alwan_f64 kg = ALWAN_LITERAL(1.0) - kr - kb;
    *Y_out = alwan_relative_luminance_f64_v(*rgb, kr, kg, kb);
    return ALWAN_OK;
}

int alwan_relative_luminance_kr_kb_f32(alwan_f32 *Y_out,
                                   alwan_rgb_f32 const *rgb,
                                   alwan_f32 kr, alwan_f32 kb) {
    if (!rgb || !Y_out) return ALWAN_E_INVALID;
    alwan_f32 kg = 1.0f - kr - kb;
    *Y_out = alwan_relative_luminance_f32_v(*rgb, kr, kg, kb);
    return ALWAN_OK;
}

int alwan_relative_luminance_space_f64(alwan_f64 *Y_out,
                                   alwan_rgb_f64 const *rgb,
                                   alwan_rgb_space_desc_f64 const *space) {
    if (!rgb || !Y_out || !space) return ALWAN_E_INVALID;
    if (!space->has_matrices) return ALWAN_E_INVALID;
    /* Y row of the RGB-to-XYZ NPM (row-major: m[3], m[4], m[5]) */
    alwan_f64 kr = space->rgb_to_xyz.m[3];
    alwan_f64 kg = space->rgb_to_xyz.m[4];
    alwan_f64 kb = space->rgb_to_xyz.m[5];
    *Y_out = alwan_relative_luminance_f64_v(*rgb, kr, kg, kb);
    return ALWAN_OK;
}

int alwan_relative_luminance_space_f32(alwan_f32 *Y_out,
                                   alwan_rgb_f32 const *rgb,
                                   alwan_rgb_space_desc_f32 const *space) {
    if (!rgb || !Y_out || !space) return ALWAN_E_INVALID;
    if (!space->has_matrices) return ALWAN_E_INVALID;
    /* Y row of the RGB-to-XYZ NPM (row-major: m[3], m[4], m[5]) */
    alwan_f32 kr = space->rgb_to_xyz.m[3];
    alwan_f32 kg = space->rgb_to_xyz.m[4];
    alwan_f32 kb = space->rgb_to_xyz.m[5];
    *Y_out = alwan_relative_luminance_f32_v(*rgb, kr, kg, kb);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Gamut-safe decode variants
 *
 * The raw alwan_ycbcr_to_rgb / alwan_yccbccrc_to_rgb decoders perform the
 * pure standard math and preserve out-of-range excursions (super-black /
 * super-white, xvYCC). These variants run the raw decode and then map the
 * result into the valid gamut with the requested method -- output is
 * guaranteed in [0,1]. ALWAN_GAMUT_MAP_CLIP reproduces the old implicit
 * clamping behaviour exactly.
 * ---------------------------------------------------------------- */

static alwan_rgb_space alwan__ycbcr_space_id(alwan_ycbcr_standard standard) {
    return standard == ALWAN_YCBCR_BT2020 ? ALWAN_RGB_SPACE_BT2020 : ALWAN_RGB_SPACE_SRGB;
}

#if ALWAN_WITH_F64
static int alwan__gamut_fixup_f64(alwan_rgb_f64 *rgb, alwan_rgb_space space_id, alwan_gamut_map_method method) {
    alwan_rgb_space_desc_f64 space;
    int st = alwan_rgb_get_space_descriptor_f64(&space, space_id, NULL);
    if (st != ALWAN_OK) return st;
    alwan_rgb_f64 raw = *rgb;
    return alwan_gamut_map_advanced_f64(rgb, method, &space, &raw);
}

int alwan_ycbcr_to_rgb_gamut_safe_f64(alwan_rgb_f64 *rgb_out, alwan_ycbcr_f64 const *ycbcr,
                                      alwan_ycbcr_standard standard, alwan_gamut_map_method method) {
    int st = alwan_ycbcr_to_rgb_f64(rgb_out, ycbcr, standard);
    if (st != ALWAN_OK) return st;
    return alwan__gamut_fixup_f64(rgb_out, alwan__ycbcr_space_id(standard), method);
}

int alwan_yccbccrc_to_rgb_gamut_safe_f64(alwan_rgb_f64 *rgb_out, alwan_yccbccrc_f64 const *yccbccrc,
                                         int bit_depth, alwan_gamut_map_method method) {
    int st = alwan_yccbccrc_to_rgb_f64(rgb_out, yccbccrc, bit_depth);
    if (st != ALWAN_OK) return st;
    return alwan__gamut_fixup_f64(rgb_out, ALWAN_RGB_SPACE_BT2020, method);
}

int alwan_ycbcr_to_rgb_gamut_safe_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride,
        alwan_f64 const *ycbcr_in, size_t in_stride, size_t count,
        alwan_ycbcr_standard standard, alwan_gamut_map_method method) {
    int st = alwan_ycbcr_to_rgb_f64_map_interleave(rgb_out, out_stride, ycbcr_in, in_stride, count, standard);
    if (st != ALWAN_OK) return st;
    alwan_rgb_space_desc_f64 space;
    st = alwan_rgb_get_space_descriptor_f64(&space, alwan__ycbcr_space_id(standard), NULL);
    if (st != ALWAN_OK) return st;
    for (size_t i = 0; i < count; i++) {
        alwan_rgb_f64 *px = (alwan_rgb_f64 *)((char *)rgb_out + i * out_stride);
        alwan_rgb_f64 raw = *px;
        st = alwan_gamut_map_advanced_f64(px, method, &space, &raw);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

int alwan_yccbccrc_to_rgb_gamut_safe_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride,
        alwan_f64 const *yccbccrc_in, size_t in_stride, size_t count,
        int bit_depth, alwan_gamut_map_method method) {
    int st = alwan_yccbccrc_to_rgb_f64_map_interleave(rgb_out, out_stride, yccbccrc_in, in_stride, count, bit_depth);
    if (st != ALWAN_OK) return st;
    alwan_rgb_space_desc_f64 space;
    st = alwan_rgb_get_space_descriptor_f64(&space, ALWAN_RGB_SPACE_BT2020, NULL);
    if (st != ALWAN_OK) return st;
    for (size_t i = 0; i < count; i++) {
        alwan_rgb_f64 *px = (alwan_rgb_f64 *)((char *)rgb_out + i * out_stride);
        alwan_rgb_f64 raw = *px;
        st = alwan_gamut_map_advanced_f64(px, method, &space, &raw);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
static int alwan__gamut_fixup_f32(alwan_rgb_f32 *rgb, alwan_rgb_space space_id, alwan_gamut_map_method method) {
    alwan_rgb_space_desc_f32 space;
    int st = alwan_rgb_get_space_descriptor_f32(&space, space_id, NULL);
    if (st != ALWAN_OK) return st;
    alwan_rgb_f32 raw = *rgb;
    return alwan_gamut_map_advanced_f32(rgb, method, &space, &raw);
}

int alwan_ycbcr_to_rgb_gamut_safe_f32(alwan_rgb_f32 *rgb_out, alwan_ycbcr_f32 const *ycbcr,
                                      alwan_ycbcr_standard standard, alwan_gamut_map_method method) {
    int st = alwan_ycbcr_to_rgb_f32(rgb_out, ycbcr, standard);
    if (st != ALWAN_OK) return st;
    return alwan__gamut_fixup_f32(rgb_out, alwan__ycbcr_space_id(standard), method);
}

int alwan_yccbccrc_to_rgb_gamut_safe_f32(alwan_rgb_f32 *rgb_out, alwan_yccbccrc_f32 const *yccbccrc,
                                         int bit_depth, alwan_gamut_map_method method) {
    int st = alwan_yccbccrc_to_rgb_f32(rgb_out, yccbccrc, bit_depth);
    if (st != ALWAN_OK) return st;
    return alwan__gamut_fixup_f32(rgb_out, ALWAN_RGB_SPACE_BT2020, method);
}

int alwan_ycbcr_to_rgb_gamut_safe_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride,
        alwan_f32 const *ycbcr_in, size_t in_stride, size_t count,
        alwan_ycbcr_standard standard, alwan_gamut_map_method method) {
    int st = alwan_ycbcr_to_rgb_f32_map_interleave(rgb_out, out_stride, ycbcr_in, in_stride, count, standard);
    if (st != ALWAN_OK) return st;
    alwan_rgb_space_desc_f32 space;
    st = alwan_rgb_get_space_descriptor_f32(&space, alwan__ycbcr_space_id(standard), NULL);
    if (st != ALWAN_OK) return st;
    for (size_t i = 0; i < count; i++) {
        alwan_rgb_f32 *px = (alwan_rgb_f32 *)((char *)rgb_out + i * out_stride);
        alwan_rgb_f32 raw = *px;
        st = alwan_gamut_map_advanced_f32(px, method, &space, &raw);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

int alwan_yccbccrc_to_rgb_gamut_safe_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride,
        alwan_f32 const *yccbccrc_in, size_t in_stride, size_t count,
        int bit_depth, alwan_gamut_map_method method) {
    int st = alwan_yccbccrc_to_rgb_f32_map_interleave(rgb_out, out_stride, yccbccrc_in, in_stride, count, bit_depth);
    if (st != ALWAN_OK) return st;
    alwan_rgb_space_desc_f32 space;
    st = alwan_rgb_get_space_descriptor_f32(&space, ALWAN_RGB_SPACE_BT2020, NULL);
    if (st != ALWAN_OK) return st;
    for (size_t i = 0; i < count; i++) {
        alwan_rgb_f32 *px = (alwan_rgb_f32 *)((char *)rgb_out + i * out_stride);
        alwan_rgb_f32 raw = *px;
        st = alwan_gamut_map_advanced_f32(px, method, &space, &raw);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */
