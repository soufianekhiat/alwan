/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Typed Planar Map Functions (_map_planar_ex variants)
 * Accept void* channel buffers with alwan_pixel_format for u8/u16/f16/f32/f64 I/O
 *
 * Functions with native SIMD planar counterparts delegate via tiled typed
 * load/store.  Others use per-pixel macros (still F16-aware).
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_convenience_core.h"
#include "../core/alwan_extended_core.h"
#include "../core/alwan_din99_core.h"
#include "../core/alwan_hunter_lab_core.h"
#include "../core/alwan_prolab_core.h"
#include "../core/alwan_osa_ucs_core.h"
#include "../core/alwan_colorspace_core.h"
#include "../core/alwan_color_correction_core.h"
#include "../core/alwan_vision_core.h"

/* ----------------------------------------------------------------
 * sRGB Convenience planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_srgb_to_xyz_map_planar_ex,   alwan_srgb_to_xyz_f32_map_planar,   alwan_srgb_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_srgb_map_planar_ex,   alwan_xyz_to_srgb_f32_map_planar,   alwan_xyz_to_srgb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_srgb_to_lab_map_planar_ex,   alwan_srgb_to_lab_f32_map_planar,   alwan_srgb_to_lab_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_lab_to_srgb_map_planar_ex,   alwan_lab_to_srgb_f32_map_planar,   alwan_lab_to_srgb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_srgb_to_oklab_map_planar_ex, alwan_srgb_to_oklab_f32_map_planar, alwan_srgb_to_oklab_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_oklab_to_srgb_map_planar_ex, alwan_oklab_to_srgb_f32_map_planar, alwan_oklab_to_srgb_f64_map_planar)

/* ----------------------------------------------------------------
 * Colorspace planar _ex (with white_xyz -- delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(alwan_xyz_to_lab_map_planar_ex, alwan_xyz_to_lab_f32_map_planar, alwan_xyz_to_lab_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(alwan_lab_to_xyz_map_planar_ex, alwan_lab_to_xyz_f32_map_planar, alwan_lab_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(alwan_xyz_to_luv_map_planar_ex, alwan_xyz_to_luv_f32_map_planar, alwan_xyz_to_luv_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(alwan_luv_to_xyz_map_planar_ex, alwan_luv_to_xyz_f32_map_planar, alwan_luv_to_xyz_f64_map_planar)

/* Colorspace planar _ex (simple 3->3 -- delegate to SIMD planar) */

ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_lab_to_lch_map_planar_ex,    alwan_lab_to_lch_f32_map_planar,    alwan_lab_to_lch_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_lch_to_lab_map_planar_ex,    alwan_lch_to_lab_f32_map_planar,    alwan_lch_to_lab_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_luv_to_lchuv_map_planar_ex,  alwan_luv_to_lchuv_f32_map_planar,  alwan_luv_to_lchuv_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_lchuv_to_luv_map_planar_ex,  alwan_lchuv_to_luv_f32_map_planar,  alwan_lchuv_to_luv_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_xyy_map_planar_ex,    alwan_xyz_to_xyy_f32_map_planar,    alwan_xyz_to_xyy_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyy_to_xyz_map_planar_ex,    alwan_xyy_to_xyz_f32_map_planar,    alwan_xyy_to_xyz_f64_map_planar)

