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
 * sRGB <-> XYZ D65 matrices (BT.709 primaries)
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_CONSTEXPR alwan_mat3x3 SRGB_TO_XYZ = {{
#include "../data/matrices/aces_rec709_to_xyz.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 XYZ_TO_SRGB = {{
#include "../data/matrices/aces_xyz_to_rec709.csv"
}};
ALWAN_DIAG_POP

/* D65 white point (Y=1 normalized) */
ALWAN_DIAG_PUSH
static alwan_scalar const D65_WP_Y1[] = {
#include "../data/white_d65_xyz_y1.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Single-pixel sRGB Convenience Conversions
 * Used by _map_interleave_ex macros; composite of EOTF/OETF + matrix + core
 * ---------------------------------------------------------------- */

int alwan_srgb_to_xyz(alwan_xyz *xyz, alwan_rgb const *rgb) {
    if (!rgb || !xyz) return ALWAN_E_INVALID;
    alwan_vec3 v = {{alwan_srgb_eotf(rgb->r), alwan_srgb_eotf(rgb->g), alwan_srgb_eotf(rgb->b)}};
    alwan_vec3 r = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
    xyz->x = r.v[0]; xyz->y = r.v[1]; xyz->z = r.v[2];
    return ALWAN_OK;
}

int alwan_xyz_to_srgb(alwan_rgb *rgb, alwan_xyz const *xyz) {
    if (!xyz || !rgb) return ALWAN_E_INVALID;
    alwan_vec3 v = {{xyz->x, xyz->y, xyz->z}};
    alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
    rgb->r = alwan_srgb_oetf(lin.v[0]); rgb->g = alwan_srgb_oetf(lin.v[1]); rgb->b = alwan_srgb_oetf(lin.v[2]);
    return ALWAN_OK;
}

int alwan_srgb_to_lab(alwan_lab *lab, alwan_rgb const *rgb) {
    if (!rgb || !lab) return ALWAN_E_INVALID;
    alwan_vec3 v = {{alwan_srgb_eotf(rgb->r), alwan_srgb_eotf(rgb->g), alwan_srgb_eotf(rgb->b)}};
    alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
    alwan_xyz wp = {D65_WP_Y1[0], D65_WP_Y1[1], D65_WP_Y1[2]};
    alwan_xyz xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
    *lab = alwan_xyz_to_lab_v(xyz_s, wp);
    return ALWAN_OK;
}

int alwan_lab_to_srgb(alwan_rgb *rgb, alwan_lab const *lab) {
    if (!lab || !rgb) return ALWAN_E_INVALID;
    alwan_xyz wp = {D65_WP_Y1[0], D65_WP_Y1[1], D65_WP_Y1[2]};
    alwan_xyz xyz = alwan_lab_to_xyz_v(*lab, wp);
    alwan_vec3 v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
    rgb->r = alwan_srgb_oetf(lin.v[0]); rgb->g = alwan_srgb_oetf(lin.v[1]); rgb->b = alwan_srgb_oetf(lin.v[2]);
    return ALWAN_OK;
}

int alwan_srgb_to_oklab(alwan_oklab *oklab, alwan_rgb const *rgb) {
    if (!rgb || !oklab) return ALWAN_E_INVALID;
    alwan_vec3 v = {{alwan_srgb_eotf(rgb->r), alwan_srgb_eotf(rgb->g), alwan_srgb_eotf(rgb->b)}};
    alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
    alwan_xyz xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
    *oklab = alwan_xyz_to_oklab_v(xyz_s);
    return ALWAN_OK;
}

int alwan_oklab_to_srgb(alwan_rgb *rgb, alwan_oklab const *oklab) {
    if (!oklab || !rgb) return ALWAN_E_INVALID;
    alwan_xyz xyz = alwan_oklab_to_xyz_v(*oklab);
    alwan_vec3 v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
    rgb->r = alwan_srgb_oetf(lin.v[0]); rgb->g = alwan_srgb_oetf(lin.v[1]); rgb->b = alwan_srgb_oetf(lin.v[2]);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * SIMD+scalar kernels for sRGB convenience conversions
 * ---------------------------------------------------------------- */

static void alwan__srgb_to_xyz_kernel(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                       alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                       size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    for (; i + W <= n; i += W) {
        alwan_simd vr = alwan__srgb_eotf_simd(alwan_simd_load(&i0[i]));
        alwan_simd vg = alwan__srgb_eotf_simd(alwan_simd_load(&i1[i]));
        alwan_simd vb = alwan__srgb_eotf_simd(alwan_simd_load(&i2[i]));
        alwan_simd ox, oy, oz;
        alwan__mat3_mul_simd(&ox, &oy, &oz, &SRGB_TO_XYZ, vr, vg, vb);
        alwan_simd_store(&o0[i], ox);
        alwan_simd_store(&o1[i], oy);
        alwan_simd_store(&o2[i], oz);
    }
#endif
    for (; i < n; i++) {
        alwan_scalar r = alwan_srgb_eotf((alwan_scalar)i0[i]);
        alwan_scalar g = alwan_srgb_eotf((alwan_scalar)i1[i]);
        alwan_scalar b = alwan_srgb_eotf((alwan_scalar)i2[i]);
        alwan_vec3 v = {{r, g, b}};
        alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
        o0[i] = (alwan_simd_lane)xyz.v[0]; o1[i] = (alwan_simd_lane)xyz.v[1]; o2[i] = (alwan_simd_lane)xyz.v[2];
    }
}

static void alwan__xyz_to_srgb_kernel(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                       alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                       size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    for (; i + W <= n; i += W) {
        alwan_simd vx = alwan_simd_load(&i0[i]);
        alwan_simd vy = alwan_simd_load(&i1[i]);
        alwan_simd vz = alwan_simd_load(&i2[i]);
        alwan_simd lr, lg, lb;
        alwan__mat3_mul_simd(&lr, &lg, &lb, &XYZ_TO_SRGB, vx, vy, vz);
        alwan_simd_store(&o0[i], alwan__srgb_oetf_simd(lr));
        alwan_simd_store(&o1[i], alwan__srgb_oetf_simd(lg));
        alwan_simd_store(&o2[i], alwan__srgb_oetf_simd(lb));
    }
#endif
    for (; i < n; i++) {
        alwan_vec3 v = {{(alwan_scalar)i0[i], (alwan_scalar)i1[i], (alwan_scalar)i2[i]}};
        alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
        o0[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[0]);
        o1[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[1]);
        o2[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[2]);
    }
}

static void alwan__srgb_to_lab_kernel(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                       alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                       size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd inv_wx = alwan_simd_set1(1.0 / (alwan_simd_lane)D65_WP_Y1[0]);
    alwan_simd inv_wy = alwan_simd_set1(1.0 / (alwan_simd_lane)D65_WP_Y1[1]);
    alwan_simd inv_wz = alwan_simd_set1(1.0 / (alwan_simd_lane)D65_WP_Y1[2]);
    for (; i + W <= n; i += W) {
        /* sRGB EOTF */
        alwan_simd vr = alwan__srgb_eotf_simd(alwan_simd_load(&i0[i]));
        alwan_simd vg = alwan__srgb_eotf_simd(alwan_simd_load(&i1[i]));
        alwan_simd vb = alwan__srgb_eotf_simd(alwan_simd_load(&i2[i]));
        /* sRGB -> XYZ */
        alwan_simd vx, vy, vz;
        alwan__mat3_mul_simd(&vx, &vy, &vz, &SRGB_TO_XYZ, vr, vg, vb);
        /* XYZ -> Lab */
        alwan_simd fx = alwan__lab_f_simd(alwan_simd_mul(vx, inv_wx));
        alwan_simd fy = alwan__lab_f_simd(alwan_simd_mul(vy, inv_wy));
        alwan_simd fz = alwan__lab_f_simd(alwan_simd_mul(vz, inv_wz));
        alwan_simd oL = alwan_simd_fmsub(alwan_simd_set1(116.0), fy, alwan_simd_set1(16.0));
        alwan_simd oa = alwan_simd_mul(alwan_simd_set1(500.0), alwan_simd_sub(fx, fy));
        alwan_simd ob = alwan_simd_mul(alwan_simd_set1(200.0), alwan_simd_sub(fy, fz));
        alwan_simd_store(&o0[i], oL);
        alwan_simd_store(&o1[i], oa);
        alwan_simd_store(&o2[i], ob);
    }
#endif
    for (; i < n; i++) {
        alwan_scalar r = alwan_srgb_eotf((alwan_scalar)i0[i]);
        alwan_scalar g = alwan_srgb_eotf((alwan_scalar)i1[i]);
        alwan_scalar b = alwan_srgb_eotf((alwan_scalar)i2[i]);
        alwan_vec3 v = {{r, g, b}};
        alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
        alwan_xyz wp = {D65_WP_Y1[0], D65_WP_Y1[1], D65_WP_Y1[2]};
        alwan_xyz xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
        alwan_lab lab = alwan_xyz_to_lab_v(xyz_s, wp);
        o0[i] = (alwan_simd_lane)lab.L; o1[i] = (alwan_simd_lane)lab.a; o2[i] = (alwan_simd_lane)lab.b;
    }
}

static void alwan__lab_to_srgb_kernel(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                       alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                       size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd wx = alwan_simd_set1((alwan_simd_lane)D65_WP_Y1[0]);
    alwan_simd wy = alwan_simd_set1((alwan_simd_lane)D65_WP_Y1[1]);
    alwan_simd wz = alwan_simd_set1((alwan_simd_lane)D65_WP_Y1[2]);
    for (; i + W <= n; i += W) {
        alwan_simd vL = alwan_simd_load(&i0[i]);
        alwan_simd va = alwan_simd_load(&i1[i]);
        alwan_simd vb = alwan_simd_load(&i2[i]);
        /* Lab -> XYZ */
        alwan_simd fy = alwan_simd_mul(alwan_simd_add(vL, alwan_simd_set1(16.0)),
                                                 alwan_simd_set1(1.0 / 116.0));
        alwan_simd fx = alwan_simd_fmadd(va, alwan_simd_set1(1.0 / 500.0), fy);
        alwan_simd fz = alwan_simd_sub(fy, alwan_simd_mul(vb, alwan_simd_set1(1.0 / 200.0)));
        alwan_simd vx = alwan_simd_mul(alwan__lab_f_inv_simd(fx), wx);
        alwan_simd vy = alwan_simd_mul(alwan__lab_f_inv_simd(fy), wy);
        alwan_simd vz = alwan_simd_mul(alwan__lab_f_inv_simd(fz), wz);
        /* XYZ -> sRGB */
        alwan_simd lr, lg, lb;
        alwan__mat3_mul_simd(&lr, &lg, &lb, &XYZ_TO_SRGB, vx, vy, vz);
        alwan_simd_store(&o0[i], alwan__srgb_oetf_simd(lr));
        alwan_simd_store(&o1[i], alwan__srgb_oetf_simd(lg));
        alwan_simd_store(&o2[i], alwan__srgb_oetf_simd(lb));
    }
#endif
    for (; i < n; i++) {
        alwan_lab lab = {(alwan_scalar)i0[i], (alwan_scalar)i1[i], (alwan_scalar)i2[i]};
        alwan_xyz wp = {D65_WP_Y1[0], D65_WP_Y1[1], D65_WP_Y1[2]};
        alwan_xyz xyz = alwan_lab_to_xyz_v(lab, wp);
        alwan_vec3 v = {{xyz.x, xyz.y, xyz.z}};
        alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
        o0[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[0]);
        o1[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[1]);
        o2[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[2]);
    }
}

static void alwan__srgb_to_oklab_kernel(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                         alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                         size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    for (; i + W <= n; i += W) {
        /* sRGB EOTF */
        alwan_simd vr = alwan__srgb_eotf_simd(alwan_simd_load(&i0[i]));
        alwan_simd vg = alwan__srgb_eotf_simd(alwan_simd_load(&i1[i]));
        alwan_simd vb = alwan__srgb_eotf_simd(alwan_simd_load(&i2[i]));
        /* sRGB -> XYZ */
        alwan_simd vx, vy, vz;
        alwan__mat3_mul_simd(&vx, &vy, &vz, &SRGB_TO_XYZ, vr, vg, vb);
        /* XYZ -> LMS (OKLAB M1) */
        alwan_simd vl, vm, vs;
        alwan__mat3_mul_simd(&vl, &vm, &vs, &OKLAB_M1, vx, vy, vz);
        /* cbrt */
        vl = alwan_simd_cbrt(vl);
        vm = alwan_simd_cbrt(vm);
        vs = alwan_simd_cbrt(vs);
        /* LMS' -> Lab (OKLAB M2) */
        alwan_simd oL, oa, ob;
        alwan__mat3_mul_simd(&oL, &oa, &ob, &OKLAB_M2, vl, vm, vs);
        alwan_simd_store(&o0[i], oL);
        alwan_simd_store(&o1[i], oa);
        alwan_simd_store(&o2[i], ob);
    }
#endif
    for (; i < n; i++) {
        alwan_scalar r = alwan_srgb_eotf((alwan_scalar)i0[i]);
        alwan_scalar g = alwan_srgb_eotf((alwan_scalar)i1[i]);
        alwan_scalar b = alwan_srgb_eotf((alwan_scalar)i2[i]);
        alwan_vec3 v = {{r, g, b}};
        alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
        alwan_xyz xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
        alwan_oklab ok = alwan_xyz_to_oklab_v(xyz_s);
        o0[i] = (alwan_simd_lane)ok.L; o1[i] = (alwan_simd_lane)ok.a; o2[i] = (alwan_simd_lane)ok.b;
    }
}

static void alwan__oklab_to_srgb_kernel(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                         alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                         size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    for (; i + W <= n; i += W) {
        alwan_simd vL = alwan_simd_load(&i0[i]);
        alwan_simd va = alwan_simd_load(&i1[i]);
        alwan_simd vb = alwan_simd_load(&i2[i]);
        /* Lab -> LMS' (OKLAB M2_INV) */
        alwan_simd lp, mp, sp;
        alwan__mat3_mul_simd(&lp, &mp, &sp, &OKLAB_M2_INV, vL, va, vb);
        /* cube */
        alwan_simd vl = alwan_simd_mul(lp, alwan_simd_mul(lp, lp));
        alwan_simd vm = alwan_simd_mul(mp, alwan_simd_mul(mp, mp));
        alwan_simd vs = alwan_simd_mul(sp, alwan_simd_mul(sp, sp));
        /* LMS -> XYZ (OKLAB M1_INV) */
        alwan_simd vx, vy, vz;
        alwan__mat3_mul_simd(&vx, &vy, &vz, &OKLAB_M1_INV, vl, vm, vs);
        /* XYZ -> sRGB */
        alwan_simd lr, lg, lb;
        alwan__mat3_mul_simd(&lr, &lg, &lb, &XYZ_TO_SRGB, vx, vy, vz);
        alwan_simd_store(&o0[i], alwan__srgb_oetf_simd(lr));
        alwan_simd_store(&o1[i], alwan__srgb_oetf_simd(lg));
        alwan_simd_store(&o2[i], alwan__srgb_oetf_simd(lb));
    }
#endif
    for (; i < n; i++) {
        alwan_oklab ok = {(alwan_scalar)i0[i], (alwan_scalar)i1[i], (alwan_scalar)i2[i]};
        alwan_xyz xyz = alwan_oklab_to_xyz_v(ok);
        alwan_vec3 v = {{xyz.x, xyz.y, xyz.z}};
        alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
        o0[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[0]);
        o1[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[1]);
        o2[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[2]);
    }
}

/* ----------------------------------------------------------------
 * Map sRGB Convenience Conversions - True SIMD vectorized
 * ---------------------------------------------------------------- */

int alwan_srgb_to_xyz_map_interleave(alwan_scalar *xyz_out,
                           alwan_scalar const *rgb_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!rgb_in || !xyz_out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_TILED(rgb_in, in_stride, xyz_out, out_stride, count, alwan__srgb_to_xyz_kernel);
    return ALWAN_OK;
}

int alwan_srgb_to_xyz_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                              void const *in, alwan_pixel_format in_fmt,
                              size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_TILED_EX(in, in_fmt, in_stride, out, out_fmt, out_stride, count, alwan__srgb_to_xyz_kernel);
    return ALWAN_OK;
}

int alwan_xyz_to_srgb_map_interleave(alwan_scalar *rgb_out,
                           alwan_scalar const *xyz_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!xyz_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_TILED(xyz_in, in_stride, rgb_out, out_stride, count, alwan__xyz_to_srgb_kernel);
    return ALWAN_OK;
}

int alwan_xyz_to_srgb_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                              void const *in, alwan_pixel_format in_fmt,
                              size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_TILED_EX(in, in_fmt, in_stride, out, out_fmt, out_stride, count, alwan__xyz_to_srgb_kernel);
    return ALWAN_OK;
}

