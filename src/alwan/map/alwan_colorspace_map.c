/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Color Space Conversions - True SIMD vectorized
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_colorspace_core.h"
#include "../core/alwan_math_core.h"

/* ----------------------------------------------------------------
 * Map XYZ <-> Lab Kernels
 * ---------------------------------------------------------------- */

static void alwan__xyz_to_lab_kernel_wp(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                         alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                         size_t n, alwan_xyz const *white_xyz) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd inv_wx = alwan_simd_set1(1.0 / (alwan_simd_lane)white_xyz->x);
    alwan_simd inv_wy = alwan_simd_set1(1.0 / (alwan_simd_lane)white_xyz->y);
    alwan_simd inv_wz = alwan_simd_set1(1.0 / (alwan_simd_lane)white_xyz->z);
    for (; i + W <= n; i += W) {
        alwan_simd vx = alwan_simd_load(&i0[i]);
        alwan_simd vy = alwan_simd_load(&i1[i]);
        alwan_simd vz = alwan_simd_load(&i2[i]);
        alwan_simd xr = alwan_simd_mul(vx, inv_wx);
        alwan_simd yr = alwan_simd_mul(vy, inv_wy);
        alwan_simd zr = alwan_simd_mul(vz, inv_wz);
        alwan_simd fx = alwan__lab_f_simd(xr);
        alwan_simd fy = alwan__lab_f_simd(yr);
        alwan_simd fz = alwan__lab_f_simd(zr);
        alwan_simd oL = alwan_simd_fmsub(alwan_simd_set1(116.0), fy, alwan_simd_set1(16.0));
        alwan_simd oa = alwan_simd_mul(alwan_simd_set1(500.0), alwan_simd_sub(fx, fy));
        alwan_simd ob = alwan_simd_mul(alwan_simd_set1(200.0), alwan_simd_sub(fy, fz));
        alwan_simd_store(&o0[i], oL);
        alwan_simd_store(&o1[i], oa);
        alwan_simd_store(&o2[i], ob);
    }
#endif
    for (; i < n; i++) {
        alwan_xyz v = {(alwan_scalar)i0[i], (alwan_scalar)i1[i], (alwan_scalar)i2[i]};
        alwan_lab r = alwan_xyz_to_lab_v(v, *white_xyz);
        o0[i] = (alwan_simd_lane)r.L; o1[i] = (alwan_simd_lane)r.a; o2[i] = (alwan_simd_lane)r.b;
    }
}

static void alwan__lab_to_xyz_kernel_wp(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                         alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                         size_t n, alwan_xyz const *white_xyz) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd wx = alwan_simd_set1((alwan_simd_lane)white_xyz->x);
    alwan_simd wy = alwan_simd_set1((alwan_simd_lane)white_xyz->y);
    alwan_simd wz = alwan_simd_set1((alwan_simd_lane)white_xyz->z);
    for (; i + W <= n; i += W) {
        alwan_simd vL = alwan_simd_load(&i0[i]);
        alwan_simd va = alwan_simd_load(&i1[i]);
        alwan_simd vb = alwan_simd_load(&i2[i]);
        alwan_simd fy = alwan_simd_mul(
            alwan_simd_add(vL, alwan_simd_set1(16.0)),
            alwan_simd_set1(1.0 / 116.0));
        alwan_simd fx = alwan_simd_fmadd(va, alwan_simd_set1(1.0 / 500.0), fy);
        alwan_simd fz = alwan_simd_sub(fy, alwan_simd_mul(vb, alwan_simd_set1(1.0 / 200.0)));
        alwan_simd ox = alwan_simd_mul(wx, alwan__lab_f_inv_simd(fx));
        alwan_simd oy = alwan_simd_mul(wy, alwan__lab_f_inv_simd(fy));
        alwan_simd oz = alwan_simd_mul(wz, alwan__lab_f_inv_simd(fz));
        alwan_simd_store(&o0[i], ox);
        alwan_simd_store(&o1[i], oy);
        alwan_simd_store(&o2[i], oz);
    }
#endif
    for (; i < n; i++) {
        alwan_lab v = {(alwan_scalar)i0[i], (alwan_scalar)i1[i], (alwan_scalar)i2[i]};
        alwan_xyz r = alwan_lab_to_xyz_v(v, *white_xyz);
        o0[i] = (alwan_simd_lane)r.x; o1[i] = (alwan_simd_lane)r.y; o2[i] = (alwan_simd_lane)r.z;
    }
}

