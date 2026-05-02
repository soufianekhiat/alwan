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
 *   - alwan_vec2, alwan_vec3, alwan_mat3x3 are struct { alwan_scalar v[N]; }
 *   - All semantic color types (alwan_rgb, alwan_xyz, ...) are structs
 *     (intentional — alwan never aliases to HLSL float3/swizzle types)
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

/* ALWAN_CORE_* — GPU single-precision pass aliases used by *_core.inc files.
 * Shared across glsl/hlsl/halide bootstraps. */
#include "alwan_core_aliases.inc"

#endif /* ALWAN_HLSL_H */
