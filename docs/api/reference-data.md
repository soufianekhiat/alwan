# Reference Data & Color Systems API

Functions for accessing standard color reference datasets and color notation systems.

---

## Overview

Reference data functions provide access to:

- **Munsell Renotation Data** — Munsell HVC to/from XYZ
- **Color Checker** — Standard color target patch data
- **NCS** — Natural Color System notation (approximate, forward only)
- **RGB Space Introspection** — Query primaries and transfer functions by enum
- **Illuminant Utilities** — White points, xy chromaticities, and D-series generation
- **Embedded Dataset Getters** — Raw illuminant-xy and sRGB-primaries arrays
- **Interpolation & LUTs** — 1D/3D table lookup and (extra)interpolation helpers

All numeric reference data is **CSV-embedded** at compile time (`ALWAN_EMBED_DATA=1`,
the only supported mode). The datasets live under `src/alwan/data/**` and are baked
into static C arrays by `src/alwan/api/alwan_data.c` (illuminants, primaries, matrices)
and `src/alwan/api/alwan_reference_data.c` (Munsell, NCS, ColorChecker). There is no
runtime/`/data/` loader.

---

## Precision Variants

Every function on this page that carries color data exists in two explicit precision
variants. `{T}` below is a placeholder for one of:

| `{T}` | Scalar type   | Color types                                      | Gate              |
|-------|---------------|--------------------------------------------------|-------------------|
| `f32` | `alwan_f32`   | `alwan_xyz_f32`, `alwan_vec2_f32`, `alwan_rgb_f32`, `alwan_spd_f32` | `ALWAN_WITH_F32` |
| `f64` | `alwan_f64`   | `alwan_xyz_f64`, `alwan_vec2_f64`, `alwan_rgb_f64`, `alwan_spd_f64` | `ALWAN_WITH_F64` |

By default **both** precisions compile. Declarations are always present.

> **f64-internal facades (by design).** The reference-data lookups on this page —
> Munsell (`alwan_munsell_to_xyz` / `alwan_xyz_to_munsell`), NCS
> (`alwan_ncs_to_xyz`), ColorChecker (`alwan_color_checker_data`), and RGB-space
> introspection (`alwan_rgb_space_by_enum` / `alwan_rgb_space_get_tfs`) — store
> their renotation/patch/primaries tables in `double`. The `_f32` twins are **not**
> independent native-f32 paths: they run the f64 data + f64 lookup and narrow the
> result at the boundary. There is **no** `#if` precision gating in
> `alwan_reference_data.c`; the f32 accessors call straight through to the f64
> implementation. This is intentional — the table data is f64 and re-quantising it
> to f32 tables would only add error — and matches the f64-internal facade pattern
> already documented for ZCAM and the CCM fits (see
> [precision-and-limits.md](../precision-and-limits.md)). A consequence is that
> these `_f32` entry points stay available even in an `ALWAN_BUILD_ONLY_F32` build
> (gated by `ALWAN_WITH_F64_FACADE`, always `1`), rather than failing at link time.

For the colour-data accessors **not** in that list, a single-precision build
(`ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64`) defines only the matching twin
and calling the excluded precision fails at **link** time.

A few helpers are precision-independent and have **no** `_{T}` suffix
(e.g. `alwan_color_checker_num_patches`).

Examples below use `_f64`; replace with `_f32` for single precision.

---

## Munsell Color System

### alwan_munsell_to_xyz_{T}

```c
int alwan_munsell_to_xyz_f64(alwan_xyz_f64 *xyz,
                             alwan_f64 hue, alwan_f64 value, alwan_f64 chroma,
                             alwan_illuminant illuminant);
int alwan_munsell_to_xyz_f32(alwan_xyz_f32 *xyz,
                             alwan_f32 hue, alwan_f32 value, alwan_f32 chroma,
                             alwan_illuminant illuminant);
```

Convert Munsell notation (Hue, Value, Chroma) to XYZ using the Munsell Renotation Data (1943).

**Parameters:**
- `hue` — Munsell hue [0, 100] (continuous: 0=R, 10=YR, 20=Y, ..., 90=RP)
- `value` — Munsell value [0, 10] (lightness)
- `chroma` — Munsell chroma [0, 20+] (saturation)
- `illuminant` — Illuminant for XYZ calculation

### alwan_xyz_to_munsell_{T}

