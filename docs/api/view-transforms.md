# View Transforms API

Display rendering transforms: the 18 fixed views behind `alwan_view_transform`, the
parameterized analytic AgX engine, and JP2499 (Juan Pablo Zambrano's "2499" DRT).

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`. Both twins are declared unconditionally, but each is *defined* only
> when its precision is enabled (`ALWAN_WITH_F32` / `ALWAN_WITH_F64`, set by
> `ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64`; see
> [configuration](../configuration.md)). Calling a disabled twin is a link error. Only
> the untyped `_map_interleave_ex` entry points degrade to `ALWAN_E_INVALID`.

---

## Overview

Three families share this page. They have different parameter blocks, different input
conventions, and three different `count == 0` contracts.

| Family | Entry points | Parameters | Output range |
|---|---|---|---|
| Fixed views | `alwan_view_transform_apply_{T}`, `alwan_view_transform_apply_unclamped_{T}`, `alwan_view_transform_{T}_map_interleave`, `alwan_view_transform_map_interleave_ex` | `alwan_view_transform` enum (0..17) | per transform |
| Analytic AgX | `alwan_agx_apply_{T}`, `alwan_agx_{T}_map_interleave`, `alwan_agx_map_interleave_ex` | `alwan_agx_params_{T}` | `[0, 1]` for finite input |
| JP2499 | `alwan_jp2499_apply_{T}`, `alwan_jp2499_{T}_map_interleave`, `alwan_jp2499_map_interleave_ex` | `alwan_jp2499_params_{T}` | unbounded below, up to about `Lp/100` |

**Strides are bytes**, including in the typed `_ex` entry points (the pointer arithmetic
is `(char *)p + i * stride`). No entry point validates a stride; a stride of `0` rewrites
pixel 0 `count` times.

**Names carry the precision infix in the middle** for the bulk variants:
`alwan_agx_f64_map_interleave`, `alwan_jp2499_f32_map_interleave`,
`alwan_view_transform_f64_map_interleave`, written here as
`alwan_agx_{T}_map_interleave`. The typed variants take no suffix
(`alwan_agx_map_interleave_ex`) and accept `f64` parameters only.

**`out_fmt` precedes `in_fmt`** in every `_ex` entry point. Both are
`alwan_pixel_format`, so swapping them compiles without a diagnostic and silently
mis-converts the image.

### Transform kernels are scalar

None of the three families has a SIMD lane kernel. Every transform path is a scalar
per-pixel loop over one shared core worker (`agx_render_{T}`, `jp2499_render_{T}`, or
the view dispatch in `src/alwan/api/alwan_view_impl.inc`), so map output is byte-identical
to apply output for the same precision and the same parameters.

The typed `_ex` tile *I/O* is the exception: the u8/u16 AoS store in
`src/alwan/map/alwan_tile_typed_gen.inc` is AVX2-vectorized, and on the f64 side the
vector head converts through `f32` (`_mm256_cvtpd_ps`) while the scalar tail scales in
`f64`. Head and tail of one tile can therefore round differently by 1 LSB at a `.5`
boundary on an AVX2 build.

### count == 0

> **`count == 0` is a hard error in the AgX and JP2499 map entry points and success
> everywhere else.** A tile loop that hits an empty tile fails on AgX/JP2499 and succeeds
> on views.

| Entry point | `count == 0` |
|---|---|
| `alwan_view_transform_apply_{T}`, `alwan_view_transform_apply_unclamped_{T}` | `ALWAN_OK` |
| `alwan_view_transform_{T}_map_interleave` | `ALWAN_OK` (bare forward to apply) |
| `alwan_agx_apply_{T}`, `alwan_jp2499_apply_{T}` | `ALWAN_OK` |
| `alwan_agx_{T}_map_interleave`, `alwan_jp2499_{T}_map_interleave` | `ALWAN_E_INVALID` |
| all three `_map_interleave_ex` | `ALWAN_E_INVALID` |

---

## Fixed View Transforms

```c
typedef enum {
    ALWAN_VIEW_ACES_REC709 = 0,          /* ACES RRT + ODT Rec.709 */
    ALWAN_VIEW_AGX_ORIGINAL = 1,         /* AgX original (Sobotka base picture formation) */
    ALWAN_VIEW_AGX_PUNCHY = 2,           /* AgX punchy (high contrast + saturation) */
    ALWAN_VIEW_AGX_GOLDEN = 3,           /* AgX golden (warm highlights, cool shadows) */
    ALWAN_VIEW_AGX_SB2383 = 4,           /* AgX SB2383 (Sobotka: own inset + contrast LUT) */
    ALWAN_VIEW_AGX_BLENDER = 5,          /* AgX Blender (EaryChow), baked 57^3 3D LUT */
    ALWAN_VIEW_BT2446A_HDR_TO_SDR = 6,
    ALWAN_VIEW_BT2446A_SDR_TO_HDR = 7,
    ALWAN_VIEW_KHRONOS_PBR_NEUTRAL = 8,
    ALWAN_VIEW_REINHARD_EXT = 9,         /* Reinhard Extended, luminance-based */
    ALWAN_VIEW_UCHIMURA = 10,            /* Uchimura / Gran Turismo */
    ALWAN_VIEW_LOTTES = 11,              /* Lottes / AMD Cauldron */
    ALWAN_VIEW_TONY_MCMAPFACE = 12,      /* Somewhat Boring Display Transform */
    ALWAN_VIEW_BT2446B_SDR_TO_HDR = 13,
    ALWAN_VIEW_BT2446C_HDR_TO_SDR = 14,
    ALWAN_VIEW_BT2390_HDR_TO_SDR = 15,   /* BT.2390 EETF, Hermite spline */
    ALWAN_VIEW_REINHARD_CALIBRATED = 16,
    ALWAN_VIEW_EXPOSURE = 17             /* Exposure with shoulder compression */
} alwan_view_transform;
```

Valid range is `0..17`. Anything outside it returns `ALWAN_E_INVALID` from the apply
entry points, and is silently accepted by the typed `_ex` tile paths (see
[`alwan_view_transform_map_interleave_ex`](#alwan_view_transform_map_interleave_ex)).

**The input convention is per-`vt`.** Four of the 18 take encoded signals, and three of
those clip the input to `[0, 1]` before doing anything.

| vt | Expects | Input handling |
|---|---|---|
| 0 `ACES_REC709` | linear AP1 | AP1 -> AP0 matrix, ACES RRT + ODT sRGB 100 nit, sRGB EOTF back to linear |
| 1-4 AgX `ORIGINAL`/`PUNCHY`/`GOLDEN`/`SB2383` | linear Rec.709 | negatives clamped to 0 before the inset; log2 rail `[-12.47393, 4.026069]` = `log2(0.18)` -10/+6.5 stops |
| 5 `AGX_BLENDER` | linear Rec.709 | no negative clip; Rec.709 -> E-gamut matrix first, then a `1e-10` floor inside the log encode; rail `[-12.47393, 12.5260688]` = -10/+15 stops; cube output decoded with `POW(x, 2.4)` |
| 6, 7 `BT2446A_*` | BT.2446 rho-log-encoded normalised signal | no input clip; `POW(rho, Y)` decode with `rho = 1 + 32*(L/10000)^(1/2.4)`, `L_hdr = 1000`, `L_sdr = 100`; output saturated |
| 8 `KHRONOS_PBR_NEUTRAL` | scene-linear | no input clip and no output clamp; raw in, raw out |
| 9 `REINHARD_EXT` | scene-linear | no input clip; BT.709-luma based, `L_white = 4.0`; output saturated |
| 10-12, 16, 17 | scene-linear | `MAX(0)`, then the operator, output saturated |
| 13 `BT2446B_SDR_TO_HDR` | gamma-2.4-encoded SDR | input saturated to `[0, 1]`, then `POW(x, 2.4)`; 100 -> 1000 nits |
| 14 `BT2446C_HDR_TO_SDR` | BT.2446 rho-log-encoded | input saturated to `[0, 1]`; 1000 -> 100 nits |
| 15 `BT2390_HDR_TO_SDR` | PQ-encoded | input saturated to `[0, 1]`; 10000 -> 100 nits |

> **vt 13, 14 and 15 clip the input to `[0, 1]` before the operator.** Handing them
> scene-linear data above 1.0 discards it with no diagnostic.

The AgX views take Rec.709. For log-encoded or non-Rec.709 footage, decode and convert
first with `alwan_eotf_apply_{T}` and `alwan_rgb_convert_{T}`.

---

### alwan_view_transform_apply_{T}

```c
alwan_status alwan_view_transform_apply_{T}(alwan_{T} *rgb_out, size_t out_stride,
                                            alwan_{T} const *rgb_in, size_t in_stride,
                                            size_t count,
                                            alwan_view_transform vt,
                                            alwan_ctx *ctx);
```

The worker behind both map wrappers. Scalar strided loop.

**Parameters:**
- `rgb_out` -- display-referred output triplets
- `out_stride` -- byte stride between output triplets (typically `3 * sizeof(alwan_{T})`)
- `rgb_in` -- input triplets; the convention depends on `vt` (see the table above)
- `in_stride` -- byte stride between input triplets
- `count` -- number of triplets; `0` is accepted and does no work
- `vt` -- transform id, `0..17`
- `ctx` -- ignored. Pass `NULL`.

**Returns:** `ALWAN_E_INVALID` for `NULL rgb_in` / `NULL rgb_out`, or a `vt` outside
`0..17` (checked before any write, so the output buffer is untouched). Otherwise
`ALWAN_OK`.

> **`ctx` is unused.** `alwan__view_apply_impl` in
> `src/alwan/api/alwan_view_impl.inc` opens with `(void)ctx;` and never references the
> pointer again. None of the 18 transforms reads it, in either entry point, and the map
> and `_ex` wrappers only thread it down. `NULL` is always correct. The older
> `alwan_view_transform_apply_{T}` entry in
> [transfer-functions.md](transfer-functions.md) says `ctx` is required and shows an
> `int` return type; this page supersedes both points.

**Example:**
```c
alwan_f64 scene[3] = {0.18, 0.18, 0.18};
alwan_f64 display[3];
alwan_view_transform_apply_f64(display, 3 * sizeof(alwan_f64),
                               scene, 3 * sizeof(alwan_f64), 1,
                               ALWAN_VIEW_AGX_SB2383, NULL);
```

---

### alwan_view_transform_apply_unclamped_{T}

```c
alwan_status alwan_view_transform_apply_unclamped_{T}(alwan_{T} *rgb_out, size_t out_stride,
                                                      alwan_{T} const *rgb_in, size_t in_stride,
                                                      size_t count,
                                                      alwan_view_transform vt,
                                                      alwan_ctx *ctx);
```

Same dispatch, then a second switch swaps in the six unclamped twins for
`REINHARD_EXT` (9), `UCHIMURA` (10), `LOTTES` (11), `TONY_MCMAPFACE` (12),
`REINHARD_CALIBRATED` (16) and `EXPOSURE` (17). Parameters and return codes match
`alwan_view_transform_apply_{T}`.

The twins differ in three ways:

| Twin | Output `[0, 1]` saturate | Input `MAX(0)` clip |
|---|---|---|
| `LOTTES`, `TONY_MCMAPFACE`, `REINHARD_CALIBRATED`, `EXPOSURE` | dropped | dropped |
| `REINHARD_EXT` | dropped | the clamped form never had one |
| `UCHIMURA` | dropped | kept, as a pow-domain guard for the toe |

So negative input reaches the operator through both entry points for `REINHARD_EXT`, and
through the unclamped entry point only for the four in the first row.

The other twelve `vt` values resolve to the identical function pointer, so both entry
points return bit-identical bytes for them.

There is no unclamped bulk or typed entry point. `alwan_view_transform_{T}_map_interleave`
and `alwan_view_transform_map_interleave_ex` both route to the clamped apply.

---

### alwan_view_transform_{T}_map_interleave

```c
alwan_status alwan_view_transform_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                                     alwan_{T} const *in, size_t in_stride,
                                                     size_t count,
                                                     alwan_view_transform vt,
                                                     alwan_ctx *ctx);
