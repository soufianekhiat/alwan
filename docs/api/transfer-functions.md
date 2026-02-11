# Transfer Functions API

Functions for applying OETF (Opto-Electronic) and EOTF (Electro-Optical) transfer functions.

---

## Overview

Transfer functions convert between linear and encoded (gamma-corrected) values:
- **OETF**: Linear scene light → Encoded signal (for storage/transmission)
- **EOTF**: Encoded signal → Linear display light (for rendering)

---

## Core Functions

### alwan_oetf_apply

```c
int alwan_oetf_apply(
    alwan_scalar *encoded,        // Output encoded values
    alwan_transfer_function tf,   // Transfer function enum
    alwan_scalar const *linear,   // Input linear values
    size_t count,                 // Number of values
    size_t in_stride,             // Input stride in bytes
    size_t out_stride             // Output stride in bytes
);
```

Applies OETF (linear → encoded). Returns `ALWAN_OK` on success.

**Example:**
```c
alwan_scalar linear[300], encoded[300];  // 100 RGB triplets
alwan_oetf_apply(encoded, ALWAN_TF_SRGB, linear, 300,
                 sizeof(alwan_scalar), sizeof(alwan_scalar));
```

---

### alwan_eotf_apply

```c
int alwan_eotf_apply(
    alwan_scalar *linear,         // Output linear values
    alwan_transfer_function tf,   // Transfer function enum
    alwan_scalar const *encoded,  // Input encoded values
    size_t count,
    size_t in_stride,
    size_t out_stride
);
```

Applies EOTF (encoded → linear). Returns `ALWAN_OK` on success.

---

### alwan_view_transform_apply

```c
int alwan_view_transform_apply(
    alwan_scalar *rgb_out,        // Output display-referred RGB
    alwan_ctx *ctx,               // Context (can be NULL)
    alwan_view_transform vt,      // View transform enum
    alwan_scalar const *rgb_in,   // Input scene-referred RGB
    size_t count,
    size_t in_stride,
    size_t out_stride
);
```

Applies view transforms (scene → display). Returns `ALWAN_OK` on success.

---

## Transfer Function Enum

```c
typedef enum {
    ALWAN_TF_LINEAR = 0,   // No transfer function
    ALWAN_TF_SRGB,         // sRGB
    ALWAN_TF_BT709,        // ITU-R BT.709
    ALWAN_TF_BT2020,       // ITU-R BT.2020
    ALWAN_TF_PQ,           // Perceptual Quantizer (ST.2084)
    ALWAN_TF_ST2084,       // Alias for PQ
    ALWAN_TF_HLG,          // Hybrid Log-Gamma
    ALWAN_TF_BT1886,       // BT.1886 EOTF
    ALWAN_TF_ACESPROXY,
    ALWAN_TF_ACESCC,
    ALWAN_TF_ACESCCT,

    // Camera logs
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

---

## View Transform Enum

```c
typedef enum {
    ALWAN_VIEW_ACES_REC709,  // ACES RRT + ODT Rec.709
    ALWAN_VIEW_AGX,          // AgX base
    ALWAN_VIEW_AGX_PUNCHY    // AgX punchy variant
} alwan_view_transform;
```

---

## Common Usage

### sRGB Encoding/Decoding

```c
alwan_scalar linear_r = 0.5;
alwan_scalar encoded_r;
alwan_oetf_apply(&encoded_r, ALWAN_TF_SRGB, &linear_r, 1,
                 sizeof(alwan_scalar), sizeof(alwan_scalar));
```

### HDR PQ Encoding

```c
alwan_oetf_apply(encoded, ALWAN_TF_PQ, linear, count,
                 sizeof(alwan_scalar), sizeof(alwan_scalar));
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — Invalid transfer function or NULL pointer
- `ALWAN_E_RANGE` (-3) — Value out of supported range

---

## See Also

- [Color Spaces](color-spaces.md) — XYZ/RGB conversions
- [Examples](../examples.md) — Usage examples
