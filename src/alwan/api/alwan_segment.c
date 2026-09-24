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
 *
 * Union-find over one raster pass (each pixel joined to its equal neighbours already
 * visited: left and above, and above-left and above-right when 8-connected), then a
 * second pass that numbers the roots as they are first met.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
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

static alwan_status alwan_sg_run(uint32_t *labels, size_t labels_rs, size_t *count_out, void const *src, size_t src_rs, size_t ch,
                                 size_t w, size_t h, alwan_segment_method method, alwan_segment_params const *params, int kind) {
    alwan_segment_params const zero = { 0 };
    alwan_segment_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const n = w * h;
    int const eight = p->connectivity != 4;
    double *v;
    size_t *par;
    unsigned char *bg;
    size_t x, y, c, i, count = 0;
    if (!labels || !src || w == 0 || h == 0 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || labels_rs / sizeof(uint32_t) < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_SEGMENT_CONNECTED) return ALWAN_E_INVALID;
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
