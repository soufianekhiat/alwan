/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * OpenCL Backend Bootstrap
 * Sets the OpenCL backend define and includes the platform layer.
 *
 * After this header:
 *   - alwan_scalar  = float, or double with -DALWAN_OPENCL_FP64=1
 *   - ALWAN_LITERAL, ALWAN_SELECT, ALWAN_SQRT, ... are OpenCL C builtins
 *   - alwan_vec2, alwan_vec3, alwan_mat3x3 are struct { alwan_scalar v[N]; }
 *   - All semantic colour types (alwan_rgb, alwan_xyz, ...) are structs
 *     (intentional: alwan never aliases to OpenCL float3, whose size is 16
 *     bytes rather than 12 and whose components are swizzles)
 *
 * Then include whichever *_core.h modules you need:
 *   #include "alwan_opencl.h"
 *   #include "core/alwan_oklab_core.h"
 *
 * Build the program with -I pointing at src/alwan and src/alwan/core.
 *
 * ON DOUBLE PRECISION. OpenCL makes fp64 an extension, so it is off here by
 * default and cannot be decided by a header: query the device for cl_khr_fp64
 * first, then pass -DALWAN_OPENCL_FP64=1. A program that enables the extension
 * on a device without it fails to build rather than silently demoting.
 *
 * ON WHY THIS IS A BACKEND ID AND CUDA IS NOT. OpenCL C is not C. Program-scope
 * constants must live in the __constant address space, there is no C library,
 * and double is optional. So it takes the same single-pass GPU branch of the
 * core headers that HLSL and GLSL take. nvcc, by contrast, is a C++ compiler
 * with a real double and can compile the C branch unchanged, which is why CUDA
 * is a qualifier switch (ALWAN_CUDA) rather than an ALWAN_BACKEND value.
 */

#ifndef ALWAN_OPENCL_H
#define ALWAN_OPENCL_H

/* Force the OpenCL backend. Redundant when __OPENCL_VERSION__ is defined, which
 * every OpenCL C compiler does, but explicit for offline compilers and tooling
 * that preprocesses kernel sources on the host. */
#ifndef ALWAN_BACKEND
#define ALWAN_BACKEND 4
#endif

/* Platform: alwan_scalar, math macros, ALWAN_SELECT, alwan_min/max/clamp. */
#include "alwan_platform.h"

/* Types: alwan_vec2/vec3/mat3x3 and the semantic colour types. */
#include "alwan_types.h"

/* ALWAN_CORE_* aliases for the GPU single-pass, shared with glsl/hlsl/halide. */
#include "alwan_core_aliases.inc"

#endif /* ALWAN_OPENCL_H */
