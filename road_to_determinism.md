# Road to Determinism

Engineering plan for cross-platform bit-exact reproducibility, ULP-budget
testing, and SIMD coverage on aarch64.

> Status as of 2026-05-03:
>   - **W1 (ULP helper)** — done. `alwan_ulps_f64/_f32` plus six macros in
>     test_common.h; self-tested by 85_ulp_helpers.
>   - **W2 (alwan_math.h math layer + libm sweep)** — done. 409 raw libm
>     calls routed through `ALWAN_*` macros. Lint enforces no regressions.
>   - **W3 Phase 1 + 1.5 (sRGB deterministic)** — done. Polynomials in
>     normalized basis; f32 path tracks f64 within FLT_EPSILON.
>   - **W3 Phase 2 (BT.2020 / BT.709 deterministic)** — done. Generic
>     `LinearPlusPowerTF` helper in gen_tf_polynomials.py.
>   - **W4 (SIMD reduction-order determinism)** — done. `*_native` rename +
>     dispatcher + canonical scalar fallback in det mode.
>   - **W6 (cross-platform bit-exact CI)** — done. `det_run_regression`
>     tool + matrix workflow comparing 6 platforms.
>   - Deferred (next): **Phase 2b** (BT.1886, gamma 2.2/2.4/2.6/2.8 —
>     pure-power TFs, need argument reduction); **Phase 3+** (PQ, HLG);
>     **Phase 5** (Lab/Oklab cube root, unbounded XYZ domain); **W5**
>     (NEON backend); **W1 followup** (mass-migrate tests to ULP budgets).

---

## 1. Goals and non-goals

### Goals

- **Opt-in bit-exact mode** (`ALWAN_DETERMINISTIC=1`) where the lib
  produces byte-identical output across:
  - x86_64 (with and without hardware FMA)
  - aarch64 (Apple Silicon, Linux ARM)
  - Compilers: MSVC 19+, gcc 11+, clang 14+
  - Optimization levels: `-O0` through `-O3`
- **ULP-budget testing**: replace fragile absolute thresholds
  (`|a - b| < 1e-15`) with documented ULP budgets per primitive
  (`ulps(a, b) <= 8`). Self-documenting and platform-aware.
- **First-class aarch64**: native NEON backend, ARM CI green, no scalar
  fallback for SIMD-able kernels.
- **Substantial test coverage** that exercises the determinism contract
  (cross-platform output hash matching) and survives compiler choice.

### Non-goals

- **Fast mode determinism.** When `ALWAN_DETERMINISTIC=0` (default), the
  lib uses libm and lets the compiler contract FMA. Different platforms
  may produce different last-bit results; this is fine.
- **Reproducibility under `-ffast-math` / `/fp:fast`.** The fast-math
  flags re-enable associativity rewrites, NaN/Inf shortcuts, and FTZ
  changes that break IEEE semantics. We don't try to recover those.
- **Bit-exactness vs other colour libraries** (colour-science, OCIO).
  Our deterministic polynomials are calibrated to match alwan's previous
  fast-mode output to within a documented ULP budget; they are not
  required to match a third-party reference at the bit level.
- **f128 / long double support.** Stays f32/f64 only.

---

## 2. Where bit-exactness leaks today

A reference, not a problem statement — these are the seams the rest of
the doc addresses.

| Source | Effect | Affected by |
|---|---|---|
| **libm `pow/exp/log/...`** | 1–3 ULP variance per call | Apple libm vs glibc vs musl vs MSVCRT |
| **FMA contraction** | 0.5 ULP per fused op, compounds | gcc/clang `-mfma`, mandatory aarch64 FMA, MSVC default-off |
| **`-ffp-contract`** | Same as above; default differs by compiler | gcc default `fast`, clang `on`, MSVC effectively `off` |
| **SIMD reduction order** | Different lane widths → different accumulation tree | SSE2 (2/4) vs AVX (4/8) vs NEON (2/4) |
| **Denormal flushing** | values < ~1e-38 become 0 | macOS/iOS aarch64 sets FZ=1 by default |
| **Excess intermediate precision** | x86 80-bit FPU registers | gcc `-fexcess-precision=fast` on 32-bit x86 (rare) |
| **Compiler reorderings** | `(a + b) + c` vs `a + (b + c)` | All compilers under `-O2`+ |

The deterministic mode locks each of these down. Fast mode leaves them
to the compiler.

---

## 3. Architecture overview

Six pieces, glued together by `ALWAN_DETERMINISTIC`:

