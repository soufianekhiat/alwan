/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Stain separation by colour deconvolution (Ruifrok and Johnston 2001), as
 * scikit-image's separate_stains and combine_stains compute it.
 *
 * Light through a stained slide loses, per stain, an amount proportional to how
 * much stain is there: optical density OD = -log10(I / I0) adds across stains.
 * A stain set is three optical-density vectors, the rows of rgb_from; the
 * amount of each stain is the pixel's OD times the inverse, from_rgb. skimage
 * measures OD in units of -log(1e-6), the floor it puts under every channel, so
 * a channel at 1e-6 (or below) reads 1 and a white pixel 0:
 *
 *   separate:  s = (ln(max(rgb, 1e-6)) / ln(1e-6)) from_rgb,  clamped to s >= 0
 *   combine:   rgb = exp(-(s * -ln(1e-6)) rgb_from),           clipped to [0, 1]
 *
 * with rgb a row vector. The eleven preset matrices are skimage's own numbers
 * (gendata/data/stain_matrices.py); a two-stain set's third row is the cross
 * product of the first two, not normalised, as skimage builds it.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../map/alwan_map_internal.h"
#include <math.h>

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static double const g_stain_matrices[] = {
#include "../data/stain/stain_matrices.csv"
};
ALWAN_DIAG_POP

#define ALWAN__STAIN_PRESETS 11

/* The inverse by cofactors; ALWAN_E_RANGE for a singular matrix. */
static alwan_status alwan__stain_inverse(double out[9], double const m[9]) {
    double const c00 = m[4] * m[8] - m[5] * m[7];
    double const c01 = m[5] * m[6] - m[3] * m[8];
    double const c02 = m[3] * m[7] - m[4] * m[6];
    double const det = m[0] * c00 + m[1] * c01 + m[2] * c02;
    double scale = 0.0;
    int i;
    for (i = 0; i < 9; i++) scale = fabs(m[i]) > scale ? fabs(m[i]) : scale;
    if (!(fabs(det) > 1e-12 * scale * scale * scale)) return ALWAN_E_RANGE;
    out[0] = c00 / det;
    out[1] = (m[2] * m[7] - m[1] * m[8]) / det;
    out[2] = (m[1] * m[5] - m[2] * m[4]) / det;
    out[3] = c01 / det;
    out[4] = (m[0] * m[8] - m[2] * m[6]) / det;
    out[5] = (m[2] * m[3] - m[0] * m[5]) / det;
    out[6] = c02 / det;
    out[7] = (m[1] * m[6] - m[0] * m[7]) / det;
    out[8] = (m[0] * m[4] - m[1] * m[3]) / det;
    return ALWAN_OK;
}

/* rgb_from and from_rgb for a set, in double whatever the entry point's precision. */
static alwan_status alwan__stain_matrices(double rgb_from[9], double from_rgb[9], alwan_stain_set set,
                                          double const *custom) {
    int i;
    if ((int)set >= 0 && (int)set < ALWAN__STAIN_PRESETS) {
        double const *p = g_stain_matrices + 18 * (int)set;
        for (i = 0; i < 9; i++) {
            rgb_from[i] = p[i];
            from_rgb[i] = p[9 + i];
        }
        return ALWAN_OK;
    }
    if (set != ALWAN_STAIN_CUSTOM || !custom) return ALWAN_E_INVALID;
    for (i = 0; i < 9; i++) {
        if (!isfinite(custom[i])) return ALWAN_E_INVALID;
        rgb_from[i] = custom[i];
    }
    if (rgb_from[6] == 0.0 && rgb_from[7] == 0.0 && rgb_from[8] == 0.0) {
        rgb_from[6] = rgb_from[1] * rgb_from[5] - rgb_from[2] * rgb_from[4];
        rgb_from[7] = rgb_from[2] * rgb_from[3] - rgb_from[0] * rgb_from[5];
        rgb_from[8] = rgb_from[0] * rgb_from[4] - rgb_from[1] * rgb_from[3];
    }
    return alwan__stain_inverse(from_rgb, rgb_from);
}

