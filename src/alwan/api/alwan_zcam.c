/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ZCAM (Z CAM) - HDR Color Appearance Model
 * Based on: Safdar et al. (2021) "ZCAM, a colour appearance model based on
 * a high dynamic range uniform colour space"
 * Reference: Optics Express 29(4), 6036-6052
 *
 * See alwan_zcam_core.h.
 *
 * NOTE: the f32 entry points intentionally compute in f64 internally
 * (widen -> f64 core -> narrow). ZCAM is an HDR model whose inverse
 * (PQ-domain) is numerically ill-conditioned: a 1-ULP f32 difference in
 * the correlates amplifies catastrophically through the inverse, so a
 * genuinely native-f32 inverse fails to round-trip. Keeping the core in
 * f64 preserves correctness. See test 93_zcam_f32.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_zcam_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_zcam_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE */
#if ALWAN_WITH_F64_FACADE
#include "alwan_api_f64_setup.h"
#include "alwan_zcam_impl.inc"
#include "alwan_api_teardown.h"
#endif /* ALWAN_WITH_F64_FACADE */

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE */
#if ALWAN_WITH_F64_FACADE
/* ----------------------------------------------------------------
 * Surround Enum Resolution (kept in the .c wrapper)
 * ---------------------------------------------------------------- */

static void get_zcam_surround_params(alwan_zcam_surround surround,
                                     alwan_f64 *Fs, alwan_f64 *c,
                                     alwan_f64 *Nc, alwan_f64 *F) {
    switch (surround) {
        case ALWAN_ZCAM_SURROUND_AVERAGE:
            *Fs = ALWAN_LITERAL(1.0);
            *c = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            *F = ALWAN_LITERAL(1.0);
            break;
        case ALWAN_ZCAM_SURROUND_DIM:
            *Fs = ALWAN_LITERAL(0.9);
            *c = ALWAN_LITERAL(0.59);
            *Nc = ALWAN_LITERAL(0.9);
            *F = ALWAN_LITERAL(0.9);
            break;
        case ALWAN_ZCAM_SURROUND_DARK:
            *Fs = ALWAN_LITERAL(0.8);
            *c = ALWAN_LITERAL(0.525);
            *Nc = ALWAN_LITERAL(0.8);
            *F = ALWAN_LITERAL(0.8);
            break;
        default:
            *Fs = ALWAN_LITERAL(1.0);
            *c = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            *F = ALWAN_LITERAL(1.0);
            break;
    }
}

/* ----------------------------------------------------------------
 * ZCAM Forward Transform: XYZ -> Correlates
 * ---------------------------------------------------------------- */

alwan_status alwan_zcam_forward_f64(alwan_zcam_correlates_f64 *out,
                       alwan_xyz_f64 const *xyz,
                       alwan_zcam_viewing_conditions_f64 const *vc) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Resolve surround enum to scalar parameters */
    alwan_f64 Fs, c, Nc, F;
    get_zcam_surround_params(vc->surround, &Fs, &c, &Nc, &F);

    /* Delegate to value-returning core */
    alwan_zcam_v_correlates_f64 v = alwan_zcam_forward_f64_v(
        *xyz, vc->xyz_w,
        Fs, c, Nc, F,
        vc->La, vc->Yb, vc->xyz_w.y);

    /* Copy correlates to public struct */
    out->Jz = v.Jz;
    out->Cz = v.Cz;
    out->hz = v.hz;
    out->Qz = v.Qz;
    out->Mz = v.Mz;
    out->Sz = v.Sz;
    out->Vz = v.Vz;
    out->Kz = v.Kz;
    out->Wz = v.Wz;

    ALWAN_NORM_ZCAM(out);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ZCAM Inverse Transform: Correlates -> XYZ
 * ---------------------------------------------------------------- */

