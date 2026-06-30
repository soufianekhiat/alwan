/* No include guard -- included twice (f32 and f64 passes) */

int ALWAN_CORE_FNLIT(alwan_atd95_forward)(
    ALWAN_CORE_ATD95_CORRELATES *out,
    ALWAN_CORE_XYZ const *xyz,
    ALWAN_CORE_ATD95_VIEWING_CONDITIONS const *vc
) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_FNLIT(alwan_atd95_v_correlates) v = ALWAN_CORE_FNVLIT(alwan_atd95_forward)(
        *xyz, vc->white_xyz, vc->Y_0, vc->k1, vc->k2, vc->sigma
    );

    out->H   = (ALWAN_CORE_T)v.H;
    out->C   = (ALWAN_CORE_T)v.C;
    out->Br  = (ALWAN_CORE_T)v.Br;
    out->A_1 = (ALWAN_CORE_T)v.A_1;
    out->T_1 = (ALWAN_CORE_T)v.T_1;
    out->D_1 = (ALWAN_CORE_T)v.D_1;
    out->A_2 = (ALWAN_CORE_T)v.A_2;
    out->T_2 = (ALWAN_CORE_T)v.T_2;
    out->D_2 = (ALWAN_CORE_T)v.D_2;

    ALWAN_NORM_ATD95(out);

    return ALWAN_OK;
}
