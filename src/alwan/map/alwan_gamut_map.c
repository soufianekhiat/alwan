/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Gamut - SIMD vectorized gamut mapping kernels
 * Clip (clamp) and CSS Color Level 4 gamut mapping.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_gamut_core.h"

/* ----------------------------------------------------------------
 * Gamut clip kernel: clamp each channel to [0,1]
 * ---------------------------------------------------------------- */

void alwan__gamut_clip_kernel(alwan_simd_lane *c0, alwan_simd_lane *c1, alwan_simd_lane *c2,
                              size_t n) {
    size_t j = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd const zero = alwan_simd_set1(0.0);
        alwan_simd const one  = alwan_simd_set1(1.0);
        for (; j + W <= n; j += W) {
            alwan_simd_store(&c0[j], alwan_simd_max(alwan_simd_min(alwan_simd_load(&c0[j]), one), zero));
            alwan_simd_store(&c1[j], alwan_simd_max(alwan_simd_min(alwan_simd_load(&c1[j]), one), zero));
            alwan_simd_store(&c2[j], alwan_simd_max(alwan_simd_min(alwan_simd_load(&c2[j]), one), zero));
        }
    }
#endif
    for (; j < n; j++) {
        alwan_scalar v;
        v = (alwan_scalar)c0[j]; c0[j] = (alwan_simd_lane)(v < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (v > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : v));
        v = (alwan_scalar)c1[j]; c1[j] = (alwan_simd_lane)(v < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (v > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : v));
        v = (alwan_scalar)c2[j]; c2[j] = (alwan_simd_lane)(v < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (v > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : v));
    }
}

/* ----------------------------------------------------------------
 * CSS gamut map kernel: binary search on OKLCh chroma
 * ---------------------------------------------------------------- */

/* CSS Oklab matrices (CSS_SRGB_TO_LMS, CSS_LMS_TO_LAB, CSS_LAB_TO_LMS,
 * CSS_LMS_TO_SRGB) are defined in alwan_gamut_core.h via CSV inclusion. */

#if ALWAN_SIMD_WIDTH > 1
/* SIMD: linear sRGB -> Oklab (3 channels, W pixels at a time) */
static void alwan__css_srgb_to_oklab_simd(alwan_simd *oL, alwan_simd *oa, alwan_simd *ob,
                                           alwan_simd vr, alwan_simd vg, alwan_simd vb) {
    alwan_simd l, m, s;
    alwan__mat3_mul_simd(&l, &m, &s, &CSS_SRGB_TO_LMS, vr, vg, vb);
    l = alwan_simd_cbrt_fast(l);
    m = alwan_simd_cbrt_fast(m);
    s = alwan_simd_cbrt_fast(s);
    alwan__mat3_mul_simd(oL, oa, ob, &CSS_LMS_TO_LAB, l, m, s);
}

/* SIMD: Oklab -> linear sRGB (3 channels, W pixels at a time) */
static void alwan__css_oklab_to_srgb_simd(alwan_simd *or_, alwan_simd *og, alwan_simd *ob,
                                           alwan_simd vL, alwan_simd va, alwan_simd vb) {
    alwan_simd lp, mp, sp;
    alwan__mat3_mul_simd(&lp, &mp, &sp, &CSS_LAB_TO_LMS, vL, va, vb);
    alwan_simd l = alwan_simd_mul(lp, alwan_simd_mul(lp, lp));
    alwan_simd m = alwan_simd_mul(mp, alwan_simd_mul(mp, mp));
    alwan_simd s = alwan_simd_mul(sp, alwan_simd_mul(sp, sp));
    alwan__mat3_mul_simd(or_, og, ob, &CSS_LMS_TO_SRGB, l, m, s);
}
#endif /* ALWAN_SIMD_WIDTH > 1 */

