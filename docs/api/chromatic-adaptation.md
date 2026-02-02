# Chromatic Adaptation API

Functions for transforming colors between different illuminants (white points).

---

## Overview

Chromatic adaptation adjusts colors to account for changes in illumination. When viewing a color under different light sources (e.g., daylight vs tungsten), the perceived color changes. Chromatic adaptation transforms correct for this effect.

**Use cases:**
- Converting between D65 and D50 white points
- Matching colors across different viewing conditions
- Print-to-screen color matching
- Cross-media color reproduction

---

## Core Function

### alwan_xyz_adapt

```c
alwan_result alwan_xyz_adapt(
    alwan_vec3 *xyz_out,          // Output first
    alwan_ctx *ctx,
    alwan_cat_type transform,
    const alwan_vec3 *src_white,
    const alwan_vec3 *dst_white,
    const alwan_vec3 *xyz_in,
    size_t count,
    size_t in_stride,
    size_t out_stride
);
```

Adapts XYZ colors from one white point to another.

**Parameters:**
- `xyz_out` — Output XYZ colors adapted to destination illuminant
- `ctx` — Library context
- `transform` — Chromatic adaptation transform type
- `src_white` — Source white point in XYZ
- `dst_white` — Destination white point in XYZ
- `xyz_in` — Input XYZ colors under source illuminant
- `count` — Number of colors
- `in_stride` — Input stride in bytes (0 = packed)
- `out_stride` — Output stride in bytes (0 = packed)

**Returns:**
- `ALWAN_SUCCESS` — Adaptation successful
- `ALWAN_ERROR_INVALID_PARAMETER` — Invalid parameters
- `ALWAN_ERROR_UNSUPPORTED` — Transform not available

**Example:**
```c
alwan_ctx *ctx = alwan_create(NULL);

// Color under D50 illuminant
alwan_vec3 xyz_d50 = {0.5, 0.6, 0.4};

// Adapt to D65 (output first)
alwan_vec3 xyz_d65;
alwan_xyz_adapt(&xyz_d65, ctx, ALWAN_CAT_BRADFORD,
                &alwan_d50_xyz, &alwan_d65_xyz,
                &xyz_d50, 1, 0, 0);

alwan_destroy(ctx);
```

---

## Chromatic Adaptation Transforms

### Transform Types

```c
typedef enum {
    ALWAN_CAT_BRADFORD,
    ALWAN_CAT_CAT02,
    ALWAN_CAT_CAT16,
    ALWAN_CAT_XYZ_SCALING
} alwan_cat_type;
```

---

### ALWAN_CAT_BRADFORD

**Bradford** — Industry standard chromatic adaptation.

```c
alwan_xyz_adapt(xyz_out, ctx, ALWAN_CAT_BRADFORD,
                &alwan_d50_xyz, &alwan_d65_xyz,
                xyz_in, count, stride_in, stride_out);
```

**Characteristics:**
- **Accuracy:** Best overall for most applications
- **Industry adoption:** Used in ICC profiles, Adobe products
- **Derivation:** Empirically optimized for human vision

**Transform matrix:**
```
M_Bradford = [
   0.8951   0.2664  -0.1614
  -0.7502   1.7135   0.0367
   0.0389  -0.0685   1.0296
]
```

**Use when:**
- Converting between D50 and D65 (printing/screen)
- ICC profile conversions
- General-purpose chromatic adaptation
- Industry-standard compatibility required

**Precision:** ±1e-6 (float), ±1e-12 (double)

---

### ALWAN_CAT_CAT02

**CAT02** — Used in CIECAM02 color appearance model.

```c
alwan_xyz_adapt(xyz_out, ctx, ALWAN_CAT_CAT02,
                &src_white, &dst_white,
                xyz_in, count, stride_in, stride_out);
```

**Characteristics:**
- **Derivation:** Designed for CIECAM02
- **Accuracy:** Good for perceptual color matching
- **Linearity:** Better linearity than Bradford

**Transform matrix:**
```
M_CAT02 = [
   0.7328   0.4296  -0.1624
  -0.7036   1.6975   0.0061
   0.0030   0.0136   0.9834
]
```

**Use when:**
- Working with CIECAM02 appearance model
- Perceptual color matching
- Better mathematical properties needed

---

### ALWAN_CAT_CAT16

**CAT16** — Used in CAM16 color appearance model.

```c
alwan_xyz_adapt(xyz_out, ctx, ALWAN_CAT_CAT16,
                &src_white, &dst_white,
                xyz_in, count, stride_in, stride_out);
```

**Characteristics:**
- **Derivation:** Refined version of CAT02 for CAM16
- **Accuracy:** Best for modern color appearance models
- **Usage:** Modern perceptual applications

