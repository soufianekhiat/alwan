/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Typed Planar Map Functions (_map_planar_ex variants)
 * Accept void* channel buffers with alwan_pixel_format for u8/u16/f32/f64 I/O
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
 * sRGB Convenience planar _ex
 * ---------------------------------------------------------------- */

ALWAN_MAP3_PLANAR_EX(alwan_srgb_to_xyz_map_planar_ex,   alwan_rgb,   alwan_xyz,   alwan_srgb_to_xyz,   r,g,b, x,y,z)
ALWAN_MAP3_PLANAR_EX(alwan_xyz_to_srgb_map_planar_ex,   alwan_xyz,   alwan_rgb,   alwan_xyz_to_srgb,   x,y,z, r,g,b)
ALWAN_MAP3_PLANAR_EX(alwan_srgb_to_lab_map_planar_ex,   alwan_rgb,   alwan_lab,   alwan_srgb_to_lab,   r,g,b, L,a,b)
ALWAN_MAP3_PLANAR_EX(alwan_lab_to_srgb_map_planar_ex,   alwan_lab,   alwan_rgb,   alwan_lab_to_srgb,   L,a,b, r,g,b)
ALWAN_MAP3_PLANAR_EX(alwan_srgb_to_oklab_map_planar_ex, alwan_rgb,   alwan_oklab, alwan_srgb_to_oklab, r,g,b, L,a,b)
ALWAN_MAP3_PLANAR_EX(alwan_oklab_to_srgb_map_planar_ex, alwan_oklab, alwan_rgb,   alwan_oklab_to_srgb, L,a,b, r,g,b)

/* ----------------------------------------------------------------
 * Colorspace planar _ex (with white_xyz)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_PLANAR_EX_WHITE(alwan_xyz_to_lab_map_planar_ex, alwan_xyz, alwan_lab, alwan_xyz_to_lab, x,y,z, L,a,b)
ALWAN_MAP3_PLANAR_EX_WHITE(alwan_lab_to_xyz_map_planar_ex, alwan_lab, alwan_xyz, alwan_lab_to_xyz, L,a,b, x,y,z)
ALWAN_MAP3_PLANAR_EX_WHITE(alwan_xyz_to_luv_map_planar_ex, alwan_xyz, alwan_luv, alwan_xyz_to_luv, x,y,z, L,u,v)
ALWAN_MAP3_PLANAR_EX_WHITE(alwan_luv_to_xyz_map_planar_ex, alwan_luv, alwan_xyz, alwan_luv_to_xyz, L,u,v, x,y,z)

/* Colorspace planar _ex (simple 3->3) */

ALWAN_MAP3_PLANAR_EX(alwan_lab_to_lch_map_planar_ex,    alwan_lab,   alwan_lch,   alwan_lab_to_lch,     L,a,b, L,C,h)
ALWAN_MAP3_PLANAR_EX(alwan_lch_to_lab_map_planar_ex,    alwan_lch,   alwan_lab,   alwan_lch_to_lab,     L,C,h, L,a,b)
ALWAN_MAP3_PLANAR_EX(alwan_luv_to_lchuv_map_planar_ex,  alwan_luv,   alwan_lchuv, alwan_luv_to_lchuv,   L,u,v, L,C,h)
ALWAN_MAP3_PLANAR_EX(alwan_lchuv_to_luv_map_planar_ex,  alwan_lchuv, alwan_luv,   alwan_lchuv_to_luv,   L,C,h, L,u,v)
ALWAN_MAP3_PLANAR_EX(alwan_xyz_to_xyy_map_planar_ex,    alwan_xyz,   alwan_xyy,   alwan_xyz_to_xyy,     x,y,z, x,y,Y)
ALWAN_MAP3_PLANAR_EX(alwan_xyy_to_xyz_map_planar_ex,    alwan_xyy,   alwan_xyz,   alwan_xyy_to_xyz,     x,y,Y, x,y,z)

/* ----------------------------------------------------------------
 * Oklab planar _ex
 * ---------------------------------------------------------------- */

ALWAN_MAP3_PLANAR_EX(alwan_xyz_to_oklab_map_planar_ex,   alwan_xyz,   alwan_oklab, alwan_xyz_to_oklab,   x,y,z, L,a,b)
ALWAN_MAP3_PLANAR_EX(alwan_oklab_to_xyz_map_planar_ex,   alwan_oklab, alwan_xyz,   alwan_oklab_to_xyz,   L,a,b, x,y,z)
ALWAN_MAP3_PLANAR_EX(alwan_oklab_to_oklch_map_planar_ex, alwan_oklab, alwan_oklch, alwan_oklab_to_oklch, L,a,b, L,C,h)
ALWAN_MAP3_PLANAR_EX(alwan_oklch_to_oklab_map_planar_ex, alwan_oklch, alwan_oklab, alwan_oklch_to_oklab, L,C,h, L,a,b)

