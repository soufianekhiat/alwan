# Matrix Operations API

Functions for 3×3 matrix mathematics used in color transformations.

---

## Overview

Alwan uses 3×3 matrices for:
- RGB ↔ XYZ conversions
- Chromatic adaptation transforms
- Color space derivations
- Custom linear transformations

All matrix operations use row-major storage and are optimized for the configured scalar precision (float or double).

---

## Types

### alwan_mat3x3

```c
typedef struct {
    alwan_scalar m[3][3];
} alwan_mat3x3;
```

3×3 matrix in row-major order.

**Memory layout:**
```
m[0][0]  m[0][1]  m[0][2]    (row 0)
m[1][0]  m[1][1]  m[1][2]    (row 1)
m[2][0]  m[2][1]  m[2][2]    (row 2)
```

**Initialization:**
```c
alwan_mat3x3 identity = {
    .m = {
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}
    }
};
```

---

### alwan_vec3

```c
typedef struct {
    alwan_scalar x, y, z;
} alwan_vec3;
```

3D vector (also used for colors).

**Initialization:**
```c
alwan_vec3 v = {1.0, 2.0, 3.0};
```

---

## Core Functions

### alwan_mat3_mul

```c
alwan_result alwan_mat3_mul(
    alwan_mat3x3 *result,         // Output first
    const alwan_mat3x3 *a,
    const alwan_mat3x3 *b
);
```

Multiplies two 3×3 matrices: `result = a × b`.

**Parameters:**
- `result` — Output matrix (can be same as `a` or `b`)
- `a` — First matrix
- `b` — Second matrix

**Returns:**
- `ALWAN_SUCCESS` — Multiplication successful
- `ALWAN_ERROR_INVALID_PARAMETER` — NULL pointer

**Example:**
```c
alwan_mat3x3 rgb_to_xyz, adaptation, rgb_to_xyz_adapted;

alwan_mat3_mul(&rgb_to_xyz_adapted, &adaptation, &rgb_to_xyz);  // Output first
```

**Precision:** ±1e-6 (float), ±1e-14 (double)

**Performance:** ~10 ns on modern CPU

---

### alwan_mat3_mul_vec3

```c
alwan_result alwan_mat3_mul_vec3(
    alwan_vec3 *result,           // Output first
    const alwan_mat3x3 *m,
    const alwan_vec3 *v
);
```

Multiplies matrix by vector: `result = m × v`.

**Parameters:**
- `result` — Output vector (can be same as `v`)
- `m` — Matrix
- `v` — Input vector

**Returns:**
- `ALWAN_SUCCESS` — Multiplication successful
- `ALWAN_ERROR_INVALID_PARAMETER` — NULL pointer

**Example:**
```c
alwan_mat3x3 rgb_to_xyz = { /* ... */ };
alwan_vec3 rgb = {0.8, 0.3, 0.2};
alwan_vec3 xyz;

alwan_mat3_mul_vec3(&xyz, &rgb_to_xyz, &rgb);  // Output first
```

**Formula:**
```
result.x = m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z
result.y = m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z
result.z = m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z
```

**Precision:** ±1e-6 (float), ±1e-14 (double)

**Performance:** ~3 ns on modern CPU

---

### alwan_mat3_inv

```c
alwan_result alwan_mat3_inv(
    alwan_mat3x3 *inv,            // Output first
    const alwan_mat3x3 *m
);
```

Computes matrix inverse: `inv = m^(-1)`.

**Parameters:**
- `inv` — Output inverse matrix (can be same as `m`)
- `m` — Input matrix

**Returns:**
- `ALWAN_SUCCESS` — Inversion successful
- `ALWAN_ERROR_INVALID_PARAMETER` — NULL pointer or singular matrix

**Example:**
```c
alwan_mat3x3 rgb_to_xyz, xyz_to_rgb;

alwan_result r = alwan_mat3_inv(&xyz_to_rgb, &rgb_to_xyz);  // Output first
if (r != ALWAN_SUCCESS) {
    // Matrix is singular (determinant ≈ 0)
}
```

