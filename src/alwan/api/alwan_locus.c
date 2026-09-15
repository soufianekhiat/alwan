/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Mired, Krystek 1985's Planckian locus and its inverse, and the inverse of the CIE
 * daylight locus. Templated in alwan_locus_impl.inc and instantiated once per
 * precision. The Planck 1900 locus, which needs the observer's CMFs, is in
 * alwan_spd_impl.inc; the luminous flux integrals, which need the V(lambda)
 * tables, are in alwan_vision_impl.inc.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stddef.h>

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f32_setup.h"
#include "alwan_locus_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f64_setup.h"
#include "alwan_locus_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif
