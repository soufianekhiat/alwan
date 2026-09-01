# Picture Formation

**A family of spatial gamut-formation operators unique to alwan.**

Most colour libraries stop at *gamut mapping*: pushing out-of-gamut colour back inside the
display volume. alwan additionally ships **picture-formation** operators: view transforms that
turn an open-domain (scene-referred, wide-gamut, unbounded) light field into a formed picture,
developed in an extended correspondence with **Troy Sobotka**, the author of AgX. Instead of
tone curves bolted onto a gamut clamp, they are constructed from a set of *constraints* about how
a picture must behave for human depth and form cognition to read it correctly.

The reasoning behind those constraints is his, and it is written up at length in his own words:
start from *The Hitchhiker's Guide to Digital Colour* and the surrounding posts and threads. This
document does not restate the argument, it records which constraints alwan tests and what each
operator scores. Where a constraint is stated below without a source, the source is that work.

This document is specific to those operators. For ordinary gamut mapping see
[`gamut_mapping.md`](gamut_mapping.md); for the spatial-solver internals see
[`gamut_spatial_formation.md`](gamut_spatial_formation.md).

All methods are reached through the spatial API:

```c
alwan_gamut_spatial_params_f64 p = {0};
p.method = ALWAN_GAMUT_FORM_CHANNEL;   /* or any method below */
p.peak = 1.0; p.iterations = 120; p.s = 1.0;
alwan_gamut_map_spatial_f64(out, in, /*depth*/ NULL, W, H, &p, ctx);
```

Output is display-referred, in `[0, 1]`.

---

## The methods

| method | one line |
|---|---|
| `GRADIENT`    | global Poisson gradient-domain reconstruction; kept as the **failure baseline** (it flips) |
| `GAIN`        | per-pixel gain toward the gamut boundary |
| `DENSITY`     | film-density model; luminance-preserving |
| `SURFACE`     | surface reconstruction with an edge-aware veil |
| `OPTICAL`     | `SURFACE` + Kitaoka additive/multiplicative (light/absorption) split |
| `CARRIER`     | tonescale driven by the **carrier** `max(RGB)`; hue-agnostic, per-pixel monotone |
| `XJUNCTION`   | carrier + local veil resolving X-junction occlusion cues |
| `FLUX`        | carrier + globally diffused flux veil |
| `DENSITY_LOG` | carrier with a log/density shoulder (keeps highlight gradient) |
| `COMPLETION`  | carrier + amodal-completion veil |
| `PICTURE`     | carrier + local veil, "picture formation" reconstruction |
| `MOMENT`      | energy-rotation formation (MacAdam complementary moment plus the rotation) |
| `HEMISPHERE`  | parameter-free C2 log-logistic on the integration `I = max(RGB)` |
| `HEMISPHERE_ABS` | `HEMISPHERE` on a **fixed absolute window** (no per-frame adaptation); the nearest-complete carrier operator, 12-of-13 under the original set |
| `CHANNEL`     | **channel integration** (AgX architecture) with every constant derived from display physics |
| `WARP`        | `HEMISPHERE_ABS` + a monotone smooth **local-contrast** warp (closed-form, no solver) that restores scene curvature the tonescale flattened |
| `COMPLETE_HEMI_LOOK` | the **first 13-of-13** operator: `HEMISPHERE_ABS`'s tone with a carrier high-rail + desaturation floor that clear its lone `RATIO` failure. Look-preserving (keeps the `HEMISPHERE_ABS` window) |
| `COMPLETE`    | the same 13-of-13 operator, isophote-through-highlight-maximised: a wider carrier window (more form through the highlight, at a brighter/softer look) |

`CHANNEL` is the current recommendation: `SB2383 inset -> per-channel C2 log-logistic on a fixed
absolute window -> 18% mid-grey anchor -> matched inverse outset -> asymptotic guard rails`, every
constant a display standard (nothing scene-derived). See the enum documentation in `alwan.h` for
the full derivation.

---

## The constraints