```

A one-line forward to `alwan_view_transform_apply_{T}` with the arguments unchanged
(`src/alwan/map/alwan_view_map.c`). It adds no `count == 0` guard, so it returns
`ALWAN_OK` for `count == 0` while `alwan_agx_{T}_map_interleave` returns
`ALWAN_E_INVALID` for the same input.

---

### alwan_view_transform_map_interleave_ex

```c
alwan_status alwan_view_transform_map_interleave_ex(void *out, size_t out_stride,
                                                    void const *in, size_t in_stride,
                                                    size_t count,
                                                    alwan_pixel_format out_fmt,
                                                    alwan_pixel_format in_fmt,
                                                    alwan_view_transform vt,
                                                    alwan_ctx *ctx);
```

Typed I/O over `ALWAN_PIXEL_U8`, `_U16`, `_F16`, `_F32`, `_F64`.

**Parameters:**
- `out` -- output buffer in `out_fmt`
- `out_stride` -- byte stride between output pixels
- `in` -- input buffer in `in_fmt`
- `in_stride` -- byte stride between input pixels
- `count` -- number of pixels; `0` returns `ALWAN_E_INVALID`
- `out_fmt` -- **output** pixel format (this argument comes first)
- `in_fmt` -- **input** pixel format
- `vt` -- transform id. Not validated here.
- `ctx` -- ignored. Pass `NULL`.

> **An unsupported `vt` on a typed tile path writes uninitialised stack memory into your
> image and returns `ALWAN_OK`.** Only the `F32/F32` and `F64/F64` fast paths forward the
> apply status. Every other format pair runs a tile loop that calls
> `alwan_view_transform_apply_{T}` and discards the return value
> (`src/alwan/map/alwan_view_map.c`). `apply` rejects a `vt` outside `0..17` before
> writing anything, so the stack tile `obuf_` is never initialised, and
> `alwan__store_tile_typed_aos_{T}` copies it out anyway. Validate `vt` yourself, or use
> `F32/F32` / `F64/F64`.

Dispatch:
- `F32/F32` -> `alwan_view_transform_apply_f32`, status forwarded
- `F64/F64` -> `alwan_view_transform_apply_f64`, status forwarded
- either side `F64` -> `f64` tiles of `ALWAN_TILE_PIXELS_F64` (2048 pixels), status discarded, returns `ALWAN_OK`
- everything else -> `f32` tiles of `ALWAN_TILE_PIXELS_F32` (4096 pixels), status discarded, returns `ALWAN_OK`
- `ALWAN_E_INVALID` when `ALWAN_WITH_F32` is compiled out and an `f32` path is needed

Integer I/O is `(2^N - 1)` normalised: `u8 * 1/255` and `u16 * 1/65535` on load; on store
`* 255` (or `* 65535`), `+ 0.5`, then a hard clamp to `[0, 255]` / `[0, 65535]`
(`src/alwan/map/alwan_tile_typed_gen.inc`, `alwan__load_tile_typed_aos_{T}` and
`alwan__store_tile_typed_aos_{T}`).

Tile buffers are fixed-size stack arrays of `ALWAN_TILE_PIXELS_{F32,F64} * 3` elements and
`count` is chunked to that size, so `count` itself has no upper bound.

**Example:**
```c
/* u16 output, f32 input. out_fmt is the sixth argument, in_fmt the seventh. */
alwan_view_transform_map_interleave_ex(dst_u16, 3 * sizeof(uint16_t),
                                       src_f32, 3 * sizeof(float),
                                       (size_t)w * h,
                                       ALWAN_PIXEL_U16, ALWAN_PIXEL_F32,
                                       ALWAN_VIEW_AGX_BLENDER, NULL);
