/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Kim, Weyrich and Kautz (2009) Color Appearance Model Implementation
 * Based on: Kim, M. H., Weyrich, T., & Kautz, J. (2009). Modeling Human Color Perception
 * under Extended Luminance Levels. ACM Transactions on Graphics, 28(3), 27:1-27:9.
 *
 * Thin wrapper that resolves viewing conditions and delegates to alwan_kim2009_core.h.
 */

#include "alwan.h"
#include "alwan_internal.h"
#include "alwan_kim2009_core.h"

/* ----------------------------------------------------------------
 * Viewing Condition Resolution Helpers
 * ---------------------------------------------------------------- */

/* Get viewing condition induction factors based on background luminance Yb
 * Following CIECAM02 conventions, we derive surround from Yb relative to white */
static void get_induction_factors(
    alwan_scalar Yb,
    alwan_scalar Y_w,
    alwan_scalar *F,
    alwan_scalar *c,
    alwan_scalar *Nc
) {
    /* Compute relative background: n = Yb / Yw */
    alwan_scalar n = Yb / Y_w;

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
static alwan_scalar compute_degree_of_adaptation(
    alwan_scalar F,
    alwan_scalar L_A,
    int discount_illuminant
) {
    if (discount_illuminant) {
        return ALWAN_LITERAL(1.0);
    }

    alwan_scalar D = F * (ALWAN_LITERAL(1.0) -
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

int alwan_kim2009_forward(
    alwan_kim2009_correlates *out,
    alwan_xyz const *xyz,
    alwan_kim2009_viewing_conditions const *vc
) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Resolve viewing condition parameters */
    alwan_scalar F, c, Nc;
    alwan_scalar Y_w = vc->white_xyz.y;
    get_induction_factors(vc->Yb, Y_w, &F, &c, &Nc);

    alwan_scalar D = compute_degree_of_adaptation(F, vc->La, vc->discount_illuminant);

    /* Default media parameter E = 1.0 (CRT Displays) */
    alwan_scalar media_E = ALWAN_LITERAL(1.0);

    /* Delegate to core */
    alwan_kim2009_v_correlates result = alwan_kim2009_forward_v(
        *xyz,
        vc->white_xyz.x, vc->white_xyz.y, vc->white_xyz.z,
        vc->La, D, media_E
    );

    out->J = result.J;
    out->C = result.C;
    out->h = result.h;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Kim2009 Inverse Transform: Appearance Correlates -> XYZ
 * ---------------------------------------------------------------- */

int alwan_kim2009_inverse(
    alwan_xyz *out,
    alwan_kim2009_correlates const *correlates,
    alwan_kim2009_viewing_conditions const *vc
) {
    if (!correlates || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Resolve viewing condition parameters */
    alwan_scalar F, c, Nc;
    alwan_scalar Y_w = vc->white_xyz.y;
    get_induction_factors(vc->Yb, Y_w, &F, &c, &Nc);

    alwan_scalar D = compute_degree_of_adaptation(F, vc->La, vc->discount_illuminant);

    /* Default media parameter E = 1.0 (CRT Displays) */
    alwan_scalar media_E = ALWAN_LITERAL(1.0);

    /* Build core correlates struct */
    alwan_kim2009_v_correlates v_correlates;
    v_correlates.J = correlates->J;
    v_correlates.C = correlates->C;
    v_correlates.h = correlates->h;

    /* Delegate to core */
    *out = alwan_kim2009_inverse_v(
        v_correlates,
        vc->white_xyz.x, vc->white_xyz.y, vc->white_xyz.z,
        vc->La, D, media_E
    );

    return ALWAN_OK;
}
