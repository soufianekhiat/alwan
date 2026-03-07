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
 * Map sRGB Convenience Conversions - True SIMD vectorized
 * ---------------------------------------------------------------- */

int alwan_srgb_to_xyz_map_interleave(alwan_scalar *xyz_out,
                           alwan_scalar const *rgb_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!rgb_in || !xyz_out || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);

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

        alwan__store_tile_aos3(xyz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_scalar r = alwan_srgb_eotf(in_ptr[0]);
        alwan_scalar g = alwan_srgb_eotf(in_ptr[1]);
        alwan_scalar b = alwan_srgb_eotf(in_ptr[2]);
        alwan_vec3 v = {{r, g, b}};
        alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
        out_ptr[0] = xyz.v[0]; out_ptr[1] = xyz.v[1]; out_ptr[2] = xyz.v[2];
    }
#endif

    return ALWAN_OK;
}

int alwan_xyz_to_srgb_map_interleave(alwan_scalar *rgb_out,
                           alwan_scalar const *xyz_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!xyz_in || !rgb_out || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
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

        alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_vec3 v = {{in_ptr[0], in_ptr[1], in_ptr[2]}};
        alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
        out_ptr[0] = alwan_srgb_oetf(lin.v[0]);
        out_ptr[1] = alwan_srgb_oetf(lin.v[1]);
        out_ptr[2] = alwan_srgb_oetf(lin.v[2]);
    }
#endif

    return ALWAN_OK;
}

int alwan_srgb_to_lab_map_interleave(alwan_scalar *lab_out,
                           alwan_scalar const *rgb_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!rgb_in || !lab_out || count == 0) {
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
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);

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

        alwan__store_tile_aos3(lab_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)lab_out + i * out_stride);
        alwan_scalar r = alwan_srgb_eotf(in_ptr[0]);
        alwan_scalar g = alwan_srgb_eotf(in_ptr[1]);
        alwan_scalar b = alwan_srgb_eotf(in_ptr[2]);
        alwan_vec3 v = {{r, g, b}};
        alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
        alwan_xyz wp = {D65_WP_Y1[0], D65_WP_Y1[1], D65_WP_Y1[2]};
        alwan_xyz xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
        alwan_lab lab = alwan_xyz_to_lab_v(xyz_s, wp);
        out_ptr[0] = lab.L; out_ptr[1] = lab.a; out_ptr[2] = lab.b;
    }
#endif

    return ALWAN_OK;
}

int alwan_lab_to_srgb_map_interleave(alwan_scalar *rgb_out,
                           alwan_scalar const *lab_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!lab_in || !rgb_out || count == 0) {
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
        alwan__load_tile_aos3(c0, c1, c2, lab_in, processed, in_stride, tile);

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

        alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)lab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_lab lab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz wp = {D65_WP_Y1[0], D65_WP_Y1[1], D65_WP_Y1[2]};
        alwan_xyz xyz = alwan_lab_to_xyz_v(lab, wp);
        alwan_vec3 v = {{xyz.x, xyz.y, xyz.z}};
        alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
        out_ptr[0] = alwan_srgb_oetf(lin.v[0]);
        out_ptr[1] = alwan_srgb_oetf(lin.v[1]);
        out_ptr[2] = alwan_srgb_oetf(lin.v[2]);
    }
#endif

    return ALWAN_OK;
}

int alwan_srgb_to_oklab_map_interleave(alwan_scalar *oklab_out,
                             alwan_scalar const *rgb_in,
                             size_t count,
                             size_t in_stride,
                             size_t out_stride) {
    if (!rgb_in || !oklab_out || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);

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

        alwan__store_tile_aos3(oklab_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)oklab_out + i * out_stride);
        alwan_scalar r = alwan_srgb_eotf(in_ptr[0]);
        alwan_scalar g = alwan_srgb_eotf(in_ptr[1]);
        alwan_scalar b = alwan_srgb_eotf(in_ptr[2]);
        alwan_vec3 v = {{r, g, b}};
        alwan_vec3 xyz = alwan_mat3_mulv_v(SRGB_TO_XYZ, v);
        alwan_xyz xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
        alwan_oklab ok = alwan_xyz_to_oklab_v(xyz_s);
        out_ptr[0] = ok.L; out_ptr[1] = ok.a; out_ptr[2] = ok.b;
    }
#endif

    return ALWAN_OK;
}

int alwan_oklab_to_srgb_map_interleave(alwan_scalar *rgb_out,
                             alwan_scalar const *oklab_in,
                             size_t count,
                             size_t in_stride,
                             size_t out_stride) {
    if (!oklab_in || !rgb_out || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, oklab_in, processed, in_stride, tile);

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

        alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)oklab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_oklab ok = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz xyz = alwan_oklab_to_xyz_v(ok);
        alwan_vec3 v = {{xyz.x, xyz.y, xyz.z}};
        alwan_vec3 lin = alwan_mat3_mulv_v(XYZ_TO_SRGB, v);
        out_ptr[0] = alwan_srgb_oetf(lin.v[0]);
        out_ptr[1] = alwan_srgb_oetf(lin.v[1]);
        out_ptr[2] = alwan_srgb_oetf(lin.v[2]);
    }
#endif

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
