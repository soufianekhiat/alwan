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

int alwan_half_to_float_f64(alwan_f32 *out, alwan_uint16 const *in, size_t count) {
    if (!out || !in || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        out[i] = alwan_half_to_float_f64_v((alwan_half)in[i]);
    }
    return ALWAN_OK;
}

int alwan_half_to_float_f32(alwan_f32 *out, alwan_uint16 const *in, size_t count) {
    if (!out || !in || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        out[i] = alwan_half_to_float_f32_v((alwan_half)in[i]);
    }
    return ALWAN_OK;
}

int alwan_float_to_half_f64(alwan_uint16 *out, alwan_f32 const *in, size_t count) {
    if (!out || !in || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        out[i] = (alwan_uint16)alwan_float_to_half_f64_v(in[i]);
    }
    return ALWAN_OK;
}

int alwan_float_to_half_f32(alwan_uint16 *out, alwan_f32 const *in, size_t count) {
    if (!out || !in || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        out[i] = (alwan_uint16)alwan_float_to_half_f32_v(in[i]);
    }
    return ALWAN_OK;
}
