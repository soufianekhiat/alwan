# Image tools

Whole-image operations for photographs and frames: linear filters, edge-aware smoothing, denoising, local
contrast, colour transfer and haze removal. Each family is one function with a method
enum and a parameter struct, so a new method joins its family without a new entry point.
The parameter structs follow one rule: a zero field reads as that method's default, and a
NULL pointer is every default. Every method is held to a reference implementation by a
test suite, named with the method below.

## Colour checker detection

```c
alwan_status alwan_color_checker_detect_{T}(alwan_checker_detection *out, size_t capacity, size_t *count,
                                            alwan_{T} const *image, size_t row_stride,
                                            size_t width, size_t height, size_t channels,
                                            alwan_checker_detect_method method,
                                            alwan_checker_detect_params const *params);
```

Finds ColorChecker charts in a photograph and reads their swatches: colour-checker-detection
0.2.3's segmentation method (`detect_colour_checkers_segmentation`), a port of the Colour
Developers' pipeline (BSD-3-Clause) with the OpenCV calls it makes reproduced from OpenCV
5.x's own sources. Two methods, `ALWAN_CHECKER_DETECT_SEGMENTATION` and
`ALWAN_CHECKER_DETECT_TEMPLATED` (below); the package's inference method needs network
weights alwan does not ship.

The image is read as float32 RGB (the first three of `channels`), turned a quarter clockwise
if it is taller than wide, and resized to `working_width` by OpenCV's cubic resize. The
segmentation works on the sRGB encoding's largest channel, stretched to 8 bits: bilateral
filtering, a mean adaptive threshold, a 3 x 3 opening, every contour (Suzuki and Abe's
tracing, as `findContours`), each brought to four points by bisecting `approxPolyDP`'s
tolerance. The square ones of a plausible size become minimum-area boxes; nested boxes keep
the smallest; the boxes, grown by `swatch_contour_scale` about their centroids and drawn
filled, join into regions, and a region whose box has the chart's aspect ratio and holds a
plausible number of swatch centres is a chart. Each chart is warped (bicubic) onto a
`working_width` x `working_width / aspect_ratio` rectangle, a `samples` x `samples` window at
every swatch centre averaged, and of its four corner orders the one whose swatches are
nearest the reference values (mean squared) is kept, so the first swatch is dark skin
whichever way the chart was photographed.

| `alwan_checker_detect_params` | Default | Meaning |
|---|---|---|
| `working_width` | 1440 | the width detection runs at |
| `swatches_horizontal`, `swatches_vertical` | 6, 4 | the chart's layout; at most 140 swatches |
| `aspect_ratio` | 6 / 4 | the chart's aspect ratio |
| `aspect_ratio_minimum`, `_maximum` | 0.9, 1.1 of it | the range a candidate must fall in |
| `swatches_count_minimum`, `_maximum` | 12, 36 | half to one and a half times the swatches |
| `swatch_minimum_area_factor` | 200 | a swatch covers at least 1/(swatches x factor) of the image |
| `swatch_contour_scale` | 4/3 | how much each swatch box grows before they are joined |
| `samples` | 32 | the side of each sampling window, in working pixels |
| `bilateral_iterations`, `_sigma_color`, `_sigma_space` | 5, 5, 5 | the denoising |
| `threshold_block_size`, `threshold_constant` | 21, 3 | the adaptive threshold (block about 1.5% of the width, odd) |
| `skip_srgb_encoding` | 0 | segment the image as given rather than its sRGB encoding |
| `reference_values` | NULL | 3 x swatches linear RGB values for the orientation; NULL is the Classic after November 2014 in linear sRGB, and required for any other layout |

Each `alwan_checker_detection` holds `quad`, the four corners in the working image as the
package reports them, `quad_image`, the same corners in the input's pixels, the swatch
colours row by row from the first corner, the swatch count and the orientation's mean squared
distance. `count` is how many charts were found; with more than `capacity`, the first
`capacity` are written and the call returns `ALWAN_E_RANGE`. Colours are sampled from the
image as given, so a linear image gives linear swatches.

