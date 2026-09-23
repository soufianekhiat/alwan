/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Histogram matching: each channel of an image remapped so that its cumulative
 * distribution matches a reference image's, the simplest way to carry one shot's tonal
 * and colour spread onto another. It follows scikit-image's match_histograms (the float
 * path of _match_cumulative_cdf, BSD-3-Clause):
 *
 *   the distinct values of the source channel, with their counts, give quantiles
 *   cumsum(counts) / n; the reference's distinct values give its own quantiles; each
 *   source value becomes numpy.interp(its quantile, reference quantiles, reference values)
 *
 * and numpy.interp's branches are followed: a quantile below the first reference quantile
 * takes the first value, one at or past the last takes the last, one exactly on a
 * reference quantile takes that value, and the rest interpolate linearly. Suite 191
 * holds it to scikit-image.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <stdlib.h>

typedef struct {
    double v;
    size_t i;
} alwan_hm_item;

static int alwan_hm_cmp_item(void const *a, void const *b) {
    double const x = ((alwan_hm_item const *)a)->v, y = ((alwan_hm_item const *)b)->v;
    return x < y ? -1 : x > y;
}

static int alwan_hm_cmp_double(void const *a, void const *b) {
    double const x = *(double const *)a, y = *(double const *)b;
    return x < y ? -1 : x > y;
}

static int alwan_hm_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

/* numpy.interp for one x against xp (increasing, n points) and fp. */
static double alwan_hm_interp(double x, double const *xp, double const *fp, size_t n) {
    size_t lo = 0, hi = n;
    if (x < xp[0]) return fp[0];
    if (x > xp[n - 1]) return fp[n - 1];
    if (x == xp[n - 1]) return fp[n - 1];
    while (hi - lo > 1) {   /* xp[lo] <= x < xp[hi] */
        size_t const mid = (lo + hi) / 2;
        if (xp[mid] <= x) lo = mid; else hi = mid;
    }
    if (xp[lo] == x) return fp[lo];
    return (fp[lo + 1] - fp[lo]) / (xp[lo + 1] - xp[lo]) * (x - xp[lo]) + fp[lo];
}

alwan_status alwan__hm_run(void *out, size_t out_stride, void const *src, size_t src_stride, size_t src_count,
                           void const *ref, size_t ref_stride, size_t ref_count, size_t channels, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    alwan_hm_item *items;
    double *rv, *rq;
    size_t c, i, j, m;
    if (!out || !src || !ref) return ALWAN_E_INVALID;
    if (src_count == 0 || ref_count == 0 || channels == 0 || channels > 4) return ALWAN_E_INVALID;
    if (out_stride < channels * elem || src_stride < channels * elem || ref_stride < channels * elem) return ALWAN_E_INVALID;
    items = (alwan_hm_item *)ALWAN_ALLOC(alwan_safe_array_size(src_count, sizeof *items), sizeof(double));
    rv = (double *)ALWAN_ALLOC(alwan_safe_array_size(ref_count, sizeof(double)), sizeof(double));
    rq = (double *)ALWAN_ALLOC(alwan_safe_array_size(ref_count, sizeof(double)), sizeof(double));
    if (!items || !rv || !rq) {
        if (items) ALWAN_FREE(items);
        if (rv) ALWAN_FREE(rv);
        if (rq) ALWAN_FREE(rq);
        return ALWAN_E_NOMEM;
    }
    for (c = 0; c < channels; c++) {
        /* the reference's distinct values and their quantiles */
        for (i = 0; i < ref_count; i++) {
            char const *p = (char const *)ref + i * ref_stride;
            rv[i] = is_f32 ? (double)((alwan_f32 const *)p)[c] : (double)((alwan_f64 const *)p)[c];
            if (!alwan_hm_finite(rv[i])) goto invalid;
        }
        qsort(rv, ref_count, sizeof *rv, alwan_hm_cmp_double);
        for (m = 0, i = 0; i < ref_count; i++) {
            if (i + 1 == ref_count || rv[i + 1] != rv[i]) {
                rv[m] = rv[i];
                rq[m] = (double)(i + 1) / (double)ref_count;
                m++;
            }
        }
        /* the source's values, sorted with their positions */
        for (i = 0; i < src_count; i++) {
            char const *p = (char const *)src + i * src_stride;
            items[i].v = is_f32 ? (double)((alwan_f32 const *)p)[c] : (double)((alwan_f64 const *)p)[c];
            items[i].i = i;
            if (!alwan_hm_finite(items[i].v)) goto invalid;
        }
        qsort(items, src_count, sizeof *items, alwan_hm_cmp_item);
        for (i = 0; i < src_count; i = j) {
            double q, v;
            for (j = i; j < src_count && items[j].v == items[i].v; j++) {}
            q = (double)j / (double)src_count;
            v = alwan_hm_interp(q, rq, rv, m);
            for (; i < j; i++) {
                char *d = (char *)out + items[i].i * out_stride;
                if (is_f32) ((alwan_f32 *)d)[c] = (alwan_f32)v;
                else ((alwan_f64 *)d)[c] = (alwan_f64)v;
            }
        }
    }
    ALWAN_FREE(items);
    ALWAN_FREE(rv);
    ALWAN_FREE(rq);
    return ALWAN_OK;
invalid:
    ALWAN_FREE(items);
    ALWAN_FREE(rv);
    ALWAN_FREE(rq);
    return ALWAN_E_INVALID;
}