Fifteen of the constraints are **numerically testable** and are asserted in
`tests/99_formation_constraints.c` (thirteen original + `REL`/`VEIL` from the later
correspondence, added later):

| key | constraint | why |
|---|---|---|
| `MONO`  | zero carrier (`max RGB`) flips | integration monotonicity: depth is read from a monotone light field (imagine cars driving into fog: a non-monotone mapping makes far cars read as near) |
| `GAMUT` | output lands in `[0,1]` | no silent out-of-gamut |
| `NEUT`  | a neutral input stays neutral | the achromatic axis must be fixed (no tint) |
| `PURE`  | purity collapses to white at maximal emission | both-end behaviour: the brightest emission is the "white infinity" |
| `DC`    | a black input reads black | the neutral DC must be respected; no milky veil |
| `C2`    | the tone curve has no slope kink | a C1 discontinuity manufactures an *unintentional segmentation* (a false edge) |
| `HUEA`  | channel-permutation equivariant | hue-agnostic: the integration must not be adjusted *per record* (adjusting one hue's integration forces a peculiar segmentation) |
| `INVR`  | the same patch in a different surround maps to the same output | **temporal constancy**: "the integration structure is not a creative decision"; a per-scene auto-exposure would flicker frame to frame |
| `SPAN`  | maximal-purity stimuli reach the gamut surface | the display volume must be *spanned* (at least one channel reaching zero emission) |
| `NEG`   | negative relative wattages produce a finite, valid result | wide-gamut content carries negative channels in the working basis; the formation must absorb them instead of destroying them |
| `RAIL`  | no hard clamp plateau at 0 or 1 | "zero is a discretized infinity": the approach to the boundaries must be asymptotic, or a clamp manufactures a contour |
| `SLOPE` | the gradient gain is a bump, ~0 at both energy ends | **the hemisphere**: gradients are maximal at the chosen exposure and roll to zero at both the black and white infinities |
| `RATIO` | the output chroma **direction** (linear hue) is preserved from the input | the **lum + chrom invariant**: the restore carries the linear chroma ratio through (Grassmann additivity / matched-inverse restore); a hue *rotation* invents chroma the scene never emitted. Desaturation toward white (`PURE`) is allowed; only a change of *direction* fails |
| `REL`   | an increment/decrement keeps its **sign** vs the enclosing field | polarity preservation: a black object is read as black because it stays a *decrement* against its surround; formation must not flip it (tested with and without a moderate common veil) |
| `VEIL`  | a decrement stays **readable** under a strong common veil (formed contrast >= 1/255 at `T = 0.1`) | **blackness-through-veil**: a veil `I = T*J + (1-T)*A` raises every code value, but the black reading must survive; merging the decrement into the white veil erases the object. All current methods pass, but at `T = 0.1` the formed contrast sits at ~1.3 display codes: technically readable, *practically marginal*; the column documents that margin and trips on any regression below one code |

Further constraints in the same set are **design principles or cognitive**; they have no per-frame
pass/fail and are honoured structurally rather than tested: no `Lab`/`OkLab`/`ICtCp` (the Abney
effect / "hueness" is cognitive and is not a Cartesian coordinate); nested envelopes (a local
envelope must fit inside the super-set envelope). The "in front vs part of" reading of a haze
(boundary-driven multistable segmentation) belongs to cognition rather than to any per-pixel or
global operator. (The Grassmann-linear / matched-inverse *ratio* invariant, previously listed here, is now
the numerically-tested `RATIO` column above.)

> **A correction to `PURE` from the later correspondence.** The tested `PURE` is *per-record*:
> any sufficiently bright record desaturates toward white. The correspondence later sharpened this: white-infinity
> belongs to the **additive/emissive component only** ("energy != cause != role"). A brightly *lit
> surface* is not an emitter and must **keep** its chroma; applying per-record `PURE` to surface
> radiance is exactly what turns a lit red surface salmon-pink (the pastel failure). The corrected
> constraint is not pointwise-testable; deciding *emission vs illuminated surface* needs context
> (the junction-evidence `P_T` field of the experimental operators, or an envelope decomposition).
> The `PURE` column therefore stands as a *pointwise approximation*, with this caveat on record.

---

## The constraint matrix

Measured by `tests/99_formation_constraints.c`. This table is the per-method record of which
constraints each operator honours.

```
method        MONO GAMUT  NEUT  PURE    DC    C2  HUEA  INVR  SPAN   NEG  RAIL SLOPE RATIO   REL  VEIL
GRADIENT     xacc    ok    ok    --    ok  xacc  (ok)  (ok)    ok    ok  xacc    --  xacc    ok    ok
GAIN           ok    ok    ok    --    ok  xacc    ok  (ok)    ok    ok  xacc    --    ok    ok    ok
DENSITY      xacc    ok    ok    --    ok  xacc  xacc  (ok)    ok    ok  xacc    --    ok    ok    ok
SURFACE      (ok)    ok    ok    --    ok  xacc  xacc  (ok)    ok    ok  xacc    --    ok    ok    ok
OPTICAL      xacc    ok    ok    --    ok  xacc  xacc  (ok)    ok    ok  xacc    --    ok    ok    ok
CARRIER        ok    ok    ok    ok    ok    ok    ok  (ok)    ok    ok  xacc    --    ok    ok    ok
XJUNCTION      ok    ok    ok    --    ok    ok    ok  (ok)    ok    ok  xacc    --    ok    ok    ok
FLUX           ok    ok    ok    --    ok    ok    ok  (ok)    ok    ok  xacc    --    ok    ok    ok
DENSITY_LOG    ok    ok    ok    --    ok    ok    ok  (ok)    ok    ok  xacc    --    ok    ok    ok
COMPLETION     ok    ok    ok    --    ok    ok    ok  (ok)    ok    ok  xacc    --    ok    ok    ok
PICTURE        ok    ok    ok    --    ok    ok    ok  (ok)    ok    ok  xacc    --    ok    ok    ok
MOMENT         ok    ok    ok    ok    ok    ok    ok  (ok)    ok    ok  xacc    --    ok    ok    ok
HEMISPHERE     ok    ok    ok    ok    ok    ok    ok  xacc    ok    ok  (ok)    ok    ok    ok    ok
CHANNEL      xacc    ok    ok    ok    ok    ok  xacc    ok    ok    ok    ok    ok  xacc    ok    ok
HEMI_ABS       ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok  xacc    ok    ok
WARP           ok    ok    ok    ok    ok    ok    ok  xacc    ok    ok    ok    ok  xacc    ok    ok
COMPLETE_L     ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok
COMPLETE       ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok
```

`COMPLETE_L` (= `COMPLETE_HEMI_LOOK`) and `COMPLETE` are the **only two all-`ok` rows**: the first
operators to satisfy every numerically-testable constraint at once (13-of-13 under the original set,
and still all-`ok` over the 15 columns after `REL`/`VEIL` joined). See the `COMPLETE` section below for how
they clear `HEMI_ABS`'s `RATIO`.

`REL`/`VEIL` measured all-`ok` across the family: every method maps the neutral veil scene
monotonically, so polarity survives; the caveat is the *margin*: at `T = 0.1` the formed
decrement is ~1.3 display codes for the shoulder-heavy methods, readable but near the floor. The
columns exist to document that margin and to catch any regression below one code.

`RATIO` is the **lum + chrom invariant** and it splits the family cleanly by *mechanism*:
- **ratio-preserve / luminance-scale methods keep it** (`ok`): every carrier method plus the
  luminance family scale chroma without turning it, so `out_chroma parallel to in_chroma`.
- **the per-channel / matrix operators rotate it** (`xacc`): `CHANNEL`'s inset crosstalk and
  `GRADIENT`'s independent per-channel Poisson both invent a hue the scene never emitted. This is
  the same mechanism that costs them `HUEA`/`MONO`; the hue rotation is that mechanism made visible.
- **`HEMI_ABS` is the surprise**: `xacc`. Its reconstruction is exactly ratio-parallel, but its
  **asymptotic rails** (the feature that earns `RAIL`) remap each channel independently near 0/1,
  lifting a saturated pixel's near-zero minimum off the floor and rotating its hue. For a pure
  primary this *looked* unavoidable: `SPAN` and `RATIO` both want the minimum channel at 0, while
  `RAIL` wants it asymptotically *off* 0, a three-way conflict `HEMI_ABS` spends on `RATIO`, landing
  at **12-of-13**. It is not actually irreducible: the escape from 0 only rotates hue because the
  rail is *per-channel*. `COMPLETE` (below) escapes 0 with a **uniform** desaturation instead, holds
  all three, and closes the row.

### Legend

| symbol | meaning |
|---|---|
| `ok`   | the constraint is **required** for this method and it is **satisfied** |
| `xacc` | the constraint is a **documented, by-design violation** (accepted) and it is indeed violated; shown red, but it does **not** fail the build |
| `(ok)` | a documented-violation constraint that **happens to pass** on the synthetic test image (candidate for promotion to required) |
| `--`   | not applicable to this method |
| `FAIL` | a **required** constraint that broke: a regression; this fails the build (none currently) |

The suite fails only on `FAIL` (a required constraint regressing). The accepted `xacc` cells stay
red forever without blocking. E.g. `GRADIENT` is *supposed* to flip; that is its whole purpose as
the baseline.

### Reading the matrix

- **`CHANNEL`** satisfies the display constraints the other methods cannot (`INVR` (temporal
  constancy), `RAIL`, `SLOPE`, `PURE`, `SPAN`) and pays with `xacc` on `MONO`, `HUEA`, and `RATIO`.
  That is the signature of channel integration: the per-primary inset crosstalk reorders
  `max(RGB)`, is not hue-symmetric, and rotates the chroma direction (three faces of the same
  inset), but the eye reads the result correctly (as AgX demonstrates). Depth rides the per-record
  integrations rather than the raw carrier maximum.
- **`HEMISPHERE`** honours *every* constraint except `INVR`: its geometric-mean pivot is
  scene-adaptive, so a patch's output depends on its surround. This is the one gap between the
  carrier family and a shippable absolute operator.
- The **luminance family** (`DENSITY`/`SURFACE`/`OPTICAL`) fails `HUEA` (luminance weights are not
  channel-symmetric) and `C2` (a knee), the reasons they are legacy baselines.

---

## `HEMISPHERE_ABS`: twelve of thirteen

`HEMISPHERE` was `ok` in every column but `INVR`, and it failed `INVR` for exactly the reason
`CHANNEL` was built to fix: a scene-adaptive pivot. `HEMISPHERE_ABS` is the **Absolute
Hemisphere**: the `HEMISPHERE` carrier tone with `CHANNEL`'s absolute `log2(0.18) +/- (10, 6.5)`
window, 18% mid-grey anchor, and asymptotic rails. It is:

- `MONO` [x] (per-pixel monotone function of `max(RGB)`; `out max = ms`),
- `HUEA` [x] (a function of the carrier + ratio-preserve, so channel-permutation equivariant),
- `INVR` [x] (fixed window, so no per-scene decision),
- `SLOPE` [x], `PURE` [x], `SPAN` [x], `C2` [x], `DC` [x], `NEUT` [x], `GAMUT` [x], `NEG` [x], `RAIL` [x],

i.e. **twelve of the thirteen**: the fullest carrier row *until* `COMPLETE`, and notably
*carrier-monotone* where `CHANNEL` is not. The one it misses is `RATIO`: the same asymptotic rails
that win it `RAIL` rotate a saturated pixel's hue near the boundaries (see the `RATIO` discussion
above). For a pure primary `SPAN`+`RATIO` want the minimum channel at 0 while `RAIL` wants it
asymptotically off 0, which reads as a three-way conflict, and `HEMISPHERE_ABS` spends it on
`RATIO`. That conflict is **not** irreducible, though: `COMPLETE` (next section) clears the minimum
off 0 with a *uniform* desaturation rather than a per-channel rail and keeps all three. A method
that wants the invariant the old way (e.g. `CARRIER`, `HEMISPHERE`, `MOMENT`) keeps `RATIO` but takes
a hard clamp (`RAIL` `xacc`) or a scene-adaptive pivot (`INVR` `xacc`) in return.

