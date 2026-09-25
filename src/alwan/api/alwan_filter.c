/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_filter_{T}: the linear filters, each channel of an image on its own.
 *
 *   GAUSSIAN     scipy.ndimage.gaussian_filter, as scikit-image's filters.gaussian calls it
 *   DOG          scikit-image's filters.difference_of_gaussians: two of those, subtracted
 *   LOG          scipy.ndimage.gaussian_laplace: the second derivative of the Gaussian
 *                along each axis, summed
 *   LAPLACE      scipy.ndimage.laplace: [1, -2, 1] along each axis, summed
 *   BUTTERWORTH  scikit-image's filters.butterworth, in the frequency domain
 *
 * The spatial filters are scipy's separable passes reproduced step for step, so that
 * suite 246 can hold them to scipy and scikit-image to the last bit. The kernel is
 * _gaussian_kernel1d: phi(x) = exp(-0.5 / sigma^2 * x^2) over x = -r..r with
 * r = int(truncate sigma + 0.5), divided by its sum (numpy's pairwise sum), times the
 * polynomial of the derivative's order, and reversed as gaussian_filter1d reverses it.
 * correlate1d extends each line by the border mode and sums a symmetric kernel as
 * c0 w0 + sum (c-j + cj) w-j, an antisymmetric one as c0 w0 + sum (c-j - cj) w-j, the
 * symmetry tested with scipy's DBL_EPSILON. The rows' axis runs first, then the columns';
 * an axis whose sigma is at most 1e-15 is skipped, as gaussian_filter skips it. f32 data
 * is rounded to float after every pass, as scipy stores a float32 image's intermediates.
 *
 * The Butterworth mask is scikit-image's _get_nd_butterworth_filter, built in the data's
 * precision: per axis ((k - (d - 1) // 2) / (d c))^2 ifftshifted, summed over the axes,
 * raised to the order, 1 / (1 + q) (times q for a high-pass), its square root unless
 * squared. The transform is alwan__fft over the whole spectrum in double, where
 * scikit-image runs a real FFT (single precision on float32 data), so this one agrees
 * to rounding rather than to the bit.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

#define ALWAN__FL_MAX_RADIUS ((size_t)1 << 20)

static double alwan_fl_r(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

/* numpy's pairwise sum, as phi.sum() normalises the kernel. */
static double alwan_fl_sum(double const *a, size_t n) {
    double r[8], res = 0.0;
    size_t i, j;
    if (n < 8) {
        for (i = 0; i < n; i++) res += a[i];
        return res;
    }
    if (n > 128) {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan_fl_sum(a, n2) + alwan_fl_sum(a + n2, n - n2);
    }
    for (j = 0; j < 8; j++) r[j] = a[j];
    for (i = 8; i < n - (n % 8); i += 8)
        for (j = 0; j < 8; j++) r[j] += a[i + j];
    res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    for (; i < n; i++) res += a[i];
    return res;
}

/* _gaussian_kernel1d(sigma, order, lw)[::-1] into k (2 lw + 1 values), order 0 to 2. The
 * derivative's polynomial is q(x) = x p for order 1 and p + x^2 p^2 for order 2,
 * p = 1 / -sigma^2, numpy's Q_deriv iterated and dotted with the powers of x. That dot
 * runs through BLAS, which fuses the last multiply and add (OpenBLAS on x86-64 with
 * FMA), so order 2 is fma(x^2, p^2, p): a separate multiply and add differed from scipy
 * by an ulp of the kernel, 6e-14 of the result. */
static void alwan_fl_kernel(double *k, double *phi, double sigma, int order, size_t lw) {
    double const s2 = sigma * sigma, a = -0.5 / s2, p = 1.0 / -s2;
    size_t const n = 2 * lw + 1;
    size_t i;
    double s;
    for (i = 0; i < n; i++) {
        long const x = (long)i - (long)lw;
        phi[i] = exp(a * (double)(x * x));
    }
    s = alwan_fl_sum(phi, n);
    for (i = 0; i < n; i++) phi[i] = phi[i] / s;
    for (i = 0; i < n; i++) {
        long const x = (long)i - (long)lw;
        double q;
        if (order == 0) q = 1.0;
        else if (order == 1) q = 0.0 + (double)x * p;
        else q = fma((double)(x * x), p * p, p);   /* numpy's dot fuses it (OpenBLAS) */
        k[n - 1 - i] = order == 0 ? phi[i] : q * phi[i];
    }
}

/* scipy's NI_ExtendLine: the source index for position j of a line of len values, or -1
 * for the constant value. */
static long alwan_fl_index(long j, long len, alwan_filter_border border) {
    long m, p;
    if (j >= 0 && j < len) return j;
    switch (border) {
    case ALWAN_FILTER_BORDER_NEAREST: return j < 0 ? 0 : len - 1;
    case ALWAN_FILTER_BORDER_REFLECT:
        p = 2 * len;
        m = j % p;
        if (m < 0) m += p;
        return m < len ? m : p - 1 - m;
    case ALWAN_FILTER_BORDER_MIRROR:
        if (len == 1) return 0;
        p = 2 * len - 2;
        m = j % p;
        if (m < 0) m += p;
        return m < len ? m : p - m;
    case ALWAN_FILTER_BORDER_WRAP:
        m = j % len;
        return m < 0 ? m + len : m;
    default: return -1;
    }
}

/* scipy's correlate1d on `count` lines of `len` values: element i of line l at
 * in[l lstep + i istep]. wts has 2 half + 1 values, already reversed. */
static void alwan_fl_corr(double *out, double const *in, size_t count, size_t len, size_t lstep, size_t istep,
                          double const *wts, size_t half, alwan_filter_border border, double cval, int f32, double *ext) {
    double const *fw = wts + half;
    size_t l, i, ii;
    long j;
    int sym = 1;
    for (ii = 1; ii <= half; ii++)
        if (fabs(fw[ii] - fw[-(long)ii]) > DBL_EPSILON) {
            sym = 0;
            break;
        }
    if (!sym) {
        sym = -1;
        for (ii = 1; ii <= half; ii++)
            if (fabs(fw[ii] + fw[-(long)ii]) > DBL_EPSILON) {
                sym = 0;
                break;
            }
    }
    for (l = 0; l < count; l++) {
        for (j = -(long)half; j < (long)(len + half); j++) {
            long const src = alwan_fl_index(j, (long)len, border);
            ext[j + (long)half] = src < 0 ? cval : in[l * lstep + (size_t)src * istep];
        }
        for (i = 0; i < len; i++) {
            double const *c = ext + half + i;
            double acc;
            if (sym > 0) {
                acc = c[0] * fw[0];
                for (j = -(long)half; j < 0; j++) acc += (c[j] + c[-j]) * fw[j];
            } else if (sym < 0) {
                acc = c[0] * fw[0];
                for (j = -(long)half; j < 0; j++) acc += (c[j] - c[-j]) * fw[j];
            } else {
                acc = c[half] * fw[half];
                for (j = -(long)half; j < (long)half; j++) acc += c[j] * fw[j];
            }
            out[l * lstep + i * istep] = alwan_fl_r(acc, f32);
        }
    }
}

typedef struct {
    alwan_filter_border border;
    double cval, truncate;
    int f32;
    double *ext, *k, *phi, *tmp;
} alwan_fl_ctx;

/* gaussian_filter on one w x h plane with order o0 along the rows' axis (down the
 * columns) and o1 along the columns' axis; out may not be in. */
static void alwan_fl_gauss(double *out, double const *in, size_t w, size_t h, double s0, double s1, int o0, int o1,
                           alwan_fl_ctx const *c) {
    double const *src = in;
    int done = 0;
    if (s0 > 1e-15) {
        size_t const lw = (size_t)(c->truncate * s0 + 0.5);
        alwan_fl_kernel(c->k, c->phi, s0, o0, lw);
        alwan_fl_corr(c->tmp, src, w, h, 1, w, c->k, lw, c->border, c->cval, c->f32, c->ext);
        src = c->tmp;
        done = 1;
    }
    if (s1 > 1e-15) {
        size_t const lw = (size_t)(c->truncate * s1 + 0.5);
        alwan_fl_kernel(c->k, c->phi, s1, o1, lw);
        alwan_fl_corr(out, src, h, w, w, 1, c->k, lw, c->border, c->cval, c->f32, c->ext);
        done = 2;
    }
    if (done != 2) memcpy(out, src, w * h * sizeof(double));
}

/* numpy.fft.ifftshift of ((k - (d - 1) // 2) / (d f))^2, k = 0..d-1, into r. */
static void alwan_fl_bw_axis(double *r, size_t d, double factor) {
    long const start = -((long)d / 2);   /* python's -(d - 1) // 2, which floors to -(d // 2) */
    size_t k;
    for (k = 0; k < d; k++) {
        size_t const src = (k + d / 2) % d;
        double const v = (double)(start + (long)src) / ((double)d * factor);
        r[k] = v * v;
    }
}

/* numpy's power with a scalar exponent: 2 squares, 1 copies, 0.5 roots, -1 inverts. */
static double alwan_fl_pow(double q, double order, int f32) {
    if (order == 2.0) return alwan_fl_r(q * q, f32);
    if (order == 1.0) return q;
    if (order == 0.5) return alwan_fl_r(sqrt(q), f32);
    if (order == -1.0) return alwan_fl_r(1.0 / q, f32);
    return f32 ? (double)powf((float)q, (float)order) : pow(q, order);
}

static alwan_status alwan_fl_butterworth(double *plane, size_t w, size_t h, alwan_filter_params const *p, int f32) {
    double const factor = p->cutoff_frequency_ratio > 0.0 ? p->cutoff_frequency_ratio : 0.005;
    double const order = p->order > 0.0 ? p->order : 2.0;
    size_t const pad = p->npad;
    size_t const W = w + 2 * pad, H = h + 2 * pad, N = W * H;
    double *buf, *re, *im, *r0, *r1, *cre, *cim;
    alwan__fft *fw = NULL, *fh = NULL;
    size_t x, y;
    alwan_status st = ALWAN_OK;
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(2 * N + W + H + 2 * H, sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    re = buf, im = re + N, r0 = im + N, r1 = r0 + H, cre = r1 + W, cim = cre + H;
    fw = alwan__fft_create(W);
    fh = alwan__fft_create(H);
    if (!fw || !fh) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    /* numpy.pad(mode="edge") */
    for (y = 0; y < H; y++) {
        size_t const sy = y < pad ? 0 : (y - pad >= h ? h - 1 : y - pad);
        for (x = 0; x < W; x++) {
            size_t const sx = x < pad ? 0 : (x - pad >= w ? w - 1 : x - pad);
            re[y * W + x] = plane[sy * w + sx];
            im[y * W + x] = 0.0;
        }
    }
    for (y = 0; y < H; y++) alwan__fft_run(fw, re + y * W, im + y * W, 0);
    for (x = 0; x < W; x++) {
        for (y = 0; y < H; y++) cre[y] = re[y * W + x], cim[y] = im[y * W + x];
        alwan__fft_run(fh, cre, cim, 0);
        for (y = 0; y < H; y++) re[y * W + x] = cre[y], im[y * W + x] = cim[y];
    }
    alwan_fl_bw_axis(r0, H, factor);
    alwan_fl_bw_axis(r1, W, factor);
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            double q = alwan_fl_r(r0[y] + r1[x], f32), wf;
            q = alwan_fl_pow(q, order, f32);
            wf = alwan_fl_r(1.0 / alwan_fl_r(1.0 + q, f32), f32);
            if (!p->low_pass) wf = alwan_fl_r(wf * q, f32);
            if (p->unsquared) wf = alwan_fl_r(sqrt(wf), f32);
            re[y * W + x] *= wf;
            im[y * W + x] *= wf;
        }
    for (x = 0; x < W; x++) {
        for (y = 0; y < H; y++) cre[y] = re[y * W + x], cim[y] = im[y * W + x];
        alwan__fft_run(fh, cre, cim, 1);
        for (y = 0; y < H; y++) re[y * W + x] = cre[y], im[y * W + x] = cim[y];
    }
    for (y = 0; y < H; y++) alwan__fft_run(fw, re + y * W, im + y * W, 1);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) plane[y * w + x] = re[(y + pad) * W + (x + pad)] / (double)N;
done:
    alwan__fft_destroy(fw);
    alwan__fft_destroy(fh);
    ALWAN_FREE(buf);
    return st;
}

/* The per-axis sigmas: sigma_row and sigma_col when either is given, else sigma on both,
 * 0 reading as 1. */
static int alwan_fl_sigmas(double *s0, double *s1, double s, double sr, double sc) {
    if (!(sr >= 0.0) || !(sc >= 0.0) || !(s >= 0.0) || sr > 1e6 || sc > 1e6 || s > 1e6) return 0;
    if (sr > 0.0 || sc > 0.0) *s0 = sr, *s1 = sc;
    else *s0 = *s1 = s > 0.0 ? s : 1.0;
    return 1;
}

/* kind: 0 f64, 1 f32, 2 u8 */
static alwan_status alwan_fl_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_filter_method method, alwan_filter_params const *params, int kind) {
    alwan_filter_params const zero = { 0 };
    alwan_filter_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1;
    int const f32 = kind == 1;
    size_t const n = w * h;
    double s0 = 0.0, s1 = 0.0, h0 = 0.0, h1 = 0.0, lwmax = 0.0;
    double *buf, *img, *res, *aux, *ext;
    alwan_fl_ctx c;
    size_t x, y, k, extn, kn;
    alwan_status st = ALWAN_OK;
    if (!out || !src || w == 0 || h == 0 || ch < 1 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem < w * ch || out_rs / elem < w * ch) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_FILTER_BUTTERWORTH || (unsigned)p->border > (unsigned)ALWAN_FILTER_BORDER_WRAP)
        return ALWAN_E_INVALID;
    if (!(p->cval - p->cval == 0.0)) return ALWAN_E_INVALID;
    if (kind == 2 && !(method == ALWAN_FILTER_GAUSSIAN || (method == ALWAN_FILTER_BUTTERWORTH && p->low_pass)))
        return ALWAN_E_INVALID;
    c.truncate = p->truncate > 0.0 ? p->truncate : 4.0;
    if (!(p->truncate >= 0.0) || c.truncate > 1e6) return ALWAN_E_RANGE;
    c.border = p->border;
    c.cval = p->cval;
    c.f32 = f32;
    if (method == ALWAN_FILTER_GAUSSIAN || method == ALWAN_FILTER_DOG || method == ALWAN_FILTER_LOG) {
        if (!alwan_fl_sigmas(&s0, &s1, p->sigma, p->sigma_row, p->sigma_col)) return ALWAN_E_RANGE;
        if (method == ALWAN_FILTER_DOG) {
            if (!(p->high_sigma >= 0.0) || !(p->high_sigma_row >= 0.0) || !(p->high_sigma_col >= 0.0)) return ALWAN_E_RANGE;
            if (p->high_sigma_row > 0.0 || p->high_sigma_col > 0.0) h0 = p->high_sigma_row, h1 = p->high_sigma_col;
            else if (p->high_sigma > 0.0) h0 = h1 = p->high_sigma;
            else h0 = s0 * 1.6, h1 = s1 * 1.6;
            if (h0 < s0 || h1 < s1 || h0 > 1e6 || h1 > 1e6) return ALWAN_E_RANGE;
        }
        lwmax = c.truncate * (s0 > s1 ? s0 : s1);
        if (method == ALWAN_FILTER_DOG) lwmax = c.truncate * (h0 > h1 ? h0 : h1);
        if (lwmax + 0.5 >= (double)ALWAN__FL_MAX_RADIUS) return ALWAN_E_RANGE;
    } else if (method == ALWAN_FILTER_LAPLACE) {
        lwmax = 1.0;
    } else {
        if (!(p->cutoff_frequency_ratio >= 0.0) || p->cutoff_frequency_ratio > 0.5) return ALWAN_E_RANGE;
        if (!(p->order >= 0.0) || p->order > 1e3) return ALWAN_E_RANGE;
        if (p->npad > 65536) return ALWAN_E_RANGE;
    }
    kn = 2 * ((size_t)(lwmax + 0.5) + 1) + 1;
    extn = (w > h ? w : h) + kn + 2;
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(4 * n + extn + 2 * kn, sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    img = buf, res = img + n, aux = res + n, c.tmp = aux + n, ext = c.tmp + n;
    c.ext = ext;
    c.k = ext + extn;
    c.phi = c.k + kn;
    for (k = 0; k < ch && st == ALWAN_OK; k++) {
        for (y = 0; y < h; y++) {
            char const *row = (char const *)src + y * src_rs;
            for (x = 0; x < w; x++) {
                double v;
                if (kind == 0) v = ((alwan_f64 const *)row)[x * ch + k];
                else if (kind == 1) v = ((alwan_f32 const *)row)[x * ch + k];
                else v = (double)((unsigned char const *)row)[x * ch + k];
                if (!(v - v == 0.0)) {
                    ALWAN_FREE(buf);
                    return ALWAN_E_INVALID;
                }
                /* scikit-image's img_as_float multiplies by 1 / 255; its butterworth
                 * transforms the raw values */
                if (kind == 2 && method == ALWAN_FILTER_GAUSSIAN) v = v * (1.0 / 255.0);
                img[y * w + x] = v;
            }
        }
        switch (method) {
        case ALWAN_FILTER_GAUSSIAN: alwan_fl_gauss(res, img, w, h, s0, s1, 0, 0, &c); break;
        case ALWAN_FILTER_DOG:
            alwan_fl_gauss(res, img, w, h, s0, s1, 0, 0, &c);
            alwan_fl_gauss(aux, img, w, h, h0, h1, 0, 0, &c);
            for (x = 0; x < n; x++) res[x] = alwan_fl_r(res[x] - aux[x], f32);
            break;
        case ALWAN_FILTER_LOG:
            alwan_fl_gauss(res, img, w, h, s0, s1, 2, 0, &c);
            alwan_fl_gauss(aux, img, w, h, s0, s1, 0, 2, &c);
            for (x = 0; x < n; x++) res[x] = alwan_fl_r(res[x] + aux[x], f32);
            break;
        case ALWAN_FILTER_LAPLACE: {
            static double const lap[3] = { 1.0, -2.0, 1.0 };
            alwan_fl_corr(res, img, w, h, 1, w, lap, 1, c.border, c.cval, f32, ext);
            alwan_fl_corr(aux, img, h, w, w, 1, lap, 1, c.border, c.cval, f32, ext);
            for (x = 0; x < n; x++) res[x] = alwan_fl_r(res[x] + aux[x], f32);
            break;
        }
        default:
            memcpy(res, img, n * sizeof(double));
            st = alwan_fl_butterworth(res, w, h, p, f32);
            break;
        }
        if (st != ALWAN_OK) break;
        for (y = 0; y < h; y++) {
            char *row = (char *)out + y * out_rs;
            for (x = 0; x < w; x++) {
                double const v = res[y * w + x];
                if (kind == 0) ((alwan_f64 *)row)[x * ch + k] = v;
                else if (kind == 1) ((alwan_f32 *)row)[x * ch + k] = (alwan_f32)v;
                else {
                    double const t = nearbyint(method == ALWAN_FILTER_GAUSSIAN ? v * 255.0 : v);
                    ((unsigned char *)row)[x * ch + k] = (unsigned char)(t < 0.0 ? 0.0 : t > 255.0 ? 255.0 : t);
                }
            }
        }
    }
    ALWAN_FREE(buf);
    return st;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_filter_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels,
                              size_t width, size_t height, alwan_filter_method method, alwan_filter_params const *params) {
    return alwan_fl_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_filter_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels,
                              size_t width, size_t height, alwan_filter_method method, alwan_filter_params const *params) {
    return alwan_fl_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif

alwan_status alwan_filter_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src, size_t src_row_stride,
                             size_t channels, size_t width, size_t height, alwan_filter_method method,
                             alwan_filter_params const *params) {
    return alwan_fl_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 2);
}
