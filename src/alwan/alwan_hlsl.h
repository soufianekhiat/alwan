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
 *     (intentional -- alwan never aliases to HLSL float3/swizzle types)
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

/* HLSL reserves identifiers that ordinary C uses freely -- `linear` is an
 * interpolation modifier, and a transfer function taking `alwan_scalar
 * linear` is a hard syntax error ("unexpected token 'linear'", then
 * "function must return a value" for the whole body). Remap it through the
 * preprocessor: comments are stripped before substitution, alwan never uses
 * the HLSL keyword itself, and the C build never sees this header. `out`
 * (a parameter modifier) was handled by renaming the few locals instead --
 * see the core headers -- because a remap here would also rewrite the
 * `out` parameters of caller-side HLSL that includes us. `linear` has no
 * such use in caller shaders' interface blocks that reach these headers. */
#define linear alwan_hlsl_linear_id

/* Platform: defines alwan_scalar (float), math macros, ALWAN_SELECT,
 * utility functions (alwan_min, alwan_max, alwan_clamp, ...) */
#include "alwan_platform.h"

/* Types: defines alwan_vec2/vec3/mat3x3 and all semantic color types */
#include "alwan_types.h"

/* ALWAN_CORE_* -- GPU single-precision pass aliases used by *_core.inc files.
 * Shared across glsl/hlsl/halide bootstraps. */
#include "alwan_core_aliases.inc"

#endif /* ALWAN_HLSL_H */
