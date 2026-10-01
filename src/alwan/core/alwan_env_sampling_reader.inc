/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The hierarchical product sampler of an equirectangular map, written against
 * an accessor instead of a pointer. No include guard - included once per
 * binding.
 *
 * WHAT IS SAMPLED. The map's weights (luminance x sin theta, one per pixel)
 * times a lobe: max(0, n . w) for a cosine, raised to an exponent for Phong. A
 * sample walks down the pyramid of alwan_env_sampling_core.inc from its one top
 * cell: at each level the children of the current cell are weighed by their
 * sum times the lobe's mean over their cell, floored at a fraction of its
 * bound there (alwan_env_lobe_weight), u0 picks
 * the column half and u1 the row half (each rescaled to [0, 1] for the next
 * level, so neighbouring u stay neighbours), and the probabilities of the
 * choices multiply. At the leaf the point is uniform in its pixel.
 *
 * WHY IT IS EXACT. The density of a point is the product of the probabilities
 * of the choices that lead to its pixel, over the pixel's area, and that is
 * what the sample returns and what the pdf query recomputes: no
 * approximation enters the density. The lobe's weight only shapes where points
 * go. It is positive wherever the lobe is positive somewhere in a cell, so
 * every pixel where light times lobe is positive can be reached, and an
 * estimate sum f / pdf is unbiased. A coarse level's bound can see light the
 * finer levels cannot reach (a cell's bright part behind the lobe, its visible
 * part dark): the walk then meets children that all weigh 0 and the sample
 * comes back empty (pdf 0), which costs a sample and biases nothing.
 *
 * ACCESSOR. The pyramid is read through ALWAN_ENVP_READ(i). A shader defines
 *     StructuredBuffer<float> Envp : register(t5);
 *     #define ALWAN_ENVP_NAME     sky
 *     #define ALWAN_ENVP_READ(i)  Envp[i]
 *     #include "core/alwan_env_sampling_reader.inc"
 * and uploads the pyramid alwan_env_product_sampler_get_layout_f32 hands back.
 */

#if defined(ALWAN_ENVP_READER_POINTER)

# define ALWAN_ENVP_FN_(base)      ALWAN_CORE_FNVLIT(base)
# define ALWAN_ENVP_PARAMS_        const ALWAN_CORE_T *tab,
# define ALWAN_ENVP_ARGS_          tab,
# define ALWAN_ENVP_READ(i)        tab[i]

#else

# if !defined(ALWAN_ENVP_NAME) || !defined(ALWAN_ENVP_READ)
#  error "alwan_env_sampling_reader.inc: define ALWAN_ENVP_NAME and ALWAN_ENVP_READ before including it"
# endif
# define ALWAN_ENVP_PASTE2_(a, b)  a##_##b
# define ALWAN_ENVP_PASTE_(a, b)   ALWAN_ENVP_PASTE2_(a, b)
# define ALWAN_ENVP_FN_(base)      ALWAN_ENVP_PASTE_(base, ALWAN_ENVP_NAME)
# define ALWAN_ENVP_PARAMS_
# define ALWAN_ENVP_ARGS_

#endif