/* ----------------------------------------------------------------
 * ICtCp planar _ex (with use_pq)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_PLANAR_EX_PQ(alwan_rgb_to_ictcp_map_planar_ex,  alwan_rgb,   alwan_ictcp, alwan_rgb_to_ictcp,  r,g,b, I,Ct,Cp)
ALWAN_MAP3_PLANAR_EX_PQ(alwan_ictcp_to_rgb_map_planar_ex,  alwan_ictcp, alwan_rgb,   alwan_ictcp_to_rgb,  I,Ct,Cp, r,g,b)
ALWAN_MAP3_PLANAR_EX_PQ(alwan_xyz_to_ictcp_map_planar_ex,  alwan_xyz,   alwan_ictcp, alwan_xyz_to_ictcp,  x,y,z, I,Ct,Cp)
ALWAN_MAP3_PLANAR_EX_PQ(alwan_ictcp_to_xyz_map_planar_ex,  alwan_ictcp, alwan_xyz,   alwan_ictcp_to_xyz,  I,Ct,Cp, x,y,z)

/* ----------------------------------------------------------------
 * JzAzBz planar _ex
 * ---------------------------------------------------------------- */

ALWAN_MAP3_PLANAR_EX(alwan_xyz_to_jzazbz_map_planar_ex,    alwan_xyz,    alwan_jzazbz, alwan_xyz_to_jzazbz,    x,y,z,    Jz,az,bz)
ALWAN_MAP3_PLANAR_EX(alwan_jzazbz_to_xyz_map_planar_ex,    alwan_jzazbz, alwan_xyz,    alwan_jzazbz_to_xyz,    Jz,az,bz, x,y,z)
ALWAN_MAP3_PLANAR_EX(alwan_jzazbz_to_jzczhz_map_planar_ex, alwan_jzazbz, alwan_jzczhz, alwan_jzazbz_to_jzczhz, Jz,az,bz, Jz,Cz,hz)
ALWAN_MAP3_PLANAR_EX(alwan_jzczhz_to_jzazbz_map_planar_ex, alwan_jzczhz, alwan_jzazbz, alwan_jzczhz_to_jzazbz, Jz,Cz,hz, Jz,az,bz)

/* ----------------------------------------------------------------
 * IPT planar _ex
 * ---------------------------------------------------------------- */

ALWAN_MAP3_PLANAR_EX(alwan_xyz_to_ipt_map_planar_ex, alwan_xyz, alwan_ipt, alwan_xyz_to_ipt, x,y,z, I,P,T)
ALWAN_MAP3_PLANAR_EX(alwan_ipt_to_xyz_map_planar_ex, alwan_ipt, alwan_xyz, alwan_ipt_to_xyz, I,P,T, x,y,z)