```

---

### alwan_agx_contrast_sample_{T}

```c
alwan_status alwan_agx_contrast_sample_{T}(alwan_{T} *result, int sb2383,
                                           alwan_{T} coord, alwan_sample_mode mode);
```

Reads the same 4096-entry AgX contrast LUT the views read, with the same delta blend, so
this reader and `vt` 1-4 agree bit for bit.

**Parameters:**
- `result` -- output display value
- `sb2383` -- `0` for the original AgX curve, non-zero for the SB2383 curve
- `coord` -- normalised log2 coordinate in `[0, 1]`
- `mode` -- `ALWAN_SAMPLE_LINEAR` (0, the default) or `ALWAN_SAMPLE_NEAREST`, optionally
  `| ALWAN_SAMPLE_STRICT`

**Returns:** `ALWAN_E_INVALID` for `NULL result` or a rank-3 sample mode;
`ALWAN_E_RANGE` when `ALWAN_SAMPLE_STRICT` is set and `coord` is outside `[0, 1]`.
Otherwise `ALWAN_OK`.

---

### alwan_agx_blender_cube_sample_{T}

```c
alwan_status alwan_agx_blender_cube_sample_{T}(alwan_rgb_{T} *result,
                                               alwan_rgb_{T} const *coord,
                                               alwan_sample_mode mode);
