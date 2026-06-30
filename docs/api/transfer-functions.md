# Transfer Functions API

Functions for applying OETF (Opto-Electronic) and EOTF (Electro-Optical) transfer functions.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

---

## Overview

Transfer functions convert between linear and encoded (gamma-corrected) values:
- **OETF**: Linear scene light -> Encoded signal (for storage/transmission)
- **EOTF**: Encoded signal -> Linear display light (for rendering)

---

## Core Functions

All batch functions follow the canonical v2.0 argument convention: outputs
before inputs, each `*_stride` (in bytes) immediately after its buffer, then
`count`, then the trailing enum/knob/`ctx` arguments.

### alwan_oetf_apply_{T}

```c
int alwan_oetf_apply_{T}(alwan_{T} *encoded_out, size_t out_stride,
                          alwan_{T} const *linear_in, size_t in_stride,
                          size_t count,
                          alwan_transfer_function tf);
```

Applies OETF (linear -> encoded). Returns `ALWAN_OK` on success.

**Example:**
```c
double linear[300], encoded[300];  /* 100 RGB triplets */
alwan_oetf_apply_{T}(encoded, sizeof(double), linear, sizeof(double),
                     300, ALWAN_TF_SRGB);
```

---

### alwan_eotf_apply_{T}

```c
int alwan_eotf_apply_{T}(alwan_{T} *linear_out, size_t out_stride,
                          alwan_{T} const *encoded_in, size_t in_stride,
                          size_t count,
                          alwan_transfer_function tf);
```

Applies EOTF (encoded -> linear). Returns `ALWAN_OK` on success.

---

### alwan_view_transform_apply_{T}

```c
int alwan_view_transform_apply_{T}(alwan_{T} *rgb_out, size_t out_stride,
                                    alwan_{T} const *rgb_in, size_t in_stride,
                                    size_t count,
                                    alwan_view_transform vt,
                                    alwan_ctx *ctx);
```

Applies view transforms (scene -> display). Returns `ALWAN_OK` on success.
`ctx` is required (it carries the embedded RGB-space registry) and is the
last argument, per convention.

---

## Transfer Function Enum

```c
typedef enum {
    ALWAN_TF_LINEAR = 0,   /* No transfer function */
    ALWAN_TF_SRGB,         /* sRGB */
    ALWAN_TF_BT709,        /* ITU-R BT.709 */
    ALWAN_TF_BT2020,       /* ITU-R BT.2020 */
    ALWAN_TF_PQ,           /* Perceptual Quantizer (SMPTE ST 2084) */
    ALWAN_TF_ST2084,       /* Alias for PQ */
    ALWAN_TF_HLG,          /* Hybrid Log-Gamma */
    ALWAN_TF_BT1886,       /* BT.1886 EOTF */
    ALWAN_TF_ACESPROXY,
    ALWAN_TF_ACESCC,
    ALWAN_TF_ACESCCT,

    /* Camera logs */
    ALWAN_TF_SLOG, ALWAN_TF_SLOG2, ALWAN_TF_SLOG3,
    ALWAN_TF_CLOG, ALWAN_TF_CLOG2, ALWAN_TF_CLOG3,
    ALWAN_TF_VLOG,
    ALWAN_TF_LOGC3, ALWAN_TF_LOGC4,
    ALWAN_TF_REDLOG, ALWAN_TF_REDLOGFILM, ALWAN_TF_LOG3G10,
    ALWAN_TF_BMDFILM, ALWAN_TF_BMDFILM4,
    ALWAN_TF_TLOG, ALWAN_TF_ELOG,
    ALWAN_TF_PROTUNE,
    ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA24, ALWAN_TF_GAMMA26, ALWAN_TF_GAMMA28,
    ALWAN_TF_NLOG,
    ALWAN_TF_CINEON,
    ALWAN_TF_APPLE_LOG,
    ALWAN_TF_FLOG, ALWAN_TF_FLOG2,
    ALWAN_TF_LLOG,
    ALWAN_TF_DLOG,
    ALWAN_TF_DCDM,
    ALWAN_TF_ADX10, ALWAN_TF_ADX16
} alwan_transfer_function;
```

