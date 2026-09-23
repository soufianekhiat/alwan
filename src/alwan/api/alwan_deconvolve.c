/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_deconvolve_{T}: an image blurred by a known point-spread function, sharpened back.
 * Each channel is deconvolved on its own with the same PSF. Both methods follow
 * scikit-image's restoration module, which suite 214 holds them to.
 *
 * WIENER, restoration.wiener with its default Laplacian regulariser: the image is taken as
 * periodic and filtered in the 2D DFT domain by
 *
 *   W = conj(H) / (|H|^2 + balance |L|^2)
 *
 * H the transform of the PSF and L that of the discrete Laplacian [[0,-1,0],[-1,4,-1],
 * [0,-1,0]], each placed with its element (floor(h / 2), floor(w / 2)) at the origin
 * (scikit-image's ir2tf). balance trades sharpness against the noise the inverse filter
 * amplifies. scikit-image's unitary transforms cancel, so the result is F^-1(W F(image)).
 *
 * RICHARDSON_LUCY, restoration.richardson_lucy: Richardson (1972) and Lucy (1974), the
 * maximum-likelihood estimate under Poisson noise, by the multiplicative iteration
 *
 *   u = 0.5 everywhere;  u *= (image / (u * psf + 1e-12)) * flip(psf)
 *
 * repeated `iterations` times, the convolutions scipy.signal.convolve's mode 'same' (zero
 * outside the image, the output centred at (P - 1) / 2). With filter_epsilon above 0 the
 * ratio is taken as 0 wherever the blurred estimate is below it. The iteration keeps
 * values non-negative for a non-negative image and PSF, and conserves flux only for a PSF
 * that sums to 1.
 *
 * scikit-image clips both results to [-1, 1] unless told not to; alwan clips only when
 * params.clip asks.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

static int alwan_dc_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

static double alwan_dc_read(void const *base, size_t rs, size_t x, size_t y, size_t ch, size_t c, int is_f32) {
    char const *row = (char const *)base + y * rs;
    return is_f32 ? (double)((alwan_f32 const *)row)[x * ch + c] : ((alwan_f64 const *)row)[x * ch + c];
}

