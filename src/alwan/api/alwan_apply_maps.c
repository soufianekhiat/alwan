/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Buffer forms of the last per-pixel operations that had none: IPT <-> IPTch,
 * the Jzczhz HDR gamut map, the two gamut mappers that take a space, and the
 * two polynomial colour corrections.
 *
 * Each is its scalar in a loop, the arguments validated once, so a map agrees
 * with its scalar twin to the bit, which suite 174 asserts. Three scalars in,
 * three out, strides in bytes, and whatever the scalar takes after the pixel
 * comes after count, with the context last where there is one.
 *
 * The planar twin of each of the seven is at the bottom of this file.
 */

#include "../alwan.h"
#include "../alwan_internal.h"

/* A void scalar of one struct type in and one out, no extra argument. */
#define ALWAN_APPLY_MAP_VOID(name, T, TN, IN, OUT)                                                       \
alwan_status alwan_##name##_##TN##_map_interleave(T *out, size_t out_stride,                             \
        T const *in, size_t in_stride, size_t count) {                                                   \
    size_t i;                                                                                            \
    if (!out || !in) return ALWAN_E_INVALID;                                                             \
    for (i = 0; i < count; i++) {                                                                        \
        T const *src = (T const *)((char const *)in + i * in_stride);                                    \
        T *dst = (T *)((char *)out + i * out_stride);                                                    \
        IN a; OUT r;                                                                                     \
        ALWAN_MEMCPY(&a, src, 3 * sizeof(T));                                                            \
        alwan_##name##_##TN(&r, &a);                                                                     \
        ALWAN_MEMCPY(dst, &r, 3 * sizeof(T));                                                            \
    }                                                                                                    \
    return ALWAN_OK;                                                                                     \
}

/* A void scalar with trailing arguments. */
#define ALWAN_APPLY_MAP_VOID_ARGS(name, T, TN, IN, OUT, PARAMS, CHECK, ARGS)                             \
alwan_status alwan_##name##_##TN##_map_interleave(T *out, size_t out_stride,                             \
        T const *in, size_t in_stride, size_t count, PARAMS) {                                           \
    size_t i;                                                                                            \
    if (!out || !in || (CHECK)) return ALWAN_E_INVALID;                                                  \
    for (i = 0; i < count; i++) {                                                                        \
        T const *src = (T const *)((char const *)in + i * in_stride);                                    \
        T *dst = (T *)((char *)out + i * out_stride);                                                    \
        IN a; OUT r;                                                                                     \
        ALWAN_MEMCPY(&a, src, 3 * sizeof(T));                                                            \
        alwan_##name##_##TN(&r, &a, ARGS);                                                               \
        ALWAN_MEMCPY(dst, &r, 3 * sizeof(T));                                                            \
    }                                                                                                    \
    return ALWAN_OK;                                                                                     \
}

#define ALWAN_COMMA ,

#define ALWAN_APPLY_MAPS_FOR(T, TN)                                                                      \
ALWAN_APPLY_MAP_VOID(ipt_to_iptch, T, TN, alwan_ipt_##TN, alwan_iptch_##TN)                              \
ALWAN_APPLY_MAP_VOID(iptch_to_ipt, T, TN, alwan_iptch_##TN, alwan_ipt_##TN)                              \
ALWAN_APPLY_MAP_VOID_ARGS(hdr_gamut_map_jzczhz, T, TN, alwan_jzczhz_##TN, alwan_jzczhz_##TN,             \
                          T Cz_max, 0, Cz_max)                                                           \
ALWAN_APPLY_MAP_VOID_ARGS(colour_correct_cheung2004, T, TN, alwan_rgb_##TN, alwan_rgb_##TN,              \
                          T const *matrix ALWAN_COMMA alwan_poly_cheung_terms terms, !matrix,             \
                          matrix ALWAN_COMMA terms)                                                      \
ALWAN_APPLY_MAP_VOID_ARGS(colour_correct_finlayson2015, T, TN, alwan_rgb_##TN, alwan_rgb_##TN,           \
                          T const *matrix ALWAN_COMMA int degree ALWAN_COMMA int root_poly, !matrix,     \
                          matrix ALWAN_COMMA degree ALWAN_COMMA root_poly)                               \
                                                                                                         \
/* The two gamut mappers return a status per pixel, which the map passes on. */                          \
alwan_status alwan_gamut_map_advanced_##TN##_map_interleave(T *out, size_t out_stride,                   \
        T const *in, size_t in_stride, size_t count, alwan_gamut_map_method method,                      \
        alwan_rgb_space_desc_##TN const *space) {                                                        \
    size_t i;                                                                                            \
    if (!out || !in || !space) return ALWAN_E_INVALID;                                                   \
    for (i = 0; i < count; i++) {                                                                        \
        T const *src = (T const *)((char const *)in + i * in_stride);                                    \
        T *dst = (T *)((char *)out + i * out_stride);                                                    \
        alwan_rgb_##TN a = { src[0], src[1], src[2] }, r;                                                \
        alwan_status status = alwan_gamut_map_advanced_##TN(&r, method, space, &a);                      \
        if (status != ALWAN_OK) return status;                                                           \
        dst[0] = r.r; dst[1] = r.g; dst[2] = r.b;                                                        \
    }                                                                                                    \
    return ALWAN_OK;                                                                                     \
}                                                                                                        \
alwan_status alwan_gamut_map_xyz_to_rgb_##TN##_map_interleave(T *out, size_t out_stride,                 \
        T const *in, size_t in_stride, size_t count, alwan_rgb_space_desc_##TN const *space,             \
        alwan_ctx *ctx) {                                                                                \
    size_t i;                                                                                            \
    if (!out || !in || !space) return ALWAN_E_INVALID;                                                   \
    for (i = 0; i < count; i++) {                                                                        \
        T const *src = (T const *)((char const *)in + i * in_stride);                                    \
        T *dst = (T *)((char *)out + i * out_stride);                                                    \
        alwan_xyz_##TN a = { src[0], src[1], src[2] };                                                   \
        alwan_rgb_##TN r;                                                                                \
        alwan_status status = alwan_gamut_map_xyz_to_rgb_##TN(&r, space, &a, ctx);                       \
        if (status != ALWAN_OK) return status;                                                           \
        dst[0] = r.r; dst[1] = r.g; dst[2] = r.b;                                                        \
    }                                                                                                    \
    return ALWAN_OK;                                                                                     \
}

