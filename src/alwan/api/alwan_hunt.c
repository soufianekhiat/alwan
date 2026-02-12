/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Hunt Color Appearance Model
 * Based on: Hunt (1991, 1995)
 * "Revised colour-appearance model for related and unrelated colours"
 *
 * Implementation delegated to alwan_hunt_core.h (single source of truth).
 * Surround parameter resolution (Nc, Nb) kept here since enums
 * are not cross-platform.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_hunt_core.h"

/* ----------------------------------------------------------------
 * Helper Functions (enum resolution - not cross-platform)
 * ---------------------------------------------------------------- */

/* Resolve Hunt surround enum to Nc (chromatic induction) and Nb (brightness) */
static void get_hunt_params(alwan_hunt_surround surround,
                            alwan_scalar *Nc, alwan_scalar *Nb) {
    switch (surround) {
        case ALWAN_HUNT_SURROUND_NORMAL:
            *Nc = ALWAN_LITERAL(1.0);
            *Nb = ALWAN_LITERAL(75.0);
            break;
        case ALWAN_HUNT_SURROUND_DIM:
            *Nc = ALWAN_LITERAL(1.0);
            *Nb = ALWAN_LITERAL(25.0);
            break;
        case ALWAN_HUNT_SURROUND_DARK:
            *Nc = ALWAN_LITERAL(0.7);
            *Nb = ALWAN_LITERAL(10.0);
            break;
        default:
            *Nc = ALWAN_LITERAL(1.0);
            *Nb = ALWAN_LITERAL(75.0);
            break;
    }
}

/* ----------------------------------------------------------------
 * Hunt Forward Transform: XYZ -> Correlates
 * ---------------------------------------------------------------- */

int alwan_hunt_forward(alwan_hunt_correlates *out,
                       alwan_xyz const *xyz,
                       alwan_hunt_viewing_conditions const *vc) {
    if (!out || !xyz || !vc) {
        return -1;
    }

    /* Resolve surround enum to scalar parameters */
    alwan_scalar Nc, Nb;
    get_hunt_params(vc->surround, &Nc, &Nb);

    /* Resolve discount_illuminant flag to degree of adaptation */
    alwan_scalar D = vc->discount_illuminant ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(0.0);

    /* Delegate to core (value-returning, cross-platform) */
    alwan_hunt_v_correlates result = alwan_hunt_forward_v(
        *xyz,
        vc->xyz_w.x, vc->xyz_w.y, vc->xyz_w.z,
        vc->La, vc->Yb,
        Nc, Nb,
        D);

    /* Map core result to public struct */
    out->J = result.J;
    out->C = result.C;
    out->h = result.h;
    out->s = result.s;
    out->Q = result.Q;
    out->M = result.M;

    return 0;
}

/* Note: Hunt inverse is extremely complex and typically not implemented.
 * It requires iterative numerical methods due to the nonlinear response functions. */
