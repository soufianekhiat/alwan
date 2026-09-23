# Test Patterns

Reference images alwan renders itself, at any size, for debugging, calibration and
reference: the colour bar signals of ITU-R BT.471-1, the ARIB STD-B28 multiformat
colour bar, and the EBU Tech 3325 monitor measurement patterns.

```c
alwan_status alwan_pattern_render_{T}(alwan_{T} *rgb_out, size_t row_stride, size_t width, size_t height,
                                      alwan_pattern pattern, alwan_pattern_params const *params);
alwan_status alwan_pattern_render_planar_{T}(alwan_{T} *r_out, size_t row_stride, alwan_{T} *g_out,
                                             alwan_{T} *b_out, size_t width, size_t height,
                                             alwan_pattern pattern, alwan_pattern_params const *params);
```

A pattern is written as its native R'G'B' signal in fractions of white: 0 is black, 1 is
100 % white. Where a pattern goes below black it stays below: ARIB's -2 % PLUGE step is
-0.02, and clipping it would remove the step it exists to show. `row_stride` is in bytes,
at least `width` x 3 values for the interleaved form and `width` values for the planar
one, so padded rows and sub-images work.

Every stripe edge is the pattern's own fraction of the width, rounded to the nearest
sample, and every band edge the same fraction of the height, so a pattern keeps its
proportions at any size.

| Pattern | Source |
|---|---|
| `ALWAN_PATTERN_BARS_100_0_100_0` | ITU-R BT.471-1 (a): eight bars at 100 % |
| `ALWAN_PATTERN_BARS_100_0_75_0` | ITU-R BT.471-1 (b), the EBU colour bars |
| `ALWAN_PATTERN_BARS_100_0_100_25` | ITU-R BT.471-1 (c) |
| `ALWAN_PATTERN_BARS_75_7_5_75_7_5` | ITU-R BT.471-1 (d), 7.5 % setup |
| `ALWAN_PATTERN_ARIB_STD_B28` | ARIB STD-B28 multiformat colour bar, the basis of SMPTE RP 219 |
| `ALWAN_PATTERN_EBU_1` | EBU Tech 3325 EBU_1: peak white and four black patches on 50 % grey |
| `ALWAN_PATTERN_EBU_2` | EBU_2: EBU_1 with the white patch at 109 % super white |
| `ALWAN_PATTERN_EBU_3` | EBU_3-1 to 3-13: a white patch at one measurement point, on black |
| `ALWAN_PATTERN_EBU_3_WINDOW` | EBU_3-1_4, _10, _25, _81: a centred white window |
| `ALWAN_PATTERN_EBU_3_BLACK` | EBU_3-black |
| `ALWAN_PATTERN_EBU_3_WHITE` | EBU_3-white: peak white frame |
| `ALWAN_PATTERN_EBU_4` | EBU_4-1 to 4-20: one grey-scale patch, on black |
| `ALWAN_PATTERN_EBU_5` | EBU_5: a BT.709 primary or one of the 15 EBU test colours, on black |
| `ALWAN_PATTERN_EBU_12_GREY` | EBU_12-grey: 50 % grey frame |
| `ALWAN_PATTERN_PLUGE_BT814` | ITU-R BT.814-4 Annex 2 PLUGE, for HDTV, UHDTV and HDR |
| `ALWAN_PATTERN_BT1729_SWEEP_H` | ITU-R BT.1729 zone 8: horizontal frequency sweep |
| `ALWAN_PATTERN_BT1729_SWEEP_V` | ITU-R BT.1729 zone 14: vertical frequency sweep |
| `ALWAN_PATTERN_BT1729_STAIRCASE` | ITU-R BT.1729 zone 11: luminance staircase in 10 % steps |

## ITU-R BT.471-1

The recommendation names a bar signal by four numbers: the white bar, the black bar, and
the highest and lowest primary level in the coloured bars, in percent of white. The eight
bars run white, yellow, cyan, green, magenta, red, blue, black, each an eighth of the width.

## ARIB STD-B28

Four bands, 7/12, 1/12, 1/12 and 3/12 of the height, so 630, 90, 90 and 270 lines at 1080.
The side panels are 1/8 of the width and the seven bars 3/28 each.

1. 75 % bars, 40 % grey side panels.
2. 100 % cyan, the user's choice, 75 % white, 100 % blue. The choice, in
   `alwan_pattern_params.b28_choice`, is 75 % white (the default), 100 % white, or the +I
   signal, R'G'B' 41.2545 / 16.6946 / 0 IRE.
