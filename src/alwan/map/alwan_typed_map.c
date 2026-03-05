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
 * sRGB Convenience _ex (pattern A: simple 3->3)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX(alwan_srgb_to_xyz_map_ex,   alwan_rgb,   alwan_xyz,   alwan_srgb_to_xyz,   r,g,b, x,y,z)
ALWAN_MAP3_EX(alwan_xyz_to_srgb_map_ex,   alwan_xyz,   alwan_rgb,   alwan_xyz_to_srgb,   x,y,z, r,g,b)
ALWAN_MAP3_EX(alwan_srgb_to_lab_map_ex,   alwan_rgb,   alwan_lab,   alwan_srgb_to_lab,   r,g,b, L,a,b)
ALWAN_MAP3_EX(alwan_lab_to_srgb_map_ex,   alwan_lab,   alwan_rgb,   alwan_lab_to_srgb,   L,a,b, r,g,b)
ALWAN_MAP3_EX(alwan_srgb_to_oklab_map_ex, alwan_rgb,   alwan_oklab, alwan_srgb_to_oklab, r,g,b, L,a,b)
ALWAN_MAP3_EX(alwan_oklab_to_srgb_map_ex, alwan_oklab, alwan_rgb,   alwan_oklab_to_srgb, L,a,b, r,g,b)

/* ----------------------------------------------------------------
 * Color Space _ex (pattern B: with white_xyz)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX_WHITE(alwan_xyz_to_lab_map_ex, alwan_xyz, alwan_lab, alwan_xyz_to_lab, x,y,z, L,a,b)
ALWAN_MAP3_EX_WHITE(alwan_lab_to_xyz_map_ex, alwan_lab, alwan_xyz, alwan_lab_to_xyz, L,a,b, x,y,z)
ALWAN_MAP3_EX_WHITE(alwan_xyz_to_luv_map_ex, alwan_xyz, alwan_luv, alwan_xyz_to_luv, x,y,z, L,u,v)
ALWAN_MAP3_EX_WHITE(alwan_luv_to_xyz_map_ex, alwan_luv, alwan_xyz, alwan_luv_to_xyz, L,u,v, x,y,z)

/* Color Space _ex (pattern A: simple 3->3) */

ALWAN_MAP3_EX(alwan_lab_to_lch_map_ex,    alwan_lab,   alwan_lch,   alwan_lab_to_lch,     L,a,b, L,C,h)
ALWAN_MAP3_EX(alwan_lch_to_lab_map_ex,    alwan_lch,   alwan_lab,   alwan_lch_to_lab,     L,C,h, L,a,b)
ALWAN_MAP3_EX(alwan_luv_to_lchuv_map_ex,  alwan_luv,   alwan_lchuv, alwan_luv_to_lchuv,   L,u,v, L,C,h)
ALWAN_MAP3_EX(alwan_lchuv_to_luv_map_ex,  alwan_lchuv, alwan_luv,   alwan_lchuv_to_luv,   L,C,h, L,u,v)
ALWAN_MAP3_EX(alwan_xyz_to_xyy_map_ex,    alwan_xyz,   alwan_xyy,   alwan_xyz_to_xyy,     x,y,z, x,y,Y)
ALWAN_MAP3_EX(alwan_xyy_to_xyz_map_ex,    alwan_xyy,   alwan_xyz,   alwan_xyy_to_xyz,     x,y,Y, x,y,z)

