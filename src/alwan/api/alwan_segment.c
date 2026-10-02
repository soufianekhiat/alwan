/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_segment_{T} and _u8: an image divided into labelled regions.
 *
 *   CONNECTED  the connected components of equal pixels (every channel equal), 4- or
 *              8-connected, the background value left as 0 unless it is labelled too,
 *              numbered 1, 2, ... in the raster order of each component's first pixel.
 *              As scikit-image's measure.label, label for label (suite 223).
 *   WATERSHED  the image flooded from markers (the caller's, or its local minima, each
 *              minimal plateau a marker numbered in raster order), each pixel taking the
 *              label of the basin that reaches it first; optionally compact, or with the
 *              one-pixel lines between basins left 0. As scikit-image's
 *              segmentation.watershed, label for label (suite 224).
 *   SLIC       superpixels: k-means over position and the channels from a regular grid
 *              of seeds, each pixel searched only by the seeds within two grid steps, then
 *              pieces too small merged into a neighbour. As scikit-image's
 *              segmentation.slic without its Lab conversion, label for label (suite 227).
 *   FELZENSZWALB, QUICKSHIFT, CHAN_VESE, RANDOM_WALKER  in alwan_segment_ext.c (suite 293).
 *
 * CONNECTED is union-find over one raster pass (each pixel joined to its equal neighbours
 * already visited: left and above, and above-left and above-right when 8-connected), then
 * a second pass that numbers the roots as they are first met.
 *
 * WATERSHED follows scikit-image's _watershed_cy operation for operation, since the order
 * in which equal levels flood decides the labels: the image padded by one pixel, a binary
 * heap ordered by (level, age) with scikit-image's own sift steps (all markers age 0,
 * pushed in raster order), neighbours visited up, left, right, down and then up-left,
 * up-right, down-left, down-right, each push one age older, a pixel's level raised to its
 * parent's. The markers found when the caller gives none are scikit-image's local_minima:
 * plateaus whose other neighbours are all higher, a plateau at the image's maximum
 * disqualified where it touches the border.
 *
 * SLIC follows scikit-image's _slic_cython and _enforce_label_connectivity_cython: the
 * image rescaled to [0, 1] over all channels and multiplied by 1 / compactness, the seeds
 * scikit-image's regular_grid of the shape (1, height, width), the grid step a C float in
 * the spatial weight, the distances in the data's precision, the centres the running sums
 * over the raster divided by the counts, and the clean-up a breadth-first fill capped at
 * max_size_factor times the mean size that hands pieces under min_size_factor times it to
 * the last neighbouring piece it met.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static size_t alwan_sg_find(size_t *par, size_t p) {
    size_t r = p, t;
    while (par[r] != r) r = par[r];
    while (par[p] != r) {
        t = par[p];
        par[p] = r;
        p = t;
    }
    return r;
}

static void alwan_sg_union(size_t *par, size_t a, size_t b) {
    a = alwan_sg_find(par, a);
    b = alwan_sg_find(par, b);
    if (a == b) return;
    if (a < b) par[b] = a;   /* the earlier pixel is the root */
    else par[a] = b;
}

/* scikit-image's watershed heap: items compared by (value, age), swapped as its pointers
 * are. */
typedef struct {
    double value;
    long age;
    size_t index;
    size_t source;
} alwan_sg_item;

static int alwan_sg_smaller(alwan_sg_item const *a, alwan_sg_item const *b) {
    if (a->value != b->value) return a->value < b->value;
    return a->age < b->age;
}

static void alwan_sg_swap(alwan_sg_item *h, size_t a, size_t b) {
    alwan_sg_item const t = h[a];
    h[a] = h[b];
    h[b] = t;
}

static void alwan_sg_push(alwan_sg_item *h, size_t *items, alwan_sg_item e) {
    size_t child = (*items)++;
    h[child] = e;
    while (child > 0) {
        size_t const parent = (child + 1) / 2 - 1;
        if (!alwan_sg_smaller(&h[child], &h[parent])) break;
        alwan_sg_swap(h, parent, child);
        child = parent;
    }
}

