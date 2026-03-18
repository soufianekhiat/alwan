/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Rayleigh Scattering (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Bodhaine et al. (1999), colour-science implementation
 */

/* ================================================================
 * Constants (from colour-science)
 * ================================================================ */

#define RAYLEIGH_V_AVOGADRO           ALWAN_CORE_LITERAL(6.02214179e+23)
#define RAYLEIGH_V_STD_TEMPERATURE    ALWAN_CORE_LITERAL(288.15)
#define RAYLEIGH_V_STD_CO2            ALWAN_CORE_LITERAL(300.0)
#define RAYLEIGH_V_STD_PRESSURE       ALWAN_CORE_LITERAL(101325.0)

/* ================================================================
 * Molecular density N_s (molecules/cm^3)
 * N_s = (AVOGADRO / 22.4141) * (273.15 / T) * (1/1000)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(rayleigh_molecular_density)(ALWAN_CORE_T temperature) {
    return (RAYLEIGH_V_AVOGADRO / ALWAN_CORE_LITERAL(22.4141)) *
           (ALWAN_CORE_LITERAL(273.15) / temperature) *
           (ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(1000.0));
}

/* ================================================================
 * Air Refraction Index -- Peck & Reeder (1972)
 * wavelength in micrometers
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(rayleigh_refraction_peck1972)(ALWAN_CORE_T wl_um) {
    ALWAN_CORE_T wl2_inv = ALWAN_CORE_ONE / (wl_um * wl_um);
    ALWAN_CORE_T n_minus_1 = ALWAN_CORE_LITERAL(8060.51) +
        ALWAN_CORE_LITERAL(2480990.0) / (ALWAN_CORE_LITERAL(132.274) - wl2_inv) +
        ALWAN_CORE_LITERAL(17455.7) / (ALWAN_CORE_LITERAL(39.32957) - wl2_inv);
    return n_minus_1 / ALWAN_CORE_LITERAL(1.0e8) + ALWAN_CORE_ONE;
}

/* ================================================================
 * Air Refraction Index -- Bodhaine (1999)
 * wavelength in micrometers, CO2 in ppm
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(rayleigh_refraction_bodhaine1999)(ALWAN_CORE_T wl_um,
                                                               ALWAN_CORE_T CO2_ppm) {
    ALWAN_CORE_T CO2_ppv = CO2_ppm * ALWAN_CORE_LITERAL(1.0e-6);
    ALWAN_CORE_T n_peck = ALWAN_CORE_FNVLIT(rayleigh_refraction_peck1972)(wl_um);
    return (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(0.54) * (CO2_ppv - ALWAN_CORE_LITERAL(300.0e-6))) *
           (n_peck - ALWAN_CORE_ONE) + ALWAN_CORE_ONE;
}

/* ================================================================
 * Depolarisation (King factors)
 * wavelength in micrometers
 * ================================================================ */

/* N2 depolarisation */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(rayleigh_N2_depolarisation)(ALWAN_CORE_T wl_um) {
    ALWAN_CORE_T wl2_inv = ALWAN_CORE_ONE / (wl_um * wl_um);
    return ALWAN_CORE_LITERAL(1.034) + ALWAN_CORE_LITERAL(3.17e-4) * wl2_inv;
}

/* O2 depolarisation */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(rayleigh_O2_depolarisation)(ALWAN_CORE_T wl_um) {
    ALWAN_CORE_T wl2_inv = ALWAN_CORE_ONE / (wl_um * wl_um);
    ALWAN_CORE_T wl4_inv = wl2_inv * wl2_inv;
    return ALWAN_CORE_LITERAL(1.096) + ALWAN_CORE_LITERAL(1.385e-3) * wl2_inv +
           ALWAN_CORE_LITERAL(1.448e-4) * wl4_inv;
}

/* ================================================================
 * Air depolarisation F(air) -- Bodhaine (1999)
 * wavelength in micrometers, CO2 in ppm
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(rayleigh_F_air_bodhaine1999)(ALWAN_CORE_T wl_um,
                                                          ALWAN_CORE_T CO2_ppm) {
    ALWAN_CORE_T O2 = ALWAN_CORE_FNVLIT(rayleigh_O2_depolarisation)(wl_um);
    ALWAN_CORE_T N2 = ALWAN_CORE_FNVLIT(rayleigh_N2_depolarisation)(wl_um);
    ALWAN_CORE_T CO2_c = CO2_ppm * ALWAN_CORE_LITERAL(1.0e-4);

    return (ALWAN_CORE_LITERAL(78.084) * N2 + ALWAN_CORE_LITERAL(20.946) * O2 +
            ALWAN_CORE_LITERAL(0.934) * ALWAN_CORE_ONE + CO2_c * ALWAN_CORE_LITERAL(1.15)) /
           (ALWAN_CORE_LITERAL(78.084) + ALWAN_CORE_LITERAL(20.946) +
            ALWAN_CORE_LITERAL(0.934) + CO2_c);
}

/* ================================================================
 * Mean molecular weight for dry air (CO2 in ppm)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(rayleigh_mean_molecular_weight)(ALWAN_CORE_T CO2_ppm) {
    ALWAN_CORE_T CO2_ppv = CO2_ppm * ALWAN_CORE_LITERAL(1.0e-6);
    return ALWAN_CORE_LITERAL(15.0556) * CO2_ppv + ALWAN_CORE_LITERAL(28.9595);
}

/* ================================================================
 * Gravity -- List (1968)
 * latitude in degrees, altitude in meters
 * Returns gravity in cm/s^2 (gal)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(rayleigh_gravity_list1968)(ALWAN_CORE_T latitude,
                                                        ALWAN_CORE_T altitude) {
    ALWAN_CORE_T cos2phi = ALWAN_CORE_COS(ALWAN_CORE_LITERAL(2.0) * latitude * ALWAN_CORE_PI /
                                      ALWAN_CORE_LITERAL(180.0));

    ALWAN_CORE_T g0 = ALWAN_CORE_LITERAL(980.6160) *
        (ALWAN_CORE_ONE - ALWAN_CORE_LITERAL(0.0026373) * cos2phi +
         ALWAN_CORE_LITERAL(0.0000059) * cos2phi * cos2phi);

    return g0
        - (ALWAN_CORE_LITERAL(3.085462e-4) + ALWAN_CORE_LITERAL(2.27e-7) * cos2phi) * altitude
        + (ALWAN_CORE_LITERAL(7.254e-11) + ALWAN_CORE_LITERAL(1.0e-13) * cos2phi) * altitude * altitude
        - (ALWAN_CORE_LITERAL(1.517e-17) + ALWAN_CORE_LITERAL(6.0e-20) * cos2phi) * altitude * altitude * altitude;
}

/* ================================================================
 * Rayleigh scattering cross section per molecule (cm^2)
 * Van de Hulst (1957) method
 * wavelength_nm in nanometers
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(rayleigh_cross_section)(ALWAN_CORE_T wavelength_nm,
                                                     ALWAN_CORE_T CO2_ppm,
                                                     ALWAN_CORE_T temperature) {
    ALWAN_CORE_T wl_cm = wavelength_nm * ALWAN_CORE_LITERAL(1.0e-7);
    ALWAN_CORE_T wl_um = wl_cm * ALWAN_CORE_LITERAL(1.0e4);

    ALWAN_CORE_T N_s = ALWAN_CORE_FNVLIT(rayleigh_molecular_density)(temperature);
    ALWAN_CORE_T n_s = ALWAN_CORE_FNVLIT(rayleigh_refraction_bodhaine1999)(wl_um, CO2_ppm);
    ALWAN_CORE_T F_air = ALWAN_CORE_FNVLIT(rayleigh_F_air_bodhaine1999)(wl_um, CO2_ppm);

    ALWAN_CORE_T n_s2 = n_s * n_s;
    ALWAN_CORE_T n_s2_minus_1 = n_s2 - ALWAN_CORE_ONE;
    ALWAN_CORE_T n_s2_plus_2 = n_s2 + ALWAN_CORE_LITERAL(2.0);

    ALWAN_CORE_T wl_cm4 = wl_cm * wl_cm * wl_cm * wl_cm;
    ALWAN_CORE_T sigma = ALWAN_CORE_LITERAL(24.0) * ALWAN_CORE_PI * ALWAN_CORE_PI * ALWAN_CORE_PI *
                         (n_s2_minus_1 * n_s2_minus_1);
    sigma /= wl_cm4 * (N_s * N_s) * (n_s2_plus_2 * n_s2_plus_2);
    sigma *= F_air;

    return sigma;
}

/* ================================================================
 * Rayleigh optical depth (dimensionless)
 * wavelength_nm in nanometers
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(rayleigh_optical_depth)(ALWAN_CORE_T wavelength_nm,
                                                     ALWAN_CORE_T CO2_ppm,
                                                     ALWAN_CORE_T temperature,
                                                     ALWAN_CORE_T pressure,
                                                     ALWAN_CORE_T latitude,
                                                     ALWAN_CORE_T altitude) {
    ALWAN_CORE_T sigma = ALWAN_CORE_FNVLIT(rayleigh_cross_section)(wavelength_nm, CO2_ppm, temperature);
    ALWAN_CORE_T m_a = ALWAN_CORE_FNVLIT(rayleigh_mean_molecular_weight)(CO2_ppm);
    ALWAN_CORE_T g = ALWAN_CORE_FNVLIT(rayleigh_gravity_list1968)(latitude, altitude);
    ALWAN_CORE_T P_dyncm2 = pressure * ALWAN_CORE_LITERAL(10.0);

    return sigma * (P_dyncm2 * RAYLEIGH_V_AVOGADRO) / (m_a * g);
}

#undef RAYLEIGH_V_AVOGADRO
#undef RAYLEIGH_V_STD_TEMPERATURE
#undef RAYLEIGH_V_STD_CO2
#undef RAYLEIGH_V_STD_PRESSURE