/* ----------------------------------------------------------------
 * HSV/HSL planar _ex (status)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_PLANAR_EX_STATUS(alwan_rgb_to_hsv_map_planar_ex, alwan_rgb, alwan_hsv, alwan_rgb_to_hsv, r,g,b, h,s,v)
ALWAN_MAP3_PLANAR_EX_STATUS(alwan_hsv_to_rgb_map_planar_ex, alwan_hsv, alwan_rgb, alwan_hsv_to_rgb, h,s,v, r,g,b)
ALWAN_MAP3_PLANAR_EX_STATUS(alwan_rgb_to_hsl_map_planar_ex, alwan_rgb, alwan_hsl, alwan_rgb_to_hsl, r,g,b, h,s,l)
ALWAN_MAP3_PLANAR_EX_STATUS(alwan_hsl_to_rgb_map_planar_ex, alwan_hsl, alwan_rgb, alwan_hsl_to_rgb, h,s,l, r,g,b)

/* ----------------------------------------------------------------
 * CMY/YCoCg/HWB planar _ex (using _v core)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_PLANAR_EX_V(alwan_rgb_to_cmy_map_planar_ex,   alwan_rgb, alwan_cmy, alwan_rgb_to_cmy_v, r,g,b, c,m,y)
ALWAN_MAP3_PLANAR_EX_V(alwan_cmy_to_rgb_map_planar_ex,   alwan_cmy, alwan_rgb, alwan_cmy_to_rgb_v, c,m,y, r,g,b)
ALWAN_MAP3_PLANAR_EX_V(alwan_rgb_to_ycocg_map_planar_ex, alwan_rgb, alwan_ycocg, alwan_rgb_to_ycocg_v, r,g,b, Y,Co,Cg)
ALWAN_MAP3_PLANAR_EX_V(alwan_ycocg_to_rgb_map_planar_ex, alwan_ycocg, alwan_rgb, alwan_ycocg_to_rgb_v, Y,Co,Cg, r,g,b)
ALWAN_MAP3_PLANAR_EX_V(alwan_rgb_to_hwb_map_planar_ex,   alwan_rgb, alwan_hwb, alwan_rgb_to_hwb_v, r,g,b, h,w,b)
ALWAN_MAP3_PLANAR_EX_V(alwan_hwb_to_rgb_map_planar_ex,   alwan_hwb, alwan_rgb, alwan_hwb_to_rgb_v, h,w,b, r,g,b)
ALWAN_MAP3_PLANAR_EX_V(alwan_hsv_to_hwb_map_planar_ex,   alwan_hsv, alwan_hwb, alwan_hsv_to_hwb_v, h,s,v, h,w,b)
ALWAN_MAP3_PLANAR_EX_V(alwan_hwb_to_hsv_map_planar_ex,   alwan_hwb, alwan_hsv, alwan_hwb_to_hsv_v, h,w,b, h,s,v)

/* ----------------------------------------------------------------
 * YCbCr planar _ex
 * ---------------------------------------------------------------- */

int alwan_rgb_to_ycbcr_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                       void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                       alwan_ycbcr_standard standard,
                                       size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar sv[3];
        sv[0] = alwan__load1_typed((char const *)in0 + i * in_stride, in_fmt);
        sv[1] = alwan__load1_typed((char const *)in1 + i * in_stride, in_fmt);
        sv[2] = alwan__load1_typed((char const *)in2 + i * in_stride, in_fmt);
        alwan_rgb rgb = {sv[0], sv[1], sv[2]};
        alwan_ycbcr dst;
        alwan_rgb_to_ycbcr(&dst, &rgb, standard);
        alwan__store1_typed((char *)out0 + i * out_stride, dst.Y, out_fmt);
        alwan__store1_typed((char *)out1 + i * out_stride, dst.Cb, out_fmt);
        alwan__store1_typed((char *)out2 + i * out_stride, dst.Cr, out_fmt);
    }
    return ALWAN_OK;
}

int alwan_ycbcr_to_rgb_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                       void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                       alwan_ycbcr_standard standard,
                                       size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar sv[3];
        sv[0] = alwan__load1_typed((char const *)in0 + i * in_stride, in_fmt);
        sv[1] = alwan__load1_typed((char const *)in1 + i * in_stride, in_fmt);
        sv[2] = alwan__load1_typed((char const *)in2 + i * in_stride, in_fmt);
        alwan_ycbcr ycbcr = {sv[0], sv[1], sv[2]};
        alwan_rgb dst;
        alwan_ycbcr_to_rgb(&dst, &ycbcr, standard);
        alwan__store1_typed((char *)out0 + i * out_stride, dst.r, out_fmt);
        alwan__store1_typed((char *)out1 + i * out_stride, dst.g, out_fmt);
        alwan__store1_typed((char *)out2 + i * out_stride, dst.b, out_fmt);
    }
    return ALWAN_OK;
}

/* YcCbcCrc / legal-full planar _ex */

ALWAN_MAP3_PLANAR_EX_V_INT(alwan_rgb_to_yccbccrc_map_planar_ex, alwan_rgb, alwan_yccbccrc, alwan_rgb_to_yccbccrc_v, int, bit_depth, r,g,b, Yc,Cbc,Crc)
ALWAN_MAP3_PLANAR_EX_V_INT(alwan_yccbccrc_to_rgb_map_planar_ex, alwan_yccbccrc, alwan_rgb, alwan_yccbccrc_to_rgb_v, int, bit_depth, Yc,Cbc,Crc, r,g,b)
ALWAN_MAP3_PLANAR_EX_V_INT(alwan_ycbcr_full_to_legal_map_planar_ex, alwan_ycbcr, alwan_ycbcr, alwan_ycbcr_full_to_legal_v, int, bit_depth, Y,Cb,Cr, Y,Cb,Cr)
ALWAN_MAP3_PLANAR_EX_V_INT(alwan_ycbcr_legal_to_full_map_planar_ex, alwan_ycbcr, alwan_ycbcr, alwan_ycbcr_legal_to_full_v, int, bit_depth, Y,Cb,Cr, Y,Cb,Cr)

