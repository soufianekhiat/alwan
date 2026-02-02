# Color Space Conversions API

Functions for converting between different color spaces and models.

---

## Overview

Alwan supports conversions between:
- **CIE spaces:** XYZ, xyY, Lab, Luv, LCh(ab), LCh(uv)
- **Perceptual models:** IPT, ICtCp, JzAzBz, Oklab, Oklch
- **RGB families:** sRGB, Adobe RGB, BT.709, BT.2020, Display P3, ACES, and more
- **Encoding spaces:** HSV, HSL, YCbCr, CMY, CMYK, HCL, IHLS

All conversion functions support bulk operations with stride for efficient array processing.

---

## CIE XYZ Conversions

### alwan_xyz_to_lab

```c
alwan_result alwan_xyz_to_lab(
    alwan_vec3 *lab,              // Output first
    const alwan_vec3 *xyz,
    const alwan_vec3 *white_point,
    size_t count,
    size_t xyz_stride,
    size_t lab_stride
);
```

Converts CIE XYZ to CIE Lab (L\*a\*b\*).

**Parameters:**
- `xyz` — Input XYZ values (tristimulus values, Y is luminance)
- `white_point` — Reference white point in XYZ (e.g., `&alwan_d65_xyz`)
- `lab` — Output Lab values
- `count` — Number of colors to convert
- `xyz_stride` — Byte stride between XYZ values (0 = packed)
- `lab_stride` — Byte stride between Lab values (0 = packed)

**Input ranges:**
- XYZ: [0, ∞) for each component
- White point: Any valid XYZ with Y ≈ 1.0

**Output ranges:**
- L: [0, 100] (lightness)
- a: [-128, 127] typical, unbounded
- b: [-128, 127] typical, unbounded

**Example:**
```c
alwan_vec3 xyz = {0.5, 0.6, 0.4};
alwan_vec3 lab;
alwan_xyz_to_lab(&lab, &xyz, &alwan_d65_xyz, 1, 0, 0);  // Output first
printf("Lab: L=%.2f, a=%.2f, b=%.2f\n", lab.x, lab.y, lab.z);
```

**Precision:**
- Float: ±0.001 typical error
- Double: ±1e-10 typical error

---

### alwan_lab_to_xyz

```c
alwan_result alwan_lab_to_xyz(
    alwan_vec3 *xyz,              // Output first
    const alwan_vec3 *lab,
    const alwan_vec3 *white_point,
    size_t count,
    size_t lab_stride,
    size_t xyz_stride
);
```

Converts CIE Lab (L\*a\*b\*) to CIE XYZ.

**Parameters:** (same structure as `alwan_xyz_to_lab`)

**Input ranges:**
- L: [0, 100]
- a, b: Unbounded (typically [-128, 127])

**Output ranges:**
- XYZ: [0, ∞) for each component

**Note:** Inverse of `alwan_xyz_to_lab` with same white point.

---

### alwan_xyz_to_luv

```c
alwan_result alwan_xyz_to_luv(
    alwan_vec3 *luv,              // Output first
    const alwan_vec3 *xyz,
    const alwan_vec3 *white_point,
    size_t count,
    size_t xyz_stride,
    size_t luv_stride
);
```

Converts CIE XYZ to CIE Luv (L\*u\*v\*).

**Output ranges:**
- L: [0, 100]
- u, v: Unbounded (typically [-100, 100])

**Use case:** Luv is more perceptually uniform than XYZ, alternative to Lab.

---

### alwan_luv_to_xyz

```c
alwan_result alwan_luv_to_xyz(
    alwan_vec3 *xyz,              // Output first
    const alwan_vec3 *luv,
    const alwan_vec3 *white_point,
    size_t count,
    size_t luv_stride,
    size_t xyz_stride
);
```

Converts CIE Luv back to XYZ.

---

### alwan_xyz_to_xyy

```c
alwan_result alwan_xyz_to_xyy(
    alwan_vec3 *xyy,              // Output first
    const alwan_vec3 *xyz,
    size_t count,
    size_t xyz_stride,
    size_t xyy_stride
);
```

