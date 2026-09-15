/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Exposure fusion, Mertens, Kautz and Van Reeth 2007. Native dual precision: the
 * implementation is templated in alwan_fusion_impl.inc and instantiated once per
 * precision.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stddef.h>

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f32_setup.h"
#include "alwan_fusion_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f64_setup.h"
#include "alwan_fusion_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif
