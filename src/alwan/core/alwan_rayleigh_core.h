/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Rayleigh Scattering
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Bodhaine et al. (1999), colour-science implementation
 */

#ifndef ALWAN_RAYLEIGH_CORE_H
#define ALWAN_RAYLEIGH_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

/* ================================================================
 * Constants (from colour-science)
 * ================================================================ */

#define RAYLEIGH_V_AVOGADRO           ALWAN_LITERAL(6.02214179e+23)
#define RAYLEIGH_V_STD_TEMPERATURE    ALWAN_LITERAL(288.15)
#define RAYLEIGH_V_STD_CO2            ALWAN_LITERAL(300.0)
#define RAYLEIGH_V_STD_PRESSURE       ALWAN_LITERAL(101325.0)

/* ================================================================
 * Molecular density N_s (molecules/cm^3)
 * N_s = (AVOGADRO / 22.4141) * (273.15 / T) * (1/1000)
 * ================================================================ */

ALWAN_INLINE alwan_scalar rayleigh_molecular_density_v(alwan_scalar temperature) {
    return (RAYLEIGH_V_AVOGADRO / ALWAN_LITERAL(22.4141)) *
           (ALWAN_LITERAL(273.15) / temperature) *
           (ALWAN_ONE / ALWAN_LITERAL(1000.0));
}

/* ================================================================
 * Air Refraction Index -- Peck & Reeder (1972)
 * wavelength in micrometers
 * ================================================================ */

ALWAN_INLINE alwan_scalar rayleigh_refraction_peck1972_v(alwan_scalar wl_um) {
    alwan_scalar wl2_inv = ALWAN_ONE / (wl_um * wl_um);
    alwan_scalar n_minus_1 = ALWAN_LITERAL(8060.51) +
        ALWAN_LITERAL(2480990.0) / (ALWAN_LITERAL(132.274) - wl2_inv) +
        ALWAN_LITERAL(17455.7) / (ALWAN_LITERAL(39.32957) - wl2_inv);
    return n_minus_1 / ALWAN_LITERAL(1.0e8) + ALWAN_ONE;
}

/* ================================================================
 * Air Refraction Index -- Bodhaine (1999)
 * wavelength in micrometers, CO2 in ppm
 * ================================================================ */

ALWAN_INLINE alwan_scalar rayleigh_refraction_bodhaine1999_v(alwan_scalar wl_um,
                                                               alwan_scalar CO2_ppm) {
    alwan_scalar CO2_ppv = CO2_ppm * ALWAN_LITERAL(1.0e-6);
    alwan_scalar n_peck = rayleigh_refraction_peck1972_v(wl_um);
    return (ALWAN_ONE + ALWAN_LITERAL(0.54) * (CO2_ppv - ALWAN_LITERAL(300.0e-6))) *
           (n_peck - ALWAN_ONE) + ALWAN_ONE;
}

/* ================================================================
 * Depolarisation (King factors)
 * wavelength in micrometers
 * ================================================================ */

/* N2 depolarisation */
ALWAN_INLINE alwan_scalar rayleigh_N2_depolarisation_v(alwan_scalar wl_um) {
    alwan_scalar wl2_inv = ALWAN_ONE / (wl_um * wl_um);
    return ALWAN_LITERAL(1.034) + ALWAN_LITERAL(3.17e-4) * wl2_inv;
}

/* O2 depolarisation */
ALWAN_INLINE alwan_scalar rayleigh_O2_depolarisation_v(alwan_scalar wl_um) {
    alwan_scalar wl2_inv = ALWAN_ONE / (wl_um * wl_um);
    alwan_scalar wl4_inv = wl2_inv * wl2_inv;
    return ALWAN_LITERAL(1.096) + ALWAN_LITERAL(1.385e-3) * wl2_inv +
           ALWAN_LITERAL(1.448e-4) * wl4_inv;
}

/* ================================================================
 * Air depolarisation F(air) -- Bodhaine (1999)
 * wavelength in micrometers, CO2 in ppm
 * ================================================================ */

ALWAN_INLINE alwan_scalar rayleigh_F_air_bodhaine1999_v(alwan_scalar wl_um,
                                                          alwan_scalar CO2_ppm) {
    alwan_scalar O2 = rayleigh_O2_depolarisation_v(wl_um);
    alwan_scalar N2 = rayleigh_N2_depolarisation_v(wl_um);
    alwan_scalar CO2_c = CO2_ppm * ALWAN_LITERAL(1.0e-4);

    return (ALWAN_LITERAL(78.084) * N2 + ALWAN_LITERAL(20.946) * O2 +
            ALWAN_LITERAL(0.934) * ALWAN_ONE + CO2_c * ALWAN_LITERAL(1.15)) /
           (ALWAN_LITERAL(78.084) + ALWAN_LITERAL(20.946) +
            ALWAN_LITERAL(0.934) + CO2_c);
}

