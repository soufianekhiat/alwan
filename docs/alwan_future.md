# Alwan Future

Forward-looking notes for work that is still intentionally outside the current
public release.

This file is not an API reference. It records roadmap themes after accounting
for what already exists in the current repository.

---

## What Already Exists

The current codebase is further along than older roadmap drafts assumed.

Implemented foundations already in this repo include:

- `alwan_platform.h` backend detection and shared math/normalization layer
- bootstrap headers for `C`, `HLSL`, `GLSL`, and `Halide`
- header-only core modules under `src/alwan/core/`
- explicit `_f32` / `_f64` public API in `alwan.h`
- interleaved, planar, and typed-pixel batch frontends
- image-level RGB conversion helpers
- LUT bake / sample / import / export helpers
- CLF export
- interop ID parsing / formatting / enumeration
- integer normalization helpers
- video signal encode / decode helpers

Because of that, future work should focus on the remaining gaps instead of
re-describing the existing foundation.

---

## Remaining Roadmap Themes

### 1. Runtime / On-Demand Data Loading

Still planned, not implemented.

Desired end state:

- `ALWAN_EMBED_DATA=0` becomes a supported build
- `runtime_data_root` becomes meaningful
- embedded and runtime data paths expose the same public descriptors/getters

### 2. Richer Interop Metadata

The current interop layer can already:

- parse IDs
- format IDs
- enumerate IDs

Still missing:

- richer metadata queries such as scene/display/basic/HDR classification
- registry-style helper structs for UI-facing introspection
- clearer data-semantic propagation through conversion entry points

### 3. Backend-Facing Examples And Guidance

The repo already includes:

- `alwan_hlsl.h`
- `alwan_glsl.h`
- `alwan_halide.h`

Future work here is mostly documentation and workflow polish:

- end-to-end shader examples using the current bootstraps
- clearer guidance on which `*_core.h` modules are stable to share across
  backends
- a narrower story around what is public-facing versus experimental

### 4. Convenience Queries And Ergonomics

Possible additions:

- richer colorspace metadata helpers
- display/scene/HDR/basic query helpers
- more convenience wrappers around common descriptor-driven workflows

These are ergonomic improvements, not architectural prerequisites.

### 5. Deterministic Numeric Layer Extension

`src/alwan/core/alwan_deterministic.h` already provides portable polynomial
implementations for `alwan_det_log2/exp2/log/exp/pow` (and a deterministic cube
root), and `ALWAN_LOG2`/`ALWAN_EXP2`/`ALWAN_POW` route through them under
`ALWAN_DETERMINISTIC`. The trig and remaining transcendental macros do not yet
have a deterministic path: `ALWAN_ATAN2/SIN/COS/TAN/TANH/LOG10` still expand to
libm directly in `alwan_platform.h`.

Desired end state:

- add `det_atan2` / `det_sin` / `det_cos` / `det_tan` / `det_tanh` / `det_log10`
- route `ALWAN_ATAN2/SIN/COS/TAN/TANH/LOG10` through them under
  `ALWAN_DETERMINISTIC`

This makes hue/angle channels structurally byte-identical across platforms
(all CAMs, all cylindrical spaces, dE2000/CMC, ACES, Rayleigh, Barten) instead
of relying on libm agreement.

### 6. Batch / Map / Planar Coverage Gaps

Several per-pixel hot paths still lack batch/map frontends, and some carry dead
normalization macros that signal an unfinished map kernel:

- batch/map variants for ZCAM forward/inverse, CAM18sl, CAM20u (consume their
  dead `NORM` macros), IPTch (consume `ALWAN_NORM_IPTCH`),
  `aces1_output_transform`, `gamut_map_advanced`, `gamut_map_xyz_to_rgb`,
  `hdr_gamut_map_jzczhz`, the Cheung2004 / Finlayson2015 CCM applies, and the 10
  scalar-only deltaE metrics (dE-OK, dE-ITP, HyAB, DIN99, ZCAM,
  CAM02/CAM16 LCD/SCD/UCS)
