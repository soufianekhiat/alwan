/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The local Laplacian filter: Paris, Hasinoff and Kautz, "Local Laplacian Filters:
 * Edge-aware Image Processing with a Laplacian Pyramid", SIGGRAPH 2011, computed the fast
 * way of Aubry, Paris, Hasinoff, Kautz and Durand, "Fast Local Laplacian Filters", ACM TOG
 * 2014. Every coefficient of the output's Laplacian pyramid is the coefficient of a copy of
 * the image remapped around that pixel's own value g0:
 *
 *     r(i) = g0 + sign(d) sigma (|d| / sigma)^alpha           for |d| <= sigma
 *     r(i) = g0 + sign(d) (beta (|d| - sigma) + sigma)        for |d| >  sigma,   d = i - g0
 *
 * so differences smaller than sigma (detail) are raised to alpha (alpha < 1 boosts them,
 * alpha > 1 smooths them) and larger ones (edges, the tonal range) are scaled by beta
 * (beta < 1 compresses the range) without halos. The fast form remaps the image at
 * `levels` reference values spread over its range and blends their Laplacian pyramids per
 * pixel with linear (hat) weights in g0.
 *
 * The reference is MATLAB's locallapfilt (Image Processing Toolbox); its source calls four
 * compiled builtins, whose behaviour was read by probing them and which this reproduces:
 *   - pyramid levels: floor(log2(min(w, h))) + 1; a level is ceil(n / 2) of the one below;
 *   - down: the 5-tap binomial-like kernel [.05 .25 .4 .25 .05] (Burt and Adelson, a = 0.4)
 *     separably, keeping even samples, the border extended by half-sample symmetry;
 *   - up: the same kernel with gain 2, the coarse level extended by half-sample symmetry,
 *     to the size of the finer level;
 *   - remap: the formula above, except that for alpha < 1 the detail curve is blended
 *     back to the identity at the noise level, as the paper's own code does:
 *     tau sigma (|d| / sigma)^alpha + (1 - tau) |d| with tau = t^2 (t - 2)^2,
 *     t = clamp((|d| - 0.01) / 0.01, 0, 1), so differences under 0.01 (in the data's
 *     units, whatever sigma) are not amplified;
 *   - blend: level i gains max(0, 1 - |G_i - ref| / delta) (R_i - up(R_i+1)) for each
 *     reference ref, delta the spacing; the coarsest level is the input's own;
 *   - `levels` 0 picks MATLAB's count from alpha: 50 below 0.1, 16 from 0.9, linear in
 *     between; one level remaps the whole image around its mid-range;
 *   - colour, luminance (the default): the filter runs on
 *     Y = 0.298936021293776 R + 0.587043074451121 G + 0.114020904255103 B and every channel
 *     is scaled by filtered Y / (Y + FLT_EPSILON); separate_channels filters each channel;
 *   - alpha = beta = 1, or sigma = 0 with beta = 1, returns the input.
 * MATLAB computes in single precision; this computes in double. MATLAB's remap is 0 / 0 at
 * d = 0 when sigma = 0; here it is the limit, g0.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

#define ALWAN_LLF_MAX_LEVELS 64
#define ALWAN_LLF_LUMINANCE 0
#define ALWAN_LLF_SEPARATE 1
#define ALWAN_LLF_NOISE 0.01 /* Paris et al.'s noise level, in the data's units */

static double const alwan_llf_k[5] = { 0.05, 0.25, 0.4, 0.25, 0.05 };

static int alwan_llf_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

/* Half-sample symmetric extension: -1 -> 0, n -> n - 1. */
static long alwan_llf_sym(long i, long n) {
    while (i < 0 || i >= n) {
        if (i < 0) i = -i - 1;
        if (i >= n) i = 2 * n - 1 - i;
    }
    return i;
}

/* dst (dw x dh, dw = ceil(sw / 2), dh = ceil(sh / 2)) from src (sw x sh); tmp dw x sh. */
static void alwan_llf_down(double *dst, double const *src, long sw, long sh, double *tmp) {
    long const dw = (sw + 1) / 2, dh = (sh + 1) / 2;
    long x, y, j;
    for (y = 0; y < sh; y++) {
        for (x = 0; x < dw; x++) {
            double s = 0.0;
            for (j = -2; j <= 2; j++) s += alwan_llf_k[j + 2] * src[y * sw + alwan_llf_sym(2 * x + j, sw)];
            tmp[y * dw + x] = s;
        }
    }
    for (y = 0; y < dh; y++) {
        for (x = 0; x < dw; x++) {
            double s = 0.0;
            for (j = -2; j <= 2; j++) s += alwan_llf_k[j + 2] * tmp[alwan_llf_sym(2 * y + j, sh) * dw + x];
            dst[y * dw + x] = s;
        }
    }
}

