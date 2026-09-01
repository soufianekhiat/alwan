# CCT & Light Quality API

Functions for correlated color temperature estimation, color rendering indices, and light source quality metrics.

---

## Overview

Light quality functions evaluate illumination characteristics:

- **CCT estimation**: Determine color temperature from chromaticity
- **CRI (Ra)**: CIE Color Rendering Index
- **CQS**: NIST Color Quality Scale
- **TM-30 / CIE 224**: Modern fidelity indices
- **SSI**: Spectral Similarity Index (Academy / SMPTE ST 2122)
- **Metamerism Index**: Color mismatch under illuminant change
- **Whiteness / Yellowness**: ASTM E313 and CIE 2004 indices

### Precision suffixes

Every function in this module is generated for both precisions. `{T}` below stands
for either `f32` (float, `alwan_f32`) or `f64` (double, `alwan_f64`); the matching
value type pairs follow (`alwan_vec2_{T}`, `alwan_xyz_{T}`, `alwan_spd_{T}`). Which
precisions are compiled is gated by `ALWAN_WITH_F32` / `ALWAN_WITH_F64` (both by
default; restrict with `ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64`). Calling a
precision that was excluded from the build fails at link time. There is no unsuffixed
alias; pick `_f32` or `_f64` at the call site.

> **f64-internal facades (by design).** `alwan_cct_kang_xy_f32` (a Newton-Raphson
> inverse with sub-f32-epsilon tolerances) and the spectral quality metrics
> `alwan_cri_ra_f32` / `alwan_cqs_calculate_f32` / `alwan_tm30_rf_f32` /
> `alwan_cie224_rf_f32` / `alwan_ssi_calculate_f32` / `alwan_metamerism_index_f32`
> (wavelength integration over f64 CMF tables) run the algorithm in `double` and
> narrow the result. This is a design choice rather than a missing native-f32
> path: the iterative/integration core needs f64 precision and repeatability. They stay
> callable even in an `ALWAN_BUILD_ONLY_F32` build (gated by `ALWAN_WITH_F64_FACADE`,
> always `1`). The other CCT estimators (McCamy / Robertson / Hernandez-Andres) are
> native f32. See [configuration.md](../configuration.md).

### Argument convention

These functions follow the v2.0 parameter convention: outputs come first, and the
optional `alwan_ctx *ctx` is always the **last** parameter (the pure `xy`-based CCT
helpers take no context at all, since they need no embedded data).

---

## CCT Estimation

### alwan_cct_mccamy_xy

```c
alwan_f32 alwan_cct_mccamy_xy_f32(alwan_vec2_f32 const *xy);
alwan_f64 alwan_cct_mccamy_xy_f64(alwan_vec2_f64 const *xy);
```

McCamy approximation. Fast, ~2% accuracy above 2800K. No context required.

### alwan_cct_robertson_xy

```c
alwan_f32 alwan_cct_robertson_xy_f32(alwan_vec2_f32 const *xy);
alwan_f64 alwan_cct_robertson_xy_f64(alwan_vec2_f64 const *xy);
```

Robertson method: accurate, iterative lookup against the Planckian locus. Returns CCT
in Kelvin, or a negative value on error.

### alwan_cct_hernandez_xy

```c
alwan_f32 alwan_cct_hernandez_xy_f32(alwan_vec2_f32 const *xy);
alwan_f64 alwan_cct_hernandez_xy_f64(alwan_vec2_f64 const *xy);
```

Hernandez-Andres et al. 1999 analytical formula. Valid range: 3000K-50000K.

### alwan_cct_kang_xy

```c
alwan_f32 alwan_cct_kang_xy_f32(alwan_vec2_f32 const *xy);
alwan_f64 alwan_cct_kang_xy_f64(alwan_vec2_f64 const *xy);
```

Kang et al. 2002 method (inverse, uses Newton-Raphson). Valid range: 1667K-25000K.

### alwan_cct_to_xy_kang

