/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ATD95 Color Vision Model
 *
 * References:
 * - Guth, S. L. (1995). Further applications of the ATD model for color vision.
 * - Fairchild, M. D. (2013). Color Appearance Models (3rd ed.). Wiley.
 *
 * See alwan_atd95_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_atd95_core.h"

int alwan_atd95_forward(
    alwan_atd95_correlates *out,
    alwan_xyz const *xyz,
    alwan_atd95_viewing_conditions const *vc
) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    alwan_atd95_v_correlates v = alwan_atd95_forward_v(
        *xyz, vc->white_xyz, vc->Y_0, vc->k1, vc->k2, vc->sigma
    );

    out->H   = v.H;
    out->C   = v.C;
    out->Br  = v.Br;
    out->A_1 = v.A_1;
    out->T_1 = v.T_1;
    out->D_1 = v.D_1;
    out->A_2 = v.A_2;
    out->T_2 = v.T_2;
    out->D_2 = v.D_2;

    return ALWAN_OK;
}
