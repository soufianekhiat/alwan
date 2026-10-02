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
 *
 * Two more estimators, by `method`: Grey Pixel (Yang, Gao and Li, CVPR 2015), written from
 * the paper and held to the authors' MATLAB code (GPconstancy.m, GetGreyidx.m; no licence:
 * run at generation time as the oracle, nothing copied), and the iterative weighted
 * Grey-Edge (Gijsenij, Gevers and van de Weijer, TPAMI 2012), held to an MIT-licensed
 * Python port of weightedGE.m (DerrickXuNu/Illuminant-Aware-Gamut-Based-Color-Transfer),
 * also run only at generation time. The shadow-shading weighting is the paper's definition
 * (no code was found for it) and is held to a transcription of that definition.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>

#define ALWAN_CC_TAPS_MAX 1024   /* sigma up to about 340 pixels */
#define ALWAN_CC_PI 3.14159265358979323846

void alwan_constancy_params_init(alwan_constancy_params *params) {
    if (!params) return;
    params->order = 0;
    params->minkowski = 1.0;
    params->sigma = 0.0;
    params->saturation = 0.0;
    params->method = ALWAN_CONSTANCY_EDGE_FRAMEWORK;
    params->grey_pixel_percent = 0.0;
    params->grey_pixel_measure = ALWAN_GREY_PIXEL_LOCAL_STD;
    params->grey_pixel_window = 0;
    params->kappa = 0.0;
    params->edge_weight = ALWAN_EDGE_WEIGHT_SPECULAR;
    params->iterations = 0;
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
    if (p->method == ALWAN_CONSTANCY_GREY_PIXEL) {
        if (!alwan_cc_finite(p->grey_pixel_percent) || p->grey_pixel_percent < 0.0 || p->grey_pixel_percent > 100.0)
            return ALWAN_E_INVALID;
        if (!alwan_cc_finite(p->saturation) || p->saturation < 0.0) return ALWAN_E_INVALID;
        if (p->grey_pixel_measure == ALWAN_GREY_PIXEL_EDGE) {
            if (!alwan_cc_finite(s) || !(s > 0.0)) return ALWAN_E_INVALID;
        } else if (p->grey_pixel_measure == ALWAN_GREY_PIXEL_LOCAL_STD) {
            if (p->grey_pixel_window != 0 && (p->grey_pixel_window < 3 || p->grey_pixel_window % 2 == 0 ||
                                              p->grey_pixel_window > 255))
                return ALWAN_E_INVALID;
        } else {
            return ALWAN_E_INVALID;
        }
        *fs = 0;
        return ALWAN_OK;
    }
    if (p->method == ALWAN_CONSTANCY_WEIGHTED_GREY_EDGE) {
        if (!alwan_cc_finite(m) || m < 0.0 || (m > 0.0 && m < 1.0)) return ALWAN_E_INVALID;
        if (!alwan_cc_finite(s) || !(s > 0.0)) return ALWAN_E_INVALID;
        if (!alwan_cc_finite(p->kappa) || p->kappa < 0.0) return ALWAN_E_INVALID;
        if (!alwan_cc_finite(p->saturation) || p->saturation < 0.0) return ALWAN_E_INVALID;
        if (p->edge_weight != ALWAN_EDGE_WEIGHT_SPECULAR && p->edge_weight != ALWAN_EDGE_WEIGHT_SHADOW)
            return ALWAN_E_INVALID;
        *fs = (int)ALWAN_FLOOR_F64(3.0 * s + 0.5);
        if (*fs < 1 || *fs > (ALWAN_CC_TAPS_MAX - 1) / 2) return ALWAN_E_INVALID;
        return ALWAN_OK;
    }
    if (p->method != ALWAN_CONSTANCY_EDGE_FRAMEWORK) return ALWAN_E_INVALID;
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

/* ------------------------------------------------------------------------------------ */
/* Grey Pixel (Yang, Gao and Li 2015)                                                   */
/* ------------------------------------------------------------------------------------ */

/* GetGreyidx's LocalStd: the sample standard deviation (n - 1) over a win x win window,
 * the edge replicated. */
static void alwan_cc_local_std(double *out, double const *in, size_t w, size_t h, int win) {
    int const r = win / 2;
    double const nn = (double)(win * win);
    size_t x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double sum = 0.0, ss = 0.0, mean;
            int dx, dy;
            /* im2col's order: the window column by column */
            for (dx = -r; dx <= r; dx++)
                for (dy = -r; dy <= r; dy++)
                    sum += in[alwan_cc_clamp((long)y + dy, h) * w + alwan_cc_clamp((long)x + dx, w)];
            mean = sum / nn;
            for (dx = -r; dx <= r; dx++) {
                for (dy = -r; dy <= r; dy++) {
                    double const d = in[alwan_cc_clamp((long)y + dy, h) * w + alwan_cc_clamp((long)x + dx, w)] - mean;
                    ss += d * d;
                }
            }
            out[y * w + x] = ALWAN_SQRT_F64(ss / (nn - 1.0));
        }
    }
}

