/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map sRGB convenience kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * sRGB -> XYZ Kernel
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__srgb_to_xyz_kernel)(
    alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
    alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
    size_t n)
{
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    for (; i + W <= n; i += W) {
        alwan_map_simd vr = ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(alwan_map_simd_load(&i0[i]));
        alwan_map_simd vg = ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(alwan_map_simd_load(&i1[i]));
        alwan_map_simd vb = ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(alwan_map_simd_load(&i2[i]));
        alwan_map_simd ox, oy, oz;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&ox, &oy, &oz, &ALWAN_MAP_LIT(SRGB_TO_XYZ), vr, vg, vb);
        alwan_map_simd_store(&o0[i], ox);
        alwan_map_simd_store(&o1[i], oy);
        alwan_map_simd_store(&o2[i], oz);
    }
#endif
    for (; i < n; i++) {
        alwan_map_lane r = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)((alwan_map_lane)i0[i]);
        alwan_map_lane g = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)((alwan_map_lane)i1[i]);
        alwan_map_lane b = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)((alwan_map_lane)i2[i]);
        ALWAN_MAP_LIT(alwan_vec3) v = {{r, g, b}};
        ALWAN_MAP_LIT(alwan_vec3) xyz = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(SRGB_TO_XYZ), v);
        o0[i] = (alwan_map_lane)xyz.v[0]; o1[i] = (alwan_map_lane)xyz.v[1]; o2[i] = (alwan_map_lane)xyz.v[2];
    }
}

/* ----------------------------------------------------------------
 * XYZ -> sRGB Kernel
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__xyz_to_srgb_kernel)(
    alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
    alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
    size_t n)
{
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    for (; i + W <= n; i += W) {
        alwan_map_simd vx = alwan_map_simd_load(&i0[i]);
        alwan_map_simd vy = alwan_map_simd_load(&i1[i]);
        alwan_map_simd vz = alwan_map_simd_load(&i2[i]);
        alwan_map_simd lr, lg, lb;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lr, &lg, &lb, &ALWAN_MAP_LIT(XYZ_TO_SRGB), vx, vy, vz);
        alwan_map_simd_store(&o0[i], ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lr));
        alwan_map_simd_store(&o1[i], ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lg));
        alwan_map_simd_store(&o2[i], ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lb));
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_vec3) v = {{(alwan_map_lane)i0[i], (alwan_map_lane)i1[i], (alwan_map_lane)i2[i]}};
        ALWAN_MAP_LIT(alwan_vec3) lin = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(XYZ_TO_SRGB), v);
        o0[i] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[0]);
        o1[i] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[1]);
        o2[i] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[2]);
    }
}

/* ----------------------------------------------------------------
 * sRGB -> Lab Kernel
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__srgb_to_lab_kernel)(
    alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
    alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
    size_t n)
{
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd inv_wx = alwan_map_simd_set1((alwan_map_lane)(1.0 / (double)ALWAN_MAP_LIT(D65_WP_Y1)[0]));
    alwan_map_simd inv_wy = alwan_map_simd_set1((alwan_map_lane)(1.0 / (double)ALWAN_MAP_LIT(D65_WP_Y1)[1]));
    alwan_map_simd inv_wz = alwan_map_simd_set1((alwan_map_lane)(1.0 / (double)ALWAN_MAP_LIT(D65_WP_Y1)[2]));
    for (; i + W <= n; i += W) {
        /* sRGB EOTF */
        alwan_map_simd vr = ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(alwan_map_simd_load(&i0[i]));
        alwan_map_simd vg = ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(alwan_map_simd_load(&i1[i]));
        alwan_map_simd vb = ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(alwan_map_simd_load(&i2[i]));
        /* sRGB -> XYZ */
        alwan_map_simd vx, vy, vz;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vx, &vy, &vz, &ALWAN_MAP_LIT(SRGB_TO_XYZ), vr, vg, vb);
        /* XYZ -> Lab */
        alwan_map_simd fx = ALWAN_MAP_NAME(alwan__lab_f_simd)(alwan_map_simd_mul(vx, inv_wx));
        alwan_map_simd fy = ALWAN_MAP_NAME(alwan__lab_f_simd)(alwan_map_simd_mul(vy, inv_wy));
        alwan_map_simd fz = ALWAN_MAP_NAME(alwan__lab_f_simd)(alwan_map_simd_mul(vz, inv_wz));
        alwan_map_simd oL = alwan_map_simd_fmsub(alwan_map_simd_set1((alwan_map_lane)116.0), fy, alwan_map_simd_set1((alwan_map_lane)16.0));
        alwan_map_simd oa = alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)500.0), alwan_map_simd_sub(fx, fy));
        alwan_map_simd ob = alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)200.0), alwan_map_simd_sub(fy, fz));
        alwan_map_simd_store(&o0[i], oL);
        alwan_map_simd_store(&o1[i], oa);
        alwan_map_simd_store(&o2[i], ob);
    }
