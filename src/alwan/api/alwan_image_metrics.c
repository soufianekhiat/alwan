/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Image comparison metrics as scikit-image's skimage.metrics computes them:
 * mean_squared_error, normalized_root_mse, normalized_mutual_information, and
 * structural_similarity with every channel and either window; and, at the end, the
 * alwan_image_compare family (MAE, RMSE, AE, NCC, DSSIM) written from the definitions.
 *
 * The sums follow numpy's orders, since that is where two correct implementations
 * part in the last bits: a mean over a contiguous array is numpy's pairwise sum (blocks
 * of eight, then halves), a float32 array averaged in float64 is reduced in buffers of
 * 8192 values, and a reduction down the rows of a 2-D array adds the rows in turn.
 *
 * SSIM's windows: GAUSSIAN is scipy's gaussian_filter (a private copy of suwar_filter's path, which is
 * bit-exact to it) with truncate 3.5 and reflected borders; UNIFORM is scipy's
 * uniform_filter, a running sum along each axis, transcribed below. An 8-bit image is
 * compared on its raw values, as scikit-image casts it without rescaling; the caller
 * gives the data range (255 for 8-bit).
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

#define ALWAN__IM_BUFSIZE 8192

/* numpy's pairwise summation of a contiguous run. */
static double alwan__im_pairwise(double const *a, size_t n) {
    if (n < 8) {
        double r = 0.0;
        size_t i;
        for (i = 0; i < n; i++) r += a[i];
        return r;
    }
    if (n <= 128) {
        double r[8], res;
        size_t i, j;
        for (j = 0; j < 8; j++) r[j] = a[j];
        for (i = 8; i < n - (n % 8); i += 8)
            for (j = 0; j < 8; j++) r[j] += a[i + j];
        res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; i++) res += a[i];
        return res;
    }
    {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan__im_pairwise(a, n2) + alwan__im_pairwise(a + n2, n - n2);
    }
}

static float alwan__im_pairwise_f(float const *a, size_t n) {
    if (n < 8) {
        float r = 0.0f;
        size_t i;
        for (i = 0; i < n; i++) r += a[i];
        return r;
    }
    if (n <= 128) {
        float r[8], res;
        size_t i, j;
        for (j = 0; j < 8; j++) r[j] = a[j];
        for (i = 8; i < n - (n % 8); i += 8)
            for (j = 0; j < 8; j++) r[j] += a[i + j];
        res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; i++) res += a[i];
        return res;
    }
    {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan__im_pairwise_f(a, n2) + alwan__im_pairwise_f(a + n2, n - n2);
    }
}

/* A float64 reduction of values that numpy casts first: buffer by buffer. */
static double alwan__im_sum_chunked(double const *a, size_t n) {
    double r = 0.0;
    size_t k;
    for (k = 0; k < n; k += ALWAN__IM_BUFSIZE) {
        size_t const m = n - k < ALWAN__IM_BUFSIZE ? n - k : ALWAN__IM_BUFSIZE;
        r += alwan__im_pairwise(a + k, m);
    }
    return r;
}

typedef enum { ALWAN__IM_F64 = 0, ALWAN__IM_F32 = 1, ALWAN__IM_U8 = 2 } alwan__im_type;

static int alwan__im_dims_ok(size_t w, size_t h, size_t ch) {
    return w > 0 && h > 0 && ch >= 1 && ch <= 4 && w <= ((size_t)1 << 28) / h && w * h <= ((size_t)1 << 28) / ch;
}

/* Value k of row y, channels interleaved, as a double. */
static double alwan__im_at(void const *img, size_t rs, alwan__im_type t, size_t y, size_t k) {
    char const *row = (char const *)img + y * rs;
    if (t == ALWAN__IM_F64) return ((double const *)row)[k];
    if (t == ALWAN__IM_F32) return (double)((float const *)row)[k];
    return (double)((unsigned char const *)row)[k];
}

/* mean((a - b)^2): float64 pairwise for f64 and u8 (cast to float64 first, raw
 * values); for f32 the square in float32, averaged in float64 buffer by buffer. */
static alwan_status alwan__im_mse(double *out, void const *a, size_t ars, void const *b, size_t brs, size_t w, size_t h,
                                  size_t ch, alwan__im_type t) {
    size_t const row = w * ch, n = row * h;
    double *d;
    size_t x, y;
    d = (double *)ALWAN_ALLOC(n * sizeof(double), sizeof(double));
    if (!d) return ALWAN_E_NOMEM;
    for (y = 0; y < h; y++)
        for (x = 0; x < row; x++) {
            if (t == ALWAN__IM_F32) {
                float const e = (float)alwan__im_at(a, ars, t, y, x) - (float)alwan__im_at(b, brs, t, y, x);
                d[y * row + x] = (double)(e * e);
            } else {
                double const e = alwan__im_at(a, ars, t, y, x) - alwan__im_at(b, brs, t, y, x);
                d[y * row + x] = e * e;
            }
        }
    *out = (t == ALWAN__IM_F32 ? alwan__im_sum_chunked(d, n) : 0.0 + alwan__im_pairwise(d, n)) / (double)n;
    ALWAN_FREE(d);
    return ALWAN_OK;
}

