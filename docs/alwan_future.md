# Alwan Future

This document describes future development plans for the Alwan color science library.

---

## Table of Contents

1. [Cross-Platform API (C, HLSL, GLSL, Halide)](#cross-platform-api-c-hlsl-glsl-halide)
2. [Missing API Functions](#missing-api-functions)
3. [Syntactic Sugar](#syntactic-sugar)
4. [Infrastructure](#infrastructure)
5. [TODO Checklist](#todo-checklist)

---

## Cross-Platform API (C, HLSL, GLSL, Halide)

### Current State

The codebase already has good foundations for abstraction:

| Feature | Current Implementation |
|---------|----------------------|
| Scalar type | `alwan_scalar` (float/double via `ALWAN_SCALAR_IS_FLOAT`) |
| Math functions | `ALWAN_SQRT`, `ALWAN_POW`, etc. macros |
| Literals | `ALWAN_LITERAL(x)` for float/double suffix |
| Color types | Semantic structs layout-compatible with `alwan_vec3` |

### Key Challenges

| Feature | C | HLSL | GLSL | Halide |
|---------|---|------|------|--------|
| Scalar type | `float`/`double` | `float` | `float` | `Halide::Expr` |
| Branching | `if/else` | Discouraged (divergence) | Discouraged (divergence) | `select()` only |
| Pointers | Yes | No | No | No (Func/Buffer) |
| Loops | Yes | Limited unrolling | Limited unrolling | Schedules |
| Math | `math.h` | Intrinsics | Builtins | `Halide::` functions |
| Memory | Dynamic | No | No | Compile-time |

---

### Proposed Architecture

#### New Header: `alwan_platform.h`

This header defines the cross-platform abstraction layer:

```c
/*
 * alwan_platform.h - Cross-platform abstraction layer
 * Supports: C, HLSL, GLSL, Halide
 */

#ifndef ALWAN_PLATFORM_H
#define ALWAN_PLATFORM_H

/* ================================================================
 * Backend Detection
 * ================================================================ */

#if defined(ALWAN_BACKEND_HALIDE)
    #define ALWAN_BACKEND 3
#elif defined(GL_core_profile) || defined(GL_es_profile)
    #define ALWAN_BACKEND 2
#elif defined(ALWAN_BACKEND_HLSL) || defined(__HLSL_VERSION)
    #define ALWAN_BACKEND 1
#else
    #define ALWAN_BACKEND 0  /* C/C++ */
#endif

/* ================================================================
 * Scalar Type Abstraction
 * ================================================================ */

#if ALWAN_BACKEND == 3  /* Halide */
    #include <Halide.h>
    typedef Halide::Expr alwan_scalar;
    typedef Halide::Expr alwan_bool;
    #define ALWAN_CONSTEXPR
    #define ALWAN_INLINE inline

#elif ALWAN_BACKEND == 1  /* HLSL */
    typedef float alwan_scalar;
    typedef bool  alwan_bool;
    #define ALWAN_CONSTEXPR
    #define ALWAN_INLINE inline

#else  /* C/C++ */
    #if ALWAN_SCALAR_IS_FLOAT
        typedef float alwan_scalar;
    #else
        typedef double alwan_scalar;
    #endif
    typedef int alwan_bool;
    #define ALWAN_CONSTEXPR static const
    #define ALWAN_INLINE static inline
#endif

/* ================================================================
 * Literal Construction
 * ================================================================ */

#if ALWAN_BACKEND == 3  /* Halide */
    #define ALWAN_LITERAL(x) Halide::Expr(x)
    #define ALWAN_ZERO       Halide::Expr(0.0f)
    #define ALWAN_ONE        Halide::Expr(1.0f)

#elif ALWAN_BACKEND == 1  /* HLSL */
    #define ALWAN_LITERAL(x) ((float)(x))
    #define ALWAN_ZERO       0.0f
    #define ALWAN_ONE        1.0f

#else  /* C */
    #if ALWAN_SCALAR_IS_FLOAT
        #define ALWAN_LITERAL(x) (x##f)
    #else
        #define ALWAN_LITERAL(x) (x)
    #endif
    #define ALWAN_ZERO ALWAN_LITERAL(0.0)
    #define ALWAN_ONE  ALWAN_LITERAL(1.0)
#endif

/* ================================================================
 * Math Functions Abstraction
 * ================================================================ */

#if ALWAN_BACKEND == 3  /* Halide */
    #define ALWAN_SQRT(x)     Halide::sqrt(x)
    #define ALWAN_CBRT(x)     Halide::pow((x), ALWAN_LITERAL(1.0/3.0))
    #define ALWAN_POW(x, y)   Halide::pow((x), (y))
    #define ALWAN_EXP(x)      Halide::exp(x)
    #define ALWAN_LOG(x)      Halide::log(x)
    #define ALWAN_LOG2(x)     (Halide::log(x) / Halide::log(ALWAN_LITERAL(2.0)))
    #define ALWAN_LOG10(x)    Halide::log(x) / Halide::log(ALWAN_LITERAL(10.0))
    #define ALWAN_SIN(x)      Halide::sin(x)
    #define ALWAN_COS(x)      Halide::cos(x)
    #define ALWAN_TAN(x)      Halide::tan(x)
    #define ALWAN_ASIN(x)     Halide::asin(x)
    #define ALWAN_ACOS(x)     Halide::acos(x)
    #define ALWAN_ATAN(x)     Halide::atan(x)
    #define ALWAN_ATAN2(y, x) Halide::atan2((y), (x))
    #define ALWAN_FLOOR(x)    Halide::floor(x)
    #define ALWAN_CEIL(x)     Halide::ceil(x)
    #define ALWAN_FABS(x)     Halide::abs(x)
    #define ALWAN_FMOD(x, y)  ((x) - Halide::floor((x)/(y)) * (y))
    #define ALWAN_TANH(x)     Halide::tanh(x)

#elif ALWAN_BACKEND == 1  /* HLSL */
    #define ALWAN_SQRT(x)     sqrt(x)
    #define ALWAN_CBRT(x)     pow((x), 1.0f/3.0f)
    #define ALWAN_POW(x, y)   pow((x), (y))
    #define ALWAN_EXP(x)      exp(x)
    #define ALWAN_LOG(x)      log(x)
    #define ALWAN_LOG2(x)     log2(x)
    #define ALWAN_LOG10(x)    log10(x)
    #define ALWAN_SIN(x)      sin(x)
    #define ALWAN_COS(x)      cos(x)
    #define ALWAN_TAN(x)      tan(x)
    #define ALWAN_ASIN(x)     asin(x)
    #define ALWAN_ACOS(x)     acos(x)
    #define ALWAN_ATAN(x)     atan(x)
    #define ALWAN_ATAN2(y, x) atan2((y), (x))
    #define ALWAN_FLOOR(x)    floor(x)
    #define ALWAN_CEIL(x)     ceil(x)
    #define ALWAN_FABS(x)     abs(x)
    #define ALWAN_FMOD(x, y)  fmod((x), (y))
    #define ALWAN_TANH(x)     tanh(x)

#else  /* C - existing macros from alwan_internal.h */
    /* Keep current float/double aware macros */
#endif

/* ================================================================
 * Branchless Select (Critical for HLSL/Halide)
 * ================================================================ */

#if ALWAN_BACKEND == 3  /* Halide */
    #define ALWAN_SELECT(cond, true_val, false_val) \
        Halide::select((cond), (true_val), (false_val))
    #define ALWAN_IF(cond, true_val, false_val) \
        ALWAN_SELECT(cond, true_val, false_val)

#elif ALWAN_BACKEND == 1  /* HLSL */
    #define ALWAN_SELECT(cond, true_val, false_val) \
        ((cond) ? (true_val) : (false_val))
    #define ALWAN_IF(cond, true_val, false_val) \
        ALWAN_SELECT(cond, true_val, false_val)

#else  /* C - can use ternary or real if/else */
    #define ALWAN_SELECT(cond, true_val, false_val) \
        ((cond) ? (true_val) : (false_val))
    #define ALWAN_IF(cond, true_val, false_val) \
        ALWAN_SELECT(cond, true_val, false_val)
#endif

/* ================================================================
 * Vector Type Abstraction
 * ================================================================ */

#if ALWAN_BACKEND == 3  /* Halide */
    /* Halide uses Tuple for multi-value returns */
    typedef struct {
        Halide::Expr x, y, z;
    } alwan_vec3;

    typedef struct {
        Halide::Expr x, y;
    } alwan_vec2;

    /* Color types */
    typedef struct { Halide::Expr r, g, b; } alwan_rgb;
    typedef struct { Halide::Expr x, y, z; } alwan_xyz;
    typedef struct { Halide::Expr L, a, b; } alwan_lab;
    typedef struct { Halide::Expr L, a, b; } alwan_oklab;
    /* ... etc */

#elif ALWAN_BACKEND == 1  /* HLSL */
    /* HLSL native types */
    typedef float3 alwan_vec3;
    typedef float2 alwan_vec2;

    /* For semantic types, use structs or typedef to float3 */
    typedef float3 alwan_rgb;
    typedef float3 alwan_xyz;
    typedef float3 alwan_lab;
    typedef float3 alwan_oklab;
    /* ... etc */

#else  /* C - existing definitions */
    /* Keep current struct definitions from alwan.h */
#endif

/* ================================================================
 * Matrix Type Abstraction
 * ================================================================ */

#if ALWAN_BACKEND == 3  /* Halide */
    typedef struct {
        Halide::Expr m[9];
    } alwan_mat3x3;

#elif ALWAN_BACKEND == 1  /* HLSL */
    typedef float3x3 alwan_mat3x3;

#else  /* C */
    /* Keep existing */
#endif

/* ================================================================
 * Utility Macros (Branchless)
 * ================================================================ */

#if ALWAN_BACKEND == 3  /* Halide */
    #define alwan_min(a, b)    Halide::min((a), (b))
    #define alwan_max(a, b)    Halide::max((a), (b))
    #define alwan_clamp(x, lo, hi) Halide::clamp((x), (lo), (hi))
    #define alwan_saturate(x)  Halide::clamp((x), ALWAN_ZERO, ALWAN_ONE)
    #define alwan_lerp(a, b, t) Halide::lerp((a), (b), (t))
    #define alwan_sign(x)      Halide::select((x) > ALWAN_ZERO, ALWAN_ONE, \
                               Halide::select((x) < ALWAN_ZERO, -ALWAN_ONE, ALWAN_ZERO))

#elif ALWAN_BACKEND == 1  /* HLSL */
    #define alwan_min(a, b)    min((a), (b))
    #define alwan_max(a, b)    max((a), (b))
    #define alwan_clamp(x, lo, hi) clamp((x), (lo), (hi))
    #define alwan_saturate(x)  saturate(x)
    #define alwan_lerp(a, b, t) lerp((a), (b), (t))
    #define alwan_sign(x)      sign(x)

#else  /* C */
    #define alwan_min(a, b)    ((a) < (b) ? (a) : (b))
    #define alwan_max(a, b)    ((a) > (b) ? (a) : (b))
    #define alwan_clamp(x, lo, hi) alwan_min(alwan_max((x), (lo)), (hi))
    #define alwan_saturate(x)  alwan_clamp((x), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0))
    #define alwan_lerp(a, b, t) ((a) + (t) * ((b) - (a)))
    #define alwan_sign(x)      (((x) > 0) - ((x) < 0))
#endif

#endif /* ALWAN_PLATFORM_H */
```

---

### Refactoring Patterns

#### Pattern 1: Branchless Functions

Transform branching functions to branchless equivalents using `ALWAN_SELECT`.

**Before (current):**
```c
static inline alwan_scalar lab_f(alwan_scalar t) {
    alwan_scalar const delta3 = ALWAN_LITERAL(0.008856451679);
    if (t > delta3) {
        return ALWAN_CBRT(t);
    } else {
        return ALWAN_LITERAL(7.787037037) * t + ALWAN_LITERAL(0.137931034);
    }
}
```

**After (branchless):**
```c
ALWAN_INLINE alwan_scalar lab_f(alwan_scalar t) {
    alwan_scalar const delta3 = ALWAN_LITERAL(0.008856451679);
    alwan_scalar const kappa = ALWAN_LITERAL(7.787037037);
    alwan_scalar const offset = ALWAN_LITERAL(0.137931034);

    alwan_scalar linear_result = kappa * t + offset;
    alwan_scalar cbrt_result = ALWAN_CBRT(t);

    return ALWAN_SELECT(t > delta3, cbrt_result, linear_result);
}
```

#### Pattern 2: Value Return vs Pointers

For pure mathematical functions (no context/state), provide value-returning versions:

**Before (output-first convention):**
```c
void alwan_xyz_to_oklab(alwan_oklab *oklab, alwan_xyz const *xyz);
```

**After (dual API):**
```c
/* Pointer version for C (backward compatible, output first) */
#if ALWAN_BACKEND == 0
void alwan_xyz_to_oklab(alwan_oklab *oklab, alwan_xyz const *xyz);
#endif

/* Value version for all backends */
ALWAN_INLINE alwan_oklab alwan_xyz_to_oklab_v(alwan_xyz xyz) {
    alwan_oklab result;
    /* ... implementation ... */
    return result;
}
```

#### Pattern 3: sRGB Transfer Function (Branchless)

**Before:**
```c
static alwan_scalar srgb_oetf_scalar(alwan_scalar linear) {
    if (linear <= ALWAN_LITERAL(0.0031308)) {
        return ALWAN_LITERAL(12.92) * linear;
    } else {
        return ALWAN_LITERAL(1.055) * ALWAN_POW(linear, ALWAN_LITERAL(1.0/2.4))
               - ALWAN_LITERAL(0.055);
    }
}
```

**After:**
```c
ALWAN_INLINE alwan_scalar alwan_srgb_oetf(alwan_scalar linear) {
    alwan_scalar const threshold = ALWAN_LITERAL(0.0031308);
    alwan_scalar const scale_linear = ALWAN_LITERAL(12.92);
    alwan_scalar const scale_gamma = ALWAN_LITERAL(1.055);
    alwan_scalar const offset = ALWAN_LITERAL(0.055);
    alwan_scalar const inv_gamma = ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4);

    alwan_scalar linear_result = scale_linear * linear;
    alwan_scalar gamma_result = scale_gamma * ALWAN_POW(linear, inv_gamma) - offset;

    return ALWAN_SELECT(linear <= threshold, linear_result, gamma_result);
}
```

---

### Matrix Operations

HLSL has native matrix support. Provide backend-specific alternatives:

```c
#if ALWAN_BACKEND == 1  /* HLSL */
    #define alwan_mat3_mulv(out, m, v) (out) = mul((m), (v))
    #define alwan_mat3_mul(out, a, b)  (out) = mul((a), (b))

#elif ALWAN_BACKEND == 3  /* Halide - unrolled */
    ALWAN_INLINE alwan_vec3 alwan_mat3_mulv_v(alwan_mat3x3 m, alwan_vec3 v) {
        alwan_vec3 result;
        result.x = m.m[0]*v.x + m.m[1]*v.y + m.m[2]*v.z;
        result.y = m.m[3]*v.x + m.m[4]*v.y + m.m[5]*v.z;
        result.z = m.m[6]*v.x + m.m[7]*v.y + m.m[8]*v.z;
        return result;
    }

#else  /* C - existing implementation (output first) */
    void alwan_mat3_mulv(alwan_vec3 *out, alwan_mat3x3 const *m, alwan_vec3 const *v);
#endif
```

---

### Proposed File Structure

```
src/alwan/
├── alwan_platform.h       # NEW: Cross-platform abstraction
├── alwan_config.h         # Existing (minimal changes)
├── alwan_types.h          # NEW: Platform-aware type definitions
├── alwan_core.h           # NEW: Branchless pure math functions (header-only)
├── alwan_hlsl.h           # HLSL backend bootstrap
├── alwan_glsl.h           # GLSL backend bootstrap
├── alwan_halide.h         # Halide backend bootstrap
├── alwan.h                # Existing public API (C-only features)
├── alwan_internal.h       # Existing
└── *.c                    # C implementations (ALWAN_BACKEND == 0 only)
```

---

### Function Portability Classification

#### Port to All Backends (Stateless, Pure Math)

These functions should be converted to header-only, branchless implementations:

| Category | Functions |
|----------|-----------|
| **Transfer Functions** | sRGB, BT.709, BT.2020, PQ, HLG, Log curves (S-Log3, LogC, etc.) |
| **Perceptual Spaces** | XYZ ↔ Oklab/Oklch, XYZ ↔ JzAzBz/JzCzhz, XYZ ↔ ICtCp, XYZ ↔ IPT |
| **CIE Spaces** | XYZ ↔ Lab/LCh, XYZ ↔ Luv/LChuv, XYZ ↔ xyY |
| **RGB Spaces** | RGB ↔ HSV, RGB ↔ HSL, RGB ↔ YCbCr, RGB ↔ YCoCg |
| **Matrix Ops** | Matrix multiply, matrix-vector multiply, determinant |
| **Delta E** | ΔE76, ΔE94, ΔECMC (simple formulas) |
| **Chromatic Adaptation** | Single-point CAT (Bradford, CAT02, CAT16) |
| **Utilities** | min, max, clamp, saturate, lerp, sign |

#### Keep C-Only (Stateful, Complex, or Data-Dependent)

| Category | Reason |
|----------|--------|
| **Context Management** | Dynamic memory, state |
| **SPD Operations** | Variable-length arrays |
| **Interpolation** | Dynamic LUT data |
| **Color Checker / Munsell** | Lookup tables |
| **Gamut Mapping** | Iterative algorithms |
| **CRI/CQS/TM-30** | Large reference data |
| **Matrix Inversion** | Pivoting with conditionals |
| **Spectrum Upsampling** | Complex LUT interpolation |

---

### Example: Complete Oklab Implementation (Header-Only)

All numerical constants come from CSV files generated by `gendata/` — no hardcoded literals in source.

```c
/* alwan_oklab_core.h - Header-only, cross-platform */

#ifndef ALWAN_OKLAB_CORE_H
#define ALWAN_OKLAB_CORE_H

#include "alwan_platform.h"

/* M1: XYZ to LMS (data from gendata/) */
ALWAN_CONSTEXPR alwan_scalar M1_OKLAB[9] = {
#include "data/matrices/oklab_m1.csv"
};

/* M2: LMS' to Lab */
ALWAN_CONSTEXPR alwan_scalar M2_OKLAB[9] = {
#include "data/matrices/oklab_m2.csv"
};

/* M1_inv: LMS to XYZ */
ALWAN_CONSTEXPR alwan_scalar M1_INV_OKLAB[9] = {
#include "data/matrices/oklab_m1_inv.csv"
};

/* M2_inv: Lab to LMS' */
ALWAN_CONSTEXPR alwan_scalar M2_INV_OKLAB[9] = {
#include "data/matrices/oklab_m2_inv.csv"
};

/* XYZ -> Oklab */
ALWAN_INLINE alwan_oklab alwan_xyz_to_oklab_v(alwan_xyz xyz) {
    /* XYZ -> LMS */
    alwan_scalar l = M1_OKLAB[0]*xyz.x + M1_OKLAB[1]*xyz.y + M1_OKLAB[2]*xyz.z;
    alwan_scalar m = M1_OKLAB[3]*xyz.x + M1_OKLAB[4]*xyz.y + M1_OKLAB[5]*xyz.z;
    alwan_scalar s = M1_OKLAB[6]*xyz.x + M1_OKLAB[7]*xyz.y + M1_OKLAB[8]*xyz.z;

    /* LMS -> LMS' (cube root) */
    alwan_scalar lp = ALWAN_CBRT(l);
    alwan_scalar mp = ALWAN_CBRT(m);
    alwan_scalar sp = ALWAN_CBRT(s);

    /* LMS' -> Lab */
    alwan_oklab result;
    result.L = M2_OKLAB[0]*lp + M2_OKLAB[1]*mp + M2_OKLAB[2]*sp;
    result.a = M2_OKLAB[3]*lp + M2_OKLAB[4]*mp + M2_OKLAB[5]*sp;
    result.b = M2_OKLAB[6]*lp + M2_OKLAB[7]*mp + M2_OKLAB[8]*sp;

    return result;
}

/* Oklab -> XYZ */
ALWAN_INLINE alwan_xyz alwan_oklab_to_xyz_v(alwan_oklab oklab) {
    /* Lab -> LMS' */
    alwan_scalar lp = M2_INV_OKLAB[0]*oklab.L + M2_INV_OKLAB[1]*oklab.a + M2_INV_OKLAB[2]*oklab.b;
    alwan_scalar mp = M2_INV_OKLAB[3]*oklab.L + M2_INV_OKLAB[4]*oklab.a + M2_INV_OKLAB[5]*oklab.b;
    alwan_scalar sp = M2_INV_OKLAB[6]*oklab.L + M2_INV_OKLAB[7]*oklab.a + M2_INV_OKLAB[8]*oklab.b;

    /* LMS' -> LMS (cube) */
    alwan_scalar l = lp * lp * lp;
    alwan_scalar m = mp * mp * mp;
    alwan_scalar s = sp * sp * sp;

    /* LMS -> XYZ */
    alwan_xyz result;
    result.x = M1_INV_OKLAB[0]*l + M1_INV_OKLAB[1]*m + M1_INV_OKLAB[2]*s;
    result.y = M1_INV_OKLAB[3]*l + M1_INV_OKLAB[4]*m + M1_INV_OKLAB[5]*s;
    result.z = M1_INV_OKLAB[6]*l + M1_INV_OKLAB[7]*m + M1_INV_OKLAB[8]*s;

    return result;
}

/* Oklab -> Oklch */
ALWAN_INLINE alwan_oklch alwan_oklab_to_oklch_v(alwan_oklab oklab) {
    alwan_oklch result;
    result.L = oklab.L;
    result.C = ALWAN_SQRT(oklab.a * oklab.a + oklab.b * oklab.b);
    result.h = ALWAN_ATAN2(oklab.b, oklab.a);
    return result;
}

/* Oklch -> Oklab */
ALWAN_INLINE alwan_oklab alwan_oklch_to_oklab_v(alwan_oklch oklch) {
    alwan_oklab result;
    result.L = oklch.L;
    result.a = oklch.C * ALWAN_COS(oklch.h);
    result.b = oklch.C * ALWAN_SIN(oklch.h);
    return result;
}

#endif /* ALWAN_OKLAB_CORE_H */
```

---

### Example: HLSL Usage

```hlsl
#define ALWAN_BACKEND_HLSL
#include "alwan_platform.h"
#include "alwan_oklab_core.h"
#include "alwan_srgb_core.h"  // sRGB-to-XYZ matrix from CSV

float4 PSMain(float4 color : COLOR) : SV_Target {
    // sRGB -> XYZ using matrix loaded from data/matrices/srgb_to_xyz.csv
    alwan_xyz xyz = alwan_srgb_to_xyz_v(color.rgb);

    alwan_oklab oklab = alwan_xyz_to_oklab_v(xyz);

    // Modify in Oklab space
    oklab.L *= 1.1;  // Brighten

    alwan_xyz xyz_out = alwan_oklab_to_xyz_v(oklab);

    return float4(xyz_out.x, xyz_out.y, xyz_out.z, color.a);
}
```

---

### Example: Halide Usage

```cpp
#define ALWAN_BACKEND_HALIDE
#include "alwan_platform.h"
#include "alwan_oklab_core.h"

class OklabGenerator : public Halide::Generator<OklabGenerator> {
public:
    Input<Buffer<float, 3>> input{"input"};
    Output<Buffer<float, 3>> output{"output"};

    void generate() {
        Var x("x"), y("y"), c("c");

        // Create XYZ from input
        alwan_xyz xyz;
        xyz.x = input(x, y, 0);
        xyz.y = input(x, y, 1);
        xyz.z = input(x, y, 2);

        // Convert to Oklab
        alwan_oklab oklab = alwan_xyz_to_oklab_v(xyz);

        // Convert back
        alwan_xyz xyz_out = alwan_oklab_to_xyz_v(oklab);

        output(x, y, 0) = xyz_out.x;
        output(x, y, 1) = xyz_out.y;
        output(x, y, 2) = xyz_out.z;
    }
};
```

---

### Cross-Platform Notes

- The `_v` suffix convention (e.g., `alwan_xyz_to_oklab_v`) denotes value-returning functions
- All header-only functions use `ALWAN_INLINE` for proper inlining
- Constants use `ALWAN_CONSTEXPR` which maps to `static const` in C
- The original pointer-based C API remains unchanged for backward compatibility
- Complex algorithms (gamut mapping, spectral operations) remain C-only

---

## Missing API Functions

Functions referenced in documentation that do NOT exist in `alwan.h`.

### Gamut

| Function | Description |
|----------|-------------|
| `alwan_is_in_gamut(rgb, space)` | Check if RGB is within a specific RGB color space gamut |
| `alwan_is_on_spectral_locus(xy)` | Check if chromaticity is on spectral locus |
| `alwan_get_gamut_boundary(space, ...)` | Get gamut boundary mesh/points for visualization |

### Spectral

| Function | Description |
|----------|-------------|
| `alwan_get_cmf(observer, ...)` | Get color matching functions (CIE 1931/1964) |

---

## Syntactic Sugar

Convenience wrappers that can be implemented using existing functions.

### Gamut Mapping Shortcuts

```c
// alwan_gamut_clip - shortcut for:
alwan_gamut_map_interleave(&rgb_out, ALWAN_GAMUT_MAP_CLIP, &rgb_in, count);

// alwan_gamut_map_perceptual - shortcut for:
alwan_gamut_map_interleave(&rgb_out, ALWAN_GAMUT_MAP_PERCEPTUAL, &rgb_in, count);

// alwan_gamut_map_hue_preserving - shortcut for:
alwan_gamut_map_interleave(&rgb_out, ALWAN_GAMUT_MAP_HUE_PRESERVING, &rgb_in, count);
```

---

## Infrastructure

### Runtime Data Loading

**Status:** Not implemented

Runtime data loading mode (`ALWAN_EMBED_DATA=0`) is not supported. Currently only embedded mode (`ALWAN_EMBED_DATA=1`, default) works.

**Blocked by:**
- File system abstraction for cross-platform support
- Data file format specification
- Installation path detection

### Build System

- Linux/macOS build support (currently Windows-only with Sharpmake)
- Package manager support (vcpkg, conan)

### Testing

- Add benchmarks for performance regression testing
- Add fuzz testing for edge cases
- Cross-platform CI (Linux, macOS)

---

## TODO Checklist

### API - Missing Functions

- [ ] Implement `alwan_is_in_gamut(rgb, space)` - check if RGB is within gamut
- [ ] Implement `alwan_is_on_spectral_locus(xy)` - check if on spectral locus
- [ ] Implement `alwan_get_gamut_boundary(space, ...)` - gamut boundary for visualization
- [ ] Implement `alwan_get_cmf(observer, ...)` - get color matching functions

### API - Syntactic Sugar

- [ ] Add `alwan_gamut_clip()` convenience wrapper
- [ ] Add `alwan_gamut_map_perceptual()` convenience wrapper
- [ ] Add `alwan_gamut_map_hue_preserving()` convenience wrapper

### Cross-Platform (HLSL/GLSL/Halide)

- [x] Create `alwan_platform.h` with backend detection and math macros
- [x] Create `alwan_types.h` with platform-aware `alwan_vec2`, `alwan_vec3`, `alwan_mat3x3`
- [x] Create `alwan_core.h` with branchless transfer functions (sRGB, BT.709, PQ, HLG, gamma)
- [x] Convert functions to use `ALWAN_SELECT` (branchless) — **396 uses** across 26 `*_core.h` files
- [x] Add value-returning `_v()` variants — **260 functions** across 29 `*_core.h` files
- [x] Create header-only core modules — **30 `*_core.h` files** covering all modules:
  - Color spaces: `colorspace`, `oklab`, `jzazbz`, `ipt`, `ictcp`, `din99`, `hunter_lab`, `prolab`, `extended`, `osa_ucs`, `convenience`
  - CAMs: `cam` (CIECAM02/CAM16), `hunt`, `hellwig2022`, `zcam`, `kim2009`, `llab`, `rlab`, `atd95`
  - Processing: `rgb` (25+ transfer functions), `cat`, `color_correction`, `vision`, `math`, `quality`, `rayleigh`, `gamut`, `view`, `aces_ff`
- [x] Unroll/remove loops in matrix ops — `alwan_math_core.h`: identity, det, mulv, mul, inv all unrolled
- [x] Add GLSL backend (`alwan_glsl.h` + platform layer support)
- [ ] Test with HLSL shader
- [ ] Test with GLSL shader
- [ ] Test with Halide generator

### Infrastructure

- [ ] Implement runtime data loading (`ALWAN_EMBED_DATA=0`)
- [ ] Add Linux build support
- [ ] Add macOS build support
- [ ] Add vcpkg package support
- [ ] Add conan package support

### Testing

- [ ] Add performance benchmarks
- [ ] Add fuzz testing
- [ ] Set up Linux CI
- [ ] Set up macOS CI
