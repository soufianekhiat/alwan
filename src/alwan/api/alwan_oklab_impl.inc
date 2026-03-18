/* No include guard — included twice (f32 and f64 passes) */

void ALWAN_CORE_FNLIT(alwan_xyz_to_oklab)(ALWAN_CORE_OKLAB *oklab, ALWAN_CORE_XYZ const *xyz) {
    *oklab = ALWAN_CORE_FNVLIT(alwan_xyz_to_oklab)(*xyz);
}

void ALWAN_CORE_FNLIT(alwan_oklab_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_OKLAB const *oklab) {
    *xyz = ALWAN_CORE_FNVLIT(alwan_oklab_to_xyz)(*oklab);
}

void ALWAN_CORE_FNLIT(alwan_oklab_to_oklch)(ALWAN_CORE_OKLCH *oklch, ALWAN_CORE_OKLAB const *oklab) {
    *oklch = ALWAN_CORE_FNVLIT(alwan_oklab_to_oklch)(*oklab);
    ALWAN_NORM_OKLCH(oklch);
}

void ALWAN_CORE_FNLIT(alwan_oklch_to_oklab)(ALWAN_CORE_OKLAB *oklab, ALWAN_CORE_OKLCH const *oklch) {
    ALWAN_CORE_OKLCH tmp = *oklch;
    ALWAN_DENORM_OKLCH(&tmp);
    *oklab = ALWAN_CORE_FNVLIT(alwan_oklch_to_oklab)(tmp);
}
