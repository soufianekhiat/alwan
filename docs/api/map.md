# Map API Reference

Batch pixel processing. A *map* function applies one colour transform across an
entire array of pixels (or planes) in a single call, with compile-time SIMD
acceleration where available.

This page is the per-function reference. For the architecture/usage overview of
the same layer, see the conceptual guide [../map.md](../map.md).

> **Precision variants:** functions written here as `name_{T}` exist in two
> native forms — `name_f32` (single precision, `alwan_f32`) and `name_f64`
> (double precision, `alwan_f64`), with `T = f32 | f64`. Which precisions are
> compiled is controlled by `ALWAN_WITH_F32` / `ALWAN_WITH_F64` (both by
> default; see [../configuration.md](../configuration.md)). The typed `_ex`
> frontends are precision-agnostic (single un-suffixed symbol) and internally
> use `f64`.

## The Four Dispatch Shapes

The same per-pixel kernels in `src/alwan/map/` are exposed through four call
shapes:

| Shape | Suffix | Buffers | Format conversion |
|-------|--------|---------|-------------------|
| Value / single-colour | `_v` (pure math) and `_{T}` | by-value or single `alwan_*_{T} *` | none |
| Interleaved (AoS) | `_{T}_map_interleave` | `alwan_{T} *` triplets, byte strides | none |
| Planar (SoA) | `_{T}_map_planar` | one `alwan_{T} *` per channel | none |
| Typed | `_map_interleave_ex` / `_map_planar_ex` | `void *` + `alwan_pixel_format` | automatic U8/U16/F16/F32/F64 |

- **Value / single-colour.** `_v` functions are pure value-typed math
  (return-by-value, knobs only, no ctx, no buffers) from the header-only core
  tier. The single-colour conversions (`alwan_xyz_to_lab_{T}(lab, xyz, white)`)
  process one pixel through an out-pointer.
- **Interleaved** is the workhorse for packed RGB/RGBA buffers.
- **Planar** takes a separate pointer per channel for SoA host layouts.
- **Typed `_ex`** accepts `void *` buffers tagged with `alwan_pixel_format` and
  converts at the boundaries; dispatch lives in `alwan_typed_map.c` /
  `alwan_typed_planar_map.c`.

All shapes share the same kernels and SIMD backends (see [simd.md](simd.md)).

## Parameter Convention (v2.0)

Alwan uses the **memcpy argument order**: outputs before inputs, and each
`*_stride` immediately follows the buffer it describes. `count` follows the
buffer/stride block; any extra knobs come last; `ctx` (when present) is always
last.

### Interleaved

```c
int alwan_*_{T}_map_interleave(alwan_{T}       *out, size_t out_stride,
                               alwan_{T} const *in,  size_t in_stride,
                               size_t           count,
                               /* ...extra parameters... */);
```

### Typed interleaved (`_ex`)

```c
int alwan_*_map_interleave_ex(void       *out, size_t out_stride,
                              void const *in,  size_t in_stride,
                              size_t      count,
                              alwan_pixel_format out_fmt,
                              alwan_pixel_format in_fmt,
                              /* ...extra parameters... */);
```

The pixel-format pair is `(out_fmt, in_fmt)` and comes **after `count`**, not
interleaved with the buffers.

**`count`** — number of pixels (3-channel unless noted).

**`out_stride` / `in_stride`** — byte offset between consecutive pixels. For
tightly packed `alwan_{T}` triplets use `3 * sizeof(alwan_{T})`. Larger strides
let you walk interleaved RGBA (`4 * sizeof(alwan_{T})`) or sparse layouts.

**Return value** — `ALWAN_OK` (0) on success, `ALWAN_E_INVALID` on NULL
pointers or bad parameters.

## Pixel Formats (`_ex`)

