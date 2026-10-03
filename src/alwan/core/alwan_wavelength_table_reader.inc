/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan's private copy of the piecewise-constant tables the 2D importance sampler used
 * (that sampler is image processing and lives in suwar). The wavelength sampler's
 * TABULATED density reads them as one column; the copy is the sampler's code renamed
 * (alwan_is2d_ to alwan_wlt_), so its results are the same bit for bit.
 *
 * 2D importance sampling, written against an accessor instead of a pointer.
 * No include guard - included once per binding.
 */

/* ================================================================
 * WHAT IS SAMPLED
 *
 * An image of nu x nv non-negative weights is a density over a rectangle,
 * constant on each cell: w / (sum of weights x cell area). A point is drawn by
 * inverting two CDFs: the row from the CDF over the rows' sums (the marginal),
 * then the column from that row's own CDF (the conditional), each u landing at
 * its proportional place inside the cell it picks, so the point is uniform
 * within its cell. Each CDF is the running sum of its weights over their total.
 *
 * Two ways to invert a CDF, and prepare builds the tables of one:
 *   SEARCH   the CDFs, and a bisection for the cell whose CDF interval holds u:
 *            exact, O(log n). The density is the image's.
 *   DIRECT   the inverse CDFs, tabulated at u = k / r and read with one index
 *            and a linear blend: O(1). Where a CDF breakpoint falls between two
 *            nodes the blend is not the exact inverse, so the points follow the
 *            image a little less closely; the pdf this mode returns is the
 *            density it actually draws from (one over r times the node
 *            spacing), so an estimate built on it stays unbiased.
 *
 * ACCESSOR. Nothing here names a pointer. Tables are read through
 *     ALWAN_WLT_READ(i)
 * POINTER BINDING: alwan_wavelength_table_core.inc defines
 * ALWAN_WLT_READER_POINTER and includes this file where pointers exist; the
 * table is then the leading parameter. NAMED BINDING: a shader defines
 *
 *     StructuredBuffer<float> Is2d : register(t3);
 *     #define ALWAN_WLT_NAME     sky
 *     #define ALWAN_WLT_READ(i)  Is2d[i]
 *     #include "core/alwan_wavelength_table_reader.inc"
 *
 *     alwan_vec3 s = alwan_wlt_sample_2d_direct_sky(nu, nv, ru, rv, 0, 0, 1, 1, u0, u1);
 *
 * and uploads the table alwan_importance_sampling_2d_get_layout_f32 hands back.
 * ================================================================ */

#if defined(ALWAN_WLT_READER_POINTER)

# define ALWAN_WLT_FN_(base)      ALWAN_CORE_FNVLIT(base)
# define ALWAN_WLT_PARAMS_        const ALWAN_CORE_T *tab,
# define ALWAN_WLT_ARGS_          tab,
# define ALWAN_WLT_READ(i)        tab[i]

#else

# if !defined(ALWAN_WLT_NAME) || !defined(ALWAN_WLT_READ)
#  error "alwan_wavelength_table_reader.inc: define ALWAN_WLT_NAME and ALWAN_WLT_READ before including it"
# endif
# define ALWAN_WLT_PASTE2_(a, b)  a##_##b
# define ALWAN_WLT_PASTE_(a, b)   ALWAN_WLT_PASTE2_(a, b)
# define ALWAN_WLT_FN_(base)      ALWAN_WLT_PASTE_(base, ALWAN_WLT_NAME)
# define ALWAN_WLT_PARAMS_
# define ALWAN_WLT_ARGS_

#endif

/* The interval of a rising table that holds v: the table has n + 1 entries from
 * base, and the answer is the i in [0, n - 1] with entry i <= v < entry i + 1,
 * found by bisection. An entry at 1 counts as above v even for v = 1, so v = 1
 * lands in the last interval that reaches 1, never in an empty one after it. */
