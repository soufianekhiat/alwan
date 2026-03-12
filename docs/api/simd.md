# SIMD Architecture Reference

Compile-time SIMD layer used internally by the [map API](map.md). Not part of the public API -- exposed here for contributors and advanced users building custom map kernels.

## ISA Selection

Compile-time only, no runtime dispatch. Highest available ISA wins:

```
alwan_simd.h
  -> __AVX2__  : alwan_simd_avx2.h
  -> __AVX__   : alwan_simd_avx.h
  -> __SSE2__  : alwan_simd_sse2.h  (x64 baseline)
  -> fallback  : alwan_simd_scalar.h (width=1, plain C)
```

## Lane Widths

| ISA | F32 | F64 | U8 | U16 |
|-----|-----|-----|----|-----|
| AVX2 | 8 | 4 | 32 | 16 |
| AVX | 8 | 4 | 16 | 8 |
| SSE2 | 4 | 2 | 16 | 8 |
| Scalar | 1 | 1 | 1 | 1 |

Width macros:

```c
ALWAN_SIMD_F32_WIDTH    // e.g. 8 on AVX2
ALWAN_SIMD_F64_WIDTH    // e.g. 4 on AVX2
ALWAN_SIMD_UINT8_WIDTH
ALWAN_SIMD_UINT16_WIDTH
```

## Types

### ISA-specific types

```c
alwan_simd_f32       // __m256 on AVX2, __m128 on SSE2, float on scalar
alwan_simd_f64       // __m256d on AVX2, __m128d on SSE2, double on scalar
alwan_simd_u8        // __m256i on AVX2, __m128i on SSE2
alwan_simd_u16       // __m256i on AVX2, __m128i on SSE2
alwan_simd_i32       // __m256i on AVX2, __m128i on SSE2
alwan_simd_f32_mask  // same as alwan_simd_f32 (bitwise masks)
alwan_simd_f64_mask  // same as alwan_simd_f64
```

### Generic aliases (in `alwan_map_internal.h`)

Map code uses type-generic aliases that resolve based on `ALWAN_SCALAR_IS_FLOAT`:

```c
alwan_simd          // alwan_simd_f32 or alwan_simd_f64
alwan_simd_mask     // corresponding mask type
alwan_simd_lane     // float or double
ALWAN_SIMD_WIDTH    // F32 or F64 width
```

## Operations

All operations have `alwan_simd_f32_*` and `alwan_simd_f64_*` variants, plus generic `alwan_simd_*` aliases.

### Arithmetic

| Operation | Signature |
|-----------|-----------|
| `add(a, b)` | a + b |
| `sub(a, b)` | a - b |
| `mul(a, b)` | a * b |
| `div(a, b)` | a / b |
| `neg(a)` | -a |
| `abs(a)` | \|a\| |
| `fmadd(a, b, c)` | a*b + c (fused on FMA-capable hardware) |
| `fmsub(a, b, c)` | a*b - c |
| `rcp(a)` | 1.0 / a (exact, not approximate) |

### Math

| Operation | Description |
|-----------|-------------|
| `sqrt(a)` | square root |
| `cbrt(a)` | cube root |
| `pow(base, exp)` | exponentiation |
| `exp(a)` | e^a |
| `log(a)` | natural log |
| `log2(a)` | base-2 log |
| `log10(a)` | base-10 log |
| `sin(a)` | sine |
| `cos(a)` | cosine |
| `atan2(y, x)` | two-argument arctangent |

Uses SVML (Intel Short Vector Math Library) when available (MSVC x64, Intel compilers). Falls back to per-lane `libm` calls otherwise.

### Rounding

| Operation | Description |
|-----------|-------------|
| `floor(a)` | round toward -inf |
| `ceil(a)` | round toward +inf |
| `round(a)` | round to nearest |
| `trunc(a)` | round toward zero |

### Min / Max / Clamp

| Operation | Description |
|-----------|-------------|
| `min(a, b)` | per-lane minimum |
| `max(a, b)` | per-lane maximum |
| `clamp(v, lo, hi)` | clamp to [lo, hi] |

