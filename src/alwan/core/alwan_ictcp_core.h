/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ICtCp Color Space (ITU-R BT.2100 HDR)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: ITU-R Recommendation BT.2100-3 (02/2025)
 */

#ifndef ALWAN_ICTCP_CORE_H
#define ALWAN_ICTCP_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_core.h"

/* ----------------------------------------------------------------
 * ICtCp Transformation Matrices
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const ICTCP_RGB_TO_LMS[9] = {
#include "../data/ictcp_rgb_to_lms.csv"
};
static alwan_scalar const ICTCP_LMS_TO_RGB[9] = {
#include "../data/ictcp_lms_to_rgb.csv"
};
static alwan_scalar const ICTCP_LMS_P_TO_ICTCP_PQ[9] = {
#include "../data/ictcp_lms_p_to_ictcp_pq.csv"
};
static alwan_scalar const ICTCP_ICTCP_TO_LMS_P_PQ[9] = {
#include "../data/ictcp_ictcp_to_lms_p_pq.csv"
};
static alwan_scalar const ICTCP_LMS_P_TO_ICTCP_HLG[9] = {
#include "../data/ictcp_lms_p_to_ictcp_hlg.csv"
};
static alwan_scalar const ICTCP_ICTCP_TO_LMS_P_HLG[9] = {
#include "../data/ictcp_ictcp_to_lms_p_hlg.csv"
};
static alwan_scalar const ICTCP_XYZ_TO_BT2020[9] = {
#include "../data/ictcp_xyz_to_bt2020.csv"
};
static alwan_scalar const ICTCP_BT2020_TO_XYZ[9] = {
#include "../data/ictcp_bt2020_to_xyz.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Helper: HLG Inverse OETF (branchless)
 * Mathematical inverse of HLG OETF, NOT the same as HLG EOTF
 * (HLG EOTF includes system gamma 1.2)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_scalar alwan_hlg_inverse_oetf_v(alwan_scalar encoded) {
    alwan_scalar a = ALWAN_LITERAL(0.17883277);
    alwan_scalar b = ALWAN_ONE - ALWAN_LITERAL(4.0) * a;
    alwan_scalar c = ALWAN_LITERAL(0.5) - a * ALWAN_LN(ALWAN_LITERAL(4.0) * a);

    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    alwan_scalar linear_lo = (E * E) / ALWAN_LITERAL(3.0);
    alwan_scalar linear_hi = (ALWAN_EXP((E - c) / a) + b) / ALWAN_LITERAL(12.0);
    return ALWAN_SELECT(E <= ALWAN_LITERAL(0.5), linear_lo, linear_hi);
}

/* ----------------------------------------------------------------
 * BT.2020 RGB -> ICtCp (PQ) (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_ictcp alwan_rgb_to_ictcp_pq_v(alwan_rgb rgb) {
    alwan_ictcp result;

    /* Step 1: BT.2020 RGB (linear) -> LMS */
    alwan_scalar l = ICTCP_RGB_TO_LMS[0] * rgb.r + ICTCP_RGB_TO_LMS[1] * rgb.g + ICTCP_RGB_TO_LMS[2] * rgb.b;
    alwan_scalar m = ICTCP_RGB_TO_LMS[3] * rgb.r + ICTCP_RGB_TO_LMS[4] * rgb.g + ICTCP_RGB_TO_LMS[5] * rgb.b;
    alwan_scalar s = ICTCP_RGB_TO_LMS[6] * rgb.r + ICTCP_RGB_TO_LMS[7] * rgb.g + ICTCP_RGB_TO_LMS[8] * rgb.b;

    /* Step 2: LMS -> LMS' via PQ OETF */
    alwan_scalar lp = alwan_pq_oetf(l);
    alwan_scalar mp = alwan_pq_oetf(m);
    alwan_scalar sp = alwan_pq_oetf(s);

    /* Step 3: LMS' -> ICtCp via PQ matrix */
    result.I  = ICTCP_LMS_P_TO_ICTCP_PQ[0] * lp + ICTCP_LMS_P_TO_ICTCP_PQ[1] * mp + ICTCP_LMS_P_TO_ICTCP_PQ[2] * sp;
    result.Ct = ICTCP_LMS_P_TO_ICTCP_PQ[3] * lp + ICTCP_LMS_P_TO_ICTCP_PQ[4] * mp + ICTCP_LMS_P_TO_ICTCP_PQ[5] * sp;
    result.Cp = ICTCP_LMS_P_TO_ICTCP_PQ[6] * lp + ICTCP_LMS_P_TO_ICTCP_PQ[7] * mp + ICTCP_LMS_P_TO_ICTCP_PQ[8] * sp;

    return result;
}

