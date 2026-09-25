/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Illuminant estimation from an image: the e(n, p, sigma) family of van de Weijer,
 * Gevers and Gijsenij, "Edge-Based Color Constancy", IEEE TIP 16(9), 2007, which holds
 * Grey World, White Patch, Shades of Grey, general Grey World and the Grey-Edge
 * estimators as parameter settings of one expression.
 *
 * THE REFERENCE IS THE AUTHORS' MATLAB CODE, general_cc.m and its helpers (gDer,
 * norm_derivative, dilation33, set_border, fill_border). It carries no licence, so none
 * of it is here: this file is written from the paper, and the code is fetched at
 * generation time and run as the oracle (gendata/tests/constancy_reference.py, suite
 * 183). Reading it settled what the paper leaves open, and this file follows it:
 *
 *   the kernels are sampled over +-floor(3 sigma + 0.5) taps; the first derivative is
 *   scaled so sum(x g) = 1, the second has its mean removed and is scaled so
 *   sum(x^2 g / 2) = 1, and the Gaussian itself sums to 1;
 *
 *   filtering is correlation over the image with its edge replicated, x first, then y;
 *
 *   the second-order magnitude is sqrt(fxx^2 + 4 fxy^2 + fyy^2), per channel;
 *
 *   the pixels used exclude a border of sigma + 1 (a real width: a pixel at 1-based
 *   column x counts when width < x < W - width + 1), every pixel whose largest channel
 *   reaches the saturation level together with its 3 x 3 neighbourhood, and the
 *   caller's mask, which is not dilated;
 *
 *   the three sums are scaled to unit length, and the correction divides by
 *   e * sqrt(3).
 *
 * The code's saturation level is 255, for 8-bit images; here it is a parameter, and 0
 * turns it off for linear float images that have no clipping level.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <float.h>

#define ALWAN_CC_TAPS_MAX 1024   /* sigma up to about 340 pixels */
#define ALWAN_CC_PI 3.14159265358979323846

void alwan_constancy_params_init(alwan_constancy_params *params) {
    if (!params) return;
    params->order = 0;
    params->minkowski = 1.0;
    params->sigma = 0.0;
    params->saturation = 0.0;
}

static int alwan_cc_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

/* One of the reference's three 1D kernels, 2 fs + 1 taps. */
static void alwan_cc_kernel(double *k, int fs, double sigma, int order) {
    double sum = 0.0, norm = 0.0;
    int i;
    for (i = -fs; i <= fs; i++) {
        double const x = (double)i;
        k[i + fs] = 1.0 / (ALWAN_SQRT_F64(2.0 * ALWAN_CC_PI) * sigma) * ALWAN_EXP_F64((x * x) / (-2.0 * sigma * sigma));
    }
    if (order == 0) {
        for (i = 0; i <= 2 * fs; i++) sum += k[i];
        for (i = 0; i <= 2 * fs; i++) k[i] /= sum;
    } else if (order == 1) {
        for (i = -fs; i <= fs; i++) k[i + fs] = -((double)i / (sigma * sigma)) * k[i + fs];
        for (i = -fs; i <= fs; i++) norm += (double)i * k[i + fs];
        for (i = 0; i <= 2 * fs; i++) k[i] /= norm;
    } else {
        double const s2 = sigma * sigma;
        for (i = -fs; i <= fs; i++) {
            double const x = (double)i;
            k[i + fs] = (x * x / (s2 * s2) - 1.0 / s2) * k[i + fs];
        }
        for (i = 0; i <= 2 * fs; i++) sum += k[i];
        for (i = 0; i <= 2 * fs; i++) k[i] -= sum / (double)(2 * fs + 1);
        for (i = -fs; i <= fs; i++) norm += 0.5 * (double)i * (double)i * k[i + fs];
        for (i = 0; i <= 2 * fs; i++) k[i] /= norm;
    }
}

static size_t alwan_cc_clamp(long v, size_t n) {
    if (v < 0) return 0;
    if ((size_t)v >= n) return n - 1;
    return (size_t)v;
}

