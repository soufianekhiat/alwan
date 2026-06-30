/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Convenience Color Model Conversion Kernels (parameterized)
 * No include guard - included once per precision pass.
 */

/* ----------------------------------------------------------------
 * SIMD helpers for HSV/HSL (only needed in SIMD path)
 * ---------------------------------------------------------------- */

#if ALWAN_MAP_SIMD_WIDTH > 1

/* hue_to_rgb helper for HSL -> RGB */
ALWAN_INLINE alwan_map_simd ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(alwan_map_simd p, alwan_map_simd q, alwan_map_simd t) {
    alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
    alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
    alwan_map_simd sixth = alwan_map_simd_set1((alwan_map_lane)(1.0 / 6.0));
    alwan_map_simd half  = alwan_map_simd_set1((alwan_map_lane)(0.5));
    alwan_map_simd two3  = alwan_map_simd_set1((alwan_map_lane)(2.0 / 3.0));
    alwan_map_simd six   = alwan_map_simd_set1((alwan_map_lane)(6.0));

    t = alwan_map_simd_select(alwan_map_simd_cmplt(t, zero), alwan_map_simd_add(t, one), t);
    t = alwan_map_simd_select(alwan_map_simd_cmpgt(t, one),  alwan_map_simd_sub(t, one), t);

    /* result = p + (q - p) * 6 * t   when t < 1/6 */
    alwan_map_simd qmp = alwan_map_simd_sub(q, p);
    alwan_map_simd r1 = alwan_map_simd_fmadd(qmp, alwan_map_simd_mul(six, t), p);
    /* result = q                      when t < 1/2 */
    /* result = p + (q - p) * (2/3 - t) * 6  when t < 2/3 */
    alwan_map_simd r3 = alwan_map_simd_fmadd(qmp, alwan_map_simd_mul(six, alwan_map_simd_sub(two3, t)), p);
    /* result = p                      otherwise */

    /* Build result from inside out (rightmost condition first) */
    alwan_map_simd result = p;
    result = alwan_map_simd_select(alwan_map_simd_cmplt(t, two3), r3, result);
    result = alwan_map_simd_select(alwan_map_simd_cmplt(t, half), q,  result);
    result = alwan_map_simd_select(alwan_map_simd_cmplt(t, sixth), r1, result);
    return result;
}

#endif /* ALWAN_MAP_SIMD_WIDTH > 1 */