/* ----------------------------------------------------------------
 * ICtCp (PQ) -> BT.2020 RGB (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_rgb alwan_ictcp_pq_to_rgb_v(alwan_ictcp ictcp) {
    alwan_rgb result;

    /* Step 1: ICtCp -> LMS' via PQ inverse matrix */
    alwan_scalar lp = ICTCP_ICTCP_TO_LMS_P_PQ[0] * ictcp.I + ICTCP_ICTCP_TO_LMS_P_PQ[1] * ictcp.Ct + ICTCP_ICTCP_TO_LMS_P_PQ[2] * ictcp.Cp;
    alwan_scalar mp = ICTCP_ICTCP_TO_LMS_P_PQ[3] * ictcp.I + ICTCP_ICTCP_TO_LMS_P_PQ[4] * ictcp.Ct + ICTCP_ICTCP_TO_LMS_P_PQ[5] * ictcp.Cp;
    alwan_scalar sp = ICTCP_ICTCP_TO_LMS_P_PQ[6] * ictcp.I + ICTCP_ICTCP_TO_LMS_P_PQ[7] * ictcp.Ct + ICTCP_ICTCP_TO_LMS_P_PQ[8] * ictcp.Cp;

    /* Step 2: LMS' -> LMS via PQ EOTF */
    alwan_scalar l = alwan_pq_eotf(lp);
    alwan_scalar m = alwan_pq_eotf(mp);
    alwan_scalar s = alwan_pq_eotf(sp);

    /* Step 3: LMS -> BT.2020 RGB (linear) */
    result.r = ICTCP_LMS_TO_RGB[0] * l + ICTCP_LMS_TO_RGB[1] * m + ICTCP_LMS_TO_RGB[2] * s;
    result.g = ICTCP_LMS_TO_RGB[3] * l + ICTCP_LMS_TO_RGB[4] * m + ICTCP_LMS_TO_RGB[5] * s;
    result.b = ICTCP_LMS_TO_RGB[6] * l + ICTCP_LMS_TO_RGB[7] * m + ICTCP_LMS_TO_RGB[8] * s;

    return result;
}

/* ----------------------------------------------------------------
 * BT.2020 RGB -> ICtCp (HLG) (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_ictcp alwan_rgb_to_ictcp_hlg_v(alwan_rgb rgb) {
    alwan_ictcp result;

    /* Step 1: BT.2020 RGB (linear) -> LMS */
    alwan_scalar l = ICTCP_RGB_TO_LMS[0] * rgb.r + ICTCP_RGB_TO_LMS[1] * rgb.g + ICTCP_RGB_TO_LMS[2] * rgb.b;
    alwan_scalar m = ICTCP_RGB_TO_LMS[3] * rgb.r + ICTCP_RGB_TO_LMS[4] * rgb.g + ICTCP_RGB_TO_LMS[5] * rgb.b;
    alwan_scalar s = ICTCP_RGB_TO_LMS[6] * rgb.r + ICTCP_RGB_TO_LMS[7] * rgb.g + ICTCP_RGB_TO_LMS[8] * rgb.b;

    /* Step 2: LMS -> LMS' via HLG OETF */
    alwan_scalar lp = alwan_hlg_oetf(l);
    alwan_scalar mp = alwan_hlg_oetf(m);
    alwan_scalar sp = alwan_hlg_oetf(s);

    /* Step 3: LMS' -> ICtCp via HLG matrix */
    result.I  = ICTCP_LMS_P_TO_ICTCP_HLG[0] * lp + ICTCP_LMS_P_TO_ICTCP_HLG[1] * mp + ICTCP_LMS_P_TO_ICTCP_HLG[2] * sp;
    result.Ct = ICTCP_LMS_P_TO_ICTCP_HLG[3] * lp + ICTCP_LMS_P_TO_ICTCP_HLG[4] * mp + ICTCP_LMS_P_TO_ICTCP_HLG[5] * sp;
    result.Cp = ICTCP_LMS_P_TO_ICTCP_HLG[6] * lp + ICTCP_LMS_P_TO_ICTCP_HLG[7] * mp + ICTCP_LMS_P_TO_ICTCP_HLG[8] * sp;

    return result;
}