#define ALWAN__IM_ENTRY_MSE(SFX, T, TT)                                                                         \
    alwan_status alwan_mean_squared_error_##SFX(double *mse, T const *a, size_t a_row_stride, T const *b,        \
                                                size_t b_row_stride, size_t width, size_t height,                \
                                                size_t channels) {                                               \
        if (!mse || !a || !b || !alwan__im_dims_ok(width, height, channels)) return ALWAN_E_INVALID;             \
        return alwan__im_mse(mse, a, a_row_stride, b, b_row_stride, width, height, channels, TT);                \
    }

#if ALWAN_WITH_F64
ALWAN__IM_ENTRY_MSE(f64, alwan_f64, ALWAN__IM_F64)
#endif
#if ALWAN_WITH_F32
ALWAN__IM_ENTRY_MSE(f32, alwan_f32, ALWAN__IM_F32)
#endif
ALWAN__IM_ENTRY_MSE(u8, unsigned char, ALWAN__IM_U8)

static alwan_status alwan__im_nrmse(double *out, void const *a, size_t ars, void const *b, size_t brs, size_t w,
                                    size_t h, size_t ch, alwan_nrmse_normalization norm, alwan__im_type t) {
    size_t const row = w * ch, n = row * h;
    double mse = 0.0, denom = 0.0;
    size_t x, y;
    alwan_status st;
    if ((int)norm < (int)ALWAN_NRMSE_EUCLIDEAN || (int)norm > (int)ALWAN_NRMSE_MEAN) return ALWAN_E_INVALID;
    st = alwan__im_mse(&mse, a, ars, b, brs, w, h, ch, t);
    if (st != ALWAN_OK) return st;
    if (norm == ALWAN_NRMSE_MIN_MAX) {
        double lo = alwan__im_at(a, ars, t, 0, 0), hi = lo;
        for (y = 0; y < h; y++)
            for (x = 0; x < row; x++) {
                double const v = alwan__im_at(a, ars, t, y, x);
                if (v < lo) lo = v;
                if (v > hi) hi = v;
            }
        denom = t == ALWAN__IM_F32 ? (double)((float)hi - (float)lo) : hi - lo;
    } else {
        double *d = (double *)ALWAN_ALLOC(n * sizeof(double), sizeof(double));
        if (!d) return ALWAN_E_NOMEM;
        if (t == ALWAN__IM_F32 && norm == ALWAN_NRMSE_MEAN) {
            float *f = (float *)d;
            for (y = 0; y < h; y++)
                for (x = 0; x < row; x++) f[y * row + x] = (float)alwan__im_at(a, ars, t, y, x);
            denom = (double)((0.0f + alwan__im_pairwise_f(f, n)) / (float)n);
        } else {
            for (y = 0; y < h; y++)
                for (x = 0; x < row; x++) {
                    double const v = alwan__im_at(a, ars, t, y, x);
                    if (norm == ALWAN_NRMSE_EUCLIDEAN)
                        d[y * row + x] = t == ALWAN__IM_F32 ? (double)((float)v * (float)v) : v * v;
                    else d[y * row + x] = v;
                }
            if (norm == ALWAN_NRMSE_EUCLIDEAN) {
                double const s = t == ALWAN__IM_F32 ? alwan__im_sum_chunked(d, n) : 0.0 + alwan__im_pairwise(d, n);
                denom = ALWAN_SQRT_F64(s / (double)n);
            } else {
                denom = (0.0 + alwan__im_pairwise(d, n)) / (double)n;
            }
        }
        ALWAN_FREE(d);
    }
    if (!(denom != 0.0)) return ALWAN_E_RANGE;
    *out = ALWAN_SQRT_F64(mse) / denom;
    return ALWAN_OK;
}

#define ALWAN__IM_ENTRY_NRMSE(SFX, T, TT)                                                                        \
    alwan_status alwan_normalized_root_mse_##SFX(double *nrmse, T const *image_true, size_t true_row_stride,      \
                                                 T const *image_test, size_t test_row_stride, size_t width,       \
                                                 size_t height, size_t channels,                                  \
                                                 alwan_nrmse_normalization normalization) {                       \
        if (!nrmse || !image_true || !image_test || !alwan__im_dims_ok(width, height, channels))                  \
            return ALWAN_E_INVALID;                                                                               \
        return alwan__im_nrmse(nrmse, image_true, true_row_stride, image_test, test_row_stride, width, height,    \
                               channels, normalization, TT);                                                      \
    }

