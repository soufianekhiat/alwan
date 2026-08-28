# Deliberate divergences

Places where alwan knowingly returns something other than what a reference
implementation returns, and why. Everything here is a decision, not a defect.

The distinction matters because alwan is validated against colour-science and
OCIO, and a future contributor diffing against either will find these. Each
entry says what the difference is, what it costs, and what would break if
someone "fixed" it.

Three principles drive most of the list, in this order of precedence:

**The definition wins.** Where a standard prescribes behaviour, alwan implements
it even when the result is inconvenient. ACESproxy is an integer encoding, so
alwan quantises and the round trip becomes a staircase. A house principle does
not get to overrule a specification.

**Reversibility.** Where the definition leaves it open, a forward transform
paired with an inverse should round-trip to machine precision. Clamping a *value*
inside a transform destroys that and the caller can never undo it, so alwan does
not. This is why an out-of-range colour is an error rather than a silent clamp,
and why the ACES 1.x inverse was made exact.

**GPU pairing.** The core headers compile to HLSL, GLSL and Halide as well as C.
A transform that needs a table search or a variable-length loop is either slow or
impossible on that path. Where a choice exists between a form that vectorises and
one that does not, alwan takes the former and says so.

Gaps that are not decisions live in [alwan_future.md](alwan_future.md).

---

## House defaults

### Every default is the zero value, and the zero value interpolates

`ALWAN_INTEGRATE_TRAPEZOID = 0` and `ALWAN_SAMPLE_LINEAR = 0`, so a
zero-initialised or memset struct integrates by trapezoid and interpolates
rather than snapping. Nothing in alwan defaults to nearest-neighbour or to
"whatever the caller forgot to set".

| axis | default | reached from the zero value |
|---|---|---|
| SPD integration | trapezoid | `ALWAN_INTEGRATE_TRAPEZOID = 0` |
| rank 1 table | linear | `ALWAN_SAMPLE_LINEAR = 0` |
| rank 2 grid | bilinear | LINEAR resolves to it |
| rank 2 strip | trilinear | LINEAR resolves to it |
| rank 3 cube | trilinear | LINEAR resolves to it |
| SPD resampling | linear | `ALWAN_RESAMPLE_LINEAR` |

LINEAR resolving to bilinear at rank 2 and trilinear at rank 3 is deliberate:
LINEAR is the zero value, so rejecting it at the higher ranks would turn a
zero-initialised mode into `ALWAN_E_INVALID` instead of the interpolated read
the caller expects. It is the one place a mode is *resolved* rather than
rejected, and it resolves only ever to the linear member of that rank's family,
never across families.

Simpson's rule is available and more accurate on smooth spectra, but trapezoid
is the default because it is what the reference implementations use, and
agreeing with them matters more than converging faster.

### Values are never clamped to a gamut

A transform returns the raw standard maths. An out-of-range excursion is
preserved, not squeezed back into `[0, 1]`: super-black and super-white survive
a Y'CbCr decode, xvYCC decodes outside the box, a CVD simulation may leave the
display gamut, and a wide-gamut conversion may go negative.

Where clamping is genuinely wanted it is a **separate, explicitly named entry
point** that takes the mapping method as an argument, never a flag on the raw
one:

    alwan_ycbcr_to_rgb_f64            raw, may leave [0,1]
    alwan_ycbcr_to_rgb_gamut_safe_f64 takes an alwan_gamut_map_method

The same pairing exists for `yccbccrc` and for the three `simulate_cvd`
entry points. `ALWAN_GAMUT_MAP_CLIP` reproduces the implicit clamping alwan did
before 2.0.0, so the old behaviour is still reachable, by name.

**Why:** a clamp is not recoverable. A caller who wanted the excursion cannot
get it back, and a caller who wanted it clamped can always clamp. It is the same
argument as the reversibility principle above, applied to range rather than to
invertibility, and it is why "addresses are clamped, values never are" below
reads the way it does.

### Unbounded axes keep their native range