/* ----------------------------------------------------------------
 * ICtCp (HLG) -> BT.2020 RGB (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_rgb alwan_ictcp_hlg_to_rgb_v(alwan_ictcp ictcp) {
    alwan_rgb result;

    /* Step 1: ICtCp -> LMS' via HLG inverse matrix */
    alwan_scalar lp = ICTCP_ICTCP_TO_LMS_P_HLG[0] * ictcp.I + ICTCP_ICTCP_TO_LMS_P_HLG[1] * ictcp.Ct + ICTCP_ICTCP_TO_LMS_P_HLG[2] * ictcp.Cp;
    alwan_scalar mp = ICTCP_ICTCP_TO_LMS_P_HLG[3] * ictcp.I + ICTCP_ICTCP_TO_LMS_P_HLG[4] * ictcp.Ct + ICTCP_ICTCP_TO_LMS_P_HLG[5] * ictcp.Cp;
    alwan_scalar sp = ICTCP_ICTCP_TO_LMS_P_HLG[6] * ictcp.I + ICTCP_ICTCP_TO_LMS_P_HLG[7] * ictcp.Ct + ICTCP_ICTCP_TO_LMS_P_HLG[8] * ictcp.Cp;

    /* Step 2: LMS' -> LMS via HLG inverse OETF (NOT EOTF - no system gamma) */
    alwan_scalar l = alwan_hlg_inverse_oetf_v(lp);
    alwan_scalar m = alwan_hlg_inverse_oetf_v(mp);
    alwan_scalar s = alwan_hlg_inverse_oetf_v(sp);

    /* Step 3: LMS -> BT.2020 RGB (linear) */
    result.r = ICTCP_LMS_TO_RGB[0] * l + ICTCP_LMS_TO_RGB[1] * m + ICTCP_LMS_TO_RGB[2] * s;
    result.g = ICTCP_LMS_TO_RGB[3] * l + ICTCP_LMS_TO_RGB[4] * m + ICTCP_LMS_TO_RGB[5] * s;
    result.b = ICTCP_LMS_TO_RGB[6] * l + ICTCP_LMS_TO_RGB[7] * m + ICTCP_LMS_TO_RGB[8] * s;

    return result;
}

/* ----------------------------------------------------------------
 * XYZ -> ICtCp (PQ) (value-returning, via BT.2020 RGB)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_ictcp alwan_xyz_to_ictcp_pq_v(alwan_xyz xyz) {
    /* Step 1: XYZ (D65) -> BT.2020 RGB (linear) */
    alwan_rgb rgb;
    rgb.r = ICTCP_XYZ_TO_BT2020[0] * xyz.x + ICTCP_XYZ_TO_BT2020[1] * xyz.y + ICTCP_XYZ_TO_BT2020[2] * xyz.z;
    rgb.g = ICTCP_XYZ_TO_BT2020[3] * xyz.x + ICTCP_XYZ_TO_BT2020[4] * xyz.y + ICTCP_XYZ_TO_BT2020[5] * xyz.z;
    rgb.b = ICTCP_XYZ_TO_BT2020[6] * xyz.x + ICTCP_XYZ_TO_BT2020[7] * xyz.y + ICTCP_XYZ_TO_BT2020[8] * xyz.z;

    /* Step 2: BT.2020 RGB -> ICtCp (PQ) */
    return alwan_rgb_to_ictcp_pq_v(rgb);
}