int alwan_srgb_to_lab_map_interleave(alwan_scalar *lab_out,
                           alwan_scalar const *rgb_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!rgb_in || !lab_out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_TILED(rgb_in, in_stride, lab_out, out_stride, count, alwan__srgb_to_lab_kernel);
    return ALWAN_OK;
}

int alwan_srgb_to_lab_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                              void const *in, alwan_pixel_format in_fmt,
                              size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_TILED_EX(in, in_fmt, in_stride, out, out_fmt, out_stride, count, alwan__srgb_to_lab_kernel);
    return ALWAN_OK;
}

int alwan_lab_to_srgb_map_interleave(alwan_scalar *rgb_out,
                           alwan_scalar const *lab_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!lab_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_TILED(lab_in, in_stride, rgb_out, out_stride, count, alwan__lab_to_srgb_kernel);
    return ALWAN_OK;
}

int alwan_lab_to_srgb_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                              void const *in, alwan_pixel_format in_fmt,
                              size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_TILED_EX(in, in_fmt, in_stride, out, out_fmt, out_stride, count, alwan__lab_to_srgb_kernel);
    return ALWAN_OK;
}

int alwan_srgb_to_oklab_map_interleave(alwan_scalar *oklab_out,
                             alwan_scalar const *rgb_in,
                             size_t count,
                             size_t in_stride,
                             size_t out_stride) {
    if (!rgb_in || !oklab_out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_TILED(rgb_in, in_stride, oklab_out, out_stride, count, alwan__srgb_to_oklab_kernel);
    return ALWAN_OK;
}

int alwan_srgb_to_oklab_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                                void const *in, alwan_pixel_format in_fmt,
                                size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_TILED_EX(in, in_fmt, in_stride, out, out_fmt, out_stride, count, alwan__srgb_to_oklab_kernel);
    return ALWAN_OK;
}

