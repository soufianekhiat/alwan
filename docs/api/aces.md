# ACES Pipeline API

Functions for the Academy Color Encoding System (ACES) 1.x and 2.0 rendering pipelines.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

---

## Overview

Alwan provides a complete ACES implementation covering:

- **ACES 1.x** -- RRT + ODT output transforms with 12 display presets
- **ACES 2.0** -- New perceptual gamut mapping with 13 display presets
- **Fixed Functions** -- RRT components (RedMod, Glow, DarkToDim)
- **Gamut Compression** -- ACES 1.3 Reference Gamut Compression
- **LMTs** -- Look Modification Transforms (parametric CDL + legacy)

**Color spaces used:**
- **AP0 (ACES2065-1)** -- Scene-referred linear, full spectral gamut
- **AP1 (ACEScg)** -- Working space for VFX compositing
- **JMh** -- ACES 2.0 perceptual color coordinates

> **f64-internal facades.** The **ACES 1.x inverse** output transform and the
> **ZCAM inverse** ([color-appearance](color-appearance.md)) are iterative
> inverses whose convergence thresholds sit below `float` epsilon. Their `_f32`
> entry points run the algorithm in `double` internally and narrow the result,
> so they stay available and numerically stable even in an `f32`-only build
> (`ALWAN_BUILD_ONLY_F32`). This machinery is gated by `ALWAN_WITH_F64_FACADE`
> (always `1`) rather than `ALWAN_WITH_F64`. See
> [configuration](../configuration.md) and
> [precision-and-limits](../precision-and-limits.md).

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
    ALWAN_ACES1_OUT_DCDM_48NIT,           /* DCDM X'Y'Z', 48 nits, Gamma 2.6 */

    ALWAN_ACES1_OUT_COUNT                 /* Sentinel -- number of presets */
} alwan_aces1_output;
```

### alwan_aces1_output_transform_{T} / alwan_aces1_output_transform_inv_{T}

```c
int alwan_aces1_output_transform_{T}(alwan_rgb_{T} *rgb_out,
                                      alwan_rgb_{T} const *rgb_in,
                                      alwan_aces1_output output);

int alwan_aces1_output_transform_inv_{T}(alwan_rgb_{T} *rgb_out,
                                          alwan_rgb_{T} const *rgb_in,
                                          alwan_aces1_output output);
```

Complete ACES 1.3 rendering pipeline (RRT + ODT). Input: ACES2065-1 (AP0 linear). Output: display-encoded RGB.

**Example:**
```c
alwan_rgb_{T} aces_pixel = {0.18, 0.18, 0.18};  /* 18% gray in AP0 */
alwan_rgb_{T} display;

alwan_aces1_output_transform_{T}(&display, &aces_pixel,
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
    ALWAN_ACES2_OUT_P3DCI_48NIT,              /* P3-DCI, Gamma 2.6 */

    ALWAN_ACES2_OUT_COUNT                     /* Sentinel -- number of presets */
} alwan_aces2_output;
```

### alwan_aces2_output_transform_{T} / alwan_aces2_output_transform_inv_{T}

```c
int alwan_aces2_output_transform_{T}(alwan_rgb_{T} *rgb_out,
                                      alwan_rgb_{T} const *rgb_in,
                                      alwan_aces2_output output);

int alwan_aces2_output_transform_inv_{T}(alwan_rgb_{T} *rgb_out,
                                          alwan_rgb_{T} const *rgb_in,
                                          alwan_aces2_output output);
```

Complete ACES 2.0 rendering pipeline. Input: ACEScg (AP1 linear). Output: display-encoded RGB.

Pipeline stages: AP1 -> JMh -> Tonescale + Chroma compress -> Gamut compress -> RGB -> Chromatic adaptation -> Display EOTF.

### alwan_aces2_output_transform_custom_{T}

```c
int alwan_aces2_output_transform_custom_{T}(alwan_rgb_{T} *rgb_out,
                                             alwan_rgb_{T} const *rgb_in,
                                             alwan_{T} peak_luminance,
                                             alwan_aces_primaries_{T} const *limit_primaries,
                                             alwan_transfer_function eotf);