Suite 269 holds it to the package on synthetic scenes rendered to the bit on both sides (one
and two charts, portrait, upside down, turned 30 degrees, a wide 16:9 frame): the same charts,
the same corners and the same swatch colours, exactly. That is against OpenCV's own code: the
package's pip build routes the float resize and the 8-bit bilateral filter through Intel's
IPP, which differs from OpenCV's code by up to 5e-5 in the resized image and a grey level on
a few pixels of the filtered one. On the 164 photographs of the SRIC collection (12 charts in
11 of them) alwan and the package find the same charts at the same corners, with the swatch
colours identical when IPP is off and within 2.4e-6 when it is on, as the package ships. The det build,
with its polynomial pow and exp and its unfused multiply-adds, finds the same corners and
swatches within 1.2e-7.

### The templated method

`ALWAN_CHECKER_DETECT_TEMPLATED` is the package's `detect_colour_checkers_templated`
(`segmenter_templated`, `extractor_templated`). It shares the resize, the encoding, the
filtering, the threshold, the contours and the clustering, and differs in what it keeps and
how it reads a chart:

- every contour is cut by `approxPolyDP` at `contour_approximation_factor` (0.1) of its
  perimeter, and the four-point ones in the area range are kept;
- their squareness (`matchShapes` I2 against the unit square), area and bounding-box aspect,
  each standardised over the candidates, go through DBSCAN (`dbscan_eps` 0.5,
  `dbscan_min_samples` 5); the points DBSCAN calls noise are dropped, all are kept when it
  calls them all noise;
- the clusters are the segmentation method's, without the aspect test; the swatch centroids
  strictly inside a cluster, 8 to 24 of them, are matched against the package's ColorChecker
  Classic template (810 x 560 pixels, 24 swatch centroids and colours, 29,424 corner
  correspondences, shipped in `data/colorchecker/`): for each correspondence the perspective
  map from the four outermost centroids (one per quadrant about their mean) to the
  template's, every centroid carried through it, and the mean distance of the best
  one-to-one assignment to the template's swatches (SciPy's `linear_sum_assignment`, ported
  with its notice so ties fall the same way). The search stops at the first cost under
  `transformation_cost_threshold` (10 template pixels);
- the image is warped onto the template by the cheapest map (OpenCV 5's float bilinear
  `warpPerspective`, its AVX2 block and scalar tail both reproduced), a `samples` x `samples`
  window averaged at each template swatch, and the order reversed when the first 18 swatches
  vary less across R, G and B than the last 6 (the chart read as upside down).

It returns one detection at most, and none when no cluster holds 8 to 24 centroids (the
package raises there). `quad` is the package's quadrilateral, the cheapest cluster's box,
with one quirk reproduced: the package indexes the clusters by the cheapest one's position
among those kept, so when a cluster before it was dropped for its centroid count, `quad` is
another cluster's box. `mse` is the mean squared distance to the template's colours.
`reference_values` is not read and the layout must be 6 x 4.

| `alwan_checker_detect_params` (templated) | Default | Meaning |
|---|---|---|
| `contour_approximation_factor` | 0.1 | `approxPolyDP`'s tolerance, as a fraction of the perimeter |
| `dbscan_eps`, `dbscan_min_samples` | 0.5, 5 | the outlier filter on the standardised features |
| `transformation_cost_threshold` | 10 | stop at the first map whose mean distance is under this |

