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

/* alwan_cam_impl.inc is intentionally empty: surround enum resolution
 * prevents parameterization via the .inc approach (enums are per-model,
 * not per-precision), so all implementations are written out below. */

/* ----------------------------------------------------------------
 * Helper Functions (enum resolution - not cross-platform)
 * ---------------------------------------------------------------- */

/* DIM Nc = 0.95 here (CIECAM02/CAM16); Hellwig2022 uses 0.9 per Li et al. 2022 — intentional spec difference */
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

int alwan_ciecam02_forward_f64(alwan_ciecam02_correlates_f64 *out,
                            alwan_xyz_f64 const *xyz,
                            alwan_ciecam02_viewing_conditions_f64 const *vc) {
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

int alwan_ciecam02_inverse_f64(alwan_xyz_f64 *xyz_out,
                            alwan_ciecam02_correlates_f64 const *correlates,
                            alwan_ciecam02_viewing_conditions_f64 const *vc) {
    if (!xyz_out || !correlates || !vc) {
        return ALWAN_E_INVALID;
    }

    if (vc->white_xyz.y <= ALWAN_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_DIVZERO;
    }

    alwan_ciecam02_correlates_f64 tmp = *correlates;
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

int alwan_cam16_forward_f64(alwan_cam16_correlates_f64 *out,
                        alwan_xyz_f64 const *xyz,
                        alwan_cam16_viewing_conditions_f64 const *vc) {
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

int alwan_cam16_inverse_f64(alwan_xyz_f64 *xyz_out,
                        alwan_cam16_correlates_f64 const *correlates,
                        alwan_cam16_viewing_conditions_f64 const *vc) {
    if (!xyz_out || !correlates || !vc) {
        return ALWAN_E_INVALID;
    }

    if (vc->white_xyz.y <= ALWAN_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_DIVZERO;
    }

    alwan_cam16_correlates_f64 tmp = *correlates;
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

int alwan_cam16_to_ucs_f64(alwan_cam_jab_f64 *Jab_out,
                       alwan_cam16_correlates_f64 const *correlates) {
    if (!Jab_out || !correlates) {
        return ALWAN_E_INVALID;
    }

    alwan_cam16_correlates_f64 tmp = *correlates;
    ALWAN_DENORM_CAM16(&tmp);
    *Jab_out = alwan_cam16_to_ucs_f64_v(tmp.J, tmp.M, tmp.h);
    ALWAN_NORM_CAM_JAB(Jab_out);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM16-UCS Inverse Transform (Jab -> JMh)
 * ---------------------------------------------------------------- */

int alwan_cam16_from_ucs_f64(alwan_cam16_correlates_f64 *correlates_out,
                         alwan_cam_jab_f64 const *Jab) {
    if (!correlates_out || !Jab) {
        return ALWAN_E_INVALID;
    }

    alwan_cam_jab_f64 tmp = *Jab;
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

int alwan_cam18sl_forward_f32(alwan_cam18sl_correlates_f32 *out,
                          alwan_xyz_f32 const *xyz,
                          alwan_f32 Y_b) {
    if (!out || !xyz) return ALWAN_E_INVALID;

    alwan_xyz_f64 xyz64 = {(alwan_f64)xyz->x, (alwan_f64)xyz->y, (alwan_f64)xyz->z};
    alwan_cam18sl_v_correlates_f64 result = alwan_cam18sl_forward_f64_v(xyz64, (alwan_f64)Y_b);

    out->Q = (alwan_f32)result.Q;
    out->C = (alwan_f32)result.C;
    out->h = (alwan_f32)result.h;
    out->M = (alwan_f32)result.M;
    out->a = (alwan_f32)result.a;
    out->b = (alwan_f32)result.b;

    ALWAN_NORM_CAM18SL(out);
    return ALWAN_OK;
}

int alwan_cam18sl_forward_f64(alwan_cam18sl_correlates_f64 *out,
                          alwan_xyz_f64 const *xyz,
                          alwan_f64 Y_b) {
    if (!out || !xyz) return ALWAN_E_INVALID;

    alwan_cam18sl_v_correlates_f64 result = alwan_cam18sl_forward_f64_v(*xyz, Y_b);

    out->Q = result.Q;
    out->C = result.C;
    out->h = result.h;
    out->M = result.M;
    out->a = result.a;
    out->b = result.b;

    ALWAN_NORM_CAM18SL(out);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM18sl Inverse Transform
 * ---------------------------------------------------------------- */

int alwan_cam18sl_inverse_f32(alwan_xyz_f32 *xyz_out,
                          alwan_cam18sl_correlates_f32 const *correlates,
                          alwan_f32 Y_b) {
    if (!xyz_out || !correlates) return ALWAN_E_INVALID;

    alwan_cam18sl_correlates_f32 tmp = *correlates;
    ALWAN_DENORM_CAM18SL(&tmp);

    alwan_cam18sl_v_correlates_f64 vc;
    vc.Q = (alwan_f64)tmp.Q;
    vc.C = (alwan_f64)tmp.C;
    vc.h = (alwan_f64)tmp.h;
    vc.M = (alwan_f64)tmp.M;
    vc.a = (alwan_f64)tmp.a;
    vc.b = (alwan_f64)tmp.b;

    alwan_xyz_f64 result = alwan_cam18sl_inverse_f64_v(vc, (alwan_f64)Y_b);
    xyz_out->x = (alwan_f32)result.x;
    xyz_out->y = (alwan_f32)result.y;
    xyz_out->z = (alwan_f32)result.z;
    return ALWAN_OK;
}

int alwan_cam18sl_inverse_f64(alwan_xyz_f64 *xyz_out,
                          alwan_cam18sl_correlates_f64 const *correlates,
                          alwan_f64 Y_b) {
    if (!xyz_out || !correlates) return ALWAN_E_INVALID;

    alwan_cam18sl_correlates_f64 tmp = *correlates;
    ALWAN_DENORM_CAM18SL(&tmp);

    alwan_cam18sl_v_correlates_f64 vc;
    vc.Q = (alwan_f64)tmp.Q;
    vc.C = (alwan_f64)tmp.C;
    vc.h = (alwan_f64)tmp.h;
    vc.M = (alwan_f64)tmp.M;
    vc.a = (alwan_f64)tmp.a;
    vc.b = (alwan_f64)tmp.b;

    *xyz_out = alwan_cam18sl_inverse_f64_v(vc, Y_b);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM20u Forward Transform
 * ---------------------------------------------------------------- */

int alwan_cam20u_forward_f32(alwan_cam20u_correlates_f32 *out,
                         alwan_xyz_f32 const *xyz,
                         alwan_f32 Y_b,
                         alwan_f32 L_a) {
    if (!out || !xyz) return ALWAN_E_INVALID;

    alwan_xyz_f64 xyz64 = {(alwan_f64)xyz->x, (alwan_f64)xyz->y, (alwan_f64)xyz->z};
    alwan_cam20u_v_correlates_f64 result = alwan_cam20u_forward_f64_v(xyz64, (alwan_f64)Y_b, (alwan_f64)L_a);

    out->Q = (alwan_f32)result.Q;
    out->M = (alwan_f32)result.M;
    out->h = (alwan_f32)result.h;
    out->C = (alwan_f32)result.C;
    out->s = (alwan_f32)result.s;
    out->a = (alwan_f32)result.a;
    out->b = (alwan_f32)result.b;

    ALWAN_NORM_CAM20U(out);
    return ALWAN_OK;
}

int alwan_cam20u_forward_f64(alwan_cam20u_correlates_f64 *out,
                         alwan_xyz_f64 const *xyz,
                         alwan_f64 Y_b,
                         alwan_f64 L_a) {
    if (!out || !xyz) return ALWAN_E_INVALID;

    alwan_cam20u_v_correlates_f64 result = alwan_cam20u_forward_f64_v(*xyz, Y_b, L_a);

    out->Q = result.Q;
    out->M = result.M;
    out->h = result.h;
    out->C = result.C;
    out->s = result.s;
    out->a = result.a;
    out->b = result.b;

    ALWAN_NORM_CAM20U(out);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM20u Inverse Transform
 * ---------------------------------------------------------------- */

int alwan_cam20u_inverse_f32(alwan_xyz_f32 *xyz_out,
                         alwan_cam20u_correlates_f32 const *correlates,
                         alwan_f32 Y_b,
                         alwan_f32 L_a) {
    if (!xyz_out || !correlates) return ALWAN_E_INVALID;

    alwan_cam20u_correlates_f32 tmp = *correlates;
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
    xyz_out->x = (alwan_f32)result.x;
    xyz_out->y = (alwan_f32)result.y;
    xyz_out->z = (alwan_f32)result.z;
    return ALWAN_OK;
}

int alwan_cam20u_inverse_f64(alwan_xyz_f64 *xyz_out,
                         alwan_cam20u_correlates_f64 const *correlates,
                         alwan_f64 Y_b,
                         alwan_f64 L_a) {
    if (!xyz_out || !correlates) return ALWAN_E_INVALID;

    alwan_cam20u_correlates_f64 tmp = *correlates;
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
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CIECAM02 f32 variants (upconvert to f64, call f64 wrapper)
 * ---------------------------------------------------------------- */

int alwan_ciecam02_forward_f32(alwan_ciecam02_correlates_f32 *out,
                                alwan_xyz_f32 const *xyz,
                                alwan_ciecam02_viewing_conditions_f32 const *vc) {
    if (!out || !xyz || !vc) return ALWAN_E_INVALID;

    alwan_xyz_f64 xyz64;
    xyz64.x = (alwan_f64)xyz->x;
    xyz64.y = (alwan_f64)xyz->y;
    xyz64.z = (alwan_f64)xyz->z;

    alwan_ciecam02_viewing_conditions_f64 vc64;
    vc64.white_xyz.x          = (alwan_f64)vc->white_xyz.x;
    vc64.white_xyz.y          = (alwan_f64)vc->white_xyz.y;
    vc64.white_xyz.z          = (alwan_f64)vc->white_xyz.z;
    vc64.adapting_luminance   = (alwan_f64)vc->adapting_luminance;
    vc64.background_luminance = (alwan_f64)vc->background_luminance;
    vc64.surround             = vc->surround;
    vc64.discount_illuminant  = vc->discount_illuminant;

    alwan_ciecam02_correlates_f64 tmp;
    int status = alwan_ciecam02_forward_f64(&tmp, &xyz64, &vc64);
    if (status != ALWAN_OK) return status;

    out->J = (alwan_f32)tmp.J;
    out->C = (alwan_f32)tmp.C;
    out->h = (alwan_f32)tmp.h;
    out->s = (alwan_f32)tmp.s;
    out->Q = (alwan_f32)tmp.Q;
    out->M = (alwan_f32)tmp.M;
    out->H = (alwan_f32)tmp.H;
    return ALWAN_OK;
}

int alwan_ciecam02_inverse_f32(alwan_xyz_f32 *xyz_out,
                                alwan_ciecam02_correlates_f32 const *correlates,
                                alwan_ciecam02_viewing_conditions_f32 const *vc) {
    if (!xyz_out || !correlates || !vc) return ALWAN_E_INVALID;

    alwan_ciecam02_correlates_f64 corr64;
    corr64.J = (alwan_f64)correlates->J;
    corr64.C = (alwan_f64)correlates->C;
    corr64.h = (alwan_f64)correlates->h;
    corr64.s = (alwan_f64)correlates->s;
    corr64.Q = (alwan_f64)correlates->Q;
    corr64.M = (alwan_f64)correlates->M;
    corr64.H = (alwan_f64)correlates->H;

    alwan_ciecam02_viewing_conditions_f64 vc64;
    vc64.white_xyz.x          = (alwan_f64)vc->white_xyz.x;
    vc64.white_xyz.y          = (alwan_f64)vc->white_xyz.y;
    vc64.white_xyz.z          = (alwan_f64)vc->white_xyz.z;
    vc64.adapting_luminance   = (alwan_f64)vc->adapting_luminance;
    vc64.background_luminance = (alwan_f64)vc->background_luminance;
    vc64.surround             = vc->surround;
    vc64.discount_illuminant  = vc->discount_illuminant;

    alwan_xyz_f64 xyz64;
    int status = alwan_ciecam02_inverse_f64(&xyz64, &corr64, &vc64);
    if (status != ALWAN_OK) return status;

    xyz_out->x = (alwan_f32)xyz64.x;
    xyz_out->y = (alwan_f32)xyz64.y;
    xyz_out->z = (alwan_f32)xyz64.z;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM16 f32 variants (upconvert to f64, call f64 wrapper)
 * ---------------------------------------------------------------- */

int alwan_cam16_forward_f32(alwan_cam16_correlates_f32 *out,
                             alwan_xyz_f32 const *xyz,
                             alwan_cam16_viewing_conditions_f32 const *vc) {
    if (!out || !xyz || !vc) return ALWAN_E_INVALID;

    alwan_xyz_f64 xyz64;
    xyz64.x = (alwan_f64)xyz->x;
    xyz64.y = (alwan_f64)xyz->y;
    xyz64.z = (alwan_f64)xyz->z;

    alwan_cam16_viewing_conditions_f64 vc64;
    vc64.white_xyz.x          = (alwan_f64)vc->white_xyz.x;
    vc64.white_xyz.y          = (alwan_f64)vc->white_xyz.y;
    vc64.white_xyz.z          = (alwan_f64)vc->white_xyz.z;
    vc64.adapting_luminance   = (alwan_f64)vc->adapting_luminance;
    vc64.background_luminance = (alwan_f64)vc->background_luminance;
    vc64.surround             = vc->surround;
    vc64.discount_illuminant  = vc->discount_illuminant;

    alwan_cam16_correlates_f64 tmp;
    int status = alwan_cam16_forward_f64(&tmp, &xyz64, &vc64);
    if (status != ALWAN_OK) return status;

    out->J = (alwan_f32)tmp.J;
    out->C = (alwan_f32)tmp.C;
    out->h = (alwan_f32)tmp.h;
    out->s = (alwan_f32)tmp.s;
    out->Q = (alwan_f32)tmp.Q;
    out->M = (alwan_f32)tmp.M;
    out->H = (alwan_f32)tmp.H;
    return ALWAN_OK;
}

int alwan_cam16_inverse_f32(alwan_xyz_f32 *xyz_out,
                             alwan_cam16_correlates_f32 const *correlates,
                             alwan_cam16_viewing_conditions_f32 const *vc) {
    if (!xyz_out || !correlates || !vc) return ALWAN_E_INVALID;

    alwan_cam16_correlates_f64 corr64;
    corr64.J = (alwan_f64)correlates->J;
    corr64.C = (alwan_f64)correlates->C;
    corr64.h = (alwan_f64)correlates->h;
    corr64.s = (alwan_f64)correlates->s;
    corr64.Q = (alwan_f64)correlates->Q;
    corr64.M = (alwan_f64)correlates->M;
    corr64.H = (alwan_f64)correlates->H;

    alwan_cam16_viewing_conditions_f64 vc64;
    vc64.white_xyz.x          = (alwan_f64)vc->white_xyz.x;
    vc64.white_xyz.y          = (alwan_f64)vc->white_xyz.y;
    vc64.white_xyz.z          = (alwan_f64)vc->white_xyz.z;
    vc64.adapting_luminance   = (alwan_f64)vc->adapting_luminance;
    vc64.background_luminance = (alwan_f64)vc->background_luminance;
    vc64.surround             = vc->surround;
    vc64.discount_illuminant  = vc->discount_illuminant;

    alwan_xyz_f64 xyz64;
    int status = alwan_cam16_inverse_f64(&xyz64, &corr64, &vc64);
    if (status != ALWAN_OK) return status;

    xyz_out->x = (alwan_f32)xyz64.x;
    xyz_out->y = (alwan_f32)xyz64.y;
    xyz_out->z = (alwan_f32)xyz64.z;
    return ALWAN_OK;
}

int alwan_cam16_to_ucs_f32(alwan_cam_jab_f32 *Jab_out,
                            alwan_cam16_correlates_f32 const *correlates) {
    if (!Jab_out || !correlates) return ALWAN_E_INVALID;

    alwan_cam16_correlates_f64 corr64;
    corr64.J = (alwan_f64)correlates->J;
    corr64.C = (alwan_f64)correlates->C;
    corr64.h = (alwan_f64)correlates->h;
    corr64.s = (alwan_f64)correlates->s;
    corr64.Q = (alwan_f64)correlates->Q;
    corr64.M = (alwan_f64)correlates->M;
    corr64.H = (alwan_f64)correlates->H;

    alwan_cam_jab_f64 jab64;
    int status = alwan_cam16_to_ucs_f64(&jab64, &corr64);
    if (status != ALWAN_OK) return status;

    Jab_out->J = (alwan_f32)jab64.J;
    Jab_out->a = (alwan_f32)jab64.a;
    Jab_out->b = (alwan_f32)jab64.b;
    return ALWAN_OK;
}

int alwan_cam16_from_ucs_f32(alwan_cam16_correlates_f32 *correlates_out,
                              alwan_cam_jab_f32 const *Jab) {
    if (!correlates_out || !Jab) return ALWAN_E_INVALID;

    alwan_cam_jab_f64 jab64;
    jab64.J = (alwan_f64)Jab->J;
    jab64.a = (alwan_f64)Jab->a;
    jab64.b = (alwan_f64)Jab->b;

    alwan_cam16_correlates_f64 tmp;
    int status = alwan_cam16_from_ucs_f64(&tmp, &jab64);
    if (status != ALWAN_OK) return status;

    correlates_out->J = (alwan_f32)tmp.J;
    correlates_out->C = (alwan_f32)tmp.C;
    correlates_out->h = (alwan_f32)tmp.h;
    correlates_out->s = (alwan_f32)tmp.s;
    correlates_out->Q = (alwan_f32)tmp.Q;
    correlates_out->M = (alwan_f32)tmp.M;
    correlates_out->H = (alwan_f32)tmp.H;
    return ALWAN_OK;
}

/* _map_interleave variants are implemented in alwan_cam_map.c (SIMD-parameterized).
 * _ex format-dispatch variants are implemented in alwan_typed_map.c.
 * Both call the single-element wrappers defined above. */
