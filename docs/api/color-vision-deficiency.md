# Color Vision Deficiency (CVD) Simulation API

Functions for simulating color blindness to test accessibility of visual content.

---

## Overview

CVD simulation transforms colors to approximate how they appear to individuals with color vision deficiencies. Alwan implements the **Brettel, Vienot & Mollon (1997)** simulation model using confusion lines.

**Types of color vision deficiency:**

| Type | Cone affected | Prevalence (male) | Prevalence (female) |
|------|--------------|-------------------|---------------------|
| Protanopia | L-cone absent (red-blind) | ~1% | ~0.01% |
| Deuteranopia | M-cone absent (green-blind) | ~1% | ~0.01% |
| Tritanopia | S-cone absent (blue-blind) | ~0.002% | ~0.002% |
| Protanomaly | L-cone deficient (red-weak) | ~1% | ~0.03% |
| Deuteranomaly | M-cone deficient (green-weak) | ~5% | ~0.35% |
| Tritanomaly | S-cone deficient (blue-weak) | rare | rare |

---

## CVD Type Enum

```c
typedef enum {
    ALWAN_CVD_PROTANOPIA = 0,     /* Red-blind (L-cone absent) */
    ALWAN_CVD_DEUTERANOPIA = 1,   /* Green-blind (M-cone absent) */
    ALWAN_CVD_TRITANOPIA = 2,     /* Blue-blind (S-cone absent) */
    ALWAN_CVD_PROTANOMALY = 3,    /* Red-weak (L-cone deficient) */
    ALWAN_CVD_DEUTERANOMALY = 4,  /* Green-weak (M-cone deficient) */
    ALWAN_CVD_TRITANOMALY = 5     /* Blue-weak (S-cone deficient) */
} alwan_cvd_type;
```

---

## Single-Element Function

### alwan_simulate_cvd

```c
int alwan_simulate_cvd(alwan_rgb *rgb_out,
                       alwan_rgb const *rgb_in,
                       alwan_cvd_type cvd_type,
                       alwan_scalar severity);
```

Simulate color vision deficiency for a single RGB color.

**Parameters:**
- `rgb_out` — Output simulated RGB color as seen by person with CVD
- `rgb_in` — Input linear RGB color [0, 1]
- `cvd_type` — Type of color vision deficiency
- `severity` — Severity of deficiency [0, 1] (1.0 = complete, 0.0 = normal vision). Only applies to anomalous trichromacy types (protanomaly, deuteranomaly, tritanomaly). Dichromacy types (protanopia, deuteranopia, tritanopia) always simulate full severity.

**Returns:** `ALWAN_OK` on success, `ALWAN_E_INVALID` on error.

**Example:**
```c
alwan_rgb red = {1.0, 0.0, 0.0};
alwan_rgb simulated;

/* How does pure red look to someone with deuteranopia? */
alwan_simulate_cvd(&simulated, &red, ALWAN_CVD_DEUTERANOPIA, 1.0);
/* simulated ~= {0.63, 0.63, 0.0} -- red and green are confused */

/* Partial red-weakness (50% severity) */
alwan_simulate_cvd(&simulated, &red, ALWAN_CVD_PROTANOMALY, 0.5);
```

---

## Batch Functions

### alwan_simulate_cvd_map_interleave

```c
int alwan_simulate_cvd_map_interleave(
    alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
    alwan_cvd_type cvd_type, alwan_scalar severity,
    size_t count, size_t in_stride, size_t out_stride);
```

Batch CVD simulation for interleaved RGB arrays.

### Type-specific batch functions

```c
int alwan_simulate_protanopia_map_interleave(
    alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
    alwan_scalar severity,
    size_t count, size_t in_stride, size_t out_stride);

int alwan_simulate_deuteranopia_map_interleave(
    alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
    alwan_scalar severity,
    size_t count, size_t in_stride, size_t out_stride);

int alwan_simulate_tritanopia_map_interleave(
    alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
    alwan_scalar severity,
    size_t count, size_t in_stride, size_t out_stride);
```

---

## Typed Batch Functions (`_ex`)

Accept `void*` buffers with explicit pixel format for mixed-precision pipelines:

```c
int alwan_simulate_cvd_map_interleave_ex(
    void *out, alwan_pixel_format out_fmt,
    void const *in, alwan_pixel_format in_fmt,
    alwan_cvd_type cvd_type, alwan_scalar severity,
    size_t count, size_t in_stride, size_t out_stride);
```

Type-specific `_ex` variants also available for protanopia, deuteranopia, and tritanopia.

---

## Planar Layout Functions

For image data stored as separate R, G, B channel buffers:

```c
int alwan_simulate_cvd_map_planar(
    alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2,
    alwan_cvd_type cvd_type, alwan_scalar severity,
    size_t count, size_t in_stride, size_t out_stride);
```

Type-specific planar variants (`_map_planar` and `_map_planar_ex`) also available.

---

## Usage Example

### Accessibility check for UI colors

```c
/* Check if two UI colors are distinguishable under all CVD types */
alwan_rgb color_a = {0.2, 0.8, 0.3};  /* Green button */
alwan_rgb color_b = {0.8, 0.2, 0.2};  /* Red warning */

alwan_cvd_type types[] = {
    ALWAN_CVD_PROTANOPIA, ALWAN_CVD_DEUTERANOPIA, ALWAN_CVD_TRITANOPIA
};
const char *names[] = {"Protanopia", "Deuteranopia", "Tritanopia"};

for (int i = 0; i < 3; i++) {
    alwan_rgb sim_a, sim_b;
    alwan_simulate_cvd(&sim_a, &color_a, types[i], 1.0);
    alwan_simulate_cvd(&sim_b, &color_b, types[i], 1.0);

    /* Convert to Lab and check ΔE */
    alwan_lab lab_a, lab_b;
    alwan_srgb_to_lab(&lab_a, &sim_a);
    alwan_srgb_to_lab(&lab_b, &sim_b);

    alwan_scalar de = alwan_delta_e_2000(&lab_a, &lab_b);
    printf("%s: dE2000 = %.1f %s\n", names[i], de,
           de < 3.0 ? "FAIL" : "OK");
}
```

### Full-image simulation

```c
/* Simulate deuteranopia on entire image buffer */
alwan_simulate_deuteranopia_map_interleave(
    (alwan_scalar*)pixels_out,
    (alwan_scalar const*)pixels_in,
    1.0,                               /* full severity */
    width * height,                    /* pixel count */
    3 * sizeof(alwan_scalar),          /* stride */
    3 * sizeof(alwan_scalar));
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — NULL pointer or invalid CVD type

---

## See Also

- [Color Spaces](color-spaces.md) — RGB/Lab conversions
- [Color Difference](color-difference.md) — Measuring perceptual difference
- [Vision Science](vision.md) — Visual perception models
