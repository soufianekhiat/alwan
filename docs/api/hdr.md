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
alwan_status alwan_maxcll_{T}(alwan_{T} *maxcll_out,
                              alwan_{T} const *rgb_in,
                              size_t stride, size_t count);

alwan_status alwan_maxfall_{T}(alwan_{T} *maxfall_out,
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
alwan_status alwan_gamma_oetf_{T}(alwan_{T} *out, size_t out_stride,
                                   alwan_{T} const *in, size_t in_stride,
                                   size_t count, alwan_{T} gamma);

alwan_status alwan_gamma_eotf_{T}(alwan_{T} *out, size_t out_stride,
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
alwan_status alwan_weber_contrast_{T}(alwan_{T} *result,
                                       alwan_{T} L_target, alwan_{T} L_bg);

alwan_status alwan_michelson_contrast_{T}(alwan_{T} *result,
                                           alwan_{T} L_max, alwan_{T} L_min);
```

- **Weber**: `(L_target - L_background) / L_background` -- for small targets on uniform backgrounds.
- **Michelson**: `(L_max - L_min) / (L_max + L_min)` -- for periodic patterns (gratings).

---

## Accessibility Contrast

### alwan_wcag_contrast_ratio_{T}

```c
alwan_status alwan_wcag_contrast_ratio_{T}(alwan_{T} *result, alwan_{T} Y1, alwan_{T} Y2);
```

WCAG 2.x: `(L_lighter + 0.05) / (L_darker + 0.05)`. `Y1` and `Y2` are **relative
luminances** in `[0, 1]`, not sRGB code values. The result runs `[1, 21]`, and
the order of the two arguments does not matter. The published thresholds are
`>= 4.5` for AA and `>= 7.0` for AAA on body text.

### alwan_apca_contrast_{T}

```c
alwan_status alwan_apca_contrast_{T}(alwan_{T} *Lc_out,
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
alwan_status alwan_bt2390_eetf_{T}(alwan_{T} *E_out, alwan_{T} E_pq,
                                    alwan_{T} LB, alwan_{T} LW,
                                    alwan_{T} LB_target, alwan_{T} LW_target);

alwan_status alwan_bt2390_eetf_luminance_{T}(alwan_{T} *E_out, alwan_{T} E_pq,
                                              alwan_{T} L_source_peak,
                                              alwan_{T} L_target_peak);
```

The BT.2390 electro-electrical transfer function: a Hermite spline roll-off that
maps a source PQ range onto a display's, leaving everything below the knee alone.
This is the standard way to show 4000-nit content on a 1000-nit panel.

Everything is in the **PQ domain**, including the levels: `E_pq`, `LB`, `LW`,
`LB_target` and `LW_target` are all PQ-encoded `[0, 1]`, not cd/m2. The
`_luminance_` form is the convenience wrapper that takes the two peaks in cd/m2
and encodes them, which is what most callers want.

### alwan_bt2446b_forward_{T} / alwan_bt2446c_forward_{T}

```c
alwan_status alwan_bt2446b_forward_{T}(alwan_{T} *Y_hdr_out, alwan_{T} Y_sdr,
                                        alwan_{T} L_hdr, alwan_{T} L_sdr);

alwan_status alwan_bt2446c_forward_{T}(alwan_{T} *Y_sdr_out, alwan_{T} Y_hdr,
                                        alwan_{T} L_hdr, alwan_{T} L_sdr);
```

The two BT.2446 methods run in **opposite directions**, which the shared name
hides:

| | direction | input domain |
|---|---|---|
| Method B | SDR to HDR, parametric up-conversion | `Y_sdr` linear `[0, 1]` |
| Method C | HDR to SDR, quantization-aware tone mapping | `Y_hdr` **PQ-encoded** `[0, 1]` |

Both take the same `L_hdr` and `L_sdr` peak luminances in cd/m2, so a call with
the arguments in the wrong place compiles and returns a plausible number. Method
C's input being PQ-encoded while Method B's is linear is the second trap.

### alwan_exposure_tonemap_{T} / alwan_reinhard_calibrated_{T}

```c
alwan_status alwan_exposure_tonemap_{T}(alwan_{T} *out, alwan_{T} L, alwan_{T} exposure);

alwan_status alwan_reinhard_calibrated_{T}(alwan_{T} *out, alwan_{T} L,
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

### alwan_tonemap_global_{T}

```c
alwan_status alwan_tonemap_global_{T}(alwan_{T} *rgb_out, size_t out_stride,
                                      alwan_{T} const *rgb_in, size_t in_stride,
                                      size_t count, alwan_tonemap_operator op,
                                      alwan_tonemap_params_{T} const *params);
```

The eleven global operators of colour-hdri, over an image of interleaved RGB.
They agree with `colour_hdri.tonemapping_operator_*` to 3e-15 (suite 130).

| Operator | Maps | Parameters, and what zero reads as |
|---|---|---|
| `SIMPLE` | `x / (x + 1)` per channel | none |
| `NORMALIZATION` | `RGB / L_max` | none |
| `GAMMA` | `(2^ev x)^(1/gamma)` per channel | `gamma` (1), `ev` |
| `LOGARITHMIC` | `log10(1 + qL) / log10(1 + k L_max)` | `q` (1), `k` (1), both at least 1 |
| `EXPONENTIAL` | `1 - exp(-qL / (k L_avg))` | `q` (1), `k` (1), both at least 1 |
| `LOGARITHMIC_MAPPING` | `(ln(1 + pL) / ln(1 + p L_max))^(1/q)` | `p` (1), `q` (1) |
| `EXPONENTIATION_MAPPING` | `(L / L_max)^(p/q)` | `p` (1), `q` (1) |
| `SCHLICK1994` | `pL / (pL - L + L_max)` | `p` (1) |
| `TUMBLIN1999` | `m L_da (L / L_wa)^(g_w / g_d)`, over `display_peak` | `display_adaptation` (20 cd/m2), `display_contrast` (100), `display_peak` (100 cd/m2) |
| `REINHARD2004` | `x / (x + (e^-f I_a)^m)` per channel | `intensity` f, `contrast` m (0.3), `light_adaptation` a, `chromatic_adaptation` c, `automatic_contrast` |
| `FILMIC` | Hable's curve at `exposure_bias * x`, over its value at `linear_white` | `shoulder_strength` (0.22), `linear_strength` (0.3), `linear_angle` (0.1), `toe_strength` (0.2), `toe_numerator` (0.01), `toe_denominator` (0.3), `exposure_bias` (2), `linear_white` (11.2) |

`L` is a pixel's luminance, `luminance_weights` times RGB, with sRGB's weights when
all three are zero; `L_max` is the image's peak, `L_avg` and `L_wa` its log averages.
A zeroed params struct, or `NULL`, is every operator at colour-hdri's defaults.

The operators from `LOGARITHMIC` to `TUMBLIN1999` scale RGB by `L_d / L`, which keeps
the ratios between the channels, and map a pixel with zero luminance to black.
`REINHARD2004` maps each channel against its adaptation level `I_a`, which `a` moves
from the image's (0) to the pixel's (1) and `c` from luminance (0) to the channel
itself (1). Every operator but `SIMPLE`, `GAMMA` and `FILMIC` reads statistics of the
whole image, so map an image in one call: a tile has its own peak and log average.
The luminance operators need every luminance finite and not negative; the
per-channel ones apply their formula to any value.

Where the result differs from colour-hdri, on purpose
([alwan_decisions.md](../alwan_decisions.md#reinhard-2004-follows-the-paper-where-colour-hdri-does-not)):

- `REINHARD2004` follows Reinhard and Devlin 2005 for `chromatic_adaptation` above 0
  and for `automatic_contrast`. colour-hdri multiplies the local term by luminance,
  `(c x + 1 - c) L` for the paper's `c x + (1 - c) L`, and its automatic `m` raises
  only the denominator of `k` to 1.4 and subtracts a log average from a log. alwan
  takes `k = (ln L_max - ln L_avg) / (ln L_max - ln L_min)`, all of luminance plus
  the log average's epsilon, so `k` lies in `[0, 1]`. Its global adaptation level,
  which `light_adaptation` below 1 mixes in, is the paper's arithmetic means,
  `c mean(channel) + (1 - c) mean(L)` (p. 17 and the source in its Fig. 7); colour-hdri
  takes the log average of luminance there, 0.314 of output away on suite 130's image.
  OpenCV's `TonemapReinhard` takes the mean too, and suite 130 holds alwan to it.
- `q` or `k` below 1 is `ALWAN_E_INVALID` for `LOGARITHMIC` and `EXPONENTIAL`; colour-hdri
  raises it to 1.
- A pixel with zero luminance is black where colour-hdri divides zero by zero.

`TUMBLIN1999` takes the natural log of `L_da` in `g_wd` and base-10 logs in the
gammas, as colour-hdri does.

---

## D-Series Illuminant from CCT

### alwan_d_series_illuminant_xy_{T}

```c
alwan_status alwan_d_series_illuminant_xy_{T}(alwan_vec2_{T} *xy_out, alwan_{T} cct);
```

Compute CIE D-series illuminant xy chromaticity from correlated color temperature (4000-25000K).

**Example:**
```c
alwan_vec2_{T} xy;
alwan_d_series_illuminant_xy_{T}(&xy, 6500.0);  /* D65 */
```
---

## Display Characterization

PQ peak clipping, ST.2086 mastering-display metadata, and the fused MaxCLL/MaxFALL scan.
The header names all three and gives no domains, no stride units and no failure modes.

Across the whole cluster only two status codes are reachable. `ALWAN_E_INVALID` means
exactly one thing: a pointer argument was NULL. Everything else returns `ALWAN_OK`,
including `count == 0`, a negative `display_peak`, `min_luminance` above `max_luminance`,
and a `pq_value` that decodes to NaN. See [Error Codes](#error-codes). No value is ever
validated.

`alwan_dev/tests/78_hdr_ecosystem.c` covers all three at f64. Untested: the f32
instantiations, `display_peak <= 0` or NaN, `pq_value` outside `[0, 1]`, `stride = 0`,
and `count == 0`.

### alwan_pq_normalize_peak_{T}

```c
alwan_status alwan_pq_normalize_peak_{T}(alwan_{T} *pq_out, alwan_{T} pq_value,
                                         alwan_{T} display_peak);
```

Three lines: decode `pq_value` to absolute cd/m2, clip that to `display_peak`, re-encode.
The decode and encode are `alwan_pq_eotf_{T}` and `alwan_pq_oetf_{T}`, `ALWAN_INLINE` core
functions in `src/alwan/core/alwan_core.inc` (lines 53 and 38). Neither name appears in
`alwan.h` and neither is exported from the shared library; the public route to the same
curve is `alwan_eotf_apply_{T}(linear_out, out_stride, encoded_in, in_stride, count,
ALWAN_TF_PQ)`, see [transfer-functions.md](transfer-functions.md).

**Parameters:**
- `pq_out` -- PQ-encoded result. NULL is the only `ALWAN_E_INVALID` in this function;
  `api/alwan_hdr_impl.inc:142` checks `pq_out` alone.
- `pq_value` -- PQ-encoded input. Intended domain is `[0, 1]` with `1.0` = 10000 cd/m2.
  Nothing enforces it and the header never states it.
- `display_peak` -- display peak luminance in cd/m2. Never validated, in either direction.

> **The clip runs after a full EOTF decode, so a large `pq_value` reaches it already
> broken.** The EOTF denominator `c2 - c3 * E^(1/m2)` reaches zero at
> `E = (c2/c3)^m2 = 1.9920600818564766`. At that value the quotient is `+inf` and the
> decode returns `+inf`, which is harmless: `+inf > display_peak` is true, the clip fires,
> and the call returns `alwan_pq_oetf_{T}(display_peak)`. The denominator stays exactly
> `0.0` for the next 77 ulps. From `pq_value = 1.992060081856494` upward it is negative,
> the final `pow` takes a negative base with exponent `1/m1 = 6.2773`, and `pow`/`powf`
> returns NaN. NaN passes the clip untouched, because `NaN > display_peak` is false, and
> leaves through the OETF. Measured in f64: `1.99195` decodes to 6.9467930716154733e+29,
> the threshold decodes to `+inf`, and `1.992060081856494` and `2.0` decode to NaN. Every
> one of these returns `ALWAN_OK`.

Below 1.99206 the decode still runs away: `pq_value = 1.5` decodes to 3140795.9 cd/m2 and
`1.9` to 2.66e+11 cd/m2. Clamp `pq_value` to `[0, 1]` before the call.

> **The output floor is 7.309559025783966e-07 in f64 and 7.3095589e-07 in f32, never 0.**
> The EOTF floors a negative `pq_value` at 0, and `alwan_pq_oetf_{T}(0)` is
> `(c1/1)^m2 = 0.8359375^78.84375`, which is that value. Black does not come back as
> black, and a negative input is neither preserved nor rejected. Suite 78's
> "black preserved" assertion uses tolerance 1e-6, so the floor passes it.

Two silent `display_peak` failures:

- `display_peak <= 0` makes `L_scaled` zero or negative for every input, so every finite
  decode returns the floor and reports `ALWAN_OK`. A `pq_value` that decodes to NaN still
  yields NaN, since `NaN > display_peak` is false whatever the sign of `display_peak`.
- `display_peak = NaN` makes the comparison `L_abs > display_peak` false, which disables
  the clip and degenerates the call into a lossy EOTF/OETF round trip.

The clip is one-sided (upper only). There is no black-level or min-luminance floor, so
`display_peak` is the only display parameter that participates.

The transform is not an identity below the peak. A sub-peak value still makes the full
`pow` round trip: `0.5` against a 1000 cd/m2 peak comes back as 0.50000000000000067 in
f64 and 0.499997199 in f32 (error -2.8e-06; worst 1.4e-05 over `[0, 1]`, near 0.875). A
second call re-clips to the same peak, so the function is idempotent while remaining
lossy. Relevant to determinism coverage; see [../determinism.md](../determinism.md).

Output is `<= 1.0` whenever `pq_value <= 1.0`. It can exceed `1.0` only when
`pq_value > 1.0` and `display_peak > 10000` or NaN.

### alwan_st2086_init_{T}

```c
alwan_status alwan_st2086_init_{T}(alwan_st2086_metadata_{T} *meta,
                                   alwan_{T} const display_primaries_xy[6],
                                   alwan_{T} const white_point_xy[2],
                                   alwan_{T} max_luminance,
                                   alwan_{T} min_luminance);
```

Copies ten fields into `alwan_st2086_metadata_{T}`. Hand-unrolled, and that is the entire
function body after the three-pointer NULL check at `api/alwan_hdr_impl.inc:152`.

**Parameters:**
- `meta` -- destination struct. NULL returns `ALWAN_E_INVALID`.
- `display_primaries_xy` -- exactly 6 values, ordered `rx, ry, gx, gy, bx, by`. NULL
  returns `ALWAN_E_INVALID`.
- `white_point_xy` -- exactly 2 values, `wx, wy`. NULL returns `ALWAN_E_INVALID`.
- `max_luminance`, `min_luminance` -- cd/m2, stored raw

> **The primaries array is R, G, B ordered. The HEVC `mastering_display_colour_volume`
> SEI orders its primaries G, B, R, and AV1's HDR MDCV metadata follows the same
> convention.** The order is written down only in `src/alwan/alwan_types_gen.inc`, on the
> struct field, and not at the declaration in `alwan.h`. Reorder when marshalling to or
> from a bitstream.

> **The function knows nothing about ST.2086 beyond the field names.** No chromaticity
> range check, no check that `min_luminance < max_luminance` (`max_luminance = 0.005`
> with `min_luminance = 4000` is stored and returns `ALWAN_OK`), no rejection of negative
> luminance, and no quantisation to any wire format. The struct holds plain `alwan_{T}` in
> cd/m2 and raw xy, while containers carry integers in format-specific units, so a round
> trip through a real container will not reproduce what you stored. The chromaticity step
> is 0.00002 in both ST 2086 and the HEVC SEI, but HEVC (Annex D) constrains the coded
> value to 0..50000, that is `[0, 1.0]`, and carries both
> `max_display_mastering_luminance` and `min_display_mastering_luminance` in 0.0001 cd/m2
> units where ST 2086 itself specifies max luminance in 1 cd/m2 steps. Read the clause for
> the container you are writing before scaling these fields.

The `[6]` and `[2]` in the prototype decay to pointers and enforce nothing. Indices `0..5`
and `0..1` are read with literal subscripts, so a shorter array is an out-of-bounds read
with no guard.

The public function never calls its core twin. The C-visible twin is
`alwan_st2086_init_{T}_v` at `src/alwan/core/alwan_hdr_core.inc:357`: it takes non-const
input arrays and places its four outputs last (`out_primaries_xy`, `out_white_xy`,
`out_max_lum`, `out_min_lum`), which inverts the outputs-first rule in
[../api-conventions.md](../api-conventions.md). The public wrapper duplicates the copy
instead, so the two can drift and code written against the core header gets a different
argument order. The unsuffixed `alwan_st2086_init_v` at `core/alwan_hdr_core.h:340` is the
GPU instantiation and is not reachable from a C build.

### alwan_content_light_level_compute_{T}

```c
alwan_status alwan_content_light_level_compute_{T}(alwan_content_light_level_{T} *cll_out,
                                                   alwan_{T} const *rgb_in,
                                                   size_t stride, size_t count);
```

One pass over `count` pixels. Per pixel it takes `max(ptr[0], ptr[1], ptr[2])`;
`max_cll` is the maximum of that over the buffer, `max_fall` is its arithmetic mean.
This is `alwan_maxcll_{T}` and `alwan_maxfall_{T}` fused into one pass. If you need one
value, the single-value twins documented above do the same work.

**Parameters:**
- `cll_out` -- receives `max_cll` and `max_fall`. NULL returns `ALWAN_E_INVALID`.
- `rgb_in` -- interleaved linear RGB. NULL returns `ALWAN_E_INVALID`; exactly 3 scalars
  are dereferenced per pixel.
- `stride` -- per-pixel stride in **bytes**
- `count` -- number of pixels

> **`stride` is the per-pixel stride in bytes**, the same memcpy convention as
> `alwan_maxcll_{T}` above. Neither this function nor its `alwan_maxcll_{T}` /
> `alwan_maxfall_{T}` siblings state the unit in `alwan.h`; the entry points that do spell
> it out are the ones with `in_stride` / `out_stride` pairs. Packed is
> `3 * sizeof(alwan_f32)` = 12 or `3 * sizeof(alwan_f64)` = 24.
> `api/alwan_hdr_impl.inc:180` casts to `char const *`, adds `i * stride`, then casts back
> and dereferences, so `stride = 3` produces misaligned loads: undefined behaviour,
> garbage on x86 and a possible fault on strict-alignment targets, and still `ALWAN_OK`.
> `stride = 0` re-reads pixel 0 `count` times, also `ALWAN_OK`. There is no minimum, and
> no overflow check on `i * stride`.

> **`max_fall` is the mean of the one buffer you pass, and CTA-861.3 MaxFALL is the
> maximum over frames of each frame's average.** The field name promises the second and
> the loop computes the first. Call this once per frame and take the max of `max_fall`
> yourself. Called once over a whole stream it yields the stream mean.

Neither output is luminance-weighted. Both are built from per-pixel `max(R,G,B)`, with no
Y coefficient anywhere in the loop, so an unequal-energy channel dominates.

The two fields of the same struct clamp differently. `max_val` is seeded at `0.0` and only
ever replaced by a larger value, so `max_cll` has a hard floor of 0 and an all-negative
buffer reports `max_cll = 0`. The running `sum` has no floor, so the same buffer reports a
negative `max_fall`.

`count == 0` is an early-return success: both fields set to `0.0`, status `ALWAN_OK`. It
is not `ALWAN_E_NODATA`. A caller checking only the status cannot separate an empty buffer
from a genuinely black frame.

> **The f32 and f64 forms are not equivalent for `max_fall`.** The running sum is declared
> in the working type, so the f32 build accumulates in single precision with no Kahan step
> and no f64 widening. On a 4K frame (8.29M pixels) around 100 cd/m2 the sum reaches
> ~8.3e8, where the float32 ULP is 64 while each addend is ~100, so `max_fall` drifts
> systematically low. `max_cll` is a max rather than a sum and is unaffected, so one field
> of the struct degrades with the type choice and the other does not. Use the f64 form for
> large frames.

`ptr[3]` is never read. A 4th channel is skipped only through `stride`, so an RGBA buffer
with `stride = 4 * sizeof(alwan_{T})` works and alpha is dropped silently. A buffer whose
last pixel has fewer than 3 readable scalars is an out-of-bounds read with no guard.

The `(cd/m2)` in the header comment is a caller obligation. The loop is max, compare, sum
and divide on the raw input values, with no constant, no transfer function and no range
test, so a normalised `[0, 1]` buffer returns normalised `max_cll` and `max_fall` and
reports `ALWAN_OK`. MaxCLL and MaxFALL are only meaningful as absolute nits.

The public wrapper contains its own loop and does not call the core twin.
`alwan_content_light_level_{T}_v` at `src/alwan/core/alwan_hdr_core.inc:378` takes
`(rgb_data, count, stride_bytes)`, which swaps the last two arguments relative to the
public form; both are `size_t`, so nothing catches the transposition. The core returns the
max only, and there is no core twin for `max_fall` at all. `alwan_hdr_core.inc` is the
C-backend body: `core/alwan_hdr_core.h` includes it twice, at lines 37 and 42, both inside
`#if ALWAN_BACKEND == ALWAN_BACKEND_C`. Everything below the `#else` at line 47 is the
GPU/Halide body, including the CPU-only fence at line 363
(`ALWAN_BACKEND_C || ALWAN_BACKEND_HALIDE`) whose `ALWAN_BACKEND_C` arm can never be true
in that branch, which makes it a Halide-only fence. See [backends.md](backends.md).

**Example:**
```c
alwan_content_light_level_f64 cll;
alwan_content_light_level_compute_f64(&cll, pixels,
                                      3 * sizeof(alwan_f64), width * height);
/* per frame; stream MaxFALL is the max of cll.max_fall over all frames */
```

### Struct declarations differ by backend

`alwan_types_gen.inc:76-82` comments every field of `alwan_st2086_metadata_{T}`: the
`rx, ry, gx, gy, bx, by` ordering, `wx, wy`, and `cd/m2 (nits)` on both luminances. The
GPU branch of `alwan_types.h` (line 124 onward) declares an unsuffixed
`alwan_st2086_metadata` over `alwan_scalar` with bare fields (line 258), so the name
differs as well as the documentation. The `_f32` / `_f64` suffixed forms these functions
take exist only on the C backend.

`alwan_content_light_level` carries no field comments in either branch
(`alwan_types_gen.inc:84-88`, `alwan_types.h:264-267`), so the cd/m2 obligation on
`max_cll` and `max_fall` is documented nowhere on the struct.


---

## BT.2408 Conversions

### alwan_bt2408_hlg_to_pq_{T}_map_interleave / alwan_bt2408_pq_to_hlg_{T}_map_interleave

```c
alwan_bt2408_hlg_to_pq_f64_map_interleave(pq, 3 * sizeof(alwan_f64),
                                          hlg, 3 * sizeof(alwan_f64), n, 0.0);
```

BT.2408's transcoding through display light: the HLG EOTF of a reference display
of peak `hlg_peak_nits` (0 for 1000 cd/m2, black at 0, system gamma
1.2 + 0.42 log10(Lw / 1000)), then the PQ inverse EOTF, or the reverse. PQ can carry
light above the HLG display's peak: `clip_to_peak` limits it before the
conversion, otherwise it comes out as an HLG signal above 1.

### alwan_bt2408_sdr_to_pq_{T}_map_interleave / alwan_bt2408_sdr_to_hlg_{T}_map_interleave

SDR display light, linear with 1 at SDR reference white, placed at `sdr_white_nits`
(0 for 203 cd/m2) and encoded. At the default, SDR white lands at 58 % PQ and 75 %
HLG, as BT.2408 tabulates.

All four match colour-science compositions of the BT.2100 functions to 1e-14.

---

## ISO 21496-1 Gain Maps

```c
alwan_gain_map_params_f64 gm;
memset(&gm, 0, sizeof(gm));
for (int c = 0; c < 3; c++) {
    gm.base_offset[c] = 1.0 / 64.0;
    gm.alternate_offset[c] = 1.0 / 64.0;
}
gm.alternate_hdr_headroom = 2.0;              /* HDR peak four times SDR white */
alwan_gain_map_measure_f64(&gm, sdr, stride, hdr, stride, n);
alwan_gain_map_encode_f64_map_interleave(gain, stride, sdr, stride, hdr, stride, n, &gm);

/* at display time */
alwan_f64 w;
alwan_gain_map_weight_f64(&w, &gm, log2(display_peak / sdr_white));
alwan_gain_map_apply_f64_map_interleave(out, stride, sdr, stride, gain, stride, 3, n, &gm, w);
```

`alwan_gain_map_params_{T}` holds the metadata in the standard's log2 units: the
gain range, the encoding gamma (0 reads as 1), the two offsets, and the headrooms
of the base and alternate renditions. The weight is (H - H_base) / (H_alt - H_base)
limited to [0, 1], and the rendition is (base + k_base) 2^(G W) - k_alt. A
single-channel map (`gain_channels` = 1) applies one gain and the channel-0
metadata to all three channels. The core functions `alwan_gain_map_weight_v`,
`alwan_gain_map_encode_v` and `alwan_gain_map_apply_v` also build on the GPU
backends.

The formulas are the standard's and libultrahdr's, and the f32 path agrees with
libultrahdr's arithmetic to 2e-7.

---

## PU21 Perceptual Encoding

```c
alwan_f64 v, psnr;
alwan_pu21_encode_f64(&v, 100.0, ALWAN_PU21_BANDING_GLARE);          /* 256.38 */
alwan_pu21_psnr_f64(&psnr, test, 3 * sizeof(alwan_f64), ref, 3 * sizeof(alwan_f64),
                    pixel_count, 3, ALWAN_PU21_BANDING_GLARE);         /* dB */
```

Mantiuk and Azimi 2021. PSNR and SSIM were built for display-referred SDR values, where
equal steps are roughly equally visible. On HDR luminance they are not: an error of
1 cd/m2 is invisible at 1000 cd/m2 and glaring at 0.1. PU21 maps absolute luminance, as
a reference display would emit it, to a scale where equal steps are about equally
visible again, fitted so that 0.005 cd/m2 encodes to about 0, 100 cd/m2 to about 256,
and 10000 cd/m2 to about 595 for `ALWAN_PU21_BANDING_GLARE`. That is the variant the
authors recommend and the zero value. The other three are the paper's fits for
other conditions.

`alwan_pu21_encode_{T}` is the curve alone. The authors' `pu21_encoder.m` limits
luminance to [0.005, 10000] cd/m2 before encoding; a caller that wants its numbers does
the same, and `alwan_pu21_psnr_{T}` does, since `pu21_metric.m` encodes through it.
PU-PSNR is 10 log10(256^2 / MSE) over every value, with one or three channels per
pixel, and +inf for images that encode identically.

The parameters are the published ones, read by gendata from the pinned
`pu21_encoder.m` (BSD-3-Clause), and the tests evaluate that file's equation in
50-digit arithmetic. The encode matches it to a few 1e-13, which is the formula's own
conditioning in double, and PU-PSNR to 1e-9 dB. PU-SSIM is in the next section.

---

## PSNR and CPSNR

### alwan_psnr_{T}

```c
alwan_f64 psnr[3], cpsnr;
alwan_psnr_f64(psnr, &cpsnr, test, width * 3 * sizeof(alwan_f64),
               ref, width * 3 * sizeof(alwan_f64), width, height, 3, 4, 1.0);
```

Peak signal to noise ratio over `width` x `height` pixels of `channels` values each (1 to
4), rows `row_stride` bytes apart. `psnr_out` receives one value per channel and
`cpsnr_out` the single pooled figure; either may be `NULL`.

`border` pixels are dropped from every edge **before** anything is accumulated. That is
what makes the number comparable: a demosaicked image is least reliable at its edges,
the demosaicing literature crops before it compares, and a figure taken over the whole
frame is not the figure those papers report. It is a parameter rather than a fixed strip
so a caller can match whatever the work being compared against used.

**CPSNR is not the mean of the per-channel values.** It is `10 log10(data_range^2 / MSE)`
with one MSE pooled across every channel, which is what the demosaicing papers mean and
what scikit-image's `peak_signal_noise_ratio` returns for a multi-channel image. Averaging
the per-channel decibels gives a different answer that looks plausible: on alwan's own
fixtures the two sit between 0.01 and 0.16 dB apart. With a single channel the two are the
same number by definition.

`data_range` is the span the values can take: 1 for `[0, 1]`, 255 for 8-bit. A channel
that matches exactly reports `+inf`, as `alwan_pu21_psnr_{T}` does. The metric accumulates
in double in both precisions, and in a deterministic build the logarithm routes through
`alwan_det_log10`, so the figures are the same bits on every platform.

`ALWAN_E_INVALID` on a NULL image, both outputs NULL, `channels` outside 1 to 4, a
`data_range` that is not finite and positive, a zero dimension, or a `border` that leaves
no pixels.

Comparing demosaicing methods is what this is for; see `alwan_cfa_bayer_demosaic_{T}` in
[spectral.md](spectral.md).

## SSIM and PU-SSIM

```c
alwan_f64 s, pu;
alwan_ssim_f64(&s, test, width * sizeof(alwan_f64), ref, width * sizeof(alwan_f64),
               width, height, 1.0);                          /* one channel, range 1 */
alwan_pu21_ssim_f64(&pu, test_rgb, 3 * width * sizeof(alwan_f64),
                    ref_rgb, 3 * width * sizeof(alwan_f64),
                    width, height, 3, ALWAN_PU21_BANDING_GLARE);   /* cd/m2 in */
```

`alwan_ssim_{T}` is the structural similarity index of Wang, Bovik, Sheikh and
Simoncelli 2004 on one channel, with the paper's settings as scikit-image's
`structural_similarity` computes them when asked to match it (`gaussian_weights=True`,
`sigma=1.5`, `use_sample_covariance=False`). The means, variances and covariance come
from an 11-tap Gaussian window of sigma 1.5, the constants are K1 = 0.01 and K2 = 0.03
times the data range, the borders are reflected with the edge sample repeated, and the
index is the mean of the map with a 5-pixel strip dropped at every edge. Both sides of
the image must be at least 11 pixels. The data range is the span the values can take:
1 for [0, 1], 255 for 8-bit. Identical images give exactly 1. Either precision reads
its images and computes in double. It matches scikit-image to 2e-14.

`alwan_pu21_ssim_{T}` is `pu21_metric.m`'s SSIM. It takes luminance from RGB with
that file's weights (0.212656, 0.715158, 0.072186), or uses the values directly with
one channel, in cd/m2. It limits them to [0.005, 10000], encodes them with PU21, and
takes the SSIM over a range of 256. `pu21_metric.m` calls MATLAB's `ssim`, which pads
and averages the borders its own way. alwan follows scikit-image, which the tests hold
it to, so the two agree away from the image edges and not at them.

---

## Image comparison metrics

```c
alwan_status alwan_mean_squared_error_{T}(double *mse, alwan_{T} const *a, size_t a_row_stride,
                                          alwan_{T} const *b, size_t b_row_stride,
                                          size_t width, size_t height, size_t channels);
alwan_status alwan_normalized_root_mse_{T}(double *nrmse, alwan_{T} const *image_true, size_t true_row_stride,
                                           alwan_{T} const *image_test, size_t test_row_stride,
                                           size_t width, size_t height, size_t channels,
                                           alwan_nrmse_normalization normalization);
alwan_status alwan_normalized_mutual_information_{T}(double *nmi, alwan_{T} const *a, size_t a_row_stride,
                                                     alwan_{T} const *b, size_t b_row_stride,
                                                     size_t width, size_t height, size_t channels,
                                                     size_t bins);
alwan_status alwan_structural_similarity_{T}(double *ssim, alwan_{T} const *test, size_t test_row_stride,
                                             alwan_{T} const *ref, size_t ref_row_stride,
                                             size_t width, size_t height, size_t channels,
                                             double data_range, alwan_ssim_params const *params);
/* each also for 8-bit images, same arguments with unsigned char pixels: */
alwan_status alwan_mean_squared_error_u8(...);
alwan_status alwan_normalized_root_mse_u8(...);
alwan_status alwan_normalized_mutual_information_u8(...);
alwan_status alwan_structural_similarity_u8(...);
```

scikit-image's `mean_squared_error`, `normalized_root_mse`, `normalized_mutual_information`
and `structural_similarity`, on `width x height` pixels of 1 to 4 channels. 8-bit images
compare their raw values, as scikit-image casts them without rescaling.

- **MSE** is the mean of `(a - b)^2` over every value.
- **NRMSE** is `sqrt(MSE)` over the true image's root mean square (`EUCLIDEAN`), its range
  (`MIN_MAX`) or its mean (`MEAN`). A zero denominator is `ALWAN_E_RANGE`.
- **NMI** is `(H(A) + H(B)) / H(A, B)`, entropies in nats, from a joint histogram of
  `bins x bins` (0 reads as 100) over each image's range.
- **SSIM** is the mean of the per-channel indices, as `channel_axis` gives it. The zero
  `alwan_ssim_params` are `alwan_ssim`'s settings, the paper's: a Gaussian window of sigma 1.5
  truncated at 3.5, and population covariance. `UNIFORM` with `sample_covariance` set is
  scikit-image's own default, a 7 x 7 box. `win_size` 0 reads as 7 for the box and
  `2 int(3.5 sigma + 0.5) + 1` for the Gaussian. The map is averaged with `(win_size - 1) / 2`
  cropped from every edge.

The sums follow numpy's orders, which is where two correct implementations part in the last
bits. A mean over a contiguous array is numpy's pairwise sum. A float32 array averaged in
float64, and a cropped view, are reduced through buffers of 8192 values. The Gaussian runs
through `alwan_filter`, which is bit-exact to scipy, and the box is scipy's running sum
transcribed from `ni_filters.c`.

In suite 256 on a 3-channel pair:
- MSE, NRMSE and NMI equal scikit-image to the bit in f64, f32 and u8.
- SSIM is within 1.1e-16 in f64 and exact in u8.
- SSIM in f32 is within 6.4e-8: scikit-image stays in float32 there, while alwan computes in
  double.
- With the paper's setting, the result is within 5e-15 of `alwan_ssim`, whose blur sums its
  taps in another order.

**Returns:** `ALWAN_E_INVALID` for a NULL, a zero size, channels outside 1 to 4, an unknown
normalisation or window, or a data range that is not finite and positive. `ALWAN_E_RANGE` for
an even window, a window larger than the image, a negative `k1` or `k2`, or a sigma over 64.

## Exposure and Bracket Merging

```c
alwan_exposure_settings_f64 bracket[3] = {
    { 8.0, 1.0 / 500.0, 100.0 }, { 8.0, 1.0 / 60.0, 100.0 }, { 8.0, 1.0 / 8.0, 100.0 },
};
alwan_f64 const *images[3] = { short_exposure, mid_exposure, long_exposure };
alwan_hdr_merge_f64_map_interleave(radiance, 3 * sizeof(alwan_f64),
                                   images, 3 * sizeof(alwan_f64), pixel_count,
                                   bracket, 3, ALWAN_MERGE_WEIGHT_DEBEVEC1997, NULL, 0);
```

The exposure functions take N, t and S as EXIF records them. `alwan_average_luminance`
is the ISO 2720 reflected-light meter, N^2 / t / S x k with k = 12.5, and the merge
reports radiance on that scale. The bracket is ordered from the shortest exposure
to the longest: the shortest is trusted fully at and above 0.5, the longest at and
below it. Values are normalised sensor data, limited to [2.2e-16, 1]. A response
curve, when given, is sampled on [0, 1] per channel after weighting.

The four weights are colour-hdri's: Debevec 1997's triangle (the default), the hat,
a Gaussian and an anchored double sigmoid. The Debevec triangle is normalised by
its own peak; colour-hdri normalises by the image's largest weight, which is the
same number whenever the image holds a value at 0.5.

---

## Camera Response Recovery

```c
typedef enum {
    ALWAN_CAMERA_RESPONSE_DEBEVEC1997 = 0,
    ALWAN_CAMERA_RESPONSE_ROBERTSON2003 = 1
} alwan_camera_response_method;

alwan_status alwan_camera_response_{T}(alwan_{T} *response_out, alwan_{T} const *const *images,
                                       size_t in_stride, size_t count,
                                       alwan_exposure_settings_{T} const *settings, size_t image_count,
                                       alwan_camera_response_method method,
                                       alwan_camera_response_params const *params);
```

The camera response of an exposure bracket, the curve that `alwan_hdr_merge` takes: per
channel, the exposure each normalised pixel value records, `bins` values per channel,
planar, R then G then B. One entry point with a method enum; each method reads its own
fields of `alwan_camera_response_params`, and a zero field is its default (colour-hdri's
for Debevec, OpenCV's for Robertson). `params` NULL is every default.

| Field | Method | 0 reads as |
|---|---|---|
| `bins` | both | 256 |
| `samples` | `DEBEVEC1997` | 1000 |
| `smoothing` | `DEBEVEC1997` | 30 |
| `weight` | `DEBEVEC1997` | `ALWAN_MERGE_WEIGHT_DEBEVEC1997` |
| `extrapolation_degree` | `DEBEVEC1997` | 7; negative leaves the solved values |
| `keep_scale` | `DEBEVEC1997` | 0: scaled to peak at 1 |
| `iterations` | `ROBERTSON2003` | 30 |
| `threshold` | `ROBERTSON2003` | 0.01 |

```c
alwan_f64 response[3 * 256];
alwan_camera_response_f64(response, images, 3 * sizeof(alwan_f64), pixel_count,
                          bracket, 3, ALWAN_CAMERA_RESPONSE_DEBEVEC1997, NULL);
alwan_hdr_merge_f64_map_interleave(radiance, 3 * sizeof(alwan_f64), images,
                                   3 * sizeof(alwan_f64), pixel_count, bracket, 3,
                                   ALWAN_MERGE_WEIGHT_DEBEVEC1997, response, 256);
```

Debevec and Malik 1997: per channel, the log exposure each pixel value records is
the least-squares solution over sampled pixels of every exposure, with a smoothness
term weighted by lambda (30) and the middle value pinned at 0. The samples are
Grossberg and Nayar's 2003 histogram points, 1000 per exposure. Where the weight is
zero the response is extrapolated with a degree 7 polynomial, and each channel is
scaled to peak at 1. The params fields change any of these; their zero value is
colour-hdri's default, which suite 119 matches.

```c
alwan_camera_response_f64(response, images, 3 * sizeof(alwan_f64), pixel_count,
                          bracket, 3, ALWAN_CAMERA_RESPONSE_ROBERTSON2003, NULL);
```

Robertson, Borman and Stevenson 2003 uses every pixel instead of samples. Starting
from a linear response, it merges the bracket with the current response, takes the
new response at each value as the mean exposure of the pixels holding it, pins the
middle value at 1, and repeats: 30 rounds, or fewer once the change is below 0.01.
The weight is OpenCV's, a Gaussian over the values, 0 at both ends. Only the ratios
of the exposures matter. `bins`, `iterations` and `threshold` set the resolution, the
rounds and the threshold; their zero value is OpenCV's default, and the result matches
OpenCV's CalibrateRobertson to its float rounding, 5e-6.

Robertson's estimate has no smoothness term. The response is tied down only where
exposures overlap, and between those ties it keeps the shape of its linear start, so
it needs closely spaced exposures. On a fairground bracket from an 8-bit 2.2 power
camera, merged afterwards with the Debevec weight:

| bracket | Robertson, merge error p90 | Debevec, merge error p90 |
|---|---|---|
| 3 exposures, 3 stops apart | 0.51 stops, a sawtooth | 0.035 stops |
| 7 exposures, 1 stop apart | 0.027 stops | 0.019 stops |
| 13 exposures, 1/2 stop apart | 0.009 stops | 0.021 stops |

At 3 stops more rounds do not help (1000 rounds leave 0.46 stops of response error),
and OpenCV gives the same curve. For widely spaced brackets use Debevec.

A value no pixel holds is filled linearly from its neighbours, and held beyond the
first and last value seen. OpenCV leaves it NaN, and that NaN also stops its
convergence test from ever passing, so OpenCV always runs every round on such a
bracket. Without a pixel at the middle value there is nothing to pin, and the call
returns `ALWAN_E_RANGE`.

---

## Exposure Fusion

```c
alwan_f64 const *images[3] = { under, metered, over };   /* display-encoded RGB */
alwan_exposure_fusion_f64(fused, 3 * width * sizeof(alwan_f64),
                                      images, 3 * width * sizeof(alwan_f64), 3,
                                      width, height, ALWAN_EXPOSURE_FUSION_MERTENS2007, NULL);
```

Mertens, Kautz and Van Reeth 2007. Where merging recovers radiance and still needs a
tone curve, fusion goes straight from a bracket of finished pictures to one finished
picture: no response curve, no exposure settings, and the images in any order. Every
pixel of every exposure is weighted by three measures: contrast, the absolute 3 x 3
Laplacian of its BT.601 luma; saturation, the spread of R, G and B about their mean;
and well-exposedness, a Gaussian of width 0.2 around 0.5 per channel. The weights
are normalised per pixel, and the images blend as Laplacian pyramids, each level
under that level of the weights' Gaussian pyramid. Blending a single level instead
leaves visible seams where the weights change quickly.

`alwan_exposure_fusion_params` sets each measure's exponent (0 is 1), leaves a measure
out through `ignore`, and sets sigma and the number of pyramid levels. Its zero value
is the paper's weighting. OpenCV's `createMergeMertens()` default leaves
well-exposedness out, which is `ignore = ALWAN_FUSION_IGNORE_EXPOSURE`. Borders are
OpenCV's. The result is not clamped, and the pyramid can overshoot [0, 1] next to
strong edges.

Given 8-bit images divided by 255, the result matches OpenCV's MergeMertens to 4e-6
on the synthetic brackets of the tests. Photographs differ more. Where a region is
flat in every exposure, every weight falls to OpenCV's floor of 1e-12, and OpenCV's
float32 rounding in the three measures is of the same order, so its rounding decides
the blend there. alwan computes the weights in the working precision. On a 960 x 540
fairground bracket the two agree to 2e-3 at the 99th percentile and differ by up to
0.06 in flat saturated areas. Computing the weights in float32 the way OpenCV does
closes the difference to 4e-7, so the pyramids and borders agree and the difference
is the weights' precision.

---

## Error Codes

- `ALWAN_OK` (0) -- Success
- `ALWAN_E_INVALID` (-1) -- NULL pointer or invalid parameter
- `ALWAN_E_DIVZERO` (-5) -- Division by zero in contrast calculation

---

## Exposure, ISO 12232 and ISO 2720

Photometric exposure as the camera standards define it. Every function is
`ALWAN_E_INVALID` for a NULL output or a luminance, illuminance, ISO, f-number
or time that is not positive.

```c
alwan_status alwan_luminance_to_exposure_value_{T}(alwan_{T} *ev_out, alwan_{T} luminance, alwan_{T} iso, alwan_{T} k);
alwan_status alwan_illuminance_to_exposure_value_{T}(alwan_{T} *ev_out, alwan_{T} illuminance, alwan_{T} iso, alwan_{T} c);
alwan_status alwan_exposure_value_100_{T}(alwan_{T} *ev100_out, alwan_{T} f_number, alwan_{T} exposure_time, alwan_{T} iso);
alwan_status alwan_average_illuminance_{T}(alwan_{T} *illuminance_out, alwan_{T} f_number, alwan_{T} exposure_time, alwan_{T} iso, alwan_{T} c);
alwan_status alwan_focal_plane_exposure_{T}(alwan_{T} *exposure_out, alwan_{T} luminance, alwan_{T} f_number, alwan_{T} exposure_time,
                                            alwan_{T} focal_length, alwan_{T} image_distance, alwan_{T} flare,
                                            alwan_{T} transmittance, alwan_{T} vignetting, alwan_{T} angle);
alwan_status alwan_saturation_based_speed_focal_plane_exposure_{T}(alwan_{T} *exposure_out, alwan_{T} luminance, alwan_{T} f_number,
                                            alwan_{T} exposure_time, alwan_{T} iso, alwan_{T} focal_length, alwan_{T} image_distance,
                                            alwan_{T} flare, alwan_{T} transmittance, alwan_{T} vignetting, alwan_{T} angle);
alwan_status alwan_exposure_index_{T}(alwan_{T} *index_out, alwan_{T} focal_plane_exposure);
alwan_status alwan_photometric_exposure_scale_factor_lagarde2014_{T}(alwan_{T} *scale_out, alwan_{T} ev100,
                                            alwan_{T} transmittance, alwan_{T} vignetting, alwan_{T} angle);
```

- `luminance_to_exposure_value`: `log2(L S / k)`; `illuminance_to_exposure_value`:
  `log2(E S / c)`. A `k` or `c` of 0 takes the ISO 2720 default, 12.5 for the
  reflected-light constant and 250 for the incident one.
- `exposure_value_100`: EV100, the exposure value of the settings referred to
  ISO 100.
- `average_illuminance`: the ISO 2720 incident-light meter, lux,
  `N^2 / t / S x c`, `c` 0 being 250.
- `focal_plane_exposure`: ISO 12232, lux-seconds,
  `q L t F^2 / (N^2 i^2) + flare` with `q = pi / 4 T f_v cos^4(angle)`; focal
  length and image distance in metres, transmittance T (0.9 typical),
  vignetting f_v (0.98), angle off axis in degrees (10).
- `saturation_based_speed_focal_plane_exposure`: the same, scaled for
  saturation-based speed, `H S / 78`.
- `exposure_index`: ISO 12232's `10 / H`.
- `photometric_exposure_scale_factor_lagarde2014`: Lagarde and de Rousiers
  2014, the factor that turns a camera's pixel values into absolute luminance,
  `1 / (78 / (100 q) 2^EV100)`.

Pinned against colour-science's `exposure` module in suite 118.

---

## Merge weights, camera-response sampling, PU21 decode, Mantiuk 2006

### alwan_hdr_merge_weight_{T}

```c
alwan_status alwan_hdr_merge_weight_{T}(alwan_{T} *weight_out, alwan_{T} value, alwan_merge_weight fn);
```

One weighting function of `alwan_hdr_merge` at one value, so a caller can see
the curve the merge is applying.

### alwan_crf_samples_grossberg2003_{T}

```c
alwan_status alwan_crf_samples_grossberg2003_{T}(size_t *bins_out, alwan_{T} const *const *images,
                                                 size_t in_stride, size_t count, size_t image_count,
                                                 size_t samples, size_t bins);
```

Grossberg and Nayar 2003: the pixel values to sample for a camera-response
recovery, chosen so that each sample sits at the same point of every exposure's
histogram. For each of `samples` points u on [0, 1], the bin whose cumulative
histogram is nearest u, per exposure and channel. `bins_out` receives
`samples x image_count x 3` bin indices.

### alwan_pu21_decode_{T}

```c
alwan_status alwan_pu21_decode_{T}(alwan_{T} *out, alwan_{T} value, alwan_pu21_variant variant);
alwan_status alwan_pu21_decode_{T}_map_interleave(alwan_{T} *out, size_t out_stride, alwan_{T} const *in,
                                                  size_t in_stride, size_t count, alwan_pu21_variant variant);
```

The inverse of `alwan_pu21_encode`: PU21 units back to luminance, for the same
four variants. `ALWAN_E_INVALID` for an unknown variant.

### alwan_tonemap_mantiuk2006_{T}

```c
alwan_status alwan_tonemap_mantiuk2006_{T}(alwan_{T} *rgb_out, size_t out_row_stride,
                                           alwan_{T} const *rgb_in, size_t in_row_stride,
                                           size_t width, size_t height,
                                           alwan_tonemap_local_params_{T} const *params, int *iterations_out);
```

Local tone mapping. Every other operator on this page is a curve: a pixel's
result depends on that pixel and on statistics of the image. This one takes the
log luminance apart into contrasts at every scale, shrinks each through a
response curve, and solves for the image whose contrasts those are, so a
pixel's result depends on its neighbours, which is what lets it hold local
texture while losing global range, and why it takes a width and a height rather
than a count.

`rgb_in` and `rgb_out` are interleaved RGB, rows a stride apart; `out` may be
`in`. `params` NULL is every default: `scale` is how hard contrasts are pulled
in, 0 reading as 0.7, larger meaning flatter; `saturation` is the exponent on
each channel's ratio to luminance, 0 reading as 1. `iterations_out`, which may
be NULL, receives how many conjugate-gradient steps the solve took; it stops on
a relative residual of 1e-3 and caps at 100, and in practice takes five to ten,
so a result at the cap is worth looking at. The luminance weights follow the
rest of alwan: zero is the sRGB primaries' Y row; OpenCV uses Rec.601 luma,
which is a different quantity and the wrong one for linear light, and suite 170
passes it explicitly to compare.

The output is not normalised: the solve is anchored at the image's own mean log
luminance, so the result has a level and the caller decides what to do with it.
An image smaller than 2 x 2 is `ALWAN_E_RANGE`; a negative or non-finite
luminance is `ALWAN_E_INVALID`. Reproduces OpenCV's TonemapMantiuk to 2e-06 in
float32 once OpenCV's own rescaling of input and output to [0, 1], which is not
in the paper, is taken off. Mantiuk, Myszkowski and Seidel, "A Perceptual
Framework for Contrast Processing of High Dynamic Range Images", ACM TAP 3(3),
2006.

---

## See Also

- [Transfer Functions](transfer-functions.md) -- PQ, HLG, standard OETF/EOTF
- [ACES Pipeline](aces.md) -- ACES output transforms
- [Color Spaces](color-spaces.md) -- Rec.2100, ICtCp conversions
