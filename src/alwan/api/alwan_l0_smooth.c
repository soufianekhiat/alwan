/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * L0 gradient minimisation: Xu, Lu, Xu and Jia, "Image Smoothing via L0 Gradient
 * Minimization", ACM TOG 30(6) (SIGGRAPH Asia 2011). The result S minimises
 *
 *   sum (S - I)^2 + lambda #{p : grad S(p) != 0}
 *
 * the count of pixels where S changes at all, so S is flat in regions and steps at the
 * edges that survive. Solved by half-quadratic splitting: auxiliary gradients (h, v) and a
 * weight beta that starts at 2 lambda and grows by kappa each round until it reaches 1e5.
 *
 *   h, v   = S's forward differences, circular (the last column to the first);
 *            zeroed where h^2 + v^2, summed over the channels, is below lambda / beta
 *   S      = F^-1[(F(I) + beta F(dx^T h + dy^T v)) / (1 + beta (|F(dx)|^2 + |F(dy)|^2))]
 *
 * with dx^T h(x) = h(x - 1) - h(x), circular too, and |F(dx)|^2 = 2 - 2 cos(2 pi k / w). The
 * system is circular, so the 2D DFT diagonalises it and each round is two transforms per
 * channel (api/alwan_fft.c). This follows the authors' MATLAB code, which suite 213 runs as
 * its reference; OpenCV's ximgproc::l0Smooth solves the same circular system but builds
 * the gradients with replicated and reflected borders, which do not match it, and works in
 * float32, where the hard threshold can flip in a whole region. The result is not clamped.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

/* The 2D transform of an h x w complex plane, rows then columns, in place. */
static void alwan_l0_fft2(alwan__fft const *fw, alwan__fft const *fh, double *re, double *im, size_t w, size_t h,
                          double *cre, double *cim, int inverse) {
    size_t x, y;
    for (y = 0; y < h; y++) alwan__fft_run(fw, re + y * w, im + y * w, inverse);
    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            cre[y] = re[y * w + x];
            cim[y] = im[y * w + x];
        }
        alwan__fft_run(fh, cre, cim, inverse);
        for (y = 0; y < h; y++) {
            re[y * w + x] = cre[y];
            im[y * w + x] = cim[y];
        }
    }
}

alwan_status alwan__l0_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                           double lambda, double kappa, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    double const pi = 3.14159265358979323846;
    alwan__fft *fw, *fh;
    double *mem, *S, *F1re, *F1im, *H, *V, *re, *im, *den2, *cre, *cim;
    double beta;
    size_t c, x, y, i;
    if (!out || !src || w == 0 || h == 0 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if (!(lambda > 0.0) || lambda > DBL_MAX || !(kappa > 1.0) || kappa > DBL_MAX) return ALWAN_E_RANGE;
    mem = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, (5 * ch + 3) * sizeof(double)) + 2 * h * sizeof(double),
                                sizeof(double));
    fw = alwan__fft_create(w);
    fh = alwan__fft_create(h);
    if (!mem || !fw || !fh) {
        if (mem) ALWAN_FREE(mem);
        alwan__fft_destroy(fw);
        alwan__fft_destroy(fh);
        return ALWAN_E_NOMEM;
    }
    S = mem;                 /* ch planes */
    F1re = S + ch * n;       /* F(I), ch planes */
    F1im = F1re + ch * n;
    H = F1im + ch * n;       /* h and v of one channel at a time are kept for all channels */
    V = H + ch * n;
    re = V + ch * n;         /* a complex plane */
    im = re + n;
    den2 = im + n;           /* |F(dx)|^2 + |F(dy)|^2 */
    cre = den2 + n;
    cim = cre + h;
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double const v = is_f32 ? (double)((alwan_f32 const *)row)[x * ch + c]
                                        : ((alwan_f64 const *)row)[x * ch + c];
                if (!(v == v) || v > DBL_MAX || v < -DBL_MAX) {
                    ALWAN_FREE(mem);
                    alwan__fft_destroy(fw);
                    alwan__fft_destroy(fh);
                    return ALWAN_E_INVALID;
                }
                S[c * n + y * w + x] = v;
            }
        }
    }
    for (y = 0; y < h; y++) {
        double const dy = 2.0 - 2.0 * ALWAN_COS_F64(2.0 * pi * (double)y / (double)h);
        for (x = 0; x < w; x++) den2[y * w + x] = dy + 2.0 - 2.0 * ALWAN_COS_F64(2.0 * pi * (double)x / (double)w);
    }
    for (c = 0; c < ch; c++) {
        for (i = 0; i < n; i++) {
            F1re[c * n + i] = S[c * n + i];
            F1im[c * n + i] = 0.0;
        }
        alwan_l0_fft2(fw, fh, F1re + c * n, F1im + c * n, w, h, cre, cim, 0);
    }
    for (beta = 2.0 * lambda; beta < 1e5; beta *= kappa) {
        double const thr = lambda / beta;
        /* the gradients, circular, and the L0 step on their summed square */
        for (c = 0; c < ch; c++) {
            double const *s = S + c * n;
            for (y = 0; y < h; y++) {
                size_t const y1 = y + 1 == h ? 0 : y + 1;
                for (x = 0; x < w; x++) {
                    size_t const x1 = x + 1 == w ? 0 : x + 1;
                    H[c * n + y * w + x] = s[y * w + x1] - s[y * w + x];
                    V[c * n + y * w + x] = s[y1 * w + x] - s[y * w + x];
                }
            }
        }
        for (i = 0; i < n; i++) {
            double m = 0.0;
            for (c = 0; c < ch; c++) m += H[c * n + i] * H[c * n + i] + V[c * n + i] * V[c * n + i];
            if (m < thr) {
                for (c = 0; c < ch; c++) H[c * n + i] = V[c * n + i] = 0.0;
            }
        }
        /* the S step per channel */
        for (c = 0; c < ch; c++) {
            double const *hc = H + c * n, *vc = V + c * n;
            for (y = 0; y < h; y++) {
                size_t const y0 = y == 0 ? h - 1 : y - 1;
                for (x = 0; x < w; x++) {
                    size_t const x0 = x == 0 ? w - 1 : x - 1;
                    re[y * w + x] = (hc[y * w + x0] - hc[y * w + x]) + (vc[y0 * w + x] - vc[y * w + x]);
                    im[y * w + x] = 0.0;
                }
            }
            alwan_l0_fft2(fw, fh, re, im, w, h, cre, cim, 0);
            for (i = 0; i < n; i++) {
                double const d = 1.0 + beta * den2[i];
                re[i] = (F1re[c * n + i] + beta * re[i]) / d;
                im[i] = (F1im[c * n + i] + beta * im[i]) / d;
            }
            alwan_l0_fft2(fw, fh, re, im, w, h, cre, cim, 1);
            for (i = 0; i < n; i++) S[c * n + i] = re[i] / (double)n;
        }
    }
    for (y = 0; y < h; y++) {
        char *row = (char *)out + y * out_rs;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double const v = S[c * n + y * w + x];
                if (is_f32) ((alwan_f32 *)row)[x * ch + c] = (alwan_f32)v;
                else ((alwan_f64 *)row)[x * ch + c] = v;
            }
        }
    }
    ALWAN_FREE(mem);
    alwan__fft_destroy(fw);
    alwan__fft_destroy(fh);
    return ALWAN_OK;
}
