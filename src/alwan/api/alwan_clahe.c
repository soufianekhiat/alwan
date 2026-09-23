/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Contrast-limited adaptive histogram equalisation (CLAHE): Zuiderveld, "Contrast Limited
 * Adaptive Histogram Equalization", Graphics Gems IV, 1994, after Pizer et al. 1987. The
 * image is cut into tiles_x x tiles_y tiles; each tile's histogram is clipped at
 * clip_limit times its mean bin count, the clipped counts are spread back over all bins,
 * and the cumulative histogram becomes that tile's tone curve. Every pixel is mapped by
 * the curves of the four nearest tile centres, blended bilinearly, so the contrast adapts
 * to the neighbourhood without seams.
 *
 * The reference is OpenCV's cv::createCLAHE (modules/imgproc/src/clahe.cpp, Apache-2.0),
 * and this reproduces its integer arithmetic and its float blend bit for bit:
 *   - an image that does not divide into tiles is extended at the bottom and right by
 *     BORDER_REFLECT_101 for the histograms, by tiles - (size % tiles) in each direction,
 *     which is a whole extra tile in the direction that did divide;
 *   - the clip level is (int)(clip_limit * tile_area / bins), at least 1; clip_limit <= 0
 *     turns clipping off;
 *   - the clipped count is returned as clipped / bins to every bin, and the remainder one
 *     at a time to bins 0, step, 2 step, ... with step = max(bins / remainder, 1);
 *   - the curve is saturate(round(cumsum * (float)(bins - 1) / tile_area)), rounding half
 *     to even;
 *   - the blend weights are x / tile_w - 0.5 in float, and the blended value rounds half to
 *     even.
 * bins is 256 for 8-bit data and 65536 for 16-bit. One channel; to equalise a colour
 * image, run it on a lightness channel.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>

/* BORDER_REFLECT_101 for an index at or past n (the image is only extended forward). */
static size_t alwan_clahe_reflect101(size_t i, size_t n) {
    if (n == 1) return 0;
    while (i >= n) {
        size_t const period = 2 * (n - 1);
        i %= period;
        if (i >= n) i = period - i;
    }
    return i;
}

/* cvRound: nearest, ties to even. */
static long alwan_clahe_round(float v) {
    float f = floorf(v);
    float const d = v - f;
    if (d > 0.5f || (d == 0.5f && fmodf(f, 2.0f) != 0.0f)) f += 1.0f;
    return (long)f;
}