```c
int alwan_xyz_to_munsell_f64(alwan_f64 *hue, alwan_f64 *value, alwan_f64 *chroma,
                             alwan_xyz_f64 const *xyz, alwan_illuminant illuminant);
int alwan_xyz_to_munsell_f32(alwan_f32 *hue, alwan_f32 *value, alwan_f32 *chroma,
                             alwan_xyz_f32 const *xyz, alwan_illuminant illuminant);
```

Convert XYZ to Munsell notation (inverse lookup).

---

## Color Checker Targets

### alwan_color_checker_data_{T}

```c
int alwan_color_checker_data_f64(alwan_xyz_f64 *xyz,
                                 alwan_colorchecker_type type,
                                 alwan_illuminant illuminant,
                                 size_t patch_index);
int alwan_color_checker_data_f32(alwan_xyz_f32 *xyz,
                                 alwan_colorchecker_type type,
                                 alwan_illuminant illuminant,
                                 size_t patch_index);
```

Get XYZ tristimulus values for a specific Color Checker patch.

### alwan_color_checker_num_patches

```c
size_t alwan_color_checker_num_patches(alwan_colorchecker_type type);
```

Get the number of patches in a Color Checker target. Precision-independent
(no `_{T}` suffix). Returns 0 on error.

**Target types:**
```c
typedef enum {
    ALWAN_COLORCHECKER_CLASSIC = 0,      /* ColorChecker Classic 24-patch */
    ALWAN_COLORCHECKER_SG,                /* ColorChecker SG 140-patch */
    ALWAN_COLORCHECKER_DIGITAL_SG,        /* ColorChecker Digital SG */
    ALWAN_BABELCOLOR_AVERAGE,             /* BabelColor Average */
    ALWAN_BABELCOLOR_HCT                  /* BabelColor HCT */
} alwan_colorchecker_type;
```

**Example:**
```c
/* Iterate all patches of a ColorChecker Classic */
size_t n = alwan_color_checker_num_patches(ALWAN_COLORCHECKER_CLASSIC);
for (size_t i = 0; i < n; i++) {
    alwan_xyz_f64 xyz;
    alwan_color_checker_data_f64(&xyz, ALWAN_COLORCHECKER_CLASSIC,
                                 ALWAN_ILLUMINANT_D65, i);
    printf("Patch %zu: X=%.3f Y=%.3f Z=%.3f\n", i, xyz.x, xyz.y, xyz.z);
}
```

---

## Natural Color System (NCS)

### alwan_ncs_to_xyz_{T}

```c
int alwan_ncs_to_xyz_f64(alwan_xyz_f64 *xyz, char const *ncs_notation);
int alwan_ncs_to_xyz_f32(alwan_xyz_f32 *xyz, char const *ncs_notation);
```

Convert an NCS notation string to XYZ. Example notation: `"S 1050-Y90R"`.
Output XYZ is on the Y = 0–100 scale, D65.

> **Approximate.** Uses published elementary-hue chromaticities (Hård & Sivik 1981)
> with linear hue interpolation; it does **not** reproduce the proprietary NCS atlas.

### alwan_xyz_to_ncs_{T}

```c
int alwan_xyz_to_ncs_f64(char *ncs_notation, size_t notation_size,
                         alwan_xyz_f64 const *xyz);
int alwan_xyz_to_ncs_f32(char *ncs_notation, size_t notation_size,
                         alwan_xyz_f32 const *xyz);
```

> **Inverse unsupported.** These always return `ALWAN_E_INVALID` — recovering NCS
> notation requires the proprietary NCS colour atlas. Reserve `notation_size >= 32`
> for any future support.

---

## RGB Space Introspection

### alwan_rgb_space_by_enum_{T}

```c
int alwan_rgb_space_by_enum_f64(alwan_f64 primaries[6],
                                alwan_vec2_f64 *white_point,
                                alwan_rgb_space space);
int alwan_rgb_space_by_enum_f32(alwan_f32 primaries[6],
                                alwan_vec2_f32 *white_point,
                                alwan_rgb_space space);
```

Get RGB primaries (rx, ry, gx, gy, bx, by) and white-point xy by enum.
Does not require a context. Returns `ALWAN_E_INVALID` if `space` is invalid.

### alwan_rgb_space_get_tfs_{T}

