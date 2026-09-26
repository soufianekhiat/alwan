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

> **f64-internal facades.** The **ACES 1.x inverse** output transform runs in `double`
> (its RedMod10 inverse is an iteration whose threshold sits below `float` epsilon). Its
> `_f32` entry points, like ZCAM's ([color-appearance](color-appearance.md)),
> run the algorithm in `double` internally and narrow the result,
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
    /* ACES 1.1 to 1.3 RRTODT.Academy.Rec2020_*nits_15nits_ST2084: the SSTS, 0.18 at 15 cd/m2 */
    ALWAN_ACES1_OUT_REC2020_1000NIT_PQ,   /* Rec.2020, 1000 nits, PQ */
    ALWAN_ACES1_OUT_REC2020_2000NIT_PQ,   /* Rec.2020, 2000 nits, PQ */
    ALWAN_ACES1_OUT_REC2020_4000NIT_PQ,   /* Rec.2020, 4000 nits, PQ */
    /* ACES 1.0.3 ODT.Academy.Rec2020_ST2084_*nits: C5 + C9 splines, 0.18 at 10 cd/m2 */
    ALWAN_ACES1_OUT_REC2020_1000NIT_PQ_V103,
    ALWAN_ACES1_OUT_REC2020_2000NIT_PQ_V103,
    ALWAN_ACES1_OUT_REC2020_4000NIT_PQ_V103,

    /* Cinema */
    ALWAN_ACES1_OUT_DCDM_48NIT,           /* DCDM X'Y'Z', 48 nits, Gamma 2.6 */

    ALWAN_ACES1_OUT_COUNT                 /* Sentinel -- number of presets */
} alwan_aces1_output;
```

### alwan_aces1_output_transform_{T} / alwan_aces1_output_transform_inv_{T}

```c
alwan_status alwan_aces1_output_transform_{T}(alwan_rgb_{T} *rgb_out,
                                               alwan_rgb_{T} const *rgb_in,
                                               alwan_aces1_output output,
                                               alwan_aces_interp interp);

alwan_status alwan_aces1_output_transform_inv_{T}(alwan_rgb_{T} *rgb_out,
                                                   alwan_rgb_{T} const *rgb_in,
                                                   alwan_aces1_output output);
```

Complete ACES 1.3 rendering pipeline (RRT + ODT). Input: ACES2065-1 (AP0 linear). Output: display-encoded RGB.

`interp` is the tone curve method: `ALWAN_ACES_INTERP_BSPLINE` is the Academy CTL reference,
`ALWAN_ACES_INTERP_OCIO` promises OCIO's pixels (honoured for ten of the fifteen presets) and
`ALWAN_ACES_INTERP_HERMITE` is the legacy fit. It is validated like the preset, `ALWAN_E_INVALID`
outside the enum. Until 3.0.0 it was a process-wide global set through
`alwan_set_aces_interp`. The inverse takes no method: it inverts the B-spline (and SSTS) chain,
so a forward run under HERMITE or OCIO does not round-trip through it. Its C9 and C5 inverses
are the CTL's closed forms (`segmented_spline_c9_rev`, `_c5_rev`); every SDR preset takes a
code from the forward back to that code within 1e-14, near black included, and greys match
OCIO's inverse to 3e-4, the gap between OCIO's fitted curves and the CTL splines (suite 262).
A code the forward cannot produce, such as a colour whose RRT clamp removed a negative AP1
channel, has no exact preimage: DCDM shows 2.6e-5 on a few saturated blues. Which curve
chain each method runs on each preset is tabulated in [Context](context.md).

```c
alwan_status alwan_aces1_output_transform_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                                             alwan_{T} const *in, size_t in_stride,
                                                             size_t count, alwan_aces1_output output,
                                                             alwan_aces_interp interp);
```

The same transform over a buffer of RGB triples, strides in bytes. The preset is validated once
and each pixel is the scalar transform, so a map and its scalar twin agree to the bit (suite 56
asserts equality over every preset). Suite 56 also holds seven presets to OCIO's display views
at a thirtieth of a 10-bit code.

**Example:**
```c
alwan_rgb_{T} aces_pixel = {0.18, 0.18, 0.18};  /* 18% gray in AP0 */
alwan_rgb_{T} display;

