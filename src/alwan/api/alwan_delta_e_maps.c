/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Buffer forms of the colour-difference metrics, none of which had one.
 *
 * Two strided buffers of triples in, one distance per pixel out. Each is its
 * scalar in a loop, so a map agrees with the scalar to the bit, which suite
 * 173 asserts. The scalars take no context and cannot fail, so the only status
 * a map returns is ALWAN_E_INVALID for a NULL buffer (or NULL params where the
 * scalar takes them).
 */

#include "../alwan.h"
#include "../alwan_internal.h"

/* A metric of two colours of one struct type. The triple is read positionally,
 * which is the struct's layout for every type here: three scalars in order. */
#define ALWAN_DE_MAP(name, T, TN, COLOUR)                                                              \
alwan_status alwan_delta_e_##name##_##TN##_map_interleave(T *out, size_t out_stride,                    \
        T const *a, size_t a_stride, T const *b, size_t b_stride, size_t count) {                        \
    size_t i;                                                                                            \
    if (!out || !a || !b) return ALWAN_E_INVALID;                                                        \
    for (i = 0; i < count; i++) {                                                                        \
        T const *pa = (T const *)((char const *)a + i * a_stride);                                       \
        T const *pb = (T const *)((char const *)b + i * b_stride);                                       \
        COLOUR ca, cb;                                                                                   \
        ALWAN_MEMCPY(&ca, pa, 3 * sizeof(T));                                                            \
        ALWAN_MEMCPY(&cb, pb, 3 * sizeof(T));                                                            \
        *(T *)((char *)out + i * out_stride) = alwan_delta_e_##name##_##TN(&ca, &cb);                    \
    }                                                                                                    \
    return ALWAN_OK;                                                                                     \
}

/* The same with a trailing argument the scalar takes: a params pointer, or the
 * `textiles` int of HyCH. */
#define ALWAN_DE_MAP_EXTRA(name, T, TN, COLOUR, PARAM_DECL, PARAM_CHECK, PARAM_USE)                       \
alwan_status alwan_delta_e_##name##_##TN##_map_interleave(T *out, size_t out_stride,                    \
        T const *a, size_t a_stride, T const *b, size_t b_stride, size_t count, PARAM_DECL) {            \
    size_t i;                                                                                            \
    if (!out || !a || !b || PARAM_CHECK) return ALWAN_E_INVALID;                                         \
    for (i = 0; i < count; i++) {                                                                        \
        T const *pa = (T const *)((char const *)a + i * a_stride);                                       \
        T const *pb = (T const *)((char const *)b + i * b_stride);                                       \
        COLOUR ca, cb;                                                                                   \
        ALWAN_MEMCPY(&ca, pa, 3 * sizeof(T));                                                            \
        ALWAN_MEMCPY(&cb, pb, 3 * sizeof(T));                                                            \
        *(T *)((char *)out + i * out_stride) = alwan_delta_e_##name##_##TN(&ca, &cb, PARAM_USE);         \
    }                                                                                                    \
    return ALWAN_OK;                                                                                     \
}

#define ALWAN_DE_MAPS_FOR(T, TN)                                                                       \
ALWAN_DE_MAP(76,        T, TN, alwan_lab_##TN)                                                         \
ALWAN_DE_MAP(94,        T, TN, alwan_lab_##TN)                                                         \
ALWAN_DE_MAP(2000,      T, TN, alwan_lab_##TN)                                                         \
ALWAN_DE_MAP(hyab,      T, TN, alwan_lab_##TN)                                                         \
ALWAN_DE_MAP(ok,        T, TN, alwan_oklab_##TN)                                                       \
ALWAN_DE_MAP(din99,     T, TN, alwan_din99_##TN)                                                       \
ALWAN_DE_MAP(zcam,      T, TN, alwan_jzazbz_##TN)                                                      \
ALWAN_DE_MAP(cam02_ucs, T, TN, alwan_cam_jab_##TN)                                                     \
ALWAN_DE_MAP(cam02_lcd, T, TN, alwan_cam_jab_##TN)                                                     \
ALWAN_DE_MAP(cam02_scd, T, TN, alwan_cam_jab_##TN)                                                     \
ALWAN_DE_MAP(cam16_ucs, T, TN, alwan_cam_jab_##TN)                                                     \
ALWAN_DE_MAP(cam16_lcd, T, TN, alwan_cam_jab_##TN)                                                     \
ALWAN_DE_MAP(cam16_scd, T, TN, alwan_cam_jab_##TN)                                                     \
ALWAN_DE_MAP_EXTRA(cmc,  T, TN, alwan_lab_##TN, alwan_delta_e_cmc_params_##TN const *params, !params, params) \
ALWAN_DE_MAP_EXTRA(itp,  T, TN, alwan_ictcp_##TN, alwan_delta_e_itp_params_##TN const *params, !params, params) \
ALWAN_DE_MAP_EXTRA(hych, T, TN, alwan_lab_##TN, int textiles, 0, textiles)

#if ALWAN_WITH_F32
ALWAN_DE_MAPS_FOR(alwan_f32, f32)
#endif

#if ALWAN_WITH_F64
ALWAN_DE_MAPS_FOR(alwan_f64, f64)
#endif