#if ALWAN_WITH_F64
ALWAN__IM_ENTRY_NRMSE(f64, alwan_f64, ALWAN__IM_F64)
#endif
#if ALWAN_WITH_F32
ALWAN__IM_ENTRY_NRMSE(f32, alwan_f32, ALWAN__IM_F32)
#endif
ALWAN__IM_ENTRY_NRMSE(u8, unsigned char, ALWAN__IM_U8)

/* ---------------------------------------------------------------- *
 * Normalised mutual information
 * ---------------------------------------------------------------- */

/* numpy's linspace(lo, hi, bins + 1) in the edges' precision, and its outer-edge
 * widening of an empty range by 0.5 each side. */
static void alwan__im_edges(double *e, size_t bins, double lo, double hi, int f32) {
    size_t const num = bins + 1;
    double const div = (double)(num - 1);
    size_t i;
    if (lo == hi) {
        lo = f32 ? (double)((float)lo - 0.5f) : lo - 0.5;
        hi = f32 ? (double)((float)hi + 0.5f) : hi + 0.5;
    }
    if (f32) {
        float const flo = (float)lo, fhi = (float)hi;
        float const delta = fhi - flo;
        float const step = delta / (float)div;
        for (i = 0; i < num; i++) {
            float y = (float)i;
            y = step == 0.0f ? (y / (float)div) * delta : y * step;
            e[i] = (double)(y + flo);
        }
        e[num - 1] = (double)fhi;
    } else {
        double const delta = hi - lo, step = delta / div;
        for (i = 0; i < num; i++) {
            double y = (double)i;
            y = step == 0.0 ? (y / div) * delta : y * step;
            e[i] = y + lo;
        }
        e[num - 1] = hi;
    }
}

/* searchsorted(edges, v, side='right'), then the value on the last edge moved into the
 * last bin; 0 below the first edge and bins + 1 above the last, as histogramdd counts
 * outliers before cutting them. */
static size_t alwan__im_bin(double const *e, size_t bins, double v) {
    size_t lo = 0, hi = bins + 1;
    while (lo < hi) {
        size_t const mid = lo + (hi - lo) / 2;
        if (e[mid] <= v) lo = mid + 1;
        else hi = mid;
    }
    if (v == e[bins]) lo--;
    return lo;
}

/* scipy.stats.entropy of n values: normalised by their sum, -sum p ln p. */
static double alwan__im_entropy(double const *pk, size_t n, double *tmp) {
    double const s = 0.0 + alwan__im_pairwise(pk, n);
    size_t i;
    for (i = 0; i < n; i++) {
        double const p = 1.0 * pk[i] / s;
        tmp[i] = p > 0.0 ? -p * ALWAN_LN_F64(p) : 0.0;
    }
    return 0.0 + alwan__im_pairwise(tmp, n);
}

