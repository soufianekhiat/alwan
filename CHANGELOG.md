# Changelog

All notable changes to this project will be documented in this file.

---

## [2.0.0], 2026-08-28

First public release (tag `v2.0.0`).

The tag sits after a correctness pass over the areas that had no external
ground truth, 2026-08-27..28. Several entries below change published output
relative to the 2026-08-19 pre-release build; they are listed first because a
caller who pinned values against that build will see them.

### Changed: output differs

- **ACESproxy is now quantised.** It is defined as an *integer* log encoding, so
  the rounding is part of the transfer function. Linear 0.18 now encodes as
  `426/1023 = 0.4164223`, matching colour-science exactly, where it previously
  returned the continuous `426.30344/1023`. `EOTF(OETF(x))` is now a staircase and
  returns the centre of the code value the input landed in, within half a step
  (`2^(1/100) - 1 = 6.96e-03`).
- **CQS Qa** ran NIST 7.4's scaling factor (3.104) with 9.0's absent CCT factor,
  which is neither method. 9.0 uses 3.2. Deviation from colour-science over 33
  illuminants: mean 0.447 -> 0.065, max 4.490 -> 0.235, D65 99.037 -> 100.0001.
- **Y'CbCr and YcCbcCrc chroma** was centred twice in the default build
  (`ALWAN_NORMALIZE_RANGES=1`), once by the kernel and once by the normalisation
  layer, so it came out offset by 1.0 instead of 0.5. The legal/full range pair
  assumed chroma centred on 0 while every producer emits it centred on 0.5.
- **The ACES 1.x inverse output transform** now inverts its own forward. Worst
  relative round-trip over non-clipping chromatic input, all twelve outputs:
  1.31e-01 -> 1.337e-11. Three defects: the inverse RedMod10 and Glow10 were
  omitted entirely, `DCDM_48NIT` returned early through an unrelated AP1-space
  path, and the inverse dimSurround used exponent `g` where it needed `g/(1-g)`.
  alwan's RedMod10 inverse is now more accurate than OCIO's, which stops at the
  closed form and round-trips to 3.7e-03.
- **Blackbody SPDs** are documented as spectral radiance but returned exitance,
  so values were pi times too large. Now matches colour-science's `planck_law`
  to 3.7e-10. Chromaticity was unaffected either way.
- **HSLuv** returned a literal `1.0` from `l2y` for every `L > 8`, so every
  non-dark lightness decoded to white. L now round-trips to 1.4e-14.
- **HSY** decoded hue 1.0, documented as in-domain, to magenta instead of red.
- **HCL** produced `H = 5pi/3`, outside the model's `[-pi, pi]`, when `R - G` was
  small and negative.
- **`alwan_xyz_from_spd`'s `bandpass_nm`** was consumed by a `(void)` cast while
  four documents said the Stearns & Stearns 1988 correction was applied. It is
  now implemented, using the original neighbours as the paper specifies.
  colour-science's version updates its array in place and so uses already
  corrected neighbours; the two differ by 4.3e-3 and alwan follows the paper.
- **CRI, TM-30 and CQS** now use a CIE daylight reference at or above 5000 K
  rather than D65 at every CCT, and adapt test-onto-reference rather than both
  onto a common third white. CES data extended from 80 samples to the full 99.
- **Colour appearance models**: Hunt implemented in full (4.6e-08), ATD95
  `spow_response` corrected and `H` returned as the model's ratio, RLAB
  adaptation fixed in both directions (5e-07), Kim2009 `media` exposed (4e-11).
- **Unsupported transfer functions** in the ACES 1.x path returned `ALWAN_OK`
  with scene-linear values. They now return the status they got.

### Added

- `alwan_table2d_grid_sample_f32` / `_f64`: bilinear and nearest sampling of a
  genuine `rows x stride` 2-d grid, which is the rank `ALWAN_SAMPLE_BILINEAR`
  fits. The 2-d strip is a flattened cube and still rejects BILINEAR.
