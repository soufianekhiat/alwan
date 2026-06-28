/* No include guard — included twice (f32 and f64 passes).
 *
 * Native dual-precision CIECAM02 / CAM16 / CAM18sl / CAM20u scalar entry
 * points. The appearance math lives in the templated cores
 * (alwan_cam_core.h, alwan_cam18sl_core.h, alwan_cam20u_core.h); these
 * wrappers resolve the surround enums to the active precision and map
 * the value-struct fields, so the f32 path (including the bulk
 * _map_interleave path that calls these wrappers) computes in float
 * throughout rather than widening to double.
 */

/* ---- surround enum resolution (per model) ---- */
static void ALWAN_CORE_FN(get_surround_params)(alwan_ciecam02_surround surround,
                                ALWAN_CORE_T *F, ALWAN_CORE_T *c, ALWAN_CORE_T *Nc) {
    switch (surround) {
        case ALWAN_CIECAM02_SURROUND_AVERAGE:
            *F = ALWAN_CORE_LITERAL(1.0); *c = ALWAN_CORE_LITERAL(0.69); *Nc = ALWAN_CORE_LITERAL(1.0); break;
        case ALWAN_CIECAM02_SURROUND_DIM:
            *F = ALWAN_CORE_LITERAL(0.9); *c = ALWAN_CORE_LITERAL(0.59); *Nc = ALWAN_CORE_LITERAL(0.95); break;
        case ALWAN_CIECAM02_SURROUND_DARK:
            *F = ALWAN_CORE_LITERAL(0.8); *c = ALWAN_CORE_LITERAL(0.525); *Nc = ALWAN_CORE_LITERAL(0.8); break;
        default:
            *F = ALWAN_CORE_LITERAL(1.0); *c = ALWAN_CORE_LITERAL(0.69); *Nc = ALWAN_CORE_LITERAL(1.0); break;
    }
}

static void ALWAN_CORE_FN(get_cam16_surround_params)(alwan_cam16_surround surround,
                                ALWAN_CORE_T *F, ALWAN_CORE_T *c, ALWAN_CORE_T *Nc) {
    switch (surround) {
        case ALWAN_CAM16_SURROUND_AVERAGE:
            *F = ALWAN_CORE_LITERAL(1.0); *c = ALWAN_CORE_LITERAL(0.69); *Nc = ALWAN_CORE_LITERAL(1.0); break;
        case ALWAN_CAM16_SURROUND_DIM:
            *F = ALWAN_CORE_LITERAL(0.9); *c = ALWAN_CORE_LITERAL(0.59); *Nc = ALWAN_CORE_LITERAL(0.95); break;
        case ALWAN_CAM16_SURROUND_DARK:
            *F = ALWAN_CORE_LITERAL(0.8); *c = ALWAN_CORE_LITERAL(0.525); *Nc = ALWAN_CORE_LITERAL(0.8); break;
        default:
            *F = ALWAN_CORE_LITERAL(1.0); *c = ALWAN_CORE_LITERAL(0.69); *Nc = ALWAN_CORE_LITERAL(1.0); break;
    }
}