`ALWAN_NORMALIZE_RANGES=1` rescales every channel with a true fixed bound into
`[0, 1]`. It leaves the opponent axes alone: Lab / Hunter-Lab / ProLab `a` and
`b`, Luv `u` and `v`, the cylindrical `C`, and Oklab `a` and `b`.

Those axes are mathematically unbounded even though encodings give them a
conventional span (`[-128, 127]` for Lab in 8-bit and ICC, roughly `[-0.4, 0.4]`
for Oklab). Normalising against a convention would silently compress anything
past it, which is exactly the wide-gamut and out-of-gamut colour a colour library
exists to carry. Leaving them native also keeps reference comparisons 1:1.

---

## Transfer functions

### ACESproxy is quantised

**Reference:** ANSI/SMPTE S-2013-001 defines ACESproxy as an *integer* log
encoding. The rounding to a code value is part of the definition, not a step for
the caller.

**alwan:** rounds, then clamps to `[CV_min, CV_max]`. Both bounds are integers so
the order is not observable, and colour-science does the same. At linear 0.18 the
code value is 426.30344, which encodes as `426 / 1023 = 0.4164223`, matching
colour-science exactly.

**Cost:** the curve is a staircase, so `EOTF(OETF(x)) != x` in general. The round
trip returns the linear value at the centre of the code value the input landed
in, within half a step, which is `2^(1/100) - 1 = 6.96e-03` in relative terms.
`alwan_dev/tests/09_tf_hdr.c` bounds it by exactly that rather than by a
precision tolerance.

**Why:** this is one place where matching the definition outranks the
reversibility principle at the top of this document, because the quantisation
*is* the transfer function. An earlier version returned the continuous code value
to preserve the round trip; that made alwan the only implementation that
disagreed with the spec and with every integer ACESproxy encoder.

The EOTF is the exact inverse of the unrounded curve, so it maps a code value
back to its bucket centre, and feeding it a non-quantised input interpolates
between codes rather than failing.

### Negative scene values clamp on some camera curves

`clog`, `clog2`, `elog` and `protune` clamp negative input to zero before the
log. colour-science extrapolates. On the valid domain the two agree exactly
(`clog` matches at 0.0e+00); the difference exists only below zero, where the
published curves are not defined.

This is a house convention, applied consistently. It is why the transfer-function
reference sweep in `alwan_dev/tests/103_tf_reference.c` uses a positives-only
grid: a negative sample there would measure the edge policy, not the curve.

### V-Log splits at `linear < cut1`, not `<=`

The cut point itself belongs to the log segment. The two segments are not exactly
continuous there, so the choice is observable: at exactly 0.01 the branch matters
to 3.1e-07. Panasonic's specification uses `<`, and so does alwan.

---

## Video signal

### Y'CbCr chroma is centred on 0.5, which is the standard's digital stage

**What the standard says.** ITU-R BT.601-7, BT.709-6 and BT.2020-2 define the
colour-difference signals in normalised form as

    E'Y  = kr E'R + kg E'G + kb E'B                  in [0, 1]
    E'Cb = (E'B - E'Y) / (2 (1 - kb))                in [-0.5, +0.5]
    E'Cr = (E'R - E'Y) / (2 (1 - kr))                in [-0.5, +0.5]

so at that stage chroma is **signed and centred on zero**. The standards then
quantise, and the quantisation is where the offset appears:

    narrow range, 8-bit :  Y' = 219 E'Y + 16,   Cb = 224 E'Cb + 128
    full range,   8-bit :  Y' = 255 E'Y,        Cb = 255 E'Cb + 128

The `+128` is `+0.5` of full scale. Both forms are the standard; they are two
stages of it.

**What alwan's API does.** `alwan_rgb_to_ycbcr_*` computes exactly the `E'Cb` and
`E'Cr` above and then adds `0.5`, so it returns the **digital full-range**
convention normalised to `[0, 1]`. That is what an 8-bit full-range code value
divided by 255 gives you, which is what a decoded image buffer actually holds.
The decode subtracts the same `0.5` before applying the standard's inverse.

