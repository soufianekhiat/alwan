/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Color Space kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * Map XYZ <-> Lab Kernels
 * ---------------------------------------------------------------- */

static void ALWAN_MAP_LIT(alwan__xyz_to_lab_kernel_wp)(
    alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
    alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
    size_t n, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz)
{
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd inv_wx = alwan_map_simd_set1((alwan_map_lane)1.0 / (alwan_map_lane)white_xyz->x);
    alwan_map_simd inv_wy = alwan_map_simd_set1((alwan_map_lane)1.0 / (alwan_map_lane)white_xyz->y);
    alwan_map_simd inv_wz = alwan_map_simd_set1((alwan_map_lane)1.0 / (alwan_map_lane)white_xyz->z);
    for (; i + W <= n; i += W) {
        alwan_map_simd vx = alwan_map_simd_load(&i0[i]);
        alwan_map_simd vy = alwan_map_simd_load(&i1[i]);
        alwan_map_simd vz = alwan_map_simd_load(&i2[i]);
        alwan_map_simd xr = alwan_map_simd_mul(vx, inv_wx);
        alwan_map_simd yr = alwan_map_simd_mul(vy, inv_wy);
        alwan_map_simd zr = alwan_map_simd_mul(vz, inv_wz);
        alwan_map_simd fx = ALWAN_MAP_NAME(alwan__lab_f_simd)(xr);
        alwan_map_simd fy = ALWAN_MAP_NAME(alwan__lab_f_simd)(yr);
        alwan_map_simd fz = ALWAN_MAP_NAME(alwan__lab_f_simd)(zr);
        alwan_map_simd oL = alwan_map_simd_fmsub(alwan_map_simd_set1((alwan_map_lane)116.0), fy, alwan_map_simd_set1((alwan_map_lane)16.0));
        alwan_map_simd oa = alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)500.0), alwan_map_simd_sub(fx, fy));
        alwan_map_simd ob = alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)200.0), alwan_map_simd_sub(fy, fz));
        alwan_map_simd_store(&o0[i], oL);
        alwan_map_simd_store(&o1[i], oa);
        alwan_map_simd_store(&o2[i], ob);
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) v = {(alwan_map_lane)i0[i], (alwan_map_lane)i1[i], (alwan_map_lane)i2[i]};
        ALWAN_MAP_LIT(alwan_lab) r = ALWAN_MAP_LITV(alwan_xyz_to_lab)(v, *white_xyz);
        o0[i] = (alwan_map_lane)r.L; o1[i] = (alwan_map_lane)r.a; o2[i] = (alwan_map_lane)r.b;
    }
}

static void ALWAN_MAP_LIT(alwan__lab_to_xyz_kernel_wp)(
    alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
    alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
    size_t n, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz)
{
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd wx = alwan_map_simd_set1((alwan_map_lane)white_xyz->x);
    alwan_map_simd wy = alwan_map_simd_set1((alwan_map_lane)white_xyz->y);
    alwan_map_simd wz = alwan_map_simd_set1((alwan_map_lane)white_xyz->z);
    for (; i + W <= n; i += W) {
        alwan_map_simd vL = alwan_map_simd_load(&i0[i]);
        alwan_map_simd va = alwan_map_simd_load(&i1[i]);
        alwan_map_simd vb = alwan_map_simd_load(&i2[i]);
        alwan_map_simd fy = alwan_map_simd_mul(
            alwan_map_simd_add(vL, alwan_map_simd_set1((alwan_map_lane)16.0)),
            alwan_map_simd_set1((alwan_map_lane)(1.0 / 116.0)));
        alwan_map_simd fx = alwan_map_simd_fmadd(va, alwan_map_simd_set1((alwan_map_lane)(1.0 / 500.0)), fy);
        alwan_map_simd fz = alwan_map_simd_sub(fy, alwan_map_simd_mul(vb, alwan_map_simd_set1((alwan_map_lane)(1.0 / 200.0))));
        alwan_map_simd ox = alwan_map_simd_mul(wx, ALWAN_MAP_NAME(alwan__lab_f_inv_simd)(fx));
        alwan_map_simd oy = alwan_map_simd_mul(wy, ALWAN_MAP_NAME(alwan__lab_f_inv_simd)(fy));
        alwan_map_simd oz = alwan_map_simd_mul(wz, ALWAN_MAP_NAME(alwan__lab_f_inv_simd)(fz));
        alwan_map_simd_store(&o0[i], ox);
        alwan_map_simd_store(&o1[i], oy);
        alwan_map_simd_store(&o2[i], oz);
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_lab) v = {(alwan_map_lane)i0[i], (alwan_map_lane)i1[i], (alwan_map_lane)i2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_lab_to_xyz)(v, *white_xyz);
        o0[i] = (alwan_map_lane)r.x; o1[i] = (alwan_map_lane)r.y; o2[i] = (alwan_map_lane)r.z;
    }
}

