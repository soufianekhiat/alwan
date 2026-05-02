# Color Space Conversions API

Functions for converting between different color spaces and models.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

---

## Overview

Alwan supports conversions between:
- **CIE spaces:** XYZ, xyY, Lab, Luv, LCh(ab), LCh(uv), UVW
- **Perceptual models:** IPT, ICtCp, JzAzBz, Oklab, Oklch, Hunter Lab, ProLab, OSA-UCS, DIN99, hdr-CIELAB, hdr-IPT, IgPgTg, ICaCb
- **RGB families:** sRGB, Adobe RGB, BT.709, BT.2020, Display P3, ACES, and 100+ more
- **Encoding spaces:** HSV, HSL, HSP, HSPLog, HSY, HWB, YCbCr, YCoCg, YcCbcCrc, CMY, CMYK, HCL, HLC, IHLS, Prismatic, CubeHelix, HSLuv, HPLuv, OkHSL, OkHSV
- **Relative luminance:** Multi-standard Y calculation from linear RGB

**API Pattern:**
- Single-element: `alwan_foo_{T}(out, in, ...)` — typed structs, no stride.
- Bulk interleaved: `alwan_foo_{T}_map_interleave(alwan_{T} *out, alwan_{T} const *in, ..., count, in_stride, out_stride)` — raw scalar arrays, strides in bytes.
- Bulk planar: `alwan_foo_{T}_map_planar(T *ch0, T *ch1, T *ch2, ...)` — separate channel arrays.

---

## CIE XYZ Conversions

### alwan_xyz_to_lab_{T} / alwan_lab_to_xyz_{T}

```c
// Single element
void alwan_xyz_to_lab_{T}(alwan_lab_{T} *lab,
                           alwan_xyz_{T} const *xyz,
                           alwan_xyz_{T} const *white_xyz);

// Bulk interleaved (strides in bytes)
int alwan_xyz_to_lab_{T}_map_interleave(alwan_{T} *lab_out,
                                         alwan_{T} const *xyz_in,
                                         alwan_xyz_{T} const *white_xyz,
                                         size_t count,
                                         size_t in_stride,
                                         size_t out_stride);

// Bulk planar
int alwan_xyz_to_lab_{T}_map_planar(alwan_{T} *out_ch0, alwan_{T} *out_ch1, alwan_{T} *out_ch2,
                                     alwan_{T} const *in_ch0, alwan_{T} const *in_ch1,
                                     alwan_{T} const *in_ch2,
                                     alwan_xyz_{T} const *white_xyz,
                                     size_t count, size_t in_stride, size_t out_stride);
```

Inverse: `alwan_lab_to_xyz_{T}` / `alwan_lab_to_xyz_{T}_map_interleave` / `alwan_lab_to_xyz_{T}_map_planar` — same pattern.

**Example:**
```c
alwan_xyz_{T} xyz = {0.5, 0.6, 0.4};
alwan_xyz_{T} d65 = {0.95047, 1.0, 1.08883};
alwan_lab_{T} lab;
alwan_xyz_to_lab_{T}(&lab, &xyz, &d65);
printf("Lab: L=%.2f, a=%.2f, b=%.2f\n", lab.L, lab.a, lab.b);
```

---

### alwan_xyz_to_luv_{T} / alwan_luv_to_xyz_{T}

```c
void alwan_xyz_to_luv_{T}(alwan_luv_{T} *luv,
                           alwan_xyz_{T} const *xyz,
                           alwan_xyz_{T} const *white_xyz);
```

`_map_interleave` and `_map_planar` bulk variants available.

---

### alwan_xyz_to_xyy_{T} / alwan_xyy_to_xyz_{T}

```c
void alwan_xyz_to_xyy_{T}(alwan_xyy_{T} *xyy, alwan_xyz_{T} const *xyz);
void alwan_xyy_to_xyz_{T}(alwan_xyz_{T} *xyz, alwan_xyy_{T} const *xyy);
```

---

## Cylindrical Representations

### alwan_lab_to_lch_{T} / alwan_lch_to_lab_{T}

