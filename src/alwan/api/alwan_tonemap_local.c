/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Mantiuk 2006: tone mapping in the contrast domain.
 *
 * Every other tone mapper alwan has is a curve. This one is not: it takes the log
 * luminance apart into contrasts at every scale, shrinks each of them, and then asks
 * what image has those contrasts. That last question has no closed-form answer, so it
 * is a least-squares problem, solved here by conjugate gradients.
 *
 *   1. build a pyramid of the log luminance by halving it repeatedly
 *   2. at each level take forward differences in x and in y: the contrasts
 *   3. map each contrast through s(c) = sign(c) (|c|^p * scale)^(1/p), p = 0.4185,
 *      which is the response curve, so a small contrast is reduced less than a large
 *      one and the picture keeps its texture while losing its range
 *   4. assemble the divergence of the mapped contrasts over all the levels: the
 *      right-hand side
 *   5. solve A x = b for the log luminance whose contrasts those are, A being the same
 *      assemble-the-divergence operator run on x
 *   6. exponentiate, and put the colour back at the new luminance
 *
 * A HAS A NULL SPACE: adding a constant to x changes no contrast, so the solution is
 * fixed only up to overall level. Conjugate gradients starting from the original log
 * luminance never leaves that starting level, so the result keeps the image's own mean
 * log luminance rather than floating free. That is why the iteration starts where it
 * does, and it is the reason this operator needs no normalisation to be meaningful.
 *
 * THE REFERENCE IS OpenCV's TonemapMantiuk, and this reproduces it to 2e-06 in float32,
 * which is that type's precision. Reproducing it means reproducing its choices, and two
 * of them are not the obvious ones:
 *
 *   the pyramid halves with a BILINEAR RESIZE, not with a Gaussian pyrDown, so the
 *   Mertens machinery already in alwan is the wrong tool and this file has its own;
 *
 *   the number of levels is (int)(logf(min(w, h)) / logf(2)) computed in FLOAT, which
 *   is reproduced exactly rather than replaced with an integer log, because a float
 *   that lands a hair under an integer would change the level count and the answer.
 *
 * The conjugate gradients stop on a relative residual of 1e-3, not on an iteration
 * count, and in practice reach it in five to ten steps. That matters for whether this
 * can be checked at all: a solver that stopped on a count would make the answer a
 * property of OpenCV's loop rather than of the equations, and no second implementation
 * could agree with it. The cap of 100 is a guard, and suite 170 reports how many steps
 * were actually used.
 *
 * Published in Mantiuk, Myszkowski and Seidel, "A Perceptual Framework for Contrast
 * Processing of High Dynamic Range Images", ACM TAP 3(3), 2006.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>
#include <math.h>
#include <float.h>

#define ALWAN_MTK_MAX_LEVELS 32
#define ALWAN_MTK_RESPONSE_POWER 0.4185
#define ALWAN_MTK_MAX_ITER 100
#define ALWAN_MTK_TARGET_ERROR 1e-3

typedef struct {
    size_t w[ALWAN_MTK_MAX_LEVELS];
    size_t h[ALWAN_MTK_MAX_LEVELS];
    double *xc[ALWAN_MTK_MAX_LEVELS];   /* h[i] rows of w[i]: the x contrasts */
    double *yc[ALWAN_MTK_MAX_LEVELS];   /* w[i] rows of h[i]: the y contrasts, transposed */
    int levels;
    double *layer;                      /* scratch for the pyramid walk */
    double *layer2;
    double *acc;                        /* scratch for the assembled divergence */
    double *acc2;
    double *pool;                       /* the one allocation the rest points into */
} alwan_mtk;

static int alwan_mtk_finite(double v) {
    return (v == v) && (v > -DBL_MAX) && (v < DBL_MAX);
}

/* OpenCV's INTER_LINEAR for a float image: a destination centre maps to a source
 * centre, fx = (x + 0.5) * scale - 0.5, and the fraction is dropped where that falls
 * outside the source rather than extrapolated. */
