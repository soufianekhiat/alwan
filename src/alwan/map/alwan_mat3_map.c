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

/* ----------------------------------------------------------------
 * Bulk Matrix-Vector Transform (interleave)
 * ---------------------------------------------------------------- */

int alwan_mat3_transform_map_interleave(alwan_scalar *vec_out,
                              alwan_mat3x3 const *matrix,
                              alwan_scalar const *vec_in,
                              size_t count,
                              size_t in_stride,
                              size_t out_stride) {
    if (!vec_in || !vec_out || !matrix || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)vec_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)vec_out + i * out_stride);

        alwan_vec3 v_in, v_out;
        v_in.v[0] = in_ptr[0];
        v_in.v[1] = in_ptr[1];
        v_in.v[2] = in_ptr[2];

        alwan_mat3_mulv(&v_out, matrix, &v_in);

        out_ptr[0] = v_out.v[0];
        out_ptr[1] = v_out.v[1];
        out_ptr[2] = v_out.v[2];
    }

    return ALWAN_OK;
}

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
