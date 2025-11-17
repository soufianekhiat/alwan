/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ZCAM (Z CAM) - HDR Color Appearance Model
 * Based on: Safdar et al. (2021) "ZCAM, a colour appearance model based on
 * a high dynamic range uniform colour space"
 * Reference: Optics Express 29(4), 6036-6052
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * ZCAM Constants
 * ---------------------------------------------------------------- */

/* Jzazbz/ZCAM chromatic adaptation parameters */
static alwan_scalar const ZCAM_B = ALWAN_LITERAL(1.15);
static alwan_scalar const ZCAM_G = ALWAN_LITERAL(0.66);

/* PQ (Perceptual Quantizer) constants */
static alwan_scalar const PQ_C1 = ALWAN_LITERAL(0.8359375);        /* 3424/4096 */
static alwan_scalar const PQ_C2 = ALWAN_LITERAL(18.8515625);       /* 2413/128 */
static alwan_scalar const PQ_C3 = ALWAN_LITERAL(18.6875);          /* 2392/128 */
static alwan_scalar const PQ_N  = ALWAN_LITERAL(0.1593017578125);  /* 2610/16384 */
static alwan_scalar const PQ_P  = ALWAN_LITERAL(134.034375);       /* 1.7*2523/32 */
static alwan_scalar const PQ_D  = ALWAN_LITERAL(-0.56);
static alwan_scalar const PQ_D0 = ALWAN_LITERAL(1.6295499532821566e-11);

/* XYZ to LMS matrix (from Jzazbz) */
static alwan_scalar const M_XYZ_TO_LMS[9] = {
    ALWAN_LITERAL( 0.41478972), ALWAN_LITERAL( 0.579999),  ALWAN_LITERAL( 0.014648),
    ALWAN_LITERAL(-0.20151),    ALWAN_LITERAL( 1.120649),  ALWAN_LITERAL( 0.0531008),
    ALWAN_LITERAL(-0.0166008),  ALWAN_LITERAL( 0.2648),    ALWAN_LITERAL( 0.6684799)
};

/* LMS' to Izazbz matrix */
static alwan_scalar const M_LMS_P_TO_IZAZBZ[9] = {
    ALWAN_LITERAL(0.5),       ALWAN_LITERAL(0.5),      ALWAN_LITERAL(0.0),
    ALWAN_LITERAL(3.524),     ALWAN_LITERAL(-4.066708), ALWAN_LITERAL(0.542708),
    ALWAN_LITERAL(0.199076),  ALWAN_LITERAL(1.096799),  ALWAN_LITERAL(-1.295875)
};

/* Inverse matrices (precomputed) */
static alwan_scalar const M_LMS_TO_XYZ[9] = {
    ALWAN_LITERAL( 1.9242264357876069),  ALWAN_LITERAL(-1.0047923125953657), ALWAN_LITERAL( 0.0376514040306180),
    ALWAN_LITERAL( 0.3503167620949991),  ALWAN_LITERAL( 0.7264811939316552), ALWAN_LITERAL(-0.0653844229480850),
    ALWAN_LITERAL(-0.0909828109828476), ALWAN_LITERAL(-0.3127282905230740), ALWAN_LITERAL( 1.5227665613052603)
};

static alwan_scalar const M_IZAZBZ_TO_LMS_P[9] = {
    ALWAN_LITERAL( 1.0000000000000000),  ALWAN_LITERAL( 0.1386050432715393), ALWAN_LITERAL( 0.0580473161561189),
    ALWAN_LITERAL( 1.0000000000000000),  ALWAN_LITERAL(-0.1386050432715393), ALWAN_LITERAL(-0.0580473161561189),
    ALWAN_LITERAL( 1.0000000000000000),  ALWAN_LITERAL(-0.0960192420263190), ALWAN_LITERAL(-0.8118918960560390)
};

/* Get surround parameters Fs, c, Nc, F based on surround type */
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

/* PQ OETF (Optical-Electro Transfer Function) */
static alwan_scalar pq_oetf(alwan_scalar L) {
    alwan_scalar L_norm = L / ALWAN_LITERAL(10000.0);
    alwan_scalar L_pow_n = ALWAN_POW(L_norm, PQ_N);
    alwan_scalar numerator = PQ_C1 + PQ_C2 * L_pow_n;
    alwan_scalar denominator = ALWAN_LITERAL(1.0) + PQ_C3 * L_pow_n;
    return ALWAN_POW(numerator / denominator, PQ_P);
}

