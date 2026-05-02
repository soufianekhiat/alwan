# Reference Data & Color Systems API

Functions for accessing standard color reference datasets and color notation systems.

---

## Overview

Reference data functions provide access to:

- **Munsell Renotation Data** — Munsell HVC to/from XYZ
- **Color Checker** — Standard color target patch data
- **NCS** — Natural Color System notation
- **RGB Space Introspection** — Query primaries and transfer functions by enum
- **Illuminant Utilities** — White points and D-series generation

---

## Munsell Color System

### alwan_munsell_to_xyz

```c
int alwan_munsell_to_xyz(alwan_xyz *xyz,
                         alwan_scalar hue, alwan_scalar value,
                         alwan_scalar chroma,
                         alwan_illuminant illuminant);
```

Convert Munsell notation (Hue, Value, Chroma) to XYZ using the Munsell Renotation Data (1943).

**Parameters:**
- `hue` — Munsell hue [0, 100] (continuous: 0=R, 10=YR, 20=Y, ..., 90=RP)
- `value` — Munsell value [0, 10] (lightness)
- `chroma` — Munsell chroma [0, 20+] (saturation)
- `illuminant` — Illuminant for XYZ calculation

### alwan_xyz_to_munsell

```c
int alwan_xyz_to_munsell(alwan_scalar *hue, alwan_scalar *value,
                         alwan_scalar *chroma,
                         alwan_xyz const *xyz,
                         alwan_illuminant illuminant);
```

Convert XYZ to Munsell notation (inverse lookup).

---

## Color Checker Targets

### alwan_color_checker_data

```c
int alwan_color_checker_data(alwan_xyz *xyz,
                             alwan_colorchecker_type type,
                             alwan_illuminant illuminant,
                             size_t patch_index);
```

Get XYZ tristimulus values for a specific Color Checker patch.

### alwan_color_checker_num_patches

```c
size_t alwan_color_checker_num_patches(alwan_colorchecker_type type);
```

Get the number of patches in a Color Checker target.

**Target types:**
```c
typedef enum {
    ALWAN_COLORCHECKER_CLASSIC = 0,      /* 24-patch (most common) */
    ALWAN_COLORCHECKER_SG,                /* 140-patch */
    ALWAN_COLORCHECKER_DIGITAL_SG,        /* Digital SG */
    ALWAN_BABELCOLOR_AVERAGE,             /* BabelColor Average */
    ALWAN_BABELCOLOR_HCT                  /* BabelColor HCT */
} alwan_colorchecker_type;
```

**Example:**
```c
/* Iterate all patches of a ColorChecker Classic */
size_t n = alwan_color_checker_num_patches(ALWAN_COLORCHECKER_CLASSIC);
for (size_t i = 0; i < n; i++) {
    alwan_xyz xyz;
    alwan_color_checker_data(&xyz, ALWAN_COLORCHECKER_CLASSIC,
                             ALWAN_ILLUMINANT_D65, i);
    printf("Patch %zu: X=%.3f Y=%.3f Z=%.3f\n", i, xyz.x, xyz.y, xyz.z);
}
```

---

## Natural Color System (NCS)

### alwan_ncs_to_xyz

```c
int alwan_ncs_to_xyz(alwan_xyz *xyz, char const *ncs_notation);
```

Convert NCS notation string to XYZ. Example notation: `"S 1050-Y90R"`.

### alwan_xyz_to_ncs

```c
int alwan_xyz_to_ncs(char *ncs_notation, size_t notation_size,
                     alwan_xyz const *xyz);
```

Convert XYZ to NCS notation. Buffer `notation_size` should be >= 32.

---

## RGB Space Introspection

### alwan_rgb_space_by_enum

```c
int alwan_rgb_space_by_enum(alwan_scalar primaries[6],
                            alwan_vec2 *white_point,
                            alwan_rgb_space space);
```

Get RGB primaries (rx, ry, gx, gy, bx, by) and white point xy by enum. Does not require a context.

### alwan_rgb_space_get_tfs

```c
int alwan_rgb_space_get_tfs(alwan_transfer_function *oetf,
                            alwan_transfer_function *eotf,
                            alwan_rgb_space space);
```

Get the OETF and EOTF associated with an RGB color space enum.

