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

### 1. Runtime / On-Demand Data Loading -- planned for 3.x.y

Not in 2.0.0, and the 2.0.0 behaviour is a stated decision rather than an
oversight: embedding is what removes the data path, the file I/O and the
load-order failures, what lets the table reader bound every access at compile
time, and what makes the GPU backends work at all, since a shader cannot open a
file. `ALWAN_EMBED_DATA=0` `#error`s rather than misbehaving. See
[alwan_decisions.md](alwan_decisions.md).

Desired end state for 3.x.y:

- `ALWAN_EMBED_DATA=0` becomes a supported build
- `runtime_data_root` becomes meaningful
- embedded and runtime data paths expose the same public descriptors and getters,
  so the choice is a build option and not a different API

The last point is the constraint that matters. If the two paths diverge in the
public surface, every caller has to know which build it is talking to, which is
worse than not having the feature.

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

### 3b. A CUDA Backend

`ALWAN_BACKEND` currently selects between C, HLSL, GLSL and Halide, auto-detected
from `__HLSL_VERSION`, `GL_core_profile`, `HALIDE_HALIDERUNTIME_H` or nothing.
CUDA would be a fifth: `ALWAN_BACKEND_CUDA`, detected from `__CUDACC__`.

It is the cheapest of the remaining backends to add, because CUDA is the one that
looks most like the C backend:

- device code is C++ with C semantics, so `alwan_scalar`, the struct-by-value core
  functions and the branchless `ALWAN_SELECT` form all carry over unchanged;
- `__device__` is the only decoration the core functions need, which is what
  `ALWAN_INLINE` already abstracts per backend;
- the ~24 `ALWAN_*` math macros map onto CUDA intrinsics directly, and unlike the
  shader backends CUDA has a real `double`, so this would be the **first GPU
  backend where the `_f64` surface is reachable**. Everything the decisions doc
  says about f64 being C-only is a statement about HLSL and GLSL, not about GPUs
  in general.

What would need deciding rather than just typing:

- **Whether `_f64` on device is actually wanted.** It is available but slow on
  consumer parts; offering it invites people to use it by accident. A separate
  opt-in is probably better than silent availability.
- **Determinism.** `ALWAN_DETERMINISTIC` currently swaps libm for alwan's own
  pow/exp/log to get bit-identical results across compilers. CUDA has its own
  fast-math and FMA contraction rules, so a deterministic CUDA build needs the
  same treatment plus `--fmad=false`, and the det regression suite would need a
  device runner before any bit-exactness claim is made.
- **Where the boundary sits.** The compiled bulk, strided and typed API is
  CPU-side by design. CUDA is the first backend where a device-side bulk layer
  would actually make sense, which is a design question and not a port.

Nothing here is started. It is recorded because the per-pixel core is already
written in the form a CUDA port needs, and that is worth not losing.

### 4. Convenience Queries And Ergonomics

Possible additions:

- richer colorspace metadata helpers
- display/scene/HDR/basic query helpers
- more convenience wrappers around common descriptor-driven workflows

These are ergonomic improvements rather than architectural prerequisites.

### 5. Deterministic Numeric Layer Extension

`src/alwan/core/alwan_deterministic.h` already provides portable polynomial
implementations for `alwan_det_log2/exp2/log/exp/pow` (and a deterministic cube
root), and `ALWAN_LOG2`/`ALWAN_EXP`/`ALWAN_POW` route through them under
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
  ZCAM) where single precision is sufficient, or formally document
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
  real Monte-Carlo **perceptual** gamut volume (Lab/Oklab solid) remains
  future work

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

## Specific Known Gaps

Roadmap themes above are directions. These are concrete, measured gaps with a
known shape, kept apart from [alwan_decisions.md](alwan_decisions.md) because
nothing here is a decision and nothing there is a plan.

## Hunt inverse -- planned for 3.0.9

`alwan_hunt_forward_*` is implemented and matches colour-science to 4.6e-08. The
inverse is not implemented, and is deliberately not in the 2.0.0 scope.

The model is invertible in principle, but the forward runs a chromatic adaptation
whose parameters depend on the adapted signal, so the inverse needs an iterative
solve rather than a closed form. The same shape as the ACES 1.x RedMod10 inverse,
which is a bracketed scalar root find; Hunt's is three-dimensional.

Nothing else in the appearance-model set is missing its inverse.

## TM-30 Rf residual, 0.53 mean against colour-science

Measured over 33 illuminants. CRI and CQS, on the same grid and the same
integration, sit an order of magnitude lower:

| metric | mean | max |
|---|---|---|
| CRI Ra | 0.057 | 0.428 |
| CQS Qa | 0.065 | 0.235 |
| TM-30 Rf | **0.530** | **1.990** |

### Ruled out, with measurements

- **The integration convention**, which `alwan_decisions.md` used to blame. The
  white point of a blackbody over 360-830 nm against 380-780 nm differs by 1e-6
  in xy from 1959 K to 6504 K; colour-science's own Rf at 1 nm against 5 nm
  differs by 0.0000; and CRI and CQS share the grid at 0.06.
- **The blackbody SPD.** alwan's and colour-science's agree to 5.3e-15 in shape
  at 2856 K over the whole 360-830 nm grid.
