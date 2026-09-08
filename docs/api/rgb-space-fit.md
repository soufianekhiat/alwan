# RGB Encoding Space Fit API

Fit primaries, a white point, a transfer function, a scale and a per-channel black point to a
dataset, so the data survives quantisation to a target bit depth with the least damage in a
chosen metric.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`. Both compile by default; restrict with `ALWAN_BUILD_ONLY_F32` /
> `ALWAN_BUILD_ONLY_F64` (see [configuration](../configuration.md)).

> **Experimental surface.** The implementation is
> `src/alwan/experimental/alwan_rgb_fit_impl.inc`, included once per precision by
> `src/alwan/experimental/alwan_rgb_fit.c` through the `ALWAN_CORE_*` macros. There is no
> `api/*.c` wrapper and no core/API split: the `.inc` is the public implementation. Constants
> are inlined instead of coming from `gendata`, and the cluster sits outside the
> constraint-tested surface. Everything below describes what the code does; where the header
> comment says something else, that is called out under [Traps](#traps).

---

## Overview

The solver searches 13 parameters:

| Index | Parameter | Moved by |
|-------|-----------|----------|
| 0..5 | primaries `rx ry gx gy bx by` (CIE xy) | `step_xy` |
| 6..7 | white `wx wy` (CIE xy) | `step_xy` |
| 8 | `ln(gamma)` | `step_log_gamma` |
| 9 | `ln(scale)` | `step_log_gamma` |
| 10..12 | per-channel offset (black point, in units of the scale) | `step_xy` |

The search is Nelder-Mead with a fixed initial simplex (reflect 1.0, expand 2.0, contract 0.5,
shrink 0.5), deterministic and resumable. The simplex has at most 14 vertices, one per free
coordinate plus the start. Locked coordinates are excluded from the simplex entirely.

The result comes back in the ordinary `alwan_rgb_space_desc_{T}` (chromaticities plus Y = 1
matrices) and in `alwan_fit_tf_{T}` (kind, gamma, scale, three offsets).

### The encoding model

The decode of a channel `c` is

```
linear_c = scale * ( offset_c + (1 - offset_c) * eotf(code_c) )
```

and the encode is its inverse:

```
code_c = oetf( ( (linear_c / scale) - offset_c ) / (1 - offset_c) )
```

The offset sits inside the encoding. `alwan.h:2581` gives the short form
`code = oetf(linear / scale)`, which holds only when every offset is 0; see
[The offset is inside the encoding](#the-offset-is-inside-the-encoding).

`oetf` / `eotf` is a free power (`eotf(x) = x^gamma`) or the sRGB piecewise curve. The sRGB
branch is chosen because its decode is free on a GPU. Both directions carry their own
breakpoint:

| Direction | Below the breakpoint | Above it | Source |
|-----------|----------------------|----------|--------|
| encode (OETF) | `12.92 * x` for linear `x <= 0.0031308` | `1.055 * x^(1/2.4) - 0.055` | `inc:76-81` |
| decode (EOTF) | `x / 12.92` for code `x <= 0.04045` | `((x + 0.055) / 1.055)^2.4` | `inc:83-88` |

---

## Types

### alwan_fit_tf_kind

```c
typedef enum { ALWAN_FIT_TF_POWER = 0, ALWAN_FIT_TF_SRGB = 1, ALWAN_FIT_TF_AUTO = 2 } alwan_fit_tf_kind;
```

`AUTO` is implemented by `alwan_rgb_fit_solve_{T}` and `alwan_rgb_fit_blocks_solve_{T}`.
`alwan_rgb_fit_begin_{T}` rejects it with `ALWAN_E_INVALID` (`inc:805`). The two `evaluate`
entry points never read `params.tf`; they take the branch from `tf->kind` and treat `AUTO` as
`POWER`.

### alwan_rgb_fit_metric

```c
typedef enum { ALWAN_FIT_METRIC_OKLAB = 0, ALWAN_FIT_METRIC_ITP = 1, ALWAN_FIT_METRIC_DE2000 = 2,
               ALWAN_FIT_METRIC_DISPLAY = 3, ALWAN_FIT_METRIC_LINEAR = 4 } alwan_rgb_fit_metric;
```

| Metric | Coordinates | Reported distance | Cloud surrogate term |
|--------|-------------|-------------------|----------------------|
| `OKLAB` | Oklab of the dataset XYZ (white Y = 1) | euclidean | Jacobian norm, gain 1 |
| `ITP` | ICtCp from `XYZ * 100`, PQ, D65 assumed | `alwan_delta_e_itp`, `scalar_factor = 720` | Jacobian norm, gain 720 |
| `DE2000` | CIELAB against a hardcoded D65 `(0.95047, 1, 1.08883)` | `alwan_delta_e_2000` | CIELAB euclidean Jacobian norm, gain 1 |
| `DISPLAY` | XYZ to linear Rec.709, clamped to 0..1, sRGB OETF | euclidean on code values (0.0039 is one 8-bit code) | Jacobian norm, gain 1 |
| `LINEAR` | linear Rec.709 coordinates, no clamp, no encode | euclidean | Jacobian norm, gain 1 |

The last column covers the quantisation term of the cloud objective only. The `PRICED` clip
charge and every per-texel error of the block objective call `rf_pdist`, which is the reported
distance itself for all five metrics. See
[Where the metric is real and where it is a surrogate](#where-the-metric-is-real-and-where-it-is-a-surrogate).

`DISPLAY` and `LINEAR` both load `ALWAN_RGB_SPACE_SRGB` for the XYZ-to-Rec.709 matrix
(`inc:830-834`), so both can fail with `ALWAN_E_NODATA` when the sRGB reference data is
unavailable, and with `ALWAN_E_RANGE` when its matrices cannot be derived.

### Lock bits

```c
enum { ALWAN_FIT_LOCK_WHITE = 1, ALWAN_FIT_LOCK_PRIMARIES = 2, ALWAN_FIT_LOCK_TF = 4,
       ALWAN_FIT_LOCK_SCALE = 8, ALWAN_FIT_LOCK_OFFSET = 16 };
```

The locks do more than freeze coordinates:

- On a cold start (`space == NULL`), `LOCK_WHITE`, `LOCK_PRIMARIES` or `LOCK_SCALE` return
  `ALWAN_E_INVALID` (`inc:635`): there is nothing to lock to.
- `LOCK_TF` suppresses the golden-section gamma search in both the cold and the warm start
  (`inc:606`, `inc:630`), and is a no-op under the sRGB branch because index 8 is never free
  there (`inc:684`).
- `LOCK_OFFSET` suppresses the data-derived offset seeding (`inc:605`, `inc:629`).
- With every coordinate locked, `k == 0`: `rf_nm_step` sets `converged` and returns without
  incrementing `report.iterations` (`inc:700`).

### alwan_fit_clip_policy

```c
typedef enum { ALWAN_FIT_CLIP_FORBID = 0, ALWAN_FIT_CLIP_PRICED = 1 } alwan_fit_clip_policy;
```

`FORBID` charges the overshoot itself per sample, and once more on the single worst overshoot
in the whole dataset (`inc:284`). `PRICED` replaces the per-sample overshoot with the metric
distance between the colour and where it lands once clamped into the gamut (`inc:266-275`), and
drops the worst-overshoot term.

### alwan_fit_tf_{T}

```c
typedef struct { alwan_fit_tf_kind kind; alwan_{T} gamma; alwan_{T} scale; alwan_{T} offset[3]; } alwan_fit_tf_{T};
```

- `gamma` -- used by `POWER` only. On the sRGB branch the output is set to 0 (`inc:762`), not
  left alone.
- `scale` -- the linear value that code 1.0 reaches when the offset is 0. On input, any value
  `<= 0` (negatives included) is read as 1 (`inc:621`); a positive value outside 1e-8 .. 1e8 is
  `ALWAN_E_INVALID` (`inc:807`). The solver's own feasible band is `|ln scale| <= 20`
  (`inc:196`, `inc:231`), i.e. 2.06e-9 .. 4.85e8.
- `offset[3]` -- the per-channel black point in units of the scale. On input an offset is taken
  only when strictly `> 0` and strictly `< 0.95` (`inc:624`); anything else, including exactly
  0.95 and any negative, is silently reset to 0. The solver's feasibility bound is the
  inclusive `0 <= offset <= 0.95` (`inc:192-194`).

### alwan_rgb_fit_params_{T}

```c
typedef struct {
    int bits;                          /* 2..16 */
    int bits_channel[3];               /* per-channel depth, 0 to take `bits` */
    int fit_offset;                    /* non-zero: the black point joins the search */
    int block_size;                    /* block fit only: 2..8 */
    int index_bits;                    /* block fit only: 1..3 */
    alwan_fit_tf_kind tf;
    alwan_rgb_fit_metric metric;
    alwan_{T} percentile;
    alwan_{T} clip_weight;
    alwan_{T} srgb_margin;
    alwan_fit_clip_policy clip_policy;
    unsigned lock;
    alwan_{T} gamma_min, gamma_max;
    alwan_{T} step_xy, step_log_gamma;
    alwan_{T} tolerance;
    alwan_{T} const *weights;
} alwan_rgb_fit_params_{T};
```

Field notes beyond the header:

- `bits` is validated 2..16 unconditionally (`inc:804`), before `bits_channel` is consulted, so
  `bits = 0` with `bits_channel = {5,6,5}` is `ALWAN_E_INVALID` even though `bits` is then
  never used. The effective per-channel depth (`bits_channel[c]` when positive, else `bits`) is
  separately required to be 2..16 (`inc:824-828`), so a `bits_channel` entry of 1 is also
  `ALWAN_E_INVALID`. `maxv = (1 << b) - 1` and the code step is `1 / maxv`.
- `block_size > 0` is rejected outright by `alwan_rgb_fit_begin_{T}` (`inc:898`), so one params
  struct cannot serve both a block fit and an incremental fit.
- `percentile` is unvalidated everywhere. See
  [percentile is never validated](#percentile-is-never-validated).
- `clip_weight` is in the metric's units under `FORBID` and dimensionless under `PRICED`. 1.0
  is calibrated for OKLAB. See [clip_weight and metric magnitude](#clip_weight-and-metric-magnitude).
- `srgb_margin` is unvalidated. The AUTO test is
  `pow_score < srgb_score * (1 - srgb_margin)` on `mean_error + tail_error` of the true report
  (`inc:989-990`, `inc:1049-1050`), so a margin `>= 1` makes sRGB win unconditionally and a
  negative margin lets a strictly worse power branch win. `report.objective`, `max_error` and
  `clipped_fraction` do not enter the decision.
- `step_xy` moves the six primaries, the two white coordinates and the three offsets. Only
  indices 8 and 9 take `step_log_gamma` (`inc:689`).
- `tolerance` drives one of two ORed criteria (`inc:743-750`): the value spread test is
  relative, `fs <= tolerance * max(|f_best|, 1)`, and there is a second hardcoded absolute
  position test, `xs <= 1e-7` on the largest coordinate spread, that `tolerance` cannot
  influence in either direction. Infeasible (`1e30`) vertices are excluded from the value
  spread and included in the position spread.
- `weights` are copied at `begin` (`inc:862`), so the caller's array need not outlive the
  state. Negatives are clamped to 0 and the array is normalised to sum 1. All-zero or
  all-negative is `ALWAN_E_INVALID` (`inc:866`). In block mode they shape the cold start only;
  see [Weights in block mode](#weights-in-block-mode).

### alwan_rgb_fit_report_{T}

```c
typedef struct {
    alwan_{T} mean_error, tail_error, max_error;
    alwan_{T} clipped_fraction;
    alwan_{T} objective;
    int iterations;
    int converged;
} alwan_rgb_fit_report_{T};
```

- `mean_error` is weighted. `tail_error` and `max_error` are unweighted (`inc:205-213`); the
  weights only shape the mean.
- `clipped_fraction` in cloud mode is the weighted share of samples with a channel outside the
  normalised 0..1 code domain, with a tolerance of 1e-9 (`inc:459`, `inc:464`). In block mode
  it is the count of out-of-range channels over `used * 3`, unweighted, over the visited tiles
  (`inc:398`, `inc:430`).
- `objective` is `state->fbest`: the smooth surrogate in cloud mode, and the modelled block
  round trip in block mode, where no surrogate exists. For `alwan_rgb_fit_evaluate_{T}` it is
  the best value over the perturbation simplex rather than the value at the given space.
- `iterations` and `converged` come back 0 from both `evaluate` entry points. In
  `alwan_rgb_fit_evaluate_{T}` they are memset and never assigned (`inc:940-942`); in
  `alwan_rgb_fit_blocks_evaluate_{T}` they are assigned from the state (`inc:1020`), which
  holds 0 and 0 because `evaluate_only` takes no step (`inc:1014`) and `rf_build_simplex`
  clears `converged` (`inc:692`).

### alwan_rgb_fit_state_{T}

```c
typedef struct alwan_rgb_fit_state_{T} alwan_rgb_fit_state_{T};   /* opaque */
```

Holds the dataset cache and the simplex. Memory in cloud mode is 17 scalars per sample
(3 XYZ + 3 perceptual + 9 Jacobian + 1 weight + 1 scratch), i.e. 136 bytes per sample in f64
and 68 in f32. Block mode drops the Jacobian (`inc:844`) for 8 scalars per texel, 64 bytes in
f64. The cold start temporarily allocates a further `4n + 2` scalars for the hull
(`inc:638-639`).

---

## Functions

### alwan_rgb_fit_params_init_{T}

```c
void alwan_rgb_fit_params_init_{T}(alwan_rgb_fit_params_{T} *params);
```

NULL-safe no-op. Memsets the struct, then sets:

| Field | Default |
|-------|---------|
| `bits` | 8 |
| `tf` | `ALWAN_FIT_TF_POWER` |
| `metric` | `ALWAN_FIT_METRIC_OKLAB` |
| `percentile` | 0.999 |
| `clip_weight` | 1.0 |
| `clip_policy` | `ALWAN_FIT_CLIP_FORBID` |
| `srgb_margin` | 0.10 |
| `gamma_min`, `gamma_max` | 1.0, 3.0 |
| `step_xy` | 0.01 |
| `step_log_gamma` | 0.1 |
| `tolerance` | 1e-6 |
| `weights` | NULL |

Everything else stays zero: `bits_channel = {0,0,0}`, `fit_offset = 0`, `block_size = 0`,
`index_bits = 0`, `lock = 0`. `index_bits = 0` is not a usable default for the block entry
points, which demand 1..3; see
[params_init does not produce a usable block-fit params](#params_init-does-not-produce-a-usable-block-fit-params).

---

### alwan_rgb_fit_begin_{T}

```c
alwan_status alwan_rgb_fit_begin_{T}(alwan_rgb_fit_state_{T} **state,
                                     alwan_{T} const *data, size_t stride, size_t count,
                                     alwan_rgb_space_desc_{T} const *data_space,
                                     alwan_rgb_space_desc_{T} const *space,
                                     alwan_fit_tf_{T} const *tf,
                                     alwan_rgb_fit_params_{T} const *params,
                                     alwan_ctx *ctx);
