/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Kim, Weyrich and Kautz (2009) Color Appearance Model Implementation
 * Based on: Kim, M. H., Weyrich, T., & Kautz, J. (2009). Modeling Human Color Perception
 * under Extended Luminance Levels. ACM Transactions on Graphics, 28(3), 27:1-27:9.
 *
 * Resolves viewing conditions; see alwan_kim2009_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_kim2009_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_kim2009_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_kim2009_impl.inc"
#include "alwan_api_teardown.h"
#endif

/* ----------------------------------------------------------------
 * Viewing Condition Resolution Helpers
 * ---------------------------------------------------------------- */

/* Get viewing condition induction factors based on background luminance Yb
 * Following CIECAM02 conventions, we derive surround from Yb relative to white */
static void get_induction_factors(
    alwan_f64 Yb,
    alwan_f64 Y_w,
    alwan_f64 *F,
    alwan_f64 *c,
    alwan_f64 *Nc
) {
    /* Compute relative background: n = Yb / Yw */
    alwan_f64 n = Yb / Y_w;

    /* Derive surround from background:
     * n >= 0.18 -> Average surround
     * 0.18 > n >= 0.01 -> Dim surround
     * n < 0.01 -> Dark surround
     */
    if (n >= ALWAN_LITERAL(0.18)) {
        /* Average surround */
        *F = ALWAN_LITERAL(1.0);
        *c = ALWAN_LITERAL(0.69);
        *Nc = ALWAN_LITERAL(1.0);
    } else if (n >= ALWAN_LITERAL(0.01)) {
        /* Dim surround */
        *F = ALWAN_LITERAL(0.9);
        *c = ALWAN_LITERAL(0.59);
        *Nc = ALWAN_LITERAL(0.95);
    } else {
        /* Dark surround */
        *F = ALWAN_LITERAL(0.8);
        *c = ALWAN_LITERAL(0.525);
        *Nc = ALWAN_LITERAL(0.8);
    }
}

/* Compute degree of adaptation D from F and adapting luminance */
static alwan_f64 compute_degree_of_adaptation(
    alwan_f64 F,
    alwan_f64 L_A,
    int discount_illuminant
) {
    if (discount_illuminant) {
        return ALWAN_LITERAL(1.0);
    }

    alwan_f64 D = F * (ALWAN_LITERAL(1.0) -
                          (ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.6)) *
                          ALWAN_EXP((-L_A - ALWAN_LITERAL(42.0)) / ALWAN_LITERAL(92.0)));

    /* Clamp D to [0, 1] */
    if (D < ALWAN_LITERAL(0.0)) D = ALWAN_LITERAL(0.0);
    if (D > ALWAN_LITERAL(1.0)) D = ALWAN_LITERAL(1.0);

    return D;
}

/* ----------------------------------------------------------------
 * Kim2009 Forward Transform: XYZ -> Appearance Correlates
 * ---------------------------------------------------------------- */

int alwan_kim2009_forward_f64(
    alwan_kim2009_correlates_f64 *out,
    alwan_xyz_f64 const *xyz,
    alwan_kim2009_viewing_conditions_f64 const *vc
) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Resolve viewing condition parameters */
    alwan_f64 F, c, Nc;
    alwan_f64 Y_w = vc->white_xyz.y;
    get_induction_factors(vc->Yb, Y_w, &F, &c, &Nc);

    alwan_f64 D = compute_degree_of_adaptation(F, vc->La, vc->discount_illuminant);

    /* Default media parameter E = 1.0 (CRT Displays) */
    alwan_f64 media_E = ALWAN_LITERAL(1.0);

    /* Delegate to core */
    alwan_kim2009_v_correlates_f64 result = alwan_kim2009_forward_f64_v(
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

int alwan_kim2009_inverse_f64(
    alwan_xyz_f64 *xyz_out,
    alwan_kim2009_correlates_f64 const *correlates,
    alwan_kim2009_viewing_conditions_f64 const *vc
) {
    if (!correlates || !vc || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    alwan_kim2009_correlates_f64 tmp = *correlates;
    ALWAN_DENORM_KIM2009(&tmp);

    /* Resolve viewing condition parameters */
    alwan_f64 F, c, Nc;
    alwan_f64 Y_w = vc->white_xyz.y;
    get_induction_factors(vc->Yb, Y_w, &F, &c, &Nc);

    alwan_f64 D = compute_degree_of_adaptation(F, vc->La, vc->discount_illuminant);

    /* Default media parameter E = 1.0 (CRT Displays) */
    alwan_f64 media_E = ALWAN_LITERAL(1.0);

    /* Build core correlates struct */
    alwan_kim2009_v_correlates_f64 v_correlates;
    v_correlates.J = tmp.J;
    v_correlates.C = tmp.C;
    v_correlates.h = tmp.h;

    /* Delegate to core */
    *xyz_out = alwan_kim2009_inverse_f64_v(
        v_correlates,
        vc->white_xyz.x, vc->white_xyz.y, vc->white_xyz.z,
        vc->La, D, media_E
    );

    return ALWAN_OK;
}
