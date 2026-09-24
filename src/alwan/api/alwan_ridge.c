/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_ridge_{T}: how strongly each pixel of one channel lies on a ridge (a line, a vessel,
 * a crease, a fibre), from the eigenvalues of the Hessian at each of several scales, the
 * largest answer over the scales kept, as scikit-image's filters.frangi, sato, meijering
 * and hessian compute it, which suite 232 holds this to.
 *
 * The Hessian is scikit-image's _hessian_matrix_with_gaussian: two first-order gaussian
 * derivatives, each scipy's gaussian_filter at sigma / sqrt(2) with the derivative
 * kernel x (1 / -sigma^2) phi(x) along one axis and phi along the other, truncated at 8
 * (100 at a sigma of 1 or less) standard deviations, the image extended by scipy's
 * "reflect"; every pass kept in the data's precision. Its eigenvalues come in closed form,
 * (Hrr + Hcc) / 2 +- sqrt(Hrc^2 + ((Hrr - Hcc) / 2)^2), the larger first. From them:
 *
 *   FRANGI     exp(-rb^2 / 2 beta^2) (1 - exp(-s^2 / 2 gamma^2)), rb the smaller
 *              eigenvalue's magnitude over the larger (floored at 1e-10), s their norm,
 *              gamma half the largest s at the first scale unless given
 *   SATO       sigma^2 max(larger eigenvalue, 0)
 *   MEIJERING  of l1 + alpha l2 and l2 + alpha l1, the one larger in magnitude, floored
 *              at 0, over its maximum at that scale
 *   HESSIAN    FRANGI with gamma 15, and 1 where it is 0
 *
 * Each of them looks for dark ridges on a light ground, where the Hessian across the ridge
 * is positive; bright_ridges negates the image first, as scikit-image's black_ridges=False does.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