```

Caches the dataset and sets the starting point.

**Parameters:**
- `state` -- receives the opaque state. `*state = NULL` is written at `inc:808`, after the
  argument validation, so every `ALWAN_E_INVALID` raised at `inc:803-807` (and the
  `block_size` rejection at `inc:898`) returns without touching it. Test the returned status,
  never the pointer.
- `data` -- linear RGB triplets in `data_space`.
- `stride` -- bytes between triplets. Never validated.
- `count` -- number of samples; must be >= 3.
- `data_space` -- the dataset's own space. Its `oetf` / `eotf` are ignored; when
  `has_matrices` is set, its matrices are used in place of its chromaticities. Its
  chromaticities also become the second cold-start candidate when
  `primaries_xy[1] > 0 && white_xy[1] > 0` (`inc:836-840`).
- `space` -- NULL for the analytic cold start; non-NULL for a warm start. The branch is on the
  pointer (`inc:616`), so a zeroed non-NULL descriptor is a warm start on a degenerate
  triangle and returns `ALWAN_E_RANGE`.
- `tf` -- reaches theta only when `space` is non-NULL. Its scale is range-checked either way.
- `params` -- must have `block_size == 0`.
- `ctx` -- required; its allocator is captured for the life of the state.

**Validation:** `state`, `data`, `data_space`, `params`, `ctx` non-NULL and `count >= 3`;
`bits` in 2..16; effective per-channel depth in 2..16; `params.tf` in {`POWER`, `SRGB`} (AUTO
rejected); `gamma_min > 0` and `gamma_max >= gamma_min`; a positive `tf->scale` in 1e-8 .. 1e8;
`params.block_size == 0`.

**What it caches:** XYZ (3n), perceptual coordinates (3n), the dP/dXYZ Jacobian by central
difference with `h = 1e-4 * max(|X_c|, 1e-2)` and a one-sided difference at the XYZ floor of 0
(9n, `inc:874-881`), the normalised weights (n) and a scratch array (n).

**The cold start** builds two candidates and keeps the lower objective:

1. The smallest enclosing triangle over the convex hull of the dataset's chromaticities, swept
   over 60 orientations (three support lines 120 degrees apart), with a margin of 2% of the
   hull extent floored at 1e-3. The candidate is retried at most 8 times with the margin
   doubled between attempts (`inc:661`), until the whole candidate objective comes back finite,
   which needs the matrices to derive (`inc:179`), a data peak `> 1e-9` (`inc:600`) and a gamma
   inside `gamma_min .. gamma_max` (`inc:230`). Corners are named by largest x = red, largest
   y = green, remainder = blue. The white is the mean chromaticity weighted by `weight * Y`
   (`inc:646`), which the header describes only as "weighted white". A sRGB-triangle fallback
   exists at `inc:538-541` and is unreachable: the three support normals are constructed
   exactly 120 degrees apart, so the `inc:530` degeneracy guard tests `|det| = 0.866` on every
   orientation and never fires.
2. `data_space`'s own primaries and white, when it carries chromaticities.

Each candidate takes its scale from the largest channel value the data reaches in that
triangle (`ln(peak)`, and `peak <= 1e-9` makes the candidate unusable), starts the exponent at
2.2, seeds the offsets from the data's per-channel minimum when `fit_offset` is set and
`LOCK_OFFSET` is clear, and then runs a 30-step golden section over
`ln(gamma_min) .. ln(gamma_max)` when the branch is POWER and `LOCK_TF` is clear
(`inc:599-608`).

**A warm start is not left alone either.** When the branch is POWER, `LOCK_TF` is clear and
`tf->gamma <= 0`, the same 30-step gamma search runs before the simplex (`inc:630-631`). When
`fit_offset` is set, `LOCK_OFFSET` is clear and no positive offset was supplied, the offsets
are reseeded from the data's per-channel minimum (`inc:629`). "No scale" means any
`tf->scale <= 0`, negatives included.

**Returns:** `ALWAN_OK`, or `ALWAN_E_INVALID`, `ALWAN_E_NOMEM`, `ALWAN_E_NODATA`,
`ALWAN_E_RANGE`. `ALWAN_E_RANGE` means the start itself is infeasible: every simplex vertex
scored `1e30` (`inc:887`).

---

### alwan_rgb_fit_step_{T}

```c
alwan_status alwan_rgb_fit_step_{T}(alwan_rgb_space_desc_{T} *space, alwan_fit_tf_{T} *tf,
                                    alwan_rgb_fit_report_{T} *report,
                                    alwan_rgb_fit_state_{T} *state);
