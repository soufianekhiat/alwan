/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_corner_response_{T}: a corner strength for every pixel of one channel, as
 * scikit-image's feature.corner_harris, corner_shi_tomasi, corner_kitchen_rosenfeld and
 * corner_foerstner compute it, which suite 229 holds this to.
 *
 * The structure tensor is scikit-image's: the derivatives are scipy's ndimage.sobel down
 * the rows (r) and across the columns (c) with a zero edge, and Arr, Arc, Acc are their
 * products each smoothed by scipy's gaussian (truncated at 4 sigma, zero edge), every pass
 * kept in the data's precision. From it:
 *
 *   HARRIS             det - k trace^2, or with harris_normalised 2 det / (trace + eps)
 *   SHI_TOMASI         the smaller eigenvalue, ((Arr + Acc) - sqrt((Arr - Acc)^2 + 4 Arc^2)) / 2
 *   KITCHEN_ROSENFELD  (Ixx Iy^2 + Iyy Ix^2 - 2 Ixy Ix Iy) / (Ix^2 + Iy^2), from the Sobel
 *                      derivatives and their own Sobel derivatives, 0 where the gradient is
 *   FOERSTNER          w = det / trace (component 0) or q = 4 det / trace^2 (component 1),
 *                      0 where the trace is
 *
 * Each expression is evaluated in scikit-image's order, a Python number rounded to the
 * data's precision first, as numpy rounds it.
 *
 * MORAVEC and FAST are scikit-image's Cython loops (feature/corner_cy.pyx, _corner_moravec
 * and _corner_fast, BSD-3-Clause, Copyright the scikit-image team), ported loop for loop
 * with every sum kept in the data's precision as theirs is: Moravec's patch differences
 * summed in order with the minimum starting at DBL_MAX (infinity in float32), FAST's ring
 * classified against centre -/+ threshold, the run scanned over 15 + n positions, the
 * response the sum of the 16 absolute differences.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

