# Map API — Batch Processing & SIMD Architecture

Batch color transforms backed by a compile-time SIMD atomic operations library. Three function families cover different pixel layouts and data types.

---

## 1. Terminology

- **Map function**: applies a color transform to an array of pixels. Three variants:
  - `_map_interleave` — interleaved `alwan_scalar*` buffers with stride
  - `_map_interleave_ex` — interleaved `void*` buffers with `alwan_pixel_format` (U8/U16/F32/F64)
  - `_map_planar` — separate per-channel `alwan_scalar*` buffers
  - `_map_planar_ex` — separate per-channel `void*` buffers with `alwan_pixel_format`
- **Atomic SIMD op**: a single SIMD primitive (`add`, `mul`, `pow`, `select`, ...) inlined by the compiler
- **Compile-time dispatch**: `#ifdef` selects ISA at build time — no runtime feature detection, no function pointers

---

## 2. Directory Layout

```
src/alwan/
├── simd/
│   ├── alwan_simd.h              ← public include, compile-time ISA select
│   ├── alwan_simd_types.h        ← ALWAN_SIMD_*_WIDTH, type aliases
│   ├── alwan_simd_scalar.h       ← fallback: width=1, plain C
│   ├── alwan_simd_sse2.h         ← SSE2 intrinsics (x64 baseline), with SSE3/SSSE3/SSE4.x upgrades
│   ├── alwan_simd_avx.h          ← AVX intrinsics (256-bit float, 128-bit int, no FMA)
│   └── alwan_simd_avx2.h         ← AVX2 intrinsics (256-bit float+int, FMA)
├── map/
│   ├── alwan_map_internal.h      ← shared SIMD building blocks (load/store/eotf/mat3)
│   ├── alwan_rgb_map_interleave.c           ← sRGB↔XYZ, sRGB↔Lab, sRGB↔Oklab
│   ├── alwan_colorspace_map_interleave.c    ← XYZ↔Lab, XYZ↔Luv, Lab↔LCh, Luv↔LChuv, XYZ↔xyY
│   ├── alwan_oklab_map_interleave.c         ← XYZ↔Oklab, Oklab↔Oklch
│   ├── alwan_convenience_map_interleave.c   ← RGB↔HSV, RGB↔HSL
│   ├── alwan_cam_map_interleave.c           ← CIECAM02, CAM16 forward/inverse
│   ├── alwan_ictcp_map_interleave.c         ← RGB↔ICtCp, XYZ↔ICtCp
│   ├── alwan_ipt_map_interleave.c           ← XYZ↔IPT
│   ├── alwan_jzazbz_map_interleave.c        ← XYZ↔Jzazbz, Jzazbz↔JzCzhz
│   └── alwan_math_map_interleave.c          ← mat3 transform
```

---

## 3. SIMD Width Constants

Defined in `alwan_simd_types.h`. Width = number of lanes per SIMD register for each element type.

```c
#if defined(__AVX2__)
#  define ALWAN_SIMD_UINT8_WIDTH   32
#  define ALWAN_SIMD_UINT16_WIDTH  16
#  define ALWAN_SIMD_F32_WIDTH      8
#  define ALWAN_SIMD_F64_WIDTH      4
#elif defined(__AVX__)
#  define ALWAN_SIMD_UINT8_WIDTH   16   // AVX has no 256-bit integer ops
#  define ALWAN_SIMD_UINT16_WIDTH   8   // still 128-bit for integers
#  define ALWAN_SIMD_F32_WIDTH      8   // 256-bit float
#  define ALWAN_SIMD_F64_WIDTH      4   // 256-bit double
#elif defined(__SSE2__) || defined(_M_X64)
#  define ALWAN_SIMD_UINT8_WIDTH   16
#  define ALWAN_SIMD_UINT16_WIDTH   8
#  define ALWAN_SIMD_F32_WIDTH      4
#  define ALWAN_SIMD_F64_WIDTH      2
#else
#  define ALWAN_SIMD_UINT8_WIDTH    1
#  define ALWAN_SIMD_UINT16_WIDTH   1
#  define ALWAN_SIMD_F32_WIDTH      1
#  define ALWAN_SIMD_F64_WIDTH      1
#endif
```

| ISA | uint8 | uint16 | float32 | float64 | Notes |
|-----|-------|--------|---------|---------|-------|
| Scalar | 1 | 1 | 1 | 1 | Plain C fallback |
| SSE2 (128-bit) | 16 | 8 | 4 | 2 | x64 baseline |
| SSE3 | 16 | 8 | 4 | 2 | Adds `haddps` (horizontal add) |
| SSSE3 | 16 | 8 | 4 | 2 | Adds `pshufb` (byte shuffle) |
| SSE4.1 | 16 | 8 | 4 | 2 | Adds `blendvps`, `roundps`, `dpps`, `pmovzx*` |
| SSE4.2 | 16 | 8 | 4 | 2 | Adds `pcmpgtq` (minor) |
| AVX (256-bit float, 128-bit int) | 16 | 8 | 8 | 4 | 256-bit float; implies SSE4.2 |
| AVX2 (256-bit) | 32 | 16 | 8 | 4 | 256-bit int + FMA3 |

**Width only changes at SSE2 → AVX → AVX2 boundaries.** SSE3/SSSE3/SSE4.x don't widen registers — they add instructions that enable faster implementations of the same atomic ops (e.g., `select` goes from 3 instructions on SSE2 to 1 on SSE4.1).

**AVX vs AVX2**: AVX introduced 256-bit float registers (`__m256`, `__m256d`) but integer SIMD remains 128-bit (`__m128i`). AVX2 extends 256-bit to integer operations. FMA3 is technically a separate extension but is present on all AVX2-capable CPUs, so FMA is only available in the AVX2 backend.

### 3.1 ISA Implication Chain

```
SSE2 < SSE3 < SSSE3 < SSE4.1 < SSE4.2 < AVX < AVX2
```

AVX implies SSE4.2 (and everything below). So the AVX and AVX2 backends can freely use all SSE4.1 instructions without additional guards. The progressive `#ifdef` upgrades only matter within the SSE2 backend, where `__SSE3__`, `__SSSE3__`, `__SSE4_1__` may or may not be defined depending on compiler flags.

---

## 4. SIMD Type Aliases

```c
// alwan_simd_types.h

#if defined(__AVX2__)
#  include <immintrin.h>
   typedef __m256   alwan_simd_f32;
   typedef __m256d  alwan_simd_f64;
   typedef __m256i  alwan_simd_u8;
   typedef __m256i  alwan_simd_u16;
   typedef __m256i  alwan_simd_i32;     // used for masks, conversions
   typedef __m256   alwan_simd_f32_mask;
   typedef __m256d  alwan_simd_f64_mask;
#elif defined(__AVX__)
#  include <immintrin.h>
   typedef __m256   alwan_simd_f32;      // 256-bit float
   typedef __m256d  alwan_simd_f64;      // 256-bit double
   typedef __m128i  alwan_simd_u8;       // 128-bit (AVX has no 256-bit int)
   typedef __m128i  alwan_simd_u16;      // 128-bit
   typedef __m128i  alwan_simd_i32;      // 128-bit
   typedef __m256   alwan_simd_f32_mask;
   typedef __m256d  alwan_simd_f64_mask;
#elif defined(__SSE2__) || defined(_M_X64)
#  include <emmintrin.h>
#  if defined(__SSE3__)
#    include <pmmintrin.h>
#  endif
#  if defined(__SSSE3__)
#    include <tmmintrin.h>
#  endif
#  if defined(__SSE4_1__)
#    include <smmintrin.h>
#  endif
#  if defined(__SSE4_2__)
#    include <nmmintrin.h>
#  endif
   typedef __m128   alwan_simd_f32;
   typedef __m128d  alwan_simd_f64;
   typedef __m128i  alwan_simd_u8;
   typedef __m128i  alwan_simd_u16;
   typedef __m128i  alwan_simd_i32;
   typedef __m128   alwan_simd_f32_mask;
   typedef __m128d  alwan_simd_f64_mask;
#else
   typedef float    alwan_simd_f32;
   typedef double   alwan_simd_f64;
   typedef uint8_t  alwan_simd_u8;
   typedef uint16_t alwan_simd_u16;
   typedef int32_t  alwan_simd_i32;
   typedef int      alwan_simd_f32_mask;
   typedef int      alwan_simd_f64_mask;
#endif
```

