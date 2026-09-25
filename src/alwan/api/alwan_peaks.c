/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_peak_local_max_{T} and _u8: the local maxima of an image as a list of coordinates,
 * as scikit-image's feature.peak_local_max computes them (BSD), which suite 250 holds this
 * to peak for peak.
 *
 * A candidate is a pixel equal to the maximum of the footprint around it (scipy's
 * maximum_filter with mode 'nearest': the footprint as given, its centre at (height / 2,
 * width / 2), the edge pixel repeated past the border) and above the threshold. An image
 * where every pixel is a candidate has none. The border strip is cleared, the candidates
 * are sorted by value, highest first, ties in raster order, and when min_distance is above
 * 1 they are thinned greedily in that order: each kept peak removes every later one closer
 * than min_distance in the p-norm. That is scikit-image's ensure_spacing, whose batches
 * reduce to the same greedy pass. With labels, each region is searched on its own inside
 * its bounding box, everything outside the region set to the lowest value of the type, and
 * the lists are joined in label order; num_peaks over the whole list then re-sorts it by
 * the image's values.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    double key;   /* sorted ascending: minus the value, or the 8-bit wrap scikit-image's -uint8 gives */
    size_t i;     /* raster index, the tie-break (a stable sort) */
} alwan_pk_cand;

static int alwan_pk_cmp(void const *a, void const *b) {
    alwan_pk_cand const *p = (alwan_pk_cand const *)a, *q = (alwan_pk_cand const *)b;
    if (p->key != q->key) return p->key < q->key ? -1 : 1;
    return p->i < q->i ? -1 : p->i > q->i ? 1 : 0;
}

/* The distance between two integer points in the p-norm. Only its comparison with the
 * integer min_distance matters. That is exact for p = 1, 2 and infinity; for any other p a
 * point on an axis is exactly its offset, as scipy's cdist returns it, where
 * pow(pow(4, 3), 1 / 3) falls an ulp short of 4 and would thin a peak scipy keeps. */
static double alwan_pk_dist(long dr, long dc, double p) {
    double const a = (double)labs(dr), b = (double)labs(dc);
    if (isinf(p)) return a > b ? a : b;
    if (p == 1.0) return a + b;
    if (p == 2.0) return sqrt(a * a + b * b);
    if (a == 0.0) return b;
    if (b == 0.0) return a;
    return pow(pow(a, p) + pow(b, p), 1.0 / p);
}

/* Sort the candidates (mask non-zero) of the w x h image v by value, thin them to spacing
 * min_distance and cut at max_out (0: no limit). Appends (row + r0, col + c0) pairs to
 * out; returns how many, or (size_t)-1 when out of memory. */
static size_t alwan_pk_select(size_t *out, double const *v, unsigned char const *mask, size_t w, size_t h, int u8,
                              size_t min_distance, double p_norm, size_t max_out, size_t r0, size_t c0) {
    size_t const n = w * h;
    size_t nc = 0, i, k, kept = 0;
    alwan_pk_cand *c;
    for (i = 0; i < n; i++) nc += mask[i] != 0;
    if (nc == 0) return 0;
    c = (alwan_pk_cand *)ALWAN_ALLOC(alwan_safe_array_size(nc, sizeof(alwan_pk_cand)), sizeof(double));
    if (!c) return (size_t)-1;
    for (i = 0, k = 0; i < n; i++) {
        if (!mask[i]) continue;
        /* np.argsort(-intensities, kind="stable"): on uint8, -x wraps to (256 - x) mod 256 */
        c[k].key = u8 ? (double)(unsigned char)(0u - (unsigned)v[i]) : -v[i];
        c[k].i = i;
        k++;
    }
    qsort(c, nc, sizeof(*c), alwan_pk_cmp);
    for (k = 0; k < nc; k++) {
        size_t const r = c[k].i / w, col = c[k].i % w;
        if (min_distance > 1) {
            /* greedy: rejected if closer than min_distance to a peak already kept */
            size_t j;
            int ok = 1;
            for (j = 0; j < kept && ok; j++) {
                long const dr = (long)r - (long)(out[2 * j] - r0), dc = (long)col - (long)(out[2 * j + 1] - c0);
                if (alwan_pk_dist(dr, dc, p_norm) < (double)min_distance) ok = 0;
            }
            if (!ok) continue;
        }
        out[2 * kept] = r + r0;
        out[2 * kept + 1] = col + c0;
        kept++;
        if (max_out && kept >= max_out) break;
    }
    ALWAN_FREE(c);
    return kept;
}

