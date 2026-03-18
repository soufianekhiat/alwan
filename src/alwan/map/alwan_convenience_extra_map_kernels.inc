/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Convenience Extra kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * RGB <-> CMY
 * ---------------------------------------------------------------- */

static void ALWAN_MAP_LIT(alwan__rgb_to_cmy_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                      alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd_store(&d0[i], alwan_map_simd_sub(one, alwan_map_simd_load(&c0[i])));
            alwan_map_simd_store(&d1[i], alwan_map_simd_sub(one, alwan_map_simd_load(&c1[i])));
            alwan_map_simd_store(&d2[i], alwan_map_simd_sub(one, alwan_map_simd_load(&c2[i])));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_cmy) r = ALWAN_MAP_LITV(alwan_rgb_to_cmy)(v);
        d0[i] = (alwan_map_lane)r.c; d1[i] = (alwan_map_lane)r.m; d2[i] = (alwan_map_lane)r.y;
    }
}

static void ALWAN_MAP_LIT(alwan__cmy_to_rgb_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                      alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd_store(&d0[i], alwan_map_simd_sub(one, alwan_map_simd_load(&c0[i])));
            alwan_map_simd_store(&d1[i], alwan_map_simd_sub(one, alwan_map_simd_load(&c1[i])));
            alwan_map_simd_store(&d2[i], alwan_map_simd_sub(one, alwan_map_simd_load(&c2[i])));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_cmy) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_cmy_to_rgb)(v);
        d0[i] = (alwan_map_lane)r.r; d1[i] = (alwan_map_lane)r.g; d2[i] = (alwan_map_lane)r.b;
    }
}

int ALWAN_MAP_LITCAT(alwan_rgb_to_cmy, _map_interleave)(alwan_map_lane *cmy_out, alwan_map_lane const *rgb_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !cmy_out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED(rgb_in, in_stride, cmy_out, out_stride, count, ALWAN_MAP_LIT(alwan__rgb_to_cmy_kernel));
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_cmy_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, alwan_map_lane const *cmy_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!cmy_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED(cmy_in, in_stride, rgb_out, out_stride, count, ALWAN_MAP_LIT(alwan__cmy_to_rgb_kernel));
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YCoCg
 * ---------------------------------------------------------------- */

static void ALWAN_MAP_LIT(alwan__rgb_to_ycocg_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                        alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd q25 = alwan_map_simd_set1((alwan_map_lane)(0.25));
        alwan_map_simd q50 = alwan_map_simd_set1((alwan_map_lane)(0.5));
        alwan_map_simd nq25 = alwan_map_simd_set1((alwan_map_lane)(-0.25));
        for (; i + W <= n; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(q25, vr, alwan_map_simd_fmadd(q50, vg, alwan_map_simd_mul(q25, vb))));
            alwan_map_simd_store(&d1[i], alwan_map_simd_mul(q50, alwan_map_simd_sub(vr, vb)));
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(nq25, vr, alwan_map_simd_fmadd(q50, vg, alwan_map_simd_mul(nq25, vb))));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_rgb) rgb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_ycocg) ycocg = ALWAN_MAP_LITV(alwan_rgb_to_ycocg)(rgb);
        d0[i] = (alwan_map_lane)ycocg.Y; d1[i] = (alwan_map_lane)ycocg.Co; d2[i] = (alwan_map_lane)ycocg.Cg;
    }
}

static void ALWAN_MAP_LIT(alwan__ycocg_to_rgb_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                        alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        for (; i + W <= n; i += W) {
            alwan_map_simd vy  = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vco = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vcg = alwan_map_simd_load(&c2[i]);
            alwan_map_simd tmp = alwan_map_simd_sub(vy, vcg);
            alwan_map_simd_store(&d0[i], alwan_map_simd_add(tmp, vco));
            alwan_map_simd_store(&d1[i], alwan_map_simd_add(vy, vcg));
            alwan_map_simd_store(&d2[i], alwan_map_simd_sub(tmp, vco));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_ycocg) ycocg = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_rgb) rgb = ALWAN_MAP_LITV(alwan_ycocg_to_rgb)(ycocg);
        d0[i] = (alwan_map_lane)rgb.r; d1[i] = (alwan_map_lane)rgb.g; d2[i] = (alwan_map_lane)rgb.b;
    }
}

int ALWAN_MAP_LITCAT(alwan_rgb_to_ycocg, _map_interleave)(alwan_map_lane *ycocg_out, alwan_map_lane const *rgb_in,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !ycocg_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__rgb_to_ycocg_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_P_NORM_ADD(d1, tile, 0.5);
        ALWAN_MAP_P_NORM_ADD(d2, tile, 0.5);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(ycocg_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_ycocg_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, alwan_map_lane const *ycocg_in,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!ycocg_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, ycocg_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_ADD(c1, tile, -0.5);
        ALWAN_MAP_P_NORM_ADD(c2, tile, -0.5);
        ALWAN_MAP_LIT(alwan__ycocg_to_rgb_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HWB, HSV <-> HWB
 * ---------------------------------------------------------------- */

static void ALWAN_MAP_LIT(alwan__rgb_to_hwb_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                      alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        /* RGB->HSV->HWB: compose both SIMD computations inline */
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd zero  = alwan_map_simd_set1((alwan_map_lane)(0.0));
        alwan_map_simd one   = alwan_map_simd_set1((alwan_map_lane)(1.0));
        alwan_map_simd sixty = alwan_map_simd_set1((alwan_map_lane)(60.0));
        alwan_map_simd i360  = alwan_map_simd_set1((alwan_map_lane)(1.0 / 360.0));
        alwan_map_simd v360  = alwan_map_simd_set1((alwan_map_lane)(360.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            /* HSV computation */
            alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
            alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);
            alwan_map_simd delta = alwan_map_simd_sub(mx, mn);
            alwan_map_simd safe_d = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), delta, one);
            alwan_map_simd inv_d = alwan_map_simd_div(one, safe_d);
            alwan_map_simd h_r = alwan_map_simd_mul(sixty, alwan_map_simd_mul(alwan_map_simd_sub(vg, vb), inv_d));
            h_r = alwan_map_simd_select(alwan_map_simd_cmplt(vg, vb), alwan_map_simd_add(h_r, v360), h_r);
            alwan_map_simd h_g = alwan_map_simd_mul(sixty, alwan_map_simd_fmadd(alwan_map_simd_sub(vb, vr), inv_d, alwan_map_simd_set1((alwan_map_lane)(2.0))));
            alwan_map_simd h_b = alwan_map_simd_mul(sixty, alwan_map_simd_fmadd(alwan_map_simd_sub(vr, vg), inv_d, alwan_map_simd_set1((alwan_map_lane)(4.0))));
            alwan_map_simd oh = alwan_map_simd_select(alwan_map_simd_cmpeq(mx, vr), h_r,
                               alwan_map_simd_select(alwan_map_simd_cmpeq(mx, vg), h_g, h_b));
            oh = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), alwan_map_simd_mul(oh, i360), zero);
            /* HWB: W = (1-S)*V, B = 1-V.  S = delta/mx */
            alwan_map_simd safe_mx = alwan_map_simd_select(alwan_map_simd_cmpgt(mx, zero), mx, one);
            alwan_map_simd os = alwan_map_simd_select(alwan_map_simd_cmpgt(mx, zero), alwan_map_simd_div(delta, safe_mx), zero);
            alwan_map_simd ow = alwan_map_simd_mul(alwan_map_simd_sub(one, os), mx);
            alwan_map_simd ob = alwan_map_simd_sub(one, mx);
            alwan_map_simd_store(&d0[i], oh);
            alwan_map_simd_store(&d1[i], ow);
            alwan_map_simd_store(&d2[i], ob);
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_rgb) rgb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_hwb) hwb = ALWAN_MAP_LITV(alwan_rgb_to_hwb)(rgb);
        d0[i] = (alwan_map_lane)hwb.h; d1[i] = (alwan_map_lane)hwb.w; d2[i] = (alwan_map_lane)hwb.b;
    }
}