```c
typedef enum {
    ALWAN_PIXEL_U8  = 0,   /* uint8_t  [0..255]   <-> [0.0, 1.0] */
    ALWAN_PIXEL_U16 = 1,   /* uint16_t [0..65535] <-> [0.0, 1.0] */
    ALWAN_PIXEL_F32 = 2,   /* float */
    ALWAN_PIXEL_F64 = 3,   /* double */
    ALWAN_PIXEL_F16 = 4    /* IEEE binary16 half float */
} alwan_pixel_format;
```

Integer formats are normalized by `(2^N - 1)`: U8 divides by 255 and U16 by
65535 on input; the result is multiplied and clamped on output.

## Usage Example

```c
/* Convert 1920x1080 sRGB pixels to CIE Lab (f64, packed triplets) */
size_t n      = 1920 * 1080;
size_t stride = 3 * sizeof(alwan_f64);

alwan_xyz_f64 d65;
alwan_illuminant_white_point_f64(&d65, ALWAN_ILLUMINANT_D65,
                                 ALWAN_OBSERVER_CIE_1931_2DEG);

alwan_srgb_to_xyz_f64_map_interleave(xyz, stride, srgb, stride, n);
alwan_xyz_to_lab_f64_map_interleave(lab, stride, xyz, stride, n, &d65);

/* Or directly from a uint8 buffer to an f32 buffer (automatic conversion) */
alwan_srgb_to_lab_map_interleave_ex(lab_f32, 3 * sizeof(float),
                                    srgb_u8, 3,
                                    n,
                                    ALWAN_PIXEL_F32, ALWAN_PIXEL_U8);
```

---

## Function Reference

### Matrix Transform

```c
int alwan_mat3_transform_{T}_map_interleave(alwan_{T} *vec_out, size_t out_stride,
                                            alwan_{T} const *vec_in, size_t in_stride,
                                            size_t count,
                                            alwan_mat3x3_{T} const *matrix);

int alwan_mat3_transform_map_interleave_ex(void *vec_out, size_t out_stride,
                                           void const *vec_in, size_t in_stride,
                                           size_t count,
                                           alwan_pixel_format out_fmt,
                                           alwan_mat3x3_f64 const *matrix,
                                           alwan_pixel_format in_fmt);
```

Applies a 3x3 matrix to each input triplet. SIMD-accelerated. Note the `_ex`
form places `matrix` *between* `out_fmt` and `in_fmt`.

### sRGB Convenience

Composite transforms that chain the sRGB EOTF/OETF with a colour-space
conversion. No extra parameters beyond the common set.

| Function | Transform |
|----------|-----------|
| `alwan_srgb_to_xyz_{T}_map_interleave` | sRGB -> linear -> XYZ (D65) |
| `alwan_xyz_to_srgb_{T}_map_interleave` | XYZ (D65) -> linear -> sRGB |
| `alwan_srgb_to_lab_{T}_map_interleave` | sRGB -> XYZ -> Lab (D65) |
| `alwan_lab_to_srgb_{T}_map_interleave` | Lab (D65) -> XYZ -> sRGB |
| `alwan_srgb_to_oklab_{T}_map_interleave` | sRGB -> linear -> Oklab |
| `alwan_oklab_to_srgb_{T}_map_interleave` | Oklab -> linear -> sRGB |

All have `_map_interleave_ex` variants.

### CIE Colorspaces

Functions that take a **white point** pass `alwan_xyz_{T} const *white_xyz` as
the trailing extra parameter:

```c
int alwan_xyz_to_lab_{T}_map_interleave(alwan_{T} *lab_out, size_t out_stride,
                                        alwan_{T} const *xyz_in, size_t in_stride,
                                        size_t count,
                                        alwan_xyz_{T} const *white_xyz);
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

All have `_map_interleave_ex` variants.

### Oklab

No extra parameters.

| Function | Direction |
|----------|-----------|
| `alwan_xyz_to_oklab_{T}_map_interleave` | XYZ -> Oklab |
| `alwan_oklab_to_xyz_{T}_map_interleave` | Oklab -> XYZ |
| `alwan_oklab_to_oklch_{T}_map_interleave` | Oklab -> OkLCh |
| `alwan_oklch_to_oklab_{T}_map_interleave` | OkLCh -> Oklab |

All have `_map_interleave_ex` variants.

### ICtCp

```c
int alwan_rgb_to_ictcp_{T}_map_interleave(alwan_{T} *ictcp_out, size_t out_stride,
                                          alwan_{T} const *rgb_in, size_t in_stride,
                                          size_t count,
                                          int use_pq /* 1 = PQ (ST 2084), 0 = HLG */);
