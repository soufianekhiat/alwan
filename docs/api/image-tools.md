# Image tools

Whole-image operations for photographs and frames: linear filters, edge-aware smoothing, denoising, local
contrast, colour transfer and haze removal. Each family is one function with a method
enum and a parameter struct, so a new method joins its family without a new entry point.
The parameter structs follow one rule: a zero field reads as that method's default, and a
NULL pointer is every default. Every method is held to a reference implementation by a
test suite, named with the method below.

## Edge-aware smoothing

```c
typedef enum {
    ALWAN_EDGE_FILTER_GUIDED = 0,
    ALWAN_EDGE_FILTER_JOINT_BILATERAL = 1,
    ALWAN_EDGE_FILTER_ROLLING_GUIDANCE = 2,
    ALWAN_EDGE_FILTER_DOMAIN_TRANSFORM_NC = 3,
    ALWAN_EDGE_FILTER_DOMAIN_TRANSFORM_RF = 4,
    ALWAN_EDGE_FILTER_FAST_GLOBAL_SMOOTHER = 5,
    ALWAN_EDGE_FILTER_L0_SMOOTH = 6,
    ALWAN_EDGE_FILTER_ADAPTIVE_MANIFOLD = 7,
    ALWAN_EDGE_FILTER_WEIGHTED_MEDIAN = 8,
    ALWAN_EDGE_FILTER_BILATERAL_TEXTURE = 9
} alwan_edge_filter_method;

typedef enum {
    ALWAN_WMF_EXP = 0, ALWAN_WMF_IV1 = 1, ALWAN_WMF_IV2 = 2,
    ALWAN_WMF_COS = 3, ALWAN_WMF_JAC = 4, ALWAN_WMF_OFF = 5
} alwan_wmf_weight;

alwan_status alwan_edge_filter_{T}(alwan_{T} *out, size_t out_row_stride,
                                   alwan_{T} const *src, size_t src_row_stride, size_t src_channels,
                                   alwan_{T} const *guide, size_t guide_row_stride, size_t guide_channels,
                                   size_t width, size_t height,
                                   alwan_edge_filter_method method,
                                   alwan_edge_filter_params_{T} const *params);
```

Each method smooths `src` while keeping the edges of a guide image: the base layer of a
base-and-detail edit, a matte or mask snapped to a picture's edges, or a no-flash photograph
denoised along a flash photograph's edges. `guide` NULL uses `src` as its own guide.
`src` has 1 to 4 channels and the guide 1 to 4 (1 or 3 for `GUIDED`, 1 for
`WEIGHTED_MEDIAN`; `BILATERAL_TEXTURE` takes 1 or 3 source channels and no guide); `out` has
`src`'s layout and may be `src`.

| Field of `alwan_edge_filter_params_{T}` | Methods | 0 reads as |
|---|---|---|
| `radius` | `GUIDED`, `JOINT_BILATERAL`, `ROLLING_GUIDANCE` / `WEIGHTED_MEDIAN` / `BILATERAL_TEXTURE` | 4 for `GUIDED`, `round(1.5 sigma_space)` for the next two / 5 / 3 |
| `eps` | `GUIDED` | 0.01 |
| `sigma_color` | `JOINT_BILATERAL`, `ROLLING_GUIDANCE` / `DOMAIN_TRANSFORM_*` / `FAST_GLOBAL_SMOOTHER` / `ADAPTIVE_MANIFOLD` / `WEIGHTED_MEDIAN` | 0.1 / 0.4 / 0.03 / 0.2 / 0.1 |
| `sigma_space` | `JOINT_BILATERAL`, `ROLLING_GUIDANCE` / `DOMAIN_TRANSFORM_*` / `ADAPTIVE_MANIFOLD` | 3 / 60 / 16 |
| `lambda` | `FAST_GLOBAL_SMOOTHER` / `L0_SMOOTH` | 900 / 0.02 |
| `lambda_attenuation` | `FAST_GLOBAL_SMOOTHER` | 0.25 |
| `iterations` | `ROLLING_GUIDANCE` / `DOMAIN_TRANSFORM_*`, `FAST_GLOBAL_SMOOTHER` / `BILATERAL_TEXTURE` | 4 / 3 / 1 |
| `start_from_source` | `ROLLING_GUIDANCE` | 0: start from the source's Gaussian (the paper) |
| `kappa` | `L0_SMOOTH` | 2 (above 1) |
| `adjust_outliers` | `ADAPTIVE_MANIFOLD` | 0: off |
| `weight_type` | `WEIGHTED_MEDIAN` | `ALWAN_WMF_EXP` |
| `sigma_alpha` | `BILATERAL_TEXTURE` | `5 radius`, OpenCV's |
| `sigma_avg` | `BILATERAL_TEXTURE` | `0.05 sqrt(channels)`, OpenCV's |

The defaults are each paper's settings for values in 0..1.

### `GUIDED`

He, Sun and Tang (ECCV 2010, TPAMI 2013): in every window of side `2 radius + 1` the
output is a linear function of the guide, `q = a . I + b`, with `a` and `b` fitted to `src`
by least squares with ridge `eps` and then averaged over the windows covering each pixel.
The image as its own guide smooths within regions and keeps edges; a colour guide with a
one-channel source (a matte, a haze transmission map, a mask from `alwan_select_*`) snaps
the source to the guide's edges. The cost does not depend on the radius. `eps` is in the
square of the guide's units: for a guide in 0..1, 1e-3 keeps fine edges and 0.1 smooths
strongly.

It follows OpenCV's `ximgproc::guidedFilter`: box means with the border reflected (the
edge pixel repeated), the colour guide's 3 x 3 covariance inverted by cofactors, and, for
`eps` below 0.01, a determinant under 1e-6 replaced by 1. It computes in double. Suite
190: the colour-guide cases agree with OpenCV to 1.8e-7, float rounding; the grey-guide
case to 6.1e-6, because OpenCV's SSE build takes the reciprocal of the variance with a
12-bit approximation.

### `JOINT_BILATERAL`

The bilateral filter (Tomasi and Manduchi 1998) with its range weight taken from the guide
(the joint image of Petschnigg et al. and Eisemann and Durand, 2004): each output is a mean
of `src` over a disc of `radius`, weighted by distance (`sigma_space`) and by how close the
guide's value there is to its value at the centre (`sigma_color`). With `src` as its own
guide it is the ordinary bilateral filter. The colour distance is the L1 sum of channel
differences, the window a disc and the border reflected without repeating the edge pixel,
as in OpenCV's `ximgproc::jointBilateralFilter`; the colour Gaussian is evaluated exactly
where OpenCV reads a 4096-bin table, and a flat guide is not replaced by a square Gaussian
blur as OpenCV does. Cost grows with the square of the radius. Suite 194 agrees with OpenCV
to 4.8e-7.

### `ROLLING_GUIDANCE`

Zhang, Shen, Xu and Jia (ECCV 2014): the joint bilateral filter iterated with its own last
output as the guide; `guide` is not read. Starting from the source's Gaussian (the paper,
`start_from_source` 0), structures smaller than `sigma_space` are removed first and the
iterations then bring the large edges back, a scale-aware smoother that keeps the outlines
of what it keeps. OpenCV's `ximgproc::rollingGuidanceFilter` starts from the source itself
(`start_from_source` non-zero), which keeps small structures that the paper's start
removes; suite 194 holds that start to OpenCV to 7.7e-7 over four iterations and checks
the paper's first step is the constant-guide filter.

### `DOMAIN_TRANSFORM_NC`, `DOMAIN_TRANSFORM_RF`

Gastal and Oliveira (SIGGRAPH 2011) replace a 2D edge-aware filter with 1D filters along
rows and columns in a transformed coordinate: each step between neighbours is
`1 + sigma_space / sigma_color * |guide difference|` (L1 over the guide's channels) long,
so an edge in the guide becomes a long gap that the 1D filter barely crosses. Rows and
columns alternate `iterations` times with a shrinking spatial sigma, which removes the
stripes a single pass leaves. The cost does not depend on `sigma_space`. `NC`
(normalized convolution) averages a box in the transformed coordinate; `RF` (recursive
filtering) runs a first-order recursive filter forward and back, which leaks further
across weak edges and costs less. The interpolated-convolution mode of the paper is not
offered.

It follows OpenCV's `ximgproc::dtFilter`: the transformed coordinate is built in float as
OpenCV builds it, so NC boxes hold the same pixels, and the data is filtered in double.
Suite 195 agrees with OpenCV to 1.9e-6 in NC (OpenCV's float running sum) and 1.5e-7 in RF,
along a colour and a one-channel guide. `sigma_space` below 1 or `sigma_color` below 0.01
returns `ALWAN_E_RANGE` where OpenCV clamps silently; `iterations` runs from 1 to 30.

### `FAST_GLOBAL_SMOOTHER`

Min, Choi, Lu, Ham, Sohn and Do (IEEE TIP 2014) solve the weighted least squares smoother,
`sum (u - f)^2 + lambda sum w_pq (u_p - u_q)^2` with `w_pq = exp(-|g_p - g_q| / sigma_color)`,
as exact 1D problems along every row and then every column. Each is tridiagonal and costs
one forward and one backward sweep. Where the filters above average a window, this one
solves for the whole line at once, so a region bounded by guide edges flattens however
large it is. Larger `lambda` smooths further; `iterations` repeat the row and column passes
with `lambda` multiplied by `lambda_attenuation` each time.

`|g_p - g_q|` is Euclidean over the guide's channels and `sigma_color` is in the guide's
units. OpenCV's `ximgproc::fastGlobalSmootherFilter` takes an 8-bit guide and
`sigma_color` in 0..255 steps; divide both by 255 to use its numbers with a guide in 0..1.
Suite 196 does that and agrees with OpenCV to 7.8e-6 at `lambda` 1000 and 4.5e-7 at
`lambda` 10, the difference being OpenCV's float solve; a constant source stays constant to
1e-14.

### `L0_SMOOTH`

Xu, Lu, Xu and Jia (SIGGRAPH Asia 2011) minimise `sum (S - I)^2 + lambda #{p : grad S(p) != 0}`:
the price is the number of pixels where the result changes at all, not how much it
changes. The result is flat inside regions and steps between them at the edges strong
enough to pay for themselves, which is the look of structure extraction, clip-art
vectorisation and edge-preserving stylisation. `lambda` sets the price, in squared data
units: 0.02 on values in 0..1 is the paper's, larger merges more.

It is solved by half-quadratic splitting. Auxiliary gradients `(h, v)` stand in for `S`'s
forward differences; each round keeps them where their squared magnitude, summed over the
channels, reaches `lambda / beta` and zeroes them elsewhere, then solves for `S` given
them. `beta` starts at `2 lambda` and grows by `kappa` until it reaches 1e5, about 22 rounds
at `kappa` 2; a smaller `kappa` takes more rounds and gives sharper steps. The first
round's threshold is 0.5 whatever `lambda` is, so a step of less than `sqrt(0.5)` is
flattened there and has to be rebuilt by later rounds.

The image is treated as periodic, as the paper and its code do: differences wrap from the
last column to the first, and the `S` step is a division in the 2D DFT domain, two
transforms per channel a round. The transform is alwan's own, radix-2 for powers of two and
Bluestein's chirp-z for any other size, in double. The guide is not used. The result is not
clamped.

Suite 213 runs the authors' `L0Smoothing.m` in MATLAB (fetched into gendata's cache and
checked against its hash; it is distributed for non-commercial use, so nothing of it is in
alwan) on grey and colour images of power-of-two and other sizes, a 13 x 7 image and linear
values past 1, and agrees to 3.3e-12. OpenCV's `ximgproc::l0Smooth` solves the same periodic
system but takes the gradients with replicated and reflected borders that do not match it,
and works in float32, where a hard threshold can decide a region differently; it is not the
reference here.

### `ADAPTIVE_MANIFOLD`

Gastal and Oliveira (SIGGRAPH 2012): a high-dimensional Gaussian filter over position and
the guide's values, computed on a tree of manifolds. The first manifold is the guide blurred;
each is sampled at `w_k = exp(-|eta - guide|^2 / sigma_r^2)`, the source weighted by `w_k`
is splatted onto it at a lower resolution, blurred there by the recursive domain transform
and sliced back, and the pixels are split between two child manifolds by the sign of their
projection on the first principal direction of `guide - eta`. `sigma_space` is `sigma_s`
(at least 1, in pixels), `sigma_color` is `sigma_r` (in (0, 1], in guide units), and
`adjust_outliers` blends each result back towards the source by its distance to the nearest
manifold. The tree's height is OpenCV's `max(2, ceil((floor(log2 sigma_s) - 1)(1 - sigma_r)))`
and the resolution drops by `2^floor(log2 min(sigma_s / 4, 256 sigma_r))`.

This is a port of OpenCV's `ximgproc::amFilter` (opencv_contrib 5.0.0, BSD-3-Clause, the
notice in `api/alwan_am_filter.c`), kept to its float arithmetic: `cv::resize` bilinear and
its fast 2x2 area average, `cv::exp` (the C runtime's `expf` in OpenCV's MSVC build), the
recursive domain transform, and `cv::RNG`'s generator seeded from the guide's centre value for
the first principal direction's random start. Suite 278 agrees with `cv2.ximgproc.amFilter`
bit for bit on 24 cases (three channel layouts, resize ratios 1, 2, 4 and 8, `adjust_outliers`
off and on); the det build, whose `expf` is a polynomial, to 3.4e-7. The reference runs
OpenCV on one thread: with three or more its result changes from call to call by about 1e-7,
a race in its parallel code. It computes in float; the `_f64` form rounds its input to float
and widens the result.

### `WEIGHTED_MEDIAN`

Zhang, Xu and Jia (CVPR 2014): each output is the weighted median of the source over a
`(2 radius + 1)`-square window, each pixel weighted by how alike its guide value is to the
centre's, which keeps edges and removes speckle, and on a disparity or flow field fills its
holes along the picture's edges. `weight_type` is the likeness: `EXP` `exp(-d^2 / (2 s^2))`,
`IV1` `1 / (d + s)`, `IV2` `1 / (d^2 + s^2)`, `COS` and `OFF` 1 (the plain median), `JAC`
`min / max` of the two values, `d` the difference of two 8-bit guide values and
`s = 255 sigma_color`.

This is a port of OpenCV's `ximgproc::weightedMedianFilter` (opencv_contrib 5.0.0,
BSD-3-Clause, the notice in `api/alwan_wmf.c`): each source channel is quantised to at most
256 levels by OpenCV's adaptive rule (a binary search on the error bound over the sorted
values, each level the median of its run) and filtered with the joint histogram and the
balance counting box in OpenCV's order, so the float sums are OpenCV's. The guide is one
channel read as 8 bits, `clamp(g, 0, 1) x 255` rounded half up (OpenCV takes an 8-bit
joint). OpenCV's three-channel joint clusters the colours with a randomly seeded k-means and
is not ported: a guide of other than one channel is `ALWAN_E_INVALID`. With `guide` NULL the
source is its own guide, read as 8 bits (OpenCV runs `medianBlur` instead). Suite 278 agrees
with `cv2.ximgproc.weightedMedianFilter` on every value of 48 cases (one and three source
channels, radius 1 and 3, `sigma` 25.5 and 60 of 255, every weight type), in both builds.

### `BILATERAL_TEXTURE`

