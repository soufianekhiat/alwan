# Changelog

All notable changes to this project will be documented in this file.

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
