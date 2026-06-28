/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Integer-to-Float Normalization (ColorInterop Section 1.5)
 * Batch API for converting between integer code values and normalized floats.
 */

#include "../alwan.h"
#include "../core/alwan_normalize_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_normalize_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_normalize_impl.inc"
#include "alwan_api_teardown.h"
#endif

/* alwan_uint_to_float_*, alwan_float_to_uint_* are templatized in
 * alwan_normalize_impl.inc. */
