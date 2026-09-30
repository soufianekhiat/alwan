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
    ALWAN_EDGE_FILTER_L0_SMOOTH = 6
} alwan_edge_filter_method;

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
`src` has 1 to 4 channels and the guide 1 to 4 (1 or 3 for `GUIDED`); `out` has `src`'s
layout and may be `src`.

| Field of `alwan_edge_filter_params_{T}` | Methods | 0 reads as |
|---|---|---|
| `radius` | `GUIDED`, `JOINT_BILATERAL`, `ROLLING_GUIDANCE` | 4 for `GUIDED`, `round(1.5 sigma_space)` for the others |
| `eps` | `GUIDED` | 0.01 |
| `sigma_color` | `JOINT_BILATERAL`, `ROLLING_GUIDANCE` / `DOMAIN_TRANSFORM_*` / `FAST_GLOBAL_SMOOTHER` | 0.1 / 0.4 / 0.03 |
| `sigma_space` | `JOINT_BILATERAL`, `ROLLING_GUIDANCE` / `DOMAIN_TRANSFORM_*` | 3 / 60 |
| `lambda` | `FAST_GLOBAL_SMOOTHER` / `L0_SMOOTH` | 900 / 0.02 |
| `lambda_attenuation` | `FAST_GLOBAL_SMOOTHER` | 0.25 |
| `iterations` | `ROLLING_GUIDANCE` / `DOMAIN_TRANSFORM_*`, `FAST_GLOBAL_SMOOTHER` | 4 / 3 |
| `start_from_source` | `ROLLING_GUIDANCE` | 0: start from the source's Gaussian (the paper) |
| `kappa` | `L0_SMOOTH` | 2 (above 1) |

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
typedef enum { ALWAN_REGISTER_PHASE_CORRELATION = 0 } alwan_register_method;
typedef enum { ALWAN_REGISTER_NORMALIZE_PHASE = 0, ALWAN_REGISTER_NORMALIZE_NONE = 1 } alwan_register_normalization;
typedef struct { size_t upsample_factor; alwan_register_normalization normalization; } alwan_register_params;
typedef struct { double shift[2]; double error; double phasediff; } alwan_register_result;

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
    ALWAN_DENOISE_NL_MEANS_BUADES = 8
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
one portrait). `alwan_denoise_u8` runs all nine; `alwan_denoise_{T}` runs every method
but `NL_MEANS` and `ANISOTROPIC_DIFFUSION`, and returns `ALWAN_E_INVALID` for those two,
whose references work on 8-bit data. 1 to 4 channels; `out` may alias `src` except for `NL_MEANS`.

| Field of `alwan_denoise_params` | Method | 0 reads as |
|---|---|---|
| `weight` | `TV_CHAMBOLLE` | 0.1 |
| `tolerance` | `TV_CHAMBOLLE` | 2e-4 (a tiny value runs every iteration) |
| `iterations` | `TV_CHAMBOLLE` / `ANISOTROPIC_DIFFUSION` | 200 (at most) / 10 |
| `h` | `NL_MEANS` / `NL_MEANS_DARBON`, `_BUADES` | 10, in 0..255 units / 0.1 in the data's units (25.5 on 8-bit) |
| `template_window`, `search_window` | `NL_MEANS` / `NL_MEANS_DARBON`, `_BUADES` | 7, 21 / 7, 23 |
| `alpha`, `k` | `ANISOTROPIC_DIFFUSION` | 0.15, 0.05 |
| `sigma` | `DCT` / `WAVELET` / `NL_MEANS_DARBON`, `_BUADES` | 10 for 8-bit data, 10 / 255 for floats / estimated from the image / 0 |
| `block_size` | `DCT` | 16 |
| `wavelet` | `WAVELET` | `ALWAN_WAVELET_DB1` (Haar) |
| `wavelet_levels` | `WAVELET` | the most the image holds, less 3, at least 1 |
| `wavelet_visushrink`, `wavelet_hard` | `WAVELET` | BayesShrink, soft |
| `kernel_size` | `MEDIAN` | 3 (odd, 3 to 255) |
| `weight`, `tolerance`, `iterations` | `TV_BREGMAN` | 5 (fidelity: larger keeps more), 1e-3, 100 |
| `anisotropic` | `TV_BREGMAN` | 0: the isotropic `|grad u|` |

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

### `WAVELET`

Wavelet shrinkage. Each channel goes through a multilevel 2D orthogonal wavelet transform,
every detail sub-band is thresholded, and the transform is inverted. Noise spreads evenly
over the detail coefficients while an image concentrates in a few large ones, so shrinking
the small ones toward zero removes noise and keeps edges. `wavelet` picks one of the
Daubechies `ALWAN_WAVELET_DB1` (Haar) to `DB8` or the symlets `SYM2` to `SYM8`; longer
filters give smoother results and ring more near edges, Haar leaves blocks.

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
    ALWAN_INPAINT_BIHARMONIC = 0
} alwan_inpaint_method;

alwan_status alwan_inpaint_{T}(alwan_{T} *out, size_t out_row_stride,
                               alwan_{T} const *src, size_t src_row_stride, size_t channels,
                               size_t width, size_t height,
                               unsigned char const *mask, size_t mask_row_stride,
                               alwan_inpaint_method method, alwan_inpaint_params const *params);
```

The pixels a mask marks (non-zero bytes, one per pixel) filled from their surroundings: a
dust spot, a scratch, a hot pixel cluster, a wire or a date stamp to remove. The values
`src` holds under the mask are never read. 1 to 4 channels; `out` may be `src`.

| Field of `alwan_inpaint_params` | Method | 0 reads as |
|---|---|---|
| `unclipped` | `BIHARMONIC` | 0: clip each channel to its known pixels' range, as scikit-image |

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
    ALWAN_STYLIZE_PENCIL_SKETCH_COLOR = 5
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

`alwan_stylize_params` holds `sigma_s` (the spatial extent, in pixels), `sigma_r` (the range
extent, in [0, 1] colour units) and `shade_factor` (the pencil drawing's darkness, 0.02).
A zero field reads as the method's default above; the values go to float, as OpenCV takes
them.

Images are 8-bit, channels 3 (RGB) or 4 (RGBA, the fourth copied through); the grey sketch
writes one channel. Row strides are in bytes, and out may be src. OpenCV computes in BGR,
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

Suite 274 holds it to cv2 5.0.0 with IPP off on procedural images, eight sizes from 2 x 2
(one with flat black rows and columns that take the path above), every method at its
defaults and at other parameters: all 101,312 bytes equal, in the ordinary and the
deterministic build. On the 166 SRIC photographs at a twelfth of their size (41 million
bytes, six methods) every byte equals cv2's with IPP off; with IPP on, as the pip wheel
ships, one byte is one level off.

## Morphology

```c
typedef enum {
    ALWAN_MORPHOLOGY_ERODE = 0, ALWAN_MORPHOLOGY_DILATE = 1,
    ALWAN_MORPHOLOGY_OPEN = 2, ALWAN_MORPHOLOGY_CLOSE = 3,
    ALWAN_MORPHOLOGY_GRADIENT = 4,
    ALWAN_MORPHOLOGY_TOP_HAT = 5, ALWAN_MORPHOLOGY_BLACK_HAT = 6,
    ALWAN_MORPHOLOGY_AREA_OPEN = 7, ALWAN_MORPHOLOGY_AREA_CLOSE = 8,
    ALWAN_MORPHOLOGY_DIAMETER_OPEN = 9, ALWAN_MORPHOLOGY_DIAMETER_CLOSE = 10,
    ALWAN_MORPHOLOGY_FILL_HOLES = 11,
    ALWAN_MORPHOLOGY_SKELETONIZE = 12, ALWAN_MORPHOLOGY_THIN = 13,
    ALWAN_MORPHOLOGY_H_MAXIMA = 14, ALWAN_MORPHOLOGY_H_MINIMA = 15,
    ALWAN_MORPHOLOGY_LOCAL_MAXIMA = 16, ALWAN_MORPHOLOGY_LOCAL_MINIMA = 17
} alwan_morphology_method;

typedef enum {
    ALWAN_MORPHOLOGY_RECT = 0, ALWAN_MORPHOLOGY_CROSS = 1,
    ALWAN_MORPHOLOGY_ELLIPSE = 2, ALWAN_MORPHOLOGY_DIAMOND = 3
} alwan_morphology_shape;

