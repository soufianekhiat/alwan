/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Oklab Conversions - True SIMD vectorized
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_oklab_core.h"

/* === f32 pass === */
#define ALWAN_MAP_F32
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_oklab_map_kernels.inc"
#include "alwan_map_simd_undef.h"

/* === f64 pass === */
#define ALWAN_MAP_F64
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_oklab_map_kernels.inc"
#include "alwan_map_simd_undef.h"

/* ================================================================
 * Backward-compatible aliases (unsuffixed -> compile-time selected)
 * ================================================================ */

#define alwan__xyz_to_oklab_kernel      alwan__xyz_to_oklab_kernel_f64
#define alwan__oklab_to_xyz_kernel      alwan__oklab_to_xyz_kernel_f64

/* ================================================================
 * _ex Interleave Variants (dual-dispatch: F64 -> f64 pipeline, else f32)
 * ================================================================ */

ALWAN_EX_DELEGATE_DUAL(alwan_xyz_to_oklab_map_interleave_ex,
                       alwan_xyz_to_oklab_f32_map_interleave,
                       alwan_xyz_to_oklab_f64_map_interleave)

ALWAN_EX_DELEGATE_DUAL(alwan_oklab_to_xyz_map_interleave_ex,
                       alwan_oklab_to_xyz_f32_map_interleave,
                       alwan_oklab_to_xyz_f64_map_interleave)
