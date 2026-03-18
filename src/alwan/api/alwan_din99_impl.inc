/* No include guard — included twice (f32 and f64 passes) */

void ALWAN_CORE_FNLIT(alwan_lab_to_din99)(ALWAN_CORE_DIN99 *din99, ALWAN_CORE_LAB const *lab, int variant) {
    if (variant < 0 || variant > 3) return;
    ALWAN_CORE_LAB tmp = *lab;
    ALWAN_DENORM_LAB(&tmp);
    *din99 = ALWAN_CORE_FNVLIT(alwan_lab_to_din99)(tmp, variant);
    ALWAN_NORM_DIN99(din99);
}

void ALWAN_CORE_FNLIT(alwan_din99_to_lab)(ALWAN_CORE_LAB *lab, ALWAN_CORE_DIN99 const *din99, int variant) {
    if (variant < 0 || variant > 3) return;
    ALWAN_CORE_DIN99 tmp = *din99;
    ALWAN_DENORM_DIN99(&tmp);
    *lab = ALWAN_CORE_FNVLIT(alwan_din99_to_lab)(tmp, variant);
    ALWAN_NORM_LAB(lab);
}
