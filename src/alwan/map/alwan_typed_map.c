/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Typed Map Functions (_ex variants)
 * Accept void* buffers with alwan_pixel_format for u8/u16/f32/f64 I/O
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

/* Color Space _ex (pattern A: simple 3->3) */

ALWAN_MAP3_EX(alwan_lab_to_lch_map_interleave_ex,    alwan_lab,   alwan_lch,   alwan_lab_to_lch,     L,a,b, L,C,h)
ALWAN_MAP3_EX(alwan_lch_to_lab_map_interleave_ex,    alwan_lch,   alwan_lab,   alwan_lch_to_lab,     L,C,h, L,a,b)
ALWAN_MAP3_EX(alwan_luv_to_lchuv_map_interleave_ex,  alwan_luv,   alwan_lchuv, alwan_luv_to_lchuv,   L,u,v, L,C,h)
ALWAN_MAP3_EX(alwan_lchuv_to_luv_map_interleave_ex,  alwan_lchuv, alwan_luv,   alwan_lchuv_to_luv,   L,C,h, L,u,v)
ALWAN_MAP3_EX(alwan_xyz_to_xyy_map_interleave_ex,    alwan_xyz,   alwan_xyy,   alwan_xyz_to_xyy,     x,y,z, x,y,Y)
ALWAN_MAP3_EX(alwan_xyy_to_xyz_map_interleave_ex,    alwan_xyy,   alwan_xyz,   alwan_xyy_to_xyz,     x,y,Y, x,y,z)