static alwan_sg_item alwan_sg_pop(alwan_sg_item *h, size_t *items) {
    alwan_sg_item const top = h[0];
    size_t i = 0;
    *items -= 1;
    if (*items == 0) return top;
    alwan_sg_swap(h, 0, *items);
    for (;;) {
        size_t const l = 2 * i + 1, r = 2 * i + 2;
        size_t smallest = i;
        if (l >= *items) break;
        if (alwan_sg_smaller(&h[l], &h[i])) smallest = l;
        if (r < *items && alwan_sg_smaller(&h[r], &h[smallest])) smallest = r;
        if (smallest == i) break;
        alwan_sg_swap(h, i, smallest);
        i = smallest;
    }
    return top;
}

/* _diff_neighbors: true (and index made a line pixel) when a neighbour carries another label */
static int alwan_sg_diff(uint32_t const *out, long const *nb, int nn, signed char *mask, size_t index, uint32_t label) {
    int d;
    if (!mask[index]) return 1;
    for (d = 0; d < nn; d++) {
        size_t const q = (size_t)((long)index + nb[d]);
        if (mask[q] && out[q] && out[q] != label) {
            mask[index] = 0;
            return 1;
        }
    }
    return 0;
}

/* The watershed of the w x h image v (one channel) into labels. */
static alwan_status alwan_sg_watershed(uint32_t *labels, size_t labels_rs, size_t *count_out, double const *v, size_t w, size_t h,
                                       alwan_segment_params const *p) {
    size_t const pw = w + 2, ph = h + 2, np_ = pw * ph;
    int const eight = p->connectivity == 8, nn = eight ? 8 : 4;
    long const nb[8] = { -(long)pw, -1, 1, (long)pw, -(long)pw - 1, -(long)pw + 1, (long)pw - 1, (long)pw + 1 };
    int const compact = p->compactness > 0.0, wsl = p->watershed_line != 0;
    double *img = (double *)ALWAN_ALLOC(alwan_safe_array_size(np_, sizeof(double)), sizeof(double));
    uint32_t *out = (uint32_t *)ALWAN_ALLOC(alwan_safe_array_size(np_, sizeof(uint32_t)), sizeof(double));
    signed char *mask = (signed char *)ALWAN_ALLOC(np_, sizeof(double));
    alwan_sg_item *heap = NULL;
    size_t x, y, i, items = 0, heap_space = 0, maxlabel = 0;
    long age = 1;
    if (!img || !out || !mask) goto nomem;
    memset(out, 0, np_ * sizeof(uint32_t));
    memset(mask, 0, np_);
    for (i = 0; i < np_; i++) img[i] = 0.0;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            img[(y + 1) * pw + x + 1] = v[y * w + x];
            mask[(y + 1) * pw + x + 1] = 1;
        }
    if (p->markers) {
        if (p->markers_row_stride / sizeof(uint32_t) < w) {
            ALWAN_FREE(img);
            ALWAN_FREE(out);
            ALWAN_FREE(mask);
            return ALWAN_E_INVALID;
        }
        for (y = 0; y < h; y++) {
            uint32_t const *row = (uint32_t const *)((char const *)p->markers + y * p->markers_row_stride);
            for (x = 0; x < w; x++) out[(y + 1) * pw + x + 1] = row[x];
        }
    } else {
        /* local minima: flood each plateau once, in raster order; a minimum when no
         * neighbour is lower and, at the image's maximum, none lies outside */
        double vmax = v[0];
        size_t *queue = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(np_, sizeof(size_t)), sizeof(size_t));
        unsigned char *seen = (unsigned char *)ALWAN_ALLOC(np_, sizeof(double));
        uint32_t next = 0;
        if (!queue || !seen) {
            ALWAN_FREE(queue);
            ALWAN_FREE(seen);
            goto nomem;
        }
        memset(seen, 0, np_);
        for (i = 1; i < w * h; i++)
            if (v[i] > vmax) vmax = v[i];
        for (i = 0; i < np_; i++) {
            size_t head = 0, tail = 0, k;
            double hgt;
            int is_min = 1;
            if (!mask[i] || seen[i]) continue;
            hgt = img[i];
            queue[tail++] = i;
            seen[i] = 1;
            while (head < tail) {
                size_t const cur = queue[head++];
                int d;
                for (d = 0; d < nn; d++) {
                    size_t const q = (size_t)((long)cur + nb[d]);
                    if (!mask[q]) {
                        if (hgt == vmax) is_min = 0;   /* the padding sits at the maximum */
                        continue;
                    }
                    if (img[q] == hgt) {
                        if (!seen[q]) {
                            seen[q] = 1;
                            queue[tail++] = q;
                        }
                    } else if (img[q] < hgt) {
                        is_min = 0;
                    }
                }
            }
            if (is_min) {
                ++next;
                for (k = 0; k < tail; k++) out[queue[k]] = next;
            }
        }
        ALWAN_FREE(queue);
        ALWAN_FREE(seen);
    }
    /* the heap: every pixel is pushed at most once besides the markers, and markers once */
    heap_space = np_ + 1;
    heap = (alwan_sg_item *)ALWAN_ALLOC(alwan_safe_array_size(heap_space, sizeof(alwan_sg_item)), sizeof(double));
    if (!heap) goto nomem;
    for (i = 0; i < np_; i++) {
        if (out[i]) {
            alwan_sg_item e;
            e.value = img[i];
            e.age = 0;
            e.index = i;
            e.source = i;
            alwan_sg_push(heap, &items, e);
        }
    }
    while (items > 0) {
        alwan_sg_item const e = alwan_sg_pop(heap, &items);
        int d;
        if (compact || wsl) {
            if (out[e.index] && e.index != e.source) continue;
            if (compact || !alwan_sg_diff(out, nb, nn, mask, e.index, out[e.source])) out[e.index] = out[e.source];
        }
        for (d = 0; d < nn; d++) {
            size_t const q = (size_t)((long)e.index + nb[d]);
            alwan_sg_item ne;
            if (!mask[q] || out[q]) continue;
            age += 1;
            ne.value = img[q];
            if (compact) {
                double const dy = (double)((long)(q / pw) - (long)(e.source / pw));
                double const dx = (double)((long)(q % pw) - (long)(e.source % pw));
                double r = 0.0;
                r += dy * dy;
                r += dx * dx;
                ne.value += p->compactness * ALWAN_SQRT_F64(r);
            } else if (!wsl) {
                out[q] = out[e.index];
            }
            ne.age = age;
            ne.index = q;
            ne.source = e.source;
            if (ne.value < e.value) ne.value = e.value;
            if (items == heap_space) {   /* compact and line floods can queue a pixel more than once */
                alwan_sg_item *grown = (alwan_sg_item *)ALWAN_ALLOC(alwan_safe_array_size(2 * heap_space, sizeof(alwan_sg_item)), sizeof(double));
                if (!grown) goto nomem;
                memcpy(grown, heap, items * sizeof(alwan_sg_item));
                ALWAN_FREE(heap);
                heap = grown;
                heap_space *= 2;
            }
            alwan_sg_push(heap, &items, ne);
        }
    }
    for (y = 0; y < h; y++) {
        uint32_t *row = (uint32_t *)((char *)labels + y * labels_rs);
        for (x = 0; x < w; x++) {
            row[x] = out[(y + 1) * pw + x + 1];
            if (row[x] > maxlabel) maxlabel = row[x];
        }
    }
    if (count_out) *count_out = maxlabel;
    ALWAN_FREE(heap);
    ALWAN_FREE(img);
    ALWAN_FREE(out);
    ALWAN_FREE(mask);
    return ALWAN_OK;