ALWAN_INLINE int ALWAN_WLT_FN_(alwan_wlt_search)(ALWAN_WLT_PARAMS_ int base, int n, ALWAN_CORE_T v) {
    int lo = 1, hi = n;
    while (lo < hi) {
        const int mid = lo + (hi - lo) / 2;
        const ALWAN_CORE_T e = ALWAN_WLT_READ(base + mid);
        if (e > v || e >= ALWAN_CORE_LITERAL(1.0)) hi = mid;
        else lo = mid + 1;
    }
    return lo - 1;
}

/* Where u falls inside interval i of the rising table at base: the fraction of
 * the way from entry i to entry i + 1, in [0, 1]. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_WLT_FN_(alwan_wlt_fraction)(ALWAN_WLT_PARAMS_ int base, int i, ALWAN_CORE_T u) {
    const ALWAN_CORE_T a = ALWAN_WLT_READ(base + i);
    const ALWAN_CORE_T span = ALWAN_WLT_READ(base + i + 1) - a;
    ALWAN_DET_PRECISE ALWAN_CORE_T f = span > ALWAN_CORE_LITERAL(0.0) ? (u - a) / span : ALWAN_CORE_LITERAL(0.0);
    if (f < ALWAN_CORE_LITERAL(0.0)) f = ALWAN_CORE_LITERAL(0.0);
    if (f > ALWAN_CORE_LITERAL(1.0)) f = ALWAN_CORE_LITERAL(1.0);
    return f;
}

/* SEARCH: the position t in [0, 1] across n cells that the CDF at base gives u,
 * and the cell it is in. */
ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_WLT_FN_(alwan_wlt_search_1d)(ALWAN_WLT_PARAMS_ int base, int n, ALWAN_CORE_T u) {
    ALWAN_CORE_VEC2 r;
    const int i = ALWAN_WLT_FN_(alwan_wlt_search)(ALWAN_WLT_ARGS_ base, n, u);
    const ALWAN_CORE_T f = ALWAN_WLT_FN_(alwan_wlt_fraction)(ALWAN_WLT_ARGS_ base, i, u);
    r.v[0] = ((ALWAN_CORE_T)i + f) / (ALWAN_CORE_T)n;
    r.v[1] = (ALWAN_CORE_T)i;
    return r;
}

/* DIRECT: the position t that the r-interval inverse table at base gives u, and
 * the density of that draw against an axis of length len. */
ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_WLT_FN_(alwan_wlt_direct_1d)(ALWAN_WLT_PARAMS_ int base, int r, ALWAN_CORE_T u,
                                                                  ALWAN_CORE_T len) {
    ALWAN_CORE_VEC2 out;
    const ALWAN_CORE_T s = u * (ALWAN_CORE_T)r;
    const int k = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(s, r);
    const ALWAN_CORE_T a = ALWAN_WLT_READ(base + k);
    const ALWAN_CORE_T b = ALWAN_WLT_READ(base + k + 1);
    ALWAN_DET_PRECISE ALWAN_CORE_T f = s - (ALWAN_CORE_T)k;
    if (f > ALWAN_CORE_LITERAL(1.0)) f = ALWAN_CORE_LITERAL(1.0);
    out.v[0] = a + f * (b - a);
    out.v[1] = b > a ? ALWAN_CORE_LITERAL(1.0) / ((ALWAN_CORE_T)r * (b - a) * len) : ALWAN_CORE_LITERAL(0.0);
    return out;
}

