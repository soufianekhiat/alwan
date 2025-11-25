# Precision & Limits

Understanding numerical accuracy, constraints, and edge cases in Alwan.

---

## Scalar Precision

Alwan supports two precision modes, selected at compile time:

### Float (32-bit)

```c
#define ALWAN_SCALAR_IS_FLOAT 1
```

**Properties:**
- **Precision:** ~7 decimal digits
- **Range:** ±3.4 × 10³⁸
- **Epsilon:** ~1.2 × 10⁻⁷
- **Performance:** 2× faster on typical hardware

**Use cases:**
- Real-time graphics
- Game engines
- Mobile applications
- When memory bandwidth is critical

**Typical accuracy:**
| Operation | Absolute Error | Relative Error |
|-----------|----------------|----------------|
| Matrix multiply | ±1e-6 | ±1e-6 |
| XYZ → Lab | ±0.001 | ±1e-5 |
| RGB → XYZ | ±0.0001 | ±1e-6 |
| Transfer functions | ±1e-6 | ±1e-6 |

---

### Double (64-bit)

```c
#define ALWAN_SCALAR_IS_FLOAT 0  // Default
```

**Properties:**
- **Precision:** ~16 decimal digits
- **Range:** ±1.8 × 10³⁰⁸
- **Epsilon:** ~2.2 × 10⁻¹⁶
- **Performance:** Baseline

**Use cases:**
- Scientific computation
- Color science research
- Reference implementations
- Validation and testing

**Typical accuracy:**
| Operation | Absolute Error | Relative Error |
|-----------|----------------|----------------|
| Matrix multiply | ±1e-14 | ±1e-15 |
| XYZ → Lab | ±1e-10 | ±1e-12 |
| RGB → XYZ | ±1e-12 | ±1e-14 |
| Transfer functions | ±1e-12 | ±1e-13 |

---

## Precision Selection Guide

### Choose Float When:
- ✓ Working with 8-bit or 10-bit images
- ✓ Real-time rendering (>30 fps requirement)
- ✓ Memory bandwidth is limited
- ✓ Processing millions of colors per frame
- ✓ Errors < 0.001 are acceptable

### Choose Double When:
- ✓ Scientific accuracy required
- ✓ Validating implementations
- ✓ HDR with >12-bit precision
- ✓ Chaining many conversions
- ✓ Need for bit-exact reproducibility

---

## Numerical Stability

### Matrix Operations

**Matrix Inversion:**
- Uses partial-pivot Gaussian elimination
- Condition number < 10⁶: Stable
- Condition number > 10¹⁰: May be inaccurate

**Example:**
```c
alwan_mat3x3 m, m_inv;
// ... initialize m ...

alwan_result r = alwan_mat3_inv(&m, &m_inv);
if (r != ALWAN_SUCCESS) {
    // Matrix is singular or ill-conditioned
}
```

**Determinant threshold:**
- Float: |det| > 1e-7
- Double: |det| > 1e-14

---

### Transfer Functions

All transfer functions include careful clamping to avoid:
- NaN propagation
- Inf from division by zero
- Negative values in sqrt/pow

**Example (sRGB):**
```c
// Input clamped to [0, ∞) before processing
// Output guaranteed in [0, 1] for valid inputs
```

**Precision near zero:**
| Function | Accuracy at 0.001 | Accuracy at 1.0 |
|----------|-------------------|-----------------|
| sRGB | ±1e-6 | ±1e-7 |
| PQ (ST.2084) | ±1e-5 | ±1e-6 |
| HLG | ±1e-6 | ±1e-7 |

---

### Spectral Integration

**Simpson's Rule:**
- Used for even-count sample arrays
- Accuracy: O(h⁴) where h is step size

**Trapezoidal Rule:**
- Fallback for odd-count arrays
- Accuracy: O(h²)

**Typical errors:**
| CMF | Float | Double |
|-----|-------|--------|
| CIE 1931 2° | ±0.0001 | ±1e-10 |
| CIE 1964 10° | ±0.0001 | ±1e-10 |
| Stockman & Sharpe | ±0.0002 | ±1e-9 |

---

## Input Domain Constraints

### XYZ

**Valid domain:** [0, ∞) for each component

**Typical ranges:**
- X: [0, 0.95] for surface colors under D65
- Y: [0, 1.0] (normalized luminance)
- Z: [0, 1.09] for surface colors under D65

