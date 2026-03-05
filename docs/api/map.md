# Map API Reference

Batch pixel processing. Applies a color transform to an array of pixels in a single call with optional SIMD acceleration.

## Overview

Every per-pixel transform (`alwan_xyz_to_lab`, etc.) has two batch variants:

| Suffix | Input/Output type | Format conversion |
|--------|-------------------|-------------------|
| `_map` | `alwan_scalar *` (float or double) | None |
| `_map_ex` | `void *` with `alwan_pixel_format` | Automatic U8/U16/F32/F64 |

Both use tiled processing with compile-time SIMD where available. See [simd.md](simd.md) for the underlying SIMD layer.

## Common Parameters

All `_map` functions share the same trailing parameters:

```c
int alwan_*_map(alwan_scalar       *out,
                alwan_scalar const *in,
                size_t              count,      // number of pixels
                size_t              in_stride,  // bytes between input pixels
                size_t              out_stride); // bytes between output pixels
```

**`count`** -- number of 3-channel pixels to process.

**`in_stride` / `out_stride`** -- byte offset between consecutive pixels. For tightly packed `alwan_scalar` triplets use `3 * sizeof(alwan_scalar)`. Larger strides allow processing interleaved RGBA (stride 4 * sizeof) or sparse layouts.

**Return value** -- `ALWAN_OK` (0) on success, `ALWAN_E_INVALID` on NULL pointers or bad parameters.

## Pixel Formats (`_map_ex`)

```c
typedef enum {
    ALWAN_PIXEL_U8  = 0,   // uint8_t  [0..255]   <-> [0.0, 1.0]
    ALWAN_PIXEL_U16 = 1,   // uint16_t [0..65535]  <-> [0.0, 1.0]
    ALWAN_PIXEL_F32 = 2,   // float
    ALWAN_PIXEL_F64 = 3    // double
} alwan_pixel_format;
```

`_map_ex` functions accept `void *` buffers and convert at the boundaries:

```c
int alwan_*_map_ex(void              *out, alwan_pixel_format out_fmt,
                   void const        *in,  alwan_pixel_format in_fmt,
                   size_t             count,
                   size_t             in_stride,
                   size_t             out_stride);
```

Integer formats are normalized: U8 divides by 255, U16 by 65535 on input; multiplied and clamped on output.

## Usage Example

```c
// Convert 1920x1080 sRGB pixels to CIE Lab
size_t n = 1920 * 1080;
size_t stride = 3 * sizeof(alwan_scalar);
alwan_xyz white = { 0.95047, 1.0, 1.08883 }; // D65

alwan_srgb_to_xyz_map(xyz, srgb, n, stride, stride);
alwan_xyz_to_lab_map(lab, xyz, &white, n, stride, stride);

// Or directly with uint8 buffers (automatic conversion)
alwan_srgb_to_lab_map_ex(lab_f32, ALWAN_PIXEL_F32,
                          srgb_u8, ALWAN_PIXEL_U8,
                          n, 3, 12);
```

---

## Function Reference

### Matrix Transform

```c
int alwan_mat3_transform_map(alwan_scalar *out, alwan_mat3x3 const *matrix,
                              alwan_scalar const *in,
                              size_t count, size_t in_stride, size_t out_stride);

int alwan_mat3_transform_map_ex(void *out, alwan_pixel_format out_fmt,
                                 alwan_mat3x3 const *matrix,
                                 void const *in, alwan_pixel_format in_fmt,
                                 size_t count, size_t in_stride, size_t out_stride);
```

Applies a 3x3 matrix to each input triplet. SIMD-accelerated.

### sRGB Convenience

Composite transforms that chain sRGB EOTF/OETF with a color space conversion.

| Function | Transform |
|----------|-----------|
| `alwan_srgb_to_xyz_map` | sRGB -> linear -> XYZ (D65) |
| `alwan_xyz_to_srgb_map` | XYZ (D65) -> linear -> sRGB |
| `alwan_srgb_to_lab_map` | sRGB -> XYZ -> Lab (D65) |
| `alwan_lab_to_srgb_map` | Lab (D65) -> XYZ -> sRGB |
| `alwan_srgb_to_oklab_map` | sRGB -> linear -> Oklab |
| `alwan_oklab_to_srgb_map` | Oklab -> linear -> sRGB |