/* ----------------------------------------------------------------
 * Oklab _ex: XYZ<->Oklab moved to alwan_oklab_map.c (SIMD-accelerated)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX(alwan_oklab_to_oklch_map_interleave_ex, alwan_oklab, alwan_oklch, alwan_oklab_to_oklch, L,a,b, L,C,h)
ALWAN_MAP3_EX(alwan_oklch_to_oklab_map_interleave_ex, alwan_oklch, alwan_oklab, alwan_oklch_to_oklab, L,C,h, L,a,b)

/* ----------------------------------------------------------------
 * ICtCp _ex (pattern C: with use_pq)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX_PQ(alwan_rgb_to_ictcp_map_interleave_ex,  alwan_rgb,   alwan_ictcp, alwan_rgb_to_ictcp,  r,g,b, I,Ct,Cp)
ALWAN_MAP3_EX_PQ(alwan_ictcp_to_rgb_map_interleave_ex,  alwan_ictcp, alwan_rgb,   alwan_ictcp_to_rgb,  I,Ct,Cp, r,g,b)
ALWAN_MAP3_EX_PQ(alwan_xyz_to_ictcp_map_interleave_ex,  alwan_xyz,   alwan_ictcp, alwan_xyz_to_ictcp,  x,y,z, I,Ct,Cp)
ALWAN_MAP3_EX_PQ(alwan_ictcp_to_xyz_map_interleave_ex,  alwan_ictcp, alwan_xyz,   alwan_ictcp_to_xyz,  I,Ct,Cp, x,y,z)

/* ----------------------------------------------------------------
 * JzAzBz _ex (pattern A)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX(alwan_xyz_to_jzazbz_map_interleave_ex,    alwan_xyz,    alwan_jzazbz, alwan_xyz_to_jzazbz,    x,y,z,    Jz,az,bz)
ALWAN_MAP3_EX(alwan_jzazbz_to_xyz_map_interleave_ex,    alwan_jzazbz, alwan_xyz,    alwan_jzazbz_to_xyz,    Jz,az,bz, x,y,z)
ALWAN_MAP3_EX(alwan_jzazbz_to_jzczhz_map_interleave_ex, alwan_jzazbz, alwan_jzczhz, alwan_jzazbz_to_jzczhz, Jz,az,bz, Jz,Cz,hz)
ALWAN_MAP3_EX(alwan_jzczhz_to_jzazbz_map_interleave_ex, alwan_jzczhz, alwan_jzazbz, alwan_jzczhz_to_jzazbz, Jz,Cz,hz, Jz,az,bz)

/* ----------------------------------------------------------------
 * IPT _ex (pattern A)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX(alwan_xyz_to_ipt_map_interleave_ex, alwan_xyz, alwan_ipt, alwan_xyz_to_ipt, x,y,z, I,P,T)
ALWAN_MAP3_EX(alwan_ipt_to_xyz_map_interleave_ex, alwan_ipt, alwan_xyz, alwan_ipt_to_xyz, I,P,T, x,y,z)

/* ----------------------------------------------------------------
 * Convenience HSV/HSL _ex (pattern D: with status)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX_STATUS(alwan_rgb_to_hsv_map_interleave_ex, alwan_rgb, alwan_hsv, alwan_rgb_to_hsv, r,g,b, h,s,v)
ALWAN_MAP3_EX_STATUS(alwan_hsv_to_rgb_map_interleave_ex, alwan_hsv, alwan_rgb, alwan_hsv_to_rgb, h,s,v, r,g,b)
ALWAN_MAP3_EX_STATUS(alwan_rgb_to_hsl_map_interleave_ex, alwan_rgb, alwan_hsl, alwan_rgb_to_hsl, r,g,b, h,s,l)
ALWAN_MAP3_EX_STATUS(alwan_hsl_to_rgb_map_interleave_ex, alwan_hsl, alwan_rgb, alwan_hsl_to_rgb, h,s,l, r,g,b)

/* ----------------------------------------------------------------
 * HSP/HSY _ex
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX_STATUS(alwan_rgb_to_hsp_map_interleave_ex, alwan_rgb, alwan_hsp, alwan_rgb_to_hsp, r,g,b, h,s,p)
ALWAN_MAP3_EX_STATUS(alwan_hsp_to_rgb_map_interleave_ex, alwan_hsp, alwan_rgb, alwan_hsp_to_rgb, h,s,p, r,g,b)
ALWAN_MAP3_EX_STATUS(alwan_rgb_to_hsplog_map_interleave_ex, alwan_rgb, alwan_hsplog, alwan_rgb_to_hsplog, r,g,b, h,s,p)
ALWAN_MAP3_EX_STATUS(alwan_hsplog_to_rgb_map_interleave_ex, alwan_hsplog, alwan_rgb, alwan_hsplog_to_rgb, h,s,p, r,g,b)
ALWAN_MAP3_EX_STATUS(alwan_rgb_to_hsy_map_interleave_ex, alwan_rgb, alwan_hsy, alwan_rgb_to_hsy, r,g,b, h,s,y)
ALWAN_MAP3_EX_STATUS(alwan_hsy_to_rgb_map_interleave_ex, alwan_hsy, alwan_rgb, alwan_hsy_to_rgb, h,s,y, r,g,b)

/* ----------------------------------------------------------------
 * Linear sRGB <-> HSV/HSL _ex
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX_STATUS(alwan_linear_srgb_to_hsv_map_interleave_ex, alwan_rgb, alwan_hsv, alwan_linear_srgb_to_hsv, r,g,b, h,s,v)
ALWAN_MAP3_EX_STATUS(alwan_hsv_to_linear_srgb_map_interleave_ex, alwan_hsv, alwan_rgb, alwan_hsv_to_linear_srgb, h,s,v, r,g,b)
ALWAN_MAP3_EX_STATUS(alwan_linear_srgb_to_hsl_map_interleave_ex, alwan_rgb, alwan_hsl, alwan_linear_srgb_to_hsl, r,g,b, h,s,l)
ALWAN_MAP3_EX_STATUS(alwan_hsl_to_linear_srgb_map_interleave_ex, alwan_hsl, alwan_rgb, alwan_hsl_to_linear_srgb, h,s,l, r,g,b)

/* ----------------------------------------------------------------
 * Convenience extra _ex (CMY, YCoCg, HWB - using _v core functions)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX_V(alwan_rgb_to_cmy_map_interleave_ex,   alwan_rgb, alwan_cmy, alwan_rgb_to_cmy_v, r,g,b, c,m,y)
ALWAN_MAP3_EX_V(alwan_cmy_to_rgb_map_interleave_ex,   alwan_cmy, alwan_rgb, alwan_cmy_to_rgb_v, c,m,y, r,g,b)
int alwan_rgb_to_ycocg_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        for (size_t i = 0; i < tile; i++) {
            alwan_rgb src_ = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_ycocg dst_ = alwan_rgb_to_ycocg_v(src_);
            d0[i] = (alwan_simd_lane)dst_.Y; d1[i] = (alwan_simd_lane)dst_.Co; d2[i] = (alwan_simd_lane)dst_.Cg;
        }
        ALWAN_MAP_NORM_ADD(d1, tile, 0.5);
        ALWAN_MAP_NORM_ADD(d2, tile, 0.5);
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_ycocg_to_rgb_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        ALWAN_MAP_NORM_ADD(c1, tile, -0.5);
        ALWAN_MAP_NORM_ADD(c2, tile, -0.5);
        for (size_t i = 0; i < tile; i++) {
            alwan_ycocg src_ = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb dst_ = alwan_ycocg_to_rgb_v(src_);
            d0[i] = (alwan_simd_lane)dst_.r; d1[i] = (alwan_simd_lane)dst_.g; d2[i] = (alwan_simd_lane)dst_.b;
        }
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}
ALWAN_MAP3_EX_V(alwan_rgb_to_hwb_map_interleave_ex,   alwan_rgb, alwan_hwb, alwan_rgb_to_hwb_v, r,g,b, h,w,b)
ALWAN_MAP3_EX_V(alwan_hwb_to_rgb_map_interleave_ex,   alwan_hwb, alwan_rgb, alwan_hwb_to_rgb_v, h,w,b, r,g,b)
ALWAN_MAP3_EX_V(alwan_hsv_to_hwb_map_interleave_ex,   alwan_hsv, alwan_hwb, alwan_hsv_to_hwb_v, h,s,v, h,w,b)
ALWAN_MAP3_EX_V(alwan_hwb_to_hsv_map_interleave_ex,   alwan_hwb, alwan_hsv, alwan_hwb_to_hsv_v, h,w,b, h,s,v)

/* ----------------------------------------------------------------
 * YCbCr _ex (with standard enum)
 * ---------------------------------------------------------------- */

