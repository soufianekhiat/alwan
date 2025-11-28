/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Hellwig2022 Color Appearance Model Implementation
 * Based on Hellwig and Fairchild (2022)
 * "Predicting lightness, chroma, and hue using IAM and CAM frameworks"
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Hellwig2022 Constants
 * ---------------------------------------------------------------- */

/* CAT16 matrix for chromatic adaptation (same as CAM16, from colour-science) */
static alwan_scalar const M_CAT16[9] = {
#include "data/matrices/cat_cat16.csv"
};

/* Inverse CAT16 matrix (from colour-science) */
static alwan_scalar const M_CAT16_inv[9] = {
#include "data/matrices/cat_cat16_inv.csv"
};

/* NOTE: Hellwig2022 does NOT use a post-adaptation matrix like CAM16.
 * Nonlinear compression is applied directly to the chromatically adapted RGB_c values. */

/* Get surround parameters F, c, Nc based on surround type */
static void get_surround_params(alwan_hellwig2022_surround surround, alwan_scalar *F, alwan_scalar *c, alwan_scalar *Nc) {
    switch (surround) {
        case ALWAN_HELLWIG2022_SURROUND_AVERAGE:
            *F = ALWAN_LITERAL(1.0);
            *c = ALWAN_LITERAL(0.69);
            *Nc = ALWAN_LITERAL(1.0);
            break;
        case ALWAN_HELLWIG2022_SURROUND_DIM:
            *F = ALWAN_LITERAL(0.9);
            *c = ALWAN_LITERAL(0.59);
            *Nc = ALWAN_LITERAL(0.95);
            break;
        case ALWAN_HELLWIG2022_SURROUND_DARK:
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

/* Post-adaptation nonlinear response compression for Hellwig2022 */
static alwan_scalar post_adaptation_nonlinear(alwan_scalar x, alwan_scalar FL) {
    alwan_scalar const FL_pow = ALWAN_LITERAL(0.42);

    alwan_scalar x_FL = (FL * x) / ALWAN_LITERAL(100.0);
    alwan_scalar x_abs = ALWAN_FABS(x_FL);
    alwan_scalar x_pow = ALWAN_POW(x_abs, FL_pow);
    alwan_scalar result = ALWAN_LITERAL(400.0) * x_pow / (ALWAN_LITERAL(27.13) + x_pow) + ALWAN_LITERAL(0.1);

    return (x_FL < ALWAN_LITERAL(0.0)) ? -result : result;
}

/* Inverse of post-adaptation nonlinear response */
static alwan_scalar post_adaptation_nonlinear_inv(alwan_scalar x, alwan_scalar FL) {
    alwan_scalar const FL_pow_inv = ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.42);

    alwan_scalar y = x - ALWAN_LITERAL(0.1);
    alwan_scalar y_abs = ALWAN_FABS(y);

    alwan_scalar result = ALWAN_POW((ALWAN_LITERAL(27.13) * y_abs) /
                             (ALWAN_LITERAL(400.0) - y_abs), FL_pow_inv) * ALWAN_LITERAL(100.0) / FL;

    return (x < ALWAN_LITERAL(0.0)) ? -result : result;
}

/* Hue angle to hue quadrature H (same as CAM16) */
static alwan_scalar hue_to_quadrature(alwan_scalar h) {
    alwan_scalar const h_i[5] = {
        ALWAN_LITERAL(20.14), ALWAN_LITERAL(90.00),
        ALWAN_LITERAL(164.25), ALWAN_LITERAL(237.53),
        ALWAN_LITERAL(380.14)
    };
    alwan_scalar const e_i[4] = {
        ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.7),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.2)
    };
    alwan_scalar const H_i[4] = {
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(100.0),
        ALWAN_LITERAL(200.0), ALWAN_LITERAL(300.0)
    };

    while (h < ALWAN_LITERAL(20.14)) h += ALWAN_LITERAL(360.0);
    while (h > ALWAN_LITERAL(380.14)) h -= ALWAN_LITERAL(360.0);

    for (int i = 0; i < 4; i++) {
        if (h >= h_i[i] && h < h_i[i + 1]) {
            alwan_scalar h_p = (h - h_i[i]) / e_i[i];
            alwan_scalar H = H_i[i] + (ALWAN_LITERAL(100.0) * h_p) / (h_p + (h_i[i + 1] - h) / e_i[i]);
            return H;
        }
    }

    return ALWAN_LITERAL(0.0);
}