```

One Nelder-Mead iteration over the free coordinates, then writes the best space so far.

**Parameters:**
- `space` -- required. Memset and rewritten on every call, including the calls that take no
  move.
- `tf` -- may be NULL. The header documents only `report` as optional.
- `report` -- may be NULL. When non-NULL, the true quantised round trip is computed after
  `space` and `tf` have been written.
- `state` -- required.

**Behaviour:**
- The move is taken only while `!state->converged` (`inc:906`). After convergence the call
  still rewrites `space`, `tf` and `report`, and returns `ALWAN_OK`, having moved nothing.
- With every coordinate locked (`k == 0`) it sets `converged` and returns without incrementing
  `report.iterations`.
- It can return `ALWAN_E_RANGE` from the true-report stage at `inc:910-911`, after `space` and
  `tf` were already overwritten at `inc:907`, leaving the caller with a written descriptor and
  an error status.
- `report.objective` is `state->fbest`; `report.iterations` and `report.converged` come from
  the state.

**Returns:** `ALWAN_OK`, `ALWAN_E_INVALID` (NULL `state` or NULL `space`), or `ALWAN_E_RANGE`.

---

### alwan_rgb_fit_restart_{T}

```c
alwan_status alwan_rgb_fit_restart_{T}(alwan_rgb_fit_state_{T} *state,
                                       alwan_{T} step_xy, alwan_{T} step_log_gamma);