/* ----------------------------------------------------------------
 * RGB -> HSV
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_rgb_to_hsv, _map_interleave)(alwan_map_lane *hsv_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count) {
    if (!rgb_in || !hsv_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);

            alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
            alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);
            alwan_map_simd delta = alwan_map_simd_sub(mx, mn);
            alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
            alwan_map_simd sixty = alwan_map_simd_set1((alwan_map_lane)(60.0));
            alwan_map_simd inv360 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 360.0));

            /* V = max */
            alwan_map_simd ov = mx;

            /* S = delta > 0 && max > 0 ? delta / max : 0 */
            alwan_map_simd safe_mx = alwan_map_simd_select(alwan_map_simd_cmpgt(mx, zero), mx, alwan_map_simd_set1((alwan_map_lane)(1.0)));
            alwan_map_simd os = alwan_map_simd_select(alwan_map_simd_cmpgt(mx, zero),
                                                       alwan_map_simd_div(delta, safe_mx), zero);

            /* Hue computation */
            alwan_map_simd safe_delta = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), delta, alwan_map_simd_set1((alwan_map_lane)(1.0)));
            alwan_map_simd inv_delta = alwan_map_simd_div(alwan_map_simd_set1((alwan_map_lane)(1.0)), safe_delta);

            /* h_r = 60 * (g - b) / delta, adjusted +360 if g < b */
            alwan_map_simd h_r = alwan_map_simd_mul(sixty, alwan_map_simd_mul(alwan_map_simd_sub(vg, vb), inv_delta));
            h_r = alwan_map_simd_select(alwan_map_simd_cmplt(vg, vb),
                                        alwan_map_simd_add(h_r, alwan_map_simd_set1((alwan_map_lane)(360.0))), h_r);
            /* h_g = 60 * ((b - r) / delta + 2) */
            alwan_map_simd h_g = alwan_map_simd_mul(sixty,
                alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vb, vr), inv_delta),
                                   alwan_map_simd_set1((alwan_map_lane)(2.0))));
            /* h_b = 60 * ((r - g) / delta + 4) */
            alwan_map_simd h_b = alwan_map_simd_mul(sixty,
                alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vr, vg), inv_delta),
                                   alwan_map_simd_set1((alwan_map_lane)(4.0))));

            /* Select hue based on which channel is max */
            alwan_map_simd_mask r_is_max = alwan_map_simd_cmpeq(mx, vr);
            alwan_map_simd_mask g_is_max = alwan_map_simd_cmpeq(mx, vg);
            alwan_map_simd oh = alwan_map_simd_select(r_is_max, h_r,
                                alwan_map_simd_select(g_is_max, h_g, h_b));
            /* Normalize to [0,1] and zero when achromatic */
            oh = alwan_map_simd_mul(oh, inv360);
            oh = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), oh, zero);

            alwan_map_simd_store(&c0[i], oh);
            alwan_map_simd_store(&c1[i], os);
            alwan_map_simd_store(&c2[i], ov);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_hsv) r = ALWAN_MAP_LITV(alwan_rgb_to_hsv)(v);
            c0[i] = (alwan_map_lane)r.h; c1[i] = (alwan_map_lane)r.s; c2[i] = (alwan_map_lane)r.v;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(hsv_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)rgb_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)hsv_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_rgb) rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_hsv) hsv = ALWAN_MAP_LITV(alwan_rgb_to_hsv)(rgb);
        out_ptr[0] = hsv.h; out_ptr[1] = hsv.s; out_ptr[2] = hsv.v;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * HSV -> RGB
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_hsv_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *hsv_in, size_t in_stride, size_t count) {
    if (!hsv_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, hsv_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vs = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vv = alwan_map_simd_load(&c2[i]);

            alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
            alwan_map_simd s360 = alwan_map_simd_set1((alwan_map_lane)(360.0));
            alwan_map_simd s60  = alwan_map_simd_set1((alwan_map_lane)(60.0));
            alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1.0));

            /* h = h * 360 */
            alwan_map_simd h = alwan_map_simd_mul(vh, s360);
            /* Normalize to [0,360) */
            h = alwan_map_simd_sub(h, alwan_map_simd_mul(alwan_map_simd_floor(alwan_map_simd_div(h, s360)), s360));
            h = alwan_map_simd_select(alwan_map_simd_cmplt(h, zero), alwan_map_simd_add(h, s360), h);

            alwan_map_simd h_sector = alwan_map_simd_div(h, s60);
            alwan_map_simd sector_f = alwan_map_simd_floor(h_sector);
            alwan_map_simd f = alwan_map_simd_sub(h_sector, sector_f);

            alwan_map_simd p = alwan_map_simd_mul(vv, alwan_map_simd_sub(one, vs));
            alwan_map_simd q = alwan_map_simd_mul(vv, alwan_map_simd_sub(one, alwan_map_simd_mul(vs, f)));
            alwan_map_simd t = alwan_map_simd_mul(vv,
                alwan_map_simd_sub(one, alwan_map_simd_mul(vs, alwan_map_simd_sub(one, f))));

            /* 6-sector select tree (same as scalar core) */
            alwan_map_simd r_01 = alwan_map_simd_select(alwan_map_simd_cmplt(sector_f, one), vv, q);
            alwan_map_simd g_01 = alwan_map_simd_select(alwan_map_simd_cmplt(sector_f, one), t, vv);
            alwan_map_simd b_01 = p;

            alwan_map_simd r_23 = p;
            alwan_map_simd g_23 = alwan_map_simd_select(alwan_map_simd_cmplt(sector_f, alwan_map_simd_set1((alwan_map_lane)(3.0))), vv, q);
            alwan_map_simd b_23 = alwan_map_simd_select(alwan_map_simd_cmplt(sector_f, alwan_map_simd_set1((alwan_map_lane)(3.0))), t, vv);

            alwan_map_simd r_45 = alwan_map_simd_select(alwan_map_simd_cmplt(sector_f, alwan_map_simd_set1((alwan_map_lane)(5.0))), t, vv);
            alwan_map_simd g_45 = p;
            alwan_map_simd b_45 = alwan_map_simd_select(alwan_map_simd_cmplt(sector_f, alwan_map_simd_set1((alwan_map_lane)(5.0))), vv, q);

            alwan_map_simd two = alwan_map_simd_set1((alwan_map_lane)(2.0));
            alwan_map_simd four = alwan_map_simd_set1((alwan_map_lane)(4.0));
            alwan_map_simd_mask lt2 = alwan_map_simd_cmplt(sector_f, two);
            alwan_map_simd r_0123 = alwan_map_simd_select(lt2, r_01, r_23);
            alwan_map_simd g_0123 = alwan_map_simd_select(lt2, g_01, g_23);
            alwan_map_simd b_0123 = alwan_map_simd_select(lt2, b_01, b_23);

            alwan_map_simd_mask lt4 = alwan_map_simd_cmplt(sector_f, four);
            alwan_map_simd or = alwan_map_simd_select(lt4, r_0123, r_45);
            alwan_map_simd og = alwan_map_simd_select(lt4, g_0123, g_45);
            alwan_map_simd ob = alwan_map_simd_select(lt4, b_0123, b_45);

            /* Achromatic: s <= 0 -> (v,v,v) */
            alwan_map_simd_mask achro = alwan_map_simd_cmple(vs, zero);
            or = alwan_map_simd_select(achro, vv, or);
            og = alwan_map_simd_select(achro, vv, og);
            ob = alwan_map_simd_select(achro, vv, ob);

            alwan_map_simd_store(&c0[i], or);
            alwan_map_simd_store(&c1[i], og);
            alwan_map_simd_store(&c2[i], ob);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_hsv) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_hsv_to_rgb)(v);
            c0[i] = (alwan_map_lane)r.r; c1[i] = (alwan_map_lane)r.g; c2[i] = (alwan_map_lane)r.b;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)hsv_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)rgb_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_hsv) hsv = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_rgb) rgb = ALWAN_MAP_LITV(alwan_hsv_to_rgb)(hsv);
        out_ptr[0] = rgb.r; out_ptr[1] = rgb.g; out_ptr[2] = rgb.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB -> HSL
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_rgb_to_hsl, _map_interleave)(alwan_map_lane *hsl_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count) {
    if (!rgb_in || !hsl_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);

            alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
            alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);
            alwan_map_simd delta = alwan_map_simd_sub(mx, mn);
            alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
            alwan_map_simd half = alwan_map_simd_set1((alwan_map_lane)(0.5));
            alwan_map_simd sixty = alwan_map_simd_set1((alwan_map_lane)(60.0));
            alwan_map_simd inv360 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 360.0));
            alwan_map_simd two = alwan_map_simd_set1((alwan_map_lane)(2.0));

            /* L = (max + min) / 2 */
            alwan_map_simd ol = alwan_map_simd_mul(alwan_map_simd_add(mx, mn), half);

            /* S: when delta > 0: l < 0.5 ? delta/(max+min) : delta/(2-max-min) */
            alwan_map_simd_mask has_delta = alwan_map_simd_cmpgt(delta, zero);
            alwan_map_simd sum = alwan_map_simd_add(mx, mn);
            alwan_map_simd s_light = alwan_map_simd_div(delta, sum);
            alwan_map_simd s_dark  = alwan_map_simd_div(delta, alwan_map_simd_sub(two, sum));
            alwan_map_simd os = alwan_map_simd_select(has_delta,
                alwan_map_simd_select(alwan_map_simd_cmplt(ol, half), s_light, s_dark), zero);

            /* Hue (same as HSV) */
            alwan_map_simd safe_delta = alwan_map_simd_select(has_delta, delta, alwan_map_simd_set1((alwan_map_lane)(1.0)));
            alwan_map_simd inv_delta = alwan_map_simd_div(alwan_map_simd_set1((alwan_map_lane)(1.0)), safe_delta);

            alwan_map_simd h_r = alwan_map_simd_mul(sixty, alwan_map_simd_mul(alwan_map_simd_sub(vg, vb), inv_delta));
            h_r = alwan_map_simd_select(alwan_map_simd_cmplt(vg, vb),
                                        alwan_map_simd_add(h_r, alwan_map_simd_set1((alwan_map_lane)(360.0))), h_r);
            alwan_map_simd h_g = alwan_map_simd_mul(sixty,
                alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vb, vr), inv_delta), two));
            alwan_map_simd h_b = alwan_map_simd_mul(sixty,
                alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vr, vg), inv_delta),
                                   alwan_map_simd_set1((alwan_map_lane)(4.0))));

            alwan_map_simd_mask r_is_max = alwan_map_simd_cmpeq(mx, vr);
            alwan_map_simd_mask g_is_max = alwan_map_simd_cmpeq(mx, vg);
            alwan_map_simd oh = alwan_map_simd_select(r_is_max, h_r,
                                alwan_map_simd_select(g_is_max, h_g, h_b));
            oh = alwan_map_simd_mul(oh, inv360);
            oh = alwan_map_simd_select(has_delta, oh, zero);

            alwan_map_simd_store(&c0[i], oh);
            alwan_map_simd_store(&c1[i], os);
            alwan_map_simd_store(&c2[i], ol);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_hsl) r = ALWAN_MAP_LITV(alwan_rgb_to_hsl)(v);
            c0[i] = (alwan_map_lane)r.h; c1[i] = (alwan_map_lane)r.s; c2[i] = (alwan_map_lane)r.l;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(hsl_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)rgb_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)hsl_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_rgb) rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_hsl) hsl = ALWAN_MAP_LITV(alwan_rgb_to_hsl)(rgb);
        out_ptr[0] = hsl.h; out_ptr[1] = hsl.s; out_ptr[2] = hsl.l;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * HSL -> RGB
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_hsl_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *hsl_in, size_t in_stride, size_t count) {
    if (!hsl_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, hsl_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vs = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vl = alwan_map_simd_load(&c2[i]);

            alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
            alwan_map_simd half = alwan_map_simd_set1((alwan_map_lane)(0.5));
            alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
            alwan_map_simd two  = alwan_map_simd_set1((alwan_map_lane)(2.0));
            alwan_map_simd third = alwan_map_simd_set1((alwan_map_lane)(1.0 / 3.0));

            /* q = l < 0.5 ? l*(1+s) : l+s-l*s */
            alwan_map_simd q_light = alwan_map_simd_mul(vl, alwan_map_simd_add(one, vs));
            alwan_map_simd q_dark  = alwan_map_simd_sub(alwan_map_simd_add(vl, vs), alwan_map_simd_mul(vl, vs));
            alwan_map_simd q = alwan_map_simd_select(alwan_map_simd_cmplt(vl, half), q_light, q_dark);
            alwan_map_simd p = alwan_map_simd_sub(alwan_map_simd_mul(two, vl), q);

            alwan_map_simd or = ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(p, q, alwan_map_simd_add(vh, third));
            alwan_map_simd og = ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(p, q, vh);
            alwan_map_simd ob = ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(p, q, alwan_map_simd_sub(vh, third));

            /* Achromatic: s <= 0 -> (l,l,l) */
            alwan_map_simd_mask achro = alwan_map_simd_cmple(vs, zero);
            or = alwan_map_simd_select(achro, vl, or);
            og = alwan_map_simd_select(achro, vl, og);
            ob = alwan_map_simd_select(achro, vl, ob);

            alwan_map_simd_store(&c0[i], or);
            alwan_map_simd_store(&c1[i], og);
            alwan_map_simd_store(&c2[i], ob);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_hsl) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_hsl_to_rgb)(v);
            c0[i] = (alwan_map_lane)r.r; c1[i] = (alwan_map_lane)r.g; c2[i] = (alwan_map_lane)r.b;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)hsl_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)rgb_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_hsl) hsl = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_rgb) rgb = ALWAN_MAP_LITV(alwan_hsl_to_rgb)(hsl);
        out_ptr[0] = rgb.r; out_ptr[1] = rgb.g; out_ptr[2] = rgb.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Planar Map Variants
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * RGB -> HSV (planar)
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_rgb_to_hsv, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);

            alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
            alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);
            alwan_map_simd delta = alwan_map_simd_sub(mx, mn);
            alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
            alwan_map_simd sixty = alwan_map_simd_set1((alwan_map_lane)(60.0));
            alwan_map_simd inv360 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 360.0));

            /* V = max */
            alwan_map_simd ov = mx;

            /* S = delta > 0 && max > 0 ? delta / max : 0 */
            alwan_map_simd safe_mx = alwan_map_simd_select(alwan_map_simd_cmpgt(mx, zero), mx, alwan_map_simd_set1((alwan_map_lane)(1.0)));
            alwan_map_simd os = alwan_map_simd_select(alwan_map_simd_cmpgt(mx, zero),
                                                       alwan_map_simd_div(delta, safe_mx), zero);

            /* Hue computation */
            alwan_map_simd safe_delta = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), delta, alwan_map_simd_set1((alwan_map_lane)(1.0)));
            alwan_map_simd inv_delta = alwan_map_simd_div(alwan_map_simd_set1((alwan_map_lane)(1.0)), safe_delta);

            /* h_r = 60 * (g - b) / delta, adjusted +360 if g < b */
            alwan_map_simd h_r = alwan_map_simd_mul(sixty, alwan_map_simd_mul(alwan_map_simd_sub(vg, vb), inv_delta));
            h_r = alwan_map_simd_select(alwan_map_simd_cmplt(vg, vb),
                                        alwan_map_simd_add(h_r, alwan_map_simd_set1((alwan_map_lane)(360.0))), h_r);
            /* h_g = 60 * ((b - r) / delta + 2) */
            alwan_map_simd h_g = alwan_map_simd_mul(sixty,
                alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vb, vr), inv_delta),
                                   alwan_map_simd_set1((alwan_map_lane)(2.0))));
            /* h_b = 60 * ((r - g) / delta + 4) */
            alwan_map_simd h_b = alwan_map_simd_mul(sixty,
                alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vr, vg), inv_delta),
                                   alwan_map_simd_set1((alwan_map_lane)(4.0))));

            /* Select hue based on which channel is max */
            alwan_map_simd_mask r_is_max = alwan_map_simd_cmpeq(mx, vr);
            alwan_map_simd_mask g_is_max = alwan_map_simd_cmpeq(mx, vg);
            alwan_map_simd oh = alwan_map_simd_select(r_is_max, h_r,
                                alwan_map_simd_select(g_is_max, h_g, h_b));
            /* Normalize to [0,1] and zero when achromatic */
            oh = alwan_map_simd_mul(oh, inv360);
            oh = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), oh, zero);

            alwan_map_simd_store(&c0[i], oh);
            alwan_map_simd_store(&c1[i], os);
            alwan_map_simd_store(&c2[i], ov);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_rgb) rgb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_hsv) hsv = ALWAN_MAP_LITV(alwan_rgb_to_hsv)(rgb);
            c0[i] = (alwan_map_lane)hsv.h; c1[i] = (alwan_map_lane)hsv.s; c2[i] = (alwan_map_lane)hsv.v;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_rgb) rgb;
        rgb.r = *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride);
        rgb.g = *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride);
        rgb.b = *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride);
        ALWAN_MAP_LIT(alwan_hsv) hsv = ALWAN_MAP_LITV(alwan_rgb_to_hsv)(rgb);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = hsv.h;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = hsv.s;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = hsv.v;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * HSV -> RGB (planar)
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_hsv_to_rgb, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vs = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vv = alwan_map_simd_load(&c2[i]);

            alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
            alwan_map_simd s360 = alwan_map_simd_set1((alwan_map_lane)(360.0));
            alwan_map_simd s60  = alwan_map_simd_set1((alwan_map_lane)(60.0));
            alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1.0));

            /* h = h * 360 */
            alwan_map_simd h = alwan_map_simd_mul(vh, s360);
            /* Normalize to [0,360) */
            h = alwan_map_simd_sub(h, alwan_map_simd_mul(alwan_map_simd_floor(alwan_map_simd_div(h, s360)), s360));
            h = alwan_map_simd_select(alwan_map_simd_cmplt(h, zero), alwan_map_simd_add(h, s360), h);

            alwan_map_simd h_sector = alwan_map_simd_div(h, s60);
            alwan_map_simd sector_f = alwan_map_simd_floor(h_sector);
            alwan_map_simd f = alwan_map_simd_sub(h_sector, sector_f);

            alwan_map_simd p = alwan_map_simd_mul(vv, alwan_map_simd_sub(one, vs));
            alwan_map_simd q = alwan_map_simd_mul(vv, alwan_map_simd_sub(one, alwan_map_simd_mul(vs, f)));
            alwan_map_simd t = alwan_map_simd_mul(vv,
                alwan_map_simd_sub(one, alwan_map_simd_mul(vs, alwan_map_simd_sub(one, f))));

            /* 6-sector select tree (same as scalar core) */
            alwan_map_simd r_01 = alwan_map_simd_select(alwan_map_simd_cmplt(sector_f, one), vv, q);
            alwan_map_simd g_01 = alwan_map_simd_select(alwan_map_simd_cmplt(sector_f, one), t, vv);
            alwan_map_simd b_01 = p;

            alwan_map_simd r_23 = p;
            alwan_map_simd g_23 = alwan_map_simd_select(alwan_map_simd_cmplt(sector_f, alwan_map_simd_set1((alwan_map_lane)(3.0))), vv, q);
            alwan_map_simd b_23 = alwan_map_simd_select(alwan_map_simd_cmplt(sector_f, alwan_map_simd_set1((alwan_map_lane)(3.0))), t, vv);

            alwan_map_simd r_45 = alwan_map_simd_select(alwan_map_simd_cmplt(sector_f, alwan_map_simd_set1((alwan_map_lane)(5.0))), t, vv);
            alwan_map_simd g_45 = p;
            alwan_map_simd b_45 = alwan_map_simd_select(alwan_map_simd_cmplt(sector_f, alwan_map_simd_set1((alwan_map_lane)(5.0))), vv, q);

            alwan_map_simd two = alwan_map_simd_set1((alwan_map_lane)(2.0));
            alwan_map_simd four = alwan_map_simd_set1((alwan_map_lane)(4.0));
            alwan_map_simd_mask lt2 = alwan_map_simd_cmplt(sector_f, two);
            alwan_map_simd r_0123 = alwan_map_simd_select(lt2, r_01, r_23);
            alwan_map_simd g_0123 = alwan_map_simd_select(lt2, g_01, g_23);
            alwan_map_simd b_0123 = alwan_map_simd_select(lt2, b_01, b_23);

            alwan_map_simd_mask lt4 = alwan_map_simd_cmplt(sector_f, four);
            alwan_map_simd or = alwan_map_simd_select(lt4, r_0123, r_45);
            alwan_map_simd og = alwan_map_simd_select(lt4, g_0123, g_45);
            alwan_map_simd ob = alwan_map_simd_select(lt4, b_0123, b_45);

            /* Achromatic: s <= 0 -> (v,v,v) */
            alwan_map_simd_mask achro = alwan_map_simd_cmple(vs, zero);
            or = alwan_map_simd_select(achro, vv, or);
            og = alwan_map_simd_select(achro, vv, og);
            ob = alwan_map_simd_select(achro, vv, ob);

            alwan_map_simd_store(&c0[i], or);
            alwan_map_simd_store(&c1[i], og);
            alwan_map_simd_store(&c2[i], ob);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_hsv) hsv = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_rgb) rgb = ALWAN_MAP_LITV(alwan_hsv_to_rgb)(hsv);
            c0[i] = (alwan_map_lane)rgb.r; c1[i] = (alwan_map_lane)rgb.g; c2[i] = (alwan_map_lane)rgb.b;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_hsv) hsv;
        hsv.h = *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride);
        hsv.s = *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride);
        hsv.v = *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride);
        ALWAN_MAP_LIT(alwan_rgb) rgb = ALWAN_MAP_LITV(alwan_hsv_to_rgb)(hsv);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = rgb.r;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = rgb.g;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = rgb.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB -> HSL (planar)
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_rgb_to_hsl, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);

            alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
            alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);
            alwan_map_simd delta = alwan_map_simd_sub(mx, mn);
            alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
            alwan_map_simd half = alwan_map_simd_set1((alwan_map_lane)(0.5));
            alwan_map_simd sixty = alwan_map_simd_set1((alwan_map_lane)(60.0));
            alwan_map_simd inv360 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 360.0));
            alwan_map_simd two = alwan_map_simd_set1((alwan_map_lane)(2.0));

            /* L = (max + min) / 2 */
            alwan_map_simd ol = alwan_map_simd_mul(alwan_map_simd_add(mx, mn), half);

            /* S: when delta > 0: l < 0.5 ? delta/(max+min) : delta/(2-max-min) */
            alwan_map_simd_mask has_delta = alwan_map_simd_cmpgt(delta, zero);
            alwan_map_simd sum = alwan_map_simd_add(mx, mn);
            alwan_map_simd s_light = alwan_map_simd_div(delta, sum);
            alwan_map_simd s_dark  = alwan_map_simd_div(delta, alwan_map_simd_sub(two, sum));
            alwan_map_simd os = alwan_map_simd_select(has_delta,
                alwan_map_simd_select(alwan_map_simd_cmplt(ol, half), s_light, s_dark), zero);

            /* Hue (same as HSV) */
            alwan_map_simd safe_delta = alwan_map_simd_select(has_delta, delta, alwan_map_simd_set1((alwan_map_lane)(1.0)));
            alwan_map_simd inv_delta = alwan_map_simd_div(alwan_map_simd_set1((alwan_map_lane)(1.0)), safe_delta);

            alwan_map_simd h_r = alwan_map_simd_mul(sixty, alwan_map_simd_mul(alwan_map_simd_sub(vg, vb), inv_delta));
            h_r = alwan_map_simd_select(alwan_map_simd_cmplt(vg, vb),
                                        alwan_map_simd_add(h_r, alwan_map_simd_set1((alwan_map_lane)(360.0))), h_r);
            alwan_map_simd h_g = alwan_map_simd_mul(sixty,
                alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vb, vr), inv_delta), two));
            alwan_map_simd h_b = alwan_map_simd_mul(sixty,
                alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vr, vg), inv_delta),
                                   alwan_map_simd_set1((alwan_map_lane)(4.0))));

            alwan_map_simd_mask r_is_max = alwan_map_simd_cmpeq(mx, vr);
            alwan_map_simd_mask g_is_max = alwan_map_simd_cmpeq(mx, vg);
            alwan_map_simd oh = alwan_map_simd_select(r_is_max, h_r,
                                alwan_map_simd_select(g_is_max, h_g, h_b));
            oh = alwan_map_simd_mul(oh, inv360);
            oh = alwan_map_simd_select(has_delta, oh, zero);

            alwan_map_simd_store(&c0[i], oh);
            alwan_map_simd_store(&c1[i], os);
            alwan_map_simd_store(&c2[i], ol);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_rgb) rgb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_hsl) hsl = ALWAN_MAP_LITV(alwan_rgb_to_hsl)(rgb);
            c0[i] = (alwan_map_lane)hsl.h; c1[i] = (alwan_map_lane)hsl.s; c2[i] = (alwan_map_lane)hsl.l;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_rgb) rgb;
        rgb.r = *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride);
        rgb.g = *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride);
        rgb.b = *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride);
        ALWAN_MAP_LIT(alwan_hsl) hsl = ALWAN_MAP_LITV(alwan_rgb_to_hsl)(rgb);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = hsl.h;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = hsl.s;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = hsl.l;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * HSL -> RGB (planar)
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_hsl_to_rgb, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vs = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vl = alwan_map_simd_load(&c2[i]);

            alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
            alwan_map_simd half = alwan_map_simd_set1((alwan_map_lane)(0.5));
            alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
            alwan_map_simd two  = alwan_map_simd_set1((alwan_map_lane)(2.0));
            alwan_map_simd third = alwan_map_simd_set1((alwan_map_lane)(1.0 / 3.0));

            /* q = l < 0.5 ? l*(1+s) : l+s-l*s */
            alwan_map_simd q_light = alwan_map_simd_mul(vl, alwan_map_simd_add(one, vs));
            alwan_map_simd q_dark  = alwan_map_simd_sub(alwan_map_simd_add(vl, vs), alwan_map_simd_mul(vl, vs));
            alwan_map_simd q = alwan_map_simd_select(alwan_map_simd_cmplt(vl, half), q_light, q_dark);
            alwan_map_simd p = alwan_map_simd_sub(alwan_map_simd_mul(two, vl), q);

            alwan_map_simd or = ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(p, q, alwan_map_simd_add(vh, third));
            alwan_map_simd og = ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(p, q, vh);
            alwan_map_simd ob = ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(p, q, alwan_map_simd_sub(vh, third));

            /* Achromatic: s <= 0 -> (l,l,l) */
            alwan_map_simd_mask achro = alwan_map_simd_cmple(vs, zero);
            or = alwan_map_simd_select(achro, vl, or);
            og = alwan_map_simd_select(achro, vl, og);
            ob = alwan_map_simd_select(achro, vl, ob);

            alwan_map_simd_store(&c0[i], or);
            alwan_map_simd_store(&c1[i], og);
            alwan_map_simd_store(&c2[i], ob);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_hsl) hsl = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_rgb) rgb = ALWAN_MAP_LITV(alwan_hsl_to_rgb)(hsl);
            c0[i] = (alwan_map_lane)rgb.r; c1[i] = (alwan_map_lane)rgb.g; c2[i] = (alwan_map_lane)rgb.b;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_hsl) hsl;
        hsl.h = *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride);
        hsl.s = *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride);
        hsl.l = *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride);
        ALWAN_MAP_LIT(alwan_rgb) rgb = ALWAN_MAP_LITV(alwan_hsl_to_rgb)(hsl);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = rgb.r;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = rgb.g;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = rgb.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * SIMD kernels for HSP / HSPlog / HSY
 * Process c0/c1/c2 tile buffers in-place.
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__rgb_to_hsp_kernel)(alwan_map_lane *c0, alwan_map_lane *c1, alwan_map_lane *c2, size_t tile) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const zero   = alwan_map_simd_set1((alwan_map_lane)(0.0));
    alwan_map_simd const one    = alwan_map_simd_set1((alwan_map_lane)(1.0));
    alwan_map_simd const sixty  = alwan_map_simd_set1((alwan_map_lane)(60.0));
    alwan_map_simd const s360   = alwan_map_simd_set1((alwan_map_lane)(360.0));
    alwan_map_simd const inv360 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 360.0));
    alwan_map_simd const Pr     = alwan_map_simd_set1((alwan_map_lane)0.299);
    alwan_map_simd const Pg     = alwan_map_simd_set1((alwan_map_lane)0.587);
    alwan_map_simd const Pb     = alwan_map_simd_set1((alwan_map_lane)0.114);
    for (; i + W <= tile; i += W) {
        alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
        alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
        alwan_map_simd vb = alwan_map_simd_load(&c2[i]);

        /* --- H and S identical to HSV --- */
        alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
        alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);
        alwan_map_simd delta = alwan_map_simd_sub(mx, mn);

        /* S = delta / max (safe) */
        alwan_map_simd safe_mx = alwan_map_simd_select(alwan_map_simd_cmpgt(mx, zero), mx, one);
        alwan_map_simd os = alwan_map_simd_select(alwan_map_simd_cmpgt(mx, zero),
                                          alwan_map_simd_div(delta, safe_mx), zero);

        /* Hue */
        alwan_map_simd safe_delta = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), delta, one);
        alwan_map_simd inv_delta = alwan_map_simd_div(one, safe_delta);

        alwan_map_simd h_r = alwan_map_simd_mul(sixty, alwan_map_simd_mul(alwan_map_simd_sub(vg, vb), inv_delta));
        h_r = alwan_map_simd_select(alwan_map_simd_cmplt(vg, vb), alwan_map_simd_add(h_r, s360), h_r);
        alwan_map_simd h_g = alwan_map_simd_mul(sixty,
            alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vb, vr), inv_delta),
                           alwan_map_simd_set1((alwan_map_lane)(2.0))));
        alwan_map_simd h_b = alwan_map_simd_mul(sixty,
            alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vr, vg), inv_delta),
                           alwan_map_simd_set1((alwan_map_lane)(4.0))));

        alwan_map_simd_mask r_is_max = alwan_map_simd_cmpeq(mx, vr);
        alwan_map_simd_mask g_is_max = alwan_map_simd_cmpeq(mx, vg);
        alwan_map_simd oh = alwan_map_simd_select(r_is_max, h_r,
                            alwan_map_simd_select(g_is_max, h_g, h_b));
        oh = alwan_map_simd_mul(oh, inv360);
        oh = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), oh, zero);

        /* --- P = sqrt(Pr*R^2 + Pg*G^2 + Pb*B^2) --- */
        alwan_map_simd op = alwan_map_simd_sqrt(
            alwan_map_simd_fmadd(Pr, alwan_map_simd_mul(vr, vr),
                alwan_map_simd_fmadd(Pg, alwan_map_simd_mul(vg, vg),
                    alwan_map_simd_mul(Pb, alwan_map_simd_mul(vb, vb)))));

        alwan_map_simd_store(&c0[i], oh);
        alwan_map_simd_store(&c1[i], os);
        alwan_map_simd_store(&c2[i], op);
    }
    }
