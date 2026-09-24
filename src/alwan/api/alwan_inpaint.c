/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_inpaint_{T}: masked pixels filled from their surroundings.
 *
 * ALWAN_INPAINT_BIHARMONIC, scikit-image's restoration.inpaint_biharmonic (Damelin and
 * Hoang 2018): every masked pixel p satisfies the discrete biharmonic equation
 *
 *   sum_q c_pq u_q = 0,   c = the Laplacian applied twice to a unit impulse at p
 *
 * the 13-point stencil (20 at p, -8 at its four neighbours, 2 at the diagonals, 1 two
 * away). Known pixels move to the right-hand side, so the fill continues the surrounding
 * surface and its slope smoothly. Within two pixels of the image's edge the stencil is
 * computed on the window that fits, with scipy.ndimage.laplace's 'reflect' border
 * (half-sample symmetric), as scikit-image computes it. The system is solved directly:
 * the masked pixels are split into the groups the stencil couples (within two rows or
 * columns of each other, |dy| + |dx| <= 2), each ordered row by row and solved by banded
 * Gaussian elimination with partial pivoting, which costs n b^2 for n pixels and a band b
 * about twice the masked pixels in a row, and n b in memory. Last, as scikit-image does,
 * each channel is clipped to the range of its known pixels, which params.unclipped turns
 * off.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The biharmonic coefficients of the impulse at (py, px) on an H x W window, laplace twice
 * with the reflect border: scipy.ndimage.laplace sums correlate1d([1, -2, 1]) over the
 * axes, and 'reflect' maps -1 to 0 and n to n - 1. */
static size_t alwan_ip_refl(long i, long n) {
    while (i < 0 || i >= n) {
        if (i < 0) i = -i - 1;
        if (i >= n) i = 2 * n - 1 - i;
    }
    return (size_t)i;
}

static void alwan_ip_laplace(double *out, double const *in, size_t H, size_t W) {
    size_t y, x;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            double const c = in[y * W + x];
            double s = in[alwan_ip_refl((long)y - 1, (long)H) * W + x] - 2.0 * c + in[alwan_ip_refl((long)y + 1, (long)H) * W + x];
            s += in[y * W + alwan_ip_refl((long)x - 1, (long)W)] - 2.0 * c + in[y * W + alwan_ip_refl((long)x + 1, (long)W)];
            out[y * W + x] = s;
        }
    }
}