```c
void alwan_cct_to_xy_kang_f32(alwan_vec2_f32 *xy_out, alwan_f32 cct);
void alwan_cct_to_xy_kang_f64(alwan_vec2_f64 *xy_out, alwan_f64 cct);
```

Forward transform: convert CCT to xy chromaticity (Kang et al. 2002). Valid range:
1667K-25000K.

### alwan_cct_duv_optimize

```c
int alwan_cct_duv_optimize_f32(alwan_f32 *cct_out, alwan_f32 *duv_out,
                               alwan_vec2_f32 const *xy);
int alwan_cct_duv_optimize_f64(alwan_f64 *cct_out, alwan_f64 *duv_out,
                               alwan_vec2_f64 const *xy);
```

Compute CCT and Duv (distance from the Planckian locus) using iterative least-squares
optimization. `duv_out` may be `NULL` if only CCT is needed. Returns `ALWAN_OK` on
success, `ALWAN_E_INVALID` if `xy` is invalid. Accuracy: CCT <= 1K, Duv <= 0.0001.

**Example:**
```c
alwan_vec2_f64 xy = {{0.3127, 0.3290}};  /* D65 chromaticity */

/* Quick estimate */
alwan_f64 cct_fast = alwan_cct_mccamy_xy_f64(&xy);

/* Accurate CCT + Duv */
alwan_f64 cct, duv;
alwan_cct_duv_optimize_f64(&cct, &duv, &xy);
printf("CCT: %.0fK, Duv: %.5f\n", cct, duv);
```

---

## Color Rendering Index (CRI)

### alwan_cri_ra

```c
alwan_f32 alwan_cri_ra_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);
alwan_f64 alwan_cri_ra_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
```

CIE Color Rendering Index Ra: average of 8 TCS (test color samples). Returns a value
in [0, 100], or negative on error.

**Example:**
```c
alwan_spd_f64 led_spd;
alwan_spd_illuminant_f64(&led_spd, ALWAN_ILLUMINANT_LED_B1, ctx);

alwan_f64 cri = alwan_cri_ra_f64(&led_spd, ctx);
printf("CRI Ra = %.0f\n", cri);  /* e.g., 82 */

alwan_spd_destroy_f64(&led_spd, ctx);
```

---

## Color Quality Scale (CQS)

### alwan_cqs_calculate

```c
alwan_f32 alwan_cqs_calculate_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);
alwan_f64 alwan_cqs_calculate_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
```

NIST Color Quality Scale using 15 saturated samples. Returns [0, 100], or negative on
error.

---

## TM-30 / CIE 224 Fidelity Index

### alwan_tm30_rf

```c
alwan_f32 alwan_tm30_rf_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);
alwan_f64 alwan_tm30_rf_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
```

IES TM-30 Fidelity Index (Rf) using 99 CES samples. Returns [0, 100], or negative on
error.

### alwan_cie224_rf

```c
alwan_f32 alwan_cie224_rf_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);
alwan_f64 alwan_cie224_rf_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
```

CIE 224:2017 Color Fidelity Index. Same algorithm as TM-30, per the CIE 224:2017
standard. Returns [0, 100], or negative on error.

---

## Spectral Similarity Index (SSI)

### alwan_ssi_calculate

```c
alwan_f32 alwan_ssi_calculate_f32(alwan_spd_f32 const *test_spd,
                                  alwan_spd_f32 const *reference_spd,
                                  alwan_ctx *ctx);
alwan_f64 alwan_ssi_calculate_f64(alwan_spd_f64 const *test_spd,
                                  alwan_spd_f64 const *reference_spd,
                                  alwan_ctx *ctx);
```

Academy / SMPTE ST 2122 Spectral Similarity Index. Measures spectral similarity
between two light sources. Returns [0, 100] where 100 = perfect match, or negative on
error.

---

## Metamerism Index

### alwan_metamerism_index

