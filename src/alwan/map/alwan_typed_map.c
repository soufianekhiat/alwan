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

int alwan_collect3_f64(alwan_f64 *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format in_fmt) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_f64 s[3];
        alwan__load3_typed(s, (char const *)in + i * in_stride, in_fmt);
        alwan_f64 *op = (alwan_f64 *)((char *)out + i * out_stride);
        op[0] = s[0]; op[1] = s[1]; op[2] = s[2];
    }
    return ALWAN_OK;
}

int alwan_collect3_f32(alwan_f32 *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format in_fmt) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_f64 s[3];
        alwan__load3_typed(s, (char const *)in + i * in_stride, in_fmt);
        alwan_f32 *op = (alwan_f32 *)((char *)out + i * out_stride);
        op[0] = (alwan_f32)s[0]; op[1] = (alwan_f32)s[1]; op[2] = (alwan_f32)s[2];
    }
    return ALWAN_OK;
}

int alwan_scatter3_f64(void *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *ip = (alwan_f64 const *)((char const *)in + i * in_stride);
        alwan__store3_typed((char *)out + i * out_stride, ip, out_fmt);
    }
    return ALWAN_OK;
}

int alwan_scatter3_f32(void *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_f32 const *ip = (alwan_f32 const *)((char const *)in + i * in_stride);
        alwan_f64 s[3] = { (double)ip[0], (double)ip[1], (double)ip[2] };
        alwan__store3_typed((char *)out + i * out_stride, s, out_fmt);
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

ALWAN_EX_DELEGATE_DUAL(alwan_lab_to_lch_map_interleave_ex,    alwan_lab_to_lch_f32_map_interleave,    alwan_lab_to_lch_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_lch_to_lab_map_interleave_ex,    alwan_lch_to_lab_f32_map_interleave,    alwan_lch_to_lab_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_luv_to_lchuv_map_interleave_ex,  alwan_luv_to_lchuv_f32_map_interleave,  alwan_luv_to_lchuv_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_lchuv_to_luv_map_interleave_ex,  alwan_lchuv_to_luv_f32_map_interleave,  alwan_lchuv_to_luv_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_xyy_map_interleave_ex,    alwan_xyz_to_xyy_f32_map_interleave,    alwan_xyz_to_xyy_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_xyy_to_xyz_map_interleave_ex,    alwan_xyy_to_xyz_f32_map_interleave,    alwan_xyy_to_xyz_f64_map_interleave)

/* ----------------------------------------------------------------
 * Oklab _ex: XYZ<->Oklab moved to alwan_oklab_map.c (SIMD-accelerated)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_DUAL(alwan_oklab_to_oklch_map_interleave_ex, alwan_oklab_to_oklch_f32_map_interleave, alwan_oklab_to_oklch_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_oklch_to_oklab_map_interleave_ex, alwan_oklch_to_oklab_f32_map_interleave, alwan_oklch_to_oklab_f64_map_interleave)

/* ----------------------------------------------------------------
 * ICtCp _ex (with use_pq int param)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_DUAL_INT(alwan_rgb_to_ictcp_map_interleave_ex,  alwan_rgb_to_ictcp_f32_map_interleave,  alwan_rgb_to_ictcp_f64_map_interleave,  int, use_pq)
ALWAN_EX_DELEGATE_DUAL_INT(alwan_ictcp_to_rgb_map_interleave_ex,  alwan_ictcp_to_rgb_f32_map_interleave,  alwan_ictcp_to_rgb_f64_map_interleave,  int, use_pq)
ALWAN_EX_DELEGATE_DUAL_INT(alwan_xyz_to_ictcp_map_interleave_ex,  alwan_xyz_to_ictcp_f32_map_interleave,  alwan_xyz_to_ictcp_f64_map_interleave,  int, use_pq)
ALWAN_EX_DELEGATE_DUAL_INT(alwan_ictcp_to_xyz_map_interleave_ex,  alwan_ictcp_to_xyz_f32_map_interleave,  alwan_ictcp_to_xyz_f64_map_interleave,  int, use_pq)

/* ----------------------------------------------------------------
 * JzAzBz _ex (simple 3->3)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_jzazbz_map_interleave_ex,    alwan_xyz_to_jzazbz_f32_map_interleave,    alwan_xyz_to_jzazbz_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_jzazbz_to_xyz_map_interleave_ex,    alwan_jzazbz_to_xyz_f32_map_interleave,    alwan_jzazbz_to_xyz_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_jzazbz_to_jzczhz_map_interleave_ex, alwan_jzazbz_to_jzczhz_f32_map_interleave, alwan_jzazbz_to_jzczhz_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_jzczhz_to_jzazbz_map_interleave_ex, alwan_jzczhz_to_jzazbz_f32_map_interleave, alwan_jzczhz_to_jzazbz_f64_map_interleave)

/* ----------------------------------------------------------------
 * IPT _ex (simple 3->3)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_ipt_map_interleave_ex, alwan_xyz_to_ipt_f32_map_interleave, alwan_xyz_to_ipt_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_ipt_to_xyz_map_interleave_ex, alwan_ipt_to_xyz_f32_map_interleave, alwan_ipt_to_xyz_f64_map_interleave)

/* ----------------------------------------------------------------
 * Convenience HSV/HSL _ex
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_DUAL(alwan_rgb_to_hsv_map_interleave_ex, alwan_rgb_to_hsv_f32_map_interleave, alwan_rgb_to_hsv_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hsv_to_rgb_map_interleave_ex, alwan_hsv_to_rgb_f32_map_interleave, alwan_hsv_to_rgb_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_rgb_to_hsl_map_interleave_ex, alwan_rgb_to_hsl_f32_map_interleave, alwan_rgb_to_hsl_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hsl_to_rgb_map_interleave_ex, alwan_hsl_to_rgb_f32_map_interleave, alwan_hsl_to_rgb_f64_map_interleave)

/* ----------------------------------------------------------------
 * HSP/HSY _ex
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_DUAL(alwan_rgb_to_hsp_map_interleave_ex,    alwan_rgb_to_hsp_f32_map_interleave,    alwan_rgb_to_hsp_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hsp_to_rgb_map_interleave_ex,    alwan_hsp_to_rgb_f32_map_interleave,    alwan_hsp_to_rgb_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_rgb_to_hsplog_map_interleave_ex, alwan_rgb_to_hsplog_f32_map_interleave, alwan_rgb_to_hsplog_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hsplog_to_rgb_map_interleave_ex, alwan_hsplog_to_rgb_f32_map_interleave, alwan_hsplog_to_rgb_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_rgb_to_hsy_map_interleave_ex,    alwan_rgb_to_hsy_f32_map_interleave,    alwan_rgb_to_hsy_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hsy_to_rgb_map_interleave_ex,    alwan_hsy_to_rgb_f32_map_interleave,    alwan_hsy_to_rgb_f64_map_interleave)

/* ----------------------------------------------------------------
 * Linear sRGB <-> HSV/HSL _ex
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_DUAL(alwan_linear_srgb_to_hsv_map_interleave_ex, alwan_linear_srgb_to_hsv_f32_map_interleave, alwan_linear_srgb_to_hsv_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hsv_to_linear_srgb_map_interleave_ex, alwan_hsv_to_linear_srgb_f32_map_interleave, alwan_hsv_to_linear_srgb_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_linear_srgb_to_hsl_map_interleave_ex, alwan_linear_srgb_to_hsl_f32_map_interleave, alwan_linear_srgb_to_hsl_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hsl_to_linear_srgb_map_interleave_ex, alwan_hsl_to_linear_srgb_f32_map_interleave, alwan_hsl_to_linear_srgb_f64_map_interleave)

/* ----------------------------------------------------------------
 * Convenience extra _ex (CMY, YCoCg, HWB)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_DUAL(alwan_rgb_to_cmy_map_interleave_ex,   alwan_rgb_to_cmy_f32_map_interleave,   alwan_rgb_to_cmy_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_cmy_to_rgb_map_interleave_ex,   alwan_cmy_to_rgb_f32_map_interleave,   alwan_cmy_to_rgb_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_rgb_to_ycocg_map_interleave_ex, alwan_rgb_to_ycocg_f32_map_interleave, alwan_rgb_to_ycocg_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_ycocg_to_rgb_map_interleave_ex, alwan_ycocg_to_rgb_f32_map_interleave, alwan_ycocg_to_rgb_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_rgb_to_hwb_map_interleave_ex,   alwan_rgb_to_hwb_f32_map_interleave,   alwan_rgb_to_hwb_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hwb_to_rgb_map_interleave_ex,   alwan_hwb_to_rgb_f32_map_interleave,   alwan_hwb_to_rgb_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hsv_to_hwb_map_interleave_ex,   alwan_hsv_to_hwb_f32_map_interleave,   alwan_hsv_to_hwb_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hwb_to_hsv_map_interleave_ex,   alwan_hwb_to_hsv_f32_map_interleave,   alwan_hwb_to_hsv_f64_map_interleave)

/* ----------------------------------------------------------------
 * YCbCr _ex (with standard enum)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_DUAL_INT(alwan_rgb_to_ycbcr_map_interleave_ex,  alwan_rgb_to_ycbcr_f32_map_interleave,  alwan_rgb_to_ycbcr_f64_map_interleave,  alwan_ycbcr_standard, standard)
ALWAN_EX_DELEGATE_DUAL_INT(alwan_ycbcr_to_rgb_map_interleave_ex,  alwan_ycbcr_to_rgb_f32_map_interleave,  alwan_ycbcr_to_rgb_f64_map_interleave,  alwan_ycbcr_standard, standard)

/* ----------------------------------------------------------------
 * YcCbcCrc / legal-full _ex (with bit_depth)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_DUAL_INT(alwan_rgb_to_yccbccrc_map_interleave_ex,      alwan_rgb_to_yccbccrc_f32_map_interleave,      alwan_rgb_to_yccbccrc_f64_map_interleave,      int, bit_depth)
ALWAN_EX_DELEGATE_DUAL_INT(alwan_yccbccrc_to_rgb_map_interleave_ex,      alwan_yccbccrc_to_rgb_f32_map_interleave,      alwan_yccbccrc_to_rgb_f64_map_interleave,      int, bit_depth)
ALWAN_EX_DELEGATE_DUAL_INT(alwan_ycbcr_full_to_legal_map_interleave_ex,  alwan_ycbcr_full_to_legal_f32_map_interleave,  alwan_ycbcr_full_to_legal_f64_map_interleave,  int, bit_depth)
ALWAN_EX_DELEGATE_DUAL_INT(alwan_ycbcr_legal_to_full_map_interleave_ex,  alwan_ycbcr_legal_to_full_f32_map_interleave,  alwan_ycbcr_legal_to_full_f64_map_interleave,  int, bit_depth)

/* ----------------------------------------------------------------
 * Extended color spaces _ex (simple 3->3)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_igpgtg_map_interleave_ex,     alwan_xyz_to_igpgtg_f32_map_interleave,     alwan_xyz_to_igpgtg_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_igpgtg_to_xyz_map_interleave_ex,     alwan_igpgtg_to_xyz_f32_map_interleave,     alwan_igpgtg_to_xyz_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_icacb_map_interleave_ex,      alwan_xyz_to_icacb_f32_map_interleave,      alwan_xyz_to_icacb_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_icacb_to_xyz_map_interleave_ex,      alwan_icacb_to_xyz_f32_map_interleave,      alwan_icacb_to_xyz_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_hdr_cielab_map_interleave_ex, alwan_xyz_to_hdr_cielab_f32_map_interleave, alwan_xyz_to_hdr_cielab_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hdr_cielab_to_xyz_map_interleave_ex, alwan_hdr_cielab_to_xyz_f32_map_interleave, alwan_hdr_cielab_to_xyz_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_hdr_ipt_map_interleave_ex,    alwan_xyz_to_hdr_ipt_f32_map_interleave,    alwan_xyz_to_hdr_ipt_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hdr_ipt_to_xyz_map_interleave_ex,    alwan_hdr_ipt_to_xyz_f32_map_interleave,    alwan_hdr_ipt_to_xyz_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_ucs_map_interleave_ex,        alwan_xyz_to_ucs_f32_map_interleave,        alwan_xyz_to_ucs_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_ucs_to_xyz_map_interleave_ex,        alwan_ucs_to_xyz_f32_map_interleave,        alwan_ucs_to_xyz_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_osa_ucs_map_interleave_ex,    alwan_xyz_to_osa_ucs_f32_map_interleave,    alwan_xyz_to_osa_ucs_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_osa_ucs_to_xyz_map_interleave_ex,    alwan_osa_ucs_to_xyz_f32_map_interleave,    alwan_osa_ucs_to_xyz_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_hunter_lab_map_interleave_ex,  alwan_xyz_to_hunter_lab_f32_map_interleave,  alwan_xyz_to_hunter_lab_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hunter_lab_to_xyz_map_interleave_ex,  alwan_hunter_lab_to_xyz_f32_map_interleave,  alwan_hunter_lab_to_xyz_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_prolab_map_interleave_ex,      alwan_xyz_to_prolab_f32_map_interleave,      alwan_xyz_to_prolab_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_prolab_to_xyz_map_interleave_ex,      alwan_prolab_to_xyz_f32_map_interleave,      alwan_prolab_to_xyz_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_rgb_to_prismatic_map_interleave_ex,   alwan_rgb_to_prismatic_f32_map_interleave,   alwan_rgb_to_prismatic_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_prismatic_to_rgb_map_interleave_ex,   alwan_prismatic_to_rgb_f32_map_interleave,   alwan_prismatic_to_rgb_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_rgb_to_hcl_map_interleave_ex,         alwan_rgb_to_hcl_f32_map_interleave,         alwan_rgb_to_hcl_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_hcl_to_rgb_map_interleave_ex,         alwan_hcl_to_rgb_f32_map_interleave,         alwan_hcl_to_rgb_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_rgb_to_ihls_map_interleave_ex,        alwan_rgb_to_ihls_f32_map_interleave,        alwan_rgb_to_ihls_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL(alwan_ihls_to_rgb_map_interleave_ex,        alwan_ihls_to_rgb_f32_map_interleave,        alwan_ihls_to_rgb_f64_map_interleave)

/* Extended _ex with white point */

ALWAN_EX_DELEGATE_DUAL_WHITE(alwan_xyz_to_uvw_map_interleave_ex,              alwan_xyz_to_uvw_f32_map_interleave,              alwan_xyz_to_uvw_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL_WHITE(alwan_uvw_to_xyz_map_interleave_ex,              alwan_uvw_to_xyz_f32_map_interleave,              alwan_uvw_to_xyz_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL_WHITE(alwan_xyz_to_hunter_lab_custom_map_interleave_ex, alwan_xyz_to_hunter_lab_custom_f32_map_interleave, alwan_xyz_to_hunter_lab_custom_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL_WHITE(alwan_hunter_lab_to_xyz_custom_map_interleave_ex, alwan_hunter_lab_to_xyz_custom_f32_map_interleave, alwan_hunter_lab_to_xyz_custom_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL_WHITE(alwan_xyz_to_prolab_custom_map_interleave_ex,     alwan_xyz_to_prolab_custom_f32_map_interleave,     alwan_xyz_to_prolab_custom_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL_WHITE(alwan_prolab_to_xyz_custom_map_interleave_ex,     alwan_prolab_to_xyz_custom_f32_map_interleave,     alwan_prolab_to_xyz_custom_f64_map_interleave)

/* DIN99 _ex (with int variant) */

ALWAN_EX_DELEGATE_DUAL_INT(alwan_lab_to_din99_map_interleave_ex, alwan_lab_to_din99_f32_map_interleave, alwan_lab_to_din99_f64_map_interleave, int, variant)
ALWAN_EX_DELEGATE_DUAL_INT(alwan_din99_to_lab_map_interleave_ex, alwan_din99_to_lab_f32_map_interleave, alwan_din99_to_lab_f64_map_interleave, int, variant)

/* ----------------------------------------------------------------
 * Color correction _ex (tiled delegation to native functions)
 * ---------------------------------------------------------------- */

int alwan_lgg_apply_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_rgb_f64 const *lift, alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain) {
    if (!in || !out || !lift || !gamma || !gain || count == 0) return ALWAN_E_INVALID;
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        alwan_rgb_f64 lift64 = {(double)lift->r, (double)lift->g, (double)lift->b};
        alwan_rgb_f64 gamma64 = {(double)gamma->r, (double)gamma->g, (double)gamma->b};
        alwan_rgb_f64 gain64 = {(double)gain->r, (double)gain->g, (double)gain->b};
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3];
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3];
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_lgg_apply_f64_map_interleave(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_, &gain64, &gamma64, &lift64);
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
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
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3];
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3];
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_lgg_apply_f32_map_interleave(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_, &gain32, &gamma32, &lift32);
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    }
    return ALWAN_OK;
}