The deeper caveat: even these thirteen constraints are **necessary but not sufficient**. They do not
measure the perceptual quality that made channel integration win (form rendered *through* a
highlight, the continuous integration of a glow), which emerges from staggered per-channel
saturation and is precisely what costs `CHANNEL` its `MONO`/`HUEA`/`RATIO` cells. Being hue-agnostic
(`HUEA`) and carrier-monotone (`MONO`) *rules out* channel integration by construction, so
`HEMISPHERE_ABS` renders form-through-highlight more weakly than `CHANNEL`. The two operators mark
the two sides of the frontier: `HEMISPHERE_ABS` is the nearest-complete carrier operator;
`CHANNEL` trades three constraints for a perceptual property no metric here captures. Whether one
operator can hold both (all thirteen constraints *and* form-through-highlight) is taken up by
`COMPLETE` next.

---

## `COMPLETE`: the first 13-of-13 operator

The `RATIO` gap is not irreducible. `HEMISPHERE_ABS` clears its saturated minimum off 0 with
*per-channel* rails, and moving each channel independently is exactly what rotates the hue. Replace
that one mechanism with two changes and the whole matrix goes green:

1. **Carrier rail, then a single scale.** Apply the asymptotic high rail to the *carrier* `ms` (one
   scalar) before reconstruction, then scale all three channels by the same `ms / max(RGB)`. The rail
   never touches channels independently, so `out_chroma parallel to in_chroma` exactly (`RATIO`).
