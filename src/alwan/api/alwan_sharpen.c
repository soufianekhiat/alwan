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
 *
 * ALWAN_SHARPEN_UNSHARP_MASK_BOX, 8-bit: the unsharp mask as Pillow's ImageFilter.UnsharpMask
 * computes it (src/libImaging/UnsharpMask.c and BoxBlur.c, HPND licence; Kevin Cazabon's
 * PILusm). The blur is an extended box blur (Gwosdek, Grewenig, Bruhn and Weickert, SSVM
 * 2011) that approximates the Gaussian: three passes along the rows and then three down
 * the columns, each a box of radius l + a, whole pixels l with a fractional weight a at
 * both ends, from sigma^2 = radius^2 / 3 as
 *
 *   L = sqrt(12 sigma^2 + 1), l = floor((L - 1) / 2),
 *   a = (2 l + 1)(l (l + 1) - 3 sigma^2) / (6 (sigma^2 - (l + 1)^2))
 *
 * in float, as Pillow computes it. Each pass weighs in 24-bit fixed point (ww = 2^24 / (2 r +
 * 1), truncated; the two end pixels share what is left), replicates the border, and rounds
 * back to 8 bits. Then, per value, diff = in - blurred; where |diff| > threshold the result
 * is in + diff percent / 100 in integer arithmetic (truncated toward zero), clamped to
 * 0..255, and elsewhere in is kept, so small differences (noise, film grain, smooth
 * gradients) are left alone.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

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
    long r;
    double *data, *tmp, *k;
    double lo = 0.0, hi = 1.0, ksum = 0.0;
    size_t x, y, c;
    long j;
    if (!out || !src) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if (!alwan_sh_finite(radius) || !alwan_sh_finite(amount)) return ALWAN_E_INVALID;
    if (!(radius > 0.0) || radius > 1000.0 || n / w != h) return ALWAN_E_RANGE;
    r = (long)(4.0 * radius + 0.5);   /* radius is finite and at most 1000 here */

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

/* ---- Pillow's unsharp mask ---- */

/* Pillow's _gaussian_blur_radius, in float as it runs. */
static float alwan_usm_box_radius(float radius, int passes) {
    float const sigma2 = radius * radius / (float)passes;
    float const L = (float)sqrt(12.0 * (double)sigma2 + 1.0);
    float const l = (float)floor(((double)L - 1.0) / 2.0);
    float a = (2.0f * l + 1.0f) * (l * (l + 1.0f) - 3.0f * sigma2);
    a /= 6.0f * (sigma2 - (l + 1.0f) * (l + 1.0f));
    return l + a;
}

/* One pass of Pillow's ImagingLineBoxBlur8 over n values `step` apart, into out (n values,
 * contiguous); arithmetic in 32-bit unsigned as Pillow's. */
static void alwan_usm_line(unsigned char *out, unsigned char const *in, size_t step, int n, int radius,
                           unsigned int ww, unsigned int fw) {
    int const lastx = n - 1;
    int const edgeA = radius + 1 < n ? radius + 1 : n;
    int const edgeB = n - radius - 1 > 0 ? n - radius - 1 : 0;
    unsigned int acc, bulk;
    int x;
#define USM_IN(i) ((unsigned int)in[(size_t)(i) * step])
#define USM_MOVE(sub, add) acc += USM_IN(add) - USM_IN(sub)
#define USM_SAVE(i, left, right)                                  \
    bulk = acc * ww + (USM_IN(left) + USM_IN(right)) * fw;          \
    out[i] = (unsigned char)((bulk + (1u << 23)) >> 24)
    acc = USM_IN(0) * (unsigned int)(radius + 1);
    for (x = 0; x < edgeA - 1; x++) acc += USM_IN(x);
    acc += USM_IN(lastx) * (unsigned int)(radius - edgeA + 1);
    if (edgeA <= edgeB) {
        for (x = 0; x < edgeA; x++) {
            USM_MOVE(0, x + radius);
            USM_SAVE(x, 0, x + radius + 1);
        }
        for (x = edgeA; x < edgeB; x++) {
            USM_MOVE(x - radius - 1, x + radius);
            USM_SAVE(x, x - radius - 1, x + radius + 1);
        }
        for (x = edgeB; x <= lastx; x++) {
            USM_MOVE(x - radius - 1, lastx);
            USM_SAVE(x, x - radius - 1, lastx);
        }
    } else {
        for (x = 0; x < edgeB; x++) {
            USM_MOVE(0, x + radius);
            USM_SAVE(x, 0, x + radius + 1);
        }
        for (x = edgeB; x < edgeA; x++) {
            USM_MOVE(0, lastx);
            USM_SAVE(x, 0, lastx);
        }
        for (x = edgeA; x <= lastx; x++) {
            USM_MOVE(x - radius - 1, lastx);
            USM_SAVE(x, x - radius - 1, lastx);
        }
    }
#undef USM_IN
#undef USM_MOVE
#undef USM_SAVE
}

