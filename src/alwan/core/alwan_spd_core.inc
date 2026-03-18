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
    ALWAN_CORE_T const c1 = ALWAN_CORE_LITERAL(3.741771e-16);  /* W*m^2 (first radiation constant) */
    ALWAN_CORE_T const c2 = ALWAN_CORE_LITERAL(1.4388e-2);     /* m*K  (second radiation constant) */
    ALWAN_CORE_T lambda5 = wavelength_m * wavelength_m * wavelength_m
                         * wavelength_m * wavelength_m;
    ALWAN_CORE_T exponent = c2 / (wavelength_m * temperature_K);
    ALWAN_CORE_T denominator = lambda5 * (ALWAN_CORE_EXP(exponent) - ALWAN_CORE_LITERAL(1.0));
    if (ALWAN_CORE_ABS(denominator) < ALWAN_CORE_LITERAL(1e-100)) return ALWAN_CORE_LITERAL(0.0);
    return c1 / denominator;
}
