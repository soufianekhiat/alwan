# Gamut Operations API

Functions for gamut mapping, gamut volume calculation, and gamut coverage analysis.

---

## Overview

Gamut operations handle colors that fall outside a color space's representable range:

**Gamut Mapping:** Convert out-of-gamut colors to in-gamut equivalents
**Gamut Volume:** Calculate the 3D volume of a color space
**Gamut Coverage:** Measure how much of one gamut fits in another
**Gamut Testing:** Check if colors are within gamut boundaries

**Implemented:**
- `alwan_gamut_map` — General gamut mapping with multiple methods
- `alwan_gamut_map_advanced` — Advanced gamut mapping options
- `alwan_gamut_volume_mc` — Monte Carlo gamut volume calculation
- `alwan_gamut_volume_ratio` — Ratio between two gamut volumes
- `alwan_gamut_coverage` — Gamut coverage calculation
- `alwan_is_within_pointer_gamut` — Pointer's gamut testing

**Not yet implemented:**
- `alwan_is_in_gamut` — Check if RGB is within a specific color space gamut
- `alwan_is_on_spectral_locus` — Check if on spectral locus
- `alwan_get_gamut_boundary` — Get gamut boundary for visualization

---

## Functions

### Gamut Testing

> **Note:** `alwan_is_in_gamut` is not yet implemented. Use manual bounds checking for now.

```c
bool alwan_is_in_gamut(
    alwan_ctx *ctx,
    const alwan_vec3 *rgb,
    alwan_rgb_space space
);
```

Check if an RGB color is within the [0,1] cube (displayable).

**Example:**
```c
alwan_vec3 rgb = {1.2, 0.5, -0.1};

if (!alwan_is_in_gamut(ctx, &rgb, ALWAN_RGB_SPACE_SRGB)) {
    printf("Color is out of sRGB gamut\n");
    // Apply gamut mapping
}
```

---

### Gamut Mapping

#### Simple Clipping

```c
void alwan_gamut_clip(
    alwan_vec3 *rgb  // In-place output
);
```

Clip RGB values to [0, 1] range (simple, fast, but may shift hue).

**Example:**
```c
alwan_vec3 rgb = {1.2, 0.5, -0.1};
alwan_gamut_clip(&rgb);
// rgb = {1.0, 0.5, 0.0}
```

**Characteristics:**
- ✓ Fast
- ✓ Preserves relative channel ratios where possible
- ✗ May cause hue shifts for saturated colors
- ✗ May reduce chroma more than necessary

---

#### Perceptual Gamut Mapping

```c
alwan_result alwan_gamut_map_perceptual(
    alwan_vec3 *rgb_out,          // Output first
    alwan_ctx *ctx,
    alwan_rgb_space space,
    const alwan_vec3 *rgb_in
);
```

Map out-of-gamut colors while preserving hue and minimizing lightness/chroma changes.

**Algorithm:**
1. Convert RGB → Lab
2. Reduce chroma while preserving hue
3. Adjust lightness if needed
4. Convert back to RGB

**Characteristics:**
- ✓ Preserves hue
- ✓ Minimizes perceptual change
- ✗ Slower than clipping
- ✗ May slightly darken bright colors

**Example:**
```c
alwan_vec3 bt2020_rgb = {0.9, 0.3, 0.1};  // Wide gamut
alwan_vec3 srgb_rgb;

alwan_gamut_map_perceptual(&srgb_rgb, ctx,  // Output first
                            ALWAN_RGB_SPACE_SRGB,
                            &bt2020_rgb);
```

---

#### Hue-Preserving Mapping

```c
alwan_result alwan_gamut_map_hue_preserving(
    alwan_vec3 *rgb_out,          // Output first
    alwan_ctx *ctx,
    alwan_rgb_space space,
    const alwan_vec3 *rgb_in
);
```

Map to gamut boundary while strictly preserving hue (in LCh space).

**Method:**
1. Convert RGB → Lab → LCh
2. Binary search for maximum chroma at that hue/lightness
3. Map to boundary
4. Convert back to RGB

**Use case:** When hue accuracy is critical (e.g., brand colors)

---

### Gamut Volume

```c
alwan_result alwan_gamut_volume_mc(
    alwan_scalar *volume,         // Output first
    alwan_ctx *ctx,
    alwan_rgb_space space
);
```

Calculate the volume of a color space in Lab space.

**Units:** Cubic Lab units

**Example volumes:**
- **sRGB:** ~820,000
- **Adobe RGB:** ~1,200,000
- **ProPhoto RGB:** ~2,900,000
- **BT.2020:** ~1,570,000

**Example:**
```c
alwan_scalar srgb_volume, bt2020_volume;

alwan_gamut_volume_mc(&srgb_volume, ctx, ALWAN_RGB_SPACE_SRGB);
alwan_gamut_volume_mc(&bt2020_volume, ctx, ALWAN_RGB_SPACE_BT2020);

printf("BT.2020 is %.1fx larger than sRGB\n",
       bt2020_volume / srgb_volume);
```

---

### Gamut Coverage

```c
alwan_result alwan_gamut_coverage(
    alwan_scalar *coverage_ratio,  // Output first
    alwan_ctx *ctx,
    alwan_rgb_space source_space,
    alwan_rgb_space target_space
);
```

Calculate what fraction of source gamut fits inside target gamut.