/* ----------------------------------------------------------------
 * Map XYZ <-> Lab Conversions
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_xyz_to_lab, _map_interleave)(
    alwan_map_lane *lab_out, alwan_map_lane const *xyz_in,
    ALWAN_MAP_LIT(alwan_xyz) const *white_xyz,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!xyz_in || !lab_out || !white_xyz || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_lab_kernel_wp)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_P_NORM_MUL(d0, tile, 0.01);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(lab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_lab_to_xyz, _map_interleave)(
    alwan_map_lane *xyz_out, alwan_map_lane const *lab_in,
    ALWAN_MAP_LIT(alwan_xyz) const *white_xyz,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!lab_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, lab_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 100.0);
        ALWAN_MAP_LIT(alwan__lab_to_xyz_kernel_wp)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Map XYZ <-> Luv Kernels
 * ---------------------------------------------------------------- */

static void ALWAN_MAP_LIT(alwan__xyz_to_luv_kernel_wp)(
    alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
    alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
    size_t n, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz)
{
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    /* Precompute white point u'n, v'n as scalars, broadcast */
    alwan_map_lane wdn = (alwan_map_lane)white_xyz->x + (alwan_map_lane)15.0 * (alwan_map_lane)white_xyz->y + (alwan_map_lane)3.0 * (alwan_map_lane)white_xyz->z;
    alwan_map_lane upn = (wdn > (alwan_map_lane)ALWAN_MAP_DIV_GUARD) ? (alwan_map_lane)4.0 * (alwan_map_lane)white_xyz->x / wdn : (alwan_map_lane)0.0;
    alwan_map_lane vpn = (wdn > (alwan_map_lane)ALWAN_MAP_DIV_GUARD) ? (alwan_map_lane)9.0 * (alwan_map_lane)white_xyz->y / wdn : (alwan_map_lane)0.0;
    alwan_map_simd v_upn = alwan_map_simd_set1(upn);
    alwan_map_simd v_vpn = alwan_map_simd_set1(vpn);
    alwan_map_simd v_inv_wy = alwan_map_simd_set1((alwan_map_lane)1.0 / (alwan_map_lane)white_xyz->y);
    alwan_map_simd v_eps = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_DIV_GUARD);
    alwan_map_simd v_zero = alwan_map_simd_zero();
    for (; i + W <= n; i += W) {
        alwan_map_simd vx = alwan_map_simd_load(&i0[i]);
        alwan_map_simd vy = alwan_map_simd_load(&i1[i]);
        alwan_map_simd vz = alwan_map_simd_load(&i2[i]);

        /* denom = x + 15*y + 3*z */
        alwan_map_simd denom = alwan_map_simd_fmadd(alwan_map_simd_set1((alwan_map_lane)15.0), vy,
            alwan_map_simd_fmadd(alwan_map_simd_set1((alwan_map_lane)3.0), vz, vx));
        alwan_map_simd_mask small = alwan_map_simd_cmplt(denom, v_eps);
        /* safe_denom: replace near-zero with 1 to avoid division issues */
        alwan_map_simd safe_denom = alwan_map_simd_select(small, alwan_map_simd_set1((alwan_map_lane)1.0), denom);
        alwan_map_simd up = alwan_map_simd_select(small, v_zero,
            alwan_map_simd_div(alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)4.0), vx), safe_denom));
        alwan_map_simd vp = alwan_map_simd_select(small, v_zero,
            alwan_map_simd_div(alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)9.0), vy), safe_denom));

        /* L = 116*lab_f(Y/Yn) - 16 */
        alwan_map_simd yr = alwan_map_simd_mul(vy, v_inv_wy);
        alwan_map_simd oL = alwan_map_simd_fmsub(alwan_map_simd_set1((alwan_map_lane)116.0), ALWAN_MAP_NAME(alwan__lab_f_simd)(yr), alwan_map_simd_set1((alwan_map_lane)16.0));

        /* u = 13*L*(u' - u'n), v = 13*L*(v' - v'n) */
        alwan_map_simd L13 = alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)13.0), oL);
        alwan_map_simd ou = alwan_map_simd_mul(L13, alwan_map_simd_sub(up, v_upn));
        alwan_map_simd ov = alwan_map_simd_mul(L13, alwan_map_simd_sub(vp, v_vpn));

        alwan_map_simd_store(&o0[i], oL);
        alwan_map_simd_store(&o1[i], ou);
        alwan_map_simd_store(&o2[i], ov);
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) v = {(alwan_map_lane)i0[i], (alwan_map_lane)i1[i], (alwan_map_lane)i2[i]};
        ALWAN_MAP_LIT(alwan_luv) r = ALWAN_MAP_LITV(alwan_xyz_to_luv)(v, *white_xyz);
        o0[i] = (alwan_map_lane)r.L; o1[i] = (alwan_map_lane)r.u; o2[i] = (alwan_map_lane)r.v;
    }
}