- **The CCT method.** alwan uses Robertson, colour-science uses Ohno 2013. Mean
  difference 1.9 K, max 7.1 K, and **uncorrelated** with the Rf error (r = -0.07).
  Illuminant A differs by 0.2 K and is still off by 1.24.
- **The Rf formula.** `10 ln(exp((100 - 6.73 dE)/10) + 1)` and the sample-count
  divisor are both correct.

### Where it is

Entirely in the Planckian branch:

| reference branch | n | mean | max |
|---|---|---|---|
| Planckian, CCT < 4000 K | 12 | **0.980** | 1.990 |
| blend, 4000-5000 K | 10 | 0.305 | 0.887 |
| daylight, CCT > 5000 K | 11 | 0.244 | 0.408 |

### The sharpest lead

Self-referential cases do not return 100. Illuminant A **is** a Planckian at
2856 K, so its reference is its own spectrum and every sample's dE should be
zero. colour-science returns exactly 100.000; alwan returns 98.762, which back-
solves to a mean dE of 0.183 in CAM02-UCS units. D65 against its own daylight
reference returns 99.739 rather than 100.

So something adds a small, roughly uniform offset between a spectrum and a
reference built from that same spectrum, and it is about four times larger on the
Planckian branch than the daylight one. That points at the reference construction
or the CIECAM02 / CAM02-UCS path rather than at any of the items ruled out above.

Next step is to dump alwan's per-sample dE for illuminant A and compare against
colour-science's, which will separate "uniform offset" (adaptation or white
point) from "a few samples" (CES data).

## Corpus files carry no chromaticities

Raised by the_flow2 in `alwan_dev/to_alwan.txt`, and it is a corpus decision
rather than a library one, so it sits here until someone makes it.

151 of the 166 SRIC EXR files declare no chromaticities attribute. alwan reads
them as linear AP0 from provenance; a general reader must treat absent
chromaticities as Rec.709, because that is what the OpenEXR specification says.
Both readings are correct in their own scope, and the report established that the
pixel data does not settle it either way.

Stamping the chromaticities attribute on those files would make the corpus
self-describing and remove the disagreement for every downstream reader. That is
a change to the corpus, not to alwan.

## EXR loader, from `alwan_dev/to_alwan.txt`

`image_gen/src/exr_loader.cpp` is a dev tool, not shipped library code. UINT
channels are now rejected rather than converted into a plausible-looking image
of nonsense. What the report raised and remains:

- **FLOAT channels are converted, and values above 65504 saturate to inf.**
  OpenEXR does the conversion, so this is correct up to that ceiling, but the
  ceiling is silent. Worth fixing before the loader is pointed at the corpus for
  the chromaticities question above, since an inf would corrupt exactly the
  out-of-gamut statistics that question turns on.
- **Non-zero data window origin is untested.** The code handles it through
  `(y - dw.min.y)`, but no file in the corpus has overscan, so that path has
  never run. Nice to have. The reporter notes their own loader has the same gap
  for the same reason.
- **Alpha is not read.** 165 of 247 files carry an A channel; only R, G and B
  are routed into the buffer. This is by design for what image_gen does, and is
  recorded here only so the next reader does not take it for an oversight.
- **The reporter offered their loader and their header-survey script.** Worth
  taking up; not yet done.

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

Any new planning should start from the current header rather than from older
drafts that predate those implementations.

---

## Working Checklist

- [ ] Support `ALWAN_EMBED_DATA=0` as a real runtime-data mode (3.x.y)
- [ ] Add richer interop metadata/query APIs
- [ ] Thread data semantics through more public conversion workflows
- [ ] Publish tighter GPU/backend examples around the current bootstrap headers
- [ ] Evaluate a CUDA backend (`ALWAN_BACKEND_CUDA`, first GPU path with real f64)
- [ ] Add ergonomic helpers only where they reduce real call-site boilerplate
- [ ] Extend the deterministic layer to trig/log10 and route the macros
- [ ] Close batch/map and `_map_planar` coverage gaps (CAMs, ZCAM, deltaE, CVD)
- [ ] Add the bulk two-step Zhai 2018 CAT
- [ ] Fill API parity gaps (norm macros, ZCAM `from_ucs`, scalar HSV<->HWB)
- [ ] Add native-f32 metric kernels or document the f64 facades
- [ ] Harden `alwan_create` validation and pin ABI-facing enum values
- [x] ~~Make `gamut_volume_mc` honor sampling, or rename it~~ (renamed to `alwan_gamut_volume` 2026-06-28)
- [ ] Document the undocumented tail surface
- [ ] Hunt inverse (3.0.9)
- [ ] Find TM-30's 0.53 residual: not the integration, the blackbody, or the CCT
- [ ] Stamp chromaticities on the 151 undeclared corpus EXRs
- [ ] EXR loader: exercise a non-zero data window origin
- [ ] Take up the_flow2's loader and header-survey script

---

## Rule For Future Roadmap Updates

When updating this file:

1. check `src/alwan/alwan.h` first
2. move implemented items out of the roadmap
3. keep this file focused on real remaining gaps

That keeps `docs/` aligned with the code instead of preserving stale plans.