**Returns:** Ratio [0, 1] where:
- 1.0 = source entirely within target
- 0.5 = 50% of source fits in target
- <0.1 = very little overlap

**Example:**
```c
alwan_scalar coverage;

// How much of BT.2020 fits in sRGB?
alwan_gamut_coverage(&coverage, ctx,  // Output first
                     ALWAN_RGB_SPACE_BT2020,
                     ALWAN_RGB_SPACE_SRGB);

printf("sRGB covers %.1f%% of BT.2020\n", coverage * 100);
// Output: "sRGB covers ~52% of BT.2020"
```

---

### Gamut Volume Ratio

```c
alwan_result alwan_gamut_volume_ratio(
    alwan_scalar *ratio,          // Output first
    alwan_ctx *ctx,
    alwan_rgb_space space1,
    alwan_rgb_space space2
);
```

Calculate the ratio of two gamut volumes.

**Example:**
```c
alwan_scalar ratio;
alwan_gamut_volume_ratio(&ratio, ctx,  // Output first
                         ALWAN_RGB_SPACE_BT2020,
                         ALWAN_RGB_SPACE_SRGB);
printf("BT.2020 is %.2fx larger than sRGB\n", ratio);
// Output: "BT.2020 is 1.91x larger than sRGB"
```

---

### Pointer's Gamut Testing

```c
bool alwan_is_within_pointer_gamut(
    const alwan_vec2 *xy
);
```

Check if a chromaticity is within Pointer's gamut (real surface colors).

**Pointer's Gamut:** The range of chromaticities of real surface colors under daylight.

**Use case:** Determine if a color could physically exist as a surface color

---

### Spectral Locus Testing

```c
bool alwan_is_on_spectral_locus(
    const alwan_vec2 *xy,
    alwan_scalar tolerance
);
```

Check if a chromaticity is on or near the spectral locus (monochromatic light).

---

## Gamut Mapping Strategies

### Strategy Comparison

| Method | Speed | Hue Accuracy | Lightness | Use Case |
|--------|-------|-------------|-----------|----------|
| **Clip** | ★★★★★ | ★☆☆☆☆ | ★★★☆☆ | Real-time, non-critical |
| **Perceptual** | ★★★☆☆ | ★★★★☆ | ★★★★☆ | General purpose |
| **Hue-preserving** | ★★☆☆☆ | ★★★★★ | ★★★☆☆ | Brand colors, graphics |

---

## Usage Patterns

### Pattern 1: Safe Display Conversion

```c
// Convert wide gamut to sRGB for display
alwan_vec3 prophoto_rgb = {0.9, 0.3, 0.2};

// Convert to sRGB space (output first)
alwan_vec3 srgb_rgb;
alwan_rgb_convert(&srgb_rgb, ctx,
                  ALWAN_RGB_SPACE_PROPHOTO_RGB,
                  ALWAN_RGB_SPACE_SRGB,
                  &prophoto_rgb);

// Check if mapping needed
if (!alwan_is_in_gamut(ctx, &srgb_rgb, ALWAN_RGB_SPACE_SRGB)) {
    // Apply perceptual mapping (output first)
    alwan_gamut_map_perceptual(&srgb_rgb, ctx,
                                ALWAN_RGB_SPACE_SRGB,
                                &srgb_rgb);
}

// Now safe to display
```

---

### Pattern 2: Compare Color Space Sizes

```c
const char *spaces[] = {
    "sRGB", "Adobe RGB", "ProPhoto RGB", "BT.2020"
};
alwan_rgb_space space_ids[] = {
    ALWAN_RGB_SPACE_SRGB,
    ALWAN_RGB_SPACE_ADOBE_RGB_1998,
    ALWAN_RGB_SPACE_PROPHOTO_RGB,
    ALWAN_RGB_SPACE_BT2020
};

printf("Color Space Volumes:\n");
for (int i = 0; i < 4; i++) {
    alwan_scalar volume;
    alwan_gamut_volume_mc(&volume, ctx, space_ids[i]);  // Output first
    printf("  %s: %.0f\n", spaces[i], volume);
}
```

---

### Pattern 3: Batch Gamut Mapping

```c
// Map entire image
void map_image_to_srgb(
    alwan_ctx *ctx,
    alwan_vec3 *rgb_data,
    int pixel_count
) {
    for (int i = 0; i < pixel_count; i++) {
        if (!alwan_is_in_gamut(ctx, &rgb_data[i],
                               ALWAN_RGB_SPACE_SRGB)) {
            alwan_gamut_map_perceptual(&rgb_data[i], ctx,  // Output first
                                        ALWAN_RGB_SPACE_SRGB,
                                        &rgb_data[i]);
        }
    }
}
```

---

## Gamut Visualization Data

```c
alwan_result alwan_get_gamut_boundary(
    alwan_vec2 **xy_points,       // Output first
    size_t *count,                // Output
    alwan_ctx *ctx,
    alwan_rgb_space space
);
```

Get chromaticity boundary points for visualization.

**Returns:** Array of xy points tracing gamut boundary

**Use case:** Plotting gamut on chromaticity diagram

---

---

## See Also

- [Color Spaces](color-spaces.md) — RGB space conversions
- [Examples](../examples.md) — Usage examples
- [Precision & Limits](../precision-and-limits.md) — Numerical accuracy
