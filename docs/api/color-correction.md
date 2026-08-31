# Color Correction & Grading API

Functions for color grading, white balance, camera profiling, and creative color transforms.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float` / `alwan_f32` / `alwan_rgb_f32`) and
> `name_f64` (double precision, `double` / `alwan_f64` / `alwan_rgb_f64`).
> `T = f32 | f64`. Which precisions compile is controlled by the build config
> (see [Configuration](../configuration.md)). The `_map_interleave_ex`
> / `_map_planar_ex` typed dispatchers are single, precision-agnostic entry points
> (`void*` buffers + `alwan_pixel_format`), so their scalar knobs are always the
> f64 value types.

---

## Overview

Alwan provides tools for both technical color correction and creative color grading:

| Tool | Purpose |
|------|---------|
| **Lift/Gamma/Gain (LGG)** | Industry-standard 3-way color correction |
| **ASC CDL / SOP** | Slope-Offset-Power + Saturation (ASC Color Decision List) |
| **Color Matrix** | 3x3 matrix transforms with creative presets |
| **Exposure / Printer Lights** | Log-unit exposure and film-style printer lights |
| **White Balance** | Neutral-gray calibration |
| **Camera Profiling** | Polynomial color correction (Cheung 2004, Finlayson 2015) |

All batch functions follow the canonical v2.0 argument convention: outputs before
inputs, each `*_stride` (in bytes) **immediately after its buffer**, then `count`,
then the trailing knob/enum/`ctx` arguments.

---

## Lift/Gamma/Gain

### alwan_lgg_apply_{T}

```c
void alwan_lgg_apply_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in,
                         alwan_rgb_{T} const *lift, alwan_rgb_{T} const *gamma,
                         alwan_rgb_{T} const *gain);
```

Apply lift/gamma/gain color correction. Formula: `out = ((in + lift) ^ (1/gamma)) * gain`

**Parameters:**
- `rgb_in`: Input RGB values (linear, [0,1] for normal range)
- `lift`: Lift adjustment per channel (shadows). Typical range: [-1, 1]
- `gamma`: Gamma adjustment per channel (midtones). Typical range: [0.0001, 10]
- `gain`: Gain adjustment per channel (highlights). Typical range: [0, 2]

**Example:**
```c
/* Warm shadows, cool highlights */
alwan_rgb_f64 lift  = {0.05, 0.02, -0.02};  /* Push shadows warm */
alwan_rgb_f64 gamma = {1.0, 1.0, 1.0};      /* Neutral midtones */
alwan_rgb_f64 gain  = {0.95, 1.0, 1.05};    /* Cool highlights */

alwan_rgb_f64 result;
alwan_lgg_apply_f64(&result, &pixel, &lift, &gamma, &gain);
```

**Batch variants:**
```c
int alwan_lgg_apply_{T}_map_interleave(
        alwan_{T} *rgb_out, size_t out_stride,
        alwan_{T} const *rgb_in, size_t in_stride, size_t count,
        alwan_rgb_{T} const *lift, alwan_rgb_{T} const *gamma,
        alwan_rgb_{T} const *gain);

int alwan_lgg_apply_map_interleave_ex(
        void *out, size_t out_stride, void const *in, size_t in_stride,
        size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
        alwan_rgb_f64 const *lift, alwan_rgb_f64 const *gamma,
        alwan_rgb_f64 const *gain);
```

Also available: `alwan_lgg_apply_{T}_map_planar`, `alwan_lgg_apply_map_planar_ex`.

---

## ASC CDL / SOP

Slope-Offset-Power (SOP) plus Saturation: the ASC Color Decision List grading
model. Parameters are carried in `alwan_aces_lmt_params_{T}` and operate in
**AP1 (ACEScg) linear** space.

```c
/* out = (in * slope + offset) ^ power, then saturation adjustment */
typedef struct {
    alwan_{T} slope[3], offset[3], power[3], saturation;
} alwan_aces_lmt_params_{T};
```

### alwan_aces_lmt_params_init_{T}

```c
void alwan_aces_lmt_params_init_{T}(alwan_aces_lmt_params_{T} *params);
```

Initialize to the neutral identity preset (`slope=1`, `offset=0`, `power=1`,
`saturation=1`). Use it as a starting point, then override the knobs you want.

### alwan_aces_lmt_apply_{T}

```c
void alwan_aces_lmt_apply_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in,
                              alwan_aces_lmt_params_{T} const *params);
