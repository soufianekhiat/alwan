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

ALWAN_PLANAR_EX_DELEGATE(alwan_srgb_to_xyz_map_planar_ex,   alwan_srgb_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_srgb_map_planar_ex,   alwan_xyz_to_srgb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_srgb_to_lab_map_planar_ex,   alwan_srgb_to_lab_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_lab_to_srgb_map_planar_ex,   alwan_lab_to_srgb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_srgb_to_oklab_map_planar_ex, alwan_srgb_to_oklab_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_oklab_to_srgb_map_planar_ex, alwan_oklab_to_srgb_map_planar)

/* ----------------------------------------------------------------
 * Colorspace planar _ex (with white_xyz — delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_WHITE(alwan_xyz_to_lab_map_planar_ex, alwan_xyz_to_lab_map_planar)
ALWAN_PLANAR_EX_DELEGATE_WHITE(alwan_lab_to_xyz_map_planar_ex, alwan_lab_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE_WHITE(alwan_xyz_to_luv_map_planar_ex, alwan_xyz_to_luv_map_planar)
ALWAN_PLANAR_EX_DELEGATE_WHITE(alwan_luv_to_xyz_map_planar_ex, alwan_luv_to_xyz_map_planar)

/* Colorspace planar _ex (simple 3->3 — delegate to SIMD planar) */

ALWAN_PLANAR_EX_DELEGATE(alwan_lab_to_lch_map_planar_ex,    alwan_lab_to_lch_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_lch_to_lab_map_planar_ex,    alwan_lch_to_lab_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_luv_to_lchuv_map_planar_ex,  alwan_luv_to_lchuv_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_lchuv_to_luv_map_planar_ex,  alwan_lchuv_to_luv_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_xyy_map_planar_ex,    alwan_xyz_to_xyy_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_xyy_to_xyz_map_planar_ex,    alwan_xyy_to_xyz_map_planar)