/* dst (dw x dh) from the coarser src (sw x sh); tmp dw x sh. */
static void alwan_llf_up(double *dst, long dw, long dh, double const *src, long sw, long sh, double *tmp) {
    long x, y, j;
    for (y = 0; y < sh; y++) {
        for (x = 0; x < dw; x++) {
            double s = 0.0;
            long const j0 = (x - 1) / 2 - (x < 1 ? 1 : 0), j1 = (x + 2) / 2;
            for (j = j0; j <= j1; j++) {
                long const t = x - 2 * j;
                if (t >= -2 && t <= 2) s += 2.0 * alwan_llf_k[t + 2] * src[y * sw + alwan_llf_sym(j, sw)];
            }
            tmp[y * dw + x] = s;
        }
    }
    for (y = 0; y < dh; y++) {
        long const j0 = (y - 1) / 2 - (y < 1 ? 1 : 0), j1 = (y + 2) / 2;
        for (x = 0; x < dw; x++) {
            double s = 0.0;
            for (j = j0; j <= j1; j++) {
                long const t = y - 2 * j;
                if (t >= -2 && t <= 2) s += 2.0 * alwan_llf_k[t + 2] * tmp[alwan_llf_sym(j, sh) * dw + x];
            }
            dst[y * dw + x] = s;
        }
    }
}

static double alwan_llf_remap(double i, double g0, double sigma, double alpha, double beta) {
    double const d = i - g0, ad = fabs(d), s = d < 0.0 ? -1.0 : 1.0;
    if (ad == 0.0) return g0;
    if (ad <= sigma) {
        double v = sigma * pow(ad / sigma, alpha);
        if (alpha < 1.0) { /* leave differences at the noise level alone */
            double t = (ad - ALWAN_LLF_NOISE) / ALWAN_LLF_NOISE;
            t = t < 0.0 ? 0.0 : t > 1.0 ? 1.0 : t;
            t = t * t * (t - 2.0) * (t - 2.0);
            v = t * v + (1.0 - t) * ad;
        }
        return g0 + s * v;
    }
    return g0 + s * (beta * (ad - sigma) + sigma);
}

typedef struct {
    long w[ALWAN_LLF_MAX_LEVELS], h[ALWAN_LLF_MAX_LEVELS];
    size_t off[ALWAN_LLF_MAX_LEVELS];
    int n;
    size_t total;
} alwan_llf_pyr;

/* One plane, in place: plane holds w x h values; work holds 3 pyramids and two scratch
 * planes, as sized by the caller. */
static void alwan_llf_plane(double *plane, alwan_llf_pyr const *py, double *ing, double *rg, double *outl, double *tmp,
                            double *up, double sigma, double alpha, double beta, int levels) {
    size_t const n0 = (size_t)py->w[0] * (size_t)py->h[0];
    double mn = plane[0], mx = plane[0], delta;
    size_t p;
    int i, k;
    for (p = 1; p < n0; p++) {
        if (plane[p] < mn) mn = plane[p];
        if (plane[p] > mx) mx = plane[p];
    }
    if (levels == 1) {
        double const ref = (mn + mx) / 2.0;
        for (p = 0; p < n0; p++) plane[p] = alwan_llf_remap(plane[p], ref, sigma, alpha, beta);
        return;
    }
    if (mn == mx) return;
    memcpy(ing, plane, n0 * sizeof(double));
    for (i = 1; i < py->n; i++) {
        alwan_llf_down(ing + py->off[i], ing + py->off[i - 1], py->w[i - 1], py->h[i - 1], tmp);
    }
    memset(outl, 0, py->total * sizeof(double));
    memcpy(outl + py->off[py->n - 1], ing + py->off[py->n - 1],
           (size_t)py->w[py->n - 1] * (size_t)py->h[py->n - 1] * sizeof(double));
    delta = (mx - mn) / (double)(levels - 1);
    for (k = 0; k < levels; k++) {
        double const ref = mn + (double)k * delta;
        for (p = 0; p < n0; p++) rg[p] = alwan_llf_remap(plane[p], ref, sigma, alpha, beta);
        for (i = 1; i < py->n; i++) {
            alwan_llf_down(rg + py->off[i], rg + py->off[i - 1], py->w[i - 1], py->h[i - 1], tmp);
        }
        for (i = py->n - 2; i >= 0; i--) {
            size_t const ni = (size_t)py->w[i] * (size_t)py->h[i];
            double const *g = ing + py->off[i], *r = rg + py->off[i];
            double *o = outl + py->off[i];
            alwan_llf_up(up, py->w[i], py->h[i], rg + py->off[i + 1], py->w[i + 1], py->h[i + 1], tmp);
            for (p = 0; p < ni; p++) {
                double const wgt = 1.0 - fabs(g[p] - ref) / delta;
                if (wgt > 0.0) o[p] += wgt * (r[p] - up[p]);
            }
        }
    }
    for (i = py->n - 2; i >= 0; i--) {
        size_t const ni = (size_t)py->w[i] * (size_t)py->h[i];
        double *o = outl + py->off[i];
        alwan_llf_up(up, py->w[i], py->h[i], outl + py->off[i + 1], py->w[i + 1], py->h[i + 1], tmp);
        for (p = 0; p < ni; p++) o[p] += up[p];
    }
    memcpy(plane, outl, n0 * sizeof(double));
}

