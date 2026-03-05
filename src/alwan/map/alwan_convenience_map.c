/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Convenience Color Model Conversions - True SIMD vectorized
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_convenience_core.h"

/* ----------------------------------------------------------------
 * SIMD helpers for HSV/HSL (only needed in SIMD path)
 * ---------------------------------------------------------------- */

#if ALWAN_SIMD_WIDTH > 1

/* hue_to_rgb helper for HSL -> RGB */
ALWAN_INLINE alwan_simd alwan__hue_to_rgb_simd(alwan_simd p, alwan_simd q, alwan_simd t) {
    alwan_simd zero = alwan_simd_set1(0.0);
    alwan_simd one  = alwan_simd_set1(1.0);
    alwan_simd sixth = alwan_simd_set1(1.0 / 6.0);
    alwan_simd half  = alwan_simd_set1(0.5);
    alwan_simd two3  = alwan_simd_set1(2.0 / 3.0);
    alwan_simd six   = alwan_simd_set1(6.0);

    t = alwan_simd_select(alwan_simd_cmplt(t, zero), alwan_simd_add(t, one), t);
    t = alwan_simd_select(alwan_simd_cmpgt(t, one),  alwan_simd_sub(t, one), t);

    /* result = p + (q - p) * 6 * t   when t < 1/6 */
    alwan_simd qmp = alwan_simd_sub(q, p);
    alwan_simd r1 = alwan_simd_fmadd(qmp, alwan_simd_mul(six, t), p);
    /* result = q                      when t < 1/2 */
    /* result = p + (q - p) * (2/3 - t) * 6  when t < 2/3 */
    alwan_simd r3 = alwan_simd_fmadd(qmp, alwan_simd_mul(six, alwan_simd_sub(two3, t)), p);
    /* result = p                      otherwise */

    /* Build result from inside out (rightmost condition first) */
    alwan_simd result = p;
    result = alwan_simd_select(alwan_simd_cmplt(t, two3), r3, result);
    result = alwan_simd_select(alwan_simd_cmplt(t, half), q,  result);
    result = alwan_simd_select(alwan_simd_cmplt(t, sixth), r1, result);
    return result;
}

#endif /* ALWAN_SIMD_WIDTH > 1 */

/* ----------------------------------------------------------------
 * RGB -> HSV
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hsv_map(alwan_scalar *hsv_out, alwan_scalar const *rgb_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !hsv_out || count == 0) return ALWAN_E_INVALID;

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
            alwan_simd vr = alwan_simd_load(&c0[i]);
            alwan_simd vg = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);

            alwan_simd mx = alwan__simd_max3(vr, vg, vb);
            alwan_simd mn = alwan__simd_min3(vr, vg, vb);
            alwan_simd delta = alwan_simd_sub(mx, mn);
            alwan_simd zero = alwan_simd_set1(0.0);
            alwan_simd sixty = alwan_simd_set1(60.0);
            alwan_simd inv360 = alwan_simd_set1(1.0 / 360.0);

            /* V = max */
            alwan_simd ov = mx;

            /* S = delta > 0 && max > 0 ? delta / max : 0 */
            alwan_simd safe_mx = alwan_simd_select(alwan_simd_cmpgt(mx, zero), mx, alwan_simd_set1(1.0));
            alwan_simd os = alwan_simd_select(alwan_simd_cmpgt(mx, zero),
                                                       alwan_simd_div(delta, safe_mx), zero);

            /* Hue computation */
            alwan_simd safe_delta = alwan_simd_select(alwan_simd_cmpgt(delta, zero), delta, alwan_simd_set1(1.0));
            alwan_simd inv_delta = alwan_simd_div(alwan_simd_set1(1.0), safe_delta);

            /* h_r = 60 * (g - b) / delta, adjusted +360 if g < b */
            alwan_simd h_r = alwan_simd_mul(sixty, alwan_simd_mul(alwan_simd_sub(vg, vb), inv_delta));
            h_r = alwan_simd_select(alwan_simd_cmplt(vg, vb),
                                        alwan_simd_add(h_r, alwan_simd_set1(360.0)), h_r);
            /* h_g = 60 * ((b - r) / delta + 2) */
            alwan_simd h_g = alwan_simd_mul(sixty,
                alwan_simd_add(alwan_simd_mul(alwan_simd_sub(vb, vr), inv_delta),
                                   alwan_simd_set1(2.0)));
            /* h_b = 60 * ((r - g) / delta + 4) */
            alwan_simd h_b = alwan_simd_mul(sixty,
                alwan_simd_add(alwan_simd_mul(alwan_simd_sub(vr, vg), inv_delta),
                                   alwan_simd_set1(4.0)));

            /* Select hue based on which channel is max */
            alwan_simd_mask r_is_max = alwan_simd_cmpeq(mx, vr);
            alwan_simd_mask g_is_max = alwan_simd_cmpeq(mx, vg);
            alwan_simd oh = alwan_simd_select(r_is_max, h_r,
                                alwan_simd_select(g_is_max, h_g, h_b));
            /* Normalize to [0,1] and zero when achromatic */
            oh = alwan_simd_mul(oh, inv360);
            oh = alwan_simd_select(alwan_simd_cmpgt(delta, zero), oh, zero);

            alwan_simd_store(&c0[i], oh);
            alwan_simd_store(&c1[i], os);
            alwan_simd_store(&c2[i], ov);
        }
        for (; i < tile; i++) {
            alwan_rgb v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_hsv r = alwan_rgb_to_hsv_v(v);
            c0[i] = (alwan_simd_lane)r.h; c1[i] = (alwan_simd_lane)r.s; c2[i] = (alwan_simd_lane)r.v;
        }

        alwan__store_tile_aos3(hsv_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)hsv_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_hsv hsv = alwan_rgb_to_hsv_v(rgb);
        out_ptr[0] = hsv.h; out_ptr[1] = hsv.s; out_ptr[2] = hsv.v;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * HSV -> RGB
 * ---------------------------------------------------------------- */