---

## 5. Atomic SIMD Operations

Each backend header (`alwan_simd_scalar.h`, `alwan_simd_sse2.h`, `alwan_simd_avx.h`, `alwan_simd_avx2.h`) implements the same set of `ALWAN_INLINE` functions. The map layer calls only these — never raw intrinsics.

All math is **exact**. No approximations. Transcendentals (`pow`, `exp`, `log`, `cbrt`, etc.) extract lanes, call `libm` per lane, and repack.

### 5.1 Float32 Operations

```c
// --- Arithmetic ---
alwan_simd_f32 alwan_simd_f32_add(alwan_simd_f32 a, alwan_simd_f32 b);
alwan_simd_f32 alwan_simd_f32_sub(alwan_simd_f32 a, alwan_simd_f32 b);
alwan_simd_f32 alwan_simd_f32_mul(alwan_simd_f32 a, alwan_simd_f32 b);
alwan_simd_f32 alwan_simd_f32_div(alwan_simd_f32 a, alwan_simd_f32 b);
alwan_simd_f32 alwan_simd_f32_neg(alwan_simd_f32 a);

// --- Broadcast / Zero ---
alwan_simd_f32 alwan_simd_f32_set1(float v);
alwan_simd_f32 alwan_simd_f32_zero(void);

// --- Math (EXACT — libm per lane for transcendentals) ---
alwan_simd_f32 alwan_simd_f32_sqrt(alwan_simd_f32 a);       // hw: sqrtps
alwan_simd_f32 alwan_simd_f32_abs(alwan_simd_f32 a);        // hw: andps mask
alwan_simd_f32 alwan_simd_f32_pow(alwan_simd_f32 base, alwan_simd_f32 exp);  // libm powf
alwan_simd_f32 alwan_simd_f32_cbrt(alwan_simd_f32 a);       // libm cbrtf
alwan_simd_f32 alwan_simd_f32_exp(alwan_simd_f32 a);        // libm expf
alwan_simd_f32 alwan_simd_f32_log(alwan_simd_f32 a);        // libm logf
alwan_simd_f32 alwan_simd_f32_log2(alwan_simd_f32 a);       // libm log2f
alwan_simd_f32 alwan_simd_f32_log10(alwan_simd_f32 a);      // libm log10f
alwan_simd_f32 alwan_simd_f32_sin(alwan_simd_f32 a);        // libm sinf
alwan_simd_f32 alwan_simd_f32_cos(alwan_simd_f32 a);        // libm cosf
alwan_simd_f32 alwan_simd_f32_atan2(alwan_simd_f32 y, alwan_simd_f32 x); // libm atan2f

// --- FMA ---
alwan_simd_f32 alwan_simd_f32_fmadd(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c);  // a*b + c
alwan_simd_f32 alwan_simd_f32_fmsub(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c);  // a*b - c

// --- Comparison → Mask ---
alwan_simd_f32_mask alwan_simd_f32_cmpeq(alwan_simd_f32 a, alwan_simd_f32 b);
alwan_simd_f32_mask alwan_simd_f32_cmplt(alwan_simd_f32 a, alwan_simd_f32 b);
alwan_simd_f32_mask alwan_simd_f32_cmple(alwan_simd_f32 a, alwan_simd_f32 b);
alwan_simd_f32_mask alwan_simd_f32_cmpgt(alwan_simd_f32 a, alwan_simd_f32 b);
alwan_simd_f32_mask alwan_simd_f32_cmpge(alwan_simd_f32 a, alwan_simd_f32 b);

// --- Select (branchless ternary: mask ? a : b) ---
// SSE4.1+: blendvps (1 instr); SSE2: AND/ANDNOT/OR (3 instr)
alwan_simd_f32 alwan_simd_f32_select(alwan_simd_f32_mask m, alwan_simd_f32 a, alwan_simd_f32 b);

// --- Min / Max ---
alwan_simd_f32 alwan_simd_f32_min(alwan_simd_f32 a, alwan_simd_f32 b);  // hw: minps
alwan_simd_f32 alwan_simd_f32_max(alwan_simd_f32 a, alwan_simd_f32 b);  // hw: maxps

// --- Clamp ---
alwan_simd_f32 alwan_simd_f32_clamp(alwan_simd_f32 v, alwan_simd_f32 lo, alwan_simd_f32 hi);

// --- Horizontal add (pairwise adjacent) ---
// [a0,a1,a2,a3] + [b0,b1,b2,b3] → [a0+a1, a2+a3, b0+b1, b2+b3]
// SSE3+: haddps (1 instr); SSE2: shuffle+add (2 instr)
alwan_simd_f32 alwan_simd_f32_hadd(alwan_simd_f32 a, alwan_simd_f32 b);

// --- Horizontal sum (reduce all lanes to scalar, broadcast to all lanes) ---
// Uses hadd iteratively; SSE3: 2x haddps; SSE2: shuffle+add chain
alwan_simd_f32 alwan_simd_f32_hsum(alwan_simd_f32 a);

// --- Dot product ---
// SSE4.1: dpps (1 instr); SSE2: mul + hadd chain
alwan_simd_f32 alwan_simd_f32_dot3(alwan_simd_f32 a, alwan_simd_f32 b);  // 3-component dot
alwan_simd_f32 alwan_simd_f32_dot4(alwan_simd_f32 a, alwan_simd_f32 b);  // 4-component dot

// --- Rounding ---
// SSE4.1: roundps (1 instr); SSE2: cast-to-int-and-back with fixup
alwan_simd_f32 alwan_simd_f32_floor(alwan_simd_f32 a);
alwan_simd_f32 alwan_simd_f32_ceil(alwan_simd_f32 a);
alwan_simd_f32 alwan_simd_f32_round(alwan_simd_f32 a);    // round to nearest even
alwan_simd_f32 alwan_simd_f32_trunc(alwan_simd_f32 a);    // round toward zero

// --- Reciprocal (exact: 1.0/x via divps, NOT approximate rcpps) ---
alwan_simd_f32 alwan_simd_f32_rcp(alwan_simd_f32 a);

// --- Load / Store ---
alwan_simd_f32 alwan_simd_f32_load(const float *ptr);        // aligned
alwan_simd_f32 alwan_simd_f32_loadu(const float *ptr);       // unaligned
void           alwan_simd_f32_store(float *ptr, alwan_simd_f32 v);   // aligned
void           alwan_simd_f32_storeu(float *ptr, alwan_simd_f32 v);  // unaligned
```

### 5.2 Float64 Operations

Same set as float32 with `_f64` suffix. Calls `pow`/`cbrt`/`exp`/`log` (double) per lane.

