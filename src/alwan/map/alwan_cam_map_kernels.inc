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

/* Precision-dependent type aliases + native scalar bindings for this pass.
 * The map lane precision (f32/f64) selects the precision-matched native
 * scalar worker so the f32 pass computes in f32 (no widen-to-f64 facade)
 * and the f64 pass keeps the exact f64 worker. */
#ifdef ALWAN_MAP_F32
#define ALWAN_CAM_MAP_CIECAM02_CORR  alwan_ciecam02_correlates_f32
#define ALWAN_CAM_MAP_CIECAM02_VC    alwan_ciecam02_viewing_conditions_f32
#define ALWAN_CAM_MAP_CAM16_CORR     alwan_cam16_correlates_f32
#define ALWAN_CAM_MAP_CAM16_VC       alwan_cam16_viewing_conditions_f32
#define ALWAN_CAM_MAP_XYZ            alwan_xyz_f32
#define ALWAN_CAM_MAP_CIECAM02_FWD   alwan_ciecam02_forward_f32
#define ALWAN_CAM_MAP_CIECAM02_INV   alwan_ciecam02_inverse_f32
#define ALWAN_CAM_MAP_CAM16_FWD      alwan_cam16_forward_f32
#define ALWAN_CAM_MAP_CAM16_INV      alwan_cam16_inverse_f32
#else
#define ALWAN_CAM_MAP_CIECAM02_CORR  alwan_ciecam02_correlates_f64
#define ALWAN_CAM_MAP_CIECAM02_VC    alwan_ciecam02_viewing_conditions_f64
#define ALWAN_CAM_MAP_CAM16_CORR     alwan_cam16_correlates_f64
#define ALWAN_CAM_MAP_CAM16_VC       alwan_cam16_viewing_conditions_f64
#define ALWAN_CAM_MAP_XYZ            alwan_xyz_f64
#define ALWAN_CAM_MAP_CIECAM02_FWD   alwan_ciecam02_forward_f64
#define ALWAN_CAM_MAP_CIECAM02_INV   alwan_ciecam02_inverse_f64
#define ALWAN_CAM_MAP_CAM16_FWD      alwan_cam16_forward_f64
#define ALWAN_CAM_MAP_CAM16_INV      alwan_cam16_inverse_f64
#endif