3. 100 % yellow, a ramp from black to white, 100 % red. The ramp climbs one 10-bit code per
   sample at 1920 samples, 876 samples wide and centred, with black before it and white after.
4. 15 % grey, black, 100 % white, black, the PLUGE steps -2, 0, +2, 0 and +4 %, black,
   15 % grey.

The standard gives its levels as BT.709 Y', PB, PR in mV and as 10-bit codes. Through the
BT.709 Y'CbCr matrix, alwan's R'G'B' reproduces every one of its 19 levels within its
0.1 mV rounding, and every 10-bit code exactly; each stripe lands within a sample of the
standard's Table A-5 at 1920 (suite 138).

## EBU Tech 3325

The patterns a studio monitor is measured with. A 10-bit code c reads as (c - 64) / 876,
so black (64) is 0, 50 % grey (502) is 0.5, peak white (940) 1 and super white (1019)
955/876.

A patch is a square of H/7.5 samples, 1 % of a 16:9 picture, centred on one of the
standard's 13 measurement points (Figure 7): the centre, (0, ±0.4 H), (±0.4 W, 0),
(±0.2 W, ±0.2 H) and (±0.4 W, ±0.4 H). A window of p % keeps the picture's aspect, each
side sqrt(p %) of the picture's. Every edge rounds to the nearest sample.

The series numbers are the standard's own, in `alwan_pattern_params`; 0 reads as the
first of each series and a number outside it is `ALWAN_E_INVALID`:

| Field | Pattern | Values |
|---|---|---|
| `ebu_point` | `EBU_3` | measurement point 1 to 13 |
| `ebu_area` | `EBU_3_WINDOW` | 4, 10, 25 or 81 % (1 % is `EBU_3` point 1) |
| `ebu_step` | `EBU_4` | Table 5 step 1 to 20, codes 64, 86, 138 ... 918, 940, 1019 |
| `ebu_colour` | `EBU_5` | 1 to 15 the EBU test colours of Table 7, 16 red, 17 green, 18 blue (Table 6) |

The EBU_5 colours are defined as D'Y, D'CB, D'CR codes; alwan decodes them with the
four-digit BT.709 coefficients the standard encodes with (Annex 3). The codes are
quantised, so a primary's other two channels come out a little off 0 (red's blue is
-0.00098) and stay there.

The EBU publishes the patterns as 1920 x 1080 files. At that size every sample of all 59
BT.709 SDR files of EBU_1 to EBU_5 carries the same 10-bit codes as alwan's render, and at
3840 x 2160 the rectangles double. Shown on a 2.35 gamma BT.709 display, the colours give
the L, u', v' of Tables 6 and 7 within their printed rounding (suite 139). The EBU_2 file
also carries a caption, which alwan does not draw. EBU_12-burn is not implemented: the
standard gives its two levels but not the shape of the transition between them.

## ITU-R BT.814-4 PLUGE

The signal a monitor's black level is set with. On a black field: twenty narrow stripes on
the left, ten at a level slightly above black and ten slightly below, a higher level patch
at the centre for the gain control, and two coarse stripes on the right, one above black
and one below. Set the black level so the darker stripes just disappear while the lighter
ones stay visible.

Levels are 10-bit codes read as (c - 64) / 876: higher level 940, black 64, slightly
lighter 80, slightly darker 48 (Table 2). `alwan_pattern_params.pluge_range` picks the
higher level patch: `ALWAN_PATTERN_PLUGE_SDR` (940, the default) or
`ALWAN_PATTERN_PLUGE_HDR` (399, which Table 3 gives for PQ and HLG alike). Nothing else in
the frame changes between the two.

Table 4 gives the sample numbers of every vertical edge and Tables 5 and 6 the line
numbers of every horizontal one. HDTV, 4K and 8K place them at the same fractions of the
picture, so alwan keeps the fractions and rounds each edge to the nearest sample, which
reproduces the published numbers exactly at those three sizes and keeps the proportions
at any other. A narrow stripe is 10 of the 1080 lines and sits on a pitch of twice that,
starting at Lc and ending at Lh.

At 1920 x 1080 and 3840 x 2160 every sample of alwan's render carries the code the
recommendation's tables place there, which is also the code in the EBU's own PLUGE files
(suite 140).

## ITU-R BT.1729

The recommendation defines a composite pattern of fifteen zones. alwan renders the three
that its text specifies numerically: the frequency sweeps of zones 8 and 14, and the
staircase of zone 11.