```

Rebuilds the simplex around the current best with new move sizes, keeping the dataset cache:
converge coarse, tighten, go again.

**Parameters:**
- `state` -- required.
- `step_xy` -- must be strictly `> 0`. Drives the six primaries, the two white coordinates and
  the three offsets.
- `step_log_gamma` -- must be strictly `> 0`. Drives `ln(gamma)` (index 8) and `ln(scale)`
  (index 9).

Re-evaluates `k + 1` objectives. `fbest` can only improve, since vertex 0 is the current theta.
Clears `converged`. Does not reset `iterations`, which keeps counting across restarts.

**Returns:** `ALWAN_OK`, or `ALWAN_E_INVALID` when `state` is NULL or either step is `<= 0`.

---

### alwan_rgb_fit_end_{T}

```c
void alwan_rgb_fit_end_{T}(alwan_rgb_fit_state_{T} *state);
```

NULL-safe. Frees the five cached arrays and then the state, through the allocator captured from
`ctx` at `begin`. No status.

---

### alwan_rgb_fit_solve_{T}

```c
alwan_status alwan_rgb_fit_solve_{T}(alwan_rgb_space_desc_{T} *space, alwan_fit_tf_{T} *tf,
                                     alwan_rgb_fit_report_{T} *report,
                                     alwan_{T} const *data, size_t stride, size_t count,
                                     alwan_rgb_space_desc_{T} const *data_space,
                                     alwan_rgb_fit_params_{T} const *params,
                                     int max_iterations, alwan_ctx *ctx);
```

`begin`, step until converged or `max_iterations`, `end`.

**Parameters:**
- `space`, `tf` -- in-out, both required non-NULL along with `report` and `params`.
- `data`, `stride`, `count`, `data_space` -- as `begin`.
- `params` -- may carry `tf = AUTO`.
- `max_iterations` -- must be `>= 0`. The loop is `it < max_iterations && !converged`
  (`inc:957`), so 0 reports the start without moving.
- `ctx` -- required.

**Warm versus cold** is decided from the content of `space`:
`primaries_xy[1] > 0 && white_xy[1] > 0` (`inc:973`). When it decides cold, it passes NULL for
`tf` as well (`inc:955`), discarding a filled `tf`.

**Under AUTO** it solves both branches from the same start, spending `max_iterations` on each
(`inc:984`, `inc:987`), and keeps sRGB unless
`pow.mean + pow.tail < (srgb.mean + srgb.tail) * (1 - srgb_margin)`.

**Returns:** `ALWAN_OK`, `ALWAN_E_INVALID` (NULL `space`/`tf`/`report`/`params`, or
`max_iterations < 0`), or any status `begin` can return.

---

### alwan_rgb_fit_blocks_solve_{T}

```c
alwan_status alwan_rgb_fit_blocks_solve_{T}(alwan_rgb_space_desc_{T} *space, alwan_fit_tf_{T} *tf,
                                            alwan_rgb_fit_report_{T} *report,
                                            alwan_{T} const *image, int width, int height,
                                            alwan_rgb_space_desc_{T} const *data_space,
                                            alwan_rgb_fit_params_{T} const *params,
                                            int max_iterations, alwan_ctx *ctx);
```

The fit for a block-compressed texture. The objective is the modelled block round trip over the
visited tiles, mean plus percentile, with no smooth surrogate.

**Parameters:**
- `space` -- in-out. Dereferenced at `inc:1032` before any null check: NULL is a crash.
- `tf` -- in-out, null-checked at `inc:1031`.
- `report` -- out. Null-checked inside `rf_blocks_run` (`inc:1005`) on the non-AUTO path only;
  under AUTO it is written blind. See
  [blocks_solve dereferences before checking](#blocks_solve-dereferences-before-checking).
- `image` -- `width * height` interleaved triplets, tightly packed. The stride is forced to
  `3 * sizeof(alwan_{T})` (`inc:1010`), so a strided image cannot be fed in.
- `width`, `height` -- each must be `> 0` and `>= block_size`.
- `data_space`, `ctx` -- as `begin`.
- `params` -- dereferenced at `inc:1033` before any null check: NULL is a crash. `block_size`
  must be 2..8, `index_bits` 1..3. `tf = AUTO` is accepted and implemented here, contrary to
  the header's "AUTO is accepted by solve() only" (`alwan.h:2637`).
- `max_iterations` -- must be `>= 0`; per branch under AUTO.

`count` is `width * height`, and the state drops the Jacobian.

**Returns:** `ALWAN_OK`, `ALWAN_E_INVALID` (NULL `tf`/`image`/`report` on the non-AUTO path,
`max_iterations < 0`, `width`/`height` `<= 0` or below `block_size`, `block_size` outside 2..8,
`index_bits` outside 1..3), or any status `begin` can return.

---

### alwan_rgb_fit_blocks_evaluate_{T}

```c
alwan_status alwan_rgb_fit_blocks_evaluate_{T}(alwan_rgb_fit_report_{T} *report,
                                               alwan_rgb_space_desc_{T} const *space,
                                               alwan_fit_tf_{T} const *tf,
                                               alwan_{T} const *image, int width, int height,
                                               alwan_rgb_space_desc_{T} const *data_space,
                                               alwan_rgb_fit_params_{T} const *params,
                                               alwan_ctx *ctx);
