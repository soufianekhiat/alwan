# Color Space Conversions API

Functions for converting between different color spaces and models.

---

## Overview

Alwan supports conversions between:
- **CIE spaces:** XYZ, xyY, Lab, Luv, LCh(ab), LCh(uv)
- **Perceptual models:** IPT, ICtCp, JzAzBz, Oklab, Oklch
- **RGB families:** sRGB, Adobe RGB, BT.709, BT.2020, Display P3, ACES, and more
- **Encoding spaces:** HSV, HSL, HSP, HSPLog, HSY, HWB, YCbCr, YCoCg, CMY, CMYK, HCL, IHLS
- **Relative luminance:** Multi-standard Y calculation from linear RGB

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

**Output format:** L: [0, 100], C: [0, inf), h: [0, 360) degrees

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
int alwan_rgb_to_hsv(alwan_hsv *hsv_out, alwan_rgb const *rgb);
int alwan_hsv_to_rgb(alwan_rgb *rgb_out, alwan_hsv const *hsv);
```

Operates on encoded (display-referred) sRGB values in [0, 1].

**Output:** H: [0, 1] (normalized, multiply by 360 for degrees), S: [0, 1], V: [0, 1]

---

### alwan_rgb_to_hsl / alwan_hsl_to_rgb

```c
int alwan_rgb_to_hsl(alwan_hsl *hsl_out, alwan_rgb const *rgb);
int alwan_hsl_to_rgb(alwan_rgb *rgb_out, alwan_hsl const *hsl);
```

Operates on encoded (display-referred) sRGB values in [0, 1].

**Output:** H: [0, 1] (normalized), S: [0, 1], L: [0, 1]

---

### alwan_linear_srgb_to_hsv / alwan_hsv_to_linear_srgb

```c
int alwan_linear_srgb_to_hsv(alwan_hsv *hsv_out, alwan_rgb const *rgb);
int alwan_hsv_to_linear_srgb(alwan_rgb *rgb_out, alwan_hsv const *hsv);
```

Applies sRGB OETF/EOTF internally so the caller works in linear light.

---

### alwan_linear_srgb_to_hsl / alwan_hsl_to_linear_srgb

```c
int alwan_linear_srgb_to_hsl(alwan_hsl *hsl_out, alwan_rgb const *rgb);
int alwan_hsl_to_linear_srgb(alwan_rgb *rgb_out, alwan_hsl const *hsl);
```

Applies sRGB OETF/EOTF internally so the caller works in linear light.

---

### alwan_rgb_to_hsp / alwan_hsp_to_rgb

```c
int alwan_rgb_to_hsp(alwan_hsp *hsp_out, alwan_rgb const *rgb);
int alwan_hsp_to_rgb(alwan_rgb *rgb_out, alwan_hsp const *hsp);
```

HSP: Hue, Saturation, Perceived brightness. P = sqrt(Pr*R^2 + Pg*G^2 + Pb*B^2) with BT.601 weights (0.299, 0.587, 0.114). H and S are identical to HSV. Used by DaVinci Resolve.

**Reference:** Darel Rex Finley (2006), http://alienryderflex.com/hsp.html

**Output:** H: [0, 1] (normalized), S: [0, 1], P: [0, 1]

---

### alwan_rgb_to_hsplog / alwan_hsplog_to_rgb

```c
int alwan_rgb_to_hsplog(alwan_hsplog *hsplog_out, alwan_rgb const *rgb);
int alwan_hsplog_to_rgb(alwan_rgb *rgb_out, alwan_hsplog const *hsplog);
```

HSPLog: HSP with logarithmic saturation stretching. S_log = log10(1 + 9*S), mapping [0,1] to [0,1]. Expands low saturation values — designed for log/flat-encoded footage. H and P are identical to HSP.

**Inspired by:** Nobe Color Remap (Time in Pixels) / DaVinci Resolve "HSP Log"

**Output:** H: [0, 1] (normalized), S: [0, 1] (log-stretched), P: [0, 1]

**Note:** No published specification exists. The formula used here is a reasonable interpretation of "logarithmic saturation stretching of HSP." The actual DaVinci Resolve implementation is proprietary (Blackmagic Design).

---

### alwan_rgb_to_hsy / alwan_hsy_to_rgb

```c
int alwan_rgb_to_hsy(alwan_hsy *hsy_out, alwan_rgb const *rgb);
int alwan_hsy_to_rgb(alwan_rgb *rgb_out, alwan_hsy const *hsy);
```

HSY: Hue, Saturation, Luma. Y = BT.601 weighted luma. S uses luma-aware max_sat remapping per hue sector. Used by DaVinci Resolve.

**Reference:** Kuzma Shapran "HCY" (chilliant.com); Krita KoColorConversions.cpp

**Output:** H: [0, 1] (normalized), S: [0, 1], Y: [0, 1]

---

### alwan_rgb_to_hwb / alwan_hwb_to_rgb

```c
int alwan_rgb_to_hwb(alwan_scalar *hwb_out, alwan_rgb const *rgb);
int alwan_hwb_to_rgb(alwan_rgb *rgb_out, alwan_scalar const *hwb_in);
```

HWB: Hue, Whiteness, Blackness (CSS Color Level 4). Derived from HSV.

**Output:** H: [0, 1] (normalized), W: [0, 1], B: [0, 1]

---

### alwan_rgb_to_ycbcr / alwan_ycbcr_to_rgb

```c
int alwan_rgb_to_ycbcr(alwan_ycbcr *ycbcr_out, alwan_rgb const *rgb, alwan_ycbcr_standard standard);
int alwan_ycbcr_to_rgb(alwan_rgb *rgb_out, alwan_ycbcr const *ycbcr, alwan_ycbcr_standard standard);
```

**Standards:** `ALWAN_YCBCR_BT601`, `ALWAN_YCBCR_BT709`, `ALWAN_YCBCR_BT2020`

---

### alwan_rgb_to_ycocg / alwan_ycocg_to_rgb

```c
int alwan_rgb_to_ycocg(alwan_ycocg *ycocg_out, alwan_rgb const *rgb);
int alwan_ycocg_to_rgb(alwan_rgb *rgb_out, alwan_ycocg const *ycocg);
```

Reversible integer transform used in H.264/AVC and video codecs.

---

### alwan_rgb_to_cmy / alwan_cmy_to_rgb

```c
int alwan_rgb_to_cmy(alwan_cmy *cmy_out, alwan_rgb const *rgb);
int alwan_cmy_to_rgb(alwan_rgb *rgb_out, alwan_cmy const *cmy);
```

---

## Relative Luminance

Computes Y = kr*R + kg*G + kb*B for a given standard or color space. Input RGB must be linear (scene-referred).

### alwan_relative_luminance

```c
int alwan_relative_luminance(alwan_scalar *Y_out, alwan_rgb const *rgb, alwan_luma_standard standard);
```

**Standards:**
```c
typedef enum {
    ALWAN_LUMA_BT601,       /* ITU-R BT.601 (SD) */
    ALWAN_LUMA_BT709,       /* ITU-R BT.709 / sRGB (HD) */
    ALWAN_LUMA_BT2020,      /* ITU-R BT.2020 (UHD) */
    ALWAN_LUMA_ACES_AP1,    /* ACES AP1 / ACEScg */
    ALWAN_LUMA_ACES_AP0,    /* ACES AP0 / ACES2065-1 */
    ALWAN_LUMA_DISPLAY_P3,  /* Display P3 / P3-D65 */
    ALWAN_LUMA_DCI_P3,      /* DCI-P3 (theater) */
    ALWAN_LUMA_ADOBE_RGB,   /* Adobe RGB (1998) */
    ALWAN_LUMA_PROPHOTO_RGB /* ProPhoto RGB / ROMM RGB */
} alwan_luma_standard;
```

### alwan_relative_luminance_kr_kb

```c
int alwan_relative_luminance_kr_kb(alwan_scalar *Y_out, alwan_rgb const *rgb,
                                   alwan_scalar kr, alwan_scalar kb);
```

Uses explicit kr/kb coefficients (kg = 1 - kr - kb).

### alwan_relative_luminance_space

```c
int alwan_relative_luminance_space(alwan_scalar *Y_out, alwan_rgb const *rgb,
                                   alwan_rgb_space_desc const *space);
```

Extracts the Y row from the RGB-to-XYZ normalized primary matrix, enabling luminance calculation for any of the 100+ supported RGB spaces.

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
