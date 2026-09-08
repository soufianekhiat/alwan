# Color Appearance Models API

Functions for color appearance models that predict color perception under different viewing conditions.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

---

## Overview

Color appearance models (CAMs) predict how colors appear under varying viewing conditions:
- Illumination level (dark, dim, average, bright)
- Background luminance
- Surround conditions (dark, dim, average)
- Chromatic adaptation state

**Implemented models:**
- **CIECAM02** -- CIE Color Appearance Model 2002 (forward + inverse, Map, `_ex`)
- **CAM16** -- Color Appearance Model 2016 (forward + inverse, +CAM16-UCS Jab, Map, `_ex`)
- **ZCAM** -- HDR CAM on Jzazbz, Safdar et al. 2021 (forward + inverse, +ZCAM-UCS)
- **RLAB** -- Fairchild cross-media model (forward + inverse)
- **Hellwig & Fairchild 2022** -- HK-effect CAM (forward + inverse)
- **Kim 2009** -- forward + inverse
- **Hunt** -- forward only (inverse not implemented)
- **LLAB**, **ATD95 (Guth)**, **Nayatani 1995** -- forward only
- **CAM18sl** -- self-luminous stimuli (forward + inverse)
- **CAM20u** -- unrelated colors (forward + inverse)

> **Precision:** CIECAM02/CAM16 and RLAB are natively dualized -- their `_f32`
> entry points (and the bulk `_map_interleave` path) compute in `float` throughout
> rather than widening to `double` (see `alwan_cam_impl.inc` / `alwan_rlab_impl.inc`,
> instantiated once per precision). ZCAM's `_f32` entry points are still f64-internal
> facades: the f32 API converts at the boundary and runs the f64 kernel, so they stay
> callable even in an `ALWAN_BUILD_ONLY_F32` build. (The `ALWAN_WITH_F64_FACADE` gate,
> always 1, additionally keeps the f64 machinery for the ZCAM inverse and ACES 1.x
> inverse compiled in single-precision-only builds.)

---

## Viewing Conditions

Each model has its own precision-specific struct. Fields shown here use the CIECAM02/CAM16 layout
(ZCAM and the other models differ slightly -- see `alwan_types_gen.inc`).

```c
/* Surround enum (model-specific: alwan_ciecam02_surround, alwan_cam16_surround, ...) */
typedef enum {
    ALWAN_CIECAM02_SURROUND_AVERAGE = 0,  /* Average surround (outdoor/office) */
    ALWAN_CIECAM02_SURROUND_DIM     = 1,  /* Dim surround (cinema) */
    ALWAN_CIECAM02_SURROUND_DARK    = 2   /* Dark surround (home theater) */
} alwan_ciecam02_surround;
/* CAM16 uses ALWAN_CAM16_SURROUND_*, ZCAM uses ALWAN_ZCAM_SURROUND_*, etc. */

/* CIECAM02 viewing conditions (precision-specific) */
typedef struct {
    alwan_xyz_{T}           white_xyz;           /* Reference white in XYZ (Y typically 100) */
    {T}                     adapting_luminance;  /* Adapting luminance La in cd/m^2 */
    {T}                     background_luminance;/* Background relative luminance Yb/Yw */
    alwan_ciecam02_surround surround;            /* Viewing surround condition */
    int                     discount_illuminant; /* 1 = discount, 0 = adapt */
} alwan_ciecam02_viewing_conditions_{T};

/* CAM16 viewing conditions -- same fields, different surround enum type */
typedef struct {
    alwan_xyz_{T}       white_xyz;
    {T}                 adapting_luminance;
    {T}                 background_luminance;
    alwan_cam16_surround surround;
    int                 discount_illuminant;
} alwan_cam16_viewing_conditions_{T};
```

---

## CIECAM02

### alwan_ciecam02_forward_{T} / alwan_ciecam02_inverse_{T}

```c
int alwan_ciecam02_forward_{T}(alwan_ciecam02_correlates_{T} *out,
                                alwan_xyz_{T} const *xyz,
                                alwan_ciecam02_viewing_conditions_{T} const *vc);

int alwan_ciecam02_inverse_{T}(alwan_xyz_{T} *xyz_out,
                                alwan_ciecam02_correlates_{T} const *correlates,
                                alwan_ciecam02_viewing_conditions_{T} const *vc);
```

Output struct fields (correlates): `J` (lightness), `C` (chroma), `h` (hue angle), `s` (saturation),
`Q` (brightness), `M` (colorfulness), `H` (hue quadrature).

