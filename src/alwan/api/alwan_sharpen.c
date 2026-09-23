/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_sharpen_{T}: sharpening behind one entry point.
 *
 * ALWAN_SHARPEN_UNSHARP_MASK: the unsharp mask, out = in + amount (in - G_radius * in), the
 * detail a Gaussian of standard deviation `radius` removes added back `amount` times. The
 * reference is scikit-image's filters.unsharp_mask, and this follows it: each channel alone,
 * the Gaussian of scipy.ndimage.gaussian_filter with mode 'reflect' (half-sample symmetric,
 * d c b a | a b c d | d c b a), truncated at int(4 radius + 0.5) taps each side, weights
 * exp(-x^2 / (2 radius^2)) normalised to sum 1, applied down the columns and then along the
 * rows. scikit-image clips the result to [0, 1] ([-1, 1] when the image has a negative
 * value) unless preserve_range is set; alwan does not clip unless asked (params.clip), as
 * it does nowhere else silently.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>

static int alwan_sh_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

/* scipy.ndimage 'reflect': -1 -> 0, n -> n - 1. */
static size_t alwan_sh_reflect(long i, long n) {
    if (n == 1) return 0;
    while (i < 0 || i >= n) {
        if (i < 0) i = -i - 1;
        if (i >= n) i = 2 * n - 1 - i;
    }
    return (size_t)i;
}

static alwan_status alwan_sh_unsharp(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w,
                                     size_t h, double radius, double amount, int clip, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    long const r = (long)(4.0 * radius + 0.5);
    double *data, *tmp, *k;
    double lo = 0.0, hi = 1.0, ksum = 0.0;
    size_t x, y, c;
    long j;
    if (!out || !src) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if (!alwan_sh_finite(radius) || !alwan_sh_finite(amount)) return ALWAN_E_INVALID;
    if (!(radius > 0.0) || radius > 1000.0 || n / w != h) return ALWAN_E_RANGE;

    data = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 2 * ch * sizeof(double)), sizeof(double));
    k = (double *)ALWAN_ALLOC(alwan_safe_array_size((size_t)(2 * r + 1), sizeof(double)), sizeof(double));
    if (!data || !k) {
        if (data) ALWAN_FREE(data);
        if (k) ALWAN_FREE(k);
        return ALWAN_E_NOMEM;
    }
    tmp = data + n * ch;
    for (j = -r; j <= r; j++) {
        k[j + r] = exp(-0.5 / (radius * radius) * (double)(j * j));
        ksum += k[j + r];
    }
    for (j = 0; j <= 2 * r; j++) k[j] /= ksum;

    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w * ch; x++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)row)[x] : ((alwan_f64 const *)row)[x];
            if (!alwan_sh_finite(v)) {
                ALWAN_FREE(data);
                ALWAN_FREE(k);
                return ALWAN_E_INVALID;
            }
            if (v < 0.0) lo = -1.0;
            data[y * w * ch + x] = v;
        }
    }

    /* down the columns (axis 0) into tmp, then along the rows (axis 1) back */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double s = 0.0;
                for (j = -r; j <= r; j++) s += k[j + r] * data[(alwan_sh_reflect((long)y + j, (long)h) * w + x) * ch + c];
                tmp[(y * w + x) * ch + c] = s;
            }
        }
    }
    for (y = 0; y < h; y++) {
        char *orow = (char *)out + y * out_rs;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double s = 0.0, v, in;
                for (j = -r; j <= r; j++) s += k[j + r] * tmp[(y * w + alwan_sh_reflect((long)x + j, (long)w)) * ch + c];
                in = data[(y * w + x) * ch + c];
                v = in + (in - s) * amount;
                if (clip) v = v < lo ? lo : v > hi ? hi : v;
                if (is_f32) ((alwan_f32 *)orow)[x * ch + c] = (alwan_f32)v;
                else ((alwan_f64 *)orow)[x * ch + c] = v;
            }
        }
    }
    ALWAN_FREE(data);
    ALWAN_FREE(k);
    return ALWAN_OK;
}

static alwan_status alwan_sh_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_sharpen_method method, alwan_sharpen_params const *params, int is_f32) {
    alwan_sharpen_params const zero = { 0 };
    alwan_sharpen_params const *p = params ? params : &zero;
    switch (method) {
    case ALWAN_SHARPEN_UNSHARP_MASK:
        return alwan_sh_unsharp(out, out_rs, src, src_rs, ch, w, h, p->radius == 0.0 ? 1.0 : p->radius,
                                p->amount == 0.0 ? 1.0 : p->amount, p->clip, is_f32);
    default:
        return ALWAN_E_INVALID;
    }
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_sharpen_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                               size_t channels, size_t width, size_t height, alwan_sharpen_method method,
                               alwan_sharpen_params const *params) {
    return alwan_sh_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_sharpen_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                               size_t channels, size_t width, size_t height, alwan_sharpen_method method,
                               alwan_sharpen_params const *params) {
    return alwan_sh_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
