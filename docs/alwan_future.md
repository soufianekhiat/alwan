# Alwan Future

Forward-looking notes for work that is still intentionally outside the current
public release.

This file is not an API reference. It records what is deliberately not
implemented, and what would have to be decided before it could be.

---

## Themes

### 1. Runtime / On-Demand Data Loading

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

### 3c. An OpenCL Backend

The same argument as CUDA, with a different trade. OpenCL C is C99 with a
restricted pointer model, so the struct-by-value cores and `ALWAN_SELECT` carry
over as directly as they do for CUDA, and `double` is available wherever the
device reports `cl_khr_fp64`. What CUDA gets for free and OpenCL does not is a
single vendor's math library: `native_*` versus the precise builtins differ per
implementation, so the deterministic layer matters more here, not less. In
exchange it is the only route that covers AMD, Intel and embedded GPUs from one
source.

What would need deciding:

- **Which precision profile to target.** `cl_khr_fp64` is optional, so the f64
  surface is per-device rather than per-backend. Either the build declares f32
  only, or the API grows a device capability query.
- **How kernels are delivered.** OpenCL compiles from source at runtime, so the
  header-only cores would ship as embedded strings rather than as headers the
  caller includes. That is a packaging decision, not a maths one.
- **Determinism.** The ULP guarantees on the builtins are per-implementation.
  A bit-exactness claim needs `ALWAN_DETERMINISTIC` polynomials plus a device
  runner in the regression harness, exactly as CUDA does.

### 3d. Fixed Point For Embedded Targets

Every core computes in `alwan_scalar`, which is `float` or `double`. A part
without an FPU, or with one too slow to use per pixel, cannot run any of it.
The transforms themselves are not the obstacle: matrix products, the piecewise
transfer functions and the gamut tests are all expressible in Q-format integers,
and the deterministic polynomial layer already replaced libm with evaluations
that a fixed-point core could share the shape of.

What would need deciding:

- **Where the radix point sits, per stage.** Scene-linear values are unbounded
  above, display-encoded values live in [0,1], and chroma is signed. One global
  Q format cannot serve all three, so the type has to carry its scale or the
  API has to fix one per stage.
- **What accuracy is promised.** The current tolerances are stated in ULP
  against f64 references. A fixed-point path needs its own budget, expressed in
  code values rather than ULP, and its own reference rows in the test suite.
- **Which surface is worth it.** The whole library in fixed point is a large
  project. The transfer functions, the RGB matrices and the video-signal
  encode path would cover most embedded uses on their own.

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
  *(done: renamed to `alwan_gamut_volume`, dead params dropped)*; a
  real Monte-Carlo **perceptual** gamut volume (Lab/Oklab solid) remains
  future work

### 10. Documentation Of The Undocumented Tail

Measured, not estimated. Of **411 distinct base operations** on the public
surface (precision and `_map_*` variants folded together), **118 have no entry in
`docs/api/`**.

Two are closed: the table and LUT sampling family now has
[api/tables.md](api/tables.md), and `alwan_gamut_map_spatial` plus the bulk gamut
forms are in [api/gamut.md](api/gamut.md).

The rest cluster, which is the useful part: they are whole areas with no page,
not scattered omissions.

| cluster | count | shape |
|---|---|---|
| `alwan_data_get_illuminant_*` | 9 | direct SPD getters, no page |
| `alwan_delta_e_*_batch` | 7 | batch deltaE, only the scalar forms are documented |
| `alwan_agx_*`, `alwan_jp2499_*` | 9 | params and cube sampling for the AgX family |
| `alwan_cube_*`, `alwan_bake_*`, `alwan_clf_*`, `alwan_lut2d_*` | 17 | the LUT import/export/bake surface |
| `alwan_picture_*` | 5 | picture formation, covered by the topic doc only |
| appearance models | ~10 | `rlab`, `llab`, `kim2009`, `nayatani95`, `hellwig2022`, `atd95` forward/inverse |
| perceptual spaces | ~12 | `hsluv`, `hpluv`, `okhsl`, `okhsv`, `cubehelix`, `iptch` |
| contrast / tone | ~6 | `apca_contrast`, `wcag_contrast_ratio`, `bt2390_eetf`, `bt2446b/c`, `reinhard_calibrated` |