/* ================================================================
 * Mean molecular weight for dry air (CO2 in ppm)
 * ================================================================ */

ALWAN_INLINE alwan_scalar rayleigh_mean_molecular_weight_v(alwan_scalar CO2_ppm) {
    alwan_scalar CO2_ppv = CO2_ppm * ALWAN_LITERAL(1.0e-6);
    return ALWAN_LITERAL(15.0556) * CO2_ppv + ALWAN_LITERAL(28.9595);
}

/* ================================================================
 * Gravity -- List (1968)
 * latitude in degrees, altitude in meters
 * Returns gravity in cm/s^2 (gal)
 * ================================================================ */

ALWAN_INLINE alwan_scalar rayleigh_gravity_list1968_v(alwan_scalar latitude,
                                                        alwan_scalar altitude) {
    alwan_scalar cos2phi = ALWAN_COS(ALWAN_LITERAL(2.0) * latitude * ALWAN_PI /
                                      ALWAN_LITERAL(180.0));

    alwan_scalar g0 = ALWAN_LITERAL(980.6160) *
        (ALWAN_ONE - ALWAN_LITERAL(0.0026373) * cos2phi +
         ALWAN_LITERAL(0.0000059) * cos2phi * cos2phi);

    return g0
        - (ALWAN_LITERAL(3.085462e-4) + ALWAN_LITERAL(2.27e-7) * cos2phi) * altitude
        + (ALWAN_LITERAL(7.254e-11) + ALWAN_LITERAL(1.0e-13) * cos2phi) * altitude * altitude
        - (ALWAN_LITERAL(1.517e-17) + ALWAN_LITERAL(6.0e-20) * cos2phi) * altitude * altitude * altitude;
}

/* ================================================================
 * Rayleigh scattering cross section per molecule (cm^2)
 * Van de Hulst (1957) method
 * wavelength_nm in nanometers
 * ================================================================ */

ALWAN_INLINE alwan_scalar rayleigh_cross_section_v(alwan_scalar wavelength_nm,
                                                     alwan_scalar CO2_ppm,
                                                     alwan_scalar temperature) {
    alwan_scalar wl_cm = wavelength_nm * ALWAN_LITERAL(1.0e-7);
    alwan_scalar wl_um = wl_cm * ALWAN_LITERAL(1.0e4);

    alwan_scalar N_s = rayleigh_molecular_density_v(temperature);
    alwan_scalar n_s = rayleigh_refraction_bodhaine1999_v(wl_um, CO2_ppm);
    alwan_scalar F_air = rayleigh_F_air_bodhaine1999_v(wl_um, CO2_ppm);

    alwan_scalar n_s2 = n_s * n_s;
    alwan_scalar n_s2_minus_1 = n_s2 - ALWAN_ONE;
    alwan_scalar n_s2_plus_2 = n_s2 + ALWAN_LITERAL(2.0);

    alwan_scalar wl_cm4 = wl_cm * wl_cm * wl_cm * wl_cm;
    alwan_scalar sigma = ALWAN_LITERAL(24.0) * ALWAN_PI * ALWAN_PI * ALWAN_PI *
                         (n_s2_minus_1 * n_s2_minus_1);
    sigma /= wl_cm4 * (N_s * N_s) * (n_s2_plus_2 * n_s2_plus_2);
    sigma *= F_air;

    return sigma;
}

/* ================================================================
 * Rayleigh optical depth (dimensionless)
 * wavelength_nm in nanometers
 * ================================================================ */

ALWAN_INLINE alwan_scalar rayleigh_optical_depth_v(alwan_scalar wavelength_nm,
                                                     alwan_scalar CO2_ppm,
                                                     alwan_scalar temperature,
                                                     alwan_scalar pressure,
                                                     alwan_scalar latitude,
                                                     alwan_scalar altitude) {
    alwan_scalar sigma = rayleigh_cross_section_v(wavelength_nm, CO2_ppm, temperature);
    alwan_scalar m_a = rayleigh_mean_molecular_weight_v(CO2_ppm);
    alwan_scalar g = rayleigh_gravity_list1968_v(latitude, altitude);
    alwan_scalar P_dyncm2 = pressure * ALWAN_LITERAL(10.0);

    return sigma * (P_dyncm2 * RAYLEIGH_V_AVOGADRO) / (m_a * g);
}

#endif /* ALWAN_RAYLEIGH_CORE_H */