static void ALWAN_MAP_LIT(alwan__luv_to_xyz_kernel_wp)(
    alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
    alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
    size_t n, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz)
{
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_lane wdn = (alwan_map_lane)white_xyz->x + (alwan_map_lane)15.0 * (alwan_map_lane)white_xyz->y + (alwan_map_lane)3.0 * (alwan_map_lane)white_xyz->z;
    alwan_map_lane upn = (wdn > (alwan_map_lane)ALWAN_MAP_DIV_GUARD) ? (alwan_map_lane)4.0 * (alwan_map_lane)white_xyz->x / wdn : (alwan_map_lane)0.0;
    alwan_map_lane vpn = (wdn > (alwan_map_lane)ALWAN_MAP_DIV_GUARD) ? (alwan_map_lane)9.0 * (alwan_map_lane)white_xyz->y / wdn : (alwan_map_lane)0.0;
    alwan_map_simd v_upn = alwan_map_simd_set1(upn);
    alwan_map_simd v_vpn = alwan_map_simd_set1(vpn);
    alwan_map_simd v_wy  = alwan_map_simd_set1((alwan_map_lane)white_xyz->y);
    alwan_map_simd v_eps = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_DIV_GUARD);
    alwan_map_simd v_zero = alwan_map_simd_zero();
    for (; i + W <= n; i += W) {
        alwan_map_simd vL = alwan_map_simd_load(&i0[i]);
        alwan_map_simd vu = alwan_map_simd_load(&i1[i]);
        alwan_map_simd vv = alwan_map_simd_load(&i2[i]);

        /* fy = (L+16)/116, Y = wy * f_inv(fy) */
        alwan_map_simd fy = alwan_map_simd_mul(
            alwan_map_simd_add(vL, alwan_map_simd_set1((alwan_map_lane)16.0)),
            alwan_map_simd_set1((alwan_map_lane)(1.0 / 116.0)));
        alwan_map_simd oY = alwan_map_simd_mul(v_wy, ALWAN_MAP_NAME(alwan__lab_f_inv_simd)(fy));

        /* u' = u/(13*L) + u'n, v' = v/(13*L) + v'n */
        alwan_map_simd L13 = alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)13.0), vL);
        alwan_map_simd abs_L = alwan_map_simd_abs(vL);
        alwan_map_simd_mask L_small = alwan_map_simd_cmplt(abs_L, v_eps);
        alwan_map_simd safe_L13 = alwan_map_simd_select(L_small, alwan_map_simd_set1((alwan_map_lane)1.0), L13);
        alwan_map_simd up = alwan_map_simd_select(L_small, v_zero,
            alwan_map_simd_add(alwan_map_simd_div(vu, safe_L13), v_upn));
        alwan_map_simd vp = alwan_map_simd_select(L_small, v_zero,
            alwan_map_simd_add(alwan_map_simd_div(vv, safe_L13), v_vpn));

        /* X = Y * 9*u' / (4*v'), Z = Y * (12 - 3*u' - 20*v') / (4*v') */
        alwan_map_simd abs_vp = alwan_map_simd_abs(vp);
        alwan_map_simd_mask vp_small = alwan_map_simd_cmplt(abs_vp, v_eps);
        alwan_map_simd safe_4vp = alwan_map_simd_select(vp_small, alwan_map_simd_set1((alwan_map_lane)1.0),
            alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)4.0), vp));
        alwan_map_simd oX = alwan_map_simd_select(vp_small, v_zero,
            alwan_map_simd_div(alwan_map_simd_mul(oY, alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)9.0), up)), safe_4vp));
        /* 12 - 3*u' - 20*v' */
        alwan_map_simd znum = alwan_map_simd_sub(
            alwan_map_simd_sub(alwan_map_simd_set1((alwan_map_lane)12.0),
                alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)3.0), up)),
            alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)20.0), vp));
        alwan_map_simd oZ = alwan_map_simd_select(vp_small, v_zero,
            alwan_map_simd_div(alwan_map_simd_mul(oY, znum), safe_4vp));

        alwan_map_simd_store(&o0[i], oX);
        alwan_map_simd_store(&o1[i], oY);
        alwan_map_simd_store(&o2[i], oZ);
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_luv) v = {(alwan_map_lane)i0[i], (alwan_map_lane)i1[i], (alwan_map_lane)i2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_luv_to_xyz)(v, *white_xyz);
        o0[i] = (alwan_map_lane)r.x; o1[i] = (alwan_map_lane)r.y; o2[i] = (alwan_map_lane)r.z;
    }
}