**Algorithm:** Partial-pivot Gaussian elimination

**Singularity threshold:**
- Float: |det| < 1e-7
- Double: |det| < 1e-14

**Precision:** ±1e-5 (float), ±1e-12 (double)

**Performance:** ~30 ns on modern CPU

---

### alwan_mat3_transpose

```c
alwan_result alwan_mat3_transpose(
    alwan_mat3x3 *result,         // Output first
    const alwan_mat3x3 *m
);
```

Computes matrix transpose: `result = m^T`.

**Parameters:**
- `result` — Output transposed matrix (can be same as `m`)
- `m` — Input matrix

**Returns:**
- `ALWAN_SUCCESS` — Transpose successful
- `ALWAN_ERROR_INVALID_PARAMETER` — NULL pointer

**Example:**
```c
alwan_mat3x3 m, m_T;
alwan_mat3_transpose(&m_T, &m);  // Output first
```

**Formula:**
```
result[i][j] = m[j][i]
```

**Precision:** Exact (no roundoff error)

**Performance:** ~5 ns on modern CPU

---

### alwan_mat3_determinant

```c
alwan_scalar alwan_mat3_determinant(const alwan_mat3x3 *m);
```

Computes matrix determinant: `det(m)`.

**Parameters:**
- `m` — Input matrix

**Returns:** Determinant value

**Example:**
```c
alwan_mat3x3 m = { /* ... */ };
alwan_scalar det = alwan_mat3_determinant(&m);

if (fabs(det) < 1e-7) {
    printf("Matrix is singular\n");
}
```

**Formula:**
```
det = m[0][0] * (m[1][1]*m[2][2] - m[1][2]*m[2][1])
    - m[0][1] * (m[1][0]*m[2][2] - m[1][2]*m[2][0])
    + m[0][2] * (m[1][0]*m[2][1] - m[1][1]*m[2][0])
```

**Precision:** ±1e-6 (float), ±1e-14 (double)

**Performance:** ~5 ns on modern CPU

---

## Usage Patterns

### Pattern 1: RGB Space Derivation

```c
// Given RGB primaries and white point, derive XYZ matrices
alwan_vec2 primaries[3] = {
    {0.64, 0.33},  // Red
    {0.30, 0.60},  // Green
    {0.15, 0.06}   // Blue
};
alwan_vec2 white_point = {0.3127, 0.3290};  // D65

alwan_mat3x3 rgb_to_xyz, xyz_to_rgb;
alwan_rgb_derive_matrices(&rgb_to_xyz, &xyz_to_rgb, primaries, &white_point);  // Outputs first

// Use matrices for conversion (output first)
alwan_vec3 rgb = {0.8, 0.3, 0.2};
alwan_vec3 xyz;
alwan_mat3_mul_vec3(&xyz, &rgb_to_xyz, &rgb);
```

---

### Pattern 2: Chromatic Adaptation Matrix

```c
// Build adaptation matrix: D65 → D50
alwan_mat3x3 bradford = {
    .m = {
        { 0.8951,  0.2664, -0.1614},
        {-0.7502,  1.7135,  0.0367},
        { 0.0389, -0.0685,  1.0296}
    }
};

alwan_vec3 d65_lms, d50_lms;
alwan_mat3_mul_vec3(&d65_lms, &bradford, &alwan_d65_xyz);  // Output first
alwan_mat3_mul_vec3(&d50_lms, &bradford, &alwan_d50_xyz);

// Compute diagonal scaling matrix
alwan_mat3x3 scale = {
    .m = {
        {d50_lms.x / d65_lms.x, 0, 0},
        {0, d50_lms.y / d65_lms.y, 0},
        {0, 0, d50_lms.z / d65_lms.z}
    }
};

// Complete adaptation: M^-1 * D * M (output first)
alwan_mat3x3 bradford_inv, temp, adaptation;
alwan_mat3_inv(&bradford_inv, &bradford);
alwan_mat3_mul(&temp, &scale, &bradford);
alwan_mat3_mul(&adaptation, &bradford_inv, &temp);

// Apply to colors (output first)
alwan_vec3 xyz_d65 = {0.5, 0.6, 0.4};
alwan_vec3 xyz_d50;
alwan_mat3_mul_vec3(&xyz_d50, &adaptation, &xyz_d65);
```

