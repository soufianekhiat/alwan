# HDR Pipeline Utilities API

Functions for HDR (High Dynamic Range) workflows including HLG OOTF, metadata computation, and display transforms.

---

## Overview

HDR pipeline utilities support:

- **HLG OOTF** — Scene-to-display transform per BT.2100-2
- **Rec.2100 Surround** — Surround luminance compensation
- **MaxCLL / MaxFALL** — HDR10 static metadata computation
- **Arbitrary Gamma** — Custom gamma encoding/decoding
- **Contrast Metrics** — Weber and Michelson contrast

---

## HLG OOTF

### alwan_hlg_ootf

```c
int alwan_hlg_ootf(alwan_rgb *out, alwan_rgb const *in,
                   alwan_scalar Lw, alwan_scalar gamma_sys);
```

Apply HLG Opto-Optical Transfer Function (scene-to-display). Converts scene-linear light to display-linear light per BT.2100-2.

**Parameters:**
- `in` — Scene-referred linear RGB
- `Lw` — Nominal peak display luminance in cd/m^2 (default: 1000)
- `gamma_sys` — System gamma (default: 1.2)

### alwan_hlg_ootf_inv

```c
int alwan_hlg_ootf_inv(alwan_rgb *out, alwan_rgb const *in,
                       alwan_scalar Lw, alwan_scalar gamma_sys);
```

Inverse OOTF (display-to-scene).

**Example:**
```c
alwan_rgb scene = {0.18, 0.18, 0.18};
alwan_rgb display;
alwan_hlg_ootf(&display, &scene, 1000.0, 1.2);
```

---

## Rec.2100 Surround Adjustment

### alwan_rec2100_surround

```c
int alwan_rec2100_surround(alwan_rgb *rgb_out, alwan_rgb const *rgb_in,
                           alwan_scalar gamma);
```

Apply Rec.2100 surround luminance compensation. Adjusts the rendering for different surround conditions (dim vs dark viewing).

---

## HDR10 Metadata

### alwan_maxcll

```c
int alwan_maxcll(alwan_scalar *maxcll_out,
                 alwan_scalar const *rgb_data,
                 size_t count, size_t stride);
```

Compute Maximum Content Light Level — the maximum value of max(R, G, B) across all pixels. Used for HDR10 static metadata.

**Parameters:**
- `rgb_data` — Interleaved RGB pixel data (linear light, in nits)
- `count` — Number of pixels
- `stride` — Stride between pixels in bytes

### alwan_maxfall

```c
int alwan_maxfall(alwan_scalar *maxfall_out,
                  alwan_scalar const *rgb_data,
                  size_t count, size_t stride);
```

Compute Maximum Frame Average Light Level — the average of per-pixel max(R, G, B). Used for HDR10 static metadata.

**Example:**
```c
alwan_scalar maxcll, maxfall;
alwan_maxcll(&maxcll, pixels, width * height, 3 * sizeof(alwan_scalar));
alwan_maxfall(&maxfall, pixels, width * height, 3 * sizeof(alwan_scalar));
printf("MaxCLL: %.1f nits, MaxFALL: %.1f nits\n", maxcll, maxfall);
```

---

## Arbitrary Gamma

### alwan_gamma_oetf / alwan_gamma_eotf

```c
int alwan_gamma_oetf(alwan_scalar *out, alwan_scalar const *in,
                     alwan_scalar gamma, size_t count,
                     size_t in_stride, size_t out_stride);

int alwan_gamma_eotf(alwan_scalar *out, alwan_scalar const *in,
                     alwan_scalar gamma, size_t count,
                     size_t in_stride, size_t out_stride);
```

Apply arbitrary gamma encoding/decoding:
- **OETF**: `out = pow(in, 1/gamma)` (linear to encoded)
- **EOTF**: `out = pow(in, gamma)` (encoded to linear)

Useful when the standard `alwan_transfer_function` enum doesn't cover your specific gamma value.

---

## Contrast Metrics

### alwan_weber_contrast

```c
int alwan_weber_contrast(alwan_scalar *result,
                         alwan_scalar L_target, alwan_scalar L_bg);
```

Weber contrast: `(L_target - L_background) / L_background`. Used for small targets on uniform backgrounds.

### alwan_michelson_contrast

```c
int alwan_michelson_contrast(alwan_scalar *result,
                             alwan_scalar L_max, alwan_scalar L_min);
```

Michelson contrast: `(L_max - L_min) / (L_max + L_min)`. Used for periodic patterns (gratings).

---

## D-Series Illuminant from CCT

### alwan_d_series_illuminant_xy

```c
int alwan_d_series_illuminant_xy(alwan_vec2 *xy_out, alwan_scalar cct);
```

Compute CIE D-series illuminant xy chromaticity from correlated color temperature (4000-25000K).

**Example:**
```c
alwan_vec2 xy;
alwan_d_series_illuminant_xy(&xy, 6500.0);  /* D65 */
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — NULL pointer or invalid parameter
- `ALWAN_E_DIVZERO` (-5) — Division by zero in contrast calculation

---

## See Also

- [Transfer Functions](transfer-functions.md) — PQ, HLG, standard OETF/EOTF
- [ACES Pipeline](aces.md) — ACES output transforms
- [Color Spaces](color-spaces.md) — Rec.2100, ICtCp conversions