```c
alwan_simd_f64 alwan_simd_f64_add(alwan_simd_f64 a, alwan_simd_f64 b);
alwan_simd_f64 alwan_simd_f64_pow(alwan_simd_f64 base, alwan_simd_f64 exp); // libm pow (double)
alwan_simd_f64 alwan_simd_f64_hadd(alwan_simd_f64 a, alwan_simd_f64 b);
alwan_simd_f64 alwan_simd_f64_hsum(alwan_simd_f64 a);
alwan_simd_f64 alwan_simd_f64_floor(alwan_simd_f64 a);
alwan_simd_f64 alwan_simd_f64_ceil(alwan_simd_f64 a);
// ... same full pattern as f32
```

### 5.3 Integer Load/Store & Conversion

For pixel type I/O — load uint8/uint16, convert to float for processing, convert back on store.

```c
// --- uint8 ---
alwan_simd_u8  alwan_simd_u8_load(const uint8_t *ptr);
alwan_simd_u8  alwan_simd_u8_loadu(const uint8_t *ptr);
void           alwan_simd_u8_store(uint8_t *ptr, alwan_simd_u8 v);
void           alwan_simd_u8_storeu(uint8_t *ptr, alwan_simd_u8 v);

// --- uint16 ---
alwan_simd_u16 alwan_simd_u16_load(const uint16_t *ptr);
alwan_simd_u16 alwan_simd_u16_loadu(const uint16_t *ptr);
void           alwan_simd_u16_store(uint16_t *ptr, alwan_simd_u16 v);
void           alwan_simd_u16_storeu(uint16_t *ptr, alwan_simd_u16 v);

// --- Widening conversions (for load path) ---
// Extract the low ALWAN_SIMD_F32_WIDTH elements and widen to f32
// SSE4.1: pmovzxbd/pmovzxwd (1 instr); SSE2: unpacklo + shift chain
alwan_simd_f32 alwan_simd_u8_to_f32(alwan_simd_u8 v);    // low N uint8 → float
alwan_simd_f32 alwan_simd_u16_to_f32(alwan_simd_u16 v);   // low N uint16 → float

// --- Narrowing conversions (for store path) ---
alwan_simd_u8  alwan_simd_f32_to_u8(alwan_simd_f32 v);    // float → uint8 (saturate)
alwan_simd_u16 alwan_simd_f32_to_u16(alwan_simd_f32 v);   // float → uint16 (saturate)

// --- Normalization helpers ---
// uint8 → [0,1] float:  (float)val / 255
// uint16 → [0,1] float: (float)val / max_val  (max_val from bit_depth)
alwan_simd_f32 alwan_simd_u8_to_f32_norm(alwan_simd_u8 v);                        // /255
alwan_simd_f32 alwan_simd_u16_to_f32_norm(alwan_simd_u16 v, alwan_simd_f32 inv_max); // /max

alwan_simd_u8  alwan_simd_f32_to_u8_norm(alwan_simd_f32 v);                        // *255 + clamp
alwan_simd_u16 alwan_simd_f32_to_u16_norm(alwan_simd_f32 v, alwan_simd_f32 max_val); // *max + clamp

// --- Byte shuffle (deinterleave/swizzle) ---
// SSSE3: pshufb (1 instr); SSE2: manual unpack chain
alwan_simd_u8 alwan_simd_u8_shuffle(alwan_simd_u8 v, alwan_simd_u8 mask);
```

### 5.4 SSE2 Implementation Pattern

SSE2 is the baseline. When SSE3, SSSE3, or SSE4.1 are available at compile time, specific ops upgrade to better instructions via `#ifdef` within this file.

```c
// alwan_simd_sse2.h

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_add(alwan_simd_f32 a, alwan_simd_f32 b) {
    return _mm_add_ps(a, b);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sqrt(alwan_simd_f32 a) {
    return _mm_sqrt_ps(a);  // hardware instruction, exact
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow(alwan_simd_f32 base, alwan_simd_f32 exp) {
    // EXACT: extract, call libm, repack. No polynomial approximation.
    ALWAN_ALIGN(16) float b[4], e[4], r[4];
    _mm_store_ps(b, base);
    _mm_store_ps(e, exp);
    r[0] = powf(b[0], e[0]);
    r[1] = powf(b[1], e[1]);
    r[2] = powf(b[2], e[2]);
    r[3] = powf(b[3], e[3]);
    return _mm_load_ps(r);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_select(alwan_simd_f32_mask m,
                                                   alwan_simd_f32 a,
                                                   alwan_simd_f32 b) {
#if defined(__SSE4_1__)
    return _mm_blendv_ps(b, a, m);  // SSE4.1: 1 instruction
#else
    // SSE2: AND/ANDNOT/OR (3 instructions)
    return _mm_or_ps(_mm_and_ps(m, a), _mm_andnot_ps(m, b));
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmadd(alwan_simd_f32 a,
                                                   alwan_simd_f32 b,
                                                   alwan_simd_f32 c) {
    // SSE2: no FMA instruction, emulate with mul+add
    return _mm_add_ps(_mm_mul_ps(a, b), c);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd(alwan_simd_f32 a, alwan_simd_f32 b) {
#if defined(__SSE3__)
    return _mm_hadd_ps(a, b);  // SSE3: 1 instruction
#else
    // SSE2 fallback: shuffle and add
    // a = [a0,a1,a2,a3], b = [b0,b1,b2,b3]
    // result = [a0+a1, a2+a3, b0+b1, b2+b3]
    __m128 t0 = _mm_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0)); // [a0,a2,b0,b2]
    __m128 t1 = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1)); // [a1,a3,b1,b3]
    return _mm_add_ps(t0, t1);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hsum(alwan_simd_f32 a) {
#if defined(__SSE3__)
    __m128 t = _mm_hadd_ps(a, a);   // [a0+a1, a2+a3, a0+a1, a2+a3]
    return _mm_hadd_ps(t, t);       // [sum, sum, sum, sum]
#else
    __m128 t1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 0, 1)); // [a1,a0,a3,a2]
    __m128 t2 = _mm_add_ps(a, t1);                              // [a0+a1,a0+a1,a2+a3,a2+a3]
    __m128 t3 = _mm_shuffle_ps(t2, t2, _MM_SHUFFLE(0, 1, 2, 3));
    return _mm_add_ps(t2, t3);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_dot3(alwan_simd_f32 a, alwan_simd_f32 b) {
#if defined(__SSE4_1__)
    return _mm_dp_ps(a, b, 0x7F);  // mask=0111 (xyz), broadcast to all lanes
#else
    __m128 m = _mm_mul_ps(a, b);
    // m = [a0*b0, a1*b1, a2*b2, a3*b3]
    // sum first 3 lanes
    __m128 t1 = _mm_shuffle_ps(m, m, _MM_SHUFFLE(0, 0, 0, 1));  // [a1*b1, ...]
    __m128 t2 = _mm_add_ss(m, t1);                                // [a0*b0+a1*b1, ...]
    __m128 t3 = _mm_shuffle_ps(m, m, _MM_SHUFFLE(0, 0, 0, 2));  // [a2*b2, ...]
    __m128 sum = _mm_add_ss(t2, t3);
    return _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(0, 0, 0, 0));   // broadcast
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_floor(alwan_simd_f32 a) {
#if defined(__SSE4_1__)
    return _mm_floor_ps(a);  // SSE4.1: roundps with floor mode
#else
    // SSE2: truncate to int and fixup
    __m128i ti = _mm_cvttps_epi32(a);
    __m128 t = _mm_cvtepi32_ps(ti);
    __m128 mask = _mm_cmpgt_ps(t, a);             // t > a means we rounded up
    __m128 one = _mm_set1_ps(1.0f);
    return _mm_sub_ps(t, _mm_and_ps(mask, one));   // subtract 1 where we rounded up
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_ceil(alwan_simd_f32 a) {
#if defined(__SSE4_1__)
    return _mm_ceil_ps(a);
#else
    __m128i ti = _mm_cvttps_epi32(a);
    __m128 t = _mm_cvtepi32_ps(ti);
    __m128 mask = _mm_cmplt_ps(t, a);
    __m128 one = _mm_set1_ps(1.0f);
    return _mm_add_ps(t, _mm_and_ps(mask, one));
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_round(alwan_simd_f32 a) {
#if defined(__SSE4_1__)
    return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
#else
    return _mm_cvtepi32_ps(_mm_cvtps_epi32(a));  // cvtps rounds to nearest
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_trunc(alwan_simd_f32 a) {
#if defined(__SSE4_1__)
    return _mm_round_ps(a, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
#else
    return _mm_cvtepi32_ps(_mm_cvttps_epi32(a));  // cvttps truncates
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_u8_to_f32(alwan_simd_u8 v) {
#if defined(__SSE4_1__)
    __m128i i32 = _mm_cvtepu8_epi32(v);  // pmovzxbd: zero-extend u8→i32
    return _mm_cvtepi32_ps(i32);
#else
    // SSE2: unpack chain u8 → u16 → i32 → f32
    __m128i zero = _mm_setzero_si128();
    __m128i u16 = _mm_unpacklo_epi8(v, zero);    // u8 → u16 (low 8)
    __m128i i32 = _mm_unpacklo_epi16(u16, zero);  // u16 → i32 (low 4)
    return _mm_cvtepi32_ps(i32);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_u16_to_f32(alwan_simd_u16 v) {
#if defined(__SSE4_1__)
    __m128i i32 = _mm_cvtepu16_epi32(v);  // pmovzxwd
    return _mm_cvtepi32_ps(i32);
#else
    __m128i zero = _mm_setzero_si128();
    __m128i i32 = _mm_unpacklo_epi16(v, zero);
    return _mm_cvtepi32_ps(i32);
#endif
}

ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_shuffle(alwan_simd_u8 v, alwan_simd_u8 mask) {
#if defined(__SSSE3__)
    return _mm_shuffle_epi8(v, mask);  // pshufb: 1 instruction
#else
    // SSE2 fallback: scalar extraction (expensive but correct)
    ALWAN_ALIGN(16) uint8_t vb[16], mb[16], rb[16];
    _mm_store_si128((__m128i *)vb, v);
    _mm_store_si128((__m128i *)mb, mask);
    for (int i = 0; i < 16; i++)
        rb[i] = (mb[i] & 0x80) ? 0 : vb[mb[i] & 0x0F];
    return _mm_load_si128((const __m128i *)rb);
#endif
}
```

