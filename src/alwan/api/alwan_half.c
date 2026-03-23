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

int alwan_half_to_float(float *out, alwan_uint16 const *in, size_t count) {
    if (!out || !in || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        out[i] = alwan_half_to_float_v((alwan_half)in[i]);
    }
    return ALWAN_OK;
}

int alwan_float_to_half(alwan_uint16 *out, float const *in, size_t count) {
    if (!out || !in || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        out[i] = (alwan_uint16)alwan_float_to_half_v(in[i]);
    }
    return ALWAN_OK;
}