So alwan is not choosing against the standard here; it is choosing which stage of
it the public type represents, and it picks the one that matches a pixel in
memory rather than an analogue signal level.

**What follows from it.** `ALWAN_NORMALIZE_RANGES` (default 1) rescales bounded
channels into `[0, 1]`. Chroma is already there, so `ALWAN_NORM_YCBCR` and
`ALWAN_DENORM_YCBCR` are deliberately no-ops. They exist as named macros rather
than being deleted so the normalisation table has an entry for every colour type,
and so the next reader finds a decision instead of an omission.

They were not always no-ops. Adding `+0.5` there, on top of the `+0.5` the kernel
already applies, offset chroma by a full 1.0 in the shipped default build.

**If you want the signed `E'Cb` form**, subtract 0.5 from what the API returns.
Changing the library convention instead is not a one-line switch: those two
macros become real conversions, the legal-range pair changes with them, and so do
the SIMD kernels below.

### YCoCg does keep its normalisation offsets

The four `ALWAN_MAP_P_NORM_ADD(..., ±0.5)` on the YCoCg map path are correct and
must stay. That kernel emits genuinely signed Co/Cg and does not self-centre, so
unlike Y'CbCr it has something for the normalisation layer to do.

### The legal-range SIMD kernels duplicate the scalar formula

`alwan__ycbcr_full_to_legal_kernel` and its inverse compute the conversion inline in
intrinsics for the vector lanes and call the core function for the scalar tail. The
two forms are algebraically equal, not textually identical: the forward folds the
0.5 into the addend so the lane path stays a single fmadd.

This is a performance decision with a maintenance cost, and the cost is real: a fix
applied to the core alone left the lanes wrong and the tail right, in the same
function. `70_planar_map` compares `_v` against `_map_planar` and catches exactly
this, which is the only reason the duplication is tolerable. **Any edit to the core
legal-range conversion must be mirrored into both kernels**, and that test is what
proves it was.

---

## ACES 1.x

### The inverse RedMod10 is exact, and OCIO's is not

**Reference:** OCIO's `Renderer_ACES_RedMod10_Inv` solves

    red_out = red * (1 - w*k) + w*k*pivot

for `red` using weights `w = f_H * f_S` evaluated at the *output*. But the
weights depend on red, so those are not the weights the forward used, and the
result is an approximation. Measured on OCIO 2.5.0, forward then inverse
round-trips a saturated red to 3.7e-03.

**alwan:** green and blue pass through the forward untouched, so recovering red
is a scalar root find. `w*k` is confined to `[0, k]` because both weights lie in
`[0, 1]`, which brackets the root with no search, and the map is monotonically
increasing in red over the full green/blue grid. Regula falsi with the Illinois
correction converges inside the bracket in single digits.

**Cost:** alwan's inverse differs from OCIO's by up to ~3.7e-03 on saturated
reds, and agrees everywhere the forward left red alone. alwan is the more
accurate of the two; the difference is OCIO's round-trip error, not alwan's.

**Why:** reversibility is the first principle in this document, and an inverse
output transform exists to recover scene-linear values.

### The inverse Glow10 is closed form

The forward multiplies all three channels by `1 + glow`. Saturation is
`(max - min) / max`, which a uniform scale leaves unchanged, and YC is
homogeneous of degree one, so both the gain and the piecewise branch are
recoverable from the output alone. No iteration is needed. This matches OCIO,
whose Glow10 inverse is also exact (measured round-trip 3.7e-09, at f32
precision).

### An unsupported transfer function is an error, not a passthrough

Two paths in the ACES 1.x output transform returned `ALWAN_OK` when the OETF or
EOTF they needed was unsupported: the forward handed back scene-linear values,
and the inverse carried on as though its display-encoded input were already
linear. Both now return the status they got. A picture that is wrong by an entire
transfer function should not report success.

---

