/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The segmentation family's FELZENSZWALB, QUICKSHIFT, CHAN_VESE and RANDOM_WALKER, called
 * by alwan_segment_{T} with the image read into doubles (alwan_segment.c).
 *
 * FELZENSZWALB and QUICKSHIFT are ports of scikit-image 0.26's _felzenszwalb_cy.pyx and
 * _quickshift_cy.pyx; CHAN_VESE follows _chan_vese.py operation for operation and
 * RANDOM_WALKER random_walker_segmentation.py, with alwan's own linear solvers.
 * scikit-image is Copyright (C) 2019, the scikit-image team, BSD-3-Clause:
 *
 *   Redistribution and use in source and binary forms, with or without modification, are
 *   permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright notice, this list
 *       of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright notice, this
 *       list of conditions and the following disclaimer in the documentation and/or other
 *       materials provided with the distribution.
 *    3. Neither the name of skimage nor the names of its contributors may be used to
 *       endorse or promote products derived from this software without specific prior
 *       written permission.
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *   AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 *   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * FELZENSZWALB: the costs sqrt of the channel sum of squared differences (numpy's order),
 * the edges right, down, down-right, up-right as scikit-image stacks them, sorted by cost
 * (stably: numpy's argsort is not, and only equal non-zero costs could order differently;
 * the partition, not the root, decides a label, because the union keeps the smaller root),
 * the internal costs compared as float32 as the Cython declares them, then the min_size
 * pass over the same order. Labels in the raster order of each region's first pixel.
 *
 * QUICKSHIFT: densities and parents in the data's precision as the Cython's fused type,
 * exp in double, a parent only where a pixel in the window is strictly denser, the nearest
 * such by distance in colour and position; roots ranked by their pixel index.
 *
 * CHAN_VESE: every array expression in numpy's operation order, the sums numpy's pairwise
 * ones, float32 data computed in float32 with each operation rounded.
 *
 * RANDOM_WALKER: the 4-neighbour graph's weights in double, the Laplacian restricted to
 * the unlabelled pixels, one right-hand side per label from its seeds' weights. DIRECT
 * factors the restricted Laplacian as L D L^T with a band of the unknowns in the shorter
 * side's raster order; CG_JACOBI is scipy's preconditioned conjugate gradients stopping at
 * ||r|| <= tol ||b|| (atol 0, at most 10 n iterations).
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double alwan_sx_r(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

/* numpy's pairwise sum over n values, each addition rounded to float for float32 data */
static double alwan_sx_sum(double const *a, size_t n, int f32) {
    if (n < 8) {
        double r = 0.0;
        size_t i;
        for (i = 0; i < n; i++) r = alwan_sx_r(r + a[i], f32);
        return r;
    }
    if (n <= 128) {
        double r[8], res;
        size_t i, j;
        for (j = 0; j < 8; j++) r[j] = a[j];
        for (i = 8; i < n - (n % 8); i += 8)
            for (j = 0; j < 8; j++) r[j] = alwan_sx_r(r[j] + a[i + j], f32);
        res = alwan_sx_r(alwan_sx_r(alwan_sx_r(r[0] + r[1], f32) + alwan_sx_r(r[2] + r[3], f32), f32) +
                             alwan_sx_r(alwan_sx_r(r[4] + r[5], f32) + alwan_sx_r(r[6] + r[7], f32), f32),
                         f32);
        for (; i < n; i++) res = alwan_sx_r(res + a[i], f32);
        return res;
    }
    {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan_sx_r(alwan_sx_sum(a, n2, f32) + alwan_sx_sum(a + n2, n - n2, f32), f32);
    }
}

static size_t alwan_sx_root(size_t *par, size_t p) {
    while (par[p] != p) {
        par[p] = par[par[p]];
        p = par[p];
    }
    return p;
}

static void alwan_sx_write(uint32_t *labels, size_t labels_rs, size_t const *lab, size_t w, size_t h, size_t *count_out) {
    size_t x, y, maxl = 0;
    for (y = 0; y < h; y++) {
        uint32_t *row = (uint32_t *)((char *)labels + y * labels_rs);
        for (x = 0; x < w; x++) {
            row[x] = (uint32_t)lab[y * w + x];
            if (lab[y * w + x] > maxl) maxl = lab[y * w + x];
        }
    }
    if (count_out) *count_out = maxl;
}

/* ---------------------------------------------------------------------------------------
 * FELZENSZWALB
 * ------------------------------------------------------------------------------------- */

/* a stable merge sort of the indices by their key */
static void alwan_sx_msort(size_t *ix, size_t *tmp, size_t n, double const *key) {
    size_t width;
    for (width = 1; width < n; width *= 2) {
        size_t lo;
        for (lo = 0; lo < n; lo += 2 * width) {
            size_t mid = lo + width < n ? lo + width : n, hi = lo + 2 * width < n ? lo + 2 * width : n, a = lo, b = mid, k = lo;
            while (a < mid && b < hi) tmp[k++] = key[ix[b]] < key[ix[a]] ? ix[b++] : ix[a++];
            while (a < mid) tmp[k++] = ix[a++];
            while (b < hi) tmp[k++] = ix[b++];
        }
        memcpy(ix, tmp, n * sizeof(size_t));
    }
}

static alwan_status alwan_sx_felzenszwalb(size_t *lab, double *v, size_t w, size_t h, size_t ch, alwan_segment_params const *p) {
    double const scale = (p->scale > 0.0 ? p->scale : 1.0) / 255.0;
    double const sigma = p->sigma == 0.0 ? 0.8 : p->sigma;
    size_t const min_size = p->min_size ? p->min_size : 20;
    size_t const n = w * h;
    size_t const nr = h * (w - 1), nd = (h - 1) * w, ndr = (h - 1) * (w - 1), ne = nr + nd + 2 * ndr;
    size_t *ea = NULL, *ix = NULL, *tmp = NULL, *par = NULL, *size = NULL;
    double *cost = NULL, *cint = NULL;
    size_t e, x, y, c, k, next;
    alwan_status st = ALWAN_E_NOMEM;
    if (sigma > 0.0) {
        alwan_filter_params fp;
        memset(&fp, 0, sizeof(fp));
        fp.sigma = fp.sigma_row = fp.sigma_col = sigma;
        fp.border = ALWAN_FILTER_BORDER_REFLECT;
        st = alwan_filter_f64(v, w * ch * sizeof(double), v, w * ch * sizeof(double), ch, w, h, ALWAN_FILTER_GAUSSIAN, &fp);
        if (st != ALWAN_OK) return st;
        st = ALWAN_E_NOMEM;
    }
    ea = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(ne ? ne : 1, 2 * sizeof(size_t)), sizeof(size_t));
    ix = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(ne ? ne : 1, 2 * sizeof(size_t)), sizeof(size_t));
    cost = (double *)ALWAN_ALLOC(alwan_safe_array_size(ne ? ne : 1, sizeof(double)), sizeof(double));
    par = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(size_t)), sizeof(size_t));
    cint = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    if (!ea || !ix || !cost || !par || !cint) goto done;
    tmp = ix + ne;
    size = par + n;
    /* the edges as scikit-image stacks them: right, down, down-right, up-right; each pair
     * (later pixel, earlier) as np.c_ builds it, the cost the colour distance */
    e = 0;
