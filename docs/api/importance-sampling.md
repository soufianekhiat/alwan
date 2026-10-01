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
    ALWAN_IMPORTANCE_SAMPLING_2D_SEARCH = 1,
    ALWAN_IMPORTANCE_SAMPLING_2D_ALIAS = 2
} alwan_importance_sampling_2d_mode;
```

| | DIRECT | SEARCH | ALIAS |
|---|---|---|---|
| prepare keeps | each distribution's inverse CDF, tabulated at `u = k / r` | the weights and every CDF | the weights and an alias table over all the pixels |
| a sample costs | one index and a linear blend per axis, O(1) | a bisection per axis, O(log n) | one index and one comparison, O(1) |
| the points | follow the image up to the tabulation error | exactly the image's distribution | exactly the image's distribution |
| the pdf returned | the density this sampler draws from | the image's: pixel / integral | SEARCH's, to the bit |
| u to point | monotone per axis, invertible | monotone per axis, invertible | scattered: no inverse |
| memory, elements | `height x (resolution_x + 1) + resolution_y + 1` | about `2 x width x height` | `3 x width x height + 1` |

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

### ALIAS

Walker's alias method (1977) in Vose's numerically stable construction (1991), over all
`width x height` pixels at once rather than as a marginal and conditionals. Prepare scales
each weight to `q_i = w_i n / sum` (in double whatever the precision) and tops every `q_i`
below 1 up from one above 1, which becomes its alias; a slot then holds a threshold and an
alias. `u1` picks slot `k = floor(u1 n)`; the fraction left over keeps pixel `k` if it is
below the threshold, else takes the alias. What is left of that fraction places the point
down the pixel, `u0` across it.

- Exact like SEARCH and O(1) like DIRECT: the distribution the table implies (each pixel's
  threshold plus what the slots naming it leave) is the image's to 2e-15, and the pdf a
  sample reports is SEARCH's bit for bit (suite 280).
- The point's place down its pixel comes from what `u1` has left after the slot and the
  comparison, `log2(width x height)` fewer bits than `u1` had: fine in double, coarse in
  single precision on a large image. The pixel and the pdf are exact either way.
- The map from `u` to the point is not monotone: a stratified or low-discrepancy `u` does
  not stay stratified (use SEARCH where that matters), and `invert` returns
  `ALWAN_E_INVALID`.
- At most 2^24 pixels in single precision, where the alias is held as a value, else
  `ALWAN_E_RANGE`.

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
  Sampling then inverting returns `u` to 2e-15 in DIRECT and SEARCH; ALIAS has no inverse
  and returns `ALWAN_E_INVALID`.

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

`alwan_is2d_sample_2d_search_*`, `alwan_is2d_sample_2d_alias_*`, `alwan_is2d_pdf_2d_*` and
`alwan_is2d_invert_2d_*` are there too. The core compiles with dxc and fxc in the fast and deterministic builds.

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
- ALIAS: the table's implied distribution against the image to 2.1e-15 on three images,
  its pdf equal to SEARCH's on 2^18 random points, none off the lit pixels, a chi-square
  of 925 over 840 degrees of freedom, the weighted blobs' shares within 0.3 standard
  deviations, and its own determinism hash.

## Environment maps

An equirectangular map position `(x, y)` in `[0, 1]^2` is the direction at azimuth
`phi = 2 pi x` and polar angle `theta = pi y` from +Y:

    (sin theta cos phi, cos theta, sin theta sin phi)

so the map's top row looks up. A density `p` over the map's area is the solid-angle
density `p / (2 pi^2 sin theta)`.

```c
alwan_status alwan_env_equirect_direction_{T}(alwan_vec3_{T} *dir_out, alwan_vec2_{T} const *xy);
alwan_status alwan_env_equirect_position_{T}(alwan_vec2_{T} *xy_out, alwan_vec3_{T} const *dir);

typedef enum { ALWAN_ENV_LOBE_COSINE = 0, ALWAN_ENV_LOBE_PHONG = 1 } alwan_env_lobe_kind;
typedef struct {
    alwan_env_lobe_kind kind;
    alwan_scalar_{T} axis_x, axis_y, axis_z;   /* the normal, or the reflected direction */
    alwan_scalar_{T} exponent;                 /* PHONG; 0 for 1 */
} alwan_env_lobe_{T};
```

A lobe is what the light is weighed by at a surface: COSINE, `max(0, axis . w)`, gives
irradiance; PHONG raises it to `exponent`. Two ways to sample light times lobe:

**Per-normal weights.** `alwan_env_weight_equirect_{T}` turns a luminance map into weights
for `alwan_importance_sampling_2d_prepare_{T}`: each pixel's luminance x `sin(theta)` at
its centre (the solid angle it covers), times, with a lobe, the lobe's mean over the pixel
floored at 1/64 of an upper bound of the lobe there. The floor keeps every pixel the lobe
reaches positive, so estimates stay unbiased. One prepare per normal, O(pixels): the
product at pixel resolution, the lowest variance and the highest cost per normal. With
`lobe` NULL it gives the plain light weights.

```c
alwan_status alwan_env_weight_equirect_{T}(alwan_scalar_{T} *out, size_t out_row_stride,
                                           alwan_scalar_{T} const *luminance, size_t row_stride,
                                           size_t width, size_t height, alwan_env_lobe_{T} const *lobe);
