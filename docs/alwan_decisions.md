# Deliberate divergences

Places where alwan knowingly returns something other than what a reference
implementation returns, and why. Everything here is a decision, not a defect.

The distinction matters because alwan is validated against colour-science and
OCIO, and a future contributor diffing against either will find these. Each
entry says what the difference is, what it costs, and what would break if
someone "fixed" it.

Two principles drive most of the list:

**Reversibility.** A forward transform paired with an inverse should round-trip
to machine precision. Quantisation and clamping inside a transform destroy that,
and the caller can always quantise afterwards. The caller cannot un-quantise.

**GPU pairing.** The core headers compile to HLSL, GLSL and Halide as well as C.
A transform that needs a table search, a variable-length loop, or an integer
staircase is either slow or impossible on that path. Where a choice exists
between a form that vectorises and one that does not, alwan takes the former and
says so.

---

## Transfer functions

### ACESproxy is not quantised

**Reference:** ANSI/SMPTE S-2013-001 defines ACESproxy as an *integer* log
encoding. colour-science rounds to integer code values even when returning a
normalised float.

**alwan:** returns the continuous code value. Everything else in the spec is
applied: the `2^-9.72` floor, the `[CV_min, CV_max]` clamp, and
`mid_log_offset = 2.5`.

**Cost:** up to half a code value, 4.7e-04 normalised. At linear 0.18 alwan
returns `426.30344 / 1023 = 0.4167189` where a rounding implementation returns
`426 / 1023 = 0.4164223`.

**Why:** quantising inside the OETF makes the curve a staircase and breaks
`EOTF(OETF(x)) == x`. It also makes the function unusable in a shader, where the
result feeds further float maths. Round the result yourself if you need bit-exact
ACESproxy code values.

**Do not change** without also changing the EOTF, and expect every round-trip
test to fail.

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

### Y'CbCr chroma is centred on 0.5, and normalisation leaves it there

alwan's float Y'CbCr uses full-range [0,1] for all three channels, with achromatic
chroma at 0.5. The alternative convention, luma in [0,1] and chroma signed in
[-0.5,0.5], is equally defensible and is what several libraries use.

The choice matters because `ALWAN_NORMALIZE_RANGES` (default 1) rescales bounded
channels into [0,1]. Under the 0.5-centred convention there is nothing to rescale,
so `ALWAN_NORM_YCBCR` and `ALWAN_DENORM_YCBCR` are deliberately no-ops. They exist
as named macros rather than being deleted so that the normalisation table has an
entry for every colour type, and so the next person to read it finds a decision
instead of an omission.

They were not always no-ops. Adding +0.5 there, on top of the +0.5 the core kernel
already applies, offset chroma by a full 1.0 in the shipped default build.

**If you change the convention to signed chroma**, these two macros become real
conversions, the legal-range pair changes with them, and so do the SIMD kernels
below. It is not a one-line switch.

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

### The ACES 1.x inverse now round-trips to 1.3e-11 on every output

Three separate defects sat behind a single achromatic round-trip test. Together
they cost 1.31e-01 of relative error on saturated chromatic input:

1. **Step 11 omitted** (below).
2. **DCDM_48NIT returned early** through a separate AP1-space RRT inverse that
   did not invert its own forward. The forward runs the whole pipeline and
   diverges only at the final XYZ gamma-2.6 encode, so the inverse now falls
   through the common path with the display-primaries matrix skipped. The stale
   `aces1_rrt_core_f64` / `aces1_rrt_core_inv_f64` pair it depended on is gone:
   the forward half was already unreferenced, and neither matched the shipped
   pipeline (glow with no YC falloff, a red modifier reading `0.03 - 0.03*r`,
   and both working in AP1 where the real forward works in AP0).
3. **The inverse dimSurround used the wrong exponent.** The forward is
   `x' = x * Y^-g` with `Y = luma(x)`, so `luma(x') = Y^(1-g)` and recovering `x`
   needs `x' * luma(x')^(g/(1-g))`. The code used `g`. Only `g/(1-g)` inverts the
   forward; `g` inverts a slightly different transform, and it left 9.4e-04 on
   every 100-nit video output.

Cinema and HDR PQ outputs skip the dimSurround block entirely, so they were
already at 1.3e-11 while the video outputs sat at 9.4e-04. That gap is what
isolated the third defect.

### Step 11 of the inverse was omitted, and the test hid it

Both of the above were absent. The code said so:

    /* For achromatic inputs (the roundtrip test case), both are identity.
     * Full inverse requires iterative solve -- omitted for now. */

The round-trip test used an achromatic input, which is exactly where the two
omitted steps are the identity, so it passed. On non-clipping chromatic input the
worst relative round-trip error was 1.31e-01, and implementing step 11 alone
brought it to 9.44e-04. The remaining two defects above account for the rest.

`alwan_dev/tests/56_aces1_output_transform.c::test_chromatic_roundtrip` pins
this across all twelve outputs. It skips cases whose forward output clips, since
a clipped channel has thrown information away and measures the display clamp
rather than the inverse. Deterministic builds get a looser bar: alwan's own
pow/exp/log replace libm and the chain runs several in series, which costs
1.5e-05.

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

### CQS omits the CCT factor

**Reference:** NIST CQS 9.0 multiplies Qa by
`CCT_f = min(1, gamut_area(reference) / GAMUT_AREA_D65)`, penalising sources
whose reference renders a small gamut.

**alwan:** does not apply it.

**Cost:** a uniformly positive residual, largest at low CCT. HP1 reads 35.4
against colour-science's 33.7. Mean absolute deviation over 35 illuminants is
0.447 without the factor and 1.063 with it.

**Why:** it was implemented and measured twice, the second time on a fully
corrected adaptation framework, and made results worse both times. The `8210`
normaliser is colour-science's D65 gamut area measured in colour-science's
pipeline; alwan's own is 8130.95, so that ratio cannot give D65 the `CCT_f = 1`
it has by definition. Renormalising does not rescue it: alwan computes illuminant
A's reference gamut as 7968, *smaller* than its own D65 area, while
colour-science's `CCT_f` for A is ~1.0, meaning its A gamut is *larger*. That is
a qualitative disagreement about the samples, not a scale factor, and it was not
isolated.

A wrong factor corrupts D65 and illuminant A, which are the first values anyone
checks. This is an open item, not a settled preference.

### Light-quality metrics integrate 360-830 nm by trapezoid

colour-science's `cfi2017` integrates 380-780 nm by summation. alwan integrates
its full 360-830 nm 5 nm grid with the trapezoid rule, because that is the grid
every other spectral table in the library uses.

**Cost:** the residual in TM-30 (mean 0.51) and CQS (mean 0.447) is mostly this.
It is convention, not algorithm.

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

`ALWAN_SAMPLE_BILINEAR` and `ALWAN_SAMPLE_CATMULL_ROM` are pinned in the enum but
not implemented; they return `ALWAN_E_INVALID`. Passing a mode a table's rank
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

## Known limitations, not decisions

Listed here so they are not mistaken for the above.

- **CQS CCT factor**, above.
- **Hunt inverse** is not implemented.
- **`ALWAN_EMBED_DATA=0`** (runtime data loading) is not implemented and
  `#error`s rather than misbehaving.
