# SIMD Architecture Reference

Compile-time SIMD layer used internally by the [map API](map.md). Not part of the public API -- exposed here for contributors and advanced users building custom map kernels.

Related: [backends.md](backends.md) (HLSL/GLSL/Halide GPU backends and the cross-backend math contract) and [determinism.md](../determinism.md) (the bit-exact counterpart of this layer: SIMD width collapse, canonical reductions, polynomial transfer functions).

## ISA Selection

Compile-time only, no runtime dispatch. Highest available ISA wins (`src/alwan/simd/alwan_simd.h`):

```
alwan_simd.h
  -> __AVX2__                          : alwan_simd_avx2.h
  -> __AVX__                           : alwan_simd_avx.h
  -> __SSE2__ / _M_X64 / _M_AMD64      : alwan_simd_sse2.h   (x64 baseline)
  -> __aarch64__ / __ARM_NEON          : alwan_simd_neon.h   (ARM)
  -> fallback                          : alwan_simd_scalar.h (width=1, plain C)
```

After the backend is selected, `alwan_simd.h` includes `alwan_simd_reduce.h`, which defines the public horizontal-reduction names (`alwan_simd_*_hsum` / `_hadd`) on top of the backend's `*_native` ops. The ISA is fixed at compile time by the target's `-march` / `/arch` flags -- see the `ALWAN_SIMD_ARCH` CMake cache var and the Sharpmake Release `/arch:AVX2` default.

## Lane Widths

| ISA | F32 | F64 | U8 | U16 |
|-----|-----|-----|----|-----|
| AVX2 | 8 | 4 | 32 | 16 |
| AVX | 8 | 4 | 16 | 8 |
| SSE2 | 4 | 2 | 16 | 8 |
| NEON | 4 | 2 | 16 | 8 |
| Scalar | 1 | 1 | 1 | 1 |

NEON F64 width is 2 on `__aarch64__`; on ARMv7 (`__ARM_NEON` without `__aarch64__`) there is no f64 SIMD, so `alwan_simd_f64` falls back to scalar `double` (the f32 lanes still vectorize).

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
alwan_simd_f32       // __m256 on AVX2, __m128 on SSE2, float32x4_t on NEON, float on scalar
alwan_simd_f64       // __m256d on AVX2, __m128d on SSE2, float64x2_t on aarch64, double on scalar/ARMv7
alwan_simd_u8        // __m256i on AVX2, __m128i on SSE2, uint8x16_t on NEON
alwan_simd_u16       // __m256i on AVX2, __m128i on SSE2, uint16x8_t on NEON
alwan_simd_i32       // __m256i on AVX2, __m128i on SSE2, int32x4_t on NEON
alwan_simd_f32_mask  // same as alwan_simd_f32 (bitwise masks); uint32x4_t on NEON
alwan_simd_f64_mask  // same as alwan_simd_f64; uint64x2_t on aarch64
```

### Generic aliases

Two distinct alias sets exist:

**Always-f64 aliases (in `alwan_map_internal.h`)** -- used by the image-conversion path and the color-math building blocks below. These are fixed to f64 regardless of build precision:

```c
alwan_simd          // == alwan_simd_f64
alwan_simd_mask     // == alwan_simd_f64_mask
alwan_simd_lane     // double
ALWAN_SIMD_WIDTH    // == ALWAN_SIMD_F64_WIDTH
```

**Precision-parameterized aliases (in `alwan_map_simd_defs.h`)** -- the `alwan_map_*`-prefixed set the per-precision map kernels (`*_map_kernels.inc`) use. The file is included once per precision pass (with `ALWAN_MAP_F32` or `ALWAN_MAP_F64` defined) and resolves to that pass's types:

```c
alwan_map_simd        // alwan_simd_f32 or alwan_simd_f64
alwan_map_simd_mask   // corresponding mask type
alwan_map_lane        // float or double
ALWAN_MAP_SIMD_WIDTH  // F32 or F64 width (forced to 1 in ALWAN_DETERMINISTIC=1)
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
| `rcp(a)` | 1.0 / a (exact true division, not the approximate `rcpps` / `vrecpeq`) |

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

`ALWAN_HAS_SVML` (`alwan_simd_types.h`) is 1 on Intel compilers (`__INTEL_COMPILER` / `__INTEL_LLVM_COMPILER`) and on MSVC x64 (ships `svml_dispmd.lib`); 0 otherwise. When set, the transcendentals above call true vector intrinsics (`_mm_cbrt_ps`, `_mm_pow_ps`, `_mm256_*`, ...). When unset (notably GCC/Clang and **NEON**), they fall back to minimax-polynomial vector kernels in the backend, or to per-lane `libm` where no polynomial twin exists.

### Fast-mode scalar/SIMD agreement (`alwan_fast_pow.h`)

