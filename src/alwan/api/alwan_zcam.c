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
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_zcam_core.h"

/* ----------------------------------------------------------------
 * Surround Enum Resolution (kept in the .c wrapper)
 * ---------------------------------------------------------------- */

static void get_zcam_surround_params(alwan_zcam_surround surround,
                                     alwan_scalar *Fs, alwan_scalar *c,
                                     alwan_scalar *Nc, alwan_scalar *F) {
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

int alwan_zcam_forward(alwan_zcam_correlates *out,
                       alwan_xyz const *xyz,
                       alwan_zcam_viewing_conditions const *vc) {
    if (!xyz || !vc || !out) {
        return -1;
    }

    /* Resolve surround enum to scalar parameters */
    alwan_scalar Fs, c, Nc, F;
    get_zcam_surround_params(vc->surround, &Fs, &c, &Nc, &F);

    /* Delegate to value-returning core */
    alwan_zcam_v_correlates v = alwan_zcam_forward_v(
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

    return 0;
}

/* ----------------------------------------------------------------
 * ZCAM Inverse Transform: Correlates -> XYZ
 * ---------------------------------------------------------------- */

int alwan_zcam_inverse(alwan_xyz *xyz,
                       alwan_zcam_correlates const *correlates,
                       alwan_zcam_viewing_conditions const *vc) {
    if (!correlates || !vc || !xyz) {
        return -1;
    }

    /* Resolve surround enum to scalar parameters */
    alwan_scalar Fs, c, Nc, F;
    get_zcam_surround_params(vc->surround, &Fs, &c, &Nc, &F);

    /* Build value-type correlates from public struct */
    alwan_zcam_v_correlates v;
    v.Jz = correlates->Jz;
    v.Cz = correlates->Cz;
    v.hz = correlates->hz;
    v.Qz = correlates->Qz;
    v.Mz = correlates->Mz;
    v.Sz = correlates->Sz;
    v.Vz = correlates->Vz;
    v.Kz = correlates->Kz;
    v.Wz = correlates->Wz;

    /* Delegate to value-returning core */
    *xyz = alwan_zcam_inverse_v(
        v, vc->xyz_w,
        Fs, c, Nc, F,
        vc->La, vc->Yb, vc->xyz_w.y);

    return 0;
}

/* ----------------------------------------------------------------
 * ZCAM to UCS (Uniform Color Space) for color difference
 * ---------------------------------------------------------------- */

int alwan_zcam_to_ucs(alwan_jzazbz *Jab_out,
                      alwan_zcam_correlates const *correlates) {
    if (!correlates || !Jab_out) {
        return -1;
    }

    /* Build value-type correlates (only Jz, Mz, hz needed) */
    alwan_zcam_v_correlates v;
    v.Jz = correlates->Jz;
    v.Mz = correlates->Mz;
    v.hz = correlates->hz;
    v.Cz = ALWAN_LITERAL(0.0);
    v.Qz = ALWAN_LITERAL(0.0);
    v.Sz = ALWAN_LITERAL(0.0);
    v.Vz = ALWAN_LITERAL(0.0);
    v.Kz = ALWAN_LITERAL(0.0);
    v.Wz = ALWAN_LITERAL(0.0);

    /* Delegate to value-returning core */
    *Jab_out = alwan_zcam_to_ucs_v(v);

    return 0;
}
