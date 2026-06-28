/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_colorspace_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_colorspace_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_colorspace_impl.inc"
#include "alwan_api_teardown.h"
#endif

void alwan_delta_e_cmc_params_default_f32(alwan_delta_e_cmc_params_f32 *p) {
    p->l = 2.0f;
    p->c = 1.0f;
}

void alwan_delta_e_cmc_params_default_f64(alwan_delta_e_cmc_params_f64 *p) {
    p->l = 2.0;
    p->c = 1.0;
}