static void alwan_mtk_resize(double *dst, size_t dw, size_t dh,
                             double const *src, size_t sw, size_t sh) {
    double const scale_x = (double)sw / (double)dw;
    double const scale_y = (double)sh / (double)dh;
    size_t x, y;
    for (y = 0; y < dh; y++) {
        double fy = ((double)y + 0.5) * scale_y - 0.5;
        ptrdiff_t iy = (ptrdiff_t)floor(fy);
        size_t iy0, iy1;
        fy -= (double)iy;
        if (iy < 0) { iy = 0; fy = 0.0; }
        if ((size_t)iy >= sh - 1) { iy = (ptrdiff_t)(sh > 0 ? sh - 1 : 0); fy = 0.0; }
        iy0 = (size_t)iy;
        iy1 = iy0 + 1 < sh ? iy0 + 1 : iy0;
        for (x = 0; x < dw; x++) {
            double fx = ((double)x + 0.5) * scale_x - 0.5;
            ptrdiff_t ix = (ptrdiff_t)floor(fx);
            size_t ix0, ix1;
            double a, b, c, d;
            fx -= (double)ix;
            if (ix < 0) { ix = 0; fx = 0.0; }
            if ((size_t)ix >= sw - 1) { ix = (ptrdiff_t)(sw > 0 ? sw - 1 : 0); fx = 0.0; }
            ix0 = (size_t)ix;
            ix1 = ix0 + 1 < sw ? ix0 + 1 : ix0;
            a = src[iy0 * sw + ix0];
            b = src[iy0 * sw + ix1];
            c = src[iy1 * sw + ix0];
            d = src[iy1 * sw + ix1];
            dst[y * dw + x] = (a * (1.0 - fx) + b * fx) * (1.0 - fy)
                            + (c * (1.0 - fx) + d * fx) * fy;
        }
    }
}

/* Forward differences along the row. pos 0 leaves the last column zero; pos 1 shifts
 * the differences right by one and puts the source's first column in column zero,
 * which is how the divergence gets its boundary term. */
static void alwan_mtk_grad(double *dst, double const *src, size_t w, size_t h, int pos) {
    size_t x, y;
    memset(dst, 0, w * h * sizeof(double));
    if (w < 2) return;
    for (y = 0; y < h; y++) {
        double const *s = src + y * w;
        double *d = dst + y * w;
        for (x = 0; x + 1 < w; x++) d[x + (size_t)pos] = s[x + 1] - s[x];
        if (pos == 1) d[0] = s[0];
    }
}

static void alwan_mtk_transpose(double *dst, double const *src, size_t w, size_t h) {
    size_t x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) dst[x * h + y] = src[y * w + x];
    }
}

/* sign(v) |v|^p, with a zero counting as negative exactly as OpenCV's (src > 0) does.
 * It makes no difference to the value, since |0|^p is zero, and it is written this way
 * so the two implementations cannot drift apart on it. */
static double alwan_mtk_signed_pow(double v, double p) {
    double const m = ALWAN_POW_F64(fabs(v), p);
    return v > 0.0 ? m : -m;
}

/* The contrasts of src at every level, left in ctx. */
static void alwan_mtk_contrast(alwan_mtk *ctx, double const *src) {
    size_t const w0 = ctx->w[0], h0 = ctx->h[0];
    double *layer = ctx->layer, *next = ctx->layer2;
    int i;
    memcpy(layer, src, w0 * h0 * sizeof(double));
    for (i = 0; i < ctx->levels; i++) {
        size_t const w = ctx->w[i], h = ctx->h[i];
        alwan_mtk_grad(ctx->xc[i], layer, w, h, 0);
        alwan_mtk_transpose(ctx->acc, layer, w, h);        /* acc is h rows of ... */
        alwan_mtk_grad(ctx->yc[i], ctx->acc, h, w, 0);     /* ... so difference down h */
        if (i + 1 < ctx->levels) {
            alwan_mtk_resize(next, ctx->w[i + 1], ctx->h[i + 1], layer, w, h);
            { double *t = layer; layer = next; next = t; }
        }
    }
}