#endif
    for (; i < n; i++) {
        alwan_map_lane r = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)((alwan_map_lane)i0[i]);
        alwan_map_lane g = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)((alwan_map_lane)i1[i]);
        alwan_map_lane b = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)((alwan_map_lane)i2[i]);
        ALWAN_MAP_LIT(alwan_vec3) v = {{r, g, b}};
        ALWAN_MAP_LIT(alwan_vec3) xyz = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(SRGB_TO_XYZ), v);
        ALWAN_MAP_LIT(alwan_xyz) wp = {ALWAN_MAP_LIT(D65_WP_Y1)[0], ALWAN_MAP_LIT(D65_WP_Y1)[1], ALWAN_MAP_LIT(D65_WP_Y1)[2]};
        ALWAN_MAP_LIT(alwan_xyz) xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
        ALWAN_MAP_LIT(alwan_lab) lab = ALWAN_MAP_LITV(alwan_xyz_to_lab)(xyz_s, wp);
        o0[i] = (alwan_map_lane)lab.L; o1[i] = (alwan_map_lane)lab.a; o2[i] = (alwan_map_lane)lab.b;
    }
}

/* ----------------------------------------------------------------
 * Lab -> sRGB Kernel
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__lab_to_srgb_kernel)(
    alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
    alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
    size_t n)
{
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd wx = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(D65_WP_Y1)[0]);
    alwan_map_simd wy = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(D65_WP_Y1)[1]);
    alwan_map_simd wz = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(D65_WP_Y1)[2]);
    for (; i + W <= n; i += W) {
        alwan_map_simd vL = alwan_map_simd_load(&i0[i]);
        alwan_map_simd va = alwan_map_simd_load(&i1[i]);
        alwan_map_simd vb = alwan_map_simd_load(&i2[i]);
        /* Lab -> XYZ */
        alwan_map_simd fy = alwan_map_simd_mul(alwan_map_simd_add(vL, alwan_map_simd_set1((alwan_map_lane)16.0)),
                                                         alwan_map_simd_set1((alwan_map_lane)(1.0 / 116.0)));
        alwan_map_simd fx = alwan_map_simd_fmadd(va, alwan_map_simd_set1((alwan_map_lane)(1.0 / 500.0)), fy);
        alwan_map_simd fz = alwan_map_simd_sub(fy, alwan_map_simd_mul(vb, alwan_map_simd_set1((alwan_map_lane)(1.0 / 200.0))));
        alwan_map_simd vx = alwan_map_simd_mul(ALWAN_MAP_NAME(alwan__lab_f_inv_simd)(fx), wx);
        alwan_map_simd vy = alwan_map_simd_mul(ALWAN_MAP_NAME(alwan__lab_f_inv_simd)(fy), wy);
        alwan_map_simd vz = alwan_map_simd_mul(ALWAN_MAP_NAME(alwan__lab_f_inv_simd)(fz), wz);
        /* XYZ -> sRGB */
        alwan_map_simd lr, lg, lb;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lr, &lg, &lb, &ALWAN_MAP_LIT(XYZ_TO_SRGB), vx, vy, vz);
        alwan_map_simd_store(&o0[i], ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lr));
        alwan_map_simd_store(&o1[i], ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lg));
        alwan_map_simd_store(&o2[i], ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lb));
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_lab) lab = {(alwan_map_lane)i0[i], (alwan_map_lane)i1[i], (alwan_map_lane)i2[i]};
        ALWAN_MAP_LIT(alwan_xyz) wp = {ALWAN_MAP_LIT(D65_WP_Y1)[0], ALWAN_MAP_LIT(D65_WP_Y1)[1], ALWAN_MAP_LIT(D65_WP_Y1)[2]};
        ALWAN_MAP_LIT(alwan_xyz) xyz = ALWAN_MAP_LITV(alwan_lab_to_xyz)(lab, wp);
        ALWAN_MAP_LIT(alwan_vec3) v = {{xyz.x, xyz.y, xyz.z}};
        ALWAN_MAP_LIT(alwan_vec3) lin = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(XYZ_TO_SRGB), v);
        o0[i] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[0]);
        o1[i] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[1]);
        o2[i] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[2]);
    }
}