nomem:
    ALWAN_FREE(heap);
    ALWAN_FREE(img);
    ALWAN_FREE(out);
    ALWAN_FREE(mask);
    return ALWAN_E_NOMEM;
}

static double alwan_sg_r(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

/* scikit-image's regular_grid over (1, h, w): the start and step along each of the three. */
static void alwan_sg_grid(long start[3], long stepv[3], double stepf[3], size_t h, size_t w, size_t n_points) {
    double const shape[3] = { 1.0, (double)h, (double)w };
    double sorted[3], steps[3], space = (double)h * (double)w;
    int order[3] = { 0, 1, 2 }, rank[3], i, j, dim;
    /* argsort of 3, stable (numpy sorts short arrays by insertion) */
    for (i = 1; i < 3; i++)
        for (j = i; j > 0 && shape[order[j - 1]] > shape[order[j]]; j--) {
            int const t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    for (i = 0; i < 3; i++) sorted[i] = shape[order[i]];
    for (i = 0; i < 3; i++) rank[order[i]] = i;
    if (space <= (double)n_points) {
        for (i = 0; i < 3; i++) start[i] = 0, stepv[i] = 1, stepf[i] = 1.0;
        return;
    }
    for (i = 0; i < 3; i++) steps[i] = ALWAN_POW_F64(space / (double)n_points, 1.0 / 3.0);
    if (sorted[0] < steps[0] || sorted[1] < steps[1] || sorted[2] < steps[2]) {
        for (dim = 0; dim < 3; dim++) {
            double sp = 1.0;
            steps[dim] = sorted[dim];
            for (i = dim + 1; i < 3; i++) sp *= sorted[i];
            for (i = dim + 1; i < 3; i++) steps[i] = ALWAN_POW_F64(sp / (double)n_points, 1.0 / (double)(3 - dim - 1));
            if (sorted[0] >= steps[0] && sorted[1] >= steps[1] && sorted[2] >= steps[2]) break;
        }
    }
    for (i = 0; i < 3; i++) {
        double const s = steps[rank[i]];
        start[i] = (long)ALWAN_FLOOR_F64(s / 2.0);
        stepv[i] = (long)nearbyint(s);   /* numpy's round: half to even */
        stepf[i] = (double)stepv[i];
    }
}

/* SLIC of the w x h x ch image v into labels. */
static alwan_status alwan_sg_slic(uint32_t *labels, size_t labels_rs, size_t *count_out, double *v, size_t w, size_t h, size_t ch,
                                  alwan_segment_params const *p, int kind) {
    int const f32 = kind == 1;
    size_t const n = w * h, nf = 3 + ch;
    size_t const n_req = p->n_segments ? p->n_segments : 100;
    size_t const max_iter = p->max_iterations ? p->max_iterations : 10;
    double const compactness = p->compactness > 0.0 ? p->compactness : 10.0;
    double const min_f = p->min_size_factor > 0.0 ? p->min_size_factor : 0.5;
    double const max_f = p->max_size_factor > 0.0 ? p->max_size_factor : 3.0;
    long start[3], stepv[3];
    double stepf[3], vmin, vmax, step_max, ratio, spatial_weight;
    float step32;
    size_t nseg, i, k, c, x, y, it;
    double *seg = NULL, *dist = NULL, *maxdc = NULL;
    size_t *cnt = NULL, *near = NULL, *conn = NULL, *coord = NULL;
    alwan_status st = ALWAN_E_NOMEM;
    alwan_sg_grid(start, stepv, stepf, h, w, n_req);
    nseg = (1 > start[0] ? (size_t)((1 - 1 - start[0]) / stepv[0] + 1) : 0) * ((h - 1 - (size_t)start[1]) / (size_t)stepv[1] + 1) *
           ((w - 1 - (size_t)start[2]) / (size_t)stepv[2] + 1);
    if (nseg == 0) return ALWAN_E_RANGE;
    /* rescale to [0, 1] over every channel, then by 1 / compactness */
    vmin = vmax = v[0];
    for (i = 1; i < n * ch; i++) {
        if (v[i] < vmin) vmin = v[i];
        if (v[i] > vmax) vmax = v[i];
    }
    ratio = 1.0 / compactness;
    for (i = 0; i < n * ch; i++) {
        double t = alwan_sg_r(v[i] - vmin, f32);
        if (vmax != vmin) t = alwan_sg_r(t / alwan_sg_r(vmax - vmin, f32), f32);
        v[i] = alwan_sg_r(t * alwan_sg_r(ratio, f32), f32);
    }
    seg = (double *)ALWAN_ALLOC(alwan_safe_array_size(nseg, (nf + 1) * sizeof(double)), sizeof(double));
    dist = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    cnt = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(nseg, sizeof(size_t)), sizeof(size_t));
    near = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(size_t)), sizeof(size_t));
    if (!seg || !dist || !cnt || !near) goto done;
    maxdc = seg + nseg * nf;
    conn = near + n;
    k = 0;
    for (y = (size_t)start[1]; y < h; y += (size_t)stepv[1])
        for (x = (size_t)start[2]; x < w; x += (size_t)stepv[2]) {
            seg[k * nf + 0] = (double)start[0];
            seg[k * nf + 1] = (double)y;
            seg[k * nf + 2] = (double)x;
            for (c = 0; c < ch; c++) seg[k * nf + 3 + c] = 0.0;
            maxdc[k] = 1.0;
            k++;
        }
    step_max = stepf[0] > stepf[1] ? stepf[0] : stepf[1];
    if (stepf[2] > step_max) step_max = stepf[2];
    step32 = (float)step_max;
    spatial_weight = alwan_sg_r(1.0 / (double)(float)(step32 * step32), f32);
    for (i = 0; i < n; i++) near[i] = 0;   /* start_label - 1 */
    for (it = 0; it < max_iter; it++) {
        int change = 0;
        for (i = 0; i < n; i++) dist[i] = f32 ? (double)HUGE_VALF : DBL_MAX;
        for (k = 0; k < nseg; k++) {
            double const cz = seg[k * nf], cy = seg[k * nf + 1], cx = seg[k * nf + 2];
            double lo, hi;
            long y0, y1, x0, x1;
            size_t zz;
            if (cy != cy) continue;   /* an emptied centre never takes a pixel */
            /* depth 1: z is 0, and the z range is kept only to decide whether the seed
             * reaches it */
            lo = alwan_sg_r(cz - 2.0 * (double)stepv[0], f32);
            hi = alwan_sg_r(cz + 2.0 * (double)stepv[0] + 1.0, f32);
            if (!((long)(0.0 > lo ? 0.0 : lo) < (long)(hi < 1.0 ? hi : 1.0))) continue;
            zz = 0;
            y0 = (long)(0.0 > alwan_sg_r(cy - 2.0 * (double)stepv[1], f32) ? 0.0 : alwan_sg_r(cy - 2.0 * (double)stepv[1], f32));
            y1 = (long)((double)h < alwan_sg_r(cy + 2.0 * (double)stepv[1] + 1.0, f32) ? (double)h : alwan_sg_r(cy + 2.0 * (double)stepv[1] + 1.0, f32));
            x0 = (long)(0.0 > alwan_sg_r(cx - 2.0 * (double)stepv[2], f32) ? 0.0 : alwan_sg_r(cx - 2.0 * (double)stepv[2], f32));
            x1 = (long)((double)w < alwan_sg_r(cx + 2.0 * (double)stepv[2] + 1.0, f32) ? (double)w : alwan_sg_r(cx + 2.0 * (double)stepv[2] + 1.0, f32));
            {
                double dz = alwan_sg_r(cz - (double)zz, f32);
                long yy, xx;
                dz = alwan_sg_r(dz * dz, f32);
                for (yy = y0; yy < y1; yy++) {
                    double dy = alwan_sg_r(cy - (double)yy, f32);
                    dy = alwan_sg_r(dy * dy, f32);
                    for (xx = x0; xx < x1; xx++) {
                        size_t const o = (size_t)yy * w + (size_t)xx;
                        double dx = alwan_sg_r(cx - (double)xx, f32), dc, d;
                        dx = alwan_sg_r(dx * dx, f32);
                        d = alwan_sg_r(alwan_sg_r(alwan_sg_r(dz + dy, f32) + dx, f32) * spatial_weight, f32);
                        dc = 0.0;
                        for (c = 0; c < ch; c++) {
                            double const t = alwan_sg_r(v[o * ch + c] - seg[k * nf + 3 + c], f32);
                            dc = alwan_sg_r(dc + alwan_sg_r(t * t, f32), f32);
                        }
                        if (p->slic_zero) dc = alwan_sg_r(dc / maxdc[k], f32);
                        d = alwan_sg_r(d + dc, f32);
                        if (dist[o] > d) {
                            near[o] = k + 1;
                            dist[o] = d;
                            change = 1;
                        }
                    }
                }
            }
        }
        if (!change) break;
        for (k = 0; k < nseg; k++) {
            cnt[k] = 0;
            for (c = 0; c < nf; c++) seg[k * nf + c] = 0.0;
        }
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                size_t const o = y * w + x;
                double *s;
                if (!near[o]) continue;   /* unreached pixels keep label 0, as scikit-image's would index -1 */
                k = near[o] - 1;
                s = seg + k * nf;
                cnt[k]++;
                s[0] = alwan_sg_r(s[0] + 0.0, f32);
                s[1] = alwan_sg_r(s[1] + (double)y, f32);
                s[2] = alwan_sg_r(s[2] + (double)x, f32);
                for (c = 0; c < ch; c++) s[3 + c] = alwan_sg_r(s[3 + c] + v[o * ch + c], f32);
            }
        for (k = 0; k < nseg; k++)
            for (c = 0; c < nf; c++) seg[k * nf + c] = alwan_sg_r(seg[k * nf + c] / (double)cnt[k], f32);
        if (p->slic_zero) {
            for (i = 0; i < n; i++) {
                double dc = 0.0;
                if (!near[i]) continue;
                k = near[i] - 1;
                for (c = 0; c < ch; c++) {
                    double const t = alwan_sg_r(v[i * ch + c] - seg[k * nf + 3 + c], f32);
                    dc = alwan_sg_r(dc + alwan_sg_r(t * t, f32), f32);
                }
                if (maxdc[k] < dc) maxdc[k] = dc;
            }
        }
    }
    /* connectivity */
    if (!p->keep_disconnected) {
        double const segsize = (double)n / (double)nseg;
        size_t const min_size = (size_t)(min_f * segsize), max_size = (size_t)(max_f * segsize);
        size_t cur = 1;
        static int const ddx[6] = { 1, -1, 0, 0, 0, 0 }, ddy[6] = { 0, 0, 1, -1, 0, 0 }, ddz[6] = { 0, 0, 0, 0, 1, -1 };
        coord = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(max_size + 1, sizeof(size_t)), sizeof(size_t));
        if (!coord) goto done;
        for (i = 0; i < n; i++) conn[i] = 0;
        for (i = 0; i < n; i++) {
            size_t adjacent, label, size = 1, visited = 0;
            if (near[i] == 0 || conn[i] > 0) continue;
            adjacent = cur;
            label = near[i];
            conn[i] = cur;
            coord[0] = i;
            while (visited < size && size < max_size) {
                int d;
                for (d = 0; d < 6; d++) {
                    long const yy = (long)(coord[visited] / w) + ddy[d], xx = (long)(coord[visited] % w) + ddx[d];
                    size_t q;
                    if (ddz[d] != 0 || xx < 0 || yy < 0 || xx >= (long)w || yy >= (long)h) continue;
                    q = (size_t)yy * w + (size_t)xx;
                    if (near[q] == label && conn[q] == 0) {
                        conn[q] = cur;
                        coord[size++] = q;
                        if (size >= max_size) break;
                    } else if (conn[q] > 0 && conn[q] != cur) {
                        adjacent = conn[q];
                    }
                }
                visited++;
            }
            if (size < min_size) {
                for (k = 0; k < size; k++) conn[coord[k]] = adjacent;
            } else {
                cur++;
            }
        }
        memcpy(near, conn, n * sizeof(size_t));
    }
    {
        size_t maxl = 0;
        for (y = 0; y < h; y++) {
            uint32_t *row = (uint32_t *)((char *)labels + y * labels_rs);
            for (x = 0; x < w; x++) {
                row[x] = (uint32_t)near[y * w + x];
                if (row[x] > maxl) maxl = row[x];
            }
        }
        if (count_out) *count_out = maxl;
    }
    st = ALWAN_OK;