```c
void alwan_lab_to_lch_{T}(alwan_lch_{T} *lch, alwan_lab_{T} const *lab);
void alwan_lch_to_lab_{T}(alwan_lab_{T} *lab, alwan_lch_{T} const *lch);
```

`_map_interleave` bulk variants available.

**Output format:** L: [0, 100], C: [0, inf), h: [0, 360) degrees

---

### alwan_luv_to_lchuv_{T} / alwan_lchuv_to_luv_{T}

```c
void alwan_luv_to_lchuv_{T}(alwan_lchuv_{T} *lchuv, alwan_luv_{T} const *luv);
void alwan_lchuv_to_luv_{T}(alwan_luv_{T} *luv, alwan_lchuv_{T} const *lchuv);
```

`_map_interleave` bulk variants available.

---

## Modern Perceptual Models

### alwan_xyz_to_oklab_{T} / alwan_oklab_to_xyz_{T}

```c
void alwan_xyz_to_oklab_{T}(alwan_oklab_{T} *oklab, alwan_xyz_{T} const *xyz);
void alwan_oklab_to_xyz_{T}(alwan_xyz_{T} *xyz, alwan_oklab_{T} const *oklab);
```

`_map_interleave` and `_map_planar` bulk variants available.

**Advantages over Lab:** Better hue linearity, better chroma uniformity.

---

### alwan_oklab_to_oklch_{T} / alwan_oklch_to_oklab_{T}

```c
void alwan_oklab_to_oklch_{T}(alwan_oklch_{T} *oklch, alwan_oklab_{T} const *oklab);
void alwan_oklch_to_oklab_{T}(alwan_oklab_{T} *oklab, alwan_oklch_{T} const *oklch);
```

---

### alwan_xyz_to_jzazbz_{T} / alwan_jzazbz_to_xyz_{T}

```c
void alwan_xyz_to_jzazbz_{T}(alwan_jzazbz_{T} *jzazbz, alwan_xyz_{T} const *xyz);
void alwan_jzazbz_to_xyz_{T}(alwan_xyz_{T} *xyz, alwan_jzazbz_{T} const *jzazbz);
```

**Use case:** HDR color difference calculations, tone mapping.

---

### alwan_xyz_to_ipt_{T} / alwan_ipt_to_xyz_{T}

```c
void alwan_xyz_to_ipt_{T}(alwan_ipt_{T} *ipt, alwan_xyz_{T} const *xyz);
void alwan_ipt_to_xyz_{T}(alwan_xyz_{T} *xyz, alwan_ipt_{T} const *ipt);
```

---

### alwan_xyz_to_ictcp_{T} / alwan_ictcp_to_xyz_{T}

```c
void alwan_xyz_to_ictcp_{T}(alwan_ictcp_{T} *ictcp, alwan_xyz_{T} const *xyz,
                              alwan_transfer_function tf);  /* ALWAN_TF_PQ or ALWAN_TF_HLG */
void alwan_ictcp_to_xyz_{T}(alwan_xyz_{T} *xyz, alwan_ictcp_{T} const *ictcp,
                              alwan_transfer_function tf);
```

---

## RGB Conversions

### alwan_rgb_to_xyz_{T} / alwan_xyz_to_rgb_{T}

```c
int alwan_rgb_to_xyz_{T}(alwan_xyz_{T} *xyz,
                          alwan_rgb_space_desc_{T} const *space,
                          alwan_rgb_{T} const *rgb);
int alwan_xyz_to_rgb_{T}(alwan_rgb_{T} *rgb,
                          alwan_rgb_space_desc_{T} const *space,
                          alwan_xyz_{T} const *xyz);
```

---

### alwan_rgb_convert_{T}

```c
int alwan_rgb_convert_{T}(alwan_rgb_{T} *dst_rgb, alwan_ctx *ctx,
                           alwan_rgb_space_desc_{T} const *src_space,
                           alwan_rgb_space_desc_{T} const *dst_space,
                           alwan_rgb_{T} const *src_rgb);
```