The sweeps are linear: the frequency rises evenly across the picture, so the phase is the
integral of that rise. The recommendation states the sweep in megahertz, once per system,
but those are one sweep read through each system's sampling clock. In samples of the
picture width it runs 120 to 1920, and in lines of the picture height 64 to 1080; a sample
or a line is half a cycle, so the horizontal sweep carries 60 to 960 cycles across the
picture and the vertical 32 to 540 down it. Tables 2 and 3 place each system's Nyquist and
0.8 x Nyquist markers as a percentage of the sweep, and those percentages follow from
those ranges: alwan reproduces all sixteen of them to better than a tenth of a point, and
the rendered sweep carries the stated frequency where each marker falls (suite 141).

Being defined in cycles across the picture, a sweep is the same signal at any size: past
each system's own Nyquist it aliases, which is what the markers are there to show.

The staircase is eleven steps of equal width, black to white in tenths.

What is left out, and why: the recommendation fixes the sweep frequencies but not their
amplitude, so the level here is alwan's own, the sinusoid filling the range from black to
white. Zone 2 is user text, zone 12 is a bar that moves with time, and the boundaries of
the fifteen zones are given in figures rather than numbers, so the composite pattern is
not rendered.

## Colour space and layout

The signal is the pattern's own. To place the bars in another space, decode and convert
them with the RGB conversion functions ([color-spaces.md](color-spaces.md)); to reach
integer or half-float buffers, the `_ex` pixel formats of the map API take the rendered
floats ([map.md](map.md)).

## A pattern to print: convert the palette, not the pixels

```c
alwan_f64 palette[64 * 3], cmyk_of[64 * 4];
size_t n;
alwan_palette_extract_f64(palette, 64, &n, rgb, 0, width * height);   /* the bars: 8 colours */
/* convert the n entries: decode, to XYZ, adapt to D50, to Lab, alwan_cmyk_inverse_eval */
alwan_palette_apply_f64(cmyk, 0, rgb, 0, width * height, palette, n, cmyk_of, 4);
```

A colour bar, a card or any flat artwork is a handful of colours, and the conversions
worth having on the way to a press are expensive per pixel and cheap per colour: the
CMYK inverse ([reference-data.md](reference-data.md)), a spectral upsampling. So the
route is the palette's. `alwan_palette_extract_{T}` collects the distinct colours of an
image in order of first appearance, comparing exactly, which is what a rendered pattern
allows; more than `max_colors` returns `ALWAN_E_RANGE` with `count_out = max_colors + 1`,
because a sweep or a photograph is not a palette and the scan stops rather than growing
quadratically (the BT.1729 sweeps have 320 and 180 distinct values on a 320 x 180 render,
the ARIB card 163, the bars 8, the PLUGE 4). `alwan_palette_apply_{T}` writes, per pixel,
the `channels` converted values of the entry the pixel's colour has in the palette, so
the converted palette becomes the converted image in one pass, four channels for CMYK.
Suite 138.

## A palette from a photograph: median cut

A photograph is not a palette, but it can be given one. `alwan_palette_median_cut_u8` is
Heckbert's median cut (SIGGRAPH 1982) as Pillow's `Image.quantize(method=MEDIANCUT)`
computes it, on 8-bit RGB:

```c
alwan_status alwan_palette_median_cut_u8(unsigned char *palette_out, size_t *count_out,
                                         unsigned int *index_out,
                                         unsigned char const *rgb, size_t pixel_stride,
                                         size_t count, size_t max_colors);
```

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

## Getting a pattern to Y'CbCr codes

A rendered pattern is already R'G'B', so it goes to Y'CbCr without a transfer function in
between. Three calls do it, and the whole picture is one `count` because the map API is
told a stride rather than a shape:

```c
size_t const count = width * height;
alwan_f64 *rgb    = malloc(count * 3 * sizeof *rgb);
alwan_f64 *ycbcr  = malloc(count * 3 * sizeof *ycbcr);
uint16_t  *codes  = malloc(count * 3 * sizeof *codes);
size_t const stride = 3 * sizeof(alwan_f64);   /* bytes, always explicit */

alwan_pattern_render_f64(rgb, width * stride, width, height,
                         ALWAN_PATTERN_ARIB_STD_B28, NULL);
alwan_rgb_to_ycbcr_f64_map_interleave(ycbcr, stride, rgb, stride, count,
                                      ALWAN_YCBCR_BT709);
alwan_ycbcr_full_to_legal_f64_map_interleave(ycbcr, stride, ycbcr, stride, count, 10);
```

`row_stride` is in bytes and so is every `_map_interleave` stride. Pass them explicitly:
most of alwan takes a stride of 0 literally and writes one value rather than treating it
as "packed".

