# Code Quality Audit Notes

This file is a historical audit summary, not a live authoritative count of
every current violation in the repository.

Older versions of this document tracked large fixed/TODO tables. Those totals
were useful during the audit pass, but they should no longer be read as an
exhaustive current-state metric unless someone recomputes them from the repo.

---

## What Was Verified As Landed

The following broad outcomes are still directly supported by the current source
layout and comments referenced in the earlier audit:

- tolerance handling was consolidated instead of being scattered per test
- several inline matrices were moved to CSV-backed includes
- photopic and scotopic LUTs were moved out of hardcoded source arrays
- many transfer-function and model constants gained source comments

Those changes are visible in the current `src/alwan/` tree and are the lasting
value of the original audit.

---

## Remaining Follow-Ups Worth Keeping Visible

The earlier audit still points to a few categories that remain useful as
follow-up work items:

- document or regenerate the AgX curve polynomial provenance
- add missing literature/spec citations for:
  - CIE Luv constants
  - DIN99 constants
  - Hunter Lab constants
  - ProLab constants
  - OSA-UCS constants
- document the source/provenance of the ACES 2.0 Fourier chroma normalization
  arrays
- clean up remaining hardcoded-reference TODOs in Python `gendata` scripts if
  those scripts return to active use

These are better treated as documentation/provenance tasks than as blockers for
the shipped C API.

---

## How To Read This File

- Use it as a reminder of provenance and audit debt.
- Do not assume old fixed/TODO counts are current.
- Re-run a fresh audit if you need exact present-day statistics.

---

## Suggested Rule For Future Updates

When adding to this file:

1. describe the category of issue
2. link it to a current source location or current follow-up task
3. avoid stale aggregate counts unless they are recomputed in the same change

---

## New Follow-Ups (Audit Pass)

The items below come from a fresh API-surface audit. They are grouped by
severity. Blockers break linking, ABI, or data correctness; the remaining
entries are contract/doc/naming nits that should be cleaned up before the
2.0.0 freeze. Several were spot-verified against `src/alwan/alwan.h`.

### Blockers (link / ABI / data-correctness)

- **Undefined `_f32` entry points.** ~30 public `_f32` functions are declared
  in `alwan.h` but have no definition anywhere, so any f32 caller hits a link
  error. Families: `cct_mccamy`/`robertson`/`hernandez`/`cct_to_xy_kang`/
  `cct_kang_xy`; `xyz_adapt`/`cat_zhai2018`; `hellwig2022_forward`/`inverse`,
  `kim2009_forward`/`inverse`, `llab_forward`; `is_within_pointer_gamut`/
  `spectral_locus_xy`/`dominant_wavelength`/`excitation_purity`/
  `complementary_wavelength`/`gamut_map_advanced`; `rgb_to_xyz`/`xyz_to_rgb`;
  `delta_e_76`/`2000`/`94`/`cmc_f32_batch`; `yellowness_astm_e313`/
  `whiteness_astm_e313`/`whiteness_cie2004`. Fix: emit the entry point from the
  module `.inc` (the ATD95/Nayatani95 pattern) or add documented f32 facades,
  and add an f32 link/smoke test so it cannot regress silently.

- **Phantom Lab/Oklab convenience API.** `alwan_rgb_to_lab`/`luv`/`oklab`/
  `oklch` + the `lab`/`luv`/`oklab`/`oklch_to_rgb` inverses and
  `alwan_xyz_to_oklch`/`oklch_to_xyz` are declared in BOTH precisions but
  defined in NEITHER (the symbols occur only in `alwan.h`; verified
  `alwan_rgb_to_oklab_f32`/`_f64` at lines 771-772). No bodies, generator,
  tests, docs, or callers. Either remove the declarations or implement them
  (compose `rgb->xyz->Lab/Oklab[->cyl]`, wiring NORM on forward / DENORM on
  inverse).

- **`_map_planar_ex` format-arg order mismatch.** 32 of 96 `*_map_planar_ex`
  declarations name their format args `(in_fmt, out_fmt)` (e.g.
  `alwan_ycocg_to_rgb_map_planar_ex` / `alwan_rgb_to_hwb_map_planar_ex` at
  lines 4298-4299) while the delegating macros
  (`ALWAN_PLANAR_EX_DELEGATE_DUAL` / `_DUAL_WHITE`) fix the positional order as
  `(out_fmt, in_fmt)`. Mixed-format callers who trust the header names silently
  swap input/output pixel formats, causing data corruption with no compiler error.
  Unify all names to `(out_fmt, in_fmt)`.