/* ----------------------------------------------------------------
 * Map XYZ <-> Lab Conversions
 * ---------------------------------------------------------------- */

int alwan_xyz_to_lab_map_interleave(alwan_scalar *lab_out,
                          alwan_scalar const *xyz_in,
                          alwan_xyz const *white_xyz,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!xyz_in || !lab_out || !white_xyz || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_lab_kernel_wp(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NORM_MUL(d0, tile, 0.01);
        alwan__store_tile_aos3(lab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_xyz_to_lab_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                             void const *in, alwan_pixel_format in_fmt,
                             alwan_xyz const *white_xyz,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || !white_xyz || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        alwan__xyz_to_lab_kernel_wp(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NORM_MUL(d0, tile, 0.01);
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_lab_to_xyz_map_interleave(alwan_scalar *xyz_out,
                          alwan_scalar const *lab_in,
                          alwan_xyz const *white_xyz,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!lab_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, lab_in, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);
        alwan__lab_to_xyz_kernel_wp(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_lab_to_xyz_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                             void const *in, alwan_pixel_format in_fmt,
                             alwan_xyz const *white_xyz,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || !white_xyz || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);
        alwan__lab_to_xyz_kernel_wp(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Map XYZ <-> Luv Kernels
 * Complex select patterns -- vectorized with per-element SIMD selects
 * ---------------------------------------------------------------- */

static void alwan__xyz_to_luv_kernel_wp(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                         alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                         size_t n, alwan_xyz const *white_xyz) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    /* Precompute white point u'n, v'n as scalars, broadcast */
    alwan_simd_lane wdn = (alwan_simd_lane)(white_xyz->x + 15.0 * white_xyz->y + 3.0 * white_xyz->z);
    alwan_simd_lane upn = (wdn > ALWAN_MAP_DIV_GUARD) ? (alwan_simd_lane)(4.0 * white_xyz->x) / wdn : 0.0;
    alwan_simd_lane vpn = (wdn > ALWAN_MAP_DIV_GUARD) ? (alwan_simd_lane)(9.0 * white_xyz->y) / wdn : 0.0;
    alwan_simd v_upn = alwan_simd_set1(upn);
    alwan_simd v_vpn = alwan_simd_set1(vpn);
    alwan_simd v_inv_wy = alwan_simd_set1(1.0 / (alwan_simd_lane)white_xyz->y);
    alwan_simd v_eps = alwan_simd_set1(ALWAN_MAP_DIV_GUARD);
    alwan_simd v_zero = alwan_simd_zero();
    for (; i + W <= n; i += W) {
        alwan_simd vx = alwan_simd_load(&i0[i]);
        alwan_simd vy = alwan_simd_load(&i1[i]);
        alwan_simd vz = alwan_simd_load(&i2[i]);

        /* denom = x + 15*y + 3*z */
        alwan_simd denom = alwan_simd_fmadd(alwan_simd_set1(15.0), vy,
            alwan_simd_fmadd(alwan_simd_set1(3.0), vz, vx));
        alwan_simd_mask small = alwan_simd_cmplt(denom, v_eps);
        /* safe_denom: replace near-zero with 1 to avoid division issues */
        alwan_simd safe_denom = alwan_simd_select(small, alwan_simd_set1(1.0), denom);
        alwan_simd up = alwan_simd_select(small, v_zero,
            alwan_simd_div(alwan_simd_mul(alwan_simd_set1(4.0), vx), safe_denom));
        alwan_simd vp = alwan_simd_select(small, v_zero,
            alwan_simd_div(alwan_simd_mul(alwan_simd_set1(9.0), vy), safe_denom));

        /* L = 116*lab_f(Y/Yn) - 16 */
        alwan_simd yr = alwan_simd_mul(vy, v_inv_wy);
        alwan_simd oL = alwan_simd_fmsub(alwan_simd_set1(116.0), alwan__lab_f_simd(yr), alwan_simd_set1(16.0));

        /* u = 13*L*(u' - u'n), v = 13*L*(v' - v'n) */
        alwan_simd L13 = alwan_simd_mul(alwan_simd_set1(13.0), oL);
        alwan_simd ou = alwan_simd_mul(L13, alwan_simd_sub(up, v_upn));
        alwan_simd ov = alwan_simd_mul(L13, alwan_simd_sub(vp, v_vpn));

        alwan_simd_store(&o0[i], oL);
        alwan_simd_store(&o1[i], ou);
        alwan_simd_store(&o2[i], ov);
    }
#endif
    for (; i < n; i++) {
        alwan_xyz v = {(alwan_scalar)i0[i], (alwan_scalar)i1[i], (alwan_scalar)i2[i]};
        alwan_luv r = alwan_xyz_to_luv_v(v, *white_xyz);
        o0[i] = (alwan_simd_lane)r.L; o1[i] = (alwan_simd_lane)r.u; o2[i] = (alwan_simd_lane)r.v;
    }
}

static void alwan__luv_to_xyz_kernel_wp(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                         alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                         size_t n, alwan_xyz const *white_xyz) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd_lane wdn = (alwan_simd_lane)(white_xyz->x + 15.0 * white_xyz->y + 3.0 * white_xyz->z);
    alwan_simd_lane upn = (wdn > ALWAN_MAP_DIV_GUARD) ? (alwan_simd_lane)(4.0 * white_xyz->x) / wdn : 0.0;
    alwan_simd_lane vpn = (wdn > ALWAN_MAP_DIV_GUARD) ? (alwan_simd_lane)(9.0 * white_xyz->y) / wdn : 0.0;
    alwan_simd v_upn = alwan_simd_set1(upn);
    alwan_simd v_vpn = alwan_simd_set1(vpn);
    alwan_simd v_wy  = alwan_simd_set1((alwan_simd_lane)white_xyz->y);
    alwan_simd v_eps = alwan_simd_set1(ALWAN_MAP_DIV_GUARD);
    alwan_simd v_zero = alwan_simd_zero();
    for (; i + W <= n; i += W) {
        alwan_simd vL = alwan_simd_load(&i0[i]);
        alwan_simd vu = alwan_simd_load(&i1[i]);
        alwan_simd vv = alwan_simd_load(&i2[i]);

        /* fy = (L+16)/116, Y = wy * f_inv(fy) */
        alwan_simd fy = alwan_simd_mul(
            alwan_simd_add(vL, alwan_simd_set1(16.0)),
            alwan_simd_set1(1.0 / 116.0));
        alwan_simd oY = alwan_simd_mul(v_wy, alwan__lab_f_inv_simd(fy));

        /* u' = u/(13*L) + u'n, v' = v/(13*L) + v'n */
        alwan_simd L13 = alwan_simd_mul(alwan_simd_set1(13.0), vL);
        alwan_simd abs_L = alwan_simd_abs(vL);
        alwan_simd_mask L_small = alwan_simd_cmplt(abs_L, v_eps);
        alwan_simd safe_L13 = alwan_simd_select(L_small, alwan_simd_set1(1.0), L13);
        alwan_simd up = alwan_simd_select(L_small, v_zero,
            alwan_simd_add(alwan_simd_div(vu, safe_L13), v_upn));
        alwan_simd vp = alwan_simd_select(L_small, v_zero,
            alwan_simd_add(alwan_simd_div(vv, safe_L13), v_vpn));

        /* X = Y * 9*u' / (4*v'), Z = Y * (12 - 3*u' - 20*v') / (4*v') */
        alwan_simd abs_vp = alwan_simd_abs(vp);
        alwan_simd_mask vp_small = alwan_simd_cmplt(abs_vp, v_eps);
        alwan_simd safe_4vp = alwan_simd_select(vp_small, alwan_simd_set1(1.0),
            alwan_simd_mul(alwan_simd_set1(4.0), vp));
        alwan_simd oX = alwan_simd_select(vp_small, v_zero,
            alwan_simd_div(alwan_simd_mul(oY, alwan_simd_mul(alwan_simd_set1(9.0), up)), safe_4vp));
        /* 12 - 3*u' - 20*v' */
        alwan_simd znum = alwan_simd_sub(
            alwan_simd_sub(alwan_simd_set1(12.0),
                alwan_simd_mul(alwan_simd_set1(3.0), up)),
            alwan_simd_mul(alwan_simd_set1(20.0), vp));
        alwan_simd oZ = alwan_simd_select(vp_small, v_zero,
            alwan_simd_div(alwan_simd_mul(oY, znum), safe_4vp));

        alwan_simd_store(&o0[i], oX);
        alwan_simd_store(&o1[i], oY);
        alwan_simd_store(&o2[i], oZ);
    }
#endif
    for (; i < n; i++) {
        alwan_luv v = {(alwan_scalar)i0[i], (alwan_scalar)i1[i], (alwan_scalar)i2[i]};
        alwan_xyz r = alwan_luv_to_xyz_v(v, *white_xyz);
        o0[i] = (alwan_simd_lane)r.x; o1[i] = (alwan_simd_lane)r.y; o2[i] = (alwan_simd_lane)r.z;
    }
}

/* ----------------------------------------------------------------
 * Map XYZ <-> Luv Conversions
 * ---------------------------------------------------------------- */

int alwan_xyz_to_luv_map_interleave(alwan_scalar *luv_out,
                          alwan_scalar const *xyz_in,
                          alwan_xyz const *white_xyz,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!xyz_in || !luv_out || !white_xyz || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_luv_kernel_wp(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NORM_MUL(d0, tile, 0.01);
        alwan__store_tile_aos3(luv_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_xyz_to_luv_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                             void const *in, alwan_pixel_format in_fmt,
                             alwan_xyz const *white_xyz,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || !white_xyz || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        alwan__xyz_to_luv_kernel_wp(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NORM_MUL(d0, tile, 0.01);
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_luv_to_xyz_map_interleave(alwan_scalar *xyz_out,
                          alwan_scalar const *luv_in,
                          alwan_xyz const *white_xyz,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!luv_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, luv_in, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);
        alwan__luv_to_xyz_kernel_wp(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_luv_to_xyz_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                             void const *in, alwan_pixel_format in_fmt,
                             alwan_xyz const *white_xyz,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || !white_xyz || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);
        alwan__luv_to_xyz_kernel_wp(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Map Lab <-> LCh Conversions
 * ---------------------------------------------------------------- */

int alwan_lab_to_lch_map_interleave(alwan_scalar *lch_out,
                          alwan_scalar const *lab_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!lab_in || !lch_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, lab_in, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd va = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            alwan_simd vC = alwan_simd_sqrt(
                alwan_simd_fmadd(va, va, alwan_simd_mul(vb, vb)));
            alwan_simd vh = alwan_simd_atan2(vb, va);
            /* rad -> deg, wrap negative to [0, 360) */
            alwan_simd const rad2deg = alwan_simd_set1((alwan_simd_lane)(180.0 / ALWAN_PI));
            alwan_simd const three60 = alwan_simd_set1((alwan_simd_lane)360.0);
            vh = alwan_simd_mul(vh, rad2deg);
            vh = alwan_simd_select(alwan_simd_cmpge(vh, alwan_simd_zero()),
                                   vh, alwan_simd_add(vh, three60));
            alwan_simd_store(&c0[i], vL);
            alwan_simd_store(&c1[i], vC);
            alwan_simd_store(&c2[i], vh);
        }
        for (; i < tile; i++) {
            alwan_lab v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_lch r = alwan_lab_to_lch_v(v);
            c0[i] = (alwan_simd_lane)r.L; c1[i] = (alwan_simd_lane)r.C; c2[i] = (alwan_simd_lane)r.h;
        }

        ALWAN_MAP_NORM_MUL(c0, tile, 0.01);
        ALWAN_MAP_NORM_MUL(c2, tile, 1.0/360.0);
        alwan__store_tile_aos3(lch_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)lab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)lch_out + i * out_stride);
        alwan_lab lab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_lch lch = alwan_lab_to_lch_v(lab);
        out_ptr[0] = lch.L; out_ptr[1] = lch.C; out_ptr[2] = lch.h;
    }
#endif
    return ALWAN_OK;
}

int alwan_lch_to_lab_map_interleave(alwan_scalar *lab_out,
                          alwan_scalar const *lch_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!lch_in || !lab_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, lch_in, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);
        ALWAN_MAP_NORM_MUL(c2, tile, 360.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd vC = alwan_simd_load(&c1[i]);
            alwan_simd vh = alwan_simd_load(&c2[i]);
            /* deg -> rad */
            alwan_simd const deg2rad = alwan_simd_set1((alwan_simd_lane)(ALWAN_PI / 180.0));
            alwan_simd vh_rad = alwan_simd_mul(vh, deg2rad);
            alwan_simd va = alwan_simd_mul(vC, alwan_simd_cos(vh_rad));
            alwan_simd vb = alwan_simd_mul(vC, alwan_simd_sin(vh_rad));
            alwan_simd_store(&c0[i], vL);
            alwan_simd_store(&c1[i], va);
            alwan_simd_store(&c2[i], vb);
        }
        for (; i < tile; i++) {
            alwan_lch v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_lab r = alwan_lch_to_lab_v(v);
            c0[i] = (alwan_simd_lane)r.L; c1[i] = (alwan_simd_lane)r.a; c2[i] = (alwan_simd_lane)r.b;
        }

        ALWAN_MAP_NORM_MUL(c0, tile, 0.01);
        alwan__store_tile_aos3(lab_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)lch_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)lab_out + i * out_stride);
        alwan_lch lch = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_lab lab = alwan_lch_to_lab_v(lch);
        out_ptr[0] = lab.L; out_ptr[1] = lab.a; out_ptr[2] = lab.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Map Luv <-> LCh(uv) Conversions
 * ---------------------------------------------------------------- */

int alwan_luv_to_lchuv_map_interleave(alwan_scalar *lchuv_out,
                            alwan_scalar const *luv_in,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride) {
    if (!luv_in || !lchuv_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, luv_in, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd vu = alwan_simd_load(&c1[i]);
            alwan_simd vv = alwan_simd_load(&c2[i]);
            alwan_simd vC = alwan_simd_sqrt(
                alwan_simd_fmadd(vu, vu, alwan_simd_mul(vv, vv)));
            alwan_simd vh = alwan_simd_atan2(vv, vu);
            /* rad -> deg, wrap negative to [0, 360) */
            alwan_simd const rad2deg = alwan_simd_set1((alwan_simd_lane)(180.0 / ALWAN_PI));
            alwan_simd const three60 = alwan_simd_set1((alwan_simd_lane)360.0);
            vh = alwan_simd_mul(vh, rad2deg);
            vh = alwan_simd_select(alwan_simd_cmpge(vh, alwan_simd_zero()),
                                   vh, alwan_simd_add(vh, three60));
            alwan_simd_store(&c0[i], vL);
            alwan_simd_store(&c1[i], vC);
            alwan_simd_store(&c2[i], vh);
        }
        for (; i < tile; i++) {
            alwan_luv v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_lchuv r = alwan_luv_to_lchuv_v(v);
            c0[i] = (alwan_simd_lane)r.L; c1[i] = (alwan_simd_lane)r.C; c2[i] = (alwan_simd_lane)r.h;
        }

        ALWAN_MAP_NORM_MUL(c0, tile, 0.01);
        ALWAN_MAP_NORM_MUL(c2, tile, 1.0/360.0);
        alwan__store_tile_aos3(lchuv_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)luv_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)lchuv_out + i * out_stride);
        alwan_luv luv = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_lchuv lchuv = alwan_luv_to_lchuv_v(luv);
        out_ptr[0] = lchuv.L; out_ptr[1] = lchuv.C; out_ptr[2] = lchuv.h;
    }
