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

- **AgX curve polynomial: MEASURED 2026-09-20, and the answer is worth more than
  a citation would have been.** `alwan_agx_curve_v` said its coefficients were
  fitted to "the Blender 4.0 AgX sigmoid" and did not record the LOG RANGE the
  fit assumed. That range is the whole answer, because the curve is fed by
  `alwan_agx_log_encode`, which takes `min_ev` and `max_ev` from the caller.
  Against the AgX this library ships, over a neutral ramp from 2^-10 to 2^4:
  0.311 worst error under AgX's own -12.474..+4.026, and 0.040 under roughly
  -8..+5. It is not a bad curve, it is an unlabelled one, and driven with AgX's
  documented range it puts 18% grey at 0.497 where Blender AgX puts it at 0.180.

  Nothing in the library calls it: it appears exactly once in each core twin, its
  own definition, and every shipped AgX path goes through
  `alwan_view_transform_apply_{T}`, with `ALWAN_VIEW_AGX_BLENDER` being a baked
  57^3 LUT rather than any polynomial. It is reachable only by a shader author
  including the core header, which is why the numbers are now written beside it
  instead of the function being left quietly alone.

  Still a decision, and an owner's one because the core tier is surface a shader
  can call: regenerate the fit with its range recorded, or remove it and let the
  view transform be the only AgX.
- **DONE 2026-09-20** for the colour-space constants, and it was not the same job
  in each case:
  - **DIN99**: the real gap. 32 numbers in a `[4][8]` table read positionally
    under a comment saying `[c1 .. c8]` and nothing else, so checking 105.509
    against DIN 6176 meant reverse-engineering the transform first. The table now
    carries the six equations, what each slot does in them, and why `c5` and `c8`
    look redundant (DIN99 spells the chroma `ln(1+0.045G)/0.045`, the later
    variants spell it `23 ln(1+0.075G)`, and one table covers both). Cited to
    DIN 6176:2001-03, with the b/c/d variants to Cui et al. (2002).
  - **OSA-UCS**: a precise citation already existed in
    `gendata/data/osa_ucs_matrices.py`, MacAdam (1978), and had never crossed
    into the code holding the numbers. It has now, beside the Y0 quadratic and
    the Lambda expression, with the note that suite 24 holds the transform to
    `colour.XYZ_to_OSA_UCS`, so these are checked and not merely cited.
  - **CIE Luv**: nothing to trace. Every constant is exact and definitional,
    4, 9, 15, 3, 116, 16, 13, so the fix is a citation to CIE 15:2004 section 8.2
    and the equations beside them, not a name per number.
  - **Hunter Lab**: already self-documenting. The constants are named macros
    (`ALWAN_HUNTER_KA_XN_REF_`, `ALWAN_HUNTER_KA_D65_` and so on) rather than
    literals in the formula.
  - **ProLab**: nothing matching the description. Its literals are guards
    (`1e-10`) and the projective matrix comes from gendata, so there was no
    uncited constant to cite.
- **DONE 2026-09-21** for the ACES 2.0 Fourier chroma normalisation arrays, and
  it was the OSA-UCS pattern again: the citation already existed in
  `gendata/data/aces2_fourier.py` and had never crossed into the file holding the
  numbers. The two four-term arrays now carry it beside them, with what they
  approximate (the AP1 gamut cusp M against hue angle, which
  `chroma_compress_norm()` divides by, so a wrong term reads as a hue-dependent
  chroma error rather than as anything obviously broken) and the note that four
  terms is the reference implementation's own truncation. Source: the Academy's
  ACES 2.0 Output Transform, `lib/Lib.Academy.OutputTransform.ctl`, cross-checked
  against OpenColorIO's `ACES2/Transform.cpp`.

