/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * A palette from a photograph: median cut (Heckbert, "Color Image Quantization for Frame
 * Buffer Display", SIGGRAPH 1982) as Pillow implements it (src/libImaging/Quant.c,
 * quantize() with kmeans 0, which Image.quantize(method=MEDIANCUT) calls). Pillow is
 * under the HPND licence; this follows its arithmetic, and suite 188 holds the palette
 * and the index map to it exactly.
 *
 *   1. count the distinct colours; if there are more than 65536, drop the low bit of
 *      every channel until there are not (Pillow's hash rescaling, whose outcome is
 *      the smallest such shift)
 *   2. keep a max-heap of boxes by pixel count, starting from one box holding every
 *      colour; take the largest, skipping boxes of a single colour, and split it on
 *      the channel whose range, weighted 77 : 150 : 29, is largest, at the count
 *      median, keeping every pixel of one channel value on the same side. The upper
 *      values go to the first child
 *   3. stop after max_colors - 1 splits or when nothing can be split
 *   4. the palette is the leaves in order, first child before second, each the mean
 *      of its ORIGINAL pixels rounded as (int)(0.5 + mean)
 *   5. each pixel maps to the nearest entry in squared distance, searched from its
 *      own box's entry in order of distance from it; a tie keeps the first found
 *
 * Pillow's heap is reproduced as well as its splits: which of two boxes with the same
 * pixel count is split first depends on the heap, and changes the palette when the
 * splits run out before the ties do.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned int key;      /* scaled colour, r << 16 | g << 8 | b */
    unsigned int count;
} alwan_mc_colour;

typedef struct {
    size_t first, n;       /* a run of the colour array */
    unsigned int pixels;   /* pixel count */
    int volume;            /* -1 until computed */
    int left, right;       /* children, -1 for a leaf */
} alwan_mc_box;

static int alwan_mc_cmp_u32(void const *a, void const *b) {
    unsigned int const x = *(unsigned int const *)a, y = *(unsigned int const *)b;
    return x < y ? -1 : x > y;
}

static unsigned int alwan_mc_scale_key(unsigned int k, int s) {
    return (((k >> 16) & 255u) >> s) << 16 | (((k >> 8) & 255u) >> s) << 8 | ((k & 255u) >> s);
}

static unsigned int alwan_mc_ch(unsigned int key, int axis) {
    return (key >> (16 - 8 * axis)) & 255u;
}

/* Orders a run of colours by one channel, descending: a counting sort over its 256
 * values through tmp (no shared state, so calls on different threads do not meet). */
static void alwan_mc_sort_axis_desc(alwan_mc_colour *c, size_t n, int axis, alwan_mc_colour *tmp) {
    size_t start[257], i;
    unsigned int v;
    memset(start, 0, sizeof start);
    for (i = 0; i < n; i++) start[256u - alwan_mc_ch(c[i].key, axis)]++;
    for (v = 1; v <= 256u; v++) start[v] += start[v - 1];
    for (i = 0; i < n; i++) tmp[start[255u - alwan_mc_ch(c[i].key, axis)]++] = c[i];
    memcpy(c, tmp, n * sizeof *c);
}

static void alwan_mc_range(alwan_mc_colour const *c, size_t n, int axis, unsigned int *lo, unsigned int *hi) {
    size_t i;
    *lo = 255u; *hi = 0u;
    for (i = 0; i < n; i++) {
        unsigned int const v = alwan_mc_ch(c[i].key, axis);
        if (v < *lo) *lo = v;
        if (v > *hi) *hi = v;
    }
}

static int alwan_mc_volume(alwan_mc_box *b, alwan_mc_colour const *c) {
    if (b->volume < 0) {
        unsigned int lo, hi;
        int v = 1, a;
        for (a = 0; a < 3; a++) {
            alwan_mc_range(c + b->first, b->n, a, &lo, &hi);
            v *= (int)(hi - lo + 1u);
        }
        b->volume = b->n ? v : 0;
    }
    return b->volume;
}

/* ImagingQuantHeap: a 1-based binary max-heap on pixel count. */
typedef struct {
    int *slot;
    size_t count;
} alwan_mc_heap;

