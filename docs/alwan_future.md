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

### 3b. CUDA: done, and not as a fifth backend

Shipped. A CUDA kernel calls the per-pixel core directly, the same way an HLSL
shader does. `ALWAN_CUDA` is 1 under nvcc.

This section planned it as `ALWAN_BACKEND_CUDA`, a fifth id detected from
`__CUDACC__`. That turned out to be the wrong shape. nvcc is a C++ compiler with
a real `double`, so it compiles the **C** emission path unchanged, dual-pass
`.inc` and all, and a kernel gets both the `_f32` and the `_f64` core rather than
a single-precision subset. Minting a separate id would only have made every
`ALWAN_BACKEND == ALWAN_BACKEND_C` guard in the tree wrong about a CUDA build.
What it needed instead was qualifiers: `__host__ __device__` on the header-only
functions, and a storage class for the constant tables that works in both of
nvcc's compilation passes.

The three questions this section raised, answered:

- **Is `_f64` on device wanted?** It is there, and it is not silent: a caller
  writes `alwan_xyz_to_oklab_f64_v` deliberately. No separate opt-in was needed
  because the name already says which one you asked for.
- **Determinism.** `--fmad=false` is required, as predicted, because the
  `#pragma STDC FP_CONTRACT OFF` in the deterministic header is a C compiler
  pragma that nvcc's device compiler ignores. The device runner this section
  asked for exists (`alwan_dev/cuda_regression/`), and with it a deterministic
  build is **bit-exact against the CPU** on every kernel measured, both
  precisions, every sample. Getting there closed a hole that was not
  CUDA-specific: the deterministic routing lived in `alwan_math.h`, which a
  core-only translation unit never includes.
- **Where the boundary sits.** Unchanged and still a design question. The
  compiled bulk, strided and typed API stays CPU-side; the backend is the core,
  not a device-side bulk layer.

### 3c. OpenCL: done, as a real backend id

Shipped as `ALWAN_BACKEND` 4. Bootstrap with `alwan_opencl.h`, then the core
headers, and call the core from your own `__kernel`. All 43 core headers build,
and six conversions run on an RTX 3060 through the NVIDIA runtime and agree with
the compiled C library to the f32 rounding scale of each
(`alwan_dev/opencl_regression/`).

Unlike CUDA this **is** a backend id, because OpenCL C is not C: program scope
needs address spaces, there is no C library, and `double` is an extension. So it
takes the single-pass GPU branch HLSL and GLSL take.

The three questions, answered:

- **Which precision profile.** f32 by default, `ALWAN_OPENCL_FP64=1` for
  `double`, and the device capability query stays the caller's: a program that
  enables `cl_khr_fp64` on a device without it fails to build, which is the
  right failure and not one a header can pre-empt.
- **How kernels are delivered.** As headers the caller includes, not embedded
  strings. OpenCL compiles from source at runtime, so the runtime IS the
  compiler and `-I` at `clBuildProgram` does the rest. That also makes
  `clBuildProgram` the compile check, since there is no offline OpenCL C
  compiler to invoke.
- **Determinism.** Not claimed yet, and the reason is worth stating: the
  deterministic layer is reachable here in principle, but nothing has run the
  device against a reference to prove it, and this document's own standard is
  that a bit-exactness claim needs a device runner behind it. The f32 tolerance
  above is a two-implementation comparison, not a determinism result.

What actually cost the time was none of the three. `ALWAN_CONSTEXPR` is
`__global const`, not `__constant`, and the reflex choice fails structurally:
**`__constant` is not part of the generic address space**, in 1.2 or in 2.0, so a
`__constant T *` cannot reach a function that also takes runtime data.
`alwan_table_core`'s samplers are called both with a compile-time table, the
eleven Machado CVD matrices selected by severity, and with a caller's own LUT,
so under `__constant` one of the two cannot compile. `__global const` is
generic-compatible and needs program-scope globals, hence `-cl-std=CL2.0`.

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

**Done, 2026-09-08.** `core/alwan_deterministic.h` now covers the angle family
alongside the original `log2 / exp2 / log / exp / pow / cbrt`: `sin`, `cos`,
`tan`, `tanh`, `atan`, `atan2`, `acos` and `log10`, with `alwan_math.h` routing
every corresponding macro.