int alwan_hsv_to_rgb_map(alwan_scalar *rgb_out, alwan_scalar const *hsv_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!hsv_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, hsv_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vh = alwan_simd_load(&c0[i]);
            alwan_simd vs = alwan_simd_load(&c1[i]);
            alwan_simd vv = alwan_simd_load(&c2[i]);

            alwan_simd zero = alwan_simd_set1(0.0);
            alwan_simd s360 = alwan_simd_set1(360.0);
            alwan_simd s60  = alwan_simd_set1(60.0);
            alwan_simd one  = alwan_simd_set1(1.0);

            /* h = h * 360 */
            alwan_simd h = alwan_simd_mul(vh, s360);
            /* Normalize to [0,360) */
            h = alwan_simd_sub(h, alwan_simd_mul(alwan_simd_floor(alwan_simd_div(h, s360)), s360));
            h = alwan_simd_select(alwan_simd_cmplt(h, zero), alwan_simd_add(h, s360), h);

            alwan_simd h_sector = alwan_simd_div(h, s60);
            alwan_simd sector_f = alwan_simd_floor(h_sector);
            alwan_simd f = alwan_simd_sub(h_sector, sector_f);

            alwan_simd p = alwan_simd_mul(vv, alwan_simd_sub(one, vs));
            alwan_simd q = alwan_simd_mul(vv, alwan_simd_sub(one, alwan_simd_mul(vs, f)));
            alwan_simd t = alwan_simd_mul(vv,
                alwan_simd_sub(one, alwan_simd_mul(vs, alwan_simd_sub(one, f))));

            /* 6-sector select tree (same as scalar core) */
            alwan_simd r_01 = alwan_simd_select(alwan_simd_cmplt(sector_f, one), vv, q);
            alwan_simd g_01 = alwan_simd_select(alwan_simd_cmplt(sector_f, one), t, vv);
            alwan_simd b_01 = p;

            alwan_simd r_23 = p;
            alwan_simd g_23 = alwan_simd_select(alwan_simd_cmplt(sector_f, alwan_simd_set1(3.0)), vv, q);
            alwan_simd b_23 = alwan_simd_select(alwan_simd_cmplt(sector_f, alwan_simd_set1(3.0)), t, vv);

            alwan_simd r_45 = alwan_simd_select(alwan_simd_cmplt(sector_f, alwan_simd_set1(5.0)), t, vv);
            alwan_simd g_45 = p;
            alwan_simd b_45 = alwan_simd_select(alwan_simd_cmplt(sector_f, alwan_simd_set1(5.0)), vv, q);

            alwan_simd two = alwan_simd_set1(2.0);
            alwan_simd four = alwan_simd_set1(4.0);
            alwan_simd_mask lt2 = alwan_simd_cmplt(sector_f, two);
            alwan_simd r_0123 = alwan_simd_select(lt2, r_01, r_23);
            alwan_simd g_0123 = alwan_simd_select(lt2, g_01, g_23);
            alwan_simd b_0123 = alwan_simd_select(lt2, b_01, b_23);

            alwan_simd_mask lt4 = alwan_simd_cmplt(sector_f, four);
            alwan_simd or = alwan_simd_select(lt4, r_0123, r_45);
            alwan_simd og = alwan_simd_select(lt4, g_0123, g_45);
            alwan_simd ob = alwan_simd_select(lt4, b_0123, b_45);

            /* Achromatic: s <= 0 -> (v,v,v) */
            alwan_simd_mask achro = alwan_simd_cmple(vs, zero);
            or = alwan_simd_select(achro, vv, or);
            og = alwan_simd_select(achro, vv, og);
            ob = alwan_simd_select(achro, vv, ob);

            alwan_simd_store(&c0[i], or);
            alwan_simd_store(&c1[i], og);
            alwan_simd_store(&c2[i], ob);
        }
        for (; i < tile; i++) {
            alwan_hsv v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb r = alwan_hsv_to_rgb_v(v);
            c0[i] = (alwan_simd_lane)r.r; c1[i] = (alwan_simd_lane)r.g; c2[i] = (alwan_simd_lane)r.b;
        }

        alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)hsv_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_hsv hsv = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb = alwan_hsv_to_rgb_v(hsv);
        out_ptr[0] = rgb.r; out_ptr[1] = rgb.g; out_ptr[2] = rgb.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB -> HSL
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hsl_map(alwan_scalar *hsl_out, alwan_scalar const *rgb_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !hsl_out || count == 0) return ALWAN_E_INVALID;

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
            alwan_simd vr = alwan_simd_load(&c0[i]);
            alwan_simd vg = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);

            alwan_simd mx = alwan__simd_max3(vr, vg, vb);
            alwan_simd mn = alwan__simd_min3(vr, vg, vb);
            alwan_simd delta = alwan_simd_sub(mx, mn);
            alwan_simd zero = alwan_simd_set1(0.0);
            alwan_simd half = alwan_simd_set1(0.5);
            alwan_simd sixty = alwan_simd_set1(60.0);
            alwan_simd inv360 = alwan_simd_set1(1.0 / 360.0);
            alwan_simd two = alwan_simd_set1(2.0);

            /* L = (max + min) / 2 */
            alwan_simd ol = alwan_simd_mul(alwan_simd_add(mx, mn), half);

            /* S: when delta > 0: l < 0.5 ? delta/(max+min) : delta/(2-max-min) */
            alwan_simd_mask has_delta = alwan_simd_cmpgt(delta, zero);
            alwan_simd sum = alwan_simd_add(mx, mn);
            alwan_simd s_light = alwan_simd_div(delta, sum);
            alwan_simd s_dark  = alwan_simd_div(delta, alwan_simd_sub(two, sum));
            alwan_simd os = alwan_simd_select(has_delta,
                alwan_simd_select(alwan_simd_cmplt(ol, half), s_light, s_dark), zero);

            /* Hue (same as HSV) */
            alwan_simd safe_delta = alwan_simd_select(has_delta, delta, alwan_simd_set1(1.0));
            alwan_simd inv_delta = alwan_simd_div(alwan_simd_set1(1.0), safe_delta);

            alwan_simd h_r = alwan_simd_mul(sixty, alwan_simd_mul(alwan_simd_sub(vg, vb), inv_delta));
            h_r = alwan_simd_select(alwan_simd_cmplt(vg, vb),
                                        alwan_simd_add(h_r, alwan_simd_set1(360.0)), h_r);
            alwan_simd h_g = alwan_simd_mul(sixty,
                alwan_simd_add(alwan_simd_mul(alwan_simd_sub(vb, vr), inv_delta), two));
            alwan_simd h_b = alwan_simd_mul(sixty,
                alwan_simd_add(alwan_simd_mul(alwan_simd_sub(vr, vg), inv_delta),
                                   alwan_simd_set1(4.0)));

            alwan_simd_mask r_is_max = alwan_simd_cmpeq(mx, vr);
            alwan_simd_mask g_is_max = alwan_simd_cmpeq(mx, vg);
            alwan_simd oh = alwan_simd_select(r_is_max, h_r,
                                alwan_simd_select(g_is_max, h_g, h_b));
            oh = alwan_simd_mul(oh, inv360);
            oh = alwan_simd_select(has_delta, oh, zero);

            alwan_simd_store(&c0[i], oh);
            alwan_simd_store(&c1[i], os);
            alwan_simd_store(&c2[i], ol);
        }
        for (; i < tile; i++) {
            alwan_rgb v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_hsl r = alwan_rgb_to_hsl_v(v);
            c0[i] = (alwan_simd_lane)r.h; c1[i] = (alwan_simd_lane)r.s; c2[i] = (alwan_simd_lane)r.l;
        }

        alwan__store_tile_aos3(hsl_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)hsl_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_hsl hsl = alwan_rgb_to_hsl_v(rgb);
        out_ptr[0] = hsl.h; out_ptr[1] = hsl.s; out_ptr[2] = hsl.l;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * HSL -> RGB
 * ---------------------------------------------------------------- */

