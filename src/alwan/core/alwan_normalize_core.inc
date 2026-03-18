/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Integer-to-Float Normalization (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Uses ALWAN_CORE_* macros (set by alwan_core_f32_setup.h or _f64_setup.h).
 */

/* Return the maximum integer value for a given bit depth: (2^N - 1) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_normalize_max)(int bit_depth) {
    switch (bit_depth) {
        case  8: return ALWAN_CORE_LITERAL(255.0);
        case 10: return ALWAN_CORE_LITERAL(1023.0);
        case 12: return ALWAN_CORE_LITERAL(4095.0);
        case 16: return ALWAN_CORE_LITERAL(65535.0);
        default: return ALWAN_CORE_LITERAL(-1.0);
    }
}

/* Convert a single unsigned integer sample to normalized [0,1] float */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_uint_to_float)(alwan_uint16 value, int bit_depth) {
    ALWAN_CORE_T max_val = ALWAN_CORE_FNVLIT(alwan_normalize_max)(bit_depth);
    return (ALWAN_CORE_T)value / max_val;
}

/* Convert a single normalized [0,1] float to unsigned integer (clamp + round) */
ALWAN_INLINE alwan_uint16 ALWAN_CORE_FNVLIT(alwan_float_to_uint)(ALWAN_CORE_T value, int bit_depth) {
    ALWAN_CORE_T max_val = ALWAN_CORE_FNVLIT(alwan_normalize_max)(bit_depth);
    ALWAN_CORE_T clamped = ALWAN_CORE_CLAMP(value, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    return (alwan_uint16)(clamped * max_val + ALWAN_CORE_LITERAL(0.5));
}
