/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * RLAB Color Appearance Model
 * Based on: Fairchild (1993, 1996)
 * "RLAB: a color appearance space for color reproduction"
 * "Refinement of the RLAB color space"
 *
 * Core appearance math in alwan_rlab_core.h. The scalar entry points are
 * instantiated natively for both f32 and f64 from alwan_rlab_impl.inc
 * (included once per precision below), so the single-precision path
 * computes in float throughout rather than widening to double.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_rlab_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_rlab_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_rlab_impl.inc"
#include "alwan_api_teardown.h"
#endif