static void ALWAN_MAP_LIT(alwan__hwb_to_rgb_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                      alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        /* HWB->HSV->RGB: compose both SIMD computations inline */
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
        alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
        alwan_map_simd eps  = alwan_map_simd_set1((alwan_map_lane)(1e-10));
        alwan_map_simd v360 = alwan_map_simd_set1((alwan_map_lane)(360.0));
        alwan_map_simd v60  = alwan_map_simd_set1((alwan_map_lane)(60.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vw = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vbk = alwan_map_simd_load(&c2[i]);
            /* HWB->HSV */
            alwan_map_simd vv = alwan_map_simd_sub(one, vbk);
            alwan_map_simd wb_sum = alwan_map_simd_add(vw, vbk);
            alwan_map_simd safe_v = alwan_map_simd_select(alwan_map_simd_cmplt(vv, eps), eps, vv);
            alwan_map_simd_mask gray = alwan_map_simd_cmpge(wb_sum, one);
            alwan_map_simd vs = alwan_map_simd_select(gray, zero, alwan_map_simd_sub(one, alwan_map_simd_div(vw, safe_v)));
            vv = alwan_map_simd_select(gray, alwan_map_simd_div(vw, wb_sum), vv);
            /* HSV->RGB: 6-sector via nested select */
            alwan_map_simd h_deg = alwan_map_simd_mul(vh, v360);
            h_deg = alwan_map_simd_sub(h_deg, alwan_map_simd_mul(alwan_map_simd_floor(alwan_map_simd_div(h_deg, v360)), v360));
            h_deg = alwan_map_simd_select(alwan_map_simd_cmplt(h_deg, zero), alwan_map_simd_add(h_deg, v360), h_deg);
            alwan_map_simd sector = alwan_map_simd_div(h_deg, v60);
            alwan_map_simd sf = alwan_map_simd_floor(sector);
            alwan_map_simd f = alwan_map_simd_sub(sector, sf);
            alwan_map_simd p = alwan_map_simd_mul(vv, alwan_map_simd_sub(one, vs));
            alwan_map_simd q = alwan_map_simd_mul(vv, alwan_map_simd_sub(one, alwan_map_simd_mul(vs, f)));
            alwan_map_simd t = alwan_map_simd_mul(vv, alwan_map_simd_sub(one, alwan_map_simd_mul(vs, alwan_map_simd_sub(one, f))));
            alwan_map_simd r01 = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(1.0))), vv, q);
            alwan_map_simd g01 = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(1.0))), t, vv);
            alwan_map_simd r23 = p;
            alwan_map_simd g23 = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(3.0))), vv, q);
            alwan_map_simd b23 = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(3.0))), t, vv);
            alwan_map_simd r45 = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(5.0))), t, vv);
            alwan_map_simd b45 = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(5.0))), vv, q);
            alwan_map_simd r0123 = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(2.0))), r01, r23);
            alwan_map_simd g0123 = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(2.0))), g01, g23);
            alwan_map_simd b0123 = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(2.0))), p, b23);
            alwan_map_simd or = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(4.0))), r0123, r45);
            alwan_map_simd og = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(4.0))), g0123, p);
            alwan_map_simd ob = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(4.0))), b0123, b45);
            or = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vv, or);
            og = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vv, og);
            ob = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vv, ob);
            alwan_map_simd_store(&d0[i], or);
            alwan_map_simd_store(&d1[i], og);
            alwan_map_simd_store(&d2[i], ob);
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_hwb) hwb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_rgb) rgb = ALWAN_MAP_LITV(alwan_hwb_to_rgb)(hwb);
        d0[i] = (alwan_map_lane)rgb.r; d1[i] = (alwan_map_lane)rgb.g; d2[i] = (alwan_map_lane)rgb.b;
    }
}

static void ALWAN_MAP_LIT(alwan__hsv_to_hwb_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                      alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vs = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vv = alwan_map_simd_load(&c2[i]);
            alwan_map_simd_store(&d0[i], vh);
            alwan_map_simd_store(&d1[i], alwan_map_simd_mul(alwan_map_simd_sub(one, vs), vv));
            alwan_map_simd_store(&d2[i], alwan_map_simd_sub(one, vv));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_hsv) hsv = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_hwb) hwb = ALWAN_MAP_LITV(alwan_hsv_to_hwb)(hsv);
        d0[i] = (alwan_map_lane)hwb.h; d1[i] = (alwan_map_lane)hwb.w; d2[i] = (alwan_map_lane)hwb.b;
    }
}

