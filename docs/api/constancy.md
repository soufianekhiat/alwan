# Illuminant Estimation

The colour of the light, estimated from the picture itself. The camera-side functions
(`alwan_idt_white_balance`, `alwan_best_illuminant`) derive white balance from spectral
sensitivities; these work on an image with nothing else known about it.

## The family

van de Weijer, Gevers and Gijsenij, "Edge-Based Color Constancy", IEEE Transactions on
Image Processing 16(9), 2007, write the classic estimators as one expression:

    e(n, p, sigma) = ( sum over x of | d^n f_sigma / dx^n |^p ) ^ (1 / p)

per channel, over the pixels that are not excluded, then scaled to unit length. `f_sigma`
is the image smoothed by a Gaussian of `sigma` pixels.

| order | minkowski | sigma | estimator |
|---|---|---|---|
| 0 | 1 | 0 | Grey World (Buchsbaum 1980): the average scene is grey |
| 0 | 0 | 0 | White Patch, max-RGB: the brightest value per channel is white |
| 0 | p | 0 | Shades of Grey (Finlayson and Trezzi 2004); p = 6 is the usual choice |
| 0 | p | > 0 | general Grey World |
| 1 | p | > 0 | first-order Grey-Edge: the average edge is grey |
| 2 | p | > 0 | second-order Grey-Edge |

A Minkowski power of 0 selects the maximum (p = infinity). The first-order magnitude is
`sqrt(fx^2 + fy^2)` and the second-order one `sqrt(fxx^2 + 4 fxy^2 + fyy^2)`, each
channel on its own.

```c
typedef struct {
    int order;               /* 0 the pixels, 1 the gradient, 2 the second derivatives */
    alwan_f64 minkowski;     /* p >= 1; 0 takes the maximum */
    alwan_f64 sigma;         /* Gaussian scale in pixels; 0 is no smoothing (order 0 only) */
    alwan_f64 saturation;    /* a pixel whose largest channel reaches this is left out; 0 leaves none out */
} alwan_constancy_params;

void alwan_constancy_params_init(alwan_constancy_params *params);   /* Grey World */

alwan_status alwan_illuminant_estimate_{T}(alwan_{T} illuminant_out[3],
                                           alwan_{T} const *rgb, size_t row_stride,
                                           size_t width, size_t height,
                                           unsigned char const *exclude, size_t exclude_row_stride,
                                           alwan_constancy_params const *params);

alwan_status alwan_illuminant_correct_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                                         alwan_{T} const *in, size_t in_stride,
                                                         size_t count, alwan_{T} const illuminant[3]);
```

The image is three values a pixel, rows `row_stride` bytes apart. The estimate is in
whatever linear RGB the image is in; estimate in linear light, since a transfer curve
bends the averages the estimators rely on.

## What is left out

- a border of `sigma + 1` pixels, where the filters would read replicated edge values;
- every pixel whose largest channel reaches `saturation`, with its eight neighbours: a
  clipped highlight reports the sensor's limit, not the light. Use the clipping level of
  the source (255 for 8-bit, 1.0 for a normalised image that clips), or 0 for linear
  float images with no clipping level;
- the pixels the caller marks in `exclude` (one byte a pixel, nonzero to leave out),
  for an object of known colour or a region lit by a second source.

## Filtering

The Gaussian and its derivatives are sampled over `+-floor(3 sigma + 0.5)` taps. The
Gaussian sums to 1, the first derivative is scaled so that `sum(x g) = 1`, and the second
has its mean removed and is scaled so that `sum(x^2 g / 2) = 1`. Filtering is separable
correlation with the image edge replicated, x first. At order 1 or 2, sigma must be at
least 1/6 so the kernel has more than one tap.

## Correction

`alwan_illuminant_correct` divides each channel by `illuminant * sqrt(3)`, the von Kries
diagonal correction, which maps a light of equal channels (1, 1, 1) / sqrt(3) to itself.
In place is allowed.

## Errors

`ALWAN_E_INVALID` for a NULL, a zero width or height, a row stride under three values a
pixel, an order outside 0 to 2, a Minkowski power that is negative or between 0 and 1, a
negative or non-finite sigma, a sigma below 1/6 at order 1 or 2, or a non-finite pixel.
`ALWAN_E_RANGE` when no pixel is left to estimate from, or all of them are zero.

## Testing

The reference is the authors' MATLAB code, `general_cc.m` with `gDer`, `norm_derivative`,
`dilation33`, `set_border` and `fill_border`, as mirrored at
<https://github.com/lynnprosper/Edge-Based-Color-Constancy>. It carries no licence, so
nothing of it is in alwan: the implementation is written from the paper, and the code is
fetched at a pinned commit and run in MATLAB as the oracle
(`gendata/tests/constancy_reference.py`). Suite 183 compares 16 settings over two
images, covering every estimator in the table, a caller mask, clipped pixels, and sigmas
whose border is not a whole number of pixels: 4.4e-16 worst, 3.3e-8 through the f32
forms, and the corrected image bit for bit.