/* ----------------------------------------------------------------
 * Oklab planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_oklab_map_planar_ex,   alwan_xyz_to_oklab_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_oklab_to_xyz_map_planar_ex,   alwan_oklab_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_oklab_to_oklch_map_planar_ex, alwan_oklab_to_oklch_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_oklch_to_oklab_map_planar_ex, alwan_oklch_to_oklab_map_planar)

/* ----------------------------------------------------------------
 * ICtCp planar _ex (with use_pq — delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_INT(alwan_rgb_to_ictcp_map_planar_ex,  alwan_rgb_to_ictcp_map_planar,  int, use_pq)
ALWAN_PLANAR_EX_DELEGATE_INT(alwan_ictcp_to_rgb_map_planar_ex,  alwan_ictcp_to_rgb_map_planar,  int, use_pq)
ALWAN_PLANAR_EX_DELEGATE_INT(alwan_xyz_to_ictcp_map_planar_ex,  alwan_xyz_to_ictcp_map_planar,  int, use_pq)
ALWAN_PLANAR_EX_DELEGATE_INT(alwan_ictcp_to_xyz_map_planar_ex,  alwan_ictcp_to_xyz_map_planar,  int, use_pq)

/* ----------------------------------------------------------------
 * JzAzBz planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_jzazbz_map_planar_ex,    alwan_xyz_to_jzazbz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_jzazbz_to_xyz_map_planar_ex,    alwan_jzazbz_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_jzazbz_to_jzczhz_map_planar_ex, alwan_jzazbz_to_jzczhz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_jzczhz_to_jzazbz_map_planar_ex, alwan_jzczhz_to_jzazbz_map_planar)

/* ----------------------------------------------------------------
 * IPT planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_ipt_map_planar_ex, alwan_xyz_to_ipt_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_ipt_to_xyz_map_planar_ex, alwan_ipt_to_xyz_map_planar)

/* ----------------------------------------------------------------
 * HSV/HSL planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE(alwan_rgb_to_hsv_map_planar_ex, alwan_rgb_to_hsv_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hsv_to_rgb_map_planar_ex, alwan_hsv_to_rgb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_rgb_to_hsl_map_planar_ex, alwan_rgb_to_hsl_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hsl_to_rgb_map_planar_ex, alwan_hsl_to_rgb_map_planar)

/* ----------------------------------------------------------------
 * CMY/YCoCg/HWB planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE(alwan_rgb_to_cmy_map_planar_ex,   alwan_rgb_to_cmy_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_cmy_to_rgb_map_planar_ex,   alwan_cmy_to_rgb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_rgb_to_ycocg_map_planar_ex, alwan_rgb_to_ycocg_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_ycocg_to_rgb_map_planar_ex, alwan_ycocg_to_rgb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_rgb_to_hwb_map_planar_ex,   alwan_rgb_to_hwb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hwb_to_rgb_map_planar_ex,   alwan_hwb_to_rgb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hsv_to_hwb_map_planar_ex,   alwan_hsv_to_hwb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hwb_to_hsv_map_planar_ex,   alwan_hwb_to_hsv_map_planar)

/* ----------------------------------------------------------------
 * HSP/HSPlog/HSY planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE(alwan_rgb_to_hsp_map_planar_ex,    alwan_rgb_to_hsp_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hsp_to_rgb_map_planar_ex,    alwan_hsp_to_rgb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_rgb_to_hsplog_map_planar_ex, alwan_rgb_to_hsplog_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hsplog_to_rgb_map_planar_ex, alwan_hsplog_to_rgb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_rgb_to_hsy_map_planar_ex,    alwan_rgb_to_hsy_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hsy_to_rgb_map_planar_ex,    alwan_hsy_to_rgb_map_planar)

/* ----------------------------------------------------------------
 * Linear sRGB <-> HSV/HSL planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE(alwan_linear_srgb_to_hsv_map_planar_ex, alwan_linear_srgb_to_hsv_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hsv_to_linear_srgb_map_planar_ex, alwan_hsv_to_linear_srgb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_linear_srgb_to_hsl_map_planar_ex, alwan_linear_srgb_to_hsl_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hsl_to_linear_srgb_map_planar_ex, alwan_hsl_to_linear_srgb_map_planar)

/* ----------------------------------------------------------------
 * YCbCr planar _ex (delegate to native planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_INT(alwan_rgb_to_ycbcr_map_planar_ex,  alwan_rgb_to_ycbcr_map_planar,  alwan_ycbcr_standard, standard)
ALWAN_PLANAR_EX_DELEGATE_INT(alwan_ycbcr_to_rgb_map_planar_ex,  alwan_ycbcr_to_rgb_map_planar,  alwan_ycbcr_standard, standard)

/* YcCbcCrc / legal-full planar _ex (delegate to SIMD planar) */

ALWAN_PLANAR_EX_DELEGATE_INT(alwan_rgb_to_yccbccrc_map_planar_ex,      alwan_rgb_to_yccbccrc_map_planar,      int, bit_depth)
ALWAN_PLANAR_EX_DELEGATE_INT(alwan_yccbccrc_to_rgb_map_planar_ex,      alwan_yccbccrc_to_rgb_map_planar,      int, bit_depth)
ALWAN_PLANAR_EX_DELEGATE_INT(alwan_ycbcr_full_to_legal_map_planar_ex,  alwan_ycbcr_full_to_legal_map_planar,  int, bit_depth)
ALWAN_PLANAR_EX_DELEGATE_INT(alwan_ycbcr_legal_to_full_map_planar_ex,  alwan_ycbcr_legal_to_full_map_planar,  int, bit_depth)

