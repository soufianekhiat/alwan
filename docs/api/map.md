# Map API Reference

Batch pixel processing. Applies a color transform to an array of pixels in a single call with optional SIMD acceleration.

> **Precision variants:** Every function shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

## Overview

Every per-pixel transform (`alwan_xyz_to_lab_{T}`, etc.) has two batch variants:

| Suffix | Input/Output type | Format conversion |
|--------|-------------------|-------------------|
| `_{T}_map_interleave` | `alwan_{T} *` | None |
| `_{T}_map_interleave_ex` | `void *` with `alwan_pixel_format` | Automatic U8/U16/F32/F64 |

Both use tiled processing with compile-time SIMD where available. See [simd.md](simd.md) for the underlying SIMD layer.

## Common Parameters

All `_map_interleave` functions share the same trailing parameters:

```c
int alwan_*_{T}_map_interleave(alwan_{T}       *out,
                alwan_{T} const *in,
                size_t           count,       /* number of pixels */
                size_t           in_stride,   /* bytes between input pixels */
                size_t           out_stride); /* bytes between output pixels */
```

**`count`** -- number of 3-channel pixels to process.

**`in_stride` / `out_stride`** -- byte offset between consecutive pixels. For tightly packed `alwan_{T}` triplets use `3 * sizeof(alwan_{T})`. Larger strides allow processing interleaved RGBA (stride `4 * sizeof(alwan_{T})`) or sparse layouts.

**Return value** -- `ALWAN_OK` (0) on success, `ALWAN_E_INVALID` on NULL pointers or bad parameters.

## Pixel Formats (`_map_interleave_ex`)

```c
typedef enum {
    ALWAN_PIXEL_U8  = 0,   /* uint8_t  [0..255]   <-> [0.0, 1.0] */
    ALWAN_PIXEL_U16 = 1,   /* uint16_t [0..65535]  <-> [0.0, 1.0] */
    ALWAN_PIXEL_F32 = 2,   /* float */
    ALWAN_PIXEL_F64 = 3,   /* double */
    ALWAN_PIXEL_F16 = 4    /* half float */
} alwan_pixel_format;
```

`_map_interleave_ex` functions accept `void *` buffers and convert at the boundaries:

```c
int alwan_*_map_interleave_ex(void              *out, alwan_pixel_format out_fmt,
                   void const        *in,  alwan_pixel_format in_fmt,
                   size_t             count,
                   size_t             in_stride,
                   size_t             out_stride);
```

Integer formats are normalized: U8 divides by 255, U16 by 65535 on input; multiplied and clamped on output.

## Usage Example

```c
/* Convert 1920x1080 sRGB pixels to CIE Lab */
size_t n = 1920 * 1080;
size_t stride = 3 * sizeof(double);

alwan_xyz_{T} d65;
alwan_illuminant_white_point_{T}(&d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);

alwan_srgb_to_xyz_{T}_map_interleave(xyz, srgb, n, stride, stride);
alwan_xyz_to_lab_{T}_map_interleave(lab, xyz, &d65, n, stride, stride);

/* Or directly with uint8 buffers (automatic conversion) */
alwan_srgb_to_lab_map_interleave_ex(lab_f32, ALWAN_PIXEL_F32,
                          srgb_u8, ALWAN_PIXEL_U8,
                          n, 3, 12);
```

---

## Function Reference

### Matrix Transform

```c
int alwan_mat3_transform_{T}_map_interleave(alwan_{T} *out, alwan_mat3x3_{T} const *matrix,
                              alwan_{T} const *in,
                              size_t count, size_t in_stride, size_t out_stride);

int alwan_mat3_transform_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                                 alwan_mat3x3_{T} const *matrix,
                                 void const *in, alwan_pixel_format in_fmt,
                                 size_t count, size_t in_stride, size_t out_stride);
```

Applies a 3x3 matrix to each input triplet. SIMD-accelerated.

### sRGB Convenience

Composite transforms that chain sRGB EOTF/OETF with a color space conversion.