Suite 269 holds it to the package on the same synthetic scenes: four charts found, the same
quadrilaterals and swatch colours to the bit, and no chart in the two scenes where the package
finds none; the det build is within 1.2e-7 on the swatches. On the 166 SRIC photographs at
the plate's exposure (IPP off) both find the same 14 charts, identical, and neither finds one
in the other 152. The search costs 2 to 5 seconds a photograph at the default width, most of
it in the 29,424 assignments a cluster can take; DBSCAN's radius is tested on squared
distances (scikit-learn's KD-tree path), which below 12 candidates can differ from
scikit-learn's brute-force path at exactly `dbscan_eps`.

## Vignetting

```c
typedef enum {
    ALWAN_VIGNETTE_PARABOLIC = 0, ALWAN_VIGNETTE_HYPERBOLIC_COSINE = 1,
    ALWAN_VIGNETTE_BIVARIATE_SPLINE = 2, ALWAN_VIGNETTE_RBF = 3
} alwan_vignette_method;

alwan_status alwan_vignette_characterise_{T}(alwan_vignette **out, alwan_{T} const *flat, size_t row_stride,
                                             size_t channels, size_t width, size_t height,
                                             alwan_vignette_method method, alwan_vignette_params const *params,
                                             alwan_ctx *ctx);
alwan_status alwan_vignette_correct_{T}(alwan_{T} *out, size_t out_row_stride,
                                        alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                        size_t width, size_t height, alwan_vignette const *vignette);
alwan_status alwan_vignette_characterise_u8(alwan_vignette **out, unsigned char const *flat, size_t row_stride,
                                            size_t channels, size_t width, size_t height,
                                            alwan_vignette_method method, alwan_vignette_params const *params,
                                            alwan_ctx *ctx);
alwan_status alwan_vignette_correct_u8(unsigned char *out, size_t out_row_stride,
                                       unsigned char const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height, alwan_vignette const *vignette);
alwan_status alwan_vignette_evaluate(double *out, size_t out_row_stride, size_t width, size_t height,
                                     size_t channel, alwan_vignette const *vignette);
alwan_status alwan_vignette_principal_point(double *xy, alwan_vignette const *vignette);
void alwan_vignette_destroy(alwan_vignette *vignette, alwan_ctx *ctx);
```

A lens passes less light towards the corners of the frame. Shoot a flat field (an evenly
lit, featureless surface: a light box, a white wall, an overcast sky through a diffuser)
through the lens, characterise it once, and divide the falloff out of every other frame
shot through that lens at that aperture. Coordinates are fractions of the frame, so one
characterisation corrects frames of any size with the same channel count. The principal
point, where the falloff centres, is the centroid of the flat field's brightest pixels
(their median over the channels above `threshold` times the largest).

| Method | The falloff | Fitted by |
|---|---|---|
| `PARABOLIC` | `(a_x2 x^2 + a_x1 x + a_x0) / 2 + (a_y2 y^2 + a_y1 y + a_y0) / 2` about the principal point | least squares within colour-hdri's bounds, exactly |
| `HYPERBOLIC_COSINE` | `1 - cosh(r_x (x - 0.5 - x_0)) cosh(r_y (y - 0.5 - y_0)) + c` | Levenberg-Marquardt within colour-hdri's bounds |
| `BIVARIATE_SPLINE` | the flat field itself, smoothed (a Gaussian of `denoise_sigma`, 6), kept at `samples` (50) points on its longer side, smoothed again (`post_denoise_sigma`, 1), read back through the interpolating bicubic spline | nothing to fit: any shape of falloff |
| `RBF` | colour-hdri's samples of the smoothed flat field (two on each diagonal, ten along each edge, a polar grid of 7 radii by 21 angles about the principal point) through the cubic radial basis function with a linear tail and `smoothing` (0.001) | one linear system |

The two fitted surfaces are bounded as colour-hdri bounds them, which expects a flat field
near 1 at its brightest; a parabola fits a lens's falloff poorly, and its constant often
rests on the bound of 0.9. The spline follows any falloff, and the radial basis function
any smooth one. A correction is the frame divided by the surface, channel by channel; 8-bit
data are read as value / 255 and written back rounded and saturated.

This is colour-hdri's `distortion.vignette`, and suite 238 holds each method to it on three
flat fields made by its own `apply_radial_gradient` (square, wide with an off-centre
channel, grey and off-centre): the spline to 2.3e-15, the radial basis function to 2.5e-14
(its jitter is colour-hdri's own draw, exported to `data/vignette/rbf_jitter.csv`), the
hyperbolic cosine to 5.7e-8 and the parabola to 1.7e-9, their optimisers' tolerance, and
every principal point equal. The spline is FITPACK's interpolation with s = 0, which is the
not-a-knot cubic along each axis; the radial basis function is scipy's `RBFInterpolator`
system, its polynomial on coordinates scaled to [-1, 1], solved by LU with partial pivoting.
One difference is deliberate: on a frame that is not square, colour-hdri's 2D functions
subtract the principal point's row fraction from the column coordinate and its column
fraction from the row. alwan pairs them the right way round, and the suite's parabola on
those frames is colour-hdri's own function, bounds and `curve_fit` with the point paired
that way.

## Light probes

```c
typedef enum {
    ALWAN_HEMISPHERE_ILLUMINANCE_EXACT = 0,
    ALWAN_HEMISPHERE_ILLUMINANCE_LAGARDE2016 = 1
} alwan_hemisphere_illuminance_method;

alwan_status alwan_upper_hemisphere_illuminance_{T}(double *illuminance, alwan_{T} const *src, size_t row_stride,
                                                    size_t channels, size_t width, size_t height,
                                                    alwan_rgb_space_desc_{T} const *space,
                                                    alwan_hemisphere_illuminance_method method);
alwan_status alwan_upper_hemisphere_illuminance_u8(double *illuminance, unsigned char const *src, size_t row_stride,
                                                   size_t channels, size_t width, size_t height,
                                                   alwan_rgb_space_desc_f64 const *space,
                                                   alwan_hemisphere_illuminance_method method);
alwan_status alwan_upper_hemisphere_illuminance_weights(double *out, size_t out_row_stride, size_t width, size_t height,
                                                        alwan_hemisphere_illuminance_method method);
alwan_status alwan_absolute_luminance_calibrate_{T}(alwan_{T} *out, size_t out_row_stride, alwan_{T} const *src,
                                                    size_t src_row_stride, size_t channels, size_t width, size_t height,
                                                    double measured_illuminance, alwan_rgb_space_desc_{T} const *space,
                                                    alwan_hemisphere_illuminance_method method);

typedef enum { ALWAN_LIGHT_PROBE_VARIANCE_MINIMIZATION = 0 } alwan_light_probe_method;
typedef struct { size_t levels; } alwan_light_probe_params;   /* 0 reads as 4: 16 lights */
typedef struct {
    size_t y0, y1, x0, x1;   /* rows [y0, y1), columns [x0, x1) */
    double cy, cx;           /* luminance centroid, whole pixels; NaN when the region has none */
    double u, v;             /* cx / width, cy / height */
    double rgb[3];           /* the region's RGB summed */
} alwan_light_probe_light;

alwan_status alwan_light_probe_sample_{T}(alwan_light_probe_light *lights, size_t capacity, size_t *count,
                                          alwan_{T} const *src, size_t row_stride, size_t channels,
                                          size_t width, size_t height, alwan_rgb_space_desc_{T} const *space,
                                          alwan_light_probe_method method, alwan_light_probe_params const *params,
                                          alwan_ctx *ctx);
alwan_status alwan_light_probe_sample_u8(alwan_light_probe_light *lights, size_t capacity, size_t *count,
                                         unsigned char const *src, size_t row_stride, size_t channels,
                                         size_t width, size_t height, alwan_rgb_space_desc_f64 const *space,
                                         alwan_light_probe_method method, alwan_light_probe_params const *params,
                                         alwan_ctx *ctx);
```

An equirectangular panorama: row 0 at the zenith, the last row at the nadir, the columns
one full turn of azimuth. Luminance is the Y row of the space's RGB to XYZ matrix applied
to the first three channels (`space` NULL is sRGB); a fourth channel is ignored, and u8
samples read as value / 255. Strides are in bytes. After colour-hdri 0.2.6 (suite 244).

**Illuminance and calibration** (Lagarde, Lachambre and Jover 2016). The illuminance E_v
on an upward horizontal surface is the integral of L cos(theta) over the upper hemisphere.

| Method | Each row weighted by | A uniform sky of 1 |
|---|---|---|
| `EXACT` | the exact integral of cos(theta) sin(theta) over the row's band of zenith angles, [i pi / H, (i + 1) pi / H] clipped at the horizon; exact for a panorama constant within each pixel | pi |
| `LAGARDE2016` | colour-hdri's `upper_hemisphere_illuminance_Lagarde2016`: the row sampled at i pi / (H - 1), poles included, weighted 2 pi^2 cos sin / (W H) | 2.934 at H = 16, 3.1385 at 1024 |

`LAGARDE2016` weights each row as though it were sampled at its centre while sampling its
edge, so it reads low by about pi / H. It is kept to reproduce colour-hdri's numbers.
`alwan_upper_hemisphere_illuminance_weights` writes each pixel's weight w, such that
E_v = sum of L w over the image divided by W H: for `LAGARDE2016`, colour-hdri's
`upper_hemisphere_illuminance_weights_Lagarde2016`, the image Lagarde gives for applying the
calibration in Photoshop. `alwan_absolute_luminance_calibrate_{T}` scales the first three
channels by `measured_illuminance / E_v` (colour-hdri's
`absolute_luminance_calibration_Lagarde2016`), so the result's illuminance is the measured
one. It copies a fourth channel, and `out` may be `src`. A panorama whose E_v is not above
0 (at H = 2, `LAGARDE2016` samples only the two poles) returns `ALWAN_E_RANGE`.

**Lights by variance minimisation** (Viriyothai and Debevec 2009), as colour-hdri's
`light_probe_sampling_variance_minimization_Viriyothai2009`. `levels` times, every region
is cut in two at the column, and then the row, that minimises the larger of the two parts'
luminance spread sqrt(sum Y ((x - cx)^2 + (y - cy)^2)), each part about its own centroid
floored to a whole pixel. Columns are tried before rows and only a strictly smaller spread
replaces the best, so the first minimum wins. Each of the 2^levels lights is a region, its
luminance centroid, that centroid as a fraction of the frame, and the sum of its RGB. The
sum is not weighted by solid angle, as in colour-hdri. A region with no luminance has a
NaN centroid (colour-hdri reports -2^63).

colour-hdri takes a light count n and runs int(sqrt(n)) levels, so only n = 4 and n = 16
give n lights: 64 gives 256. alwan takes the number of levels.

Cuts are scored from per-row and per-column moment sums, so a level costs O(W H): 16 lights
on a 1024 x 512 panorama take about 40 ms, where colour-hdri takes 0.4 s on a 128 x 64 one.
Candidates near the best are scored again pixel by pixel, and on panoramas without exact
ties the regions equal colour-hdri's. With an exact tie, such as one bright pixel on black
or a uniform field, colour-hdri's cut follows the last bit of its luminance and numpy's
summation order, and alwan's may differ.

**Returns:** `ALWAN_OK`; `ALWAN_E_INVALID` for a NULL, a zero size, a channel count other
than 3 or 4, a stride too small, a non-finite sample or an unknown method; `ALWAN_E_RANGE`
as above, for `levels` over 20, for `capacity` under 2^levels, or for a region with no
admissible cut (no pixels left, or every score NaN, which only negative luminance causes).

## Colour quantisation

```c
typedef enum {
    ALWAN_QUANTIZE_MEDIAN_CUT = 0,
    ALWAN_QUANTIZE_FAST_OCTREE = 1,
    ALWAN_QUANTIZE_MAX_COVERAGE = 2,
    ALWAN_QUANTIZE_KMEANS = 3,
    ALWAN_QUANTIZE_WU = 4,
    ALWAN_QUANTIZE_OCTREE_CLASSIC = 5
} alwan_quantize_method;

typedef struct {
    size_t max_iterations;            /* KMEANS: 0 = 300 */
    alwan_f64 tolerance;              /* KMEANS: 0 = 1e-4, relative to the mean channel variance */
    alwan_f64 const *initial_centres; /* KMEANS: initial_count x 3, NULL = the median-cut palette */
    size_t initial_count;
    alwan_f64 *centres_out;           /* KMEANS, optional: the final centres */
    size_t *iterations_out;           /* KMEANS, optional: Lloyd iterations run */
} alwan_quantize_params;

alwan_status alwan_quantize_u8(unsigned char *palette_out, size_t *count_out,
                               unsigned int *index_out,
                               unsigned char const *rgb, size_t pixel_stride,
                               size_t count, size_t max_colors, alwan_quantize_method method);
alwan_status alwan_quantize_ex_u8(unsigned char *palette_out, size_t *count_out,
                                  unsigned int *index_out,
                                  unsigned char const *rgb, size_t pixel_stride,
                                  size_t count, size_t max_colors, alwan_quantize_method method,
                                  alwan_quantize_params const *params);
```

A palette of at most `max_colors` entries for a photograph, and optionally each pixel's
entry. An unknown method is `ALWAN_E_INVALID`. `alwan_quantize_u8` is
`alwan_quantize_ex_u8` with `params` NULL, every setting at its default; only `KMEANS`
reads the params.

On three SRIC frames (plate 164) at 16 colours, RMS error in 8-bit levels and mean
CIEDE2000, and the time on a 512 x 192 frame:

| method | mean RMS | mean dE00 | time |
|---|---|---|---|
| `MEDIAN_CUT` | 16.1 | 7.77 | 40 ms |
| `FAST_OCTREE` | 15.9 | 7.64 | 0.9 ms |
| `MAX_COVERAGE` | 26.8 | 14.20 | 39 ms |
| `KMEANS` | 12.9 | 6.88 | 200 ms |
| `WU` | 14.9 | 7.13 | 0.9 ms |
| `OCTREE_CLASSIC` | 27.6 | 9.91 | 32 ms |

### `MEDIAN_CUT`

Heckbert's median cut (SIGGRAPH 1982) as Pillow's `Image.quantize(method=MEDIANCUT)`
computes it, on 8-bit RGB.
The distinct colours are split into at most `max_colors` boxes: each time the box with
the most pixels, on the channel whose range weighted 77 : 150 : 29 is widest, at the
median of its pixel count, with every pixel of one channel value kept on one side; a box
of a single colour is not split. Each palette entry is the rounded mean of its box's
pixels, in Pillow's order (the upper half of each split first), and `index_out` (optional)
maps every pixel to its nearest entry in squared distance, searched as Pillow searches.
An image with more than 65536 distinct colours first loses low bits until it has at most
that many, as Pillow does. `palette_out` holds `3 * max_colors` bytes; the result may have
fewer entries when the splits run out.

Suite 188 compares palettes, their order and every pixel's index with Pillow, exactly:
an SRIC crop at 8 to 256 colours, a posterised gradient whose boxes tie on pixel count
(where the order Pillow's heap breaks ties decides the palette, so that heap is
reproduced too), a four-colour image asked for 16, and 70000 pixels with more than 65536
colours.

### `FAST_OCTREE`

A two-level octree as Pillow's `Image.quantize(method=FASTOCTREE)` computes it
(Tonnhofer 2010). One pass counts the pixels, and sums their channels, in a fine cube of
16 x 16 x 16 cells (the top four bits of each channel) and a coarse cube of 4 x 4 x 4.
Every occupied coarse cell gets an entry, the rest of the palette goes to the most
populated fine cells, and a fine cell with an entry takes its pixels out of its coarse
cell; a coarse cell left empty frees its entry for another fine cell. The entries are the
coarse cells, most populated first, then the fine ones, each the mean of its pixels in
float, truncated. A pixel maps to its fine cell's entry when it has one and to its
coarse cell's otherwise, through a table, so no pixel searches the palette unless its
coarse cell got no entry (below). On the three SRIC frames of plate 79 at eight colours
its RMS error is lower than median cut's (28.8, 18.0 and 13.4 levels against 30.0, 20.1
and 17.4): median cut splits by pixel count and spends its entries on large dark areas.

Three differences from Pillow, each deliberate:

- Pillow sorts the cells by count with `qsort` and a comparator that calls equal counts
  equal, so the order of tied cells, and with it the palette and the index map, depends
  on the C library: a Windows build of Pillow and a glibc build give different palettes
  for the same image. alwan orders equal counts by cell index, the same on every
  platform.
- With `max_colors` below the number of occupied coarse cells (at most 64), some coarse
  cells get no entry, and Pillow maps their pixels to entry 0, the default of its lookup
  table, wherever that entry is. alwan maps them to the nearest entry in squared
  distance. On suite 207's images this more than halves the RMS error at 12 to 40
  colours (at 16 colours, 28.4 against Pillow's 67.5).
- Pillow pads the palette to `max_colors` with black entries no pixel uses; `count_out`
  counts only the entries that hold pixels.

Suite 207 holds the palette and every pixel's index to Pillow 12.0 exactly on three
images built so that no two occupied cells share a pixel count, where Pillow's result
does not depend on its C library, at 12 to 256 colours; the pixels Pillow sends to
entry 0 are checked to go to their nearest entry. On an SRIC crop, which ties, the
error is Pillow's to two decimals, 3.64 at 256 colours and 5.07 at 64.

### `MAX_COVERAGE`

Farthest-point sampling of the image's distinct colours, as Pillow's
`Image.quantize(method=MAXCOVERAGE)` computes it. The first entry is the colour farthest
from the mean pixel (rounded per channel), and each next entry the colour farthest from
its nearest entry so far, in squared RGB distance; pixels map to their nearest entry. The
entries are colours of the image, and the extremes come first: a small palette covers the
whole gamut of the image and leaves its bulk to few entries, the opposite trade from
median cut.

Squared distances are small integers and tie often, and Pillow keeps the first colour at
the largest distance in the order its hash table walks them. That order is reproduced
without a hash table: bucket `hash % L` for Pillow's pixel hash, then (r, g, b)
ascending, as its chains are kept, where `L` is the table length Pillow reaches after that
many distinct colours. Pillow grows the table with a `_findPrime` whose test reads
`!start % t`, which is `(!start) % t`, so it never finds a factor; its lengths are
reproduced as it computes them. The index map is Pillow's search, which finds the
nearest entry and breaks ties by distance from entry 0, then by index.

Two differences from Pillow: the mean is summed in 64 bits, where Pillow's 32-bit sums
wrap above 16843009 pixels, and `count_out` stops at the number of distinct colours where
Pillow repeats a colour up to `max_colors`.

Suite 208 holds palette and index map to Pillow 12.0 exactly in twelve cases: the SRIC
crop at 8 to 256 colours, the posterised gradient, four colours asked for 16, 12000
random colours and a grey ramp. With the hash order replaced by plain colour order, ten
of the twelve fail.

### `KMEANS`

Lloyd's algorithm over the pixels, as scikit-learn's `KMeans(algorithm="lloyd", n_init=1)`
runs it from the same initial centres: `params->initial_centres`, or by default the
`MEDIAN_CUT` palette (so `count_out` is the median cut's entry count). Each iteration
labels every pixel with the first centre of least `||c||^2 - 2 x.c` and sets each centre
to the sum of its pixels times `1 / count`. A centre left without pixels moves onto the
pixel farthest from the centre it was labelled with; scikit-learn takes those through
numpy's `argpartition`, whose order among several equal distances is not defined, and
alwan takes the farthest first, then the colour seen first. It stops when the labels
repeat, when the summed squared centre shift is at most `tolerance` (default 1e-4) times
the mean per-channel variance of the pixels, or after `max_iterations` (default 300), and
labels once more from the final centres unless it stopped on repeated labels. The palette
is the centres rounded; `index_out` holds the labels, the nearest centre, which is not
always the nearest rounded entry; `params->centres_out` gets the centres and
`params->iterations_out` the iteration count. The pixels are worked as their distinct
colours weighted by count, which sums the same integers.

Suite 297 holds labels and iteration counts equal to scikit-learn 1.9 in twelve cases
(two SRIC crops, a poster and a noisy image at 4, 16 and 64 colours) and the centres to
2.1e-12. scikit-learn first subtracts the mean pixel, which alwan does not. On integer
pixels from integer centres (a median-cut palette) many pixels lie exactly half way
between two centres; alwan sends them to the first, scikit-learn to whichever its BLAS
rounding of the shifted dot products favours, which is a property of the BLAS build. The
suite therefore starts both from the median-cut palette moved off the integer grid by a
fixed fraction.

### `WU`

Wu 1991, "Efficient statistical computations for optimal color quantization" (Graphics
Gems II). The pixels are counted in a 33 x 33 x 33 cube over the top five bits of each
channel with their first and second moments, made cumulative so that any box's sums take
eight reads. Starting from the whole cube, the box of largest variance is split next,
along the axis and at the plane where the summed squared means of the two halves, each
weighted by its count, are largest; a box of one cell, or one no plane splits, is left
alone. Each entry is its box's mean, truncated, and a pixel maps to its cell's box, as the
paper's listing does. The listing keeps the second moment and the variances in single
precision; alwan keeps them in double, which can choose a different plane where two are
within float rounding. At most 65535 entries. Suite 297 holds palette and index map to a
transcription of the listing (in double) in sixteen cases, and checks every entry is the
truncated mean of the pixels mapped to it.

### `OCTREE_CLASSIC`

Gervautz and Purgathofer 1988: an octree of depth 8 over the distinct colours, each level
one bit of red, green and blue, then folded from the deepest level: the node with the
fewest pixels first, at equal counts the one earlier in the tree, its children merged into
it, until at most `max_colors` leaves remain. Folding a node of several children can
leave fewer than `max_colors` (15 for 16 on plate 164's garden). Entries are each leaf's
mean rounded half up, in tree order, and a pixel maps to its leaf. The paper folds while it
inserts pixels, which makes its palette depend on the pixel order; folding the full tree
does not. Folding by fewest pixels keeps a rare colour on its own branch as an entry while
the bulk of the image shares few, which is why its error on photographs is high (plate
164). Suite 297 holds palette and index map to a transcription in sixteen cases and checks
every entry is the rounded mean of its pixels.