- `_map_planar` parity for the extended-space block, all CAMs, Brettel CVD, and
  YCoCg (SoA callers currently have no planar path for these)

### 7. Performance Hoisting (CAT)

- bulk two-step Zhai 2018 CAT that hoists the white-point-dependent factors out
  of the per-pixel loop, mirroring the existing one-step `xyz_adapt` batch
  design

### 8. API Parity And Dual-Precision Completeness

- normalization macros for UVW (`ALWAN_NORM_UVW`), HSLuv/HPLuv
  (`ALWAN_NORM_HSLUV` / `ALWAN_NORM_HPLUV`) and HLC (`ALWAN_NORM_HLC`) for
  parity with HCL / IHLS / LCH under `ALWAN_NORMALIZE_RANGES`
- ZCAM inverse-UCS (`from_ucs`) to match CAM16's round-trip symmetry
- scalar `alwan_hsv_to_hwb` / `alwan_hwb_to_hsv` (currently map-only)
- native f32 numeric kernels for the metrics implemented as f64-widening
  facades (CRI/CQS/TM30/CIE224/SSI/metamerism, gamut volume/ratio/coverage,
  ZCAM) where single precision is genuinely sufficient — or formally document
  them as intentional f64-internal facades (some already are)
- f32 twin accessor for `alwan_pointer_gamut_boundary` and the other f64-only
  reference-data accessors, for full dual-precision interop

### 9. Robustness And ABI Stability

- defensive validation in `alwan_create` (reject non-zero reserved flags,
  reject mismatched alloc/free callback pairs) instead of silently accepting
  misuse
- append-only ABI policy, or pinned explicit enumerator values, for
  `alwan_rgb_space` and other ABI-facing enums, to allow safe future additions
- ~~rename `gamut_volume_mc` to reflect that it returns an exact determinant~~
  *(done 2026-06-28: renamed to `alwan_gamut_volume`, dead params dropped)*; a
  real Monte-Carlo **perceptual** gamut volume (Lab/Oklab solid) is still a
  genuine future addition

### 10. Documentation Of The Undocumented Tail

A pass over the surface that exists but is not yet documented:

- the 18 named illuminant getters
- ACES interp setters + OCIO mode
- ~20 extended color spaces (signature-level)
- UVW, the convenience/codec spaces
- WCAG/APCA
- standalone BT.2446 / BT.2390 / `reinhard_calibrated`
- the Display Characterization group
- `hdr_gamut_map_jzczhz`

---

## Things This File No Longer Treats As Future Work

These items used to appear in older planning notes, but they are already
present in the current codebase:

- platform abstraction headers
- typed map frontends
- interop ID parsing
- integer normalization helpers
- video range helpers
- LUT/CLF export surface

Any new planning should start from the current header, not from older drafts
that predate those implementations.

---

## Working Checklist

- [ ] Support `ALWAN_EMBED_DATA=0` as a real runtime-data mode
- [ ] Add richer interop metadata/query APIs
- [ ] Thread data semantics through more public conversion workflows
- [ ] Publish tighter GPU/backend examples around the current bootstrap headers
- [ ] Add ergonomic helpers only where they reduce real call-site boilerplate
- [ ] Extend the deterministic layer to trig/log10 and route the macros
- [ ] Close batch/map and `_map_planar` coverage gaps (CAMs, ZCAM, deltaE, CVD)
- [ ] Add the bulk two-step Zhai 2018 CAT
- [ ] Fill API parity gaps (norm macros, ZCAM `from_ucs`, scalar HSV<->HWB)
- [ ] Add native-f32 metric kernels or document the f64 facades
- [ ] Harden `alwan_create` validation and pin ABI-facing enum values
- [x] ~~Make `gamut_volume_mc` honor sampling, or rename it~~ (renamed to `alwan_gamut_volume` 2026-06-28)
- [ ] Document the undocumented tail surface

---

## Rule For Future Roadmap Updates

When updating this file:

1. check `src/alwan/alwan.h` first
2. move implemented items out of the roadmap
3. keep this file focused on real remaining gaps

That keeps `docs/` aligned with the code instead of preserving stale plans.