/* GetGreyidx's DerivGauss: the gradient magnitude of the 2D kernel -x exp(-(x^2 + y^2) /
 * (2 sigma^2)) / (pi sigma^2) and its transpose, convolved with the edge replicated, over
 * the largest half-width up to 50 whose Gaussian tail still exceeds 1e-6. */
static alwan_status alwan_cc_deriv_gauss(double *out, double const *in, size_t w, size_t h, double sigma) {
    double const ssq = sigma * sigma;
    int width = 0, pw, side, i, j;
    double *k;
    size_t x, y;
    for (pw = 1; pw <= 50; pw++)
        if (ALWAN_EXP_F64(-((double)pw * (double)pw) / (2.0 * ssq)) > 0.000001) width = pw;
    if (width == 0) width = 1;
    side = 2 * width + 1;
    k = (double *)ALWAN_ALLOC(alwan_safe_array_size((size_t)side * (size_t)side, sizeof(double)), sizeof(double));
    if (!k) return ALWAN_E_NOMEM;
    for (j = -width; j <= width; j++)
        for (i = -width; i <= width; i++)
            k[(j + width) * side + (i + width)] =
                -(double)i * ALWAN_EXP_F64(-((double)i * i + (double)j * j) / (2.0 * ssq)) / (ALWAN_CC_PI * ssq);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double ax = 0.0, ay = 0.0;
            /* convolution: the kernel turned half round, so offset (dy, dx) meets k(-dy, -dx) */
            for (j = -width; j <= width; j++) {
                for (i = -width; i <= width; i++) {
                    double const v = in[alwan_cc_clamp((long)y + j, h) * w + alwan_cc_clamp((long)x + i, w)];
                    ax += v * k[(-j + width) * side + (-i + width)];
                    ay += v * k[(-i + width) * side + (-j + width)];
                }
            }
            out[y * w + x] = ALWAN_SQRT_F64(ax * ax + ay * ay);
        }
    }
    ALWAN_FREE(k);
    return ALWAN_OK;
}

static int alwan_cc_cmp_double(void const *a, void const *b) {
    double const x = *(double const *)a, y = *(double const *)b;
    return (x > y) - (x < y);
}