/* The divergence of the contrasts now in ctx, accumulated from the smallest level up. */
static void alwan_mtk_sum(alwan_mtk *ctx, double *out) {
    int const last = ctx->levels - 1;
    double *acc = ctx->acc, *tmp = ctx->acc2;
    int i;
    memset(acc, 0, ctx->w[last] * ctx->h[last] * sizeof(double));
    for (i = last; i >= 0; i--) {
        size_t const w = ctx->w[i], h = ctx->h[i];
        size_t x, y;
        if (i != last) {
            alwan_mtk_resize(tmp, w, h, acc, ctx->w[i + 1], ctx->h[i + 1]);
            { double *t = acc; acc = tmp; tmp = t; }
        }
        /* The x term in place, then the y term read back out of its transpose. */
        alwan_mtk_grad(ctx->layer, ctx->xc[i], w, h, 1);
        alwan_mtk_grad(ctx->layer2, ctx->yc[i], h, w, 1);
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                acc[y * w + x] += ctx->layer[y * w + x] + ctx->layer2[x * h + y];
            }
        }
    }
    memcpy(out, acc, ctx->w[0] * ctx->h[0] * sizeof(double));
}

/* A x: the same assemble-the-divergence operator, with no contrast mapping in it, which
 * is what makes it linear and the problem a least-squares one. */
static void alwan_mtk_product(alwan_mtk *ctx, double *out, double const *x) {
    alwan_mtk_contrast(ctx, x);
    alwan_mtk_sum(ctx, out);
}

