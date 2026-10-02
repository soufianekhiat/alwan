# Dithering

A float image quantised to 8 or 16 bit codes bands where the signal changes slowly, and a
view transform's smooth gradients (skies, vignettes, fog) show it first. Dithering trades the
bands for fine noise: it adds a pattern before rounding, or carries each pixel's rounding
error to its neighbours, so the local mean stays the signal's. These functions work on the
ENCODED values (after the view transform or the OETF), in [0, 1]. Suite 285, plate 150.

```c
typedef enum {
    ALWAN_DITHER_NONE = 0,                 /* floor(v L + 0.5) */
    ALWAN_DITHER_ORDERED_BAYER = 1,        /* floor(v L + t), t from the Bayer matrix */
    ALWAN_DITHER_ORDERED_BLUE_NOISE = 2,   /* floor(v L + t), t from a blue noise mask */
    ALWAN_DITHER_FLOYD_STEINBERG = 3,      /* 7 3 5 1 / 16 */
    ALWAN_DITHER_JARVIS_JUDICE_NINKE = 4,  /* two rows, / 48 */
    ALWAN_DITHER_STUCKI = 5,               /* two rows, / 42 */
    ALWAN_DITHER_ATKINSON = 6,             /* 1 1 / 1 1 1 / 1, / 8: 6/8 of the error passes on */
    ALWAN_DITHER_BURKES = 7,               /* 8 4 / 2 4 8 4 2, / 32 */
    ALWAN_DITHER_SIERRA = 8,               /* 5 3 / 2 4 5 4 2 / 2 3 2, / 32 */
    ALWAN_DITHER_SIERRA_TWO_ROW = 9,       /* 4 3 / 1 2 3 2 1, / 16 */
    ALWAN_DITHER_SIERRA_LITE = 10,         /* 2 / 1 1, / 4 */
    ALWAN_DITHER_OSTROMOUKHOV = 11         /* Ostromoukhov 2001, variable coefficients */
} alwan_dither_method;

typedef struct {
    int bits;                   /* 1 to the format's [8 for U8, 16 for U16] */
    int store_codes;            /* [0]: scaled to the format's range; else the code itself */
    int bayer_log2_size;        /* ORDERED_BAYER: side 2^n, 1 to 8 [3] */
    uint32_t const *mask;       /* ORDERED_BLUE_NOISE: a rank mask [NULL: the built-in 64] */
    size_t mask_width, mask_height;
    int tpdf;                   /* ordered: triangular noise [0] */
    int decorrelate_channels;   /* ordered: an R2 offset per channel [0] */
    uint32_t frame;             /* ordered: an R2 offset per frame [0] */
    int serpentine;             /* error diffusion: odd rows right to left [0] */
} alwan_dither_params;

alwan_status alwan_dither_quantize_{T}(void *dst, size_t dst_row_stride, alwan_pixel_format dst_fmt,
                                       alwan_scalar_{T} const *src, size_t src_row_stride, size_t channels,
                                       size_t width, size_t height, alwan_dither_method method,
                                       alwan_dither_params const *params);

alwan_status alwan_image_convert_dithered_{T}(void *dst, size_t dst_row_stride, void const *src,
                                              size_t src_row_stride, size_t width, size_t height,
                                              alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt,
                                              alwan_rgb_space_desc_{T} const *src_space,
                                              alwan_rgb_space_desc_{T} const *dst_space,
                                              alwan_dither_method method, alwan_dither_params const *params,
                                              alwan_ctx *ctx);

alwan_status alwan_blue_noise_mask_generate(uint32_t *ranks_out, size_t width, size_t height, uint64_t seed);
alwan_status alwan_blue_noise_mask_builtin(uint32_t *ranks_out, size_t side);
```

## Quantisation

`L = 2^bits - 1` steps. A value `v` becomes the code `floor(v L + offset)` held to `[0, L]`:

| Method | offset |
|---|---|
| NONE | 0.5, round to nearest, the codes `alwan_image_convert` writes |
| ORDERED_BAYER | `(rank + 0.5) / side^2`, the Bayer matrix's rank at the pixel |
| ORDERED_BLUE_NOISE | `(rank + 0.5) / n`, the mask's rank at the pixel, the mask tiled |
| ordered with `tpdf` | `0.5 + tpdf(threshold)`, the threshold remapped to the triangle on (-1, 1) |

Over a whole tile the ordered thresholds are the slots of [0, 1) taken once each, so a
constant image's mean code is the value to within half a slot: 1 / (2 side^2) of a step for
Bayer, 1 / (2 n) for a mask of n pixels (suite 285 measures 7.3e-6 of full range at 4 bits on
the 64 x 64 mask, the slot being 8.1e-6). Uniform (RPDF) noise of one step leaves the error's
variance depending on the signal; triangular (TPDF) noise of two steps makes it constant
(Lipshitz, Wannamaker and Vanderkooy 1992), at the price of more noise.