static int alwan_mc_hcmp(alwan_mc_box const *boxes, int a, int b) {
    return (int)boxes[a].pixels - (int)boxes[b].pixels;
}

static void alwan_mc_heap_add(alwan_mc_heap *h, alwan_mc_box const *boxes, int val) {
    size_t k = ++h->count;
    while (k != 1) {
        if (alwan_mc_hcmp(boxes, val, h->slot[k / 2]) <= 0) break;
        h->slot[k] = h->slot[k / 2];
        k >>= 1;
    }
    h->slot[k] = val;
}

static int alwan_mc_heap_remove(alwan_mc_heap *h, alwan_mc_box const *boxes, int *out) {
    size_t k, l;
    int v;
    if (!h->count) return 0;
    *out = h->slot[1];
    v = h->slot[h->count--];
    for (k = 1; k * 2 <= h->count; k = l) {
        l = k * 2;
        if (l < h->count && alwan_mc_hcmp(boxes, h->slot[l], h->slot[l + 1]) < 0) l++;
        if (alwan_mc_hcmp(boxes, v, h->slot[l]) > 0) break;
        h->slot[k] = h->slot[l];
    }
    h->slot[k] = v;
    return 1;
}

/* split(): choose the axis, order the box's colours on it descending, and cut. The first
 * `left` colours (the upper values) stay first; returns how many. */
static size_t alwan_mc_split(alwan_mc_colour *c, alwan_mc_box const *b, unsigned int *left_pixels,
                             alwan_mc_colour *tmp) {
    static int const weight[3] = { 77, 150, 29 };
    int best = -1, axis = 0, a;
    unsigned int acc = 0, split_value;
    size_t i, nl;
    for (a = 0; a < 3; a++) {
        unsigned int lo, hi;
        int f;
        alwan_mc_range(c + b->first, b->n, a, &lo, &hi);
        f = (int)(hi - lo) * weight[a];
        if (best < f) { best = f; axis = a; }
    }
    alwan_mc_sort_axis_desc(c + b->first, b->n, axis, tmp);
    c += b->first;
    for (i = 0; i < b->n;) {
        acc += c[i].count;
        i++;
        if ((unsigned long long)acc * 2u > b->pixels) break;
    }
    if (i < b->n) {
        split_value = alwan_mc_ch(c[i - 1].key, axis);
        while (i < b->n && alwan_mc_ch(c[i].key, axis) == split_value) { acc += c[i].count; i++; }
    }
    nl = i;
    if (nl == b->n) {   /* nothing went right: move the lowest value's colours across */
        split_value = alwan_mc_ch(c[b->n - 1].key, axis);
        while (nl > 0 && alwan_mc_ch(c[nl - 1].key, axis) == split_value) { nl--; acc -= c[nl].count; }
    }
    *left_pixels = acc;
    return nl;
}

static void alwan_mc_leaves(alwan_mc_box const *boxes, int node, int *order, size_t *n) {
    if (boxes[node].left >= 0) {
        alwan_mc_leaves(boxes, boxes[node].left, order, n);
        alwan_mc_leaves(boxes, boxes[node].right, order, n);
    } else if (boxes[node].n) {
        order[(*n)++] = node;
    }
}

typedef struct {
    unsigned int dist;
    unsigned int index;
} alwan_mc_dist;

static int alwan_mc_cmp_dist(void const *a, void const *b) {
    alwan_mc_dist const *x = (alwan_mc_dist const *)a, *y = (alwan_mc_dist const *)b;
    if (x->dist == y->dist) return x->index < y->index ? -1 : 1;
    return x->dist < y->dist ? -1 : 1;
}

static unsigned int alwan_mc_d2(unsigned char const *p, unsigned char const *q) {
    int const dr = (int)p[0] - (int)q[0], dg = (int)p[1] - (int)q[1], db = (int)p[2] - (int)q[2];
    return (unsigned int)(dr * dr + dg * dg + db * db);
}

