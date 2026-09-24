/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_resize_{T} and _u8: an image resampled to a new size, as Pillow's Image.resize
 * computes it (libImaging Resample.c and Geometry.c), which suite 233 holds this to.
 *
 * Every method but NEAREST is a separable convolution: for each output sample a filter
 * centred on in0 + (x + 0.5) scale, stretched by the scale when shrinking so that it
 * averages (antialiases), its weights in double normalised to sum to 1; a horizontal pass
 * (skipped when the width and horizontal box are unchanged), stored in the data's
 * precision, then a vertical one. 8-bit data takes the weights in 22-bit fixed point,
 * each pass rounded and clipped to 8 bits, as Pillow does. The filters:
 *
 *   BOX       1 on (-0.5, 0.5], support 0.5
 *   BILINEAR  the triangle 1 - |x|, support 1
 *   HAMMING   sinc(x) (0.54 + 0.46 cos(pi x)), support 1 (with Pillow's float constants)
 *   BICUBIC   Keys' cubic with a = -0.5, support 2
 *   LANCZOS   sinc(x) sinc(x / 3), support 3
 *
 * NEAREST takes the source sample at (int)(position), the position starting at half a step
 * and advancing a step at a time as Pillow's scale loop does; a position past the image
 * leaves the output 0. A request for the source's own size and box returns a copy.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

#define ALWAN_RS_PREC 22

static double alwan_rs_filter(int m, double x) {
    double const pi = 3.14159265358979323846;
    switch (m) {
    case ALWAN_RESIZE_BOX:
        return (x > -0.5 && x <= 0.5) ? 1.0 : 0.0;
    case ALWAN_RESIZE_BILINEAR:
        if (x < 0.0) x = -x;
        return x < 1.0 ? 1.0 - x : 0.0;
    case ALWAN_RESIZE_HAMMING:
        if (x < 0.0) x = -x;
        if (x == 0.0) return 1.0;
        if (x >= 1.0) return 0.0;
        x = x * pi;
        return sin(x) / x * ((double)0.54f + (double)0.46f * cos(x));
    case ALWAN_RESIZE_BICUBIC:
        if (x < 0.0) x = -x;
        if (x < 1.0) return ((-0.5 + 2.0) * x - (-0.5 + 3.0)) * x * x + 1;
        if (x < 2.0) return (((x - 5) * x + 8) * x - 4) * -0.5;
        return 0.0;
    default: {   /* LANCZOS */
        double s1, s3, y;
        if (!(-3.0 <= x && x < 3.0)) return 0.0;
        if (x == 0.0) s1 = 1.0;
        else {
            y = x * pi;
            s1 = sin(y) / y;
        }
        y = x / 3;
        if (y == 0.0) s3 = 1.0;
        else {
            y = y * pi;
            s3 = sin(y) / y;
        }
        return s1 * s3;
    }
    }
}

static double alwan_rs_support(int m) {
    return m == ALWAN_RESIZE_BOX ? 0.5 : m == ALWAN_RESIZE_BICUBIC ? 2.0 : m == ALWAN_RESIZE_LANCZOS ? 3.0 : 1.0;
}

/* Pillow's precompute_coeffs: ksize weights a sample in kk, bounds (first, count) in b. */
static int alwan_rs_coeffs(double *kk, int *b, int in_size, float in0, float in1, int out_size, int m) {
    double const scale = (double)(in1 - in0) / out_size;
    double const filterscale = scale < 1.0 ? 1.0 : scale;
    double const support = alwan_rs_support(m) * filterscale;
    int const ksize = (int)ceil(support) * 2 + 1;
    int xx, x;
    for (xx = 0; xx < out_size; xx++) {
        double const center = in0 + (xx + 0.5) * scale, ss = 1.0 / filterscale;
        double ww = 0.0, *k = kk + (size_t)xx * (size_t)ksize;
        int xmin = (int)(center - support + 0.5), xmax = (int)(center + support + 0.5);
        if (xmin < 0) xmin = 0;
        if (xmax > in_size) xmax = in_size;
        xmax -= xmin;
        for (x = 0; x < xmax; x++) {
            double const wgt = alwan_rs_filter(m, (x + xmin - center + 0.5) * ss);
            k[x] = wgt;
            ww += wgt;
        }
        for (x = 0; x < xmax; x++)
            if (ww != 0.0) k[x] /= ww;
        for (; x < ksize; x++) k[x] = 0;
        b[xx * 2] = xmin;
        b[xx * 2 + 1] = xmax;
    }
    return ksize;
}

static int alwan_rs_fix(double v) {
    return v < 0 ? (int)(-0.5 + v * (1 << ALWAN_RS_PREC)) : (int)(0.5 + v * (1 << ALWAN_RS_PREC));
}

static unsigned char alwan_rs_clip8(int v) {
    int const q = v >> ALWAN_RS_PREC;   /* arithmetic shift, as Pillow's lookup is indexed */
    return (unsigned char)(q < 0 ? 0 : q > 255 ? 255 : q);
}

static double alwan_rs_get(void const *p, int kind, size_t i) {
    return kind == 0 ? ((alwan_f64 const *)p)[i] : kind == 1 ? (double)((alwan_f32 const *)p)[i] : (double)((unsigned char const *)p)[i];
}

static alwan_status alwan_rs_run(void *out, size_t out_rs, size_t ow, size_t oh, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_resize_method method, alwan_resize_params const *params, int kind) {
    alwan_resize_params const zero = { { 0 } };
    alwan_resize_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    int const whole = p->box[0] == 0.0 && p->box[1] == 0.0 && p->box[2] == 0.0 && p->box[3] == 0.0;
    float const bx0 = whole ? 0.0f : (float)p->box[0], by0 = whole ? 0.0f : (float)p->box[1];
    float const bx1 = whole ? (float)w : (float)p->box[2], by1 = whole ? (float)h : (float)p->box[3];
    size_t x, y, c;
    if (!out || !src || w == 0 || h == 0 || ow == 0 || oh == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < ow) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_RESIZE_LANCZOS) return ALWAN_E_INVALID;
    if (w > 1u << 24 || h > 1u << 24 || ow > 1u << 24 || oh > 1u << 24) return ALWAN_E_RANGE;
    if (!(bx0 >= 0.0f) || !(by0 >= 0.0f) || !(bx1 <= (float)w) || !(by1 <= (float)h) || !(bx1 > bx0) || !(by1 > by0)) return ALWAN_E_RANGE;
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w * ch; x++) {
            double const v = alwan_rs_get(row, kind, x);
            if (!(v - v == 0.0)) return ALWAN_E_INVALID;
        }
    }
    /* the source's own size and box: a copy */
    if (ow == w && oh == h && bx0 == 0.0f && by0 == 0.0f && bx1 == (float)w && by1 == (float)h) {
        for (y = 0; y < h; y++) memmove((char *)out + y * out_rs, (char const *)src + y * src_rs, w * ch * elem);
        return ALWAN_OK;
    }
    if (method == ALWAN_RESIZE_NEAREST) {
        double const a0 = (double)(bx1 - bx0) / (double)ow, a4 = (double)(by1 - by0) / (double)oh;
        int *xin = (int *)ALWAN_ALLOC(alwan_safe_array_size(ow, sizeof(int)), sizeof(int));
        double xo = (double)bx0 + a0 * 0.5, yo = (double)by0 + a4 * 0.5;
        if (!xin) return ALWAN_E_NOMEM;
        for (x = 0; x < ow; x++) {
            xin[x] = xo < 0.0 ? -1 : (int)xo;
            xo += a0;
        }
        for (y = 0; y < oh; y++) {
            int const yi = yo < 0.0 ? -1 : (int)yo;
            char *orow = (char *)out + y * out_rs;
            memset(orow, 0, ow * ch * elem);
            if (yi >= 0 && yi < (int)h) {
                char const *irow = (char const *)src + (size_t)yi * src_rs;
                for (x = 0; x < ow; x++)
                    if (xin[x] >= 0 && xin[x] < (int)w) memcpy(orow + x * ch * elem, irow + (size_t)xin[x] * ch * elem, ch * elem);
            }
            yo += a4;
        }
        ALWAN_FREE(xin);
        return ALWAN_OK;
    }
    {
        int const need_h = ow != w || bx0 != 0.0f || bx1 != (float)ow;
        int const need_v = oh != h || by0 != 0.0f || by1 != (float)oh;
        double const sh = (double)(bx1 - bx0) / (double)ow, sv = (double)(by1 - by0) / (double)oh;
        size_t const kh = (size_t)ceil(alwan_rs_support(method) * (sh < 1.0 ? 1.0 : sh)) * 2 + 1;
        size_t const kv = (size_t)ceil(alwan_rs_support(method) * (sv < 1.0 ? 1.0 : sv)) * 2 + 1;
        double *kkh = (double *)ALWAN_ALLOC(alwan_safe_array_size(ow * kh + oh * kv, sizeof(double)), sizeof(double));
        int *bh = (int *)ALWAN_ALLOC(alwan_safe_array_size(2 * (ow + oh), sizeof(int)), sizeof(int));
        double *kkv, *tmp = NULL;
        int *bv, yfirst, ylast, *fk = NULL;
        size_t tw, th;
        if (!kkh || !bh) {
            ALWAN_FREE(kkh);
            ALWAN_FREE(bh);
            return ALWAN_E_NOMEM;
        }
        kkv = kkh + ow * kh;
        bv = bh + 2 * ow;
        alwan_rs_coeffs(kkh, bh, (int)w, bx0, bx1, (int)ow, method);
        alwan_rs_coeffs(kkv, bv, (int)h, by0, by1, (int)oh, method);
        yfirst = bv[0];
        ylast = bv[oh * 2 - 2] + bv[oh * 2 - 1];
        /* the intermediate: rows yfirst .. ylast of the horizontally resampled image, or the
         * source itself when there is no horizontal pass */
        tw = need_h ? ow : w;
        th = need_h ? (size_t)(ylast - yfirst) : h;
        tmp = (double *)ALWAN_ALLOC(alwan_safe_array_size(tw * (th ? th : 1) * ch, sizeof(double)), sizeof(double));
        if (kind == 2) fk = (int *)ALWAN_ALLOC(alwan_safe_array_size(ow * kh + oh * kv, sizeof(int)), sizeof(int));
        if (!tmp || (kind == 2 && !fk)) {
            ALWAN_FREE(tmp);
            ALWAN_FREE(fk);
            ALWAN_FREE(kkh);
            ALWAN_FREE(bh);
            return ALWAN_E_NOMEM;
        }
        if (kind == 2)
            for (x = 0; x < ow * kh + oh * kv; x++) fk[x] = alwan_rs_fix(kkh[x]);
        if (need_h) {
            for (y = 0; y < th; y++) {
                char const *irow = (char const *)src + (size_t)(y + (size_t)yfirst) * src_rs;
                for (x = 0; x < ow; x++) {
                    int const xmin = bh[x * 2], xmax = bh[x * 2 + 1];
                    for (c = 0; c < ch; c++) {
                        int i;
                        if (kind == 2) {
                            int ss = 1 << (ALWAN_RS_PREC - 1);
                            int const *k = fk + x * kh;
                            for (i = 0; i < xmax; i++) ss += (int)((unsigned char const *)irow)[(size_t)(i + xmin) * ch + c] * k[i];
                            tmp[(y * tw + x) * ch + c] = (double)alwan_rs_clip8(ss);
                        } else {
                            double ss = 0.0;
                            double const *k = kkh + x * kh;
                            for (i = 0; i < xmax; i++) ss += alwan_rs_get(irow, kind, (size_t)(i + xmin) * ch + c) * k[i];
                            tmp[(y * tw + x) * ch + c] = kind == 1 ? (double)(float)ss : ss;
                        }
                    }
                }
            }
            for (y = 0; y < oh; y++) bv[y * 2] -= yfirst;
        } else {
            for (y = 0; y < h; y++) {
                char const *irow = (char const *)src + y * src_rs;
                for (x = 0; x < w * ch; x++) tmp[y * w * ch + x] = alwan_rs_get(irow, kind, x);
            }
        }
        for (y = 0; y < (need_v ? oh : th); y++) {
            char *orow = (char *)out + y * out_rs;
            for (x = 0; x < tw; x++) {
                for (c = 0; c < ch; c++) {
                    double v;
                    if (!need_v) {
                        v = tmp[(y * tw + x) * ch + c];
                    } else {
                        int const ymin = bv[y * 2], ymax = bv[y * 2 + 1];
                        int i;
                        if (kind == 2) {
                            int ss = 1 << (ALWAN_RS_PREC - 1);
                            int const *k = fk + ow * kh + y * kv;
                            for (i = 0; i < ymax; i++) ss += (int)tmp[((size_t)(i + ymin) * tw + x) * ch + c] * k[i];
                            v = (double)alwan_rs_clip8(ss);
                        } else {
                            double ss = 0.0;
                            double const *k = kkv + y * kv;
                            for (i = 0; i < ymax; i++) ss += tmp[((size_t)(i + ymin) * tw + x) * ch + c] * k[i];
                            v = ss;
                        }
                    }
                    if (kind == 0) ((alwan_f64 *)orow)[x * ch + c] = v;
                    else if (kind == 1) ((alwan_f32 *)orow)[x * ch + c] = (alwan_f32)v;
                    else ((unsigned char *)orow)[x * ch + c] = (unsigned char)v;
                }
            }
        }
        ALWAN_FREE(tmp);
        ALWAN_FREE(fk);
        ALWAN_FREE(kkh);
        ALWAN_FREE(bh);
    }
    return ALWAN_OK;
}

alwan_status alwan_resize_u8(unsigned char *out, size_t out_row_stride, size_t out_width, size_t out_height, unsigned char const *src,
                             size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_resize_method method,
                             alwan_resize_params const *params) {
    return alwan_rs_run(out, out_row_stride, out_width, out_height, src, src_row_stride, channels, width, height, method, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_resize_f64(alwan_f64 *out, size_t out_row_stride, size_t out_width, size_t out_height, alwan_f64 const *src,
                              size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_resize_method method,
                              alwan_resize_params const *params) {
    return alwan_rs_run(out, out_row_stride, out_width, out_height, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_resize_f32(alwan_f32 *out, size_t out_row_stride, size_t out_width, size_t out_height, alwan_f32 const *src,
                              size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_resize_method method,
                              alwan_resize_params const *params) {
    return alwan_rs_run(out, out_row_stride, out_width, out_height, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