/* The 2D transform of an h x w complex plane, rows then columns, in place. */
static void alwan_dc_fft2(alwan__fft const *fw, alwan__fft const *fh, double *re, double *im, size_t w, size_t h,
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

/* scikit-image's ir2tf: a kh x kw kernel on an h x w zero plane, element (kh / 2, kw / 2) at
 * the origin, transformed. */
static void alwan_dc_ir2tf(double *re, double *im, double const *k, size_t kw, size_t kh, size_t w, size_t h,
                           alwan__fft const *fw, alwan__fft const *fh, double *cre, double *cim) {
    size_t x, y;
    for (x = 0; x < w * h; x++) re[x] = im[x] = 0.0;
    for (y = 0; y < kh; y++) {
        size_t const ty = (y + h - kh / 2) % h;
        for (x = 0; x < kw; x++) re[ty * w + (x + w - kw / 2) % w] = k[y * kw + x];
    }
    alwan_dc_fft2(fw, fh, re, im, w, h, cre, cim, 0);
}

static alwan_status alwan_dc_wiener(double *img, size_t ch, size_t w, size_t h, double const *psf, size_t pw, size_t ph,
                                    double balance) {
    static double const lap[9] = { 0, -1, 0, -1, 4, -1, 0, -1, 0 };
    size_t const n = w * h;
    alwan__fft *fw, *fh;
    double *mem, *Wre, *Wim, *Lre, *Lim, *re, *im, *cre, *cim;
    size_t c, i;
    if (w < 3 || h < 3) return ALWAN_E_INVALID;   /* the Laplacian is 3 x 3 */
    mem = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 6 * sizeof(double)) + 2 * h * sizeof(double), sizeof(double));
    fw = alwan__fft_create(w);
    fh = alwan__fft_create(h);
    if (!mem || !fw || !fh) {
        if (mem) ALWAN_FREE(mem);
        alwan__fft_destroy(fw);
        alwan__fft_destroy(fh);
        return ALWAN_E_NOMEM;
    }
    Wre = mem; Wim = Wre + n; Lre = Wim + n; Lim = Lre + n; re = Lim + n; im = re + n; cre = im + n; cim = cre + h;
    alwan_dc_ir2tf(Wre, Wim, psf, pw, ph, w, h, fw, fh, cre, cim);
    alwan_dc_ir2tf(Lre, Lim, lap, 3, 3, w, h, fw, fh, cre, cim);
    for (i = 0; i < n; i++) {
        double const hr = Wre[i], hi = Wim[i];
        double const d = hr * hr + hi * hi + balance * (Lre[i] * Lre[i] + Lim[i] * Lim[i]);
        Wre[i] = d > 0.0 ? hr / d : 0.0;
        Wim[i] = d > 0.0 ? -hi / d : 0.0;
    }
    for (c = 0; c < ch; c++) {
        double *p = img + c * n;
        for (i = 0; i < n; i++) {
            re[i] = p[i];
            im[i] = 0.0;
        }
        alwan_dc_fft2(fw, fh, re, im, w, h, cre, cim, 0);
        for (i = 0; i < n; i++) {
            double const r = re[i] * Wre[i] - im[i] * Wim[i], q = re[i] * Wim[i] + im[i] * Wre[i];
            re[i] = r;
            im[i] = q;
        }
        alwan_dc_fft2(fw, fh, re, im, w, h, cre, cim, 1);
        for (i = 0; i < n; i++) p[i] = re[i] / (double)n;
    }
    ALWAN_FREE(mem);
    alwan__fft_destroy(fw);
    alwan__fft_destroy(fh);
    return ALWAN_OK;
}

/* scipy.signal.convolve(x, k, mode='same'): zero outside, output centred at (P - 1) / 2. */
static void alwan_dc_conv_same(double *out, double const *x, size_t w, size_t h, double const *k, size_t kw, size_t kh) {
    long const oy = (long)((kh - 1) / 2), ox = (long)((kw - 1) / 2);
    size_t xx, yy, a, b;
    for (yy = 0; yy < h; yy++) {
        for (xx = 0; xx < w; xx++) {
            double s = 0.0;
            for (b = 0; b < kh; b++) {
                long const sy = (long)yy + oy - (long)b;
                if (sy < 0 || sy >= (long)h) continue;
                for (a = 0; a < kw; a++) {
                    long const sx = (long)xx + ox - (long)a;
                    if (sx < 0 || sx >= (long)w) continue;
                    s += k[b * kw + a] * x[(size_t)sy * w + (size_t)sx];
                }
            }
            out[yy * w + xx] = s;
        }
    }
}

static alwan_status alwan_dc_rl(double *img, size_t ch, size_t w, size_t h, double const *psf, size_t pw, size_t ph,
                                size_t iterations, double filter_epsilon) {
    size_t const n = w * h, np = pw * ph;
    double *mem, *u, *conv, *back, *mirror;
    size_t c, i, it;
    mem = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 3 * sizeof(double)) + np * sizeof(double), sizeof(double));
    if (!mem) return ALWAN_E_NOMEM;
    u = mem;
    conv = u + n;
    back = conv + n;
    mirror = back + n;
    for (i = 0; i < np; i++) mirror[i] = psf[np - 1 - i];   /* flipped on both axes */
    for (c = 0; c < ch; c++) {
        double *p = img + c * n;
        for (i = 0; i < n; i++) u[i] = 0.5;
        for (it = 0; it < iterations; it++) {
            alwan_dc_conv_same(conv, u, w, h, psf, pw, ph);
            for (i = 0; i < n; i++) {
                double const d = conv[i] + 1e-12;
                conv[i] = filter_epsilon > 0.0 && d < filter_epsilon ? 0.0 : p[i] / d;
            }
            alwan_dc_conv_same(back, conv, w, h, mirror, pw, ph);
            for (i = 0; i < n; i++) u[i] *= back[i];
        }
        for (i = 0; i < n; i++) p[i] = u[i];
    }
    ALWAN_FREE(mem);
    return ALWAN_OK;
}

