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