/* ----------------------------------------------------------------
 * sRGB -> Oklab Kernel
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__srgb_to_oklab_kernel)(
    alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
    alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
    size_t n)
{
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    for (; i + W <= n; i += W) {
        /* sRGB EOTF */
        alwan_map_simd vr = ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(alwan_map_simd_load(&i0[i]));
        alwan_map_simd vg = ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(alwan_map_simd_load(&i1[i]));
        alwan_map_simd vb = ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(alwan_map_simd_load(&i2[i]));
        /* sRGB -> XYZ */
        alwan_map_simd vx, vy, vz;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vx, &vy, &vz, &ALWAN_MAP_LIT(SRGB_TO_XYZ), vr, vg, vb);
        /* XYZ -> LMS (OKLAB M1) */
        alwan_map_simd vl, vm, vs;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vl, &vm, &vs, &ALWAN_MAP_LIT(OKLAB_M1), vx, vy, vz);
        /* cbrt */
        vl = alwan_map_simd_cbrt(vl);
        vm = alwan_map_simd_cbrt(vm);
        vs = alwan_map_simd_cbrt(vs);
        /* LMS' -> Lab (OKLAB M2) */
        alwan_map_simd oL, oa, ob;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&oL, &oa, &ob, &ALWAN_MAP_LIT(OKLAB_M2), vl, vm, vs);
        alwan_map_simd_store(&o0[i], oL);
        alwan_map_simd_store(&o1[i], oa);
        alwan_map_simd_store(&o2[i], ob);
    }
#endif
    for (; i < n; i++) {
        alwan_map_lane r = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)((alwan_map_lane)i0[i]);
        alwan_map_lane g = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)((alwan_map_lane)i1[i]);
        alwan_map_lane b = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)((alwan_map_lane)i2[i]);
        ALWAN_MAP_LIT(alwan_vec3) v = {{r, g, b}};
        ALWAN_MAP_LIT(alwan_vec3) xyz = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(SRGB_TO_XYZ), v);
        ALWAN_MAP_LIT(alwan_xyz) xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
        ALWAN_MAP_LIT(alwan_oklab) ok = ALWAN_MAP_LITV(alwan_xyz_to_oklab)(xyz_s);
        o0[i] = (alwan_map_lane)ok.L; o1[i] = (alwan_map_lane)ok.a; o2[i] = (alwan_map_lane)ok.b;
    }
}

