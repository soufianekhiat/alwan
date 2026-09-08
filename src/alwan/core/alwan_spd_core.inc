/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only spectral primitives (per-element math, parameterized)
 * No include guard - included twice via setup/teardown.
 */

/* Planck's law: spectral radiance at a given wavelength and temperature
 * wavelength_m:   wavelength in metres
 * temperature_K:  blackbody temperature in Kelvin
 * Returns:        spectral radiance in W*sr^-1*m^-3 */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(spd_planck_radiance)(ALWAN_CORE_T wavelength_m,
                                                 ALWAN_CORE_T temperature_K) {
    /* c1 = 2*pi*h*c^2 is the first radiation constant, so c1/(lambda^5 (e^x -1))
     * is spectral radiant EXITANCE in W*m^-3. This function is documented, here
     * and in the public header, as spectral RADIANCE in W*sr^-1*m^-3, which for
     * a Lambertian emitter is exitance / pi. The division was missing, so every
     * value came out pi times the documented quantity and pi times what
     * colour-science's planck_law returns for the same input. Chromaticity is
     * unaffected, being scale-invariant; absolute values were not. */
    const ALWAN_CORE_T c1 = ALWAN_CORE_LITERAL(3.741771e-16);  /* W*m^2 (first radiation constant) */
    const ALWAN_CORE_T c2 = ALWAN_CORE_LITERAL(1.4388e-2);     /* m*K  (second radiation constant) */
    ALWAN_CORE_T lambda5 = wavelength_m * wavelength_m * wavelength_m
                         * wavelength_m * wavelength_m;
    ALWAN_CORE_T exponent = c2 / (wavelength_m * temperature_K);
    ALWAN_CORE_T denominator = lambda5 * (ALWAN_CORE_EXP(exponent) - ALWAN_CORE_LITERAL(1.0));
    /* 1e-37, not 1e-100. This template is instantiated at both precisions, and
     * 1e-100f is not a representable float: the f32 pass got an out-of-range
     * constant, which is undefined behaviour and in practice folds to zero, so
     * the guard was dead there and a zero denominator reached the division.
     * nvcc rejects the literal outright, which is how this surfaced.
     *
     * 1e-37 is representable in both and is not reachable by any real
     * temperature. The denominator is lambda^5 (exp(c2/(lambda T)) - 1), and
     * lambda^5 is at least 6e-33 over the visible range, so falling below 1e-37
     * needs exp(x) - 1 < 1.6e-5, which needs T above 2.5e9 K. */
    if (ALWAN_CORE_ABS(denominator) < ALWAN_CORE_LITERAL(1e-37)) return ALWAN_CORE_LITERAL(0.0);
    return c1 / (ALWAN_CORE_PI * denominator);
}