static double alwan_mtk_dot(double const *a, double const *b, size_t n) {
    double s = 0.0;
    size_t i;
    for (i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

/* ---------------------------------------------------------------- the operator */

static alwan_status alwan_mtk_run(double *rgb, size_t width, size_t height,
                                  double const *weights, double scale, double saturation,
                                  int *iterations_out) {
    alwan_mtk ctx;
    size_t const n = width * height;
    double *gray = NULL, *x = NULL, *r = NULL, *p = NULL, *prod = NULL, *right = NULL;
    double *pool = NULL;
    size_t need = 0, level_px = 0;
    double rr, target;
    alwan_status st = ALWAN_OK;
    size_t i;
    int it, used = 0;

    memset(&ctx, 0, sizeof ctx);
    if (width < 2 || height < 2) return ALWAN_E_RANGE;

    /* The level count exactly as OpenCV computes it, in float, because a value a hair
     * under an integer would drop a level and change the answer. */
    {
        float const m = (float)(width < height ? width : height);
        int levels = (int)(logf(m) / logf(2.0f));
        if (levels < 1) levels = 1;
        if (levels > ALWAN_MTK_MAX_LEVELS) levels = ALWAN_MTK_MAX_LEVELS;
        ctx.levels = levels;
    }
    ctx.w[0] = width;
    ctx.h[0] = height;
    for (i = 1; i < (size_t)ctx.levels; i++) {
        ctx.w[i] = ctx.w[i - 1] / 2;
        ctx.h[i] = ctx.h[i - 1] / 2;
        if (ctx.w[i] < 1 || ctx.h[i] < 1) { ctx.levels = (int)i; break; }
    }
    for (i = 0; i < (size_t)ctx.levels; i++) level_px += ctx.w[i] * ctx.h[i];

    /* One allocation: two contrast pyramids, four scratch planes, and the five vectors
     * the iteration needs. */
    need = 2u * level_px + 4u * n + 6u * n;
    pool = (double *)ALWAN_ALLOC(alwan_safe_array_size(need, sizeof(double)), sizeof(double));
    if (!pool) return ALWAN_E_NOMEM;
    ctx.pool = pool;
    {
        double *q = pool;
        for (i = 0; i < (size_t)ctx.levels; i++) { ctx.xc[i] = q; q += ctx.w[i] * ctx.h[i]; }
        for (i = 0; i < (size_t)ctx.levels; i++) { ctx.yc[i] = q; q += ctx.w[i] * ctx.h[i]; }
        ctx.layer = q;  q += n;
        ctx.layer2 = q; q += n;
        ctx.acc = q;    q += n;
        ctx.acc2 = q;   q += n;
        gray = q;  q += n;
        x = q;     q += n;
        r = q;     q += n;
        p = q;     q += n;
        prod = q;  q += n;
        right = q;
    }

    /* Luminance, and the log of it with OpenCV's floor of 1e-4 so a black pixel has a
     * finite log. */
    for (i = 0; i < n; i++) {
        double const L = weights[0] * rgb[i * 3] + weights[1] * rgb[i * 3 + 1]
                       + weights[2] * rgb[i * 3 + 2];
        if (!alwan_mtk_finite(L) || L < 0.0) { ALWAN_FREE(pool); return ALWAN_E_INVALID; }
        gray[i] = L;
        x[i] = ALWAN_LN_F64(L > 1e-4 ? L : 1e-4);
    }

    /* The contrasts, mapped through the response curve, and their divergence. */
    alwan_mtk_contrast(&ctx, x);
    for (i = 0; i < (size_t)ctx.levels; i++) {
        size_t const c = ctx.w[i] * ctx.h[i];
        size_t k;
        for (k = 0; k < c; k++) {
            double v = alwan_mtk_signed_pow(ctx.xc[i][k], ALWAN_MTK_RESPONSE_POWER) * scale;
            ctx.xc[i][k] = alwan_mtk_signed_pow(v, 1.0 / ALWAN_MTK_RESPONSE_POWER);
            v = alwan_mtk_signed_pow(ctx.yc[i][k], ALWAN_MTK_RESPONSE_POWER) * scale;
            ctx.yc[i][k] = alwan_mtk_signed_pow(v, 1.0 / ALWAN_MTK_RESPONSE_POWER);
        }
    }
    alwan_mtk_sum(&ctx, right);

    /* Conjugate gradients from the image's own log luminance, which is what anchors
     * the level: A has constants in its null space and the iteration never leaves the
     * level it starts at. */
    alwan_mtk_product(&ctx, r, x);
    for (i = 0; i < n; i++) r[i] = right[i] - r[i];
    memcpy(p, r, n * sizeof(double));
    target = alwan_mtk_dot(right, right, n)
           * (ALWAN_MTK_TARGET_ERROR * ALWAN_MTK_TARGET_ERROR);
    rr = alwan_mtk_dot(r, r, n);
    for (it = 0; it < ALWAN_MTK_MAX_ITER; it++) {
        double alpha, new_rr, denom;
        alwan_mtk_product(&ctx, prod, p);
        denom = alwan_mtk_dot(p, prod, n);
        used = it + 1;
        if (denom == 0.0 || !alwan_mtk_finite(denom)) break;
        alpha = rr / denom;
        for (i = 0; i < n; i++) {
            r[i] -= alpha * prod[i];
            x[i] += alpha * p[i];
        }
        new_rr = alwan_mtk_dot(r, r, n);
        if (rr == 0.0) break;
        for (i = 0; i < n; i++) p[i] = r[i] + (new_rr / rr) * p[i];
        rr = new_rr;
        if (rr < target) break;
    }

    /* The new luminance, and the colour put back at it. A pixel with no light has no
     * ratio to preserve and stays black. */
    for (i = 0; i < n; i++) {
        double const new_l = ALWAN_EXP_F64(x[i]);
        int ch;
        for (ch = 0; ch < 3; ch++) {
            double const c = gray[i] > 0.0 ? rgb[i * 3 + ch] / gray[i] : 0.0;
            rgb[i * 3 + ch] = (c > 0.0 ? ALWAN_POW_F64(c, saturation) : 0.0) * new_l;
        }
    }
    if (iterations_out) *iterations_out = used;
    ALWAN_FREE(pool);
    return st;
}

/* ---------------------------------------------------------------- entry */

static alwan_status alwan_mtk_params(double *weights, double *scale, double *saturation,
                                     double in_w0, double in_w1, double in_w2,
                                     double in_scale, double in_sat) {
    if (in_w0 == 0.0 && in_w1 == 0.0 && in_w2 == 0.0) {
        alwan_rgb_space_desc_f64 desc;
        alwan_mat3x3_f64 to_xyz, from_xyz;
        int i;
        if (alwan_rgb_get_space_descriptor_f64(&desc, ALWAN_RGB_SPACE_SRGB, NULL) != ALWAN_OK
            || alwan_rgb_derive_matrices_f64(&to_xyz, &from_xyz, &desc) != ALWAN_OK) {
            return ALWAN_E_INVALID;
        }
        for (i = 0; i < 3; i++) weights[i] = (double)to_xyz.m[3 + i];
    } else {
        weights[0] = in_w0;
        weights[1] = in_w1;
        weights[2] = in_w2;
    }
    *scale = in_scale == 0.0 ? 0.7 : in_scale;
    *saturation = in_sat == 0.0 ? 1.0 : in_sat;
    if (!(*scale > 0.0) || !(*saturation >= 0.0)) return ALWAN_E_INVALID;
    if (!alwan_mtk_finite(weights[0]) || !alwan_mtk_finite(weights[1])
        || !alwan_mtk_finite(weights[2])) {
        return ALWAN_E_INVALID;
    }
    return ALWAN_OK;
}

alwan_status alwan_tonemap_mantiuk2006_f64(alwan_f64 *rgb_out, size_t out_row_stride,
                                           alwan_f64 const *rgb_in, size_t in_row_stride,
                                           size_t width, size_t height,
                                           alwan_tonemap_local_params_f64 const *params,
                                           int *iterations_out) {
    double weights[3], scale, saturation;
    double *buf;
    alwan_status st;
    size_t y, x;
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    if (width < 2 || height < 2) return ALWAN_E_RANGE;
    st = alwan_mtk_params(weights, &scale, &saturation,
                          params ? (double)params->luminance_weights[0] : 0.0,
                          params ? (double)params->luminance_weights[1] : 0.0,
                          params ? (double)params->luminance_weights[2] : 0.0,
                          params ? (double)params->scale : 0.0,
                          params ? (double)params->saturation : 0.0);
    if (st != ALWAN_OK) return st;

    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(width * height * 3u, sizeof(double)),
                                sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    for (y = 0; y < height; y++) {
        alwan_f64 const *s = (alwan_f64 const *)((char const *)rgb_in + y * in_row_stride);
        for (x = 0; x < width * 3u; x++) buf[y * width * 3u + x] = (double)s[x];
    }
    st = alwan_mtk_run(buf, width, height, weights, scale, saturation, iterations_out);
    if (st == ALWAN_OK) {
        for (y = 0; y < height; y++) {
            alwan_f64 *d = (alwan_f64 *)((char *)rgb_out + y * out_row_stride);
            for (x = 0; x < width * 3u; x++) d[x] = (alwan_f64)buf[y * width * 3u + x];
        }
    }
    ALWAN_FREE(buf);
    return st;
}

alwan_status alwan_tonemap_mantiuk2006_f32(alwan_f32 *rgb_out, size_t out_row_stride,
                                           alwan_f32 const *rgb_in, size_t in_row_stride,
                                           size_t width, size_t height,
                                           alwan_tonemap_local_params_f32 const *params,
                                           int *iterations_out) {
    double weights[3], scale, saturation;
    double *buf;
    alwan_status st;
    size_t y, x;
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    if (width < 2 || height < 2) return ALWAN_E_RANGE;
    st = alwan_mtk_params(weights, &scale, &saturation,
                          params ? (double)params->luminance_weights[0] : 0.0,
                          params ? (double)params->luminance_weights[1] : 0.0,
                          params ? (double)params->luminance_weights[2] : 0.0,
                          params ? (double)params->scale : 0.0,
                          params ? (double)params->saturation : 0.0);
    if (st != ALWAN_OK) return st;

    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(width * height * 3u, sizeof(double)),
                                sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    for (y = 0; y < height; y++) {
        alwan_f32 const *s = (alwan_f32 const *)((char const *)rgb_in + y * in_row_stride);
        for (x = 0; x < width * 3u; x++) buf[y * width * 3u + x] = (double)s[x];
    }
    st = alwan_mtk_run(buf, width, height, weights, scale, saturation, iterations_out);
    if (st == ALWAN_OK) {
        for (y = 0; y < height; y++) {
            alwan_f32 *d = (alwan_f32 *)((char *)rgb_out + y * out_row_stride);
            for (x = 0; x < width * 3u; x++) d[x] = (alwan_f32)buf[y * width * 3u + x];
        }
    }
    ALWAN_FREE(buf);
    return st;
}