static void ALWAN_MAP_LIT(alwan__hwb_to_hsv_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                      alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
        alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
        alwan_map_simd eps  = alwan_map_simd_set1((alwan_map_lane)(1e-10));
        for (; i + W <= n; i += W) {
            alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vw = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd vv = alwan_map_simd_sub(one, vb);
            alwan_map_simd wb_sum = alwan_map_simd_add(vw, vb);
            alwan_map_simd safe_v = alwan_map_simd_select(alwan_map_simd_cmplt(vv, eps), eps, vv);
            alwan_map_simd_mask gray = alwan_map_simd_cmpge(wb_sum, one);
            alwan_map_simd vs = alwan_map_simd_select(gray, zero,
                               alwan_map_simd_sub(one, alwan_map_simd_div(vw, safe_v)));
            vv = alwan_map_simd_select(gray, alwan_map_simd_div(vw, wb_sum), vv);
            alwan_map_simd_store(&d0[i], vh);
            alwan_map_simd_store(&d1[i], vs);
            alwan_map_simd_store(&d2[i], vv);
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_hwb) hwb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_hsv) hsv = ALWAN_MAP_LITV(alwan_hwb_to_hsv)(hwb);
        d0[i] = (alwan_map_lane)hsv.h; d1[i] = (alwan_map_lane)hsv.s; d2[i] = (alwan_map_lane)hsv.v;
    }
}

int ALWAN_MAP_LITCAT(alwan_rgb_to_hwb, _map_interleave)(alwan_map_lane *hwb_out, alwan_map_lane const *rgb_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !hwb_out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED(rgb_in, in_stride, hwb_out, out_stride, count, ALWAN_MAP_LIT(alwan__rgb_to_hwb_kernel));
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hwb_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, alwan_map_lane const *hwb_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!hwb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED(hwb_in, in_stride, rgb_out, out_stride, count, ALWAN_MAP_LIT(alwan__hwb_to_rgb_kernel));
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hsv_to_hwb, _map_interleave)(alwan_map_lane *hwb_out, alwan_map_lane const *hsv_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!hsv_in || !hwb_out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED(hsv_in, in_stride, hwb_out, out_stride, count, ALWAN_MAP_LIT(alwan__hsv_to_hwb_kernel));
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hwb_to_hsv, _map_interleave)(alwan_map_lane *hsv_out, alwan_map_lane const *hwb_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!hwb_in || !hsv_out || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED(hwb_in, in_stride, hsv_out, out_stride, count, ALWAN_MAP_LIT(alwan__hwb_to_hsv_kernel));
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YCbCr (with standard enum)
 * ---------------------------------------------------------------- */

static void ALWAN_MAP_LIT(alwan__rgb_to_ycbcr_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                        alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                        size_t n, alwan_map_lane kr, alwan_map_lane kb) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd vkr = alwan_map_simd_set1((alwan_map_lane)kr);
        alwan_map_simd vkb = alwan_map_simd_set1((alwan_map_lane)kb);
        alwan_map_simd vkg = alwan_map_simd_set1((alwan_map_lane)(1.0 - (alwan_map_lane)kr - (alwan_map_lane)kb));
        alwan_map_simd half = alwan_map_simd_set1((alwan_map_lane)(0.5));
        alwan_map_simd inv_2_1mkb = alwan_map_simd_set1((alwan_map_lane)(1.0 / (2.0 * (1.0 - (alwan_map_lane)kb))));
        alwan_map_simd inv_2_1mkr = alwan_map_simd_set1((alwan_map_lane)(1.0 / (2.0 * (1.0 - (alwan_map_lane)kr))));
        for (; i + W <= n; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd vy = alwan_map_simd_fmadd(vkr, vr, alwan_map_simd_fmadd(vkg, vg, alwan_map_simd_mul(vkb, vb)));
            alwan_map_simd_store(&d0[i], vy);
            alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(alwan_map_simd_sub(vb, vy), inv_2_1mkb, half));
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(alwan_map_simd_sub(vr, vy), inv_2_1mkr, half));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_rgb) rgb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_ycbcr) ycbcr = ALWAN_MAP_LITV(alwan_rgb_to_ycbcr_kr_kb)(rgb, (alwan_map_lane)kr, (alwan_map_lane)kb);
        d0[i] = (alwan_map_lane)ycbcr.Y; d1[i] = (alwan_map_lane)ycbcr.Cb; d2[i] = (alwan_map_lane)ycbcr.Cr;
    }
}

static void ALWAN_MAP_LIT(alwan__ycbcr_to_rgb_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                        alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                        size_t n, alwan_map_lane kr, alwan_map_lane kb) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd vkr = alwan_map_simd_set1((alwan_map_lane)kr);
        alwan_map_simd vkb = alwan_map_simd_set1((alwan_map_lane)kb);
        alwan_map_simd vkg_inv = alwan_map_simd_set1((alwan_map_lane)(1.0 / (1.0 - (alwan_map_lane)kr - (alwan_map_lane)kb)));
        alwan_map_simd scale_cr = alwan_map_simd_set1((alwan_map_lane)(2.0 * (1.0 - (alwan_map_lane)kr)));
        alwan_map_simd scale_cb = alwan_map_simd_set1((alwan_map_lane)(2.0 * (1.0 - (alwan_map_lane)kb)));
        alwan_map_simd vhalf = alwan_map_simd_set1((alwan_map_lane)(0.5));
        alwan_map_simd vzero = alwan_map_simd_set1((alwan_map_lane)(0.0));
        alwan_map_simd vone  = alwan_map_simd_set1((alwan_map_lane)(1.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vy  = alwan_map_simd_load(&c0[i]);
            /* Subtract 0.5 to match alwan_ycbcr_to_rgb_kr_kb_v which expects [0,1] Cb/Cr */
            alwan_map_simd vcb = alwan_map_simd_sub(alwan_map_simd_load(&c1[i]), vhalf);
            alwan_map_simd vcr = alwan_map_simd_sub(alwan_map_simd_load(&c2[i]), vhalf);
            alwan_map_simd vr = alwan_map_simd_fmadd(vcr, scale_cr, vy);
            alwan_map_simd vb = alwan_map_simd_fmadd(vcb, scale_cb, vy);
            alwan_map_simd vg = alwan_map_simd_mul(alwan_map_simd_sub(vy, alwan_map_simd_fmadd(vkr, vr, alwan_map_simd_mul(vkb, vb))), vkg_inv);
            alwan_map_simd_store(&d0[i], alwan_map_simd_clamp(vr, vzero, vone));
            alwan_map_simd_store(&d1[i], alwan_map_simd_clamp(vg, vzero, vone));
            alwan_map_simd_store(&d2[i], alwan_map_simd_clamp(vb, vzero, vone));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_ycbcr) ycbcr = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_rgb) rgb = ALWAN_MAP_LITV(alwan_ycbcr_to_rgb_kr_kb)(ycbcr, (alwan_map_lane)kr, (alwan_map_lane)kb);
        d0[i] = (alwan_map_lane)rgb.r; d1[i] = (alwan_map_lane)rgb.g; d2[i] = (alwan_map_lane)rgb.b;
    }
}

