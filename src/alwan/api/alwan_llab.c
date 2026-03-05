/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * LLAB Color Appearance Model
 * Based on Luo, Lo and Kuo (1996)
 * "The LLAB(l:c) colour model"
 *
 * References:
 * - Luo, M. R., Lo, M.-C., & Kuo, W.-G. (1996). The LLAB(l:c) colour model.
 *   Color Research & Application, 21(6), 412-429.
 * - Fairchild, M. D. (2013). Color Appearance Models (3rd ed.). Wiley.
 *
 * See alwan_llab_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_llab_core.h"

/* ----------------------------------------------------------------
 * LLAB Surround Induction Factors
 *
 * Enums are not cross-platform (HLSL/Halide), so surround
 * resolution lives here in the C wrapper.
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_scalar D;   /* Discounting-the-Illuminant factor */
    alwan_scalar F_S; /* Surround induction factor */
    alwan_scalar F_L; /* Lightness induction factor */
    alwan_scalar F_C; /* Chroma induction factor (unused by core) */
} llab_induction_factors;

static llab_induction_factors const LLAB_SURROUND_FACTORS[] = {
    /* AVERAGE: Reference samples > 4 degrees */
    {ALWAN_LITERAL(1.0), ALWAN_LITERAL(3.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)},
    /* DIM: Television/VDU displays */
    {ALWAN_LITERAL(0.7), ALWAN_LITERAL(3.5), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},
    /* DARK: 35mm projection transparencies */
    {ALWAN_LITERAL(0.7), ALWAN_LITERAL(4.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)}
};

/* ----------------------------------------------------------------
 * LLAB Forward Transform
 * ---------------------------------------------------------------- */

int alwan_llab_forward(
    alwan_llab_correlates *out,
    alwan_xyz const *xyz,
    alwan_llab_viewing_conditions const *vc
) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }

    /* Resolve induction factors from surround enum */
    llab_induction_factors const *factors = &LLAB_SURROUND_FACTORS[vc->surround];
    alwan_scalar D   = factors->D;
    alwan_scalar F_S = factors->F_S;
    alwan_scalar F_L = factors->F_L;

    /* Override D if user specified a non-negative value */
    if (vc->D_factor >= 0) {
        D = (alwan_scalar)vc->D_factor;
    }

    /* Delegate to the core value-returning implementation */
    alwan_llab_v_correlates v = alwan_llab_forward_v(
        *xyz, vc->xyz_0, vc->xyz_r, vc->Y_b, D, F_S, F_L);

    /* Map core result to public struct */
    out->L  = v.L;
    out->Ch = v.Ch;
    out->h  = v.h;
    out->s  = v.s;

    return ALWAN_OK;
}
