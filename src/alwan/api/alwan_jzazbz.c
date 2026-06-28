/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Jzazbz & JzCzhz Color Spaces (HDR Perceptual)
 *
 * Reference: Safdar et al. (2017)
 * "Perceptually uniform color space for image signals including
 *  high dynamic range and wide gamut"
 * https://opg.optica.org/oe/fulltext.cfm?uri=oe-25-13-15131
 *
 * See alwan_jzazbz_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_jzazbz_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_jzazbz_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_jzazbz_impl.inc"
#include "alwan_api_teardown.h"
#endif