#if ALWAN_WITH_F32
ALWAN_APPLY_MAPS_FOR(alwan_f32, f32)
#endif

#if ALWAN_WITH_F64
ALWAN_APPLY_MAPS_FOR(alwan_f64, f64)
#endif

/* ----------------------------------------------------------------
 * Planar twins
 *
 * The same operations over three separate channel planes instead of
 * interleaved triples, which is the shape a video pipeline and a
 * plane-per-channel image already hold. One stride is shared by the three
 * planes, as everywhere else in this convention, and the extra arguments keep
 * the interleave order. A planar call is its interleave twin to the bit, which
 * suite 174 asserts on the same pixels.
 * ---------------------------------------------------------------- */

#define ALWAN_PLANAR_GUARD()                                                                             \
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;

/* Read one pixel out of the three input planes into the scalar's own struct,
 * and write the scalar's result back across the three output planes. The
 * struct is read and written as three contiguous T, which is what the
 * interleave twin does to the same memory, so the two cannot drift. */
#define ALWAN_PLANAR_READ(T, IN, a)                                                                      \
    T src_[3];                                                                                           \
    IN a;                                                                                                \
    src_[0] = *(T const *)((char const *)in_ch0 + i * in_stride);                                        \
    src_[1] = *(T const *)((char const *)in_ch1 + i * in_stride);                                        \
    src_[2] = *(T const *)((char const *)in_ch2 + i * in_stride);                                        \
    ALWAN_MEMCPY(&a, src_, 3 * sizeof(T));

#define ALWAN_PLANAR_WRITE(T, r)                                                                         \
    {                                                                                                    \
        T dst_[3];                                                                                       \
        ALWAN_MEMCPY(dst_, &r, 3 * sizeof(T));                                                           \
        *(T *)((char *)out_ch0 + i * out_stride) = dst_[0];                                              \
        *(T *)((char *)out_ch1 + i * out_stride) = dst_[1];                                              \
        *(T *)((char *)out_ch2 + i * out_stride) = dst_[2];                                              \
    }

#define ALWAN_APPLY_PLANAR_VOID(name, T, TN, IN, OUT)                                                    \
alwan_status alwan_##name##_##TN##_map_planar(T *out_ch0, size_t out_stride, T *out_ch1, T *out_ch2,      \
        T const *in_ch0, size_t in_stride, T const *in_ch1, T const *in_ch2, size_t count) {             \
    size_t i;                                                                                            \
    ALWAN_PLANAR_GUARD()                                                                                 \
    for (i = 0; i < count; i++) {                                                                        \
        OUT r;                                                                                           \
        ALWAN_PLANAR_READ(T, IN, a)                                                                      \
        alwan_##name##_##TN(&r, &a);                                                                     \
        ALWAN_PLANAR_WRITE(T, r)                                                                         \
    }                                                                                                    \
    return ALWAN_OK;                                                                                     \
}