Cho, Lee, Kang and Lee (SIGGRAPH 2014): fine texture (fabric, foliage, gravel) is smoothed
away and the structure it sits on kept, which a plain bilateral filter cannot do because
texture has edges too. Each pixel's guide is the box blur taken from the pixel of its window
with the least modified relative total variation, `maxG (2 fr + 1) / sumG x (maxL - minL)`
(the window's largest gradient over its summed gradients, times its range), blended back
toward its own blur where its own mRTV is close to that least one; a joint bilateral filter
over a `4 fr + 1` window then smooths the image on that guide. `radius` is the paper's `fr`;
`iterations` repeats the whole on its result.

It is a port of OpenCV's `ximgproc::bilateralTextureFilter` (opencv_contrib 5.0.0, the
Intel and Willow Garage 3-clause notice in `api/alwan_bilateral_texture.c`), kept to its
float arithmetic: `cv::blur`'s double row and running column sums (fresh row sums for
windows up to 5, a running sum wider), the forward-difference gradient, the window's
maximum and minimum started from 0 and from 1 as OpenCV starts them, `expf` for every
exponential, and `accumulateProduct`'s fused multiply-adds in its AVX2 blocks of 16 for
three channels. Values are expected in 0..1 (OpenCV divides 8-bit input by 255 first); a
double call is computed in float. Suite 298 is equal to
`cv2.ximgproc.bilateralTextureFilter`, IPP off, on every value of 16 cases (one and three
channels, `fr` 1 to 4, one and two iterations, OpenCV's defaults and given sigmas); the
deterministic build, whose `expf` is its polynomial, is within 2.4e-7.

OpenCV's `ximgproc::fastBilateralSolverFilter` (Barron and Poole 2016) is not in this family:
the installed cv2 5.0.0 is built without Eigen, where that function raises "not implemented",
so there is no reference to hold a port to.

## Linear filters

```c
typedef enum {
    ALWAN_FILTER_GAUSSIAN = 0,
    ALWAN_FILTER_DOG = 1,
    ALWAN_FILTER_LOG = 2,
    ALWAN_FILTER_LAPLACE = 3,
    ALWAN_FILTER_BUTTERWORTH = 4
} alwan_filter_method;

typedef enum {
    ALWAN_FILTER_BORDER_NEAREST = 0,   /* a a a | a b c d | d d d */
    ALWAN_FILTER_BORDER_REFLECT = 1,   /* d c b a | a b c d | d c b a */
    ALWAN_FILTER_BORDER_MIRROR = 2,    /* d c b | a b c d | c b a */
    ALWAN_FILTER_BORDER_CONSTANT = 3,  /* k k k | a b c d | k k k */
    ALWAN_FILTER_BORDER_WRAP = 4       /* a b c d | a b c d | a b c d */
} alwan_filter_border;

alwan_status alwan_filter_{T}(alwan_{T} *out, size_t out_row_stride,
                              alwan_{T} const *src, size_t src_row_stride, size_t channels,
                              size_t width, size_t height,
                              alwan_filter_method method, alwan_filter_params const *params);
alwan_status alwan_filter_u8(unsigned char *out, size_t out_row_stride,
                             unsigned char const *src, size_t src_row_stride, size_t channels,
                             size_t width, size_t height,
                             alwan_filter_method method, alwan_filter_params const *params);
```

The linear filters, each channel on its own. 1 to 4 channels; `out` may alias `src`.

| Method | What it is | Reference |
|---|---|---|
| `GAUSSIAN` | a Gaussian blur | `skimage.filters.gaussian`, i.e. `scipy.ndimage.gaussian_filter` |
| `DOG` | difference of Gaussians, the `sigma` blur minus the `high_sigma` one: a band-pass | `skimage.filters.difference_of_gaussians` |
| `LOG` | Laplacian of Gaussian, the Gaussian's second derivative along each axis, summed; negative on bright blobs | `scipy.ndimage.gaussian_laplace` |
| `LAPLACE` | the discrete Laplacian, `[1, -2, 1]` along each axis, summed | `scipy.ndimage.laplace`; `skimage.filters.laplace` is its negative, with `REFLECT` |
| `BUTTERWORTH` | a Butterworth filter in the frequency domain, high-pass unless `low_pass` | `skimage.filters.butterworth` |

The spatial filters are scipy's separable passes step for step: the kernel of
`_gaussian_kernel1d` over `int(truncate sigma + 0.5)` pixels each side, normalised by
numpy's pairwise sum, correlated along the rows' axis and then the columns', each line
extended by the border mode, a symmetric kernel summed in scipy's pairs. An f32 image is
rounded to float after every pass, as scipy does for float32 data. The result is the
same to the last bit in f64 and f32, every border mode included (suite 246).
`BUTTERWORTH` builds scikit-image's mask in the data's precision and transforms in double
with alwan's own DFT where scikit-image runs a real FFT: 1.4e-15 of the image's range
from it in f64, and 3.2e-7 in f32, where scikit-image's FFT runs in single precision.

`alwan_filter_u8` runs `GAUSSIAN`, on the values read as v / 255 (scikit-image's
`img_as_float`) and rounded back to levels, and a low-pass `BUTTERWORTH`, on the raw values
as scikit-image transforms them. The other results are signed and are `ALWAN_E_INVALID`
on 8-bit data; filter a float copy.

| Field of `alwan_filter_params` | Method | 0 reads as |
|---|---|---|
| `sigma` | `GAUSSIAN`, `LOG`; `DOG`'s lower one | 1 pixel on both axes |
| `sigma_row`, `sigma_col` | the same, per axis | unused: either above 0 replaces `sigma`, and a 0 beside it leaves that axis unsmoothed |
| `high_sigma`, `high_sigma_row`, `high_sigma_col` | `DOG` | 1.6 times the lower sigma, per axis |
| `truncate` | the spatial methods | scipy's 4 standard deviations |
| `border`, `cval` | the spatial methods | `NEAREST`, scikit-image's default for `gaussian` and `difference_of_gaussians` (scipy's `gaussian_laplace` and `laplace` default to `REFLECT`); `cval` is the value past the edge for `CONSTANT` |
| `cutoff_frequency_ratio` | `BUTTERWORTH` | 0.005 of the sampling frequency, in (0, 0.5] |
| `order` | `BUTTERWORTH` | 2 |
| `low_pass` | `BUTTERWORTH` | high-pass, scikit-image's default |
| `unsquared` | `BUTTERWORTH` | the squared Butterworth scikit-image uses (gain 1/2 at the cut-off; non-zero gives 1/sqrt(2)) |
| `npad` | `BUTTERWORTH` | no padding; otherwise that many pixels of edge padding each side before the transform |

**Returns:** `ALWAN_OK`; `ALWAN_E_INVALID` for a NULL, a zero size, a channel count out of
range, a stride too small, a non-finite value, an unknown method or border, or a u8 call
with a signed result; `ALWAN_E_RANGE` for a negative sigma or truncate, a kernel radius
over 2^20 pixels, `DOG`'s high sigma under the low one on an axis, a cut-off outside
[0, 0.5], a negative order or an `npad` over 65536; `ALWAN_E_NOMEM`.

## Registration

```c
typedef enum { ALWAN_REGISTER_PHASE_CORRELATION = 0, ALWAN_REGISTER_ECC = 1 } alwan_register_method;
typedef enum {
    ALWAN_REGISTER_MOTION_AFFINE = 0, ALWAN_REGISTER_MOTION_TRANSLATION = 1,
    ALWAN_REGISTER_MOTION_EUCLIDEAN = 2, ALWAN_REGISTER_MOTION_HOMOGRAPHY = 3
} alwan_register_motion;
typedef enum { ALWAN_REGISTER_NORMALIZE_PHASE = 0, ALWAN_REGISTER_NORMALIZE_NONE = 1 } alwan_register_normalization;
typedef struct {
    size_t upsample_factor; alwan_register_normalization normalization;    /* PHASE_CORRELATION */
    alwan_register_motion motion; size_t max_iterations; double epsilon;    /* ECC */
    size_t gauss_filter_size; double const *initial_warp;
    unsigned char const *reference_mask; size_t reference_mask_row_stride;
    unsigned char const *moving_mask; size_t moving_mask_row_stride;
} alwan_register_params;
typedef struct {
    double shift[2]; double error; double phasediff;
    double warp[9]; double correlation; size_t iterations;
} alwan_register_result;

alwan_status alwan_register_{T}(alwan_register_result *out, alwan_{T} const *reference, size_t reference_row_stride,
                                alwan_{T} const *moving, size_t moving_row_stride, size_t width, size_t height,
                                alwan_register_method method, alwan_register_params const *params);
alwan_status alwan_register_u8(alwan_register_result *out, unsigned char const *reference, size_t reference_row_stride,
                               unsigned char const *moving, size_t moving_row_stride, size_t width, size_t height,
                               alwan_register_method method, alwan_register_params const *params);
```

The translation that lines the moving image up with the reference, to a fraction of a pixel:
the step before merging a bracket or stacking frames that moved. Both images are one channel;
register colour through one channel or a luminance computed first.

`ALWAN_REGISTER_PHASE_CORRELATION` is scikit-image's `phase_cross_correlation` (space `real`,
no masks, `disambiguate` off), after Guizar-Sicairos, Thurman and Fienup (2008). The cross-power
spectrum, each term divided by its modulus unless `normalization` is `NONE`, is transformed back
and its largest sample is the whole-pixel shift. With `upsample_factor` u above 1, the shift is
rounded to 1/u and the cross-correlation is evaluated again on a `ceil(1.5 u)` square of points
1/u apart about it, by a matrix-multiply DFT that costs about `1.5 u` passes over the image rather
than an FFT u times larger.

`ALWAN_REGISTER_ECC` is OpenCV 5.0.0's `findTransformECC` (Evangelidis and Psarakis 2008,
"Parametric Image Alignment Using Enhanced Correlation Coefficient Maximization"), ported from
`video/src/ecc.cpp` with its float arithmetic: both images Gaussian-blurred (`gauss_filter_size`,
0 reads as 5, 1 is none), the moving image and its gradients warped back by the current warp,
the warp updated by the Gauss-Newton step that maximises the zero-mean normalised correlation,
until `max_iterations` (0 reads as 50) or until the correlation changes by less than `epsilon`
(0 reads as 0.001, negative never stops early). `motion` picks the model; `initial_warp` (9
values) starts it; the masks, non-zero valid, are `findTransformECC`'s template and input masks.
The result holds the 3 x 3 `warp` from reference pixels to moving points (OpenCV's matrix; the
affine models end in 0 0 1), the final `correlation`, `error = 1 - correlation`, the updates
made, and `shift` as the negated translation (the phase-correlation convention). Both images
are read as float whatever the entry point, as OpenCV converts them. Suite 300 holds the
warps of TRANSLATION, AFFINE and HOMOGRAPHY to cv2 to the bit; EUCLIDEAN within a few float
ulps. `ALWAN_E_RANGE` when OpenCV would raise `StsNoConv` (a NaN coefficient or a negative
illumination term).

Dense optical flow, its warp and its colour wheel are a family of their own:
[optical-flow.md](optical-flow.md).

- `shift` is (rows, columns), the shift to apply to the moving image, each in (-n/2, n/2] and to
  1/u; 0 along an axis of length 1. It is modulo the frame: a translation of more than half the
  frame reads as its wrapped counterpart.
- `error` and `phasediff` are computed as scikit-image computes them. With phase normalisation
  `error` is near 1 whatever the match, because the normalised peak is at most 1 while the
  amplitudes are the images' energies; it is meaningful with `NONE`.

Transforms are alwan's own FFT in double for every pixel type; u8 values are read as they are.

Suite 248 runs 70 cases against scikit-image: every shift is equal, error squared within
2.1e-14. `phasediff` agrees to 4.2e-6, not closer. Phase normalisation divides every spectral
term by its modulus, so terms that are only rounding noise become unit phasors of noise, and two
FFTs make different noise. The same sensitivity decides how far rounding the input to float
moves the answer: up to 27 steps of 1/u at u = 100 on a noisy smooth field, for scikit-image as
for alwan.

**Returns:** `ALWAN_OK`; `ALWAN_E_INVALID` for a NULL pointer, a zero size, a stride shorter than
a row, or an unknown method or normalization; `ALWAN_E_RANGE` when either image has no energy or
the input is not finite; `ALWAN_E_NOMEM`.

## Denoising

```c
typedef enum {
    ALWAN_DENOISE_TV_CHAMBOLLE = 0,
    ALWAN_DENOISE_NL_MEANS = 1,
    ALWAN_DENOISE_ANISOTROPIC_DIFFUSION = 2,
    ALWAN_DENOISE_DCT = 3,
    ALWAN_DENOISE_WAVELET = 4,
    ALWAN_DENOISE_MEDIAN = 5,
    ALWAN_DENOISE_TV_BREGMAN = 6,
    ALWAN_DENOISE_NL_MEANS_DARBON = 7,
    ALWAN_DENOISE_NL_MEANS_BUADES = 8,
    ALWAN_DENOISE_DCT_OPENCV = 9,
    ALWAN_DENOISE_BILATERAL_OPENCV = 10,
    ALWAN_DENOISE_BILATERAL_SKIMAGE = 11,
    ALWAN_DENOISE_WIENER_LOCAL = 12
} alwan_denoise_method;

alwan_status alwan_denoise_{T}(alwan_{T} *out, size_t out_row_stride,
                               alwan_{T} const *src, size_t src_row_stride, size_t channels,
                               size_t width, size_t height,
                               alwan_denoise_method method, alwan_denoise_params const *params);
alwan_status alwan_denoise_u8(unsigned char *out, size_t out_row_stride,
                              unsigned char const *src, size_t src_row_stride, size_t channels,
                              size_t width, size_t height,
                              alwan_denoise_method method, alwan_denoise_params const *params);
```

Classic denoisers that fail in different ways (plate 89 of the v3 plates shows them on
one portrait). `alwan_denoise_u8` runs all thirteen; `alwan_denoise_{T}` runs every method
but `NL_MEANS` and `ANISOTROPIC_DIFFUSION`, and returns `ALWAN_E_INVALID` for those two,
whose references work on 8-bit data. 1 to 4 channels (1 or 3 for `BILATERAL_OPENCV`);
`out` may alias `src` except for `NL_MEANS`.

| Field of `alwan_denoise_params` | Method | 0 reads as |
|---|---|---|
| `weight` | `TV_CHAMBOLLE` | 0.1 |
| `tolerance` | `TV_CHAMBOLLE` | 2e-4 (a tiny value runs every iteration) |
| `iterations` | `TV_CHAMBOLLE` / `ANISOTROPIC_DIFFUSION` | 200 (at most) / 10 |
| `h` | `NL_MEANS` / `NL_MEANS_DARBON`, `_BUADES` | 10, in 0..255 units / 0.1 in the data's units (25.5 on 8-bit) |
| `template_window`, `search_window` | `NL_MEANS` / `NL_MEANS_DARBON`, `_BUADES` | 7, 21 / 7, 23 |
| `alpha`, `k` | `ANISOTROPIC_DIFFUSION` | 0.15, 0.05 |
| `sigma` | `DCT`, `DCT_OPENCV` / `WAVELET` / `NL_MEANS_DARBON`, `_BUADES` | 10 for 8-bit data, 10 / 255 for floats / estimated from the image / 0 |
| `block_size` | `DCT`, `DCT_OPENCV` | 16 |
| `wavelet` | `WAVELET` | `ALWAN_WAVELET_DB1` (Haar) |
| `wavelet_levels` | `WAVELET` | the most the image holds, less 3, at least 1 |
| `wavelet_visushrink`, `wavelet_hard` | `WAVELET` | BayesShrink, soft |
| `kernel_size` | `MEDIAN` | 3 (odd, 3 to 255) |
| `weight`, `tolerance`, `iterations` | `TV_BREGMAN` | 5 (fidelity: larger keeps more), 1e-3, 100 |
| `anisotropic` | `TV_BREGMAN` | 0: the isotropic `|grad u|` |
| `sigma_color` | `BILATERAL_OPENCV` / `BILATERAL_SKIMAGE` | 0.1 in the data's units (25.5 on 8-bit) / the image's standard deviation |
| `sigma_space` | `BILATERAL_OPENCV` / `BILATERAL_SKIMAGE` | 3 / 1, pixels |
| `bilateral_diameter` | `BILATERAL_OPENCV` / `BILATERAL_SKIMAGE` | a radius of `round(1.5 sigma_space)` / `max(5, 2 ceil(3 sigma_space) + 1)` |
| `bilateral_bins` | `BILATERAL_SKIMAGE` | 10000 |
| `border`, `cval` | `BILATERAL_SKIMAGE` | `ALWAN_FILTER_BORDER_NEAREST`; `cval` for `CONSTANT` |
| `wiener_size` | `WIENER_LOCAL` | 3 (odd, at most 255) |
| `wiener_noise` | `WIENER_LOCAL` | the mean local variance |

### `TV_CHAMBOLLE`

The Rudin, Osher and Fatemi model: each channel becomes the image `u` minimising
`sum (u - f)^2 / 2 + weight sum |grad u|`, solved by Chambolle's dual iterations (J. Math.
Imaging and Vision, 2004). Total variation charges for every change between neighbours,
not for its size, so noise and fine texture go while edges stay sharp; flat regions go
flat, which reads as a painted look at high `weight`. On values in 0..1, `weight` 0.05 to
0.2 covers light to strong denoising. The iterations stop when the energy changes by less
than `tolerance` times its first value, or after `iterations`. It follows scikit-image's
`denoise_tv_chambolle` with `channel_axis` set, down to which iteration's image it
returns; suite 200 agrees exactly in f64, including runs cut after 1, 2 and 7 iterations.
On 8-bit data it runs in double on values divided by 255 and rounds back.