/* ----------------------------------------------------------------
 * Extended color spaces planar _ex (using _v core)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_PLANAR_EX_V(alwan_xyz_to_igpgtg_map_planar_ex,     alwan_xyz,    alwan_igpgtg,    alwan_xyz_to_igpgtg_v,     x,y,z,    Ig,Pg,Tg)
ALWAN_MAP3_PLANAR_EX_V(alwan_igpgtg_to_xyz_map_planar_ex,     alwan_igpgtg, alwan_xyz,       alwan_igpgtg_to_xyz_v,     Ig,Pg,Tg, x,y,z)
ALWAN_MAP3_PLANAR_EX_V(alwan_xyz_to_icacb_map_planar_ex,      alwan_xyz,    alwan_icacb,     alwan_xyz_to_icacb_v,      x,y,z,    I,Ca,Cb)
ALWAN_MAP3_PLANAR_EX_V(alwan_icacb_to_xyz_map_planar_ex,      alwan_icacb,  alwan_xyz,       alwan_icacb_to_xyz_v,      I,Ca,Cb,  x,y,z)
ALWAN_MAP3_PLANAR_EX_V(alwan_xyz_to_hdr_cielab_map_planar_ex, alwan_xyz,    alwan_lab,       alwan_xyz_to_hdr_cielab_v, x,y,z,    L,a,b)
ALWAN_MAP3_PLANAR_EX_V(alwan_hdr_cielab_to_xyz_map_planar_ex, alwan_lab,    alwan_xyz,       alwan_hdr_cielab_to_xyz_v, L,a,b,    x,y,z)
ALWAN_MAP3_PLANAR_EX_V(alwan_xyz_to_hdr_ipt_map_planar_ex,    alwan_xyz,    alwan_ipt,       alwan_xyz_to_hdr_ipt_v,    x,y,z,    I,P,T)
ALWAN_MAP3_PLANAR_EX_V(alwan_hdr_ipt_to_xyz_map_planar_ex,    alwan_ipt,    alwan_xyz,       alwan_hdr_ipt_to_xyz_v,    I,P,T,    x,y,z)
ALWAN_MAP3_PLANAR_EX_V(alwan_xyz_to_ucs_map_planar_ex,        alwan_xyz,    alwan_ucs,       alwan_xyz_to_ucs_v,        x,y,z,    U,V,W)
ALWAN_MAP3_PLANAR_EX_V(alwan_ucs_to_xyz_map_planar_ex,        alwan_ucs,    alwan_xyz,       alwan_ucs_to_xyz_v,        U,V,W,    x,y,z)
ALWAN_MAP3_PLANAR_EX_V(alwan_xyz_to_osa_ucs_map_planar_ex,    alwan_xyz,    alwan_osa_ucs,   alwan_xyz_to_osa_ucs_v,    x,y,z,    L,j,g)
ALWAN_MAP3_PLANAR_EX_V(alwan_osa_ucs_to_xyz_map_planar_ex,    alwan_osa_ucs,alwan_xyz,       alwan_osa_ucs_to_xyz_v,    L,j,g,    x,y,z)
ALWAN_MAP3_PLANAR_EX_V(alwan_xyz_to_hunter_lab_map_planar_ex,  alwan_xyz,   alwan_hunter_lab, alwan_xyz_to_hunter_lab_v, x,y,z,    L,a,b)
ALWAN_MAP3_PLANAR_EX_V(alwan_hunter_lab_to_xyz_map_planar_ex,  alwan_hunter_lab, alwan_xyz,   alwan_hunter_lab_to_xyz_v, L,a,b,    x,y,z)
ALWAN_MAP3_PLANAR_EX_V(alwan_xyz_to_prolab_map_planar_ex,      alwan_xyz,   alwan_prolab,    alwan_xyz_to_prolab_v,     x,y,z,    L,a,b)
ALWAN_MAP3_PLANAR_EX_V(alwan_prolab_to_xyz_map_planar_ex,      alwan_prolab, alwan_xyz,      alwan_prolab_to_xyz_v,     L,a,b,    x,y,z)
ALWAN_MAP3_PLANAR_EX_V(alwan_rgb_to_prismatic_map_planar_ex,   alwan_rgb,   alwan_prismatic, alwan_rgb_to_prismatic_v,  r,g,b,    L,s,h)
ALWAN_MAP3_PLANAR_EX_V(alwan_prismatic_to_rgb_map_planar_ex,   alwan_prismatic, alwan_rgb,   alwan_prismatic_to_rgb_v,  L,s,h,    r,g,b)
ALWAN_MAP3_PLANAR_EX_V(alwan_rgb_to_hcl_map_planar_ex,         alwan_rgb,   alwan_hcl,      alwan_rgb_to_hcl_v,        r,g,b,    H,C,L)
ALWAN_MAP3_PLANAR_EX_V(alwan_hcl_to_rgb_map_planar_ex,         alwan_hcl,   alwan_rgb,      alwan_hcl_to_rgb_v,        H,C,L,    r,g,b)
ALWAN_MAP3_PLANAR_EX_V(alwan_rgb_to_ihls_map_planar_ex,        alwan_rgb,   alwan_ihls,     alwan_rgb_to_ihls_v,       r,g,b,    H,L,S)
ALWAN_MAP3_PLANAR_EX_V(alwan_ihls_to_rgb_map_planar_ex,        alwan_ihls,  alwan_rgb,      alwan_ihls_to_rgb_v,       H,L,S,    r,g,b)

/* Extended with white point planar _ex */

