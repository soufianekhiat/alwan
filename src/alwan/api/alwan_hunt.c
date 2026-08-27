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

/* ----------------------------------------------------------------
 * Hunt Forward Transform: XYZ -> Correlates
 * ---------------------------------------------------------------- */

alwan_status alwan_hunt_forward_f64(alwan_hunt_correlates_f64 *out,
                       alwan_xyz_f64 const *xyz,
                       alwan_hunt_viewing_conditions_f64 const *vc) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }



    /* Delegate to core (value-returning, cross-platform) */
    alwan_hunt_v_correlates_f64 result = alwan_hunt_forward_f64_v(*xyz, *vc);

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
alwan_status alwan_hunt_forward_f32(alwan_hunt_correlates_f32 *out,
                       alwan_xyz_f32 const *xyz,
                       alwan_hunt_viewing_conditions_f32 const *vc) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }


    /* Delegate to the native f32 core (value-returning, cross-platform).
     * The whole viewing-conditions struct goes through now: Hunt needs the
     * background, the proximal field, the scotopic terms and the induction
     * factors, none of which fit in a scalar argument list. */
    alwan_hunt_v_correlates_f32 result = alwan_hunt_forward_f32_v(*xyz, *vc);

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
