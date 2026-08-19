# Color Vision Deficiency (CVD) Simulation API

Functions for simulating color blindness to test accessibility of visual content.

---

## Overview

CVD simulation transforms colors to approximate how they appear to individuals with
color vision deficiencies. Alwan implements two complementary models, selectable per call:

| Model | Enum | Method | Best for |
|-------|------|--------|----------|
| **Brettel, Viénot & Mollon (1997)** | `ALWAN_CVD_MODEL_BRETTEL` | Confusion lines (projection onto the dichromat plane in LMS) | Full dichromacy |
| **Machado, Oliveira & Fernandes (2009)** | `ALWAN_CVD_MODEL_MACHADO` | Cone spectral-sensitivity shift, applied as a per-severity sRGB→sRGB 3×3 matrix | Anomalous (partial) trichromacy |

The Machado model uses precomputed matrices at **11 discrete severity levels**
(`severity = 0.0, 0.1, … 1.0`) and linearly interpolates between them for continuous
parameterization. The matrices are embedded via the gendata pipeline (`ALWAN_EMBED_DATA`).

**Types of color vision deficiency:**

| Type | Cone affected | Prevalence (male) | Prevalence (female) |
|------|--------------|-------------------|---------------------|
| Protanopia | L-cone absent (red-blind) | ~1% | ~0.01% |
| Deuteranopia | M-cone absent (green-blind) | ~1% | ~0.01% |
| Tritanopia | S-cone absent (blue-blind) | ~0.002% | ~0.002% |
| Protanomaly | L-cone deficient (red-weak) | ~1% | ~0.03% |
| Deuteranomaly | M-cone deficient (green-weak) | ~5% | ~0.35% |
| Tritanomaly | S-cone deficient (blue-weak) | rare | rare |

> **Precision suffixes.** Like the rest of the library, every CVD function exists in two
> native-precision flavors: `_f32` (operating on `alwan_f32` / `alwan_rgb_f32`) and `_f64`
> (`alwan_f64` / `alwan_rgb_f64`). Below, `_{T}` denotes either `f32` or `f64`; pick one and
> use it consistently for a given call. `ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64`
> restrict which definitions are compiled (default: both).

---

## Enums

### CVD Type

```c
typedef enum {
    ALWAN_CVD_PROTANOPIA = 0,     /* Red-blind (L-cone absent) */
    ALWAN_CVD_DEUTERANOPIA = 1,   /* Green-blind (M-cone absent) */
    ALWAN_CVD_TRITANOPIA = 2,     /* Blue-blind (S-cone absent) */
    ALWAN_CVD_PROTANOMALY = 3,    /* Red-weak (L-cone deficient) */
    ALWAN_CVD_DEUTERANOMALY = 4,  /* Green-weak (M-cone deficient) */
    ALWAN_CVD_TRITANOMALY = 5     /* Blue-weak (S-cone deficient) */
} alwan_cvd_type;
```

### CVD Model

```c
typedef enum {
    ALWAN_CVD_MODEL_BRETTEL = 0,    /* Brettel, Vienot & Mollon 1997 (confusion lines) */
    ALWAN_CVD_MODEL_MACHADO = 1     /* Machado, Oliveira & Fernandes 2009 (cone shift) */
} alwan_cvd_model;
```

For Machado, `cvd_type` is folded onto its dimension: `PROTANOPIA`/`PROTANOMALY` → protan,
`DEUTERANOPIA`/`DEUTERANOMALY` → deutan, `TRITANOPIA`/`TRITANOMALY` → tritan.

---

## Single-Element Functions

### alwan_simulate_cvd_{T} (Brettel 1997)

```c
int alwan_simulate_cvd_f32(alwan_rgb_f32 *rgb_out,
                           alwan_rgb_f32 const *rgb_in,
                           alwan_cvd_type cvd_type,
                           alwan_f32 severity);

int alwan_simulate_cvd_f64(alwan_rgb_f64 *rgb_out,
                           alwan_rgb_f64 const *rgb_in,
                           alwan_cvd_type cvd_type,
                           alwan_f64 severity);
```

Simulate color vision deficiency for a single linear-RGB color using the Brettel/Viénot/Mollon
confusion-line model.

**Parameters:**
- `rgb_out`: Output simulated RGB color as seen by a person with CVD
- `rgb_in`: Input linear RGB color [0, 1]
- `cvd_type`: Type of color vision deficiency
- `severity`: Severity [0, 1] (1.0 = complete, 0.0 = normal vision). Applies to the anomalous
  trichromacy types (protanomaly, deuteranomaly, tritanomaly); the dichromacy types
  (protanopia, deuteranopia, tritanopia) always simulate full severity.

