/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * A fast octree palette as Pillow computes it (src/libImaging/QuantOctree.c, Oliver
 * Tonnhofer 2010, MIT licence), which Image.quantize(method=FASTOCTREE) calls. Two
 * levels of an RGB octree stand in for the tree:
 *
 *   1. every pixel is counted, and its channels summed, in a fine cube of 16 x 16 x 16
 *      cells (the top four bits of each channel); the fine cube summed four by four
 *      gives a coarse cube of 4 x 4 x 4 cells
 *   2. every occupied coarse cell gets a palette entry (at most max_colors of them, the
 *      most populated first) and the rest of the palette goes to the most populated
 *      fine cells. A fine cell that gets an entry has its pixels taken out of its coarse
 *      cell; a coarse cell left empty frees its entry for one more fine cell, until no
 *      more empty
 *   3. the palette is the coarse entries, most populated first, then the fine ones, each
 *      the mean of its cell's pixels, taken in float and truncated as Pillow does
 *   4. a pixel maps to its fine cell's entry when the cell has one, and otherwise to its
 *      coarse cell's entry
 *
 * Pillow sorts the cells with qsort, which does not order cells of equal count the same
 * way on every C library; this sorts equal counts by cell index, the order a stable sort
 * (glibc's) gives. When max_colors is below the number of occupied coarse cells some of
 * them get no entry, and Pillow maps their pixels to entry 0, the default of its lookup
 * table, wherever that is; this maps them to the nearest entry in squared distance.
 * Pillow also pads its palette to max_colors with black entries no pixel maps to;
 * count_out here counts the entries that hold pixels.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned int count;
    unsigned int cell;
    unsigned long long r, g, b;
} alwan_oq_bucket;

static unsigned int alwan_oq_fine(unsigned int r, unsigned int g, unsigned int b) {
    return (r >> 4) << 8 | (g >> 4) << 4 | (b >> 4);
}

static unsigned int alwan_oq_coarse(unsigned int r, unsigned int g, unsigned int b) {
    return (r >> 6) << 4 | (g >> 6) << 2 | (b >> 6);
}

/* Pillow's avg_color_from_color_bucket: float sums over a float count, truncated. */
static void alwan_oq_mean(alwan_oq_bucket const *bk, unsigned char c[3]) {
    if (bk->count == 0) {
        c[0] = c[1] = c[2] = 0;
    } else {
        float const n = (float)bk->count;
        int const v[3] = { (int)((float)bk->r / n), (int)((float)bk->g / n), (int)((float)bk->b / n) };
        int k;
        for (k = 0; k < 3; k++) c[k] = (unsigned char)(v[k] < 0 ? 0 : v[k] > 255 ? 255 : v[k]);
    }
}

static int alwan_oq_cmp(void const *x, void const *y) {
    alwan_oq_bucket const *a = (alwan_oq_bucket const *)x, *b = (alwan_oq_bucket const *)y;
    if (a->count != b->count) return a->count > b->count ? -1 : 1;
    return a->cell < b->cell ? -1 : a->cell > b->cell ? 1 : 0;
}

static size_t alwan_oq_used(alwan_oq_bucket const *cube, size_t n) {
    size_t i, u = 0;
    for (i = 0; i < n; i++) u += cube[i].count > 0;
    return u;
}

static void alwan_oq_subtract(alwan_oq_bucket *coarse, alwan_oq_bucket const *fine_sorted, size_t from, size_t to) {
    size_t i;
    for (i = from; i < to; i++) {
        alwan_oq_bucket const *s = &fine_sorted[i];
        alwan_oq_bucket *m;
        unsigned char c[3];
        if (s->count == 0) continue;
        alwan_oq_mean(s, c);
        m = &coarse[alwan_oq_coarse(c[0], c[1], c[2])];
        m->count -= s->count;
        m->r -= s->r;
        m->g -= s->g;
        m->b -= s->b;
    }
}