```
┌─────────────────────────────────────────────────────────────┐
│  alwan_math.h                                                │
│  Routes ALWAN_POW / ALWAN_EXP / ALWAN_FMA / ...              │
│  to libm  (fast mode)  or  alwan_deterministic.h  (det mode) │
└─────────────────────────────────────────────────────────────┘
              │                              │
              ▼                              ▼
   ┌─────────────────┐         ┌─────────────────────────┐
   │  <math.h>       │         │  alwan_deterministic.h  │
   │  (fast mode)    │         │  Polynomial impls,      │
   │                 │         │  no libm, no FMA,       │
   │                 │         │  FP_CONTRACT OFF        │
   └─────────────────┘         └─────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  alwan_simd_*.h  (per-backend)                               │
│  Element-wise: native NEON / SSE / AVX                       │
│  Reductions in det mode: scalar fallback (canonical order)   │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  test_common.h  (alwan_dev/tests)                            │
│  TEST_ASSERT_CLOSE_ULP_F32 / _F64                            │
│  ULP budgets documented per test                             │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  CI: cross-platform-determinism.yml                          │
│  Build with ALWAN_DETERMINISTIC=1 on Linux x64,              │
│  Linux ARM, macOS Intel, macOS ARM, Windows x64.             │
│  Run a fixed-input regression tool, compare output           │
│  hashes. PASS only if all hashes match.                      │
└─────────────────────────────────────────────────────────────┘
```

The two big public surface changes:

1. **`alwan_math.h`** becomes a real header (today most of its content
   lives in `alwan_platform.h` as `ALWAN_POW`/etc. macros). Every libm
   call in `src/alwan/` routes through it.
2. **`ALWAN_DETERMINISTIC`** is a public CMake option and `#define`. End
   users opt in via `-DALWAN_DETERMINISTIC=ON` at configure time.

Everything else is internal.

---

## 4. Workstream 1 — ULP helper

### 4.1 Design

Bruce Dawson's integer-cast technique. Two `double`s differ by N ULPs iff
their bit representations (after canonicalizing the sign bit) differ by N
when read as `int64_t`.

```c
// In alwan_dev/tests/test_common.h
#include <stdint.h>
#include <string.h>

static inline uint64_t alwan_ulps_f64(double a, double b) {
    if (a == b) return 0;                      // covers +0 vs -0
    if (isnan(a) || isnan(b)) return UINT64_MAX;
    int64_t ia, ib;
    memcpy(&ia, &a, sizeof(ia));
    memcpy(&ib, &b, sizeof(ib));
    // Canonicalise: map negatives to "below zero" in a monotone way.
    if (ia < 0) ia = (int64_t)0x8000000000000000LL - ia;
    if (ib < 0) ib = (int64_t)0x8000000000000000LL - ib;
    return (uint64_t)((ia > ib) ? (ia - ib) : (ib - ia));
}

static inline uint32_t alwan_ulps_f32(float a, float b) {
    if (a == b) return 0;
    if (isnan(a) || isnan(b)) return UINT32_MAX;
    int32_t ia, ib;
    memcpy(&ia, &a, sizeof(ia));
    memcpy(&ib, &b, sizeof(ib));
    if (ia < 0) ia = 0x80000000 - ia;
    if (ib < 0) ib = 0x80000000 - ib;
    return (uint32_t)((ia > ib) ? (ia - ib) : (ib - ia));
}
```

Cross-sign-zero is handled (`+0.0 == -0.0` returns 0). Cross-sign
non-zero returns the sum of distances to zero, which is conservative.
NaN returns max — a NaN is never "close" to anything.

### 4.2 Public API

```c
// Comparing scalars:
TEST_ASSERT_CLOSE_ULP_F64(actual, expected, max_ulps, msg)
TEST_ASSERT_CLOSE_ULP_F32(actual, expected, max_ulps, msg)

// Comparing buffers (point-wise):
TEST_ASSERT_BUF_CLOSE_ULP_F64(actual_ptr, expected_ptr, count, max_ulps, msg)
TEST_ASSERT_BUF_CLOSE_ULP_F32(actual_ptr, expected_ptr, count, max_ulps, msg)

// Bit-exact assertion (used by determinism tests):
TEST_ASSERT_BITEXACT_F64(actual, expected, msg)
TEST_ASSERT_BITEXACT_BUF_F64(actual_ptr, expected_ptr, count, msg)
```

The buffer variants report the worst offender: file, line, index,
absolute values, ULP distance.

### 4.3 Migration plan

Sweep tests in this order (highest-impact first):

