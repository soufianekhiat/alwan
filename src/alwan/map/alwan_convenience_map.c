/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Convenience Color Model Conversions - True SIMD vectorized
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_core.h"
#include "../core/alwan_convenience_core.h"

#if ALWAN_WITH_F32
/* === f32 pass === */
#define ALWAN_MAP_F32
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_convenience_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

#if ALWAN_WITH_F64
/* === f64 pass === */
#define ALWAN_MAP_F64
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_convenience_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

/* ================================================================
 * Backward-compatible kernel aliases (unsuffixed -> compile-time selected)
 * ================================================================ */

#define alwan__rgb_to_hsp_kernel      alwan__rgb_to_hsp_kernel_f64
#define alwan__hsp_to_rgb_kernel      alwan__hsp_to_rgb_kernel_f64
#define alwan__rgb_to_hsplog_kernel      alwan__rgb_to_hsplog_kernel_f64
#define alwan__hsplog_to_rgb_kernel      alwan__hsplog_to_rgb_kernel_f64
#define alwan__rgb_to_hsy_kernel      alwan__rgb_to_hsy_kernel_f64
#define alwan__hsy_to_rgb_kernel      alwan__hsy_to_rgb_kernel_f64
#define alwan__relative_luminance_kernel      alwan__relative_luminance_kernel_f64