alwan_status alwan__llf_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t ch,
                                  size_t w, size_t h, double sigma, double alpha, double beta, size_t levels_in,
                                  int mode, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    int const luminance = mode == ALWAN_LLF_LUMINANCE && ch == 3;
    size_t const planes = luminance ? 4 : ch;
    alwan_llf_pyr py;
    double *data, *ing, *rg, *outl, *tmp, *up;
    size_t x, y, c, m;
    int levels, identity;
    if (!out || !src) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (mode != ALWAN_LLF_LUMINANCE && mode != ALWAN_LLF_SEPARATE) return ALWAN_E_INVALID;
    if (mode == ALWAN_LLF_LUMINANCE && ch != 1 && ch != 3) return ALWAN_E_INVALID;
    if (src_row_stride / elem / ch < w || out_row_stride / elem / ch < w) return ALWAN_E_INVALID;
    if (!alwan_llf_finite(sigma) || !alwan_llf_finite(alpha) || !alwan_llf_finite(beta)) return ALWAN_E_INVALID;
    if (!(sigma >= 0.0) || !(alpha > 0.0) || !(beta >= 0.0) || levels_in > 1000 || n / w != h) return ALWAN_E_RANGE;

    if (levels_in == 0) {
        float const a = (float)alpha;
        if (a < 0.1f) levels = 50;
        else if (a < 0.9f) levels = (int)floorf(((float)43.4 - 34.0f * a) / (float)0.8 + 0.5f);
        else levels = 16;
    } else {
        levels = (int)levels_in;
    }
    identity = (alpha == 1.0 && beta == 1.0) || (sigma == 0.0 && beta == 1.0);

    /* pyramid geometry */
    {
        size_t mn = w < h ? w : h;
        int nl = 1;
        while (mn >= 2) { mn >>= 1; nl++; }
        py.n = nl;
        py.w[0] = (long)w; py.h[0] = (long)h; py.off[0] = 0;
        py.total = n;
        for (m = 1; m < (size_t)nl; m++) {
            py.w[m] = (py.w[m - 1] + 1) / 2;
            py.h[m] = (py.h[m - 1] + 1) / 2;
            py.off[m] = py.total;
            py.total += (size_t)py.w[m] * (size_t)py.h[m];
        }
    }

    data = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, planes * sizeof(double)), sizeof(double));
    ing = (double *)ALWAN_ALLOC(alwan_safe_array_size(py.total, 3 * sizeof(double)), sizeof(double));
    tmp = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * sizeof(double)), sizeof(double));
    if (!data || !ing || !tmp) {
        if (data) ALWAN_FREE(data);
        if (ing) ALWAN_FREE(ing);
        if (tmp) ALWAN_FREE(tmp);
        return ALWAN_E_NOMEM;
    }
    rg = ing + py.total;
    outl = rg + py.total;
    up = tmp + n;

    for (y = 0; y < h; y++) {
        char const *srow = (char const *)src + y * src_row_stride;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double const v = is_f32 ? (double)((alwan_f32 const *)srow)[x * ch + c] : ((alwan_f64 const *)srow)[x * ch + c];
                if (!alwan_llf_finite(v)) goto invalid;
                data[c * n + y * w + x] = v;
            }
        }
    }

    if (!identity) {
        if (luminance) {
            double *lum = data + 3 * n;
            for (m = 0; m < n; m++) {
                lum[m] = 0.298936021293776 * data[m] + 0.587043074451121 * data[n + m] + 0.114020904255103 * data[2 * n + m];
            }
            for (m = 0; m < n; m++) {
                double const inv = 1.0 / (lum[m] + (double)FLT_EPSILON);
                data[m] *= inv; data[n + m] *= inv; data[2 * n + m] *= inv;
            }
            alwan_llf_plane(lum, &py, ing, rg, outl, tmp, up, sigma, alpha, beta, levels);
            for (m = 0; m < n; m++) {
                data[m] *= lum[m]; data[n + m] *= lum[m]; data[2 * n + m] *= lum[m];
            }
        } else {
            for (c = 0; c < ch; c++) alwan_llf_plane(data + c * n, &py, ing, rg, outl, tmp, up, sigma, alpha, beta, levels);
        }
    }

    for (y = 0; y < h; y++) {
        char *orow = (char *)out + y * out_row_stride;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double const v = data[c * n + y * w + x];
                if (is_f32) ((alwan_f32 *)orow)[x * ch + c] = (alwan_f32)v;
                else ((alwan_f64 *)orow)[x * ch + c] = v;
            }
        }
    }
    ALWAN_FREE(data);
    ALWAN_FREE(ing);
    ALWAN_FREE(tmp);
    return ALWAN_OK;
invalid:
    ALWAN_FREE(data);
    ALWAN_FREE(ing);
    ALWAN_FREE(tmp);
    return ALWAN_E_INVALID;
}
