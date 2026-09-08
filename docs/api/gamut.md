# Gamut Operations API

Functions for gamut mapping, volume calculation, and coverage analysis.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`. Both compile by default; restrict with `ALWAN_BUILD_ONLY_F32` /
> `ALWAN_BUILD_ONLY_F64` (see [configuration](../configuration.md)).

---

## Overview

Gamut operations handle colors that fall outside a color space's representable range:

- **Gamut Mapping:** Convert out-of-gamut colors to in-gamut equivalents
- **Gamut Volume:** Calculate the 3D volume of a color space
- **Gamut Coverage:** Measure how much of one gamut fits in another

Batch mappers come in three dispatch shapes, mirroring the rest of the map layer:

| Shape | Naming | Buffers |
|-------|--------|---------|
| Interleaved (AoS) | `_map_interleave` | one packed RGB buffer + byte stride |
| Planar (SoA) | `_map_planar` | three per-channel pointers + per-sample stride |
| Typed (any pixel format) | `_map_interleave_ex` / `_map_planar_ex` | `void*` buffers + `out_fmt` / `in_fmt` (`alwan_pixel_format`) |

All follow the v2.0 parameter convention: each `*_stride` immediately follows its buffer,
outputs precede inputs, `count` follows the buffer/stride block, and enums/scalars tail.

---

## Gamut Mapping Methods

```c
typedef enum {
    ALWAN_GAMUT_MAP_CLIP = 0,            /* Simple clipping to [0,1] */
    ALWAN_GAMUT_MAP_HUE_PRESERVING = 1,  /* Project to gamut boundary preserving hue */
    ALWAN_GAMUT_MAP_ADAPTIVE_L0,         /* Adaptive L0 (project toward L=0.5) */
    ALWAN_GAMUT_MAP_ADAPTIVE_CUSP,       /* Adaptive toward cusp (hue-dependent) */
    ALWAN_GAMUT_MAP_CHROMA_COMPRESS,     /* Chroma compression */
    ALWAN_GAMUT_MAP_SGCK,                /* SGCK 2004 (Segment-Maximal Gamut Clipping w/ Knee) */
    ALWAN_GAMUT_MAP_HPMINDE,             /* Hue-Preserving Minimum dE */
    ALWAN_GAMUT_MAP_LIGHTNESS_PRESERVE   /* Lightness Preserving */
} alwan_gamut_map_method;
```

---

## Functions

### alwan_gamut_{T}_map_interleave

```c
int alwan_gamut_{T}_map_interleave(
    alwan_{T} *rgb_out, size_t out_stride,
    alwan_{T} const *rgb_in, size_t in_stride,
    size_t count,
    alwan_gamut_map_method method);
```

Maps interleaved (AoS) RGB triplets to the `[0,1]` gamut using the specified method.
Strides are in **bytes** (typically `3 * sizeof(alwan_{T})` for packed data).

**Example:**
```c
alwan_rgb_{T} rgb_in  = {1.2, 0.5, -0.1};  /* out of gamut */
alwan_rgb_{T} rgb_out;
alwan_gamut_{T}_map_interleave(
    (alwan_{T}*)&rgb_out, sizeof(alwan_rgb_{T}),
    (alwan_{T} const*)&rgb_in, sizeof(alwan_rgb_{T}),
    1, ALWAN_GAMUT_MAP_CLIP);
/* rgb_out = {1.0, 0.5, 0.0} */
```

---

### alwan_gamut_{T}_map_planar

```c
int alwan_gamut_{T}_map_planar(
    alwan_{T} *out_ch0, size_t out_stride,
    alwan_{T} *out_ch1, alwan_{T} *out_ch2,
    alwan_{T} const *in_ch0, size_t in_stride,
    alwan_{T} const *in_ch1, alwan_{T} const *in_ch2,
    size_t count,
    alwan_gamut_map_method method);
```

Planar (SoA) variant: separate per-channel pointers. `out_stride` / `in_stride` are the
per-sample strides (in bytes) within each channel plane.

---

### alwan_gamut_map_interleave_ex / alwan_gamut_map_planar_ex

```c
int alwan_gamut_map_interleave_ex(
    void *rgb_out, size_t out_stride,
    void const *rgb_in, size_t in_stride,
    size_t count,
    alwan_pixel_format out_fmt,
    alwan_gamut_map_method method,
    alwan_pixel_format in_fmt);

int alwan_gamut_map_planar_ex(
    void *out0, size_t out_stride, void *out1, void *out2,
    void const *in0, size_t in_stride, void const *in1, void const *in2,
    size_t count,
    alwan_pixel_format out_fmt,
    alwan_gamut_map_method method,
    alwan_pixel_format in_fmt);
```

Typed variants accepting any `alwan_pixel_format` for input and output
(`U8`, `U16`, `F16`, `F32`, `F64`), dispatched at runtime by format.

---

### alwan_gamut_map_advanced_{T}

```c
int alwan_gamut_map_advanced_{T}(alwan_rgb_{T} *rgb_out,
                                  alwan_gamut_map_method method,
                                  alwan_rgb_space_desc_{T} const *space,
                                  alwan_rgb_{T} const *rgb_linear);
```

Single-color advanced gamut mapping with awareness of the target RGB space's gamut boundary.
`rgb_linear` is linear (not gamma-corrected); output is guaranteed in `[0,1]`.

---

### alwan_gamut_map_xyz_to_rgb_{T}

```c
int alwan_gamut_map_xyz_to_rgb_{T}(alwan_rgb_{T} *rgb_out,
                                   alwan_rgb_space_desc_{T} const *space,
                                   alwan_xyz_{T} const *xyz_in,
                                   alwan_ctx *ctx);