/* ----------------------------------------------------------------
 * Oklab planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_oklab_map_planar_ex,   alwan_xyz_to_oklab_f32_map_planar,   alwan_xyz_to_oklab_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_oklab_to_xyz_map_planar_ex,   alwan_oklab_to_xyz_f32_map_planar,   alwan_oklab_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_oklab_to_oklch_map_planar_ex, alwan_oklab_to_oklch_f32_map_planar, alwan_oklab_to_oklch_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_oklch_to_oklab_map_planar_ex, alwan_oklch_to_oklab_f32_map_planar, alwan_oklch_to_oklab_f64_map_planar)

/* ----------------------------------------------------------------
 * ICtCp planar _ex (with use_pq -- delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(alwan_rgb_to_ictcp_map_planar_ex,  alwan_rgb_to_ictcp_f32_map_planar,  alwan_rgb_to_ictcp_f64_map_planar,  int, use_pq)
ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(alwan_ictcp_to_rgb_map_planar_ex,  alwan_ictcp_to_rgb_f32_map_planar,  alwan_ictcp_to_rgb_f64_map_planar,  int, use_pq)
ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(alwan_xyz_to_ictcp_map_planar_ex,  alwan_xyz_to_ictcp_f32_map_planar,  alwan_xyz_to_ictcp_f64_map_planar,  int, use_pq)
ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(alwan_ictcp_to_xyz_map_planar_ex,  alwan_ictcp_to_xyz_f32_map_planar,  alwan_ictcp_to_xyz_f64_map_planar,  int, use_pq)

/* ----------------------------------------------------------------
 * JzAzBz planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_jzazbz_map_planar_ex,    alwan_xyz_to_jzazbz_f32_map_planar,    alwan_xyz_to_jzazbz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_jzazbz_to_xyz_map_planar_ex,    alwan_jzazbz_to_xyz_f32_map_planar,    alwan_jzazbz_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_jzazbz_to_jzczhz_map_planar_ex, alwan_jzazbz_to_jzczhz_f32_map_planar, alwan_jzazbz_to_jzczhz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_jzczhz_to_jzazbz_map_planar_ex, alwan_jzczhz_to_jzazbz_f32_map_planar, alwan_jzczhz_to_jzazbz_f64_map_planar)

/* ----------------------------------------------------------------
 * IPT planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_ipt_map_planar_ex, alwan_xyz_to_ipt_f32_map_planar, alwan_xyz_to_ipt_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_ipt_to_xyz_map_planar_ex, alwan_ipt_to_xyz_f32_map_planar, alwan_ipt_to_xyz_f64_map_planar)

/* ----------------------------------------------------------------
 * HSV/HSL planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_rgb_to_hsv_map_planar_ex, alwan_rgb_to_hsv_f32_map_planar, alwan_rgb_to_hsv_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hsv_to_rgb_map_planar_ex, alwan_hsv_to_rgb_f32_map_planar, alwan_hsv_to_rgb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_rgb_to_hsl_map_planar_ex, alwan_rgb_to_hsl_f32_map_planar, alwan_rgb_to_hsl_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hsl_to_rgb_map_planar_ex, alwan_hsl_to_rgb_f32_map_planar, alwan_hsl_to_rgb_f64_map_planar)

/* ----------------------------------------------------------------
 * CMY/YCoCg/HWB planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_rgb_to_cmy_map_planar_ex,   alwan_rgb_to_cmy_f32_map_planar,   alwan_rgb_to_cmy_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_cmy_to_rgb_map_planar_ex,   alwan_cmy_to_rgb_f32_map_planar,   alwan_cmy_to_rgb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_rgb_to_ycocg_map_planar_ex, alwan_rgb_to_ycocg_f32_map_planar, alwan_rgb_to_ycocg_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_ycocg_to_rgb_map_planar_ex, alwan_ycocg_to_rgb_f32_map_planar, alwan_ycocg_to_rgb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_rgb_to_hwb_map_planar_ex,   alwan_rgb_to_hwb_f32_map_planar,   alwan_rgb_to_hwb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hwb_to_rgb_map_planar_ex,   alwan_hwb_to_rgb_f32_map_planar,   alwan_hwb_to_rgb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hsv_to_hwb_map_planar_ex,   alwan_hsv_to_hwb_f32_map_planar,   alwan_hsv_to_hwb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hwb_to_hsv_map_planar_ex,   alwan_hwb_to_hsv_f32_map_planar,   alwan_hwb_to_hsv_f64_map_planar)

/* ----------------------------------------------------------------
 * HSP/HSPlog/HSY planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_rgb_to_hsp_map_planar_ex,    alwan_rgb_to_hsp_f32_map_planar,    alwan_rgb_to_hsp_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hsp_to_rgb_map_planar_ex,    alwan_hsp_to_rgb_f32_map_planar,    alwan_hsp_to_rgb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_rgb_to_hsplog_map_planar_ex, alwan_rgb_to_hsplog_f32_map_planar, alwan_rgb_to_hsplog_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hsplog_to_rgb_map_planar_ex, alwan_hsplog_to_rgb_f32_map_planar, alwan_hsplog_to_rgb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_rgb_to_hsy_map_planar_ex,    alwan_rgb_to_hsy_f32_map_planar,    alwan_rgb_to_hsy_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hsy_to_rgb_map_planar_ex,    alwan_hsy_to_rgb_f32_map_planar,    alwan_hsy_to_rgb_f64_map_planar)

/* ----------------------------------------------------------------
 * Linear sRGB <-> HSV/HSL planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_linear_srgb_to_hsv_map_planar_ex, alwan_linear_srgb_to_hsv_f32_map_planar, alwan_linear_srgb_to_hsv_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hsv_to_linear_srgb_map_planar_ex, alwan_hsv_to_linear_srgb_f32_map_planar, alwan_hsv_to_linear_srgb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_linear_srgb_to_hsl_map_planar_ex, alwan_linear_srgb_to_hsl_f32_map_planar, alwan_linear_srgb_to_hsl_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hsl_to_linear_srgb_map_planar_ex, alwan_hsl_to_linear_srgb_f32_map_planar, alwan_hsl_to_linear_srgb_f64_map_planar)

/* ----------------------------------------------------------------
 * YCbCr planar _ex (delegate to native planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(alwan_rgb_to_ycbcr_map_planar_ex,  alwan_rgb_to_ycbcr_f32_map_planar,  alwan_rgb_to_ycbcr_f64_map_planar,  alwan_ycbcr_standard, standard)
ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(alwan_ycbcr_to_rgb_map_planar_ex,  alwan_ycbcr_to_rgb_f32_map_planar,  alwan_ycbcr_to_rgb_f64_map_planar,  alwan_ycbcr_standard, standard)

/* YcCbcCrc / legal-full planar _ex (delegate to SIMD planar) */

ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(alwan_rgb_to_yccbccrc_map_planar_ex,      alwan_rgb_to_yccbccrc_f32_map_planar,      alwan_rgb_to_yccbccrc_f64_map_planar,      int, bit_depth)
ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(alwan_yccbccrc_to_rgb_map_planar_ex,      alwan_yccbccrc_to_rgb_f32_map_planar,      alwan_yccbccrc_to_rgb_f64_map_planar,      int, bit_depth)
ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(alwan_ycbcr_full_to_legal_map_planar_ex,  alwan_ycbcr_full_to_legal_f32_map_planar,  alwan_ycbcr_full_to_legal_f64_map_planar,  int, bit_depth)
ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(alwan_ycbcr_legal_to_full_map_planar_ex,  alwan_ycbcr_legal_to_full_f32_map_planar,  alwan_ycbcr_legal_to_full_f64_map_planar,  int, bit_depth)