> **Camera log curves.** The `ALWAN_TF_SLOG*`, `ALWAN_TF_NLOG`, `ALWAN_TF_VLOG`,
> `ALWAN_TF_CLOG*`, `ALWAN_TF_REDLOG*`/`ALWAN_TF_LOG3G10`, and related camera-vendor
> log encodings are supported as ordinary `tf` values. These curves carry prior
> correctness fixes: S-Log3 OETF performs no negative pre-clamp (negatives fall
> into the linear segment, callers clip), and N-Log OETF uses a signed cube root
> (`sign(arg) * |arg|^(1/3)`) to avoid `NaN` when `linear < -b`.

---

## Arbitrary Gamma

```c
int alwan_gamma_oetf_{T}(alwan_{T} *out, size_t out_stride,
                          alwan_{T} const *in, size_t in_stride,
                          size_t count, alwan_{T} gamma);

int alwan_gamma_eotf_{T}(alwan_{T} *out, size_t out_stride,
                          alwan_{T} const *in, size_t in_stride,
                          size_t count, alwan_{T} gamma);
```

Apply arbitrary gamma values not covered by the enum. OETF: `pow(in, 1/gamma)`, EOTF: `pow(in, gamma)`.

---

## Bit Depth Conversion

```c
int alwan_uint_to_float_{T}(alwan_{T} *out, alwan_uint16 const *in,
                             int bit_depth, size_t count);
int alwan_float_to_uint_{T}(alwan_uint16 *out, alwan_{T} const *in,
                             int bit_depth, size_t count);
```

Convert between integer code values and floating-point [0, 1] range. Supports arbitrary bit depths.

---

## View Transform Enum

```c
typedef enum {
    ALWAN_VIEW_ACES_REC709,         /* ACES RRT + ODT Rec.709 */
    ALWAN_VIEW_AGX,                 /* AgX base */
    ALWAN_VIEW_AGX_PUNCHY,          /* AgX punchy variant */
    ALWAN_VIEW_AGX_GOLDEN,          /* AgX golden (warm highlights, cool shadows) */
    ALWAN_VIEW_BT2446A_HDR_TO_SDR,  /* BT.2446 Method A: HDR to SDR */
    ALWAN_VIEW_BT2446A_SDR_TO_HDR,  /* BT.2446 Method A: SDR to HDR */
    ALWAN_VIEW_KHRONOS_PBR_NEUTRAL, /* Khronos PBR Neutral (glTF/WebGL) */
    ALWAN_VIEW_REINHARD_EXT,        /* Reinhard Extended (luminance-based, Reinhard 2002) */
    ALWAN_VIEW_UCHIMURA,            /* Uchimura / Gran Turismo (parametric S-curve) */
    ALWAN_VIEW_LOTTES,              /* Lottes / AMD Cauldron (parametric rational curve) */
    ALWAN_VIEW_TONY_MCMAPFACE,      /* Somewhat Boring Display Transform (Stachowiak 2023) */
    ALWAN_VIEW_BT2446B_SDR_TO_HDR,  /* BT.2446 Method B: SDR to HDR up-conversion */
    ALWAN_VIEW_BT2446C_HDR_TO_SDR,  /* BT.2446 Method C: HDR to SDR (quantization-aware) */
    ALWAN_VIEW_BT2390_HDR_TO_SDR,   /* BT.2390 EETF: HDR to SDR (Hermite spline) */
    ALWAN_VIEW_REINHARD_CALIBRATED, /* Reinhard calibrated (key-based, Reinhard 2002) */
    ALWAN_VIEW_EXPOSURE             /* Exposure-based with shoulder compression */
} alwan_view_transform;
```

---

## Common Usage

### sRGB Encoding/Decoding

```c
double linear_r = 0.5;
double encoded_r;
alwan_oetf_apply_{T}(&encoded_r, sizeof(double), &linear_r, sizeof(double),
                     1, ALWAN_TF_SRGB);
```

### HDR PQ Encoding

```c
alwan_oetf_apply_{T}(encoded, sizeof(double), linear, sizeof(double),
                     count, ALWAN_TF_PQ);
```

---

## Error Codes

- `ALWAN_OK` (0) -- Success
- `ALWAN_E_INVALID` (-1) -- Invalid transfer function or NULL pointer
- `ALWAN_E_RANGE` (-3) -- Value out of supported range

---

## See Also

- [Color Spaces](color-spaces.md) -- XYZ/RGB conversions
- [Examples](../examples.md) -- Usage examples
