/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Hunter Lab Color Space
 * See alwan_hunter_lab_core.h
 *
 * Reference: Hunter (1948), ASTM D 1535
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_hunter_lab_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_hunter_lab_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_hunter_lab_impl.inc"
#include "alwan_api_teardown.h"
#endif
