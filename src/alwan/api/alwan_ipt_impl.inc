/* No include guard — included twice (f32 and f64 passes) */

void ALWAN_CORE_FNLIT(alwan_xyz_to_ipt)(ALWAN_CORE_IPT *ipt, ALWAN_CORE_XYZ const *xyz) {
    *ipt = ALWAN_CORE_FNVLIT(alwan_xyz_to_ipt)(*xyz);
}

void ALWAN_CORE_FNLIT(alwan_ipt_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_IPT const *ipt) {
    *xyz = ALWAN_CORE_FNVLIT(alwan_ipt_to_xyz)(*ipt);
}

void ALWAN_CORE_FNLIT(alwan_ipt_to_iptch)(ALWAN_CORE_IPTCH *iptch, ALWAN_CORE_IPT const *ipt) {
    *iptch = ALWAN_CORE_FNVLIT(alwan_ipt_to_iptch)(*ipt);
    ALWAN_NORM_IPTCH(iptch);
}

void ALWAN_CORE_FNLIT(alwan_iptch_to_ipt)(ALWAN_CORE_IPT *ipt, ALWAN_CORE_IPTCH const *iptch) {
    ALWAN_CORE_IPTCH tmp = *iptch;
    ALWAN_DENORM_IPTCH(&tmp);
    *ipt = ALWAN_CORE_FNVLIT(alwan_iptch_to_ipt)(tmp);
}