**Returns:** `ALWAN_OK` on success, `ALWAN_E_INVALID` on error.

### alwan_simulate_cvd_machado_{T} (Machado 2009)

```c
int alwan_simulate_cvd_machado_f32(alwan_rgb_f32 *rgb_out,
                                   alwan_rgb_f32 const *rgb_in,
                                   alwan_cvd_type cvd_type,
                                   alwan_f32 severity);

int alwan_simulate_cvd_machado_f64(alwan_rgb_f64 *rgb_out,
                                   alwan_rgb_f64 const *rgb_in,
                                   alwan_cvd_type cvd_type,
                                   alwan_f64 severity);
```

Cone-shift model. `severity` in [0, 1] indexes/interpolates the 11 precomputed sRGB→sRGB
matrices, where 0 = normal vision and 1 = full dichromacy.

### alwan_simulate_cvd_ex_{T} (model-selectable)

```c
int alwan_simulate_cvd_ex_f32(alwan_rgb_f32 *rgb_out,
                              alwan_rgb_f32 const *rgb_in,
                              alwan_cvd_type cvd_type,
                              alwan_f32 severity,
                              alwan_cvd_model model);

int alwan_simulate_cvd_ex_f64(alwan_rgb_f64 *rgb_out,
                              alwan_rgb_f64 const *rgb_in,
                              alwan_cvd_type cvd_type,
                              alwan_f64 severity,
                              alwan_cvd_model model);
```

Dispatches to Brettel or Machado according to `model`.

**Example:**
```c
alwan_rgb_f64 red = {1.0, 0.0, 0.0};
alwan_rgb_f64 simulated;

/* How does pure red look to someone with deuteranopia? (Brettel) */
alwan_simulate_cvd_f64(&simulated, &red, ALWAN_CVD_DEUTERANOPIA, 1.0);

/* Partial red-weakness (50% severity) via the Machado cone-shift model */
alwan_simulate_cvd_machado_f64(&simulated, &red, ALWAN_CVD_PROTANOMALY, 0.5);

/* Same call, model chosen at runtime */
alwan_simulate_cvd_ex_f64(&simulated, &red, ALWAN_CVD_PROTANOMALY, 0.5,
                          ALWAN_CVD_MODEL_MACHADO);
```

---

## Batch Functions (interleaved / AoS)

Batch maps follow the library-wide convention: **output before input, and each `*_stride`
immediately follows its buffer** (memcpy order), then `count`, then the knobs.

### alwan_simulate_cvd_{T}_map_interleave (Brettel)

```c
int alwan_simulate_cvd_f32_map_interleave(
    alwan_f32 *rgb_out, size_t out_stride,
    alwan_f32 const *rgb_in, size_t in_stride,
    size_t count, alwan_cvd_type cvd_type, alwan_f32 severity);

int alwan_simulate_cvd_f64_map_interleave(
    alwan_f64 *rgb_out, size_t out_stride,
    alwan_f64 const *rgb_in, size_t in_stride,
    size_t count, alwan_cvd_type cvd_type, alwan_f64 severity);
```

### Type-specific Brettel batch functions

```c
int alwan_simulate_protanopia_{T}_map_interleave(
    alwan_{T} *rgb_out, size_t out_stride,
    alwan_{T} const *rgb_in, size_t in_stride,
    size_t count, alwan_{T} severity);

int alwan_simulate_deuteranopia_{T}_map_interleave(
    alwan_{T} *rgb_out, size_t out_stride,
    alwan_{T} const *rgb_in, size_t in_stride,
    size_t count, alwan_{T} severity);

int alwan_simulate_tritanopia_{T}_map_interleave(
    alwan_{T} *rgb_out, size_t out_stride,
    alwan_{T} const *rgb_in, size_t in_stride,
    size_t count, alwan_{T} severity);
```

### alwan_simulate_cvd_machado_{T}_map_interleave (Machado)

```c
int alwan_simulate_cvd_machado_f32_map_interleave(
    alwan_f32 *rgb_out, size_t out_stride,
    alwan_f32 const *rgb_in, size_t in_stride,
    size_t count, alwan_cvd_type cvd_type, alwan_f32 severity);

int alwan_simulate_cvd_machado_f64_map_interleave(
    alwan_f64 *rgb_out, size_t out_stride,
    alwan_f64 const *rgb_in, size_t in_stride,
    size_t count, alwan_cvd_type cvd_type, alwan_f64 severity);
```

---

## Typed Batch Functions (`_ex`)

Accept `void*` buffers with explicit pixel formats for mixed-precision pipelines. Both
formats may be any of `ALWAN_PIXEL_U8`/`U16`/`F16`/`F32`/`F64`. Note that `out_fmt`/`in_fmt`
come **after** `count` (extras tail order), while `severity` is `alwan_f64` in every `_ex`
entry point.

