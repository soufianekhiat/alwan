/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The guided filter: He, Sun and Tang, "Guided Image Filtering", ECCV 2010 and IEEE TPAMI
 * 35(6), 2013. An edge-preserving smoother whose output is, in every window, a linear
 * function of a guide image: q = a . I + b, with a and b fitted to the input p over the
 * window by least squares with a ridge eps, then averaged over the windows that cover
 * each pixel. With the image as its own guide it smooths within regions and keeps their
 * edges; with a colour guide and one input channel (a matte, a transmission map) it
 * snaps the input to the guide's edges. It runs in time independent of the radius.
 *
 * For a colour guide (Eq. 14 to 16 of the TPAMI paper), per window k:
 *
 *     a_k = (Sigma_k + eps U)^-1 (mean(I p) - mean(I) mean(p))
 *     b_k = mean(p) - a_k . mean(I)
 *     q   = mean(a) . I + mean(b)
 *
 * where Sigma_k is the 3 x 3 covariance of the guide in the window. The reference is
 * OpenCV's cv::ximgproc::guidedFilter (opencv_contrib, Apache-2.0), and this follows it:
 * the window means are box filters of side 2 r + 1 with the border REFLECTED (the edge
 * pixel repeated: ... c b a | a b c ...); the 3 x 3 inverse is by cofactors; and when eps
 * is below 0.01 a determinant under 1e-6 in magnitude is replaced by 1. OpenCV works in
 * float32; this works in double.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <float.h>
#include <string.h>

/* cv::borderInterpolate with BORDER_REFLECT */
static size_t alwan_gf_reflect(long p, size_t n) {
    if (n == 1) return 0;
    while (p < 0 || p >= (long)n) p = p < 0 ? -p - 1 : 2 * (long)n - p - 1;
    return (size_t)p;
}

/* A normalised box filter of side 2 r + 1, reflected border: rows then columns, as
 * running sums. tmp holds w * h doubles. */
static void alwan_gf_box(double *dst, double const *src, size_t w, size_t h, size_t r, double *tmp) {
    double const norm = 1.0 / ((double)(2 * r + 1) * (double)(2 * r + 1));
    long const rr = (long)r;
    size_t x, y;
    for (y = 0; y < h; y++) {
        double const *row = src + y * w;
        double s = 0.0;
        long k;
        for (k = -rr; k <= rr; k++) s += row[alwan_gf_reflect(k, w)];
        for (x = 0; x < w; x++) {
            tmp[y * w + x] = s;
            s += row[alwan_gf_reflect((long)x + rr + 1, w)] - row[alwan_gf_reflect((long)x - rr, w)];
        }
    }
    for (x = 0; x < w; x++) {
        double s = 0.0;
        long k;
        for (k = -rr; k <= rr; k++) s += tmp[alwan_gf_reflect(k, h) * w + x];
        for (y = 0; y < h; y++) {
            dst[y * w + x] = s * norm;
            s += tmp[alwan_gf_reflect((long)y + rr + 1, h) * w + x] - tmp[alwan_gf_reflect((long)y - rr, h) * w + x];
        }
    }
}

static int alwan_gf_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

/* Symmetric 3 x 3 storage, OpenCV's SymArray2D: (i, j) with i >= j at i (i + 1) / 2 + j. */
static size_t alwan_gf_sym(int i, int j) {
    if (i < j) { int const t = i; i = j; j = t; }
    return (size_t)(i * (i + 1) / 2 + j);
}