#define ALWAN_APPLY_PLANAR_VOID_ARGS(name, T, TN, IN, OUT, PARAMS, CHECK, ARGS)                          \
alwan_status alwan_##name##_##TN##_map_planar(T *out_ch0, size_t out_stride, T *out_ch1, T *out_ch2,      \
        T const *in_ch0, size_t in_stride, T const *in_ch1, T const *in_ch2, size_t count, PARAMS) {     \
    size_t i;                                                                                            \
    ALWAN_PLANAR_GUARD()                                                                                 \
    if (CHECK) return ALWAN_E_INVALID;                                                                   \
    for (i = 0; i < count; i++) {                                                                        \
        OUT r;                                                                                           \
        ALWAN_PLANAR_READ(T, IN, a)                                                                      \
        alwan_##name##_##TN(&r, &a, ARGS);                                                               \
        ALWAN_PLANAR_WRITE(T, r)                                                                         \
    }                                                                                                    \
    return ALWAN_OK;                                                                                     \
}

#define ALWAN_APPLY_PLANAR_FOR(T, TN)                                                                    \
ALWAN_APPLY_PLANAR_VOID(ipt_to_iptch, T, TN, alwan_ipt_##TN, alwan_iptch_##TN)                           \
ALWAN_APPLY_PLANAR_VOID(iptch_to_ipt, T, TN, alwan_iptch_##TN, alwan_ipt_##TN)                           \
ALWAN_APPLY_PLANAR_VOID_ARGS(hdr_gamut_map_jzczhz, T, TN, alwan_jzczhz_##TN, alwan_jzczhz_##TN,          \
                             T Cz_max, 0, Cz_max)                                                        \
ALWAN_APPLY_PLANAR_VOID_ARGS(colour_correct_cheung2004, T, TN, alwan_rgb_##TN, alwan_rgb_##TN,           \
                             T const *matrix ALWAN_COMMA alwan_poly_cheung_terms terms, !matrix,         \
                             matrix ALWAN_COMMA terms)                                                   \
ALWAN_APPLY_PLANAR_VOID_ARGS(colour_correct_finlayson2015, T, TN, alwan_rgb_##TN, alwan_rgb_##TN,        \
                             T const *matrix ALWAN_COMMA int degree ALWAN_COMMA int root_poly, !matrix,  \
                             matrix ALWAN_COMMA degree ALWAN_COMMA root_poly)                            \
                                                                                                         \
/* The two gamut mappers return a status per pixel, which the map passes on. */                          \
alwan_status alwan_gamut_map_advanced_##TN##_map_planar(T *out_ch0, size_t out_stride, T *out_ch1,        \
        T *out_ch2, T const *in_ch0, size_t in_stride, T const *in_ch1, T const *in_ch2, size_t count,   \
        alwan_gamut_map_method method, alwan_rgb_space_desc_##TN const *space) {                         \
    size_t i;                                                                                            \
    ALWAN_PLANAR_GUARD()                                                                                 \
    if (!space) return ALWAN_E_INVALID;                                                                  \
    for (i = 0; i < count; i++) {                                                                        \
        alwan_rgb_##TN r;                                                                                \
        alwan_status status;                                                                             \
        ALWAN_PLANAR_READ(T, alwan_rgb_##TN, a)                                                          \
        status = alwan_gamut_map_advanced_##TN(&r, method, space, &a);                                   \
        if (status != ALWAN_OK) return status;                                                           \
        ALWAN_PLANAR_WRITE(T, r)                                                                         \
    }                                                                                                    \
    return ALWAN_OK;                                                                                     \
}                                                                                                        \
alwan_status alwan_gamut_map_xyz_to_rgb_##TN##_map_planar(T *out_ch0, size_t out_stride, T *out_ch1,      \
        T *out_ch2, T const *in_ch0, size_t in_stride, T const *in_ch1, T const *in_ch2, size_t count,   \
        alwan_rgb_space_desc_##TN const *space, alwan_ctx *ctx) {                                        \
    size_t i;                                                                                            \
    ALWAN_PLANAR_GUARD()                                                                                 \
    if (!space) return ALWAN_E_INVALID;                                                                  \
    for (i = 0; i < count; i++) {                                                                        \
        alwan_rgb_##TN r;                                                                                \
        alwan_status status;                                                                             \
        ALWAN_PLANAR_READ(T, alwan_xyz_##TN, a)                                                          \
        status = alwan_gamut_map_xyz_to_rgb_##TN(&r, space, &a, ctx);                                    \
        if (status != ALWAN_OK) return status;                                                           \
        ALWAN_PLANAR_WRITE(T, r)                                                                         \
    }                                                                                                    \
    return ALWAN_OK;                                                                                     \
}

#if ALWAN_WITH_F32
ALWAN_APPLY_PLANAR_FOR(alwan_f32, f32)
#endif

#if ALWAN_WITH_F64
ALWAN_APPLY_PLANAR_FOR(alwan_f64, f64)
#endif

