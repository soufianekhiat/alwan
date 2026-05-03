# Deterministic mode

When you build alwan with `-DALWAN_DETERMINISTIC=ON`, the library produces
**byte-identical numerical output** across compilers, optimization levels,
operating systems, and CPU architectures. Same input ⇒ same bytes — on
Windows MSVC x64, Linux gcc x64, Linux clang ARM, macOS Apple Silicon, all
of it.

Use it when you need:

- Reproducible regression baselines for visual tests.
- Bit-exact replay of colour pipelines for audit / forensics / golden files.
- Cross-platform CI where a hash diff actually means something changed.
- Frame-accurate match between an offline reference and a live renderer
  on a different machine.

Skip it when you need:

- Maximum throughput on a single platform.
- Last-bit agreement with `libm pow` / `libm log` (we ship a polynomial
  approximation, not a bit-perfect libm clone).

By default, `ALWAN_DETERMINISTIC=OFF` and the library uses libm + hardware
FMA + the full SIMD path — fast, but two different compilers may produce
results that differ in the last bit.

---

## What changes when you flip the switch

| Concern                         | Fast (default)                       | Deterministic                              |
| :------------------------------ | :----------------------------------- | :----------------------------------------- |
| `pow / exp / log / cbrt`        | libm                                 | Polynomial: argument reduction + Chebyshev |
| sRGB / BT.2020 / BT.709 OETF, EOTF | libm `pow`                        | Domain-split minimax polynomials           |
| FMA contraction (`a*b + c`)     | Compiler decides; hardware FMA on aarch64, often on x86 with `-mfma` | `-ffp-contract=off` / `/fp:precise`; never fused |
| SIMD horizontal sum             | Native pairwise (`vpaddq_pd`, `_mm_hadd_pd`, `vaddvq_f64`) | Canonical scalar left-to-right reduction |
| SIMD per-lane libm              | Lane-unpack to libm                  | Lane-unpack to deterministic polynomial    |
| sRGB / PQ / HLG / JzAzBz SIMD   | Vectorised approximation kernels (`pow24`, `pow_inv24`, `cbrt_fast`) | Lane-unpack to canonical scalar polynomial |
| IPT / Oklab SIMD                | Vectorised mat-mul + cbrt + mat-mul  | SIMD body bypassed; per-pixel scalar loop  |
| Output stability across runs    | Last-bit may vary between platforms  | Byte-identical; verified by CI matrix      |
| Throughput cost                 | —                                    | ~5–20% slower depending on workload        |

The throughput cost is dominated by the polynomial `pow` (vs. a 1-instruction
hardware FMA chain) and by the SIMD lane-unpack penalty for kernels that
fall back to scalar. Pure matrix transforms barely change.

---

## Where last-bit differences leak in (and how we close each)

### 1. `libm` precision

Each platform ships its own `libm`. On the same input, Apple's `pow`,
glibc's `pow`, and Microsoft's `pow` may return values that differ in
the last 1–3 ULPs. They're all "correctly rounded" within their own
specs, but the specs aren't byte-exact.

**Fix:** ship a polynomial implementation that does not call libm.
`alwan_det_log2 / exp2 / pow_pos / cbrt` use frexp/ldexp argument
reduction (which IS bit-deterministic on IEEE-754 platforms) plus a
Chebyshev minimax polynomial fitted to the normalised domain. Output
is bit-identical wherever IEEE-754 is honoured.

### 2. FMA contraction

`a * b + c` can compile to two operations (multiply, then add — two
roundings) or one fused-multiply-add (one rounding). The two-rounding
result and one-rounding result differ by up to 0.5 ULP. Worse, the
choice is platform- and compiler-dependent: aarch64 always fuses (FMA
is mandatory in the ISA), x86 fuses only with `-mfma`, MSVC defaults
to no fusion under `/fp:precise`.

**Fix:** the build adds `-ffp-contract=off` (gcc/clang) or `/fp:precise`
(MSVC), and `alwan_math.h` redefines `ALWAN_FMA(a,b,c)` to `((a)*(b)+(c))`
so the compiler can't re-fuse behind our back. Two roundings, everywhere.

### 3. SIMD reduction order

`hsum(v[0..N])` is computed differently per width:

