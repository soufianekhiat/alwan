# Spectral Operations API

Functions for spectral power distribution (SPD) lifecycle, integration, resampling, and spectral rendering.

---

## Overview

Spectral operations convert between spectral power distributions (SPDs) and tristimulus values (XYZ) using color matching functions (CMFs).

---

## SPD Data Structure

```c
typedef struct {
    alwan_scalar *values;        /* SPD values (power/reflectance/transmittance) */
    alwan_scalar wavelength_min; /* Starting wavelength (nm) */
    alwan_scalar wavelength_max; /* Ending wavelength (nm) */
    size_t count;                /* Number of samples */
} alwan_spd;
```

---

## SPD Lifecycle

### alwan_spd_create

```c
int alwan_spd_create(alwan_spd *out, alwan_ctx *ctx,
                     alwan_scalar wavelength_min,
                     alwan_scalar wavelength_max, size_t count);
```

Allocate an empty SPD with `count` samples spanning the given wavelength range. Values are zero-initialized.

### alwan_spd_destroy

```c
void alwan_spd_destroy(alwan_ctx *ctx, alwan_spd *spd);
```

Free memory allocated by `alwan_spd_create` or other SPD constructors.

**Example:**
```c
alwan_spd spd;
alwan_spd_create(&spd, ctx, 380.0, 780.0, 81);  /* 5nm steps */
/* ... use spd ... */
alwan_spd_destroy(ctx, &spd);
```

---

## SPD Constructors

### alwan_spd_illuminant

```c
int alwan_spd_illuminant(alwan_spd *out, alwan_ctx *ctx,
                         alwan_illuminant ill);
```

Load a standard illuminant SPD by enum.

### alwan_spd_blackbody

```c
int alwan_spd_blackbody(alwan_spd *out, alwan_ctx *ctx,
                        alwan_scalar temperature_K,
                        alwan_scalar wavelength_min,
                        alwan_scalar wavelength_max, size_t count);
```

Generate a Planckian (blackbody) radiator SPD at the given temperature.

**Example:**
```c
alwan_spd blackbody;
alwan_spd_blackbody(&blackbody, ctx, 5500.0, 380.0, 780.0, 81);
/* 5500K blackbody, visible range, 5nm steps */
alwan_spd_destroy(ctx, &blackbody);
```

---

## SPD Resampling

### alwan_spd_resample

```c
int alwan_spd_resample(alwan_spd *dst, alwan_ctx *ctx,
                       alwan_spd const *src,
                       alwan_scalar wavelength_min,
                       alwan_scalar wavelength_max, size_t count,
                       alwan_resample_method method,
                       alwan_extrapolate_mode extrapolate);
```

Resample an SPD to a different wavelength range and step size.

**Resample methods:**
```c
typedef enum {
    ALWAN_RESAMPLE_LINEAR = 0,      /* Linear interpolation */
    ALWAN_RESAMPLE_CATMULL_ROM = 1  /* Catmull-Rom spline (smoother) */
} alwan_resample_method;
```

**Extrapolation modes:**
```c
typedef enum {
    ALWAN_EXTRAPOLATE_ZERO = 0,     /* Zero outside range */
    ALWAN_EXTRAPOLATE_CONSTANT = 1, /* Use boundary value */
    ALWAN_EXTRAPOLATE_LINEAR = 2    /* Linear extrapolation */
} alwan_extrapolate_mode;
```

---

## SPD to XYZ Integration

### alwan_xyz_from_spd

```c
int alwan_xyz_from_spd(alwan_xyz *xyz_out, alwan_ctx *ctx,
                       alwan_spd const *spd,
                       alwan_spd const *illuminant,
                       alwan_observer_type observer,
                       alwan_integrate_method method,
                       alwan_scalar bandpass_nm);
```

Convert SPD to XYZ tristimulus values via integration with color matching functions.

**Observer types:**
```c
typedef enum {
    ALWAN_OBSERVER_CIE_1931_2DEG = 0,  /* CIE 1931 2-degree */
    ALWAN_OBSERVER_CIE_1964_10DEG,     /* CIE 1964 10-degree */
    ALWAN_OBSERVER_CIE_2012_2DEG,      /* CIE 2012 2-degree */
    ALWAN_OBSERVER_CIE_2012_10DEG,     /* CIE 2012 10-degree */
    ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG,
    ALWAN_OBSERVER_CIE_2015_2DEG,
    ALWAN_OBSERVER_CIE_2015_10DEG,
    ALWAN_OBSERVER_WRIGHT_GUILD_1931
} alwan_observer_type;
```