alwan_status alwan_morphology_{T}(alwan_{T} *out, size_t out_row_stride,
                                  alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                  size_t width, size_t height,
                                  alwan_morphology_method method,
                                  alwan_morphology_params const *params);
alwan_status alwan_morphology_u8(...);   /* same, unsigned char pixels */
```

Grey-level erosion (the minimum over a structuring element) and dilation (the maximum),
and what is built from them: opening (erode, then dilate) removes bright specks smaller
than the element and leaves larger shapes as they were; closing (dilate, then erode) fills
dark holes and gaps smaller than it; the gradient (dilate minus erode) outlines every edge;
the top-hat (the image minus its opening) and black-hat (its closing minus the image) keep
only the bright or dark detail smaller than the element. The everyday use beside this
library's keyers and selections is cleaning a mask: an opening then a closing takes the
speckle off a hard key and closes its pinholes. Each channel on its own; `out` may be
`src`.

| Field of `alwan_morphology_params` | 0 reads as |
|---|---|
| `shape` | `RECT` |
| `kernel_width`, `kernel_height` | 3, 3 (1 to 255) |
| `iterations` | 1: each erosion and dilation runs once; for `THIN`, until nothing changes |
| `kernel` | NULL: build `shape`; otherwise the caller's element, `kernel_width x kernel_height` bytes |
| `area_threshold` | 64: `AREA_OPEN` and `AREA_CLOSE`, the smallest region kept, in pixels |
| `connectivity` | 4: `AREA_*`, `DIAMETER_*` and `FILL_HOLES`, 4 or 8 neighbours |
| `diameter_threshold` | 8: `DIAMETER_OPEN` and `DIAMETER_CLOSE`, the shortest region kept, as the longer side of its bounding box |
| `h` | required for `H_MAXIMA` and `H_MINIMA`: 0 is refused, as scikit-image refuses it |
| `exclude_borders` | 0: `LOCAL_MAXIMA` and `LOCAL_MINIMA` may mark a plateau touching the edge |

The element's anchor is `(kernel_width / 2, kernel_height / 2)`, off centre for an even
size, and the element is used as given, not reflected, for dilation as for erosion. Pixels
outside the image take no part. The shapes are OpenCV's `getStructuringElement`, the
ellipse rasterised row by row as OpenCV rounds it. This is OpenCV's `erode`, `dilate` and
`morphologyEx` with their default border, and suite 217 holds it to them value for value:
all seven operations, the four shapes at odd and even sizes, up to three iterations and an
asymmetric caller's element, on 8-bit grey, colour and a speckled mask and on float32. The
8-bit composites saturate at 0 and 255 as OpenCV's do. A caller's element that reaches no
pixel of the image leaves that pixel as it was, where OpenCV writes its border value; the
four shapes always include the anchor.

### `AREA_OPEN`, `AREA_CLOSE`

The element-based opening removes what is narrower than its element, so a long thin bright
line and a small round speck of the same pixel count are treated apart. The area operators
count pixels instead and take no element. A region is a connected set of pixels at or above
some level (4- or 8-connected); `AREA_OPEN` lowers every pixel to the highest level at
which its region still has `area_threshold` pixels or more, so a bright region of fewer
pixels sinks to its surroundings whatever its shape, and every region at least that large,
thin or not, keeps its value and its outline exactly. `AREA_CLOSE` does the same for dark
regions: it fills holes of fewer pixels. On a key it removes specks and pinholes below a
size without rounding the corners of what it keeps.

The regions come from the image's max-tree (Berger et al. 2007), built by union-find over
the pixels in decreasing order, so the cost is a sort and near-linear after it. This is
scikit-image's `morphology.area_opening` and `area_closing`, and suite 219 holds it to them
value for value: thresholds of 16 to 500, both connectivities, a frame at whole levels and
at 12 levels where ties are everywhere, an unrounded frame, float32 and 8-bit colour. Two
differences, both outside what the suite compares: a threshold above the whole image's area
flattens it to its minimum (maximum for `AREA_CLOSE`), where scikit-image returns 0; and
scikit-image closes a float image as `1 - opening(1 - x)`, which gives back `x` only to
rounding, where alwan negates, which is exact. Each channel is its own image, where
scikit-image would read a colour array as a volume.

### `DIAMETER_OPEN`, `DIAMETER_CLOSE`

The same tree, with each region judged by the longer side of its bounding box,
`max(width, height)` in pixels, against `diameter_threshold`. A one-pixel line as long as the
threshold survives, where the area operators would need the threshold's worth of pixels:
scratches, hairs and wires stay while dots and specks of the same pixel count go. Suite 220
holds them to scikit-image's `diameter_opening` and `diameter_closing` value for value on
the same kinds of image as suite 219, with the same two differences.

### `FILL_HOLES`

Every dark region the border cannot reach without climbing is raised to the lowest level
that lets it drain: each pixel becomes the least, over paths to the border, of the highest
value on the path. On a mask this fills every enclosed hole whatever its size; on a grey
image it fills pits and basins to their rims, and subtracting the image from the result
leaves only what was enclosed. The connectivity is that of the paths, so a ring whose wall
touches itself only at a corner holds 4-connected and drains 8-connected. It is computed by
a priority flood from the border pixels, and it is the reconstruction by erosion of the
image from a seed equal to the image on the border and its maximum inside, which is
scikit-image's own recipe; suite 220 holds it to `morphology.reconstruction` that way, value
for value. An image one or two pixels across is all border and comes back unchanged.

### `SKELETONIZE` and `THIN`

Each shape worn down to a line one pixel wide along its middle, keeping how it connects:
the centre line of a stroke, a road, a vessel, a crack. Each channel is a mask, non-zero
the shape; a pixel removed becomes 0 and a pixel kept keeps its value, so a labelled or
coloured mask keeps its labels along the skeleton. Both remove border pixels in two
alternating passes, each pass deciding from the image as it stood when the pass began, and
repeat until a pass removes nothing.

| Method | Rule | As |
|---|---|---|
| `SKELETONIZE` | Zhang and Suen 1984: each 8-neighbourhood classified by a 256-entry table, one class removed in the first pass, another in the second, a third in both | `skimage.morphology.skeletonize` (2D) |
| `THIN` | Guo and Hall 1989: the paper's conditions G1, G2 and G3 (G3' in the second pass), which leave diagonal strokes one pixel thin; `iterations` stops it early | `skimage.morphology.thin` |

The table is scikit-image's own, transcribed from its `_skeletonize_various_cy.pyx` at
v0.26.0 (BSD); Guo and Hall's two tables are built from the conditions as scikit-image's
generator builds them. Suite 237 holds both pixel for pixel on five masks (blobs with holes,
block letters, a disc and a ring, a diagonal band, shapes on the image's border) and `THIN`
stopped after one and three passes, in f64, f32, 8 bits and as one channel of three.

### `H_MAXIMA`, `H_MINIMA`

A maximum's dynamic is how far one has to go down from it before reaching somewhere higher.
`H_MAXIMA` marks with 1 every pixel of a maximum whose dynamic is `h` or more, and 0
elsewhere: the peaks that stand out by at least `h`, however wide, with noise ripples below
`h` ignored. It is the image less its reconstruction by dilation from the image lowered by
`h`, compared with `h` (Soille 2003), which is how scikit-image's `h_maxima` computes it;
`H_MINIMA` is the same for minima, by erosion from the image raised by `h`. The element is the
reconstruction's footprint (below), 3 x 3 `RECT` by default.

The shifted image is built in the data's own arithmetic, as scikit-image builds it: in float32
for float32, with `h` rounded to float32 as NumPy rounds a Python float against a float32
array; minus (plus) scikit-image's allowance `2 * finfo.resolution * |x|` for floats, so a
maximum of dynamic exactly `h` is not lost to rounding; by a saturating subtraction (addition)
for 8-bit data with a whole `h`. A fractional `h` takes 8-bit data to float64, as scikit-image
does. An `h` greater than a channel's range marks nothing in it. Suite 250 holds both to
scikit-image pixel for pixel: smooth fields, integer terrain, plateaus and isolated peaks, at
three dynamics each, with the square and the cross, in f64, f32 and 8 bits.

### `LOCAL_MAXIMA`, `LOCAL_MINIMA`

Every plateau (a set of equal pixels joined through the element) whose neighbours outside it
are all lower (higher), marked 1; everything else 0. A single pixel is a plateau of one, and a
flat top of any size and shape is found whole. The element must be 3 x 3: the default `RECT`
is scikit-image's full connectivity, `CROSS` its 4-connectivity. With `exclude_borders` 0 a
plateau may touch the edge, except one at the channel's lowest (highest) value, which
scikit-image's padding with the minimum makes equal to the outside; with `exclude_borders` set
a plateau touching the edge is never marked, and an image under 3 pixels a side has none.
This is scikit-image's `local_maxima` and `local_minima` (on 8-bit data `local_minima` inverts
as `255 - x`, which orders pixels as negation does), pixel for pixel in suite 250, including
a constant image (nothing) and a two-row one.

### Reconstruction: `alwan_reconstruct_{T}`

```c
typedef enum {
    ALWAN_RECONSTRUCT_DILATION = 0,
    ALWAN_RECONSTRUCT_EROSION = 1
} alwan_reconstruct_method;

