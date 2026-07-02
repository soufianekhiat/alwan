/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only analytic AgX picture-formation core -- a fully parameterized,
 * geometric AgX engine (everything geometric except the single 1D sigmoid).
 * Shared by the scalar apply (api/alwan_agx.c) and the tiled map kernels
 * (map/alwan_agx_map_kernels.inc). Both the per-call setup (sigmoid scales) and
 * the per-pixel render are instantiated natively for f32 and f64 from the
 * shared .inc, so the map path is byte-exact with the scalar path.
 */

#ifndef ALWAN_AGX_CORE_H
#define ALWAN_AGX_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass -> agx_*_f32 */
#include "alwan_core_f32_setup.h"
#include "alwan_agx_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass -> agx_*_f64 */
#include "alwan_core_f64_setup.h"
#include "alwan_agx_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU backends: single-precision pass via the same .inc. ALWAN_CORE_*
 * are defined by alwan_{hlsl,glsl,halide}.h (alwan_core_aliases.inc).
 * The per-pixel agx_render is GPU-portable (struct-by-value); the
 * host-only setup agx_tip_scales is #if'd out inside the .inc.
 * ================================================================ */

#include "alwan_agx_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_AGX_CORE_H */