#endif
    return ALWAN_OK;
}

int alwan_lchuv_to_luv_map_interleave(alwan_scalar *luv_out,
                            alwan_scalar const *lchuv_in,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride) {
    if (!lchuv_in || !luv_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, lchuv_in, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);
        ALWAN_MAP_NORM_MUL(c2, tile, 360.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd vC = alwan_simd_load(&c1[i]);
            alwan_simd vh = alwan_simd_load(&c2[i]);
            /* deg -> rad */
            alwan_simd const deg2rad = alwan_simd_set1((alwan_simd_lane)(ALWAN_PI / 180.0));
            alwan_simd vh_rad = alwan_simd_mul(vh, deg2rad);
            alwan_simd vu = alwan_simd_mul(vC, alwan_simd_cos(vh_rad));
            alwan_simd vv = alwan_simd_mul(vC, alwan_simd_sin(vh_rad));
            alwan_simd_store(&c0[i], vL);
            alwan_simd_store(&c1[i], vu);
            alwan_simd_store(&c2[i], vv);
        }
        for (; i < tile; i++) {
            alwan_lchuv v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_luv r = alwan_lchuv_to_luv_v(v);
            c0[i] = (alwan_simd_lane)r.L; c1[i] = (alwan_simd_lane)r.u; c2[i] = (alwan_simd_lane)r.v;
        }

        ALWAN_MAP_NORM_MUL(c0, tile, 0.01);
        alwan__store_tile_aos3(luv_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)lchuv_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)luv_out + i * out_stride);
        alwan_lchuv lchuv = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_luv luv = alwan_lchuv_to_luv_v(lchuv);
        out_ptr[0] = luv.L; out_ptr[1] = luv.u; out_ptr[2] = luv.v;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Map XYZ <-> xyY Conversions
 * ---------------------------------------------------------------- */

int alwan_xyz_to_xyy_map_interleave(alwan_scalar *xyy_out,
                          alwan_scalar const *xyz_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!xyz_in || !xyy_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd v_eps = alwan_simd_set1(ALWAN_MAP_DIV_GUARD);
    alwan_simd v_zero = alwan_simd_zero();
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            alwan_simd sum = alwan_simd_add(vx, alwan_simd_add(vy, vz));
            alwan_simd_mask small = alwan_simd_cmplt(sum, v_eps);
            alwan_simd safe_sum = alwan_simd_select(small, alwan_simd_set1(1.0), sum);
            alwan_simd ox = alwan_simd_select(small, v_zero, alwan_simd_div(vx, safe_sum));
            alwan_simd oy = alwan_simd_select(small, v_zero, alwan_simd_div(vy, safe_sum));
            alwan_simd oY = alwan_simd_select(small, v_zero, vy);
            alwan_simd_store(&c0[i], ox);
            alwan_simd_store(&c1[i], oy);
            alwan_simd_store(&c2[i], oY);
        }
        for (; i < tile; i++) {
            alwan_xyz v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_xyy r = alwan_xyz_to_xyy_v(v);
            c0[i] = (alwan_simd_lane)r.x; c1[i] = (alwan_simd_lane)r.y; c2[i] = (alwan_simd_lane)r.Y;
        }

        alwan__store_tile_aos3(xyy_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyy_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyy xyy = alwan_xyz_to_xyy_v(xyz);
        out_ptr[0] = xyy.x; out_ptr[1] = xyy.y; out_ptr[2] = xyy.Y;
    }
#endif
    return ALWAN_OK;
}

int alwan_xyy_to_xyz_map_interleave(alwan_scalar *xyz_out,
                          alwan_scalar const *xyy_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!xyy_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd v_eps = alwan_simd_set1(ALWAN_MAP_DIV_GUARD);
    alwan_simd v_zero = alwan_simd_zero();
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyy_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vY = alwan_simd_load(&c2[i]);
            alwan_simd_mask small = alwan_simd_cmplt(vy, v_eps);
            alwan_simd safe_y = alwan_simd_select(small, alwan_simd_set1(1.0), vy);
            alwan_simd oX = alwan_simd_select(small, v_zero,
                alwan_simd_div(alwan_simd_mul(vx, vY), safe_y));
            alwan_simd oYout = alwan_simd_select(small, v_zero, vY);
            /* z_chrom = 1 - x - y; Z = z_chrom * Y / y */
            alwan_simd z_chrom = alwan_simd_sub(
                alwan_simd_sub(alwan_simd_set1(1.0), vx), vy);
            alwan_simd oZ = alwan_simd_select(small, v_zero,
                alwan_simd_div(alwan_simd_mul(z_chrom, vY), safe_y));
            alwan_simd_store(&c0[i], oX);
            alwan_simd_store(&c1[i], oYout);
            alwan_simd_store(&c2[i], oZ);
        }
        for (; i < tile; i++) {
            alwan_xyy v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_xyz r = alwan_xyy_to_xyz_v(v);
            c0[i] = (alwan_simd_lane)r.x; c1[i] = (alwan_simd_lane)r.y; c2[i] = (alwan_simd_lane)r.z;
        }

        alwan__store_tile_aos3(xyz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyy_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_xyy xyy = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz xyz = alwan_xyy_to_xyz_v(xyy);
        out_ptr[0] = xyz.x; out_ptr[1] = xyz.y; out_ptr[2] = xyz.z;
    }