int alwan_oklab_to_srgb_map_interleave(alwan_scalar *rgb_out,
                             alwan_scalar const *oklab_in,
                             size_t count,
                             size_t in_stride,
                             size_t out_stride) {
    if (!oklab_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_TILED(oklab_in, in_stride, rgb_out, out_stride, count, alwan__oklab_to_srgb_kernel);
    return ALWAN_OK;
}

int alwan_oklab_to_srgb_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                                void const *in, alwan_pixel_format in_fmt,
                                size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_TILED_EX(in, in_fmt, in_stride, out, out_fmt, out_stride, count, alwan__oklab_to_srgb_kernel);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Planar Map Variants
 * ---------------------------------------------------------------- */

int alwan_srgb_to_xyz_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                  alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                  size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vr = alwan__srgb_eotf_simd(alwan_simd_load(&c0[i]));
            alwan_simd vg = alwan__srgb_eotf_simd(alwan_simd_load(&c1[i]));
            alwan_simd vb = alwan__srgb_eotf_simd(alwan_simd_load(&c2[i]));
            alwan_simd ox, oy, oz;
            alwan__mat3_mul_simd(&ox, &oy, &oz, &SRGB_TO_XYZ, vr, vg, vb);
            alwan_simd_store(&c0[i], ox);
            alwan_simd_store(&c1[i], oy);
            alwan_simd_store(&c2[i], oz);
        }
        for (; i < tile; i++) {
            alwan_scalar r = alwan_srgb_eotf((alwan_scalar)c0[i]);
            alwan_scalar g = alwan_srgb_eotf((alwan_scalar)c1[i]);
            alwan_scalar b = alwan_srgb_eotf((alwan_scalar)c2[i]);
            alwan_vec3 v = {{r, g, b}};
            alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
            c0[i] = (alwan_simd_lane)xyz.v[0]; c1[i] = (alwan_simd_lane)xyz.v[1]; c2[i] = (alwan_simd_lane)xyz.v[2];
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar r = alwan_srgb_eotf(*(alwan_scalar const *)((char const *)in_ch0 + i * in_stride));
        alwan_scalar g = alwan_srgb_eotf(*(alwan_scalar const *)((char const *)in_ch1 + i * in_stride));
        alwan_scalar b = alwan_srgb_eotf(*(alwan_scalar const *)((char const *)in_ch2 + i * in_stride));
        alwan_vec3 v = {{r, g, b}};
        alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = xyz.v[0];
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = xyz.v[1];
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = xyz.v[2];
    }
