# Matrix Operations API

Functions for 3×3 matrix mathematics used in color transformations.

---

## Overview

Alwan uses 3×3 matrices for:
- RGB ↔ XYZ conversions
- Chromatic adaptation transforms
- Color space derivations

All matrix functions are provided in both native precisions: `_f32`
(`alwan_f32`, single) and `_f64` (`alwan_f64`, double). Pick the suffix that
matches your data; the two compile independently and are gated by
`ALWAN_WITH_F32` / `ALWAN_WITH_F64` (both on by default — see
[Configuration](../configuration.md)).

---

## Types

The math types are generated per precision from `alwan_types_gen.inc`:

```c
typedef struct { alwan_f32 v[3]; } alwan_vec3_f32;
typedef struct { alwan_f64 v[3]; } alwan_vec3_f64;

typedef struct { alwan_f32 m[9]; } alwan_mat3x3_f32;
typedef struct { alwan_f64 m[9]; } alwan_mat3x3_f64;
```

Default-precision aliases (`alwan_vec3`, `alwan_mat3x3`) over `alwan_scalar`
also exist for GPU backends and convenience value types.

3×3 matrices are stored in **row-major** order (flat array of 9 elements):

```
m[0] m[1] m[2]    (row 0)
m[3] m[4] m[5]    (row 1)
m[6] m[7] m[8]    (row 2)
```

**Initialization:**
```c
alwan_mat3x3_f64 identity = {
    .m = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0}
};
```

---

## Scalar Functions

### alwan_mat3_mul

```c
void alwan_mat3_mul_f32(alwan_mat3x3_f32 *out, alwan_mat3x3_f32 const *a, alwan_mat3x3_f32 const *b);
void alwan_mat3_mul_f64(alwan_mat3x3_f64 *out, alwan_mat3x3_f64 const *a, alwan_mat3x3_f64 const *b);
```

Multiplies two 3×3 matrices: `out = a * b`.

---

### alwan_mat3_mulv

```c
void alwan_mat3_mulv_f32(alwan_vec3_f32 *out, alwan_mat3x3_f32 const *m, alwan_vec3_f32 const *v);
void alwan_mat3_mulv_f64(alwan_vec3_f64 *out, alwan_mat3x3_f64 const *m, alwan_vec3_f64 const *v);
```

Multiplies matrix by vector: `out = m * v`.

---

### alwan_mat3_inv

```c
int alwan_mat3_inv_f32(alwan_mat3x3_f32 *out, alwan_mat3x3_f32 const *m);
int alwan_mat3_inv_f64(alwan_mat3x3_f64 *out, alwan_mat3x3_f64 const *m);
```

Inverts a 3×3 matrix using partial-pivot Gaussian elimination.

**Returns:**
- `ALWAN_OK` — Success
- `ALWAN_E_RANGE` — Matrix is singular

---

### alwan_mat3_identity

```c
void alwan_mat3_identity_f32(alwan_mat3x3_f32 *out);
void alwan_mat3_identity_f64(alwan_mat3x3_f64 *out);
```

Writes an identity matrix into `out`.

---

### alwan_mat3_det

```c
alwan_f32 alwan_mat3_det_f32(alwan_mat3x3_f32 const *m);
alwan_f64 alwan_mat3_det_f64(alwan_mat3x3_f64 const *m);
```

Returns the determinant of a 3×3 matrix.

---

## Batch Functions

### alwan_mat3_transform (map / interleaved)

```c
int alwan_mat3_transform_f32_map_interleave(
    alwan_f32 *vec_out, size_t out_stride,    // output buffer + its byte stride
    alwan_f32 const *vec_in, size_t in_stride, // input buffer + its byte stride
    size_t count,
    alwan_mat3x3_f32 const *matrix
);
int alwan_mat3_transform_f64_map_interleave(
    alwan_f64 *vec_out, size_t out_stride,
    alwan_f64 const *vec_in, size_t in_stride,
    size_t count,
    alwan_mat3x3_f64 const *matrix
);
```

Transforms an array of `count` 3D vectors by the same `matrix`: `out[i] = matrix * in[i]`.

Following the v2.0 parameter convention, each buffer is immediately followed by
its own byte stride (memcpy order), outputs precede inputs, and `count` sits
after the buffer/stride block. Strides are in **bytes** (typically
`3 * sizeof(alwan_f32)` / `3 * sizeof(alwan_f64)` for tightly packed triplets).

Returns `ALWAN_OK` on success.

---

### alwan_mat3_transform_map_interleave_ex (typed)

```c
int alwan_mat3_transform_map_interleave_ex(
    void *vec_out, size_t out_stride,
    void const *vec_in, size_t in_stride,
    size_t count,
    alwan_pixel_format out_fmt,
    alwan_mat3x3_f64 const *matrix,
    alwan_pixel_format in_fmt
);
```

Typed variant: `void*` buffers whose element types are given by
`out_fmt` / `in_fmt` (any of `ALWAN_PIXEL_U8`, `U16`, `F16`, `F32`, `F64`).
Math is carried in `alwan_f64`. Useful for transforming pixel buffers without
manually unpacking integer or half-float formats.

Returns `ALWAN_OK` on success.

---

## Collect / Scatter Helpers (typed ↔ f64)

Utilities to load/store typed 3-channel pixels as contiguous `alwan_scalar`
triplets — convenient when feeding the scalar matrix functions from arbitrary
pixel formats.

```c
/* Collect: load typed 3-channel pixels into triplets */
int alwan_collect3_f64(alwan_f64 *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format in_fmt);
int alwan_collect3_f32(alwan_f32 *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format in_fmt);

/* Scatter: store triplets into typed 3-channel pixels */
int alwan_scatter3_f64(void *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt);
int alwan_scatter3_f32(void *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt);
```

`in_fmt` / `out_fmt` is the format of the **typed** (`void*`) side; the other
side is native `alwan_f32` / `alwan_f64`. Strides are in bytes. Integer↔float
conversion uses the standard `(2^N - 1)` normalization. Each returns
`ALWAN_OK` on success.

---

## Usage Examples

### RGB to XYZ Transformation

```c
alwan_mat3x3_f64 rgb_to_xyz, xyz_to_rgb;
alwan_rgb_space_desc_f64 desc;
alwan_rgb_get_space_descriptor_f64(&desc, ALWAN_RGB_SPACE_SRGB, ctx); // ctx LAST
alwan_rgb_derive_matrices_f64(&rgb_to_xyz, &xyz_to_rgb, &desc);

alwan_vec3_f64 rgb = {.v = {0.5, 0.3, 0.2}};
alwan_vec3_f64 xyz;
alwan_mat3_mulv_f64(&xyz, &rgb_to_xyz, &rgb);
```

### Matrix Inversion

```c
alwan_mat3x3_f64 M, M_inv;
int status = alwan_mat3_inv_f64(&M_inv, &M);
if (status != ALWAN_OK) {
    // Matrix is singular
}
```

### Batch Transform of a Packed Buffer

```c
alwan_f32 const *src;  // count * 3 floats, tightly packed
alwan_f32 *dst;        // count * 3 floats, tightly packed
alwan_mat3_transform_f32_map_interleave(
    dst, 3 * sizeof(alwan_f32),
    src, 3 * sizeof(alwan_f32),
    count, &rgb_to_xyz);
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_RANGE` (-3) — Singular matrix

See [Error Handling](../api-conventions.md) for the full `alwan_status` enum.

---

## See Also

- [Chromatic Adaptation](chromatic-adaptation.md) — CAT matrices
- [Color Spaces](color-spaces.md) — RGB/XYZ conversions
- [API Conventions](../api-conventions.md) — parameter ordering rules
