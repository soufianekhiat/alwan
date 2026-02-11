/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * CIECAM02 Color Appearance Model Implementation
 * Based on CIE 159:2004 specification
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * CIECAM02 Constants and Helper Functions
 * ---------------------------------------------------------------- */

/* Hunt-Pointer-Estevez (HPE) matrices for XYZ <-> LMS conversion
 * Data defined in alwan_data.c */
#define M_HPE g_hpe
#define M_HPE_inv g_hpe_inv

/* Get surround parameters F, c, Nc based on surround type */
static void get_surround_params(alwan_ciecam02_surround surround, alwan_scalar *F, alwan_scalar *c, alwan_scalar *Nc) {
    switch (surround) {
        case ALWAN_CIECAM02_SURROUND_AVERAGE:
            *F = ALWAN_LITERAL(1.0);
            *c = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            break;
        case ALWAN_CIECAM02_SURROUND_DIM:
            *F = ALWAN_LITERAL(0.9);
            *c = ALWAN_LITERAL(0.59);
            *Nc = ALWAN_LITERAL(0.95);
            break;
        case ALWAN_CIECAM02_SURROUND_DARK:
            *F = ALWAN_LITERAL(0.8);
            *c = ALWAN_LITERAL(0.525);
            *Nc = ALWAN_LITERAL(0.8);
            break;
        default:
            *F = ALWAN_LITERAL(1.0);
            *c = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            break;
    }
}

/* Compute degree of adaptation D */
static alwan_scalar compute_D(alwan_scalar F, alwan_scalar La, int discount_illuminant) {
    if (discount_illuminant) {
        return ALWAN_LITERAL(1.0);
    }
    alwan_scalar D = F * (ALWAN_LITERAL(1.0) - ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.6) *
                     ALWAN_EXP((-La - ALWAN_LITERAL(42.0)) / ALWAN_LITERAL(92.0)));
    return alwan_clamp(D, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
}

/* Post-adaptation nonlinear response compression */
static alwan_scalar post_adaptation_nonlinear(alwan_scalar x) {
    alwan_scalar const FL_pow = ALWAN_LITERAL(0.42);

    alwan_scalar x_abs = ALWAN_ABS(x);
    alwan_scalar x_pow = ALWAN_POW(x_abs, FL_pow);
    alwan_scalar result = ALWAN_LITERAL(400.0) * x_pow / (ALWAN_LITERAL(27.13) + x_pow) + ALWAN_LITERAL(0.1);

    return (x < ALWAN_LITERAL(0.0)) ? -result : result;
}

/* Inverse of post-adaptation nonlinear response */
static alwan_scalar post_adaptation_nonlinear_inv(alwan_scalar x) {
    alwan_scalar const FL_pow_inv = ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.42);

    /* First subtract the additive constant */
    alwan_scalar y = x - ALWAN_LITERAL(0.1);
    alwan_scalar y_abs = ALWAN_ABS(y);

    /* Invert: x^0.42 = (27.13 * y) / (400 - y) */
    alwan_scalar result = ALWAN_POW((ALWAN_LITERAL(27.13) * y_abs) /
                             (ALWAN_LITERAL(400.0) - y_abs), FL_pow_inv);

    return (x < ALWAN_LITERAL(0.0)) ? -result : result;
}

/* Hue angle to hue quadrature H */
static alwan_scalar hue_to_quadrature(alwan_scalar h) {
    /* Unique hue data: i, hi, ei, Hi */
    alwan_scalar const h_i[5] = {
        ALWAN_LITERAL(20.14), ALWAN_LITERAL(90.00),
        ALWAN_LITERAL(164.25), ALWAN_LITERAL(237.53),
        ALWAN_LITERAL(380.14)  /* 20.14 + 360 for wraparound */
    };
    alwan_scalar const e_i[4] = {
        ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.7),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.2)
    };
    alwan_scalar const H_i[5] = {
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(100.0),
        ALWAN_LITERAL(200.0), ALWAN_LITERAL(300.0),
        ALWAN_LITERAL(400.0)
    };

    /* Normalize h to [0, 360) - use fmod to avoid infinite loops with infinity */
    h = ALWAN_FMOD(h, ALWAN_LITERAL(360.0));
    if (h < ALWAN_LITERAL(0.0)) h += ALWAN_LITERAL(360.0);

    /* Find which quadrant h is in */
    int i;
    for (i = 0; i < 4; i++) {
        if (h < h_i[i + 1]) break;
    }

    /* Compute H using eccentricity factors */
    alwan_scalar h_diff = h - h_i[i];
    alwan_scalar e_i_cur = e_i[i];
    alwan_scalar e_i_next = (i == 3) ? e_i[0] : e_i[i + 1];
    alwan_scalar h_i_diff = h_i[i + 1] - h_i[i];

    alwan_scalar H = H_i[i] + (ALWAN_LITERAL(100.0) * h_diff * e_i_cur) /
               (h_diff * e_i_cur + (h_i_diff - h_diff) * e_i_next);

    return H;
}

/* ----------------------------------------------------------------
 * CIECAM02 Forward Transform
 * ---------------------------------------------------------------- */

