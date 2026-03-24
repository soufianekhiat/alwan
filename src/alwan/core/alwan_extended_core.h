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

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_extended_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_extended_core.inc"
#include "alwan_core_teardown.h"

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: single-precision pass via the same .inc
 * ALWAN_CORE_* macros are defined by alwan_hlsl/glsl/halide.h
 * ================================================================ */

#include "alwan_extended_core.inc"

#endif /* ALWAN_BACKEND */

ALWAN_DIAG_POP

#endif /* ALWAN_EXTENDED_CORE_H */
