# LUT Baking, Interchange And Sampling API

Bake a colour pipeline into a lookup table, move that table in and out of `.cube`
and CLF, and sample it.

> **Precision variants:** Every function shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

---

## Overview

Three separate jobs live on this page, and they compose in that order:

1. **Bake.** Evaluate a conversion, or a conversion plus a view transform, on a
   regular grid and write the result to a buffer you own.
2. **Interchange.** Write that buffer as `.cube` or CLF, or read one back.
3. **Sample.** Interpolate the table at an arbitrary coordinate.

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
pair. `alwan_lut2d_dimensions_{T}` computes the two numbers so a caller never
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

### alwan_lut2d_dimensions_{T}

```c
void alwan_lut2d_dimensions_{T}(int size, int *width, int *height);
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
alwan_lut2d_dimensions_f32(33, &w, &h);          /* 1089 x 33 */
alwan_f32 *strip = malloc((size_t)w * h * 3 * sizeof *strip);
alwan_lut3d_to_2d_f32(strip, lut, 33);
```

---

## Sampling

### alwan_lut1d_sample_{T} / alwan_lut2d_sample_{T} / alwan_lut3d_sample_{T}

```c
alwan_status alwan_lut1d_sample_{T}(alwan_{T} *result, alwan_{T} const *lut,
                                    alwan_{T} t, int size);

alwan_status alwan_lut2d_sample_{T}(alwan_rgb_{T} *result, alwan_{T} const *lut2d,
                                    alwan_rgb_{T} const *rgb, int size);

alwan_status alwan_lut3d_sample_{T}(alwan_rgb_{T} *result, alwan_{T} const *lut,
                                    alwan_rgb_{T} const *rgb, int size);
```

Interpolate at a coordinate in `[0, 1]`: linear for the 1-D curve, trilinear for
the strip and the cube. The strip sampler reproduces what a GPU does with the
same texture, so a preview on the CPU matches the shader.

These three are one-line delegates to the general table readers in
[tables.md](tables.md), passing LINEAR, TRILINEAR and TRILINEAR. Reach for
`alwan_table3d_sample_{T}` instead when you want tetrahedral interpolation,
nearest, or `ALWAN_SAMPLE_STRICT`; the addressing contract, including how
out-of-range coordinates and NaN resolve, is documented there and applies here
unchanged.

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