The sRGB / BT.1886 transfer functions reduce to `pow(x, 2.4)` and `pow(x, 1/2.4)`. The SIMD map kernels evaluate these with `alwan_simd_{f32,f64}_pow24` / `_pow_inv24`. Their **scalar twins** live in `src/alwan/core/alwan_fast_pow.h`:

```c
alwan_fast_pow24_f32(x)      alwan_fast_pow24_f64(x)        // pow(x, 2.4)
alwan_fast_pow_inv24_f32(x)  alwan_fast_pow_inv24_f64(x)    // pow(x, 1/2.4)
```

Both files are gated on `ALWAN_HAS_SVML` so the scalar `_v` API and the SIMD map path agree on every platform:

- **SVML present:** the SIMD pow kernels use libm-accurate `_mm_pow_*`, so the scalar twin also calls libm `pow`/`powf`.
- **No SVML (e.g. NEON):** the SIMD kernels use the polynomial (atanh-series `log2` on the mantissa + degree-10 `exp2` Taylor), so the scalar twin runs the *identical* decomposition -- same coefficients, same operation order, same bit-exact mantissa/exponent split. This keeps `floor(y)` identical to the vector path (no power-of-two boundary divergence) and avoids the scalar path drifting from the SIMD path through libm.

Accuracy of the polynomial path vs libm is ~5e-10 absolute over [0,1] -- the documented fast-mode floor. In **deterministic mode** these fast twins are not used: the `alwan_det_*` polynomials in `alwan_platform.h` replace them (see [determinism.md](../determinism.md)).

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

### Reduction

| Operation | Description |
|-----------|-------------|
| `f32_hadd(a, b)` | horizontal add (adjacent pairs), F32 |
| `f32_hsum(a)` | sum all F32 lanes to a broadcast scalar |
| `f64_hsum(a)` | sum all F64 lanes to a broadcast scalar |

These public names are defined in `alwan_simd_reduce.h`. In fast mode they forward to the backend's `*_native` op (whatever lane order the ISA finds convenient -- SSE2 4-lane tree, AVX 8-lane tree, NEON `vaddvq_*`). In `ALWAN_DETERMINISTIC=1` mode they instead use a canonical left-to-right scalar reduction that is bit-exact across SSE/AVX/NEON/scalar. Element-wise ops are unaffected by this switch; only reductions change. Never reference the `*_native` suffix from call sites.

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
#define ALWAN_TILE_W        64   // pixels wide
#define ALWAN_TILE_H        32   // pixels tall
#define ALWAN_TILE_PIXELS 2048   // 64 * 32
```

Scratch buffers per tile (3 channels, Structure-of-Arrays layout):
- F32: 3 x 2048 x 4 = 24 KB (fits L1 cache)
- F64: 3 x 2048 x 8 = 48 KB (fits L1/L2)

The per-precision map kernels (`*_map_kernels.inc`) use their own tile sizing through `alwan_map_simd_defs.h`: `ALWAN_MAP_TILE_PIXELS` is 4096 on the f32 pass and 2048 on the f64 pass.

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
    alwan_simd_reduce.h      # deterministic-aware horizontal reductions (hsum/hadd)
    alwan_simd_avx2.h        # AVX2 intrinsic wrappers
    alwan_simd_avx.h         # AVX intrinsic wrappers
    alwan_simd_sse2.h        # SSE2 intrinsic wrappers (+ SSE3/SSSE3/SSE4.x)
    alwan_simd_neon.h        # ARM NEON intrinsic wrappers (aarch64 + ARMv7)
    alwan_simd_scalar.h      # scalar fallback (width=1)

src/alwan/core/
    alwan_fast_pow.h         # scalar pow24 / pow_inv24 twins of the SIMD kernels

src/alwan/map/
    alwan_map_internal.h     # always-f64 SIMD building blocks, tile helpers, macros
    alwan_map_simd_defs.h    # per-precision alwan_map_* aliases (one pass each)
    alwan_rgb_map.c          # sRGB convenience map implementations
    alwan_colorspace_map.c   # CIE colorspace maps
    alwan_oklab_map.c        # Oklab/OkLCh maps
    alwan_convenience_map.c       # HSV, HSL, CMY, YCoCg, HWB maps
    alwan_convenience_extra_map.c # additional convenience-model maps
    alwan_ictcp_map.c        # ICtCp maps
    alwan_ipt_map.c          # IPT maps
    alwan_jzazbz_map.c       # Jzazbz/JzCzhz maps
    alwan_cam_map.c          # CIECAM02, CAM16 maps
    alwan_color_correction_map.c  # lift/gamma/gain, CDL, CCM maps
    alwan_extended_map.c     # extended-space batch maps
    alwan_vision_map.c       # CVD simulation maps
    alwan_gamut_map.c        # gamut mapping (incl. NEON kernels)
    alwan_mat3_map.c         # mat3 transform map
    alwan_typed_map.c        # typed _ex instantiations (any U8/U16/F16/F32/F64)
    alwan_typed_planar_map.c # typed planar (SoA) _ex instantiations
```