int ALWAN_MAP_LITCAT(alwan_rgb_to_ycbcr, _map_interleave)(alwan_map_lane *ycbcr_out, alwan_map_lane const *rgb_in,
                            alwan_ycbcr_standard standard,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !ycbcr_out || count == 0) return ALWAN_E_INVALID;
    alwan_scalar kr_s, kb_s;
    alwan__get_ycbcr_coeffs(standard, &kr_s, &kb_s);
    alwan_map_lane kr = (alwan_map_lane)kr_s, kb = (alwan_map_lane)kb_s;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__rgb_to_ycbcr_kernel)(d0, d1, d2, c0, c1, c2, tile, kr, kb);
        ALWAN_MAP_P_NORM_ADD(d1, tile, 0.5);
        ALWAN_MAP_P_NORM_ADD(d2, tile, 0.5);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(ycbcr_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_ycbcr_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, alwan_map_lane const *ycbcr_in,
                            alwan_ycbcr_standard standard,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!ycbcr_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    alwan_scalar kr_s, kb_s;
    alwan__get_ycbcr_coeffs(standard, &kr_s, &kb_s);
    alwan_map_lane kr = (alwan_map_lane)kr_s, kb = (alwan_map_lane)kb_s;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, ycbcr_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_ADD(c1, tile, -0.5);
        ALWAN_MAP_P_NORM_ADD(c2, tile, -0.5);
        ALWAN_MAP_LIT(alwan__ycbcr_to_rgb_kernel)(d0, d1, d2, c0, c1, c2, tile, kr, kb);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YcCbcCrc (with bit_depth)
 * ---------------------------------------------------------------- */

static void ALWAN_MAP_LIT(alwan__rgb_to_yccbccrc_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                           alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                           size_t n, int bit_depth) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const kr_s = (alwan_map_lane)ALWAN_LUMA_KR_BT2020;
        alwan_map_lane const kg_s = (alwan_map_lane)ALWAN_LUMA_KG_BT2020;
        alwan_map_lane const kb_s = (alwan_map_lane)ALWAN_LUMA_KB_BT2020;
        alwan_map_simd vkr = alwan_map_simd_set1((alwan_map_lane)kr_s);
        alwan_map_simd vkg = alwan_map_simd_set1((alwan_map_lane)kg_s);
        alwan_map_simd vkb = alwan_map_simd_set1((alwan_map_lane)kb_s);
        alwan_map_simd v_beta = alwan_map_simd_set1((alwan_map_lane)(0.018));
        alwan_map_simd v_alpha = alwan_map_simd_set1((alwan_map_lane)(1.099));
        alwan_map_simd v_am1 = alwan_map_simd_set1((alwan_map_lane)(0.099));
        alwan_map_simd v_45 = alwan_map_simd_set1((alwan_map_lane)(4.5));
        alwan_map_simd v_exp = alwan_map_simd_set1((alwan_map_lane)(0.45));
        alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0));
        alwan_map_simd v_inv1 = alwan_map_simd_set1((alwan_map_lane)(1.0/1.9404)), v_inv2 = alwan_map_simd_set1((alwan_map_lane)(1.0/1.5816));
        alwan_map_simd v_inv3 = alwan_map_simd_set1((alwan_map_lane)(1.0/1.7184)), v_inv4 = alwan_map_simd_set1((alwan_map_lane)(1.0/0.9936));
        ALWAN_MAP_LIT(alwan_legal_range) lr = ALWAN_MAP_LIT(alwan_legal_range_from_bit_depth)(bit_depth);
        alwan_map_simd v_ys = alwan_map_simd_set1((alwan_map_lane)(lr.y_max - lr.y_min));
        alwan_map_simd v_yo = alwan_map_simd_set1((alwan_map_lane)lr.y_min);
        alwan_map_simd v_cs = alwan_map_simd_set1((alwan_map_lane)(lr.c_max - lr.c_min));
        alwan_map_simd v_cc = alwan_map_simd_set1((alwan_map_lane)((lr.c_max + lr.c_min) / 2.0));
        /* BT.2020 OETF macro */
        #define BT2020_OETF(x) alwan_map_simd_select(alwan_map_simd_cmplt(x, v_beta), \
            alwan_map_simd_mul(v_45, x), \
            alwan_map_simd_sub(alwan_map_simd_mul(v_alpha, alwan_map_simd_pow(x, v_exp)), v_am1))
        for (; i + W <= n; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd yc_lin = alwan_map_simd_fmadd(vkr, vr, alwan_map_simd_fmadd(vkg, vg, alwan_map_simd_mul(vkb, vb)));
            alwan_map_simd yc = BT2020_OETF(yc_lin);
            alwan_map_simd rg = BT2020_OETF(vr);
            alwan_map_simd bg = BT2020_OETF(vb);
            alwan_map_simd diff_b = alwan_map_simd_sub(bg, yc);
            alwan_map_simd diff_r = alwan_map_simd_sub(rg, yc);
            alwan_map_simd cbc = alwan_map_simd_select(alwan_map_simd_cmple(diff_b, zero),
                alwan_map_simd_mul(diff_b, v_inv1), alwan_map_simd_mul(diff_b, v_inv2));
            alwan_map_simd crc = alwan_map_simd_select(alwan_map_simd_cmple(diff_r, zero),
                alwan_map_simd_mul(diff_r, v_inv3), alwan_map_simd_mul(diff_r, v_inv4));
            alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(yc, v_ys, v_yo));
            alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(cbc, v_cs, v_cc));
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(crc, v_cs, v_cc));
        }
        #undef BT2020_OETF
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_rgb) rgb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_yccbccrc) ycc = ALWAN_MAP_LITV(alwan_rgb_to_yccbccrc)(rgb, bit_depth);
        d0[i] = (alwan_map_lane)ycc.Yc; d1[i] = (alwan_map_lane)ycc.Cbc; d2[i] = (alwan_map_lane)ycc.Crc;
    }
}