int alwan_ciecam02_forward(alwan_ciecam02_correlates *out,
                            alwan_xyz const *xyz,
                            alwan_ciecam02_viewing_conditions const *vc) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }

    /* Validate viewing conditions to prevent division by zero */
    if (vc->white_xyz.y <= ALWAN_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_DIVZERO;
    }

    /* Step 1: Get surround parameters */
    alwan_scalar F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    /* Step 2: Compute degree of adaptation D */
    alwan_scalar D = compute_D(F, vc->adapting_luminance, vc->discount_illuminant);

    /* Step 3: Transform XYZ to cone responses (LMS) using HPE matrix */
    alwan_scalar RGB[3];
    RGB[0] = M_HPE[0] * xyz->x + M_HPE[1] * xyz->y + M_HPE[2] * xyz->z;
    RGB[1] = M_HPE[3] * xyz->x + M_HPE[4] * xyz->y + M_HPE[5] * xyz->z;
    RGB[2] = M_HPE[6] * xyz->x + M_HPE[7] * xyz->y + M_HPE[8] * xyz->z;

    /* Step 4: Transform white point to cone responses */
    alwan_scalar RGB_w[3];
    RGB_w[0] = M_HPE[0] * vc->white_xyz.x + M_HPE[1] * vc->white_xyz.y + M_HPE[2] * vc->white_xyz.z;
    RGB_w[1] = M_HPE[3] * vc->white_xyz.x + M_HPE[4] * vc->white_xyz.y + M_HPE[5] * vc->white_xyz.z;
    RGB_w[2] = M_HPE[6] * vc->white_xyz.x + M_HPE[7] * vc->white_xyz.y + M_HPE[8] * vc->white_xyz.z;

    /* Step 5: Compute adapted cone responses (chromatic adaptation) */
    alwan_scalar RGB_c[3];
    for (int i = 0; i < 3; i++) {
        alwan_scalar D_RGB_div = D * (vc->white_xyz.y / RGB_w[i]) + ALWAN_LITERAL(1.0) - D;
        RGB_c[i] = RGB[i] * D_RGB_div;
    }

    /* Step 6: Compute luminance level adaptation factor FL */
    alwan_scalar k = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(5.0) * vc->adapting_luminance + ALWAN_LITERAL(1.0));
    alwan_scalar k4 = k * k * k * k;
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * vc->adapting_luminance) +
                ALWAN_LITERAL(0.1) * (ALWAN_LITERAL(1.0) - k4) * (ALWAN_LITERAL(1.0) - k4) *
                ALWAN_POW(ALWAN_LITERAL(5.0) * vc->adapting_luminance, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    /* Step 7: Transform to Hunt-Pointer-Estevez fundamentals */
    alwan_scalar RGB_a[3];
    for (int i = 0; i < 3; i++) {
        RGB_a[i] = post_adaptation_nonlinear(FL * RGB_c[i] / ALWAN_LITERAL(100.0));
    }

    /* Step 7a: Compute Nbb (background induction factor) */
    alwan_scalar n = vc->background_luminance / vc->white_xyz.y;  /* Yb / Yw */
    alwan_scalar Nbb = ALWAN_LITERAL(0.725) * ALWAN_POW(ALWAN_LITERAL(1.0) / n, ALWAN_LITERAL(0.2));
    alwan_scalar Ncb = Nbb;  /* For CIECAM02, Nbb = Ncb */

    /* Step 8: Compute achromatic response A */
    alwan_scalar A = (ALWAN_LITERAL(2.0) * RGB_a[0] + RGB_a[1] +
                ALWAN_LITERAL(0.05) * RGB_a[2] - ALWAN_LITERAL(0.305)) * Nbb;

    /* Step 9: Compute hue angle h */
    alwan_scalar a = RGB_a[0] - ALWAN_LITERAL(12.0) * RGB_a[1] / ALWAN_LITERAL(11.0) +
               RGB_a[2] / ALWAN_LITERAL(11.0);
    alwan_scalar b = (RGB_a[0] + RGB_a[1] - ALWAN_LITERAL(2.0) * RGB_a[2]) / ALWAN_LITERAL(9.0);
    alwan_scalar h = ALWAN_ATAN2(b, a) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    if (h < ALWAN_LITERAL(0.0)) h += ALWAN_LITERAL(360.0);

    /* Step 10: Compute eccentricity et and hue quadrature H */
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));
    alwan_scalar H = hue_to_quadrature(h);

    /* Step 11: Compute lightness J */
    alwan_scalar RGB_aw[3];
    for (int i = 0; i < 3; i++) {
        alwan_scalar D_RGB_w_div = D * (vc->white_xyz.y / RGB_w[i]) + ALWAN_LITERAL(1.0) - D;
        alwan_scalar RGB_cw = RGB_w[i] * D_RGB_w_div;
        RGB_aw[i] = post_adaptation_nonlinear(FL * RGB_cw / ALWAN_LITERAL(100.0));
    }
    alwan_scalar A_w = (ALWAN_LITERAL(2.0) * RGB_aw[0] + RGB_aw[1] +
                  ALWAN_LITERAL(0.05) * RGB_aw[2] - ALWAN_LITERAL(0.305)) * Nbb;

    alwan_scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);
    alwan_scalar J = ALWAN_LITERAL(100.0) * ALWAN_POW(A / A_w, c * z);

    /* Step 12: Compute brightness Q */
    alwan_scalar Q = (ALWAN_LITERAL(4.0) / c) * ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               (A_w + ALWAN_LITERAL(4.0)) * ALWAN_POW(FL, ALWAN_LITERAL(0.25));

    /* Step 13: Compute chroma C */
    alwan_scalar t = (ALWAN_LITERAL(50000.0) / ALWAN_LITERAL(13.0) * Nc * Ncb * et *
                ALWAN_SQRT(a * a + b * b)) /
               (RGB_a[0] + RGB_a[1] + ALWAN_LITERAL(21.0) / ALWAN_LITERAL(20.0) * RGB_a[2]);
    alwan_scalar C = ALWAN_POW(t, ALWAN_LITERAL(0.9)) *
               ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73));

    /* Step 14: Compute colorfulness M and saturation s */
    alwan_scalar M = C * ALWAN_POW(FL, ALWAN_LITERAL(0.25));
    alwan_scalar s = ALWAN_LITERAL(100.0) * ALWAN_SQRT(M / Q);

    /* Fill output */
    out->J = J;
    out->C = C;
    out->h = h;
    out->s = s;
    out->Q = Q;
    out->M = M;
    out->H = H;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CIECAM02 Inverse Transform
 * ---------------------------------------------------------------- */

