/* No include guard — included twice (f32 and f64 passes) */

int ALWAN_CORE_FNLIT(alwan_atd95_forward)(
    alwan_atd95_correlates *out,
    ALWAN_CORE_XYZ const *xyz,
    alwan_atd95_viewing_conditions const *vc
) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* ATD95 viewing conditions use alwan_xyz (f64); always call f64 core. */
    alwan_xyz_f64 xyz64 = { (alwan_f64)xyz->x, (alwan_f64)xyz->y, (alwan_f64)xyz->z };
    alwan_atd95_v_correlates_f64 v = alwan_atd95_forward_f64_v(
        xyz64, vc->white_xyz, vc->Y_0, vc->k1, vc->k2, vc->sigma
    );

    out->H   = v.H;
    out->C   = v.C;
    out->Br  = v.Br;
    out->A_1 = v.A_1;
    out->T_1 = v.T_1;
    out->D_1 = v.D_1;
    out->A_2 = v.A_2;
    out->T_2 = v.T_2;
    out->D_2 = v.D_2;

    ALWAN_NORM_ATD95(out);

    return ALWAN_OK;
}
