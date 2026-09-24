# Image tools

Whole-image operations for photographs and frames: edge-aware smoothing, denoising, local
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

## Morphology

```c
typedef enum {
    ALWAN_MORPHOLOGY_ERODE = 0, ALWAN_MORPHOLOGY_DILATE = 1,
    ALWAN_MORPHOLOGY_OPEN = 2, ALWAN_MORPHOLOGY_CLOSE = 3,
    ALWAN_MORPHOLOGY_GRADIENT = 4,
    ALWAN_MORPHOLOGY_TOP_HAT = 5, ALWAN_MORPHOLOGY_BLACK_HAT = 6,
    ALWAN_MORPHOLOGY_AREA_OPEN = 7, ALWAN_MORPHOLOGY_AREA_CLOSE = 8,
    ALWAN_MORPHOLOGY_DIAMETER_OPEN = 9, ALWAN_MORPHOLOGY_DIAMETER_CLOSE = 10,
    ALWAN_MORPHOLOGY_FILL_HOLES = 11
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
| `iterations` | 1: each erosion and dilation runs once |
| `kernel` | NULL: build `shape`; otherwise the caller's element, `kernel_width x kernel_height` bytes |
| `area_threshold` | 64: `AREA_OPEN` and `AREA_CLOSE`, the smallest region kept, in pixels |
| `connectivity` | 4: `AREA_*`, `DIAMETER_*` and `FILL_HOLES`, 4 or 8 neighbours |
| `diameter_threshold` | 8: `DIAMETER_OPEN` and `DIAMETER_CLOSE`, the shortest region kept, as the longer side of its bounding box |

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
| `FIELD` | a sampled map, two doubles per texel: texel `(i, j)` holds the source point of output point `(i + 0.5, j + 0.5)`, read bilinearly between texels and clamped at the edges; a NaN texel has no source (fill) |
| `CALLBACK` | `callback(x, y, &sx, &sy, user)`; a zero return means no source (fill) |

A field is the form a flow field, a UV pass from a renderer or a lens-distortion table
takes; a callback takes any analytic map.

| Field of `alwan_warp_params` | 0 reads as |
|---|---|
| `matrix[8]` | all 0: the identity |
| `perspective` | 0: the affine map, `m6` and `m7` unused |
| `fill[4]` | 0 in every channel |
| `map` | `ALWAN_WARP_MAP_MATRIX`: the matrix above |
| `swirl_center[2]`, `swirl_radius`, `swirl_angle` | the image centre, half the shorter side, no turn |
| `field`, `field_row_stride` | required by `ALWAN_WARP_MAP_FIELD` |
| `callback`, `callback_user` | required by `ALWAN_WARP_MAP_CALLBACK` |
| `integration` | `ALWAN_PIXEL_INTEGRATE_POINT`: one sample at the centre |
| `kernel` | `ALWAN_PIXEL_KERNEL_BOX`: the pixel's mean |
| `samples` | 16 x 16 for `GRID`, 16 for `R2`, a cap of 64 for `ADAPTIVE` |
| `tolerance` | 0.05 source pixels |
| `seed`, `r2_disk` | seed 0, points over the square pixel (`r2_disk` applies to the box only) |
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
| `R2` | `samples` points (16 by default, up to 4096) of Roberts' R2 sequence in antithetic pairs `+d, -d`, so no pair shifts the pixel; the set is shifted toroidally per pixel by a hash of `(x, y, seed)` so neighbours do not share one pattern. `r2_disk` spreads it over the disk of the pixel's area instead, turned per pixel |
| `ADAPTIVE` | R2 with a count per pixel, from central differences of the map at half a pixel: the footprint (the Jacobian's largest column) and the second difference. A footprint at most 1 with a second difference at most `tolerance` takes one sample; otherwise the first of 4, 8, 16, 32, 64 not below the larger of footprint^2 and second difference / tolerance, capped by `samples`. A kernel other than the box takes 16 at least and four times the count |

`ADAPTIVE` reads the map, not the picture: a rigid turn, a translation or a mild
enlargement takes one sample even where the content has sharp edges, and the budget goes
where the map shrinks or bends. `samples_out` (one byte per pixel, 255 for more) records
the count each pixel took, for a heat map. On a 256 x 256 checker of 8-pixel squares under
a swirl of three turns, against a 48 x 48 grid, bilinear reconstruction:

| Integration | Mean samples | RMS inside the swirl |
|---|---|---|
| `POINT` | 1 | 0.349 |
| `R2`, 16 | 16 | 0.059 |
| `GRID`, 4 | 16 | 0.031 |
| `ADAPTIVE`, cap 64 | 37.7 | 0.020 |
| `R2`, 64 | 64 | 0.017 |
| `GRID`, 16 | 256 | 0.0009 |

`kernel` sets the weight each sub-position takes, in output pixels around the centre:

| Kernel | Weight | Support |
|---|---|---|
| `BOX` | 1: the pixel's mean | the pixel |
| `TENT` | `(1 - abs(dx)) (1 - abs(dy))` | two pixels each way |
| `GAUSSIAN` | `exp(-r^2 / (2 s^2))`, `s = 0.5` | a disk of radius 1.5 |

`GRID` lays its cells over the kernel's support and weights them; `R2` draws its points
from the kernel through its inverse distribution (Box-Muller for the Gaussian, cut at
`3 s`), so every point weighs the same and the antithetic pairs still cancel. The box is
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

With `alpha_channel` set, the last of two or four channels is straight alpha: the colour is
premultiplied before the mean and divided after, so colour under zero alpha does not bleed
into an edge. The mean is of the data's own values; for light-correct integration of
display-encoded colour, convert to linear light first.

No library carries this integration to compare with; suite 235 holds its properties
instead: `POINT` through a callback or a field that encode a matrix equals the matrix path,
`GRID` with one sample equals `POINT`, antithetic R2 returns a linear ramp to 3e-16, R2
converges to the grid reference under a swirl (RMS 0.029, 0.0032, 0.0006 at 16, 256 and
2048 points), `ADAPTIVE` takes one sample past the swirl and more inside it, a zero angle is
the identity, and a NaN texel is fill. For the tent and the Gaussian, a linear ramp comes
back to 1e-15 through the grid and through R2, one grid cell is the point, R2 converges to
the weighted grid (RMS 0.037 and 0.040 at 16 points, 0.0015 and 0.0019 at 1024), and
`ADAPTIVE` never takes fewer than 16 points. Resize's integration and subpixel layouts are held
the same way: an integrated box halving is the 2 x 2 mean, a constant stays constant under
every layout, and RGB and BGR move a thin line's red and blue in opposite directions.

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

There is no reference implementation to compare with, so suite 192 makes haze with a
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