static alwan_status alwan__im_nmi(double *out, void const *a, size_t ars, void const *b, size_t brs, size_t w,
                                  size_t h, size_t ch, size_t bins, alwan__im_type t) {
    size_t const row = w * ch, n = row * h;
    double *e0, *e1, *hist, *col, *rowsum, *tmp, *de0, *de1;
    double lo0, hi0, lo1, hi1, s, h0, h1, h01;
    size_t x, y, i, j;
    if (bins == 0) bins = 100;
    if (bins > 4096) return ALWAN_E_RANGE;
    e0 = (double *)ALWAN_ALLOC(((bins + 1) * 4 + bins * bins + bins * 3) * sizeof(double) + bins * bins * sizeof(double),
                               sizeof(double));
    if (!e0) return ALWAN_E_NOMEM;
    e1 = e0 + bins + 1;
    de0 = e1 + bins + 1;
    de1 = de0 + bins + 1;
    hist = de1 + bins + 1;
    col = hist + bins * bins;
    rowsum = col + bins;
    tmp = rowsum + bins;
    lo0 = hi0 = alwan__im_at(a, ars, t, 0, 0);
    lo1 = hi1 = alwan__im_at(b, brs, t, 0, 0);
    for (y = 0; y < h; y++)
        for (x = 0; x < row; x++) {
            double const u = alwan__im_at(a, ars, t, y, x), v = alwan__im_at(b, brs, t, y, x);
            if (!(u == u) || !(v == v)) {
                ALWAN_FREE(e0);
                return ALWAN_E_RANGE;
            }
            if (u < lo0) lo0 = u;
            if (u > hi0) hi0 = u;
            if (v < lo1) lo1 = v;
            if (v > hi1) hi1 = v;
        }
    alwan__im_edges(e0, bins, lo0, hi0, t == ALWAN__IM_F32);
    alwan__im_edges(e1, bins, lo1, hi1, t == ALWAN__IM_F32);
    for (i = 0; i < bins; i++) {
        de0[i] = t == ALWAN__IM_F32 ? (double)((float)e0[i + 1] - (float)e0[i]) : e0[i + 1] - e0[i];
        de1[i] = t == ALWAN__IM_F32 ? (double)((float)e1[i + 1] - (float)e1[i]) : e1[i + 1] - e1[i];
    }
    memset(hist, 0, bins * bins * sizeof(double));
    for (y = 0; y < h; y++)
        for (x = 0; x < row; x++) {
            size_t const b0 = alwan__im_bin(e0, bins, alwan__im_at(a, ars, t, y, x));
            size_t const b1 = alwan__im_bin(e1, bins, alwan__im_at(b, brs, t, y, x));
            if (b0 >= 1 && b0 <= bins && b1 >= 1 && b1 <= bins) hist[(b0 - 1) * bins + (b1 - 1)] += 1.0;
        }
    /* density: the counts over their total and each bin's widths, in numpy's order */
    s = 0.0 + alwan__im_pairwise(hist, bins * bins);
    for (i = 0; i < bins; i++)
        for (j = 0; j < bins; j++) {
            double v = hist[i * bins + j] / de0[i];
            v = v / de1[j];
            hist[i * bins + j] = v / s;
        }
    /* H0 from the sums down the columns (row by row), H1 from the sums along the rows */
    for (j = 0; j < bins; j++) col[j] = hist[j];
    for (i = 1; i < bins; i++)
        for (j = 0; j < bins; j++) col[j] += hist[i * bins + j];
    for (i = 0; i < bins; i++) rowsum[i] = 0.0 + alwan__im_pairwise(hist + i * bins, bins);
    h0 = alwan__im_entropy(col, bins, tmp);
    h1 = alwan__im_entropy(rowsum, bins, tmp);
    {
        double *flat = (double *)ALWAN_ALLOC(bins * bins * sizeof(double), sizeof(double));
        if (!flat) {
            ALWAN_FREE(e0);
            return ALWAN_E_NOMEM;
        }
        h01 = alwan__im_entropy(hist, bins * bins, flat);
        ALWAN_FREE(flat);
    }
    (void)n;
    ALWAN_FREE(e0);
    *out = (h0 + h1) / h01;
    return ALWAN_OK;
}

#define ALWAN__IM_ENTRY_NMI(SFX, T, TT)                                                                         \
    alwan_status alwan_normalized_mutual_information_##SFX(double *nmi, T const *a, size_t a_row_stride,         \
                                                           T const *b, size_t b_row_stride, size_t width,        \
                                                           size_t height, size_t channels, size_t bins) {        \
        if (!nmi || !a || !b || !alwan__im_dims_ok(width, height, channels)) return ALWAN_E_INVALID;             \
        return alwan__im_nmi(nmi, a, a_row_stride, b, b_row_stride, width, height, channels, bins, TT);          \
    }

#if ALWAN_WITH_F64
ALWAN__IM_ENTRY_NMI(f64, alwan_f64, ALWAN__IM_F64)
#endif
#if ALWAN_WITH_F32
ALWAN__IM_ENTRY_NMI(f32, alwan_f32, ALWAN__IM_F32)
#endif
ALWAN__IM_ENTRY_NMI(u8, unsigned char, ALWAN__IM_U8)

/* ---------------------------------------------------------------- *
 * SSIM, any number of channels, either window
 * ---------------------------------------------------------------- */

/* scipy.ndimage 'reflect': d c b a | a b c d | d c b a */
static size_t alwan__im_fold(ptrdiff_t i, size_t n) {
    ptrdiff_t const period = (ptrdiff_t)(2u * n);
    i %= period;
    if (i < 0) i += period;
    return i < (ptrdiff_t)n ? (size_t)i : (size_t)(period - 1 - i);
}

/* scipy's uniform_filter1d along rows (axis 1) or columns (axis 0): a running sum of the
 * window, entering minus leaving, each output that sum over size (ni_filters.c). */
static void alwan__im_uniform1d(double *dst, double const *src, size_t w, size_t h, size_t size, int along_rows,
                                double *line) {
    size_t const len = along_rows ? w : h, lines = along_rows ? h : w;
    size_t const size1 = size / 2, size2 = size - size1 - 1;
    size_t k, i;
    for (k = 0; k < lines; k++) {
        double tmp = 0.0;
        double const *l1, *l2;
        for (i = 0; i < len + size1 + size2; i++) {
            size_t const s = alwan__im_fold((ptrdiff_t)i - (ptrdiff_t)size1, len);
            line[i] = along_rows ? src[k * w + s] : src[s * w + k];
        }
        for (i = 0; i < size; i++) tmp += line[i];
        if (along_rows) dst[k * w] = tmp / (double)size;
        else dst[k] = tmp / (double)size;
        l1 = line;
        l2 = line + size;
        for (i = 1; i < len; i++) {
            tmp += *l2++ - *l1++;
            if (along_rows) dst[k * w + i] = tmp / (double)size;
            else dst[i * w + k] = tmp / (double)size;
        }
    }
}