#if ALWAN_WITH_F64
alwan_status alwan_stain_matrix_f64(alwan_mat3x3_f64 *rgb_from, alwan_mat3x3_f64 *from_rgb, alwan_stain_set set,
                                    alwan_mat3x3_f64 const *custom) {
    double rf[9], fr[9];
    alwan_status st;
    int i;
    if (!rgb_from && !from_rgb) return ALWAN_E_INVALID;
    st = alwan__stain_matrices(rf, fr, set, custom ? custom->m : NULL);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < 9; i++) {
        if (rgb_from) rgb_from->m[i] = rf[i];
        if (from_rgb) from_rgb->m[i] = fr[i];
    }
    return ALWAN_OK;
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_stain_matrix_f32(alwan_mat3x3_f32 *rgb_from, alwan_mat3x3_f32 *from_rgb, alwan_stain_set set,
                                    alwan_mat3x3_f32 const *custom) {
    double rf[9], fr[9], c[9];
    alwan_status st;
    int i;
    if (!rgb_from && !from_rgb) return ALWAN_E_INVALID;
    if (custom)
        for (i = 0; i < 9; i++) c[i] = (double)custom->m[i];
    st = alwan__stain_matrices(rf, fr, set, custom ? c : NULL);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < 9; i++) {
        if (rgb_from) rgb_from->m[i] = (float)rf[i];
        if (from_rgb) from_rgb->m[i] = (float)fr[i];
    }
    return ALWAN_OK;
}
#endif

/* The per-pixel arithmetic, in the order skimage runs it. */
#define ALWAN__STAIN_KERNELS(T, SFX, LN, EXP)                                                          \
    static void alwan__stain_separate_##SFX(T out[3], T const in[3], T const f[9], T log_floor) {      \
        T l[3];                                                                                          \
        int i;                                                                                           \
        for (i = 0; i < 3; i++) {                                                                        \
            T const v = in[i] > (T)1e-6 ? in[i] : (T)1e-6;                                               \
            l[i] = LN(v) / log_floor;                                                                    \
        }                                                                                                \
        for (i = 0; i < 3; i++) {                                                                        \
            T const s = l[0] * f[i] + l[1] * f[3 + i] + l[2] * f[6 + i];                                 \
            out[i] = s > (T)0 ? s : (T)0;                                                                \
        }                                                                                                \
    }                                                                                                    \
    static void alwan__stain_combine_##SFX(T out[3], T const in[3], T const r[9], T neg_log_floor) {   \
        T a[3];                                                                                          \
        int i;                                                                                           \
        for (i = 0; i < 3; i++) a[i] = in[i] * neg_log_floor;                                            \
        for (i = 0; i < 3; i++) {                                                                        \
            T const v = EXP(-(a[0] * r[i] + a[1] * r[3 + i] + a[2] * r[6 + i]));                         \
            out[i] = v < (T)0 ? (T)0 : v > (T)1 ? (T)1 : v;                                              \
        }                                                                                                \
    }

/* alwan_math.h routes these to alwan's own functions in deterministic builds. */
#define ALWAN__STAIN_LN64  ALWAN_LN_F64
#define ALWAN__STAIN_EXP64 ALWAN_EXP_F64
#define ALWAN__STAIN_LN32  ALWAN_LN_F32
#define ALWAN__STAIN_EXP32 ALWAN_EXP_F32

/* The f64 kernels serve the typed entry points in every build. */
ALWAN__STAIN_KERNELS(double, f64, ALWAN__STAIN_LN64, ALWAN__STAIN_EXP64)
#if ALWAN_WITH_F32
ALWAN__STAIN_KERNELS(float, f32, ALWAN__STAIN_LN32, ALWAN__STAIN_EXP32)
#endif

