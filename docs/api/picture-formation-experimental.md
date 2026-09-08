# Experimental Picture Formation API

Thirteen research entries in `src/alwan/experimental/` that form a display picture from
scene-linear RGB by driving a per-pixel exposure field into a spatial formation operator.

> **Precision variants:** every function written here as `name_{T}` exists in two forms:
> `name_f32` (single precision, `alwan_f32`) and `name_f64` (double precision, `alwan_f64`),
> with `T = f32 | f64`. Which precisions are compiled is controlled by `ALWAN_WITH_F32` /
> `ALWAN_WITH_F64` (both by default; see [../configuration.md](../configuration.md)).

> **Header corrections:** the declarations live at `alwan.h:2413-2566` (the exposure cluster)
> and `alwan.h:2752-2759` (`pure_exp`). Fourteen statements in those comment blocks are
> reconciled with `src/alwan/experimental/` in [Header corrections](#header-corrections) at the
> end of this page. The per-function sections link to the ones that change how a call behaves.

---

## Overview

| Entry | Writes | Cost class |
|-------|--------|-----------|
| `alwan_picture_form_hybrid_exp_{T}` | picture | 2 formation passes |
| `alwan_picture_form_global_exp_{T}` | picture | 1 formation pass |
| `alwan_picture_form_global_exp_field_{T}` | exposure field + optional picture | 1 formation pass |
| `alwan_picture_form_evidence_{T}` | 3-channel evidence map (diagnostic) | no formation pass |
| `alwan_picture_form_local_exp_{T}` | picture | 3 passes per iteration + 1 |
| `alwan_picture_form_local_exp_field_{T}` | exposure field + optional picture | same |
| `alwan_picture_form_local_exp_field_m_{T}` | exposure field + optional picture | same, with accelerators |
| `alwan_picture_form_local_exp_resume_{T}` | field, base, optional picture | same, per frame |
| `alwan_picture_form_local_exp_resume_m_{T}` | field, velocity, base, optional picture | same, per frame |
| `alwan_picture_form_local_exp_inputs_{T}` | carrier, base, target | no formation pass |
| `alwan_picture_form_local_exp_apply_{T}` | picture | 1 formation pass |
| `alwan_picture_form_local_exp_lag_{T}` | lagged field + level scalar | no formation pass, no allocation |
| `alwan_picture_form_pure_exp_{T}` | picture | inline tone, 2 erosions, 200 Jacobi sweeps |

All picture buffers are interleaved RGB, `width * height * 3`. All field buffers are
`width * height` scalars in stops. There is no stride parameter anywhere in the cluster.

---

## Common Contract

### Input encoding

Every entry is hard-wired to **scene-linear RGB with 0.18 as mid grey**. Some thresholds move
when the input is scaled by a constant and some do not, so a gain applied before the call
changes the picture in one part of the operator and leaves another part alone.

Scale-dependent (a uniform gain moves these):

| Threshold | Value | Units | Used by |
|-----------|-------|-------|---------|
| Carrier floor on `max(R,G,B)` | `1e-6` (z floor `-19.93` stops) | linear | all |
| Black test on `max(R,G,B)` | `1e-9` outputs exact black | linear | tone, `pure_exp` |
| Channel floor in the hybrid gate | `1e-4` | linear | `hybrid_exp` |
| Near-black pivot floor | `-7.64` stops (about `5e-3`) | absolute stops | global + local pivot |
| Tone window | `log2(0.18) - 10` to `+6.5` stops over mid grey (16.5 stops wide) | absolute stops | all formation |
| Transmission band | `smoothstep(0.30, 1.4)` on the blurred min channel | linear | `evidence`, `pure_exp` |
| `hybrid_exp` highlight gate `psi` | `smoothstep(1, 4)` on `log2(max/0.18)` | stops over mid grey | `hybrid_exp` |
| `hybrid_exp` chroma gate `phi` | `smoothstep(0.15*Tpmax, 0.55*Tpmax)` on the channel tone-slope spread | slope | `hybrid_exp` |
| `pure_exp` emissive floor band | `smoothstep(0.03, 0.15)` on `Ex` | linear | `pure_exp` |
| `pure_exp` surface-ness | `smoothstep(-3.85, -2.85)` on `log2(max - min)` | absolute stops | `pure_exp` |

Scale-invariant, away from the floors above:

| Threshold | Value | Units | Used by |
|-----------|-------|-------|---------|
| Occlusion band | `smoothstep(0.35, 1.1)` on the carrier gradient | stops **per pixel** | `evidence`, local solve, `pure_exp` |
| Order hinge | `|dz| > 0.05` | stops | local solve |
| `pure_exp` edge agreement | `1 - smoothstep(0.55, 0.85, |dzs|)` on the glow-discounted carrier | stops | `pure_exp` |
| `pure_exp` head | `smoothstep(2.5, 4.0, log2(max / max_k e12))` | stops (until the `1e-4` floor on the erosion bites) | `pure_exp` |

Multiply the input by 2 and the tone, `P_T`, the `hybrid_exp` gate and `pure_exp`'s emissive
band all move; the occlusion structure and the order hinge do not.

Several thresholds are resolution-dependent instead. The occlusion band is a per-pixel central
difference, so the same scene at half resolution produces a different region structure. The
erosions in `pure_exp` are clamped separable minima: on an axis of 31 px or fewer the radius-30
pass covers that axis end to end (13 px for the radius-12 pass), and when both collapse the same
way, on an image 13 px or smaller in both axes, `Ex = clip(e12 - e30, 0)` is 0 everywhere and the
emissive verdict is lost.

### Output range

Every internal formation call sets `peak = 1.0` along with `s = 1.0`, `beta = 8.0`,
`compress = 1.0`, `iterations = 120`. Output pictures are display-referred `[0,1]`; the
clamp is applied by the formation operator itself, by the `hybrid_exp` blend and by
`pure_exp`. There is no way to drive this cluster at an HDR peak, and the
`alwan_gamut_spatial_params_{T}.peak` field one screen earlier in the header is not reachable
from here.

### Context

`ctx` must be non-NULL for all twelve allocating entries. `ctx->alloc_fn` is dereferenced
directly and the library has no default-context fallback. `alwan_picture_form_local_exp_lag_{T}`
is the only entry with no `ctx` parameter and the only one that allocates nothing.

### Return codes

| Code | When |
|------|------|
| `ALWAN_OK` (0) | success; the only success value |
| `ALWAN_E_INVALID` (-1) | a required output pointer NULL, `in` NULL, `width <= 0` or `height <= 0`, `ctx` NULL, `e` NULL in `_apply`, `level_io` NULL in `_lag`, `dt_seconds` negative or NaN in `_lag` |
| `ALWAN_E_NOMEM` (-4) | any scratch allocation failure |

`ALWAN_E_RANGE` (-3) and `ALWAN_E_NODATA` (-2) are **never** returned by this cluster.
Out-of-range scalars are silently clamped or silently accepted. `strength` is clamped below at
0 and taken as given above, so `k >= 1` breaks the global operator's own monotonicity bound with
no diagnostic; `momentum > 0.98`, `relax > 1`, `relax <= 0`, a `warm` outside 0/1/2 and an
unrecognised `form_method` are likewise silent. `dt_seconds` in `_lag` is the only scalar in the
cluster whose range is checked.

Which output pointers are mandatory differs per entry: `global_exp_field` requires `e_out` and
not `out`; `_inputs` requires only that one of its three outputs is non-NULL; `_resume`
requires `e_io` and `base_io`; `_resume_m` requires `e_io`, `v_io` and `base_io`.

`N = (size_t)width * (size_t)height` and `N * 3 * sizeof(T)` are computed without
`alwan_safe_array_size` (`src/alwan/alwan_internal.h:37`), which the translation unit already
includes. There is no overflow guard on the dimensions.

### Argument order

> **The argument order is not consistent across the family, and every pointer is the same
> scalar type.** A swap compiles clean and runs. Passing a field where a picture is expected
> writes one third of the buffer; passing a picture where a field is expected produces garbage.

```
_global_exp_field / _local_exp_field / _local_exp_field_m   (e_out, out, in, ...)
_local_exp_resume                                           (e_io, base_io, out, in, ...)
_local_exp_resume_m                                         (e_io, v_io, base_io, out, in, ...)
_local_exp_inputs                                           (carrier_out, base_out, target_out, in, ...)
_local_exp_apply                                            (out, in, e, width, height, form_method, ctx)
_local_exp_lag                                              (e_out, level_io, e_in, width, height, warm, dt, tau_light, tau_dark)
```

`momentum` comes **before** `relax` in both `_m` signatures and both are `alwan_{T}`.
Swapping them is silent and it matters: `relax` is the accelerator that works, `momentum` is
capped at 0.98.

### In-place

`out == in` happens to work in every entry. Twelve of them stage `in` through scratch before the
single write of `out`; `pure_exp` reads and writes the same pixel index inside one pass
(`alwan_form_global_exp_impl.inc:812-836`). The header does not document it either way. The
property is incidental, so do not build on it.

---

## Base Operators

### alwan_picture_form_hybrid_exp_{T}

```c
alwan_status alwan_picture_form_hybrid_exp_{T}(alwan_{T} *out, alwan_{T} const *in,
                                               int width, int height, alwan_ctx *ctx);
```

`out = (1-q)*COMPLETE_HEMI_LOOK(in) + q*CHANNEL(in)`, with the gate `q` frozen from the
original input. Both formation passes run over the whole image at `s = 1`, `beta = 8`,
`compress = 1`, `peak = 1`, `iterations = 120`. The blend is clamped per channel to `[0,1]`.

The header defines `q` nowhere, and `q` is the operator:

```
rk,gk,bk = the input channels floored at 1e-4
q   = phi * psi
phi = smoothstep(0.15*Tpmax, 0.55*Tpmax, max(Tr,Tg,Tb) - min(Tr,Tg,Tb))
psi = smoothstep(1 stop, 4 stops, log2(max(rk,gk,bk) / 0.18))
```

`Tr/Tg/Tb` are per-channel tone slopes measured with a `+/-0.1 EV` difference on the floored
channels. `Tpmax` is the largest slope found by scanning 400 carriers over `z` in `[-14, +10]`
stops.

> **This is the one entry in the cluster that is not hue-stable.** The file's own comment above
> the solver base (`alwan_form_global_exp_impl.inc:102-105`) states that CHANNEL rotates hue
> about 3.8 degrees per 0.1 EV, and that this is why the HYBRID blend is not used as the solver
> base. The header block advertises no such caveat while every neighbouring block advertises
> structural hue safety.

**Parameters:**
- `out`, `in` -- `width * height * 3` interleaved scene-linear RGB
- `ctx` -- required, non-NULL

Allocates two `N*3` buffers (`:69-70`) and one `N` gate field (`:95`); `ALWAN_E_NOMEM` if any
fails.

---

### alwan_picture_form_evidence_{T}

```c
alwan_status alwan_picture_form_evidence_{T}(alwan_{T} *out, alwan_{T} const *in,
                                             int width, int height, alwan_ctx *ctx);
```

Diagnostic only. Writes `width * height * 3` interleaved `(P_C, P_T, P_O)`:

```
P_O = smoothstep(0.35, 1.1, |central-difference gradient of log2 max(RGB)|)
P_T = (1 - P_O) * smoothstep(0.30, 1.4, blur(min channel))
P_C = clamp(1 - P_O - P_T, 0, 1)
```

The min channel is clipped at 0 then box-blurred 5 passes at radius 2 (about sigma 3.2 px).
The carrier is floored at `1e-6`.

**Parameters:**
- `out` -- `width * height * 3` interleaved `(P_C, P_T, P_O)`
- `in` -- `width * height * 3` interleaved scene-linear RGB
- `ctx` -- required, non-NULL

Two spatial details the header omits, both of which change how the map reads:

- **At a border pixel only the gradient component perpendicular to that border is dropped**
  (`:156-158`): `gx = 0` at `x = 0` or `x = W-1`, `gy = 0` at `y = 0` or `y = H-1`, and
  `edge = sqrt(gx^2 + gy^2)`. A carrier step running parallel to the frame edge is therefore
  invisible to `P_O`, so an edge-hugging silhouette reads as continuation and the local solver's
  region gates inherit that. The four corners are the only pixels where `P_O` is forced to 0.
- **`P_T` bleeds several pixels past the lifted region**, because of the 5-pass blur.
  `pure_exp` works around this by recomputing `P_T` from the unblurred min for its own anchors
  (`:770-781`).

The three channels sum to 1 by construction, since `P_C` is the residual. They are three
readings on two different spatial supports (a 3-tap central difference and a 5-pass box blur)
rather than a decomposition of one measurement.

Four `N`-sized scratch allocations (`:172-175`).

---

## Global Exposure

### alwan_picture_form_global_exp_{T}

```c
alwan_status alwan_picture_form_global_exp_{T}(alwan_{T} *out, alwan_{T} const *in,
                                               int width, int height, int iterations,
                                               alwan_{T} strength, alwan_{T} pivot,
                                               alwan_ctx *ctx);
```

`out = COMPLETE_HEMI_LOOK(2^e0 * in)` with a scalar per-pixel exposure:

```
z    = log2(max(R,G,B)), max floored at 1e-6  (z floor -19.93 stops)
e0   = -1.0 * tanh(k * (z - zbar) / 1.0)      cap is the literal 1.0 stop
k    = max(strength, 0)                       no upper clamp
zbar = log2(pivot)               when pivot > 0
     = mean of z over z > -7.64  otherwise, falling back to the mean of all z
```

`tanh` is hand-rolled from `exp` with the argument clamped to `+/-20`. Both means accumulate
in `double` even in the f32 build. `k <= 1e-6` gives an all-zero exposure field and the plain
`COMPLETE_HEMI_LOOK` picture.

**Parameters:**
- `out`, `in` -- `width * height * 3` interleaved scene-linear RGB
- `iterations` -- **ignored**, consumed by `(void)` at `:224`
- `strength` -- compression gain `k`; clamped below at 0, unbounded above. Monotone only while
  `k < 1` (see [correction 3](#3-global_exp-is-monotone-only-while-strength--1))
- `pivot` -- linear carrier value; `> 0` anchors at the fixed `log2(pivot)` (0.18 = mid grey),
  `<= 0` uses the robust scene mean
- `ctx` -- required, non-NULL

The base method is hard-wired to `ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK` with `peak = 1.0` and
is not selectable.

---

### alwan_picture_form_global_exp_field_{T}

```c
alwan_status alwan_picture_form_global_exp_field_{T}(alwan_{T} *e_out, alwan_{T} *out,
                                                     alwan_{T} const *in,
                                                     int width, int height, int iterations,
                                                     alwan_{T} strength, alwan_{T} pivot,
                                                     alwan_ctx *ctx);
```

The same core, returning the exposure field it applied. `e_out` is `width * height` scalars in
stops, always within `+/-1` stop, such that `Y = COMPLETE_HEMI_LOOK(2^e_out * X)`.

**Parameters:**
- `e_out` -- required; `ALWAN_E_INVALID` if NULL
- `out` -- optional. When NULL the core still allocates its own `N*3` picture buffer and
  discards the result
- everything else as `alwan_picture_form_global_exp_{T}`

`e_out` is written only when the internal formation call returned `ALWAN_OK` (`:266`). On
`ALWAN_E_NOMEM` the caller's buffer is left untouched rather than zeroed.

---

## Local Exposure Solve

Six entries share one core (`alwan__local_exp_core_ex`, `:311`): the three below, the two under
[Moving pictures](#moving-pictures), and `_inputs`, which returns after stage 5. The stages, in
order:

1. Carrier `z = log2(max(R,G,B))`, floored at `1e-6`.
2. Occlusion field `P_O` from `z` (the `evidence` rule).
3. Continuation weights `cs = 1 - max(P_O_i, P_O_j)` on each lattice edge, softened by 3
   rounds of `0.6*cs + 0.4*box1(cs)`. The last column and the last row have no forward edge and
   take `cs = 0` (`:357-358`), so the frame border is a sealed boundary in the smoothing.
4. Adaptation base: weighted Jacobi on `z`, data weight 1, smoothing weight 16, conductance
   `cs^2`, for `base_sweeps` sweeps.
5. Anchor `e0 = clamp(-k*(base - bbar), +/-1 stop)`.
6. Repair loop, `iterations` times (`:443-496`): form the picture at `e`, `e+0.05` and `e-0.05`
   to get the carrier `C` and the finite-difference Jacobian `Jc = (Cp - Cm)/0.1`; gradient =
   anchor `g_i = 2*(e_i - e0_i)` with diagonal `hs_i = 2`, plus, on every 4-neighbour edge with
   `|dz| > 0.05` stop and `s*(C_i - C_j) < 0` where `s = sign(dz)`:

   ```
   co = 2*rho*(s*(C_i - C_j))*s        /* s*s = 1, so co = 2*rho*(C_i - C_j) */
   g_i  += co*Jc_i          g_j  -= co*Jc_j
   hs_i += 2*rho*Jc_i^2     hs_j += 2*rho*Jc_j^2
   ```

   `rho` ramped 30 -> 3000 over the iterations; step `0.02*g` (or `relax*g/hs`), momentum
   applied to the step, then capped at 0.15 stops, `e` clamped to `+/-3` stops.
7. Final picture `= form_method(2^e * in)` (`:499-500`).

Stages 2 to 5 are inside `if (k > 1e-6)` (`:352-401`). Stages 6 and 7 always run.

### alwan_picture_form_local_exp_{T}

```c
alwan_status alwan_picture_form_local_exp_{T}(alwan_{T} *out, alwan_{T} const *in,
                                              int width, int height, int iterations,
                                              alwan_{T} strength, alwan_{T} pivot,
                                              alwan_ctx *ctx);
```

**Parameters:**
- `out`, `in` -- `width * height * 3` interleaved scene-linear RGB. `out` is required here
  (`:516`)
- `iterations` -- repair iterations; `<= 0` selects 60. `base_sweeps` is fixed at 200 here
- `strength` -- adaptation gain, clamped below at 0. `<= 1e-6` gives the baseline picture and
  still pays every formation pass (see
  [correction 12](#12-strength--0-does-not-short-circuit-the-local-solve))
- `pivot` -- as the global entry
- `ctx` -- required, non-NULL

Base method hard-wired to `ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK` (`:512`). Allocates 18 buffers,
16 of `N` and 2 of `N*3` (`:324-341`), about `22*N*sizeof(T)`.

---

### alwan_picture_form_local_exp_field_{T}

```c
alwan_status alwan_picture_form_local_exp_field_{T}(alwan_{T} *e_out, alwan_{T} *out,
                                                    alwan_{T} const *in,
                                                    int width, int height, int iterations,
                                                    alwan_{T} strength, alwan_{T} pivot,
                                                    alwan_ctx *ctx);
```

Identical solve, returning the converged field. `momentum` and `relax` are passed as 0 and
`form_method` as `ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK`. `e_out` is written only on `ALWAN_OK`.

**Parameters:**
- `e_out` -- required, `width * height` stops; `ALWAN_E_INVALID` if NULL
- `out` -- optional. When NULL the core allocates a 19th buffer, `N*3`, for the picture it then
  discards
- everything else as `alwan_picture_form_local_exp_{T}`

To reproduce this picture later with `alwan_picture_form_local_exp_apply_{T}`, pass
`ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK` explicitly.

---

### alwan_picture_form_local_exp_field_m_{T}

```c
alwan_status alwan_picture_form_local_exp_field_m_{T}(alwan_{T} *e_out, alwan_{T} *out,
                                                      alwan_{T} const *in,
                                                      int width, int height, int iterations,
                                                      alwan_{T} strength, alwan_{T} pivot,
                                                      alwan_{T} momentum, alwan_{T} relax,
                                                      alwan_gamut_formation_method form_method,
                                                      alwan_ctx *ctx);
```

Cold-start solve with the two accelerators and a selectable base.

**Parameters:**
- `momentum` -- clamped to `[0, 0.98]`. Velocity starts at rest, is heavy-ball accumulated
  **before** the 0.15 step cap (so it can wind up), and is discarded on return
- `relax` -- clamped above at 1.0, not below. `relax <= 0` selects the fixed 0.02 step
- `form_method` -- passed straight to `alwan_gamut_map_spatial` with no validation. 0 is
  `ALWAN_GAMUT_FORM_GRADIENT`
- `e_out`, `out`, `in`, `width`, `height`, `iterations`, `strength`, `pivot`, `ctx` -- as
  `alwan_picture_form_local_exp_field_{T}`

> **`form_method` is not mentioned in this function's own header block.** A reader of
> `alwan.h:2466-2471` has no reason to pass anything but a zero, and a zero runs the
> gradient-domain Poisson baseline three times per iteration.

---

## Moving Pictures

Both entries run the seven stages listed under
[Local exposure solve](#local-exposure-solve), with `warm` deciding which of the incoming
states seed them.

### alwan_picture_form_local_exp_resume_{T}

```c
alwan_status alwan_picture_form_local_exp_resume_{T}(alwan_{T} *e_io, alwan_{T} *base_io,
                                                     alwan_{T} *out, alwan_{T} const *in,
                                                     int width, int height,
                                                     int warm, int iterations, int base_sweeps,
                                                     alwan_{T} strength, alwan_{T} pivot,
                                                     alwan_ctx *ctx);
```

One frame, carrying the exposure field and the adaptation base between calls.

**Parameters:**
- `e_io`, `base_io` -- both `width * height`, both required (`ALWAN_E_INVALID` if either is
  NULL). Both read when `warm` is non-zero (`:531` passes `base_warm = warm` verbatim), both
  written back on `ALWAN_OK`
- `out` -- optional
- `in` -- `width * height * 3` interleaved scene-linear RGB
- `warm` -- 0 cold, non-zero resumes. **This entry never reseats** (`reseat = 0` at `:531-532`),
  so `warm == 2` behaves exactly like `warm == 1`
- `iterations` -- `<= 0` selects 60; `base_sweeps` -- `<= 0` selects 200
- `strength`, `pivot`, `ctx` -- as above

Any warm frame holds the order penalty at `rho = 3000` for every iteration instead of running
the 30 -> 3000 ramp, so a warm frame is a different schedule from the still call (see
[correction 7](#7-a-warm-frame-runs-a-different-schedule-from-the-still-call)).

With `strength <= 1e-6`, `base_io` is written from uninitialised heap and then read back on
the next warm frame (see [correction 2](#2-base_out-is-uninitialised-heap-when-strength--0)).

---

### alwan_picture_form_local_exp_resume_m_{T}

```c
alwan_status alwan_picture_form_local_exp_resume_m_{T}(alwan_{T} *e_io, alwan_{T} *v_io,
                                                       alwan_{T} *base_io, alwan_{T} *out,
                                                       alwan_{T} const *in,
                                                       int width, int height,
                                                       int warm, int iterations, int base_sweeps,
                                                       alwan_{T} strength, alwan_{T} pivot,
                                                       alwan_{T} momentum, alwan_{T} relax,
                                                       alwan_gamut_formation_method form_method,
                                                       alwan_ctx *ctx);
```

The same frame with a heavy-ball velocity carried alongside the field, and a selectable base.

**Parameters:**
- `e_io`, `v_io`, `base_io` -- `width * height` each, all three required (`:568`), all three
  written back on `ALWAN_OK`
- `out` -- optional; `in` -- `width * height * 3`
- `warm` -- four independent decisions, see the table below
- `iterations` -- `<= 0` selects 60; `base_sweeps` -- `<= 0` selects 200
- `strength` -- adaptation gain, clamped below at 0; `pivot` -- as the global entry
- `momentum` -- clamped to `[0, 0.98]`, accumulated before the 0.15 step cap
- `relax` -- clamped above at 1.0, not below; `relax <= 0` selects the fixed 0.02 step
- `form_method` -- unvalidated; 0 is `ALWAN_GAMUT_FORM_GRADIENT`
- `ctx` -- required, non-NULL

`warm` decomposes into four independent decisions (`:569-570`):

| `warm` | field read | base read | velocity read | reseat |
|--------|-----------|-----------|---------------|--------|
| 0 | no | no | no (starts at rest) | no |
| 1 | yes | yes | yes | no |
| 2 | yes | **no** | **no** | yes |
| anything else | yes | no | no | no |

The reseat sets `e = clamp(e0 + (mean(e) - mean(e0)), +/-3 stops)`. The mean is preserved
exactly only while that clamp does not bite.

`momentum 0` with `relax 0` reproduces `alwan_picture_form_local_exp_resume_{T}` bit for bit at
`warm` 0 and 1 only. At `warm == 2`, and at any other non-zero value, the two diverge: `_resume`
treats every non-zero `warm` as a plain resume (base resumed, no reseat) while `_resume_m` tests
`warm == 1` and `warm == 2` separately.

---

### alwan_picture_form_local_exp_lag_{T}

```c
alwan_status alwan_picture_form_local_exp_lag_{T}(alwan_{T} *e_out, alwan_{T} *level_io,
                                                  alwan_{T} const *e_in,
                                                  int width, int height, int warm,
                                                  alwan_{T} dt_seconds,
                                                  alwan_{T} tau_light_seconds,
                                                  alwan_{T} tau_dark_seconds);
```

A first-order lag on the field's mean, applied back to every pixel as a uniform offset. The
only entry with no `ctx` and no allocation, and the only one that validates a scalar's range.

```
target = mean(e_in)
warm == 0 : level = target                       (shift is zero)
otherwise : tau   = (target < *level_io) ? tau_light_seconds : tau_dark_seconds
            alpha = tau > 0 ? 1 - exp(-dt_seconds/tau) : 1
            level = *level_io + alpha*(target - *level_io)
e_out = e_in + (level - target)                  no clamp
*level_io = level                                written on every call, cold included
```

**Parameters:**
- `e_out`, `e_in` -- `width * height` stops; may be the same buffer
- `level_io` -- one scalar carried between frames; required
- `warm` -- 0 sets the level to the solved mean; any other value filters
- `dt_seconds` -- must be `>= 0` and non-NaN, else `ALWAN_E_INVALID`. With `tau > 0`, `dt = 0`
  gives `alpha = 0`, which freezes the level and is not an error
- `tau_light_seconds`, `tau_dark_seconds` -- `<= 0` means instant in that direction. A NaN tau
  fails the `tau > 0` test and is also treated as instant, so the level jumps to the target
  whatever `dt_seconds` is

> **The mean is accumulated in `ALWAN_CORE_T`, plain `float` in the f32 build** (`:883-885`).
> The global and local pivot means deliberately accumulate in `double`; this one does not, so
> the f32 lag level loses precision on large frames.

Form the picture from `e_out` with `alwan_picture_form_local_exp_apply_{T}`.

---

## Pipeline Pieces

### alwan_picture_form_local_exp_inputs_{T}

```c
alwan_status alwan_picture_form_local_exp_inputs_{T}(alwan_{T} *carrier_out, alwan_{T} *base_out,
                                                     alwan_{T} *target_out, alwan_{T} const *in,
                                                     int width, int height,
                                                     alwan_{T} strength, alwan_{T} pivot,
                                                     alwan_ctx *ctx);
```

Runs the core with `inputs_only = 1` and returns at `:407`, before any repair iteration.
`base_sweeps` is pinned at 200 and `iterations` at 1 inside the wrapper (`:603-604`); neither is
a parameter, so this entry has no budget knob.

**Parameters:**
- `carrier_out` -- `log2(max(R,G,B))`, floor `1e-6`
- `base_out` -- the Jacobi-smoothed carrier. **Uninitialised heap when `strength <= 1e-6`**
- `target_out` -- `clamp(-strength*(base - bbar), +/-1 stop)`, with `bbar = log2(pivot)` for
  `pivot > 0` and the robust mean of the base otherwise. The header gives only the `pivot > 0`
  formula and never mentions the `pivot <= 0` mode, though the function takes `pivot`
- `in`, `width`, `height`, `ctx` -- as the solve
- `ALWAN_E_INVALID` only when all three outputs are NULL (plus the usual `in`/dimensions/`ctx`
  checks)

It allocates all 18 buffers, including both `N*3` pictures, even though it forms nothing, so its
memory cost equals the full solve's.

---

### alwan_picture_form_local_exp_apply_{T}

```c
alwan_status alwan_picture_form_local_exp_apply_{T}(alwan_{T} *out, alwan_{T} const *in,
                                                    alwan_{T} const *e,
                                                    int width, int height,
                                                    alwan_gamut_formation_method form_method,
                                                    alwan_ctx *ctx);
```

`out = form_method(2^e * in)`, a single `alwan_gamut_map_spatial` (`:856`) at `s = 1`,
`beta = 8`, `compress = 1`, `peak = 1`, `iterations = 120`.

**Parameters:**
- `out`, `in` -- `width * height * 3`; `e` -- `width * height` stops, required
- `form_method` -- unvalidated. Pass `ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK` to match the non-`_m`
  solvers. 0 is `ALWAN_GAMUT_FORM_GRADIENT`
- `ctx` -- required, non-NULL

No clamp is applied to `e` here. The `+/-3` stop clamp belongs to the solve, so a field the
caller has filtered, blended or offset goes in as written. Allocates one `N*3` buffer.

---

## PURE-Corrected Formation

### alwan_picture_form_pure_exp_{T}

```c
alwan_status alwan_picture_form_pure_exp_{T}(alwan_{T} *out, alwan_{T} const *in,
                                             int width, int height, alwan_ctx *ctx);
```

`COMPLETE_HEMI_LOOK`'s reconstruction with the purity rolloff gated by emission evidence: a
brightly lit surface keeps its chroma, additive and emissive records still roll to white.

Stages:

1. `alwan_picture_form_evidence_{T}` (`:654`).
2. Per-channel emissive coherence from two greyscale erosions at radius 12 px and 30 px:
   `Ex = sum_k clip(e12 - e30, 0) * (x_k / max)`, `head = log2(max / max_k e12)` with
   `max_k e12` floored at `1e-4` (`:688`), `coh = smoothstep(0.03, 0.15, Ex) *
   smoothstep(2.5, 4.0, head)`.
3. Lattice edges opened and sealed from the glow-discounted carrier `log2(max - min)`, with
   `max - min` floored at `1e-6` (`:712`): surface-ness `smoothstep(-3.85, -2.85)`, agreement
   `1 - smoothstep(0.55, 0.85, |dzs|)`.
4. 200 weighted-Jacobi sweeps (data weight 0.05, smoothing weight 16) on
   `max(P_T_unblurred, coh)`, clamped to `[0,1]`.
5. Final pass with `keep = (1-pt)*km + pt*kroll`, `km = 0.997`, then a `[0,1]` clamp.

**Parameters:**
- `out`, `in` -- `width * height * 3` interleaved RGB; `in` scene-linear, `out` display-referred
- `ctx` -- required, non-NULL

Negative channels are clipped to 0. `max <= 1e-9` outputs exact black.

The final pass reimplements the `COMPLETE_HEMI_LOOK` tone inline (window top `-12.47393119`,
width 16.5 stops, exponent 2.47393119, high rail 0.02, cap 0.997) rather than calling the
operator, with two rounded constants. See
[correction 13](#13-pure_exp-is-not-bit-exact-to-complete_hemi_look).

Nine allocations of its own (`:638-646`, one of `N*3`), plus the four `N`-sized scratch buffers
inside the evidence call, so peak residency is thirteen. The two erosions are `O(N*r)` with
`r = 12` and `r = 30`, per channel per axis.

---

## Header Corrections

### 1. The 0.861 purity cap belongs to a method these entries do not use

> **`alwan.h:2499-2502` attributes the 0.861 purity cap to the wrong method.** It tells you
> `ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK` has a 0.861 purity cap that desaturates every saturated
> pixel by about 14% from black up, and that `ALWAN_GAMUT_FORM_COMPLETE_PEAK` fixes it with a
> 0.997 cap. `COMPLETE_HEMI_LOOK`'s cap **is** 0.997. 0.861 belongs to plain
> `ALWAN_GAMUT_FORM_COMPLETE`, which no entry in this cluster selects by default.

`alwan_gamut_spatial_impl.inc:650` reads `keepmax = (look || top) ? 0.997 : 0.861`, with `look` =
`COMPLETE_HEMI_LOOK` and `top` = `COMPLETE_PEAK`. `:649` gives both the same 16.5-stop window.

The header names the right benefit and the wrong cause. What separates the two methods is
`w_shelf` at `:670`, `w_shelf = top ? 19.85 : w_win`, feeding the purity shelf at `:684-690`.
The shelf is written in window position (0.72 to 1.2), so pinning it to 19.85 stops moves the
desaturation onset from +1.88 to +4.29 stops over mid grey:

| Scene stops over mid grey | Display carrier | `COMPLETE_HEMI_LOOK` keep | `COMPLETE_PEAK` keep |
|---|---|---|---|
| +1.88 | 0.50 | 0.997 | 0.997 |
| +3.00 | 0.69 | 0.951 | 0.997 |
| +4.29 | 0.86 | 0.788 | 0.997 |
| +6.00 | 0.98 | 0.468 | 0.923 |

Those carriers are the interior of the display range, so the header's stated outcome, that the
interior is left alone, is substantially right; only its mechanism is misattributed.

What actually changes when you move `COMPLETE_HEMI_LOOK` to `COMPLETE_PEAK`:

| Quantity | `COMPLETE_HEMI_LOOK` | `COMPLETE_PEAK` |
|----------|----------------------|-----------------|
| Purity cap `keepmax` (`:650`) | 0.997 | 0.997 |
| Window width (`:649`) | 16.5 stops | 16.5 stops |
| Purity shelf width `w_shelf` (`:670`) | 16.5 | 19.85 |
| Purity shelf start | +1.88 stops over mid grey | +4.29 stops over mid grey |
| High-rail width `rhi` (`:651`) | 0.02 | 0.005 |
| Tone exponent (`:663-665`) | 2.47393119 (`-log2(0.18)`) | 2.559234 (derived from the pivot's normalised position) |
| 18% grey maps to | 0.1906 | 0.1800 |

For reference, plain `COMPLETE` is 19.85 / 0.861 / 0.005 (window / cap / rail).

The same misattribution is duplicated in the implementation comment at
`alwan_form_global_exp_impl.inc:106-110`, so fixing only `alwan.h:2499-2502` leaves the error in
the source.

### 2. `base_out` is uninitialised heap when `strength <= 0`

> **`strength <= 0` is documented as a legal call ("exact baseline", `alwan.h:2456`). On that
> path `base_out` and `base_io` receive raw allocator memory.** Do not read the base field
> after a zero-strength call, and do not feed it back as the next frame's adaptation base.

`alwan_form_global_exp_impl.inc:329` allocates `bs` with `ctx->alloc_fn`. Every write to `bs`
is inside `if (kad > 1e-6)` at `:352`. The copies out at `:405` (`base_out`) and `:502`
(`base_io`) are unconditional. `alwan_default_alloc` (`src/alwan/api/alwan_context.c:16-48`)
is `_aligned_malloc`/`malloc` and does not zero. The threshold is `1e-6`, so any
`strength` at or below that, including exactly 0 and any negative value, hits it.

### 3. `global_exp` is monotone only while `strength < 1`

`alwan.h:2423-2428` advertises the operator as MONOTONE with `slope >= 1-k`. `strength` is
clamped from below at 0 (`:240`) and has **no upper clamp**. At `k >= 1` the slope bound is
zero or negative and the carrier order inverts in the middle of the range, silently, with no
error. The header never states that `k < 1` is the condition for its own guarantee. The
example gain it gives is `~0.15`.

### 4. `form_method = 0` is `ALWAN_GAMUT_FORM_GRADIENT`, and it is never validated

> **A zero-initialised or `memset` `form_method` argument selects the halo-prone Poisson
> baseline, at the baked `s = 1`, `beta = 8`, `iterations = 120`, three times per repair
> iteration.** `_apply`, `_field_m` and `_resume_m` all take the value unchecked.

`alwan_gamut_map_spatial`'s dispatch is a chain of `if (p->method == X) { ...; return; }` with
the GRADIENT branch at `alwan_gamut_spatial_impl.inc:894` as the fall-through. Any
unrecognised enum value runs it. No entry in this cluster returns `ALWAN_E_INVALID` for a bad
`form_method`.

The block above `alwan_picture_form_local_exp_field_m` (`alwan.h:2466-2471`) documents `relax`
and `momentum` and never mentions that the function takes a `form_method` at all. The only
description of that argument sits 30 lines later in the `_resume_m` block, and it is the
description corrected in item 1.

### 5. `_apply` reproduces the solve only if you pass the same `form_method`

`alwan.h:2540-2542` says `_apply` is "the solve's own last step". True only when the caller
passes the method the solve used. The non-`_m` solvers pin `ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK`
internally (`:512` for both cold-start entries, `:532` for `_resume`, `:604` for `_inputs`) and
never report it. Pairing `alwan_picture_form_local_exp_field_{T}` with a zero `form_method` in
`_apply` runs GRADIENT against a field solved under `COMPLETE_HEMI_LOOK`.

### 6. `warm` is three separate booleans in `_resume_m`, and `_resume` never reseats

`alwan.h:2504-2505` documents `warm` as exactly three values. `_resume_m` consumes it as
`warm ? e_io : NULL` for the field, `warm == 1` for the base, `warm == 1` for the velocity and
`warm == 2` for the reseat (`:569-570`). Consequences:

- At `warm == 2` the velocity is reset to rest and the previous frame's adaptation base is
  discarded and recomputed from this frame's carrier. The warm-2 paragraph mentions neither.
  The reseat replaces the field with `clamp(e0 + (mean(e) - mean(e0)), +/-3 stops)`, so
  "keeps the field's mean exactly" fails whenever that clamp bites.
- Any other `int` (3, -1, a bool cast, an uninitialised flag) is a fourth undocumented mode:
  the field is read, the base and velocity start cold, no reseat. There is no range check.
- `alwan_picture_form_local_exp_resume_{T}` passes `base_warm = warm` verbatim and
  `reseat = 0` always (`:531-532`), so `warm == 2` behaves there exactly like `warm == 1`. Code
  written against `_resume_m` and switched to `_resume` ghosts across cuts with no compile
  error.

`alwan.h:2480-2481` ("both read when warm is non-zero") is accurate for `_resume`, the function
its block documents. The sentence that misleads is `alwan.h:2495-2496`, "v_io is a third
width*height state, read and written exactly like e_io": in `_resume_m`, `v_io` and `base_io`
are read only at `warm == 1` while `e_io` is read at any non-zero `warm`.

### 7. A warm frame runs a different schedule from the still call

`alwan.h:2484-2485` says that at about 60 iterations and 200 sweeps "every frame is solved
independently and the answer matches the still-picture call". Because `e_in` is non-NULL on a
warm frame, the order penalty is held at `rho = 3000` for every iteration (`:449-451`), while
the still call runs the 30 -> 3000 homotopy. The implementation comment (`:444-448`) sources
half of this: the ramp is a cold-start homotopy, and re-running it every frame "would make each
frame a different operator and leave the field permanently in motion". The rest is this page's
own reading: the objective is non-convex (the hinge acts on the carrier of a non-linear
formation operator), so the two schedules agree asymptotically and are not the same solve.

### 8. The drive is capped at one stop, everywhere

`alwan.h:2426-2427` writes the global drive as `e0 = -cap*tanh(k*(z-zbar)/cap)` without ever
giving `cap`. `alwan.h:2456` documents local `strength` with no cap at all. Only the `_inputs`
block (`alwan.h:2530`) says "capped at one stop either way".

`cap` is the literal `1.0` stop in both operators (`:242` global, `:388` local), and in the
local case `e0` is hard-clamped at `:397-398` rather than rolled. No value of `strength` buys
more than one stop of drive. The two entry points a caller reaches first are the two that do
not say so.

### 9. Two numeric rails the header never gives

`alwan.h:2506-2507` says only that "the field may only move so far per frame" and that it
"stays there for the better part of a second". The numbers:

- Per-iteration step capped at **0.15 stops** (`:442`, applied at `:493`).
- Exposure field clamped to **+/-3.0 stops** every iteration (`:494`) and in the warm-2
  reseat (`:424-425`).

The step cap is applied **after** momentum, so the velocity keeps accumulating while the step
saturates.

### 10. `relax <= 0` silently means the fixed 0.02 step

`alwan.h:2466-2471` gives no ranges. `momentum` is clamped to `[0, 0.98]` (`:436-437`): 0.99
and 1.5 both become 0.98. `relax` is clamped above at 1.0 and **not** below (`:438`); at
`:491` the step is `rx > 0 ? rx*g[i]/hs[i] : lr*g[i]`, so any `relax <= 0`, including a
negative value passed by mistake, falls back to the fixed 0.02 gradient step.

### 11. "Robust mean" is one specific rule, and only the global block names it

`alwan.h:2433-2434` describes `pivot <= 0` as "the scene-adaptive robust mean". The local block
(`alwan.h:2457-2459`) says only "<= 0 scene-adaptive" and defines nothing. The rule is: average
only pixels whose **carrier** exceeds `-7.64` stops (about `5e-3` linear), and fall back to the
mean over **all** pixels when no pixel qualifies, which is what a very dark frame gets
(`:243-251` global, `:390-395` local). In the local operator the selection test is on the
carrier `zM` while the quantity averaged is the smoothed base `bs`, so the two fields are mixed.
Both means accumulate in `double` even in the f32 build.

### 12. `strength <= 0` does not short-circuit the local solve

> **The repair loop runs `iterations` times whether or not any adaptation was asked for, and
> each iteration forms the whole picture three times.** At the default 60 iterations that is
> 181 full-image formation passes for an answer equal to the baseline.

The `kad > 1e-6` guard at `:352` covers the base and `e0` stage, so the `base_sweeps` Jacobi
sweeps at `:373` are skipped entirely at `strength <= 1e-6`. The formation passes are not: the
loop at `:443-496` is unconditional, with `alwan__exp_form_base` called at `:453`, `:456`,
`:459` (at `e`, `e+0.05`, `e-0.05` for the finite-difference Jacobian) and once more at `:500`.

The answer is the baseline only because the default base is pointwise carrier-monotone, so the
order hinge never fires at `e = 0`. The hinge tests the **output** carrier of whatever
`form_method` was passed (`:467-484`). With a base that is not carrier-monotone, and CHANNEL is
documented as such in the enum while GRADIENT is what an out-of-range value selects, the hinge
can be active at `e = 0` and move the field at `strength = 0`. The "`<=0 => exact baseline`"
claim is stated unconditionally for the `_m` entries that also take `form_method`.

### 13. `pure_exp` is not bit-exact to `COMPLETE_HEMI_LOOK`

`alwan.h:2752-2757` says the carrier and hue are "EXACTLY those of COMPLETE_HEMI_LOOK" and
that "below the purity shelf the picture is COMPLETE_HEMI_LOOK exactly". `pure_exp` does not
call the operator; it reimplements the tone inline. The shipped operator computes
`g_slope = 5.54/(10/16.5) = 9.141` (`alwan_gamut_spatial_impl.inc:654-655`); the inline
copy hard-codes `9.1419001` and `vp = 0.60606061` (`alwan_form_global_exp_impl.inc:18`, `:51`,
`:806`). Everything else matches: cap 0.997, window top `-12.47393119`, width 16.5, exponent
2.47393119, rail 0.02, shelf.

The two tones agree to `2.9e-5` absolute in the carrier at worst, near +3.0 stops over mid grey,
which is 0.0075 of an 8-bit code: below one quantisation step. A bit-exactness test against
`ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK` will still fail. The same rounded constant is used by
`hybrid_exp`'s gate.

### 14. NON-DETERMINISTIC is a scope statement

`alwan.h:2415` calls the cluster NON-DETERMINISTIC. The other two disclaimers on that line are
accurate as written: the constants are inlined (no gendata), and the entries are outside the
constraint-tested surface, since `tests/99_formation_constraints.c` never runs the formation
constraint matrix against any of them.

Outside that matrix they are tested. `tests/100_formation_experimental.c` and
`tests/108_temporal_exposure.c` exercise 12 of the 13 entries (`hybrid_exp` is the exception)
and pin the contracts this page relies on: the global and local `strength = 0` baseline identity
to `1e-15`, `|e| <= 1` stop on the returned field, the evidence partition of unity, the NULL
`e_out` / `e_io` / `base_io` / `level_io` rejections, the negative-`dt` rejection, `_apply` as
the solve's last step bit for bit, and `pure_exp`'s hue drift, carrier order and `[0,1]` range.

The determinism disclaimer describes policy. There is no RNG, no threading, no OpenMP and no
unordered reduction anywhere in `src/alwan/experimental/`; every loop is a fixed sequential
sweep, so a fixed build returns the same bytes for the same input. Read the line as "outside the
determinism contract" rather than "results vary run to run". See
[../determinism.md](../determinism.md) for what the contract covers.

---

## Cost

| Entry | Formation passes | Other |
|-------|-----------------|-------|
| `hybrid_exp` | 2 | 400-sample scan for `Tpmax`, one `q` pass |
| `global_exp`, `global_exp_field` | 1 | one carrier pass, one mean |
| `evidence` | 0 | 3-tap gradient, 5 box passes at radius 2 |
| `local_exp` and every `_field` / `_resume` variant | `3 * iterations + 1` (181 at the default 60), regardless of `strength` | `base_sweeps` Jacobi sweeps (200 by default) only when `strength > 1e-6` |
| `local_exp_inputs` | 0 | 200 Jacobi sweeps only when `strength > 1e-6`; allocates the full solve's 18 buffers either way |
| `local_exp_apply` | 1 | one exposure multiply |
| `local_exp_lag` | 0 | one mean, one add; no allocation |
| `pure_exp` | 0 (tone is inline) | evidence, 2 erosions, 200 Jacobi sweeps |

`strength <= 0` removes the Jacobi sweeps from the local solve and none of the formation passes.

---

## See Also

- [Picture Formation](../picture_formation.md) -- the constraint set these operators are
  measured against, and the reasoning behind each formation method
- [Spatial Gamut Formation](../gamut_spatial_formation.md) -- the spatial operator this cluster
  wraps
- [Gamut API](gamut.md) -- `alwan_gamut_map_spatial_{T}` and the
  `alwan_gamut_formation_method` enum
- [Context API](context.md) -- `alwan_ctx`, allocators, and why `ctx` cannot be NULL here
- [Determinism](../determinism.md) -- what the determinism contract covers, and what the
  experimental disclaimer excludes
- [Precision and Limits](../precision-and-limits.md) -- f32 vs f64 behaviour
- [Configuration](../configuration.md) -- `ALWAN_WITH_F32` / `ALWAN_WITH_F64`