- **`gamut_map_advanced` ignores its space arg.** `alwan_gamut_map_advanced_f64`/
  `_f32` NULL-checks its `alwan_rgb_space_desc*` argument and then ignores it;
  the mapping is hardwired to sRGB/Oklab via `alwan_linear_srgb_to_oklab` +
  `find_cusp`. `gamut.md:129` claims target-space awareness, which is false.
  Honor the space or change the signature + docs.

- **Precision-guard gap for hellwig2022/kim2009/llab.** The f64-entry
  definitions for these three models live OUTSIDE the `#if ALWAN_WITH_F64`
  guard (only the `.inc` include is guarded) yet reference f64-only cores, so
  they fail to compile under `ALWAN_BUILD_ONLY_F32`. Gate consistently and add
  the matching f32 entries.

### Error-contract violations

- **Unreachable documented error.** `alwan_rgb_derive_matrices_{f32,f64}` (doc +
  `color-spaces.md`) promise `ALWAN_E_RANGE` on singular primaries, but the impl
  unconditionally returns `ALWAN_OK` (the core returns matrices by value with no
  singularity signal), so degenerate primaries silently yield NaN/inf. Add a
  determinant check or fix the doc.

- **Sentinel collision on whiteness/yellowness.**
  `alwan_yellowness_astm_e313` / `whiteness_astm_e313` / `whiteness_cie2004`
  return `-1.0` as the NULL/invalid sentinel, but all three are legitimately
  negative for some inputs. Use NaN or document the ambiguity.

- **Misleading `_mc` name + dead params.** *(RESOLVED)*
  `alwan_gamut_volume_mc_{f64,f32}` did `(void)num_samples; (void)seed;` and
  returned exact `|det(rgb_to_xyz)|`, not Monte Carlo. Renamed to
  `alwan_gamut_volume_{f64,f32}`, dropped the two dead params, and rewrote the
  header/`gamut.md` docs to describe the exact-determinant method (with a note
  that a real perceptual-volume MC remains unimplemented).

- **CIE 1964 U\*V\*W\* conversion incorrect.** *(RESOLVED)*
  `alwan_xyz_to_uvw` divided Y by `white.y` before `25*Y^(1/3)-17`, yielding a
  non-standard `W* in [-17,8]` (red `W*=-2.08` vs colour-science `52.27`); the
  inverse meanwhile assumed absolute `[0,100]`, so forward/inverse disagreed by
  ~100x and never round-tripped. Two latent bugs: (1) the spurious `/white.y`
  (now forms the luminance factor in percent scale-invariantly as
  `(Y/Yn)*100`), and (2) the inverse X/Z recovery coefficients (`9*u` and
  `12-3u-20v` -> correct `6*u` and `8-2u-20v`, from inverting CIE 1960 UCS).
  Now matches colour-science to ~2e-8 and round-trips to ~1e-16. W\* gained an
  `ALWAN_NORM_UVW` ([0,100]->[0,1], like Lab L\*); U\*,V\* stay native. Pinned by
  a new colour-science fixture test `05:test_xyz_uvw_d65_roundtrip`. Was
  uncaught because UVW previously had no value-vs-reference or round-trip test.
  **Follow-up:** the formula lives in THREE copies and the first
  pass only fixed `_core.inc`. The remaining two were fixed after a full
  Sharpmake build + test run: (a) the generic header `alwan_*_uvw_v` in
  `core/alwan_colorspace_core.h` (caught by the post-build `check_core_parity.py`
  hook, which CMake test builds skip), and (b) the SIMD bulk path
  `alwan__xyz_to_uvw_kernel` / `_uvw_to_xyz_kernel` in
  `map/alwan_extended_map_kernels.inc` (caught only by `70_planar_map` `_v` vs
  `_map_planar`, since the parity checker does not compare the map kernels).
  All three now agree; Release and Release_Det suites are 95/95.

