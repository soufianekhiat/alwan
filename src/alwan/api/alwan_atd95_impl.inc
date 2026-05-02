/* No include guard — included twice (f32 and f64 passes) */

int ALWAN_CORE_FNLIT(alwan_atd95_forward)(
    ALWAN_CORE_ATD95_CORRELATES *out,
    ALWAN_CORE_XYZ const *xyz,
    ALWAN_CORE_ATD95_VIEWING_CONDITIONS const *vc
) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Always call f64 core; cast inputs to f64. */
    alwan_xyz_f64 xyz64 = { (alwan_f64)xyz->x, (alwan_f64)xyz->y, (alwan_f64)xyz->z };
    alwan_xyz_f64 white64 = { (alwan_f64)vc->white_xyz.x, (alwan_f64)vc->white_xyz.y, (alwan_f64)vc->white_xyz.z };
    alwan_atd95_v_correlates_f64 v = alwan_atd95_forward_f64_v(
        xyz64, white64, (alwan_f64)vc->Y_0, (alwan_f64)vc->k1, (alwan_f64)vc->k2, (alwan_f64)vc->sigma
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