/* PQ EOTF (Electro-Optical Transfer Function) - inverse */
static alwan_scalar pq_eotf(alwan_scalar E) {
    alwan_scalar E_pow = ALWAN_POW(E, ALWAN_LITERAL(1.0) / PQ_P);
    alwan_scalar numerator = E_pow - PQ_C1;
    alwan_scalar denominator = PQ_C2 - PQ_C3 * E_pow;

    if (ALWAN_FABS(denominator) < ALWAN_LITERAL(1e-10)) {
        return ALWAN_LITERAL(0.0);
    }

    alwan_scalar L_norm = ALWAN_POW(numerator / denominator, ALWAN_LITERAL(1.0) / PQ_N);
    return L_norm * ALWAN_LITERAL(10000.0);
}

/* Convert Iz to Jz */
static alwan_scalar Iz_to_Jz(alwan_scalar Iz) {
    alwan_scalar numerator = (ALWAN_LITERAL(1.0) + PQ_D) * Iz;
    alwan_scalar denominator = ALWAN_LITERAL(1.0) + PQ_D * Iz;
    return (numerator / denominator) - PQ_D0;
}

/* Convert Jz to Iz (inverse) */
static alwan_scalar Jz_to_Iz(alwan_scalar Jz) {
    alwan_scalar Jz_adj = Jz + PQ_D0;
    alwan_scalar numerator = Jz_adj;
    alwan_scalar denominator = (ALWAN_LITERAL(1.0) + PQ_D) - PQ_D * Jz_adj;
    return numerator / denominator;
}

/* Compute eccentricity factor */
static alwan_scalar compute_eccentricity(alwan_scalar h_degrees) {
    alwan_scalar h_rad = (h_degrees + ALWAN_LITERAL(89.038)) * ALWAN_PI / ALWAN_LITERAL(180.0);
    return ALWAN_LITERAL(1.015) + ALWAN_COS(h_rad);
}

/* ----------------------------------------------------------------
 * ZCAM Forward Transform: XYZ -> Correlates
 * ---------------------------------------------------------------- */

