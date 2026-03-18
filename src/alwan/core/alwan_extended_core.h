/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Extended Color Spaces & Models
 * hdr-CIELAB, hdr-IPT, IgPgTg, ICaCb, Prismatic, HCL, IHLS
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_EXTENDED_CORE_H
#define ALWAN_EXTENDED_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_math_core.h"
#include "alwan_core.h"
#include "alwan_gamut_core.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_extended_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_extended_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

/* NOTE: GPU backend not yet implemented for extended core.
 * When needed, the original functions should be placed here
 * using alwan_scalar / ALWAN_LITERAL / ALWAN_SELECT. */

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_EXTENDED_CORE_H */
