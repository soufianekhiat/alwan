/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Hunt Color Appearance Model
 * Based on: Hunt (1991, 1995)
 * "Revised colour-appearance model for related and unrelated colours"
 * Comprehensive but complex historical CAM
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Hunt CAM Constants
 * ---------------------------------------------------------------- */

/* Hunt-Pointer-Estevez matrix */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const M_HPE[9] = {
#include "data/matrices/hpe.csv"
};
ALWAN_DIAG_POP

/* Viewing condition parameters (Nc, Nb) */
static void get_hunt_params(alwan_hunt_surround surround,
                            alwan_scalar *Nc, alwan_scalar *Nb) {
    switch (surround) {
        case ALWAN_HUNT_SURROUND_NORMAL:
            *Nc = ALWAN_LITERAL(1.0);
            *Nb = ALWAN_LITERAL(75.0);
            break;
        case ALWAN_HUNT_SURROUND_DIM:
            *Nc = ALWAN_LITERAL(1.0);
            *Nb = ALWAN_LITERAL(25.0);
            break;
        case ALWAN_HUNT_SURROUND_DARK:
            *Nc = ALWAN_LITERAL(0.7);
            *Nb = ALWAN_LITERAL(10.0);
            break;
        default:
            *Nc = ALWAN_LITERAL(1.0);
            *Nb = ALWAN_LITERAL(75.0);
            break;
    }
}

/* Nonlinear response function f_n */
static alwan_scalar hunt_fn(alwan_scalar x) {
    if (x < ALWAN_LITERAL(0.0)) {
        return -hunt_fn(-x);
    }
    alwan_scalar x_pow = ALWAN_POW(x, ALWAN_LITERAL(0.73));
    return ALWAN_LITERAL(40.0) * x_pow / (x_pow + ALWAN_LITERAL(2.0));
}

/* ----------------------------------------------------------------
 * Hunt Forward Transform: XYZ -> Correlates
 * ---------------------------------------------------------------- */