- **Header-only core not self-contained for sRGB/BT.2020 TF.**
  *(RESOLVED)* `ALWAN_CORE_SRGB_OETF`/`EOTF` (and BT.2020) in the
  `alwan_core_f{32,64}_setup.h` macros call `alwan_fast_pow*` (fast mode) or
  `alwan_det_srgb_*` (det mode) but neither setup header included the file that
  declares them; the lib `.c` TUs happened to pull them in first, but any
  header-only consumer (image_gen, GPU bootstraps, external users) hit C4013
  ("undefined; assuming extern returning int") which under `/WX` truncated the
  float result to int. Fixed by `#include "alwan_fast_pow.h"` / `"alwan_deterministic.h"`
  in the matching branch of each setup header (both are guarded).

### Thread-safety / build-config consistency

- **Non-atomic ACES interp global.** `alwan_set/get_aces_interp` store the
  interp method in a non-atomic file-scope global `g_aces_interp`
  (`alwan_aces_ff.c:23`) with no ctx/sync, contradicting the per-context model.
  Move it into `alwan_ctx`.

- **`reference_data.c` precision gating absent.** `reference-data.md` claims
  single-precision builds define only the matching twin ("excluded precision
  fails at link time"), but `alwan_reference_data.c` has zero precision `#if`
  guards and every f32 entry calls the f64 impl. An f32-only build would leave
  unresolved f64 symbols. Align the file or the doc.

### Documentation rot

- **Header apply/contract comments.** void apply functions (`lgg_apply`,
  `color_matrix_apply`, `printer_lights_apply`,
  `colour_correct_cheung2004`/`finlayson2015`,
  `white_balance_from_gray`/`apply`) say "Returns ALWAN_OK"; `rayleigh_spd` and
  `hunt_forward` document raw `0`/`-1` instead of `alwan_status`;
  `cat_zhai2018`'s real `E_RANGE`/`E_INVALID` contract is undocumented; the
  `gamut_*_map` comment lists 2 of 8 methods. Align comments with signatures +
  enum.

- **Mojibake in public header.** a UTF-8 Delta and a degree sign each decoded as two Latin-1 characters, in "Hue-Preserving Minimum <?>E", "2<?>" and "<?>E*ab"
  (lines 2691-2692, 3001), and an em dash decoded the same way in "APCA <?> WCAG 3.0 draft" (3899); plus
  "half-alwan_f32" (line 71) and "Mapmatrix-vector multiplication" (line 220)
  from a blanket `float`->`alwan_f32` substitution. (Lines 71, 220, and 4694
  verified.) Restore ASCII.

- **hdr.md arg-order bugs.** `maxcll`/`maxfall` are documented as
  `(out, rgb, count, stride)` with a worked example, but the header is
  `(out, rgb, stride, count)` (swapped). `gamma_oetf`/`eotf` documented arg
  order differs entirely from the header. `hlg_ootf` is documented `int` vs
  header `void`. Fix all three.

- **`config.runtime_data_root` doc rot.** `context.md:93` struct comment still
  says "Optional data path for ALWAN_EMBED_DATA=0", contradicting the header
  (line 107: "Reserved... ignored") and the corrected prose later in the same
  doc.

### Naming / header-layout nits

- **Spurious precision suffixes (duplicate identical functions).**
  `half_to_float_f32`/`f64` and `float_to_half_f32`/`f64` (both take
  `alwan_f32` buffers), `lut2d_dimensions_f32`/`f64` (no float arg),
  `interop_parse`/`entry_at_f32`/`f64` (string<->enum). Collapse to single
  un-suffixed functions.

- **Naming-convention drift.** `alwan_xyz_adapt` is the one-step CAT bulk map
  but does not follow the `_map_interleave` naming convention;
  `delta_e_cmc` takes a params struct in scalar form but raw `l,c` scalars in
  the batch/`_ex` forms; `igpgtg_f32_map_planar` names its pointers `i2,i0,i1`
  vs `i0,i1,i2` elsewhere.

- **Header fragmentation.** The convenience-model family is split across
  ~2472-2616 and ~4262-4306; the YCoCg inverse is orphaned at 2618 far from its
  forward; HWB scalars sit at 4014-4017 away from the HWB maps at 2580-2592.
  Consolidate so an audit of one region sees the whole family.

### ABI / coverage

- *(resolved)* **F16 in image_convert.** `image_convert` / `image_convert_rgba`
  dispatch every pixel format through the shared typed helpers
  (`alwan__load3_typed` / `alwan__store3_typed` / `alwan__load1_typed` in
  `map/alwan_map_internal.h`), which handle `ALWAN_PIXEL_F16`: full
  U8/U16/F16/F32/F64 coverage.
