/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_local_contrast: contrast that adapts to the neighbourhood, behind one entry point
 * per data type. CLAHE (api/alwan_clahe.c) works on 8- and 16-bit data; the local
 * Laplacian filter (api/alwan_local_laplacian.c) on floats, and on 8- and 16-bit data
 * through double in 0..1, rounded back as MATLAB's locallapfilt does for integer input.
 *
 * Histogram equalisation is the global form of CLAHE: one tone curve for the whole image,
 * its cumulative histogram.
 *
 *   8-bit, as OpenCV's equalizeHist: with h the histogram and f the first occupied level,
 *   lut[f] = 0 and lut[v] = saturate_cast<uchar>(float(sum h[f+1..v]) * (255.f / (n - h[f])))
 *   for v > f, the product in float and rounded half to even; an image of one level is
 *   left as it is.
 *   floats, as scikit-image's exposure.equalize_hist: numpy.histogram over [min, max] into
 *   `bins` bins (min - 0.5 to max + 0.5 when the two are equal), the cumulative counts over
 *   n as the curve at the bin centres, and each value numpy.interp-olated on it; the result
 *   is in 0..1 whatever the input's range.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
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
                double v = ALWAN_FLOOR_F64(buf[y * w * ch + x] * top + 0.5);
                v = v < 0.0 ? 0.0 : v > top ? top : v;
                if (is16) ((unsigned short *)row)[x] = (unsigned short)v;
                else ((unsigned char *)row)[x] = (unsigned char)v;
            }
        }
    }
    ALWAN_FREE(buf);
    return st;
}

/* ---- histogram equalisation ---- */

static alwan_status alwan_lc_he_u8(unsigned char *out, size_t out_rs, unsigned char const *src, size_t src_rs, size_t w,
                                   size_t h) {
    size_t hist[256] = { 0 };
    unsigned char lut[256];
    size_t const n = w * h;
    size_t x, y, f = 0, sum = 0;
    int v;
    float scale;
    if (!out || !src || w == 0 || h == 0 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs < w || out_rs < w) return ALWAN_E_INVALID;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) hist[src[y * src_rs + x]]++;
    }
    while (hist[f] == 0) f++;
    if (hist[f] == n) { /* one level: OpenCV sets the image to it */
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) out[y * out_rs + x] = (unsigned char)f;
        }
        return ALWAN_OK;
    }
    for (v = 0; v < 256; v++) lut[v] = 0;
    scale = 255.0f / (float)(n - hist[f]);
    for (v = (int)f + 1; v < 256; v++) {
        float r;
        sum += hist[v];
        r = (float)sum * scale;
        {   /* cvRound: half to even */
            float const fl = ALWAN_FLOOR_F32(r), d = r - fl;
            r = (d > 0.5f || (d == 0.5f && ALWAN_FMOD_F32(fl, 2.0f) != 0.0f)) ? fl + 1.0f : fl;
        }
        lut[v] = (unsigned char)(r < 0.0f ? 0.0f : r > 255.0f ? 255.0f : r);
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) out[y * out_rs + x] = lut[src[y * src_rs + x]];
    }
    return ALWAN_OK;
}

/* numpy.interp for one x against increasing xp (n points) and fp. */
static double alwan_lc_interp(double x, double const *xp, double const *fp, size_t n) {
    size_t lo = 0, hi = n;
    if (x < xp[0]) return fp[0];
    if (x >= xp[n - 1]) return fp[n - 1];
    while (hi - lo > 1) {
        size_t const mid = (lo + hi) / 2;
        if (xp[mid] <= x) lo = mid;
        else hi = mid;
    }
    if (xp[lo] == x) return fp[lo];
    return (fp[lo + 1] - fp[lo]) / (xp[lo + 1] - xp[lo]) * (x - xp[lo]) + fp[lo];
}

static alwan_status alwan_lc_he_float(void *out, size_t out_rs, void const *src, size_t src_rs, size_t w, size_t h,
                                      size_t bins, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    double lo = DBL_MAX, hi = -DBL_MAX, step;
    double *edges, *centres, *cdf;
    size_t *hist;
    size_t x, y, k;
    if (!out || !src || w == 0 || h == 0 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem < w || out_rs / elem < w) return ALWAN_E_INVALID;
    if (bins < 1 || bins > 1000000) return ALWAN_E_RANGE;
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)row)[x] : ((alwan_f64 const *)row)[x];
            if (!(v == v) || v > DBL_MAX || v < -DBL_MAX) return ALWAN_E_INVALID;
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    }
    if (lo == hi) { /* numpy widens an empty range by a half on each side */
        lo -= 0.5;
        hi += 0.5;
    }
    edges = (double *)ALWAN_ALLOC((2 * bins + 1) * sizeof(double) + bins * sizeof(double), sizeof(double));
    hist = (size_t *)ALWAN_ALLOC(bins * sizeof(size_t), sizeof(size_t));
    if (!edges || !hist) {
        if (edges) ALWAN_FREE(edges);
        if (hist) ALWAN_FREE(hist);
        return ALWAN_E_NOMEM;
    }
    centres = edges + bins + 1;
    cdf = centres + bins;
    step = (hi - lo) / (double)bins; /* numpy.linspace: k step + lo, the last edge exactly hi */
    for (k = 0; k < bins; k++) edges[k] = (double)k * step + lo;
    edges[bins] = hi;
    for (k = 0; k < bins; k++) {
        hist[k] = 0;
        centres[k] = (edges[k] + edges[k + 1]) / 2.0;
    }
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)row)[x] : ((alwan_f64 const *)row)[x];
            size_t a = 0, b = bins + 1; /* the first edge above v lies in [a, b] */
            if (v == edges[bins]) {
                hist[bins - 1]++;
                continue;
            }
            while (a < b) {
                size_t const mid = (a + b) / 2;
                if (edges[mid] <= v) a = mid + 1;
                else b = mid;
            }
            hist[a - 1]++;
        }
    }
    {
        size_t run = 0;
        for (k = 0; k < bins; k++) {
            run += hist[k];
            cdf[k] = (double)run / (double)n;
        }
    }
    if (is_f32) { /* scikit-image casts the curve to float32 for float32 input */
        for (k = 0; k < bins; k++) cdf[k] = (double)(alwan_f32)cdf[k];
    }
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        char *orow = (char *)out + y * out_rs;
        for (x = 0; x < w; x++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)row)[x] : ((alwan_f64 const *)row)[x];
            double const r = alwan_lc_interp(v, centres, cdf, bins);
            if (is_f32) ((alwan_f32 *)orow)[x] = (alwan_f32)r;
            else ((alwan_f64 *)orow)[x] = r;
        }
    }
    ALWAN_FREE(edges);
    ALWAN_FREE(hist);
    return ALWAN_OK;
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
    case ALWAN_LOCAL_CONTRAST_HISTOGRAM_EQUALIZE: /* 8-bit as OpenCV; no 16-bit reference */
        if (ch != 1 || is16) return ALWAN_E_INVALID;
        return alwan_lc_he_u8((unsigned char *)out, out_rs, (unsigned char const *)src, src_rs, w, h);
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
    case ALWAN_LOCAL_CONTRAST_HISTOGRAM_EQUALIZE:
        if (ch != 1) return ALWAN_E_INVALID;
        return alwan_lc_he_float(out, out_rs, src, src_rs, w, h, p->bins == 0 ? 256 : p->bins, is_f32);
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
