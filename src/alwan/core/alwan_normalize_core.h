/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Integer-to-Float Normalization (ColorInterop Section 1.5)
 * Value-returning inline functions for the C backend.
 *
 * Supported bit depths: 8, 10, 12, 16.
 * Uses (2^N - 1) normalization.
 */

#ifndef ALWAN_NORMALIZE_CORE_H
#define ALWAN_NORMALIZE_CORE_H

#include "../alwan_platform.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C

/* Return the maximum integer value for a given bit depth: (2^N - 1) */
ALWAN_INLINE alwan_scalar alwan_normalize_max_v(int bit_depth) {
    switch (bit_depth) {
        case  8: return ALWAN_LITERAL(255.0);
        case 10: return ALWAN_LITERAL(1023.0);
        case 12: return ALWAN_LITERAL(4095.0);
        case 16: return ALWAN_LITERAL(65535.0);
        default: return ALWAN_LITERAL(-1.0);
    }
}

/* Convert a single unsigned integer sample to normalized [0,1] float */
ALWAN_INLINE alwan_scalar alwan_uint_to_float_v(alwan_uint16 value, int bit_depth) {
    alwan_scalar max_val = alwan_normalize_max_v(bit_depth);
    return (alwan_scalar)value / max_val;
}

/* Convert a single normalized [0,1] float to unsigned integer (clamp + round) */
ALWAN_INLINE alwan_uint16 alwan_float_to_uint_v(alwan_scalar value, int bit_depth) {
    alwan_scalar max_val = alwan_normalize_max_v(bit_depth);
    alwan_scalar clamped = alwan_clamp(value, ALWAN_ZERO, ALWAN_ONE);
    return (alwan_uint16)(clamped * max_val + ALWAN_LITERAL(0.5));
}

#endif /* ALWAN_BACKEND == ALWAN_BACKEND_C */

#endif /* ALWAN_NORMALIZE_CORE_H */