alwan_status alwan_reconstruct_{T}(alwan_{T} *out, size_t out_row_stride,
                                   alwan_{T} const *seed, size_t seed_row_stride,
                                   alwan_{T} const *mask, size_t mask_row_stride,
                                   size_t channels, size_t width, size_t height,
                                   alwan_reconstruct_method method,
                                   alwan_morphology_params const *params);
alwan_status alwan_reconstruct_u8(...);   /* same, unsigned char pixels */
```

Grey-level reconstruction: the seed dilated through the element over and over, never above
the mask, until nothing changes (`DILATION`), or eroded and never below it (`EROSION`). Every
bright region of the seed floods the part of the mask it is connected to, up to the mask's own
level; a mask region the seed does not touch sinks to the seed. With the image as the mask
and a lowered copy as the seed it removes peaks lower than the lowering (the h-dome); with a
seed that is the image on the border and its maximum inside, by erosion, it fills holes
(`FILL_HOLES` computes that case by a priority flood). Each channel on its own; `out` may be
`seed`.

The element comes from `params` as the morphology family's (`kernel`, or `shape` with its
size, 3 x 3 `RECT` by default, which is scikit-image's default footprint). Its anchor
`(kernel_width / 2, kernel_height / 2)` is scikit-image's offset; values flow from `p` to
`p + d` for each other cell `d`, as scikit-image moves them, so an asymmetric element is taken
the same way; pixels outside the image take no part. The result is made of the seed's and
the mask's own values, so it is exact in every type. It is computed by Vincent's hybrid
algorithm (1993): one raster pass, one anti-raster pass, then a queue from every pixel that
can still raise a neighbour. scikit-image sorts all pixels and walks a linked list; the fixed
point is unique, and suite 250 finds the two identical on every case: dilation and erosion,
the square, the cross, a 5 x 5 block and an asymmetric 3 x 3 element, in f64, f32 and 8 bits.
An even-sized element takes its anchor as above; scikit-image cannot be given one (its
`offset` check compares the offset's dimensions with the footprint's and always refuses), so
that case has no oracle.

`ALWAN_E_RANGE` for a seed above the mask when dilating or below it when eroding, as
scikit-image raises; `ALWAN_E_INVALID` for a NaN, a NULL, a zero size or an unknown method.

## Peaks

```c
typedef struct {
    size_t min_distance;                     /* 0 reads as 1 */
    int threshold_abs_given; double threshold_abs;
    int threshold_rel_given; double threshold_rel;
    int exclude_border_given;
    size_t exclude_border_rows, exclude_border_cols;
    size_t num_peaks;                        /* 0: all */
    unsigned char const *footprint;          /* NULL: (2 min_distance + 1) square */
    size_t footprint_width, footprint_height;
    int const *labels; size_t labels_row_stride;
    size_t num_peaks_per_label;              /* 0: all */
    double p_norm;                           /* 0 reads as infinity */
} alwan_peak_params;

alwan_status alwan_peak_local_max_{T}(size_t *peaks, size_t capacity, size_t *count,
                                      alwan_{T} const *src, size_t row_stride,
                                      size_t width, size_t height,
                                      alwan_peak_params const *params);
alwan_status alwan_peak_local_max_u8(...);   /* same, unsigned char pixels */
```

The local maxima of a single-channel image as `(row, column)` pairs, highest first:
scikit-image's `feature.peak_local_max`, peak for peak. The corner and ridge maps, a
distance transform or a blurred blob response become points this way. A pixel is a
candidate when it equals the maximum of the footprint around it (scipy's `maximum_filter`
with mode `nearest`: the footprint as given, centred at `(height / 2, width / 2)`, the edge
repeated) and lies above the threshold: `threshold_abs`, or the image's minimum, raised to
`threshold_rel` times its maximum when that is given. An image in which every pixel is a
candidate, a flat one, has no peaks. A strip `min_distance` wide along the border is dropped
unless `exclude_border_given` sets the rows and columns to drop (0 for none).

The candidates are sorted by value, ties in raster order, and when `min_distance` is above 1
they are kept greedily in that order: each keeps its place unless it lies closer than
`min_distance`, in the `p_norm`, to a peak already kept. That is scikit-image's
`ensure_spacing`, whose batches of 50 and more reduce to the same greedy pass. On a plateau
this keeps one pixel per `min_distance`, not the plateau's centre. `num_peaks` then cuts the
list.

With `labels`, each region (a label above 0) is searched on its own inside its bounding box,
with the rest of the box set to the type's lowest value: a region's peak is its own maximum
even where a neighbouring region is higher. A region whose every pixel is a candidate keeps
its isolated pixels (those its opening by the 3 x 3 cross removes), as scikit-image keeps
them. The regions' lists are joined in label order, `num_peaks_per_label` each; when
`num_peaks` cuts the joined list it is sorted again by the image's values. As in
scikit-image, the border strip clears the labels only for finding each region's box, so a
box that reaches into the strip can still hold a peak there.

Three details keep the lists identical to scikit-image's. On float32 the threshold is
compared rounded to float32, which is what NumPy does with a Python float against a float32
array, and `threshold_rel` times the maximum is a float32 product. On 8-bit data the sort
follows scikit-image's `argsort(-image)`, in which a negated 0 wraps to the top: it matters
only with a threshold under 1. And the spacing takes a point on an axis at exactly its
offset in any p-norm: for p = 3, `pow(pow(4, 3), 1 / 3)` falls one unit in the last place
short of 4 and would thin a peak that scipy's `cdist` keeps.

`peaks` receives up to `capacity` pairs and `count` how many there are; `ALWAN_E_RANGE`, with
`count` set, when they do not fit, so a first call with no room sizes the buffer. Suite 250
holds 185 cases to scikit-image, every coordinate equal: smooth fields, isolated and paired
blobs, plateaus and integer terrain full of ties, every parameter, three p-norms, an
asymmetric and an even footprint, labels with all their options, 8-bit data with the wrap,
and a flat image with and without labels.

## Thresholds

```c
typedef enum {
    ALWAN_THRESHOLD_OTSU = 0, ALWAN_THRESHOLD_LI = 1, ALWAN_THRESHOLD_YEN = 2,
    ALWAN_THRESHOLD_ISODATA = 3, ALWAN_THRESHOLD_TRIANGLE = 4,
    ALWAN_THRESHOLD_MINIMUM = 5, ALWAN_THRESHOLD_MEAN = 6
} alwan_threshold_method;

alwan_status alwan_threshold_{T}(double *threshold_out,
                                 alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                 size_t width, size_t height,
                                 alwan_threshold_method method,
                                 alwan_threshold_params const *params);
