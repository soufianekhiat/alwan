# CPU / SIMD Backend

Alwan's default backend is the **C backend** (`ALWAN_BACKEND_C`). On the CPU, the
batch (`Map` / `_map` / planar / `_ex`) kernels are accelerated with hand-written
SIMD that is selected **entirely at compile time**: there is no runtime CPU
feature detection and no dynamic dispatch. The highest ISA the compiler is told to
target wins, and the whole library is built for that one ISA.

For the GPU shader backends (HLSL / GLSL / Halide) see [api/backends.md](api/backends.md).
For the deterministic build mode referenced throughout this page see
[determinism.md](determinism.md).

---

## Compile-time ISA selection

`simd/alwan_simd.h` picks exactly one backend header by preprocessor probe.
Highest ISA wins; first match in this order:

| Probe (preprocessor) | Backend header | ISA |
|----------------------|----------------|-----|
| `__AVX2__` | `alwan_simd_avx2.h` | AVX2 |
| `__AVX__` | `alwan_simd_avx.h` | AVX |
| `__SSE2__` \| `_M_X64` \| `_M_AMD64` | `alwan_simd_sse2.h` | SSE2 |
| `__aarch64__` \| `__ARM_NEON` | `alwan_simd_neon.h` | NEON |
| *(none of the above)* | `alwan_simd_scalar.h` | scalar |

After the backend header, `alwan_simd.h` includes `alwan_simd_reduce.h`, which
defines the public horizontal-reduction names (`alwan_simd_*_hsum` / `_hadd`) on
top of the backend's `*_native` reductions (see [Fast vs deterministic paths](#fast-vs-deterministic-paths)).

There is no `cpuid` / `__builtin_cpu_supports` check anywhere; selecting the ISA
is the compiler's job (via `/arch` or `-m` flags, below). A binary built for AVX2
requires an AVX2 CPU to run.

---

## Lane widths

Lane counts come from `simd/alwan_simd_types.h` as
`ALWAN_SIMD_{F32,F64,UINT8,UINT16}_WIDTH`. Widths only change at the
SSE2-to-AVX and AVX-to-AVX2 boundaries; the SSE3/SSSE3/SSE4.x families add
instructions rather than register width.

| ISA | F32 lanes | F64 lanes | UINT8 | UINT16 |
|-----|:---------:|:---------:|:-----:|:------:|
| AVX2 | 8 | 4 | 32 | 16 |
| AVX | 8 | 4 | 16^1 | 8^1 |
| SSE2 | 4 | 2 | 16 | 8 |
| NEON (aarch64) | 4 | 2 | 16 | 8 |
| NEON (ARMv7) | 4 | 1^2 | 16 | 8 |
| scalar | 1 | 1 | 1 | 1 |

^1 AVX has no 256-bit integer ops, so the integer lane vectors stay 128-bit.
^2 ARMv7 has no f64 SIMD: `alwan_simd_f64` is a plain `double` and f64 work runs
scalar (one lane). `aarch64` has native `float64x2_t`.

`ALWAN_ALIGN(n)` (same header) maps to `__declspec(align(n))` on MSVC and
`__attribute__((aligned(n)))` on GCC/Clang.

---

## Transcendentals and `ALWAN_HAS_SVML`

Element-wise SIMD transcendentals (`pow`, `cbrt`, `log2`, `exp2`, ...) have two
implementations, gated by `ALWAN_HAS_SVML` in `alwan_simd_types.h`:

```c
#if defined(__INTEL_COMPILER) || defined(__INTEL_LLVM_COMPILER)
#  define ALWAN_HAS_SVML 1
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
#  define ALWAN_HAS_SVML 1
#else
#  define ALWAN_HAS_SVML 0
#endif
```

| `ALWAN_HAS_SVML` | Compilers | SIMD transcendentals use |
|------------------|-----------|--------------------------|
| `1` | Intel C/C++, Intel LLVM, MSVC x64 | true vector intrinsics: `_mm_cbrt_ps`, `_mm_pow_ps`, `_mm256_*_pd`, ... (libm-accurate) |
| `0` | GCC/Clang on x86, **all NEON/ARM**, scalar | per-lane scalar / minimax-polynomial `_fast` impls |

When SVML is absent, the SIMD kernels fall back to polynomial/bit-trick `_fast`
impls in `simd/alwan_simd_scalar.h`, for example:

- `alwan_simd_f32_cbrt_fast` / `alwan_simd_f64_cbrt_fast`: integer bit-trick seed
  plus Newton-Raphson refinement (2 steps f32, 3 steps f64).
- `log2` / `exp2` Taylor polynomials (the f32 `exp2` is a degree-5 Taylor on
  `[0,1)`, coefficients `ln2^k/k!`) used to build `alwan_simd_f32_pow24`.
- f64 `alwan_simd_f64_pow24` / `_pow_inv24` route through `ALWAN_POW_F64` (libm)
  even on the fast path, since f64 has no polynomial twin.

### Fast-mode scalar <-> SIMD agreement (incl. NEON)

The scalar `_v` value API and the SIMD `Map` kernels must produce **matching**
results in fast mode. The risk: on a platform with no accurate vector `pow`
(no SVML, notably **NEON**), the SIMD path would use a polynomial while the
scalar path used libm `powf`, and the two would diverge.

`core/alwan_fast_pow.h` closes that gap. It defines the *scalar twins* of the
vector `pow(x, 2.4)` / `pow(x, 1/2.4)` kernels and selects them with the **same**
`ALWAN_HAS_SVML` switch:

```c
#if ALWAN_HAS_SVML
ALWAN_INLINE alwan_f32 alwan_fast_pow24_f32(alwan_f32 x)     { return powf(x, 2.4f); }
ALWAN_INLINE alwan_f32 alwan_fast_pow_inv24_f32(alwan_f32 x) { return powf(x, 1.0f / 2.4f); }
#else
ALWAN_INLINE alwan_f32 alwan_fast_pow24_f32(alwan_f32 x)     { return alwan__fast_pow_pos_f32(x, 2.4f); }
ALWAN_INLINE alwan_f32 alwan_fast_pow_inv24_f32(alwan_f32 x) { return alwan__fast_pow_pos_f32(x, 1.0f / 2.4f); }
#endif
```

The polynomial twin (`alwan__fast_pow_pos_*`) evaluates the **identical**
log2/exp2 decomposition (atanh-series `log2` on the mantissa plus a Taylor
`exp2`) with the same coefficients and the same operation order as the vector
kernel. Because the mantissa/exponent split is bit-identical to the intrinsic
path, there is no power-of-two boundary divergence between the scalar and SIMD
results. Net effect:

- **SVML present** (MSVC x64 / Intel): both scalar and SIMD call libm `pow*`.
- **No SVML** (GCC/Clang x86, **NEON**): both scalar and SIMD evaluate the same
  polynomial.

Either way the scalar `_v` path and the SIMD map path agree on every platform.
Accuracy floor of the polynomial twins vs libm is ~5e-10 absolute over `[0,1]`:
the documented "fast mode is approximate" trade-off. In deterministic mode this
file is unused; the canonical `alwan_det_*` polynomials are used instead.

---

## Choosing the ISA at build time

### CMake: `ALWAN_SIMD_ARCH`

The cache variable `ALWAN_SIMD_ARCH` (in the root `CMakeLists.txt`) maps a target
ISA to the right compiler flags. Empty (the default) leaves the compiler's own
default ISA in place.

```sh
cmake -S . -B build -DALWAN_SIMD_ARCH=AVX2
cmake --build build
```

Accepted values: `SSE2`, `AVX`, `AVX2`, `AVX512`, `native`. Flag mapping:

| `ALWAN_SIMD_ARCH` | MSVC | GCC / Clang |
|-------------------|------|-------------|
| `AVX2` | `/arch:AVX2` | `-mavx2 -mfma` |
| `AVX` | `/arch:AVX` | `-mavx` |
| `AVX512` | `/arch:AVX512` | `-mavx512f -mavx512bw` |
| `native` | *(no MSVC flag)* | `-march=native` |
| *(empty)* | compiler default | compiler default |

Notes:
- SSE2 needs no flag on 64-bit targets (it is baseline for x64, and `_M_X64`
  already selects the SSE2 backend on MSVC), so there is no SSE2 branch in the flag
  map.
- `native` has no MSVC equivalent: MSVC has no `-march=native`.
- These options are `PUBLIC`, so the chosen ISA propagates to consumers linking
  `Alwan::alwan`.

### Sharpmake (reference build)

Sharpmake is the reference build system; CMake replicates it. The Sharpmake
Release configuration **hardcodes** `/arch:AVX2`
(`buildsystem/sharpmake/src/common.cs`), so the reference Windows Release binary
selects the AVX2 backend (f32 x8, f64 x4). Debug builds use the compiler default.

---

## Fast vs deterministic paths

The SIMD layer has two operating modes. The default **fast path** maximises
throughput; **deterministic mode** (`ALWAN_DETERMINISTIC=1`, CMake
`-DALWAN_DETERMINISTIC=ON`) trades ~20% perf for bit-exact, cross-platform output.

| Aspect | Fast path (default) | Deterministic path |
|--------|---------------------|--------------------|
| Horizontal reductions | forward to backend `*_native` (`vaddvq_*`, SSE/AVX trees) | canonical left-to-right scalar `hsum`/`hadd` in `alwan_simd_reduce.h`, bit-exact across SSE/AVX/NEON/scalar |
| Transcendentals | SVML vector intrinsics, or libm, or `_fast` polynomials (per `ALWAN_HAS_SVML`) | canonical `alwan_det_*` polynomial TFs (no libm transcendentals) |
| SIMD width | full lane width (per ISA above) | `ALWAN_MAP_SIMD_WIDTH` collapses to 1; vector kernel bodies skipped |
| FMA contraction | allowed | disabled (`-ffp-contract=off` on GCC/Clang; `/fp:precise` on MSVC) |

The reason native horizontal reductions are not deterministic is lane order:
SSE2's 4-lane tree, AVX's 8-lane tree, and NEON's `vaddvq_*` sum in
implementation-defined orders that differ in the last ULP. **Element-wise** SIMD
ops (per-lane add/mul/sqrt/...) are bit-identical regardless of width, so only the
reductions and the transcendentals need the deterministic treatment. The full
det-mode contract (FMA, SVML, NEON, polynomial transfer functions) is documented
in [determinism.md](determinism.md).

---

## Related configuration

- **Precision**: the C backend builds both `_f32` and `_f64` variants by default;
  restrict with `ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64` (resolved to
  `ALWAN_WITH_F32` / `ALWAN_WITH_F64` in `alwan_build_config.h`). See
  [configuration.md](configuration.md) and [precision-and-limits.md](precision-and-limits.md).
- **Embedded data**: SIMD has no bearing on data loading; alwan is embedded-only
  (`ALWAN_EMBED_DATA=1`); runtime loading is not implemented. See
  [data-management.md](data-management.md).
- **GPU backends**: `ALWAN_BACKEND` C / HLSL / GLSL / Halide; the GPU shader
  backends are documented in [api/backends.md](api/backends.md).