int alwan_color_matrix_apply_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_mat3x3_f64 const *matrix) {
    if (!in || !out || !matrix || count == 0) return ALWAN_E_INVALID;
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        alwan_mat3x3_f64 m64; for (int i = 0; i < 9; i++) m64.m[i] = (double)matrix->m[i];
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3];
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3];
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_color_matrix_apply_f64_map_interleave(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_, &m64);
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    } else {
        alwan_mat3x3_f32 m32; for (int i = 0; i < 9; i++) m32.m[i] = (float)matrix->m[i];
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3];
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3];
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_color_matrix_apply_f32_map_interleave(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_, &m32);
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    }
    return ALWAN_OK;
}

int alwan_printer_lights_apply_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 red_lights, alwan_f64 green_lights, alwan_f64 blue_lights) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3];
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3];
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_printer_lights_apply_f64_map_interleave(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_, (double)blue_lights, (double)green_lights, (double)red_lights);
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    } else {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3];
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3];
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_printer_lights_apply_f32_map_interleave(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_, (float)blue_lights, (float)green_lights, (float)red_lights);
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    }
    return ALWAN_OK;
}

int alwan_white_balance_apply_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_rgb_f64 const *multipliers) {
    if (!in || !out || !multipliers || count == 0) return ALWAN_E_INVALID;
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        alwan_rgb_f64 mul64 = {(double)multipliers->r, (double)multipliers->g, (double)multipliers->b};
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3];
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3];
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_white_balance_apply_f64_map_interleave(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_, &mul64);
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    } else {
        alwan_rgb_f32 mul32 = {(float)multipliers->r, (float)multipliers->g, (float)multipliers->b};
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3];
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3];
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_white_balance_apply_f32_map_interleave(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_, &mul32);
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CVD _ex (with severity)
 * ---------------------------------------------------------------- */