/* ----------------------------------------------------------------
 * Oklab -> sRGB Kernel
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__oklab_to_srgb_kernel)(
    alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
    alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
    size_t n)
{
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    for (; i + W <= n; i += W) {
        alwan_map_simd vL = alwan_map_simd_load(&i0[i]);
        alwan_map_simd va = alwan_map_simd_load(&i1[i]);
        alwan_map_simd vb = alwan_map_simd_load(&i2[i]);
        /* Lab -> LMS' (OKLAB M2_INV) */
        alwan_map_simd lp, mp, sp;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lp, &mp, &sp, &ALWAN_MAP_LIT(OKLAB_M2_INV), vL, va, vb);
        /* cube */
        alwan_map_simd vl = alwan_map_simd_mul(lp, alwan_map_simd_mul(lp, lp));
        alwan_map_simd vm = alwan_map_simd_mul(mp, alwan_map_simd_mul(mp, mp));
        alwan_map_simd vs = alwan_map_simd_mul(sp, alwan_map_simd_mul(sp, sp));
        /* LMS -> XYZ (OKLAB M1_INV) */
        alwan_map_simd vx, vy, vz;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vx, &vy, &vz, &ALWAN_MAP_LIT(OKLAB_M1_INV), vl, vm, vs);
        /* XYZ -> sRGB */
        alwan_map_simd lr, lg, lb;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lr, &lg, &lb, &ALWAN_MAP_LIT(XYZ_TO_SRGB), vx, vy, vz);
        alwan_map_simd_store(&o0[i], ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lr));
        alwan_map_simd_store(&o1[i], ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lg));
        alwan_map_simd_store(&o2[i], ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lb));
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_oklab) ok = {(alwan_map_lane)i0[i], (alwan_map_lane)i1[i], (alwan_map_lane)i2[i]};
        ALWAN_MAP_LIT(alwan_xyz) xyz = ALWAN_MAP_LITV(alwan_oklab_to_xyz)(ok);
        ALWAN_MAP_LIT(alwan_vec3) v = {{xyz.x, xyz.y, xyz.z}};
        ALWAN_MAP_LIT(alwan_vec3) lin = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(XYZ_TO_SRGB), v);
        o0[i] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[0]);
        o1[i] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[1]);
        o2[i] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[2]);
    }
}


/* ----------------------------------------------------------------
 * Interleave Wrappers
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_srgb_to_xyz, _map_interleave)(
    alwan_map_lane *xyz_out, alwan_map_lane const *rgb_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!rgb_in || !xyz_out || count == 0) return ALWAN_E_INVALID;
    if (in_stride == 3 * sizeof(alwan_map_lane) && out_stride == 3 * sizeof(alwan_map_lane)) {
        size_t j = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
        for (; j + ALWAN_MAP_SIMD_WIDTH <= count; j += ALWAN_MAP_SIMD_WIDTH) {
            alwan_map_simd vr, vg, vb;
            alwan_map_simd_deinterleave3(rgb_in + j * 3, &vr, &vg, &vb);
            alwan_map_simd ox, oy, oz;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&ox, &oy, &oz, &ALWAN_MAP_LIT(SRGB_TO_XYZ),
                ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(vr),
                ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(vg),
                ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(vb));
            alwan_map_simd_interleave3(xyz_out + j * 3, ox, oy, oz);
        }
#endif
        for (; j < count; j++) {
            alwan_map_lane r = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)(rgb_in[j*3+0]);
            alwan_map_lane g = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)(rgb_in[j*3+1]);
            alwan_map_lane b = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)(rgb_in[j*3+2]);
            ALWAN_MAP_LIT(alwan_vec3) v = {{r, g, b}};
            ALWAN_MAP_LIT(alwan_vec3) xyz = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(SRGB_TO_XYZ), v);
            xyz_out[j*3+0] = (alwan_map_lane)xyz.v[0];
            xyz_out[j*3+1] = (alwan_map_lane)xyz.v[1];
            xyz_out[j*3+2] = (alwan_map_lane)xyz.v[2];
        }
        return ALWAN_OK;
    }
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__srgb_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_xyz_to_srgb, _map_interleave)(
    alwan_map_lane *rgb_out, alwan_map_lane const *xyz_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!xyz_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    if (in_stride == 3 * sizeof(alwan_map_lane) && out_stride == 3 * sizeof(alwan_map_lane)) {
        size_t j = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
        for (; j + ALWAN_MAP_SIMD_WIDTH <= count; j += ALWAN_MAP_SIMD_WIDTH) {
            alwan_map_simd vx, vy, vz;
            alwan_map_simd_deinterleave3(xyz_in + j * 3, &vx, &vy, &vz);
            /* Inline mat3 multiply: no pointer outputs so lr/lg/lb stay in registers */
            alwan_map_simd lr = alwan_map_simd_add(alwan_map_simd_add(
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[0]), vx),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[1]), vy)),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[2]), vz));
            alwan_map_simd lg = alwan_map_simd_add(alwan_map_simd_add(
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[3]), vx),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[4]), vy)),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[5]), vz));
            alwan_map_simd lb = alwan_map_simd_add(alwan_map_simd_add(
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[6]), vx),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[7]), vy)),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[8]), vz));
            alwan_map_simd_interleave3(rgb_out + j * 3,
                ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lr),
                ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lg),
                ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lb));
        }