---

### Pattern 3: Matrix Chain Composition

```c
// Compose: sRGB → XYZ → adapted → ProPhoto
alwan_mat3x3 srgb_to_xyz, adapt, xyz_to_prophoto;
// ... initialize matrices ...

// Compose into single matrix (output first)
alwan_mat3x3 temp, srgb_to_prophoto;
alwan_mat3_mul(&temp, &adapt, &srgb_to_xyz);
alwan_mat3_mul(&srgb_to_prophoto, &xyz_to_prophoto, &temp);

// Apply composed transform (output first)
alwan_vec3 srgb = {0.8, 0.3, 0.2};
alwan_vec3 prophoto;
alwan_mat3_mul_vec3(&prophoto, &srgb_to_prophoto, &srgb);
```

---

## Standard Matrices

### sRGB → XYZ (D65)

```c
alwan_mat3x3 srgb_to_xyz = {
    .m = {
        {0.4124564, 0.3575761, 0.1804375},
        {0.2126729, 0.7151522, 0.0721750},
        {0.0193339, 0.1191920, 0.9503041}
    }
};
```

---

### XYZ (D65) → sRGB

```c
alwan_mat3x3 xyz_to_srgb = {
    .m = {
        { 3.2404542, -1.5371385, -0.4985314},
        {-0.9692660,  1.8760108,  0.0415560},
        { 0.0556434, -0.2040259,  1.0572252}
    }
};
```

---

### Bradford Adaptation Transform

```c
alwan_mat3x3 bradford = {
    .m = {
        { 0.8951000,  0.2664000, -0.1614000},
        {-0.7502000,  1.7135000,  0.0367000},
        { 0.0389000, -0.0685000,  1.0296000}
    }
};
```

---

## Precision Considerations

### Condition Number

Matrix inversion accuracy depends on condition number:

```
κ(M) = ||M|| * ||M^(-1)||
```

| Condition Number | Float Accuracy | Double Accuracy |
|-----------------|----------------|-----------------|
| κ < 10² | Excellent (±1e-7) | Excellent (±1e-15) |
| 10² < κ < 10⁴ | Good (±1e-5) | Excellent (±1e-13) |
| 10⁴ < κ < 10⁶ | Fair (±1e-3) | Good (±1e-11) |
| κ > 10⁶ | Poor | Fair (±1e-9) |

**Typical color matrices:** κ ≈ 10-100 (well-conditioned)

---

### Round-Trip Accuracy

```c
alwan_mat3x3 m, m_inv, identity;

alwan_mat3_inv(&m_inv, &m);       // Output first
alwan_mat3_mul(&identity, &m, &m_inv);

// identity should be close to:
// {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}
//
// Float: ±1e-6
// Double: ±1e-13
```

---

### Accumulated Error

Each matrix operation adds error:

| Operations | Float Error | Double Error |
|-----------|-------------|--------------|
| 1 multiply | ±1e-6 | ±1e-14 |
| 5 multiplies | ±5e-6 | ±5e-14 |
| 10 multiplies | ±1e-5 | ±1e-13 |

**Recommendation:** Compose matrices before applying to data when possible.

---

## Performance

### Operation Timing

Typical single-threaded performance on modern CPU:

| Operation | Float | Double |
|-----------|-------|--------|
| Matrix × Vector | 3 ns | 4 ns |
| Matrix × Matrix | 10 ns | 12 ns |
| Matrix Inverse | 30 ns | 35 ns |
| Matrix Determinant | 5 ns | 6 ns |
| Matrix Transpose | 5 ns | 6 ns |

---

### SIMD Acceleration

When available (SSE/AVX on x86, NEON on ARM):
- Matrix × Vector: ~4× faster
- Matrix × Matrix: ~3× faster
- Inversion: ~2× faster