#endif
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

int alwan_xyz_to_lab_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                 alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                 alwan_xyz const *white_xyz,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || !white_xyz || count == 0)
        return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        alwan__xyz_to_lab_kernel_wp(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NORM_MUL(d0, tile, 0.01);
        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_lab_to_xyz_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                 alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                 alwan_xyz const *white_xyz,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || !white_xyz || count == 0)
        return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);
        alwan__lab_to_xyz_kernel_wp(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_xyz_to_luv_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                 alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                 alwan_xyz const *white_xyz,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || !white_xyz || count == 0)
        return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        alwan__xyz_to_luv_kernel_wp(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NORM_MUL(d0, tile, 0.01);
        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_luv_to_xyz_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                 alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                 alwan_xyz const *white_xyz,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || !white_xyz || count == 0)
        return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);
        alwan__luv_to_xyz_kernel_wp(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_lab_to_lch_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                 alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0)
        return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd va = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            alwan_simd vC = alwan_simd_sqrt(
                alwan_simd_fmadd(va, va, alwan_simd_mul(vb, vb)));
            alwan_simd vh = alwan_simd_atan2(vb, va);
            /* rad -> deg, wrap negative to [0, 360) */
            alwan_simd const rad2deg = alwan_simd_set1((alwan_simd_lane)(180.0 / ALWAN_PI));
            alwan_simd const three60 = alwan_simd_set1((alwan_simd_lane)360.0);
            vh = alwan_simd_mul(vh, rad2deg);
            vh = alwan_simd_select(alwan_simd_cmpge(vh, alwan_simd_zero()),
                                   vh, alwan_simd_add(vh, three60));
            alwan_simd_store(&c0[i], vL);
            alwan_simd_store(&c1[i], vC);
            alwan_simd_store(&c2[i], vh);
        }
        for (; i < tile; i++) {
            alwan_lab v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_lch r = alwan_lab_to_lch_v(v);
            c0[i] = (alwan_simd_lane)r.L; c1[i] = (alwan_simd_lane)r.C; c2[i] = (alwan_simd_lane)r.h;
        }

        ALWAN_MAP_NORM_MUL(c0, tile, 0.01);
        ALWAN_MAP_NORM_MUL(c2, tile, 1.0/360.0);
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
        alwan_lch lch = alwan_lab_to_lch_v(lab);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = lch.L;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = lch.C;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = lch.h;
    }