/* Correlate plane with kx along x, then ky along y, the edge replicated. */
static void alwan_cc_filter(double *out, double const *in, double *tmp, size_t w, size_t h,
                            double const *kx, double const *ky, int fs) {
    size_t x, y;
    int j;
    for (y = 0; y < h; y++) {
        double const *row = in + y * w;
        for (x = 0; x < w; x++) {
            double acc = 0.0;
            for (j = -fs; j <= fs; j++) acc += kx[j + fs] * row[alwan_cc_clamp((long)x + j, w)];
            tmp[y * w + x] = acc;
        }
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double acc = 0.0;
            for (j = -fs; j <= fs; j++) acc += ky[j + fs] * tmp[alwan_cc_clamp((long)y + j, h) * w + x];
            out[y * w + x] = acc;
        }
    }
}

static alwan_status alwan_cc_check(alwan_constancy_params const *p, int *fs) {
    double const s = p->sigma, m = p->minkowski;
    if (p->order < 0 || p->order > 2) return ALWAN_E_INVALID;
    if (!alwan_cc_finite(m) || m < 0.0 || (m > 0.0 && m < 1.0)) return ALWAN_E_INVALID;
    if (!alwan_cc_finite(s) || s < 0.0) return ALWAN_E_INVALID;
    if (!alwan_cc_finite(p->saturation) || p->saturation < 0.0) return ALWAN_E_INVALID;
    *fs = (int)ALWAN_FLOOR_F64(3.0 * s + 0.5);
    if (*fs > (ALWAN_CC_TAPS_MAX - 1) / 2) return ALWAN_E_INVALID;
    if (p->order > 0 && *fs < 1) return ALWAN_E_INVALID;
    return ALWAN_OK;
}

/* img: w x h x 3 doubles, packed. use: w x h bytes, nonzero for a pixel that counts. */
static alwan_status alwan_cc_run(double out[3], double const *img, unsigned char const *use,
                                 size_t w, size_t h, alwan_constancy_params const *p, int fs) {
    double k0[ALWAN_CC_TAPS_MAX], k1[ALWAN_CC_TAPS_MAX], k2[ALWAN_CC_TAPS_MAX];
    size_t const n = w * h;
    size_t const planes = 3u + (p->order >= 1 ? 1u : 0u) + (p->order == 2 ? 1u : 0u);
    size_t const bytes = alwan_safe_array_size(n, planes * sizeof(double));
    double *pool, *plane, *tmp, *a, *b, *c;
    double norm2 = 0.0;
    size_t i;
    int ch;
    if (bytes == 0) return ALWAN_E_NOMEM;
    pool = (double *)ALWAN_ALLOC(bytes, sizeof(double));
    if (!pool) return ALWAN_E_NOMEM;
    plane = pool; tmp = pool + n; a = pool + 2 * n;
    b = p->order >= 1 ? pool + 3 * n : NULL;
    c = p->order == 2 ? pool + 4 * n : NULL;
    if (p->sigma > 0.0) {
        alwan_cc_kernel(k0, fs, p->sigma, 0);
        alwan_cc_kernel(k1, fs, p->sigma, 1);
        alwan_cc_kernel(k2, fs, p->sigma, 2);
    }
    for (ch = 0; ch < 3; ch++) {
        double acc = 0.0;
        for (i = 0; i < n; i++) plane[i] = img[i * 3 + (size_t)ch];
        if (p->order == 0) {
            if (p->sigma > 0.0) alwan_cc_filter(a, plane, tmp, w, h, k0, k0, fs);
            else for (i = 0; i < n; i++) a[i] = plane[i];
            for (i = 0; i < n; i++) a[i] = ALWAN_ABS_F64(a[i]);
        } else if (p->order == 1) {
            alwan_cc_filter(a, plane, tmp, w, h, k1, k0, fs);
            alwan_cc_filter(b, plane, tmp, w, h, k0, k1, fs);
            for (i = 0; i < n; i++) a[i] = ALWAN_SQRT_F64(a[i] * a[i] + b[i] * b[i]);
        } else {
            alwan_cc_filter(a, plane, tmp, w, h, k2, k0, fs);
            alwan_cc_filter(b, plane, tmp, w, h, k0, k2, fs);
            alwan_cc_filter(c, plane, tmp, w, h, k1, k1, fs);
            for (i = 0; i < n; i++) a[i] = ALWAN_SQRT_F64(a[i] * a[i] + 4.0 * c[i] * c[i] + b[i] * b[i]);
        }
        if (p->minkowski == 0.0) {
            for (i = 0; i < n; i++) if (use[i] && a[i] > acc) acc = a[i];
        } else if (p->minkowski == 1.0) {
            for (i = 0; i < n; i++) if (use[i]) acc += a[i];
        } else {
            for (i = 0; i < n; i++) if (use[i]) acc += ALWAN_POW_F64(a[i], p->minkowski);
            acc = ALWAN_POW_F64(acc, 1.0 / p->minkowski);
        }
        out[ch] = acc;
        norm2 += acc * acc;
    }
    ALWAN_FREE(pool);
    if (!(norm2 > 0.0) || !alwan_cc_finite(norm2)) return ALWAN_E_RANGE;
    norm2 = ALWAN_SQRT_F64(norm2);
    for (ch = 0; ch < 3; ch++) out[ch] /= norm2;
    return ALWAN_OK;
}

