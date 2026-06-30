/* No include guard -- included twice (f32 and f64 passes) */

void ALWAN_CORE_FNLIT(alwan_xyz_to_jzazbz)(ALWAN_CORE_JZAZBZ *jzazbz, ALWAN_CORE_XYZ const *xyz) {
    *jzazbz = ALWAN_CORE_FNVLIT(alwan_xyz_to_jzazbz)(*xyz);
}

void ALWAN_CORE_FNLIT(alwan_jzazbz_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_JZAZBZ const *jzazbz) {
    *xyz = ALWAN_CORE_FNVLIT(alwan_jzazbz_to_xyz)(*jzazbz);
}

void ALWAN_CORE_FNLIT(alwan_jzazbz_to_jzczhz)(ALWAN_CORE_JZCZHZ *jzczhz, ALWAN_CORE_JZAZBZ const *jzazbz) {
    *jzczhz = ALWAN_CORE_FNVLIT(alwan_jzazbz_to_jzczhz)(*jzazbz);
    ALWAN_NORM_JZCZHZ(jzczhz);
}

void ALWAN_CORE_FNLIT(alwan_jzczhz_to_jzazbz)(ALWAN_CORE_JZAZBZ *jzazbz, ALWAN_CORE_JZCZHZ const *jzczhz) {
    ALWAN_CORE_JZCZHZ tmp = *jzczhz;
    ALWAN_DENORM_JZCZHZ(&tmp);
    *jzazbz = ALWAN_CORE_FNVLIT(alwan_jzczhz_to_jzazbz)(tmp);
}