## Colour appearance and quality metrics

### ATD95 returns H as a ratio

The model defines the hue correlate as `H = T_2 / D_2`, a plain ratio. It is
unbounded and may be negative. alwan returns exactly that.

An earlier version returned `degrees(atan2(T_2, D_2))` wrapped into `[0, 360)`,
which is a *different quantity*. If a caller expects degrees, the fix is in the
caller.

### CRI Ra is not clamped to [0, 100]

CIE 13.3 lets Ra go negative for very poor sources. Clamping would hide exactly
the sources the metric exists to flag. High-pressure sodium scores 8.07; a bad
enough source scores below zero, and it should.

### CQS 9.0 has no CCT factor, and its scaling factor is 3.2

Not a divergence. NIST CQS 9.0 sets `CCT_f = 1` and `scaling_f = 3.2`; the CCT
factor belongs to CQS 7.4, which pairs it with `scaling_f = 3.104`.
colour-science branches on exactly that. alwan implements 9.0.

This entry previously claimed alwan omitted the factor as a deliberate
divergence, and recorded two attempts to add it that measured worse. Both
attempts were chasing a step 9.0 does not have. The real defect was next to it:
alwan ran 7.4's scaling factor with 9.0's absent factor, which is neither method.
With 3.2 restored, agreement with colour-science over 33 illuminants is mean
0.065 and max 0.235, against mean 0.447 and max 4.490 before, and D65 lands on
100.0001 where it must be 100.

One number from those attempts is worth keeping, because it says something about
alwan rather than about the method: the 8210 normaliser that 7.4 divides by is
not the D65 gamut area colour-science itself computes, which is 8131.03 against
alwan's 8130.95. That agreement is a check that alwan's gamut geometry is right;
8210 is a published constant the sample set no longer reproduces.

### Light-quality metrics integrate 360-830 nm by trapezoid

colour-science's `cfi2017` works on 380-780 nm and trims the test spectrum to it.
alwan integrates its full 360-830 nm 5 nm grid with the trapezoid rule, because
that is the grid every other spectral table in the library uses.

**Cost: not measurable.** This entry used to read "the residual in TM-30 (mean
0.51) and CQS (mean 0.447) is mostly this", which was asserted, never measured,
and is wrong. Three measurements retire it:

- CQS's 0.447 was a crossed scaling factor. With the integration untouched it is
  now 0.065.
- The white point of a blackbody computed over 360-830 against 380-780 differs by
  **1e-6 in xy**, across 1959 K to 6504 K. Daylight is the same.
- colour-science's own Rf at 1 nm against 5 nm differs by **0.0000** on eleven
  illuminants, so the interval does not matter either.

Measured against colour-science over 33 illuminants, all three metrics on this
grid:

| metric | mean | max |
|---|---|---|
| CRI Ra | 0.057 | 0.428 |
| CQS Qa | 0.065 | 0.235 |
| TM-30 Rf | **0.530** | **1.990** |

CRI and CQS sit where the convention would predict. TM-30 does not, and the
reason is not this. It is tracked in [alwan_future.md](alwan_future.md) with the
evidence.

### The TM-30 reference blend is normalised at 560 nm, not by Y

In the 4000-5000 K region TM-30 blends a Planckian and a daylight reference. The
two legs have to be brought to a common scale first or the blend is dominated by
whichever carries more absolute radiance. alwan normalises both to 100 at 560 nm,
the CIE normalisation wavelength; colour-science divides each by its own Y.

The two agree on the endpoints and differ slightly in between. Residual in the
blend region is 0.30 mean against 0.24 for pure daylight, so this is worth at
most 0.06, and the blend region is not where TM-30's problem is.

### Ohno 2013 switches locus model at 15000 K

Below 15000 K the CCT is solved by Ohno's triangular and parabolic method over
the Krystek 1985 locus. Above it, alwan returns the Hernandez-Andres 1999 closed
form.