/* The pixels that count: inside the border, clear of saturation (dilated 3 x 3), and
 * not excluded by the caller. Returns how many. */
static size_t alwan_cc_mask(unsigned char *use, double const *img, size_t w, size_t h,
                            unsigned char const *exclude, size_t exclude_row_stride,
                            double sigma, double saturation) {
    double const border = sigma + 1.0;
    size_t x, y, count = 0;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double const cx = (double)(x + 1), cy = (double)(y + 1);
            int keep = cx > border && cx < (double)w - border + 1.0
                    && cy > border && cy < (double)h - border + 1.0;
            if (keep && exclude && exclude[y * exclude_row_stride + x]) keep = 0;
            if (keep && saturation > 0.0) {
                long dy, dx;
                for (dy = -1; dy <= 1 && keep; dy++) {
                    for (dx = -1; dx <= 1 && keep; dx++) {
                        size_t const yy = alwan_cc_clamp((long)y + dy, h), xx = alwan_cc_clamp((long)x + dx, w);
                        double const *px = img + (yy * w + xx) * 3;
                        double const mx = px[0] > px[1] ? (px[0] > px[2] ? px[0] : px[2]) : (px[1] > px[2] ? px[1] : px[2]);
                        if (mx >= saturation) keep = 0;
                    }
                }
            }
            use[y * w + x] = (unsigned char)keep;
            count += (size_t)keep;
        }
    }
    return count;
}

/* The shared body: rgb is f32 when is_f32, else f64. */
static alwan_status alwan_cc_estimate(double out[3], void const *rgb, int is_f32, size_t row_stride,
                                      size_t width, size_t height, unsigned char const *exclude,
                                      size_t exclude_row_stride, alwan_constancy_params const *params) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    double *img;
    unsigned char *use;
    size_t n, x, y, bytes;
    alwan_status st;
    int fs = 0;
    if (!rgb || !params) return ALWAN_E_INVALID;
    if (width == 0 || height == 0) return ALWAN_E_INVALID;
    if (row_stride / 3u / elem < width) return ALWAN_E_INVALID;
    if (exclude && exclude_row_stride < width) return ALWAN_E_INVALID;
    st = alwan_cc_check(params, &fs);
    if (st != ALWAN_OK) return st;
    n = width * height;
    if (n / width != height) return ALWAN_E_NOMEM;
    bytes = alwan_safe_array_size(n, 3 * sizeof(double) + 1);
    if (bytes == 0) return ALWAN_E_NOMEM;
    img = (double *)ALWAN_ALLOC(bytes, sizeof(double));
    if (!img) return ALWAN_E_NOMEM;
    use = (unsigned char *)(img + 3 * n);
    for (y = 0; y < height; y++) {
        char const *row = (char const *)rgb + y * row_stride;
        for (x = 0; x < width * 3u; x++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)row)[x] : (double)((alwan_f64 const *)row)[x];
            if (!alwan_cc_finite(v)) { ALWAN_FREE(img); return ALWAN_E_INVALID; }
            img[y * width * 3u + x] = v;
        }
    }
    if (alwan_cc_mask(use, img, width, height, exclude, exclude_row_stride,
                      params->sigma, params->saturation) == 0) {
        ALWAN_FREE(img);
        return ALWAN_E_RANGE;
    }
    st = alwan_cc_run(out, img, use, width, height, params, fs);
    ALWAN_FREE(img);
    return st;
}

