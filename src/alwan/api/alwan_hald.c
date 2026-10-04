/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Hald CLUTs (Eskil Steenberg's format, as ImageMagick's hald: coder and -hald-clut and
 * G'MIC read it). A level-L Hald image is a square of L^3 x L^3 RGB pixels holding a
 * cube of edge N = L^2: the pixel at row-major index i = r + N (g + N b) holds the
 * colour the grid point (r, g, b) / (N - 1) maps to, red changing fastest. That is
 * exactly alwan's R-fastest cube layout, so converting is a copy that drops or adds row
 * padding and changes the pixel format, and applying one is alwan_table3d_sample.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

static int alwan__hald_level_ok(int level) {
    return level >= 2 && level <= 16;
}

static size_t alwan__hald_esz(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8: return 1;
    case ALWAN_PIXEL_U16: return 2;
    case ALWAN_PIXEL_F16: return 2;
    case ALWAN_PIXEL_F32: return 4;
    case ALWAN_PIXEL_F64: return 8;
    default: return 0;
    }
}

static double alwan__hald_get(void const *p, alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8: return (double)*(unsigned char const *)p / 255.0;
    case ALWAN_PIXEL_U16: return (double)*(alwan_uint16 const *)p / 65535.0;
    case ALWAN_PIXEL_F16: {
        alwan_f32 f;
        alwan_half_to_float(&f, (alwan_uint16 const *)p, 1);
        return (double)f;
    }
    case ALWAN_PIXEL_F32: return (double)*(alwan_f32 const *)p;
    default: return *(alwan_f64 const *)p;
    }
}

static void alwan__hald_put(void *p, alwan_pixel_format fmt, double v) {
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        double q = ALWAN_FLOOR_F64(v * 255.0 + 0.5);
        *(unsigned char *)p = (unsigned char)(q < 0.0 ? 0.0 : (q > 255.0 ? 255.0 : q));
        break;
    }
    case ALWAN_PIXEL_U16: {
        double q = ALWAN_FLOOR_F64(v * 65535.0 + 0.5);
        *(alwan_uint16 *)p = (alwan_uint16)(q < 0.0 ? 0.0 : (q > 65535.0 ? 65535.0 : q));
        break;
    }
    case ALWAN_PIXEL_F16: {
        alwan_f32 const f = (alwan_f32)v;
        alwan_float_to_half((alwan_uint16 *)p, &f, 1);
        break;
    }
    case ALWAN_PIXEL_F32: *(alwan_f32 *)p = (alwan_f32)v; break;
    default: *(alwan_f64 *)p = v; break;
    }
}

alwan_status alwan_hald_dimensions(size_t *side, size_t *cube_size, int level) {
    size_t const l = (size_t)level;
    if (!alwan__hald_level_ok(level)) return ALWAN_E_RANGE;
    if (side) *side = l * l * l;
    if (cube_size) *cube_size = l * l;
    return ALWAN_OK;
}

alwan_status alwan_hald_identity(void *out, size_t row_stride, alwan_pixel_format fmt, int level) {
    size_t const l = (size_t)level, n = l * l, side = l * l * l, esz = alwan__hald_esz(fmt);
    size_t y, x;
    if (!out || esz == 0) return ALWAN_E_INVALID;
    if (!alwan__hald_level_ok(level)) return ALWAN_E_RANGE;
    if (row_stride == 0) row_stride = side * 3 * esz;
    if (row_stride < side * 3 * esz) return ALWAN_E_INVALID;
    for (y = 0; y < side; y++) {
        char *row = (char *)out + y * row_stride;
        for (x = 0; x < side; x++) {
            size_t const i = y * side + x;
            double const d = (double)(n - 1);
            alwan__hald_put(row + (x * 3 + 0) * esz, fmt, (double)(i % n) / d);
            alwan__hald_put(row + (x * 3 + 1) * esz, fmt, (double)((i / n) % n) / d);
            alwan__hald_put(row + (x * 3 + 2) * esz, fmt, (double)(i / (n * n)) / d);
        }
    }
    return ALWAN_OK;
}

