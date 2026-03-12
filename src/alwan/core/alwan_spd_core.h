/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only spectral primitives (per-element math)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_SPD_CORE_H
#define ALWAN_SPD_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

/* Planck's law: spectral radiance at a given wavelength and temperature
 * wavelength_m:   wavelength in metres
 * temperature_K:  blackbody temperature in Kelvin
 * Returns:        spectral radiance in W*sr^-1*m^-3 */
ALWAN_INLINE alwan_scalar spd_planck_radiance_v(alwan_scalar wavelength_m,
                                                 alwan_scalar temperature_K) {
    alwan_scalar const c1 = ALWAN_LITERAL(3.741771e-16);  /* W*m^2 (first radiation constant) */
    alwan_scalar const c2 = ALWAN_LITERAL(1.4388e-2);     /* m*K  (second radiation constant) */
    alwan_scalar lambda5 = wavelength_m * wavelength_m * wavelength_m
                         * wavelength_m * wavelength_m;
    alwan_scalar exponent = c2 / (wavelength_m * temperature_K);
    alwan_scalar denominator = lambda5 * (ALWAN_EXP(exponent) - ALWAN_LITERAL(1.0));
    if (ALWAN_ABS(denominator) < ALWAN_LITERAL(1e-100)) return ALWAN_LITERAL(0.0);
    return c1 / denominator;
}

#endif /* ALWAN_SPD_CORE_H */
