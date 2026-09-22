# Film API

Spectral film stocks: negatives, prints and reversals as their datasheets describe them,
and the pipeline from a scene spectrum to the projected print.

---

## Overview

A stock is three emulsion layers, each with a spectral sensitivity, a characteristic
curve (dye amount against log exposure) and a dye whose absorption spectrum is known,
over a base that absorbs a little on its own. Exposing a scene spectrum gives three log
exposures; the curves turn them into three dye amounts; the dyes and the base make a
transmittance; a printer light through that transmittance exposes the print stock, which
has its own curves and dyes; and a projection light through the print, against the
observer, gives XYZ.

The profiles are spectral_film_lut's (Jan Lohse, MIT): stocks digitised from the
manufacturers' sheets and profiled by its `FilmSpectral`. alwan ships that profiling's
output for the whole of the package's exported catalogue at the pinned commit, 87
profiles on 380-780 nm at 10 nm, and holds this pipeline to the model's over the same
tables (suite 158). The model is float32 throughout, so agreement is within 1.1e-06
relative over nine cases and not to the bit. Grain, halation and interlayer diffusion are spatial and
not here.

Every spectrum in this API has `ALWAN_FILM_BANDS` = 41 samples on that grid. Resample
with `alwan_spd_resample`, or hand a Mallett 2019 spectrum (81 samples at 5 nm) to the
map form, which reads every second band.

Every function has an `_f64` and an `_f32` form; this page shows `_f64`.

---

## Stocks

```c
typedef enum {
    ALWAN_FILM_NONE = -1,
    ALWAN_FILM_KODAK_5203 = 0, ALWAN_FILM_KODAK_5207, ALWAN_FILM_KODAK_5213, ALWAN_FILM_KODAK_5219,
    ALWAN_FILM_KODAK_PORTRA_400, ALWAN_FILM_KODAK_EKTAR_100, ALWAN_FILM_FUJI_ETERNA_500,
    ALWAN_FILM_KODAK_5222,                                          /* black and white negative */
    ALWAN_FILM_KODAK_2383, ALWAN_FILM_KODAK_2393, ALWAN_FILM_FUJI_3513DI,   /* prints */
    ALWAN_FILM_KODAK_2302,                                          /* black and white print */
    ALWAN_FILM_KODAK_EKTACHROME_100D, ALWAN_FILM_FUJI_VELVIA_50,    /* reversals */
    ALWAN_FILM_AGFA_VISTA_100 = 14, /* ... the rest of the catalogue, 87 in all */
    ALWAN_FILM_STOCK_COUNT
} alwan_film_stock;

alwan_status alwan_film_stock_info(alwan_film_stock stock, char const **name, char const **manufacturer);
alwan_status alwan_film_get_profile_f64(alwan_film_profile_f64 *out, alwan_film_stock stock);
```

`alwan_film_stock_info` answers for every stock; `alwan_film_get_profile` returns
`ALWAN_E_NODATA` for one compiled out (`ALWAN_TABLES_FILM`, or the stock's own switch,
`ALWAN_TABLE_FILM_KODAK_5219` and so on). Values are stable; new stocks append.

The catalogue, in enum order after the first fourteen. Colour negatives: Agfa Vista
100; Fuji C200, Eterna 500 Vivid, Natura 1600, Pro 160C, Pro 160S, Pro 400H, Superia
Reala, Superia X-Tra 400; Kodak Verita 200D 5206, 5247, 5247 II and its alternate
sheet, 5248, EXR 100T 5248, 5250, Vision 320T 5277, EXR 200T 5293, Aerocolor IV 2460
with its low and high variants, Gold 200, Portra 160, Portra 800 and its pushes to
1600 and 3200, Ultramax 400, Vericolor III. Black and white negatives: 5222 at four
development times and Tri-X 400 at its base and three development times. Prints:
Fuji Eterna-CP 3523XD, Crystal Archive DPII, Maxima, Pro PDII and Super Type C,
Fujiflex old and new; Kodak 5381, 5383, 5384, Duraflex Plus, Endura Premier, EXR 5386,
Portra Endura, Supra Endura. Black and white prints: 2302 at five development times,
Polymax Fine-Art and its seven grades. Reversals: Fuji FP-100C, Instax, Provia 100F;
Kodachrome 64; Kodak Aerochrome III and the alternate Ektachrome 100D sheet. Reversal
prints, positive paper for a positive original: Ilfochrome Micrographic M and P,
Kodak Ektachrome Radiance III. A variant is its own profile because it is its own
curve; `alwan_film_stock_info` gives the sheet's name for any of them.

