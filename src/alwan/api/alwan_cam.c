/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * CIECAM02 & CAM16 Color Appearance Models
 * Thin wrapper -- logic in alwan_cam_core.h
 *
 * Only enum resolution (surround -> F/c/Nc) lives here;
 * all derived-parameter computation delegated to core.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_cam_core.h"
#include "../core/alwan_cam18sl_core.h"
#include "../core/alwan_cam20u_core.h"

/* ----------------------------------------------------------------
 * Helper Functions (enum resolution - not cross-platform)
 * ---------------------------------------------------------------- */

static void get_surround_params(alwan_ciecam02_surround surround,
                                alwan_scalar *F, alwan_scalar *c, alwan_scalar *Nc) {
    switch (surround) {
        case ALWAN_CIECAM02_SURROUND_AVERAGE:
            *F = ALWAN_LITERAL(1.0);
            *c = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            break;
        case ALWAN_CIECAM02_SURROUND_DIM:
            *F = ALWAN_LITERAL(0.9);
            *c = ALWAN_LITERAL(0.59);
            *Nc = ALWAN_LITERAL(0.95);
            break;
        case ALWAN_CIECAM02_SURROUND_DARK:
            *F = ALWAN_LITERAL(0.8);
            *c = ALWAN_LITERAL(0.525);
            *Nc = ALWAN_LITERAL(0.8);
            break;
        default:
            *F = ALWAN_LITERAL(1.0);
            *c = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            break;
    }
}

static void get_cam16_surround_params(alwan_cam16_surround surround,
                                      alwan_scalar *F, alwan_scalar *c, alwan_scalar *Nc) {
    switch (surround) {
        case ALWAN_CAM16_SURROUND_AVERAGE:
            *F = ALWAN_LITERAL(1.0);
            *c = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            break;
        case ALWAN_CAM16_SURROUND_DIM:
            *F = ALWAN_LITERAL(0.9);
            *c = ALWAN_LITERAL(0.59);
            *Nc = ALWAN_LITERAL(0.95);
            break;
        case ALWAN_CAM16_SURROUND_DARK:
            *F = ALWAN_LITERAL(0.8);
            *c = ALWAN_LITERAL(0.525);
            *Nc = ALWAN_LITERAL(0.8);
            break;
        default:
            *F = ALWAN_LITERAL(1.0);
            *c = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            break;
    }
}

/* ----------------------------------------------------------------
 * CIECAM02 Forward Transform
 * ---------------------------------------------------------------- */

int alwan_ciecam02_forward(alwan_ciecam02_correlates *out,
                            alwan_xyz const *xyz,
                            alwan_ciecam02_viewing_conditions const *vc) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }

    if (vc->white_xyz.y <= ALWAN_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_DIVZERO;
    }

    alwan_scalar F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    alwan_cam_derived_params p = cam_compute_derived_params_v(
        F, vc->adapting_luminance, vc->background_luminance,
        vc->white_xyz.y, (alwan_scalar)vc->discount_illuminant,
        vc->white_xyz, CAM_M_CAT02, CAM_M_CAT02_INV, CAM_M_HPE, 1);

    alwan_ciecam02_v_correlates result = alwan_ciecam02_forward_v(
        *xyz, vc->white_xyz,
        F, c, Nc, p.D, p.FL, p.n, p.Nbb, p.Ncb, p.z, p.A_w);

    out->J = result.J;
    out->C = result.C;
    out->h = result.h;
    out->s = result.s;
    out->Q = result.Q;
    out->M = result.M;
    out->H = result.H;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CIECAM02 Inverse Transform
 * ---------------------------------------------------------------- */

int alwan_ciecam02_inverse(alwan_xyz *xyz_out,
                            alwan_ciecam02_correlates const *correlates,
                            alwan_ciecam02_viewing_conditions const *vc) {
    if (!xyz_out || !correlates || !vc) {
        return ALWAN_E_INVALID;
    }

    if (vc->white_xyz.y <= ALWAN_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_DIVZERO;
    }

    alwan_scalar F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    alwan_cam_derived_params p = cam_compute_derived_params_v(
        F, vc->adapting_luminance, vc->background_luminance,
        vc->white_xyz.y, (alwan_scalar)vc->discount_illuminant,
        vc->white_xyz, CAM_M_CAT02, CAM_M_CAT02_INV, CAM_M_HPE, 1);

    *xyz_out = alwan_ciecam02_inverse_v(
        correlates->J, correlates->C, correlates->h,
        vc->white_xyz,
        F, c, Nc, p.D, p.FL, p.n, p.Nbb, p.Ncb, p.z, p.A_w);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM16 Forward Transform
 * ---------------------------------------------------------------- */

int alwan_cam16_forward(alwan_cam16_correlates *out,
                        alwan_xyz const *xyz,
                        alwan_cam16_viewing_conditions const *vc) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }

    if (vc->white_xyz.y <= ALWAN_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_DIVZERO;
    }

    alwan_scalar F, c, Nc;
    get_cam16_surround_params(vc->surround, &F, &c, &Nc);

    alwan_cam_derived_params p = cam_compute_derived_params_v(
        F, vc->adapting_luminance, vc->background_luminance,
        vc->white_xyz.y, (alwan_scalar)vc->discount_illuminant,
        vc->white_xyz, CAM_M_CAT16, CAM_M_CAT16_INV, CAM_M_CAT16, 0);

    alwan_cam16_v_correlates result = alwan_cam16_forward_v(
        *xyz, vc->white_xyz,
        F, c, Nc, p.D, p.FL, p.n, p.Nbb, p.Ncb, p.z, p.A_w);

    out->J = result.J;
    out->C = result.C;
    out->h = result.h;
    out->s = result.s;
    out->Q = result.Q;
    out->M = result.M;
    out->H = result.H;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM16 Inverse Transform
 * ---------------------------------------------------------------- */