### `NL_MEANS`

Buades, Coll and Morel (CVPR 2005). Each pixel becomes the weighted mean of the pixels in a
`search_window` square around it, weighted by `exp(-d / (h^2 channels))`, where `d` is the
mean squared difference between the `template_window` patches around the two pixels. A
pixel draws on others with the same neighbourhood wherever they are in the window, so
repeated structure survives and noise averages away. `h` about 10 keeps fine detail,
higher values remove more. It reproduces OpenCV's `cv::fastNlMeansDenoising`
(`NORM_L2`) bit for bit, fixed-point weight table included; suite 201 holds every value of
ten cases equal. For a colour photograph, OpenCV's `fastNlMeansDenoisingColored` denoises
CIELAB with a separate `h` for a and b: convert first and denoise the channels you choose.

### `ANISOTROPIC_DIFFUSION`

Perona and Malik (IEEE PAMI 1990). Each iteration moves every pixel toward its eight
neighbours by `alpha sum g(d) (I_n - I)`, with `g(d) = exp(-(d / (k channels 255))^2)` and
`d` the L1 difference over the channels. Differences well below `k` diffuse and edges well
above it stay. For three channels it reproduces OpenCV's
`ximgproc::anisotropicDiffusion` bit for bit for one iteration, and suite 202 holds `n`
iterations to `n` chained one-iteration OpenCV calls. OpenCV's own loop is correct only for
one iteration: it refreshes the border with `copyMakeBorder` from a view into its padded
buffer, which without `BORDER_ISOLATED` grows the view instead of replicating, so later
iterations read a border that is stale or was never written. alwan replicates the border
every iteration.

### `DCT`

Yu and Sapiro (IPOL 2011). Every `block_size` square of the image, at every position, goes
through an orthonormal 2D DCT-II; coefficients of magnitude at most `3 sigma` are zeroed,
the inverse DCT gives the block back, and each pixel is the mean of every block estimate
covering it. Three channels are first turned into an orthonormal opponent space
(`(1, 1, 1) / sqrt3`, `(1, 0, -1) / sqrt2`, `(1, -2, 1) / sqrt6`) and back after, so the
noise of each is thresholded apart from the colour. `sigma` is the noise's standard
deviation in the data's own units.

It follows OpenCV's `xphoto::dctDenoising` except at the border: OpenCV places its blocks
at `x < width - block_size` only, so its last row and column are covered by none and come
out 0 for 8-bit data and NaN for floats. alwan places them up to `x = width - block_size`,
as the paper's own code does, so every pixel is covered. Suite 204 compares the pixels more
than one block from the right and bottom edges, where the two agree to 5e-7 in float and
exactly in 8-bit but for one value in one case, a coefficient within rounding of the
threshold; a constant image is unchanged at the border too.

### `DCT_OPENCV`

OpenCV's `xphoto::dctDenoising` itself, ported from opencv_contrib 5.0.0's
`xphoto/src/dct_image_denoising.cpp` (Intel's BSD-style licence for OpenCV, its notice kept
in `api/alwan_dct_denoise_opencv.c`) so that the result is OpenCV's to the bit: the same
algorithm as `DCT`, in OpenCV's single precision and with its border. Its steps:

- the image converted to float; three channels (in OpenCV's B, G, R order) turned by
  `cv::transform` with the float opponent matrix, whose AVX2 build takes two pixels at a
  time as `fma(v0, m0, fma(v1, m1, v2 m2))` and the last pixel or two unfused;
- every `block_size` square at `x < width - block_size`, `y < height - block_size` through
  `cv::dct`, coefficients with `|c| > 3 sigma` kept and the rest multiplied by 0,
  `cv::idct`; the estimates summed in float in block order and divided by the float count;
- three channels turned back by the float inverse of the matrix (`Matx33f::inv`), the
  result converted to the output type, 8 bits rounding half to even.

`cv::dct` is ported in `api/alwan_cv_dxt.c` from OpenCV's `core/src/dxt.cpp`: the DCT
through a real DFT of the even-odd reordered row (`RealDFT`, `CCSIDFT`), the mixed-radix
DFT beneath it (the SSE3 radix-4 pass in scalar code, the radix-3, -5 and odd-factor
passes, and the inverse permutation table the inverse DCT asks for when a length has more
than one kind of factor), rows then columns. `alwan_gradient_edit`'s sine transform runs on
the same file.

OpenCV's border is kept: the last row and column are covered by no block, and come out 0 on
8-bit data and NaN on floats. 1 or 3 channels; `block_size` even, 2 to 64 and smaller than
both sides (OpenCV rejects odd sizes). Suite 279 compares 16 cases against cv2 with IPP off,
`block_size` 2 to 16 (the powers of two and the lengths whose DFT runs through the radix-3,
-5 and -7 passes), from 8-bit, float and double data: every value equal, NaN where
OpenCV's is, in the ordinary and the deterministic build. With IPP on, as the pip wheel
ships, cv2's DCT is IPP's and differs: a coefficient near `3 sigma` can land on the other
side, and 8-bit results move by up to 8 levels.

### `WAVELET`

Wavelet shrinkage. Each channel goes through a multilevel 2D orthogonal wavelet transform,
every detail sub-band is thresholded, and the transform is inverted. Noise spreads evenly
over the detail coefficients while an image concentrates in a few large ones, so shrinking
the small ones toward zero removes noise and keeps edges. `wavelet` picks one of the
Daubechies `ALWAN_WAVELET_DB1` (Haar) to `DB8`, the symlets `SYM2` to `SYM8`, the coiflets
`COIF1` to `COIF5`, or the biorthogonal `BIOR1_1` to `BIOR6_8` and reverse biorthogonal
`RBIO1_1` to `RBIO6_8` (PyWavelets' `bior1.1` ... `rbio6.8`); longer filters give smoother
results and ring more near edges, Haar leaves blocks. A biorthogonal bank has different
analysis and synthesis filters and the transform uses each, so it reconstructs exactly;
its thresholds are the orthogonal ones, which scikit-image warns assume an orthogonal
transform (its noise in the sub-bands is coloured). alwan computes what scikit-image
computes.

The threshold is BayesShrink (Chang, Yu and Vetterli, IEEE TIP 2000) by default, one per
sub-band, `sigma^2 / sqrt(max(mean(d^2) - sigma^2, eps))`, or VisuShrink (Donoho and
Johnstone, Biometrika 1994) with `wavelet_visushrink`, the universal `sigma sqrt(2 ln n)`
over the channel's `n` pixels, which removes more. Soft thresholding shrinks the kept
coefficients by the threshold; `wavelet_hard` keeps them unchanged, sharper but with more
artefacts. `sigma` 0 estimates the noise per channel as `median(|d|) / 0.6745` over the
finest diagonal sub-band (Donoho and Johnstone); a given `sigma` is in the data's own units,
0..255 for 8-bit.

It follows scikit-image's `restoration.denoise_wavelet` with `channel_axis` set and
`convert2ycbcr` off, over PyWavelets' `symmetric` border mode, and the filter banks are
PyWavelets' own (`data/wavelets/wavelet_filters.csv`). Suite 206 agrees to 1.1e-15 in
double over nine cases of every option, to 1.8e-7 in float32 (PyWavelets transforms float32
in float32) and within half a level in 8-bit. The published symlet coefficients are
orthogonal to about 1e-12 only, so with no threshold a symlet round trip returns the image
to 1e-11, as PyWavelets' own does.

### `MEDIAN`

Each value of each channel becomes the median of the `kernel_size` x `kernel_size` window
around it, the border replicated. The window holds an odd count, so the median is one of
its values: a lone outlier (a hot or dead pixel, salt-and-pepper noise) is removed
outright rather than spread, as averaging would spread it, and a straight step edge stays
exactly where it is. It rounds corners and removes lines thinner than half the window,
and on Gaussian noise it does less than the methods above. Channels are filtered apart.

8-bit data slides a 256-bin histogram along each row (Huang, Yang and Tang 1979), so the
cost per pixel grows with the window's side, not its area; floats select the middle value
of each window. Suite 211 is equal, value for value, to OpenCV's `medianBlur` on 8-bit
grey and colour images at sizes 3 to 21, to `scipy.ndimage.median_filter` with
`mode='nearest'` on float64 at 3 to 9, and to `medianBlur` on float32 at 3 and 5 (the sizes
OpenCV takes float32 at).

### `TV_BREGMAN`

The same total-variation model as `TV_CHAMBOLLE`, written the other way round,
`weight / 2 |u - f|^2 + |grad u|`, and solved by split Bregman (Goldstein and Osher, SIAM
J. Imaging Sciences 2009): Gauss-Seidel sweeps for `u`, a shrinkage of the gradient plus
its Bregman variable for `d`, and the Bregman update, repeated until a sweep's RMS change
falls to `tolerance` or `iterations` have run. `weight` weighs fidelity here, so a larger
value keeps more of the image, where Chambolle's `weight` weighs smoothness.
`anisotropic` penalises `|u_x| + |u_y|`, which prefers edges along the axes; the default
isotropic form treats every direction alike.

It follows scikit-image's `restoration.denoise_tv_bregman` (its Cython kernel) sweep for
sweep: the frame of one pixel scikit-image surrounds the image with (the second row and
column at the top and left, the last at the bottom and right, zero corners, never
updated), and its isotropic shrinkage `s lambda t / (s lambda + 1)`. Suite 216 agrees to
the last bit in double on grey and colour images, both forms, and stops decided by either
limit; within half a level in 8-bit and 2.4e-7 in float32, which scikit-image computes in
float32.

Colour is each channel alone, from zero. scikit-image 0.26's `channel_axis` path is not:
it reuses one output buffer across channels and its kernel starts the Bregman variables as
copies of it, so each channel after the first starts from the previous channel's result.
Its second and third channels come out 0.016 and 0.020 away from the same channel
denoised alone, and reversing the channel order changes them. Suite 216 compares colour
with scikit-image's single-channel calls.

### `NL_MEANS_DARBON`, `NL_MEANS_BUADES`

Non-local means on float data (and on 8-bit through double in 0..1, rounded back): each
pixel becomes the mean of the pixels within `search_window / 2` of it, each weighted by
`exp(-d)`, where `d` compares the `template_window` patches around the two over all
channels, less `2 sigma^2` a sample for a known noise level, and is scaled by `h`. Pixels
whose neighbourhoods look alike are averaged whatever their distance in the search
window, so texture and edges that repeat keep their shape where a local filter would blur
them. A weight past a distance of 5 is 0. The weight is the exact `exp(-d)`; set
`nl_means_fast_exp` to weigh by scikit-image's Schraudolph approximation (1999) instead, a
few percent off `exp`, when its results are wanted to the bit.

| | `NL_MEANS_DARBON` | `NL_MEANS_BUADES` |
|---|---|---|
| Patch distance | the flat mean of the squared differences, over `channels h^2` | weighted by a gaussian of sigma `(template_window - 1) / 4`, summing to `1 / (channels h^2)` |
| How | every shift at once by an integral image of the squared differences (Darbon et al. 2008), each pair visited once and weighted both ways | patch by patch (Buades, Coll and Morel 2005), a patch's sum stopped once past 5 |
| Cost a pixel | about the search window's area | that times the patch's area |
| Precision | double | the data's |

`h` is in the data's units, 0.1 by default (25.5 on 8-bit data), `template_window` 7 (an
even size is raised by one) and `search_window` 23, a patch distance of 11; `sigma` 0
subtracts nothing. These are scikit-image's `restoration.denoise_nl_means` with its fast
mode and with `fast_mode=False`, and suite 231 holds them to it value for value: grey and
colour, double and float32, patches of 3 to 7, distances of 3 to 11, `h` and `sigma`, and
8-bit, 12 cases. With `nl_means_fast_exp` every value is equal but for `NL_MEANS_BUADES`
on float32, 3.6e-7 away, because numpy's float32 `exp` builds the gaussian patch kernel a
unit apart from the C library's. With the exact `exp`, the default, the same cases land
within 3e-3 of scikit-image's, the reach of its approximation. `ALWAN_DENOISE_NL_MEANS` is
OpenCV's 8-bit algorithm and stays as it was.

### `BILATERAL_OPENCV`

The bilateral filter (Tomasi and Manduchi, ICCV 1998): each pixel the mean of its window,
each neighbour weighted by a Gaussian of its distance (`sigma_space`) and one of its
difference from the centre (`sigma_color`), so flat regions are averaged and edges are not.
This is `cv::bilateralFilter` itself (OpenCV 5.0.0, the 3-clause notice in
`api/alwan_denoise_bilateral.c`): the window is the pixels within `round(1.5 sigma_space)`
(or `bilateral_diameter / 2`), reflected 101 at the border; on 8 bits the difference reads a
256-level table OpenCV fills with its polynomial `v_exp`, and on floats a 4096-bin table
over the image's range, interpolated linearly; three channels compare by the sum of their
absolute differences. Its float path leaves the centre out of the window and adds it with
weight 1 at the end. OpenCV's AVX2 kernel and its scalar tail add differently (fused
multiply-adds in blocks of 32 or 16 pixels, a 13-pixel window in its own load order, a
different interpolation of the float table), and the port follows each. One or three
channels; a double call is computed in float, as OpenCV has no double form. Suite 298 is
equal to `cv2.bilateralFilter` with IPP off on every value of 48 cases (8-bit and float32,
one and three channels, windows of 5 and 13 pixels and wider, widths across both blocks),
in both builds.

### `BILATERAL_SKIMAGE`

The same filter as scikit-image's `restoration.denoise_bilateral`: the difference is the
Euclidean distance over the channels, read from a `bilateral_bins` table over `[0, max)`,
and the window any border. Two of scikit-image's details set its values and are kept: its
spatial table is built over `arange(-win // 2, win // 2 + 1)`, one sample wider than the
window, and read with the window's width, so the spatial weights are not the radially
symmetric Gaussian a reader would expect; and the colour index is `(bins / channels)` (an
integer division) `/ max x distance`, truncated. Two are not: an image with a negative value
comes back shifted by its minimum in scikit-image and in place here, and an 8-bit image is
filtered as `v / 255` with that image's maximum, where scikit-image takes the maximum of the
8-bit codes and so spans its colour table over 0..255 for data in 0..1. `border` maps to
scikit-image's modes: `NEAREST` its `'edge'`, `REFLECT` its `'symmetric'`, `MIRROR` its
`'reflect'`, `CONSTANT` its default `'constant'` with `cval`, `WRAP` its `'wrap'`. Suite 298
is equal to scikit-image on every float64 and 8-bit value of 30 cases; the float32 path runs
in float as scikit-image's does, but its tables come from `exp` in double where
scikit-image's float32 `exp` is neither the C runtime's nor correctly rounded, so it is
within 2.4e-7.

### `WIENER_LOCAL`

Lee's local adaptive Wiener filter (1980) as `scipy.signal.wiener` computes it: the mean
and variance of each `wiener_size` square window (zero outside the image), the noise power
the mean of the local variances unless `wiener_noise` gives it, and each pixel pulled to its
window's mean by `noise / variance`, or set to the mean where the variance is below the
noise. Flat regions are smoothed and busy ones kept. Per channel. scipy adds the window
sums with an FFT on almost every image size its method picker sees (anything past about
32 x 32 with a 3 x 3 window); alwan adds them directly, row by row from 0 as scipy's direct
method does, and suite 298 is equal to scipy's formula with `method='direct'` on every value
of 36 cases in both builds. scipy's default differs from that by its FFT's rounding, at
most 1.7e-15 on those cases. On 8 bits it works on the codes and rounds back; scipy squares
an 8-bit image in 8 bits, which wraps, and is not followed there.

## Local contrast

```c
typedef enum {
    ALWAN_LOCAL_CONTRAST_LAPLACIAN = 0,
    ALWAN_LOCAL_CONTRAST_CLAHE = 1,
    ALWAN_LOCAL_CONTRAST_HISTOGRAM_EQUALIZE = 2
} alwan_local_contrast_method;

alwan_status alwan_local_contrast_{T}(alwan_{T} *out, size_t out_row_stride,
                                      alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                      size_t width, size_t height,
                                      alwan_local_contrast_method method,
                                      alwan_local_contrast_params const *params);
alwan_status alwan_local_contrast_u8(...);   /* same, unsigned char pixels */
alwan_status alwan_local_contrast_u16(...);  /* same, unsigned short pixels */
```

