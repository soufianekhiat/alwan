/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only neural layer kernels: activations as value functions, and the
 * tensor kernels of alwan_nn_reader.inc bound to pointers where pointers
 * exist. Cross-platform (C / CUDA / OpenCL / HLSL / GLSL / Halide).
 *
 * Roadmap 3.10, step two. What one output element of a layer is lives in the
 * core; what a tensor is lives with the backend, behind the ALWAN_NN_READ_*
 * accessors, which is the same seam alwan_table_reader.inc proved on the
 * table cores. Every accumulate runs in a fixed order into an
 * ALWAN_DET_PRECISE local, so a deterministic build is bit-exact across
 * backends, which is the reason to run a network here rather than through a
 * library that picks its algorithm at run time.
 */

#ifndef ALWAN_NN_CORE_H
#define ALWAN_NN_CORE_H

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
#include "alwan_nn_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_nn_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / OpenCL / Halide: the single-precision pass */

#include "alwan_nn_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_NN_CORE_H */
