/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only summed-area table queries (Crow 1984): the sum of an image over
 * any axis-aligned rectangle from four reads, whole pixels or continuous
 * bounds. The queries read the table through the ALWAN_SAT_READ accessor of
 * alwan_summed_area_reader.inc, bound to a pointer where pointers exist, so a
 * shader queries the same table with the same arithmetic.
 * Cross-platform (C / CUDA / OpenCL / HLSL / GLSL / Halide).
 */

#ifndef ALWAN_SUMMED_AREA_CORE_H
#define ALWAN_SUMMED_AREA_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_summed_area_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_summed_area_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / OpenCL / Halide: the single-precision pass */

#include "alwan_summed_area_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_SUMMED_AREA_CORE_H */