The full list is reproducible:

    python - <<'EOF'
    # families in alwan.h with no mention in docs/api/*.md
    EOF

Rule for closing it: a cluster at a time with its own page, in the house style of
the existing `docs/api/*.md`, rather than one function at a time. A page that
explains why a reader exists is worth more than an entry per symbol.

---

## Specific Known Gaps

The themes above are directions. What follows are concrete, measured gaps with
a known shape, kept apart from [alwan_decisions.md](alwan_decisions.md) because
nothing here is a decision and nothing there is a plan.

## Hunt inverse, planned for 3.0.0

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

This is a corpus decision rather than a library one, so it sits here until
someone makes it.

151 of the 166 SRIC EXR files declare no chromaticities attribute. alwan reads
them as linear AP0 from provenance; a general reader must treat absent
chromaticities as Rec.709, because that is what the OpenEXR specification says.
Both readings are correct in their own scope, and the report established that the
pixel data does not settle it either way.

Stamping the chromaticities attribute on those files would make the corpus
self-describing and remove the disagreement for every downstream reader. That is
a change to the corpus, not to alwan.

## EXR loader

`image_gen/src/exr_loader.cpp` is a dev tool, not shipped library code. UINT
channels are rejected rather than converted into a plausible-looking image of
nonsense. What remains:

- **FLOAT channels are converted, and values above 65504 saturate to inf.**
  OpenEXR does the conversion, so this is correct up to that ceiling, but the
  ceiling is silent. Worth fixing before the loader is pointed at the corpus for
  the chromaticities question above, since an inf would corrupt exactly the
  out-of-gamut statistics that question turns on.
- **Non-zero data window origin is untested.** The code handles it through
  `(y - dw.min.y)`, but no file in the corpus has overscan, so that path has
  never run. Nice to have.
- **Alpha is not read.** 165 of 247 files carry an A channel; only R, G and B
  are routed into the buffer. This is by design for what image_gen does, and is
  recorded here only so the next reader does not take it for an oversight.

---

## Working Checklist

- [ ] Support `ALWAN_EMBED_DATA=0` as a real runtime-data mode
- [ ] Add richer interop metadata/query APIs
- [ ] Thread data semantics through more public conversion workflows
- [ ] Publish tighter GPU/backend examples around the current bootstrap headers
- [ ] Evaluate a CUDA backend (`ALWAN_BACKEND_CUDA`, first GPU path with real f64)
- [ ] Evaluate an OpenCL backend (one source across AMD, Intel and embedded GPUs)
- [ ] Scope a fixed-point path for parts without an FPU (transfer functions, RGB matrices, video encode first)
- [ ] Add ergonomic helpers only where they reduce real call-site boilerplate
- [ ] Extend the deterministic layer to trig/log10 and route the macros
- [ ] Close batch/map and `_map_planar` coverage gaps (CAMs, ZCAM, deltaE, CVD)
- [ ] Add the bulk two-step Zhai 2018 CAT
- [ ] Fill API parity gaps (norm macros, ZCAM `from_ucs`, scalar HSV<->HWB)
- [ ] Add native-f32 metric kernels or document the f64 facades
- [ ] Harden `alwan_create` validation and pin ABI-facing enum values
- [ ] Document the undocumented tail surface
- [ ] Hunt inverse (3.0.0)
- [ ] Find TM-30's 0.53 residual: not the integration, the blackbody, or the CCT
- [ ] Stamp chromaticities on the 151 undeclared corpus EXRs
- [ ] EXR loader: exercise a non-zero data window origin

---

## Keeping this file honest

When updating it:

1. check `src/alwan/alwan.h` first
2. delete anything that has since been implemented
3. keep it to real remaining gaps, not to what was once intended

A file of intentions ages badly; a file of known gaps stays useful.