### What the numbers mean at each step

`alwan_rgb_to_ycbcr_{T}` gives `Y` in `[0, 1]` and `Cb` / `Cr` **centred on 0.5 in
`[0, 1]`**, not on zero: 100 % white is `Y 1.0, Cb 0.5, Cr 0.5`. That is the convention
throughout alwan, and it is why normalising a YCbCr value is a no-op
([../ranges.md](../ranges.md)).

`alwan_ycbcr_full_to_legal_{T}` returns a fraction of the peak code rather than the code
itself, so a value multiplied by `(1 << bit_depth) - 1` is the code the standard names:

| `bit_depth` | black | white | neutral chroma |
|---|---|---|---|
| 8 | 16 | 235 | 128 |
| 10 | 64 | 940 | 512 |
| 12 | 256 | 3760 | 2048 |
| 16 | 4096 | 60160 | 32768 |

A pattern that goes below black keeps its sign through both calls, which is the point of
rendering it: ARIB's -2 % PLUGE step reaches `Y` below 64 at 10 bits, and clipping it
anywhere in this chain removes the step being measured.

### Reaching an integer buffer

`alwan_scatter3_{T}` writes float triplets into a typed buffer, scaling `[0, 1]` onto the
format's full range: `ALWAN_PIXEL_U8` onto `[0, 255]` and `ALWAN_PIXEL_U16` onto
`[0, 65535]`. That scale is the format's, not the bit depth you asked legal range for, so
the two compose exactly only when they agree:

```c
/* 8-bit legal codes: full_to_legal(8) scales by 255, U8 scales by 255. Exact. */
alwan_ycbcr_full_to_legal_f64_map_interleave(ycbcr, stride, ycbcr, stride, count, 8);
alwan_scatter3_f64(bytes, 3, ycbcr, stride, count, ALWAN_PIXEL_U8);

/* 16-bit legal codes: likewise, both scale by 65535. Exact. */
alwan_ycbcr_full_to_legal_f64_map_interleave(ycbcr, stride, ycbcr, stride, count, 16);
alwan_scatter3_f64(codes, 3 * sizeof(uint16_t), ycbcr, stride, count, ALWAN_PIXEL_U16);
```

**10-bit and 12-bit have no matching pixel format**, so scattering them to `U16` would
scale by 65535 and give 16-bit codes, not 10-bit ones in a 16-bit word. Those two depths
are the caller's multiply:

```c
alwan_ycbcr_full_to_legal_f64_map_interleave(ycbcr, stride, ycbcr, stride, count, 10);
for (size_t i = 0; i < count * 3; i++) {
    double const code = ycbcr[i] * 1023.0;         /* (1 << 10) - 1 */
    codes[i] = (uint16_t)(code < 0.0 ? 0.0 : code > 1023.0 ? 1023.0 : code + 0.5);
}
```

The clamp is the caller's decision and belongs here rather than earlier: a PLUGE step
below black is a negative code that no unsigned buffer can hold, and the place to decide
what happens to it is the place that chose the container.

## Sources

ITU-R BT.471-1 is a free download from itu.int. The English translation of ARIB STD-B28
is free from arib.or.jp; alwan implements the signal the standard defines and does not
reproduce its text. SMPTE RP 219 and EG 1 are not freely available, so ARIB STD-B28 stands
in for RP 219. EBU Tech 3325 is free from tech.ebu.ch, and so are its pattern files; the
files state no licence, so alwan_dev records the rectangles and codes measured from them
rather than the files. ITU-R BT.814-4 is a free download from itu.int; its own tables give
the PLUGE, and the EBU files only check the transcription. ITU-R BT.1729 is free from
itu.int as well.

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

## Edge-aware smoothing: the guided filter

```c
alwan_status alwan_guided_filter_{T}(alwan_{T} *out, size_t out_row_stride,
                                     alwan_{T} const *src, size_t src_row_stride, size_t src_channels,
                                     alwan_{T} const *guide, size_t guide_row_stride, size_t guide_channels,
                                     size_t width, size_t height, size_t radius, alwan_{T} eps);
```

He, Sun and Tang's guided filter (ECCV 2010, TPAMI 2013): in every window of side
`2 radius + 1` the output is a linear function of the guide, `q = a . I + b`, with `a` and
`b` fitted to `src` by least squares with ridge `eps` and then averaged over the windows
covering each pixel. The image as its own guide smooths within regions and keeps edges;
a colour guide with a one-channel source (a matte, a haze transmission map, a mask from
`alwan_select_*`) snaps the source to the guide's edges. The cost does not depend on the
radius. `eps` is in the square of the guide's units: for a guide in [0, 1], 1e-3 keeps
fine edges and 0.1 smooths strongly.

