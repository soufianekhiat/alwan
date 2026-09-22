/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * IEEE 754 binary16 (half-float) batch conversion API
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_half_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_half_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_half_impl.inc"
#include "alwan_api_teardown.h"
#endif

/* One body each since 3.0.0: a half-float conversion has no precision to pick.
 * The f64 core is used because every core is instantiated in both precisions
 * whatever the build's API precision, and a float widened to double and rounded
 * to half rounds exactly as the float would (the float is exact in double). */
alwan_status alwan_half_to_float(alwan_f32 *out, alwan_uint16 const *in, size_t count) {
    if (!out || !in || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        out[i] = alwan_half_to_float_f64_v((alwan_half)in[i]);
    }
    return ALWAN_OK;
}

alwan_status alwan_float_to_half(alwan_uint16 *out, alwan_f32 const *in, size_t count) {
    if (!out || !in || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        out[i] = (alwan_uint16)alwan_float_to_half_f64_v(in[i]);
    }
    return ALWAN_OK;
}
