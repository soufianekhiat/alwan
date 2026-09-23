/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * A maximum-coverage palette as Pillow computes it (src/libImaging/Quant.c, quantize2 with
 * kmeans 0, which Image.quantize(method=MAXCOVERAGE) calls; HPND licence). It is farthest-
 * point sampling of the image's distinct colours:
 *
 *   1. the first entry is the colour farthest from the mean pixel, the mean rounded as
 *      (int)(0.5 + mean) per channel
 *   2. each further entry is the colour farthest, in squared RGB distance, from its
 *      nearest entry so far
 *   3. each pixel maps to its nearest entry
 *
 * Pillow walks the distinct colours in the order of its hash table and keeps the first
 * colour at the largest distance, so ties are decided by that order, and it is
 * reproduced: bucket hash % L for Pillow's PIXEL_HASH, then (r, g, b) ascending within a
 * bucket, which is how its chains are kept. L is the table length after inserting that
 * many distinct colours: 11, grown to _findPrime(2 L + 1) whenever three times L falls
 * below the count. Pillow's _findPrime tests !start % t, which is (!start) % t, so it never
 * finds a factor and returns the first number whose last hex digit is 1, 3, 7, 9, 11 or
 * 13; that is reproduced too, since L decides the order.
 *
 * The index map is Pillow's map_image_pixels: entries sorted by distance from entry 0,
 * then by index; the search starts at entry 0 and stops past four times the pixel's
 * squared distance to it, keeping the first entry strictly nearer. The bound is exact, so
 * this is the nearest entry, ties broken by that order.
 *
 * Pillow sums the channels for the mean in 32 bits, which wraps past 16843009 pixels;
 * this sums in 64. Pillow returns max_colors entries, repeating its first pixel's colour
 * once the distinct colours run out; count_out here stops at the distinct colours.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned int key; /* r << 16 | g << 8 | b */
    unsigned int bucket;
} alwan_mcov_colour;

static unsigned int alwan_mcov_hash(unsigned int key) {
    unsigned int const r = key >> 16, g = (key >> 8) & 255u, b = key & 255u;
    return (r * 463u) ^ ((g << 8) * 10069u) ^ ((b << 16) * 64997u);
}

/* Pillow's _findPrime, as it runs. */
static unsigned int alwan_mcov_find_prime(unsigned int start, int dir) {
    static int const unit[] = { 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0 };
    while (start > 1) {
        if (!unit[start & 0x0f]) {
            start = (unsigned int)((int)start + dir);
            continue;
        }
        break;
    }
    return start;
}

/* The length of Pillow's hash table after n distinct insertions. */
static unsigned int alwan_mcov_table_length(size_t n) {
    unsigned int len = 11;
    size_t c;
    for (c = 1; c <= n; c++) {
        unsigned int nl = len;
        if (c * 3 < len) {
            nl = alwan_mcov_find_prime(len / 2 - 1, -1);
        } else if ((size_t)len * 3 < c) {
            nl = alwan_mcov_find_prime(len * 2 + 1, +1);
        }
        if (nl >= 11) len = nl;
    }
    return len;
}

static int alwan_mcov_cmp_key(void const *x, void const *y) {
    unsigned int const a = *(unsigned int const *)x, b = *(unsigned int const *)y;
    return a < b ? -1 : a > b ? 1 : 0;
}

static int alwan_mcov_cmp_order(void const *x, void const *y) {
    alwan_mcov_colour const *a = (alwan_mcov_colour const *)x, *b = (alwan_mcov_colour const *)y;
    if (a->bucket != b->bucket) return a->bucket < b->bucket ? -1 : 1;
    return a->key < b->key ? -1 : a->key > b->key ? 1 : 0;
}

static unsigned int alwan_mcov_dist(unsigned int a, unsigned int b) {
    int const dr = (int)(a >> 16) - (int)(b >> 16), dg = (int)((a >> 8) & 255u) - (int)((b >> 8) & 255u),
              db = (int)(a & 255u) - (int)(b & 255u);
    return (unsigned int)(dr * dr + dg * dg + db * db);
}

typedef struct {
    unsigned int d;
    unsigned int index;
} alwan_mcov_sortkey;

static int alwan_mcov_cmp_sortkey(void const *x, void const *y) {
    alwan_mcov_sortkey const *a = (alwan_mcov_sortkey const *)x, *b = (alwan_mcov_sortkey const *)y;
    if (a->d != b->d) return a->d < b->d ? -1 : 1;
    return a->index < b->index ? -1 : a->index > b->index ? 1 : 0;
}

