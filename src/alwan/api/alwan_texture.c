/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_texture_{T} and _u8: a texture code for every pixel of one channel, as
 * scikit-image's feature.local_binary_pattern computes it, which suite 230 holds this to.
 *
 * P neighbours on a circle of radius R, at row -R sin(2 pi p / P) and column
 * R cos(2 pi p / P), each rounded to 5 decimals as numpy.round rounds it, read by
 * scikit-image's bilinear interpolation with 0 outside the image; each neighbour is a 1
 * where it is at least the centre. From those bits:
 *
 *   LBP             sum of bit p times 2^p
 *   LBP_ROR         the smallest of its P right rotations: rotation invariant
 *   LBP_UNIFORM     the number of ones when the pattern has at most two changes along
 *                   p = 0 .. P - 1 (not wrapping, as scikit-image counts), else P + 1
 *   LBP_NRI_UNIFORM the non-rotation-invariant uniform code, 0 .. P (P - 1) + 2
 *   LBP_VAR         the neighbours' variance (ddof 0), NaN where it is 0
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double alwan_tx_pix(double const *img, size_t rows, size_t cols, long r, long c) {
    if (r < 0 || c < 0 || r >= (long)rows || c >= (long)cols) return 0.0;
    return img[(size_t)r * cols + (size_t)c];
}

/* numpy.round(x, 5): x * 10^5, rounded half to even, divided by 10^5 */
static double alwan_tx_round5(double x) {
    return nearbyint(x * 100000.0) / 100000.0;
}

static alwan_status alwan_tx_run(double *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_texture_method method, alwan_texture_params const *params, int kind) {
    alwan_texture_params const zero = { 0 };
    alwan_texture_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    int const P = p->points ? (int)p->points : 8;
    double const R = p->radius > 0.0 ? p->radius : 1.0;
    double const pi = 3.14159265358979323846;
    double *img, rp[31], cp[31], tex[31];
    int bits[31], i;
    size_t x, y, n = w * h;
    if (!out || !src || w == 0 || h == 0 || ch != 1 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem < w || out_rs / sizeof(double) < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_TEXTURE_LBP_VAR) return ALWAN_E_INVALID;
    if (p->points > 31 || !(p->radius >= 0.0) || p->radius > 1e6) return ALWAN_E_RANGE;
    for (i = 0; i < P; i++) {
        double const a = 2.0 * pi * (double)i / (double)P;
        rp[i] = alwan_tx_round5(-R * ALWAN_SIN_F64(a));
        cp[i] = alwan_tx_round5(R * ALWAN_COS_F64(a));
    }
    img = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    if (!img) return ALWAN_E_NOMEM;
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            double const v = kind == 0 ? ((alwan_f64 const *)row)[x] : kind == 1 ? (double)((alwan_f32 const *)row)[x]
                                                                               : (double)((unsigned char const *)row)[x];
            if (!(v - v == 0.0)) {
                ALWAN_FREE(img);
                return ALWAN_E_INVALID;
            }
            img[y * w + x] = v;
        }
    }
    for (y = 0; y < h; y++) {
        double *orow = (double *)((char *)out + y * out_rs);
        for (x = 0; x < w; x++) {
            double const centre = img[y * w + x];
            double lbp = 0.0;
            for (i = 0; i < P; i++) {
                /* scikit-image's bilinear_interpolation, mode 'C', cval 0 */
                double const rr = (double)y + rp[i], cc = (double)x + cp[i];
                long const minr = (long)ALWAN_FLOOR_F64(rr), minc = (long)ALWAN_FLOOR_F64(cc), maxr = (long)ALWAN_CEIL_F64(rr), maxc = (long)ALWAN_CEIL_F64(cc);
                double const dr = rr - (double)minr, dc = cc - (double)minc;
                double const tl = alwan_tx_pix(img, h, w, minr, minc), tr = alwan_tx_pix(img, h, w, minr, maxc);
                double const bl = alwan_tx_pix(img, h, w, maxr, minc), br = alwan_tx_pix(img, h, w, maxr, maxc);
                double const top = (1.0 - dc) * tl + dc * tr, bottom = (1.0 - dc) * bl + dc * br;
                tex[i] = (1.0 - dr) * top + dr * bottom;
                bits[i] = tex[i] - centre >= 0.0;
            }
            if (method == ALWAN_TEXTURE_LBP_VAR) {
                double s = 0.0, v = 0.0;
                for (i = 0; i < P; i++) {
                    s += tex[i];
                    v += tex[i] * tex[i];
                }
                v = (v - (s * s) / (double)P) / (double)P;
                lbp = v != 0.0 ? v : NAN;
            } else if (method == ALWAN_TEXTURE_LBP_UNIFORM || method == ALWAN_TEXTURE_LBP_NRI_UNIFORM) {
                int changes = 0;
                for (i = 0; i < P - 1; i++) changes += bits[i] != bits[i + 1];
                if (method == ALWAN_TEXTURE_LBP_NRI_UNIFORM) {
                    if (changes <= 2) {
                        int n_ones = 0, first_one = -1, first_zero = -1;
                        for (i = 0; i < P; i++) {
                            if (bits[i]) {
                                n_ones++;
                                if (first_one == -1) first_one = i;
                            } else if (first_zero == -1) {
                                first_zero = i;
                            }
                        }
                        if (n_ones == 0) lbp = 0.0;
                        else if (n_ones == P) lbp = (double)(P * (P - 1) + 1);
                        else {
                            int const rot = first_one == 0 ? n_ones - first_zero : P - first_one;
                            lbp = (double)(1 + (n_ones - 1) * P + rot);
                        }
                    } else {
                        lbp = (double)(P * (P - 1) + 2);
                    }
                } else if (changes <= 2) {
                    for (i = 0; i < P; i++) lbp += (double)bits[i];
                } else {
                    lbp = (double)(P + 1);
                }
            } else {
                unsigned long code = 0, best, cur;
                for (i = 0; i < P; i++) code |= (unsigned long)bits[i] << i;
                if (method == ALWAN_TEXTURE_LBP_ROR) {
                    best = cur = code;
                    for (i = 1; i < P; i++) {
                        cur = (cur >> 1) | ((cur & 1u) << (P - 1));
                        if (cur < best) best = cur;
                    }
                    code = best;
                }
                lbp = (double)code;
            }
            orow[x] = lbp;
        }
    }
    ALWAN_FREE(img);
    return ALWAN_OK;
}

alwan_status alwan_texture_u8(double *out, size_t out_row_stride, unsigned char const *src, size_t src_row_stride, size_t channels, size_t width,
                              size_t height, alwan_texture_method method, alwan_texture_params const *params) {
    return alwan_tx_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_texture_f64(double *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels, size_t width,
                               size_t height, alwan_texture_method method, alwan_texture_params const *params) {
    return alwan_tx_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_texture_f32(double *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels, size_t width,
                               size_t height, alwan_texture_method method, alwan_texture_params const *params) {
    return alwan_tx_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
