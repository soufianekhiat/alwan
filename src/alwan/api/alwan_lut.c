/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * LUT baking, 2D flatten/unflatten, and trilinear sampling
 * Per-pixel math in alwan_lut_core.h, TFs in alwan_rgb_core.h
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_lut_core.h"
#include "../core/alwan_rgb_core.h"
#include "../core/alwan_colorspace_core.h"
#include "../core/alwan_view_core.h"
#include "../core/alwan_hdr_core.h"
#include "../core/alwan_math_core.h"
#include <string.h>

/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_lut_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

/* Compiled in every build, not just an f64 one: the documented f64-internal
 * facades (the spectral quality metrics, the CCM fits, the iterative inverses)
 * call this f64 API from their f32 entry points, so it has to exist even when
 * the f64 public surface is otherwise excluded. See ALWAN_WITH_F64_FACADE. */
#if ALWAN_WITH_F64_FACADE
#include "alwan_api_f64_setup.h"
#include "alwan_lut_impl.inc"
#include "alwan_api_teardown.h"
#endif /* ALWAN_WITH_F64_FACADE */
