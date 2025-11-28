/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * ICtCp (ITU-R BT.2100 HDR Color Space)
 *
 * Reference: ITU-R Recommendation BT.2100-3 (02/2025)
 * "Image parameter values for high dynamic range television for use in production and international programme exchange"
 * https://www.itu.int/rec/R-REC-BT.2100/
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ----------------------------------------------------------------
 * ICtCp Transformation Matrices
 * Data defined in alwan_data.c
 * ---------------------------------------------------------------- */

#define RGB_TO_LMS g_ictcp_rgb_to_lms
#define LMS_TO_RGB g_ictcp_lms_to_rgb
#define LMS_P_TO_ICTCP_PQ g_ictcp_lms_p_to_ictcp_pq
#define ICTCP_TO_LMS_P_PQ g_ictcp_ictcp_to_lms_p_pq
#define LMS_P_TO_ICTCP_HLG g_ictcp_lms_p_to_ictcp_hlg
#define ICTCP_TO_LMS_P_HLG g_ictcp_ictcp_to_lms_p_hlg
#define XYZ_TO_BT2020 g_ictcp_xyz_to_bt2020
#define BT2020_TO_XYZ g_ictcp_bt2020_to_xyz

/* ----------------------------------------------------------------
 * Helper Functions
 * ---------------------------------------------------------------- */

/* Apply PQ OETF (linear to encoded) to a single value */
static alwan_scalar pq_oetf(alwan_scalar linear) {
    alwan_scalar encoded;
    alwan_oetf_apply(ALWAN_TF_PQ, &linear, 1, 1, &encoded, 1);
    return encoded;
}

/* Apply PQ EOTF (encoded to linear) to a single value */
static alwan_scalar pq_eotf(alwan_scalar encoded) {
    alwan_scalar linear;
    alwan_eotf_apply(ALWAN_TF_PQ, &encoded, 1, 1, &linear, 1);
    return linear;
}

/* Apply HLG OETF (scene linear to encoded) to a single value */
static alwan_scalar hlg_oetf(alwan_scalar linear) {
    alwan_scalar encoded;
    alwan_oetf_apply(ALWAN_TF_HLG, &linear, 1, 1, &encoded, 1);
    return encoded;
}

/* Apply HLG EOTF (encoded to display linear) to a single value */
static alwan_scalar hlg_eotf(alwan_scalar encoded) {
    alwan_scalar linear;
    alwan_eotf_apply(ALWAN_TF_HLG, &encoded, 1, 1, &linear, 1);
    return linear;
}

/* Apply HLG inverse OETF (encoded to scene linear) to a single value
 * This is the mathematical inverse of HLG OETF, NOT the same as HLG EOTF
 * (HLG EOTF includes system gamma 1.2 and produces display linear) */
static alwan_scalar hlg_inverse_oetf(alwan_scalar encoded) {
    /* HLG constants */
    alwan_scalar const a = ALWAN_LITERAL(0.17883277);
    alwan_scalar const b = ALWAN_LITERAL(1.0) - ALWAN_LITERAL(4.0) * a;
    alwan_scalar const c = ALWAN_LITERAL(0.5) - a * ALWAN_LOG(ALWAN_LITERAL(4.0) * a);

    if (encoded < ALWAN_LITERAL(0.0)) encoded = ALWAN_LITERAL(0.0);

    /* Inverse of HLG OETF (scene linear -> encoded):
     * if linear <= 1/12: encoded = sqrt(3 * linear)
     *   => linear = encoded^2 / 3
     * if linear > 1/12: encoded = a * ln(12 * linear - b) + c
     *   => linear = (exp((encoded - c) / a) + b) / 12
     */
    if (encoded <= ALWAN_LITERAL(0.5)) {
        return (encoded * encoded) / ALWAN_LITERAL(3.0);
    } else {
        return (ALWAN_EXP((encoded - c) / a) + b) / ALWAN_LITERAL(12.0);
    }
}

/* ----------------------------------------------------------------
 * RGB (BT.2020 linear) <-> ICtCp
 * ---------------------------------------------------------------- */

void alwan_rgb_to_ictcp(alwan_rgb const *rgb, alwan_ictcp *ictcp, int use_pq) {
    /* Step 1: BT.2020 RGB (linear) → LMS */
    alwan_vec3 lms;
    lms.v[0] = RGB_TO_LMS[0] * rgb->r + RGB_TO_LMS[1] * rgb->g + RGB_TO_LMS[2] * rgb->b;
    lms.v[1] = RGB_TO_LMS[3] * rgb->r + RGB_TO_LMS[4] * rgb->g + RGB_TO_LMS[5] * rgb->b;
    lms.v[2] = RGB_TO_LMS[6] * rgb->r + RGB_TO_LMS[7] * rgb->g + RGB_TO_LMS[8] * rgb->b;

    /* Step 2: Apply nonlinear transfer function: LMS → LMS' */
    alwan_vec3 lms_p;
    if (use_pq) {
        /* PQ transfer function */
        lms_p.v[0] = pq_oetf(lms.v[0]);
        lms_p.v[1] = pq_oetf(lms.v[1]);
        lms_p.v[2] = pq_oetf(lms.v[2]);
    } else {
        /* HLG transfer function */
        lms_p.v[0] = hlg_oetf(lms.v[0]);
        lms_p.v[1] = hlg_oetf(lms.v[1]);
        lms_p.v[2] = hlg_oetf(lms.v[2]);
    }

    /* Step 3: LMS' → ICtCp (matrix depends on PQ vs HLG) */
    alwan_scalar const *M = use_pq ? LMS_P_TO_ICTCP_PQ : LMS_P_TO_ICTCP_HLG;
    ictcp->I = M[0] * lms_p.v[0] + M[1] * lms_p.v[1] + M[2] * lms_p.v[2];
    ictcp->Ct = M[3] * lms_p.v[0] + M[4] * lms_p.v[1] + M[5] * lms_p.v[2];
    ictcp->Cp = M[6] * lms_p.v[0] + M[7] * lms_p.v[1] + M[8] * lms_p.v[2];
}