/* ----------------------------------------------------------------
 * Oklab _ex (pattern A)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX(alwan_xyz_to_oklab_map_ex,   alwan_xyz,   alwan_oklab, alwan_xyz_to_oklab,   x,y,z, L,a,b)
ALWAN_MAP3_EX(alwan_oklab_to_xyz_map_ex,   alwan_oklab, alwan_xyz,   alwan_oklab_to_xyz,   L,a,b, x,y,z)
ALWAN_MAP3_EX(alwan_oklab_to_oklch_map_ex, alwan_oklab, alwan_oklch, alwan_oklab_to_oklch, L,a,b, L,C,h)
ALWAN_MAP3_EX(alwan_oklch_to_oklab_map_ex, alwan_oklch, alwan_oklab, alwan_oklch_to_oklab, L,C,h, L,a,b)

/* ----------------------------------------------------------------
 * ICtCp _ex (pattern C: with use_pq)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX_PQ(alwan_rgb_to_ictcp_map_ex,  alwan_rgb,   alwan_ictcp, alwan_rgb_to_ictcp,  r,g,b, I,Ct,Cp)
ALWAN_MAP3_EX_PQ(alwan_ictcp_to_rgb_map_ex,  alwan_ictcp, alwan_rgb,   alwan_ictcp_to_rgb,  I,Ct,Cp, r,g,b)
ALWAN_MAP3_EX_PQ(alwan_xyz_to_ictcp_map_ex,  alwan_xyz,   alwan_ictcp, alwan_xyz_to_ictcp,  x,y,z, I,Ct,Cp)
ALWAN_MAP3_EX_PQ(alwan_ictcp_to_xyz_map_ex,  alwan_ictcp, alwan_xyz,   alwan_ictcp_to_xyz,  I,Ct,Cp, x,y,z)

/* ----------------------------------------------------------------
 * JzAzBz _ex (pattern A)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX(alwan_xyz_to_jzazbz_map_ex,    alwan_xyz,    alwan_jzazbz, alwan_xyz_to_jzazbz,    x,y,z,    Jz,az,bz)
ALWAN_MAP3_EX(alwan_jzazbz_to_xyz_map_ex,    alwan_jzazbz, alwan_xyz,    alwan_jzazbz_to_xyz,    Jz,az,bz, x,y,z)
ALWAN_MAP3_EX(alwan_jzazbz_to_jzczhz_map_ex, alwan_jzazbz, alwan_jzczhz, alwan_jzazbz_to_jzczhz, Jz,az,bz, Jz,Cz,hz)
ALWAN_MAP3_EX(alwan_jzczhz_to_jzazbz_map_ex, alwan_jzczhz, alwan_jzazbz, alwan_jzczhz_to_jzazbz, Jz,Cz,hz, Jz,az,bz)

/* ----------------------------------------------------------------
 * IPT _ex (pattern A)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX(alwan_xyz_to_ipt_map_ex, alwan_xyz, alwan_ipt, alwan_xyz_to_ipt, x,y,z, I,P,T)
ALWAN_MAP3_EX(alwan_ipt_to_xyz_map_ex, alwan_ipt, alwan_xyz, alwan_ipt_to_xyz, I,P,T, x,y,z)

/* ----------------------------------------------------------------
 * Convenience HSV/HSL _ex (pattern D: with status)
 * ---------------------------------------------------------------- */

ALWAN_MAP3_EX_STATUS(alwan_rgb_to_hsv_map_ex, alwan_rgb, alwan_hsv, alwan_rgb_to_hsv, r,g,b, h,s,v)
ALWAN_MAP3_EX_STATUS(alwan_hsv_to_rgb_map_ex, alwan_hsv, alwan_rgb, alwan_hsv_to_rgb, h,s,v, r,g,b)
ALWAN_MAP3_EX_STATUS(alwan_rgb_to_hsl_map_ex, alwan_rgb, alwan_hsl, alwan_rgb_to_hsl, r,g,b, h,s,l)
ALWAN_MAP3_EX_STATUS(alwan_hsl_to_rgb_map_ex, alwan_hsl, alwan_rgb, alwan_hsl_to_rgb, h,s,l, r,g,b)

/* ----------------------------------------------------------------
 * CAM _ex (typed XYZ side, struct correlates side)
 * ---------------------------------------------------------------- */

int alwan_ciecam02_forward_map_ex(alwan_ciecam02_correlates *correlates_out,
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

int alwan_ciecam02_inverse_map_ex(void *xyz_out, alwan_pixel_format out_fmt,
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

int alwan_cam16_forward_map_ex(alwan_cam16_correlates *correlates_out,
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

int alwan_cam16_inverse_map_ex(void *xyz_out, alwan_pixel_format out_fmt,
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
 * Mat3 transform _ex
 * ---------------------------------------------------------------- */

int alwan_mat3_transform_map_ex(void *vec_out, alwan_pixel_format out_fmt,
                                 alwan_mat3x3 const *matrix,
                                 void const *vec_in, alwan_pixel_format in_fmt,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!vec_in || !vec_out || !matrix || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar s[3];
        alwan__load3_typed(s, (char const *)vec_in + i * in_stride, in_fmt);
        alwan_vec3 v = {{s[0], s[1], s[2]}};
        alwan_vec3 r;
        alwan_mat3_mulv(&r, matrix, &v);
        alwan_scalar d[3] = {r.v[0], r.v[1], r.v[2]};
        alwan__store3_typed((char *)vec_out + i * out_stride, d, out_fmt);
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