```

Reads the baked 57^3 AgX Blender cube that `ALWAN_VIEW_AGX_BLENDER` uses.

**Parameters:**
- `result` -- output RGB, power-2.4 encoded display values
- `coord` -- RGB coordinate in `[0, 1]`, in the log2 allocation space
- `mode` -- `ALWAN_SAMPLE_LINEAR` (0, the default, resolves to trilinear),
  `ALWAN_SAMPLE_NEAREST`, `ALWAN_SAMPLE_TRILINEAR` or `ALWAN_SAMPLE_TETRAHEDRAL`,
  optionally `| ALWAN_SAMPLE_STRICT`

**Returns:** `ALWAN_E_INVALID` for `NULL result` / `NULL coord` or a rank-1 sample mode;
`ALWAN_E_RANGE` under `ALWAN_SAMPLE_STRICT` with an out-of-range coordinate. Otherwise
`ALWAN_OK`.

> **Pass `ALWAN_SAMPLE_TETRAHEDRAL` to match `vt` 5.** The view samples the cube
> tetrahedrally; the default `ALWAN_SAMPLE_LINEAR` resolves to trilinear, which gives
> different values off the lattice.

---

## Analytic AgX

A parameterized geometric AgX engine (Troy Sobotka's design: everything geometric except
the single 1D sigmoid). Input is linear Rec.709; output is display-linear in `[0, 1]` for
finite input.

Per-pixel pipeline (`agx_render_{T}` in `src/alwan/core/alwan_agx_core.inc`):

```
MAX(0) per channel
  -> inset 3x3
  -> log2 guard-rail encode (floor 1e-10, result SATURATEd to [0,1])
  -> Jed Smith tunable sigmoid per channel (its own output SATURATEd)
  -> outset 3x3
  -> SATURATE to [0,1]
  -> sRGB EOTF
