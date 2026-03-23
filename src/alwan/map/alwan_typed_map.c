/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Typed Map Functions (_ex variants)
 * Accept void* buffers with alwan_pixel_format for u8/u16/f16/f32/f64 I/O
 *
 * All _ex functions delegate to native SIMD interleave functions via tiled
 * typed load/store.  The format switch is outside the inner loop so the
 * compiler can auto-vectorize the typed conversion.
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
 * Collect / Scatter utilities
 * ---------------------------------------------------------------- */

int alwan_collect3(alwan_scalar *out,
                   void const *in, alwan_pixel_format in_fmt,
                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar s[3];
        alwan__load3_typed(s, (char const *)in + i * in_stride, in_fmt);
        alwan_scalar *op = (alwan_scalar *)((char *)out + i * out_stride);
        op[0] = s[0]; op[1] = s[1]; op[2] = s[2];
    }
    return ALWAN_OK;
}

int alwan_scatter3(void *out, alwan_pixel_format out_fmt,
                   alwan_scalar const *in,
                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *ip = (alwan_scalar const *)((char const *)in + i * in_stride);
        alwan__store3_typed((char *)out + i * out_stride, ip, out_fmt);
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * sRGB Convenience _ex: moved to alwan_rgb_map.c (SIMD-accelerated)
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * Color Space _ex (pattern B: with white_xyz)
 * XYZ<->Lab, XYZ<->Luv: moved to alwan_colorspace_map.c (SIMD-accelerated)
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * Color Space _ex (simple 3->3)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE(alwan_lab_to_lch_map_interleave_ex,    alwan_lab_to_lch_map_interleave)
ALWAN_EX_DELEGATE(alwan_lch_to_lab_map_interleave_ex,    alwan_lch_to_lab_map_interleave)
ALWAN_EX_DELEGATE(alwan_luv_to_lchuv_map_interleave_ex,  alwan_luv_to_lchuv_map_interleave)
ALWAN_EX_DELEGATE(alwan_lchuv_to_luv_map_interleave_ex,  alwan_lchuv_to_luv_map_interleave)
ALWAN_EX_DELEGATE(alwan_xyz_to_xyy_map_interleave_ex,    alwan_xyz_to_xyy_map_interleave)
ALWAN_EX_DELEGATE(alwan_xyy_to_xyz_map_interleave_ex,    alwan_xyy_to_xyz_map_interleave)

/* ----------------------------------------------------------------
 * Oklab _ex: XYZ<->Oklab moved to alwan_oklab_map.c (SIMD-accelerated)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE(alwan_oklab_to_oklch_map_interleave_ex, alwan_oklab_to_oklch_map_interleave)
ALWAN_EX_DELEGATE(alwan_oklch_to_oklab_map_interleave_ex, alwan_oklch_to_oklab_map_interleave)

/* ----------------------------------------------------------------
 * ICtCp _ex (with use_pq int param)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_INT(alwan_rgb_to_ictcp_map_interleave_ex,  alwan_rgb_to_ictcp_map_interleave,  int, use_pq)
ALWAN_EX_DELEGATE_INT(alwan_ictcp_to_rgb_map_interleave_ex,  alwan_ictcp_to_rgb_map_interleave,  int, use_pq)
ALWAN_EX_DELEGATE_INT(alwan_xyz_to_ictcp_map_interleave_ex,  alwan_xyz_to_ictcp_map_interleave,  int, use_pq)
ALWAN_EX_DELEGATE_INT(alwan_ictcp_to_xyz_map_interleave_ex,  alwan_ictcp_to_xyz_map_interleave,  int, use_pq)

/* ----------------------------------------------------------------
 * JzAzBz _ex (simple 3->3)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE(alwan_xyz_to_jzazbz_map_interleave_ex,    alwan_xyz_to_jzazbz_map_interleave)
ALWAN_EX_DELEGATE(alwan_jzazbz_to_xyz_map_interleave_ex,    alwan_jzazbz_to_xyz_map_interleave)
ALWAN_EX_DELEGATE(alwan_jzazbz_to_jzczhz_map_interleave_ex, alwan_jzazbz_to_jzczhz_map_interleave)
ALWAN_EX_DELEGATE(alwan_jzczhz_to_jzazbz_map_interleave_ex, alwan_jzczhz_to_jzazbz_map_interleave)

/* ----------------------------------------------------------------
 * IPT _ex (simple 3->3)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE(alwan_xyz_to_ipt_map_interleave_ex, alwan_xyz_to_ipt_map_interleave)
ALWAN_EX_DELEGATE(alwan_ipt_to_xyz_map_interleave_ex, alwan_ipt_to_xyz_map_interleave)

/* ----------------------------------------------------------------
 * Convenience HSV/HSL _ex
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE(alwan_rgb_to_hsv_map_interleave_ex, alwan_rgb_to_hsv_map_interleave)
ALWAN_EX_DELEGATE(alwan_hsv_to_rgb_map_interleave_ex, alwan_hsv_to_rgb_map_interleave)
ALWAN_EX_DELEGATE(alwan_rgb_to_hsl_map_interleave_ex, alwan_rgb_to_hsl_map_interleave)
ALWAN_EX_DELEGATE(alwan_hsl_to_rgb_map_interleave_ex, alwan_hsl_to_rgb_map_interleave)

/* ----------------------------------------------------------------
 * HSP/HSY _ex
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE(alwan_rgb_to_hsp_map_interleave_ex,    alwan_rgb_to_hsp_map_interleave)
ALWAN_EX_DELEGATE(alwan_hsp_to_rgb_map_interleave_ex,    alwan_hsp_to_rgb_map_interleave)
ALWAN_EX_DELEGATE(alwan_rgb_to_hsplog_map_interleave_ex, alwan_rgb_to_hsplog_map_interleave)
ALWAN_EX_DELEGATE(alwan_hsplog_to_rgb_map_interleave_ex, alwan_hsplog_to_rgb_map_interleave)
ALWAN_EX_DELEGATE(alwan_rgb_to_hsy_map_interleave_ex,    alwan_rgb_to_hsy_map_interleave)
ALWAN_EX_DELEGATE(alwan_hsy_to_rgb_map_interleave_ex,    alwan_hsy_to_rgb_map_interleave)

/* ----------------------------------------------------------------
 * Linear sRGB <-> HSV/HSL _ex
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE(alwan_linear_srgb_to_hsv_map_interleave_ex, alwan_linear_srgb_to_hsv_map_interleave)
ALWAN_EX_DELEGATE(alwan_hsv_to_linear_srgb_map_interleave_ex, alwan_hsv_to_linear_srgb_map_interleave)
ALWAN_EX_DELEGATE(alwan_linear_srgb_to_hsl_map_interleave_ex, alwan_linear_srgb_to_hsl_map_interleave)
ALWAN_EX_DELEGATE(alwan_hsl_to_linear_srgb_map_interleave_ex, alwan_hsl_to_linear_srgb_map_interleave)

/* ----------------------------------------------------------------
 * Convenience extra _ex (CMY, YCoCg, HWB)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE(alwan_rgb_to_cmy_map_interleave_ex,   alwan_rgb_to_cmy_map_interleave)
ALWAN_EX_DELEGATE(alwan_cmy_to_rgb_map_interleave_ex,   alwan_cmy_to_rgb_map_interleave)
ALWAN_EX_DELEGATE(alwan_rgb_to_ycocg_map_interleave_ex, alwan_rgb_to_ycocg_map_interleave)
ALWAN_EX_DELEGATE(alwan_ycocg_to_rgb_map_interleave_ex, alwan_ycocg_to_rgb_map_interleave)
ALWAN_EX_DELEGATE(alwan_rgb_to_hwb_map_interleave_ex,   alwan_rgb_to_hwb_map_interleave)
ALWAN_EX_DELEGATE(alwan_hwb_to_rgb_map_interleave_ex,   alwan_hwb_to_rgb_map_interleave)
ALWAN_EX_DELEGATE(alwan_hsv_to_hwb_map_interleave_ex,   alwan_hsv_to_hwb_map_interleave)
ALWAN_EX_DELEGATE(alwan_hwb_to_hsv_map_interleave_ex,   alwan_hwb_to_hsv_map_interleave)

/* ----------------------------------------------------------------
 * YCbCr _ex (with standard enum)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_INT(alwan_rgb_to_ycbcr_map_interleave_ex,  alwan_rgb_to_ycbcr_map_interleave,  alwan_ycbcr_standard, standard)
ALWAN_EX_DELEGATE_INT(alwan_ycbcr_to_rgb_map_interleave_ex,  alwan_ycbcr_to_rgb_map_interleave,  alwan_ycbcr_standard, standard)

/* ----------------------------------------------------------------
 * YcCbcCrc / legal-full _ex (with bit_depth)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_INT(alwan_rgb_to_yccbccrc_map_interleave_ex,      alwan_rgb_to_yccbccrc_map_interleave,      int, bit_depth)
ALWAN_EX_DELEGATE_INT(alwan_yccbccrc_to_rgb_map_interleave_ex,      alwan_yccbccrc_to_rgb_map_interleave,      int, bit_depth)
ALWAN_EX_DELEGATE_INT(alwan_ycbcr_full_to_legal_map_interleave_ex,  alwan_ycbcr_full_to_legal_map_interleave,  int, bit_depth)
ALWAN_EX_DELEGATE_INT(alwan_ycbcr_legal_to_full_map_interleave_ex,  alwan_ycbcr_legal_to_full_map_interleave,  int, bit_depth)

/* ----------------------------------------------------------------
 * Extended color spaces _ex (simple 3->3)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE(alwan_xyz_to_igpgtg_map_interleave_ex,     alwan_xyz_to_igpgtg_map_interleave)
ALWAN_EX_DELEGATE(alwan_igpgtg_to_xyz_map_interleave_ex,     alwan_igpgtg_to_xyz_map_interleave)
ALWAN_EX_DELEGATE(alwan_xyz_to_icacb_map_interleave_ex,      alwan_xyz_to_icacb_map_interleave)
ALWAN_EX_DELEGATE(alwan_icacb_to_xyz_map_interleave_ex,      alwan_icacb_to_xyz_map_interleave)
ALWAN_EX_DELEGATE(alwan_xyz_to_hdr_cielab_map_interleave_ex, alwan_xyz_to_hdr_cielab_map_interleave)
ALWAN_EX_DELEGATE(alwan_hdr_cielab_to_xyz_map_interleave_ex, alwan_hdr_cielab_to_xyz_map_interleave)
ALWAN_EX_DELEGATE(alwan_xyz_to_hdr_ipt_map_interleave_ex,    alwan_xyz_to_hdr_ipt_map_interleave)
ALWAN_EX_DELEGATE(alwan_hdr_ipt_to_xyz_map_interleave_ex,    alwan_hdr_ipt_to_xyz_map_interleave)
ALWAN_EX_DELEGATE(alwan_xyz_to_ucs_map_interleave_ex,        alwan_xyz_to_ucs_map_interleave)
ALWAN_EX_DELEGATE(alwan_ucs_to_xyz_map_interleave_ex,        alwan_ucs_to_xyz_map_interleave)
ALWAN_EX_DELEGATE(alwan_xyz_to_osa_ucs_map_interleave_ex,    alwan_xyz_to_osa_ucs_map_interleave)
ALWAN_EX_DELEGATE(alwan_osa_ucs_to_xyz_map_interleave_ex,    alwan_osa_ucs_to_xyz_map_interleave)
ALWAN_EX_DELEGATE(alwan_xyz_to_hunter_lab_map_interleave_ex,  alwan_xyz_to_hunter_lab_map_interleave)
ALWAN_EX_DELEGATE(alwan_hunter_lab_to_xyz_map_interleave_ex,  alwan_hunter_lab_to_xyz_map_interleave)
ALWAN_EX_DELEGATE(alwan_xyz_to_prolab_map_interleave_ex,      alwan_xyz_to_prolab_map_interleave)
ALWAN_EX_DELEGATE(alwan_prolab_to_xyz_map_interleave_ex,      alwan_prolab_to_xyz_map_interleave)
ALWAN_EX_DELEGATE(alwan_rgb_to_prismatic_map_interleave_ex,   alwan_rgb_to_prismatic_map_interleave)
ALWAN_EX_DELEGATE(alwan_prismatic_to_rgb_map_interleave_ex,   alwan_prismatic_to_rgb_map_interleave)
ALWAN_EX_DELEGATE(alwan_rgb_to_hcl_map_interleave_ex,         alwan_rgb_to_hcl_map_interleave)
ALWAN_EX_DELEGATE(alwan_hcl_to_rgb_map_interleave_ex,         alwan_hcl_to_rgb_map_interleave)
ALWAN_EX_DELEGATE(alwan_rgb_to_ihls_map_interleave_ex,        alwan_rgb_to_ihls_map_interleave)
ALWAN_EX_DELEGATE(alwan_ihls_to_rgb_map_interleave_ex,        alwan_ihls_to_rgb_map_interleave)

/* Extended _ex with white point */

ALWAN_EX_DELEGATE_WHITE(alwan_xyz_to_uvw_map_interleave_ex,              alwan_xyz_to_uvw_map_interleave)
ALWAN_EX_DELEGATE_WHITE(alwan_uvw_to_xyz_map_interleave_ex,              alwan_uvw_to_xyz_map_interleave)
ALWAN_EX_DELEGATE_WHITE(alwan_xyz_to_hunter_lab_custom_map_interleave_ex, alwan_xyz_to_hunter_lab_custom_map_interleave)
ALWAN_EX_DELEGATE_WHITE(alwan_hunter_lab_to_xyz_custom_map_interleave_ex, alwan_hunter_lab_to_xyz_custom_map_interleave)
ALWAN_EX_DELEGATE_WHITE(alwan_xyz_to_prolab_custom_map_interleave_ex,     alwan_xyz_to_prolab_custom_map_interleave)
ALWAN_EX_DELEGATE_WHITE(alwan_prolab_to_xyz_custom_map_interleave_ex,     alwan_prolab_to_xyz_custom_map_interleave)

/* DIN99 _ex (with int variant) */

ALWAN_EX_DELEGATE_INT(alwan_lab_to_din99_map_interleave_ex, alwan_lab_to_din99_map_interleave, int, variant)
ALWAN_EX_DELEGATE_INT(alwan_din99_to_lab_map_interleave_ex, alwan_din99_to_lab_map_interleave, int, variant)

/* ----------------------------------------------------------------
 * Color correction _ex (tiled delegation to native functions)
 * ---------------------------------------------------------------- */

int alwan_lgg_apply_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                            void const *in, alwan_pixel_format in_fmt,
                            alwan_rgb const *lift, alwan_rgb const *gamma, alwan_rgb const *gain,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || !lift || !gamma || !gain || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ibuf_[ALWAN_TILE_PIXELS * 3];
        ALWAN_ALIGN(32) alwan_scalar obuf_[ALWAN_TILE_PIXELS * 3];
        alwan__load_tile_typed_aos(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
        alwan_lgg_apply_map_interleave(obuf_, ibuf_, lift, gamma, gain, tile_,
                                       3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
        alwan__store_tile_typed_aos(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

int alwan_color_matrix_apply_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                                     void const *in, alwan_pixel_format in_fmt,
                                     alwan_mat3x3 const *matrix,
                                     size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || !matrix || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ibuf_[ALWAN_TILE_PIXELS * 3];
        ALWAN_ALIGN(32) alwan_scalar obuf_[ALWAN_TILE_PIXELS * 3];
        alwan__load_tile_typed_aos(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
        alwan_color_matrix_apply_map_interleave(obuf_, ibuf_, matrix, tile_,
                                                3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
        alwan__store_tile_typed_aos(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

int alwan_printer_lights_apply_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                                       void const *in, alwan_pixel_format in_fmt,
                                       alwan_scalar red_lights, alwan_scalar green_lights,
                                       alwan_scalar blue_lights,
                                       size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ibuf_[ALWAN_TILE_PIXELS * 3];
        ALWAN_ALIGN(32) alwan_scalar obuf_[ALWAN_TILE_PIXELS * 3];
        alwan__load_tile_typed_aos(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
        alwan_printer_lights_apply_map_interleave(obuf_, ibuf_, red_lights, green_lights, blue_lights, tile_,
                                                  3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
        alwan__store_tile_typed_aos(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

int alwan_white_balance_apply_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                                      void const *in, alwan_pixel_format in_fmt,
                                      alwan_rgb const *multipliers,
                                      size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || !multipliers || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ibuf_[ALWAN_TILE_PIXELS * 3];
        ALWAN_ALIGN(32) alwan_scalar obuf_[ALWAN_TILE_PIXELS * 3];
        alwan__load_tile_typed_aos(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
        alwan_white_balance_apply_map_interleave(obuf_, ibuf_, multipliers, tile_,
                                                 3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
        alwan__store_tile_typed_aos(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CVD _ex (with severity)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_SCALAR(alwan_simulate_protanopia_map_interleave_ex,   alwan_simulate_protanopia_map_interleave)
ALWAN_EX_DELEGATE_SCALAR(alwan_simulate_deuteranopia_map_interleave_ex, alwan_simulate_deuteranopia_map_interleave)
ALWAN_EX_DELEGATE_SCALAR(alwan_simulate_tritanopia_map_interleave_ex,   alwan_simulate_tritanopia_map_interleave)

int alwan_simulate_cvd_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               alwan_cvd_type cvd_type, alwan_scalar severity,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ibuf_[ALWAN_TILE_PIXELS * 3];
        ALWAN_ALIGN(32) alwan_scalar obuf_[ALWAN_TILE_PIXELS * 3];
        alwan__load_tile_typed_aos(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
        alwan_simulate_cvd_map_interleave(obuf_, ibuf_, cvd_type, severity, tile_,
                                          3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
        alwan__store_tile_typed_aos(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CVD Machado _ex
 * ---------------------------------------------------------------- */

int alwan_simulate_cvd_machado_map_interleave_ex(void *rgb_out, alwan_pixel_format out_fmt,
                                                   void const *rgb_in, alwan_pixel_format in_fmt,
                                                   alwan_cvd_type cvd_type, alwan_scalar severity,
                                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ibuf_[ALWAN_TILE_PIXELS * 3];
        ALWAN_ALIGN(32) alwan_scalar obuf_[ALWAN_TILE_PIXELS * 3];
        alwan__load_tile_typed_aos(ibuf_, rgb_in, in_fmt, off_, in_stride, tile_, 3);
        alwan_simulate_cvd_machado_map_interleave(obuf_, ibuf_, cvd_type, severity, tile_,
                                                  3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
        alwan__store_tile_typed_aos(rgb_out, out_fmt, off_, out_stride, obuf_, tile_, 3);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CMY <-> CMYK _ex (3<->4 channel, tiled delegation)
 * ---------------------------------------------------------------- */

int alwan_cmy_to_cmyk_map_interleave_ex(void *cmyk_out, alwan_pixel_format out_fmt,
                           void const *cmy_in, alwan_pixel_format in_fmt,
                           size_t count, size_t in_stride, size_t out_stride) {
    if (!cmy_in || !cmyk_out || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ibuf_[ALWAN_TILE_PIXELS * 3];
        ALWAN_ALIGN(32) alwan_scalar obuf_[ALWAN_TILE_PIXELS * 4];
        alwan__load_tile_typed_aos(ibuf_, cmy_in, in_fmt, off_, in_stride, tile_, 3);
        alwan_cmy_to_cmyk_map_interleave(obuf_, ibuf_, tile_,
                                         3 * sizeof(alwan_scalar), 4 * sizeof(alwan_scalar));
        alwan__store_tile_typed_aos(cmyk_out, out_fmt, off_, out_stride, obuf_, tile_, 4);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

int alwan_cmyk_to_cmy_map_interleave_ex(void *cmy_out, alwan_pixel_format out_fmt,
                           void const *cmyk_in, alwan_pixel_format in_fmt,
                           size_t count, size_t in_stride, size_t out_stride) {
    if (!cmyk_in || !cmy_out || count == 0) return ALWAN_E_INVALID;
    { size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_scalar ibuf_[ALWAN_TILE_PIXELS * 4];
        ALWAN_ALIGN(32) alwan_scalar obuf_[ALWAN_TILE_PIXELS * 3];
        alwan__load_tile_typed_aos(ibuf_, cmyk_in, in_fmt, off_, in_stride, tile_, 4);
        alwan_cmyk_to_cmy_map_interleave(obuf_, ibuf_, tile_,
                                         4 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
        alwan__store_tile_typed_aos(cmy_out, out_fmt, off_, out_stride, obuf_, tile_, 3);
        off_ += tile_;
    } }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM _ex (typed XYZ side, struct correlates side)
 * These have non-standard signatures (one side struct, one side typed)
 * and per-pixel error returns, so they remain per-pixel.
 * ---------------------------------------------------------------- */

int alwan_ciecam02_forward_map_interleave_ex(alwan_ciecam02_correlates *correlates_out,
                                   void const *xyz_in, alwan_pixel_format in_fmt,
                                   alwan_ciecam02_viewing_conditions const *vc,
                                   size_t count, size_t in_stride) {
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar s[3];
        alwan__load3_typed(s, (char const *)xyz_in + i * in_stride, in_fmt);
        alwan_xyz xyz = {s[0], s[1], s[2]};
        int st = alwan_ciecam02_forward(&correlates_out[i], &xyz, vc);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

int alwan_ciecam02_inverse_map_interleave_ex(void *xyz_out, alwan_pixel_format out_fmt,
                                   alwan_ciecam02_correlates const *correlates_in,
                                   alwan_ciecam02_viewing_conditions const *vc,
                                   size_t count, size_t out_stride) {
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_xyz xyz;
        int st = alwan_ciecam02_inverse(&xyz, &correlates_in[i], vc);
        if (st != ALWAN_OK) return st;
        alwan_scalar d[3] = {xyz.x, xyz.y, xyz.z};
        alwan__store3_typed((char *)xyz_out + i * out_stride, d, out_fmt);
    }
    return ALWAN_OK;
}

int alwan_cam16_forward_map_interleave_ex(alwan_cam16_correlates *correlates_out,
                                void const *xyz_in, alwan_pixel_format in_fmt,
                                alwan_cam16_viewing_conditions const *vc,
                                size_t count, size_t in_stride) {
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar s[3];
        alwan__load3_typed(s, (char const *)xyz_in + i * in_stride, in_fmt);
        alwan_xyz xyz = {s[0], s[1], s[2]};
        int st = alwan_cam16_forward(&correlates_out[i], &xyz, vc);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

int alwan_cam16_inverse_map_interleave_ex(void *xyz_out, alwan_pixel_format out_fmt,
                                alwan_cam16_correlates const *correlates_in,
                                alwan_cam16_viewing_conditions const *vc,
                                size_t count, size_t out_stride) {
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_xyz xyz;
        int st = alwan_cam16_inverse(&xyz, &correlates_in[i], vc);
        if (st != ALWAN_OK) return st;
        alwan_scalar d[3] = {xyz.x, xyz.y, xyz.z};
        alwan__store3_typed((char *)xyz_out + i * out_stride, d, out_fmt);
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Delta E batch _ex (typed Lab inputs)
 * Two typed inputs → scalar output; remain per-pixel.
 * ---------------------------------------------------------------- */

int alwan_delta_e_76_batch_ex(alwan_scalar *delta_e_out,
                               void const *lab1_in, alwan_pixel_format lab1_fmt,
                               void const *lab2_in, alwan_pixel_format lab2_fmt,
                               size_t count, size_t in1_stride, size_t in2_stride) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar s1[3], s2[3];
        alwan__load3_typed(s1, (char const *)lab1_in + i * in1_stride, lab1_fmt);
        alwan__load3_typed(s2, (char const *)lab2_in + i * in2_stride, lab2_fmt);
        alwan_lab l1 = {s1[0], s1[1], s1[2]};
        alwan_lab l2 = {s2[0], s2[1], s2[2]};
        delta_e_out[i] = alwan_delta_e_76(&l1, &l2);
    }
    return ALWAN_OK;
}

int alwan_delta_e_2000_batch_ex(alwan_scalar *delta_e_out,
                                 void const *lab1_in, alwan_pixel_format lab1_fmt,
                                 void const *lab2_in, alwan_pixel_format lab2_fmt,
                                 size_t count, size_t in1_stride, size_t in2_stride) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar s1[3], s2[3];
        alwan__load3_typed(s1, (char const *)lab1_in + i * in1_stride, lab1_fmt);
        alwan__load3_typed(s2, (char const *)lab2_in + i * in2_stride, lab2_fmt);
        alwan_lab l1 = {s1[0], s1[1], s1[2]};
        alwan_lab l2 = {s2[0], s2[1], s2[2]};
        delta_e_out[i] = alwan_delta_e_2000(&l1, &l2);
    }
    return ALWAN_OK;
}

int alwan_delta_e_94_batch_ex(alwan_scalar *delta_e_out,
                               void const *lab1_in, alwan_pixel_format lab1_fmt,
                               void const *lab2_in, alwan_pixel_format lab2_fmt,
                               size_t count, size_t in1_stride, size_t in2_stride) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar s1[3], s2[3];
        alwan__load3_typed(s1, (char const *)lab1_in + i * in1_stride, lab1_fmt);
        alwan__load3_typed(s2, (char const *)lab2_in + i * in2_stride, lab2_fmt);
        alwan_lab l1 = {s1[0], s1[1], s1[2]};
        alwan_lab l2 = {s2[0], s2[1], s2[2]};
        delta_e_out[i] = alwan_delta_e_94(&l1, &l2);
    }
    return ALWAN_OK;
}

int alwan_delta_e_cmc_batch_ex(alwan_scalar *delta_e_out,
                                void const *lab1_in, alwan_pixel_format lab1_fmt,
                                void const *lab2_in, alwan_pixel_format lab2_fmt,
                                alwan_scalar l, alwan_scalar c,
                                size_t count, size_t in1_stride, size_t in2_stride) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar s1[3], s2[3];
        alwan__load3_typed(s1, (char const *)lab1_in + i * in1_stride, lab1_fmt);
        alwan__load3_typed(s2, (char const *)lab2_in + i * in2_stride, lab2_fmt);
        alwan_lab l1 = {s1[0], s1[1], s1[2]};
        alwan_lab l2 = {s2[0], s2[1], s2[2]};
        delta_e_out[i] = alwan_delta_e_cmc(&l1, &l2, l, c);
    }
    return ALWAN_OK;
}
