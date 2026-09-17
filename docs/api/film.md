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

The profiles are spectral_film_lut's (Jan Lohse, MIT): 55 stocks digitised from the
manufacturers' sheets and profiled by its `FilmSpectral`. alwan ships that profiling's
output for 14 of them, on 380-780 nm at 10 nm, and holds this pipeline to the model's
over the same tables (suite 158). The model is float32 throughout, so agreement is
within 5.4e-07 relative and not to the bit. Grain, halation and interlayer diffusion
are spatial and not here.

Every spectrum in this API has `ALWAN_FILM_BANDS` = 41 samples on that grid. Resample
with `alwan_spd_resample`, or hand a Mallett 2019 spectrum (81 samples at 5 nm) to the
map form, which reads every second band.

Every function has an `_f64` and an `_f32` form; this page shows `_f64`.

---

## Stocks

```c
typedef enum {
    ALWAN_FILM_KODAK_5203, ALWAN_FILM_KODAK_5207, ALWAN_FILM_KODAK_5213, ALWAN_FILM_KODAK_5219,
    ALWAN_FILM_KODAK_PORTRA_400, ALWAN_FILM_KODAK_EKTAR_100, ALWAN_FILM_FUJI_ETERNA_500,
    ALWAN_FILM_KODAK_5222,                                          /* black and white negative */
    ALWAN_FILM_KODAK_2383, ALWAN_FILM_KODAK_2393, ALWAN_FILM_FUJI_3513DI,   /* prints */
    ALWAN_FILM_KODAK_2302,                                          /* black and white print */
    ALWAN_FILM_KODAK_EKTACHROME_100D, ALWAN_FILM_FUJI_VELVIA_50,    /* reversals */
    ALWAN_FILM_STOCK_COUNT
} alwan_film_stock;

alwan_status alwan_film_stock_info(alwan_film_stock stock, char const **name, char const **manufacturer);
alwan_status alwan_film_get_profile_f64(alwan_film_profile_f64 *out, alwan_film_stock stock);
```

`alwan_film_stock_info` answers for every stock; `alwan_film_get_profile` returns
`ALWAN_E_NODATA` for one compiled out (`ALWAN_TABLES_FILM`, or the stock's own switch,
`ALWAN_TABLE_FILM_KODAK_5219` and so on). Values are stable; new stocks append.

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

## Data

`src/alwan/data/film/` holds one CSV per stock (3,400 values in the layout
`gendata/data/film_stocks.py` documents), the four shared tables, the source's
`LICENSE.txt` and a `SOURCE.txt` naming the commit. The generator refuses to write unless
a shipped profile regenerates bit for bit, so a change in the model's profiling cannot
land a mixed set.
