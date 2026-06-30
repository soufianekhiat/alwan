# Spectral Operations API

Functions for spectral power distribution (SPD) lifecycle, resampling, integration to XYZ, spectral analysis, hero-wavelength sampling, and RGB→spectrum upsampling.

---

## Overview

Spectral operations convert between spectral power distributions (SPDs) and tristimulus values (XYZ) using color matching functions (CMFs) or measured camera sensitivities, and recover spectra from RGB/XYZ.

All spectral entry points are **suffixed by precision** — `_f64` (double, `alwan_f64`) and `_f32` (float, `alwan_f32`); there are no unsuffixed aliases. Pick the suffix that matches the SPD/XYZ types you allocate. The SPD and spectral-upsampling layers are **native dual-precision**: the implementations are templated and instantiated once per precision, with native float data tables on the `_f32` pass, so single precision integrates in float over float data throughout — there is no widen-to-double facade (see [Precision and Limits](../precision-and-limits.md)).

This page shows the `_f64` signatures; every function has an identical `_f32` twin (swap `_f64`→`_f32`, `alwan_f64`→`alwan_f32`, `alwan_spd_f64`→`alwan_spd_f32`, `alwan_xyz_f64`→`alwan_xyz_f32`).

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
int alwan_spd_create_f64(alwan_spd_f64 *out,
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
int alwan_spd_illuminant_f64(alwan_spd_f64 *out,
                             alwan_illuminant ill,
                             alwan_ctx *ctx);
```

Load a standard illuminant SPD (e.g. `ALWAN_ILLUMINANT_D65`) by enum. Returns `ALWAN_E_INVALID` if the illuminant is not supported.

### alwan_spd_blackbody

```c
int alwan_spd_blackbody_f64(alwan_spd_f64 *out,
                            alwan_f64 temperature_K,
                            alwan_f64 wavelength_min,
                            alwan_f64 wavelength_max,
                            size_t count,
                            alwan_ctx *ctx);
```

Generate a Planckian (blackbody) radiator SPD via Planck's law. `temperature_K` is typically 1000–25000 K; out-of-range returns `ALWAN_E_INVALID`.

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
int alwan_spd_resample_f64(alwan_spd_f64 *dst,
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
int alwan_xyz_from_spd_f64(alwan_xyz_f64 *xyz_out,
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
    ALWAN_OBSERVER_CIE_2012_2DEG = 2,        /* CIE 2012 2-degree (physiologically-based) */
    ALWAN_OBSERVER_CIE_2012_10DEG = 3,       /* CIE 2012 10-degree (physiologically-based) */
    ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG = 4, /* Stockman & Sharpe 2000 2-degree cone fundamentals */
    ALWAN_OBSERVER_CIE_2015_2DEG = 5,        /* CIE 2015 2-degree cone-fundamental-based */
    ALWAN_OBSERVER_CIE_2015_10DEG = 6,       /* CIE 2015 10-degree cone-fundamental-based */
    ALWAN_OBSERVER_WRIGHT_GUILD_1931 = 7     /* Wright & Guild 1931 2-degree RGB CMFs (historical) */
} alwan_observer_type;
```

**Integration methods:**
```c
typedef enum {
    ALWAN_INTEGRATE_TRAPEZOID = 0,  /* Trapezoidal rule (fast) */
    ALWAN_INTEGRATE_SIMPSON = 1     /* Simpson's rule (more accurate) */
} alwan_integrate_method;
```

> **Reference-matching note.** alwan integrates with the trapezoidal or **Simpson** rule. Some reference libraries (e.g. colour-science) compute the tristimulus integral as a plain **Riemann summation** of `CMF · SPD · Δλ`. To reproduce such reference values exactly, match the quadrature on the reference side (colour-science exposes `scipy.integrate.simpson`) and use linear interpolation / matched wavelength sampling — see the project gendata notes.

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

---

## Camera Sensitivities

### alwan_spd_camera_sensitivity

```c
int alwan_spd_camera_sensitivity_f64(alwan_spd_f64 *spd_r,
                                     alwan_spd_f64 *spd_g,
                                     alwan_spd_f64 *spd_b,
                                     alwan_camera_sensitivity camera,
                                     alwan_ctx *ctx);
```

Load measured camera R/G/B spectral sensitivity curves. The three output SPDs **must already be created** (`alwan_spd_create_f64`) with the desired wavelength range/count; the curves are resampled into them. Returns `ALWAN_E_INVALID` for an unsupported camera.

**Camera types:**
```c
typedef enum {
    ALWAN_CAMERA_NIKON_5100,        /* Nikon D5100 (NPL measured) */
    ALWAN_CAMERA_SIGMA_SDMERILL     /* Sigma SD Merrill (NPL measured) */
} alwan_camera_sensitivity;
```

### alwan_xyz_from_spd_camera

```c
int alwan_xyz_from_spd_camera_f64(alwan_xyz_f64 *xyz_out,
                                  alwan_spd_f64 const *spd,
                                  alwan_spd_f64 const *illuminant,
                                  alwan_camera_sensitivity camera,
                                  alwan_integrate_method method,
                                  alwan_ctx *ctx);
```

Like `alwan_xyz_from_spd`, but integrates against the camera's RGB sensitivities instead of a standard observer (`illuminant` may be `NULL` for an already-weighted SPD).

---

## SPD Shape Analysis

### alwan_spd_analyze_shape

```c
int alwan_spd_analyze_shape_f64(alwan_spd_shape_f64 *shape_out,
                                alwan_spd_f64 const *spd);
```

Compute descriptive statistics for an SPD. Pure analysis — no `ctx`.

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
int alwan_optimize_spectrum_for_xyz_f64(alwan_spd_f64 *spd_out,
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

CIE Special Metamerism Index (change in illuminant): the ΔE\*ab between a metameric sample/reference pair under the test illuminant. Returns a negative value on error. (See also [CCT & Light Quality](cct-light-quality.md) for CRI/TM-30/SSI.)

---

## Hero-Wavelength Spectral Sampling

For Monte-Carlo spectral rendering. Maps a uniform `[0,1]` sample to a wavelength over `[380, 780] nm` and evaluates analytic CMFs (Wyman 2013 fit).

```c
/* Single-sample: u in [0,1] -> lambda in [380,780] nm */
int  alwan_hero_wavelength_sample_f64(alwan_f64 *lambda_out, alwan_f64 u);

/* Wavelength -> XYZ via Wyman 2013 analytic CMF fit (no ctx) */
void alwan_hero_wavelength_to_xyz_f64(alwan_xyz_f64 *xyz_out, alwan_f64 lambda);

/* Stratified batch: generate `count` wavelengths from a single seed.
 * xyz_weights receives per-sample XYZ importance weights (may be NULL). */
int  alwan_hero_wavelength_batch_f64(alwan_f64 *lambda_out,
                                     alwan_xyz_f64 *xyz_weights,
                                     size_t count,
                                     alwan_f64 seed);
```

---

## RGB to Spectrum (Spectral Upsampling)

Recover a plausible reflectance SPD from an RGB triplet. `out_spd` is allocated internally by each call (fixed wavelength range/count per method).

### alwan_rgb_to_spectrum_smits1999

```c
int alwan_rgb_to_spectrum_smits1999_f64(alwan_spd_f64 *out_spd,
                                        alwan_rgb_f64 const *rgb,
                                        alwan_ctx *ctx);
```

Smits 1999 basis-spectra mixing. Input is sRGB, clamped to `[0,1]`. Output: **380–720 nm, 10 samples**. Fast; intended for spectral rendering.

### alwan_rgb_to_spectrum_mallett2019

```c
int alwan_rgb_to_spectrum_mallett2019_f64(alwan_spd_f64 *out_spd,
                                          alwan_rgb_f64 const *rgb,
                                          alwan_ctx *ctx);
```

Mallett & Yuksel 2019 spectral primary decomposition. Input is sRGB. Output: **380–780 nm, 81 samples at 5 nm**. Higher spectral fidelity than Smits.

### alwan_rgb_to_spectrum_jakob2019

```c
int alwan_rgb_to_spectrum_jakob2019_f64(alwan_spd_f64 *out_spd,
                                        alwan_jakob2019_gamut gamut,
                                        alwan_rgb_f64 const *rgb,
                                        alwan_ctx *ctx);
```

Jakob & Hanika 2019 polynomial coefficient model. Input RGB is in the selected `gamut`, clamped to `[0,1]`. Output: **360–780 nm, 85 samples at 5 nm**.

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

---

## Error Codes

Spectral functions return the `alwan_status` enum:

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — Invalid parameter (unsupported illuminant/camera/gamut, bad range)
- `ALWAN_E_NODATA` (-2) — Required spectral data not present
- `ALWAN_E_NOMEM` (-4) — Memory allocation failed

`alwan_metamerism_index_*` instead returns the index directly (negative on error).

---

## See Also

- [CCT & Light Quality](cct-light-quality.md) — CRI, TM-30, SSI, metamerism
- [Color Spaces](color-spaces.md) — XYZ conversions
- [Atmospheric Optics](atmosphere.md) — Rayleigh scattering
- [Precision and Limits](../precision-and-limits.md) — `_f32`/`_f64` and f64-facade behaviour
- [Data Management](../data-management.md) — embedded CMF, illuminant, and spectral LUT data