/* ----------------------------------------------------------------
 * Extended color spaces planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_igpgtg_map_planar_ex,     alwan_xyz_to_igpgtg_f32_map_planar,     alwan_xyz_to_igpgtg_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_igpgtg_to_xyz_map_planar_ex,     alwan_igpgtg_to_xyz_f32_map_planar,     alwan_igpgtg_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_icacb_map_planar_ex,      alwan_xyz_to_icacb_f32_map_planar,      alwan_xyz_to_icacb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_icacb_to_xyz_map_planar_ex,      alwan_icacb_to_xyz_f32_map_planar,      alwan_icacb_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_hdr_cielab_map_planar_ex, alwan_xyz_to_hdr_cielab_f32_map_planar, alwan_xyz_to_hdr_cielab_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hdr_cielab_to_xyz_map_planar_ex, alwan_hdr_cielab_to_xyz_f32_map_planar, alwan_hdr_cielab_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_hdr_ipt_map_planar_ex,    alwan_xyz_to_hdr_ipt_f32_map_planar,    alwan_xyz_to_hdr_ipt_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hdr_ipt_to_xyz_map_planar_ex,    alwan_hdr_ipt_to_xyz_f32_map_planar,    alwan_hdr_ipt_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_ucs_map_planar_ex,        alwan_xyz_to_ucs_f32_map_planar,        alwan_xyz_to_ucs_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_ucs_to_xyz_map_planar_ex,        alwan_ucs_to_xyz_f32_map_planar,        alwan_ucs_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_osa_ucs_map_planar_ex,    alwan_xyz_to_osa_ucs_f32_map_planar,    alwan_xyz_to_osa_ucs_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_osa_ucs_to_xyz_map_planar_ex,    alwan_osa_ucs_to_xyz_f32_map_planar,    alwan_osa_ucs_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_hunter_lab_map_planar_ex,  alwan_xyz_to_hunter_lab_f32_map_planar,  alwan_xyz_to_hunter_lab_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hunter_lab_to_xyz_map_planar_ex,  alwan_hunter_lab_to_xyz_f32_map_planar,  alwan_hunter_lab_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_xyz_to_prolab_map_planar_ex,      alwan_xyz_to_prolab_f32_map_planar,      alwan_xyz_to_prolab_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_prolab_to_xyz_map_planar_ex,      alwan_prolab_to_xyz_f32_map_planar,      alwan_prolab_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_rgb_to_prismatic_map_planar_ex,   alwan_rgb_to_prismatic_f32_map_planar,   alwan_rgb_to_prismatic_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_prismatic_to_rgb_map_planar_ex,   alwan_prismatic_to_rgb_f32_map_planar,   alwan_prismatic_to_rgb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_rgb_to_hcl_map_planar_ex,         alwan_rgb_to_hcl_f32_map_planar,         alwan_rgb_to_hcl_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_hcl_to_rgb_map_planar_ex,         alwan_hcl_to_rgb_f32_map_planar,         alwan_hcl_to_rgb_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_rgb_to_ihls_map_planar_ex,        alwan_rgb_to_ihls_f32_map_planar,        alwan_rgb_to_ihls_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_ihls_to_rgb_map_planar_ex,        alwan_ihls_to_rgb_f32_map_planar,        alwan_ihls_to_rgb_f64_map_planar)

/* Extended with white point planar _ex (delegate to SIMD planar) */

ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(alwan_xyz_to_uvw_map_planar_ex,              alwan_xyz_to_uvw_f32_map_planar,              alwan_xyz_to_uvw_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(alwan_uvw_to_xyz_map_planar_ex,              alwan_uvw_to_xyz_f32_map_planar,              alwan_uvw_to_xyz_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(alwan_xyz_to_hunter_lab_custom_map_planar_ex, alwan_xyz_to_hunter_lab_custom_f32_map_planar, alwan_xyz_to_hunter_lab_custom_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(alwan_hunter_lab_to_xyz_custom_map_planar_ex, alwan_hunter_lab_to_xyz_custom_f32_map_planar, alwan_hunter_lab_to_xyz_custom_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(alwan_xyz_to_prolab_custom_map_planar_ex,     alwan_xyz_to_prolab_custom_f32_map_planar,     alwan_xyz_to_prolab_custom_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(alwan_prolab_to_xyz_custom_map_planar_ex,     alwan_prolab_to_xyz_custom_f32_map_planar,     alwan_prolab_to_xyz_custom_f64_map_planar)

/* DIN99 planar _ex (delegate to SIMD planar) */

ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(alwan_lab_to_din99_map_planar_ex, alwan_lab_to_din99_f32_map_planar, alwan_lab_to_din99_f64_map_planar, int, variant)
ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(alwan_din99_to_lab_map_planar_ex, alwan_din99_to_lab_f32_map_planar, alwan_din99_to_lab_f64_map_planar, int, variant)

/* ----------------------------------------------------------------
 * CVD planar _ex (delegate to SIMD planar where possible)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_DUAL_SCALAR(alwan_simulate_protanopia_map_planar_ex,   alwan_simulate_protanopia_f32_map_planar,   alwan_simulate_protanopia_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL_SCALAR(alwan_simulate_deuteranopia_map_planar_ex, alwan_simulate_deuteranopia_f32_map_planar, alwan_simulate_deuteranopia_f64_map_planar)
ALWAN_PLANAR_EX_DELEGATE_DUAL_SCALAR(alwan_simulate_tritanopia_map_planar_ex,   alwan_simulate_tritanopia_f32_map_planar,   alwan_simulate_tritanopia_f64_map_planar)

alwan_status alwan_simulate_cvd_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_cvd_type cvd_type, alwan_f64 severity) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
#if ALWAN_WITH_BOTH
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_simulate_cvd_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, cvd_type, (double)severity);
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    } else {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_simulate_cvd_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, cvd_type, (float)severity);
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#elif ALWAN_WITH_F64
    {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_simulate_cvd_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, cvd_type, (double)severity);
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#else
    {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_simulate_cvd_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, cvd_type, (float)severity);
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Color correction planar _ex (tiled delegation to native planar)
 * ---------------------------------------------------------------- */

alwan_status alwan_lgg_apply_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_rgb_f64 const *lift, alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !lift || !gamma || !gain || count == 0)
        return ALWAN_E_INVALID;
#if ALWAN_WITH_BOTH
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        alwan_rgb_f64 lift64 = {(double)lift->r, (double)lift->g, (double)lift->b};
        alwan_rgb_f64 gamma64 = {(double)gamma->r, (double)gamma->g, (double)gamma->b};
        alwan_rgb_f64 gain64 = {(double)gain->r, (double)gain->g, (double)gain->b};
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_lgg_apply_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, &lift64, &gamma64, &gain64);
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    } else {
        alwan_rgb_f32 lift32 = {(float)lift->r, (float)lift->g, (float)lift->b};
        alwan_rgb_f32 gamma32 = {(float)gamma->r, (float)gamma->g, (float)gamma->b};
        alwan_rgb_f32 gain32 = {(float)gain->r, (float)gain->g, (float)gain->b};
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_lgg_apply_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, &lift32, &gamma32, &gain32);
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#elif ALWAN_WITH_F64
    {
        alwan_rgb_f64 lift64 = {(double)lift->r, (double)lift->g, (double)lift->b};
        alwan_rgb_f64 gamma64 = {(double)gamma->r, (double)gamma->g, (double)gamma->b};
        alwan_rgb_f64 gain64 = {(double)gain->r, (double)gain->g, (double)gain->b};
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_lgg_apply_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, &lift64, &gamma64, &gain64);
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#else
    {
        alwan_rgb_f32 lift32 = {(float)lift->r, (float)lift->g, (float)lift->b};
        alwan_rgb_f32 gamma32 = {(float)gamma->r, (float)gamma->g, (float)gamma->b};
        alwan_rgb_f32 gain32 = {(float)gain->r, (float)gain->g, (float)gain->b};
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_lgg_apply_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, &lift32, &gamma32, &gain32);
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#endif
    return ALWAN_OK;
}

alwan_status alwan_color_matrix_apply_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_mat3x3_f64 const *matrix) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !matrix || count == 0) return ALWAN_E_INVALID;
#if ALWAN_WITH_BOTH
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        alwan_mat3x3_f64 m64; for (int i = 0; i < 9; i++) m64.m[i] = (double)matrix->m[i];
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_color_matrix_apply_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, &m64);
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    } else {
        alwan_mat3x3_f32 m32; for (int i = 0; i < 9; i++) m32.m[i] = (float)matrix->m[i];
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_color_matrix_apply_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, &m32);
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#elif ALWAN_WITH_F64
    {
        alwan_mat3x3_f64 m64; for (int i = 0; i < 9; i++) m64.m[i] = (double)matrix->m[i];
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_color_matrix_apply_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, &m64);
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#else
    {
        alwan_mat3x3_f32 m32; for (int i = 0; i < 9; i++) m32.m[i] = (float)matrix->m[i];
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_color_matrix_apply_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, &m32);
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#endif
    return ALWAN_OK;
}

