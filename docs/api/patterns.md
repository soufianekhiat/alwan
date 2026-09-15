# Test Patterns

Reference images alwan renders itself, at any size, for debugging, calibration and
reference: the colour bar signals of ITU-R BT.471-1 and the ARIB STD-B28 multiformat
colour bar.

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

## Colour space and layout

The signal is the pattern's own. To place the bars in another space, decode and convert
them with the RGB conversion functions ([color-spaces.md](color-spaces.md)); to reach
integer or half-float buffers, the `_ex` pixel formats of the map API take the rendered
floats ([map.md](map.md)).

## Sources

ITU-R BT.471-1 is a free download from itu.int. The English translation of ARIB STD-B28
is free from arib.or.jp; alwan implements the signal the standard defines and does not
reproduce its text. SMPTE RP 219 and EG 1 are not freely available, so ARIB STD-B28 stands
in for RP 219.
