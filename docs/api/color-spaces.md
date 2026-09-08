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

**API Pattern** (follows the [API conventions](../api-conventions.md): outputs first, each `*_stride` immediately after its buffer, `ctx` last):
- Single-element: `alwan_foo_{T}(out, in, ...)`: typed structs, no stride.
- Bulk interleaved: `alwan_foo_{T}_map_interleave(alwan_{T} *out, size_t out_stride, alwan_{T} const *in, size_t in_stride, size_t count, ...)`: raw scalar arrays, strides in bytes.
- Bulk planar: `alwan_foo_{T}_map_planar(alwan_{T} *out_ch0, size_t out_stride, alwan_{T} *out_ch1, alwan_{T} *out_ch2, alwan_{T} const *in_ch0, size_t in_stride, alwan_{T} const *in_ch1, alwan_{T} const *in_ch2, size_t count, ...)`: separate channel arrays.

---

## CIE XYZ Conversions

### alwan_xyz_to_lab_{T} / alwan_lab_to_xyz_{T}

```c
// Single element
void alwan_xyz_to_lab_{T}(alwan_lab_{T} *lab,
                           alwan_xyz_{T} const *xyz,
                           alwan_xyz_{T} const *white_xyz);

// Bulk interleaved (strides in bytes, immediately after each buffer)
int alwan_xyz_to_lab_{T}_map_interleave(alwan_{T} *lab_out, size_t out_stride,
                                         alwan_{T} const *xyz_in, size_t in_stride,
                                         size_t count,
                                         alwan_xyz_{T} const *white_xyz);

// Bulk planar
int alwan_xyz_to_lab_{T}_map_planar(alwan_{T} *out_ch0, size_t out_stride,
                                     alwan_{T} *out_ch1, alwan_{T} *out_ch2,
                                     alwan_{T} const *in_ch0, size_t in_stride,
                                     alwan_{T} const *in_ch1, alwan_{T} const *in_ch2,
                                     size_t count,
                                     alwan_xyz_{T} const *white_xyz);
```

Inverse: `alwan_lab_to_xyz_{T}` / `alwan_lab_to_xyz_{T}_map_interleave` / `alwan_lab_to_xyz_{T}_map_planar`: same pattern.

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
                              int use_pq);  /* 1 = PQ (BT.2100), 0 = HLG */
void alwan_ictcp_to_xyz_{T}(alwan_xyz_{T} *xyz, alwan_ictcp_{T} const *ictcp,
                              int use_pq);
```

---

### alwan_ipt_to_iptch_{T} / alwan_iptch_to_ipt_{T}

```c
void alwan_ipt_to_iptch_{T}(alwan_iptch_{T} *iptch, alwan_ipt_{T} const *ipt);
void alwan_iptch_to_ipt_{T}(alwan_ipt_{T} *ipt, alwan_iptch_{T} const *iptch);
```

The cylindrical form of IPT: intensity, chroma, hue. Hue is **radians** natively,
and maps to `[0, 1]` under the default normalization. See
[ranges.md](../ranges.md).

---

## Perceptual Pickers

Five spaces exist to be steered by hand rather than to measure with. They are
defined against the **sRGB gamut**, and they take and return **encoded** sRGB,
`[0, 1]` with the OETF applied, not linear light. All return `void`; see
[api-conventions.md](../api-conventions.md) for what that means about null
pointers.

```c
void alwan_hsluv_to_srgb_{T}(alwan_rgb_{T} *rgb, alwan_hsluv_{T} const *hsluv);
void alwan_srgb_to_hsluv_{T}(alwan_hsluv_{T} *hsluv, alwan_rgb_{T} const *srgb);

void alwan_hpluv_to_srgb_{T}(alwan_rgb_{T} *rgb, alwan_hpluv_{T} const *hpluv);
void alwan_srgb_to_hpluv_{T}(alwan_hpluv_{T} *hpluv, alwan_rgb_{T} const *srgb);

void alwan_okhsl_to_srgb_{T}(alwan_rgb_{T} *rgb, alwan_okhsl_{T} const *okhsl);
void alwan_srgb_to_okhsl_{T}(alwan_okhsl_{T} *okhsl, alwan_rgb_{T} const *srgb);

void alwan_okhsv_to_srgb_{T}(alwan_rgb_{T} *rgb, alwan_okhsv_{T} const *okhsv);
void alwan_srgb_to_okhsv_{T}(alwan_okhsv_{T} *okhsv, alwan_rgb_{T} const *srgb);

void alwan_cubehelix_to_rgb_{T}(alwan_rgb_{T} *rgb, alwan_cubehelix_{T} const *ch);
void alwan_rgb_to_cubehelix_{T}(alwan_cubehelix_{T} *ch, alwan_rgb_{T} const *rgb);
```

| space | built on | what `s` means |
|---|---|---|
| HSLuv (Boronine) | CIE LCHuv | percentage of the maximum chroma sRGB holds at that hue and lightness |
| HPLuv (Boronine) | CIE LCHuv | percentage of the minimum chroma across **all** hues at that lightness, so every triple is in gamut |
| Okhsl (Ottosson 2021) | Oklab | perceptual saturation, gamut-aware |
| Okhsv (Ottosson 2021) | Oklab | perceptual saturation, value rather than lightness |
| Cubehelix (Green 2011) | Rec.601 luminance helix | amplitude of the helix, unbounded |

**Choosing between them.** HSLuv keeps the saturated corners of sRGB reachable at
the cost of `s` meaning a different chroma at every hue, so a swatch set built by
sweeping hue at fixed `s` will not look equally colourful. HPLuv gives up those
corners so that `s` is comparable across hues, which is why it produces pastels
and why it is the better choice for categorical palettes. Okhsl and Okhsv are the
Oklab equivalents and are the better default for a picker UI. Cubehelix is a
visualization ramp with monotonically increasing luminance: it survives being
printed in greyscale, which is what it was designed for.

> **Read [ranges.md](../ranges.md) before passing literals.** The three
> conventions disagree, and the default build normalizes two of them. HSLuv and
> HPLuv are `h` `[0, 360)` and `s`, `l` `[0, 100]` in the model, but with
> `ALWAN_NORMALIZE_RANGES` at its default of `1` the converters here take and
> return all three in `[0, 1]`. Okhsl and Okhsv are `[0, 1]` either way.
> Cubehelix normalizes only `h`, and clamps nothing on output.

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

To obtain the linear RGB<->XYZ (NPM) matrices for a space directly from its
primaries and white point, use `alwan_rgb_derive_matrices_{T}`:

```c
int alwan_rgb_derive_matrices_{T}(alwan_mat3x3_{T} *rgb_to_xyz,
                                   alwan_mat3x3_{T} *xyz_to_rgb,
                                   alwan_rgb_space_desc_{T} const *desc);
