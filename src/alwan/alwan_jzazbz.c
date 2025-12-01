/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Jzazbz & JzCzhz Color Spaces (HDR Perceptual)
 *
 * Reference: Safdar et al. (2017)
 * "Perceptually uniform color space for image signals including
 *  high dynamic range and wide gamut"
 * https://opg.optica.org/oe/fulltext.cfm?uri=oe-25-13-15131
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ----------------------------------------------------------------
 * Jzazbz Constants (Safdar 2017)
 * ---------------------------------------------------------------- */

/* Chromatic adaptation coefficients */
static alwan_scalar const JZAZBZ_B = ALWAN_LITERAL(1.15);
static alwan_scalar const JZAZBZ_G = ALWAN_LITERAL(0.66);

/* Perceptual quantizer (PQ) constants from ST 2084 */
static alwan_scalar const JZAZBZ_C1 = ALWAN_LITERAL(0.8359375);              /* 3424/4096 */
static alwan_scalar const JZAZBZ_C2 = ALWAN_LITERAL(18.8515625);             /* 2413/128 */
static alwan_scalar const JZAZBZ_C3 = ALWAN_LITERAL(18.6875);                /* 2392/128 */
static alwan_scalar const JZAZBZ_N  = ALWAN_LITERAL(0.1593017578125);        /* 2610/16384 */
static alwan_scalar const JZAZBZ_P  = ALWAN_LITERAL(134.034375);             /* 1.7 * 2523/32 */

/* Lightness transformation constants */
static alwan_scalar const JZAZBZ_D  = ALWAN_LITERAL(-0.56);
static alwan_scalar const JZAZBZ_D0 = ALWAN_LITERAL(1.6295499532821566e-11);

/* XYZ (D65) to LMS matrix */
static alwan_scalar const XYZ_TO_LMS[9] = {
    ALWAN_LITERAL( 0.41478972), ALWAN_LITERAL( 0.579999),   ALWAN_LITERAL( 0.0146480),
    ALWAN_LITERAL(-0.2015100),  ALWAN_LITERAL( 1.120649),   ALWAN_LITERAL( 0.0531008),
    ALWAN_LITERAL(-0.0166008),  ALWAN_LITERAL( 0.264800),   ALWAN_LITERAL( 0.6684799)
};

/* LMS to XYZ inverse matrix */
static alwan_scalar const LMS_TO_XYZ[9] = {
    ALWAN_LITERAL( 1.9242264357876067),  ALWAN_LITERAL(-1.0047923125953657),  ALWAN_LITERAL( 0.0376514040306180),
    ALWAN_LITERAL( 0.3503167620949991),  ALWAN_LITERAL( 0.7264811939316552),  ALWAN_LITERAL(-0.0653844229480850),
    ALWAN_LITERAL(-0.0909828109828476),  ALWAN_LITERAL(-0.3127282905230740),  ALWAN_LITERAL( 1.5227665613052603)
};

/* LMS' to Izazbz matrix (Safdar 2017) */
static alwan_scalar const LMS_P_TO_IZAZBZ[9] = {
    ALWAN_LITERAL( 0.500000),  ALWAN_LITERAL( 0.500000),  ALWAN_LITERAL( 0.000000),
    ALWAN_LITERAL( 3.524000),  ALWAN_LITERAL(-4.066708),  ALWAN_LITERAL( 0.542708),
    ALWAN_LITERAL( 0.199076),  ALWAN_LITERAL( 1.096799),  ALWAN_LITERAL(-1.295875)
};

/* Izazbz to LMS' inverse matrix */
static alwan_scalar const IZAZBZ_TO_LMS_P[9] = {
    ALWAN_LITERAL( 1.0000000000000000),  ALWAN_LITERAL( 0.1386050432715393),  ALWAN_LITERAL( 0.0580473161561189),
    ALWAN_LITERAL( 1.0000000000000000),  ALWAN_LITERAL(-0.1386050432715393),  ALWAN_LITERAL(-0.0580473161561189),
    ALWAN_LITERAL( 1.0000000000000000),  ALWAN_LITERAL(-0.0960192420263190),  ALWAN_LITERAL(-0.8118918960560390)
};

/* ----------------------------------------------------------------
 * Helper Functions: PQ Transfer Functions
 * ---------------------------------------------------------------- */