static alwan_status alwan_dc_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 void const *psf, size_t pw, size_t ph, alwan_deconvolve_method method,
                                 alwan_deconvolve_params const *params, int is_f32) {
    alwan_deconvolve_params const zero = { 0 };
    alwan_deconvolve_params const *p = params ? params : &zero;
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    double *img, *k;
    double const balance = p->balance == 0.0 ? 0.1 : p->balance;
    size_t const iterations = p->iterations == 0 ? 50 : p->iterations;
    alwan_status st;
    size_t c, x, y;
    if (!out || !src || !psf || w == 0 || h == 0 || ch == 0 || ch > 4 || pw == 0 || ph == 0 || n / w != h) {
        return ALWAN_E_INVALID;
    }
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if (method != ALWAN_DECONVOLVE_WIENER && method != ALWAN_DECONVOLVE_RICHARDSON_LUCY) return ALWAN_E_INVALID;
    if (pw > w || ph > h) return ALWAN_E_RANGE;
    if (!alwan_dc_finite(balance) || balance < 0.0 || !alwan_dc_finite(p->filter_epsilon) || p->filter_epsilon < 0.0) {
        return ALWAN_E_RANGE;
    }
    img = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, ch * sizeof(double)) + pw * ph * sizeof(double), sizeof(double));
    if (!img) return ALWAN_E_NOMEM;
    k = img + ch * n;
    for (y = 0; y < ph; y++) {
        for (x = 0; x < pw; x++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)psf)[y * pw + x] : ((alwan_f64 const *)psf)[y * pw + x];
            if (!alwan_dc_finite(v)) {
                ALWAN_FREE(img);
                return ALWAN_E_INVALID;
            }
            k[y * pw + x] = v;
        }
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double const v = alwan_dc_read(src, src_rs, x, y, ch, c, is_f32);
                if (!alwan_dc_finite(v)) {
                    ALWAN_FREE(img);
                    return ALWAN_E_INVALID;
                }
                img[c * n + y * w + x] = v;
            }
        }
    }
    st = method == ALWAN_DECONVOLVE_WIENER ? alwan_dc_wiener(img, ch, w, h, k, pw, ph, balance)
                                           : alwan_dc_rl(img, ch, w, h, k, pw, ph, iterations, p->filter_epsilon);
    if (st == ALWAN_OK) {
        for (y = 0; y < h; y++) {
            char *row = (char *)out + y * out_rs;
            for (x = 0; x < w; x++) {
                for (c = 0; c < ch; c++) {
                    double v = img[c * n + y * w + x];
                    if (p->clip) v = v > 1.0 ? 1.0 : v < -1.0 ? -1.0 : v;
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
alwan_status alwan_deconvolve_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                                  size_t channels, size_t width, size_t height, alwan_f64 const *psf, size_t psf_width,
                                  size_t psf_height, alwan_deconvolve_method method,
                                  alwan_deconvolve_params const *params) {
    return alwan_dc_run(out, out_row_stride, src, src_row_stride, channels, width, height, psf, psf_width, psf_height,
                        method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_deconvolve_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                                  size_t channels, size_t width, size_t height, alwan_f32 const *psf, size_t psf_width,
                                  size_t psf_height, alwan_deconvolve_method method,
                                  alwan_deconvolve_params const *params) {
    return alwan_dc_run(out, out_row_stride, src, src_row_stride, channels, width, height, psf, psf_width, psf_height,
                        method, params, 1);
}
#endif
