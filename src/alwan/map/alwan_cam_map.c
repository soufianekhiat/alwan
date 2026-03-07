/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Color Appearance Model Conversions
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"

/* CAM functions use struct I/O - tiled path processes XYZ side only */

/* ----------------------------------------------------------------
 * Map CIECAM02 Conversions
 * ---------------------------------------------------------------- */

int alwan_ciecam02_forward_map_interleave(alwan_ciecam02_correlates *correlates_out,
                                alwan_scalar const *xyz_in,
                                alwan_ciecam02_viewing_conditions const *vc,
                                size_t count, size_t in_stride) {
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
            alwan_simd_lane ci0[ALWAN_TILE_PIXELS], ci1[ALWAN_TILE_PIXELS], ci2[ALWAN_TILE_PIXELS];
            alwan__load_tile_aos3(ci0, ci1, ci2, xyz_in, off, in_stride, tile);
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
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        int status = alwan_ciecam02_forward(&correlates_out[i], &xyz, vc);
        if (status != ALWAN_OK) return status;
    }
#endif
    return ALWAN_OK;
}

int alwan_ciecam02_inverse_map_interleave(alwan_scalar *xyz_out,
                                alwan_ciecam02_correlates const *correlates_in,
                                alwan_ciecam02_viewing_conditions const *vc,
                                size_t count, size_t out_stride) {
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
            alwan_simd_lane co0[ALWAN_TILE_PIXELS], co1[ALWAN_TILE_PIXELS], co2[ALWAN_TILE_PIXELS];
            for (size_t j = 0; j < tile; j++) {
                alwan_xyz xyz;
                int status = alwan_ciecam02_inverse(&xyz, &correlates_in[off + j], vc);
                if (status != ALWAN_OK) return status;
                co0[j] = (alwan_simd_lane)xyz.x; co1[j] = (alwan_simd_lane)xyz.y; co2[j] = (alwan_simd_lane)xyz.z;
            }
            alwan__store_tile_aos3(xyz_out, off, out_stride, co0, co1, co2, tile);
            off += tile;
        }
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_xyz xyz;
        int status = alwan_ciecam02_inverse(&xyz, &correlates_in[i], vc);
        if (status != ALWAN_OK) return status;
        out_ptr[0] = xyz.x; out_ptr[1] = xyz.y; out_ptr[2] = xyz.z;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Map CAM16 Conversions
 * ---------------------------------------------------------------- */

int alwan_cam16_forward_map_interleave(alwan_cam16_correlates *correlates_out,
                             alwan_scalar const *xyz_in,
                             alwan_cam16_viewing_conditions const *vc,
                             size_t count, size_t in_stride) {
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
            alwan_simd_lane ci0[ALWAN_TILE_PIXELS], ci1[ALWAN_TILE_PIXELS], ci2[ALWAN_TILE_PIXELS];
            alwan__load_tile_aos3(ci0, ci1, ci2, xyz_in, off, in_stride, tile);
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
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        int status = alwan_cam16_forward(&correlates_out[i], &xyz, vc);
        if (status != ALWAN_OK) return status;
    }
#endif
    return ALWAN_OK;
}

int alwan_cam16_inverse_map_interleave(alwan_scalar *xyz_out,
                             alwan_cam16_correlates const *correlates_in,
                             alwan_cam16_viewing_conditions const *vc,
                             size_t count, size_t out_stride) {
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
            alwan_simd_lane co0[ALWAN_TILE_PIXELS], co1[ALWAN_TILE_PIXELS], co2[ALWAN_TILE_PIXELS];
            for (size_t j = 0; j < tile; j++) {
                alwan_xyz xyz;
                int status = alwan_cam16_inverse(&xyz, &correlates_in[off + j], vc);
                if (status != ALWAN_OK) return status;
                co0[j] = (alwan_simd_lane)xyz.x; co1[j] = (alwan_simd_lane)xyz.y; co2[j] = (alwan_simd_lane)xyz.z;
            }
            alwan__store_tile_aos3(xyz_out, off, out_stride, co0, co1, co2, tile);
            off += tile;
        }
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_xyz xyz;
        int status = alwan_cam16_inverse(&xyz, &correlates_in[i], vc);
        if (status != ALWAN_OK) return status;
        out_ptr[0] = xyz.x; out_ptr[1] = xyz.y; out_ptr[2] = xyz.z;
    }
#endif
    return ALWAN_OK;
}