int alwan_ciecam02_inverse(alwan_xyz *xyz_out,
                            alwan_ciecam02_correlates const *correlates,
                            alwan_ciecam02_viewing_conditions const *vc) {
    if (!xyz_out || !correlates || !vc) {
        return ALWAN_E_INVALID;
    }

    /* Validate viewing conditions to prevent division by zero */
    if (vc->white_xyz.y <= ALWAN_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_DIVZERO;
    }

    /* Extract J, C, h from correlates */
    alwan_scalar J = correlates->J;
    alwan_scalar C = correlates->C;
    alwan_scalar h = correlates->h;

    /* Step 1: Get surround parameters */
    alwan_scalar F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    /* Step 2: Compute degree of adaptation D */
    alwan_scalar D = compute_D(F, vc->adapting_luminance, vc->discount_illuminant);

    /* Step 3: Compute FL */
    alwan_scalar k = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(5.0) * vc->adapting_luminance + ALWAN_LITERAL(1.0));
    alwan_scalar k4 = k * k * k * k;
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * vc->adapting_luminance) +
                ALWAN_LITERAL(0.1) * (ALWAN_LITERAL(1.0) - k4) * (ALWAN_LITERAL(1.0) - k4) *
                ALWAN_POW(ALWAN_LITERAL(5.0) * vc->adapting_luminance, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    /* Step 4: Compute white point adapted cone responses */
    alwan_scalar RGB_w[3];
    RGB_w[0] = M_HPE[0] * vc->white_xyz.x + M_HPE[1] * vc->white_xyz.y + M_HPE[2] * vc->white_xyz.z;
    RGB_w[1] = M_HPE[3] * vc->white_xyz.x + M_HPE[4] * vc->white_xyz.y + M_HPE[5] * vc->white_xyz.z;
    RGB_w[2] = M_HPE[6] * vc->white_xyz.x + M_HPE[7] * vc->white_xyz.y + M_HPE[8] * vc->white_xyz.z;

    alwan_scalar RGB_aw[3];
    for (int i = 0; i < 3; i++) {
        alwan_scalar D_RGB_w_div = D * (vc->white_xyz.y / RGB_w[i]) + ALWAN_LITERAL(1.0) - D;
        alwan_scalar RGB_cw = RGB_w[i] * D_RGB_w_div;
        RGB_aw[i] = post_adaptation_nonlinear(FL * RGB_cw / ALWAN_LITERAL(100.0));
    }
    /* Compute Nbb and related parameters */
    alwan_scalar n = vc->background_luminance / vc->white_xyz.y;
    alwan_scalar Nbb = ALWAN_LITERAL(0.725) * ALWAN_POW(ALWAN_LITERAL(1.0) / n, ALWAN_LITERAL(0.2));
    alwan_scalar Ncb = Nbb;
    alwan_scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);

    alwan_scalar A_w = (ALWAN_LITERAL(2.0) * RGB_aw[0] + RGB_aw[1] +
                  ALWAN_LITERAL(0.05) * RGB_aw[2] - ALWAN_LITERAL(0.305)) * Nbb;

    /* Step 5: Compute achromatic response A from J */
    alwan_scalar A = A_w * ALWAN_POW(J / ALWAN_LITERAL(100.0), ALWAN_LITERAL(1.0) / (c * z));

    /* Step 6: Compute t from C */
    alwan_scalar t = ALWAN_POW(C / (ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
                              ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73))),
                        ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.9));

    /* Step 7: Compute eccentricity et */
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));

    /* Step 8: Compute a and b from h and t */
    /* Following colour-science's opponent_colour_dimensions_inverse implementation */
    alwan_scalar h_rad = h * ALWAN_PI / ALWAN_LITERAL(180.0);

    /* Compute P_n components as in colour-science CAM16_to_XYZ */
    alwan_scalar P_1 = (ALWAN_LITERAL(50000.0) / ALWAN_LITERAL(13.0)) * Nc * Ncb * et / t;
    alwan_scalar P_2 = A / Nbb + ALWAN_LITERAL(0.305);
    alwan_scalar P_3 = ALWAN_LITERAL(21.0) / ALWAN_LITERAL(20.0);

    alwan_scalar sin_h = ALWAN_SIN(h_rad);
    alwan_scalar cos_h = ALWAN_COS(h_rad);

    /* Numerator: numerator = P_2 * (2 + P_3) * (460 / 1403) */
    alwan_scalar numerator = P_2 * (ALWAN_LITERAL(2.0) + P_3) * (ALWAN_LITERAL(460.0) / ALWAN_LITERAL(1403.0));

    alwan_scalar a, b;
    /* Choose formula based on hue angle to avoid division by small numbers */
    if (ALWAN_ABS(sin_h) >= ALWAN_ABS(cos_h)) {
        /* Case 1: |sin(h)| >= |cos(h)| - solve for b first */
        /* P_4 = P_1 / sin(h) */
        alwan_scalar P_4 = P_1 / sin_h;

        /* b = numerator / (P_4 + (2 + P_3) * (220/1403) * (cos(h)/sin(h)) - (27/1403) + P_3 * (6300/1403)) */
        alwan_scalar denom = P_4 +
                            (ALWAN_LITERAL(2.0) + P_3) * (ALWAN_LITERAL(220.0) / ALWAN_LITERAL(1403.0)) * (cos_h / sin_h) -
                            (ALWAN_LITERAL(27.0) / ALWAN_LITERAL(1403.0)) +
                            P_3 * (ALWAN_LITERAL(6300.0) / ALWAN_LITERAL(1403.0));
        b = numerator / denom;
        a = b * (cos_h / sin_h);
    } else {
        /* Case 2: |sin(h)| < |cos(h)| - solve for a first */
        /* P_5 = P_1 / cos(h) */
        alwan_scalar P_5 = P_1 / cos_h;

        /* a = numerator / (P_5 + (2 + P_3) * (220/1403) - ((27/1403) - P_3 * (6300/1403)) * (sin(h)/cos(h))) */
        alwan_scalar denom = P_5 +
                            (ALWAN_LITERAL(2.0) + P_3) * (ALWAN_LITERAL(220.0) / ALWAN_LITERAL(1403.0)) -
                            ((ALWAN_LITERAL(27.0) / ALWAN_LITERAL(1403.0)) - P_3 * (ALWAN_LITERAL(6300.0) / ALWAN_LITERAL(1403.0))) * (sin_h / cos_h);
        a = numerator / denom;
        b = a * (sin_h / cos_h);
    }

    /* Step 9: Compute RGB_a from a, b, and A */
    alwan_scalar RGB_a[3];
    RGB_a[0] = (ALWAN_LITERAL(460.0) * P_2 + ALWAN_LITERAL(451.0) * a + ALWAN_LITERAL(288.0) * b) / ALWAN_LITERAL(1403.0);
    RGB_a[1] = (ALWAN_LITERAL(460.0) * P_2 - ALWAN_LITERAL(891.0) * a - ALWAN_LITERAL(261.0) * b) / ALWAN_LITERAL(1403.0);
    RGB_a[2] = (ALWAN_LITERAL(460.0) * P_2 - ALWAN_LITERAL(220.0) * a - ALWAN_LITERAL(6300.0) * b) / ALWAN_LITERAL(1403.0);

    /* Step 10: Apply inverse post-adaptation nonlinearity */
    alwan_scalar RGB_c[3];
    for (int i = 0; i < 3; i++) {
        RGB_c[i] = ALWAN_LITERAL(100.0) / FL * post_adaptation_nonlinear_inv(RGB_a[i]);
    }

    /* Step 11: Apply inverse chromatic adaptation */
    alwan_scalar RGB[3];
    for (int i = 0; i < 3; i++) {
        alwan_scalar D_RGB_div = D * (vc->white_xyz.y / RGB_w[i]) + ALWAN_LITERAL(1.0) - D;
        RGB[i] = RGB_c[i] / D_RGB_div;
    }

    /* Step 12: Transform back to XYZ using inverse HPE matrix */
    xyz_out->x = M_HPE_inv[0] * RGB[0] + M_HPE_inv[1] * RGB[1] + M_HPE_inv[2] * RGB[2];
    xyz_out->y = M_HPE_inv[3] * RGB[0] + M_HPE_inv[4] * RGB[1] + M_HPE_inv[5] * RGB[2];
    xyz_out->z = M_HPE_inv[6] * RGB[0] + M_HPE_inv[7] * RGB[1] + M_HPE_inv[8] * RGB[2];

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CAM16 Color Appearance Model
 * ---------------------------------------------------------------- */

