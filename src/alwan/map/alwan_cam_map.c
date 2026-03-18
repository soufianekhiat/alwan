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

/* === f32 pass === */
#define ALWAN_MAP_F32
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_cam_map_kernels.inc"
#include "alwan_map_simd_undef.h"

/* === f64 pass === */
#define ALWAN_MAP_F64
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_cam_map_kernels.inc"
#include "alwan_map_simd_undef.h"

