/*
 * Rayleigh Scattering Implementation
 * Reference: Bodhaine et al. (1999), colour-science implementation
 *
 * All formulas and constants from colour-science 0.4.6:
 * https://github.com/colour-science/colour/blob/develop/colour/phenomena/rayleigh.py
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <math.h>

/* Constants from colour-science */
#define AVOGADRO_CONSTANT           6.02214179e+23   /* molecules/mol (CODATA value) */
#define STANDARD_AIR_TEMPERATURE    288.15           /* K */
#define STANDARD_CO2_CONCENTRATION  300.0            /* ppm */
#define AVERAGE_PRESSURE_SEA_LEVEL  101325.0         /* Pa */
#define DEFAULT_LATITUDE            0.0              /* degrees */
#define DEFAULT_ALTITUDE            0.0              /* meters */

/* Internal: Molecular density N_s (molecules/cm^3) */
static alwan_scalar molecular_density(alwan_scalar temperature)
{
    /* N_s = (AVOGADRO / 22.4141) * (273.15 / T) * (1/1000) */
    return (AVOGADRO_CONSTANT / 22.4141) * (273.15 / temperature) * (1.0 / 1000.0);
}

/* Internal: Air refraction index using Peck & Reeder (1972) method
 * wavelength in micrometers */
static alwan_scalar air_refraction_index_Peck1972(alwan_scalar wl_um)
{
    alwan_scalar wl2_inv = 1.0 / (wl_um * wl_um);
    alwan_scalar n;

    n = 8060.51 + 2480990.0 / (132.274 - wl2_inv) + 17455.7 / (39.32957 - wl2_inv);
    n /= 1.0e8;
    n += 1.0;

    return n;
}

/* Internal: Air refraction index using Bodhaine (1999) method
 * wavelength in micrometers, CO2 in ppm */
static alwan_scalar air_refraction_index_Bodhaine1999(alwan_scalar wl_um,
                                                       alwan_scalar CO2_ppm)
{
    /* Convert ppm to parts per volume */
    alwan_scalar CO2_ppv = CO2_ppm * 1.0e-6;

    alwan_scalar n_peck = air_refraction_index_Peck1972(wl_um);

    return (1.0 + 0.54 * (CO2_ppv - 300.0e-6)) * (n_peck - 1.0) + 1.0;
}

/* Internal: Nitrogen N2 depolarisation (King factor)
 * wavelength in micrometers */
static alwan_scalar N2_depolarisation(alwan_scalar wl_um)
{
    alwan_scalar wl2_inv = 1.0 / (wl_um * wl_um);
    return 1.034 + 3.17e-4 * wl2_inv;
}

/* Internal: Oxygen O2 depolarisation (King factor)
 * wavelength in micrometers */
static alwan_scalar O2_depolarisation(alwan_scalar wl_um)
{
    alwan_scalar wl2_inv = 1.0 / (wl_um * wl_um);
    alwan_scalar wl4_inv = wl2_inv * wl2_inv;
    return 1.096 + 1.385e-3 * wl2_inv + 1.448e-4 * wl4_inv;
}

/* Internal: Air depolarisation F(air) using Bodhaine (1999) method
 * wavelength in micrometers, CO2 in ppm */
static alwan_scalar F_air_Bodhaine1999(alwan_scalar wl_um, alwan_scalar CO2_ppm)
{
    alwan_scalar O2 = O2_depolarisation(wl_um);
    alwan_scalar N2 = N2_depolarisation(wl_um);

    /* Convert ppm to parts per volume per percent */
    alwan_scalar CO2_c = CO2_ppm * 1.0e-4;

    /* Atmospheric composition weighting:
     * N2: 78.084%, O2: 20.946%, Ar: 0.934%, CO2: variable */
    alwan_scalar F_air = (78.084 * N2 + 20.946 * O2 + 0.934 * 1.0 + CO2_c * 1.15) /
                         (78.084 + 20.946 + 0.934 + CO2_c);

    return F_air;
}

/* Internal: Mean molecular weights for dry air
 * CO2 in ppm */
static alwan_scalar mean_molecular_weights(alwan_scalar CO2_ppm)
{
    alwan_scalar CO2_ppv = CO2_ppm * 1.0e-6;
    return 15.0556 * CO2_ppv + 28.9595;
}

/* Internal: Gravity using List (1968) method
 * latitude in degrees, altitude in meters
 * Returns gravity in cm/s^2 (gal) */
static alwan_scalar gravity_List1968(alwan_scalar latitude, alwan_scalar altitude)
{
    alwan_scalar cos2phi = cos(2.0 * latitude * ALWAN_PI / 180.0);

    /* Sea level acceleration of gravity */
    alwan_scalar g0 = 980.6160 * (1.0 - 0.0026373 * cos2phi + 0.0000059 * cos2phi * cos2phi);

    /* Altitude correction */
    alwan_scalar g = g0
        - (3.085462e-4 + 2.27e-7 * cos2phi) * altitude
        + (7.254e-11 + 1.0e-13 * cos2phi) * altitude * altitude
        - (1.517e-17 + 6.0e-20 * cos2phi) * altitude * altitude * altitude;

    return g;
}