#endif

    return ALWAN_OK;
}

int alwan_xyz_to_srgb_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                  alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                  size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            alwan_simd lr, lg, lb;
            alwan__mat3_mul_simd(&lr, &lg, &lb, &XYZ_TO_SRGB, vx, vy, vz);
            alwan_simd_store(&c0[i], alwan__srgb_oetf_simd(lr));
            alwan_simd_store(&c1[i], alwan__srgb_oetf_simd(lg));
            alwan_simd_store(&c2[i], alwan__srgb_oetf_simd(lb));
        }
        for (; i < tile; i++) {
            alwan_vec3 v = {{(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]}};
            alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
            c0[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[0]);
            c1[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[1]);
            c2[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[2]);
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_vec3 v = {{
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        }};
        alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = alwan_srgb_oetf(lin.v[0]);
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = alwan_srgb_oetf(lin.v[1]);
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = alwan_srgb_oetf(lin.v[2]);
    }
#endif

    return ALWAN_OK;
}

int alwan_srgb_to_lab_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                  alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                  size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd inv_wx = alwan_simd_set1(1.0 / (alwan_simd_lane)D65_WP_Y1[0]);
    alwan_simd inv_wy = alwan_simd_set1(1.0 / (alwan_simd_lane)D65_WP_Y1[1]);
    alwan_simd inv_wz = alwan_simd_set1(1.0 / (alwan_simd_lane)D65_WP_Y1[2]);
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            /* sRGB EOTF */
            alwan_simd vr = alwan__srgb_eotf_simd(alwan_simd_load(&c0[i]));
            alwan_simd vg = alwan__srgb_eotf_simd(alwan_simd_load(&c1[i]));
            alwan_simd vb = alwan__srgb_eotf_simd(alwan_simd_load(&c2[i]));
            /* sRGB -> XYZ */
            alwan_simd vx, vy, vz;
            alwan__mat3_mul_simd(&vx, &vy, &vz, &SRGB_TO_XYZ, vr, vg, vb);
            /* XYZ -> Lab */
            alwan_simd fx = alwan__lab_f_simd(alwan_simd_mul(vx, inv_wx));
            alwan_simd fy = alwan__lab_f_simd(alwan_simd_mul(vy, inv_wy));
            alwan_simd fz = alwan__lab_f_simd(alwan_simd_mul(vz, inv_wz));
            alwan_simd oL = alwan_simd_fmsub(alwan_simd_set1(116.0), fy, alwan_simd_set1(16.0));
            alwan_simd oa = alwan_simd_mul(alwan_simd_set1(500.0), alwan_simd_sub(fx, fy));
            alwan_simd ob = alwan_simd_mul(alwan_simd_set1(200.0), alwan_simd_sub(fy, fz));
            alwan_simd_store(&c0[i], oL);
            alwan_simd_store(&c1[i], oa);
            alwan_simd_store(&c2[i], ob);
        }
        for (; i < tile; i++) {
            alwan_scalar r = alwan_srgb_eotf((alwan_scalar)c0[i]);
            alwan_scalar g = alwan_srgb_eotf((alwan_scalar)c1[i]);
            alwan_scalar b = alwan_srgb_eotf((alwan_scalar)c2[i]);
            alwan_vec3 v = {{r, g, b}};
            alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
            alwan_xyz wp = {D65_WP_Y1[0], D65_WP_Y1[1], D65_WP_Y1[2]};
            alwan_xyz xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
            alwan_lab lab = alwan_xyz_to_lab_v(xyz_s, wp);
            c0[i] = (alwan_simd_lane)lab.L; c1[i] = (alwan_simd_lane)lab.a; c2[i] = (alwan_simd_lane)lab.b;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar r = alwan_srgb_eotf(*(alwan_scalar const *)((char const *)in_ch0 + i * in_stride));
        alwan_scalar g = alwan_srgb_eotf(*(alwan_scalar const *)((char const *)in_ch1 + i * in_stride));
        alwan_scalar b = alwan_srgb_eotf(*(alwan_scalar const *)((char const *)in_ch2 + i * in_stride));
        alwan_vec3 v = {{r, g, b}};
        alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
        alwan_xyz wp = {D65_WP_Y1[0], D65_WP_Y1[1], D65_WP_Y1[2]};
        alwan_xyz xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
        alwan_lab lab = alwan_xyz_to_lab_v(xyz_s, wp);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = lab.L;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = lab.a;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = lab.b;
    }
