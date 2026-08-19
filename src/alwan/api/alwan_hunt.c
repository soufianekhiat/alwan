/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Hunt Color Appearance Model
 * Based on: Hunt (1991, 1995)
 * "Revised colour-appearance model for related and unrelated colours"
 *
 * See alwan_hunt_core.h.
 * Surround parameter resolution (Nc, Nb) kept here since enums
 * are not cross-platform.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_hunt_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_hunt_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_hunt_impl.inc"
#include "alwan_api_teardown.h"
#endif

/* ----------------------------------------------------------------
 * Helper Functions (enum resolution - not cross-platform)
 * ---------------------------------------------------------------- */

/* Resolve Hunt surround enum to Nc (chromatic induction) and Nb (brightness) */
static void get_hunt_params(alwan_hunt_surround surround,
                            alwan_f64 *Nc, alwan_f64 *Nb) {
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

int alwan_hunt_forward_f64(alwan_hunt_correlates_f64 *out,
                       alwan_xyz_f64 const *xyz,
                       alwan_hunt_viewing_conditions_f64 const *vc) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }

    /* Resolve surround enum to scalar parameters */
    alwan_f64 Nc, Nb;
    get_hunt_params(vc->surround, &Nc, &Nb);

    /* Resolve discount_illuminant flag to degree of adaptation */
    alwan_f64 D = vc->discount_illuminant ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(0.0);

    /* Delegate to core (value-returning, cross-platform) */
    alwan_hunt_v_correlates_f64 result = alwan_hunt_forward_f64_v(
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

    ALWAN_NORM_HUNT(out);

    return ALWAN_OK;
}

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
int alwan_hunt_forward_f32(alwan_hunt_correlates_f32 *out,
                       alwan_xyz_f32 const *xyz,
                       alwan_hunt_viewing_conditions_f32 const *vc) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }

    /* Resolve surround enum to scalar parameters */
    alwan_f64 Nc, Nb;
    get_hunt_params(vc->surround, &Nc, &Nb);

    /* Resolve discount_illuminant flag to degree of adaptation */
    alwan_f32 D = vc->discount_illuminant ? 1.0f : 0.0f;

    /* Delegate to the native f32 core (value-returning, cross-platform) */
    alwan_hunt_v_correlates_f32 result = alwan_hunt_forward_f32_v(
        *xyz,
        vc->xyz_w.x, vc->xyz_w.y, vc->xyz_w.z,
        vc->La, vc->Yb,
        (alwan_f32)Nc, (alwan_f32)Nb,
        D);

    /* Map core result to public struct */
    out->J = result.J;
    out->C = result.C;
    out->h = result.h;
    out->s = result.s;
    out->Q = result.Q;
    out->M = result.M;

    ALWAN_NORM_HUNT(out);

    return ALWAN_OK;
}
ALWAN_DIAG_POP
#endif /* ALWAN_WITH_F32 */

/* Note: Hunt inverse is extremely complex and typically not implemented.
 * It requires iterative numerical methods due to the nonlinear response functions. */
