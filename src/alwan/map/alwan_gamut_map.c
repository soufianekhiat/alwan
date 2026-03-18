/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Gamut - SIMD vectorized gamut mapping kernels
 * Clip (clamp) and CSS Color Level 4 gamut mapping.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_gamut_core.h"

/* === f32 pass === */
#define ALWAN_MAP_F32
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_gamut_map_kernels.inc"
#include "alwan_map_simd_undef.h"

/* === f64 pass === */
#define ALWAN_MAP_F64
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_gamut_map_kernels.inc"
#include "alwan_map_simd_undef.h"

/* ================================================================
 * Backward-compatible aliases (unsuffixed -> compile-time selected)
 * ================================================================ */

#define alwan__gamut_clip_kernel      alwan__gamut_clip_kernel_f64
#define alwan__css_gamut_map_kernel   alwan__css_gamut_map_kernel_f64
