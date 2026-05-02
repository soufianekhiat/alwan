/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Halide Backend Bootstrap
 * Sets the Halide backend define and includes the platform layer.
 *
 * After this header:
 *   - alwan_scalar  = Halide::Expr (typedef'd in alwan_platform.h)
 *   - ALWAN_LITERAL wraps via Halide::Internal::make_const
 *   - ALWAN_SELECT  maps to Halide::select
 *   - ALWAN_SQRT, ALWAN_POW, ... map to Halide:: functions
 *   - alwan_vec2, alwan_vec3, alwan_mat3x3 are struct { alwan_scalar v[N]; }
 *   - All semantic color types (alwan_rgb, alwan_xyz, ...) are structs
 *
 * Halide supports both Float(32) and Float(64); instantiate with the appropriate Halide type.
 *
 * Then include whichever *_core.h modules you need:
 *   #include "alwan_halide.h"
 *   #include "alwan_oklab_core.h"
 *   #include "alwan_colorspace_core.h"
 */

#ifndef ALWAN_HALIDE_H
#define ALWAN_HALIDE_H

/* Force Halide backend */
#ifndef ALWAN_BACKEND
#define ALWAN_BACKEND 3
#endif

/* Platform: includes <Halide.h>, typedefs alwan_scalar = Halide::Expr,
 * defines ALWAN_LITERAL, math macros, ALWAN_SELECT,
 * utility functions (alwan_min, alwan_max, alwan_clamp, ...) */
#include "alwan_platform.h"

/* Types: defines alwan_vec2/vec3/mat3x3 and all semantic color types */
#include "alwan_types.h"

/* ALWAN_CORE_* — GPU single-precision pass aliases used by *_core.inc files.
 * Shared across glsl/hlsl/halide bootstraps. */
#include "alwan_core_aliases.inc"

#endif /* ALWAN_HALIDE_H */