2. **A desaturation floor instead of a low rail.** Cap the purity-keep at `KEEPMAX < 1`, drawing every
   saturated pixel a hair toward its own achromatic level `ms`. That lifts the minimum channel off 0
   (`RAIL`) as a *uniform* chroma scale `1 - keep`: direction preserved (`RATIO`), minimum
   `= (1-keep)*ms > 0`, and saturation `~= KEEPMAX > 0.85` (`SPAN`). The escape from 0 is now uniform
   across channels, so the `SPAN`/`RATIO`/`RAIL` conflict dissolves.

Everything else is `HEMISPHERE_ABS`: carrier-monotone (`MONO`), a function of `max(RGB)` + ratios
(`HUEA`), a fixed window (`INVR`). So `COMPLETE` is **13-of-13**: the first operator to satisfy every
numerically-testable constraint, and the first all-`ok` row in the matrix. It is necessarily
**pointwise**: `INVR` demands that a patch's output not depend on its surround, which forbids any
spatial or whole-image solve; so a "global solver" here fits the operator's *fixed constants*, it
never runs per-image.

Two constant sets ship, the same operator:

- **`COMPLETE_HEMI_LOOK`** (`16.5 / 0.997 / 0.02`) keeps `HEMISPHERE_ABS`'s exact window: visually the
  same picture, now `RATIO`-clean. On the real saber/kids frames it takes `RATIO` from `0.24`/`0.35`
  (rotated) to `0.006`/`0.03` (parallel) with the isophote-through-highlight score level-or-better.
  This is the reference-look default.