/* img: w x h x 3 doubles; never: w x h bytes, nonzero for a pixel that may not be grey. */
static alwan_status alwan_cc_grey_pixel(double out[3], double const *img, unsigned char const *never, size_t w,
                                        size_t h, alwan_constancy_params const *p) {
    size_t const n = w * h;
    double const pct = p->grey_pixel_percent > 0.0 ? p->grey_pixel_percent : 0.1;
    int const win = p->grey_pixel_window > 0 ? (int)p->grey_pixel_window : 3;
    size_t const bytes = alwan_safe_array_size(n, 8 * sizeof(double));
    double *pool, *lg, *m[3], *gi, *g2, *sorted, gmax = 0.0, tt, norm2 = 0.0;
    size_t i, num, x, y;
    int c;
    alwan_status st = ALWAN_OK;
    if (bytes == 0) return ALWAN_E_NOMEM;
    pool = (double *)ALWAN_ALLOC(bytes, sizeof(double));
    if (!pool) return ALWAN_E_NOMEM;
    lg = pool;
    m[0] = pool + n;
    m[1] = pool + 2 * n;
    m[2] = pool + 3 * n;
    gi = pool + 4 * n;
    g2 = pool + 5 * n;
    sorted = pool + 6 * n;
    for (c = 0; c < 3; c++) {
        for (i = 0; i < n; i++) {
            double v = img[i * 3 + (size_t)c];
            if (v == 0.0) v = DBL_EPSILON;
            lg[i] = ALWAN_LN_F64(v);
        }
        if (p->grey_pixel_measure == ALWAN_GREY_PIXEL_EDGE) {
            st = alwan_cc_deriv_gauss(m[c], lg, w, h, p->sigma);
            if (st != ALWAN_OK) goto done;
        } else {
            alwan_cc_local_std(m[c], lg, w, h, win);
        }
    }
    for (i = 0; i < n; i++) {
        double const a = m[0][i], b = m[1][i], cc = m[2][i];
        double const mean = (a + b + cc) / 3.0;
        double const sd = ALWAN_SQRT_F64(((a - mean) * (a - mean) + (b - mean) * (b - mean) + (cc - mean) * (cc - mean)) / 2.0);
        double r = img[i * 3], g = img[i * 3 + 1], bl = img[i * 3 + 2];
        if (r == 0.0) r = DBL_EPSILON;
        if (g == 0.0) g = DBL_EPSILON;
        if (bl == 0.0) bl = DBL_EPSILON;
        gi[i] = (sd / (mean + DBL_EPSILON)) / ((r + g + bl) / 3.0 + DBL_EPSILON);
        if (i == 0 || gi[i] > gmax) gmax = gi[i];
    }
    {
        double gmax2 = 0.0;
        for (i = 0; i < n; i++) {
            gi[i] /= gmax + DBL_EPSILON;
            if (i == 0 || gi[i] > gmax2) gmax2 = gi[i];
        }
        for (i = 0; i < n; i++)
            if (m[0][i] < DBL_EPSILON && m[1][i] < DBL_EPSILON && m[2][i] < DBL_EPSILON) gi[i] = gmax2;
    }
    /* fspecial('average', [7 7]) with the image wrapped round */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double acc = 0.0;
            size_t oy, ox;
            /* offsets -3..3, wrapped: 7 h - 3 keeps the sum positive for any h >= 1 */
            for (oy = 0; oy < 7; oy++) {
                size_t const yy = (y + 7 * h - 3 + oy) % h;
                for (ox = 0; ox < 7; ox++) {
                    size_t const xx = (x + 7 * w - 3 + ox) % w;
                    acc += gi[yy * w + xx] * (1.0 / 49.0);
                }
            }
            g2[y * w + x] = acc;
        }
    }
    if (never) {
        double gm = g2[0];
        for (i = 1; i < n; i++)
            if (g2[i] > gm) gm = g2[i];
        for (i = 0; i < n; i++)
            if (never[i]) g2[i] = gm;
    }
    num = (size_t)ALWAN_FLOOR_F64(pct * (double)n / 100.0);
    if (num < 1) num = 1;
    if (num > n) num = n;
    for (i = 0; i < n; i++) sorted[i] = g2[i];
    qsort(sorted, n, sizeof(double), alwan_cc_cmp_double);
    tt = sorted[num - 1];
    out[0] = out[1] = out[2] = 0.0;
    for (i = 0; i < n; i++) {
        if (g2[i] <= tt) {
            for (c = 0; c < 3; c++) {
                double v = img[i * 3 + (size_t)c];
                out[c] += v == 0.0 ? DBL_EPSILON : v;
            }
        }
    }
    for (c = 0; c < 3; c++) norm2 += out[c] * out[c];
    if (!(norm2 > 0.0) || !alwan_cc_finite(norm2)) {
        st = ALWAN_E_RANGE;
        goto done;
    }
    norm2 = ALWAN_SQRT_F64(norm2);
    for (c = 0; c < 3; c++) out[c] /= norm2;
done:
    ALWAN_FREE(pool);
    return st;
}

/* ------------------------------------------------------------------------------------ */
/* Weighted Grey-Edge (Gijsenij, Gevers and van de Weijer 2012)                          */
/* ------------------------------------------------------------------------------------ */