1. `04_srgb_tf.c` — sRGB roundtrip
2. `83_dual_precision.c` — f32/f64 agreement
3. `69_map_validation.c`, `70_planar_map.c`, `71_linear_srgb_hsv_hsl.c`
4. ACES tests (52, 54, 55, 56, 57)
5. The remainder, file by file.

**Per-test work:** find each `TEST_ASSERT_NEAR(a, b, eps)` whose `eps` is
calibrated against a specific platform. Replace with
`TEST_ASSERT_CLOSE_ULP_F64(a, b, N, ...)` where `N` is derived from:

```
N = 2 * sqrt(operations_in_chain) + 4
```

(rule of thumb from Higham, *Accuracy and Stability of Numerical
Algorithms*; the 4 is slack for libm itself).

Add a comment above each ULP assertion documenting the budget rationale,
so future me knows whether widening it is OK.

**`TEST_ASSERT_NEAR` stays** for cases where absolute tolerance is the
right semantic — e.g., "is this hue angle approximately 120°".

### 4.4 Exit criteria

- Helpers exist, documented, unit-tested with known-distance pairs.
- 10+ tests migrated to ULP budgets.
- The macos-arm-tests workflow shows fewer failures because the new
  budgets accommodate Apple libm precision.

### 4.5 Effort

3–4 days. Half a day for the helpers, the rest is the sweep.

---

## 5. Workstream 2 — `alwan_math.h` math layer

### 5.1 Header API

```c
// alwan/src/alwan/alwan_math.h
//
// Single source of truth for math functions. Routes to libm (fast mode)
// or alwan_deterministic.h (det mode).

#pragma once
#include "alwan_config.h"   // defines ALWAN_DETERMINISTIC

#if ALWAN_DETERMINISTIC
#  include "core/alwan_deterministic.h"
#  define ALWAN_POW(x, y)    alwan_det_pow_f64((x), (y))
#  define ALWAN_POWF(x, y)   alwan_det_pow_f32((x), (y))
#  define ALWAN_EXP(x)       alwan_det_exp_f64((x))
#  define ALWAN_EXPF(x)      alwan_det_exp_f32((x))
#  define ALWAN_LOG(x)       alwan_det_log_f64((x))
#  define ALWAN_LOGF(x)      alwan_det_log_f32((x))
#  define ALWAN_LOG2(x)      alwan_det_log2_f64((x))
#  // ... cos/sin/tan/asin/acos/atan/atan2/sqrt/cbrt/...

#  define ALWAN_FMA(a, b, c)  ((a) * (b) + (c))   /* forced 2-rounding */
#  define ALWAN_FMAF(a, b, c) ((a) * (b) + (c))
#else
#  include <math.h>
#  define ALWAN_POW(x, y)    pow((x), (y))
#  define ALWAN_POWF(x, y)   powf((x), (y))
#  define ALWAN_EXP(x)       exp((x))
#  // ... etc.
#  define ALWAN_FMA(a, b, c)  fma((a), (b), (c))    /* may use HW FMA */
#  define ALWAN_FMAF(a, b, c) fmaf((a), (b), (c))
#endif
```

### 5.2 Naming convention

- Macros (`ALWAN_POW`) for the routed surface — preserves preprocessor
  flexibility, matches existing `alwan_platform.h` style.
- `alwan_det_*` for deterministic implementations — namespaces the slow
  path so it doesn't pollute the regular API.
- Both `f32` and `f64` variants. Avoid silent type promotion.

### 5.3 Sweep plan

Find every raw libm call in `src/alwan/`:

```bash
grep -rnE '\b(pow|powf|exp|expf|exp2|exp2f|log|logf|log2|log2f|log10|sin|sinf|cos|cosf|tan|tanf|asin|acos|atan|atan2|sqrt|sqrtf|cbrt|cbrtf|fabs|floor|ceil|round|trunc|fmod|fma|fmaf|sinh|cosh|tanh)\s*\(' src/alwan/
```

Triage each hit:
- **Replace** with `ALWAN_POW`-style macro if it's actual library math.
- **Leave alone** if it's already a macro reference (false positive).
- **Annotate** if it's a structured constant (e.g., `ALWAN_LITERAL(M_PI)`
  is fine, even though it transitively touches `<math.h>`).

Expected hits: 30–80 sites. Most should already be wrapped.

### 5.4 Forbidden raw calls (post-sweep)

Add a `tools/check_no_raw_libm.py` to alwan_dev/tools that runs the same
grep and fails CI if a non-macro libm call appears in `src/alwan/`. Wire
into the tooling.yml workflow.

### 5.5 Exit criteria

- `alwan_math.h` exists, public.
- Every `<math.h>` call in `src/alwan/` routes through `ALWAN_*` macros.
- `tools/check_no_raw_libm.py` runs clean.
- Fast mode behaviour is unchanged (verified by all existing tests passing).

