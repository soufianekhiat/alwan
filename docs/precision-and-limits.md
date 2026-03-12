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
- **Range:** ±3.4 × 10^38
- **Epsilon:** ~1.2 × 10^-7
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
- **Range:** ±1.8 × 10^308
- **Epsilon:** ~2.2 × 10^-16
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
- Yes Working with 8-bit or 10-bit images
- Yes Real-time rendering (>30 fps requirement)
- Yes Memory bandwidth is limited
- Yes Processing millions of colors per frame
- Yes Errors < 0.001 are acceptable

### Choose Double When:
- Yes Scientific accuracy required
- Yes Validating implementations
- Yes HDR with >12-bit precision
- Yes Chaining many conversions
- Yes Need for bit-exact reproducibility

---

## Numerical Stability

### Matrix Operations

**Matrix Inversion:**
- Uses partial-pivot Gaussian elimination
- Condition number < 10^6: Stable
- Condition number > 10^10: May be inaccurate

**Example:**
```c
alwan_mat3x3 m, m_inv;
// ... initialize m ...

alwan_result r = alwan_mat3_inv(&m_inv, &m);  // Output first
if (r != ALWAN_SUCCESS) {
    // Matrix is singular or ill-conditioned
}
```

**Determinant threshold:**
- Float: |det| > 1e-7
- Double: |det| > 1e-14

---

## Floating-Point Determinism

Cross-platform reproducibility requires controlling floating-point contraction.

### The FMA Problem

Fused Multiply-Add (FMA) instructions compute `a * b + c` in a single operation with only one rounding step, producing slightly different results than separate multiply and add operations (which round twice).

**Example:**
```c
// Without FMA: rounded twice
float tmp = a * b;      // round 1
float result = tmp + c; // round 2

// With FMA: rounded once (more accurate but different)
float result = fma(a, b, c);  // round 1 only
```

**Impact:**
- Results may differ by 1-2 ULP (Units in Last Place)
- Different compilers/platforms make different FMA decisions
- Same code, same inputs → different outputs across platforms

---

### Compiler Flags

#### GCC / Clang (Linux, macOS)

By default, GCC and Clang may contract `a * b + c` into FMA when optimizing.

**Recommended for determinism:**
```bash
# Disable FP contraction (FMA folding)
-ffp-contract=off
```

**Flags summary:**
| Flag | Behavior |
|------|----------|
| `-ffp-contract=off` | No FMA contraction (deterministic) |
| `-ffp-contract=on` | Contract within single expression |
| `-ffp-contract=fast` | Contract across statements (default at -O2+) |

**Full recommended build:**
```bash
gcc -O2 -ffp-contract=off -o alwan alwan.c
clang -O2 -ffp-contract=off -o alwan alwan.c
```

---

#### MSVC (Windows)

MSVC does **not** contract FP operations by default. No special flags needed.

**Relevant flags:**
| Flag | Behavior |
|------|----------|
| `/fp:precise` | Default, no FMA contraction |
| `/fp:fast` | May contract (avoid for determinism) |
| `/fp:strict` | Strictest conformance |

**Recommended:**
```batch
cl /O2 /fp:precise alwan.c
```

---

### Cross-Platform Determinism

For bit-exact results across platforms:

1. **Use `-ffp-contract=off`** on GCC/Clang
2. **Use `/fp:precise`** on MSVC (default)
3. **Avoid `-ffast-math`** or `/fp:fast`
4. **Use the same scalar precision** (float or double) everywhere
5. **Test round-trip conversions** on all target platforms

**CMake example:**
```cmake
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(alwan PRIVATE -ffp-contract=off)
endif()
```

---

### When Determinism Matters

| Use Case | Need Determinism? |
|----------|-------------------|
| Unit tests with exact tolerances | Yes Yes |
| Regression testing across builds | Yes Yes |
| Multi-platform CI/CD | Yes Yes |
| Reference implementation validation | Yes Yes |
| Real-time game rendering | Usually not |
| One-off image processing | Usually not |

