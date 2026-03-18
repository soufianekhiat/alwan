/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Bulk Mat3 transform kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * Bulk Matrix-Vector Transform (interleave)
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_mat3_transform, _map_interleave)(
    alwan_map_lane *vec_out,
    ALWAN_MAP_LIT(alwan_mat3x3) const *matrix,
    alwan_map_lane const *vec_in,
    size_t count,
    size_t in_stride,
    size_t out_stride)
{
    if (!vec_in || !vec_out || !matrix || count == 0) {
        return ALWAN_E_INVALID;
    }

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, vec_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_lane const *M = matrix->m;
                alwan_map_simd m00 = alwan_map_simd_set1(M[0]), m01 = alwan_map_simd_set1(M[1]), m02 = alwan_map_simd_set1(M[2]);
                alwan_map_simd m10 = alwan_map_simd_set1(M[3]), m11 = alwan_map_simd_set1(M[4]), m12 = alwan_map_simd_set1(M[5]);
                alwan_map_simd m20 = alwan_map_simd_set1(M[6]), m21 = alwan_map_simd_set1(M[7]), m22 = alwan_map_simd_set1(M[8]);
                for (; i + W <= tile; i += W) {
                    alwan_map_simd v0 = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd v1 = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd v2 = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(m00, v0, alwan_map_simd_fmadd(m01, v1, alwan_map_simd_mul(m02, v2))));
                    alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(m10, v0, alwan_map_simd_fmadd(m11, v1, alwan_map_simd_mul(m12, v2))));
                    alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(m20, v0, alwan_map_simd_fmadd(m21, v1, alwan_map_simd_mul(m22, v2))));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_vec3) v_in = {{(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]}};
                ALWAN_MAP_LIT(alwan_vec3) v_out = ALWAN_MAP_LITV(alwan_mat3_mulv)(*matrix, v_in);
                d0[i] = (alwan_map_lane)v_out.v[0]; d1[i] = (alwan_map_lane)v_out.v[1]; d2[i] = (alwan_map_lane)v_out.v[2];
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(vec_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}