#define ALWAN_SX_EDGE(A, B)                                                       \
    do {                                                                          \
        size_t const a_ = (A), b_ = (B);                                          \
        double s_ = 0.0;                                                          \
        for (c = 0; c < ch; c++) {                                                \
            double const d_ = v[a_ * ch + c] - v[b_ * ch + c];                    \
            s_ += d_ * d_;                                                        \
        }                                                                         \
        ea[2 * e] = a_;                                                           \
        ea[2 * e + 1] = b_;                                                       \
        cost[e] = ALWAN_SQRT_F64(s_);                                             \
        e++;                                                                      \
    } while (0)
    for (y = 0; y < h; y++)
        for (x = 0; x + 1 < w; x++) ALWAN_SX_EDGE(y * w + x + 1, y * w + x);
    for (y = 0; y + 1 < h; y++)
        for (x = 0; x < w; x++) ALWAN_SX_EDGE((y + 1) * w + x, y * w + x);
    for (y = 0; y + 1 < h; y++)
        for (x = 0; x + 1 < w; x++) ALWAN_SX_EDGE((y + 1) * w + x + 1, y * w + x);
    for (y = 0; y + 1 < h; y++)
        for (x = 0; x + 1 < w; x++) ALWAN_SX_EDGE(y * w + x + 1, (y + 1) * w + x);
#undef ALWAN_SX_EDGE
    for (e = 0; e < ne; e++) ix[e] = e;
    alwan_sx_msort(ix, tmp, ne, cost);
    for (k = 0; k < n; k++) {
        par[k] = k;
        size[k] = 1;
        cint[k] = 0.0;
    }
    for (e = 0; e < ne; e++) {
        size_t const i = ix[e];
        size_t const s0 = alwan_sx_root(par, ea[2 * i]), s1 = alwan_sx_root(par, ea[2 * i + 1]);
        float in0, in1;
        size_t sn;
        if (s0 == s1) continue;
        in0 = (float)(cint[s0] + scale / (double)size[s0]);
        in1 = (float)(cint[s1] + scale / (double)size[s1]);
        if (cost[i] < (double)(in0 < in1 ? in0 : in1)) {
            sn = s0 < s1 ? s0 : s1;
            par[s0] = sn;
            par[s1] = sn;
            size[sn] = size[s0] + size[s1];
            cint[sn] = cost[i];
        }
    }
    for (e = 0; e < ne; e++) {
        size_t const i = ix[e];
        size_t const s0 = alwan_sx_root(par, ea[2 * i]), s1 = alwan_sx_root(par, ea[2 * i + 1]);
        size_t sn;
        if (s0 == s1) continue;
        if (size[s0] < min_size || size[s1] < min_size) {
            sn = s0 < s1 ? s0 : s1;
            par[s0] = sn;
            par[s1] = sn;
            size[sn] = size[s0] + size[s1];
        }
    }
    /* the union keeps the smaller root, so a region's root is its first pixel in raster
     * order and the roots met in raster order are numbered 1, 2, ... (scikit-image's
     * np.unique, plus 1) */
    next = 0;
    for (k = 0; k < n; k++) {
        size_t const r = alwan_sx_root(par, k);
        if (r == k) {
            size[k] = ++next;
            lab[k] = next;
        } else {
            lab[k] = size[r];
        }
    }
    st = ALWAN_OK;
done:
    ALWAN_FREE(ea);
    ALWAN_FREE(ix);
    ALWAN_FREE(cost);
    ALWAN_FREE(par);
    ALWAN_FREE(cint);
    return st;
}

/* ---------------------------------------------------------------------------------------
 * QUICKSHIFT
 * ------------------------------------------------------------------------------------- */

