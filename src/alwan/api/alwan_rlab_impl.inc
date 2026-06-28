/* No include guard — included twice (f32 and f64 passes).
 *
 * Native dual-precision RLAB scalar entry points. The core appearance
 * math lives in alwan_rlab_core.h (templated); these wrappers resolve
 * the surround/discounting enums to the active precision and map the
 * value-struct fields, so the f32 path computes in float throughout.
 */

/* Surround parameter (sigma) from viewing-condition enum. */
static ALWAN_CORE_T ALWAN_CORE_FN(rlab_get_sigma)(alwan_rlab_surround surround) {
    switch (surround) {
        case ALWAN_RLAB_SURROUND_AVERAGE: return ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.3);
        case ALWAN_RLAB_SURROUND_DIM:     return ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.9);
        case ALWAN_RLAB_SURROUND_DARK:    return ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(3.5);
        default:                          return ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.3);
    }
}

/* Discounting-the-illuminant factor (D) from media setting. */
static ALWAN_CORE_T ALWAN_CORE_FN(rlab_get_D_factor)(int D_setting) {
    switch (D_setting) {
        case 1:  return ALWAN_CORE_LITERAL(1.0);  /* Hard copy images */
        case 2:  return ALWAN_CORE_LITERAL(0.0);  /* Soft copy images */
        case 3:  return ALWAN_CORE_LITERAL(0.5);  /* Projected transparencies */
        default: return ALWAN_CORE_LITERAL(1.0);  /* Auto: default 1.0 */
    }
}

/* RLAB forward: XYZ -> correlates. */
int ALWAN_CORE_FNLIT(alwan_rlab_forward)(
        ALWAN_CORE_RLAB_CORRELATES *out,
        ALWAN_CORE_XYZ const *xyz,
        ALWAN_CORE_RLAB_VIEWING_CONDITIONS const *vc) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_T sigma = ALWAN_CORE_FN(rlab_get_sigma)(vc->surround);
    ALWAN_CORE_T D     = ALWAN_CORE_FN(rlab_get_D_factor)(vc->D_factor);

    ALWAN_CORE_FNLIT(alwan_rlab_v_correlates) result =
        ALWAN_CORE_FNVLIT(alwan_rlab_forward)(*xyz, vc->xyz_w, vc->xyz_n, sigma, D);

    out->L = result.L;
    out->a = result.a;
    out->b = result.b;
    out->h = result.h;
    out->C = result.C;
    out->s = result.s;

    ALWAN_NORM_RLAB(out);

    return ALWAN_OK;
}

/* RLAB inverse: correlates -> XYZ. */
int ALWAN_CORE_FNLIT(alwan_rlab_inverse)(
        ALWAN_CORE_XYZ *xyz,
        ALWAN_CORE_RLAB_CORRELATES const *correlates,
        ALWAN_CORE_RLAB_VIEWING_CONDITIONS const *vc) {
    if (!xyz || !correlates || !vc) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_RLAB_CORRELATES tmp = *correlates;
    ALWAN_DENORM_RLAB(&tmp);

    ALWAN_CORE_T sigma = ALWAN_CORE_FN(rlab_get_sigma)(vc->surround);
    ALWAN_CORE_T D     = ALWAN_CORE_FN(rlab_get_D_factor)(vc->D_factor);

    ALWAN_CORE_FNLIT(alwan_rlab_v_correlates) vc_in;
    vc_in.L = tmp.L;
    vc_in.a = tmp.a;
    vc_in.b = tmp.b;
    vc_in.h = tmp.h;
    vc_in.C = tmp.C;
    vc_in.s = tmp.s;

    *xyz = ALWAN_CORE_FNVLIT(alwan_rlab_inverse)(vc_in, vc->xyz_w, vc->xyz_n, sigma, D);

    return ALWAN_OK;
}