### 5.5 AVX Implementation Pattern

AVX gives 256-bit float registers (`__m256` / `__m256d`) but integer SIMD stays at 128-bit (`__m128i`). No FMA — emulate with mul+add, same as SSE2. AVX implies SSE4.2, so all SSE instructions are available. The key win over SSE2 is double the float throughput and `vblendvps` for branchless select.

```c
// alwan_simd_avx.h

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_add(alwan_simd_f32 a, alwan_simd_f32 b) {
    return _mm256_add_ps(a, b);  // 8-wide float add
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sqrt(alwan_simd_f32 a) {
    return _mm256_sqrt_ps(a);  // 8-wide hardware sqrt
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow(alwan_simd_f32 base, alwan_simd_f32 exp) {
    ALWAN_ALIGN(32) float b[8], e[8], r[8];
    _mm256_store_ps(b, base);
    _mm256_store_ps(e, exp);
    for (int i = 0; i < 8; i++) r[i] = powf(b[i], e[i]);
    return _mm256_load_ps(r);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_select(alwan_simd_f32_mask m,
                                                   alwan_simd_f32 a,
                                                   alwan_simd_f32 b) {
    return _mm256_blendv_ps(b, a, m);  // AVX vblendvps (available since AVX1)
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmadd(alwan_simd_f32 a,
                                                   alwan_simd_f32 b,
                                                   alwan_simd_f32 c) {
    // AVX: no FMA instruction, emulate with mul+add (same as SSE2 strategy)
    return _mm256_add_ps(_mm256_mul_ps(a, b), c);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd(alwan_simd_f32 a, alwan_simd_f32 b) {
    return _mm256_hadd_ps(a, b);  // AVX vhaddps (in-lane: 128-bit halves independently)
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_floor(alwan_simd_f32 a) {
    return _mm256_floor_ps(a);  // AVX vroundps
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_ceil(alwan_simd_f32 a) {
    return _mm256_ceil_ps(a);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_round(alwan_simd_f32 a) {
    return _mm256_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
}

// Integer ops remain 128-bit — delegate to SSE intrinsics
ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_loadu(const uint8_t *ptr) {
    return _mm_loadu_si128((const __m128i *)ptr);  // 128-bit, same as SSE2
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_u8_to_f32(alwan_simd_u8 v) {
    // SSE4.1 is implied by AVX, so pmovzxbd is always available
    // But we only get 4 floats from u8 → need to call twice for 8-wide f32
    __m128i lo4 = _mm_cvtepu8_epi32(v);
    __m128i hi4 = _mm_cvtepu8_epi32(_mm_srli_si128(v, 4));
    return _mm256_set_m128(_mm_cvtepi32_ps(hi4), _mm_cvtepi32_ps(lo4));
}
```

### 5.6 AVX2 Implementation Pattern

```c
// alwan_simd_avx2.h

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_add(alwan_simd_f32 a, alwan_simd_f32 b) {
    return _mm256_add_ps(a, b);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow(alwan_simd_f32 base, alwan_simd_f32 exp) {
    ALWAN_ALIGN(32) float b[8], e[8], r[8];
    _mm256_store_ps(b, base);
    _mm256_store_ps(e, exp);
    for (int i = 0; i < 8; i++) r[i] = powf(b[i], e[i]);
    return _mm256_load_ps(r);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_select(alwan_simd_f32_mask m,
                                                   alwan_simd_f32 a,
                                                   alwan_simd_f32 b) {
    return _mm256_blendv_ps(b, a, m);  // AVX vblendvps
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmadd(alwan_simd_f32 a,
                                                   alwan_simd_f32 b,
                                                   alwan_simd_f32 c) {
    return _mm256_fmadd_ps(a, b, c);  // FMA3 (implied by AVX2)
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd(alwan_simd_f32 a, alwan_simd_f32 b) {
    return _mm256_hadd_ps(a, b);  // same as AVX
}

// AVX2: 256-bit integer ops now available
ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_loadu(const uint8_t *ptr) {
    return _mm256_loadu_si256((const __m256i *)ptr);  // 256-bit integer load
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_u8_to_f32(alwan_simd_u8 v) {
    // AVX2: vpmovzxbd operates on 256-bit — but only widens 8 bytes → 8 i32
    __m256i i32 = _mm256_cvtepu8_epi32(_mm256_castsi256_si128(v));
    return _mm256_cvtepi32_ps(i32);
}

ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_shuffle(alwan_simd_u8 v, alwan_simd_u8 mask) {
    return _mm256_shuffle_epi8(v, mask);  // AVX2 vpshufb (in-lane)
}
```

### 5.7 Scalar Fallback Pattern

