/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ATD95 Color Vision Model
 *
 * References:
 * - Guth, S. L. (1995). Further applications of the ATD model for color vision.
 * - Fairchild, M. D. (2013). Color Appearance Models (3rd ed.). Wiley.
 *
 * See alwan_atd95_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_atd95_core.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_atd95_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_atd95_impl.inc"
#include "alwan_api_teardown.h"
