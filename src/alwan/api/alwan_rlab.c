/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * RLAB Color Appearance Model
 * Based on: Fairchild (1993, 1996)
 * "RLAB: a color appearance space for color reproduction"
 * "Refinement of the RLAB color space"
 *
 * See alwan_rlab_core.h.
 * Enum resolution (get_sigma, get_D_factor) kept here since enums
 * are not cross-platform.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_rlab_core.h"

/* ----------------------------------------------------------------
 * Helper Functions (enum resolution - not cross-platform)
 * ---------------------------------------------------------------- */

/* Get sigma (surround parameter) based on viewing conditions */
static alwan_scalar get_sigma(alwan_rlab_surround surround) {
    switch (surround) {
        case ALWAN_RLAB_SURROUND_AVERAGE: return ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.3);
        case ALWAN_RLAB_SURROUND_DIM:     return ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.9);
        case ALWAN_RLAB_SURROUND_DARK:    return ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.5);
        default:                          return ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.3);
    }
}

/* Get D (discounting-the-illuminant factor) based on media */
static alwan_scalar get_D_factor(int D_setting) {
    /* D_setting: 0 = auto (use defaults), 1 = hard copy, 2 = soft copy, 3 = transparency */
    switch (D_setting) {
        case 1:  return ALWAN_LITERAL(1.0);  /* Hard copy images */
        case 2:  return ALWAN_LITERAL(0.0);  /* Soft copy images */
        case 3:  return ALWAN_LITERAL(0.5);  /* Projected transparencies */
        default: return ALWAN_LITERAL(1.0);  /* Auto: use 1.0 as default */
    }
}

/* ----------------------------------------------------------------
 * RLAB Forward Transform: XYZ -> Correlates
 * ---------------------------------------------------------------- */

int alwan_rlab_forward(alwan_rlab_correlates *out,
                       alwan_xyz const *xyz,
                       alwan_rlab_viewing_conditions const *vc) {
    if (!out || !xyz || !vc) {
        return -1;
    }

    alwan_scalar sigma = get_sigma(vc->surround);
    alwan_scalar D     = get_D_factor(vc->D_factor);

    alwan_rlab_v_correlates result = alwan_rlab_forward_v(*xyz, vc->xyz_w, vc->xyz_n, sigma, D);

    out->L = result.L;
    out->a = result.a;
    out->b = result.b;
    out->h = result.h;
    out->C = result.C;
    out->s = result.s;

    return 0;
}

/* ----------------------------------------------------------------
 * RLAB Inverse Transform: Correlates -> XYZ
 * ---------------------------------------------------------------- */

int alwan_rlab_inverse(alwan_xyz *xyz,
                       alwan_rlab_correlates const *correlates,
                       alwan_rlab_viewing_conditions const *vc) {
    if (!xyz || !correlates || !vc) {
        return -1;
    }

    alwan_scalar sigma = get_sigma(vc->surround);
    alwan_scalar D     = get_D_factor(vc->D_factor);

    alwan_rlab_v_correlates vc_in;
    vc_in.L = correlates->L;
    vc_in.a = correlates->a;
    vc_in.b = correlates->b;
    vc_in.h = correlates->h;
    vc_in.C = correlates->C;
    vc_in.s = correlates->s;

    *xyz = alwan_rlab_inverse_v(vc_in, vc->xyz_w, vc->xyz_n, sigma, D);

    return 0;
}
