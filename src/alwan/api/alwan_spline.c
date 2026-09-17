/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_interpolate_cubic_spline_{f32,f64}. The spline solves for its node slopes
 * before it evaluates anything, which needs scratch, so it sits in its own unit
 * instead of in alwan_math.c, which allocates nothing.
 */

#include "../alwan.h"
#include "../alwan_internal.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_spline_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64_FACADE
#include "alwan_api_f64_setup.h"
#include "alwan_spline_impl.inc"
#include "alwan_api_teardown.h"
#endif