/* Cell (i, j) of level l weighed: its sum times the lobe's bound over it; 0 for
 * a cell wholly in the padding. w x h is the map, 2^la x 2^lb its padding. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_ENVP_FN_(alwan_envp_weight)(ALWAN_ENVP_PARAMS_ int w, int h, int la, int lb, int l,
                                                            int i, int j, int kind, ALWAN_CORE_T ax, ALWAN_CORE_T ay,
                                                            ALWAN_CORE_T az, ALWAN_CORE_T e) {
    const int sx = ALWAN_CORE_FNVLIT(alwan_envp_span)(la, l);
    const int sy = ALWAN_CORE_FNVLIT(alwan_envp_span)(lb, l);
    const int dw = ALWAN_CORE_FNVLIT(alwan_envp_dim)(la, l);
    int xe, ye;
    ALWAN_CORE_T sum;
    if (i * sx >= w || j * sy >= h) return ALWAN_CORE_LITERAL(0.0);
    sum = ALWAN_ENVP_READ(ALWAN_CORE_FNVLIT(alwan_envp_level_off)(la, lb, l) + j * dw + i);
    if (!(sum > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(0.0);
    xe = (i + 1) * sx; if (xe > w) xe = w;
    ye = (j + 1) * sy; if (ye > h) ye = h;
    return sum * ALWAN_CORE_FNVLIT(alwan_env_lobe_weight)((ALWAN_CORE_T)(i * sx) / (ALWAN_CORE_T)w, (ALWAN_CORE_T)xe / (ALWAN_CORE_T)w,
                                                         (ALWAN_CORE_T)(j * sy) / (ALWAN_CORE_T)h, (ALWAN_CORE_T)ye / (ALWAN_CORE_T)h,
                                                         kind, ax, ay, az, e);
}

/* One sample: (x, y, density against the map's area), the density 0 for an
 * empty sample. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_ENVP_FN_(alwan_envp_sample)(ALWAN_ENVP_PARAMS_ int w, int h, int la, int lb, int kind,
                                                               ALWAN_CORE_T ax, ALWAN_CORE_T ay, ALWAN_CORE_T az,
                                                               ALWAN_CORE_T e, ALWAN_CORE_T u0, ALWAN_CORE_T u1) {
    ALWAN_CORE_VEC3 out;
    const int top = la > lb ? la : lb;
    int l, i = 0, j = 0;
    ALWAN_DET_PRECISE ALWAN_CORE_T prob = ALWAN_CORE_LITERAL(1.0);
    out.v[0] = ALWAN_CORE_LITERAL(0.0);
    out.v[1] = ALWAN_CORE_LITERAL(0.0);
    out.v[2] = ALWAN_CORE_LITERAL(0.0);
    for (l = top; l > 0; l--) {
        const int split_x = ALWAN_CORE_FNVLIT(alwan_envp_dim)(la, l - 1) > ALWAN_CORE_FNVLIT(alwan_envp_dim)(la, l);
        const int split_y = ALWAN_CORE_FNVLIT(alwan_envp_dim)(lb, l - 1) > ALWAN_CORE_FNVLIT(alwan_envp_dim)(lb, l);
        const int ci = split_x ? 2 * i : i;
        const int cj = split_y ? 2 * j : j;
        ALWAN_DET_PRECISE ALWAN_CORE_T w00, w10, w01, w11, left, total, colw, top_w, p;
        w00 = ALWAN_ENVP_FN_(alwan_envp_weight)(ALWAN_ENVP_ARGS_ w, h, la, lb, l - 1, ci, cj, kind, ax, ay, az, e);
        w10 = split_x ? ALWAN_ENVP_FN_(alwan_envp_weight)(ALWAN_ENVP_ARGS_ w, h, la, lb, l - 1, ci + 1, cj, kind, ax, ay, az, e)
                      : ALWAN_CORE_LITERAL(0.0);
        w01 = split_y ? ALWAN_ENVP_FN_(alwan_envp_weight)(ALWAN_ENVP_ARGS_ w, h, la, lb, l - 1, ci, cj + 1, kind, ax, ay, az, e)
                      : ALWAN_CORE_LITERAL(0.0);
        w11 = (split_x && split_y)
                  ? ALWAN_ENVP_FN_(alwan_envp_weight)(ALWAN_ENVP_ARGS_ w, h, la, lb, l - 1, ci + 1, cj + 1, kind, ax, ay, az, e)
                  : ALWAN_CORE_LITERAL(0.0);
        left = w00 + w01;
        total = left + (w10 + w11);
        if (!(total > ALWAN_CORE_LITERAL(0.0))) return out;
        /* the column half from u0 */
        i = ci;
        colw = left;
        if (split_x) {
            p = left / total;
            /* never into a half that weighs 0: rounding can carry u to 1 */
            if ((u0 < p && left > ALWAN_CORE_LITERAL(0.0)) || !(total - left > ALWAN_CORE_LITERAL(0.0))) {
                u0 = u0 / p;
            } else {
                u0 = (u0 - p) / (ALWAN_CORE_LITERAL(1.0) - p);
                i = ci + 1;
                colw = total - left;
            }
        }
        /* the row half from u1, within the column */
        j = cj;
        top_w = (i == ci) ? w00 : w10;
        if (split_y) {
            p = top_w / colw;
            if ((u1 < p && top_w > ALWAN_CORE_LITERAL(0.0)) || !(colw - top_w > ALWAN_CORE_LITERAL(0.0))) {
                u1 = u1 / p;
            } else {
                u1 = (u1 - p) / (ALWAN_CORE_LITERAL(1.0) - p);
                j = cj + 1;
                top_w = colw - top_w;
            }
        }
        prob = prob * (top_w / total);
        if (u0 > ALWAN_CORE_LITERAL(1.0)) u0 = ALWAN_CORE_LITERAL(1.0);
        if (u1 > ALWAN_CORE_LITERAL(1.0)) u1 = ALWAN_CORE_LITERAL(1.0);
        if (u0 < ALWAN_CORE_LITERAL(0.0)) u0 = ALWAN_CORE_LITERAL(0.0);
        if (u1 < ALWAN_CORE_LITERAL(0.0)) u1 = ALWAN_CORE_LITERAL(0.0);
    }
    out.v[0] = ((ALWAN_CORE_T)i + u0) / (ALWAN_CORE_T)w;
    out.v[1] = ((ALWAN_CORE_T)j + u1) / (ALWAN_CORE_T)h;
    out.v[2] = prob * (ALWAN_CORE_T)w * (ALWAN_CORE_T)h;
    return out;
}