| Function | Transform |
|----------|-----------|
| `alwan_srgb_to_xyz_{T}_map_interleave` | sRGB -> linear -> XYZ (D65) |
| `alwan_xyz_to_srgb_{T}_map_interleave` | XYZ (D65) -> linear -> sRGB |
| `alwan_srgb_to_lab_{T}_map_interleave` | sRGB -> XYZ -> Lab (D65) |
| `alwan_lab_to_srgb_{T}_map_interleave` | Lab (D65) -> XYZ -> sRGB |
| `alwan_srgb_to_oklab_{T}_map_interleave` | sRGB -> linear -> Oklab |
| `alwan_oklab_to_srgb_{T}_map_interleave` | Oklab -> linear -> sRGB |

All have `_map_interleave_ex` variants. No extra parameters beyond the common set.

### CIE Colorspaces

Functions that take a **white point** parameter:

```c
int alwan_xyz_to_lab_{T}_map_interleave(alwan_{T} *out, alwan_{T} const *in,
                          alwan_xyz_{T} const *white_xyz,
                          size_t count, size_t in_stride, size_t out_stride);
```

| Function | Direction | White point |
|----------|-----------|-------------|
| `alwan_xyz_to_lab_{T}_map_interleave` | XYZ -> Lab | yes |
| `alwan_lab_to_xyz_{T}_map_interleave` | Lab -> XYZ | yes |
| `alwan_xyz_to_luv_{T}_map_interleave` | XYZ -> Luv | yes |
| `alwan_luv_to_xyz_{T}_map_interleave` | Luv -> XYZ | yes |
| `alwan_lab_to_lch_{T}_map_interleave` | Lab -> LCh(ab) | no |
| `alwan_lch_to_lab_{T}_map_interleave` | LCh(ab) -> Lab | no |
| `alwan_luv_to_lchuv_{T}_map_interleave` | Luv -> LCh(uv) | no |
| `alwan_lchuv_to_luv_{T}_map_interleave` | LCh(uv) -> Luv | no |
| `alwan_xyz_to_xyy_{T}_map_interleave` | XYZ -> xyY | no |
| `alwan_xyy_to_xyz_{T}_map_interleave` | xyY -> XYZ | no |

All have `_map_interleave_ex` variants. White-point variants pass `alwan_xyz_{T} const *white_xyz` after the input buffer.

### Oklab

| Function | Direction |
|----------|-----------|
| `alwan_xyz_to_oklab_{T}_map_interleave` | XYZ -> Oklab |
| `alwan_oklab_to_xyz_{T}_map_interleave` | Oklab -> XYZ |
| `alwan_oklab_to_oklch_{T}_map_interleave` | Oklab -> OkLCh |
| `alwan_oklch_to_oklab_{T}_map_interleave` | OkLCh -> Oklab |

All have `_map_interleave_ex` variants. No extra parameters.

### ICtCp