static alwan_status alwan_sx_quickshift(size_t *lab, double *v, size_t w, size_t h, size_t ch, alwan_segment_params const *p, int f32) {
    double const ratio = p->ratio > 0.0 ? p->ratio : 1.0;
    double const ks_in = p->kernel_size > 0.0 ? p->kernel_size : 5.0;
    double const md_in = p->max_dist > 0.0 ? p->max_dist : 10.0;
    size_t const n = w * h;
    double ks, md, inv;
    size_t kw, r, cc, r2, c2;
    double *dens = NULL;
    size_t *par = NULL, *rank, k, next, i;
    alwan_status st;
    if (p->sigma > 0.0) {
        alwan_filter_params fp;
        memset(&fp, 0, sizeof(fp));
        fp.sigma = fp.sigma_row = fp.sigma_col = p->sigma;
        fp.border = ALWAN_FILTER_BORDER_REFLECT;
        if (f32) {
#if ALWAN_WITH_F32
            float *t = (float *)ALWAN_ALLOC(alwan_safe_array_size(n * ch, sizeof(float)), sizeof(float));
            if (!t) return ALWAN_E_NOMEM;
            for (i = 0; i < n * ch; i++) t[i] = (float)v[i];
            st = alwan_filter_f32(t, w * ch * sizeof(float), t, w * ch * sizeof(float), ch, w, h, ALWAN_FILTER_GAUSSIAN, &fp);
            for (i = 0; i < n * ch; i++) v[i] = (double)t[i];
            ALWAN_FREE(t);
#else
            st = ALWAN_E_INVALID;   /* unreachable: an f64-only build has no alwan_segment_f32 */
#endif
        } else {
            st = alwan_filter_f64(v, w * ch * sizeof(double), v, w * ch * sizeof(double), ch, w, h, ALWAN_FILTER_GAUSSIAN, &fp);
        }
        if (st != ALWAN_OK) return st;
    }
    /* image * ratio, the Python float taken in the array's precision */
    for (i = 0; i < n * ch; i++) v[i] = alwan_sx_r(v[i] * alwan_sx_r(ratio, f32), f32);
    ks = alwan_sx_r(ks_in, f32);
    md = alwan_sx_r(md_in, f32);
    inv = alwan_sx_r(-0.5 / alwan_sx_r(ks * ks, f32), f32);
    kw = (size_t)ALWAN_CEIL_F64(alwan_sx_r(3.0 * ks, f32));   /* ks >= 1: at least 3 */
    dens = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    par = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(size_t)), sizeof(size_t));
    if (!dens || !par) {
        ALWAN_FREE(dens);
        ALWAN_FREE(par);
        return ALWAN_E_NOMEM;
    }
    rank = par + n;
    for (k = 0; k < n; k++) {
        dens[k] = 0.0;
        par[k] = k;
    }
    for (r = 0; r < h; r++) {
        size_t const r0 = r > kw ? r - kw : 0, r1 = r + kw + 1 < h ? r + kw + 1 : h;
        for (cc = 0; cc < w; cc++) {
            size_t const c0 = cc > kw ? cc - kw : 0, c1 = cc + kw + 1 < w ? cc + kw + 1 : w;
            size_t const me = r * w + cc;
            double const *cur = v + me * ch;
            double acc = dens[me];
            for (r2 = r0; r2 < r1; r2++)
                for (c2 = c0; c2 < c1; c2++) {
                    double const *o = v + (r2 * w + c2) * ch;
                    double dist = 0.0, t;
                    size_t q;
                    for (q = 0; q < ch; q++) {
                        t = alwan_sx_r(cur[q] - o[q], f32);
                        dist = alwan_sx_r(dist + alwan_sx_r(t * t, f32), f32);
                    }
                    t = (double)r - (double)r2;
                    dist = alwan_sx_r(dist + alwan_sx_r(t * t, f32), f32);
                    t = (double)cc - (double)c2;
                    dist = alwan_sx_r(dist + alwan_sx_r(t * t, f32), f32);
                    acc = alwan_sx_r(acc + ALWAN_EXP_F64(alwan_sx_r(dist * inv, f32)), f32);
                }
            dens[me] = acc;
        }
    }
    for (r = 0; r < h; r++) {
        size_t const r0 = r > kw ? r - kw : 0, r1 = r + kw + 1 < h ? r + kw + 1 : h;
        for (cc = 0; cc < w; cc++) {
            size_t const c0 = cc > kw ? cc - kw : 0, c1 = cc + kw + 1 < w ? cc + kw + 1 : w;
            size_t const me = r * w + cc;
            double const *cur = v + me * ch;
            double const cd = dens[me];
            double closest = f32 ? (double)HUGE_VALF : DBL_MAX;
            for (r2 = r0; r2 < r1; r2++)
                for (c2 = c0; c2 < c1; c2++) {
                    size_t const oi = r2 * w + c2;
                    double const *o = v + oi * ch;
                    double dist = 0.0, t;
                    size_t q;
                    if (!(dens[oi] > cd)) continue;
                    for (q = 0; q < ch; q++) {
                        t = alwan_sx_r(cur[q] - o[q], f32);
                        dist = alwan_sx_r(dist + alwan_sx_r(t * t, f32), f32);
                    }
                    t = (double)r - (double)r2;
                    dist = alwan_sx_r(dist + alwan_sx_r(t * t, f32), f32);
                    t = (double)cc - (double)c2;
                    dist = alwan_sx_r(dist + alwan_sx_r(t * t, f32), f32);
                    if (dist < closest) {
                        closest = dist;
                        par[me] = oi;
                    }
                }
            /* a link longer than max_dist is cut: the pixel is its own root */
            if (alwan_sx_r(ALWAN_SQRT_F64(closest), f32) > md) par[me] = me;
        }
    }
    /* roots ranked by their pixel index (np.unique), 1-based */
    next = 0;
    for (k = 0; k < n; k++)
        if (par[k] == k) rank[k] = ++next;
    for (k = 0; k < n; k++) {
        size_t q = k;
        while (par[q] != q) q = par[q];
        lab[k] = rank[q];
    }
    ALWAN_FREE(dens);
    ALWAN_FREE(par);
    return ALWAN_OK;
}

/* ---------------------------------------------------------------------------------------
 * CHAN_VESE
 * ------------------------------------------------------------------------------------- */

/* P = np.pad(phi, 1, 'edge') read at (y + dy, x + dx) of phi's own coordinates */
static double alwan_sx_pad(double const *phi, size_t w, size_t h, long y, long x) {
    if (y < 0) y = 0;
    if (y >= (long)h) y = (long)h - 1;
    if (x < 0) x = 0;
    if (x >= (long)w) x = (long)w - 1;
    return phi[(size_t)y * w + (size_t)x];
}

/* _cv_calculate_averages: (sum(img * H) / sum(H), sum(img * (1 - H)) / sum(1 - H)) */
static void alwan_sx_averages(double *c1, double *c2, double const *img, double const *H, double *t1, double *t2, size_t n, int f32) {
    double hs, his, ai, ao;
    size_t i;
    for (i = 0; i < n; i++) t1[i] = alwan_sx_r(1.0 - H[i], f32);   /* Hinv */
    hs = alwan_sx_sum(H, n, f32);
    his = alwan_sx_sum(t1, n, f32);
    for (i = 0; i < n; i++) t2[i] = alwan_sx_r(img[i] * H[i], f32);
    ai = alwan_sx_sum(t2, n, f32);
    for (i = 0; i < n; i++) t2[i] = alwan_sx_r(img[i] * t1[i], f32);
    ao = alwan_sx_sum(t2, n, f32);
    if (hs != 0.0) ai = alwan_sx_r(ai / hs, f32);
    if (his != 0.0) ao = alwan_sx_r(ao / his, f32);
    *c1 = ai;
    *c2 = ao;
}