**Integration methods:**
```c
typedef enum {
    ALWAN_INTEGRATE_TRAPEZOID = 0,
    ALWAN_INTEGRATE_SIMPSON = 1
} alwan_integrate_method;
```

**Example:**
```c
alwan_spd d65;
alwan_spd_illuminant(&d65, ctx, ALWAN_ILLUMINANT_D65);

alwan_xyz white;
alwan_xyz_from_spd(&white, ctx, &d65, NULL,
                   ALWAN_OBSERVER_CIE_1931_2DEG,
                   ALWAN_INTEGRATE_SIMPSON, 5.0);

alwan_spd_destroy(ctx, &d65);
```

---

## Camera Sensitivity

### alwan_spd_camera_sensitivity

```c
int alwan_spd_camera_sensitivity(alwan_spd *spd_r, alwan_spd *spd_g,
                                 alwan_spd *spd_b, alwan_ctx *ctx,
                                 alwan_camera_sensitivity camera);
```

Load camera RGB spectral sensitivity curves.

**Camera types:**
```c
typedef enum {
    ALWAN_CAMERA_NIKON_5100,        /* Nikon D5100 (NPL measured) */
    ALWAN_CAMERA_SIGMA_SDMERILL     /* Sigma SD Merill (NPL measured) */
} alwan_camera_sensitivity;
```

### alwan_xyz_from_spd_camera

```c
int alwan_xyz_from_spd_camera(alwan_xyz *xyz_out, alwan_ctx *ctx,
                              alwan_spd const *spd,
                              alwan_spd const *illuminant,
                              alwan_camera_sensitivity camera,
                              alwan_integrate_method method);
```

Integrate SPD using camera sensitivity functions instead of standard observer CMFs.

---

## SPD Shape Analysis

### alwan_spd_analyze_shape

```c
int alwan_spd_analyze_shape(alwan_spd_shape *shape_out,
                            alwan_spd const *spd);
```

Analyze SPD characteristics.

```c
typedef struct {
    alwan_scalar peak_wavelength;  /* Peak wavelength (nm) */
    alwan_scalar peak_value;       /* Peak power value */
    alwan_scalar fwhm;             /* Full width at half maximum (nm) */
    alwan_scalar centroid;         /* Weighted mean wavelength (nm) */
    alwan_scalar bandwidth;        /* Total wavelength range (nm) */
} alwan_spd_shape;
```

---

## RGB to Spectrum (Spectral Upsampling)

### alwan_rgb_to_spectrum_smits1999

```c
int alwan_rgb_to_spectrum_smits1999(alwan_spd *out_spd, alwan_ctx *ctx,
                                    alwan_rgb const *rgb);
```

Smits 1999 method. Fast, used for spectral rendering.

### alwan_rgb_to_spectrum_mallett2019

```c
int alwan_rgb_to_spectrum_mallett2019(alwan_spd *out_spd, alwan_ctx *ctx,
                                     alwan_rgb const *rgb);
```

Mallett & Yuksel 2019 method. Improved spectral fidelity.

### alwan_rgb_to_spectrum_jakob2019

```c
int alwan_rgb_to_spectrum_jakob2019(alwan_spd *out_spd, alwan_ctx *ctx,
                                    alwan_jakob2019_gamut gamut,
                                    alwan_rgb const *rgb);
```

Jakob et al. 2019 method with gamut-specific coefficients.

**Gamut types:**
```c
typedef enum {
    ALWAN_JAKOB2019_SRGB,
    ALWAN_JAKOB2019_PROPHOTO_RGB,
    ALWAN_JAKOB2019_ACES2065_1,
    ALWAN_JAKOB2019_REC2020,
    ALWAN_JAKOB2019_ERGB,
    ALWAN_JAKOB2019_XYZ
} alwan_jakob2019_gamut;
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — Invalid parameter
- `ALWAN_E_NODATA` (-2) — Spectral data not loaded
- `ALWAN_E_NOMEM` (-4) — Memory allocation failed

---

## See Also

- [CCT & Light Quality](cct-light-quality.md) — CRI, SSI, metamerism
- [Color Spaces](color-spaces.md) — XYZ conversions
- [Atmospheric Optics](atmosphere.md) — Rayleigh scattering
- [Data Management](../data-management.md) — CMF and illuminant data