```c
// alwan_simd_scalar.h

typedef float alwan_simd_f32;
typedef int   alwan_simd_f32_mask;

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_add(alwan_simd_f32 a, alwan_simd_f32 b) {
    return a + b;
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow(alwan_simd_f32 base, alwan_simd_f32 exp) {
    return powf(base, exp);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_select(alwan_simd_f32_mask m,
                                                   alwan_simd_f32 a,
                                                   alwan_simd_f32 b) {
    return m ? a : b;
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd(alwan_simd_f32 a, alwan_simd_f32 b) {
    return a + b;  // width=1, trivially just addition
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hsum(alwan_simd_f32 a) {
    return a;  // width=1, already a scalar
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_floor(alwan_simd_f32 a) {
    return floorf(a);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_ceil(alwan_simd_f32 a) {
    return ceilf(a);
}
```

### 5.8 Progressive Upgrade Summary

Which instructions each atomic op uses at each ISA level:

| Atomic Op | SSE2 | SSE3 | SSSE3 | SSE4.1 | AVX | AVX2 |
|-----------|------|------|-------|--------|-----|------|
| `select` | AND/ANDNOT/OR | = | = | `blendvps` | `vblendvps` | = |
| `hadd` | shuffle+add | `haddps` | = | = | `vhaddps` | = |
| `hsum` | shuffle+add chain | 2x `haddps` | = | = | `vhaddps`+cross-lane | = |
| `dot3` | mul+shuffle+add | mul+`haddps` | = | `dpps` | `vdpps` (in-lane) | = |
| `floor` | cvtt+fixup | = | = | `roundps` | `vroundps` | = |
| `ceil` | cvtt+fixup | = | = | `roundps` | `vroundps` | = |
| `round` | `cvtps` | = | = | `roundps` | `vroundps` | = |
| `trunc` | `cvttps` | = | = | `roundps` | `vroundps` | = |
| `fmadd` | mul+add | = | = | = | mul+add | `vfmadd` |
| `u8_to_f32` | unpack chain | = | = | `pmovzxbd` | `pmovzxbd`+`set_m128` | `vpmovzxbd` |
| `u16_to_f32` | unpack chain | = | = | `pmovzxwd` | `pmovzxwd`+`set_m128` | `vpmovzxwd` |
| `u8_shuffle` | scalar extract | = | `pshufb` | = | = | `vpshufb` |
| `sqrt` | `sqrtps` | = | = | = | `vsqrtps` | = |
| `min`/`max` | `minps`/`maxps` | = | = | = | `vminps`/`vmaxps` | = |

`=` means same as the column to its left (inherited from lower ISA level).

---

## 6. Map Function Architecture

### 6.1 Guiding Principle

Each `_map_interleave` function has **one implementation** that uses `alwan_simd_*` atomic ops. The SIMD backend is resolved at compile time. A `#ifdef` gate provides a naive fallback loop calling the `_v` core functions for platforms without SIMD.

### 6.2 Tiling for Cache Efficiency

For interleaved (AoS) input, the SIMD path deinterleaves into temporary SoA float buffers. To keep these buffers in L1 cache, processing is done in tiles of `ALWAN_TILE_PIXELS` pixels.

```c
// alwan_map_internal.h

// Tile size: 128 x 32 = 4096 pixels
// SoA scratch: 3 channels x 4096 x sizeof(float) = 48 KB → fits in L1 (typically 48-64 KB)
#define ALWAN_TILE_W      128
#define ALWAN_TILE_H       32
#define ALWAN_TILE_PIXELS (ALWAN_TILE_W * ALWAN_TILE_H)  // 4096
```

The tile loop:
1. **Load tile**: deinterleave AoS input into 3 SoA float arrays (r[], g[], b[]) of up to `ALWAN_TILE_PIXELS` elements
2. **Process tile**: SIMD inner loop over the SoA arrays, `ALWAN_SIMD_F32_WIDTH` elements per iteration
3. **Store tile**: reinterleave SoA back to AoS output

This makes the AoS-to-SoA overhead negligible — amortized over 4096 pixels of compute per tile.

### 6.3 Structure of a Map Function

```c
// alwan_rgb_map_interleave.c
#include "alwan_simd.h"
#include "core/alwan_core.h"
#include "map/alwan_map_internal.h"

int alwan_srgb_to_xyz_map_interleave(alwan_scalar *xyz_out,
                          alwan_scalar const *rgb_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride)
{
    if (!xyz_out || !rgb_in) return ALWAN_E_INVALID;
    if (count == 0) return ALWAN_OK;

#if ALWAN_SIMD_F32_WIDTH > 1
    /* ---- SIMD path with tiling ---- */
    const size_t W = ALWAN_SIMD_F32_WIDTH;

    ALWAN_ALIGN(ALWAN_SIMD_F32_WIDTH * 4)
    float tile_r[ALWAN_TILE_PIXELS], tile_g[ALWAN_TILE_PIXELS], tile_b[ALWAN_TILE_PIXELS];

    size_t processed = 0;
    while (processed < count) {
        size_t tile_count = count - processed;
        if (tile_count > ALWAN_TILE_PIXELS) tile_count = ALWAN_TILE_PIXELS;

        /* 1. Load tile: AoS → SoA */
        alwan__load_tile_aos(tile_r, tile_g, tile_b,
                             rgb_in, processed, in_stride, tile_count);

        /* 2. Process tile: SIMD inner loop over SoA arrays */
        size_t i = 0;
        for (; i + W <= tile_count; i += W) {
            alwan_simd_f32 r = alwan_simd_f32_load(&tile_r[i]);
            alwan_simd_f32 g = alwan_simd_f32_load(&tile_g[i]);
            alwan_simd_f32 b = alwan_simd_f32_load(&tile_b[i]);

            // sRGB EOTF (exact, branchless)
            r = alwan__srgb_eotf_simd(r);
            g = alwan__srgb_eotf_simd(g);
            b = alwan__srgb_eotf_simd(b);

            // Matrix multiply: XYZ = M * [R,G,B]
            alwan_simd_f32 x, y, z;
            alwan__mat3_mul_simd(&x, &y, &z, &srgb_to_xyz_matrix, r, g, b);

            alwan_simd_f32_store(&tile_r[i], x);
            alwan_simd_f32_store(&tile_g[i], y);
            alwan_simd_f32_store(&tile_b[i], z);
        }
        // Scalar tail within tile
        for (; i < tile_count; i++) {
            alwan_rgb rgb = { tile_r[i], tile_g[i], tile_b[i] };
            alwan_xyz xyz = alwan_srgb_to_xyz_v(rgb);
            tile_r[i] = (float)xyz.x;
            tile_g[i] = (float)xyz.y;
            tile_b[i] = (float)xyz.z;
        }

        /* 3. Store tile: SoA → AoS */
        alwan__store_tile_aos(xyz_out, processed, out_stride,
                              tile_r, tile_g, tile_b, tile_count);

        processed += tile_count;
    }

#else
    /* ---- Naive fallback: loop calling core _v function ---- */
    for (size_t i = 0; i < count; i++) {
        const alwan_scalar *in_ptr  = (const alwan_scalar *)((const char *)rgb_in  + i * in_stride);
        alwan_scalar       *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_rgb rgb = { in_ptr[0], in_ptr[1], in_ptr[2] };
        alwan_xyz xyz = alwan_srgb_to_xyz_v(rgb);
        out_ptr[0] = xyz.x; out_ptr[1] = xyz.y; out_ptr[2] = xyz.z;
    }
#endif

    return ALWAN_OK;
}
```

### 6.4 Shared SIMD Building Blocks (inlined, used by all map functions)

These are **not** in the atomic ops layer — they're color-math helpers built from atomic ops, defined in a shared internal header (`map/alwan_map_internal.h`):

