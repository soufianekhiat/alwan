# Changelog

All notable changes to this project will be documented in this file.

---

## [Unreleased] / [2.0.0-rc]

> No `v2.0.0` git tag has been cut yet; the entries below land on top of the
> dated `2.0.0` block and describe the current release-candidate state.

### Added
- **Cross-platform byte-identical file I/O** — CLF (SMPTE ST 2136-1:2024) and
  `.cube` export are now hardened to produce byte-for-byte identical files on
  every supported platform/precision, including native-f32 cube export
  (commits `069240e`, `6c14449`).
- **File-I/O byte content on the determinism contract** — the determinism
  guarantee now explicitly covers the exact bytes written to `.cube`/`.clf`
  files, not just in-memory numeric output. The determinism harness MD5s the
  on-disk files and asserts hash equality across runners (commit `5f311c4`).

### Improved
- **NEON gamut kernel completion** — the aarch64 NEON gamut-mapping kernel is
  finished (select-vs-mask path), bringing the NEON backend to parity with the
  SSE2/AVX/AVX2 kernels; MSVC ARM warnings (C4189 unused-local, C4101
  unused-local) cleaned up (commit `2ced0f6`).
- **Determinism CI extended to six runners** — `determinism.yml` now runs on
  all six Linux/Windows/macOS × x86_64/aarch64 targets and asserts
  byte/hash equality against the first target. The `det_run_regression` dump
  grew to ~407k lines pinning the full public API surface, and NEON f64 on
  aarch64 is exercised so any FMA-contraction or SIMD reduction-order drift
  fails the build.
- **Test coverage** — 94 test suites (up from 84).

### In progress
- **Native f32/f64 dualization** — migrating the f32 API from f64 facades to
  genuine native single-precision code + data twins. The allocator, cube, view,
  CAM, RLAB, SPD, and spectrum-upsampling subsystems are now native
  dual-precision (their cores are instantiated once per precision and compute in
  float throughout); ACES, ZCAM, and gamut remain f64-internal facades wrapped
  for f32, where the iterative-inverse / least-squares cores are gated by
  `ALWAN_WITH_F64_FACADE` (always `1`). Precision is selected per call site via
  the `_f32`/`_f64` suffixes (`alwan_f32`/`alwan_f64`), with `alwan_scalar` as
  the default-precision alias; an entire single-precision-only library can be
  built with `ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64` (resolved to
  `ALWAN_WITH_F32` / `ALWAN_WITH_F64`, both on by default).

---

## [2.0.0] — 2026-03-24

### Added
- **F16 pixel format** — half-float as a first-class pixel format (U8/U16/F16/F32/F64 parity)
- **F32/F64 runtime dispatch** — single binary emits both precision variants; no recompilation required
- **LUT system** — 1D/2D/3D LUT baking, import/export (.cube, CLF/Color Interop Forum XML)
- **CVD simulation** — Machado 2009 for protanopia, deutanopia, tritanopia at 11 severity levels
- **HDR tone mapping** — BT.2446 Method A, BT.2390 EETF, Reinhard 2002, Stachowiak 2023, MaxCLL/MaxFALL
- **Video signal encoding** — full and narrow range YCbCr per SMPTE BT.601/709/2020
- **High-level image API** — `alwan_image_convert`, `alwan_image_convert_rgba` with stride and premultiplied alpha
- **New color spaces** — LCH(ab), HLC, HSLuv, HPLuv, OkHSL, OkHSV
- **Color Interop Forum** — CIF interoperability helpers and CLF export
- **GPU shader backends** — GLSL, HLSL, Halide header-only backends

### Improved
- **SIMD vectorization** — AVX2 kernels for sRGB↔XYZ, sRGB↔CIELab, sRGB↔Oklab; force-inlined eotf/oetf under SVML
- **U16 SIMD** — AVX2 paths for U16 planar (8 px/batch) and AoS (4 px/batch); previously scalar
- **SIMD scalar arrays** — `alwan_eotf_apply` / `alwan_oetf_apply` gain contiguous SIMD fast paths
- **Test coverage** — 84 test suites (up from 75)

---

## [1.0.0] — 2025

Initial release.
