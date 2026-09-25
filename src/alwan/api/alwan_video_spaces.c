/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The analogue video colour spaces as scikit-image computes them (rgb2yiq, rgb2yuv,
 * rgb2ydbdr, rgb2ypbpr and rgb2ycbcr, with their inverses): one 3 x 3 matrix each on
 * RGB in [0, 1], plus an offset for YCbCr, whose 8-bit form puts luma on 16 to 235
 * and centres the chroma on 128.
 *
 *   in:   out = from_rgb . rgb + offset
 *   out:  rgb = rgb_from . (in - offset)
 *
 * The matrices, rgb_from included, are scikit-image's own numbers
 * (gendata/data/video_matrices.py), so alwan inverts exactly as it does. alwan's own
 * alwan_rgb_to_ycbcr_{T} is the same BT.601 transform written full range: its values
 * through alwan_ycbcr_full_to_legal_{T} at 8 bits, times 255, land within 1.3e-4 of
 * ALWAN_VIDEO_YCBCR, the difference being scikit-image's coefficients rounded to three
 * decimals (65.481, 128.553, ...).
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../map/alwan_map_internal.h"
#include <math.h>

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static double const g_video_matrices[] = {
#include "../data/video/video_matrices.csv"
};
ALWAN_DIAG_POP

#define ALWAN__VIDEO_SPACES 5

static int alwan__video_ok(alwan_video_space s) {
    return (int)s >= 0 && (int)s < ALWAN__VIDEO_SPACES;
}

/* from_rgb (to the space) or rgb_from (back), and the offset. */
static void alwan__video_get(double m[9], double off[3], alwan_video_space s, int forward) {
    double const *p = g_video_matrices + 21 * (int)s;
    int i;
    for (i = 0; i < 9; i++) m[i] = p[(forward ? 0 : 9) + i];
    for (i = 0; i < 3; i++) off[i] = p[18 + i];
}

#if ALWAN_WITH_F64
alwan_status alwan_video_matrix_f64(alwan_mat3x3_f64 *from_rgb, alwan_mat3x3_f64 *rgb_from, alwan_f64 *offset,
                                    alwan_video_space space) {
    double m[9], o[3];
    int i;
    if ((!from_rgb && !rgb_from && !offset) || !alwan__video_ok(space)) return ALWAN_E_INVALID;
    if (from_rgb) {
        alwan__video_get(m, o, space, 1);
        for (i = 0; i < 9; i++) from_rgb->m[i] = m[i];
    }
    if (rgb_from) {
        alwan__video_get(m, o, space, 0);
        for (i = 0; i < 9; i++) rgb_from->m[i] = m[i];
    }
    if (offset) {
        alwan__video_get(m, o, space, 1);
        for (i = 0; i < 3; i++) offset[i] = o[i];
    }
    return ALWAN_OK;
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_video_matrix_f32(alwan_mat3x3_f32 *from_rgb, alwan_mat3x3_f32 *rgb_from, alwan_f32 *offset,
                                    alwan_video_space space) {
    double m[9], o[3];
    int i;
    if ((!from_rgb && !rgb_from && !offset) || !alwan__video_ok(space)) return ALWAN_E_INVALID;
    if (from_rgb) {
        alwan__video_get(m, o, space, 1);
        for (i = 0; i < 9; i++) from_rgb->m[i] = (float)m[i];
    }
    if (rgb_from) {
        alwan__video_get(m, o, space, 0);
        for (i = 0; i < 9; i++) rgb_from->m[i] = (float)m[i];
    }
    if (offset) {
        alwan__video_get(m, o, space, 1);
        for (i = 0; i < 3; i++) offset[i] = (float)o[i];
    }
    return ALWAN_OK;
}
#endif

/* One pixel: numpy's arr @ matrix.T with the offset after (in) or before (out). numpy's
 * matmul goes through BLAS, which accumulates the three products with fused
 * multiply-adds, fma(v2, m2, fma(v1, m1, v0 m0)); written the same way here, the maps
 * return scikit-image's values to the bit where its BLAS does so (OpenBLAS on x86-64),
 * and within an ulp where it does not. */
#define ALWAN__VIDEO_KERNEL(T, SFX, FMA)                                                                 \
    static void alwan__video_px_##SFX(T out[3], T const in[3], T const m[9], T const off[3], int fwd) { \
        T v[3];                                                                                         \
        int i;                                                                                          \
        for (i = 0; i < 3; i++) v[i] = fwd ? in[i] : in[i] - off[i];                                    \
        for (i = 0; i < 3; i++) {                                                                       \
            T const r = FMA(v[2], m[3 * i + 2], FMA(v[1], m[3 * i + 1], v[0] * m[3 * i]));              \
            out[i] = fwd ? r + off[i] : r;                                                              \
        }                                                                                               \
    }

