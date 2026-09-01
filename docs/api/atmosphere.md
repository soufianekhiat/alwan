# Atmospheric Optics API

Functions for Rayleigh scattering and atmospheric optical effects.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two
> forms: `name_f32` (single precision, `alwan_f32`) and `name_f64` (double precision,
> `alwan_f64`). `T = f32 | f64`. These are native dual-precision entry points; there is
> no unsuffixed/`alwan_scalar` alias for the Rayleigh API; pick the precision explicitly.
> Which precisions are compiled is gated by `ALWAN_WITH_F32` / `ALWAN_WITH_F64` (both by
> default; `ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64` restrict to one). See
> [precision-and-limits.md](../precision-and-limits.md) and
> [configuration.md](../configuration.md).

---

## Overview

Rayleigh scattering models the wavelength-dependent scattering of light by atmospheric
molecules. This is responsible for the blue sky and red sunsets. The implementation
follows **Bodhaine et al. (1999)** and is validated against the colour-science
reference implementation.

**Bodhaine (1999) coverage.** The model implements the full Bodhaine pipeline:

| Stage | Status | Where |
|-------|--------|-------|
| Refractive index of air `n(lambda)` (Peck & Reeder 1972 + Bodhaine CO2 correction) | implemented internally | building block of the cross section |
| Scattering cross section per molecule `sigma(lambda)` | public | `alwan_rayleigh_cross_section_{T}` |
| Rayleigh optical depth `tau_R(lambda)` | public | `alwan_rayleigh_optical_depth_{T}` |
| Spectral array of optical depths over a wavelength range | public | `alwan_rayleigh_spd_{T}` |

The refractive index, King correction factor (N2/O2 depolarisation), molecular density,
mean molecular weight, and latitude/altitude gravity term are computed inside the core
math and feed the cross section / optical depth. They are not exposed as standalone
public functions.

---

## Atmospheric Parameters

```c
typedef struct {
    alwan_{T} CO2_concentration;  /* CO2 in ppm (default: 300) */
    alwan_{T} temperature;        /* Temperature in Kelvin (default: 288.15) */
    alwan_{T} pressure;           /* Pressure in Pascals (default: 101325) */
    alwan_{T} latitude;           /* Latitude in degrees (default: 0) */
    alwan_{T} altitude;           /* Altitude in meters (default: 0) */
} alwan_atmosphere_params_{T};

void alwan_atmosphere_params_default_{T}(alwan_atmosphere_params_{T} *params);
```

---

## Functions

### alwan_rayleigh_cross_section_{T}

```c
alwan_{T} alwan_rayleigh_cross_section_{T}(alwan_{T} wavelength_nm,
                                           alwan_atmosphere_params_{T} const *params);
```

Rayleigh scattering cross section per molecule (sigma). Van de Hulst (1957) method with
Bodhaine et al. (1999) corrections. Pass `NULL` for `params` to use defaults (only CO2
and temperature are used).

**Returns:** Cross section in cm^2. This scalar-valued query has no `alwan_status`
channel and no error path: it always evaluates the Bodhaine formula on the inputs
(`NULL` params falls back to defaults), so supply physically valid arguments.

### alwan_rayleigh_optical_depth_{T}

```c
alwan_{T} alwan_rayleigh_optical_depth_{T}(alwan_{T} wavelength_nm,
                                           alwan_atmosphere_params_{T} const *params);
```

Rayleigh optical depth through the atmosphere, `tau_R(lambda)`. Pass `NULL` for `params` to use
defaults.

**Returns:** Optical depth (dimensionless). Like the cross-section query, this function
has no `alwan_status` channel and no error path; supply physically valid arguments.

### alwan_rayleigh_spd_{T}

```c
int alwan_rayleigh_spd_{T}(alwan_{T} wavelength_start, alwan_{T} wavelength_end,
                           alwan_{T} wavelength_step,
                           alwan_atmosphere_params_{T} const *params,
                           alwan_{T} *out, int *out_count);
```

Generate Rayleigh optical depth values across a wavelength range.

**Parameters:**
- `wavelength_start`, `wavelength_end`, `wavelength_step`: Wavelength range in nm
- `params`: Atmospheric parameters (`NULL` for defaults)
- `out`: Output array (must be large enough for `(end - start) / step + 1` values)
- `out_count`: Receives the number of values written

**Returns:** an `alwan_status` value: `ALWAN_OK` (`0`) on success, `ALWAN_E_INVALID`
(`-1`) if `out`/`out_count` is `NULL`, `ALWAN_E_RANGE` (`-3`) if `wavelength_step <= 0`
or `wavelength_start > wavelength_end`. See the error-contract note below.

**Example:**
```c
alwan_atmosphere_params_f64 params;
alwan_atmosphere_params_default_f64(&params);
params.altitude = 2000.0;  /* Mountain observatory */

/* Optical depth at 550nm (green) */
alwan_f64 tau = alwan_rayleigh_optical_depth_f64(550.0, &params);
printf("Optical depth at 550nm: %.4f\n", tau);

/* Full visible spectrum */
alwan_f64 depths[81];
int count;
alwan_rayleigh_spd_f64(380.0, 780.0, 5.0, &params, depths, &count);
```

---

## Error Contract

`int`-returning functions in alwan use the `alwan_status` enum (`ALWAN_OK = 0`,
`ALWAN_E_INVALID = -1`, `ALWAN_E_NODATA = -2`, `ALWAN_E_RANGE = -3`,
`ALWAN_E_NOMEM = -4`, `ALWAN_E_DIVZERO = -5`).

`alwan_rayleigh_spd_{T}` returns `alwan_status` values: `ALWAN_OK` on success,
`ALWAN_E_INVALID` for a `NULL` `out`/`out_count`, and `ALWAN_E_RANGE` for a
non-positive step or an inverted `start > end` range. (The header doc-comment's older
"returns 0 / -1" wording predates the enum but the implementation returns the named
status codes.)

The scalar-valued queries `alwan_rayleigh_cross_section_{T}` and
`alwan_rayleigh_optical_depth_{T}` return the physical quantity directly and have no
status channel and no error sentinel; they evaluate the formula unconditionally, so the
caller is responsible for passing physically valid inputs.

---

## See Also

- [Spectral Operations](spectral.md): SPD operations
- [Vision Science](vision.md): Human visual perception