static alwan_status alwan_cc_weighted_ge(double out[3], double const *img, unsigned char const *never, size_t w,
                                         size_t h, alwan_constancy_params const *p, int fs) {
    double k0[ALWAN_CC_TAPS_MAX], k1[ALWAN_CC_TAPS_MAX];
    size_t const n = w * h;
    size_t const bytes = alwan_safe_array_size(n, 15 * sizeof(double) + 1);
    double const kappa = p->kappa > 0.0 ? p->kappa : 1.0, mink = p->minkowski, border = p->sigma + 1.0;
    size_t const iters = p->iterations > 0 ? p->iterations : 10;
    double *pool, *cur, *tmp, *dx[3], *dy[3], *sm[3], *wgt;
    unsigned char *use;
    double tmp_ill[3], final_ill[3];
    size_t it, i, x, y;
    int c, flag = 1;
    if (bytes == 0) return ALWAN_E_NOMEM;
    pool = (double *)ALWAN_ALLOC(bytes, sizeof(double));
    if (!pool) return ALWAN_E_NOMEM;
    cur = pool;
    tmp = pool + 3 * n;
    for (c = 0; c < 3; c++) {
        dx[c] = pool + (4 + (size_t)c) * n;
        dy[c] = pool + (7 + (size_t)c) * n;
        sm[c] = pool + (10 + (size_t)c) * n;
    }
    wgt = pool + 13 * n;
    use = (unsigned char *)(pool + 14 * n);
    alwan_cc_kernel(k0, fs, p->sigma, 0);
    alwan_cc_kernel(k1, fs, p->sigma, 1);
    for (i = 0; i < 3 * n; i++) cur[i] = img[i];
    for (c = 0; c < 3; c++) tmp_ill[c] = final_ill[c] = 1.0 / ALWAN_SQRT_F64(3.0);
    for (it = 0; it < iters && flag; it++) {
        double norm2 = 0.0, dot;
        double *plane = wgt; /* scratch for one channel, reused below */
        for (c = 0; c < 3; c++) {
            double const div = ALWAN_SQRT_F64(3.0) * tmp_ill[c];
            for (i = 0; i < n; i++) cur[i * 3 + (size_t)c] /= div;
        }
        for (c = 0; c < 3; c++) {
            for (i = 0; i < n; i++) plane[i] = cur[i * 3 + (size_t)c];
            alwan_cc_filter(dx[c], plane, tmp, w, h, k1, k0, fs);
            alwan_cc_filter(dy[c], plane, tmp, w, h, k0, k1, fs);
            if (p->edge_weight == ALWAN_EDGE_WEIGHT_SHADOW) alwan_cc_filter(sm[c], plane, tmp, w, h, k0, k0, fs);
        }
        /* the pixels that count this pass, and each pixel's weight */
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                size_t const k = y * w + x;
                double const cxp = (double)(x + 1), cyp = (double)(y + 1);
                double rw = ALWAN_SQRT_F64(dx[0][k] * dx[0][k] + dy[0][k] * dy[0][k]);
                double gwv = ALWAN_SQRT_F64(dx[1][k] * dx[1][k] + dy[1][k] * dy[1][k]);
                double bw = ALWAN_SQRT_F64(dx[2][k] * dx[2][k] + dy[2][k] * dy[2][k]);
                double mx = rw > gwv ? (rw > bw ? rw : bw) : (gwv > bw ? gwv : bw);
                double grad = ALWAN_SQRT_F64(rw * rw + gwv * gwv + bw * bw), var, wv;
                int keep = cxp > border && cxp < (double)w - border + 1.0 && cyp > border && cyp < (double)h - border + 1.0;
                if (mx < DBL_EPSILON) keep = 0;
                if (never && never[k]) keep = 0;
                use[k] = (unsigned char)keep;
                if (p->edge_weight == ALWAN_EDGE_WEIGHT_SHADOW) {
                    double const cn = ALWAN_SQRT_F64(sm[0][k] * sm[0][k] + sm[1][k] * sm[1][k] + sm[2][k] * sm[2][k]);
                    double sx = 0.0, sy = 0.0;
                    if (cn > 0.0) {
                        sx = (sm[0][k] * dx[0][k] + sm[1][k] * dx[1][k] + sm[2][k] * dx[2][k]) / cn;
                        sy = (sm[0][k] * dy[0][k] + sm[1][k] * dy[1][k] + sm[2][k] * dy[2][k]) / cn;
                    }
                    var = ALWAN_SQRT_F64(sx * sx + sy * sy);
                } else {
                    double const ox = (dx[0][k] + dx[1][k] + dx[2][k]) / ALWAN_SQRT_F64(3.0);
                    double const oy = (dy[0][k] + dy[1][k] + dy[2][k]) / ALWAN_SQRT_F64(3.0);
                    var = ALWAN_SQRT_F64(ox * ox + oy * oy);
                }
                wv = grad > 0.0 ? ALWAN_POW_F64(var / grad, kappa) : 0.0;
                if (wv > 1.0) wv = 1.0;
                /* keep the three weighted magnitudes where the derivatives were (no longer needed) */
                dx[0][k] = rw * wv;
                dx[1][k] = gwv * wv;
                dx[2][k] = bw * wv;
            }
        }
        for (c = 0; c < 3; c++) {
            double acc = 0.0;
            for (i = 0; i < n; i++) {
                if (!use[i]) continue;
                if (mink == 0.0) {
                    if (dx[c][i] > acc) acc = dx[c][i];
                } else if (mink == 1.0) {
                    acc += dx[c][i];
                } else {
                    acc += ALWAN_POW_F64(dx[c][i], mink);
                }
            }
            tmp_ill[c] = (mink == 0.0 || mink == 1.0) ? acc : ALWAN_POW_F64(acc, 1.0 / mink);
            norm2 += tmp_ill[c] * tmp_ill[c];
        }
        if (!(norm2 > 0.0) || !alwan_cc_finite(norm2)) {
            ALWAN_FREE(pool);
            return ALWAN_E_RANGE;
        }
        norm2 = ALWAN_SQRT_F64(norm2);
        for (c = 0; c < 3; c++) tmp_ill[c] /= norm2;
        norm2 = 0.0;
        for (c = 0; c < 3; c++) {
            final_ill[c] *= tmp_ill[c];
            norm2 += final_ill[c] * final_ill[c];
        }
        norm2 = ALWAN_SQRT_F64(norm2);
        for (c = 0; c < 3; c++) final_ill[c] /= norm2;
        {
            double const s = 1.0 / ALWAN_SQRT_F64(3.0);
            dot = tmp_ill[0] * s + tmp_ill[1] * s + tmp_ill[2] * s;
        }
        if (ALWAN_ACOS_F64(dot) / ALWAN_CC_PI * 180.0 < 0.05) flag = 0;
    }
    for (c = 0; c < 3; c++) out[c] = final_ill[c];
    ALWAN_FREE(pool);
    return ALWAN_OK;
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
    if (params->method != ALWAN_CONSTANCY_EDGE_FRAMEWORK) {
        /* the pixels marked by the caller or by saturation (dilated 3 x 3); no border */
        int any = 0;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                int never = exclude && exclude[y * exclude_row_stride + x];
                if (!never && params->saturation > 0.0) {
                    long dy, dx;
                    for (dy = -1; dy <= 1 && !never; dy++) {
                        for (dx = -1; dx <= 1 && !never; dx++) {
                            size_t const yy = alwan_cc_clamp((long)y + dy, height), xx = alwan_cc_clamp((long)x + dx, width);
                            double const *px = img + (yy * width + xx) * 3;
                            double const mx = px[0] > px[1] ? (px[0] > px[2] ? px[0] : px[2]) : (px[1] > px[2] ? px[1] : px[2]);
                            if (mx >= params->saturation) never = 1;
                        }
                    }
                }
                use[y * width + x] = (unsigned char)never;
                any |= never;
            }
        }
        st = params->method == ALWAN_CONSTANCY_GREY_PIXEL
                 ? alwan_cc_grey_pixel(out, img, any ? use : NULL, width, height, params)
                 : alwan_cc_weighted_ge(out, img, any ? use : NULL, width, height, params, fs);
        ALWAN_FREE(img);
        return st;
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
