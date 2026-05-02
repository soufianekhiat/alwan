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

/* Precision-dependent type aliases for this pass */
#ifdef ALWAN_MAP_F32
#define ALWAN_CAM_MAP_CIECAM02_CORR  alwan_ciecam02_correlates_f32
#define ALWAN_CAM_MAP_CIECAM02_VC    alwan_ciecam02_viewing_conditions_f32
#define ALWAN_CAM_MAP_CAM16_CORR     alwan_cam16_correlates_f32
#define ALWAN_CAM_MAP_CAM16_VC       alwan_cam16_viewing_conditions_f32
#else
#define ALWAN_CAM_MAP_CIECAM02_CORR  alwan_ciecam02_correlates_f64
#define ALWAN_CAM_MAP_CIECAM02_VC    alwan_ciecam02_viewing_conditions_f64
#define ALWAN_CAM_MAP_CAM16_CORR     alwan_cam16_correlates_f64
#define ALWAN_CAM_MAP_CAM16_VC       alwan_cam16_viewing_conditions_f64
#endif

/* ----------------------------------------------------------------
 * Map CIECAM02 Conversions
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_ciecam02_forward, _map_interleave)(ALWAN_CAM_MAP_CIECAM02_CORR *correlates_out, alwan_map_lane const *xyz_in, size_t in_stride, ALWAN_CAM_MAP_CIECAM02_VC const *vc, size_t count)
{
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
    /* Build f64 viewing conditions for scalar call */
    alwan_ciecam02_viewing_conditions_f64 vc64;
    vc64.white_xyz.x = (alwan_f64)vc->white_xyz.x;
    vc64.white_xyz.y = (alwan_f64)vc->white_xyz.y;
    vc64.white_xyz.z = (alwan_f64)vc->white_xyz.z;
    vc64.adapting_luminance = (alwan_f64)vc->adapting_luminance;
    vc64.background_luminance = (alwan_f64)vc->background_luminance;
    vc64.surround = vc->surround;
    vc64.discount_illuminant = vc->discount_illuminant;

