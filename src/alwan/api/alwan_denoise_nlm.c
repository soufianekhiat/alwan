/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan__denoise_nlm: non-local means on float data, as scikit-image's
 * restoration.denoise_nl_means computes it, for ALWAN_DENOISE_NL_MEANS_DARBON (its fast
 * mode) and ALWAN_DENOISE_NL_MEANS_BUADES (its gaussian-weighted patches), which suite 231
 * holds this to.
 *
 * Every pixel becomes the weighted mean of the pixels within patch_distance d of it, each
 * weighted by exp(-distance) between the s x s patches around the two, the distance the
 * patches' squared difference over all channels, less 2 sigma^2 a sample, scaled by h:
 *
 *   DARBON  (Darbon et al. 2008) the flat patch distance / (channels h^2 s^2), for every
 *           shift at once by an integral image of the squared differences; each pair is
 *           visited once and weighted both ways. All in double; the image padded by numpy's
 *           "reflect" by s / 2 + d + 1.
 *   BUADES  (Buades, Coll and Morel 2005) the patch distance weighted by a gaussian of
 *           sigma (s - 1) / 4 normalised to 1 / (channels h^2), in the data's precision,
 *           padded by s / 2; a patch stops being summed once its distance passes 5.
 *
 * Both give zero weight past a distance of 5. The weight is exp(-distance); with fast_exp
 * set it is scikit-image's Schraudolph approximation (1999) instead, the double whose high
 * word is (int32)(2^20 / ln 2 * y) + 1072632447 and whose low word is 0, a few percent off
 * exp, which reproduces scikit-image's results to the bit.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

/* Schraudolph (1999), "A fast, compact approximation of the exponential function", with
 * the constant for the least RMS error, as scikit-image's fast_exp.h */
static double alwan_nlm_fast_exp(double y) {
    int32_t const hi = (int32_t)(1512775.3951951856938 * y) + 1072632447;
    uint64_t const bits = (uint64_t)(uint32_t)hi << 32;
    double d;
    memcpy(&d, &bits, sizeof(d));
    return d;
}

static double alwan_nlm_exp(double y, int fast) {
    return fast ? alwan_nlm_fast_exp(y) : ALWAN_EXP_F64(y);
}

