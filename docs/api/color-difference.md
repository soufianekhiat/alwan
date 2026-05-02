# Color Difference API

Functions for calculating perceptual color differences using various dE (delta E) metrics.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

---

## Overview

Color difference metrics quantify how different two colors appear to human vision:

- **dE76 (CIE 1976)** -- Simple Euclidean distance in Lab
- **dE94 (CIE 1994)** -- Improved with weighting factors
- **dE00 (CIEDE2000)** -- Most accurate, industry standard
- **dE CMC** -- Textile industry standard
- **dE ITP** -- For HDR content (ICtCp-based)

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

CIE 1994 color difference with default weighting (graphic arts).

---

### alwan_delta_e_2000_{T}

```c
alwan_{T} alwan_delta_e_2000_{T}(alwan_lab_{T} const *lab1, alwan_lab_{T} const *lab2);
```

CIEDE2000 -- most accurate for perceptual color differences.

---

### alwan_delta_e_cmc_{T}

```c
alwan_{T} alwan_delta_e_cmc_{T}(alwan_lab_{T} const *lab1, alwan_lab_{T} const *lab2,
                                  alwan_{T} l, alwan_{T} c);
```

CMC l:c color difference. Use l=2, c=1 for acceptability; l=1, c=1 for perceptibility.

---

### alwan_delta_e_itp_{T}

```c
alwan_{T} alwan_delta_e_itp_{T}(alwan_ictcp_{T} const *itp1, alwan_ictcp_{T} const *itp2,
                                  alwan_{T} scalar_factor);
```

ICtCp-based metric for HDR content. Use scalar_factor=720 for standard usage.

---

### alwan_delta_e_ok_{T}

```c
alwan_{T} alwan_delta_e_ok_{T}(alwan_oklab_{T} const *a, alwan_oklab_{T} const *b);
```

Euclidean distance in Oklab space. A modern, simple metric with good perceptual uniformity.

---

### alwan_delta_e_hyab_{T}

```c
alwan_{T} alwan_delta_e_hyab_{T}(alwan_lab_{T} const *lab1, alwan_lab_{T} const *lab2);
```

HyAb (Hybrid Absolute) color difference. Uses L1 norm for lightness and L2 for chroma/hue.
Better correlation with perceived difference for large color differences.

---

### alwan_delta_e_din99_{T}

```c
alwan_{T} alwan_delta_e_din99_{T}(alwan_din99_{T} const *din99_1,
                                    alwan_din99_{T} const *din99_2);
```

Euclidean distance in DIN99 color space.

---

### alwan_delta_e_cam02_lcd_{T} / scd / ucs

```c
alwan_{T} alwan_delta_e_cam02_lcd_{T}(alwan_cam_jab_{T} const *jab1,
                                       alwan_cam_jab_{T} const *jab2);
alwan_{T} alwan_delta_e_cam02_scd_{T}(alwan_cam_jab_{T} const *jab1,
                                       alwan_cam_jab_{T} const *jab2);
alwan_{T} alwan_delta_e_cam02_ucs_{T}(alwan_cam_jab_{T} const *jab1,
                                       alwan_cam_jab_{T} const *jab2);
```

CIECAM02-based metrics optimized for Large Color Differences (LCD), Small Color Differences (SCD),
or Uniform Color Space (UCS). Input is Jab from CIECAM02 UCS.

---

### alwan_delta_e_cam16_lcd_{T} / scd / ucs

```c
alwan_{T} alwan_delta_e_cam16_lcd_{T}(alwan_cam_jab_{T} const *jab1,
                                       alwan_cam_jab_{T} const *jab2);
alwan_{T} alwan_delta_e_cam16_scd_{T}(alwan_cam_jab_{T} const *jab1,
                                       alwan_cam_jab_{T} const *jab2);
alwan_{T} alwan_delta_e_cam16_ucs_{T}(alwan_cam_jab_{T} const *jab1,
                                       alwan_cam_jab_{T} const *jab2);
```

CAM16-based metrics. Same LCD/SCD/UCS variants as CIECAM02 but using CAM16 UCS coordinates.

---

### alwan_delta_e_zcam_{T}

```c
alwan_{T} alwan_delta_e_zcam_{T}(alwan_jzazbz_{T} const *jab1,
                                   alwan_jzazbz_{T} const *jab2);
```

ZCAM-based color difference using Jzazbz coordinates.

---

## Bulk Functions (`_map_interleave`)

```c
int alwan_delta_e_76_{T}_map_interleave(
    alwan_{T} *delta_e_out,
    alwan_{T} const *lab1_in,
    alwan_{T} const *lab2_in,
    size_t count,
    size_t in1_stride,
    size_t in2_stride);

int alwan_delta_e_2000_{T}_map_interleave(
    alwan_{T} *delta_e_out,
    alwan_{T} const *lab1_in,
    alwan_{T} const *lab2_in,
    size_t count,
    size_t in1_stride,
    size_t in2_stride);

int alwan_delta_e_94_{T}_map_interleave(alwan_{T} *delta_e_out,
    alwan_{T} const *lab1_in, alwan_{T} const *lab2_in,
    size_t count, size_t in1_stride, size_t in2_stride);

int alwan_delta_e_cmc_{T}_map_interleave(alwan_{T} *delta_e_out,
    alwan_{T} const *lab1_in, alwan_{T} const *lab2_in,
    alwan_{T} l, alwan_{T} c,
    size_t count, size_t in1_stride, size_t in2_stride);
```

All map_interleave functions return `ALWAN_OK` on success.

---

## Usage Example

```c
alwan_lab_{T} lab1 = {50.0, 10.0, 20.0};
alwan_lab_{T} lab2 = {52.0, 12.0, 18.0};

double de76   = alwan_delta_e_76_{T}(&lab1, &lab2);
double de2000 = alwan_delta_e_2000_{T}(&lab1, &lab2);

printf("dE76: %.2f, dE00: %.2f\n", de76, de2000);
```

---

## Error Codes

- `ALWAN_OK` (0) -- Success
- `ALWAN_E_INVALID` (-1) -- NULL pointer

---

## See Also

- [Color Spaces](color-spaces.md) -- Lab conversions
- [Color Appearance](color-appearance.md) -- CAM-based metrics
