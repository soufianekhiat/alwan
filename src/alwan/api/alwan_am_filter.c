/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The adaptive manifold filter: Gastal and Oliveira, "Adaptive Manifolds for Real-Time
 * High-Dimensional Filtering", SIGGRAPH 2012, as OpenCV's ximgproc::amFilter computes it
 * (opencv_contrib 5.0.0, modules/ximgproc/src/adaptive_manifold_filter_n.cpp, with the
 * recursive domain transform of dtfilter_cpu and the row helpers of
 * edgeaware_filters_common.cpp). This file is a port of that code, kept to its float
 * arithmetic and order of operations so the result is OpenCV's value for value:
 *
 *   - cv::exp on float is the C runtime's expf (OpenCV's MSVC build takes that branch), so
 *     every exponential here is ALWAN_EXP_F32 in a loop the compiler may not vectorise;
 *   - cv::resize INTER_LINEAR on one float channel, the fast 2x2 area average when the
 *     factor is exactly 2 (its SIMD blocks of four and scalar tail add in different
 *     orders), a copy when the size does not change;
 *   - cv::RNG's multiply-with-carry generator, seeded from the joint image's centre value
 *     as the implementation seeds it, for the first PCA vector of each split;
 *   - the eigenvector sums in float per row, gathered in double, normalised in double.
 *
 * The tree height, the PCA iteration count (1) and the random PCA start are OpenCV's defaults,
 * the settings its Python binding reaches. 0 / 0 where a cluster's weights are all zero is
 * NaN, as in OpenCV. OpenCV's own result is not repeatable with three or more threads (a race
 * in its parallel code changes about 1e-7 of the values from call to call); with one or two
 * threads it is fixed, and that is the value this computes (the reference sets one thread).
 *
 * The OpenCV code carries this notice:
 *
 *   License Agreement For Open Source Computer Vision Library (3 - clause BSD License)
 *
 *   Redistribution and use in source and binary forms, with or without modification,
 *   are permitted provided that the following conditions are met :
 *   * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and / or other materials provided with the distribution.
 *   * Neither the names of the copyright holders nor the names of the contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 *   This software is provided by the copyright holders and contributors "as is" and
 *   any express or implied warranties, including, but not limited to, the implied
 *   warranties of merchantability and fitness for a particular purpose are disclaimed.
 *   In no event shall copyright holders or contributors be liable for any direct,
 *   indirect, incidental, special, exemplary, or consequential damages
 *   (including, but not limited to, procurement of substitute goods or services;
 *   loss of use, data, or profits; or business interruption) however caused
 *   and on any theory of liability, whether in contract, strict liability,
 *   or tort(including negligence or otherwise) arising in any way out of
 *   the use of this software, even if advised of the possibility of such damage.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

/* cvRound on a double: to nearest, ties to even. */
static int amf_round(double v) {
    double r = ALWAN_FLOOR_F64(v + 0.5);
    if (r - v == 0.5 && ALWAN_FMOD_F64(r, 2.0) != 0.0) r -= 1.0;
    if (!(r > (double)INT_MIN && r < (double)INT_MAX)) return 0;
    return (int)r;
}

static int amf_floor(double v) {
    double const r = ALWAN_FLOOR_F64(v);
    if (!(r > (double)INT_MIN && r < (double)INT_MAX)) return 0;
    return (int)r;
}

/* expf over a buffer, one call an element: the loop is kept scalar so MSVC does not swap in
 * its vector exponential, whose last bit differs from expf's. */
static void amf_exp(float *p, size_t n) {
    size_t i;
#if defined(_MSC_VER)
#pragma loop(no_vector)
#endif
    for (i = 0; i < n; i++) p[i] = ALWAN_EXP_F32(p[i]);
}

/* ------------------------------------------------------------------------------------ */
/* cv::resize on one float channel, non-IPP                                              */
/* ------------------------------------------------------------------------------------ */

/* INTER_LINEAR: the horizontal pass first, S[sx] a0 + S[sx + 1] a1 unfused, a column past
 * the last pair copied; the vertical pass with its rows clamped and its weights kept. */
