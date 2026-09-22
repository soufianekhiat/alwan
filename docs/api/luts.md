# LUT Baking, Interchange And Sampling API

Bake a colour pipeline into a lookup table, move that table in and out of
`.cube`, `.spi1d`, `.spi3d`, `.3dl`, `.csp`, `.spimtx` and CLF, sample it, and
invert it.

> **Precision variants:** Every function shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

---

## Overview

Four separate jobs live on this page, and the first three compose in that order:

1. **Bake.** Evaluate a conversion, or a conversion plus a view transform, on a
   regular grid and write the result to a buffer you own.
2. **Interchange.** Write that buffer as `.cube` (plain or with a Resolve
   shaper), `.spi1d`, `.spi3d`, `.3dl`, `.csp` or CLF, or read one back.
3. **Sample.** Interpolate the table at an arbitrary coordinate.
4. **Invert.** Build the cube that undoes one, where the transform allows it.

Nothing here allocates. Every entry point writes into a buffer the caller sized,
and the sizing rules are in [Layout](#layout) below; getting one wrong is the
common way to corrupt a LUT, so they are stated per function as well.

A LUT is a cache of a pipeline, not a colour space. It carries no primaries, no
white point and no transfer function of its own: what it means is whatever
`src_space` and `dst_space` were when it was baked. `.cube` has nowhere to record
that, which is why CLF exists and why the CLF export is the one that survives a
trip through another application unambiguously.

---

## Layout

Two layouts appear throughout, and they are not interchangeable.

**3-D cube, `size^3 * 3` values, R-fastest.** The index of the sample at grid
position `(r, g, b)` is `((b * size + g) * size + r) * 3`. This is the order
`.cube` files use on disk, so an import is a straight read.

**2-D strip, `(size*size) * size * 3` values, row-major RGB interleaved.** The
cube's blue slices are laid side by side into one image:

```
width  = size * size      /* N constant-blue slices, left to right */
height = size
x within a slice = R,  y = G,  slice index = B
```

This is the game-engine convention (Unreal, Unity), which is why it is here: a
strip uploads as one 2-D texture and samples with one bilinear fetch per slice
pair. `alwan_lut2d_dimensions` computes the two numbers so a caller never
open-codes them.

**1-D, `size` values.** A single curve, not per channel.

---

## Baking

### alwan_bake_3dlut_{T}

```c
alwan_status alwan_bake_3dlut_{T}(alwan_{T} *out, int size,
                                  alwan_rgb_space_desc_{T} const *src_space,
                                  alwan_rgb_space_desc_{T} const *dst_space,
                                  alwan_ctx *ctx);
```

Sample an RGB space conversion on a `size^3` grid.

**Parameters:**
- `out` -- `size^3 * 3` values, R-fastest
- `size` -- cube edge length, `2` to `256`, typically 17, 33 or 65
- `src_space`, `dst_space` -- descriptors; see [color-spaces.md](color-spaces.md)
- `ctx` -- needed for chromatic adaptation, may be `NULL` when the white points match

The pipeline is the source EOTF, the combined RGB-to-RGB matrix including the CAT
when white points differ, then the destination OETF.

**Example:**
```c
alwan_f32 lut[33*33*33*3];
alwan_bake_3dlut_f32(lut, 33, &acescg, &srgb, ctx);
```

### alwan_bake_3dlut_view_{T}

```c
alwan_status alwan_bake_3dlut_view_{T}(alwan_{T} *out, int size,
                                       alwan_rgb_space_desc_{T} const *src_space,
                                       alwan_rgb_space_desc_{T} const *dst_space,
                                       alwan_view_transform vt, alwan_ctx *ctx);
```

The same, with a view transform between the matrix and the destination OETF:
`EOTF(src) -> matrix -> view transform -> OETF(dst)`. See
[transfer-functions.md](transfer-functions.md) for the `alwan_view_transform`
values.

This is the call that bakes a display rendering, so it is the one whose cube size
matters: a tone curve has curvature that a 17-cube will visibly facet. Start at 33
and check against the direct call before shipping a smaller one.

### alwan_bake_1dlut_{T}

```c
alwan_status alwan_bake_1dlut_{T}(alwan_{T} *out, int size,
                                  alwan_transfer_function tf, int encode);
```

Sample one transfer function over `[0, 1]`.

**Parameters:**
- `out` -- `size` values
- `size` -- number of samples, `2` to `65536`, typically 256, 1024 or 4096
- `tf` -- the curve to sample
- `encode` -- non-zero for the OETF (linear to encoded), zero for the EOTF

A 1-D LUT cannot carry a matrix, so this is for the curve alone. Pairing it with a
3-D LUT that holds only the matrix is the usual way to keep the cube small: the
curvature goes in the 1-D table where samples are cheap.

### alwan_bake_2dlut_{T}

```c
alwan_status alwan_bake_2dlut_{T}(alwan_{T} *out, int size,
                                  alwan_rgb_space_desc_{T} const *src_space,
                                  alwan_rgb_space_desc_{T} const *dst_space,
                                  alwan_ctx *ctx);
```

`alwan_bake_3dlut_{T}` followed by the flatten, into a `(size*size) * size * 3`
buffer. A convenience: the two-step form is identical.

---

## The 2-D strip

### alwan_lut2d_dimensions

```c
void alwan_lut2d_dimensions(int size, int *width, int *height);
```

`*width = size * size`, `*height = size`. It returns `void` because there is
nothing to fail on; both outputs may not be `NULL`.

### alwan_lut3d_to_2d_{T} / alwan_lut2d_to_3d_{T}

```c
alwan_status alwan_lut3d_to_2d_{T}(alwan_{T} *out, alwan_{T} const *lut3d, int size);
alwan_status alwan_lut2d_to_3d_{T}(alwan_{T} *out, alwan_{T} const *lut2d, int size);
```

Flatten and unflatten. Both are exact rearrangements: no value is interpolated or
altered, so a round trip is bit-identical. `out` must not alias the input.

**Example:**
```c
int w, h;
alwan_lut2d_dimensions(33, &w, &h);          /* 1089 x 33 */
alwan_f32 *strip = malloc((size_t)w * h * 3 * sizeof *strip);
alwan_lut3d_to_2d_f32(strip, lut, 33);
```

---

## Sampling

### alwan_table1d_sample_{T} / alwan_table2d_sample_{T} / alwan_table3d_sample_{T}

```c
alwan_status alwan_table1d_sample_{T}(alwan_{T} *result, alwan_{T} const *table, int size,
                                      alwan_{T} coord, alwan_sample_mode mode);
alwan_status alwan_table2d_sample_{T}(alwan_rgb_{T} *result, alwan_{T} const *strip, int size,
                                      alwan_rgb_{T} const *coord, alwan_sample_mode mode);
alwan_status alwan_table3d_sample_{T}(alwan_rgb_{T} *result, alwan_{T} const *cube, int size,
                                      alwan_rgb_{T} const *coord, alwan_sample_mode mode);
```

Interpolate at a coordinate in `[0, 1]`: `ALWAN_SAMPLE_LINEAR` for the 1-D curve,
`ALWAN_SAMPLE_TRILINEAR` for the strip and the cube (`ALWAN_SAMPLE_TETRAHEDRAL`,
`ALWAN_SAMPLE_NEAREST` and `ALWAN_SAMPLE_STRICT` are the other choices). The strip
sampler reproduces what a GPU does with the same texture, so a preview on the CPU
matches the shader. These are the general table readers of [tables.md](tables.md),
where the addressing contract, including how out-of-range coordinates and NaN
resolve, is documented.

Until 3.0.0 `alwan_lut{1,2,3}d_sample_{T}` existed beside them as one-line delegates
with the mode filled in and the size after the coordinate; one operation had two
spellings, and the delegates are gone.

---

## Inversion

```c
alwan_status alwan_lut3d_invert_{T}(alwan_{T} *out, int out_size,
                                    alwan_{T} const *lut, int size,
                                    int iterations, alwan_{T} *out_worst_residual);
```

Build the cube that undoes a cube. For each node of the output, Newton's method
on the 3x3 system, with the Jacobian by central differences over half a forward
cell and a fixed step count so a deterministic build takes one path. The inverse
is addressed over `[0, 1]` in the **forward cube's output space**.

**Parameters:**
- `out` -- `out_size^3 * 3` values, R-fastest; must not alias `lut`
- `out_size` -- the inverse cube's edge, `2` to `256`
- `lut`, `size` -- the forward cube
- `iterations` -- `<= 0` means 20; above 200 is `ALWAN_E_RANGE`
- `out_worst_residual` -- may be `NULL`; the largest `|F(G(y)) - y|` over the
  inverse's own nodes

> **A cube is not invertible everywhere, and this does not pretend otherwise.**
> Where the forward table flattens, one preimage is as good as another and the
> iteration settles on one. Where a node lies outside the forward table's image
> there is no preimage at all, and the iteration ends on the nearest point it
> can reach. Neither is an error, because neither is one. **The residual is the
> answer**: read it and judge the table against it. A large worst residual on an
> inverse that should be exact says the forward table folds; on one baked from a
> clipping view transform it says only that the clipped region cannot come back.

Cost is `out_size^3 * iterations * 7` trilinear samples.

### What it is worth

Suite 161 does not assert a tolerance and call that a proof. It asserts the
thing that identifies the cause: refine the forward grid and the disagreement
with the analytically baked inverse has to shrink, because the sampled cube
approaches the transform it was baked from. Inverting P3-to-sRGB against a baked
sRGB-to-P3 cube of 33:

| forward grid | worst against the baked inverse | worst node residual |
|---|---|---|
| 9 | 4.13e-02 | 7.9e-06 |
| 17 | 1.64e-02 | 1.0e-07 |
| 33 | 6.36e-03 | 4.8e-09 |

The residual column is the solver and it converges to nothing. The other column
is the grid, and it halves as the grid refines. A solver that was simply wrong
would not improve.

Two cases have exact answers and are checked as such: the identity inverts to
the identity to 0.0, and a cube that clips everything above 0.5 reports a
residual of exactly 0.50, which is the half that cannot come back.

A round trip through both cubes at points inside the cells, rather than on the
nodes, lands within 1.2e-02 for a pair of 33-cubes over a display conversion,
and the worst of it sits in the darks, where a uniformly spaced cube is weakest
against a display curve. That is the reason a shaper exists.

---

## .cube import and export

`.cube` is the Iridas/Adobe format every grading application reads. It stores the
grid and an optional title, and nothing else: no primaries, no transfer function,
no indication of what the table converts from or to.

### Export

```c
alwan_status alwan_cube_export_3d_{T}(char const *path, alwan_{T} const *lut,
                                      int size, char const *title);
alwan_status alwan_cube_export_1d_{T}(char const *path, alwan_{T} const *lut,
                                      int size, char const *title);
alwan_status alwan_cube_export_3d_buffer_{T}(char *buf, size_t buf_size,
                                             size_t *bytes_written,
                                             alwan_{T} const *lut, int size,
                                             char const *title);
```

`title` may be `NULL` to omit the `TITLE` line. The buffer form returns
`ALWAN_E_RANGE` when `buf_size` is too small and sets `*bytes_written` to what it
did write; there is no query mode, so size the buffer generously or write to a
file.

Since the format records nothing about meaning, put it in the title. A file called
`acescg_to_srgb_33.cube` whose title says the same thing is the difference between
a LUT that can be reused next year and one that gets applied twice.

### Import

```c
alwan_status alwan_cube_import_3d_{T}(alwan_{T} *lut, int *out_size, char const *path);
alwan_status alwan_cube_import_1d_{T}(alwan_{T} *lut, int *out_size, char const *path);
alwan_status alwan_cube_import_3d_buffer_{T}(alwan_{T} *lut, int *out_size,
                                             char const *buf, size_t buf_len);
```

The caller allocates. **Pass `lut = NULL` to query the size first**: the header is
parsed, `*out_size` is set, and no pixel data is read. That is the intended
two-pass idiom, since the file decides the size.

**Example:**
```c
int size = 0;
if (alwan_cube_import_3d_f32(NULL, &size, path) != ALWAN_OK) return;
alwan_f32 *lut = malloc((size_t)size*size*size*3 * sizeof *lut);
alwan_cube_import_3d_f32(lut, &size, path);
```

`lut` must match the call's precision: `alwan_f32` for `_f32`, `alwan_f64` for
`_f64`. A 3-D cube is accepted from 2 to 256 a side and a 1-D from 2 to 65536
entries; outside that the import returns `ALWAN_E_RANGE` rather than allocating
against a number the file chose.

> **The 1-D import averages the three columns.** A `.cube` 1-D table has an RGB
> triplet per row, and `alwan_cube_import_1d_{T}` stores `(r + g + b) / 3` in the
> single channel it keeps. For the usual file, where a 1-D LUT is one curve
> written out three times, that is exact. For a file carrying three genuinely
> different per-channel curves it is not: import it as three separate passes, or
> keep it as a 3-D LUT.

---

## .spi1d, .spi3d, .3dl, .csp and .spimtx

Five more interchange formats beside `.cube`, plus the Resolve `.cube` that
carries a shaper, and the same caveat applies to all of them: they record
numbers, not meaning. What a table converts from and to is
yours to track.

The caller allocates and `lut = NULL` queries the size, the same two-pass idiom
the `.cube` importers use. A 3D LUT is held R-fastest in memory, as `.cube`
stores it; `.spi3d` and `.3dl` are B-fastest on disk and both directions
transpose.

### Sony Pictures Imageworks

```c
alwan_status alwan_spi1d_import_{T}(alwan_{T} *lut, int *out_size, int *out_channels,
                                    alwan_{T} *out_domain, char const *path);
alwan_status alwan_spi1d_export_{T}(char const *path, alwan_{T} const *lut, int size,
                                    int channels, alwan_{T} domain_min, alwan_{T} domain_max);

alwan_status alwan_spi3d_import_{T}(alwan_{T} *lut, int *out_size, char const *path);
alwan_status alwan_spi3d_export_{T}(char const *path, alwan_{T} const *lut, int size);
```

`.spi1d` carries one or three components and an input domain, and both are
reported rather than assumed. The domain is **reported, never applied**: alwan's
1-D samplers address `[0, 1]`, so a file whose `From` line reads `-0.5 1.5` has
to be mapped by the caller before the table is sampled. `out_channels` and
`out_domain` may be `NULL` when you do not care. Three components means
`size * 3` values interleaved, not three separate tables.

`.spi3d` writes an explicit index triple per line, so the file's line order does
not matter to the reader and a truncated file is caught by the sample count
rather than by silently reading a partial cube. Only cubic tables are accepted;
unequal dimensions are `ALWAN_E_RANGE`.

### Autodesk .3dl

```c
alwan_status alwan_3dl_import_{T}(alwan_{T} *lut, int *out_size, char const *path);
alwan_status alwan_3dl_export_{T}(char const *path, alwan_{T} const *lut, int size,
                                  int bit_depth);
```

One format in two flavours. Flame writes a mesh line and the data; Lustre puts
`3DMESH` and `Mesh <e> <d>` above it, where the cube edge is `2^e + 1` and `d`
is the output bit depth, and a `LUT8` trailer below. The reader takes either.

> **The values are integers, so the bit depth decides what they mean.** Lustre
> states it and that is used. Flame does not, and the depth is then inferred as
> the smallest of 8, 10, 12, 14 and 16 bits that holds the largest value in the
> file. A table that never reaches its own maximum reads one stop bright, and
> nothing in the file can prevent that. This is what every other reader does.

On export, `bit_depth` is 8, 10, 12, 14 or 16, and `<= 0` means 12, which is
what the Lustre writers use. Values are scaled, rounded and clamped into range,
so anything outside `[0, 1]` is lost: the format cannot carry it. The Lustre
header goes on where the size is `2^e + 1`, the only shape that header can
state, and the Flame flavour is written otherwise.

> **Size 3 is ambiguous in the Flame flavour, and readers differ.** A mesh line
> at that size holds three numbers and looks exactly like a data line. It is
> still decidable: with `L` three-number lines, `L` a perfect cube means every
> line is data and `L - 1` a perfect cube means the first is the mesh, and
> consecutive cubes differ by more than one so both cannot hold. OCIO 2.5 does
> not make that distinction and cannot read back the size-3 `.3dl` it writes, in
> either flavour. This reader can. Suite 161 pins the case.

### Cinespace .csp

```c
alwan_status alwan_csp_import_3d_{T}(alwan_{T} *lut, int *out_size,
                                     alwan_{T} *prelut_in, alwan_{T} *prelut_out,
                                     int *out_prelut_size, char const *path);
alwan_status alwan_csp_export_3d_{T}(char const *path, alwan_{T} const *lut, int size,
                                     alwan_{T} const *prelut_in, alwan_{T} const *prelut_out,
                                     int const *prelut_size);
```

`.csp` is R-fastest on disk, like `.cube`, unlike the two above. What makes it
different is the **prelut**: a per-channel piecewise-linear remap applied to the
input before the cube is addressed, which is how a shaper for log material is
stored. Each channel carries its own point count, and they need not agree.

> **A prelut is never dropped silently.** It cannot be delivered through a
> cube-only signature, so passing `NULL` for `prelut_in` and `prelut_out` against
> a file whose prelut is not the identity returns `ALWAN_E_INVALID`. The ordinary
> file, whose prelut is the two-point identity every writer emits when there is
> no shaper, reads with `NULL`. Query first with `lut = NULL` to size the
> buffers: the cube edge and the three counts are set from the header.

The prelut buffers are packed channel after channel, by the counts reported:
channel 0's points, then channel 1's, then channel 2's. On export, pass all
three prelut arguments or none; none writes the two-point identity.

A prelut of more than 1024 points per channel is taken as a real shaper rather
than compared point by point, so it is refused when no buffers are supplied.

### Sony .spimtx

```c
alwan_status alwan_spimtx_import_{T}(alwan_mat3x3_{T} *matrix, alwan_{T} *offset,
                                     char const *path);
alwan_status alwan_spimtx_export_{T}(char const *path, alwan_mat3x3_{T} const *matrix,
                                     alwan_{T} const *offset);
```

Twelve numbers: three rows of a 3x3 matrix, each followed by that channel's
offset. `offset` may be `NULL` on both sides; on export that writes zeros.

> **The file holds the offsets in 16-bit code units.** An offset of 65535 on disk
> adds exactly 1.0 to the channel. This was measured against OCIO rather than
> read off a specification, and suite 161 pins it. These entry points take and
> return the offset in the data's own units, so the file's convention stays in
> the file.

### The Resolve .cube: a shaper and a cube in one file

```c
alwan_status alwan_cube_import_3d_shaper_{T}(alwan_{T} *lut, int *out_size,
                                             alwan_{T} *shaper, int *out_shaper_size,
                                             alwan_{T} *out_range, char const *path);
alwan_status alwan_cube_export_3d_shaper_{T}(char const *path, alwan_{T} const *lut, int size,
                                             alwan_{T} const *shaper, int shaper_size,
                                             alwan_{T} range_min, alwan_{T} range_max,
                                             char const *title);
```

A Resolve `.cube` may carry `LUT_1D_SIZE` and `LUT_3D_SIZE` together: a
per-channel curve, uniformly sampled over `LUT_1D_INPUT_RANGE`, whose output
addresses the cube. It is how a cube stays small over log material, and it is
the same idea as the `.csp` prelut stored as a uniform table instead of
irregular pairs.

> **The shaper's rows carry no marker of their own.** They sit above the cube's
> rows, so a reader that skips only the keyword takes the first of them as cube
> samples. `alwan_cube_import_3d_{T}` therefore refuses a file with a
> `LUT_1D_SIZE` outright, with `ALWAN_E_INVALID`, and `alwan_cube_import_1d_{T}`
> refuses one with a `LUT_3D_SIZE`, because neither table is the transform on
> its own.

A file with no shaper reads through this call too, reporting a shaper size of 0
and leaving the buffer untouched, so one call takes any `.cube`. On export,
`shaper_size = 0` writes a plain `.cube` that the ordinary reader takes.

### What is checked

Suite 161 holds each reader to OpenColorIO's own evaluation of the same file at
the cube's grid nodes, where interpolation is the identity, so a difference is
the parse and nothing else. The transform behind the fixtures has channel
crosstalk on purpose: a symmetric table would read the same whichever way round
the axes went, and an ordering mistake would survive.

The writers are not compared against OCIO's bytes, since two writers can differ
in spacing and digits and mean the same table. They are read back through the
reader that was just pinned: `.spi3d`, `.spi1d` and `.csp` round trip bit for
bit, prelut included, and `.3dl` to within half a step of the depth asked for.

Two cases need a different bound, and the suite says why.

- A `.csp` with a real shaper is checked end to end: the prelut alwan read is
  applied, the cube is sampled, and the result must land where OCIO lands. OCIO
  resamples a prelut onto a uniform grid before applying it rather than
  interpolating the file's own points, so the two agree exactly at the ends and
  differ by about a thousandth through a curved shaper. That bound still catches
  every structural mistake, which miss by a tenth or more. The exact parse is
  pinned separately on a file the generator wrote, whose three prelut blocks have
  different point counts.
- `.spimtx` has no grid at all, so it is applied to probe points and compared,
  and its matrix and offsets are pinned entry by entry.
- The Resolve shaper is uniformly sampled, so OCIO applies it without that
  resampling and the composition holds to float32 round-off, 7.7e-08.

---

## CLF export

CLF (Common LUT Format, SMPTE ST 2136-1:2024) is XML, and unlike `.cube` it
records the operations rather than only their samples. OCIO, ACES, DaVinci Resolve
and Baselight read it.

```c
alwan_status alwan_clf_export_{T}(char const *path,
                                  alwan_rgb_space_desc_{T} const *src_space,
                                  alwan_rgb_space_desc_{T} const *dst_space,
                                  char const *id, char const *name,
                                  int lut_size, alwan_ctx *ctx);

alwan_status alwan_clf_export_view_{T}(char const *path,
                                       alwan_rgb_space_desc_{T} const *src_space,
                                       alwan_rgb_space_desc_{T} const *dst_space,
                                       alwan_view_transform view,
                                       char const *id, char const *name,
                                       int lut_size, alwan_ctx *ctx);

alwan_status alwan_clf_export_buffer_{T}(char *buf, size_t *bytes_written,
                                         size_t buf_size, /* ...as above... */);
alwan_status alwan_clf_export_view_buffer_{T}(char *buf, size_t *bytes_written,
                                              size_t buf_size, /* ...as above... */);
```

The conversion is decomposed into ProcessNodes rather than flattened into one
cube:

```
EOTF (Exponent or LUT1D)
  -> Matrix    (src RGB -> XYZ)
  -> Matrix    (CAT, only when the white points differ)
  -> Matrix    (XYZ -> dst RGB)
  -> Range     (gamut clamp)
  -> OETF (Exponent or LUT1D)
```

The `_view` forms insert a LUT3D node for the view transform between the matrices
and the OETF.

**Parameters:**
- `id` -- the ProcessList `id` attribute, `NULL` for a default
- `name` -- the ProcessList `name` attribute, `NULL` to omit
- `lut_size` -- resolution of any baked LUT1D node; below 2 it defaults to 4096
- `ctx` -- for the CAT, may be `NULL` when the white points match

A curve that alwan can express as a pure power becomes an `Exponent` node and
carries no sampling error at all. Only the curves that need one get a LUT1D, which
is where `lut_size` applies.

Note the argument order on the buffer forms: `bytes_written` comes before
`buf_size`, the opposite of `alwan_cube_export_3d_buffer_{T}`. Both are
`ALWAN_E_RANGE` when the buffer is too small.

> **The LUT3D array is written in CLF's order, which is not alwan's.** CLF has
> blue varying fastest and red slowest; alwan holds a cube R-fastest, as `.cube`
> stores it. The writer treated the two as the same until 2026-09-18, so every
> CLF exported with a view transform before that has its red and blue axes
> exchanged. Greys were unaffected, which is why it went unnoticed. Suite 80
> pins the order now.

---

## CLF import

```c
typedef struct alwan_clf_s alwan_clf;

typedef enum {
    ALWAN_CLF_NODE_MATRIX, ALWAN_CLF_NODE_RANGE, ALWAN_CLF_NODE_EXPONENT,
    ALWAN_CLF_NODE_LUT1D,  ALWAN_CLF_NODE_LUT3D,  ALWAN_CLF_NODE_ASC_CDL,
    ALWAN_CLF_NODE_LOG
} alwan_clf_node_type;

alwan_status alwan_clf_import(alwan_clf **out, char const *path, alwan_ctx *ctx);
alwan_status alwan_clf_import_buffer(alwan_clf **out, char const *buf, size_t len, alwan_ctx *ctx);
void alwan_clf_destroy(alwan_clf *clf, alwan_ctx *ctx);

size_t alwan_clf_node_count(alwan_clf const *clf);
alwan_status alwan_clf_node_type_at(alwan_clf_node_type *out, alwan_clf const *clf, size_t index);
char const *alwan_clf_id(alwan_clf const *clf);

alwan_status alwan_clf_apply_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                                alwan_{T} const *in, size_t in_stride,
                                                size_t count, alwan_clf const *clf);
```

Read a ProcessList and evaluate it, so an ACES LMT or an OCIO transform comes
into alwan rather than only out of it. The object owns its tables; free it with
`alwan_clf_destroy`. `ctx` is taken for symmetry and not used.

Every ProcessNode type CLF defines is understood: Matrix (3x3 or 3x4), Range,
Exponent (`basicFwd`, `basicRev`, `monCurveFwd`, `monCurveRev`), LUT1D, LUT3D,
ASC_CDL (`Fwd`, `Rev`, `FwdNoClamp`, `RevNoClamp`) and Log (`log2`, `log10`,
`antiLog2`, `antiLog10`, `linToLog`, `logToLin`, `cameraLinToLog`,
`cameraLogToLin`).

> **A file carrying anything else is refused, not partly applied.** A ProcessList
> missing one of its stages is not the transform, and a wrong answer is worse
> than none, so one of the nodes OCIO writes into a CTF but CLF does not define,
> such as `ExposureContrast` or `FixedFunction`, returns `ALWAN_E_NODATA` for the
> whole file. The same goes for a style one of the seven does not list.

Evaluation runs in double on both precision paths, because a CLF is decimal text
and there is nothing an f32 pass would preserve.

### What the nodes mean

None of this was transcribed from the specification. Each node was written out,
evaluated by OpenColorIO, and the pair is what suite 162 holds the reader to:

| node | meaning |
|---|---|
| Range | `(in - minIn) * (maxOut - minOut) / (maxIn - minIn) + minOut`, clamped to `[minOut, maxOut]` unless `style="noClamp"` |
| basicFwd / basicRev | `max(x, 0)^g` and `max(x, 0)^(1/g)` |
| monCurveFwd | a linear segment below `xb = a / (g - 1)`, then `((x + a) / (1 + a))^g`; the segment's slope is `yb / xb` with `yb` the curve at `xb`, so value and slope both carry across, and negative input stays on the segment |
| monCurveRev | the same with the axes exchanged |
| ASC_CDL | `(in * slope + offset)^power` per channel, then the saturation about the Rec. 709 luma. `Fwd` and `Rev` hold the SOP result in `[0, 1]` before the power and the final result after the saturation; the NoClamp pair do neither and pass a negative through the power unchanged |
| log2 / log10 | the logarithm, with the input floored at the smallest normal float32, so `log10(0)` is -37.9298 and `log2(0)` is -126 |
| linToLog | `logSideSlope * log_base(linSideSlope * x + linSideOffset) + logSideOffset`, with the same floor |
| logToLin | that inverted |
| cameraLinToLog | the same curve with a linear segment below `linSideBreak`, whose slope is the curve's own slope there unless `linearSlope` is given |
| cameraLogToLin | that inverted |

Worst against OCIO over twenty-five cases: 5.3e-03 on `logToLin`, whose answer
reaches 1585, which is 3.4e-06 of the value. Held per case at 3e-05 of the case's
own peak, because OCIO evaluates in float32 and its `pow` is a fast
approximation. The Matrix, Range and LUT cases sit at 1e-8.

> **One trap, recorded because it produced four confident wrong answers before
> it was caught.** OCIO caches a processor against the file *path*. Reusing a
> filename while probing returns the first file's transform for every later one,
> and the mismatch looks exactly like a formula error. Give every probe file its
> own name.

---

## CLF's parametric curves, without a file

```c
typedef enum { ALWAN_CLF_EXPONENT_BASIC, ALWAN_CLF_EXPONENT_MONCURVE } alwan_clf_exponent_style;
typedef struct { alwan_clf_exponent_style style; alwan_f64 gamma, offset; } alwan_clf_exponent_params;

typedef enum { ALWAN_CLF_LOG_PLAIN, ALWAN_CLF_LOG_LIN_TO_LOG, ALWAN_CLF_LOG_CAMERA } alwan_clf_log_style;
typedef struct {
    alwan_clf_log_style style;
    alwan_f64 base;
    alwan_f64 log_side_slope, log_side_offset, lin_side_slope, lin_side_offset;
    alwan_f64 lin_side_break, linear_slope;
} alwan_clf_log_params;

void alwan_clf_exponent_params_init(alwan_clf_exponent_params *params);
void alwan_clf_log_params_init(alwan_clf_log_params *params);

alwan_status alwan_clf_exponent_apply_{T}(alwan_{T} *out, size_t out_stride, alwan_{T} const *in, size_t in_stride,
                                          size_t count, alwan_clf_exponent_params const *params, int inverse);
alwan_status alwan_clf_log_apply_{T}(alwan_{T} *out, size_t out_stride, alwan_{T} const *in, size_t in_stride,
                                     size_t count, alwan_clf_log_params const *params, int inverse);
```

The Exponent and Log ProcessNodes are two families of curve with a handful of
parameters each, and most camera encodings are one of them. These evaluate a
curve from its parameters, forward or with `inverse` non-zero in the Rev /
logToLin direction, with exactly the arithmetic the reader gives the same node
in a file: the entry points build the reader's own node and call its
evaluator, and suite 178 holds the two bit for bit. So a curve published as
CLF or OpenColorIO parameters is reachable without writing the file. The
values are single channels, `count` of them, strides in bytes with 0 meaning
packed as for `alwan_clf_apply`; evaluation runs in double on both paths.

The curves, as the reader defines them (see "What the nodes mean" above):

| style | forward | reverse |
|---|---|---|
| `EXPONENT_BASIC` | max(x, 0)^g | max(x, 0)^(1/g) |
| `EXPONENT_MONCURVE` | a linear segment below x_b = a / (g - 1), joined in value and slope to ((x + a) / (1 + a))^g; negatives stay on the segment | the same with the axes exchanged |
| `LOG_PLAIN` | log_base(x), the input floored at the smallest normal float32 as OCIO floors it | base^x |
| `LOG_LIN_TO_LOG` | log_side_slope log_base(lin_side_slope x + lin_side_offset) + log_side_offset, the same floor | solved for x |
| `LOG_CAMERA` | `LIN_TO_LOG` with a linear segment below `lin_side_break`, of slope `linear_slope`, or the curve's own slope at the break when that is 0 | the same with the axes exchanged |

ACEScct is `LOG_CAMERA` in base 2 with `log_side_slope` 1/17.52,
`log_side_offset` 9.72/17.52, unit `lin_side_slope`, zero `lin_side_offset`
and `lin_side_break` 0.0078125; the derived slope is the standard's
10.5402377416545, and suite 178 holds that curve to S-2016-001's formula at
3e-15 over 401 points. LogC3 EI 800 is the same shape in base 10 with ARRI's
published five parameters.

`ALWAN_E_INVALID` for a NULL, a style outside the enum, or parameters the
curve cannot be built from: gamma not above 0, a `MONCURVE` gamma not above 1
or a negative offset (the segment ends at a / (g - 1)), a base not above 0 or
equal to 1, a zero slope on either side, or a `CAMERA` break where the log's
argument is not positive.

Measured against OpenColorIO 2.5's `ExponentTransform`,
`ExponentWithLinearTransform`, `LogTransform`, `LogAffineTransform` and
`LogCameraTransform`, 28 cases of 12 probes, forward and inverse: worst
2.4e-5 of max(|value|, 1), which is OCIO's float32 (its logarithm puts log2(1)
at 1.3e-5); the f32 path is the f64 one rounded once (5.7e-8).