/* CAM16 adaptation matrix (CAT16) for XYZ to LMS conversion */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const M_CAT16[9] = {
#include "data/matrices/cat_cat16.csv"
};
ALWAN_DIAG_POP

/* Inverse CAT16 matrix for LMS to XYZ conversion */
static alwan_scalar const M_CAT16_inv[9] = {
    ALWAN_LITERAL( 1.8620678550872327), ALWAN_LITERAL(-1.0112546305316843), ALWAN_LITERAL( 0.14918677544445175),
    ALWAN_LITERAL( 0.38752654323613722), ALWAN_LITERAL( 0.62144744193147528), ALWAN_LITERAL(-0.0089739851676125162),
    ALWAN_LITERAL(-0.015841498849333859), ALWAN_LITERAL(-0.03412293802851557), ALWAN_LITERAL( 1.0499644368778496)
};

/* Get surround parameters F, c, Nc for CAM16 (same as CIECAM02) */
static void get_cam16_surround_params(alwan_cam16_surround surround, alwan_scalar *F, alwan_scalar *c, alwan_scalar *Nc) {
    switch (surround) {
        case ALWAN_CAM16_SURROUND_AVERAGE:
            *F = ALWAN_LITERAL(1.0);
            *c = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            break;
        case ALWAN_CAM16_SURROUND_DIM:
            *F = ALWAN_LITERAL(0.9);
            *c = ALWAN_LITERAL(0.59);
            *Nc = ALWAN_LITERAL(0.95);
            break;
        case ALWAN_CAM16_SURROUND_DARK:
            *F = ALWAN_LITERAL(0.8);
            *c = ALWAN_LITERAL(0.525);
            *Nc = ALWAN_LITERAL(0.8);
            break;
        default:
            *F = ALWAN_LITERAL(1.0);
            *c = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            break;
    }
}

