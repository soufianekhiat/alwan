/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_threshold_local_{T}: a threshold for every pixel from its neighbourhood, as
 * scikit-image's filters.threshold_local (gaussian, mean, median), threshold_niblack and
 * threshold_sauvola compute it, which suite 222 holds this to.
 *
 *   GAUSSIAN  scipy's gaussian_filter, sigma (block_size - 1) / 6, truncated at 4 sigma,
 *             minus offset
 *   MEAN      scipy's uniform_filter over block_size x block_size, minus offset
 *   MEDIAN    scipy's median_filter over block_size x block_size, minus offset
 *   NIBLACK   m - k s
 *   SAUVOLA   m (1 + k (s / r - 1))
 *
 * The first three filter the columns, then the rows, each pass stored in the data's
 * precision, with scipy's "reflect" edge (d c b a | a b c d). The gaussian correlates as
 * scipy's symmetric kernel does, outer pairs first; the mean is scipy's running sum. m and
 * s, the mean and standard deviation over the window, come from scikit-image's integral
 * images: the image padded by numpy's "reflect" (d c b | a b c d), w / 2 + 1 above and to
 * the left and w / 2 below and to the right, summed down the columns and then along the
 * rows in double, the window's sum the four-corner combination in scikit-image's order.
 * On float32 data every operation outside the double sums, and every constant, is rounded
 * to float as numpy rounds it.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double alwan_tl_r(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

/* scipy's "reflect": period 2 n, d c b a | a b c d | d c b a */
static size_t alwan_tl_scipy_reflect(long i, size_t n) {
    long const p = 2 * (long)n;
    long m = i % p;
    if (m < 0) m += p;
    return (size_t)(m >= (long)n ? p - 1 - m : m);
}

/* numpy's pad "reflect": period 2 (n - 1), d c b | a b c d | c b a */
static size_t alwan_tl_numpy_reflect(long i, size_t n) {
    long p, m;
    if (n == 1) return 0;
    p = 2 * ((long)n - 1);
    m = i % p;
    if (m < 0) m += p;
    return (size_t)(m >= (long)n ? p - m : m);
}

/* One pass of scipy's gaussian (kind 0) or uniform (1) filter along lines: count lines of
 * len samples, sample (l, i) at in[l * lstep + i * istep]. ext holds len + 2 half + 1. */
static void alwan_tl_pass(double *out, double const *in, size_t count, size_t len, size_t lstep, size_t istep, int kind,
                          double const *wts, size_t half, int f32, double *ext) {
    size_t l, i;
    long j;
    for (l = 0; l < count; l++) {
        for (j = -(long)half; j < (long)(len + half); j++)
            ext[j + (long)half] = in[l * lstep + alwan_tl_scipy_reflect(j, len) * istep];
        if (kind == 0) {
            for (i = 0; i < len; i++) {
                double const *c = ext + half + i;
                double acc = c[0] * wts[half];
                for (j = -(long)half; j < 0; j++) acc += (c[j] + c[-j]) * wts[(long)half + j];
                out[l * lstep + i * istep] = alwan_tl_r(acc, f32);
            }
        } else {
            size_t const size = 2 * half + 1;
            double t = 0.0;
            for (i = 0; i < size; i++) t += ext[i];
            out[l * lstep] = alwan_tl_r(t / (double)size, f32);
            for (i = 1; i < len; i++) {
                t += ext[i + size - 1] - ext[i - 1];
                out[l * lstep + i * istep] = alwan_tl_r(t / (double)size, f32);
            }
        }
    }
}

/* numpy's pairwise sum, for the kernel's normalisation (fewer than 129 terms here, never
 * more than the eight-accumulator block). */
static double alwan_tl_sum(double const *a, size_t n) {
    double r[8], res = 0.0;
    size_t i, j;
    if (n < 8) {
        for (i = 0; i < n; i++) res += a[i];
        return res;
    }
    if (n > 128) {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan_tl_sum(a, n2) + alwan_tl_sum(a + n2, n - n2);
    }
    for (j = 0; j < 8; j++) r[j] = a[j];
    for (i = 8; i < n - (n % 8); i += 8)
        for (j = 0; j < 8; j++) r[j] += a[i + j];
    res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    for (; i < n; i++) res += a[i];
    return res;
}

static int alwan_tl_cmp(void const *a, void const *b) {
    double const x = *(double const *)a, y = *(double const *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

static alwan_status alwan_tl_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_threshold_local_method method, alwan_threshold_local_params const *params, int f32) {
    alwan_threshold_local_params const zero = { 0 };
    alwan_threshold_local_params const *p = params ? params : &zero;
    size_t const elem = f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const b = p->block_size ? p->block_size : 15, half = b / 2;
    size_t const n = w * h, pw = w + b, ph = h + b;
    double const k = alwan_tl_r(p->k != 0.0 ? p->k : 0.2, f32);
    double const r = alwan_tl_r(p->r != 0.0 ? p->r : 1.0, f32);
    double const offset = alwan_tl_r(p->offset, f32);
    double const sigma = p->sigma > 0.0 ? p->sigma : (double)(b - 1) / 6.0;
    size_t const lw = (size_t)(4.0 * sigma + 0.5);
    size_t const ext_len = (w > h ? w : h) + 2 * (lw > half ? lw : half) + 2;
    double *buf, *img, *tmp, *res, *ext, *wts, *ia = NULL, *ib = NULL;
    size_t x, y, c, i;
    alwan_status st = ALWAN_OK;
    if (!out || !src || w == 0 || h == 0 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_THRESHOLD_LOCAL_SAUVOLA) return ALWAN_E_INVALID;
    if (b % 2 == 0 || b > 1023 || !(p->sigma >= 0.0) || p->sigma > 256.0 || !(p->k == p->k) || !(p->r == p->r) ||
        !(p->offset == p->offset))
        return ALWAN_E_RANGE;
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(3 * n + ext_len + 2 * lw + 1 + b * b, sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    img = buf;
    tmp = img + n;
    res = tmp + n;
    ext = res + n;
    wts = ext + ext_len;
    if (method == ALWAN_THRESHOLD_LOCAL_NIBLACK || method == ALWAN_THRESHOLD_LOCAL_SAUVOLA) {
        ia = (double *)ALWAN_ALLOC(alwan_safe_array_size(pw * ph, 2 * sizeof(double)), sizeof(double));
        if (!ia) {
            ALWAN_FREE(buf);
            return ALWAN_E_NOMEM;
        }
        ib = ia + pw * ph;
    }
    if (method == ALWAN_THRESHOLD_LOCAL_GAUSSIAN && sigma > 1e-15) {
        /* scipy's _gaussian_kernel1d: exp(-0.5 / sigma^2 * x^2), normalised by numpy's sum */
        double const a = -0.5 / (sigma * sigma);
        double s;
        for (i = 0; i <= 2 * lw; i++) {
            long const xi = (long)i - (long)lw;
            wts[i] = ALWAN_EXP_F64(a * (double)(xi * xi));
        }
        s = alwan_tl_sum(wts, 2 * lw + 1);
        for (i = 0; i <= 2 * lw; i++) wts[i] = wts[i] / s;
    }
    for (c = 0; c < ch && st == ALWAN_OK; c++) {
        for (y = 0; y < h; y++) {
            char const *row = (char const *)src + y * src_rs;
            for (x = 0; x < w; x++) {
                double const v = f32 ? (double)((alwan_f32 const *)row)[x * ch + c] : ((alwan_f64 const *)row)[x * ch + c];
                if (!(v - v == 0.0)) {
                    st = ALWAN_E_INVALID;
                    break;
                }
                img[y * w + x] = v;
            }
        }
        if (st != ALWAN_OK) break;
        switch (method) {
        case ALWAN_THRESHOLD_LOCAL_GAUSSIAN:
            if (sigma > 1e-15) {
                alwan_tl_pass(tmp, img, w, h, 1, w, 0, wts, lw, f32, ext);   /* axis 0: down the columns */
                alwan_tl_pass(res, tmp, h, w, w, 1, 0, wts, lw, f32, ext);   /* axis 1: along the rows */
            } else {
                memcpy(res, img, n * sizeof(double));
            }
            for (i = 0; i < n; i++) res[i] = alwan_tl_r(res[i] - offset, f32);
            break;
        case ALWAN_THRESHOLD_LOCAL_MEAN:
            alwan_tl_pass(tmp, img, w, h, 1, w, 1, NULL, half, f32, ext);
            alwan_tl_pass(res, tmp, h, w, w, 1, 1, NULL, half, f32, ext);
            for (i = 0; i < n; i++) res[i] = alwan_tl_r(res[i] - offset, f32);
            break;
        case ALWAN_THRESHOLD_LOCAL_MEDIAN: {
            double *win = wts;   /* b * b entries follow the kernel */
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    size_t m = 0, u, v;
                    for (u = 0; u < b; u++) {
                        size_t const sy = alwan_tl_scipy_reflect((long)y + (long)u - (long)half, h);
                        for (v = 0; v < b; v++) win[m++] = img[sy * w + alwan_tl_scipy_reflect((long)x + (long)v - (long)half, w)];
                    }
                    qsort(win, m, sizeof(double), alwan_tl_cmp);
                    res[y * w + x] = alwan_tl_r(alwan_tl_r(win[m / 2], f32) - offset, f32);
                }
            }
            break;
        }
        default: {   /* NIBLACK, SAUVOLA */
            double const area = alwan_tl_r((double)(b * b), f32);
            long const top = (long)half + 1;
            for (y = 0; y < ph; y++) {
                size_t const sy = alwan_tl_numpy_reflect((long)y - top, h);
                for (x = 0; x < pw; x++) {
                    double const v = img[sy * w + alwan_tl_numpy_reflect((long)x - top, w)];
                    ia[y * pw + x] = v;
                    ib[y * pw + x] = alwan_tl_r(v * v, f32);
                }
            }
            for (y = 1; y < ph; y++)
                for (x = 0; x < pw; x++) {
                    ia[y * pw + x] += ia[(y - 1) * pw + x];
                    ib[y * pw + x] += ib[(y - 1) * pw + x];
                }
            for (y = 0; y < ph; y++)
                for (x = 1; x < pw; x++) {
                    ia[y * pw + x] += ia[y * pw + x - 1];
                    ib[y * pw + x] += ib[y * pw + x - 1];
                }
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    size_t const o00 = y * pw + x, o01 = o00 + b, o10 = (y + b) * pw + x, o11 = o10 + b;
                    double m = ((ia[o00] - ia[o01]) - ia[o10]) + ia[o11];
                    double g2 = ((ib[o00] - ib[o01]) - ib[o10]) + ib[o11];
                    double s, t;
                    m = alwan_tl_r(alwan_tl_r(m, f32) / area, f32);
                    g2 = alwan_tl_r(alwan_tl_r(g2, f32) / area, f32);
                    s = alwan_tl_r(g2 - alwan_tl_r(m * m, f32), f32);
                    s = alwan_tl_r(ALWAN_SQRT_F64(s > 0.0 ? s : 0.0), f32);
                    if (method == ALWAN_THRESHOLD_LOCAL_NIBLACK) {
                        t = alwan_tl_r(m - alwan_tl_r(k * s, f32), f32);
                    } else {
                        t = alwan_tl_r(alwan_tl_r(s / r, f32) - 1.0, f32);
                        t = alwan_tl_r(1.0 + alwan_tl_r(k * t, f32), f32);
                        t = alwan_tl_r(m * t, f32);
                    }
                    res[y * w + x] = t;
                }
            }
            break;
        }
        }
        for (y = 0; y < h; y++) {
            char *row = (char *)out + y * out_rs;
            for (x = 0; x < w; x++) {
                if (f32) ((alwan_f32 *)row)[x * ch + c] = (alwan_f32)res[y * w + x];
                else ((alwan_f64 *)row)[x * ch + c] = res[y * w + x];
            }
        }
    }
    ALWAN_FREE(ia);
    ALWAN_FREE(buf);
    return st;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_threshold_local_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height, alwan_threshold_local_method method,
                                       alwan_threshold_local_params const *params) {
    return alwan_tl_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_threshold_local_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height, alwan_threshold_local_method method,
                                       alwan_threshold_local_params const *params) {
    return alwan_tl_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