---

## Choosing a size

| what is baked | size that holds up |
|---|---|
| matrix only, both spaces linear | 2 is exact; the transform is affine |
| matrix plus ordinary display curves | 17 to 33 |
| anything with a view transform or tone curve | 33, verified against the direct call |
| a curve alone, as 1-D | 1024, or 4096 for a log encoding |

The test is not how the cube looks: bake it, sample the same input through the LUT
and through the direct conversion, and look at the largest difference over a few
thousand random inputs. A cube that is one step too coarse shows up there long
before it shows up in a picture, and it shows up first in the darks, where a log
encoding spends most of its samples.

---

## Determinism

The `.cube` and CLF writers are part of the determinism contract: the same build
produces byte-identical files across platforms, which is what makes a LUT
reviewable in a diff. See [determinism.md](../determinism.md).

---

## Error codes

| code | when |
|---|---|
| `ALWAN_OK` | success |
| `ALWAN_E_INVALID` | null pointer, unknown transfer function, **a file that cannot be opened**, or a malformed data line |
| `ALWAN_E_RANGE` | size outside `[2, 256]` for a cube or `[2, 65536]` for a curve, a data count that disagrees with the declared size, or a `_buffer` export that does not fit |
| `ALWAN_E_NODATA` | the file parsed but declared no `LUT_3D_SIZE` / `LUT_1D_SIZE` |

A missing file is `ALWAN_E_INVALID`, not `ALWAN_E_NODATA`: the path could not be
opened, which is a bad argument. `ALWAN_E_NODATA` is narrower and means the file
was read and contained no LUT.

---

## See also

- [tables.md](tables.md) -- the general table readers behind the samplers, with the modes and the bounds policy
- [color-spaces.md](color-spaces.md) -- building the `alwan_rgb_space_desc_{T}` a bake needs
- [transfer-functions.md](transfer-functions.md) -- `alwan_transfer_function` and `alwan_view_transform`
- [determinism.md](../determinism.md) -- byte-identical file export
