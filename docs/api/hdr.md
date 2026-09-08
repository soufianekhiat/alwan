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
`LB_target` and `LW_target` are all PQ-encoded `[0, 1]`, not cd/m2. The
`_luminance_` form is the convenience wrapper that takes the two peaks in cd/m2
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

Both take the same `L_hdr` and `L_sdr` peak luminances in cd/m2, so a call with
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

## Error Codes

- `ALWAN_OK` (0) -- Success
- `ALWAN_E_INVALID` (-1) -- NULL pointer or invalid parameter
- `ALWAN_E_DIVZERO` (-5) -- Division by zero in contrast calculation

---

## See Also

- [Transfer Functions](transfer-functions.md) -- PQ, HLG, standard OETF/EOTF
- [ACES Pipeline](aces.md) -- ACES output transforms
- [Color Spaces](color-spaces.md) -- Rec.2100, ICtCp conversions