/* ----------------------------------------------------------------
 * Hellwig2022 Forward Transform
 * ---------------------------------------------------------------- */

int alwan_hellwig2022_forward(alwan_xyz const *xyz,
                               alwan_hellwig2022_viewing_conditions const *vc,
                               alwan_hellwig2022_correlates *out) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Get surround parameters */
    alwan_scalar F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    /* Step 1: Calculate degree of adaptation D */
    alwan_scalar La = vc->adapting_luminance;
    alwan_scalar D = compute_D(F, La, vc->discount_illuminant);

    /* Step 2: Calculate luminance level adaptation factor FL */
    alwan_scalar k = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(5.0) * La + ALWAN_LITERAL(1.0));
    alwan_scalar k4 = k * k * k * k;
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * La) +
                      ALWAN_LITERAL(0.1) * (ALWAN_LITERAL(1.0) - k4) * (ALWAN_LITERAL(1.0) - k4) *
                      ALWAN_POW(ALWAN_LITERAL(5.0) * La, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    /* Step 3: Transform test color XYZ to RGB using CAT16 */
    alwan_vec3 RGB;
    RGB.v[0] = M_CAT16[0] * xyz->x + M_CAT16[1] * xyz->y + M_CAT16[2] * xyz->z;
    RGB.v[1] = M_CAT16[3] * xyz->x + M_CAT16[4] * xyz->y + M_CAT16[5] * xyz->z;
    RGB.v[2] = M_CAT16[6] * xyz->x + M_CAT16[7] * xyz->y + M_CAT16[8] * xyz->z;

    /* Step 4: Transform white point to RGB */
    alwan_vec3 RGB_w;
    RGB_w.v[0] = M_CAT16[0] * vc->white_xyz.x + M_CAT16[1] * vc->white_xyz.y + M_CAT16[2] * vc->white_xyz.z;
    RGB_w.v[1] = M_CAT16[3] * vc->white_xyz.x + M_CAT16[4] * vc->white_xyz.y + M_CAT16[5] * vc->white_xyz.z;
    RGB_w.v[2] = M_CAT16[6] * vc->white_xyz.x + M_CAT16[7] * vc->white_xyz.y + M_CAT16[8] * vc->white_xyz.z;

    /* Step 5: Apply chromatic adaptation */
    alwan_vec3 RGB_c;
    RGB_c.v[0] = (vc->white_xyz.y * D / RGB_w.v[0] + ALWAN_LITERAL(1.0) - D) * RGB.v[0];
    RGB_c.v[1] = (vc->white_xyz.y * D / RGB_w.v[1] + ALWAN_LITERAL(1.0) - D) * RGB.v[1];
    RGB_c.v[2] = (vc->white_xyz.y * D / RGB_w.v[2] + ALWAN_LITERAL(1.0) - D) * RGB.v[2];

    /* Step 6: Apply nonlinear response compression DIRECTLY to RGB_c */
    /* NOTE: Hellwig2022 does NOT use post-adaptation matrix like CAM16! */
    alwan_scalar R_a_prime = post_adaptation_nonlinear(RGB_c.v[0], FL);
    alwan_scalar G_a_prime = post_adaptation_nonlinear(RGB_c.v[1], FL);
    alwan_scalar B_a_prime = post_adaptation_nonlinear(RGB_c.v[2], FL);

    /* Step 7: Calculate opponent dimensions */
    alwan_scalar a = R_a_prime - ALWAN_LITERAL(12.0) * G_a_prime / ALWAN_LITERAL(11.0) + B_a_prime / ALWAN_LITERAL(11.0);
    alwan_scalar b = (R_a_prime + G_a_prime - ALWAN_LITERAL(2.0) * B_a_prime) / ALWAN_LITERAL(9.0);

    /* Step 8: Calculate hue angle h */
    alwan_scalar h_rad = ALWAN_ATAN2(b, a);
    out->h = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    if (out->h < ALWAN_LITERAL(0.0)) out->h += ALWAN_LITERAL(360.0);

    /* Step 9: Calculate eccentricity factor et using Fourier series (Hellwig2022) */
    alwan_scalar et = ALWAN_LITERAL(1.0) +
                      ALWAN_LITERAL(-0.0582) * ALWAN_COS(h_rad) +
                      ALWAN_LITERAL(-0.0258) * ALWAN_COS(ALWAN_LITERAL(2.0) * h_rad) +
                      ALWAN_LITERAL(-0.1347) * ALWAN_COS(ALWAN_LITERAL(3.0) * h_rad) +
                      ALWAN_LITERAL( 0.0289) * ALWAN_COS(ALWAN_LITERAL(4.0) * h_rad) +
                      ALWAN_LITERAL(-0.1475) * ALWAN_SIN(h_rad) +
                      ALWAN_LITERAL(-0.0308) * ALWAN_SIN(ALWAN_LITERAL(2.0) * h_rad) +
                      ALWAN_LITERAL( 0.0385) * ALWAN_SIN(ALWAN_LITERAL(3.0) * h_rad) +
                      ALWAN_LITERAL( 0.0096) * ALWAN_SIN(ALWAN_LITERAL(4.0) * h_rad);

    /* Step 10: Calculate achromatic response for white point (A_w) */
    /* Transform white point through the pipeline (no post-adaptation matrix) */
    alwan_vec3 RGB_wc;
    RGB_wc.v[0] = (vc->white_xyz.y * D / RGB_w.v[0] + ALWAN_LITERAL(1.0) - D) * RGB_w.v[0];
    RGB_wc.v[1] = (vc->white_xyz.y * D / RGB_w.v[1] + ALWAN_LITERAL(1.0) - D) * RGB_w.v[1];
    RGB_wc.v[2] = (vc->white_xyz.y * D / RGB_w.v[2] + ALWAN_LITERAL(1.0) - D) * RGB_w.v[2];

    alwan_scalar R_aw_prime = post_adaptation_nonlinear(RGB_wc.v[0], FL);
    alwan_scalar G_aw_prime = post_adaptation_nonlinear(RGB_wc.v[1], FL);
    alwan_scalar B_aw_prime = post_adaptation_nonlinear(RGB_wc.v[2], FL);

    /* Hellwig2022 achromatic response formula for white */
    alwan_scalar A_w = ALWAN_LITERAL(2.0) * R_aw_prime + G_aw_prime +
                       ALWAN_LITERAL(0.05) * B_aw_prime - ALWAN_LITERAL(0.305);

    /* Step 11: Calculate achromatic response A for test color */
    /* Hellwig2022 uses simplified formula WITHOUT Nc factor */
    alwan_scalar A = ALWAN_LITERAL(2.0) * R_a_prime + G_a_prime +
                     ALWAN_LITERAL(0.05) * B_a_prime - ALWAN_LITERAL(0.305);

    /* Step 12: Calculate base exponential nonlinearity z */
    alwan_scalar n = vc->background_luminance / vc->white_xyz.y;
    alwan_scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);

    /* Step 13: Calculate lightness J */
    out->J = ALWAN_LITERAL(100.0) * ALWAN_POW(A / A_w, c * z);

    /* Step 14: Calculate colorfulness M (Hellwig2022 simplified formula) */
    out->M = ALWAN_LITERAL(43.0) * Nc * et * ALWAN_SQRT(a * a + b * b);

    /* Step 15: Calculate chroma C (Hellwig2022 simplified formula) */
    out->C = ALWAN_LITERAL(35.0) * out->M / A_w;

    /* Step 16: Calculate brightness Q (Hellwig2022 simplified formula) */
    out->Q = (ALWAN_LITERAL(2.0) / c) * (out->J / ALWAN_LITERAL(100.0)) * A_w;

    /* Step 17: Calculate saturation s (no sqrt in Hellwig2022!) */
    out->s = ALWAN_LITERAL(100.0) * out->M / out->Q;

    /* Step 18: Calculate hue composition H (optional) */
    out->H = hue_to_quadrature(out->h);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Hellwig2022 Inverse Transform
 * ---------------------------------------------------------------- */