int alwan_zcam_forward(alwan_vec3 const *xyz,
                       alwan_zcam_viewing_conditions const *vc,
                       alwan_zcam_correlates *out) {
    if (!xyz || !vc || !out) {
        return -1;
    }

    /* Get surround parameters */
    alwan_scalar Fs, c, Nc, F;
    get_zcam_surround_params(vc->surround, &Fs, &c, &Nc, &F);

    /* Compute degree of adaptation D */
    alwan_scalar D;
    if (vc->discount_illuminant) {
        D = ALWAN_LITERAL(1.0);
    } else {
        D = F * (ALWAN_LITERAL(1.0) - ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.6) *
                 ALWAN_EXP((-vc->La - ALWAN_LITERAL(42.0)) / ALWAN_LITERAL(92.0)));
        D = alwan_clamp(D, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    }

    /* Step 1: Compute viewing condition factors */
    alwan_scalar Fb = ALWAN_SQRT(vc->Yb / vc->xyz_w.v[1]);
    alwan_scalar FL = ALWAN_LITERAL(0.171) * ALWAN_POW(vc->La, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0)) *
                      (ALWAN_LITERAL(1.0) - ALWAN_EXP(-ALWAN_LITERAL(48.0) / ALWAN_LITERAL(9.0) * vc->La));

    /* Step 2: Chromatic adaptation (simplified - using XYZ directly) */
    alwan_vec3 xyz_adapted;
    xyz_adapted.v[0] = ZCAM_B * xyz->v[0] - (ZCAM_B - ALWAN_LITERAL(1.0)) * xyz->v[2];
    xyz_adapted.v[1] = ZCAM_G * xyz->v[1] - (ZCAM_G - ALWAN_LITERAL(1.0)) * xyz->v[0];
    xyz_adapted.v[2] = xyz->v[2];

    /* Step 3: XYZ -> LMS */
    alwan_vec3 lms;
    lms.v[0] = M_XYZ_TO_LMS[0] * xyz_adapted.v[0] + M_XYZ_TO_LMS[1] * xyz_adapted.v[1] + M_XYZ_TO_LMS[2] * xyz_adapted.v[2];
    lms.v[1] = M_XYZ_TO_LMS[3] * xyz_adapted.v[0] + M_XYZ_TO_LMS[4] * xyz_adapted.v[1] + M_XYZ_TO_LMS[5] * xyz_adapted.v[2];
    lms.v[2] = M_XYZ_TO_LMS[6] * xyz_adapted.v[0] + M_XYZ_TO_LMS[7] * xyz_adapted.v[1] + M_XYZ_TO_LMS[8] * xyz_adapted.v[2];

    /* Step 4: Apply PQ nonlinearity */
    alwan_vec3 lms_p;
    lms_p.v[0] = pq_oetf(lms.v[0]);
    lms_p.v[1] = pq_oetf(lms.v[1]);
    lms_p.v[2] = pq_oetf(lms.v[2]);

    /* Step 5: LMS' -> Izazbz */
    alwan_vec3 Izazbz;
    Izazbz.v[0] = M_LMS_P_TO_IZAZBZ[0] * lms_p.v[0] + M_LMS_P_TO_IZAZBZ[1] * lms_p.v[1] + M_LMS_P_TO_IZAZBZ[2] * lms_p.v[2];
    Izazbz.v[1] = M_LMS_P_TO_IZAZBZ[3] * lms_p.v[0] + M_LMS_P_TO_IZAZBZ[4] * lms_p.v[1] + M_LMS_P_TO_IZAZBZ[5] * lms_p.v[2];
    Izazbz.v[2] = M_LMS_P_TO_IZAZBZ[6] * lms_p.v[0] + M_LMS_P_TO_IZAZBZ[7] * lms_p.v[1] + M_LMS_P_TO_IZAZBZ[8] * lms_p.v[2];

    alwan_scalar Iz = Izazbz.v[0];
    alwan_scalar az = Izazbz.v[1];
    alwan_scalar bz = Izazbz.v[2];

    /* Step 6: Compute white point Izw */
    alwan_vec3 xyz_w_adapted;
    xyz_w_adapted.v[0] = ZCAM_B * vc->xyz_w.v[0] - (ZCAM_B - ALWAN_LITERAL(1.0)) * vc->xyz_w.v[2];
    xyz_w_adapted.v[1] = ZCAM_G * vc->xyz_w.v[1] - (ZCAM_G - ALWAN_LITERAL(1.0)) * vc->xyz_w.v[0];
    xyz_w_adapted.v[2] = vc->xyz_w.v[2];

    alwan_vec3 lms_w;
    lms_w.v[0] = M_XYZ_TO_LMS[0] * xyz_w_adapted.v[0] + M_XYZ_TO_LMS[1] * xyz_w_adapted.v[1] + M_XYZ_TO_LMS[2] * xyz_w_adapted.v[2];
    lms_w.v[1] = M_XYZ_TO_LMS[3] * xyz_w_adapted.v[0] + M_XYZ_TO_LMS[4] * xyz_w_adapted.v[1] + M_XYZ_TO_LMS[5] * xyz_w_adapted.v[2];
    lms_w.v[2] = M_XYZ_TO_LMS[6] * xyz_w_adapted.v[0] + M_XYZ_TO_LMS[7] * xyz_w_adapted.v[1] + M_XYZ_TO_LMS[8] * xyz_w_adapted.v[2];

    alwan_vec3 lms_w_p;
    lms_w_p.v[0] = pq_oetf(lms_w.v[0]);
    lms_w_p.v[1] = pq_oetf(lms_w.v[1]);
    lms_w_p.v[2] = pq_oetf(lms_w.v[2]);

    alwan_scalar Izw = M_LMS_P_TO_IZAZBZ[0] * lms_w_p.v[0] + M_LMS_P_TO_IZAZBZ[1] * lms_w_p.v[1] + M_LMS_P_TO_IZAZBZ[2] * lms_w_p.v[2];

    /* Step 7: Compute hue angle */
    out->hz = ALWAN_ATAN2(bz, az) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    if (out->hz < ALWAN_LITERAL(0.0)) {
        out->hz += ALWAN_LITERAL(360.0);
    }

    /* Step 8: Compute eccentricity factor */
    alwan_scalar ez = compute_eccentricity(out->hz);

    /* Step 9: Compute brightness Qz */
    alwan_scalar Iz_pow = ALWAN_POW(Iz, ALWAN_LITERAL(1.6) * Fs / ALWAN_POW(Fb, ALWAN_LITERAL(0.12)));
    out->Qz = ALWAN_LITERAL(2700.0) * Iz_pow * ALWAN_POW(Fs, ALWAN_LITERAL(2.2)) *
              ALWAN_SQRT(Fb) * ALWAN_POW(FL, ALWAN_LITERAL(0.2));

    /* Step 10: Compute white point brightness Qzw */
    alwan_scalar Izw_pow = ALWAN_POW(Izw, ALWAN_LITERAL(1.6) * Fs / ALWAN_POW(Fb, ALWAN_LITERAL(0.12)));
    alwan_scalar Qzw = ALWAN_LITERAL(2700.0) * Izw_pow * ALWAN_POW(Fs, ALWAN_LITERAL(2.2)) *
                       ALWAN_SQRT(Fb) * ALWAN_POW(FL, ALWAN_LITERAL(0.2));

    /* Step 11: Compute lightness Jz */
    out->Jz = ALWAN_LITERAL(100.0) * (out->Qz / Qzw);

    /* Step 12: Compute colorfulness Mz */
    alwan_scalar chroma_component = ALWAN_POW(az * az + bz * bz, ALWAN_LITERAL(0.37));
    out->Mz = ALWAN_LITERAL(100.0) * chroma_component *
              ALWAN_POW(ez, ALWAN_LITERAL(0.068)) *
              ALWAN_POW(FL, ALWAN_LITERAL(0.2)) /
              (ALWAN_POW(Izw, ALWAN_LITERAL(0.78)) * ALWAN_POW(Fb, ALWAN_LITERAL(0.1)));

    /* Step 13: Compute chroma Cz */
    out->Cz = ALWAN_LITERAL(100.0) * out->Mz / Qzw;

    /* Step 14: Compute saturation Sz */
    if (out->Qz > ALWAN_LITERAL(1e-10)) {
        out->Sz = ALWAN_LITERAL(100.0) * ALWAN_POW(out->Mz / out->Qz, ALWAN_LITERAL(0.5)) *
                  ALWAN_POW(FL, ALWAN_LITERAL(0.6));
    } else {
        out->Sz = ALWAN_LITERAL(0.0);
    }

    /* Step 15: Compute vividness Vz */
    alwan_scalar J_diff = out->Jz - ALWAN_LITERAL(58.0);
    out->Vz = ALWAN_SQRT(J_diff * J_diff + ALWAN_LITERAL(3.4) * out->Cz * out->Cz);

    /* Step 16: Compute blackness Kz */
    out->Kz = ALWAN_LITERAL(100.0) -
              ALWAN_LITERAL(0.8) * ALWAN_SQRT(out->Jz * out->Jz + ALWAN_LITERAL(8.0) * out->Cz * out->Cz);

    /* Step 17: Compute whiteness Wz */
    alwan_scalar J_diff_w = ALWAN_LITERAL(100.0) - out->Jz;
    out->Wz = ALWAN_LITERAL(100.0) - ALWAN_SQRT(J_diff_w * J_diff_w + out->Cz * out->Cz);

    return 0;
}

