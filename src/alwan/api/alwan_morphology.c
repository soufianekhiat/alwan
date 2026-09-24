/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_morphology_{T} and _u8: grey-level mathematical morphology, as OpenCV's erode,
 * dilate and morphologyEx compute it, which suite 217 holds this to value for value.
 *
 * Erosion is the minimum and dilation the maximum over the structuring element placed with
 * its anchor, (kernel_width / 2, kernel_height / 2), on the pixel: out(x, y) = min or max of
 * src(x + j - ax, y + i - ay) over the element's non-zero (i, j), the element used as given,
 * not reflected, as OpenCV uses it. Pixels outside the image take no part, which is
 * OpenCV's default border for both. `iterations` repeats each erosion and dilation. The
 * composites:
 *
 *   OPEN      erode, then dilate          removes bright specks smaller than the element
 *   CLOSE     dilate, then erode          fills dark holes and gaps smaller than it
 *   GRADIENT  dilate - erode              the outline of every edge
 *   TOP_HAT   src - open                  the bright detail smaller than the element
 *   BLACK_HAT close - src                 the dark detail smaller than it
 *
 * The elements are OpenCV's getStructuringElement: RECT, CROSS through the anchor, ELLIPSE
 * rasterised row by row with half-width round(c sqrt((r^2 - dy^2) / r^2)), r = h / 2 and
 * c = w / 2, and DIAMOND |dx| + |dy| <= r in rows; or the caller's own mask.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void alwan_mo_element(unsigned char *el, size_t kw, size_t kh, alwan_morphology_shape shape) {
    long const r = (long)(kh / 2), c = (long)(kw / 2);
    double const inv_r2 = r ? 1.0 / ((double)r * (double)r) : 0.0;
    size_t i, j;
    if (kw == 1 && kh == 1) shape = ALWAN_MORPHOLOGY_RECT;
    for (i = 0; i < kh; i++) {
        long j1 = 0, j2 = 0;
        if (shape == ALWAN_MORPHOLOGY_RECT || (shape == ALWAN_MORPHOLOGY_CROSS && (long)i == r)) {
            j2 = (long)kw;
        } else if (shape == ALWAN_MORPHOLOGY_CROSS) {
            j1 = c;
            j2 = c + 1;
        } else if (shape == ALWAN_MORPHOLOGY_DIAMOND) {
            long const dy = labs((long)i - r);
            if (dy <= r) {
                long const dx = r - dy;
                j1 = c - dx > 0 ? c - dx : 0;
                j2 = c + dx + 1 < (long)kw ? c + dx + 1 : (long)kw;
            }
        } else {   /* ELLIPSE */
            long const dy = (long)i - r;
            if (labs(dy) <= r) {
                double const v = (double)c * sqrt((double)(r * r - dy * dy) * inv_r2);
                double const fl = floor(v), d = v - fl;   /* saturate_cast<int> is cvRound: half to even */
                long const dx = (long)((d > 0.5 || (d == 0.5 && fmod(fl, 2.0) != 0.0)) ? fl + 1.0 : fl);
                j1 = c - dx > 0 ? c - dx : 0;
                j2 = c + dx + 1 < (long)kw ? c + dx + 1 : (long)kw;
            }
        }
        for (j = 0; j < kw; j++) el[i * kw + j] = (unsigned char)((long)j >= j1 && (long)j < j2);
    }
}

/* One erosion (dilate 0) or dilation (1) of an h x w x ch plane set, in -> out. */
static void alwan_mo_pass(double *out, double const *in, size_t w, size_t h, size_t ch, unsigned char const *el, size_t kw,
                          size_t kh, int dilate) {
    long const ax = (long)(kw / 2), ay = (long)(kh / 2);
    size_t x, y, c, i, j;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double v = dilate ? -DBL_MAX : DBL_MAX;
                int any = 0;
                for (i = 0; i < kh; i++) {
                    long const sy = (long)y + (long)i - ay;
                    if (sy < 0 || sy >= (long)h) continue;
                    for (j = 0; j < kw; j++) {
                        long const sx = (long)x + (long)j - ax;
                        double s;
                        if (!el[i * kw + j] || sx < 0 || sx >= (long)w) continue;
                        s = in[((size_t)sy * w + (size_t)sx) * ch + c];
                        if (dilate ? s > v : s < v) v = s;
                        any = 1;
                    }
                }
                /* an element with no pixel inside the image leaves the value (OpenCV's border
                 * value is the operation's identity) */
                out[(y * w + x) * ch + c] = any ? v : in[(y * w + x) * ch + c];
            }
        }
    }
}

/* iterations passes, in place through tmp. */
static void alwan_mo_repeat(double *img, double *tmp, size_t w, size_t h, size_t ch, unsigned char const *el, size_t kw,
                            size_t kh, int dilate, size_t iterations) {
    size_t k;
    for (k = 0; k < iterations; k++) {
        alwan_mo_pass(tmp, img, w, h, ch, el, kw, kh, dilate);
        memcpy(img, tmp, w * h * ch * sizeof(double));
    }
}

static alwan_status alwan_mo_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_morphology_method method, alwan_morphology_params const *params,
                                 int kind /* 0 f64, 1 f32, 2 u8 */) {
    alwan_morphology_params const zero = { 0 };
    alwan_morphology_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const kw = p->kernel_width ? p->kernel_width : 3, kh = p->kernel_height ? p->kernel_height : 3;
    size_t const iterations = p->iterations ? p->iterations : 1;
    size_t const n = w * h * ch;
    double *a, *b, *t;
    unsigned char *el;
    size_t x, y, i;
    if (!out || !src || w == 0 || h == 0 || ch == 0 || ch > 4 || n / ch / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_MORPHOLOGY_BLACK_HAT) return ALWAN_E_INVALID;
    if (!p->kernel && (unsigned)p->shape > (unsigned)ALWAN_MORPHOLOGY_DIAMOND) return ALWAN_E_INVALID;
    if (kw > 255 || kh > 255 || iterations > 1000) return ALWAN_E_RANGE;
    a = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 3 * sizeof(double)) + kw * kh, sizeof(double));
    if (!a) return ALWAN_E_NOMEM;
    b = a + n;
    t = b + n;
    el = (unsigned char *)(t + n);
    if (p->kernel) {
        for (i = 0; i < kw * kh; i++) el[i] = (unsigned char)(p->kernel[i] != 0);
    } else {
        alwan_mo_element(el, kw, kh, p->shape);
    }
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w * ch; x++) {
            double const v = kind == 0 ? ((alwan_f64 const *)row)[x] : kind == 1 ? (double)((alwan_f32 const *)row)[x]
                                                                                 : (double)((unsigned char const *)row)[x];
            if (!(v == v)) {
                ALWAN_FREE(a);
                return ALWAN_E_INVALID;
            }
            a[y * w * ch + x] = v;
        }
    }
    memcpy(b, a, n * sizeof(double));   /* b keeps the source */
    switch (method) {
    case ALWAN_MORPHOLOGY_ERODE:
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 0, iterations);
        break;
    case ALWAN_MORPHOLOGY_DILATE:
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 1, iterations);
        break;
    case ALWAN_MORPHOLOGY_OPEN:
    case ALWAN_MORPHOLOGY_TOP_HAT:
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 0, iterations);
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 1, iterations);
        if (method == ALWAN_MORPHOLOGY_TOP_HAT) for (i = 0; i < n; i++) a[i] = b[i] - a[i];
        break;
    case ALWAN_MORPHOLOGY_CLOSE:
    case ALWAN_MORPHOLOGY_BLACK_HAT:
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 1, iterations);
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 0, iterations);
        if (method == ALWAN_MORPHOLOGY_BLACK_HAT) for (i = 0; i < n; i++) a[i] = a[i] - b[i];
        break;
    case ALWAN_MORPHOLOGY_GRADIENT:
        alwan_mo_repeat(a, t, w, h, ch, el, kw, kh, 1, iterations);   /* a: dilated */
        alwan_mo_repeat(b, t, w, h, ch, el, kw, kh, 0, iterations);   /* b: eroded */
        for (i = 0; i < n; i++) a[i] = a[i] - b[i];
        break;
    }
    for (y = 0; y < h; y++) {
        char *row = (char *)out + y * out_rs;
        for (x = 0; x < w * ch; x++) {
            double const v = a[y * w * ch + x];
            if (kind == 0) ((alwan_f64 *)row)[x] = v;
            else if (kind == 1) ((alwan_f32 *)row)[x] = (alwan_f32)v;
            else ((unsigned char *)row)[x] = (unsigned char)(v < 0.0 ? 0.0 : v > 255.0 ? 255.0 : v);   /* saturating */
        }
    }
    ALWAN_FREE(a);
    return ALWAN_OK;
}

alwan_status alwan_morphology_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src, size_t src_row_stride,
                                 size_t channels, size_t width, size_t height, alwan_morphology_method method,
                                 alwan_morphology_params const *params) {
    return alwan_mo_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_morphology_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                                  size_t channels, size_t width, size_t height, alwan_morphology_method method,
                                  alwan_morphology_params const *params) {
    return alwan_mo_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_morphology_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                                  size_t channels, size_t width, size_t height, alwan_morphology_method method,
                                  alwan_morphology_params const *params) {
    return alwan_mo_run(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
