# 2D Importance Sampling

Draw points distributed like a greyscale image: an environment map's luminance for
light sampling, a mask, any non-negative weight map. The image is a density over a
rectangle, constant on each pixel, `w / (sum of pixels x pixel area)`. A point is drawn
by inverting two CDFs: a row from the marginal (the distribution over the rows, from each
row's sum), then a column of that row from its conditional distribution. Each `u` lands at
its proportional place inside the pixel it picks, so the point is uniform within the
pixel.

`prepare` builds the sampler once; `sample` draws from it.

## Modes

```c
typedef enum {
    ALWAN_IMPORTANCE_SAMPLING_2D_DIRECT = 0,
    ALWAN_IMPORTANCE_SAMPLING_2D_SEARCH = 1
} alwan_importance_sampling_2d_mode;
```

| | DIRECT | SEARCH |
|---|---|---|
| prepare keeps | each distribution's inverse CDF, tabulated at `u = k / r` | the weights and every CDF |
| a sample costs | one index and a linear blend per axis, O(1) | a bisection per axis, O(log n) |
| the points | follow the image up to the tabulation error | exactly the image's distribution |
| the pdf returned | the density this sampler draws from | the image's: pixel / integral |
| memory, elements | `height x (resolution_x + 1) + resolution_y + 1` | about `2 x width x height` |

DIRECT's blend is the exact inverse wherever no CDF breakpoint falls between two nodes,
so a uniform image, or one region whose CDF is linear, is sampled exactly. Elsewhere the
points follow the image a little less closely, and where a row has empty pixels between
lit ones, a few land in the gap. Because the pdf it returns is its own density, an
estimate `E[f / pdf]` built on it stays unbiased; what a coarse table loses is variance,
not correctness. On a 64 x 32 sky with a sun 1e4 above the horizon (suite 280), the
distance between DIRECT's and SEARCH's histograms of 1e6 points falls with the table:

| `resolution` | 1 x pixels (default) | 2 x | 4 x | 8 x |
|---|---|---|---|---|
| total variation from SEARCH | 0.055 | 0.027 | 0.013 | 0.006 |

Two discs of weights 1 and 4 at the default resolution: SEARCH puts no point in the gap
and recovers the weighted share to 1e-6; DIRECT puts 0.55% of its points in the gap and
its estimate of the image's integral is still within 4e-4.

## Prepare

```c
typedef struct {
    /* the domain [x0, x1] x [y0, y1]; all four 0 for [0, 1] x [0, 1], else x1 > x0, y1 > y0 */
    alwan_scalar_{T} domain_min_x, domain_min_y, domain_max_x, domain_max_y;
    /* DIRECT: the intervals of each row's inverse table and of the marginal's; 0 for width and height */
    size_t resolution_x, resolution_y;
} alwan_importance_sampling_2d_params_{T};

alwan_status alwan_importance_sampling_2d_prepare_{T}(alwan_importance_sampling_2d_{T} **out,
                                                      alwan_scalar_{T} const *image, size_t row_stride,
                                                      size_t width, size_t height,
                                                      alwan_importance_sampling_2d_mode mode,
                                                      alwan_importance_sampling_2d_params_{T} const *params,
                                                      alwan_ctx *ctx);
void alwan_importance_sampling_2d_destroy_{T}(alwan_importance_sampling_2d_{T} *sampler, alwan_ctx *ctx);
```

- `image`: `width x height` weights, rows `row_stride` bytes apart (0 for packed). Pixel
  `(i, j)` covers `[x0 + i dx, x0 + (i + 1) dx] x [y0 + j dy, y0 + (j + 1) dy]`, row 0 at
  `y0`.
- A negative or non-finite weight, or an image whose weights are all 0, is
  `ALWAN_E_INVALID`; `*out` is set to NULL on any failure.
- `params` may be NULL for the unit square and the default resolution.
- The tables hold fewer than 2^31 elements, else `ALWAN_E_RANGE` (offsets are ints, for a
  shader).

## Sample, pdf, invert

```c
alwan_status alwan_importance_sampling_2d_sample_{T}(alwan_vec2_{T} *xy_out, alwan_scalar_{T} *pdf_out,
                                                     size_t *offset_out, alwan_vec2_{T} const *u,
                                                     alwan_importance_sampling_2d_{T} const *sampler);
alwan_status alwan_importance_sampling_2d_sample_{T}_map_interleave(alwan_scalar_{T} *out, size_t out_stride,
                                                                    alwan_scalar_{T} const *in, size_t in_stride,
                                                                    size_t count,
                                                                    alwan_importance_sampling_2d_{T} const *sampler);
alwan_status alwan_importance_sampling_2d_pdf_{T}(alwan_scalar_{T} *pdf_out, alwan_vec2_{T} const *xy,
                                                  alwan_importance_sampling_2d_{T} const *sampler);
alwan_status alwan_importance_sampling_2d_invert_{T}(alwan_vec2_{T} *u_out, alwan_vec2_{T} const *xy,
                                                     alwan_importance_sampling_2d_{T} const *sampler);
```

- `sample`: `u = (u0, u1)` in `[0, 1]`, `u1` choosing the row and `u0` the column. The pdf
  is against the domain's area; `pdf_out` and `offset_out` (the pixel, column then row)
  may be NULL. `u` outside `[0, 1]` or NaN is `ALWAN_E_RANGE`.
- `_map_interleave`: `in` holds `(u0, u1)` per sample, `out` receives `(x, y, pdf)`, both
  strided in bytes (0 for packed). It stops at the first `u` outside `[0, 1]` with
  `ALWAN_E_RANGE`, the samples before it written.
- `pdf`: the density as the sampler draws, 0 outside the domain. A sample's own pdf equals
  `pdf` at its point.
- `invert`: the `u` the sampler turns into a point, `ALWAN_E_RANGE` outside the domain.
  Sampling then inverting returns `u` to 2e-15 in both modes.

## On a GPU

The sampling lives in `core/alwan_importance_sampling_reader.inc`, written against an
accessor, `ALWAN_IS2D_READ(i)`, rather than a pointer, so a shader samples the same tables
with the same arithmetic:

```c
alwan_status alwan_importance_sampling_2d_get_layout_{T}(alwan_importance_sampling_2d_layout_{T} *out,
                                                         alwan_importance_sampling_2d_{T} const *sampler);
```

hands back the prepared table, its element count, the image's size, the resolution, the
mode, the domain and the image's integral. Upload the table and include the reader under
a name:

```hlsl
StructuredBuffer<float> Sky : register(t3);
#define ALWAN_IS2D_NAME     sky
#define ALWAN_IS2D_READ(i)  Sky[i]
#include "core/alwan_importance_sampling_reader.inc"

alwan_vec3 s = alwan_is2d_sample_2d_direct_sky(nu, nv, ru, rv, x0, y0, x1, y1, u0, u1); /* x, y, pdf */
```

`alwan_is2d_sample_2d_search_*`, `alwan_is2d_pdf_2d_*` and `alwan_is2d_invert_2d_*` are
there too. The core compiles with dxc and fxc in the fast and deterministic builds.

## Determinism

Preparing and sampling use only `+ - * /` and comparisons, so the deterministic build
draws the same bits as the ordinary one: suite 280 hashes 4096 samples per mode and
precision and requires the same value in both.

## Validation (suite 280)

Against analytic ground truth on synthetic images, both modes:

- a constant 0.5 map over the sphere's `(phi, theta)` rectangle: every pdf is 1 / area,
  a (0, m, 2)-net of 2^17 points puts exactly 64 in each of the 64 x 32 pixels, and
  `E[f sin(theta) / pdf]` is 2 pi to 4.8e-11;
- one rectangle: no point outside it, the pdf 1 / its area to 1.8e-14, the integral of
  `x y` over it to 1.9e-5 with 2^16 points;
- one disc: every point on a lit pixel, `E[1 / pdf]` the lit area (exact for SEARCH,
  2.8e-6 for DIRECT), the centroid within 5e-6;
- two discs of weights 1 and 4: the share per disc, and the integral, as above;
- the pdf integrates to 1 (SEARCH summed over the pixels, 1 + 1e-15; DIRECT per axis,
  exact), and a sample's pdf equals `pdf` at its point.