Contrast that adapts to each neighbourhood, and its global form. The 8-bit entry point
runs all three methods and the 16-bit one `LAPLACIAN` and `CLAHE`, the local Laplacian
through double in 0..1, rounded back as MATLAB rounds integer output. The float entry
points run `LAPLACIAN` and `HISTOGRAM_EQUALIZE` and refuse `CLAHE`, whose histogram bins
need integer data. `out` may alias `src`.

| Field of `alwan_local_contrast_params` | Method | 0 reads as |
|---|---|---|
| `sigma` | `LAPLACIAN` | 0.4 |
| `alpha` | `LAPLACIAN` | 0.5 |
| `beta` | `LAPLACIAN` | 1 |
| `intensity_levels` | `LAPLACIAN` | MATLAB's count from `alpha`, 16 to 50 |
| `separate_channels` | `LAPLACIAN` | 0: filter the luma of three channels |
| `tiles_x`, `tiles_y` | `CLAHE` | 8 |
| `clip_limit` | `CLAHE` | OpenCV's 40; a negative value turns clipping off |
| `bins` | `HISTOGRAM_EQUALIZE` on floats | 256 |

The `LAPLACIAN` defaults are MATLAB's documented example, `locallapfilt(I, 0.4, 0.5)`.

### `LAPLACIAN`

Paris, Hasinoff and Kautz (SIGGRAPH 2011) build the output's Laplacian pyramid one
coefficient at a time, each from a copy of the image remapped around that pixel's own value
`g0`. A difference `d` from `g0` up to `sigma` is detail and becomes
`sigma (|d| / sigma)^alpha`; a larger one is an edge and becomes `beta (|d| - sigma) + sigma`.
Because the pyramid, not a blur, separates the two, edges keep their shape and nothing
halos.

| Setting | Effect |
|---|---|
| `alpha` < 1 | more local detail, the "clarity" or texture look |
| `alpha` > 1 | smoother detail, a skin or noise softener |
| `beta` < 1 | compressed tonal range, a tone mapper that keeps detail |
| `sigma` | the size of a difference that still counts as detail, in the data's units |

With `alpha` < 1, differences under 0.01 are blended back to the identity so that grain
is not amplified; that level is in the data's units, so scale an image to 0..1 before
boosting its detail. This uses the fast form of Aubry et al. (ACM TOG 2014): the image is
remapped at `intensity_levels` values across its range and the pyramids are blended per
pixel. With three channels and `separate_channels` 0 the filter runs on the luma and
scales the channels by it, which keeps their ratios and so the hues; otherwise each channel
is filtered. The filter follows MATLAB's `locallapfilt`. Its compiled pyramid, remap and
blend steps were read by probing them (the kernel `[.05 .25 .4 .25 .05]`, half-sample
symmetric borders, `floor(log2(min(w, h))) + 1` levels, linear weights between intensity
levels; gendata/tests/local_laplacian_probe.m in alwan_dev). Suite 198 agrees with MATLAB
to 1.5e-6 over eight cases, the size of MATLAB's single precision.

### `CLAHE`

Contrast-limited adaptive histogram equalisation (Zuiderveld, Graphics Gems IV, 1994).
The image is cut into `tiles_x` by `tiles_y` tiles. Each tile's histogram is clipped at
`clip_limit` times its mean bin count, the excess is spread back over every bin, and the
cumulative histogram becomes that tile's tone curve. Each pixel is mapped by the curves of
its four nearest tiles, blended bilinearly. Flat regions gain contrast and `clip_limit`
caps how much: 2 to 4 is the usual photographic range.

One channel of 8-bit (256 bins) or 16-bit (65536 bins) values. For a colour image,
equalise a lightness channel (CIELAB L*, Oklab L, or luma) and rebuild the colour from it;
equalising R, G and B apart shifts hues. It reproduces OpenCV's `cv::createCLAHE` bit for
bit, including its padding of an image that does not divide into tiles (suite 197).

The clip level is counted per bin: `(int)(clip_limit * tile_area / bins)`, at least 1.
With 65536 bins, a tile of fewer than `65536 / clip_limit` pixels clips at one count
whatever `clip_limit` says, so on 16-bit data clip limits of 2 and 4 give the same image
unless the tiles are large. OpenCV behaves the same way. For photographic clip limits on
an image of ordinary size, equalise an 8-bit lightness channel.

### `HISTOGRAM_EQUALIZE`

Global histogram equalisation: one tone curve for the whole image, its cumulative
histogram, so that the levels are used about equally often. It is what CLAHE does per tile
without the clip; with nothing to limit it, a large flat area (a sky, a wall) takes most
of the range and its noise with it. One channel; for a colour image equalise a lightness
channel and rebuild the colour from it.

On 8-bit data it is OpenCV's `equalizeHist` bit for bit: the first occupied level goes to
0 and each level `v` above it to the count at or below `v`, past that first level, times
`255 / (n - h[first])`, the product in float and rounded half to even. An image of one
level is left as it is. There is no 16-bit form: OpenCV's is 8-bit only and
scikit-image's integer path is a different curve, so neither can be followed for 16 bits;
use the float path and scale.

On floats it is scikit-image's `exposure.equalize_hist`: a `numpy.histogram` of `bins`
bins over the image's own [min, max] (widened by a half on each side when the image is
constant), the cumulative counts over `n` as the curve at the bin centres, and each value
interpolated on it with `numpy.interp`'s rules. The result is in 0..1 whatever the input's
range, and any finite values are accepted, signed or past 1.

Suite 210 matches OpenCV exactly on four 8-bit images (a frame's luma, a low-contrast
strip, levels with gaps, two levels) and scikit-image exactly on five float64 ones at 64,
256 and 1000 bins, a constant and a signed one among them. On float32 input scikit-image
builds its bin edges in float32 and alwan in double, and 13 of 28800 values differ by one
float32 step.

## Deconvolution

```c
typedef enum {
    ALWAN_DECONVOLVE_WIENER = 0,
    ALWAN_DECONVOLVE_RICHARDSON_LUCY = 1
} alwan_deconvolve_method;

alwan_status alwan_deconvolve_{T}(alwan_{T} *out, size_t out_row_stride,
                                  alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                  size_t width, size_t height,
                                  alwan_{T} const *psf, size_t psf_width, size_t psf_height,
                                  alwan_deconvolve_method method,
                                  alwan_deconvolve_params const *params);
```

An image blurred by a known point-spread function (a lens's blur, a motion streak, a
defocus disc) sharpened back. Each of 1 to 4 channels is deconvolved on its own with the
same PSF, given row by row and at most the image's size; `out` may be `src`.

| Field of `alwan_deconvolve_params` | Method | 0 reads as |
|---|---|---|
| `balance` | `WIENER` | 0.1 |
| `iterations` | `RICHARDSON_LUCY` | 50 |
| `filter_epsilon` | `RICHARDSON_LUCY` | off |
| `clip` | both | 0: no clipping; non-zero clips to [-1, 1] as scikit-image does |

### `WIENER`

The Wiener filter with a Laplacian regulariser, in the 2D DFT domain:
`conj(H) / (|H|^2 + balance |L|^2)`, `H` the transform of the PSF and `L` that of
`[[0,-1,0],[-1,4,-1],[0,-1,0]]`, each with its element `(floor(h / 2), floor(w / 2))` at the
origin. Dividing by `H` alone would restore the frequencies the blur weakened and blow up
the noise at those it all but removed; the Laplacian term charges for high frequencies,
and `balance` sets how much. One pass, and the image is taken as periodic, so a blur that
crossed the frame's edge in the real scene rings there. At least 3 x 3.

### `RICHARDSON_LUCY`

Richardson (1972) and Lucy (1974): the maximum-likelihood image under Poisson noise, by
`u *= (image / (u * psf)) * flip(psf)` from 0.5 everywhere, the convolutions with zeros
outside the image and centred at `(P - 1) / 2`, `iterations` times. Each iteration
sharpens further and amplifies noise further, so the count is the regulariser. With a
non-negative image and PSF it stays non-negative, and a PSF summing to 1 conserves flux.
`filter_epsilon` zeroes the ratio where the blurred estimate falls below it, which keeps
dark noise from being divided up.

Suite 214 holds both to scikit-image's `restoration.wiener` and `richardson_lucy` (with
`clip=False`) on a blurred frame, grey and colour, with a Gaussian, a motion streak and an
even-sized box PSF: 2.7e-15 in double, and 6e-7 in float32, where scikit-image computes in
float32. The transforms are alwan's own (see `L0_SMOOTH`).

## Inpainting

```c
typedef enum {
    ALWAN_INPAINT_BIHARMONIC = 0,
    ALWAN_INPAINT_TELEA_OPENCV = 1,
    ALWAN_INPAINT_NS_OPENCV = 2
} alwan_inpaint_method;

alwan_status alwan_inpaint_{T}(alwan_{T} *out, size_t out_row_stride,
                               alwan_{T} const *src, size_t src_row_stride, size_t channels,
                               size_t width, size_t height,
                               unsigned char const *mask, size_t mask_row_stride,
                               alwan_inpaint_method method, alwan_inpaint_params const *params);
alwan_status alwan_inpaint(void *out, size_t out_row_stride, void const *src, size_t src_row_stride,
                           alwan_pixel_format format, size_t channels, size_t width, size_t height,
                           unsigned char const *mask, size_t mask_row_stride,
                           alwan_inpaint_method method, alwan_inpaint_params const *params);
```

The pixels a mask marks (non-zero bytes, one per pixel) filled from their surroundings: a
dust spot, a scratch, a hot pixel cluster, a wire or a date stamp to remove. 1 to 4
channels; `out` may be `src`. `BIHARMONIC` never reads the values `src` holds under the
mask; the OpenCV methods can (see below). `alwan_inpaint` takes a pixel format: U8 and U16
for the OpenCV methods, F32 and F64 for all three.

| Field of `alwan_inpaint_params` | Method | 0 reads as |
|---|---|---|
| `unclipped` | `BIHARMONIC` | 0: clip each channel to its known pixels' range, as scikit-image |
| `radius` | `TELEA_OPENCV`, `NS_OPENCV` | 3, OpenCV's usual `inpaintRadius`; rounded half to even and held to 1..100 as OpenCV does |

### `BIHARMONIC`

Each masked pixel satisfies the discrete biharmonic equation, the Laplacian applied twice
(a 13-point stencil: 20 at the pixel, -8 at its four neighbours, 2 at the diagonals, 1 two
away), with the known pixels as boundary values. A harmonic fill would meet the
surroundings' values but put a crease where their slope changes; the biharmonic one meets
the slope too, so a gradient or a curved surface carries on through the hole, and a linear
ramp is filled exactly. It knows nothing about texture: a hole in grass or hair fills with
a smooth blur of the colours around it, so it suits small defects rather than large
objects. Within two pixels of the image's edge the stencil is the one the reflecting
border gives on the part of the window that fits, as scikit-image computes it. After the
solve each channel is clipped to the range of its known pixels, as scikit-image does;
`unclipped` keeps the solution as it is, which can overshoot where the surroundings curve.

The system is solved directly. The masked pixels are split into the groups the stencil
couples, each ordered row by row, and each group is solved by banded Gaussian elimination
with partial pivoting. That is fast and small for spots, lines and scratches, whatever
their number; a single solid hole costs memory in proportion to its area times its width,
about 400 MB for 200 x 200, and returns `ALWAN_E_NOMEM` when that does not fit.

Suite 215 holds it to scikit-image's `restoration.inpaint_biharmonic`, solved by SuperLU,
to 5.8e-15 on grey and colour images with scattered spots, a diagonal scratch, a solid
block and a mask along the top edge into a corner, and to 9.5e-7 on float32, which
scikit-image solves in float32.

### `TELEA_OPENCV` and `NS_OPENCV`

OpenCV 5.0.0's `cv::inpaint`, ported from `photo/src/inpaint.cpp` (Intel Corporation's
licence, BSD-style; the notice is in `api/alwan_inpaint_opencv.c`). Both march the mask's
boundary inwards in order of distance, a fast-marching front keyed by arrival time, and set
each pixel from the known pixels within the radius. `TELEA_OPENCV` is Telea 2004: a sum
weighted by distance, by how close the pixel lies to the front and by its direction to it,
of each neighbour's value carried forward by its gradient. `NS_OPENCV` is OpenCV's reading
of Bertalmio, Bertozzi and Sapiro 2001: a mean weighted to follow the isophotes. Both are
quick and local, like `BIHARMONIC` better on thin defects than on large holes, and both
leave the blur and streaks those methods are known for.

Types. U8 with one channel runs OpenCV's `CV_8UC1` path, with three its `CV_8UC3` path (a
fourth channel is copied from `src`), U16 and F32 with one channel are OpenCV's `CV_16UC1`
and `CV_32FC1`. OpenCV takes nothing else; alwan runs each channel of a multi-channel U16,
F32 or F64 image through the one-channel path on its own, and computes F64 in float, as
`CV_32FC1`, writing back only the masked pixels (the known ones keep their bits).

OpenCV's arithmetic is kept as it is. An integer `TELEA_OPENCV` result is `cvRound(v + 0.5)`,
so about half the time it lands a level above the nearest; its central image difference is
scaled by 2 where the one-sided ones are not; its three-channel path takes the distance
weight's square root in double and the one-channel path in float, so a grey image filled
as one channel and as three copies can differ. Next to the image's first row or column the
gradient reads shift one pixel inwards (`km = k - 1 + (k == 1)`), so they can read a masked
pixel's `src` value before that pixel is filled. Known values are checked for being finite,
masked ones are not. An image one pixel high or wide makes OpenCV read outside its buffer;
alwan does not, and does not claim to match it there. A mask that marks every pixel is
`ALWAN_E_INVALID`, where OpenCV returns the image unchanged.

Suite 275 holds both to `cv2.inpaint` (IPP off) on every filled value, ordinary and
deterministic builds: five sizes, one two pixels wide, five masks (a thin diagonal
scratch, a disc, a band along two edges, a speckle, blocks against the edges), U8 with one
and three channels, U16 and F32, radii 3, 1, 5, 2.5 and 0.4. The forms OpenCV lacks are held
to the ones it has. Outside the suite, 1,008 cases drawn from 42 SRIC photographs at a
sixteenth of their size, RGB, RGBA, U16 and F32, radii 0.4 to 7, were equal too.

## Decolorization

```c
typedef enum {
    ALWAN_DECOLOR_LU2012 = 0
} alwan_decolor_method;

alwan_status alwan_decolor_{T}(alwan_{T} *out, size_t out_row_stride,
                               alwan_{T} const *src, size_t src_row_stride, size_t channels,
                               size_t width, size_t height,
                               alwan_decolor_method method, alwan_decolor_params const *params);
```

An sRGB-encoded RGB image (3 or 4 channels, R first, the fourth ignored) to one grey channel
in [0, 1] that keeps the colour contrast a luma loses: a red and a green of the same
lightness, which any fixed weighting of R, G and B maps to the same grey, stay apart.

| Field of `alwan_decolor_params` | Method | 0 reads as |
|---|---|---|
| `sigma` | `LU2012` | 0.02, the width of the contrast kernel |
| `max_iterations` | `LU2012` | 15, OpenCV's `maxIter` (the loop stops after one more update) |
| `tolerance` | `LU2012` | 1e-4, the energy change that ends the loop |
| `working_size` | `LU2012` | 800: the fit runs on the image resized to at most this height plus width |

### `LU2012`