static int amf_resize_linear(float *dst, int dw, int dh, float const *src, int sw, int sh, double scale_x,
                             double scale_y) {
    int *xofs = (int *)ALWAN_ALLOC((size_t)dw * sizeof(int), sizeof(int));
    float *alpha = (float *)ALWAN_ALLOC((size_t)dw * 2 * sizeof(float), sizeof(float));
    float *hbuf = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)sh * (size_t)dw, sizeof(float)), sizeof(float));
    int xmax = dw, dx, dy, y;
    if (!xofs || !alpha || !hbuf) {
        if (xofs) ALWAN_FREE(xofs);
        if (alpha) ALWAN_FREE(alpha);
        if (hbuf) ALWAN_FREE(hbuf);
        return 0;
    }
    for (dx = 0; dx < dw; dx++) {
        float fx = (float)(((double)dx + 0.5) * scale_x - 0.5);
        int sx = amf_floor((double)fx);
        fx -= (float)sx;
        if (sx < 0) { fx = 0.0f; sx = 0; }
        if (sx + 1 >= sw) {
            if (dx < xmax) xmax = dx;
            if (sx >= sw - 1) { fx = 0.0f; sx = sw - 1; }
        }
        xofs[dx] = sx;
        alpha[2 * dx] = 1.0f - fx;
        alpha[2 * dx + 1] = fx;
    }
    for (y = 0; y < sh; y++) {
        float const *s = src + (size_t)y * (size_t)sw;
        float *d = hbuf + (size_t)y * (size_t)dw;
        for (dx = 0; dx < dw; dx++) {
            int const sx = xofs[dx];
            if (dx < xmax) {
                float const t0 = s[sx] * alpha[2 * dx];
                float const t1 = s[sx + 1] * alpha[2 * dx + 1];
                d[dx] = t0 + t1;
            } else {
                d[dx] = s[sx] * alpha[2 * dx];
            }
        }
    }
    for (dy = 0; dy < dh; dy++) {
        float fy = (float)(((double)dy + 0.5) * scale_y - 0.5);
        int const sy = amf_floor((double)fy);
        int r0, r1;
        float b0, b1;
        fy -= (float)sy;
        b0 = 1.0f - fy;
        b1 = fy;
        r0 = sy < 0 ? 0 : (sy > sh - 1 ? sh - 1 : sy);
        r1 = sy + 1 < 0 ? 0 : (sy + 1 > sh - 1 ? sh - 1 : sy + 1);
        for (dx = 0; dx < dw; dx++) {
            float const t0 = hbuf[(size_t)r0 * (size_t)dw + (size_t)dx] * b0;
            float const t1 = hbuf[(size_t)r1 * (size_t)dw + (size_t)dx] * b1;
            dst[(size_t)dy * (size_t)dw + (size_t)dx] = t0 + t1;
        }
    }
    ALWAN_FREE(xofs);
    ALWAN_FREE(alpha);
    ALWAN_FREE(hbuf);
    return 1;
}

/* The fast 2x2 area average INTER_LINEAR becomes at a factor of exactly 2: blocks of four
 * outputs as ((a00 + a01) + (a10 + a11)) / 4 (the SIMD form), the rest of a full row as
 * 0 + (((a00 + a01) + a10) + a11), times 1/4, and a column or row past the last full pair as
 * the mean of what is inside the image. */
static void amf_resize_area2(float *dst, int dw, int dh, float const *src, int sw, int sh) {
    int const dwidth1 = sw / 2;
    int dx, dy;
    for (dy = 0; dy < dh; dy++) {
        float *D = dst + (size_t)dy * (size_t)dw;
        int const sy0 = dy * 2;
        int const w = sy0 + 2 <= sh ? dwidth1 : 0;
        float const *S0 = src + (size_t)sy0 * (size_t)sw;
        float const *S1 = S0 + sw;
        if (sy0 >= sh) {
            for (dx = 0; dx < dw; dx++) D[dx] = 0.0f;
            continue;
        }
        dx = 0;
        for (; dx <= w - 4; dx += 4) {
            int k;
            for (k = 0; k < 4; k++) {
                int const s = 2 * (dx + k);
                D[dx + k] = ((S0[s] + S0[s + 1]) + (S1[s] + S1[s + 1])) * 0.25f;
            }
        }
        for (; dx < w; dx++) {
            int const s = 2 * dx;
            float sum = 0.0f;
            sum += ((S0[s] + S0[s + 1]) + S1[s]) + S1[s + 1];
            D[dx] = sum * 0.25f;
        }
        for (; dx < dw; dx++) {
            float sum = 0.0f;
            int count = 0, sy, sx;
            int const sx0 = 2 * dx;
            for (sy = 0; sy < 2; sy++) {
                float const *S;
                if (sy0 + sy >= sh) break;
                S = src + (size_t)(sy0 + sy) * (size_t)sw + (size_t)sx0;
                for (sx = 0; sx < 2; sx++) {
                    if (sx0 + sx >= sw) break;
                    sum += S[sx];
                    count++;
                }
            }
            D[dx] = sum / (float)count;
        }
    }
}