- **`COMPLETE`** (`19.85 / 0.861 / 0.005`) widens the window to the isophote-through-highlight maximum
  of the 13-feasible set. It reaches `CHANNEL`-level form-through-highlight (iso `0.77`/`0.81` vs
  `CHANNEL`'s `0.75`/`0.83`) **without** `CHANNEL`'s hue rotation (`RATIO` `0.006`/`0.03` vs
  `0.81`/`0.71`), at the cost of a brighter, softer look. Pure isophote-maximisation is
  under-specified for appearance (it rewards a wider window and more desaturation), which is why the
  constants are a *design choice* and not a solver output.

This settles the open question's testable half: one operator **does** hold all thirteen. The
perceptual half (form rendered *through* a highlight) `COMPLETE` closes most of on the isophote
metric, but through tonescale width rather than the staggered per-channel saturation of true channel
integration; `HUEA` + `MONO` still rule that mechanism out by construction. What no metric here
settles is whether the eye reads `COMPLETE`'s wider-window highlight as *the same thing* channel
integration delivers; the remaining question concerns the metric, since a complete operator
now exists.

---

## `WARP`: a monotone local-contrast refinement

`WARP` takes the `HEMISPHERE_ABS` base and adds a **monotone smooth local-contrast warp** of the
log-carrier, a closed-form spatial field pass (no solver):

```
f_out = m + gamma*(f_H - m)      f_H = log2 max(base),  m = box*3 smooth pivot
gamma = 1 + b*blur(|mean-curvature of log2 max(scene)|)   (>= 0, normalized)
```

reconstructed by a per-pixel **positive ratio-preserving scale** `2^(f_out - f_H)` on all three
channels, capped by the same asymptotic high rail as `HEMISPHERE_ABS`. Because `gamma >= 0` and the
fields are smooth, the carrier stays monotone at every significant edge (`MONO` holds *by
construction*, without a penalty, a projection, or the staircase of isotonic pooling); because it is
an analytic function of the base it manufactures no plateaus. It measures **11-of-13**: the same
row as `HEMISPHERE_ABS` except that it loses `INVR` (it is a *spatial* operator, so a patch's output
depends on its surround: the same gap `HEMISPHERE` has, and the reason the veil operators are
`INVR` `xacc`) and inherits `HEMISPHERE_ABS`'s `RATIO` `xacc` (the ratio-preserving scale does
not add rotation, but the base rails already spent `RATIO`).

