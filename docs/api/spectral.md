# Spectral Operations API

> **⚠️ PLANNED API:** This documentation describes planned functionality that is not yet implemented in v0.1-alpha. Refer to [alwan.h](../../src/alwan/alwan.h) for current capabilities.

Functions for spectral power distribution calculations and spectral-to-tristimulus conversions.

---

## Overview

Spectral operations convert between spectral power distributions (SPDs) and tristimulus values (XYZ) using color matching functions (CMFs).

**Planned features:**
- SPD integration to XYZ tristimulus values
- Support for multiple CMF sets (CIE 1931, 1964, 2012, 2015, Stockman & Sharpe)
- Spectral upsampling (RGB → approximate SPD)
- Illuminant SPD access and manipulation
- Metamerism calculations

---

## Planned Functions

### alwan_spd_to_xyz

```c
alwan_result alwan_spd_to_xyz(
    alwan_ctx *ctx,
    const alwan_scalar *spd,
    size_t wavelength_count,
    alwan_scalar wavelength_start,
    alwan_scalar wavelength_step,
    const char *cmf_type,
    const alwan_scalar *illuminant_spd,
    alwan_vec3 *xyz
);
```

Convert spectral power distribution to XYZ tristimulus values.

---

### alwan_get_cmf

```c
alwan_result alwan_get_cmf(
    alwan_ctx *ctx,
    const char *cmf_type,
    const alwan_scalar **x_bar,
    const alwan_scalar **y_bar,
    const alwan_scalar **z_bar,
    size_t *count
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

### alwan_get_illuminant_spd

```c
alwan_result alwan_get_illuminant_spd(
    alwan_ctx *ctx,
    const char *illuminant,
    const alwan_scalar **spd,
    size_t *count,
    alwan_scalar *wavelength_start,
    alwan_scalar *wavelength_step
);
```

Get standard illuminant spectral power distribution.

**Illuminants:**
- D-series: `"D50"`, `"D55"`, `"D60"`, `"D65"`, `"D75"`, `"D93"`
- Standard: `"A"`, `"B"`, `"C"`, `"E"`
- Fluorescent: `"F1"`...`"F12"`
- LED: `"LED-B1"`...`"LED-B5"`, `"LED-V1"`, `"LED-V2"`

---

## Implementation Status

**Current:** Not yet implemented
**Planned for:** v0.2

---

## See Also

- [Color Spaces](color-spaces.md) — XYZ conversions
- [Data Management](../data-management.md) — CMF and illuminant data
