/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Color Appearance Model kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* CAM functions use struct I/O - tiled path processes XYZ side only */

/* ----------------------------------------------------------------
 * Map CIECAM02 Conversions
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_ciecam02_forward, _map_interleave)(
    alwan_ciecam02_correlates *correlates_out,
    alwan_map_lane const *xyz_in,
    alwan_ciecam02_viewing_conditions const *vc,
    size_t count, size_t in_stride)
{
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_map_lane ci0[ALWAN_MAP_TILE_PIXELS], ci1[ALWAN_MAP_TILE_PIXELS], ci2[ALWAN_MAP_TILE_PIXELS];
            ALWAN_MAP_NAME(alwan__load_tile_aos3)(ci0, ci1, ci2, xyz_in, off, in_stride, tile);
            for (size_t j = 0; j < tile; j++) {
                alwan_xyz xyz = {(alwan_scalar)ci0[j], (alwan_scalar)ci1[j], (alwan_scalar)ci2[j]};
                int status = alwan_ciecam02_forward(&correlates_out[off + j], &xyz, vc);
                if (status != ALWAN_OK) return status;
            }
            off += tile;
        }
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)xyz_in + i * in_stride);
        alwan_xyz xyz = {(alwan_scalar)in_ptr[0], (alwan_scalar)in_ptr[1], (alwan_scalar)in_ptr[2]};
        int status = alwan_ciecam02_forward(&correlates_out[i], &xyz, vc);
        if (status != ALWAN_OK) return status;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_ciecam02_inverse, _map_interleave)(
    alwan_map_lane *xyz_out,
    alwan_ciecam02_correlates const *correlates_in,
    alwan_ciecam02_viewing_conditions const *vc,
    size_t count, size_t out_stride)
{
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_map_lane co0[ALWAN_MAP_TILE_PIXELS], co1[ALWAN_MAP_TILE_PIXELS], co2[ALWAN_MAP_TILE_PIXELS];
            for (size_t j = 0; j < tile; j++) {
                alwan_xyz xyz;
                int status = alwan_ciecam02_inverse(&xyz, &correlates_in[off + j], vc);
                if (status != ALWAN_OK) return status;
                co0[j] = (alwan_map_lane)xyz.x; co1[j] = (alwan_map_lane)xyz.y; co2[j] = (alwan_map_lane)xyz.z;
            }
            ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, off, out_stride, co0, co1, co2, tile);
            off += tile;
        }
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)xyz_out + i * out_stride);
        alwan_xyz xyz;
        int status = alwan_ciecam02_inverse(&xyz, &correlates_in[i], vc);
        if (status != ALWAN_OK) return status;
        out_ptr[0] = (alwan_map_lane)xyz.x; out_ptr[1] = (alwan_map_lane)xyz.y; out_ptr[2] = (alwan_map_lane)xyz.z;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Map CAM16 Conversions
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_cam16_forward, _map_interleave)(
    alwan_cam16_correlates *correlates_out,
    alwan_map_lane const *xyz_in,
    alwan_cam16_viewing_conditions const *vc,
    size_t count, size_t in_stride)
{
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_map_lane ci0[ALWAN_MAP_TILE_PIXELS], ci1[ALWAN_MAP_TILE_PIXELS], ci2[ALWAN_MAP_TILE_PIXELS];
            ALWAN_MAP_NAME(alwan__load_tile_aos3)(ci0, ci1, ci2, xyz_in, off, in_stride, tile);
            for (size_t j = 0; j < tile; j++) {
                alwan_xyz xyz = {(alwan_scalar)ci0[j], (alwan_scalar)ci1[j], (alwan_scalar)ci2[j]};
                int status = alwan_cam16_forward(&correlates_out[off + j], &xyz, vc);
                if (status != ALWAN_OK) return status;
            }
            off += tile;
        }
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)xyz_in + i * in_stride);
        alwan_xyz xyz = {(alwan_scalar)in_ptr[0], (alwan_scalar)in_ptr[1], (alwan_scalar)in_ptr[2]};
        int status = alwan_cam16_forward(&correlates_out[i], &xyz, vc);
        if (status != ALWAN_OK) return status;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_cam16_inverse, _map_interleave)(
    alwan_map_lane *xyz_out,
    alwan_cam16_correlates const *correlates_in,
    alwan_cam16_viewing_conditions const *vc,
    size_t count, size_t out_stride)
{
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_map_lane co0[ALWAN_MAP_TILE_PIXELS], co1[ALWAN_MAP_TILE_PIXELS], co2[ALWAN_MAP_TILE_PIXELS];
            for (size_t j = 0; j < tile; j++) {
                alwan_xyz xyz;
                int status = alwan_cam16_inverse(&xyz, &correlates_in[off + j], vc);
                if (status != ALWAN_OK) return status;
                co0[j] = (alwan_map_lane)xyz.x; co1[j] = (alwan_map_lane)xyz.y; co2[j] = (alwan_map_lane)xyz.z;
            }
            ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, off, out_stride, co0, co1, co2, tile);
            off += tile;
        }
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)xyz_out + i * out_stride);
        alwan_xyz xyz;
        int status = alwan_cam16_inverse(&xyz, &correlates_in[i], vc);
        if (status != ALWAN_OK) return status;
        out_ptr[0] = (alwan_map_lane)xyz.x; out_ptr[1] = (alwan_map_lane)xyz.y; out_ptr[2] = (alwan_map_lane)xyz.z;
    }
#endif
    return ALWAN_OK;
}