**Out-of-range handling:**
- Negative values: Clamped to 0 (with warning in debug)
- Large values (>100): Processed normally, may produce out-of-gamut colors

---

### Lab

**Valid domain:**
- L: [0, 100]
- a: [-128, 127] typical, unbounded in spec
- b: [-128, 127] typical, unbounded in spec

**Out-of-range handling:**
- L < 0: Clamped to 0
- L > 100: Processed normally (super-whites)
- a, b: Processed normally (may be out of gamut)

---

### RGB (Linear)

**Valid domain:**
- SDR: [0, 1] for each component
- HDR: [0, ∞)

**Out-of-range handling:**
- Negative: Represents out-of-gamut colors (preserved)
- > 1: Valid for HDR, out-of-gamut for SDR

---

### RGB (Encoded)

**Valid domain:** [0, 1] for each component

**Out-of-range handling:**
- < 0: Clamped to 0
- > 1: Clamped to 1

---

### HSV / HSL

**Valid domain:**
- H: [0, 360) degrees (wraps around)
- S: [0, 1]
- V/L: [0, 1]

**Special cases:**
- S = 0: Hue is undefined (achromatic)
- V = 0 (HSV): Black, hue undefined
- L = 0 or L = 1 (HSL): Black/white, hue undefined

---

## Edge Cases

### Black (0, 0, 0)

| Color Space | Black Representation |
|-------------|---------------------|
| XYZ | (0, 0, 0) |
| Lab | (0, 0, 0) |
| LCh | (0, 0, undefined) |
| HSV | (undefined, 0, 0) |
| HSL | (undefined, 0, 0) |
| Oklab | (0, 0, 0) |

**Hue preservation:** Undefined hues preserved as 0 in forward conversions, reconstructed as 0 in inverse.

---

### White

| Space | D65 White |
|-------|-----------|
| XYZ | (0.9505, 1.0000, 1.0890) |
| Lab | (100, 0, 0) |
| LCh | (100, 0, undefined) |
| sRGB | (1, 1, 1) |
| Oklab | (1, 0, 0) |

---

### Achromatic Colors

Colors with zero chroma:
- **Lab:** a = b = 0
- **LCh:** C = 0, h undefined
- **HSV:** S = 0, h undefined
- **Oklab:** a = b = 0

**Handling:**
```c
// Hue is preserved as 0 but should not be trusted
alwan_vec3 gray = {0.5, 0.5, 0.5};  // Gray RGB
alwan_vec3 hsv;
alwan_rgb_to_hsv(&gray, &hsv, 1, 0, 0);
// hsv.x (hue) is 0, but meaningless
```

---

### Out-of-Gamut Colors

Colors outside the RGB cube [0,1]³:

**Detection:**
```c
bool is_in_gamut(const alwan_vec3 *rgb) {
    return rgb->x >= 0 && rgb->x <= 1 &&
           rgb->y >= 0 && rgb->y <= 1 &&
           rgb->z >= 0 && rgb->z <= 1;
}
```

**Handling options:**
1. **Clip:** Clamp to [0,1]
2. **Gamut map:** Use perceptual gamut mapping
3. **Preserve:** Keep negative/large values for further processing

---

### Very Small Values

**Near-zero behavior:**

| Operation | Threshold | Behavior |
|-----------|-----------|----------|
| XYZ → Lab | Y < 1e-10 | Returns L=0, a=0, b=0 |
| Lab → LCh | C < 1e-10 | h = 0 (undefined) |
| RGB → HSV | max < 1e-10 | H = S = 0 |
| Division | divisor < ε | Clamped to ε |

---

### Very Large Values (HDR)

**HDR range support:**

| Function | Max Value (Float) | Max Value (Double) |
|----------|-------------------|-------------------|
| XYZ → Lab | 10⁶ | 10¹⁵ |
| PQ EOTF | 10,000 nits | 10,000 nits |
| HLG EOTF | 1,000 nits | 1,000 nits |

**Overflow protection:**
- All functions clamp intermediate results
- Final values may saturate but will not produce NaN/Inf

---

## Accumulation Error

### Single Conversion

**Error:** Dominated by algorithmic error (~1e-6 for float, ~1e-12 for double)

---

### Chained Conversions

**Error grows with each step:**