### 5.6 Effort

1 day for header + sweep + the lint script.

---

## 6. Workstream 3 — `ALWAN_DETERMINISTIC` Phase 1 (sRGB)

### 6.1 What "deterministic" means in the contract

Bit-exact across:
- Compilers (MSVC 19+, gcc 11+, clang 14+)
- Optimization levels `-O0` through `-O3`
- Platforms (Linux x86_64, Linux aarch64, macOS x86_64, macOS aarch64,
  Windows x86_64, Windows aarch64)
- With or without hardware FMA
- With or without SIMD (scalar fallback bit-matches SIMD path)

What it does NOT promise:
- Same answer as libm `pow`. The polynomial is calibrated to be within
  a documented ULP budget of the previous fast-mode output, not equal.
- Anything under `-ffast-math` / `/fp:fast`.

### 6.2 sRGB OETF/EOTF deterministic implementation

Polynomial form:

```c
// alwan/src/alwan/core/alwan_deterministic.h
#pragma once
#pragma STDC FP_CONTRACT OFF
#include "alwan_types.h"

// sRGB OETF: linear -> encoded
// Reference (fast):
//   x <= 0.0031308:  12.92 * x
//   else:            1.055 * pow(x, 1/2.4) - 0.055
//
// Deterministic: replace pow(x, 1/2.4) with a degree-7 minimax
// polynomial over [0.0031308, 1.0]. Coefficients computed once
// in alwan_dev/gendata/gen_srgb_polynomial.py via Sollya/Remez.
//
// Max error vs libm: documented in tests/reference_values/srgb_oetf_ulp.csv.

ALWAN_INLINE alwan_f64 alwan_det_srgb_oetf_f64(alwan_f64 x) {
    if (x <= ALWAN_LITERAL(0.0031308)) {
        return ALWAN_LITERAL(12.92) * x;
    }
    // Horner evaluation, no FMA contraction (FP_CONTRACT OFF above).
    // Coefficients generated by gendata; see header table.
    alwan_f64 const c0 = /* ... */;
    alwan_f64 const c1 = /* ... */;
    // ...
    alwan_f64 y = c7;
    y = y * x + c6;
    y = y * x + c5;
    // ...
    return ALWAN_LITERAL(1.055) * y - ALWAN_LITERAL(0.055);
}

ALWAN_INLINE alwan_f32 alwan_det_srgb_oetf_f32(alwan_f32 x) { /* mirrored */ }
```

### 6.3 Polynomial generation

A new tool `alwan_dev/gendata/gen_srgb_polynomial.py`:

- Inputs: target function `pow(x, 1/2.4)` over `[0.0031308, 1.0]`.
- Tool: Sollya for the minimax fit; fallback to `numpy.polynomial.chebyshev`
  if Sollya isn't installed.
- Outputs: a header `src/alwan/core/alwan_det_srgb_coeffs.h` with the
  coefficients as `static const alwan_f64` arrays + a CSV reporting ULP
  error against libm at 1024 sample points across the domain.
- The python script is run by `generate_data.ps1`, like other gendata
  tools.

Degree choice: start with 7. If max ULP error vs libm > 16, bump to 9.
Beyond 9 is a smell — switch to a piecewise approximation.

### 6.4 Build flags

```cmake
option(ALWAN_DETERMINISTIC "Use bit-exact polynomial math (no libm, no FMA)" OFF)

if(ALWAN_DETERMINISTIC)
  target_compile_definitions(alwan PUBLIC ALWAN_DETERMINISTIC=1)
  if(MSVC)
    target_compile_options(alwan PRIVATE /fp:precise)
  else()
    target_compile_options(alwan PRIVATE -ffp-contract=off)
  endif()
endif()
```

`PUBLIC` on the define so consumer code (`alwan_dev/tests/`) sees the
same routing.

### 6.5 Tests

Three test files added in alwan_dev:

- `tests/det_01_srgb_oetf.c` — for 1024 inputs across the sRGB domain,
  assert `ulps(alwan_det_srgb_oetf, libm_reference) <= BUDGET`.
- `tests/det_02_srgb_bit_exact.c` — runs only when `ALWAN_DETERMINISTIC=1`.
  Hashes (xxhash3) the output buffer for a fixed 1024-pixel input.
  Compares against a checked-in reference hash. The CI matrix verifies
  the same hash on every platform.
- `tests/det_03_srgb_roundtrip.c` — `EOTF(OETF(x))` is bit-exact identity
  modulo the documented 1-ULP budget.

