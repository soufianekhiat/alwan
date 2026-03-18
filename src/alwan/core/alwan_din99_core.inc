/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only DIN99 Family (DIN99, DIN99b, DIN99c, DIN99d) (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: DIN 6176:2001-03, ASTM D2244-07
 */

/* ----------------------------------------------------------------
 * DIN99 Variant Coefficients [c1, c2, c3, c4, c5, c6, c7, c8]
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[4][8] = {
    /* DIN99 / ASTM D2244-07 */
    {
        ALWAN_CORE_LITERAL(105.509),
        ALWAN_CORE_LITERAL(0.0158),
        ALWAN_CORE_LITERAL(16.0),
        ALWAN_CORE_LITERAL(0.7),
        ALWAN_CORE_LITERAL(1.0),
        ALWAN_CORE_LITERAL(0.045),      /* 9/200 */
        ALWAN_CORE_LITERAL(0.0),
        ALWAN_CORE_LITERAL(0.045)       /* 9/200 */
    },
    /* DIN99b */
    {
        ALWAN_CORE_LITERAL(303.67),
        ALWAN_CORE_LITERAL(0.0039),
        ALWAN_CORE_LITERAL(26.0),
        ALWAN_CORE_LITERAL(0.83),
        ALWAN_CORE_LITERAL(23.0),
        ALWAN_CORE_LITERAL(0.075),
        ALWAN_CORE_LITERAL(26.0),
        ALWAN_CORE_LITERAL(1.0)
    },
    /* DIN99c */
    {
        ALWAN_CORE_LITERAL(317.65),
        ALWAN_CORE_LITERAL(0.0037),
        ALWAN_CORE_LITERAL(0.0),
        ALWAN_CORE_LITERAL(0.94),
        ALWAN_CORE_LITERAL(23.0),
        ALWAN_CORE_LITERAL(0.066),
        ALWAN_CORE_LITERAL(0.0),
        ALWAN_CORE_LITERAL(1.0)
    },
    /* DIN99d */
    {
        ALWAN_CORE_LITERAL(325.22),
        ALWAN_CORE_LITERAL(0.0036),
        ALWAN_CORE_LITERAL(50.0),
        ALWAN_CORE_LITERAL(1.14),
        ALWAN_CORE_LITERAL(22.5),
        ALWAN_CORE_LITERAL(0.06),
        ALWAN_CORE_LITERAL(50.0),
        ALWAN_CORE_LITERAL(1.0)
    }
};

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Lab -> DIN99 (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_DIN99 ALWAN_CORE_FNVLIT(alwan_lab_to_din99)(ALWAN_CORE_LAB lab, int variant) {
    ALWAN_CORE_DIN99 result;

    /* Clamp variant to [0,3] */
    int v = ALWAN_CORE_SELECT(variant < 0, 0, ALWAN_CORE_SELECT(variant > 3, 3, variant));

    ALWAN_CORE_T const k_E = ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T const k_CH = ALWAN_CORE_LITERAL(1.0);

    /* DIN99 lightness */
    result.L99 = ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][0] * ALWAN_CORE_LN(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][1] * lab.L) * k_E;

    /* Achromatic guard */
    ALWAN_CORE_T lab_chroma_sq = lab.a * lab.a + lab.b * lab.b;

    /* Convert angle to radians */
    ALWAN_CORE_T c3_rad = ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][2] * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_T c7_rad = ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][6] * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);

    ALWAN_CORE_T cos_c3 = ALWAN_CORE_COS(c3_rad);
    ALWAN_CORE_T sin_c3 = ALWAN_CORE_SIN(c3_rad);

    /* Calculate e and f */
    ALWAN_CORE_T e = cos_c3 * lab.a + sin_c3 * lab.b;
    ALWAN_CORE_T f = ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][3] * (-sin_c3 * lab.a + cos_c3 * lab.b);

    /* Calculate G */
    ALWAN_CORE_T G = ALWAN_CORE_SQRT(e * e + f * f);

    /* Calculate hue angle h_ef and chroma C99 */
    ALWAN_CORE_T h_ef = ALWAN_CORE_ATAN2(f, e) + c7_rad;
    ALWAN_CORE_T C99 = (ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][4] * ALWAN_CORE_LN(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][5] * G)) / (ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][7] * k_CH * k_E);

    /* Calculate chromatic coordinates with achromatic guard */
    result.a99 = ALWAN_CORE_SELECT(lab_chroma_sq < ALWAN_CORE_LITERAL(1e-12),
                              ALWAN_CORE_LITERAL(0.0), C99 * ALWAN_CORE_COS(h_ef));
    result.b99 = ALWAN_CORE_SELECT(lab_chroma_sq < ALWAN_CORE_LITERAL(1e-12),
                              ALWAN_CORE_LITERAL(0.0), C99 * ALWAN_CORE_SIN(h_ef));

    return result;
}

/* ----------------------------------------------------------------
 * DIN99 -> Lab (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_LAB ALWAN_CORE_FNVLIT(alwan_din99_to_lab)(ALWAN_CORE_DIN99 din99, int variant) {
    ALWAN_CORE_LAB result;

    /* Clamp variant to [0,3] */
    int v = ALWAN_CORE_SELECT(variant < 0, 0, ALWAN_CORE_SELECT(variant > 3, 3, variant));

    ALWAN_CORE_T const k_E = ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T const k_CH = ALWAN_CORE_LITERAL(1.0);

    /* Convert angle to radians */
    ALWAN_CORE_T c3_rad = ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][2] * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_T c7_rad = ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][6] * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);

    ALWAN_CORE_T cos_c3 = ALWAN_CORE_COS(c3_rad);
    ALWAN_CORE_T sin_c3 = ALWAN_CORE_SIN(c3_rad);

    /* Calculate hue angle h99 */
    ALWAN_CORE_T h99 = ALWAN_CORE_ATAN2(din99.b99, din99.a99) - c7_rad;

    /* Calculate chroma C99 */
    ALWAN_CORE_T C99 = ALWAN_CORE_SQRT(din99.a99 * din99.a99 + din99.b99 * din99.b99);

    /* Calculate G */
    ALWAN_CORE_T G = (ALWAN_CORE_EXP((ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][7] / ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][4]) * C99 * k_CH * k_E) - ALWAN_CORE_LITERAL(1.0)) / ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][5];

    /* Calculate e and f */
    ALWAN_CORE_T e = G * ALWAN_CORE_COS(h99);
    ALWAN_CORE_T f = G * ALWAN_CORE_SIN(h99);

    /* Calculate Lab coordinates */
    result.a = e * cos_c3 - (f / ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][3]) * sin_c3;
    result.b = e * sin_c3 + (f / ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][3]) * cos_c3;
    result.L = (ALWAN_CORE_EXP(din99.L99 * k_E / ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][0]) - ALWAN_CORE_LITERAL(1.0)) / ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[v][1];

    return result;
}