Error diffusion quantises the pixels in scan order, each value plus the error carried to it
rounded to nearest, and spreads the new error over the next pixels with the method's weights;
error that would land outside the image is dropped. `serpentine` scans odd rows right to left
with the kernel mirrored, which breaks up the diagonal structures plain scanning leaves.

`store_codes = 0` writes each code scaled to the format's range, `round(code (2^fmt - 1) / L)`,
so a 6-bit result in U8 reads 0, 4, 8 ... 255 and displays as it will look; non-zero writes the
code itself (10 bits in U16 as 0..1023, say). Values outside [0, 1] clamp; NaN or infinity is
`ALWAN_E_INVALID`, with the rows before it written.

`decorrelate_channels` and `frame` move the mask by the R2 sequence's fractions (Roberts
2018, offsets (k 0.7548776662466927, k 0.5698402909980532) mod 1 of the mask's size, k = 4
frame + channel), so the three channels do not carry the same noise, which reads as grey grain,
and an animation gets a new pattern each frame without a new mask. A Bayer matrix moves the
same way.

## Blue noise masks

`alwan_blue_noise_mask_generate` ranks every pixel of a `width x height` torus by Ulichney's
void-and-cluster method ("The void-and-cluster method for dither array generation", Proc. SPIE
1913, 1993): a tenth of the pixels as an initial pattern (splitmix64 from `seed`), relaxed by
moving the tightest cluster into the largest void until they coincide, then ranked down by
removing clusters and up by filling voids. Energy is a Gaussian of sigma 1.5, Ulichney's value,
over a window of radius 8 round the torus, read from a table (`data/blue_noise/
void_cluster_kernel.csv`) rather than computed with `exp`, so the deterministic build ranks the
same mask as the release build. Ulichney's third phase, which fills the tightest cluster of
zeros, is the same as filling the largest void here: on a torus the window sums to the same
everywhere. Ties go to the first pixel in raster order. 128 x 128 takes about 0.4 s; the method
is quadratic in the pixel count, so it is capped at 65536 pixels.

The built-in masks are side 64 and 128 at seed 0 (`alwan_blue_noise_mask_builtin` copies one
out); ORDERED_BLUE_NOISE reads the 64 by default. Measured on the 64 (suite 285), the power of
its thresholds per frequency bin is 1.3e-5 below 0.1 cycles per pixel, 2.5e-4 from 0.1 to 0.2
and 0.127 above 0.35, where white noise of the same variance gives 1/12 = 0.083 everywhere:
the low frequencies, which the eye sees as blotches, are nearly empty.

## On the GPU

`core/alwan_dither_core.h` holds the per-pixel pieces: `alwan_dither_bayer_rank` (the matrix by
its recursive definition, the finest coordinate bit the most significant pair of the rank),
`alwan_dither_threshold`, `alwan_dither_tpdf` and `alwan_dither_quantize`. A shader reading the
built-in mask from a texture (the ranks, as `alwan_blue_noise_mask_builtin` gives them) dithers
with the same arithmetic as `alwan_dither_quantize`. It compiles with dxc and fxc.

## alwan_image_convert_dithered

`alwan_image_convert` with a dither: each row is converted to float in `dst_space`'s encoding,
then quantised to U8 or U16 by the rules above at its own row index, so masks tile and error
diffusion carries down the image exactly as on a whole float image (suite 285 checks Stucki row
by row against the whole image, code for code). With ALWAN_DITHER_NONE the codes are
`alwan_image_convert`'s.

## Accuracy

Suite 285 holds the methods to their definitions written out as plain loops
(`gendata/tests/dither_reference.py`): round, Bayer 4 x 4, the built-in blue noise mask,
Floyd-Steinberg, Jarvis-Judice-Ninke and Stucki, plain and serpentine, every code equal. On a
slow ramp quantised to 6 bits, the mean over 16-column blocks follows the ramp to 2.2e-4 (Bayer),
1.3e-4 (blue noise), 2.1e-4 (Floyd-Steinberg) and 2.6e-4 (Stucki) of full range, where
rounding is 7.5e-3 off.

## The added error-diffusion kernels

Atkinson (MacPaint, 1984) passes on only 6/8 of each pixel's error, which keeps highlights
and shadows clean at the cost of the mean: on a constant 128 x 128 image at 4 bits the
mean of its codes strays by up to 8.3e-3 of full scale, where Burkes, the three Sierra
kernels and Ostromoukhov stay within 3.1e-4 (suite 285).

Ostromoukhov 2001 ("A Simple and Efficient Error-Diffusion Algorithm", SIGGRAPH 2001)
sends the error to three neighbours, right, down-left and down, with weights from the
paper's Appendix I table, chosen per input level 0..127 and mirrored above (level i reads
255 - i). The paper dithers 8-bit input to one bit; alwan indexes the table by the input's
place inside its quantisation step, scaled to 0..255, which is the paper's level at one
bit. The paper scans in serpentine order: set `serpentine` for its results.

Every kernel is checked code for code against its definition written out in
`gendata/tests/dither_reference.py`, plain and serpentine.