#endif
        for (; j < count; j++) {
            ALWAN_MAP_LIT(alwan_vec3) v = {{xyz_in[j*3+0], xyz_in[j*3+1], xyz_in[j*3+2]}};
            ALWAN_MAP_LIT(alwan_vec3) lin = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(XYZ_TO_SRGB), v);
            rgb_out[j*3+0] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[0]);
            rgb_out[j*3+1] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[1]);
            rgb_out[j*3+2] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[2]);
        }
        return ALWAN_OK;
    }
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_srgb_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_srgb_to_lab, _map_interleave)(
    alwan_map_lane *lab_out, alwan_map_lane const *rgb_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!rgb_in || !lab_out || count == 0) return ALWAN_E_INVALID;
    if (in_stride == 3 * sizeof(alwan_map_lane) && out_stride == 3 * sizeof(alwan_map_lane)) {
        size_t j = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
        for (; j + ALWAN_MAP_SIMD_WIDTH <= count; j += ALWAN_MAP_SIMD_WIDTH) {
            alwan_map_simd vr, vg, vb;
            alwan_map_simd_deinterleave3(rgb_in + j * 3, &vr, &vg, &vb);
            alwan_map_simd inv_wx = alwan_map_simd_set1((alwan_map_lane)(1.0 / (double)ALWAN_MAP_LIT(D65_WP_Y1)[0]));
            alwan_map_simd inv_wy = alwan_map_simd_set1((alwan_map_lane)(1.0 / (double)ALWAN_MAP_LIT(D65_WP_Y1)[1]));
            alwan_map_simd inv_wz = alwan_map_simd_set1((alwan_map_lane)(1.0 / (double)ALWAN_MAP_LIT(D65_WP_Y1)[2]));
            alwan_map_simd vx, vy, vz;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vx, &vy, &vz, &ALWAN_MAP_LIT(SRGB_TO_XYZ),
                ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(vr),
                ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(vg),
                ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(vb));
            alwan_map_simd fx = ALWAN_MAP_NAME(alwan__lab_f_simd)(alwan_map_simd_mul(vx, inv_wx));
            alwan_map_simd fy = ALWAN_MAP_NAME(alwan__lab_f_simd)(alwan_map_simd_mul(vy, inv_wy));
            alwan_map_simd fz = ALWAN_MAP_NAME(alwan__lab_f_simd)(alwan_map_simd_mul(vz, inv_wz));
            alwan_map_simd oL = alwan_map_simd_fmsub(alwan_map_simd_set1((alwan_map_lane)116.0), fy,
                                                       alwan_map_simd_set1((alwan_map_lane)16.0));
            alwan_map_simd oa = alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)500.0),
                                                    alwan_map_simd_sub(fx, fy));
            alwan_map_simd ob = alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)200.0),
                                                    alwan_map_simd_sub(fy, fz));
            alwan_map_simd_interleave3(lab_out + j * 3, oL, oa, ob);
        }
