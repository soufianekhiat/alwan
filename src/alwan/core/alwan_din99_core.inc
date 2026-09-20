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
 * DIN99 Variant Coefficients [c1 .. c8]
 *
 * WHAT EACH SLOT IS, so a value can be checked against the standard without
 * reading the transform first. Taken from the forward transform below, which is
 * the only thing that reads this table:
 *
 *   L99 = c1 * ln(1 + c2 * L*) * k_E
 *   e   =      a* cos(c3) + b* sin(c3)          c3 in DEGREES
 *   f   = c4 * (-a* sin(c3) + b* cos(c3))
 *   G   = sqrt(e^2 + f^2)
 *   C99 = c5 * ln(1 + c6 * G) / (c8 * k_CH * k_E)
 *   h99 = atan2(f, e) + c7                      c7 in DEGREES
 *   a99 = C99 cos(h99),  b99 = C99 sin(h99)
 *
 *   c1  lightness scale          c2  lightness log compression
 *   c3  rotation before chroma   c4  compression of the rotated b axis
 *   c5  chroma scale             c6  chroma log compression
 *   c7  hue rotation added back  c8  chroma denominator
 *
 * The two spellings of the same thing are why c5 and c8 look redundant: DIN99
 * writes the chroma as ln(1 + 0.045 G) / 0.045, so c5 = 1 and c8 = 0.045, while
 * the later variants write it as 23 ln(1 + 0.075 G), so c5 = 23 and c8 = 1. One
 * table covers both forms rather than a branch per variant.
 *
 * c3 = c7 in every variant that rotates at all: the transform turns the axes by
 * c3 before measuring chroma and turns the hue back by c7 afterwards. DIN99c
 * rotates by neither, which is what makes its row read 0.0 twice rather than
 * looking like a gap.
 *
 * Sources: DIN 6176:2001-03 for DIN99; ASTM D2244-07 reproduces it. The b, c and
 * d variants are Cui, Luo, Rigg, Roesler and Witt (2002), "Uniform colour spaces
 * based on the DIN99 colour-difference formula", Color Research & Application
 * 27(4), 282-290.
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_CONSTEXPR ALWAN_CORE_T ALWAN_CORE_FNLIT(ALWAN_DIN99_COEFFS)[4][8] = {
    /* DIN99, DIN 6176:2001-03. c5/c8 carry the 1/0.045 spelling. */
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
    /* DIN99b, Cui 2002. Rotates 26 degrees, chroma as 23 ln(1 + 0.075 G). */
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
    /* DIN99c, Cui 2002. No rotation, so c3 and c7 are both 0. */
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
    /* DIN99d, Cui 2002. Rotates 50 degrees; c4 above 1 stretches rather than compresses. */
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

    const ALWAN_CORE_T k_E = ALWAN_CORE_LITERAL(1.0);
    const ALWAN_CORE_T k_CH = ALWAN_CORE_LITERAL(1.0);

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

    const ALWAN_CORE_T k_E = ALWAN_CORE_LITERAL(1.0);
    const ALWAN_CORE_T k_CH = ALWAN_CORE_LITERAL(1.0);

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
