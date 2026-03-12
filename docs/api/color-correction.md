# Color Correction & Grading API

Functions for color grading, white balance, camera profiling, and creative color transforms.

---

## Overview

Alwan provides tools for both technical color correction and creative color grading:

- **Lift/Gamma/Gain (LGG)** — Industry-standard 3-way color correction
- **Color Matrix** — 3x3 matrix transforms with creative presets
- **Printer Lights** — Film-style exposure control
- **White Balance** — Neutral gray calibration
- **Camera Profiling** — Polynomial color correction (Cheung 2004, Finlayson 2015)

---

## Lift/Gamma/Gain

### alwan_lgg_apply

```c
int alwan_lgg_apply(alwan_rgb *rgb_out, alwan_rgb const *rgb_in,
                    alwan_rgb const *lift, alwan_rgb const *gamma,
                    alwan_rgb const *gain);
```

Apply lift/gamma/gain color correction. Formula: `out = ((in + lift) ^ (1/gamma)) * gain`

**Parameters:**
- `rgb_in` — Input RGB values (linear, [0,1] for normal range)
- `lift` — Lift adjustment per channel (shadows). Typical range: [-1, 1]
- `gamma` — Gamma adjustment per channel (midtones). Typical range: [0.0001, 10]
- `gain` — Gain adjustment per channel (highlights). Typical range: [0, 2]

**Example:**
```c
/* Warm shadows, cool highlights */
alwan_rgb lift  = {0.05, 0.02, -0.02};  /* Push shadows warm */
alwan_rgb gamma = {1.0, 1.0, 1.0};      /* Neutral midtones */
alwan_rgb gain  = {0.95, 1.0, 1.05};    /* Cool highlights */

alwan_rgb result;
alwan_lgg_apply(&result, &pixel, &lift, &gamma, &gain);
```

Batch variants: `alwan_lgg_apply_map_interleave`, `_map_interleave_ex`, `_map_planar`, `_map_planar_ex`

---

## Color Matrix

### alwan_color_matrix_apply

```c
int alwan_color_matrix_apply(alwan_rgb *rgb_out, alwan_rgb const *rgb_in,
                             alwan_mat3x3 const *matrix_3x3);
```

Apply a 3x3 color transformation matrix to RGB values.

### alwan_color_matrix_get_preset

```c
int alwan_color_matrix_get_preset(alwan_mat3x3 *matrix_3x3,
                                  alwan_color_matrix_preset preset);
```

Get a preset color grading matrix.

**Preset Enum:**
```c
typedef enum {
    ALWAN_COLOR_MATRIX_SEPIA,           /* Sepia tone effect */
    ALWAN_COLOR_MATRIX_VINTAGE,         /* Vintage look */
    ALWAN_COLOR_MATRIX_BLEACH_BYPASS,   /* Bleach bypass effect */
    ALWAN_COLOR_MATRIX_COOL,            /* Cool tone shift */
    ALWAN_COLOR_MATRIX_WARM,            /* Warm tone shift */
    ALWAN_COLOR_MATRIX_MONOCHROME,      /* Black and white */
    ALWAN_COLOR_MATRIX_NIGHT_VISION     /* Night vision look */
} alwan_color_matrix_preset;
```

**Example:**
```c
/* Apply sepia tone to an image */
alwan_mat3x3 sepia;
alwan_color_matrix_get_preset(&sepia, ALWAN_COLOR_MATRIX_SEPIA);

alwan_color_matrix_apply_map_interleave(
    (alwan_scalar*)out, (alwan_scalar const*)in,
    &sepia, width * height,
    3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
```

Batch variants: `alwan_color_matrix_apply_map_interleave`, `_map_interleave_ex`, `_map_planar`, `_map_planar_ex`

---

## Printer Lights

### alwan_printer_lights_apply

```c
int alwan_printer_lights_apply(alwan_rgb *rgb_out, alwan_rgb const *rgb_in,
                               alwan_scalar red_lights, alwan_scalar green_lights,
                               alwan_scalar blue_lights);
```

Film-style printer light color correction. Each light unit represents approximately 0.025 log exposure change.

**Parameters:**
- `red_lights`, `green_lights`, `blue_lights` — Printer light values (0-50, default 25 = neutral)

**Example:**
```c
/* Push warm: add red, reduce blue */
alwan_printer_lights_apply(&result, &pixel, 27.0, 25.0, 23.0);
```

Batch variants: `alwan_printer_lights_apply_map_interleave`, `_map_interleave_ex`, `_map_planar`, `_map_planar_ex`

---

## White Balance

### alwan_white_balance_from_gray

```c
int alwan_white_balance_from_gray(alwan_rgb *multipliers_out,
                                  alwan_rgb const *measured_gray);
```

Compute white balance multipliers from a measured neutral gray sample. The multipliers are normalized so the minimum channel equals 1.0.

