# Color Difference API

Functions for calculating perceptual color differences (dE / delta E) plus
whiteness and yellowness indices.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `alwan_f32`) and `name_f64` (double precision, `alwan_f64`).
> `T = f32 | f64`. Which forms are compiled is gated by `ALWAN_WITH_F32` / `ALWAN_WITH_F64`
> (both by default; see [configuration](../configuration.md)).

---

## Overview

Color difference metrics quantify how different two colors appear to human vision.
The full metric set:

| Metric | Function | Input space |
|---|---|---|
| dE76 (CIE 1976) | `alwan_delta_e_76_{T}` | Lab |
| dE94 (CIE 1994) | `alwan_delta_e_94_{T}` | Lab |
| dE00 (CIEDE2000) | `alwan_delta_e_2000_{T}` | Lab |
| dE CMC(l:c) | `alwan_delta_e_cmc_{T}` | Lab |
| dE OK | `alwan_delta_e_ok_{T}` | Oklab |
| dE ITP (BT.2124) | `alwan_delta_e_itp_{T}` | ICtCp |
| dE HyAB | `alwan_delta_e_hyab_{T}` | Lab |
| dE DIN99 | `alwan_delta_e_din99_{T}` | DIN99 |
| dE CAM02 LCD / SCD / UCS | `alwan_delta_e_cam02_{lcd,scd,ucs}_{T}` | CIECAM02 UCS (Jab) |
| dE CAM16 LCD / SCD / UCS | `alwan_delta_e_cam16_{lcd,scd,ucs}_{T}` | CAM16 UCS (Jab) |
| dE ZCAM | `alwan_delta_e_zcam_{T}` | ZCAM UCS (Jzazbz) |