#endif
    for (; i < tile; i++) {
        ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_hsp) r = ALWAN_MAP_LITV(alwan_rgb_to_hsp)(v);
        c0[i] = (alwan_map_lane)r.h; c1[i] = (alwan_map_lane)r.s; c2[i] = (alwan_map_lane)r.p;
    }
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__hsp_to_rgb_kernel)(alwan_map_lane *c0, alwan_map_lane *c1, alwan_map_lane *c2, size_t tile) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
    alwan_map_simd const one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
    alwan_map_simd const s60  = alwan_map_simd_set1((alwan_map_lane)(60.0));
    alwan_map_simd const s360 = alwan_map_simd_set1((alwan_map_lane)(360.0));
    alwan_map_simd const Pr   = alwan_map_simd_set1((alwan_map_lane)0.299);
    alwan_map_simd const Pg   = alwan_map_simd_set1((alwan_map_lane)0.587);
    alwan_map_simd const Pb   = alwan_map_simd_set1((alwan_map_lane)0.114);
    for (; i + W <= tile; i += W) {
        alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
        alwan_map_simd vs = alwan_map_simd_load(&c1[i]);
        alwan_map_simd vp = alwan_map_simd_load(&c2[i]);

        /* h = h * 360, normalize to [0,360) */
        alwan_map_simd h = alwan_map_simd_mul(vh, s360);
        h = alwan_map_simd_sub(h, alwan_map_simd_mul(alwan_map_simd_floor(alwan_map_simd_div(h, s360)), s360));
        h = alwan_map_simd_select(alwan_map_simd_cmplt(h, zero), alwan_map_simd_add(h, s360), h);

        alwan_map_simd min_part = alwan_map_simd_sub(one, vs);
        alwan_map_simd min_part_sq = alwan_map_simd_mul(min_part, min_part);

        /* Sector 0: [0,60) R=max, G=mid, B=min */
        alwan_map_simd f0 = alwan_map_simd_div(h, s60);
        alwan_map_simd mf0 = alwan_map_simd_fmadd(vs, f0, min_part);
        alwan_map_simd d0 = alwan_map_simd_sqrt(alwan_map_simd_fmadd(Pg, alwan_map_simd_mul(mf0, mf0),
                            alwan_map_simd_fmadd(Pb, min_part_sq, Pr)));
        alwan_map_simd safe_d0 = alwan_map_simd_select(alwan_map_simd_cmpgt(d0, zero), d0, one);
        alwan_map_simd v0 = alwan_map_simd_select(alwan_map_simd_cmpgt(d0, zero), alwan_map_simd_div(vp, safe_d0), zero);
        alwan_map_simd r0 = v0;
        alwan_map_simd g0 = alwan_map_simd_mul(v0, mf0);
        alwan_map_simd b0 = alwan_map_simd_mul(v0, min_part);

        /* Sector 1: [60,120) G=max, R=mid, B=min */
        alwan_map_simd f1 = alwan_map_simd_sub(one, alwan_map_simd_div(alwan_map_simd_sub(h, s60), s60));
        alwan_map_simd mf1 = alwan_map_simd_fmadd(vs, f1, min_part);
        alwan_map_simd d1 = alwan_map_simd_sqrt(alwan_map_simd_fmadd(Pr, alwan_map_simd_mul(mf1, mf1),
                            alwan_map_simd_fmadd(Pb, min_part_sq, Pg)));
        alwan_map_simd safe_d1 = alwan_map_simd_select(alwan_map_simd_cmpgt(d1, zero), d1, one);
        alwan_map_simd v1 = alwan_map_simd_select(alwan_map_simd_cmpgt(d1, zero), alwan_map_simd_div(vp, safe_d1), zero);
        alwan_map_simd r1 = alwan_map_simd_mul(v1, mf1);
        alwan_map_simd g1 = v1;
        alwan_map_simd b1 = alwan_map_simd_mul(v1, min_part);

        /* Sector 2: [120,180) G=max, B=mid, R=min */
        alwan_map_simd f2 = alwan_map_simd_div(alwan_map_simd_sub(h, alwan_map_simd_set1((alwan_map_lane)(120.0))), s60);
        alwan_map_simd mf2 = alwan_map_simd_fmadd(vs, f2, min_part);
        alwan_map_simd d2 = alwan_map_simd_sqrt(alwan_map_simd_fmadd(Pb, alwan_map_simd_mul(mf2, mf2),
                            alwan_map_simd_fmadd(Pr, min_part_sq, Pg)));
        alwan_map_simd safe_d2 = alwan_map_simd_select(alwan_map_simd_cmpgt(d2, zero), d2, one);
        alwan_map_simd v2 = alwan_map_simd_select(alwan_map_simd_cmpgt(d2, zero), alwan_map_simd_div(vp, safe_d2), zero);
        alwan_map_simd r2 = alwan_map_simd_mul(v2, min_part);
        alwan_map_simd g2 = v2;
        alwan_map_simd b2 = alwan_map_simd_mul(v2, mf2);

        /* Sector 3: [180,240) B=max, G=mid, R=min */
        alwan_map_simd f3 = alwan_map_simd_sub(one, alwan_map_simd_div(alwan_map_simd_sub(h, alwan_map_simd_set1((alwan_map_lane)(180.0))), s60));
        alwan_map_simd mf3 = alwan_map_simd_fmadd(vs, f3, min_part);
        alwan_map_simd d3 = alwan_map_simd_sqrt(alwan_map_simd_fmadd(Pg, alwan_map_simd_mul(mf3, mf3),
                            alwan_map_simd_fmadd(Pr, min_part_sq, Pb)));
        alwan_map_simd safe_d3 = alwan_map_simd_select(alwan_map_simd_cmpgt(d3, zero), d3, one);
        alwan_map_simd v3 = alwan_map_simd_select(alwan_map_simd_cmpgt(d3, zero), alwan_map_simd_div(vp, safe_d3), zero);
        alwan_map_simd r3 = alwan_map_simd_mul(v3, min_part);
        alwan_map_simd g3 = alwan_map_simd_mul(v3, mf3);
        alwan_map_simd b3 = v3;

        /* Sector 4: [240,300) B=max, R=mid, G=min */
        alwan_map_simd f4 = alwan_map_simd_div(alwan_map_simd_sub(h, alwan_map_simd_set1((alwan_map_lane)(240.0))), s60);
        alwan_map_simd mf4 = alwan_map_simd_fmadd(vs, f4, min_part);
        alwan_map_simd d4 = alwan_map_simd_sqrt(alwan_map_simd_fmadd(Pr, alwan_map_simd_mul(mf4, mf4),
                            alwan_map_simd_fmadd(Pg, min_part_sq, Pb)));
        alwan_map_simd safe_d4 = alwan_map_simd_select(alwan_map_simd_cmpgt(d4, zero), d4, one);
        alwan_map_simd v4 = alwan_map_simd_select(alwan_map_simd_cmpgt(d4, zero), alwan_map_simd_div(vp, safe_d4), zero);
        alwan_map_simd r4 = alwan_map_simd_mul(v4, mf4);
        alwan_map_simd g4 = alwan_map_simd_mul(v4, min_part);
        alwan_map_simd b4 = v4;

        /* Sector 5: [300,360) R=max, B=mid, G=min */
        alwan_map_simd f5 = alwan_map_simd_sub(one, alwan_map_simd_div(alwan_map_simd_sub(h, alwan_map_simd_set1((alwan_map_lane)(300.0))), s60));
        alwan_map_simd mf5 = alwan_map_simd_fmadd(vs, f5, min_part);
        alwan_map_simd d5 = alwan_map_simd_sqrt(alwan_map_simd_fmadd(Pb, alwan_map_simd_mul(mf5, mf5),
                            alwan_map_simd_fmadd(Pg, min_part_sq, Pr)));
        alwan_map_simd safe_d5 = alwan_map_simd_select(alwan_map_simd_cmpgt(d5, zero), d5, one);
        alwan_map_simd v5 = alwan_map_simd_select(alwan_map_simd_cmpgt(d5, zero), alwan_map_simd_div(vp, safe_d5), zero);
        alwan_map_simd r5 = v5;
        alwan_map_simd g5 = alwan_map_simd_mul(v5, min_part);
        alwan_map_simd b5 = alwan_map_simd_mul(v5, mf5);

        /* Binary select tree */
        alwan_map_simd_mask lt60  = alwan_map_simd_cmplt(h, s60);
        alwan_map_simd_mask lt120 = alwan_map_simd_cmplt(h, alwan_map_simd_set1((alwan_map_lane)(120.0)));
        alwan_map_simd_mask lt180 = alwan_map_simd_cmplt(h, alwan_map_simd_set1((alwan_map_lane)(180.0)));
        alwan_map_simd_mask lt240 = alwan_map_simd_cmplt(h, alwan_map_simd_set1((alwan_map_lane)(240.0)));
        alwan_map_simd_mask lt300 = alwan_map_simd_cmplt(h, alwan_map_simd_set1((alwan_map_lane)(300.0)));

        alwan_map_simd or_01 = alwan_map_simd_select(lt60, r0, r1);
        alwan_map_simd og_01 = alwan_map_simd_select(lt60, g0, g1);
        alwan_map_simd ob_01 = alwan_map_simd_select(lt60, b0, b1);

        alwan_map_simd or_23 = alwan_map_simd_select(lt180, r2, r3);
        alwan_map_simd og_23 = alwan_map_simd_select(lt180, g2, g3);
        alwan_map_simd ob_23 = alwan_map_simd_select(lt180, b2, b3);

        alwan_map_simd or_45 = alwan_map_simd_select(lt300, r4, r5);
        alwan_map_simd og_45 = alwan_map_simd_select(lt300, g4, g5);
        alwan_map_simd ob_45 = alwan_map_simd_select(lt300, b4, b5);

        alwan_map_simd or_0123 = alwan_map_simd_select(lt120, or_01, or_23);
        alwan_map_simd og_0123 = alwan_map_simd_select(lt120, og_01, og_23);
        alwan_map_simd ob_0123 = alwan_map_simd_select(lt120, ob_01, ob_23);

        alwan_map_simd or = alwan_map_simd_select(lt240, or_0123, or_45);
        alwan_map_simd og = alwan_map_simd_select(lt240, og_0123, og_45);
        alwan_map_simd ob = alwan_map_simd_select(lt240, ob_0123, ob_45);

        /* Achromatic: s <= 0 -> (p, p, p) */
        alwan_map_simd_mask achro = alwan_map_simd_cmple(vs, zero);
        or = alwan_map_simd_select(achro, vp, or);
        og = alwan_map_simd_select(achro, vp, og);
        ob = alwan_map_simd_select(achro, vp, ob);

        alwan_map_simd_store(&c0[i], or);
        alwan_map_simd_store(&c1[i], og);
        alwan_map_simd_store(&c2[i], ob);
    }
    }