/* CAM16 forward transform */
int alwan_cam16_forward(alwan_cam16_correlates *out,
                        alwan_xyz const *xyz,
                        alwan_cam16_viewing_conditions const *vc) {
    if (!out || !xyz || !vc) {
        return ALWAN_E_INVALID;
    }

    /* Validate viewing conditions to prevent division by zero */
    if (vc->white_xyz.y <= ALWAN_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_DIVZERO;
    }

    /* Step 1: Get surround parameters */
    alwan_scalar F, c, Nc;
    get_cam16_surround_params(vc->surround, &F, &c, &Nc);

    /* Step 2: Compute degree of adaptation D */
    alwan_scalar D = compute_D(F, vc->adapting_luminance, vc->discount_illuminant);

    /* Step 3: Transform XYZ to cone responses (RGB) using CAT16 matrix */
    alwan_scalar RGB[3];
    RGB[0] = M_CAT16[0] * xyz->x + M_CAT16[1] * xyz->y + M_CAT16[2] * xyz->z;
    RGB[1] = M_CAT16[3] * xyz->x + M_CAT16[4] * xyz->y + M_CAT16[5] * xyz->z;
    RGB[2] = M_CAT16[6] * xyz->x + M_CAT16[7] * xyz->y + M_CAT16[8] * xyz->z;

    /* Step 4: Transform white point to cone responses */
    alwan_scalar RGB_w[3];
    RGB_w[0] = M_CAT16[0] * vc->white_xyz.x + M_CAT16[1] * vc->white_xyz.y + M_CAT16[2] * vc->white_xyz.z;
    RGB_w[1] = M_CAT16[3] * vc->white_xyz.x + M_CAT16[4] * vc->white_xyz.y + M_CAT16[5] * vc->white_xyz.z;
    RGB_w[2] = M_CAT16[6] * vc->white_xyz.x + M_CAT16[7] * vc->white_xyz.y + M_CAT16[8] * vc->white_xyz.z;

    /* Step 5: Compute adapted cone responses (chromatic adaptation) */
    alwan_scalar RGB_c[3];
    for (int i = 0; i < 3; i++) {
        alwan_scalar D_RGB_div = D * (vc->white_xyz.y / RGB_w[i]) + ALWAN_LITERAL(1.0) - D;
        RGB_c[i] = RGB[i] * D_RGB_div;
    }

    /* Step 6: Compute luminance level adaptation factor FL */
    alwan_scalar k = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(5.0) * vc->adapting_luminance + ALWAN_LITERAL(1.0));
    alwan_scalar k4 = k * k * k * k;
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * vc->adapting_luminance) +
                ALWAN_LITERAL(0.1) * (ALWAN_LITERAL(1.0) - k4) * (ALWAN_LITERAL(1.0) - k4) *
                ALWAN_POW(ALWAN_LITERAL(5.0) * vc->adapting_luminance, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    /* Step 7: Apply post-adaptation nonlinear response */
    alwan_scalar RGB_a[3];
    for (int i = 0; i < 3; i++) {
        RGB_a[i] = post_adaptation_nonlinear(FL * RGB_c[i] / ALWAN_LITERAL(100.0));
    }

    /* Step 8: Compute background parameters */
    alwan_scalar n = vc->background_luminance / vc->white_xyz.y;
    alwan_scalar Nbb = ALWAN_LITERAL(0.725) * ALWAN_POW(ALWAN_LITERAL(1.0) / n, ALWAN_LITERAL(0.2));
    alwan_scalar Ncb = Nbb;
    alwan_scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);

    /* Step 9: Compute achromatic response A */
    alwan_scalar A = (ALWAN_LITERAL(2.0) * RGB_a[0] + RGB_a[1] +
                ALWAN_LITERAL(0.05) * RGB_a[2] - ALWAN_LITERAL(0.305)) * Nbb;

    /* Step 10: Compute hue angle h */
    alwan_scalar a = RGB_a[0] - ALWAN_LITERAL(12.0) * RGB_a[1] / ALWAN_LITERAL(11.0) +
               RGB_a[2] / ALWAN_LITERAL(11.0);
    alwan_scalar b = (RGB_a[0] + RGB_a[1] - ALWAN_LITERAL(2.0) * RGB_a[2]) / ALWAN_LITERAL(9.0);
    alwan_scalar h = ALWAN_ATAN2(b, a) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    if (h < ALWAN_LITERAL(0.0)) h += ALWAN_LITERAL(360.0);

    /* Step 11: Compute eccentricity et and hue quadrature H */
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));
    alwan_scalar H = hue_to_quadrature(h);

    /* Step 12: Compute lightness J */
    alwan_scalar RGB_aw[3];
    for (int i = 0; i < 3; i++) {
        alwan_scalar D_RGB_w_div = D * (vc->white_xyz.y / RGB_w[i]) + ALWAN_LITERAL(1.0) - D;
        alwan_scalar RGB_cw = RGB_w[i] * D_RGB_w_div;
        RGB_aw[i] = post_adaptation_nonlinear(FL * RGB_cw / ALWAN_LITERAL(100.0));
    }
    alwan_scalar A_w = (ALWAN_LITERAL(2.0) * RGB_aw[0] + RGB_aw[1] +
                  ALWAN_LITERAL(0.05) * RGB_aw[2] - ALWAN_LITERAL(0.305)) * Nbb;

    alwan_scalar J = ALWAN_LITERAL(100.0) * ALWAN_POW(A / A_w, c * z);

    /* Step 13: Compute brightness Q */
    alwan_scalar Q = (ALWAN_LITERAL(4.0) / c) * ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               (A_w + ALWAN_LITERAL(4.0)) * ALWAN_POW(FL, ALWAN_LITERAL(0.25));

    /* Step 14: Compute chroma C */
    alwan_scalar t = (ALWAN_LITERAL(50000.0) / ALWAN_LITERAL(13.0) * Nc * Ncb * et *
                ALWAN_SQRT(a * a + b * b)) /
               (RGB_a[0] + RGB_a[1] + ALWAN_LITERAL(21.0) / ALWAN_LITERAL(20.0) * RGB_a[2]);
    alwan_scalar C = ALWAN_POW(t, ALWAN_LITERAL(0.9)) *
               ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73));

    /* Step 15: Compute colorfulness M and saturation s */
    alwan_scalar M = C * ALWAN_POW(FL, ALWAN_LITERAL(0.25));
    alwan_scalar s = ALWAN_LITERAL(100.0) * ALWAN_SQRT(M / Q);

    /* Fill output */
    out->J = J;
    out->C = C;
    out->h = h;
    out->s = s;
    out->Q = Q;
    out->M = M;
    out->H = H;

    return ALWAN_OK;
}

