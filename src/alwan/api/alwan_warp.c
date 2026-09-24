/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_warp_{T} and _u8: an image resampled through an affine or perspective map, as
 * Pillow's Image.transform computes it (libImaging Geometry.c), which suite 234 holds this
 * to.
 *
 * Every output pixel's centre (x + 0.5, y + 0.5) is carried into the source by the
 * coefficients a: affine, (a0 x + a1 y + a2, a3 x + a4 y + a5); perspective, both divided
 * by a6 x + a7 y + 1. The source is sampled there:
 *
 *   NEAREST   the pixel under the point; for affine maps Pillow's three routes are
 *             followed: a pure scale by its pretabulated running sums, a rotation in its
 *             16.16 fixed point while every corner maps inside +-32768, otherwise its
 *             running sums in double
 *   BILINEAR  the four pixels around it, the columns clamped at the edges and a missing
 *             lower row taken as the upper
 *   BICUBIC   the sixteen around it by the Catmull-Rom cubic (a = -0.5) in Pillow's
 *             Horner form, the same edge rules
 *
 * A point outside the image leaves the output at `fill`. 8-bit results are truncated
 * (bicubic clamped first), float32 results stored from double, as Pillow's are.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

#define ALWAN_WP_COORD(v) ((v) < 0.0 ? -1 : (long)(v))
#define ALWAN_WP_FLOOR(v) ((v) < 0.0 ? (long)floor(v) : (long)(v))

typedef struct {
    void const *src;
    size_t rs, ch, w, h;
    int kind;
} alwan_wp_img;

static double alwan_wp_px(alwan_wp_img const *im, long x, long y, size_t c) {
    char const *row = (char const *)im->src + (size_t)y * im->rs;
    size_t const i = (size_t)x * im->ch + c;
    return im->kind == 0 ? ((alwan_f64 const *)row)[i] : im->kind == 1 ? (double)((alwan_f32 const *)row)[i]
                                                                       : (double)((unsigned char const *)row)[i];
}

static long alwan_wp_xclip(alwan_wp_img const *im, long x) {
    return x < 0 ? 0 : x < (long)im->w ? x : (long)im->w - 1;
}

static void alwan_wp_store(void *out, int kind, size_t i, double v, int cubic) {
    if (kind == 0) ((alwan_f64 *)out)[i] = v;
    else if (kind == 1) ((alwan_f32 *)out)[i] = (alwan_f32)v;
    else if (cubic) ((unsigned char *)out)[i] = (unsigned char)(v <= 0.0 ? 0.0 : v >= 255.0 ? 255.0 : v);
    else ((unsigned char *)out)[i] = (unsigned char)v;
}

