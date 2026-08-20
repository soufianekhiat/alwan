# Changelog

All notable changes to this project will be documented in this file.

---

## [2.0.0] — 2026-08-19

First public release (tag `v2.0.0`). Everything below plus the dated
foundation block that follows shipped together.

### Added
- **Table reader layer** — every embedded table is now read through a
  declared reader (`alwan_table1d_sample`, `alwan_table2d_sample`,
  `alwan_table3d_sample`, `alwan_table1d_mat3_sample`, plus named readers
  for the AgX contrast curves, the AgX Blender cube and the Jakob2019
  coefficients). Readers take an `alwan_sample_mode`
  (`LINEAR`/`NEAREST`/`TRILINEAR`/`TETRAHEDRAL`, optionally
  `| ALWAN_SAMPLE_STRICT`), and a single addressing gate converts the
  coordinate to a validated cell, so no reader indexes a table unchecked.
  Definitions live in `src/alwan/data/`, declarations in `alwan.h`.
- **`alwan_version_string()`** — reports the version compiled into the
  binary, for checking a dynamically loaded library against the headers.
- **`.cube` import size query** — passing `NULL` for the LUT buffer parses
  the header and returns the cube size, so callers can allocate exactly.
- **`ALWAN_READ_DATA_NO_BOUND_CHECK`** — opt-in switch that compiles the
  addressing clamp out when every coordinate is known finite and in range.

### Fixed
- **Out-of-bounds table reads on non-finite coordinates** — a NaN
  coordinate defeated every `SATURATE`/range guard (all comparisons against
  NaN are false) and reached an `(int)`/`(size_t)` cast, which is undefined
  behaviour and yields `INT_MIN` on x86-64. That indexed the table far out
  of bounds. Affected the AgX contrast LUT, the AgX Blender cube, the public
  `alwan_lut{1,2,3}d_sample` family, `alwan_table_interp_1d`,
  `alwan_table_interp_3d_{trilinear,tetrahedral}` and SPD resampling.
  A NaN coordinate now resolves to the low edge everywhere; results for
  finite in-range input are unchanged, bit for bit.
- **Picture formation** — `alwan_gamut_map_spatial` with 18 spatial
  formation methods (`alwan_gamut_formation_method`), developed in
  correspondence with Troy Sobotka; the `COMPLETE` /
  `COMPLETE_HEMI_LOOK` operators are the first to satisfy all 15
  numerically-testable formation constraints at once, and the constraint
  matrix ships as a regression test (`docs/picture_formation.md`).
- **Experimental formation tier** — `src/alwan/experimental/`:
  evidence-driven research operators (`alwan_picture_form_hybrid_exp`,
  `_global_exp`, `_local_exp`, `_evidence`, `_pure_exp`), clearly fenced
  and structurally tested.
- **AgX family expansion** — `ALWAN_VIEW_AGX_{ORIGINAL,PUNCHY,GOLDEN,
  SB2383,BLENDER}` presets plus the parameterized analytic AgX engine and
  the JP2499 display transform; HLSL-compilable analytic core.
- **Cross-platform byte-identical file I/O** — CLF (SMPTE ST 2136-1:2024)
  and `.cube` export are hardened to produce byte-for-byte identical files
  on every supported platform/precision, including native-f32 cube export.
- **File-I/O byte content on the determinism contract** — the determinism
  guarantee explicitly covers the exact bytes written to `.cube`/`.clf`
  files, not just in-memory numeric output; the harness MD5s the on-disk
  files and asserts hash equality across runners.

### Improved
- **Native f32/f64 dualization** — the allocator, cube, view, CAM, RLAB,
  SPD, and spectrum-upsampling subsystems are native dual-precision
  (cores instantiated once per precision, computing in float throughout);
  the iterative-inverse / least-squares cores (ZCAM/ACES 1.x inverses,
  CCM fits, gamut volume/coverage) remain f64-internal facades gated by
  `ALWAN_WITH_F64_FACADE` (always `1`). Single-precision-only builds via
  `ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64`.
- **NEON gamut kernel completion** — the aarch64 NEON gamut-mapping kernel
  reaches parity with the SSE2/AVX/AVX2 kernels (select-vs-mask path).
- **Determinism CI on six runners** — `determinism.yml` runs on all six
  Linux/Windows/macOS × x86_64/aarch64 targets and asserts byte/hash
  equality; the `det_run_regression` dump pins the full public API surface
  (~407k lines), with NEON f64 exercised so FMA-contraction or SIMD
  reduction-order drift fails the build.
- **Test coverage** — 102 test suites (up from 84).

---

## [2.0.0-foundation] — 2026-03-24

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