ALWAN__VIDEO_KERNEL(double, f64, fma)
#if ALWAN_WITH_F32
ALWAN__VIDEO_KERNEL(float, f32, fmaf)
#endif

#define ALWAN__VIDEO_MAPS(T, SFX)                                                                          \
    static alwan_status alwan__video_run_##SFX(char *o0, char *o1, char *o2, size_t out_stride,           \
                                               char const *i0, char const *i1, char const *i2,            \
                                               size_t in_stride, size_t count, alwan_video_space space,   \
                                               int fwd) {                                                  \
        double md[9], od[3];                                                                              \
        T m[9], off[3];                                                                                   \
        size_t k;                                                                                         \
        int i;                                                                                            \
        if (!alwan__video_ok(space)) return ALWAN_E_INVALID;                                              \
        alwan__video_get(md, od, space, fwd);                                                             \
        for (i = 0; i < 9; i++) m[i] = (T)md[i];                                                          \
        for (i = 0; i < 3; i++) off[i] = (T)od[i];                                                        \
        for (k = 0; k < count; k++) {                                                                     \
            T in[3], out[3];                                                                              \
            in[0] = *(T const *)(i0 + k * in_stride);                                                     \
            in[1] = *(T const *)(i1 + k * in_stride);                                                     \
            in[2] = *(T const *)(i2 + k * in_stride);                                                     \
            alwan__video_px_##SFX(out, in, m, off, fwd);                                                  \
            *(T *)(o0 + k * out_stride) = out[0];                                                         \
            *(T *)(o1 + k * out_stride) = out[1];                                                         \
            *(T *)(o2 + k * out_stride) = out[2];                                                         \
        }                                                                                                 \
        return ALWAN_OK;                                                                                  \
    }                                                                                                     \
    alwan_status alwan_rgb_to_video_##SFX##_map_interleave(T *out, size_t out_stride, T const *rgb_in,     \
                                                           size_t in_stride, size_t count,                 \
                                                           alwan_video_space space) {                      \
        if (!out || !rgb_in || count == 0) return ALWAN_E_INVALID;                                        \
        return alwan__video_run_##SFX((char *)out, (char *)(out + 1), (char *)(out + 2), out_stride,      \
                                      (char const *)rgb_in, (char const *)(rgb_in + 1),                   \
                                      (char const *)(rgb_in + 2), in_stride, count, space, 1);            \
    }                                                                                                     \
    alwan_status alwan_video_to_rgb_##SFX##_map_interleave(T *rgb_out, size_t out_stride, T const *in,     \
                                                           size_t in_stride, size_t count,                 \
                                                           alwan_video_space space) {                      \
        if (!rgb_out || !in || count == 0) return ALWAN_E_INVALID;                                        \
        return alwan__video_run_##SFX((char *)rgb_out, (char *)(rgb_out + 1), (char *)(rgb_out + 2),      \
                                      out_stride, (char const *)in, (char const *)(in + 1),               \
                                      (char const *)(in + 2), in_stride, count, space, 0);                \
    }                                                                                                     \
    alwan_status alwan_rgb_to_video_##SFX##_map_planar(T *out_ch0, size_t out_stride, T *out_ch1,         \
                                                       T *out_ch2, T const *in_ch0, size_t in_stride,     \
                                                       T const *in_ch1, T const *in_ch2, size_t count,    \
                                                       alwan_video_space space) {                         \
        if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2 || count == 0)              \
            return ALWAN_E_INVALID;                                                                       \
        return alwan__video_run_##SFX((char *)out_ch0, (char *)out_ch1, (char *)out_ch2, out_stride,      \
                                      (char const *)in_ch0, (char const *)in_ch1, (char const *)in_ch2,   \
                                      in_stride, count, space, 1);                                        \
    }                                                                                                     \
    alwan_status alwan_video_to_rgb_##SFX##_map_planar(T *out_ch0, size_t out_stride, T *out_ch1,         \
                                                       T *out_ch2, T const *in_ch0, size_t in_stride,     \
                                                       T const *in_ch1, T const *in_ch2, size_t count,    \
                                                       alwan_video_space space) {                         \
        if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2 || count == 0)              \
            return ALWAN_E_INVALID;                                                                       \
        return alwan__video_run_##SFX((char *)out_ch0, (char *)out_ch1, (char *)out_ch2, out_stride,      \
                                      (char const *)in_ch0, (char const *)in_ch1, (char const *)in_ch2,   \
                                      in_stride, count, space, 0);                                        \
    }