```c
// --- Tile load: AoS strided input -> flat SoA float arrays ---
ALWAN_INLINE void alwan__load_tile_aos(float *r, float *g, float *b,
                                        const alwan_scalar *base, size_t offset,
                                        size_t stride, size_t tile_count) {
    for (size_t j = 0; j < tile_count; j++) {
        const alwan_scalar *p = (const alwan_scalar *)((const char *)base + (offset + j) * stride);
        r[j] = (float)p[0];
        g[j] = (float)p[1];
        b[j] = (float)p[2];
    }
}

// --- Tile store: flat SoA float arrays -> AoS strided output ---
ALWAN_INLINE void alwan__store_tile_aos(alwan_scalar *base, size_t offset, size_t stride,
                                         const float *r, const float *g, const float *b,
                                         size_t tile_count) {
    for (size_t j = 0; j < tile_count; j++) {
        alwan_scalar *p = (alwan_scalar *)((char *)base + (offset + j) * stride);
        p[0] = (alwan_scalar)r[j];
        p[1] = (alwan_scalar)g[j];
        p[2] = (alwan_scalar)b[j];
    }
}

// --- sRGB EOTF (exact, branchless) ---
ALWAN_INLINE alwan_simd_f32 alwan__srgb_eotf_simd(alwan_simd_f32 v) {
    alwan_simd_f32 thresh   = alwan_simd_f32_set1(0.04045f);
    alwan_simd_f32 lo       = alwan_simd_f32_mul(v, alwan_simd_f32_set1(1.0f / 12.92f));
    alwan_simd_f32 hi_base  = alwan_simd_f32_mul(
        alwan_simd_f32_add(v, alwan_simd_f32_set1(0.055f)),
        alwan_simd_f32_set1(1.0f / 1.055f));
    alwan_simd_f32 hi       = alwan_simd_f32_pow(hi_base, alwan_simd_f32_set1(2.4f));
    alwan_simd_f32_mask mask = alwan_simd_f32_cmple(v, thresh);
    return alwan_simd_f32_select(mask, lo, hi);
}

// --- 3x3 matrix x SoA vectors (row-major matrix, SoA r/g/b in, SoA x/y/z out) ---
ALWAN_INLINE void alwan__mat3_mul_simd(alwan_simd_f32 *ox, alwan_simd_f32 *oy, alwan_simd_f32 *oz,
                                        const alwan_mat3x3 *m,
                                        alwan_simd_f32 r, alwan_simd_f32 g, alwan_simd_f32 b) {
    *ox = alwan_simd_f32_fmadd(alwan_simd_f32_set1((float)m->m[0]), r,
          alwan_simd_f32_fmadd(alwan_simd_f32_set1((float)m->m[1]), g,
          alwan_simd_f32_mul(  alwan_simd_f32_set1((float)m->m[2]), b)));
    *oy = alwan_simd_f32_fmadd(alwan_simd_f32_set1((float)m->m[3]), r,
          alwan_simd_f32_fmadd(alwan_simd_f32_set1((float)m->m[4]), g,
          alwan_simd_f32_mul(  alwan_simd_f32_set1((float)m->m[5]), b)));
    *oz = alwan_simd_f32_fmadd(alwan_simd_f32_set1((float)m->m[6]), r,
          alwan_simd_f32_fmadd(alwan_simd_f32_set1((float)m->m[7]), g,
          alwan_simd_f32_mul(  alwan_simd_f32_set1((float)m->m[8]), b)));
}
```

### 6.5 Where the `#ifdef` Lives

```
alwan_simd_scalar.h ← no ifdef (plain C)
alwan_simd_sse2.h   ← #ifdef __SSE2__ || _M_X64 (per atomic op)
                       internal: #ifdef __SSE3__, __SSSE3__, __SSE4_1__ for progressive upgrades
alwan_simd_avx.h    ← #ifdef __AVX__ (per atomic op, 256-bit float / 128-bit int)
alwan_simd_avx2.h   ← #ifdef __AVX2__ (per atomic op, 256-bit float+int, FMA)
        |
        v
alwan_simd.h        ← #ifdef chain selects which backend to include
        |
        v
alwan_*_map_interleave.c       ← #if ALWAN_SIMD_F32_WIDTH > 1 (SIMD path vs naive fallback)
                       NO other #ifdef — all SIMD abstracted by atomic ops
```

---

## 7. Complete Map API Surface

All batch processing functions use the `_map_interleave` suffix for interleaved (AoS) layout. Each also has `_ex`, `_map_planar`, and `_map_planar_ex` variants (see sections 8-9).

### 7.1 RGB Convenience (sRGB shortcuts)

```c
int alwan_srgb_to_xyz_map_interleave   (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_xyz_to_srgb_map_interleave   (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_srgb_to_lab_map_interleave   (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_lab_to_srgb_map_interleave   (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_srgb_to_oklab_map_interleave (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_oklab_to_srgb_map_interleave (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
```

### 7.2 Colorspace Conversions

```c
int alwan_xyz_to_lab_map_interleave    (alwan_scalar *out, const alwan_scalar *in, const alwan_xyz *white, size_t count, size_t in_stride, size_t out_stride);
int alwan_lab_to_xyz_map_interleave    (alwan_scalar *out, const alwan_scalar *in, const alwan_xyz *white, size_t count, size_t in_stride, size_t out_stride);
int alwan_xyz_to_luv_map_interleave    (alwan_scalar *out, const alwan_scalar *in, const alwan_xyz *white, size_t count, size_t in_stride, size_t out_stride);
int alwan_luv_to_xyz_map_interleave    (alwan_scalar *out, const alwan_scalar *in, const alwan_xyz *white, size_t count, size_t in_stride, size_t out_stride);
int alwan_lab_to_lch_map_interleave    (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_lch_to_lab_map_interleave    (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_luv_to_lchuv_map_interleave  (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_lchuv_to_luv_map_interleave  (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_xyz_to_xyy_map_interleave    (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_xyy_to_xyz_map_interleave    (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
```

### 7.3 Oklab / Oklch

```c
int alwan_xyz_to_oklab_map_interleave   (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_oklab_to_xyz_map_interleave   (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_oklab_to_oklch_map_interleave (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_oklch_to_oklab_map_interleave (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
```

### 7.4 ICtCp

```c
int alwan_rgb_to_ictcp_map_interleave  (alwan_scalar *out, const alwan_scalar *in, int use_pq, size_t count, size_t in_stride, size_t out_stride);
int alwan_ictcp_to_rgb_map_interleave  (alwan_scalar *out, const alwan_scalar *in, int use_pq, size_t count, size_t in_stride, size_t out_stride);
int alwan_xyz_to_ictcp_map_interleave  (alwan_scalar *out, const alwan_scalar *in, int use_pq, size_t count, size_t in_stride, size_t out_stride);
int alwan_ictcp_to_xyz_map_interleave  (alwan_scalar *out, const alwan_scalar *in, int use_pq, size_t count, size_t in_stride, size_t out_stride);
```

### 7.5 Jzazbz / JzCzhz

```c
int alwan_xyz_to_jzazbz_map_interleave    (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_jzazbz_to_xyz_map_interleave    (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_jzazbz_to_jzczhz_map_interleave (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_jzczhz_to_jzazbz_map_interleave (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
```

### 7.6 IPT

```c
int alwan_xyz_to_ipt_map_interleave (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_ipt_to_xyz_map_interleave (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
```

### 7.7 CAM Models

