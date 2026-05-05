# Alwan API Conventions

This document summarizes the public API patterns used by `src/alwan/alwan.h`.
It lives in `docs/` on purpose: it is a companion guide for the whole library,
not a per-module reference page.

---

## Quick Reference

The dominant signature shape across the public batch surface is:

```text
alwan_<...>_<f32|f64>_map_interleave(
    out, out_stride,            <- output first, stride next to it
    in,  in_stride,             <- input second, stride next to it
    count,                      <- always size_t
    [extras]...,                <- white points, descriptors, enums
    [ctx])                      <- alwan_ctx*, last when needed
```

Quick rules:

- precision is explicit at the call site (`_f32` or `_f64`)
- output buffer comes before input buffer
- each buffer's stride sits next to that buffer
- pixel-format pairs in `_ex` variants follow `(out_fmt, in_fmt)` after `count`
  (a small block of legacy planar `_ex` entries still uses `(in_fmt, out_fmt)` —
  see [map.md](map.md))
- `alwan_ctx*` only appears on functions that allocate, look up registry data,
  or run descriptor-driven workflows; pure math helpers omit it
- fallible functions return `int` matching `alwan_status`; output buffers are
  undefined unless the call returned `ALWAN_OK`

The rest of this document is the long-form expansion of those rules.

---

## Naming

- Functions use the form `alwan_<verb>_<object>_<suffix>`.
- Precision suffixes are explicit:
  - `_f32` for `float`
  - `_f64` for `double`
- Batch variants append an additional shape suffix such as:
  - `_map_interleave`
  - `_map_planar`
  - `_map_interleave_ex`
  - `_map_planar_ex`
- Image-level helpers use `alwan_image_*`.
- Lookup / utility helpers may omit a precision suffix when they are not
  precision-specific, for example `alwan_interop_format()` and
  `alwan_interop_count()`.

---

## Precision Style

Public scalar and semantic types are explicit:

```c
alwan_f32
alwan_f64
alwan_rgb_f32
alwan_rgb_f64
alwan_xyz_f32
alwan_xyz_f64
```

The docs sometimes use `{T}` as a template placeholder:

- `alwan_rgb_{T}` means `alwan_rgb_f32` or `alwan_rgb_f64`
- `alwan_scalar_{T}` means `alwan_f32` or `alwan_f64`
- `alwan_function_{T}` means the matching `_f32` or `_f64` function

---

## Signature Families

Alwan does not force one signature shape for every function. Instead, it uses a
small number of repeatable families.

### 1. Single-value transforms

Outputs come first, followed by inputs, then any extra descriptors.

```c
void alwan_xyz_to_lab_f64(
    alwan_lab_f64 *lab_out,
    alwan_xyz_f64 const *xyz_in,
    alwan_xyz_f64 const *white_xyz
);
```

### 2. Typed batch transforms

Typed interleaved batch functions operate on `alwan_f32*` or `alwan_f64*`
buffers. The pattern is:

```c
int alwan_<...>_f64_map_interleave(
    alwan_f64 *out, size_t out_stride,
    alwan_f64 const *in, size_t in_stride,
    size_t count,
    ...extra parameters...
);
```

Examples:

```c
int alwan_xyz_to_lab_f64_map_interleave(
    alwan_f64 *lab_out, size_t out_stride,
    alwan_f64 const *xyz_in, size_t in_stride,
    size_t count,
    alwan_xyz_f64 const *white_xyz
);

int alwan_view_transform_apply_f64(
    alwan_f64 *rgb_out, size_t out_stride,
    alwan_f64 const *rgb_in, size_t in_stride,
    size_t count,
    alwan_view_transform vt,
    alwan_ctx *ctx
);
```

### 3. Typed-pixel batch transforms (`_ex`)

`_ex` variants accept `void*` buffers plus `alwan_pixel_format` so they can
operate on `U8`, `U16`, `F16`, `F32`, or `F64` buffers without a separate
caller-side normalize/cast pass.

Most `_map_interleave_ex` functions follow:

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

### 4. Planar batch transforms

Planar functions group outputs first, then inputs, then `count`, then extra
parameters.

```c
int alwan_xyz_to_lab_f64_map_planar(
    alwan_f64 *out0, size_t out_stride,
    alwan_f64 *out1,
    alwan_f64 *out2,
    alwan_f64 const *in0, size_t in_stride,
    alwan_f64 const *in1,
    alwan_f64 const *in2,
    size_t count,
    alwan_xyz_f64 const *white_xyz
);
```

`_map_planar_ex` variants follow the same general model but add pixel-format
arguments. Most use `(out_fmt, in_fmt)` after `count`, but a few older planar
`_ex` entries still preserve legacy argument ordering. When in doubt, use
`alwan.h` as the literal source of truth for that specific function.

### 5. Image-level helpers

Image helpers operate on rows rather than per-pixel strides:

```c
int alwan_image_convert_f64(
    void *dst, size_t dst_row_stride,
    void const *src, size_t src_row_stride,
    size_t width, size_t height,
    alwan_pixel_format dst_fmt,
    alwan_pixel_format src_fmt,
    alwan_rgb_space_desc_f64 const *src_space,
    alwan_rgb_space_desc_f64 const *dst_space,
    alwan_ctx *ctx
);
```

RGBA image helpers add `alwan_alpha_mode` before `ctx`.

### 6. Lookup / utility functions

Registry and utility helpers use the smallest signature that fits the job:

```c
int         alwan_interop_parse_f64(alwan_rgb_space *space, char const *id);
char const *alwan_interop_format(alwan_rgb_space space);
size_t      alwan_interop_count(void);
```

---

## Output-First Rule

The dominant style across the public API is output-first:

- single-value math writes into output structs first
- batch APIs place output buffers before input buffers
- file/buffer export APIs place the destination path or destination buffer first

This keeps call sites visually aligned with data flow.

---

## Strides And Formats

- Strides are expressed in bytes.
- Interleaved typed APIs carry one output stride and one input stride.
- Planar APIs use one stride shared across the three channels for that call.
- `_ex` APIs attach pixel-format metadata directly in the signature via
  `alwan_pixel_format`.

Current public pixel formats are:

```c
ALWAN_PIXEL_U8
ALWAN_PIXEL_U16
ALWAN_PIXEL_F32
ALWAN_PIXEL_F64
ALWAN_PIXEL_F16
```

---

## Error Model

- Fallible functions return `int` and use `alwan_status`.
- Common status codes:
  - `ALWAN_OK`
  - `ALWAN_E_INVALID`
  - `ALWAN_E_NODATA`
  - `ALWAN_E_RANGE`
  - `ALWAN_E_NOMEM`
  - `ALWAN_E_DIVZERO`
- In general, callers should treat output buffers as undefined unless the
  function returned `ALWAN_OK`.

---

## Context Usage

`alwan_ctx *ctx` is only present on APIs that need:

- RGB-space descriptor lookup
- chromatic adaptation with descriptor-driven workflows
- LUT / CLF generation
- video encode / decode space lookup
- other allocation- or registry-backed helpers

Pure math helpers usually do not take a context.

---

## Practical Rule

For any new or updated documentation in `docs/`, prefer this order of truth:

1. `src/alwan/alwan.h`
2. this conventions document
3. individual guide pages in `docs/`

If a guide example disagrees with the header, the header wins.