```

Apply SOP per channel, then the saturation adjustment.

**Example:**
```c
alwan_aces_lmt_params_f64 cdl;
alwan_aces_lmt_params_init_f64(&cdl);   /* neutral preset */
cdl.slope[0]   = 1.05;                   /* lift reds in highlights */
cdl.offset[2]  = -0.01;                  /* cool the blacks */
cdl.saturation = 1.10;                   /* +10% saturation */

alwan_rgb_f64 graded;
alwan_aces_lmt_apply_f64(&graded, &acescg_pixel, &cdl);
```

> The CDL/SOP primitives live in the ACES LMT section of the header; see
> [ACES Pipeline](aces.md) for the surrounding RRT/ODT and look-LMT context.

---

## Color Matrix

### alwan_color_matrix_apply_{T}

```c
void alwan_color_matrix_apply_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in,
                                  alwan_mat3x3_{T} const *matrix_3x3);
```

Apply a 3x3 color transformation matrix to RGB values.

### alwan_color_matrix_get_preset_{T}

```c
int alwan_color_matrix_get_preset_{T}(alwan_mat3x3_{T} *matrix_3x3,
                                      alwan_color_matrix_preset_{T} preset);
```

Get a preset creative color-grading matrix. Returns `ALWAN_OK`, or
`ALWAN_E_INVALID` for an unknown preset.

**Preset Enum** (`alwan_color_matrix_preset_{T}`: the enum is precision-independent;
`alwan_color_matrix_preset_f32` is a typedef of `alwan_color_matrix_preset_f64`):
```c
typedef enum {
    ALWAN_COLOR_MATRIX_SEPIA = 0,       /* Sepia tone effect */
    ALWAN_COLOR_MATRIX_VINTAGE,         /* Vintage look */
    ALWAN_COLOR_MATRIX_BLEACH_BYPASS,   /* Bleach bypass effect */
    ALWAN_COLOR_MATRIX_COOL,            /* Cool tone shift */
    ALWAN_COLOR_MATRIX_WARM,            /* Warm tone shift */
    ALWAN_COLOR_MATRIX_MONOCHROME,      /* Black and white */
    ALWAN_COLOR_MATRIX_NIGHT_VISION     /* Night vision look */
} alwan_color_matrix_preset_f64;
```

**Example:**
```c
/* Apply sepia tone to an image */
alwan_mat3x3_f64 sepia;
alwan_color_matrix_get_preset_f64(&sepia, ALWAN_COLOR_MATRIX_SEPIA);

alwan_color_matrix_apply_f64_map_interleave(
    out, 3 * sizeof(alwan_f64),
    in,  3 * sizeof(alwan_f64),
    width * height, &sepia);
```

**Batch variants:**
```c
int alwan_color_matrix_apply_{T}_map_interleave(
        alwan_{T} *rgb_out, size_t out_stride,
        alwan_{T} const *rgb_in, size_t in_stride, size_t count,
        alwan_mat3x3_{T} const *matrix);

int alwan_color_matrix_apply_map_interleave_ex(
        void *out, size_t out_stride, void const *in, size_t in_stride,
        size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
        alwan_mat3x3_f64 const *matrix);
```

Also available: `alwan_color_matrix_apply_{T}_map_planar`, `alwan_color_matrix_apply_map_planar_ex`.

---

## Exposure & Printer Lights

### alwan_printer_lights_apply_{T}

```c
void alwan_printer_lights_apply_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in,
                                    alwan_{T} red_lights, alwan_{T} green_lights,
                                    alwan_{T} blue_lights);
```

Film-style printer-light color correction in **log exposure**. Each light unit is
approximately `0.025` log-exposure change.

**Parameters:**
- `red_lights`, `green_lights`, `blue_lights`: Printer light values (0–50, default 25 = neutral)

**Example:**
```c
/* Push warm: add red, reduce blue */
alwan_printer_lights_apply_f64(&result, &pixel, 27.0, 25.0, 23.0);
```

**Batch variants:**
```c
int alwan_printer_lights_apply_{T}_map_interleave(
        alwan_{T} *rgb_out, size_t out_stride,
        alwan_{T} const *rgb_in, size_t in_stride, size_t count,
        alwan_{T} red_lights, alwan_{T} green_lights, alwan_{T} blue_lights);

int alwan_printer_lights_apply_map_interleave_ex(
        void *out, size_t out_stride, void const *in, size_t in_stride,
        size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
        alwan_f64 red_lights, alwan_f64 green_lights, alwan_f64 blue_lights);
```

Also available: `alwan_printer_lights_apply_{T}_map_planar`, `alwan_printer_lights_apply_map_planar_ex`.

### alwan_exposure_tonemap_{T}

```c
int alwan_exposure_tonemap_{T}(alwan_{T} *out, alwan_{T} L, alwan_{T} exposure);
```

Exposure-based tone mapping on a luminance value: `out = 1 - exp(-2^exposure * L)`.
`exposure` is an EV offset in stops (0 = neutral, +1 = one stop brighter). Returns
`ALWAN_OK` on success. (Declared with the [HDR](hdr.md) tone-mapping operators.)

---

## White Balance

### alwan_white_balance_from_gray_{T}

```c
void alwan_white_balance_from_gray_{T}(alwan_rgb_{T} *multipliers_out,
                                       alwan_rgb_{T} const *measured_gray);
```

Compute white-balance multipliers from a measured neutral-gray sample. The
multipliers are normalized so the minimum channel equals `1.0`.

### alwan_white_balance_apply_{T}

```c
void alwan_white_balance_apply_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb,
                                   alwan_rgb_{T} const *multipliers);
```

Apply white-balance multipliers to an RGB color.

**Example:**
```c
/* Measure a gray card under the scene illuminant */
alwan_rgb_f64 measured_gray = {0.42, 0.50, 0.65};  /* Bluish gray */

alwan_rgb_f64 multipliers;
alwan_white_balance_from_gray_f64(&multipliers, &measured_gray);

/* Apply to all pixels in the image */
alwan_white_balance_apply_f64_map_interleave(
    out, 3 * sizeof(alwan_f64),
    in,  3 * sizeof(alwan_f64),
    width * height, &multipliers);
```

**Batch variants:**
```c
int alwan_white_balance_apply_{T}_map_interleave(
        alwan_{T} *rgb_out, size_t out_stride,
        alwan_{T} const *rgb_in, size_t in_stride, size_t count,
        alwan_rgb_{T} const *multipliers);

int alwan_white_balance_apply_map_interleave_ex(
        void *out, size_t out_stride, void const *in, size_t in_stride,
        size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
        alwan_rgb_f64 const *multipliers);
```

Also available: `alwan_white_balance_apply_{T}_map_planar`, `alwan_white_balance_apply_map_planar_ex`.

---

## Camera Profiling (Polynomial Color Correction)

Polynomial and root-polynomial color-correction matrix (CCM) fitting for
camera-to-reference profiling.

> **f64-facade note:** the CCM *fits* (`alwan_colour_correction_matrix_cheung2004_{T}`
> and `..._finlayson2015_{T}`) run their least-squares normal-equations solve in
> `double` internally regardless of the requested precision: squaring the
> condition number in `float` would be numerically fragile. The f32 entry points
> therefore stay available even in an f32-only build via `ALWAN_WITH_F64_FACADE`.
> See [Configuration](../configuration.md).

### Cheung 2004 Method

Polynomial expansion for camera-to-reference color correction.

```c
/* Polynomial expansion terms = number of terms in the expanded polynomial */
typedef enum {
    ALWAN_POLY_CHEUNG_3  = 3,   /* [R, G, B] */
    ALWAN_POLY_CHEUNG_4  = 4,   /* [R, G, B, 1] */
    ALWAN_POLY_CHEUNG_5  = 5,   /* [R, G, B, RG, 1] */
    ALWAN_POLY_CHEUNG_7  = 7,   /* [R, G, B, RG, RB, GB, 1] */
    ALWAN_POLY_CHEUNG_8  = 8,   /* [R, G, B, RG, RB, GB, RGB, 1] */
    ALWAN_POLY_CHEUNG_10 = 10,  /* [R, G, B, RG, RB, GB, R^2, G^2, B^2, 1] */
    ALWAN_POLY_CHEUNG_11 = 11,  /* [R, G, B, RG, RB, GB, R^2, G^2, B^2, RGB, 1] */
    ALWAN_POLY_CHEUNG_14 = 14,
    ALWAN_POLY_CHEUNG_16 = 16,
    ALWAN_POLY_CHEUNG_17 = 17,
    ALWAN_POLY_CHEUNG_19 = 19,
    ALWAN_POLY_CHEUNG_20 = 20,
    ALWAN_POLY_CHEUNG_22 = 22,
    ALWAN_POLY_CHEUNG_35 = 35   /* Full 35-term expansion (maximum) */
} alwan_poly_cheung_terms;
```

#### alwan_colour_correction_matrix_cheung2004_{T}

```c
int alwan_colour_correction_matrix_cheung2004_{T}(
        alwan_{T} *matrix_out,        /* Output correction matrix (terms x 3) */
        alwan_{T} const *M_T,         /* Test (camera) RGB, Nx3 row-major */
        alwan_{T} const *M_R,         /* Reference RGB, Nx3 row-major */
        int num_samples,              /* Number of color samples (N) */
        alwan_poly_cheung_terms terms);