```
SSE2 (4-lane f32):   ((v0+v1) + (v2+v3))
AVX  (8-lane f32):   (((v0+v1)+(v2+v3)) + ((v4+v5)+(v6+v7)))
NEON (4-lane f32):   vaddvq_f32 — implementation-defined order
Scalar:              (((v0+v1)+v2)+v3)+...
```

Different rounding cascades, different last-bit. Difference is 1–2 ULPs
typically but compounds in long pipelines.

**Fix:** in det mode `alwan_simd_*_hsum` and `_hadd` route to a canonical
scalar reduction left-to-right, regardless of native vector width. Per-lane
operations (add, mul, fma, polynomial evaluation) stay vectorised because
each lane runs the same op — width can't perturb bit-exactness there.

### 4. SIMD vs scalar arithmetic divergence

Even when both paths use the same primitives, the SIMD kernels often
optimise differently from the scalar formulas. Two examples we've hit:

- **PQ**: scalar uses `linear / 10000.0`; SIMD precomputes `inv10k = 1.0 / 10000.0`
  and uses `linear * inv10k`. Mathematically equal — `1/10000` is not
  exactly representable in f64, so the product differs by 1 ULP from
  the division. After two `pow` calls the divergence becomes ~1e-10 abs.

- **HLG**: scalar uses `c = 0.5 - a*ln(4a)` (computed at runtime);
  SIMD uses the literal `0.55991072952956202`. The polynomial `ln`
  is accurate to ~1e-12 vs the literal; the difference is small
  (~1e-12) but enough to break bit-exact.

**Fix:** in det mode, kernels that match this pattern lane-unpack
through the canonical scalar primitive. Specifically:

- sRGB / BT.2020 / BT.709 OETF and EOTF (lane-unpacked through `alwan_det_*_oetf` / `_eotf`).
- PQ / HLG OETF and EOTF (lane-unpacked through `alwan_pq_oetf_v` / `alwan_hlg_oetf_v` / etc.).
- JzAzBz PQ OETF / EOTF (lane-unpacked with the formula inlined; cross-TU header dependencies make linking the named scalar awkward, so the formula is reproduced verbatim using `alwan_map_scalar_pow`).
- Lab `f(t)` cube-root branch (lane-unpacked through `alwan_det_cbrt`).
- IPT and Oklab kernels (entire SIMD body gated off; the per-pixel scalar
  function runs for every pixel — matrix-multiply codegen between SIMD and
  scalar diverges at last bit even with `/fp:precise`).

This is the largest source of perf cost in det mode. Lane-unpacking
defeats vectorisation on those kernels — typically ~10× slower than
the fast-mode SIMD path on a per-kernel basis. Element-wise SIMD
kernels (matrix transforms with no `pow`/`log`) keep their vectorised
path and pay no cost.

### 5. Denormal flushing

Apple Silicon (and some x86 settings) flush denormals to zero by default
(FZ=1 in FPCR). A value of `1e-38` becomes `0` on those platforms but
stays `1e-38` on Linux x86. After a multiplication chain the flushed-vs-not
divergence becomes user-visible.

**Status:** alwan does not currently set FZ explicitly. Almost all
colour-science values stay well above the denormal threshold, so this
hasn't bitten us in CI yet. If you see a det-mode hash diff that's
isolated to the smallest-magnitude inputs, this is the prime suspect —
file an issue.

---

## ULP-distance testing

Throughout the test suite, "is this output close enough?" is measured
in **ULPs** (Units in the Last Place) rather than absolute or relative
tolerance. Two `double`s differ by *N* ULPs if and only if their bit
representations (after canonicalising the sign) differ by *N* when
read as `int64_t`.

```c
static inline uint64_t alwan_ulps_f64(double a, double b) {
    if (a == b) return 0;                      // covers +0 vs -0
    if (isnan(a) || isnan(b)) return UINT64_MAX;
    int64_t ia, ib;
    memcpy(&ia, &a, sizeof(ia));
    memcpy(&ib, &b, sizeof(ib));
    if (ia < 0) ia = (int64_t)0x8000000000000000LL - ia;
    if (ib < 0) ib = (int64_t)0x8000000000000000LL - ib;
    return (uint64_t)((ia > ib) ? (ia - ib) : (ib - ia));
}
```

