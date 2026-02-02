# Transfer Functions API

Functions for encoding and decoding non-linear color representations.

---

## Overview

Transfer functions (also called gamma curves or EOTFs/OETFs) convert between:
- **Linear light** — Physical light values proportional to photons
- **Encoded values** — Non-linear values optimized for storage/transmission

**Key terminology:**
- **EOTF** (Electro-Optical Transfer Function) — Encoded → Linear (decoding)
- **OETF** (Opto-Electronic Transfer Function) — Linear → Encoded (encoding)
- **OOTF** (Opto-Optical Transfer Function) — Scene → Display (includes both)

---

## Core Functions

### alwan_eotf_apply

```c
alwan_result alwan_eotf_apply(
    alwan_vec3 *linear,           // Output first
    alwan_ctx *ctx,
    const char *transfer,
    const alwan_vec3 *encoded,
    size_t count,
    size_t in_stride,
    size_t out_stride
);
```

Applies inverse transfer function (EOTF): encoded → linear.

**Parameters:**
- `linear` — Output linear values [0, ∞)
- `ctx` — Library context
- `transfer` — Transfer function name (e.g., `"srgb"`, `"bt709"`, `"pq"`)
- `encoded` — Input encoded values [0, 1]
- `count` — Number of colors
- `in_stride` — Input stride in bytes (0 = packed)
- `out_stride` — Output stride in bytes (0 = packed)

**Use cases:**
- Loading encoded images for processing
- Converting display-referred to scene-referred
- Preparing for linear color operations

**Example:**
```c
// Decode sRGB image to linear (output first)
float srgb_encoded[1920 * 1080 * 3];  // [0, 1]
float srgb_linear[1920 * 1080 * 3];   // [0, 1]

alwan_eotf_apply((alwan_vec3*)srgb_linear, ctx, "srgb",
                 (alwan_vec3*)srgb_encoded, 1920 * 1080, 12, 12);
```

**Precision:** ±1e-6 (float), ±1e-12 (double)

---

### alwan_oetf_apply

```c
alwan_result alwan_oetf_apply(
    alwan_vec3 *encoded,          // Output first
    alwan_ctx *ctx,
    const char *transfer,
    const alwan_vec3 *linear,
    size_t count,
    size_t in_stride,
    size_t out_stride
);
```

Applies transfer function (OETF): linear → encoded.

**Parameters:** (similar to `alwan_eotf_apply`, output first)

**Use cases:**
- Encoding linear images for display
- Converting scene-referred to display-referred
- Preparing for storage/transmission

**Example:**
```c
// Encode linear RGB to sRGB (output first)
float linear[1000 * 3];   // [0, 1] or [0, ∞) for HDR
float encoded[1000 * 3];  // [0, 1]

alwan_oetf_apply((alwan_vec3*)encoded, ctx, "srgb",
                 (alwan_vec3*)linear, 1000, 12, 12);
```

**Note:** Values outside [0, 1] are clamped for SDR transfer functions.

---

## Supported Transfer Functions

### SDR (Standard Dynamic Range)

#### sRGB / BT.709

```c
alwan_eotf_apply(ctx, "srgb", ...);
alwan_eotf_apply(ctx, "bt709", ...);  // Same as sRGB
```

**Characteristics:**
- **Range:** [0, 1] → [0, 1]
- **Gamma:** ~2.2 (piecewise with linear segment)
- **Use case:** Web, sRGB displays, HDTV

**Formula (EOTF):**
```
if (V < 0.04045)
    L = V / 12.92
else
    L = ((V + 0.055) / 1.055)^2.4
```

**Precision:** ±1e-7

---

#### Gamma 2.2

```c
alwan_eotf_apply(ctx, "gamma_2.2", ...);
```

**Characteristics:**
- **Range:** [0, 1] → [0, 1]
- **Gamma:** Exact 2.2 (pure power)
- **Use case:** Simple gamma correction, legacy systems

**Formula (EOTF):**
```
L = V^2.2
```

---

#### BT.1886

```c
alwan_eotf_apply(ctx, "bt1886", ...);
```

**Characteristics:**
- **Range:** [0, 1] → [0, 1]
- **Gamma:** 2.4 (reference display EOTF)
- **Use case:** Broadcast reference displays

---

### HDR (High Dynamic Range)

#### ST.2084 (PQ - Perceptual Quantizer)

```c
alwan_eotf_apply(ctx, "pq", ...);
alwan_eotf_apply(ctx, "st2084", ...);  // Same as "pq"
```

**Characteristics:**
- **Range:** [0, 1] → [0, 10000] cd/m²
- **Peak:** 10,000 nits
- **Use case:** HDR10, HDR video, Dolby Vision

**Formula (EOTF):**
```
Y = 10000 * ((max(V^(1/m) - c1, 0) / (c2 - c3 * V^(1/m)))^(1/n))
where m=78.8438, c1=0.8359, c2=18.8516, c3=18.6875, n=0.1593
```

