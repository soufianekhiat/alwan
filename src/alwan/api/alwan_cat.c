/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Chromatic Adaptation Transform (CAT) implementation
 *
 * This .c wrapper resolves the CAT enum to matrices loaded from
 * embedded global data and delegates the math to the _v() core
 * functions defined in alwan_cat_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_cat_core.h"
#include <string.h>

/* ----------------------------------------------------------------
 * CAT API Implementation (templated, dual-precision)
 *
 * The enum->matrix resolution, bulk adaptation loop, and Zhai 2018
 * two-step adaptation all live in alwan_cat_impl.inc, which is
 * compiled once per precision below. Cone-response matrices are
 * selected directly from the native-precision data twin
 * (g_cat_NAME_f32 / g_cat_NAME_f64), so f32 is genuinely native.
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_cat_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_cat_impl.inc"
#include "alwan_api_teardown.h"
#endif