alwan_status alwan_threshold_u8(...);   /* same, unsigned char pixels */
```

One level per channel that splits it into a background and a foreground, the pixels above
it: the automatic step before a mask is cleaned by `alwan_morphology`. `threshold_out`
receives `channels` values.

| Method | The level it chooses |
|---|---|
| `OTSU` | the one maximising the variance between the two classes (Otsu 1979) |
| `LI` | Li's minimum cross entropy, iterated from the mean: `t <- (mb - mf) / (ln mb - ln mf)` over the class means (Li and Tam 1998) |
| `YEN` | the maximum of Yen's correlation criterion (Yen, Chang and Chang 1995) |
| `ISODATA` | the lowest level halfway, to within a bin, between the means below and above it (Ridler and Calvard 1978) |
| `TRIANGLE` | the level furthest below the line from the histogram's peak to the end of its longer tail (Zack et al. 1977): for one bright or dark population on a long tail |
| `MINIMUM` | the valley between the two maxima of the histogram smoothed by a 3-tap mean until it has two (Prewitt and Mendelsohn 1966): for two clear populations |
| `MEAN` | the mean |

| Field of `alwan_threshold_params` | 0 reads as |
|---|---|
| `bins` | 256: float data only, 2 to 65536 |
| `tolerance` | `LI`: half the smallest gap between two values (0.5 on 8-bit data) |
| `max_iterations` | `MINIMUM`: 10000 smoothing passes |

The histogram is scikit-image's: on 8-bit data one bin per level from the channel's
minimum to its maximum (`bins` is not used), on float data `bins` equal bins over the
channel's range with edges and placement as `numpy.histogram` computes them, a threshold
being a bin centre. The arithmetic follows scikit-image's `filters.threshold_*`
operation for operation, including where it works in float32 (the counts and their
running sums, and all of Yen's criterion) and numpy's pairwise summation for the means, so
suite 221 holds all seven methods to it value for value: 8-bit, double and float32 luma, a
bimodal image, 64 bins and 8-bit colour channel by channel, 56 cases. A constant channel
returns its value, as scikit-image's Otsu, Li and triangle do; its Yen, isodata and minimum
look at a histogram widened by half a unit either side there. `ISODATA` without a level and
`MINIMUM` without two maxima return `ALWAN_E_RANGE`, where scikit-image raises.

## Local thresholds

```c
typedef enum {
    ALWAN_THRESHOLD_LOCAL_GAUSSIAN = 0, ALWAN_THRESHOLD_LOCAL_MEAN = 1,
    ALWAN_THRESHOLD_LOCAL_MEDIAN = 2, ALWAN_THRESHOLD_LOCAL_NIBLACK = 3,
    ALWAN_THRESHOLD_LOCAL_SAUVOLA = 4
} alwan_threshold_local_method;

alwan_status alwan_threshold_local_{T}(alwan_{T} *out, size_t out_row_stride,
                                       alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height,
                                       alwan_threshold_local_method method,
                                       alwan_threshold_local_params const *params);
```

A threshold for every pixel from the `block_size x block_size` neighbourhood around it,
where one level for the whole image (`alwan_threshold`) cannot follow a change of light.
`out` is the threshold image; a pixel is foreground where it is above its threshold.
Each channel on its own; `out` may be `src`.

| Method | Threshold |
|---|---|
| `GAUSSIAN` | the gaussian-weighted mean of the block, sigma `(block_size - 1) / 6` by default, minus `offset` |
| `MEAN` | the block's mean, minus `offset` |
| `MEDIAN` | the block's median, minus `offset` |
| `NIBLACK` | `m - k s`, the block's mean and standard deviation (Niblack 1986) |
| `SAUVOLA` | `m (1 + k (s / r - 1))`: lower than the mean where the block is flat, so a page's plain paper stays background (Sauvola and Pietikainen 2000) |

| Field of `alwan_threshold_local_params` | 0 reads as |
|---|---|
| `block_size` | 15; odd, 1 to 1023 |
| `offset` | 0 |
| `sigma` | `(block_size - 1) / 6`, covering the block to three sigma |
| `k` | 0.2 (a k of 0 is not expressible; `MEAN` is Niblack's with it) |
| `r` | 1, the dynamic range scikit-image gives float data |

This is scikit-image's `filters.threshold_local` with its gaussian, mean and median
methods, `threshold_niblack` and `threshold_sauvola`, and suite 222 holds all five to them
value for value (45 cases: double and float32, blocks of 3 to 51, an image smaller than the
block, colour). The first three are scipy's filters, columns and then rows, each pass kept
in the data's precision, the image extended by scipy's `reflect` (the edge sample
repeated); the gaussian is scipy's kernel truncated at four sigma. Niblack's and Sauvola's
mean and deviation come from scikit-image's integral images of the image padded by
numpy's `reflect` (the edge sample not repeated), summed in double. On float32 data
`offset`, `k` and `r` are rounded to float first, as numpy does with a Python number. There
is no 8-bit entry point, since scikit-image converts 8-bit data to double: convert it and,
for Sauvola, pass `r = 127.5`, the value scikit-image uses for 8-bit data.

## Gradient

```c
typedef enum {
    ALWAN_GRADIENT_SOBEL = 0, ALWAN_GRADIENT_SCHARR = 1, ALWAN_GRADIENT_PREWITT = 2,
    ALWAN_GRADIENT_FARID = 3, ALWAN_GRADIENT_ROBERTS = 4
} alwan_gradient_method;

alwan_status alwan_gradient_{T}(alwan_{T} *out, size_t out_row_stride,
                                alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                size_t width, size_t height,
                                alwan_gradient_method method,
                                alwan_gradient_params const *params);
```

The derivative of each channel by a small kernel: the gradient magnitude by default, or one
signed derivative. The magnitude is the relief `alwan_segment`'s watershed floods to
divide an image along its edges, and the input to edge maps and detail measures. `out` may
be `src`.

| Method | Kernel |
|---|---|
| `SOBEL` | `[1, 0, -1]` across, `[1, 2, 1] / 4` along |
| `SCHARR` | `[1, 0, -1]` across, `[3, 10, 3] / 16` along: the most nearly rotation-invariant of the 3 x 3 kernels |
| `PREWITT` | `[1, 0, -1]` across, `[1, 1, 1] / 3` along |
| `FARID` | Farid and Simoncelli's 5-tap derivative and interpolator (2004), optimised for rotation invariance |
| `ROBERTS` | the diagonal differences `[[1, 0], [0, -1]]` and `[[0, 1], [-1, 0]]` |

`params->component`: 0 (the default) for the magnitude `sqrt(d0^2 + d1^2) / sqrt(2)`; 1
for the signed derivative down the rows (for `ROBERTS` the first diagonal); 2 for the
derivative across the columns (the second diagonal).

This is scikit-image's `filters.sobel`, `scharr`, `prewitt`, `farid`, `roberts`,
`roberts_pos_diag` and `roberts_neg_diag` (the components are their `axis=0` and `axis=1`),
and suite 225 holds all of them to it value for value: every method's magnitude and both
components, double and float32, colour channel by channel, and an image smaller than
Farid's kernel, 40 cases. The kernels are applied as scipy's `ndimage.convolve` applies
them (flipped, zero weights skipped, the rest summed in raster order in double) with its
`reflect` edge; the magnitude is formed in the data's precision. There is no 8-bit entry
point: scikit-image divides 8-bit data by 255 first, and so should the caller.

## Edge detection

```c
typedef enum {
    ALWAN_EDGE_DETECT_CANNY = 0
} alwan_edge_detect_method;

alwan_status alwan_edge_detect_{T}(unsigned char *edges, size_t edges_row_stride,
                                   alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                   size_t width, size_t height,
                                   alwan_edge_detect_method method,
                                   alwan_edge_detect_params const *params);
alwan_status alwan_edge_detect_u8(...);   /* same, unsigned char pixels */
```

A binary edge map of one channel (`channels` must be 1): 1 on an edge, 0 elsewhere.

| Field of `alwan_edge_detect_params` | 0 reads as |
|---|---|
| `sigma` | 1: the smoothing's standard deviation in pixels, up to 64 |
| `low_threshold` | 0.1 of the float range (25.5 on 8-bit data) |
| `high_threshold` | 0.2 of the float range (51 on 8-bit data) |

### `CANNY`

Canny's detector (1986): the channel smoothed by a gaussian, its Sobel gradient,
non-maximum suppression that keeps a pixel only where its magnitude is at least that of
the two points a pixel away along the gradient (interpolated between the neighbours they
fall between), and hysteresis: of the pixels left above the low threshold, the
8-connected pieces that hold a pixel at or above the high threshold. The result is edges
one pixel wide that run on through weak stretches but do not start in them. The border
row and column are never edges.

This is scikit-image's `feature.canny`, and suite 226 holds it to it pixel for pixel: double
at sigmas of 0.5 to 2.5 with default and chosen thresholds, float32, 8-bit with thresholds
in 8-bit units, and an image smaller than the smoothing, 12 cases. The smoothing has a zero
edge and is divided by the same smoothing of an all-ones image, so the border does not
darken; the gradient is scipy's `ndimage.sobel` with its `reflect` edge; each pass is kept
in the data's precision; and the suppression follows scikit-image's Cython, whose low
threshold is a C `float` whatever the data. 8-bit data is multiplied by `1 / 255` and its
thresholds divided by 255, as scikit-image does. scikit-image's quantile thresholds and
mask are not carried.

## Corner response

```c
typedef enum {
    ALWAN_CORNER_HARRIS = 0, ALWAN_CORNER_SHI_TOMASI = 1,
    ALWAN_CORNER_KITCHEN_ROSENFELD = 2, ALWAN_CORNER_FOERSTNER = 3
} alwan_corner_method;