static int alwan_ip_cmp_idx(void const *a, void const *b) {
    size_t const x = *(size_t const *)a, y = *(size_t const *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

/* Banded LU with partial pivoting: n x n, lower band kl, upper band ku before fill; row i
 * holds columns i - kl .. i + kl + ku at B[i * wd + (j - i + kl)]. Solves for nrhs columns
 * of rhs (n x nrhs, row-major) in place. Returns 0 on a singular pivot. */
static int alwan_ip_band_solve(double *B, size_t n, size_t kl, size_t ku, double *rhs, size_t nrhs) {
    size_t const ue = kl + ku, wd = 2 * kl + ku + 1;
    size_t k, i, j, c;
#define AB(r, cc) B[(r) * wd + ((cc) + kl - (r))]
    for (k = 0; k < n; k++) {
        size_t const last = k + kl < n - 1 ? k + kl : n - 1;
        size_t const jend = k + ue < n - 1 ? k + ue : n - 1;
        size_t p = k;
        double best = fabs(AB(k, k));
        for (i = k + 1; i <= last; i++) {
            if (fabs(AB(i, k)) > best) {
                best = fabs(AB(i, k));
                p = i;
            }
        }
        if (!(best > 0.0)) return 0;
        if (p != k) {
            for (j = k; j <= jend; j++) {
                double const t = AB(k, j);
                AB(k, j) = AB(p, j);
                AB(p, j) = t;
            }
            for (c = 0; c < nrhs; c++) {
                double const t = rhs[k * nrhs + c];
                rhs[k * nrhs + c] = rhs[p * nrhs + c];
                rhs[p * nrhs + c] = t;
            }
        }
        for (i = k + 1; i <= last; i++) {
            double const l = AB(i, k) / AB(k, k);
            if (l == 0.0) continue;
            AB(i, k) = 0.0;
            for (j = k + 1; j <= jend; j++) AB(i, j) -= l * AB(k, j);
            for (c = 0; c < nrhs; c++) rhs[i * nrhs + c] -= l * rhs[k * nrhs + c];
        }
    }
    for (k = n; k-- > 0;) {
        size_t const jend = k + ue < n - 1 ? k + ue : n - 1;
        for (c = 0; c < nrhs; c++) {
            double s = rhs[k * nrhs + c];
            for (j = k + 1; j <= jend; j++) s -= AB(k, j) * rhs[j * nrhs + c];
            rhs[k * nrhs + c] = s / AB(k, k);
        }
    }
#undef AB
    return 1;
}

static alwan_status alwan_ip_biharmonic(double *img, size_t ch, size_t w, size_t h, unsigned char const *mask,
                                        size_t mask_rs, int unclipped) {
    size_t const n = w * h;
    size_t *list, *idx_of, *stack;
    long *group;
    size_t nu = 0, i, c, g, ngroups = 0;
    double lo[4], hi[4];
    alwan_status st = ALWAN_OK;
    list = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, 3 * sizeof(size_t)), sizeof(size_t));
    group = (long *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(long)), sizeof(long));
    if (!list || !group) {
        if (list) ALWAN_FREE(list);
        if (group) ALWAN_FREE(group);
        return ALWAN_E_NOMEM;
    }
    idx_of = list + n;
    stack = idx_of + n;
    for (c = 0; c < ch; c++) {
        lo[c] = DBL_MAX;
        hi[c] = -DBL_MAX;
    }
    for (i = 0; i < n; i++) {
        size_t const y = i / w, x = i % w;
        group[i] = mask[y * mask_rs + x] ? -1 : -2;   /* -1 masked, not yet grouped; -2 known */
        if (group[i] == -2) {
            for (c = 0; c < ch; c++) {
                double const v = img[c * n + i];
                if (v < lo[c]) lo[c] = v;
                if (v > hi[c]) hi[c] = v;
            }
        }
    }
    /* the groups the stencil couples, by flood fill over |dy| + |dx| <= 2 */
    for (i = 0; i < n && st == ALWAN_OK; i++) {
        size_t top, first = nu, cnt, kl = 0, ku = 0, wd, k;
        double *B, *rhs;
        if (group[i] != -1) continue;
        top = 0;
        stack[top++] = i;
        group[i] = (long)ngroups;
        while (top) {
            size_t const p = stack[--top], py = p / w, px = p % w;
            long dy, dx;
            list[nu++] = p;
            for (dy = -2; dy <= 2; dy++) {
                for (dx = -2; dx <= 2; dx++) {
                    long const qy = (long)py + dy, qx = (long)px + dx;
                    size_t q;
                    if (labs(dy) + labs(dx) > 2 || (dy == 0 && dx == 0)) continue;
                    if (qy < 0 || qx < 0 || qy >= (long)h || qx >= (long)w) continue;
                    q = (size_t)qy * w + (size_t)qx;
                    if (group[q] == -1) {
                        group[q] = (long)ngroups;
                        stack[top++] = q;
                    }
                }
            }
        }
        cnt = nu - first;
        qsort(list + first, cnt, sizeof(size_t), alwan_ip_cmp_idx);   /* row by row */
        for (k = 0; k < cnt; k++) idx_of[list[first + k]] = k;
        /* the bands: the furthest coupled neighbours in this ordering */
        for (k = 0; k < cnt; k++) {
            size_t const p = list[first + k], py = p / w, px = p % w;
            long dy, dx;
            for (dy = -2; dy <= 2; dy++) {
                for (dx = -2; dx <= 2; dx++) {
                    long const qy = (long)py + dy, qx = (long)px + dx;
                    size_t q, kq;
                    if (labs(dy) + labs(dx) > 2) continue;
                    if (qy < 0 || qx < 0 || qy >= (long)h || qx >= (long)w) continue;
                    q = (size_t)qy * w + (size_t)qx;
                    if (group[q] != (long)ngroups) continue;
                    kq = idx_of[q];
                    if (kq < k && k - kq > kl) kl = k - kq;
                    if (kq > k && kq - k > ku) ku = kq - k;
                }
            }
        }
        wd = 2 * kl + ku + 1;
        B = (double *)ALWAN_ALLOC(alwan_safe_array_size(cnt, wd * sizeof(double)), sizeof(double));
        rhs = (double *)ALWAN_ALLOC(alwan_safe_array_size(cnt, ch * sizeof(double)), sizeof(double));
        if (!B || !rhs) {
            if (B) ALWAN_FREE(B);
            if (rhs) ALWAN_FREE(rhs);
            st = ALWAN_E_NOMEM;
            break;
        }
        memset(B, 0, cnt * wd * sizeof(double));
        memset(rhs, 0, cnt * ch * sizeof(double));
        for (k = 0; k < cnt; k++) {
            size_t const p = list[first + k], py = p / w, px = p % w;
            size_t const y0 = py >= 2 ? py - 2 : 0, x0 = px >= 2 ? px - 2 : 0;
            size_t const y1 = py + 2 < h ? py + 2 : h - 1, x1 = px + 2 < w ? px + 2 : w - 1;
            size_t const WH = y1 - y0 + 1, WW = x1 - x0 + 1;
            double d[25], l1[25], coef[25];
            size_t a, b;
            memset(d, 0, sizeof(d));
            d[(py - y0) * WW + (px - x0)] = 1.0;
            alwan_ip_laplace(l1, d, WH, WW);
            alwan_ip_laplace(coef, l1, WH, WW);
            for (a = 0; a < WH; a++) {
                for (b = 0; b < WW; b++) {
                    double const cf = coef[a * WW + b];
                    size_t const q = (y0 + a) * w + (x0 + b);
                    if (cf == 0.0) continue;
                    if (group[q] == (long)ngroups) {
                        size_t const kq = idx_of[q];
                        B[k * wd + (kq + kl - k)] += cf;
                    } else {
                        for (c = 0; c < ch; c++) rhs[k * ch + c] -= cf * img[c * n + q];
                    }
                }
            }
        }
        if (!alwan_ip_band_solve(B, cnt, kl, ku, rhs, ch)) {
            st = ALWAN_E_DIVZERO;
        } else {
            for (k = 0; k < cnt; k++) {
                for (c = 0; c < ch; c++) img[c * n + list[first + k]] = rhs[k * ch + c];
            }
        }
        ALWAN_FREE(B);
        ALWAN_FREE(rhs);
        ngroups++;
    }
    if (st == ALWAN_OK && !unclipped && nu < n) {
        for (g = 0; g < nu; g++) {
            size_t const p = list[g];
            for (c = 0; c < ch; c++) {
                double *v = &img[c * n + p];
                if (*v < lo[c]) *v = lo[c];
                if (*v > hi[c]) *v = hi[c];
            }
        }
    }
    ALWAN_FREE(list);
    ALWAN_FREE(group);
    return st;
}

