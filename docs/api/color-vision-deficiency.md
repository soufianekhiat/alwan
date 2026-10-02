# Color Vision Deficiency (CVD) Simulation API

Functions for simulating color blindness to test accessibility of visual content.

---

## Overview

CVD simulation transforms colors to approximate how they appear to individuals with
color vision deficiencies. Alwan implements three models, selectable per call:

| Model | Enum | Method | Best for |
|-------|------|--------|----------|
| **Brettel, Vienot & Mollon (1997)** | `ALWAN_CVD_MODEL_BRETTEL` | Projection along the missing cone's axis onto one of two half-planes in LMS | Full dichromacy, tritanopia included |
| **Machado, Oliveira & Fernandes (2009)** | `ALWAN_CVD_MODEL_MACHADO` | Cone spectral-sensitivity shift, applied as a per-severity sRGB->sRGB 3x3 matrix | Anomalous (partial) trichromacy |
| **Vienot, Brettel & Mollon (1999)** | `ALWAN_CVD_MODEL_VIENOT` | One plane per deficiency, so one linear-sRGB 3x3 matrix; severity mixes linearly | Protanopia and deuteranopia, cheaply |

Vienot 1999 is Brettel 1997's two half-planes collapsed into one plane through black, blue
and yellow (the paper's simplification for video), as DaltonLens-Python's
`Simulator_Vienot1999` builds it on the same Smith-Pokorny LMS model. The paper does not
cover tritanopia; the tritan matrix is DaltonLens's plane through red and cyan, which
DaltonLens flags as inaccurate, so use Brettel for tritans. Suite 82 holds all three
deficiencies at severities 1, 0.6 and 0.25 to DaltonLens (f64 to rounding).

The Machado model uses precomputed matrices at **11 discrete severity levels**
(`severity = 0.0, 0.1, ... 1.0`) and linearly interpolates between them for continuous
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
    ALWAN_CVD_MODEL_MACHADO = 1,    /* Machado, Oliveira & Fernandes 2009 (cone shift) */
    ALWAN_CVD_MODEL_VIENOT = 2      /* Vienot, Brettel & Mollon 1999 (one plane, one matrix) */
} alwan_cvd_model;
```

For Machado, `cvd_type` is folded onto its dimension: `PROTANOPIA`/`PROTANOMALY` -> protan,
`DEUTERANOPIA`/`DEUTERANOMALY` -> deutan, `TRITANOPIA`/`TRITANOMALY` -> tritan.

---

## Single-Element Functions

### alwan_simulate_cvd_{T} (Brettel 1997)

```c
alwan_status alwan_simulate_cvd_f32(alwan_rgb_f32 *rgb_out,
                                    alwan_rgb_f32 const *rgb_in,
                                    alwan_cvd_type cvd_type,
                                    alwan_f32 severity);

alwan_status alwan_simulate_cvd_f64(alwan_rgb_f64 *rgb_out,
                                    alwan_rgb_f64 const *rgb_in,
                                    alwan_cvd_type cvd_type,
                                    alwan_f64 severity);
```

Simulate color vision deficiency for a single linear-sRGB color by Brettel, Vienot and
Mollon (1997), as DaltonLens-Python's `Simulator_Brettel1997` does at its defaults, which
suite 261 holds it to within 2e-15:

- linear sRGB to LMS by the Smith and Pokorny (1975) cone fundamentals;
- the missing cone's coordinate replaced by the colour's projection onto one of two
  half-planes through the neutral axis (RGB white), each through one anchor wavelength:
  475 and 575 nm for protanopia and deuteranopia, 485 and 660 nm for tritanopia;
- the half-plane chosen by the side of a separation plane the colour falls on;
- back to linear sRGB, then mixed with the input by `severity`, linearly in linear sRGB.

Until 2026-09-25 this was one projection per deficiency, the widely copied "daltonize"
coefficients on a Hunt-Pointer-Estevez matrix: Vienot 1999's single-plane shortcut, which
does not hold for tritanopia. Outputs moved by up to about 1.0 (a component of a saturated
colour), a mean of about 0.2 in sRGB code values.

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
alwan_status alwan_simulate_cvd_machado_f32(alwan_rgb_f32 *rgb_out,
                                            alwan_rgb_f32 const *rgb_in,
                                            alwan_cvd_type cvd_type,
                                            alwan_f32 severity);

alwan_status alwan_simulate_cvd_machado_f64(alwan_rgb_f64 *rgb_out,
                                            alwan_rgb_f64 const *rgb_in,
                                            alwan_cvd_type cvd_type,
                                            alwan_f64 severity);
```