It follows OpenCV's `cv::ximgproc::guidedFilter`: box means with the border reflected (the
edge pixel repeated), the colour guide's 3 x 3 covariance inverted by cofactors, and, for
`eps` below 0.01, a determinant under 1e-6 replaced by 1. It computes in double; `out`
may be `src`. Suite 190: the colour-guide cases agree with OpenCV to 1.8e-7, float
rounding; the grey-guide case to 6.1e-6, because OpenCV's SSE build takes the reciprocal
of the variance with a 12-bit approximation.

## Matching one shot to another: histogram matching

```c
alwan_status alwan_histogram_match_{T}(alwan_{T} *out, size_t out_stride,
                                       alwan_{T} const *src, size_t src_stride, size_t src_count,
                                       alwan_{T} const *ref, size_t ref_stride, size_t ref_count,
                                       size_t channels);
```

Each channel of `src` is remapped so that its cumulative distribution matches the
reference's: every distinct source value's quantile, `cumsum(counts) / n`, is looked up
in the reference's quantiles, as scikit-image's `exposure.match_histograms` does (its float
path, `numpy.interp` branch for branch). The images need not be the same size. Channels
are matched independently, so in RGB the channels drift apart; matching in a decorrelated
space (Oklab, CIELAB) carries a look more gently. `out` may be `src`. Suite 191 is bit for
bit against scikit-image: all three channels, one alone, and a single-value reference.

## Haze removal: the dark channel prior

```c
typedef struct {
    size_t patch_radius;      /* 7: the paper's 15 x 15 */
    alwan_f64 omega;          /* 0.95: the haze kept for depth */
    alwan_f64 t0;             /* 0.1: the transmission floor */
    alwan_f64 top_fraction;   /* 0.001: of the dark channel searched for the airlight */
    size_t guide_radius;      /* 30; 0 skips the refinement */
    alwan_f64 guide_eps;      /* 1e-3 */
} alwan_dehaze_params;

void alwan_dehaze_params_init(alwan_dehaze_params *params);
alwan_status alwan_dehaze_{T}(alwan_{T} *out, size_t out_row_stride,
                              alwan_{T} *transmission_out, size_t t_row_stride,
                              alwan_{T} airlight_out[3],
                              alwan_{T} const *rgb, size_t row_stride,
                              size_t width, size_t height, alwan_dehaze_params const *params);
```