static void ALWAN_MAP_LIT(alwan__yccbccrc_to_rgb_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                           alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                           size_t n, int bit_depth) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const kr_s = (alwan_map_lane)ALWAN_LUMA_KR_BT2020;
        alwan_map_lane const kg_s = (alwan_map_lane)ALWAN_LUMA_KG_BT2020;
        alwan_map_lane const kb_s = (alwan_map_lane)ALWAN_LUMA_KB_BT2020;
        alwan_map_simd vkr = alwan_map_simd_set1((alwan_map_lane)kr_s);
        alwan_map_simd vkb = alwan_map_simd_set1((alwan_map_lane)kb_s);
        alwan_map_simd inv_kg = alwan_map_simd_set1((alwan_map_lane)(1.0 / kg_s));
        ALWAN_MAP_LIT(alwan_legal_range) lr = ALWAN_MAP_LIT(alwan_legal_range_from_bit_depth)(bit_depth);
        alwan_map_simd v_y_inv = alwan_map_simd_set1((alwan_map_lane)(1.0 / (lr.y_max - lr.y_min)));
        alwan_map_simd v_yo = alwan_map_simd_set1((alwan_map_lane)lr.y_min);
        alwan_map_simd v_c_inv = alwan_map_simd_set1((alwan_map_lane)(1.0 / (lr.c_max - lr.c_min)));
        alwan_map_simd v_cc = alwan_map_simd_set1((alwan_map_lane)((lr.c_max + lr.c_min) / 2.0));
        alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0));
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1));
        alwan_map_simd v_am1 = alwan_map_simd_set1((alwan_map_lane)(0.099));
        alwan_map_simd v_inv_alpha = alwan_map_simd_set1((alwan_map_lane)(1.0 / 1.099));
        alwan_map_simd v_inv45 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 4.5));
        alwan_map_simd v_threshold = alwan_map_simd_set1((alwan_map_lane)(4.5 * 0.018));
        alwan_map_simd v_inv_exp = alwan_map_simd_set1((alwan_map_lane)(1.0 / 0.45));
        alwan_map_simd v_19404 = alwan_map_simd_set1((alwan_map_lane)(1.9404)), v_15816 = alwan_map_simd_set1((alwan_map_lane)(1.5816));
        alwan_map_simd v_17184 = alwan_map_simd_set1((alwan_map_lane)(1.7184)), v_09936 = alwan_map_simd_set1((alwan_map_lane)(0.9936));
        /* BT.2020 EOTF macro */
        #define BT2020_EOTF(x) alwan_map_simd_select(alwan_map_simd_cmplt(x, v_threshold), \
            alwan_map_simd_mul(x, v_inv45), \
            alwan_map_simd_pow(alwan_map_simd_mul(alwan_map_simd_add(x, v_am1), v_inv_alpha), v_inv_exp))
        for (; i + W <= n; i += W) {
            alwan_map_simd vyc_lr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vcbc_lr = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vcrc_lr = alwan_map_simd_load(&c2[i]);
            /* Reverse legal range */
            alwan_map_simd yc = alwan_map_simd_mul(alwan_map_simd_sub(vyc_lr, v_yo), v_y_inv);
            alwan_map_simd cbc = alwan_map_simd_mul(alwan_map_simd_sub(vcbc_lr, v_cc), v_c_inv);
            alwan_map_simd crc = alwan_map_simd_mul(alwan_map_simd_sub(vcrc_lr, v_cc), v_c_inv);
            /* Reverse chroma divisors */
            alwan_map_simd diff_b = alwan_map_simd_select(alwan_map_simd_cmple(cbc, zero),
                alwan_map_simd_mul(cbc, v_19404), alwan_map_simd_mul(cbc, v_15816));
            alwan_map_simd diff_r = alwan_map_simd_select(alwan_map_simd_cmple(crc, zero),
                alwan_map_simd_mul(crc, v_17184), alwan_map_simd_mul(crc, v_09936));
            alwan_map_simd rg = alwan_map_simd_add(yc, diff_r);
            alwan_map_simd bg = alwan_map_simd_add(yc, diff_b);
            /* BT.2020 EOTF */
            alwan_map_simd yc_lin = BT2020_EOTF(yc);
            alwan_map_simd vr = BT2020_EOTF(rg);
            alwan_map_simd vb = BT2020_EOTF(bg);
            alwan_map_simd vg = alwan_map_simd_mul(alwan_map_simd_sub(yc_lin, alwan_map_simd_fmadd(vkr, vr, alwan_map_simd_mul(vkb, vb))), inv_kg);
            alwan_map_simd_store(&d0[i], alwan_map_simd_clamp(vr, zero, one));
            alwan_map_simd_store(&d1[i], alwan_map_simd_clamp(vg, zero, one));
            alwan_map_simd_store(&d2[i], alwan_map_simd_clamp(vb, zero, one));
        }
        #undef BT2020_EOTF
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_yccbccrc) ycc = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_rgb) rgb = ALWAN_MAP_LITV(alwan_yccbccrc_to_rgb)(ycc, bit_depth);
        d0[i] = (alwan_map_lane)rgb.r; d1[i] = (alwan_map_lane)rgb.g; d2[i] = (alwan_map_lane)rgb.b;
    }
}

int ALWAN_MAP_LITCAT(alwan_rgb_to_yccbccrc, _map_interleave)(alwan_map_lane *yccbccrc_out, alwan_map_lane const *rgb_in,
                               int bit_depth,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !yccbccrc_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__rgb_to_yccbccrc_kernel)(d0, d1, d2, c0, c1, c2, tile, bit_depth);
        ALWAN_MAP_P_NORM_ADD(d1, tile, 0.5);
        ALWAN_MAP_P_NORM_ADD(d2, tile, 0.5);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(yccbccrc_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_yccbccrc_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, alwan_map_lane const *yccbccrc_in,
                               int bit_depth,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!yccbccrc_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, yccbccrc_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_ADD(c1, tile, -0.5);
        ALWAN_MAP_P_NORM_ADD(c2, tile, -0.5);
        ALWAN_MAP_LIT(alwan__yccbccrc_to_rgb_kernel)(d0, d1, d2, c0, c1, c2, tile, bit_depth);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * YCbCr legal <-> full range (with bit_depth)
 * ---------------------------------------------------------------- */

static void ALWAN_MAP_LIT(alwan__ycbcr_full_to_legal_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                               alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                               size_t n, int bit_depth) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        ALWAN_MAP_LIT(alwan_legal_range) lr = ALWAN_MAP_LIT(alwan_legal_range_from_bit_depth)(bit_depth);
        alwan_map_simd y_scale  = alwan_map_simd_set1((alwan_map_lane)(lr.y_max - lr.y_min));
        alwan_map_simd y_off    = alwan_map_simd_set1((alwan_map_lane)lr.y_min);
        alwan_map_simd c_scale  = alwan_map_simd_set1((alwan_map_lane)(lr.c_max - lr.c_min));
        alwan_map_simd c_center = alwan_map_simd_set1((alwan_map_lane)((lr.c_max + lr.c_min) / 2.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(alwan_map_simd_load(&c0[i]), y_scale, y_off));
            alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(alwan_map_simd_load(&c1[i]), c_scale, c_center));
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(alwan_map_simd_load(&c2[i]), c_scale, c_center));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_ycbcr) ycbcr = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_ycbcr) result = ALWAN_MAP_LITV(alwan_ycbcr_full_to_legal)(ycbcr, bit_depth);
        d0[i] = (alwan_map_lane)result.Y; d1[i] = (alwan_map_lane)result.Cb; d2[i] = (alwan_map_lane)result.Cr;
    }
}