```

> **The cube tips are per-call setup, not a pipeline stage.** There is no post-outset
> tint pass in the render. The split tone lives in the per-channel sigmoid **endpoints**,
> derived once per call by `agx_tip_scales_{T}` before the pixel loop
> (`src/alwan/api/alwan_agx.c`, `src/alwan/map/alwan_agx_map_kernels.inc`). Changing any
> `tip_*` field changes the per-channel `w`, `k` and `py` uniforms; nothing is added to
> the pixel after the outset matrix.

> **The default parameters match the `ALWAN_VIEW_AGX_SB2383` view to `3e-3`, not to the
> last bit.** `ALWAN_VIEW_AGX_SB2383` samples a baked 4096-entry contrast LUT with linear
> interpolation; the analytic engine evaluates the Jed Smith sigmoid the LUT was baked
> from. `3e-3` is the tolerance the AgX-family test asserts (that suite lives in the
> separate `alwan_dev` tree, not in this repository).

---

### alwan_agx_params_{T}

```c
typedef struct {
    alwan_mat3x3_{T} inset;
    alwan_{T} log2_min;
    alwan_{T} log2_max;
    alwan_{T} pivot_input;
    alwan_{T} pivot_output;
    alwan_{T} slope;
    alwan_{T} toe_power;
    alwan_{T} shoulder_power;
    alwan_mat3x3_{T} outset;
    alwan_{T} tip_upper_angle;
    alwan_{T} tip_upper_force;
    alwan_{T} tip_upper_offset;
    alwan_{T} tip_lower_angle;
    alwan_{T} tip_lower_force;
    alwan_{T} tip_lower_offset;
    alwan_{T} tip_middle_angle;
    alwan_{T} tip_middle_force;
    alwan_{T} primary_rotation[3];
    alwan_{T} primary_inset[3];
    alwan_{T} primary_purity[3];
} alwan_agx_params_{T};
```

> **Always seed the struct from `alwan_agx_default_params_{T}`.** There is no version or
> size field, and `alwan_agx_apply_{T}` reads every field unconditionally. A zeroed struct
> divides by `(log2_max - log2_min) == 0` in the log encode and feeds a zero slope with
> zero powers into the sigmoid setup.

**Fields:**
- `inset`, `outset` -- row-major 3x3. Identity outset = no restore.
- `log2_min`, `log2_max` -- absolute log2 bounds, in the units the encoder takes rather
  than EV. Defaults `-12.47393` and `4.026069`, which is `log2(0.18) + [-10, +6.5] EV`.
  No guard for `log2_max == log2_min`.
- `pivot_input` -- scene-linear grey the sigmoid pivots on (0.18). Keep it strictly inside
  `(2^log2_min, 2^log2_max)`; see the limits below.
- `pivot_output` -- its display-encoded value (0.458656). After the middle tint the
  per-channel pivot is clamped into `[1e-4, 1 - 1e-4]`.
- `slope` -- contrast at the pivot (2.4).
- `toe_power`, `shoulder_power` -- lower/upper sigmoid shape (1.5, 1.5).
- `tip_*_angle`, `primary_rotation` -- radians, on a hue wheel with phase `0 = R`,
  `2pi/3 = G`, `4pi/3 = B`. `agx_hue_weight` is `0.5 + 0.5 * cos(angle - phase)`.
- `tip_upper_force`, `tip_upper_offset` -- upper endpoint `w = 1 - up_offset - up_force * (1 - hue_up)`.
- `tip_lower_force`, `tip_lower_offset` -- lower endpoint `k = lo_offset + lo_force * hue_lo`.
- `tip_middle_angle`, `tip_middle_force` -- per-channel fulcrum tint, balanced
  (`hue_mid - 0.5`).
- `primary_rotation`, `primary_inset`, `primary_purity` -- scalar knobs per primary. Inert
  until `alwan_agx_build_geometry_{T}` is called.

`w` is forced to at least `pivot + 1e-4`, `k` to at most `pivot - 1e-4`. A tip force or
offset strong enough to cross the fulcrum is truncated, with no error.

**AgX limits:**
- The log encode floors its input at `1e-10` so `log2` stays finite. The `SATURATE` on the
  normalised log coordinate is what rails the signal: at or below `2^log2_min` it hard-rails
  to 0, at or above `2^log2_max` to 1.
- Output is clamped to `[0, 1]` twice, inside the sigmoid and again after the outset before
  the sRGB EOTF. For finite input AgX cannot return a value outside `[0, 1]`, so a
  non-identity outset that pushes a channel out of range is clipped. `alwan_saturate_{T}`
  is `if (x < 0) ...; if (x > 1) ...; return x;`, so NaN passes both clamps unchanged.
- `pivot_input` at or below `2^log2_min` collapses `px` to 0 and destroys the curve: the
  toe branch needs `x < px` and the log coordinate is never negative, so every channel
  takes the shoulder and the whole image is lifted. With the default parameters and
  `pivot_input = 0`, scene `{0, 1e-6, 0.045, 0.18, 0.9, 10}` renders as
  `{0.458656, 0.458656, 0.935349, 0.960630, 0.979744, 0.997402}`. Output stays finite.
- `pivot_input` at or above `2^log2_max` gives `px == 1` and a zero shoulder scale. Every
  channel that hard-rails to exactly 1.0 evaluates `0/0` and comes out NaN; everything
  below the rail uses the toe and stays finite.
- Neither case is validated and no error code is returned.

---

### alwan_agx_default_params_{T}

```c
alwan_agx_params_{T} alwan_agx_default_params_{T}(void);
```

Returns the SB2383 preset by value. No allocation, no failure path.

| Field | Value |
|---|---|
| `inset` | the 9 values of `src/alwan/data/matrices/agx_sb2383_inset.csv` |
| `log2_min` / `log2_max` | `-12.47393` / `4.026069` |
| `pivot_input` / `pivot_output` | `0.18` / `0.458656` |
| `slope` | `2.4` |
| `toe_power` / `shoulder_power` | `1.5` / `1.5` |
| `outset` | identity |
| all `tip_*`, all `primary_*` | `0` |

The inset is read one element at a time through `alwan_table1d_row_{T}_v`
(`src/alwan/api/alwan_agx.c`), so `ALWAN_READ_DATA_NO_BOUND_CHECK` governs that read.

---

### alwan_agx_build_geometry_{T}

```c
void alwan_agx_build_geometry_{T}(alwan_agx_params_{T} *params);
```

Rebuilds `params->inset` and `params->outset` from the three per-primary knob arrays.

**Parameters:**
- `params` -- reads `primary_rotation[3]`, `primary_inset[3]` and `primary_purity[3]`;
  overwrites `inset` and `outset`. A `NULL` pointer is silently ignored.

**Returns:** nothing. There is no status and no way to detect the no-op.

It copies the three knob triples into `alwan_vec3_{T}` and calls
`jp2499_geometry_{T}(rot, ins, pur)`. AgX's `primary_rotation` occupies JP2499's
`hue_flight` slot, `primary_inset` occupies JP2499's `chroma_attenuation` slot, and
`primary_purity` occupies JP2499's `purity` slot, so the degeneracies documented under
[`alwan_jp2499_params_{T}`](#alwan_jp2499_params_t) apply here too.

`params->inset` receives `g.inset`; `params->outset` receives `g.outset_inv`, the inverse
of the purity matrix, because AgX applies its outset forward after the sigmoid while
JP2499 applies the same matrix in its outset position.

The geometry is built on JP2499's hexagonal moment-chromaticity wheel around white
`(1/3, 1/3)` rather than on Rec.709 primaries. Angles are radians.

> **All-zero knobs are not a no-op.** The identity is computed as
> `inv(npm(unity)) @ npm(unity)`, accurate to about `1e-16` in f64 (the shipped test only
> asserts `1e-9`), and the call overwrites the baked SB2383 inset unconditionally. Call it
> only when you intend to replace the SB2383 geometry.

---

### alwan_agx_apply_{T}

```c
alwan_status alwan_agx_apply_{T}(alwan_{T} *out, size_t out_stride,
                                 alwan_{T} const *in, size_t in_stride,
                                 size_t count,
                                 alwan_agx_params_{T} const *params);
```

**Parameters:**
- `out` -- display-linear RGB in `[0, 1]`. Apply the display OETF yourself.
- `out_stride` -- byte stride between output triplets
- `in` -- linear Rec.709. Negatives are clamped to 0 before the inset.
- `in_stride` -- byte stride between input triplets
- `count` -- number of triplets; `0` returns `ALWAN_OK` and does no work
- `params` -- must be seeded from `alwan_agx_default_params_{T}`

**Returns:** `ALWAN_E_INVALID` if `out`, `in` or `params` is `NULL`. Otherwise `ALWAN_OK`.

**Example:**
```c
alwan_agx_params_f64 p = alwan_agx_default_params_f64();
p.slope = 2.6;
p.tip_lower_angle = 4.18879020478639;   /* 4pi/3 = blue, radians */
p.tip_lower_force = 0.05;               /* lift the blacks toward blue */
alwan_agx_apply_f64(out, 3 * sizeof(double), in, 3 * sizeof(double), w * h, &p);
```

---

### alwan_agx_{T}_map_interleave

```c
alwan_status alwan_agx_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                          alwan_{T} const *in, size_t in_stride,
                                          size_t count,
                                          alwan_agx_params_{T} const *params);
