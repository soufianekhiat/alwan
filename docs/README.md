# Alwan Documentation

Top-level documentation for the Alwan colour science library.

`docs/api/` contains the module reference. The files in this directory are the
guides, design notes, comparisons, and status documents that explain how to use
the library around that API surface.

> Runtime / on-demand data loading is still planned only. Today the supported
> build is embedded data mode: `ALWAN_EMBED_DATA=1`.

---

## Start Here

- [getting-started.md](getting-started.md) - first build, first program, and `{T}` convention
- [configuration.md](configuration.md) - compile-time switches, allocators, deterministic mode
- [examples.md](examples.md) - representative workflows, including typed, `_ex`, planar, image, LUT, and interop helpers

## Core Guides

- [api-conventions.md](api-conventions.md) - naming and signature patterns used by `alwan.h`
- [map.md](map.md) - batch processing APIs, SIMD-backed map layer, typed-pixel entry points
- [precision-and-limits.md](precision-and-limits.md) - choosing `_f32` vs `_f64`, numerical expectations, determinism trade-offs
- [data-management.md](data-management.md) - embedded data today, runtime loading plan for a future release
- [determinism.md](determinism.md) - what `ALWAN_DETERMINISTIC` changes and why
- [ranges.md](ranges.md) - channel-range conventions and `ALWAN_NORMALIZE_RANGES`

## Companion Docs

These stay in `docs/` intentionally because they support the whole library, not
just one API module.

- [compare_api.md](compare_api.md) - capability comparison against related libraries
- [lib_mem.md](lib_mem.md) - how external libraries lay out pixels in memory, and how Alwan maps onto them

## Planning And Audit Docs

- [TODO_ColorInterop.md](TODO_ColorInterop.md) - current state of the Color Interop Forum work
- [alwan_future.md](alwan_future.md) - forward-looking roadmap items
- [violations.md](violations.md) - code-quality audit notes and remaining follow-ups
- [../road_to_determinism.md](../road_to_determinism.md) - workstream history
  for the `ALWAN_DETERMINISTIC=ON` path; the user-facing summary is in
  [determinism.md](determinism.md)

## API Reference

Module reference pages live under [`docs/api/`](api/).

Key entry points:

- [`docs/api/context.md`](api/context.md)
- [`docs/api/color-spaces.md`](api/color-spaces.md)
- [`docs/api/chromatic-adaptation.md`](api/chromatic-adaptation.md)
- [`docs/api/transfer-functions.md`](api/transfer-functions.md)
- [`docs/api/gamut.md`](api/gamut.md)
- [`docs/api/backends.md`](api/backends.md)

## Notes On Templates

Several guide pages keep `{T}` placeholders in examples.

- `{T}` means `f32` or `f64`
- `alwan_scalar_{T}` means `alwan_f32` or `alwan_f64`
- templated examples are documentation shorthand, not copy-paste typedefs

When exact function signatures matter, always prefer `src/alwan/alwan.h`.