/* ----------------------------------------------------------------
 * Map XYZ <-> Luv Conversions
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_xyz_to_luv, _map_interleave)(
    alwan_map_lane *luv_out, alwan_map_lane const *xyz_in,
    ALWAN_MAP_LIT(alwan_xyz) const *white_xyz,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!xyz_in || !luv_out || !white_xyz || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_luv_kernel_wp)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_P_NORM_MUL(d0, tile, 0.01);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(luv_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_luv_to_xyz, _map_interleave)(
    alwan_map_lane *xyz_out, alwan_map_lane const *luv_in,
    ALWAN_MAP_LIT(alwan_xyz) const *white_xyz,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!luv_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, luv_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 100.0);
        ALWAN_MAP_LIT(alwan__luv_to_xyz_kernel_wp)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Map Lab <-> LCh Conversions
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_lab_to_lch, _map_interleave)(
    alwan_map_lane *lch_out, alwan_map_lane const *lab_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!lab_in || !lch_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, lab_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 100.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd va = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd vC = alwan_map_simd_sqrt(
                alwan_map_simd_fmadd(va, va, alwan_map_simd_mul(vb, vb)));
            alwan_map_simd vh = alwan_map_simd_atan2(vb, va);
            /* rad -> deg, wrap negative to [0, 360) */
            alwan_map_simd const rad2deg = alwan_map_simd_set1((alwan_map_lane)(180.0 / 3.14159265358979323846));
            alwan_map_simd const three60 = alwan_map_simd_set1((alwan_map_lane)360.0);
            vh = alwan_map_simd_mul(vh, rad2deg);
            vh = alwan_map_simd_select(alwan_map_simd_cmpge(vh, alwan_map_simd_zero()),
                                   vh, alwan_map_simd_add(vh, three60));
            alwan_map_simd_store(&c0[i], vL);
            alwan_map_simd_store(&c1[i], vC);
            alwan_map_simd_store(&c2[i], vh);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_lab) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_lch) r = ALWAN_MAP_LITV(alwan_lab_to_lch)(v);
            c0[i] = (alwan_map_lane)r.L; c1[i] = (alwan_map_lane)r.C; c2[i] = (alwan_map_lane)r.h;
        }

        ALWAN_MAP_P_NORM_MUL(c0, tile, 0.01);
        ALWAN_MAP_P_NORM_MUL(c2, tile, 1.0/360.0);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(lch_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)lab_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)lch_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_lab) lab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_lch) lch = ALWAN_MAP_LITV(alwan_lab_to_lch)(lab);
        out_ptr[0] = lch.L; out_ptr[1] = lch.C; out_ptr[2] = lch.h;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_lch_to_lab, _map_interleave)(
    alwan_map_lane *lab_out, alwan_map_lane const *lch_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!lch_in || !lab_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, lch_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 100.0);
        ALWAN_MAP_P_NORM_MUL(c2, tile, 360.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vC = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vh = alwan_map_simd_load(&c2[i]);
            /* deg -> rad */
            alwan_map_simd const deg2rad = alwan_map_simd_set1((alwan_map_lane)(3.14159265358979323846 / 180.0));
            alwan_map_simd vh_rad = alwan_map_simd_mul(vh, deg2rad);
            alwan_map_simd va = alwan_map_simd_mul(vC, alwan_map_simd_cos(vh_rad));
            alwan_map_simd vb = alwan_map_simd_mul(vC, alwan_map_simd_sin(vh_rad));
            alwan_map_simd_store(&c0[i], vL);
            alwan_map_simd_store(&c1[i], va);
            alwan_map_simd_store(&c2[i], vb);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_lch) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_lab) r = ALWAN_MAP_LITV(alwan_lch_to_lab)(v);
            c0[i] = (alwan_map_lane)r.L; c1[i] = (alwan_map_lane)r.a; c2[i] = (alwan_map_lane)r.b;
        }

        ALWAN_MAP_P_NORM_MUL(c0, tile, 0.01);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(lab_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)lch_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)lab_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_lch) lch = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_lab) lab = ALWAN_MAP_LITV(alwan_lch_to_lab)(lch);
        out_ptr[0] = lab.L; out_ptr[1] = lab.a; out_ptr[2] = lab.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Map Luv <-> LCh(uv) Conversions
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_luv_to_lchuv, _map_interleave)(
    alwan_map_lane *lchuv_out, alwan_map_lane const *luv_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!luv_in || !lchuv_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, luv_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 100.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vu = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vv = alwan_map_simd_load(&c2[i]);
            alwan_map_simd vC = alwan_map_simd_sqrt(
                alwan_map_simd_fmadd(vu, vu, alwan_map_simd_mul(vv, vv)));
            alwan_map_simd vh = alwan_map_simd_atan2(vv, vu);
            /* rad -> deg, wrap negative to [0, 360) */
            alwan_map_simd const rad2deg = alwan_map_simd_set1((alwan_map_lane)(180.0 / 3.14159265358979323846));
            alwan_map_simd const three60 = alwan_map_simd_set1((alwan_map_lane)360.0);
            vh = alwan_map_simd_mul(vh, rad2deg);
            vh = alwan_map_simd_select(alwan_map_simd_cmpge(vh, alwan_map_simd_zero()),
                                   vh, alwan_map_simd_add(vh, three60));
            alwan_map_simd_store(&c0[i], vL);
            alwan_map_simd_store(&c1[i], vC);
            alwan_map_simd_store(&c2[i], vh);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_luv) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_lchuv) r = ALWAN_MAP_LITV(alwan_luv_to_lchuv)(v);
            c0[i] = (alwan_map_lane)r.L; c1[i] = (alwan_map_lane)r.C; c2[i] = (alwan_map_lane)r.h;
        }

        ALWAN_MAP_P_NORM_MUL(c0, tile, 0.01);
        ALWAN_MAP_P_NORM_MUL(c2, tile, 1.0/360.0);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(lchuv_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)luv_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)lchuv_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_luv) luv = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_lchuv) lchuv = ALWAN_MAP_LITV(alwan_luv_to_lchuv)(luv);
        out_ptr[0] = lchuv.L; out_ptr[1] = lchuv.C; out_ptr[2] = lchuv.h;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_lchuv_to_luv, _map_interleave)(
    alwan_map_lane *luv_out, alwan_map_lane const *lchuv_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!lchuv_in || !luv_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, lchuv_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 100.0);
        ALWAN_MAP_P_NORM_MUL(c2, tile, 360.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vC = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vh = alwan_map_simd_load(&c2[i]);
            /* deg -> rad */
            alwan_map_simd const deg2rad = alwan_map_simd_set1((alwan_map_lane)(3.14159265358979323846 / 180.0));
            alwan_map_simd vh_rad = alwan_map_simd_mul(vh, deg2rad);
            alwan_map_simd vu = alwan_map_simd_mul(vC, alwan_map_simd_cos(vh_rad));
            alwan_map_simd vv = alwan_map_simd_mul(vC, alwan_map_simd_sin(vh_rad));
            alwan_map_simd_store(&c0[i], vL);
            alwan_map_simd_store(&c1[i], vu);
            alwan_map_simd_store(&c2[i], vv);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_lchuv) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_luv) r = ALWAN_MAP_LITV(alwan_lchuv_to_luv)(v);
            c0[i] = (alwan_map_lane)r.L; c1[i] = (alwan_map_lane)r.u; c2[i] = (alwan_map_lane)r.v;
        }

        ALWAN_MAP_P_NORM_MUL(c0, tile, 0.01);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(luv_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)lchuv_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)luv_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_lchuv) lchuv = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_luv) luv = ALWAN_MAP_LITV(alwan_lchuv_to_luv)(lchuv);
        out_ptr[0] = luv.L; out_ptr[1] = luv.u; out_ptr[2] = luv.v;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Map XYZ <-> xyY Conversions
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_xyz_to_xyy, _map_interleave)(
    alwan_map_lane *xyy_out, alwan_map_lane const *xyz_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!xyz_in || !xyy_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd v_eps = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_DIV_GUARD);
    alwan_map_simd v_zero = alwan_map_simd_zero();
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            alwan_map_simd sum = alwan_map_simd_add(vx, alwan_map_simd_add(vy, vz));
            alwan_map_simd_mask small = alwan_map_simd_cmplt(sum, v_eps);
            alwan_map_simd safe_sum = alwan_map_simd_select(small, alwan_map_simd_set1((alwan_map_lane)1.0), sum);
            alwan_map_simd ox = alwan_map_simd_select(small, v_zero, alwan_map_simd_div(vx, safe_sum));
            alwan_map_simd oy = alwan_map_simd_select(small, v_zero, alwan_map_simd_div(vy, safe_sum));
            alwan_map_simd oY = alwan_map_simd_select(small, v_zero, vy);
            alwan_map_simd_store(&c0[i], ox);
            alwan_map_simd_store(&c1[i], oy);
            alwan_map_simd_store(&c2[i], oY);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_xyz) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_xyy) r = ALWAN_MAP_LITV(alwan_xyz_to_xyy)(v);
            c0[i] = (alwan_map_lane)r.x; c1[i] = (alwan_map_lane)r.y; c2[i] = (alwan_map_lane)r.Y;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyy_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)xyz_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)xyy_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_xyz) xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_xyy) xyy = ALWAN_MAP_LITV(alwan_xyz_to_xyy)(xyz);
        out_ptr[0] = xyy.x; out_ptr[1] = xyy.y; out_ptr[2] = xyy.Y;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_xyy_to_xyz, _map_interleave)(
    alwan_map_lane *xyz_out, alwan_map_lane const *xyy_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!xyy_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd v_eps = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_DIV_GUARD);
    alwan_map_simd v_zero = alwan_map_simd_zero();
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyy_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vY = alwan_map_simd_load(&c2[i]);
            alwan_map_simd_mask small = alwan_map_simd_cmplt(vy, v_eps);
            alwan_map_simd safe_y = alwan_map_simd_select(small, alwan_map_simd_set1((alwan_map_lane)1.0), vy);
            alwan_map_simd oX = alwan_map_simd_select(small, v_zero,
                alwan_map_simd_div(alwan_map_simd_mul(vx, vY), safe_y));
            alwan_map_simd oYout = alwan_map_simd_select(small, v_zero, vY);
            /* z_chrom = 1 - x - y; Z = z_chrom * Y / y */
            alwan_map_simd z_chrom = alwan_map_simd_sub(
                alwan_map_simd_sub(alwan_map_simd_set1((alwan_map_lane)1.0), vx), vy);
            alwan_map_simd oZ = alwan_map_simd_select(small, v_zero,
                alwan_map_simd_div(alwan_map_simd_mul(z_chrom, vY), safe_y));
            alwan_map_simd_store(&c0[i], oX);
            alwan_map_simd_store(&c1[i], oYout);
            alwan_map_simd_store(&c2[i], oZ);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_xyy) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_xyy_to_xyz)(v);
            c0[i] = (alwan_map_lane)r.x; c1[i] = (alwan_map_lane)r.y; c2[i] = (alwan_map_lane)r.z;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)xyy_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)xyz_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_xyy) xyy = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_xyz) xyz = ALWAN_MAP_LITV(alwan_xyy_to_xyz)(xyy);
        out_ptr[0] = xyz.x; out_ptr[1] = xyz.y; out_ptr[2] = xyz.z;
    }
