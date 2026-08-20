/* No include guard -- included twice (f32 and f64 passes) */

alwan_status ALWAN_CORE_FNLIT(alwan_uint_to_float)(ALWAN_CORE_T *out, alwan_uint16 const *in,
                                           int bit_depth, size_t count) {
    if (!out || !in || count == 0) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_T max_val = ALWAN_CORE_FNVLIT(alwan_normalize_max)(bit_depth);
    if (max_val < ALWAN_CORE_ZERO) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_T inv_max = ALWAN_CORE_ONE / max_val;
    for (size_t i = 0; i < count; i++) {
        out[i] = (ALWAN_CORE_T)in[i] * inv_max;
    }

    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_float_to_uint)(alwan_uint16 *out, ALWAN_CORE_T const *in,
                                           int bit_depth, size_t count) {
    if (!out || !in || count == 0) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_T max_val = ALWAN_CORE_FNVLIT(alwan_normalize_max)(bit_depth);
    if (max_val < ALWAN_CORE_ZERO) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T clamped = ALWAN_CORE_CLAMP(in[i], ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
        out[i] = (alwan_uint16)(clamped * max_val + ALWAN_CORE_LITERAL(0.5));
    }

    return ALWAN_OK;
}
