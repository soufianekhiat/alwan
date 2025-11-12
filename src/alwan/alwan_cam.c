/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
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

/* Hunt-Pointer-Estevez (HPE) matrix for XYZ to LMS (cone response) conversion */
static Scalar const M_HPE[9] = {
    ALWAN_LITERAL( 0.38971),  ALWAN_LITERAL( 0.68898), ALWAN_LITERAL(-0.07868),
    ALWAN_LITERAL(-0.22981),  ALWAN_LITERAL( 1.18340), ALWAN_LITERAL( 0.04641),
    ALWAN_LITERAL( 0.00000),  ALWAN_LITERAL( 0.00000), ALWAN_LITERAL( 1.00000)
};

/* Inverse HPE matrix for LMS to XYZ conversion */
static Scalar const M_HPE_inv[9] = {
    ALWAN_LITERAL( 1.910197),  ALWAN_LITERAL(-1.112124), ALWAN_LITERAL( 0.201908),
    ALWAN_LITERAL( 0.370950),  ALWAN_LITERAL( 0.629054), ALWAN_LITERAL(-0.000008),
    ALWAN_LITERAL( 0.000000),  ALWAN_LITERAL( 0.000000), ALWAN_LITERAL( 1.000000)
};

/* Get surround parameters F, c, Nc based on surround type */
static void get_surround_params(alwan_ciecam02_surround surround, Scalar *F, Scalar *c, Scalar *Nc) {
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
static Scalar compute_D(Scalar F, Scalar La, int discount_illuminant) {
    if (discount_illuminant) {
        return ALWAN_LITERAL(1.0);
    }
    Scalar D = F * (ALWAN_LITERAL(1.0) - ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.6) *
                     ALWAN_EXP((-La - ALWAN_LITERAL(42.0)) / ALWAN_LITERAL(92.0)));
    return alwan_clamp(D, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
}

/* Post-adaptation nonlinear response compression */
static Scalar post_adaptation_nonlinear(Scalar x) {
    Scalar const FL_pow = ALWAN_LITERAL(0.42);
    Scalar const FL_scale = ALWAN_LITERAL(100.0) / ALWAN_LITERAL(27.13);

    Scalar x_abs = ALWAN_FABS(x);
    Scalar x_pow = ALWAN_POW(x_abs * FL_scale, FL_pow);
    Scalar result = (ALWAN_LITERAL(400.0) * x_pow) / (x_pow + ALWAN_LITERAL(27.13));

    return (x < ALWAN_LITERAL(0.0)) ? -result : result;
}

/* Inverse of post-adaptation nonlinear response */
static Scalar post_adaptation_nonlinear_inv(Scalar x) {
    Scalar const FL_pow_inv = ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.42);
    Scalar const FL_scale = ALWAN_LITERAL(100.0) / ALWAN_LITERAL(27.13);

    Scalar x_abs = ALWAN_FABS(x);
    Scalar x_pow = ALWAN_POW((ALWAN_LITERAL(27.13) * x_abs) /
                             (ALWAN_LITERAL(400.0) - x_abs), FL_pow_inv);
    Scalar result = x_pow / FL_scale;

    return (x < ALWAN_LITERAL(0.0)) ? -result : result;
}

/* Hue angle to hue quadrature H */
static Scalar hue_to_quadrature(Scalar h) {
    /* Unique hue data: i, hi, ei, Hi */
    Scalar const h_i[5] = {
        ALWAN_LITERAL(20.14), ALWAN_LITERAL(90.00),
        ALWAN_LITERAL(164.25), ALWAN_LITERAL(237.53),
        ALWAN_LITERAL(380.14)  /* 20.14 + 360 for wraparound */
    };
    Scalar const e_i[4] = {
        ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.7),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.2)
    };
    Scalar const H_i[5] = {
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(100.0),
        ALWAN_LITERAL(200.0), ALWAN_LITERAL(300.0),
        ALWAN_LITERAL(400.0)
    };

    /* Normalize h to [0, 360) */
    while (h < ALWAN_LITERAL(0.0)) h += ALWAN_LITERAL(360.0);
    while (h >= ALWAN_LITERAL(360.0)) h -= ALWAN_LITERAL(360.0);

    /* Find which quadrant h is in */
    int i;
    for (i = 0; i < 4; i++) {
        if (h < h_i[i + 1]) break;
    }

    /* Compute H using eccentricity factors */
    Scalar h_diff = h - h_i[i];
    Scalar e_i_cur = e_i[i];
    Scalar e_i_next = (i == 3) ? e_i[0] : e_i[i + 1];
    Scalar h_i_diff = h_i[i + 1] - h_i[i];

    Scalar H = H_i[i] + (ALWAN_LITERAL(100.0) * h_diff * e_i_cur) /
               (h_diff * e_i_cur + (h_i_diff - h_diff) * e_i_next);

    return H;
}

