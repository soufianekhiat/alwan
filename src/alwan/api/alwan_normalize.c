/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Integer-to-Float Normalization (ColorInterop Section 1.5)
 * Batch API for converting between integer code values and normalized floats.
 */

#include "../alwan.h"
#include "../core/alwan_normalize_core.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_normalize_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_normalize_impl.inc"
#include "alwan_api_teardown.h"

int alwan_uint_to_float(alwan_f64 *out, alwan_uint16 const *in,
                        int bit_depth, size_t count) {
    if (!out || !in || count == 0) {
        return ALWAN_E_INVALID;
    }

    alwan_f64 max_val = alwan_normalize_max_f64_v(bit_depth);
    if (max_val < ALWAN_ZERO) {
        return ALWAN_E_INVALID;  /* unsupported bit depth */
    }

    alwan_f64 inv_max = ALWAN_ONE / max_val;
    for (size_t i = 0; i < count; i++) {
        out[i] = (alwan_f64)in[i] * inv_max;
    }

    return ALWAN_OK;
}

int alwan_float_to_uint(alwan_uint16 *out, alwan_f64 const *in,
                        int bit_depth, size_t count) {
    if (!out || !in || count == 0) {
        return ALWAN_E_INVALID;
    }

    alwan_f64 max_val = alwan_normalize_max_f64_v(bit_depth);
    if (max_val < ALWAN_ZERO) {
        return ALWAN_E_INVALID;  /* unsupported bit depth */
    }

    for (size_t i = 0; i < count; i++) {
        alwan_f64 clamped = alwan_clamp(in[i], ALWAN_ZERO, ALWAN_ONE);
        out[i] = (alwan_uint16)(clamped * max_val + ALWAN_LITERAL(0.5));
    }

    return ALWAN_OK;
}