static alwan_status alwan_mc_run(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                 unsigned char const *rgb, size_t pixel_stride, size_t count, size_t max_colors) {
    unsigned int *keys = NULL;
    alwan_mc_colour *col = NULL, *tmp = NULL;
    alwan_mc_box *boxes = NULL;
    alwan_mc_heap heap = { NULL, 0 };
    int *order = NULL;
    double *sum = NULL;
    alwan_mc_dist *sorted = NULL;
    size_t ncol = 0, nboxes = 0, nleaves = 0, i, j, splits;
    int scale = 0;
    alwan_status st = ALWAN_E_NOMEM;
    if (!palette_out || !count_out || !rgb) return ALWAN_E_INVALID;
    if (count == 0 || max_colors == 0 || pixel_stride < 3) return ALWAN_E_INVALID;
    if (max_colors > 65536u || count > 0xFFFFFFFFu) return ALWAN_E_RANGE;

    keys = (unsigned int *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(unsigned int)), sizeof(unsigned int));
    if (!keys) goto done;
    for (;;) {   /* 1. distinct colours at the smallest workable scale */
        for (i = 0; i < count; i++) {
            unsigned char const *p = rgb + i * pixel_stride;
            keys[i] = alwan_mc_scale_key((unsigned int)p[0] << 16 | (unsigned int)p[1] << 8 | p[2], scale);
        }
        qsort(keys, count, sizeof *keys, alwan_mc_cmp_u32);
        for (ncol = 0, i = 0; i < count; i++) if (i == 0 || keys[i] != keys[i - 1]) ncol++;
        if (ncol <= 65536u) break;
        scale++;
    }
    col = (alwan_mc_colour *)ALWAN_ALLOC(ncol * sizeof *col, sizeof(unsigned int));
    tmp = (alwan_mc_colour *)ALWAN_ALLOC(ncol * sizeof *tmp, sizeof(unsigned int));
    boxes = (alwan_mc_box *)ALWAN_ALLOC((2 * max_colors + 1) * sizeof *boxes, sizeof(size_t));
    heap.slot = (int *)ALWAN_ALLOC((2 * max_colors + 2) * sizeof(int), sizeof(int));
    if (!col || !tmp || !boxes || !heap.slot) goto done;
    for (j = 0, i = 0; i < count; i++) {
        if (i == 0 || keys[i] != keys[i - 1]) { col[j].key = keys[i]; col[j].count = 0; j++; }
        col[j - 1].count++;
    }

    /* 2-3. the median cut */
    boxes[0].first = 0; boxes[0].n = ncol; boxes[0].pixels = (unsigned int)count;
    boxes[0].volume = -1; boxes[0].left = boxes[0].right = -1;
    nboxes = 1;
    alwan_mc_heap_add(&heap, boxes, 0);
    for (splits = max_colors - 1; splits > 0; splits--) {
        int node;
        unsigned int lp;
        size_t nl;
        do {
            if (!alwan_mc_heap_remove(&heap, boxes, &node)) goto cut;
        } while (alwan_mc_volume(&boxes[node], col) == 1);
        nl = alwan_mc_split(col, &boxes[node], &lp, tmp);
        boxes[nboxes].first = boxes[node].first; boxes[nboxes].n = nl; boxes[nboxes].pixels = lp;
        boxes[nboxes + 1].first = boxes[node].first + nl; boxes[nboxes + 1].n = boxes[node].n - nl;
        boxes[nboxes + 1].pixels = boxes[node].pixels - lp;
        for (i = nboxes; i < nboxes + 2; i++) { boxes[i].volume = -1; boxes[i].left = boxes[i].right = -1; }
        boxes[node].left = (int)nboxes;
        boxes[node].right = (int)nboxes + 1;
        alwan_mc_heap_add(&heap, boxes, (int)nboxes);
        alwan_mc_heap_add(&heap, boxes, (int)nboxes + 1);
        nboxes += 2;
    }