```

**The product sampler.** Prepared once per map, it samples light x lobe for any lobe in
O(log pixels) a sample, after Clarberg, Jarosz, Akenine-Moller and Jensen (2005, "Wavelet
importance sampling") and Clarberg and Akenine-Moller (2008, "Practical product importance
sampling for direct illumination").

```c
alwan_status alwan_env_product_sampler_prepare_{T}(alwan_env_product_sampler_{T} **out,
                                                   alwan_scalar_{T} const *luminance, size_t row_stride,
                                                   size_t width, size_t height, alwan_ctx *ctx);
void alwan_env_product_sampler_destroy_{T}(alwan_env_product_sampler_{T} *sampler, alwan_ctx *ctx);
alwan_status alwan_env_product_sampler_sample_{T}(alwan_vec3_{T} *dir_out, alwan_vec2_{T} *xy_out,
                                                  alwan_scalar_{T} *pdf_out, alwan_vec2_{T} const *u,
                                                  alwan_env_lobe_{T} const *lobe,
                                                  alwan_env_product_sampler_{T} const *sampler);
alwan_status alwan_env_product_sampler_pdf_{T}(alwan_scalar_{T} *pdf_out, alwan_vec3_{T} const *dir,
                                               alwan_env_lobe_{T} const *lobe,
                                               alwan_env_product_sampler_{T} const *sampler);
alwan_status alwan_env_product_sampler_get_layout_{T}(alwan_env_product_sampler_layout_{T} *out,
                                                      alwan_env_product_sampler_{T} const *sampler);
```

- `prepare` sums luminance x `sin(theta)` into a pyramid, the map padded with zeros to
  powers of two, every level the sums of the one below.
- `sample` walks down from the one top cell. At each level the current cell's children are
  weighed by their sum times the lobe's mean over the child's cell (a 3 x 3 midpoint rule
  by solid angle), floored at 1/64 of an upper bound of the lobe there; `u0` picks the
  column half and `u1` the row half, each rescaled for the next level. At the pixel the
  point is uniform inside it. The pdf returned (solid angle) is the product of the
  probabilities of the choices made, so it is exact: the lobe's weights shape where samples
  go, they never enter the density.
- The bound: a cell lies inside the cone around its centre of half-angle
  `(theta_b - theta_a) / 2 + max sin(theta) over the cell x (phi_b - phi_a) / 2` (down the
  meridian, then along the parallel, which is longer than the great circle), so the lobe's
  largest value on it is at `max(0, angle to the axis - half-angle)`. It is positive
  wherever the lobe is positive somewhere in the cell, so every direction where light x
  lobe is positive can be reached, and `sum f / pdf` is unbiased.
- An empty sample, `pdf` 0, happens when a coarse cell's bound saw light that no finer cell
  both holds and faces: a lost sample, not a bias. 0.42% of the samples in suite 282.
- The coarse levels weigh a child as if its light were spread evenly over it. Where one
  cell holds a bright source the lobe does not see and a dim region it does (a sun above a
  surface facing down), samples go to that cell more often than its visible light earns;
  the estimate stays unbiased and the variance rises (5.5% at 1024 points on suite 282's
  sunny sky facing down, against 0.4% to 1.5% elsewhere).
- With a stratified or low-discrepancy `u`, plain sampling's single monotone warp keeps the
  stratification better than these per-level choices, and on smooth maps the two come out
  close (a few 0.1% at 1024 points); the gains below are for random `u`.
- `pdf` recomputes the same choices down to the pixel holding `dir`, for multiple
  importance sampling; it equals the sample's own pdf to 1e-12.
- At most 32768 x 32768 pixels.

Measured on suite 282's maps, relative RMS error at 1024 random points (32 repeats), every
estimator's mean within 2.1 standard errors of the truth:

| map, lobe | product sampler | light only | per-normal weights |
|---|---|---|---|
| constant 0.5, cosine up | 0.52% | 4.2% | 0.16% |
| overcast, cosine horizontal | 0.42% | 4.8% | 0.14% |
| overcast, cosine down | 1.2% | 9.7% | 0.18% |
| overcast, Phong 20 | 1.5% | 8.6% | 0.24% |
| overcast + sun, cosine up | 0.41% | 0.74% | 0.07% |

The pyramid is read through `ALWAN_ENVP_READ(i)` in `core/alwan_env_sampling_reader.inc`,
so a shader samples it the same way (the core compiles with dxc and fxc):

```hlsl
StructuredBuffer<float> Envp : register(t5);
#define ALWAN_ENVP_NAME     sky
#define ALWAN_ENVP_READ(i)  Envp[i]
#include "core/alwan_env_sampling_reader.inc"

alwan_vec3 s = alwan_envp_sample_sky(w, h, log2_w, log2_h, 0 /* cosine */, nx, ny, nz, 1.0, u0, u1); /* x, y, pdf */
```

Validation (suite 282): the samplers against ground truth on a constant map (cosine
integral pi L, Phong 2 pi L / (e + 1)) and two synthetic skies whose integrals the suite
computes pixel by pixel; the map position and direction round trip to 3e-15; the f32
sampler to 7.6e-6.