int alwan_rgb_to_ycbcr_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               alwan_ycbcr_standard standard,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    alwan_scalar kr, kb;
    alwan__get_ycbcr_coeffs(standard, &kr, &kb);
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        for (size_t i = 0; i < tile; i++) {
            alwan_rgb rgb = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_ycbcr dst = alwan_rgb_to_ycbcr_kr_kb_v(rgb, kr, kb);
            d0[i] = (alwan_simd_lane)dst.Y; d1[i] = (alwan_simd_lane)dst.Cb; d2[i] = (alwan_simd_lane)dst.Cr;
        }
        ALWAN_MAP_NORM_ADD(d1, tile, 0.5);
        ALWAN_MAP_NORM_ADD(d2, tile, 0.5);
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_ycbcr_to_rgb_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               alwan_ycbcr_standard standard,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    alwan_scalar kr, kb;
    alwan__get_ycbcr_coeffs(standard, &kr, &kb);
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        ALWAN_MAP_NORM_ADD(c1, tile, -0.5);
        ALWAN_MAP_NORM_ADD(c2, tile, -0.5);
        for (size_t i = 0; i < tile; i++) {
            alwan_ycbcr ycbcr = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb dst = alwan_ycbcr_to_rgb_kr_kb_v(ycbcr, kr, kb);
            d0[i] = (alwan_simd_lane)dst.r; d1[i] = (alwan_simd_lane)dst.g; d2[i] = (alwan_simd_lane)dst.b;
        }
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * YcCbcCrc / legal-full _ex (with bit_depth)
 * ---------------------------------------------------------------- */

int alwan_rgb_to_yccbccrc_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               int bit_depth,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        for (size_t i = 0; i < tile; i++) {
            alwan_rgb src_ = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_yccbccrc dst_ = alwan_rgb_to_yccbccrc_v(src_, bit_depth);
            d0[i] = (alwan_simd_lane)dst_.Yc; d1[i] = (alwan_simd_lane)dst_.Cbc; d2[i] = (alwan_simd_lane)dst_.Crc;
        }
        ALWAN_MAP_NORM_ADD(d1, tile, 0.5);
        ALWAN_MAP_NORM_ADD(d2, tile, 0.5);
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_yccbccrc_to_rgb_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               int bit_depth,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        ALWAN_MAP_NORM_ADD(c1, tile, -0.5);
        ALWAN_MAP_NORM_ADD(c2, tile, -0.5);
        for (size_t i = 0; i < tile; i++) {
            alwan_yccbccrc src_ = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb dst_ = alwan_yccbccrc_to_rgb_v(src_, bit_depth);
            d0[i] = (alwan_simd_lane)dst_.r; d1[i] = (alwan_simd_lane)dst_.g; d2[i] = (alwan_simd_lane)dst_.b;
        }
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}
ALWAN_MAP3_EX_V_INT(alwan_ycbcr_full_to_legal_map_interleave_ex, alwan_ycbcr, alwan_ycbcr, alwan_ycbcr_full_to_legal_v, int, bit_depth, Y,Cb,Cr, Y,Cb,Cr)
ALWAN_MAP3_EX_V_INT(alwan_ycbcr_legal_to_full_map_interleave_ex, alwan_ycbcr, alwan_ycbcr, alwan_ycbcr_legal_to_full_v, int, bit_depth, Y,Cb,Cr, Y,Cb,Cr)