```

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_rgb_to_ictcp_{T}_map_interleave` | BT.2020 RGB -> ICtCp | `int use_pq` |
| `alwan_ictcp_to_rgb_{T}_map_interleave` | ICtCp -> BT.2020 RGB | `int use_pq` |
| `alwan_xyz_to_ictcp_{T}_map_interleave` | XYZ -> ICtCp | `int use_pq` |
| `alwan_ictcp_to_xyz_{T}_map_interleave` | ICtCp -> XYZ | `int use_pq` |

All have `_map_interleave_ex` variants.

### JzAzBz

No extra parameters.

| Function | Direction |
|----------|-----------|
| `alwan_xyz_to_jzazbz_{T}_map_interleave` | XYZ -> Jzazbz |
| `alwan_jzazbz_to_xyz_{T}_map_interleave` | Jzazbz -> XYZ |
| `alwan_jzazbz_to_jzczhz_{T}_map_interleave` | Jzazbz -> JzCzhz |
| `alwan_jzczhz_to_jzazbz_{T}_map_interleave` | JzCzhz -> Jzazbz |

All have `_map_interleave_ex` variants.

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

With custom white point (`alwan_xyz_{T} const *white_xyz` trailing):

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

DIN99 (with `int variant` trailing):

```c
int alwan_lab_to_din99_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                          alwan_{T} const *in, size_t in_stride,
                                          size_t count,
                                          int variant);
```

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_lab_to_din99_{T}_map_interleave` | Lab -> DIN99 | `int variant` |
| `alwan_din99_to_lab_{T}_map_interleave` | DIN99 -> Lab | `int variant` |

All extended colourspace functions have `_map_interleave_ex` variants.

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

```c
int alwan_relative_luminance_{T}_map_interleave(alwan_{T} *Y_out, size_t out_stride,
                                                alwan_{T} const *rgb_in, size_t in_stride,
                                                size_t count,
                                                alwan_luma_standard standard);
```

| Function | Direction | Extra param |
|----------|-----------|-------------|
| `alwan_relative_luminance_{T}_map_interleave` | RGB -> Y | `alwan_luma_standard` |
| `alwan_relative_luminance_space_{T}_map_interleave` | RGB -> Y | `alwan_rgb_space_desc_{T} const *` |

With `alwan_ycbcr_standard standard`:

```c
int alwan_rgb_to_ycbcr_{T}_map_interleave(alwan_{T} *ycbcr_out, size_t out_stride,
                                          alwan_{T} const *rgb_in, size_t in_stride,
                                          size_t count,
                                          alwan_ycbcr_standard standard);
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

CMYK (4-channel side uses 4-element strides):

| Function | I/O channels |
|----------|--------------|
| `alwan_cmy_to_cmyk_{T}_map_interleave` | 3 in, 4 out |
| `alwan_cmyk_to_cmy_{T}_map_interleave` | 4 in, 3 out |

All convenience functions have `_map_interleave_ex` variants.

### Gamut Mapping

```c
int alwan_gamut_{T}_map_interleave(alwan_{T} *rgb_out, size_t out_stride,
                                   alwan_{T} const *rgb_in, size_t in_stride,
                                   size_t count,
                                   alwan_gamut_map_method method);

int alwan_css_gamut_{T}_map_interleave(alwan_{T} *rgb_out, size_t out_stride,
                                       alwan_{T} const *rgb_in, size_t in_stride,
                                       size_t count);
```

