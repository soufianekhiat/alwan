/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Color Space Conversions - True SIMD vectorized
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_colorspace_core.h"
#include "../core/alwan_math_core.h"

#if ALWAN_WITH_F32
/* === f32 pass === */
#define ALWAN_MAP_F32
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_colorspace_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

#if ALWAN_WITH_F64
/* === f64 pass === */
#define ALWAN_MAP_F64
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_colorspace_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

/* ================================================================
 * Backward-compatible kernel aliases (for _ex functions)
 * ================================================================ */

#define alwan__xyz_to_lab_kernel_wp   alwan__xyz_to_lab_kernel_wp_f64
#define alwan__lab_to_xyz_kernel_wp   alwan__lab_to_xyz_kernel_wp_f64
#define alwan__xyz_to_luv_kernel_wp   alwan__xyz_to_luv_kernel_wp_f64
#define alwan__luv_to_xyz_kernel_wp   alwan__luv_to_xyz_kernel_wp_f64

/* ================================================================
 * _ex Interleave Variants (dual-dispatch: F64 -> f64 pipeline, else f32)
 * ================================================================ */

ALWAN_EX_DELEGATE_DUAL_WHITE(alwan_xyz_to_lab_map_interleave_ex,
                              alwan_xyz_to_lab_f32_map_interleave,
                              alwan_xyz_to_lab_f64_map_interleave)

ALWAN_EX_DELEGATE_DUAL_WHITE(alwan_lab_to_xyz_map_interleave_ex,
                              alwan_lab_to_xyz_f32_map_interleave,
                              alwan_lab_to_xyz_f64_map_interleave)

ALWAN_EX_DELEGATE_DUAL_WHITE(alwan_xyz_to_luv_map_interleave_ex,
                              alwan_xyz_to_luv_f32_map_interleave,
                              alwan_xyz_to_luv_f64_map_interleave)

ALWAN_EX_DELEGATE_DUAL_WHITE(alwan_luv_to_xyz_map_interleave_ex,
                              alwan_luv_to_xyz_f32_map_interleave,
                              alwan_luv_to_xyz_f64_map_interleave)
