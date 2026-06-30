/* No include guard -- included twice (f32 and f64 passes) */

void ALWAN_CORE_FNLIT(alwan_xyz_to_hunter_lab)(ALWAN_CORE_HUNTER_LAB *hunter_lab, ALWAN_CORE_XYZ const *xyz) {
    *hunter_lab = ALWAN_CORE_FNVLIT(alwan_xyz_to_hunter_lab)(*xyz);
    ALWAN_NORM_HUNTER_LAB(hunter_lab);
}

void ALWAN_CORE_FNLIT(alwan_hunter_lab_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_HUNTER_LAB const *hunter_lab) {
    ALWAN_CORE_HUNTER_LAB tmp = *hunter_lab;
    ALWAN_DENORM_HUNTER_LAB(&tmp);
    *xyz = ALWAN_CORE_FNVLIT(alwan_hunter_lab_to_xyz)(tmp);
}

void ALWAN_CORE_FNLIT(alwan_xyz_to_hunter_lab_custom)(ALWAN_CORE_HUNTER_LAB *hunter_lab,
                                                       ALWAN_CORE_XYZ const *xyz,
                                                       ALWAN_CORE_XYZ const *xyz_n) {
    *hunter_lab = ALWAN_CORE_FNVLIT(alwan_xyz_to_hunter_lab_custom)(*xyz, *xyz_n);
    ALWAN_NORM_HUNTER_LAB(hunter_lab);
}

void ALWAN_CORE_FNLIT(alwan_hunter_lab_to_xyz_custom)(ALWAN_CORE_XYZ *xyz,
                                                       ALWAN_CORE_HUNTER_LAB const *hunter_lab,
                                                       ALWAN_CORE_XYZ const *xyz_n) {
    ALWAN_CORE_HUNTER_LAB tmp = *hunter_lab;
    ALWAN_DENORM_HUNTER_LAB(&tmp);
    *xyz = ALWAN_CORE_FNVLIT(alwan_hunter_lab_to_xyz_custom)(tmp, *xyz_n);
}
