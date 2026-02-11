# Color Space Conversions API

Functions for converting between different color spaces and models.

---

## Overview

Alwan supports conversions between:
- **CIE spaces:** XYZ, xyY, Lab, Luv, LCh(ab), LCh(uv)
- **Perceptual models:** IPT, ICtCp, JzAzBz, Oklab, Oklch
- **RGB families:** sRGB, Adobe RGB, BT.709, BT.2020, Display P3, ACES, and more
- **Encoding spaces:** HSV, HSL, YCbCr, CMY, CMYK, HCL, IHLS

**API Pattern:** Single-element functions use semantic types. Bulk functions use `alwan_scalar*` with strides.

---

## CIE XYZ Conversions

### alwan_xyz_to_lab / alwan_xyz_to_lab_bulk

```c
// Single element
void alwan_xyz_to_lab(alwan_lab *lab, alwan_xyz const *xyz, alwan_xyz const *white_xyz);

// Bulk with strides (in bytes)
int alwan_xyz_to_lab_bulk(alwan_scalar *lab_out, alwan_scalar const *xyz_in,
                          alwan_xyz const *white_xyz, size_t count,
                          size_t in_stride, size_t out_stride);
```

**Example:**
```c
alwan_xyz xyz = {0.5, 0.6, 0.4};
alwan_xyz d65 = {0.95047, 1.0, 1.08883};
alwan_lab lab;
alwan_xyz_to_lab(&lab, &xyz, &d65);
printf("Lab: L=%.2f, a=%.2f, b=%.2f\n", lab.L, lab.a, lab.b);
```

---

### alwan_lab_to_xyz / alwan_lab_to_xyz_bulk

```c
void alwan_lab_to_xyz(alwan_xyz *xyz, alwan_lab const *lab, alwan_xyz const *white_xyz);
int alwan_lab_to_xyz_bulk(alwan_scalar *xyz_out, alwan_scalar const *lab_in,
                          alwan_xyz const *white_xyz, size_t count,
                          size_t in_stride, size_t out_stride);
```

---

### alwan_xyz_to_luv / alwan_xyz_to_luv_bulk

```c
void alwan_xyz_to_luv(alwan_luv *luv, alwan_xyz const *xyz, alwan_xyz const *white_xyz);
int alwan_xyz_to_luv_bulk(alwan_scalar *luv_out, alwan_scalar const *xyz_in,
                          alwan_xyz const *white_xyz, size_t count,
                          size_t in_stride, size_t out_stride);
```

---

### alwan_luv_to_xyz / alwan_luv_to_xyz_bulk

```c
void alwan_luv_to_xyz(alwan_xyz *xyz, alwan_luv const *luv, alwan_xyz const *white_xyz);
int alwan_luv_to_xyz_bulk(alwan_scalar *xyz_out, alwan_scalar const *luv_in,
                          alwan_xyz const *white_xyz, size_t count,
                          size_t in_stride, size_t out_stride);
```

---

### alwan_xyz_to_xyy / alwan_xyy_to_xyz

```c
void alwan_xyz_to_xyy(alwan_xyy *xyy, alwan_xyz const *xyz);
void alwan_xyy_to_xyz(alwan_xyz *xyz, alwan_xyy const *xyy);
```

---

## Cylindrical Representations

### alwan_lab_to_lch / alwan_lch_to_lab

```c
void alwan_lab_to_lch(alwan_lch *lch, alwan_lab const *lab);
void alwan_lch_to_lab(alwan_lab *lab, alwan_lch const *lch);

// Bulk versions
int alwan_lab_to_lch_bulk(alwan_scalar *lch_out, alwan_scalar const *lab_in,
                          size_t count, size_t in_stride, size_t out_stride);
int alwan_lch_to_lab_bulk(alwan_scalar *lab_out, alwan_scalar const *lch_in,
                          size_t count, size_t in_stride, size_t out_stride);
```

**Output format:** L: [0, 100], C: [0, ∞), h: [0, 360) degrees

---

### alwan_luv_to_lchuv / alwan_lchuv_to_luv

```c
void alwan_luv_to_lchuv(alwan_lchuv *lchuv, alwan_luv const *luv);
void alwan_lchuv_to_luv(alwan_luv *luv, alwan_lchuv const *lchuv);

// Bulk versions available
```

---

## Modern Perceptual Models

### alwan_xyz_to_oklab / alwan_oklab_to_xyz

```c
void alwan_xyz_to_oklab(alwan_oklab *oklab, alwan_xyz const *xyz);
void alwan_oklab_to_xyz(alwan_xyz *xyz, alwan_oklab const *oklab);

// Bulk versions
int alwan_xyz_to_oklab_bulk(alwan_scalar *oklab_out, alwan_scalar const *xyz_in,
                            size_t count, size_t in_stride, size_t out_stride);
```

**Advantages over Lab:** Better hue linearity, better chroma uniformity.

---

### alwan_oklab_to_oklch / alwan_oklch_to_oklab

```c
void alwan_oklab_to_oklch(alwan_oklch *oklch, alwan_oklab const *oklab);
void alwan_oklch_to_oklab(alwan_oklab *oklab, alwan_oklch const *oklch);
```

---

### alwan_xyz_to_jzazbz / alwan_jzazbz_to_xyz

```c
void alwan_xyz_to_jzazbz(alwan_jzazbz *jzazbz, alwan_xyz const *xyz);
void alwan_jzazbz_to_xyz(alwan_xyz *xyz, alwan_jzazbz const *jzazbz);
```

