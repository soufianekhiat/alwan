# Table And LUT Sampling API

Interpolated reads of caller-supplied lookup tables: 1-D curves, 2-D grids, 2-D
strips, 3-D cubes, and 1-D ramps of 3x3 matrices.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

---

## Overview

These are the readers behind alwan's own embedded tables, exposed for tables you
supply yourself. They share one contract:

- **Coordinates are `[0, 1]`** on every axis, whatever the table's extent.
- **A mode is rejected, never downgraded.** Passing a mode a rank cannot honour
  returns `ALWAN_E_INVALID` rather than quietly substituting another. Silently
  changing the interpolation is how a pipeline ships the wrong curve and nobody
  notices for two releases.
- **Addresses are clamped; values are not.** An out-of-range coordinate reads the
  edge sample rather than off the end. What comes back is never clamped.
- **NaN resolves to the low edge**, and `ALWAN_SAMPLE_STRICT` turns that into an
  error instead. See [Strict mode](#strict-mode).

`ALWAN_E_INVALID` is returned for a null pointer, an extent below 2, or an
unsupported mode. All three are loop-invariant and checked once on entry.

---

## Sample modes

```c
typedef enum {
    ALWAN_SAMPLE_LINEAR      = 0,   /* the default for every rank */
    ALWAN_SAMPLE_NEAREST     = 1,
    ALWAN_SAMPLE_BILINEAR    = 2,
    ALWAN_SAMPLE_TRILINEAR   = 3,
    ALWAN_SAMPLE_TETRAHEDRAL = 4,
    ALWAN_SAMPLE_CATMULL_ROM = 5,
    ALWAN_SAMPLE_STRICT      = 0x100  /* OR into the mode, scalar readers only */
} alwan_sample_mode;
```

`ALWAN_SAMPLE_LINEAR` is `0` so a zero-initialised or `memset` mode interpolates
rather than snapping to the nearest sample. Banding from an accidental NEAREST is
silent and easy to miss.

Accepted modes by rank:

| reader | accepted | default |
|---|---|---|
| 1-D scalar | NEAREST, LINEAR, CATMULL_ROM | LINEAR |
| 1-D mat3x3 ramp | NEAREST, LINEAR | LINEAR |
| 2-D grid | NEAREST, BILINEAR, LINEAR | bilinear |
| 2-D strip | NEAREST, TRILINEAR, LINEAR | trilinear |
| 3-D cube | NEAREST, TRILINEAR, TETRAHEDRAL, LINEAR | trilinear |

LINEAR is accepted at every rank and **resolves to that rank's linear member**:
bilinear on a grid, trilinear on a strip or cube. It is the zero value, so
rejecting it would turn a zero-initialised mode into an error instead of the
interpolated read the caller expects. This is the one place a mode is resolved
rather than rejected, and it only ever resolves within the same family.

---

## 1-D table

### alwan_table1d_sample_{T}

```c
alwan_status alwan_table1d_sample_{T}(alwan_{T} *result,
                                      alwan_{T} const *table, int size,
                                      alwan_{T} coord, alwan_sample_mode mode);
```

Sample a curve of `size` scalars at `coord` in `[0, 1]`.

**Parameters:**
- `table` -- `size` entries
- `size` -- number of entries, `>= 2`
- `coord` -- `[0, 1]`
- `mode` -- LINEAR (default), NEAREST or CATMULL_ROM, optionally `| ALWAN_SAMPLE_STRICT`

**Example:**
```c
alwan_{T} curve[16], out;
alwan_table1d_sample_{T}(&out, curve, 16, 0.5, ALWAN_SAMPLE_LINEAR);
```

**CATMULL_ROM** is the four-tap interpolating cubic through
`table[i-1 .. i+2]`, with the two outer taps clamped at the ends. It passes
through every sample, unlike a smoothing spline, and is worth having for LUT
sampling where linear leaves visible facets on a coarse grid.

> **It can overshoot.** A four-tap cubic across a hard edge leaves the convex hull
> of its taps by up to about 1/8 of the step. If the table's range is a contract,
> clamp the result yourself. This is why it is not the default for any rank.

---

## 2-D grid

### alwan_table2d_grid_sample_{T}

```c
alwan_status alwan_table2d_grid_sample_{T}(alwan_{T} *result,
                                           alwan_{T} const *table,
                                           int rows, int cols,
                                           alwan_{T} row_coord, alwan_{T} col_coord,
                                           alwan_sample_mode mode);
```

Sample a genuine 2-D grid of scalars, row-major, `index = row * stride + col`.
The two axes are independent and may have different extents.

**Parameters:**
- `table` -- `rows * stride` values
- `rows`, `stride` -- extents, both `>= 2`
- `row_coord`, `col_coord` -- `[0, 1]` on each axis
- `mode` -- LINEAR (default, resolves to bilinear), NEAREST or BILINEAR

**Example:**
```c
/* 3 rows x 4 columns; centre of the grid */
alwan_{T} grid[3 * 4], out;
alwan_table2d_grid_sample_{T}(&out, grid, 3, 4, 0.5, 0.5, ALWAN_SAMPLE_BILINEAR);
```

This is the rank `ALWAN_SAMPLE_BILINEAR` fits. The 2-D *strip* below is a cube
flattened into two dimensions, not a 2-D grid, and rejects BILINEAR for that
reason.

---

## 2-D strip

### alwan_table2d_sample_{T}

```c
alwan_status alwan_table2d_sample_{T}(alwan_rgb_{T} *result,
                                      alwan_{T} const *strip, int size,
                                      alwan_rgb_{T} const *coord,
                                      alwan_sample_mode mode);
```

Sample an RGB cube stored flattened to `(size*size) x size`, RGB interleaved.

**Parameters:**
- `strip` -- `size*size * size * 3` values
- `size` -- cube edge length, `>= 2`
- `coord` -- `[0, 1]` RGB coordinate
- `mode` -- LINEAR (default, resolves to trilinear), NEAREST or TRILINEAR

The strip is a cube, so it is sampled over r, g and b trilinearly.
`ALWAN_SAMPLE_BILINEAR` returns `ALWAN_E_INVALID` here rather than being accepted
as a friendly spelling of TRILINEAR.

---

## 3-D cube

### alwan_table3d_sample_{T}

```c
alwan_status alwan_table3d_sample_{T}(alwan_rgb_{T} *result,
                                      alwan_{T} const *cube, int size,
                                      alwan_rgb_{T} const *coord,
                                      alwan_sample_mode mode);
```

Sample an RGB cube, R-fastest, RGB interleaved,
`index = ((b*size + g)*size + r)*3 + channel`.

**Parameters:**
- `cube` -- `size^3 * 3` values
- `size` -- cube edge length, `>= 2`
- `coord` -- `[0, 1]` RGB coordinate
- `mode` -- LINEAR (default, resolves to trilinear), NEAREST, TRILINEAR or TETRAHEDRAL

**Example:**
```c
alwan_rgb_{T} in = {0.3, 0.6, 0.9}, out;
alwan_table3d_sample_{T}(&out, cube, 33, &in, ALWAN_SAMPLE_TETRAHEDRAL);
```

**TETRAHEDRAL** splits each cell into six tetrahedra and interpolates within one,
reading four corners instead of eight. It is the convention most film and video
LUT tools use, and it preserves the neutral axis exactly where trilinear can
introduce a small cross-channel error. Prefer it when matching another tool's
LUT output; prefer trilinear when matching a reference that specifies it.

---

## 1-D ramp of matrices

### alwan_table1d_mat3_sample_{T}

```c
alwan_status alwan_table1d_mat3_sample_{T}(alwan_mat3x3_{T} *result,
                                           alwan_mat3x3_{T} const *table, int size,
                                           alwan_{T} coord, alwan_sample_mode mode);
```

Sample a ramp of 3x3 matrices, interpolating all nine elements. This is the shape
a severity or temperature ramp takes, such as the CVD matrices behind
[colour-vision-deficiency](color-vision-deficiency.md).

**Modes:** NEAREST and LINEAR only. CATMULL_ROM is deliberately not accepted: the
reference CVD model defines *linear* severity interpolation, and a four-tap
kernel would change published output.

---

## Strict mode

`ALWAN_SAMPLE_STRICT` ORs into the mode of a **scalar** reader:

```c
alwan_table1d_sample_{T}(&out, table, 16, coord,
                         ALWAN_SAMPLE_LINEAR | ALWAN_SAMPLE_STRICT);
```

With it, a non-finite or out-of-`[0,1]` coordinate returns `ALWAN_E_RANGE` and
leaves `*result` untouched, instead of addressing the clamped edge. Without it,
the coordinate is clamped and **NaN resolves to the low edge**, which is pinned by
`alwan_dev/tests/102_table_gate.c`.

Strict is scalar-only. A bulk reader given it returns `ALWAN_E_INVALID` rather
than ignoring it.

---

## Bounds checking

Every read goes through the table reader layer, which clamps the computed index
into range. `ALWAN_READ_DATA_NO_BOUND_CHECK` (default `0`, meaning checked)
compiles that clamp out for callers who can guarantee finite, in-range
coordinates. With it set, a NaN coordinate is an out-of-bounds read rather than a
wrong colour.

See [alwan_decisions.md](../alwan_decisions.md), "Addresses are clamped; values
never are".

---

## Named readers for the embedded tables

Four readers over alwan's own baked-in tables. There is no buffer and no `size`
parameter: the table is the identity, so a caller cannot hand one a mismatched
extent. All four are scalar readers, so `ALWAN_SAMPLE_STRICT` applies to each of
them; see [Strict mode](#strict-mode). None has a bulk `_map_` form.

Reachable statuses for all four: `ALWAN_OK`, `ALWAN_E_INVALID`, `ALWAN_E_RANGE`.
`ALWAN_E_NOMEM` is never returned; these allocate nothing.

`ALWAN_E_INVALID` covers a null pointer, a mode the rank cannot honour, and, for
the two readers that take one, an out-of-enum `cvd_type` or `gamut`. Validation
order is not uniform across the four. `alwan_machado_matrix_sample_{T}` and
`alwan_jakob2019_coeff_sample_{T}` check the enum before the STRICT range test,
so a bad enum together with an out-of-range coordinate returns `ALWAN_E_INVALID`
and never `ALWAN_E_RANGE`. `alwan_agx_contrast_sample_{T}` runs the STRICT test
first and has no enum to reject.

The edge-clamp behaviour in [Bounds checking](#bounds-checking) applies to these
four like every other reader, and is conditional on the same
`ALWAN_READ_DATA_NO_BOUND_CHECK == 0` default.

---

### alwan_machado_matrix_sample_{T}

```c
alwan_status alwan_machado_matrix_sample_{T}(alwan_mat3x3_{T} *result,
                                             alwan_cvd_type cvd_type,
                                             alwan_{T} severity,
                                             alwan_sample_mode mode);
```

Read the Machado 2009 CVD simulation matrix for one deficiency at one severity.

**Parameters:**
- `result` -- output matrix; null returns `ALWAN_E_INVALID`
- `cvd_type` -- `0..5`; any other value returns `ALWAN_E_INVALID`
- `severity` -- `[0, 1]`, clamped
- `mode` -- LINEAR (default) or NEAREST, optionally `| ALWAN_SAMPLE_STRICT`.
  CATMULL_ROM returns the LINEAR result and `ALWAN_OK`

**Six enum values, three ramps.** `ALWAN_CVD_PROTANOMALY` reads the protan ramp,
`ALWAN_CVD_DEUTERANOMALY` the deutan ramp, `ALWAN_CVD_TRITANOMALY` the tritan
ramp. PROTANOMALY at severity 0.6 returns the same matrix PROTANOPIA returns at
severity 0.6; severity is the only thing separating anomalous from dichromatic.

**The ramp is 11 matrices**, at severity 0.0, 0.1, ... 1.0. The internal spelling
of the count is `ALWAN_MACHADO_SEVERITY_STEPS` in `core/alwan_vision_core.h` and
is not on the public surface. `severity` is a normalised coordinate over those
nodes: NEAREST snaps to the nearest tenth, LINEAR interpolates all nine elements
between two tenths. Node 0 is the exact identity, so severity `0.0` returns I.

> **The matrix is applied to whatever RGB you pass.** Storage is
> `alwan_mat3x3_{T}.m[9]`, **row-major**, applied to a column vector as
> `out = M * rgb`: `out.r = m[0]*r + m[1]*g + m[2]*b`,
> `out.g = m[3]*r + m[4]*g + m[5]*b`, `out.b = m[6]*r + m[7]*g + m[8]*b`. The
> library performs no linearisation and no re-encoding on either side of that
> multiply and cannot tell you which domain the values are in.
> `alwan_simulate_cvd_machado_{T}` does the same multiply on the caller's values;
> see [color-vision-deficiency.md](color-vision-deficiency.md).

> **CATMULL_ROM is accepted and resolves to LINEAR.** The rank-1 mode check lets
> it through and the mat3x3 core has no cubic branch, so the call returns
> `ALWAN_OK` with the linear blend. `alwan_table1d_mat3_sample_{T}` above behaves
> the same way, through the same predicate and the same dispatch: "not accepted"
> there describes the core, which carries no four-tap kernel for a matrix ramp
> because the reference CVD model defines linear severity interpolation and a
> four-tap kernel would change published output. The accepted-modes table above
> lists the two modes with distinct results.

An out-of-range `severity` is clamped and still returns `ALWAN_OK`: `5.0` and
`+inf` give the severity-1.0 matrix, `-1.0` and `-inf` give the identity, and NaN
gives the identity. With `| ALWAN_SAMPLE_STRICT` every one of those returns
`ALWAN_E_RANGE` and leaves `*result` untouched, NaN included: the in-range
predicate is false for NaN by construction. The clamping behaviour is pinned by
`alwan_dev/tests/102_table_gate.c`.

Under LINEAR the returned matrix is bit-identical to the one
`alwan_simulate_cvd_machado_{T}` builds internally at the same severity.

**Example:**
```c
alwan_mat3x3_{T} m;
alwan_machado_matrix_sample_{T}(&m, ALWAN_CVD_DEUTERANOMALY, 0.55,
                                ALWAN_SAMPLE_LINEAR);
/* m.m is row-major. Apply to rgb as out = m * rgb. */
```

---

### alwan_jakob2019_coeff_sample_{T}

```c
alwan_status alwan_jakob2019_coeff_sample_{T}(alwan_{T} *result_c012,
                                              alwan_jakob2019_gamut gamut,
                                              alwan_rgb_{T} const *coord,
                                              alwan_sample_mode mode);
```

Read the three Jakob 2019 spectral-upsampling polynomial coefficients for one
coordinate.

**Parameters:**
- `result_c012` -- caller array with room for exactly 3 elements, written as
  `c0`, `c1`, `c2`; null returns `ALWAN_E_INVALID`
- `gamut` -- `0..5`; any other value returns `ALWAN_E_INVALID`
- `coord` -- `[0, 1]` on each axis, clamped; null returns `ALWAN_E_INVALID`
- `mode` -- LINEAR (default, resolves to trilinear), NEAREST or TRILINEAR,
  optionally `| ALWAN_SAMPLE_STRICT`

> **The coefficients are for lambda in NANOMETRES.** The library evaluates
> `U = c0*lambda*lambda + c1*lambda + c2`, then
> `R = 0.5 + U / (2 * sqrt(1 + U*U))`, clamped to `[0, 1]`, over 360-780 nm at 85
> samples (5 nm steps). Several other Jakob 2019 implementations store their
> coefficients against a lambda normalised to `[0, 1]`. Fed to such an evaluator,
> these coefficients give a reflectance that is wrong at every wavelength, with
> no error raised anywhere.

> **`ALWAN_SAMPLE_TETRAHEDRAL` returns `ALWAN_E_INVALID` here**, even though the
> tables are 64^3 cubes: this reader uses the 2-D strip mode set (see
> [Sample modes](#sample-modes)) and the planar core has no tetrahedral path.
> `ALWAN_SAMPLE_BILINEAR` and `ALWAN_SAMPLE_CATMULL_ROM` are rejected too.
> Accepted: NEAREST, TRILINEAR, LINEAR.

> **With `ALWAN_JAKOB2019_XYZ` the `alwan_rgb_{T}` fields carry X, Y and Z.** The
> struct field names stay `r`, `g`, `b` and are fed straight into the sampler.

Each gamut selects three **planar** scalar cubes, one per coefficient, 64 nodes
per axis, indexed **b-fastest** as `(r*64 + g)*64 + b`. That is a different
layout from the interleaved r-fastest `alwan_table3d_sample_{T}` above, which is
why these have their own reader rather than a flag on that one.

Coordinates are clamped into `[0, 1]` per axis and a NaN axis resolves to the
cube's low corner, both returning `ALWAN_OK`. `| ALWAN_SAMPLE_STRICT` turns an
out-of-range or NaN axis into `ALWAN_E_RANGE` with `result_c012` untouched.

Under LINEAR or TRILINEAR the coefficients are bit-identical to the ones
`alwan_rgb_to_spectrum_jakob2019_{T}` samples internally; see
[spectral.md](spectral.md). Unlike that function, this reader allocates nothing,
never returns `ALWAN_E_NOMEM`, and takes no `alwan_ctx`.

---

### alwan_agx_contrast_sample_{T}

```c
alwan_status alwan_agx_contrast_sample_{T}(alwan_{T} *result, int sb2383,
                                           alwan_{T} coord,
                                           alwan_sample_mode mode);
```

Read one of the two AgX contrast curves.

**Parameters:**
- `result` -- output scalar; null returns `ALWAN_E_INVALID`
- `sb2383` -- `0` for the original AgX curve, any non-zero value for the SB2383
  curve. Plain truthiness, no validation
- `coord` -- `[0, 1]`, clamped
- `mode` -- LINEAR (default) or NEAREST, optionally `| ALWAN_SAMPLE_STRICT`.
  CATMULL_ROM returns the LINEAR result and `ALWAN_OK`

Both curves are 4096 entries; the constant is internal to
`data/alwan_data_tables.h`. LINEAR uses the `a + f*(b - a)` delta blend, the same
spelling the view transform's `agx_lut_eval` uses, so the LUT value matches the
one the view reads, bit for bit. NEAREST has no counterpart in the view.

The library ships exactly two 1-D contrast LUTs and `sb2383` reaches both.
`ALWAN_VIEW_AGX_ORIGINAL`, `ALWAN_VIEW_AGX_PUNCHY` and `ALWAN_VIEW_AGX_GOLDEN`
all read the default curve, PUNCHY and GOLDEN then applying a grade this reader
has no access to. `ALWAN_VIEW_AGX_SB2383` reads the SB2383 curve.
`ALWAN_VIEW_AGX_BLENDER` uses no 1-D curve at all.

> **`coord` is a specific log2 encoding, and the encoder is internal.** The view
> builds it as
> `saturate((log2(max(x, 1e-10)) - (-12.47393)) / (4.026069 - (-12.47393)))`,
> where `x` is the inset-matrix output and the bounds are `log2(0.18) + [-10,
> +6.5] EV`. Both curves use the same bounds; they differ in the inset matrix and
> the LUT. None of those constants is in `alwan.h`, `alwan_agx_log_encode` is not
> declared there, and the inset matrices are `static` in `alwan_view.c`. Copy the
> expression above to reproduce the view's coordinate.

> **The output is the raw sRGB-encoded LUT value.**
> `alwan_view_transform_apply_{T}` (see
> [transfer-functions.md](transfer-functions.md)) SATURATEs it and applies the
> sRGB EOTF before returning, and PUNCHY and GOLDEN grade it first, so this
> reader's output is not what the view returns for the same scene value.

---

### alwan_agx_blender_cube_sample_{T}

```c
alwan_status alwan_agx_blender_cube_sample_{T}(alwan_rgb_{T} *result,
                                               alwan_rgb_{T} const *coord,
                                               alwan_sample_mode mode);
```

Read the baked Blender AgX picture-formation cube.

**Parameters:**
- `result` -- output RGB; null returns `ALWAN_E_INVALID`
- `coord` -- `[0, 1]` on each axis, clamped; null returns `ALWAN_E_INVALID`
- `mode` -- LINEAR (default, resolves to trilinear), NEAREST, TRILINEAR or
  TETRAHEDRAL, optionally `| ALWAN_SAMPLE_STRICT`

The cube is 57^3 x 3, interleaved RGB, r-fastest, the same layout
`alwan_table3d_sample_{T}` above reads. The `57` is fixed by the table and has no
public constant.

> **Pass `ALWAN_SAMPLE_TETRAHEDRAL` to match the view transform.**
> `ALWAN_VIEW_AGX_BLENDER` samples this cube tetrahedrally, unconditionally. The
> zero-value default resolves to trilinear, so a `memset` mode gives a different
> picture from the shipped view.

> **The returned values are power-2.4 encoded. Raise them to the power 2.4 to get
> display-linear**, then apply your own display OETF. Applying the inverse gives
> a wrong picture that still looks plausible.

The view builds `coord` as: Rec.709 linear straight into the
`agx_blender_rec709_to_egamut` matrix with no input clamp, then
`alwan_agx_log_encode(x, -12.47393, 12.5260688117)`, which floors each channel at
`1e-10` after the matrix and SATURATEs the normalised result. Those bounds are
`log2(0.18) + [-10, +15] EV`, a different upper bound from the contrast curve's
`4.026069`. The other AgX views clamp negatives to `0` before their inset matrix;
the Blender view does not, so a negative Rec.709 channel reaches the 3x3 and
cross-contaminates the other two channels before the per-channel floor. Neither
the matrix nor the encoder is on the public surface, so reproducing the view's
coordinate means copying both.

Coordinates are clamped per axis and a NaN axis resolves to the low corner, both
`ALWAN_OK`, unless `ALWAN_SAMPLE_STRICT` is set.

For the AgX pipeline as a whole see
[../picture_formation.md](../picture_formation.md).

---

## See also

- [color-vision-deficiency.md](color-vision-deficiency.md) -- the matrix ramp in use
- [aces.md](aces.md) -- the AgX and Jakob2019 cube readers
- [map.md](map.md) -- bulk and strided conversion APIs