alwan_status alwan_corner_response_{T}(alwan_{T} *out, size_t out_row_stride,
                                       alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height,
                                       alwan_corner_method method,
                                       alwan_corner_params const *params);
```

How strongly each pixel of one channel (`channels` must be 1) is a corner: large where the
image changes in two directions at once, near zero on flat ground and along straight
edges. The local maxima of the response are the feature points of registration and
tracking. `out` may be `src`.

| Method | Response |
|---|---|
| `HARRIS` | `det - k trace^2` of the structure tensor (Harris and Stephens 1988); with `harris_normalised`, `2 det / (trace + eps)` (Noble 1989) |
| `SHI_TOMASI` | the tensor's smaller eigenvalue (Shi and Tomasi 1994) |
| `KITCHEN_ROSENFELD` | `(Ixx Iy^2 + Iyy Ix^2 - 2 Ixy Ix Iy) / (Ix^2 + Iy^2)`, the level line's curvature times the gradient, from second derivatives without smoothing; 0 where the gradient is |
| `FOERSTNER` | `det / trace`, the size of the error ellipse (`component` 0), or `4 det / trace^2`, its roundness in [0, 1] (`component` 1); 0 where the trace is |

| Field of `alwan_corner_params` | 0 reads as |
|---|---|
| `sigma` | 1: the structure tensor's smoothing, up to 64 |
| `k` | 0.05 |
| `harris_normalised` | 0: the `k` form |
| `eps` | 1e-6 |
| `component` | 0 |

The structure tensor is scikit-image's: scipy's `ndimage.sobel` down the rows and across
the columns with a zero edge, and the three products of the two derivatives each smoothed
by scipy's gaussian with a zero edge, in the data's precision. This is scikit-image's
`feature.corner_harris`, `corner_shi_tomasi`, `corner_kitchen_rosenfeld` and
`corner_foerstner`, and suite 229 holds all of them to it value for value: double and
float32, Harris at three sigmas, another `k` and normalised, both Foerstner components,
and a checkerboard, 17 cases. The zero edge makes the image's border read as a step, so
the response is not zero there even on a constant image. There is no 8-bit entry point;
scikit-image divides 8-bit data by 255 first.

## Texture codes

```c
typedef enum {
    ALWAN_TEXTURE_LBP = 0, ALWAN_TEXTURE_LBP_ROR = 1, ALWAN_TEXTURE_LBP_UNIFORM = 2,
    ALWAN_TEXTURE_LBP_NRI_UNIFORM = 3, ALWAN_TEXTURE_LBP_VAR = 4
} alwan_texture_method;

alwan_status alwan_texture_{T}(double *out, size_t out_row_stride,
                               alwan_{T} const *src, size_t src_row_stride, size_t channels,
                               size_t width, size_t height,
                               alwan_texture_method method,
                               alwan_texture_params const *params);
alwan_status alwan_texture_u8(...);   /* same, unsigned char pixels */
```

A code for every pixel of one channel (`channels` must be 1) describing the pattern of its
neighbourhood; a histogram of the codes over a region is that region's texture, the
descriptor of texture classification and matching. `out` is doubles for every input type,
since codes pass 255 for more than 8 points and the variance is real.

The local binary pattern (Ojala, Pietikainen and Maenpaa 2002) samples `points` neighbours
on a circle of `radius` pixels, bilinearly interpolated with 0 outside the image, and sets
bit p where neighbour p is at least the centre. It sees the pattern and not the contrast,
so it is unchanged by any increasing change of the levels: exposure, gamma, a tone curve.

| Method | Code |
|---|---|
| `LBP` | sum of bit p times 2^p, 0 to 2^P - 1 |
| `LBP_ROR` | the smallest of the code's P rotations: invariant to rotating the image |
| `LBP_UNIFORM` | the number of set bits for a uniform pattern (at most two changes along p = 0 .. P - 1), else P + 1: rotation invariant, P + 2 codes |
| `LBP_NRI_UNIFORM` | uniform patterns told apart by their rotation as well, P (P - 1) + 3 codes |
| `LBP_VAR` | the neighbours' variance (ddof 0), the contrast the others ignore; NaN where it is 0 |

| Field of `alwan_texture_params` | 0 reads as |
|---|---|
| `points` | 8; up to 31 |
| `radius` | 1 |

This is scikit-image's `feature.local_binary_pattern` value for value, and suite 230 holds
it there: 8-bit luma with a flat patch through every method at (8, 1) and (16, 2), other
circles up to (24, 3), and double and float32, 18 cases. The neighbour offsets are rounded
to 5 decimals as scikit-image rounds them, and the uniform count, like scikit-image's,
does not wrap from the last neighbour to the first. scikit-image recommends integer
images, since on float data neighbours that differ from the centre by rounding alone flip
bits.

## Ridge filters

```c
typedef enum {
    ALWAN_RIDGE_FRANGI = 0, ALWAN_RIDGE_SATO = 1,
    ALWAN_RIDGE_MEIJERING = 2, ALWAN_RIDGE_HESSIAN = 3
} alwan_ridge_method;

alwan_status alwan_ridge_{T}(alwan_{T} *out, size_t out_row_stride,
                             alwan_{T} const *src, size_t src_row_stride, size_t channels,
                             size_t width, size_t height,
                             alwan_ridge_method method, alwan_ridge_params const *params);
```

How strongly each pixel of one channel (`channels` must be 1) lies on a ridge: a line, a
vessel, a crease, a fibre, a scratch. At each scale in `sigmas` the Hessian's two
eigenvalues say how the image curves along and across the pixel's strongest direction; a
ridge curves strongly across and little along. The largest answer over the scales is kept,
so each ridge answers at the scale of its own width. Dark ridges on a light ground by
default, which is where the Hessian across the ridge is positive; `bright_ridges` looks for
the opposite. `out` may be `src`.

| Method | Response |
|---|---|
| `FRANGI` | Frangi et al. 1998, 0 to 1: `exp(-rb^2 / 2 beta^2) (1 - exp(-s^2 / 2 gamma^2))`, `rb` the eigenvalues' ratio (blobness), `s` their norm (structuredness) |
| `SATO` | Sato et al. 1998: `sigma^2` times the larger eigenvalue, floored at 0 |
| `MEIJERING` | Meijering et al. 2004 (neuriteness): of `l1 + alpha l2` and `l2 + alpha l1` the one larger in magnitude, floored at 0, normalised to 1 at each scale |
| `HESSIAN` | `FRANGI` with `gamma` 15, and 1 where that is 0 |

| Field of `alwan_ridge_params` | 0 reads as |
|---|---|
| `sigmas`, `sigma_count` | NULL: 1, 3, 5, 7, 9 |
| `alpha` | `MEIJERING`: 1 / 3 (`FRANGI` has no use for it in 2-D) |
| `beta` | 0.5 |
| `gamma` | `FRANGI`: half the largest Hessian norm at the first scale; `HESSIAN`: 15 |
| `bright_ridges` | 0: dark ridges |

This is scikit-image's `filters.frangi`, `sato`, `meijering` and `hessian`, the Hessian from
two first-order gaussian derivatives at `sigma / sqrt(2)` as scipy's `gaussian_filter`
computes them (truncated at 8 standard deviations, 100 at a sigma of 1 or less, with its
`reflect` edge) and its eigenvalues in closed form. Suite 232 holds them to it on a frame's
luma and on dark lines of three widths, 19 cases: `FRANGI`, `SATO` and `HESSIAN` in double
equal, `MEIJERING` to 1.1e-16 (scikit-image mixes the eigenvalues through BLAS). On float32
scikit-image calls numpy's float32 `exp`, whose AVX2 kernel is a unit or two from the
correctly rounded value that the C library's `expf` gives, so `FRANGI` agrees to 1.2e-7;
`HESSIAN` then turns the pixels where `FRANGI` rounds to exactly 0 on one side only into
1, 4.8% of the float32 case.

## Resizing

```c
typedef enum {
    ALWAN_RESIZE_NEAREST = 0, ALWAN_RESIZE_BOX = 1, ALWAN_RESIZE_BILINEAR = 2,
    ALWAN_RESIZE_HAMMING = 3, ALWAN_RESIZE_BICUBIC = 4, ALWAN_RESIZE_LANCZOS = 5
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

Two options leave Pillow's resampling for other ends. `integration` (an
`alwan_pixel_integration`, see Warping) makes each output pixel the mean of the source over
its own area, through `alwan_warp`'s integration with the scale as the map: the source is
reconstructed at sub-positions of the output pixel by nearest (`NEAREST`, `BOX`), bilinear
(`BILINEAR`, `HAMMING`) or bicubic (`BICUBIC`, `LANCZOS`), and `kernel`, `samples`, `seed`
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

