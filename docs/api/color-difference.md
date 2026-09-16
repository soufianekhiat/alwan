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

### alwan_delta_e_hych_{T}

```c
alwan_{T} alwan_delta_e_hych_{T}(alwan_lab_{T} const *lab1, alwan_lab_{T} const *lab2, int textiles);
```

HyCH (Huang et al. 2015) takes the same hybrid shape as HyAB, absolute in lightness and
Euclidean across chroma and hue, but over CIEDE2000's terms rather than plain Lab: the
weighting functions S_L, S_C and S_H apply, while CIEDE2000's rotation term R_T does not.
`textiles` sets k_L to 2, as CIEDE2000 does for textile work; anything else leaves it at 1.

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

## Fitting and Agreement

Two things that are about colour differences without being one.

### alwan_power_function_huang2015_{T}

```c
alwan_status alwan_power_function_huang2015_{T}(alwan_{T} *out, alwan_{T} delta_e,
                                                alwan_huang2015_formula formula);
```

Huang et al. 2015 fitted a power function, dE' = a x dE^b, to each of twelve difference
formulas, so that the numbers a formula produces line up better with what observers
judged. The pair (a, b) is the published one for the formula named, from
`ALWAN_HUANG2015_CIE1976` through `ALWAN_HUANG2015_ULAB`. A formula outside that list is
`ALWAN_E_INVALID`, and so is a negative difference, which no formula produces. Zero stays
zero.

### alwan_index_stress_{T}

```c
alwan_status alwan_index_stress_{T}(alwan_{T} *stress_out, alwan_{T} const *delta_e,
                                    alwan_{T} const *delta_v, size_t count);
```

STRESS, the standardised residual sum of squares of García et al. 2007: how far a set of
computed differences sits from the visual differences it should track, once the scale
factor between the two sets is taken out. Zero is perfect agreement, and the result is a
fraction rather than a percentage. The two arrays are read in step, one pair per
judgement, and `count` must be at least 1.

A set with no scale factor between its two halves, where the computed and visual
differences have no overlap to fit, is `ALWAN_E_DIVZERO`; colour-science yields zero
there instead.

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

### More whiteness and yellowness indices

```c
alwan_status alwan_whiteness_berger1959_{T}(alwan_{T} *W_out, alwan_xyz_{T} const *xyz, alwan_xyz_{T} const *xyz_0);
alwan_status alwan_whiteness_taube1960_{T}(alwan_{T} *W_out, alwan_xyz_{T} const *xyz, alwan_xyz_{T} const *xyz_0);
alwan_status alwan_whiteness_stensby1968_{T}(alwan_{T} *W_out, alwan_lab_{T} const *lab);
alwan_status alwan_whiteness_ganz1979_{T}(alwan_{T} *W_out, alwan_{T} *T_out, alwan_vec2_{T} const *xy, alwan_{T} Y);
alwan_status alwan_yellowness_astm_d1925_{T}(alwan_{T} *YI_out, alwan_xyz_{T} const *xyz);
alwan_status alwan_yellowness_astm_e313_alternative_{T}(alwan_{T} *YI_out, alwan_xyz_{T} const *xyz);
```

| Index | Formula |
|---|---|
| Berger 1959 | `0.333 Y + 125 Z / Z_0 - 125 X / X_0`, against the illuminant's XYZ_0 |
| Taube 1960 | `400 Z / Z_0 - 3 Y` |
| Stensby 1968 | `L* - 3 b* + 3 a*` |
| Ganz 1979 | `W = Y - 1868.322 x - 3695.690 y + 1809.441`, and the tint `T = -1001.223 x + 748.366 y + 68.261` |
| ASTM D1925 | `100 (1.28 X - 1.06 Z) / Y` |
| ASTM E313 alternative | `100 (1 - 0.847 Z / Y)` |

XYZ in [0, 100]. Unlike the E313 and CIE 2004 functions above, these return an
`alwan_status`: `ALWAN_E_INVALID` for a NULL pointer or a divisor of zero, a Y of 0
in the yellowness indices included, where colour-science returns 0. They match
colour-science's `colour.colorimetry` functions exactly (suite 132).

---

## Lightness and Munsell Value

### alwan_lightness_{T} / alwan_luminance_from_lightness_{T}

```c
alwan_status alwan_lightness_{T}(alwan_{T} *L_out, alwan_{T} Y, alwan_lightness_method method,
                                 alwan_lightness_params_{T} const *params);
alwan_status alwan_luminance_from_lightness_{T}(alwan_{T} *Y_out, alwan_{T} L,
                                                alwan_lightness_method method,
                                                alwan_lightness_params_{T} const *params);
```

| Method | Y | Parameters (zero reads as) |
|---|---|---|
| `ALWAN_LIGHTNESS_CIE1976` | [0, 100] against `Y_n` | `Y_n` (100) |
| `ALWAN_LIGHTNESS_GLASSER1958` | [0, 100] | none |
| `ALWAN_LIGHTNESS_WYSZECKI1963` | [0, 100], meant for 1 to 98 | none |
| `ALWAN_LIGHTNESS_FAIRCHILD2010` | relative to diffuse white, 1, and above | `epsilon` (1.836) |
| `ALWAN_LIGHTNESS_FAIRCHILD2011_CIELAB`, `_IPT` | relative to diffuse white | `epsilon` (0.474) |
| `ALWAN_LIGHTNESS_ABEBE2017_MICHAELIS_MENTEN`, `_STEVENS` | cd/m2 against the adapting `Y_n` | `Y_n` (100); above 100 the second set of constants |

The Fairchild scales run past 100 for Y above diffuse white; Abebe 2017 returns
lightness near [0, 1]. The inverse returns the Y of a lightness. colour-science has no
inverse for Glasser 1958 and Wyszecki 1963, and alwan inverts them in closed form.

### alwan_munsell_value_{T} / alwan_luminance_from_munsell_value_{T}

```c
alwan_status alwan_munsell_value_{T}(alwan_{T} *V_out, alwan_{T} Y, alwan_munsell_value_method method);
alwan_status alwan_luminance_from_munsell_value_{T}(alwan_{T} *Y_out, alwan_{T} V,
                                                    alwan_munsell_luminance_method method);
```

Munsell value, 0 to 10, of Y in [0, 100] by ASTM D1535 (the default, 0), Priest 1920,
Munsell 1933, Moon 1943, Saunderson 1944, Ladd 1955 or McCamy 1987, and the Y of a
value by the ASTM D1535 or Newhall 1943 quintic. ASTM D1535 defines luminance as a
quintic in value; `alwan_munsell_value_{T}` inverts it exactly by Newton's method, where
colour-science interpolates a table of it at 0.001 steps. The two agree to 2.1e-7, and
alwan's value returns Y through the quintic to 4e-16.

Every method matches colour-science exactly in double precision, the inverses to 5e-16
(suite 132).

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
