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
                                           int rows, int stride,
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

## See also

- [color-vision-deficiency.md](color-vision-deficiency.md) -- the matrix ramp in use
- [aces.md](aces.md) -- the AgX and Jakob2019 cube readers
- [map.md](map.md) -- bulk and strided conversion APIs