```

#### alwan_colour_correct_cheung2004_{T}

```c
void alwan_colour_correct_cheung2004_{T}(
        alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb,
        alwan_{T} const *matrix,
        alwan_poly_cheung_terms terms);
```

### Finlayson 2015 Method

Root-polynomial color correction with improved exposure invariance.

```c
int alwan_colour_correction_matrix_finlayson2015_{T}(
        alwan_{T} *matrix_out, int *matrix_size,
        alwan_{T} const *M_T, alwan_{T} const *M_R,
        int num_samples, int degree, int root_poly);

void alwan_colour_correct_finlayson2015_{T}(
        alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb,
        alwan_{T} const *matrix, int degree, int root_poly);
```

- `degree`: polynomial degree (1–4)
- `root_poly`: if non-zero, use root-polynomial expansion (must match between fit and apply)

**Example (camera profiling workflow):**
```c
/* 1. Photograph a ColorChecker under scene lighting */
/* 2. Extract camera RGB for each patch */
alwan_f64 camera_rgb[24 * 3];    /* 24 patches, camera-measured, Nx3 row-major */
alwan_f64 reference_rgb[24 * 3]; /* 24 patches, known reference */

/* 3. Compute correction matrix */
alwan_f64 matrix[11 * 3];
alwan_colour_correction_matrix_cheung2004_f64(
    matrix, camera_rgb, reference_rgb, 24, ALWAN_POLY_CHEUNG_11);

/* 4. Apply to all image pixels */
alwan_rgb_f64 corrected;
alwan_colour_correct_cheung2004_f64(&corrected, &camera_pixel, matrix,
                                    ALWAN_POLY_CHEUNG_11);
```

### Polynomial Expansion Utilities

```c
/* Expand RGB to polynomial terms (Cheung) */
int alwan_poly_expand_cheung2004_{T}(alwan_{T} *out, alwan_rgb_{T} const *rgb,
                                     alwan_poly_cheung_terms terms);

/* Expand RGB to polynomial terms (Finlayson) */
int alwan_poly_expand_finlayson2015_{T}(alwan_{T} *out, int *out_size,
                                        alwan_rgb_{T} const *rgb,
                                        int degree, int root_poly);

/* Generic Vandermonde expansion */
int alwan_poly_expand_vandermonde_{T}(alwan_{T} *out, int *out_size,
                                      alwan_{T} const *a, int a_size, int degree);
```

---

## Error Codes

| Code | Value | Meaning |
|------|-------|---------|
| `ALWAN_OK` | 0 | Success |
| `ALWAN_E_INVALID` | -1 | NULL pointer or invalid parameter (e.g. unknown matrix preset, `num_samples < terms`) |
| `ALWAN_E_NOMEM` | -4 | Scratch allocation failed during a CCM fit (`alwan_colour_correction_matrix_*`) |
| `ALWAN_E_DIVZERO` | -5 | Singular normal-equations matrix during a CCM fit (`alwan_colour_correction_matrix_*`) |

`void`-returning helpers (LGG, color-matrix apply, printer lights, ASC CDL,
white-balance apply, `alwan_colour_correct_*`) perform the transform in place and
skip the work on NULL pointers.

---

## See Also

- [Transfer Functions](transfer-functions.md): OETF/EOTF encoding
- [ACES Pipeline](aces.md): ACES LMT grading and output transforms
- [HDR](hdr.md): exposure tone-mapping operators
- [Color Spaces](color-spaces.md): RGB conversions
- [Configuration](../configuration.md): `_{T}` variants and the f64-facade
