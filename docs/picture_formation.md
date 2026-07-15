# Picture Formation

**A family of spatial gamut-formation operators unique to alwan.**

Most colour libraries stop at *gamut mapping* — pushing out-of-gamut colour back inside the
display volume. alwan additionally ships **picture-formation** operators: view transforms that
turn an open-domain (scene-referred, wide-gamut, unbounded) light field into a formed picture,
developed in an extended correspondence with **Troy Sobotka** (AgX). They are not tone curves
bolted onto a gamut clamp — they are constructed from a set of *constraints* about how a picture
must behave for human depth and form cognition to read it correctly.

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
| `GRADIENT`    | global Poisson gradient-domain reconstruction — kept as the **failure baseline** (it flips) |
| `GAIN`        | per-pixel gain toward the gamut boundary |
| `DENSITY`     | film-density model; luminance-preserving |
| `SURFACE`     | surface reconstruction with an edge-aware veil |
| `OPTICAL`     | `SURFACE` + Kitaoka additive/multiplicative (light/absorption) split |
| `CARRIER`     | tonescale driven by the **carrier** `max(RGB)` — hue-agnostic, per-pixel monotone |
| `XJUNCTION`   | carrier + local veil resolving X-junction occlusion cues |
| `FLUX`        | carrier + globally diffused flux veil |
| `DENSITY_LOG` | carrier with a log/density shoulder (keeps highlight gradient) |
| `COMPLETION`  | carrier + amodal-completion veil |
| `PICTURE`     | carrier + local veil, "picture formation" reconstruction |
| `MOMENT`      | energy-rotation formation (MacAdam complementary moment + Troy's rotation) |
| `HEMISPHERE`  | parameter-free C2 log-logistic on the integration `I = max(RGB)` |
| `CHANNEL`     | **channel integration** (AgX architecture) with every constant derived from display physics |

`CHANNEL` is the current recommendation: `SB2383 inset → per-channel C2 log-logistic on a fixed
absolute window → 18% mid-grey anchor → matched inverse outset → asymptotic guard rails`, every
constant a display standard (nothing scene-derived). See the enum documentation in `alwan.h` for
the full derivation.

---

## The constraints (Troy Sobotka)

Thirteen of the constraints are **numerically testable** and are asserted in
`tests/99_formation_constraints.c`:

| key | constraint | why |
|---|---|---|
| `MONO`  | zero carrier (`max RGB`) flips | integration monotonicity: depth is read from a monotone light field (imagine cars driving into fog — a non-monotone mapping makes far cars read as near) |
| `GAMUT` | output lands in `[0,1]` | no silent out-of-gamut |
| `NEUT`  | a neutral input stays neutral | the achromatic axis must be fixed (no tint) |
| `PURE`  | purity collapses to white at maximal emission | both-end behaviour: the brightest emission is the "white infinity" |
| `DC`    | a black input reads black | the neutral DC must be respected — no milky veil |
| `C2`    | the tone curve has no slope kink | a C1 discontinuity manufactures an *unintentional segmentation* (a false edge) |
| `HUEA`  | channel-permutation equivariant | hue-agnostic: the integration must not be adjusted *per record* (adjusting one hue's integration forces a peculiar segmentation) |
| `INVR`  | the same patch in a different surround maps to the same output | **temporal constancy** — "the integration structure is not a creative decision"; a per-scene auto-exposure would flicker frame to frame |
| `SPAN`  | maximal-purity stimuli reach the gamut surface | the display volume must be *spanned* (at least one channel reaching zero emission) |
| `NEG`   | negative relative wattages produce a finite, valid result | wide-gamut content carries negative channels in the working basis; they must be absorbed, not destroyed |
| `RAIL`  | no hard clamp plateau at 0 or 1 | "zero is a discretized infinity" — the approach to the boundaries must be asymptotic, or a clamp manufactures a contour |
| `SLOPE` | the gradient gain is a bump, ~0 at both energy ends | **the hemisphere**: gradients are maximal at the chosen exposure and roll to zero at both the black and white infinities |
| `RATIO` | the output chroma **direction** (linear hue) is preserved from the input | the **lum + chrom invariant**: the restore carries the linear chroma ratio through (Grassmann additivity / matched-inverse restore); a hue *rotation* invents chroma the scene never emitted. Desaturation toward white (`PURE`) is allowed — only a change of *direction* fails |

Further constraints Troy stated are **design principles or cognitive**, not per-frame pass/fail,
and are honoured structurally rather than tested: no `Lab`/`OkLab`/`ICtCp` (the Abney effect /
"hueness" is cognitive, not a Cartesian coordinate); nested envelopes (a local envelope must fit
inside the super-set envelope). The "in front vs part of" reading of a haze — boundary-driven
multistable segmentation — Troy holds to be cognition's territory, outside any per-pixel or global
operator. (The Grassmann-linear / matched-inverse *ratio* invariant, previously listed here, is now
the numerically-tested `RATIO` column above.)

---

## The constraint matrix

Measured by `tests/99_formation_constraints.c`. This table is the per-method record of which of
Troy's constraints each operator honours.

```
method        MONO GAMUT  NEUT  PURE    DC    C2  HUEA  INVR  SPAN   NEG  RAIL SLOPE RATIO
GRADIENT     xacc    ok    ok    --    ok  xacc  (ok)  (ok)    ok    ok  xacc    --  xacc
GAIN           ok    ok    ok    --    ok  xacc    ok  (ok)    ok    ok  xacc    --    ok
DENSITY      xacc    ok    ok    --    ok  xacc  xacc  (ok)    ok    ok  xacc    --    ok
SURFACE      (ok)    ok    ok    --    ok  xacc  xacc  (ok)    ok    ok  xacc    --    ok
OPTICAL      xacc    ok    ok    --    ok  xacc  xacc  (ok)    ok    ok  xacc    --    ok
CARRIER        ok    ok    ok    ok    ok    ok    ok  (ok)    ok    ok  xacc    --    ok
XJUNCTION      ok    ok    ok    --    ok    ok    ok  (ok)    ok    ok  xacc    --    ok
FLUX           ok    ok    ok    --    ok    ok    ok  (ok)    ok    ok  xacc    --    ok
DENSITY_LOG    ok    ok    ok    --    ok    ok    ok  (ok)    ok    ok  xacc    --    ok
COMPLETION     ok    ok    ok    --    ok    ok    ok  (ok)    ok    ok  xacc    --    ok
PICTURE        ok    ok    ok    --    ok    ok    ok  (ok)    ok    ok  xacc    --    ok
MOMENT         ok    ok    ok    ok    ok    ok    ok  (ok)    ok    ok  xacc    --    ok
HEMISPHERE     ok    ok    ok    ok    ok    ok    ok  xacc    ok    ok  (ok)    ok    ok
CHANNEL      xacc    ok    ok    ok    ok    ok  xacc    ok    ok    ok    ok    ok  xacc
HEMI_ABS       ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok    ok  xacc
```

`RATIO` is the **lum + chrom invariant** and it splits the family cleanly by *mechanism*:
- **ratio-preserve / luminance-scale methods keep it** (`ok`) — every carrier method plus the
  luminance family scale chroma without turning it, so `out_chroma ∥ in_chroma`.
- **the per-channel / matrix operators rotate it** (`xacc`) — `CHANNEL`'s inset crosstalk and
  `GRADIENT`'s independent per-channel Poisson both invent a hue the scene never emitted. This is
  the same mechanism that costs them `HUEA`/`MONO`; the hue rotation is that mechanism made visible.
- **`HEMI_ABS` is the surprise** — `xacc`. Its reconstruction is exactly ratio-parallel, but its
  **asymptotic rails** (the feature that earns `RAIL`) remap each channel independently near 0/1,
  lifting a saturated pixel's near-zero minimum off the floor and rotating its hue. For a pure
  primary this is provably unavoidable: `SPAN` and `RATIO` both want the minimum channel at 0,
  while `RAIL` wants it asymptotically *off* 0 — a three-way conflict no single operator resolves
  exactly. So `HEMI_ABS` is **12-of-13**, trading the invariant for its soft rails.

### Legend

| symbol | meaning |
|---|---|
| `ok`   | the constraint is **required** for this method and it is **satisfied** |
| `xacc` | the constraint is a **documented, by-design violation** (accepted) and it is indeed violated — shown red, but it does **not** fail the build |
| `(ok)` | a documented-violation constraint that **happens to pass** on the synthetic test image (candidate for promotion to required) |
| `--`   | not applicable to this method |
| `FAIL` | a **required** constraint that broke — a regression; this fails the build (none currently) |

The suite fails only on `FAIL` (a required constraint regressing). The accepted `xacc` cells stay
red forever without blocking — e.g. `GRADIENT` is *supposed* to flip; that is its whole purpose as
the baseline.

### Reading the matrix

- **`CHANNEL`** satisfies the display constraints the other methods cannot — `INVR` (temporal
  constancy), `RAIL`, `SLOPE`, `PURE`, `SPAN` — and pays with `xacc` on `MONO`, `HUEA`, and `RATIO`.
  That is the honest signature of channel integration: the per-primary inset crosstalk reorders
  `max(RGB)`, is not hue-symmetric, and rotates the chroma direction — three faces of the same
  inset — but the eye reads the result correctly (as AgX demonstrates). Depth rides the per-record
  integrations, not the raw carrier maximum.
- **`HEMISPHERE`** honours *every* constraint except `INVR` — its geometric-mean pivot is
  scene-adaptive, so a patch's output depends on its surround. This is the one gap between the
  carrier family and a shippable absolute operator.
- The **luminance family** (`DENSITY`/`SURFACE`/`OPTICAL`) fails `HUEA` (luminance weights are not
  channel-symmetric) and `C2` (a knee) — the reasons they are legacy baselines.

---

## The nearest-complete operator: `HEMISPHERE_ABS`

`HEMISPHERE` was `ok` in every column but `INVR`, and it failed `INVR` for exactly the reason
`CHANNEL` was built to fix: a scene-adaptive pivot. `HEMISPHERE_ABS` is the **Absolute
Hemisphere** — the `HEMISPHERE` carrier tone with `CHANNEL`'s absolute `log2(0.18) ± (10, 6.5)`
window, 18% mid-grey anchor, and asymptotic rails. It is:

- `MONO` ✓ (per-pixel monotone function of `max(RGB)` — `out max = ms`),
- `HUEA` ✓ (a function of the carrier + ratio-preserve, so channel-permutation equivariant),
- `INVR` ✓ (fixed window ⇒ no per-scene decision),
- `SLOPE` ✓, `PURE` ✓, `SPAN` ✓, `C2` ✓, `DC` ✓, `NEUT` ✓, `GAMUT` ✓, `NEG` ✓, `RAIL` ✓,

i.e. **twelve of the thirteen** — the fullest row in the matrix, and notably *carrier-monotone*
where `CHANNEL` is not. The one it misses is `RATIO`: the same asymptotic rails that win it `RAIL`
rotate a saturated pixel's hue near the boundaries (see the `RATIO` discussion above). For a pure
primary `SPAN`+`RATIO` want the minimum channel at 0 while `RAIL` wants it asymptotically off 0, so
no operator can hold all three at once — `HEMISPHERE_ABS` spends that irreducible conflict on
`RATIO`. A method that wants the invariant instead (e.g. `CARRIER`, `HEMISPHERE`, `MOMENT`) keeps
`RATIO` but takes a hard clamp (`RAIL` `xacc`) or a scene-adaptive pivot (`INVR` `xacc`) in return.

The deeper caveat: even these thirteen constraints are **necessary, not sufficient**. They do not
measure the perceptual quality that made channel integration win — form rendered *through* a
highlight, the continuous integration of a glow — which emerges from staggered per-channel
saturation and is precisely what costs `CHANNEL` its `MONO`/`HUEA`/`RATIO` cells. Being hue-agnostic
(`HUEA`) and carrier-monotone (`MONO`) *rules out* channel integration by construction, so
`HEMISPHERE_ABS` renders form-through-highlight more weakly than `CHANNEL`. The two operators mark
the two sides of the frontier: `HEMISPHERE_ABS` is the nearest-complete carrier operator;
`CHANNEL` trades three constraints for a perceptual property no metric here captures. Whether one
operator can hold both remains the open research question.