**Transform matrix:**
```
M_CAT16 = [
   0.401288   0.650173  -0.051461
  -0.250268   1.204414   0.045854
  -0.002079   0.048952   0.953127
]
```

**Use when:**
- Working with CAM16 appearance model
- Most accurate perceptual matching
- Modern color science applications

---

### ALWAN_CAT_XYZ_SCALING

**XYZ Scaling** (von Kries) — Simplest adaptation, scales XYZ independently.

```c
alwan_xyz_adapt(xyz_out, ctx, ALWAN_CAT_XYZ_SCALING,
                &src_white, &dst_white,
                xyz_in, count, stride_in, stride_out);
```

**Characteristics:**
- **Method:** Scale each XYZ component independently
- **Accuracy:** Lower than modern methods
- **Speed:** Fastest (no matrix multiply)

**Formula:**
```
X_out = X_in * (X_dst_white / X_src_white)
Y_out = Y_in * (Y_dst_white / Y_src_white)
Z_out = Z_in * (Z_dst_white / Z_src_white)
```

**Use when:**
- Simple scaling is sufficient
- Maximum performance required
- Legacy compatibility

**Note:** Generally not recommended for perceptual accuracy.

---

## Standard Illuminants

Alwan provides constants for common illuminants:

```c
extern const alwan_vec3 alwan_d65_xyz;  // Daylight 6504K
extern const alwan_vec3 alwan_d50_xyz;  // Daylight 5003K (printing)
extern const alwan_vec3 alwan_a_xyz;    // Tungsten 2856K
extern const alwan_vec3 alwan_e_xyz;    // Equal energy
```

**Values (normalized, Y=1.0):**

| Illuminant | X | Y | Z |
|-----------|---|---|---|
| **D65** | 0.95047 | 1.00000 | 1.08883 |
| **D50** | 0.96422 | 1.00000 | 0.82521 |
| **A** | 1.09850 | 1.00000 | 0.35585 |
| **E** | 1.00000 | 1.00000 | 1.00000 |

---

## Usage Patterns

### Pattern 1: Print-to-Screen Conversion

```c
// Colors for print (D50 standard)
alwan_vec3 xyz_print[1000];

// Convert to screen (D65) - output first
alwan_vec3 xyz_screen[1000];
alwan_xyz_adapt(xyz_screen, ctx, ALWAN_CAT_BRADFORD,
                &alwan_d50_xyz, &alwan_d65_xyz,
                xyz_print, 1000, 12, 12);
```

---

### Pattern 2: Complete Color Space Conversion with Adaptation

```c
// Convert Adobe RGB (D65) → ProPhoto RGB (D50)

// 1. Adobe RGB → XYZ (D65) - output first
alwan_vec3 adobe_rgb = {0.8, 0.3, 0.2};
alwan_vec3 xyz_d65;
alwan_rgb_to_xyz(&xyz_d65, ctx, "adobe_rgb", &adobe_rgb, 1, 0, 0);

// 2. Adapt D65 → D50 - output first
alwan_vec3 xyz_d50;
alwan_xyz_adapt(&xyz_d50, ctx, ALWAN_CAT_BRADFORD,
                &alwan_d65_xyz, &alwan_d50_xyz,
                &xyz_d65, 1, 0, 0);

// 3. XYZ (D50) → ProPhoto RGB - output first
alwan_vec3 prophoto_rgb;
alwan_xyz_to_rgb(&prophoto_rgb, ctx, "prophoto_rgb", &xyz_d50, 1, 0, 0);
```

---

### Pattern 3: Custom Illuminant Adaptation

```c
// Adapt from fluorescent F2 to D65 - output first
alwan_vec3 f2_white = {0.99186, 1.00000, 0.67393};  // F2 illuminant

alwan_vec3 color_f2 = {0.5, 0.6, 0.4};
alwan_vec3 color_d65;

alwan_xyz_adapt(&color_d65, ctx, ALWAN_CAT_BRADFORD,
                &f2_white, &alwan_d65_xyz,
                &color_f2, 1, 0, 0);
```

---

## Transform Comparison

| Transform | Accuracy | Speed | Use Case |
|-----------|----------|-------|----------|
| **Bradford** | ★★★★★ | ★★★★☆ | General purpose, industry standard |
| **CAT02** | ★★★★☆ | ★★★★☆ | CIECAM02, perceptual |
| **CAT16** | ★★★★★ | ★★★★☆ | CAM16, most accurate |
| **XYZ Scaling** | ★★☆☆☆ | ★★★★★ | Simple, legacy |