#endif

    return ALWAN_OK;
}

int alwan_lab_to_srgb_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                  alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                  size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd wx = alwan_simd_set1((alwan_simd_lane)D65_WP_Y1[0]);
    alwan_simd wy = alwan_simd_set1((alwan_simd_lane)D65_WP_Y1[1]);
    alwan_simd wz = alwan_simd_set1((alwan_simd_lane)D65_WP_Y1[2]);
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd va = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            /* Lab -> XYZ */
            alwan_simd fy = alwan_simd_mul(alwan_simd_add(vL, alwan_simd_set1(16.0)),
                                                     alwan_simd_set1(1.0 / 116.0));
            alwan_simd fx = alwan_simd_fmadd(va, alwan_simd_set1(1.0 / 500.0), fy);
            alwan_simd fz = alwan_simd_sub(fy, alwan_simd_mul(vb, alwan_simd_set1(1.0 / 200.0)));
            alwan_simd vx = alwan_simd_mul(alwan__lab_f_inv_simd(fx), wx);
            alwan_simd vy = alwan_simd_mul(alwan__lab_f_inv_simd(fy), wy);
            alwan_simd vz = alwan_simd_mul(alwan__lab_f_inv_simd(fz), wz);
            /* XYZ -> sRGB */
            alwan_simd lr, lg, lb;
            alwan__mat3_mul_simd(&lr, &lg, &lb, &XYZ_TO_SRGB, vx, vy, vz);
            alwan_simd_store(&c0[i], alwan__srgb_oetf_simd(lr));
            alwan_simd_store(&c1[i], alwan__srgb_oetf_simd(lg));
            alwan_simd_store(&c2[i], alwan__srgb_oetf_simd(lb));
        }
        for (; i < tile; i++) {
            alwan_lab lab = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_xyz wp = {D65_WP_Y1[0], D65_WP_Y1[1], D65_WP_Y1[2]};
            alwan_xyz xyz = alwan_lab_to_xyz_v(lab, wp);
            alwan_vec3 v = {{xyz.x, xyz.y, xyz.z}};
            alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
            c0[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[0]);
            c1[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[1]);
            c2[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[2]);
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_lab lab = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_xyz wp = {D65_WP_Y1[0], D65_WP_Y1[1], D65_WP_Y1[2]};
        alwan_xyz xyz = alwan_lab_to_xyz_v(lab, wp);
        alwan_vec3 v = {{xyz.x, xyz.y, xyz.z}};
        alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = alwan_srgb_oetf(lin.v[0]);
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = alwan_srgb_oetf(lin.v[1]);
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = alwan_srgb_oetf(lin.v[2]);
    }
#endif

    return ALWAN_OK;
}

int alwan_srgb_to_oklab_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                    alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            /* sRGB EOTF */
            alwan_simd vr = alwan__srgb_eotf_simd(alwan_simd_load(&c0[i]));
            alwan_simd vg = alwan__srgb_eotf_simd(alwan_simd_load(&c1[i]));
            alwan_simd vb = alwan__srgb_eotf_simd(alwan_simd_load(&c2[i]));
            /* sRGB -> XYZ */
            alwan_simd vx, vy, vz;
            alwan__mat3_mul_simd(&vx, &vy, &vz, &SRGB_TO_XYZ, vr, vg, vb);
            /* XYZ -> LMS (OKLAB M1) */
            alwan_simd vl, vm, vs;
            alwan__mat3_mul_simd(&vl, &vm, &vs, &OKLAB_M1, vx, vy, vz);
            /* cbrt */
            vl = alwan_simd_cbrt(vl);
            vm = alwan_simd_cbrt(vm);
            vs = alwan_simd_cbrt(vs);
            /* LMS' -> Lab (OKLAB M2) */
            alwan_simd oL, oa, ob;
            alwan__mat3_mul_simd(&oL, &oa, &ob, &OKLAB_M2, vl, vm, vs);
            alwan_simd_store(&c0[i], oL);
            alwan_simd_store(&c1[i], oa);
            alwan_simd_store(&c2[i], ob);
        }
        for (; i < tile; i++) {
            alwan_scalar r = alwan_srgb_eotf((alwan_scalar)c0[i]);
            alwan_scalar g = alwan_srgb_eotf((alwan_scalar)c1[i]);
            alwan_scalar b = alwan_srgb_eotf((alwan_scalar)c2[i]);
            alwan_vec3 v = {{r, g, b}};
            alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
            alwan_xyz xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
            alwan_oklab ok = alwan_xyz_to_oklab_v(xyz_s);
            c0[i] = (alwan_simd_lane)ok.L; c1[i] = (alwan_simd_lane)ok.a; c2[i] = (alwan_simd_lane)ok.b;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar r = alwan_srgb_eotf(*(alwan_scalar const *)((char const *)in_ch0 + i * in_stride));
        alwan_scalar g = alwan_srgb_eotf(*(alwan_scalar const *)((char const *)in_ch1 + i * in_stride));
        alwan_scalar b = alwan_srgb_eotf(*(alwan_scalar const *)((char const *)in_ch2 + i * in_stride));
        alwan_vec3 v = {{r, g, b}};
        alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
        alwan_xyz xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
        alwan_oklab ok = alwan_xyz_to_oklab_v(xyz_s);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = ok.L;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = ok.a;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = ok.b;
    }
#endif

    return ALWAN_OK;
}

int alwan_oklab_to_srgb_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                    alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd va = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            /* Lab -> LMS' (OKLAB M2_INV) */
            alwan_simd lp, mp, sp;
            alwan__mat3_mul_simd(&lp, &mp, &sp, &OKLAB_M2_INV, vL, va, vb);
            /* cube */
            alwan_simd vl = alwan_simd_mul(lp, alwan_simd_mul(lp, lp));
            alwan_simd vm = alwan_simd_mul(mp, alwan_simd_mul(mp, mp));
            alwan_simd vs = alwan_simd_mul(sp, alwan_simd_mul(sp, sp));
            /* LMS -> XYZ (OKLAB M1_INV) */
            alwan_simd vx, vy, vz;
            alwan__mat3_mul_simd(&vx, &vy, &vz, &OKLAB_M1_INV, vl, vm, vs);
            /* XYZ -> sRGB */
            alwan_simd lr, lg, lb;
            alwan__mat3_mul_simd(&lr, &lg, &lb, &XYZ_TO_SRGB, vx, vy, vz);
            alwan_simd_store(&c0[i], alwan__srgb_oetf_simd(lr));
            alwan_simd_store(&c1[i], alwan__srgb_oetf_simd(lg));
            alwan_simd_store(&c2[i], alwan__srgb_oetf_simd(lb));
        }
        for (; i < tile; i++) {
            alwan_oklab ok = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_xyz xyz = alwan_oklab_to_xyz_v(ok);
            alwan_vec3 v = {{xyz.x, xyz.y, xyz.z}};
            alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
            c0[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[0]);
            c1[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[1]);
            c2[i] = (alwan_simd_lane)alwan_srgb_oetf(lin.v[2]);
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_oklab ok = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_xyz xyz = alwan_oklab_to_xyz_v(ok);
        alwan_vec3 v = {{xyz.x, xyz.y, xyz.z}};
        alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = alwan_srgb_oetf(lin.v[0]);
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = alwan_srgb_oetf(lin.v[1]);
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = alwan_srgb_oetf(lin.v[2]);
    }