```c
int alwan_rgb_to_ictcp_{T}_map_interleave(alwan_{T} *out, alwan_{T} const *in,
                            int use_pq, /* 1 = PQ (ST 2084), 0 = HLG */
                            size_t count, size_t in_stride, size_t out_stride);
```

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_rgb_to_ictcp_{T}_map_interleave` | BT.2020 RGB -> ICtCp | `int use_pq` |
| `alwan_ictcp_to_rgb_{T}_map_interleave` | ICtCp -> BT.2020 RGB | `int use_pq` |
| `alwan_xyz_to_ictcp_{T}_map_interleave` | XYZ -> ICtCp | `int use_pq` |
| `alwan_ictcp_to_xyz_{T}_map_interleave` | ICtCp -> XYZ | `int use_pq` |

All have `_map_interleave_ex` variants.

### JzAzBz

| Function | Direction |
|----------|-----------|
| `alwan_xyz_to_jzazbz_{T}_map_interleave` | XYZ -> Jzazbz |
| `alwan_jzazbz_to_xyz_{T}_map_interleave` | Jzazbz -> XYZ |
| `alwan_jzazbz_to_jzczhz_{T}_map_interleave` | Jzazbz -> JzCzhz |
| `alwan_jzczhz_to_jzazbz_{T}_map_interleave` | JzCzhz -> Jzazbz |

All have `_map_interleave_ex` variants. No extra parameters.

### IPT

| Function | Direction |
|----------|-----------|
| `alwan_xyz_to_ipt_{T}_map_interleave` | XYZ -> IPT |
| `alwan_ipt_to_xyz_{T}_map_interleave` | IPT -> XYZ |

Both have `_map_interleave_ex` variants.

### Extended Colorspaces

No extra parameters:

| Function | Direction |
|----------|-----------|
| `alwan_xyz_to_igpgtg_{T}_map_interleave` | XYZ -> IgPgTg |
| `alwan_igpgtg_to_xyz_{T}_map_interleave` | IgPgTg -> XYZ |
| `alwan_xyz_to_icacb_{T}_map_interleave` | XYZ -> ICaCb |
| `alwan_icacb_to_xyz_{T}_map_interleave` | ICaCb -> XYZ |
| `alwan_xyz_to_hdr_cielab_{T}_map_interleave` | XYZ -> HDR-CIELAB |
| `alwan_hdr_cielab_to_xyz_{T}_map_interleave` | HDR-CIELAB -> XYZ |
| `alwan_xyz_to_hdr_ipt_{T}_map_interleave` | XYZ -> HDR-IPT |
| `alwan_hdr_ipt_to_xyz_{T}_map_interleave` | HDR-IPT -> XYZ |
| `alwan_xyz_to_ucs_{T}_map_interleave` | XYZ -> CIE 1960 UCS |
| `alwan_ucs_to_xyz_{T}_map_interleave` | UCS -> XYZ |
| `alwan_xyz_to_osa_ucs_{T}_map_interleave` | XYZ -> OSA-UCS |
| `alwan_osa_ucs_to_xyz_{T}_map_interleave` | OSA-UCS -> XYZ |
| `alwan_xyz_to_hunter_lab_{T}_map_interleave` | XYZ -> Hunter Lab (D65) |
| `alwan_hunter_lab_to_xyz_{T}_map_interleave` | Hunter Lab -> XYZ (D65) |
| `alwan_xyz_to_prolab_{T}_map_interleave` | XYZ -> ProLab (D65) |
| `alwan_prolab_to_xyz_{T}_map_interleave` | ProLab -> XYZ (D65) |

With custom white point (`alwan_xyz_{T} const *white_xyz`):

| Function | Direction |
|----------|-----------|
| `alwan_xyz_to_hunter_lab_custom_{T}_map_interleave` | XYZ -> Hunter Lab |
| `alwan_hunter_lab_to_xyz_custom_{T}_map_interleave` | Hunter Lab -> XYZ |
| `alwan_xyz_to_prolab_custom_{T}_map_interleave` | XYZ -> ProLab |
| `alwan_prolab_to_xyz_custom_{T}_map_interleave` | ProLab -> XYZ |
| `alwan_xyz_to_uvw_{T}_map_interleave` | XYZ -> U\*V\*W\* |
| `alwan_uvw_to_xyz_{T}_map_interleave` | U\*V\*W\* -> XYZ |

RGB-based (no extra parameters):

| Function | Direction |
|----------|-----------|
| `alwan_rgb_to_prismatic_{T}_map_interleave` | RGB -> Prismatic |
| `alwan_prismatic_to_rgb_{T}_map_interleave` | Prismatic -> RGB |
| `alwan_rgb_to_hcl_{T}_map_interleave` | RGB -> HCL |
| `alwan_hcl_to_rgb_{T}_map_interleave` | HCL -> RGB |
| `alwan_rgb_to_ihls_{T}_map_interleave` | RGB -> IHLS |
| `alwan_ihls_to_rgb_{T}_map_interleave` | IHLS -> RGB |

DIN99 (with `int variant`):

```c
int alwan_lab_to_din99_{T}_map_interleave(alwan_{T} *out, alwan_{T} const *in,
                            int variant,
                            size_t count, size_t in_stride, size_t out_stride);
```

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_lab_to_din99_{T}_map_interleave` | Lab -> DIN99 | `int variant` |
| `alwan_din99_to_lab_{T}_map_interleave` | DIN99 -> Lab | `int variant` |

All extended colorspace functions have `_map_interleave_ex` variants.

### Convenience (HSV, HSL, CMY, YCbCr, ...)

No extra parameters:

| Function | Direction |
|----------|-----------|
| `alwan_rgb_to_hsv_{T}_map_interleave` | RGB -> HSV |
| `alwan_hsv_to_rgb_{T}_map_interleave` | HSV -> RGB |
| `alwan_rgb_to_hsl_{T}_map_interleave` | RGB -> HSL |
| `alwan_hsl_to_rgb_{T}_map_interleave` | HSL -> RGB |
| `alwan_rgb_to_cmy_{T}_map_interleave` | RGB -> CMY |
| `alwan_cmy_to_rgb_{T}_map_interleave` | CMY -> RGB |
| `alwan_rgb_to_ycocg_{T}_map_interleave` | RGB -> YCoCg |
| `alwan_ycocg_to_rgb_{T}_map_interleave` | YCoCg -> RGB |
| `alwan_rgb_to_hwb_{T}_map_interleave` | RGB -> HWB |
| `alwan_hwb_to_rgb_{T}_map_interleave` | HWB -> RGB |
| `alwan_hsv_to_hwb_{T}_map_interleave` | HSV -> HWB |
| `alwan_hwb_to_hsv_{T}_map_interleave` | HWB -> HSV |
| `alwan_rgb_to_hsp_{T}_map_interleave` | RGB -> HSP (perceived brightness) |
| `alwan_hsp_to_rgb_{T}_map_interleave` | HSP -> RGB |
| `alwan_rgb_to_hsplog_{T}_map_interleave` | RGB -> HSPLog (log saturation HSP) |
| `alwan_hsplog_to_rgb_{T}_map_interleave` | HSPLog -> RGB |
| `alwan_rgb_to_hsy_{T}_map_interleave` | RGB -> HSY (luma-weighted) |
| `alwan_hsy_to_rgb_{T}_map_interleave` | HSY -> RGB |
| `alwan_linear_srgb_to_hsv_{T}_map_interleave` | Linear sRGB -> HSV |
| `alwan_hsv_to_linear_srgb_{T}_map_interleave` | HSV -> Linear sRGB |
| `alwan_linear_srgb_to_hsl_{T}_map_interleave` | Linear sRGB -> HSL |
| `alwan_hsl_to_linear_srgb_{T}_map_interleave` | HSL -> Linear sRGB |

With `alwan_luma_standard standard` (3-channel input, 1-channel output):

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_relative_luminance_{T}_map_interleave` | RGB -> Y | `alwan_luma_standard` |

With `alwan_rgb_space_desc_{T} const *space` (3-channel input, 1-channel output):

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_relative_luminance_space_{T}_map_interleave` | RGB -> Y | `alwan_rgb_space_desc_{T}` |

With `alwan_ycbcr_standard standard`:

```c
int alwan_rgb_to_ycbcr_{T}_map_interleave(alwan_{T} *out, alwan_{T} const *in,
                            alwan_ycbcr_standard standard,
                            size_t count, size_t in_stride, size_t out_stride);
```

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_rgb_to_ycbcr_{T}_map_interleave` | RGB -> YCbCr | `alwan_ycbcr_standard` |
| `alwan_ycbcr_to_rgb_{T}_map_interleave` | YCbCr -> RGB | `alwan_ycbcr_standard` |

With `int bit_depth`:

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_rgb_to_yccbccrc_{T}_map_interleave` | RGB -> YcCbcCrc | `int bit_depth` |
| `alwan_yccbccrc_to_rgb_{T}_map_interleave` | YcCbcCrc -> RGB | `int bit_depth` |
| `alwan_ycbcr_full_to_legal_{T}_map_interleave` | Full range -> Legal | `int bit_depth` |
| `alwan_ycbcr_legal_to_full_{T}_map_interleave` | Legal -> Full range | `int bit_depth` |

CMYK (4-channel, uses 4-element strides):

| Function | I/O channels |
|----------|-------------|
| `alwan_cmy_to_cmyk_{T}_map_interleave` | 3 in, 4 out |
| `alwan_cmyk_to_cmy_{T}_map_interleave` | 4 in, 3 out |

All convenience functions have `_map_interleave_ex` variants.

### Gamut Mapping