alwan_aces1_output_transform_{T}(&display, &aces_pixel,
                                  ALWAN_ACES1_OUT_SRGB_100NIT,
                                  ALWAN_ACES_INTERP_BSPLINE);
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
    ALWAN_ACES2_OUT_DCDM_48NIT,               /* DCDM (P3-D65 Limited), X'Y'Z' */
    ALWAN_ACES2_OUT_P3DCI_48NIT,              /* P3-D65 (48 nits), Gamma 2.6 */

    ALWAN_ACES2_OUT_COUNT                     /* Sentinel -- number of presets */
} alwan_aces2_output;
```

### alwan_aces2_output_transform_{T} / alwan_aces2_output_transform_inv_{T}

```c
alwan_status alwan_aces2_output_transform_{T}(alwan_rgb_{T} *rgb_out,
                                               alwan_rgb_{T} const *rgb_in,
                                               alwan_aces2_output output);

alwan_status alwan_aces2_output_transform_inv_{T}(alwan_rgb_{T} *rgb_out,
                                                   alwan_rgb_{T} const *rgb_in,
                                                   alwan_aces2_output output);
```

Complete ACES 2.0 rendering pipeline. Input: ACEScg (AP1 linear). Output: display-encoded RGB.

Pipeline stages: AP1 -> JMh -> Tonescale + Chroma compress -> Gamut compress -> RGB -> Chromatic adaptation -> Display EOTF.

The inverse takes display-encoded RGB back to ACEScg by undoing each stage in reverse,
as OCIO's `ACES_OUTPUT_TRANSFORM_20` inverse does:
1. Decode, back to display-linear with 1.0 = 100 nits. For PQ this means nits divided
   by 100. For HLG it means the inverse OETF, then the OOTF with BT.2100's system gamma
   for the peak, rescaled by peak / 100.
2. Convert to JMh with the limiting primaries' parameters, which is where the forward
   did its D60 to D65 adaptation.
3. Invert the gamut compression. Above the analytical threshold this is OCIO's own
   one-step approximation (`Jx`).
4. Invert the tonescale, then the chroma compression.
5. Convert JMh back to AP1 RGB.

The cinema presets follow the Academy's aces-output CTL. Both render at a peak of 100,
the tonescale of a 100 nit display that the projector shows at 48 nits, limited to
P3-D65 and clamped to [0, 1]. DCDM is the CTL's "DCDM (P3-D65 Limited)": the limit RGB
goes to XYZ with an equal-energy white, times 48 / 52.37, then gamma 2.6. The preset
named P3DCI is the CTL's "P3-D65 (48 nits)", P3-D65 at gamma 2.6, since ACES 2.0 has no
P3-DCI output; the enum keeps its name. The inverse decodes DCDM's XYZ back to the
P3-D65 limit RGB and continues as for any other preset.

Suite 258 holds presets 0 to 8 to OCIO 2.5's inverse, and the two cinema presets'
forward and inverse to OCIO's fixed function at peak 100 and P3-D65 followed by the
CTL's encoding. The median error is at float32's
floor, a few 1e-7 to 2e-6. The tails, up to 3e-4 at the 99th percentile in SDR, come
from alwan's gamut-compression tables, which differ from OCIO 2.5's by a median 1e-5 in
the forward. The inverse magnifies that difference near the gamut and peak boundaries.
Forward then inverse returns the scene value to a few 1e-6 wherever the display did not
clamp it. Clamped values cannot come back.

Until 2026-09-25 the inverse skipped the tonescale and the chroma compression. It also
converted through an adaptation matrix instead of the limiting primaries' JMh, and the
cinema presets used a hand-typed XYZ to AP1 matrix with a wrong third row. SDR output
was off by nearly the whole value, and PQ output by up to 661 times.

Until 2026-09-26 the cinema presets ran a chain of their own that was not ACES 2.0: a
tonescale for a 48 nit peak, the result converted from AP1 through D60 matrices, and DCDM
normalised to an equal-energy white by dividing X and Z by D60's. Their codes sat a
median 0.22 from the CTL's, a grey of scene 1.0 topped out below 0.79, and a code from
the forward could come back from the inverse wrong by up to 7e7.

### alwan_aces2_output_transform_custom_{T}

```c
alwan_status alwan_aces2_output_transform_custom_{T}(alwan_rgb_{T} *rgb_out,
                                                      alwan_rgb_{T} const *rgb_in,
                                                      alwan_{T} peak_luminance,
                                                      alwan_aces_primaries_{T} const *limit_primaries,
                                                      alwan_transfer_function eotf);