/* PQ OETF: linear → encoded (using PQ-like formula with Jzazbz constants) */
static alwan_scalar pq_jz_oetf(alwan_scalar linear) {
    if (linear <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);

    alwan_scalar linear_n = ALWAN_POW(linear / ALWAN_LITERAL(10000.0), JZAZBZ_N);
    alwan_scalar numerator = JZAZBZ_C1 + JZAZBZ_C2 * linear_n;
    alwan_scalar denominator = ALWAN_LITERAL(1.0) + JZAZBZ_C3 * linear_n;
    return ALWAN_POW(numerator / denominator, JZAZBZ_P);
}

/* PQ EOTF: encoded → linear (inverse of PQ OETF) */
static alwan_scalar pq_jz_eotf(alwan_scalar encoded) {
    if (encoded <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);

    alwan_scalar encoded_p = ALWAN_POW(encoded, ALWAN_LITERAL(1.0) / JZAZBZ_P);
    alwan_scalar numerator = encoded_p - JZAZBZ_C1;
    alwan_scalar denominator = JZAZBZ_C2 - JZAZBZ_C3 * encoded_p;

    if (denominator == ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);

    alwan_scalar ratio = numerator / denominator;
    if (ratio < ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);

    return ALWAN_LITERAL(10000.0) * ALWAN_POW(ratio, ALWAN_LITERAL(1.0) / JZAZBZ_N);
}

/* ----------------------------------------------------------------
 * XYZ <-> Jzazbz
 * ---------------------------------------------------------------- */

void alwan_xyz_to_jzazbz(alwan_xyz const *xyz, alwan_jzazbz *jzazbz) {
    /* XYZ should be in absolute luminance (cd/m²) where Y=100 means 100 cd/m²
     * This matches colour-science's XYZ_to_Jzazbz behavior */

    /* Step 1: Chromatic adaptation (D65) */
    alwan_vec3 xyz_adapted;
    xyz_adapted.v[0] = JZAZBZ_B * xyz->x - (JZAZBZ_B - ALWAN_LITERAL(1.0)) * xyz->z;
    xyz_adapted.v[1] = JZAZBZ_G * xyz->y - (JZAZBZ_G - ALWAN_LITERAL(1.0)) * xyz->x;
    xyz_adapted.v[2] = xyz->z;

    /* Step 2: XYZ (adapted) → LMS */
    alwan_vec3 lms;
    lms.v[0] = XYZ_TO_LMS[0] * xyz_adapted.v[0] + XYZ_TO_LMS[1] * xyz_adapted.v[1] + XYZ_TO_LMS[2] * xyz_adapted.v[2];
    lms.v[1] = XYZ_TO_LMS[3] * xyz_adapted.v[0] + XYZ_TO_LMS[4] * xyz_adapted.v[1] + XYZ_TO_LMS[5] * xyz_adapted.v[2];
    lms.v[2] = XYZ_TO_LMS[6] * xyz_adapted.v[0] + XYZ_TO_LMS[7] * xyz_adapted.v[1] + XYZ_TO_LMS[8] * xyz_adapted.v[2];

    /* Step 3: Apply PQ OETF: LMS → LMS' */
    alwan_vec3 lms_p;
    lms_p.v[0] = pq_jz_oetf(lms.v[0]);
    lms_p.v[1] = pq_jz_oetf(lms.v[1]);
    lms_p.v[2] = pq_jz_oetf(lms.v[2]);

    /* Step 4: LMS' → Izazbz */
    alwan_vec3 izazbz;
    izazbz.v[0] = LMS_P_TO_IZAZBZ[0] * lms_p.v[0] + LMS_P_TO_IZAZBZ[1] * lms_p.v[1] + LMS_P_TO_IZAZBZ[2] * lms_p.v[2];
    izazbz.v[1] = LMS_P_TO_IZAZBZ[3] * lms_p.v[0] + LMS_P_TO_IZAZBZ[4] * lms_p.v[1] + LMS_P_TO_IZAZBZ[5] * lms_p.v[2];
    izazbz.v[2] = LMS_P_TO_IZAZBZ[6] * lms_p.v[0] + LMS_P_TO_IZAZBZ[7] * lms_p.v[1] + LMS_P_TO_IZAZBZ[8] * lms_p.v[2];

    /* Step 5: Calculate Jz from Iz */
    alwan_scalar iz = izazbz.v[0];
    jzazbz->Jz = ((ALWAN_LITERAL(1.0) + JZAZBZ_D) * iz) / (ALWAN_LITERAL(1.0) + JZAZBZ_D * iz) - JZAZBZ_D0;
    jzazbz->az = izazbz.v[1];  /* az */
    jzazbz->bz = izazbz.v[2];  /* bz */
}