#endif
        for (; j < count; j++) {
            alwan_map_lane r = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)(rgb_in[j*3+0]);
            alwan_map_lane g = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)(rgb_in[j*3+1]);
            alwan_map_lane b = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)(rgb_in[j*3+2]);
            ALWAN_MAP_LIT(alwan_vec3) v = {{r, g, b}};
            ALWAN_MAP_LIT(alwan_vec3) xyz = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(SRGB_TO_XYZ), v);
            ALWAN_MAP_LIT(alwan_xyz) wp = {ALWAN_MAP_LIT(D65_WP_Y1)[0], ALWAN_MAP_LIT(D65_WP_Y1)[1], ALWAN_MAP_LIT(D65_WP_Y1)[2]};
            ALWAN_MAP_LIT(alwan_xyz) xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
            ALWAN_MAP_LIT(alwan_lab) lab = ALWAN_MAP_LITV(alwan_xyz_to_lab)(xyz_s, wp);
            lab_out[j*3+0] = (alwan_map_lane)lab.L;
            lab_out[j*3+1] = (alwan_map_lane)lab.a;
            lab_out[j*3+2] = (alwan_map_lane)lab.b;
        }
        return ALWAN_OK;
    }
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__srgb_to_lab_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(lab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_lab_to_srgb, _map_interleave)(
    alwan_map_lane *rgb_out, alwan_map_lane const *lab_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!lab_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    if (in_stride == 3 * sizeof(alwan_map_lane) && out_stride == 3 * sizeof(alwan_map_lane)) {
        size_t j = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
        for (; j + ALWAN_MAP_SIMD_WIDTH <= count; j += ALWAN_MAP_SIMD_WIDTH) {
            alwan_map_simd vL, va, vb;
            alwan_map_simd_deinterleave3(lab_in + j * 3, &vL, &va, &vb);
            alwan_map_simd wx = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(D65_WP_Y1)[0]);
            alwan_map_simd wy = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(D65_WP_Y1)[1]);
            alwan_map_simd wz = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(D65_WP_Y1)[2]);
            alwan_map_simd fy = alwan_map_simd_mul(
                alwan_map_simd_add(vL, alwan_map_simd_set1((alwan_map_lane)16.0)),
                alwan_map_simd_set1((alwan_map_lane)(1.0 / 116.0)));
            alwan_map_simd fx = alwan_map_simd_fmadd(va, alwan_map_simd_set1((alwan_map_lane)(1.0 / 500.0)), fy);
            alwan_map_simd fz = alwan_map_simd_sub(fy, alwan_map_simd_mul(vb,
                                  alwan_map_simd_set1((alwan_map_lane)(1.0 / 200.0))));
            alwan_map_simd vx = alwan_map_simd_mul(ALWAN_MAP_NAME(alwan__lab_f_inv_simd)(fx), wx);
            alwan_map_simd vy = alwan_map_simd_mul(ALWAN_MAP_NAME(alwan__lab_f_inv_simd)(fy), wy);
            alwan_map_simd vz = alwan_map_simd_mul(ALWAN_MAP_NAME(alwan__lab_f_inv_simd)(fz), wz);
            alwan_map_simd lr, lg, lb;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lr, &lg, &lb, &ALWAN_MAP_LIT(XYZ_TO_SRGB), vx, vy, vz);
            alwan_map_simd_interleave3(rgb_out + j * 3,
                ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lr),
                ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lg),
                ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lb));
        }