```

Custom output transform with user-specified peak luminance, limiting primaries, and display EOTF.

### alwan_aces2_output_transform_{T}_map_interleave

```c
int alwan_aces2_output_transform_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                                    alwan_{T} const *in, size_t in_stride,
                                                    size_t count,
                                                    alwan_aces2_output output);
```

Batch (map) form over interleaved RGB triplets. The preset's parameters
(limiting primaries, peak luminance, EOTF) are initialized **once** and reused
across all `count` pixels, so this is much faster than calling the per-pixel
`alwan_aces2_output_transform_{T}` in a loop. `out_stride` / `in_stride` are
byte strides between consecutive RGB triplets (typically `3 * sizeof(alwan_{T})`
for tightly packed data).

The unified ACES 2.0 output transform therefore covers four entry shapes:
**presets** (`_output_transform`), **inverse** (`_output_transform_inv`),
**custom** (`_output_transform_custom`), and **batch-initialized**
(`_output_transform_{T}_map_interleave`).

---

## ACES 2.0 Components

### Tonescale Compression

```c
void alwan_aces_tonescale_compress20_{T}(alwan_rgb_{T} *rgb_out,
                                         alwan_rgb_{T} const *rgb_in,
                                         alwan_{T} peak_luminance);
```

### JMh Color Appearance Encoding

```c
/* Primaries descriptor for ACES 2.0 */
typedef struct {
    alwan_{T} red_x, red_y;
    alwan_{T} green_x, green_y;
    alwan_{T} blue_x, blue_y;
    alwan_{T} white_x, white_y;
} alwan_aces_primaries_{T};

/* Initialize with AP1 defaults */
void alwan_aces_primaries_ap1_default_{T}(alwan_aces_primaries_{T} *primaries);

/* Convert to/from JMh appearance coordinates (forward + inverse) */
void alwan_aces_rgb_to_jmh20_{T}(alwan_vec3_{T} *jmh_out,
                                  alwan_rgb_{T} const *rgb_in,
                                  alwan_aces_primaries_{T} const *primaries);

void alwan_aces_jmh_to_rgb20_{T}(alwan_rgb_{T} *rgb_out,
                                  alwan_vec3_{T} const *jmh_in,
                                  alwan_aces_primaries_{T} const *primaries);
```

`rgb_to_jmh20` (forward) and `jmh_to_rgb20` (inverse) are exact inverses of each
other and round-trip. They return `void` (no error code): the conversion is
total over finite inputs.

### Gamut Compression (ACES 2.0)

```c
void alwan_aces_gamut_compress20_{T}(alwan_vec3_{T} *jmh_out,
                                     alwan_vec3_{T} const *jmh_in,
                                     alwan_{T} peak_luminance,
                                     alwan_aces_primaries_{T} const *limit_primaries);

void alwan_aces_gamut_compress20_inv_{T}(alwan_vec3_{T} *jmh_out,
                                         alwan_vec3_{T} const *jmh_in,
                                         alwan_{T} peak_luminance,
                                         alwan_aces_primaries_{T} const *limit_primaries);
