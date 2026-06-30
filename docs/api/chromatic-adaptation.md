# Chromatic Adaptation API

Functions for transforming colors between different illuminants (white points).

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`. Both precisions compile by default; restrict with `ALWAN_BUILD_ONLY_F32`
> / `ALWAN_BUILD_ONLY_F64` (see [configuration](../configuration.md)).

---

## Overview

Chromatic adaptation adjusts colors to account for changes in illumination. When viewing a color
under different light sources (e.g., daylight vs tungsten), the perceived color changes.

**Use cases:**
- Converting between D65 and D50 white points
- Matching colors across different viewing conditions
- Print-to-screen color matching

---

## Core Functions

### alwan_cat_matrix_{T}

```c
int alwan_cat_matrix_{T}(alwan_mat3x3_{T} *out,
                         alwan_xyz_{T} const *src_white_xyz,
                         alwan_xyz_{T} const *dst_white_xyz,
                         alwan_cat_method method);
```

Computes the 3x3 chromatic adaptation matrix from source to destination white point.
White points are XYZ normalized to Y=1.

**Returns:** `ALWAN_OK` on success, `ALWAN_E_INVALID` if the white points are invalid.

---

### alwan_xyz_adapt_{T}

```c
int alwan_xyz_adapt_{T}(alwan_{T} *xyz_out, size_t out_stride,
                        alwan_{T} const *xyz_in, size_t in_stride,
                        size_t count,
                        alwan_xyz_{T} const *src_white_xyz,
                        alwan_xyz_{T} const *dst_white_xyz,
                        alwan_cat_method method);
```

Applies chromatic adaptation to a batch of XYZ colors (interleaved/AoS). Each `*_stride`
is in bytes and immediately follows its buffer (memcpy argument order); use
`sizeof(alwan_xyz_{T})` for a packed array.

**Returns:** `ALWAN_OK` on success, `ALWAN_E_INVALID` if parameters are invalid.

**Example:**
```c
alwan_xyz_{T} d50 = {0.96422, 1.0, 0.82521};
alwan_xyz_{T} d65 = {0.95047, 1.0, 1.08883};

alwan_xyz_{T} xyz_in  = {0.5, 0.6, 0.4};
alwan_xyz_{T} xyz_out;

alwan_xyz_adapt_{T}((alwan_{T}*)&xyz_out, sizeof(alwan_xyz_{T}),
                    (alwan_{T} const*)&xyz_in, sizeof(alwan_xyz_{T}),
                    1, &d50, &d65, ALWAN_CAT_BRADFORD);
```

---

### alwan_cat_zhai2018_{T}

```c
int alwan_cat_zhai2018_{T}(alwan_xyz_{T} *xyz_out,
                           alwan_xyz_{T} const *xyz_in,
                           alwan_xyz_{T} const *xyz_src,
                           alwan_xyz_{T} const *xyz_dst,
                           alwan_{T} D_src,
                           alwan_{T} D_dst,
                           alwan_xyz_{T} const *xyz_baseline,
                           alwan_cat_method transform);
