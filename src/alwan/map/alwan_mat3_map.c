/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Bulk Mat3 transform (interleave + _ex)
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_math_core.h"

/* === f32 pass === */
#define ALWAN_MAP_F32
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_mat3_map_kernels.inc"
#include "alwan_map_simd_undef.h"

/* === f64 pass === */
#define ALWAN_MAP_F64
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_mat3_map_kernels.inc"
#include "alwan_map_simd_undef.h"

/* ----------------------------------------------------------------
 * Bulk Matrix-Vector Transform _ex (typed I/O)
 * ---------------------------------------------------------------- */

int alwan_mat3_transform_map_interleave_ex(void *vec_out, alwan_pixel_format out_fmt,
                                 alwan_mat3x3 const *matrix,
                                 void const *vec_in, alwan_pixel_format in_fmt,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!vec_in || !vec_out || !matrix || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar s[3];
        alwan__load3_typed(s, (char const *)vec_in + i * in_stride, in_fmt);
        alwan_vec3 v = {{s[0], s[1], s[2]}};
        alwan_vec3 r;
        alwan_mat3_mulv(&r, matrix, &v);
        alwan_scalar d[3] = {r.v[0], r.v[1], r.v[2]};
        alwan__store3_typed((char *)vec_out + i * out_stride, d, out_fmt);
    }
    return ALWAN_OK;
}