```c
int alwan_gamut_{T}_map_interleave(alwan_{T} *rgb_out,
                    alwan_gamut_map_method method,
                    alwan_{T} const *rgb_in,
                    size_t count, size_t in_stride, size_t out_stride);

int alwan_css_gamut_map_interleave(alwan_f64 *rgb_out,
                         alwan_f64 const *rgb_in,
                         size_t count, size_t in_stride, size_t out_stride);
```

`alwan_gamut_{T}_map_interleave` supports multiple mapping methods (clip, compress, etc.). `alwan_css_gamut_map_interleave` implements CSS Color Level 4 section 13.2 OKLCh binary search.

### Color Appearance Models (CAM)

```c
int alwan_ciecam02_forward_{T}_map_interleave(
    alwan_ciecam02_correlates_{T} *correlates_out,
    alwan_{T} const *xyz_in,
    alwan_ciecam02_viewing_conditions_{T} const *vc,
    size_t count, size_t in_stride);

int alwan_ciecam02_inverse_{T}_map_interleave(
    alwan_{T} *xyz_out,
    alwan_ciecam02_correlates_{T} const *correlates_in,
    alwan_ciecam02_viewing_conditions_{T} const *vc,
    size_t count, size_t out_stride);
```

| Model | Forward | Inverse |
|-------|---------|---------|
| CIECAM02 | `alwan_ciecam02_forward_{T}_map_interleave` | `alwan_ciecam02_inverse_{T}_map_interleave` |
| CAM16 | `alwan_cam16_forward_{T}_map_interleave` | `alwan_cam16_inverse_{T}_map_interleave` |

Forward maps output to `*_correlates_{T}` structs (not raw scalars). Inverse maps output XYZ scalars. All have `_map_interleave_ex` variants.

### Color Vision Deficiency (CVD)

```c
int alwan_simulate_cvd_{T}_map_interleave(alwan_{T} *rgb_out, alwan_{T} const *rgb_in,
                             alwan_cvd_type cvd_type, alwan_{T} severity,
                             size_t count, size_t in_stride, size_t out_stride);
```

| Function | Extra params |
|----------|-------------|
| `alwan_simulate_cvd_{T}_map_interleave` | `alwan_cvd_type`, `alwan_{T} severity` |
| `alwan_simulate_protanopia_{T}_map_interleave` | `alwan_{T} severity` |
| `alwan_simulate_deuteranopia_{T}_map_interleave` | `alwan_{T} severity` |
| `alwan_simulate_tritanopia_{T}_map_interleave` | `alwan_{T} severity` |

All have `_map_interleave_ex` variants. Input/output is linear RGB.

### Color Correction

```c
int alwan_lgg_apply_{T}_map_interleave(alwan_{T} *rgb_out, alwan_{T} const *rgb_in,
                          alwan_rgb_{T} const *lift, alwan_rgb_{T} const *gamma,
                          alwan_rgb_{T} const *gain,
                          size_t count, size_t in_stride, size_t out_stride);

int alwan_color_matrix_apply_{T}_map_interleave(alwan_{T} *rgb_out, alwan_{T} const *rgb_in,
                                   alwan_mat3x3_{T} const *matrix,
                                   size_t count, size_t in_stride, size_t out_stride);

int alwan_printer_lights_apply_{T}_map_interleave(alwan_{T} *rgb_out, alwan_{T} const *rgb_in,
                                     alwan_{T} red_lights, alwan_{T} green_lights,
                                     alwan_{T} blue_lights,
                                     size_t count, size_t in_stride, size_t out_stride);

int alwan_white_balance_apply_{T}_map_interleave(alwan_{T} *rgb_out, alwan_{T} const *rgb_in,
                                    alwan_rgb_{T} const *multipliers,
                                    size_t count, size_t in_stride, size_t out_stride);
```

All have `_map_interleave_ex` variants.

### Collect / Scatter Utilities

Convert between typed pixel buffers and `double` triplets:

```c
int alwan_collect3_{T}(alwan_{T} *out,
                   void const *in, alwan_pixel_format in_fmt,
                   size_t count, size_t in_stride, size_t out_stride);

int alwan_scatter3_{T}(void *out, alwan_pixel_format out_fmt,
                   alwan_{T} const *in,
                   size_t count, size_t in_stride, size_t out_stride);
```

Useful for building custom pipelines: collect typed input into `alwan_{T}`, process with `_map_interleave`, scatter back.