/* _get_peak_mask: candidates of the w x h image v (footprint fp of fw x fh) above the
 * threshold into out. region, when given, is the label mask (the trivial-image rule then
 * looks only at it, and its isolated pixels become peaks). tmp holds w h bytes. */
static void alwan_pk_mask(unsigned char *out, double const *v, size_t w, size_t h, unsigned char const *fp, size_t fw,
                          size_t fh, double threshold, int thr_f32, unsigned char const *region, unsigned char *tmp) {
    size_t const n = w * h;
    long const cr = (long)(fh / 2), cc = (long)(fw / 2);
    size_t fpn = 0, y, x, i;
    int all = 1;
    float const tf = (float)threshold;
    for (i = 0; i < fw * fh; i++) fpn += fp[i] != 0;
#define ALWAN_PK_ABOVE(val) (thr_f32 ? (float)(val) > tf : (val) > threshold)
    if (fw * fh == 1 || n == 1) {
        for (i = 0; i < n; i++) out[i] = (unsigned char)ALWAN_PK_ABOVE(v[i]);
        return;
    }
    (void)fpn;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double m = -DBL_MAX;
            size_t a, b;
            int any = 0;
            for (a = 0; a < fh; a++) {
                long sy = (long)y + (long)a - cr;
                if (sy < 0) sy = 0;
                if (sy >= (long)h) sy = (long)h - 1;
                for (b = 0; b < fw; b++) {
                    long sx = (long)x + (long)b - cc;
                    double s;
                    if (!fp[a * fw + b]) continue;
                    if (sx < 0) sx = 0;
                    if (sx >= (long)w) sx = (long)w - 1;
                    s = v[(size_t)sy * w + (size_t)sx];
                    if (!any || s > m) m = s;
                    any = 1;
                }
            }
            out[y * w + x] = (unsigned char)(any && v[y * w + x] == m);
        }
    }
    for (i = 0; i < n; i++)
        if ((!region || region[i]) && !out[i]) all = 0;
    if (all) {
        memset(out, 0, n);
        if (region) {
            /* isolated pixels of the region: the region minus its binary opening by the cross */
            unsigned char *er = tmp;
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) {
                    size_t const p = y * w + x;
                    /* erosion, border value 0 */
                    er[p] = (unsigned char)(region[p] && y > 0 && region[p - w] && y + 1 < h && region[p + w] && x > 0 &&
                                            region[p - 1] && x + 1 < w && region[p + 1]);
                }
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) {
                    size_t const p = y * w + x;
                    int const opened = er[p] || (y > 0 && er[p - w]) || (y + 1 < h && er[p + w]) || (x > 0 && er[p - 1]) ||
                                       (x + 1 < w && er[p + 1]);
                    if (region[p] && !opened) out[p] = 1;
                }
        }
    }
    for (i = 0; i < n; i++)
        if (out[i] && !ALWAN_PK_ABOVE(v[i])) out[i] = 0;
#undef ALWAN_PK_ABOVE
}