int alwan_hsl_to_rgb_map(alwan_scalar *rgb_out, alwan_scalar const *hsl_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!hsl_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, hsl_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vh = alwan_simd_load(&c0[i]);
            alwan_simd vs = alwan_simd_load(&c1[i]);
            alwan_simd vl = alwan_simd_load(&c2[i]);

            alwan_simd zero = alwan_simd_set1(0.0);
            alwan_simd half = alwan_simd_set1(0.5);
            alwan_simd one  = alwan_simd_set1(1.0);
            alwan_simd two  = alwan_simd_set1(2.0);
            alwan_simd third = alwan_simd_set1(1.0 / 3.0);

            /* q = l < 0.5 ? l*(1+s) : l+s-l*s */
            alwan_simd q_light = alwan_simd_mul(vl, alwan_simd_add(one, vs));
            alwan_simd q_dark  = alwan_simd_sub(alwan_simd_add(vl, vs), alwan_simd_mul(vl, vs));
            alwan_simd q = alwan_simd_select(alwan_simd_cmplt(vl, half), q_light, q_dark);
            alwan_simd p = alwan_simd_sub(alwan_simd_mul(two, vl), q);

            alwan_simd or = alwan__hue_to_rgb_simd(p, q, alwan_simd_add(vh, third));
            alwan_simd og = alwan__hue_to_rgb_simd(p, q, vh);
            alwan_simd ob = alwan__hue_to_rgb_simd(p, q, alwan_simd_sub(vh, third));

            /* Achromatic: s <= 0 -> (l,l,l) */
            alwan_simd_mask achro = alwan_simd_cmple(vs, zero);
            or = alwan_simd_select(achro, vl, or);
            og = alwan_simd_select(achro, vl, og);
            ob = alwan_simd_select(achro, vl, ob);

            alwan_simd_store(&c0[i], or);
            alwan_simd_store(&c1[i], og);
            alwan_simd_store(&c2[i], ob);
        }
        for (; i < tile; i++) {
            alwan_hsl v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb r = alwan_hsl_to_rgb_v(v);
            c0[i] = (alwan_simd_lane)r.r; c1[i] = (alwan_simd_lane)r.g; c2[i] = (alwan_simd_lane)r.b;
        }

        alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)hsl_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_hsl hsl = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb = alwan_hsl_to_rgb_v(hsl);
        out_ptr[0] = rgb.r; out_ptr[1] = rgb.g; out_ptr[2] = rgb.b;
    }
#endif
    return ALWAN_OK;
}