The inverse uses only `J`, `C`, `h` from the correlates struct; other fields are ignored.

**Example:**
```c
alwan_xyz_{T} xyz = {19.01, 20.00, 21.78};

alwan_ciecam02_viewing_conditions_{T} vc;
vc.white_xyz.x          = 95.05;
vc.white_xyz.y          = 100.0;
vc.white_xyz.z          = 108.88;
vc.adapting_luminance   = 64.0;   /* cd/m^2 */
vc.background_luminance = 20.0;   /* Yb/Yw */
vc.surround             = ALWAN_CIECAM02_SURROUND_AVERAGE;
vc.discount_illuminant  = 0;

alwan_ciecam02_correlates_{T} corr;
alwan_ciecam02_forward_{T}(&corr, &xyz, &vc);
printf("J=%.2f C=%.2f h=%.2f\n", corr.J, corr.C, corr.h);
```

---

## CAM16

### alwan_cam16_forward_{T} / alwan_cam16_inverse_{T}

```c
int alwan_cam16_forward_{T}(alwan_cam16_correlates_{T} *out,
                             alwan_xyz_{T} const *xyz,
                             alwan_cam16_viewing_conditions_{T} const *vc);

int alwan_cam16_inverse_{T}(alwan_xyz_{T} *xyz_out,
                             alwan_cam16_correlates_{T} const *correlates,
                             alwan_cam16_viewing_conditions_{T} const *vc);
```

Improved CIECAM02 with better chromatic adaptation and numerical stability.

### alwan_cam16_to_ucs_{T} / alwan_cam16_from_ucs_{T}

```c
int alwan_cam16_to_ucs_{T}(alwan_cam_jab_{T} *jab_out,
                            alwan_cam16_correlates_{T} const *correlates);

int alwan_cam16_from_ucs_{T}(alwan_cam16_correlates_{T} *out,
                              alwan_cam_jab_{T} const *jab);
```

Convert CAM16 JMh to uniform color space J'a'b' for color difference calculations.

---

## Batch Processing (CIECAM02 and CAM16)

Each `*_stride` immediately follows the buffer it describes (memcpy argument order).
The forward maps take only an `in_stride` (interleaved XYZ); the output is a packed
array of correlates structs. The inverse maps take only an `out_stride` (interleaved XYZ);
the input is a packed array of correlates structs.

```c
/* Forward: interleaved XYZ -> packed correlates */
int alwan_ciecam02_forward_{T}_map_interleave(
    alwan_ciecam02_correlates_{T} *correlates_out,
    alwan_{T} const *xyz_in, size_t in_stride,
    alwan_ciecam02_viewing_conditions_{T} const *vc,
    size_t count);

int alwan_cam16_forward_{T}_map_interleave(
    alwan_cam16_correlates_{T} *correlates_out,
    alwan_{T} const *xyz_in, size_t in_stride,
    alwan_cam16_viewing_conditions_{T} const *vc,
    size_t count);

/* Inverse: packed correlates -> interleaved XYZ */
int alwan_ciecam02_inverse_{T}_map_interleave(
    alwan_{T} *xyz_out, size_t out_stride,
    alwan_ciecam02_correlates_{T} const *correlates_in,
    alwan_ciecam02_viewing_conditions_{T} const *vc,
    size_t count);

int alwan_cam16_inverse_{T}_map_interleave(
    alwan_{T} *xyz_out, size_t out_stride,
    alwan_cam16_correlates_{T} const *correlates_in,
    alwan_cam16_viewing_conditions_{T} const *vc,
    size_t count);
```

Typed `_map_interleave_ex` variants accept a `void*` interleaved buffer plus an
`alwan_pixel_format` (any of U8/U16/F16/F32/F64), with the precision-agnostic `f64`
correlates struct. The pixel-format argument tails the signature:

```c
int alwan_ciecam02_forward_map_interleave_ex(
    alwan_ciecam02_correlates_f64 *correlates_out,
    void const *xyz_in, size_t in_stride,
    alwan_ciecam02_viewing_conditions_f64 const *vc,
    size_t count, alwan_pixel_format in_fmt);

int alwan_ciecam02_inverse_map_interleave_ex(
    void *xyz_out, size_t out_stride,
    alwan_ciecam02_correlates_f64 const *correlates_in,
    alwan_ciecam02_viewing_conditions_f64 const *vc,
    size_t count, alwan_pixel_format out_fmt);
/* alwan_cam16_forward_map_interleave_ex / _inverse_map_interleave_ex mirror these. */
```

