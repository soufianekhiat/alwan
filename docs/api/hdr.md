# HDR Pipeline Utilities API

Functions for HDR (High Dynamic Range) workflows including HLG OOTF, metadata computation, and display transforms.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

---

## Overview

HDR pipeline utilities support:

- **HLG OOTF** -- Scene-to-display transform per BT.2100-2
- **Rec.2100 Surround** -- Surround luminance compensation
- **MaxCLL / MaxFALL** -- HDR10 static metadata computation
- **Arbitrary Gamma** -- Custom gamma encoding/decoding
- **Contrast Metrics** -- Weber and Michelson contrast

---

## HLG OOTF

### alwan_hlg_ootf_{T} / alwan_hlg_ootf_inv_{T}

```c
int alwan_hlg_ootf_{T}(alwan_rgb_{T} *out, alwan_rgb_{T} const *in,
                        alwan_{T} Lw, alwan_{T} gamma_sys);

int alwan_hlg_ootf_inv_{T}(alwan_rgb_{T} *out, alwan_rgb_{T} const *in,
                            alwan_{T} Lw, alwan_{T} gamma_sys);
```

Apply (or invert) the HLG Opto-Optical Transfer Function (scene-to-display).
Converts scene-linear light to display-linear light per BT.2100-2.

**Parameters:**
- `in` -- Scene-referred linear RGB
- `Lw` -- Nominal peak display luminance in cd/m^2 (default: 1000)
- `gamma_sys` -- System gamma (default: 1.2)

**Example:**
```c
alwan_rgb_{T} scene   = {0.18, 0.18, 0.18};
alwan_rgb_{T} display;
alwan_hlg_ootf_{T}(&display, &scene, 1000.0, 1.2);
```

---

## Rec.2100 Surround Adjustment

### alwan_rec2100_surround_{T}

```c
void alwan_rec2100_surround_{T}(alwan_rgb_{T} *rgb_out, alwan_rgb_{T} const *rgb_in,
                                 alwan_{T} gamma);
```

Apply Rec.2100 surround luminance compensation. Adjusts the rendering for different surround
conditions (dim vs dark viewing).

---

## HDR10 Metadata

### alwan_maxcll_{T} / alwan_maxfall_{T}

```c
int alwan_maxcll_{T}(alwan_{T} *maxcll_out,
                     alwan_{T} const *rgb_data,
                     size_t count, size_t stride);

int alwan_maxfall_{T}(alwan_{T} *maxfall_out,
                      alwan_{T} const *rgb_data,
                      size_t count, size_t stride);
```

- **MaxCLL**: Maximum Content Light Level -- max(R,G,B) across all pixels.
- **MaxFALL**: Maximum Frame Average Light Level -- average of per-pixel max(R,G,B).

Both expect `rgb_data` as interleaved linear-light RGB in nits.

**Example:**
```c
alwan_{T} maxcll, maxfall;
alwan_maxcll_{T}(&maxcll,  pixels, width * height, 3 * sizeof(double));
alwan_maxfall_{T}(&maxfall, pixels, width * height, 3 * sizeof(double));
printf("MaxCLL: %.1f nits, MaxFALL: %.1f nits\n", maxcll, maxfall);
```

---

## Arbitrary Gamma

### alwan_gamma_oetf_{T} / alwan_gamma_eotf_{T}

```c
int alwan_gamma_oetf_{T}(alwan_{T} *out, alwan_{T} const *in,
                          alwan_{T} gamma, size_t count,
                          size_t in_stride, size_t out_stride);

int alwan_gamma_eotf_{T}(alwan_{T} *out, alwan_{T} const *in,
                          alwan_{T} gamma, size_t count,
                          size_t in_stride, size_t out_stride);
```

Apply arbitrary gamma encoding/decoding:
- **OETF**: `out = pow(in, 1/gamma)` (linear to encoded)
- **EOTF**: `out = pow(in, gamma)` (encoded to linear)

Useful when the standard `alwan_transfer_function` enum does not cover your specific gamma value.

---

## Contrast Metrics

### alwan_weber_contrast_{T} / alwan_michelson_contrast_{T}

```c
int alwan_weber_contrast_{T}(alwan_{T} *result,
                              alwan_{T} L_target, alwan_{T} L_bg);

int alwan_michelson_contrast_{T}(alwan_{T} *result,
                                  alwan_{T} L_max, alwan_{T} L_min);
```

- **Weber**: `(L_target - L_background) / L_background` -- for small targets on uniform backgrounds.
- **Michelson**: `(L_max - L_min) / (L_max + L_min)` -- for periodic patterns (gratings).

---

## D-Series Illuminant from CCT

### alwan_d_series_illuminant_xy_{T}

```c
int alwan_d_series_illuminant_xy_{T}(alwan_vec2_{T} *xy_out, alwan_{T} cct);
```

Compute CIE D-series illuminant xy chromaticity from correlated color temperature (4000-25000K).

**Example:**
```c
alwan_vec2_{T} xy;
alwan_d_series_illuminant_xy_{T}(&xy, 6500.0);  /* D65 */
```

---

## Error Codes

- `ALWAN_OK` (0) -- Success
- `ALWAN_E_INVALID` (-1) -- NULL pointer or invalid parameter
- `ALWAN_E_DIVZERO` (-5) -- Division by zero in contrast calculation

---

## See Also

- [Transfer Functions](transfer-functions.md) -- PQ, HLG, standard OETF/EOTF
- [ACES Pipeline](aces.md) -- ACES output transforms
- [Color Spaces](color-spaces.md) -- Rec.2100, ICtCp conversions
