/* No include guard -- included twice (f32 and f64 passes) */

/* ----------------------------------------------------------------
 * Arbitrary Gamma OETF/EOTF
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_gamma_oetf)(ALWAN_CORE_T *out, size_t out_stride, ALWAN_CORE_T const *in, size_t in_stride, size_t count, ALWAN_CORE_T gamma) {
    if (!out || !in) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T const *in_ptr = (ALWAN_CORE_T const *)((char const *)in + i * in_stride);
        ALWAN_CORE_T *out_ptr = (ALWAN_CORE_T *)((char *)out + i * out_stride);
        *out_ptr = ALWAN_CORE_FNVLIT(alwan_gamma_oetf)(*in_ptr, gamma);
    }
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_gamma_eotf)(ALWAN_CORE_T *out, size_t out_stride, ALWAN_CORE_T const *in, size_t in_stride, size_t count, ALWAN_CORE_T gamma) {
    if (!out || !in) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T const *in_ptr = (ALWAN_CORE_T const *)((char const *)in + i * in_stride);
        ALWAN_CORE_T *out_ptr = (ALWAN_CORE_T *)((char *)out + i * out_stride);
        *out_ptr = ALWAN_CORE_FNVLIT(alwan_gamma_eotf)(*in_ptr, gamma);
    }
    return ALWAN_OK;
}