/* ----------------------------------------------------------------
 * Extended color spaces _ex (simple 3->3, using _v core functions)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX_V(alwan_xyz_to_igpgtg_map_interleave_ex,     alwan_xyz,    alwan_igpgtg,    alwan_xyz_to_igpgtg_v,     x,y,z,    Ig,Pg,Tg)
ALWAN_MAP3_EX_V(alwan_igpgtg_to_xyz_map_interleave_ex,     alwan_igpgtg, alwan_xyz,       alwan_igpgtg_to_xyz_v,     Ig,Pg,Tg, x,y,z)
ALWAN_MAP3_EX_V(alwan_xyz_to_icacb_map_interleave_ex,      alwan_xyz,    alwan_icacb,     alwan_xyz_to_icacb_v,      x,y,z,    I,Ca,Cb)
ALWAN_MAP3_EX_V(alwan_icacb_to_xyz_map_interleave_ex,      alwan_icacb,  alwan_xyz,       alwan_icacb_to_xyz_v,      I,Ca,Cb,  x,y,z)
ALWAN_MAP3_EX_V(alwan_xyz_to_hdr_cielab_map_interleave_ex, alwan_xyz,    alwan_lab,       alwan_xyz_to_hdr_cielab_v, x,y,z,    L,a,b)
ALWAN_MAP3_EX_V(alwan_hdr_cielab_to_xyz_map_interleave_ex, alwan_lab,    alwan_xyz,       alwan_hdr_cielab_to_xyz_v, L,a,b,    x,y,z)
ALWAN_MAP3_EX_V(alwan_xyz_to_hdr_ipt_map_interleave_ex,    alwan_xyz,    alwan_ipt,       alwan_xyz_to_hdr_ipt_v,    x,y,z,    I,P,T)
ALWAN_MAP3_EX_V(alwan_hdr_ipt_to_xyz_map_interleave_ex,    alwan_ipt,    alwan_xyz,       alwan_hdr_ipt_to_xyz_v,    I,P,T,    x,y,z)
ALWAN_MAP3_EX_V(alwan_xyz_to_ucs_map_interleave_ex,        alwan_xyz,    alwan_ucs,       alwan_xyz_to_ucs_v,        x,y,z,    U,V,W)
ALWAN_MAP3_EX_V(alwan_ucs_to_xyz_map_interleave_ex,        alwan_ucs,    alwan_xyz,       alwan_ucs_to_xyz_v,        U,V,W,    x,y,z)
ALWAN_MAP3_EX_V(alwan_xyz_to_osa_ucs_map_interleave_ex,    alwan_xyz,    alwan_osa_ucs,   alwan_xyz_to_osa_ucs_v,    x,y,z,    L,j,g)
ALWAN_MAP3_EX_V(alwan_osa_ucs_to_xyz_map_interleave_ex,    alwan_osa_ucs,alwan_xyz,       alwan_osa_ucs_to_xyz_v,    L,j,g,    x,y,z)
ALWAN_MAP3_EX_V(alwan_xyz_to_hunter_lab_map_interleave_ex,  alwan_xyz,   alwan_hunter_lab, alwan_xyz_to_hunter_lab_v, x,y,z,    L,a,b)
ALWAN_MAP3_EX_V(alwan_hunter_lab_to_xyz_map_interleave_ex,  alwan_hunter_lab, alwan_xyz,   alwan_hunter_lab_to_xyz_v, L,a,b,    x,y,z)
ALWAN_MAP3_EX_V(alwan_xyz_to_prolab_map_interleave_ex,      alwan_xyz,   alwan_prolab,    alwan_xyz_to_prolab_v,     x,y,z,    L,a,b)
ALWAN_MAP3_EX_V(alwan_prolab_to_xyz_map_interleave_ex,      alwan_prolab, alwan_xyz,      alwan_prolab_to_xyz_v,     L,a,b,    x,y,z)
ALWAN_MAP3_EX_V(alwan_rgb_to_prismatic_map_interleave_ex,   alwan_rgb,   alwan_prismatic, alwan_rgb_to_prismatic_v,  r,g,b,    L,s,h)
ALWAN_MAP3_EX_V(alwan_prismatic_to_rgb_map_interleave_ex,   alwan_prismatic, alwan_rgb,   alwan_prismatic_to_rgb_v,  L,s,h,    r,g,b)
int alwan_rgb_to_hcl_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        for (size_t i = 0; i < tile; i++) {
            alwan_rgb src_ = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_hcl dst_ = alwan_rgb_to_hcl_v(src_);
            d0[i] = (alwan_simd_lane)dst_.H; d1[i] = (alwan_simd_lane)dst_.C; d2[i] = (alwan_simd_lane)dst_.L;
        }
        ALWAN_MAP_NORM_AFFINE(d0, tile, 0.15915494309189533577, 0.5);
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_hcl_to_rgb_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        ALWAN_MAP_NORM_AFFINE(c0, tile, 6.28318530717958647692, -3.14159265358979323846);
        for (size_t i = 0; i < tile; i++) {
            alwan_hcl src_ = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb dst_ = alwan_hcl_to_rgb_v(src_);
            d0[i] = (alwan_simd_lane)dst_.r; d1[i] = (alwan_simd_lane)dst_.g; d2[i] = (alwan_simd_lane)dst_.b;
        }
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_rgb_to_ihls_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        for (size_t i = 0; i < tile; i++) {
            alwan_rgb src_ = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_ihls dst_ = alwan_rgb_to_ihls_v(src_);
            d0[i] = (alwan_simd_lane)dst_.H; d1[i] = (alwan_simd_lane)dst_.L; d2[i] = (alwan_simd_lane)dst_.S;
        }
        ALWAN_MAP_NORM_MUL(d0, tile, 0.15915494309189533577);
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_ihls_to_rgb_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 6.28318530717958647692);
        for (size_t i = 0; i < tile; i++) {
            alwan_ihls src_ = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb dst_ = alwan_ihls_to_rgb_v(src_);
            d0[i] = (alwan_simd_lane)dst_.r; d1[i] = (alwan_simd_lane)dst_.g; d2[i] = (alwan_simd_lane)dst_.b;
        }
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* Extended _ex with white point */