/* ----------------------------------------------------------------
 * Map CIECAM02 Conversions
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_ciecam02_forward, _map_interleave)(ALWAN_CAM_MAP_CIECAM02_CORR *correlates_out, alwan_map_lane const *xyz_in, size_t in_stride, ALWAN_CAM_MAP_CIECAM02_VC const *vc, size_t count)
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
                ALWAN_CAM_MAP_XYZ xyz = {ci0[j], ci1[j], ci2[j]};
                ALWAN_CAM_MAP_CIECAM02_CORR corr;
                int status = ALWAN_CAM_MAP_CIECAM02_FWD(&corr, &xyz, vc);
                if (status != ALWAN_OK) return status;
                correlates_out[off + j].J = (alwan_map_lane)corr.J;
                correlates_out[off + j].C = (alwan_map_lane)corr.C;
                correlates_out[off + j].h = (alwan_map_lane)corr.h;
                correlates_out[off + j].s = (alwan_map_lane)corr.s;
                correlates_out[off + j].Q = (alwan_map_lane)corr.Q;
                correlates_out[off + j].M = (alwan_map_lane)corr.M;
                correlates_out[off + j].H = (alwan_map_lane)corr.H;
            }
            off += tile;
        }
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)xyz_in + i * in_stride);
        ALWAN_CAM_MAP_XYZ xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_CAM_MAP_CIECAM02_CORR corr;
        int status = ALWAN_CAM_MAP_CIECAM02_FWD(&corr, &xyz, vc);
        if (status != ALWAN_OK) return status;
        correlates_out[i].J = (alwan_map_lane)corr.J;
        correlates_out[i].C = (alwan_map_lane)corr.C;
        correlates_out[i].h = (alwan_map_lane)corr.h;
        correlates_out[i].s = (alwan_map_lane)corr.s;
        correlates_out[i].Q = (alwan_map_lane)corr.Q;
        correlates_out[i].M = (alwan_map_lane)corr.M;
        correlates_out[i].H = (alwan_map_lane)corr.H;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_ciecam02_inverse, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, ALWAN_CAM_MAP_CIECAM02_CORR const *correlates_in, ALWAN_CAM_MAP_CIECAM02_VC const *vc, size_t count)
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
                ALWAN_CAM_MAP_CIECAM02_CORR corr;
                corr.J = correlates_in[off + j].J;
                corr.C = correlates_in[off + j].C;
                corr.h = correlates_in[off + j].h;
                corr.s = correlates_in[off + j].s;
                corr.Q = correlates_in[off + j].Q;
                corr.M = correlates_in[off + j].M;
                corr.H = correlates_in[off + j].H;
                ALWAN_CAM_MAP_XYZ xyz;
                int status = ALWAN_CAM_MAP_CIECAM02_INV(&xyz, &corr, vc);
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
        ALWAN_CAM_MAP_CIECAM02_CORR corr;
        corr.J = correlates_in[i].J;
        corr.C = correlates_in[i].C;
        corr.h = correlates_in[i].h;
        corr.s = correlates_in[i].s;
        corr.Q = correlates_in[i].Q;
        corr.M = correlates_in[i].M;
        corr.H = correlates_in[i].H;
        ALWAN_CAM_MAP_XYZ xyz;
        int status = ALWAN_CAM_MAP_CIECAM02_INV(&xyz, &corr, vc);
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

#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t off = 0;
        while (off < count) {
            size_t tile = count - off;
            if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_map_lane ci0[ALWAN_MAP_TILE_PIXELS], ci1[ALWAN_MAP_TILE_PIXELS], ci2[ALWAN_MAP_TILE_PIXELS];
            ALWAN_MAP_NAME(alwan__load_tile_aos3)(ci0, ci1, ci2, xyz_in, off, in_stride, tile);
            for (size_t j = 0; j < tile; j++) {
                ALWAN_CAM_MAP_XYZ xyz = {ci0[j], ci1[j], ci2[j]};
                ALWAN_CAM_MAP_CAM16_CORR corr;
                int status = ALWAN_CAM_MAP_CAM16_FWD(&corr, &xyz, vc);
                if (status != ALWAN_OK) return status;
                correlates_out[off + j].J = (alwan_map_lane)corr.J;
                correlates_out[off + j].C = (alwan_map_lane)corr.C;
                correlates_out[off + j].h = (alwan_map_lane)corr.h;
                correlates_out[off + j].s = (alwan_map_lane)corr.s;
                correlates_out[off + j].Q = (alwan_map_lane)corr.Q;
                correlates_out[off + j].M = (alwan_map_lane)corr.M;
                correlates_out[off + j].H = (alwan_map_lane)corr.H;
            }
            off += tile;
        }
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)xyz_in + i * in_stride);
        ALWAN_CAM_MAP_XYZ xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_CAM_MAP_CAM16_CORR corr;
        int status = ALWAN_CAM_MAP_CAM16_FWD(&corr, &xyz, vc);
        if (status != ALWAN_OK) return status;
        correlates_out[i].J = (alwan_map_lane)corr.J;
        correlates_out[i].C = (alwan_map_lane)corr.C;
        correlates_out[i].h = (alwan_map_lane)corr.h;
        correlates_out[i].s = (alwan_map_lane)corr.s;
        correlates_out[i].Q = (alwan_map_lane)corr.Q;
        correlates_out[i].M = (alwan_map_lane)corr.M;
        correlates_out[i].H = (alwan_map_lane)corr.H;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_cam16_inverse, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, ALWAN_CAM_MAP_CAM16_CORR const *correlates_in, ALWAN_CAM_MAP_CAM16_VC const *vc, size_t count)
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
                ALWAN_CAM_MAP_CAM16_CORR corr;
                corr.J = correlates_in[off + j].J;
                corr.C = correlates_in[off + j].C;
                corr.h = correlates_in[off + j].h;
                corr.s = correlates_in[off + j].s;
                corr.Q = correlates_in[off + j].Q;
                corr.M = correlates_in[off + j].M;
                corr.H = correlates_in[off + j].H;
                ALWAN_CAM_MAP_XYZ xyz;
                int status = ALWAN_CAM_MAP_CAM16_INV(&xyz, &corr, vc);
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
        ALWAN_CAM_MAP_CAM16_CORR corr;
        corr.J = correlates_in[i].J;
        corr.C = correlates_in[i].C;
        corr.h = correlates_in[i].h;
        corr.s = correlates_in[i].s;
        corr.Q = correlates_in[i].Q;
        corr.M = correlates_in[i].M;
        corr.H = correlates_in[i].H;
        ALWAN_CAM_MAP_XYZ xyz;
        int status = ALWAN_CAM_MAP_CAM16_INV(&xyz, &corr, vc);
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
#undef ALWAN_CAM_MAP_XYZ
#undef ALWAN_CAM_MAP_CIECAM02_FWD
#undef ALWAN_CAM_MAP_CIECAM02_INV
#undef ALWAN_CAM_MAP_CAM16_FWD
#undef ALWAN_CAM_MAP_CAM16_INV
