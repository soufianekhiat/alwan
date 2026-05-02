/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Hellwig2022 Color Appearance Model Implementation
 * Based on Hellwig and Fairchild (2022)
 * "Predicting lightness, chroma, and hue using IAM and CAM frameworks"
 *
 * Resolves surround enum; see alwan_hellwig2022_core.h
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_hellwig2022_core.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_hellwig2022_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_hellwig2022_impl.inc"
#include "alwan_api_teardown.h"

/* ----------------------------------------------------------------
 * Surround enum -> F, c, Nc resolution (kept in .c wrapper)
 * ---------------------------------------------------------------- */

/* DIM Nc = 0.9 here (Hellwig2022); CIECAM02/CAM16 uses 0.95 per Li et al. 2022 — intentional spec difference */
static void get_surround_params(alwan_hellwig2022_surround surround,
                                alwan_f64 *F, alwan_f64 *c, alwan_f64 *Nc) {
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

int alwan_hellwig2022_forward_f64(alwan_hellwig2022_correlates_f64 *out,
                               alwan_xyz_f64 const *xyz,
                               alwan_hellwig2022_viewing_conditions_f64 const *vc) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Resolve surround enum to scalar parameters */
    alwan_f64 F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    /* Delegate to the core value-returning function */
    alwan_hellwig2022_v_correlates_f64 v = alwan_hellwig2022_forward_f64_v(
        *xyz,
        vc->white_xyz.x, vc->white_xyz.y, vc->white_xyz.z,
        F, c, Nc,
        vc->adapting_luminance,
        vc->background_luminance,
        vc->white_xyz.y,
        (alwan_f64)vc->discount_illuminant);

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

int alwan_hellwig2022_inverse_f64(alwan_xyz_f64 *xyz_out,
                               alwan_hellwig2022_correlates_f64 const *correlates,
                               alwan_hellwig2022_viewing_conditions_f64 const *vc) {
    if (!correlates || !vc || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    alwan_hellwig2022_correlates_f64 tmp = *correlates;
    ALWAN_DENORM_HELLWIG2022(&tmp);

    /* Resolve surround enum to scalar parameters */
    alwan_f64 F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    /* Build core correlates from public correlates (only J, C, h needed for inverse) */
    alwan_hellwig2022_v_correlates_f64 v_corr;
    v_corr.J = tmp.J;
    v_corr.C = tmp.C;
    v_corr.h = tmp.h;
    v_corr.s = ALWAN_LITERAL(0.0);
    v_corr.Q = ALWAN_LITERAL(0.0);
    v_corr.M = ALWAN_LITERAL(0.0);
    v_corr.H = ALWAN_LITERAL(0.0);

    /* Delegate to the core value-returning function */
    *xyz_out = alwan_hellwig2022_inverse_f64_v(
        v_corr,
        vc->white_xyz.x, vc->white_xyz.y, vc->white_xyz.z,
        F, c, Nc,
        vc->adapting_luminance,
        vc->background_luminance,
        vc->white_xyz.y,
        (alwan_f64)vc->discount_illuminant);

    return ALWAN_OK;
}
