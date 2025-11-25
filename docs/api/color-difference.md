# Color Difference API

> **⚠️ PLANNED API:** This documentation describes planned functionality that is not yet implemented in v0.1-alpha. Refer to [alwan.h](../../src/alwan/alwan.h) for current capabilities.

Functions for calculating perceptual color differences using various ΔE (delta E) metrics.

---

## Overview

Color difference metrics quantify how different two colors appear to human vision. Different formulas optimize for different use cases:

- **ΔE76 (CIE 1976)** — Simple Euclidean distance in Lab
- **ΔE94 (CIE 1994)** — Improved with weighting factors
- **ΔE00 (CIEDE2000)** — Most accurate, industry standard
- **ΔE CMC** — Textile industry standard
- **ΔE CAM02** — Based on CIECAM02 color appearance model
- **ΔE CAM16** — Based on CAM16 (improved CIECAM02)
- **ΔE ITP** — For HDR content (ICtCp-based)

---

## Perceptual Thresholds

| ΔE Value | Perception |
|----------|-----------|
| < 1.0 | Not perceptible by human eyes |
| 1.0 - 2.0 | Perceptible through close observation |
| 2.0 - 10.0 | Perceptible at a glance |
| 11.0 - 49.0 | Colors are more similar than opposite |
| > 50.0 | Colors are completely different |

---

## Planned Functions

### ΔE76 (CIE 1976)

```c
alwan_scalar alwan_delta_e_76(
    const alwan_vec3 *lab1,
    const alwan_vec3 *lab2
);
```

Simplest color difference: Euclidean distance in Lab space.

**Formula:**
```
ΔE76 = sqrt((L2-L1)² + (a2-a1)² + (b2-b1)²)
```

**Use case:** Quick approximation, not perceptually uniform

**Example:**
```c
alwan_vec3 lab1 = {50.0, 20.0, 10.0};
alwan_vec3 lab2 = {51.0, 21.0, 11.0};

alwan_scalar delta_e = alwan_delta_e_76(&lab1, &lab2);
// delta_e ≈ 1.73
```

---

### ΔE94 (CIE 1994)

```c
alwan_scalar alwan_delta_e_94(
    const alwan_vec3 *lab1,
    const alwan_vec3 *lab2,
    alwan_scalar kL,  // Lightness weight (1.0 graphic arts, 2.0 textiles)
    alwan_scalar K1,  // Chroma weight (0.045 graphic arts, 0.048 textiles)
    alwan_scalar K2   // Hue weight (0.015 graphic arts, 0.014 textiles)
);
```

Improved color difference with application-specific weighting.

**Formula:**
```
ΔE94 = sqrt((ΔL/kL*SL)² + (ΔC/SC)² + (ΔH/SH)²)
```

**Presets:**
```c
// Graphic arts
alwan_delta_e_94(&lab1, &lab2, 1.0, 0.045, 0.015);

// Textiles
alwan_delta_e_94(&lab1, &lab2, 2.0, 0.048, 0.014);
```

---

### ΔE00 (CIEDE2000)

```c
alwan_scalar alwan_delta_e_2000(
    const alwan_vec3 *lab1,
    const alwan_vec3 *lab2,
    alwan_scalar kL,  // Lightness weight (default 1.0)
    alwan_scalar kC,  // Chroma weight (default 1.0)
    alwan_scalar kH   // Hue weight (default 1.0)
);
```

Most accurate perceptual color difference, industry standard.

**Characteristics:**
- Accounts for non-uniformity of Lab space
- Adjusts for neutral colors (low chroma)
- Considers hue-dependent perceptual differences
- Most complex formula, most accurate results

**Use case:** Default choice for perceptual color matching

**Example:**
```c
alwan_scalar delta_e = alwan_delta_e_2000(&lab1, &lab2, 1.0, 1.0, 1.0);

if (delta_e < 1.0) {
    printf("Just noticeable difference\n");
} else if (delta_e < 2.3) {
    printf("Small color difference\n");
} else {
    printf("Noticeable color difference\n");
}
```

---

### ΔE CMC (Colour Measurement Committee)

```c
alwan_scalar alwan_delta_e_cmc(
    const alwan_vec3 *lab1,
    const alwan_vec3 *lab2,
    alwan_scalar l,  // Lightness tolerance (2.0 typical)
    alwan_scalar c   // Chroma tolerance (1.0 typical)
);
```