/* DIRECT: the density of the draw at position t, 0 where the table never goes. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_WLT_FN_(alwan_wlt_direct_pdf_1d)(ALWAN_WLT_PARAMS_ int base, int r, ALWAN_CORE_T t,
                                                                   ALWAN_CORE_T len) {
    int k;
    ALWAN_CORE_T a, b;
    if (!(t >= ALWAN_WLT_READ(base) && t <= ALWAN_WLT_READ(base + r))) return ALWAN_CORE_LITERAL(0.0);
    k = ALWAN_WLT_FN_(alwan_wlt_search)(ALWAN_WLT_ARGS_ base, r, t);
    a = ALWAN_WLT_READ(base + k);
    b = ALWAN_WLT_READ(base + k + 1);
    return b > a ? ALWAN_CORE_LITERAL(1.0) / ((ALWAN_CORE_T)r * (b - a) * len) : ALWAN_CORE_LITERAL(0.0);
}

/* DIRECT: the u that the inverse table at base turns into t. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_WLT_FN_(alwan_wlt_direct_invert_1d)(ALWAN_WLT_PARAMS_ int base, int r, ALWAN_CORE_T t) {
    const int k = ALWAN_WLT_FN_(alwan_wlt_search)(ALWAN_WLT_ARGS_ base, r, t);
    const ALWAN_CORE_T f = ALWAN_WLT_FN_(alwan_wlt_fraction)(ALWAN_WLT_ARGS_ base, k, t);
    return ((ALWAN_CORE_T)k + f) / (ALWAN_CORE_T)r;
}

/* SEARCH: the u that the CDF at base (n cells) turns into position t. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_WLT_FN_(alwan_wlt_search_invert_1d)(ALWAN_WLT_PARAMS_ int base, int n, ALWAN_CORE_T t) {
    const ALWAN_CORE_T c = t * (ALWAN_CORE_T)n;
    const int i = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(c, n);
    const ALWAN_CORE_T a = ALWAN_WLT_READ(base + i);
    ALWAN_DET_PRECISE ALWAN_CORE_T f = c - (ALWAN_CORE_T)i;
    if (f > ALWAN_CORE_LITERAL(1.0)) f = ALWAN_CORE_LITERAL(1.0);
    return a + f * (ALWAN_WLT_READ(base + i + 1) - a);
}

/* ----------------------------------------------------------------
 * 2D, on the domain [x0, x1] x [y0, y1]. A sample is (x, y, pdf), the pdf
 * against the domain's area.
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_WLT_FN_(alwan_wlt_sample_2d_search)(ALWAN_WLT_PARAMS_ int nu, int nv,
                                                                         ALWAN_CORE_T x0, ALWAN_CORE_T y0,
                                                                         ALWAN_CORE_T x1, ALWAN_CORE_T y1,
                                                                         ALWAN_CORE_T u0, ALWAN_CORE_T u1) {
    ALWAN_CORE_VEC3 out;
    const ALWAN_CORE_VEC2 sy = ALWAN_WLT_FN_(alwan_wlt_search_1d)(ALWAN_WLT_ARGS_ ALWAN_CORE_FNVLIT(alwan_wlt_off_mcdf)(nu, nv),
                                                                    nv, u1);
    const int iv = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(sy.v[1], nv);
    const ALWAN_CORE_VEC2 sx = ALWAN_WLT_FN_(alwan_wlt_search_1d)(ALWAN_WLT_ARGS_
                                                                    ALWAN_CORE_FNVLIT(alwan_wlt_off_cdf)(nu, nv) + iv * (nu + 1),
                                                                    nu, u0);
    const int iu = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(sx.v[1], nu);
    out.v[0] = x0 + sx.v[0] * (x1 - x0);
    out.v[1] = y0 + sy.v[0] * (y1 - y0);
    out.v[2] = ALWAN_WLT_READ(iv * nu + iu) * ALWAN_WLT_READ(ALWAN_CORE_FNVLIT(alwan_wlt_off_norm)(nu, nv));
    return out;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_WLT_FN_(alwan_wlt_sample_2d_direct)(ALWAN_WLT_PARAMS_ int nu, int nv, int ru, int rv,
                                                                         ALWAN_CORE_T x0, ALWAN_CORE_T y0,
                                                                         ALWAN_CORE_T x1, ALWAN_CORE_T y1,
                                                                         ALWAN_CORE_T u0, ALWAN_CORE_T u1) {
    ALWAN_CORE_VEC3 out;
    const ALWAN_CORE_VEC2 sy = ALWAN_WLT_FN_(alwan_wlt_direct_1d)(ALWAN_WLT_ARGS_ ALWAN_CORE_FNVLIT(alwan_wlt_off_minv)(nv, ru),
                                                                    rv, u1, y1 - y0);
    const int iv = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(sy.v[0] * (ALWAN_CORE_T)nv, nv);
    const ALWAN_CORE_VEC2 sx = ALWAN_WLT_FN_(alwan_wlt_direct_1d)(ALWAN_WLT_ARGS_ iv * (ru + 1), ru, u0, x1 - x0);
    (void)nu;
    out.v[0] = x0 + sx.v[0] * (x1 - x0);
    out.v[1] = y0 + sy.v[0] * (y1 - y0);
    out.v[2] = sx.v[1] * sy.v[1];
    return out;
}

/* The density at (x, y): the image's in SEARCH, the sampler's own in DIRECT;
 * 0 outside the domain in both. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_WLT_FN_(alwan_wlt_pdf_2d_search)(ALWAN_WLT_PARAMS_ int nu, int nv,
                                                                   ALWAN_CORE_T x0, ALWAN_CORE_T y0,
                                                                   ALWAN_CORE_T x1, ALWAN_CORE_T y1,
                                                                   ALWAN_CORE_T x, ALWAN_CORE_T y) {
    const ALWAN_CORE_T tx = (x - x0) / (x1 - x0);
    const ALWAN_CORE_T ty = (y - y0) / (y1 - y0);
    int iu, iv;
    if (!(tx >= ALWAN_CORE_LITERAL(0.0) && tx <= ALWAN_CORE_LITERAL(1.0) &&
          ty >= ALWAN_CORE_LITERAL(0.0) && ty <= ALWAN_CORE_LITERAL(1.0))) return ALWAN_CORE_LITERAL(0.0);
    iu = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(tx * (ALWAN_CORE_T)nu, nu);
    iv = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(ty * (ALWAN_CORE_T)nv, nv);
    return ALWAN_WLT_READ(iv * nu + iu) * ALWAN_WLT_READ(ALWAN_CORE_FNVLIT(alwan_wlt_off_norm)(nu, nv));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_WLT_FN_(alwan_wlt_pdf_2d_direct)(ALWAN_WLT_PARAMS_ int nu, int nv, int ru, int rv,
                                                                   ALWAN_CORE_T x0, ALWAN_CORE_T y0,
                                                                   ALWAN_CORE_T x1, ALWAN_CORE_T y1,
                                                                   ALWAN_CORE_T x, ALWAN_CORE_T y) {
    const ALWAN_CORE_T tx = (x - x0) / (x1 - x0);
    const ALWAN_CORE_T ty = (y - y0) / (y1 - y0);
    const ALWAN_CORE_T py = ALWAN_WLT_FN_(alwan_wlt_direct_pdf_1d)(ALWAN_WLT_ARGS_ ALWAN_CORE_FNVLIT(alwan_wlt_off_minv)(nv, ru),
                                                                     rv, ty, y1 - y0);
    const int iv = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(ty * (ALWAN_CORE_T)nv, nv);
    (void)nu;
    if (py == ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
    return ALWAN_WLT_FN_(alwan_wlt_direct_pdf_1d)(ALWAN_WLT_ARGS_ iv * (ru + 1), ru, tx, x1 - x0) * py;
}

/* ----------------------------------------------------------------
 * ALIAS (Walker 1977, Vose 1991): one table over all n = nu x nv cells. u1
 * picks a slot k = floor(u1 n); the fraction f left over is compared with the
 * slot's threshold: f < prob[k] keeps cell k, else the slot's alias. What is
 * left of f after that comparison, rescaled to [0, 1], is the point's place
 * down its row (fewer bits than u1 had: n's worth went to the slot); u0 is its
 * place across. The density is the image's, as SEARCH's.
 * ---------------------------------------------------------------- */