Converts XYZ to xyY (chromaticity + luminance).

**Output format:**
- x: x chromaticity coordinate [0, 1]
- y: y chromaticity coordinate [0, 1]
- Y: Luminance [0, ∞)

**Special case:** Black (X=Y=Z=0) maps to (x=0, y=0, Y=0)

---

### alwan_xyy_to_xyz

```c
alwan_result alwan_xyy_to_xyz(
    alwan_vec3 *xyz,              // Output first
    const alwan_vec3 *xyy,
    size_t count,
    size_t xyy_stride,
    size_t xyz_stride
);
```

Converts xyY back to XYZ.

---

## Cylindrical Representations

### alwan_lab_to_lch

```c
alwan_result alwan_lab_to_lch(
    alwan_vec3 *lch,              // Output first
    const alwan_vec3 *lab,
    size_t count,
    size_t lab_stride,
    size_t lch_stride
);
```

Converts Lab to LCh(ab) (cylindrical Lab).

**Output format:**
- L: [0, 100] (same as Lab)
- C: [0, ∞) (chroma, sqrt(a² + b²))
- h: [0, 360) (hue angle in degrees)

**Use case:** Easier hue manipulation than Cartesian Lab.

---

### alwan_lch_to_lab

```c
alwan_result alwan_lch_to_lab(
    alwan_vec3 *lab,              // Output first
    const alwan_vec3 *lch,
    size_t count,
    size_t lch_stride,
    size_t lab_stride
);
```

Converts LCh(ab) back to Lab.

---

### alwan_luv_to_lchuv

```c
alwan_result alwan_luv_to_lchuv(
    alwan_vec3 *lchuv,            // Output first
    const alwan_vec3 *luv,
    size_t count,
    size_t luv_stride,
    size_t lchuv_stride
);
```

Converts Luv to LCh(uv) (cylindrical Luv).

---

### alwan_lchuv_to_luv

```c
alwan_result alwan_lchuv_to_luv(
    alwan_vec3 *luv,              // Output first
    const alwan_vec3 *lchuv,
    size_t count,
    size_t lchuv_stride,
    size_t luv_stride
);
```

Converts LCh(uv) back to Luv.

---

## Modern Perceptual Models

### alwan_xyz_to_oklab

```c
alwan_result alwan_xyz_to_oklab(
    alwan_vec3 *oklab,            // Output first
    const alwan_vec3 *xyz,
    size_t count,
    size_t xyz_stride,
    size_t oklab_stride
);
```

Converts XYZ to Oklab (perceptually uniform, better than Lab for graphics).

**Output ranges:**
- L: [0, 1] (normalized lightness)
- a, b: [-0.5, 0.5] typical range

**Advantages over Lab:**
- Better hue linearity
- Better chroma uniformity
- Simpler math (no cube root approximations)

**Use case:** Modern color grading, UI color manipulation

---

### alwan_oklab_to_xyz

```c
alwan_result alwan_oklab_to_xyz(
    alwan_vec3 *xyz,              // Output first
    const alwan_vec3 *oklab,
    size_t count,
    size_t oklab_stride,
    size_t xyz_stride
);
```

Converts Oklab back to XYZ.

---

### alwan_oklab_to_oklch

```c
alwan_result alwan_oklab_to_oklch(
    alwan_vec3 *oklch,            // Output first
    const alwan_vec3 *oklab,
    size_t count,
    size_t oklab_stride,
    size_t oklch_stride
);
```

Converts Oklab to Oklch (cylindrical Oklab).

**Output format:**
- L: [0, 1]
- C: [0, ~0.4] (chroma)
- h: [0, 360) (hue in degrees)

---

### alwan_xyz_to_jzazbz

```c
alwan_result alwan_xyz_to_jzazbz(
    alwan_vec3 *jzazbz,           // Output first
    const alwan_vec3 *xyz,
    size_t count,
    size_t xyz_stride,
    size_t jzazbz_stride
);
```

Converts XYZ to JzAzBz (perceptually uniform for HDR).

**Use case:** HDR color difference calculations, tone mapping

