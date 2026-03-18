/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * IPT Color Space (Image Processing Transform)
 *
 * Reference: Ebner & Fairchild (1998)
 * "Development and Testing of a Color Space (IPT) with Improved Hue Uniformity"
 * https://www.researchgate.net/publication/221677980
 *
 * See alwan_ipt_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_ipt_core.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_ipt_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_ipt_impl.inc"
#include "alwan_api_teardown.h"
