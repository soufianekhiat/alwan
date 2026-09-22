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

/* The CIECAM02 and CAM16 maps delegate to the batch workers in
 * api/alwan_cam_impl.inc, which compute the viewing-condition terms once. */

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

alwan_status ALWAN_MAP_LITCAT(alwan_ciecam02_forward, _map_interleave)(ALWAN_CAM_MAP_CIECAM02_CORR *correlates_out, alwan_map_lane const *xyz_in, size_t in_stride, ALWAN_CAM_MAP_CIECAM02_VC const *vc, size_t count)
{
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_NAME(alwan__ciecam02_forward_n)(correlates_out, xyz_in, in_stride, vc, count);
}

alwan_status ALWAN_MAP_LITCAT(alwan_ciecam02_inverse, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, ALWAN_CAM_MAP_CIECAM02_CORR const *correlates_in, ALWAN_CAM_MAP_CIECAM02_VC const *vc, size_t count)
{
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_NAME(alwan__ciecam02_inverse_n)(xyz_out, out_stride, correlates_in, vc, count);
}

/* ----------------------------------------------------------------
 * Map CAM16 Conversions
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_cam16_forward, _map_interleave)(ALWAN_CAM_MAP_CAM16_CORR *correlates_out, alwan_map_lane const *xyz_in, size_t in_stride, ALWAN_CAM_MAP_CAM16_VC const *vc, size_t count)
{
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_NAME(alwan__cam16_forward_n)(correlates_out, xyz_in, in_stride, vc, count);
}

alwan_status ALWAN_MAP_LITCAT(alwan_cam16_inverse, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, ALWAN_CAM_MAP_CAM16_CORR const *correlates_in, ALWAN_CAM_MAP_CAM16_VC const *vc, size_t count)
{
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_NAME(alwan__cam16_inverse_n)(xyz_out, out_stride, correlates_in, vc, count);
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