/* cv::resize with inv_scale (the fx and fy of the call; the size ratio when a size is
 * given). Same size is a copy. */
static int amf_resize(float *dst, int dw, int dh, float const *src, int sw, int sh, double inv_x, double inv_y) {
    double const scale_x = 1.0 / inv_x, scale_y = 1.0 / inv_y;
    int const ix = amf_round(scale_x), iy = amf_round(scale_y);
    int const area_fast = ALWAN_ABS_F64(scale_x - (double)ix) < DBL_EPSILON && ALWAN_ABS_F64(scale_y - (double)iy) < DBL_EPSILON;
    if (dw == sw && dh == sh) {
        memcpy(dst, src, (size_t)sw * (size_t)sh * sizeof(float));
        return 1;
    }
    if (area_fast && ix == 2 && iy == 2) {
        amf_resize_area2(dst, dw, dh, src, sw, sh);
        return 1;
    }
    return amf_resize_linear(dst, dw, dh, src, sw, sh, scale_x, scale_y);
}

/* ------------------------------------------------------------------------------------ */
/* The filter                                                                            */
/* ------------------------------------------------------------------------------------ */

typedef struct {
    int w, h, ws, hs;      /* full and small (downsampled) sizes */
    size_t n, ns;          /* their pixel counts */
    int scn, jcn;          /* source and joint channels */
    double df;             /* the resize ratio, a power of two */
    double sigma_s, sigma_r;
    float sr_sqrt2;        /* sigma_r / sqrt(2) */
    int tree_height, pca_iterations, adjust_outliers, use_rng;
    uint64_t rng;
    float *src;            /* scn planes, full */
    float *joint;          /* jcn planes, full */
    float *sum;            /* scn planes, full: sum of w_k blur(Psi_k) */
    float *sum0;           /* full */
    float *wk;             /* full */
    float *mindist;        /* full, adjust_outliers only */
    float *eta_full;       /* jcn planes, full */
    float *tmp;            /* full scratch */
    float *tmp2;           /* full scratch */
} amf_ctx;

/* The recursive filter of the manifold's first eta: each row forward then back, then the
 * rows downwards and upwards, with a = exp(-sqrt(2) / sigma). src may be dst. */
static void amf_h_filter(float *dst, float const *src, int rows, int cols, float sigma) {
    float const a = ALWAN_EXP_F32(-ALWAN_SQRT_F32(2.0f) / sigma);
    int x, y;
    for (y = 0; y < rows; y++) {
        float const *s = src + (size_t)y * (size_t)cols;
        float *d = dst + (size_t)y * (size_t)cols;
        d[0] = s[0];
        for (x = 1; x < cols; x++) d[x] = s[x] + a * (d[x - 1] - s[x]);
        for (x = cols - 2; x >= 0; x--) d[x] = d[x] + a * (d[x + 1] - d[x]);
    }
    for (y = 1; y < rows; y++) {
        float *c = dst + (size_t)y * (size_t)cols;
        float const *p = c - cols;
        for (x = 0; x < cols; x++) c[x] += a * (p[x] - c[x]);
    }
    for (y = rows - 2; y >= 0; y--) {
        float *c = dst + (size_t)y * (size_t)cols;
        float const *p = c + cols;
        for (x = 0; x < cols; x++) c[x] += a * (p[x] - c[x]);
    }
}

/* w_k = exp(-|eta - joint|^2 / (2 sigma^2)), the squared distance summed channel by channel
 * in float; with adjust_outliers the running minimum of that distance over the manifolds. */
static void amf_compute_wk(amf_ctx *c, float const *eta, int level) {
    float const sigma = c->sr_sqrt2;
    float const arg = -0.5f / (sigma * sigma);
    size_t i;
    int k;
    for (i = 0; i < c->n; i++) {
        float d = 0.0f;
        for (k = 0; k < c->jcn; k++) {
            float const t = eta[(size_t)k * c->n + i] - c->joint[(size_t)k * c->n + i];
            if (k == 0) d = t * t;
            else d += t * t;
        }
        if (c->adjust_outliers) {
            if (level != 1) {
                float const m = c->mindist[i];
                c->mindist[i] = d < m ? d : m;
            } else {
                c->mindist[i] = d;
            }
        }
        c->wk[i] = d * arg;
    }
    amf_exp(c->wk, c->n);
}

/* The domain transform's a^d on the small eta, between horizontal (hs x (ws - 1)) and
 * vertical ((hs - 1) x ws) neighbours. */