/* ----------------------------------------------------------------
 * CIECAM02 Forward Transform
 * ---------------------------------------------------------------- */

int alwan_ciecam02_forward(alwan_vec3 const *xyz,
                            alwan_ciecam02_viewing_conditions const *vc,
                            alwan_ciecam02_correlates *out) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Step 1: Get surround parameters */
    Scalar F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    /* Step 2: Compute degree of adaptation D */
    Scalar D = compute_D(F, vc->adapting_luminance, vc->discount_illuminant);

    /* Step 3: Transform XYZ to cone responses (LMS) using HPE matrix */
    Scalar RGB[3];
    RGB[0] = M_HPE[0] * xyz->v[0] + M_HPE[1] * xyz->v[1] + M_HPE[2] * xyz->v[2];
    RGB[1] = M_HPE[3] * xyz->v[0] + M_HPE[4] * xyz->v[1] + M_HPE[5] * xyz->v[2];
    RGB[2] = M_HPE[6] * xyz->v[0] + M_HPE[7] * xyz->v[1] + M_HPE[8] * xyz->v[2];

    /* Step 4: Transform white point to cone responses */
    Scalar RGB_w[3];
    RGB_w[0] = M_HPE[0] * vc->white_xyz.v[0] + M_HPE[1] * vc->white_xyz.v[1] + M_HPE[2] * vc->white_xyz.v[2];
    RGB_w[1] = M_HPE[3] * vc->white_xyz.v[0] + M_HPE[4] * vc->white_xyz.v[1] + M_HPE[5] * vc->white_xyz.v[2];
    RGB_w[2] = M_HPE[6] * vc->white_xyz.v[0] + M_HPE[7] * vc->white_xyz.v[1] + M_HPE[8] * vc->white_xyz.v[2];

    /* Step 5: Compute adapted cone responses (chromatic adaptation) */
    Scalar RGB_c[3];
    for (int i = 0; i < 3; i++) {
        Scalar D_RGB_div = D * (vc->white_xyz.v[1] / RGB_w[i]) + ALWAN_LITERAL(1.0) - D;
        RGB_c[i] = RGB[i] * D_RGB_div;
    }

    /* Step 6: Compute luminance level adaptation factor FL */
    Scalar k = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(5.0) * vc->adapting_luminance + ALWAN_LITERAL(1.0));
    Scalar k4 = k * k * k * k;
    Scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * vc->adapting_luminance) +
                ALWAN_LITERAL(0.1) * (ALWAN_LITERAL(1.0) - k4) * (ALWAN_LITERAL(1.0) - k4) *
                ALWAN_POW(ALWAN_LITERAL(5.0) * vc->adapting_luminance, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    /* Step 7: Transform to Hunt-Pointer-Estevez fundamentals */
    Scalar RGB_a[3];
    for (int i = 0; i < 3; i++) {
        RGB_a[i] = post_adaptation_nonlinear(FL * RGB_c[i] / ALWAN_LITERAL(100.0));
    }

    /* Step 7a: Compute Nbb (background induction factor) */
    Scalar n = vc->background_luminance / vc->white_xyz.v[1];  /* Yb / Yw */
    Scalar Nbb = ALWAN_LITERAL(0.725) * ALWAN_POW(ALWAN_LITERAL(1.0) / n, ALWAN_LITERAL(0.2));
    Scalar Ncb = Nbb;  /* For CIECAM02, Nbb = Ncb */

    /* Step 8: Compute achromatic response A */
    Scalar A = (ALWAN_LITERAL(2.0) * RGB_a[0] + RGB_a[1] +
                ALWAN_LITERAL(0.05) * RGB_a[2] - ALWAN_LITERAL(0.305)) * Nbb;

    /* Step 9: Compute hue angle h */
    Scalar a = RGB_a[0] - ALWAN_LITERAL(12.0) * RGB_a[1] / ALWAN_LITERAL(11.0) +
               RGB_a[2] / ALWAN_LITERAL(11.0);
    Scalar b = (RGB_a[0] + RGB_a[1] - ALWAN_LITERAL(2.0) * RGB_a[2]) / ALWAN_LITERAL(9.0);
    Scalar h = ALWAN_ATAN2(b, a) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    if (h < ALWAN_LITERAL(0.0)) h += ALWAN_LITERAL(360.0);

    /* Step 10: Compute eccentricity et and hue quadrature H */
    Scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));
    Scalar H = hue_to_quadrature(h);

    /* Step 11: Compute lightness J */
    Scalar RGB_aw[3];
    for (int i = 0; i < 3; i++) {
        Scalar D_RGB_w_div = D * (vc->white_xyz.v[1] / RGB_w[i]) + ALWAN_LITERAL(1.0) - D;
        Scalar RGB_cw = RGB_w[i] * D_RGB_w_div;
        RGB_aw[i] = post_adaptation_nonlinear(FL * RGB_cw / ALWAN_LITERAL(100.0));
    }
    Scalar A_w = (ALWAN_LITERAL(2.0) * RGB_aw[0] + RGB_aw[1] +
                  ALWAN_LITERAL(0.05) * RGB_aw[2] - ALWAN_LITERAL(0.305)) * Nbb;

    Scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);
    Scalar J = ALWAN_LITERAL(100.0) * ALWAN_POW(A / A_w, c * z);

    /* Step 12: Compute brightness Q */
    Scalar Q = (ALWAN_LITERAL(4.0) / c) * ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               (A_w + ALWAN_LITERAL(4.0)) * ALWAN_POW(FL, ALWAN_LITERAL(0.25));

    /* Step 13: Compute chroma C */
    Scalar t = (ALWAN_LITERAL(50000.0) / ALWAN_LITERAL(13.0) * Nc * Ncb * et *
                ALWAN_SQRT(a * a + b * b)) /
               (RGB_a[0] + RGB_a[1] + ALWAN_LITERAL(21.0) / ALWAN_LITERAL(20.0) * RGB_a[2]);
    Scalar C = ALWAN_POW(t, ALWAN_LITERAL(0.9)) *
               ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
               ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73));

    /* Step 14: Compute colorfulness M and saturation s */
    Scalar M = C * ALWAN_POW(FL, ALWAN_LITERAL(0.25));
    Scalar s = ALWAN_LITERAL(100.0) * ALWAN_SQRT(M / Q);

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