**Note:** SIMD enabled automatically when `ALWAN_USE_SIMD` defined and supported.

---

## Validation

### Check Matrix Validity

```c
bool is_valid_transform(const alwan_mat3x3 *m) {
    alwan_scalar det = alwan_mat3_determinant(m);

    // Check determinant
    if (fabs(det) < 1e-7) {
        return false;  // Singular
    }

    // Check for NaN/Inf
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (!isfinite(m->m[i][j])) {
                return false;
            }
        }
    }

    return true;
}
```

---

### Test Matrix Inversion

```c
void test_inversion(const alwan_mat3x3 *m) {
    alwan_mat3x3 m_inv, identity;

    alwan_result r = alwan_mat3_inv(&m_inv, m);  // Output first
    if (r != ALWAN_SUCCESS) {
        printf("Inversion failed\n");
        return;
    }

    alwan_mat3_mul(&identity, m, &m_inv);  // Output first

    // Check identity
    alwan_scalar error = 0;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            alwan_scalar expected = (i == j) ? 1.0 : 0.0;
            error += fabs(identity.m[i][j] - expected);
        }
    }

    printf("Round-trip error: %.2e\n", error);
}
```

---

## Error Handling

### Singular Matrix Detection

```c
alwan_mat3x3 singular = {
    .m = {
        {1, 2, 3},
        {2, 4, 6},  // Row 2 = 2 * Row 1 (linearly dependent)
        {3, 6, 9}   // Row 3 = 3 * Row 1
    }
};

alwan_mat3x3 inv;
alwan_result r = alwan_mat3_inv(&inv, &singular);  // Output first

if (r == ALWAN_ERROR_INVALID_PARAMETER) {
    printf("Matrix is singular (determinant ≈ 0)\n");
}
```

---

### Numerical Instability

```c
// Ill-conditioned matrix (high condition number)
alwan_mat3x3 ill_conditioned = {
    .m = {
        {1.0, 1.0, 1.0},
        {1.0, 1.0 + 1e-7, 1.0},
        {1.0, 1.0, 1.0 + 1e-7}
    }
};

alwan_mat3x3 inv;
alwan_result r = alwan_mat3_inv(&inv, &ill_conditioned);  // Output first

if (r == ALWAN_SUCCESS) {
    // Inversion succeeded, but may have large error
    // Consider using double precision
}
```

---

## Best Practices

1. **Compose matrices before applying to data**
   ```c
   // Good: One matrix application per color (output first)
   alwan_mat3x3 composed;
   alwan_mat3_mul(&composed, &m2, &m1);
   for (int i = 0; i < count; i++) {
       alwan_mat3_mul_vec3(&colors[i], &composed, &colors[i]);
   }

   // Bad: Two matrix applications per color
   for (int i = 0; i < count; i++) {
       alwan_mat3_mul_vec3(&colors[i], &m1, &colors[i]);
       alwan_mat3_mul_vec3(&colors[i], &m2, &colors[i]);
   }
   ```

2. **Check determinant before inversion**
   ```c
   alwan_scalar det = alwan_mat3_determinant(&m);
   if (fabs(det) > 1e-7) {
       alwan_mat3_inv(&m_inv, &m);  // Output first
   }
   ```

3. **Use double precision for matrix chains**
   - Float: OK for 1-3 operations
   - Double: Recommended for >3 operations

4. **Cache inverted matrices**
   ```c
   // Compute once (output first)
   static alwan_mat3x3 xyz_to_rgb;
   static bool initialized = false;

   if (!initialized) {
       alwan_mat3_inv(&xyz_to_rgb, &rgb_to_xyz);  // Output first
       initialized = true;
   }
   ```

---

## See Also

- [Color Spaces](color-spaces.md) — Using matrices for RGB conversions
- [Chromatic Adaptation](chromatic-adaptation.md) — Adaptation matrices
- [Precision & Limits](../precision-and-limits.md) — Numerical accuracy
- [Examples](../examples.md) — Usage examples
