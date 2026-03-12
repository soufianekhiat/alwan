# ACES Pipeline API

Functions for the Academy Color Encoding System (ACES) 1.x and 2.0 rendering pipelines.

---

## Overview

Alwan provides a complete ACES implementation covering:

- **ACES 1.x** — RRT + ODT output transforms with 12 display presets
- **ACES 2.0** — New perceptual gamut mapping with 13 display presets
- **Fixed Functions** — RRT components (RedMod, Glow, DarkToDim)
- **Gamut Compression** — ACES 1.3 Reference Gamut Compression
- **LMTs** — Look Modification Transforms (parametric CDL + legacy)

**Color spaces used:**
- **AP0 (ACES2065-1)** — Scene-referred linear, full spectral gamut
- **AP1 (ACEScg)** — Working space for VFX compositing
- **JMh** — ACES 2.0 perceptual color coordinates

---

## ACES 1.x Output Transforms

### Output Presets

```c
typedef enum {
    /* SDR Displays */
    ALWAN_ACES1_OUT_REC709_100NIT,        /* Rec.709, 100 nits, BT.1886 */
    ALWAN_ACES1_OUT_SRGB_100NIT,          /* sRGB, 100 nits, sRGB EOTF */
    ALWAN_ACES1_OUT_SRGB_D60_100NIT,      /* sRGB (D60 sim), 100 nits */

    /* P3 Displays */
    ALWAN_ACES1_OUT_P3DCI_48NIT,          /* P3-DCI, 48 nits, Gamma 2.6 */
    ALWAN_ACES1_OUT_P3D60_48NIT,          /* P3-D60, 48 nits, Gamma 2.6 */
    ALWAN_ACES1_OUT_P3D65_48NIT,          /* P3-D65, 48 nits, Gamma 2.6 */
    ALWAN_ACES1_OUT_P3D65_100NIT,         /* P3-D65 (Display P3), 100 nits */

    /* Rec.2020 / HDR */
    ALWAN_ACES1_OUT_REC2020_100NIT,       /* Rec.2020, 100 nits, BT.1886 */
    ALWAN_ACES1_OUT_REC2020_1000NIT_PQ,   /* Rec.2020, 1000 nits, PQ */
    ALWAN_ACES1_OUT_REC2020_2000NIT_PQ,   /* Rec.2020, 2000 nits, PQ */
    ALWAN_ACES1_OUT_REC2020_4000NIT_PQ,   /* Rec.2020, 4000 nits, PQ */

    /* Cinema */
    ALWAN_ACES1_OUT_DCDM_48NIT            /* DCDM X'Y'Z', 48 nits */
} alwan_aces1_output;
```

### alwan_aces1_output_transform

```c
int alwan_aces1_output_transform(alwan_rgb *rgb_out,
                                 alwan_rgb const *rgb_in,
                                 alwan_aces1_output output);
```

Complete ACES 1.3 rendering pipeline (RRT + ODT). Input: ACES2065-1 (AP0 linear). Output: display-encoded RGB.

### alwan_aces1_output_transform_inv

```c
int alwan_aces1_output_transform_inv(alwan_rgb *rgb_out,
                                     alwan_rgb const *rgb_in,
                                     alwan_aces1_output output);
```

Inverse of ACES 1.x output transform for round-trip workflows.

**Example:**
```c
alwan_rgb aces_pixel = {0.18, 0.18, 0.18};  /* 18% gray in AP0 */
alwan_rgb display;

alwan_aces1_output_transform(&display, &aces_pixel,
                             ALWAN_ACES1_OUT_SRGB_100NIT);
```

---

## ACES 2.0 Output Transforms

### Output Presets

```c
typedef enum {
    /* SDR */
    ALWAN_ACES2_OUT_REC709_100NIT_BT1886,    /* Rec.709, BT.1886 EOTF */
    ALWAN_ACES2_OUT_SRGB_100NIT,              /* sRGB, sRGB EOTF */
    ALWAN_ACES2_OUT_P3D65_100NIT_SRGB,        /* Display P3, sRGB piecewise */
    ALWAN_ACES2_OUT_P3D65_100NIT_G22,         /* Display P3, Gamma 2.2 */

    /* HDR (PQ) */
    ALWAN_ACES2_OUT_P3D65_1000NIT_PQ,         /* Display P3, 1000 nits */
    ALWAN_ACES2_OUT_REC2100_500NIT_PQ,        /* Rec.2100, 500 nits */
    ALWAN_ACES2_OUT_REC2100_1000NIT_PQ,       /* Rec.2100, 1000 nits */
    ALWAN_ACES2_OUT_REC2100_2000NIT_PQ,       /* Rec.2100, 2000 nits */
    ALWAN_ACES2_OUT_REC2100_4000NIT_PQ,       /* Rec.2100, 4000 nits */

    /* HDR (HLG) */
    ALWAN_ACES2_OUT_REC2100_1000NIT_HLG,      /* Rec.2100, HLG */

    /* Cinema */
    ALWAN_ACES2_OUT_DCDM_48NIT,               /* DCDM X'Y'Z' */
    ALWAN_ACES2_OUT_P3DCI_48NIT               /* P3-DCI, Gamma 2.6 */
} alwan_aces2_output;
```

### alwan_aces2_output_transform

```c
int alwan_aces2_output_transform(alwan_rgb *rgb_out,
                                 alwan_rgb const *rgb_in,
                                 alwan_aces2_output output);
```

Complete ACES 2.0 rendering pipeline. Input: ACEScg (AP1 linear). Output: display-encoded RGB.

Pipeline stages: AP1 -> JMh -> Tonescale + Chroma compress -> Gamut compress -> RGB -> Chromatic adaptation -> Display EOTF.

