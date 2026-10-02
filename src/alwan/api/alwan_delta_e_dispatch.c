/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * One entry point for every colour difference alwan has: alwan_delta_e_{T} takes the
 * method and the two colours as three numbers each, in the method's own space, and calls
 * the formula's own function, so its result is that function's to the bit (suite 30).
 * The per-formula functions stay; this only picks one by enum.
 */

#include "../alwan.h"
#include "../alwan_internal.h"

#define ALWAN_DE_DISPATCH(T, TN)                                                                          \
alwan_status alwan_delta_e_##TN(T *out, alwan_delta_e_method method, T const a[3], T const b[3],          \
                                alwan_delta_e_params_##TN const *params) {                                \
    alwan_delta_e_params_##TN p;                                                                          \
    if (!out || !a || !b) return ALWAN_E_INVALID;                                                         \
    if (params) p = *params; else { p.cmc_l = 0; p.cmc_c = 0; p.hych_textiles = 0; p.itp_scalar = 0; }    \
    switch (method) {                                                                                     \
    case ALWAN_DELTA_E_CIE1976: case ALWAN_DELTA_E_CIE1994: case ALWAN_DELTA_E_CIEDE2000:                \
    case ALWAN_DELTA_E_CMC: case ALWAN_DELTA_E_HYAB: case ALWAN_DELTA_E_HYCH: {                           \
        alwan_lab_##TN la, lb;                                                                            \
        la.L = a[0]; la.a = a[1]; la.b = a[2];                                                            \
        lb.L = b[0]; lb.a = b[1]; lb.b = b[2];                                                            \
        if (method == ALWAN_DELTA_E_CIE1976) *out = alwan_delta_e_76_##TN(&la, &lb);                      \
        else if (method == ALWAN_DELTA_E_CIE1994) *out = alwan_delta_e_94_##TN(&la, &lb);                 \
        else if (method == ALWAN_DELTA_E_CIEDE2000) *out = alwan_delta_e_2000_##TN(&la, &lb);             \
        else if (method == ALWAN_DELTA_E_HYAB) *out = alwan_delta_e_hyab_##TN(&la, &lb);                  \
        else if (method == ALWAN_DELTA_E_HYCH) *out = alwan_delta_e_hych_##TN(&la, &lb, p.hych_textiles); \
        else {                                                                                            \
            alwan_delta_e_cmc_params_##TN cp;                                                             \
            alwan_delta_e_cmc_params_default_##TN(&cp);                                                  \
            if (p.cmc_l != 0) cp.l = p.cmc_l;                                                             \
            if (p.cmc_c != 0) cp.c = p.cmc_c;                                                             \
            *out = alwan_delta_e_cmc_##TN(&la, &lb, &cp);                                                 \
        }                                                                                                 \
        return ALWAN_OK;                                                                                  \
    }                                                                                                     \
    case ALWAN_DELTA_E_DIN99: {                                                                           \
        alwan_din99_##TN da, db;                                                                          \
        da.L99 = a[0]; da.a99 = a[1]; da.b99 = a[2];                                                      \
        db.L99 = b[0]; db.a99 = b[1]; db.b99 = b[2];                                                      \
        *out = alwan_delta_e_din99_##TN(&da, &db);                                                        \
        return ALWAN_OK;                                                                                  \
    }                                                                                                     \
    case ALWAN_DELTA_E_CAM02_LCD: case ALWAN_DELTA_E_CAM02_SCD: case ALWAN_DELTA_E_CAM02_UCS:            \
    case ALWAN_DELTA_E_CAM16_LCD: case ALWAN_DELTA_E_CAM16_SCD: case ALWAN_DELTA_E_CAM16_UCS: {          \
        alwan_cam_jab_##TN ja, jb;                                                                        \
        ja.J = a[0]; ja.a = a[1]; ja.b = a[2];                                                            \
        jb.J = b[0]; jb.a = b[1]; jb.b = b[2];                                                            \
        if (method == ALWAN_DELTA_E_CAM02_LCD) *out = alwan_delta_e_cam02_lcd_##TN(&ja, &jb);             \
        else if (method == ALWAN_DELTA_E_CAM02_SCD) *out = alwan_delta_e_cam02_scd_##TN(&ja, &jb);        \
        else if (method == ALWAN_DELTA_E_CAM02_UCS) *out = alwan_delta_e_cam02_ucs_##TN(&ja, &jb);        \
        else if (method == ALWAN_DELTA_E_CAM16_LCD) *out = alwan_delta_e_cam16_lcd_##TN(&ja, &jb);        \
        else if (method == ALWAN_DELTA_E_CAM16_SCD) *out = alwan_delta_e_cam16_scd_##TN(&ja, &jb);        \
        else *out = alwan_delta_e_cam16_ucs_##TN(&ja, &jb);                                               \
        return ALWAN_OK;                                                                                  \
    }                                                                                                     \
    case ALWAN_DELTA_E_ITP: {                                                                             \
        alwan_ictcp_##TN ia, ib;                                                                          \
        alwan_delta_e_itp_params_##TN ip;                                                                 \
        ia.I = a[0]; ia.Ct = a[1]; ia.Cp = a[2];                                                          \
        ib.I = b[0]; ib.Ct = b[1]; ib.Cp = b[2];                                                          \
        ip.scalar_factor = p.itp_scalar != 0 ? p.itp_scalar : (T)720;                                     \
        *out = alwan_delta_e_itp_##TN(&ia, &ib, &ip);                                                     \
        return ALWAN_OK;                                                                                  \
    }                                                                                                     \
    case ALWAN_DELTA_E_OKLAB: {                                                                           \
        alwan_oklab_##TN oa, ob;                                                                          \
        oa.L = a[0]; oa.a = a[1]; oa.b = a[2];                                                            \
        ob.L = b[0]; ob.a = b[1]; ob.b = b[2];                                                            \
        *out = alwan_delta_e_ok_##TN(&oa, &ob);                                                           \
        return ALWAN_OK;                                                                                  \
    }                                                                                                     \
    case ALWAN_DELTA_E_JZAZBZ: {                                                                          \
        alwan_jzazbz_##TN za, zb;                                                                         \
        za.Jz = a[0]; za.az = a[1]; za.bz = a[2];                                                         \
        zb.Jz = b[0]; zb.az = b[1]; zb.bz = b[2];                                                         \
        *out = alwan_delta_e_zcam_##TN(&za, &zb);                                                         \
        return ALWAN_OK;                                                                                  \
    }                                                                                                     \
    default:                                                                                              \
        return ALWAN_E_INVALID;                                                                           \
    }                                                                                                     \
}

#if ALWAN_WITH_F32
ALWAN_DE_DISPATCH(alwan_f32, f32)
#endif
#if ALWAN_WITH_F64
ALWAN_DE_DISPATCH(alwan_f64, f64)
#endif