#endif
    return ALWAN_OK;
}

int alwan_lch_to_lab_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                 alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0)
        return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);
        ALWAN_MAP_NORM_MUL(c2, tile, 360.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd vC = alwan_simd_load(&c1[i]);
            alwan_simd vh = alwan_simd_load(&c2[i]);
            /* deg -> rad */
            alwan_simd const deg2rad = alwan_simd_set1((alwan_simd_lane)(ALWAN_PI / 180.0));
            alwan_simd vh_rad = alwan_simd_mul(vh, deg2rad);
            alwan_simd va = alwan_simd_mul(vC, alwan_simd_cos(vh_rad));
            alwan_simd vb = alwan_simd_mul(vC, alwan_simd_sin(vh_rad));
            alwan_simd_store(&c0[i], vL);
            alwan_simd_store(&c1[i], va);
            alwan_simd_store(&c2[i], vb);
        }
        for (; i < tile; i++) {
            alwan_lch v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_lab r = alwan_lch_to_lab_v(v);
            c0[i] = (alwan_simd_lane)r.L; c1[i] = (alwan_simd_lane)r.a; c2[i] = (alwan_simd_lane)r.b;
        }

        ALWAN_MAP_NORM_MUL(c0, tile, 0.01);
        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_lch lch = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_lab lab = alwan_lch_to_lab_v(lch);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = lab.L;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = lab.a;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = lab.b;
    }
#endif
    return ALWAN_OK;
}

int alwan_luv_to_lchuv_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                   alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0)
        return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd vu = alwan_simd_load(&c1[i]);
            alwan_simd vv = alwan_simd_load(&c2[i]);
            alwan_simd vC = alwan_simd_sqrt(
                alwan_simd_fmadd(vu, vu, alwan_simd_mul(vv, vv)));
            alwan_simd vh = alwan_simd_atan2(vv, vu);
            /* rad -> deg, wrap negative to [0, 360) */
            alwan_simd const rad2deg = alwan_simd_set1((alwan_simd_lane)(180.0 / ALWAN_PI));
            alwan_simd const three60 = alwan_simd_set1((alwan_simd_lane)360.0);
            vh = alwan_simd_mul(vh, rad2deg);
            vh = alwan_simd_select(alwan_simd_cmpge(vh, alwan_simd_zero()),
                                   vh, alwan_simd_add(vh, three60));
            alwan_simd_store(&c0[i], vL);
            alwan_simd_store(&c1[i], vC);
            alwan_simd_store(&c2[i], vh);
        }
        for (; i < tile; i++) {
            alwan_luv v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_lchuv r = alwan_luv_to_lchuv_v(v);
            c0[i] = (alwan_simd_lane)r.L; c1[i] = (alwan_simd_lane)r.C; c2[i] = (alwan_simd_lane)r.h;
        }

        ALWAN_MAP_NORM_MUL(c0, tile, 0.01);
        ALWAN_MAP_NORM_MUL(c2, tile, 1.0/360.0);
        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_luv luv = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_lchuv lchuv = alwan_luv_to_lchuv_v(luv);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = lchuv.L;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = lchuv.C;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = lchuv.h;
    }
#endif
    return ALWAN_OK;
}