/* _cv_energy */
static double alwan_sx_cv_energy(double const *img, double const *phi, double mu, double l1, double l2, size_t w, size_t h, double *H,
                                 double *t1, double *t2, int f32) {
    size_t const n = w * h;
    double const two_over_pi = alwan_sx_r(2.0 / 3.141592653589793, f32);
    double c1, c2, ea, el;
    size_t i, x, y;
    for (i = 0; i < n; i++) {
        double const at = f32 ? (double)ALWAN_ATAN_F32((float)phi[i]) : ALWAN_ATAN_F64(phi[i]);
        H[i] = alwan_sx_r(0.5 * alwan_sx_r(1.0 + alwan_sx_r(two_over_pi * at, f32), f32), f32);
    }
    alwan_sx_averages(&c1, &c2, img, H, t1, t2, n, f32);
    /* lambda_pos * (image - c1)**2 * Hphi + lambda_neg * (image - c2)**2 * Hinv; t1 holds Hinv */
    for (i = 0; i < n; i++) {
        double const d1 = alwan_sx_r(img[i] - c1, f32), d2 = alwan_sx_r(img[i] - c2, f32);
        double const a = alwan_sx_r(alwan_sx_r(l1 * alwan_sx_r(d1 * d1, f32), f32) * H[i], f32);
        double const b = alwan_sx_r(alwan_sx_r(l2 * alwan_sx_r(d2 * d2, f32), f32) * t1[i], f32);
        t2[i] = alwan_sx_r(a + b, f32);
    }
    ea = alwan_sx_sum(t2, n, f32);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            size_t const o = y * w + x;
            double const fy = alwan_sx_r(alwan_sx_r(alwan_sx_pad(phi, w, h, (long)y + 1, (long)x) - alwan_sx_pad(phi, w, h, (long)y - 1, (long)x), f32) / 2.0, f32);
            double const fx = alwan_sx_r(alwan_sx_r(alwan_sx_pad(phi, w, h, (long)y, (long)x + 1) - alwan_sx_pad(phi, w, h, (long)y, (long)x - 1), f32) / 2.0, f32);
            double const del = alwan_sx_r(1.0 / alwan_sx_r(1.0 + alwan_sx_r(phi[o] * phi[o], f32), f32), f32);
            double const g = alwan_sx_r(ALWAN_SQRT_F64(alwan_sx_r(alwan_sx_r(fx * fx, f32) + alwan_sx_r(fy * fy, f32), f32)), f32);
            t2[o] = alwan_sx_r(alwan_sx_r(mu * del, f32) * g, f32);
        }
    el = alwan_sx_sum(t2, n, f32);
    return alwan_sx_r(ea + el, f32);
}