Its use is *visible local form* (restoring detail the tonescale compressed) while staying
carrier-monotone and artifact-free. It is **not** a shortcut to channel integration:
measured on an isophote-curvature-through-highlight score (which ranks `CHANNEL`/AgX above the
carrier family and exposes a highlight curvature-steepening in `HEMISPHERE_ABS` that `C2` cannot
see), `WARP` sits essentially level with `HEMISPHERE_ABS`. Adding spatial local contrast to a
carrier operator does not buy the form-through-highlight of staggered per-channel saturation; the
gap is structural, exactly as the caveat above states. (A hard *clip* instead of the asymptotic
rail would flatten highlights and flatter that score, but at the cost of `RATIO`; `WARP` keeps the
rail and reports the score as measured.) `WARP` is therefore the carrier family's local-contrast
option; the resolution of the open question is `COMPLETE`, above.

---

## Constraints from the global arc (experimental operators)

The matrix above is **pointwise**: `INVR` forbids the operators in it from looking at a pixel's
surround. The later correspondence developed *global* picture formation (operators that read scene
structure) and with it a second constraint set. These apply to the **experimental operators** in
`src/alwan/experimental/` (`alwan_picture_form_global_exp`, `alwan_picture_form_local_exp`,
`alwan_picture_form_evidence`, `alwan_picture_form_pure_exp`, plus the earlier single-image
`alwan_picture_form_hybrid_exp` prototype), tested in `tests/100_formation_experimental.c`:

