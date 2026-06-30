/* No include guard -- included twice (f32 and f64 passes) */

/* ----------------------------------------------------------------
 * LLAB Surround Induction Factors
 *
 * Enums are not cross-platform (HLSL/Halide), so surround
 * resolution lives here in the templated API wrapper.
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T D;   /* Discounting-the-Illuminant factor */
    ALWAN_CORE_T F_S; /* Surround induction factor */
    ALWAN_CORE_T F_L; /* Lightness induction factor */
    ALWAN_CORE_T F_C; /* Chroma induction factor (unused by core) */
} ALWAN_CORE_FNLIT(llab_induction_factors);

static ALWAN_CORE_FNLIT(llab_induction_factors) const ALWAN_CORE_FNLIT(LLAB_SURROUND_FACTORS)[] = {
    /* AVERAGE: Reference samples > 4 degrees */
    {ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(3.0), ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0)},
    /* DIM: Television/VDU displays */
    {ALWAN_CORE_LITERAL(0.7), ALWAN_CORE_LITERAL(3.5), ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0)},
    /* DARK: 35mm projection transparencies */
    {ALWAN_CORE_LITERAL(0.7), ALWAN_CORE_LITERAL(4.0), ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0)}
};

/* ----------------------------------------------------------------
 * LLAB Forward Transform
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_llab_forward)(
    ALWAN_CORE_LLAB_CORRELATES *out,
    ALWAN_CORE_XYZ const *xyz,
    ALWAN_CORE_LLAB_VIEWING_CONDITIONS const *vc
) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }

    /* Resolve induction factors from surround enum */
    ALWAN_CORE_FNLIT(llab_induction_factors) const *factors = &ALWAN_CORE_FNLIT(LLAB_SURROUND_FACTORS)[vc->surround];
    ALWAN_CORE_T D   = factors->D;
    ALWAN_CORE_T F_S = factors->F_S;
    ALWAN_CORE_T F_L = factors->F_L;

    /* Override D if user specified a non-negative value */
    if (vc->D_factor >= 0) {
        D = (ALWAN_CORE_T)vc->D_factor;
    }

    /* Delegate to the core value-returning implementation */
    ALWAN_CORE_FNLIT(alwan_llab_v_correlates) v = ALWAN_CORE_FNVLIT(alwan_llab_forward)(
        *xyz, vc->xyz_0, vc->xyz_r, vc->Y_b, D, F_S, F_L);

    /* Map core result to public struct */
    out->L  = v.L;
    out->Ch = v.Ch;
    out->h  = v.h;
    out->s  = v.s;

    ALWAN_NORM_LLAB(out);

    return ALWAN_OK;
}