static double alwan_rd_r(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

static double alwan_rd_sum(double const *a, size_t n) {
    double r[8], res = 0.0;
    size_t i, j;
    if (n < 8) {
        for (i = 0; i < n; i++) res += a[i];
        return res;
    }
    if (n > 128) {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan_rd_sum(a, n2) + alwan_rd_sum(a + n2, n - n2);
    }
    for (j = 0; j < 8; j++) r[j] = a[j];
    for (i = 8; i < n - (n % 8); i += 8)
        for (j = 0; j < 8; j++) r[j] += a[i + j];
    res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    for (; i < n; i++) res += a[i];
    return res;
}

/* scipy's _gaussian_kernel1d for order 0 (k0) and order 1 (k1), radius lw, reversed as
 * gaussian_filter1d reverses them (phi is symmetric; the derivative flips its sign). */
static void alwan_rd_kernels(double *k0, double *k1, double sigma, size_t lw) {
    double const s2 = sigma * sigma, a = -0.5 / s2, p = 1.0 / -s2;
    double s;
    size_t i, n = 2 * lw + 1;
    for (i = 0; i < n; i++) {
        long const x = (long)i - (long)lw;
        k0[i] = exp(a * (double)(x * x));
    }
    s = alwan_rd_sum(k0, n);
    for (i = 0; i < n; i++) k0[i] = k0[i] / s;
    for (i = 0; i < n; i++) {
        long const x = (long)i - (long)lw;
        k1[n - 1 - i] = (0.0 + (double)x * p) * k0[i];   /* reversed; numpy forms 1 * 0 + x * p */
    }
}

/* correlate1d, scipy's "reflect", symmetric (1) or antisymmetric (-1) kernel wts. */
static void alwan_rd_corr(double *out, double const *in, size_t count, size_t len, size_t lstep, size_t istep, double const *wts, size_t half,
                          int sym, int f32, double *ext) {
    size_t l, i;
    long j;
    for (l = 0; l < count; l++) {
        for (j = -(long)half; j < (long)(len + half); j++) {
            long const p = 2 * (long)len;
            long m = j % p;
            if (m < 0) m += p;
            ext[j + (long)half] = in[l * lstep + (size_t)(m >= (long)len ? p - 1 - m : m) * istep];
        }
        for (i = 0; i < len; i++) {
            double const *c = ext + half + i;
            double acc = c[0] * wts[half];
            for (j = -(long)half; j < 0; j++) acc += (sym > 0 ? c[j] + c[-j] : c[j] - c[-j]) * wts[(long)half + j];
            out[l * lstep + i * istep] = alwan_rd_r(acc, f32);
        }
    }
}

/* gaussian_filter with order o0 along the rows' axis, then o1 along the columns'. */
static void alwan_rd_gauss(double *out, double const *in, double *tmp, size_t w, size_t h, double const *k0, double const *k1, size_t lw,
                           int o0, int o1, int f32, double *ext) {
    alwan_rd_corr(tmp, in, w, h, 1, w, o0 ? k1 : k0, lw, o0 ? -1 : 1, f32, ext);
    alwan_rd_corr(out, tmp, h, w, w, 1, o1 ? k1 : k0, lw, o1 ? -1 : 1, f32, ext);
}

static alwan_status alwan_rd_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_ridge_method method, alwan_ridge_params const *params, int f32) {
    static double const dflt_sigmas[5] = { 1.0, 3.0, 5.0, 7.0, 9.0 };
    alwan_ridge_params const zero = { 0 };
    alwan_ridge_params const *p = params ? params : &zero;
    size_t const elem = f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    double const *sigmas = p->sigmas ? p->sigmas : dflt_sigmas;
    size_t const ns = p->sigmas ? p->sigma_count : 5;
    double const alpha = p->alpha > 0.0 ? p->alpha : (method == ALWAN_RIDGE_MEIJERING ? 1.0 / 3.0 : 0.5);
    double const beta = p->beta > 0.0 ? p->beta : 0.5;
    double gamma = p->gamma > 0.0 ? p->gamma : (method == ALWAN_RIDGE_HESSIAN ? 15.0 : 0.0);
    double *buf, *img, *gr, *gc, *hrr, *hrc, *hcc, *tmp, *best, *vals, *ext, *k0, *k1;
    size_t lwmax = 0, x, y, i, k, extn;
    if (!out || !src || w == 0 || h == 0 || ch != 1 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem < w || out_rs / elem < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_RIDGE_HESSIAN || (p->sigmas && p->sigma_count == 0)) return ALWAN_E_INVALID;
    for (k = 0; k < ns; k++) {
        double const tr = sigmas[k] > 1.0 ? 8.0 : 100.0;
        size_t lw;
        if (!(sigmas[k] > 0.0) || sigmas[k] > 64.0) return ALWAN_E_RANGE;
        lw = (size_t)(tr * ((1.0 / sqrt(2.0)) * sigmas[k]) + 0.5);
        if (lw > lwmax) lwmax = lw;
    }
    if (!(p->alpha >= 0.0) || !(p->beta >= 0.0) || !(p->gamma >= 0.0)) return ALWAN_E_RANGE;
    extn = (w > h ? w : h) + 2 * lwmax + 2;
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(9 * n + extn + 2 * (2 * lwmax + 1), sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    img = buf, gr = img + n, gc = gr + n, hrr = gc + n, hrc = hrr + n, hcc = hrc + n, tmp = hcc + n, best = tmp + n, vals = best + n;
    ext = vals + n;
    k0 = ext + extn;
    k1 = k0 + 2 * lwmax + 1;
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            double const v = f32 ? (double)((alwan_f32 const *)row)[x] : ((alwan_f64 const *)row)[x];
            if (!(v - v == 0.0)) {
                ALWAN_FREE(buf);
                return ALWAN_E_INVALID;
            }
            img[y * w + x] = p->bright_ridges ? -v : v;
        }
    }
    for (i = 0; i < n; i++) best[i] = 0.0;
    for (k = 0; k < ns; k++) {
        double const sig = sigmas[k], sd = (1.0 / sqrt(2.0)) * sig, tr = sig > 1.0 ? 8.0 : 100.0;
        size_t const lw = (size_t)(tr * sd + 0.5);
        double smax = 0.0, vmax = 0.0;
        alwan_rd_kernels(k0, k1, sd, lw);
        alwan_rd_gauss(gr, img, tmp, w, h, k0, k1, lw, 1, 0, f32, ext);
        alwan_rd_gauss(gc, img, tmp, w, h, k0, k1, lw, 0, 1, f32, ext);
        alwan_rd_gauss(hrr, gr, tmp, w, h, k0, k1, lw, 1, 0, f32, ext);
        alwan_rd_gauss(hrc, gr, tmp, w, h, k0, k1, lw, 0, 1, f32, ext);
        alwan_rd_gauss(hcc, gc, tmp, w, h, k0, k1, lw, 0, 1, f32, ext);
        /* eigenvalues, the larger in hrr's place and the smaller in hcc's */
        for (i = 0; i < n; i++) {
            double const half = alwan_rd_r(alwan_rd_r(hrr[i] + hcc[i], f32) / 2.0, f32);
            double const dh = alwan_rd_r(alwan_rd_r(hrr[i] - hcc[i], f32) / 2.0, f32);
            double const q = alwan_rd_r(alwan_rd_r(hrc[i] * hrc[i], f32) + alwan_rd_r(dh * dh, f32), f32);
            double const r = alwan_rd_r(sqrt(q), f32);
            hrr[i] = alwan_rd_r(half + r, f32);
            hcc[i] = alwan_rd_r(half - r, f32);
        }
        if (method == ALWAN_RIDGE_FRANGI || method == ALWAN_RIDGE_HESSIAN) {
            double const floor_ = alwan_rd_r(1e-10, f32), b2 = alwan_rd_r(2.0 * beta * beta, f32);
            /* s, then gamma at the first scale when not given */
            for (i = 0; i < n; i++) {
                double const e0 = hrr[i], e1 = hcc[i];
                double const l1 = fabs(e0) <= fabs(e1) ? e0 : e1, l2 = fabs(e0) <= fabs(e1) ? e1 : e0;
                double const s = alwan_rd_r(sqrt(alwan_rd_r(alwan_rd_r(l1 * l1, f32) + alwan_rd_r(l2 * l2, f32), f32)), f32);
                tmp[i] = s;
                if (s > smax) smax = s;
            }
            if (gamma == 0.0) {
                gamma = alwan_rd_r(smax / 2.0, f32);
                if (gamma == 0.0) gamma = 1.0;
            }
            {
                double const g2 = alwan_rd_r(2.0 * alwan_rd_r(gamma * gamma, f32), f32);
                for (i = 0; i < n; i++) {
                    double const e0 = hrr[i], e1 = hcc[i];
                    double const l1 = fabs(e0) <= fabs(e1) ? e0 : e1, l2r = fabs(e0) <= fabs(e1) ? e1 : e0;
                    double const l2 = l2r > floor_ ? l2r : floor_;
                    double const rb = alwan_rd_r(fabs(l1) / l2, f32), s = tmp[i];
                    double const eb = f32 ? (double)expf((float)alwan_rd_r(-alwan_rd_r(rb * rb, f32) / b2, f32))
                                          : exp(-(rb * rb) / b2);
                    double const es = f32 ? (double)expf((float)alwan_rd_r(-alwan_rd_r(s * s, f32) / g2, f32)) : exp(-(s * s) / g2);
                    double const v = alwan_rd_r(eb * alwan_rd_r(1.0 - es, f32), f32);
                    if (v > best[i]) best[i] = v;
                }
            }
        } else if (method == ALWAN_RIDGE_SATO) {
            double const s2 = alwan_rd_r(sig * sig, f32);
            for (i = 0; i < n; i++) {
                double const v = alwan_rd_r(s2 * (hrr[i] > 0.0 ? hrr[i] : 0.0), f32);
                if (v > best[i]) best[i] = v;
            }
        } else {   /* MEIJERING */
            double const a = alwan_rd_r(alpha, f32);
            for (i = 0; i < n; i++) {
                double const v0 = alwan_rd_r(hrr[i] + alwan_rd_r(a * hcc[i], f32), f32);
                double const v1 = alwan_rd_r(alwan_rd_r(a * hrr[i], f32) + hcc[i], f32);
                double v = fabs(v1) > fabs(v0) ? v1 : v0;
                v = v > 0.0 ? v : 0.0;
                vals[i] = v;
                if (v > vmax) vmax = v;
            }
            for (i = 0; i < n; i++) {
                double const v = vmax > 0.0 ? alwan_rd_r(vals[i] / vmax, f32) : vals[i];
                if (v > best[i]) best[i] = v;
            }
        }
    }
    if (method == ALWAN_RIDGE_HESSIAN)
        for (i = 0; i < n; i++)
            if (best[i] <= 0.0) best[i] = 1.0;
    for (y = 0; y < h; y++) {
        char *row = (char *)out + y * out_rs;
        for (x = 0; x < w; x++) {
            if (f32) ((alwan_f32 *)row)[x] = (alwan_f32)best[y * w + x];
            else ((alwan_f64 *)row)[x] = best[y * w + x];
        }
    }
    ALWAN_FREE(buf);
    return ALWAN_OK;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_ridge_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels, size_t width,
                             size_t height, alwan_ridge_method method, alwan_ridge_params const *params) {
    return alwan_rd_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_ridge_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels, size_t width,
                             size_t height, alwan_ridge_method method, alwan_ridge_params const *params) {
    return alwan_rd_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