#endif
    for (; i < tile; i++) {
        ALWAN_MAP_LIT(alwan_hsp) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_hsp_to_rgb)(v);
        c0[i] = (alwan_map_lane)r.r; c1[i] = (alwan_map_lane)r.g; c2[i] = (alwan_map_lane)r.b;
    }
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__rgb_to_hsplog_kernel)(alwan_map_lane *c0, alwan_map_lane *c1, alwan_map_lane *c2, size_t tile) {
    ALWAN_MAP_LIT(alwan__rgb_to_hsp_kernel)(c0, c1, c2, tile);
    /* Apply log10 to saturation channel */
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const nine = alwan_map_simd_set1((alwan_map_lane)(9.0));
    alwan_map_simd const one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
    for (; i + W <= tile; i += W) {
        alwan_map_simd s = alwan_map_simd_load(&c1[i]);
        alwan_map_simd_store(&c1[i], alwan_map_simd_log10(alwan_map_simd_fmadd(nine, s, one)));
    }
    }
#endif
    for (; i < tile; i++)
        c1[i] = (alwan_map_lane)alwan_map_scalar_log10((alwan_map_lane)(1.0) + (alwan_map_lane)(9.0) * (alwan_map_lane)c1[i]);
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__hsplog_to_rgb_kernel)(alwan_map_lane *c0, alwan_map_lane *c1, alwan_map_lane *c2, size_t tile) {
    /* Invert log saturation: S = (10^S_log - 1) / 9 */
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const one     = alwan_map_simd_set1((alwan_map_lane)(1.0));
    alwan_map_simd const inv9    = alwan_map_simd_set1((alwan_map_lane)(1.0 / 9.0));
    alwan_map_simd const ten     = alwan_map_simd_set1((alwan_map_lane)(10.0));
    for (; i + W <= tile; i += W) {
        alwan_map_simd s_log = alwan_map_simd_load(&c1[i]);
        alwan_map_simd s = alwan_map_simd_mul(alwan_map_simd_sub(alwan_map_simd_pow(ten, s_log), one), inv9);
        alwan_map_simd_store(&c1[i], s);
    }
    }