#endif
        for (; j < count; j++) {
            ALWAN_MAP_LIT(alwan_lab) lab = {lab_in[j*3+0], lab_in[j*3+1], lab_in[j*3+2]};
            ALWAN_MAP_LIT(alwan_xyz) wp = {ALWAN_MAP_LIT(D65_WP_Y1)[0], ALWAN_MAP_LIT(D65_WP_Y1)[1], ALWAN_MAP_LIT(D65_WP_Y1)[2]};
            ALWAN_MAP_LIT(alwan_xyz) xyz = ALWAN_MAP_LITV(alwan_lab_to_xyz)(lab, wp);
            ALWAN_MAP_LIT(alwan_vec3) v = {{xyz.x, xyz.y, xyz.z}};
            ALWAN_MAP_LIT(alwan_vec3) lin = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(XYZ_TO_SRGB), v);
            rgb_out[j*3+0] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[0]);
            rgb_out[j*3+1] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[1]);
            rgb_out[j*3+2] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[2]);
        }
        return ALWAN_OK;
    }
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, lab_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__lab_to_srgb_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_srgb_to_oklab, _map_interleave)(
    alwan_map_lane *oklab_out, alwan_map_lane const *rgb_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!rgb_in || !oklab_out || count == 0) return ALWAN_E_INVALID;
    if (in_stride == 3 * sizeof(alwan_map_lane) && out_stride == 3 * sizeof(alwan_map_lane)) {
        size_t j = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
        for (; j + ALWAN_MAP_SIMD_WIDTH <= count; j += ALWAN_MAP_SIMD_WIDTH) {
            alwan_map_simd vr, vg, vb;
            alwan_map_simd_deinterleave3(rgb_in + j * 3, &vr, &vg, &vb);
            alwan_map_simd lr = ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(vr);
            alwan_map_simd lg = ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(vg);
            alwan_map_simd lb = ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(vb);
            alwan_map_simd vx, vy, vz;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vx, &vy, &vz, &ALWAN_MAP_LIT(SRGB_TO_XYZ), lr, lg, lb);
            alwan_map_simd vl, vm, vs;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vl, &vm, &vs, &ALWAN_MAP_LIT(OKLAB_M1), vx, vy, vz);
            vl = alwan_map_simd_cbrt(vl);
            vm = alwan_map_simd_cbrt(vm);
            vs = alwan_map_simd_cbrt(vs);
            alwan_map_simd oL, oa, ob;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&oL, &oa, &ob, &ALWAN_MAP_LIT(OKLAB_M2), vl, vm, vs);
            alwan_map_simd_interleave3(oklab_out + j * 3, oL, oa, ob);
        }
