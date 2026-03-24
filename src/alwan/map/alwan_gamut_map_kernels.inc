/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Gamut kernels (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * Gamut clip kernel: clamp each channel to [0,1]
 * ---------------------------------------------------------------- */

void ALWAN_MAP_LIT(alwan__gamut_clip_kernel)(alwan_map_lane *c0, alwan_map_lane *c1, alwan_map_lane *c2,
                              size_t n) {
    size_t j = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd const zero = alwan_map_simd_set1(0.0);
        alwan_map_simd const one  = alwan_map_simd_set1(1.0);
        for (; j + W <= n; j += W) {
            alwan_map_simd_store(&c0[j], alwan_map_simd_max(alwan_map_simd_min(alwan_map_simd_load(&c0[j]), one), zero));
            alwan_map_simd_store(&c1[j], alwan_map_simd_max(alwan_map_simd_min(alwan_map_simd_load(&c1[j]), one), zero));
            alwan_map_simd_store(&c2[j], alwan_map_simd_max(alwan_map_simd_min(alwan_map_simd_load(&c2[j]), one), zero));
        }
    }
#endif
    for (; j < n; j++) {
        alwan_map_lane v;
        v = c0[j]; c0[j] = (v < (alwan_map_lane)0.0 ? (alwan_map_lane)0.0 : (v > (alwan_map_lane)1.0 ? (alwan_map_lane)1.0 : v));
        v = c1[j]; c1[j] = (v < (alwan_map_lane)0.0 ? (alwan_map_lane)0.0 : (v > (alwan_map_lane)1.0 ? (alwan_map_lane)1.0 : v));
        v = c2[j]; c2[j] = (v < (alwan_map_lane)0.0 ? (alwan_map_lane)0.0 : (v > (alwan_map_lane)1.0 ? (alwan_map_lane)1.0 : v));
    }
}

/* ----------------------------------------------------------------
 * CSS gamut map kernel: binary search on OKLCh chroma
 * ---------------------------------------------------------------- */

/* CSS Oklab matrices (CSS_SRGB_TO_LMS, CSS_LMS_TO_LAB, CSS_LAB_TO_LMS,
 * CSS_LMS_TO_SRGB) are defined in alwan_gamut_core.h via CSV inclusion. */

#if ALWAN_MAP_SIMD_WIDTH > 1
/* SIMD: linear sRGB -> Oklab (3 channels, W pixels at a time) */
ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__css_srgb_to_oklab_simd)(alwan_map_simd *oL, alwan_map_simd *oa, alwan_map_simd *ob,
                                           alwan_map_simd vr, alwan_map_simd vg, alwan_map_simd vb) {
    alwan_map_simd l, m, s;
    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&l, &m, &s, &ALWAN_MAP_LIT(CSS_SRGB_TO_LMS), vr, vg, vb);
    l = alwan_map_simd_cbrt_fast(l);
    m = alwan_map_simd_cbrt_fast(m);
    s = alwan_map_simd_cbrt_fast(s);
    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(oL, oa, ob, &ALWAN_MAP_LIT(CSS_LMS_TO_LAB), l, m, s);
}

/* SIMD: Oklab -> linear sRGB (3 channels, W pixels at a time) */
ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__css_oklab_to_srgb_simd)(alwan_map_simd *or_, alwan_map_simd *og, alwan_map_simd *ob,
                                           alwan_map_simd vL, alwan_map_simd va, alwan_map_simd vb) {
    alwan_map_simd lp, mp, sp;
    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lp, &mp, &sp, &ALWAN_MAP_LIT(CSS_LAB_TO_LMS), vL, va, vb);
    alwan_map_simd l = alwan_map_simd_mul(lp, alwan_map_simd_mul(lp, lp));
    alwan_map_simd m = alwan_map_simd_mul(mp, alwan_map_simd_mul(mp, mp));
    alwan_map_simd s = alwan_map_simd_mul(sp, alwan_map_simd_mul(sp, sp));
    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(or_, og, ob, &ALWAN_MAP_LIT(CSS_LMS_TO_SRGB), l, m, s);
}
#endif /* ALWAN_MAP_SIMD_WIDTH > 1 */