All have `_map_ex` variants. No extra parameters beyond the common set.

### CIE Colorspaces

Functions that take a **white point** parameter:

```c
int alwan_xyz_to_lab_map(alwan_scalar *out, alwan_scalar const *in,
                          alwan_xyz const *white_xyz,
                          size_t count, size_t in_stride, size_t out_stride);
```

| Function | Direction | White point |
|----------|-----------|-------------|
| `alwan_xyz_to_lab_map` | XYZ -> Lab | yes |
| `alwan_lab_to_xyz_map` | Lab -> XYZ | yes |
| `alwan_xyz_to_luv_map` | XYZ -> Luv | yes |
| `alwan_luv_to_xyz_map` | Luv -> XYZ | yes |
| `alwan_lab_to_lch_map` | Lab -> LCh(ab) | no |
| `alwan_lch_to_lab_map` | LCh(ab) -> Lab | no |
| `alwan_luv_to_lchuv_map` | Luv -> LCh(uv) | no |
| `alwan_lchuv_to_luv_map` | LCh(uv) -> Luv | no |
| `alwan_xyz_to_xyy_map` | XYZ -> xyY | no |
| `alwan_xyy_to_xyz_map` | xyY -> XYZ | no |

All have `_map_ex` variants. White-point variants pass `alwan_xyz const *white_xyz` after the input buffer.

### Oklab

| Function | Direction |
|----------|-----------|
| `alwan_xyz_to_oklab_map` | XYZ -> Oklab |
| `alwan_oklab_to_xyz_map` | Oklab -> XYZ |
| `alwan_oklab_to_oklch_map` | Oklab -> OkLCh |
| `alwan_oklch_to_oklab_map` | OkLCh -> Oklab |

All have `_map_ex` variants. No extra parameters.

### ICtCp

