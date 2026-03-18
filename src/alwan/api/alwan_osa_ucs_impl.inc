/* No include guard — included twice (f32 and f64 passes) */

void ALWAN_CORE_FNLIT(alwan_xyz_to_osa_ucs)(ALWAN_CORE_OSA_UCS *osa_ucs, ALWAN_CORE_XYZ const *xyz) {
    *osa_ucs = ALWAN_CORE_FNVLIT(alwan_xyz_to_osa_ucs)(*xyz);
}

void ALWAN_CORE_FNLIT(alwan_osa_ucs_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_OSA_UCS const *osa_ucs) {
    *xyz = ALWAN_CORE_FNVLIT(alwan_osa_ucs_to_xyz)(*osa_ucs);
}
