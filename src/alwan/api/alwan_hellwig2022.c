/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Hellwig2022 Color Appearance Model Implementation
 * Based on Hellwig and Fairchild (2022)
 * "Predicting lightness, chroma, and hue using IAM and CAM frameworks"
 *
 * Thin wrapper: resolves surround enum, delegates to alwan_hellwig2022_core.h
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_hellwig2022_core.h"

/* ----------------------------------------------------------------
 * Surround enum -> F, c, Nc resolution (kept in .c wrapper)
 * ---------------------------------------------------------------- */

static void get_surround_params(alwan_hellwig2022_surround surround,
                                alwan_scalar *F, alwan_scalar *c, alwan_scalar *Nc) {
    switch (surround) {
        case ALWAN_HELLWIG2022_SURROUND_AVERAGE:
            *F  = ALWAN_LITERAL(1.0);
            *c  = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            break;
        case ALWAN_HELLWIG2022_SURROUND_DIM:
            *F  = ALWAN_LITERAL(0.9);
            *c  = ALWAN_LITERAL(0.59);
            *Nc = ALWAN_LITERAL(0.9);
            break;
        case ALWAN_HELLWIG2022_SURROUND_DARK:
            *F  = ALWAN_LITERAL(0.8);
            *c  = ALWAN_LITERAL(0.525);
            *Nc = ALWAN_LITERAL(0.8);
            break;
        default:
            *F  = ALWAN_LITERAL(1.0);
            *c  = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            break;
    }
}

/* ----------------------------------------------------------------
 * Hellwig2022 Forward Transform
 * ---------------------------------------------------------------- */

int alwan_hellwig2022_forward(alwan_hellwig2022_correlates *out,
                               alwan_xyz const *xyz,
                               alwan_hellwig2022_viewing_conditions const *vc) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Resolve surround enum to scalar parameters */
    alwan_scalar F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    /* Delegate to the core value-returning function */
    alwan_hellwig2022_v_correlates v = alwan_hellwig2022_forward_v(
        *xyz,
        vc->white_xyz.x, vc->white_xyz.y, vc->white_xyz.z,
        F, c, Nc,
        vc->adapting_luminance,
        vc->background_luminance,
        vc->white_xyz.y,
        (alwan_scalar)vc->discount_illuminant);

    /* Copy correlates to output */
    out->J = v.J;
    out->C = v.C;
    out->h = v.h;
    out->s = v.s;
    out->Q = v.Q;
    out->M = v.M;
    out->H = v.H;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Hellwig2022 Inverse Transform
 * ---------------------------------------------------------------- */

int alwan_hellwig2022_inverse(alwan_xyz *xyz_out,
                               alwan_hellwig2022_correlates const *correlates,
                               alwan_hellwig2022_viewing_conditions const *vc) {
    if (!correlates || !vc || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    /* Resolve surround enum to scalar parameters */
    alwan_scalar F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    /* Build core correlates from public correlates (only J, C, h needed for inverse) */
    alwan_hellwig2022_v_correlates v_corr;
    v_corr.J = correlates->J;
    v_corr.C = correlates->C;
    v_corr.h = correlates->h;
    v_corr.s = ALWAN_LITERAL(0.0);
    v_corr.Q = ALWAN_LITERAL(0.0);
    v_corr.M = ALWAN_LITERAL(0.0);
    v_corr.H = ALWAN_LITERAL(0.0);

    /* Delegate to the core value-returning function */
    *xyz_out = alwan_hellwig2022_inverse_v(
        v_corr,
        vc->white_xyz.x, vc->white_xyz.y, vc->white_xyz.z,
        F, c, Nc,
        vc->adapting_luminance,
        vc->background_luminance,
        vc->white_xyz.y,
        (alwan_scalar)vc->discount_illuminant);

    return ALWAN_OK;
}