void alwan_jzazbz_to_xyz(alwan_jzazbz const *jzazbz, alwan_xyz *xyz) {
    /* Step 1: Recover Iz from Jz */
    alwan_scalar jz = jzazbz->Jz;
    alwan_scalar iz = (jz + JZAZBZ_D0) / (ALWAN_LITERAL(1.0) + JZAZBZ_D - JZAZBZ_D * (jz + JZAZBZ_D0));

    /* Step 2: Construct Izazbz */
    alwan_vec3 izazbz;
    izazbz.v[0] = iz;
    izazbz.v[1] = jzazbz->az;  /* az */
    izazbz.v[2] = jzazbz->bz;  /* bz */

    /* Step 3: Izazbz → LMS' */
    alwan_vec3 lms_p;
    lms_p.v[0] = IZAZBZ_TO_LMS_P[0] * izazbz.v[0] + IZAZBZ_TO_LMS_P[1] * izazbz.v[1] + IZAZBZ_TO_LMS_P[2] * izazbz.v[2];
    lms_p.v[1] = IZAZBZ_TO_LMS_P[3] * izazbz.v[0] + IZAZBZ_TO_LMS_P[4] * izazbz.v[1] + IZAZBZ_TO_LMS_P[5] * izazbz.v[2];
    lms_p.v[2] = IZAZBZ_TO_LMS_P[6] * izazbz.v[0] + IZAZBZ_TO_LMS_P[7] * izazbz.v[1] + IZAZBZ_TO_LMS_P[8] * izazbz.v[2];

    /* Step 4: Apply PQ EOTF: LMS' → LMS */
    alwan_vec3 lms;
    lms.v[0] = pq_jz_eotf(lms_p.v[0]);
    lms.v[1] = pq_jz_eotf(lms_p.v[1]);
    lms.v[2] = pq_jz_eotf(lms_p.v[2]);

    /* Step 5: LMS → XYZ (adapted) */
    alwan_vec3 xyz_adapted;
    xyz_adapted.v[0] = LMS_TO_XYZ[0] * lms.v[0] + LMS_TO_XYZ[1] * lms.v[1] + LMS_TO_XYZ[2] * lms.v[2];
    xyz_adapted.v[1] = LMS_TO_XYZ[3] * lms.v[0] + LMS_TO_XYZ[4] * lms.v[1] + LMS_TO_XYZ[5] * lms.v[2];
    xyz_adapted.v[2] = LMS_TO_XYZ[6] * lms.v[0] + LMS_TO_XYZ[7] * lms.v[1] + LMS_TO_XYZ[8] * lms.v[2];

    /* Step 6: Inverse chromatic adaptation - output in absolute luminance (cd/m²) */
    xyz->x = (xyz_adapted.v[0] + (JZAZBZ_B - ALWAN_LITERAL(1.0)) * xyz_adapted.v[2]) / JZAZBZ_B;
    xyz->y = (xyz_adapted.v[1] + (JZAZBZ_G - ALWAN_LITERAL(1.0)) * xyz->x) / JZAZBZ_G;
    xyz->z = xyz_adapted.v[2];
}

/* ----------------------------------------------------------------
 * Jzazbz <-> JzCzhz (Cylindrical)
 * ---------------------------------------------------------------- */

void alwan_jzazbz_to_jzczhz(alwan_jzazbz const *jzazbz, alwan_jzczhz *jzczhz) {
    /* Jz stays the same */
    jzczhz->Jz = jzazbz->Jz;

    /* Cz = sqrt(az^2 + bz^2) */
    jzczhz->Cz = ALWAN_SQRT(jzazbz->az * jzazbz->az + jzazbz->bz * jzazbz->bz);

    /* hz = atan2(bz, az) in radians */
    jzczhz->hz = ALWAN_ATAN2(jzazbz->bz, jzazbz->az);
}

void alwan_jzczhz_to_jzazbz(alwan_jzczhz const *jzczhz, alwan_jzazbz *jzazbz) {
    /* Jz stays the same */
    jzazbz->Jz = jzczhz->Jz;

    /* az = Cz * cos(hz) */
    jzazbz->az = jzczhz->Cz * ALWAN_COS(jzczhz->hz);

    /* bz = Cz * sin(hz) */
    jzazbz->bz = jzczhz->Cz * ALWAN_SIN(jzczhz->hz);
}