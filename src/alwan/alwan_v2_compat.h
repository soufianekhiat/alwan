/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2026 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Source compatibility with alwan 2.0.0 for the names 3.0.0 renamed without changing what they
 * do. Include it after alwan.h while porting; it adds no symbol to the library and costs
 * nothing once the calls are renamed. docs/migration-v2-v3.md covers every change, including
 * the ones no shim can hide:
 *
 *   alwan_set_aces_interp / alwan_get_aces_interp   the process global is gone: pass the
 *       method to alwan_aces1_output_transform_{T}, or set it on a context
 *   alwan_aces1_output_transform_{T}                takes the method as a 4th parameter
 *   alwan_whiteness_* / alwan_yellowness_*          return a status, the index in an out
 *       parameter (they returned the index, and -1 on error)
 *   alwan_delta_e_cmc_batch_ex                      takes alwan_delta_e_cmc_params_f64
 *   alwan_xyz_from_spd_camera_{T}, alwan_spd_camera_sensitivity_{T}   the camera enum is gone:
 *       alwan_camera_rgb_from_spd_{T} and alwan_camera_sensitivities_{T} take a registry index
 *   alwan_apca_contrast_{T}                         removed (licence)
 */

#ifndef ALWAN_V2_COMPAT_H
#define ALWAN_V2_COMPAT_H

#include "alwan.h"

/* The precision suffix dropped from pairs whose two halves were the same function. */
#define alwan_float_to_half_f32       alwan_float_to_half
#define alwan_float_to_half_f64       alwan_float_to_half
#define alwan_half_to_float_f32       alwan_half_to_float
#define alwan_half_to_float_f64       alwan_half_to_float
#define alwan_interop_entry_at_f32    alwan_interop_entry_at
#define alwan_interop_entry_at_f64    alwan_interop_entry_at
#define alwan_interop_parse_f32       alwan_interop_parse
#define alwan_interop_parse_f64       alwan_interop_parse
#define alwan_lut2d_dimensions_f32    alwan_lut2d_dimensions
#define alwan_lut2d_dimensions_f64    alwan_lut2d_dimensions

/* xyz_adapt is a map, and is named like one. */
#define alwan_xyz_adapt_f32           alwan_xyz_adapt_f32_map_interleave
#define alwan_xyz_adapt_f64           alwan_xyz_adapt_f64_map_interleave

/* The lut*d_sample delegates: the table samplers with the size before the coordinate and the
 * interpolation the delegates used. */
#if ALWAN_WITH_F32
static inline alwan_status alwan_lut1d_sample_f32(alwan_f32 *result, alwan_f32 const *lut, alwan_f32 t, int size) {
    return alwan_table1d_sample_f32(result, lut, size, t, ALWAN_SAMPLE_LINEAR);
}
static inline alwan_status alwan_lut2d_sample_f32(alwan_rgb_f32 *result, alwan_f32 const *lut2d, alwan_rgb_f32 const *rgb, int size) {
    return alwan_table2d_sample_f32(result, lut2d, size, rgb, ALWAN_SAMPLE_TRILINEAR);
}
static inline alwan_status alwan_lut3d_sample_f32(alwan_rgb_f32 *result, alwan_f32 const *lut, alwan_rgb_f32 const *rgb, int size) {
    return alwan_table3d_sample_f32(result, lut, size, rgb, ALWAN_SAMPLE_TRILINEAR);
}
#endif
#if ALWAN_WITH_F64
static inline alwan_status alwan_lut1d_sample_f64(alwan_f64 *result, alwan_f64 const *lut, alwan_f64 t, int size) {
    return alwan_table1d_sample_f64(result, lut, size, t, ALWAN_SAMPLE_LINEAR);
}
static inline alwan_status alwan_lut2d_sample_f64(alwan_rgb_f64 *result, alwan_f64 const *lut2d, alwan_rgb_f64 const *rgb, int size) {
    return alwan_table2d_sample_f64(result, lut2d, size, rgb, ALWAN_SAMPLE_TRILINEAR);
}
static inline alwan_status alwan_lut3d_sample_f64(alwan_rgb_f64 *result, alwan_f64 const *lut, alwan_rgb_f64 const *rgb, int size) {
    return alwan_table3d_sample_f64(result, lut, size, rgb, ALWAN_SAMPLE_TRILINEAR);
}
#endif

#endif /* ALWAN_V2_COMPAT_H */