static alwan_status alwan_sx_chan_vese(size_t *lab, double *img, size_t w, size_t h, alwan_segment_params const *p, int f32) {
    int const exact = p->chan_vese_exact != 0;
    double const mu = alwan_sx_r(exact ? p->mu : (p->mu != 0.0 ? p->mu : 0.25), f32);
    double const l1 = alwan_sx_r(exact ? p->lambda1 : (p->lambda1 != 0.0 ? p->lambda1 : 1.0), f32);
    double const l2 = alwan_sx_r(exact ? p->lambda2 : (p->lambda2 != 0.0 ? p->lambda2 : 1.0), f32);
    double const tol = exact ? p->tol : (p->tol != 0.0 ? p->tol : 1e-3);
    double const dt_in = p->dt != 0.0 ? p->dt : 0.5;
    double const dt = alwan_sx_r(dt_in, f32);
    /* mu * dt is a Python product before it meets the array */
    double const mudt = alwan_sx_r((exact ? p->mu : (p->mu != 0.0 ? p->mu : 0.25)) * dt_in, f32);
    size_t const max_it = p->max_iterations ? p->max_iterations : 500;
    size_t const n = w * h;
    double *phi = NULL, *nphi = NULL, *H = NULL, *t1 = NULL, *t2 = NULL, *cs = NULL;
    double mn, mx, old_energy, phivar;
    size_t i, x, y, it = 0;
    alwan_status st = ALWAN_E_NOMEM;
    phi = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 6 * sizeof(double)), sizeof(double));
    if (!phi) return ALWAN_E_NOMEM;
    nphi = phi + n;
    H = nphi + n;
    t1 = H + n;
    t2 = t1 + n;
    cs = t2 + n;
    /* the starting level set, cast to the data's precision */
    if (p->init == ALWAN_CHAN_VESE_INIT_CHECKERBOARD) {
        /* xv *= np.pi / 5 in place: the factor taken in the array's precision */
        double const sf = alwan_sx_r(3.141592653589793 / 5.0, f32);
        for (y = 0; y < h; y++) {
            double const yv = alwan_sx_r((double)y * sf, f32);
            double const sy = f32 ? (double)ALWAN_SIN_F32((float)yv) : ALWAN_SIN_F64(yv);
            for (x = 0; x < w; x++) {
                double const xv = alwan_sx_r((double)x * sf, f32);
                double const sx = f32 ? (double)ALWAN_SIN_F32((float)xv) : ALWAN_SIN_F64(xv);
                phi[y * w + x] = alwan_sx_r(sy * sx, f32);
            }
        }
    } else if (p->init == ALWAN_CHAN_VESE_INIT_DISK || p->init == ALWAN_CHAN_VESE_INIT_SMALL_DISK) {
        long const cy = (long)((double)(h - 1) / 2.0), cx = (long)((double)(w - 1) / 2.0);
        double radius = (double)(cx < cy ? cx : cy);
        if (p->init == ALWAN_CHAN_VESE_INIT_SMALL_DISK) radius = radius / 2.0;
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                double const dy = (double)((long)y - cy), dx = (double)((long)x - cx);
                double const d = ALWAN_SQRT_F64(dy * dy + dx * dx);
                double const val = p->init == ALWAN_CHAN_VESE_INIT_DISK ? (radius - d) / radius : (radius - d) / (radius * 3.0);
                phi[y * w + x] = alwan_sx_r(val, f32);
            }
    } else {
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                phi[y * w + x] =
                    alwan_sx_r(((double const *)((char const *)p->init_level_set_values + y * p->init_level_set_row_stride))[x], f32);
    }
    /* the image cast, shifted to 0 and divided by its maximum */
    mn = img[0];
    for (i = 1; i < n; i++)
        if (img[i] < mn) mn = img[i];
    for (i = 0; i < n; i++) img[i] = alwan_sx_r(alwan_sx_r(img[i], f32) - alwan_sx_r(mn, f32), f32);
    mx = img[0];
    for (i = 1; i < n; i++)
        if (img[i] > mx) mx = img[i];
    if (mx != 0.0)
        for (i = 0; i < n; i++) img[i] = alwan_sx_r(img[i] / mx, f32);
    old_energy = alwan_sx_cv_energy(img, phi, mu, l1, l2, w, h, H, t1, t2, f32);
    phivar = tol + 1.0;
    while (phivar > alwan_sx_r(tol, f32) && it < max_it) {
        double c1, c2;
        /* Hphi = (phi > 0) */
        for (i = 0; i < n; i++) H[i] = phi[i] > 0.0 ? 1.0 : 0.0;
        alwan_sx_averages(&c1, &c2, img, H, t1, t2, n, f32);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                size_t const o = y * w + x;
                long const yy = (long)y, xx = (long)x;
                double const pc = phi[o];
                double const pxp = alwan_sx_pad(phi, w, h, yy, xx + 1), pxn = alwan_sx_pad(phi, w, h, yy, xx - 1);
                double const pyp = alwan_sx_pad(phi, w, h, yy + 1, xx), pyn = alwan_sx_pad(phi, w, h, yy - 1, xx);
                double const phixp = alwan_sx_r(pxp - pc, f32), phixn = alwan_sx_r(pc - pxn, f32);
                double const phix0 = alwan_sx_r(alwan_sx_r(pxp - pxn, f32) / 2.0, f32);
                double const phiyp = alwan_sx_r(pyp - pc, f32), phiyn = alwan_sx_r(pc - pyn, f32);
                double const phiy0 = alwan_sx_r(alwan_sx_r(pyp - pyn, f32) / 2.0, f32);
                double const eta = alwan_sx_r(1e-16, f32);
                double const C1 = alwan_sx_r(1.0 / alwan_sx_r(ALWAN_SQRT_F64(alwan_sx_r(alwan_sx_r(eta + alwan_sx_r(phixp * phixp, f32), f32) + alwan_sx_r(phiy0 * phiy0, f32), f32)), f32), f32);
                double const C2 = alwan_sx_r(1.0 / alwan_sx_r(ALWAN_SQRT_F64(alwan_sx_r(alwan_sx_r(eta + alwan_sx_r(phixn * phixn, f32), f32) + alwan_sx_r(phiy0 * phiy0, f32), f32)), f32), f32);
                double const C3 = alwan_sx_r(1.0 / alwan_sx_r(ALWAN_SQRT_F64(alwan_sx_r(alwan_sx_r(eta + alwan_sx_r(phix0 * phix0, f32), f32) + alwan_sx_r(phiyp * phiyp, f32), f32)), f32), f32);
                double const C4 = alwan_sx_r(1.0 / alwan_sx_r(ALWAN_SQRT_F64(alwan_sx_r(alwan_sx_r(eta + alwan_sx_r(phix0 * phix0, f32), f32) + alwan_sx_r(phiyn * phiyn, f32), f32)), f32), f32);
                double const K = alwan_sx_r(alwan_sx_r(alwan_sx_r(alwan_sx_r(pxp * C1, f32) + alwan_sx_r(pxn * C2, f32), f32) + alwan_sx_r(pyp * C3, f32), f32) +
                                                alwan_sx_r(pyn * C4, f32),
                                            f32);
                double const d1 = alwan_sx_r(img[o] - c1, f32), d2 = alwan_sx_r(img[o] - c2, f32);
                double const diff = alwan_sx_r(alwan_sx_r(-l1 * alwan_sx_r(d1 * d1, f32), f32) + alwan_sx_r(l2 * alwan_sx_r(d2 * d2, f32), f32), f32);
                double const del = alwan_sx_r(1.0 / alwan_sx_r(1.0 + alwan_sx_r(pc * pc, f32), f32), f32);
                double const np_ = alwan_sx_r(pc + alwan_sx_r(alwan_sx_r(dt * del, f32) * alwan_sx_r(alwan_sx_r(mu * K, f32) + diff, f32), f32), f32);
                double const csum = alwan_sx_r(alwan_sx_r(alwan_sx_r(C1 + C2, f32) + C3, f32) + C4, f32);
                double const den = alwan_sx_r(1.0 + alwan_sx_r(alwan_sx_r(mudt * del, f32) * csum, f32), f32);
                nphi[o] = alwan_sx_r(np_ / den, f32);
            }
        for (i = 0; i < n; i++) {
            double const d = alwan_sx_r(nphi[i] - phi[i], f32);
            cs[i] = alwan_sx_r(d * d, f32);
        }
        phivar = alwan_sx_r(ALWAN_SQRT_F64(alwan_sx_r(alwan_sx_sum(cs, n, f32) / (double)n, f32)), f32);
        memcpy(phi, nphi, n * sizeof(double));
        if (p->energies && it < p->energies_capacity) p->energies[it] = old_energy;
        old_energy = alwan_sx_cv_energy(img, phi, mu, l1, l2, w, h, H, t1, t2, f32);
        it++;
    }
    if (p->energies_count) *p->energies_count = it;
    for (i = 0; i < n; i++) lab[i] = phi[i] > 0.0 ? 1u : 0u;
    if (p->level_set)
        for (y = 0; y < h; y++) {
            double *row = (double *)((char *)p->level_set + y * p->level_set_row_stride);
            for (x = 0; x < w; x++) row[x] = phi[y * w + x];
        }
    st = ALWAN_OK;
    ALWAN_FREE(phi);
    return st;
}

/* ---------------------------------------------------------------------------------------
 * RANDOM_WALKER
 * ------------------------------------------------------------------------------------- */

typedef struct {
    size_t w, h, nu;
    size_t const *uidx;      /* pixel -> unknown index + 1, 0 for a seed */
    double const *wr, *wd;   /* the right and down edge weights, h x (w - 1) and (h - 1) x w */
    double const *diag;      /* the Laplacian's diagonal at each pixel */
    size_t const *upix;      /* unknown -> pixel */
} alwan_sx_rw;

/* y = L_U x over the unknowns */
static void alwan_sx_rw_apply(alwan_sx_rw const *g, double const *xv, double *yv) {
    size_t u;
    for (u = 0; u < g->nu; u++) {
        size_t const pix = g->upix[u], y = pix / g->w, x = pix % g->w;
        double s = g->diag[pix] * xv[u];
        if (x + 1 < g->w && g->uidx[pix + 1]) s -= g->wr[y * (g->w - 1) + x] * xv[g->uidx[pix + 1] - 1];
        if (x > 0 && g->uidx[pix - 1]) s -= g->wr[y * (g->w - 1) + x - 1] * xv[g->uidx[pix - 1] - 1];
        if (y + 1 < g->h && g->uidx[pix + g->w]) s -= g->wd[y * g->w + x] * xv[g->uidx[pix + g->w] - 1];
        if (y > 0 && g->uidx[pix - g->w]) s -= g->wd[(y - 1) * g->w + x] * xv[g->uidx[pix - g->w] - 1];
        yv[u] = s;
    }
}

