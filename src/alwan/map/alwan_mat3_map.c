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

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, vec_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_SIMD_WIDTH;
                alwan_scalar const *M = matrix->m;
                alwan_simd m00 = alwan_simd_set1(M[0]), m01 = alwan_simd_set1(M[1]), m02 = alwan_simd_set1(M[2]);
                alwan_simd m10 = alwan_simd_set1(M[3]), m11 = alwan_simd_set1(M[4]), m12 = alwan_simd_set1(M[5]);
                alwan_simd m20 = alwan_simd_set1(M[6]), m21 = alwan_simd_set1(M[7]), m22 = alwan_simd_set1(M[8]);
                for (; i + W <= tile; i += W) {
                    alwan_simd v0 = alwan_simd_load(&c0[i]);
                    alwan_simd v1 = alwan_simd_load(&c1[i]);
                    alwan_simd v2 = alwan_simd_load(&c2[i]);
                    alwan_simd_store(&d0[i], alwan_simd_fmadd(m00, v0, alwan_simd_fmadd(m01, v1, alwan_simd_mul(m02, v2))));
                    alwan_simd_store(&d1[i], alwan_simd_fmadd(m10, v0, alwan_simd_fmadd(m11, v1, alwan_simd_mul(m12, v2))));
                    alwan_simd_store(&d2[i], alwan_simd_fmadd(m20, v0, alwan_simd_fmadd(m21, v1, alwan_simd_mul(m22, v2))));
                }
            }
#endif
            for (; i < tile; i++) {
                alwan_vec3 v_in = {{(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]}};
                alwan_vec3 v_out;
                alwan_mat3_mulv(&v_out, matrix, &v_in);
                d0[i] = (alwan_simd_lane)v_out.v[0]; d1[i] = (alwan_simd_lane)v_out.v[1]; d2[i] = (alwan_simd_lane)v_out.v[2];
            }
        }
        alwan__store_tile_aos3(vec_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
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