#endif

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Batch Delta E Computations
 * ---------------------------------------------------------------- */

int alwan_delta_e_76_batch(alwan_scalar *delta_e_out,
                           alwan_scalar const *lab1_in,
                           alwan_scalar const *lab2_in,
                           size_t count,
                           size_t in1_stride,
                           size_t in2_stride) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in1_ptr = (alwan_scalar const *)((char const *)lab1_in + i * in1_stride);
        alwan_scalar const *in2_ptr = (alwan_scalar const *)((char const *)lab2_in + i * in2_stride);
        alwan_lab lab1 = {in1_ptr[0], in1_ptr[1], in1_ptr[2]};
        alwan_lab lab2 = {in2_ptr[0], in2_ptr[1], in2_ptr[2]};
        delta_e_out[i] = alwan_delta_e_76(&lab1, &lab2);
    }

    return ALWAN_OK;
}

int alwan_delta_e_2000_batch(alwan_scalar *delta_e_out,
                             alwan_scalar const *lab1_in,
                             alwan_scalar const *lab2_in,
                             size_t count,
                             size_t in1_stride,
                             size_t in2_stride) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in1_ptr = (alwan_scalar const *)((char const *)lab1_in + i * in1_stride);
        alwan_scalar const *in2_ptr = (alwan_scalar const *)((char const *)lab2_in + i * in2_stride);
        alwan_lab lab1 = {in1_ptr[0], in1_ptr[1], in1_ptr[2]};
        alwan_lab lab2 = {in2_ptr[0], in2_ptr[1], in2_ptr[2]};
        delta_e_out[i] = alwan_delta_e_2000(&lab1, &lab2);
    }

    return ALWAN_OK;
}

int alwan_delta_e_94_batch(alwan_scalar *delta_e_out,
                           alwan_scalar const *lab1_in,
                           alwan_scalar const *lab2_in,
                           size_t count,
                           size_t in1_stride,
                           size_t in2_stride) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in1_ptr = (alwan_scalar const *)((char const *)lab1_in + i * in1_stride);
        alwan_scalar const *in2_ptr = (alwan_scalar const *)((char const *)lab2_in + i * in2_stride);
        alwan_lab lab1 = {in1_ptr[0], in1_ptr[1], in1_ptr[2]};
        alwan_lab lab2 = {in2_ptr[0], in2_ptr[1], in2_ptr[2]};
        delta_e_out[i] = alwan_delta_e_94(&lab1, &lab2);
    }

    return ALWAN_OK;
}

int alwan_delta_e_cmc_batch(alwan_scalar *delta_e_out,
                            alwan_scalar const *lab1_in,
                            alwan_scalar const *lab2_in,
                            alwan_scalar l,
                            alwan_scalar c,
                            size_t count,
                            size_t in1_stride,
                            size_t in2_stride) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in1_ptr = (alwan_scalar const *)((char const *)lab1_in + i * in1_stride);
        alwan_scalar const *in2_ptr = (alwan_scalar const *)((char const *)lab2_in + i * in2_stride);
        alwan_lab lab1 = {in1_ptr[0], in1_ptr[1], in1_ptr[2]};
        alwan_lab lab2 = {in2_ptr[0], in2_ptr[1], in2_ptr[2]};
        delta_e_out[i] = alwan_delta_e_cmc(&lab1, &lab2, l, c);
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
    case ALWAN_PIXEL_F32: return 4 * sizeof(float);
    case ALWAN_PIXEL_F64: return 4 * sizeof(double);
    default:              return 0;
    }
}

