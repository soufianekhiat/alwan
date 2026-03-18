/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * DIN99 Family (DIN99, DIN99b, DIN99c, DIN99d)
 * See alwan_din99_core.h
 *
 * Reference: DIN 6176:2001-03, ASTM D2244-07
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_din99_core.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_din99_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_din99_impl.inc"
#include "alwan_api_teardown.h"