int alwan_cam16_inverse(alwan_xyz *xyz_out,
                        alwan_cam16_correlates const *correlates,
                        alwan_cam16_viewing_conditions const *vc) {
    if (!xyz_out || !correlates || !vc) {
        return ALWAN_E_INVALID;
    }

    if (vc->white_xyz.y <= ALWAN_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_DIVZERO;
    }

    alwan_scalar F, c, Nc;
    get_cam16_surround_params(vc->surround, &F, &c, &Nc);

    alwan_cam_derived_params p = cam_compute_derived_params_v(
        F, vc->adapting_luminance, vc->background_luminance,
        vc->white_xyz.y, (alwan_scalar)vc->discount_illuminant,
        vc->white_xyz, CAM_M_CAT16, CAM_M_CAT16_INV, CAM_M_CAT16, 0);

    *xyz_out = alwan_cam16_inverse_v(
        correlates->J, correlates->C, correlates->h,
        vc->white_xyz,
        F, c, Nc, p.D, p.FL, p.n, p.Nbb, p.Ncb, p.z, p.A_w);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM16-UCS Forward Transform (JMh -> Jab)
 * ---------------------------------------------------------------- */

int alwan_cam16_to_ucs(alwan_cam_jab *Jab_out,
                       alwan_cam16_correlates const *correlates) {
    if (!Jab_out || !correlates) {
        return ALWAN_E_INVALID;
    }

    *Jab_out = alwan_cam16_to_ucs_v(correlates->J, correlates->M, correlates->h);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM16-UCS Inverse Transform (Jab -> JMh)
 * ---------------------------------------------------------------- */

int alwan_cam16_from_ucs(alwan_cam16_correlates *correlates_out,
                         alwan_cam_jab const *Jab) {
    if (!correlates_out || !Jab) {
        return ALWAN_E_INVALID;
    }

    alwan_cam16_v_correlates result = alwan_cam16_from_ucs_v(
        Jab->J, Jab->a, Jab->b);

    correlates_out->J = result.J;
    correlates_out->C = result.C;
    correlates_out->h = result.h;
    correlates_out->s = result.s;
    correlates_out->Q = result.Q;
    correlates_out->M = result.M;
    correlates_out->H = result.H;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM18sl Forward Transform
 * ---------------------------------------------------------------- */

int alwan_cam18sl_forward(alwan_cam18sl_correlates *out,
                           alwan_xyz const *xyz,
                           alwan_scalar Y_b) {
    if (!out || !xyz) return ALWAN_E_INVALID;

    alwan_cam18sl_v_correlates result = alwan_cam18sl_forward_v(*xyz, Y_b);

    out->Q = result.Q;
    out->C = result.C;
    out->h = result.h;
    out->M = result.M;
    out->a = result.a;
    out->b = result.b;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM18sl Inverse Transform
 * ---------------------------------------------------------------- */

int alwan_cam18sl_inverse(alwan_xyz *xyz_out,
                           alwan_cam18sl_correlates const *correlates,
                           alwan_scalar Y_b) {
    if (!xyz_out || !correlates) return ALWAN_E_INVALID;

    alwan_cam18sl_v_correlates vc;
    vc.Q = correlates->Q;
    vc.C = correlates->C;
    vc.h = correlates->h;
    vc.M = correlates->M;
    vc.a = correlates->a;
    vc.b = correlates->b;

    *xyz_out = alwan_cam18sl_inverse_v(vc, Y_b);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM20u Forward Transform
 * ---------------------------------------------------------------- */

int alwan_cam20u_forward(alwan_cam20u_correlates *out,
                          alwan_xyz const *xyz,
                          alwan_scalar Y_b,
                          alwan_scalar L_a) {
    if (!out || !xyz) return ALWAN_E_INVALID;

    alwan_cam20u_v_correlates result = alwan_cam20u_forward_v(*xyz, Y_b, L_a);

    out->Q = result.Q;
    out->M = result.M;
    out->h = result.h;
    out->C = result.C;
    out->s = result.s;
    out->a = result.a;
    out->b = result.b;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM20u Inverse Transform
 * ---------------------------------------------------------------- */

int alwan_cam20u_inverse(alwan_xyz *xyz_out,
                          alwan_cam20u_correlates const *correlates,
                          alwan_scalar Y_b,
                          alwan_scalar L_a) {
    if (!xyz_out || !correlates) return ALWAN_E_INVALID;

    alwan_cam20u_v_correlates vc;
    vc.Q = correlates->Q;
    vc.M = correlates->M;
    vc.h = correlates->h;
    vc.C = correlates->C;
    vc.s = correlates->s;
    vc.a = correlates->a;
    vc.b = correlates->b;

    *xyz_out = alwan_cam20u_inverse_v(vc, Y_b, L_a);

    return ALWAN_OK;
}