---

## ZCAM

### alwan_zcam_forward_{T} / alwan_zcam_inverse_{T}

```c
int alwan_zcam_forward_{T}(alwan_zcam_correlates_{T} *out,
                            alwan_xyz_{T} const *xyz,
                            alwan_zcam_viewing_conditions_{T} const *vc);

int alwan_zcam_inverse_{T}(alwan_xyz_{T} *xyz,
                            alwan_zcam_correlates_{T} const *correlates,
                            alwan_zcam_viewing_conditions_{T} const *vc);
```

Latest CIE color appearance model with improved HDR support. Built on Jzazbz color space.

### alwan_zcam_to_ucs_{T} / alwan_zcam_from_ucs_{T}

```c
int alwan_zcam_to_ucs_{T}(alwan_jzazbz_{T} *Jab_out,
                            alwan_zcam_correlates_{T} const *correlates);

int alwan_zcam_from_ucs_{T}(alwan_zcam_correlates_{T} *out,
                              alwan_jzazbz_{T} const *Jab);
```

The UCS is Jz with Mz and hz in Cartesian form, so the pair round-trips Jz, Mz
and hz exactly; `from_ucs` sets the other correlates to 0, as CAM16's does.

---

## CAM18sl (Self-Luminous Colors)

No viewing conditions struct -- takes scalar parameters directly.

```c
int alwan_cam18sl_forward_{T}(alwan_cam18sl_correlates_{T} *out,
                               alwan_xyz_{T} const *xyz,
                               alwan_{T} Y_b);   /* background luminance in cd/m^2 */

int alwan_cam18sl_inverse_{T}(alwan_xyz_{T} *xyz_out,
                               alwan_cam18sl_correlates_{T} const *correlates,
                               alwan_{T} Y_b);
```

Color appearance model for self-luminous stimuli (displays, LEDs). Designed for emissive sources.

---

## CAM20u (Unrelated Colors)

No viewing conditions struct -- takes scalar parameters directly.

```c
int alwan_cam20u_forward_{T}(alwan_cam20u_correlates_{T} *out,
                              alwan_xyz_{T} const *xyz,
                              alwan_{T} Y_b,   /* background luminance in cd/m^2 */
                              alwan_{T} L_a);  /* adapting luminance in cd/m^2 */

int alwan_cam20u_inverse_{T}(alwan_xyz_{T} *xyz_out,
                              alwan_cam20u_correlates_{T} const *correlates,
                              alwan_{T} Y_b,
                              alwan_{T} L_a);
```

---

## Additional Color Appearance Models

Alwan also implements several historical and specialized CAMs. All follow the same shape:

```c
alwan_status alwan_<model>_forward_{T}(alwan_<model>_correlates_{T} *out,
                                       alwan_xyz_{T} const *xyz,
                                       alwan_<model>_viewing_conditions_{T} const *vc);

alwan_status alwan_<model>_inverse_{T}(alwan_xyz_{T} *xyz_out,
                                       alwan_<model>_correlates_{T} const *correlates,
                                       alwan_<model>_viewing_conditions_{T} const *vc);
```

| Model | Prefix | Inverse | Agrees with colour-science to |
|---|---|---|---|
| RLAB (Fairchild 1996) | `alwan_rlab_` | Yes | 2.8e-08 |
| Hunt (1991, 1995) | `alwan_hunt_` | No | 4.6e-08 |
| Hellwig and Fairchild 2022 | `alwan_hellwig2022_` | Yes | -- |
| Kim, Weyrich and Kautz 2009 | `alwan_kim2009_` | Yes | 4e-11 |
| LLAB (Luo, Lo, Kuo 1996) | `alwan_llab_` | No | -- |
| ATD95 (Guth 1995) | `alwan_atd95_` | No | 4.3e-10 |
| Nayatani 1995 | `alwan_nayatani95_` | No | validated |

Hunt has no inverse because inverting it is disproportionately complex, not
because the forward is approximate. LLAB, ATD95 and Nayatani95 are forward-only
as published.

> **Zero-initialise the viewing conditions.** Several of these structs use `0` as
> a "derive this for me" sentinel, so `alwan_hunt_viewing_conditions_f64 vc = {0};`
> and then setting what you know is the correct idiom. A struct left as stack
> garbage produces confident nonsense rather than an error, which is what happened
> to alwan's own f32/f64 twin test when Hunt's fields were added.