### Comparison (return mask)

| Operation | Condition |
|-----------|-----------|
| `cmpeq(a, b)` | a == b |
| `cmplt(a, b)` | a < b |
| `cmple(a, b)` | a <= b |
| `cmpgt(a, b)` | a > b |
| `cmpge(a, b)` | a >= b |

### Branchless Select

```c
alwan_simd_select(mask, if_true, if_false)
```

Per-lane: chooses `if_true` where mask bits are set, `if_false` otherwise. Core mechanism for branchless color math (replaces all `if/else` in transfer functions, piecewise formulas, etc.).

### Load / Store / Broadcast

| Operation | Description |
|-----------|-------------|
| `load(ptr)` | load W lanes from aligned memory |
| `store(ptr, v)` | store W lanes to aligned memory |
| `set1(scalar)` | broadcast scalar to all lanes |
| `zero()` | all-zero vector |

### Reduction (F32 only)

| Operation | Description |
|-----------|-------------|
| `hadd(a, b)` | horizontal add (adjacent pairs) |
| `hsum(a)` | sum all lanes to scalar |

## Color-Math Building Blocks

Defined in `alwan_map_internal.h`. Process `ALWAN_SIMD_WIDTH` pixels per call.

### Matrix multiply

```c
alwan__mat3_mul_simd(&out_x, &out_y, &out_z, &matrix, in_x, in_y, in_z)
```

3x3 matrix times [x,y,z] for W pixels. Used by all matrix-based color space conversions and `alwan_mat3_transform_map_interleave`.

### Transfer functions

```c
alwan__srgb_eotf_simd(v)       // sRGB encoded -> linear
alwan__srgb_oetf_simd(v)       // sRGB linear -> encoded
alwan__pq_eotf_simd(v)         // PQ ST 2084 encoded -> linear
alwan__pq_oetf_simd(v)         // PQ linear -> encoded
alwan__pq_jz_eotf_simd(v)      // Jzazbz-specific PQ variant
alwan__pq_jz_oetf_simd(v)
alwan__hlg_eotf_simd(v)        // HLG encoded -> linear
alwan__hlg_oetf_simd(v)        // HLG linear -> encoded
```

All use `alwan_simd_select` for branchless piecewise evaluation.

### CIE Lab

```c
alwan__lab_f_simd(t)            // Lab f(t) companding
alwan__lab_f_inv_simd(t)        // inverse
```

### Utilities

```c
alwan__simd_min3(a, b, c)       // min of three vectors
alwan__simd_max3(a, b, c)       // max of three vectors
```

## Tile Processing Model

Map functions process pixels in tiles to maximize cache utilization.

### Tile dimensions

```c
#define ALWAN_TILE_W       128   // pixels wide
#define ALWAN_TILE_H        32   // pixels tall
#define ALWAN_TILE_PIXELS  4096  // 128 * 32
```

Scratch buffers per tile (3 channels, Structure-of-Arrays layout):
- F32: 3 x 4096 x 4 = 48 KB (fits L1 cache)
- F64: 3 x 4096 x 8 = 96 KB (fits L2)

### Data flow

```
Input (AoS, strided)            Scratch (SoA, packed)            Output (AoS, strided)
[R0 G0 B0] [R1 G1 B1] ...  ->  c0: [R0 R1 R2 ...]         ->  [X0 Y0 Z0] [X1 Y1 Z1] ...
                                 c1: [G0 G1 G2 ...]
                                 c2: [B0 B1 B2 ...]
```

1. **Load**: `alwan__load_tile_aos3` deinterleaves AoS input into SoA scratch buffers
2. **SIMD loop**: process W lanes per iteration using `alwan_simd_load` / `alwan_simd_store`
3. **Scalar tail**: remaining `tile_count % W` pixels processed with scalar core functions
4. **Store**: `alwan__store_tile_aos3` reinterleaves SoA scratch back to AoS output

### Pseudocode