#endif
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

int ALWAN_MAP_LITCAT(alwan_xyz_to_lab, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    ALWAN_MAP_LIT(alwan_xyz) const *white_xyz,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || !white_xyz || count == 0)
        return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_lab_kernel_wp)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_P_NORM_MUL(d0, tile, 0.01);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_lab_to_xyz, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    ALWAN_MAP_LIT(alwan_xyz) const *white_xyz,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || !white_xyz || count == 0)
        return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 100.0);
        ALWAN_MAP_LIT(alwan__lab_to_xyz_kernel_wp)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_xyz_to_luv, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    ALWAN_MAP_LIT(alwan_xyz) const *white_xyz,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || !white_xyz || count == 0)
        return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_luv_kernel_wp)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_P_NORM_MUL(d0, tile, 0.01);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_luv_to_xyz, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    ALWAN_MAP_LIT(alwan_xyz) const *white_xyz,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || !white_xyz || count == 0)
        return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 100.0);
        ALWAN_MAP_LIT(alwan__luv_to_xyz_kernel_wp)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_lab_to_lch, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0)
        return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 100.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd va = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd vC = alwan_map_simd_sqrt(
                alwan_map_simd_fmadd(va, va, alwan_map_simd_mul(vb, vb)));
            alwan_map_simd vh = alwan_map_simd_atan2(vb, va);
            /* rad -> deg, wrap negative to [0, 360) */
            alwan_map_simd const rad2deg = alwan_map_simd_set1((alwan_map_lane)(180.0 / 3.14159265358979323846));
            alwan_map_simd const three60 = alwan_map_simd_set1((alwan_map_lane)360.0);
            vh = alwan_map_simd_mul(vh, rad2deg);
            vh = alwan_map_simd_select(alwan_map_simd_cmpge(vh, alwan_map_simd_zero()),
                                   vh, alwan_map_simd_add(vh, three60));
            alwan_map_simd_store(&c0[i], vL);
            alwan_map_simd_store(&c1[i], vC);
            alwan_map_simd_store(&c2[i], vh);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_lab) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_lch) r = ALWAN_MAP_LITV(alwan_lab_to_lch)(v);
            c0[i] = (alwan_map_lane)r.L; c1[i] = (alwan_map_lane)r.C; c2[i] = (alwan_map_lane)r.h;
        }

        ALWAN_MAP_P_NORM_MUL(c0, tile, 0.01);
        ALWAN_MAP_P_NORM_MUL(c2, tile, 1.0/360.0);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_lab) lab = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_lch) lch = ALWAN_MAP_LITV(alwan_lab_to_lch)(lab);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = lch.L;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = lch.C;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = lch.h;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_lch_to_lab, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0)
        return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 100.0);
        ALWAN_MAP_P_NORM_MUL(c2, tile, 360.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vC = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vh = alwan_map_simd_load(&c2[i]);
            /* deg -> rad */
            alwan_map_simd const deg2rad = alwan_map_simd_set1((alwan_map_lane)(3.14159265358979323846 / 180.0));
            alwan_map_simd vh_rad = alwan_map_simd_mul(vh, deg2rad);
            alwan_map_simd va = alwan_map_simd_mul(vC, alwan_map_simd_cos(vh_rad));
            alwan_map_simd vb = alwan_map_simd_mul(vC, alwan_map_simd_sin(vh_rad));
            alwan_map_simd_store(&c0[i], vL);
            alwan_map_simd_store(&c1[i], va);
            alwan_map_simd_store(&c2[i], vb);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_lch) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_lab) r = ALWAN_MAP_LITV(alwan_lch_to_lab)(v);
            c0[i] = (alwan_map_lane)r.L; c1[i] = (alwan_map_lane)r.a; c2[i] = (alwan_map_lane)r.b;
        }

        ALWAN_MAP_P_NORM_MUL(c0, tile, 0.01);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_lch) lch = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_lab) lab = ALWAN_MAP_LITV(alwan_lch_to_lab)(lch);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = lab.L;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = lab.a;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = lab.b;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_luv_to_lchuv, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0)
        return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 100.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vu = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vv = alwan_map_simd_load(&c2[i]);
            alwan_map_simd vC = alwan_map_simd_sqrt(
                alwan_map_simd_fmadd(vu, vu, alwan_map_simd_mul(vv, vv)));
            alwan_map_simd vh = alwan_map_simd_atan2(vv, vu);
            /* rad -> deg, wrap negative to [0, 360) */
            alwan_map_simd const rad2deg = alwan_map_simd_set1((alwan_map_lane)(180.0 / 3.14159265358979323846));
            alwan_map_simd const three60 = alwan_map_simd_set1((alwan_map_lane)360.0);
            vh = alwan_map_simd_mul(vh, rad2deg);
            vh = alwan_map_simd_select(alwan_map_simd_cmpge(vh, alwan_map_simd_zero()),
                                   vh, alwan_map_simd_add(vh, three60));
            alwan_map_simd_store(&c0[i], vL);
            alwan_map_simd_store(&c1[i], vC);
            alwan_map_simd_store(&c2[i], vh);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_luv) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_lchuv) r = ALWAN_MAP_LITV(alwan_luv_to_lchuv)(v);
            c0[i] = (alwan_map_lane)r.L; c1[i] = (alwan_map_lane)r.C; c2[i] = (alwan_map_lane)r.h;
        }

        ALWAN_MAP_P_NORM_MUL(c0, tile, 0.01);
        ALWAN_MAP_P_NORM_MUL(c2, tile, 1.0/360.0);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_luv) luv = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_lchuv) lchuv = ALWAN_MAP_LITV(alwan_luv_to_lchuv)(luv);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = lchuv.L;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = lchuv.C;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = lchuv.h;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_lchuv_to_luv, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0)
        return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 100.0);
        ALWAN_MAP_P_NORM_MUL(c2, tile, 360.0);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vC = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vh = alwan_map_simd_load(&c2[i]);
            /* deg -> rad */
            alwan_map_simd const deg2rad = alwan_map_simd_set1((alwan_map_lane)(3.14159265358979323846 / 180.0));
            alwan_map_simd vh_rad = alwan_map_simd_mul(vh, deg2rad);
            alwan_map_simd vu = alwan_map_simd_mul(vC, alwan_map_simd_cos(vh_rad));
            alwan_map_simd vv = alwan_map_simd_mul(vC, alwan_map_simd_sin(vh_rad));
            alwan_map_simd_store(&c0[i], vL);
            alwan_map_simd_store(&c1[i], vu);
            alwan_map_simd_store(&c2[i], vv);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_lchuv) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_luv) r = ALWAN_MAP_LITV(alwan_lchuv_to_luv)(v);
            c0[i] = (alwan_map_lane)r.L; c1[i] = (alwan_map_lane)r.u; c2[i] = (alwan_map_lane)r.v;
        }

        ALWAN_MAP_P_NORM_MUL(c0, tile, 0.01);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_lchuv) lchuv = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_luv) luv = ALWAN_MAP_LITV(alwan_lchuv_to_luv)(lchuv);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = luv.L;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = luv.u;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = luv.v;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_xyz_to_xyy, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0)
        return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd v_eps = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_DIV_GUARD);
    alwan_map_simd v_zero = alwan_map_simd_zero();
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            alwan_map_simd sum = alwan_map_simd_add(vx, alwan_map_simd_add(vy, vz));
            alwan_map_simd_mask small = alwan_map_simd_cmplt(sum, v_eps);
            alwan_map_simd safe_sum = alwan_map_simd_select(small, alwan_map_simd_set1((alwan_map_lane)1.0), sum);
            alwan_map_simd ox = alwan_map_simd_select(small, v_zero, alwan_map_simd_div(vx, safe_sum));
            alwan_map_simd oy = alwan_map_simd_select(small, v_zero, alwan_map_simd_div(vy, safe_sum));
            alwan_map_simd oY = alwan_map_simd_select(small, v_zero, vy);
            alwan_map_simd_store(&c0[i], ox);
            alwan_map_simd_store(&c1[i], oy);
            alwan_map_simd_store(&c2[i], oY);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_xyz) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_xyy) r = ALWAN_MAP_LITV(alwan_xyz_to_xyy)(v);
            c0[i] = (alwan_map_lane)r.x; c1[i] = (alwan_map_lane)r.y; c2[i] = (alwan_map_lane)r.Y;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_xyy) xyy = ALWAN_MAP_LITV(alwan_xyz_to_xyy)(xyz);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = xyy.x;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = xyy.y;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = xyy.Y;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_xyy_to_xyz, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0)
        return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd v_eps = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_DIV_GUARD);
    alwan_map_simd v_zero = alwan_map_simd_zero();
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vY = alwan_map_simd_load(&c2[i]);
            alwan_map_simd_mask small = alwan_map_simd_cmplt(vy, v_eps);
            alwan_map_simd safe_y = alwan_map_simd_select(small, alwan_map_simd_set1((alwan_map_lane)1.0), vy);
            alwan_map_simd oX = alwan_map_simd_select(small, v_zero,
                alwan_map_simd_div(alwan_map_simd_mul(vx, vY), safe_y));
            alwan_map_simd oYout = alwan_map_simd_select(small, v_zero, vY);
            /* z_chrom = 1 - x - y; Z = z_chrom * Y / y */
            alwan_map_simd z_chrom = alwan_map_simd_sub(
                alwan_map_simd_sub(alwan_map_simd_set1((alwan_map_lane)1.0), vx), vy);
            alwan_map_simd oZ = alwan_map_simd_select(small, v_zero,
                alwan_map_simd_div(alwan_map_simd_mul(z_chrom, vY), safe_y));
            alwan_map_simd_store(&c0[i], oX);
            alwan_map_simd_store(&c1[i], oYout);
            alwan_map_simd_store(&c2[i], oZ);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_xyy) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_xyy_to_xyz)(v);
            c0[i] = (alwan_map_lane)r.x; c1[i] = (alwan_map_lane)r.y; c2[i] = (alwan_map_lane)r.z;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_xyy) xyy = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_xyz) xyz = ALWAN_MAP_LITV(alwan_xyy_to_xyz)(xyy);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = xyz.x;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = xyz.y;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = xyz.z;
    }
#endif
    return ALWAN_OK;
}