static void ALWAN_MAP_LIT(alwan__ycbcr_legal_to_full_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                               alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                               size_t n, int bit_depth) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        ALWAN_MAP_LIT(alwan_legal_range) lr = ALWAN_MAP_LIT(alwan_legal_range_from_bit_depth)(bit_depth);
        alwan_map_simd y_inv  = alwan_map_simd_set1((alwan_map_lane)(1.0 / (alwan_map_lane)(lr.y_max - lr.y_min)));
        alwan_map_simd y_off  = alwan_map_simd_set1((alwan_map_lane)lr.y_min);
        alwan_map_simd c_inv  = alwan_map_simd_set1((alwan_map_lane)(1.0 / (alwan_map_lane)(lr.c_max - lr.c_min)));
        alwan_map_simd c_center = alwan_map_simd_set1((alwan_map_lane)((lr.c_max + lr.c_min) / 2.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd_store(&d0[i], alwan_map_simd_mul(alwan_map_simd_sub(alwan_map_simd_load(&c0[i]), y_off), y_inv));
            alwan_map_simd_store(&d1[i], alwan_map_simd_mul(alwan_map_simd_sub(alwan_map_simd_load(&c1[i]), c_center), c_inv));
            alwan_map_simd_store(&d2[i], alwan_map_simd_mul(alwan_map_simd_sub(alwan_map_simd_load(&c2[i]), c_center), c_inv));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_ycbcr) ycbcr = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_ycbcr) result = ALWAN_MAP_LITV(alwan_ycbcr_legal_to_full)(ycbcr, bit_depth);
        d0[i] = (alwan_map_lane)result.Y; d1[i] = (alwan_map_lane)result.Cb; d2[i] = (alwan_map_lane)result.Cr;
    }
}