/* CAM16 inverse transform */
int alwan_cam16_inverse(alwan_xyz *xyz_out,
                        alwan_cam16_correlates const *correlates,
                        alwan_cam16_viewing_conditions const *vc) {
    if (!xyz_out || !correlates || !vc) {
        return ALWAN_E_INVALID;
    }

    /* Validate viewing conditions to prevent division by zero */
    if (vc->white_xyz.y <= ALWAN_LITERAL(0.0) ||
        vc->background_luminance <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_DIVZERO;
    }

    /* Extract J, C, h from correlates */
    alwan_scalar J = correlates->J;
    alwan_scalar C = correlates->C;
    alwan_scalar h = correlates->h;

    /* Step 1: Get surround parameters */
    alwan_scalar F, c, Nc;
    get_cam16_surround_params(vc->surround, &F, &c, &Nc);

    /* Step 2: Compute degree of adaptation D */
    alwan_scalar D = compute_D(F, vc->adapting_luminance, vc->discount_illuminant);

    /* Step 3: Compute FL */
    alwan_scalar k = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(5.0) * vc->adapting_luminance + ALWAN_LITERAL(1.0));
    alwan_scalar k4 = k * k * k * k;
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * vc->adapting_luminance) +
                ALWAN_LITERAL(0.1) * (ALWAN_LITERAL(1.0) - k4) * (ALWAN_LITERAL(1.0) - k4) *
                ALWAN_POW(ALWAN_LITERAL(5.0) * vc->adapting_luminance, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    /* Step 4: Compute white point adapted cone responses */
    alwan_scalar RGB_w[3];
    RGB_w[0] = M_CAT16[0] * vc->white_xyz.x + M_CAT16[1] * vc->white_xyz.y + M_CAT16[2] * vc->white_xyz.z;
    RGB_w[1] = M_CAT16[3] * vc->white_xyz.x + M_CAT16[4] * vc->white_xyz.y + M_CAT16[5] * vc->white_xyz.z;
    RGB_w[2] = M_CAT16[6] * vc->white_xyz.x + M_CAT16[7] * vc->white_xyz.y + M_CAT16[8] * vc->white_xyz.z;

    alwan_scalar RGB_aw[3];
    for (int i = 0; i < 3; i++) {
        alwan_scalar D_RGB_w_div = D * (vc->white_xyz.y / RGB_w[i]) + ALWAN_LITERAL(1.0) - D;
        alwan_scalar RGB_cw = RGB_w[i] * D_RGB_w_div;
        RGB_aw[i] = post_adaptation_nonlinear(FL * RGB_cw / ALWAN_LITERAL(100.0));
    }

    /* Compute background parameters */
    alwan_scalar n = vc->background_luminance / vc->white_xyz.y;
    alwan_scalar Nbb = ALWAN_LITERAL(0.725) * ALWAN_POW(ALWAN_LITERAL(1.0) / n, ALWAN_LITERAL(0.2));
    alwan_scalar Ncb = Nbb;
    alwan_scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);

    alwan_scalar A_w = (ALWAN_LITERAL(2.0) * RGB_aw[0] + RGB_aw[1] +
                  ALWAN_LITERAL(0.05) * RGB_aw[2] - ALWAN_LITERAL(0.305)) * Nbb;

    /* Step 5: Compute achromatic response A from J */
    alwan_scalar A = A_w * ALWAN_POW(J / ALWAN_LITERAL(100.0), ALWAN_LITERAL(1.0) / (c * z));

    /* Step 6: Compute t from C */
    alwan_scalar t = ALWAN_POW(C / (ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
                              ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73))),
                        ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.9));

    /* Step 7: Compute eccentricity et */
    alwan_scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));

    /* Step 8: Compute a and b from h and t */
    /* Following colour-science's opponent_colour_dimensions_inverse implementation */
    alwan_scalar h_rad = h * ALWAN_PI / ALWAN_LITERAL(180.0);

    /* Compute P_n components as in colour-science CAM16_to_XYZ */
    alwan_scalar P_1 = (ALWAN_LITERAL(50000.0) / ALWAN_LITERAL(13.0)) * Nc * Ncb * et / t;
    alwan_scalar P_2 = A / Nbb + ALWAN_LITERAL(0.305);
    alwan_scalar P_3 = ALWAN_LITERAL(21.0) / ALWAN_LITERAL(20.0);

    alwan_scalar sin_h = ALWAN_SIN(h_rad);
    alwan_scalar cos_h = ALWAN_COS(h_rad);

    /* Numerator: numerator = P_2 * (2 + P_3) * (460 / 1403) */
    alwan_scalar numerator = P_2 * (ALWAN_LITERAL(2.0) + P_3) * (ALWAN_LITERAL(460.0) / ALWAN_LITERAL(1403.0));

    alwan_scalar a, b;
    /* Choose formula based on hue angle to avoid division by small numbers */
    if (ALWAN_ABS(sin_h) >= ALWAN_ABS(cos_h)) {
        /* Case 1: |sin(h)| >= |cos(h)| - solve for b first */
        /* P_4 = P_1 / sin(h) */
        alwan_scalar P_4 = P_1 / sin_h;

        /* b = numerator / (P_4 + (2 + P_3) * (220/1403) * (cos(h)/sin(h)) - (27/1403) + P_3 * (6300/1403)) */
        alwan_scalar denom = P_4 +
                            (ALWAN_LITERAL(2.0) + P_3) * (ALWAN_LITERAL(220.0) / ALWAN_LITERAL(1403.0)) * (cos_h / sin_h) -
                            (ALWAN_LITERAL(27.0) / ALWAN_LITERAL(1403.0)) +
                            P_3 * (ALWAN_LITERAL(6300.0) / ALWAN_LITERAL(1403.0));
        b = numerator / denom;
        a = b * (cos_h / sin_h);
    } else {
        /* Case 2: |sin(h)| < |cos(h)| - solve for a first */
        /* P_5 = P_1 / cos(h) */
        alwan_scalar P_5 = P_1 / cos_h;

        /* a = numerator / (P_5 + (2 + P_3) * (220/1403) - ((27/1403) - P_3 * (6300/1403)) * (sin(h)/cos(h))) */
        alwan_scalar denom = P_5 +
                            (ALWAN_LITERAL(2.0) + P_3) * (ALWAN_LITERAL(220.0) / ALWAN_LITERAL(1403.0)) -
                            ((ALWAN_LITERAL(27.0) / ALWAN_LITERAL(1403.0)) - P_3 * (ALWAN_LITERAL(6300.0) / ALWAN_LITERAL(1403.0))) * (sin_h / cos_h);
        a = numerator / denom;
        b = a * (sin_h / cos_h);
    }

    /* Step 9: Compute RGB_a from a, b, and A */
    alwan_scalar RGB_a[3];
    RGB_a[0] = (ALWAN_LITERAL(460.0) * P_2 + ALWAN_LITERAL(451.0) * a + ALWAN_LITERAL(288.0) * b) / ALWAN_LITERAL(1403.0);
    RGB_a[1] = (ALWAN_LITERAL(460.0) * P_2 - ALWAN_LITERAL(891.0) * a - ALWAN_LITERAL(261.0) * b) / ALWAN_LITERAL(1403.0);
    RGB_a[2] = (ALWAN_LITERAL(460.0) * P_2 - ALWAN_LITERAL(220.0) * a - ALWAN_LITERAL(6300.0) * b) / ALWAN_LITERAL(1403.0);

    /* Step 10: Apply inverse post-adaptation nonlinearity */
    alwan_scalar RGB_c[3];
    for (int i = 0; i < 3; i++) {
        RGB_c[i] = ALWAN_LITERAL(100.0) / FL * post_adaptation_nonlinear_inv(RGB_a[i]);
    }

    /* Step 11: Apply inverse chromatic adaptation */
    alwan_scalar RGB[3];
    for (int i = 0; i < 3; i++) {
        alwan_scalar D_RGB_div = D * (vc->white_xyz.y / RGB_w[i]) + ALWAN_LITERAL(1.0) - D;
        RGB[i] = RGB_c[i] / D_RGB_div;
    }

    /* Step 12: Transform back to XYZ using inverse CAT16 matrix */
    xyz_out->x = M_CAT16_inv[0] * RGB[0] + M_CAT16_inv[1] * RGB[1] + M_CAT16_inv[2] * RGB[2];
    xyz_out->y = M_CAT16_inv[3] * RGB[0] + M_CAT16_inv[4] * RGB[1] + M_CAT16_inv[5] * RGB[2];
    xyz_out->z = M_CAT16_inv[6] * RGB[0] + M_CAT16_inv[7] * RGB[1] + M_CAT16_inv[8] * RGB[2];

    return ALWAN_OK;
}