- `ALWAN_SAMPLE_CATMULL_ROM` implemented at rank 1: the four-tap interpolating
  cubic with clamped outer taps. It can overshoot by about 1/8 of a step across
  a hard edge, so it is not any rank's default.
- `ALWAN_ROUND` on the HLSL, GLSL and Halide backends, and `ALWAN_CORE_ROUND`
  through the core aliases and both precision setups.
- `docs/alwan_decisions.md`: where alwan knowingly differs from a reference and
  why, including the house defaults, the no-gamut-clamp rule, and the precision
  model of the test suite.
- `docs/api/tables.md`: the table and LUT sampling family, which had no
  reference page. `alwan_gamut_map_spatial` and the bulk gamut forms added to
  `docs/api/gamut.md`. The remaining documentation gap is now measured rather
  than estimated: 118 of 411 base operations have no entry in `docs/api/`,
  clustered by area in `docs/alwan_future.md` theme 10.

### Fixed: no output change

- Table indices are tied to the enum that bounds them.
  `alwan_rgb_get_space_descriptor_*` bounds-checked `space` against one table's
  count and then subscripted three others; all four are now
  `_Static_assert`ed against `ALWAN_RGB_SPACE_COUNT`. `spd_copy_table` took its
  extent as a parameter unrelated to the table it was given.
- `alwan_spectral_locus_xy_*` accepted NaN: `x < MIN || x > MAX` is both-false
  for NaN, so it reached the cast and resolved to the high edge with
  `ALWAN_OK`. Range tests on float-indexed entry points are now negated.
- Two test suites could not fail. `47_llab` discarded two of its three test
  functions with `(void)` casts, and `33_rgb_spaces_p5` had twelve failure paths
  returning the runner's success code. Both were latent; alwan was right in
  every case, and the tests were wrong.
- The EXR loader (dev tool) asked OpenEXR for HALF regardless of storage, so a
  FLOAT channel saturated to inf above 65504. It now decodes as FLOAT, and
  rejects UINT rather than converting it into a plausible-looking image.

### Known

- TM-30 Rf still deviates from colour-science by 0.53 mean, concentrated in the
  Planckian branch. The integration convention, the blackbody SPD, the CCT
  method and the Rf formula are all ruled out with measurements. Tracked in
  `docs/alwan_future.md`.

---

### Foundation, 2026-08-19

The pre-release build the hardening pass started from. Everything below plus
the dated foundation block that follows is also in the tag.

### Added
- **ACES constants on the public macro surface**: `ALWAN_AP0_RED_x` .. 
  `ALWAN_AP0_BLUE_y` (SMPTE ST 2065-1 primaries) and `ALWAN_ACES_WHITE_x/y`,
  so ACES matrices can be derived from `alwan_platform.h` without hardcoding.
  Previously only the AP1 primaries and a D65 white were exported, and pairing
  those is a colour space that does not exist: the derived RGB-to-XYZ matrix is
  off by 8e-2 per element against ACEScg, and it compiled without complaint.
  `ALWAN_D60_x/y` (CIE D60) is also exported, and is deliberately NOT the same
  number as `ALWAN_ACES_WHITE_x/y`: ACES pins its white to a rounded value that
  differs from CIE D60 in the fifth decimal. Reported by the_flow2 after hitting
  the AP1-plus-D65 combination in their EXR loader.
- **Table reader layer**: every embedded table is now read through a
  declared reader (`alwan_table1d_sample`, `alwan_table2d_sample`,
  `alwan_table3d_sample`, `alwan_table1d_mat3_sample`, plus named readers
  for the AgX contrast curves, the AgX Blender cube and the Jakob2019
  coefficients). Readers take an `alwan_sample_mode`
  (`LINEAR`/`NEAREST`/`TRILINEAR`/`TETRAHEDRAL`, optionally
  `| ALWAN_SAMPLE_STRICT`), and a single addressing gate converts the
  coordinate to a validated cell, so no reader indexes a table unchecked.
  Definitions live in `src/alwan/data/`, declarations in `alwan.h`.