#if ALWAN_WITH_F64
ALWAN__VIDEO_MAPS(double, f64)
#endif
#if ALWAN_WITH_F32
ALWAN__VIDEO_MAPS(float, f32)
#endif

/* Typed I/O: each pixel read through the library's format loader, converted in double,
 * stored through its format writer. */
static int alwan__video_fmt_ok(alwan_pixel_format f) {
    return f == ALWAN_PIXEL_U8 || f == ALWAN_PIXEL_U16 || f == ALWAN_PIXEL_F16 || f == ALWAN_PIXEL_F32 ||
           f == ALWAN_PIXEL_F64;
}

static alwan_status alwan__video_run_ex(char *o0, char *o1, char *o2, size_t out_stride, alwan_pixel_format out_fmt,
                                        char const *i0, char const *i1, char const *i2, size_t in_stride,
                                        alwan_pixel_format in_fmt, size_t count, int planar, alwan_video_space space,
                                        int fwd) {
    double m[9], off[3];
    size_t k;
    if (!alwan__video_ok(space) || !alwan__video_fmt_ok(out_fmt) || !alwan__video_fmt_ok(in_fmt))
        return ALWAN_E_INVALID;
    alwan__video_get(m, off, space, fwd);
    for (k = 0; k < count; k++) {
        alwan_f64 in[3], out[3];
        if (planar) {
            in[0] = alwan__load1_typed(i0 + k * in_stride, in_fmt);
            in[1] = alwan__load1_typed(i1 + k * in_stride, in_fmt);
            in[2] = alwan__load1_typed(i2 + k * in_stride, in_fmt);
        } else {
            alwan__load3_typed(in, i0 + k * in_stride, in_fmt);
        }
        alwan__video_px_f64(out, in, m, off, fwd);
        if (planar) {
            alwan__store1_typed(o0 + k * out_stride, out[0], out_fmt);
            alwan__store1_typed(o1 + k * out_stride, out[1], out_fmt);
            alwan__store1_typed(o2 + k * out_stride, out[2], out_fmt);
        } else {
            alwan__store3_typed(o0 + k * out_stride, out, out_fmt);
        }
    }
    return ALWAN_OK;
}

alwan_status alwan_rgb_to_video_map_interleave_ex(void *out, size_t out_stride, void const *rgb_in, size_t in_stride,
                                                  size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                                  alwan_video_space space) {
    if (!out || !rgb_in || count == 0) return ALWAN_E_INVALID;
    return alwan__video_run_ex((char *)out, NULL, NULL, out_stride, out_fmt, (char const *)rgb_in, NULL, NULL,
                               in_stride, in_fmt, count, 0, space, 1);
}

alwan_status alwan_video_to_rgb_map_interleave_ex(void *rgb_out, size_t out_stride, void const *in, size_t in_stride,
                                                  size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                                  alwan_video_space space) {
    if (!rgb_out || !in || count == 0) return ALWAN_E_INVALID;
    return alwan__video_run_ex((char *)rgb_out, NULL, NULL, out_stride, out_fmt, (char const *)in, NULL, NULL,
                               in_stride, in_fmt, count, 0, space, 0);
}