ALWAN_MAP3_EX_V_WHITE(alwan_xyz_to_uvw_map_interleave_ex,              alwan_xyz, alwan_uvw,        alwan_xyz_to_uvw_v,                x,y,z, U,V,W)
ALWAN_MAP3_EX_V_WHITE(alwan_uvw_to_xyz_map_interleave_ex,              alwan_uvw, alwan_xyz,        alwan_uvw_to_xyz_v,                U,V,W, x,y,z)
ALWAN_MAP3_EX_V_WHITE(alwan_xyz_to_hunter_lab_custom_map_interleave_ex, alwan_xyz, alwan_hunter_lab, alwan_xyz_to_hunter_lab_custom_v, x,y,z, L,a,b)
ALWAN_MAP3_EX_V_WHITE(alwan_hunter_lab_to_xyz_custom_map_interleave_ex, alwan_hunter_lab, alwan_xyz, alwan_hunter_lab_to_xyz_custom_v, L,a,b, x,y,z)
ALWAN_MAP3_EX_V_WHITE(alwan_xyz_to_prolab_custom_map_interleave_ex,     alwan_xyz, alwan_prolab,    alwan_xyz_to_prolab_custom_v,     x,y,z, L,a,b)
ALWAN_MAP3_EX_V_WHITE(alwan_prolab_to_xyz_custom_map_interleave_ex,     alwan_prolab, alwan_xyz,    alwan_prolab_to_xyz_custom_v,     L,a,b, x,y,z)

/* DIN99 _ex (with int variant) */