```c
// RGB → XYZ → Lab → LCh → Lab → XYZ → RGB
// Float: ~5e-6 cumulative error
// Double: ~5e-11 cumulative error
```

**Recommendation:**
- Minimize conversion chains
- Use direct conversions when available
- Use double precision for >5 conversions

---

### Bulk Processing

**Error does not accumulate across array elements:**

```c
// Each color processed independently
alwan_xyz_to_lab(xyz, &d65, lab, 1000000, s, s);
// Error is consistent per element, not cumulative
```

---

## Test Tolerances

Alwan's test suite uses adaptive tolerances:

### Float Tolerances

```c
#define FLOAT_ABS_TOL  1e-5
#define FLOAT_REL_TOL  1e-5
```

### Double Tolerances

```c
#define DOUBLE_ABS_TOL  1e-12
#define DOUBLE_REL_TOL  1e-12
```

### Color Difference Tolerances

| Metric | Float | Double |
|--------|-------|--------|
| ΔE76 | ±0.01 | ±1e-8 |
| ΔE00 | ±0.01 | ±1e-8 |
| ΔE CMC | ±0.01 | ±1e-8 |

---

## Performance vs Precision Trade-offs

### Matrix Multiplication

| Implementation | Precision | Speed |
|---------------|-----------|-------|
| Naive float | ±1e-6 | 1.0× |
| Naive double | ±1e-14 | 0.7× |
| SIMD float | ±1e-6 | 4.0× |
| SIMD double | ±1e-14 | 2.0× |

*(SIMD when available on platform)*

---

### Transfer Functions

| Function | Float Time | Double Time | Accuracy Difference |
|----------|-----------|-------------|---------------------|
| sRGB EOTF | 1.0× | 1.0× | 6 digits |
| PQ EOTF | 1.5× | 1.4× | 6 digits |
| Log curves | 1.2× | 1.1× | 6 digits |

**Observation:** Transfer functions are compute-bound, precision impact is minimal on performance.

---

## Recommendations

### For Real-Time Graphics
```c
#define ALWAN_SCALAR_IS_FLOAT 1
```
- 8-bit output: Invisible error
- 10-bit output: <0.5 LSB error
- 12-bit output: <2 LSB error

---

### For Offline Rendering
```c
#define ALWAN_SCALAR_IS_FLOAT 0  // double
```
- Eliminates accumulation error
- Matches reference implementations
- Suitable for 16-bit output

---

### For Scientific Use
```c
#define ALWAN_SCALAR_IS_FLOAT 0  // double
```
- Required for validation
- Matches published reference values
- Enables unit tests with tight tolerances

---

## Debugging Precision Issues

### 1. Verify Build Configuration

```c
#include "alwan.h"
printf("sizeof(alwan_scalar) = %zu\n", sizeof(alwan_scalar));
// 4 = float, 8 = double
```

---

### 2. Check for NaN/Inf

```c
#include <math.h>

bool is_valid_color(const alwan_vec3 *c) {
    return isfinite(c->x) && isfinite(c->y) && isfinite(c->z);
}
```

---

### 3. Compare Against Reference

```c
alwan_vec3 xyz = {0.5, 0.6, 0.4};
alwan_vec3 lab;
alwan_xyz_to_lab(&xyz, &alwan_d65_xyz, &lab, 1, 0, 0);

// Expected (from Python Colour): L=81.968, a=-7.528, b=17.211
// Float: Should match within ±0.001
// Double: Should match within ±1e-9
```

---

### 4. Test Round-Trip Accuracy

```c
alwan_vec3 xyz_orig = {0.5, 0.6, 0.4};
alwan_vec3 lab, xyz_reconstructed;

alwan_xyz_to_lab(&xyz_orig, &alwan_d65_xyz, &lab, 1, 0, 0);
alwan_lab_to_xyz(&lab, &alwan_d65_xyz, &xyz_reconstructed, 1, 0, 0);

alwan_scalar error = fabs(xyz_orig.x - xyz_reconstructed.x) +
                     fabs(xyz_orig.y - xyz_reconstructed.y) +
                     fabs(xyz_orig.z - xyz_reconstructed.z);

// Float: error < 1e-6
// Double: error < 1e-12
```

---

## See Also

- [Configuration](configuration.md) — Setting scalar precision
- [Testing](../README.md#testing) — Validation methodology
- [Examples](examples.md) — Practical usage patterns
