/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * EXPERIMENTAL: film halation, the light that comes back from the base. The
 * declarations, the derivation and the references are in the film section of
 * alwan.h. Everything under experimental/ is research code.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>

alwan_status alwan_film_halation_params_default(alwan_film_halation_params *params, alwan_f64 rim_radius) {
    if (!params || !(rim_radius > 0.0)) return ALWAN_E_INVALID;
    params->rim_radius = rim_radius;
    params->index = 1.5;
    params->reach = 3.0;
    params->strength[0] = 0.08;
    params->strength[1] = 0.03;
    params->strength[2] = 0.01;
    return ALWAN_OK;
}

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "../api/alwan_api_f32_setup.h"
#include "alwan_film_halation_impl.inc"
#include "../api/alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "../api/alwan_api_f64_setup.h"
#include "alwan_film_halation_impl.inc"
#include "../api/alwan_api_teardown.h"
#endif