**Why:** Krystek is published for 1000-15000 K. Above that its own deviation from
the true Planckian locus reaches 8.1e-04 in uv at 25000 K, about 1550 K measured
along the locus, which is more than any solver over that locus can recover.
Hernandez-Andres is fitted directly to the true locus and stays accurate there.
The switch is at the model's published limit, not a tuned crossover.

Measured against colour-science on 60 locus points: max 47.6 K, mean 8.0 K.
Below 5000 K, max 1.2 K.

### Hellwig2022 has no N_bb, N_cb or N_c in its achromatic response

Not a simplification alwan made. Dropping CAM16's induction factors is
Hellwig2022's own contribution, and `A = 2R + G + 0.05B - 0.305` is the
published formula. Matches colour-science to 3.9e-10.

The code once carried comments reading "Hellwig2022 simplified", which is true
of the model relative to CAM16 and reads as though the implementation cut a
corner. A vocabulary scan for incomplete work flagged all five of them as
suspects. They now say what they mean.

---

## Spectral

### Blackbody SPDs are spectral radiance

`alwan_spd_blackbody` returns W*sr^-1*m^-3, matching colour-science's `planck_law`
to 3.7e-10. The first radiation constant `c1 = 2*pi*h*c^2` gives radiant
*exitance*, so the implementation divides by pi. Chromaticity is unaffected either
way, being scale-invariant; absolute values are not, and they were pi times too
large until this was fixed.

### The bandpass correction follows the paper, not colour-science

**Reference:** Stearns & Stearns (1988) correct a measured SPD for a triangular
passband with

    X'[i] = -a*X[i-1] + (1+2a)*X[i] - a*X[i+1],   a = 0.083

and `X'[0] = (1+a)X[0] - a*X[1]` at the ends. Every X on the right is a *measured*
value: it is a fixed linear filter.

**colour-science:** `bandpass_correction_Stearns1988` writes into the same array it
reads, so from the second element on it uses already-corrected neighbours. That is a
sequential recursive filter, not the paper's.

**alwan:** reads the original values throughout.

**Cost:** the two disagree by 4.3e-3 on a spiky 31-sample SPD. The first element
agrees exactly, and the difference grows through the array, which is the signature
of the aliasing.

### bandpass_nm asserts the sampling interval, it does not scale the correction

`a = 0.083` is not a free parameter: the correction is derived for a passband whose
width equals the sampling interval. So `bandpass_nm` says what that interval is, and
a value disagreeing with the SPD's own spacing returns `ALWAN_E_INVALID` rather than
applying the correction at a strength it was never derived for. Pass 0 to skip it.

---

## Data

### `cie_2012_*` and `cie_2015_*` CMF tables are identical

This is correct and must not be "fixed". colour-science carries only
"CIE 2015 2/10 Degree Standard Observer"; the 2012 proposal was adopted as the
2015 standard. Two names, one dataset.

### `cat_xyz_scaling` equals its own inverse

Also correct. The XYZ-scaling CAT is the identity.

### CES reflectances are zero outside 380-780 nm

alwan stores 360-830 nm at 5 nm; the CIE 224:2017 TCS set is native 380-780 nm.
Outside that band the reflectance is zero, matching `cfi2017`, which warns that
"missing values will be filled with zeros".

Both edge-hold and zero-fill were built and scored over 35 illuminants: mean
absolute deviation 0.514 against 0.513. The choice is not observable because the
10-degree CMFs are ~0 out there. Zero-fill is kept because it is what the
reference does, not because it measured better.

---

## Bounds and sampling

### Addresses are clamped; values never are

An out-of-range array *address* is clamped, because the alternative is an
out-of-bounds read. A *colour value* is never silently clamped, because the
alternative is a wrong answer that looks plausible.

`ALWAN_READ_DATA_NO_BOUND_CHECK` (default 0, meaning checked) compiles the
address clamp out for callers who can guarantee finite in-range coordinates.
With it set, a NaN coordinate is an out-of-bounds read rather than a wrong
colour. See `alwan_config.h`.

