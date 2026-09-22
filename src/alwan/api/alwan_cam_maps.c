/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Buffer forms of the appearance models that had none: Hellwig 2022,
 * Kim 2009, Hunt, LLAB, ATD95, Nayatani 95, RLAB, CAM18sl and CAM20u.
 *
 * Each is its scalar in a loop, the arguments validated once. None of these
 * scalars carries per-call state worth hoisting the way ZCAM's does, so the
 * per-pixel arithmetic is exactly the scalar's and a map agrees with its
 * scalar twin to the bit, which suite 172 asserts. The XYZ side is a strided
 * buffer of triples; the correlates side is one struct per pixel, as the
 * CIECAM02, CAM16 and ZCAM maps have it.
 */

#include "../alwan.h"
#include "../alwan_internal.h"

/* A model whose scalar takes a viewing-conditions struct. */
#define ALWAN_CAM_MAP_VC(model, T, TN, CORR, VC)                                                            \
alwan_status alwan_##model##_forward_##TN##_map_interleave(CORR *correlates_out, T const *xyz_in,          \
        size_t in_stride, VC const *vc, size_t count) {                                                     \
    size_t i;                                                                                               \
    if (!correlates_out || !xyz_in || !vc) return ALWAN_E_INVALID;                                          \
    for (i = 0; i < count; i++) {                                                                           \
        T const *src = (T const *)((char const *)xyz_in + i * in_stride);                                   \
        alwan_xyz_##TN xyz = { src[0], src[1], src[2] };                                                    \
        alwan_status status = alwan_##model##_forward_##TN(&correlates_out[i], &xyz, vc);                   \
        if (status != ALWAN_OK) return status;                                                              \
    }                                                                                                       \
    return ALWAN_OK;                                                                                        \
}

#define ALWAN_CAM_MAP_VC_INV(model, T, TN, CORR, VC)                                                        \
alwan_status alwan_##model##_inverse_##TN##_map_interleave(T *xyz_out, size_t out_stride,                  \
        CORR const *correlates_in, VC const *vc, size_t count) {                                            \
    size_t i;                                                                                               \
    if (!xyz_out || !correlates_in || !vc) return ALWAN_E_INVALID;                                          \
    for (i = 0; i < count; i++) {                                                                           \
        T *dst = (T *)((char *)xyz_out + i * out_stride);                                                   \
        alwan_xyz_##TN xyz;                                                                                 \
        alwan_status status = alwan_##model##_inverse_##TN(&xyz, &correlates_in[i], vc);                    \
        if (status != ALWAN_OK) return status;                                                              \
        dst[0] = xyz.x; dst[1] = xyz.y; dst[2] = xyz.z;                                                     \
    }                                                                                                       \
    return ALWAN_OK;                                                                                        \
}

/* A model whose scalar takes plain scalars: CAM18sl (Y_b), CAM20u (Y_b, L_a). */
#define ALWAN_CAM_MAP_ARGS(model, T, TN, CORR, PARAMS, ARGS)                                                \
alwan_status alwan_##model##_forward_##TN##_map_interleave(CORR *correlates_out, T const *xyz_in,          \
        size_t in_stride, PARAMS, size_t count) {                                                           \
    size_t i;                                                                                               \
    if (!correlates_out || !xyz_in) return ALWAN_E_INVALID;                                                 \
    for (i = 0; i < count; i++) {                                                                           \
        T const *src = (T const *)((char const *)xyz_in + i * in_stride);                                   \
        alwan_xyz_##TN xyz = { src[0], src[1], src[2] };                                                    \
        alwan_status status = alwan_##model##_forward_##TN(&correlates_out[i], &xyz, ARGS);                 \
        if (status != ALWAN_OK) return status;                                                              \
    }                                                                                                       \
    return ALWAN_OK;                                                                                        \
}                                                                                                           \
alwan_status alwan_##model##_inverse_##TN##_map_interleave(T *xyz_out, size_t out_stride,                  \
        CORR const *correlates_in, PARAMS, size_t count) {                                                  \
    size_t i;                                                                                               \
    if (!xyz_out || !correlates_in) return ALWAN_E_INVALID;                                                 \
    for (i = 0; i < count; i++) {                                                                           \
        T *dst = (T *)((char *)xyz_out + i * out_stride);                                                   \
        alwan_xyz_##TN xyz;                                                                                 \
        alwan_status status = alwan_##model##_inverse_##TN(&xyz, &correlates_in[i], ARGS);                  \
        if (status != ALWAN_OK) return status;                                                              \
        dst[0] = xyz.x; dst[1] = xyz.y; dst[2] = xyz.z;                                                     \
    }                                                                                                       \
    return ALWAN_OK;                                                                                        \
}

#define ALWAN_CAM_MAPS_FOR(T, TN)                                                                           \
ALWAN_CAM_MAP_VC(hellwig2022, T, TN, alwan_hellwig2022_correlates_##TN, alwan_hellwig2022_viewing_conditions_##TN) \
ALWAN_CAM_MAP_VC_INV(hellwig2022, T, TN, alwan_hellwig2022_correlates_##TN, alwan_hellwig2022_viewing_conditions_##TN) \
ALWAN_CAM_MAP_VC(kim2009, T, TN, alwan_kim2009_correlates_##TN, alwan_kim2009_viewing_conditions_##TN)     \
ALWAN_CAM_MAP_VC_INV(kim2009, T, TN, alwan_kim2009_correlates_##TN, alwan_kim2009_viewing_conditions_##TN) \
ALWAN_CAM_MAP_VC(hunt, T, TN, alwan_hunt_correlates_##TN, alwan_hunt_viewing_conditions_##TN)              \
ALWAN_CAM_MAP_VC_INV(hunt, T, TN, alwan_hunt_correlates_##TN, alwan_hunt_viewing_conditions_##TN)          \
ALWAN_CAM_MAP_VC(llab, T, TN, alwan_llab_correlates_##TN, alwan_llab_viewing_conditions_##TN)              \
ALWAN_CAM_MAP_VC(atd95, T, TN, alwan_atd95_correlates_##TN, alwan_atd95_viewing_conditions_##TN)           \
ALWAN_CAM_MAP_VC(nayatani95, T, TN, alwan_nayatani95_correlates_##TN, alwan_nayatani95_viewing_conditions_##TN) \
ALWAN_CAM_MAP_VC(rlab, T, TN, alwan_rlab_correlates_##TN, alwan_rlab_viewing_conditions_##TN)              \
ALWAN_CAM_MAP_VC_INV(rlab, T, TN, alwan_rlab_correlates_##TN, alwan_rlab_viewing_conditions_##TN)          \
ALWAN_CAM_MAP_ARGS(cam18sl, T, TN, alwan_cam18sl_correlates_##TN, T Y_b, Y_b)                              \
ALWAN_CAM_MAP_ARGS(cam20u, T, TN, alwan_cam20u_correlates_##TN, ALWAN_CAM20U_PARAMS(T), Y_b ALWAN_COMMA L_a)

#define ALWAN_COMMA ,
#define ALWAN_CAM20U_PARAMS(T) T Y_b, T L_a

#if ALWAN_WITH_F32
ALWAN_CAM_MAPS_FOR(alwan_f32, f32)
#endif

#if ALWAN_WITH_F64
ALWAN_CAM_MAPS_FOR(alwan_f64, f64)
#endif