done:
    ALWAN_FREE(coord);
    ALWAN_FREE(seg);
    ALWAN_FREE(dist);
    ALWAN_FREE(cnt);
    ALWAN_FREE(near);
    return st;
}

static alwan_status alwan_sg_run(uint32_t *labels, size_t labels_rs, size_t *count_out, void const *src, size_t src_rs, size_t ch,
                                 size_t w, size_t h, alwan_segment_method method, alwan_segment_params const *params, int kind) {
    alwan_segment_params const zero = { 0 };
    alwan_segment_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const n = w * h;
    int const eight = method == ALWAN_SEGMENT_WATERSHED ? p->connectivity == 8 : p->connectivity != 4;
    double *v;
    size_t *par;
    unsigned char *bg;
    size_t x, y, c, i, count = 0;
    if (!labels || !src || w == 0 || h == 0 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || labels_rs / sizeof(uint32_t) < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_SEGMENT_RANDOM_WALKER) return ALWAN_E_INVALID;
    if (method == ALWAN_SEGMENT_SLIC && (!(p->compactness >= 0.0) || !(p->min_size_factor >= 0.0) || !(p->max_size_factor >= 0.0) ||
                                         p->max_iterations > 100000))
        return ALWAN_E_RANGE;
    if (method == ALWAN_SEGMENT_WATERSHED && (ch != 1 || !(p->compactness >= 0.0))) return ALWAN_E_INVALID;
    if (p->connectivity != 0 && p->connectivity != 4 && p->connectivity != 8) return ALWAN_E_INVALID;
    if (n >= (size_t)0xFFFFFFFFu) return ALWAN_E_RANGE;
    v = (double *)ALWAN_ALLOC(alwan_safe_array_size(n * ch, sizeof(double)), sizeof(double));
    par = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(size_t)), sizeof(size_t));
    bg = (unsigned char *)ALWAN_ALLOC(n, sizeof(double));
    if (!v || !par || !bg) {
        ALWAN_FREE(v);
        ALWAN_FREE(par);
        ALWAN_FREE(bg);
        return ALWAN_E_NOMEM;
    }
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w * ch; x++) {
            double const s = kind == 0 ? ((alwan_f64 const *)row)[x] : kind == 1 ? (double)((alwan_f32 const *)row)[x]
                                                                               : (double)((unsigned char const *)row)[x];
            if (s != s) {
                ALWAN_FREE(v);
                ALWAN_FREE(par);
                ALWAN_FREE(bg);
                return ALWAN_E_INVALID;
            }
            v[y * w * ch + x] = s;
        }
    }
    if (method >= ALWAN_SEGMENT_FELZENSZWALB) {
        alwan_status const st = alwan__segment_ext(labels, labels_rs, count_out, v, w, h, ch, method, p, kind);
        ALWAN_FREE(v);
        ALWAN_FREE(par);
        ALWAN_FREE(bg);
        return st;
    }
    if (method == ALWAN_SEGMENT_SLIC) {
        alwan_status st;
        if (kind == 2)
            for (i = 0; i < n * ch; i++) v[i] = v[i] * (1.0 / 255.0);   /* img_as_float */
        st = alwan_sg_slic(labels, labels_rs, count_out, v, w, h, ch, p, kind);
        ALWAN_FREE(v);
        ALWAN_FREE(par);
        ALWAN_FREE(bg);
        return st;
    }
    if (method == ALWAN_SEGMENT_WATERSHED) {
        alwan_status const st = alwan_sg_watershed(labels, labels_rs, count_out, v, w, h, p);
        ALWAN_FREE(v);
        ALWAN_FREE(par);
        ALWAN_FREE(bg);
        return st;
    }
    (void)eight;
    for (i = 0; i < n; i++) {
        int is_bg = !p->label_background;
        for (c = 0; c < ch && is_bg; c++) is_bg = v[i * ch + c] == p->background;
        bg[i] = (unsigned char)is_bg;
        par[i] = i;
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            size_t const i0 = y * w + x;
            static int const dx[4] = { -1, 0, -1, 1 }, dy[4] = { 0, -1, -1, -1 };
            int d;
            if (bg[i0]) continue;
            for (d = 0; d < (eight ? 4 : 2); d++) {
                long const sx = (long)x + dx[d], sy = (long)y + dy[d];
                size_t q;
                int same = 1;
                if (sx < 0 || sy < 0 || sx >= (long)w) continue;
                q = (size_t)sy * w + (size_t)sx;
                if (bg[q]) continue;
                for (c = 0; c < ch && same; c++) same = v[q * ch + c] == v[i0 * ch + c];
                if (same) alwan_sg_union(par, i0, q);
            }
        }
    }
    /* number the roots in raster order; a root is its component's first pixel */
    for (y = 0; y < h; y++) {
        uint32_t *row = (uint32_t *)((char *)labels + y * labels_rs);
        for (x = 0; x < w; x++) {
            size_t const i0 = y * w + x;
            size_t r;
            if (bg[i0]) {
                row[x] = 0;
                continue;
            }
            r = alwan_sg_find(par, i0);
            if (r == i0) {
                row[x] = (uint32_t)++count;
            } else {
                row[x] = ((uint32_t const *)((char const *)labels + (r / w) * labels_rs))[r % w];
            }
        }
    }
    if (count_out) *count_out = count;
    ALWAN_FREE(v);
    ALWAN_FREE(par);
    ALWAN_FREE(bg);
    return ALWAN_OK;
}

alwan_status alwan_segment_u8(uint32_t *labels, size_t labels_row_stride, size_t *count_out, unsigned char const *src, size_t src_row_stride,
                              size_t channels, size_t width, size_t height, alwan_segment_method method, alwan_segment_params const *params) {
    return alwan_sg_run(labels, labels_row_stride, count_out, src, src_row_stride, channels, width, height, method, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_segment_f64(uint32_t *labels, size_t labels_row_stride, size_t *count_out, alwan_f64 const *src, size_t src_row_stride,
                               size_t channels, size_t width, size_t height, alwan_segment_method method, alwan_segment_params const *params) {
    return alwan_sg_run(labels, labels_row_stride, count_out, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_segment_f32(uint32_t *labels, size_t labels_row_stride, size_t *count_out, alwan_f32 const *src, size_t src_row_stride,
                               size_t channels, size_t width, size_t height, alwan_segment_method method, alwan_segment_params const *params) {
    return alwan_sg_run(labels, labels_row_stride, count_out, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
