# Spectral Operations API

Functions for spectral power distribution (SPD) lifecycle, resampling, integration to XYZ, spectral analysis, hero-wavelength sampling, and RGB-to-spectrum upsampling.

---

## Overview

Spectral operations convert between spectral power distributions (SPDs) and tristimulus values (XYZ) using color matching functions (CMFs) or measured camera sensitivities, and recover spectra from RGB/XYZ.

All spectral entry points are **suffixed by precision**: `_f64` (double, `alwan_f64`) and `_f32` (float, `alwan_f32`); there are no unsuffixed aliases. Pick the suffix that matches the SPD/XYZ types you allocate. The SPD and spectral-upsampling layers are **native dual-precision**: the implementations are templated and instantiated once per precision, with native float data tables on the `_f32` pass, so single precision integrates in float over float data throughout; there is no widen-to-double facade (see [Precision and Limits](../precision-and-limits.md)).

This page shows the `_f64` signatures; every function has an identical `_f32` twin (replace `_f64` with `_f32`, `alwan_f64` with `alwan_f32`, `alwan_spd_f64` with `alwan_spd_f32`, `alwan_xyz_f64` with `alwan_xyz_f32`).

> Argument convention (v2.0): outputs first, `alwan_ctx *ctx` **last** (or absent for pure math). The old `ctx`-second ordering used by earlier docs is gone.

---

## SPD Data Structure

```c
typedef struct {
    alwan_f64 *values;        /* SPD values (power/reflectance/transmittance) */
    alwan_f64  wavelength_min; /* Starting wavelength (nm) */
    alwan_f64  wavelength_max; /* Ending wavelength (nm) */
    size_t     count;          /* Number of samples (uniformly spaced) */
} alwan_spd_f64;               /* and alwan_spd_f32 with alwan_f32 fields */
```

Samples are uniformly spaced across `[wavelength_min, wavelength_max]`.

---

## SPD Lifecycle

### alwan_spd_create

```c
alwan_status alwan_spd_create_f64(alwan_spd_f64 *out,
                                  alwan_f64 wavelength_min,
                                  alwan_f64 wavelength_max,
                                  size_t count,
                                  alwan_ctx *ctx);
```

Allocate an empty SPD with `count` uniformly-spaced samples over the wavelength range. Values are zero-initialized. Returns `ALWAN_OK`, or `ALWAN_E_NOMEM` on allocation failure.

### alwan_spd_destroy

```c
void alwan_spd_destroy_f64(alwan_spd_f64 *spd, alwan_ctx *ctx);
```

Free memory allocated by `alwan_spd_create` or any SPD constructor below.

**Example:**
```c
alwan_spd_f64 spd;
alwan_spd_create_f64(&spd, 380.0, 780.0, 81, ctx);  /* 5 nm steps */
/* ... use spd ... */
alwan_spd_destroy_f64(&spd, ctx);
```

---

## SPD Constructors

### alwan_spd_illuminant

```c
alwan_status alwan_spd_illuminant_f64(alwan_spd_f64 *out,
                                      alwan_illuminant ill,
                                      alwan_ctx *ctx);
```

Load an illuminant SPD (e.g. `ALWAN_ILLUMINANT_D65`) by enum, 471 samples over
360-830 nm at 1 nm. Returns `ALWAN_E_INVALID` for an enum value out of range, and
`ALWAN_E_NODATA` when the value is valid but its table was compiled out.

`alwan_illuminant` holds 111 values in two groups. 0 to 54 are the CIE standards:
A, B, C, the D series, E, F1 to F12, FL3.1 to FL3.15 (spelled `F3_1` to `F3_15`,
and not variants of `F3`), the LED and HP discharge series, and the indoor
daylights ID50 and ID65. 55 to 110 are `ALWAN_ILLUMINANT_LS_*`, colour-science's
`SDS_LIGHT_SOURCES` under colour's own names, most of them real lamps as measured.

The `LS_` prefix keeps them apart from the standards, and two of them show why.
`LS_SA` and `LS_SC` are CIE illuminants A and C as tabulated in RIT's PointerData
spreadsheet: over 380-780 nm they match `ALWAN_ILLUMINANT_A` to 5e-4 and
`ALWAN_ILLUMINANT_C` exactly. Below 380 nm they are the flat hold, while A and C
carry real data, so use A and C. They exist so a lookup by colour's name finds
something.

`LS_INCANDESCENT` and `LS_60_AW_SOFT_WHITE` return identical tables. colour ships
them that way, from two sheets of one NIST spreadsheet.

Every `LS_` source is tabulated over 380-780 nm and held flat over the rest of the
360-830 nm range. They share one compile switch, `ALWAN_TABLES_LIGHT_SOURCES`.

### alwan_spd_iso7589_native, alwan_spd_iso7589

```c
typedef enum {
    ALWAN_ISO7589_PHOTOGRAPHIC_DAYLIGHT, ALWAN_ISO7589_SENSITOMETRIC_DAYLIGHT,
    ALWAN_ISO7589_STUDIO_TUNGSTEN, ALWAN_ISO7589_SENSITOMETRIC_STUDIO_TUNGSTEN,
    ALWAN_ISO7589_PHOTOFLOOD, ALWAN_ISO7589_SENSITOMETRIC_PHOTOFLOOD,
    ALWAN_ISO7589_SENSITOMETRIC_PRINTER
} alwan_iso7589_source;

alwan_status alwan_spd_iso7589_native_f64(alwan_spd_f64 *out, alwan_iso7589_source source, alwan_ctx *ctx);
alwan_status alwan_spd_iso7589_f64(alwan_spd_f64 *out, alwan_iso7589_source source,
                                   alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count,
                                   alwan_resample_method method, alwan_extrapolate_mode extrapolate,
                                   alwan_ctx *ctx);
```