/* ----------------------------------------------------------------
 * Extended color spaces planar _ex (delegate to SIMD planar)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_igpgtg_map_planar_ex,     alwan_xyz_to_igpgtg_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_igpgtg_to_xyz_map_planar_ex,     alwan_igpgtg_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_icacb_map_planar_ex,      alwan_xyz_to_icacb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_icacb_to_xyz_map_planar_ex,      alwan_icacb_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_hdr_cielab_map_planar_ex, alwan_xyz_to_hdr_cielab_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hdr_cielab_to_xyz_map_planar_ex, alwan_hdr_cielab_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_hdr_ipt_map_planar_ex,    alwan_xyz_to_hdr_ipt_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hdr_ipt_to_xyz_map_planar_ex,    alwan_hdr_ipt_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_ucs_map_planar_ex,        alwan_xyz_to_ucs_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_ucs_to_xyz_map_planar_ex,        alwan_ucs_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_osa_ucs_map_planar_ex,    alwan_xyz_to_osa_ucs_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_osa_ucs_to_xyz_map_planar_ex,    alwan_osa_ucs_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_hunter_lab_map_planar_ex,  alwan_xyz_to_hunter_lab_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hunter_lab_to_xyz_map_planar_ex,  alwan_hunter_lab_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_xyz_to_prolab_map_planar_ex,      alwan_xyz_to_prolab_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_prolab_to_xyz_map_planar_ex,      alwan_prolab_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_rgb_to_prismatic_map_planar_ex,   alwan_rgb_to_prismatic_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_prismatic_to_rgb_map_planar_ex,   alwan_prismatic_to_rgb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_rgb_to_hcl_map_planar_ex,         alwan_rgb_to_hcl_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_hcl_to_rgb_map_planar_ex,         alwan_hcl_to_rgb_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_rgb_to_ihls_map_planar_ex,        alwan_rgb_to_ihls_map_planar)
ALWAN_PLANAR_EX_DELEGATE(alwan_ihls_to_rgb_map_planar_ex,        alwan_ihls_to_rgb_map_planar)

/* Extended with white point planar _ex (delegate to SIMD planar) */

ALWAN_PLANAR_EX_DELEGATE_WHITE(alwan_xyz_to_uvw_map_planar_ex,              alwan_xyz_to_uvw_map_planar)
ALWAN_PLANAR_EX_DELEGATE_WHITE(alwan_uvw_to_xyz_map_planar_ex,              alwan_uvw_to_xyz_map_planar)
ALWAN_PLANAR_EX_DELEGATE_WHITE(alwan_xyz_to_hunter_lab_custom_map_planar_ex, alwan_xyz_to_hunter_lab_custom_map_planar)
ALWAN_PLANAR_EX_DELEGATE_WHITE(alwan_hunter_lab_to_xyz_custom_map_planar_ex, alwan_hunter_lab_to_xyz_custom_map_planar)
ALWAN_PLANAR_EX_DELEGATE_WHITE(alwan_xyz_to_prolab_custom_map_planar_ex,     alwan_xyz_to_prolab_custom_map_planar)
ALWAN_PLANAR_EX_DELEGATE_WHITE(alwan_prolab_to_xyz_custom_map_planar_ex,     alwan_prolab_to_xyz_custom_map_planar)

/* DIN99 planar _ex (delegate to SIMD planar) */

ALWAN_PLANAR_EX_DELEGATE_INT(alwan_lab_to_din99_map_planar_ex, alwan_lab_to_din99_map_planar, int, variant)
ALWAN_PLANAR_EX_DELEGATE_INT(alwan_din99_to_lab_map_planar_ex, alwan_din99_to_lab_map_planar, int, variant)

/* ----------------------------------------------------------------
 * CVD planar _ex (delegate to SIMD planar where possible)
 * ---------------------------------------------------------------- */

ALWAN_PLANAR_EX_DELEGATE_SCALAR(alwan_simulate_protanopia_map_planar_ex,   alwan_simulate_protanopia_map_planar)
ALWAN_PLANAR_EX_DELEGATE_SCALAR(alwan_simulate_deuteranopia_map_planar_ex, alwan_simulate_deuteranopia_map_planar)
ALWAN_PLANAR_EX_DELEGATE_SCALAR(alwan_simulate_tritanopia_map_planar_ex,   alwan_simulate_tritanopia_map_planar)

