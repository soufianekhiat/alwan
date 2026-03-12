# Matrix Operations API

Functions for 3×3 matrix mathematics used in color transformations.

---

## Overview

Alwan uses 3×3 matrices for:
- RGB ↔ XYZ conversions
- Chromatic adaptation transforms
- Color space derivations

---

## Types

### alwan_mat3x3

```c
typedef struct {
    alwan_scalar m[9];  // Flat array: [m00 m01 m02 m10 m11 m12 m20 m21 m22]
} alwan_mat3x3;
```

3×3 matrix in row-major order (flat array of 9 elements).

**Memory layout:**
```
m[0] m[1] m[2]    (row 0)
m[3] m[4] m[5]    (row 1)
m[6] m[7] m[8]    (row 2)
```

**Initialization:**
```c
alwan_mat3x3 identity = {
    .m = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0}
};
```

---

## Functions

### alwan_mat3_mul

```c
void alwan_mat3_mul(
    alwan_mat3x3 *out,        // Output: result matrix
    alwan_mat3x3 const *a,
    alwan_mat3x3 const *b
);
```

Multiplies two 3×3 matrices: `out = a * b`

---

### alwan_mat3_mulv

```c
void alwan_mat3_mulv(
    alwan_vec3 *out,          // Output: result vector
    alwan_mat3x3 const *m,
    alwan_vec3 const *v
);
```

Multiplies matrix by vector: `out = m * v`

---

### alwan_mat3_inv

```c
int alwan_mat3_inv(
    alwan_mat3x3 *out,        // Output: inverse matrix
    alwan_mat3x3 const *m
);
```

Inverts a 3×3 matrix using partial-pivot Gaussian elimination.

**Returns:**
- `ALWAN_OK` — Success
- `ALWAN_E_RANGE` — Matrix is singular

---

### alwan_mat3_identity

```c
void alwan_mat3_identity(alwan_mat3x3 *out);
```

Creates an identity matrix.

---

### alwan_mat3_det

```c
alwan_scalar alwan_mat3_det(alwan_mat3x3 const *m);
```

Computes the determinant of a 3×3 matrix.

---

### alwan_mat3_transform_bulk

```c
int alwan_mat3_transform_bulk(
    alwan_scalar *vec_out,
    alwan_mat3x3 const *matrix,
    alwan_scalar const *vec_in,
    size_t count,
    size_t in_stride,         // Input stride in bytes
    size_t out_stride         // Output stride in bytes
);
```

Transforms array of 3D vectors by the same matrix.

---

## Usage Examples

### RGB to XYZ Transformation

```c
alwan_mat3x3 rgb_to_xyz, xyz_to_rgb;
alwan_rgb_space_desc desc;
alwan_rgb_get_space_descriptor(&desc, ctx, ALWAN_RGB_SPACE_SRGB);
alwan_rgb_derive_matrices(&rgb_to_xyz, &xyz_to_rgb, &desc);

alwan_vec3 rgb = {.v = {0.5, 0.3, 0.2}};
alwan_vec3 xyz;
alwan_mat3_mulv(&xyz, &rgb_to_xyz, &rgb);
```

### Matrix Inversion

```c
alwan_mat3x3 M, M_inv;
int status = alwan_mat3_inv(&M_inv, &M);
if (status != ALWAN_OK) {
    // Matrix is singular
}
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_RANGE` (-3) — Singular matrix

---

## See Also

- [Chromatic Adaptation](chromatic-adaptation.md) — CAT matrices
- [Color Spaces](color-spaces.md) — RGB/XYZ conversions