The seven ISO 7589 sensitometric sources. They are not `alwan_illuminant` values because
they do not fit that table: tabulated at 10 nm from 350 to 690 nm (the Printer to 560),
and five of them are still climbing where the tabulation stops. `_native` returns the
tabulation exactly, 35 samples (22 for the Printer). `alwan_spd_iso7589` is that table
and one `alwan_spd_resample` with your method and extrapolation mode, so what happens
past 690 nm is your choice: `ALWAN_EXTRAPOLATE_CONSTANT` is the flat hold the other
illuminants have, `ZERO` is what the CMFs have, `LINEAR_CLAMP_ZERO` continues the end
slope and stops at zero, which the two descending daylight sources reach.

Studio tungsten is also `alwan_spd_iso7589_tungsten`, the Academy's tabulation from the
camera pack on 380-780 nm at 5 nm. Same curve, scale 200 apart, one curve to 1.4e-16 over
380-690 nm once scaled at 560 nm.

### alwan_spd_extend_planckian

```c
alwan_status alwan_spd_extend_planckian_f64(alwan_spd_f64 *dst, alwan_spd_f64 const *src,
                                            alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count,
                                            size_t fit_count, alwan_f64 *temperature_out, alwan_ctx *ctx);
```

An SPD continued past its ends along a Planckian curve, for a source that is near-thermal
where its tabulation stops. The temperature is fitted to the last `fit_count` samples
(0 fits all) with the scale free, so only the window's shape sets T, by golden section
over 1000-25000 K; beyond each end the curve is the Planckian at T through the end sample,
so the result is continuous, and inside `src`'s range it is the linear resample. Think
of it as `ALWAN_EXTRAPOLATE_CONSTANT` with the hold replaced by the continuation.
`temperature_out` may be NULL. `ALWAN_E_INVALID` for fewer than two samples in the window.

Measured (suite 155) on ISO 7589 studio tungsten against the Academy's tabulation of the
same source, which has real values from 695 to 780 nm: the Planckian tail at the fitted
3068 K is within 1.1% of them, the straight line 6.0% high, the flat hold 16.5% low. The
fitted temperature agrees with scipy's to 7e-8 relative.

### alwan_spd_blackbody

```c
alwan_status alwan_spd_blackbody_f64(alwan_spd_f64 *out,
                                     alwan_f64 temperature_K,
                                     alwan_f64 wavelength_min,
                                     alwan_f64 wavelength_max,
                                     size_t count,
                                     alwan_ctx *ctx);
```

Generate a Planckian (blackbody) radiator SPD via Planck's law. `temperature_K` is typically 1000-25000 K; out-of-range returns `ALWAN_E_INVALID`.

**Example:**
```c
alwan_spd_f64 blackbody;
alwan_spd_blackbody_f64(&blackbody, 5500.0, 380.0, 780.0, 81, ctx);
/* 5500 K blackbody, visible range, 5 nm steps */
alwan_spd_destroy_f64(&blackbody, ctx);
```

---

## SPD Resampling

### alwan_spd_resample

```c
alwan_status alwan_spd_resample_f64(alwan_spd_f64 *dst,
                                    alwan_spd_f64 const *src,
                                    alwan_f64 wavelength_min,
                                    alwan_f64 wavelength_max,
                                    size_t count,
                                    alwan_resample_method method,
                                    alwan_extrapolate_mode extrapolate,
                                    alwan_ctx *ctx);
```

Resample an SPD to a new wavelength range and sample count. `dst` is allocated internally.

**Resample methods** (interpolation inside the source range):
```c
typedef enum {
    ALWAN_RESAMPLE_LINEAR = 0,      /* Linear interpolation */
    ALWAN_RESAMPLE_CATMULL_ROM = 1  /* Catmull-Rom spline (smoother) */
} alwan_resample_method;
```

**Extrapolation modes** (for samples outside the source range):
```c
typedef enum {
    ALWAN_EXTRAPOLATE_ZERO = 0,     /* Clamp to zero (default for reflectance) */
    ALWAN_EXTRAPOLATE_CONSTANT = 1, /* Repeat edge values (good for smooth SPDs) */
    ALWAN_EXTRAPOLATE_LINEAR = 2    /* Linear extrapolation from the edge slope */
} alwan_extrapolate_mode;
```

---

## SPD to XYZ Integration

### alwan_xyz_from_spd

```c
alwan_status alwan_xyz_from_spd_f64(alwan_xyz_f64 *xyz_out,
                                    alwan_spd_f64 const *spd,
                                    alwan_spd_f64 const *illuminant,
                                    alwan_observer_type observer,
                                    alwan_integrate_method method,
                                    alwan_f64 bandpass_nm,
                                    alwan_ctx *ctx);
```

Integrate an SPD against an observer's CMFs to obtain XYZ tristimulus values.

| Argument | Meaning |
|----------|---------|
| `spd` | Reflectance or emission SPD. |
| `illuminant` | Illuminant SPD to weight a reflectance by, or `NULL` if `spd` is already illuminant-weighted (emission). |
| `observer` | Standard observer / CMF set (see below). |
| `method` | Numerical integration rule (see below). |
| `bandpass_nm` | Bandpass width for the Stearns & Stearns bandpass correction; `0` disables it. |