int ALWAN_MAP_LITCAT(alwan_ycbcr_full_to_legal, _map_interleave)(alwan_map_lane *out, alwan_map_lane const *in,
                                   int bit_depth,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__ycbcr_full_to_legal_kernel)(c0, c1, c2, c0, c1, c2, tile, bit_depth);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_ycbcr_legal_to_full, _map_interleave)(alwan_map_lane *out, alwan_map_lane const *in,
                                   int bit_depth,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__ycbcr_legal_to_full_kernel)(c0, c1, c2, c0, c1, c2, tile, bit_depth);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CMY <-> CMYK (4-channel, hand-written)
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_cmy_to_cmyk, _map_interleave)(alwan_map_lane *cmyk_out,
                           alwan_map_lane const *cmy_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!cmy_in || !cmyk_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS], d3[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, cmy_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1));
                alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0));
                for (; i + W <= tile; i += W) {
                    alwan_map_simd vc = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd vm = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd vy = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd vk = ALWAN_MAP_NAME(alwan__simd_min3)(vc, vm, vy);
                    alwan_map_simd denom = alwan_map_simd_sub(one, vk);
                    alwan_map_simd safe_d = alwan_map_simd_select(alwan_map_simd_cmpgt(denom, zero), denom, one);
                    alwan_map_simd inv_d = alwan_map_simd_div(one, safe_d);
                    alwan_map_simd_mask full_k = alwan_map_simd_cmpge(vk, one);
                    alwan_map_simd_store(&d0[i], alwan_map_simd_select(full_k, zero, alwan_map_simd_mul(alwan_map_simd_sub(vc, vk), inv_d)));
                    alwan_map_simd_store(&d1[i], alwan_map_simd_select(full_k, zero, alwan_map_simd_mul(alwan_map_simd_sub(vm, vk), inv_d)));
                    alwan_map_simd_store(&d2[i], alwan_map_simd_select(full_k, zero, alwan_map_simd_mul(alwan_map_simd_sub(vy, vk), inv_d)));
                    alwan_map_simd_store(&d3[i], vk);
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_cmy) cmy = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_cmyk) cmyk = ALWAN_MAP_LITV(alwan_cmy_to_cmyk)(cmy);
                d0[i] = (alwan_map_lane)cmyk.c; d1[i] = (alwan_map_lane)cmyk.m;
                d2[i] = (alwan_map_lane)cmyk.y; d3[i] = (alwan_map_lane)cmyk.k;
            }
        }
        /* Store 4-channel output */
        for (size_t i = 0; i < tile; i++) {
            alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)cmyk_out + (processed + i) * out_stride);
            out_ptr[0] = (alwan_map_lane)d0[i]; out_ptr[1] = (alwan_map_lane)d1[i];
            out_ptr[2] = (alwan_map_lane)d2[i]; out_ptr[3] = (alwan_map_lane)d3[i];
        }
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_cmyk_to_cmy, _map_interleave)(alwan_map_lane *cmy_out,
                           alwan_map_lane const *cmyk_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!cmyk_in || !cmy_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS], c3[ALWAN_MAP_TILE_PIXELS];
        /* Load 4-channel input */
        for (size_t i = 0; i < tile; i++) {
            alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)cmyk_in + (processed + i) * in_stride);
            c0[i] = (alwan_map_lane)in_ptr[0]; c1[i] = (alwan_map_lane)in_ptr[1];
            c2[i] = (alwan_map_lane)in_ptr[2]; c3[i] = (alwan_map_lane)in_ptr[3];
        }
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                for (; i + W <= tile; i += W) {
                    alwan_map_simd vc = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd vm = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd vy = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd vk = alwan_map_simd_load(&c3[i]);
                    alwan_map_simd omk = alwan_map_simd_sub(alwan_map_simd_set1((alwan_map_lane)(1)), vk);
                    alwan_map_simd_store(&c0[i], alwan_map_simd_fmadd(vc, omk, vk));
                    alwan_map_simd_store(&c1[i], alwan_map_simd_fmadd(vm, omk, vk));
                    alwan_map_simd_store(&c2[i], alwan_map_simd_fmadd(vy, omk, vk));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_cmyk) cmyk = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i], (alwan_map_lane)c3[i]};
                ALWAN_MAP_LIT(alwan_cmy) cmy = ALWAN_MAP_LITV(alwan_cmyk_to_cmy)(cmyk);
                c0[i] = (alwan_map_lane)cmy.c; c1[i] = (alwan_map_lane)cmy.m; c2[i] = (alwan_map_lane)cmy.y;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(cmy_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants (SIMD tiled)
 * ================================================================ */

/* RGB <-> CMY */
int ALWAN_MAP_LITCAT(alwan_rgb_to_cmy, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                 alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED_PLANAR(in0, in1, in2, in_stride, out0, out1, out2, out_stride, count, ALWAN_MAP_LIT(alwan__rgb_to_cmy_kernel));
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_cmy_to_rgb, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                 alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED_PLANAR(in0, in1, in2, in_stride, out0, out1, out2, out_stride, count, ALWAN_MAP_LIT(alwan__cmy_to_rgb_kernel));
    return ALWAN_OK;
}

/* RGB <-> YCoCg */
int ALWAN_MAP_LITCAT(alwan_rgb_to_ycocg, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                   alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED_PLANAR(in0, in1, in2, in_stride, out0, out1, out2, out_stride, count, ALWAN_MAP_LIT(alwan__rgb_to_ycocg_kernel));
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_ycocg_to_rgb, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                   alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED_PLANAR(in0, in1, in2, in_stride, out0, out1, out2, out_stride, count, ALWAN_MAP_LIT(alwan__ycocg_to_rgb_kernel));
    return ALWAN_OK;
}

/* RGB <-> HWB, HSV <-> HWB */
int ALWAN_MAP_LITCAT(alwan_rgb_to_hwb, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                 alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED_PLANAR(in0, in1, in2, in_stride, out0, out1, out2, out_stride, count, ALWAN_MAP_LIT(alwan__rgb_to_hwb_kernel));
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hwb_to_rgb, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                 alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED_PLANAR(in0, in1, in2, in_stride, out0, out1, out2, out_stride, count, ALWAN_MAP_LIT(alwan__hwb_to_rgb_kernel));
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hsv_to_hwb, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                 alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED_PLANAR(in0, in1, in2, in_stride, out0, out1, out2, out_stride, count, ALWAN_MAP_LIT(alwan__hsv_to_hwb_kernel));
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hwb_to_hsv, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                 alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP3_P_TILED_PLANAR(in0, in1, in2, in_stride, out0, out1, out2, out_stride, count, ALWAN_MAP_LIT(alwan__hwb_to_hsv_kernel));
    return ALWAN_OK;
}

/* RGB <-> YCbCr (with standard enum) */
int ALWAN_MAP_LITCAT(alwan_rgb_to_ycbcr, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                    alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                    alwan_ycbcr_standard standard,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    alwan_scalar kr_s, kb_s;
    alwan__get_ycbcr_coeffs(standard, &kr_s, &kb_s);
    alwan_map_lane kr = (alwan_map_lane)kr_s, kb = (alwan_map_lane)kb_s;
    size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_MAP_TILE_PIXELS) tile_ = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane ci0_[ALWAN_MAP_TILE_PIXELS], ci1_[ALWAN_MAP_TILE_PIXELS], ci2_[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane co0_[ALWAN_MAP_TILE_PIXELS], co1_[ALWAN_MAP_TILE_PIXELS], co2_[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(ci0_, ci1_, ci2_, in0, in1, in2, off_, in_stride, tile_);
        ALWAN_MAP_LIT(alwan__rgb_to_ycbcr_kernel)(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_, kr, kb);
        ALWAN_MAP_P_NORM_ADD(co1_, tile_, 0.5);
        ALWAN_MAP_P_NORM_ADD(co2_, tile_, 0.5);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, off_, out_stride, co0_, co1_, co2_, tile_);
        off_ += tile_;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_ycbcr_to_rgb, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                    alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                    alwan_ycbcr_standard standard,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    alwan_scalar kr_s, kb_s;
    alwan__get_ycbcr_coeffs(standard, &kr_s, &kb_s);
    alwan_map_lane kr = (alwan_map_lane)kr_s, kb = (alwan_map_lane)kb_s;
    size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_MAP_TILE_PIXELS) tile_ = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane ci0_[ALWAN_MAP_TILE_PIXELS], ci1_[ALWAN_MAP_TILE_PIXELS], ci2_[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane co0_[ALWAN_MAP_TILE_PIXELS], co1_[ALWAN_MAP_TILE_PIXELS], co2_[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(ci0_, ci1_, ci2_, in0, in1, in2, off_, in_stride, tile_);
        ALWAN_MAP_P_NORM_ADD(ci1_, tile_, -0.5);
        ALWAN_MAP_P_NORM_ADD(ci2_, tile_, -0.5);
        ALWAN_MAP_LIT(alwan__ycbcr_to_rgb_kernel)(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_, kr, kb);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, off_, out_stride, co0_, co1_, co2_, tile_);
        off_ += tile_;
    }
    return ALWAN_OK;
}

/* RGB <-> YcCbcCrc */
int ALWAN_MAP_LITCAT(alwan_rgb_to_yccbccrc, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                      alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                      int bit_depth,
                                      size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_MAP_TILE_PIXELS) tile_ = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane ci0_[ALWAN_MAP_TILE_PIXELS], ci1_[ALWAN_MAP_TILE_PIXELS], ci2_[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane co0_[ALWAN_MAP_TILE_PIXELS], co1_[ALWAN_MAP_TILE_PIXELS], co2_[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(ci0_, ci1_, ci2_, in0, in1, in2, off_, in_stride, tile_);
        ALWAN_MAP_LIT(alwan__rgb_to_yccbccrc_kernel)(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_, bit_depth);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, off_, out_stride, co0_, co1_, co2_, tile_);
        off_ += tile_;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_yccbccrc_to_rgb, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                      alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                      int bit_depth,
                                      size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_MAP_TILE_PIXELS) tile_ = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane ci0_[ALWAN_MAP_TILE_PIXELS], ci1_[ALWAN_MAP_TILE_PIXELS], ci2_[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane co0_[ALWAN_MAP_TILE_PIXELS], co1_[ALWAN_MAP_TILE_PIXELS], co2_[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(ci0_, ci1_, ci2_, in0, in1, in2, off_, in_stride, tile_);
        ALWAN_MAP_LIT(alwan__yccbccrc_to_rgb_kernel)(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_, bit_depth);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, off_, out_stride, co0_, co1_, co2_, tile_);
        off_ += tile_;
    }
    return ALWAN_OK;
}