**Output ranges:**
- Jz: [0, ~0.17] for SDR, higher for HDR
- Az, Bz: [-0.5, 0.5] typical

---

### alwan_xyz_to_ipt

```c
alwan_result alwan_xyz_to_ipt(
    alwan_vec3 *ipt,              // Output first
    const alwan_vec3 *xyz,
    size_t count,
    size_t xyz_stride,
    size_t ipt_stride
);
```

Converts XYZ to IPT (image processing transform).

**Use case:** Hue-preserving image manipulation

---

### alwan_xyz_to_ictcp

```c
alwan_result alwan_xyz_to_ictcp(
    alwan_vec3 *ictcp,            // Output first
    const alwan_vec3 *xyz,
    const char *transfer,  // "pq" or "hlg"
    size_t count,
    size_t xyz_stride,
    size_t ictcp_stride
);
```

Converts XYZ to ICtCp (HDR color space used in BT.2100).

**Parameters:**
- `transfer` — Transfer function: `"pq"` (ST.2084) or `"hlg"` (Hybrid Log-Gamma)

**Use case:** HDR video processing, broadcast

---

## RGB Conversions

### alwan_rgb_to_xyz

```c
alwan_result alwan_rgb_to_xyz(
    alwan_vec3 *xyz,              // Output first
    const alwan_vec3 *rgb,
    size_t count,
    size_t rgb_stride,
    size_t xyz_stride
);
```

Converts linear RGB to XYZ using the current RGB space (typically sRGB).

**Input:** Linear RGB [0, 1] for SDR, [0, ∞) for HDR

**Note:** Assumes sRGB primaries by default. For other spaces, use `alwan_rgb_convert()` or derive custom matrices.

---

### alwan_xyz_to_rgb

```c
alwan_result alwan_xyz_to_rgb(
    alwan_vec3 *rgb,              // Output first
    const alwan_vec3 *xyz,
    size_t count,
    size_t xyz_stride,
    size_t rgb_stride
);
```

Converts XYZ to linear RGB.

---

### alwan_rgb_convert

```c
alwan_result alwan_rgb_convert(
    alwan_vec3 *rgb_out,          // Output first
    alwan_ctx *ctx,
    const char *src_space,
    const char *dst_space,
    const alwan_vec3 *rgb_in,
    size_t count,
    size_t in_stride,
    size_t out_stride
);
```

Converts between different RGB color spaces.

**Parameters:**
- `rgb_out` — Output RGB values (linear)
- `ctx` — Library context
- `src_space` — Source RGB space name (e.g., `"srgb"`, `"bt2020"`)
- `dst_space` — Destination RGB space name
- `rgb_in` — Input RGB values (linear)
- `count` — Number of colors
- `in_stride` — Input stride in bytes
- `out_stride` — Output stride in bytes

**Supported spaces:**
- `"srgb"` — sRGB / IEC 61966-2-1
- `"bt709"` — ITU-R BT.709 (HDTV)
- `"bt2020"` — ITU-R BT.2020 (UHDTV)
- `"display_p3"` — Apple Display P3
- `"dci_p3"` — DCI-P3 (digital cinema)
- `"adobe_rgb"` — Adobe RGB (1998)
- `"prophoto_rgb"` — ProPhoto RGB
- `"aces2065_1"` — ACES 2065-1 (AP0)
- `"acescg"` — ACEScg (AP1)
- Many more (see RGB spaces reference)

**Example:**
```c
alwan_vec3 srgb[100];
alwan_vec3 bt2020[100];
alwan_rgb_convert(bt2020, ctx, "srgb", "bt2020", srgb, 100, 12, 12);
```

---

## Encoding Spaces

### alwan_rgb_to_hsv

```c
alwan_result alwan_rgb_to_hsv(
    alwan_vec3 *hsv,              // Output first
    const alwan_vec3 *rgb,
    size_t count,
    size_t rgb_stride,
    size_t hsv_stride
);
```

Converts RGB to HSV (Hue, Saturation, Value).

**Input:** RGB [0, 1]

**Output:**
- H: [0, 360) degrees
- S: [0, 1]
- V: [0, 1]