static void amf_compute_dt(amf_ctx const *c, float const *eta, float *hor, float *ver, float ss, float sr) {
    float const ratio2 = (ss / sr) * (ss / sr);
    float const ln_alpha = (float)(-ALWAN_SQRT_F64(2.0) / (double)ss);
    int const ws = c->ws, hs = c->hs;
    int x, y, k;
    for (y = 0; y < hs; y++) {
        float *d = hor + (size_t)y * (size_t)(ws - 1);
        for (x = 0; x < ws - 1; x++) {
            float acc = 0.0f;
            for (k = 0; k < c->jcn; k++) {
                float const *e = eta + (size_t)k * c->ns + (size_t)y * (size_t)ws;
                float const t = e[x] - e[x + 1];
                if (k == 0) acc = t * t;
                else acc += t * t;
            }
            acc = acc * ratio2 + 1.0f;
            acc = ALWAN_SQRT_F32(acc);
            d[x] = acc * ln_alpha;
        }
    }
    for (y = 0; y < hs - 1; y++) {
        float *d = ver + (size_t)y * (size_t)ws;
        for (x = 0; x < ws; x++) {
            float acc = 0.0f;
            for (k = 0; k < c->jcn; k++) {
                float const *e = eta + (size_t)k * c->ns + (size_t)y * (size_t)ws;
                float const t = e[x] - e[x + ws];
                if (k == 0) acc = t * t;
                else acc += t * t;
            }
            acc = acc * ratio2 + 1.0f;
            acc = ALWAN_SQRT_F32(acc);
            d[x] = acc * ln_alpha;
        }
    }
    amf_exp(hor, (size_t)hs * (size_t)(ws - 1));
    amf_exp(ver, (size_t)(hs - 1) * (size_t)ws);
}

/* One recursive-filter iteration of the domain transform, in place: rows forward and back,
 * then columns down and up. */
static void amf_rf(float *p, int hs, int ws, float const *hor, float const *ver) {
    int x, y;
    for (y = 0; y < hs; y++) {
        float *d = p + (size_t)y * (size_t)ws;
        float const *a = hor + (size_t)y * (size_t)(ws - 1);
        for (x = 1; x < ws; x++) d[x] += a[x - 1] * (d[x - 1] - d[x]);
        for (x = ws - 2; x >= 0; x--) d[x] += a[x] * (d[x + 1] - d[x]);
    }
    for (y = 1; y < hs; y++) {
        float *cur = p + (size_t)y * (size_t)ws;
        float const *prev = cur - ws;
        float const *a = ver + (size_t)(y - 1) * (size_t)ws;
        for (x = 0; x < ws; x++) cur[x] += a[x] * (prev[x] - cur[x]);
    }
    for (y = hs - 2; y >= 0; y--) {
        float *cur = p + (size_t)y * (size_t)ws;
        float const *next = cur + ws;
        float const *a = ver + (size_t)y * (size_t)ws;
        for (x = 0; x < ws; x++) cur[x] += a[x] * (next[x] - cur[x]);
    }
}

static int amf_down(amf_ctx const *c, float *dst, float const *src) {
    return amf_resize(dst, c->ws, c->hs, src, c->w, c->h, 1.0 / c->df, 1.0 / c->df);
}

static int amf_up(amf_ctx const *c, float *dst, float const *src) {
    return amf_resize(dst, c->w, c->h, src, c->ws, c->hs, (double)c->w / (double)c->ws, (double)c->h / (double)c->hs);
}

/* The offset of row y in a plane w wide. */
static size_t amf_at(int y, int w) {
    return (size_t)y * (size_t)w;
}

/* cv::RNG: multiply with carry. */
static uint64_t amf_rng_next(uint64_t x) {
    return (uint64_t)(unsigned)x * 4164903690U + (x >> 32);
}

/* The split of one cluster: the first principal direction of joint - eta over the cluster
 * (one power step from a random start, OpenCV's), and the sign of each pixel's projection. */