/* Initialize atmosphere parameters with defaults */
void alwan_atmosphere_params_default(alwan_atmosphere_params *params)
{
    if (params) {
        params->CO2_concentration = STANDARD_CO2_CONCENTRATION;
        params->temperature = STANDARD_AIR_TEMPERATURE;
        params->pressure = AVERAGE_PRESSURE_SEA_LEVEL;
        params->latitude = DEFAULT_LATITUDE;
        params->altitude = DEFAULT_ALTITUDE;
    }
}

/* Rayleigh scattering cross section per molecule
 * Van de Hulst (1957) method
 * wavelength_nm: wavelength in nanometers
 * Returns: cross section in cm^2 */
alwan_scalar alwan_rayleigh_cross_section(alwan_scalar wavelength_nm,
                                           alwan_atmosphere_params const *params)
{
    alwan_atmosphere_params defaults;
    alwan_scalar CO2_ppm, temperature;
    alwan_scalar wl_cm, wl_um;
    alwan_scalar N_s, n_s, F_air;
    alwan_scalar n_s2, n_s2_minus_1, n_s2_plus_2;
    alwan_scalar sigma;

    /* Use defaults if params is NULL */
    if (params) {
        CO2_ppm = params->CO2_concentration;
        temperature = params->temperature;
    } else {
        alwan_atmosphere_params_default(&defaults);
        CO2_ppm = defaults.CO2_concentration;
        temperature = defaults.temperature;
    }

    /* Convert wavelength: nm -> cm -> micrometers */
    wl_cm = wavelength_nm * 1.0e-7;
    wl_um = wl_cm * 1.0e4;  /* cm to micrometers */

    /* Molecular density (molecules/cm^3) */
    N_s = molecular_density(temperature);

    /* Air refraction index */
    n_s = air_refraction_index_Bodhaine1999(wl_um, CO2_ppm);

    /* Air depolarisation (King factor) */
    F_air = F_air_Bodhaine1999(wl_um, CO2_ppm);

    /* Scattering cross section formula:
     * sigma = 24 * pi^3 * (n_s^2 - 1)^2 / (wl^4 * N_s^2 * (n_s^2 + 2)^2) * F_air */
    n_s2 = n_s * n_s;
    n_s2_minus_1 = n_s2 - 1.0;
    n_s2_plus_2 = n_s2 + 2.0;

    sigma = 24.0 * ALWAN_PI * ALWAN_PI * ALWAN_PI * (n_s2_minus_1 * n_s2_minus_1);
    sigma /= (wl_cm * wl_cm * wl_cm * wl_cm) * (N_s * N_s) * (n_s2_plus_2 * n_s2_plus_2);
    sigma *= F_air;

    return sigma;
}

/* Rayleigh optical depth through atmosphere
 * wavelength_nm: wavelength in nanometers
 * Returns: optical depth (dimensionless) */
alwan_scalar alwan_rayleigh_optical_depth(alwan_scalar wavelength_nm,
                                           alwan_atmosphere_params const *params)
{
    alwan_atmosphere_params defaults;
    alwan_scalar CO2_ppm, temperature, pressure, latitude, altitude;
    alwan_scalar sigma, m_a, g, P_dyncm2;
    alwan_scalar T_R;

    /* Use defaults if params is NULL */
    if (params) {
        CO2_ppm = params->CO2_concentration;
        temperature = params->temperature;
        pressure = params->pressure;
        latitude = params->latitude;
        altitude = params->altitude;
    } else {
        alwan_atmosphere_params_default(&defaults);
        CO2_ppm = defaults.CO2_concentration;
        temperature = defaults.temperature;
        pressure = defaults.pressure;
        latitude = defaults.latitude;
        altitude = defaults.altitude;
    }

    /* Scattering cross section */
    sigma = alwan_rayleigh_cross_section(wavelength_nm, params);

    /* Mean molecular weights */
    m_a = mean_molecular_weights(CO2_ppm);

    /* Gravity in cm/s^2 */
    g = gravity_List1968(latitude, altitude);

    /* Convert pressure from Pa to dyne/cm^2 */
    P_dyncm2 = pressure * 10.0;

    /* Optical depth:
     * T_R = sigma * (P * AVOGADRO) / (m_a * g) */
    T_R = sigma * (P_dyncm2 * AVOGADRO_CONSTANT) / (m_a * g);

    return T_R;
}

/* Rayleigh scattering spectral distribution */
int alwan_rayleigh_spd(alwan_scalar wavelength_start, alwan_scalar wavelength_end,
                        alwan_scalar wavelength_step,
                        alwan_atmosphere_params const *params,
                        alwan_scalar *out, int *out_count)
{
    alwan_scalar wl;
    int count = 0;

    if (!out || !out_count) {
        return -1;
    }

    if (wavelength_step <= 0 || wavelength_start > wavelength_end) {
        return -1;
    }

    for (wl = wavelength_start; wl <= wavelength_end; wl += wavelength_step) {
        out[count++] = alwan_rayleigh_optical_depth(wl, params);
    }

    *out_count = count;
    return 0;
}