ALWAN_MAP3_PLANAR_EX_V_WHITE(alwan_xyz_to_uvw_map_planar_ex,              alwan_xyz, alwan_uvw,        alwan_xyz_to_uvw_v,                x,y,z, U,V,W)
ALWAN_MAP3_PLANAR_EX_V_WHITE(alwan_uvw_to_xyz_map_planar_ex,              alwan_uvw, alwan_xyz,        alwan_uvw_to_xyz_v,                U,V,W, x,y,z)
ALWAN_MAP3_PLANAR_EX_V_WHITE(alwan_xyz_to_hunter_lab_custom_map_planar_ex, alwan_xyz, alwan_hunter_lab, alwan_xyz_to_hunter_lab_custom_v, x,y,z, L,a,b)
ALWAN_MAP3_PLANAR_EX_V_WHITE(alwan_hunter_lab_to_xyz_custom_map_planar_ex, alwan_hunter_lab, alwan_xyz, alwan_hunter_lab_to_xyz_custom_v, L,a,b, x,y,z)
ALWAN_MAP3_PLANAR_EX_V_WHITE(alwan_xyz_to_prolab_custom_map_planar_ex,     alwan_xyz, alwan_prolab,    alwan_xyz_to_prolab_custom_v,     x,y,z, L,a,b)
ALWAN_MAP3_PLANAR_EX_V_WHITE(alwan_prolab_to_xyz_custom_map_planar_ex,     alwan_prolab, alwan_xyz,    alwan_prolab_to_xyz_custom_v,     L,a,b, x,y,z)

/* DIN99 planar _ex */

ALWAN_MAP3_PLANAR_EX_V_INT(alwan_lab_to_din99_map_planar_ex, alwan_lab, alwan_din99, alwan_lab_to_din99_v, int, variant, L,a,b, L99,a99,b99)
ALWAN_MAP3_PLANAR_EX_V_INT(alwan_din99_to_lab_map_planar_ex, alwan_din99, alwan_lab, alwan_din99_to_lab_v, int, variant, L99,a99,b99, L,a,b)

/* ----------------------------------------------------------------
 * CVD planar _ex
 * ---------------------------------------------------------------- */

ALWAN_MAP3_PLANAR_EX_V_SCALAR(alwan_simulate_protanopia_map_planar_ex,   alwan_rgb, alwan_rgb, alwan_simulate_protanopia_v,   r,g,b, r,g,b)
ALWAN_MAP3_PLANAR_EX_V_SCALAR(alwan_simulate_deuteranopia_map_planar_ex, alwan_rgb, alwan_rgb, alwan_simulate_deuteranopia_v, r,g,b, r,g,b)
ALWAN_MAP3_PLANAR_EX_V_SCALAR(alwan_simulate_tritanopia_map_planar_ex,   alwan_rgb, alwan_rgb, alwan_simulate_tritanopia_v,   r,g,b, r,g,b)