/* ---- CIECAM02 forward / inverse ---- */
int ALWAN_CORE_FNLIT(alwan_ciecam02_forward)(ALWAN_CORE_CIECAM02_CORRELATES *out,
        ALWAN_CORE_XYZ const *xyz, ALWAN_CORE_CIECAM02_VIEWING_CONDITIONS const *vc) {
    if (!out || !xyz || !vc) return ALWAN_E_INVALID;
    if (vc->white_xyz.y <= ALWAN_CORE_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_E_DIVZERO;

    ALWAN_CORE_T F, c, Nc;
    ALWAN_CORE_FN(get_surround_params)(vc->surround, &F, &c, &Nc);

    ALWAN_CORE_FNLIT(alwan_cam_derived_params) p = ALWAN_CORE_FNVLIT(cam_compute_derived_params)(
        F, vc->adapting_luminance, vc->background_luminance,
        vc->white_xyz.y, (ALWAN_CORE_T)vc->discount_illuminant,
        vc->white_xyz, ALWAN_CORE_FNLIT(CAM_M_CAT02), ALWAN_CORE_FNLIT(CAM_M_CAT02_INV), ALWAN_CORE_FNLIT(CAM_M_HPE), 1);

    ALWAN_CORE_FNLIT(alwan_ciecam02_v_correlates) result = ALWAN_CORE_FNVLIT(alwan_ciecam02_forward)(
        *xyz, vc->white_xyz, F, c, Nc, p.D, p.FL, p.n, p.Nbb, p.Ncb, p.z, p.A_w);

    out->J = result.J; out->C = result.C; out->h = result.h; out->s = result.s;
    out->Q = result.Q; out->M = result.M; out->H = result.H;
    ALWAN_NORM_CIECAM02(out);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_ciecam02_inverse)(ALWAN_CORE_XYZ *xyz_out,
        ALWAN_CORE_CIECAM02_CORRELATES const *correlates, ALWAN_CORE_CIECAM02_VIEWING_CONDITIONS const *vc) {
    if (!xyz_out || !correlates || !vc) return ALWAN_E_INVALID;
    if (vc->white_xyz.y <= ALWAN_CORE_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_E_DIVZERO;

    ALWAN_CORE_CIECAM02_CORRELATES tmp = *correlates;
    ALWAN_DENORM_CIECAM02(&tmp);

    ALWAN_CORE_T F, c, Nc;
    ALWAN_CORE_FN(get_surround_params)(vc->surround, &F, &c, &Nc);

    ALWAN_CORE_FNLIT(alwan_cam_derived_params) p = ALWAN_CORE_FNVLIT(cam_compute_derived_params)(
        F, vc->adapting_luminance, vc->background_luminance,
        vc->white_xyz.y, (ALWAN_CORE_T)vc->discount_illuminant,
        vc->white_xyz, ALWAN_CORE_FNLIT(CAM_M_CAT02), ALWAN_CORE_FNLIT(CAM_M_CAT02_INV), ALWAN_CORE_FNLIT(CAM_M_HPE), 1);

    *xyz_out = ALWAN_CORE_FNVLIT(alwan_ciecam02_inverse)(
        tmp.J, tmp.C, tmp.h, vc->white_xyz, F, c, Nc, p.D, p.FL, p.n, p.Nbb, p.Ncb, p.z, p.A_w);
    return ALWAN_OK;
}

/* ---- CAM16 forward / inverse / UCS ---- */
int ALWAN_CORE_FNLIT(alwan_cam16_forward)(ALWAN_CORE_CAM16_CORRELATES *out,
        ALWAN_CORE_XYZ const *xyz, ALWAN_CORE_CAM16_VIEWING_CONDITIONS const *vc) {
    if (!out || !xyz || !vc) return ALWAN_E_INVALID;
    if (vc->white_xyz.y <= ALWAN_CORE_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_E_DIVZERO;

    ALWAN_CORE_T F, c, Nc;
    ALWAN_CORE_FN(get_cam16_surround_params)(vc->surround, &F, &c, &Nc);

    ALWAN_CORE_FNLIT(alwan_cam_derived_params) p = ALWAN_CORE_FNVLIT(cam_compute_derived_params)(
        F, vc->adapting_luminance, vc->background_luminance,
        vc->white_xyz.y, (ALWAN_CORE_T)vc->discount_illuminant,
        vc->white_xyz, ALWAN_CORE_FNLIT(CAM_M_CAT16), ALWAN_CORE_FNLIT(CAM_M_CAT16_INV), ALWAN_CORE_FNLIT(CAM_M_CAT16), 0);

    ALWAN_CORE_FNLIT(alwan_cam16_v_correlates) result = ALWAN_CORE_FNVLIT(alwan_cam16_forward)(
        *xyz, vc->white_xyz, F, c, Nc, p.D, p.FL, p.n, p.Nbb, p.Ncb, p.z, p.A_w);

    out->J = result.J; out->C = result.C; out->h = result.h; out->s = result.s;
    out->Q = result.Q; out->M = result.M; out->H = result.H;
    ALWAN_NORM_CAM16(out);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_cam16_inverse)(ALWAN_CORE_XYZ *xyz_out,
        ALWAN_CORE_CAM16_CORRELATES const *correlates, ALWAN_CORE_CAM16_VIEWING_CONDITIONS const *vc) {
    if (!xyz_out || !correlates || !vc) return ALWAN_E_INVALID;
    if (vc->white_xyz.y <= ALWAN_CORE_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_E_DIVZERO;

    ALWAN_CORE_CAM16_CORRELATES tmp = *correlates;
    ALWAN_DENORM_CAM16(&tmp);

    ALWAN_CORE_T F, c, Nc;
    ALWAN_CORE_FN(get_cam16_surround_params)(vc->surround, &F, &c, &Nc);

    ALWAN_CORE_FNLIT(alwan_cam_derived_params) p = ALWAN_CORE_FNVLIT(cam_compute_derived_params)(
        F, vc->adapting_luminance, vc->background_luminance,
        vc->white_xyz.y, (ALWAN_CORE_T)vc->discount_illuminant,
        vc->white_xyz, ALWAN_CORE_FNLIT(CAM_M_CAT16), ALWAN_CORE_FNLIT(CAM_M_CAT16_INV), ALWAN_CORE_FNLIT(CAM_M_CAT16), 0);

    *xyz_out = ALWAN_CORE_FNVLIT(alwan_cam16_inverse)(
        tmp.J, tmp.C, tmp.h, vc->white_xyz, F, c, Nc, p.D, p.FL, p.n, p.Nbb, p.Ncb, p.z, p.A_w);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_cam16_to_ucs)(ALWAN_CORE_CAM_JAB *Jab_out,
        ALWAN_CORE_CAM16_CORRELATES const *correlates) {
    if (!Jab_out || !correlates) return ALWAN_E_INVALID;
    ALWAN_CORE_CAM16_CORRELATES tmp = *correlates;
    ALWAN_DENORM_CAM16(&tmp);
    *Jab_out = ALWAN_CORE_FNVLIT(alwan_cam16_to_ucs)(tmp.J, tmp.M, tmp.h);
    ALWAN_NORM_CAM_JAB(Jab_out);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_cam16_from_ucs)(ALWAN_CORE_CAM16_CORRELATES *correlates_out,
        ALWAN_CORE_CAM_JAB const *Jab) {
    if (!correlates_out || !Jab) return ALWAN_E_INVALID;
    ALWAN_CORE_CAM_JAB tmp = *Jab;
    ALWAN_DENORM_CAM_JAB(&tmp);
    ALWAN_CORE_FNLIT(alwan_cam16_v_correlates) result = ALWAN_CORE_FNVLIT(alwan_cam16_from_ucs)(tmp.J, tmp.a, tmp.b);
    correlates_out->J = result.J; correlates_out->C = result.C; correlates_out->h = result.h;
    correlates_out->s = result.s; correlates_out->Q = result.Q; correlates_out->M = result.M; correlates_out->H = result.H;
    ALWAN_NORM_CAM16(correlates_out);
    return ALWAN_OK;
}

/* ---- CAM18sl forward / inverse ---- */
int ALWAN_CORE_FNLIT(alwan_cam18sl_forward)(ALWAN_CORE_CAM18SL_CORRELATES *out,
        ALWAN_CORE_XYZ const *xyz, ALWAN_CORE_T Y_b) {
    if (!out || !xyz) return ALWAN_E_INVALID;
    ALWAN_CORE_FNLIT(alwan_cam18sl_v_correlates) result = ALWAN_CORE_FNVLIT(alwan_cam18sl_forward)(*xyz, Y_b);
    out->Q = result.Q; out->C = result.C; out->h = result.h;
    out->M = result.M; out->a = result.a; out->b = result.b;
    ALWAN_NORM_CAM18SL(out);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_cam18sl_inverse)(ALWAN_CORE_XYZ *xyz_out,
        ALWAN_CORE_CAM18SL_CORRELATES const *correlates, ALWAN_CORE_T Y_b) {
    if (!xyz_out || !correlates) return ALWAN_E_INVALID;
    ALWAN_CORE_CAM18SL_CORRELATES tmp = *correlates;
    ALWAN_DENORM_CAM18SL(&tmp);
    ALWAN_CORE_FNLIT(alwan_cam18sl_v_correlates) v;
    v.Q = tmp.Q; v.C = tmp.C; v.h = tmp.h; v.M = tmp.M; v.a = tmp.a; v.b = tmp.b;
    *xyz_out = ALWAN_CORE_FNVLIT(alwan_cam18sl_inverse)(v, Y_b);
    return ALWAN_OK;
}

/* ---- CAM20u forward / inverse ---- */
int ALWAN_CORE_FNLIT(alwan_cam20u_forward)(ALWAN_CORE_CAM20U_CORRELATES *out,
        ALWAN_CORE_XYZ const *xyz, ALWAN_CORE_T Y_b, ALWAN_CORE_T L_a) {
    if (!out || !xyz) return ALWAN_E_INVALID;
    ALWAN_CORE_FNLIT(alwan_cam20u_v_correlates) result = ALWAN_CORE_FNVLIT(alwan_cam20u_forward)(*xyz, Y_b, L_a);
    out->Q = result.Q; out->M = result.M; out->h = result.h; out->C = result.C;
    out->s = result.s; out->a = result.a; out->b = result.b;
    ALWAN_NORM_CAM20U(out);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_cam20u_inverse)(ALWAN_CORE_XYZ *xyz_out,
        ALWAN_CORE_CAM20U_CORRELATES const *correlates, ALWAN_CORE_T Y_b, ALWAN_CORE_T L_a) {
    if (!xyz_out || !correlates) return ALWAN_E_INVALID;
    ALWAN_CORE_CAM20U_CORRELATES tmp = *correlates;
    ALWAN_DENORM_CAM20U(&tmp);
    ALWAN_CORE_FNLIT(alwan_cam20u_v_correlates) v;
    v.Q = tmp.Q; v.M = tmp.M; v.h = tmp.h; v.C = tmp.C; v.s = tmp.s; v.a = tmp.a; v.b = tmp.b;
    *xyz_out = ALWAN_CORE_FNVLIT(alwan_cam20u_inverse)(v, Y_b, L_a);
    return ALWAN_OK;
}