/* CSS gamut map kernel: processes n pixels in planar SoA layout */
void alwan__css_gamut_map_kernel(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                 alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                 size_t n) {
    size_t i = 0;

#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd const zero = alwan_simd_set1((alwan_simd_lane)0.0);
        alwan_simd const one  = alwan_simd_set1((alwan_simd_lane)1.0);
        alwan_simd const half = alwan_simd_set1((alwan_simd_lane)0.5);
        alwan_simd const neg_eps = alwan_simd_set1((alwan_simd_lane)(-ALWAN_EPSILON));
        alwan_simd const one_eps = alwan_simd_set1((alwan_simd_lane)(1.0 + ALWAN_EPSILON));
        alwan_simd const jnd  = alwan_simd_set1((alwan_simd_lane)0.02);
        alwan_simd const eps_v = alwan_simd_set1((alwan_simd_lane)ALWAN_EPSILON);
        alwan_simd const tiny = alwan_simd_set1((alwan_simd_lane)1e-12);

        for (; i + W <= n; i += W) {
            alwan_simd vr = alwan_simd_load(&i0[i]);
            alwan_simd vg = alwan_simd_load(&i1[i]);
            alwan_simd vb = alwan_simd_load(&i2[i]);

            /* Check which pixels are already in gamut */
            alwan_simd_mask in_gamut = alwan_simd_cmpge(vr, neg_eps);
            in_gamut = alwan_simd_select(in_gamut, alwan_simd_cmple(vr, one_eps), zero);
            in_gamut = alwan_simd_select(in_gamut, alwan_simd_cmple(vg, one_eps), zero);
            in_gamut = alwan_simd_select(in_gamut, alwan_simd_cmpge(vg, neg_eps), zero);
            in_gamut = alwan_simd_select(in_gamut, alwan_simd_cmple(vb, one_eps), zero);
            in_gamut = alwan_simd_select(in_gamut, alwan_simd_cmpge(vb, neg_eps), zero);

            /* Early-out: all in gamut */
            if (alwan_simd_mask_all_set(in_gamut)) {
                alwan_simd_store(&o0[i], vr);
                alwan_simd_store(&o1[i], vg);
                alwan_simd_store(&o2[i], vb);
                continue;
            }

            /* RGB -> Oklab */
            alwan_simd okL, oka, okb;
            alwan__css_srgb_to_oklab_simd(&okL, &oka, &okb, vr, vg, vb);

            /* Oklab -> OKLCh: C = sqrt(a^2+b^2), h = atan2(b,a) */
            alwan_simd C = alwan_simd_sqrt(alwan_simd_fmadd(oka, oka, alwan_simd_mul(okb, okb)));
            alwan_simd h = alwan_simd_atan2(okb, oka);

            /* Clamp L: if L >= 1 -> white, if L <= 0 -> black */
            alwan_simd_mask is_white = alwan_simd_cmpge(okL, one);
            alwan_simd_mask is_black = alwan_simd_cmple(okL, zero);

            /* Result accumulators — start with original RGB for in-gamut pixels */
            alwan_simd res_r = vr, res_g = vg, res_b = vb;

            /* For white pixels: result = (1,1,1) */
            res_r = alwan_simd_select(is_white, one, res_r);
            res_g = alwan_simd_select(is_white, one, res_g);
            res_b = alwan_simd_select(is_white, one, res_b);

            /* For black pixels: result = (0,0,0) */
            res_r = alwan_simd_select(is_black, zero, res_r);
            res_g = alwan_simd_select(is_black, zero, res_g);
            res_b = alwan_simd_select(is_black, zero, res_b);

            /* Binary search for remaining pixels */
            alwan_simd needs_f = alwan_simd_select(in_gamut, zero, one);
            needs_f = alwan_simd_select(is_white, zero, needs_f);
            needs_f = alwan_simd_select(is_black, zero, needs_f);
            alwan_simd_mask needs_search = alwan_simd_cmpgt(needs_f, zero);

            if (!alwan_simd_mask_all_set(alwan_simd_cmple(needs_f, zero))) {
                /* At least one lane needs binary search */
                alwan_simd lo = zero;
                alwan_simd hi = C;
                alwan_simd cos_h = alwan_simd_cos(h);
                alwan_simd sin_h = alwan_simd_sin(h);
                int iter;

                for (iter = 0; iter < 30; iter++) {
                    alwan_simd trial_C = alwan_simd_mul(alwan_simd_add(lo, hi), half);

                    /* OKLCh -> Oklab: a = C*cos(h), b = C*sin(h) */
                    alwan_simd trial_a = alwan_simd_mul(trial_C, cos_h);
                    alwan_simd trial_b = alwan_simd_mul(trial_C, sin_h);

                    /* Oklab -> linear sRGB */
                    alwan_simd tr, tg, tb;
                    alwan__css_oklab_to_srgb_simd(&tr, &tg, &tb, okL, trial_a, trial_b);

                    /* Clip to [0,1] */
                    alwan_simd cr = alwan_simd_max(alwan_simd_min(tr, one), zero);
                    alwan_simd cg = alwan_simd_max(alwan_simd_min(tg, one), zero);
                    alwan_simd cb = alwan_simd_max(alwan_simd_min(tb, one), zero);

                    /* Clipped -> Oklab */
                    alwan_simd cL, ca, cb2;
                    alwan__css_srgb_to_oklab_simd(&cL, &ca, &cb2, cr, cg, cb);

                    /* deltaE_OK = sqrt((L-cL)^2 + (a-ca)^2 + (b-cb2)^2) */
                    alwan_simd dL = alwan_simd_sub(okL, cL);
                    alwan_simd da = alwan_simd_sub(trial_a, ca);
                    alwan_simd db = alwan_simd_sub(trial_b, cb2);
                    alwan_simd de = alwan_simd_sqrt(alwan_simd_fmadd(dL, dL,
                                    alwan_simd_fmadd(da, da, alwan_simd_mul(db, db))));

                    /* Check in-gamut for trial_rgb (not clipped) */
                    alwan_simd_mask trial_in = alwan_simd_cmpge(tr, neg_eps);
                    trial_in = alwan_simd_select(trial_in, alwan_simd_cmple(tr, one_eps), zero);
                    trial_in = alwan_simd_select(trial_in, alwan_simd_cmpge(tg, neg_eps), zero);
                    trial_in = alwan_simd_select(trial_in, alwan_simd_cmple(tg, one_eps), zero);
                    trial_in = alwan_simd_select(trial_in, alwan_simd_cmpge(tb, neg_eps), zero);
                    trial_in = alwan_simd_select(trial_in, alwan_simd_cmple(tb, one_eps), zero);

                    /* de_close = (de - jnd) < eps */
                    alwan_simd_mask de_close = alwan_simd_cmplt(alwan_simd_sub(de, jnd), eps_v);

                    /* hi = trial_C unless converged (de_close AND in_gamut) */
                    alwan_simd new_hi = trial_C;
                    {
                        alwan_simd converged_f = alwan_simd_select(de_close, one, zero);
                        converged_f = alwan_simd_select(trial_in, converged_f, zero);
                        alwan_simd_mask converged = alwan_simd_cmpgt(converged_f, zero);
                        new_hi = alwan_simd_select(converged, hi, new_hi);
                    }
                    /* Only update active (needs_search) lanes */
                    hi = alwan_simd_select(needs_search, new_hi, hi);

                    /* Convergence check: hi - lo < 1e-12 */
                    alwan_simd_mask conv = alwan_simd_cmplt(alwan_simd_sub(hi, lo), tiny);
                    needs_f = alwan_simd_select(conv, zero, needs_f);
                    needs_search = alwan_simd_cmpgt(needs_f, zero);

                    if (alwan_simd_mask_all_set(alwan_simd_cmple(needs_f, zero)))
                        break;
                }

                /* Final reconstruction: OKLCh -> Oklab -> linear sRGB -> clip */
                {
                    alwan_simd final_C = alwan_simd_mul(alwan_simd_add(lo, hi), half);
                    alwan_simd final_a = alwan_simd_mul(final_C, cos_h);
                    alwan_simd final_b = alwan_simd_mul(final_C, sin_h);
                    alwan_simd fr, fg, fb;
                    alwan__css_oklab_to_srgb_simd(&fr, &fg, &fb, okL, final_a, final_b);
                    fr = alwan_simd_max(alwan_simd_min(fr, one), zero);
                    fg = alwan_simd_max(alwan_simd_min(fg, one), zero);
                    fb = alwan_simd_max(alwan_simd_min(fb, one), zero);

                    /* Merge result for lanes that were searching */
                    alwan_simd was_searching = alwan_simd_select(in_gamut, zero, one);
                    was_searching = alwan_simd_select(is_white, zero, was_searching);
                    was_searching = alwan_simd_select(is_black, zero, was_searching);
                    alwan_simd_mask was_mask = alwan_simd_cmpgt(was_searching, zero);
                    res_r = alwan_simd_select(was_mask, fr, res_r);
                    res_g = alwan_simd_select(was_mask, fg, res_g);
                    res_b = alwan_simd_select(was_mask, fb, res_b);
                }
            }

            alwan_simd_store(&o0[i], res_r);
            alwan_simd_store(&o1[i], res_g);
            alwan_simd_store(&o2[i], res_b);
        }
    }
#endif /* ALWAN_SIMD_WIDTH > 1 */

    /* Scalar tail */
    for (; i < n; i++) {
        alwan_vec3 origin = {{(alwan_scalar)i0[i], (alwan_scalar)i1[i], (alwan_scalar)i2[i]}};
        alwan_vec3 mapped = gamut_css_map_v(origin);
        o0[i] = (alwan_simd_lane)mapped.v[0];
        o1[i] = (alwan_simd_lane)mapped.v[1];
        o2[i] = (alwan_simd_lane)mapped.v[2];
    }
}