/* ----------------------------------------------------------------
 * ZCAM Inverse Transform: Correlates -> XYZ (simplified)
 * Note: Full inverse requires iterative solution. This is approximate.
 * ---------------------------------------------------------------- */

int alwan_zcam_inverse(alwan_zcam_correlates const *correlates,
                       alwan_zcam_viewing_conditions const *vc,
                       alwan_vec3 *xyz) {
    if (!correlates || !vc || !xyz) {
        return -1;
    }

    /* Get surround parameters */
    alwan_scalar Fs, c, Nc, F;
    get_zcam_surround_params(vc->surround, &Fs, &c, &Nc, &F);

    /* Recover Iz from Jz (simplified - assumes we have Jz) */
    alwan_scalar Jz_to_use = correlates->Jz;

    /* Convert Jz to Iz approximation */
    alwan_scalar Iz = Jz_to_Iz(Jz_to_use / ALWAN_LITERAL(100.0));

    /* Recover az, bz from Mz and hz */
    alwan_scalar hz_rad = correlates->hz * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar Mz_norm = correlates->Mz / ALWAN_LITERAL(100.0);

    /* Approximate inverse of colorfulness formula */
    alwan_scalar chroma_component_target = Mz_norm;  /* Simplified */
    alwan_scalar chroma_squared = ALWAN_POW(chroma_component_target, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.37));

    alwan_scalar az = ALWAN_SQRT(chroma_squared) * ALWAN_COS(hz_rad);
    alwan_scalar bz = ALWAN_SQRT(chroma_squared) * ALWAN_SIN(hz_rad);

    /* Izazbz -> LMS' */
    alwan_vec3 Izazbz = {{Iz, az, bz}};
    alwan_vec3 lms_p;
    lms_p.v[0] = M_IZAZBZ_TO_LMS_P[0] * Izazbz.v[0] + M_IZAZBZ_TO_LMS_P[1] * Izazbz.v[1] + M_IZAZBZ_TO_LMS_P[2] * Izazbz.v[2];
    lms_p.v[1] = M_IZAZBZ_TO_LMS_P[3] * Izazbz.v[0] + M_IZAZBZ_TO_LMS_P[4] * Izazbz.v[1] + M_IZAZBZ_TO_LMS_P[5] * Izazbz.v[2];
    lms_p.v[2] = M_IZAZBZ_TO_LMS_P[6] * Izazbz.v[0] + M_IZAZBZ_TO_LMS_P[7] * Izazbz.v[1] + M_IZAZBZ_TO_LMS_P[8] * Izazbz.v[2];

    /* Apply inverse PQ */
    alwan_vec3 lms;
    lms.v[0] = pq_eotf(lms_p.v[0]);
    lms.v[1] = pq_eotf(lms_p.v[1]);
    lms.v[2] = pq_eotf(lms_p.v[2]);

    /* LMS -> XYZ */
    alwan_vec3 xyz_adapted;
    xyz_adapted.v[0] = M_LMS_TO_XYZ[0] * lms.v[0] + M_LMS_TO_XYZ[1] * lms.v[1] + M_LMS_TO_XYZ[2] * lms.v[2];
    xyz_adapted.v[1] = M_LMS_TO_XYZ[3] * lms.v[0] + M_LMS_TO_XYZ[4] * lms.v[1] + M_LMS_TO_XYZ[5] * lms.v[2];
    xyz_adapted.v[2] = M_LMS_TO_XYZ[6] * lms.v[0] + M_LMS_TO_XYZ[7] * lms.v[1] + M_LMS_TO_XYZ[8] * lms.v[2];

    /* Inverse chromatic adaptation (simplified) */
    xyz->v[2] = xyz_adapted.v[2];
    xyz->v[0] = (xyz_adapted.v[0] + (ZCAM_B - ALWAN_LITERAL(1.0)) * xyz->v[2]) / ZCAM_B;
    xyz->v[1] = (xyz_adapted.v[1] + (ZCAM_G - ALWAN_LITERAL(1.0)) * xyz->v[0]) / ZCAM_G;

    return 0;
}

/* ----------------------------------------------------------------
 * ZCAM to UCS (Uniform Color Space) for color difference
 * ---------------------------------------------------------------- */

int alwan_zcam_to_ucs(alwan_zcam_correlates const *correlates,
                      alwan_vec3 *Jab_out) {
    if (!correlates || !Jab_out) {
        return -1;
    }

    /* ZCAM UCS uses Jz, Mz, hz to create Jab coordinates */
    Jab_out->v[0] = correlates->Jz;

    alwan_scalar hz_rad = correlates->hz * ALWAN_PI / ALWAN_LITERAL(180.0);
    Jab_out->v[1] = correlates->Mz * ALWAN_COS(hz_rad);
    Jab_out->v[2] = correlates->Mz * ALWAN_SIN(hz_rad);

    return 0;
}