#endif
    for (; i < tile; i++) {
        alwan_map_lane s_log = (alwan_map_lane)c1[i];
        c1[i] = (alwan_map_lane)((alwan_map_scalar_pow((alwan_map_lane)(10.0), s_log) - (alwan_map_lane)(1.0)) / (alwan_map_lane)(9.0));
    }
    ALWAN_MAP_LIT(alwan__hsp_to_rgb_kernel)(c0, c1, c2, tile);
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__rgb_to_hsy_kernel)(alwan_map_lane *c0, alwan_map_lane *c1, alwan_map_lane *c2, size_t tile) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const zero   = alwan_map_simd_set1((alwan_map_lane)(0.0));
    alwan_map_simd const one    = alwan_map_simd_set1((alwan_map_lane)(1.0));
    alwan_map_simd const sixty  = alwan_map_simd_set1((alwan_map_lane)(60.0));
    alwan_map_simd const s360   = alwan_map_simd_set1((alwan_map_lane)(360.0));
    alwan_map_simd const inv360 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 360.0));
    alwan_map_simd const six    = alwan_map_simd_set1((alwan_map_lane)(6.0));
    alwan_map_simd const eps    = alwan_map_simd_set1((alwan_map_lane)1e-10);
    alwan_map_simd const kr     = alwan_map_simd_set1((alwan_map_lane)0.299);
    alwan_map_simd const kg     = alwan_map_simd_set1((alwan_map_lane)0.587);
    alwan_map_simd const kb     = alwan_map_simd_set1((alwan_map_lane)0.114);
    for (; i + W <= tile; i += W) {
        alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
        alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
        alwan_map_simd vb = alwan_map_simd_load(&c2[i]);

        /* --- H identical to HSV --- */
        alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
        alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);
        alwan_map_simd delta = alwan_map_simd_sub(mx, mn);

        alwan_map_simd safe_delta = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), delta, one);
        alwan_map_simd inv_delta = alwan_map_simd_div(one, safe_delta);

        alwan_map_simd h_r = alwan_map_simd_mul(sixty, alwan_map_simd_mul(alwan_map_simd_sub(vg, vb), inv_delta));
        h_r = alwan_map_simd_select(alwan_map_simd_cmplt(vg, vb), alwan_map_simd_add(h_r, s360), h_r);
        alwan_map_simd h_g = alwan_map_simd_mul(sixty,
            alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vb, vr), inv_delta),
                           alwan_map_simd_set1((alwan_map_lane)(2.0))));
        alwan_map_simd h_b = alwan_map_simd_mul(sixty,
            alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vr, vg), inv_delta),
                           alwan_map_simd_set1((alwan_map_lane)(4.0))));

        alwan_map_simd_mask r_is_max = alwan_map_simd_cmpeq(mx, vr);
        alwan_map_simd_mask g_is_max = alwan_map_simd_cmpeq(mx, vg);
        alwan_map_simd oh = alwan_map_simd_select(r_is_max, h_r,
                            alwan_map_simd_select(g_is_max, h_g, h_b));
        oh = alwan_map_simd_mul(oh, inv360);
        oh = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), oh, zero);

        /* --- Y = kr*R + kg*G + kb*B --- */
        alwan_map_simd y = alwan_map_simd_fmadd(kr, vr, alwan_map_simd_fmadd(kg, vg, alwan_map_simd_mul(kb, vb)));

        /* --- Chroma = max - min --- */
        alwan_map_simd chroma = delta;

        /* --- Compute y_pure (luma of pure-hue color at this hue) --- */
        alwan_map_simd h6 = alwan_map_simd_mul(oh, six);
        alwan_map_simd sector = alwan_map_simd_floor(h6);
        alwan_map_simd f = alwan_map_simd_sub(h6, sector);
        alwan_map_simd one_minus_f = alwan_map_simd_sub(one, f);

        /* Pure-hue RGB per sector */
        alwan_map_simd pr_01 = alwan_map_simd_select(alwan_map_simd_cmplt(sector, one), one, one_minus_f);
        alwan_map_simd pg_01 = alwan_map_simd_select(alwan_map_simd_cmplt(sector, one), f, one);
        alwan_map_simd pb_01 = zero;

        alwan_map_simd pr_23 = zero;
        alwan_map_simd pg_23 = alwan_map_simd_select(alwan_map_simd_cmplt(sector, alwan_map_simd_set1((alwan_map_lane)(3.0))), one, one_minus_f);
        alwan_map_simd pb_23 = alwan_map_simd_select(alwan_map_simd_cmplt(sector, alwan_map_simd_set1((alwan_map_lane)(3.0))), f, one);

        alwan_map_simd pr_45 = alwan_map_simd_select(alwan_map_simd_cmplt(sector, alwan_map_simd_set1((alwan_map_lane)(5.0))), f, one);
        alwan_map_simd pg_45 = zero;
        alwan_map_simd pb_45 = alwan_map_simd_select(alwan_map_simd_cmplt(sector, alwan_map_simd_set1((alwan_map_lane)(5.0))), one, one_minus_f);

        alwan_map_simd two  = alwan_map_simd_set1((alwan_map_lane)(2.0));
        alwan_map_simd four = alwan_map_simd_set1((alwan_map_lane)(4.0));
        alwan_map_simd_mask lt2 = alwan_map_simd_cmplt(sector, two);
        alwan_map_simd pr_0123 = alwan_map_simd_select(lt2, pr_01, pr_23);
        alwan_map_simd pg_0123 = alwan_map_simd_select(lt2, pg_01, pg_23);
        alwan_map_simd pb_0123 = alwan_map_simd_select(lt2, pb_01, pb_23);

        alwan_map_simd_mask lt4 = alwan_map_simd_cmplt(sector, four);
        alwan_map_simd pr_pure = alwan_map_simd_select(lt4, pr_0123, pr_45);
        alwan_map_simd pg_pure = alwan_map_simd_select(lt4, pg_0123, pg_45);
        alwan_map_simd pb_pure = alwan_map_simd_select(lt4, pb_0123, pb_45);

        /* y_pure = kr*pr + kg*pg + kb*pb */
        alwan_map_simd y_pure = alwan_map_simd_fmadd(kr, pr_pure,
                                alwan_map_simd_fmadd(kg, pg_pure,
                                    alwan_map_simd_mul(kb, pb_pure)));

        /* safe_ypure, safe_ypure_inv */
        alwan_map_simd safe_ypure = alwan_map_simd_select(alwan_map_simd_cmplt(y_pure, eps), eps, y_pure);
        alwan_map_simd one_minus_ypure = alwan_map_simd_sub(one, y_pure);
        alwan_map_simd safe_ypure_inv = alwan_map_simd_select(alwan_map_simd_cmplt(one_minus_ypure, eps),
                                                       eps, one_minus_ypure);

        /* max_c = select(y <= y_pure, y/safe_ypure, (1-y)/safe_ypure_inv) */
        alwan_map_simd one_minus_y = alwan_map_simd_sub(one, y);
        alwan_map_simd_mask y_le_ypure = alwan_map_simd_cmple(y, y_pure);
        alwan_map_simd max_c = alwan_map_simd_select(y_le_ypure,
                               alwan_map_simd_div(y, safe_ypure),
                               alwan_map_simd_div(one_minus_y, safe_ypure_inv));
        max_c = alwan_map_simd_select(alwan_map_simd_cmplt(max_c, eps), eps, max_c);

        /* S = select(chroma > 0, chroma / max_c, 0) */
        alwan_map_simd os = alwan_map_simd_select(alwan_map_simd_cmpgt(chroma, zero),
                            alwan_map_simd_div(chroma, max_c), zero);

        alwan_map_simd_store(&c0[i], oh);
        alwan_map_simd_store(&c1[i], os);
        alwan_map_simd_store(&c2[i], y);
    }
    }