void alwan_ictcp_to_rgb(alwan_ictcp const *ictcp, alwan_rgb *rgb, int use_pq) {
    /* Step 1: ICtCp → LMS' (matrix depends on PQ vs HLG) */
    alwan_scalar const *M_inv = use_pq ? ICTCP_TO_LMS_P_PQ : ICTCP_TO_LMS_P_HLG;
    alwan_vec3 lms_p;
    lms_p.v[0] = M_inv[0] * ictcp->I + M_inv[1] * ictcp->Ct + M_inv[2] * ictcp->Cp;
    lms_p.v[1] = M_inv[3] * ictcp->I + M_inv[4] * ictcp->Ct + M_inv[5] * ictcp->Cp;
    lms_p.v[2] = M_inv[6] * ictcp->I + M_inv[7] * ictcp->Ct + M_inv[8] * ictcp->Cp;

    /* Step 2: Apply inverse transfer function: LMS' → LMS (scene linear) */
    alwan_vec3 lms;
    if (use_pq) {
        /* PQ inverse (EOTF = inverse OETF for PQ) */
        lms.v[0] = pq_eotf(lms_p.v[0]);
        lms.v[1] = pq_eotf(lms_p.v[1]);
        lms.v[2] = pq_eotf(lms_p.v[2]);
    } else {
        /* HLG inverse OETF (NOT EOTF - EOTF produces display linear) */
        lms.v[0] = hlg_inverse_oetf(lms_p.v[0]);
        lms.v[1] = hlg_inverse_oetf(lms_p.v[1]);
        lms.v[2] = hlg_inverse_oetf(lms_p.v[2]);
    }

    /* Step 3: LMS → BT.2020 RGB (linear) */
    rgb->r = LMS_TO_RGB[0] * lms.v[0] + LMS_TO_RGB[1] * lms.v[1] + LMS_TO_RGB[2] * lms.v[2];
    rgb->g = LMS_TO_RGB[3] * lms.v[0] + LMS_TO_RGB[4] * lms.v[1] + LMS_TO_RGB[5] * lms.v[2];
    rgb->b = LMS_TO_RGB[6] * lms.v[0] + LMS_TO_RGB[7] * lms.v[1] + LMS_TO_RGB[8] * lms.v[2];
}

/* ----------------------------------------------------------------
 * XYZ (D65) <-> ICtCp (via BT.2020 RGB)
 * ---------------------------------------------------------------- */

void alwan_xyz_to_ictcp(alwan_xyz const *xyz, alwan_ictcp *ictcp, int use_pq) {
    /* Step 1: XYZ (D65) → BT.2020 RGB (linear) */
    alwan_rgb rgb;
    rgb.r = XYZ_TO_BT2020[0] * xyz->x + XYZ_TO_BT2020[1] * xyz->y + XYZ_TO_BT2020[2] * xyz->z;
    rgb.g = XYZ_TO_BT2020[3] * xyz->x + XYZ_TO_BT2020[4] * xyz->y + XYZ_TO_BT2020[5] * xyz->z;
    rgb.b = XYZ_TO_BT2020[6] * xyz->x + XYZ_TO_BT2020[7] * xyz->y + XYZ_TO_BT2020[8] * xyz->z;

    /* Step 2: BT.2020 RGB → ICtCp */
    alwan_rgb_to_ictcp(&rgb, ictcp, use_pq);
}

void alwan_ictcp_to_xyz(alwan_ictcp const *ictcp, alwan_xyz *xyz, int use_pq) {
    /* Step 1: ICtCp → BT.2020 RGB (linear) */
    alwan_rgb rgb;
    alwan_ictcp_to_rgb(ictcp, &rgb, use_pq);

    /* Step 2: BT.2020 RGB → XYZ (D65) */
    xyz->x = BT2020_TO_XYZ[0] * rgb.r + BT2020_TO_XYZ[1] * rgb.g + BT2020_TO_XYZ[2] * rgb.b;
    xyz->y = BT2020_TO_XYZ[3] * rgb.r + BT2020_TO_XYZ[4] * rgb.g + BT2020_TO_XYZ[5] * rgb.b;
    xyz->z = BT2020_TO_XYZ[6] * rgb.r + BT2020_TO_XYZ[7] * rgb.g + BT2020_TO_XYZ[8] * rgb.b;
}