alwan_status alwan_zcam_inverse_f64(alwan_xyz_f64 *xyz,
                       alwan_zcam_correlates_f64 const *correlates,
                       alwan_zcam_viewing_conditions_f64 const *vc) {
    if (!correlates || !vc || !xyz) {
        return ALWAN_E_INVALID;
    }

    alwan_zcam_correlates_f64 tmp = *correlates;
    ALWAN_DENORM_ZCAM(&tmp);

    /* Resolve surround enum to scalar parameters */
    alwan_f64 Fs, c, Nc, F;
    get_zcam_surround_params(vc->surround, &Fs, &c, &Nc, &F);

    /* Build value-type correlates from public struct */
    alwan_zcam_v_correlates_f64 v;
    v.Jz = tmp.Jz;
    v.Cz = tmp.Cz;
    v.hz = tmp.hz;
    v.Qz = tmp.Qz;
    v.Mz = tmp.Mz;
    v.Sz = tmp.Sz;
    v.Vz = tmp.Vz;
    v.Kz = tmp.Kz;
    v.Wz = tmp.Wz;

    /* Delegate to value-returning core */
    *xyz = alwan_zcam_inverse_f64_v(
        v, vc->xyz_w,
        Fs, c, Nc, F,
        vc->La, vc->Yb, vc->xyz_w.y);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ZCAM to UCS (Uniform Color Space) for color difference
 * ---------------------------------------------------------------- */

alwan_status alwan_zcam_to_ucs_f64(alwan_jzazbz_f64 *Jab_out,
                      alwan_zcam_correlates_f64 const *correlates) {
    if (!correlates || !Jab_out) {
        return ALWAN_E_INVALID;
    }

    /* Denormalize input correlates */
    alwan_zcam_correlates_f64 tmp = *correlates;
    ALWAN_DENORM_ZCAM(&tmp);

    /* Build value-type correlates (only Jz, Mz, hz needed) */
    alwan_zcam_v_correlates_f64 v;
    v.Jz = tmp.Jz;
    v.Mz = tmp.Mz;
    v.hz = tmp.hz;
    v.Cz = ALWAN_LITERAL(0.0);
    v.Qz = ALWAN_LITERAL(0.0);
    v.Sz = ALWAN_LITERAL(0.0);
    v.Vz = ALWAN_LITERAL(0.0);
    v.Kz = ALWAN_LITERAL(0.0);
    v.Wz = ALWAN_LITERAL(0.0);

    /* Delegate to value-returning core */
    *Jab_out = alwan_zcam_to_ucs_f64_v(v);

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F32
/* ----------------------------------------------------------------
 * f32 Wrappers
 *
 * The ZCAM core math is computed in f64 (see file header note); the f32
 * entry points convert inputs up to f64, call the f64 implementation,
 * then narrow the outputs back to f32.
 * ---------------------------------------------------------------- */

static void zcam_vc_f32_to_f64(alwan_zcam_viewing_conditions_f64 *dst,
                               alwan_zcam_viewing_conditions_f32 const *src) {
    dst->xyz_w.x = (alwan_f64)src->xyz_w.x;
    dst->xyz_w.y = (alwan_f64)src->xyz_w.y;
    dst->xyz_w.z = (alwan_f64)src->xyz_w.z;
    dst->La = (alwan_f64)src->La;
    dst->Yb = (alwan_f64)src->Yb;
    dst->surround = src->surround;
    dst->discount_illuminant = src->discount_illuminant;
}

static void zcam_correlates_f64_to_f32(alwan_zcam_correlates_f32 *dst,
                                       alwan_zcam_correlates_f64 const *src) {
    dst->Jz = (alwan_f32)src->Jz;
    dst->Cz = (alwan_f32)src->Cz;
    dst->hz = (alwan_f32)src->hz;
    dst->Qz = (alwan_f32)src->Qz;
    dst->Mz = (alwan_f32)src->Mz;
    dst->Sz = (alwan_f32)src->Sz;
    dst->Vz = (alwan_f32)src->Vz;
    dst->Kz = (alwan_f32)src->Kz;
    dst->Wz = (alwan_f32)src->Wz;
}

static void zcam_correlates_f32_to_f64(alwan_zcam_correlates_f64 *dst,
                                       alwan_zcam_correlates_f32 const *src) {
    dst->Jz = (alwan_f64)src->Jz;
    dst->Cz = (alwan_f64)src->Cz;
    dst->hz = (alwan_f64)src->hz;
    dst->Qz = (alwan_f64)src->Qz;
    dst->Mz = (alwan_f64)src->Mz;
    dst->Sz = (alwan_f64)src->Sz;
    dst->Vz = (alwan_f64)src->Vz;
    dst->Kz = (alwan_f64)src->Kz;
    dst->Wz = (alwan_f64)src->Wz;
}

alwan_status alwan_zcam_forward_f32(alwan_zcam_correlates_f32 *out,
                           alwan_xyz_f32 const *xyz,
                           alwan_zcam_viewing_conditions_f32 const *vc) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }
    alwan_xyz_f64 xyz64 = {(alwan_f64)xyz->x, (alwan_f64)xyz->y, (alwan_f64)xyz->z};
    alwan_zcam_viewing_conditions_f64 vc64;
    zcam_vc_f32_to_f64(&vc64, vc);
    alwan_zcam_correlates_f64 out64;
    int rc = alwan_zcam_forward_f64(&out64, &xyz64, &vc64);
    if (rc != ALWAN_OK) return rc;
    zcam_correlates_f64_to_f32(out, &out64);
    return ALWAN_OK;
}

alwan_status alwan_zcam_inverse_f32(alwan_xyz_f32 *xyz,
                           alwan_zcam_correlates_f32 const *correlates,
                           alwan_zcam_viewing_conditions_f32 const *vc) {
    if (!xyz || !correlates || !vc) {
        return ALWAN_E_INVALID;
    }
    alwan_zcam_correlates_f64 c64;
    zcam_correlates_f32_to_f64(&c64, correlates);
    alwan_zcam_viewing_conditions_f64 vc64;
    zcam_vc_f32_to_f64(&vc64, vc);
    alwan_xyz_f64 xyz64;
    int rc = alwan_zcam_inverse_f64(&xyz64, &c64, &vc64);
    if (rc != ALWAN_OK) return rc;
    xyz->x = (alwan_f32)xyz64.x;
    xyz->y = (alwan_f32)xyz64.y;
    xyz->z = (alwan_f32)xyz64.z;
    return ALWAN_OK;
}

alwan_status alwan_zcam_to_ucs_f32(alwan_jzazbz_f32 *Jab_out,
                          alwan_zcam_correlates_f32 const *correlates) {
    if (!Jab_out || !correlates) {
        return ALWAN_E_INVALID;
    }
    alwan_zcam_correlates_f64 c64;
    zcam_correlates_f32_to_f64(&c64, correlates);
    alwan_jzazbz_f64 jab64;
    int rc = alwan_zcam_to_ucs_f64(&jab64, &c64);
    if (rc != ALWAN_OK) return rc;
    Jab_out->Jz = (alwan_f32)jab64.Jz;
    Jab_out->az = (alwan_f32)jab64.az;
    Jab_out->bz = (alwan_f32)jab64.bz;
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */
