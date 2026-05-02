# Chromatic Adaptation API

Functions for transforming colors between different illuminants (white points).

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

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

Computes the chromatic adaptation matrix from source to destination white point.

**Returns:** `ALWAN_OK` on success, `ALWAN_E_INVALID` if white points are invalid.

---

### alwan_cat_adapt_{T}

```c
int alwan_cat_adapt_{T}(alwan_{T} *xyz_out,
                         alwan_xyz_{T} const *src_white_xyz,
                         alwan_xyz_{T} const *dst_white_xyz,
                         alwan_cat_method method,
                         alwan_{T} const *xyz_in,
                         size_t count,
                         size_t in_stride,
                         size_t out_stride);
```

Applies chromatic adaptation to XYZ colors (bulk interleaved).

**Example:**
```c
alwan_xyz_{T} d50 = {0.96422, 1.0, 0.82521};
alwan_xyz_{T} d65 = {0.95047, 1.0, 1.08883};

alwan_xyz_{T} xyz_in  = {0.5, 0.6, 0.4};
alwan_xyz_{T} xyz_out;

alwan_cat_adapt_{T}((alwan_{T}*)&xyz_out, &d50, &d65, ALWAN_CAT_BRADFORD,
                    (alwan_{T} const*)&xyz_in, 1,
                    sizeof(alwan_xyz_{T}), sizeof(alwan_xyz_{T}));
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

Zhai & Luo (2018) two-step chromatic adaptation with degree of adaptation parameters.

---

## Chromatic Adaptation Methods

```c
typedef enum {
    ALWAN_CAT_XYZ_SCALING = 0,    /* Simple XYZ scaling (von Kries) */
    ALWAN_CAT_BRADFORD    = 1,    /* Bradford (industry standard) */
    ALWAN_CAT_VON_KRIES   = 2,    /* von Kries */
    ALWAN_CAT_CAT02       = 3,    /* CAT02 (CIECAM02) */
    ALWAN_CAT_CAT16       = 4,    /* CAT16 (CAM16) */
    ALWAN_CAT_CMCCAT97    = 5,    /* CMC CAT97 */
    ALWAN_CAT_CMCCAT2000  = 6,    /* CMC CAT2000 */
    ALWAN_CAT_SHARP       = 7,    /* Sharp */
    ALWAN_CAT_BIANCO_SCHETTINI_2010 = 8,
    ALWAN_CAT_FAIRCHILD   = 9,    /* Fairchild */
    ALWAN_CAT_HUNT_POINTER_ESTEVEZ = 10,
    ALWAN_CAT_ZHAI_2018   = 11    /* Zhai & Luo 2018 */
} alwan_cat_method;
```

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

Simplest adaptation -- scales XYZ independently. Not recommended for perceptual accuracy.
Use when maximum performance is required or legacy compatibility is needed.

---

## Common White Point Values

```c
/* Y=1.0 normalized (use Y=100.0 for CAM viewing conditions) */
alwan_xyz_{T} d65 = {0.95047, 1.0, 1.08883};  /* Daylight 6504K */
alwan_xyz_{T} d50 = {0.96422, 1.0, 0.82521};  /* Daylight 5003K (printing) */
alwan_xyz_{T} a   = {1.09850, 1.0, 0.35585};  /* Tungsten 2856K */
alwan_xyz_{T} e   = {1.00000, 1.0, 1.00000};  /* Equal energy */
```

Or compute from standard illuminant enum:

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
alwan_cat_adapt_{T}((double*)xyz_out, &d50, &d65, ALWAN_CAT_BRADFORD,
                    (double const*)xyz_in, 1000,
                    sizeof(alwan_xyz_{T}), sizeof(alwan_xyz_{T}));
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
- `ALWAN_E_INVALID` (-1) -- Invalid white point (e.g., Y=0)
- `ALWAN_E_DIVZERO` (-5) -- Division by zero in adaptation

---

## See Also

- [Color Spaces](color-spaces.md) -- XYZ conversions
- [Color Appearance Models](color-appearance.md) -- CIECAM02, CAM16
- [Examples](../examples.md) -- Usage examples