ALWAN_MAP3_EX_V_INT(alwan_lab_to_din99_map_interleave_ex, alwan_lab, alwan_din99, alwan_lab_to_din99_v, int, variant, L,a,b, L99,a99,b99)
ALWAN_MAP3_EX_V_INT(alwan_din99_to_lab_map_interleave_ex, alwan_din99, alwan_lab, alwan_din99_to_lab_v, int, variant, L99,a99,b99, L,a,b)

/* ----------------------------------------------------------------
 * Color correction _ex
 * ---------------------------------------------------------------- */

int alwan_lgg_apply_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                            void const *in, alwan_pixel_format in_fmt,
                            alwan_rgb const *lift, alwan_rgb const *gamma, alwan_rgb const *gain,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || !lift || !gamma || !gain || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar sv[3];
        alwan__load3_typed(sv, (char const *)in + i * in_stride, in_fmt);
        alwan_rgb rgb = {sv[0], sv[1], sv[2]};
        alwan_rgb r = alwan_lgg_apply_v(rgb, *lift, *gamma, *gain);
        alwan_scalar dv[3] = {r.r, r.g, r.b};
        alwan__store3_typed((char *)out + i * out_stride, dv, out_fmt);
    }
    return ALWAN_OK;
}

int alwan_color_matrix_apply_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                                     void const *in, alwan_pixel_format in_fmt,
                                     alwan_mat3x3 const *matrix,
                                     size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || !matrix || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar sv[3];
        alwan__load3_typed(sv, (char const *)in + i * in_stride, in_fmt);
        alwan_rgb rgb = {sv[0], sv[1], sv[2]};
        alwan_rgb r = alwan_color_matrix_apply_v(rgb, *matrix);
        alwan_scalar dv[3] = {r.r, r.g, r.b};
        alwan__store3_typed((char *)out + i * out_stride, dv, out_fmt);
    }
    return ALWAN_OK;
}

int alwan_printer_lights_apply_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                                       void const *in, alwan_pixel_format in_fmt,
                                       alwan_scalar red_lights, alwan_scalar green_lights,
                                       alwan_scalar blue_lights,
                                       size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar sv[3];
        alwan__load3_typed(sv, (char const *)in + i * in_stride, in_fmt);
        alwan_rgb rgb = {sv[0], sv[1], sv[2]};
        alwan_rgb r = alwan_printer_lights_apply_v(rgb, red_lights, green_lights, blue_lights);
        alwan_scalar dv[3] = {r.r, r.g, r.b};
        alwan__store3_typed((char *)out + i * out_stride, dv, out_fmt);
    }
    return ALWAN_OK;
}

int alwan_white_balance_apply_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                                      void const *in, alwan_pixel_format in_fmt,
                                      alwan_rgb const *multipliers,
                                      size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || !multipliers || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar sv[3];
        alwan__load3_typed(sv, (char const *)in + i * in_stride, in_fmt);
        alwan_rgb rgb = {sv[0], sv[1], sv[2]};
        alwan_rgb r = alwan_white_balance_apply_v(rgb, *multipliers);
        alwan_scalar dv[3] = {r.r, r.g, r.b};
        alwan__store3_typed((char *)out + i * out_stride, dv, out_fmt);
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CVD _ex (with severity)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX_V_SCALAR(alwan_simulate_protanopia_map_interleave_ex,   alwan_rgb, alwan_rgb, alwan_simulate_protanopia_v,   r,g,b, r,g,b)
ALWAN_MAP3_EX_V_SCALAR(alwan_simulate_deuteranopia_map_interleave_ex, alwan_rgb, alwan_rgb, alwan_simulate_deuteranopia_v, r,g,b, r,g,b)
ALWAN_MAP3_EX_V_SCALAR(alwan_simulate_tritanopia_map_interleave_ex,   alwan_rgb, alwan_rgb, alwan_simulate_tritanopia_v,   r,g,b, r,g,b)

int alwan_simulate_cvd_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               alwan_cvd_type cvd_type, alwan_scalar severity,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar sv[3];
        alwan__load3_typed(sv, (char const *)in + i * in_stride, in_fmt);
        alwan_rgb rgb_in = {sv[0], sv[1], sv[2]};
        alwan_rgb r;
        alwan_simulate_cvd(&r, &rgb_in, cvd_type, severity);
        alwan_scalar dv[3] = {r.r, r.g, r.b};
        alwan__store3_typed((char *)out + i * out_stride, dv, out_fmt);
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM _ex (typed XYZ side, struct correlates side)
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