Textile industry standard color difference.

**Ratios:**
- **2:1** (l=2.0, c=1.0) — Acceptability (typical)
- **1:1** (l=1.0, c=1.0) — Perceptibility

---

### ΔE CAM02-LCD/SCD

```c
alwan_scalar alwan_delta_e_cam02_lcd(
    const alwan_vec3 *xyz1,
    const alwan_vec3 *xyz2,
    const alwan_cam_viewing_conditions *vc
);

alwan_scalar alwan_delta_e_cam02_scd(
    const alwan_vec3 *xyz1,
    const alwan_vec3 *xyz2,
    const alwan_cam_viewing_conditions *vc
);
```

Color difference based on CIECAM02 uniform color space.

- **LCD** — Large Color Difference
- **SCD** — Small Color Difference

---

### ΔE CAM16-LCD/SCD

```c
alwan_scalar alwan_delta_e_cam16_lcd(
    const alwan_vec3 *xyz1,
    const alwan_vec3 *xyz2,
    const alwan_cam_viewing_conditions *vc
);

alwan_scalar alwan_delta_e_cam16_scd(
    const alwan_vec3 *xyz1,
    const alwan_vec3 *xyz2,
    const alwan_cam_viewing_conditions *vc
);
```

Color difference based on CAM16 (improved accuracy over CAM02).

---

### ΔE ITP (for HDR)

```c
alwan_scalar alwan_delta_e_itp(
    const alwan_vec3 *ictcp1,
    const alwan_vec3 *ictcp2,
    alwan_scalar scalar  // Scaling factor (720.0 typical)
);
```

Color difference for HDR content using ICtCp color space.

**Formula:**
```
ΔE_ITP = scalar * sqrt((I2-I1)² + (CT2-CT1)² + (CP2-CP1)²)
```

**Use case:** HDR video quality assessment

---

## Bulk Operations

```c
void alwan_delta_e_2000_bulk(
    const alwan_vec3 *lab1,
    const alwan_vec3 *lab2,
    alwan_scalar *delta_e,
    size_t count,
    size_t lab1_stride,
    size_t lab2_stride,
    size_t delta_e_stride
);
```

Compute ΔE for arrays of colors efficiently.

---

## Comparison of Metrics

| Metric | Accuracy | Speed | Use Case |
|--------|----------|-------|----------|
| **ΔE76** | ★☆☆☆☆ | ★★★★★ | Quick approximation |
| **ΔE94** | ★★★☆☆ | ★★★★☆ | Improved perceptual |
| **ΔE00** | ★★★★★ | ★★★☆☆ | Industry standard |
| **ΔE CMC** | ★★★☆☆ | ★★★★☆ | Textiles |
| **ΔE CAM16** | ★★★★☆ | ★★☆☆☆ | Appearance-based |
| **ΔE ITP** | ★★★★☆ | ★★★★☆ | HDR content |

---

## Usage Examples

### Find Closest Color in Palette

```c
int find_closest_color(
    const alwan_vec3 *target_lab,
    const alwan_vec3 *palette_lab,
    int palette_size
) {
    int closest_idx = 0;
    alwan_scalar min_delta_e = INFINITY;

    for (int i = 0; i < palette_size; i++) {
        alwan_scalar delta_e = alwan_delta_e_2000(
            target_lab, &palette_lab[i], 1.0, 1.0, 1.0
        );

        if (delta_e < min_delta_e) {
            min_delta_e = delta_e;
            closest_idx = i;
        }
    }

    return closest_idx;
}
```

---

### Quality Control Tolerance

```c
bool is_within_tolerance(
    const alwan_vec3 *reference_lab,
    const alwan_vec3 *sample_lab,
    alwan_scalar tolerance
) {
    alwan_scalar delta_e = alwan_delta_e_2000(
        reference_lab, sample_lab, 1.0, 1.0, 1.0
    );

    return delta_e <= tolerance;
}

// Usage
if (is_within_tolerance(&target_color, &produced_color, 2.0)) {
    printf("Color match acceptable (ΔE ≤ 2.0)\n");
} else {
    printf("Color out of tolerance\n");
}
```

---

## Implementation Status

**Current:** Not yet implemented
**Planned for:** v0.2

---

## See Also

- [Color Spaces](color-spaces.md) — Lab conversions
- [Color Appearance Models](color-appearance.md) — CAM-based ΔE
- [Examples](../examples.md) — Practical examples
