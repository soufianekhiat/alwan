/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * IEEE 754 binary16 (half-float) <-> float conversion
 * Software implementation using bit manipulation (~5 instructions each way).
 * Used by the Color Interop Forum float16 pixel format.
 *
 * Reference: IEEE 754-2008, Section 3.6 (binary16)
 */

#ifndef ALWAN_HALF_CORE_H
#define ALWAN_HALF_CORE_H

#include "../alwan_platform.h"
#include <stdint.h>

/* ================================================================
 * Half-float (binary16) representation
 *
 *   Bit layout: S EEEEE MMMMMMMMM (1 + 5 + 10 = 16 bits)
 *   Exponent bias: 15
 *   Denorm:  E=0, M!=0  -> value = (-1)^S * 2^-14 * (M/1024)
 *   Zero:    E=0, M=0   -> value = (-1)^S * 0
 *   Normal:  1<=E<=30   -> value = (-1)^S * 2^(E-15) * (1 + M/1024)
 *   Inf:     E=31, M=0  -> value = (-1)^S * Inf
 *   NaN:     E=31, M!=0 -> NaN
 * ================================================================ */

typedef uint16_t alwan_half;

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * Note: half-float operations are inherently float32, but we
 * generate both suffixed variants for API consistency.
 * ================================================================ */

#include "../alwan_types.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_half_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_half_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

/* Convert float32 -> float16 (with rounding to nearest even) */
ALWAN_INLINE alwan_half alwan_float_to_half_v(float f) {
    uint32_t fbits;
    /* Type-pun via memcpy to avoid strict aliasing violations */
    ALWAN_MEMCPY(&fbits, &f, sizeof(fbits));

    uint32_t sign = (fbits >> 16) & 0x8000u;
    int32_t  exp  = (int32_t)((fbits >> 23) & 0xFFu) - 127;
    uint32_t mant = fbits & 0x007FFFFFu;

    if (exp > 15) {
        /* Overflow -> Inf (or NaN if source was NaN) */
        if (exp == 128 && mant != 0) {
            /* NaN: preserve some mantissa bits */
            return (alwan_half)(sign | 0x7C00u | (mant >> 13));
        }
        return (alwan_half)(sign | 0x7C00u);
    }

    if (exp > -15) {
        /* Normal range: round to nearest even */
        uint32_t round_bit = (mant >> 12) & 1u;
        uint32_t sticky = (mant & 0x0FFFu) ? 1u : 0u;
        uint32_t guard = (mant >> 12) & 1u;
        uint32_t half_mant = mant >> 13;

        /* Round to nearest even */
        if ((mant & 0x1FFFu) > 0x1000u ||
            ((mant & 0x1FFFu) == 0x1000u && (half_mant & 1u))) {
            half_mant++;
            if (half_mant > 0x3FFu) {
                half_mant = 0;
                exp++;
                if (exp > 15) {
                    return (alwan_half)(sign | 0x7C00u);
                }
            }
        }
        ALWAN_UNUSED(round_bit); ALWAN_UNUSED(sticky); ALWAN_UNUSED(guard);

        return (alwan_half)(sign | (uint32_t)((exp + 15) << 10) | half_mant);
    }

    if (exp >= -24) {
        /* Denormalized: shift mantissa with implicit leading 1 */
        mant |= 0x00800000u; /* add implicit leading 1 */
        int shift = -exp - 1; /* shift = 14 + (-exp - 15 + 1) => but we need total shift from 23-bit to 10-bit + denorm */
        shift = 13 + (-exp - 14); /* 13 is normal shift, plus extra for each step below normal */
        uint32_t half_mant = mant >> shift;

        /* Round to nearest even */
        uint32_t remainder = mant & ((1u << shift) - 1u);
        uint32_t halfway = 1u << (shift - 1);
        if (remainder > halfway ||
            (remainder == halfway && (half_mant & 1u))) {
            half_mant++;
        }

        return (alwan_half)(sign | half_mant);
    }

    /* Too small -> zero */
    return (alwan_half)sign;
}

/* Convert float16 -> float32 */
ALWAN_INLINE float alwan_half_to_float_v(alwan_half h) {
    uint32_t sign = ((uint32_t)h & 0x8000u) << 16;
    uint32_t exp  = ((uint32_t)h >> 10) & 0x1Fu;
    uint32_t mant = (uint32_t)h & 0x03FFu;

    uint32_t fbits;

    if (exp == 0) {
        if (mant == 0) {
            /* Zero (positive or negative) */
            fbits = sign;
        } else {
            /* Denormalized: normalize */
            exp = 1;
            while ((mant & 0x0400u) == 0) {
                mant <<= 1;
                exp--;
            }
            mant &= 0x03FFu; /* remove implicit leading 1 */
            fbits = sign | ((uint32_t)(exp + 127 - 15) << 23) | (mant << 13);
        }
    } else if (exp == 31) {
        /* Inf or NaN */
        fbits = sign | 0x7F800000u | (mant << 13);
    } else {
        /* Normalized */
        fbits = sign | ((uint32_t)(exp + 127 - 15) << 23) | (mant << 13);
    }

    float result;
    ALWAN_MEMCPY(&result, &fbits, sizeof(result));
    return result;
}

#endif

#endif /* ALWAN_HALF_CORE_H */