## Warping

```c
typedef enum {
    ALWAN_WARP_NEAREST = 0, ALWAN_WARP_BILINEAR = 1, ALWAN_WARP_BICUBIC = 2
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

## Distance transforms

```c
typedef enum {
    ALWAN_DISTANCE_EUCLIDEAN = 0, ALWAN_DISTANCE_CITYBLOCK = 1, ALWAN_DISTANCE_CHESSBOARD = 2
} alwan_distance_method;

alwan_status alwan_distance_transform_{T}(double *out, size_t out_row_stride,
                                          alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                          size_t width, size_t height,
                                          alwan_distance_method method, alwan_distance_params const *params);
alwan_status alwan_distance_transform_u8(double *out, ...);   /* same, unsigned char pixels */
```

For every feature pixel, its distance to the nearest background pixel, one double a pixel
whatever the source's type. A pixel is background when every channel equals `background`;
the image's border is not background. The distances feather a mask, grow or shrink a
shape by a threshold on them, find a shape's thickness, and with `signed_distance` give a
signed distance field (positive inside, negative outside), the form text is often stored
in for rendering at any size.

| Method | Distance | As |
|---|---|---|
| `EUCLIDEAN` | the straight-line distance, in the pixel's width and height (`sampling`) | `scipy.ndimage.distance_transform_edt` |
| `CITYBLOCK` | `abs(dx) + abs(dy)` in pixels | `distance_transform_cdt`, taxicab |
| `CHESSBOARD` | `max(abs(dx), abs(dy))` in pixels | `distance_transform_cdt`, chessboard |

| Field of `alwan_distance_params` | 0 reads as |
|---|---|
| `background` | 0 in every channel |
| `sampling[2]` | square pixels of side 1 (Euclidean only) |
| `signed_distance` | distances for the features only, 0 on the background |

The Euclidean distance is exact: Felzenszwalb and Huttenlocher's lower envelope of
parabolas, one pass down the columns and one along the rows on squared distances, so with
unit sampling every value is the correctly rounded square root of an integer. The other two
are exact for their metrics after a forward and a backward chamfer pass. With no background
pixel every distance is +infinity; scipy's EDT then measures from a point outside the image
and its CDT returns -1, so that case is alwan's own.

Suite 236 holds all three to scipy on sixteen cases: blobs, rectangles and discs, a lone
background pixel, a background other than 0, three 8-bit channels, two anisotropic
samplings and the signed field (as `edt(features) - edt(background)`). Every value is
equal but the sampled ones, which agree to 1.8e-15.

## Segmentation

```c
typedef enum {
    ALWAN_SEGMENT_CONNECTED = 0,
    ALWAN_SEGMENT_WATERSHED = 1,
    ALWAN_SEGMENT_SLIC = 2
} alwan_segment_method;

alwan_status alwan_segment_{T}(uint32_t *labels, size_t labels_row_stride, size_t *count_out,
                               alwan_{T} const *src, size_t src_row_stride, size_t channels,
                               size_t width, size_t height,
                               alwan_segment_method method,
                               alwan_segment_params const *params);
alwan_status alwan_segment_u8(...);   /* same, unsigned char pixels */
```

An image divided into regions, one `uint32_t` label a pixel, 0 for background or a
watershed line. `labels_row_stride` is in bytes, as every stride here is. `count_out`, which
may be NULL, receives the largest label: the number of regions for `CONNECTED`, and for
`WATERSHED` from its own markers.

| Field of `alwan_segment_params` | 0 reads as |
|---|---|
| `connectivity` | scikit-image's default for the method: 8 for `CONNECTED`, 4 for `WATERSHED`; or give 4 or 8 |
| `background` | `CONNECTED`: 0; pixels equal to it in every channel stay 0 |
| `label_background` | `CONNECTED`: 0; non-zero labels the background's regions too |
| `markers`, `markers_row_stride` | `WATERSHED`: NULL, flood from the local minima; otherwise a `uint32_t` image of labels, 0 where unmarked |
| `compactness` | `WATERSHED`: 0, the classic flood |
| `watershed_line` | `WATERSHED`: 0; non-zero leaves the pixels between basins 0 |
| `compactness` | `SLIC`: 10 |
| `n_segments` | `SLIC`: 100 seeds asked for; the grid gives about that many |
| `max_iterations` | `SLIC`: 10 |
| `slic_zero` | `SLIC`: 0; non-zero scales each superpixel's colour distance by its largest (SLIC-zero) |
| `keep_disconnected` | `SLIC`: 0; non-zero skips the merging of small and disconnected pieces |
| `min_size_factor`, `max_size_factor` | `SLIC`: 0.5 and 3 times the mean superpixel size |

### `CONNECTED`

The connected components of equal pixels: every region is a set of pixels with the same
value in every channel, each reachable from the others through such pixels. After
`alwan_threshold` it gives a mask's objects, which `alwan_morphology`'s area operators
can then filter by size; on a quantised or palette image it gives the flat regions of each
colour. Regions are numbered in the raster order of their first pixel. This is
scikit-image's `measure.label`, and suite 223 holds it to it label for label and count
for count: a thresholded mask at both connectivities, a quantised luma with backgrounds of
0 and 3 and with the background labelled, float32, and colour (which the reference packs
into one integer a pixel, since scikit-image reads a colour array as a volume). One
union-find pass joins each pixel to its equal neighbours already visited, and a second
numbers the roots.

### `WATERSHED`

One channel, read as a relief, flooded from markers: every pixel joins the basin that
reaches it first, the lowest levels flooding first. Flood a gradient magnitude and the
basins are the image's regions and their borders its edges; flood a distance transform's
negative and touching objects separate. Without `markers` each local minimum (a plateau
whose other neighbours are all higher) is a marker, numbered in raster order, which on a
noisy gradient over-segments; markers from a threshold or a coarse grid give one region
each. `compactness` adds that times the distance to the marker to each level, pulling
regions toward even shapes (Neubert and Protzel 2014); `watershed_line` leaves the
one-pixel boundaries between basins at 0.

Which basin takes a pixel on a tie depends on the order the flood visits equal levels, so
this follows scikit-image's `segmentation.watershed` step for step: the image padded by
one pixel, a binary heap ordered by level and then by age with scikit-image's own sift
steps (markers all age 0, pushed in raster order), neighbours visited up, left, right,
down and then the diagonals, each push one age older, and a pixel's level raised to the
level that reached it. Suite 224 holds it to scikit-image label for label: a gradient from
its minima at both connectivities, an image quantised to 16 levels where ties are
everywhere, a caller's markers numbered with gaps, compact floods, watershed lines,
float32 and 8-bit, 15 cases. A constant image has no minima (scikit-image disqualifies a
plateau at the image's maximum where it meets the border) and stays 0.

### `SLIC`

Superpixels by simple linear iterative clustering (Achanta et al. 2012): seeds on a
regular grid of about `n_segments` cells, then k-means over position and every channel, each
pixel compared only with the seeds within two grid steps. The distance is the squared
spatial distance over the squared grid step plus the squared channel distance, the image
first rescaled to [0, 1] and divided by `compactness`: a high compactness gives square
cells, a low one cells that follow the colour. A last pass merges each piece smaller than
`min_size_factor` of the mean size, or cut off from its superpixel, into a neighbour, so
the labels run from 1 to `*count_out` with none missing.

Distances are measured in the channels as given: pass Lab (converted with alwan) for
perceptual superpixels on colour, as scikit-image does by default. The rescaling to
[0, 1] comes first, over every channel together; scikit-image rescales before its own Lab
conversion, so its compactness of 10 acts on Lab's range of about 100 to 150. On a Lab
image passed here the same balance is a compactness of about 0.07 (10 over the range). This is scikit-image's
`segmentation.slic` with `convert2lab=False`, and suite 227 holds it to it label for label:
grey and colour, 30 to 250 segments, compactness 0.05 to 10, float32, 8-bit, SLIC-zero,
the merging off, and a short run, 11 cases. It follows scikit-image's Cython: the seeds from
its `regular_grid`, the grid step held as a C `float` in the spatial weight, the distances
in the data's precision, and the merging's capped breadth-first fill.

## Label overlays

```c
alwan_status alwan_label2rgb_{T}(double *out, size_t out_row_stride,
                                 uint32_t const *labels, size_t labels_row_stride,
                                 alwan_{T} const *image, size_t image_row_stride, size_t channels,
                                 size_t width, size_t height, alwan_label2rgb_params const *params);