```c
int alwan_rgb_space_get_tfs_f64(alwan_transfer_function *oetf,
                                alwan_transfer_function *eotf,
                                alwan_rgb_space space);
int alwan_rgb_space_get_tfs_f32(alwan_transfer_function *oetf,
                                alwan_transfer_function *eotf,
                                alwan_rgb_space space);
```

Get the OETF and EOTF associated with an RGB color space enum. The `oetf`/`eotf`
outputs are precision-independent enums; the `_{T}` suffix matches the calling
convention only.

**Example:**
```c
alwan_f64 primaries[6];
alwan_vec2_f64 white;
alwan_transfer_function oetf, eotf;

alwan_rgb_space_by_enum_f64(primaries, &white, ALWAN_RGB_SPACE_DISPLAY_P3);
alwan_rgb_space_get_tfs_f64(&oetf, &eotf, ALWAN_RGB_SPACE_DISPLAY_P3);

printf("White: (%.4f, %.4f)\n", white.x, white.y);
printf("OETF: %d, EOTF: %d\n", oetf, eotf);
```

---

## Illuminant White Points

### alwan_illuminant_white_point_{T}

```c
int alwan_illuminant_white_point_f64(alwan_xyz_f64 *out_xyz,
                                     alwan_illuminant illuminant,
                                     alwan_observer_type observer);
int alwan_illuminant_white_point_f32(alwan_xyz_f32 *out_xyz,
                                     alwan_illuminant illuminant,
                                     alwan_observer_type observer);
```

Get the XYZ white point for a standard illuminant, normalized to Y = 1.0.
Returns `ALWAN_E_INVALID` if the illuminant is not supported.

**Example:**
```c
alwan_xyz_f64 d65_white;
alwan_illuminant_white_point_f64(&d65_white, ALWAN_ILLUMINANT_D65,
                                 ALWAN_OBSERVER_CIE_1931_2DEG);
/* d65_white ~= {0.9505, 1.0000, 1.0890} */
```

---

## Embedded Dataset Getters

These return a pointer into the **embedded** static dataset plus its element `count`.
In embedded mode (the only supported mode) the data is owned by the library — do not
free it. (`alwan_data_free_{T}` is declared only for the unimplemented runtime mode,
reserved for a future release.)

### alwan_data_get_illuminant_xy_{T}

```c
int alwan_data_get_illuminant_xy_f64(alwan_f64 **data, size_t *count,
                                     alwan_illuminant illuminant, alwan_ctx *ctx);
int alwan_data_get_illuminant_xy_f32(alwan_f32 **data, size_t *count,
                                     alwan_illuminant illuminant, alwan_ctx *ctx);
```

Enum-based illuminant xy chromaticity accessor. Returns 2 values (x, y).
Returns `ALWAN_E_INVALID` if the illuminant is not supported or has no xy data.

### alwan_data_get_srgb_primaries_{T}

```c
int alwan_data_get_srgb_primaries_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
int alwan_data_get_srgb_primaries_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);
```

Get the sRGB primaries as 6 values: rx, ry, gx, gy, bx, by.

---

## Interpolation & Table Lookup Utilities

### alwan_interpolate_{T}

```c
int alwan_interpolate_f64(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                          alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                          alwan_interp_method method);
int alwan_interpolate_f32(alwan_f32 const *x_in, alwan_f32 const *y_in, size_t count_in,
                          alwan_f32 const *x_out, alwan_f32 *y_out, size_t count_out,
                          alwan_interp_method method);
```

Interpolate data points using the specified method (`x_in` must be sorted ascending).

**Methods:**
```c
typedef enum {
    ALWAN_INTERP_LINEAR = 0,     /* Linear */
    ALWAN_INTERP_CUBIC,           /* Cubic */
    ALWAN_INTERP_LANCZOS,         /* Lanczos windowed sinc */
    ALWAN_INTERP_SPRAGUE,         /* Sprague 5th order */
    ALWAN_INTERP_LAGRANGE,        /* Lagrange polynomial */
    ALWAN_INTERP_AKIMA            /* Akima spline (non-overshooting) */
} alwan_interp_method;
```

### alwan_extrapolate_{T}

```c
int alwan_extrapolate_f64(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                          alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                          alwan_extrap_method method);
int alwan_extrapolate_f32(alwan_f32 const *x_in, alwan_f32 const *y_in, size_t count_in,
                          alwan_f32 const *x_out, alwan_f32 *y_out, size_t count_out,
                          alwan_extrap_method method);
```

