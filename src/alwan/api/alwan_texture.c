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
 *
 * The GABOR methods and alwan_gabor_kernel_{T} follow scikit-image 0.26's
 * filters/_gabor.py (gabor_kernel, gabor) step for step, and the convolution follows
 * scipy.ndimage's NI_Correlate: the kernel flipped, its weights of magnitude at most
 * DBL_EPSILON left out, the rest summed in the flipped kernel's row-major order in double
 * (suite 294). scikit-image is Copyright the scikit-image team, BSD-3-Clause
 * (https://github.com/scikit-image/scikit-image); see THIRD_PARTY_NOTICES.md and
 * licenses/scikit-image-BSD-3-Clause.txt, whose conditions apply to this port:
 *
 *   Redistribution and use in source and binary forms, with or without modification, are
 *   permitted provided that the following conditions are met: (1) redistributions of
 *   source code must retain the above copyright notice, this list of conditions and the
 *   following disclaimer; (2) redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution; (3) neither the
 *   name of the copyright holder nor the names of its contributors may be used to endorse
 *   or promote products derived from this software without specific prior written
 *   permission. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 *   NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------------------
 * Gabor kernel, as filters/_gabor.py's gabor_kernel builds it
 * --------------------------------------------------------------------------------------- */

typedef struct {
    double sx, sy, ct, st, c2pf, offset, scale;
    long x0, y0;
} alwan_gb_spec;

static alwan_status alwan_gb_spec_make(alwan_gb_spec *g, alwan_texture_params const *p) {
    double const pi = 3.14159265358979323846;
    double const b = p->bandwidth > 0.0 ? p->bandwidth : 1.0;
    double const n = p->n_stds > 0.0 ? p->n_stds : 3.0;
    double ax, ay, m;
    if (!(p->frequency > 0.0) || !(p->frequency < 1e300)) return ALWAN_E_RANGE;
    if (!(p->bandwidth >= 0.0) || !(p->sigma_x >= 0.0) || !(p->sigma_y >= 0.0) || !(p->n_stds >= 0.0)) return ALWAN_E_RANGE;
    if (!(p->theta - p->theta == 0.0) || !(p->offset - p->offset == 0.0)) return ALWAN_E_INVALID;
    if (p->sigma_x == 0.0 || p->sigma_y == 0.0) {
        /* _sigma_prefactor: 1.0 / np.pi * sqrt(log(2) / 2.0) * (2.0**b + 1) / (2.0**b - 1) */
        double const pb = ALWAN_POW_F64(2.0, b);
        double const pre = 1.0 / pi * ALWAN_SQRT_F64(ALWAN_LN_F64(2.0) / 2.0) * (pb + 1.0) / (pb - 1.0);
        if (!(pb - 1.0 > 0.0)) return ALWAN_E_RANGE;
        g->sx = p->sigma_x > 0.0 ? p->sigma_x : pre / p->frequency;
        g->sy = p->sigma_y > 0.0 ? p->sigma_y : pre / p->frequency;
    } else {
        g->sx = p->sigma_x;
        g->sy = p->sigma_y;
    }
    if (!(g->sx > 0.0) || !(g->sy > 0.0) || !(g->sx < 1e300) || !(g->sy < 1e300)) return ALWAN_E_RANGE;
    g->ct = ALWAN_COS_F64(p->theta);
    g->st = ALWAN_SIN_F64(p->theta);
    /* x0 = ceil(max(abs(n sx ct), abs(n sy st), 1)), y0 likewise with the roles swapped */
    ax = ALWAN_ABS_F64(n * g->sx * g->ct);
    ay = ALWAN_ABS_F64(n * g->sy * g->st);
    m = ax > ay ? ax : ay;
    if (m < 1.0) m = 1.0;
    if (!(m <= 32767.0)) return ALWAN_E_RANGE;
    g->x0 = (long)ALWAN_CEIL_F64(m);
    ax = ALWAN_ABS_F64(n * g->sy * g->ct);
    ay = ALWAN_ABS_F64(n * g->sx * g->st);
    m = ax > ay ? ax : ay;
    if (m < 1.0) m = 1.0;
    if (!(m <= 32767.0)) return ALWAN_E_RANGE;
    g->y0 = (long)ALWAN_CEIL_F64(m);
    g->c2pf = 2.0 * pi * p->frequency;
    g->offset = p->offset;
    g->scale = 1.0 / (2.0 * pi * g->sx * g->sy);
    return ALWAN_OK;
}

/* One kernel value before scaling: exp(-0.5 (x'^2 / sx^2 + y'^2 / sy^2)) times
 * (cos, sin)(2 pi f x' + offset), as numpy's complex exp gives it */
static void alwan_gb_value(alwan_gb_spec const *g, long xx, long yy, double *re, double *im) {
    double const rotx = (double)xx * g->ct + (double)yy * g->st;
    double const roty = (double)(-xx) * g->st + (double)yy * g->ct;
    double const a = -0.5 * (rotx * rotx / (g->sx * g->sx) + roty * roty / (g->sy * g->sy));
    double const ph = g->c2pf * rotx + g->offset;
    double const e = ALWAN_EXP_F64(a);
    *re = e * ALWAN_COS_F64(ph);
    *im = e * ALWAN_SIN_F64(ph);
}

/* The kernel into kr and ki (kw x kh, row-major), double, or rounded as complex64 */
static void alwan_gb_fill(alwan_gb_spec const *g, double *kr, double *ki, int as_float) {
    size_t const kw = (size_t)(2 * g->x0 + 1), kh = (size_t)(2 * g->y0 + 1);
    size_t ry, cx;
    float const sf = (float)g->scale;
    for (ry = 0; ry < kh; ry++) {
        for (cx = 0; cx < kw; cx++) {
            size_t const k = ry * kw + cx;
            double re, im;
            alwan_gb_value(g, (long)cx - g->x0, (long)ry - g->y0, &re, &im);
            if (as_float) {
                kr[k] = (double)((float)re * sf);
                ki[k] = (double)((float)im * sf);
            } else {
                kr[k] = re * g->scale;
                ki[k] = im * g->scale;
            }
        }
    }
}

/* scipy.ndimage's extension of an index past [0, n) */
static long alwan_gb_index(long i, long n, alwan_filter_border border) {
    if (i >= 0 && i < n) return i;
    switch (border) {
    case ALWAN_FILTER_BORDER_NEAREST:
        return i < 0 ? 0 : n - 1;
    case ALWAN_FILTER_BORDER_WRAP: {
        long r = i % n;
        return r < 0 ? r + n : r;
    }
    case ALWAN_FILTER_BORDER_REFLECT: {
        long const p2 = 2 * n;
        long r = i % p2;
        if (r < 0) r += p2;
        return r < n ? r : p2 - 1 - r;
    }
    case ALWAN_FILTER_BORDER_MIRROR: {
        long p2, r;
        if (n == 1) return 0;
        p2 = 2 * n - 2;
        r = i % p2;
        if (r < 0) r += p2;
        return r < n ? r : p2 - r;
    }
    default:
        return -1; /* CONSTANT */
    }
}

/* ndimage.convolve of img (w x h, double) with k (kw x kh) into out rows */
static alwan_status alwan_gb_convolve(double *out, size_t out_rs, double const *img, size_t w, size_t h, double const *k, size_t kw, size_t kh,
                              alwan_filter_border border, double cval, int as_float, double const *other, int magnitude) {
    size_t const ksz = kw * kh;
    size_t y, x, a, nz = 0;
    long const cy = (long)(kh / 2), cx = (long)(kw / 2);
    double *ww = (double *)ALWAN_ALLOC(alwan_safe_array_size(ksz, sizeof(double)), sizeof(double));
    long *dy = (long *)ALWAN_ALLOC(alwan_safe_array_size(ksz, sizeof(long)), sizeof(long));
    long *dx = (long *)ALWAN_ALLOC(alwan_safe_array_size(ksz, sizeof(long)), sizeof(long));
    alwan_status const st = (ww && dy && dx) ? ALWAN_OK : ALWAN_E_NOMEM;
    if (st == ALWAN_OK) {
        /* the flipped kernel in row-major order, weights at most DBL_EPSILON left out */
        for (a = 0; a < ksz; a++) {
            size_t const ar = a / kw, ac = a % kw;
            double const v = k[(kh - 1 - ar) * kw + (kw - 1 - ac)];
            if (ALWAN_ABS_F64(v) > DBL_EPSILON) {
                ww[nz] = v;
                dy[nz] = (long)ar - cy;
                dx[nz] = (long)ac - cx;
                nz++;
            }
        }
        for (y = 0; y < h; y++) {
            double *orow = (double *)((char *)out + y * out_rs);
            for (x = 0; x < w; x++) {
                double tmp = 0.0;
                for (a = 0; a < nz; a++) {
                    long const yy = alwan_gb_index((long)y + dy[a], (long)h, border);
                    long const xx = alwan_gb_index((long)x + dx[a], (long)w, border);
                    if (yy < 0 || xx < 0) tmp += cval * ww[a];
                    else tmp += img[(size_t)yy * w + (size_t)xx] * ww[a];
                }
                if (as_float) tmp = (double)(float)tmp;
                if (magnitude) {
                    double const o = other[y * w + x];
                    tmp = as_float ? (double)(float)ALWAN_HYPOT_F64(o, tmp) : ALWAN_HYPOT_F64(o, tmp);
                }
                orow[x] = tmp;
            }
        }
    }
    ALWAN_FREE(dx);
    ALWAN_FREE(dy);
    ALWAN_FREE(ww);
    return st;
}

static alwan_status alwan_gb_run(double *out, size_t out_rs, double const *img, size_t w, size_t h, alwan_texture_method method,
                                 alwan_texture_params const *p, int as_float) {
    alwan_gb_spec g;
    alwan_status st = alwan_gb_spec_make(&g, p);
    size_t kw, kh, ksz;
    double *kr, *ki, *re = NULL;
    if (st != ALWAN_OK) return st;
    if ((unsigned)p->border > (unsigned)ALWAN_FILTER_BORDER_WRAP) return ALWAN_E_INVALID;
    if (!(p->cval - p->cval == 0.0)) return ALWAN_E_INVALID;
    kw = (size_t)(2 * g.x0 + 1);
    kh = (size_t)(2 * g.y0 + 1);
    ksz = kw * kh;
    kr = (double *)ALWAN_ALLOC(alwan_safe_array_size(ksz, sizeof(double)), sizeof(double));
    ki = (double *)ALWAN_ALLOC(alwan_safe_array_size(ksz, sizeof(double)), sizeof(double));
    if (method == ALWAN_TEXTURE_GABOR_MAGNITUDE) re = (double *)ALWAN_ALLOC(alwan_safe_array_size(w * h, sizeof(double)), sizeof(double));
    if (!kr || !ki || (method == ALWAN_TEXTURE_GABOR_MAGNITUDE && !re)) {
        ALWAN_FREE(re);
        ALWAN_FREE(ki);
        ALWAN_FREE(kr);
        return ALWAN_E_NOMEM;
    }
    alwan_gb_fill(&g, kr, ki, as_float);
    if (method == ALWAN_TEXTURE_GABOR_REAL) {
        st = alwan_gb_convolve(out, out_rs, img, w, h, kr, kw, kh, p->border, p->cval, as_float, NULL, 0);
    } else if (method == ALWAN_TEXTURE_GABOR_IMAG) {
        st = alwan_gb_convolve(out, out_rs, img, w, h, ki, kw, kh, p->border, p->cval, as_float, NULL, 0);
    } else {
        st = alwan_gb_convolve(re, w * sizeof(double), img, w, h, kr, kw, kh, p->border, p->cval, as_float, NULL, 0);
        if (st == ALWAN_OK) st = alwan_gb_convolve(out, out_rs, img, w, h, ki, kw, kh, p->border, p->cval, as_float, re, 1);
    }
    ALWAN_FREE(re);
    ALWAN_FREE(ki);
    ALWAN_FREE(kr);
    return st;
}

static alwan_status alwan_gb_kernel(void *real, void *imag, size_t *width_out, size_t *height_out, alwan_texture_params const *p, int as_float) {
    alwan_gb_spec g;
    alwan_status st;
    size_t kw, kh, ksz, i;
    double *kr, *ki;
    if (!p || !width_out || !height_out) return ALWAN_E_INVALID;
    st = alwan_gb_spec_make(&g, p);
    if (st != ALWAN_OK) return st;
    kw = (size_t)(2 * g.x0 + 1);
    kh = (size_t)(2 * g.y0 + 1);
    *width_out = kw;
    *height_out = kh;
    if (!real && !imag) return ALWAN_OK;
    ksz = kw * kh;
    kr = (double *)ALWAN_ALLOC(alwan_safe_array_size(ksz, sizeof(double)), sizeof(double));
    ki = (double *)ALWAN_ALLOC(alwan_safe_array_size(ksz, sizeof(double)), sizeof(double));
    if (!kr || !ki) {
        ALWAN_FREE(ki);
        ALWAN_FREE(kr);
        return ALWAN_E_NOMEM;
    }
    alwan_gb_fill(&g, kr, ki, as_float);
    for (i = 0; i < ksz; i++) {
        if (as_float) {
            if (real) ((alwan_f32 *)real)[i] = (alwan_f32)kr[i];
            if (imag) ((alwan_f32 *)imag)[i] = (alwan_f32)ki[i];
        } else {
            if (real) ((alwan_f64 *)real)[i] = kr[i];
            if (imag) ((alwan_f64 *)imag)[i] = ki[i];
        }
    }
    ALWAN_FREE(ki);
    ALWAN_FREE(kr);
    return ALWAN_OK;
}

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
    if ((unsigned)method > (unsigned)ALWAN_TEXTURE_GABOR_MAGNITUDE) return ALWAN_E_INVALID;
    if (p->points > 31 || !(p->radius >= 0.0) || p->radius > 1e6) return ALWAN_E_RANGE;
    if (method >= ALWAN_TEXTURE_GABOR_REAL) {
        alwan_gb_spec g;
        alwan_status const st = alwan_gb_spec_make(&g, p);
        if (st != ALWAN_OK) return st;
        if ((unsigned)p->border > (unsigned)ALWAN_FILTER_BORDER_WRAP || !(p->cval - p->cval == 0.0)) return ALWAN_E_INVALID;
    }
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
    if (method >= ALWAN_TEXTURE_GABOR_REAL) {
        alwan_status const st = alwan_gb_run(out, out_rs, img, w, h, method, p, kind == 1);
        ALWAN_FREE(img);
        return st;
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

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_gabor_kernel_f64(alwan_f64 *real, alwan_f64 *imag, size_t *width_out, size_t *height_out, alwan_texture_params const *params) {
    return alwan_gb_kernel(real, imag, width_out, height_out, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_gabor_kernel_f32(alwan_f32 *real, alwan_f32 *imag, size_t *width_out, size_t *height_out, alwan_texture_params const *params) {
    return alwan_gb_kernel(real, imag, width_out, height_out, params, 1);
}
#endif
