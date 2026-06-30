/* No include guard -- included twice (f32 and f64 passes) */

/* ----------------------------------------------------------------
 * Viewing Condition Resolution Helpers
 * ---------------------------------------------------------------- */

/* Get viewing condition induction factors based on background luminance Yb
 * Following CIECAM02 conventions, we derive surround from Yb relative to white */
static void ALWAN_CORE_FNLIT(kim2009_get_induction_factors)(
    ALWAN_CORE_T Yb,
    ALWAN_CORE_T Y_w,
    ALWAN_CORE_T *F,
    ALWAN_CORE_T *c,
    ALWAN_CORE_T *Nc
) {
    /* Compute relative background: n = Yb / Yw */
    ALWAN_CORE_T n = Yb / Y_w;

    /* Derive surround from background:
     * n >= 0.18 -> Average surround
     * 0.18 > n >= 0.01 -> Dim surround
     * n < 0.01 -> Dark surround
     */
    if (n >= ALWAN_CORE_LITERAL(0.18)) {
        /* Average surround */
        *F = ALWAN_CORE_LITERAL(1.0);
        *c = ALWAN_CORE_LITERAL(0.69);
        *Nc = ALWAN_CORE_LITERAL(1.0);
    } else if (n >= ALWAN_CORE_LITERAL(0.01)) {
        /* Dim surround */
        *F = ALWAN_CORE_LITERAL(0.9);
        *c = ALWAN_CORE_LITERAL(0.59);
        *Nc = ALWAN_CORE_LITERAL(0.95);
    } else {
        /* Dark surround */
        *F = ALWAN_CORE_LITERAL(0.8);
        *c = ALWAN_CORE_LITERAL(0.525);
        *Nc = ALWAN_CORE_LITERAL(0.8);
    }
}

/* Compute degree of adaptation D from F and adapting luminance */
static ALWAN_CORE_T ALWAN_CORE_FNLIT(kim2009_compute_degree_of_adaptation)(
    ALWAN_CORE_T F,
    ALWAN_CORE_T L_A,
    int discount_illuminant
) {
    if (discount_illuminant) {
        return ALWAN_CORE_LITERAL(1.0);
    }

    ALWAN_CORE_T D = F * (ALWAN_CORE_LITERAL(1.0) -
                          (ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(3.6)) *
                          ALWAN_CORE_EXP((-L_A - ALWAN_CORE_LITERAL(42.0)) / ALWAN_CORE_LITERAL(92.0)));

    /* Clamp D to [0, 1] */
    if (D < ALWAN_CORE_LITERAL(0.0)) D = ALWAN_CORE_LITERAL(0.0);
    if (D > ALWAN_CORE_LITERAL(1.0)) D = ALWAN_CORE_LITERAL(1.0);

    return D;
}

/* ----------------------------------------------------------------
 * Kim2009 Forward Transform: XYZ -> Appearance Correlates
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_kim2009_forward)(
    ALWAN_CORE_KIM2009_CORRELATES *out,
    ALWAN_CORE_XYZ const *xyz,
    ALWAN_CORE_KIM2009_VIEWING_CONDITIONS const *vc
) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Resolve viewing condition parameters */
    ALWAN_CORE_T F, c, Nc;
    ALWAN_CORE_T Y_w = vc->white_xyz.y;
    ALWAN_CORE_FNLIT(kim2009_get_induction_factors)(vc->Yb, Y_w, &F, &c, &Nc);

    ALWAN_CORE_T D = ALWAN_CORE_FNLIT(kim2009_compute_degree_of_adaptation)(F, vc->La, vc->discount_illuminant);

    /* Default media parameter E = 1.0 (CRT Displays) */
    ALWAN_CORE_T media_E = ALWAN_CORE_LITERAL(1.0);

    /* Delegate to core */
    ALWAN_CORE_FNLIT(alwan_kim2009_v_correlates) result = ALWAN_CORE_FNVLIT(alwan_kim2009_forward)(
        *xyz,
        vc->white_xyz.x, vc->white_xyz.y, vc->white_xyz.z,
        vc->La, D, media_E
    );

    out->J = result.J;
    out->C = result.C;
    out->h = result.h;

    ALWAN_NORM_KIM2009(out);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Kim2009 Inverse Transform: Appearance Correlates -> XYZ
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_kim2009_inverse)(
    ALWAN_CORE_XYZ *xyz_out,
    ALWAN_CORE_KIM2009_CORRELATES const *correlates,
    ALWAN_CORE_KIM2009_VIEWING_CONDITIONS const *vc
) {
    if (!correlates || !vc || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_KIM2009_CORRELATES tmp = *correlates;
    ALWAN_DENORM_KIM2009(&tmp);

    /* Resolve viewing condition parameters */
    ALWAN_CORE_T F, c, Nc;
    ALWAN_CORE_T Y_w = vc->white_xyz.y;
    ALWAN_CORE_FNLIT(kim2009_get_induction_factors)(vc->Yb, Y_w, &F, &c, &Nc);

    ALWAN_CORE_T D = ALWAN_CORE_FNLIT(kim2009_compute_degree_of_adaptation)(F, vc->La, vc->discount_illuminant);

    /* Default media parameter E = 1.0 (CRT Displays) */
    ALWAN_CORE_T media_E = ALWAN_CORE_LITERAL(1.0);

    /* Build core correlates struct */
    ALWAN_CORE_FNLIT(alwan_kim2009_v_correlates) v_correlates;
    v_correlates.J = tmp.J;
    v_correlates.C = tmp.C;
    v_correlates.h = tmp.h;

    /* Delegate to core */
    *xyz_out = ALWAN_CORE_FNVLIT(alwan_kim2009_inverse)(
        v_correlates,
        vc->white_xyz.x, vc->white_xyz.y, vc->white_xyz.z,
        vc->La, D, media_E
    );

    return ALWAN_OK;
}