```

Custom output transform with user-specified peak luminance, limiting primaries, and display EOTF.

### alwan_aces2_output_transform_{T}_map_interleave

```c
alwan_status alwan_aces2_output_transform_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
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

### alwan_aces2_output_transform_inv_{T}_map_interleave / _map_planar

```c
alwan_status alwan_aces2_output_transform_inv_{T}_map_interleave(alwan_{T} *out, size_t out_stride,
                                                                 alwan_{T} const *in, size_t in_stride,
                                                                 size_t count,
                                                                 alwan_aces2_output output);
alwan_status alwan_aces2_output_transform_inv_{T}_map_planar(alwan_{T} *out_ch0, size_t out_stride,
                                                             alwan_{T} *out_ch1, alwan_{T} *out_ch2,
                                                             alwan_{T} const *in_ch0, size_t in_stride,
                                                             alwan_{T} const *in_ch1, alwan_{T} const *in_ch2,
                                                             size_t count, alwan_aces2_output output);
```

The inverse over a buffer: display code values in, ACEScg (AP1 linear) out, strides in
bytes. The scalar inverse builds the preset's tables on every call: the JMh parameters,
the tonescale, the chroma compression and the gamut-compression tables. That costs
about 1.1 ms a pixel on an x64 desktop, and the forward scalar costs the same. These
forms build the tables once and send every pixel through the scalar's own per-pixel
code, so the two return the same bits. Measured through the Release DLL, they are about
900 to 1,000 times faster: 0.8 to 1.7 us a pixel. The f32 forms widen each pixel to the
f64 path and narrow the result, as the f32 scalar does. The planar form runs the
interleave form on a packed tile.

`ALWAN_E_INVALID` for a NULL buffer, `count` 0 or an unknown preset. A pixel whose decode
fails stops the loop with its status, and the pixels before it are written. Suite 258
holds both forms equal to the scalar on every preset, in both precisions.

The unified ACES 2.0 output transform therefore covers five entry shapes:
**presets** (`_output_transform`), **inverse** (`_output_transform_inv`),
**custom** (`_output_transform_custom`), and the **batch-initialized** forward and
inverse (`_output_transform_{T}_map_interleave`, `_output_transform_inv_{T}_map_interleave`).
For more than a few pixels, use the map forms: the scalar forms rebuild their tables on
every call.

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

All of these work in ACES2065-1 (AP0) linear and are held to OCIO's
`FixedFunctionTransform` of the same name in both directions (suite 262): 1e-7 forward,
OCIO's float32. The glow uses the CTL's `sigmoid_shaper` (`ACESlib.RRT_Common.ctl`).
RedMod03's inverse is OCIO's closed form, exact where every channel is non-negative and red
is at least 0.01. RedMod10's inverse is exact everywhere; OCIO's evaluates its hue weight on
the output and does not undo its own forward on about a quarter of red inputs, so it is not
the reference there.

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

### ACES 0.x sweeteners under a 1.x RRT (`look_1_0`)

```c
void alwan_aces_look_1_0_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
void alwan_aces_look_1_0_inv_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in);
```

Glow10 inverse, Glow03, RedMod10 inverse, RedMod03, in ACES2065-1. Placed before an ACES
1.x RRT, the RRT's own Glow10 and RedMod10 cancel and the 0.x glow and red modifier remain.
Each step is held to OCIO; the composition is alwan's and is not an aces-dev transform. The
Academy's own emulation of the earlier look, `LMT.Academy.ACES_0_1_1.ctl` ("ACES 1.0 to 0.1
emulation"), is a 65^3 LUT that also moves the tone scale, and this does not reproduce it.

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
