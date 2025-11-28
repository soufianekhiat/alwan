/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * IPT Color Space (Image Processing Transform)
 *
 * Reference: Ebner & Fairchild (1998)
 * "Development and Testing of a Color Space (IPT) with Improved Hue Uniformity"
 * https://www.researchgate.net/publication/221677980
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ----------------------------------------------------------------
 * IPT Constants (Ebner & Fairchild 1998)
 * Data defined in alwan_data.c
 * ---------------------------------------------------------------- */

#define IPT_EXPONENT g_ipt_exponent
#define XYZ_TO_LMS_IPT g_ipt_xyz_to_lms
#define LMS_TO_XYZ_IPT g_ipt_lms_to_xyz
#define LMS_P_TO_IPT g_ipt_lms_p_to_ipt
#define IPT_TO_LMS_P g_ipt_ipt_to_lms_p

/* ----------------------------------------------------------------
 * Helper Functions: IPT Nonlinearity
 * ---------------------------------------------------------------- */

/* Apply IPT nonlinearity: f(x) = sign(x) × |x|^0.43 */
static alwan_scalar ipt_nonlinearity(alwan_scalar x) {
    if (x >= ALWAN_LITERAL(0.0)) {
        return ALWAN_POW(x, IPT_EXPONENT);
    } else {
        return -ALWAN_POW(-x, IPT_EXPONENT);
    }
}

/* Apply inverse IPT nonlinearity: f_inv(x) = sign(x) × |x|^(1/0.43) */
static alwan_scalar ipt_nonlinearity_inverse(alwan_scalar x) {
    alwan_scalar inv_exponent = ALWAN_LITERAL(1.0) / IPT_EXPONENT;
    if (x >= ALWAN_LITERAL(0.0)) {
        return ALWAN_POW(x, inv_exponent);
    } else {
        return -ALWAN_POW(-x, inv_exponent);
    }
}

/* ----------------------------------------------------------------
 * XYZ <-> IPT
 * ---------------------------------------------------------------- */

void alwan_xyz_to_ipt(alwan_xyz const *xyz, alwan_ipt *ipt) {
    /* Normalize XYZ from Y=100 scale to Y=1 scale */
    alwan_vec3 xyz_norm;
    xyz_norm.v[0] = xyz->x / ALWAN_LITERAL(100.0);
    xyz_norm.v[1] = xyz->y / ALWAN_LITERAL(100.0);
    xyz_norm.v[2] = xyz->z / ALWAN_LITERAL(100.0);

    /* Step 1: XYZ (D65) → LMS */
    alwan_vec3 lms;
    lms.v[0] = XYZ_TO_LMS_IPT[0] * xyz_norm.v[0] + XYZ_TO_LMS_IPT[1] * xyz_norm.v[1] + XYZ_TO_LMS_IPT[2] * xyz_norm.v[2];
    lms.v[1] = XYZ_TO_LMS_IPT[3] * xyz_norm.v[0] + XYZ_TO_LMS_IPT[4] * xyz_norm.v[1] + XYZ_TO_LMS_IPT[5] * xyz_norm.v[2];
    lms.v[2] = XYZ_TO_LMS_IPT[6] * xyz_norm.v[0] + XYZ_TO_LMS_IPT[7] * xyz_norm.v[1] + XYZ_TO_LMS_IPT[8] * xyz_norm.v[2];

    /* Step 2: Apply nonlinearity: LMS → LMS' */
    alwan_vec3 lms_p;
    lms_p.v[0] = ipt_nonlinearity(lms.v[0]);
    lms_p.v[1] = ipt_nonlinearity(lms.v[1]);
    lms_p.v[2] = ipt_nonlinearity(lms.v[2]);

    /* Step 3: LMS' → IPT */
    ipt->I = LMS_P_TO_IPT[0] * lms_p.v[0] + LMS_P_TO_IPT[1] * lms_p.v[1] + LMS_P_TO_IPT[2] * lms_p.v[2];
    ipt->P = LMS_P_TO_IPT[3] * lms_p.v[0] + LMS_P_TO_IPT[4] * lms_p.v[1] + LMS_P_TO_IPT[5] * lms_p.v[2];
    ipt->T = LMS_P_TO_IPT[6] * lms_p.v[0] + LMS_P_TO_IPT[7] * lms_p.v[1] + LMS_P_TO_IPT[8] * lms_p.v[2];
}

void alwan_ipt_to_xyz(alwan_ipt const *ipt, alwan_xyz *xyz) {
    /* Step 1: IPT → LMS' */
    alwan_vec3 lms_p;
    lms_p.v[0] = IPT_TO_LMS_P[0] * ipt->I + IPT_TO_LMS_P[1] * ipt->P + IPT_TO_LMS_P[2] * ipt->T;
    lms_p.v[1] = IPT_TO_LMS_P[3] * ipt->I + IPT_TO_LMS_P[4] * ipt->P + IPT_TO_LMS_P[5] * ipt->T;
    lms_p.v[2] = IPT_TO_LMS_P[6] * ipt->I + IPT_TO_LMS_P[7] * ipt->P + IPT_TO_LMS_P[8] * ipt->T;

    /* Step 2: Apply inverse nonlinearity: LMS' → LMS */
    alwan_vec3 lms;
    lms.v[0] = ipt_nonlinearity_inverse(lms_p.v[0]);
    lms.v[1] = ipt_nonlinearity_inverse(lms_p.v[1]);
    lms.v[2] = ipt_nonlinearity_inverse(lms_p.v[2]);

    /* Step 3: LMS → XYZ */
    alwan_vec3 xyz_norm;
    xyz_norm.v[0] = LMS_TO_XYZ_IPT[0] * lms.v[0] + LMS_TO_XYZ_IPT[1] * lms.v[1] + LMS_TO_XYZ_IPT[2] * lms.v[2];
    xyz_norm.v[1] = LMS_TO_XYZ_IPT[3] * lms.v[0] + LMS_TO_XYZ_IPT[4] * lms.v[1] + LMS_TO_XYZ_IPT[5] * lms.v[2];
    xyz_norm.v[2] = LMS_TO_XYZ_IPT[6] * lms.v[0] + LMS_TO_XYZ_IPT[7] * lms.v[1] + LMS_TO_XYZ_IPT[8] * lms.v[2];

    /* Scale back to Y=100 */
    xyz->x = xyz_norm.v[0] * ALWAN_LITERAL(100.0);
    xyz->y = xyz_norm.v[1] * ALWAN_LITERAL(100.0);
    xyz->z = xyz_norm.v[2] * ALWAN_LITERAL(100.0);
}

/* ----------------------------------------------------------------
 * IPT <-> IPTch (Cylindrical)
 * ---------------------------------------------------------------- */

void alwan_ipt_to_iptch(alwan_ipt const *ipt, alwan_vec3 *iptch) {
    /* I stays the same */
    iptch->v[0] = ipt->I;

    /* Chroma C = sqrt(P^2 + T^2) */
    iptch->v[1] = ALWAN_SQRT(ipt->P * ipt->P + ipt->T * ipt->T);

    /* Hue h = atan2(T, P) in radians */
    iptch->v[2] = ALWAN_ATAN2(ipt->T, ipt->P);
}

void alwan_iptch_to_ipt(alwan_vec3 const *iptch, alwan_ipt *ipt) {
    /* I stays the same */
    ipt->I = iptch->v[0];

    /* P = C * cos(h) */
    ipt->P = iptch->v[1] * ALWAN_COS(iptch->v[2]);

    /* T = C * sin(h) */
    ipt->T = iptch->v[1] * ALWAN_SIN(iptch->v[2]);
}