/* scipy's gaussian_filter for SSIM's window: one f64 plane, the same sigma on both axes,
 * truncate 3.5, reflected borders. A private copy of the path suwar_filter takes for these
 * settings (alwan does not call suwar), with the same arithmetic: the kernel normalised by
 * numpy's pairwise sum, scipy's symmetric correlate1d down the columns, then along the rows. */
static long alwan__im_reflect(long j, long len) {
    long const p = 2 * len;
    long m;
    if (j >= 0 && j < len) return j;
    m = j % p;
    if (m < 0) m += p;
    return m < len ? m : p - 1 - m;
}

static void alwan__im_corr(double *out, double const *in, size_t count, size_t len, size_t lstep, size_t istep,
                           double const *wts, size_t half, double *ext) {
    double const *fw = wts + half;
    size_t l, i, ii;
    long j;
    int sym = 1;
    for (ii = 1; ii <= half; ii++)
        if (ALWAN_ABS_F64(fw[ii] - fw[-(long)ii]) > DBL_EPSILON) {
            sym = 0;
            break;
        }
    for (l = 0; l < count; l++) {
        for (j = -(long)half; j < (long)(len + half); j++)
            ext[j + (long)half] = in[l * lstep + (size_t)alwan__im_reflect(j, (long)len) * istep];
        for (i = 0; i < len; i++) {
            double const *c = ext + half + i;
            double acc;
            if (sym) {
                acc = c[0] * fw[0];
                for (j = -(long)half; j < 0; j++) acc += (c[j] + c[-j]) * fw[j];
            } else {
                acc = c[half] * fw[half];
                for (j = -(long)half; j < (long)half; j++) acc += c[j] * fw[j];
            }
            out[l * lstep + i * istep] = acc;
        }
    }
}