He, Sun and Tang (CVPR 2009, TPAMI 2011) model a hazy image as `I = J t + A (1 - t)` and
observe that in most patches of a clear outdoor image some channel is near zero. So the
patch minimum of `I / A` measures the haze: the airlight `A` is taken among the brightest
0.1 % of the dark channel (the pixel with the largest channel mean), the transmission is
`t = 1 - omega min_patch min_c I_c / A_c`, refined here by `alwan_guided_filter` with the
image as the colour guide (the authors' own replacement for soft matting), and the scene
is `J = (I - A) / max(t, t0) + A`. Work on linear light; scale the radii with the image.

There is no reference implementation to compare with, so suite 192 makes haze with a
known answer: a scene that satisfies the prior, hazed with a known `A` and `t`. The
airlight comes back within 0.017 (the sky it is read from is 2 % scene), the transmission
to a median error of 0.029, the scene to a mean error of 0.032 (omega keeps 5 % of the
haze on purpose). A clear image with a white patch changes by 0.0005 on average; one with
no bright neutral at all has no airlight to find and returns `ALWAN_E_RANGE`.

## Carrying a look: colour transfer

```c
alwan_status alwan_color_transfer_reinhard_{T}(alwan_{T} *out, size_t out_stride,
                                               alwan_{T} const *src, size_t src_stride, size_t src_count,
                                               alwan_{T} const *ref, size_t ref_stride, size_t ref_count);
```

Reinhard, Ashikhmin, Gooch and Shirley's colour transfer (IEEE CG&A 2001): the mean and
standard deviation of each channel of `src` are matched to `ref`'s in Ruderman's
l alpha beta space, a log LMS space whose channels are nearly decorrelated for natural
images, so a per-channel shift and scale moves the look without the cross-talk the same
operation causes in RGB. It uses the paper's RGB to LMS matrix and the exact inverse of
it (the paper prints the inverse rounded), floors LMS at 1e-6 before the logarithm, and
does not clamp. Compared with `alwan_histogram_match`, it carries two moments where that
carries the whole distribution, and so keeps the source's own shape.

No implementation of the paper's own space exists to compare with; suite 193 checks the
property that defines it: the result's l alpha beta means and standard deviations equal
the reference's, to 3e-15, and an image transferred onto itself comes back unchanged.

## Edges from another image: the joint bilateral filter

```c
alwan_status alwan_joint_bilateral_filter_{T}(alwan_{T} *out, size_t out_row_stride,
                                              alwan_{T} const *src, size_t src_row_stride, size_t src_channels,
                                              alwan_{T} const *joint, size_t joint_row_stride, size_t joint_channels,
                                              size_t width, size_t height, size_t radius,
                                              alwan_{T} sigma_color, alwan_{T} sigma_space);
```

The bilateral filter (Tomasi and Manduchi 1998) with its range weight taken from a second,
joint image (Petschnigg et al. and Eisemann and Durand, 2004): each output is a mean of
`src` over a disc, weighted by distance and by how close the joint image's value there is
to its value at the centre. It denoises a no-flash photograph along a flash photograph's
edges, or smooths a mask or depth map along a picture's; with `src` as its own joint it is
the ordinary bilateral filter. The colour distance is the L1 sum of channel differences, the
window a disc and the border reflected without repeating the edge pixel, as in OpenCV's
`ximgproc::jointBilateralFilter`; the colour Gaussian is evaluated exactly where OpenCV reads
a 4096-bin table, and a flat joint image is not replaced by a square Gaussian blur as
OpenCV does. Cost grows with the square of the radius, unlike the guided filter's. Suite
194 agrees with OpenCV to 4.8e-7.

### The rolling guidance filter

```c
alwan_status alwan_rolling_guidance_filter_{T}(alwan_{T} *out, size_t out_row_stride,
                                               alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                               size_t width, size_t height, size_t radius,
                                               alwan_{T} sigma_color, alwan_{T} sigma_space,
                                               size_t iterations, int from_gaussian);
```

Zhang, Shen, Xu and Jia (ECCV 2014): the joint bilateral filter iterated with its own last
output as the joint image. Starting from the source's Gaussian (`from_gaussian = 1`, the
paper), structures smaller than `sigma_space` are removed first and the iterations then
bring the large edges back, a scale-aware smoother that keeps the outlines of what it keeps.
OpenCV's `ximgproc::rollingGuidanceFilter` starts from the source itself
(`from_gaussian = 0`), which keeps small structures that the paper's start removes; suite
194 holds that start to OpenCV to 7.7e-7 over four iterations and checks the paper's first
step is the constant-guide filter.

## Edge-aware smoothing in linear time: the domain transform

```c
typedef enum { ALWAN_DT_NC = 0, ALWAN_DT_RF = 2 } alwan_domain_transform_mode;

alwan_status alwan_domain_transform_filter_{T}(alwan_{T} *out, size_t out_row_stride,
                                               alwan_{T} const *src, size_t src_row_stride, size_t src_channels,
                                               alwan_{T} const *guide, size_t guide_row_stride, size_t guide_channels,
                                               size_t width, size_t height,
                                               alwan_{T} sigma_spatial, alwan_{T} sigma_color,
                                               alwan_domain_transform_mode mode, size_t iterations);
```