**Recommendation:** Use Bradford for most applications unless working with specific color appearance models.

---

## Mathematical Details

### Two-Step Process

Chromatic adaptation uses a two-step process:

1. **Transform to cone response space**
   ```
   LMS_src = M_adapt * XYZ_src
   LMS_white_src = M_adapt * XYZ_white_src
   LMS_white_dst = M_adapt * XYZ_white_dst
   ```

2. **Scale and transform back**
   ```
   LMS_dst = LMS_src .* (LMS_white_dst ./ LMS_white_src)
   XYZ_dst = M_adapt^(-1) * LMS_dst
   ```

### Complete Formula

```
XYZ_out = M^(-1) * diag(D) * M * XYZ_in

where:
  M = adaptation matrix (Bradford, CAT02, etc.)
  D = diagonal scaling: [Xw_dst/Xw_src, Yw_dst/Yw_src, Zw_dst/Zw_src]
  Xw, Yw, Zw = white point in cone response space
```

---

## Precision & Limits

### Numerical Accuracy

| Aspect | Float | Double |
|--------|-------|--------|
| Matrix multiply | ±1e-6 | ±1e-14 |
| Division | ±1e-6 | ±1e-14 |
| Overall | ±1e-5 | ±1e-12 |

### Round-Trip Accuracy

Adapting A→B→A should recover original:

```c
alwan_vec3 original = {0.5, 0.6, 0.4};
alwan_vec3 adapted, recovered;

alwan_xyz_adapt(&adapted, ctx, ALWAN_CAT_BRADFORD,
                &alwan_d65_xyz, &alwan_d50_xyz,
                &original, 1, 0, 0);

alwan_xyz_adapt(&recovered, ctx, ALWAN_CAT_BRADFORD,
                &alwan_d50_xyz, &alwan_d65_xyz,
                &adapted, 1, 0, 0);

// |original - recovered| < 1e-6 (float) or 1e-12 (double)
```

---

## Common Conversions

### D65 ↔ D50

Most common conversion (screen ↔ print):

```c
// D65 → D50 (screen to print) - output first
alwan_xyz_adapt(xyz_out, ctx, ALWAN_CAT_BRADFORD,
                &alwan_d65_xyz, &alwan_d50_xyz,
                xyz_in, count, stride, stride);

// D50 → D65 (print to screen) - output first
alwan_xyz_adapt(xyz_out, ctx, ALWAN_CAT_BRADFORD,
                &alwan_d50_xyz, &alwan_d65_xyz,
                xyz_in, count, stride, stride);
```

---

### A → D65

Tungsten to daylight:

```c
alwan_xyz_adapt(xyz_out, ctx, ALWAN_CAT_BRADFORD,
                &alwan_a_xyz, &alwan_d65_xyz,
                xyz_in, count, stride, stride);
```

---

## Performance

### Timing

Typical performance on modern CPU:
- **Bradford/CAT02/CAT16:** ~5 ns/color (float), ~7 ns/color (double)
- **XYZ Scaling:** ~2 ns/color (float), ~3 ns/color (double)

**Bulk operation speedup:** ~4× with SIMD (when available)

---

## Error Handling

### Invalid White Points

```c
alwan_vec3 invalid_white = {0.0, 0.0, 0.0};  // Invalid!

alwan_result r = alwan_xyz_adapt(xyz_out, ctx, ALWAN_CAT_BRADFORD,
                                  &invalid_white, &alwan_d65_xyz,
                                  xyz_in, count, stride, stride);

if (r == ALWAN_ERROR_INVALID_PARAMETER) {
    // White point Y must be > 0
}
```

### Singular Matrices

If adaptation matrix is singular (extremely rare):
- Returns `ALWAN_ERROR_INVALID_PARAMETER`
- Check white point values for reasonableness

---

## Best Practices

1. **Use Bradford by default**
   ```c
   alwan_xyz_adapt(ctx, ALWAN_CAT_BRADFORD, ...);
   ```

2. **Match transform to workflow**
   - Bradford: General purpose, ICC profiles
   - CAT16: Working with CAM16
   - CAT02: Working with CIECAM02

3. **Normalize white points**
   - Y component should be 1.0
   - Alwan constants are pre-normalized

4. **Avoid repeated adaptation**
   - One adaptation: ±1e-6 error
   - Ten adaptations: ±1e-5 error (accumulated)
   - Minimize adaptation steps in pipeline

---

## See Also

- [Color Spaces](color-spaces.md) — XYZ conversions
- [Color Appearance Models](color-appearance.md) — CIECAM02, CAM16
- [Examples](../examples.md) — Usage examples
- [Precision & Limits](../precision-and-limits.md) — Numerical accuracy
