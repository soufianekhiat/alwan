/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Integer-to-Float Normalization (ColorInterop Section 1.5)
 * Value-returning inline functions for the C backend.
 *
 * Supported bit depths: 8, 10, 12, 16.
 * Uses (2^N - 1) normalization.
 */

#ifndef ALWAN_NORMALIZE_CORE_H
#define ALWAN_NORMALIZE_CORE_H

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
#include "alwan_normalize_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_normalize_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU backends: single-precision pass via the same .inc.
 * ALWAN_CORE_* macros come from alwan_{hlsl,glsl,halide}.h.
 * ================================================================ */

#include "alwan_normalize_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_NORMALIZE_CORE_H */
