/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_edge_detect_{T} and _u8: a binary edge map.
 *
 *   CANNY  Canny's detector as scikit-image's feature.canny computes it, which suite 226
 *          holds this to: the image smoothed by scipy's gaussian with a zero edge and
 *          divided by the same smoothing of an all-ones image (plus the precision's
 *          epsilon), so the border is not darkened; scipy's ndimage.sobel down the rows and
 *          across the columns; the magnitude; non-maximum suppression across the gradient
 *          direction by linear interpolation between the two neighbours it falls between,
 *          the border excluded and the low threshold applied; then hysteresis: the
 *          8-connected pieces above the low threshold that hold a pixel at or above the
 *          high one.
 *
 * Every pass is kept in the data's precision as scipy and numpy keep it (8-bit data is
 * multiplied by 1 / 255 into double first). The suppression is scikit-image's Cython: its
 * low threshold is a C float whatever the data, and its interpolation weight (1 - w) is
 * formed in double.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double alwan_ed_r(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

/* numpy's pairwise sum of the kernel (at most 2 * 4 * 64 + 1 taps). */
static double alwan_ed_sum(double const *a, size_t n) {
    double r[8], res = 0.0;
    size_t i, j;
    if (n < 8) {
        for (i = 0; i < n; i++) res += a[i];
        return res;
    }
    if (n > 128) {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan_ed_sum(a, n2) + alwan_ed_sum(a + n2, n - n2);
    }
    for (j = 0; j < 8; j++) r[j] = a[j];
    for (i = 8; i < n - (n % 8); i += 8)
        for (j = 0; j < 8; j++) r[j] += a[i + j];
    res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    for (; i < n; i++) res += a[i];
    return res;
}

/* scipy's correlate1d with a symmetric (sym 1) or antisymmetric (-1) kernel wts of
 * 2 half + 1 taps, along lines (count lines of len, step lstep, sample step istep), the
 * edge "constant" 0 (reflect 0) or "reflect" (1). */
static void alwan_ed_corr(double *out, double const *in, size_t count, size_t len, size_t lstep, size_t istep, double const *wts,
                          size_t half, int sym, int reflect, int f32, double *ext) {
    size_t l, i;
    long j;
    for (l = 0; l < count; l++) {
        for (j = -(long)half; j < (long)(len + half); j++) {
            double v;
            if (j >= 0 && j < (long)len) {
                v = in[l * lstep + (size_t)j * istep];
            } else if (!reflect) {
                v = 0.0;
            } else {
                long const p = 2 * (long)len;
                long m = j % p;
                if (m < 0) m += p;
                v = in[l * lstep + (size_t)(m >= (long)len ? p - 1 - m : m) * istep];
            }
            ext[j + (long)half] = v;
        }
        for (i = 0; i < len; i++) {
            double const *c = ext + half + i;
            double acc = c[0] * wts[half];
            for (j = -(long)half; j < 0; j++) acc += (sym > 0 ? c[j] + c[-j] : c[j] - c[-j]) * wts[(long)half + j];
            out[l * lstep + i * istep] = alwan_ed_r(acc, f32);
        }
    }
}

static alwan_status alwan_ed_canny(unsigned char *edges, size_t edges_rs, double const *img, size_t w, size_t h, double sigma, double low,
                                   double high, int f32) {
    size_t const n = w * h, lw = (size_t)(4.0 * sigma + 0.5), extn = (w > h ? w : h) + 2 * (lw > 1 ? lw : 1) + 2;
    static double const d3[3] = { -1.0, 0.0, 1.0 }, s3[3] = { 1.0, 2.0, 1.0 };
    double *buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(7 * n + extn + 2 * lw + 1, sizeof(double)), sizeof(double));
    double *sm, *bl, *tmp, *is, *js, *mag, *lm, *ext, *wts;
    size_t *lab = NULL, *stack = NULL;
    unsigned char *good = NULL;
    size_t x, y, i;
    float const low_f = (float)(low == 0.0 ? 1e-14 : low);
    double const high_t = alwan_ed_r(high, f32);
    if (!buf) return ALWAN_E_NOMEM;
    sm = buf;
    bl = sm + n;
    tmp = bl + n;
    is = tmp + n;
    js = is + n;
    mag = js + n;
    lm = mag + n;
    ext = lm + n;
    wts = ext + extn;
    /* smoothing, and the same smoothing of an all-ones image */
    if (sigma > 1e-15) {
        double const a = -0.5 / (sigma * sigma);
        double s;
        for (i = 0; i <= 2 * lw; i++) {
            long const xi = (long)i - (long)lw;
            wts[i] = exp(a * (double)(xi * xi));
        }
        s = alwan_ed_sum(wts, 2 * lw + 1);
        for (i = 0; i <= 2 * lw; i++) wts[i] = wts[i] / s;
        for (i = 0; i < n; i++) is[i] = 1.0;
        alwan_ed_corr(tmp, is, w, h, 1, w, wts, lw, 1, 0, f32, ext);
        alwan_ed_corr(bl, tmp, h, w, w, 1, wts, lw, 1, 0, f32, ext);
        alwan_ed_corr(tmp, img, w, h, 1, w, wts, lw, 1, 0, f32, ext);
        alwan_ed_corr(sm, tmp, h, w, w, 1, wts, lw, 1, 0, f32, ext);
    } else {
        memcpy(sm, img, n * sizeof(double));
        for (i = 0; i < n; i++) bl[i] = 1.0;
    }
    {
        double const eps = f32 ? (double)FLT_EPSILON : DBL_EPSILON;
        for (i = 0; i < n; i++) sm[i] = alwan_ed_r(sm[i] / alwan_ed_r(bl[i] + eps, f32), f32);
    }
    /* ndimage.sobel: [-1, 0, 1] along its axis, then [1, 2, 1] along the other */
    alwan_ed_corr(tmp, sm, h, w, w, 1, d3, 1, -1, 1, f32, ext);   /* jsobel: axis 1 */
    alwan_ed_corr(js, tmp, w, h, 1, w, s3, 1, 1, 1, f32, ext);
    alwan_ed_corr(tmp, sm, w, h, 1, w, d3, 1, -1, 1, f32, ext);   /* isobel: axis 0 */
    alwan_ed_corr(is, tmp, h, w, w, 1, s3, 1, 1, 1, f32, ext);
    for (i = 0; i < n; i++) {
        double const m = alwan_ed_r(alwan_ed_r(is[i] * is[i], f32) + alwan_ed_r(js[i] * js[i], f32), f32);
        mag[i] = alwan_ed_r(sqrt(m), f32);
    }
    /* non-maximum suppression, scikit-image's Cython */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            size_t const o = y * w + x;
            double const m = mag[o], gi = is[o], gj = js[o];
            int const down = gi <= 0.0, up = gi >= 0.0, left = gj <= 0.0, right = gj >= 0.0;
            int const c1 = (up && right) || (down && left), c2 = (down && right) || (up && left);
            double ai, aj, wt, n11, n12, n21, n22, a1, a2;
            lm[o] = 0.0;
            if (y == 0 || x == 0 || y + 1 == h || x + 1 == w || !(m >= (double)low_f)) continue;
            if (!c1 && !c2) continue;
            ai = fabs(gi);
            aj = fabs(gj);
            if (c1) {
                if (ai > aj) {
                    wt = alwan_ed_r(aj / ai, f32);
                    n11 = mag[o + w], n12 = mag[o + w + 1], n21 = mag[o - w], n22 = mag[o - w - 1];
                } else {
                    wt = alwan_ed_r(ai / aj, f32);
                    n11 = mag[o + 1], n12 = mag[o + w + 1], n21 = mag[o - 1], n22 = mag[o - w - 1];
                }
            } else {
                if (ai < aj) {
                    wt = alwan_ed_r(ai / aj, f32);
                    n11 = mag[o + 1], n12 = mag[o - w + 1], n21 = mag[o - 1], n22 = mag[o + w - 1];
                } else {
                    wt = alwan_ed_r(aj / ai, f32);
                    n11 = mag[o - w], n12 = mag[o - w + 1], n21 = mag[o + w], n22 = mag[o + w - 1];
                }
            }
            /* neigh * w in the data's precision, neigh * (1.0 - w) in double */
            a1 = alwan_ed_r(n12 * wt, f32) + n11 * (1.0 - wt);
            if (!(a1 <= m)) continue;
            a2 = alwan_ed_r(n22 * wt, f32) + n21 * (1.0 - wt);
            if (a2 <= m) lm[o] = m;
        }
    }
    /* hysteresis: keep the 8-connected pieces of lm > 0 that hold a pixel >= high */
    lab = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(size_t)), sizeof(size_t));
    good = (unsigned char *)ALWAN_ALLOC(n, sizeof(double));
    if (!lab || !good) {
        ALWAN_FREE(lab);
        ALWAN_FREE(good);
        ALWAN_FREE(buf);
        return ALWAN_E_NOMEM;
    }
    stack = lab + n;
    memset(good, 0, n);
    for (i = 0; i < n; i++) lab[i] = 0;
    {
        size_t next = 0;
        for (i = 0; i < n; i++) {
            size_t top = 0;
            int keep = 0;
            if (!(lm[i] > 0.0) || lab[i]) continue;
            ++next;
            lab[i] = next;
            stack[top++] = i;
            while (top) {
                size_t const cur = stack[--top], cy = cur / w, cx = cur % w;
                int dy, dx;
                if (lm[cur] >= high_t) keep = 1;
                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        long const sy = (long)cy + dy, sx = (long)cx + dx;
                        size_t q;
                        if (sy < 0 || sx < 0 || sy >= (long)h || sx >= (long)w) continue;
                        q = (size_t)sy * w + (size_t)sx;
                        if (lm[q] > 0.0 && !lab[q]) {
                            lab[q] = next;
                            stack[top++] = q;
                        }
                    }
                }
            }
            if (keep) good[i] = 1;   /* marks the piece by its first pixel */
        }
        /* the kept labels, by label */
        {
            unsigned char *kl = (unsigned char *)ALWAN_ALLOC(next + 1, sizeof(double));
            if (!kl) {
                ALWAN_FREE(lab);
                ALWAN_FREE(good);
                ALWAN_FREE(buf);
                return ALWAN_E_NOMEM;
            }
            memset(kl, 0, next + 1);
            for (i = 0; i < n; i++)
                if (good[i]) kl[lab[i]] = 1;
            for (y = 0; y < h; y++) {
                unsigned char *row = edges + y * edges_rs;
                for (x = 0; x < w; x++) row[x] = (unsigned char)(lab[y * w + x] && kl[lab[y * w + x]]);
            }
            ALWAN_FREE(kl);
        }
    }
    ALWAN_FREE(lab);
    ALWAN_FREE(good);
    ALWAN_FREE(buf);
    return ALWAN_OK;
}