**Observer types:**
```c
typedef enum {
    ALWAN_OBSERVER_CIE_1931_2DEG = 0,        /* CIE 1931 2-degree */
    ALWAN_OBSERVER_CIE_1964_10DEG = 1,       /* CIE 1964 10-degree */
    ALWAN_OBSERVER_CIE_2012_2DEG = 2,        /* same observer as CIE_2015_2DEG */
    ALWAN_OBSERVER_CIE_2012_10DEG = 3,       /* same observer as CIE_2015_10DEG */
    ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG = 4, /* Stockman & Sharpe 2000 2-degree cone fundamentals */
    ALWAN_OBSERVER_CIE_2015_2DEG = 5,        /* CIE 170-2:2015 2-degree cone-fundamental-based */
    ALWAN_OBSERVER_CIE_2015_10DEG = 6,       /* CIE 170-2:2015 10-degree cone-fundamental-based */
    ALWAN_OBSERVER_WRIGHT_GUILD_1931 = 7,    /* Wright & Guild 1931 2-degree RGB CMFs (historical) */
    ALWAN_OBSERVER_STOCKMAN_SHARPE_10DEG = 8,   /* Stockman & Sharpe 10-degree, 390-830 */
    ALWAN_OBSERVER_SMITH_POKORNY_1975 = 9,      /* Smith & Pokorny 1975, 380-780 */
    ALWAN_OBSERVER_STILES_BURCH_1955_2DEG = 10, /* Stiles & Burch 1955 2-degree RGB, 390-730 */
    ALWAN_OBSERVER_STILES_BURCH_1959_10DEG = 11 /* Stiles & Burch 1959 10-degree RGB, 390-830 */
} alwan_observer_type;
```

CIE 2012 and CIE 2015 are one observer under two names: the CMFs derived from the
CIE 2006 cone fundamentals, published by CVRL as the 2012 proposal and standardised
as CIE 170-2:2015. Both enumerators return the same CMFs bit for bit from one table
set, so either name works. The 2015 names are the standard's.

**Integration methods:**
```c
typedef enum {
    ALWAN_INTEGRATE_TRAPEZOID = 0,  /* Trapezoidal rule (fast) */
    ALWAN_INTEGRATE_SIMPSON = 1     /* Simpson's rule (more accurate) */
} alwan_integrate_method;
```

> **Reference-matching note.** alwan integrates with the trapezoidal or **Simpson** rule. Some reference libraries (e.g. colour-science) compute the tristimulus integral as a plain **Riemann summation** of `CMF * SPD * Deltalambda`. To reproduce such reference values exactly, match the quadrature on the reference side (colour-science exposes `scipy.integrate.simpson`) and use linear interpolation / matched wavelength sampling; see the project gendata notes.

**Example:**
```c
alwan_spd_f64 d65;
alwan_spd_illuminant_f64(&d65, ALWAN_ILLUMINANT_D65, ctx);

alwan_xyz_f64 white;
alwan_xyz_from_spd_f64(&white, &d65, NULL,
                       ALWAN_OBSERVER_CIE_1931_2DEG,
                       ALWAN_INTEGRATE_SIMPSON, 5.0, ctx);

alwan_spd_destroy_f64(&d65, ctx);
```

Any `method` other than the two rules is `ALWAN_E_INVALID`; before 3.0.0 it ran Simpson.

### alwan_xyz_from_spd_astm_e308

```c
typedef struct {
    int observer_range;  /* 0: 360-780 nm; non-zero: 360-830 nm */
    int tables_at_5nm;   /* 0: omission method; non-zero: E2022 tables */
    int tables_at_20nm;  /* 0: interpolate to 10 nm first; non-zero: tables at 20 nm */
} alwan_astm_e308_params;

alwan_status alwan_xyz_from_spd_astm_e308_f64(alwan_xyz_f64 *xyz_out,
                                              alwan_spd_f64 const *spd,
                                              alwan_spd_f64 const *illuminant,
                                              alwan_observer_type observer,
                                              alwan_astm_e308_params const *params,
                                              alwan_ctx *ctx);
```

XYZ by ASTM E308, scaled so a perfect reflector has Y = 100. This is the number a
spectrophotometer report carries; `alwan_xyz_from_spd` is an integral and is not
normalised.