```c
size_t processed = 0;
while (processed < count) {
    size_t tile = min(ALWAN_TILE_PIXELS, count - processed);

    alwan__load_tile_aos3(c0, c1, c2, in, processed, in_stride, tile);

    // SIMD: W lanes per iteration
    for (i = 0; i + W <= tile; i += W) {
        alwan_simd r = alwan_simd_load(&c0[i]);
        alwan_simd g = alwan_simd_load(&c1[i]);
        alwan_simd b = alwan_simd_load(&c2[i]);
        // ... color math ...
        alwan_simd_store(&c0[i], out_x);
        alwan_simd_store(&c1[i], out_y);
        alwan_simd_store(&c2[i], out_z);
    }

    // Scalar tail
    for (; i < tile; i++) {
        // ... scalar core function ...
    }

    alwan__store_tile_aos3(out, processed, out_stride, c0, c1, c2, tile);
    processed += tile;
}
```

### Divide-by-zero guards

```c
#define ALWAN_MAP_DIV_GUARD    1e-10   // chromaticity / xyY denominators
#define ALWAN_MAP_PQ_DIV_GUARD 1e-30   // PQ EOTF denominator
```

Used with `alwan_simd_cmpgt` + `alwan_simd_select` to avoid NaN/Inf in SIMD paths.

## Macro Patterns

Internal macros for generating map function bodies. Used by implementors adding new color spaces.

### Tile kernel generation

```c
// Generates a scalar-per-element kernel from a core _v function
ALWAN_TILE_KERNEL_3TO3(name, InT, OutT, core_fn,
                        in_field0, in_field1, in_field2,
                        out_field0, out_field1, out_field2)
```

### Tiled loop

```c
// Full tile load -> SIMD+scalar loop -> store
ALWAN_MAP3_TILED(in_ptr, in_stride, out_ptr, out_stride, count, kernel_fn)
```

### _map_interleave_ex function generation

```c
ALWAN_MAP3_EX(name, InT, OutT, core_fn, ...)           // basic 3->3
ALWAN_MAP3_EX_WHITE(name, ...)                          // + alwan_xyz const *white
ALWAN_MAP3_EX_PQ(name, ...)                             // + int use_pq
ALWAN_MAP3_EX_STATUS(name, ...)                         // core returns status code
ALWAN_MAP3_EX_V(name, ...)                              // core returns by value
ALWAN_MAP3_EX_V_WHITE(name, ...)                        // value-returning + white
ALWAN_MAP3_EX_V_INT(name, ...)                          // value-returning + int param
ALWAN_MAP3_EX_V_SCALAR(name, ...)                       // value-returning + scalar param
```

## Files

```
src/alwan/simd/
    alwan_simd.h             # ISA dispatch (include this one)
    alwan_simd_types.h       # width constants, type aliases, SVML detection
    alwan_simd_avx2.h        # AVX2 intrinsic wrappers
    alwan_simd_avx.h         # AVX intrinsic wrappers
    alwan_simd_sse2.h        # SSE2 intrinsic wrappers (+ SSE3/SSSE3/SSE4.x)
    alwan_simd_scalar.h      # scalar fallback (width=1)

src/alwan/map/
    alwan_map_internal.h     # SIMD building blocks, tile helpers, macros
    alwan_rgb_map_interleave.c          # sRGB convenience map implementations
    alwan_colorspace_map_interleave.c   # CIE colorspace maps
    alwan_oklab_map_interleave.c        # Oklab/OkLCh maps
    alwan_convenience_map_interleave.c  # HSV, HSL, CMY, YCoCg, HWB maps
    alwan_ictcp_map_interleave.c        # ICtCp maps
    alwan_ipt_map_interleave.c          # IPT maps
    alwan_jzazbz_map_interleave.c       # Jzazbz/JzCzhz maps
    alwan_cam_map_interleave.c          # CIECAM02, CAM16 maps
    alwan_math_map_interleave.c         # mat3 transform map
    alwan_typed_map_interleave.c        # all _map_interleave_ex instantiations (macro-generated)
```