alwan_status alwan_rgb_to_video_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0,
                                              size_t in_stride, void const *in1, void const *in2, size_t count,
                                              alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                              alwan_video_space space) {
    if (!out0 || !out1 || !out2 || !in0 || !in1 || !in2 || count == 0) return ALWAN_E_INVALID;
    return alwan__video_run_ex((char *)out0, (char *)out1, (char *)out2, out_stride, out_fmt, (char const *)in0,
                               (char const *)in1, (char const *)in2, in_stride, in_fmt, count, 1, space, 1);
}

alwan_status alwan_video_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0,
                                              size_t in_stride, void const *in1, void const *in2, size_t count,
                                              alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                              alwan_video_space space) {
    if (!out0 || !out1 || !out2 || !in0 || !in1 || !in2 || count == 0) return ALWAN_E_INVALID;
    return alwan__video_run_ex((char *)out0, (char *)out1, (char *)out2, out_stride, out_fmt, (char const *)in0,
                               (char const *)in1, (char const *)in2, in_stride, in_fmt, count, 1, space, 0);
}

/* ---------------------------------------------------------------- *
 * rgba2rgb: alpha flattened over a background
 * ---------------------------------------------------------------- */

/* scikit-image's rgba2rgb: clip((1 - alpha) background + alpha rgb, 0, 1), in the
 * image's precision; 8-bit values read as value * (1 / 255). */
#define ALWAN__RGBA_MAP(T, SFX)                                                                            \
    alwan_status alwan_rgba_to_rgb_##SFX##_map_interleave(T *rgb_out, size_t out_stride, T const *rgba_in,  \
                                                          size_t in_stride, size_t count,                  \
                                                          alwan_##SFX const *background) {                  \
        T bg[3] = {(T)1, (T)1, (T)1};                                                                      \
        size_t k;                                                                                          \
        int c;                                                                                             \
        if (!rgb_out || !rgba_in || count == 0) return ALWAN_E_INVALID;                                    \
        if (background)                                                                                    \
            for (c = 0; c < 3; c++) {                                                                      \
                if (!(background[c] >= (T)0 && background[c] <= (T)1)) return ALWAN_E_RANGE;              \
                bg[c] = background[c];                                                                     \
            }                                                                                              \
        for (k = 0; k < count; k++) {                                                                      \
            T const *px = (T const *)((char const *)rgba_in + k * in_stride);                              \
            T *o = (T *)((char *)rgb_out + k * out_stride);                                                \
            T const a = px[3];                                                                             \
            T v[3];                                                                                        \
            for (c = 0; c < 3; c++) v[c] = ((T)1 - a) * bg[c] + a * px[c];                                 \
            for (c = 0; c < 3; c++) o[c] = v[c] < (T)0 ? (T)0 : v[c] > (T)1 ? (T)1 : v[c];                 \
        }                                                                                                  \
        return ALWAN_OK;                                                                                   \
    }

#if ALWAN_WITH_F64
ALWAN__RGBA_MAP(double, f64)
#endif
#if ALWAN_WITH_F32
ALWAN__RGBA_MAP(float, f32)
#endif

alwan_status alwan_rgba_to_rgb_u8_map_interleave(double *rgb_out, size_t out_stride, unsigned char const *rgba_in,
                                                 size_t in_stride, size_t count, double const *background) {
    double bg[3] = {1.0, 1.0, 1.0};
    size_t k;
    int c;
    if (!rgb_out || !rgba_in || count == 0) return ALWAN_E_INVALID;
    if (background)
        for (c = 0; c < 3; c++) {
            if (!(background[c] >= 0.0 && background[c] <= 1.0)) return ALWAN_E_RANGE;
            bg[c] = background[c];
        }
    for (k = 0; k < count; k++) {
        unsigned char const *px = rgba_in + k * in_stride;
        double *o = (double *)((char *)rgb_out + k * out_stride);
        double const a = (double)px[3] * (1.0 / 255.0);
        for (c = 0; c < 3; c++) {
            double const v = (1.0 - a) * bg[c] + a * ((double)px[c] * (1.0 / 255.0));
            o[c] = v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v;
        }
    }
    return ALWAN_OK;
}