int alwan_ciecam02_inverse(alwan_ciecam02_correlates const *correlates,
                            alwan_ciecam02_viewing_conditions const *vc,
                            alwan_vec3 *xyz_out) {
    if (!correlates || !vc || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    /* Extract J, C, h from correlates */
    Scalar J = correlates->J;
    Scalar C = correlates->C;
    Scalar h = correlates->h;

    /* Step 1: Get surround parameters */
    Scalar F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    /* Step 2: Compute degree of adaptation D */
    Scalar D = compute_D(F, vc->adapting_luminance, vc->discount_illuminant);

    /* Step 3: Compute FL */
    Scalar k = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(5.0) * vc->adapting_luminance + ALWAN_LITERAL(1.0));
    Scalar k4 = k * k * k * k;
    Scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * vc->adapting_luminance) +
                ALWAN_LITERAL(0.1) * (ALWAN_LITERAL(1.0) - k4) * (ALWAN_LITERAL(1.0) - k4) *
                ALWAN_POW(ALWAN_LITERAL(5.0) * vc->adapting_luminance, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    /* Step 4: Compute white point adapted cone responses */
    Scalar RGB_w[3];
    RGB_w[0] = M_HPE[0] * vc->white_xyz.v[0] + M_HPE[1] * vc->white_xyz.v[1] + M_HPE[2] * vc->white_xyz.v[2];
    RGB_w[1] = M_HPE[3] * vc->white_xyz.v[0] + M_HPE[4] * vc->white_xyz.v[1] + M_HPE[5] * vc->white_xyz.v[2];
    RGB_w[2] = M_HPE[6] * vc->white_xyz.v[0] + M_HPE[7] * vc->white_xyz.v[1] + M_HPE[8] * vc->white_xyz.v[2];

    Scalar RGB_aw[3];
    for (int i = 0; i < 3; i++) {
        Scalar D_RGB_w_div = D * (vc->white_xyz.v[1] / RGB_w[i]) + ALWAN_LITERAL(1.0) - D;
        Scalar RGB_cw = RGB_w[i] * D_RGB_w_div;
        RGB_aw[i] = post_adaptation_nonlinear(FL * RGB_cw / ALWAN_LITERAL(100.0));
    }
    /* Compute Nbb and related parameters */
    Scalar n = vc->background_luminance / vc->white_xyz.v[1];
    Scalar Nbb = ALWAN_LITERAL(0.725) * ALWAN_POW(ALWAN_LITERAL(1.0) / n, ALWAN_LITERAL(0.2));
    Scalar Ncb = Nbb;
    Scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);

    Scalar A_w = (ALWAN_LITERAL(2.0) * RGB_aw[0] + RGB_aw[1] +
                  ALWAN_LITERAL(0.05) * RGB_aw[2] - ALWAN_LITERAL(0.305)) * Nbb;

    /* Step 5: Compute achromatic response A from J */
    Scalar A = A_w * ALWAN_POW(J / ALWAN_LITERAL(100.0), ALWAN_LITERAL(1.0) / (c * z));

    /* Step 6: Compute t from C */
    Scalar t = ALWAN_POW(C / (ALWAN_SQRT(J / ALWAN_LITERAL(100.0)) *
                              ALWAN_POW(ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), n), ALWAN_LITERAL(0.73))),
                        ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.9));

    /* Step 7: Compute eccentricity et */
    Scalar et = ALWAN_LITERAL(0.25) * (ALWAN_COS(h * ALWAN_PI / ALWAN_LITERAL(180.0) + ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(3.8));

    /* Step 8: Compute a and b from h and t */
    Scalar h_rad = h * ALWAN_PI / ALWAN_LITERAL(180.0);
    Scalar p1 = (ALWAN_LITERAL(50000.0) / ALWAN_LITERAL(13.0)) * Nc * Ncb * et;
    Scalar p2 = A / Nbb + ALWAN_LITERAL(0.305);
    Scalar p3 = ALWAN_LITERAL(21.0) / ALWAN_LITERAL(20.0);

    /* Solve for a and b */
    Scalar sin_h = ALWAN_SIN(h_rad);
    Scalar cos_h = ALWAN_COS(h_rad);

    Scalar a, b;
    if (ALWAN_FABS(sin_h) >= ALWAN_FABS(cos_h)) {
        b = (p2 * (ALWAN_LITERAL(2.0) + p3) * (ALWAN_LITERAL(460.0) / ALWAN_LITERAL(1403.0))) /
            (p1 / t + (ALWAN_LITERAL(2.0) + p3) * (ALWAN_LITERAL(220.0) / ALWAN_LITERAL(1403.0)) * (cos_h / sin_h) -
             (ALWAN_LITERAL(27.0) / ALWAN_LITERAL(1403.0)) + p3 * (ALWAN_LITERAL(6300.0) / ALWAN_LITERAL(1403.0)));
        a = b * (cos_h / sin_h);
    } else {
        a = (p2 * (ALWAN_LITERAL(2.0) + p3) * (ALWAN_LITERAL(460.0) / ALWAN_LITERAL(1403.0))) /
            (p1 / t + (ALWAN_LITERAL(2.0) + p3) * (ALWAN_LITERAL(220.0) / ALWAN_LITERAL(1403.0)) -
             ((ALWAN_LITERAL(27.0) / ALWAN_LITERAL(1403.0)) - p3 * (ALWAN_LITERAL(6300.0) / ALWAN_LITERAL(1403.0))) * (sin_h / cos_h));
        b = a * (sin_h / cos_h);
    }

    /* Step 9: Compute RGB_a from a, b, and A */
    Scalar RGB_a[3];
    RGB_a[0] = (ALWAN_LITERAL(460.0) * p2 + ALWAN_LITERAL(451.0) * a + ALWAN_LITERAL(288.0) * b) / ALWAN_LITERAL(1403.0);
    RGB_a[1] = (ALWAN_LITERAL(460.0) * p2 - ALWAN_LITERAL(891.0) * a - ALWAN_LITERAL(261.0) * b) / ALWAN_LITERAL(1403.0);
    RGB_a[2] = (ALWAN_LITERAL(460.0) * p2 - ALWAN_LITERAL(220.0) * a - ALWAN_LITERAL(6300.0) * b) / ALWAN_LITERAL(1403.0);

    /* Step 10: Apply inverse post-adaptation nonlinearity */
    Scalar RGB_c[3];
    for (int i = 0; i < 3; i++) {
        RGB_c[i] = ALWAN_LITERAL(100.0) / FL * post_adaptation_nonlinear_inv(RGB_a[i]);
    }

    /* Step 11: Apply inverse chromatic adaptation */
    Scalar RGB[3];
    for (int i = 0; i < 3; i++) {
        Scalar D_RGB_div = D * (vc->white_xyz.v[1] / RGB_w[i]) + ALWAN_LITERAL(1.0) - D;
        RGB[i] = RGB_c[i] / D_RGB_div;
    }

    /* Step 12: Transform back to XYZ using inverse HPE matrix */
    xyz_out->v[0] = M_HPE_inv[0] * RGB[0] + M_HPE_inv[1] * RGB[1] + M_HPE_inv[2] * RGB[2];
    xyz_out->v[1] = M_HPE_inv[3] * RGB[0] + M_HPE_inv[4] * RGB[1] + M_HPE_inv[5] * RGB[2];
    xyz_out->v[2] = M_HPE_inv[6] * RGB[0] + M_HPE_inv[7] * RGB[1] + M_HPE_inv[8] * RGB[2];

    return ALWAN_OK;
}