Three profiles carry the model's own overshoot and ship as they are: Verita 5206's
adjusted base spectrum dips to -0.66 at a band, Vericolor III's dye density to -0.49,
and 5247's colour-masking matrix has a third row of -9.3, -45.0, 55.3, which amplifies
that layer's exposure noise some fifty times and shows as blue speckle in a frame's
shadows. Pass `masking = 0` to `alwan_film_develop`, or zero `color_masking` in the
profile before a look or a render reads it, to print 5247 without it. Suite 158 prints
the most negative base and the largest masking entry it saw.

### The profile

```c
typedef struct {
    alwan_film_stock stock;  char const *name;  char const *manufacturer;  int year;
    int layers;            /* 3, or 1 for black and white */
    int is_print;          /* exposed by a printer light, not a scene */
    int is_positive;       /* reversal */
    alwan_f64 iso;         /* 0 where the sheet gives none */
    alwan_f64 exposure_kelvin;
    alwan_f64 log_h_min, log_h_max;  size_t curve_count;   /* the uniform log exposure grid */
    alwan_f64 log_h_ref[3], d_ref[3];   /* the 18 percent grey's exposure and activation */
    alwan_f64 d_min[3], d_max[3], gamma, color_masking, rms_granularity;
    alwan_f64 masking[9];               /* log exposure interlayer inhibition, row-major */
    alwan_f64 const *sensitivity;       /* [41 * 3], sensitivity[k * 3 + c] at 380 + 10 k nm */
    alwan_f64 const *dye_density;       /* [41 * 3], per unit activation */
    alwan_f64 const *d_min_spectral;    /* [41], base plus fog */
    alwan_f64 const *density_curve;     /* [1024 * 3], activation against log exposure */
} alwan_film_profile_f64;
```

Pointers are into the embedded tables; nothing is allocated. What the profiling did to
the sheet: sensitivities extrapolated and resampled; the characteristic curve extended
past the sheet with a logistic roll-off, resampled to 1024 uniform points and D-min
removed; status densities unmixed into per-layer activations through the status
responsivities; dyes normalised so a unit activation reads 1 in its own status channel;
the grey placed at 12.5 / ISO lux-seconds. A one-layer stock stores its one column three
times and says so in `layers`; every function reads column 0 and repeats it.

A caller may fill a profile by hand from their own tables; every function checks the
pointers and needs `curve_count` to be `ALWAN_FILM_CURVE_COUNT`.

---

## The pipeline

```c
alwan_film_profile_f64 neg, prt;
alwan_film_get_profile_f64(&neg, ALWAN_FILM_KODAK_5219);
alwan_film_get_profile_f64(&prt, ALWAN_FILM_KODAK_2383);

alwan_f64 factors[3], light[41], log_h[3], d[3], dp[3];
alwan_film_calibrate_f64(factors, &neg, grey_spd, balance);            /* once per scene */
alwan_film_printer_light_f64(light, &neg, &prt, 0.0, 0.0, 0.0);        /* once per pair */

alwan_film_expose_f64(log_h, &neg, spd, balance, factors);
alwan_film_develop_f64(d, &neg, log_h, 1);
alwan_film_print_f64(dp, &neg, d, &prt, light);
alwan_xyz_f64 xyz;
alwan_film_project_f64(&xyz, &prt, dp, projection_light);
```

### alwan_film_expose / alwan_film_calibrate

Layer c receives `H_c = factor_c sum_k spd_k balance_k S_kc`, a plain sum over the 41
bands with no bandwidth, and `log_h_out[c] = log10(H_c)` floored at 1e-16. `balance` is
the model's spectral white balance, the ratio of the reference daylight spectrum to the
scene white's, per band; NULL is none. `factors` put the scene's 18 percent grey at the
stock's reference exposure: `alwan_film_calibrate` returns `10^log_h_ref / H` for the
grey spectrum, and `ALWAN_E_RANGE` if the grey exposes a layer to nothing.

### alwan_film_develop

Log exposure to activation through the characteristic curve, read linearly on its
uniform grid and held flat outside. With `masking` nonzero and three layers the masking
matrix is applied to the log exposures first: the interlayer inhibition of a colour
negative at the stock's default strength. Prints and one-layer stocks ignore it.