/* One direction, one precision, interleaved; planar shares it through a pointer per channel. */
#define ALWAN__STAIN_MAPS(T, SFX, MAT)                                                                    \
    static alwan_status alwan__stain_run_##SFX(char *o0, char *o1, char *o2, size_t out_stride,          \
                                               char const *i0, char const *i1, char const *i2,           \
                                               size_t in_stride, size_t count, alwan_stain_set set,      \
                                               MAT const *custom, int separate) {                         \
        double rf[9], fr[9], c[9];                                                                       \
        T m[9], log_floor;                                                                               \
        alwan_status st;                                                                                 \
        size_t k;                                                                                        \
        int i;                                                                                           \
        if (custom)                                                                                      \
            for (i = 0; i < 9; i++) c[i] = (double)custom->m[i];                                         \
        st = alwan__stain_matrices(rf, fr, set, custom ? c : NULL);                                      \
        if (st != ALWAN_OK) return st;                                                                   \
        for (i = 0; i < 9; i++) m[i] = (T)(separate ? fr[i] : rf[i]);                                    \
        log_floor = (T)ALWAN__STAIN_LN64(1e-6);                                                          \
        for (k = 0; k < count; k++) {                                                                    \
            T in[3], out[3];                                                                             \
            in[0] = *(T const *)(i0 + k * in_stride);                                                    \
            in[1] = *(T const *)(i1 + k * in_stride);                                                    \
            in[2] = *(T const *)(i2 + k * in_stride);                                                    \
            if (separate) alwan__stain_separate_##SFX(out, in, m, log_floor);                            \
            else alwan__stain_combine_##SFX(out, in, m, -log_floor);                                     \
            *(T *)(o0 + k * out_stride) = out[0];                                                        \
            *(T *)(o1 + k * out_stride) = out[1];                                                        \
            *(T *)(o2 + k * out_stride) = out[2];                                                        \
        }                                                                                                \
        return ALWAN_OK;                                                                                 \
    }                                                                                                    \
    alwan_status alwan_rgb_to_stains_##SFX##_map_interleave(T *out, size_t out_stride, T const *rgb_in,   \
                                                            size_t in_stride, size_t count,               \
                                                            alwan_stain_set set, MAT const *custom) {     \
        if (!out || !rgb_in || count == 0) return ALWAN_E_INVALID;                                       \
        return alwan__stain_run_##SFX((char *)out, (char *)(out + 1), (char *)(out + 2), out_stride,     \
                                      (char const *)rgb_in, (char const *)(rgb_in + 1),                  \
                                      (char const *)(rgb_in + 2), in_stride, count, set, custom, 1);     \
    }                                                                                                    \
    alwan_status alwan_stains_to_rgb_##SFX##_map_interleave(T *rgb_out, size_t out_stride, T const *in,   \
                                                            size_t in_stride, size_t count,               \
                                                            alwan_stain_set set, MAT const *custom) {     \
        if (!rgb_out || !in || count == 0) return ALWAN_E_INVALID;                                       \
        return alwan__stain_run_##SFX((char *)rgb_out, (char *)(rgb_out + 1), (char *)(rgb_out + 2),     \
                                      out_stride, (char const *)in, (char const *)(in + 1),              \
                                      (char const *)(in + 2), in_stride, count, set, custom, 0);         \
    }                                                                                                    \
    alwan_status alwan_rgb_to_stains_##SFX##_map_planar(T *out_ch0, size_t out_stride, T *out_ch1,       \
                                                        T *out_ch2, T const *in_ch0, size_t in_stride,   \
                                                        T const *in_ch1, T const *in_ch2, size_t count,  \
                                                        alwan_stain_set set, MAT const *custom) {        \
        if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2 || count == 0)             \
            return ALWAN_E_INVALID;                                                                      \
        return alwan__stain_run_##SFX((char *)out_ch0, (char *)out_ch1, (char *)out_ch2, out_stride,     \
                                      (char const *)in_ch0, (char const *)in_ch1, (char const *)in_ch2,  \
                                      in_stride, count, set, custom, 1);                                 \
    }                                                                                                    \
    alwan_status alwan_stains_to_rgb_##SFX##_map_planar(T *out_ch0, size_t out_stride, T *out_ch1,       \
                                                        T *out_ch2, T const *in_ch0, size_t in_stride,   \
                                                        T const *in_ch1, T const *in_ch2, size_t count,  \
                                                        alwan_stain_set set, MAT const *custom) {        \
        if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2 || count == 0)             \
            return ALWAN_E_INVALID;                                                                      \
        return alwan__stain_run_##SFX((char *)out_ch0, (char *)out_ch1, (char *)out_ch2, out_stride,     \
                                      (char const *)in_ch0, (char const *)in_ch1, (char const *)in_ch2,  \
                                      in_stride, count, set, custom, 0);                                 \
    }

#if ALWAN_WITH_F64
ALWAN__STAIN_MAPS(double, f64, alwan_mat3x3_f64)
#endif
#if ALWAN_WITH_F32
ALWAN__STAIN_MAPS(float, f32, alwan_mat3x3_f32)
#endif

/* Typed I/O: each pixel read through the library's format loader (u8 as value / 255,
 * u16 as value / 65535, f16, f32 and f64 as they are), converted in double, stored
 * through its format writer. */
