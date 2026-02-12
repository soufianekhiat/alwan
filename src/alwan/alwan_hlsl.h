/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * HLSL Backend Bootstrap
 * Sets the HLSL backend define and includes the platform layer.
 *
 * After this header:
 *   - alwan_scalar  = float
 *   - ALWAN_LITERAL, ALWAN_SELECT, ALWAN_SQRT, ... are HLSL intrinsics
 *   - alwan_vec2 = float2, alwan_vec3 = float3, alwan_mat3x3 = float3x3
 *   - All semantic color types (alwan_rgb, alwan_xyz, ...) = float3
 *
 * Then include whichever *_core.h modules you need:
 *   #include "alwan_hlsl.h"
 *   #include "alwan_oklab_core.h"
 *   #include "alwan_colorspace_core.h"
 */

#ifndef ALWAN_HLSL_H
#define ALWAN_HLSL_H

/* Force HLSL backend (redundant when __HLSL_VERSION is defined,
 * but makes intent explicit and works in cross-compilation tools) */
#ifndef ALWAN_BACKEND
#define ALWAN_BACKEND 1
#endif

/* Platform: defines alwan_scalar (float), math macros, ALWAN_SELECT,
 * utility functions (alwan_min, alwan_max, alwan_clamp, ...) */
#include "alwan_platform.h"

/* Types: defines alwan_vec2/vec3/mat3x3 and all semantic color types */
#include "alwan_types.h"

#endif /* ALWAN_HLSL_H */