ALWAN_EX_DELEGATE_DUAL_SCALAR(alwan_simulate_protanopia_map_interleave_ex,   alwan_simulate_protanopia_f32_map_interleave,   alwan_simulate_protanopia_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL_SCALAR(alwan_simulate_deuteranopia_map_interleave_ex, alwan_simulate_deuteranopia_f32_map_interleave, alwan_simulate_deuteranopia_f64_map_interleave)
ALWAN_EX_DELEGATE_DUAL_SCALAR(alwan_simulate_tritanopia_map_interleave_ex,   alwan_simulate_tritanopia_f32_map_interleave,   alwan_simulate_tritanopia_f64_map_interleave)

int alwan_simulate_cvd_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_cvd_type cvd_type, alwan_f64 severity) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3];
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3];
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_simulate_cvd_f64_map_interleave(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_, cvd_type, (double)severity);
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    } else {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3];
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3];
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_simulate_cvd_f32_map_interleave(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_, cvd_type, (float)severity);
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CVD Machado _ex
 * ---------------------------------------------------------------- */

int alwan_simulate_cvd_machado_map_interleave_ex(void *rgb_out, size_t out_stride, void const *rgb_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_cvd_type cvd_type, alwan_f64 severity) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3];
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3];
            alwan__load_tile_typed_aos_f64(ibuf_, rgb_in, in_fmt, off_, in_stride, tile_, 3);
            alwan_simulate_cvd_machado_f64_map_interleave(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_, cvd_type, (double)severity);
            alwan__store_tile_typed_aos_f64(rgb_out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    } else {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3];
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3];
            alwan__load_tile_typed_aos_f32(ibuf_, rgb_in, in_fmt, off_, in_stride, tile_, 3);
            alwan_simulate_cvd_machado_f32_map_interleave(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_, cvd_type, (float)severity);
            alwan__store_tile_typed_aos_f32(rgb_out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CMY <-> CMYK _ex (3<->4 channel, tiled delegation)
 * ---------------------------------------------------------------- */

int alwan_cmy_to_cmyk_map_interleave_ex(void *cmyk_out, size_t out_stride, void const *cmy_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt) {
    if (!cmy_in || !cmyk_out || count == 0) return ALWAN_E_INVALID;
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3];
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 4];
            alwan__load_tile_typed_aos_f64(ibuf_, cmy_in, in_fmt, off_, in_stride, tile_, 3);
            alwan_cmy_to_cmyk_f64_map_interleave(obuf_, 4 * sizeof(double), ibuf_, 3 * sizeof(double), tile_);
            alwan__store_tile_typed_aos_f64(cmyk_out, out_fmt, off_, out_stride, obuf_, tile_, 4);
            off_ += tile_;
        }
    } else {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3];
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 4];
            alwan__load_tile_typed_aos_f32(ibuf_, cmy_in, in_fmt, off_, in_stride, tile_, 3);
            alwan_cmy_to_cmyk_f32_map_interleave(obuf_, 4 * sizeof(float), ibuf_, 3 * sizeof(float), tile_);
            alwan__store_tile_typed_aos_f32(cmyk_out, out_fmt, off_, out_stride, obuf_, tile_, 4);
            off_ += tile_;
        }
    }
    return ALWAN_OK;
}