**Methods:**
```c
typedef enum {
    ALWAN_EXTRAP_CONSTANT = 0,    /* Use boundary value */
    ALWAN_EXTRAP_LINEAR,          /* Linear extrapolation */
    ALWAN_EXTRAP_POLYNOMIAL,      /* Polynomial extrapolation */
    ALWAN_EXTRAP_EXPONENTIAL,     /* Exponential decay */
    ALWAN_EXTRAP_REFLECT,         /* Reflective boundary */
    ALWAN_EXTRAP_NATURAL          /* Natural neighbor */
} alwan_extrap_method;
```

### Table Interpolation

```c
/* 1D table interpolation (returns the interpolated value) */
alwan_f64 alwan_table_interp_1d_f64(alwan_f64 const *table, size_t size,
                                    alwan_f64 x, alwan_interp_method method);

/* 3D trilinear interpolation */
int alwan_table_interp_3d_trilinear_f64(alwan_rgb_f64 *rgb_out,
                                        alwan_f64 const *table, size_t const sizes[3],
                                        alwan_rgb_f64 const *rgb_in);

/* 3D tetrahedral interpolation (more accurate for color transforms) */
int alwan_table_interp_3d_tetrahedral_f64(alwan_rgb_f64 *rgb_out,
                                          alwan_f64 const *table, size_t const sizes[3],
                                          alwan_rgb_f64 const *rgb_in);
```

(`_f32` twins exist for all three.)

**Parameters for 3D LUT:**
- `table` — 3D LUT array (R-major: `table[r][g][b]`, 3 values per entry)
- `sizes` — Dimensions [size_r, size_g, size_b]
- `rgb_in` — Input RGB coordinates [0, 1] (normalized)

---

## Tristimulus Optimization

### alwan_optimize_spectrum_for_xyz_{T}

```c
int alwan_optimize_spectrum_for_xyz_f64(alwan_spd_f64 *spd_out,
                                        alwan_xyz_f64 const *target_xyz,
                                        alwan_observer_type observer,
                                        alwan_ctx *ctx);
int alwan_optimize_spectrum_for_xyz_f32(alwan_spd_f32 *spd_out,
                                        alwan_xyz_f32 const *target_xyz,
                                        alwan_observer_type observer,
                                        alwan_ctx *ctx);
```

Find a spectral power distribution that matches target XYZ tristimulus values
(`spd_out` must be pre-allocated with the desired wavelength range; `ctx` is last).
Due to metamerism, multiple SPDs can match the same XYZ; this finds one valid solution.

---

## Hero Wavelength Spectral Sampling

### alwan_hero_wavelength_sample_{T}

```c
int alwan_hero_wavelength_sample_f64(alwan_f64 *lambda_out, alwan_f64 u);
int alwan_hero_wavelength_sample_f32(alwan_f32 *lambda_out, alwan_f32 u);
```

Sample a hero wavelength from uniform variable `u` in [0,1], mapped to [380, 780] nm.

### alwan_hero_wavelength_to_xyz_{T}

```c
void alwan_hero_wavelength_to_xyz_f64(alwan_xyz_f64 *xyz_out, alwan_f64 lambda);
void alwan_hero_wavelength_to_xyz_f32(alwan_xyz_f32 *xyz_out, alwan_f32 lambda);
```

Convert a single wavelength to XYZ via the Wyman 2013 analytic CMF fit. Returns `void`.

### alwan_hero_wavelength_batch_{T}

```c
int alwan_hero_wavelength_batch_f64(alwan_f64 *lambda_out, alwan_xyz_f64 *xyz_weights,
                                    size_t count, alwan_f64 seed);
int alwan_hero_wavelength_batch_f32(alwan_f32 *lambda_out, alwan_xyz_f32 *xyz_weights,
                                    size_t count, alwan_f32 seed);
```

Stratified batch sampling: generates `count` wavelengths from `seed`. `xyz_weights`
receives per-sample XYZ importance weights and may be `NULL`.

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — Invalid parameter or notation (also the NCS inverse contract)
- `ALWAN_E_NODATA` (-2) — Reference data not found

---

## See Also

- [Spectral Operations](spectral.md) — SPD creation and XYZ integration
- [Color Correction](color-correction.md) — Camera profiling with ColorChecker
- [Color Spaces](color-spaces.md) — RGB space conversions
