/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Hero Wavelength Sampling for spectral rendering (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Wilkie et al. (2014), using Wyman (2013) analytic CMF fit
 */

/* ================================================================
 * Wyman 2013 Analytic CIE 1931 CMF Fit
 *
 * Sum-of-Gaussians approximation to CIE 1931 2-degree CMFs.
 * Reference: Chris Wyman, Peter-Pike Sloan, Peter Shirley (2013)
 * "Simple Analytic Approximations to the CIE XYZ Color Matching Functions"
 *
 * These are fully GPU-compatible (no table lookup needed).
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_wyman_xbar)(ALWAN_CORE_T lambda) {
    ALWAN_CORE_T t1 = (lambda - ALWAN_CORE_LITERAL(442.0)) *
        ALWAN_CORE_SELECT(lambda < ALWAN_CORE_LITERAL(442.0),
                     ALWAN_CORE_LITERAL(0.0624), ALWAN_CORE_LITERAL(0.0374));
    ALWAN_CORE_T t2 = (lambda - ALWAN_CORE_LITERAL(599.8)) *
        ALWAN_CORE_SELECT(lambda < ALWAN_CORE_LITERAL(599.8),
                     ALWAN_CORE_LITERAL(0.0264), ALWAN_CORE_LITERAL(0.0323));
    ALWAN_CORE_T t3 = (lambda - ALWAN_CORE_LITERAL(501.1)) *
        ALWAN_CORE_SELECT(lambda < ALWAN_CORE_LITERAL(501.1),
                     ALWAN_CORE_LITERAL(0.0490), ALWAN_CORE_LITERAL(0.0382));

    return ALWAN_CORE_LITERAL(0.362) * ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(-0.5) * t1 * t1)
         + ALWAN_CORE_LITERAL(1.056) * ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(-0.5) * t2 * t2)
         - ALWAN_CORE_LITERAL(0.065) * ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(-0.5) * t3 * t3);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_wyman_ybar)(ALWAN_CORE_T lambda) {
    ALWAN_CORE_T t1 = (lambda - ALWAN_CORE_LITERAL(568.8)) *
        ALWAN_CORE_SELECT(lambda < ALWAN_CORE_LITERAL(568.8),
                     ALWAN_CORE_LITERAL(0.0213), ALWAN_CORE_LITERAL(0.0247));
    ALWAN_CORE_T t2 = (lambda - ALWAN_CORE_LITERAL(530.9)) *
        ALWAN_CORE_SELECT(lambda < ALWAN_CORE_LITERAL(530.9),
                     ALWAN_CORE_LITERAL(0.0613), ALWAN_CORE_LITERAL(0.0322));

    return ALWAN_CORE_LITERAL(0.821) * ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(-0.5) * t1 * t1)
         + ALWAN_CORE_LITERAL(0.286) * ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(-0.5) * t2 * t2);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_wyman_zbar)(ALWAN_CORE_T lambda) {
    ALWAN_CORE_T t1 = (lambda - ALWAN_CORE_LITERAL(437.0)) *
        ALWAN_CORE_SELECT(lambda < ALWAN_CORE_LITERAL(437.0),
                     ALWAN_CORE_LITERAL(0.0845), ALWAN_CORE_LITERAL(0.0278));
    ALWAN_CORE_T t2 = (lambda - ALWAN_CORE_LITERAL(459.0)) *
        ALWAN_CORE_SELECT(lambda < ALWAN_CORE_LITERAL(459.0),
                     ALWAN_CORE_LITERAL(0.0385), ALWAN_CORE_LITERAL(0.0725));

    return ALWAN_CORE_LITERAL(1.217) * ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(-0.5) * t1 * t1)
         + ALWAN_CORE_LITERAL(0.681) * ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(-0.5) * t2 * t2);
}

/* ================================================================
 * Single wavelength -> XYZ via Wyman 2013 fit
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_hero_wavelength_to_xyz)(ALWAN_CORE_T lambda) {
    ALWAN_CORE_XYZ result;
    result.x = ALWAN_CORE_FNVLIT(alwan_wyman_xbar)(lambda);
    result.y = ALWAN_CORE_FNVLIT(alwan_wyman_ybar)(lambda);
    result.z = ALWAN_CORE_FNVLIT(alwan_wyman_zbar)(lambda);
    return result;
}

/* ================================================================
 * Hero Wavelength PDF
 *
 * Probability density for importance sampling the visible spectrum.
 * Uses a simple uniform distribution over [380, 780] nm for now.
 * (A shaped PDF using the luminous efficiency function would be
 *  more efficient but this keeps the implementation simple.)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_hero_wavelength_pdf)(ALWAN_CORE_T lambda) {
    (void)lambda;
    return ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(400.0);  /* 1 / (780 - 380) */
}

/* ================================================================
 * Hero Wavelength Sample
 *
 * Map uniform [0,1] -> wavelength [380, 780] nm via CDF inversion.
 * With uniform PDF, this is simply linear interpolation.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_hero_wavelength_sample)(ALWAN_CORE_T u) {
    return ALWAN_CORE_LITERAL(380.0) + u * ALWAN_CORE_LITERAL(400.0);
}

/* ================================================================
 * Hero Wavelength Stratified Sampling
 *
 * Generate N stratified wavelengths from a single hero wavelength.
 * Each wavelength is offset by i/N * range, wrapping around.
 *
 * hero - the primary (hero) wavelength
 * i    - index of the wavelength to generate [0, N-1]
 * N    - total number of stratified wavelengths
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_hero_wavelength_stratified)(
    ALWAN_CORE_T hero,
    int i,
    int N) {
    ALWAN_CORE_T range = ALWAN_CORE_LITERAL(400.0);  /* 780 - 380 */
    ALWAN_CORE_T offset = (ALWAN_CORE_T)i / (ALWAN_CORE_T)N * range;
    ALWAN_CORE_T lambda = hero + offset;

    /* Wrap to [380, 780] */
    lambda = ALWAN_CORE_SELECT(lambda > ALWAN_CORE_LITERAL(780.0),
                           lambda - range, lambda);

    return lambda;
}