```

Returns `ALWAN_OK` on success, `ALWAN_E_RANGE` if the primaries/white point form
a singular matrix.

---

### alwan_rgb_convert_{T}

```c
int alwan_rgb_convert_{T}(alwan_rgb_{T} *dst_rgb,
                           alwan_rgb_space_desc_{T} const *src_space,
                           alwan_rgb_space_desc_{T} const *dst_space,
                           alwan_rgb_{T} const *src_rgb,
                           alwan_ctx *ctx);
```

Converts a single RGB color between two spaces. When the source and destination
white points differ, chromatic adaptation is applied automatically using the
**Bradford CAT** by default. Returns `ALWAN_OK` on success, `ALWAN_E_INVALID` on error.

A strided bulk variant is available:

```c
int alwan_rgb_convert_map_interleave_{T}(alwan_rgb_{T} *dst_rgb,
                                          alwan_rgb_space_desc_{T} const *src_space,
                                          alwan_rgb_space_desc_{T} const *dst_space,
                                          alwan_rgb_{T} const *src_rgb,
                                          size_t count,
                                          alwan_ctx *ctx);
```

**Example:**
```c
alwan_rgb_space_desc_{T} srgb_desc, bt2020_desc;
alwan_rgb_get_space_descriptor_{T}(&srgb_desc, ALWAN_RGB_SPACE_SRGB, ctx);
alwan_rgb_get_space_descriptor_{T}(&bt2020_desc, ALWAN_RGB_SPACE_BT2020, ctx);

alwan_rgb_{T} rgb_in = {0.8, 0.3, 0.2};
alwan_rgb_{T} rgb_out;
alwan_rgb_convert_{T}(&rgb_out, &srgb_desc, &bt2020_desc, &rgb_in, ctx);
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

## Direct Conversions, Convenience Models, and Relative Luminance

39 single-pixel operations: four direct RGB<->perceptual pairs, four direct
XYZ<->cylindrical pairs (LCh(ab), LCh(uv), Oklch, and the HLC reorder), CMY and
CMYK, four video encodings, HSY, HSPLog, HWB, and three relative-luminance entry
points. They are declared in seven separate blocks of `alwan.h` (`:772-817`,
`:825-840`, `:1468-1471`, `:3196-3267`, `:3289-3310`, `:3418-3421`,
`:4986-4989`), plus the orphaned YCoCg inverse at `:3468-3469`. Four
consequences follow.

> **The ranges printed in `alwan.h` are the `ALWAN_NORMALIZE_RANGES=0` ranges,
> and the default is `1`.** The string `NORMALIZE_RANGES` does not occur
> anywhere in `alwan.h`. The macro is documented in `alwan_platform.h:1150-1159`
> and defaults to `1` (`:1161-1163`), under which the API wrappers rescale
> bounded channels on output and undo it on input. Affected here: Lab `L`, Luv
> `L`, LCh `L` and `h`, LCh(uv) `L` and `h`, Oklch `h`, HLC `H` and `L`, YCoCg
> `Co` and `Cg`, YcCbcCrc `Cbc` and `Crc`. Every range below is given for both
> builds. See [ranges.md](../ranges.md).

> **`ALWAN_NORMALIZE_RANGES=0` in your own translation unit does nothing to a
> prebuilt library.** The `ALWAN_NORM_*` / `ALWAN_DENORM_*` macros expand inside
> the library's own `.c` files, so this is a library-compile-time setting.
> `alwan_platform.h:1158` tells you to define it before including `alwan.h`,
> which is wrong for a consumer linking against a built `libalwan`. The
> Sharpmake reference build defines it for every project
> (`buildsystem/sharpmake/src/common.cs:125`). The tests live in the sibling
> `alwan_dev` repo (this repo has no `tests/`; see `CMakeLists.txt:346-348`),
> and `alwan_dev/CMakeLists.txt:33` puts the library target itself in
> `ALWAN_NORMALIZE_RANGES=0` (`alwan_dev/tests/CMakeLists.txt:122` repeats it on
> the test target, where it cannot change library behaviour). No test covers the
> code path a default build takes. Both known double-offset bugs lived there:
> YCbCr, fixed 2026-08-27, and YcCbcCrc, found while writing this page and fixed
> the same way. See
> [configuration.md](../configuration.md).

> **`alwan_rgb_to_lab`, `_to_luv`, `_to_oklab` and `_to_oklch` take LINEAR RGB.**
> Each applies the space descriptor's NPM matrix and nothing else. No transfer
> function is applied or removed anywhere in the path. Passing display-encoded
> sRGB returns `ALWAN_OK` with wrong numbers. The header states the linearity
> requirement for `alwan_rgb_to_xyz` (`alwan.h:645`) and for relative luminance
> (`alwan.h:3272`), and omits it from the direct-conversion block at
> `alwan.h:766-817`.

> **The space descriptor is the SECOND argument, ahead of the colour.**
> `alwan_rgb_to_lab_{T}(lab, space, rgb, white_xyz)` and
> `alwan_lab_to_rgb_{T}(rgb, space, lab, white_xyz)`. The output pointer stays
> first and `space` stays second in both directions, so the two colour pointers
> trade slots between forward and inverse. The same shape applies to Luv, Oklab
> and Oklch.

