/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * OSA-UCS Color Space (Optical Society of America Uniform Color Scales)
 *
 * Reference: OSA Uniform Color Scales Committee (1977)
 *
 * See alwan_osa_ucs_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_osa_ucs_core.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_osa_ucs_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_osa_ucs_impl.inc"
#include "alwan_api_teardown.h"