| Data interval | Method |
|---------------|--------|
| 1 nm | Summed on the samples over the range. |
| 5 nm | Omission: summed on every fifth 1 nm sample. With `tables_at_5nm`, E2022 tables. |
| 10 nm | E2022 weighting tables. |
| 20 nm | Interpolated to 10 nm (ASTM's cubic, extrapolated 20 nm each side), then 10 nm tables. With `tables_at_20nm`, 20 nm tables. |

The tables are built from the 1 nm observer and the illuminant sampled at every
nanometre (linear between its samples, its end values beyond them). A spectrum that
stops short of the range keeps its end values in the summed methods; in the tabular
ones the weights past its ends are added to its end weights, the E308 adjustment.
`illuminant` `NULL` is the equal-energy illuminant; `params` `NULL` is all zeros.

The samples must lie on whole nanometres at 1, 5, 10 or 20 nm, a multiple of the
interval from 360 nm, inside the range. Anything else is `ALWAN_E_INVALID`.

Reference: colour-science 0.4.7 `sd_to_XYZ_ASTME308`, on alwan's own observer and
illuminant data. XYZ agrees to 7e-14 (suite 129).

### alwan_astm_e2022_weights

```c
alwan_status alwan_astm_e2022_weights_f64(alwan_f64 *weights_out,
                                          size_t *node_count,
                                          alwan_spd_f64 const *illuminant,
                                          alwan_observer_type observer,
                                          int interval_nm,
                                          int observer_range,
                                          alwan_ctx *ctx);
```

The E2022 weighting tables: `node_count` rows of X, Y, Z weights from 360 nm at
`interval_nm` (5, 10 or 20), with the Y weights summing to 100. Pass
`weights_out` `NULL` to get the count. Agrees with colour-science's
`tristimulus_weighting_factors_ASTME2022` to 9e-16.

---

## Camera Sensitivities

Cameras are identified by an index into the camera registry: `alwan_camera_find` by make
and model, `alwan_camera_count` for the slot count, `alwan_camera_info` for the names.
Slots 0 to 51 are the rawtoaces-data pack on 380-780 nm at 5 nm. Slots 52 and 53 are
the two NPL cameras, `"Nikon" "5100 (NPL)"` and `"Sigma" "SDMerill (NPL)"`, on 360-830 nm
at 1 nm. An index never moves, and a camera whose table is compiled out answers
`ALWAN_E_NODATA` from its slot.

### alwan_camera_sensitivities

```c
alwan_status alwan_camera_sensitivities_f64(alwan_spd_f64 *spd_r, alwan_spd_f64 *spd_g,
                                            alwan_spd_f64 *spd_b, size_t index, alwan_ctx *ctx);
```

The camera's R, G and B spectral sensitivities, created by the call on the camera's own
grid. `ALWAN_E_RANGE` for an index past the registry.

### alwan_camera_rgb_from_spd

```c
alwan_status alwan_camera_rgb_from_spd_f64(alwan_rgb_f64 *rgb_out,
                                           alwan_spd_f64 const *spd,
                                           alwan_spd_f64 const *illuminant,
                                           size_t camera,
                                           alwan_integrate_method method,
                                           alwan_ctx *ctx);
```

Like `alwan_xyz_from_spd`, but integrates against a camera's RGB sensitivities instead of
a standard observer. The sensitivities are resampled onto the SPD's grid, linearly and
zero outside their own range; `illuminant` may be `NULL` for an already-weighted SPD.

**Changed in 3.0.0.** `camera` was an `alwan_camera_sensitivity` enum naming the two NPL
cameras, and the same integration was also `alwan_xyz_from_spd_camera`, which returned
camera RGB in an `alwan_xyz`. The enum, that function and `alwan_spd_camera_sensitivity`
are removed:

| before 3.0.0 | from 3.0.0 |
|---|---|
| `ALWAN_CAMERA_NIKON_5100` | registry index 52 |
| `ALWAN_CAMERA_SIGMA_SDMERILL` | registry index 53 |
| `alwan_xyz_from_spd_camera_{T}(&xyz, spd, ill, cam, m, ctx)` | `alwan_camera_rgb_from_spd_{T}(&rgb, spd, ill, index, m, ctx)`, reading `.r .g .b` for `.x .y .z` |
| `alwan_spd_camera_sensitivity_{T}` into three pre-created 471-sample SPDs | `alwan_camera_sensitivities_{T}`, which creates them |

The two NPL cameras give the same numbers, to the bit, through the new calls.

---

## SPD Shape Analysis

### alwan_spd_analyze_shape

```c
alwan_status alwan_spd_analyze_shape_f64(alwan_spd_shape_f64 *shape_out,
                                         alwan_spd_f64 const *spd);
```

Compute descriptive statistics for an SPD. Pure analysis; no `ctx`.

```c
typedef struct {
    alwan_f64 peak_wavelength;  /* Peak wavelength (nm) */
    alwan_f64 peak_value;       /* Peak power/reflectance value */
    alwan_f64 fwhm;             /* Full width at half maximum (nm) */
    alwan_f64 centroid;         /* Weighted mean wavelength (nm) */
    alwan_f64 bandwidth;        /* Total wavelength range (nm) */
} alwan_spd_shape_f64;          /* and alwan_spd_shape_f32 */
```

---

## Reflectance / Metamer Recovery from XYZ

### alwan_optimize_spectrum_for_xyz

```c
alwan_status alwan_optimize_spectrum_for_xyz_f64(alwan_spd_f64 *spd_out,
                                                 alwan_xyz_f64 const *target_xyz,
                                                 alwan_observer_type observer,
                                                 alwan_ctx *ctx);
```

Find a smooth SPD whose tristimulus integral matches `target_xyz` (least-squares). `spd_out` **must be pre-allocated** (`alwan_spd_create_f64`) with the desired wavelength range/count. Multiple spectra metamerise to the same XYZ; this returns one solution.

### alwan_metamerism_index

```c
alwan_f64 alwan_metamerism_index_f64(alwan_spd_f64 const *sample_reflectance,
                                     alwan_spd_f64 const *reference_reflectance,
                                     alwan_spd_f64 const *reference_illuminant,
                                     alwan_spd_f64 const *test_illuminant,
                                     alwan_observer_type observer,
                                     alwan_ctx *ctx);
```

CIE Special Metamerism Index (change in illuminant): the DeltaE\*ab between a metameric sample/reference pair under the test illuminant. Returns a negative value on error. (See also [CCT & Light Quality](cct-light-quality.md) for CRI/TM-30/SSI.)

---

## Hero-Wavelength Spectral Sampling

For Monte-Carlo spectral rendering. Maps a uniform `[0,1]` sample to a wavelength over `[380, 780] nm` and evaluates analytic CMFs (Wyman 2013 fit).

```c
/* Single-sample: u in [0,1] -> lambda in [380,780] nm */
alwan_status alwan_hero_wavelength_sample_f64(alwan_f64 *lambda_out, alwan_f64 u);

/* Wavelength -> XYZ via Wyman 2013 analytic CMF fit (no ctx) */
void alwan_hero_wavelength_to_xyz_f64(alwan_xyz_f64 *xyz_out, alwan_f64 lambda);

/* Stratified batch: generate `count` wavelengths from a single seed.
 * xyz_weights receives per-sample XYZ importance weights (may be NULL). */
alwan_status alwan_hero_wavelength_batch_f64(alwan_f64 *lambda_out,
                                              alwan_xyz_f64 *xyz_weights,
                                              size_t count,
                                              alwan_f64 seed);
```

---

## RGB to Spectrum (Spectral Upsampling)

Recover a plausible reflectance SPD from a colour. Three of these take an RGB triplet. Otsu 2018 takes XYZ, because its cluster selector is defined over chromaticity and so has no RGB space to assume. `out_spd` is allocated internally by each call (fixed wavelength range/count per method).

### alwan_rgb_to_spectrum_smits1999

```c
alwan_status alwan_rgb_to_spectrum_smits1999_f64(alwan_spd_f64 *out_spd,
                                                 alwan_rgb_f64 const *rgb,
                                                 alwan_ctx *ctx);
```

Smits 1999 basis-spectra mixing. Input is sRGB, clamped to `[0,1]`. Output: **380-720 nm, 10 samples**. Fast; intended for spectral rendering.

### alwan_rgb_to_spectrum_mallett2019

```c
alwan_status alwan_rgb_to_spectrum_mallett2019_f64(alwan_spd_f64 *out_spd,
                                                   alwan_rgb_f64 const *rgb,
                                                   alwan_ctx *ctx);
```

Mallett & Yuksel 2019 spectral primary decomposition. Input is sRGB. Output: **380-780 nm, 81 samples at 5 nm**. Higher spectral fidelity than Smits.

### alwan_rgb_to_spectrum_jakob2019

```c
alwan_status alwan_rgb_to_spectrum_jakob2019_f64(alwan_spd_f64 *out_spd,
                                                 alwan_jakob2019_gamut gamut,
                                                 alwan_rgb_f64 const *rgb,
                                                 alwan_ctx *ctx);
```

Jakob & Hanika 2019 polynomial coefficient model. Input RGB is in the selected `gamut`, clamped to `[0,1]`. Output: **360-780 nm, 85 samples at 5 nm**.

> **Requires generated LUT data.** Jakob2019 reads a per-gamut polynomial coefficient table embedded from `src/alwan/data/spectral_lut/**`. These tables are produced by the gendata pipeline (`generate_data.ps1` in the `alwan_dev` repo) and compiled in via `ALWAN_EMBED_DATA`. If a gamut's table was not generated, the call returns an error. Smits1999 and Mallett2019 use small embedded basis spectra and do not need this step.

**Gamut types:**
```c
typedef enum {
    ALWAN_JAKOB2019_SRGB = 0,     /* sRGB / Rec.709 primaries */
    ALWAN_JAKOB2019_PROPHOTO_RGB, /* ProPhoto RGB (wide gamut) */
    ALWAN_JAKOB2019_ACES2065_1,   /* ACES2065-1 */
    ALWAN_JAKOB2019_REC2020,      /* ITU-R BT.2020 */
    ALWAN_JAKOB2019_ERGB,         /* Extended RGB */
    ALWAN_JAKOB2019_XYZ           /* CIE XYZ */
} alwan_jakob2019_gamut;
```

### alwan_xyz_to_spectrum_otsu2018

```c
alwan_status alwan_xyz_to_spectrum_otsu2018_f64(alwan_spd_f64 *out_spd,
                                                alwan_xyz_f64 const *xyz,
                                                alwan_ctx *ctx);
```

Otsu, Yamamoto and Hachisuka 2018. A decision tree over CIE xy selects one of eight clusters, and the reflectance is that cluster's mean plus a weighted sum of its three basis functions. Input is XYZ on the Y = 1 scale under D65, which is what the embedded cluster matrices were built for. Output: **380-730 nm, 36 samples at 10 nm**, clamped to `[0,1]`.

The cluster's basis-to-XYZ matrix is embedded already inverted, with the XYZ of its mean beside it. The reference implementation rebuilds both by integrating the basis functions against the CMFs and the illuminant on every call, but neither depends on the stimulus, so they are constants. What remains at runtime is a tree walk, a 3x3 multiply and a weighted sum: no spectral integration, and no CMF table needed, which is what keeps this method available where one is not.

Returns `ALWAN_E_NODATA` when the Otsu tables are compiled out (`ALWAN_TABLE_OTSU2018`).

### Image buffers: `_map_interleave` and `_map_interleave_ex`

```c
size_t bands = 0;
alwan_rgb_to_spectrum_mallett2019_f64_map_interleave(NULL, 0, NULL, 0, 0, &bands); /* 81 */
alwan_f64 *cube = malloc(pixels * bands * sizeof(alwan_f64));
alwan_rgb_to_spectrum_mallett2019_f64_map_interleave(cube, bands * sizeof(alwan_f64),
                                                     rgb, 3 * sizeof(alwan_f64), pixels, &bands);

/* u16 colours straight from a decoder, f32 spectra for the renderer */
alwan_rgb_to_spectrum_mallett2019_map_interleave_ex(cube32, bands * sizeof(float),
                                                    rgb16, 3 * sizeof(uint16_t), pixels,
                                                    ALWAN_PIXEL_F32, ALWAN_PIXEL_U16, &bands);
```

Each method has a bulk form per precision and one `_ex` form. The bulk forms share
their body with the per-colour call, so a buffer is its pixels bit for bit (suite 151).
`out` NULL is a shape query that reports the method's band count; a `band_count` that
does not match is `ALWAN_E_INVALID`. Strides are in bytes on both sides. Jakob 2019
takes its `gamut` after `band_count`.

The `_ex` forms take `void` pointers and an `alwan_pixel_format` for each side, in the
`(out_fmt, in_fmt)` order of the other `_map_interleave_ex`. Both formats f32 runs the
f32 bulk form, both f64 the f64 one, and either result is that form's exactly. Any other
pair tiles through f64 when either side is f64 and f32 otherwise, so the result is the
tiled precision's bulk form with the edges converted: u16 in is `v / 65535` in that
precision, u8 in `v / 255`, f16 both ways through the half conversion, and an integer out
is rounded and clamped. A tile holds fewer pixels the more bands there are, 72 at 85
bands, so the buffers stay on the stack (suite 157).

---

## Observers as SPDs

### alwan_spd_observer_{T}

```c
alwan_spd_f64 xbar, ybar, zbar;
alwan_spd_observer_f64(&xbar, &ybar, &zbar, ALWAN_OBSERVER_CIE_1931_2DEG, ctx);
/* 360-830 nm at 1 nm; destroy all three */
```

An observer's three functions: x-bar, y-bar and z-bar, or L, M and S for
`ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG`.

---

## Generated Sources

### alwan_spd_cie_daylight_{T}

```c
alwan_vec2_f64 xy;
alwan_d_series_illuminant_xy_f64(&xy, 6500.0 * 1.4388 / 1.4380); /* D65 */
alwan_spd_f64 d;
alwan_spd_cie_daylight_f64(&d, &xy, 1, 380.0, 780.0, 401, ctx);
```

CIE 015 daylight, S0 + M1 S1 + M2 S2, at any chromaticity. `round_m1_m2` rounds M1
and M2 to three decimals, as CIE 015 does for the tabulated illuminants. The grid
must lie inside 360-830 nm (`ALWAN_E_RANGE` otherwise), and the basis is
interpolated linearly between its 5 nm samples.

### alwan_spd_gaussian_{T} / alwan_spd_led_ohno2005_{T}

A Gaussian of peak 1 and the given FWHM, and Ohno's (2005) LED model,
(g + 2 g^5) / 3 with g = exp(-((lambda - peak) / half_width)^2), for one LED or a
weighted sum of several. They match colour-science's `sd_gaussian(method="FWHM")`,
`sd_single_led_Ohno2005` and `sd_multi_leds_Ohno2005`.

---

## Multispectral Integration

### alwan_spectral_weights_{T} / alwan_spectral_to_tristimulus_{T}_map_interleave

```c
enum { BANDS = 31 };                         /* 400-700 nm at 10 nm */
alwan_f64 w[3 * BANDS];
alwan_spectral_weights_observer_f64(w, BANDS, 400.0, 700.0, &d65,
                                    ALWAN_OBSERVER_CIE_1931_2DEG,
                                    ALWAN_INTEGRATE_TRAPEZOID, 1, ctx);
alwan_spectral_to_tristimulus_f64_map_interleave(xyz, 3 * sizeof(alwan_f64),
                                                 cube, BANDS * sizeof(alwan_f64),
                                                 pixel_count, BANDS, w);
```

The weights fold the illuminant, three responses and the integration rule into a
3 x N matrix, so channel c of a spectrum is sum_i w[c N + i] s_i: what
`alwan_xyz_from_spd` computes for that spectrum. `normalize` scales the rows so a
perfect reflector has Y = 1. `alwan_spectral_weights_{T}` takes any three
responses: an observer, cone fundamentals, or a camera.

`alwan_spectral_to_tristimulus_map_interleave_ex` is the same sum over a typed cube:
`void` pointers, an `alwan_pixel_format` per side, weights as `alwan_f64`. It always sums
in f64 and stores to `out_fmt`, so an f32/f32 call gets the f64 sum narrowed rather than
the f32 bulk form's sum; f64/f64 is the f64 bulk form exactly.

---

## Camera Characterisation

The camera pack is rawtoaces-data (Apache-2.0): 52 cameras on 380-780 nm at 5 nm,
the 190-patch IDT training set, and ISO 7589 studio tungsten. The f32 entry points
widen, compute in f64 and narrow.

### Finding a camera

```c
size_t cam;
if (alwan_camera_find(&cam, "Canon", "EOS 5D Mark II") == ALWAN_OK) {
    alwan_spd_f64 r, g, b;
    alwan_camera_sensitivities_f64(&r, &g, &b, cam, ctx);
}
```

Make and model compare ASCII case-insensitively, through rawtoaces' alias tables.
`alwan_camera_count` and `alwan_camera_info` enumerate the pack. An index is
stable: new cameras are appended.

### alwan_idt_matrix_{T}

```c
alwan_mat3x3_f64 idt;
alwan_rgb_f64 wb;
alwan_idt_matrix_f64(&idt, &wb, &r, &g, &b, &d55, NULL, 0, NULL, ctx);
alwan_camera_rgb_to_aces2065_1_f64_map_interleave(aces, stride, raw, stride, n,
                                                  &idt, &wb, 1.0, 1);
```

ACES P-2013-001 Method A as rawtoaces v1 computes it. The training reflectances
(NULL for the built-in 190) are rendered under the illuminant into white-balanced
camera RGB and, through the CIE 1931 observer, into XYZ adapted to the ACES white
with CAT02. The 3x3 is fitted by BFGS with every row summing to 1, so camera white
maps to ACES white. `alwan_idt_params` picks the objective (CIE Lab by default, or
Jzazbz), can skip the adaptation, and sets the iteration budget; the zero value is
the default. It matches `colour.matrix_idt` to the precision that optimiser
reaches, about 1e-8.

### alwan_camera_sensitivities_from_chart_{T}

```c
alwan_spd_f64 r, g, b;
alwan_camera_sensitivities_from_chart_f64(&r, &g, &b, chart_rgb, 3 * sizeof(alwan_f64),
                                          patches, 24, &d65, 0, ctx);
alwan_idt_matrix_f64(&idt, &wb, &r, &g, &b, &d65, NULL, 0, NULL, ctx);
```

For a camera the pack does not hold: Jiang, Liu, Gu and Suesstrunk 2013. The input is
the linear, black-subtracted response to each patch of a chart whose reflectances are
known, shot under a known light. Each channel is fitted by least squares as a
combination of principal components of the 52 rawtoaces-data cameras (all 6 when
`basis_components` is 0), and the three curves are scaled together to a peak of 1.
With the same basis it matches `colour.recovery.RGB_to_msds_camera_sensitivities_Jiang2013`
to 2e-14.

The basis is alwan's own, derived by colour-science's `PCA_Jiang2013` from data whose
licence is known; colour-science's `BASIS_FUNCTIONS_DYER2017` is not in the pinned
rawtoaces-data. A basis spans what its cameras share, so the recovery gives the
shape of a sensor, not its fine structure.

Measured on a ColorChecker 24 under D65 with noise-free responses, four cameras
from the pack come back within 0.007 to 0.026 RMS of their measured curves, and
within 0.010 to 0.052 when the basis is rebuilt without the camera. Close curves do
not guarantee the same IDT: the chart through the recovered Canon EOS 5D Mark II
lands within 0.07 ΔE00 of the measured camera, the Nikon D5100 within 1.9. Noise
costs more than the basis does. 1 % noise on the chart raises the error to 0.018 to
0.12 with 6 components and 0.028 to 0.055 with 3, so pass fewer components for a
noisy chart, and use more patches with more spectral variety where possible.

### alwan_spd_to_aces2065_1_{T}

Spectral radiance, or a reflectance with the illuminant lighting it, to ACES2065-1
relative exposure values through the Academy's Reference Input Capture Device, with
the ACES 0.5 % flare and CAT02 to the ACES white. This is
`colour.sd_to_aces_relative_exposure_values`, with the illuminant white taken over
360-830 nm ([alwan_decisions.md](../alwan_decisions.md)).

---

## DNG Colour Model

```c
alwan_dng_profile_f64 p = { 0 };
p.calibration_cct_1 = 2856.0;              /* Standard light A */
p.calibration_cct_2 = 6504.0;              /* D65 */
/* color_matrix_1/2, forward_matrix_1/2 ... from the DNG tags; all zero = absent */
alwan_vec2_f64 white;
alwan_dng_camera_neutral_to_xy_f64(&white, &p, &as_shot_neutral);
alwan_mat3x3_f64 to_xyz;
alwan_dng_camera_to_xyz_matrix_f64(&to_xyz, &p, &white, ALWAN_CAT_BRADFORD);
```

The tags are interpolated in inverse CCT between the two calibration illuminants,
at the white's CCT (Robertson 1968). `alwan_dng_camera_to_xyz_matrix` gives camera
space to XYZ under the connection white, D50 at (0.3457, 0.3585): through the
ForwardMatrix tags when the profile has them, otherwise through the inverse colour
matrix and a chromatic adaptation. It matches colour-hdri's `colour_hdri.models.dng`
to 2e-15; [alwan_decisions.md](../alwan_decisions.md) lists the two places alwan
reads a profile differently.

### alwan_highlights_recovery_blend_{T}_map_interleave

dcraw's highlight blend for white-balanced camera RGB: channels clipped at
min(multipliers) x threshold, each pixel keeping its lightness and taking the chroma
magnitude of its clipped version, so clipped highlights stay neutral.

---

## Bayer Demosaicing

```c
alwan_cfa_bayer_demosaic_f64(rgb, 3 * width * sizeof(alwan_f64),
                             cfa, width * sizeof(alwan_f64), width, height,
                             ALWAN_CFA_RGGB, ALWAN_DEMOSAIC_MENON2007);
```

The input is a linear, black-subtracted CFA plane; decoding raw files stays with
LibRaw or the DNG SDK. Bilinear averages each channel's own sites. Malvar, He and
Cutler 2004 adds a gradient correction from the other channels with fixed 5 x 5
filters. Menon, Andriani and Calvagno 2007 interpolates green horizontally and
vertically, keeps the direction with the smaller colour-difference gradient, and
can refine all three channels afterwards.

The borders are colour-demosaicing's, the reference: bilinear and Malvar extend the
image by repeating the edge sample, Menon's one-dimensional filters mirror it
without repeating it, and its direction decision reads zero outside. The sums run
in scipy's order, and the results match colour-demosaicing bit for bit.

To compare the methods, use `alwan_psnr_{T}` in [hdr.md](hdr.md), which reports CPSNR
with a border crop because the edges are where demosaicing is least reliable and where
the literature stops measuring. On alwan's own test image: bilinear 30.4 dB, Malvar
33.6, Menon 33.9 without its refining step and 34.2 with it.

---

## Error Codes

Spectral functions return the `alwan_status` enum:

- `ALWAN_OK` (0): Success
- `ALWAN_E_INVALID` (-1): Invalid parameter (unsupported illuminant/camera/gamut, bad range)
- `ALWAN_E_NODATA` (-2): Required spectral data not present
- `ALWAN_E_NOMEM` (-4): Memory allocation failed

`alwan_metamerism_index_*` instead returns the index directly (negative on error).

---

## Camera characterisation helpers

The pieces the IDT and DNG paths are built from, callable on their own.

### alwan_idt_params_init

```c
void alwan_idt_params_init(alwan_idt_params *params);
```

Sets the defaults an IDT fit uses when handed no parameters: `objective`
`ALWAN_IDT_OBJECTIVE_LAB`, `skip_chromatic_adaptation` 0, `max_iterations` 0
(the fit's own default). A NULL `params` is ignored. Zeroing the struct is not
the same thing, since the objective enum's zero may not be the default.

### alwan_idt_training_count, alwan_spd_idt_training_{T}

```c
size_t alwan_idt_training_count(void);
alwan_status alwan_spd_idt_training_{T}(alwan_spd_{T} *out, size_t patch_index, alwan_ctx *ctx);
```

The IDT training reflectances: how many there are, and one of them as an SPD
on 380-780 nm at 5 nm, allocated through `ctx` and released with
`alwan_spd_destroy_{T}`. The count is 190 with the table compiled in and 0
without (`ALWAN_TABLE_IDT_TRAINING_190`), in which case the accessor is
`ALWAN_E_NODATA`; `patch_index` at or past the count is `ALWAN_E_RANGE`.

### alwan_idt_white_balance_{T}

```c
alwan_status alwan_idt_white_balance_{T}(alwan_rgb_{T} *white_balance_out,
                                         alwan_spd_{T} const *sens_r, alwan_spd_{T} const *sens_g,
                                         alwan_spd_{T} const *sens_b, alwan_spd_{T} const *illuminant);
```

White balance multipliers of a camera under an illuminant: `1 / sum(sensitivity
x illuminant)` per channel, scaled so the smallest multiplier is 1. The three
sensitivities must share one grid; the illuminant is read on that grid.

### alwan_best_illuminant_{T}

```c
alwan_status alwan_best_illuminant_{T}(size_t *index_out, alwan_{T} *sse_out,
                                       alwan_rgb_{T} const *white_balance,
                                       alwan_spd_{T} const *sens_r, alwan_spd_{T} const *sens_g,
                                       alwan_spd_{T} const *sens_b,
                                       alwan_spd_{T} const *candidates, size_t candidate_count);
```

Which of a set of candidate illuminants a camera was shot under, given the
white balance it needed. For each candidate the multipliers it would call for
are computed as `alwan_idt_white_balance` does, and the closest to the measured
`white_balance` wins, by the sum over channels of
`(candidate_multiplier / measured_multiplier - 1)^2`: a relative error with no
separate normalising step. `index_out` receives the winner's position in
`candidates`; `sse_out`, when not NULL, its error, 0 when the measurement came
from a candidate exactly. Equal errors keep the earlier candidate.

The candidates are the caller's, read on the sensitivities' grid, so this does
not decide what a sensible bank is. colour-science's rawtoaces v1 bank is 50
sources on 380-780 nm at 5 nm: daylights every 500 K from 4000 K to 25000 K,
blackbodies from 1000 K to 3500 K, and ISO 7589 studio tungsten, all of which
`alwan_spd_cie_daylight`, `alwan_spd_blackbody` and `alwan_spd_iso7589_tungsten`
build.

**Returns:** `ALWAN_E_INVALID` for no candidates or a measured multiplier that
is not positive, since the error divides by it; `ALWAN_E_RANGE` for a candidate
that integrates to nothing in some channel, reported rather than passed over,
so a degenerate entry in the array is visible. Matches
`colour.characterisation.best_illuminant`.

### alwan_dng_interpolate_matrix_{T}, alwan_dng_xyz_to_camera_matrix_{T}, alwan_dng_xy_to_camera_neutral_{T}

```c
alwan_status alwan_dng_interpolate_matrix_{T}(alwan_mat3x3_{T} *matrix_out, alwan_{T} cct,
                                              alwan_{T} cct_1, alwan_{T} cct_2,
                                              alwan_mat3x3_{T} const *m1, alwan_mat3x3_{T} const *m2);
alwan_status alwan_dng_xyz_to_camera_matrix_{T}(alwan_mat3x3_{T} *matrix_out,
                                                alwan_dng_profile_{T} const *profile,
                                                alwan_vec2_{T} const *white_xy);
alwan_status alwan_dng_xy_to_camera_neutral_{T}(alwan_rgb_{T} *neutral_out,
                                                alwan_dng_profile_{T} const *profile,
                                                alwan_vec2_{T} const *white_xy);
```

The three steps of the DNG colour model, separately. `interpolate_matrix` is one
tag at a CCT: `m1` at or below `cct_1`, `m2` at or above `cct_2`, linear in
1/CCT between; the two illuminants may come in either order.
`xyz_to_camera_matrix` is XYZ to camera space at a white, AnalogBalance x
CameraCalibration x ColorMatrix, each interpolated at the white's CCT.
`xy_to_camera_neutral` is the camera neutral of a white, what AsShotNeutral
stores, with G = 1.

### alwan_cfa_bayer_mosaic_{T}

```c
alwan_status alwan_cfa_bayer_mosaic_{T}(alwan_{T} *cfa_out, size_t cfa_row_stride,
                                        alwan_{T} const *rgb, size_t rgb_row_stride,
                                        size_t width, size_t height, alwan_cfa_pattern pattern);
```

The mosaic an RGB image would record: each site keeps its layout's channel.
The inverse of the demosaicers, and what their tests mosaic their inputs with.

---

## See Also

- [CCT & Light Quality](cct-light-quality.md): CRI, TM-30, SSI, metamerism
- [Color Spaces](color-spaces.md): XYZ conversions
- [Atmospheric Optics](atmosphere.md): Rayleigh scattering
- [Precision and Limits](../precision-and-limits.md): `_f32`/`_f64` and f64-facade behaviour
- [Data Management](../data-management.md): embedded CMF, illuminant, and spectral LUT data