```

Zhai & Luo (2018) two-step chromatic adaptation. Adapts XYZ from the source illuminant to the
destination illuminant via a baseline illuminant, with independent degree-of-adaptation factors:

- `xyz_in` -- input XYZ under the source illuminant (Y=100 scale).
- `xyz_src` / `xyz_dst` -- source / destination illuminant XYZ (Y=100 scale).
- `D_src` / `D_dst` -- degree of adaptation in `[0,1]` for the source / destination
  illuminant (`1` = full adaptation).
- `xyz_baseline` -- baseline illuminant XYZ, or `NULL` for equal-energy white `{100,100,100}`.
- `transform` -- underlying one-step CAT (`ALWAN_CAT_CAT02` or `ALWAN_CAT_CAT16`).

**Returns:** `ALWAN_OK` on success.

---

## Chromatic Adaptation Methods

```c
typedef enum {
    ALWAN_CAT_XYZ_SCALING      = 0,  /* Von Kries in XYZ space (simplest) */
    ALWAN_CAT_BRADFORD         = 1,  /* Bradford (most common, used in ICC) */
    ALWAN_CAT_CAT02            = 2,  /* CAT02 (from CIECAM02) */
    ALWAN_CAT_CAT16            = 3,  /* CAT16 (from CAM16) */

    /* Extended CAT methods */
    ALWAN_CAT_SHARP            = 4,  /* Sharp transform */
    ALWAN_CAT_FAIRCHILD        = 5,  /* Fairchild 1990 */
    ALWAN_CAT_CMCCAT97         = 6,  /* CMC CAT97 */
    ALWAN_CAT_CMCCAT2000       = 7,  /* CMC CAT2000 */
    ALWAN_CAT_CAT02_BRILL_2008 = 8,  /* CAT02 Brill 2008 variant */
    ALWAN_CAT_BIANCO_2010      = 9,  /* Bianco 2010 */
    ALWAN_CAT_BIANCO_PC_2010   = 10, /* Bianco PC 2010 */

    /* Two-step CAT methods */
    ALWAN_CAT_ZHAI_2018        = 11  /* Zhai & Luo 2018 two-step CAT */
} alwan_cat_method;
```

12 methods total. `ALWAN_CAT_ZHAI_2018` is the only two-step method and is applied through
`alwan_cat_zhai2018_{T}`; the others are one-step matrices usable with `alwan_cat_matrix_{T}`
and `alwan_xyz_adapt_{T}`.

---

### ALWAN_CAT_BRADFORD

Industry standard chromatic adaptation. Used in ICC profiles and Adobe products.
Empirically optimized for human vision. Best overall for most applications.

**Use when:** Converting between D50 and D65, ICC profile conversions, general-purpose.

---

### ALWAN_CAT_CAT02

Used in the CIECAM02 color appearance model.

---

### ALWAN_CAT_CAT16

Used in the CAM16 color appearance model. Most accurate perceptual matching.

---

### ALWAN_CAT_XYZ_SCALING

Simplest adaptation -- scales XYZ independently (Von Kries in XYZ space). Not recommended for
perceptual accuracy. Use when maximum performance is required or legacy compatibility is needed.

---

## Common White Point Values

```c
/* Y=1.0 normalized (use Y=100.0 for CAM viewing conditions) */
alwan_xyz_{T} d65 = {0.95047, 1.0, 1.08883};  /* Daylight 6504K */
alwan_xyz_{T} d50 = {0.96422, 1.0, 0.82521};  /* Daylight 5003K (printing) */
alwan_xyz_{T} a   = {1.09850, 1.0, 0.35585};  /* Tungsten 2856K */
alwan_xyz_{T} e   = {1.00000, 1.0, 1.00000};  /* Equal energy */
```

Or compute from a standard illuminant enum:

```c
alwan_xyz_{T} d65;
alwan_illuminant_white_point_{T}(&d65, ALWAN_ILLUMINANT_D65,
                                 ALWAN_OBSERVER_CIE_1931_2DEG);
```

---

## Usage Patterns

### Pattern 1: Print-to-Screen Conversion

```c
alwan_xyz_{T} d50 = {0.96422, 1.0, 0.82521};
alwan_xyz_{T} d65 = {0.95047, 1.0, 1.08883};

/* Convert 1000 colors from D50 (print) to D65 (screen) */
alwan_xyz_adapt_{T}((alwan_{T}*)xyz_out, sizeof(alwan_xyz_{T}),
                    (alwan_{T} const*)xyz_in, sizeof(alwan_xyz_{T}),
                    1000, &d50, &d65, ALWAN_CAT_BRADFORD);
```

---

### Pattern 2: Get Adaptation Matrix for Manual Use

```c
alwan_mat3x3_{T} adapt_matrix;
alwan_cat_matrix_{T}(&adapt_matrix, &d50, &d65, ALWAN_CAT_BRADFORD);

/* Apply manually */
alwan_vec3_{T} xyz_in_v, xyz_out_v;
alwan_mat3_mulv_{T}(&xyz_out_v, &adapt_matrix, &xyz_in_v);
```

---

## Transform Comparison

| Transform | Accuracy | Speed | Use Case |
|---|---|---|---|
| **Bradford** | 5/5 | 4/5 | General purpose, industry standard |
| **CAT02** | 4/5 | 4/5 | CIECAM02, perceptual |
| **CAT16** | 5/5 | 4/5 | CAM16, most accurate |
| **XYZ Scaling** | 2/5 | 5/5 | Simple, legacy |

**Recommendation:** Use Bradford for most applications unless working with a specific CAM.

---

## Error Codes

- `ALWAN_OK` (0) -- Success
- `ALWAN_E_INVALID` (-1) -- Invalid white point or parameters (e.g., Y=0)
- `ALWAN_E_DIVZERO` (-5) -- Division by zero in adaptation

---

## See Also

- [Color Spaces](color-spaces.md) -- XYZ conversions
- [Color Appearance Models](color-appearance.md) -- CIECAM02, CAM16
- [Examples](../examples.md) -- Usage examples