Gastal and Oliveira (SIGGRAPH 2011) replace a 2D edge-aware filter with 1D filters along
rows and columns in a transformed coordinate: each step between neighbours is
`1 + sigma_spatial / sigma_color * |guide difference|` (L1 over the guide's channels) long,
so an edge in the guide becomes a long gap that the 1D filter barely crosses. Rows and
columns alternate `iterations` times with a shrinking spatial sigma, which removes the
stripes a single pass leaves. The cost does not depend on `sigma_spatial`.

`ALWAN_DT_NC` (normalized convolution) averages a box in the transformed coordinate;
`ALWAN_DT_RF` (recursive filtering) runs a first-order recursive filter forward and back,
which leaks further across weak edges and costs less. The interpolated-convolution mode of
the paper is not offered.

The filter follows OpenCV's `ximgproc::dtFilter`: the transformed coordinate is built in
float as OpenCV builds it, so NC boxes hold the same pixels, and the data is filtered in
double. Suite 195 agrees with OpenCV to 1.9e-6 in NC mode (OpenCV's float running sum) and
1.5e-7 in RF mode, along a colour and a one-channel guide. `sigma_spatial` below 1 or
`sigma_color` below 0.01 returns `ALWAN_E_RANGE` where OpenCV clamps silently; `iterations`
runs from 1 to 30. `out` may alias `src`; `guide` may be `src` itself.

## Global smoothing in linear time: the fast global smoother

```c
alwan_status alwan_fast_global_smoother_{T}(alwan_{T} *out, size_t out_row_stride,
                                            alwan_{T} const *src, size_t src_row_stride, size_t src_channels,
                                            alwan_{T} const *guide, size_t guide_row_stride, size_t guide_channels,
                                            size_t width, size_t height,
                                            alwan_{T} lambda, alwan_{T} sigma_color,
                                            alwan_{T} lambda_attenuation, size_t iterations);
```

Min, Choi, Lu, Ham, Sohn and Do (IEEE TIP 2014) solve the weighted least squares
smoother, `sum (u - f)^2 + lambda sum w_pq (u_p - u_q)^2` with
`w_pq = exp(-|g_p - g_q| / sigma_color)`, as exact 1D problems along every row and then
every column. Each is tridiagonal and costs one forward and one backward sweep. Where
the filters above average a window, this one solves for the whole line at once, so a
region bounded by guide edges flattens however large it is. Larger `lambda` smooths
further; `iterations` repeat the row and column passes with `lambda` multiplied by
`lambda_attenuation` each time (OpenCV's defaults are 3 and 0.25).

The distance `|g_p - g_q|` is Euclidean over the guide's channels and `sigma_color` is in
the guide's units. OpenCV's `ximgproc::fastGlobalSmootherFilter` takes an 8-bit guide and
`sigma_color` in 0..255 steps; divide both by 255 to use its numbers with a guide in
0..1. Suite 196 does that and agrees with OpenCV to 7.8e-6 at `lambda` 1000 and to
4.5e-7 at `lambda` 10, the difference being OpenCV's float solve; a constant source stays
constant to 1e-14. `out` may alias `src` or the guide.

## Local contrast: CLAHE

```c
alwan_status alwan_clahe_u8(unsigned char *out, size_t out_row_stride,
                            unsigned char const *src, size_t src_row_stride,
                            size_t width, size_t height, size_t tiles_x, size_t tiles_y,
                            double clip_limit);
alwan_status alwan_clahe_u16(unsigned short *out, size_t out_row_stride,
                             unsigned short const *src, size_t src_row_stride,
                             size_t width, size_t height, size_t tiles_x, size_t tiles_y,
                             double clip_limit);
```

Contrast-limited adaptive histogram equalisation (Zuiderveld, Graphics Gems IV, 1994).
The image is cut into `tiles_x` by `tiles_y` tiles. Each tile's histogram is clipped at
`clip_limit` times its mean bin count, the excess is spread back over every bin, and the
cumulative histogram becomes that tile's tone curve. Each pixel is mapped by the curves of
its four nearest tiles, blended bilinearly. Flat regions gain contrast and `clip_limit`
caps how much: 2 to 4 is the usual photographic range, OpenCV's default is 40, and 0 or
below turns clipping off.

The data is one channel of 8-bit (256 bins) or 16-bit (65536 bins) values. For a colour
image, equalise a lightness channel (CIELAB L*, Oklab L, or luma) and rebuild the colour
from it; equalising R, G and B apart shifts hues.

The clip level is counted per bin: `(int)(clip_limit * tile_area / bins)`, at least 1.
With 65536 bins, a tile of fewer than `65536 / clip_limit` pixels clips at one count
whatever `clip_limit` says, so on 16-bit data clip limits of 2 and 4 give the same image
unless the tiles are large. OpenCV behaves the same way. For photographic clip limits on
an image of ordinary size, equalise an 8-bit lightness channel.

## Detail and tone without halos: the local Laplacian filter

```c
typedef enum { ALWAN_LLF_LUMINANCE = 0, ALWAN_LLF_SEPARATE = 1 } alwan_llf_color_mode;

alwan_status alwan_local_laplacian_filter_{T}(alwan_{T} *out, size_t out_row_stride,
                                              alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                              size_t width, size_t height,
                                              alwan_{T} sigma, alwan_{T} alpha, alwan_{T} beta,
                                              size_t intensity_levels, alwan_llf_color_mode mode);
```

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
pixel. `0` picks MATLAB's count from `alpha`, 16 at `alpha` 0.9 and above, rising to 50
below 0.1.

`ALWAN_LLF_LUMINANCE` filters the luma of a three-channel image and scales the channels
by it, which keeps their ratios and so the hues; `ALWAN_LLF_SEPARATE` filters each channel.
The filter follows MATLAB's `locallapfilt`. Its compiled pyramid, remap and blend steps
were read by probing them (the kernel `[.05 .25 .4 .25 .05]`, half-sample symmetric
borders, `floor(log2(min(w, h))) + 1` levels, linear weights between intensity levels).
Suite 198 agrees with MATLAB to 1.5e-6 over eight cases, the size of MATLAB's single
precision. `out` may alias `src`.

## Denoising: total variation

```c
alwan_status alwan_denoise_tv_chambolle_{T}(alwan_{T} *out, size_t out_row_stride,
                                            alwan_{T} const *src, size_t src_row_stride, size_t channels,
                                            size_t width, size_t height,
                                            alwan_{T} weight, alwan_{T} eps, size_t max_iterations);
```

The Rudin, Osher and Fatemi model: each channel becomes the image `u` minimising
`sum (u - f)^2 / 2 + weight sum |grad u|`, solved by Chambolle's dual iterations (J. Math.
Imaging and Vision, 2004). Total variation charges for every change between neighbours,
not for its size, so noise and fine texture go while edges stay sharp; flat regions go
flat, which reads as a painted look at high `weight`. On values in 0..1, `weight` 0.05 to
0.2 covers light to strong denoising. The iterations stop when the energy changes by less
than `eps` times its first value, or after `max_iterations`; scikit-image's defaults are
2e-4 and 200.

The function follows scikit-image's `denoise_tv_chambolle` with `channel_axis` set, down
to which iteration's image it returns. Suite 200 agrees with it exactly in f64 over seven
cases, including runs cut after 1, 2 and 7 iterations. `out` may alias `src`.

### Non-local means

```c
alwan_status alwan_denoise_nl_means_u8(unsigned char *out, size_t out_row_stride,
                                       unsigned char const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height, double h,
                                       size_t template_window, size_t search_window);
```

Buades, Coll and Morel (CVPR 2005). Each pixel becomes the weighted mean of the pixels in a
`search_window` square around it, weighted by `exp(-d / (h^2 channels))`, where `d` is the
mean squared difference between the `template_window` patches around the two pixels. A
pixel draws on others with the same neighbourhood wherever they are in the window, so
repeated structure survives and noise averages away. `h` is in 0..255 units: about 10
keeps fine detail, higher values remove more. OpenCV's default windows are 7 and 21.

The function takes 8-bit data of 1 to 4 channels with one `h`, and reproduces OpenCV's
`cv::fastNlMeansDenoising` (`NORM_L2`) bit for bit, fixed-point weight table included.
Suite 201 holds every value of ten cases equal. For a colour photograph, OpenCV's
`fastNlMeansDenoisingColored` denoises CIELAB with a separate `h` for a and b. Convert to
Lab first and denoise the channels you choose. `out` must not alias `src`.

### Anisotropic diffusion

```c
alwan_status alwan_anisotropic_diffusion_u8(unsigned char *out, size_t out_row_stride,
                                            unsigned char const *src, size_t src_row_stride, size_t channels,
                                            size_t width, size_t height,
                                            double alpha, double k, size_t iterations);
```

Perona and Malik (IEEE PAMI 1990). Each iteration moves every pixel toward its eight
neighbours by `alpha sum g(d) (I_n - I)`, with `g(d) = exp(-(d / (k channels 255))^2)` and
`d` the L1 difference over the channels. Differences well below `k` diffuse and edges well
above it stay, so the image smooths within regions and not across them. `alpha` 0.1 to 0.2
keeps the eight-neighbour step stable; `k` 0.02 to 0.1 sets the edge threshold as a
fraction of full scale per channel.

For three channels the function reproduces OpenCV's `ximgproc::anisotropicDiffusion` bit
for bit for one iteration, and suite 202 holds `n` iterations to `n` chained one-iteration
OpenCV calls. OpenCV's own loop is correct only for one iteration. It refreshes the
border with `copyMakeBorder` from a view into the padded buffer itself, and without
`BORDER_ISOLATED` that call grows the view instead of replicating. Later iterations then
read a border that is stale or was never written. alwan replicates the border every
iteration. `out` may alias `src`.

The functions reproduce OpenCV's `cv::createCLAHE` bit for bit, and suite 197 holds every
pixel of ten cases equal. An image that does not divide into tiles is extended at the
bottom and right by reflection, by `tiles - size % tiles` in each direction, so an image
that divides only across still gains a whole tile across; alwan keeps that, since it moves
the tile grid. `out` may alias `src`.
