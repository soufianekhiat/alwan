# Precision And Limits

How to choose `_f32` vs `_f64`, what normalization changes, and where
deterministic mode fits.

---

## Precision Model

Alwan exposes explicit precision variants at the public API boundary:

- `_f32` for `float`
- `_f64` for `double`

There is no global "default precision" switch *at the call site*: each call
picks the suffix it wants, and a `both` build ships every `_f32` and `_f64`
entry point side by side with its own embedded data tables. The default-scalar
alias `alwan_scalar` (`double`, or `float` when `ALWAN_SCALAR_IS_FLOAT=1`) only
backs the convenience value types and GPU backends.

### Single-precision-only builds

You can, however, compile a single-precision-only library to shrink both the
compiled code and the embedded data footprint. Define at most one of these
macros before including any alwan header (or use the CMake `ALWAN_BUILD_PRECISION`
cache variable, which sets them for you):

| User macro            | CMake `ALWAN_BUILD_PRECISION` | Result                          |
|-----------------------|-------------------------------|---------------------------------|
| *(neither; default)* | `both`                        | `ALWAN_WITH_F32=1`, `ALWAN_WITH_F64=1` |
| `ALWAN_BUILD_ONLY_F32`| `f32`                         | f32 API + data only             |
| `ALWAN_BUILD_ONLY_F64`| `f64`                         | f64 API + data only             |

`alwan_build_config.h` resolves these into the internal gates `ALWAN_WITH_F32`,
`ALWAN_WITH_F64`, and `ALWAN_WITH_BOTH`. Defining both `ONLY_*` macros is a
`#error`. Public *declarations* and the `alwan_*_f32` / `alwan_*_f64` struct
typedefs stay present in every build (they cost nothing); only the *definitions*
and embedded *data* twins are gated, so calling an excluded-precision symbol
fails at **link** time rather than compile time. A single-precision build also forces
`alwan_scalar` to the matching precision (`ALWAN_BUILD_ONLY_F32` implies
`ALWAN_SCALAR_IS_FLOAT=1`; conflicting combinations are `#error`s).

### f64-internal facades

A handful of `_f32` public entry points are numerically **f64-internal
facades**: they run the algorithm in `double` and narrow the result, because
their f32 path is not numerically viable. This is a **design choice rather than
a gap**: each is an iterative solver, a wavelength integration, or a
least-squares fit whose precision and run-to-run repeatability depend on a single
f64 core; they should **not** be re-implemented in native f32. These stay
**available even in an f32-only build**: their private double-precision
machinery (and the f64 data it reads) is gated by `ALWAN_WITH_F64_FACADE`
(always `1`) rather than `ALWAN_WITH_F64`. They are:

- **ZCAM** (forward + inverse, incl. `alwan_delta_e_zcam_f32`) and **ACES 1.x
  inverse**: iterative inverses whose convergence thresholds fall below f32
  epsilon.
- **Cheung 2004 / Finlayson 2015 CCM fits**: least-squares solves whose normal
  equations square the condition number.
- **Gamut volume / ratio / coverage** (`alwan_gamut_volume_f32`,
  `alwan_gamut_volume_ratio_f32`, `alwan_gamut_coverage_f32`): computed in f64 for
  stability; `volume` is an exact `|det(M)|`, ratio/coverage are reductions over it.
- **Spectral quality metrics** (`alwan_cri_ra_f32`, `alwan_cqs_calculate_f32`,
  `alwan_tm30_rf_f32`, `alwan_cie224_rf_f32`, `alwan_ssi_calculate_f32`,
  `alwan_metamerism_index_f32`): wavelength integration over f64 CMF tables.
- **`alwan_cct_kang_xy_f32`**: Newton-Raphson solver with sub-f32-epsilon
  tolerances (the other CCT estimators are native f32).
- **Munsell / NCS / ColorChecker / RGB-space reference-data lookups**: the
  renotation/patch/primaries tables are f64; the `_f32` accessors read them and
  narrow (see [reference-data.md](api/reference-data.md)).