static alwan_status alwan_sh_usm_box(unsigned char *out, size_t out_rs, unsigned char const *src, size_t src_rs,
                                     size_t ch, size_t w, size_t h, double radius, double amount, int threshold) {
    size_t const n = w * h * ch;
    unsigned char *img, *line;
    size_t c, x, y;
    int pass, percent;
    float fr;
    if (!out || !src || w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_rs / ch < w || out_rs / ch < w || n / w / ch != h) return ALWAN_E_INVALID;
    if (w > 0x7fffffff || h > 0x7fffffff) return ALWAN_E_RANGE;
    if (!alwan_sh_finite(radius) || !alwan_sh_finite(amount) || radius < 0.0 || radius > 1000.0 ||
        fabs(amount) > 1000.0) {
        return ALWAN_E_RANGE;
    }
    percent = (int)floor(amount * 100.0 + 0.5);
    img = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size(n, 1) + (w > h ? w : h), 1);
    if (!img) return ALWAN_E_NOMEM;
    line = img + n;
    for (y = 0; y < h; y++) memcpy(img + y * w * ch, src + y * src_rs, w * ch);
    fr = alwan_usm_box_radius((float)radius, 3);
    if (fr != 0.0f) {
        int const r = (int)fr;
        unsigned int const ww = (unsigned int)((float)(1 << 24) / (fr * 2.0f + 1.0f));
        unsigned int const fw = ((1u << 24) - (unsigned int)(r * 2 + 1) * ww) / 2u;
        for (c = 0; c < ch; c++) {
            for (pass = 0; pass < 3; pass++) {
                for (y = 0; y < h; y++) {
                    unsigned char *row = img + y * w * ch + c;
                    alwan_usm_line(line, row, ch, (int)w, r, ww, fw);
                    for (x = 0; x < w; x++) row[x * ch] = line[x];
                }
            }
            for (pass = 0; pass < 3; pass++) {
                for (x = 0; x < w; x++) {
                    unsigned char *col = img + x * ch + c;
                    alwan_usm_line(line, col, w * ch, (int)h, r, ww, fw);
                    for (y = 0; y < h; y++) col[y * w * ch] = line[y];
                }
            }
        }
    }
    for (y = 0; y < h; y++) {
        unsigned char const *s = src + y * src_rs;
        unsigned char *b = img + y * w * ch;
        for (x = 0; x < w * ch; x++) {
            int const diff = (int)s[x] - (int)b[x];
            int v = s[x];
            if (abs(diff) > threshold) {
                v = (int)s[x] + diff * percent / 100;
                v = v < 0 ? 0 : v > 255 ? 255 : v;
            }
            b[x] = (unsigned char)v;
        }
    }
    for (y = 0; y < h; y++) memcpy(out + y * out_rs, img + y * w * ch, w * ch);
    ALWAN_FREE(img);
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
    case ALWAN_SHARPEN_UNSHARP_MASK_BOX: /* 8-bit fixed point, as Pillow's */
    default:
        return ALWAN_E_INVALID;
    }
}

alwan_status alwan_sharpen_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src, size_t src_row_stride,
                              size_t channels, size_t width, size_t height, alwan_sharpen_method method,
                              alwan_sharpen_params const *params) {
    alwan_sharpen_params const zero = { 0 };
    alwan_sharpen_params const *p = params ? params : &zero;
    switch (method) {
    case ALWAN_SHARPEN_UNSHARP_MASK_BOX:
        return alwan_sh_usm_box(out, out_row_stride, src, src_row_stride, channels, width, height,
                                p->radius == 0.0 ? 2.0 : p->radius, p->amount == 0.0 ? 1.5 : p->amount,
                                p->threshold == 0 ? 3 : p->threshold < 0 ? 0 : p->threshold);
    case ALWAN_SHARPEN_UNSHARP_MASK: /* a float method: its reference returns floats */
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
