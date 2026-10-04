# Tonal adjustments

Per-pixel tonal adjustments: levels, posterize, solarize, sigmoidal contrast and its inverse,
and modulate (brightness, saturation and hue). Each maps a pixel's values to new values on
their own, with no neighbours and no image statistics. The adjustments that need the image's
histogram (auto-level, auto-gamma, autocontrast with a cutoff) are image processing, and they
are in [Suwar](https://github.com/soufianekhiat/Suwar), not here.

```c
alwan_status alwan_tone_adjust_{T}(alwan_{T} *out, size_t out_stride, alwan_{T} const *in, size_t in_stride,
                                   size_t count, size_t channels, alwan_tone_adjust_method method,
                                   alwan_tone_adjust_params const *params);
alwan_status alwan_tone_adjust_u8(unsigned char *out, size_t out_stride, unsigned char const *in,
                                  size_t in_stride, size_t count, size_t channels,
                                  alwan_tone_adjust_method method, alwan_tone_adjust_params const *params);
```

`count` pixels of `channels` values, 1 to 4, each pixel `stride` bytes from the last (0 for
packed). The alpha, which is the 2nd of 2 values or the 4th of 4, is copied. `out` may be `in`.
Values are in `[0, 1]`. The 8-bit form reads code / 255 and stores rounded to nearest. Nothing
is clamped unless the method says so.

`alwan_tone_adjust_params` follows the family rule: a zero field is the default. `params` may
be `NULL` for all defaults.

## Methods

| Method | Formula | Defaults |
|---|---|---|
| `ALWAN_TONE_ADJUST_LEVELS` | `out_black + (out_white - out_black) g(x)`, `x = (v - in_black) / (in_white - in_black)`, `g(x) = x^(1/gamma)`, `x < 0` passing linearly (ImageMagick's `-level`). `clamp` limits `x` to `[0, 1]`. | `in_white`, `out_white`, `gamma` 0 read as 1: the identity |
| `ALWAN_TONE_ADJUST_POSTERIZE` | `floor(v (levels - 1) + 0.5) / (levels - 1)`: ImageMagick's `-posterize`, which rounds. | `levels` 0 reads as 4 |
| `ALWAN_TONE_ADJUST_SOLARIZE` | `v >= threshold` becomes `1 - v` (Pillow's `ImageOps.solarize`; ImageMagick inverts only above it). A negative threshold inverts everything. | `threshold` 0 reads as 0.5; Pillow's 128 is `128 / 255` |
| `ALWAN_TONE_ADJUST_SIGMOIDAL_CONTRAST` | ImageMagick's `-sigmoidal-contrast`: with `Sig(x) = 1 / (1 + exp(contrast (midpoint - x)))`, `(Sig(v) - Sig(0)) / (Sig(1) - Sig(0))`. | `contrast` 0 is the identity; `midpoint` 0 reads as 0.5 |
| `ALWAN_TONE_ADJUST_SIGMOIDAL_CONTRAST_INVERSE` | Its exact inverse (`+sigmoidal-contrast`), the logistic's argument limited to `(1e-12, 1 - 1e-12)` as ImageMagick limits it. | as above |
| `ALWAN_TONE_ADJUST_MODULATE` | Brightness, saturation and hue on three colour channels; see below. | all 0: the identity |

**Posterize, round or floor.** ImageMagick's posterize rounds to `levels` evenly spaced values
that include black and white. Pillow's `ImageOps.posterize(image, bits)` keeps the top `bits`
of each 8-bit code instead. That is a floor that never reaches white: 2 bits give 0, 64, 128
and 192. Set `pillow_bits` (1 to 8, 8-bit form only) for Pillow's.

**Levels with an inverted output.** Because a zero field is a default, `out_white = 0` reads as
1. Map to an inverted range with a tiny `out_white` such as `1e-300`, or with
`ALWAN_TONE_ADJUST_SOLARIZE` and a negative threshold for a plain `1 - v`.

## Modulate

`brightness` and `saturation` are relative changes: 0 leaves the value unchanged, -1 takes it to
zero, and 0.5 makes it 1.5 times. `hue` is a rotation in `[-1, 1]` of half turns, so 1 and -1
are 180 degrees; this is the signed-range rule of [ranges.md](../ranges.md), hue / pi.

- `ALWAN_MODULATE_OKLCH`, the default, scales Oklab L and chroma and turns the Oklab hue. It
  works on sRGB-encoded values, or on linear sRGB with `input_linear`. It is perceptual: a
  hue turn keeps lightness, and a saturation change keeps hue.
- `ALWAN_MODULATE_HSL` is ImageMagick's `-modulate` (its default model), scaling HSL
  lightness and saturation on the encoded values.
- `ALWAN_MODULATE_HSV` is the same in HSV, scaling value and saturation.

ImageMagick's percentages convert as `brightness = B / 100 - 1`, `saturation = S / 100 - 1`
and `hue = H / 100 - 1`, since its `-modulate 100,100,200` is a half turn. Results may leave
`[0, 1]`; nothing is clamped, and the 8-bit form clamps only when it stores.

## Accuracy

Suite 305:

- Pillow 12's `ImageOps.posterize` (bits 1 to 8) and `ImageOps.solarize` (thresholds 1, 64,
  128, 200 and 255) on 8-bit images are exact: 0 of 9984 codes differ.
- ImageMagick's level, posterize and sigmoidal formulas, transcribed in numpy, agree within
  2.8e-16.
- Modulate in HSL and HSV against Python's `colorsys` agrees within 8.4e-16.
- OKLCH modulate against colour's Oklab, with the sRGB matrix derived from its primaries,
  agrees within 9.6e-15.
- The sigmoidal inverse undoes the forward within 2.3e-15.

**Returns:** `ALWAN_E_INVALID` for:

- `NULL` buffers;
- `channels` outside 1 to 4;
- an unknown method or modulate space;
- modulate without three colour channels;
- `pillow_bits` outside the 8-bit form.

`ALWAN_E_RANGE` for a parameter that is not finite or out of its range: `in_white` equal to
`in_black`, a gamma that is not positive, fewer than 2 levels, a negative contrast, or a
brightness or saturation below -1.
