/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Yrg (Kirk 2019), IPT Ragoo 2021, sUCS (Li and Luo 2024), Izazbz (Safdar 2017 and
 * 2021) and Hunter Rdab. Templated in alwan_colour_models_impl.inc and instantiated
 * once per precision.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_hunter_lab_core.h"
#include <stddef.h>

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f32_setup.h"
#include "alwan_colour_models_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

/* Under the FACADE gate, not plain ALWAN_WITH_F64: an f64-internal facade in
 * another module calls into this one, so its f64 side has to exist even in an
 * f32-only build. Found by linking every declared _f32 entry point against an
 * ALWAN_BUILD_PRECISION=f32 library, which no CI job had ever done. */
#if ALWAN_WITH_F64_FACADE
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f64_setup.h"
#include "alwan_colour_models_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif
