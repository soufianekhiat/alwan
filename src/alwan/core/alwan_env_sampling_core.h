/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only environment-map sampling on the equirectangular map: the
 * direction of a map position and back, a conservative bound of a cosine or
 * Phong lobe over a map cell, and the hierarchical product sampler (sample
 * warping down a pyramid of sums, after Clarberg, Jarosz, Akenine-Moller and
 * Jensen 2005 and Clarberg and Akenine-Moller 2008). The sampler reads its
 * pyramid through the ALWAN_ENVP_READ accessor of alwan_env_sampling_reader.inc,
 * bound to a pointer where pointers exist.
 * Cross-platform (C / CUDA / OpenCL / HLSL / GLSL / Halide).
 */

#ifndef ALWAN_ENV_SAMPLING_CORE_H
#define ALWAN_ENV_SAMPLING_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_env_sampling_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_env_sampling_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / OpenCL / Halide: the single-precision pass */

#include "alwan_env_sampling_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_ENV_SAMPLING_CORE_H */