Lu, Xu and Jia (2012), "Contrast Preserving Decolorization", as OpenCV's `cv::decolor`
computes it: a port of OpenCV 5.0.0's `photo/src/contrast_preserve.cpp` (Apache-2.0, notice
in the source). The grey is a polynomial of degree two in R, G and B, nine weights on the
monomials R^r G^g B^b with 1 <= r + g + b <= 2. They are fitted on the image, brought down
to `working_size` in height plus width by OpenCV's float bilinear resize when larger, so
that each pair of horizontal and vertical neighbours differs in grey by the CIELAB
difference of their colours / 100, with the sign the three channels agree on (when all
rise or all fall by more than 0.05) and either sign otherwise; the fit starts from 0.33 on
R, G and B and takes the paper's expectation-maximisation steps. The weights are then
applied to the full image and the result stretched to [0, 1]. The CIELAB step is OpenCV's
own float conversion, which interpolates a 33-node int16 table rather than evaluating the
formula; alwan carries that table, read back from cv2 (`data/opencv/rgb2lab_lut_s16.csv`,
with OpenCV's licence beside it). Everything is in single precision as OpenCV has it, the
f64 entry point included.

`cv::decolor` returns 8 bits: `cvRound(out * 255)` of the f32 result is its grey, value
for value. A fit OpenCV cannot solve (a 9 x 9 pivot under 10 FLT_EPSILON, as for an image
of two flat colours) leaves its weights at 0, and a grey with no range is 0 everywhere, as
OpenCV's is. Its second output, the colour-boosted image, is not provided: it swaps L* for
the grey, which alwan's Lab conversions do.

Suite 271 holds it to `cv2.decolor` with IPP off on procedural images (red and green
stripes over a ramp, a noisy texture as RGB and RGBA, two flat colours, a flat image, and
610 x 250 ramps that go through the resize): all 183,172 greys equal, in the ordinary and
the deterministic build. With IPP on, as the pip wheel ships, OpenCV's resize is Intel's,
and on the resized case 31 of 152,500 greys are one level off.

## Gradient-domain editing

```c
typedef enum {
    ALWAN_GRADIENT_EDIT_CLONE_NORMAL = 0,
    ALWAN_GRADIENT_EDIT_CLONE_MIXED = 1,
    ALWAN_GRADIENT_EDIT_CLONE_MONOCHROME = 2,
    ALWAN_GRADIENT_EDIT_COLOR_CHANGE = 3,
    ALWAN_GRADIENT_EDIT_ILLUMINATION_CHANGE = 4,
    ALWAN_GRADIENT_EDIT_TEXTURE_FLATTENING = 5
} alwan_gradient_edit_method;

alwan_status alwan_gradient_edit(unsigned char *out, size_t out_row_stride,
                                 unsigned char const *src, size_t src_row_stride,
                                 size_t src_width, size_t src_height,
                                 unsigned char const *mask, size_t mask_row_stride,
                                 unsigned char const *dst, size_t dst_row_stride,
                                 size_t dst_width, size_t dst_height,
                                 size_t channels, alwan_gradient_edit_method method,
                                 alwan_gradient_edit_params const *params);
```

Poisson image editing (Perez, Gangnet and Blake 2003) and the edits OpenCV builds on the
same solver, as OpenCV 5.0.0's photo module computes them: a port of
`photo/src/seamless_cloning.cpp` and `seamless_cloning_impl.cpp` (Apache-2.0, notice in the
source). Each method sets a gradient field inside a mask and solves the Poisson equation for
the image whose gradients are closest to it, with the pixels on the region's frame held; the
solve is OpenCV's discrete sine transform, built on OpenCV's own mixed-radix DFT.

| Method | OpenCV | What it does |
|---|---|---|
| `CLONE_NORMAL` | `seamlessClone`, `NORMAL_CLONE` | src's gradients inside the mask, dst's outside; the region pasted into dst |
| `CLONE_MIXED` | `MIXED_CLONE` | at each pixel and channel, the stronger of src's and dst's gradient (by \|gx - gy\|) |
| `CLONE_MONOCHROME` | `MONOCHROME_TRANSFER` | the gradients of src's grey, so dst keeps its colour and takes src's texture |
| `COLOR_CHANGE` | `colorChange` | the region's gradients scaled per channel: its colour changes, its texture stays |
| `ILLUMINATION_CHANGE` | `illuminationChange` | the region's gradients remapped to alpha^beta \|g\|^-beta g, flattening highlights and shadows |
| `TEXTURE_FLATTENING` | `textureFlattening` | only the gradients on Canny edges of the masked image kept, washing texture out |

| Field of `alwan_gradient_edit_params` | Method | 0 reads as |
|---|---|---|
| `center_x`, `center_y` | `CLONE_*` | both 0: the centre of dst, (width / 2, height / 2) |
| `centre_on_mask_image` | `CLONE_*` | 0: the region's bounding box is centred there; non-zero centres the whole mask image instead (OpenCV's `*_WIDE` flags) |
| `red_mul`, `green_mul`, `blue_mul` | `COLOR_CHANGE` | 1.0 |
| `alpha`, `beta` | `ILLUMINATION_CHANGE` | 0.2 and 0.4 |
| `low_threshold`, `high_threshold` | `TEXTURE_FLATTENING` | 30 and 45 |
| `kernel_size` | `TEXTURE_FLATTENING` | 3 (Canny's Sobel aperture: 3, 5 or 7) |

A zero field reads as its default, so an exact 0 is out of reach; the factors go to float as
OpenCV's do, so 1e-300 becomes 0.

Images are 8-bit, as OpenCV's are: the solution is truncated to 8 bits, not rounded, and
every float step is single precision, as OpenCV has it. channels is 3 (RGB) or 4 (RGBA: the
fourth channel is copied through from dst for the clones and from src otherwise); row strides
are in bytes. mask is one byte per src pixel: non-zero pixels are the region, and the value
weights the gradients (255 is full weight, OpenCV's mask / 255); NULL is all 255. The mask is
eroded by 3 pixels (OpenCV's three 3 x 3 erosions) before it weights anything. The clones write
out at dst's size, first clearing a one-pixel frame of the mask as OpenCV does; the other methods
write out at src's size and ignore dst. out may be src or dst.

The clones return `ALWAN_E_RANGE` when the region does not fit inside dst, or is under 3 x 3
(OpenCV throws). A mask with nothing left after its frame is cleared leaves dst as it was.

Suite 272 holds every method to cv2 5.0.0 with IPP off, value for value, on procedural images:
the four clone flags, a region on dst's corner, the default centre, a weighted mask, colour
change with a mask and without, illumination change on the generic power and on the integer
and half powers, texture flattening at apertures 3, 5 and 7, and a 3 x 3 image; the ordinary
and the deterministic build alike. OpenCV's result depends on the CPU in two places, and
alwan's matches the AVX2 build the reference came from: the gradient magnitude of
`ILLUMINATION_CHANGE` is fused (sqrt of fma(x, x, y y)), and at beta exactly 0.5 OpenCV takes
the CPU's approximate reciprocal square root with one Newton step where alwan takes 1 / sqrt,
so on another CPU a few pixels may land a level apart there.

## Stylization filters

```c
typedef enum {
    ALWAN_STYLIZE_EDGE_PRESERVING_RECURSIVE = 0,
    ALWAN_STYLIZE_EDGE_PRESERVING_NORMCONV = 1,
    ALWAN_STYLIZE_DETAIL_ENHANCE = 2,
    ALWAN_STYLIZE_STYLIZATION = 3,
    ALWAN_STYLIZE_PENCIL_SKETCH_GREY = 4,
    ALWAN_STYLIZE_PENCIL_SKETCH_COLOR = 5,
    ALWAN_STYLIZE_OIL_PAINTING = 6
} alwan_stylize_method;

alwan_status alwan_stylize(unsigned char *out, size_t out_row_stride,
                           unsigned char const *src, size_t src_row_stride,
                           size_t width, size_t height, size_t channels,
                           alwan_stylize_method method, alwan_stylize_params const *params);
```

OpenCV's non-photorealistic filters on the domain transform (Gastal and Oliveira 2011), a
port of OpenCV 5.0.0's `photo/src/npr.cpp` and `npr.hpp` (Apache-2.0, notice in the
source). The domain transform maps each row and each column to a line whose length grows
with the colour change between neighbours, so a 1-D filter along that line smooths flat
areas and stops at edges; three passes, each horizontal then vertical, with the extent
shrinking by half each pass.

| Method | OpenCV | What it does | sigma_s, sigma_r |
|---|---|---|---|
| `EDGE_PRESERVING_RECURSIVE` | `edgePreservingFilter`, `RECURS_FILTER` | the recursive filter along the transformed lines | 60, 0.4 |
| `EDGE_PRESERVING_NORMCONV` | `NORMCONV_FILTER` | the normalised convolution: a box in the transformed domain | 60, 0.4 |
| `DETAIL_ENHANCE` | `detailEnhance` | CIELAB L* split into the recursive filter's base and the rest, the rest tripled | 10, 0.15 |
| `STYLIZATION` | `stylization` | the normalised convolution, darkened by 1 - the summed Sobel magnitude of its channels | 60, 0.45 |
| `PENCIL_SKETCH_GREY` | `pencilSketch`, first output | shade_factor times the widths of the domain boxes; one channel out | 60, 0.07 |
| `PENCIL_SKETCH_COLOR` | `pencilSketch`, second output | the grey drawing as the Y of the image's YCrCb | 60, 0.07 |
| `OIL_PAINTING` | `xphoto::oilPainting` | each pixel the mean colour of its window's most frequent luminance | see below |

`alwan_stylize_params` holds `sigma_s` (the spatial extent, in pixels), `sigma_r` (the range
extent, in [0, 1] colour units) and `shade_factor` (the pencil drawing's darkness, 0.02).
A zero field reads as the method's default above; the values go to float, as OpenCV takes
them.

Images are 8-bit, channels 3 (RGB) or 4 (RGBA, the fourth copied through), and 1 as well for
the oil painting; the grey sketch writes one channel. Row strides are in bytes, and out may be src. OpenCV computes in BGR,
and alwan hands the port BGR: the normalised convolution decodes flat indices in a way that
is not symmetric in the channels.

The OpenCV operations the functions call are reproduced from its sources, each as the x64
build computes it with IPP off: the 8-bit conversions (cvRound, ties to even; an infinite or
NaN value is 0, as the SSE conversion's overflow makes it), float BGR to Lab (the int16
table `alwan_decolor` reads) and Lab to BGR (blocks of eight pixels with reciprocal
constants and a scalar tail with divisions; the sRGB curve a cubic spline over 1024
intervals, its coefficients and the matrix in `data/opencv/lab2srgb_float.csv`, rebuilt from
OpenCV's source by `gendata/data/stylize_tables.py`), float YCrCb both ways (fused
multiply-adds in blocks of eight, the tail unfused), the 3 x 3 Sobel (its [1 2 1] pass sums
(a + c) + 2b on the vector blocks, (a + 2b) + c on the tail) and `magnitude` (fused from
sixteen values). The filter itself is OpenCV's scalar code: single precision, powf for the
recursive feedback, the feedback's exp and each pass's sigma in double.

One OpenCV behaviour is kept rather than repaired: when a whole row or column is shorter in
the transformed domain than the normalised convolution's radius (a flat run of fewer than
about 91 pixels at the defaults), OpenCV's index decoding reads one float past its table
and divides by zero, the non-finite value spreads through the running sums, and every pixel
it reaches comes out 0. alwan reads 0 at that index and gets the same result.

### `OIL_PAINTING`

opencv_contrib 5.0.0's `xphoto::oilPainting` (`xphoto/src/oilpainting.cpp`, Apache-2.0;
after the histogram method in Holzmann, "Beyond Photography", 1988). The luminance is
OpenCV's 8-bit BGR2GRAY, `(b 3735 + g 19235 + r 9798 + 2^14) >> 15`, divided by
`oil_dyn_ratio` and rounded (`cvRound`, ties to even); for each pixel the window of side
`2 oil_size + 1`, clipped at the image's edges, is histogrammed by that quantised luminance,
and the pixel becomes the mean colour of the most frequent bin (the first, on a tie). The
histogram and the float colour sums slide along each row. The mean is the float sum times
`1 / count` in double, rounded to float; three channels round it to 8 bits half to even,
one channel truncates it, as OpenCV's `static_cast` does.

`oil_size` 0 reads as 10 and `oil_dyn_ratio` 0 as 1, the values of OpenCV's sample;
`oil_dyn_ratio` goes to 127, as OpenCV's does. Channels 1, 3 or 4 (the fourth copied
through), any size from 1 x 1, out may be src. The luminance is OpenCV's default
conversion; its other `cvtColor` codes are not ported. OpenCV requantises a copy of its
result by `dynRatio` after handing the result back, which has no effect; the output here is
the same. Suite 279 compares eight cases against cv2 (one and three channels, sizes 1 to
25, windows past every edge, `dynRatio` 1 to 127): every byte equal, with IPP off and on.

Suite 274 holds it to cv2 5.0.0 with IPP off on procedural images, eight sizes from 2 x 2
(one with flat black rows and columns that take the path above), every method at its
defaults and at other parameters: all 101,312 bytes equal, in the ordinary and the
deterministic build. On the 166 SRIC photographs at a twelfth of their size (41 million
bytes, six methods) every byte equals cv2's with IPP off; with IPP on, as the pip wheel
ships, one byte is one level off.

## Resizing

```c
typedef enum {
    ALWAN_RESIZE_NEAREST = 0, ALWAN_RESIZE_BOX = 1, ALWAN_RESIZE_BILINEAR = 2,
    ALWAN_RESIZE_HAMMING = 3, ALWAN_RESIZE_BICUBIC = 4, ALWAN_RESIZE_LANCZOS = 5,
    ALWAN_RESIZE_LANCZOS2 = 6, ALWAN_RESIZE_LANCZOS4 = 7, ALWAN_RESIZE_MITCHELL = 8,
    ALWAN_RESIZE_BSPLINE = 9, ALWAN_RESIZE_GAUSSIAN = 10,
    ALWAN_RESIZE_MAGIC_KERNEL_SHARP_2013 = 11, ALWAN_RESIZE_MAGIC_KERNEL_SHARP_2021 = 12,
    ALWAN_RESIZE_OPENCV_CUBIC = 13, ALWAN_RESIZE_OPENCV_AREA = 14
} alwan_resize_method;

alwan_status alwan_resize_{T}(alwan_{T} *out, size_t out_row_stride, size_t out_width, size_t out_height,
                              alwan_{T} const *src, size_t src_row_stride, size_t channels,
                              size_t width, size_t height,
                              alwan_resize_method method, alwan_resize_params const *params);
alwan_status alwan_resize_u8(...);   /* same, unsigned char pixels */
```

An image resampled to `out_width x out_height`, 1 to 4 channels each on its own. Every
method but `NEAREST` is a separable convolution whose filter is stretched by the reduction
factor when shrinking, so each output averages all the source it covers and fine detail
does not alias into moire; enlarging uses the filter at its own width.

| Method | Filter | Support |
|---|---|---|
| `NEAREST` | the nearest sample | |
| `BOX` | 1 on (-0.5, 0.5]: the mean of the covered area when shrinking | 0.5 |
| `BILINEAR` | the triangle `1 - |x|` | 1 |
| `HAMMING` | a Hamming-windowed sinc: sharper than bilinear at the same cost | 1 |
| `BICUBIC` | Keys' cubic convolution, `a = -0.5` | 2 |
| `LANCZOS` | `sinc(x) sinc(x / 3)`: the sharpest, with the most ringing at edges | 3 |
| `LANCZOS2` | `sinc(x) sinc(x / 2)`: less ringing than Lanczos-3, a little softer | 2 |
| `LANCZOS4` | `sinc(x) sinc(x / 4)`: sharper still, rings more | 4 |
| `MITCHELL` | the Mitchell-Netravali (1988) cubic with `B` and `C` (`cubic_b`, `cubic_c` when `cubic_bc_set`, else their recommended `B = C = 1/3`): a balance of blur, ringing and anisotropy. `B = 0, C = 0.5` is `BICUBIC`; `B = 0, C = 0` the Hermite cubic | 2 |
| `BSPLINE` | the cubic B-spline (`B = 1, C = 0`): smooth and free of ringing, but it blurs and does not pass through the samples | 2 |
| `GAUSSIAN` | `exp(-x^2 / (2 sigma^2))`, `sigma` from `gaussian_sigma` (0.5 by default) | `4 sigma` |
| `MAGIC_KERNEL_SHARP_2013` | Costella's Magic Kernel Sharp: the magic kernel convolved with its sharpening step, piecewise quadratic | 2.5 |
| `MAGIC_KERNEL_SHARP_2021` | its 2021 revision, flatter in the passband | 4.5 |

`params->box` resamples a region of the source, `x0, y0, x1, y1` in pixels with fractions
allowed; all zero is the whole image. The resampling happens in the data's own values:
for an energy-correct result on display-encoded colour, convert to linear light first and
back after (alwan's transfer functions do both).

This is Pillow's `Image.resize` (libImaging's Resample.c and its nearest-neighbour scale),
and suite 233 holds it there value for value: every filter on 8-bit grey, colour and
four-channel images and float32 grey, shrinking by non-integer factors, enlarging, one
side only and a fractional box, 72 cases. It follows Pillow's weights and its arithmetic:
the horizontal pass first, a pass whose size and box leave the axis unchanged skipped,
8-bit data in 22-bit fixed point clipped to 8 bits after each pass, float32 through double
sums stored in float32 between the passes. The double entry point keeps its intermediate
in double and lands within 1.0e-7 of Pillow's float32 results, their rounding. A request for the source's own
size and box is a copy. Pillow premultiplies RGBA by its alpha before resampling; alwan
treats the fourth channel like the others, so premultiply first where alpha matters.