- **`alwan_version_string()`**: reports the version compiled into the
  binary, for checking a dynamically loaded library against the headers.
- **`.cube` import size query**: passing `NULL` for the LUT buffer parses
  the header and returns the cube size, so callers can allocate exactly.
- **`ALWAN_READ_DATA_NO_BOUND_CHECK`**: opt-in switch that compiles the
  addressing clamp out when every coordinate is known finite and in range.

### Fixed
- **Five camera log curves computed the wrong transfer function**: T-Log,
  REDLog, REDLogFilm, L-Log and ACESproxy each shipped a plausible-looking but
  incorrect curve, and V-Log picked the wrong branch exactly at its cut point.
  All six now match colour-science (and therefore OCIO / aces-dev) to f64
  round-off:
  - `ALWAN_TF_TLOG` was `0.33*log10(lin/0.01)+0.02`, a generic Cineon-shaped
    log that is neither FilmLight T-Log (0.21 away) nor Panasonic V-Log (0.16
    away), while its comment claimed V-Log and the README claimed T-Log. It is
    now the real FilmLight T-Log, including the `T-Log(0) = 0.075` black offset
    the old curve lacked. Pairs with E-Gamut, whose primaries were already
    correct.
  - `ALWAN_TF_REDLOG` and `ALWAN_TF_REDLOGFILM` used an invented
    `(log10(0.9x + 0.1) + 3)/3` form. They now use the Sony Imageworks
    reference encodings; REDLogFilm is Cineon, as the reference defines it.
  - `ALWAN_TF_LLOG` used Panasonic V-Log's constants. It now uses Leica's
    published L-Log parameters (`cut1 = 0.006`, `a = 8`, `b = 0.09`,
    `c = 0.27`, `d = 1.3`, `e = 0.0115`, `f = 0.6`).
  - `ALWAN_TF_ACESPROXY` omitted the `2^-9.72` floor and the
    `[CV_min, CV_max]` clamp entirely and used `log2(lin/0.18)` where
    S-2013-001 specifies `mid_log_offset = 2.5`; it was off by up to 113 code
    values. The spec's integer rounding is still left to the caller so the
    EOTF stays an exact inverse.
  - `ALWAN_TF_VLOG` split on `linear <= cut1`; Panasonic's spec splits on
    `linear < cut1`, so the cut point itself belongs to the log segment.
- **E-Log attributed to the wrong manufacturer**: `ALWAN_TF_ELOG` is
  Olympus/OM System OM-Log400, not a FilmLight curve (FilmLight's companion to
  T-Log is E-Gamut, a gamut, not a transfer function). The implementation was
  always correct; the enum comment, README and `docs/determinism.md` were not.
  The enum name is unchanged for compatibility.

- **Out-of-bounds table reads on non-finite coordinates**: a NaN
  coordinate defeated every `SATURATE`/range guard (all comparisons against
  NaN are false) and reached an `(int)`/`(size_t)` cast, which is undefined
  behaviour and yields `INT_MIN` on x86-64. That indexed the table far out
  of bounds. Affected the AgX contrast LUT, the AgX Blender cube, the public
  `alwan_lut{1,2,3}d_sample` family, `alwan_table_interp_1d`,
  `alwan_table_interp_3d_{trilinear,tetrahedral}` and SPD resampling.
  A NaN coordinate now resolves to the low edge everywhere; results for
  finite in-range input are unchanged, bit for bit.
- **Picture formation**: `alwan_gamut_map_spatial` with 18 spatial
  formation methods (`alwan_gamut_formation_method`), developed in
  correspondence with Troy Sobotka; the `COMPLETE` /
  `COMPLETE_HEMI_LOOK` operators are the first to satisfy all 15
  numerically-testable formation constraints at once, and the constraint
  matrix ships as a regression test (`docs/picture_formation.md`).
- **Experimental formation tier**: `src/alwan/experimental/`:
  evidence-driven research operators (`alwan_picture_form_hybrid_exp`,
  `_global_exp`, `_local_exp`, `_evidence`, `_pure_exp`), clearly fenced
  and structurally tested.
- **AgX family expansion**: `ALWAN_VIEW_AGX_{ORIGINAL,PUNCHY,GOLDEN,
  SB2383,BLENDER}` presets plus the parameterized analytic AgX engine and
  the JP2499 display transform; HLSL-compilable analytic core.
- **Cross-platform byte-identical file I/O**: CLF (SMPTE ST 2136-1:2024)
  and `.cube` export are hardened to produce byte-for-byte identical files
  on every supported platform/precision, including native-f32 cube export.
- **File-I/O byte content on the determinism contract**: the determinism
  guarantee explicitly covers the exact bytes written to `.cube`/`.clf`
  files, not just in-memory numeric output; the harness MD5s the on-disk
  files and asserts hash equality across runners.

### Improved
- **Native f32/f64 dualization**: the allocator, cube, view, CAM, RLAB,
  SPD, and spectrum-upsampling subsystems are native dual-precision
  (cores instantiated once per precision, computing in float throughout);
  the iterative-inverse / least-squares cores (ZCAM/ACES 1.x inverses,
  CCM fits, gamut volume/coverage) remain f64-internal facades gated by
  `ALWAN_WITH_F64_FACADE` (always `1`). Single-precision-only builds via
  `ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64`.
- **NEON gamut kernel completion**: the aarch64 NEON gamut-mapping kernel
  reaches parity with the SSE2/AVX/AVX2 kernels (select-vs-mask path).
- **Determinism CI on six runners**: `determinism.yml` runs on all six
  Linux/Windows/macOS × x86_64/aarch64 targets and asserts byte/hash
  equality; the `det_run_regression` dump pins the full public API surface
  (~407k lines), with NEON f64 exercised so FMA-contraction or SIMD
  reduction-order drift fails the build.
- **Test coverage**: 102 test suites (up from 84).

---

## [2.0.0-foundation], 2026-03-24

### Added
- **F16 pixel format**: half-float as a first-class pixel format (U8/U16/F16/F32/F64 parity)
- **F32/F64 runtime dispatch**: single binary emits both precision variants; no recompilation required
- **LUT system**: 1D/2D/3D LUT baking, import/export (.cube, CLF/Color Interop Forum XML)
- **CVD simulation**: Machado 2009 for protanopia, deutanopia, tritanopia at 11 severity levels
- **HDR tone mapping**: BT.2446 Method A, BT.2390 EETF, Reinhard 2002, Stachowiak 2023, MaxCLL/MaxFALL
- **Video signal encoding**: full and narrow range YCbCr per SMPTE BT.601/709/2020
- **High-level image API**: `alwan_image_convert`, `alwan_image_convert_rgba` with stride and premultiplied alpha
- **New color spaces**: LCH(ab), HLC, HSLuv, HPLuv, OkHSL, OkHSV
- **Color Interop Forum**: CIF interoperability helpers and CLF export
- **GPU shader backends**: GLSL, HLSL, Halide header-only backends

### Improved
- **SIMD vectorization**: AVX2 kernels for sRGB↔XYZ, sRGB↔CIELab, sRGB↔Oklab; force-inlined eotf/oetf under SVML
- **U16 SIMD**: AVX2 paths for U16 planar (8 px/batch) and AoS (4 px/batch); previously scalar
- **SIMD scalar arrays**: `alwan_eotf_apply` / `alwan_oetf_apply` gain contiguous SIMD fast paths
- **Test coverage**: 84 test suites (up from 75)

---

## [1.0.0]: 2025

Initial release.
