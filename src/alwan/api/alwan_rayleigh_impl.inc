/* No include guard -- included twice (f32 and f64 passes) */

/* Initialize atmosphere parameters with defaults */
void ALWAN_CORE_FNLIT(alwan_atmosphere_params_default)(ALWAN_CORE_ATMOSPHERE_PARAMS *params)
{
    if (params) {
        /* Standard atmosphere (NIST; CIE 15:2018 4.2) */
        params->CO2_concentration = ALWAN_CORE_LITERAL(300.0);
        params->temperature       = ALWAN_CORE_LITERAL(288.15);
        params->pressure          = ALWAN_CORE_LITERAL(101325.0);
        params->latitude          = ALWAN_CORE_LITERAL(0.0);
        params->altitude          = ALWAN_CORE_LITERAL(0.0);
    }
}

/* Rayleigh scattering cross section */
ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_rayleigh_cross_section)(ALWAN_CORE_T wavelength_nm,
                                           ALWAN_CORE_ATMOSPHERE_PARAMS const *params)
{
    ALWAN_CORE_ATMOSPHERE_PARAMS defaults;
    if (!params) {
        ALWAN_CORE_FNLIT(alwan_atmosphere_params_default)(&defaults);
        params = &defaults;
    }

    return ALWAN_CORE_FNVLIT(rayleigh_cross_section)(wavelength_nm,
                                     params->CO2_concentration,
                                     params->temperature);
}

/* Rayleigh optical depth */
ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_rayleigh_optical_depth)(ALWAN_CORE_T wavelength_nm,
                                           ALWAN_CORE_ATMOSPHERE_PARAMS const *params)
{
    ALWAN_CORE_ATMOSPHERE_PARAMS defaults;
    if (!params) {
        ALWAN_CORE_FNLIT(alwan_atmosphere_params_default)(&defaults);
        params = &defaults;
    }

    return ALWAN_CORE_FNVLIT(rayleigh_optical_depth)(wavelength_nm,
                                     params->CO2_concentration,
                                     params->temperature,
                                     params->pressure,
                                     params->latitude,
                                     params->altitude);
}

/* Rayleigh scattering spectral distribution */
alwan_status ALWAN_CORE_FNLIT(alwan_rayleigh_spd)(ALWAN_CORE_T wavelength_start, ALWAN_CORE_T wavelength_end,
                        ALWAN_CORE_T wavelength_step,
                        ALWAN_CORE_ATMOSPHERE_PARAMS const *params,
                        ALWAN_CORE_T *out, int *out_count)
{
    ALWAN_CORE_T wl;
    int count = 0;

    if (!out || !out_count) {
        return ALWAN_E_INVALID;
    }

    if (wavelength_step <= ALWAN_CORE_LITERAL(0.0) || wavelength_start > wavelength_end) {
        return ALWAN_E_RANGE;
    }

    for (wl = wavelength_start; wl <= wavelength_end; wl += wavelength_step) {
        out[count++] = ALWAN_CORE_FNLIT(alwan_rayleigh_optical_depth)(wl, params);
    }

    *out_count = count;
    return ALWAN_OK;
}