Cone-shift model. `severity` in [0, 1] indexes/interpolates the 11 precomputed sRGB->sRGB
matrices, where 0 = normal vision and 1 = full dichromacy.

### alwan_simulate_cvd_ex_{T} (model-selectable)

```c
alwan_status alwan_simulate_cvd_ex_f32(alwan_rgb_f32 *rgb_out,
                                       alwan_rgb_f32 const *rgb_in,
                                       alwan_cvd_type cvd_type,
                                       alwan_f32 severity,
                                       alwan_cvd_model model);

alwan_status alwan_simulate_cvd_ex_f64(alwan_rgb_f64 *rgb_out,
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
alwan_status alwan_simulate_cvd_f32_map_interleave(
             alwan_f32 *rgb_out, size_t out_stride,
             alwan_f32 const *rgb_in, size_t in_stride,
             size_t count, alwan_cvd_type cvd_type, alwan_f32 severity);

alwan_status alwan_simulate_cvd_f64_map_interleave(
             alwan_f64 *rgb_out, size_t out_stride,
             alwan_f64 const *rgb_in, size_t in_stride,
             size_t count, alwan_cvd_type cvd_type, alwan_f64 severity);
```

### Type-specific Brettel batch functions

```c
alwan_status alwan_simulate_protanopia_{T}_map_interleave(
             alwan_{T} *rgb_out, size_t out_stride,
             alwan_{T} const *rgb_in, size_t in_stride,
             size_t count, alwan_{T} severity);

alwan_status alwan_simulate_deuteranopia_{T}_map_interleave(
             alwan_{T} *rgb_out, size_t out_stride,
             alwan_{T} const *rgb_in, size_t in_stride,
             size_t count, alwan_{T} severity);

alwan_status alwan_simulate_tritanopia_{T}_map_interleave(
             alwan_{T} *rgb_out, size_t out_stride,
             alwan_{T} const *rgb_in, size_t in_stride,
             size_t count, alwan_{T} severity);
```

### alwan_simulate_cvd_machado_{T}_map_interleave (Machado)

```c
alwan_status alwan_simulate_cvd_machado_f32_map_interleave(
             alwan_f32 *rgb_out, size_t out_stride,
             alwan_f32 const *rgb_in, size_t in_stride,
             size_t count, alwan_cvd_type cvd_type, alwan_f32 severity);

alwan_status alwan_simulate_cvd_machado_f64_map_interleave(
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
alwan_status alwan_simulate_cvd_map_interleave_ex(
             void *out, size_t out_stride,
             void const *in, size_t in_stride,
             size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
             alwan_cvd_type cvd_type, alwan_f64 severity);

alwan_status alwan_simulate_protanopia_map_interleave_ex(
             void *out, size_t out_stride, void const *in, size_t in_stride,
             size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
             alwan_f64 severity);
/* deuteranopia / tritanopia _ex variants follow the same signature */

alwan_status alwan_simulate_cvd_machado_map_interleave_ex(
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
alwan_status alwan_simulate_cvd_{T}_map_planar(
             alwan_{T} *o0, size_t out_stride, alwan_{T} *o1, alwan_{T} *o2,
             alwan_{T} const *i0, size_t in_stride, alwan_{T} const *i1, alwan_{T} const *i2,
             size_t count, alwan_cvd_type cvd_type, alwan_{T} severity);

alwan_status alwan_simulate_cvd_machado_{T}_map_planar(
             alwan_{T} *out_r, size_t out_stride, alwan_{T} *out_g, alwan_{T} *out_b,
             alwan_{T} const *in_r, size_t in_stride, alwan_{T} const *in_g, alwan_{T} const *in_b,
             size_t count, alwan_cvd_type cvd_type, alwan_{T} severity);
```

