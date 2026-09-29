/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * IEEE 754 binary16 (half-float) <-> float conversion (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Note: These functions operate on float/uint16_t regardless of
 * precision pass. Both f32 and f64 variants are identical.
 */

/* Convert float32 -> float16 (with rounding to nearest even) */
ALWAN_INLINE alwan_half ALWAN_CORE_FNVLIT(alwan_float_to_half)(float f) {
    /* Reinterpret the float's bits; see alwan_half_asuint above for why each
     * backend spells this differently. */
    uint32_t fbits = alwan_half_asuint(f);

    uint32_t sign = (fbits >> 16) & 0x8000u;
    int32_t  exp  = (int32_t)((fbits >> 23) & 0xFFu) - 127;
    uint32_t mant = fbits & 0x007FFFFFu;

    if (exp > 15) {
        /* Overflow -> Inf (or NaN if source was NaN) */
        if (exp == 128 && mant != 0) {
            /* NaN: keep the payload's top ten bits. A payload held only in the low
             * thirteen would leave a zero mantissa, which is infinity, so it keeps
             * bit 0 set instead (numpy's rule). */
            uint32_t payload = mant >> 13;
            return (alwan_half)(sign | 0x7C00u | (payload != 0u ? payload : 1u));
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

    if (exp >= -25) {
        /* Subnormal half: the implicit 1 joins the mantissa, which shifts right by the
         * normal 13 plus one per step below 2^-14. 2^-25 to 2^-24 is included: it is
         * half the smallest subnormal and above, and rounds up to it except at the
         * exact tie. Below 2^-25 everything rounds to zero. */
        mant |= 0x00800000u;
        int shift = 13 + (-exp - 14);
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
ALWAN_INLINE float ALWAN_CORE_FNVLIT(alwan_half_to_float)(alwan_half h) {
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

    return alwan_half_asfloat(fbits);
}