### alwan_film_transmittance / alwan_film_status_density

`10^-(sum_c density_c dye_kc + d_min_k)`, 41 values. `alwan_film_status_density` is
ISO 5-3 status density of a transmittance: -log10 of it integrated against the Status
A, Status M or ACES printing density responsivities, whose columns sum to 1 so clear
film reads 0. Sheets are measured in Status M for negatives and Status A for prints and
reversals.

### alwan_film_printer_light / alwan_film_print

The printer light that prints the negative's reference grey onto the print stock at the
print's reference exposure: the ACES printing density printer light split into red,
green and blue, mixed by a 3 x 3 solve through the grey's transmittance and the print's
sensitivities. `red`, `green`, `blue` are offsets in stops, 0 for the neutral print. A
one-layer print gets a flat light set by `green` alone. `alwan_film_print` is the
developed negative's transmittance times that light, integrated against the print's
sensitivities, floored at 1e-5, through the print's curve.

### alwan_film_project

The stock's transmittance, dye term clamped at zero, times the light, against the CIE
1931 2 degree observer at the 41 bands. XYZ is in the light's units: a light with Y = 1
through clear film gives Y = 1 less the base.

### alwan_film_render and the map form

```c
alwan_film_render_f64(&xyz, &neg, &prt, spd, balance, factors, light, projection_light);
alwan_film_render_f64_map_interleave(xyz_out, 3 * sizeof(alwan_f64), spectra, 81 * sizeof(alwan_f64),
                                     pixels, 81, &neg, &prt, balance, factors, light, projection_light);
```

The whole trip for one spectrum, masking at the negative's default strength, the print
skipped when `print` is NULL, and the same over a buffer of 41 or 81 bands per pixel.

---

## A look on scene-linear RGB

```c
alwan_film_look look;
alwan_film_look_default(&look, ALWAN_FILM_KODAK_5219, ALWAN_FILM_KODAK_2383);
look.stops = -0.5;                                   /* pull half a stop */
alwan_film_render_rgb_f64_map_interleave(xyz, 3 * sizeof(alwan_f64), rgb, 3 * sizeof(alwan_f64),
                                         pixels, &look, ctx);
/* xyz is relative to the print's clear base: Y = 1 is paper white; encode for the display */
```

The whole chain from footage, so a caller never touches a spectrum. The input is
scene-linear RGB **as shot**, with the grey card at 0.18: that is the calibration point
and nothing else is taken from the picture. Each pixel is scaled into the unit cube by
`m = max(1, max channel)`, upsampled with Jakob 2019 in `look.gamut`, scaled back by `m`
and lit by `look.light` normalised to Y = 1, so a highlight above white keeps its spectrum
and its exposure. The negative is calibrated on 0.18 of that light, per layer when
`balance_on_grey` is set (what an 85 filter does for a tungsten stock under daylight),
with one scale for all three layers otherwise, so the stock shows its cast. The print
gets the neutral printer light plus `red`, `green`, `blue` offsets in stops, and the last
stock is projected under the same light. `print` may be `ALWAN_FILM_NONE`.

Two things not to do, both learned the hard way. Do not anchor a frame's median or
mean at grey before this call: a low-key scene's median sits stops below a grey card,
and the push makes every stock look overexposed while the film was fine. Push or pull
with `stops`, or take the scale from the camera's exposure through the ISO 12232 model.
And do not tone-map first: the negative's and the print's curves are the picture
formation, and the highlights above white are what the shoulder is for.

Suite 158 holds the call to the same chain spelled out with the public pieces, checks
that +1 stop is exactly twice the light, that the grey card prints within 0.01 of D65 in
xy, that a 3x highlight lands above grey and below paper white, and that an unbalanced
tungsten stock under D65 leaves the grey off neutral.

### The look in two halves

```c
alwan_film_look_expose_f64_map_interleave(h, 24, rgb, 24, pixels, &look, ctx);   /* linear layer exposures */
alwan_film_halation_f64(h, width * 24, width, height, &halation, ctx);            /* a spatial operator */
alwan_film_look_finish_f64_map_interleave(xyz, 24, h, 24, pixels, &look, ctx);    /* the rest of the chain */
```