```c
int alwan_ciecam02_forward_map_interleave (alwan_ciecam02_correlates *out, const alwan_scalar *xyz_in, const alwan_ciecam02_viewing_conditions *vc, size_t count, size_t in_stride);
int alwan_ciecam02_inverse_map_interleave (alwan_scalar *xyz_out, const alwan_ciecam02_correlates *in, const alwan_ciecam02_viewing_conditions *vc, size_t count, size_t out_stride);
int alwan_cam16_forward_map_interleave    (alwan_cam16_correlates *out, const alwan_scalar *xyz_in, const alwan_cam16_viewing_conditions *vc, size_t count, size_t in_stride);
int alwan_cam16_inverse_map_interleave    (alwan_scalar *xyz_out, const alwan_cam16_correlates *in, const alwan_cam16_viewing_conditions *vc, size_t count, size_t out_stride);
```

### 7.8 Convenience (HSV, HSL)

```c
int alwan_rgb_to_hsv_map_interleave (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_hsv_to_rgb_map_interleave (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_rgb_to_hsl_map_interleave (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
int alwan_hsl_to_rgb_map_interleave (alwan_scalar *out, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
```

### 7.9 Math

```c
int alwan_mat3_transform_map_interleave (alwan_scalar *out, const alwan_mat3x3 *matrix, const alwan_scalar *in, size_t count, size_t in_stride, size_t out_stride);
```

### 7.10 Stateful RGB Convert

```c
int alwan_rgb_convert_map_interleave (alwan_rgb *dst, alwan_ctx *ctx, const alwan_rgb_space_desc *src_space, const alwan_rgb_space_desc *dst_space, const alwan_rgb *src, size_t count);
```

---

## 8. Typed Interleaved Variants (`_map_interleave_ex`)

The `_ex` variants accept `void*` buffers with explicit `alwan_pixel_format` for both input and output. This allows direct processing of U8, U16, F32, or F64 pixel data without manual conversion.

### Pixel Format Enum

```c
typedef enum {
    ALWAN_PIXEL_U8  = 0,  /* uint8_t  [0,255]   -> [0.0, 1.0] */
    ALWAN_PIXEL_U16 = 1,  /* uint16_t [0,65535]  -> [0.0, 1.0] */
    ALWAN_PIXEL_F32 = 2,  /* float                              */
    ALWAN_PIXEL_F64 = 3   /* double                             */
} alwan_pixel_format;

/* Convenience: matches the configured alwan_scalar type */
#define ALWAN_PIXEL_SCALAR  /* ALWAN_PIXEL_F32 or ALWAN_PIXEL_F64 */
```

### Signature Pattern

```c
int alwan_srgb_to_xyz_map_interleave_ex(
    void *out,                  // Output buffer (any pixel type)
    alwan_pixel_format out_fmt, // Output format
    void const *in,             // Input buffer (any pixel type)
    alwan_pixel_format in_fmt,  // Input format
    size_t count,               // Number of pixels
    size_t in_stride,           // Input stride in bytes
    size_t out_stride           // Output stride in bytes
);
```

Input and output formats can differ — the function handles conversion internally.

### Example

```c
// Convert 8-bit sRGB image to float32 XYZ
uint8_t srgb_u8[1024 * 3];
float   xyz_f32[1024 * 3];

alwan_srgb_to_xyz_map_interleave_ex(
    xyz_f32, ALWAN_PIXEL_F32,
    srgb_u8, ALWAN_PIXEL_U8,
    1024,
    3 * sizeof(uint8_t),   // in_stride
    3 * sizeof(float)      // out_stride
);
```

### Available `_ex` Functions

Every `_map_interleave` function listed in section 7 has a corresponding `_ex` variant. Additional `_ex`-only functions include color spaces not available in the base `_map_interleave` API:

- IgPgTg, ICAcb, HDR-CIELAB, HDR-IPT
- UCS (1960/1964/1976), OSA-UCS, Hunter Lab
- ProLab, CIE 1964 U\*V\*W\*, Prismatic, HCL, IHLS
- DIN99 variants, YCbCr, YcCbcCrc, CMY, YCoCg, HWB

---

## 9. Planar Variants (`_map_planar` / `_map_planar_ex`)

Planar functions process pixels stored as separate per-channel arrays (Structure-of-Arrays layout). Each channel is a separate pointer with per-sample stride.

### `_map_planar` Signature Pattern

```c
int alwan_srgb_to_xyz_map_planar(
    alwan_scalar *out_ch0,          // Output channel 0 (X)
    alwan_scalar *out_ch1,          // Output channel 1 (Y)
    alwan_scalar *out_ch2,          // Output channel 2 (Z)
    alwan_scalar const *in_ch0,     // Input channel 0 (R)
    alwan_scalar const *in_ch1,     // Input channel 1 (G)
    alwan_scalar const *in_ch2,     // Input channel 2 (B)
    size_t count,                   // Number of pixels
    size_t in_stride,               // Input stride per channel (bytes)
    size_t out_stride               // Output stride per channel (bytes)
);
```

### `_map_planar_ex` Signature Pattern

```c
int alwan_srgb_to_xyz_map_planar_ex(
    void *out0, void *out1, void *out2,     // Output channels (any pixel type)
    alwan_pixel_format out_fmt,              // Output format
    void const *in0, void const *in1, void const *in2,  // Input channels
    alwan_pixel_format in_fmt,               // Input format
    size_t count,
    size_t in_stride,
    size_t out_stride
);
```

### Example

```c
// Planar float64 Lab to XYZ
alwan_scalar L[4096], a[4096], b[4096];
alwan_scalar X[4096], Y[4096], Z[4096];
alwan_xyz d65 = {0.95047, 1.0, 1.08883};

alwan_lab_to_xyz_map_planar(X, Y, Z, L, a, b, &d65,
                            4096, sizeof(alwan_scalar), sizeof(alwan_scalar));

// Planar _ex: 16-bit input, float32 output
uint16_t r16[1024], g16[1024], b16[1024];
float xf[1024], yf[1024], zf[1024];

alwan_srgb_to_xyz_map_planar_ex(
    xf, yf, zf, ALWAN_PIXEL_F32,
    r16, g16, b16, ALWAN_PIXEL_U16,
    1024, sizeof(uint16_t), sizeof(float)
);
```

### Available Planar Functions

Planar variants mirror the interleaved functions. Available for:
- **sRGB convenience**: sRGB/XYZ, sRGB/Lab, sRGB/Oklab
- **Colorspace**: XYZ/Lab, XYZ/Luv, Lab/LCh, Luv/LChuv, XYZ/xyY
- **Oklab**: XYZ/Oklab, Oklab/Oklch
- **ICtCp**: RGB/ICtCp, XYZ/ICtCp
- **Jzazbz**: XYZ/Jzazbz, Jzazbz/JzCzhz
- **IPT**: XYZ/IPT
- **CAM models**: CIECAM02, CAM16 (forward/inverse)
- **Convenience**: RGB/HSV, RGB/HSL
- **Math**: mat3 transform

All planar functions also have `_ex` variants for typed buffers.

---

## 10. Image Descriptor (Future Layer)

A higher-level `alwan_image_desc` + `alwan_image_process()` layer can be added on top of map functions to handle pixel type conversion and layout transposition. See `lib_mem.md` for target library layouts.

---

## 11. Unit Tests

### 11.1 Strategy: Subsampled LUT Comparison

Each test generates an "artificial image" by subsampling the input color space as a flattened 3D LUT. The map function output is compared against a reference loop that calls the scalar `_v` core function per pixel.

### 11.2 Subsampling Parameters