```

**Embedded gamut-reach tables.** The ACES 2.0 gamut compressor reads
per-display **gamut cusp / reach boundary** tables that are embedded into the
library at compile time (`ALWAN_EMBED_DATA=1`, the only supported mode). The
CSV sources live under `src/alwan/data/aces2/gamut_*.csv` (one per
limiting-gamut + peak-luminance preset, e.g. `gamut_rec709_100.csv`,
`gamut_p3d65_1000.csv`, `gamut_rec2020_4000.csv`) and are produced by the
`gendata` pipeline (`alwan_dev/gendata/aces2_gamut_tables.py`), never
hand-authored. The provenance of the related ACES 2.0 Fourier chroma
normalization arrays is tracked as an open documentation item in
[violations.md](../violations.md).

---

## ACES Fixed Functions (RRT Components)

### RedMod -- Red channel desaturation

```c
void alwan_aces_redmod03_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
void alwan_aces_redmod10_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
void alwan_aces_redmod03_inv_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
void alwan_aces_redmod10_inv_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
```

### Glow -- Flare/glow compensation

```c
void alwan_aces_glow03_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
void alwan_aces_glow10_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
void alwan_aces_glow03_inv_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
void alwan_aces_glow10_inv_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
```

### DarkToDim -- Surround compensation

```c
void alwan_aces_dark_to_dim10_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
```

---

## Gamut Compression (ACES 1.3)

```c
typedef struct {
    alwan_{T} lim_cyan, lim_magenta, lim_yellow;     /* Compression limits */
    alwan_{T} thr_cyan, thr_magenta, thr_yellow;     /* Thresholds */
    alwan_{T} power;                                  /* Compression power */
} alwan_aces_gamut_comp13_params_{T};

void alwan_aces_gamut_comp13_params_default_{T}(alwan_aces_gamut_comp13_params_{T} *params);

void alwan_aces_gamut_comp13_{T}(alwan_rgb_{T} *rgb_out,
                                 alwan_rgb_{T} const *rgb_in,
                                 alwan_aces_gamut_comp13_params_{T} const *params);

void alwan_aces_gamut_comp13_inv_{T}(alwan_rgb_{T} *rgb_out,
                                     alwan_rgb_{T} const *rgb_in,
                                     alwan_aces_gamut_comp13_params_{T} const *params);
```

---

## Look Modification Transforms (LMTs)

### Blue Light Artifact Fix

```c
void alwan_aces_blue_light_fix_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
void alwan_aces_blue_light_fix_inv_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
```

### ACES 1.0 Look

```c
void alwan_aces_look_1_0_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
void alwan_aces_look_1_0_inv_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
```

### Parametric LMT (CDL-style)

```c
typedef struct {
    alwan_{T} slope[3];      /* Per-channel gain. Default: 1.0 */
    alwan_{T} offset[3];     /* Per-channel offset. Default: 0.0 */
    alwan_{T} power[3];      /* Per-channel gamma. Default: 1.0 */
    alwan_{T} saturation;    /* Global saturation. Default: 1.0 */
} alwan_aces_lmt_params_{T};

void alwan_aces_lmt_params_init_{T}(alwan_aces_lmt_params_{T} *params);

void alwan_aces_lmt_apply_{T}(alwan_rgb_{T} *rgb_out,
                              alwan_rgb_{T} const *rgb_in,
                              alwan_aces_lmt_params_{T} const *params);
```

Formula: `out = (in * slope + offset) ^ power`, then saturation adjustment. Input/output in AP1 (ACEScg) linear.

**Example:**
```c
alwan_aces_lmt_params_{T} lmt;
alwan_aces_lmt_params_init_{T}(&lmt);
lmt.slope[0] = 1.1;      /* Boost red slightly */
lmt.saturation = 1.2;    /* Increase overall saturation */

alwan_aces_lmt_apply_{T}(&result, &acescg_pixel, &lmt);
```

---

## Error Codes

The ACES 1.x / ACES 2.0 **output transforms** (`alwan_aces1_output_transform*`,
`alwan_aces2_output_transform*`, including the `_custom` and `_map_interleave`
forms) return an `int` status:

- `ALWAN_OK` (0) -- Success
- `ALWAN_E_INVALID` (-1) -- Invalid output preset or NULL pointer

The fixed functions, LMTs, JMh conversions, tonescale, and gamut-compression
helpers shown above return `void` (total over finite inputs, no status code).

---

## See Also

- [Transfer Functions](transfer-functions.md) -- OETF/EOTF (PQ, HLG, sRGB)
- [Color Spaces](color-spaces.md) -- ACEScg, ACES2065-1 conversions
- [HDR Utilities](hdr.md) -- HLG OOTF, MaxCLL/MaxFALL
