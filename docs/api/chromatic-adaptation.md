# Chromatic Adaptation API

Functions for transforming colors between different illuminants (white points).

---

## Overview

Chromatic adaptation adjusts colors to account for changes in illumination. When viewing a color under different light sources (e.g., daylight vs tungsten), the perceived color changes.

**Use cases:**
- Converting between D65 and D50 white points
- Matching colors across different viewing conditions
- Print-to-screen color matching

---

## Core Functions

### alwan_cat_matrix

```c
int alwan_cat_matrix(
    alwan_mat3x3 *out,            // Output adaptation matrix
    alwan_xyz const *src_white_xyz,
    alwan_xyz const *dst_white_xyz,
    alwan_cat_method method
);
```

Computes the chromatic adaptation matrix from source to destination white point.

**Returns:** `ALWAN_OK` on success, `ALWAN_E_INVALID` if white points are invalid.

---

### alwan_cat_adapt

```c
int alwan_cat_adapt(
    alwan_scalar *xyz_out,        // Output adapted XYZ colors
    alwan_xyz const *src_white_xyz,
    alwan_xyz const *dst_white_xyz,
    alwan_cat_method method,
    alwan_scalar const *xyz_in,
    size_t count,
    size_t in_stride,             // Input stride in bytes
    size_t out_stride             // Output stride in bytes
);
```

Applies chromatic adaptation to XYZ colors (bulk operation).

**Example:**
```c
alwan_xyz d50 = {0.96422, 1.0, 0.82521};
alwan_xyz d65 = {0.95047, 1.0, 1.08883};

alwan_xyz xyz_in = {0.5, 0.6, 0.4};
alwan_xyz xyz_out;

alwan_cat_adapt((alwan_scalar*)&xyz_out, &d50, &d65, ALWAN_CAT_BRADFORD,
                (alwan_scalar*)&xyz_in, 1, sizeof(alwan_xyz), sizeof(alwan_xyz));
```

---

### alwan_cat_zhai2018

```c
int alwan_cat_zhai2018(
    alwan_xyz *xyz_out,
    alwan_xyz const *xyz_in,
    alwan_xyz const *xyz_src,
    alwan_xyz const *xyz_dst,
    alwan_scalar D_src,
    alwan_scalar D_dst,
    alwan_xyz const *xyz_baseline,
    alwan_cat_method transform
);
```

Zhai & Luo (2018) two-step chromatic adaptation with degree of adaptation parameters.

---

## Chromatic Adaptation Methods

```c
typedef enum {
    ALWAN_CAT_XYZ_SCALING = 0,    // Simple XYZ scaling (von Kries)
    ALWAN_CAT_BRADFORD    = 1,    // Bradford (industry standard)
    ALWAN_CAT_VON_KRIES   = 2,    // von Kries
    ALWAN_CAT_CAT02       = 3,    // CAT02 (CIECAM02)
    ALWAN_CAT_CAT16       = 4,    // CAT16 (CAM16)
    ALWAN_CAT_CMCCAT97    = 5,    // CMC CAT97
    ALWAN_CAT_CMCCAT2000  = 6,    // CMC CAT2000
    ALWAN_CAT_SHARP       = 7,    // Sharp
    ALWAN_CAT_BIANCO_SCHETTINI_2010 = 8,
    ALWAN_CAT_FAIRCHILD   = 9,    // Fairchild
    ALWAN_CAT_HUNT_POINTER_ESTEVEZ = 10,
    ALWAN_CAT_ZHAI_2018   = 11    // Zhai & Luo 2018
} alwan_cat_method;
```

---

### ALWAN_CAT_BRADFORD

**Bradford** — Industry standard chromatic adaptation.

**Characteristics:**
- Best overall for most applications
- Used in ICC profiles, Adobe products
- Empirically optimized for human vision

**Use when:**
- Converting between D50 and D65 (printing/screen)
- ICC profile conversions
- General-purpose chromatic adaptation

---

### ALWAN_CAT_CAT02

**CAT02** — Used in CIECAM02 color appearance model.

**Use when:**
- Working with CIECAM02 appearance model
- Perceptual color matching

---

### ALWAN_CAT_CAT16

**CAT16** — Used in CAM16 color appearance model.

**Use when:**
- Working with CAM16 appearance model
- Most accurate perceptual matching
- Modern color science applications

---

### ALWAN_CAT_XYZ_SCALING

**XYZ Scaling** (von Kries) — Simplest adaptation, scales XYZ independently.

**Use when:**
- Simple scaling is sufficient
- Maximum performance required
- Legacy compatibility

**Note:** Generally not recommended for perceptual accuracy.

---

## Common White Point Values

Create white point XYZ values directly (Y=1.0 normalized):

```c
alwan_xyz d65 = {0.95047, 1.0, 1.08883};  // Daylight 6504K
alwan_xyz d50 = {0.96422, 1.0, 0.82521};  // Daylight 5003K (printing)
alwan_xyz a   = {1.09850, 1.0, 0.35585};  // Tungsten 2856K
alwan_xyz e   = {1.00000, 1.0, 1.00000};  // Equal energy
```

---

## Usage Patterns

### Pattern 1: Print-to-Screen Conversion

```c
alwan_xyz d50 = {0.96422, 1.0, 0.82521};
alwan_xyz d65 = {0.95047, 1.0, 1.08883};

// Convert 1000 colors from D50 (print) to D65 (screen)
alwan_cat_adapt((alwan_scalar*)xyz_out, &d50, &d65, ALWAN_CAT_BRADFORD,
                (alwan_scalar*)xyz_in, 1000,
                sizeof(alwan_xyz), sizeof(alwan_xyz));
```

---

### Pattern 2: Get Adaptation Matrix for Manual Use

```c
alwan_mat3x3 adapt_matrix;
alwan_cat_matrix(&adapt_matrix, &d50, &d65, ALWAN_CAT_BRADFORD);

// Apply manually using alwan_mat3_mulv
alwan_vec3 xyz_in_v, xyz_out_v;
alwan_mat3_mulv(&xyz_out_v, &adapt_matrix, &xyz_in_v);
```

---

## Transform Comparison

| Transform | Accuracy | Speed | Use Case |
|-----------|----------|-------|----------|
| **Bradford** | 5/5 | 4/5 | General purpose, industry standard |
| **CAT02** | 4/5 | 4/5 | CIECAM02, perceptual |
| **CAT16** | 5/5 | 4/5 | CAM16, most accurate |
| **XYZ Scaling** | 2/5 | 5/5 | Simple, legacy |

**Recommendation:** Use Bradford for most applications unless working with specific color appearance models.

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — Invalid white point (e.g., Y=0)
- `ALWAN_E_DIVZERO` (-5) — Division by zero in adaptation

---

## See Also

- [Color Spaces](color-spaces.md) — XYZ conversions
- [Color Appearance Models](color-appearance.md) — CIECAM02, CAM16
- [Examples](../examples.md) — Usage examples
