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

/* Compiled in every build, not just an f64 one: the documented f64-internal
 * facades (the spectral quality metrics, the CCM fits, the iterative inverses)
 * call this f64 API from their f32 entry points, so it has to exist even when
 * the f64 public surface is otherwise excluded. See ALWAN_WITH_F64_FACADE. */
#if ALWAN_WITH_F64_FACADE
#include "alwan_api_f64_setup.h"
#include "alwan_colorspace_impl.inc"
#include "alwan_api_teardown.h"
#endif /* ALWAN_WITH_F64_FACADE */

void alwan_delta_e_cmc_params_default_f32(alwan_delta_e_cmc_params_f32 *p) {
    p->l = 2.0f;
    p->c = 1.0f;
}

void alwan_delta_e_cmc_params_default_f64(alwan_delta_e_cmc_params_f64 *p) {
    p->l = 2.0;
    p->c = 1.0;
}