```

Score any space on the same block round trip, fitted or not. `space`, `tf` and `params` are
copied locally (`inc:1062-1064`), so the `const` contract holds.

**Parameters:**
- `report` -- out, required. Receives the block round trip of the given space.
- `space`, `tf` -- the space being scored, copied locally before the run. Both required.
- `image`, `width`, `height` -- as `alwan_rgb_fit_blocks_solve_{T}`; NULL `image` and a
  geometry outside the block rules are rejected at `inc:1005-1008`.
- `data_space`, `ctx` -- as `begin`.
- `params` -- required. `block_size` and `index_bits` still apply; `tf` and `lock` are
  overridden below.

**Behaviour:**
- `params.tf` is overridden from `tf->kind` (`AUTO` becomes `POWER`, `inc:1065`).
- `params.lock` is overwritten with `WHITE | PRIMARIES | TF | SCALE | OFFSET` (`inc:1066`), so
  `k == 0` and the given space is measured exactly as handed in. This is the reference lock set
  for a baseline score.
- `gamma_min` / `gamma_max` are widened around `tf->gamma` so the exponent is feasible.
- Runs with `max_iterations = 0` and `evaluate_only = 1` (`inc:1071`).
- `report.iterations` and `report.converged` come back 0.

**Returns:** `ALWAN_OK`, `ALWAN_E_INVALID` (NULL `report`/`space`/`tf`/`params`/`image`, or the
block geometry checks above), or any status `begin` can return.

---

### alwan_rgb_fit_evaluate_{T}

```c
alwan_status alwan_rgb_fit_evaluate_{T}(alwan_rgb_fit_report_{T} *report,
                                        alwan_rgb_space_desc_{T} const *space,
                                        alwan_fit_tf_{T} const *tf,
                                        alwan_{T} const *data, size_t stride, size_t count,
                                        alwan_rgb_space_desc_{T} const *data_space,
                                        alwan_rgb_fit_params_{T} const *params,
                                        alwan_ctx *ctx);
```

Score any space on the dataset, fitted or not.

**Parameters:**
- `report` -- out, required. Receives the true quantised round trip.
- `space`, `tf` -- the space being scored. Copied into the params and passed as the warm start;
  both required.
- `data`, `stride`, `count`, `data_space` -- as `begin`.
- `params` -- required. `tf` and `lock` are overridden below.
- `ctx` -- required.

**Behaviour:**
- `params.tf` is overridden from `tf->kind` (`AUTO` becomes `POWER`, `inc:932`).
- `params.lock` is overwritten with `WHITE | PRIMARIES | TF` only (`inc:933`). `SCALE` stays
  free always, and the offsets stay free whenever `params.fit_offset` is non-zero, so the
  scored space can differ from the one passed in. See
  [evaluate leaves the scale free](#evaluate-leaves-the-scale-free).
- `gamma_min` / `gamma_max` are widened around `tf->gamma`.
- Runs a full `begin`, which allocates the 9n Jacobian for a call that only needs a round trip,
  then frees the state.
- `report.iterations` and `report.converged` come back 0.

**Returns:** `ALWAN_OK`, `ALWAN_E_INVALID` (NULL `report`/`space`/`tf`/`params`), or any status
`begin` can return.

---

## Usage Example

One-shot cloud fit:

```c
alwan_ctx *ctx = alwan_create(NULL);

alwan_rgb_space_desc_f64 data_space;
alwan_rgb_get_space_descriptor_f64(&data_space, ALWAN_RGB_SPACE_SRGB, ctx);

alwan_rgb_fit_params_f64 params;
alwan_rgb_fit_params_init_f64(&params);
params.bits = 8;
params.metric = ALWAN_FIT_METRIC_OKLAB;

alwan_rgb_space_desc_f64 space;
alwan_fit_tf_f64 tf;
alwan_rgb_fit_report_f64 report;
memset(&space, 0, sizeof space);   /* zeroed: solve() reads it as a cold start */
memset(&tf, 0, sizeof tf);

alwan_status st = alwan_rgb_fit_solve_f64(&space, &tf, &report,
                                          samples, 3 * sizeof(alwan_f64), count,
                                          &data_space, &params, 400, ctx);
if (st == ALWAN_OK) {
    printf("mean %.4f  tail %.4f  clipped %.4f  in %d iterations\n",
           report.mean_error, report.tail_error, report.clipped_fraction, report.iterations);
    printf("gamma %.4f  scale %.6f\n", tf.gamma, tf.scale);
}
```

The same fit, resumable, coarse then tightened:

```c
alwan_rgb_fit_state_f64 *state = NULL;

/* NULL space is the analytic cold start. A zeroed non-NULL descriptor is a warm start on a
 * degenerate triangle and returns ALWAN_E_RANGE. */
alwan_status st = alwan_rgb_fit_begin_f64(&state, samples, 3 * sizeof(alwan_f64), count,
                                          &data_space, NULL, NULL, &params, ctx);
if (st != ALWAN_OK) return st;     /* state is written only from the first allocation on */

alwan_rgb_space_desc_f64 space;
alwan_fit_tf_f64 tf;
alwan_rgb_fit_report_f64 report;

report.converged = 0;
for (int it = 0; it < 500 && !report.converged; it++) {
    st = alwan_rgb_fit_step_f64(&space, &tf, &report, state);
    if (st != ALWAN_OK) break;     /* space and tf may already be written */
}

alwan_rgb_fit_restart_f64(state, 0.002, 0.02);   /* smaller moves, same dataset cache */
report.converged = 0;                            /* restart cleared it in the state */
for (int it = 0; it < 500 && !report.converged; it++) {
    st = alwan_rgb_fit_step_f64(&space, &tf, &report, state);
    if (st != ALWAN_OK) break;
}

alwan_rgb_fit_end_f64(state);
```

A BC1 block fit, and a baseline score for the space it started from:

```c
alwan_rgb_fit_params_f64 bp;
alwan_rgb_fit_params_init_f64(&bp);
bp.bits_channel[0] = 5; bp.bits_channel[1] = 6; bp.bits_channel[2] = 5;   /* RGB565 endpoints */
bp.block_size = 4;        /* params_init leaves this 0 */
bp.index_bits = 2;        /* params_init leaves this 0, and 0 is rejected */
bp.percentile = 0.99;     /* the block path does not validate this: keep it inside 0..1 */

alwan_rgb_fit_report_f64 base, fitted;
alwan_rgb_space_desc_f64 srgb = data_space;
alwan_fit_tf_f64 srgb_tf; memset(&srgb_tf, 0, sizeof srgb_tf);
srgb_tf.kind = ALWAN_FIT_TF_SRGB; srgb_tf.scale = 1.0;

alwan_rgb_fit_blocks_evaluate_f64(&base, &srgb, &srgb_tf, image, width, height,
                                  &data_space, &bp, ctx);

alwan_rgb_space_desc_f64 bspace; memset(&bspace, 0, sizeof bspace);
alwan_fit_tf_f64 btf; memset(&btf, 0, sizeof btf);
alwan_rgb_fit_blocks_solve_f64(&bspace, &btf, &fitted, image, width, height,
                               &data_space, &bp, 200, ctx);   /* bspace and bp must be non-NULL */

