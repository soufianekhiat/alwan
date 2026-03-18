/* No include guard — included twice (f32 and f64 passes) */

void ALWAN_CORE_FNLIT(alwan_hlg_ootf)(ALWAN_CORE_RGB *out, ALWAN_CORE_RGB const *in,
                                      ALWAN_CORE_T Lw, ALWAN_CORE_T gamma_sys) {
    if (!out || !in) return;
    *out = ALWAN_CORE_FNVLIT(alwan_hlg_ootf)(*in, Lw, gamma_sys);
}

void ALWAN_CORE_FNLIT(alwan_hlg_ootf_inv)(ALWAN_CORE_RGB *out, ALWAN_CORE_RGB const *in,
                                          ALWAN_CORE_T Lw, ALWAN_CORE_T gamma_sys) {
    if (!out || !in) return;
    *out = ALWAN_CORE_FNVLIT(alwan_hlg_ootf_inv)(*in, Lw, gamma_sys);
}

void ALWAN_CORE_FNLIT(alwan_hdr_gamut_map_jzczhz)(ALWAN_CORE_JZCZHZ *out, ALWAN_CORE_JZCZHZ const *in,
                                                   ALWAN_CORE_T Cz_max) {
    if (!out || !in) return;
    *out = ALWAN_CORE_FNVLIT(alwan_hdr_gamut_map_jzczhz)(*in, Cz_max);
}