```c
int alwan_simulate_cvd_map_interleave_ex(
    void *out, size_t out_stride,
    void const *in, size_t in_stride,
    size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
    alwan_cvd_type cvd_type, alwan_f64 severity);

int alwan_simulate_protanopia_map_interleave_ex(
    void *out, size_t out_stride, void const *in, size_t in_stride,
    size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
    alwan_f64 severity);
/* deuteranopia / tritanopia _ex variants follow the same signature */

int alwan_simulate_cvd_machado_map_interleave_ex(
    void *rgb_out, size_t out_stride,
    void const *rgb_in, size_t in_stride,
    size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
    alwan_cvd_type cvd_type, alwan_f64 severity);
```

---

## Planar Layout Functions

For image data stored as separate R, G, B channel buffers. A single `out_stride` /
`in_stride` applies to all three channels of that side.

```c
int alwan_simulate_cvd_{T}_map_planar(
    alwan_{T} *o0, size_t out_stride, alwan_{T} *o1, alwan_{T} *o2,
    alwan_{T} const *i0, size_t in_stride, alwan_{T} const *i1, alwan_{T} const *i2,
    size_t count, alwan_cvd_type cvd_type, alwan_{T} severity);

int alwan_simulate_cvd_machado_{T}_map_planar(
    alwan_{T} *out_r, size_t out_stride, alwan_{T} *out_g, alwan_{T} *out_b,
    alwan_{T} const *in_r, size_t in_stride, alwan_{T} const *in_g, alwan_{T} const *in_b,
    size_t count, alwan_cvd_type cvd_type, alwan_{T} severity);
```

Typed planar `_ex` variants (`void*` + formats) are also available:

```c
int alwan_simulate_cvd_map_planar_ex(
    void *out0, size_t out_stride, void *out1, void *out2,
    void const *in0, size_t in_stride, void const *in1, void const *in2,
    size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
    alwan_cvd_type cvd_type, alwan_f64 severity);

int alwan_simulate_cvd_machado_map_planar_ex(
    void *out0, size_t out_stride, void *out1, void *out2,
    void const *in0, size_t in_stride, void const *in1, void const *in2,
    size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
    alwan_cvd_type cvd_type, alwan_f64 severity);
```

---

## Usage Examples

### Accessibility check for UI colors

```c
/* Check if two UI colors stay distinguishable under each dichromacy type */
alwan_rgb_f64 color_a = {0.2, 0.8, 0.3};  /* Green button  */
alwan_rgb_f64 color_b = {0.8, 0.2, 0.2};  /* Red warning   */

alwan_cvd_type types[] = {
    ALWAN_CVD_PROTANOPIA, ALWAN_CVD_DEUTERANOPIA, ALWAN_CVD_TRITANOPIA
};
const char *names[] = {"Protanopia", "Deuteranopia", "Tritanopia"};

for (int i = 0; i < 3; i++) {
    alwan_rgb_f64 sim_a, sim_b;
    alwan_simulate_cvd_f64(&sim_a, &color_a, types[i], 1.0);
    alwan_simulate_cvd_f64(&sim_b, &color_b, types[i], 1.0);

    /* Convert to Lab and check dE2000 */
    alwan_lab_f64 lab_a, lab_b;
    alwan_srgb_to_lab_f64(&lab_a, &sim_a);
    alwan_srgb_to_lab_f64(&lab_b, &sim_b);

    alwan_f64 de = alwan_delta_e_2000_f64(&lab_a, &lab_b);
    printf("%s: dE2000 = %.1f %s\n", names[i], de,
           de < 3.0 ? "FAIL" : "OK");
}
```

### Full-image simulation

```c
/* Simulate deuteranopia on an entire interleaved f64 RGB buffer */
alwan_simulate_deuteranopia_f64_map_interleave(
    (alwan_f64*)pixels_out, 3 * sizeof(alwan_f64),   /* out + stride */
    (alwan_f64 const*)pixels_in, 3 * sizeof(alwan_f64), /* in + stride */
    (size_t)width * height,                          /* pixel count  */
    1.0);                                            /* full severity */
```

---

## Error Codes

- `ALWAN_OK` (0): Success
- `ALWAN_E_INVALID` (-1): NULL pointer or invalid CVD type / pixel format

---

## See Also

- [Color Spaces](color-spaces.md): RGB/Lab conversions
- [Color Difference](color-difference.md): Measuring perceptual difference
- [Vision Science](vision.md): Visual perception models
- [Map / Batch API](../map.md): Stride-adjacent batch convention and `_ex` dispatch