Three polynomials carry all eight. Parity is factored out, so `sin(r) = r*P(r*r)`
and `atan(s) = s*R(s*s)` are exactly zero at the origin rather than approximately
so, and `tan`, `atan2`, `acos`, `log10` and `tanh` are exact identities on top of
those and of the existing `log2`/`exp2` pair. Coefficients come from a Remez
minimax fit at 60 digits (`alwan_dev/gendata/gen_trig_minimax.wls`), which a
float64 Chebyshev fit could not match: that plateaus near 1e-15 because the fit
and the basis conversion both round at double precision, and raising the degree
made it worse. The committed fits sit at 1.9e-21, 1.8e-23 and 1.6e-21, three or
more orders below f64 epsilon, so the realised accuracy is Horner rounding: 0.5
ULP for sin and cos, 1 ULP for atan.

What it bought: the byte-identity gate covered 70.7% of the determinism dump,
with thirty families excluded for the single reason that they reached libm
through an angle. Those exclusions are gone. The only `ALWAN_*` macros still
reaching libm are ABS, CEIL, FLOOR, FMOD, ROUND, SQRT and TRUNC, all exact
IEEE-754 operations that cannot differ between vendors.

Still open, and it is a confirmation rather than an implementation: the argument
above is source-level. A determinism CI run is what turns "no libm is reachable"
into "six runners agree", and that run has not happened yet.

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

Measured, not estimated, and re-measured after each pass. Counting base
operations on the public surface with precision and `_map_*` variants folded
together: **641 base operations, 445 documented, 196 without an entry** anywhere
under `docs/`. It was 324 undocumented before the documentation passes of
2026-09-08.

Closed so far, each as its own page or as sections on an existing one:

| surface | page |
| --- | --- |
| table sampling | [api/tables.md](api/tables.md) |
| spatial and bulk gamut forms | [api/gamut.md](api/gamut.md) |
| LUT bake, interchange, sampling | [api/luts.md](api/luts.md) |
| the seven additional appearance models | [api/color-appearance.md](api/color-appearance.md) |
| perceptual pickers, and their ranges | [api/color-spaces.md](api/color-spaces.md), [ranges.md](ranges.md) |
| accessibility contrast, HDR tone mapping | [api/hdr.md](api/hdr.md) |
| AgX and JP2499 view transforms | [api/view-transforms.md](api/view-transforms.md) |
| experimental picture formation | [api/picture-formation-experimental.md](api/picture-formation-experimental.md) |
| the RGB encoding-space fit | [api/rgb-space-fit.md](api/rgb-space-fit.md) |
| interop IDs, half floats, video signals | [api/interchange.md](api/interchange.md) |
| embedded illuminant SPDs and chart data | [api/reference-data.md](api/reference-data.md) |
| version, global settings, the allocator | [api/context.md](api/context.md) |

The four families this section used to list by name (illuminant SPD getters,
batch deltaE, the AgX and JP2499 family, picture formation) are all in that
table now.

What is left is mostly the bulk `_map_interleave` / `_map_planar` twins of
conversions whose scalar forms are documented, concentrated in `alwan_xyz_*`
(36) and `alwan_rgb_*` (27). Those need a convention stated once in
[map.md](map.md) rather than an entry each, which is a different job from the
cluster pages above.
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

## Hunt inverse: done, and the premise for deferring it was wrong

`alwan_hunt_inverse_f32` / `_f64` are implemented. This section used to plan them
for 3.0.0 on the grounds that "the forward runs a chromatic adaptation whose
parameters depend on the adapted signal, so the inverse needs an iterative solve
rather than a closed form ... Hunt's is three-dimensional".

The adaptation's parameters do not depend on the adapted signal. Its four
per-channel terms, cone bleaching, the chromatic adaptation factor, the
Helson-Judd term and the proximal-field-adjusted white, read the white, the
background, the proximal field and the adapting luminance and nothing else. So
the adaptation is a per-channel monotone map on each cone response and undoes
exactly, and what is left is algebra rather than a search:

- The hue fixes the direction of the opponent pair, leaving one length.
- The three colour-difference signals sum to zero, so they fix only the
  differences between the adapted responses, leaving one achromatic level.