**Perceptual:** Designed to match human perception across full luminance range

**Precision:** ±1e-6 (float), ±1e-11 (double)

---

#### HLG (Hybrid Log-Gamma)

```c
alwan_eotf_apply(ctx, "hlg", ...);
```

**Characteristics:**
- **Range:** [0, 1] → [0, 1000] cd/m² (nominal)
- **Peak:** Scene-dependent (typically 1000 nits)
- **Use case:** HDR broadcast, backwards-compatible with SDR

**Formula (EOTF):**
```
if (V <= 0.5)
    L = (V^2) / 3
else
    L = (exp((V - c) / a) + b) / 12
where a=0.17883277, b=0.28466892, c=0.55991073
```

**Hybrid:** SDR-compatible (first half ~= gamma 2.0)

---

### Log Curves (Camera/Cinema)

#### S-Log (Sony)

```c
alwan_eotf_apply(ctx, "slog", ...);    // S-Log
alwan_eotf_apply(ctx, "slog2", ...);   // S-Log2
alwan_eotf_apply(ctx, "slog3", ...);   // S-Log3
```

**Characteristics:**
- **Range:** Wide dynamic range capture
- **Use case:** Sony cinema cameras, post-production

---

#### C-Log (Canon)

```c
alwan_eotf_apply(ctx, "clog", ...);    // Canon Log
```

**Characteristics:**
- **Range:** ~14 stops dynamic range
- **Use case:** Canon cinema cameras

---

#### V-Log (Panasonic)

```c
alwan_eotf_apply(ctx, "vlog", ...);
```

**Characteristics:**
- **Range:** ~14 stops dynamic range
- **Use case:** Panasonic cinema cameras

---

#### ARRI LogC

```c
alwan_eotf_apply(ctx, "logc3", ...);   // LogC3
alwan_eotf_apply(ctx, "logc4", ...);   // LogC4
```

**Characteristics:**
- **Range:** ~14-15 stops dynamic range
- **Use case:** ARRI Alexa cameras

---

### ACES

#### ACEScc / ACEScct

```c
alwan_eotf_apply(ctx, "acescc", ...);   // ACEScc (log)
alwan_eotf_apply(ctx, "acescct", ...);  // ACEScct (log with toe)
```

**Characteristics:**
- **Working space:** ACES AP1 color space
- **Use case:** ACES color grading, VFX

---

## View Transforms

View transforms go beyond simple transfer functions, often including tone mapping and gamut mapping.

### alwan_view_transform_apply

```c
alwan_result alwan_view_transform_apply(
    alwan_vec3 *display_rgb,      // Output first
    alwan_ctx *ctx,
    const char *transform,
    const alwan_vec3 *scene_rgb,
    size_t count,
    size_t in_stride,
    size_t out_stride
);
```

Applies a complete view transform: scene-referred HDR → display-referred SDR.

**Parameters:**
- `display_rgb` — Output display-referred RGB [0, 1]
- `ctx` — Library context
- `transform` — View transform name (e.g., `"agx"`, `"aces_rrt_odt"`)
- `scene_rgb` — Input scene-referred linear RGB [0, ∞)
- `count` — Number of colors
- `in_stride` — Input stride in bytes
- `out_stride` — Output stride in bytes

---

### Supported View Transforms

#### AgX

```c
alwan_view_transform_apply(ctx, "agx", ...);
```

**Characteristics:**
- **Input:** Scene-referred linear RGB (any primaries)
- **Output:** Display sRGB [0, 1]
- **Dynamic range:** Unlimited (logarithmic compression)
- **Hue:** Excellent hue preservation

**Use case:**
- Modern game engines (Blender 4.0+)
- Real-time rendering
- Alternative to ACES for games

**Example:**
```c
// HDR scene to SDR display (output first)
alwan_vec3 hdr_scene[1920 * 1080];  // [0, ∞)
alwan_vec3 sdr_display[1920 * 1080]; // [0, 1]

alwan_view_transform_apply(sdr_display, ctx, "agx",
                           hdr_scene, 1920 * 1080, 12, 12);

// Ready for display encoding (output first)
alwan_oetf_apply(sdr_display, ctx, "srgb",
                 sdr_display, 1920 * 1080, 12, 12);
```

---

#### ACES RRT+ODT

```c
alwan_view_transform_apply(ctx, "aces_rrt_odt_rec709", ...);
alwan_view_transform_apply(ctx, "aces_rrt_odt_p3", ...);
alwan_view_transform_apply(ctx, "aces_rrt_odt_rec2020", ...);
```

**Characteristics:**
- **Input:** ACES2065-1 (AP0) scene-referred
- **Output:** Display-referred for specific display
- **Dynamic range:** ~20 stops compressed to display range

**Use case:**
- Film and TV production
- VFX pipelines
- Standardized color management

---

## Usage Patterns

### Pattern 1: Load and Process sRGB Image

