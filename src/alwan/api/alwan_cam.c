/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * CIECAM02 / CAM16 / CAM18sl / CAM20u Color Appearance Models.
 *
 * Derived-parameter and appearance math lives in the templated cores
 * (alwan_cam_core.h, alwan_cam18sl_core.h, alwan_cam20u_core.h). The
 * scalar entry points are instantiated natively for both f32 and f64
 * from alwan_cam_impl.inc (included once per precision below), so the
 * single-precision path -- including the bulk _map_interleave path that
 * calls these wrappers -- computes in float throughout rather than
 * widening to double.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_cam_core.h"
#include "../core/alwan_cam18sl_core.h"
#include "../core/alwan_cam20u_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_cam_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_cam_impl.inc"
#include "alwan_api_teardown.h"
#endif

/* _map_interleave variants are implemented in alwan_cam_map.c (SIMD-parameterized).
 * _ex format-dispatch variants are implemented in alwan_typed_map.c.
 * Both call the single-element wrappers defined above. */
