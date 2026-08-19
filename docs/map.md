# Map API

Alwan's batch-processing layer for interleaved buffers, planar buffers, and
typed-pixel frontends.

This guide describes the current implementation and public entry points. It
stays in `docs/` because it is a usage and architecture guide rather than a
per-module reference page.

---

## What "map" Means

A map function applies one transform across many pixels or samples in a single
call.

The main public families are:

- typed interleaved: `..._f32_map_interleave`, `..._f64_map_interleave`
- typed planar: `..._f32_map_planar`, `..._f64_map_planar`
- typed-pixel interleaved: `..._map_interleave_ex`
- typed-pixel planar: `..._map_planar_ex`
- image helpers layered on top of the same typed-pixel concepts:
  `alwan_image_convert_{T}` and `alwan_image_convert_rgba_{T}`

---

## Current Source Layout

The current map implementation lives under `src/alwan/map/`.

Important files:

```text
src/alwan/map/alwan_rgb_map.c
src/alwan/map/alwan_colorspace_map.c
src/alwan/map/alwan_oklab_map.c
src/alwan/map/alwan_convenience_map.c
src/alwan/map/alwan_convenience_extra_map.c
src/alwan/map/alwan_cam_map.c
src/alwan/map/alwan_ictcp_map.c
src/alwan/map/alwan_ipt_map.c
src/alwan/map/alwan_jzazbz_map.c
src/alwan/map/alwan_extended_map.c
src/alwan/map/alwan_gamut_map.c
src/alwan/map/alwan_vision_map.c
src/alwan/map/alwan_color_correction_map.c
src/alwan/map/alwan_mat3_map.c
src/alwan/map/alwan_typed_map.c
src/alwan/map/alwan_typed_planar_map.c
src/alwan/map/alwan_map_internal.h
src/alwan/map/alwan_map_simd_defs.h
src/alwan/map/alwan_map_simd_helpers.inc
```

SIMD backend selection lives under `src/alwan/simd/`.

---

## API Families

### 1. Typed interleaved (`_f32` / `_f64`)

These functions operate on raw `alwan_f32*` or `alwan_f64*` triplet buffers.

Pattern:

```c
int alwan_<...>_{T}_map_interleave(
    alwan_scalar_{T} *out, size_t out_stride,
    alwan_scalar_{T} const *in, size_t in_stride,
    size_t count,
    ...extra parameters...
);
```

Example:

```c
int alwan_xyz_to_lab_f64_map_interleave(
    alwan_f64 *lab_out, size_t out_stride,
    alwan_f64 const *xyz_in, size_t in_stride,
    size_t count,
    alwan_xyz_f64 const *white_xyz
);
```

### 2. Typed-pixel interleaved (`_map_interleave_ex`)

These load and store typed pixel buffers directly.

Pattern:

```c
int alwan_<...>_map_interleave_ex(
    void *out, size_t out_stride,
    void const *in, size_t in_stride,
    size_t count,
    alwan_pixel_format out_fmt,
    alwan_pixel_format in_fmt,
    ...extra parameters...
);
```

Example:

```c
int alwan_xyz_to_lab_map_interleave_ex(
    void *out, size_t out_stride,
    void const *in, size_t in_stride,
    size_t count,
    alwan_pixel_format out_fmt,
    alwan_pixel_format in_fmt,
    alwan_xyz_f64 const *white_xyz
);
```

Supported typed-pixel formats:

- `ALWAN_PIXEL_U8`
- `ALWAN_PIXEL_U16`
- `ALWAN_PIXEL_F16`
- `ALWAN_PIXEL_F32`
- `ALWAN_PIXEL_F64`

### 3. Typed planar (`_map_planar`)

Planar APIs expose one pointer per channel.

Pattern:

```c
int alwan_<...>_{T}_map_planar(
    alwan_scalar_{T} *out0, size_t out_stride,
    alwan_scalar_{T} *out1,
    alwan_scalar_{T} *out2,
    alwan_scalar_{T} const *in0, size_t in_stride,
    alwan_scalar_{T} const *in1,
    alwan_scalar_{T} const *in2,
    size_t count,
    ...extra parameters...
);
```

### 4. Typed-pixel planar (`_map_planar_ex`)

Planar `_ex` functions combine per-channel pointers with typed-pixel formats.

The general pattern is:

```c
int alwan_<...>_map_planar_ex(
    void *out0, size_t out_stride,
    void *out1,
    void *out2,
    void const *in0, size_t in_stride,
    void const *in1,
    void const *in2,
    size_t count,
    ...pixel formats...
    ...extra parameters...
);
```

Most current planar `_ex` functions use `out_fmt` followed by `in_fmt` to
match the rest of the API:

```c
int alwan_xyz_to_lab_map_planar_ex(
    void *out0, size_t out_stride, void *out1, void *out2,
    void const *in0, size_t in_stride, void const *in1, void const *in2,
    size_t count,
    alwan_pixel_format out_fmt,
    alwan_pixel_format in_fmt,
    alwan_xyz_f64 const *white_xyz);
```

All `_ex` variants, including every `_map_planar_ex` entry, use the
canonical `(out_fmt, in_fmt)` order; the pre-2.0 legacy `(in_fmt, out_fmt)`
block has been fully migrated.

### 5. Image-level helpers

These are not implemented in `src/alwan/map/`, but they belong to the same
zero-copy data-flow story:

```c
int alwan_image_convert_{T}(...);
int alwan_image_convert_rgba_{T}(...);
```

They operate on row strides and descriptor-driven RGB conversion.

---

## What Exists Today

Representative map-covered areas in the current header:

- sRGB convenience conversions
- XYZ / Lab / Luv / LCh / xyY
- Oklab / Oklch
- ICtCp
- JzAzBz / JzCzhz
- IPT
- extended spaces such as IgPgTg, ICaCb, hdr-CIELAB, hdr-IPT, UCS, OSA-UCS,
  Hunter Lab, ProLab, UVW
- CIECAM02 / CAM16 batch entry points
- gamut mapping
- colour-correction helpers
- vision / CVD helpers
- matrix transforms

The typed frontends in `alwan_typed_map.c` and `alwan_typed_planar_map.c` are
what make the `_ex` APIs possible.

---

## Strides

- Strides are byte strides.
- For interleaved RGB triplets, tightly packed stride is usually
  `3 * sizeof(channel_type)`.
- For planar buffers, the stride is the byte distance between adjacent samples
  within the same plane.

Examples:

```c
size_t rgb_f32_stride = 3 * sizeof(float);
size_t rgb_u8_stride  = 3;
```

---

## Collect / Scatter Utilities

When you want to do your own staging instead of calling `_ex` directly, use:

```c
alwan_collect3_f32(...)
alwan_collect3_f64(...)
alwan_scatter3_f32(...)
alwan_scatter3_f64(...)
```

These convert between packed typed pixels and `alwan_f32` / `alwan_f64`
triplets.

Typical reasons to use them:

- normalize once, run multiple map passes, then scatter once
- integrate Alwan into a larger staged processing graph
- inspect normalized data between passes

---

## Example: `_map_interleave_ex`

```c
uint8_t src_u8[count * 3];
float dst_oklab[count * 3];

alwan_srgb_to_oklab_map_interleave_ex(
    dst_oklab, sizeof(float) * 3,
    src_u8, 3,
    count,
    ALWAN_PIXEL_F32,
    ALWAN_PIXEL_U8);
```

## Example: `_map_planar`

```c
alwan_xyz_f64 d65;
alwan_illuminant_white_point_f64(
    &d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);

alwan_xyz_to_lab_f64_map_planar(
    L, sizeof(alwan_f64),
    a, b,
    X, sizeof(alwan_f64),
    Y, Z,
    count,
    &d65);
```

## Example: image helper

```c
alwan_image_convert_f64(
    dst, dst_row_stride,
    src, src_row_stride,
    width, height,
    ALWAN_PIXEL_U16,
    ALWAN_PIXEL_U8,
    &src_desc, &dst_desc,
    ctx);
```

---

## SIMD And Determinism

The map layer is designed to exploit SIMD backends where available. The backend
headers live in:

```text
src/alwan/simd/alwan_simd_scalar.h
src/alwan/simd/alwan_simd_sse2.h
src/alwan/simd/alwan_simd_avx.h
src/alwan/simd/alwan_simd_avx2.h
src/alwan/simd/alwan_simd_neon.h
```

In normal builds:

- map kernels use the selected SIMD backend where profitable
- typed-pixel frontends vectorize load/convert/store paths where possible

In `ALWAN_DETERMINISTIC` builds:

- the library favors canonicalized evaluation order over peak throughput
- reduction-sensitive SIMD behavior is constrained so cross-platform output is
  reproducible

See [determinism.md](determinism.md) for the full design details.

---

## Guidance

- Use typed `_f32` / `_f64` map APIs when your working buffer is already in
  floating-point triplets.
- Use `_ex` APIs when your host data is still in `U8`, `U16`, or `F16`.
- Use planar APIs when your host library stores channels separately.
- Use image helpers when you are converting complete images row-by-row with RGB
  space descriptors.

See also:

- [examples.md](examples.md)
- [lib_mem.md](lib_mem.md)
- [docs/api/map.md](api/map.md)