static int amf_clusters(amf_ctx *c, unsigned char const *cluster, unsigned char *minus, unsigned char *plus) {
    float *orient = c->tmp;
    size_t i;
    int k;
    if (c->jcn > 1) {
        float init[4], vec[4];
        double vd[4] = {0.0, 0.0, 0.0, 0.0}, norm = 0.0;
        float *dif = (float *)ALWAN_ALLOC(alwan_safe_array_size(c->n * (size_t)c->jcn, sizeof(float)), sizeof(float));
        int it, y, x;
        if (!dif) return 0;
        for (k = 0; k < c->jcn; k++) {
            if (c->use_rng) {
                int const t = (int)(unsigned)(c->rng = amf_rng_next(c->rng));
                init[k] = (float)t * 2.3283064365386963e-10f;
                init[k] = init[k] + 0.0f;
            } else {
                init[k] = (k % 2 == 0) ? 0.5f : -0.5f;
            }
        }
        for (k = 0; k < c->jcn; k++) {
            for (i = 0; i < c->n; i++) dif[(size_t)k * c->n + i] = c->joint[(size_t)k * c->n + i] - c->eta_full[(size_t)k * c->n + i];
        }
        for (it = 0; it < c->pca_iterations; it++) {
            for (y = 0; y < c->h; y++) {
                float *mul = c->tmp2 + (size_t)y * (size_t)c->w;
                size_t const row = amf_at(y, c->w);
                for (k = 0; k < c->jcn; k++) {
                    float const *s = dif + (size_t)k * c->n + row;
                    if (k == 0) for (x = 0; x < c->w; x++) mul[x] = init[k] * s[x];
                    else for (x = 0; x < c->w; x++) mul[x] += init[k] * s[x];
                }
                for (x = 0; x < c->w; x++) if (!cluster[row + (size_t)x]) mul[x] = 0.0f;
                for (k = 0; k < c->jcn; k++) {
                    float const *s = dif + (size_t)k * c->n + row;
                    float acc = 0.0f;
                    for (x = 0; x < c->w; x++) acc += mul[x] * s[x];
                    vd[k] += (double)acc;
                }
            }
        }
        for (k = 0; k < c->jcn; k++) norm += vd[k] * vd[k];
        norm = ALWAN_SQRT_F64(norm);
        for (k = 0; k < c->jcn; k++) vec[k] = (float)(vd[k] / norm);
        for (i = 0; i < c->n; i++) {
            float o = 0.0f;
            for (k = 0; k < c->jcn; k++) {
                if (k == 0) o = vec[k] * dif[(size_t)k * c->n + i];
                else o += vec[k] * dif[(size_t)k * c->n + i];
            }
            orient[i] = o;
        }
        ALWAN_FREE(dif);
    } else {
        for (i = 0; i < c->n; i++) orient[i] = c->joint[i] - c->eta_full[i];
    }
    for (i = 0; i < c->n; i++) {
        minus[i] = (unsigned char)((orient[i] < 0.0f ? 0xFF : 0) & cluster[i]);
        plus[i] = (unsigned char)((orient[i] >= 0.0f ? 0xFF : 0) & cluster[i]);
    }
    return 1;
}

/* A child manifold's eta: the joint image averaged under (1 - w_k) over the cluster, both
 * downsampled and blurred by the recursive filter, then divided. */
static int amf_eta(amf_ctx *c, float const *teta, unsigned char const *cl, float *eta) {
    float const ss = (float)(c->sigma_s / c->df);
    float *masked = c->tmp;
    float *blur = (float *)ALWAN_ALLOC(alwan_safe_array_size(c->ns, sizeof(float)), sizeof(float));
    size_t i;
    int k;
    if (!blur) return 0;
    for (i = 0; i < c->n; i++) masked[i] = cl[i] ? teta[i] : 0.0f;
    if (!amf_down(c, blur, masked)) { ALWAN_FREE(blur); return 0; }
    amf_h_filter(blur, blur, c->hs, c->ws, ss);
    for (k = 0; k < c->jcn; k++) {
        float *e = eta + (size_t)k * c->ns;
        for (i = 0; i < c->n; i++) c->tmp2[i] = masked[i] * c->joint[(size_t)k * c->n + i];
        if (!amf_down(c, e, c->tmp2)) { ALWAN_FREE(blur); return 0; }
        amf_h_filter(e, e, c->hs, c->ws, ss);
        for (i = 0; i < c->ns; i++) e[i] = e[i] / blur[i];
    }
    ALWAN_FREE(blur);
    return 1;
}

/* One manifold and, below the tree's height, its two children, depth first. eta is the
 * full-size first manifold at level 1 and a small one below; this call owns eta and cluster
 * from level 2 on. */