**Example:**
```c
alwan_scalar primaries[6];
alwan_vec2 white;
alwan_transfer_function oetf, eotf;

alwan_rgb_space_by_enum(primaries, &white, ALWAN_RGB_SPACE_DISPLAY_P3);
alwan_rgb_space_get_tfs(&oetf, &eotf, ALWAN_RGB_SPACE_DISPLAY_P3);

printf("White: (%.4f, %.4f)\n", white.x, white.y);
printf("OETF: %d, EOTF: %d\n", oetf, eotf);
```

---

## Illuminant White Points

### alwan_illuminant_white_point

```c
int alwan_illuminant_white_point(alwan_xyz *out_xyz,
                                 alwan_illuminant illuminant,
                                 alwan_observer_type observer);
```

Get the XYZ white point for a standard illuminant, normalized to Y=1.0.

**Example:**
```c
alwan_xyz d65_white;
alwan_illuminant_white_point(&d65_white, ALWAN_ILLUMINANT_D65,
                             ALWAN_OBSERVER_CIE_1931_2DEG);
/* d65_white ~= {0.9505, 1.0000, 1.0890} */
```

---

## Interpolation & Table Lookup Utilities

### alwan_interpolate

```c
int alwan_interpolate(alwan_scalar const *x_in, alwan_scalar const *y_in,
                      size_t count_in,
                      alwan_scalar const *x_out, alwan_scalar *y_out,
                      size_t count_out,
                      alwan_interp_method method);
```

Interpolate data points using specified method.

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

### alwan_extrapolate

```c
int alwan_extrapolate(alwan_scalar const *x_in, alwan_scalar const *y_in,
                      size_t count_in,
                      alwan_scalar const *x_out, alwan_scalar *y_out,
                      size_t count_out,
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

### 3D LUT Interpolation

```c
/* Trilinear interpolation */
int alwan_table_interp_3d_trilinear(alwan_rgb *rgb_out,
                                    alwan_scalar const *table,
                                    size_t const sizes[3],
                                    alwan_rgb const *rgb_in);

/* Tetrahedral interpolation (more accurate for color transforms) */
int alwan_table_interp_3d_tetrahedral(alwan_rgb *rgb_out,
                                      alwan_scalar const *table,
                                      size_t const sizes[3],
                                      alwan_rgb const *rgb_in);

/* 1D table interpolation */
alwan_scalar alwan_table_interp_1d(alwan_scalar const *table, size_t size,
                                   alwan_scalar x, alwan_interp_method method);
```

**Parameters for 3D LUT:**
- `table` — 3D LUT array (R-major: table[r][g][b], 3 values per entry)
- `sizes` — Dimensions [size_r, size_g, size_b]
- `rgb_in` — Input RGB coordinates [0, 1] (normalized)

---

## Tristimulus Optimization

### alwan_optimize_spectrum_for_xyz

```c
int alwan_optimize_spectrum_for_xyz(alwan_spd *spd_out,
                                    alwan_ctx *ctx,
                                    alwan_xyz const *target_xyz,
                                    alwan_observer_type observer);
```

Find a spectral power distribution that matches target XYZ tristimulus values. Due to metamerism, multiple SPDs can match the same XYZ; this finds one valid solution.

---

## Hero Wavelength Spectral Sampling

### alwan_hero_wavelength_sample

```c
int alwan_hero_wavelength_sample(alwan_scalar *lambda_out, alwan_scalar u);
```

Sample a hero wavelength from uniform random variable `u` in [0,1]. Used for spectral rendering with hero wavelength sampling.

### alwan_hero_wavelength_to_xyz

```c
int alwan_hero_wavelength_to_xyz(alwan_xyz *xyz_out, alwan_scalar lambda);
```

Convert a hero wavelength to XYZ using the CIE 1931 color matching functions.

### alwan_hero_wavelength_batch

```c
int alwan_hero_wavelength_batch(alwan_scalar *lambda_out,
                                alwan_scalar const *u_in,
                                size_t count, size_t in_stride,
                                size_t out_stride);
```

Batch version of hero wavelength sampling.

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — Invalid parameter or notation
- `ALWAN_E_NODATA` (-2) — Reference data not found

---

## See Also

- [Spectral Operations](spectral.md) — SPD creation and XYZ integration
- [Color Correction](color-correction.md) — Camera profiling with ColorChecker
- [Color Spaces](color-spaces.md) — RGB space conversions