- **FOUND WHILE DOING IT, and it is an owner question, not a documentation one.**
  `src/alwan/data/` had 35 CSVs at its root with no SOURCE.txt, the earlier
  provenance pass having covered subdirectories only. Almost all are published
  formula constants and are now listed by family. Three are not:
  `agx_blender_lut3d.csv` (14 MB, Blender's AgX baked from a 57^3 cube),
  `agx_default_contrast_lut.csv` and `agx_sb2383_contrast_lut.csv`.

  They are baked from twelve tracked files vendored at
  `alwan_dev/extern/agx/{blender,sobotka,sb2383}`, and **none of those three
  upstreams has a licence file vendored beside it**. The generators name where
  each came from and say nothing about terms. That is a gap rather than a
  finding: it means nobody wrote the terms down, not that the terms are bad. But
  section 4.1 of the release plan is to vendor only under a licence that has been
  recorded, so these three want an answer before the tag.
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

- ~~**Undefined `_f32` entry points.**~~ **VERIFIED CLOSED 2026-09-20**, by the
  2.0.0 review this list predates. Measured rather than re-read: of 596 `_f32`
  entry points declared across the public headers, 596 are exported by the built
  library. Eleven of the thirteen families this entry named were spot-checked by
  name and every one is declared and exported. The other two, `cct_mccamy` and
  `cct_robertson`, are not missing either: they ship as `alwan_cct_mccamy_xy_{T}`
  and `alwan_cct_robertson_xy_{T}`, and the entry was written against a name they
  no longer have.

  Two symbols are declared and not exported, and both are correct:
  `alwan_data_free_f32` / `_f64` sit inside `#if !ALWAN_EMBED_DATA`, a block for
  runtime data loading that is not implemented.

  The stronger result is the one worth keeping: across all 1,986 exports, **zero**
  `_f64` entry points lack an `_f32` twin. The asymmetry this entry describes is
  gone from the whole ABI, not just from the families it listed.

  `alwan_dev/tools/check_declared_exported.py` now gates both of those in the
  tooling workflow, which is the smoke test the plan asked for. A generated .c
  that takes 596 addresses would have needed regenerating with every header
  change; comparing the headers against the generated `.def` needs nothing.

- **Phantom Lab/Oklab convenience API.** `alwan_rgb_to_lab`/`luv`/`oklab`/
  `oklch` + the `lab`/`luv`/`oklab`/`oklch_to_rgb` inverses and
  `alwan_xyz_to_oklch`/`oklch_to_xyz` are declared in BOTH precisions but
  defined in NEITHER (the symbols occur only in `alwan.h`; verified
  `alwan_rgb_to_oklab_f32`/`_f64` at lines 771-772). No bodies, generator,
  tests, docs, or callers. Either remove the declarations or implement them
  (compose `rgb->xyz->Lab/Oklab[->cyl]`, wiring NORM on forward / DENORM on
  inverse).

- ~~**`_map_planar_ex` format-arg order mismatch.**~~ **VERIFIED CLOSED
  2026-09-20.** Not true any more, and measured rather than eyeballed: of 213
  declarations in `alwan.h` taking a pair of `alwan_pixel_format` arguments,
  **zero** name them `(in_fmt, out_fmt)`. 209 are `(out_fmt, in_fmt)` and 4 are
  `(dst_fmt, src_fmt)`, which is the same order under different names and is the
  image-shaped `alwan_image_convert` family, where dst/src reads better beside
  `width, height`. Both functions this entry named as examples,
  `alwan_ycocg_to_rgb_map_planar_ex` and `alwan_rgb_to_hwb_map_planar_ex`, now
  read `(out_fmt, in_fmt)`.

  The danger it described was real and was realised the same day, not in the
  header but in a CALLER: `alwan_dev/det_regression/det_run_regression.c` passed
  the pair the other way round under a comment claiming a legacy order that no
  `_ex` entry point has ever had, and wrote U16 into a `double` buffer while
  reading F64 past the end of a `uint16_t` one. Fixed in alwan_dev 1774d43. The
  lesson is that the header being uniform is what makes a wrong call a bug
  rather than an ambiguity, so keep it uniform.

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

- ~~**Unreachable documented error.**~~ **FIXED 2026-09-20**, by adding the
  determinant check rather than weakening the doc: the header promised
  `ALWAN_E_RANGE` on singular primaries, so the honest move was to return it.

  One correction to this entry, measured. Degenerate primaries did not yield
  NaN/inf; they yielded the **IDENTITY**, which is worse, because NaN propagates
  and announces itself while an identity looks like a usable matrix and quietly
  makes every colour after it wrong. `alwan_mat3_inv_v` fills a singular inverse
  with the identity branchlessly, which is correct for a core that has to compile
  as a shader and cannot return a status.

  Both entry points now test the determinant of the primaries matrix AND of the
  scaled RGB->XYZ derived from it, since a white point can flatten the second on
  its own, and write NOTHING when they refuse: there is no nearest usable answer,
  and returning the identity beside an error invites its use. The threshold is
  `ALWAN_EPSILON_{F32,F64}`, at least as strict as the core's `ALWAN_EPSILON`, so
  a matrix the API accepts is never one the core would quietly fill.

  Suite 03 covers it in both precisions, and proves the ordinary cases still
  pass first: a refusal test that only tested refusals would be satisfied by a
  function that refuses everything. ACES AP0 is among them on purpose, since one
  of its primaries is negative.

- **Sentinel collision on whiteness/yellowness.** *(RESOLVED 2026-09-22, as a
  3.0.0 break)* `alwan_yellowness_astm_e313` / `whiteness_astm_e313` /
  `whiteness_cie2004` returned `-1.0` as the NULL/invalid sentinel, but all
  three are legitimately negative for some inputs. They now return
  `alwan_status` with an out parameter, the form the newer whiteness family
  already had; suite 31 pins that a whiteness of exactly -1 comes back as -1
  with `ALWAN_OK`, and that Y at zero is `ALWAN_E_RANGE` rather than a value.

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

- ~~**`reference_data.c` precision gating absent.**~~ **FIXED 2026-09-21, and the
  entry was pointing at a much larger problem than the file it named.** Its
  literal claim was half stale: `reference-data.md` already documents that these
  lookups are f64-internal facades on purpose. But it predicted a link failure,
  and nobody had ever tested one, because NO job builds
  `-DALWAN_BUILD_PRECISION=f32` or `=f64`. So both were built and every declared
  entry point of the matching precision was linked against each.

  **Both single-precision builds were broken at link time**, while looking fine:
  the static library builds in every configuration, since a `.lib` never links.
  - f32-only: 28 unresolved. Facade modules, which compile their f64 side always,
    called f64 functions from modules that compile theirs only under
    `ALWAN_WITH_F64` (sCAM into sUCS, film into the Jakob 2019 upsampler, exposure
    and camera into the table samplers, the LUT bake into the view transforms).
    And `alwan_color_matrix_get_preset_f32` was gated on BOTH precisions because
    it narrows an f64 table, so it vanished from the one build that wanted only
    it.
  - f64-only: 9 unresolved. Newer modules defined `_f32` entry points with no gate
    at all, so an f64-only build compiled them and they called f32 functions from
    modules that are gated. `alwan_color_checker_reflectance_f32` in this very
    file was one, so the entry was right about the file and wrong about which
    direction.
  - Either: **seven source files were missing from `CMakeLists.txt`**, everything
    from several sessions. Sharpmake globs sources and CMake lists them, so the
    Windows build never noticed, and CI builds with CMake on every other runner.

  All fixed with gates, never stubs, and the dual build that ships is unchanged:
  171 suites, 213,230 checks, `alwan_exports.def` byte-identical. One fix went on
  the caller instead: closing the facade set from the callee side dragged the
  LUT bake into the view transforms, then the ACES 1 output transform, then its
  f64 matrix tables, so in an f32-only build the bake goes through the f32 view
  transform, which is the only one that build has.

  `alwan_dev/tools/check_precision_link.py` and `check_cmake_sources.py` now gate
  both in the tooling workflow. The first was run against a tree with one bug
  deliberately put back, since a gate that has only ever passed proves nothing; it
  failed on exactly the two expected symbols, and in doing so exposed a bug in its
  own error parsing, which is the argument for the exercise.

### Documentation rot

- **Header apply/contract comments.** void apply functions (`lgg_apply`,
  `color_matrix_apply`, `printer_lights_apply`,
  `colour_correct_cheung2004`/`finlayson2015`,
  `white_balance_from_gray`/`apply`) say "Returns ALWAN_OK"; `rayleigh_spd` and
  `hunt_forward` document raw `0`/`-1` instead of `alwan_status`;
  `cat_zhai2018`'s real `E_RANGE`/`E_INVALID` contract is undocumented; the
  `gamut_*_map` comment lists 2 of 8 methods. Align comments with signatures +
  enum.

- ~~**Mojibake in public header.**~~ **VERIFIED CLOSED 2026-09-20.**
  `alwan.h` now contains **zero** bytes above 0x7F, so there is no mojibake left
  to restore. Every line this entry cited reads as ASCII. The docs still carry 69
  non-ASCII bytes across seven files, and those were checked too: they are
  correctly encoded characters that belong there, an em dash, a plus-minus, a
  superscript two in cd/m2 and a Delta, not a character decoded twice.

- ~~**hdr.md arg-order bugs.**~~ **VERIFIED, AND THE REAL ONE FIXED
  2026-09-20.** Two of the three are stale: `maxcll`/`maxfall` are documented
  `(out, rgb, stride, count)`, matching the header, and the worked example passes
  them in that order; `hlg_ootf` is documented `void`, matching the header.

  The third was real and larger than this entry. `maxcll`/`maxfall` were
  documented as returning `int` where the header returns `alwan_status`, and a
  sweep found the same thing in **224 declarations across 19 doc files**. A doc
  that spells the return `int` tells a caller to test against 0 and -1, which is
  not the contract. All 224 were rewritten after checking each name against the
  header, so the three pointer-gamut predicates that genuinely return `int` were
  left alone, and the seventeen exported-but-undeclared illuminant getters, which
  really are plain `int` and are documented as such on purpose, were not touched.

- ~~**`config.runtime_data_root` doc rot.**~~ **VERIFIED, AND A WORSE ONE FIXED
  2026-09-20.** The contradiction this entry names is gone: the struct comment
  agrees with the header that the field is reserved and ignored.

  What was left mattered more on the eve of a 3.0.0 release. Eight places, three
  in `alwan.h` and five in `context.md`, said runtime data loading was "planned
  for alwan 3.0.0". This IS 3.0.0 and it is not implemented, so every one of them
  would have become false the moment the tag was cut. They now say it is not
  implemented and not scheduled. Nothing was moved to a later version instead: an
  unimplemented feature with no work behind it gets no date.

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

- ~~**Header fragmentation.**~~ **RESOLVED 2026-09-22.** Measured before
  touching anything: the convenience families (HSV, HSL, HSY, CMY, YCbCr,
  YcCbcCrc, YCoCg, HWB) had each become one contiguous region, 5178-5500,
  plus their planar forms in the header's deliberate "Planar Map Functions"
  section at the end, which groups every family's planar declarations on
  purpose and stays. Two orphans were real and are moved: the YCoCg inverse
  scalars sat 206 lines below their forward, after the YCbCr legal-range
  maps, and now follow it directly; the four HWB scalars sat in their own
  banner between the HDR metadata and hero-wavelength sampling, 2950 lines
  from the HWB maps, and now open the HWB block beside them. No declaration
  changed, and the build is unaffected by declaration order.

### ABI / coverage

- *(resolved)* **F16 in image_convert.** `image_convert` / `image_convert_rgba`
  dispatch every pixel format through the shared typed helpers
  (`alwan__load3_typed` / `alwan__store3_typed` / `alwan__load1_typed` in
  `map/alwan_map_internal.h`), which handle `ALWAN_PIXEL_F16`: full
  U8/U16/F16/F32/F64 coverage.
