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
 * ---------------------------------------------------------------- */

/* Nonlinearity exponent from colour-science */
static alwan_scalar const IPT_EXPONENT =
#include "data/ipt_exponent.csv"
;

/* XYZ (D65) to LMS matrix (Hunt-Pointer-Estevez adapted for D65) from colour-science */
static alwan_scalar const XYZ_TO_LMS_IPT[9] = {
#include "data/ipt_xyz_to_lms.csv"
};

/* LMS to XYZ inverse matrix from colour-science */
static alwan_scalar const LMS_TO_XYZ_IPT[9] = {
#include "data/ipt_lms_to_xyz.csv"
};

/* LMS' to IPT matrix from colour-science */
static alwan_scalar const LMS_P_TO_IPT[9] = {
#include "data/ipt_lms_p_to_ipt.csv"
};

/* IPT to LMS' inverse matrix from colour-science */
static alwan_scalar const IPT_TO_LMS_P[9] = {
#include "data/ipt_ipt_to_lms_p.csv"
};

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

void alwan_xyz_to_ipt(alwan_vec3 const *xyz, alwan_vec3 *ipt) {
    /* Normalize XYZ from Y=100 scale to Y=1 scale */
    alwan_vec3 xyz_norm;
    xyz_norm.v[0] = xyz->v[0] / ALWAN_LITERAL(100.0);
    xyz_norm.v[1] = xyz->v[1] / ALWAN_LITERAL(100.0);
    xyz_norm.v[2] = xyz->v[2] / ALWAN_LITERAL(100.0);

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
    ipt->v[0] = LMS_P_TO_IPT[0] * lms_p.v[0] + LMS_P_TO_IPT[1] * lms_p.v[1] + LMS_P_TO_IPT[2] * lms_p.v[2];
    ipt->v[1] = LMS_P_TO_IPT[3] * lms_p.v[0] + LMS_P_TO_IPT[4] * lms_p.v[1] + LMS_P_TO_IPT[5] * lms_p.v[2];
    ipt->v[2] = LMS_P_TO_IPT[6] * lms_p.v[0] + LMS_P_TO_IPT[7] * lms_p.v[1] + LMS_P_TO_IPT[8] * lms_p.v[2];
}

void alwan_ipt_to_xyz(alwan_vec3 const *ipt, alwan_vec3 *xyz) {
    /* Step 1: IPT → LMS' */
    alwan_vec3 lms_p;
    lms_p.v[0] = IPT_TO_LMS_P[0] * ipt->v[0] + IPT_TO_LMS_P[1] * ipt->v[1] + IPT_TO_LMS_P[2] * ipt->v[2];
    lms_p.v[1] = IPT_TO_LMS_P[3] * ipt->v[0] + IPT_TO_LMS_P[4] * ipt->v[1] + IPT_TO_LMS_P[5] * ipt->v[2];
    lms_p.v[2] = IPT_TO_LMS_P[6] * ipt->v[0] + IPT_TO_LMS_P[7] * ipt->v[1] + IPT_TO_LMS_P[8] * ipt->v[2];

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
    xyz->v[0] = xyz_norm.v[0] * ALWAN_LITERAL(100.0);
    xyz->v[1] = xyz_norm.v[1] * ALWAN_LITERAL(100.0);
    xyz->v[2] = xyz_norm.v[2] * ALWAN_LITERAL(100.0);
}

/* ----------------------------------------------------------------
 * IPT <-> IPTch (Cylindrical)
 * ---------------------------------------------------------------- */

void alwan_ipt_to_iptch(alwan_vec3 const *ipt, alwan_vec3 *iptch) {
    /* I stays the same */
    iptch->v[0] = ipt->v[0];

    /* Chroma C = sqrt(P^2 + T^2) */
    iptch->v[1] = ALWAN_SQRT(ipt->v[1] * ipt->v[1] + ipt->v[2] * ipt->v[2]);

    /* Hue h = atan2(T, P) in radians */
    iptch->v[2] = ALWAN_ATAN2(ipt->v[2], ipt->v[1]);
}

void alwan_iptch_to_ipt(alwan_vec3 const *iptch, alwan_vec3 *ipt) {
    /* I stays the same */
    ipt->v[0] = iptch->v[0];

    /* P = C * cos(h) */
    ipt->v[1] = iptch->v[1] * ALWAN_COS(iptch->v[2]);

    /* T = C * sin(h) */
    ipt->v[2] = iptch->v[1] * ALWAN_SIN(iptch->v[2]);
}
