/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * RLAB Color Appearance Model
 * Based on: Fairchild (1993, 1996)
 * "RLAB: a color appearance space for color reproduction"
 * "Refinement of the RLAB color space"
 *
 * See alwan_rlab_core.h.
 * Enum resolution (get_sigma, get_D_factor) kept here since enums
 * are not cross-platform.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_rlab_core.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_rlab_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_rlab_impl.inc"
#include "alwan_api_teardown.h"

/* ----------------------------------------------------------------
 * Helper Functions (enum resolution - not cross-platform)
 * ---------------------------------------------------------------- */

/* Get sigma (surround parameter) based on viewing conditions */
static alwan_f64 get_sigma(alwan_rlab_surround surround) {
    switch (surround) {
        case ALWAN_RLAB_SURROUND_AVERAGE: return ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.3);
        case ALWAN_RLAB_SURROUND_DIM:     return ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.9);
        case ALWAN_RLAB_SURROUND_DARK:    return ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.5);
        default:                          return ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.3);
    }
}

/* Get D (discounting-the-illuminant factor) based on media */
static alwan_f64 get_D_factor(int D_setting) {
    /* D_setting: 0 = auto (use defaults), 1 = hard copy, 2 = soft copy, 3 = transparency */
    switch (D_setting) {
        case 1:  return ALWAN_LITERAL(1.0);  /* Hard copy images */
        case 2:  return ALWAN_LITERAL(0.0);  /* Soft copy images */
        case 3:  return ALWAN_LITERAL(0.5);  /* Projected transparencies */
        default: return ALWAN_LITERAL(1.0);  /* Auto: use 1.0 as default */
    }
}

/* ----------------------------------------------------------------
 * RLAB Forward Transform: XYZ -> Correlates
 * ---------------------------------------------------------------- */

int alwan_rlab_forward_f64(alwan_rlab_correlates_f64 *out,
                       alwan_xyz_f64 const *xyz,
                       alwan_rlab_viewing_conditions_f64 const *vc) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }

    alwan_f64 sigma = get_sigma(vc->surround);
    alwan_f64 D     = get_D_factor(vc->D_factor);

    alwan_rlab_v_correlates_f64 result = alwan_rlab_forward_f64_v(*xyz, vc->xyz_w, vc->xyz_n, sigma, D);

    out->L = result.L;
    out->a = result.a;
    out->b = result.b;
    out->h = result.h;
    out->C = result.C;
    out->s = result.s;

    ALWAN_NORM_RLAB(out);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RLAB Inverse Transform: Correlates -> XYZ
 * ---------------------------------------------------------------- */

int alwan_rlab_inverse_f64(alwan_xyz_f64 *xyz,
                       alwan_rlab_correlates_f64 const *correlates,
                       alwan_rlab_viewing_conditions_f64 const *vc) {
    if (!xyz || !correlates || !vc) {
        return ALWAN_E_INVALID;
    }

    alwan_rlab_correlates_f64 tmp = *correlates;
    ALWAN_DENORM_RLAB(&tmp);

    alwan_f64 sigma = get_sigma(vc->surround);
    alwan_f64 D     = get_D_factor(vc->D_factor);

    alwan_rlab_v_correlates_f64 vc_in;
    vc_in.L = tmp.L;
    vc_in.a = tmp.a;
    vc_in.b = tmp.b;
    vc_in.h = tmp.h;
    vc_in.C = tmp.C;
    vc_in.s = tmp.s;

    *xyz = alwan_rlab_inverse_f64_v(vc_in, vc->xyz_w, vc->xyz_n, sigma, D);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * f32 Wrappers
 *
 * The RLAB core math is implemented in f64 only. The f32 entry points
 * convert inputs up to f64, call the f64 implementation, then narrow
 * the outputs back to f32.
 * ---------------------------------------------------------------- */

static void rlab_vc_f32_to_f64(alwan_rlab_viewing_conditions_f64 *dst,
                               alwan_rlab_viewing_conditions_f32 const *src) {
    dst->xyz_w.x = (alwan_f64)src->xyz_w.x;
    dst->xyz_w.y = (alwan_f64)src->xyz_w.y;
    dst->xyz_w.z = (alwan_f64)src->xyz_w.z;
    dst->xyz_n.x = (alwan_f64)src->xyz_n.x;
    dst->xyz_n.y = (alwan_f64)src->xyz_n.y;
    dst->xyz_n.z = (alwan_f64)src->xyz_n.z;
    dst->surround = src->surround;
    dst->D_factor = src->D_factor;
}

int alwan_rlab_forward_f32(alwan_rlab_correlates_f32 *out,
                           alwan_xyz_f32 const *xyz,
                           alwan_rlab_viewing_conditions_f32 const *vc) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }
    alwan_xyz_f64 xyz64 = {(alwan_f64)xyz->x, (alwan_f64)xyz->y, (alwan_f64)xyz->z};
    alwan_rlab_viewing_conditions_f64 vc64;
    rlab_vc_f32_to_f64(&vc64, vc);
    alwan_rlab_correlates_f64 out64;
    int rc = alwan_rlab_forward_f64(&out64, &xyz64, &vc64);
    if (rc != ALWAN_OK) return rc;
    out->L = (alwan_f32)out64.L;
    out->C = (alwan_f32)out64.C;
    out->h = (alwan_f32)out64.h;
    out->s = (alwan_f32)out64.s;
    out->a = (alwan_f32)out64.a;
    out->b = (alwan_f32)out64.b;
    return ALWAN_OK;
}

int alwan_rlab_inverse_f32(alwan_xyz_f32 *xyz,
                           alwan_rlab_correlates_f32 const *correlates,
                           alwan_rlab_viewing_conditions_f32 const *vc) {
    if (!xyz || !correlates || !vc) {
        return ALWAN_E_INVALID;
    }
    alwan_rlab_correlates_f64 c64;
    c64.L = (alwan_f64)correlates->L;
    c64.C = (alwan_f64)correlates->C;
    c64.h = (alwan_f64)correlates->h;
    c64.s = (alwan_f64)correlates->s;
    c64.a = (alwan_f64)correlates->a;
    c64.b = (alwan_f64)correlates->b;
    alwan_rlab_viewing_conditions_f64 vc64;
    rlab_vc_f32_to_f64(&vc64, vc);
    alwan_xyz_f64 xyz64;
    int rc = alwan_rlab_inverse_f64(&xyz64, &c64, &vc64);
    if (rc != ALWAN_OK) return rc;
    xyz->x = (alwan_f32)xyz64.x;
    xyz->y = (alwan_f32)xyz64.y;
    xyz->z = (alwan_f32)xyz64.z;
    return ALWAN_OK;
}