#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_map_lane ci0[ALWAN_MAP_TILE_PIXELS], ci1[ALWAN_MAP_TILE_PIXELS], ci2[ALWAN_MAP_TILE_PIXELS];
            ALWAN_MAP_NAME(alwan__load_tile_aos3)(ci0, ci1, ci2, xyz_in, off, in_stride, tile);
            for (size_t j = 0; j < tile; j++) {
                alwan_xyz_f64 xyz = {(alwan_f64)ci0[j], (alwan_f64)ci1[j], (alwan_f64)ci2[j]};
                alwan_ciecam02_correlates_f64 corr64;
                int status = alwan_ciecam02_forward_f64(&corr64, &xyz, &vc64);
                if (status != ALWAN_OK) return status;
                correlates_out[off + j].J = (alwan_map_lane)corr64.J;
                correlates_out[off + j].C = (alwan_map_lane)corr64.C;
                correlates_out[off + j].h = (alwan_map_lane)corr64.h;
                correlates_out[off + j].s = (alwan_map_lane)corr64.s;
                correlates_out[off + j].Q = (alwan_map_lane)corr64.Q;
                correlates_out[off + j].M = (alwan_map_lane)corr64.M;
                correlates_out[off + j].H = (alwan_map_lane)corr64.H;
            }
            off += tile;
        }
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)xyz_in + i * in_stride);
        alwan_xyz_f64 xyz = {(alwan_f64)in_ptr[0], (alwan_f64)in_ptr[1], (alwan_f64)in_ptr[2]};
        alwan_ciecam02_correlates_f64 corr64;
        int status = alwan_ciecam02_forward_f64(&corr64, &xyz, &vc64);
        if (status != ALWAN_OK) return status;
        correlates_out[i].J = (alwan_map_lane)corr64.J;
        correlates_out[i].C = (alwan_map_lane)corr64.C;
        correlates_out[i].h = (alwan_map_lane)corr64.h;
        correlates_out[i].s = (alwan_map_lane)corr64.s;
        correlates_out[i].Q = (alwan_map_lane)corr64.Q;
        correlates_out[i].M = (alwan_map_lane)corr64.M;
        correlates_out[i].H = (alwan_map_lane)corr64.H;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_ciecam02_inverse, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, ALWAN_CAM_MAP_CIECAM02_CORR const *correlates_in, ALWAN_CAM_MAP_CIECAM02_VC const *vc, size_t count)
{
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
    /* Build f64 viewing conditions for scalar call */
    alwan_ciecam02_viewing_conditions_f64 vc64;
    vc64.white_xyz.x = (alwan_f64)vc->white_xyz.x;
    vc64.white_xyz.y = (alwan_f64)vc->white_xyz.y;
    vc64.white_xyz.z = (alwan_f64)vc->white_xyz.z;
    vc64.adapting_luminance = (alwan_f64)vc->adapting_luminance;
    vc64.background_luminance = (alwan_f64)vc->background_luminance;
    vc64.surround = vc->surround;
    vc64.discount_illuminant = vc->discount_illuminant;

#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_map_lane co0[ALWAN_MAP_TILE_PIXELS], co1[ALWAN_MAP_TILE_PIXELS], co2[ALWAN_MAP_TILE_PIXELS];
            for (size_t j = 0; j < tile; j++) {
                alwan_ciecam02_correlates_f64 corr64;
                corr64.J = (alwan_f64)correlates_in[off + j].J;
                corr64.C = (alwan_f64)correlates_in[off + j].C;
                corr64.h = (alwan_f64)correlates_in[off + j].h;
                corr64.s = (alwan_f64)correlates_in[off + j].s;
                corr64.Q = (alwan_f64)correlates_in[off + j].Q;
                corr64.M = (alwan_f64)correlates_in[off + j].M;
                corr64.H = (alwan_f64)correlates_in[off + j].H;
                alwan_xyz_f64 xyz;
                int status = alwan_ciecam02_inverse_f64(&xyz, &corr64, &vc64);
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
        alwan_ciecam02_correlates_f64 corr64;
        corr64.J = (alwan_f64)correlates_in[i].J;
        corr64.C = (alwan_f64)correlates_in[i].C;
        corr64.h = (alwan_f64)correlates_in[i].h;
        corr64.s = (alwan_f64)correlates_in[i].s;
        corr64.Q = (alwan_f64)correlates_in[i].Q;
        corr64.M = (alwan_f64)correlates_in[i].M;
        corr64.H = (alwan_f64)correlates_in[i].H;
        alwan_xyz_f64 xyz;
        int status = alwan_ciecam02_inverse_f64(&xyz, &corr64, &vc64);
        if (status != ALWAN_OK) return status;
        out_ptr[0] = (alwan_map_lane)xyz.x; out_ptr[1] = (alwan_map_lane)xyz.y; out_ptr[2] = (alwan_map_lane)xyz.z;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Map CAM16 Conversions
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_cam16_forward, _map_interleave)(ALWAN_CAM_MAP_CAM16_CORR *correlates_out, alwan_map_lane const *xyz_in, size_t in_stride, ALWAN_CAM_MAP_CAM16_VC const *vc, size_t count)
{
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
    alwan_cam16_viewing_conditions_f64 vc64;
    vc64.white_xyz.x = (alwan_f64)vc->white_xyz.x;
    vc64.white_xyz.y = (alwan_f64)vc->white_xyz.y;
    vc64.white_xyz.z = (alwan_f64)vc->white_xyz.z;
    vc64.adapting_luminance = (alwan_f64)vc->adapting_luminance;
    vc64.background_luminance = (alwan_f64)vc->background_luminance;
    vc64.surround = vc->surround;
    vc64.discount_illuminant = vc->discount_illuminant;

#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_map_lane ci0[ALWAN_MAP_TILE_PIXELS], ci1[ALWAN_MAP_TILE_PIXELS], ci2[ALWAN_MAP_TILE_PIXELS];
            ALWAN_MAP_NAME(alwan__load_tile_aos3)(ci0, ci1, ci2, xyz_in, off, in_stride, tile);
            for (size_t j = 0; j < tile; j++) {
                alwan_xyz_f64 xyz = {(alwan_f64)ci0[j], (alwan_f64)ci1[j], (alwan_f64)ci2[j]};
                alwan_cam16_correlates_f64 corr64;
                int status = alwan_cam16_forward_f64(&corr64, &xyz, &vc64);
                if (status != ALWAN_OK) return status;
                correlates_out[off + j].J = (alwan_map_lane)corr64.J;
                correlates_out[off + j].C = (alwan_map_lane)corr64.C;
                correlates_out[off + j].h = (alwan_map_lane)corr64.h;
                correlates_out[off + j].s = (alwan_map_lane)corr64.s;
                correlates_out[off + j].Q = (alwan_map_lane)corr64.Q;
                correlates_out[off + j].M = (alwan_map_lane)corr64.M;
                correlates_out[off + j].H = (alwan_map_lane)corr64.H;
            }
            off += tile;
        }
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)xyz_in + i * in_stride);
        alwan_xyz_f64 xyz = {(alwan_f64)in_ptr[0], (alwan_f64)in_ptr[1], (alwan_f64)in_ptr[2]};
        alwan_cam16_correlates_f64 corr64;
        int status = alwan_cam16_forward_f64(&corr64, &xyz, &vc64);
        if (status != ALWAN_OK) return status;
        correlates_out[i].J = (alwan_map_lane)corr64.J;
        correlates_out[i].C = (alwan_map_lane)corr64.C;
        correlates_out[i].h = (alwan_map_lane)corr64.h;
        correlates_out[i].s = (alwan_map_lane)corr64.s;
        correlates_out[i].Q = (alwan_map_lane)corr64.Q;
        correlates_out[i].M = (alwan_map_lane)corr64.M;
        correlates_out[i].H = (alwan_map_lane)corr64.H;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_cam16_inverse, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, ALWAN_CAM_MAP_CAM16_CORR const *correlates_in, ALWAN_CAM_MAP_CAM16_VC const *vc, size_t count)
{
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
    alwan_cam16_viewing_conditions_f64 vc64;
    vc64.white_xyz.x = (alwan_f64)vc->white_xyz.x;
    vc64.white_xyz.y = (alwan_f64)vc->white_xyz.y;
    vc64.white_xyz.z = (alwan_f64)vc->white_xyz.z;
    vc64.adapting_luminance = (alwan_f64)vc->adapting_luminance;
    vc64.background_luminance = (alwan_f64)vc->background_luminance;
    vc64.surround = vc->surround;
    vc64.discount_illuminant = vc->discount_illuminant;

#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_map_lane co0[ALWAN_MAP_TILE_PIXELS], co1[ALWAN_MAP_TILE_PIXELS], co2[ALWAN_MAP_TILE_PIXELS];
            for (size_t j = 0; j < tile; j++) {
                alwan_cam16_correlates_f64 corr64;
                corr64.J = (alwan_f64)correlates_in[off + j].J;
                corr64.C = (alwan_f64)correlates_in[off + j].C;
                corr64.h = (alwan_f64)correlates_in[off + j].h;
                corr64.s = (alwan_f64)correlates_in[off + j].s;
                corr64.Q = (alwan_f64)correlates_in[off + j].Q;
                corr64.M = (alwan_f64)correlates_in[off + j].M;
                corr64.H = (alwan_f64)correlates_in[off + j].H;
                alwan_xyz_f64 xyz;
                int status = alwan_cam16_inverse_f64(&xyz, &corr64, &vc64);
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
        alwan_cam16_correlates_f64 corr64;
        corr64.J = (alwan_f64)correlates_in[i].J;
        corr64.C = (alwan_f64)correlates_in[i].C;
        corr64.h = (alwan_f64)correlates_in[i].h;
        corr64.s = (alwan_f64)correlates_in[i].s;
        corr64.Q = (alwan_f64)correlates_in[i].Q;
        corr64.M = (alwan_f64)correlates_in[i].M;
        corr64.H = (alwan_f64)correlates_in[i].H;
        alwan_xyz_f64 xyz;
        int status = alwan_cam16_inverse_f64(&xyz, &corr64, &vc64);
        if (status != ALWAN_OK) return status;
        out_ptr[0] = (alwan_map_lane)xyz.x; out_ptr[1] = (alwan_map_lane)xyz.y; out_ptr[2] = (alwan_map_lane)xyz.z;
    }
#endif
    return ALWAN_OK;
}

#undef ALWAN_CAM_MAP_CIECAM02_CORR
#undef ALWAN_CAM_MAP_CIECAM02_VC
#undef ALWAN_CAM_MAP_CAM16_CORR
#undef ALWAN_CAM_MAP_CAM16_VC