**Note:** The accuracy difference is typically 1-2 ULP. Both results are "correct" — they just differ slightly due to rounding.

---

### Transfer Functions

All transfer functions include careful clamping to avoid:
- NaN propagation
- Inf from division by zero
- Negative values in sqrt/pow

**Example (sRGB):**
```c
// Input clamped to [0, inf) before processing
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
- Accuracy: O(h^4) where h is step size

**Trapezoidal Rule:**
- Fallback for odd-count arrays
- Accuracy: O(h^2)

**Typical errors:**
| CMF | Float | Double |
|-----|-------|--------|
| CIE 1931 2° | ±0.0001 | ±1e-10 |
| CIE 1964 10° | ±0.0001 | ±1e-10 |
| Stockman & Sharpe | ±0.0002 | ±1e-9 |

---

## Input Domain Constraints

### XYZ

**Valid domain:** [0, inf) for each component

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
- HDR: [0, inf)

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
- H: [0, 1] (normalized; multiply by 360 for degrees, wraps around)
- S: [0, 1]
- V/L: [0, 1]

**Special cases:**
- S = 0: Hue is undefined (achromatic)
- V = 0 (HSV): Black, hue undefined
- L = 0 or L = 1 (HSL): Black/white, hue undefined

---

### HSP (Perceived Brightness)

**Valid domain:**
- H: [0, 1] (normalized, identical to HSV)
- S: [0, 1] (identical to HSV)
- P: [0, 1] (perceived brightness)

**Notes:** P = sqrt(0.299*R^2 + 0.587*G^2 + 0.114*B^2). Round-trip accuracy is within 1e-6.

---

### HSPLog (Log Saturation HSP)

**Valid domain:**
- H: [0, 1] (normalized, identical to HSV/HSP)
- S: [0, 1] (log-stretched: log10(1 + 9*S))
- P: [0, 1] (perceived brightness, identical to HSP)

**Notes:** Logarithmic saturation stretching of HSP. S_log = log10(1 + 9*S) expands low saturations for log/flat footage. Round-trip accuracy is within 1e-6. No published specification; formula is an approximation.

---

### HSY (Luma-Weighted)

**Valid domain:**
- H: [0, 1] (normalized, identical to HSV)
- S: [0, 1] (luma-aware saturation)
- Y: [0, 1] (BT.601 weighted luma)

**Notes:** Y preserves exact BT.601 luma through round-trip. Saturation uses max_sat remapping per hue sector.

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
alwan_rgb_to_hsv(&hsv, &gray, 1, 0, 0);  // Output first
// hsv.x (hue) is 0, but meaningless
```

---

### Out-of-Gamut Colors

Colors outside the RGB cube [0,1]^3:

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
| Division | divisor < eps | Clamped to eps |

---

### Very Large Values (HDR)

**HDR range support:**

| Function | Max Value (Float) | Max Value (Double) |
|----------|-------------------|-------------------|
| XYZ → Lab | 10^6 | 10^15 |
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
// Each color processed independently (output first)
alwan_xyz_to_lab(lab, xyz, &d65, 1000000, s, s);
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
alwan_xyz_to_lab(&lab, &xyz, &alwan_d65_xyz, 1, 0, 0);  // Output first

// Expected (from Python Colour): L=81.968, a=-7.528, b=17.211
// Float: Should match within ±0.001
// Double: Should match within ±1e-9
```

---

### 4. Test Round-Trip Accuracy

```c
alwan_vec3 xyz_orig = {0.5, 0.6, 0.4};
alwan_vec3 lab, xyz_reconstructed;

alwan_xyz_to_lab(&lab, &xyz_orig, &alwan_d65_xyz, 1, 0, 0);  // Output first
alwan_lab_to_xyz(&xyz_reconstructed, &lab, &alwan_d65_xyz, 1, 0, 0);  // Output first

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