/* The cell u1 picks (as a value) and the fraction left for the row. */
ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_WLT_FN_(alwan_wlt_alias_pick)(ALWAN_WLT_PARAMS_ int n, ALWAN_CORE_T u1) {
    ALWAN_CORE_VEC2 r;
    const ALWAN_CORE_T s = u1 * (ALWAN_CORE_T)n;
    const int k = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(s, n);
    const ALWAN_CORE_T p = ALWAN_WLT_READ(ALWAN_CORE_FNVLIT(alwan_wlt_off_aprob)(n) + k);
    ALWAN_DET_PRECISE ALWAN_CORE_T f = s - (ALWAN_CORE_T)k;
    ALWAN_DET_PRECISE ALWAN_CORE_T t;
    if (f > ALWAN_CORE_LITERAL(1.0)) f = ALWAN_CORE_LITERAL(1.0);
    if (f < p) {
        r.v[0] = (ALWAN_CORE_T)k;
        t = f / p;
    } else {
        r.v[0] = ALWAN_WLT_READ(ALWAN_CORE_FNVLIT(alwan_wlt_off_aidx)(n) + k);
        t = p < ALWAN_CORE_LITERAL(1.0) ? (f - p) / (ALWAN_CORE_LITERAL(1.0) - p) : ALWAN_CORE_LITERAL(0.0);
    }
    if (t < ALWAN_CORE_LITERAL(0.0)) t = ALWAN_CORE_LITERAL(0.0);
    if (t > ALWAN_CORE_LITERAL(1.0)) t = ALWAN_CORE_LITERAL(1.0);
    r.v[1] = t;
    return r;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_WLT_FN_(alwan_wlt_sample_2d_alias)(ALWAN_WLT_PARAMS_ int nu, int nv,
                                                                        ALWAN_CORE_T x0, ALWAN_CORE_T y0,
                                                                        ALWAN_CORE_T x1, ALWAN_CORE_T y1,
                                                                        ALWAN_CORE_T u0, ALWAN_CORE_T u1) {
    ALWAN_CORE_VEC3 out;
    const int n = nu * nv;
    const ALWAN_CORE_VEC2 pick = ALWAN_WLT_FN_(alwan_wlt_alias_pick)(ALWAN_WLT_ARGS_ n, u1);
    const int c = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(pick.v[0], n);
    const int iv = c / nu;
    const int iu = c - iv * nu;
    out.v[0] = x0 + (((ALWAN_CORE_T)iu + u0) / (ALWAN_CORE_T)nu) * (x1 - x0);
    out.v[1] = y0 + (((ALWAN_CORE_T)iv + pick.v[1]) / (ALWAN_CORE_T)nv) * (y1 - y0);
    out.v[2] = ALWAN_WLT_READ(c) * ALWAN_WLT_READ(ALWAN_CORE_FNVLIT(alwan_wlt_off_anorm)(n));
    return out;
}

/* ALIAS: the image's density at (x, y), 0 outside the domain. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_WLT_FN_(alwan_wlt_pdf_2d_alias)(ALWAN_WLT_PARAMS_ int nu, int nv,
                                                                  ALWAN_CORE_T x0, ALWAN_CORE_T y0,
                                                                  ALWAN_CORE_T x1, ALWAN_CORE_T y1,
                                                                  ALWAN_CORE_T x, ALWAN_CORE_T y) {
    const ALWAN_CORE_T tx = (x - x0) / (x1 - x0);
    const ALWAN_CORE_T ty = (y - y0) / (y1 - y0);
    int iu, iv;
    if (!(tx >= ALWAN_CORE_LITERAL(0.0) && tx <= ALWAN_CORE_LITERAL(1.0) &&
          ty >= ALWAN_CORE_LITERAL(0.0) && ty <= ALWAN_CORE_LITERAL(1.0))) return ALWAN_CORE_LITERAL(0.0);
    iu = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(tx * (ALWAN_CORE_T)nu, nu);
    iv = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(ty * (ALWAN_CORE_T)nv, nv);
    return ALWAN_WLT_READ(iv * nu + iu) * ALWAN_WLT_READ(ALWAN_CORE_FNVLIT(alwan_wlt_off_anorm)(nu * nv));
}

/* The (u0, u1) that turns into (x, y), or (-1, -1) outside the domain. */
ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_WLT_FN_(alwan_wlt_invert_2d_search)(ALWAN_WLT_PARAMS_ int nu, int nv,
                                                                         ALWAN_CORE_T x0, ALWAN_CORE_T y0,
                                                                         ALWAN_CORE_T x1, ALWAN_CORE_T y1,
                                                                         ALWAN_CORE_T x, ALWAN_CORE_T y) {
    ALWAN_CORE_VEC2 r;
    const ALWAN_CORE_T tx = (x - x0) / (x1 - x0);
    const ALWAN_CORE_T ty = (y - y0) / (y1 - y0);
    int iv;
    r.v[0] = ALWAN_CORE_LITERAL(-1.0);
    r.v[1] = ALWAN_CORE_LITERAL(-1.0);
    if (!(tx >= ALWAN_CORE_LITERAL(0.0) && tx <= ALWAN_CORE_LITERAL(1.0) &&
          ty >= ALWAN_CORE_LITERAL(0.0) && ty <= ALWAN_CORE_LITERAL(1.0))) return r;
    iv = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(ty * (ALWAN_CORE_T)nv, nv);
    r.v[1] = ALWAN_WLT_FN_(alwan_wlt_search_invert_1d)(ALWAN_WLT_ARGS_ ALWAN_CORE_FNVLIT(alwan_wlt_off_mcdf)(nu, nv), nv, ty);
    r.v[0] = ALWAN_WLT_FN_(alwan_wlt_search_invert_1d)(ALWAN_WLT_ARGS_
                                                         ALWAN_CORE_FNVLIT(alwan_wlt_off_cdf)(nu, nv) + iv * (nu + 1), nu, tx);
    return r;
}

ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_WLT_FN_(alwan_wlt_invert_2d_direct)(ALWAN_WLT_PARAMS_ int nu, int nv, int ru, int rv,
                                                                         ALWAN_CORE_T x0, ALWAN_CORE_T y0,
                                                                         ALWAN_CORE_T x1, ALWAN_CORE_T y1,
                                                                         ALWAN_CORE_T x, ALWAN_CORE_T y) {
    ALWAN_CORE_VEC2 r;
    const ALWAN_CORE_T tx = (x - x0) / (x1 - x0);
    const ALWAN_CORE_T ty = (y - y0) / (y1 - y0);
    int iv;
    (void)nu;
    r.v[0] = ALWAN_CORE_LITERAL(-1.0);
    r.v[1] = ALWAN_CORE_LITERAL(-1.0);
    if (!(tx >= ALWAN_CORE_LITERAL(0.0) && tx <= ALWAN_CORE_LITERAL(1.0) &&
          ty >= ALWAN_CORE_LITERAL(0.0) && ty <= ALWAN_CORE_LITERAL(1.0))) return r;
    iv = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(ty * (ALWAN_CORE_T)nv, nv);
    r.v[1] = ALWAN_WLT_FN_(alwan_wlt_direct_invert_1d)(ALWAN_WLT_ARGS_ ALWAN_CORE_FNVLIT(alwan_wlt_off_minv)(nv, ru), rv, ty);
    r.v[0] = ALWAN_WLT_FN_(alwan_wlt_direct_invert_1d)(ALWAN_WLT_ARGS_ iv * (ru + 1), ru, tx);
    return r;
}

#undef ALWAN_WLT_FN_
#undef ALWAN_WLT_PARAMS_
#undef ALWAN_WLT_ARGS_
#undef ALWAN_WLT_READ
#if !defined(ALWAN_WLT_READER_POINTER)
# undef ALWAN_WLT_PASTE2_
# undef ALWAN_WLT_PASTE_
# undef ALWAN_WLT_NAME
#endif
#undef ALWAN_WLT_READER_POINTER
