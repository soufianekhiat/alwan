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
void alwan_hlg_ootf_{T}(alwan_rgb_{T} *out, alwan_rgb_{T} const *in,
                        alwan_{T} Lw, alwan_{T} gamma_sys);

void alwan_hlg_ootf_inv_{T}(alwan_rgb_{T} *out, alwan_rgb_{T} const *in,
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
                     alwan_{T} const *rgb_in,
                     size_t stride, size_t count);

int alwan_maxfall_{T}(alwan_{T} *maxfall_out,
                      alwan_{T} const *rgb_in,
                      size_t stride, size_t count);
```

- **MaxCLL**: Maximum Content Light Level -- max(R,G,B) across all pixels.
- **MaxFALL**: Maximum Frame Average Light Level -- average of per-pixel max(R,G,B).

Both expect `rgb_in` as interleaved linear-light RGB in nits. `stride` is the
per-pixel byte stride (memcpy convention -- it follows the buffer it describes)
and `count` is the number of pixels.

**Example:**
```c
alwan_{T} maxcll, maxfall;
alwan_maxcll_{T}(&maxcll,  pixels, 3 * sizeof(double), width * height);
alwan_maxfall_{T}(&maxfall, pixels, 3 * sizeof(double), width * height);
printf("MaxCLL: %.1f nits, MaxFALL: %.1f nits\n", maxcll, maxfall);
```

---

## Arbitrary Gamma

### alwan_gamma_oetf_{T} / alwan_gamma_eotf_{T}

```c
int alwan_gamma_oetf_{T}(alwan_{T} *out, size_t out_stride,
                          alwan_{T} const *in, size_t in_stride,
                          size_t count, alwan_{T} gamma);

int alwan_gamma_eotf_{T}(alwan_{T} *out, size_t out_stride,
                          alwan_{T} const *in, size_t in_stride,
                          size_t count, alwan_{T} gamma);
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

## Accessibility Contrast

### alwan_wcag_contrast_ratio_{T}

```c
int alwan_wcag_contrast_ratio_{T}(alwan_{T} *result, alwan_{T} Y1, alwan_{T} Y2);
```

WCAG 2.x: `(L_lighter + 0.05) / (L_darker + 0.05)`. `Y1` and `Y2` are **relative
luminances** in `[0, 1]`, not sRGB code values. The result runs `[1, 21]`, and
the order of the two arguments does not matter. The published thresholds are
`>= 4.5` for AA and `>= 7.0` for AAA on body text.

### alwan_apca_contrast_{T}

```c
int alwan_apca_contrast_{T}(alwan_{T} *Lc_out,
                             alwan_rgb_{T} const *srgb_text,
                             alwan_rgb_{T} const *srgb_bg);
```

APCA / SAPC, the algorithm drafted for WCAG 3.0 (Myndex APCA-W3). It takes
**sRGB-encoded** colours, `[0, 1]` per channel, and does its own decoding.

Three things differ from WCAG and all three bite:

- **The arguments are ordered.** Text first, background second. Swapping them
  does not give the same number: APCA uses one pair of exponents when the
  background is lighter than the text and a different pair when it is darker, so
  polarity is built into the result rather than removed from it.
- **The result is signed and scaled by 100.** Positive means dark text on a light
  background, negative means light on dark, and the contrast level is the
  **magnitude** on APCA's `Lc` scale, not a ratio. Take `fabs` before comparing
  against a threshold, and keep the sign if you care which way round the pair is.
- **Very low contrast returns exactly zero.** Below APCA's clip the result is
  snapped to `0` rather than reported as a small number, so `Lc == 0` means "under
  the floor", not "identical colours".

It decodes with a plain `2.4` power on each channel, which is APCA's own
definition and not the piecewise sRGB EOTF. Do not linearise the colours
yourself before the call.

> **These two are not interchangeable and do not share an input.** WCAG takes
> luminance, APCA takes encoded sRGB. Feeding sRGB code values to
> `alwan_wcag_contrast_ratio_{T}` returns a number in the right range that is
> simply wrong, with nothing to signal it. Convert with the sRGB EOTF and a
> luminance weighting first; see [color-spaces.md](color-spaces.md).

---

## HDR Tone Mapping

### alwan_bt2390_eetf_{T} / alwan_bt2390_eetf_luminance_{T}

```c
int alwan_bt2390_eetf_{T}(alwan_{T} *E_out, alwan_{T} E_pq,
                           alwan_{T} LB, alwan_{T} LW,
                           alwan_{T} LB_target, alwan_{T} LW_target);

int alwan_bt2390_eetf_luminance_{T}(alwan_{T} *E_out, alwan_{T} E_pq,
                                     alwan_{T} L_source_peak,
                                     alwan_{T} L_target_peak);
```

The BT.2390 electro-electrical transfer function: a Hermite spline roll-off that
maps a source PQ range onto a display's, leaving everything below the knee alone.
This is the standard way to show 4000-nit content on a 1000-nit panel.

Everything is in the **PQ domain**, including the levels: `E_pq`, `LB`, `LW`,
`LB_target` and `LW_target` are all PQ-encoded `[0, 1]`, not cd/m². The
`_luminance_` form is the convenience wrapper that takes the two peaks in cd/m²
and encodes them, which is what most callers want.

### alwan_bt2446b_forward_{T} / alwan_bt2446c_forward_{T}

```c
int alwan_bt2446b_forward_{T}(alwan_{T} *Y_hdr_out, alwan_{T} Y_sdr,
                               alwan_{T} L_hdr, alwan_{T} L_sdr);

int alwan_bt2446c_forward_{T}(alwan_{T} *Y_sdr_out, alwan_{T} Y_hdr,
                               alwan_{T} L_hdr, alwan_{T} L_sdr);
```

The two BT.2446 methods run in **opposite directions**, which the shared name
hides:

| | direction | input domain |
|---|---|---|
| Method B | SDR to HDR, parametric up-conversion | `Y_sdr` linear `[0, 1]` |
| Method C | HDR to SDR, quantization-aware tone mapping | `Y_hdr` **PQ-encoded** `[0, 1]` |

Both take the same `L_hdr` and `L_sdr` peak luminances in cd/m², so a call with
the arguments in the wrong place compiles and returns a plausible number. Method
C's input being PQ-encoded while Method B's is linear is the second trap.

### alwan_exposure_tonemap_{T} / alwan_reinhard_calibrated_{T}

```c
int alwan_exposure_tonemap_{T}(alwan_{T} *out, alwan_{T} L, alwan_{T} exposure);

int alwan_reinhard_calibrated_{T}(alwan_{T} *out, alwan_{T} L,
                                   alwan_{T} key, alwan_{T} L_avg,
                                   alwan_{T} L_white);
```

- **Exposure**: `1 - exp(-2^exposure * L)`. `exposure` is an EV offset, `0`
  neutral and `+1` one stop brighter.
- **Reinhard calibrated**: the key-based form with a white point. `key` is the
  exposure key, `0.18` for standard 18% grey; `L_avg` is the scene's log-average
  luminance, which the caller computes over the frame; `L_white` is the smallest
  luminance that maps to pure white.

These are the classic per-pixel operators and they work on a scalar luminance.
For picture formation that keeps hue and carrier order rather than tone-mapping a
luminance, see [gamut.md](gamut.md).

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