static double alwan_cn_r(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

static double alwan_cn_sum(double const *a, size_t n) {
    double r[8], res = 0.0;
    size_t i, j;
    if (n < 8) {
        for (i = 0; i < n; i++) res += a[i];
        return res;
    }
    if (n > 128) {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan_cn_sum(a, n2) + alwan_cn_sum(a + n2, n - n2);
    }
    for (j = 0; j < 8; j++) r[j] = a[j];
    for (i = 8; i < n - (n % 8); i += 8)
        for (j = 0; j < 8; j++) r[j] += a[i + j];
    res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    for (; i < n; i++) res += a[i];
    return res;
}

/* scipy's correlate1d with a symmetric (1) or antisymmetric (-1) kernel of 2 half + 1
 * taps and a zero edge, along lines of len (count of them, steps lstep and istep). */
static void alwan_cn_corr(double *out, double const *in, size_t count, size_t len, size_t lstep, size_t istep, double const *wts, size_t half,
                          int sym, int f32, double *ext) {
    size_t l, i;
    long j;
    for (l = 0; l < count; l++) {
        for (j = -(long)half; j < (long)(len + half); j++)
            ext[j + (long)half] = (j >= 0 && j < (long)len) ? in[l * lstep + (size_t)j * istep] : 0.0;
        for (i = 0; i < len; i++) {
            double const *c = ext + half + i;
            double acc = c[0] * wts[half];
            for (j = -(long)half; j < 0; j++) acc += (sym > 0 ? c[j] + c[-j] : c[j] - c[-j]) * wts[(long)half + j];
            out[l * lstep + i * istep] = alwan_cn_r(acc, f32);
        }
    }
}

/* ndimage.sobel along axis (0 rows, 1 columns), zero edge. */
static void alwan_cn_sobel(double *out, double const *in, double *tmp, size_t w, size_t h, int axis, int f32, double *ext) {
    static double const d3[3] = { -1.0, 0.0, 1.0 }, s3[3] = { 1.0, 2.0, 1.0 };
    if (axis == 0) {
        alwan_cn_corr(tmp, in, w, h, 1, w, d3, 1, -1, f32, ext);
        alwan_cn_corr(out, tmp, h, w, w, 1, s3, 1, 1, f32, ext);
    } else {
        alwan_cn_corr(tmp, in, h, w, w, 1, d3, 1, -1, f32, ext);
        alwan_cn_corr(out, tmp, w, h, 1, w, s3, 1, 1, f32, ext);
    }
}

static void alwan_cn_gauss(double *out, double const *in, double *tmp, size_t w, size_t h, double const *wts, size_t lw, double sigma,
                           int f32, double *ext) {
    if (!(sigma > 1e-15)) {
        memcpy(out, in, w * h * sizeof(double));
        return;
    }
    alwan_cn_corr(tmp, in, w, h, 1, w, wts, lw, 1, f32, ext);
    alwan_cn_corr(out, tmp, h, w, w, 1, wts, lw, 1, f32, ext);
}

/* scikit-image's _corner_moravec: out is zero outside [2 ws, size - 2 ws). */
static void alwan_cn_moravec(double *res, double const *img, size_t w, size_t h, size_t s, int f32) {
    size_t r, c, a, b, u, v;
    memset(res, 0, w * h * sizeof(double));
    /* rows and columns from 2 s to size - 2 s - 1; the shifts br = r - s + a, the patch
     * offsets r - s + u, in scikit-image's order */
    for (r = 2 * s; r + 2 * s < h; r++) {
        for (c = 2 * s; c + 2 * s < w; c++) {
            double mn = f32 ? (double)(float)DBL_MAX : DBL_MAX;   /* float32: inf */
            for (a = 0; a <= 2 * s; a++) {
                size_t const br = r - s + a;
                for (b = 0; b <= 2 * s; b++) {
                    size_t const bc = c - s + b;
                    if (br != r && bc != c) {
                        double msum = 0.0;
                        for (u = 0; u <= 2 * s; u++)
                            for (v = 0; v <= 2 * s; v++) {
                                double const t =
                                    alwan_cn_r(img[(r - s + u) * w + (c - s + v)] - img[(br - s + u) * w + (bc - s + v)], f32);
                                msum = alwan_cn_r(msum + alwan_cn_r(t * t, f32), f32);
                            }
                        if (msum < mn) mn = msum;
                    }
                }
            }
            res[r * w + c] = mn;
        }
    }
}

/* scikit-image's _corner_fast_response: the response when n consecutive ring pixels (with
 * wrap-around) are in state st, else 0. */
static double alwan_cn_fast_response(double cp, double const *ring, signed char const *bins, signed char st, int n, int f32) {
    int count = 0, l, m;
    for (l = 0; l < 15 + n; l++) {
        if (bins[l % 16] == st) {
            if (++count == n) {
                double r = 0.0;
                for (m = 0; m < 16; m++) r = alwan_cn_r(r + ALWAN_ABS(alwan_cn_r(ring[m] - cp, f32)), f32);
                return r;
            }
        } else {
            count = 0;
        }
    }
    return 0.0;
}

static void alwan_cn_fast(double *res, double const *img, size_t w, size_t h, int n, double thr, int f32) {
    /* scikit-image's ring offsets rp, cp, here plus 3 so that they index from i - 3 */
    static unsigned char const rp[16] = { 3, 4, 5, 6, 6, 6, 5, 4, 3, 2, 1, 0, 0, 0, 1, 2 };
    static unsigned char const cp[16] = { 6, 6, 5, 4, 3, 2, 1, 0, 0, 0, 1, 2, 3, 4, 5, 6 };
    size_t i, j;
    int k;
    memset(res, 0, w * h * sizeof(double));
    for (i = 3; i + 3 < h; i++) {
        for (j = 3; j + 3 < w; j++) {
            double const cur = img[i * w + j];
            double const lo = alwan_cn_r(cur - thr, f32), hi = alwan_cn_r(cur + thr, f32);
            double ring[16], r;
            signed char bins[16];
            if (n >= 12) {
                int sb = 0, sd = 0;
                for (k = 0; k < 16; k += 4) {
                    double const v = img[(i - 3 + rp[k]) * w + (j - 3 + cp[k])];
                    if (v > hi) sb++;
                    else if (v < lo) sd++;
                }
                if (sd < 3 && sb < 3) continue;
            }
            for (k = 0; k < 16; k++) {
                ring[k] = img[(i - 3 + rp[k]) * w + (j - 3 + cp[k])];
                bins[k] = ring[k] > hi ? 'b' : ring[k] < lo ? 'd' : 's';
            }
            r = alwan_cn_fast_response(cur, ring, bins, 'b', n, f32);
            if (r == 0.0) r = alwan_cn_fast_response(cur, ring, bins, 'd', n, f32);
            res[i * w + j] = r;
        }
    }
}

static alwan_status alwan_cn_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_corner_method method, alwan_corner_params const *params, int f32) {
    alwan_corner_params const zero = { 0 };
    alwan_corner_params const *p = params ? params : &zero;
    size_t const elem = f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    double const sigma = p->sigma > 0.0 ? p->sigma : 1.0;
    double const k = alwan_cn_r(p->k != 0.0 ? p->k : 0.05, f32);
    double const eps = alwan_cn_r(p->eps > 0.0 ? p->eps : 1e-6, f32);
    size_t const lw = (size_t)(4.0 * sigma + 0.5), extn = (w > h ? w : h) + 2 * (lw > 1 ? lw : 1) + 2;
    double *buf, *img, *iy, *ix, *a, *b, *c, *t, *u, *ext, *wts, *res;
    size_t x, y, i;
    if (!out || !src || w == 0 || h == 0 || ch != 1 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem < w || out_rs / elem < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_CORNER_FAST || p->component < 0 || p->component > 1) return ALWAN_E_INVALID;
    if (!(p->sigma >= 0.0) || p->sigma > 64.0 || !(p->k == p->k) || !(p->eps >= 0.0)) return ALWAN_E_RANGE;
    if (p->window_size > 64 || p->fast_n > 16 || !(p->fast_threshold >= 0.0)) return ALWAN_E_RANGE;
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(9 * n + extn + 2 * lw + 1, sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    img = buf, iy = img + n, ix = iy + n, a = ix + n, b = a + n, c = b + n, t = c + n, u = t + n, res = u + n, ext = res + n;
    wts = ext + extn;
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            double const v = f32 ? (double)((alwan_f32 const *)row)[x] : ((alwan_f64 const *)row)[x];
            if (!(v - v == 0.0)) {
                ALWAN_FREE(buf);
                return ALWAN_E_INVALID;
            }
            img[y * w + x] = v;
        }
    }
    if (method == ALWAN_CORNER_MORAVEC) {
        alwan_cn_moravec(res, img, w, h, p->window_size ? p->window_size : 1, f32);
    } else if (method == ALWAN_CORNER_FAST) {
        alwan_cn_fast(res, img, w, h, p->fast_n ? (int)p->fast_n : 12,
                      alwan_cn_r(p->fast_threshold > 0.0 ? p->fast_threshold : 0.15, f32), f32);
    } else {
        alwan_cn_sobel(iy, img, t, w, h, 0, f32, ext);
        alwan_cn_sobel(ix, img, t, w, h, 1, f32, ext);
        if (method == ALWAN_CORNER_KITCHEN_ROSENFELD) {
            /* imxy, imxx from imx; imyy from imy (a, b, c) */
            alwan_cn_sobel(a, ix, t, w, h, 0, f32, ext);   /* imxy */
            alwan_cn_sobel(b, ix, t, w, h, 1, f32, ext);   /* imxx */
            alwan_cn_sobel(c, iy, t, w, h, 0, f32, ext);   /* imyy */
            for (i = 0; i < n; i++) {
                double const y2 = alwan_cn_r(iy[i] * iy[i], f32), x2 = alwan_cn_r(ix[i] * ix[i], f32);
                double num = alwan_cn_r(alwan_cn_r(b[i] * y2, f32) + alwan_cn_r(c[i] * x2, f32), f32);
                double const cross = alwan_cn_r(alwan_cn_r(alwan_cn_r(2.0 * a[i], f32) * ix[i], f32) * iy[i], f32);
                double const den = alwan_cn_r(x2 + y2, f32);
                num = alwan_cn_r(num - cross, f32);
                res[i] = den != 0.0 ? alwan_cn_r(num / den, f32) : 0.0;
            }
        } else {
            double arr, arc, acc;
            if (sigma > 1e-15) {
                double const g = -0.5 / (sigma * sigma);
                double s;
                for (i = 0; i <= 2 * lw; i++) {
                    long const xi = (long)i - (long)lw;
                    wts[i] = ALWAN_EXP_F64(g * (double)(xi * xi));
                }
                s = alwan_cn_sum(wts, 2 * lw + 1);
                for (i = 0; i <= 2 * lw; i++) wts[i] = wts[i] / s;
            }
            /* Arr, Arc, Acc into a, b, c */
            for (i = 0; i < n; i++) u[i] = alwan_cn_r(iy[i] * iy[i], f32);
            alwan_cn_gauss(a, u, t, w, h, wts, lw, sigma, f32, ext);
            for (i = 0; i < n; i++) u[i] = alwan_cn_r(iy[i] * ix[i], f32);
            alwan_cn_gauss(b, u, t, w, h, wts, lw, sigma, f32, ext);
            for (i = 0; i < n; i++) u[i] = alwan_cn_r(ix[i] * ix[i], f32);
            alwan_cn_gauss(c, u, t, w, h, wts, lw, sigma, f32, ext);
            for (i = 0; i < n; i++) {
                double det, tr;
                arr = a[i], arc = b[i], acc = c[i];
                det = alwan_cn_r(alwan_cn_r(arr * acc, f32) - alwan_cn_r(arc * arc, f32), f32);
                tr = alwan_cn_r(arr + acc, f32);
                switch (method) {
                case ALWAN_CORNER_HARRIS:
                    res[i] = p->harris_normalised ? alwan_cn_r(alwan_cn_r(2.0 * det, f32) / alwan_cn_r(tr + eps, f32), f32)
                                                  : alwan_cn_r(det - alwan_cn_r(k * alwan_cn_r(tr * tr, f32), f32), f32);
                    break;
                case ALWAN_CORNER_SHI_TOMASI: {
                    double const d = alwan_cn_r(arr - acc, f32);
                    double const q = alwan_cn_r(alwan_cn_r(d * d, f32) + alwan_cn_r(4.0 * alwan_cn_r(arc * arc, f32), f32), f32);
                    res[i] = alwan_cn_r(alwan_cn_r(tr - alwan_cn_r(ALWAN_SQRT_F64(q), f32), f32) / 2.0, f32);
                    break;
                }
                default:   /* FOERSTNER */
                    if (tr == 0.0) res[i] = 0.0;
                    else if (p->component == 0) res[i] = alwan_cn_r(det / tr, f32);
                    else res[i] = alwan_cn_r(alwan_cn_r(4.0 * det, f32) / alwan_cn_r(tr * tr, f32), f32);
                    break;
                }
            }
        }
    }
    for (y = 0; y < h; y++) {
        char *row = (char *)out + y * out_rs;
        for (x = 0; x < w; x++) {
            if (f32) ((alwan_f32 *)row)[x] = (alwan_f32)res[y * w + x];
            else ((alwan_f64 *)row)[x] = res[y * w + x];
        }
    }
    ALWAN_FREE(buf);
    return ALWAN_OK;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_corner_response_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height, alwan_corner_method method, alwan_corner_params const *params) {
    return alwan_cn_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_corner_response_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height, alwan_corner_method method, alwan_corner_params const *params) {
    return alwan_cn_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