alwan_status alwan__gf_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride,
                           size_t src_channels, void const *guide, size_t guide_row_stride,
                           size_t guide_channels, size_t w, size_t h, size_t radius, double eps,
                           int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    int const gc = (int)guide_channels;
    size_t const ncov = guide_channels == 3 ? 6 : 1;
    /* planes: guide (gc), guide means (gc), inverse covariance (ncov), p, mean p,
     * cov(p, I) (gc), a (gc), b, scratch (2) */
    size_t const planes = (size_t)gc * 4 + ncov + 5;
    double *pool, *I, *mI, *inv, *p, *mp, *cpI, *a, *b, *t0, *t1;
    size_t i, x, y, s;
    int c, k, l;
    if (!out || !src || !guide) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || src_channels == 0 || src_channels > 4) return ALWAN_E_INVALID;
    if (guide_channels != 1 && guide_channels != 3) return ALWAN_E_INVALID;
    if (!alwan_gf_finite(eps) || eps < 0.0) return ALWAN_E_INVALID;
    if (src_row_stride / elem / src_channels < w || guide_row_stride / elem / guide_channels < w
        || out_row_stride / elem / src_channels < w) return ALWAN_E_INVALID;
    if (n / w != h || radius > 1u << 20) return ALWAN_E_RANGE;
    if (alwan_safe_array_size(n, planes * sizeof(double)) == 0) return ALWAN_E_NOMEM;
    pool = (double *)ALWAN_ALLOC(n * planes * sizeof(double), sizeof(double));
    if (!pool) return ALWAN_E_NOMEM;
    I = pool; mI = I + gc * n; inv = mI + gc * n; p = inv + ncov * n; mp = p + n;
    cpI = mp + n; a = cpI + gc * n; b = a + gc * n; t0 = b + n; t1 = t0 + n;

    for (y = 0; y < h; y++) {
        char const *row = (char const *)guide + y * guide_row_stride;
        for (x = 0; x < w; x++) {
            for (c = 0; c < gc; c++) {
                double const v = is_f32 ? (double)((alwan_f32 const *)row)[x * guide_channels + (size_t)c]
                                  : (double)((alwan_f64 const *)row)[x * guide_channels + (size_t)c];
                if (!alwan_gf_finite(v)) { ALWAN_FREE(pool); return ALWAN_E_INVALID; }
                I[(size_t)c * n + y * w + x] = v;
            }
        }
    }
    for (c = 0; c < gc; c++) alwan_gf_box(mI + (size_t)c * n, I + (size_t)c * n, w, h, radius, t0);

    /* the guide's covariance plus eps on the diagonal, then its inverse */
    if (gc == 1) {
        for (i = 0; i < n; i++) t1[i] = I[i] * I[i];
        alwan_gf_box(inv, t1, w, h, radius, t0);
        for (i = 0; i < n; i++) inv[i] = 1.0 / (inv[i] - mI[i] * mI[i] + eps);
    } else {
        double *covp[6];   /* six planes of scratch: a (3) and cpI (3) are free until later */
        covp[0] = a; covp[1] = a + n; covp[2] = a + 2 * n; covp[3] = cpI; covp[4] = cpI + n; covp[5] = cpI + 2 * n;
        for (k = 0; k < 3; k++) {
            for (l = 0; l <= k; l++) {
                double *d = covp[alwan_gf_sym(k, l)];
                for (i = 0; i < n; i++) t1[i] = I[(size_t)k * n + i] * I[(size_t)l * n + i];
                alwan_gf_box(d, t1, w, h, radius, t0);
                for (i = 0; i < n; i++) d[i] -= mI[(size_t)k * n + i] * mI[(size_t)l * n + i] + (k == l ? -eps : 0.0);
            }
        }
        for (i = 0; i < n; i++) {
            double cv[3][3], adj[6], det = 0.0;
            for (k = 0; k < 3; k++) for (l = 0; l < 3; l++) cv[k][l] = covp[alwan_gf_sym(k, l)][i];
            for (k = 0; k < 3; k++) {
                for (l = 0; l <= k; l++) {
                    adj[alwan_gf_sym(k, l)] = cv[(k + 1) % 3][(l + 1) % 3] * cv[(k + 2) % 3][(l + 2) % 3]
                                      - cv[(k + 1) % 3][(l + 2) % 3] * cv[(k + 2) % 3][(l + 1) % 3];
                }
            }
            for (k = 0; k < 3; k++) det += cv[k][0] * adj[alwan_gf_sym(k, 0)];
            if (eps < 1e-2 && fabs(det) < 1e-6) det = 1.0;
            for (k = 0; k < 6; k++) inv[(size_t)k * n + i] = adj[k] / det;
        }
    }

    for (s = 0; s < src_channels; s++) {
        for (y = 0; y < h; y++) {
            char const *row = (char const *)src + y * src_row_stride;
            for (x = 0; x < w; x++) {
                double const v = is_f32 ? (double)((alwan_f32 const *)row)[x * src_channels + s]
                                  : (double)((alwan_f64 const *)row)[x * src_channels + s];
                if (!alwan_gf_finite(v)) { ALWAN_FREE(pool); return ALWAN_E_INVALID; }
                p[y * w + x] = v;
            }
        }
        alwan_gf_box(mp, p, w, h, radius, t0);
        for (c = 0; c < gc; c++) {
            double *d = cpI + (size_t)c * n;
            for (i = 0; i < n; i++) t1[i] = p[i] * I[(size_t)c * n + i];
            alwan_gf_box(d, t1, w, h, radius, t0);
            for (i = 0; i < n; i++) d[i] -= mp[i] * mI[(size_t)c * n + i];
        }
        for (c = 0; c < gc; c++) {
            double *d = a + (size_t)c * n;
            for (i = 0; i < n; i++) {
                double acc = 0.0;
                for (k = 0; k < gc; k++) acc += inv[alwan_gf_sym(c, k) * n + i] * cpI[(size_t)k * n + i];
                d[i] = acc;
            }
        }
        for (i = 0; i < n; i++) {
            double v = mp[i];
            for (c = 0; c < gc; c++) v -= a[(size_t)c * n + i] * mI[(size_t)c * n + i];
            b[i] = v;
        }
        alwan_gf_box(b, b, w, h, radius, t0);
        for (c = 0; c < gc; c++) alwan_gf_box(a + (size_t)c * n, a + (size_t)c * n, w, h, radius, t0);
        for (y = 0; y < h; y++) {
            char *row = (char *)out + y * out_row_stride;
            for (x = 0; x < w; x++) {
                double v = b[y * w + x];
                for (c = 0; c < gc; c++) v += a[(size_t)c * n + y * w + x] * I[(size_t)c * n + y * w + x];
                if (is_f32) ((alwan_f32 *)row)[x * src_channels + s] = (alwan_f32)v;
                else ((alwan_f64 *)row)[x * src_channels + s] = (alwan_f64)v;
            }
        }
    }
    ALWAN_FREE(pool);
    return ALWAN_OK;
}
