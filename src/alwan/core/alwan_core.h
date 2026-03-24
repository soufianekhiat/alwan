/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only branchless transfer functions
 * Uses ALWAN_SELECT for cross-platform (C/HLSL/Halide) compatibility.
 */

#ifndef ALWAN_CORE_H
#define ALWAN_CORE_H

#include "../alwan_platform.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

#include "../alwan_types.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: single-precision pass via the same .inc
 * ALWAN_CORE_* macros are defined by alwan_hlsl/glsl/halide.h
 * ================================================================ */

#include "alwan_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_CORE_H */