```c
// 1. Load encoded sRGB
float *encoded = load_png("image.png");  // [0, 1]

// 2. Decode to linear (output first)
float *linear = malloc(...);
alwan_eotf_apply(linear, ctx, "srgb", encoded, count, 12, 12);

// 3. Process in linear space
apply_color_grading(linear, count);

// 4. Encode back to sRGB (output first)
alwan_oetf_apply(encoded, ctx, "srgb", linear, count, 12, 12);

// 5. Save
save_png("output.png", encoded);
```

---

### Pattern 2: HDR to SDR Tone Mapping

```c
// HDR scene (e.g., from ray tracer)
float *hdr_linear = render_scene();  // [0, ∞)

// Apply view transform (output first)
float *sdr_linear = malloc(...);
alwan_view_transform_apply(sdr_linear, ctx, "agx",
                           hdr_linear, count, 12, 12);

// Encode for display (output first)
float *sdr_encoded = malloc(...);
alwan_oetf_apply(sdr_encoded, ctx, "srgb",
                 sdr_linear, count, 12, 12);
```

---

### Pattern 3: Camera Log to Display

```c
// Load camera footage (log encoded)
float *clog = load_video("footage.mp4");

// Decode to linear (output first)
float *linear = malloc(...);
alwan_eotf_apply(linear, ctx, "clog", clog, count, 12, 12);

// Color grade in linear space
apply_lut(linear, count);

// Encode to display (sRGB) (output first)
float *srgb = malloc(...);
alwan_oetf_apply(srgb, ctx, "srgb", linear, count, 12, 12);
```

---

## Transfer Function Properties

| Function | Gamma | Range (nits) | Perceptual | Use Case |
|----------|-------|-------------|------------|----------|
| **sRGB** | ~2.2 | 0-80 | No | Web, displays |
| **BT.1886** | 2.4 | 0-100 | No | Broadcast reference |
| **PQ** | Varies | 0-10,000 | Yes | HDR content |
| **HLG** | Hybrid | 0-1,000 | Yes | HDR broadcast |
| **S-Log3** | Log | Wide | No | Cinema capture |
| **LogC3** | Log | Wide | No | Cinema capture |
| **AgX** | Custom | Unlimited | Yes | Game rendering |
| **ACES** | Complex | ~20 stops | Yes | Film production |

---

## Input/Output Ranges

### EOTF (Decoding)

| Function | Input (Encoded) | Output (Linear) |
|----------|----------------|-----------------|
| sRGB | [0, 1] | [0, 1] |
| PQ | [0, 1] | [0, 10000] cd/m² |
| HLG | [0, 1] | [0, 1000] cd/m² |
| S-Log3 | [0, 1] | [0, ∞) scene |
| LogC3 | [0, 1] | [0, ∞) scene |

---

### OETF (Encoding)

| Function | Input (Linear) | Output (Encoded) |
|----------|---------------|------------------|
| sRGB | [0, 1] | [0, 1] |
| PQ | [0, 10000] cd/m² | [0, 1] |
| HLG | [0, 1000] cd/m² | [0, 1] |
| S-Log3 | [0, ∞) scene | [0, 1] |

**Note:** Out-of-range inputs are clamped.

---

## Precision Considerations

### Banding Prevention

Transfer functions can introduce visible banding in gradients:

**8-bit:** Visible banding with linear encoding
**10-bit:** Adequate for most SDR content
**12-bit:** Good for HDR (PQ, HLG)
**16-bit (half-float):** Excellent for linear HDR

**Recommendation:**
- Use 10+ bits for encoded SDR
- Use 12+ bits for encoded HDR
- Use 16-bit float for linear HDR

---

### Round-Trip Accuracy

Encoding then decoding should recover original (within precision):

```c
alwan_vec3 original = {0.5, 0.3, 0.2};
alwan_vec3 encoded, decoded;

alwan_oetf_apply(&encoded, ctx, "srgb", &original, 1, 0, 0);
alwan_eotf_apply(&decoded, ctx, "srgb", &encoded, 1, 0, 0);

// |original - decoded| < 1e-6 (float) or 1e-12 (double)
```

---

## Performance

### Relative Cost

| Function | Relative Time |
|----------|--------------|
| Linear (identity) | 1.0× |
| Pure power (gamma 2.2) | 1.1× |
| sRGB | 1.2× |
| PQ | 1.5× |
| HLG | 1.4× |
| Log curves | 1.3× |
| AgX | 2.0× |

**Note:** All functions are highly optimized and suitable for real-time use.

---

## Error Codes

- `ALWAN_SUCCESS` — Operation successful
- `ALWAN_ERROR_INVALID_PARAMETER` — NULL pointer or invalid count
- `ALWAN_ERROR_NOT_FOUND` — Transfer function name not recognized
- `ALWAN_ERROR_UNSUPPORTED` — Function not available in this build

---

## See Also

- [Color Spaces](color-spaces.md) — RGB conversions
- [Examples](../examples.md) — Usage examples
- [Precision & Limits](../precision-and-limits.md) — Numerical accuracy
- [Configuration](../configuration.md) — Build options