int alwan_image_convert(
    void *dst, alwan_pixel_format dst_fmt, size_t dst_row_stride,
    void const *src, alwan_pixel_format src_fmt, size_t src_row_stride,
    size_t width, size_t height,
    alwan_ctx *ctx,
    alwan_rgb_space_desc const *src_space,
    alwan_rgb_space_desc const *dst_space) {

    if (!dst || !src || !src_space || !dst_space || width == 0 || height == 0) {
        return ALWAN_E_INVALID;
    }

    /* Resolve pixel strides from format */
    size_t const src_px = alwan__pixel_stride(src_fmt);
    size_t const dst_px = alwan__pixel_stride(dst_fmt);
    if (src_px == 0 || dst_px == 0) return ALWAN_E_INVALID;

    /* Derive conversion matrices */
    alwan_mat3x3 src_to_xyz, xyz_to_src;
    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
    } else {
        int status = alwan_rgb_derive_matrices(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    alwan_mat3x3 dst_to_xyz, xyz_to_dst;
    if (dst_space->has_matrices) {
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Check if chromatic adaptation is needed */
    alwan_scalar const tol = ALWAN_LITERAL(1e-6);
    alwan_scalar dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_scalar dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_cat = (ALWAN_ABS(dx) > tol || ALWAN_ABS(dy) > tol);

    /* Precompute a single combined matrix: xyz_to_dst * [cat *] src_to_xyz
     * This reduces per-pixel work to one mat3 multiply. */
    alwan_mat3x3 combined;
    if (need_cat && ctx) {
        alwan_xyy src_xyy, dst_xyy;
        alwan_xyz src_wp, dst_wp;
        src_xyy.x = src_space->white_xy[0];
        src_xyy.y = src_space->white_xy[1];
        src_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&src_wp, &src_xyy);

        dst_xyy.x = dst_space->white_xy[0];
        dst_xyy.y = dst_space->white_xy[1];
        dst_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&dst_wp, &dst_xyy);

        alwan_mat3x3 cat;
        int status = alwan_cat_matrix(&cat, &src_wp, &dst_wp, ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        /* combined = xyz_to_dst * cat * src_to_xyz */
        alwan_mat3x3 tmp = alwan_mat3_mul_v(cat, src_to_xyz);
        combined = alwan_mat3_mul_v(xyz_to_dst, tmp);
    } else {
        /* combined = xyz_to_dst * src_to_xyz */
        combined = alwan_mat3_mul_v(xyz_to_dst, src_to_xyz);
    }

    /* Resolve transfer function pointers */
    alwan_scalar (*eotf_fn)(alwan_scalar) = alwan__resolve_eotf(src_space->eotf);
    alwan_scalar (*oetf_fn)(alwan_scalar) = alwan__resolve_oetf(dst_space->oetf);
    if (!eotf_fn || !oetf_fn) return ALWAN_E_INVALID;

    /* Process image row by row */
    for (size_t y = 0; y < height; y++) {
        char const *src_row = (char const *)src + y * src_row_stride;
        char       *dst_row = (char       *)dst + y * dst_row_stride;

        for (size_t x = 0; x < width; x++) {
            /* Load 3-channel pixel with format conversion */
            alwan_scalar rgb[3];
            alwan__load3_typed(rgb, src_row + x * src_px, src_fmt);

            /* Source EOTF: encoded -> linear */
            alwan_vec3 lin = {{eotf_fn(rgb[0]), eotf_fn(rgb[1]), eotf_fn(rgb[2])}};

            /* Combined matrix: src linear RGB -> dst linear RGB */
            alwan_vec3 dst_lin = alwan_mat3_mulv_v(combined, lin);

            /* Destination OETF: linear -> encoded */
            alwan_scalar out[3] = {
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

int alwan_image_convert_rgba(
    void *dst, alwan_pixel_format dst_fmt, size_t dst_row_stride,
    void const *src, alwan_pixel_format src_fmt, size_t src_row_stride,
    size_t width, size_t height,
    alwan_ctx *ctx,
    alwan_rgb_space_desc const *src_space,
    alwan_rgb_space_desc const *dst_space,
    alwan_alpha_mode alpha_mode) {

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
    alwan_mat3x3 src_to_xyz, xyz_to_src;
    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
    } else {
        int status = alwan_rgb_derive_matrices(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    alwan_mat3x3 dst_to_xyz, xyz_to_dst;
    if (dst_space->has_matrices) {
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Check if chromatic adaptation is needed */
    alwan_scalar const tol = ALWAN_LITERAL(1e-6);
    alwan_scalar dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_scalar dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_cat = (ALWAN_ABS(dx) > tol || ALWAN_ABS(dy) > tol);

    /* Precompute combined matrix: xyz_to_dst * [cat *] src_to_xyz */
    alwan_mat3x3 combined;
    if (need_cat && ctx) {
        alwan_xyy src_xyy, dst_xyy;
        alwan_xyz src_wp, dst_wp;
        src_xyy.x = src_space->white_xy[0];
        src_xyy.y = src_space->white_xy[1];
        src_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&src_wp, &src_xyy);

        dst_xyy.x = dst_space->white_xy[0];
        dst_xyy.y = dst_space->white_xy[1];
        dst_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&dst_wp, &dst_xyy);

        alwan_mat3x3 cat;
        int status = alwan_cat_matrix(&cat, &src_wp, &dst_wp, ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        alwan_mat3x3 tmp = alwan_mat3_mul_v(cat, src_to_xyz);
        combined = alwan_mat3_mul_v(xyz_to_dst, tmp);
    } else {
        combined = alwan_mat3_mul_v(xyz_to_dst, src_to_xyz);
    }

    /* Resolve transfer function pointers */
    alwan_scalar (*eotf_fn)(alwan_scalar) = alwan__resolve_eotf(src_space->eotf);
    alwan_scalar (*oetf_fn)(alwan_scalar) = alwan__resolve_oetf(dst_space->oetf);
    if (!eotf_fn || !oetf_fn) return ALWAN_E_INVALID;

    int const premul = (alpha_mode == ALWAN_ALPHA_PREMULTIPLIED);

    /* Process image row by row */
    for (size_t y = 0; y < height; y++) {
        char const *src_row = (char const *)src + y * src_row_stride;
        char       *dst_row = (char       *)dst + y * dst_row_stride;

        for (size_t x = 0; x < width; x++) {
            char const *src_pixel = src_row + x * src_px;
            char       *dst_pixel = dst_row + x * dst_px;

            /* Load RGB channels (first 3 elements of 4-channel pixel) */
            alwan_scalar rgb[3];
            alwan__load3_typed(rgb, src_pixel, src_fmt);

            /* Load alpha channel (4th element) */
            alwan_scalar alpha = alwan__load1_typed(src_pixel + 3 * src_elem, src_fmt);

            /* Unpremultiply if needed */
            if (premul && alpha > ALWAN_LITERAL(0.0)) {
                alwan_scalar inv_a = ALWAN_LITERAL(1.0) / alpha;
                rgb[0] *= inv_a;
                rgb[1] *= inv_a;
                rgb[2] *= inv_a;
            }

            /* Source EOTF: encoded -> linear */
            alwan_vec3 lin = {{eotf_fn(rgb[0]), eotf_fn(rgb[1]), eotf_fn(rgb[2])}};

            /* Combined matrix: src linear RGB -> dst linear RGB */
            alwan_vec3 dst_lin = alwan_mat3_mulv_v(combined, lin);

            /* Destination OETF: linear -> encoded */
            alwan_scalar out[3] = {
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