### alwan_white_balance_apply

```c
int alwan_white_balance_apply(alwan_rgb *rgb_out, alwan_rgb const *rgb,
                              alwan_rgb const *multipliers);
```

Apply white balance multipliers to an RGB color.

**Example:**
```c
/* Measure a gray card under the scene illuminant */
alwan_rgb measured_gray = {0.42, 0.50, 0.65};  /* Bluish gray */

alwan_rgb multipliers;
alwan_white_balance_from_gray(&multipliers, &measured_gray);

/* Apply to all pixels in the image */
alwan_white_balance_apply_map_interleave(
    (alwan_scalar*)out, (alwan_scalar const*)in,
    &multipliers, width * height,
    3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
```

Batch variants: `alwan_white_balance_apply_map_interleave`, `_map_interleave_ex`, `_map_planar`, `_map_planar_ex`

---

## Camera Profiling (Polynomial Color Correction)

### Cheung 2004 Method

Polynomial expansion for camera-to-reference color correction.

```c
/* Polynomial expansion terms */
typedef enum {
    ALWAN_POLY_CHEUNG_3  = 3,   /* [R, G, B] */
    ALWAN_POLY_CHEUNG_4  = 4,   /* [R, G, B, 1] */
    ALWAN_POLY_CHEUNG_7  = 7,   /* [R, G, B, RG, RB, GB, 1] */
    ALWAN_POLY_CHEUNG_11 = 11,  /* [R, G, B, RG, RB, GB, R2, G2, B2, RGB, 1] */
    ALWAN_POLY_CHEUNG_35 = 35   /* Full 35-term expansion (maximum) */
    /* ... and more: 5, 8, 10, 14, 16, 17, 19, 20, 22 */
} alwan_poly_cheung_terms;
```

#### alwan_colour_correction_matrix_cheung2004

```c
int alwan_colour_correction_matrix_cheung2004(
    alwan_scalar *matrix_out,        /* Output correction matrix (terms x 3) */
    alwan_scalar const *M_T,         /* Test (camera) RGB, Nx3 row-major */
    alwan_scalar const *M_R,         /* Reference RGB, Nx3 row-major */
    int num_samples,                 /* Number of color samples (N) */
    alwan_poly_cheung_terms terms);
```

#### alwan_colour_correct_cheung2004

```c
int alwan_colour_correct_cheung2004(
    alwan_rgb *rgb_out, alwan_rgb const *rgb,
    alwan_scalar const *matrix,
    alwan_poly_cheung_terms terms);
```

### Finlayson 2015 Method

Root-polynomial color correction with improved accuracy.

```c
int alwan_colour_correction_matrix_finlayson2015(
    alwan_scalar *matrix_out, int *matrix_size,
    alwan_scalar const *M_T, alwan_scalar const *M_R,
    int num_samples, int degree, int root_poly);

int alwan_colour_correct_finlayson2015(
    alwan_rgb *rgb_out, alwan_rgb const *rgb,
    alwan_scalar const *matrix, int degree, int root_poly);
```

**Example (camera profiling workflow):**
```c
/* 1. Photograph a ColorChecker under scene lighting */
/* 2. Extract camera RGB for each patch */
alwan_scalar camera_rgb[24 * 3];   /* 24 patches, camera-measured */
alwan_scalar reference_rgb[24 * 3]; /* 24 patches, known reference */

/* 3. Compute correction matrix */
alwan_scalar matrix[11 * 3];
alwan_colour_correction_matrix_cheung2004(
    matrix, camera_rgb, reference_rgb, 24, ALWAN_POLY_CHEUNG_11);

/* 4. Apply to all image pixels */
alwan_rgb corrected;
alwan_colour_correct_cheung2004(&corrected, &camera_pixel, matrix,
                                ALWAN_POLY_CHEUNG_11);
```

### Polynomial Expansion Utilities

```c
/* Expand RGB to polynomial terms (Cheung) */
int alwan_poly_expand_cheung2004(alwan_scalar *out, alwan_rgb const *rgb,
                                 alwan_poly_cheung_terms terms);

/* Expand RGB to polynomial terms (Finlayson) */
int alwan_poly_expand_finlayson2015(alwan_scalar *out, int *out_size,
                                    alwan_rgb const *rgb, int degree,
                                    int root_poly);

/* Generic Vandermonde expansion */
int alwan_poly_expand_vandermonde(alwan_scalar *out, int *out_size,
                                  alwan_scalar const *a, int a_size,
                                  int degree);
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — NULL pointer or invalid parameter
- `ALWAN_E_DIVZERO` (-5) — Division by zero (e.g., measured_gray with zero channel)

---

## See Also

- [Transfer Functions](transfer-functions.md) — OETF/EOTF encoding
- [ACES Pipeline](aces.md) — ACES LMT grading
- [Color Spaces](color-spaces.md) — RGB conversions
