/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Convenience Extra Color Model Conversions
 * CMY, YCoCg, HWB, YCbCr, YcCbcCrc, CMYK
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_convenience_core.h"

/* YCbCr coefficients resolved via alwan__get_ycbcr_coeffs() in alwan_internal.h */

#if ALWAN_WITH_F32
/* === f32 pass === */
#define ALWAN_MAP_F32
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_convenience_extra_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

#if ALWAN_WITH_F64
/* === f64 pass === */
#define ALWAN_MAP_F64
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_convenience_extra_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

/* ================================================================
 * Backward-compatible kernel aliases (unsuffixed -> compile-time selected)
 * ================================================================ */

#define alwan__cmy_to_rgb_kernel                         alwan__cmy_to_rgb_kernel_f64
#define alwan__hsv_to_hwb_kernel                         alwan__hsv_to_hwb_kernel_f64
#define alwan__hwb_to_hsv_kernel                         alwan__hwb_to_hsv_kernel_f64
#define alwan__hwb_to_rgb_kernel                         alwan__hwb_to_rgb_kernel_f64
#define alwan__rgb_to_cmy_kernel                         alwan__rgb_to_cmy_kernel_f64
#define alwan__rgb_to_hwb_kernel                         alwan__rgb_to_hwb_kernel_f64
#define alwan__rgb_to_ycbcr_kernel                       alwan__rgb_to_ycbcr_kernel_f64
#define alwan__rgb_to_yccbccrc_kernel                    alwan__rgb_to_yccbccrc_kernel_f64
#define alwan__rgb_to_ycocg_kernel                       alwan__rgb_to_ycocg_kernel_f64
#define alwan__ycbcr_full_to_legal_kernel                alwan__ycbcr_full_to_legal_kernel_f64
#define alwan__ycbcr_legal_to_full_kernel                alwan__ycbcr_legal_to_full_kernel_f64
#define alwan__ycbcr_to_rgb_kernel                       alwan__ycbcr_to_rgb_kernel_f64
#define alwan__yccbccrc_to_rgb_kernel                    alwan__yccbccrc_to_rgb_kernel_f64
#define alwan__ycocg_to_rgb_kernel                       alwan__ycocg_to_rgb_kernel_f64