/* YCbCr legal <-> full */
int ALWAN_MAP_LITCAT(alwan_ycbcr_full_to_legal, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                          alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                          int bit_depth,
                                          size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_MAP_TILE_PIXELS) tile_ = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane ci0_[ALWAN_MAP_TILE_PIXELS], ci1_[ALWAN_MAP_TILE_PIXELS], ci2_[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane co0_[ALWAN_MAP_TILE_PIXELS], co1_[ALWAN_MAP_TILE_PIXELS], co2_[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(ci0_, ci1_, ci2_, in0, in1, in2, off_, in_stride, tile_);
        ALWAN_MAP_LIT(alwan__ycbcr_full_to_legal_kernel)(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_, bit_depth);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, off_, out_stride, co0_, co1_, co2_, tile_);
        off_ += tile_;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_ycbcr_legal_to_full, _map_planar)(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
                                          alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
                                          int bit_depth,
                                          size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    size_t off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > ALWAN_MAP_TILE_PIXELS) tile_ = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane ci0_[ALWAN_MAP_TILE_PIXELS], ci1_[ALWAN_MAP_TILE_PIXELS], ci2_[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane co0_[ALWAN_MAP_TILE_PIXELS], co1_[ALWAN_MAP_TILE_PIXELS], co2_[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(ci0_, ci1_, ci2_, in0, in1, in2, off_, in_stride, tile_);
        ALWAN_MAP_LIT(alwan__ycbcr_legal_to_full_kernel)(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_, bit_depth);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, off_, out_stride, co0_, co1_, co2_, tile_);
        off_ += tile_;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CMY -> CMYK planar (3 in -> 4 out, SIMD vectorized)
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_cmy_to_cmyk, _map_planar)(alwan_map_lane *out_c, alwan_map_lane *out_m, alwan_map_lane *out_y, alwan_map_lane *out_k,
                           alwan_map_lane const *in_c, alwan_map_lane const *in_m, alwan_map_lane const *in_y,
                           size_t count, size_t in_stride, size_t out_stride) {
    if (!in_c || !in_m || !in_y || !out_c || !out_m || !out_y || !out_k || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS], d3[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_c, in_m, in_y, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1));
                alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0));
                for (; i + W <= tile; i += W) {
                    alwan_map_simd vc = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd vm = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd vy = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd vk = ALWAN_MAP_NAME(alwan__simd_min3)(vc, vm, vy);
                    alwan_map_simd denom = alwan_map_simd_sub(one, vk);
                    alwan_map_simd safe_d = alwan_map_simd_select(alwan_map_simd_cmpgt(denom, zero), denom, one);
                    alwan_map_simd inv_d = alwan_map_simd_div(one, safe_d);
                    alwan_map_simd_mask full_k = alwan_map_simd_cmpge(vk, one);
                    alwan_map_simd_store(&d0[i], alwan_map_simd_select(full_k, zero, alwan_map_simd_mul(alwan_map_simd_sub(vc, vk), inv_d)));
                    alwan_map_simd_store(&d1[i], alwan_map_simd_select(full_k, zero, alwan_map_simd_mul(alwan_map_simd_sub(vm, vk), inv_d)));
                    alwan_map_simd_store(&d2[i], alwan_map_simd_select(full_k, zero, alwan_map_simd_mul(alwan_map_simd_sub(vy, vk), inv_d)));
                    alwan_map_simd_store(&d3[i], vk);
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_cmy) cmy = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_cmyk) cmyk = ALWAN_MAP_LITV(alwan_cmy_to_cmyk)(cmy);
                d0[i] = (alwan_map_lane)cmyk.c; d1[i] = (alwan_map_lane)cmyk.m;
                d2[i] = (alwan_map_lane)cmyk.y; d3[i] = (alwan_map_lane)cmyk.k;
            }
        }
        /* Store 4 planar channels */
        for (size_t i = 0; i < tile; i++) {
            *(alwan_map_lane *)((char *)out_c + (processed + i) * out_stride) = (alwan_map_lane)d0[i];
            *(alwan_map_lane *)((char *)out_m + (processed + i) * out_stride) = (alwan_map_lane)d1[i];
            *(alwan_map_lane *)((char *)out_y + (processed + i) * out_stride) = (alwan_map_lane)d2[i];
            *(alwan_map_lane *)((char *)out_k + (processed + i) * out_stride) = (alwan_map_lane)d3[i];
        }
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CMYK -> CMY planar (4 in -> 3 out, SIMD vectorized)
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_cmyk_to_cmy, _map_planar)(alwan_map_lane *out_c, alwan_map_lane *out_m, alwan_map_lane *out_y,
                           alwan_map_lane const *in_c, alwan_map_lane const *in_m, alwan_map_lane const *in_y, alwan_map_lane const *in_k,
                           size_t count, size_t in_stride, size_t out_stride) {
    if (!in_c || !in_m || !in_y || !in_k || !out_c || !out_m || !out_y || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS], c3[ALWAN_MAP_TILE_PIXELS];
        /* Load 4 planar input channels */
        for (size_t i = 0; i < tile; i++) {
            c0[i] = (alwan_map_lane)*(alwan_map_lane const *)((char const *)in_c + (processed + i) * in_stride);
            c1[i] = (alwan_map_lane)*(alwan_map_lane const *)((char const *)in_m + (processed + i) * in_stride);
            c2[i] = (alwan_map_lane)*(alwan_map_lane const *)((char const *)in_y + (processed + i) * in_stride);
            c3[i] = (alwan_map_lane)*(alwan_map_lane const *)((char const *)in_k + (processed + i) * in_stride);
        }
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                for (; i + W <= tile; i += W) {
                    alwan_map_simd vc = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd vm = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd vy = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd vk = alwan_map_simd_load(&c3[i]);
                    alwan_map_simd omk = alwan_map_simd_sub(alwan_map_simd_set1((alwan_map_lane)(1)), vk);
                    alwan_map_simd_store(&c0[i], alwan_map_simd_fmadd(vc, omk, vk));
                    alwan_map_simd_store(&c1[i], alwan_map_simd_fmadd(vm, omk, vk));
                    alwan_map_simd_store(&c2[i], alwan_map_simd_fmadd(vy, omk, vk));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_cmyk) cmyk = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i], (alwan_map_lane)c3[i]};
                ALWAN_MAP_LIT(alwan_cmy) cmy = ALWAN_MAP_LITV(alwan_cmyk_to_cmy)(cmyk);
                c0[i] = (alwan_map_lane)cmy.c; c1[i] = (alwan_map_lane)cmy.m; c2[i] = (alwan_map_lane)cmy.y;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_c, out_m, out_y, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}