static alwan_status alwan_pk_run(size_t *peaks, size_t capacity, size_t *count, void const *src, size_t rs, size_t w, size_t h,
                                 alwan_peak_params const *params, int kind /* 0 f64, 1 f32, 2 u8 */) {
    alwan_peak_params const zero = { 0 };
    alwan_peak_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const md = p->min_distance ? p->min_distance : 1;
    size_t const n = w * h;
    double const pn = p->p_norm > 0.0 ? p->p_norm : INFINITY;
    size_t bw_r, bw_c, fw, fh, total = 0, x, y, i;
    double *v, vmin = DBL_MAX, vmax = -DBL_MAX, threshold;
    unsigned char *fp, *mask, *tmp;
    size_t *list;
    alwan_status st = ALWAN_OK;
    if (!count) return ALWAN_E_INVALID;
    *count = 0;
    if ((!peaks && capacity) || !src || w == 0 || h == 0 || n / w != h || rs / elem < w) return ALWAN_E_INVALID;
    if (p->p_norm < 0.0 || p->p_norm != p->p_norm || (p->p_norm > 0.0 && p->p_norm < 1.0)) return ALWAN_E_INVALID;
    if (p->footprint && (p->footprint_width == 0 || p->footprint_height == 0)) return ALWAN_E_INVALID;
    if (p->labels && p->labels_row_stride / sizeof(int) < w) return ALWAN_E_INVALID;
    if (md > 1000000) return ALWAN_E_RANGE;
    fw = p->footprint ? p->footprint_width : 2 * md + 1;
    fh = p->footprint ? p->footprint_height : 2 * md + 1;
    if (fw > 4096 || fh > 4096) return ALWAN_E_RANGE;
    if (p->exclude_border_given) {
        bw_r = p->exclude_border_rows;
        bw_c = p->exclude_border_cols;
    } else {
        bw_r = bw_c = md;   /* exclude_border=True: min_distance */
    }
    v = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    fp = (unsigned char *)ALWAN_ALLOC(fw * fh + 3 * n, sizeof(double));
    list = NULL;
    if (!v || !fp) {
        ALWAN_FREE(v);
        ALWAN_FREE(fp);
        return ALWAN_E_NOMEM;
    }
    mask = fp + fw * fh;
    tmp = mask + n;
    for (i = 0; i < fw * fh; i++) fp[i] = (unsigned char)(p->footprint ? p->footprint[i] != 0 : 1);
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * rs;
        for (x = 0; x < w; x++) {
            double const s = kind == 0 ? ((alwan_f64 const *)row)[x] : kind == 1 ? (double)((alwan_f32 const *)row)[x]
                                                                                 : (double)((unsigned char const *)row)[x];
            if (!(s == s)) {
                st = ALWAN_E_INVALID;
                goto done;
            }
            v[y * w + x] = s;
            if (s < vmin) vmin = s;
            if (s > vmax) vmax = s;
        }
    }
    /* _get_threshold. On float32 the product is float32's, and the comparison is made with
     * the threshold rounded to float32, as NumPy compares a float32 array with a Python float. */
    threshold = p->threshold_abs_given ? p->threshold_abs : vmin;
    if (p->threshold_rel_given) {
        double const r = kind == 1 ? (double)((float)p->threshold_rel * (float)vmax) : p->threshold_rel * vmax;
        if (r > threshold) threshold = r;
    }
    if (!p->labels) {
        size_t k;
        list = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(size_t)), sizeof(size_t));
        if (!list) {
            st = ALWAN_E_NOMEM;
            goto done;
        }
        alwan_pk_mask(mask, v, w, h, fp, fw, fh, threshold, kind == 1, NULL, tmp);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                if (y < bw_r || y + bw_r >= h || x < bw_c || x + bw_c >= w) mask[y * w + x] = 0;
        k = alwan_pk_select(list, v, mask, w, h, kind == 2, md, pn, p->num_peaks, 0, 0);
        if (k == (size_t)-1) {
            st = ALWAN_E_NOMEM;
            goto done;
        }
        total = k;
    } else {
        /* per label, in label order, inside the bounding box of the border-cleared labels */
        double const bg = kind == 2 ? 0.0 : kind == 1 ? -(double)FLT_MAX : -DBL_MAX;
        int maxl = 0;
        for (y = 0; y < h; y++) {
            int const *row = (int const *)((char const *)p->labels + y * p->labels_row_stride);
            for (x = 0; x < w; x++) {
                int const l = row[x];
                if (l < 0) {
                    st = ALWAN_E_INVALID;
                    goto done;
                }
                if (y < bw_r || y + bw_r >= h || x < bw_c || x + bw_c >= w) continue;
                if (l > maxl) maxl = l;
            }
        }
        {
            size_t *box = (size_t *)ALWAN_ALLOC(alwan_safe_array_size((size_t)maxl + 1, 4 * sizeof(size_t)), sizeof(size_t));
            double *sub = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
            unsigned char *reg = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size(n, 3), 1);
            int l;
            if (!box || !sub || !reg) {
                ALWAN_FREE(box);
                ALWAN_FREE(sub);
                ALWAN_FREE(reg);
                st = ALWAN_E_NOMEM;
                goto done;
            }
            for (l = 0; l <= maxl; l++) {
                box[4 * l] = box[4 * l + 2] = (size_t)-1;
                box[4 * l + 1] = box[4 * l + 3] = 0;
            }
            for (y = bw_r; y + bw_r < h; y++) {
                int const *row = (int const *)((char const *)p->labels + y * p->labels_row_stride);
                for (x = bw_c; x + bw_c < w; x++) {
                    int const lb = row[x];
                    if (lb <= 0) continue;
                    if (box[4 * lb] == (size_t)-1 || y < box[4 * lb]) box[4 * lb] = y;
                    if (y + 1 > box[4 * lb + 1]) box[4 * lb + 1] = y + 1;
                    if (box[4 * lb + 2] == (size_t)-1 || x < box[4 * lb + 2]) box[4 * lb + 2] = x;
                    if (x + 1 > box[4 * lb + 3]) box[4 * lb + 3] = x + 1;
                }
            }
            {
                /* a region's peaks lie in its box; the boxes may overlap, so the list is
                 * sized by their sum (background pixels can pass a negative threshold) */
                size_t need = n;
                for (l = 1, i = 0; l <= maxl; l++)
                    if (box[4 * l] != (size_t)-1) i += (box[4 * l + 1] - box[4 * l]) * (box[4 * l + 3] - box[4 * l + 2]);
                if (i > need) need = i;
                list = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(need, 2 * sizeof(size_t)), sizeof(size_t));
                if (!list) st = ALWAN_E_NOMEM;
            }
            for (l = 1; l <= maxl && st == ALWAN_OK; l++) {
                size_t const y0 = box[4 * l], y1 = box[4 * l + 1], x0 = box[4 * l + 2], x1 = box[4 * l + 3];
                size_t const bw = x1 - x0, bh = y1 - y0;
                unsigned char *rmask = reg, *pm = reg + n, *t2 = reg + 2 * n;
                size_t k;
                if (y0 == (size_t)-1) continue;
                for (y = 0; y < bh; y++) {
                    int const *row = (int const *)((char const *)p->labels + (y0 + y) * p->labels_row_stride);
                    for (x = 0; x < bw; x++) {
                        int const in = row[x0 + x] == l;   /* the ORIGINAL labels, as scikit-image */
                        rmask[y * bw + x] = (unsigned char)in;
                        sub[y * bw + x] = in ? v[(y0 + y) * w + x0 + x] : bg;
                    }
                }
                alwan_pk_mask(pm, sub, bw, bh, fp, fw, fh, threshold, kind == 1, rmask, t2);
                k = alwan_pk_select(list + 2 * total, sub, pm, bw, bh, kind == 2, md, pn, p->num_peaks_per_label, y0, x0);
                if (k == (size_t)-1) st = ALWAN_E_NOMEM;
                else total += k;
            }
            ALWAN_FREE(box);
            ALWAN_FREE(sub);
            ALWAN_FREE(reg);
            if (st != ALWAN_OK) goto done;
        }
        if (p->num_peaks && total > p->num_peaks) {
            size_t k;
            memset(mask, 0, n);
            for (i = 0; i < total; i++) mask[list[2 * i] * w + list[2 * i + 1]] = 1;
            k = alwan_pk_select(list, v, mask, w, h, kind == 2, md, pn, p->num_peaks, 0, 0);
            if (k == (size_t)-1) {
                st = ALWAN_E_NOMEM;
                goto done;
            }
            total = k;
        }
    }
    *count = total;
    for (i = 0; i < total && i < capacity; i++) {
        peaks[2 * i] = list[2 * i];
        peaks[2 * i + 1] = list[2 * i + 1];
    }
    if (total > capacity) st = ALWAN_E_RANGE;
done:
    ALWAN_FREE(v);
    ALWAN_FREE(fp);
    ALWAN_FREE(list);
    return st;
}

alwan_status alwan_peak_local_max_u8(size_t *peaks, size_t capacity, size_t *count, unsigned char const *src, size_t row_stride,
                                     size_t width, size_t height, alwan_peak_params const *params) {
    return alwan_pk_run(peaks, capacity, count, src, row_stride, width, height, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_peak_local_max_f64(size_t *peaks, size_t capacity, size_t *count, alwan_f64 const *src, size_t row_stride,
                                      size_t width, size_t height, alwan_peak_params const *params) {
    return alwan_pk_run(peaks, capacity, count, src, row_stride, width, height, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_peak_local_max_f32(size_t *peaks, size_t capacity, size_t *count, alwan_f32 const *src, size_t row_stride,
                                      size_t width, size_t height, alwan_peak_params const *params) {
    return alwan_pk_run(peaks, capacity, count, src, row_stride, width, height, params, 1);
}
#endif