/* ----------------------------------------------------------------
 * Gamut clip map wrappers (interleave + planar)
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_gamut_clip, _map_interleave)(
    alwan_map_lane *rgb_out, alwan_map_lane const *rgb_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__gamut_clip_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_gamut_clip, _map_planar)(
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
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__gamut_clip_kernel)(c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CSS gamut map wrappers (interleave + planar)
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_css_gamut_map, _map_interleave)(
    alwan_map_lane *rgb_out, alwan_map_lane const *rgb_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__css_gamut_map_kernel)(c0, c1, c2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_css_gamut_map, _map_planar)(
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
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__css_gamut_map_kernel)(c0, c1, c2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* CSS gamut map kernel: processes n pixels in planar SoA layout */
void ALWAN_MAP_LIT(alwan__css_gamut_map_kernel)(alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
                                 alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
                                 size_t n) {
    size_t i = 0;

#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd const zero = alwan_map_simd_set1((alwan_map_lane)0.0);
        alwan_map_simd const one  = alwan_map_simd_set1((alwan_map_lane)1.0);
        alwan_map_simd const half = alwan_map_simd_set1((alwan_map_lane)0.5);
        alwan_map_simd const neg_eps = alwan_map_simd_set1((alwan_map_lane)(-ALWAN_EPSILON));
        alwan_map_simd const one_eps = alwan_map_simd_set1((alwan_map_lane)(1.0 + ALWAN_EPSILON));
        alwan_map_simd const jnd  = alwan_map_simd_set1((alwan_map_lane)0.02);
        alwan_map_simd const eps_v = alwan_map_simd_set1((alwan_map_lane)ALWAN_EPSILON);
        alwan_map_simd const tiny = alwan_map_simd_set1((alwan_map_lane)1e-12);

        for (; i + W <= n; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&i0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&i1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&i2[i]);

            /* Check which pixels are already in gamut */
            alwan_map_simd_mask in_gamut = alwan_map_simd_cmpge(vr, neg_eps);
            in_gamut = alwan_map_simd_select(in_gamut, alwan_map_simd_cmple(vr, one_eps), zero);
            in_gamut = alwan_map_simd_select(in_gamut, alwan_map_simd_cmple(vg, one_eps), zero);
            in_gamut = alwan_map_simd_select(in_gamut, alwan_map_simd_cmpge(vg, neg_eps), zero);
            in_gamut = alwan_map_simd_select(in_gamut, alwan_map_simd_cmple(vb, one_eps), zero);
            in_gamut = alwan_map_simd_select(in_gamut, alwan_map_simd_cmpge(vb, neg_eps), zero);

            /* Early-out: all in gamut */
            if (alwan_map_simd_mask_all_set(in_gamut)) {
                alwan_map_simd_store(&o0[i], vr);
                alwan_map_simd_store(&o1[i], vg);
                alwan_map_simd_store(&o2[i], vb);
                continue;
            }

            /* RGB -> Oklab */
            alwan_map_simd okL, oka, okb;
            ALWAN_MAP_LIT(alwan__css_srgb_to_oklab_simd)(&okL, &oka, &okb, vr, vg, vb);

            /* Oklab -> OKLCh: C = sqrt(a^2+b^2), h = atan2(b,a) */
            alwan_map_simd C = alwan_map_simd_sqrt(alwan_map_simd_fmadd(oka, oka, alwan_map_simd_mul(okb, okb)));
            alwan_map_simd h = alwan_map_simd_atan2(okb, oka);

            /* Clamp L: if L >= 1 -> white, if L <= 0 -> black */
            alwan_map_simd_mask is_white = alwan_map_simd_cmpge(okL, one);
            alwan_map_simd_mask is_black = alwan_map_simd_cmple(okL, zero);

            /* Result accumulators -- start with original RGB for in-gamut pixels */
            alwan_map_simd res_r = vr, res_g = vg, res_b = vb;

            /* For white pixels: result = (1,1,1) */
            res_r = alwan_map_simd_select(is_white, one, res_r);
            res_g = alwan_map_simd_select(is_white, one, res_g);
            res_b = alwan_map_simd_select(is_white, one, res_b);

            /* For black pixels: result = (0,0,0) */
            res_r = alwan_map_simd_select(is_black, zero, res_r);
            res_g = alwan_map_simd_select(is_black, zero, res_g);
            res_b = alwan_map_simd_select(is_black, zero, res_b);

            /* Binary search for remaining pixels */
            alwan_map_simd needs_f = alwan_map_simd_select(in_gamut, zero, one);
            needs_f = alwan_map_simd_select(is_white, zero, needs_f);
            needs_f = alwan_map_simd_select(is_black, zero, needs_f);
            alwan_map_simd_mask needs_search = alwan_map_simd_cmpgt(needs_f, zero);

            if (!alwan_map_simd_mask_all_set(alwan_map_simd_cmple(needs_f, zero))) {
                /* At least one lane needs binary search */
                alwan_map_simd lo = zero;
                alwan_map_simd hi = C;
                alwan_map_simd cos_h = alwan_map_simd_cos(h);
                alwan_map_simd sin_h = alwan_map_simd_sin(h);
                int iter;

                for (iter = 0; iter < 30; iter++) {
                    alwan_map_simd trial_C = alwan_map_simd_mul(alwan_map_simd_add(lo, hi), half);

                    /* OKLCh -> Oklab: a = C*cos(h), b = C*sin(h) */
                    alwan_map_simd trial_a = alwan_map_simd_mul(trial_C, cos_h);
                    alwan_map_simd trial_b = alwan_map_simd_mul(trial_C, sin_h);

                    /* Oklab -> linear sRGB */
                    alwan_map_simd tr, tg, tb;
                    ALWAN_MAP_LIT(alwan__css_oklab_to_srgb_simd)(&tr, &tg, &tb, okL, trial_a, trial_b);

                    /* Clip to [0,1] */
                    alwan_map_simd cr = alwan_map_simd_max(alwan_map_simd_min(tr, one), zero);
                    alwan_map_simd cg = alwan_map_simd_max(alwan_map_simd_min(tg, one), zero);
                    alwan_map_simd cb = alwan_map_simd_max(alwan_map_simd_min(tb, one), zero);

                    /* Clipped -> Oklab */
                    alwan_map_simd cL, ca, cb2;
                    ALWAN_MAP_LIT(alwan__css_srgb_to_oklab_simd)(&cL, &ca, &cb2, cr, cg, cb);

                    /* deltaE_OK = sqrt((L-cL)^2 + (a-ca)^2 + (b-cb2)^2) */
                    alwan_map_simd dL = alwan_map_simd_sub(okL, cL);
                    alwan_map_simd da = alwan_map_simd_sub(trial_a, ca);
                    alwan_map_simd db = alwan_map_simd_sub(trial_b, cb2);
                    alwan_map_simd de = alwan_map_simd_sqrt(alwan_map_simd_fmadd(dL, dL,
                                    alwan_map_simd_fmadd(da, da, alwan_map_simd_mul(db, db))));

                    /* Check in-gamut for trial_rgb (not clipped) */
                    alwan_map_simd_mask trial_in = alwan_map_simd_cmpge(tr, neg_eps);
                    trial_in = alwan_map_simd_select(trial_in, alwan_map_simd_cmple(tr, one_eps), zero);
                    trial_in = alwan_map_simd_select(trial_in, alwan_map_simd_cmpge(tg, neg_eps), zero);
                    trial_in = alwan_map_simd_select(trial_in, alwan_map_simd_cmple(tg, one_eps), zero);
                    trial_in = alwan_map_simd_select(trial_in, alwan_map_simd_cmpge(tb, neg_eps), zero);
                    trial_in = alwan_map_simd_select(trial_in, alwan_map_simd_cmple(tb, one_eps), zero);

                    /* de_close = (de - jnd) < eps */
                    alwan_map_simd_mask de_close = alwan_map_simd_cmplt(alwan_map_simd_sub(de, jnd), eps_v);

                    /* hi = trial_C unless converged (de_close AND in_gamut) */
                    alwan_map_simd new_hi = trial_C;
                    {
                        alwan_map_simd converged_f = alwan_map_simd_select(de_close, one, zero);
                        converged_f = alwan_map_simd_select(trial_in, converged_f, zero);
                        alwan_map_simd_mask converged = alwan_map_simd_cmpgt(converged_f, zero);
                        new_hi = alwan_map_simd_select(converged, hi, new_hi);
                    }
                    /* Only update active (needs_search) lanes */
                    hi = alwan_map_simd_select(needs_search, new_hi, hi);

                    /* Convergence check: hi - lo < 1e-12 */
                    alwan_map_simd_mask conv = alwan_map_simd_cmplt(alwan_map_simd_sub(hi, lo), tiny);
                    needs_f = alwan_map_simd_select(conv, zero, needs_f);
                    needs_search = alwan_map_simd_cmpgt(needs_f, zero);

                    if (alwan_map_simd_mask_all_set(alwan_map_simd_cmple(needs_f, zero)))
                        break;
                }

                /* Final reconstruction: OKLCh -> Oklab -> linear sRGB -> clip */
                {
                    alwan_map_simd final_C = alwan_map_simd_mul(alwan_map_simd_add(lo, hi), half);
                    alwan_map_simd final_a = alwan_map_simd_mul(final_C, cos_h);
                    alwan_map_simd final_b = alwan_map_simd_mul(final_C, sin_h);
                    alwan_map_simd fr, fg, fb;
                    ALWAN_MAP_LIT(alwan__css_oklab_to_srgb_simd)(&fr, &fg, &fb, okL, final_a, final_b);
                    fr = alwan_map_simd_max(alwan_map_simd_min(fr, one), zero);
                    fg = alwan_map_simd_max(alwan_map_simd_min(fg, one), zero);
                    fb = alwan_map_simd_max(alwan_map_simd_min(fb, one), zero);

                    /* Merge result for lanes that were searching */
                    alwan_map_simd was_searching = alwan_map_simd_select(in_gamut, zero, one);
                    was_searching = alwan_map_simd_select(is_white, zero, was_searching);
                    was_searching = alwan_map_simd_select(is_black, zero, was_searching);
                    alwan_map_simd_mask was_mask = alwan_map_simd_cmpgt(was_searching, zero);
                    res_r = alwan_map_simd_select(was_mask, fr, res_r);
                    res_g = alwan_map_simd_select(was_mask, fg, res_g);
                    res_b = alwan_map_simd_select(was_mask, fb, res_b);
                }
            }

            alwan_map_simd_store(&o0[i], res_r);
            alwan_map_simd_store(&o1[i], res_g);
            alwan_map_simd_store(&o2[i], res_b);
        }
    }
#endif /* ALWAN_MAP_SIMD_WIDTH > 1 */

    /* Scalar tail */
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_vec3) origin = {{(alwan_map_lane)i0[i], (alwan_map_lane)i1[i], (alwan_map_lane)i2[i]}};
        ALWAN_MAP_LIT(alwan_vec3) mapped = ALWAN_MAP_LITV(gamut_css_map)(origin);
        o0[i] = (alwan_map_lane)mapped.v[0];
        o1[i] = (alwan_map_lane)mapped.v[1];
        o2[i] = (alwan_map_lane)mapped.v[2];
    }
}