static double alwan_wp_r32(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

/* Pillow's BICUBIC macro. On float32 pixels (f32 set) its p2, p3 and p4 are C float
 * arithmetic, the operands being FLOAT32; the Horner step with the double d is double. */
static double alwan_wp_cubic(double v1, double v2, double v3, double v4, double d, int f32) {
    double const p1 = v2;
    double const p2 = alwan_wp_r32(-v1 + v3, f32);
    double const p3 = alwan_wp_r32(alwan_wp_r32(alwan_wp_r32(2 * alwan_wp_r32(v1 - v2, f32) + v3, f32), f32) - v4, f32);
    double const p4 = alwan_wp_r32(alwan_wp_r32(alwan_wp_r32(-v1 + v2, f32) - v3, f32) + v4, f32);
    return p1 + d * (p2 + d * (p3 + d * p4));
}

/* Pillow's bilinear and bicubic filters: 0 when (xin, yin) is outside, else 1 and out set. */
static int alwan_wp_sample(void *orow, size_t ox, alwan_wp_img const *im, double xin, double yin, int cubic) {
    long x, y;
    double dx, dy;
    size_t c;
    int const f32 = im->kind == 1;
    if (xin < 0.0 || xin >= (double)im->w || yin < 0.0 || yin >= (double)im->h) return 0;
    xin -= 0.5;
    yin -= 0.5;
    x = ALWAN_WP_FLOOR(xin);
    y = ALWAN_WP_FLOOR(yin);
    dx = xin - (double)x;
    dy = yin - (double)y;
    if (!cubic) {
        long const x0 = alwan_wp_xclip(im, x), x1 = alwan_wp_xclip(im, x + 1);
        long const yc = y < 0 ? 0 : y < (long)im->h ? y : (long)im->h - 1;
        for (c = 0; c < im->ch; c++) {
            double v1, v2;
            double a = alwan_wp_px(im, x0, yc, c), b = alwan_wp_px(im, x1, yc, c);
            /* on float32 pixels (b - a) is float arithmetic, as in Pillow's BILINEAR macro */
            v1 = a + alwan_wp_r32(b - a, f32) * dx;
            if (y + 1 >= 0 && y + 1 < (long)im->h) {
                a = alwan_wp_px(im, x0, y + 1, c);
                b = alwan_wp_px(im, x1, y + 1, c);
                v2 = a + alwan_wp_r32(b - a, f32) * dx;
            } else {
                v2 = v1;
            }
            v1 = v1 + (v2 - v1) * dy;
            alwan_wp_store(orow, im->kind, ox * im->ch + c, v1, 0);
        }
    } else {
        long xs[4], k;
        long const yc = (y - 1) < 0 ? 0 : (y - 1) < (long)im->h ? (y - 1) : (long)im->h - 1;
        x--;
        y--;
        for (k = 0; k < 4; k++) xs[k] = alwan_wp_xclip(im, x + k);
        for (c = 0; c < im->ch; c++) {
            double v[4];
            v[0] = alwan_wp_cubic(alwan_wp_px(im, xs[0], yc, c), alwan_wp_px(im, xs[1], yc, c), alwan_wp_px(im, xs[2], yc, c),
                                  alwan_wp_px(im, xs[3], yc, c), dx, f32);
            for (k = 1; k < 4; k++) {
                if (y + k >= 0 && y + k < (long)im->h) {
                    v[k] = alwan_wp_cubic(alwan_wp_px(im, xs[0], y + k, c), alwan_wp_px(im, xs[1], y + k, c), alwan_wp_px(im, xs[2], y + k, c),
                                          alwan_wp_px(im, xs[3], y + k, c), dx, f32);
                } else {
                    v[k] = v[k - 1];
                }
            }
            alwan_wp_store(orow, im->kind, ox * im->ch + c, alwan_wp_cubic(v[0], v[1], v[2], v[3], dy, 0), 1);
        }
    }
    return 1;
}

static void alwan_wp_copy_px(void *orow, size_t ox, alwan_wp_img const *im, long x, long y) {
    size_t const elem = im->kind == 0 ? sizeof(alwan_f64) : im->kind == 1 ? sizeof(alwan_f32) : 1u;
    memcpy((char *)orow + ox * im->ch * elem, (char const *)im->src + (size_t)y * im->rs + (size_t)x * im->ch * elem, im->ch * elem);
}

static int alwan_wp_check_fixed(double const a[6], long x, long y) {
    return fabs((double)x * a[0] + (double)y * a[1] + a[2]) < 32768.0 && fabs((double)x * a[3] + (double)y * a[4] + a[5]) < 32768.0;
}

static long alwan_wp_fix(double v) {
    double const t = v * 65536.0 + 0.5;
    return ALWAN_WP_FLOOR(t);
}

static alwan_status alwan_wp_run(void *out, size_t out_rs, size_t ow, size_t oh, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_warp_method method, alwan_warp_params const *params, int kind) {
    alwan_warp_params const zero = { { 0 } };
    alwan_warp_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    int identity = 1, i;
    double a[8];
    alwan_wp_img im;
    size_t x, y, c;
    if (!out || !src || w == 0 || h == 0 || ow == 0 || oh == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < ow) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_WARP_BICUBIC) return ALWAN_E_INVALID;
    if (w > 1u << 24 || h > 1u << 24 || ow > 1u << 24 || oh > 1u << 24) return ALWAN_E_RANGE;
    for (i = 0; i < 8; i++) {
        if (!(p->matrix[i] - p->matrix[i] == 0.0)) return ALWAN_E_RANGE;
        if (p->matrix[i] != 0.0) identity = 0;
    }
    for (i = 0; i < 8; i++) a[i] = p->matrix[i];
    if (identity) a[0] = 1.0, a[4] = 1.0;
    if (!p->perspective) a[6] = a[7] = 0.0;
    im.src = src, im.rs = src_rs, im.ch = ch, im.w = w, im.h = h, im.kind = kind;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w * ch; x++) {
            double const v = alwan_wp_px(&im, (long)(x / ch), (long)y, x % ch);
            if (!(v - v == 0.0)) return ALWAN_E_INVALID;
        }
    }
    /* the fill first; every sample that lands inside overwrites it */
    for (y = 0; y < oh; y++) {
        char *orow = (char *)out + y * out_rs;
        for (x = 0; x < ow; x++)
            for (c = 0; c < ch; c++) {
                double const f = p->fill[c];
                if (kind == 0) ((alwan_f64 *)orow)[x * ch + c] = f;
                else if (kind == 1) ((alwan_f32 *)orow)[x * ch + c] = (alwan_f32)f;
                else ((unsigned char *)orow)[x * ch + c] = (unsigned char)(f < 0.0 ? 0.0 : f > 255.0 ? 255.0 : f);
            }
    }
    if (method == ALWAN_WARP_NEAREST && !p->perspective) {
        if (a[1] == 0.0 && a[3] == 0.0) {
            /* ImagingScaleAffine */
            double xo = a[2] + a[0] * 0.5, yo = a[5] + a[4] * 0.5;
            long *xin = (long *)ALWAN_ALLOC(alwan_safe_array_size(ow, sizeof(long)), sizeof(long));
            if (!xin) return ALWAN_E_NOMEM;
            for (x = 0; x < ow; x++) {
                xin[x] = ALWAN_WP_COORD(xo);
                xo += a[0];
            }
            for (y = 0; y < oh; y++) {
                long const yi = ALWAN_WP_COORD(yo);
                if (yi >= 0 && yi < (long)h)
                    for (x = 0; x < ow; x++)
                        if (xin[x] >= 0 && xin[x] < (long)w) alwan_wp_copy_px((char *)out + y * out_rs, x, &im, xin[x], yi);
                yo += a[4];
            }
            ALWAN_FREE(xin);
        } else if (alwan_wp_check_fixed(a, 0, 0) && alwan_wp_check_fixed(a, (long)ow, (long)oh) && alwan_wp_check_fixed(a, 0, (long)oh) &&
                   alwan_wp_check_fixed(a, (long)ow, 0)) {
            /* affine_fixed: 16.16 fixed point */
            long const a0 = alwan_wp_fix(a[0]), a1 = alwan_wp_fix(a[1]), a3 = alwan_wp_fix(a[3]), a4 = alwan_wp_fix(a[4]);
            long a2 = alwan_wp_fix(a[2] + a[0] * 0.5 + a[1] * 0.5), a5 = alwan_wp_fix(a[5] + a[3] * 0.5 + a[4] * 0.5);
            for (y = 0; y < oh; y++) {
                long xx = a2, yy = a5;
                for (x = 0; x < ow; x++) {
                    long const xi = xx >> 16;   /* arithmetic, as Pillow's int shift */
                    if (xi >= 0 && xi < (long)w) {
                        long const yi = yy >> 16;
                        if (yi >= 0 && yi < (long)h) alwan_wp_copy_px((char *)out + y * out_rs, x, &im, xi, yi);
                    }
                    xx += a0;
                    yy += a3;
                }
                a2 += a1;
                a5 += a4;
            }
        } else {
            double xo = a[2] + a[1] * 0.5 + a[0] * 0.5, yo = a[5] + a[4] * 0.5 + a[3] * 0.5;
            for (y = 0; y < oh; y++) {
                double xx = xo, yy = yo;
                for (x = 0; x < ow; x++) {
                    long const xi = ALWAN_WP_COORD(xx);
                    if (xi >= 0 && xi < (long)w) {
                        long const yi = ALWAN_WP_COORD(yy);
                        if (yi >= 0 && yi < (long)h) alwan_wp_copy_px((char *)out + y * out_rs, x, &im, xi, yi);
                    }
                    xx += a[0];
                    yy += a[3];
                }
                xo += a[1];
                yo += a[4];
            }
        }
        return ALWAN_OK;
    }
    for (y = 0; y < oh; y++) {
        char *orow = (char *)out + y * out_rs;
        for (x = 0; x < ow; x++) {
            double const xin = (double)x + 0.5, yin = (double)y + 0.5;
            double xs, ys;
            if (p->perspective) {
                xs = (a[0] * xin + a[1] * yin + a[2]) / (a[6] * xin + a[7] * yin + 1);
                ys = (a[3] * xin + a[4] * yin + a[5]) / (a[6] * xin + a[7] * yin + 1);
            } else {
                xs = a[0] * xin + a[1] * yin + a[2];
                ys = a[3] * xin + a[4] * yin + a[5];
            }
            if (method == ALWAN_WARP_NEAREST) {
                long const xi = ALWAN_WP_COORD(xs), yi = ALWAN_WP_COORD(ys);
                if (xi >= 0 && xi < (long)w && yi >= 0 && yi < (long)h) alwan_wp_copy_px(orow, x, &im, xi, yi);
            } else {
                alwan_wp_sample(orow, x, &im, xs, ys, method == ALWAN_WARP_BICUBIC);
            }
        }
    }
    return ALWAN_OK;
}

alwan_status alwan_warp_u8(unsigned char *out, size_t out_row_stride, size_t out_width, size_t out_height, unsigned char const *src,
                           size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_warp_method method,
                           alwan_warp_params const *params) {
    return alwan_wp_run(out, out_row_stride, out_width, out_height, src, src_row_stride, channels, width, height, method, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_warp_f64(alwan_f64 *out, size_t out_row_stride, size_t out_width, size_t out_height, alwan_f64 const *src,
                            size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_warp_method method,
                            alwan_warp_params const *params) {
    return alwan_wp_run(out, out_row_stride, out_width, out_height, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_warp_f32(alwan_f32 *out, size_t out_row_stride, size_t out_width, size_t out_height, alwan_f32 const *src,
                            size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_warp_method method,
                            alwan_warp_params const *params) {
    return alwan_wp_run(out, out_row_stride, out_width, out_height, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
