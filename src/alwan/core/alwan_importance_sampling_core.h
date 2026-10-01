/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only 2D importance sampling of a piecewise-constant image: the
 * marginal and conditional distributions, sampled through tabulated inverse
 * CDFs (DIRECT) or by CDF bisection (SEARCH). The sampling reads
 * its tables through the ALWAN_IS2D_READ accessor of
 * alwan_importance_sampling_reader.inc, bound to a pointer where pointers
 * exist, so a shader samples the same tables with the same arithmetic.
 * Cross-platform (C / CUDA / OpenCL / HLSL / GLSL / Halide).
 */

#ifndef ALWAN_IMPORTANCE_SAMPLING_CORE_H
#define ALWAN_IMPORTANCE_SAMPLING_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_importance_sampling_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_importance_sampling_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / OpenCL / Halide: the single-precision pass */

#include "alwan_importance_sampling_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_IMPORTANCE_SAMPLING_CORE_H */
