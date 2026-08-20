/* No include guard -- included twice (f32 and f64 passes) */

/* ----------------------------------------------------------------
 * Surround enum -> F, c, Nc resolution
 *
 * Enums are not cross-platform (HLSL/Halide), so surround
 * resolution lives here in the templated API wrapper.
 *
 * DIM Nc = 0.9 here (Hellwig2022); CIECAM02/CAM16 uses 0.95 per
 * Li et al. 2022 -- intentional spec difference.
 * ---------------------------------------------------------------- */

static void ALWAN_CORE_FNLIT(hw22_get_surround_params)(
    alwan_hellwig2022_surround surround,
    ALWAN_CORE_T *F, ALWAN_CORE_T *c, ALWAN_CORE_T *Nc) {
    switch (surround) {
        case ALWAN_HELLWIG2022_SURROUND_AVERAGE:
            *F  = ALWAN_CORE_LITERAL(1.0);
            *c  = ALWAN_CORE_LITERAL(0.69);
            *Nc = ALWAN_CORE_LITERAL(1.0);
            break;
        case ALWAN_HELLWIG2022_SURROUND_DIM:
            *F  = ALWAN_CORE_LITERAL(0.9);
            *c  = ALWAN_CORE_LITERAL(0.59);
            *Nc = ALWAN_CORE_LITERAL(0.9);
            break;
        case ALWAN_HELLWIG2022_SURROUND_DARK:
            *F  = ALWAN_CORE_LITERAL(0.8);
            *c  = ALWAN_CORE_LITERAL(0.525);
            *Nc = ALWAN_CORE_LITERAL(0.8);
            break;
        default:
            *F  = ALWAN_CORE_LITERAL(1.0);
            *c  = ALWAN_CORE_LITERAL(0.69);
            *Nc = ALWAN_CORE_LITERAL(1.0);
            break;
    }
}

/* ----------------------------------------------------------------
 * Hellwig2022 Forward Transform
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_hellwig2022_forward)(
    ALWAN_CORE_HELLWIG2022_CORRELATES *out,
    ALWAN_CORE_XYZ const *xyz,
    ALWAN_CORE_HELLWIG2022_VIEWING_CONDITIONS const *vc) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Resolve surround enum to scalar parameters */
    ALWAN_CORE_T F, c, Nc;
    ALWAN_CORE_FNLIT(hw22_get_surround_params)(vc->surround, &F, &c, &Nc);

    /* Delegate to the core value-returning function */
    ALWAN_CORE_FNLIT(alwan_hellwig2022_v_correlates) v = ALWAN_CORE_FNVLIT(alwan_hellwig2022_forward)(
        *xyz,
        vc->white_xyz.x, vc->white_xyz.y, vc->white_xyz.z,
        F, c, Nc,
        vc->adapting_luminance,
        vc->background_luminance,
        vc->white_xyz.y,
        (ALWAN_CORE_T)vc->discount_illuminant);

    /* Copy correlates to output */
    out->J = v.J;
    out->C = v.C;
    out->h = v.h;
    out->s = v.s;
    out->Q = v.Q;
    out->M = v.M;
    out->H = v.H;

    ALWAN_NORM_HELLWIG2022(out);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Hellwig2022 Inverse Transform
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_hellwig2022_inverse)(
    ALWAN_CORE_XYZ *xyz_out,
    ALWAN_CORE_HELLWIG2022_CORRELATES const *correlates,
    ALWAN_CORE_HELLWIG2022_VIEWING_CONDITIONS const *vc) {
    if (!correlates || !vc || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_HELLWIG2022_CORRELATES tmp = *correlates;
    ALWAN_DENORM_HELLWIG2022(&tmp);

    /* Resolve surround enum to scalar parameters */
    ALWAN_CORE_T F, c, Nc;
    ALWAN_CORE_FNLIT(hw22_get_surround_params)(vc->surround, &F, &c, &Nc);

    /* Build core correlates from public correlates (only J, C, h needed for inverse) */
    ALWAN_CORE_FNLIT(alwan_hellwig2022_v_correlates) v_corr;
    v_corr.J = tmp.J;
    v_corr.C = tmp.C;
    v_corr.h = tmp.h;
    v_corr.s = ALWAN_CORE_LITERAL(0.0);
    v_corr.Q = ALWAN_CORE_LITERAL(0.0);
    v_corr.M = ALWAN_CORE_LITERAL(0.0);
    v_corr.H = ALWAN_CORE_LITERAL(0.0);

    /* Delegate to the core value-returning function */
    *xyz_out = ALWAN_CORE_FNVLIT(alwan_hellwig2022_inverse)(
        v_corr,
        vc->white_xyz.x, vc->white_xyz.y, vc->white_xyz.z,
        F, c, Nc,
        vc->adapting_luminance,
        vc->background_luminance,
        vc->white_xyz.y,
        (ALWAN_CORE_T)vc->discount_illuminant);

    return ALWAN_OK;
}