`expose` stops at the negative's linear layer exposures, calibrated and pushed, before
the log; `finish` takes them through the log, the masking, the curves, the print and
the projection. Back to back they are `alwan_film_render_rgb` to the bit (suite 159).
`finish` itself splits into `alwan_film_look_develop` (to the negative's activations)
and `alwan_film_look_print` (through the print and the projection), again to the bit
(suite 160), so grain can act on the developed negative:

```c
alwan_film_look_develop_f64_map_interleave(d, 24, h, 24, pixels, &look, ctx);
alwan_film_grain_density_f64(d, width * 24, width, height, &grain, ctx);
alwan_film_look_print_f64_map_interleave(xyz, 24, d, 24, pixels, &look, ctx);
```

---

## Halation (experimental)

Light that exposes the emulsion goes on into the base, reflects off its rear surface
and comes back up to expose the emulsion again, a distance away: the glow around a
bright source on a stock without an anti-halation layer, and red first, because the
red-sensitive layer is nearest the base. The kernel is derived, not drawn. Light
entering the base with a Lambertian distribution, reflected at the base-to-air
interface with the unpolarised Fresnel reflectance, totally reflected past the
critical angle, and returning at `r = 2d tan(theta)`, gives

    K(r) proportional to R(theta(r)) / (1 + (r / 2d)^2)^2

a disc suppressed by the Fresnel reflectance (4 % at the centre for n = 1.5), a sharp
rim where total internal reflection starts at `r_c = 2d / sqrt(n^2 - 1)`, and a
`(2d / r)^4` tail. The rim radius in pixels is the parameter; it carries the base
thickness, the pixel pitch and the format in one number.

```c
alwan_film_halation_params p;
alwan_film_halation_params_default(&p, 8.0);        /* rim at 8 px, PET, reach 3, 8 / 3 / 1 percent */
alwan_film_halation_f64(h, width * 3 * sizeof(alwan_f64), width, height, &p, ctx);
```

`H_c := H_c + strength_c (K * H_c)` on linear exposure, edges replicated, the kernel
summing to 1 so each strength is the fraction of the layer's exposure that comes back.
It is a direct convolution, `(2R + 1)^2` taps a pixel with `R = ceil(reach x rim)`.
`alwan_film_halation_kernel` returns the kernel for inspection.

Not testable against a reference: no installed library implements it. Suite 159 pins
the derivation's own statements (energy, symmetry, the rim at `r_c`, the Fresnel
suppression inside it, a point source becoming exactly strength times the kernel, a
flat field staying flat) and the sources it was written from are in the header.
Everything under `experimental/` is research code and may change between releases.

---

## Grain (experimental)

Newson, Faraj, Galerne and Delon's Boolean model: a developed layer is a field of
opaque discs, the dye clouds, with Poisson centres and log-normal radii, and a pixel's
value is the fraction of its area they cover. The process intensity is
`-ln(1 - u) / (pi E[R^2])`, so the expected coverage is `u` itself and the grain is
what the discreteness of the discs adds. Rendered by Monte Carlo as the paper does:
each output pixel takes `samples` points, each jittered by a Gaussian of
`filter_sigma`, and counts how many land inside a disc; each input cell's discs come
from a generator seeded by the cell, the channel and `seed`, so the field is the same
on every run and however the pixels are visited.

```c
alwan_film_grain_params g;
alwan_film_grain_params_default(&g, 0.5);       /* radius 0.5 px, one size, jitter 0.8, 64 samples, seed 1 */
alwan_film_grain_f64(image, width * 3 * sizeof(alwan_f64), width, height, 3, &g, ctx);   /* any [0, 1] image */
alwan_film_grain_density_f64(d, width * 3 * sizeof(alwan_f64), width, height, &g, ctx);  /* a negative's activations */
```

That is the paper's use, on a picture, with the discs as large as the radius makes
them. It is not how a colour negative's density is built: a layer's density is thousands
of dye clouds a fraction of a micrometre across, weakly absorbing, through the depth of
the emulsion, and one opaque disc field per layer at `D = 1` comes out a hundred times
grainier than any datasheet (the first version of the plate showed exactly that).

`alwan_film_grain_density` is the limit that describes the negative. Stack many thin
Boolean fields whose transmittances multiply and `ln T` becomes a sum of small
independent terms: Gaussian, with variance `D ln10 pi r^2 / A` over a sampling area `A`,
which is Selwyn's law, `sigma_D^2 A` proportional to `D`. The sheet's RMS granularity
is that constant measured at `D = 1` through a 48 um aperture, so the size comes from
the sheet and the pixel pitch on the film, and there is no radius to choose:

    sigma_D(pixel) = (rms / 1000) sqrt(D) sqrt(A_48 / pitch^2),   A_48 = pi 24^2 um^2