alwan_status alwan_label2rgb_u8(...);          /* the same with an 8-bit image */
alwan_status alwan_find_boundaries(unsigned char *out, size_t out_row_stride,
                                   uint32_t const *labels, size_t labels_row_stride,
                                   size_t width, size_t height, alwan_boundary_params const *params);
alwan_status alwan_mark_boundaries_{T}(double *out, size_t out_row_stride,
                                       alwan_{T} const *image, size_t image_row_stride, size_t channels,
                                       uint32_t const *labels, size_t labels_row_stride,
                                       size_t width, size_t height,
                                       alwan_mark_boundaries_params const *params);
alwan_status alwan_mark_boundaries_u8(...);    /* the same with an 8-bit image */
```

Ways to look at the label images `alwan_segment` writes, as scikit-image's `label2rgb`,
`find_boundaries` and `mark_boundaries` produce them. Suite 254 holds every value to
scikit-image's.

**`alwan_label2rgb_{T}`** writes RGB doubles.
- `OVERLAY` colours each region by its rank among the distinct labels that are not the
  background, smallest first. The colours cycle through `params->colors` (`color_count` RGB
  triples) or scikit-image's ten: red, blue, yellow, magenta, green, indigo, darkorange, cyan,
  pink, yellowgreen. The background takes `bg_color`, or keeps the image when `bg_color_none`
  is set.
- With an image (1 or 3 channels), its saturation is scaled by `saturation` through
  scikit-image's `rgb2hsv` and `hsv2rgb`, which are transcribed; the default of 0 turns it
  grey. It is then brightened to `image * image_alpha + (1 - image_alpha)`, and the colours
  are blended over it at `alpha`.
- Without an image, the colours are written as they are.
- `AVG` paints each region with the image's mean over it, summed in numpy's order, and the
  background with `bg_color`. An 8-bit mean is truncated, as scikit-image stores it back into
  a `uint8` array.
- Zero fields are scikit-image's defaults: `alpha` 0.3, background label 0 with colour
  black, `image_alpha` 1. The `*_given` flags let 0 be passed as a value.

8-bit images are read as `v * (1 / 255)`, as `img_as_float` does; `v / 255` differs from it
in the last bit for some values. A float32 image is processed in float32, as scikit-image
does.

**`alwan_find_boundaries`** writes a byte mask, 1 where a pixel's neighbourhood holds more
than one label: a grey dilation of the labels differs from a grey erosion, over the cross
(`connectivity` 1) or the full 3 x 3 square (2), edges repeated.
- `INNER` keeps the boundary pixels that are not background.
- `OUTER` keeps those that are background or that touch another object.
- `SUBPIXEL` writes a `(2 width - 1) x (2 height - 1)` mask whose in-between cells mark where
  labels meet.

**`alwan_mark_boundaries_{T}`** returns the image as RGB doubles with the boundary pixels
(`OUTER` by default) painted `color`, yellow by default. With `outline_given`, the 3 x 3
neighbourhood of each boundary pixel is first painted `outline_color`. `SUBPIXEL` is refused:
scikit-image resamples the image with a cubic zoom there.

**Returns:** `ALWAN_E_INVALID` for a NULL output or labels, a zero size, channels other than
1 or 3, an unknown kind or mode, a connectivity other than 0, 1 or 2, a saturation outside
`[0, 1]`, or `AVG` without an image.

## Region properties

```c
alwan_status alwan_region_props_{T}(alwan_region_props *props, size_t capacity, size_t *count_out,
                                    uint32_t const *labels, size_t labels_row_stride,
                                    alwan_{T} const *intensity, size_t intensity_row_stride,
                                    size_t channels, size_t width, size_t height);
alwan_status alwan_region_props_u8(...);   /* same, unsigned char intensity */
```

Measurements of every region of a label image, such as `alwan_segment` produces: one
`alwan_region_props` a label present, in increasing label order, 0 being background. Call
with `props` NULL and `capacity` 0 to count the regions first; too small a capacity returns
`ALWAN_E_RANGE` with the count in `*count_out`. `intensity` may be NULL; otherwise each
region's mean, minimum and maximum are taken in each of its `channels`.

| Field | Meaning |
|---|---|
| `label`, `area` | the label and its pixel count |
| `bbox` | min row, min column, max row + 1, max column + 1 |
| `centroid` | row, column |
| `intensity_mean`, `_min`, `_max` | per channel |
| `orientation` | radians in (-pi/2, pi/2], from the rows' axis to the major axis |
| `axis_major_length`, `axis_minor_length` | of the ellipse with the region's second moments |
| `eccentricity` | 0 for a circle, towards 1 for a line |
| `perimeter` | along the 4-connected border, diagonal steps counted sqrt(2) |
| `equivalent_diameter` | of the circle with the region's area |
| `moments`, `moments_central` | the region's box as a 0/1 image, raw and about its centroid, [p][q] row power p, column power q, orders 0 to 3 |
| `moments_normalized` | NaN where p + q < 2 |
| `moments_hu` | Hu's seven invariants |
| `inertia_tensor`, `inertia_tensor_eigvals` | eigenvalues largest first, clipped at 0 |
| `area_convex`, `solidity` | pixels of the box inside the convex hull or on its edges; area / area_convex |
| `euler_number` | 8-connected objects less 4-connected holes |
| `perimeter_crofton` | Crofton's formula over four directions |
| `feret_diameter_max` | the longest distance across the convex hull's outline |

These are scikit-image's `measure.regionprops` (`label`, `area`, `bbox`, `centroid`,
`intensity_mean`, `intensity_min`, `intensity_max`, `orientation`, `axis_major_length`,
`axis_minor_length`, `eccentricity`, `perimeter`, `equivalent_diameter_area`), and suite
228 holds them to it on SLIC, connected-component and watershed label images with grey,
colour and no intensity, double, float32 and 8-bit, 10 cases. The counts, boxes,
centroids and intensity statistics are equal (the mean summed as numpy sums it: pairwise
for one channel, a running sum per channel for several). The shape measures come from
the central moments with the inertia tensor's eigenvalues in closed form, where
scikit-image uses einsum and LAPACK, and agree to 6e-15; the perimeter, a weighted
histogram scikit-image sums by BLAS, to the same.

The fields from `moments` on are scikit-image's properties of the same names, and suite 252
holds them to it on shapes with holes, an island in a hole, a single pixel, a diagonal line,
a thin bar, slanted polygons, random blobs and a one-row image. `area_convex`, `solidity`,
`euler_number` and `feret_diameter_max` are equal: scikit-image's convex hull is the hull of
the four edge midpoints of the row and column extremes, and the pixel centres inside it or
on it, which alwan builds in doubled integer coordinates, so no rounding enters; its largest
Feret diameter is the longest distance between the points where the hull mask's outline
crosses between pixels, which are half-integer midpoints. The moments agree to rounding
(scikit-image sums them in einsum), Crofton's perimeter to 2e-16 of itself.

## Moments

```c
alwan_status alwan_moments_{T}(double *mu, size_t order, alwan_{T} const *src, size_t row_stride,
                               size_t width, size_t height, alwan_moments_method method,
                               alwan_moments_params const *params);