### RLAB

```c
typedef struct {
    alwan_xyz_{T} xyz_w;          /* white point, Y = 100 */
    alwan_xyz_{T} xyz_n;          /* reference white, usually D65 */
    alwan_{T} Y_n;                /* absolute adapting luminance, cd/m^2 */
    alwan_rlab_surround surround; /* AVERAGE | DIM | DARK */
    int D_factor;                 /* 0 auto, 1 hard copy, 2 soft copy, 3 transparency */
} alwan_rlab_viewing_conditions_{T};
```

`Y_n` is **absolute**, in cd/m², and it is not decoration: RLAB's
incomplete-adaptation term depends on it whenever `D < 1`. The reference viewing
condition is 318.31 cd/m², and `0` is treated as that value so a zero-initialised
struct behaves rather than taking a cube root of zero.

### Hunt

The most demanding struct in the library. On top of the white and the adapting
luminance it takes a background, a proximal field, two scotopic responses and two
induction factors, and **every one of them derives itself from `0` or a
non-positive value**:

| field | left at 0 or below |
|---|---|
| `xyz_b` | derived from `Yb` |
| `xyz_p` | uses the background |
| `p` | simultaneous contrast disabled, which Hunt allows |
| `L_AS` | derived from `CCT_w` |
| `CCT_w` | with `L_AS` also unset, falls back to 5000 K |
| `S`, `S_w` | use the stimulus and white `Y` |
| `N_cb`, `N_bb` | derive `0.725 * (Y_w / Y_b)^0.2` |

`helson_judd_effect` is off by default, matching Hunt's own default.

### Hellwig2022

CAM16 with Helmholtz-Kohlrausch support. The viewing conditions mirror CAM16's:
white in XYZ, `adapting_luminance` in cd/m², `background_luminance` as the
relative `Yb/Yw`, a surround and `discount_illuminant`.

### Kim2009

```c
typedef struct {
    alwan_xyz_{T} white_xyz;
    alwan_{T} La;                 /* adapting luminance, cd/m^2 */
    alwan_{T} Yb;                 /* background luminance factor */
    int discount_illuminant;
    alwan_kim2009_media media;    /* selects the published E parameter */
} alwan_kim2009_viewing_conditions_{T};
```

```c
ALWAN_KIM2009_MEDIA_HIGH_LUMINANCE_LCD = 0   /* E = 1.0    */
ALWAN_KIM2009_MEDIA_TRANSPARENT_AD     = 1   /* E = 1.2175 */
ALWAN_KIM2009_MEDIA_CRT                = 2   /* E = 1.4572 */
ALWAN_KIM2009_MEDIA_REFLECTIVE_PAPER   = 3   /* E = 1.7526 */
```

**`media` changes the answer by more than a factor of three.** For
`XYZ = [19.01, 20, 21.78]` under D65 the four media give `J` = 51.18, 40.56,
28.86 and 14.44. It is an enum rather than a float because the model publishes
four values and defines no continuum between them. Zero, which is what a
zero-initialised struct gets, is the high-luminance LCD.

### LLAB

Takes two illuminants, `xyz_0` for the test condition and `xyz_r` for the
reference, alongside the white, the background factor `Y_b`, a surround and
`D_factor`. Forward only.

### ATD95

```c
typedef struct {
    alwan_xyz_{T} white_xyz;
    alwan_{T} Y_0;      /* absolute adapting field luminance, cd/m^2 */
    alwan_{T} sigma;    /* saturation adjustment */
    alwan_{T} k1, k2;   /* adaptation parameters */
} alwan_atd95_viewing_conditions_{T};
```

> **`H` is a ratio, not an angle.** ATD95 defines `H` as `T_2 / D_2`. It is
> unbounded and may be negative. Do not wrap it into `[0, 360)` or feed it to
> anything expecting degrees; alwan returned degrees here until 2026-08-27 and it
> was wrong.

### Nayatani95

Works in the **`[0, 100]` domain** for both the stimulus and the reference white.
`E_0` is the field illuminance and `E_0r` the normalising illuminance, both in
**lux**; `Y_0` is the background luminance factor. The noise term is fixed at the
model default of 1.0. Forward only.

---

## See Also

- [Color Difference](color-difference.md) -- dE metrics (CAM02 UCS, CAM16 UCS, ZCAM)
- [Chromatic Adaptation](chromatic-adaptation.md) -- White point adaptation
- [Color Spaces](color-spaces.md) -- XYZ conversions