static alwan_status alwan__stain_run_ex(char *o0, char *o1, char *o2, size_t out_stride, alwan_pixel_format out_fmt,
                                        char const *i0, char const *i1, char const *i2, size_t in_stride,
                                        alwan_pixel_format in_fmt, size_t count, int planar, alwan_stain_set set,
                                        alwan_mat3x3_f64 const *custom, int separate) {
    double rf[9], fr[9], m[9], log_floor;
    alwan_status st;
    size_t k;
    st = alwan__stain_matrices(rf, fr, set, custom ? custom->m : NULL);
    if (st != ALWAN_OK) return st;
    {
        int i;
        for (i = 0; i < 9; i++) m[i] = separate ? fr[i] : rf[i];
    }
    log_floor = ALWAN__STAIN_LN64(1e-6);
    for (k = 0; k < count; k++) {
        alwan_f64 in[3], out[3];
        if (planar) {
            in[0] = alwan__load1_typed(i0 + k * in_stride, in_fmt);
            in[1] = alwan__load1_typed(i1 + k * in_stride, in_fmt);
            in[2] = alwan__load1_typed(i2 + k * in_stride, in_fmt);
        } else {
            alwan__load3_typed(in, i0 + k * in_stride, in_fmt);
        }
        if (separate) alwan__stain_separate_f64(out, in, m, log_floor);
        else alwan__stain_combine_f64(out, in, m, -log_floor);
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

static int alwan__stain_fmt_ok(alwan_pixel_format f) {
    return f == ALWAN_PIXEL_U8 || f == ALWAN_PIXEL_U16 || f == ALWAN_PIXEL_F16 || f == ALWAN_PIXEL_F32 ||
           f == ALWAN_PIXEL_F64;
}

alwan_status alwan_rgb_to_stains_map_interleave_ex(void *out, size_t out_stride, void const *rgb_in, size_t in_stride,
                                                   size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                                   alwan_stain_set set, alwan_mat3x3_f64 const *custom) {
    if (!out || !rgb_in || count == 0 || !alwan__stain_fmt_ok(out_fmt) || !alwan__stain_fmt_ok(in_fmt))
        return ALWAN_E_INVALID;
    return alwan__stain_run_ex((char *)out, NULL, NULL, out_stride, out_fmt, (char const *)rgb_in, NULL, NULL,
                               in_stride, in_fmt, count, 0, set, custom, 1);
}

alwan_status alwan_stains_to_rgb_map_interleave_ex(void *rgb_out, size_t out_stride, void const *in, size_t in_stride,
                                                   size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                                   alwan_stain_set set, alwan_mat3x3_f64 const *custom) {
    if (!rgb_out || !in || count == 0 || !alwan__stain_fmt_ok(out_fmt) || !alwan__stain_fmt_ok(in_fmt))
        return ALWAN_E_INVALID;
    return alwan__stain_run_ex((char *)rgb_out, NULL, NULL, out_stride, out_fmt, (char const *)in, NULL, NULL,
                               in_stride, in_fmt, count, 0, set, custom, 0);
}

alwan_status alwan_rgb_to_stains_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0,
                                               size_t in_stride, void const *in1, void const *in2, size_t count,
                                               alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                               alwan_stain_set set, alwan_mat3x3_f64 const *custom) {
    if (!out0 || !out1 || !out2 || !in0 || !in1 || !in2 || count == 0 || !alwan__stain_fmt_ok(out_fmt) ||
        !alwan__stain_fmt_ok(in_fmt))
        return ALWAN_E_INVALID;
    return alwan__stain_run_ex((char *)out0, (char *)out1, (char *)out2, out_stride, out_fmt, (char const *)in0,
                               (char const *)in1, (char const *)in2, in_stride, in_fmt, count, 1, set, custom, 1);
}

alwan_status alwan_stains_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0,
                                               size_t in_stride, void const *in1, void const *in2, size_t count,
                                               alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                               alwan_stain_set set, alwan_mat3x3_f64 const *custom) {
    if (!out0 || !out1 || !out2 || !in0 || !in1 || !in2 || count == 0 || !alwan__stain_fmt_ok(out_fmt) ||
        !alwan__stain_fmt_ok(in_fmt))
        return ALWAN_E_INVALID;
    return alwan__stain_run_ex((char *)out0, (char *)out1, (char *)out2, out_stride, out_fmt, (char const *)in0,
                               (char const *)in1, (char const *)in2, in_stride, in_fmt, count, 1, set, custom, 0);
}
