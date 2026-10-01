/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only sampling of fluorescent reradiation: given the wavelength a path carries,
 * the event (reflection or reradiation) and the other wavelength, drawn from the rows of
 * a Donaldson matrix's tables. The tables are read through the ALWAN_FLUO_READ accessor
 * of alwan_fluorescence_reader.inc, bound to a pointer where pointers exist, so a shader
 * samples the same tables with the same arithmetic.
 * Cross-platform (C / CUDA / OpenCL / HLSL / GLSL / Halide).
 */

#ifndef ALWAN_FLUORESCENCE_CORE_H
#define ALWAN_FLUORESCENCE_CORE_H

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
#include "alwan_fluorescence_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_fluorescence_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / OpenCL / Halide: the single-precision pass */

#include "alwan_fluorescence_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_FLUORESCENCE_CORE_H */