static int amf_build(amf_ctx *c, float *eta, unsigned char *cluster, int level) {
    size_t const ns = c->ns;
    float *eta_small = NULL, *psi = NULL, *hor = NULL, *ver = NULL;
    float *eta_minus = NULL, *eta_plus = NULL, *teta = NULL;
    unsigned char *minus = NULL, *plus = NULL;
    float const rf_ss = (float)(c->sigma_s / c->df);
    int ok = 0, k;
    size_t i;

    if (level == 1) {
        amf_compute_wk(c, eta, level);
        eta_small = (float *)ALWAN_ALLOC(alwan_safe_array_size(ns * (size_t)c->jcn, sizeof(float)), sizeof(float));
        if (!eta_small) goto done;
        for (k = 0; k < c->jcn; k++) if (!amf_down(c, eta_small + (size_t)k * ns, eta + (size_t)k * c->n)) goto done;
    } else {
        eta_small = eta;
        for (k = 0; k < c->jcn; k++) if (!amf_up(c, c->eta_full + (size_t)k * c->n, eta_small + (size_t)k * ns)) goto done;
        amf_compute_wk(c, c->eta_full, level);
    }

    /* splat, blur, slice */
    psi = (float *)ALWAN_ALLOC(alwan_safe_array_size(ns * (size_t)(c->scn + 1), sizeof(float)), sizeof(float));
    hor = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)c->hs * (size_t)(c->ws > 1 ? c->ws - 1 : 1), sizeof(float)), sizeof(float));
    ver = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)(c->hs > 1 ? c->hs - 1 : 1) * (size_t)c->ws, sizeof(float)), sizeof(float));
    if (!psi || !hor || !ver) goto done;
    for (k = 0; k < c->scn; k++) {
        for (i = 0; i < c->n; i++) c->tmp[i] = c->src[(size_t)k * c->n + i] * c->wk[i];
        if (!amf_down(c, psi + (size_t)k * ns, c->tmp)) goto done;
    }
    if (!amf_down(c, psi + (size_t)c->scn * ns, c->wk)) goto done;
    amf_compute_dt(c, eta_small, hor, ver, rf_ss, c->sr_sqrt2);
    for (k = 0; k <= c->scn; k++) amf_rf(psi + (size_t)k * ns, c->hs, c->ws, hor, ver);
    for (k = 0; k <= c->scn; k++) {
        float *acc = k < c->scn ? c->sum + (size_t)k * c->n : c->sum0;
        if (!amf_up(c, c->tmp, psi + (size_t)k * ns)) goto done;
        for (i = 0; i < c->n; i++) {
            float const t = c->tmp[i] * c->wk[i];
            acc[i] = acc[i] + t;
        }
    }
    ALWAN_FREE(psi); psi = NULL;
    ALWAN_FREE(hor); hor = NULL;
    ALWAN_FREE(ver); ver = NULL;

    if (level < c->tree_height) {
        minus = (unsigned char *)ALWAN_ALLOC(c->n, 1);
        plus = (unsigned char *)ALWAN_ALLOC(c->n, 1);
        teta = (float *)ALWAN_ALLOC(alwan_safe_array_size(c->n, sizeof(float)), sizeof(float));
        eta_minus = (float *)ALWAN_ALLOC(alwan_safe_array_size(ns * (size_t)c->jcn, sizeof(float)), sizeof(float));
        eta_plus = (float *)ALWAN_ALLOC(alwan_safe_array_size(ns * (size_t)c->jcn, sizeof(float)), sizeof(float));
        if (!minus || !plus || !teta || !eta_minus || !eta_plus) goto done;
        if (!amf_clusters(c, cluster, minus, plus)) goto done;
        for (i = 0; i < c->n; i++) teta[i] = 1.0f - c->wk[i];
        if (!amf_eta(c, teta, minus, eta_minus) || !amf_eta(c, teta, plus, eta_plus)) goto done;
        ALWAN_FREE(teta); teta = NULL;
        if (level > 1) {
            ALWAN_FREE(eta_small);
            ALWAN_FREE(cluster);
        } else {
            ALWAN_FREE(eta_small);
        }
        eta_small = NULL;
        cluster = NULL;
        ok = amf_build(c, eta_minus, minus, level + 1);
        eta_minus = NULL;
        minus = NULL;
        if (ok) {
            ok = amf_build(c, eta_plus, plus, level + 1);
        } else {
            ALWAN_FREE(eta_plus);
            ALWAN_FREE(plus);
        }
        eta_plus = NULL;
        plus = NULL;
        return ok;
    }
    ok = 1;

done:
    if (psi) ALWAN_FREE(psi);
    if (hor) ALWAN_FREE(hor);
    if (ver) ALWAN_FREE(ver);
    if (teta) ALWAN_FREE(teta);
    if (eta_minus) ALWAN_FREE(eta_minus);
    if (eta_plus) ALWAN_FREE(eta_plus);
    if (minus) ALWAN_FREE(minus);
    if (plus) ALWAN_FREE(plus);
    if (eta_small && (level > 1 || eta_small != eta)) ALWAN_FREE(eta_small);
    if (level > 1 && cluster) ALWAN_FREE(cluster);
    return ok;
}

