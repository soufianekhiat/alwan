/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_local_contrast: contrast that adapts to the neighbourhood, behind one entry point
 * per data type. CLAHE (api/alwan_clahe.c) works on 8- and 16-bit data; the local
 * Laplacian filter (api/alwan_local_laplacian.c) on floats, and on 8- and 16-bit data
 * through double in 0..1, rounded back as MATLAB's locallapfilt does for integer input.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>

static double alwan_lc_or(double v, double def) {
    return v == 0.0 ? def : v;
}

static alwan_status alwan_lc_llf(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w,
                                 size_t h, alwan_local_contrast_params const *p, int is_f32) {
    return alwan__llf_run(out, out_rs, src, src_rs, ch, w, h, alwan_lc_or(p->sigma, 0.4), alwan_lc_or(p->alpha, 0.5),
                          alwan_lc_or(p->beta, 1.0), p->intensity_levels, p->separate_channels ? 1 : 0, is_f32);
}

/* The local Laplacian on integer data: values over `top` in double, filtered, then
 * round(v top) clamped, rounding half away from zero as im2uint8 / im2uint16 do. */
static alwan_status alwan_lc_llf_int(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w,
                                     size_t h, alwan_local_contrast_params const *p, int is16) {
    size_t const n = w * h * ch, elem = is16 ? 2u : 1u;
    double const top = is16 ? 65535.0 : 255.0;
    double *buf;
    size_t x, y;
    alwan_status st;
    if (!out || !src) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w || n / w / ch != h) return ALWAN_E_INVALID;
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w * ch; x++) {
            buf[y * w * ch + x] = (is16 ? (double)((unsigned short const *)row)[x] : (double)((unsigned char const *)row)[x]) / top;
        }
    }
    st = alwan_lc_llf(buf, w * ch * sizeof(double), buf, w * ch * sizeof(double), ch, w, h, p, 0);
    if (st == ALWAN_OK) {
        for (y = 0; y < h; y++) {
            char *row = (char *)out + y * out_rs;
            for (x = 0; x < w * ch; x++) {
                double v = floor(buf[y * w * ch + x] * top + 0.5);
                v = v < 0.0 ? 0.0 : v > top ? top : v;
                if (is16) ((unsigned short *)row)[x] = (unsigned short)v;
                else ((unsigned char *)row)[x] = (unsigned char)v;
            }
        }
    }
    ALWAN_FREE(buf);
    return st;
}

static alwan_status alwan_lc_int(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_local_contrast_method method, alwan_local_contrast_params const *params, int is16) {
    alwan_local_contrast_params const zero = { 0 };
    alwan_local_contrast_params const *p = params ? params : &zero;
    switch (method) {
    case ALWAN_LOCAL_CONTRAST_CLAHE:
        if (ch != 1) return ALWAN_E_INVALID;
        return alwan__clahe_run(out, out_rs, src, src_rs, w, h, p->tiles_x == 0 ? 8 : p->tiles_x,
                                p->tiles_y == 0 ? 8 : p->tiles_y, p->clip_limit == 0.0 ? 40.0 : p->clip_limit < 0.0 ? 0.0 : p->clip_limit,
                                is16);
    case ALWAN_LOCAL_CONTRAST_LAPLACIAN:
        return alwan_lc_llf_int(out, out_rs, src, src_rs, ch, w, h, p, is16);
    default:
        return ALWAN_E_INVALID;
    }
}

static alwan_status alwan_lc_float(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                   alwan_local_contrast_method method, alwan_local_contrast_params const *params,
                                   int is_f32) {
    alwan_local_contrast_params const zero = { 0 };
    alwan_local_contrast_params const *p = params ? params : &zero;
    switch (method) {
    case ALWAN_LOCAL_CONTRAST_LAPLACIAN:
        return alwan_lc_llf(out, out_rs, src, src_rs, ch, w, h, p, is_f32);
    case ALWAN_LOCAL_CONTRAST_CLAHE: /* histogram bins need integer data */
    default:
        return ALWAN_E_INVALID;
    }
}

alwan_status alwan_local_contrast_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src,
                                     size_t src_row_stride, size_t channels, size_t width, size_t height,
                                     alwan_local_contrast_method method, alwan_local_contrast_params const *params) {
    return alwan_lc_int(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}

alwan_status alwan_local_contrast_u16(unsigned short *out, size_t out_row_stride, unsigned short const *src,
                                      size_t src_row_stride, size_t channels, size_t width, size_t height,
                                      alwan_local_contrast_method method, alwan_local_contrast_params const *params) {
    return alwan_lc_int(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_local_contrast_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                                      size_t channels, size_t width, size_t height, alwan_local_contrast_method method,
                                      alwan_local_contrast_params const *params) {
    return alwan_lc_float(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_local_contrast_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                                      size_t channels, size_t width, size_t height, alwan_local_contrast_method method,
                                      alwan_local_contrast_params const *params) {
    return alwan_lc_float(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