alwan_status alwan_moments_u8(double *mu, size_t order, unsigned char const *src, ...);
alwan_status alwan_moments_normalized(double *nu, double const *mu, size_t order, double const *spacing);
alwan_status alwan_moments_hu(double *hu, double const *nu, size_t order);
alwan_status alwan_inertia_tensor(double *tensor, double *eigvals, double const *mu, size_t order);
```

The moments of a one-channel image, as scikit-image's `measure.moments`, `moments_central`,
`moments_normalized`, `moments_hu`, `inertia_tensor` and `inertia_tensor_eigvals`. `mu`
receives (order + 1)^2 values, `mu[p * (order + 1) + q]` the sum over pixels of the value
times dr^p dc^q, with dr = row * spacing[0] - center[0] and dc = column * spacing[1] -
center[1]; order is at most 16.

| `method` | Centre | Formed |
|---|---|---|
| `RAW` | 0 | summed directly, every p and q |
| `CENTRAL` | the centroid | from the raw moments as scikit-image forms them: its closed forms to order 3, the binomial expansion above; 0 where p + q > order |
| `CENTRAL`, `center_given` | `center` | summed directly about it, every p and q |

`alwan_moments_params` is `center_given`, `center[2]` (row, column) and `spacing[2]` (0 reads
as 1). u8 takes raw values 0 to 255, as scikit-image does; f32 computes in double.
`alwan_moments_normalized` divides by mu00^((p + q) / 2 + 1) and min(spacing)^(p + q), NaN
where p + q < 2; `alwan_moments_hu` evaluates scikit-image's expressions in its order; the
inertia tensor is [[mu02, -mu11], [-mu11, mu20]] / mu00, its eigenvalues in closed form where
scikit-image calls LAPACK.

Suite 252 holds them to scikit-image at orders 0 to 6 on float and u8 images, with and
without spacing, about the centroid and about a given centre: summed moments within 1e-13 of
the matrix's largest entry, central moments from raw ones within 64 epsilon of the size of
what they cancel (the raw terms of that expansion, which is large far from the origin), and
the normalised moments, Hu's invariants, the tensor and its eigenvalues within 1e-12. On
float32-exact values the f32 entry point gives the f64 moments exactly.

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
centre ((size - 1) / 2). The best match is the largest value: `alwan_peak_local_max` finds it,
and the next ones.

| `alwan_template_params` | Default | Meaning |
|---|---|---|
| `pad_input` | 0 | as above |
| `mode` | `CONSTANT` | how the image is padded: `CONSTANT`, `EDGE`, `SYMMETRIC`, `REFLECT`, `WRAP`, numpy.pad's |
| `constant_value` | 0 | `CONSTANT`'s value |

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
5.x's own sources. One method so far, `ALWAN_CHECKER_DETECT_SEGMENTATION`; the package's
inference method needs network weights alwan does not ship.

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

## Contours

```c
typedef struct {
    int level_given;                  /* 0: the midpoint of the smallest and largest value */
    double level;
    int fully_connected_high;         /* the saddles: 0 joins the low corners, else the high */
    int positive_orientation_high;    /* non-zero reverses every contour */
    unsigned char const *mask;        /* NULL, or 0 where there is no data */
    size_t mask_row_stride;
} alwan_contour_params;

alwan_status alwan_find_contours_{T}(alwan_contours **out, alwan_{T} const *src, size_t row_stride,
                                     size_t width, size_t height,
                                     alwan_contour_params const *params, alwan_ctx *ctx);
alwan_status alwan_find_contours_u8(...);   /* same, unsigned char */
size_t alwan_contours_count(alwan_contours const *contours);
alwan_status alwan_contours_get(double const **points, size_t *count,
                                alwan_contours const *contours, size_t index);
void alwan_contours_destroy(alwan_contours *contours, alwan_ctx *ctx);
alwan_status alwan_approximate_polygon(double *out, size_t *out_count, double const *points,
                                       size_t count, double tolerance);
```

The lines along which a one-channel image crosses a level, by marching squares, as
scikit-image's `measure.find_contours`. A 2 x 2 square walks the image row by row. Each
corner above the level sets a bit; a corner equal to the level counts as below. The
square's case gives the segment or segments that cross it, their ends interpolated
linearly along the edges and directed so the lower values lie on the left. The two saddle
cases, where diagonal corners are above, join the high corners when `fully_connected_high`
is set and the low ones otherwise. A square with a NaN corner, or a corner where `mask` is
0, is skipped, so contours stop at missing data.

Segments are joined into contours in the order they are made, and when two contours meet,
the one begun first keeps its place: contours come out ordered by where they begin, left
to right and top to bottom. A closed contour repeats its first point at the end; an open
one ends at the image border, the mask or a NaN. `positive_orientation_high` reverses every
contour. Points are (row, column), 0 at the first pixel's centre.

`level_given` 0 takes the midpoint of the smallest and largest value that is not NaN.
8-bit data are read as they are, 0 to 255, with the level in the same units, as
scikit-image reads a uint8 image; scikit-image forms that midpoint's sum in uint8, which
wraps past 255, and alwan takes the true midpoint. For a colour image, pass one channel or
a luminance plane. The result belongs to the caller: `alwan_contours_get` points into it,
and `alwan_contours_destroy` frees it.

`alwan_approximate_polygon` simplifies a polyline by Douglas-Peucker, as scikit-image's
`approximate_polygon`. It keeps the first and last points, then splits each span at its
farthest point while that point lies more than `tolerance` from the span: the
perpendicular distance where the point projects inside the span, else the distance to the
nearer end. The kept points stay in their original order. `out` needs room for `count`
pairs and may be `points` itself; a tolerance of 0 or below copies the polyline unchanged.

Suite 249 holds both to scikit-image 0.26: 29 images (smooth fields, binary shapes with a
hole and diagonal contacts under both saddle rules and both orientations, a checkerboard of
saddles, levels exactly at pixel values, a mask, NaN holes, float32 and 8-bit input, 2 x N
and N x 2, a flat image, a rendered 4 x 6 chart), 226 contours and 4,516 points, every
coordinate equal; and 65 simplifications of those contours at five tolerances, in place
and not, every kept point equal. An image under 2 x 2 returns `ALWAN_E_INVALID`, as
scikit-image refuses it.

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
    ALWAN_COLOR_TRANSFER_MKL = 2
} alwan_color_transfer_method;

alwan_status alwan_color_transfer_{T}(alwan_{T} *out, size_t out_stride,
                                      alwan_{T} const *src, size_t src_stride, size_t src_count,
                                      alwan_{T} const *ref, size_t ref_stride, size_t ref_count,
                                      size_t channels, alwan_color_transfer_method method,
                                      alwan_color_transfer_params const *params);
```

The look of a reference image carried onto a source. The two need not be the same size,
and pixels are passed as counts, since no method looks at neighbours. `out` may be
`src`. `params.amount` blends the result with the source,
`source + amount (transfer - source)`; 0 (or NULL params) reads as 1, the full transfer.

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

## Sharpening

```c
typedef enum {
    ALWAN_SHARPEN_UNSHARP_MASK = 0,
    ALWAN_SHARPEN_UNSHARP_MASK_BOX = 1
} alwan_sharpen_method;

alwan_status alwan_sharpen_{T}(alwan_{T} *out, size_t out_row_stride,
                               alwan_{T} const *src, size_t src_row_stride, size_t channels,
                               size_t width, size_t height,
                               alwan_sharpen_method method, alwan_sharpen_params const *params);
alwan_status alwan_sharpen_u8(...);   /* same, unsigned char pixels */
```

The detail of an image amplified. 1 to 4 channels, each sharpened on its own; `out` may be
`src`. Sharpening R, G and B apart can fringe colour at edges; sharpen a lightness channel
for a gentler result. The float entry points run `UNSHARP_MASK` and `alwan_sharpen_u8`
runs `UNSHARP_MASK_BOX`; each refuses the other, since each follows a reference that
works in that type.

| Field of `alwan_sharpen_params` | Method | 0 reads as |
|---|---|---|
| `radius` | `UNSHARP_MASK` / `UNSHARP_MASK_BOX` | 1 / 2, the Gaussian's standard deviation in pixels |
| `amount` | `UNSHARP_MASK` / `UNSHARP_MASK_BOX` | 1 / 1.5 (it may be negative, which softens) |
| `clip` | `UNSHARP_MASK` | 0: no clipping; non-zero clips as scikit-image does |
| `threshold` | `UNSHARP_MASK_BOX` | 3 levels; a negative value is 0, every difference |

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
    ALWAN_QUANTIZE_MAX_COVERAGE = 2
} alwan_quantize_method;

alwan_status alwan_quantize_u8(unsigned char *palette_out, size_t *count_out,
                               unsigned int *index_out,
                               unsigned char const *rgb, size_t pixel_stride,
                               size_t count, size_t max_colors, alwan_quantize_method method);
```

A palette of at most `max_colors` entries for a photograph, and optionally each pixel's
entry. An unknown method is `ALWAN_E_INVALID`.

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