**Example:**
```c
alwan_rgb_space_desc_{T} srgb_desc, bt2020_desc;
alwan_rgb_get_space_descriptor_{T}(&srgb_desc, ctx, ALWAN_RGB_SPACE_SRGB);
alwan_rgb_get_space_descriptor_{T}(&bt2020_desc, ctx, ALWAN_RGB_SPACE_BT2020);

alwan_rgb_{T} rgb_in = {0.8, 0.3, 0.2};
alwan_rgb_{T} rgb_out;
alwan_rgb_convert_{T}(&rgb_out, ctx, &srgb_desc, &bt2020_desc, &rgb_in);
```

---

## sRGB Convenience Functions

Direct conversions assuming sRGB primaries and D65 white point:

```c
int alwan_srgb_to_xyz_{T}(alwan_xyz_{T} *xyz, alwan_rgb_{T} const *rgb);
int alwan_xyz_to_srgb_{T}(alwan_rgb_{T} *rgb, alwan_xyz_{T} const *xyz);
int alwan_srgb_to_lab_{T}(alwan_lab_{T} *lab, alwan_rgb_{T} const *rgb);
int alwan_lab_to_srgb_{T}(alwan_rgb_{T} *rgb, alwan_lab_{T} const *lab);
int alwan_srgb_to_oklab_{T}(alwan_oklab_{T} *oklab, alwan_rgb_{T} const *rgb);
int alwan_oklab_to_srgb_{T}(alwan_rgb_{T} *rgb, alwan_oklab_{T} const *oklab);
```

Bulk interleaved variants (`_map_interleave`) available for all of the above.

---

## Encoding Spaces

### alwan_rgb_to_hsv_{T} / alwan_hsv_to_rgb_{T}

```c
int alwan_rgb_to_hsv_{T}(alwan_hsv_{T} *hsv_out, alwan_rgb_{T} const *rgb);
int alwan_hsv_to_rgb_{T}(alwan_rgb_{T} *rgb_out, alwan_hsv_{T} const *hsv);
```

Operates on encoded (display-referred) sRGB values in [0, 1].

**Output:** H: [0, 1] (normalized, multiply by 360 for degrees), S: [0, 1], V: [0, 1]

---

### alwan_rgb_to_hsl_{T} / alwan_hsl_to_rgb_{T}

```c
int alwan_rgb_to_hsl_{T}(alwan_hsl_{T} *hsl_out, alwan_rgb_{T} const *rgb);
int alwan_hsl_to_rgb_{T}(alwan_rgb_{T} *rgb_out, alwan_hsl_{T} const *hsl);
```

**Output:** H: [0, 1] (normalized), S: [0, 1], L: [0, 1]

---

### alwan_rgb_to_hsp_{T} / alwan_hsp_to_rgb_{T}

```c
int alwan_rgb_to_hsp_{T}(alwan_hsp_{T} *hsp_out, alwan_rgb_{T} const *rgb);
int alwan_hsp_to_rgb_{T}(alwan_rgb_{T} *rgb_out, alwan_hsp_{T} const *hsp);
```

HSP: Hue, Saturation, Perceived brightness. P = sqrt(Pr*R^2 + Pg*G^2 + Pb*B^2) with BT.601 weights. H and S are identical to HSV.

**Reference:** Darel Rex Finley (2006), http://alienryderflex.com/hsp.html

**Output:** H: [0, 1] (normalized), S: [0, 1], P: [0, 1]

---

### alwan_linear_srgb_to_hsv_{T} / alwan_hsv_to_linear_srgb_{T}

```c
int alwan_linear_srgb_to_hsv_{T}(alwan_hsv_{T} *hsv_out, alwan_rgb_{T} const *rgb);
int alwan_hsv_to_linear_srgb_{T}(alwan_rgb_{T} *rgb_out, alwan_hsv_{T} const *hsv);
```

Applies sRGB OETF/EOTF internally so the caller works in linear light.

`alwan_linear_srgb_to_hsl_{T}` / `alwan_hsl_to_linear_srgb_{T}` follow the same pattern.

---

## See Also

- [Transfer Functions](transfer-functions.md) — OETF/EOTF/view transforms
- [Chromatic Adaptation](chromatic-adaptation.md) — White point transforms
- [Color Difference](color-difference.md) — dE metrics
