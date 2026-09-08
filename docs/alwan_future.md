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

Done since this list was written: the UVW, HSLuv, HPLuv and HLC normalisation
macros, the scalar `alwan_hsv_to_hwb` / `alwan_hwb_to_hsv`, and
`alwan_zcam_from_ucs`, which gives ZCAM the round-trip symmetry CAM16 had.

- ~~native f32 numeric kernels for the metrics implemented as f64-widening
  facades (CRI/CQS/TM30/CIE224/SSI/metamerism, gamut volume/ratio/coverage,
  ZCAM) where single precision is sufficient, or formally document
  them as intentional f64-internal facades (some already are)~~ *(documented:
  [precision-and-limits.md](precision-and-limits.md) lists every facade and why
  it stays one; the position taken there is that they should not be made native)*
- f32 twin accessor for `alwan_pointer_gamut_boundary` and the other f64-only
  reference-data accessors, for full dual-precision interop

### 9. Robustness And ABI Stability

- ~~defensive validation in `alwan_create` (reject non-zero reserved flags,
  reject mismatched alloc/free callback pairs) instead of silently accepting
  misuse~~ *(done: both return NULL, suite 00 checks them)*
- ~~append-only ABI policy, or pinned explicit enumerator values, for
  `alwan_rgb_space` and other ABI-facing enums, to allow safe future additions~~
  *(done: every enumerator in the 44 public enums carries an explicit value;
  new values are appended, as the transfer-function and ACES 1.x additions were)*
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

## TM-30 Rf residual: found, the CCT was taken on the wrong observer

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

### Found: the CCT was taken on the wrong observer

The lead was that self-referential cases did not return 100. Illuminant A **is** a
Planckian at 2856 K, so its reference is its own spectrum and every sample's dE
should be zero, yet alwan returned 98.762 where colour-science returns 100.0003.

TM-30 uses the CIE 1964 10 degree observer for the rendition samples, which is
correct, and alwan was also using it for the white point handed to the CCT
routine. The Planckian locus and every isotemperature table, Robertson's
included, are defined on the 1931 2 degree observer. Handing a 10 degree
chromaticity to them asks a chart about a point that is not on it:

| built at | read back, 10 degree | read back, 2 degree |
| --- | --- | --- |
| 2000 K | 1966.69 K | 2000.03 K |
| 2856 K | 2789.12 K | 2856.08 K |
| 3500 K | 3427.91 K | 3499.71 K |

So the reference was built as a different Planckian from the source. The error
grew with temperature, which is exactly why the residual was four times larger on
the Planckian branch than the daylight one, and why chasing the formula, the
blackbody and the integration all came back clean: none of them was wrong.

Fixed by computing a second white point on the 2 degree observer for the CCT
alone. A self-referential Planckian is now 0.003 from 100 rather than 1.29, and
illuminant A reads 99.9990 against colour-science's 100.0003, a difference of
0.0013 where it was 1.238.

CRI and CQS were never affected: both use the 2 degree observer throughout, which
is why their residuals sat an order of magnitude lower and made this look like
something in TM-30's formula rather than one line in its setup.

The sweep, re-measured over the 35 illuminants alwan and colour-science both
carry, with the reference generated by `gendata/gen_tm30_reference.py` and walked
by suite 32:

| | mean | max |
|---|---|---|
| before | 0.530 | 1.990 |
| after | **0.0010** | **0.0058** |

The worst remaining case is D55 at 0.0058, which is the size of the difference
between Robertson and Ohno for the CCT and nothing more. TM-30 now sits with CRI
and CQS rather than an order of magnitude above them.

## Block-aware RGB space fit

The experimental fit minimises quantisation error over a whole dataset, which is
the right objective for a texture stored as plain codes and the wrong one for a
texture stored in a block format.

