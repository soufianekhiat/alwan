/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * DIN99 Family (DIN99, DIN99b, DIN99c, DIN99d)
 *
 * Reference: DIN 6176:2001-03, ASTM D2244-07
 * German color difference standards with improved uniformity
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ----------------------------------------------------------------
 * DIN99 Variant Coefficients [c1, c2, c3, c4, c5, c6, c7, c8]
 * ---------------------------------------------------------------- */

static alwan_scalar const DIN99_COEFFS[4][8] = {
    /* DIN99 / ASTM D2244-07 */
    {
        ALWAN_LITERAL(105.509),
        ALWAN_LITERAL(0.0158),
        ALWAN_LITERAL(16.0),
        ALWAN_LITERAL(0.7),
        ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(0.045),      /* 9/200 */
        ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.045)       /* 9/200 */
    },
    /* DIN99b */
    {
        ALWAN_LITERAL(303.67),
        ALWAN_LITERAL(0.0039),
        ALWAN_LITERAL(26.0),
        ALWAN_LITERAL(0.83),
        ALWAN_LITERAL(23.0),
        ALWAN_LITERAL(0.075),
        ALWAN_LITERAL(26.0),
        ALWAN_LITERAL(1.0)
    },
    /* DIN99c */
    {
        ALWAN_LITERAL(317.65),
        ALWAN_LITERAL(0.0037),
        ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.94),
        ALWAN_LITERAL(23.0),
        ALWAN_LITERAL(0.066),
        ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(1.0)
    },
    /* DIN99d */
    {
        ALWAN_LITERAL(325.22),
        ALWAN_LITERAL(0.0036),
        ALWAN_LITERAL(50.0),
        ALWAN_LITERAL(1.14),
        ALWAN_LITERAL(22.5),
        ALWAN_LITERAL(0.06),
        ALWAN_LITERAL(50.0),
        ALWAN_LITERAL(1.0)
    }
};

/* ----------------------------------------------------------------
 * Lab <-> DIN99 Family
 * ---------------------------------------------------------------- */

void alwan_lab_to_din99(alwan_vec3 const *lab, alwan_vec3 *din99, int variant) {
    if (variant < 0 || variant > 3) return;  /* Invalid variant */

    alwan_scalar const *c = DIN99_COEFFS[variant];
    alwan_scalar const k_E = ALWAN_LITERAL(1.0);   /* Texture compensation */
    alwan_scalar const k_CH = ALWAN_LITERAL(1.0);  /* Chroma compensation */

    /* Calculate DIN99 lightness first */
    din99->v[0] = c[0] * ALWAN_LOG(ALWAN_LITERAL(1.0) + c[1] * lab->v[0]) * k_E;  /* L99 */

    /* Check for achromatic colors (very small Lab chroma) to avoid numerical noise */
    alwan_scalar lab_chroma_sq = lab->v[1] * lab->v[1] + lab->v[2] * lab->v[2];
    if (lab_chroma_sq < ALWAN_LITERAL(1e-12)) {
        din99->v[1] = ALWAN_LITERAL(0.0);  /* a99 */
        din99->v[2] = ALWAN_LITERAL(0.0);  /* b99 */
        return;
    }

    /* Convert angle to radians */
    alwan_scalar c3_rad = c[2] * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar c7_rad = c[6] * ALWAN_PI / ALWAN_LITERAL(180.0);

    alwan_scalar cos_c3 = ALWAN_COS(c3_rad);
    alwan_scalar sin_c3 = ALWAN_SIN(c3_rad);

    /* Calculate e and f */
    alwan_scalar e = cos_c3 * lab->v[1] + sin_c3 * lab->v[2];
    alwan_scalar f = c[3] * (-sin_c3 * lab->v[1] + cos_c3 * lab->v[2]);

    /* Calculate G */
    alwan_scalar G = ALWAN_SQRT(e * e + f * f);

    /* Calculate hue angle h_ef and chroma C99 */
    alwan_scalar h_ef = ALWAN_ATAN2(f, e) + c7_rad;
    alwan_scalar C99 = (c[4] * ALWAN_LOG(ALWAN_LITERAL(1.0) + c[5] * G)) / (c[7] * k_CH * k_E);

    /* Calculate chromatic coordinates */
    din99->v[1] = C99 * ALWAN_COS(h_ef);  /* a99 */
    din99->v[2] = C99 * ALWAN_SIN(h_ef);  /* b99 */
}

void alwan_din99_to_lab(alwan_vec3 const *din99, alwan_vec3 *lab, int variant) {
    if (variant < 0 || variant > 3) return;  /* Invalid variant */

    alwan_scalar const *c = DIN99_COEFFS[variant];
    alwan_scalar const k_E = ALWAN_LITERAL(1.0);   /* Texture compensation */
    alwan_scalar const k_CH = ALWAN_LITERAL(1.0);  /* Chroma compensation */

    /* Convert angle to radians */
    alwan_scalar c3_rad = c[2] * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar c7_rad = c[6] * ALWAN_PI / ALWAN_LITERAL(180.0);

    alwan_scalar cos_c3 = ALWAN_COS(c3_rad);
    alwan_scalar sin_c3 = ALWAN_SIN(c3_rad);

    /* Calculate hue angle h99 */
    alwan_scalar h99 = ALWAN_ATAN2(din99->v[2], din99->v[1]) - c7_rad;

    /* Calculate chroma C99 */
    alwan_scalar C99 = ALWAN_SQRT(din99->v[1] * din99->v[1] + din99->v[2] * din99->v[2]);

    /* Calculate G */
    alwan_scalar G = (ALWAN_EXP((c[7] / c[4]) * C99 * k_CH * k_E) - ALWAN_LITERAL(1.0)) / c[5];

    /* Calculate e and f */
    alwan_scalar e = G * ALWAN_COS(h99);
    alwan_scalar f = G * ALWAN_SIN(h99);

    /* Calculate Lab coordinates */
    lab->v[1] = e * cos_c3 - (f / c[3]) * sin_c3;  /* a */
    lab->v[2] = e * sin_c3 + (f / c[3]) * cos_c3;  /* b */
    lab->v[0] = (ALWAN_EXP(din99->v[0] * k_E / c[0]) - ALWAN_LITERAL(1.0)) / c[1];  /* L */
}