#endif
        for (; j < count; j++) {
            alwan_map_lane r = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)(rgb_in[j*3+0]);
            alwan_map_lane g = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)(rgb_in[j*3+1]);
            alwan_map_lane b = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_eotf)(rgb_in[j*3+2]);
            ALWAN_MAP_LIT(alwan_vec3) v = {{r, g, b}};
            ALWAN_MAP_LIT(alwan_vec3) xyz = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(SRGB_TO_XYZ), v);
            ALWAN_MAP_LIT(alwan_xyz) xyz_s = {xyz.v[0], xyz.v[1], xyz.v[2]};
            ALWAN_MAP_LIT(alwan_oklab) ok = ALWAN_MAP_LITV(alwan_xyz_to_oklab)(xyz_s);
            oklab_out[j*3+0] = (alwan_map_lane)ok.L;
            oklab_out[j*3+1] = (alwan_map_lane)ok.a;
            oklab_out[j*3+2] = (alwan_map_lane)ok.b;
        }
        return ALWAN_OK;
    }
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__srgb_to_oklab_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(oklab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_oklab_to_srgb, _map_interleave)(
    alwan_map_lane *rgb_out, alwan_map_lane const *oklab_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!oklab_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    if (in_stride == 3 * sizeof(alwan_map_lane) && out_stride == 3 * sizeof(alwan_map_lane)) {
        size_t j = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
        for (; j + ALWAN_MAP_SIMD_WIDTH <= count; j += ALWAN_MAP_SIMD_WIDTH) {
            alwan_map_simd vL, va, vb;
            alwan_map_simd_deinterleave3(oklab_in + j * 3, &vL, &va, &vb);
            /* M2_INV: inline to avoid pointer-output stack spills */
            alwan_map_simd lp = alwan_map_simd_add(alwan_map_simd_add(
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M2_INV).m[0]), vL),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M2_INV).m[1]), va)),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M2_INV).m[2]), vb));
            alwan_map_simd mp = alwan_map_simd_add(alwan_map_simd_add(
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M2_INV).m[3]), vL),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M2_INV).m[4]), va)),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M2_INV).m[5]), vb));
            alwan_map_simd sp = alwan_map_simd_add(alwan_map_simd_add(
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M2_INV).m[6]), vL),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M2_INV).m[7]), va)),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M2_INV).m[8]), vb));
            /* cube */
            alwan_map_simd vl = alwan_map_simd_mul(lp, alwan_map_simd_mul(lp, lp));
            alwan_map_simd vm = alwan_map_simd_mul(mp, alwan_map_simd_mul(mp, mp));
            alwan_map_simd vs = alwan_map_simd_mul(sp, alwan_map_simd_mul(sp, sp));
            /* M1_INV: inline */
            alwan_map_simd vx = alwan_map_simd_add(alwan_map_simd_add(
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M1_INV).m[0]), vl),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M1_INV).m[1]), vm)),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M1_INV).m[2]), vs));
            alwan_map_simd vy = alwan_map_simd_add(alwan_map_simd_add(
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M1_INV).m[3]), vl),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M1_INV).m[4]), vm)),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M1_INV).m[5]), vs));
            alwan_map_simd vz = alwan_map_simd_add(alwan_map_simd_add(
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M1_INV).m[6]), vl),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M1_INV).m[7]), vm)),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(OKLAB_M1_INV).m[8]), vs));
            /* XYZ_TO_SRGB: inline */
            alwan_map_simd lr = alwan_map_simd_add(alwan_map_simd_add(
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[0]), vx),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[1]), vy)),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[2]), vz));
            alwan_map_simd lg = alwan_map_simd_add(alwan_map_simd_add(
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[3]), vx),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[4]), vy)),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[5]), vz));
            alwan_map_simd lb = alwan_map_simd_add(alwan_map_simd_add(
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[6]), vx),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[7]), vy)),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(XYZ_TO_SRGB).m[8]), vz));
            alwan_map_simd_interleave3(rgb_out + j * 3,
                ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lr),
                ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lg),
                ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(lb));
        }
#endif
        for (; j < count; j++) {
            ALWAN_MAP_LIT(alwan_oklab) ok = {oklab_in[j*3+0], oklab_in[j*3+1], oklab_in[j*3+2]};
            ALWAN_MAP_LIT(alwan_xyz) xyz = ALWAN_MAP_LITV(alwan_oklab_to_xyz)(ok);
            ALWAN_MAP_LIT(alwan_vec3) v = {{xyz.x, xyz.y, xyz.z}};
            ALWAN_MAP_LIT(alwan_vec3) lin = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(XYZ_TO_SRGB), v);
            rgb_out[j*3+0] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[0]);
            rgb_out[j*3+1] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[1]);
            rgb_out[j*3+2] = (alwan_map_lane)ALWAN_MAP_LIT(alwan_srgb_oetf)(lin.v[2]);
        }
        return ALWAN_OK;
    }
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, oklab_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__oklab_to_srgb_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Planar Wrappers
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_srgb_to_xyz, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__srgb_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_xyz_to_srgb, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_srgb_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_srgb_to_lab, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__srgb_to_lab_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_lab_to_srgb, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__lab_to_srgb_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_srgb_to_oklab, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__srgb_to_oklab_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_oklab_to_srgb, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__oklab_to_srgb_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}
