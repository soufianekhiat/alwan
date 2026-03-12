# CCT & Light Quality API

Functions for correlated color temperature estimation, color rendering indices, and light source quality metrics.

---

## Overview

Light quality functions evaluate illumination characteristics:

- **CCT estimation** — Determine color temperature from chromaticity
- **CRI (Ra)** — CIE Color Rendering Index
- **CQS** — NIST Color Quality Scale
- **TM-30 / CIE 224** — Modern fidelity indices
- **SSI** — Spectral Similarity Index (SMPTE ST 2122)
- **Metamerism Index** — Color mismatch under illuminant change
- **Whiteness / Yellowness** — ASTM E313 and CIE 2004 indices

---

## CCT Estimation

### alwan_cct_mccamy_xy

```c
alwan_scalar alwan_cct_mccamy_xy(alwan_vec2 const *xy);
```

McCamy approximation. Fast, approximately 2% accuracy above 2800K.

### alwan_cct_robertson_xy

```c
alwan_scalar alwan_cct_robertson_xy(alwan_vec2 const *xy);
```

Robertson method. Accurate, iterative lookup against Planckian locus.

### alwan_cct_hernandez_xy

```c
alwan_scalar alwan_cct_hernandez_xy(alwan_vec2 const *xy);
```

Hernandez-Andres 1999 analytical formula. Valid range: 3000K-50000K.

### alwan_cct_kang_xy

```c
alwan_scalar alwan_cct_kang_xy(alwan_vec2 const *xy);
```

Kang 2002 method using Newton-Raphson. Valid range: 1667K-25000K.

### alwan_cct_to_xy_kang

```c
void alwan_cct_to_xy_kang(alwan_vec2 *xy_out, alwan_scalar cct);
```

Inverse: convert CCT to xy chromaticity. Valid range: 1667K-25000K.

### alwan_cct_duv_optimize

```c
int alwan_cct_duv_optimize(alwan_scalar *cct_out, alwan_scalar *duv_out,
                           alwan_vec2 const *xy);
```

Compute CCT and Duv (distance from Planckian locus) using iterative least-squares optimization. Accuracy: CCT <= 1K, Duv <= 0.0001.

**Example:**
```c
alwan_vec2 xy = {0.3127, 0.3290};  /* D65 chromaticity */

/* Quick estimate */
alwan_scalar cct_fast = alwan_cct_mccamy_xy(&xy);

/* Accurate CCT + Duv */
alwan_scalar cct, duv;
alwan_cct_duv_optimize(&cct, &duv, &xy);
printf("CCT: %.0fK, Duv: %.5f\n", cct, duv);
```

---

## Color Rendering Index (CRI)

### alwan_cri_ra

```c
alwan_scalar alwan_cri_ra(alwan_ctx *ctx, alwan_spd const *test_spd);
```

CIE Color Rendering Index Ra — average of 8 TCS (test color samples). Returns value [0, 100], or negative on error.

**Example:**
```c
alwan_spd led_spd;
alwan_spd_illuminant(&led_spd, ctx, ALWAN_ILLUMINANT_LED_B1);

alwan_scalar cri = alwan_cri_ra(ctx, &led_spd);
printf("CRI Ra = %.0f\n", cri);  /* e.g., 82 */

alwan_spd_destroy(ctx, &led_spd);
```

---

## Color Quality Scale (CQS)

### alwan_cqs_calculate

```c
alwan_scalar alwan_cqs_calculate(alwan_ctx *ctx, alwan_spd const *test_spd);
```

NIST Color Quality Scale using 15 saturated samples. Returns [0, 100].

---

## TM-30 / CIE 224 Fidelity Index

### alwan_tm30_rf

```c
alwan_scalar alwan_tm30_rf(alwan_ctx *ctx, alwan_spd const *test_spd);
```

IES TM-30 Fidelity Index (Rf) using 99 CES samples. Returns [0, 100].

### alwan_cie224_rf

```c
alwan_scalar alwan_cie224_rf(alwan_ctx *ctx, alwan_spd const *test_spd);
```

CIE 224:2017 Color Fidelity Index. Same algorithm as TM-30 per CIE 224:2017 standard. Returns [0, 100].

---

## Spectral Similarity Index (SSI)

### alwan_ssi_calculate

```c
alwan_scalar alwan_ssi_calculate(alwan_ctx *ctx,
                                 alwan_spd const *test_spd,
                                 alwan_spd const *reference_spd);
```

Academy/SMPTE ST 2122 Spectral Similarity Index. Measures spectral similarity between two light sources. Returns [0, 100] where 100 = perfect match.

---

## Metamerism Index

### alwan_metamerism_index

```c
alwan_scalar alwan_metamerism_index(alwan_ctx *ctx,
                                    alwan_spd const *sample_reflectance,
                                    alwan_spd const *reference_reflectance,
                                    alwan_spd const *reference_illuminant,
                                    alwan_spd const *test_illuminant,
                                    alwan_observer_type observer);
```

CIE Special Metamerism Index: quantifies color mismatch when samples that match under a reference illuminant are viewed under a test illuminant. Returns the metamerism index as ΔE*ab.

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
alwan_scalar alwan_yellowness_astm_e313(alwan_xyz const *xyz,
                                        alwan_astm_e313_illuminant illuminant);
```

ASTM E313 Yellowness Index. Input XYZ must be normalized to Y=100 for perfect white.

### alwan_whiteness_astm_e313

```c
alwan_scalar alwan_whiteness_astm_e313(alwan_xyz const *xyz,
                                       alwan_astm_e313_illuminant illuminant);
```

ASTM E313 Whiteness Index. Input XYZ must be normalized to Y=100 for perfect white.

### alwan_whiteness_cie2004

```c
alwan_scalar alwan_whiteness_cie2004(alwan_vec2 const *xy, alwan_scalar Y,
                                     alwan_vec2 const *xy_n);
```

CIE 2004 Whiteness Index. Returns whiteness (W) value. Tint (T) is computed internally but not returned by this function.

**Example:**
```c
alwan_xyz paper = {93.0, 95.0, 101.0};  /* Slightly blue-white */

alwan_scalar yi = alwan_yellowness_astm_e313(&paper, ALWAN_ASTM_E313_D65_2DEG);
alwan_scalar wi = alwan_whiteness_astm_e313(&paper, ALWAN_ASTM_E313_D65_2DEG);
printf("YI = %.1f, WI = %.1f\n", yi, wi);
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — NULL pointer or unsupported parameter
- `ALWAN_E_NODATA` (-2) — Required spectral data not loaded

---

## See Also

- [Spectral Operations](spectral.md) — SPD creation and integration
- [Chromatic Adaptation](chromatic-adaptation.md) — White point transforms
- [Color Difference](color-difference.md) — ΔE metrics