The seven kernels after `LANCZOS` run in the same scheme, Pillow's resampler with a kernel
Pillow does not have: the same pixel grid, the kernel widened by the reduction factor when
shrinking, its support clipped at the edges and the weights renormalised, 8-bit through
the same fixed point. Suite 291 holds each in double to the scheme with the kernel written
out from its definition (Mitchell and Netravali's paper, Costella's Magic Kernel pages, the
Lanczos window, a Gaussian cut at four sigma), 30 cases within 1.2e-15, and `MITCHELL`
with `B = 0, C = 0.5` to `BICUBIC` within 1.6e-15.

Two more methods are OpenCV's own resamplers, not Pillow's scheme: `cv::resize` ported
from OpenCV 5.0.0 (Apache-2.0, see THIRD_PARTY_NOTICES.md), the whole image only.

| Method | What it does |
|---|---|
| `OPENCV_CUBIC` | `INTER_CUBIC`: Keys' cubic with `a = -0.75` over the 4 x 4 pixels around `(x + 0.5) scale - 0.5`, the edge pixel repeated. The kernel is NOT widened when shrinking, so a strong reduction aliases where `BICUBIC` averages |
| `OPENCV_AREA` | `INTER_AREA`: shrinking both ways, each output the mean of the source area it covers (exactly the block mean at an integer factor); enlarging along either axis, OpenCV's area-weighted bilinear |

They reproduce OpenCV's arithmetic as cv2 runs it with IPP off: 8-bit cubic through its
11-bit coefficients, its vertical pass in OpenCV's SSE form (eight lanes in float, rounded
to even) and a fixed-point tail; float32 cubic with the vertical sum nested over whole
vectors of four as its SSE baseline evaluates it; the 8-bit area mean at a factor of two
as `(a + b + c + d + 2) >> 2` and otherwise the float mean rounded to even;
`computeResizeAreaTab`'s weights for other factors. Suite 291 holds them to cv2.resize
value for value on 8-bit, float32 and double, one to four channels, 36 cases. A box,
integration or a subpixel layout is `ALWAN_E_INVALID` with these two.

Two options leave Pillow's resampling for other ends. `integration` (an
`alwan_pixel_integration`, see Warping) makes each output pixel the mean of the source over
its own area, through `alwan_warp`'s integration with the scale as the map: the source is
reconstructed at sub-positions of the output pixel by nearest (`NEAREST`, `BOX`), bilinear
(`BILINEAR`, `HAMMING`, `GAUSSIAN`), Lanczos-4 (`LANCZOS4`, `MAGIC_KERNEL_SHARP_2021`), the
cubic B-spline (`BSPLINE`) or bicubic (the rest), and `kernel`, `samples`, `seed`
and `alpha_channel` mean what they mean there. `GRID` with `BOX` at an integer factor is the
block mean exactly.

`subpixel` is for text shown on an LCD. With a layout (`ALWAN_SUBPIXEL_RGB`, `BGR`, or the
vertical `VRGB`, `VBGR`), the image is resampled to three times the output along the
stripes, and each of the first three channels then takes the value at its own subpixel
through FreeType's default LCD filter, `(8, 77, 86, 77, 8) / 256` across the subpixels; a
fourth channel sits at the middle one. Glyph edges keep about three times the resolution
across the stripes on a display of that layout, and show coloured fringes on any other.
It needs three or four channels.

| Field of `alwan_resize_params` | 0 reads as |
|---|---|
| `box[4]` | the whole image |
| `integration`, `kernel`, `samples`, `seed`, `alpha_channel` | `POINT`: Pillow's resampling |
| `subpixel` | `ALWAN_SUBPIXEL_NONE` |
| `cubic_bc_set`, `cubic_b`, `cubic_c` | `MITCHELL`: `B = C = 1/3` |
| `gaussian_sigma` | `GAUSSIAN`: 0.5 |

## Warping

```c
typedef enum {
    ALWAN_WARP_NEAREST = 0, ALWAN_WARP_BILINEAR = 1, ALWAN_WARP_BICUBIC = 2,
    ALWAN_WARP_LANCZOS4 = 3, ALWAN_WARP_BSPLINE3 = 4, ALWAN_WARP_BSPLINE5 = 5
} alwan_warp_method;

alwan_status alwan_warp_{T}(alwan_{T} *out, size_t out_row_stride, size_t out_width, size_t out_height,
                            alwan_{T} const *src, size_t src_row_stride, size_t channels,
                            size_t width, size_t height,
                            alwan_warp_method method, alwan_warp_params const *params);
alwan_status alwan_warp_u8(...);   /* same, unsigned char pixels */
```

An image resampled through an affine or perspective map: a rotation, a shear, a keystone
correction, a registration found elsewhere. The map runs backwards, from each output pixel
to the source point it shows: the centre `(x + 0.5, y + 0.5)` of output pixel `(x, y)` goes
to `(m0 x + m1 y + m2, m3 x + m4 y + m5)`, divided for a perspective map by
`m6 x + m7 y + 1`. The source is sampled there by the method, and an output whose point
falls outside the image keeps `fill`.

`map` chooses where the source points come from:

| Map | The source point of output point `p` |
|---|---|
| `MATRIX` | the matrix above |
| `SWIRL` | `c + R(phi) (p - c)`, `phi = angle f(s)`, `s = 1 - min(abs(p - c) / radius, 1)` and `f(s) = 6s^5 - 15s^4 + 10s^3`: a turn that fades to nothing at the radius with its first and second derivatives |
| `FIELD` | a lattice of source points, two doubles each, `field_width x field_height` (the output's size by default, one point a pixel), read by `field_interpolation` (below); a NaN point has no source (fill) |
| `CALLBACK` | `callback(x, y, &sx, &sy, user)`; a zero return means no source (fill) |

A field is the form a flow field, a UV pass from a renderer, a lens-distortion table or a
mesh warp's control net takes; a callback takes any analytic map.

A field need not have a point for every pixel. A coarse lattice spans the output and is
read between its points by `field_interpolation`:

| Interpolation | Between the points | Point `(i, j)` sits at |
|---|---|---|
| `LINEAR` | bilinear: continuous, the slope jumps at every point, so straight lines bend into polygons | the texel centre `((i + 0.5) ow / gw, (j + 0.5) oh / gh)` |
| `CATMULL_ROM` | the Catmull-Rom cubic each way: through every point, continuous slope, may overshoot | the texel centre |
| `BSPLINE` | the uniform cubic B-spline each way: C2 and inside the points' hull, near the points but not through them | the texel centre |
| `NURBS` | a tensor-product NURBS surface of `field_degree` (1 to 7, 3 by default, fewer than the points each way) on clamped uniform knots, each point weighted by `field_weights` (positive; all 1 when NULL): a weight above 1 pulls the surface toward its point | a control net spanning the output corner to corner: the output's corners go to the corner points and its edges follow the edge rows' curves |

The first three continue the lattice past its edge along the edge's slope (point -1 is
`2 p0 - p1`, point -2 is `3 p0 - 2 p1`): a map affine near its edge stays affine past it,
and a coarse net does not smear the half cell between its outer points and the border. The lattice holds source
points, so it runs backwards like every map: a mesh warp's net placed over the output says
where each part of the output takes its picture from.