```

Same per-call setup and same `agx_render_{T}` as apply, so the pixel output is
bit-identical. The guard differs:
`if (!out || !in || !params || count == 0) return ALWAN_E_INVALID;`
(`src/alwan/map/alwan_agx_map_kernels.inc`).

---

### alwan_agx_map_interleave_ex

```c
alwan_status alwan_agx_map_interleave_ex(void *out, size_t out_stride,
                                         void const *in, size_t in_stride,
                                         size_t count,
                                         alwan_pixel_format out_fmt,
                                         alwan_pixel_format in_fmt,
                                         alwan_agx_params_f64 const *params);
```

No precision suffix. Takes `alwan_agx_params_f64` only.

**Parameters:**
- `out` -- output buffer in `out_fmt`
- `out_stride` -- byte stride between output pixels
- `in` -- input buffer in `in_fmt`, linear Rec.709
- `in_stride` -- byte stride between input pixels
- `count` -- number of pixels; `0` returns `ALWAN_E_INVALID`
- `out_fmt` -- **output** pixel format (this argument comes first)
- `in_fmt` -- **input** pixel format
- `params` -- `f64` parameter block, required

**Returns:** `ALWAN_E_INVALID` on `NULL out`, `NULL in`, `NULL params`, or `count == 0`;
`ALWAN_E_INVALID` when `ALWAN_WITH_F32` is compiled out and an `f32` path is needed. The
tile paths return `ALWAN_OK` unconditionally; the kernel they call can only fail on `NULL`
or `count == 0`, which the tile loop never passes.

Dispatch:
- `F32/F32` -> `f32` kernel with the parameters narrowed to `f32`
- `F64/F64` -> `f64` kernel
- either side `F64` -> `f64` tiles of 2048 pixels
- everything else (u8, u16, f16, and mixed pairs with f32) -> `f32` tiles of 4096 pixels with the `f32`-narrowed parameters

> **`f64` params does not mean `f64` arithmetic.** Only `F64/F64` and mixed pairs where one
> side is `ALWAN_PIXEL_F64` run the `f64` kernel. Every u8, u16, f16 and f32 combination
> narrows the whole parameter block field by field to `alwan_agx_params_f32` and runs the
> `f32` kernel (`src/alwan/map/alwan_agx_map.c`).

> **Integer input defeats the log2 guard rail.** A `u8` or `u16` input is normalised to
> `[0, 1]`, so the scene signal tops out at about +2.5 stops over mid-grey 0.18. The guard
> rail exists to carry +6.5 stops. On store the result is multiplied back, rounded with
> `+ 0.5` and clamped.

---

## JP2499

Juan Pablo Zambrano's "2499" DRT: a ratio-preserving Michaelis-Menten tonescale with
per-primary hue-flight, chroma-attenuation and purity controls, plus optional cube-tip
split-toning. Input is linear Rec.709.

Per-pixel pipeline (`jp2499_render_{T}` in `src/alwan/core/alwan_agx_jp2499_core.inc`):

```
keep max(R,G,B)
  -> ratio-toe pass 1
  -> inset 3x3 (hue_flight + chroma_attenuation)
  -> per-channel Michaelis-Menten tonescale + parabolic flare (and the same on the stored max)
  -> outset_inv 3x3 (hue_flight + purity)
  -> ratio-toe pass 2
  -> recombine ratios with the tonescaled max via a safe divide
  -> += black_tip * (1 - lum) + white_tip * lum
  -> * ds, where ds is hard-coded 1.0 in alwan_jp2499_apply_{T}