NaN resolves to the **low** edge everywhere, and that is pinned by
`alwan_dev/tests/102_table_gate.c`.

### A float argument that cannot address a table is an error, not an edge

The address clamp above governs the *reader layer*, which has no way to report a
problem. An API entry point that takes a float and turns it into an index does
have a status return, so it uses it: `alwan_spectral_locus_xy_*` returns
`ALWAN_E_INVALID` for NaN, either infinity, and any out-of-range wavelength, and
leaves the output buffer untouched.

The two rules are consistent, not in tension. NaN resolves to the low edge where
the alternative is an out-of-bounds read, and is rejected where the alternative
is a plausible-looking answer to an unanswerable question.

Range tests on these paths are written negated:

    if (!(x >= LO && x <= HI)) return ALWAN_E_INVALID;

`x < LO || x > HI` is false for NaN in both halves, so the natural form lets NaN
through to the cast. That is not a hypothetical: it is how
`alwan_spectral_locus_xy_f64(NaN)` came to return `ALWAN_OK` and the 830 nm value.

### Table extents come from the enum, never from a sibling table

Where an index is checked against one count and used to subscript several tables,
every one of those tables is `_Static_assert`ed against the enum that defines the
index. Checking the tables against *each other* would let them all drift away from
the enum together, which is the same bug one indirection further out.

Concretely, `alwan_rgb_get_space_descriptor_*` validates `space` against
`ALWAN_RGB_SPACE_COUNT` and then reads four tables: primaries, transfer functions
and matrices, in two precisions. Each asserts its own length against that constant.

A helper that copies a whole table takes no extent parameter for the same reason.
An `int size` argument sitting next to a `T const *table` argument means the loop
bound and the bounds-gate clamp both come from the caller, so a caller that passes
the wrong constant reads past the end and the gate clamps to the wrong number
rather than catching it.

### An unsupported sample mode is rejected, not downgraded

`ALWAN_SAMPLE_BILINEAR` on the 2-d strip returns `ALWAN_E_INVALID`, because the
strip is a flattened cube and is genuinely sampled trilinearly. It is accepted by
the 2-d grid reader, which is a real 2-d table. Passing a mode a table's rank
cannot honour is an error, never a silent fall back to the default. A silent
downgrade means the caller gets a different interpolation than they asked for and
no way to find out.

### The default sample mode is LINEAR

`ALWAN_SAMPLE_LINEAR = 0`, so a zero-initialised mode interpolates rather than
snapping to the nearest sample. Banding from an accidental NEAREST is silent and
easy to miss; the cost of the default being interpolation is a little arithmetic.

---

## Zero-initialise viewing-conditions structs

Several appearance models take a viewing-conditions struct whose optional fields
use 0 as a "derive this" sentinel: Hunt has ten of them, RLAB and Kim2009 one
each. Always write

    alwan_hunt_viewing_conditions_f64 vc = {0};

and then set what you know. A struct left as stack garbage produces confident
nonsense rather than an error, because there is no value the library can reject:
a plausible-looking float is indistinguishable from a deliberate one.

This is not hypothetical. alwan's own f32/f64 twin test set four Hunt fields and
left the rest uninitialised; when the new fields landed, the two precisions read
different stack rubbish and diverged by 34 units of lightness.

---

## Sector trees exist in three copies

`alwan_hsy_to_rgb` and its chroma helper decide a colour from `floor(h*6)` using a
branchless select tree. That tree is written out three times: in `_core.inc` for C,
in `_core.h` for the GPU backends, and again inside the SIMD map kernel in
`alwan_convenience_extra_map_kernels.inc`, which open-codes it in intrinsics rather
than calling the core.

Any change to the sector logic must be made in all three. Fixing the first two and
not the third made the bulk path disagree with the scalar path by 2.0e16 ULP.
`88_simd_parity` is what catches that, and it is the reason the duplication is
tolerable at all. The same applies to the legal-range Y'CbCr kernels above.

---

## The shipped GPU backends are single precision