alwan_status alwan__quantize_octree(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                    unsigned char const *rgb, size_t pixel_stride, size_t count, size_t max_colors) {
    enum { NF = 4096, NC = 64 };
    alwan_oq_bucket *fine, *fine_sorted, *pal;
    alwan_oq_bucket coarse[NC], coarse_sorted[NC];
    unsigned int coarse_lookup[NC];
    unsigned int *lookup;
    unsigned char *has_entry, *fine_set;
    size_t n_coarse, n_fine, n_real, i;
    if (!palette_out || !count_out || !rgb || count == 0 || max_colors == 0 || pixel_stride < 3) return ALWAN_E_INVALID;
    if (max_colors > 65536) return ALWAN_E_RANGE;
    fine = (alwan_oq_bucket *)ALWAN_ALLOC(sizeof(alwan_oq_bucket) * (3 * NF + NC), sizeof(unsigned long long));
    lookup = (unsigned int *)ALWAN_ALLOC(sizeof(unsigned int) * NF, sizeof(unsigned int));
    has_entry = (unsigned char *)ALWAN_ALLOC(NC + NF, 1);
    if (!fine || !lookup || !has_entry) {
        if (fine) ALWAN_FREE(fine);
        if (lookup) ALWAN_FREE(lookup);
        if (has_entry) ALWAN_FREE(has_entry);
        return ALWAN_E_NOMEM;
    }
    fine_sorted = fine + NF;
    pal = fine_sorted + NF;
    memset(fine, 0, sizeof(alwan_oq_bucket) * NF);
    memset(coarse, 0, sizeof(coarse));
    for (i = 0; i < count; i++) {
        unsigned char const *p = rgb + i * pixel_stride;
        alwan_oq_bucket *bk = &fine[alwan_oq_fine(p[0], p[1], p[2])];
        bk->count++;
        bk->r += p[0];
        bk->g += p[1];
        bk->b += p[2];
    }
    for (i = 0; i < NF; i++) {
        unsigned int const r = (unsigned int)(i >> 8), g = (unsigned int)(i >> 4) & 15u, b = (unsigned int)i & 15u;
        alwan_oq_bucket *c = &coarse[(r >> 2) << 4 | (g >> 2) << 2 | (b >> 2)];
        fine[i].cell = (unsigned int)i;
        c->count += fine[i].count;
        c->r += fine[i].r;
        c->g += fine[i].g;
        c->b += fine[i].b;
    }
    for (i = 0; i < NC; i++) coarse[i].cell = (unsigned int)i;

    n_coarse = alwan_oq_used(coarse, NC);
    if (n_coarse > max_colors) n_coarse = max_colors;
    n_fine = max_colors - n_coarse;
    if (n_fine > NF) n_fine = NF;
    memcpy(fine_sorted, fine, sizeof(alwan_oq_bucket) * NF);
    qsort(fine_sorted, NF, sizeof(alwan_oq_bucket), alwan_oq_cmp);
    alwan_oq_subtract(coarse, fine_sorted, 0, n_fine);
    while (n_coarse > alwan_oq_used(coarse, NC)) {
        size_t const done = n_fine;
        n_coarse = alwan_oq_used(coarse, NC);
        n_fine = max_colors - n_coarse;
        if (n_fine > NF) n_fine = NF;
        alwan_oq_subtract(coarse, fine_sorted, done, n_fine);
    }
    memcpy(coarse_sorted, coarse, sizeof(coarse));
    qsort(coarse_sorted, NC, sizeof(alwan_oq_bucket), alwan_oq_cmp);
    memcpy(pal, coarse_sorted, sizeof(alwan_oq_bucket) * n_coarse);
    memcpy(pal + n_coarse, fine_sorted, sizeof(alwan_oq_bucket) * n_fine);

    /* the lookup: coarse entries spread over their fine cells, then the fine entries,
     * each written at its mean's cell, the lowest index last so it wins */
    memset(coarse_lookup, 0, sizeof(coarse_lookup));
    memset(has_entry, 0, NC + NF);
    fine_set = has_entry + NC;
    for (i = n_coarse; i-- > 0;) {
        unsigned char c[3];
        unsigned int cell;
        alwan_oq_mean(&pal[i], c);
        cell = alwan_oq_coarse(c[0], c[1], c[2]);
        coarse_lookup[cell] = (unsigned int)i;
        has_entry[cell] = 1;
    }
    for (i = 0; i < NF; i++) {
        unsigned int const r = (unsigned int)(i >> 8), g = (unsigned int)(i >> 4) & 15u, b = (unsigned int)i & 15u;
        lookup[i] = coarse_lookup[(r >> 2) << 4 | (g >> 2) << 2 | (b >> 2)];
    }
    for (i = n_coarse + n_fine; i-- > n_coarse;) {
        unsigned char c[3];
        alwan_oq_mean(&pal[i], c);
        lookup[alwan_oq_fine(c[0], c[1], c[2])] = (unsigned int)i;
        fine_set[alwan_oq_fine(c[0], c[1], c[2])] = 1;
    }

    n_real = n_coarse;
    while (n_real < n_coarse + n_fine && pal[n_real].count > 0) n_real++;
    for (i = 0; i < n_real; i++) alwan_oq_mean(&pal[i], palette_out + 3 * i);
    *count_out = n_real;

    if (index_out) {
        for (i = 0; i < count; i++) {
            unsigned char const *p = rgb + i * pixel_stride;
            unsigned int const f = alwan_oq_fine(p[0], p[1], p[2]);
            unsigned int const cc = alwan_oq_coarse(p[0], p[1], p[2]);
            unsigned int idx = lookup[f];
            /* a cell a fine entry was written to maps there, the others to their coarse
             * cell's entry; a coarse cell without one sends its pixels to the nearest
             * entry, where Pillow's table holds 0 */
            if ((!fine_set[f] && !has_entry[cc]) || idx >= n_real) {
                long best = -1;
                size_t k;
                for (k = 0; k < n_real; k++) {
                    long const dr = (long)p[0] - palette_out[3 * k], dg = (long)p[1] - palette_out[3 * k + 1],
                               db = (long)p[2] - palette_out[3 * k + 2];
                    long const d = dr * dr + dg * dg + db * db;
                    if (best < 0 || d < best) {
                        best = d;
                        idx = (unsigned int)k;
                    }
                }
            }
            index_out[i] = idx;
        }
    }
    ALWAN_FREE(fine);
    ALWAN_FREE(lookup);
    ALWAN_FREE(has_entry);
    return ALWAN_OK;
}