```c
alwan_film_grain_density_params g;
alwan_film_grain_density_params_default(&g, neg.rms_granularity, 5.86);   /* 500T's 4.4, a 35 mm frame at 4K */
alwan_film_grain_density_f64(d, width * 3 * sizeof(alwan_f64), width, height, &g, ctx);
```

Gaussian on `D` per layer and pixel from a stream seeded by the pixel and the layer; the
mean density is kept exactly and a zero stays zero. Vision3 500T's 4.4 at 5.86 um is
0.032 at `D = 1`; Super 8 at 2K (2.8 um) is 0.066. `layer_scale` weights the three
layers.

No two implementations of a stochastic renderer agree pixel for pixel, so suite 160
pins the models' statements. For the disc model: black stays black, a flat field keeps
its mean, the same seed is the same bits, more samples are less noise, bigger discs are
coarser grain. For the negative: the spread is the formula's to 5 %, a quarter of the
density is half the spread, twice the pitch is half the spread, a zero scale leaves a
layer untouched, and the stock's own grain on a printed grey keeps its value within
five percent. The papers, and Selwyn and Dainty and Shaw for the limit, are named in
the header.

---

## Data

`src/alwan/data/film/` holds one CSV per stock (3,400 values in the layout
`gendata/data/film_stocks.py` documents), the four shared tables, the source's
`LICENSE.txt` and a `SOURCE.txt` naming the commit. The generator refuses to write unless
a shipped profile regenerates bit for bit, so a change in the model's profiling cannot
land a mixed set.

### A second reading of the same sheets

`OwlMightyCh/film-scan-calibration` (data CC-BY-4.0, registered in
`alwan_dev/gendata/datasets.py`, fetched for validation only) digitised the same
manufacturer datasheets independently, with a per-sheet audit of its axis fits.
`alwan_dev/tools/film_cross_check.py` runs the 14 stocks both sources hold through the
built library and compares, measured on 2026-09-22:

| what | how | negatives (11) | reversals (3) |
|---|---|---|---|
| characteristic curve | a neutral exposure at the sheet's log H developed, transmittance, Status M or A density; one log H shift per stock fitted (the sources do not share an exposure origin), RMS after it | G 0.02 to 0.05 D; R 0.08 to 0.25, B 0.04 to 0.14 | 0.01 to 0.05 D, all layers |
| log spectral sensitivity | per-layer offset fitted (alwan's linear sensitivity is in the model's units), RMS of the shape at the 10 nm bands | 0.005 to 0.04 log10 | no file |
| D-min spectrum | 400-700 nm, no fit | 0.015 to 0.09 D, alwan the denser by 0.01 to 0.09 | no file |
| dye shapes | peak-normalised, no fit; the second source's dyes are surrogate fits and say so | Vision3 0.003 to 0.006; the others 0.04 to 0.09 (theirs are warped Vision3 dyes) | 0.003 to 0.011 |
| midscale neutral minus D-min | fitted as three amplitudes of alwan's dyes, residual | 0.002 to 0.012 D | no file |

The one number that needs explaining is the negatives' red and blue curves, because
green and every reversal agree to a few hundredths. It is not digitisation and it is not
alwan: it is how the model holds a negative's curve. For `status_m` stocks
spectral_film_lut inverts the sheet's Status M curves through
`inv(APD^T . dyes . CDD_TO_CID^T)`, the ACES printing-density responsivities and the
CDD-to-CID matrix, "to get appropriate interlayer interaction factors for color
masking", where a reversal's Status A curve is inverted through its own responsivities.
So a negative's activation curve is the sheet's curve in a channel-independent density
space, and reading it back through the Status M responsivities returns the sheet's green
but a red that is low and a blue that is high by an amount that grows with exposure
(Portra 400 at the top of its curve: R 1.64 against the sheet's 2.02, B 3.28 against
3.05). Suite 158 pins alwan to the oracle at 1e-6, so this is the model's decision
carried faithfully, and it matters to one thing: `alwan_film_status_density` of a
developed negative is the model's Status M, not the datasheet's, and anyone comparing to a
sheet should compare green or a reversal. The D-min level, the dye shapes and the
midscale neutral are the sheet's in both sources.
