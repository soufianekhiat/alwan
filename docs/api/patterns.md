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

## Colour space and layout

The signal is the pattern's own. To place the bars in another space, decode and convert
them with the RGB conversion functions ([color-spaces.md](color-spaces.md)); to reach
integer or half-float buffers, the `_ex` pixel formats of the map API take the rendered
floats ([map.md](map.md)).

## Sources

ITU-R BT.471-1 is a free download from itu.int. The English translation of ARIB STD-B28
is free from arib.or.jp; alwan implements the signal the standard defines and does not
reproduce its text. SMPTE RP 219 and EG 1 are not freely available, so ARIB STD-B28 stands
in for RP 219. EBU Tech 3325 is free from tech.ebu.ch, and so are its pattern files; the
files state no licence, so alwan_dev records the rectangles and codes measured from them
rather than the files.