static double alwan_sx_dot(double const *a, double const *b, size_t n) {
    double s = 0.0;
    size_t i;
    for (i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

/* scipy.sparse.linalg.cg with the Jacobi preconditioner, x0 = 0 */
static void alwan_sx_rw_cg(alwan_sx_rw const *g, double const *b, double *xv, double tol, double *r, double *z, double *pv, double *q) {
    size_t const n = g->nu, maxit = 10 * n;
    double const bn = ALWAN_SQRT_F64(alwan_sx_dot(b, b, n)), stop = tol * bn;
    double rho_prev = 0.0;
    size_t i, it;
    for (i = 0; i < n; i++) {
        xv[i] = 0.0;
        r[i] = b[i];
    }
    if (bn == 0.0) return;
    for (it = 0; it < maxit; it++) {
        double rho, alpha;
        if (ALWAN_SQRT_F64(alwan_sx_dot(r, r, n)) <= stop) break;
        for (i = 0; i < n; i++) z[i] = r[i] / g->diag[g->upix[i]];
        rho = alwan_sx_dot(r, z, n);
        if (it == 0) {
            for (i = 0; i < n; i++) pv[i] = z[i];
        } else {
            double const beta = rho / rho_prev;
            for (i = 0; i < n; i++) pv[i] = z[i] + beta * pv[i];
        }
        alwan_sx_rw_apply(g, pv, q);
        alpha = rho / alwan_sx_dot(pv, q, n);
        for (i = 0; i < n; i++) {
            xv[i] += alpha * pv[i];
            r[i] -= alpha * q[i];
        }
        rho_prev = rho;
    }
}

/* L_U = L D L^T with a band of bw below the diagonal, the unknowns in the order of ord
 * (unknown -> position); ab holds the band row by row, nu x (bw + 1), the diagonal last. */
static void alwan_sx_rw_factor(alwan_sx_rw const *g, size_t const *pos, size_t const *at, size_t bw, double *ab) {
    size_t const n = g->nu;
    size_t i, j, k;
    /* fill the band with L_U, row i (by position) holding columns i - bw .. i */
    for (i = 0; i < n * (bw + 1); i++) ab[i] = 0.0;
    for (i = 0; i < n; i++) {
        size_t const pix = g->upix[at[i]], y = pix / g->w, x = pix % g->w;
        ab[i * (bw + 1) + bw] = g->diag[pix];
#define ALWAN_SX_NB(Q, WT)                                              \
        do {                                                            \
            size_t const q_ = (Q);                                      \
            if (g->uidx[q_]) {                                          \
                size_t const jp_ = pos[g->uidx[q_] - 1];                \
                if (jp_ < i) ab[i * (bw + 1) + bw - (i - jp_)] = -(WT); \
            }                                                           \
        } while (0)
        if (x + 1 < g->w) ALWAN_SX_NB(pix + 1, g->wr[y * (g->w - 1) + x]);
        if (x > 0) ALWAN_SX_NB(pix - 1, g->wr[y * (g->w - 1) + x - 1]);
        if (y + 1 < g->h) ALWAN_SX_NB(pix + g->w, g->wd[y * g->w + x]);
        if (y > 0) ALWAN_SX_NB(pix - g->w, g->wd[(y - 1) * g->w + x]);
#undef ALWAN_SX_NB
    }
    /* L D L^T in place: below the diagonal L, on it D */
    for (i = 0; i < n; i++) {
        size_t const j0 = i > bw ? i - bw : 0;
        for (j = j0; j <= i; j++) {
            size_t const k0 = (i > bw ? i - bw : 0) > (j > bw ? j - bw : 0) ? (i > bw ? i - bw : 0) : (j > bw ? j - bw : 0);
            double s = ab[i * (bw + 1) + bw - (i - j)];
            for (k = k0; k < j; k++) s -= ab[i * (bw + 1) + bw - (i - k)] * ab[j * (bw + 1) + bw - (j - k)] * ab[k * (bw + 1) + bw];
            if (j < i) {
                ab[i * (bw + 1) + bw - (i - j)] = s / ab[j * (bw + 1) + bw];
            } else {
                ab[i * (bw + 1) + bw] = s;
            }
        }
    }
}

static void alwan_sx_rw_solve(size_t n, size_t bw, double const *ab, double *xv) {
    size_t i, k;
    for (i = 0; i < n; i++) {
        size_t const k0 = i > bw ? i - bw : 0;
        double s = xv[i];
        for (k = k0; k < i; k++) s -= ab[i * (bw + 1) + bw - (i - k)] * xv[k];
        xv[i] = s;
    }
    for (i = 0; i < n; i++) xv[i] /= ab[i * (bw + 1) + bw];
    for (i = n; i-- > 0;) {
        size_t const k1 = i + bw < n - 1 ? i + bw : n - 1;
        double s = xv[i];
        for (k = i + 1; k <= k1; k++) s -= ab[k * (bw + 1) + bw - (k - i)] * xv[k];
        xv[i] = s;
    }
}

static int alwan_sx_cmp_u32(void const *a, void const *b) {
    uint32_t const x = *(uint32_t const *)a, y = *(uint32_t const *)b;
    return x < y ? -1 : x > y;
}

static alwan_status alwan_sx_random_walker(size_t *lab, size_t *count_out, double const *v, size_t w, size_t h, size_t ch,
                                           alwan_segment_params const *p, int f32) {
    double const beta = p->beta > 0.0 ? p->beta : 130.0;
    double const tol = p->tol > 0.0 ? p->tol : 1e-3;
    size_t const n = w * h, nv = n * ch;
    uint32_t *seeds = NULL, *vals = NULL;
    size_t *uidx = NULL, *upix = NULL, *pos = NULL, *at = NULL;
    double *wr = NULL, *wd = NULL, *diag = NULL, *work = NULL, *X = NULL, *ab = NULL;
    size_t nl = 0, nu = 0, i, x, y, c, l, nvals;
    double mean, var, sf;
    alwan_sx_rw g;
    alwan_status st = ALWAN_E_NOMEM;
    if (!p->markers) return ALWAN_E_INVALID;
    seeds = (uint32_t *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(uint32_t)), sizeof(uint32_t));
    if (!seeds) return ALWAN_E_NOMEM;
    vals = seeds + n;
    for (y = 0; y < h; y++) {
        uint32_t const *row = (uint32_t const *)((char const *)p->markers + y * p->markers_row_stride);
        for (x = 0; x < w; x++) seeds[y * w + x] = row[x];
    }
    /* the seeds' distinct values, renumbered 1, 2, ... in increasing order */
    memcpy(vals, seeds, n * sizeof(uint32_t));
    qsort(vals, n, sizeof(uint32_t), alwan_sx_cmp_u32);
    nvals = 0;
    for (i = 0; i < n; i++)
        if (i == 0 || vals[i] != vals[i - 1]) vals[nvals++] = vals[i];
    if (vals[nvals - 1] == 0) {
        ALWAN_FREE(seeds);
        return ALWAN_E_INVALID;
    }
    if (vals[0] != 0) {
        /* nothing to solve: scikit-image returns the labels as they are */
        for (i = 0; i < n; i++) lab[i] = seeds[i];
        if (count_out) *count_out = vals[nvals - 1];
        if (p->probabilities) {
            if (nvals > p->probabilities_capacity) {
                ALWAN_FREE(seeds);
                return ALWAN_E_RANGE;
            }
            for (l = 0; l < nvals; l++)
                for (i = 0; i < n; i++) p->probabilities[l * n + i] = seeds[i] == vals[l] ? 1.0 : 0.0;
        }
        ALWAN_FREE(seeds);
        return ALWAN_OK;
    }
    nl = nvals - 1;
    if (p->probabilities && nl > p->probabilities_capacity) {
        ALWAN_FREE(seeds);
        return ALWAN_E_RANGE;
    }
    for (i = 0; i < n; i++) {
        size_t lo = 0, hi = nvals;
        while (hi - lo > 1) {
            size_t const mid = (lo + hi) / 2;
            if (vals[mid] <= seeds[i]) lo = mid; else hi = mid;
        }
        seeds[i] = (uint32_t)lo;   /* 0 unlabelled, 1..nl the renumbered seeds */
    }
    uidx = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, 4 * sizeof(size_t)), sizeof(size_t));
    wr = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 3 * sizeof(double)), sizeof(double));
    if (!uidx || !wr) goto done;
    upix = uidx + n;
    pos = upix + n;
    at = pos + n;
    wd = wr + n;
    diag = wd + n;
    for (i = 0; i < n; i++) {
        uidx[i] = 0;
        if (seeds[i] == 0) {
            upix[nu] = i;
            uidx[i] = ++nu;
        }
    }
    /* data.std() over every channel, numpy's mean and sum of squares */
    work = (double *)ALWAN_ALLOC(alwan_safe_array_size(nv > 5 * n ? nv : 5 * n, sizeof(double)), sizeof(double));
    if (!work) goto done;
    mean = alwan_sx_r(alwan_sx_sum(v, nv, f32) / (double)nv, f32);
    for (i = 0; i < nv; i++) {
        double const d = alwan_sx_r(v[i] - mean, f32);
        work[i] = alwan_sx_r(d * d, f32);
    }
    var = alwan_sx_r(alwan_sx_sum(work, nv, f32) / (double)nv, f32);
    sf = -beta / (10.0 * ALWAN_SQRT_F64(var));
    if (ch > 1) sf /= ALWAN_SQRT_F64((double)ch);
    /* the weights: right edges then down edges, gradients summed over the channels */
    for (y = 0; y < h; y++)
        for (x = 0; x + 1 < w; x++) {
            double gsum = 0.0;
            for (c = 0; c < ch; c++) {
                double const d = v[(y * w + x + 1) * ch + c] - v[(y * w + x) * ch + c];
                gsum += d * d;
            }
            wr[y * (w - 1) + x] = ALWAN_EXP_F64(sf * gsum) + 1e-10;
        }
    for (y = 0; y + 1 < h; y++)
        for (x = 0; x < w; x++) {
            double gsum = 0.0;
            for (c = 0; c < ch; c++) {
                double const d = v[((y + 1) * w + x) * ch + c] - v[(y * w + x) * ch + c];
                gsum += d * d;
            }
            wd[y * w + x] = ALWAN_EXP_F64(sf * gsum) + 1e-10;
        }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            double s = 0.0;
            if (x + 1 < w) s += wr[y * (w - 1) + x];
            if (x > 0) s += wr[y * (w - 1) + x - 1];
            if (y + 1 < h) s += wd[y * w + x];
            if (y > 0) s += wd[(y - 1) * w + x];
            diag[y * w + x] = s;
        }
    g.w = w;
    g.h = h;
    g.nu = nu;
    g.uidx = uidx;
    g.wr = wr;
    g.wd = wd;
    g.diag = diag;
    g.upix = upix;
    X = (double *)ALWAN_ALLOC(alwan_safe_array_size(nl ? nl : 1, (nu ? nu : 1) * sizeof(double)), sizeof(double));
    if (!X) goto done;
    if (p->solver == ALWAN_RANDOM_WALKER_DIRECT) {
        /* the unknowns in the shorter side's raster order keep the band at that side */
        int const by_col = h < w;
        size_t const bw = by_col ? h : w;
        size_t k = 0;
        if (by_col) {
            for (x = 0; x < w; x++)
                for (y = 0; y < h; y++)
                    if (uidx[y * w + x]) at[k++] = uidx[y * w + x] - 1;
        } else {
            for (k = 0; k < nu; k++) at[k] = k;
        }
        for (k = 0; k < nu; k++) pos[at[k]] = k;
        ab = (double *)ALWAN_ALLOC(alwan_safe_array_size(nu, (bw + 1) * sizeof(double)), sizeof(double));
        if (!ab) goto done;
        alwan_sx_rw_factor(&g, pos, at, bw, ab);
        for (l = 0; l < nl; l++) {
            double *xl = X + l * nu;
            for (k = 0; k < nu; k++) work[k] = 0.0;
            for (k = 0; k < nu; k++) {
                size_t const pix = upix[k];
                double b = 0.0;
                y = pix / w;
                x = pix % w;
                if (x + 1 < w && seeds[pix + 1] == l + 1) b += wr[y * (w - 1) + x];
                if (x > 0 && seeds[pix - 1] == l + 1) b += wr[y * (w - 1) + x - 1];
                if (y + 1 < h && seeds[pix + w] == l + 1) b += wd[y * w + x];
                if (y > 0 && seeds[pix - w] == l + 1) b += wd[(y - 1) * w + x];
                work[pos[k]] = b;
            }
            alwan_sx_rw_solve(nu, bw, ab, work);
            for (k = 0; k < nu; k++) xl[k] = work[pos[k]];
        }
    } else {
        double *b = work, *r = work + n, *z = work + 2 * n, *pv = work + 3 * n, *q = work + 4 * n;
        size_t k;
        for (l = 0; l < nl; l++) {
            for (k = 0; k < nu; k++) {
                size_t const pix = upix[k];
                double bb = 0.0;
                y = pix / w;
                x = pix % w;
                if (x + 1 < w && seeds[pix + 1] == l + 1) bb += wr[y * (w - 1) + x];
                if (x > 0 && seeds[pix - 1] == l + 1) bb += wr[y * (w - 1) + x - 1];
                if (y + 1 < h && seeds[pix + w] == l + 1) bb += wd[y * w + x];
                if (y > 0 && seeds[pix - w] == l + 1) bb += wd[(y - 1) * w + x];
                b[k] = bb;
            }
            alwan_sx_rw_cg(&g, b, X + l * nu, tol, r, z, pv, q);
        }
    }
    /* labels: a seed keeps its renumbered label, an unknown the most probable (first on a tie) */
    for (i = 0; i < n; i++) {
        if (uidx[i]) {
            size_t const u = uidx[i] - 1;
            size_t best = 0;
            for (l = 1; l < nl; l++)
                if (X[l * nu + u] > X[best * nu + u]) best = l;
            lab[i] = best + 1;
        } else {
            lab[i] = seeds[i];
        }
    }
    if (count_out) *count_out = nl;
    if (p->probabilities)
        for (l = 0; l < nl; l++)
            for (i = 0; i < n; i++)
                p->probabilities[l * n + i] = uidx[i] ? X[l * nu + uidx[i] - 1] : (seeds[i] == l + 1 ? 1.0 : 0.0);
    st = ALWAN_OK;