| Pixel Type | Range | Step (K) | Samples/Channel | Total Pixels | Notes |
|------------|-------|----------|-----------------|--------------|-------|
| uint8 | 0-255 | 17 | 16 | 4,096 | Covers 0, 17, 34, ..., 255 |
| uint16 | 0-65535 | 1024 | 64 | 262,144 | For 16-bit; adjust K for 10/12/14-bit |
| float32 | 0.0-1.0 | 0.05 | 21 | 9,261 | Includes 0.0 and 1.0 exactly |
| float64 | 0.0-1.0 | 0.05 | 21 | 9,261 | Same grid, double precision |

For color spaces with ranges beyond [0,1] (e.g., Lab L in [0,100], a/b in [-128,127]; HDR linear RGB up to 10.0), adjust the range and step accordingly per test.

### 11.3 Test Structure

```c
// tests/NN_srgb_to_xyz_map_interleave.c

#include "alwan.h"
#include "core/alwan_core.h"
#include <math.h>

#define K_F32 0.05f
#define TOL   1e-6f

int test_NN_srgb_to_xyz_map_main(void)
{
    int fail = 0;

    /* --- Build subsampled LUT as flat pixel array --- */
    size_t count = 0;
    for (float r = 0.0f; r <= 1.0f; r += K_F32)
        for (float g = 0.0f; g <= 1.0f; g += K_F32)
            for (float b = 0.0f; b <= 1.0f; b += K_F32)
                count++;

    alwan_scalar *rgb_in  = malloc(count * 3 * sizeof(alwan_scalar));
    alwan_scalar *xyz_map_interleave = malloc(count * 3 * sizeof(alwan_scalar));
    alwan_scalar *xyz_ref = malloc(count * 3 * sizeof(alwan_scalar));

    /* Fill input */
    size_t idx = 0;
    for (float r = 0.0f; r <= 1.0f; r += K_F32)
        for (float g = 0.0f; g <= 1.0f; g += K_F32)
            for (float b = 0.0f; b <= 1.0f; b += K_F32) {
                rgb_in[idx * 3 + 0] = (alwan_scalar)r;
                rgb_in[idx * 3 + 1] = (alwan_scalar)g;
                rgb_in[idx * 3 + 2] = (alwan_scalar)b;
                idx++;
            }

    /* --- Map function under test --- */
    size_t stride = 3 * sizeof(alwan_scalar);
    alwan_srgb_to_xyz_map_interleave(xyz_map_interleave, rgb_in, count, stride, stride);

    /* --- Reference: scalar _v loop --- */
    for (size_t i = 0; i < count; i++) {
        alwan_rgb rgb = { rgb_in[i*3+0], rgb_in[i*3+1], rgb_in[i*3+2] };
        alwan_xyz xyz = alwan_srgb_to_xyz_v(rgb);
        xyz_ref[i*3+0] = xyz.x;
        xyz_ref[i*3+1] = xyz.y;
        xyz_ref[i*3+2] = xyz.z;
    }

    /* --- Compare --- */
    for (size_t i = 0; i < count * 3; i++) {
        double diff = fabs((double)xyz_map_interleave[i] - (double)xyz_ref[i]);
        if (diff > TOL) {
            fprintf(stderr, "FAIL at pixel %zu component %zu: map=%g ref=%g diff=%g\n",
                    i / 3, i % 3, (double)xyz_map_interleave[i], (double)xyz_ref[i], diff);
            fail = 1;
        }
    }

    free(rgb_in); free(xyz_map_interleave); free(xyz_ref);
    return fail ? 1 : 0;
}
```

### 11.4 Test Matrix

One test file per map function. Each test:
1. Generates the subsampled LUT for the appropriate input range
2. Calls the `_map_interleave` function on the entire buffer
3. Calls the `_v` core function per pixel as reference
4. Compares all outputs within tolerance

Tolerance depends on whether `alwan_scalar` is float or double, and whether the SIMD path uses float32 internally (which introduces rounding vs the double `_v` path). Expected tolerances:

| Configuration | Tolerance |
|---------------|-----------|
| f32 variants, SIMD f32 | `1e-6` (round-trip through same type) |
| f64 variants, SIMD f32 | `1e-5` (f32 SIMD vs f64 scalar) |
| f64 variants, SIMD f64 | `1e-12` (same precision) |

### 11.5 Test File Naming

Following existing convention (`NN_description.c`):

```
tests/60_srgb_to_xyz_map_interleave.c
tests/61_xyz_to_lab_map_interleave.c
tests/62_xyz_to_oklab_map_interleave.c
tests/63_rgb_to_hsv_map_interleave.c
tests/64_xyz_to_jzazbz_map_interleave.c
tests/65_xyz_to_ictcp_map_interleave.c
tests/66_xyz_to_ipt_map_interleave.c
tests/67_mat3_transform_map_interleave.c
...
```

Each exports `test_NN_description_main(void)` and is registered in `test_runner.c` + `tests/CMakeLists.txt`.

---

## 12. Build System Integration

Sharpmake is the reference build system. CMake mirrors it. Both auto-discover `map/` sources via their glob patterns (`SourceRootPath` in Sharpmake, explicit listing in CMake).

### 12.1 Sharpmake

```csharp
// In the Alwan project .sharpmake.cs

// SIMD level — set per platform/configuration
// Sharpmake auto-discovers map/*.c via SourceRootPath
conf.Options.Add(Options.Vc.Compiler.EnhancedInstructionSet.AdvancedVectorExtensions2); // /arch:AVX2
// or
conf.Options.Add(Options.Vc.Compiler.EnhancedInstructionSet.AdvancedVectorExtensions);  // /arch:AVX
// or default (SSE2 implied on x64)
```

MSVC only supports `/arch:SSE2`, `/arch:AVX`, `/arch:AVX2`. No fine-grained SSE3/SSE4.1 flag — intermediate SSE upgrades within `alwan_simd_sse2.h` are driven by what the compiler defines when AVX is enabled (AVX implies SSE4.2).

### 12.2 CMakeLists.txt

Mirrors Sharpmake:

```cmake
option(ALWAN_ENABLE_AVX  "Enable AVX SIMD (256-bit float, 128-bit int, no FMA)" OFF)
option(ALWAN_ENABLE_AVX2 "Enable AVX2 SIMD (256-bit float+int, FMA)" OFF)

if(ALWAN_ENABLE_AVX2)
    if(MSVC)
        set(ALWAN_SIMD_FLAGS "/arch:AVX2")
    else()
        set(ALWAN_SIMD_FLAGS "-mavx2 -mfma")
    endif()
elseif(ALWAN_ENABLE_AVX)
    if(MSVC)
        set(ALWAN_SIMD_FLAGS "/arch:AVX")
    else()
        set(ALWAN_SIMD_FLAGS "-mavx")
    endif()
else()
    # SSE2 is implicit on x64 for all compilers
    # GCC/Clang: optionally pass -msse4.1 for sub-AVX upgrades
    set(ALWAN_SIMD_FLAGS "")
endif()

set(ALWAN_MAP_SOURCES
    src/alwan/map/alwan_rgb_map_interleave.c
    src/alwan/map/alwan_colorspace_map_interleave.c
    src/alwan/map/alwan_oklab_map_interleave.c
    src/alwan/map/alwan_convenience_map_interleave.c
    src/alwan/map/alwan_cam_map_interleave.c
    src/alwan/map/alwan_ictcp_map_interleave.c
    src/alwan/map/alwan_ipt_map_interleave.c
    src/alwan/map/alwan_jzazbz_map_interleave.c
    src/alwan/map/alwan_math_map_interleave.c
)

set_source_files_properties(${ALWAN_MAP_SOURCES}
    PROPERTIES COMPILE_FLAGS "${ALWAN_SIMD_FLAGS}")
```

