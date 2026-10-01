/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only image-based lighting: the real spherical harmonics of alwan's
 * equirectangular convention, the clamped-cosine coefficients of Ramamoorthi and
 * Hanrahan 2001 and the ringing windows, the GGX distribution, the height-correlated
 * Smith masking-shadowing term and GGX half-vector sampling. The coefficient and
 * split-sum table readers live in alwan_ibl_reader.inc, read through the
 * ALWAN_IBL_READ accessor so a shader evaluates the same arithmetic as alwan_sh_* and
 * alwan_ibl_*.
 * Cross-platform (C / CUDA / OpenCL / HLSL / GLSL / Halide).
 */

#ifndef ALWAN_IBL_CORE_H
#define ALWAN_IBL_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_ibl_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_ibl_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / OpenCL / Halide: the single-precision pass */

#include "alwan_ibl_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_IBL_CORE_H */