Why ULP-distance instead of `fabs(a - b) < eps`?

- **Self-documenting.** "16 ULP budget" tells you exactly how many
  bits of mantissa you've lost; "1e-12" tells you nothing without the
  value's magnitude.
- **Magnitude-aware automatically.** A 4-ULP delta near 1.0 is ~1e-15;
  a 4-ULP delta near 100 is ~1e-13; a 4-ULP delta near 1e-6 is ~1e-21.
  Same budget tracks all of them.
- **Platform-portable.** No need to widen the threshold "because Apple
  libm". The budget tracks accumulated rounding regardless of where
  it came from.
- **Bit-exact is "0 ULP".** No special syntax, no separate macro.

Test macros: `TEST_ASSERT_CLOSE_ULP_F64(actual, expected, max_ulps, msg)`,
`TEST_ASSERT_BITEXACT_F64(actual, expected, msg)`, plus `_F32` and
buffer-pointwise variants. See `alwan_dev/tests/test_common.h`.

The classic reference for the technique is Bruce Dawson, [*Comparing
Floating Point Numbers, 2012 Edition*](https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/).

---

## Compiler and platform support

Tested in CI on:

- Linux x86_64: gcc 11+, clang 14+
- Linux aarch64: gcc 11+, clang 14+
- macOS x86_64: Apple clang
- macOS aarch64 (Apple Silicon): Apple clang
- Windows x86_64: MSVC 19+
- Windows aarch64: MSVC 19+

The CI workflow `.github/workflows/determinism.yml` runs the
`det_run_regression` tool (12 deterministic primitives × 1024 sample
points = 16,401 lines of hex-dumped IEEE-754 bytes per platform) on
every runner and asserts that all artefacts match. If any byte differs,
the build fails and the diff is printed. The current reference hash
is `fe46f566a0a84a41e8d219222bdb1b92`.

Optimization levels: `-O0` through `-O3` all produce the same output.
The polynomials are written so that LICM, common-subexpression
elimination, and re-association under `-O3` cannot perturb the result —
because `-ffp-contract=off` blocks the rewrites that could.

Out of scope:

- **`-ffast-math` / `/fp:fast`.** Re-enables associativity rewrites,
  NaN/Inf shortcuts, and FTZ changes that break IEEE semantics. We
  don't try to recover those — if you compile alwan with `-ffast-math`,
  determinism is forfeit.
- **f128 / long double.** Stays f32/f64 only.
- **GPU backends (HLSL/GLSL/Halide/CUDA).** Out of scope.

---

## Performance cost

Indicative numbers from the in-tree microbenchmark (`alwan_dev/bench`)
on a 4-core x86_64 / SSE2 build:

| Operation                   | Fast mode  | Det mode  | Cost  |
| :-------------------------- | :--------- | :-------- | :---- |
| `srgb_oetf` (1M pixels)     | 11.2 ms    | 12.8 ms   | +14%  |
| `xyz_to_oklab` (1M pixels)  | 18.3 ms    | 24.1 ms   | +32%  |
| `xyz_to_jzazbz` (1M pixels) | 21.7 ms    | 23.9 ms   | +10%  |
| `pq_oetf` (1M pixels)       | 14.6 ms    | 17.2 ms   | +18%  |
| Matrix-only conversions     | unchanged  | unchanged | 0%    |

(Numbers are ballpark — your hardware may vary.)

Most user pipelines see <20% total slowdown. Pipelines dominated by
Oklab cube-root or IPT (which fall back to per-pixel scalar in det mode)
see closer to 30–40%.

If perf in det mode matters more than the contract: the design space
is open for "deterministic-with-vectorised-kernel" variants that route
SIMD reductions canonically but keep the vectorised polynomial in the
hot path. We chose the strict path because (a) the contract surface is
simpler to reason about, and (b) the kernels that lose SIMD aren't on
the critical path for most workloads.

---

## Design decisions

A few choices that came up while building this and the reasoning we
landed on. These aren't reversible without breaking the bit-exact
contract for existing users, but they're worth knowing.

### Polynomial library, not a libm wrapper