done:
    ALWAN_FREE(seeds);
    ALWAN_FREE(uidx);
    ALWAN_FREE(wr);
    ALWAN_FREE(work);
    ALWAN_FREE(X);
    ALWAN_FREE(ab);
    return st;
}

/* ---------------------------------------------------------------------------------------
 * dispatch
 * ------------------------------------------------------------------------------------- */

/* v: h x w x ch doubles, the data's values (8-bit as they come); kind 0 f64, 1 f32, 2 u8 */
alwan_status alwan__segment_ext(uint32_t *labels, size_t labels_rs, size_t *count_out, double *v, size_t w, size_t h, size_t ch,
                                alwan_segment_method method, alwan_segment_params const *p, int kind) {
    int const f32 = kind == 1;
    size_t const n = w * h;
    size_t *lab, i, cnt = 0;
    alwan_status st;
    if (method == ALWAN_SEGMENT_FELZENSZWALB && !(p->scale >= 0.0)) return ALWAN_E_RANGE;
    if (method == ALWAN_SEGMENT_QUICKSHIFT && (p->kernel_size != 0.0 && !(p->kernel_size >= 1.0))) return ALWAN_E_RANGE;
    if (method == ALWAN_SEGMENT_QUICKSHIFT && (!(p->ratio >= 0.0) || !(p->max_dist >= 0.0) || !(p->sigma >= 0.0))) return ALWAN_E_RANGE;
    if (method == ALWAN_SEGMENT_CHAN_VESE) {
        if (ch != 1) return ALWAN_E_INVALID;
        if ((unsigned)p->init > (unsigned)ALWAN_CHAN_VESE_INIT_GIVEN) return ALWAN_E_INVALID;
        if (p->init == ALWAN_CHAN_VESE_INIT_GIVEN && (!p->init_level_set_values || p->init_level_set_row_stride / sizeof(double) < w))
            return ALWAN_E_INVALID;
        if (!(p->dt >= 0.0) || p->max_iterations > 100000000) return ALWAN_E_RANGE;
        if (p->level_set && p->level_set_row_stride / sizeof(double) < w) return ALWAN_E_INVALID;
    }
    if (method == ALWAN_SEGMENT_RANDOM_WALKER) {
        if (!p->markers || p->markers_row_stride / sizeof(uint32_t) < w) return ALWAN_E_INVALID;
        if (!(p->beta >= 0.0) || !(p->tol >= 0.0)) return ALWAN_E_RANGE;
        if ((unsigned)p->solver > (unsigned)ALWAN_RANDOM_WALKER_DIRECT) return ALWAN_E_INVALID;
    }
    lab = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(size_t)), sizeof(size_t));
    if (!lab) return ALWAN_E_NOMEM;
    switch (method) {
    case ALWAN_SEGMENT_FELZENSZWALB:
        if (kind == 2)
            for (i = 0; i < n * ch; i++) v[i] = v[i] * (1.0 / 255.0);   /* img_as_float64 */
        st = alwan_sx_felzenszwalb(lab, v, w, h, ch, p);
        break;
    case ALWAN_SEGMENT_QUICKSHIFT:
        if (kind == 2)
            for (i = 0; i < n * ch; i++) v[i] = v[i] * (1.0 / 255.0);   /* img_as_float */
        st = alwan_sx_quickshift(lab, v, w, h, ch, p, f32);
        break;
    case ALWAN_SEGMENT_CHAN_VESE:
        st = alwan_sx_chan_vese(lab, v, w, h, p, f32);   /* 8-bit cast as it comes, then normalised */
        break;
    case ALWAN_SEGMENT_RANDOM_WALKER:
        if (kind == 2)
            for (i = 0; i < n * ch; i++) v[i] = v[i] * (1.0 / 255.0);   /* img_as_float */
        st = alwan_sx_random_walker(lab, &cnt, v, w, h, ch, p, f32);
        break;
    default:
        st = ALWAN_E_INVALID;
        break;
    }
    if (st == ALWAN_OK) {
        alwan_sx_write(labels, labels_rs, lab, w, h, count_out);
        if (method == ALWAN_SEGMENT_RANDOM_WALKER && count_out) *count_out = cnt;
    }
    ALWAN_FREE(lab);
    return st;
}