/* CAM16-UCS forward transform (JMh -> Jab) */
int alwan_cam16_to_ucs(alwan_cam_jab *Jab_out,
                       alwan_cam16_correlates const *correlates) {
    if (!Jab_out || !correlates) {
        return ALWAN_E_INVALID;
    }

    /* Extract J, M, h */
    alwan_scalar J = correlates->J;
    alwan_scalar M = correlates->M;
    alwan_scalar h = correlates->h;

    /* Compute CAM16-UCS coordinates */
    alwan_scalar J_prime = ALWAN_LITERAL(1.7) * J / (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.007) * J);
    alwan_scalar M_prime = ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.0228) * ALWAN_LN(ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.0228) * M);

    /* Convert to Cartesian coordinates */
    alwan_scalar h_rad = h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar a_prime = M_prime * ALWAN_COS(h_rad);
    alwan_scalar b_prime = M_prime * ALWAN_SIN(h_rad);

    Jab_out->J = J_prime;
    Jab_out->a = a_prime;
    Jab_out->b = b_prime;

    return ALWAN_OK;
}

/* CAM16-UCS inverse transform (Jab -> JMh) */
int alwan_cam16_from_ucs(alwan_cam16_correlates *correlates_out,
                         alwan_cam_jab const *Jab) {
    if (!correlates_out || !Jab) {
        return ALWAN_E_INVALID;
    }

    /* Extract J', a', b' */
    alwan_scalar J_prime = Jab->J;
    alwan_scalar a_prime = Jab->a;
    alwan_scalar b_prime = Jab->b;

    /* Compute J from J' */
    alwan_scalar J = J_prime / (ALWAN_LITERAL(1.7) - ALWAN_LITERAL(0.007) * J_prime);

    /* Compute M from M' */
    alwan_scalar M_prime = ALWAN_SQRT(a_prime * a_prime + b_prime * b_prime);
    alwan_scalar M = (ALWAN_EXP(M_prime * ALWAN_LITERAL(0.0228)) - ALWAN_LITERAL(1.0)) / ALWAN_LITERAL(0.0228);

    /* Compute h from a', b' */
    alwan_scalar h = ALWAN_ATAN2(b_prime, a_prime) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    if (h < ALWAN_LITERAL(0.0)) h += ALWAN_LITERAL(360.0);

    /* Fill output (only J, M, h are valid; others set to 0) */
    correlates_out->J = J;
    correlates_out->M = M;
    correlates_out->h = h;
    correlates_out->C = ALWAN_LITERAL(0.0);
    correlates_out->s = ALWAN_LITERAL(0.0);
    correlates_out->Q = ALWAN_LITERAL(0.0);
    correlates_out->H = ALWAN_LITERAL(0.0);

    return ALWAN_OK;
}