alwan_status alwan__clahe_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t w,
                                    size_t h, size_t tiles_x, size_t tiles_y, double clip_limit, int is16) {
    size_t const bins = is16 ? 65536u : 256u;
    size_t const elem = is16 ? 2u : 1u;
    long const maxv = (long)bins - 1;
    size_t tw, th, area, t, x, y, i;
    size_t ew = w, eh = h;
    int clip = 0;
    float lut_scale;
    int *hist;
    unsigned short *lut;
    size_t *xi1, *xi2;
    float *xa;
    if (!out || !src) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || tiles_x == 0 || tiles_y == 0) return ALWAN_E_INVALID;
    if (src_row_stride / elem < w || out_row_stride / elem < w) return ALWAN_E_INVALID;
    if (!(clip_limit == clip_limit) || clip_limit > 1e9 || tiles_x > 4096 || tiles_y > 4096 || tiles_x * tiles_y > 4096) {
        return ALWAN_E_RANGE;
    }
    if (w % tiles_x != 0 || h % tiles_y != 0) {
        ew = w + tiles_x - w % tiles_x;
        eh = h + tiles_y - h % tiles_y;
    }
    tw = ew / tiles_x;
    th = eh / tiles_y;
    area = tw * th;
    if (area > 0x7fffffffu / 2u) return ALWAN_E_RANGE;
    lut_scale = (float)(bins - 1) / (float)(int)area;
    if (clip_limit > 0.0) {
        clip = (int)(clip_limit * (double)(int)area / (double)(int)bins);
        if (clip < 1) clip = 1;
    }

    hist = (int *)ALWAN_ALLOC(bins * sizeof(int), sizeof(int));
    lut = (unsigned short *)ALWAN_ALLOC(alwan_safe_array_size(tiles_x * tiles_y, bins * sizeof(unsigned short)), 16);
    xi1 = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(w, 2 * sizeof(size_t)), sizeof(size_t));
    xa = (float *)ALWAN_ALLOC(alwan_safe_array_size(w, 2 * sizeof(float)), sizeof(float));
    if (!hist || !lut || !xi1 || !xa) {
        if (hist) ALWAN_FREE(hist);
        if (lut) ALWAN_FREE(lut);
        if (xi1) ALWAN_FREE(xi1);
        if (xa) ALWAN_FREE(xa);
        return ALWAN_E_NOMEM;
    }
    xi2 = xi1 + w;

    for (t = 0; t < tiles_x * tiles_y; t++) {
        size_t const tx = t % tiles_x, ty = t / tiles_x;
        unsigned short *tl = lut + t * bins;
        int sum = 0;
        for (i = 0; i < bins; i++) hist[i] = 0;
        for (y = ty * th; y < (ty + 1) * th; y++) {
            char const *row = (char const *)src + alwan_clahe_reflect101(y, h) * src_row_stride;
            for (x = tx * tw; x < (tx + 1) * tw; x++) {
                size_t const sx = alwan_clahe_reflect101(x, w);
                hist[is16 ? ((unsigned short const *)row)[sx] : ((unsigned char const *)row)[sx]]++;
            }
        }
        if (clip > 0) {
            int clipped = 0, batch, residual;
            for (i = 0; i < bins; i++) {
                if (hist[i] > clip) {
                    clipped += hist[i] - clip;
                    hist[i] = clip;
                }
            }
            batch = clipped / (int)bins;
            residual = clipped - batch * (int)bins;
            for (i = 0; i < bins; i++) hist[i] += batch;
            if (residual != 0) {
                size_t step = bins / (size_t)residual;
                if (step < 1) step = 1;
                for (i = 0; i < bins && residual > 0; i += step, residual--) hist[i]++;
            }
        }
        for (i = 0; i < bins; i++) {
            long v;
            sum += hist[i];
            v = alwan_clahe_round((float)sum * lut_scale);
            tl[i] = (unsigned short)(v < 0 ? 0 : v > maxv ? maxv : v);
        }
    }

    {
        float const inv_tw = 1.0f / (float)(int)tw;
        float const inv_th = 1.0f / (float)(int)th;
        for (x = 0; x < w; x++) {
            float const txf = (float)(int)x * inv_tw - 0.5f;
            long tx1 = (long)floorf(txf), tx2 = tx1 + 1;
            xa[x] = txf - (float)tx1;
            xa[w + x] = 1.0f - xa[x];
            if (tx1 < 0) tx1 = 0;
            if (tx2 > (long)tiles_x - 1) tx2 = (long)tiles_x - 1;
            xi1[x] = (size_t)tx1 * bins;
            xi2[x] = (size_t)tx2 * bins;
        }
        for (y = 0; y < h; y++) {
            char const *srow = (char const *)src + y * src_row_stride;
            char *orow = (char *)out + y * out_row_stride;
            float const tyf = (float)(int)y * inv_th - 0.5f;
            long ty1 = (long)floorf(tyf), ty2 = ty1 + 1;
            float const ya = tyf - (float)ty1, ya1 = 1.0f - ya;
            unsigned short const *p1, *p2;
            if (ty1 < 0) ty1 = 0;
            if (ty2 > (long)tiles_y - 1) ty2 = (long)tiles_y - 1;
            p1 = lut + (size_t)ty1 * tiles_x * bins;
            p2 = lut + (size_t)ty2 * tiles_x * bins;
            for (x = 0; x < w; x++) {
                size_t const v = is16 ? ((unsigned short const *)srow)[x] : ((unsigned char const *)srow)[x];
                size_t const i1 = xi1[x] + v, i2 = xi2[x] + v;
                float const a = (float)p1[i1] * xa[w + x];
                float const b = (float)p1[i2] * xa[x];
                float const c = (float)p2[i1] * xa[w + x];
                float const d = (float)p2[i2] * xa[x];
                float const res = (a + b) * ya1 + (c + d) * ya;
                long r = alwan_clahe_round(res);
                r = r < 0 ? 0 : r > maxv ? maxv : r;
                if (is16) ((unsigned short *)orow)[x] = (unsigned short)r;
                else ((unsigned char *)orow)[x] = (unsigned char)r;
            }
        }
    }
    ALWAN_FREE(hist);
    ALWAN_FREE(lut);
    ALWAN_FREE(xi1);
    ALWAN_FREE(xa);
    return ALWAN_OK;
}
