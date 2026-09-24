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
    ALWAN_DENOISE_TV_BREGMAN = 6
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
one portrait). `alwan_denoise_u8` runs all seven; `alwan_denoise_{T}` runs
`TV_CHAMBOLLE`, `DCT`, `WAVELET`, `MEDIAN` and `TV_BREGMAN`, and returns `ALWAN_E_INVALID` for the two whose references work
on 8-bit data. 1 to 4 channels; `out` may alias `src` except for `NL_MEANS`.

| Field of `alwan_denoise_params` | Method | 0 reads as |
|---|---|---|
| `weight` | `TV_CHAMBOLLE` | 0.1 |
| `tolerance` | `TV_CHAMBOLLE` | 2e-4 (a tiny value runs every iteration) |
| `iterations` | `TV_CHAMBOLLE` / `ANISOTROPIC_DIFFUSION` | 200 (at most) / 10 |
| `h` | `NL_MEANS` | 10, in 0..255 units |
| `template_window`, `search_window` | `NL_MEANS` | 7, 21 |
| `alpha`, `k` | `ANISOTROPIC_DIFFUSION` | 0.15, 0.05 |
| `sigma` | `DCT` / `WAVELET` | 10 for 8-bit data, 10 / 255 for floats / estimated from the image |
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
    ALWAN_MORPHOLOGY_TOP_HAT = 5, ALWAN_MORPHOLOGY_BLACK_HAT = 6
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