/* The von Kries gains 1 / (e sqrt 3), or 0 when the illuminant is unusable. */
static int alwan_cc_gains(double g[3], double e0, double e1, double e2) {
    double const e[3] = { e0, e1, e2 };
    int c;
    for (c = 0; c < 3; c++) {
        if (!(e[c] > 0.0) || !alwan_cc_finite(e[c])) return 0;
        g[c] = e[c] * ALWAN_SQRT_F64(3.0);
    }
    return 1;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_illuminant_estimate_f64(alwan_f64 illuminant_out[3], alwan_f64 const *rgb,
                                           size_t row_stride, size_t width, size_t height,
                                           unsigned char const *exclude, size_t exclude_row_stride,
                                           alwan_constancy_params const *params) {
    double out[3];
    alwan_status st;
    if (!illuminant_out) return ALWAN_E_INVALID;
    st = alwan_cc_estimate(out, rgb, 0, row_stride, width, height, exclude, exclude_row_stride, params);
    if (st != ALWAN_OK) return st;
    illuminant_out[0] = (alwan_f64)out[0];
    illuminant_out[1] = (alwan_f64)out[1];
    illuminant_out[2] = (alwan_f64)out[2];
    return ALWAN_OK;
}

alwan_status alwan_illuminant_correct_f64_map_interleave(alwan_f64 *out, size_t out_stride,
                                                         alwan_f64 const *in, size_t in_stride,
                                                         size_t count, alwan_f64 const illuminant[3]) {
    double g[3];
    size_t i;
    int c;
    if (!out || !in || !illuminant) return ALWAN_E_INVALID;
    if (out_stride < 3 * sizeof(alwan_f64) || in_stride < 3 * sizeof(alwan_f64)) return ALWAN_E_INVALID;
    if (!alwan_cc_gains(g, (double)illuminant[0], (double)illuminant[1], (double)illuminant[2])) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        alwan_f64 const *s = (alwan_f64 const *)((char const *)in + i * in_stride);
        alwan_f64 *d = (alwan_f64 *)((char *)out + i * out_stride);
        for (c = 0; c < 3; c++) d[c] = (alwan_f64)((double)s[c] / g[c]);
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F32
alwan_status alwan_illuminant_estimate_f32(alwan_f32 illuminant_out[3], alwan_f32 const *rgb,
                                           size_t row_stride, size_t width, size_t height,
                                           unsigned char const *exclude, size_t exclude_row_stride,
                                           alwan_constancy_params const *params) {
    double out[3];
    alwan_status st;
    if (!illuminant_out) return ALWAN_E_INVALID;
    st = alwan_cc_estimate(out, rgb, 1, row_stride, width, height, exclude, exclude_row_stride, params);
    if (st != ALWAN_OK) return st;
    illuminant_out[0] = (alwan_f32)out[0];
    illuminant_out[1] = (alwan_f32)out[1];
    illuminant_out[2] = (alwan_f32)out[2];
    return ALWAN_OK;
}

alwan_status alwan_illuminant_correct_f32_map_interleave(alwan_f32 *out, size_t out_stride,
                                                         alwan_f32 const *in, size_t in_stride,
                                                         size_t count, alwan_f32 const illuminant[3]) {
    double g[3];
    size_t i;
    int c;
    if (!out || !in || !illuminant) return ALWAN_E_INVALID;
    if (out_stride < 3 * sizeof(alwan_f32) || in_stride < 3 * sizeof(alwan_f32)) return ALWAN_E_INVALID;
    if (!alwan_cc_gains(g, (double)illuminant[0], (double)illuminant[1], (double)illuminant[2])) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        alwan_f32 const *s = (alwan_f32 const *)((char const *)in + i * in_stride);
        alwan_f32 *d = (alwan_f32 *)((char *)out + i * out_stride);
        for (c = 0; c < 3; c++) d[c] = (alwan_f32)((double)s[c] / g[c]);
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */
