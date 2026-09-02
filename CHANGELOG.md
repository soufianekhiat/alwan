# Changelog

All notable changes to this project will be documented in this file.

---

## [2.0.0]

First public release (tag `v2.0.0`).

The tag sits after a correctness pass over the areas that had no external
ground truth, a pass over the GPU-reachable core,
a build-matrix pass over every combination of build system, precision,
determinism, linkage and toolchain, and a pass making the GPU-portable core
compile under both shader compilers. Several entries below change
published output relative to the pre-release build; they are listed
first because a caller who pinned values against that build will see them.

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
- **Illuminants F2, F7 and F11 had xy data but no way to reach it.**
  `illuminants_xy/f2_xy.csv`, `f7_xy.csv` and `f11_xy.csv` were generated and
  shipped, but never embedded and never given a case in
  `alwan_data_get_illuminant_xy_*`, so `alwan_illuminant_white_point_*`
  returned `ALWAN_E_INVALID` for the whole F series. The three CIE 15 names
  for practical use now resolve; the other nine fluorescents still have no xy
  table. Illuminants resolvable through the enum: 26 -> 29.

### Added

- **`ALWAN_GAMUT_FORM_COMPLETE_PEAK`.** `COMPLETE`'s wider window turned out to
  be a flat 0.945 gain on the carrier with the display peak 9.85 stops over
  mid-grey, so ordinary footage never reached white (scene 16 formed to
  0.945), and its 0.861 purity cap desaturates every saturated pixel by
  13.9% from black up, which reads as a veil inside the display range. The
  new variant keeps `COMPLETE`'s white rail, purity shelf (held at its scene
  positions, +4.3 to +13.8 stops, rather than moving with the window) and
  exact 18% anchor on the family's 16.5-stop window, with the tone exponent
  derived from the pivot's actual position (2.559) instead of assuming a
  symmetric window, and the 0.997 cap. Same operator, same 15-of-15 row.
  `COMPLETE` and `COMPLETE_HEMI_LOOK` are unchanged.

- **Sharpmake reaches the precision axis.** `ALWAN_BUILD_PRECISION` (`both`,
  `f32`, `f64`) is read at generation time and adds the matching define, so the
  same axis is drivable from both build systems instead of requiring an edit to
  `AlwanLib.cs`.

- `alwan_dev/tools/check_gpu_identifiers.py`: reserved-word lint over the
  GPU-reachable lines of every core (101 words probed against dxc, plus the
  GLSL 4.60 and Metal lists); gates the Tooling CI job and runs as a post-build
  step of the library. Reports C types and C headers in the same code without
  gating (`--strict` to gate).
- `alwan_dev/tools/check_gpu_compile.py`: every core compiled as a shader, one
  translation unit each, fast and deterministic, gated on a per-compiler
  expected-clean list; a `gpu-compile` CI job on Windows runs it.
- `docs/backends_limits.md` verification status now carries the measured
  40/43 under each compiler, and why the remaining three cannot compile.

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
- Both shader compilers gate it, via `--compiler dxc|fxc|both`, and both run in
  the Tooling CI job. dxc is Clang-based and accepts C-shaped syntax the legacy
  HLSL grammar never allowed; fxc (Shader Model 5, D3D11) rejects it, so
  checking only dxc hides a whole class of breakage.
- `alwan_dev/tools/check_east_const.py` (with `--fix`): gates qualifiers
  written after the type over `src/alwan/core` and the platform layer, without
  needing a shader compiler. Outside that tier east const is legal C and is not
  flagged. Preprocessor directives are skipped, since reordering
  `# define ALWAN_CONSTEXPR const` would redefine the `const` keyword itself.
- `check_core_parity.py` now compares declarations as well as function bodies.
  Named constants, embedded CSVs and struct definitions in the GPU branch were
  unchecked. A missing declaration is caught by the compilers; what only this
  catches is a constant whose *value* differs between the two copies, which
  compiles cleanly on both sides and silently gives the GPU a different answer
  from the C reference.

### Fixed: no output change

- **A single-precision build could not be linked as a shared library.**
  `ALWAN_BUILD_ONLY_F32` left 219 unresolved symbols and `ALWAN_BUILD_ONLY_F64`
  left 203, because entry points written by hand rather than emitted from the
  dual-precision `.inc` carried no precision gate, the typed `_ex` delegates
  named both precisions' workers unconditionally, and the f64-internal facades
  reached past the extent of `ALWAN_WITH_F64_FACADE`. A static archive never
  resolves a symbol nothing references and an ELF shared object permits
  undefined symbols by default, so only an MSVC DLL failed. Every pixel format
  keeps working in a single-precision build: the typed tile loaders read and
  write all of them through whichever worker was compiled. Output in the
  default dual-precision build is unchanged, and the suite executes the same
  75,034 checks it did before.
- **An f32-only build is no longer meaningfully smaller.** Measured on a static
  MSVC Release build: dual 69.3 MB, f32-only 67.1 MB, f64-only 45.8 MB. Keeping
  the f64-internal facades means an f32 entry point defined to compute in
  double pulls in the f64 spectral, colorspace, CAT, CAM, LUT and Oklab code
  and the tables it reads. `ALWAN_BUILD_ONLY_F32` selects a float-only API
  surface; `ALWAN_BUILD_ONLY_F64` is the one that selects a smaller library.
  See [docs/configuration.md](docs/configuration.md).

- **Core headers compiled as HLSL failed on reserved words.** The cores are
  one source for every backend and the_flow compiles them as HLSL through
  `alwan_hlsl.h`; a local named `out` in `alwan_math_core.h`,
  `alwan_cat_core.h` and `alwan_vision_core.h` was a hard syntax error there and
  took the whole preview shader down. Every reserved word used as a name in
  GPU-reachable core code is now renamed, in the `.h` and its `.inc` twin:
  `out` (also `alwan_hunt_core`, `alwan_extended_core.inc`), `linear` (the
  camera-log and gamma OETF parameters in `alwan_rgb_core`, `alwan_atd95_core`,
  `alwan_jzazbz_core`, `alwan_convenience_core`, `alwan_extended_core.inc`),
  `in` (`alwan_hdr_core`), `matrix` and `input` (`alwan_color_correction_core`,
  `alwan_prolab_core`). 436 identifier sites, C semantics and output unchanged
  (107/107 suites). Two more cores compile under dxc as a result: 32 of 43,
  listed in `alwan_dev/hlsl_regression/cores_dxc_clean.txt`.

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
- **The HLSL headers did not compile under fxc** (Shader Model 5, D3D11), which
  a dxc-only matrix never sees. Type qualifiers were written after the type
  (`alwan_scalar const a = ...`); the legacy grammar requires them before it and
  rejects the east form with `error X3000: syntax error: unexpected token
  'const'`, then a spurious `X3080: function must return a value` from the
  enclosing function failing to parse. Rewritten to the west form at 814 sites
  across 50 files, in both the `.h` GPU branch and the `.inc` template so the
  two stay in lockstep. Value declarations only: pointer forms are untouched,
  since `T *const p` is not `const T *p`.
- `(void)x;` casts in `core/` are not valid HLSL, and were why
  `alwan_hero_wavelength_core.h` failed under fxc while passing under dxc. Now
  routed through `ALWAN_UNUSED(x)`, which is empty on the GPU branches.
- **Eight more cores compile as shaders, 32 of 43 to 40 of 43** under both
  compilers. `atd95` (correlate struct), `hunt` (surround enum and
  viewing-conditions struct), `quality` (Krystek coefficient block) and `view`
  (`alwan_cdl_apply_v`) were each missing a declaration from their GPU branch.
  `extended`, `hellwig2022` and `hdr` used raw pointer out-parameters where the
  house macros exist for exactly that; they now use `ALWAN_PARAM_*` and
  `ALWAN_CORE_PARAM_*`, so C still gets pointers and a shader gets `out`.
  `half` punned float bits through `memcpy` and now goes through
  `alwan_half_asuint` / `_asfloat`, which map to `asuint` / `asfloat` on HLSL
  and `floatBitsToUint` / `uintBitsToFloat` on GLSL.
- `alwan_config.h` included `<stddef.h>`, `<stdint.h>` and `<string.h>`
  unconditionally, so every core reaching it failed on the include line before
  any of its own code was parsed. Those, and the allocator hooks that take
  addresses, are now guarded to the CPU backends; the size and fixed-width
  spellings come from `alwan_types.h` on HLSL and GLSL.
- `alwan_content_light_level_v` is CPU-only: it walks a strided host buffer,
  which a shader has no equivalent for.
- Documentation is plain ASCII, except the Arabic spelling of the project name.
  540 typographic characters (arrows, dashes, Greek, box drawing, check marks)
  replaced with ASCII equivalents across 25 files.

### Known

- TM-30 Rf still deviates from colour-science by 0.53 mean, concentrated in the
  Planckian branch. The integration convention, the blackbody SPD, the CCT
  method and the Rf formula are all ruled out with measurements. Tracked in
  `docs/alwan_future.md`.
- `table`, `lut` and `vision` do not compile as shaders under either compiler.
  Their readers take the table by pointer and a shading language has no pointer
  type; the addressing gate itself is pure integer and float maths and would
  compile, the fetch is what does not. Serving them on GPU needs a resource
  abstraction (`Buffer` / `StructuredBuffer`, or a texture) rather than a syntax
  fix. See `docs/backends_limits.md`.

---

### Foundation

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
  files as well as in-memory numeric output; the harness MD5s the on-disk
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
  Linux/Windows/macOS x x86_64/aarch64 targets and asserts byte/hash
  equality; the `det_run_regression` dump pins the full public API surface
  (~407k lines), with NEON f64 exercised so FMA-contraction or SIMD
  reduction-order drift fails the build.
- **Test coverage**: 102 test suites (up from 84).

---

## [2.0.0-foundation]

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
- **SIMD vectorization**: AVX2 kernels for sRGB<->XYZ, sRGB<->CIELab, sRGB<->Oklab; force-inlined eotf/oetf under SVML
- **U16 SIMD**: AVX2 paths for U16 planar (8 px/batch) and AoS (4 px/batch); previously scalar
- **SIMD scalar arrays**: `alwan_eotf_apply` / `alwan_oetf_apply` gain contiguous SIMD fast paths
- **Test coverage**: 84 test suites (up from 75)

---

## [1.0.0]

Initial release.