`alwan_scalar` is `float` on the HLSL, GLSL and Halide backends. There is no
`_f64` instantiation on any of them, so every `_f64` public function is C-only,
and so is anything whose correctness depends on double accuracy.

This is a property of *those* backends, not of GPUs. HLSL and GLSL have no
double, and the shipped Halide path runs single precision even though Halide's
scalar can be double. A CUDA backend would have a real `double` and would be the
first GPU path where the `_f64` surface is reachable at all; it is on the roadmap
in [alwan_future.md](alwan_future.md) and nothing here assumes it never lands.

This is why the core headers are written the way they are. A core function is
`ALWAN_CORE_T`-generic and value-returning so the same source compiles as f32 on
GPU and as either precision on CPU, and why the parity checker exists at all: the
`_core.h` copy is what a shader sees, the `_core.inc` copy is what C sees, and a
divergence between them is a divergence between backends.

What stays CPU-only follows from it, not from a separate decision: the compiled
bulk and strided API, the typed pixel formats, the `_map_interleave` and tiled
SIMD kernels, and the null-checking stride-loop wrappers. On GPU the application
owns buffers and dispatch and calls the per-pixel core functions directly.

Two consequences that catch people:

- **An f64-validated result is not automatically a GPU result.** Where a metric
  is only validated in double, that validation says nothing about the shader
  path; `101_f32_twins.c` is what covers the crossing.
- **Iterative solvers are the sharp edge.** The ACES 1.x inverse keeps f64
  internals precisely because its convergence thresholds sit below f32 epsilon,
  so a native-f32 version fails to converge rather than converging less well.

---

## The test suite runs in f64 against f32 references

alwan's validation suite exercises the `_f64` surface, and the references it
compares against are single precision. OCIO's published values are float32, and
so are the ACES fixtures derived from it. colour-science is f64, so the metrics
validated against it are held to 1e-12; the OCIO-derived ones cannot be.

Tolerances are set from that, not from alwan's own precision. `54_aces20.c`
allows 0.1 against the OCIO tonescale fixtures, and says why in the line above
the constant: the reference is float32 and negative inputs diverge fastest
between the two precisions.

Two consequences worth being explicit about:

- **A loose tolerance against an OCIO fixture is not slack in alwan.** It is the
  reference's precision. Tightening it would be measuring float32 round-off.
- **The f32 surface is validated against alwan's own f64 path**, not against
  OCIO directly, in `90_aces_f32_validation.c`. That separates "is the f32 path
  faithful to the f64 one" from "is the f64 one right", so a regression in
  either is attributable.

Deterministic builds are a third axis again: they replace libm with alwan's own
pow/exp/log, which costs about 3.0e-11 relative on the transfer functions and
1.5e-05 on the ACES chain, so several suites carry a separate
`#if ALWAN_DETERMINISTIC` bar. Those are stated at each site with the measured
number rather than rounded up to a comfortable constant.

---

## Data is embedded, not loaded at runtime -- in 2.0.0

`ALWAN_EMBED_DATA=1` is the only supported build for 2.0.0. Setting it to 0
`#error`s rather than compiling something that would look for files that are not
there, and `alwan_config.runtime_data_root` is reserved and inert.

Every table is `#include`d from CSV into a C array at build time, so the library
has no data path, no file I/O, no load-order failures, and nothing to ship
alongside the binary. It is also what lets the table reader layer bound every
access at compile time, and what makes the GPU backends possible at all: a shader
cannot open a file.

The cost is binary size and a rebuild to change data.

**A runtime / on-demand mode is planned for 3.x.y.** This entry is here because
embedding is a real decision with real consequences for 2.0.0, not because the
alternative was rejected forever. When runtime loading lands, embedded and
runtime paths are expected to expose the same public descriptors and getters, so
the choice becomes a build option rather than a different API. See
[alwan_future.md](alwan_future.md).

---

Gaps that are not decisions live in [alwan_future.md](alwan_future.md).