int alwan_cmyk_to_cmy_map_interleave_ex(void *cmy_out, size_t out_stride, void const *cmyk_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt) {
    if (!cmyk_in || !cmy_out || count == 0) return ALWAN_E_INVALID;
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 4];
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3];
            alwan__load_tile_typed_aos_f64(ibuf_, cmyk_in, in_fmt, off_, in_stride, tile_, 4);
            alwan_cmyk_to_cmy_f64_map_interleave(obuf_, 3 * sizeof(double), ibuf_, 4 * sizeof(double), tile_);
            alwan__store_tile_typed_aos_f64(cmy_out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    } else {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 4];
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3];
            alwan__load_tile_typed_aos_f32(ibuf_, cmyk_in, in_fmt, off_, in_stride, tile_, 4);
            alwan_cmyk_to_cmy_f32_map_interleave(obuf_, 3 * sizeof(float), ibuf_, 4 * sizeof(float), tile_);
            alwan__store_tile_typed_aos_f32(cmy_out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM _ex (typed XYZ side, struct correlates side)
 * These have non-standard signatures (one side struct, one side typed)
 * and per-pixel error returns, so they remain per-pixel.
 * ---------------------------------------------------------------- */

int alwan_ciecam02_forward_map_interleave_ex(alwan_ciecam02_correlates_f64 *correlates_out, void const *xyz_in, size_t in_stride, alwan_ciecam02_viewing_conditions_f64 const *vc, size_t count, alwan_pixel_format in_fmt) {
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_f64 s[3];
        alwan__load3_typed(s, (char const *)xyz_in + i * in_stride, in_fmt);
        alwan_xyz_f64 xyz = {s[0], s[1], s[2]};
        int st = alwan_ciecam02_forward_f64(&correlates_out[i], &xyz, vc);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

int alwan_ciecam02_inverse_map_interleave_ex(void *xyz_out, size_t out_stride, alwan_ciecam02_correlates_f64 const *correlates_in, alwan_ciecam02_viewing_conditions_f64 const *vc, size_t count, alwan_pixel_format out_fmt) {
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_xyz_f64 xyz;
        int st = alwan_ciecam02_inverse_f64(&xyz, &correlates_in[i], vc);
        if (st != ALWAN_OK) return st;
        alwan_f64 d[3] = {xyz.x, xyz.y, xyz.z};
        alwan__store3_typed((char *)xyz_out + i * out_stride, d, out_fmt);
    }
    return ALWAN_OK;
}

int alwan_cam16_forward_map_interleave_ex(alwan_cam16_correlates_f64 *correlates_out, void const *xyz_in, size_t in_stride, alwan_cam16_viewing_conditions_f64 const *vc, size_t count, alwan_pixel_format in_fmt) {
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_f64 s[3];
        alwan__load3_typed(s, (char const *)xyz_in + i * in_stride, in_fmt);
        alwan_xyz_f64 xyz = {s[0], s[1], s[2]};
        int st = alwan_cam16_forward_f64(&correlates_out[i], &xyz, vc);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

int alwan_cam16_inverse_map_interleave_ex(void *xyz_out, size_t out_stride, alwan_cam16_correlates_f64 const *correlates_in, alwan_cam16_viewing_conditions_f64 const *vc, size_t count, alwan_pixel_format out_fmt) {
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_xyz_f64 xyz;
        int st = alwan_cam16_inverse_f64(&xyz, &correlates_in[i], vc);
        if (st != ALWAN_OK) return st;
        alwan_f64 d[3] = {xyz.x, xyz.y, xyz.z};
        alwan__store3_typed((char *)xyz_out + i * out_stride, d, out_fmt);
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Delta E batch _ex (typed Lab inputs)
 * Two typed inputs -> scalar output; remain per-pixel.
 * ---------------------------------------------------------------- */

int alwan_delta_e_76_batch_ex(alwan_f64 *delta_e_out,
                               void const *lab1_in, size_t in1_stride,
                               void const *lab2_in, size_t in2_stride,
                               size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_f64 s1[3], s2[3];
        alwan__load3_typed(s1, (char const *)lab1_in + i * in1_stride, lab1_fmt);
        alwan__load3_typed(s2, (char const *)lab2_in + i * in2_stride, lab2_fmt);
        alwan_lab_f64 l1 = {s1[0], s1[1], s1[2]};
        alwan_lab_f64 l2 = {s2[0], s2[1], s2[2]};
        delta_e_out[i] = alwan_delta_e_76_f64(&l1, &l2);
    }
    return ALWAN_OK;
}

int alwan_delta_e_2000_batch_ex(alwan_f64 *delta_e_out,
                                 void const *lab1_in, size_t in1_stride,
                                 void const *lab2_in, size_t in2_stride,
                                 size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_f64 s1[3], s2[3];
        alwan__load3_typed(s1, (char const *)lab1_in + i * in1_stride, lab1_fmt);
        alwan__load3_typed(s2, (char const *)lab2_in + i * in2_stride, lab2_fmt);
        alwan_lab_f64 l1 = {s1[0], s1[1], s1[2]};
        alwan_lab_f64 l2 = {s2[0], s2[1], s2[2]};
        delta_e_out[i] = alwan_delta_e_2000_f64(&l1, &l2);
    }
    return ALWAN_OK;
}

int alwan_delta_e_94_batch_ex(alwan_f64 *delta_e_out,
                               void const *lab1_in, size_t in1_stride,
                               void const *lab2_in, size_t in2_stride,
                               size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_f64 s1[3], s2[3];
        alwan__load3_typed(s1, (char const *)lab1_in + i * in1_stride, lab1_fmt);
        alwan__load3_typed(s2, (char const *)lab2_in + i * in2_stride, lab2_fmt);
        alwan_lab_f64 l1 = {s1[0], s1[1], s1[2]};
        alwan_lab_f64 l2 = {s2[0], s2[1], s2[2]};
        delta_e_out[i] = alwan_delta_e_94_f64(&l1, &l2);
    }
    return ALWAN_OK;
}

int alwan_delta_e_cmc_batch_ex(alwan_f64 *delta_e_out,
                                void const *lab1_in, size_t in1_stride,
                                void const *lab2_in, size_t in2_stride,
                                size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt,
                                alwan_f64 l, alwan_f64 c) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_f64 s1[3], s2[3];
        alwan__load3_typed(s1, (char const *)lab1_in + i * in1_stride, lab1_fmt);
        alwan__load3_typed(s2, (char const *)lab2_in + i * in2_stride, lab2_fmt);
        alwan_lab_f64 l1 = {s1[0], s1[1], s1[2]};
        alwan_lab_f64 l2 = {s2[0], s2[1], s2[2]};
        alwan_delta_e_cmc_params_f64 cmc_p; cmc_p.l = (double)l; cmc_p.c = (double)c;
        delta_e_out[i] = alwan_delta_e_cmc_f64(&l1, &l2, &cmc_p);
    }
    return ALWAN_OK;
}
