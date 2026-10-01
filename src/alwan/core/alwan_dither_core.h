/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ordered dithering per pixel: the Bayer matrix by its recursive definition,
 * a mask rank taken as a threshold, the triangular (TPDF) remap of a uniform value and the
 * quantisation step. alwan_dither_quantize is these in a loop; a shader that reads a blue
 * noise mask from a texture dithers with the same arithmetic.
 * Cross-platform (C / CUDA / OpenCL / HLSL / GLSL / Halide).
 */

#ifndef ALWAN_DITHER_CORE_H
#define ALWAN_DITHER_CORE_H

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
#include "alwan_dither_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_dither_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / OpenCL / Halide: the single-precision pass */

#include "alwan_dither_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_DITHER_CORE_H */