/* The density at map position (x, y) against the map's area, as the sampler
 * draws: the same choices recomputed down to the pixel holding (x, y). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_ENVP_FN_(alwan_envp_pdf)(ALWAN_ENVP_PARAMS_ int w, int h, int la, int lb, int kind,
                                                         ALWAN_CORE_T ax, ALWAN_CORE_T ay, ALWAN_CORE_T az, ALWAN_CORE_T e,
                                                         ALWAN_CORE_T x, ALWAN_CORE_T y) {
    const int top = la > lb ? la : lb;
    const int pi_ = ALWAN_CORE_FNVLIT(alwan_env_cell)(x * (ALWAN_CORE_T)w, w);
    const int pj = ALWAN_CORE_FNVLIT(alwan_env_cell)(y * (ALWAN_CORE_T)h, h);
    int l;
    ALWAN_DET_PRECISE ALWAN_CORE_T prob = ALWAN_CORE_LITERAL(1.0);
    if (!(x >= ALWAN_CORE_LITERAL(0.0) && x <= ALWAN_CORE_LITERAL(1.0) &&
          y >= ALWAN_CORE_LITERAL(0.0) && y <= ALWAN_CORE_LITERAL(1.0))) return ALWAN_CORE_LITERAL(0.0);
    for (l = top; l > 0; l--) {
        const int split_x = ALWAN_CORE_FNVLIT(alwan_envp_dim)(la, l - 1) > ALWAN_CORE_FNVLIT(alwan_envp_dim)(la, l);
        const int split_y = ALWAN_CORE_FNVLIT(alwan_envp_dim)(lb, l - 1) > ALWAN_CORE_FNVLIT(alwan_envp_dim)(lb, l);
        const int i = pi_ / ALWAN_CORE_FNVLIT(alwan_envp_span)(la, l);
        const int j = pj / ALWAN_CORE_FNVLIT(alwan_envp_span)(lb, l);
        const int ci = split_x ? 2 * i : i;
        const int cj = split_y ? 2 * j : j;
        const int ti = pi_ / ALWAN_CORE_FNVLIT(alwan_envp_span)(la, l - 1);
        const int tj = pj / ALWAN_CORE_FNVLIT(alwan_envp_span)(lb, l - 1);
        ALWAN_DET_PRECISE ALWAN_CORE_T w00, w10, w01, w11, total, wt;
        w00 = ALWAN_ENVP_FN_(alwan_envp_weight)(ALWAN_ENVP_ARGS_ w, h, la, lb, l - 1, ci, cj, kind, ax, ay, az, e);
        w10 = split_x ? ALWAN_ENVP_FN_(alwan_envp_weight)(ALWAN_ENVP_ARGS_ w, h, la, lb, l - 1, ci + 1, cj, kind, ax, ay, az, e)
                      : ALWAN_CORE_LITERAL(0.0);
        w01 = split_y ? ALWAN_ENVP_FN_(alwan_envp_weight)(ALWAN_ENVP_ARGS_ w, h, la, lb, l - 1, ci, cj + 1, kind, ax, ay, az, e)
                      : ALWAN_CORE_LITERAL(0.0);
        w11 = (split_x && split_y)
                  ? ALWAN_ENVP_FN_(alwan_envp_weight)(ALWAN_ENVP_ARGS_ w, h, la, lb, l - 1, ci + 1, cj + 1, kind, ax, ay, az, e)
                  : ALWAN_CORE_LITERAL(0.0);
        total = (w00 + w01) + (w10 + w11);
        if (!(total > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(0.0);
        wt = (ti == ci) ? ((tj == cj) ? w00 : w01) : ((tj == cj) ? w10 : w11);
        prob = prob * (wt / total);
        if (!(prob > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(0.0);
    }
    return prob * (ALWAN_CORE_T)w * (ALWAN_CORE_T)h;
}

#undef ALWAN_ENVP_FN_
#undef ALWAN_ENVP_PARAMS_
#undef ALWAN_ENVP_ARGS_
#undef ALWAN_ENVP_READ
#if !defined(ALWAN_ENVP_READER_POINTER)
# undef ALWAN_ENVP_PASTE2_
# undef ALWAN_ENVP_PASTE_
# undef ALWAN_ENVP_NAME
#endif
#undef ALWAN_ENVP_READER_POINTER