The code fixes its saturation level at 255; alwan makes it a parameter.

## Grey Pixel and weighted Grey-Edge

Two estimators outside e(n, p, sigma), chosen by `method` in the same parameters (append-only
fields: zero the struct or call `alwan_constancy_params_init`).

`ALWAN_CONSTANCY_GREY_PIXEL` is Yang, Gao and Li, "Efficient Illuminant Estimation for Color
Constancy Using Grey Pixels" (CVPR 2015): a grey surface keeps the same local contrast in every
channel's logarithm whatever the light, so the pixels whose three log contrasts agree best are
taken as grey and averaged. As the authors' code computes it:

- each channel's zeros become `DBL_EPSILON`, then its natural logarithm is measured for local
  contrast: the sample standard deviation over `grey_pixel_window` x `grey_pixel_window` (3)
  with the edge replicated (`ALWAN_GREY_PIXEL_LOCAL_STD`, their GPstd), or the gradient
  magnitude of their 2D Gaussian derivative at `sigma` (`ALWAN_GREY_PIXEL_EDGE`, GPedge);
- the grey index is the three contrasts' standard deviation (n - 1) over their mean, over the
  pixel's mean intensity, scaled by its maximum; a pixel with no contrast at all takes the
  maximum, and the index is averaged over 7 x 7 with the image wrapped round;
- the `grey_pixel_percent` % lowest (0.1; at least one pixel; every pixel tied with the last
  one taken) are summed per channel.

`exclude` and `saturation` (dilated 3 x 3) mark pixels as never grey; there is no border.

`ALWAN_CONSTANCY_WEIGHTED_GREY_EDGE` is Gijsenij, Gevers and van de Weijer, "Improving Color
Constancy by Photometric Edge Weighting" (TPAMI 2012): the first-order Grey-Edge, with each
pixel's edge weighted by the share of it that runs along a photometric direction, and iterated
because that direction depends on the light being estimated. Each pass divides the image by the
estimate so far times sqrt 3, takes the first-order Gaussian derivatives at `sigma`, and weights
each channel's gradient magnitude by (|quasi-variant| / |gradient|)^`kappa` (1), capped at 1:

- `ALWAN_EDGE_WEIGHT_SPECULAR`: the derivative along (R + G + B) / sqrt 3, the illuminant's
  direction once corrected, which highlights follow; the paper's best;
- `ALWAN_EDGE_WEIGHT_SHADOW`: the derivative along the pixel's own colour (the Gaussian-smoothed
  colour at sigma), which shadows and shading follow.

The pass's estimate is the Minkowski sum (`minkowski` p, 0 the maximum) over the pixels inside
a sigma + 1 border, not excluded, not saturated and with some gradient; the result is the
product of the passes' estimates, unit length, after `iterations` (10) passes or as soon as a
pass lands within 0.05 degrees of neutral. A pixel with no gradient has weight 0, where the
reference divides 0 by 0.

| field | Default | Read by |
|---|---|---|
| `method` | `ALWAN_CONSTANCY_EDGE_FRAMEWORK` | all |
| `grey_pixel_percent` | 0.1 | GREY_PIXEL |
| `grey_pixel_measure`, `grey_pixel_window` | LOCAL_STD, 3 (odd, 3 to 255) | GREY_PIXEL |
| `sigma` | required (> 0) | GREY_PIXEL with EDGE, WEIGHTED_GREY_EDGE |
| `kappa`, `edge_weight`, `iterations` | 1, SPECULAR, 10 | WEIGHTED_GREY_EDGE |
| `minkowski`, `saturation` | as above | WEIGHTED_GREY_EDGE; `saturation` also GREY_PIXEL |

Suite 183 holds Grey Pixel to the authors' MATLAB code (`GetGreyidx.m`, `DerivGauss.m`,
`LocalStd.m`, at <https://github.com/jackygsb/Efficient-Illuminant-Estimation-for-Color-Constancy-Using-Grey-Pixels>,
no licence: fetched at a pinned commit and run in MATLAB, nothing copied) over 11 settings on
three images with zeros, a flat patch and a caller mask: 1.7e-15 worst. The weighted Grey-Edge
is held to an MIT-licensed Python port of `weightedGE.m` for the specular weighting (6
settings) and, for the shadow weighting, the maximum and the mask, which the port does not
have, to a transcription of the same iteration that agrees with the port to 1e-12 where both
apply (6 settings): 1.1e-15 worst. The f32 forms equal the f64 ones on the pixels narrowed to
float. The weighted Grey-Edge's saturation test reads the image as given, once; the port tests
the corrected image for 255 at every pass.

## White balance, as OpenCV's xphoto

```c
alwan_status alwan_white_balance(void *out, size_t out_row_stride, void const *src, size_t src_row_stride,
                                 alwan_pixel_format format, size_t channels, size_t width, size_t height,
                                 alwan_white_balance_method method, alwan_white_balance_params const *params,
                                 alwan_f64 gains_out[3]);
```

A port of the three white balancers in opencv_contrib 5.0.0's xphoto module (Apache-2.0;
the notice is in `alwan_white_balance.c`, the licence beside the model in
`src/alwan/data/opencv/`). Unlike the estimators above, these work on the image in its
own encoding and return it balanced, as xphoto does.

| Method | xphoto | Formats | What it does |
|---|---|---|---|
| `ALWAN_WHITE_BALANCE_SIMPLE` | `SimpleWB` | U8, F32; 1 to 4 channels | Per channel, the p percent darkest and brightest values are found in a histogram (256 bins for U8, 4096 for F32) and the range between them is stretched onto `[output_min, output_max]`. |
| `ALWAN_WHITE_BALANCE_GRAYWORLD` | `GrayworldWB` | U8, U16; 3 or 4 channels | Gains from the channel sums over the pixels whose saturation `(max - min) / max` is at most `saturation_threshold` (0.9). |
| `ALWAN_WHITE_BALANCE_LEARNING_BASED` | `LearningBasedWB` | U8, U16; 3 or 4 channels | Cheng, Price, Cohen and Brown, CVPR 2015: four chromaticity features (the mean, the brightest pixel, the dominant histogram bin, the mode of the 300 commonest bins) fed to OpenCV's forest of 20 regression trees per feature for the light's chromaticity. |

The grey world here and the one in the family above differ in what they leave out:
xphoto drops colourful pixels, `alwan_illuminant_estimate` drops clipped ones.

The two gain methods apply their gains as xphoto's `applyChannelGains` does, scaled so the
largest is 1 and in fixed point: `(v * round(gain * 256)) >> 8` at 8 bits, `>> 16` at 16.
`gains_out`, when given, receives those normalised gains, R, G, B; `out` may be NULL to
estimate only. SIMPLE processes every channel, alpha included, as xphoto does; the other
two copy an alpha through.

Parameters, zero being the default:

| Field | Method | Default |
|---|---|---|
| `p` | SIMPLE | 2 (percent at each end); a negative p cuts nothing, as xphoto's p = 0 |
| `input_min`, `input_max`, `output_min`, `output_max` | SIMPLE | 0 and the format's top (255, or 1.0 for F32) |
| `saturation_threshold` | GRAYWORLD, LEARNING_BASED | 0.9, 0.98 |
| `range_max` | LEARNING_BASED | the format's top, 255 or 65535 |
| `hist_bin_num` | LEARNING_BASED | 64 |

Where alwan and xphoto part:

- xphoto's LearningBasedWB keeps `range_max` at 255 for 16-bit images, which leaves out
  nearly every pixel; alwan's default is 65535. Pass 255 for xphoto's behaviour.
- With fewer than 300 occupied bins xphoto reads past its palette; alwan takes the mode
  over the bins there are. When SIMPLE's p asks for more values than fall inside the input
  range, xphoto's search walks off its histogram; alwan stops at its ends.
- xphoto's 16-bit gain loop overflows a signed int on its last few pixels (when the pixel
  count is not a multiple of 8); alwan keeps the unsigned result the vector loop computes.
- SIMPLE's final step reproduces OpenCV's AVX2 `convertTo`: a fused multiply-add on each
  whole block of 16 values, unfused on the rest. An OpenCV that takes another vector width
  can differ in the last place of an F32 value, or by one 8-bit level at a rounding tie.

Errors: `ALWAN_E_INVALID` for a NULL src, a NULL out with no `gains_out` (or any NULL out for
SIMPLE), a zero size, a stride too small, a format or channel count the method does not
take, an unknown method, a non-finite parameter or F32 pixel, `input_max` below
`input_min`, a `saturation_threshold` outside [0, 1], a `range_max` outside 1 to the
format's top, or `hist_bin_num` outside 1 to 256. `ALWAN_E_RANGE` when LEARNING_BASED has
no pixel under its threshold, or the image has more than `INT_MAX` pixels; `ALWAN_E_NOMEM`
when its histogram does not fit.

Suite 273 compares twelve procedural cases with cv2 5.0.0's xphoto (IPP off,
`gendata/tests/white_balance_reference.py`): every output value is equal, in the ordinary
and the deterministic builds. Over the 164 SRIC photographs (a third of their size) all
three methods at 8 and 16 bits and SIMPLE in float are equal too, with IPP on or off,
except where xphoto's own search leaves its histogram.