Whiteness / yellowness indices: ASTM E313 YI and WI, and CIE 2004 Whiteness
(see [Whiteness & Yellowness](#whiteness--yellowness)).

---

## Perceptual Thresholds

| dE Value | Perception |
|---|---|
| < 1.0 | Not perceptible |
| 1.0 - 2.0 | Perceptible through close observation |
| 2.0 - 10.0 | Perceptible at a glance |
| > 10.0 | Colors are clearly different |

---

## Single-Element Functions

### alwan_delta_e_76_{T}

```c
alwan_{T} alwan_delta_e_76_{T}(alwan_lab_{T} const *lab1, alwan_lab_{T} const *lab2);
```

Euclidean distance in Lab space. Simplest but least accurate.

---

### alwan_delta_e_94_{T}

```c
alwan_{T} alwan_delta_e_94_{T}(alwan_lab_{T} const *lab1, alwan_lab_{T} const *lab2);
```

CIE 1994 color difference with graphic-arts defaults (kL=1, K1=0.045, K2=0.015).

---

### alwan_delta_e_2000_{T}

```c
alwan_{T} alwan_delta_e_2000_{T}(alwan_lab_{T} const *lab1, alwan_lab_{T} const *lab2);
```

CIEDE2000 -- most perceptually uniform metric.

---

### alwan_delta_e_cmc_{T}

```c
typedef struct { alwan_{T} l; alwan_{T} c; } alwan_delta_e_cmc_params_{T};

void      alwan_delta_e_cmc_params_default_{T}(alwan_delta_e_cmc_params_{T} *p);
alwan_{T} alwan_delta_e_cmc_{T}(alwan_lab_{T} const *lab1, alwan_lab_{T} const *lab2,
                                alwan_delta_e_cmc_params_{T} const *params);
```

CMC l:c color difference. `alwan_delta_e_cmc_params_default_{T}` fills the acceptability
defaults (l=2, c=1); set l=1, c=1 for perceptibility.

---

### alwan_delta_e_itp_{T}

```c
typedef struct { alwan_{T} scalar_factor; } alwan_delta_e_itp_params_{T};

alwan_{T} alwan_delta_e_itp_{T}(alwan_ictcp_{T} const *ictcp1, alwan_ictcp_{T} const *ictcp2,
                                alwan_delta_e_itp_params_{T} const *params);
```

ITU-R BT.2124 ICtCp-based metric for HDR content. Use `scalar_factor = 720` for standard usage.

---

### alwan_delta_e_ok_{T}

```c
alwan_{T} alwan_delta_e_ok_{T}(alwan_oklab_{T} const *a, alwan_oklab_{T} const *b);
```

Euclidean distance in Oklab space (CSS Color Level 4 JND criterion). A modern, simple
metric with good perceptual uniformity.

---

### alwan_delta_e_hyab_{T}

```c
alwan_{T} alwan_delta_e_hyab_{T}(alwan_lab_{T} const *lab1, alwan_lab_{T} const *lab2);
```

HyAB (Hybrid Absolute) color difference. Uses an L1 norm for lightness and an L2 norm for
chroma/hue. Better correlation with perceived difference for large color differences.

---

### alwan_delta_e_din99_{T}

```c
alwan_{T} alwan_delta_e_din99_{T}(alwan_din99_{T} const *din99_1,
                                  alwan_din99_{T} const *din99_2);
```

Euclidean distance in DIN99 color space. The variant (DIN99 / 99b / 99c / 99d) is fixed
when the inputs are produced by the DIN99 conversion; this function takes the coordinates as-is.

---

### alwan_delta_e_cam02_lcd_{T} / scd / ucs

```c
alwan_{T} alwan_delta_e_cam02_lcd_{T}(alwan_cam_jab_{T} const *jab1, alwan_cam_jab_{T} const *jab2);
alwan_{T} alwan_delta_e_cam02_scd_{T}(alwan_cam_jab_{T} const *jab1, alwan_cam_jab_{T} const *jab2);
alwan_{T} alwan_delta_e_cam02_ucs_{T}(alwan_cam_jab_{T} const *jab1, alwan_cam_jab_{T} const *jab2);
```

CIECAM02-based metrics optimized for Large Color Differences (LCD), Small Color Differences (SCD),
or general-purpose Uniform Color Space (UCS, Luo et al. 2006: K_L=1.0, c1=0.007, c2=0.0228).
Input is Jab from the corresponding CIECAM02 UCS.

---

### alwan_delta_e_cam16_lcd_{T} / scd / ucs

```c
alwan_{T} alwan_delta_e_cam16_lcd_{T}(alwan_cam_jab_{T} const *jab1, alwan_cam_jab_{T} const *jab2);
alwan_{T} alwan_delta_e_cam16_scd_{T}(alwan_cam_jab_{T} const *jab1, alwan_cam_jab_{T} const *jab2);
alwan_{T} alwan_delta_e_cam16_ucs_{T}(alwan_cam_jab_{T} const *jab1, alwan_cam_jab_{T} const *jab2);
```

CAM16-based metrics. Same LCD/SCD/UCS variants as CIECAM02 (UCS per Li et al. 2017), using
CAM16 UCS coordinates.

---

### alwan_delta_e_zcam_{T}

```c
alwan_{T} alwan_delta_e_zcam_{T}(alwan_jzazbz_{T} const *jab1, alwan_jzazbz_{T} const *jab2);
```

ZCAM-based color difference: Euclidean distance in ZCAM UCS (Jzazbz) space.

---

## Batch Functions (`_batch`)

Compare arrays of colors efficiently. Strides follow the memcpy convention -- each
`*_stride` is in **bytes** and immediately follows the buffer it describes (typically
`3 * sizeof(alwan_{T})` for tightly packed Lab triplets). Batch variants are provided
for dE76, dE00, dE94 and dE CMC, and each ships **both** precisions
(`alwan_delta_e_76_f32_batch` / `_f64_batch`, etc.) -- so the "every `name_{T}`
exists in two forms" rule at the top of this page holds for the scalar **and** batch
metric set.

```c
int alwan_delta_e_76_{T}_batch(
    alwan_{T} *delta_e_out,
    alwan_{T} const *lab1_in, size_t in1_stride,
    alwan_{T} const *lab2_in, size_t in2_stride,
    size_t count);

int alwan_delta_e_2000_{T}_batch(
    alwan_{T} *delta_e_out,
    alwan_{T} const *lab1_in, size_t in1_stride,
    alwan_{T} const *lab2_in, size_t in2_stride,
    size_t count);

int alwan_delta_e_94_{T}_batch(
    alwan_{T} *delta_e_out,
    alwan_{T} const *lab1_in, size_t in1_stride,
    alwan_{T} const *lab2_in, size_t in2_stride,
    size_t count);

int alwan_delta_e_cmc_{T}_batch(
    alwan_{T} *delta_e_out,
    alwan_{T} const *lab1_in, size_t in1_stride,
    alwan_{T} const *lab2_in, size_t in2_stride,
    size_t count, alwan_{T} l, alwan_{T} c);
```

All batch functions return `ALWAN_OK` on success.

### Typed batch (`_batch_ex`)

`void*` buffers with per-buffer `alwan_pixel_format` (any U8/U16/F16/F32/F64 input);
the output is always `alwan_f64`. These are precision-agnostic (no `_{T}` suffix).

```c
int alwan_delta_e_76_batch_ex(alwan_f64 *delta_e_out,
    void const *lab1_in, size_t in1_stride,
    void const *lab2_in, size_t in2_stride,
    size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt);

int alwan_delta_e_2000_batch_ex(alwan_f64 *delta_e_out,
    void const *lab1_in, size_t in1_stride,
    void const *lab2_in, size_t in2_stride,
    size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt);

int alwan_delta_e_94_batch_ex(alwan_f64 *delta_e_out,
    void const *lab1_in, size_t in1_stride,
    void const *lab2_in, size_t in2_stride,
    size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt);

int alwan_delta_e_cmc_batch_ex(alwan_f64 *delta_e_out,
    void const *lab1_in, size_t in1_stride,
    void const *lab2_in, size_t in2_stride,
    size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt,
    alwan_f64 l, alwan_f64 c);
```

---

## Whiteness & Yellowness

### alwan_yellowness_astm_e313_{T} / alwan_whiteness_astm_e313_{T}

```c
typedef enum {
    ALWAN_ASTM_E313_C_2DEG    = 0,  /* Illuminant C,   CIE 1931 2 deg observer */
    ALWAN_ASTM_E313_D65_2DEG  = 1,  /* Illuminant D65, CIE 1931 2 deg observer */
    ALWAN_ASTM_E313_C_10DEG   = 2,  /* Illuminant C,   CIE 1964 10 deg observer */
    ALWAN_ASTM_E313_D65_10DEG = 3   /* Illuminant D65, CIE 1964 10 deg observer */
} alwan_astm_e313_illuminant;

alwan_{T} alwan_yellowness_astm_e313_{T}(alwan_xyz_{T} const *xyz,
                                         alwan_astm_e313_illuminant illuminant);
alwan_{T} alwan_whiteness_astm_e313_{T}(alwan_xyz_{T} const *xyz,
                                        alwan_astm_e313_illuminant illuminant);
```

ASTM E313 Yellowness Index (YI) and Whiteness Index (WI). `xyz` is CIE XYZ normalized to
Y=100 for a perfect white; `illuminant` selects the illuminant/observer pair.

### alwan_whiteness_cie2004_{T}

```c
alwan_{T} alwan_whiteness_cie2004_{T}(alwan_vec2_{T} const *xy, alwan_{T} Y,
                                      alwan_vec2_{T} const *xy_n);
```

CIE 2004 Whiteness (W). `xy` is the sample's CIE 1931 chromaticity, `Y` its luminance factor,
and `xy_n` the reference white chromaticity. Returns W only (the companion Tint value is not
returned).

---

## Usage Example

```c
alwan_lab_f64 lab1 = { 50.0, 10.0, 20.0 };
alwan_lab_f64 lab2 = { 52.0, 12.0, 18.0 };

double de76   = alwan_delta_e_76_f64(&lab1, &lab2);
double de2000 = alwan_delta_e_2000_f64(&lab1, &lab2);

alwan_delta_e_cmc_params_f64 cmc;
alwan_delta_e_cmc_params_default_f64(&cmc);   /* l=2, c=1 */
double decmc  = alwan_delta_e_cmc_f64(&lab1, &lab2, &cmc);

printf("dE76: %.2f  dE00: %.2f  dE CMC: %.2f\n", de76, de2000, decmc);
```

---

## Error Codes

Batch functions return [`alwan_status`](../api-conventions.md):

- `ALWAN_OK` (0) -- Success
- `ALWAN_E_INVALID` (-1) -- NULL pointer or invalid argument

Single-element functions return the dE value directly (no status code).

---

## See Also

- [Color Spaces](color-spaces.md) -- Lab / Oklab / ICtCp / DIN99 conversions
- [Color Appearance](color-appearance.md) -- CIECAM02 / CAM16 / ZCAM correlates feeding the CAM metrics
- [API Conventions](../api-conventions.md) -- parameter ordering and status codes