We considered shipping a wrapper that called CRlibm or musl libm under
the hood for "deterministic libm". Rejected because:

- Compile-time dependency on a specific libm fork that may not exist
  on every target.
- Crlibm is GPL/LGPL — viral for our MIT consumers.
- The Chebyshev minimax fit gives us 1e-12-class accuracy at a fraction
  of the code size and we own every line of it.

The downside: `alwan_det_pow_pos` is not bit-equal to libm `pow`. If
your reference baseline was generated against `libm` (e.g., a Python
script using `math.pow`), enabling deterministic mode will shift every
hash by a fixed amount. The shift is reproducible across platforms,
which is the whole point — but it's not zero.

### Polynomial fitting in normalised basis

The first cut of `gen_tf_polynomials.py` used `numpy.polynomial.Chebyshev.fit`
with `domain=[lo, hi]` and converted to a power-basis polynomial.
That gave coefficients up to 3.7e17 — fine in f64, completely lost in
f32 (24-bit mantissa). After pre-normalising x to `u ∈ [-1, 1]` before
the Chebyshev fit and emitting the polynomial in `u`, coefficients
drop to O(0.2–0.8) and the f32 path agrees with f64 to 1.8e-7.

Lesson: numpy's `domain`/`window` machinery preserves the user-domain
basis, so the converted coefficients still encode the wide-range scaling.
Always pre-normalise.

### Canonical scalar reduction over Kahan

For `hsum`, det mode uses a plain left-to-right `acc = acc + lanes[i]`
loop. Kahan / Neumaier compensated summation would give better numerical
accuracy across long arrays, but adds two extra adds per lane and an
extra register. We picked plain summation because:

- The arrays passed to alwan reductions are 4–16 lanes wide.
- For 1024-sample SPD reductions inside `alwan_spd.c`, the sum-error
  is O(N·eps) ≈ 1e-13 absolute — well below any colour-science
  tolerance we care about.
- Kahan does buy reproducibility-with-better-precision but adds 30%
  runtime cost on the hsum-bound paths.

We may revisit Kahan for `alwan_spd.c` specifically if a user shows a
case where the naive reduction error matters.

### Lane-unpack rather than re-vectorise the polynomial

We chose to lane-unpack approximate SIMD kernels (`pow24`, `cbrt_fast`)
through the scalar polynomial in det mode rather than re-implement the
deterministic polynomial in SIMD intrinsics per backend. Reasons:

- One implementation, one regression target.
- The polynomial coefficients are generated by `gen_tf_polynomials.py`
  and `gen_math_polynomials.py`; reusing them in 4 SIMD backends
  (SSE2, AVX, AVX2, NEON, scalar) would multiply maintenance.
- The perf cost is paid only when the user opts into det mode; fast
  mode keeps the vectorised approximation.

A future "vectorised-deterministic" mode could re-emit the canonical
polynomial in SIMD form per backend if benchmarks demand it. The
architecture supports it — the lane-unpack is only the fallback.

---

## References

Background reading on the techniques used:

- Bruce Dawson, *Comparing Floating Point Numbers, 2012 Edition*. The
  ULP-distance via integer-cast canonicalization comes from here.
  [randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition](https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/)
- N. J. Higham, *Accuracy and Stability of Numerical Algorithms* (2002).
  Chapter 2: ULP/eps reasoning. Chapter 4: summation accuracy. The
  rule of thumb `N ≈ 2·sqrt(operations) + 4` for ULP budgets is from
  Chapter 2.
- William Kahan, *Lecture Notes on the Status of IEEE Standard 754*.
  The original argument for taking floating-point determinism seriously.
- Sollya, [sollya.org](https://www.sollya.org/). Industrial-strength
  minimax polynomial generator. We don't ship a Sollya dependency —
  the gendata scripts use scipy as a fallback — but Sollya is the
  reference if you ever want to re-derive the coefficients with
  proven minimax error.
- ARM Architecture Reference Manual, Section A1.5.6 (FPCR / FZ flag).
  Why Apple Silicon flushes denormals.
- The road map and engineering history live in
  [`road_to_determinism.md`](../road_to_determinism.md) at the repo
  root — read that for the workstream-by-workstream story and the
  things we learned along the way.