alwan_status alwan_printer_lights_apply_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 red_lights, alwan_f64 green_lights, alwan_f64 blue_lights) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
#if ALWAN_WITH_BOTH
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_printer_lights_apply_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, (double)red_lights, (double)green_lights, (double)blue_lights);
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    } else {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_printer_lights_apply_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, (float)red_lights, (float)green_lights, (float)blue_lights);
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#elif ALWAN_WITH_F64
    {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_printer_lights_apply_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, (double)red_lights, (double)green_lights, (double)blue_lights);
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#else
    {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_printer_lights_apply_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, (float)red_lights, (float)green_lights, (float)blue_lights);
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#endif
    return ALWAN_OK;
}

alwan_status alwan_white_balance_apply_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_rgb_f64 const *multipliers) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !multipliers || count == 0) return ALWAN_E_INVALID;
#if ALWAN_WITH_BOTH
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        alwan_rgb_f64 mul64 = {(double)multipliers->r, (double)multipliers->g, (double)multipliers->b};
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_white_balance_apply_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, &mul64);
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    } else {
        alwan_rgb_f32 mul32 = {(float)multipliers->r, (float)multipliers->g, (float)multipliers->b};
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_white_balance_apply_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, &mul32);
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#elif ALWAN_WITH_F64
    {
        alwan_rgb_f64 mul64 = {(double)multipliers->r, (double)multipliers->g, (double)multipliers->b};
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_white_balance_apply_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, &mul64);
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#else
    {
        alwan_rgb_f32 mul32 = {(float)multipliers->r, (float)multipliers->g, (float)multipliers->b};
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_white_balance_apply_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, &mul32);
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CVD Machado planar _ex (tiled delegation to native planar)
 * ---------------------------------------------------------------- */

