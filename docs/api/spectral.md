# Spectral Operations API

Functions for spectral power distribution calculations and spectral-to-tristimulus conversions.

---

## Overview

Spectral operations convert between spectral power distributions (SPDs) and tristimulus values (XYZ) using color matching functions (CMFs).

**Implemented:**
- `alwan_spd_create` / `alwan_spd_destroy` — SPD lifecycle management
- `alwan_spd_illuminant` — Get standard illuminant SPDs
- `alwan_spd_blackbody` — Generate blackbody radiator SPD
- `alwan_spd_resample` — Resample SPD to different wavelength range
- `alwan_xyz_from_spd` — Convert SPD to XYZ tristimulus values
- `alwan_rgb_to_spectrum_smits1999` — RGB to spectrum (Smits method)
- `alwan_rgb_to_spectrum_mallett2019` — RGB to spectrum (Mallett method)
- `alwan_rgb_to_spectrum_jakob2019` — RGB to spectrum (Jakob method)

**Not yet implemented:**
- `alwan_get_cmf` — Direct access to color matching function data

---

## Functions

### alwan_xyz_from_spd

```c
alwan_result alwan_xyz_from_spd(
    alwan_vec3 *xyz,              // Output first
    alwan_ctx *ctx,
    const alwan_scalar *spd,
    size_t wavelength_count,
    alwan_scalar wavelength_start,
    alwan_scalar wavelength_step,
    const char *cmf_type,
    const alwan_scalar *illuminant_spd
);
```

Convert spectral power distribution to XYZ tristimulus values.

---

### alwan_get_cmf

```c
alwan_result alwan_get_cmf(
    const alwan_scalar **x_bar,   // Output first
    const alwan_scalar **y_bar,
    const alwan_scalar **z_bar,
    size_t *count,
    alwan_ctx *ctx,
    const char *cmf_type
);
```

Get color matching function data.

**CMF types:**
- `"cie_1931_2deg"` — CIE 1931 2° Standard Observer
- `"cie_1964_10deg"` — CIE 1964 10° Standard Observer
- `"cie_2012_2deg"` — CIE 2012 2° (physiologically-based)
- `"cie_2015_2deg"` — CIE 2015 2° (final revision)
- `"stockman_sharpe_2deg"` — Stockman & Sharpe 2° cone fundamentals

---

### alwan_spd_illuminant

```c
alwan_result alwan_spd_illuminant(
    const alwan_scalar **spd,     // Output first
    size_t *count,
    alwan_scalar *wavelength_start,
    alwan_scalar *wavelength_step,
    alwan_ctx *ctx,
    const char *illuminant
);
```

Get standard illuminant spectral power distribution.

**Illuminants:**
- D-series: `"D50"`, `"D55"`, `"D60"`, `"D65"`, `"D75"`, `"D93"`
- Standard: `"A"`, `"B"`, `"C"`, `"E"`
- Fluorescent: `"F1"`...`"F12"`
- LED: `"LED-B1"`...`"LED-B5"`, `"LED-V1"`, `"LED-V2"`

---

## See Also

- [Color Spaces](color-spaces.md) — XYZ conversions
- [Data Management](../data-management.md) — CMF and illuminant data
