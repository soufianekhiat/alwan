/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * CIECAM02 & CAM16 Color Appearance Models
 * Only enum resolution (surround -> F/c/Nc) lives here;
 * see alwan_cam_core.h for derived-parameter computation.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_cam_core.h"
#include "../core/alwan_cam18sl_core.h"
#include "../core/alwan_cam20u_core.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_cam_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_cam_impl.inc"
#include "alwan_api_teardown.h"

/* ----------------------------------------------------------------
 * Helper Functions (enum resolution - not cross-platform)
 * ---------------------------------------------------------------- */

static void get_surround_params(alwan_ciecam02_surround surround,
                                alwan_f64 *F, alwan_f64 *c, alwan_f64 *Nc) {
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
                                      alwan_f64 *F, alwan_f64 *c, alwan_f64 *Nc) {
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

    alwan_f64 F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    alwan_cam_derived_params_f64 p = cam_compute_derived_params_f64_v(
        F, vc->adapting_luminance, vc->background_luminance,
        vc->white_xyz.y, (alwan_f64)vc->discount_illuminant,
        vc->white_xyz, CAM_M_CAT02_f64, CAM_M_CAT02_INV_f64, CAM_M_HPE_f64, 1);

    alwan_ciecam02_v_correlates_f64 result = alwan_ciecam02_forward_f64_v(
        *xyz, vc->white_xyz,
        F, c, Nc, p.D, p.FL, p.n, p.Nbb, p.Ncb, p.z, p.A_w);

    out->J = result.J;
    out->C = result.C;
    out->h = result.h;
    out->s = result.s;
    out->Q = result.Q;
    out->M = result.M;
    out->H = result.H;

    ALWAN_NORM_CIECAM02(out);

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

    alwan_ciecam02_correlates tmp = *correlates;
    ALWAN_DENORM_CIECAM02(&tmp);

    alwan_f64 F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    alwan_cam_derived_params_f64 p = cam_compute_derived_params_f64_v(
        F, vc->adapting_luminance, vc->background_luminance,
        vc->white_xyz.y, (alwan_f64)vc->discount_illuminant,
        vc->white_xyz, CAM_M_CAT02_f64, CAM_M_CAT02_INV_f64, CAM_M_HPE_f64, 1);

    *xyz_out = alwan_ciecam02_inverse_f64_v(
        tmp.J, tmp.C, tmp.h,
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

    alwan_f64 F, c, Nc;
    get_cam16_surround_params(vc->surround, &F, &c, &Nc);

    alwan_cam_derived_params_f64 p = cam_compute_derived_params_f64_v(
        F, vc->adapting_luminance, vc->background_luminance,
        vc->white_xyz.y, (alwan_f64)vc->discount_illuminant,
        vc->white_xyz, CAM_M_CAT16_f64, CAM_M_CAT16_INV_f64, CAM_M_CAT16_f64, 0);

    alwan_cam16_v_correlates_f64 result = alwan_cam16_forward_f64_v(
        *xyz, vc->white_xyz,
        F, c, Nc, p.D, p.FL, p.n, p.Nbb, p.Ncb, p.z, p.A_w);

    out->J = result.J;
    out->C = result.C;
    out->h = result.h;
    out->s = result.s;
    out->Q = result.Q;
    out->M = result.M;
    out->H = result.H;

    ALWAN_NORM_CAM16(out);

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

    alwan_cam16_correlates tmp = *correlates;
    ALWAN_DENORM_CAM16(&tmp);

    alwan_f64 F, c, Nc;
    get_cam16_surround_params(vc->surround, &F, &c, &Nc);

    alwan_cam_derived_params_f64 p = cam_compute_derived_params_f64_v(
        F, vc->adapting_luminance, vc->background_luminance,
        vc->white_xyz.y, (alwan_f64)vc->discount_illuminant,
        vc->white_xyz, CAM_M_CAT16_f64, CAM_M_CAT16_INV_f64, CAM_M_CAT16_f64, 0);

    *xyz_out = alwan_cam16_inverse_f64_v(
        tmp.J, tmp.C, tmp.h,
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

    alwan_cam16_correlates tmp = *correlates;
    ALWAN_DENORM_CAM16(&tmp);
    *Jab_out = alwan_cam16_to_ucs_f64_v(tmp.J, tmp.M, tmp.h);
    ALWAN_NORM_CAM_JAB(Jab_out);

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

    alwan_cam_jab tmp = *Jab;
    ALWAN_DENORM_CAM_JAB(&tmp);
    alwan_cam16_v_correlates_f64 result = alwan_cam16_from_ucs_f64_v(
        tmp.J, tmp.a, tmp.b);

    correlates_out->J = result.J;
    correlates_out->C = result.C;
    correlates_out->h = result.h;
    correlates_out->s = result.s;
    correlates_out->Q = result.Q;
    correlates_out->M = result.M;
    correlates_out->H = result.H;

    ALWAN_NORM_CAM16(correlates_out);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM18sl Forward Transform
 * ---------------------------------------------------------------- */

void alwan_cam18sl_forward_f32(alwan_cam18sl_correlates *out,
                           alwan_xyz_f32 const *xyz,
                           float Y_b) {
    if (!out || !xyz) return;

    alwan_xyz_f64 xyz64 = {(alwan_f64)xyz->x, (alwan_f64)xyz->y, (alwan_f64)xyz->z};
    alwan_cam18sl_v_correlates_f64 result = alwan_cam18sl_forward_f64_v(xyz64, (alwan_f64)Y_b);

    out->Q = (alwan_scalar)result.Q;
    out->C = (alwan_scalar)result.C;
    out->h = (alwan_scalar)result.h;
    out->M = (alwan_scalar)result.M;
    out->a = (alwan_scalar)result.a;
    out->b = (alwan_scalar)result.b;

    ALWAN_NORM_CAM18SL(out);
}

void alwan_cam18sl_forward_f64(alwan_cam18sl_correlates *out,
                           alwan_xyz_f64 const *xyz,
                           double Y_b) {
    if (!out || !xyz) return;

    alwan_cam18sl_v_correlates_f64 result = alwan_cam18sl_forward_f64_v(*xyz, Y_b);

    out->Q = (alwan_scalar)result.Q;
    out->C = (alwan_scalar)result.C;
    out->h = (alwan_scalar)result.h;
    out->M = (alwan_scalar)result.M;
    out->a = (alwan_scalar)result.a;
    out->b = (alwan_scalar)result.b;

    ALWAN_NORM_CAM18SL(out);
}

/* ----------------------------------------------------------------
 * CAM18sl Inverse Transform
 * ---------------------------------------------------------------- */

void alwan_cam18sl_inverse_f32(alwan_xyz_f32 *xyz_out,
                           alwan_cam18sl_correlates const *correlates,
                           float Y_b) {
    if (!xyz_out || !correlates) return;

    alwan_cam18sl_correlates tmp = *correlates;
    ALWAN_DENORM_CAM18SL(&tmp);

    alwan_cam18sl_v_correlates_f64 vc;
    vc.Q = (alwan_f64)tmp.Q;
    vc.C = (alwan_f64)tmp.C;
    vc.h = (alwan_f64)tmp.h;
    vc.M = (alwan_f64)tmp.M;
    vc.a = (alwan_f64)tmp.a;
    vc.b = (alwan_f64)tmp.b;

    alwan_xyz_f64 result = alwan_cam18sl_inverse_f64_v(vc, (alwan_f64)Y_b);
    xyz_out->x = (float)result.x;
    xyz_out->y = (float)result.y;
    xyz_out->z = (float)result.z;
}

void alwan_cam18sl_inverse_f64(alwan_xyz_f64 *xyz_out,
                           alwan_cam18sl_correlates const *correlates,
                           double Y_b) {
    if (!xyz_out || !correlates) return;

    alwan_cam18sl_correlates tmp = *correlates;
    ALWAN_DENORM_CAM18SL(&tmp);

    alwan_cam18sl_v_correlates_f64 vc;
    vc.Q = (alwan_f64)tmp.Q;
    vc.C = (alwan_f64)tmp.C;
    vc.h = (alwan_f64)tmp.h;
    vc.M = (alwan_f64)tmp.M;
    vc.a = (alwan_f64)tmp.a;
    vc.b = (alwan_f64)tmp.b;

    *xyz_out = alwan_cam18sl_inverse_f64_v(vc, Y_b);
}

/* ----------------------------------------------------------------
 * CAM20u Forward Transform
 * ---------------------------------------------------------------- */

void alwan_cam20u_forward_f32(alwan_cam20u_correlates *out,
                          alwan_xyz_f32 const *xyz,
                          float Y_b,
                          float L_a) {
    if (!out || !xyz) return;

    alwan_xyz_f64 xyz64 = {(alwan_f64)xyz->x, (alwan_f64)xyz->y, (alwan_f64)xyz->z};
    alwan_cam20u_v_correlates_f64 result = alwan_cam20u_forward_f64_v(xyz64, (alwan_f64)Y_b, (alwan_f64)L_a);

    out->Q = (alwan_scalar)result.Q;
    out->M = (alwan_scalar)result.M;
    out->h = (alwan_scalar)result.h;
    out->C = (alwan_scalar)result.C;
    out->s = (alwan_scalar)result.s;
    out->a = (alwan_scalar)result.a;
    out->b = (alwan_scalar)result.b;

    ALWAN_NORM_CAM20U(out);
}

void alwan_cam20u_forward_f64(alwan_cam20u_correlates *out,
                          alwan_xyz_f64 const *xyz,
                          double Y_b,
                          double L_a) {
    if (!out || !xyz) return;

    alwan_cam20u_v_correlates_f64 result = alwan_cam20u_forward_f64_v(*xyz, Y_b, L_a);

    out->Q = (alwan_scalar)result.Q;
    out->M = (alwan_scalar)result.M;
    out->h = (alwan_scalar)result.h;
    out->C = (alwan_scalar)result.C;
    out->s = (alwan_scalar)result.s;
    out->a = (alwan_scalar)result.a;
    out->b = (alwan_scalar)result.b;

    ALWAN_NORM_CAM20U(out);
}

/* ----------------------------------------------------------------
 * CAM20u Inverse Transform
 * ---------------------------------------------------------------- */

void alwan_cam20u_inverse_f32(alwan_xyz_f32 *xyz_out,
                          alwan_cam20u_correlates const *correlates,
                          float Y_b,
                          float L_a) {
    if (!xyz_out || !correlates) return;

    alwan_cam20u_correlates tmp = *correlates;
    ALWAN_DENORM_CAM20U(&tmp);

    alwan_cam20u_v_correlates_f64 vc;
    vc.Q = (alwan_f64)tmp.Q;
    vc.M = (alwan_f64)tmp.M;
    vc.h = (alwan_f64)tmp.h;
    vc.C = (alwan_f64)tmp.C;
    vc.s = (alwan_f64)tmp.s;
    vc.a = (alwan_f64)tmp.a;
    vc.b = (alwan_f64)tmp.b;

    alwan_xyz_f64 result = alwan_cam20u_inverse_f64_v(vc, (alwan_f64)Y_b, (alwan_f64)L_a);
    xyz_out->x = (float)result.x;
    xyz_out->y = (float)result.y;
    xyz_out->z = (float)result.z;
}

void alwan_cam20u_inverse_f64(alwan_xyz_f64 *xyz_out,
                          alwan_cam20u_correlates const *correlates,
                          double Y_b,
                          double L_a) {
    if (!xyz_out || !correlates) return;

    alwan_cam20u_correlates tmp = *correlates;
    ALWAN_DENORM_CAM20U(&tmp);

    alwan_cam20u_v_correlates_f64 vc;
    vc.Q = (alwan_f64)tmp.Q;
    vc.M = (alwan_f64)tmp.M;
    vc.h = (alwan_f64)tmp.h;
    vc.C = (alwan_f64)tmp.C;
    vc.s = (alwan_f64)tmp.s;
    vc.a = (alwan_f64)tmp.a;
    vc.b = (alwan_f64)tmp.b;

    *xyz_out = alwan_cam20u_inverse_f64_v(vc, Y_b, L_a);
}