```

> **JP2499 clamps neither its input nor its output.** `jp2499_render` takes `r`, `g`, `b`
> straight from the caller, and both `jp2499_tonescale` and `jp2499_flare` pass any
> `x <= 0` through unchanged, so negative scene values survive into the outset matrix. The
> only `MAX(0)` is on the luminance divisor inside `jp2499_ratio_toe`. AgX clamps to
> `>= 0`; JP2499 does not.

> **Output range.** The display scale `ds` is hard-coded to `1.0`, and the
> Michaelis-Menten fit is solved so the tonescale reaches `py = Lp/100` at the scene value
> `px = 256*log2(Lp)/log2(100) - 128`. Units are therefore `1.0 == 100 nits`: the range is
> unbounded below (negatives pass through), up to about `Lp/100` with an asymptote slightly
> above it. Measured, at `Lp = 100` mid-grey 0.18 lands at 0.100000, scene `px = 128` lands
> at 1.000000, and the asymptote is 1.008548. At `Lp = 1000` mid-grey lands at 0.139863,
> scene `px = 256` at 10.0, asymptote 10.276. At `Lp = 4000` the peak is 40.0, asymptote
> 42.650. Feeding that into a display OETF, or into the `_ex` u8/u16 store (which clamps at
> 1.0), blows out everything above 100 nits.

Rescaling by `100 / Lp` yourself gets a signal in roughly `[0, 1.01]` at `Lp = 100`,
`[0, 1.03]` at `Lp = 1000` and `[0, 1.07]` at `Lp = 4000`. A clamp or a highlight rolloff
is still needed for a strict `[0, 1]`.

---

### alwan_jp2499_params_{T}

```c
typedef struct {
    alwan_{T} chroma_attenuation[3];
    alwan_{T} hue_flight[3];
    alwan_{T} purity[3];
    alwan_{T} peak_luminance;
    alwan_rgb_{T} white_tip;
    alwan_rgb_{T} black_tip;
} alwan_jp2499_params_{T};
```

**Fields:**
- `hue_flight[3]` -- radians, added to each primary's base angle `i * 2pi/3`.
- `chroma_attenuation[3]` -- per-primary path-to-white rate. Enters as a `(1 - value)`
  scale on the primary's distance from white on the hexagonal moment wheel, and builds the
  inset.
- `purity[3]` -- per-primary complementary restore. Same `(1 - value)` form, used to build
  the outset.
- `peak_luminance` -- cd/m^2 (`Lp`). Shapes the tonescale. It does not scale the output to
  nits.
- `white_tip`, `black_tip` -- `alwan_rgb_{T}` triples added after the tonescale.

> **`chroma_attenuation` and `purity` are usable in `[0, 1)` only, and they fail
> differently.** At exactly `1.0` the primary collapses onto white `(1/3, 1/3)` and the NPM
> goes rank-1. For `purity[i] == 1.0` the outset matrix is then singular, `jp2499_invert3`'s
> `det != 0` guard substitutes an all-zero matrix, the image renders black, and the call
> still returns `ALWAN_OK`. For `chroma_attenuation[i] == 1.0` the inset goes rank-1 and
> every output channel becomes that one primary's input channel (`{1,0,0}` gives
> `inset = [[1,0,0],[1,0,0],[1,0,0]]`); all three at `1.0` give a zero inset and a black
> image. Above `1.0` the primary is reflected through white and its hue inverts. The same
> applies to AgX's `primary_inset` and `primary_purity`, which feed the same constructor.

> **The split-tone weights are `0.25 R + 0.45 G + 0.30 B` on the already-tonescaled RGB,
> and the shadow weight is `1 - lum` with no clamp.** Those are Jp-DRT's weights, and they
> differ from Rec.709 luma (`0.2126 / 0.7152 / 0.0722`). Wherever `lum > 1` the shadow
> weight goes negative and `black_tip` is subtracted instead of added. `lum` exceeds 1
> marginally at the default `Lp = 100` and reaches about 10 at `Lp = 1000`.

**peak_luminance limits.** The only validation is `params->peak_luminance > 0`; both `<= 0`
and `NaN` fall back to `100.0` (`NaN > 0` is false). Three unguarded degeneracies follow
from `jp2499_tonescale_params_{T}`:

| `Lp` | What happens |
|---|---|
| exactly `10.0` | `px = 256 * log2(10) / log2(100) - 128 = 0`, so `M = m0^(1/C) * (S + px) / px` is `0/0` and every pixel is `NaN` |
| below `~0.3100` (exactly `100 * 2^(-1/0.12)` = 0.310039) | `gy = 0.1 * (1 + 0.12 * log2(Lp/100))` goes negative, so `sqrt(gy * (4*FL + gy))` takes the square root of a negative and every pixel is `NaN`. Below `Lp ~ 2.4e-5` the product turns positive again and the failure moves to `POW(s0, 1/C)` with `s0 < 0` |
| `0.388` to `4.710`, and `10.0` to `10.046` | the solved `S` is negative, putting a pole at `x = -S`; any scene channel below `\|S\|` evaluates `POW(negative, 1.15)`. At `Lp = 1`, `S = -0.0586`, so every scene value under 0.0586 fails |

`px` is negative for every `Lp < 10`, so the tonescale anchor below 10 nits is a negative
scene value. Nothing clamps, warns, or returns an error for any of this.

> **The `POW` rows are build-dependent.** In a default (libm) build `POW` of a negative
> base is `NaN`. Under `ALWAN_DETERMINISTIC` (the shipped `Debug_Det` / `Release_Det`
> configurations) `ALWAN_POW` is redefined to `alwan_det_pow_pos_{T}`, which returns `0`
> for any base `<= 0`, so the same inputs render black instead of `NaN`. The `Lp == 10`
> row (a division) and the square-root row hold in both builds; `ALWAN_SQRT` is not
> overridden.

---

### alwan_jp2499_default_params_{T}

```c
alwan_jp2499_params_{T} alwan_jp2499_default_params_{T}(void);
```

Returns the jedypod/JP2499 `Jp-DRT.dctl` UI defaults by value.

| Field | Value |
|---|---|
| `chroma_attenuation` | `{0.15, 0.20, 0.128}` |
| `hue_flight` | `{0.08, -0.05, -0.142}` radians |
| `purity` | `{0.15, 0.20, 0.0}` |
| `peak_luminance` | `100.0` |
| `white_tip`, `black_tip` | all zero |

All-zero parameters give the plain ratio-preserving tonescale with no hue shaping;
`peak_luminance = 0` then falls back to `Lp = 100`.

---

### alwan_jp2499_apply_{T}

```c
alwan_status alwan_jp2499_apply_{T}(alwan_{T} *out, size_t out_stride,
                                    alwan_{T} const *in, size_t in_stride,
                                    size_t count,
                                    alwan_jp2499_params_{T} const *params);
```

**Parameters:**
- `out` -- display-linear Rec.709, up to about `Lp/100`. Rescale before encoding.
- `out_stride` -- byte stride between output triplets
- `in` -- linear Rec.709. Not clamped.
- `in_stride` -- byte stride between input triplets
- `count` -- number of triplets; `0` returns `ALWAN_OK` and does no work
- `params` -- required

**Returns:** `ALWAN_E_INVALID` if `out`, `in` or `params` is `NULL`. Otherwise `ALWAN_OK`.

The tonescale and the hue geometry are solved once per call natively in `{T}`
(`jp2499_tonescale_params_{T}` and `jp2499_geometry_{T}`), then reused for every pixel. The
`f32` tonescale solve is where `f32` rounding shows up.

**Example:**
```c
alwan_jp2499_params_f64 p = alwan_jp2499_default_params_f64();
p.peak_luminance = 1000.0;                  /* 1000 nit display */
alwan_jp2499_apply_f64(out, 3 * sizeof(double), in, 3 * sizeof(double), w * h, &p);

/* Bring the result back toward [0,1]: 1.0 == 100 nits on the way out. */
double const k = 100.0 / p.peak_luminance;
for (size_t i = 0; i < (size_t)w * h * 3; i++) out[i] *= k;
```

---

### alwan_jp2499_{T}_map_interleave

```c
alwan_status alwan_jp2499_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                             alwan_{T} const *in, size_t in_stride,
                                             size_t count,
                                             alwan_jp2499_params_{T} const *params);
```

Same per-call setup and the same `jp2499_render_{T}` as apply, so the pixel output is
bit-identical. The guard adds `count == 0`:
`if (!out || !in || !params || count == 0) return ALWAN_E_INVALID;`
(`src/alwan/map/alwan_agx_jp2499_map_kernels.inc`).

---

### alwan_jp2499_map_interleave_ex

```c
alwan_status alwan_jp2499_map_interleave_ex(void *out, size_t out_stride,
                                            void const *in, size_t in_stride,
                                            size_t count,
                                            alwan_pixel_format out_fmt,
                                            alwan_pixel_format in_fmt,
                                            alwan_jp2499_params_f64 const *params);
```

**Parameters:**
- `out` -- output buffer in `out_fmt`
- `out_stride` -- byte stride between output pixels
- `in` -- input buffer in `in_fmt`, linear Rec.709
- `in_stride` -- byte stride between input pixels
- `count` -- number of pixels; `0` returns `ALWAN_E_INVALID`
- `out_fmt` -- **output** pixel format (this argument comes first)
- `in_fmt` -- **input** pixel format
- `params` -- `f64` parameter block, required

**Returns:** `ALWAN_E_INVALID` on `NULL out`, `NULL in`, `NULL params`, or `count == 0`;
`ALWAN_E_INVALID` when `ALWAN_WITH_F32` is compiled out and an `f32` path is needed. The
tile paths return `ALWAN_OK` unconditionally.

Dispatch matches the AgX `_ex`: only `F64/F64` and mixed pairs involving `F64` run the
`f64` kernel; every u8, u16, f16 and f32 combination narrows the parameters to
`alwan_jp2499_params_f32` and runs the `f32` kernel
(`src/alwan/map/alwan_agx_jp2499_map.c`).

> **The u8/u16 store clamps at 1.0, and JP2499 exceeds 1.0 even at the default
> `peak_luminance`.** The asymptote at `Lp = 100` is 1.008548. At `Lp = 1000` a bright pixel
> comes out at about 10.0 and is written as 255 or 65535. Rescale by `100 / Lp` before the
> typed store if you want the highlights back.

---

## Error Codes

`ALWAN_E_INVALID` is the only failure code the transform entry points return.
`ALWAN_E_RANGE` appears only on the two table samplers.

- **`ALWAN_OK` (0)** -- every successful call. Also returned by
  `alwan_view_transform_map_interleave_ex` on a typed tile path when the underlying `vt`
  was rejected.
- **`ALWAN_E_INVALID` (-1)** -- `NULL out` / `NULL in` (all entry points); `NULL params`
  (AgX and JP2499 apply, map and `_ex`); `count == 0` (all map and `_ex` entry points
  except `alwan_view_transform_{T}_map_interleave`, and none of the apply entry points); a
  `vt` outside `0..17` (`alwan_view_transform_apply_{T}` and
  `alwan_view_transform_apply_unclamped_{T}` only); a sample mode of the wrong rank on the
  two samplers; a format pair needing `f32` when `ALWAN_WITH_F32` is compiled out.
- **`ALWAN_E_NODATA` (-2)** -- never returned here, even though the AgX views read embedded
  LUT and matrix tables.
- **`ALWAN_E_RANGE` (-3)** -- returned by `alwan_agx_contrast_sample_{T}` and
  `alwan_agx_blender_cube_sample_{T}` when `ALWAN_SAMPLE_STRICT` is set and the coordinate
  is outside `[0, 1]`. Never returned by a transform: `peak_luminance`, `pivot_input`,
  `chroma_attenuation` and `purity` are unvalidated, and out-of-range values produce `NaN`,
  a black image, or a silently clamped value.
- **`ALWAN_E_DIVZERO` (-5)** -- never returned. `jp2499_invert3` substitutes a zero matrix
  for a zero determinant, `jp2499_sdiv` substitutes zero for a zero denominator, and the
  AgX log encode divides by `(lmax - lmin)` with no guard.

`alwan_agx_build_geometry_{T}` returns `void` and reports nothing.

---

## See Also

- [Transfer Functions](transfer-functions.md) -- OETF/EOTF and the `alwan_transfer_function` enum
- [ACES](aces.md) -- ACES 1.x and 2.0 output transforms
- [LUTs](luts.md) -- baking a view transform into a 1D or 3D LUT
- [Batch / Map API](map.md) -- the four dispatch shapes, pixel formats, and the `_ex` convention
- [HDR Pipeline](hdr.md) -- HLG OOTF, MaxCLL/MaxFALL, and the BT.2446 / BT.2390 cores
- [Gamut Operations](gamut.md) -- gamut mapping operators
- [Reference Data](reference-data.md) -- the embedded table registry and sample modes
- [Configuration](../configuration.md) -- precision build flags and `ALWAN_DETERMINISTIC`
- [Picture Formation](../picture_formation.md) -- the picture-formation operators and the 13-column test matrix
- [Gamut Mapping](../gamut_mapping.md) -- clamped and unclamped entry points across the library
- [Ranges](../ranges.md) -- input and output ranges per API
- [Precision and Limits](../precision-and-limits.md) -- f32 and f64 behaviour
- [Determinism](../determinism.md) -- byte-exactness of the view and AgX paths