int alwan_hunt_forward(alwan_hunt_correlates *out,
                       alwan_xyz const *xyz,
                       alwan_hunt_viewing_conditions const *vc) {
    if (!xyz || !vc || !out) {
        return -1;
    }

    /* Get surround parameters */
    alwan_scalar Nc, Nb;
    get_hunt_params(vc->surround, &Nc, &Nb);

    /* Step 1: Compute adaptation factor k */
    alwan_scalar k = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(5.0) * vc->La + ALWAN_LITERAL(1.0));
    alwan_scalar k4 = k * k * k * k;

    /* Step 2: Compute luminance adaptation factor FL */
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * vc->La) +
                      ALWAN_LITERAL(0.1) * (ALWAN_LITERAL(1.0) - k4) * (ALWAN_LITERAL(1.0) - k4) *
                      ALWAN_POW(ALWAN_LITERAL(5.0) * vc->La, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    /* Step 3: Convert XYZ to LMS using HPE matrix */
    alwan_vec3 lms, lms_w;
    lms.v[0] = M_HPE[0] * xyz->x + M_HPE[1] * xyz->y + M_HPE[2] * xyz->z;
    lms.v[1] = M_HPE[3] * xyz->x + M_HPE[4] * xyz->y + M_HPE[5] * xyz->z;
    lms.v[2] = M_HPE[6] * xyz->x + M_HPE[7] * xyz->y + M_HPE[8] * xyz->z;

    lms_w.v[0] = M_HPE[0] * vc->xyz_w.x + M_HPE[1] * vc->xyz_w.y + M_HPE[2] * vc->xyz_w.z;
    lms_w.v[1] = M_HPE[3] * vc->xyz_w.x + M_HPE[4] * vc->xyz_w.y + M_HPE[5] * vc->xyz_w.z;
    lms_w.v[2] = M_HPE[6] * vc->xyz_w.x + M_HPE[7] * vc->xyz_w.y + M_HPE[8] * vc->xyz_w.z;

    /* Step 4: Apply chromatic adaptation with discounting */
    alwan_scalar D = vc->discount_illuminant ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(0.0);
    alwan_vec3 lms_adapted;
    for (int i = 0; i < 3; i++) {
        lms_adapted.v[i] = (D + ALWAN_LITERAL(1.0) - D) * lms.v[i];
    }

    /* Step 5: Apply nonlinear response */
    alwan_vec3 lms_n;
    lms_n.v[0] = hunt_fn(FL * lms_adapted.v[0]);
    lms_n.v[1] = hunt_fn(FL * lms_adapted.v[1]);
    lms_n.v[2] = hunt_fn(FL * lms_adapted.v[2]);

    /* Step 6: Compute opponent color dimensions */
    alwan_scalar C1 = lms_n.v[0] - lms_n.v[1];
    alwan_scalar C2 = lms_n.v[1] - lms_n.v[2];
    alwan_scalar C3 = lms_n.v[2] - lms_n.v[0];

    /* Step 7: Compute hue angle h */
    alwan_scalar h_num = ALWAN_LITERAL(0.5) * (C2 - C3) / ALWAN_LITERAL(4.5);
    alwan_scalar h_den = C1 - C2 / ALWAN_LITERAL(11.0);
    out->h = ALWAN_ATAN2(h_num, h_den) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    if (out->h < ALWAN_LITERAL(0.0)) {
        out->h += ALWAN_LITERAL(360.0);
    }

    /* Step 8: Eccentricity factor omitted (simplified model) */

    /* Step 9: Compute achromatic response A */
    alwan_scalar A = (ALWAN_LITERAL(2.0) * lms_n.v[0] + lms_n.v[1] +
                      ALWAN_LITERAL(0.05) * lms_n.v[2] - ALWAN_LITERAL(3.05) + ALWAN_LITERAL(1.0));

    /* Step 10: Compute brightness Q (simplified) */
    alwan_scalar Yw = vc->xyz_w.y;
    alwan_scalar Yb = vc->Yb;

    alwan_scalar N1 = ALWAN_POW(ALWAN_LITERAL(7.0), ALWAN_LITERAL(0.6));
    alwan_scalar M_total = ALWAN_SQRT(C1 * C1 + C2 * C2);
    out->Q = N1 * (A + M_total / ALWAN_LITERAL(100.0));

    /* Step 11: Compute lightness J (simplified) */
    alwan_scalar Qw = N1 * ALWAN_LITERAL(2.0);  /* Simplified white point brightness */
    if (Qw > ALWAN_LITERAL(1e-10)) {
        out->J = ALWAN_LITERAL(100.0) * ALWAN_POW(out->Q / Qw, ALWAN_LITERAL(0.67));
    } else {
        out->J = ALWAN_LITERAL(0.0);
    }

    /* Step 12: Compute saturation s */
    alwan_scalar M = M_total;
    if (A > ALWAN_LITERAL(1e-10)) {
        out->s = ALWAN_LITERAL(50.0) * M / A;
    } else {
        out->s = ALWAN_LITERAL(0.0);
    }

    /* Step 13: Compute chroma C (simplified) */
    if (out->Q > ALWAN_LITERAL(1e-10)) {
        out->C = ALWAN_LITERAL(2.44) * ALWAN_POW(out->s, ALWAN_LITERAL(0.69)) *
                 ALWAN_POW(out->Q / Qw, Yb / Yw);
    } else {
        out->C = ALWAN_LITERAL(0.0);
    }

    /* Step 14: Compute colourfulness M */
    out->M = ALWAN_POW(FL, ALWAN_LITERAL(0.15)) * out->C;

    return 0;
}

/* Note: Hunt inverse is extremely complex and typically not implemented.
 * It requires iterative numerical methods due to the nonlinear response functions. */