int alwan_hellwig2022_inverse(alwan_hellwig2022_correlates const *correlates,
                               alwan_hellwig2022_viewing_conditions const *vc,
                               alwan_xyz *xyz_out) {
    if (!correlates || !vc || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    /* Get surround parameters */
    alwan_scalar F, c, Nc;
    get_surround_params(vc->surround, &F, &c, &Nc);

    /* Calculate adaptation parameters */
    alwan_scalar La = vc->adapting_luminance;
    alwan_scalar D = compute_D(F, La, vc->discount_illuminant);

    alwan_scalar k = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(5.0) * La + ALWAN_LITERAL(1.0));
    alwan_scalar k4 = k * k * k * k;
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * La) +
                      ALWAN_LITERAL(0.1) * (ALWAN_LITERAL(1.0) - k4) * (ALWAN_LITERAL(1.0) - k4) *
                      ALWAN_POW(ALWAN_LITERAL(5.0) * La, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    /* Step 1: Calculate achromatic response for white point (A_w) - same as forward */
    alwan_vec3 RGB_w;
    RGB_w.v[0] = M_CAT16[0] * vc->white_xyz.x + M_CAT16[1] * vc->white_xyz.y + M_CAT16[2] * vc->white_xyz.z;
    RGB_w.v[1] = M_CAT16[3] * vc->white_xyz.x + M_CAT16[4] * vc->white_xyz.y + M_CAT16[5] * vc->white_xyz.z;
    RGB_w.v[2] = M_CAT16[6] * vc->white_xyz.x + M_CAT16[7] * vc->white_xyz.y + M_CAT16[8] * vc->white_xyz.z;

    alwan_vec3 RGB_wc;
    RGB_wc.v[0] = (vc->white_xyz.y * D / RGB_w.v[0] + ALWAN_LITERAL(1.0) - D) * RGB_w.v[0];
    RGB_wc.v[1] = (vc->white_xyz.y * D / RGB_w.v[1] + ALWAN_LITERAL(1.0) - D) * RGB_w.v[1];
    RGB_wc.v[2] = (vc->white_xyz.y * D / RGB_w.v[2] + ALWAN_LITERAL(1.0) - D) * RGB_w.v[2];

    /* Apply nonlinear directly to RGB_wc (no post-adaptation matrix in Hellwig2022) */
    alwan_scalar R_aw_prime = post_adaptation_nonlinear(RGB_wc.v[0], FL);
    alwan_scalar G_aw_prime = post_adaptation_nonlinear(RGB_wc.v[1], FL);
    alwan_scalar B_aw_prime = post_adaptation_nonlinear(RGB_wc.v[2], FL);

    alwan_scalar A_w = ALWAN_LITERAL(2.0) * R_aw_prime + G_aw_prime +
                       ALWAN_LITERAL(0.05) * B_aw_prime - ALWAN_LITERAL(0.305);

    /* Step 2: Calculate base exponential nonlinearity z */
    alwan_scalar n = vc->background_luminance / vc->white_xyz.y;
    alwan_scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);

    /* Step 3: Compute achromatic response A from lightness J */
    alwan_scalar A = A_w * ALWAN_POW(correlates->J / ALWAN_LITERAL(100.0), ALWAN_LITERAL(1.0) / (c * z));

    /* Step 4: Compute opponent dimensions a and b from hue and chroma */
    alwan_scalar h_rad = correlates->h * ALWAN_PI / ALWAN_LITERAL(180.0);

    /* Hellwig2022 Fourier series eccentricity */
    alwan_scalar et = ALWAN_LITERAL(1.0) +
                      ALWAN_LITERAL(-0.0582) * ALWAN_COS(h_rad) +
                      ALWAN_LITERAL(-0.0258) * ALWAN_COS(ALWAN_LITERAL(2.0) * h_rad) +
                      ALWAN_LITERAL(-0.1347) * ALWAN_COS(ALWAN_LITERAL(3.0) * h_rad) +
                      ALWAN_LITERAL( 0.0289) * ALWAN_COS(ALWAN_LITERAL(4.0) * h_rad) +
                      ALWAN_LITERAL(-0.1475) * ALWAN_SIN(h_rad) +
                      ALWAN_LITERAL(-0.0308) * ALWAN_SIN(ALWAN_LITERAL(2.0) * h_rad) +
                      ALWAN_LITERAL( 0.0385) * ALWAN_SIN(ALWAN_LITERAL(3.0) * h_rad) +
                      ALWAN_LITERAL( 0.0096) * ALWAN_SIN(ALWAN_LITERAL(4.0) * h_rad);

    /* Hellwig2022 simplified inverse: C = 35*M/A_w, so M = C*A_w/35 */
    alwan_scalar M = correlates->C * A_w / ALWAN_LITERAL(35.0);

    /* From M = 43*Nc*et*sqrt(a^2+b^2), get sqrt(a^2+b^2) */
    alwan_scalar ab_magnitude = M / (ALWAN_LITERAL(43.0) * Nc * et);

    alwan_scalar a = ALWAN_COS(h_rad) * ab_magnitude;
    alwan_scalar b = ALWAN_SIN(h_rad) * ab_magnitude;

    /* Step 5: Compute post-adaptation cone responses from opponent dimensions */
    alwan_scalar p2 = A + ALWAN_LITERAL(0.305);
    alwan_scalar R_a_prime = (ALWAN_LITERAL(460.0) * p2 + ALWAN_LITERAL(451.0) * a + ALWAN_LITERAL(288.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar G_a_prime = (ALWAN_LITERAL(460.0) * p2 - ALWAN_LITERAL(891.0) * a - ALWAN_LITERAL(261.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar B_a_prime = (ALWAN_LITERAL(460.0) * p2 - ALWAN_LITERAL(220.0) * a - ALWAN_LITERAL(6300.0) * b) / ALWAN_LITERAL(1403.0);

    /* Step 6: Apply inverse nonlinear compression DIRECTLY to get RGB_c */
    /* NOTE: Hellwig2022 has no post-adaptation matrix! */
    alwan_vec3 RGB_c;
    RGB_c.v[0] = post_adaptation_nonlinear_inv(R_a_prime, FL);
    RGB_c.v[1] = post_adaptation_nonlinear_inv(G_a_prime, FL);
    RGB_c.v[2] = post_adaptation_nonlinear_inv(B_a_prime, FL);

    /* Step 7: Reverse chromatic adaptation (RGB_w already calculated above) */
    alwan_vec3 RGB;
    RGB.v[0] = RGB_c.v[0] / (vc->white_xyz.y * D / RGB_w.v[0] + ALWAN_LITERAL(1.0) - D);
    RGB.v[1] = RGB_c.v[1] / (vc->white_xyz.y * D / RGB_w.v[1] + ALWAN_LITERAL(1.0) - D);
    RGB.v[2] = RGB_c.v[2] / (vc->white_xyz.y * D / RGB_w.v[2] + ALWAN_LITERAL(1.0) - D);

    /* Step 8: Transform RGB back to XYZ using inverse CAT16 */
    xyz_out->x = M_CAT16_inv[0] * RGB.v[0] + M_CAT16_inv[1] * RGB.v[1] + M_CAT16_inv[2] * RGB.v[2];
    xyz_out->y = M_CAT16_inv[3] * RGB.v[0] + M_CAT16_inv[4] * RGB.v[1] + M_CAT16_inv[5] * RGB.v[2];
    xyz_out->z = M_CAT16_inv[6] * RGB.v[0] + M_CAT16_inv[7] * RGB.v[1] + M_CAT16_inv[8] * RGB.v[2];

    return ALWAN_OK;
}