int alwan_simulate_cvd_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                       void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                       alwan_cvd_type cvd_type, alwan_scalar severity,
                                       size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar sv[3];
        sv[0] = alwan__load1_typed((char const *)in0 + i * in_stride, in_fmt);
        sv[1] = alwan__load1_typed((char const *)in1 + i * in_stride, in_fmt);
        sv[2] = alwan__load1_typed((char const *)in2 + i * in_stride, in_fmt);
        alwan_rgb rgb_in = {sv[0], sv[1], sv[2]};
        alwan_rgb r;
        alwan_simulate_cvd(&r, &rgb_in, cvd_type, severity);
        alwan__store1_typed((char *)out0 + i * out_stride, r.r, out_fmt);
        alwan__store1_typed((char *)out1 + i * out_stride, r.g, out_fmt);
        alwan__store1_typed((char *)out2 + i * out_stride, r.b, out_fmt);
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Color correction planar _ex
 * ---------------------------------------------------------------- */

int alwan_lgg_apply_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                    void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                    alwan_rgb const *lift, alwan_rgb const *gamma, alwan_rgb const *gain,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !lift || !gamma || !gain || count == 0)
        return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar sv[3];
        sv[0] = alwan__load1_typed((char const *)in0 + i * in_stride, in_fmt);
        sv[1] = alwan__load1_typed((char const *)in1 + i * in_stride, in_fmt);
        sv[2] = alwan__load1_typed((char const *)in2 + i * in_stride, in_fmt);
        alwan_rgb rgb = {sv[0], sv[1], sv[2]};
        alwan_rgb r = alwan_lgg_apply_v(rgb, *lift, *gamma, *gain);
        alwan__store1_typed((char *)out0 + i * out_stride, r.r, out_fmt);
        alwan__store1_typed((char *)out1 + i * out_stride, r.g, out_fmt);
        alwan__store1_typed((char *)out2 + i * out_stride, r.b, out_fmt);
    }
    return ALWAN_OK;
}

int alwan_color_matrix_apply_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                             void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                             alwan_mat3x3 const *matrix,
                                             size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !matrix || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar sv[3];
        sv[0] = alwan__load1_typed((char const *)in0 + i * in_stride, in_fmt);
        sv[1] = alwan__load1_typed((char const *)in1 + i * in_stride, in_fmt);
        sv[2] = alwan__load1_typed((char const *)in2 + i * in_stride, in_fmt);
        alwan_rgb rgb = {sv[0], sv[1], sv[2]};
        alwan_rgb r = alwan_color_matrix_apply_v(rgb, *matrix);
        alwan__store1_typed((char *)out0 + i * out_stride, r.r, out_fmt);
        alwan__store1_typed((char *)out1 + i * out_stride, r.g, out_fmt);
        alwan__store1_typed((char *)out2 + i * out_stride, r.b, out_fmt);
    }
    return ALWAN_OK;
}

int alwan_printer_lights_apply_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                               void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                               alwan_scalar red_lights, alwan_scalar green_lights, alwan_scalar blue_lights,
                                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar sv[3];
        sv[0] = alwan__load1_typed((char const *)in0 + i * in_stride, in_fmt);
        sv[1] = alwan__load1_typed((char const *)in1 + i * in_stride, in_fmt);
        sv[2] = alwan__load1_typed((char const *)in2 + i * in_stride, in_fmt);
        alwan_rgb rgb = {sv[0], sv[1], sv[2]};
        alwan_rgb r = alwan_printer_lights_apply_v(rgb, red_lights, green_lights, blue_lights);
        alwan__store1_typed((char *)out0 + i * out_stride, r.r, out_fmt);
        alwan__store1_typed((char *)out1 + i * out_stride, r.g, out_fmt);
        alwan__store1_typed((char *)out2 + i * out_stride, r.b, out_fmt);
    }
    return ALWAN_OK;
}

int alwan_white_balance_apply_map_planar_ex(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt,
                                              void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt,
                                              alwan_rgb const *multipliers,
                                              size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !multipliers || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar sv[3];
        sv[0] = alwan__load1_typed((char const *)in0 + i * in_stride, in_fmt);
        sv[1] = alwan__load1_typed((char const *)in1 + i * in_stride, in_fmt);
        sv[2] = alwan__load1_typed((char const *)in2 + i * in_stride, in_fmt);
        alwan_rgb rgb = {sv[0], sv[1], sv[2]};
        alwan_rgb r = alwan_white_balance_apply_v(rgb, *multipliers);
        alwan__store1_typed((char *)out0 + i * out_stride, r.r, out_fmt);
        alwan__store1_typed((char *)out1 + i * out_stride, r.g, out_fmt);
        alwan__store1_typed((char *)out2 + i * out_stride, r.b, out_fmt);
    }
    return ALWAN_OK;
}