### 6.6 Exit criteria

- `ALWAN_DETERMINISTIC=ON` builds and passes existing tests on Linux x86_64.
- New `det_*` tests pass.
- Cross-platform CI matrix (workstream 7) confirms identical output hashes.
- Documented in user-facing README: "When you need reproducible output,
  build with `-DALWAN_DETERMINISTIC=ON`. Performance trade-off: ~20%
  slower on sRGB-heavy workloads."

### 6.7 Effort

3–5 days: 1 day polynomial generation + tooling, 1 day the kernel
itself, 1 day the test scaffolding, 1–2 days for the cross-platform
CI infra to come online.

### 6.8 Phase 2+ priorities

In order:
- ✅ BT.2020 / BT.709 — done (Phase 2). Same shape as sRGB; the
  refactor in `alwan_deterministic.h` introduced a generic
  `alwan__det_lin_pow_oetf_f{32,64}` helper that takes the TF
  parameters as args. New linear-plus-power TFs are now a one-line
  entry in `gen_tf_polynomials.py` (LinearPlusPowerTF) plus a thin
  inline wrapper.
- ⏳ BT.1886 / generic gamma 2.2/2.4/2.6/2.8 — same exponent as
  sRGB/BT.2020 but no linear segment, so the polynomial domain
  starts at 0 where `pow(x, exp<1)` has infinite slope. Needs
  IEEE-form argument reduction:
      x = m * 2^k via frexp; fit `pow(m, exp)` on [0.5, 1] only.
      `pow(2, k*exp)` = `2^int_part * pow(2, frac_part)` where the
      integer part is exact (ldexp) and the fractional part is one
      of `denom` distinct cases for `exp = num/denom`.
  Adds infrastructure (frexp/ldexp wrappers in alwan_math.h) but
  the polynomial fits become trivial.
- ⏳ Lab/Oklab cube root (`pow(x, 1/3)`) — used everywhere in CIE
  math. Domain is unbounded (XYZ can be > 1 for HDR), so single-
  segment polynomial doesn't apply. Same IEEE-form reduction
  approach as BT.1886 plus a Lab-specific linear-segment carve-out
  near t = (6/29)^3.
- ⏳ PQ (ST.2084) — rational polynomial in `pow(L, m1)` then
  `pow(num/den, m2)`. Both exponents are fractional; needs the
  IEEE reduction infrastructure from BT.1886.
- ⏳ HLG (Hybrid Log-Gamma) — sqrt + log + linear segments. Sqrt
  is already deterministic via `ALWAN_SQRT` (single rounding); log
  needs argument reduction. Roughly the same level of complexity
  as PQ.
- ⏳ ACES segmented splines — already piecewise polynomial, just
  needs `FP_CONTRACT OFF`. Should be the easiest of the remaining
  phases once we have the routing pattern.
- The long tail (atd95, llab, kim2009, ...) — same kind of
  per-TF fits, mostly mechanical once the patterns are in place.

### 6.9 Lessons learned during phases 1–2

- **numpy.polynomial domain/window machinery is misleading.** Calling
  `Chebyshev.fit(x, y, deg, domain=[lo, hi])` then `.convert(kind=
  Polynomial)` keeps the ORIGINAL x range alive in `.coef`, so the
  emitted coefficients reach 1e17 when the domain is small. The fix
  is to pre-normalize x to [-1, 1] before fitting; see
  `chebyshev_minimax_fit` in gen_tf_polynomials.py.
- **f32 needs O(1) coefficients.** The original power-basis fit had
  coefficients up to 3.7e17 — fine in f64 but completely lost in
  f32 (24-bit mantissa). After normalization the coefficients drop
  to O(0.2–0.8) and the f32 path agrees with f64 to 1.8e-7.
- **SIMD parity matters.** SIMD's `pow24`/`pow_inv24` use lane-
  order- and FMA-dependent approximations that diverge from the
  scalar polynomial by ~5e-5. In det mode we pay ~10× perf to
  unpack lanes and route through the scalar polynomial; without
  this, scalar-vs-SIMD parity tests fail.
- **Existing tolerance constants must be precision-aware.** Tests
  pinning `1e-12` (libm noise floor) fail in det mode because the
  polynomial has 5e-5 absolute error vs libm — and that's a *feature*,
  not a bug, since 5e-5 is within the documented contract.
  ALWAN_TEST_TOLERANCE / TEST_REL_EPSILON / ALWAN_SIMD_TOLERANCE all
  flip wider in det mode.
- **The cleanest refactor pattern for new TFs.** Once the generic
  helper landed, adding BT.2020 was: one entry in the Python TF
  table, plus four three-line wrappers in `alwan_deterministic.h`,
  plus four macro definitions in setup files. ~1 hour of work
  per linear-plus-power TF after the infrastructure exists.

---

## 7. Workstream 4 — SIMD reduction-order determinism

### 7.1 The problem

A horizontal sum (`sum(v[0..N])`) is computed differently per width:

```
SSE2 (4-lane f32):   ((v0+v1) + (v2+v3))
AVX  (8-lane f32):   (((v0+v1)+(v2+v3)) + ((v4+v5)+(v6+v7)))
NEON (4-lane f32):   vaddvq_f32 — implementation-defined order, not specified by ARM ARM
Scalar:              (((v0+v1)+v2)+v3)+...
```

These give different rounding cascades. Difference is usually 1–2 ULPs
but compounds in long pipelines.

### 7.2 Solution

In `ALWAN_DETERMINISTIC=1` mode, every horizontal reduction (sum, dot
product, max, min over a vector) uses a **canonical scalar reduction**
left-to-right:

```c
#if ALWAN_DETERMINISTIC
ALWAN_INLINE alwan_f64 alwan__hsum_f64(alwan_simd v) {
    alwan_f64 lanes[ALWAN_SIMD_WIDTH_F64];
    alwan_simd_store(lanes, v);
    alwan_f64 acc = lanes[0];
    for (int i = 1; i < ALWAN_SIMD_WIDTH_F64; i++) acc = acc + lanes[i];
    return acc;
}
#else
ALWAN_INLINE alwan_f64 alwan__hsum_f64(alwan_simd v) {
    return /* native horizontal-add intrinsic */;
}
#endif
```

Element-wise SIMD ops (per-lane add/mul/sub/div, polynomial evaluation)
stay vectorized — every lane runs the same operation, so width doesn't
affect bit-exactness.

For Kahan/Neumaier compensated summation (better accuracy, also
deterministic): consider for the long-array reductions in `alwan_spd.c`,
where naive summation can lose precision after ~10⁴ samples. Optional
Phase 2.

### 7.3 Inventory

A grep for horizontal reductions in the lib:
- `alwan__hsum_*` in alwan_simd_types.h (if present)
- `_mm_hadd_*`, `_mm_reduce_*` (AVX-512 only), `vaddvq_*`
- Any explicit `for (i=0; i<W; i++) acc += v[i]` SIMD-tile reductions

Audit each, route through `alwan__hsum_*` macros that branch on
`ALWAN_DETERMINISTIC`.

### 7.4 Effort

1–2 days, mostly audit. The deterministic scalar fallback is one
function per type.

---

## 8. Workstream 5 — NEON SIMD backend

### 8.1 Approach

Native NEON, not simde. Reasons documented separately, summary:

- alwan already has a SIMD abstraction (`alwan_simd`, `alwan_simd_lane`
  in `alwan_simd_types.h`) — adding a NEON backend means filling in the
  abstraction's missing implementation.
- simde maps SSE→NEON 1:1 but loses NEON-native primitives
  (`vaddvq_f32` for horizontal sums, `vfmaq_f32` for FMA, etc.).
- AVX→NEON via simde splits into two 128-bit halves, throwing away
  width.
- Hand-written NEON kernels are tighter on icache.
- Future RISC-V / SVE backends slot into the same pattern.

### 8.2 Inventory

Audit `src/alwan/simd/alwan_simd_types.h` and any `_simd_helpers.inc`
files. Make a list of every abstract op:

```
load, store, broadcast, set_zero
add, sub, mul, div, fma, neg, abs, min, max
cmp_eq, cmp_lt, cmp_le, cmp_gt, cmp_ge
blend, select, mask
shift_left, shift_right (int variants)
gather, scatter
hsum, hmax, hmin, dot
sqrt, rsqrt
to_int, to_float, cast
```

Approximate count: 30–50 ops.

### 8.3 Implementation

New files:
- `src/alwan/simd/alwan_simd_neon.h` — type aliases for `float32x4_t`,
  `float64x2_t`, `int32x4_t`, etc.
- `src/alwan/simd/alwan_simd_neon.inc` — implementations of every
  abstract op.

Gating in `alwan_simd_types.h`:

```c
#if defined(__aarch64__) && !defined(ALWAN_DISABLE_SIMD)
#  include "simd/alwan_simd_neon.h"
#  define ALWAN_SIMD_BACKEND "neon"
#  define ALWAN_SIMD_WIDTH_F32 4
#  define ALWAN_SIMD_WIDTH_F64 2
#elif defined(__AVX2__)
   /* existing AVX2 path */
#elif defined(__SSE2__)
   /* existing SSE2 path */
#else
#  define ALWAN_SIMD_BACKEND "scalar"
#  define ALWAN_SIMD_WIDTH_F32 1
#  define ALWAN_SIMD_WIDTH_F64 1
#endif
```