#define ALWAN__HALD_TO_LUT(T, NAME)                                                                     \
    alwan_status NAME(T *lut, void const *hald, size_t row_stride, alwan_pixel_format fmt, int level) { \
        size_t const l = (size_t)level, side = l * l * l, esz = alwan__hald_esz(fmt);                   \
        size_t y, x, c;                                                                                 \
        if (!lut || !hald || esz == 0) return ALWAN_E_INVALID;                                          \
        if (!alwan__hald_level_ok(level)) return ALWAN_E_RANGE;                                         \
        if (row_stride == 0) row_stride = side * 3 * esz;                                               \
        if (row_stride < side * 3 * esz) return ALWAN_E_INVALID;                                        \
        for (y = 0; y < side; y++) {                                                                    \
            char const *row = (char const *)hald + y * row_stride;                                      \
            for (x = 0; x < side; x++)                                                                  \
                for (c = 0; c < 3; c++)                                                                 \
                    lut[(y * side + x) * 3 + c] = (T)alwan__hald_get(row + (x * 3 + c) * esz, fmt);     \
        }                                                                                               \
        return ALWAN_OK;                                                                                \
    }

#define ALWAN__LUT_TO_HALD(T, NAME)                                                                     \
    alwan_status NAME(void *hald, size_t row_stride, alwan_pixel_format fmt, T const *lut, int level) { \
        size_t const l = (size_t)level, side = l * l * l, esz = alwan__hald_esz(fmt);                   \
        size_t y, x, c;                                                                                 \
        if (!lut || !hald || esz == 0) return ALWAN_E_INVALID;                                          \
        if (!alwan__hald_level_ok(level)) return ALWAN_E_RANGE;                                         \
        if (row_stride == 0) row_stride = side * 3 * esz;                                               \
        if (row_stride < side * 3 * esz) return ALWAN_E_INVALID;                                        \
        for (y = 0; y < side; y++) {                                                                    \
            char *row = (char *)hald + y * row_stride;                                                  \
            for (x = 0; x < side; x++)                                                                  \
                for (c = 0; c < 3; c++)                                                                 \
                    alwan__hald_put(row + (x * 3 + c) * esz, fmt, (double)lut[(y * side + x) * 3 + c]); \
        }                                                                                               \
        return ALWAN_OK;                                                                                \
    }

#if ALWAN_WITH_F64
ALWAN__HALD_TO_LUT(alwan_f64, alwan_hald_to_lut3d_f64)
ALWAN__LUT_TO_HALD(alwan_f64, alwan_lut3d_to_hald_f64)
#endif
#if ALWAN_WITH_F32
ALWAN__HALD_TO_LUT(alwan_f32, alwan_hald_to_lut3d_f32)
ALWAN__LUT_TO_HALD(alwan_f32, alwan_lut3d_to_hald_f32)
#endif

#define ALWAN__HALD_APPLY(T, RGB, SAMPLE, NAME)                                                                  \
    alwan_status NAME(T *out, size_t out_stride, T const *in, size_t in_stride, size_t count, T const *lut,       \
                      int level, alwan_sample_mode mode) {                                                       \
        size_t i;                                                                                                \
        alwan_status st;                                                                                         \
        if (!out || !in || !lut) return ALWAN_E_INVALID;                                                         \
        if (!alwan__hald_level_ok(level)) return ALWAN_E_RANGE;                                                  \
        if (out_stride == 0) out_stride = 3 * sizeof(T);                                                         \
        if (in_stride == 0) in_stride = 3 * sizeof(T);                                                           \
        for (i = 0; i < count; i++) {                                                                            \
            T const *s = (T const *)((char const *)in + i * in_stride);                                          \
            T *d = (T *)((char *)out + i * out_stride);                                                          \
            RGB c, r;                                                                                            \
            c.r = s[0];                                                                                          \
            c.g = s[1];                                                                                          \
            c.b = s[2];                                                                                          \
            if ((st = SAMPLE(&r, lut, level * level, &c, mode)) != ALWAN_OK) return st;                          \
            d[0] = r.r;                                                                                          \
            d[1] = r.g;                                                                                          \
            d[2] = r.b;                                                                                          \
        }                                                                                                        \
        return ALWAN_OK;                                                                                         \
    }

#if ALWAN_WITH_F64
ALWAN__HALD_APPLY(alwan_f64, alwan_rgb_f64, alwan_table3d_sample_f64, alwan_hald_apply_f64)
#endif
#if ALWAN_WITH_F32
ALWAN__HALD_APPLY(alwan_f32, alwan_rgb_f32, alwan_table3d_sample_f32, alwan_hald_apply_f32)
#endif