static alwan_status alwan_ip_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 unsigned char const *mask, size_t mask_rs, alwan_inpaint_method method,
                                 alwan_inpaint_params const *params, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    double *img;
    size_t c, x, y, known = 0;
    alwan_status st;
    if (!out || !src || !mask || w == 0 || h == 0 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w || mask_rs < w) return ALWAN_E_INVALID;
    if (method != ALWAN_INPAINT_BIHARMONIC) return ALWAN_E_INVALID;
    img = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, ch * sizeof(double)), sizeof(double));
    if (!img) return ALWAN_E_NOMEM;
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            int const m = mask[y * mask_rs + x] != 0;
            known += !m;
            for (c = 0; c < ch; c++) {
                double const v = is_f32 ? (double)((alwan_f32 const *)row)[x * ch + c] : ((alwan_f64 const *)row)[x * ch + c];
                if (!m && !(v == v && v <= DBL_MAX && v >= -DBL_MAX)) {   /* masked values are never read */
                    ALWAN_FREE(img);
                    return ALWAN_E_INVALID;
                }
                img[c * n + y * w + x] = m ? 0.0 : v;
            }
        }
    }
    if (known == 0) {
        ALWAN_FREE(img);
        return ALWAN_E_INVALID;   /* nothing to fill from */
    }
    st = alwan_ip_biharmonic(img, ch, w, h, mask, mask_rs, params && params->unclipped);
    if (st == ALWAN_OK) {
        for (y = 0; y < h; y++) {
            char *row = (char *)out + y * out_rs;
            for (x = 0; x < w; x++) {
                for (c = 0; c < ch; c++) {
                    double const v = img[c * n + y * w + x];
                    if (is_f32) ((alwan_f32 *)row)[x * ch + c] = (alwan_f32)v;
                    else ((alwan_f64 *)row)[x * ch + c] = v;
                }
            }
        }
    }
    ALWAN_FREE(img);
    return st;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_inpaint_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                               size_t channels, size_t width, size_t height, unsigned char const *mask,
                               size_t mask_row_stride, alwan_inpaint_method method, alwan_inpaint_params const *params) {
    return alwan_ip_run(out, out_row_stride, src, src_row_stride, channels, width, height, mask, mask_row_stride, method,
                        params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_inpaint_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                               size_t channels, size_t width, size_t height, unsigned char const *mask,
                               size_t mask_row_stride, alwan_inpaint_method method, alwan_inpaint_params const *params) {
    return alwan_ip_run(out, out_row_stride, src, src_row_stride, channels, width, height, mask, mask_row_stride, method,
                        params, 1);
}
#endif
