/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only physical sky models, per direction: the Perez distribution and the
 * Preetham 1999 zenith values, the Hosek-Wilkie 2012 radiance function, and lookups
 * into Bruneton 2017 precomputed atmospheric scattering tables. The tables are read
 * through the ALWAN_ATM_READ accessor of alwan_sky_atmosphere_reader.inc, bound to a
 * pointer where pointers exist, so a shader reads the same tables with the same
 * arithmetic. Cross-platform (C / CUDA / OpenCL / HLSL / GLSL / Halide).
 */

#ifndef ALWAN_SKY_CORE_H
#define ALWAN_SKY_CORE_H

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
#include "alwan_sky_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_sky_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / OpenCL / Halide: the single-precision pass */

#include "alwan_sky_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_SKY_CORE_H */