For the full build matrix, Sharpmake/CMake flavor mapping, and binary-size
trade-offs, see [configuration.md](configuration.md).

---

## Choosing `_f32` vs `_f64`

### Prefer `_f32` when:

- your pipeline is graphics-oriented
- your host buffers are already `float`
- memory bandwidth matters
- tiny numerical drift is acceptable

### Prefer `_f64` when:

- you are validating algorithms
- you want closer agreement with scientific references
- you are chaining many transforms
- you care about stable regression baselines

---

## Templated Example

```c
alwan_xyz_{T} xyz = {0.5, 0.6, 0.4};
alwan_xyz_{T} d65;
alwan_lab_{T} lab;

alwan_illuminant_white_point_{T}(
    &d65, ALWAN_ILLUMINANT_D65, ALWAN_OBSERVER_CIE_1931_2DEG);
alwan_xyz_to_lab_{T}(&lab, &xyz, &d65);
```

---

## Normalized Public Ranges

On the C backend, `ALWAN_NORMALIZE_RANGES` defaults to `1`.

That means public API calls normalize bounded channels such as:

- `Lab.L`: `[0, 100]` -> `[0, 1]`
- `LCh.h`: `[0, 360)` -> `[0, 1]`
- `LChuv.h`: `[0, 360)` -> `[0, 1]`
- `Oklch.h`, `JzCzhz.hz`, `IPTch.h`: angle domain -> `[0, 1]`

Unbounded channels remain unbounded.

If you need the raw mathematical ranges at the public API boundary:

```c
#define ALWAN_NORMALIZE_RANGES 0
#include "alwan.h"
```

This setting does not change the core internal math formulas; it changes the
public normalization layer around bounded channels.

See [ranges.md](ranges.md) for the channel summary.

---

## Deterministic Mode

`ALWAN_DETERMINISTIC` is the library's reproducibility mode.

Use it when you care about:

- cross-platform golden files
- stable CI comparison dumps
- byte-level regression outputs

Expect trade-offs:

- slower transcendentals than the fastest platform `libm` path
- more conservative SIMD behavior in sensitive kernels
- slightly different last-bit behavior compared to the fastest non-deterministic path

See [determinism.md](determinism.md) for the detailed architecture.

---

## Practical Limits

### Matrix inversion

Use the return code from `alwan_mat3_inv_{T}` and treat failure as a real
condition:

```c
alwan_mat3x3_{T} m, inv;
if (alwan_mat3_inv_{T}(&inv, &m) != ALWAN_OK) {
    /* singular or numerically unsafe */
}
```

### White-point-dependent transforms

Functions such as `alwan_xyz_to_lab_{T}` and `alwan_lab_to_xyz_{T}` require a
valid white point. Passing the wrong white point is often a larger error source
than choosing `_f32` instead of `_f64`.

### Typed-pixel frontends

`_ex` APIs introduce quantization boundaries from the source or destination
format:

- `U8` adds 8-bit quantization
- `U16` adds integer quantization
- `F16` adds half-precision conversion

If you need maximum numerical stability, stage into `alwan_f64` buffers first
with `alwan_collect3_f64`.

---

## Recommended Usage Patterns

### Real-time display / grading tools

- use `_f32`
- use `_map_interleave_ex` or image helpers directly on host buffers
- enable deterministic mode only if reproducibility matters more than throughput

### Validation / regression tools

- use `_f64`
- prefer descriptor-driven conversion paths
- enable `ALWAN_DETERMINISTIC`
- keep `ALWAN_NORMALIZE_RANGES` explicit so tests do not depend on hidden assumptions

### Scientific or reference workflows

- use `_f64`
- use explicit white points and descriptors everywhere
- keep an eye on normalization if you compare against external references that
  use raw mathematical ranges

---

## Related Docs

- [configuration.md](configuration.md)
- [configuration.md](configuration.md)
- [ranges.md](ranges.md)
- [determinism.md](determinism.md)
- [examples.md](examples.md)