Measured, on five Poly Haven diffuse maps through a real BC1 codec (two RGB565
endpoints per 4x4 block on the block's principal axis, a 2-bit index per texel):
the index interpolation alone accounts for 67 to 94% of BC1's total error, and
the endpoint quantisation for the rest. A fitted space can only touch the
endpoints, so it gained 1 to 18% end to end, and the 18% was the texture with
both the tightest gamut and the lowest index share.

**Built and measured, and the answer is no.** `alwan_rgb_fit_blocks_solve` runs
the codec itself as the objective, on a subsample of tiles, with no surrogate in
between: endpoints on each tile's principal axis, quantised per channel, a
nearest-of-four palette assignment. It converges, and on the five Poly Haven
diffuse maps it does not beat the ordinary fit through a real BC1 codec:

| texture | cloud fit | block-aware fit |
|---|---|---|
| brick | 1% | 1% |
| gravel | 2% | 1% |
| cobble | 6% | 2% |
| denim | 0% | 2% |
| wood | 18% | 15% |

That is a ceiling and not an unconverged simplex: four times the iteration budget
converges at 202 iterations to the identical answer, to every printed digit.

Two things came out of it that are worth keeping. The block-aware answer is
qualitatively different: it holds sRGB's triangle (area 1.00 to 1.02) and moves
only the scale, which is the right instinct for a format whose endpoints already
adapt per tile, where shrinking the global gamut buys nothing. And on a synthetic
texture built to have turning per-tile axes it takes 16% off the 99.9th
percentile while leaving the mean alone, so what it buys, when it buys anything,
is the worst tiles rather than the average one.

**Looked at, the columns are indistinguishable, and that is the result.** A recap
sheet rendering five Poly Haven textures through BC1 in sRGB, in a per-texture
fit and in the pooled fit, each with its error map on one shared scale and a zoom
on the worst tile, shows three compressed columns no reader can tell apart. The
decomposition says why: BC1's index alone, with exact endpoints, already accounts
for most of the error.

| texture | sRGB, codes | index alone | its share |
| --- | --- | --- | --- |
| brick | 7.87 | 7.04 | 89% |
| gravel | 12.87 | 12.16 | 94% |
| cobble | 4.48 | 3.36 | 75% |
| denim | 11.42 | 10.77 | 94% |
| wood | 3.68 | 2.47 | 67% |

So the most any choice of encoding space can remove is 6% to 33% depending on the
texture, and the fit gets 1% to 2% on four of the five. The exception is the one
texture whose error is least index-dominated: wood, at 67%, gives up 15% to a fit
of its own. That is the shape of the whole result. The encoding space is not the
binding constraint in BC1, the 2-bit index is, and no amount of solver work moves
it.

What is left open is whether an objective over *tile geometry* rather than over
tile error would do better: choosing a space so that tiles are collinear, scored
before any quantisation. That is a different and cheaper objective and it has not
been tried. On these numbers it would be competing for at most a tenth of the
error, so it is worth trying only where a texture looks like wood rather than
like gravel.

## Temporal picture formation, exposure adaptation across frames

`alwan_picture_form_global_exp` solves a whole image at once, and
`alwan_picture_form_local_exp_field` already returns the per-pixel exposure field
it converged to. Neither is tractable per frame at real-time rates: the global
solve is an iteration to convergence on the whole frame.

The shape of the idea: run the same solver for a few iterations per frame,
warm-started from the previous frame's exposure field rather than from scratch.
A Jacobi or Gauss-Seidel sweep is cheap and parallel, the field is temporally
coherent because the scene is, and the residual convergence lag then reads as
adaptation: the picture catches up to a lighting change over several frames the
way an eye does. The `_field` variants exist precisely so a caller can hold the
field between frames, so the storage half is already built.

**Built, and the five tests below all pass**, so what follows is a record of what
was asked and what came back rather than a plan.
`alwan_picture_form_local_exp_resume` carries both of the solver's states across
frames, and `iterations` and `base_sweeps` are the per-frame budget. Suite 108 in
alwan_dev is the harness. What was measured:

- **No flicker on a static input.** The field moves 0.222 stops on the first
  frame and 0.0078 by frames 20 to 23. It settles rather than shimmering.
- **Monotone response.** Zero non-monotone steps in 40 frames of a step
  response, at both budgets tried.
- **Convergence gap against the converged answer.** 12 frames at 1, 4 and 16
  iterations land 0.077, 0.017 and 0.0004 stops from the converged field. The
  budget buys convergence, monotonically.
- **Bounded rate.** The largest single-frame move on a settling scene is 0.222
  stops, and the operator's own cap holds the field to plus or minus 3.
- **A step response with a time constant, and the budget steers it.** Four stops
  of light arriving at once, with a fixed pivot: 63% of the travel after 17
  frames at 2 iterations per frame, after 5 at 8. At 60 Hz that is 0.28 s and
  0.08 s, which is the range human light adaptation occupies.

The risk this was written to catch, that the lag is a numerical accident and not
steerable, did not materialise: the budget moves the time constant by a factor of
three and the response stays monotone at both ends.

Two things had to be fixed for that to be true, and both are the sort of thing
that would otherwise have shipped looking plausible. A cold resume was starting
its Jacobi from the caller's zeroed buffer rather than from the frame, so it
answered a different question on frame one. And the penalty homotopy, which ramps
rho from 30 to 3000 to let a cold field find its shape before the constraint is
made hard, was re-running inside every frame: each frame was a different operator
and the field never came to rest. A warm start now holds rho at the value the
ramp ends on, which is what makes the static-scene test pass.

**The step size was the whole cost, and it was the wrong size.** The field update
was gradient descent with a fixed step of 0.02, and the smooth half of its
gradient is the anchor `2*(e - e0)`, so each iteration removed 4% of the
remaining error and the solve needed tens of them. That step was not chosen for
the anchor. It was chosen for the order penalty, whose curvature is
`2*rho*Jc^2` with rho at 3000, three orders of magnitude away, and only at the
pixels where its hinge is active. One step size for both meant either instability
on the stiff term or a crawl on the smooth one, and 0.02 chose the crawl for
every pixel in the picture to protect the few under an active hinge.

Scaling the step by the local diagonal curvature fixes it, because the whole
gradient is scaled and the fixed point does not move. `relax * g / diag(H)` is
the exact move for a pixel the hinge is not touching, so the anchor is satisfied
in one iteration rather than sixty, and a hinge-bound pixel is stepped small
automatically. Measured on a 4-stop step from a settled field, frames to come
within 0.02 stops rms of the answer:

| per-frame budget | plain step | momentum 0.9 | relax 1.0 |
| --- | --- | --- | --- |
| 1 iteration | 102 | 72 | 13 |
| 2 iterations | 51 | 36 | 7 |
| 8 iterations | 13 | 9 | 2 |

One iteration a frame with the scaled step matches eight with the plain one, and
all rows settle on the same field to four decimals.

**Momentum was asked for and is the weaker answer.** A heavy ball at a fixed step
has an asymptotic rate floor of `sqrt(mu)`, so 0.9 turns 0.96 per iteration into
0.95, worth about 1.4x, and it rings getting there. At 0.95 it never settles at
all: 300 frames later it is still 0.25 stops rms out. It is available, carried
across frames like the field, and it is not the thing to reach for. On top of the
scaled step it is slightly worse than the scaled step alone.

At relax 1.0 the remaining limit is the deliberate one. The per-iteration cap of
0.15 stops means a 1.25-stop move needs at least 9 iterations, which is most of
the 13 measured, so the solve is now bounded by the rail that stops a frame
jumping rather than by the arithmetic.

**What is still open** is the same thing as before, and the accelerator sharpens
rather than closes it: the time constant is in frames, not seconds. What has
changed is that it is no longer forced to equal the budget. Converging in two
frames at one iteration each leaves the adaptation rate free to be chosen. A
small relax does slow the picture, but it slows each pixel by its own curvature,
so hinge-bound pixels lag free ones and the field arrives unevenly, which shows
as speckle in the field maps at relax 0.12. The uniform way is to converge with
relax near 1 and filter the returned field toward the solved one, which is a
caller-side line today and wants to be a rate limiter in stops per second in the
library. That is also where the asymmetry that matters perceptually belongs:
light adaptation in seconds, dark adaptation far slower.

**Closed.** `alwan_picture_form_local_exp_lag` filters the solved field's mean
toward a level the caller carries, by `1 - exp(-dt / tau)` of the gap per frame,
with `tau_light` for a field that is falling (the light went up) and `tau_dark`
for one that is rising, and puts the difference back as a uniform offset; the
structure is the frame's own and does not move. `alwan_picture_form_local_exp_apply`
forms the picture from whatever field the caller ends up with, and is the solve's
own last step bit for bit. Suite 108 holds the mean to the formula and the
structure to zero at 1e-12, and image_gen's `lag=` runs on the two calls.

### Running it on real footage: three halos, and only one of them temporal

Put through 2048x858 ACES frames from the ASC StEM2 delivery, the operator draws
a visible halo around high-contrast edges. Taking it apart needed three controls
that now exist in image_gen: the formation operator with no exposure field at
all, a field solved cold on each frame carrying nothing, and the ordinary
resumed solve. Three points place an artefact; one does not.

**The formation operator is not involved.** With no field there is no halo, at
any edge, in any of the shots tried.

**A sharp moving edge trails, and that is the resume.** The field may only move
0.15 stops per iteration, so at one iteration a frame it lags. Correlating the
field against the scene carrier at the current frame and at earlier ones says how
far:

| iterations a frame | the field matches |
| --- | --- |
| 1 | 3 frames back |
| 2 | 2 frames back |
| 4 | 1 frame back |
| 8 | the current frame |

**A defocused edge halos, and that is not temporal at all.** A cold solve, with
no state carried whatsoever, still puts the field 1.5 stops above the background
across the blur of an out-of-focus foreground; the resumed version reads 1.7, so
the resume adds about a seventh and the rest is there in a single frame. The
cause is that the base's gate has nothing to cut at. Measured on the cabin shot's
foreground window frame, the carrier falls 0.69 stops across the blur but only
**0.069 stops between adjacent pixels**, against 3.55 for a sharp edge in the
same frame. Perona-Malik conductance was tried on that stencil and changed the
result by nothing at four decimal places, for the obvious reason: at one pixel a
defocused edge is texture. Cutting it needs a measure taken over a distance
comparable to the blur, which means a multi-scale base rather than a one-pixel
one, and that is real work rather than a constant.

**The field is unstable frame to frame independent of any of this, and the resume
is what damps it.** Over a nearly static second:

| | field movement, stops rms |
| --- | --- |
| cold solve, every frame independent | 0.145 |
| resumed, 1 iteration | 0.071 |
| resumed, 8 iterations | 0.123 |

So converging harder moves the field toward the per-frame answer, and the
per-frame answer flickers. Raising the budget to fix the trail made the flicker
worse in the same shot.

**Diagnosed: the flicker enters on the input side, and the solve is well posed.**
Suite 108 rebuilds the situation with a known input, the same scene every frame
with noise redrawn per frame, and measures the frame-to-frame rms movement of
four things: the evidence base, the target it hands the solver, a cold solve at
16 and at 200 iterations, and the resumed solve at 16.

| scene, noise | base | target | cold 16 | cold 200 | resumed 16 |
| --- | --- | --- | --- | --- | --- |
| structured, 2% white | 0.0051 | 0.0028 | 0.0042 | 0.0042 | 0.0057 |
| one edge 0 to 3 stops, 2% white | 0.0033 to 0.0050 | 0.0020 to 0.0030 | 0.0020 to 0.0038 | 0.0020 to 0.0046 | 0.0013 to 0.0050 |
| structured, 5% grain | 0.0706 | 0.0366 | 0.0367 | 0.0366 | 0.0210 |
| one edge 1.5 stops, 5% grain | 0.0695 | 0.0417 | 0.0417 | 0.0417 | 0.0228 |

White noise does nothing: everything sits at a few thousandths of a stop and the
cold solve moves at most 1.5 times what its target moves, at every edge contrast
across the gate's thresholds. Grain, white noise blurred to a few pixels, which
is what debayered and compressed footage carries, is a different matter: the
evidence base follows it at 0.07 stops rms, the target at 0.04, and the cold
solve reproduces its target to four decimals whether it runs 16 iterations or
200. There is nothing in the solve that flickers; it renders the target it is
given, and the target moves because the base's Jacobi is a regional consensus
with a weak data anchor, which passes low spatial frequencies by design. The
resumed solve halves the movement because it carries the base between frames.

That is what the 0.145 on footage was: low-frequency change in the frames, grain
and compression and small motion, read faithfully by a per-frame base. The remedy
is temporal and belongs on the base, a filter on `base_io` that a cut resets,
not in the solver; the level lag above takes the uniform part of it already. It
is not done because on a moving picture a temporal filter on the base trades this
flicker for a trail, the same trade the halo work went through, and choosing
where to sit on it needs footage in front of it.

**The one lever that helps all three is the adaptation strength**, and it costs
the effect in proportion:

| strength | halo step | flicker, rms | adaptation swing |
| --- | --- | --- | --- |
| 0.60 | 1.72 stops | 0.071 | 0.81 stops |
| 0.30 | 1.30 | 0.060 | 0.60 |
| 0.15 | 0.65 | 0.034 | 0.36 |

At 0.6 the anchor saturates its one-stop cap across the blur, so the transition
becomes a block with a hard boundary that shifts with the shot. At 0.15 nothing
saturates. That cap is worth questioning on its own: a scene that falls 4.9 stops
gets 0.8 back.

**A level lag exists, in the tool rather than the library.** `--exp-video
lag=<seconds>` converges the field every frame and filters only its mean with a
first-order response, putting the difference back as a uniform offset before the
picture is formed. That is the separation the section above asks for, and it
belongs in the library once the flicker above is understood, because the two
interact: the lag smooths the level and does nothing for structure.

## ACES 1.x HDR: the embedded spline targeted a 10-nit mid point; now the SSTS

The sweep on the light-saber frame reported 10.3% of pixels differing from OCIO
by more than 4 PQ codes on `ALWAN_ACES1_OUT_REC2020_1000NIT_PQ`. Chasing it
turned up two separate things, and neither is the one the report suggested.

**The transform is right, under one of its two paths.** With
`ALWAN_ACES_INTERP_OCIO`, which is what the sweep uses, alwan agrees with OCIO's
"ACES 1.1 - HDR Video (1000 nits & Rec.2020 lim)" view to **0.05 of a 10-bit
code** on everything away from black, mid-grey included: 0.18 lands at PQ
0.332589, which is 15.00 cd/m2 exactly, the mid point the ODT is named for. The
matrix chain AP1 to Rec.2020 through the D60 to D65 Bradford CAT reproduces one
built from the published ACES primaries to 2.2e-16, so the matrices are not
involved either.

**The reported disagreement is PQ steepness near black.** What differs is
channels whose linear value is about a thousandth of peak or less: for AP0 blue,
OCIO's Rec.2020 red is +1.1e-3 and alwan's is at or below zero. In linear light
that is nothing, one part in a thousand of a channel that is already black. PQ
near zero is steep enough to turn it into 66 code values, which is how a
difference invisible on any display becomes 10% of pixels over a threshold. The
lesson is about the metric: a PQ code count is the wrong yardstick below a few
cd/m2, and the sweep should compare in linear or in a perceptual space there.

**The real defect is elsewhere and was not what was being measured.** Under the
DEFAULT `ALWAN_ACES_INTERP_BSPLINE`, the same enum puts 0.18 at PQ 0.299698,
which is 10.00 cd/m2 exactly. The embedded c9 breakpoints say so:
`aces1_c9_1000nit_breakpoints.csv` carries yMid = 10, as do the 2000 and 4000
nit files. That is the older ACES HDR ODT; every current ACES config ships the
15-nit one, and OCIO offers no 10-nit view to validate against. So the same enum
value is two different output transforms depending on a setting documented as a
choice of how to evaluate a curve, and they sit **34 codes apart on mid-grey**,
which is half a stop of luminance on every pixel of every frame.

Suite 56 now pins all of it, including the 34-code gap, so a change is noticed
rather than absorbed.

**The plan this section used to carry was wrong, and the CTL says why.** It said
to regenerate the c9 coefficients for the 15-nit ODTs. There are none. The two
transforms are not two tunings of one spline, they are different constructions
from different ACES revisions:

| | transform | spline | mid |
| --- | --- | --- | --- |
| ACES 1.0.3 | `ODT.Academy.Rec2020_ST2084_1000nits` | `segmented_spline_c9_fwd`, 10 coefficients a side | 10 cd/m2 |
| ACES 1.1 to 1.3 | `RRTODT.Academy.Rec2020_1000nits_15nits_ST2084` | SSTS, 6 coefficients a side, built from Y_MIN/Y_MID/Y_MAX | 15 cd/m2 |

alwan's embedded c9 tables are the 1.0.3 transform, faithfully. The 15-nit one
does not have a coefficient table to copy: `ACESlib.SSTS.ctl` constructs its
knots at run time from the three luminances, so there is nothing to regenerate
and no fitting to do.

So `alwan_set_aces_interp` was not choosing how to evaluate a curve. It was
choosing between two ACES revisions, and the enum name said neither.

**Closed.** The SSTS is implemented. `gendata/data/aces1_ssts.py` builds the
knots from Y_MIN, Y_MID and Y_MAX exactly as `init_TsParams` and the RRTODT's
exposure shift do, and `alwan_aces_ff.c` evaluates the spline forward and
inverse, with the RRTODT's clip to the limiting primaries before the CAT and its
stretched black. `ALWAN_ACES1_OUT_REC2020_{1000,2000,4000}NIT_PQ` are the 1.1 to
1.3 transforms under every setting; the 1.0.3 ODTs keep their C9 tables under
`ALWAN_ACES1_OUT_REC2020_*NIT_PQ_V103`. Measured: the evaluator matches the
transcription to 1.3e-13 over three curves, 0.18 lands on 15.000000000 cd/m2 for
each peak and 10.000000000 for the V103 outputs, the default path sits 0.05 of a
PQ code from OCIO's ACES 1.1 view away from black, and all fifteen outputs
round-trip at 1.3e-11. The 34-code gap suite 56 used to pin is now the stated
difference between two named outputs.

One thing the CTL does that is worth knowing: `limit_to_primaries` clips in the
limiting primaries before the D60 to D65 adaptation, so a D60 grey within a few
percent of peak, which is not grey in Rec.2020 coordinates, loses a little red
there. It is 1.3e-5 at peak, it is what the reference does, and the tone-scale
test leaves the top five percent out for that reason.

## RGB-space transfer functions: audited against a reference, and fixed

`alwan_rgb_space_get_tfs` used a hand-written switch naming 20 of the 104 RGB
spaces and refused the rest, ACEScct and every camera log space among them. It
now delegates to the descriptor table, which is generated with one row per enum
value, so nothing is refused and there is one source rather than two.

That exposed the larger problem: neither table had ever been checked against
anything. Evaluating the library's own OETF for every space and comparing against
colour-science 0.4.6 found **40 of the 65 matched spaces disagreeing**. Twenty-one
of those are a convention rather than an error, and it is worth writing down: a
"wide gamut" entry such as `ARRI_WIDE_GAMUT_3` or `S_GAMUT3` is primaries only
and is linear here, with the curve in its own entry (`ARRI_LOGC3`, `S_LOG3`),
while colour-science bundles the two. Any comparison against it will show every
such row differing by design.

Seventeen were real and are fixed, sourced from
`RGB_COLOURSPACES[...].cctf_encoding`:

| correction | spaces |
| --- | --- |
| linear to gamma 2.2 | CIE RGB, Adobe Wide Gamut, Best, Beta, Don 4, Ekta Space PS5, Max, Russell, Xtreme |
| BT.709 to gamma 2.2 | SMPTE-C, NTSC 1987 |
| BT.709 to gamma 2.8 | NTSC 1953, PAL/SECAM, BT.470-525, BT.470-625 |
| sRGB to gamma 2.6 | P3-D65 |
| BT.709 to linear | EBU Tech. 3213-E, which defines primaries only |

Adobe Wide Gamut is gamma 2.19921875 and first went to GAMMA22, exact to 2e-4; it
has its own value now, below.

**Nine more were wrong because the library had no curve for them.** Seven were
added: gamma 1.8, the ROMM encoding, RIMM, ERIMM, CIE 1976 lightness, the SMPTE
240M OETF, and the Adobe 563/256 gamma. That last one came out of tightening the
test rather than the audit: Adobe RGB (1998) and Adobe Wide Gamut are 2.19921875
and were both carried as 2.2, wrong by 4.2e-4.

| space | now uses |
| --- | --- |
| Apple RGB, ColorMatch RGB | gamma 1.8 |
| ProPhoto RGB, ROMM RGB | the ROMM curve, 1.8 with a linear toe below 1/512 |
| RIMM RGB | the RIMM curve |
| ERIMM RGB | the ERIMM log encoding, 0.001 to 316.2 |
| ECI RGB v2 | CIE 1976 lightness |
| SMPTE 240M | its own OETF rather than BT.709 |
| Adobe RGB (1998), Adobe Wide Gamut | gamma 563/256 |

DCDM XYZ turned out not to be a defect. alwan normalises to the 48 cd/m2
reference white where colour-science works in absolute cd/m2, and at 48 the two
agree exactly.

**Every space colour-science knows now matches it to 1e-6.** The wide-gamut
entries stay excluded by design, and the new enum values are appended so nothing
renumbers. `gendata/gen_rgb_space_tf_reference.py` emits the reference and suite
43 walks it, so the table cannot drift again without a test noticing.

That left 38 spaces with no `RGB_COLOURSPACES` counterpart, and they are covered
another way now. The ten log-curve entries (`ARRI_LOGC3`, `S_LOG3`, `V_LOG`, and
so on) are checked against the colour-science gamut that bundles the same curve,
the mirror of the exclusion above. S-Log, Canon Log and REDLog go against the
curve functions directly, `REC1886_REC709` against the BT.709 OETF, `REC2100_PQ`
and `DISPLAY_P3_HDR` against ST 2084 in cd/m2, which is alwan's PQ convention
too, and `REC2100_HLG` against BT.2100. The fourteen `LINEAR_*` entries are
checked against the identity and the composites against the gamma in their name.

Two of those were wrong. `GAMMA18_REC709` was recorded as linear, and
`DAVINCI_INTERMEDIATE` was recorded as linear because the library had no DaVinci
Intermediate curve; it has one now (`ALWAN_TF_DAVINCI_INTERMEDIATE`, the
published log with a linear toe below 0.00262409). A log-encoded delivery read as
scene light is the kind of error that survives a long time, since the picture is
merely flat rather than broken.

79 of the 104 spaces are now in the reference, all within 8e-16. The 25 outside
it are the 24 primaries-only entries and DCDM XYZ for the white convention above.

`BLACKMAGIC_FILM` needed its own answer. Blackmagic has never published the Gen 4
curve; what alwan carries is Nathan Vegdahl's "Broadcast Film Gen 4" fit against
LUTs extracted from Resolve (psychopath.io, 2022, relative error at most 1.7e-5),
constant for constant. The reference re-implements that fit from the post, so it
pins the transcription rather than the truth, and the header now says where the
numbers came from. The space's comment said "Film Generation 1-4"; the post shows
those are five different curves, and alwan has the Broadcast one.

## Corpus files carried no chromaticities: stamped

151 of the 166 SRIC EXR files declared no chromaticities attribute, and a
general reader must treat absent chromaticities as Rec.709, because that is what
the OpenEXR specification says. The collection's README says every file is
FilmLight E-Gamut linear, which is what image_gen's loader converts it as; an
earlier version of this note said AP0, and that was wrong.

The 15 files that did declare chromaticities, all ACES VWG frames, said AP0,
which contradicts the README. Matching them against the original ACES VWG sample
frames settled it: `SRIC_vwg_output-transforms.01015` is frame 0065 at 0.996
correlation and reads 0.0167 stops rms as E-Gamut against 0.249 as AP0, and
`gamut-mapping.01014` is frame 0061 at 0.962 with 0.39 against 1.08. The AP0
attribute was carried over from the source files, not a description of the
pixels.

All 166 files now carry E-Gamut chromaticities, from alwan's own table for
`ALWAN_RGB_SPACE_FILMLIGHT_E_GAMUT`. The files are DWAA, which is lossy, so
`image_gen --exr-stamp` copies each chunk's packed bytes as they are and writes
only a new header; `--exr-diff` decodes old and new through the same loader and
reports zero differing values on every file, and every other attribute survives,
the RED files' 118 nuke/r3d/* names included. `tools/stamp_chromaticities.py`
drives it. The corpus is a submodule of alwan_dev, so the stamping lives in its
working tree: `git -C extern/SRIC checkout -- exr` undoes it and the tool redoes
it. Making it permanent means a fork of the collection with the stamped files,
which is the owner's call.

## EXR loader

`image_gen/src/exr_loader.cpp` is a dev tool, not shipped library code. UINT
channels are rejected rather than converted into a plausible-looking image of
nonsense. What remains:

- **FLOAT channels are decoded as FLOAT.** The loader once asked OpenEXR for
  half on the way out, which saturated any FLOAT value above 65504 to inf,
  silently. It asks for float now; HALF widens losslessly and FLOAT passes
  through. `tools/exr_window_check.py` writes FLOAT files with values up to
  191900 and reads them back exact.
- **Non-zero data window origin is exercised.** No file in the corpus has
  overscan, so the `(y - dw.min.y)` path had never run. `tools/exr_window_check.py`
  writes a known pattern under four origins, including a negative one and one at
  (1000, 2000), five compressions and both pixel types, and loads each through
  `image_gen --exr-probe`: 40 of 40 pixel-exact, partial last chunk included.
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
- [x] Fill API parity gaps: norm macros and scalar HSV<->HWB were already in; ZCAM `from_ucs` added
- [x] The f64 facades are documented, each with its reason, in precision-and-limits.md; they stay facades by design
- [x] Harden `alwan_create` validation (non-zero flags and a half allocator pair return NULL); the 44 public enums were already fully pinned
- [ ] Document the undocumented tail surface
- [ ] Hunt inverse (3.0.0)
- [x] TM-30 residual: the CCT was read on the 10 degree observer against a 2 degree locus; sweep now 0.0010 mean
- [x] RGB-space transfer functions: audited against colour-science, 24 rows corrected, 7 curves added
- [x] RGB-space transfer functions: 79 of 104 in the reference; found GAMMA18_REC709 and DaVinci Intermediate recorded as linear, added the DaVinci curve
- [x] RGB-space transfer functions: Blackmagic Film Gen 4 is the psychopath.io fit to Resolve; pinned to it and said so in the header
- [x] Exposure adaptation: a time constant in seconds on the level, tau_light and tau_dark apart (alwan_picture_form_local_exp_lag)
- [x] Exposure field: the cold per-frame flicker is input side, the base following grain; the solve reproduces its target to four decimals (suite 108)
- [ ] Exposure base: a temporal filter on base_io, reset at a cut, for grain and compression noise; needs footage to place the flicker/trail trade
- [ ] Exposure field: a multi-scale gate, so a defocused edge can be cut at all
- [ ] Exposure anchor: the one-stop cap gives back 0.8 of a 4.9-stop change; is that the right number
- [x] ACES 1.x HDR: the SSTS is implemented; the 1.1 to 1.3 outputs are the default, the 1.0.3 ODTs are _V103
- [x] Stamp chromaticities on the corpus EXRs: all 166 SRIC files are E-Gamut now, pixels untouched; the 15 that said AP0 were stale
- [x] EXR loader: non-zero data window origin, 40 of 40 files pixel-exact in tools/exr_window_check.py
- [x] Temporal picture formation: warm-started iterations per frame as exposure adaptation
- [x] Temporal picture formation: a time constant in seconds in the library, and the picture from a chosen field (alwan_picture_form_local_exp_apply)
- [x] Block-aware RGB space fit (built; measured no better than the cloud fit on real textures)

---

## Keeping this file honest

When updating it:

1. check `src/alwan/alwan.h` first
2. delete anything that has since been implemented
3. keep it to real remaining gaps, not to what was once intended

A file of intentions ages badly; a file of known gaps stays useful.
