/* ================================================================
 * Alwan - Rayleigh Scattering
 * Thin wrapper -- per-pixel math in alwan_rayleigh_core.h
 *
 * Only NULL-param defaults and the SPD loop live here.
 * ================================================================ */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_rayleigh_core.h"
#include <math.h>

/* Initialize atmosphere parameters with defaults */
void alwan_atmosphere_params_default(alwan_atmosphere_params *params)
{
    if (params) {
        params->CO2_concentration = RAYLEIGH_V_STD_CO2;
        params->temperature       = RAYLEIGH_V_STD_TEMPERATURE;
        params->pressure          = RAYLEIGH_V_STD_PRESSURE;
        params->latitude          = ALWAN_LITERAL(0.0);
        params->altitude          = ALWAN_LITERAL(0.0);
    }
}

/* Rayleigh scattering cross section -- delegates to core */
alwan_scalar alwan_rayleigh_cross_section(alwan_scalar wavelength_nm,
                                           alwan_atmosphere_params const *params)
{
    alwan_atmosphere_params defaults;
    if (!params) {
        alwan_atmosphere_params_default(&defaults);
        params = &defaults;
    }

    return rayleigh_cross_section_v(wavelength_nm,
                                     params->CO2_concentration,
                                     params->temperature);
}

/* Rayleigh optical depth -- delegates to core */
alwan_scalar alwan_rayleigh_optical_depth(alwan_scalar wavelength_nm,
                                           alwan_atmosphere_params const *params)
{
    alwan_atmosphere_params defaults;
    if (!params) {
        alwan_atmosphere_params_default(&defaults);
        params = &defaults;
    }

    return rayleigh_optical_depth_v(wavelength_nm,
                                     params->CO2_concentration,
                                     params->temperature,
                                     params->pressure,
                                     params->latitude,
                                     params->altitude);
}

/* Rayleigh scattering spectral distribution -- loop stays in .c */
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