alwan_status alwan_simulate_cvd_machado_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_cvd_type cvd_type, alwan_f64 severity) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
#if ALWAN_WITH_BOTH
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_simulate_cvd_machado_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, cvd_type, (double)severity);
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    } else {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_simulate_cvd_machado_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, cvd_type, (float)severity);
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#elif ALWAN_WITH_F64
    {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_simulate_cvd_machado_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, cvd_type, (double)severity);
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#else
    {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
            alwan_simulate_cvd_machado_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, cvd_type, (float)severity);
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CMY <-> CMYK planar _ex (3<->4 channel, tiled delegation to SIMD native)
 * ---------------------------------------------------------------- */

alwan_status alwan_cmy_to_cmyk_map_planar_ex(void *out_c, size_t out_stride, void *out_m, void *out_y, void *out_k, void const *in_c, size_t in_stride, void const *in_m, void const *in_y, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt) {
    if (!in_c || !in_m || !in_y || !out_c || !out_m || !out_y || !out_k || count == 0) return ALWAN_E_INVALID;
#if ALWAN_WITH_BOTH
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64], ok_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in_c, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in_m, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in_y, in_fmt, off_, in_stride, tile_);
            alwan_cmy_to_cmyk_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ok_, ic0_, sizeof(double), ic1_, ic2_, tile_);
            alwan__store_tile_typed_ch_f64(out_c, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out_m, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out_y, out_fmt, off_, out_stride, oc2_, tile_);
            alwan__store_tile_typed_ch_f64(out_k, out_fmt, off_, out_stride, ok_, tile_);
            off_ += tile_;
        }
    } else {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32], ok_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in_c, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in_m, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in_y, in_fmt, off_, in_stride, tile_);
            alwan_cmy_to_cmyk_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ok_, ic0_, sizeof(float), ic1_, ic2_, tile_);
            alwan__store_tile_typed_ch_f32(out_c, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out_m, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out_y, out_fmt, off_, out_stride, oc2_, tile_);
            alwan__store_tile_typed_ch_f32(out_k, out_fmt, off_, out_stride, ok_, tile_);
            off_ += tile_;
        }
    }