alwan_status alwan__quantize_max_coverage(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                          unsigned char const *rgb, size_t pixel_stride, size_t count, size_t max_colors) {
    unsigned int *keys, *dist, *entries;
    alwan_mcov_colour *order;
    alwan_mcov_sortkey *row0;
    unsigned long long sum[3] = { 0, 0, 0 };
    unsigned int mean, len;
    size_t i, n, k, n_entries;
    if (!palette_out || !count_out || !rgb || count == 0 || max_colors == 0 || pixel_stride < 3) return ALWAN_E_INVALID;
    if (max_colors > 65536) return ALWAN_E_RANGE;
    keys = (unsigned int *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(unsigned int)), sizeof(unsigned int));
    if (!keys) return ALWAN_E_NOMEM;
    for (i = 0; i < count; i++) {
        unsigned char const *p = rgb + i * pixel_stride;
        keys[i] = (unsigned int)p[0] << 16 | (unsigned int)p[1] << 8 | p[2];
        sum[0] += p[0];
        sum[1] += p[1];
        sum[2] += p[2];
    }
    qsort(keys, count, sizeof(unsigned int), alwan_mcov_cmp_key);
    for (i = 1, n = 1; i < count; i++) {
        if (keys[i] != keys[n - 1]) keys[n++] = keys[i];
    }

    order = (alwan_mcov_colour *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(alwan_mcov_colour)), sizeof(unsigned int));
    dist = (unsigned int *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(unsigned int)), sizeof(unsigned int));
    n_entries = max_colors < n ? max_colors : n;
    entries = (unsigned int *)ALWAN_ALLOC(n_entries * sizeof(unsigned int), sizeof(unsigned int));
    row0 = (alwan_mcov_sortkey *)ALWAN_ALLOC(n_entries * sizeof(alwan_mcov_sortkey), sizeof(unsigned int));
    if (!order || !dist || !entries || !row0) {
        if (order) ALWAN_FREE(order);
        if (dist) ALWAN_FREE(dist);
        if (entries) ALWAN_FREE(entries);
        if (row0) ALWAN_FREE(row0);
        ALWAN_FREE(keys);
        return ALWAN_E_NOMEM;
    }

    /* Pillow's iteration order over its hash table of the distinct colours */
    len = alwan_mcov_table_length(n);
    for (i = 0; i < n; i++) {
        order[i].key = keys[i];
        order[i].bucket = alwan_mcov_hash(keys[i]) % len;
        dist[i] = 0xffffffffu;
    }
    qsort(order, n, sizeof(alwan_mcov_colour), alwan_mcov_cmp_order);

    /* farthest-point sampling; Pillow's second pass replaces the distances to the mean
     * with the distances to the first entry */
    mean = (unsigned int)(int)(0.5 + (double)sum[0] / (double)count) << 16 |
           (unsigned int)(int)(0.5 + (double)sum[1] / (double)count) << 8 |
           (unsigned int)(int)(0.5 + (double)sum[2] / (double)count);
    {
        unsigned int from = mean;
        for (k = 0; k < n_entries; k++) {
            unsigned int far_d = 0, far_key = 0;
            int found = 0;
            for (i = 0; i < n; i++) {
                unsigned int const d = alwan_mcov_dist(from, order[i].key);
                if (k == 1 || d < dist[i]) dist[i] = d;
                if (dist[i] > far_d) {
                    far_d = dist[i];
                    far_key = order[i].key;
                    found = 1;
                }
            }
            /* no colour left at a positive distance: only when there is one colour, and
             * then it is Pillow's first pixel */
            if (!found) far_key = order[0].key;
            entries[k] = far_key;
            from = far_key;
        }
    }
    for (k = 0; k < n_entries; k++) {
        palette_out[3 * k] = (unsigned char)(entries[k] >> 16);
        palette_out[3 * k + 1] = (unsigned char)(entries[k] >> 8);
        palette_out[3 * k + 2] = (unsigned char)entries[k];
    }
    *count_out = n_entries;

    if (index_out) {
        /* each distinct colour's entry, then the pixels by binary search */
        for (k = 0; k < n_entries; k++) {
            row0[k].d = alwan_mcov_dist(entries[0], entries[k]);
            row0[k].index = (unsigned int)k;
        }
        qsort(row0, n_entries, sizeof(alwan_mcov_sortkey), alwan_mcov_cmp_sortkey);
        for (i = 0; i < n; i++) {
            unsigned int best = 0, bestd = alwan_mcov_dist(entries[0], keys[i]);
            unsigned int const bound = bestd << 2;
            for (k = 0; k < n_entries && row0[k].d <= bound; k++) {
                unsigned int const d = alwan_mcov_dist(entries[row0[k].index], keys[i]);
                if (d < bestd) {
                    bestd = d;
                    best = row0[k].index;
                }
            }
            dist[i] = best; /* reused: the entry of keys[i] */
        }
        for (i = 0; i < count; i++) {
            unsigned char const *p = rgb + i * pixel_stride;
            unsigned int const key = (unsigned int)p[0] << 16 | (unsigned int)p[1] << 8 | p[2];
            size_t lo = 0, hi = n;
            while (hi - lo > 1) {
                size_t const mid = (lo + hi) / 2;
                if (keys[mid] <= key) lo = mid;
                else hi = mid;
            }
            index_out[i] = dist[lo];
        }
    }
    ALWAN_FREE(order);
    ALWAN_FREE(dist);
    ALWAN_FREE(entries);
    ALWAN_FREE(row0);
    ALWAN_FREE(keys);
    return ALWAN_OK;
}