Suite 235 holds the four against scipy through a ramp image whose bilinear sample is the
source point itself: `LINEAR` against `RegularGridInterpolator`, `CATMULL_ROM` against
`CubicHermiteSpline` with Catmull-Rom tangents, `BSPLINE` and `NURBS` against `NdBSpline`
(the NURBS as the weighted surface divided by the weights' surface), eight lattices from
3 x 3 of degree 1 to 9 x 7 of degree 5, weighted and not, all within 1.2e-13.

| Field of `alwan_warp_params` | 0 reads as |
|---|---|
| `matrix[8]` | all 0: the identity |
| `perspective` | 0: the affine map, `m6` and `m7` unused |
| `fill[4]` | 0 in every channel |
| `map` | `ALWAN_WARP_MAP_MATRIX`: the matrix above |
| `swirl_center[2]`, `swirl_radius`, `swirl_angle` | the image centre, half the shorter side, no turn |
| `field`, `field_row_stride` | required by `ALWAN_WARP_MAP_FIELD` |
| `field_width`, `field_height` | the output's size: one point a pixel |
| `field_interpolation` | `ALWAN_WARP_FIELD_LINEAR` |
| `field_degree`, `field_weights` | NURBS: cubic, every weight 1 |
| `callback`, `callback_user` | required by `ALWAN_WARP_MAP_CALLBACK` |
| `integration` | `ALWAN_PIXEL_INTEGRATE_POINT`: one sample at the centre |
| `kernel` | `ALWAN_PIXEL_KERNEL_BOX`: the pixel's mean |
| `sequence` | `ALWAN_PIXEL_SEQUENCE_R2` |
| `samples` | 16 x 16 for `GRID`, 16 for `QMC`, a cap of 64 for `ADAPTIVE` |
| `tolerance` | 0.05 source pixels |
| `seed`, `disk` | seed 0, points over the square pixel (`disk` applies to the box only) |
| `alpha_channel` | 0: channels independent |
| `samples_out`, `samples_out_row_stride` | not written |

`NEAREST` takes the pixel under the point, `BILINEAR` the four around it, `BICUBIC` the
sixteen by the Catmull-Rom cubic (`a = -0.5`). With `POINT` integration nothing filters
against aliasing, so a map that shrinks much should follow a reduction by `alwan_resize`,
or integrate. To rotate the picture
counter-clockwise by `t` about `(cx, cy)`, the matrix is `cos t, -sin t,
cx - cx cos t + cy sin t, sin t, cos t, cy - cx sin t - cy cos t`, which is what Pillow's
`Image.rotate` builds.

This is Pillow's `Image.transform` with its `AFFINE` and `PERSPECTIVE` methods, value for
value, and suite 234 holds it there: rotations with `Image.rotate`'s own matrices, a scale
with a flip, a shear, a translation far enough to leave nearest's fixed-point range, a
larger output and a keystone, through the three methods on 8-bit grey, RGB and
four-channel images and float32 grey, with and without a fill colour, 53 cases. It follows
Pillow's edge rules (columns clamped, a missing lower row repeating the upper), its three
nearest-neighbour routes for affine maps (a pure scale, 16.16 fixed point, double), 8-bit
results truncated with bicubic clamped first, and on float32 its horizontal stage in float
arithmetic, as its macros compute it on `FLOAT32` pixels. The double entry point computes
in double throughout and agrees with Pillow's float32 to 1.3e-7. Pillow premultiplies RGBA
before a bilinear or bicubic transform; alwan's channels are independent at `POINT`.

Three more methods go past Pillow's:

| Method | Around the point |
|---|---|
| `LANCZOS4` | 8 x 8 pixels weighted by `sinc(t) sinc(t / 4)`, normalised to sum 1: OpenCV's `INTER_LANCZOS4` kernel, at the exact position |
| `BSPLINE3` | the cubic B-spline through the samples: the image prefiltered into B-spline coefficients once per call, then 4 x 4 of them weighted at the point |
| `BSPLINE5` | the same with the quintic B-spline, 6 x 6 coefficients |

All three repeat the edge pixel past the image, leave `fill` for a point outside it, and
round 8-bit results to nearest (half up, with clamping). The B-splines are
`scipy.ndimage.map_coordinates(order=3 or 5, mode='nearest')`, value for value: the image
padded by 12 repeated pixels each side as scipy pads it, prefiltered along y then x with
the gain and, per pole, the causal and anticausal passes with scipy's reflect
initialisations, then the weights of `get_spline_interpolation_weights` summed in row order.
Suite 291 holds them to scipy exactly on double, float32 and 8-bit, 18 cases. `LANCZOS4`
evaluates the kernel at the exact position, where cv2.remap and cv2.warpAffine round
positions to 1/32 pixel and tabulate the weights in float: on translations by multiples of
1/32 it agrees with cv2.remap to 3.6e-7 (float rounding), and on double at a rotation with
the kernel's definition to 8.9e-16. The B-splines pass through the samples, so they
overshoot near edges as Lanczos does, and they cost one prefilter pass over the image per
call.

### Output-space integration

A point sample of a map that compresses or bends the image aliases: a swirl's centre turns
into noise, fine text under a strong shrink breaks up. `integration` makes each output
pixel the mean of the source over the pixel's own area,

    C(p) = mean over d in the pixel of I(W(p + d)),

with the sub-offset `d` added before the map, so the map's own curvature is integrated and
any map (matrix, swirl, field, callback) takes it the same way. The source is reconstructed
at every sub-position by the method.

| Integration | Sub-positions |
|---|---|
| `POINT` | the centre; Pillow's transform for a matrix map |
| `GRID` | an n x n grid of cell centres (`samples` is n, 16 by default, up to 64): the brute-force reference |
| `QMC` | `samples` low-discrepancy points (16 by default, up to 4096) of `sequence`, made different per pixel by a hash of `(x, y, seed)` so neighbours do not share one pattern (below). `disk` spreads them over the disk of the pixel's area instead |
| `ADAPTIVE` | `QMC` with a count per pixel, from central differences of the map at half a pixel: the footprint (the Jacobian's largest column) and the second difference. A footprint at most 1 with a second difference at most `tolerance` takes one sample; otherwise the first of 4, 8, 16, 32, 64 not below the larger of footprint^2 and second difference / tolerance, capped by `samples`. A kernel other than the box takes 16 at least and four times the count |
| `EWA` | none: Heckbert's elliptical weighted average weighs the source pixels themselves. The Gaussian kernel is carried into the source by the map's Jacobian at the pixel, a reconstruction Gaussian of bilinear's variance (1/6) is added, and every source pixel under the ellipse (out to 3 sigma) is weighed, those past the image as the fill. The ellipse is widened to 0.36 source pixels squared at least, since a narrower Gaussian summed on the pixel lattice ripples, and narrowed past 16384 source pixels. `method` and `kernel` are not used |
| `AUTO` | per pixel, `EWA` where the map is locally affine (second difference within `tolerance`) and shrinks every way (the Jacobian's smaller singular value at least 1), `ADAPTIVE` where it curves or enlarges along any axis; the Gaussian kernel throughout, whatever `kernel` says |

`ADAPTIVE` reads the map, not the picture: a rigid turn, a translation or a mild
enlargement takes one sample even where the content has sharp edges, and the budget goes
where the map shrinks or bends. The one step it does look for is the source image's own
edge: a pixel whose footprint straddles it meets the fill in a step that no smoothness of
the map shows, and 64 shifted points leave that edge grainy (RMS 0.016 against 4096
points), so it takes four times `samples` (up to 4096), which brings it to 0.005, what 256
points everywhere reach. Edge pixels are few: on a 360 x 240 turn, 2,000 of 86,400. `samples_out` (one byte per pixel, 255 for more) records
the count each pixel took, for a heat map. On a 256 x 256 checker of 8-pixel squares under
a swirl of three turns, against a 48 x 48 grid, bilinear reconstruction:

| Integration | Mean samples | RMS inside the swirl |
|---|---|---|
| `POINT` | 1 | 0.349 |
| `QMC`, R2, 16 | 16 | 0.059 |
| `GRID`, 4 | 16 | 0.031 |
| `ADAPTIVE`, cap 64 | 37.7 | 0.020 |
| `QMC`, R2, 64 | 64 | 0.017 |
| `GRID`, 16 | 256 | 0.0009 |

`sequence` sets the points `QMC`, `ADAPTIVE` and `AUTO` draw:

| Sequence | Points | Per pixel |
|---|---|---|
| `R2` | Roberts' R2 (the plastic constant), in antithetic pairs `+d, -d`, so no pair shifts the pixel | shifted toroidally by the pixel's hash (Cranley-Patterson), or turned on the disk |
| `SOBOL` | the first two Sobol dimensions, a (0, m, 2)-net at a power of two, unpaired | the index shuffled and each coordinate Owen-scrambled by the pixel's hash, after Burley's hash-based nested uniform scrambling (JCGT 2020) |

A scrambled net is unbiased as it stands, and adding its reflection back breaks its
strata: paired, Sobol left 0.0009 RMS at 1024 points on a three-turn swirl of a checker,
unpaired 0.0006, so Sobol draws the net alone. Against a 48 x 48 grid on that swirl
(256 x 256, 8-pixel squares), RMS inside the swirl:

| Points | R2, box | Sobol, box | R2, Gaussian | Sobol, Gaussian |
|---|---|---|---|---|
| 16 | 0.059 | 0.046 | 0.074 | 0.071 |
| 64 | 0.017 | 0.014 | 0.027 | 0.025 |
| 256 | 0.0061 | 0.0030 | 0.0087 | 0.0074 |
| 1024 | 0.0020 | 0.0006 | 0.0024 | 0.0021 |

Under the box kernel Sobol's error falls faster, a third of R2's at 1024 points; under the
Gaussian, whose Box-Muller map bends the strata, the gain is small. A Sobol point costs
about twice an R2 one (the scrambling hashes), which still leaves Sobol ahead at equal time
under the box. `ADAPTIVE` with Sobol went from 0.0195 to 0.0150 there.

`kernel` sets the weight each sub-position takes, in output pixels around the centre:

| Kernel | Weight | Support |
|---|---|---|
| `BOX` | 1: the pixel's mean | the pixel |
| `TENT` | `(1 - abs(dx)) (1 - abs(dy))` | two pixels each way |
| `GAUSSIAN` | `exp(-r^2 / (2 s^2))`, `s = 0.5` | a disk of radius 1.5 |
| `MITCHELL` | Mitchell-Netravali `B = C = 1/3` per axis | two pixels each way |
| `LANCZOS` | `sinc(d) sinc(d / 2)` per axis (Lanczos-2) | two pixels each way |
| `BLACKMAN_HARRIS` | four-term Blackman-Harris window per axis, `t = d / 1.5` | 1.5 pixels each way |

The last three (2026-10-02) are separable. Mitchell and Lanczos dip below zero, which no
density can, so for them and for Blackman-Harris `QMC`, `ADAPTIVE` and `AUTO` spread their
points evenly over the support square and weigh each by the kernel, dividing by the
weights' sum; `GRID` keeps a cell whatever its weight's sign. Suite 296 holds `GRID` to the
kernels' definitions exactly, and the weighted `QMC` sums give a constant back to 1.2e-15 and
a ramp (R2's antithetic pairs) to 1.8e-15. They are sharper than the Gaussian, at the cost of
a little ringing for Mitchell and Lanczos; Blackman-Harris, the filter Cycles uses at its
default width, does not ring.

`GRID` lays its cells over the kernel's support and weights them; `QMC` draws its points
from the kernel through its inverse distribution (Box-Muller for the Gaussian, cut at
`3 s`), so every point weighs the same and R2's antithetic pairs still cancel. The box is
the pixel's exact area mean, but its flat, abrupt response lets detail finer than a pixel
fold back as moire even when the mean is computed exactly; the tent and the Gaussian
overlap the neighbours and fall off smoothly, trading a little softness for much less
folding. `ADAPTIVE` with a wider kernel keeps drawing it where the map is flat, since one
sample there would drop the kernel and leave the flat parts sharper than the rest.

The folding is measured against each kernel's own alias-free target: the same kernel on a
4x render (8 x 8 points a canvas pixel) brought down by `alwan_resize`'s Lanczos, so the
kernel's softness is on both sides and only what folds back counts. The same checker and
swirl, RMS inside the swirl, a 16 x 16 grid per kernel:

| Turns | `POINT` | `BOX` | `TENT` | `GAUSSIAN` | `ADAPTIVE`, `GAUSSIAN` |
|---|---|---|---|---|---|
| 0 | 0.077 | 0.037 | 0.034 | 0.032 | 0.034 |
| 0.5 | 0.096 | 0.054 | 0.043 | 0.039 | 0.042 |
| 1 | 0.146 | 0.065 | 0.050 | 0.043 | 0.046 |
| 2 | 0.233 | 0.079 | 0.050 | 0.040 | 0.044 |
| 3 | 0.270 | 0.074 | 0.045 | 0.037 | 0.041 |

At no swirl every kernel sits on a floor that the checker's own hard edges set; the box
climbs well above it as the swirl compresses the pattern, the Gaussian barely leaves it.
At three turns, adaptive with the Gaussian took 58 points a pixel on average and 0.74 s
against the grid's 2.5 s.

EWA's cost follows the footprint's area in the source, not a sample count, and it is exact
for an affine map, since one Jacobian then holds for the whole footprint. It is blind to a
map that curves inside the footprint, and its Gaussian reconstruction is softer than
bilinear where the map enlarges. `AUTO` gives each pixel the one that suits it. The
aliasing against the Gaussian kernel's alias-free target, on a checker of 6-pixel squares,
320 x 240:

| Map | `POINT` | `ADAPTIVE` | `GRID`, 16 | `EWA` | `AUTO` |
|---|---|---|---|---|---|
| affine, turned 23 degrees, shrunk 2.6 times | 0.080 | 0.022 | 0.020 | 0.020 | 0.020 |
| perspective floor to a horizon | 0.089 | 0.045 | 0.043 | 0.057 | 0.045 |
| swirl, three turns | 0.232 | 0.041 | 0.036 | 0.043 | 0.039 |

On the affine map EWA reaches the 16 x 16 grid in about a sixth of its time. On the floor
it loses where the floor enlarges toward the viewer, which `AUTO` leaves to `ADAPTIVE`; on
the swirl it misses the curvature, which `AUTO` also samples, and `AUTO` comes out ahead of
`ADAPTIVE` alone.

With `alpha_channel` set, the last of two or four channels is straight alpha: the colour is
premultiplied before the mean and divided after, so colour under zero alpha does not bleed
into an edge. The mean is of the data's own values; for light-correct integration of
display-encoded colour, convert to linear light first.

No library carries this integration to compare with; suite 235 holds its properties
instead: `POINT` through a callback or a field that encode a matrix equals the matrix path,
`GRID` with one sample equals `POINT`, antithetic R2 returns a linear ramp to 3e-16, R2
converges to the grid reference under a swirl (RMS 0.029, 0.0032, 0.0006 at 16, 256 and
2048 points) and Sobol faster (0.017, 0.0006, 0.0001), a seed repeats an image and another
seed changes it, `ADAPTIVE` takes one sample past the swirl and more inside it, a zero angle is
the identity, and a NaN texel is fill. For the tent and the Gaussian, a linear ramp comes
back to 1e-15 through the grid and through R2, one grid cell is the point, R2 converges to
the weighted grid (RMS 0.037 and 0.040 at 16 points, 0.0015 and 0.0019 at 1024), and
`ADAPTIVE` never takes fewer than 16 points. `EWA` keeps a constant to 4e-16, returns a
linear ramp at the pixel's source point to 2.2e-4 under an affine shrink, lands 0.0004 RMS
from the Gaussian kernel integrated by a 32 x 32 grid there, and averages a 400-times
shrink of a pixel checker to 0.5000; every pixel of `AUTO` on a perspective floor is, bit
for bit, `EWA`'s or `ADAPTIVE`'s. Resize's integration and subpixel layouts are held
the same way: an integrated box halving is the 2 x 2 mean, a constant stays constant under
every layout, and RGB and BGR move a thin line's red and blue in opposite directions.

## Template matching

```c
alwan_status alwan_match_template_{T}(alwan_{T} *out, size_t out_row_stride,
                                      alwan_{T} const *image, size_t image_row_stride,
                                      size_t width, size_t height,
                                      alwan_{T} const *templ, size_t templ_row_stride,
                                      size_t templ_width, size_t templ_height,
                                      alwan_template_params const *params);
alwan_status alwan_match_template_u8(alwan_f64 *out, ...);   /* unsigned char image and template, double out */
```

The normalised cross-correlation of a template at every placement over an image, the
correlation coefficient in [-1, 1], as scikit-image's `feature.match_template` (Lewis, "Fast
Normalized Cross-Correlation"). With `pad_input` 0, `out` is (width - templ_width + 1) x
(height - templ_height + 1) and each value belongs to the template's top-left corner at that
pixel; with `pad_input` set, `out` is width x height and each value belongs to the template's
centre ((size - 1) / 2). The best match is the largest value: `suwar_peak_local_max` (suwar) finds it,
and the next ones.

| `alwan_template_params` | Default | Meaning |
|---|---|---|
| `pad_input` | 0 | as above |
| `mode` | `CONSTANT` | how the image is padded: `CONSTANT`, `EDGE`, `SYMMETRIC`, `REFLECT`, `WRAP`, numpy.pad's |
| `constant_value` | 0 | `CONSTANT`'s value |
| `method` | `NCC` | scikit-image's coefficient, or one of OpenCV's six scores below |

### OpenCV's scores

`method` set to `SQDIFF`, `SQDIFF_NORMED`, `CCORR`, `CCORR_NORMED`, `CCOEFF` or
`CCOEFF_NORMED` gives `cv::matchTemplate`'s `TM_*` score of the same name instead, for the
template's top-left corner at each placement, `out` (width - templ_width + 1) x (height -
templ_height + 1), without padding (`pad_input` must be 0). The best match is the smallest
`SQDIFF*` value and the largest of the others. OpenCV's normalisation
(`common_matchTemplate`) is followed step for step in double: integral images of the image
and its square, the template's mean and deviation, the guard that scores 0 on a window
flatter than 10 FLT_EPSILON of its energy, the snap of a ratio just over 1 to +/-1, a flat
template scoring 1 everywhere for `CCOEFF_NORMED`. The correlation is exact here and rounded
to float32 where OpenCV keeps it; OpenCV computes it by a DFT, float32 for 8-bit images and
double for float32 ones, and the result is rounded to float32 as OpenCV's is. Suite 292
holds the six to `cv2.matchTemplate` on a direct-sized and an FFT-sized template: float32
images equal, 8-bit images within 2.2e-5 of the score's scale (OpenCV's float32 DFT). On a
nearly flat window `CCOEFF_NORMED`'s numerator is the difference of two large, nearly equal
sums, so one float32 step of the stored correlation moves the score by up to a few 1e-4.
On plate 158's frame (a 32 x 32 template, sums near 4e7, the worst window's deviation 0.6
code values) alwan differs from cv2's 8-bit result by 3.8e-4 and from cv2 run on the same
values as float32 (a double DFT) by 6.7e-5, while cv2's two paths differ from each other by
3.8e-4; 2,969 of the 158,497 scores differ from the 8-bit result by more than 1e-5.

As scikit-image, the image is padded by the template's size on every side whatever
`pad_input` says, so the mode matters at the edges in both. The window sums and sums of
squares are running sums down the columns and then along the rows, exactly as scikit-image
forms them; the cross-correlation comes from alwan's FFT on power-of-two sizes, or directly
when the template is small enough for that to cost less. A window whose variance term is
at most the precision's epsilon answers 0, so a flat template gives 0 everywhere. One
channel: for colour, match on luminance, or per channel and combine. u8 takes raw values and
writes doubles; f32 computes in double and rounds.

Suite 251 holds it to scikit-image on random images of odd and even sizes, with small
templates (direct) and large ones (FFT), `pad_input` off and on, every mode, templates as
large as the image, a template cut from the image (1 at its place, to 4e-16), a flat
template, a flat block in the image and u8 camera crops: within 6.4e-15 in f64 and 3.3e-13
in u8 (values to 255). The exception is a window lying on a flat patch: there the response
is rounding divided by a denominator of the same size, scikit-image's and alwan's both
within 1e-7 of 0 and not comparable closer. On float32-exact values the f32 entry point
gives the f64 response rounded.

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

## Background estimation

```c
typedef enum {
    ALWAN_BACKGROUND_ROLLING_BALL = 0
} alwan_background_method;

alwan_status alwan_background_{T}(alwan_{T} *out, size_t out_row_stride,
                                  alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                  size_t width, size_t height,
                                  alwan_background_method method,
                                  alwan_background_params const *params);
alwan_status alwan_background_u8(...);   /* same, unsigned char pixels */
```

The slowly varying background of an image, returned as an image: subtract it (or divide
by it) to take out uneven illumination, a scanned print or film frame lit unevenly, a copy
stand's vignetting, the shading across a photographed document. Each channel on its own;
`out` may be `src`.

| Field of `alwan_background_params` | 0 reads as |
|---|---|
| `radius` | 100: the ball's radius, in pixels and in the data's units alike |
| `ellipsoid_width`, `ellipsoid_height` | 0: use the ball; otherwise the ellipsoid's size in pixels |
| `ellipsoid_intensity` | the ellipsoid's semi-axis along the data, in the data's units (required with it) |

### `ROLLING_BALL`

A ball pushed up under the image, seen as a surface whose height is the value. At each
pixel the background is the height the top of the ball reaches when it is centred beneath
that pixel and touches the surface from below:
`min over the ball's offsets o of img(p + o) + k(0) - k(o)`, with `k(o) = sqrt(r^2 - |o|^2)`
and pixels outside the image not counted. Objects narrower than the ball keep it from
rising into them, so they are left out of the background. This is a grey erosion by the
ball's surface, which is scikit-image's `restoration.rolling_ball`; ImageJ's "Subtract
Background" goes on to dilate by the same ball (an opening), which follows the surface more
closely. On a slope `s` the ball's top sits `r (sqrt(1 + s^2) - 1)` below the surface.

Because the ball is round in pixels and in value at once, its radius is a length in both:
a radius of 50 on data in 0..1 is a flat disc. Scale the data to the ball, or use the
ellipsoid, whose `ellipsoid_intensity` sets its height apart from its footprint. The cost
is about `pi r^2` a pixel, as scikit-image's; shrink the image first for a large radius.
8-bit results are truncated, as scikit-image's cast back truncates.

Suite 218 holds it to scikit-image value for value in double and 8-bit (balls of radius 5
to 30 and 7.5, an image smaller than the ball, 8-bit colour), the ellipsoid to 1.4e-14 and
float32, which scikit-image computes in float32, to 3.8e-6 on values up to 255.

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

## Colour transfer

```c
typedef enum {
    ALWAN_COLOR_TRANSFER_HISTOGRAM_MATCH = 0,
    ALWAN_COLOR_TRANSFER_REINHARD2001 = 1,
    ALWAN_COLOR_TRANSFER_MKL = 2,
    ALWAN_COLOR_TRANSFER_IDT = 3,
    ALWAN_COLOR_TRANSFER_XIAO2006 = 4
} alwan_color_transfer_method;

typedef struct {
    double amount;             /* 0 reads as 1 */
    size_t iterations;         /* IDT, 0 reads as 20 */
    size_t bins;               /* IDT, 0 reads as 300 */
    uint64_t seed;             /* IDT rotation sequence */
    size_t regrain_iterations; /* 0 = no regrain */
    double regrain_smoothness; /* 0 reads as 1 */
    size_t width;              /* pixels a row, for the regrain */
} alwan_color_transfer_params;

alwan_status alwan_color_transfer_{T}(alwan_{T} *out, size_t out_stride,
                                      alwan_{T} const *src, size_t src_stride, size_t src_count,
                                      alwan_{T} const *ref, size_t ref_stride, size_t ref_count,
                                      size_t channels, alwan_color_transfer_method method,
                                      alwan_color_transfer_params const *params);
```

The look of a reference image carried onto a source. The two need not be the same size,
and pixels are passed as counts, since no method looks at neighbours (only the optional
regrain does, and it takes the row width in `params.width`). `out` may be
`src`. `params.amount` blends the result with the source,
`source + amount (transfer - source)`; 0 (or NULL params) reads as 1, the full transfer.
Zero the whole params struct (`= { 0 }` or memset) before setting the fields you want: every
field reads 0 as its default, and since 3.0.0 the struct has more fields than `amount`, so
one left uninitialised (stack garbage in `regrain_iterations`, say) is no longer harmless.

### `HISTOGRAM_MATCH`

Each of 1 to 4 channels is remapped so that its cumulative distribution matches the
reference's: every distinct source value's quantile, `cumsum(counts) / n`, is looked up
in the reference's quantiles, as scikit-image's `exposure.match_histograms` does (its float
path, `numpy.interp` branch for branch). Channels are matched independently, so in RGB the
channels drift apart; matching in a decorrelated space (Oklab, CIELAB) carries a look more
gently. Suite 191 is bit for bit against scikit-image: all three channels, one alone, and a
single-value reference.

### `REINHARD2001`

Reinhard, Ashikhmin, Gooch and Shirley (IEEE CG&A 2001): the mean and standard deviation of
each channel are matched to the reference's in Ruderman's l alpha beta space, a log LMS
space whose channels are nearly decorrelated for natural images, so a per-channel shift and
scale moves the look without the cross-talk the same operation causes in RGB. Three
channels of linear RGB. It uses the paper's RGB to LMS matrix and the exact inverse of it
(the paper prints the inverse rounded), floors LMS at 1e-6 before the logarithm, and does
not clamp. Compared with `HISTOGRAM_MATCH` it carries two moments where that carries the
whole distribution, and so keeps the source's own shape.

No implementation of the paper's own space exists to compare with; suite 193 checks the
property that defines it: the result's l alpha beta means and standard deviations equal the
reference's, to 3e-15, and an image transferred onto itself comes back unchanged.

### `MKL`

Pitie and Kokaram's linear Monge-Kantorovich mapping (IET CVMP 2007). Of all the affine
maps that carry a Gaussian with the source's mean and covariance onto one with the
reference's, it is the one that moves colours least on average, the optimal transport
between the two Gaussians:

```
T   = Sr^-1/2 (Sr^1/2 Sz Sr^1/2)^1/2 Sr^-1/2
out = T (x - mean_src) + mean_ref
```

with `Sr`, `Sz` the sample covariances (divided by n - 1). Where `REINHARD2001` matches
each channel's spread in a space chosen to decorrelate them, this matches the whole
covariance in the space it is given, cross-channel terms included, so the correlations of
the reference (warm highlights with cool shadows, say) arrive with its spread. It takes 1
to 4 channels and at least two pixels in each image, and does not clamp. One channel is a
shift and a scale by the ratio of standard deviations.

The square roots come from a cyclic Jacobi eigen decomposition. As in the authors' MATLAB
code, negative eigenvalues are zeroed and the inverse square root is `1 / (sqrt(l) + eps)`.
`T` is unique when the source's covariance is not singular. When it is (a grey image, a
constant channel) the authors' code divides by `eps` and its result carries rounding noise
magnified by about 1e31; alwan takes the pseudo-inverse instead, an eigenvalue under 1e-12
of the largest counting as zero, so the source moves only within the directions it spans.