**Use case:** Color picking, image adjustment

---

### alwan_hsv_to_rgb

```c
alwan_result alwan_hsv_to_rgb(
    alwan_vec3 *rgb,              // Output first
    const alwan_vec3 *hsv,
    size_t count,
    size_t hsv_stride,
    size_t rgb_stride
);
```

Converts HSV back to RGB.

---

### alwan_rgb_to_hsl

```c
alwan_result alwan_rgb_to_hsl(
    alwan_vec3 *hsl,              // Output first
    const alwan_vec3 *rgb,
    size_t count,
    size_t rgb_stride,
    size_t hsl_stride
);
```

Converts RGB to HSL (Hue, Saturation, Lightness).

**Output:**
- H: [0, 360) degrees
- S: [0, 1]
- L: [0, 1]

---

### alwan_rgb_to_ycbcr

```c
alwan_result alwan_rgb_to_ycbcr(
    alwan_vec3 *ycbcr,            // Output first
    const char *standard,  // "bt601", "bt709", or "bt2020"
    const alwan_vec3 *rgb,
    size_t count,
    size_t rgb_stride,
    size_t ycbcr_stride
);
```

Converts RGB to YCbCr.

**Parameters:**
- `standard` — Video standard: `"bt601"`, `"bt709"`, or `"bt2020"`

**Output ranges:**
- Y: [0, 1]
- Cb, Cr: [-0.5, 0.5]

**Use case:** Video compression, broadcast

---

## Bulk Operations

All conversion functions support efficient bulk processing:

```c
// Convert 1 million colors (output first)
alwan_vec3 *xyz = malloc(1000000 * sizeof(alwan_vec3));
alwan_vec3 *lab = malloc(1000000 * sizeof(alwan_vec3));

alwan_xyz_to_lab(lab, xyz, &alwan_d65_xyz, 1000000,
                 sizeof(alwan_vec3), sizeof(alwan_vec3));
```

**Performance:** Bulk operations are optimized and may use SIMD on supported platforms.

---

## Stride Usage

Process interleaved or non-contiguous data:

```c
// RGBA data: [R,G,B,A, R,G,B,A, ...]
float rgba[1000 * 4];

// Convert only RGB, skip alpha (output first)
alwan_vec3 *rgb = (alwan_vec3*)rgba;
alwan_vec3 *lab = malloc(1000 * sizeof(alwan_vec3));

alwan_xyz_to_lab(lab, rgb, &alwan_d65_xyz, 1000,
                 4 * sizeof(float),    // Input: skip alpha
                 sizeof(alwan_vec3));  // Output: packed
```

---

## Error Codes

- `ALWAN_SUCCESS` — Conversion successful
- `ALWAN_ERROR_INVALID_PARAMETER` — NULL pointer or invalid count
- `ALWAN_ERROR_NOT_FOUND` — RGB space name not found (rgb_convert)
- `ALWAN_ERROR_OUT_OF_RANGE` — Input values outside valid domain (rare)

---

## Precision & Limits

### Numerical Accuracy

| Conversion | Float Error | Double Error |
|-----------|-------------|--------------|
| XYZ ↔ Lab | ±0.001 | ±1e-10 |
| XYZ ↔ Luv | ±0.001 | ±1e-10 |
| Lab ↔ LCh | ±0.01° | ±1e-8° |
| RGB ↔ XYZ | ±0.0001 | ±1e-12 |
| RGB ↔ HSV | ±0.1° | ±1e-6° |

### Special Cases

**Black (0, 0, 0):**
- Lab: (0, 0, 0)
- LCh: (0, 0, undefined hue)
- HSV: (undefined, 0, 0)

**Achromatic colors:**
- LCh hue: Undefined (preserved as 0)
- HSV hue: Undefined (preserved as 0)

---

## See Also

- [Chromatic Adaptation](chromatic-adaptation.md) — White point transforms
- [Transfer Functions](transfer-functions.md) — Encoding/decoding
- [Precision & Limits](../precision-and-limits.md) — Numerical details
- [Examples](../examples.md) — Usage examples
