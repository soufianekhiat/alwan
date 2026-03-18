/* No include guard — included twice (f32 and f64 passes) */

/* ================================================================
 * Accessibility Contrast Metrics
 * ================================================================ */

int ALWAN_CORE_FNLIT(alwan_apca_contrast)(ALWAN_CORE_T *Lc_out,
                                          ALWAN_CORE_RGB const *srgb_text,
                                          ALWAN_CORE_RGB const *srgb_bg) {
    if (!Lc_out || !srgb_text || !srgb_bg) return ALWAN_E_INVALID;
    *Lc_out = ALWAN_CORE_FNVLIT(alwan_apca_contrast)(*srgb_text, *srgb_bg);
    return ALWAN_OK;
}
