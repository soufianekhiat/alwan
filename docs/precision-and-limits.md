# Precision And Limits

How to choose `_f32` vs `_f64`, what normalization changes, and where
deterministic mode fits.

---

## Precision Model

Alwan exposes explicit precision variants at the public API boundary:

- `_f32` for `float`
- `_f64` for `double`

There is no global public "default precision" switch. Call sites choose the
suffix they want.

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
- [ranges.md](ranges.md)
- [determinism.md](determinism.md)
- [examples.md](examples.md)