cut:
    order = (int *)ALWAN_ALLOC(nboxes * sizeof(int), sizeof(int));
    if (!order) goto done;
    alwan_mc_leaves(boxes, 0, order, &nleaves);

    /* 4. each distinct colour's leaf, then the means of the original pixels */
    sum = (double *)ALWAN_ALLOC(nleaves * 4 * sizeof(double), sizeof(double));
    if (!sum) goto done;
    /* each colour's count becomes its leaf's index, then the colours are sorted by key
     * (the first member) so a pixel finds its leaf by bisection */
    for (j = 0; j < nleaves; j++) {
        alwan_mc_box const *b = &boxes[order[j]];
        for (i = 0; i < b->n; i++) col[b->first + i].count = (unsigned int)j;
    }
    qsort(col, ncol, sizeof *col, alwan_mc_cmp_u32);
    memset(sum, 0, nleaves * 4 * sizeof(double));
    for (i = 0; i < count; i++) {
        unsigned char const *p = rgb + i * pixel_stride;
        unsigned int const k = alwan_mc_scale_key((unsigned int)p[0] << 16 | (unsigned int)p[1] << 8 | p[2], scale);
        size_t lo = 0, hi = ncol;
        while (hi - lo > 1) { size_t const mid = (lo + hi) / 2; if (col[mid].key <= k) lo = mid; else hi = mid; }
        keys[i] = col[lo].count;   /* the pixel's leaf */
        sum[4 * col[lo].count] += p[0];
        sum[4 * col[lo].count + 1] += p[1];
        sum[4 * col[lo].count + 2] += p[2];
        sum[4 * col[lo].count + 3] += 1.0;
    }
    for (j = 0; j < nleaves; j++) {
        for (i = 0; i < 3; i++) palette_out[3 * j + i] = (unsigned char)(int)(.5 + sum[4 * j + i] / sum[4 * j + 3]);
    }
    *count_out = nleaves;

    /* 5. the index map */
    if (index_out) {
        sorted = (alwan_mc_dist *)ALWAN_ALLOC(nleaves * nleaves * sizeof *sorted, sizeof(unsigned int));
        if (!sorted) goto done;
        for (j = 0; j < nleaves; j++) {
            for (i = 0; i < nleaves; i++) {
                sorted[j * nleaves + i].dist = alwan_mc_d2(palette_out + 3 * j, palette_out + 3 * i);
                sorted[j * nleaves + i].index = (unsigned int)i;
            }
            qsort(sorted + j * nleaves, nleaves, sizeof *sorted, alwan_mc_cmp_dist);
        }
        for (i = 0; i < count; i++) {
            unsigned char const *p = rgb + i * pixel_stride;
            unsigned int const box = keys[i];
            unsigned int best = box, bestd = alwan_mc_d2(palette_out + 3 * box, p);
            unsigned int const bound = bestd << 2;
            alwan_mc_dist const *row = sorted + (size_t)box * nleaves;
            for (j = 0; j < nleaves && row[j].dist <= bound; j++) {
                unsigned int const d = alwan_mc_d2(palette_out + 3 * row[j].index, p);
                if (d < bestd) { bestd = d; best = row[j].index; }
            }
            index_out[i] = best;
        }
    }
    st = ALWAN_OK;
done:
    if (keys) ALWAN_FREE(keys);
    if (tmp) ALWAN_FREE(tmp);
    if (col) ALWAN_FREE(col);
    if (boxes) ALWAN_FREE(boxes);
    if (heap.slot) ALWAN_FREE(heap.slot);
    if (order) ALWAN_FREE(order);
    if (sum) ALWAN_FREE(sum);
    if (sorted) ALWAN_FREE(sorted);
    return st;
}

alwan_status alwan_quantize_u8(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                               unsigned char const *rgb, size_t pixel_stride, size_t count, size_t max_colors,
                               alwan_quantize_method method) {
    switch (method) {
    case ALWAN_QUANTIZE_MEDIAN_CUT:
        return alwan_mc_run(palette_out, count_out, index_out, rgb, pixel_stride, count, max_colors);
    case ALWAN_QUANTIZE_FAST_OCTREE:
        return alwan__quantize_octree(palette_out, count_out, index_out, rgb, pixel_stride, count, max_colors);
    case ALWAN_QUANTIZE_MAX_COVERAGE:
        return alwan__quantize_max_coverage(palette_out, count_out, index_out, rgb, pixel_stride, count, max_colors);
    default:
        return ALWAN_E_INVALID;
    }
}
