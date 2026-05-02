/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * HDR Pipeline Utilities
 * Per-pixel math in alwan_hdr_core.h
 *
 * HLG OOTF, MaxCLL, MaxFALL, BT.2408 reference white.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_hdr_core.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_hdr_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_hdr_impl.inc"
#include "alwan_api_teardown.h"