static alwan_status alwan_ed_run(unsigned char *edges, size_t edges_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_edge_detect_method method, alwan_edge_detect_params const *params, int kind) {
    alwan_edge_detect_params const zero = { 0 };
    alwan_edge_detect_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const n = w * h;
    double const dmax = kind == 2 ? 255.0 : 1.0;
    double const sigma = p->sigma > 0.0 ? p->sigma : 1.0;
    double const low = p->low_threshold != 0.0 ? p->low_threshold / dmax : 0.1;
    double const high = p->high_threshold != 0.0 ? p->high_threshold / dmax : 0.2;
    double *img;
    size_t x, y;
    alwan_status st;
    if (!edges || !src || w == 0 || h == 0 || ch != 1 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem < w || edges_rs < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_EDGE_DETECT_CANNY) return ALWAN_E_INVALID;
    if (!(p->sigma >= 0.0) || p->sigma > 64.0 || !(low >= 0.0) || !(high >= low)) return ALWAN_E_RANGE;
    img = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    if (!img) return ALWAN_E_NOMEM;
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            double const v = kind == 0 ? ((alwan_f64 const *)row)[x] : kind == 1 ? (double)((alwan_f32 const *)row)[x]
                                                                               : (double)((unsigned char const *)row)[x] * (1.0 / 255.0);
            if (!(v - v == 0.0)) {
                ALWAN_FREE(img);
                return ALWAN_E_INVALID;
            }
            img[y * w + x] = v;
        }
    }
    st = alwan_ed_canny(edges, edges_rs, img, w, h, sigma, low, high, kind == 1);
    ALWAN_FREE(img);
    return st;
}

alwan_status alwan_edge_detect_u8(unsigned char *edges, size_t edges_row_stride, unsigned char const *src, size_t src_row_stride, size_t channels,
                                  size_t width, size_t height, alwan_edge_detect_method method, alwan_edge_detect_params const *params) {
    return alwan_ed_run(edges, edges_row_stride, src, src_row_stride, channels, width, height, method, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_edge_detect_f64(unsigned char *edges, size_t edges_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels,
                                   size_t width, size_t height, alwan_edge_detect_method method, alwan_edge_detect_params const *params) {
    return alwan_ed_run(edges, edges_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_edge_detect_f32(unsigned char *edges, size_t edges_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels,
                                   size_t width, size_t height, alwan_edge_detect_method method, alwan_edge_detect_params const *params) {
    return alwan_ed_run(edges, edges_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