```c
int alwan_rgb_to_ictcp_map(alwan_scalar *out, alwan_scalar const *in,
                            int use_pq, // 1 = PQ (ST 2084), 0 = HLG
                            size_t count, size_t in_stride, size_t out_stride);
```

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_rgb_to_ictcp_map` | BT.2020 RGB -> ICtCp | `int use_pq` |
| `alwan_ictcp_to_rgb_map` | ICtCp -> BT.2020 RGB | `int use_pq` |
| `alwan_xyz_to_ictcp_map` | XYZ -> ICtCp | `int use_pq` |
| `alwan_ictcp_to_xyz_map` | ICtCp -> XYZ | `int use_pq` |

All have `_map_ex` variants.

### JzAzBz

| Function | Direction |
|----------|-----------|
| `alwan_xyz_to_jzazbz_map` | XYZ -> Jzazbz |
| `alwan_jzazbz_to_xyz_map` | Jzazbz -> XYZ |
| `alwan_jzazbz_to_jzczhz_map` | Jzazbz -> JzCzhz |
| `alwan_jzczhz_to_jzazbz_map` | JzCzhz -> Jzazbz |

All have `_map_ex` variants. No extra parameters.

### IPT

| Function | Direction |
|----------|-----------|
| `alwan_xyz_to_ipt_map` | XYZ -> IPT |
| `alwan_ipt_to_xyz_map` | IPT -> XYZ |

Both have `_map_ex` variants.

### Extended Colorspaces

No extra parameters:

| Function | Direction |
|----------|-----------|
| `alwan_xyz_to_igpgtg_map` | XYZ -> IgPgTg |
| `alwan_igpgtg_to_xyz_map` | IgPgTg -> XYZ |
| `alwan_xyz_to_icacb_map` | XYZ -> ICaCb |
| `alwan_icacb_to_xyz_map` | ICaCb -> XYZ |
| `alwan_xyz_to_hdr_cielab_map` | XYZ -> HDR-CIELAB |
| `alwan_hdr_cielab_to_xyz_map` | HDR-CIELAB -> XYZ |
| `alwan_xyz_to_hdr_ipt_map` | XYZ -> HDR-IPT |
| `alwan_hdr_ipt_to_xyz_map` | HDR-IPT -> XYZ |
| `alwan_xyz_to_ucs_map` | XYZ -> CIE 1960 UCS |
| `alwan_ucs_to_xyz_map` | UCS -> XYZ |
| `alwan_xyz_to_osa_ucs_map` | XYZ -> OSA-UCS |
| `alwan_osa_ucs_to_xyz_map` | OSA-UCS -> XYZ |
| `alwan_xyz_to_hunter_lab_map` | XYZ -> Hunter Lab (D65) |
| `alwan_hunter_lab_to_xyz_map` | Hunter Lab -> XYZ (D65) |
| `alwan_xyz_to_prolab_map` | XYZ -> ProLab (D65) |
| `alwan_prolab_to_xyz_map` | ProLab -> XYZ (D65) |

With custom white point (`alwan_xyz const *white_xyz`):

| Function | Direction |
|----------|-----------|
| `alwan_xyz_to_hunter_lab_custom_map` | XYZ -> Hunter Lab |
| `alwan_hunter_lab_to_xyz_custom_map` | Hunter Lab -> XYZ |
| `alwan_xyz_to_prolab_custom_map` | XYZ -> ProLab |
| `alwan_prolab_to_xyz_custom_map` | ProLab -> XYZ |
| `alwan_xyz_to_uvw_map` | XYZ -> U\*V\*W\* |
| `alwan_uvw_to_xyz_map` | U\*V\*W\* -> XYZ |

RGB-based (no extra parameters):

| Function | Direction |
|----------|-----------|
| `alwan_rgb_to_prismatic_map` | RGB -> Prismatic |
| `alwan_prismatic_to_rgb_map` | Prismatic -> RGB |
| `alwan_rgb_to_hcl_map` | RGB -> HCL |
| `alwan_hcl_to_rgb_map` | HCL -> RGB |
| `alwan_rgb_to_ihls_map` | RGB -> IHLS |
| `alwan_ihls_to_rgb_map` | IHLS -> RGB |

DIN99 (with `int variant`):

```c
int alwan_lab_to_din99_map(alwan_scalar *out, alwan_scalar const *in,
                            int variant,
                            size_t count, size_t in_stride, size_t out_stride);
```

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_lab_to_din99_map` | Lab -> DIN99 | `int variant` |
| `alwan_din99_to_lab_map` | DIN99 -> Lab | `int variant` |

All extended colorspace functions have `_map_ex` variants.

### Convenience (HSV, HSL, CMY, YCbCr, ...)

No extra parameters:

| Function | Direction |
|----------|-----------|
| `alwan_rgb_to_hsv_map` | RGB -> HSV |
| `alwan_hsv_to_rgb_map` | HSV -> RGB |
| `alwan_rgb_to_hsl_map` | RGB -> HSL |
| `alwan_hsl_to_rgb_map` | HSL -> RGB |
| `alwan_rgb_to_cmy_map` | RGB -> CMY |
| `alwan_cmy_to_rgb_map` | CMY -> RGB |
| `alwan_rgb_to_ycocg_map` | RGB -> YCoCg |
| `alwan_ycocg_to_rgb_map` | YCoCg -> RGB |
| `alwan_rgb_to_hwb_map` | RGB -> HWB |
| `alwan_hwb_to_rgb_map` | HWB -> RGB |
| `alwan_hsv_to_hwb_map` | HSV -> HWB |
| `alwan_hwb_to_hsv_map` | HWB -> HSV |

With `alwan_ycbcr_standard standard`:

```c
int alwan_rgb_to_ycbcr_map(alwan_scalar *out, alwan_scalar const *in,
                            alwan_ycbcr_standard standard,
                            size_t count, size_t in_stride, size_t out_stride);
```

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_rgb_to_ycbcr_map` | RGB -> YCbCr | `alwan_ycbcr_standard` |
| `alwan_ycbcr_to_rgb_map` | YCbCr -> RGB | `alwan_ycbcr_standard` |

With `int bit_depth`:

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_rgb_to_yccbccrc_map` | RGB -> YcCbcCrc | `int bit_depth` |
| `alwan_yccbccrc_to_rgb_map` | YcCbcCrc -> RGB | `int bit_depth` |
| `alwan_ycbcr_full_to_legal_map` | Full range -> Legal | `int bit_depth` |
| `alwan_ycbcr_legal_to_full_map` | Legal -> Full range | `int bit_depth` |