### alwan_aces2_output_transform_inv

```c
int alwan_aces2_output_transform_inv(alwan_rgb *rgb_out,
                                     alwan_rgb const *rgb_in,
                                     alwan_aces2_output output);
```

### alwan_aces2_output_transform_custom

```c
int alwan_aces2_output_transform_custom(alwan_rgb *rgb_out,
                                        alwan_rgb const *rgb_in,
                                        alwan_scalar peak_luminance,
                                        alwan_aces_primaries const *limit_primaries,
                                        alwan_transfer_function eotf);
```

Custom output transform with user-specified peak luminance, limiting primaries, and display EOTF.

---

## ACES 2.0 Components

### Tonescale Compression

```c
int alwan_aces_tonescale_compress20(alwan_rgb *rgb_out,
                                    alwan_rgb const *rgb_in,
                                    alwan_scalar peak_luminance);
```

### JMh Color Appearance Encoding

```c
/* Primaries descriptor for ACES 2.0 */
typedef struct {
    alwan_scalar red_x, red_y;
    alwan_scalar green_x, green_y;
    alwan_scalar blue_x, blue_y;
    alwan_scalar white_x, white_y;
} alwan_aces_primaries;

/* Initialize with AP1 defaults */
void alwan_aces_primaries_ap1_default(alwan_aces_primaries *primaries);

/* Convert to/from JMh appearance coordinates */
int alwan_aces_rgb_to_jmh20(alwan_vec3 *jmh_out,
                            alwan_rgb const *rgb_in,
                            alwan_aces_primaries const *primaries);

int alwan_aces_jmh_to_rgb20(alwan_rgb *rgb_out,
                            alwan_vec3 const *jmh_in,
                            alwan_aces_primaries const *primaries);
```

### Gamut Compression (ACES 2.0)

```c
int alwan_aces_gamut_compress20(alwan_vec3 *jmh_out,
                                alwan_vec3 const *jmh_in,
                                alwan_scalar peak_luminance,
                                alwan_aces_primaries const *limit_primaries);

int alwan_aces_gamut_compress20_inv(alwan_vec3 *jmh_out,
                                    alwan_vec3 const *jmh_in,
                                    alwan_scalar peak_luminance,
                                    alwan_aces_primaries const *limit_primaries);
```

---

## ACES Fixed Functions (RRT Components)

### RedMod — Red channel desaturation

```c
int alwan_aces_redmod03(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
int alwan_aces_redmod10(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
int alwan_aces_redmod03_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
int alwan_aces_redmod10_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
```

### Glow — Flare/glow compensation

```c
int alwan_aces_glow03(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
int alwan_aces_glow10(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
int alwan_aces_glow03_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
int alwan_aces_glow10_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
```

### DarkToDim — Surround compensation

```c
int alwan_aces_dark_to_dim10(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
```

---

## Gamut Compression (ACES 1.3)

### alwan_aces_gamut_comp13

```c
typedef struct {
    alwan_scalar lim_cyan, lim_magenta, lim_yellow;     /* Compression limits */
    alwan_scalar thr_cyan, thr_magenta, thr_yellow;     /* Thresholds */
    alwan_scalar power;                                  /* Compression power */
} alwan_aces_gamut_comp13_params;

void alwan_aces_gamut_comp13_params_default(alwan_aces_gamut_comp13_params *params);

int alwan_aces_gamut_comp13(alwan_rgb *rgb_out,
                            alwan_rgb const *rgb_in,
                            alwan_aces_gamut_comp13_params const *params);

int alwan_aces_gamut_comp13_inv(alwan_rgb *rgb_out,
                                alwan_rgb const *rgb_in,
                                alwan_aces_gamut_comp13_params const *params);
```

---

## Look Modification Transforms (LMTs)

### Blue Light Artifact Fix

Legacy LMT for neon/blue artifacts from cameras with gamuts outside AP0. Superseded by GamutComp13 in ACES 1.3+.

```c
int alwan_aces_blue_light_fix(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
int alwan_aces_blue_light_fix_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
```

### ACES 1.0 Look

Emulates the ACES 1.0 rendering look when using ACES 1.0.3+ RRT.

```c
int alwan_aces_look_1_0(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
int alwan_aces_look_1_0_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);
```

### Parametric LMT (CDL-style)

```c
typedef struct {
    alwan_scalar slope[3];      /* Per-channel gain. Default: 1.0 */
    alwan_scalar offset[3];     /* Per-channel offset. Default: 0.0 */
    alwan_scalar power[3];      /* Per-channel gamma. Default: 1.0 */
    alwan_scalar saturation;    /* Global saturation. Default: 1.0 */
} alwan_aces_lmt_params;

void alwan_aces_lmt_params_init(alwan_aces_lmt_params *params);

int alwan_aces_lmt_apply(alwan_rgb *rgb_out,
                         alwan_rgb const *rgb_in,
                         alwan_aces_lmt_params const *params);
```

Formula: `out = (in * slope + offset) ^ power`, then saturation adjustment. Input/output in AP1 (ACEScg) linear.

**Example:**
```c
alwan_aces_lmt_params lmt;
alwan_aces_lmt_params_init(&lmt);
lmt.slope[0] = 1.1;        /* Boost red slightly */
lmt.saturation = 1.2;      /* Increase overall saturation */

alwan_aces_lmt_apply(&result, &acescg_pixel, &lmt);
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — Invalid output preset or NULL pointer

---

## See Also

- [Transfer Functions](transfer-functions.md) — OETF/EOTF (PQ, HLG, sRGB)
- [Color Spaces](color-spaces.md) — ACEScg, ACES2065-1 conversions
- [HDR Utilities](hdr.md) — HLG OOTF, MaxCLL/MaxFALL