### 8.4 Tests

The existing SIMD-vs-scalar parity tests (e.g., the `_v vs _f64_map_planar`
checks in 70_planar_map.c) exercise the new backend automatically once
NEON is enabled. With ULP budgets in place (workstream 1), small
NEON-vs-SSE differences are tolerated.

A new property test: for every kernel, run both scalar and SIMD over the
same input on the same machine. They should agree to within a per-kernel
ULP budget.

### 8.5 Performance

Expected throughput on Apple Silicon (M2): roughly 2–4× scalar for
element-wise paths (sRGB OETF, matrix transforms). Less for
reduction-heavy code (CCT search) until we vectorize those.

### 8.6 Exit criteria

- `linux-arm-tests` and `macos-arm-tests` workflows turn green for
  build (already) and testing (after workstream 1's ULP migration).
- Bench output on aarch64 shows non-trivial SIMD speedup vs current
  scalar fallback.
- No regressions on x86_64.

### 8.7 Effort

1–2 weeks. Op-by-op NEON implementations are mechanical but volume.

### 8.8 Fallback for unsupported architectures

Add simde as a second-tier backend, behind native NEON:

```c
#if defined(__aarch64__)
#  include "simd/alwan_simd_neon.h"
#elif defined(__AVX2__)
#  include "simd/alwan_simd_avx2.h"
#elif defined(__SSE2__)
#  include "simd/alwan_simd_sse2.h"
#elif __has_include("simde/x86/sse2.h")
#  include "simd/alwan_simd_simde.h"  /* RISC-V, POWER, etc. */
#else
#  include "simd/alwan_simd_scalar.h"
#endif
```

Optional, lands as workstream 5b after the main NEON work.

---

## 9. Workstream 6 — CI: cross-platform bit-exact verification

### 9.1 What we need to prove

For `ALWAN_DETERMINISTIC=1`, the following must be true:
- `det_run_regression` (a new tool) emits an output buffer.
- The xxhash3 of that buffer is identical on every supported platform.

### 9.2 Tool

`alwan_dev/tools/det_run_regression.c` (a new C program):
- Takes a fixed 1024-pixel test input (committed CSV).
- Runs every deterministic primitive over it.
- Concatenates outputs in a documented order.
- Prints xxhash3 of the concatenation.

### 9.3 Workflow

`alwan_dev/.github/workflows/determinism.yml`:

```yaml
name: Determinism
on: [push, pull_request]
jobs:
  matrix:
    strategy:
      matrix:
        os: [ubuntu-latest, ubuntu-24.04-arm, macos-13,
             macos-latest, windows-latest, windows-11-arm]
    runs-on: ${{ matrix.os }}
    steps:
      - checkout alwan_dev + alwan
      - cmake -B build -DALWAN_DETERMINISTIC=ON
      - cmake --build build
      - hash=$(./build/tools/det_run_regression)
      - echo "$hash" > hash-$RUNNER_OS-$RUNNER_ARCH.txt
      - upload-artifact: hash-*

  verify:
    needs: matrix
    runs-on: ubuntu-latest
    steps:
      - download all artifacts
      - assert all hash files contain the same string
      - if not, print the diff and fail
```

A separate "Determinism" badge in the README.

### 9.4 Effort

1 day for the tool, half a day for the workflow.

### 9.5 Maintenance

When adding a new deterministic primitive (e.g., extending to PQ in
Phase 2), `det_run_regression` is updated to call it, and the hash
changes. Reviewer's job to verify the new hash is correct on one
platform; CI verifies it then matches everywhere else.

---

## 10. Workstream 7 — Substantial unit tests

### 10.1 Property tests

Beyond pointwise comparisons, assert structural properties:

- `EOTF(OETF(x)) == x` to within budget for all x in domain.
- `OETF` is monotonically increasing on its domain.
- `OETF(0) == 0`, `OETF(1) == 1` (fixed points).
- For any TF: derivative continuity at the linear/non-linear seam.
- Symmetric primaries (R/G/B) produce equivalent results when permuted.
- f32 and f64 paths agree to f32-precision ULP budget.

### 10.2 Per-primitive ULP budget table

A CSV at `alwan_dev/tests/reference_values/ulp_budgets.csv`:

```
primitive,                     f64_ulps, f32_ulps, notes
alwan_srgb_oetf,               4,        8,        Phase 1 deterministic
alwan_srgb_eotf,               4,        8,        Phase 1 deterministic
alwan_bt2020_oetf,             6,        10,       Phase 2
alwan_pq_oetf,                 12,       24,       Phase 3, rational
alwan_aces2_tonescale,         32,       64,       Phase 4, multi-stage
...
```

Tests pull their budget from this table. Updating the table is a
deliberate decision visible in code review.

### 10.3 Determinism regression set

The `det_run_regression` tool grows over time as new deterministic
primitives land. The committed reference hash is the only "correct"
answer; any change to the deterministic implementation must update the
hash deliberately.

### 10.4 Effort

Ongoing. ~1 day per Phase to add the per-primitive property tests.

---

## 11. Order of execution

The workstreams have dependencies. Recommended order:

1. **W1 (ULP helper)** — 3–4 days. No dependencies. Immediately useful.
2. **W2 (alwan_math.h)** — 1 day. No dependencies. Carries the FMA fix.
3. **W4 (SIMD reduction det)** — 1–2 days. Depends on W2.
4. **W3 Phase 1 (sRGB det)** — 3–5 days. Depends on W2, W4.
5. **W6 (cross-platform CI)** — 1–2 days. Depends on W3 Phase 1.
6. **W5 (NEON backend)** — 1–2 weeks. Independent of W2/W3 but needs
   W1 in place to deal with ULP variance.
7. **W3 Phase 2+ (more deterministic primitives)** — incremental.
8. **W7 (substantial tests)** — ongoing alongside everything.

Total time-to-first-deterministic-build: ~2 weeks of focused work
(W1 → W2 → W4 → W3 Phase 1 → W6).

Total time to "all primitives deterministic, NEON full": ~2 months
of focused work, more realistic ~3–4 months interleaved with other
priorities.

---

## 12. Open questions

- **Where do the polynomial coefficients live?** Generated header
  (committed) or runtime-computed at startup? Header is faster, simpler,
  and reproducible across compilers; runtime would let us tune for the
  caller's accuracy needs but adds startup cost. Default: header.
- **Should `ALWAN_DETERMINISTIC` change ABI?** No. The deterministic
  primitives are inline in core headers; no change to the public C API
  or struct layouts. Different builds of the same alwan version are
  ABI-compatible at the symbol level but emit different numerical
  output.
- **Sollya availability in CI?** Sollya isn't always packaged. Make the
  polynomial generation a one-time offline step (committed coefficients)
  rather than a CI dependency. The python script in `gendata/` falls
  back to scipy if Sollya isn't found, with a documented quality
  difference.
- **Determinism for SIMD widths > 256-bit?** AVX-512 (16-lane f32) and
  SVE (variable). Element-wise SIMD is already deterministic regardless
  of width. Reductions force scalar in det mode (workstream 4), so
  width doesn't matter. No special action needed.
- **What about transcendentals we don't yet have polynomials for?**
  When `ALWAN_DETERMINISTIC=1` but a primitive hasn't been ported,
  routing through libm violates the contract. Two options: (a) compile
  error if a non-deterministic primitive is referenced, (b) document
  the gap and let the user opt-in to "best-effort determinism" with a
  separate flag. Lean toward (a) — strict mode catches gaps loudly.

---

## 13. Outside scope (explicitly)

These come up adjacent to determinism but are not part of this plan:

- **Reproducible RNG.** alwan's gamut Monte Carlo etc. uses a simple LCG;
  reproducibility there is already deterministic given a seed. No change.
- **Threading.** No alwan code uses threads; all map functions are
  single-threaded loops the caller can parallelize. If multi-threading is
  added later, careful with reductions (use thread-local accumulators
  reduced sequentially).
- **GPU backends** (CUDA/Metal/Vulkan compute). Out of scope. If added,
  they'd need their own determinism story.
- **Fast-math reproducibility.** Not promised. `-ffast-math` users are
  on their own.

---

## 14. References

- Bruce Dawson, *Comparing Floating Point Numbers, 2012 Edition*
  https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
- N. J. Higham, *Accuracy and Stability of Numerical Algorithms* (2002).
  Chapter 2: ULP/eps reasoning. Chapter 4: summation accuracy.
- William Kahan, *Lecture Notes on the Status of IEEE Standard 754*
- Sollya: https://www.sollya.org/ — minimax polynomial generator.
- simde: https://github.com/simd-everywhere/simde — fallback SIMD
  translator for unsupported architectures.
- ARM Architecture Reference Manual, Section A1.5.6 (FPCR / FZ flag).
