/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only DIN99 Family (DIN99, DIN99b, DIN99c, DIN99d)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: DIN 6176:2001-03, ASTM D2244-07
 */

#ifndef ALWAN_DIN99_CORE_H
#define ALWAN_DIN99_CORE_H

#include "alwan_platform.h"
#include "alwan_types.h"

/* ----------------------------------------------------------------
 * DIN99 Variant Coefficients [c1, c2, c3, c4, c5, c6, c7, c8]
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

static alwan_scalar const ALWAN_DIN99_COEFFS[4][8] = {
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

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Lab -> DIN99 (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_din99 alwan_lab_to_din99_v(alwan_lab lab, int variant) {
    alwan_din99 result;

    /* Clamp variant to [0,3] */
    int v = ALWAN_SELECT(variant < 0, 0, ALWAN_SELECT(variant > 3, 3, variant));

    alwan_scalar const *c = ALWAN_DIN99_COEFFS[v];
    alwan_scalar const k_E = ALWAN_LITERAL(1.0);
    alwan_scalar const k_CH = ALWAN_LITERAL(1.0);

    /* DIN99 lightness */
    result.L99 = c[0] * ALWAN_LN(ALWAN_LITERAL(1.0) + c[1] * lab.L) * k_E;

    /* Achromatic guard */
    alwan_scalar lab_chroma_sq = lab.a * lab.a + lab.b * lab.b;

    /* Convert angle to radians */
    alwan_scalar c3_rad = c[2] * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar c7_rad = c[6] * ALWAN_PI / ALWAN_LITERAL(180.0);

    alwan_scalar cos_c3 = ALWAN_COS(c3_rad);
    alwan_scalar sin_c3 = ALWAN_SIN(c3_rad);

    /* Calculate e and f */
    alwan_scalar e = cos_c3 * lab.a + sin_c3 * lab.b;
    alwan_scalar f = c[3] * (-sin_c3 * lab.a + cos_c3 * lab.b);

    /* Calculate G */
    alwan_scalar G = ALWAN_SQRT(e * e + f * f);

    /* Calculate hue angle h_ef and chroma C99 */
    alwan_scalar h_ef = ALWAN_ATAN2(f, e) + c7_rad;
    alwan_scalar C99 = (c[4] * ALWAN_LN(ALWAN_LITERAL(1.0) + c[5] * G)) / (c[7] * k_CH * k_E);

    /* Calculate chromatic coordinates with achromatic guard */
    result.a99 = ALWAN_SELECT(lab_chroma_sq < ALWAN_LITERAL(1e-12),
                              ALWAN_LITERAL(0.0), C99 * ALWAN_COS(h_ef));
    result.b99 = ALWAN_SELECT(lab_chroma_sq < ALWAN_LITERAL(1e-12),
                              ALWAN_LITERAL(0.0), C99 * ALWAN_SIN(h_ef));

    return result;
}

/* ----------------------------------------------------------------
 * DIN99 -> Lab (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_lab alwan_din99_to_lab_v(alwan_din99 din99, int variant) {
    alwan_lab result;

    /* Clamp variant to [0,3] */
    int v = ALWAN_SELECT(variant < 0, 0, ALWAN_SELECT(variant > 3, 3, variant));

    alwan_scalar const *c = ALWAN_DIN99_COEFFS[v];
    alwan_scalar const k_E = ALWAN_LITERAL(1.0);
    alwan_scalar const k_CH = ALWAN_LITERAL(1.0);

    /* Convert angle to radians */
    alwan_scalar c3_rad = c[2] * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar c7_rad = c[6] * ALWAN_PI / ALWAN_LITERAL(180.0);

    alwan_scalar cos_c3 = ALWAN_COS(c3_rad);
    alwan_scalar sin_c3 = ALWAN_SIN(c3_rad);

    /* Calculate hue angle h99 */
    alwan_scalar h99 = ALWAN_ATAN2(din99.b99, din99.a99) - c7_rad;

    /* Calculate chroma C99 */
    alwan_scalar C99 = ALWAN_SQRT(din99.a99 * din99.a99 + din99.b99 * din99.b99);

    /* Calculate G */
    alwan_scalar G = (ALWAN_EXP((c[7] / c[4]) * C99 * k_CH * k_E) - ALWAN_LITERAL(1.0)) / c[5];

    /* Calculate e and f */
    alwan_scalar e = G * ALWAN_COS(h99);
    alwan_scalar f = G * ALWAN_SIN(h99);

    /* Calculate Lab coordinates */
    result.a = e * cos_c3 - (f / c[3]) * sin_c3;
    result.b = e * sin_c3 + (f / c[3]) * cos_c3;
    result.L = (ALWAN_EXP(din99.L99 * k_E / c[0]) - ALWAN_LITERAL(1.0)) / c[1];

    return result;
}

#endif /* ALWAN_DIN99_CORE_H */
