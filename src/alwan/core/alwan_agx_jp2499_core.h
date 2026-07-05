/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only JP2499 picture-formation core (Juan Pablo Zambrano's "2499" DRT).
 * Shared by the scalar apply (api/alwan_agx_jp2499.c) and the tiled map kernels
 * (map/alwan_agx_jp2499_map_kernels.inc). Everything (per-call setup + per-pixel
 * render) is instantiated natively for f32 and f64 from the shared .inc.
 */

#ifndef ALWAN_AGX_JP2499_CORE_H
#define ALWAN_AGX_JP2499_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass -> jp2499_*_f32 */
#include "alwan_core_f32_setup.h"
#include "alwan_agx_jp2499_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass -> jp2499_*_f64 */
#include "alwan_core_f64_setup.h"
#include "alwan_agx_jp2499_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU backends: single-precision pass via the same .inc. The whole
 * core is pointer-free (struct-by-value), so setup (tonescale params,
 * hue geometry incl. the 3x3 inversions) AND the per-pixel render all
 * compile for GPU; hosts typically run the setup once and feed the
 * render its results as uniforms.
 * ================================================================ */

#include "alwan_agx_jp2499_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_AGX_JP2499_CORE_H */