- Saturation ties the length to the level linearly, which makes both the
  achromatic signal and the colourfulness affine in the level.
- Brightness then gives the level from one linear equation.

The one implicit term is the scotopic response, and only because it defaults to
the stimulus Y. It enters through a single scalar, so it is a fixed point that
converges in under 20 passes, and it is skipped whenever the caller sets `S`.

Measured: the round trip over the sRGB gamut lands at 6.5e-11 in f64, across ten
viewing-condition variants covering the Helson-Judd, proximal-field,
discounting, coloured-background and explicit-scotopic paths. Suite 29 pins it.

Two limits remain, and both are properties of the forward rather than of the
inverse. `f_n` clamps its argument at zero, so a stimulus with a negative cone
response is not recoverable and the boundary member of that set comes back. And
a negative saturation is reported as `C = 0`, which discards the chroma. Neither
is reachable from inside a real display gamut.

The forward's agreement with colour-science was also re-measured in the course
of this, and it is not 4.6e-08. It is 2.8e-14 on lightness and 2.0e-13 on
chroma. The old figure came from a reference file whose layout did not match
what the test read, and from a test that printed mismatches without failing.

Nothing in the appearance-model set is missing its inverse.

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
drives it. image_gen's loader now converts from the attribute when a file has
one, deriving the matrices through alwan, and falls back to the by-corpus route
only for a file that declares nothing; `--exr-chroma-check` shows the two agree
to float rounding on both collections. The corpus is a submodule of alwan_dev,
so the stamping lives in its working tree: `git -C extern/SRIC checkout -- exr`
undoes it and the tool redoes it. Making it permanent means a fork of the
collection with the stamped files, which is the owner's call.

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

## The CCM fit has one solver, one objective, and no way to choose either

What a caller gets today, in `api/alwan_color_correction.c`:

- `least_squares_solve` at line 513. Normal equations, `AtA x = Atb`, by Gaussian
  elimination with partial pivoting. The three output channels share one
  elimination. A pivot under `1e-12` returns `ALWAN_E_DIVZERO`.
- Always in `double`. The f32 entry points widen, call the f64 fit, and narrow,
  which is why these are on the `ALWAN_WITH_F64_FACADE` list.
- Two expansions to fit through: Cheung 2004 (3 to 35 terms) and Finlayson 2015
  (degree 1 to 4, optionally root-polynomial, which is the exposure-invariant
  one because every term stays degree 1 in intensity).
- The interface is N paired samples and a term count. It never learns which
  chart it is looking at, which is the right design and should stay.

What is missing is any choice about how the fit is made. A camera profile is a
fit, and the numerical method, the objective, the weighting and the
regularisation all change the answer. None of the four is reachable.

The shape this wants is a params struct with a defaulted initialiser, so the
existing two-line call keeps its behaviour and the knobs are opt-in:

    alwan_ccm_fit_params_f64 p;
    alwan_ccm_fit_params_init_f64(&p);      /* today's behaviour exactly */
    p.solver    = ALWAN_CCM_SOLVER_QR;
    p.objective = ALWAN_CCM_OBJECTIVE_DE2000;
    p.weights   = per_patch;                /* NULL for uniform */
    alwan_ccm_fit_cheung2004_f64(matrix, &p, camera, reference, 24, terms);

Options worth having, roughly in the order they are worth adding:

1. **Householder QR on `A` directly**, instead of forming `AtA`. Normal
   equations square the condition number. The `double` solve is a mitigation,
   not a fix. QR costs about the same at these sizes and gives back roughly
   half the lost digits. Changes nothing on well-conditioned input, so it is
   safe to make the default once it exists.

   Suite 44 now measures this, by recovering a known matrix from samples
   generated through it, which is exact in the linear case so every digit lost
   belongs to the solver:

   | basis | terms | worst coefficient error |
   |---|---|---|
   | Cheung | 3 | 2.1e-15 |
   | Cheung | 11 | 4.5e-13 |
   | Cheung | 22 | 5.7e-12 |
   | Cheung | 35 | 2.9e-10 |
   | Finlayson plain, degree 4 | 34 | 1.5e-10 |
   | Finlayson **root**, degree 3 | 13 | 7.2e-09 |
   | Finlayson **root**, degree 4 | 22 | **1.3e-03** |

   The root basis is the case that matters, and it is much worse than its term
   count suggests. `sqrt(RG)`, `cbrt(R2G)` and `(R3G)^(1/4)` are all "R-ish
   times G-ish" with slowly separating exponents, so the columns crowd
   together as the degree rises. At degree 4 the fit loses about thirteen
   digits, which is what `1.3e-03` on coefficients of order 1 means. That is
   the exposure-invariant model, the one a caller reaches for precisely when
   the lighting is uncertain, and it is currently the least trustworthy fit in
   the library. A QR would put it near `1e-10`.

   A second measured consequence, worth knowing before anyone debugs a
   profile: past degree 2 the root fit's **coefficients are not identifiable**.
   Many coefficient vectors reproduce the data about equally well, so two fits
   of the same data at different exposures land on quite different matrices
   while predicting the same answers. Compare CCMs by what they predict, never
   coefficient by coefficient. The exposure invariance itself is exact and
   holds to 1.8e-15 when measured on predictions.

2. **SVD with rank truncation.** Answers the case QR still cannot: a design
   matrix that is actually rank deficient. Duplicate patches, a chart shot with
   a clipped channel, a term set wider than the chart can support. It also lets
   the fit report its rank, so a caller learns the fit was degenerate instead
   of receiving `ALWAN_E_DIVZERO` with no reason attached.

   The `num_samples < terms` guard is **necessary and not sufficient**, which
   is the concrete case for this. What a fit needs is not a sample count but
   enough distinct levels per channel: the 35-term Cheung set contains `1`,
   `R`, `R2`, `R3` and `R4`, five functions of R, so it needs at least five
   distinct R levels no matter how many patches there are. Suite 44 pins both
   halves: 24 samples that pass the count guard but share one R level are
   singular at 22 terms, and the same 24 spread across the levels fit fine.
   A real chart is not a grid, so its patches can be short of levels in
   exactly this way while looking like plenty of data, and today the only
   signal is `ALWAN_E_DIVZERO` with nothing attached to it.

3. **Tikhonov regularisation.** One parameter, shrinking the high-order terms.
   This is the honest answer to overfitting when the term count approaches the
   patch count, which a ColorChecker Classic reaches at 22 terms for 24
   patches. Cheap: it is a diagonal added before the solve.

4. **Per-patch weights.** Skin and the neutral ramp matter more than a
   saturated cyan for most work, and a uniform fit does not know that. The same
   mechanism drops a patch that glared or is scratched, by weighting it zero,
   which is currently only possible by rebuilding the input arrays.

5. **A perceptual objective.** Minimise dE2000 or CAM16-UCS instead of squared
   error in linear RGB. This is the option that changes results most: least
   squares in linear RGB spends its accuracy where the numbers are large, which
   is the bright patches, not where the eye is sensitive. It is not linear, so
   it needs an iterative solve, with the linear fit as the starting point. It
   also needs a reference to validate against before it can be trusted.

6. **A robust loss**, Huber or plain IRLS. One bad patch currently moves the
   whole matrix. IRLS is a loop around the same linear solve, so it costs
   almost nothing once weights from 4 exist.

7. **A neutral-preserving constraint.** Force the fit so equal RGB maps to the
   reference neutral. A profile that tints greys is worse in use than a
   slightly less accurate one that does not, and this is the constraint most
   profiling tools apply by default. Equality-constrained least squares, or
   solve in a reduced basis and reconstruct.

8. **Term selection by cross-validation.** Choose the Cheung term count from the
   data instead of by hand. Leave-one-out over N patches is N solves of a small
   system, which is nothing at these sizes, and it stops a caller reaching for
   35 terms because it sounds better than 11.

Notes on sequencing. Items 1 and 2 are numerics and change no result on good
data, so they can land first and independently. Items 4 and 6 share one
mechanism. Item 5 is the largest change and the only one that needs external
validation data.

Two things to fix alongside, both found while reading this code:

- **The fits had no test coverage. Done.** Suite 44 covered the expansions and
  nothing called `alwan_colour_correction_matrix_cheung2004` or
  `..._finlayson2015`. Six tests now do: exact recovery of a known matrix
  across the Cheung ladder and every Finlayson degree, exposure invariance
  measured on predictions with the plain basis as a control, the sample-count
  guard and the rank case behind it, the apply round trip (which is what would
  catch a term-ordering slip between fit and apply, since the two read the
  expansion independently), and the f32 twin. The numbers above come from it.

  The Finlayson budgets in that suite are the current normal-equations
  behaviour, measured, not a target. When the solver options land they should
  drop by orders, and tightening them is how the improvement gets recorded.

- **The stack arrays cap the fit at 35 terms** (`AtA[35 * 35]`,
  `aug[35 * 38]`). Fine for the Cheung ladder, but it is a structural ceiling
  rather than a chosen one, and a solver rewrite should take the allocation
  with it.

Worth recording: the usable term count is bounded by the patch count, since the
fit rejects `num_samples < terms`. A Classic caps at 22, an SG at 140 patches
and an IT8 at 288 reach all 35. Bigger charts are not a nicety, they unlock
model capacity, which is a second argument for `alwan_chart_*` beyond reading a
target's own numbers.

---

## Working Checklist

- [ ] Support `ALWAN_EMBED_DATA=0` as a real runtime-data mode
- [ ] Add richer interop metadata/query APIs
- [ ] Thread data semantics through more public conversion workflows
- [ ] Publish tighter GPU/backend examples around the current bootstrap headers
- [x] CUDA backend: kernel-side core, both precisions, run-verified on sm_86. No `ALWAN_BACKEND_CUDA` id in the end, because the emission path is the C one; test `ALWAN_CUDA`
- [x] OpenCL backend (ALWAN_BACKEND 4): 43 of 43 core headers build, run-verified against the C library on an RTX 3060. Needs -cl-std=CL2.0, because __constant is not generic-compatible and the table samplers serve both compile-time tables and runtime LUTs
- [ ] Scope a fixed-point path for parts without an FPU (transfer functions, RGB matrices, video encode first)
- [ ] Add ergonomic helpers only where they reduce real call-site boilerplate
- [x] Extend the deterministic layer to trig/log10 and route the macros; the 30 CI exclusions are removed, pending a confirming run
- [ ] Close batch/map and `_map_planar` coverage gaps (CAMs, ZCAM, deltaE, CVD)
- [ ] Add the bulk two-step Zhai 2018 CAT
- [ ] CCM fit: a params struct, so the solver, objective, weighting and regularisation are the caller's choice; today all four are fixed
- [ ] CCM fit: QR or SVD instead of the normal equations, which square the condition number at 22 and 35 terms
- [ ] CCM fit: per-patch weights, and the robust loss that reuses them
- [ ] CCM fit: a dE2000 or CAM16-UCS objective; the current least squares in linear RGB spends its accuracy on the bright patches
- [ ] CCM fit: a neutral-preserving constraint, so a profile cannot tint greys
- [x] CCM fit: the solve is covered now (suite 44, exact recovery of a known matrix); it measured the conditioning, and the root-polynomial at degree 4 errs by 1.3e-3
- [ ] CCM fit: report rank, not just ALWAN_E_DIVZERO; the count guard is necessary and not sufficient, a chart can pass it and still be short of levels
- [x] Measured chart files: alwan_chart_* reads CGATS.17 / OpenQualia, so a target's own numbers reach the solvers; no network needed
- [x] Fill API parity gaps: norm macros and scalar HSV<->HWB were already in; ZCAM `from_ucs` added
- [x] The f64 facades are documented, each with its reason, in precision-and-limits.md; they stay facades by design
- [x] Harden `alwan_create` validation (non-zero flags and a half allocator pair return NULL); the 44 public enums were already fully pinned
- [ ] Document the undocumented tail surface
- [x] Hunt inverse: closed form, not the three-dimensional solve this planned; round-trips the sRGB gamut to 6.5e-11
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

## Test data still assumes native ranges under ALWAN_NORMALIZE_RANGES

`ALWAN_NORMALIZE_RANGES` defaults to `1` in `alwan_platform.h`, and that is what
a consumer linking a stock build gets: the API wrappers rescale bounded channels
through `ALWAN_NORM_*` on the way out and `ALWAN_DENORM_*` on the way in.