static alwan_status alwan__im_gauss(double *dst, double const *src, size_t w, size_t h, double sigma) {
    size_t const n = w * h, lw = (size_t)(3.5 * sigma + 0.5), kn = 2 * lw + 1;
    size_t const extn = (w > h ? w : h) + 2 * lw + 2;
    double const a = -0.5 / (sigma * sigma);
    double *buf, *tmp, *k, *phi, *ext, s;
    size_t i;
    if (!(sigma > 0.0) || 3.5 * sigma + 0.5 >= (double)((size_t)1 << 20)) return ALWAN_E_RANGE;
    for (i = 0; i < n; i++)
        if (!(src[i] - src[i] == 0.0)) return ALWAN_E_INVALID;
    if (sigma <= 1e-15) {
        memcpy(dst, src, n * sizeof(double));
        return ALWAN_OK;
    }
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(n + 2 * kn + extn, sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    tmp = buf, k = tmp + n, phi = k + kn, ext = phi + kn;
    for (i = 0; i < kn; i++) {
        long const x = (long)i - (long)lw;
        phi[i] = ALWAN_EXP_F64(a * (double)(x * x));
    }
    s = alwan__im_pairwise(phi, kn);
    for (i = 0; i < kn; i++) phi[i] = phi[i] / s;
    for (i = 0; i < kn; i++) k[kn - 1 - i] = phi[i];
    alwan__im_corr(tmp, src, w, h, 1, w, k, lw, ext);
    alwan__im_corr(dst, tmp, h, w, w, 1, k, lw, ext);
    ALWAN_FREE(buf);
    return ALWAN_OK;
}

static alwan_status alwan__im_window(double *dst, double const *src, size_t w, size_t h, int uniform, size_t win,
                                     double sigma, double *tmp, double *line) {
    if (uniform) {
        alwan__im_uniform1d(tmp, src, w, h, win, 0, line);
        alwan__im_uniform1d(dst, tmp, w, h, win, 1, line);
        return ALWAN_OK;
    }
    return alwan__im_gauss(dst, src, w, h, sigma);
}

/* One channel's mean SSIM, the map cropped by (win - 1) / 2 and averaged row by row. */
static alwan_status alwan__im_ssim_plane(double *out, double const *a, double const *b, size_t w, size_t h, int uniform,
                                         size_t win, double sigma, double cov_norm, double c1, double c2, double *buf) {
    size_t const n = w * h, pad = (win - 1) / 2;
    double *prod = buf, *tmp = prod + n, *ux = tmp + n, *uy = ux + n, *uxx = uy + n, *uyy = uxx + n, *uxy = uyy + n,
           *srow = uxy + n, *line = srow + w;
    double total = 0.0;
    size_t x, y, p, m = 0;
    alwan_status st;
    (void)srow;
    if ((st = alwan__im_window(ux, a, w, h, uniform, win, sigma, tmp, line)) != ALWAN_OK) return st;
    if ((st = alwan__im_window(uy, b, w, h, uniform, win, sigma, tmp, line)) != ALWAN_OK) return st;
    for (p = 0; p < n; p++) prod[p] = a[p] * a[p];
    if ((st = alwan__im_window(uxx, prod, w, h, uniform, win, sigma, tmp, line)) != ALWAN_OK) return st;
    for (p = 0; p < n; p++) prod[p] = b[p] * b[p];
    if ((st = alwan__im_window(uyy, prod, w, h, uniform, win, sigma, tmp, line)) != ALWAN_OK) return st;
    for (p = 0; p < n; p++) prod[p] = a[p] * b[p];
    if ((st = alwan__im_window(uxy, prod, w, h, uniform, win, sigma, tmp, line)) != ALWAN_OK) return st;
    for (y = pad; y < h - pad; y++) {
        for (x = pad; x < w - pad; x++) {
            size_t const q = y * w + x;
            double const vx = cov_norm * (uxx[q] - ux[q] * ux[q]);
            double const vy = cov_norm * (uyy[q] - uy[q] * uy[q]);
            double const vxy = cov_norm * (uxy[q] - ux[q] * uy[q]);
            double const a1 = 2.0 * ux[q] * uy[q] + c1, a2 = 2.0 * vxy + c2;
            double const b1 = ux[q] * ux[q] + uy[q] * uy[q] + c1, b2 = vx + vy + c2;
            prod[m++] = (a1 * a2) / (b1 * b2);
        }
    }
    /* numpy reduces the cropped view through a contiguous buffer of 8192 values at a time */
    total = alwan__im_sum_chunked(prod, m);
    *out = total / ((double)(w - 2 * pad) * (double)(h - 2 * pad));
    return ALWAN_OK;
}

static alwan_status alwan__im_ssim(double *out, double *per_out, void const *a, size_t ars, void const *b,
                                   size_t brs, size_t w, size_t h, size_t ch, double range,
                                   alwan_ssim_params const *params, alwan__im_type t) {
    alwan_ssim_params p;
    size_t win, c, x, y, n;
    int uniform;
    double sigma, k1, k2, cov_norm, c1, c2, per[4], *buf, *pa, *pb;
    alwan_status st = ALWAN_OK;
    if (params) p = *params;
    else memset(&p, 0, sizeof p);
    if ((int)p.window < (int)ALWAN_SSIM_WINDOW_GAUSSIAN || (int)p.window > (int)ALWAN_SSIM_WINDOW_UNIFORM)
        return ALWAN_E_INVALID;
    if (!(range > 0.0) || range - range != 0.0) return ALWAN_E_INVALID;
    uniform = p.window == ALWAN_SSIM_WINDOW_UNIFORM;
    sigma = p.sigma > 0.0 ? p.sigma : 1.5;
    k1 = p.k1 > 0.0 ? p.k1 : 0.01;
    k2 = p.k2 > 0.0 ? p.k2 : 0.03;
    if (!(sigma == sigma) || sigma > 64.0 || p.k1 < 0.0 || p.k2 < 0.0) return ALWAN_E_RANGE;
    if (p.win_size) win = p.win_size;
    else if (uniform) win = 7;
    else win = 2 * (size_t)(3.5 * sigma + 0.5) + 1;
    if (win % 2 == 0 || win > w || win > h) return ALWAN_E_RANGE;
    cov_norm = p.sample_covariance ? (double)(win * win) / (double)(win * win - 1) : 1.0;
    c1 = (k1 * range) * (k1 * range);
    c2 = (k2 * range) * (k2 * range);
    n = w * h;
    buf = (double *)ALWAN_ALLOC((9 * n + w + w + h + 2 * win) * sizeof(double), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    pa = buf;
    pb = pa + n;
    for (c = 0; c < ch && st == ALWAN_OK; c++) {
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                pa[y * w + x] = alwan__im_at(a, ars, t, y, x * ch + c);
                pb[y * w + x] = alwan__im_at(b, brs, t, y, x * ch + c);
            }
        st = alwan__im_ssim_plane(&per[c], pa, pb, w, h, uniform, win, sigma, cov_norm, c1, c2, pb + n);
    }
    ALWAN_FREE(buf);
    if (st != ALWAN_OK) return st;
    {
        double s = 0.0;
        for (c = 0; c < ch; c++) s += per[c];
        *out = ch == 1 ? per[0] : s / (double)ch;
        if (per_out)
            for (c = 0; c < ch; c++) per_out[c] = per[c];
    }
    return ALWAN_OK;
}

#define ALWAN__IM_ENTRY_SSIM(SFX, T, TT)                                                                        \
    alwan_status alwan_structural_similarity_##SFX(double *ssim, T const *test, size_t test_row_stride,          \
                                                   T const *ref, size_t ref_row_stride, size_t width,            \
                                                   size_t height, size_t channels, double data_range,            \
                                                   alwan_ssim_params const *params) {                            \
        if (!ssim || !test || !ref || !alwan__im_dims_ok(width, height, channels)) return ALWAN_E_INVALID;       \
        return alwan__im_ssim(ssim, NULL, test, test_row_stride, ref, ref_row_stride, width, height, channels,         \
                              data_range, params, TT);                                                           \
    }