| key | constraint | status in the experimental operators |
|---|---|---|
| `ORDER` | spatial carrier order: `sign(Deltaz)*DeltaC >= 0` between neighbouring pixels on reliable edges (`\|Deltaz\| > 0.05` stop); the spatial generalisation of `MONO` | **global: exact** (strictly monotone by construction, 0 violations); **local: approximate** (order-repair solve; residuals <= `4e-3`, sub-visible; tested) |
| `DRIFT` | a global stage adds **zero hue rotation** over its own pointwise base; the spatial generalisation of `RATIO`, and the *structural pink-immunity* guarantee | **exact, structural**: only a scalar exposure reaches the hue-stable base; measured `~4e-14 deg` (tested) |
| `GAUGE` / `NOLOW` | no unconstrained low-frequency mode (the free harmonic Poisson mode *is* the haze) | **carried by construction**: the exposure field is explicitly anchored; there is no gradient integration, so the free mode does not exist |
| *no-pink-objective* | pink metrics are regression **displays**, never terms in a loss (a pink penalty suppresses legitimately pink objects) | **carried**: the local solve minimises anchor + carrier-order only |
| *rivalry preservation* | competing scene organisations stay **soft**: genuine ambiguity is represented and never forced to a single labelling | **partial**: the evidence field is a continuous distribution `P(C)/P(T)/P(O)` (never hard labels); its entropy is not yet exposed |
| `INTG` | every local gradient interpretation participates in **one** globally-consistent integration | **partial**: the local operator uses evidence-gated smoothing; the full junction-rivalry integration (X-junction `C`/`T`/`O` posteriors, regional coherence) is prototype-validated but not yet ported |
| `PURE` (corrected) | white-infinity belongs to the **emissive component only**; lit surfaces keep chroma (see the `PURE` caveat above) | **carried by `alwan_picture_form_pure_exp`**: `COMPLETE_HEMI_LOOK`'s purity rolloff gated by the emission evidence `P_T`. A lit red surface keeps sat `0.95` where the per-record shelf pastelises it to `0.72` (tested); real frames: bulbs and neutral-glow beams still roll white, bright warm surfaces keep chroma. Carrier and hue are *exactly* the base's (the gate only scales chroma), so `ORDER`/`DRIFT` are inherited. **Known limit**: `P_T` (dark-channel lift) detects *neutral-ish* additive light; a pure-primary emitter with no neutral glow (a CG laser blade) reads as surface and keeps chroma instead of rolling; the missing measure is *per-channel emissive coherence* from Troy's diagnostic battery. The gate is **regionally coherent** (the `INTG` lesson): the emission verdict is smoothed within continuation regions behind surface-identity boundaries judged on the glow-discounted carrier `log2(max-min)`, so an object crossing a glow (a blade over a tube light) receives **one verdict along its length**, with zero white holes, instead of flipping per-pixel where the glow lifts its dark channel |
| `TCONST` | temporal stability of the latent fields (replaces `INVR` for global operators; no framewise auto-exposure) | **carried in fixed-pivot mode, measured under a synthetic camera pan** (sliding crop, identical scene content in the overlap): `global_exp` fixed-pivot is *exactly* 0, bit-stable under motion; adaptive pivot drifts <= `5e-4`; `local_exp` and `pure_exp` are **interior-stable <= `6e-3` / `1e-3`**, their residual instability confined to regions touching the moving frame edge (new content legitimately re-informs a contextual operator, the part Phase-5 temporal filtering owns). One continuity lesson: the evidence gates must be *steep clamped smoothsteps*, never hard 0/1: a hard threshold flips verdicts under sub-pixel content shifts (measured 0.20 of flicker), while a clamped smoothstep keeps clear cases exactly sealed and near-threshold pixels continuous |

Two structural notes. First, `REL`'s *sign* half is inherited free by any `ORDER`-exact operator:
if the carrier map is monotone, a decrement against its surround cannot flip, which is why the
global operator carries `REL` by construction and only the *readability* half (`VEIL`'s margin)
remains a tone-scale question. Second, strict `ORDER` monotonicity in `z` is equivalent to being a
**global tone curve**; genuine surround-dependent local adaptation is *mathematically incompatible*
with it. That is the entire design split of the trio: `global_exp` keeps `ORDER` exact and gives up
locality; `local_exp` buys evidence-structured locality and keeps `ORDER` only as a repaired,
measured approximation. No operator can have both exactly; the constraint set forces the choice.
