/* No include guard -- included twice (f32 and f64 passes) */

alwan_status ALWAN_CORE_FNLIT(alwan_nayatani95_forward)(
    ALWAN_CORE_NAYATANI95_CORRELATES *out,
    ALWAN_CORE_XYZ const *xyz,
    ALWAN_CORE_NAYATANI95_VIEWING_CONDITIONS const *vc
) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* The model is parameterised by (white, Y_o, E_o, E_or); the viewing-
     * condition's L_0 field is reserved and not consumed by the transform. */
    ALWAN_CORE_FNLIT(alwan_nayatani95_v_correlates) v = ALWAN_CORE_FNVLIT(alwan_nayatani95_forward)(
        *xyz, vc->white_xyz, vc->Y_0, vc->E_0, vc->E_0r
    );

    out->L_star_N = (ALWAN_CORE_T)v.L_star_N;
    out->C        = (ALWAN_CORE_T)v.C;
    out->theta    = (ALWAN_CORE_T)v.theta;
    out->S        = (ALWAN_CORE_T)v.S;
    out->B_r      = (ALWAN_CORE_T)v.B_r;
    out->L_star_P = (ALWAN_CORE_T)v.L_star_P;

    ALWAN_NORM_NAYATANI95(out);

    return ALWAN_OK;
}