#if ALWAN_WITH_F64
ALWAN__IM_ENTRY_SSIM(f64, alwan_f64, ALWAN__IM_F64)
#endif
#if ALWAN_WITH_F32
ALWAN__IM_ENTRY_SSIM(f32, alwan_f32, ALWAN__IM_F32)
#endif
ALWAN__IM_ENTRY_SSIM(u8, unsigned char, ALWAN__IM_U8)

/* ---------------------------------------------------------------- *
 * Comparison metrics beside PSNR and SSIM: MAE, RMSE, AE, NCC, DSSIM
 *
 * These are written from their definitions, computed in double for every input type:
 * MAE and RMSE as fractions of the data range (ImageMagick's -metric MAE and RMSE
 * normalise by the quantum range the same way), AE as a count, NCC as Pearson's
 * correlation, DSSIM as (1 - SSIM) / 2 on alwan's SSIM above.
 * ---------------------------------------------------------------- */

/* Pearson correlation from centred sums. Constancy is tested on the values themselves, not
 * on the centred sums: a mean of n copies of 0.4 is not exactly 0.4, so a constant channel's
 * centred sum can come out a few ulps from 0. Both constant reads as 1 when they are equal
 * and 0 otherwise; one constant as 0. */
static double alwan__im_corr_of(double sab, double saa, double sbb, int a_const, int b_const, int equal) {
    if (a_const && b_const) return equal ? 1.0 : 0.0;
    if (a_const || b_const || !(saa > 0.0) || !(sbb > 0.0)) return 0.0;
    return sab / ALWAN_SQRT_F64(saa * sbb);
}