/* ----------------------------------------------------------------
 * ICtCp (PQ) -> XYZ (value-returning, via BT.2020 RGB)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_xyz alwan_ictcp_pq_to_xyz_v(alwan_ictcp ictcp) {
    alwan_xyz result;

    /* Step 1: ICtCp (PQ) -> BT.2020 RGB (linear) */
    alwan_rgb rgb = alwan_ictcp_pq_to_rgb_v(ictcp);

    /* Step 2: BT.2020 RGB -> XYZ (D65) */
    result.x = ICTCP_BT2020_TO_XYZ[0] * rgb.r + ICTCP_BT2020_TO_XYZ[1] * rgb.g + ICTCP_BT2020_TO_XYZ[2] * rgb.b;
    result.y = ICTCP_BT2020_TO_XYZ[3] * rgb.r + ICTCP_BT2020_TO_XYZ[4] * rgb.g + ICTCP_BT2020_TO_XYZ[5] * rgb.b;
    result.z = ICTCP_BT2020_TO_XYZ[6] * rgb.r + ICTCP_BT2020_TO_XYZ[7] * rgb.g + ICTCP_BT2020_TO_XYZ[8] * rgb.b;

    return result;
}

/* ----------------------------------------------------------------
 * XYZ -> ICtCp (HLG) (value-returning, via BT.2020 RGB)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_ictcp alwan_xyz_to_ictcp_hlg_v(alwan_xyz xyz) {
    /* Step 1: XYZ (D65) -> BT.2020 RGB (linear) */
    alwan_rgb rgb;
    rgb.r = ICTCP_XYZ_TO_BT2020[0] * xyz.x + ICTCP_XYZ_TO_BT2020[1] * xyz.y + ICTCP_XYZ_TO_BT2020[2] * xyz.z;
    rgb.g = ICTCP_XYZ_TO_BT2020[3] * xyz.x + ICTCP_XYZ_TO_BT2020[4] * xyz.y + ICTCP_XYZ_TO_BT2020[5] * xyz.z;
    rgb.b = ICTCP_XYZ_TO_BT2020[6] * xyz.x + ICTCP_XYZ_TO_BT2020[7] * xyz.y + ICTCP_XYZ_TO_BT2020[8] * xyz.z;

    /* Step 2: BT.2020 RGB -> ICtCp (HLG) */
    return alwan_rgb_to_ictcp_hlg_v(rgb);
}

/* ----------------------------------------------------------------
 * ICtCp (HLG) -> XYZ (value-returning, via BT.2020 RGB)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_xyz alwan_ictcp_hlg_to_xyz_v(alwan_ictcp ictcp) {
    alwan_xyz result;

    /* Step 1: ICtCp (HLG) -> BT.2020 RGB (linear) */
    alwan_rgb rgb = alwan_ictcp_hlg_to_rgb_v(ictcp);

    /* Step 2: BT.2020 RGB -> XYZ (D65) */
    result.x = ICTCP_BT2020_TO_XYZ[0] * rgb.r + ICTCP_BT2020_TO_XYZ[1] * rgb.g + ICTCP_BT2020_TO_XYZ[2] * rgb.b;
    result.y = ICTCP_BT2020_TO_XYZ[3] * rgb.r + ICTCP_BT2020_TO_XYZ[4] * rgb.g + ICTCP_BT2020_TO_XYZ[5] * rgb.b;
    result.z = ICTCP_BT2020_TO_XYZ[6] * rgb.r + ICTCP_BT2020_TO_XYZ[7] * rgb.g + ICTCP_BT2020_TO_XYZ[8] * rgb.b;

    return result;
}

#endif /* ALWAN_ICTCP_CORE_H */