#endif
    for (; i < tile; i++) {
        ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_hsy) r = ALWAN_MAP_LITV(alwan_rgb_to_hsy)(v);
        c0[i] = (alwan_map_lane)r.h; c1[i] = (alwan_map_lane)r.s; c2[i] = (alwan_map_lane)r.y;
    }
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__hsy_to_rgb_kernel)(alwan_map_lane *c0, alwan_map_lane *c1, alwan_map_lane *c2, size_t tile) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
    alwan_map_simd const one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
    alwan_map_simd const six  = alwan_map_simd_set1((alwan_map_lane)(6.0));
    alwan_map_simd const eps  = alwan_map_simd_set1((alwan_map_lane)1e-10);
    alwan_map_simd const kr   = alwan_map_simd_set1((alwan_map_lane)0.299);
    alwan_map_simd const kg   = alwan_map_simd_set1((alwan_map_lane)0.587);
    alwan_map_simd const kb   = alwan_map_simd_set1((alwan_map_lane)0.114);
    for (; i + W <= tile; i += W) {
        alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
        alwan_map_simd vs = alwan_map_simd_load(&c1[i]);
        alwan_map_simd vy = alwan_map_simd_load(&c2[i]);

        /* --- Compute y_pure (luma of pure-hue color at this hue) --- */
        alwan_map_simd h6 = alwan_map_simd_mul(vh, six);
        alwan_map_simd sector = alwan_map_simd_floor(h6);
        alwan_map_simd f = alwan_map_simd_sub(h6, sector);
        alwan_map_simd one_minus_f = alwan_map_simd_sub(one, f);

        /* Pure-hue RGB per sector */
        alwan_map_simd pr_01 = alwan_map_simd_select(alwan_map_simd_cmplt(sector, one), one, one_minus_f);
        alwan_map_simd pg_01 = alwan_map_simd_select(alwan_map_simd_cmplt(sector, one), f, one);
        alwan_map_simd pb_01 = zero;

        alwan_map_simd pr_23 = zero;
        alwan_map_simd pg_23 = alwan_map_simd_select(alwan_map_simd_cmplt(sector, alwan_map_simd_set1((alwan_map_lane)(3.0))), one, one_minus_f);
        alwan_map_simd pb_23 = alwan_map_simd_select(alwan_map_simd_cmplt(sector, alwan_map_simd_set1((alwan_map_lane)(3.0))), f, one);

        alwan_map_simd pr_45 = alwan_map_simd_select(alwan_map_simd_cmplt(sector, alwan_map_simd_set1((alwan_map_lane)(5.0))), f, one);
        alwan_map_simd pg_45 = zero;
        alwan_map_simd pb_45 = alwan_map_simd_select(alwan_map_simd_cmplt(sector, alwan_map_simd_set1((alwan_map_lane)(5.0))), one, one_minus_f);

        alwan_map_simd two  = alwan_map_simd_set1((alwan_map_lane)(2.0));
        alwan_map_simd four = alwan_map_simd_set1((alwan_map_lane)(4.0));
        alwan_map_simd_mask lt2 = alwan_map_simd_cmplt(sector, two);
        alwan_map_simd pr_0123 = alwan_map_simd_select(lt2, pr_01, pr_23);
        alwan_map_simd pg_0123 = alwan_map_simd_select(lt2, pg_01, pg_23);
        alwan_map_simd pb_0123 = alwan_map_simd_select(lt2, pb_01, pb_23);

        alwan_map_simd_mask lt4 = alwan_map_simd_cmplt(sector, four);
        alwan_map_simd pr_pure = alwan_map_simd_select(lt4, pr_0123, pr_45);
        alwan_map_simd pg_pure = alwan_map_simd_select(lt4, pg_0123, pg_45);
        alwan_map_simd pb_pure = alwan_map_simd_select(lt4, pb_0123, pb_45);

        /* y_pure = kr*pr + kg*pg + kb*pb */
        alwan_map_simd y_pure = alwan_map_simd_fmadd(kr, pr_pure,
                                alwan_map_simd_fmadd(kg, pg_pure,
                                    alwan_map_simd_mul(kb, pb_pure)));

        /* safe_ypure, safe_ypure_inv */
        alwan_map_simd safe_ypure = alwan_map_simd_select(alwan_map_simd_cmplt(y_pure, eps), eps, y_pure);
        alwan_map_simd one_minus_ypure = alwan_map_simd_sub(one, y_pure);
        alwan_map_simd safe_ypure_inv = alwan_map_simd_select(alwan_map_simd_cmplt(one_minus_ypure, eps),
                                                       eps, one_minus_ypure);

        /* max_c = select(y <= y_pure, y/safe_ypure, (1-y)/safe_ypure_inv) */
        alwan_map_simd one_minus_y = alwan_map_simd_sub(one, vy);
        alwan_map_simd_mask y_le_ypure = alwan_map_simd_cmple(vy, y_pure);
        alwan_map_simd max_c = alwan_map_simd_select(y_le_ypure,
                               alwan_map_simd_div(vy, safe_ypure),
                               alwan_map_simd_div(one_minus_y, safe_ypure_inv));

        /* chroma = S * max_c */
        alwan_map_simd chroma = alwan_map_simd_mul(vs, max_c);

        /* Scale pure-hue by chroma */
        alwan_map_simd r_hue = alwan_map_simd_mul(chroma, pr_pure);
        alwan_map_simd g_hue = alwan_map_simd_mul(chroma, pg_pure);
        alwan_map_simd b_hue = alwan_map_simd_mul(chroma, pb_pure);

        /* m = y - (kr*r_hue + kg*g_hue + kb*b_hue) */
        alwan_map_simd m = alwan_map_simd_sub(vy,
            alwan_map_simd_fmadd(kr, r_hue,
                alwan_map_simd_fmadd(kg, g_hue,
                    alwan_map_simd_mul(kb, b_hue))));

        alwan_map_simd or = alwan_map_simd_add(r_hue, m);
        alwan_map_simd og = alwan_map_simd_add(g_hue, m);
        alwan_map_simd ob = alwan_map_simd_add(b_hue, m);

        /* Achromatic: s <= 0 -> (y, y, y) */
        alwan_map_simd_mask achro = alwan_map_simd_cmple(vs, zero);
        or = alwan_map_simd_select(achro, vy, or);
        og = alwan_map_simd_select(achro, vy, og);
        ob = alwan_map_simd_select(achro, vy, ob);

        alwan_map_simd_store(&c0[i], or);
        alwan_map_simd_store(&c1[i], og);
        alwan_map_simd_store(&c2[i], ob);
    }
    }
#endif
    for (; i < tile; i++) {
        ALWAN_MAP_LIT(alwan_hsy) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_hsy_to_rgb)(v);
        c0[i] = (alwan_map_lane)r.r; c1[i] = (alwan_map_lane)r.g; c2[i] = (alwan_map_lane)r.b;
    }
}

/* ----------------------------------------------------------------
 * RGB -> HSP
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_rgb_to_hsp, _map_interleave)(alwan_map_lane *hsp_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count) {
    if (!rgb_in || !hsp_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__rgb_to_hsp_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(hsp_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * HSP -> RGB
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_hsp_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *hsp_in, size_t in_stride, size_t count) {
    if (!hsp_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, hsp_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__hsp_to_rgb_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB -> HSPLog
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_rgb_to_hsplog, _map_interleave)(alwan_map_lane *hsplog_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count) {
    if (!rgb_in || !hsplog_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__rgb_to_hsplog_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(hsplog_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * HSPLog -> RGB
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_hsplog_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *hsplog_in, size_t in_stride, size_t count) {
    if (!hsplog_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, hsplog_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__hsplog_to_rgb_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB -> HSY
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_rgb_to_hsy, _map_interleave)(alwan_map_lane *hsy_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count) {
    if (!rgb_in || !hsy_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__rgb_to_hsy_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(hsy_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * HSY -> RGB
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_hsy_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *hsy_in, size_t in_stride, size_t count) {
    if (!hsy_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, hsy_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__hsy_to_rgb_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ================================================================
 * Linear sRGB <-> HSV/HSL (chains sRGB OETF/EOTF with HSV/HSL)
 * ================================================================ */

int ALWAN_MAP_LITCAT(alwan_linear_srgb_to_hsv, _map_interleave)(alwan_map_lane *hsv_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count) {
    if (!rgb_in || !hsv_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
                alwan_map_simd sixty = alwan_map_simd_set1((alwan_map_lane)(60.0));
                alwan_map_simd inv360 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 360.0));
                for (; i + W <= tile; i += W) {
                    /* Apply sRGB OETF (linear -> nonlinear) */
                    alwan_map_simd vr = ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd_load(&c0[i]));
                    alwan_map_simd vg = ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd_load(&c1[i]));
                    alwan_map_simd vb = ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd_load(&c2[i]));
                    /* RGB->HSV */
                    alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
                    alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);
                    alwan_map_simd delta = alwan_map_simd_sub(mx, mn);
                    alwan_map_simd safe_mx = alwan_map_simd_select(alwan_map_simd_cmpgt(mx, zero), mx, alwan_map_simd_set1((alwan_map_lane)(1.0)));
                    alwan_map_simd os = alwan_map_simd_select(alwan_map_simd_cmpgt(mx, zero), alwan_map_simd_div(delta, safe_mx), zero);
                    alwan_map_simd safe_d = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), delta, alwan_map_simd_set1((alwan_map_lane)(1.0)));
                    alwan_map_simd inv_d = alwan_map_simd_div(alwan_map_simd_set1((alwan_map_lane)(1.0)), safe_d);
                    alwan_map_simd h_r = alwan_map_simd_mul(sixty, alwan_map_simd_mul(alwan_map_simd_sub(vg, vb), inv_d));
                    h_r = alwan_map_simd_select(alwan_map_simd_cmplt(vg, vb), alwan_map_simd_add(h_r, alwan_map_simd_set1((alwan_map_lane)(360.0))), h_r);
                    alwan_map_simd h_g = alwan_map_simd_mul(sixty, alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vb, vr), inv_d), alwan_map_simd_set1((alwan_map_lane)(2.0))));
                    alwan_map_simd h_b = alwan_map_simd_mul(sixty, alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vr, vg), inv_d), alwan_map_simd_set1((alwan_map_lane)(4.0))));
                    alwan_map_simd oh = alwan_map_simd_select(alwan_map_simd_cmpeq(mx, vr), h_r,
                                       alwan_map_simd_select(alwan_map_simd_cmpeq(mx, vg), h_g, h_b));
                    oh = alwan_map_simd_mul(oh, inv360);
                    oh = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), oh, zero);
                    alwan_map_simd_store(&c0[i], oh);
                    alwan_map_simd_store(&c1[i], os);
                    alwan_map_simd_store(&c2[i], mx);
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_hsv) r = ALWAN_MAP_LITV(alwan_linear_srgb_to_hsv)(v);
                c0[i] = (alwan_map_lane)r.h; c1[i] = (alwan_map_lane)r.s; c2[i] = (alwan_map_lane)r.v;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(hsv_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hsv_to_linear_srgb, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *hsv_in, size_t in_stride, size_t count) {
    if (!hsv_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, hsv_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
                alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
                alwan_map_simd v360 = alwan_map_simd_set1((alwan_map_lane)(360.0));
                alwan_map_simd v60  = alwan_map_simd_set1((alwan_map_lane)(60.0));
                for (; i + W <= tile; i += W) {
                    alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd vs = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd vv = alwan_map_simd_load(&c2[i]);
                    /* HSV->RGB (6-sector branchless) */
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
                    alwan_map_simd or_ = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(4.0))), r0123, r45);
                    alwan_map_simd og = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(4.0))), g0123, p);
                    alwan_map_simd ob = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(4.0))), b0123, b45);
                    or_ = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vv, or_);
                    og = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vv, og);
                    ob = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vv, ob);
                    /* Apply sRGB EOTF (nonlinear -> linear) */
                    alwan_map_simd_store(&c0[i], ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(or_));
                    alwan_map_simd_store(&c1[i], ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(og));
                    alwan_map_simd_store(&c2[i], ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(ob));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_hsv) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_hsv_to_linear_srgb)(v);
                c0[i] = (alwan_map_lane)r.r; c1[i] = (alwan_map_lane)r.g; c2[i] = (alwan_map_lane)r.b;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_linear_srgb_to_hsl, _map_interleave)(alwan_map_lane *hsl_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count) {
    if (!rgb_in || !hsl_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
                alwan_map_simd half = alwan_map_simd_set1((alwan_map_lane)(0.5));
                alwan_map_simd sixty = alwan_map_simd_set1((alwan_map_lane)(60.0));
                alwan_map_simd inv360 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 360.0));
                for (; i + W <= tile; i += W) {
                    alwan_map_simd vr = ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd_load(&c0[i]));
                    alwan_map_simd vg = ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd_load(&c1[i]));
                    alwan_map_simd vb = ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd_load(&c2[i]));
                    /* RGB->HSL */
                    alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
                    alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);
                    alwan_map_simd delta = alwan_map_simd_sub(mx, mn);
                    alwan_map_simd ol = alwan_map_simd_mul(half, alwan_map_simd_add(mx, mn));
                    alwan_map_simd safe_d = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), delta, alwan_map_simd_set1((alwan_map_lane)(1.0)));
                    alwan_map_simd inv_d = alwan_map_simd_div(alwan_map_simd_set1((alwan_map_lane)(1.0)), safe_d);
                    alwan_map_simd denom = alwan_map_simd_sub(alwan_map_simd_set1((alwan_map_lane)(1.0)), alwan_map_simd_abs(alwan_map_simd_sub(alwan_map_simd_add(mx, mn), alwan_map_simd_set1((alwan_map_lane)(1.0)))));
                    alwan_map_simd safe_den = alwan_map_simd_select(alwan_map_simd_cmpgt(denom, zero), denom, alwan_map_simd_set1((alwan_map_lane)(1.0)));
                    alwan_map_simd os = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), alwan_map_simd_div(delta, safe_den), zero);
                    alwan_map_simd h_r = alwan_map_simd_mul(sixty, alwan_map_simd_mul(alwan_map_simd_sub(vg, vb), inv_d));
                    h_r = alwan_map_simd_select(alwan_map_simd_cmplt(vg, vb), alwan_map_simd_add(h_r, alwan_map_simd_set1((alwan_map_lane)(360.0))), h_r);
                    alwan_map_simd h_g = alwan_map_simd_mul(sixty, alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vb, vr), inv_d), alwan_map_simd_set1((alwan_map_lane)(2.0))));
                    alwan_map_simd h_b = alwan_map_simd_mul(sixty, alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vr, vg), inv_d), alwan_map_simd_set1((alwan_map_lane)(4.0))));
                    alwan_map_simd oh = alwan_map_simd_select(alwan_map_simd_cmpeq(mx, vr), h_r,
                                       alwan_map_simd_select(alwan_map_simd_cmpeq(mx, vg), h_g, h_b));
                    oh = alwan_map_simd_mul(oh, inv360);
                    oh = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), oh, zero);
                    alwan_map_simd_store(&c0[i], oh);
                    alwan_map_simd_store(&c1[i], os);
                    alwan_map_simd_store(&c2[i], ol);
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_hsl) r = ALWAN_MAP_LITV(alwan_linear_srgb_to_hsl)(v);
                c0[i] = (alwan_map_lane)r.h; c1[i] = (alwan_map_lane)r.s; c2[i] = (alwan_map_lane)r.l;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(hsl_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hsl_to_linear_srgb, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *hsl_in, size_t in_stride, size_t count) {
    if (!hsl_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, hsl_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
                alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
                alwan_map_simd half = alwan_map_simd_set1((alwan_map_lane)(0.5));
                alwan_map_simd third = alwan_map_simd_set1((alwan_map_lane)(1.0 / 3.0));
                for (; i + W <= tile; i += W) {
                    alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd vs = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd vl = alwan_map_simd_load(&c2[i]);
                    /* HSL->RGB */
                    alwan_map_simd q = alwan_map_simd_select(alwan_map_simd_cmplt(vl, half),
                        alwan_map_simd_mul(vl, alwan_map_simd_add(one, vs)),
                        alwan_map_simd_sub(alwan_map_simd_add(vl, vs), alwan_map_simd_mul(vl, vs)));
                    alwan_map_simd p = alwan_map_simd_sub(alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)(2.0)), vl), q);
                    alwan_map_simd or_ = ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(p, q, alwan_map_simd_add(vh, third));
                    alwan_map_simd og = ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(p, q, vh);
                    alwan_map_simd ob = ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(p, q, alwan_map_simd_sub(vh, third));
                    or_ = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vl, or_);
                    og = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vl, og);
                    ob = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vl, ob);
                    /* Apply sRGB EOTF (nonlinear -> linear) */
                    alwan_map_simd_store(&c0[i], ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(or_));
                    alwan_map_simd_store(&c1[i], ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(og));
                    alwan_map_simd_store(&c2[i], ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(ob));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_hsl) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_hsl_to_linear_srgb)(v);
                c0[i] = (alwan_map_lane)r.r; c1[i] = (alwan_map_lane)r.g; c2[i] = (alwan_map_lane)r.b;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Relative Luminance map (3->1)
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__relative_luminance_kernel)(alwan_map_lane *Y_out, alwan_map_lane const *rgb_in,
                                              size_t count, alwan_map_lane kr, alwan_map_lane kg, alwan_map_lane kb,
                                              size_t in_stride, size_t out_stride) {
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd vkr = alwan_map_simd_set1((alwan_map_lane)kr);
                alwan_map_simd vkg = alwan_map_simd_set1((alwan_map_lane)kg);
                alwan_map_simd vkb = alwan_map_simd_set1((alwan_map_lane)kb);
                for (; i + W <= tile; i += W) {
                    alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(vkr, vr, alwan_map_simd_fmadd(vkg, vg, alwan_map_simd_mul(vkb, vb))));
                }
            }
#endif
            for (; i < tile; i++) {
                d0[i] = (alwan_map_lane)(c0[i] * kr + (alwan_map_lane)c1[i] * kg + (alwan_map_lane)c2[i] * kb);
            }
        }
        for (size_t i = 0; i < tile; i++) {
            *(alwan_map_lane *)((char *)Y_out + (processed + i) * out_stride) = (alwan_map_lane)d0[i];
        }
        processed += tile;
    }
}