static double alwan_nlm_r(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

/* numpy's pad "reflect": d c b | a b c d | c b a */
static size_t alwan_nlm_reflect(long i, size_t n) {
    long p, m;
    if (n == 1) return 0;
    p = 2 * ((long)n - 1);
    m = i % p;
    if (m < 0) m += p;
    return (size_t)(m >= (long)n ? p - m : m);
}

static double alwan_nlm_pairwise(double const *a, size_t n, int f32) {
    if (n < 8) {
        double r = 0.0;
        size_t i;
        for (i = 0; i < n; i++) r = alwan_nlm_r(r + a[i], f32);
        return r;
    }
    if (n <= 128) {
        double r[8], res;
        size_t i, j;
        for (j = 0; j < 8; j++) r[j] = a[j];
        for (i = 8; i < n - (n % 8); i += 8)
            for (j = 0; j < 8; j++) r[j] = alwan_nlm_r(r[j] + a[i + j], f32);
        res = alwan_nlm_r(alwan_nlm_r(alwan_nlm_r(r[0] + r[1], f32) + alwan_nlm_r(r[2] + r[3], f32), f32) +
                              alwan_nlm_r(alwan_nlm_r(r[4] + r[5], f32) + alwan_nlm_r(r[6] + r[7], f32), f32),
                          f32);
        for (; i < n; i++) res = alwan_nlm_r(res + a[i], f32);
        return res;
    }
    {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan_nlm_r(alwan_nlm_pairwise(a, n2, f32) + alwan_nlm_pairwise(a + n2, n - n2, f32), f32);
    }
}

/* img: h x w x ch doubles (the data's values), res receives the result in the same layout. */
static alwan_status alwan_nlm_darbon(double *res, double const *img, size_t w, size_t h, size_t ch, size_t s, size_t d, double hh, double var,
                                     int fast) {
    size_t const offset = s / 2, pad = offset + d + 1, pw = w + 2 * pad, ph = h + 2 * pad, np_ = pw * ph;
    double *padded = (double *)ALWAN_ALLOC(alwan_safe_array_size(np_, (2 * ch + 2) * sizeof(double)), sizeof(double));
    double *weights, *integral, *result;
    double const h2s2 = (double)ch * hh * hh * (double)s * (double)s;
    long t_row, t_col, row, col;
    size_t c, y, x;
    if (!padded) return ALWAN_E_NOMEM;
    weights = padded + np_ * ch;
    integral = weights + np_;
    result = integral + np_;
    for (y = 0; y < ph; y++)
        for (x = 0; x < pw; x++) {
            size_t const sy = alwan_nlm_reflect((long)y - (long)pad, h), sx = alwan_nlm_reflect((long)x - (long)pad, w);
            for (c = 0; c < ch; c++) padded[(y * pw + x) * ch + c] = img[(sy * w + sx) * ch + c];
        }
    memset(weights, 0, np_ * sizeof(double));
    memset(integral, 0, np_ * sizeof(double));
    memset(result, 0, np_ * ch * sizeof(double));
    var *= 2.0;
    for (t_row = -(long)d; t_row <= (long)d; t_row++) {
        long const row_start = (long)offset > (long)offset - t_row ? (long)offset : (long)offset - t_row;
        long const row_end = (long)(ph - offset) < (long)(ph - offset) - t_row ? (long)(ph - offset) : (long)(ph - offset) - t_row;
        for (t_col = 0; t_col <= (long)d; t_col++) {
            double const alpha = t_col == 0 ? 0.5 : 1.0;
            /* the integral image of the squared differences at this shift */
            long const ir0 = 1 > -t_row ? 1 : -t_row, ir1 = (long)ph < (long)ph - t_row ? (long)ph : (long)ph - t_row;
            for (row = ir0; row < ir1; row++) {
                for (col = 1; col < (long)pw - t_col; col++) {
                    double dist = 0.0;
                    for (c = 0; c < ch; c++) {
                        double const t = padded[((size_t)row * pw + (size_t)col) * ch + c] -
                                         padded[((size_t)(row + t_row) * pw + (size_t)(col + t_col)) * ch + c];
                        dist += t * t;
                    }
                    dist -= (double)ch * var;
                    integral[(size_t)row * pw + (size_t)col] = dist + integral[(size_t)(row - 1) * pw + (size_t)col] +
                                                               integral[(size_t)row * pw + (size_t)(col - 1)] -
                                                               integral[(size_t)(row - 1) * pw + (size_t)(col - 1)];
                }
            }
            for (row = row_start; row < row_end; row++) {
                long const row_shift = row + t_row;
                for (col = (long)offset; col < (long)pw - (long)offset - t_col; col++) {
                    size_t const o = offset;
                    double dist = integral[(size_t)(row + (long)o) * pw + (size_t)(col + (long)o)] +
                                  integral[(size_t)(row - (long)o) * pw + (size_t)(col - (long)o)] -
                                  integral[(size_t)(row - (long)o) * pw + (size_t)(col + (long)o)] -
                                  integral[(size_t)(row + (long)o) * pw + (size_t)(col - (long)o)];
                    double wgt;
                    long const col_shift = col + t_col;
                    dist = (dist > 0.0 ? dist : 0.0) / h2s2;
                    if (dist > 5.0) continue;
                    wgt = alpha * alwan_nlm_exp(-dist, fast);
                    weights[(size_t)row * pw + (size_t)col] += wgt;
                    weights[(size_t)row_shift * pw + (size_t)col_shift] += wgt;
                    for (c = 0; c < ch; c++) {
                        result[((size_t)row * pw + (size_t)col) * ch + c] += wgt * padded[((size_t)row_shift * pw + (size_t)col_shift) * ch + c];
                        result[((size_t)row_shift * pw + (size_t)col_shift) * ch + c] += wgt * padded[((size_t)row * pw + (size_t)col) * ch + c];
                    }
                }
            }
        }
    }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            for (c = 0; c < ch; c++) {
                size_t const po = (y + pad) * pw + (x + pad);
                res[(y * w + x) * ch + c] = result[po * ch + c] / weights[po];
            }
    ALWAN_FREE(padded);
    return ALWAN_OK;
}

static alwan_status alwan_nlm_buades(double *res, double const *img, size_t w, size_t h, size_t ch, size_t s, size_t d, double hh, double var,
                                     int f32, int fast) {
    size_t const offset = s / 2, pw = w + 2 * offset, ph = h + 2 * offset;
    double *padded = (double *)ALWAN_ALLOC(alwan_safe_array_size(pw * ph * ch + s * s + ch, sizeof(double)), sizeof(double));
    double *wk, *nv, A, two_a2, sum, scale;
    size_t y, x, c, i, j;
    if (!padded) return ALWAN_E_NOMEM;
    wk = padded + pw * ph * ch;
    nv = wk + s * s;
    for (y = 0; y < ph; y++)
        for (x = 0; x < pw; x++) {
            size_t const sy = alwan_nlm_reflect((long)y - (long)offset, h), sx = alwan_nlm_reflect((long)x - (long)offset, w);
            for (c = 0; c < ch; c++) padded[(y * pw + x) * ch + c] = img[(sy * w + sx) * ch + c];
        }
    /* the gaussian patch kernel as scikit-image builds it with numpy, in the data's precision */
    A = alwan_nlm_r(((double)s - 1.0) / 4.0, f32);
    two_a2 = alwan_nlm_r(2.0 * A * A, f32);
    for (i = 0; i < s; i++)
        for (j = 0; j < s; j++) {
            double const r = (double)((long)i - (long)offset), q = (double)((long)j - (long)offset);
            double const e = alwan_nlm_r(-alwan_nlm_r(alwan_nlm_r(r * r, f32) + alwan_nlm_r(q * q, f32), f32) / two_a2, f32);
            wk[i * s + j] = f32 ? (double)ALWAN_EXP_F32((float)e) : ALWAN_EXP_F64(e);
        }
    sum = alwan_nlm_r(0.0 + alwan_nlm_pairwise(wk, s * s, f32), f32);
    scale = alwan_nlm_r(1.0 / alwan_nlm_r(alwan_nlm_r(alwan_nlm_r((double)ch * sum, f32) * alwan_nlm_r(hh, f32), f32) * alwan_nlm_r(hh, f32), f32), f32);
    for (i = 0; i < s * s; i++) wk[i] = alwan_nlm_r(wk[i] * scale, f32);
    var = alwan_nlm_r(var * 2.0, f32);
    for (y = 0; y < h; y++) {
        size_t const i0 = y - (d < y ? d : y), i1 = y + (d + 1 < h - y ? d + 1 : h - y);
        for (x = 0; x < w; x++) {
            size_t const j0 = x - (d < x ? d : x), j1 = x + (d + 1 < w - x ? d + 1 : w - x);
            double wsum = 0.0;
            for (c = 0; c < ch; c++) nv[c] = 0.0;
            for (i = i0; i < i1; i++) {
                for (j = j0; j < j1; j++) {
                    double dist = 0.0, wgt;
                    size_t a, b;
                    int cut = 0;
                    for (a = 0; a < s && !cut; a++) {
                        if (dist > 5.0) {
                            cut = 1;
                            break;
                        }
                        for (b = 0; b < s; b++)
                            for (c = 0; c < ch; c++) {
                                double const t = alwan_nlm_r(padded[((y + a) * pw + x + b) * ch + c] - padded[((i + a) * pw + j + b) * ch + c], f32);
                                dist = alwan_nlm_r(dist + alwan_nlm_r(wk[a * s + b] * alwan_nlm_r(alwan_nlm_r(t * t, f32) - var, f32), f32), f32);
                            }
                    }
                    wgt = cut ? 0.0 : alwan_nlm_r(alwan_nlm_exp(-(0.0 > dist ? 0.0 : dist), fast), f32);
                    wsum = alwan_nlm_r(wsum + wgt, f32);
                    for (c = 0; c < ch; c++) nv[c] = alwan_nlm_r(nv[c] + alwan_nlm_r(wgt * padded[((i + offset) * pw + j + offset) * ch + c], f32), f32);
                }
            }
            for (c = 0; c < ch; c++) res[(y * w + x) * ch + c] = alwan_nlm_r(nv[c] / wsum, f32);
        }
    }
    ALWAN_FREE(padded);
    return ALWAN_OK;
}

alwan_status alwan__denoise_nlm(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h, int buades,
                                double hh, size_t s, size_t d, double sigma, int fast_exp, int kind) {
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const n = w * h;
    double *img, *res;
    size_t x, y;
    alwan_status st;
    if (!out || !src || w == 0 || h == 0 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if (!(hh > 0.0) || !(sigma >= 0.0) || s > 63 || d > 255) return ALWAN_E_RANGE;
    if (s % 2 == 0) s += 1;
    img = (double *)ALWAN_ALLOC(alwan_safe_array_size(n * ch, 2 * sizeof(double)), sizeof(double));
    if (!img) return ALWAN_E_NOMEM;
    res = img + n * ch;
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w * ch; x++) {
            double const v = kind == 0 ? ((alwan_f64 const *)row)[x] : kind == 1 ? (double)((alwan_f32 const *)row)[x]
                                                                               : (double)((unsigned char const *)row)[x] * (1.0 / 255.0);
            if (!(v - v == 0.0)) {
                ALWAN_FREE(img);
                return ALWAN_E_INVALID;
            }
            img[y * w * ch + x] = v;
        }
    }
    st = buades ? alwan_nlm_buades(res, img, w, h, ch, s, d, hh, sigma * sigma, kind == 1, fast_exp != 0)
                : alwan_nlm_darbon(res, img, w, h, ch, s, d, hh, sigma * sigma, fast_exp != 0);
    if (st == ALWAN_OK) {
        for (y = 0; y < h; y++) {
            char *row = (char *)out + y * out_rs;
            for (x = 0; x < w * ch; x++) {
                double const v = res[y * w * ch + x];
                if (kind == 0) ((alwan_f64 *)row)[x] = v;
                else if (kind == 1) ((alwan_f32 *)row)[x] = (alwan_f32)v;
                else {
                    double const t = nearbyint(v * 255.0);
                    ((unsigned char *)row)[x] = (unsigned char)(t < 0.0 ? 0.0 : t > 255.0 ? 255.0 : t);
                }
            }
        }
    }
    ALWAN_FREE(img);
    return st;
}
