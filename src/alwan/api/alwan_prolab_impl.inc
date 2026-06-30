/* No include guard -- included twice (f32 and f64 passes) */

void ALWAN_CORE_FNLIT(alwan_xyz_to_prolab)(ALWAN_CORE_PROLAB *prolab, ALWAN_CORE_XYZ const *xyz) {
    *prolab = ALWAN_CORE_FNVLIT(alwan_xyz_to_prolab)(*xyz);
    ALWAN_NORM_PROLAB(prolab);
}

void ALWAN_CORE_FNLIT(alwan_prolab_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_PROLAB const *prolab) {
    ALWAN_CORE_PROLAB tmp = *prolab;
    ALWAN_DENORM_PROLAB(&tmp);
    *xyz = ALWAN_CORE_FNVLIT(alwan_prolab_to_xyz)(tmp);
}

void ALWAN_CORE_FNLIT(alwan_xyz_to_prolab_custom)(ALWAN_CORE_PROLAB *prolab,
                                                   ALWAN_CORE_XYZ const *xyz,
                                                   ALWAN_CORE_XYZ const *xyz_n) {
    *prolab = ALWAN_CORE_FNVLIT(alwan_xyz_to_prolab_custom)(*xyz, *xyz_n);
    ALWAN_NORM_PROLAB(prolab);
}

void ALWAN_CORE_FNLIT(alwan_prolab_to_xyz_custom)(ALWAN_CORE_XYZ *xyz,
                                                   ALWAN_CORE_PROLAB const *prolab,
                                                   ALWAN_CORE_XYZ const *xyz_n) {
    ALWAN_CORE_PROLAB tmp = *prolab;
    ALWAN_DENORM_PROLAB(&tmp);
    *xyz = ALWAN_CORE_FNVLIT(alwan_prolab_to_xyz_custom)(tmp, *xyz_n);
}