printf("sRGB %.4f -> fitted %.4f\n", base.mean_error, fitted.mean_error);
```

---

## Traps

Places where the header and the implementation disagree, or where the implementation does
something the header does not mention.

### The offset is inside the encoding

`alwan.h:2581` says `code = oetf(linear / scale)`, which is right only when every offset is 0.
The header's own `alwan_fit_tf` comment (`alwan.h:2620`) gives the full decode, so the two
statements in the header contradict each other. The implementation subtracts the offset and
rescales the remainder by `1 / (1 - offset_c)` before the OETF (`inc:247-252`, repeated for the
block path at `inc:396-397`). A decoder written from the short form decodes a fitted space with
a non-zero black point wrong.

### The clip hinge starts at the black point

The header calls `clip_weight` a "penalty per unit of linear value outside 0..scale"
(`alwan.h:2640`) and `clipped_fraction` the "share of samples with a channel outside 0..scale"
(`alwan.h:2676`). Both act on the offset-normalised value (`inc:249-250`, `inc:458-459`), so the
protected band is `offset_c * scale .. scale`. With a non-zero black point, a linear value
between 0 and `offset_c * scale` is charged as an undershoot and counted as clipped.

### evaluate leaves the scale free

`alwan_rgb_fit_evaluate_{T}` does not lock the scale, so the space it scores can differ from the
space handed in. Its lock set is `WHITE | PRIMARIES | TF` (`inc:933`). `rf_begin` always builds
a simplex, perturbs each free coordinate by its step and moves theta to the best vertex
(`inc:680-696`, move at `inc:693-695`); the report is computed at that theta (`inc:941`). With
the default `step_log_gamma = 0.1` the alternative is `scale * 1.105`, and on any dataset that
overshoots the given scale the perturbed scale wins on the clip penalty. Nothing is written
back, since `space` is `const`, so the substitution is invisible. Offsets are perturbed too
when `params.fit_offset` is non-zero. `alwan_rgb_fit_blocks_evaluate_{T}` locks
`WHITE | PRIMARIES | TF | SCALE | OFFSET` (`inc:1066`) and does not have this problem.

### Two different cold-start sentinels

`alwan_rgb_fit_begin_{T}` branches on the pointer (`inc:616`): a non-NULL but zeroed descriptor
takes the warm path, produces a degenerate triangle, makes every simplex vertex infeasible, and
returns `ALWAN_E_RANGE`. A NULL pointer is the cold start. `alwan_rgb_fit_solve_{T}` and
`alwan_rgb_fit_blocks_solve_{T}` branch on the content, and only on `primaries_xy[1]` (the red
y) and `white_xy[1]` (`inc:973`, `inc:1032`), so a fully populated descriptor whose red y
happens to be 0 is read as cold.

### A cold start in solve drops the tf

When `solve()` decides the start is cold it passes `warm ? space : NULL, warm ? tf : NULL`
(`inc:955`), so a filled `tf` with a zeroed descriptor loses the scale, the gamma and the
offsets. `begin()` behaves the same way for a different reason: `tf` reaches theta only inside
the `if (space)` branch (`inc:616-633`), so `begin(space = NULL, tf = &filled)` discards its
gamma, scale and offsets. The scale is still range-validated at `inc:807`, so a cold start
whose `tf->scale` sits outside 1e-8 .. 1e8 returns `ALWAN_E_INVALID` for a value that is then
never used.

### The sRGB branch carries a hidden 2.2 exponent

The header never mentions an exponent on the sRGB branch (`alwan.h:2571-2581`), but
`rf_gamma_of` returns a hardcoded 2.2 for it (`inc:184-186`) and that value is tested against
`gamma_min` / `gamma_max` like any other (`inc:230`, `inc:377`). `tf = SRGB` with
`gamma_min = 2.4` (or `gamma_max = 2.1`) makes every candidate infeasible and
`begin()` / `solve()` return `ALWAN_E_RANGE` with nothing pointing at the exponent bounds as the
cause.

### blocks_solve dereferences before checking

`alwan_rgb_fit_blocks_solve_{T}` validates `tf` and `max_iterations` only (`inc:1031`), then
reads `space->primaries_xy[1]` (`inc:1032`) and `params->tf` (`inc:1033`). NULL in either is a
crash, not `ALWAN_E_INVALID`. NULL `report` is `ALWAN_E_INVALID` on the non-AUTO path
(`inc:1005`), and a crash under AUTO, where `rf_blocks_run` is handed the locals `&r_pow` and
`&r_srgb` (`inc:1042`, `inc:1046`) and the caller's pointer is written blind at
`inc:1050-1051`. `solve()`, `evaluate()` and `blocks_evaluate()` all null-check first.

### clip_weight changes meaning with clip_policy

Under `FORBID` the charge is the raw overshoot in units of the scale, so `clip_weight` is the
scale-units-to-metric-units conversion, and a second charge on the single worst overshoot is
added to the objective (`inc:284`). Under `PRICED` the charge is already a metric distance and
it replaces the overshoot rather than adding to it (`inc:266-275`), so `clip_weight` is a
dimensionless multiplier and the single-worst-overshoot term is dropped entirely.

### Where the metric is real and where it is a surrogate

In cloud mode under `FORBID`, the per-sample term is the metric gain times the Jacobian norm
(`inc:238`, `inc:276`), where the gain is 720 for ITP and 1 for everything else
(`inc:156-158`). For `DE2000` that term is a CIELAB euclidean quantity, and
`alwan_delta_e_2000` (`inc:143-147`) is reached only by the true report (`inc:467`).

Two paths break that rule:

- Under `PRICED` the per-sample clip charge is `rf_pdist` (`inc:274`), which dispatches to
  `alwan_delta_e_2000` for DE2000 and `alwan_delta_e_itp` for ITP. That part of the objective
  is the real metric.
- In block mode the whole objective is `rf_pdist`, once per texel (`inc:414`), so a block
  DE2000 fit descends real dE2000 and a block ITP fit descends real dE ITP, with no surrogate
  anywhere.

### clip_weight and metric magnitude

The header advises raising `clip_weight` for "a metric whose numbers run larger (DISPLAY, ITP)"
(`alwan.h:2642-2645`). The list is wrong at both ends:

- `DISPLAY` coordinates are sRGB code values clamped to 0..1 before the OETF (`inc:110-118`), so
  a DISPLAY distance is bounded by sqrt(3) = 1.73 and one 8-bit code is 0.0039. That is the same
  order as Oklab, and `clip_weight` does not need raising for it.
- `ITP` multiplies the ICtCp coordinate distance by `scalar_factor = 720` (`inc:141`) and the
  surrogate carries the same gain (`inc:157`), so its numbers do run far larger.
- `DE2000` runs on CIELAB, whose `L` spans 0..100 against Oklab's 0..1, and is missing from the
  header's list. It needs `clip_weight` raised for exactly the reason the header gives.

### Nothing chromatically adapts the dataset

The ITP metric multiplies the dataset XYZ by 100 before a PQ-based ICtCp conversion
(`inc:120-124`) with `use_pq = 1`, whose XYZ is assumed D65-adapted (`alwan.h:1238-1240`). The
DE2000 metric uses a hardcoded D65 white `(0.95047, 1, 1.08883)` regardless of `data_space`'s
white (`inc:126-130`). A non-D65 dataset is measured in the wrong frame under both, and ITP
pins Y = 1 to 100 cd/m2, so an HDR dataset is silently re-scaled.

### clipped_fraction counts channels in block mode

The header calls it a "weighted share of samples" (`alwan.h:2676`). The block path adds 1 per
out-of-range channel (`inc:398`) and divides by `used * 3` (`inc:430`), unweighted, over the
visited tiles only, so a texel with all three channels out of range contributes 3. The cloud
path does what the header says (`inc:464`), with an undocumented tolerance of 1e-9 in the
normalised code domain (`inc:459`) that the objective's hinge does not have.

### percentile is never validated

The cloud path treats `percentile <= 0` as "tail = 0" (`inc:208`) and clamps the index to
`n - 1` (`inc:213`). The block path indexes
`scratch[(size_t)((used-1)*percentile + 0.5)]` with no guard (`inc:426`): `percentile = 0`
yields the minimum error added to the mean, a percentile above 1 reads past the `used` prefix
(and past the allocation when the visited texels cover the whole image), and a negative
percentile casts to a huge `size_t`. `percentile` must be kept strictly inside 0..1 for block
fits.

The objective is `mean + tail` in both modes (`inc:284`, `inc:431`), plus the worst-overshoot
term under `FORBID`, so the mean is in it regardless of `percentile`. The header's "0 means
mean only" (`alwan.h:2639`) describes the cloud tail term alone; in block mode `percentile = 0`
does not drop the tail, it selects the smallest per-texel error.

### A fit result is not round-trip-safe as a warm start

The solver's own scale bound is `|ln scale| <= 20`, so it can land on 2.06e-9 .. 4.85e8, while
the input band is exactly 1e-8 .. 1e8 and anything outside it returns `ALWAN_E_INVALID`
(`inc:196` versus `inc:807`). The offset seeding clamps to exactly 0.95 (`inc:593`), while the
input acceptance is the open interval `0 < offset < 0.95` (`inc:624`), so an offset of exactly
0.95 is silently reset to 0 on the next warm start with no error. On the sRGB branch
`tf->gamma` is actively zeroed (`inc:762`), so feeding that `tf` back into a POWER solve reads
as "no gamma given" and triggers the golden-section gamma search from 2.2.

### An unsnapped exponent leaves ALWAN_TF_LINEAR in the descriptor

Under the power branch the enums are set only when the exponent is within 5e-4 of 2.2, 2.4, 2.6
or 2.8 (`inc:767-770`); otherwise `rf_desc_from_theta` leaves what it memset, which is
`oetf = eotf = ALWAN_TF_LINEAR` (`inc:162-165`). That is an active claim about the space, and a
downstream consumer will take it at face value. The exponent lives only in
`alwan_fit_tf_{T}.gamma`. `rf_write_answer` also memsets the caller's descriptor first, so any
field it carried in is wiped rather than merged.

### params_init does not produce a usable block-fit params

It leaves `block_size = 0` and `index_bits = 0`. The block entry points reject `index_bits < 1`
(`inc:1006-1008`) before `rf_begin`'s internal `index_bits > 0 ? index_bits : 2` fallback
(`inc:816`) can apply, so that default is dead code. `block_size` and `index_bits` must both be
set explicitly.

### The block objective covers at most 4000 tiles

The tile stride grows until `(width/block_size) * (height/block_size) / stride^2 <= 4000` and is
then applied in both axes (`inc:819-822`). The loops require `by + bs <= height` and
`bx + bs <= width` (`inc:388-389`), so the right and bottom remainder of an image whose
dimensions are not multiples of `block_size` is never measured. `mean_error`, `tail_error`,
`max_error`, `clipped_fraction` and `objective` are all over the visited tiles only.

### The block codec is a model of BC1

Endpoints are the two extreme texels along a covariance principal axis found by exactly 8 power
iterations (`inc:309-352`); they are rounded per channel by
`floor(clamp01(v)*maxv + 0.5) / maxv`, an exact division rather than BC1's 565 bit-replication
expansion; the palette is `levels` evenly spaced points with nearest-of-levels assignment
(`inc:359-367`); `levels` is capped at 8 (`inc:353-358`), which the `index_bits` 1..3 validation
already guarantees. There is no BC1 3-colour or punchthrough mode and no endpoint refinement.
The header's "Measured through a real BC1 codec" (`alwan.h:2728-2729`) refers to the
measurements that motivated the entry point, reported in [alwan_future](../alwan_future.md),
and not to what this objective runs.

### Weights in block mode

Weights do not enter the block error: `rf_block_error` averages uniformly (`inc:422-426`). They
still shape the analytic cold start's white, which is the mean chromaticity weighted by
`weight * Y` (`inc:646`), and that path runs in block mode whenever the start is cold, so a
weights array moves where a cold block fit begins. They are also still read for all
`width * height` entries (`inc:862`), so a shorter array is an overread, and an all-zero array
is still `ALWAN_E_INVALID` (`inc:866`).

### Smaller traps

- Only `mean_error` is weighted. `tail_error` is the percentile of an unweighted sort and
  `max_error` is a plain unweighted max (`inc:205-213`); the header's "weighted mean /
  percentile / worst" (`alwan.h:2675`) reads as if all three were.
- `LINEAR` measures linear sRGB/Rec.709 coordinates (`inc:104-108`), which are neither the
  caller's `data_space` values nor the fitted space's stored values, and it makes the fit
  depend on the sRGB reference data being loadable (`ALWAN_E_NODATA` otherwise, `inc:832`).
- Both `evaluate` entry points ignore `params.tf` entirely and take the branch from `tf->kind`,
  mapping `AUTO` to `POWER` (`inc:932`, `inc:1065`); `begin()` is the only entry point that
  rejects AUTO (`inc:805`).
- `stride` is in bytes and is never validated. 0, or anything under `3 * sizeof(alwan_{T})`,
  reads overlapping or identical samples (`inc:857`).
- `data_space`'s `oetf` / `eotf` are ignored: `alwan_rgb_to_xyz` is a matrix multiply with no
  decode, so the data must already be linear. When `data_space->has_matrices` is set, those
  matrices are used in place of its chromaticities.
- `AUTO` runs a full solve on each branch (`inc:984`, `inc:987`), so `max_iterations` is a
  per-branch budget and the wall time is roughly double.

---

## Error Codes

- `ALWAN_OK` (0) -- success.
- `ALWAN_E_INVALID` (-1) -- NULL `state`/`data`/`data_space`/`params`/`ctx`; `count < 3`;
  `bits` outside 2..16 or an effective per-channel depth outside 2..16; `params.tf` not `POWER`
  or `SRGB` in `begin()`; `gamma_min <= 0` or `gamma_max < gamma_min`; a positive `tf->scale`
  outside 1e-8 .. 1e8; `params.block_size > 0` passed to `begin()`; a cold start with
  `LOCK_WHITE`, `LOCK_PRIMARIES` or `LOCK_SCALE`; fewer than 3 samples with `X+Y+Z > 1e-9`
  (an effectively black dataset); all weights zero or negative; `max_iterations < 0`; NULL
  `space`/`tf`/`report`/`params` at the solve and evaluate entry points, except
  `alwan_rgb_fit_blocks_solve_{T}`, which dereferences `space` and `params` before any check and
  checks `report` only on the non-AUTO path (see
  [blocks_solve dereferences before checking](#blocks_solve-dereferences-before-checking)); NULL
  `state` or a non-positive step in `restart()`; NULL `state` or NULL `space` in `step()`; block
  geometry outside `block_size` 2..8, `index_bits` 1..3, `width`/`height` `<= 0` or below
  `block_size`, or a NULL `image`. The `alwan_rgb_to_xyz` failure path at `inc:860` is
  unreachable: the matrices are derived into a local copy first (`inc:850-854`), so the call
  cannot fail there.
- `ALWAN_E_NODATA` (-2) -- only from the `DISPLAY` and `LINEAR` metrics, when
  `alwan_rgb_get_space_descriptor(ALWAN_RGB_SPACE_SRGB)` fails (`inc:832`). It means the sRGB
  reference data is missing, and says nothing about the caller's dataset.
- `ALWAN_E_RANGE` (-3) -- the start is infeasible, i.e. every simplex vertex scores 1e30. That
  covers a degenerate triangle, a white outside the triangle, a chromaticity y `<= 1e-6`,
  `|ln scale| > 20`, an offset outside 0..0.95, a gamma outside `gamma_min .. gamma_max`
  (including the hardcoded 2.2 of the sRGB branch), and both cold-start candidates failing.
  Also from `alwan_rgb_derive_matrices` failing on `data_space` (`inc:852`) or on the sRGB
  display matrix (`inc:833`), and from the true report when the best theta is itself
  infeasible. In `step()` that last one arrives after `space` and `tf` were already written.
- `ALWAN_E_NOMEM` (-4) -- state allocation, the hull scratch buffers in the cold start, or any
  of the five dataset arrays.

`alwan_rgb_fit_params_init_{T}` and `alwan_rgb_fit_end_{T}` return no status and are both
NULL-safe.

---

## Limits And Hardcoded Constants

| Quantity | Value |
|----------|-------|
| Samples | `count >= 3`; the cold start also needs >= 3 samples with `X+Y+Z > 1e-9` |
| Bit depth | `bits` 2..16, effective per-channel depth 2..16; `maxv = 2^b - 1`, code step `1/maxv` |
| Quantiser | `floor(clamp01(code) * maxv + 0.5) / maxv`, round-half-up |
| Block size | 2..8 texels per tile edge (64 texels is the hard array bound) |
| Index bits | 1..3, i.e. 2..8 palette entries; `levels` further capped at 8 |
| Gamma | `gamma_min .. gamma_max`, defaults 1.0 and 3.0; the sRGB branch is tested against a hardcoded 2.2 |
| Scale, input | 1e-8 .. 1e8 when positive; `<= 0` means 1 |
| Scale, solver | `\|ln scale\| <= 20`, i.e. 2.06e-9 .. 4.85e8 |
| Offset, input | open interval `0 < offset < 0.95`, else silently 0 |
| Offset, solver | inclusive `0 <= offset <= 0.95` |
| sRGB encode breakpoint | linear 0.0031308: `12.92 * x` below, `1.055 * x^(1/2.4) - 0.055` above |
| sRGB decode breakpoint | code 0.04045: `x / 12.92` below, `((x + 0.055)/1.055)^2.4` above |
| Triangle feasibility | white strictly inside the primaries triangle, `\|signed area x2\| >= 1e-6`, every y (three primaries and the white) `> 1e-6` |
| Search dimension | 13 parameters, simplex of at most 14 vertices |
| Convergence | value spread `<= tolerance * max(\|f_best\|, 1)` OR coordinate spread `<= 1e-7` |
| Infeasible score | 1e30 (fits f32) |
| Cold-start triangle | 60 orientations, margin 2% of the hull extent with a 1e-3 floor, at most 8 attempts with the margin doubled between them |
| Gamma search | golden section, exactly 30 iterations over `ln(gamma_min) .. ln(gamma_max)` |
| Jacobian step | `h = 1e-4 * max(\|X_c\|, 1e-2)`, central difference, one-sided at the XYZ floor of 0 |
| Block subsampling | tile stride grows until `tiles / stride^2 <= 4000`, applied in both axes |
| Block principal axis | power iteration, exactly 8 iterations |
| Named-gamma snap | `\|g - {2.2, 2.4, 2.6, 2.8}\| < 5e-4` sets `ALWAN_TF_GAMMA22/24/26/28` |
| Clip tolerance in the report | 1e-9 in the normalised code domain; the objective's hinge has none |
| Memory, cloud | 17 scalars per sample (136 bytes in f64, 68 in f32) |
| Memory, block | 8 scalars per texel (64 bytes in f64, 32 in f32) |

---

## See Also

- [Color Spaces](color-spaces.md) -- `alwan_rgb_space_desc_{T}`, `alwan_rgb_derive_matrices`
- [Transfer Functions](transfer-functions.md) -- the `alwan_transfer_function` enum the fitted descriptor carries
- [Color Difference](color-difference.md) -- `alwan_delta_e_2000` and `alwan_delta_e_itp`, the reported metrics
- [Gamut Operations](gamut.md) -- what to do with colours the fitted gamut does not hold
- [Context Management](context.md) -- `alwan_create`, the allocator the state captures
- [API Conventions](../api-conventions.md) -- parameter ordering and status codes
- [Precision and Limits](../precision-and-limits.md) -- f32 versus f64 behaviour
- [Future work](../alwan_future.md) -- the BC1 measurements behind the block fit