**Use case:** HDR color difference calculations, tone mapping.

---

### alwan_xyz_to_ipt / alwan_ipt_to_xyz

```c
void alwan_xyz_to_ipt(alwan_ipt *ipt, alwan_xyz const *xyz);
void alwan_ipt_to_xyz(alwan_xyz *xyz, alwan_ipt const *ipt);
```

---

### alwan_xyz_to_ictcp / alwan_ictcp_to_xyz

```c
void alwan_xyz_to_ictcp(alwan_ictcp *ictcp, alwan_xyz const *xyz,
                        alwan_transfer_function tf);  // ALWAN_TF_PQ or ALWAN_TF_HLG
void alwan_ictcp_to_xyz(alwan_xyz *xyz, alwan_ictcp const *ictcp,
                        alwan_transfer_function tf);
```

---

## RGB Conversions

### alwan_rgb_to_xyz / alwan_xyz_to_rgb

```c
int alwan_rgb_to_xyz(alwan_xyz *xyz, alwan_rgb_space_desc const *space, alwan_rgb const *rgb);
int alwan_xyz_to_rgb(alwan_rgb *rgb, alwan_rgb_space_desc const *space, alwan_xyz const *xyz);
```

---

### alwan_rgb_convert

```c
int alwan_rgb_convert(alwan_rgb *dst_rgb, alwan_ctx *ctx,
                      alwan_rgb_space_desc const *src_space,
                      alwan_rgb_space_desc const *dst_space,
                      alwan_rgb const *src_rgb);

// Bulk version
int alwan_rgb_convert_bulk(alwan_rgb *dst_rgb, alwan_ctx *ctx,
                           alwan_rgb_space_desc const *src_space,
                           alwan_rgb_space_desc const *dst_space,
                           alwan_rgb const *src_rgb, size_t count);
```

**Example:**
```c
alwan_rgb_space_desc srgb_desc, bt2020_desc;
alwan_rgb_get_space_descriptor(&srgb_desc, ctx, ALWAN_RGB_SPACE_SRGB);
alwan_rgb_get_space_descriptor(&bt2020_desc, ctx, ALWAN_RGB_SPACE_BT2020);

alwan_rgb rgb_in = {0.8, 0.3, 0.2};
alwan_rgb rgb_out;
alwan_rgb_convert(&rgb_out, ctx, &srgb_desc, &bt2020_desc, &rgb_in);
```

---

## sRGB Convenience Functions

Direct conversions for sRGB (D65 white point):

```c
int alwan_srgb_to_xyz(alwan_xyz *xyz, alwan_rgb const *rgb);
int alwan_xyz_to_srgb(alwan_rgb *rgb, alwan_xyz const *xyz);
int alwan_srgb_to_lab(alwan_lab *lab, alwan_rgb const *rgb);
int alwan_lab_to_srgb(alwan_rgb *rgb, alwan_lab const *lab);
int alwan_srgb_to_oklab(alwan_oklab *oklab, alwan_rgb const *rgb);
int alwan_oklab_to_srgb(alwan_rgb *rgb, alwan_oklab const *oklab);

// Bulk versions with strides
int alwan_srgb_to_xyz_bulk(alwan_scalar *xyz_out, alwan_scalar const *rgb_in,
                           size_t count, size_t in_stride, size_t out_stride);
```

---

## Encoding Spaces

### alwan_rgb_to_hsv / alwan_hsv_to_rgb

```c
void alwan_rgb_to_hsv(alwan_hsv *hsv, alwan_rgb const *rgb);
void alwan_hsv_to_rgb(alwan_rgb *rgb, alwan_hsv const *hsv);
```

**Output:** H: [0, 360), S: [0, 1], V: [0, 1]

---

### alwan_rgb_to_hsl / alwan_hsl_to_rgb

```c
void alwan_rgb_to_hsl(alwan_hsl *hsl, alwan_rgb const *rgb);
void alwan_hsl_to_rgb(alwan_rgb *rgb, alwan_hsl const *hsl);
```

---

### alwan_rgb_to_ycbcr / alwan_ycbcr_to_rgb

```c
void alwan_rgb_to_ycbcr(alwan_ycbcr *ycbcr, alwan_rgb const *rgb, int standard);
void alwan_ycbcr_to_rgb(alwan_rgb *rgb, alwan_ycbcr const *ycbcr, int standard);
```

**Standards:** 0=BT.601, 1=BT.709, 2=BT.2020

---

## Bulk Operations

Always prefer bulk operations over single-element loops:

```c
alwan_xyz d65 = {0.95047, 1.0, 1.08883};

// Slow: single-element loop
for (int i = 0; i < 1000; i++) {
    alwan_xyz_to_lab(&lab[i], &xyz[i], &d65);
}

// Fast: bulk with strides (in bytes)
alwan_xyz_to_lab_bulk((alwan_scalar*)lab, (alwan_scalar*)xyz, &d65,
                      1000, sizeof(alwan_xyz), sizeof(alwan_lab));
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — Invalid parameter
- `ALWAN_E_NODATA` (-2) — Data not found
- `ALWAN_E_RANGE` (-3) — Value out of range
- `ALWAN_E_NOMEM` (-4) — Allocation failed
- `ALWAN_E_DIVZERO` (-5) — Division by zero

---

## See Also

- [Chromatic Adaptation](chromatic-adaptation.md) — White point transforms
- [Transfer Functions](transfer-functions.md) — Encoding/decoding
- [Examples](../examples.md) — Usage examples