static alwan_status alwan__im_compare(double *combined, double *per_channel, void const *a, size_t ars,
                                      void const *b, size_t brs, size_t w, size_t h, size_t ch,
                                      alwan_image_compare_metric metric,
                                      alwan_image_compare_params const *params, alwan__im_type t) {
    alwan_image_compare_params p;
    double range, per[4] = {0.0, 0.0, 0.0, 0.0}, comb = 0.0;
    double const npix = (double)w * (double)h;
    size_t x, y, c;
    if (params) p = *params;
    else memset(&p, 0, sizeof p);
    range = p.data_range != 0.0 ? p.data_range : (t == ALWAN__IM_U8 ? 255.0 : 1.0);
    if (!(range > 0.0) || range - range != 0.0) return ALWAN_E_INVALID;
    if (!(p.fuzz >= 0.0) || p.fuzz - p.fuzz != 0.0) return ALWAN_E_INVALID;
    switch (metric) {
    case ALWAN_IMAGE_COMPARE_MAE:
    case ALWAN_IMAGE_COMPARE_RMSE: {
        int const sq = metric == ALWAN_IMAGE_COMPARE_RMSE;
        double total = 0.0;
        for (c = 0; c < ch; c++) {
            double s = 0.0;
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) {
                    double const d = (alwan__im_at(a, ars, t, y, x * ch + c) - alwan__im_at(b, brs, t, y, x * ch + c)) / range;
                    s += sq ? d * d : (d < 0.0 ? -d : d);
                }
            total += s;
            per[c] = sq ? ALWAN_SQRT_F64(s / npix) : s / npix;
        }
        comb = sq ? ALWAN_SQRT_F64(total / (npix * (double)ch)) : total / (npix * (double)ch);
    } break;
    case ALWAN_IMAGE_COMPARE_AE: {
        double const thr = p.fuzz * range, thr2 = thr * thr;
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                double d2 = 0.0;
                for (c = 0; c < ch; c++) {
                    double const d = alwan__im_at(a, ars, t, y, x * ch + c) - alwan__im_at(b, brs, t, y, x * ch + c);
                    double const ad = d < 0.0 ? -d : d;
                    if (ad > thr) per[c] += 1.0;
                    d2 += d * d;
                }
                if (d2 > thr2) comb += 1.0;
            }
    } break;
    case ALWAN_IMAGE_COMPARE_NCC:
    case ALWAN_IMAGE_COMPARE_NCC_POOLED: {
        double pma = 0.0, pmb = 0.0, psab = 0.0, psaa = 0.0, psbb = 0.0, mean_sum = 0.0;
        double const fa = alwan__im_at(a, ars, t, 0, 0), fb = alwan__im_at(b, brs, t, 0, 0);
        int pequal = 1, pa_const = 1, pb_const = 1;
        for (c = 0; c < ch; c++)
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) {
                    pma += alwan__im_at(a, ars, t, y, x * ch + c);
                    pmb += alwan__im_at(b, brs, t, y, x * ch + c);
                }
        pma /= npix * (double)ch;
        pmb /= npix * (double)ch;
        for (c = 0; c < ch; c++) {
            double ma = 0.0, mb = 0.0, sab = 0.0, saa = 0.0, sbb = 0.0;
            double const ca = alwan__im_at(a, ars, t, 0, c), cb = alwan__im_at(b, brs, t, 0, c);
            int equal = 1, a_const = 1, b_const = 1;
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) {
                    ma += alwan__im_at(a, ars, t, y, x * ch + c);
                    mb += alwan__im_at(b, brs, t, y, x * ch + c);
                }
            ma /= npix;
            mb /= npix;
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) {
                    double const va = alwan__im_at(a, ars, t, y, x * ch + c);
                    double const vb = alwan__im_at(b, brs, t, y, x * ch + c);
                    double const da = va - ma, db = vb - mb, ea = va - pma, eb = vb - pmb;
                    sab += da * db;
                    saa += da * da;
                    sbb += db * db;
                    psab += ea * eb;
                    psaa += ea * ea;
                    psbb += eb * eb;
                    if (va != vb) equal = 0;
                    if (va != ca) a_const = 0;
                    if (vb != cb) b_const = 0;
                    if (va != fa) pa_const = 0;
                    if (vb != fb) pb_const = 0;
                }
            if (!equal) pequal = 0;
            per[c] = alwan__im_corr_of(sab, saa, sbb, a_const, b_const, equal);
            mean_sum += per[c];
        }
        comb = metric == ALWAN_IMAGE_COMPARE_NCC ? mean_sum / (double)ch
                                                 : alwan__im_corr_of(psab, psaa, psbb, pa_const, pb_const, pequal);
    } break;
    case ALWAN_IMAGE_COMPARE_DSSIM: {
        double ssim = 0.0, sper[4];
        alwan_status const st = alwan__im_ssim(&ssim, sper, a, ars, b, brs, w, h, ch, range, p.ssim, t);
        if (st != ALWAN_OK) return st;
        for (c = 0; c < ch; c++) per[c] = (1.0 - sper[c]) / 2.0;
        comb = (1.0 - ssim) / 2.0;
    } break;
    default:
        return ALWAN_E_INVALID;
    }
    if (combined) *combined = comb;
    if (per_channel)
        for (c = 0; c < ch; c++) per_channel[c] = per[c];
    return ALWAN_OK;
}

#if ALWAN_WITH_F64
alwan_status alwan_image_compare_f64(double *combined, double *per_channel, alwan_f64 const *a, size_t a_row_stride,
                                     alwan_f64 const *b, size_t b_row_stride, size_t width, size_t height,
                                     size_t channels, alwan_image_compare_metric metric,
                                     alwan_image_compare_params const *params) {
    if ((!combined && !per_channel) || !a || !b || !alwan__im_dims_ok(width, height, channels)) return ALWAN_E_INVALID;
    return alwan__im_compare(combined, per_channel, a, a_row_stride, b, b_row_stride, width, height, channels, metric,
                             params, ALWAN__IM_F64);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_image_compare_f32(double *combined, double *per_channel, alwan_f32 const *a, size_t a_row_stride,
                                     alwan_f32 const *b, size_t b_row_stride, size_t width, size_t height,
                                     size_t channels, alwan_image_compare_metric metric,
                                     alwan_image_compare_params const *params) {
    if ((!combined && !per_channel) || !a || !b || !alwan__im_dims_ok(width, height, channels)) return ALWAN_E_INVALID;
    return alwan__im_compare(combined, per_channel, a, a_row_stride, b, b_row_stride, width, height, channels, metric,
                             params, ALWAN__IM_F32);
}
#endif

alwan_status alwan_image_compare_u8(double *combined, double *per_channel, unsigned char const *a, size_t a_row_stride,
                                    unsigned char const *b, size_t b_row_stride, size_t width, size_t height,
                                    size_t channels, alwan_image_compare_metric metric,
                                    alwan_image_compare_params const *params) {
    if ((!combined && !per_channel) || !a || !b || !alwan__im_dims_ok(width, height, channels)) return ALWAN_E_INVALID;
    return alwan__im_compare(combined, per_channel, a, a_row_stride, b, b_row_stride, width, height, channels, metric,
                             params, ALWAN__IM_U8);
}