`alwan_gamut_{T}_map_interleave` supports the methods in
`alwan_gamut_map_method` (clip, compress, etc.).
`alwan_css_gamut_{T}_map_interleave` implements CSS Color Level 4 §13.2 OKLCh
binary search. Both have `_map_interleave_ex` and `_map_planar_ex` variants.

### Color Appearance Models (CAM)

Forward maps emit `*_correlates_{T}` structs (not raw scalars), so they take
only an `in_stride`; inverse maps emit XYZ scalars and take only an
`out_stride`:

```c
int alwan_ciecam02_forward_{T}_map_interleave(
    alwan_ciecam02_correlates_{T} *correlates_out,
    alwan_{T} const *xyz_in, size_t in_stride,
    alwan_ciecam02_viewing_conditions_{T} const *vc,
    size_t count);

int alwan_ciecam02_inverse_{T}_map_interleave(
    alwan_{T} *xyz_out, size_t out_stride,
    alwan_ciecam02_correlates_{T} const *correlates_in,
    alwan_ciecam02_viewing_conditions_{T} const *vc,
    size_t count);
```

| Model | Forward | Inverse |
|-------|---------|---------|
| CIECAM02 | `alwan_ciecam02_forward_{T}_map_interleave` | `alwan_ciecam02_inverse_{T}_map_interleave` |
| CAM16 | `alwan_cam16_forward_{T}_map_interleave` | `alwan_cam16_inverse_{T}_map_interleave` |

### Color Vision Deficiency (CVD)

```c
int alwan_simulate_cvd_{T}_map_interleave(alwan_{T} *rgb_out, size_t out_stride,
                                          alwan_{T} const *rgb_in, size_t in_stride,
                                          size_t count,
                                          alwan_cvd_type cvd_type, alwan_{T} severity);
```

| Function | Extra params |
|----------|--------------|
| `alwan_simulate_cvd_{T}_map_interleave` | `alwan_cvd_type`, `alwan_{T} severity` |
| `alwan_simulate_protanopia_{T}_map_interleave` | `alwan_{T} severity` |
| `alwan_simulate_deuteranopia_{T}_map_interleave` | `alwan_{T} severity` |
| `alwan_simulate_tritanopia_{T}_map_interleave` | `alwan_{T} severity` |

All have `_map_interleave_ex` variants. Input/output is linear RGB.

### Color Correction

```c
int alwan_lgg_apply_{T}_map_interleave(alwan_{T} *rgb_out, size_t out_stride,
                                       alwan_{T} const *rgb_in, size_t in_stride,
                                       size_t count,
                                       alwan_rgb_{T} const *lift,
                                       alwan_rgb_{T} const *gamma,
                                       alwan_rgb_{T} const *gain);

int alwan_color_matrix_apply_{T}_map_interleave(alwan_{T} *rgb_out, size_t out_stride,
                                                alwan_{T} const *rgb_in, size_t in_stride,
                                                size_t count,
                                                alwan_mat3x3_{T} const *matrix);

int alwan_printer_lights_apply_{T}_map_interleave(alwan_{T} *rgb_out, size_t out_stride,
                                                  alwan_{T} const *rgb_in, size_t in_stride,
                                                  size_t count,
                                                  alwan_{T} red_lights,
                                                  alwan_{T} green_lights,
                                                  alwan_{T} blue_lights);

int alwan_white_balance_apply_{T}_map_interleave(alwan_{T} *rgb_out, size_t out_stride,
                                                 alwan_{T} const *rgb_in, size_t in_stride,
                                                 size_t count,
                                                 alwan_rgb_{T} const *multipliers);
```

All have `_map_interleave_ex` variants.

---

## Planar (SoA) Shape

Planar maps expose one pointer per channel; `out_stride` / `in_stride` are the
byte distance between adjacent samples *within* a plane:

```c
int alwan_xyz_to_lab_{T}_map_planar(alwan_{T} *out_ch0, size_t out_stride,
                                    alwan_{T} *out_ch1,
                                    alwan_{T} *out_ch2,
                                    alwan_{T} const *in_ch0, size_t in_stride,
                                    alwan_{T} const *in_ch1,
                                    alwan_{T} const *in_ch2,
                                    size_t count,
                                    alwan_xyz_{T} const *white_xyz);
```

The typed planar form keeps the canonical `(out_fmt, in_fmt)` order:

```c
int alwan_xyz_to_lab_map_planar_ex(void *out0, size_t out_stride,
                                   void *out1, void *out2,
                                   void const *in0, size_t in_stride,
                                   void const *in1, void const *in2,
                                   size_t count,
                                   alwan_pixel_format out_fmt,
                                   alwan_pixel_format in_fmt,
                                   alwan_xyz_f64 const *white_xyz);
```

> **Legacy ordering caveat.** A small block of `_map_planar_ex` helpers near the
> end of `alwan.h` (around lines 4117-4156: the `alwan_srgb_to_*`,
> `alwan_lab_to_lch` / `alwan_lch_to_lab`, `alwan_luv_to_lchuv` /
> `alwan_lchuv_to_luv` planar variants) still ship with the older
> `(in_fmt, out_fmt)` argument order. Read the specific declaration before
> wiring one up; these will migrate to `(out_fmt, in_fmt)` in a future release.

---

## Image Conversion

`alwan_image_convert` is now its own unit
(`src/alwan/map/alwan_image_convert_impl.inc`), separate from the generic
colourspace maps. It runs a full 2D image pipeline —
EOTF → matrix (with Bradford CAT across whitepoints) → OETF — with pixel-format
conversion, driven by RGB space descriptors and row strides:

```c
int alwan_image_convert_{T}(void *dst, size_t dst_row_stride,
                            void const *src, size_t src_row_stride,
                            size_t width, size_t height,
                            alwan_pixel_format dst_fmt,
                            alwan_pixel_format src_fmt,
                            alwan_rgb_space_desc_{T} const *src_space,
                            alwan_rgb_space_desc_{T} const *dst_space,
                            alwan_ctx *ctx);
```

The `_rgba` variant runs the same pipeline on 4-channel pixels, preserving the
alpha channel per the `alwan_alpha_mode` argument:

```c
int alwan_image_convert_rgba_{T}(void *dst, size_t dst_row_stride,
                                 void const *src, size_t src_row_stride,
                                 size_t width, size_t height,
                                 alwan_pixel_format dst_fmt,
                                 alwan_pixel_format src_fmt,
                                 alwan_rgb_space_desc_{T} const *src_space,
                                 alwan_rgb_space_desc_{T} const *dst_space,
                                 alwan_alpha_mode alpha_mode,
                                 alwan_ctx *ctx);
```

Both exist as native `_f32` and `_f64`.

---

## Collect / Scatter Utilities

Convert between typed pixel buffers and `alwan_{T}` triplets so you can stage
your own pipeline (collect once, run several `_map_interleave` passes, scatter
once):

```c
int alwan_collect3_{T}(alwan_{T} *out, size_t out_stride,
                       void const *in, size_t in_stride,
                       size_t count,
                       alwan_pixel_format in_fmt);

int alwan_scatter3_{T}(void *out, size_t out_stride,
                       alwan_{T} const *in, size_t in_stride,
                       size_t count,
                       alwan_pixel_format out_fmt);
```

---

## See Also

- [../map.md](../map.md) — conceptual/architecture guide for the map layer
- [simd.md](simd.md) — the SIMD backend selection underneath these kernels
- [color-spaces.md](color-spaces.md), [gamut.md](gamut.md),
  [color-appearance.md](color-appearance.md) — per-domain references
- [../configuration.md](../configuration.md) — `ALWAN_WITH_F32` / `ALWAN_WITH_F64`
  precision build config