int ALWAN_MAP_LITCAT(alwan_relative_luminance, _map_interleave)(alwan_map_lane *Y_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count, alwan_luma_standard standard) {
    if (!rgb_in || !Y_out || count == 0) return ALWAN_E_INVALID;
    alwan_f64 kr_s, kg_s, kb_s;
    alwan__get_luma_coeffs((int)standard, &kr_s, &kg_s, &kb_s);
    alwan_map_lane kr = (alwan_map_lane)kr_s, kg = (alwan_map_lane)kg_s, kb = (alwan_map_lane)kb_s;
    ALWAN_MAP_LIT(alwan__relative_luminance_kernel)(Y_out, rgb_in, count, kr, kg, kb, in_stride, out_stride);
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_relative_luminance_space, _map_interleave)(alwan_map_lane *Y_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count, ALWAN_MAP_LIT(alwan_rgb_space_desc) const *space) {
    if (!rgb_in || !Y_out || count == 0 || !space) return ALWAN_E_INVALID;
    if (!space->has_matrices) return ALWAN_E_INVALID;
    alwan_map_lane kr = (alwan_map_lane)space->rgb_to_xyz.m[3];
    alwan_map_lane kg = (alwan_map_lane)space->rgb_to_xyz.m[4];
    alwan_map_lane kb = (alwan_map_lane)space->rgb_to_xyz.m[5];
    ALWAN_MAP_LIT(alwan__relative_luminance_kernel)(Y_out, rgb_in, count, kr, kg, kb, in_stride, out_stride);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * HSP / HSPlog / HSY planar (tiled scalar)
 * ---------------------------------------------------------------- */

#define PLANAR_TILED_SCALAR(name, InT, OutT, core_fn, fi0,fi1,fi2, fo0,fo1,fo2) \
int name(alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2, \
         alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    size_t processed = 0; \
    while (processed < count) { \
        size_t tile = count - processed; \
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS; \
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS]; \
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in0, in1, in2, processed, in_stride, tile); \
        for (size_t i = 0; i < tile; i++) { \
            InT src; src.fi0 = (alwan_map_lane)c0[i]; src.fi1 = (alwan_map_lane)c1[i]; src.fi2 = (alwan_map_lane)c2[i]; \
            OutT dst = core_fn(src); \
            c0[i] = (alwan_map_lane)dst.fo0; c1[i] = (alwan_map_lane)dst.fo1; c2[i] = (alwan_map_lane)dst.fo2; \
        } \
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, processed, out_stride, c0, c1, c2, tile); \
        processed += tile; \
    } \
    return ALWAN_OK; \
}

int ALWAN_MAP_LITCAT(alwan_rgb_to_hsp, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__rgb_to_hsp_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hsp_to_rgb, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__hsp_to_rgb_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_rgb_to_hsplog, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__rgb_to_hsplog_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hsplog_to_rgb, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__hsplog_to_rgb_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_rgb_to_hsy, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__rgb_to_hsy_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hsy_to_rgb, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__hsy_to_rgb_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Linear sRGB <-> HSV planar (SIMD vectorized)
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_linear_srgb_to_hsv, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
                alwan_map_simd sixty = alwan_map_simd_set1((alwan_map_lane)(60.0));
                alwan_map_simd inv360 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 360.0));
                for (; i + W <= tile; i += W) {
                    alwan_map_simd vr = ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd_load(&c0[i]));
                    alwan_map_simd vg = ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd_load(&c1[i]));
                    alwan_map_simd vb = ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd_load(&c2[i]));
                    alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
                    alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);
                    alwan_map_simd delta = alwan_map_simd_sub(mx, mn);
                    alwan_map_simd safe_mx = alwan_map_simd_select(alwan_map_simd_cmpgt(mx, zero), mx, alwan_map_simd_set1((alwan_map_lane)(1.0)));
                    alwan_map_simd os = alwan_map_simd_select(alwan_map_simd_cmpgt(mx, zero), alwan_map_simd_div(delta, safe_mx), zero);
                    alwan_map_simd safe_d = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), delta, alwan_map_simd_set1((alwan_map_lane)(1.0)));
                    alwan_map_simd inv_d = alwan_map_simd_div(alwan_map_simd_set1((alwan_map_lane)(1.0)), safe_d);
                    alwan_map_simd h_r = alwan_map_simd_mul(sixty, alwan_map_simd_mul(alwan_map_simd_sub(vg, vb), inv_d));
                    h_r = alwan_map_simd_select(alwan_map_simd_cmplt(vg, vb), alwan_map_simd_add(h_r, alwan_map_simd_set1((alwan_map_lane)(360.0))), h_r);
                    alwan_map_simd h_g = alwan_map_simd_mul(sixty, alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vb, vr), inv_d), alwan_map_simd_set1((alwan_map_lane)(2.0))));
                    alwan_map_simd h_b = alwan_map_simd_mul(sixty, alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vr, vg), inv_d), alwan_map_simd_set1((alwan_map_lane)(4.0))));
                    alwan_map_simd oh = alwan_map_simd_select(alwan_map_simd_cmpeq(mx, vr), h_r,
                                       alwan_map_simd_select(alwan_map_simd_cmpeq(mx, vg), h_g, h_b));
                    oh = alwan_map_simd_mul(oh, inv360);
                    oh = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), oh, zero);
                    alwan_map_simd_store(&c0[i], oh);
                    alwan_map_simd_store(&c1[i], os);
                    alwan_map_simd_store(&c2[i], mx);
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_hsv) r = ALWAN_MAP_LITV(alwan_linear_srgb_to_hsv)(v);
                c0[i] = (alwan_map_lane)r.h; c1[i] = (alwan_map_lane)r.s; c2[i] = (alwan_map_lane)r.v;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hsv_to_linear_srgb, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
                alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
                alwan_map_simd v360 = alwan_map_simd_set1((alwan_map_lane)(360.0));
                alwan_map_simd v60  = alwan_map_simd_set1((alwan_map_lane)(60.0));
                for (; i + W <= tile; i += W) {
                    alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd vs = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd vv = alwan_map_simd_load(&c2[i]);
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
                    alwan_map_simd or_ = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(4.0))), r0123, r45);
                    alwan_map_simd og = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(4.0))), g0123, p);
                    alwan_map_simd ob = alwan_map_simd_select(alwan_map_simd_cmplt(sf, alwan_map_simd_set1((alwan_map_lane)(4.0))), b0123, b45);
                    or_ = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vv, or_);
                    og = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vv, og);
                    ob = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vv, ob);
                    alwan_map_simd_store(&c0[i], ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(or_));
                    alwan_map_simd_store(&c1[i], ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(og));
                    alwan_map_simd_store(&c2[i], ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(ob));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_hsv) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_hsv_to_linear_srgb)(v);
                c0[i] = (alwan_map_lane)r.r; c1[i] = (alwan_map_lane)r.g; c2[i] = (alwan_map_lane)r.b;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Linear sRGB <-> HSL planar (SIMD vectorized)
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_linear_srgb_to_hsl, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
                alwan_map_simd half = alwan_map_simd_set1((alwan_map_lane)(0.5));
                alwan_map_simd sixty = alwan_map_simd_set1((alwan_map_lane)(60.0));
                alwan_map_simd inv360 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 360.0));
                for (; i + W <= tile; i += W) {
                    alwan_map_simd vr = ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd_load(&c0[i]));
                    alwan_map_simd vg = ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd_load(&c1[i]));
                    alwan_map_simd vb = ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd_load(&c2[i]));
                    alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
                    alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);
                    alwan_map_simd delta = alwan_map_simd_sub(mx, mn);
                    alwan_map_simd ol = alwan_map_simd_mul(half, alwan_map_simd_add(mx, mn));
                    alwan_map_simd safe_d = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), delta, alwan_map_simd_set1((alwan_map_lane)(1.0)));
                    alwan_map_simd inv_d = alwan_map_simd_div(alwan_map_simd_set1((alwan_map_lane)(1.0)), safe_d);
                    alwan_map_simd denom = alwan_map_simd_sub(alwan_map_simd_set1((alwan_map_lane)(1.0)), alwan_map_simd_abs(alwan_map_simd_sub(alwan_map_simd_add(mx, mn), alwan_map_simd_set1((alwan_map_lane)(1.0)))));
                    alwan_map_simd safe_den = alwan_map_simd_select(alwan_map_simd_cmpgt(denom, zero), denom, alwan_map_simd_set1((alwan_map_lane)(1.0)));
                    alwan_map_simd os = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), alwan_map_simd_div(delta, safe_den), zero);
                    alwan_map_simd h_r = alwan_map_simd_mul(sixty, alwan_map_simd_mul(alwan_map_simd_sub(vg, vb), inv_d));
                    h_r = alwan_map_simd_select(alwan_map_simd_cmplt(vg, vb), alwan_map_simd_add(h_r, alwan_map_simd_set1((alwan_map_lane)(360.0))), h_r);
                    alwan_map_simd h_g = alwan_map_simd_mul(sixty, alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vb, vr), inv_d), alwan_map_simd_set1((alwan_map_lane)(2.0))));
                    alwan_map_simd h_b = alwan_map_simd_mul(sixty, alwan_map_simd_add(alwan_map_simd_mul(alwan_map_simd_sub(vr, vg), inv_d), alwan_map_simd_set1((alwan_map_lane)(4.0))));
                    alwan_map_simd oh = alwan_map_simd_select(alwan_map_simd_cmpeq(mx, vr), h_r,
                                       alwan_map_simd_select(alwan_map_simd_cmpeq(mx, vg), h_g, h_b));
                    oh = alwan_map_simd_mul(oh, inv360);
                    oh = alwan_map_simd_select(alwan_map_simd_cmpgt(delta, zero), oh, zero);
                    alwan_map_simd_store(&c0[i], oh);
                    alwan_map_simd_store(&c1[i], os);
                    alwan_map_simd_store(&c2[i], ol);
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_hsl) r = ALWAN_MAP_LITV(alwan_linear_srgb_to_hsl)(v);
                c0[i] = (alwan_map_lane)r.h; c1[i] = (alwan_map_lane)r.s; c2[i] = (alwan_map_lane)r.l;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_hsl_to_linear_srgb, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)(0.0));
                alwan_map_simd one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
                alwan_map_simd half = alwan_map_simd_set1((alwan_map_lane)(0.5));
                alwan_map_simd third = alwan_map_simd_set1((alwan_map_lane)(1.0 / 3.0));
                for (; i + W <= tile; i += W) {
                    alwan_map_simd vh = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd vs = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd vl = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd q = alwan_map_simd_select(alwan_map_simd_cmplt(vl, half),
                        alwan_map_simd_mul(vl, alwan_map_simd_add(one, vs)),
                        alwan_map_simd_sub(alwan_map_simd_add(vl, vs), alwan_map_simd_mul(vl, vs)));
                    alwan_map_simd p = alwan_map_simd_sub(alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)(2.0)), vl), q);
                    alwan_map_simd or_ = ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(p, q, alwan_map_simd_add(vh, third));
                    alwan_map_simd og = ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(p, q, vh);
                    alwan_map_simd ob = ALWAN_MAP_LIT(alwan__hue_to_rgb_simd)(p, q, alwan_map_simd_sub(vh, third));
                    or_ = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vl, or_);
                    og = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vl, og);
                    ob = alwan_map_simd_select(alwan_map_simd_cmple(vs, zero), vl, ob);
                    alwan_map_simd_store(&c0[i], ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(or_));
                    alwan_map_simd_store(&c1[i], ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(og));
                    alwan_map_simd_store(&c2[i], ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(ob));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_hsl) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_hsl_to_linear_srgb)(v);
                c0[i] = (alwan_map_lane)r.r; c1[i] = (alwan_map_lane)r.g; c2[i] = (alwan_map_lane)r.b;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}