```c
alwan_f32 alwan_metamerism_index_f32(alwan_spd_f32 const *sample_reflectance,
                                     alwan_spd_f32 const *reference_reflectance,
                                     alwan_spd_f32 const *reference_illuminant,
                                     alwan_spd_f32 const *test_illuminant,
                                     alwan_observer_type observer,
                                     alwan_ctx *ctx);
alwan_f64 alwan_metamerism_index_f64(alwan_spd_f64 const *sample_reflectance,
                                     alwan_spd_f64 const *reference_reflectance,
                                     alwan_spd_f64 const *reference_illuminant,
                                     alwan_spd_f64 const *test_illuminant,
                                     alwan_observer_type observer,
                                     alwan_ctx *ctx);
```

CIE Special Metamerism Index (change in illuminant): quantifies color mismatch when
samples that match under a reference illuminant are viewed under a test illuminant.
Returns the metamerism index as DeltaE*ab under the test illuminant, or negative on error.

---

## Whiteness & Yellowness Indices

### ASTM E313 Illuminant/Observer Pairs

```c
typedef enum {
    ALWAN_ASTM_E313_C_2DEG = 0,    /* Illuminant C, 2-degree observer */
    ALWAN_ASTM_E313_D65_2DEG = 1,  /* Illuminant D65, 2-degree observer */
    ALWAN_ASTM_E313_C_10DEG = 2,   /* Illuminant C, 10-degree observer */
    ALWAN_ASTM_E313_D65_10DEG = 3  /* Illuminant D65, 10-degree observer */
} alwan_astm_e313_illuminant;
```

### alwan_yellowness_astm_e313

```c
alwan_f32 alwan_yellowness_astm_e313_f32(alwan_xyz_f32 const *xyz,
                                         alwan_astm_e313_illuminant illuminant);
alwan_f64 alwan_yellowness_astm_e313_f64(alwan_xyz_f64 const *xyz,
                                         alwan_astm_e313_illuminant illuminant);
```

ASTM E313 Yellowness Index. Input XYZ must be normalized to Y=100 for perfect white.

### alwan_whiteness_astm_e313

```c
alwan_f32 alwan_whiteness_astm_e313_f32(alwan_xyz_f32 const *xyz,
                                        alwan_astm_e313_illuminant illuminant);
alwan_f64 alwan_whiteness_astm_e313_f64(alwan_xyz_f64 const *xyz,
                                        alwan_astm_e313_illuminant illuminant);
```

ASTM E313 Whiteness Index. Input XYZ must be normalized to Y=100 for perfect white.

### alwan_whiteness_cie2004

```c
alwan_f32 alwan_whiteness_cie2004_f32(alwan_vec2_f32 const *xy, alwan_f32 Y,
                                      alwan_vec2_f32 const *xy_n);
alwan_f64 alwan_whiteness_cie2004_f64(alwan_vec2_f64 const *xy, alwan_f64 Y,
                                      alwan_vec2_f64 const *xy_n);
```

CIE 2004 Whiteness Index. Returns the whiteness (W) value. Tint (T) is computed
internally but not returned by this function.

**Example:**
```c
alwan_xyz_f64 paper = {93.0, 95.0, 101.0};  /* Slightly blue-white */

alwan_f64 yi = alwan_yellowness_astm_e313_f64(&paper, ALWAN_ASTM_E313_D65_2DEG);
alwan_f64 wi = alwan_whiteness_astm_e313_f64(&paper, ALWAN_ASTM_E313_D65_2DEG);
printf("YI = %.1f, WI = %.1f\n", yi, wi);
```

---

## Error Codes

The CCT/Duv and other `int`-returning helpers use the `alwan_status` enum; the metric
functions return a score directly and signal failure with a negative value.

- `ALWAN_OK` (0): Success
- `ALWAN_E_INVALID` (-1): NULL pointer or unsupported parameter
- `ALWAN_E_NODATA` (-2): Required spectral data not loaded

---

## See Also

- [Spectral Operations](spectral.md): SPD creation and integration
- [Chromatic Adaptation](chromatic-adaptation.md): White point transforms
- [Color Difference](color-difference.md): DeltaE metrics