int alwan_lchuv_to_luv_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                   alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0)
        return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 100.0);
        ALWAN_MAP_NORM_MUL(c2, tile, 360.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd vC = alwan_simd_load(&c1[i]);
            alwan_simd vh = alwan_simd_load(&c2[i]);
            /* deg -> rad */
            alwan_simd const deg2rad = alwan_simd_set1((alwan_simd_lane)(ALWAN_PI / 180.0));
            alwan_simd vh_rad = alwan_simd_mul(vh, deg2rad);
            alwan_simd vu = alwan_simd_mul(vC, alwan_simd_cos(vh_rad));
            alwan_simd vv = alwan_simd_mul(vC, alwan_simd_sin(vh_rad));
            alwan_simd_store(&c0[i], vL);
            alwan_simd_store(&c1[i], vu);
            alwan_simd_store(&c2[i], vv);
        }
        for (; i < tile; i++) {
            alwan_lchuv v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_luv r = alwan_lchuv_to_luv_v(v);
            c0[i] = (alwan_simd_lane)r.L; c1[i] = (alwan_simd_lane)r.u; c2[i] = (alwan_simd_lane)r.v;
        }

        ALWAN_MAP_NORM_MUL(c0, tile, 0.01);
        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_lchuv lchuv = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_luv luv = alwan_lchuv_to_luv_v(lchuv);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = luv.L;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = luv.u;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = luv.v;
    }
#endif
    return ALWAN_OK;
}

int alwan_xyz_to_xyy_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                 alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0)
        return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd v_eps = alwan_simd_set1(ALWAN_MAP_DIV_GUARD);
    alwan_simd v_zero = alwan_simd_zero();
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
            alwan_simd sum = alwan_simd_add(vx, alwan_simd_add(vy, vz));
            alwan_simd_mask small = alwan_simd_cmplt(sum, v_eps);
            alwan_simd safe_sum = alwan_simd_select(small, alwan_simd_set1(1.0), sum);
            alwan_simd ox = alwan_simd_select(small, v_zero, alwan_simd_div(vx, safe_sum));
            alwan_simd oy = alwan_simd_select(small, v_zero, alwan_simd_div(vy, safe_sum));
            alwan_simd oY = alwan_simd_select(small, v_zero, vy);
            alwan_simd_store(&c0[i], ox);
            alwan_simd_store(&c1[i], oy);
            alwan_simd_store(&c2[i], oY);
        }
        for (; i < tile; i++) {
            alwan_xyz v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_xyy r = alwan_xyz_to_xyy_v(v);
            c0[i] = (alwan_simd_lane)r.x; c1[i] = (alwan_simd_lane)r.y; c2[i] = (alwan_simd_lane)r.Y;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_xyz xyz = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_xyy xyy = alwan_xyz_to_xyy_v(xyz);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = xyy.x;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = xyy.y;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = xyy.Y;
    }
#endif
    return ALWAN_OK;
}

int alwan_xyy_to_xyz_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                 alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0)
        return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd v_eps = alwan_simd_set1(ALWAN_MAP_DIV_GUARD);
    alwan_simd v_zero = alwan_simd_zero();
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
            alwan_simd vY = alwan_simd_load(&c2[i]);
            alwan_simd_mask small = alwan_simd_cmplt(vy, v_eps);
            alwan_simd safe_y = alwan_simd_select(small, alwan_simd_set1(1.0), vy);
            alwan_simd oX = alwan_simd_select(small, v_zero,
                alwan_simd_div(alwan_simd_mul(vx, vY), safe_y));
            alwan_simd oYout = alwan_simd_select(small, v_zero, vY);
            /* z_chrom = 1 - x - y; Z = z_chrom * Y / y */
            alwan_simd z_chrom = alwan_simd_sub(
                alwan_simd_sub(alwan_simd_set1(1.0), vx), vy);
            alwan_simd oZ = alwan_simd_select(small, v_zero,
                alwan_simd_div(alwan_simd_mul(z_chrom, vY), safe_y));
            alwan_simd_store(&c0[i], oX);
            alwan_simd_store(&c1[i], oYout);
            alwan_simd_store(&c2[i], oZ);
        }
        for (; i < tile; i++) {
            alwan_xyy v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_xyz r = alwan_xyy_to_xyz_v(v);
            c0[i] = (alwan_simd_lane)r.x; c1[i] = (alwan_simd_lane)r.y; c2[i] = (alwan_simd_lane)r.z;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_xyy xyy = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_xyz xyz = alwan_xyy_to_xyz_v(xyy);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = xyz.x;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = xyz.y;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = xyz.z;
    }
#endif
    return ALWAN_OK;
}