**No colour-value clamping anywhere on this page.** No operation here clamps or
range-checks a colour channel on input or output; out-of-range input flows to
out-of-range output with `ALWAN_OK`. Several kernels do carry degenerate-input
overrides (CMY->CMYK `k >= 1`, HWB->HSV `w + b >= 1`, the HSY `max_c` floor, the
HSP hue wrap), and none of them clamps a value into a nominal range or reports
an error. Pointer and descriptor validation is a separate matter; see
[Error Codes](#error-codes). The `[0, 1]` phrasing in the header
(`alwan.h:3190`, `:3202`, `:3212`, `:3218`, `:3231`, `:4985`) describes nominal
ranges, and only the YCbCr comment says so. The gamut guarantees available are
the separate `alwan_ycbcr_to_rgb_gamut_safe_{T}` and
`alwan_yccbccrc_to_rgb_gamut_safe_{T}` entry points, whose signatures are given
under [Video Encodings](#video-encodings).

---

## Direct RGB <-> Perceptual Spaces

Four pairs that fold the XYZ step into one call. All eight take a
`alwan_rgb_space_desc_{T}`. All eight return `ALWAN_OK` or `ALWAN_E_INVALID`.
None performs chromatic adaptation.

### alwan_rgb_to_lab_{T} / alwan_lab_to_rgb_{T}

```c
alwan_status alwan_rgb_to_lab_{T}(alwan_lab_{T} *lab,
                                   alwan_rgb_space_desc_{T} const *space,
                                   alwan_rgb_{T} const *rgb,
                                   alwan_xyz_{T} const *white_xyz);
alwan_status alwan_lab_to_rgb_{T}(alwan_rgb_{T} *rgb,
                                   alwan_rgb_space_desc_{T} const *space,
                                   alwan_lab_{T} const *lab,
                                   alwan_xyz_{T} const *white_xyz);
```

**Parameters:**
- `lab` / `rgb`: output, written on success only.
- `space`: RGB space descriptor. If `space->has_matrices == 0` the NPM is
  derived from `primaries_xy` / `white_xy` on the fly (`alwan_rgb.c:100-106`).
- `rgb`: **linear** RGB, unclamped, may be negative.
- `white_xyz`: reference white for the Lab stage, used as a raw divisor.

**Ranges:** core `L` is `[0, 100]`; `a` and `b` are unbounded and never
rescaled. In a default build (`ALWAN_NORMALIZE_RANGES=1`) `alwan_rgb_to_lab`
returns `L` on `[0, 1]` and `alwan_lab_to_rgb` expects `L` on `[0, 1]`. With
`ALWAN_NORMALIZE_RANGES=0` both use `[0, 100]`.

**Output is linear RGB, unclamped**, and freely outside `[0, 1]` for a Lab value
outside the space's gamut.

> **`white_xyz` is unvalidated and unreconciled.** It is divided into XYZ with no
> zero guard (`alwan_colorspace_core.inc:42-44`), so `{0, 0, 0}` passes the null
> check and yields `inf`/`NaN` with `ALWAN_OK`. Nothing checks that it agrees
> with `space->white_xy`, and there is no chromatic adaptation between the two.
> A D65 sRGB descriptor plus a D50 white produces Lab in a hybrid reference
> frame, silently. To bridge white points, adapt the XYZ yourself; see
> [chromatic-adaptation.md](chromatic-adaptation.md).

`alwan_lab_f_{T}` uses `POW(t, 1/3)` rather than `cbrt`. `alwan_core.inc:122`
materializes the result into a local before the select at `:124`, so the `pow`
runs for every `t`, negatives included; the select then discards the `NaN`.
Results are correct, and `FE_INVALID` can be raised on strict floating-point
setups.

### alwan_rgb_to_luv_{T} / alwan_luv_to_rgb_{T}

```c
alwan_status alwan_rgb_to_luv_{T}(alwan_luv_{T} *luv,
                                   alwan_rgb_space_desc_{T} const *space,
                                   alwan_rgb_{T} const *rgb,
                                   alwan_xyz_{T} const *white_xyz);
alwan_status alwan_luv_to_rgb_{T}(alwan_rgb_{T} *rgb,
                                   alwan_rgb_space_desc_{T} const *space,
                                   alwan_luv_{T} const *luv,
                                   alwan_xyz_{T} const *white_xyz);
```

**Parameters:** identical in shape and meaning to the Lab pair above, with
`luv` in place of `lab`. Same linear-RGB requirement, same raw `white_xyz`
divisor, same absence of chromatic adaptation.

**Ranges:** core `L` is `[0, 100]` and becomes `[0, 1]` in a default build; `u`
and `v` are unbounded and never rescaled.

**Degenerate guards are asymmetric.** The forward tests
`(X + 15Y + 3Z) < ALWAN_CORE_EPSILON` on the signed denominator and forces
`u' = v' = 0` (`alwan_colorspace_core.inc:76`, `:79`), so a negative denominator
(reachable from out-of-gamut XYZ) collapses to 0 as well. The inverse tests
`ABS(L) < ALWAN_CORE_EPSILON` (`:121`, `:124`) and `ABS(v') < ALWAN_CORE_EPSILON`
(`:129`, `:133`). The two are not mirror images, and round-trips near the
collapse do not close. `ALWAN_CORE_EPSILON` is `ALWAN_EPSILON_F64` = `1e-12` for
f64 and `ALWAN_EPSILON_F32` = `1e-6f` for f32 (`alwan_platform.h:476-477`, wired
in at `alwan_core_f64_setup.h:77` and `alwan_core_f32_setup.h:77`).

### alwan_rgb_to_oklab_{T} / alwan_oklab_to_rgb_{T}

```c
alwan_status alwan_rgb_to_oklab_{T}(alwan_oklab_{T} *oklab,
                                     alwan_rgb_space_desc_{T} const *space,
                                     alwan_rgb_{T} const *rgb);
alwan_status alwan_oklab_to_rgb_{T}(alwan_rgb_{T} *rgb,
                                     alwan_rgb_space_desc_{T} const *space,
                                     alwan_oklab_{T} const *oklab);
```

**Parameters:**
- `oklab` / `rgb`: output, written on success only.
- `space`: RGB space descriptor, used for the RGB<->XYZ matrix only. Oklab fixes
  its own reference white, so the descriptor's white point selects nothing.
- `rgb`: **linear** RGB, unclamped.

No white point parameter. No `NORM`/`DENORM` on Oklab, so `L` is `[0, 1]` in
every build and `a`, `b` are unbounded. The cube root is a true signed `cbrt`
(`alwan_oklab_core.inc:47-49`), so negative LMS from out-of-gamut input does not
produce `NaN`.

> **The header comment at `alwan.h:807` claims Oklab "handles chromatic
> adaptation if needed". It does not.** The implementation is the descriptor's
> RGB->XYZ matrix followed by the fixed Ottosson M1 / cbrt / M2 chain
> (`alwan_oklab_core.inc:36-59`), three steps with no white point parameter and
> no adaptation. Passing an ACES AP0 (D60) or ProPhoto (D50) descriptor produces
> XYZ relative to that white and feeds it straight into a D65-referred
> transform. Adapt to D65 first if the descriptor is not D65.

### alwan_rgb_to_oklch_{T} / alwan_oklch_to_rgb_{T}

```c
alwan_status alwan_rgb_to_oklch_{T}(alwan_oklch_{T} *oklch,
                                     alwan_rgb_space_desc_{T} const *space,
                                     alwan_rgb_{T} const *rgb);
alwan_status alwan_oklch_to_rgb_{T}(alwan_rgb_{T} *rgb,
                                     alwan_rgb_space_desc_{T} const *space,
                                     alwan_oklch_{T} const *oklch);
```

**Parameters:** as for the Oklab pair. Composes RGB -> XYZ -> Oklab -> Oklch and
inherits the missing adaptation above.

**Hue units:** the core hue is `atan2(b, a)` in **radians** on `[-pi, pi]`
(`alwan_oklab_core.inc:98`). A default build applies `h = (h + pi) / 2pi` and
returns `[0, 1]`; the inverse applies `h = h * 2pi - pi` and accepts `[0, 1]`
(`ALWAN_NORM_OKLCH` / `ALWAN_DENORM_OKLCH`, `alwan_platform.h:1201-1202`). With
`ALWAN_NORMALIZE_RANGES=0` both use radians on `[-pi, pi]`. **Degrees are never
accepted.** `C` is unbounded. Neither direction wraps or range-checks `h`.

---

## Direct XYZ <-> Cylindrical

Six `void` functions declared at `alwan.h:825-840` under the comment "Skip the
cartesian intermediate step", with no range, unit or null-pointer note on any of
them.

> **All six dereference their arguments with no null check and crash on NULL**
> (`api/alwan_colorspace_impl.inc:100-138`). They return `void`, so there is no
> status to test.

> **Three hue conventions, three formulas, one output range.** LCh(ab) and
> LCh(uv) hue is **degrees**, folded to `[0, 360)`, normalized as `h / 360`.
> Oklch hue is **radians** on `[-pi, pi]`, normalized as `(h + pi) / 2pi`. In a
> default build all three come back on `[0, 1]`, from different native units and
> by different formulas. With `ALWAN_NORMALIZE_RANGES=0` they come back in their
> native units, and the inverses then refuse the other convention.

### alwan_xyz_to_lch_{T} / alwan_lch_to_xyz_{T}

```c
void alwan_xyz_to_lch_{T}(alwan_lch_{T} *lch,
                           alwan_xyz_{T} const *xyz,
                           alwan_xyz_{T} const *white_xyz);
void alwan_lch_to_xyz_{T}(alwan_xyz_{T} *xyz,
                           alwan_lch_{T} const *lch,
                           alwan_xyz_{T} const *white_xyz);
```

Core: `L` `[0, 100]`, `C` `[0, ~181]`, `h` degrees `[0, 360)`
(`alwan_colorspace_core.inc:145-153`). Default build: `L` `[0, 1]`, `h` `[0, 1]`,
`C` untouched (`ALWAN_NORM_LCH` / `ALWAN_DENORM_LCH`,
`alwan_platform.h:1193-1194`). The inverse applies `L *= 100` and `h *= 360`
first. No hue wrapping in the inverse: an `h` outside the range feeds `cos`/`sin`
directly.

### alwan_xyz_to_lchuv_{T} / alwan_lchuv_to_xyz_{T}

```c
void alwan_xyz_to_lchuv_{T}(alwan_lchuv_{T} *lchuv,
                             alwan_xyz_{T} const *xyz,
                             alwan_xyz_{T} const *white_xyz);
void alwan_lchuv_to_xyz_{T}(alwan_xyz_{T} *xyz,
                             alwan_lchuv_{T} const *lchuv,
                             alwan_xyz_{T} const *white_xyz);
```

Same conventions as LCh(ab): core `h` in degrees `[0, 360)`, default build `L`
and `h` on `[0, 1]`, inverse multiplies by 100 and 360
(`alwan_platform.h:1197-1198`).

### alwan_xyz_to_oklch_{T} / alwan_oklch_to_xyz_{T}

```c
void alwan_xyz_to_oklch_{T}(alwan_oklch_{T} *oklch, alwan_xyz_{T} const *xyz);
void alwan_oklch_to_xyz_{T}(alwan_xyz_{T} *xyz, alwan_oklch_{T} const *oklch);
```

**No white point parameter.** The input XYZ is assumed already D65-relative and
nothing verifies or adapts it; the output XYZ is D65-relative. Hue is radians
`[-pi, pi]` in the core and `[0, 1]` in a default build, by `(h + pi) / 2pi`.

---

## HLC (LCh reordered)

### alwan_lch_to_hlc_{T} / alwan_hlc_to_lch_{T}

```c
void alwan_lch_to_hlc_{T}(alwan_hlc_{T} *hlc, alwan_lch_{T} const *lch);
void alwan_hlc_to_lch_{T}(alwan_lch_{T} *lch, alwan_hlc_{T} const *hlc);
```

Declared at `alwan.h:1468-1471`, 600 lines away from the XYZ<->cylindrical block
and touching no XYZ. The core is a field reorder: `H = h`, `L = L`, `C = C`.

> **The API wrapper does more than reorder, and the header's ranges are the wrong
> build's.** `alwan.h:1466` says "Pure reordering of CIE LCH(ab): H=[0-360],
> L=[0-100], C=[0-~181]". The wrapper runs `ALWAN_DENORM_LCH` on the input
> (`L *= 100`, `h *= 360`) and `ALWAN_NORM_HLC` on the output (`H /= 360`,
> `L *= 0.01`) (`api/alwan_extended_impl.inc:55-69`,
> `alwan_platform.h:1327-1328`). In a default build it is therefore a reorder of
> values already on `[0, 1]`, and `H` and `L` come back on `[0, 1]`. The
> header's ranges hold only with `ALWAN_NORMALIZE_RANGES=0`.

Unlike the six `void` functions above, these two null-check and return silently
(`api/alwan_extended_impl.inc:56`, `:64`). Being `void`, they cannot report the
rejection, and the caller cannot tell it apart from success.

---

## Subtractive: CMY and CMYK

### alwan_rgb_to_cmy_{T} / alwan_cmy_to_rgb_{T}

```c
alwan_status alwan_rgb_to_cmy_{T}(alwan_cmy_{T} *cmy_out, alwan_rgb_{T} const *rgb);
alwan_status alwan_cmy_to_rgb_{T}(alwan_rgb_{T} *rgb_out, alwan_cmy_{T} const *cmy);
```

`c = 1 - r`, `m = 1 - g`, `y = 1 - b`, and the exact involution back. No
`NORM`/`DENORM`, no clamping in either direction: an `r` of `2.0` gives
`c = -1.0` with `ALWAN_OK`.

### alwan_cmy_to_cmyk_{T} / alwan_cmyk_to_cmy_{T}

```c
alwan_status alwan_cmy_to_cmyk_{T}(alwan_cmyk_{T} *cmyk_out, alwan_cmy_{T} const *cmy);
alwan_status alwan_cmyk_to_cmy_{T}(alwan_cmy_{T} *cmy_out, alwan_cmyk_{T} const *cmyk);
```

Forward: `k = min(c, m, y)`, then `c' = (c - k) / (1 - k)` for each channel.
When `k >= 1.0` the three CMY outputs are forced to exactly `0`
(`alwan_convenience_core.inc:437-439`); that case is lossless because CMY were
all `1`. The select is a ternary, so the `(c - k) / (1 - k)` arm is never
evaluated on that branch.

Inverse: `c = c * (1 - k) + k`, unconditional. No `k >= 1` special case is
needed, since the formula already yields `1` there.

Inputs are unvalidated. Negative CMY produces `k < 0` and a denominator greater
than `1`, with no error. An out-of-range CMY with `k >= 1` does not flow through
unchanged: it is rewritten to `(0, 0, 0, k)`.

---

## Video Encodings

Four pairs declared adjacently at `alwan.h:3231-3267`, with three different
chroma conventions and two opposite input domains between them.

> **YCbCr takes NON-LINEAR R'G'B'. YcCbcCrc takes LINEAR RGB.** The YCbCr kernel
> is a pure affine transform, matching the ITU definition on gamma-encoded
> signal. The YcCbcCrc kernel applies the BT.2020 OETF itself, to `Yc`, `R` and
> `B` (`G` is never encoded) (`alwan_convenience_core.inc:306-314`). The
> declarations sit 12 lines apart with no note on either. Passing the same buffer
> to both gives one correct answer and one wrong one.

> **Do not mix scalar and bulk entry points on the same data in a default
> build.** For YCoCg the scalar and `_map_interleave` paths add `+0.5` to `Co`
> and `Cg` (`api/alwan_convenience_impl.inc:157`,
> `map/alwan_convenience_extra_map_kernels.inc:135-136`); `_map_planar` adds
> nothing (`:857-861`). For YcCbcCrc the scalar path adds `+0.5` to `Cbc` and
> `Crc` (`api/alwan_convenience_impl.inc:138`); both bulk paths add nothing. The
> disagreement is exactly `0.5` in two of three channels. With
> `ALWAN_NORMALIZE_RANGES=0` all paths agree.

Both raw decoders have a gamut-safe twin. Their signatures take a fourth
argument, and the third argument differs between them:

```c
alwan_status alwan_ycbcr_to_rgb_gamut_safe_{T}(alwan_rgb_{T} *rgb_out,
                                                alwan_ycbcr_{T} const *ycbcr,
                                                alwan_ycbcr_standard standard,
                                                alwan_gamut_map_method method);
alwan_status alwan_yccbccrc_to_rgb_gamut_safe_{T}(alwan_rgb_{T} *rgb_out,
                                                   alwan_yccbccrc_{T} const *yccbccrc,
                                                   int bit_depth,
                                                   alwan_gamut_map_method method);
```

Declared at `alwan.h:3240-3241` and `:3251-3252`. Output is guaranteed in
`[0, 1]`; `ALWAN_GAMUT_MAP_CLIP` reproduces the pre-2.0 implicit clamp. For
`alwan_gamut_map_method` itself see [gamut.md](gamut.md).

### alwan_rgb_to_ycbcr_{T} / alwan_ycbcr_to_rgb_{T}

```c
alwan_status alwan_rgb_to_ycbcr_{T}(alwan_ycbcr_{T} *ycbcr_out,
                                     alwan_rgb_{T} const *rgb,
                                     alwan_ycbcr_standard standard);
alwan_status alwan_ycbcr_to_rgb_{T}(alwan_rgb_{T} *rgb_out,
                                     alwan_ycbcr_{T} const *ycbcr,
                                     alwan_ycbcr_standard standard);
```

**Parameters:**
- `ycbcr_out` / `rgb_out`: output, written on success only.
- `rgb` / `ycbcr`: input. RGB is **non-linear** R'G'B'.
- `standard`: `ALWAN_YCBCR_BT601`, `_BT709` or `_BT2020`. Selects `kr`/`kb` only.

`Y = kr*R' + kg*G' + kb*B'`, `Cb = (B' - Y) / (2(1 - kb)) + 0.5`,
`Cr = (R' - Y) / (2(1 - kr)) + 0.5`.

> **Chroma is centred on 0.5, and the offset lives in the core kernel.** The
> `+ 0.5` is applied by `alwan_rgb_to_ycbcr_kr_kb_{T}_v` itself
> (`alwan_convenience_core.inc:226-227`), and subtracted by
> `alwan_ycbcr_to_rgb_kr_kb_{T}_v` (`:236-237`), so it is present in every build
> and on every code path. `ALWAN_NORM_YCBCR` and `ALWAN_DENORM_YCBCR` are
> deliberately no-ops (`alwan_platform.h:1221-1222`) for exactly this reason.
> Achromatic grey reads `Cb = Cr = 0.5`. Do not add your own offset. Until
> 2026-08-27 the normalization layer added a second `0.5` and grey encoded to
> `1.0`.

`ALWAN_YCBCR_CB_MIN` / `CB_MAX` / `CR_MIN` / `CR_MAX` are `ALWAN_ZERO` /
`ALWAN_ONE` (`alwan_platform.h:903-906`). Full-range output is `Y` on `[0, 1]`
and `Cb`, `Cr` on `[0, 1]` centred on `0.5`.

The decode is the raw affine math with **no clamp**, so super-black,
super-white and xvYCC excursions are preserved. For guaranteed `[0, 1]` output
use `alwan_ycbcr_to_rgb_gamut_safe_{T}` above. Note that the gamut-safe path
resolves `ALWAN_YCBCR_BT601` onto the sRGB/BT.709 descriptor rather than SMPTE
170M primaries (`alwan__ycbcr_space_id`, `api/alwan_convenience.c:236-238`);
only `ALWAN_YCBCR_BT2020` gets its own space.

> **An unrecognised `alwan_ycbcr_standard` silently becomes BT.709.** The lookup
> at `alwan_internal.h:157-161` has a `default` arm that assigns the BT.709
> coefficients. No `ALWAN_E_INVALID`, no `ALWAN_E_RANGE`. An uninitialised `int`
> produces plausible BT.709 numbers with `ALWAN_OK`.

Coefficients are resolved in f64 and narrowed for the f32 entry point
(`api/alwan_convenience.c:77-80`, `:99-101`). See
[precision-and-limits.md](../precision-and-limits.md).

### alwan_ycbcr_full_to_legal_{T} / alwan_ycbcr_legal_to_full_{T}

```c
alwan_status alwan_ycbcr_full_to_legal_{T}(alwan_ycbcr_{T} *out,
                                            alwan_ycbcr_{T} const *in,
                                            int bit_depth);
alwan_status alwan_ycbcr_legal_to_full_{T}(alwan_ycbcr_{T} *out,
                                            alwan_ycbcr_{T} const *in,
                                            int bit_depth);
```

**Parameters:**
- `out`: output, written on success only.
- `in`: input. On the full-range side chroma is centred on `0.5`.
- `bit_depth`: `8`, `10`, `12` or `16`. Anything else is silently treated as
  `10`.

`Y' = Y * (y_max - y_min) + y_min`, and chroma is treated as centred on `0.5` on
the full-range side: `Cb' = (Cb - 0.5) * (c_max - c_min) + (c_max + c_min) / 2`
(`alwan_convenience_core.inc:410-413`). The inverse puts the `0.5` back. Both
`ALWAN_NORM_YCBCR` and `ALWAN_DENORM_YCBCR` are no-ops, so these behave
identically in both builds.

Legal ranges are `[16s, 235s] / (2^N - 1)` for luma and `[16s, 240s] / (2^N - 1)`
for chroma, with `s = 2^(N-8)`:

| bit_depth | y_min | y_max | c_min | c_max | chroma centre |
|---|---|---|---|---|---|
| 8  | 0.062745 | 0.921569 | 0.062745 | 0.941176 | 0.501961 |
| 10 | 0.062561 | 0.918866 | 0.062561 | 0.938416 | 0.500489 |
| 12 | 0.062515 | 0.918193 | 0.062515 | 0.937729 | 0.500122 |
| 16 | 0.062501 | 0.917983 | 0.062501 | 0.937514 | 0.500008 |

> **Any `bit_depth` other than 8, 10, 12 or 16 is silently treated as 10-bit.**
> The `switch` at `alwan_convenience_core.inc:271-277` has a `default` arm
> assigning `max_val = 1023.0`, `scale = 4.0`. `bit_depth` of `0`, `14` or `-1`
> returns `ALWAN_OK` with 10-bit legal ranges. There is no `ALWAN_E_RANGE` and
> no assertion. The same fallback applies to `alwan_rgb_to_yccbccrc_{T}` and
> `alwan_yccbccrc_to_rgb_{T}`.

Feeding `alwan_ycbcr_full_to_legal_{T}` chroma centred on `0` puts achromatic
grey at `16/255` at 8 bits, the legal minimum. The `240/255` bug the code
comment at `alwan_convenience_core.inc:407-409` records was the reverse case:
the pre-2026-08-27 code scaled `Cb` directly, as though it were centred on `0`,
so producer output at `0.5` landed on the legal maximum. Neither direction
clamps: a sub-16 code decodes to a negative `Y`.

### alwan_rgb_to_yccbccrc_{T} / alwan_yccbccrc_to_rgb_{T}

```c
alwan_status alwan_rgb_to_yccbccrc_{T}(alwan_yccbccrc_{T} *yccbccrc_out,
                                        alwan_rgb_{T} const *rgb,
                                        int bit_depth);
alwan_status alwan_yccbccrc_to_rgb_{T}(alwan_rgb_{T} *rgb_out,
                                        alwan_yccbccrc_{T} const *yccbccrc,
                                        int bit_depth);
```

**Parameters:**
- `yccbccrc_out` / `rgb_out`: output, written on success only.
- `rgb` / `yccbccrc`: input. RGB is **linear**.
- `bit_depth`: `8`, `10`, `12` or `16`, controlling legal-range scaling only.
  Anything else is silently treated as `10`.

BT.2020 constant-luminance. Coefficients are `kr = 0.2627`, `kg = 0.6780`,
`kb = 0.0593`; chroma divisors are `1.9404` / `1.5816` for `Cbc` and `1.7184` /
`0.9936` for `Crc`, selected on the sign of the difference
(`alwan_convenience_core.inc:320-325`). Output is legal-range scaled per the
table above. The inverse reverses the scaling and the BT.2020 EOTF (threshold
`4.5 * beta = 0.081`) and recovers `G` as `(Yc_linear - kr*R - kb*B) / kg`.
Output is linear RGB, unclamped.

> **The scalar path used to double-offset chroma in a default build.** The core
> already centres `Cbc`/`Crc` on `(c_max + c_min) / 2`, which is `0.500489` at
> 10 bit (`alwan_convenience_core.inc:331-332`). `ALWAN_NORM_YCCBCCRC` then added
> a further `+0.5`, so achromatic grey encoded to `Cbc = Crc = 1.000489` and
> in-gamut chroma spanned roughly `[0.5626, 1.4384]`: outside `[0, 1]`, and
> outside the `[-0.5, 0.5]` the macro comment claimed. It was the same defect
> fixed for YCbCr on 2026-08-27, missed then in the constant-luminance twin.
>
> Writing this page found it, and the macro is now a no-op like YCbCr's. Three
> things had already disagreed with the scalar path and now agree with it: the
> bulk kernels never added the offset
> (`map/alwan_convenience_extra_map_kernels.inc:613-616`, `:633-636`), the
> reference CSV holds the un-offset value
> (`alwan_dev/tests/reference_values/rgb_to_yccbccrc.csv`, first row
> `Cbc = 5.00488758553274681873e-01`), and an `ALWAN_NORMALIZE_RANGES=0` build
> produced the un-offset value all along, which is every build in either repo and
> the reason no test caught it. YCoCg is not affected: its kernel emits `Co` and
> `Cg` centred on 0, so the `+0.5` there is the correct mapping.

The OETF constants are hardcoded to `alpha = 1.099`, `beta = 0.018`
(`alwan_convenience_core.inc:303-304`, `:365-366`) for every `bit_depth`,
including 12 and 16. The core comment at `:302` calls them "the 10/12-bit
precision constants". `bit_depth` affects legal-range scaling only.

### alwan_rgb_to_ycocg_{T} / alwan_ycocg_to_rgb_{T}

```c
alwan_status alwan_rgb_to_ycocg_{T}(alwan_ycocg_{T} *ycocg_out, alwan_rgb_{T} const *rgb);
alwan_status alwan_ycocg_to_rgb_{T}(alwan_rgb_{T} *rgb_out, alwan_ycocg_{T} const *ycocg);
```

`Y = 0.25R + 0.5G + 0.25B`, `Co = 0.5(R - B)`, `Cg = -0.25R + 0.5G - 0.25B`.
Inverse: `t = Y - Cg`, `R = t + Co`, `G = Y + Cg`, `B = t - Co`, an exact inverse
in real arithmetic. No clamp.

> **The header calls this a "Reversible integer transform ... Used in H.264/AVC"
> (`alwan.h:3262-3265`). It is neither.** This is the plain lossy
> floating-point YCoCg. The reversible lifting transform H.264 specifies is
> YCoCg-R, which is absent from the library; there is no integer path anywhere.
> Round-trips are exact only to floating-point rounding.

> **The forward and inverse are declared 200 lines apart.**
> `alwan_rgb_to_ycocg_{T}` is at `alwan.h:3266-3267`;
> `alwan_ycocg_to_rgb_{T}` is at `alwan.h:3468-3469`, orphaned at the end of the
> YCbCr legal/full map block with no comment. A reader scanning the YCoCg block
> would conclude there is no scalar inverse.

**Ranges:** core `Y` `[0, 1]`, `Co` and `Cg` `[-0.5, 0.5]`
(`alwan_platform.h:908-914`). In a default build the scalar and
`_map_interleave` paths shift both to `[0, 1]` and `_map_planar` does not. The
header never mentions any chroma offset.

---

## Luma Pickers: HSY and HSPLog

### alwan_rgb_to_hsy_{T} / alwan_hsy_to_rgb_{T}

```c
alwan_status alwan_rgb_to_hsy_{T}(alwan_hsy_{T} *hsy_out, alwan_rgb_{T} const *rgb);
alwan_status alwan_hsy_to_rgb_{T}(alwan_rgb_{T} *rgb_out, alwan_hsy_{T} const *hsy);
```

`H` is taken straight from HSV, a `[0, 1]` fraction, `0` for achromatic.
`Y = 0.299R + 0.587G + 0.114B`, hardcoded BT.601 and not selectable.
`S = chroma / max_c`, where `max_c` is `Y / Y_pure` below the pure-hue luma and
`(1 - Y) / (1 - Y_pure)` above it, which is the maximum chroma reachable at that
luma and hue. No `NORM`/`DENORM` in either direction.

`S` is therefore bounded by `1` for RGB in `[0, 1]`, the domain the header
documents (`alwan.h:3202`) and the core comment repeats
(`alwan_convenience_core.inc:696`). It is not clamped, so out-of-range RGB
(negative, or above `1`) produces `S > 1`, which `alwan_hsy_to_rgb` faithfully
decodes back out of `[0, 1]`.

The inverse rebuilds the pure-hue RGB from `h * 6` (with `h == 1.0` folded back
to sector 0), scales by `chroma = S * max_c`, and offsets by
`m = Y - luma(chroma triple)`. **Output is not clamped** and leaves `[0, 1]` for
high `S`.

> **The pair is asymmetric at the extremes.** `alwan_rgb_to_hsy` divides by
> `max_c` and floors it at `1e-10` (`alwan_convenience_core.inc:783`, `:785`).
> `alwan_hsy_to_rgb` multiplies by `max_c` and omits that floor (`:809-814`), so
> near `Y = 0` or `Y = 1` the inverse produces a chroma the forward would have
> clamped away, and round-trips do not close. The inner `Y_pure` floors are
> present in both directions (`:776-779`, `:805-808`).

### alwan_rgb_to_hsplog_{T} / alwan_hsplog_to_rgb_{T}

```c
alwan_status alwan_rgb_to_hsplog_{T}(alwan_hsplog_{T} *hsplog_out, alwan_rgb_{T} const *rgb);
alwan_status alwan_hsplog_to_rgb_{T}(alwan_rgb_{T} *rgb_out, alwan_hsplog_{T} const *hsplog);
```

HSP with logarithmic saturation stretch. The forward runs `alwan_rgb_to_hsp`
(`H` and `S` from HSV, `P = sqrt(0.299R^2 + 0.587G^2 + 0.114B^2)`) and then
`s = log10(1 + 9*s)`, which maps `S = 0` to `0` and `S = 1` to `1`. The inverse
applies `s = (10^s_log - 1) / 9` and then Finley's per-sector quadratic HSP
inverse: six sectors, `v = P / sqrt(weighted denominator)`, guarded on
`denom > 0` (`alwan_convenience_core.inc:576`, `:585`, and the four siblings),
with an achromatic override when `s <= 0` (`:648-650`). Hue is wrapped modulo
360 internally (`:554-555`), so an `h` outside `[0, 1]` is accepted. Output is
unclamped.

`S` out of HSV is never negative, so the `log10` argument is never below `1`. An
RGB with a negative channel pushes `S` above `1` and the stretched `S_log` above
`1` with it; the inverse `10^s_log` then grows without bound.

**Struct field naming:** `alwan_hsplog` is
`struct { alwan_scalar h, s, p; }` (`alwan_types.h:221`). The brightness channel
is `p`. `alwan_hsp` has an identical layout (`alwan_types.h:220`), so the two
types are interchangeable at the ABI level with no diagnostic.

`alwan.h:3195` says "No published specification exists; see alwan_types.h". That
cross-reference is dangling: `alwan_types.h` carries only the two struct
definitions and no note. The explanation is at
`alwan_convenience_core.inc:655-664`.

---

## HWB (CSS Color Level 4)

### alwan_rgb_to_hwb_{T} / alwan_hwb_to_rgb_{T}

```c
alwan_status alwan_rgb_to_hwb_{T}(alwan_hwb_{T} *hwb_out, alwan_rgb_{T} const *rgb);
alwan_status alwan_hwb_to_rgb_{T}(alwan_rgb_{T} *rgb_out, alwan_hwb_{T} const *hwb);
```

Routed through HSV: `w = (1 - S) * V`, which equals `min(R, G, B)` whenever
`max(R, G, B) > 0`; `b = 1 - V = 1 - max(R, G, B)`; `h` from HSV, and `0` for
achromatic. Matches CSS Color 4. No clamp: out-of-range RGB gives out-of-range
`W`/`B`. For all-negative RGB, `alwan_rgb_to_hsv` sets `S = 0` and `w` becomes
`max(R, G, B)`. The inverse goes HWB -> HSV -> RGB and inherits the
`w + b >= 1` grey rule below; hue is wrapped modulo 360 inside `hsv_to_rgb`, so
an `h` outside `[0, 1]` is tolerated.

These two are declared at `alwan.h:4986-4989`, in a separate HWB block roughly
1500 lines below their HSV siblings at `alwan.h:3418-3421`.

### alwan_hsv_to_hwb_{T} / alwan_hwb_to_hsv_{T}

```c
alwan_status alwan_hsv_to_hwb_{T}(alwan_hwb_{T} *hwb_out, alwan_hsv_{T} const *hsv);
alwan_status alwan_hwb_to_hsv_{T}(alwan_hsv_{T} *hsv_out, alwan_hwb_{T} const *hwb);
```

The forward is exactly the documented `w = (1 - s) * v`, `b = 1 - v`, `h`
passthrough, with no clamp and no guard.

> **The inverse carries a degenerate branch the header does not mention.**
> `alwan.h:3417` gives the pair as a one-line formula. `alwan_hwb_to_hsv`
> computes `v = 1 - b` and `s = 1 - w / max(v, 1e-10)`, and when `w + b >= 1` it
> forces grey: `s = 0` and `v = w / (w + b)`. The `1e-10` floor on `v` prevents
> a divide-by-zero for near-black. The `w / (w + b)` division is itself
> unguarded, and is reached only when `w + b >= 1`
> (`alwan_convenience_core.inc:470-486`).

---

## Relative Luminance

Three entry points for `Y = kr*R + kg*G + kb*B`, one fused expression, no clamp.
Input RGB must be **linear** (scene-referred); the header states this at
`alwan.h:3272`.

### alwan_relative_luminance_{T}

```c
alwan_status alwan_relative_luminance_{T}(alwan_{T} *Y_out,
                                           alwan_rgb_{T} const *rgb,
                                           alwan_luma_standard standard);
```

Nine standards: `ALWAN_LUMA_BT601`, `BT709`, `BT2020`, `ACES_AP1`, `ACES_AP0`,
`DISPLAY_P3`, `DCI_P3`, `ADOBE_RGB`, `PROPHOTO_RGB`. This is the one of the three
that resolves its coefficients in f64 and narrows them for the f32 entry point
(`api/alwan_convenience.c:175-177`).

> **An out-of-range or unrecognised `alwan_luma_standard` silently resolves to
> BT.709** (`alwan_internal.h:217-221`). There is no error and no way to detect
> that a bad enum was passed.

> **`ALWAN_LUMA_ACES_AP0` has a negative blue coefficient**,
> `kb = -0.07213254637856078560` (`alwan_platform.h:780`), because AP0's blue
> primary has negative `y`. A saturated AP0 blue yields a negative `Y`. Nothing
> flags it.

### alwan_relative_luminance_space_{T}

```c
alwan_status alwan_relative_luminance_space_{T}(alwan_{T} *Y_out,
                                                 alwan_rgb_{T} const *rgb,
                                                 alwan_rgb_space_desc_{T} const *space);
```

Reads `kr`, `kg`, `kb` from the row-major NPM entries `m[3]`, `m[4]`, `m[5]`. The
f32 entry point reads the f32 NPM members directly, with no f64 stage
(`api/alwan_convenience.c:218-220`).

> **This is the one function here that rejects a primaries-only descriptor.** It
> requires `space->has_matrices != 0` and returns `ALWAN_E_INVALID` otherwise
> (`api/alwan_convenience.c:203`, `:216`). It will not derive the NPM from
> `primaries_xy` / `white_xy`. `alwan_rgb_to_lab` and the other direct
> conversions do derive it (`alwan_rgb.c:100-106`), so the same descriptor
> succeeds there and fails here.

To populate the descriptor, call:

```c
alwan_status alwan_rgb_derive_matrices_{T}(alwan_mat3x3_{T} *rgb_to_xyz,
                                            alwan_mat3x3_{T} *xyz_to_rgb,
                                            alwan_rgb_space_desc_{T} const *desc);
```

It writes two standalone matrices and does not touch the descriptor
(`alwan_rgb.c:44-61`, `:65-79`). The caller must assign them into
`space->rgb_to_xyz` and `space->xyz_to_rgb` and set `space->has_matrices = 1`
itself; re-passing the same descriptor without that copy-back still returns
`ALWAN_E_INVALID`.

### alwan_relative_luminance_kr_kb_{T}

```c
alwan_status alwan_relative_luminance_kr_kb_{T}(alwan_{T} *Y_out,
                                                 alwan_rgb_{T} const *rgb,
                                                 alwan_{T} kr, alwan_{T} kb);
```

> **`kg` is derived as `1 - kr - kb` and cannot be supplied**
> (`api/alwan_convenience.c:185`, `:194`), despite the header calling these
> "explicit coefficients". There is no validation that `kr` and `kb` lie in
> `[0, 1]` or that `kr + kb <= 1`. A Y row that does not sum to `1` cannot be
> expressed through this entry point, and passing one silently produces the
> wrong luminance. The nine built-in standards sum to `1` to within ~`1e-13`
> (the three ITU triples are exact; the six derived from NPM rows are not, for
> example ProPhoto at `0.99999999999992065`), so they round-trip through this
> call to that tolerance. For anything else, use
> `alwan_relative_luminance_space_{T}` with a populated NPM.

The f32 entry point derives `kg` in f32 (`api/alwan_convenience.c:194`), with no
f64 stage.

---

## Error Codes

- `ALWAN_OK` = 0 (`alwan.h:59`). The only success code, returned by every
  `alwan_status` function above.
- `ALWAN_E_INVALID` = -1 (`alwan.h:60`). The only error the conversions above
  return. Emitted for any NULL pointer argument; for `space->has_matrices == 0`
  in `alwan_relative_luminance_space_{T}` only; and for `count == 0` in every
  `_map_interleave` / `_map_planar` variant, where zero elements is an error
  rather than a no-op.
- `ALWAN_E_RANGE` = -3 (`alwan.h:62`) is **not reachable** from these
  conversions. `alwan.h:637` documents it as the failure mode of
  `alwan_rgb_derive_matrices_{T}`, which `alwan_rgb_to_lab` / `_to_luv` /
  `_to_oklab` / `_to_oklch` forward. The function never returns it:
  `alwan_rgb.c:44-61` and `:65-79` return `ALWAN_E_INVALID` for NULL and
  `ALWAN_OK` unconditionally otherwise, including for degenerate primaries that
  produce a singular matrix. Those callers get `ALWAN_OK` and `NaN`/`inf`
  matrices.
- `ALWAN_E_NODATA` (-2), `ALWAN_E_NOMEM` (-4) and `ALWAN_E_DIVZERO` (-5) are
  never returned here.
- Eight functions have no status at all: `alwan_xyz_to_lch_{T}`,
  `alwan_lch_to_xyz_{T}`, `alwan_xyz_to_lchuv_{T}`, `alwan_lchuv_to_xyz_{T}`,
  `alwan_xyz_to_oklch_{T}`, `alwan_oklch_to_xyz_{T}`, `alwan_lch_to_hlc_{T}`,
  `alwan_hlc_to_lch_{T}`. The first six crash on NULL. The last two return
  silently on NULL, indistinguishable from success.

**Bulk variants.** Tiles are processed 4096 pixels at a time on the f32 pass and
2048 on the f64 pass (`ALWAN_MAP_TILE_PIXELS`, see [simd.md](simd.md)); that is
a chunking constant rather than a size limit. Non-CLIP gamut methods in the
`_gamut_safe` bulk decoders run the scalar perceptual mapper per pixel; prefer
CLIP on hot paths (`alwan.h:3450-3452`). For the bulk argument conventions see
[map.md](map.md).


---

## See Also

- [Transfer Functions](transfer-functions.md): OETF/EOTF/view transforms
- [Chromatic Adaptation](chromatic-adaptation.md): White point transforms
- [Color Difference](color-difference.md): dE metrics