int alwan_simulate_cvd_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                       void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                       alwan_cvd_type cvd_type, alwan_scalar severity,
                                       size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ic0_[ALWAN_TILE_PIXELS], ic1_[ALWAN_TILE_PIXELS], ic2_[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_scalar oc0_[ALWAN_TILE_PIXELS], oc1_[ALWAN_TILE_PIXELS], oc2_[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_ch(ic0_, in0, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic1_, in1, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic2_, in2, in_fmt, off_, in_stride, tile_);
        alwan_simulate_cvd_map_planar(oc0_, oc1_, oc2_, ic0_, ic1_, ic2_,
                                      cvd_type, severity, tile_,
                                      sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan__store_tile_typed_ch(out0, out_fmt, off_, out_stride, oc0_, tile_);
        alwan__store_tile_typed_ch(out1, out_fmt, off_, out_stride, oc1_, tile_);
        alwan__store_tile_typed_ch(out2, out_fmt, off_, out_stride, oc2_, tile_);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Color correction planar _ex (tiled delegation to native planar)
 * ---------------------------------------------------------------- */

int alwan_lgg_apply_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                    void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                    alwan_rgb const *lift, alwan_rgb const *gamma, alwan_rgb const *gain,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !lift || !gamma || !gain || count == 0)
        return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ic0_[ALWAN_TILE_PIXELS], ic1_[ALWAN_TILE_PIXELS], ic2_[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_scalar oc0_[ALWAN_TILE_PIXELS], oc1_[ALWAN_TILE_PIXELS], oc2_[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_ch(ic0_, in0, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic1_, in1, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic2_, in2, in_fmt, off_, in_stride, tile_);
        alwan_lgg_apply_map_planar(oc0_, oc1_, oc2_, ic0_, ic1_, ic2_,
                                   lift, gamma, gain, tile_,
                                   sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan__store_tile_typed_ch(out0, out_fmt, off_, out_stride, oc0_, tile_);
        alwan__store_tile_typed_ch(out1, out_fmt, off_, out_stride, oc1_, tile_);
        alwan__store_tile_typed_ch(out2, out_fmt, off_, out_stride, oc2_, tile_);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

int alwan_color_matrix_apply_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                             void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                             alwan_mat3x3 const *matrix,
                                             size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !matrix || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ic0_[ALWAN_TILE_PIXELS], ic1_[ALWAN_TILE_PIXELS], ic2_[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_scalar oc0_[ALWAN_TILE_PIXELS], oc1_[ALWAN_TILE_PIXELS], oc2_[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_ch(ic0_, in0, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic1_, in1, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic2_, in2, in_fmt, off_, in_stride, tile_);
        alwan_color_matrix_apply_map_planar(oc0_, oc1_, oc2_, ic0_, ic1_, ic2_,
                                            matrix, tile_,
                                            sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan__store_tile_typed_ch(out0, out_fmt, off_, out_stride, oc0_, tile_);
        alwan__store_tile_typed_ch(out1, out_fmt, off_, out_stride, oc1_, tile_);
        alwan__store_tile_typed_ch(out2, out_fmt, off_, out_stride, oc2_, tile_);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

int alwan_printer_lights_apply_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                               void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                               alwan_scalar red_lights, alwan_scalar green_lights, alwan_scalar blue_lights,
                                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ic0_[ALWAN_TILE_PIXELS], ic1_[ALWAN_TILE_PIXELS], ic2_[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_scalar oc0_[ALWAN_TILE_PIXELS], oc1_[ALWAN_TILE_PIXELS], oc2_[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_ch(ic0_, in0, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic1_, in1, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic2_, in2, in_fmt, off_, in_stride, tile_);
        alwan_printer_lights_apply_map_planar(oc0_, oc1_, oc2_, ic0_, ic1_, ic2_,
                                              red_lights, green_lights, blue_lights, tile_,
                                              sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan__store_tile_typed_ch(out0, out_fmt, off_, out_stride, oc0_, tile_);
        alwan__store_tile_typed_ch(out1, out_fmt, off_, out_stride, oc1_, tile_);
        alwan__store_tile_typed_ch(out2, out_fmt, off_, out_stride, oc2_, tile_);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

int alwan_white_balance_apply_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                              void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                              alwan_rgb const *multipliers,
                                              size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !multipliers || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ic0_[ALWAN_TILE_PIXELS], ic1_[ALWAN_TILE_PIXELS], ic2_[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_scalar oc0_[ALWAN_TILE_PIXELS], oc1_[ALWAN_TILE_PIXELS], oc2_[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_ch(ic0_, in0, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic1_, in1, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic2_, in2, in_fmt, off_, in_stride, tile_);
        alwan_white_balance_apply_map_planar(oc0_, oc1_, oc2_, ic0_, ic1_, ic2_,
                                             multipliers, tile_,
                                             sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan__store_tile_typed_ch(out0, out_fmt, off_, out_stride, oc0_, tile_);
        alwan__store_tile_typed_ch(out1, out_fmt, off_, out_stride, oc1_, tile_);
        alwan__store_tile_typed_ch(out2, out_fmt, off_, out_stride, oc2_, tile_);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CVD Machado planar _ex (tiled delegation to native planar)
 * ---------------------------------------------------------------- */

int alwan_simulate_cvd_machado_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                               void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                               alwan_cvd_type cvd_type, alwan_scalar severity,
                                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ic0_[ALWAN_TILE_PIXELS], ic1_[ALWAN_TILE_PIXELS], ic2_[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_scalar oc0_[ALWAN_TILE_PIXELS], oc1_[ALWAN_TILE_PIXELS], oc2_[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_ch(ic0_, in0, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic1_, in1, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic2_, in2, in_fmt, off_, in_stride, tile_);
        alwan_simulate_cvd_machado_map_planar(oc0_, oc1_, oc2_, ic0_, ic1_, ic2_,
                                              cvd_type, severity, tile_,
                                              sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan__store_tile_typed_ch(out0, out_fmt, off_, out_stride, oc0_, tile_);
        alwan__store_tile_typed_ch(out1, out_fmt, off_, out_stride, oc1_, tile_);
        alwan__store_tile_typed_ch(out2, out_fmt, off_, out_stride, oc2_, tile_);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CMY <-> CMYK planar _ex (3<->4 channel, tiled delegation to SIMD native)
 * ---------------------------------------------------------------- */

int alwan_cmy_to_cmyk_map_planar_ex(void *out_c, void *out_m, void *out_y, void *out_k, alwan_pixel_format out_fmt,
                           void const *in_c, void const *in_m, void const *in_y, alwan_pixel_format in_fmt,
                           size_t count, size_t in_stride, size_t out_stride) {
    if (!in_c || !in_m || !in_y || !out_c || !out_m || !out_y || !out_k || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ic0_[ALWAN_TILE_PIXELS], ic1_[ALWAN_TILE_PIXELS], ic2_[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_scalar oc0_[ALWAN_TILE_PIXELS], oc1_[ALWAN_TILE_PIXELS], oc2_[ALWAN_TILE_PIXELS], ok_[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_ch(ic0_, in_c, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic1_, in_m, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic2_, in_y, in_fmt, off_, in_stride, tile_);
        alwan_cmy_to_cmyk_map_planar(oc0_, oc1_, oc2_, ok_, ic0_, ic1_, ic2_, tile_,
                                     sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan__store_tile_typed_ch(out_c, out_fmt, off_, out_stride, oc0_, tile_);
        alwan__store_tile_typed_ch(out_m, out_fmt, off_, out_stride, oc1_, tile_);
        alwan__store_tile_typed_ch(out_y, out_fmt, off_, out_stride, oc2_, tile_);
        alwan__store_tile_typed_ch(out_k, out_fmt, off_, out_stride, ok_, tile_);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

int alwan_cmyk_to_cmy_map_planar_ex(void *out_c, void *out_m, void *out_y, alwan_pixel_format out_fmt,
                           void const *in_c, void const *in_m, void const *in_y, void const *in_k, alwan_pixel_format in_fmt,
                           size_t count, size_t in_stride, size_t out_stride) {
    if (!in_c || !in_m || !in_y || !in_k || !out_c || !out_m || !out_y || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ic0_[ALWAN_TILE_PIXELS], ic1_[ALWAN_TILE_PIXELS], ic2_[ALWAN_TILE_PIXELS], ik_[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_scalar oc0_[ALWAN_TILE_PIXELS], oc1_[ALWAN_TILE_PIXELS], oc2_[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_ch(ic0_, in_c, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic1_, in_m, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ic2_, in_y, in_fmt, off_, in_stride, tile_);
        alwan__load_tile_typed_ch(ik_, in_k, in_fmt, off_, in_stride, tile_);
        alwan_cmyk_to_cmy_map_planar(oc0_, oc1_, oc2_, ic0_, ic1_, ic2_, ik_, tile_,
                                     sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan__store_tile_typed_ch(out_c, out_fmt, off_, out_stride, oc0_, tile_);
        alwan__store_tile_typed_ch(out_m, out_fmt, off_, out_stride, oc1_, tile_);
        alwan__store_tile_typed_ch(out_y, out_fmt, off_, out_stride, oc2_, tile_);
        off_ += tile_;
    } }
    return ALWAN_OK;
}