/* floor(log2 v) for a finite v > 0, counted exactly. OpenCV takes floor(log(v) / log(2)),
 * the same integer except within an ulp or two of a power of two; counting keeps the det
 * build's polynomial logarithm from moving it at the powers of two sigma_s is usually set to. */
static int amf_floor_log2(double v) {
    int e = 0;
    while (v >= 2.0) { v *= 0.5; e++; }
    while (v < 1.0) { v *= 2.0; e--; }
    return e;
}

/* Hs = floor(log2 sigma_s) - 1, Lr = 1 - sigma_r, height max(2, ceil(Hs Lr)). */
static int amf_tree_height(double sigma_s, double sigma_r) {
    double const hs = (double)amf_floor_log2(sigma_s) - 1.0;
    double const lr = 1.0 - sigma_r;
    int const t = (int)ALWAN_CEIL_F64(hs * lr);
    return t > 2 ? t : 2;
}

/* 2 ^ floor(log2 min(sigma_s / 4, 256 sigma_r)), at least 1. */
static double amf_resize_ratio(double sigma_s, double sigma_r) {
    double const m = sigma_s / 4.0 < 256.0 * sigma_r ? sigma_s / 4.0 : 256.0 * sigma_r;
    int e = amf_floor_log2(m);
    double df = 1.0;
    for (; e > 0; e--) df *= 2.0;
    for (; e < 0; e++) df *= 0.5;
    return df > 1.0 ? df : 1.0;
}