Suite 209 compares with color-matcher's `mkl` (C. Hahne's Python port of the authors'
code, GPL-3.0, used as an oracle only) on six pairs: display-rendered SRIC frames of
different sizes, linear light running past 1, and 4- and 2-channel pairs, to 5.5e-14
relative. It also checks the property that defines the method, that the result's mean and
covariance are the reference's (to 2.3e-15), and the identity, one-channel, grey-source,
float32 and blend cases.

`MKL` is Pitie and Kokaram's linear Monge-Kantorovich mapping; there is no second method for
it.

### `IDT`

Pitie, Kokaram and Dahyot's iterative distribution transfer (ICCV 2005; CVIU 2007), alwan's
own code from the papers. Where `MKL` matches two moments of the joint distribution, IDT
moves the whole of it: each iteration rotates the 1 to 4 channel space, matches the source
to the reference along every rotated axis with a 1D CDF transfer, and rotates back. The
marginals along more and more directions agree, and with them the joint distribution.

The 1D transfer, per axis:

- `lo`, `hi`: the smallest and largest projection of either image; an axis on which
  `hi <= lo` moves nothing.
- `bins` edges `e_k = lo + k step`, `step = (hi - lo) / (bins - 1)`. A value's bin is the
  largest `k` with `e_k <= v` (a binary search), at most `bins - 1`.
- `C_k`: the running sum of `count_k + 1e-6`, divided by its last value. The small constant
  keeps `C` strictly increasing, so its inverse exists everywhere.
- `F_k = lo + f step`, where `f` is the position at which the reference's `C` reaches the
  source's `C_k`, linear between bins; 0 at or below the reference's first value, `bins - 1`
  at or above its last.
- `v' = F_k + ((v - e_k) / step) (F_{k+1} - F_k)`, `k` the bin of `v` at most `bins - 2`.

Each pixel then moves by `R' (v' - v)` over the axes, with `R` the rotation (its rows are
the axes). The rotations are a fixed, documented sequence: iteration 0 is the identity, so
one iteration is a per-channel histogram transfer; iteration `i > 0` draws `n x n` values
from splitmix64 (state = `seed`, value `(x >> 11) 2^-53 2 - 1`, row major) and makes the
rows orthonormal by modified Gram-Schmidt, redrawing a row whose norm falls under 1e-6. The
papers draw random rotations and do not fix them; a fixed sequence makes the result
repeatable and lets a test reproduce it. Defaults: 20 iterations, 300 bins, seed 0. Not
clamped.

color-matcher (GPL) implements IDT with numpy's random rotations and its own histogram
details, so it cannot reproduce alwan's result and is not used. Suite 299 compares with a
numpy transcription of the rules above, driven by the same rotations: bit for bit in f64
over ten cases of 1 to 4 channels, bin counts from 40 to 300, several seeds and iteration
counts, with and without the regrain; f32 differs from f64 on the same float inputs by
2.5e-8. It also checks the property that defines the method: on 16 fixed projections the
distance between the result's and the reference's distributions is 0.256 untransferred,
0.0089 after one iteration, 0.0038 after ten and 0.0030 after twenty, never growing.

### `XIAO2006`

Xiao and Ma, "Color transfer in correlated color space" (VRCIA 2006). The source's colour
ellipsoid is translated, rotated and scaled onto the reference's in the space it is given
(RGB in the paper), with no decorrelating space in between:

```
out = m_r + U_r S_r S_s^-1 U_s' (x - m_s),   cov = U L U',   S = sqrt(L)
```

The paper takes `U` and `L` from an SVD, which leaves each eigenvector's sign and the order
of equal eigenvalues open, and the result depends on both. Here the eigenvalues are sorted
largest first and each eigenvector is signed so its component of largest magnitude is
positive (the first such on a tie). Sample covariances (n - 1); a source eigenvalue under
1e-12 of the largest scales by 0. Unlike `MKL` this is not the least-motion map: it lines up
the principal axes by their order, which can turn colours where `MKL` would not. 1 to 4
channels, two pixels at least, not clamped. Suite 299 compares with numpy's `eigh` under the
same order and sign rule, to 1.2e-15.

### Regrain

Any method's result can be regrained, Pitie, Kokaram and Dahyot's step (CVIU 2007) against
the grain a transfer leaves when it stretches the source's small variations. Set
`regrain_iterations` (0 is off) and `width` (pixels a row; `src_count` must be a multiple).
It looks for `J` close to the transfer `t` where the source `I` has detail and with the
source's gradients where it is flat, by Jacobi sweeps from `J = t`:

```
J'(p) = (psi_p t(p) + sum_q phi_pq (J(q) - I(q) + I(p))) / (psi_p + sum_q phi_pq)
```

over the four neighbours `q` (left, right, up, down; past the edge the neighbour is `p`),
`phi_pq = (phi_p + phi_q) / 2`, with `g` the source's gradient magnitude (central
differences over every channel), `psi_p = min(1, 51.2 g)` and `phi_p = 30 / (1 + 10 g /
smoothness)`. These are the weights Pitie et al. use for images in [0, 1]; for other ranges
scale `regrain_smoothness`, or the image. One scale only. Suite 299 checks the sweeps to the
bit and that a regrain moves the transfer by a little (0.021 on average for 25 sweeps on its
test pair).

## Sharpening

```c
typedef enum {
    ALWAN_SHARPEN_UNSHARP_MASK = 0,
    ALWAN_SHARPEN_UNSHARP_MASK_BOX = 1,
    ALWAN_SHARPEN_CAS = 2
} alwan_sharpen_method;

alwan_status alwan_sharpen_{T}(alwan_{T} *out, size_t out_row_stride,
                               alwan_{T} const *src, size_t src_row_stride, size_t channels,
                               size_t width, size_t height,
                               alwan_sharpen_method method, alwan_sharpen_params const *params);
alwan_status alwan_sharpen_u8(...);   /* same, unsigned char pixels */
```

The detail of an image amplified. 1 to 4 channels, each sharpened on its own; `out` may be
`src`. Sharpening R, G and B apart can fringe colour at edges; sharpen a lightness channel
for a gentler result. The float entry points run `UNSHARP_MASK` and `CAS`, and
`alwan_sharpen_u8` runs `UNSHARP_MASK_BOX`; each refuses the others, since each follows a
reference that works in that type.

| Field of `alwan_sharpen_params` | Method | 0 reads as |
|---|---|---|
| `radius` | `UNSHARP_MASK` / `UNSHARP_MASK_BOX` | 1 / 2, the Gaussian's standard deviation in pixels |
| `amount` | `UNSHARP_MASK` / `UNSHARP_MASK_BOX` | 1 / 1.5 (it may be negative, which softens) |
| `clip` | `UNSHARP_MASK` | 0: no clipping; non-zero clips as scikit-image does |
| `threshold` | `UNSHARP_MASK_BOX` | 3 levels; a negative value is 0, every difference |
| `cas_sharpness` | `CAS` | 0, the shader's default (least ringing); 1 is the most |
| `cas_better_diagonals` | `CAS` | 0: the cross of four neighbours only |
| `cas_per_channel` | `CAS` | 0: green's weight for all three channels |
| `cas_approximate` | `CAS` | 0: exact reciprocals and square root |

### `UNSHARP_MASK`

`out = in + amount (in - G * in)`: the detail a Gaussian of standard deviation `radius`
removes is added back `amount` times. It follows scikit-image's `filters.unsharp_mask`:
each channel alone, the Gaussian of `scipy.ndimage.gaussian_filter` in mode `reflect`
(half-sample symmetric), truncated at `int(4 radius + 0.5)` taps a side. scikit-image clips
the result to [0, 1], or [-1, 1] when the image has a negative value, unless
`preserve_range` is set; alwan clips only when `clip` asks, as it clips nowhere else
silently. Suite 203 agrees with scikit-image to 9e-16 over seven cases, clipped and not.

scikit-image 0.26's `unsharp_mask` mishandles `channel_axis=-1`: it passes the axis to
`slice_at_axis` without taking it modulo the dimension count, so it sharpens rows 0, 1 and 2
in turn instead of the three channels. Suite 203's reference passes `channel_axis=2`.

### `UNSHARP_MASK_BOX`

The unsharp mask as Pillow's `ImageFilter.UnsharpMask(radius, percent, threshold)`
computes it, on 8-bit data, with the defaults Pillow's are (radius 2, 150 %, threshold 3).
Two things set it apart from `UNSHARP_MASK`.

The blur is not a sampled Gaussian but Gwosdek et al.'s extended box blur (SSVM 2011):
three passes along the rows and three down the columns of a box whose radius `l + a` has a
whole part `l` and a fractional weight `a` for the two end pixels, chosen so the three
passes have the Gaussian's variance, `radius^2`. It costs the same at any radius. Each pass
weighs in 24-bit fixed point, replicates the border and rounds back to 8 bits, as Pillow's
does.

A difference between a pixel and its blur of at most `threshold` levels is left alone; a
larger one is added back `amount` times (`percent / 100` in integer arithmetic, truncated
toward zero) and clamped to 0..255. The threshold keeps noise, film grain and smooth
gradients from being sharpened with the edges; photographers' unsharp-mask dialogs have
it for that.

Suite 212 is equal to Pillow 12.0, value for value, on grey and colour images at radii 0.3
to 12, 50 to 300 %, thresholds 0 to 10, and on an image narrower than the box, which takes
the blur's other branch.

### `CAS`

AMD FidelityFX Contrast Adaptive Sharpening (1.20190610, MIT, the notice in
`api/alwan_sharpen.c`), its sharpen-only `CasFilter`. Each pixel's four neighbours get the
weight `w = amp x peak`, `peak = -1 / lerp(8, 5, cas_sharpness)`, with `amp` the square root
of the window's distance to the signal's limits over its maximum,
`sat(min(mn, 1 - mx) / mx)`, and the result is `(w (b + d + f + h) + e) / (1 + 4 w)`. Where
the window already spans most of the range, `amp` falls and the pixel is sharpened less, so
edges do not ring and dark or bright detail does not clip; low-contrast texture gets the
most. `cas_better_diagonals` adds the full 3 x 3 window's extremes to the cross's
(`CAS_BETTER_DIAGONALS`), `cas_per_channel` weighs each channel by its own `amp`
(`CAS_SLOW`) instead of green's, and `cas_approximate` uses the shader's bit-level
reciprocal and square root (`APrxLoRcpF1`, `APrxLoSqrtF1`, `APrxMedRcpF1`, its default)
in place of exact ones (`CAS_GO_SLOWER`). Values are in 0..1, 1 the display's peak, and
come back clamped to 0..1; 1, 3 or 4 channels, the fourth copied; the border its nearest
pixel. It runs in float, as the shader does. Suite 298 is equal, on every value of 36 cases,
to `CasFilter` transcribed in numpy float32 from `ffx_cas.h` (exact and approximate,
cross and full window, shared and per-channel weights).

## Haze removal

```c
typedef struct {
    size_t patch_radius;      /* 7: the paper's 15 x 15 */
    alwan_f64 omega;          /* 0.95: the haze kept for depth */
    alwan_f64 t0;             /* 0.1: the transmission floor */
    alwan_f64 top_fraction;   /* 0.001: of the dark channel searched for the airlight */
    size_t guide_radius;      /* 30; 0 skips the refinement */
    alwan_f64 guide_eps;      /* 1e-3 */
} alwan_dehaze_params;

typedef enum {
    ALWAN_DEHAZE_DARK_CHANNEL = 0
} alwan_dehaze_method;

void alwan_dehaze_params_init(alwan_dehaze_params *params);
alwan_status alwan_dehaze_{T}(alwan_{T} *out, size_t out_row_stride,
                              alwan_{T} *transmission_out, size_t t_row_stride,
                              alwan_{T} airlight_out[3],
                              alwan_{T} const *rgb, size_t row_stride,
                              size_t width, size_t height,
                              alwan_dehaze_method method, alwan_dehaze_params const *params);
```

The parameters use an initialiser rather than zero defaults, since a zero guide radius is
meaningful (it skips the refinement); NULL is `alwan_dehaze_params_init`'s values.

### `DARK_CHANNEL`

He, Sun and Tang (CVPR 2009, TPAMI 2011) model a hazy image as `I = J t + A (1 - t)` and
observe that in most patches of a clear outdoor image some channel is near zero. So the
patch minimum of `I / A` measures the haze: the airlight `A` is taken among the brightest
0.1 % of the dark channel (the pixel with the largest channel mean), the transmission is
`t = 1 - omega min_patch min_c I_c / A_c`, refined here by the guided filter (`alwan_edge_filter`, `ALWAN_EDGE_FILTER_GUIDED`) with the
image as the colour guide (the authors' own replacement for soft matting), and the scene
is `J = (I - A) / max(t, t0) + A`. Work on linear light; scale the radii with the image.

No library ships the whole method, so suite 192 composes one from oracle primitives:
scipy's `minimum_filter` for both patch minima, the paper's airlight rule (the top 0.1 %
of the dark channel, then the highest channel mean), and OpenCV's `guidedFilter` with the
hazy image as the colour guide. On a hazed photograph at three settings alwan picks the
same airlight pixel, and its `t` and `J` agree to OpenCV's float32 rounding (1.0e-7 and
1.9e-7); with the refinement off they agree exactly.

The suite also makes haze with a
known answer: a scene that satisfies the prior, hazed with a known `A` and `t`. The
airlight comes back within 0.017 (the sky it is read from is 2 % scene), the transmission
to a median error of 0.029, the scene to a mean error of 0.032 (omega keeps 5 % of the
haze on purpose). A clear image with a white patch changes by 0.0005 on average; one with
no bright neutral at all has no airlight to find and returns `ALWAN_E_RANGE`.

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

## The colour cube: a 3D histogram

```c
alwan_status alwan_histogram3d_{T}(unsigned int *counts_out, size_t bins,
                                   alwan_{T} const *rgb, size_t stride, size_t count,
                                   alwan_{T} const lo[3], alwan_{T} const hi[3]);
```

How many pixels fall in each cell of a `bins x bins x bins` lattice over `[lo, hi]`, the
data a colour-cube or point-cloud view of an image is drawn from, stored as
`counts_out[(r * bins + g) * bins + b]`. It bins as `numpy.histogramdd` does: the edges of
each axis are `lo + i (hi - lo) / bins` with the last exactly `hi`, a value's cell is the
last edge at or below it, a value equal to `hi` counts in the last cell, and a pixel with a
channel outside `[lo, hi]` or NaN is not counted. `bins` runs from 1 to 1024; the caller
holds `bins^3` counts. Suite 189 matches `numpy.histogramdd` cell for cell over five
binnings, with values on edges, on both bounds, outside every range, and a NaN.
