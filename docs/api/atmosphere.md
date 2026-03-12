# Atmospheric Optics API

Functions for Rayleigh scattering and atmospheric optical effects.

---

## Overview

Rayleigh scattering models the wavelength-dependent scattering of light by atmospheric molecules. This is responsible for the blue sky and red sunsets. Implementation follows **Bodhaine et al. (1999)**.

---

## Atmospheric Parameters

```c
typedef struct {
    alwan_scalar CO2_concentration;  /* CO2 in ppm (default: 300) */
    alwan_scalar temperature;        /* Temperature in Kelvin (default: 288.15) */
    alwan_scalar pressure;           /* Pressure in Pascals (default: 101325) */
    alwan_scalar latitude;           /* Latitude in degrees (default: 0) */
    alwan_scalar altitude;           /* Altitude in meters (default: 0) */
} alwan_atmosphere_params;

void alwan_atmosphere_params_default(alwan_atmosphere_params *params);
```

---

## Functions

### alwan_rayleigh_cross_section

```c
alwan_scalar alwan_rayleigh_cross_section(alwan_scalar wavelength_nm,
                                          alwan_atmosphere_params const *params);
```

Rayleigh scattering cross section per molecule (sigma). Van de Hulst (1957) method with Bodhaine et al. (1999) corrections. Pass `NULL` for `params` to use defaults (only CO2 and temperature are used).

**Returns:** Cross section in cm^2.

### alwan_rayleigh_optical_depth

```c
alwan_scalar alwan_rayleigh_optical_depth(alwan_scalar wavelength_nm,
                                          alwan_atmosphere_params const *params);
```

Rayleigh optical depth through the atmosphere. Pass `NULL` for `params` to use defaults.

**Returns:** Optical depth (dimensionless).

### alwan_rayleigh_spd

```c
int alwan_rayleigh_spd(alwan_scalar wavelength_start, alwan_scalar wavelength_end,
                       alwan_scalar wavelength_step,
                       alwan_atmosphere_params const *params,
                       alwan_scalar *out, int *out_count);
```

Generate Rayleigh optical depth values across a wavelength range.

**Parameters:**
- `wavelength_start`, `wavelength_end`, `wavelength_step` — Wavelength range in nm
- `params` — Atmospheric parameters (NULL for defaults)
- `out` — Output array (must be large enough for (end-start)/step + 1 values)
- `out_count` — Receives number of values written

**Example:**
```c
alwan_atmosphere_params params;
alwan_atmosphere_params_default(&params);
params.altitude = 2000.0;  /* Mountain observatory */

/* Optical depth at 550nm (green) */
alwan_scalar tau = alwan_rayleigh_optical_depth(550.0, &params);
printf("Optical depth at 550nm: %.4f\n", tau);

/* Full visible spectrum */
alwan_scalar depths[81];
int count;
alwan_rayleigh_spd(380.0, 780.0, 5.0, &params, depths, &count);
```

---

## Error Codes

- Returns 0 on success, -1 on error for `alwan_rayleigh_spd`
- Cross section and optical depth return negative on error

---

## See Also

- [Spectral Operations](spectral.md) — SPD operations
- [Vision Science](vision.md) — Human visual perception