#elif ALWAN_WITH_F64
    {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64], ok_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in_c, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in_m, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in_y, in_fmt, off_, in_stride, tile_);
            alwan_cmy_to_cmyk_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ok_, ic0_, sizeof(double), ic1_, ic2_, tile_);
            alwan__store_tile_typed_ch_f64(out_c, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out_m, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out_y, out_fmt, off_, out_stride, oc2_, tile_);
            alwan__store_tile_typed_ch_f64(out_k, out_fmt, off_, out_stride, ok_, tile_);
            off_ += tile_;
        }
    }
#else
    {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32], ok_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in_c, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in_m, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in_y, in_fmt, off_, in_stride, tile_);
            alwan_cmy_to_cmyk_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ok_, ic0_, sizeof(float), ic1_, ic2_, tile_);
            alwan__store_tile_typed_ch_f32(out_c, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out_m, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out_y, out_fmt, off_, out_stride, oc2_, tile_);
            alwan__store_tile_typed_ch_f32(out_k, out_fmt, off_, out_stride, ok_, tile_);
            off_ += tile_;
        }
    }
#endif
    return ALWAN_OK;
}

alwan_status alwan_cmyk_to_cmy_map_planar_ex(void *out_c, size_t out_stride, void *out_m, void *out_y, void const *in_c, size_t in_stride, void const *in_m, void const *in_y, void const *in_k, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt) {
    if (!in_c || !in_m || !in_y || !in_k || !out_c || !out_m || !out_y || count == 0) return ALWAN_E_INVALID;
#if ALWAN_WITH_BOTH
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64], ik_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in_c, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in_m, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in_y, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ik_, in_k, in_fmt, off_, in_stride, tile_);
            alwan_cmyk_to_cmy_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, ik_, tile_);
            alwan__store_tile_typed_ch_f64(out_c, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out_m, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out_y, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    } else {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32], ik_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in_c, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in_m, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in_y, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ik_, in_k, in_fmt, off_, in_stride, tile_);
            alwan_cmyk_to_cmy_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, ik_, tile_);
            alwan__store_tile_typed_ch_f32(out_c, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out_m, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out_y, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#elif ALWAN_WITH_F64
    {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64], ik_[ALWAN_TILE_PIXELS_F64];
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
            alwan__load_tile_typed_ch_f64(ic0_, in_c, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic1_, in_m, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ic2_, in_y, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f64(ik_, in_k, in_fmt, off_, in_stride, tile_);
            alwan_cmyk_to_cmy_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, ik_, tile_);
            alwan__store_tile_typed_ch_f64(out_c, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f64(out_m, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f64(out_y, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#else
    {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32], ik_[ALWAN_TILE_PIXELS_F32];
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
            alwan__load_tile_typed_ch_f32(ic0_, in_c, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic1_, in_m, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ic2_, in_y, in_fmt, off_, in_stride, tile_);
            alwan__load_tile_typed_ch_f32(ik_, in_k, in_fmt, off_, in_stride, tile_);
            alwan_cmyk_to_cmy_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, ik_, tile_);
            alwan__store_tile_typed_ch_f32(out_c, out_fmt, off_, out_stride, oc0_, tile_);
            alwan__store_tile_typed_ch_f32(out_m, out_fmt, off_, out_stride, oc1_, tile_);
            alwan__store_tile_typed_ch_f32(out_y, out_fmt, off_, out_stride, oc2_, tile_);
            off_ += tile_;
        }
    }
#endif
    return ALWAN_OK;
}