```

Map an XYZ color into the target RGB gamut with hue preservation (in JCh). `ctx` may be `NULL`.

---

### CSS Color Level 4 section 13.2 (OKLCh binary search)

```c
int alwan_css_gamut_{T}_map_interleave(
    alwan_{T} *rgb_out, size_t out_stride,
    alwan_{T} const *rgb_in, size_t in_stride,
    size_t count);

int alwan_css_gamut_{T}_map_planar(
    alwan_{T} *out_ch0, size_t out_stride,
    alwan_{T} *out_ch1, alwan_{T} *out_ch2,
    alwan_{T} const *in_ch0, size_t in_stride,
    alwan_{T} const *in_ch1, alwan_{T} const *in_ch2,
    size_t count);

int alwan_css_gamut_map_interleave_ex(
    void *rgb_out, size_t out_stride,
    void const *rgb_in, size_t in_stride,
    size_t count,
    alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

int alwan_css_gamut_map_planar_ex(
    void *out0, size_t out_stride, void *out1, void *out2,
    void const *in0, size_t in_stride, void const *in1, void const *in2,
    size_t count,
    alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
```

Implements the CSS Color Level 4 section 13.2 gamut-mapping algorithm: binary search on OKLCh
chroma with a `deltaEOK` JND criterion (threshold 0.02). Maps out-of-gamut **linear sRGB**
to in-gamut linear sRGB. No `method` parameter; the algorithm is fixed by the spec.

---

### alwan_gamut_volume_{T}

```c
int alwan_gamut_volume_{T}(alwan_{T} *volume,
                           alwan_rgb_space_desc_{T} const *space);
```

Returns the **exact** RGB gamut volume in linear XYZ (in XYZ units cubed). The RGB
unit cube maps to a parallelepiped under the RGB->XYZ matrix `M`, whose volume is
exactly `|det(M)|`, a closed-form result rather than a stochastic estimate.

> A *perceptual* gamut volume (the gamut's image in a nonlinear space such as
> Lab/Oklab) has no closed form and would require Monte Carlo sampling; that is a
> separate, currently unimplemented operation. An earlier `alwan_gamut_volume_mc`
> with `num_samples`/`seed` parameters advertised sampling it never performed;
> it has been renamed to `alwan_gamut_volume` and the dead parameters removed.

**Example:**
```c
alwan_rgb_space_desc_{T} srgb_desc;
alwan_rgb_get_space_descriptor_{T}(&srgb_desc, ALWAN_RGB_SPACE_SRGB, ctx);

alwan_{T} volume;
alwan_gamut_volume_{T}(&volume, &srgb_desc);
```

> The numerical estimators (`alwan_gamut_volume_ratio`, `alwan_gamut_coverage`)
> run their reduction in `f64` internally even in an `f32`-only build
> (`ALWAN_WITH_F64_FACADE`), so they remain available everywhere. This is a
> design choice rather than a missing native-f32 path. See
> [configuration.md](../configuration.md). (`alwan_gamut_volume`
> itself is an exact determinant and equally cheap in either precision.)

---

### alwan_gamut_volume_ratio_{T}

```c
int alwan_gamut_volume_ratio_{T}(alwan_{T} *ratio_out,
                                 alwan_rgb_space_desc_{T} const *space1,
                                 alwan_rgb_space_desc_{T} const *space2);
```

Computes `volume(space1) / volume(space2)`.

---

### alwan_gamut_coverage_{T}

```c
int alwan_gamut_coverage_{T}(alwan_{T} *coverage_out,
                             alwan_rgb_space_desc_{T} const *space1,
                             alwan_rgb_space_desc_{T} const *space2,
                             size_t num_samples,
                             unsigned int seed);
```

Estimates what percentage (`[0, 100]`) of `space1`'s gamut is covered by `space2` using
Monte Carlo sampling.

---

### Pointer's Gamut

```c
int alwan_is_within_pointer_gamut_{T}(alwan_vec2_{T} const *xy);
```

Test whether an xy chromaticity falls within Pointer's gamut (the gamut of real surface colors).

---

### Dominant Wavelength and Excitation Purity

```c
int alwan_spectral_locus_xy_{T}(alwan_vec2_{T} *xy_out, alwan_{T} wavelength);

int alwan_dominant_wavelength_{T}(alwan_{T} *wavelength_out,
                                  alwan_vec2_{T} *xy_wl_out, alwan_vec2_{T} *xy_cw_out,
                                  alwan_vec2_{T} const *xy, alwan_vec2_{T} const *xy_white);

int alwan_excitation_purity_{T}(alwan_{T} *purity_out,
                                alwan_vec2_{T} const *xy, alwan_vec2_{T} const *xy_white);

int alwan_complementary_wavelength_{T}(alwan_{T} *wavelength_out,
                                       alwan_vec2_{T} *xy_wl_out, alwan_vec2_{T} *xy_cw_out,
                                       alwan_vec2_{T} const *xy, alwan_vec2_{T} const *xy_white);
```

---

## Manual Gamut Checking

Check if RGB is within [0,1] bounds:

```c
static int is_in_gamut(alwan_rgb_{T} const *rgb) {
    return rgb->r >= 0.0 && rgb->r <= 1.0 &&
           rgb->g >= 0.0 && rgb->g <= 1.0 &&
           rgb->b >= 0.0 && rgb->b <= 1.0;
}
```

---

## Spatial Gamut Mapping (experimental)

### alwan_gamut_map_spatial_{T}

```c
alwan_status alwan_gamut_map_spatial_{T}(alwan_{T} *out, alwan_{T} const *in,
                                         alwan_{T} const *depth,
                                         int width, int height,
                                         alwan_gamut_spatial_params_{T} const *params,
                                         alwan_ctx *ctx);
```

Whole-image gamut mapping. Unlike the per-pixel mappers above, this one sees the
image: it reconstructs a field over the frame rather than mapping each pixel in
isolation, which is what lets it preserve the polarity of local contrast instead
of flattening it.

**Parameters:**
- `out`, `in` -- `width * height * 3` interleaved RGB
- `depth` -- optional `width * height` depth or flux field, may be `NULL`
- `params` -- method and its parameters, see below
- `ctx` -- context, may be `NULL`

```c
typedef struct {
    alwan_gamut_formation_method method;
    alwan_{T} s, reach, beta, compress, depth_sigma;
    int iterations;
    alwan_{T} peak;
} alwan_gamut_spatial_params_{T};
```

The meaning of `s` depends on `method`: for the carrier family
(DENSITY / CARRIER / XJUNCTION / FLUX) it is applied per pixel, so it cannot
reintroduce `max(RGB)` carrier flips. The header carries the per-method note.

> **Experimental.** This is a research surface, not a settled one. The method
> enum is expected to grow and the parameter meanings are not frozen. It is
> exported so the work is usable, not because the API is stable.

Background, the constraint set it is measured against, and the reasoning behind
each method are in [picture_formation.md](../picture_formation.md) and
[gamut_spatial_formation.md](../gamut_spatial_formation.md).

---

## Bulk Gamut Checking

### alwan_gamut_{T}_map_interleave / alwan_gamut_{T}_map_planar

```c
alwan_status alwan_gamut_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                            alwan_{T} const *in, size_t in_stride,
                                            size_t count, ...);
alwan_status alwan_gamut_{T}_map_planar(...);
```

Strided bulk forms of the per-pixel gamut mapping above. Stride and buffer
conventions follow [map.md](map.md); the strides are in **bytes**, not elements.

---

## Gamut Mapping and the No-Silent-Clamp Variants

v2.0 split every implicit post-conversion clamp into two entry points: a raw one that
preserves out-of-range excursions, and a `_gamut_safe` / `_unclamped` / `_display_linear`
sibling that decides what happens to them. This section documents the siblings, and the
places where the header comment claims more than the implementation delivers. The raw
twins live on [color-vision-deficiency.md](color-vision-deficiency.md),
[vision.md](vision.md), [hdr.md](hdr.md), [aces.md](aces.md) and
[color-spaces.md](color-spaces.md); [ranges.md](../ranges.md) covers
`ALWAN_NORMALIZE_RANGES` and the NORM / DENORM macros, and
[gamut_mapping.md](../gamut_mapping.md) carries the boundary-model honesty notes
referenced below.

> **Two entries below sit outside the Error Codes table further down.**
> `alwan_hdr_gamut_map_jzczhz_{T}` returns `void` and has no error channel;
> `alwan_pointer_gamut_boundary` returns a non-NULL pointer to static storage and cannot
> fail.

> **The `[0,1]` guarantee holds only for `ALWAN_GAMUT_MAP_CLIP`.** It is printed in every
> header comment that introduces the `_gamut_safe` twins (above the raw YCbCr and
> YcCbcCrc decoders, above the bulk twins, and above the CVD twins), and it is repeated
> for `alwan_gamut_map_advanced_{T}` earlier on this page. The six Oklab methods all
> route through `alwan_gamut_map_advanced_{T}`, which returns near-achromatic input
> verbatim with `ALWAN_OK` and no clamp at all when Oklab chroma `C < 0.0001`. Grey is
> near-achromatic by construction: `(1.05, 1.05, 1.05)` has Oklab `C = 3.79e-8`,
> `(-0.1, -0.1, -0.1)` has `C = 1.73e-8`. So super-white and super-black pass straight
> through. `alwan_ycbcr_to_rgb_gamut_safe_f64` on `Y = 1.05, Cb = Cr = 0.5`,
> `ALWAN_YCBCR_BT709`, `ALWAN_GAMUT_MAP_CHROMA_COMPRESS` returns `(1.05, 1.05, 1.05)` and
> `ALWAN_OK`. The shipped tests call the `_gamut_safe` entries only with
> `ALWAN_GAMUT_MAP_CLIP`, which is why this survived.

> **`ALWAN_GAMUT_MAP_HUE_PRESERVING` (value 1) cannot be used with any `_gamut_safe`
> function.** `alwan_gamut_map_advanced_{T}` implements CLIP, ADAPTIVE_L0, ADAPTIVE_CUSP,
> CHROMA_COMPRESS, SGCK, HPMINDE and LIGHTNESS_PRESERVE, and returns `ALWAN_E_INVALID`
> for HUE_PRESERVING. The batch mappers at the top of this page
> (`alwan_gamut_{T}_map_interleave` / `_map_planar`) have the complementary restriction:
> they accept CLIP and HUE_PRESERVING only, and return `ALWAN_E_INVALID` for the six
> Oklab methods. The two families cover disjoint halves of one enum, and the enum
> comment describes all eight as if they worked everywhere.

| Method | value | `alwan_gamut_map_advanced_{T}` and every `_gamut_safe` | `alwan_gamut_{T}_map_interleave` / `_map_planar` |
|--------|-------|------------------------------------------------------|---------------------------------------------------|
| `ALWAN_GAMUT_MAP_CLIP` | 0 | supported | supported |
| `ALWAN_GAMUT_MAP_HUE_PRESERVING` | 1 | `ALWAN_E_INVALID` | supported |
| `ALWAN_GAMUT_MAP_ADAPTIVE_L0` | 2 | supported | `ALWAN_E_INVALID` |
| `ALWAN_GAMUT_MAP_ADAPTIVE_CUSP` | 3 | supported | `ALWAN_E_INVALID` |
| `ALWAN_GAMUT_MAP_CHROMA_COMPRESS` | 4 | supported | `ALWAN_E_INVALID` |
| `ALWAN_GAMUT_MAP_SGCK` | 5 | supported | `ALWAN_E_INVALID` |
| `ALWAN_GAMUT_MAP_HPMINDE` | 6 | supported | `ALWAN_E_INVALID` |
| `ALWAN_GAMUT_MAP_LIGHTNESS_PRESERVE` | 7 | supported | `ALWAN_E_INVALID` |

---

### The shared engine behind every `_gamut_safe` entry

The header declares nine `_gamut_safe` entry points (18 concrete f32 / f64 symbols): the
YCbCr and YcCbcCrc decoders plus their `_map_interleave` twins, the Brettel, Machado and
`_ex` CVD simulators, and the Brettel and Machado bulk twins. All nine do the same two
steps: run the raw conversion, then call `alwan_gamut_map_advanced_{T}` in place against a
fixed RGB space descriptor chosen by the entry point. Everything below is a property of
that mapper and therefore applies to all of them.

- **Near-achromatic bypass.** Oklab `C < 0.0001` returns the input unmodified with
  `ALWAN_OK`. So does any input already inside `[0,1]^3`. Only the six Oklab methods
  reach this test: CLIP short-circuits into the per-channel clamp before it, and
  HUE_PRESERVING is rejected after it.
- **The boundary model is sRGB, always.** The mapper converts the color to linear sRGB,
  projects against the hard-coded sRGB cusp/intersection fit, converts back, and only
  then clamps into the caller's cube. Feeding it a BT.2020 descriptor does not give a
  BT.2020 boundary: a wide-gamut color is desaturated all the way to the sRGB gamut, then
  clipped in BT.2020. The header notes this once, in the method table's honesty notes,
  and at none of the `_gamut_safe` declarations.
- **`desc->has_matrices` is ignored.** `alwan_rgb_derive_matrices_{T}` re-derives from
  `primaries_xy` / `white_xy` on every call and never reads `has_matrices`, so a
  CAT-adapted or measured NPM supplied in the descriptor is discarded silently. This is
  asymmetric with `alwan_rgb_to_xyz_{T}`, which does branch on `has_matrices`.
- **Identity snap at 1e-3.** If every element of the composed target->sRGB matrix is
  within `1e-3` of identity, it is replaced by exact identity. A space whose primaries
  merely resemble sRGB is treated as sRGB.
- **Degenerate primaries give a wrong-but-finite transform.** The derivation always
  reports `ALWAN_OK`, and `alwan_mat3_inv` substitutes exact identity when
  `|det| < ALWAN_CORE_EPSILON` instead of dividing by zero, so there is no diagnostic and
  no inf. NaN-valued primaries are the exception: the singularity comparison itself fails
  and NaN propagates.
- **Terminal clamp.** The boundary parameter `t` is clamped to `[0,1]` and the result is
  hard-clamped per channel to `[0,1]` in the caller's space, after the projection.
- **Cost.** The mapper derives its matrices per call. The bulk `_gamut_safe` twins hoist
  the space-descriptor lookup out of the loop, but not the matrix derivation inside the
  mapper, so per-pixel matrix work is paid per pixel in every form.

---

### alwan_ycbcr_to_rgb_gamut_safe_{T} / alwan_ycbcr_to_rgb_gamut_safe_{T}_map_interleave

```c
alwan_status alwan_ycbcr_to_rgb_gamut_safe_{T}(
    alwan_rgb_{T} *rgb_out,
    alwan_ycbcr_{T} const *ycbcr,
    alwan_ycbcr_standard standard,
    alwan_gamut_map_method method);

alwan_status alwan_ycbcr_to_rgb_gamut_safe_{T}_map_interleave(
    alwan_{T} *rgb_out, size_t out_stride,
    alwan_{T} const *ycbcr_in, size_t in_stride,
    size_t count,
    alwan_ycbcr_standard standard,
    alwan_gamut_map_method method);
```

Raw `alwan_ycbcr_to_rgb_{T}` decode, then `alwan_gamut_map_advanced_{T}` in place.

**Parameters:**
- `ycbcr` -- `Y` on `[0,1]`, `Cb` / `Cr` on `[0,1]` centered at `0.5`.
  `ALWAN_DENORM_YCBCR` has been a no-op since 2026-08-27, so scalar and bulk agree on
  this convention.
- `standard` -- selects the decode matrix, and (indirectly) the mapping primaries.
- `method` -- see the support table above; `ALWAN_GAMUT_MAP_HUE_PRESERVING` returns
  `ALWAN_E_INVALID`.
- `out_stride` / `in_stride` -- **bytes**. `out_stride` must address contiguous
  `alwan_rgb_{T}` triplets; the mapping pass re-reads `rgb_out` as
  `(char*)rgb_out + i * out_stride` cast to `alwan_rgb_{T}*`.
- `count` -- bulk form only.

> **BT.601 is gamut-mapped in sRGB/BT.709 primaries.** The space selector maps
> `ALWAN_YCBCR_BT2020` to `ALWAN_RGB_SPACE_BT2020` and collapses both
> `ALWAN_YCBCR_BT601` and `ALWAN_YCBCR_BT709` onto `ALWAN_RGB_SPACE_SRGB`. SMPTE 170M /
> EBU content is mapped against BT.709 primaries. For the BT.2020 case the mapping still
> runs against the sRGB boundary (see the engine notes above), so out-of-range BT.2020
> color is pulled in to sRGB and then clipped in BT.2020.

The bulk form runs the SIMD raw decode first, then a scalar mapping pass over the output
buffer. It returns `ALWAN_E_INVALID` for `count == 0` (the scalar form has no such case),
and the per-pixel loop returns on the first mapping error, leaving earlier pixels mapped
and the remainder holding raw decode values.

---

### alwan_yccbccrc_to_rgb_gamut_safe_{T} / alwan_yccbccrc_to_rgb_gamut_safe_{T}_map_interleave

```c
alwan_status alwan_yccbccrc_to_rgb_gamut_safe_{T}(
    alwan_rgb_{T} *rgb_out,
    alwan_yccbccrc_{T} const *yccbccrc,
    int bit_depth,
    alwan_gamut_map_method method);

alwan_status alwan_yccbccrc_to_rgb_gamut_safe_{T}_map_interleave(
    alwan_{T} *rgb_out, size_t out_stride,
    alwan_{T} const *yccbccrc_in, size_t in_stride,
    size_t count,
    int bit_depth,
    alwan_gamut_map_method method);
```

Constant-luminance BT.2020 decode, then mapping into `ALWAN_RGB_SPACE_BT2020`. The core
decode undoes the bit-depth legal-range scaling, undoes the asymmetric BT.2020-CL chroma
divisors, applies the BT.2020 EOTF to `Yc` / `R'` / `B'` and reconstructs `G` from
constant luminance, so the RGB handed to the mapper is scene-linear BT.2020.

**Parameters:**
- `yccbccrc` -- legal range, `Yc` on `[16s, 235s] / (2^N - 1)` and chroma on
  `[16s, 240s] / (2^N - 1)` with `s = 2^(N-8)`. See the centering note below.
- `bit_depth` -- `8`, `10`, `12` or `16`. Every other value falls through to the 10-bit
  legal range (`max_val = 1023`, `scale = 4`) with no error and no diagnostic: passing
  `14` returns `ALWAN_OK` with 10-bit scaling applied to 14-bit data.
- `method` -- see the support table above.
- `out_stride` / `in_stride` -- **bytes**, with the same aliasing requirement as the
  YCbCr twin.
- `count` -- bulk form only.

> **The scalar and bulk entry points disagree on chroma centering by exactly 0.5.** The
> scalar path applies `ALWAN_DENORM_YCCBCCRC`, which subtracts `0.5` from `Cbc` and `Crc`
> on top of a core that already re-centers chroma at `(c_max + c_min) / 2` (about
> `0.5005` at 10-bit). The bulk kernel applies no offset. In the default build
> (`ALWAN_NORMALIZE_RANGES = 1`) neutral grey is `Cbc = Crc ~ 1.0005` for
> `alwan_rgb_to_yccbccrc_{T}` and `alwan_yccbccrc_to_rgb_gamut_safe_{T}`, and
> `Cbc = Crc ~ 0.5005` for the `_map_interleave` twins. Data produced by
> `alwan_rgb_to_yccbccrc_{T}` cannot be fed to the bulk decoder, and vice versa. YCbCr's
> equivalent macros were made no-ops on 2026-08-27; the YcCbcCrc ones were not.

The bulk form returns `ALWAN_E_INVALID` for `count == 0` and aborts mid-buffer on the
first per-pixel mapping error, as above.

---

### alwan_simulate_cvd_gamut_safe_{T} / alwan_simulate_cvd_machado_gamut_safe_{T} / alwan_simulate_cvd_ex_gamut_safe_{T}

```c
alwan_status alwan_simulate_cvd_gamut_safe_{T}(
    alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in,
    alwan_cvd_type cvd_type, alwan_{T} severity,
    alwan_gamut_map_method method);

alwan_status alwan_simulate_cvd_machado_gamut_safe_{T}(
    alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in,
    alwan_cvd_type cvd_type, alwan_{T} severity,
    alwan_gamut_map_method method);

alwan_status alwan_simulate_cvd_ex_gamut_safe_{T}(
    alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in,
    alwan_cvd_type cvd_type, alwan_{T} severity,
    alwan_cvd_model model,                 /* NOTE: model BEFORE method */
    alwan_gamut_map_method method);

alwan_status alwan_simulate_cvd_gamut_safe_{T}_map_interleave(
    alwan_{T} *rgb_out, size_t out_stride,
    alwan_{T} const *rgb_in, size_t in_stride,
    size_t count,
    alwan_cvd_type cvd_type, alwan_{T} severity,
    alwan_gamut_map_method method);

alwan_status alwan_simulate_cvd_machado_gamut_safe_{T}_map_interleave(
    alwan_{T} *rgb_out, size_t out_stride,
    alwan_{T} const *rgb_in, size_t in_stride,
    size_t count,
    alwan_cvd_type cvd_type, alwan_{T} severity,
    alwan_gamut_map_method method);
```

All five run the raw simulation, then the same fixup against `ALWAN_RGB_SPACE_SRGB`.
`alwan_simulate_cvd_ex_gamut_safe_{T}` has no `_map_interleave` twin, and no `_gamut_safe`
planar form exists in any of the five families.

**Parameters:**
- `rgb_in` -- linear sRGB for the Brettel paths. The Machado encoding is not established
  in-tree, see below.
- `cvd_type` -- `ALWAN_CVD_*`. The `*ANOPIA` and `*ANOMALY` values of one axis share a
  case label, so they select the same kernel.
- `severity` -- `[0,1]`, clamped rather than rejected.
- `model` -- `_ex` only, and it comes **before** `method`.
- `method` -- see the support table above.
- `out_stride` / `in_stride` -- **bytes**; `count` -- bulk forms only.

> **`alwan_simulate_cvd_ex_gamut_safe_{T}` takes `model` before `method`.** Every other
> `_gamut_safe` function ends in `method`. Both are enums and convert implicitly, so a
> swapped call compiles: at best it returns `ALWAN_E_INVALID`, at worst it runs the wrong
> CVD model with a valid-looking method value.

> **`severity` reaches the dichromacy types too**, contradicting the header's "only
> applies to anomalous trichromacy types". `ALWAN_CVD_PROTANOPIA` and
> `ALWAN_CVD_PROTANOMALY` are the same case label calling the same function, and so are
> the deutan and tritan pairs. What `severity` then does differs by model: the Brettel
> kernel LERPs the simulated color against the original by `severity`, so
> `severity = 0.5` with `ALWAN_CVD_PROTANOPIA` gives a half-strength simulation. The
> Machado kernel is a bare matrix product with no blend, and `severity` is only a
> coordinate into its 11-matrix table. Both return the input unchanged at
> `severity = 0`, Brettel through the LERP and Machado because table node 0 is the
> identity matrix.

`severity` is clamped to `[0,1]` rather than rejected: Brettel clamps directly, Machado
clamps through the table coordinate gate, which also resolves NaN to `0`. Machado uses 11
severity nodes at `0.0 .. 1.0` in `0.1` steps, linearly interpolated; the bulk Machado
form interpolates the matrix once outside the loop.

Error behavior differs between the scalar and bulk forms, and between the two bulk forms:

| Case | scalar Brettel / Machado / ex | bulk Brettel | bulk Machado |
|------|-------------------------------|--------------|--------------|
| `cvd_type` outside the enum | `ALWAN_E_INVALID` | **copies input, returns `ALWAN_OK`** | `ALWAN_E_INVALID` |
| `model` outside the enum (`_ex` only) | `ALWAN_E_INVALID` | n/a | n/a |
| `count == 0` | n/a | `ALWAN_E_INVALID` | `ALWAN_E_INVALID` |

So `alwan_simulate_cvd_gamut_safe_{T}_map_interleave` with a bad `cvd_type` gamut-maps
unsimulated pixels and reports success.

The header calls the Machado matrices sRGB->sRGB. The shipped CSVs are the paper's
published matrices and the repo carries no generator for them, so nothing in-tree
establishes the encoding. The gamut fixup that both paths share assumes **linear** sRGB
(the mapper's own parameter is named `rgb_linear`). If the Machado matrices are the
paper's gamma-encoded operators, then `alwan_simulate_cvd_machado_gamut_safe_{T}` with a
non-CLIP method runs an Oklab mapper over gamma-encoded values.

---

### alwan_css_gamut_space_{T}

```c
alwan_status alwan_css_gamut_space_{T}(
    alwan_rgb_{T} *rgb_out,
    alwan_rgb_space_desc_{T} const *target_space,
    alwan_rgb_{T} const *rgb_in);
```

CSS Color 4 section 13.2 chroma reduction with the destination gamut taken from
`target_space` instead of hard-wired sRGB. There is no `_map_interleave` twin.

**Parameters:**
- `rgb_out`, `rgb_in` -- linear target-space RGB. A NULL argument returns
  `ALWAN_E_INVALID`, the only error this function reports.
- `target_space` -- destination gamut. `has_matrices` is ignored, and both the target and
  sRGB matrices are re-derived on every call (two derivations plus an sRGB descriptor
  lookup). Degenerate primaries behave as in the engine notes above: identity
  substitution, no diagnostic.

Order of operations: exact `[0,1]` passthrough test in the target cube, then Oklch via
the linear-sRGB pivot, then the two lightness shortcuts, then at most `MAX_ITER = 30`
chroma halvings, then an unconditional per-channel clip into the target cube. A halving
stops early only when the trial is within a `deltaEOK` JND of `0.02` of its own clipped
version **and** inside the target cube.

> **Two lightness shortcuts run before any search, and the header mentions neither.**
> Oklab `L >= 1.0` returns pure white `(1,1,1)`; `L <= 0.0` returns pure black `(0,0,0)`.
> Hue, chroma and how far outside the cube the input was are all discarded. In a wide
> target space such as Rec.2020, any color whose Oklab lightness reaches `1.0` collapses
> to the target's white point.

> **The search is a halving, not a bisection.** Both arms of the JND test assign
> `hi = trial.C`; `lo` is set to `0` before the loop and never advanced. The result is
> the first chroma in the sequence `C/2, C/4, C/8, ...` that passes both break
> conditions, which is systematically under-saturated relative to the true boundary
> chroma. The `hi - lo < 1e-12` guard therefore reduces to `C/2^k < 1e-12`, out of reach
> within 30 iterations for any `C` above about `1.1e-3`, so the loop runs all 30
> iterations unless the in-cube test breaks it. The sRGB-hardwired core mapper has the
> same shape, so the two agree with each other; the description of this algorithm as a
> binary search earlier on this page matches neither.

The header claims that "with the sRGB descriptor this matches `alwan_css_gamut_*` on
linear values". Three mechanical differences prevent bit-exact agreement:

1. In-gamut test: `alwan_css_gamut_space_{T}` compares against exact `0.0` / `1.0`, while
   the core `gamut_css_map_v` uses `+/- ALWAN_CORE_EPSILON` (`1e-12` in f64, `1e-6` in
   f32).
2. JND test: `de < JND` here against `de - JND < EPSILON` in the core.
3. `alwan_css_gamut_space_f32` is a widening facade over the f64 worker (and zeroes
   `has_matrices` while widening), whereas `alwan_css_gamut_f32_map_interleave` runs a
   native f32 SIMD kernel.

---

### alwan_hdr_gamut_map_ictcp_{T}

```c
alwan_status alwan_hdr_gamut_map_ictcp_{T}(
    alwan_rgb_{T} *rgb_out,
    alwan_rgb_{T} const *rgb_linear,
    alwan_{T} peak_nits);
```

Chroma reduction in ICtCp (PQ) into the display volume `[0, peak_nits]^3`.

**Parameters:**
- `rgb_linear` -- linear BT.2020 in absolute cd/m2. Input with all three channels already
  in `[0, peak_nits]` is a bit-exact passthrough.
- `peak_nits` -- accepted range `[1.0, 10000.0]` inclusive; outside returns
  `ALWAN_E_INVALID`. A NaN `peak_nits` passes the check (both comparisons are false) and
  produces garbage with `ALWAN_OK`.

None of the operating constants appear in the header. The search is exactly 32 bisection
iterations on the joint `Ct` / `Cp` scale `s` in `[0,1]`. The dE-ITP early-out threshold
is exactly `1.0`: if the naive per-channel clip is within `1.0` dE-ITP of the target, it
is returned directly. The in-volume tolerance is `peak * 1e-9` in f64 against
`peak * 1e-6` in f32, three orders of magnitude apart.

> **"Output guaranteed in `[0, peak]`" holds for finite input only.** A NaN channel fails
> the fast-path range test, propagates through the PQ round trip, and survives the final
> clamp because both `best.r < 0.0` and `best.r > peak_nits` are false for NaN. NaN is
> returned with `ALWAN_OK`.

> **Negative input channels are only partly discarded before the search.** The PQ OETF
> floors its argument at zero, but it is applied to the LMS components after the
> RGB->LMS matrix, not to the RGB channels. A negative R feeds all three LMS mixes and is
> clipped only where the mixed component itself goes negative, so for most inputs the
> negativity survives into the ICtCp target the search aims at. The winning candidate is
> hard-clipped to `[0, peak_nits]` either way.

---

### alwan_hdr_gamut_map_jzczhz_{T}

```c
void alwan_hdr_gamut_map_jzczhz_{T}(
    alwan_jzczhz_{T} *out,
    alwan_jzczhz_{T} const *in,
    alwan_{T} Cz_max);
```

**Parameters:**
- `out`, `in` -- `Jz` and `hz` are copied verbatim; only `Cz` is transformed. A NULL `out`
  or `in` is a silent no-op that leaves `*out` untouched.
- `Cz_max` -- maximum chroma at the input's `(Jz, hz)` for the target gamut. A negative
  `Cz_max` returns a negative chroma: the `1e-10` floor guards the divisor only, and the
  multiplier uses the original value.

`Cz` is transformed as:

```
ratio = Cz / max(Cz_max, 1e-10)
Cz_out = (ratio <= 1) ? Cz
                      : (1 + 0.1 * tanh(ratio - 1)) * Cz_max
```

> **The output is not bounded by `Cz_max`.** Above the boundary the result rises
> monotonically toward `1.1 * Cz_max`, so a color handed its exact gamut-boundary chroma
> comes back up to 10% outside the gamut. Below the boundary there is no compression at
> all: `Cz` is returned bit-identical. This is a hard knee with 10% overshoot. Do not
> treat the result as in-gamut.

This is the only function in this cluster with no error channel: it returns `void`. (Its
neighbor `alwan_jzazbz_to_jzczhz_{T}` is also `void` and does not null-check at all.)

The header documents `hz` as "hue in radians". In the default build
(`ALWAN_NORMALIZE_RANGES = 1`) the public producers of `alwan_jzczhz_{T}` rescale `hz`
from `[-pi, pi]` to `[0,1]`, so the field arriving from `alwan_jzazbz_to_jzczhz_{T}` is a
normalized fraction. This mapper only copies `hz`, so it is convention-agnostic; readers
of the field are not. See [ranges.md](../ranges.md).

---

### alwan_view_transform_apply_unclamped_{T}

```c
alwan_status alwan_view_transform_apply_unclamped_{T}(
    alwan_{T} *rgb_out, size_t out_stride,
    alwan_{T} const *rgb_in, size_t in_stride,
    size_t count,
    alwan_view_transform vt,
    alwan_ctx *ctx);
```

Same dispatch table as `alwan_view_transform_apply_{T}`, with unclamped workers swapped in
for exactly six views: `ALWAN_VIEW_REINHARD_EXT`, `ALWAN_VIEW_UCHIMURA`,
`ALWAN_VIEW_LOTTES`, `ALWAN_VIEW_TONY_MCMAPFACE`, `ALWAN_VIEW_REINHARD_CALIBRATED` and
`ALWAN_VIEW_EXPOSURE`. Every other `vt` falls through to its standard worker and is
byte-identical between the two entry points.

**Parameters:**
- `rgb_out` / `rgb_in` -- NULL returns `ALWAN_E_INVALID`.
- `out_stride` / `in_stride` -- **bytes**.
- `count` -- never validated; `count == 0` is a successful no-op.
- `vt` -- an unrecognized value returns `ALWAN_E_INVALID`.
- `ctx` -- discarded on the first line of the implementation (`(void)ctx;`) for every
  view. No view transform in the table is stateful, so "optional context" understates it:
  it is unused.

The negative-input clip is removed for four of the six. `ALWAN_VIEW_UCHIMURA` keeps its
`max(0)` as a pow-domain guard. `ALWAN_VIEW_LOTTES`, `ALWAN_VIEW_TONY_MCMAPFACE`,
`ALWAN_VIEW_REINHARD_CALIBRATED` and `ALWAN_VIEW_EXPOSURE` lose theirs.
`ALWAN_VIEW_REINHARD_EXT` never had one, so it loses only its output `SATURATE`.
`ALWAN_VIEW_EXPOSURE` is `1 - exp(-gain * L)` with `gain = 2^0 = 1`, which never exceeds
`1` for `L >= 0`, so its unclamped form differs from the clamped one only for negative
input.

> **Three views apply no clamp in the dispatch wrapper and are bounded anyway.**
> `ALWAN_VIEW_KHRONOS_PBR_NEUTRAL` and the two BT.2446 Method A views carry neither a
> `SATURATE` nor a `max(0)` in the wrapper, so the clamped entry point looks unclamped
> for them. The bound lives in the operators: `alwan_bt2446a_forward` and
> `alwan_bt2446a_inverse` both end in `SATURATE`, and PBR Neutral is bounded by
> construction (the offset step floors the minimum channel at `0`, and
> `new_peak = 1 - d^2 / (peak + d - start) < 1` for every `peak`). In the other
> direction, `ALWAN_VIEW_BT2446B_SDR_TO_HDR`, `ALWAN_VIEW_BT2446C_HDR_TO_SDR` and
> `ALWAN_VIEW_BT2390_HDR_TO_SDR` `SATURATE` their **input** to `[0,1]` before the
> operator runs, and the unclamped entry point does not lift that input clamp.

Every per-view operator parameter is baked into the enum value and documented nowhere:

| View | Baked parameters |
|------|------------------|
| `ALWAN_VIEW_REINHARD_EXT` | `L_white = 4.0` |
| `ALWAN_VIEW_REINHARD_CALIBRATED` | `key = 0.18`, `L_avg = 0.18`, `L_white = 4.0` |
| `ALWAN_VIEW_EXPOSURE` | `0 EV` |
| `ALWAN_VIEW_BT2446A_HDR_TO_SDR`, `ALWAN_VIEW_BT2446C_HDR_TO_SDR` | 1000 nits HDR -> 100 nits SDR |
| `ALWAN_VIEW_BT2446A_SDR_TO_HDR`, `ALWAN_VIEW_BT2446B_SDR_TO_HDR` | 100 nits SDR -> 1000 nits HDR |
| `ALWAN_VIEW_BT2390_HDR_TO_SDR` | 10000 nits -> 100 nits |

---

### alwan_aces2_output_transform_custom_display_linear_{T}

```c
alwan_status alwan_aces2_output_transform_custom_display_linear_{T}(
    alwan_rgb_{T} *rgb_out,
    alwan_rgb_{T} const *rgb_in,
    alwan_{T} peak_luminance,
    alwan_aces_primaries_{T} const *limit_primaries);
```

The front half of the custom ACES 2.0 output transform: AP1 tonescale plus chroma
compression, RGB->JMh, gamut compression to `limit_primaries`, then JMh->RGB decoded
directly in the limit primaries. No clamp, no display encode.

**Parameters:**
- `rgb_in` -- ACEScg (AP1 linear).
- `rgb_out` -- display-linear in the limit primaries, at ACES 2.0 reference luminance.
- `peak_luminance` -- nits, validated to `[1.0, 10000.0]`; outside returns
  `ALWAN_E_INVALID`. The header states that range only on the sibling
  `alwan_aces2_output_transform_custom`.
- `limit_primaries` -- display gamut primaries. NULL returns `ALWAN_E_INVALID`.

> **The output scale is ACES 2.0 reference luminance, `n_r = 100` nits: `1.0` means
> 100 cd/m2, regardless of `peak_luminance`.** It is not display code values in `[0,1]`
> and not absolute nits. Reading it as either is wrong by a factor of 100 or of
> `peak / 100`. Values are unbounded above and below.

> **`custom == display_linear + clamp + encode` holds for SDR EOTFs only.** For
> `ALWAN_TF_PQ` the tail multiplies by `n_r = 100` to reach absolute nits and clamps to
> `[0, peak_luminance]`, not `[0,1]`. For `ALWAN_TF_HLG` the tail scales by
> `100 / peak_luminance`, clamps to `[0,1]`, and then applies a full inverse OOTF (system
> gamma `1.2 + 0.42 * log10(peak / 1000)`, BT.2020 luminance weights, a `1e-12` floor on
> `Yd`) before the OETF. Composing this function with your own clamp and OETF will not
> reproduce the custom transform for PQ or HLG.

> **The f32 entry writes `rgb_out` before checking the status.** It is a widening facade
> over the f64 worker, and it copies the f64 temporary into `rgb_out` unconditionally, so
> a rejected call (out-of-range `peak_luminance`) fills the caller's buffer with the
> uninitialized stack value and returns `ALWAN_E_INVALID` alongside it. The same pattern
> is in `alwan_aces2_output_transform_custom_f32` and in the DCDM / P3-DCI branch of
> `alwan_aces2_output_transform_f32`.

---

### alwan_pointer_gamut_boundary

```c
alwan_vec2_f64 const* alwan_pointer_gamut_boundary(size_t *count_out);
```

Returns a pointer to file-static storage holding the 32 boundary points. Never fails,
never returns NULL, and must not be freed.

**Parameters:**
- `count_out` -- receives `32`. **NULL is accepted** and is a silent no-op, which the
  header does not say and which matters because there is no other way to learn the
  length.

f64-only. There is no `_f32` twin, and the function is compiled outside any
`ALWAN_WITH_F64` guard, so it exists even in f32-only builds.

Three properties of the data:

- The chromaticities are CIE 1931 xy referenced to **Illuminant C** (MacAdam 1935,
  reanalyzed). The illuminant is named only on the sibling
  `alwan_is_within_pointer_gamut_{T}`, and not where Pointer's gamut is introduced
  earlier on this page. Testing D65-referenced chromaticities against these points is a
  white-point mismatch.
- The polygon is **not closed**: the first point `(0.659, 0.316)` is not repeated at the
  end, which holds `(0.508, 0.226)`. `alwan_is_within_pointer_gamut_f64` closes it
  implicitly with the `j = count - 1` seed in its point-in-polygon loop. A caller drawing
  or testing this array must close it.
- It is a 2-D chromaticity hull. Pointer's gamut is lightness-dependent, and this
  projection discards that.

---

## Error Codes

All functions on this page return an `int` from the `alwan_status` enum:

| Code | Value | Meaning in gamut functions |
|------|-------|----------------------------|
| `ALWAN_OK` | `0` | Success |
| `ALWAN_E_INVALID` | `-1` | NULL pointer, or unsupported `method` / pixel format |
| `ALWAN_E_NODATA` | `-2` | Required embedded data not available |
| `ALWAN_E_RANGE` | `-3` | Singular primaries/white-point matrix; value out of valid range |
| `ALWAN_E_NOMEM` | `-4` | Allocation failed (e.g. Monte Carlo scratch buffers) |
| `ALWAN_E_DIVZERO` | `-5` | Division by zero would occur |

Not every function returns every code; check each call site against `ALWAN_OK`.

---

## See Also

- [Color Spaces](color-spaces.md) -- RGB conversions
- [Chromatic Adaptation](chromatic-adaptation.md) -- White point transforms
- [Batch / Map API](map.md) -- interleaved, planar, and typed `_ex` dispatch shapes
- [Configuration](../configuration.md) -- precision build flags and facades