alwan_status alwan__amf_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t sch,
                            void const *guide, size_t guide_row_stride, size_t gch, size_t w, size_t h,
                            double sigma_s, double sigma_r, int adjust_outliers, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    amf_ctx c;
    float *eta0 = NULL;
    unsigned char *cluster0 = NULL;
    size_t x, y, i;
    int k, ok;
    double seed, prod;
    alwan_status st = ALWAN_OK;

    if (!out || !src || !guide) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || sch == 0 || sch > 4 || gch == 0 || gch > 4) return ALWAN_E_INVALID;
    if (src_row_stride / elem / sch < w || guide_row_stride / elem / gch < w || out_row_stride / elem / sch < w) {
        return ALWAN_E_INVALID;
    }
    if (!(sigma_s >= 1.0 && sigma_s <= 1e6) || !(sigma_r > 0.0 && sigma_r <= 1.0)) return ALWAN_E_RANGE;
    if (w > (size_t)INT_MAX / 2 || h > (size_t)INT_MAX / 2 || w * h / w != h) return ALWAN_E_RANGE;

    memset(&c, 0, sizeof c);
    c.w = (int)w;
    c.h = (int)h;
    c.n = w * h;
    c.scn = (int)sch;
    c.jcn = (int)gch;
    c.sigma_s = sigma_s;
    c.sigma_r = sigma_r;
    c.df = amf_resize_ratio(sigma_s, sigma_r);
    c.ws = amf_round((double)w * (1.0 / c.df));
    c.hs = amf_round((double)h * (1.0 / c.df));
    if (c.ws < 1 || c.hs < 1) return ALWAN_E_RANGE;
    c.ns = (size_t)c.ws * (size_t)c.hs;
    c.sr_sqrt2 = (float)(sigma_r / ALWAN_SQRT_F64(2.0));
    c.tree_height = amf_tree_height(sigma_s, sigma_r);
    c.pca_iterations = 1;
    c.adjust_outliers = adjust_outliers != 0;
    c.use_rng = 1;

    c.src = (float *)ALWAN_ALLOC(alwan_safe_array_size(c.n * sch, sizeof(float)), sizeof(float));
    c.joint = (float *)ALWAN_ALLOC(alwan_safe_array_size(c.n * gch, sizeof(float)), sizeof(float));
    c.sum = (float *)ALWAN_ALLOC(alwan_safe_array_size(c.n * (sch + 3), sizeof(float)), sizeof(float));
    c.eta_full = (float *)ALWAN_ALLOC(alwan_safe_array_size(c.n * gch, sizeof(float)), sizeof(float));
    c.tmp = (float *)ALWAN_ALLOC(alwan_safe_array_size(c.n * 2, sizeof(float)), sizeof(float));
    cluster0 = (unsigned char *)ALWAN_ALLOC(c.n, 1);
    if (c.adjust_outliers) c.mindist = (float *)ALWAN_ALLOC(alwan_safe_array_size(c.n, sizeof(float)), sizeof(float));
    if (!c.src || !c.joint || !c.sum || !c.eta_full || !c.tmp || !cluster0 || (c.adjust_outliers && !c.mindist)) {
        st = ALWAN_E_NOMEM;
        goto finish;
    }
    c.sum0 = c.sum + c.n * sch;
    c.wk = c.sum0 + c.n;
    c.tmp2 = c.tmp + c.n;
    memset(c.sum, 0, c.n * (sch + 1) * sizeof(float));

    for (y = 0; y < h; y++) {
        char const *srow = (char const *)src + y * src_row_stride;
        char const *grow = (char const *)guide + y * guide_row_stride;
        for (x = 0; x < w * sch; x++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)srow)[x] : ((alwan_f64 const *)srow)[x];
            if (!(v == v && v <= DBL_MAX && v >= -DBL_MAX)) { st = ALWAN_E_INVALID; goto finish; }
            c.src[(x % sch) * c.n + y * w + x / sch] = (float)v;
        }
        for (x = 0; x < w * gch; x++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)grow)[x] : ((alwan_f64 const *)grow)[x];
            if (!(v == v && v <= DBL_MAX && v >= -DBL_MAX)) { st = ALWAN_E_INVALID; goto finish; }
            c.joint[(x % gch) * c.n + y * w + x / gch] = (float)v;
        }
    }

    /* the seed: the centre of the joint's first channel times UINT64_MAX / 0xFFFF, cast to
     * int64 (an out-of-range product reads as INT64_MIN, as x64 converts it) */
    seed = (double)c.joint[(size_t)(h / 2) * w + w / 2];
    prod = 281479271743489.0 * seed;
    if (prod >= -9223372036854775808.0 && prod < 9223372036854775808.0) c.rng = (uint64_t)(int64_t)prod;
    else c.rng = (uint64_t)1 << 63;

    for (i = 0; i < c.n; i++) cluster0[i] = 0xFF;
    eta0 = c.eta_full; /* level 1's eta is the full-size etaFull */
    for (k = 0; k < c.jcn; k++) amf_h_filter(eta0 + (size_t)k * c.n, c.joint + (size_t)k * c.n, c.h, c.w, (float)sigma_s);
    ok = amf_build(&c, eta0, cluster0, 1);
    if (!ok) { st = ALWAN_E_NOMEM; goto finish; }

    for (k = 0; k < c.scn; k++) {
        float *g = c.sum + (size_t)k * c.n;
        float const *f = c.src + (size_t)k * c.n;
        if (!c.adjust_outliers) {
            for (i = 0; i < c.n; i++) g[i] = g[i] / c.sum0[i];
        } else {
            /* cv::multiply by a double scalar works in double, then rounds */
            double const member = -0.5 / (sigma_r * sigma_r);
            if (k == 0) {
                for (i = 0; i < c.n; i++) c.mindist[i] = (float)((double)c.mindist[i] * member);
                amf_exp(c.mindist, c.n);
            }
            for (i = 0; i < c.n; i++) {
                float t = g[i] / c.sum0[i];
                t = t - f[i];
                t = c.mindist[i] * t;
                g[i] = t + f[i];
            }
        }
    }
    for (y = 0; y < h; y++) {
        char *orow = (char *)out + y * out_row_stride;
        for (x = 0; x < w * sch; x++) {
            float const v = c.sum[(x % sch) * c.n + y * w + x / sch];
            if (is_f32) ((alwan_f32 *)orow)[x] = v;
            else ((alwan_f64 *)orow)[x] = (alwan_f64)v;
        }
    }

finish:
    if (c.src) ALWAN_FREE(c.src);
    if (c.joint) ALWAN_FREE(c.joint);
    if (c.sum) ALWAN_FREE(c.sum);
    if (c.eta_full) ALWAN_FREE(c.eta_full);
    if (c.tmp) ALWAN_FREE(c.tmp);
    if (c.mindist) ALWAN_FREE(c.mindist);
    if (cluster0) ALWAN_FREE(cluster0);
    return st;
}

/* cv::resize of one packed float plane to dw x dh, for the other OpenCV ports
 * (alwan_optical_flow.c): INTER_LINEAR, its fast 2x2 area path at a factor of exactly 2, a
 * copy at the same size. 0 when a buffer cannot be had. */
int alwan__cv_resize_f32_plane(float *dst, int dw, int dh, float const *src, int sw, int sh) {
    return amf_resize(dst, dw, dh, src, sw, sh, (double)dw / (double)sw, (double)dh / (double)sh);
}