CMYK (4-channel, uses 4-element strides):

| Function | I/O channels |
|----------|-------------|
| `alwan_cmy_to_cmyk_map` | 3 in, 4 out |
| `alwan_cmyk_to_cmy_map` | 4 in, 3 out |

All convenience functions have `_map_ex` variants.

### Gamut Mapping

```c
int alwan_gamut_map(alwan_scalar *rgb_out,
                    alwan_gamut_map_method method,
                    alwan_scalar const *rgb_in,
                    size_t count, size_t in_stride, size_t out_stride);

int alwan_css_gamut_map(alwan_scalar *rgb_out,
                         alwan_scalar const *rgb_in,
                         size_t count, size_t in_stride, size_t out_stride);
```

`alwan_gamut_map` supports multiple mapping methods (clip, compress, etc.). `alwan_css_gamut_map` implements CSS Color Level 4 section 13.2 OKLCh binary search.

### Color Appearance Models (CAM)

```c
int alwan_ciecam02_forward_map(alwan_ciecam02_correlates *correlates_out,
                                alwan_scalar const *xyz_in,
                                alwan_ciecam02_viewing_conditions const *vc,
                                size_t count, size_t in_stride);

int alwan_ciecam02_inverse_map(alwan_scalar *xyz_out,
                                alwan_ciecam02_correlates const *correlates_in,
                                alwan_ciecam02_viewing_conditions const *vc,
                                size_t count, size_t out_stride);
```

| Model | Forward | Inverse |
|-------|---------|---------|
| CIECAM02 | `alwan_ciecam02_forward_map` | `alwan_ciecam02_inverse_map` |
| CAM16 | `alwan_cam16_forward_map` | `alwan_cam16_inverse_map` |

Forward maps output to `*_correlates` structs (not raw scalars). Inverse maps output XYZ scalars. All have `_map_ex` variants.

### Color Vision Deficiency (CVD)

```c
int alwan_simulate_cvd_map(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                             alwan_cvd_type cvd_type, alwan_scalar severity,
                             size_t count, size_t in_stride, size_t out_stride);
```

| Function | Extra params |
|----------|-------------|
| `alwan_simulate_cvd_map` | `alwan_cvd_type`, `alwan_scalar severity` |
| `alwan_simulate_protanopia_map` | `alwan_scalar severity` |
| `alwan_simulate_deuteranopia_map` | `alwan_scalar severity` |
| `alwan_simulate_tritanopia_map` | `alwan_scalar severity` |

All have `_map_ex` variants. Input/output is linear RGB.

### Color Correction

```c
int alwan_lgg_apply_map(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                          alwan_rgb const *lift, alwan_rgb const *gamma, alwan_rgb const *gain,
                          size_t count, size_t in_stride, size_t out_stride);

int alwan_color_matrix_apply_map(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                   alwan_mat3x3 const *matrix,
                                   size_t count, size_t in_stride, size_t out_stride);

int alwan_printer_lights_apply_map(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                     alwan_scalar red_lights, alwan_scalar green_lights,
                                     alwan_scalar blue_lights,
                                     size_t count, size_t in_stride, size_t out_stride);

int alwan_white_balance_apply_map(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                    alwan_rgb const *multipliers,
                                    size_t count, size_t in_stride, size_t out_stride);
```

All have `_map_ex` variants.

### Collect / Scatter Utilities

Convert between typed pixel buffers and `alwan_scalar` triplets:

```c
int alwan_collect3(alwan_scalar *out,
                   void const *in, alwan_pixel_format in_fmt,
                   size_t count, size_t in_stride, size_t out_stride);

int alwan_scatter3(void *out, alwan_pixel_format out_fmt,
                   alwan_scalar const *in,
                   size_t count, size_t in_stride, size_t out_stride);
```

Useful for building custom pipelines: collect typed input into `alwan_scalar`, process with `_map`, scatter back.