Nothing in either repo compiles the library that way. `alwan_dev/CMakeLists.txt`
puts the library target in `ALWAN_NORMALIZE_RANGES=0`, and the Sharpmake
reference build defines `=0` for every project. The macros expand inside the
library's own translation units, so a consumer cannot change the setting from
its own code either; it is a library-compile-time decision, and
`alwan_platform.h` telling the reader to define it before including `alwan.h` is
wrong for anyone linking a built library.

The result was that the default code path had no test coverage at all, and both
defects found in it up to then were found by reading rather than by a failure:
YCbCr double-offsetting chroma, fixed 2026-08-27, and YcCbcCrc doing the same,
found on 2026-09-08 while documenting the conversions and fixed the same way.
In the second case the scalar path disagreed with the bulk kernels and with the
committed reference CSV, and no test noticed, because every test build takes the
other branch.

**The configuration now exists.** `cmake -S . -B build_norm
-DALWAN_DEV_NORMALIZE_RANGES=1` builds the library and the tests at the shipped
default, and suite 111 asserts the documented ranges in it: achromatic grey on
the chroma midpoint for each `Y*` encoding, bounded channels inside `[0, 1]`,
round trips through the public API, and the scalar-against-bulk agreement check
this section asked for.

**That check found the defect it was predicted to find.** At the default
setting, `alwan_xyz_to_hunter_lab_f64` and
`alwan_xyz_to_hunter_lab_f64_map_interleave` are the same public conversion and
disagree in `L` by 99.0, the whole normalisation factor: the scalar wrapper
applies `ALWAN_NORM_HUNTER_LAB`, the bulk path does not. A caller mixing the two
in a stock build gets lightness a hundred times out on one of them, and hue
three hundred and sixty times out for the cylindrical spaces.

It is not uniform, which is the part that makes it a bug rather than a
documented split. `alwan_xyz_to_lab_f64` and its bulk twin agree to 3e-16 in the
same build. So some conversions honour the setting on both paths and some honour
it on one, and nothing says which.

**Fixed 2026-09-09.** Every bulk entry point for a space with an `ALWAN_NORM_*`
macro now applies it, which was the right half of the choice: the alternative
makes the two halves of the public API answer different questions. The sweep
covered the interleave wrappers for Hunter Lab, ProLab, DIN99 and UVW, the
planar generators for all of those plus HCL and IHLS, the YCoCg planar pair,
and both directions of the sRGB/Lab convenience pair. The planar generators in
`alwan_extended_map_kernels.inc` take a pair of channel hooks
(`ALWAN_MAP_P_N_L_UNIT`, `..._W_NATIVE`, `..._HS_UNIT`, and so on) so each
generated function states its scaling at the call site instead of leaving it
implicit; they expand to nothing when the setting is off.

Two things worth keeping:

- `alwan_srgb_to_lab_f64` and `alwan_lab_to_srgb_f64` were wrong on the
  *scalar* side. They call `alwan_xyz_to_lab_f64_v` directly, so they skipped
  the `ALWAN_NORM_LAB` that every other scalar wrapper applies. The bug was
  visible only once the bulk path was correct.
- Suite 88's grid generators fed native-range values into a normalised API.
  Both paths accepted them, but `LCh->Lab` denormalised a hue of 360 into
  1.3e5 degrees, where SIMD and scalar argument reduction legitimately part
  company: 8.9e18 ULP. The grids now scale with the setting.

Suites 70, 88 and 111 are green in `build_norm`; the default and deterministic
builds stay at 111 of 111.

Nineteen suites still fail in `build_norm`, and that number is not a bug count.
They are reference CSVs and test inputs written for native ranges: suite 106
feeds `h=20, s=60, l=50` into HSLuv, which at the shipped default denormalises
to 7200 degrees. Making those configuration-aware is a separate job, and the
signal to keep watching is the tests of configuration-independent properties,
which are the three above.

Also still true, and the reason this went unnoticed for so long: the macros
expand inside the library's own translation units, so a consumer cannot change
the setting from its own code. It is a library-compile-time decision, and
`alwan_platform.h` telling the reader to define it before including `alwan.h` is
wrong for anyone linking a built library.

---
## Keeping this file honest

When updating it:

1. check `src/alwan/alwan.h` first
2. delete anything that has since been implemented
3. keep it to real remaining gaps, not to what was once intended

A file of intentions ages badly; a file of known gaps stays useful.