Typed planar `_ex` variants (`void*` + formats) are also available:

```c
alwan_status alwan_simulate_cvd_map_planar_ex(
             void *out0, size_t out_stride, void *out1, void *out2,
             void const *in0, size_t in_stride, void const *in1, void const *in2,
             size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
             alwan_cvd_type cvd_type, alwan_f64 severity);

alwan_status alwan_simulate_cvd_machado_map_planar_ex(
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

---

## Machado 2009 as a continuous model

```c
typedef enum {
    ALWAN_DISPLAY_PRIMARIES_CRT_BRAINARD1997 = 0,
    ALWAN_DISPLAY_PRIMARIES_APPLE_STUDIO = 1
} alwan_display_primaries;

alwan_status alwan_display_primaries_spd_{T}(alwan_spd_{T} *r, alwan_spd_{T} *g, alwan_spd_{T} *b,
                                             alwan_display_primaries which, alwan_ctx *ctx);

alwan_status alwan_cvd_matrix_machado2009_shift_{T}(alwan_mat3x3_{T} *out,
                                                    alwan_{T} shift_l, alwan_{T} shift_m,
                                                    alwan_{T} shift_s,
                                                    alwan_spd_{T} const *primary_r,
                                                    alwan_spd_{T} const *primary_g,
                                                    alwan_spd_{T} const *primary_b,
                                                    alwan_ctx *ctx);
```

`alwan_simulate_cvd_machado_{T}` above interpolates between the eleven matrices the paper
tabulates. Those are right for those eleven severities **on the display the authors used**, a
1997 CRT, and they cannot answer for a different display. This derives the matrix instead.

The model shifts the Stockman and Sharpe cone fundamentals, projects both the normal and the
shifted ones onto the paper's opponent axes, integrates each against each display primary's
spectrum, and takes `inv(normal) * shifted`. The shifts are in **nanometres**, not a severity
in `[0, 1]`: the paper's protanomaly and deuteranomaly run 0 to 20, and its tritanomaly tables
use 5 to 59, with the authors saying plainly that the shift paradigm is an approximation there
rather than a model of tritanopia.

**The display changes the answer, which is the whole reason this exists.** Full protanopia on
the 1997 CRT and on an Apple Studio Display differ by 0.40 in a matrix coefficient. Simulating
deficiency for a modern panel with a 1997 CRT's matrices is simulating it for the wrong
display.

Pass `NULL` for all three primaries to get that CRT, which is embedded at 1 nm and reproduces
colour-science's answer to 1e-14. Pass measured spectra to get your own display.

**One caveat about measured primaries.** The published spectra are at 5 nm and the reference
resamples them with Sprague interpolation. Resampling linearly instead moves the derived matrix
by up to 1.8e-02, which is a thousand times the agreement otherwise available, so the built-in
sets are embedded already resampled rather than interpolated at run time. A caller passing
measured spectra gets alwan's own linear resampling between samples, and should expect their
own numbers rather than another library's. Supplying spectra already on a 1 nm grid avoids the
question entirely.

A related trap, recorded because it cost a cycle: extrapolating those spectra by **holding the
last measured value** rather than by zero leaves a display primary emitting 0.0066 from 781 nm
to 830 nm, and that tail alone moves the derived matrix by 7e-06. A display emits nothing past
the end of its measurement.
