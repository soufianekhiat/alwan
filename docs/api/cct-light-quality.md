# CCT & Light Quality API

Functions for correlated color temperature estimation, color rendering indices, and light source quality metrics.

---

## Overview

Light quality functions evaluate illumination characteristics:

- **CCT estimation**: Determine color temperature from chromaticity
- **CRI (Ra)**: CIE Color Rendering Index
- **CQS**: NIST Color Quality Scale
- **TM-30 / CIE 224**: Modern fidelity indices
- **SSI**: Spectral Similarity Index (Academy / SMPTE ST 2122)
- **Metamerism Index**: Color mismatch under illuminant change
- **Whiteness / Yellowness**: ASTM E313 and CIE 2004 indices

### Precision suffixes

Every function in this module is generated for both precisions. `{T}` below stands
for either `f32` (float, `alwan_f32`) or `f64` (double, `alwan_f64`); the matching
value type pairs follow (`alwan_vec2_{T}`, `alwan_xyz_{T}`, `alwan_spd_{T}`). Which
precisions are compiled is gated by `ALWAN_WITH_F32` / `ALWAN_WITH_F64` (both by
default; restrict with `ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64`). Calling a
precision that was excluded from the build fails at link time. There is no unsuffixed
alias; pick `_f32` or `_f64` at the call site.

> **f64-internal facades (by design).** `alwan_cct_kang_xy_f32` (a Newton-Raphson
> inverse with sub-f32-epsilon tolerances) and the spectral quality metrics
> `alwan_cri_ra_f32` / `alwan_cri_specification_f32` / `alwan_cqs_calculate_f32` /
> `alwan_tm30_rf_f32` / `alwan_tm30_specification_f32` / `alwan_cie224_rf_f32` /
> `alwan_ssi_calculate_f32` / `alwan_metamerism_index_f32`
> (wavelength integration over f64 CMF tables) run the algorithm in `double` and
> narrow the result. This is a design choice rather than a missing native-f32
> path: the iterative/integration core needs f64 precision and repeatability. They stay
> callable even in an `ALWAN_BUILD_ONLY_F32` build (gated by `ALWAN_WITH_F64_FACADE`,
> always `1`). The other CCT estimators (McCamy / Robertson / Hernandez-Andres) are
> native f32. See [configuration.md](../configuration.md).

### Argument convention

These functions follow the v2.0 parameter convention: outputs come first, and the
optional `alwan_ctx *ctx` is always the **last** parameter (the pure `xy`-based CCT
helpers take no context at all, since they need no embedded data).

---

## CCT Estimation

### alwan_cct_mccamy_xy

```c
alwan_f32 alwan_cct_mccamy_xy_f32(alwan_vec2_f32 const *xy);
alwan_f64 alwan_cct_mccamy_xy_f64(alwan_vec2_f64 const *xy);
```

McCamy approximation. Fast, ~2% accuracy above 2800K. No context required.

### alwan_cct_robertson_xy

```c
alwan_f32 alwan_cct_robertson_xy_f32(alwan_vec2_f32 const *xy);
alwan_f64 alwan_cct_robertson_xy_f64(alwan_vec2_f64 const *xy);
```

Robertson method: accurate, iterative lookup against the Planckian locus. Returns CCT
in Kelvin, or a negative value on error.

### alwan_cct_hernandez_xy

```c
alwan_f32 alwan_cct_hernandez_xy_f32(alwan_vec2_f32 const *xy);
alwan_f64 alwan_cct_hernandez_xy_f64(alwan_vec2_f64 const *xy);
```

Hernandez-Andres et al. 1999 analytical formula. Valid range: 3000K-50000K.

### alwan_cct_kang_xy

```c
alwan_f32 alwan_cct_kang_xy_f32(alwan_vec2_f32 const *xy);
alwan_f64 alwan_cct_kang_xy_f64(alwan_vec2_f64 const *xy);
```

Kang et al. 2002 method (inverse, uses Newton-Raphson). Valid range: 1667K-25000K.

### alwan_cct_to_xy_kang

```c
void alwan_cct_to_xy_kang_f32(alwan_vec2_f32 *xy_out, alwan_f32 cct);
void alwan_cct_to_xy_kang_f64(alwan_vec2_f64 *xy_out, alwan_f64 cct);
```

Forward transform: convert CCT to xy chromaticity (Kang et al. 2002). Valid range:
1667K-25000K.

### alwan_cct_duv_optimize

```c
alwan_status alwan_cct_duv_optimize_f32(alwan_f32 *cct_out, alwan_f32 *duv_out,
                                        alwan_vec2_f32 const *xy);
alwan_status alwan_cct_duv_optimize_f64(alwan_f64 *cct_out, alwan_f64 *duv_out,
                                        alwan_vec2_f64 const *xy);
```

Compute CCT and Duv (distance from the Planckian locus) using iterative least-squares
optimization. `duv_out` may be `NULL` if only CCT is needed. Returns `ALWAN_OK` on
success, `ALWAN_E_INVALID` if `xy` is invalid. Accuracy: CCT <= 1K, Duv <= 0.0001.

**Example:**
```c
alwan_vec2_f64 xy = {{0.3127, 0.3290}};  /* D65 chromaticity */

/* Quick estimate */
alwan_f64 cct_fast = alwan_cct_mccamy_xy_f64(&xy);

/* Accurate CCT + Duv */
alwan_f64 cct, duv;
alwan_cct_duv_optimize_f64(&cct, &duv, &xy);
printf("CCT: %.0fK, Duv: %.5f\n", cct, duv);
```

### Mired

```c
alwan_status alwan_cct_to_mired_f64(alwan_f64 *mired_out, alwan_f64 cct);
alwan_status alwan_mired_to_cct_f64(alwan_f64 *cct_out, alwan_f64 mired);
```

1e6 / CCT and back. `ALWAN_E_INVALID` for 0.

### Planckian and daylight loci

```c
alwan_status alwan_cct_to_uv_krystek1985_f64(alwan_vec2_f64 *uv_out, alwan_f64 cct);
alwan_status alwan_uv_to_cct_krystek1985_f64(alwan_f64 *cct_out, alwan_vec2_f64 const *uv);
alwan_status alwan_cct_to_uv_planck1900_f64(alwan_vec2_f64 *uv_out, alwan_f64 cct,
                                            alwan_observer_type observer, alwan_ctx *ctx);
alwan_status alwan_xy_to_cct_cie_d_f64(alwan_f64 *cct_out, alwan_vec2_f64 const *xy);
```

`alwan_cct_to_uv_krystek1985` is Krystek's 1985 rational fit of the Planckian locus in
CIE 1960 uv, valid 1000 K to 15000 K. `alwan_cct_to_uv_planck1900` computes the locus
itself: Planck's law with c2 = 1.4388e-2 m K summed against the observer's 1 nm CMFs,
as colour-science's `CCT_to_uv_Planck1900`.

The two inverses return the CCT whose locus point is nearest the input: in uv on
Krystek's curve over 1000 K to 15000 K, in xy on the CIE daylight locus
(`alwan_d_series_illuminant_xy`) over 4000 K to 25000 K. They sample the range evenly
in mired, then bisect on the derivative of the squared distance, so the answer is the
exact minimum. colour-science's `uv_to_CCT_Krystek1985` and `xy_to_CCT_CIE_D` run a
general minimiser and land within 5e-5 K of it (suite 133). A point whose nearest
locus point lies past the range is `ALWAN_E_RANGE`; a point on the locus at the range
end itself is in range. The daylight locus switches formula at 7000 K, and the
inverse searches each side separately.

All take f32 twins; the searches and Planck's sum run in double.

---

## Color Rendering Index (CRI)

### alwan_cri_ra

```c
alwan_f32 alwan_cri_ra_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);
alwan_f64 alwan_cri_ra_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
```

CIE Color Rendering Index Ra: the average of the first eight test colour samples. Returns
Ra, or -1 on error.

Ra is not clamped. CIE 13.3 lets it go well below zero for a source that renders badly, so
-1 is a reachable score as well as this call's error value. Where that ambiguity matters,
use `alwan_cri_specification`, which reports failure as a status.

The `ALWAN_ILLUMINANT_LS_*` lamps are the set that exercises this. A discharge lamp with
no continuum to speak of scores far below zero on Ra, and further below on R9, which is
the reading those numbers exist to give.

**Example:**
```c
alwan_spd_f64 led_spd;
alwan_spd_illuminant_f64(&led_spd, ALWAN_ILLUMINANT_LED_B1, ctx);

alwan_f64 cri = alwan_cri_ra_f64(&led_spd, ctx);
printf("CRI Ra = %.0f\n", cri);  /* e.g., 82 */

alwan_spd_destroy_f64(&led_spd, ctx);
```

### alwan_cri_specification

```c
#define ALWAN_CRI_SAMPLES 14

typedef struct {
    alwan_f64 ra;                     /* average of rs[0] to rs[7], as alwan_cri_ra */
    alwan_f64 rs[ALWAN_CRI_SAMPLES];  /* R1 to R14 */
} alwan_cri_f64;                      /* alwan_cri_f32 is the f32 twin */

alwan_status alwan_cri_specification_f32(alwan_cri_f32 *spec_out,
                                         alwan_spd_f32 const *test_spd, alwan_ctx *ctx);
alwan_status alwan_cri_specification_f64(alwan_cri_f64 *spec_out,
                                         alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
```

The fourteen special indices of CIE 13.3-1995, in the standard's own order: the eight
muted samples that Ra averages, then saturated red, saturated yellow, saturated green and
saturated blue, then Caucasian skin and leaf green. `ra` is the average of the first
eight, the same number `alwan_cri_ra` returns, from the same pipeline.

R9, the saturated red, is what an averaged score cannot show. A narrow band fluorescent
renders the muted samples tolerably and carries almost no red: FL2 scores Ra 64 and R9
-84. Nothing is clamped, so those negative numbers arrive as they are.

Computing the specification costs six more sample integrations than `alwan_cri_ra`, which
stops at the eight samples it needs.

---

## Color Quality Scale (CQS)

### alwan_cqs_specification

```c
typedef enum { ALWAN_CQS_9_0 = 0, ALWAN_CQS_7_4 = 1 } alwan_cqs_version;
#define ALWAN_CQS_SAMPLES 15

typedef struct {
    alwan_f64 qa, qf, qp, qg, qd;                    /* qp, qd NaN under 9.0 */
    alwan_f64 cct, duv;                              /* Ohno 2013 */
    alwan_f64 cct_factor;                            /* 1 under 9.0 */
    alwan_f64 qas[ALWAN_CQS_SAMPLES];                /* each sample's Qa */
    alwan_f64 delta_c[ALWAN_CQS_SAMPLES];            /* C*ab test - reference */
    alwan_f64 delta_e[ALWAN_CQS_SAMPLES];            /* dE*ab */
    alwan_f64 delta_ep[ALWAN_CQS_SAMPLES];           /* dE*ab with a chroma gain removed */
    alwan_f64 lab_test[ALWAN_CQS_SAMPLES][3];        /* adapted, against the reference white */
    alwan_f64 lab_reference[ALWAN_CQS_SAMPLES][3];
    alwan_f64 gamut_test, gamut_reference;           /* a*b* polygon areas */
} alwan_cqs_f64;                                     /* alwan_cqs_f32 is the f32 twin */

alwan_status alwan_cqs_specification_f32(alwan_cqs_f32 *spec_out, alwan_spd_f32 const *test_spd,
                                         alwan_cqs_version version, alwan_ctx *ctx);
alwan_status alwan_cqs_specification_f64(alwan_cqs_f64 *spec_out, alwan_spd_f64 const *test_spd,
                                         alwan_cqs_version version, alwan_ctx *ctx);
```

The NIST Colour Quality Scale (Davis and Ohno), versions 9.0 and 7.4, as colour-science's
`colour_quality_scale` computes them, step for step: suite 257 holds every field to it
within 4e-11 on 54 sources.

- The test SPD is read on 360-780 nm at 1 nm, linearly between its samples and held at its
  end values beyond them. That is colour's reading of its own illuminant datasets; a
  spectrum given to colour with its default Sprague interpolator reads slightly differently.
- CCT by Ohno 2013, on a Planckian table from the CIE 1931 CMFs over 360-780 nm.
- The reference is a Planckian radiator below 5000 K, else CIE daylight with M1 and M2
  rounded to three decimals.
- Each of the 15 VS samples under the test source is adapted to the reference by the von
  Kries CMCCAT2000 matrix and taken to CIELAB against the reference white.
- `delta_ep` removes a chroma gain: the square root of dE^2 - dC^2 where dC > 0, else dE.
  `qa` is `10 ln(1 + exp((100 - s rms(delta_ep)) / 10))` times the CCT factor, with `s` 3.2
  (9.0) or 3.104 (7.4); `qf` is the same from `delta_e` with 2.93 x 1.0343 or 2.928; `qg` is
  the test samples' a*b* gamut area over 8210, times 100.
- Only 7.4 has a CCT factor: the reference samples' gamut area at D65 over 8210, at most 1.
  It also has `qp` and `qd`. 9.0 defines neither, and leaves them NaN.

**Returns:** `ALWAN_OK`; `ALWAN_E_INVALID` for a NULL, an unknown version or an SPD with
fewer than two samples or a non-finite value; `ALWAN_E_RANGE` when the SPD has no
luminance or its chromaticity falls off the Planckian table; `ALWAN_E_NODATA` when the VS
reflectances were compiled out.

### alwan_cqs_calculate

```c
alwan_f32 alwan_cqs_calculate_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);
alwan_f64 alwan_cqs_calculate_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
```

`qa` of NIST CQS 9.0, as `alwan_cqs_specification`, or -1 on any failure (the 2.0.0
contract). Until 2026-09-25 this ran an approximation of its own, up to 0.24 below
colour-science (HP1: 33.466 against 33.702).

---

## TM-30 / CIE 224 Fidelity Index

### alwan_tm30_rf

```c
alwan_f32 alwan_tm30_rf_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);
alwan_f64 alwan_tm30_rf_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
```

ANSI/IES TM-30-18 and CIE 224:2017 colour fidelity Rf, as colour-science's
`colour_fidelity_index_CIE2017`, step for step (suite 257, within 3e-11 of it):

- The test SPD is taken on 380-780 nm at its own interval when that is 1 or 5 nm, linearly
  between its samples and zero beyond them. Any other interval is read at 1 nm; colour
  refuses intervals over 5 nm.
- CCT by Ohno 2013 on a CIE 1931 table from 1000 to 25000 K.
- The reference is Planckian below 4000 K and CIE daylight above 5000 K (M1 and M2 rounded,
  the basis Sprague-interpolated at 1 nm). Between those two temperatures it is the two
  blended by luminance.
- The 99 CIE 2017 test colour samples, for that interval, go through the CIE 1964 10 deg
  observer, CIECAM02 (L_A 100, Y_b 20, average surround, illuminant discounted) and
  CAM02-UCS. Rf is `10 ln(1 + exp((100 - 6.73 mean dE) / 10))`.

The interval matters. CIE 2017 publishes its samples at 1 nm and at 5 nm, and the same lamp
scores differently on each: colour's HP1 at 5 nm is Rf 34.19, and the same spectrum read at
1 nm is 34.49. `alwan_spd_illuminant` returns 1 nm spectra, so a comparison against colour
has to be made on those.

Returns [0, 100], or -1 on error. Until 2026-09-25 this resampled every source to 360-830
nm at 5 nm, used trapezoid sums and a Robertson CCT, and blended no references: Rf up to
4e-3 and Rg up to 6e-4 from colour-science on the same input.

### alwan_tm30_specification

```c
#define ALWAN_TM30_HUE_BINS 16
#define ALWAN_TM30_SAMPLES 99

typedef struct {
    alwan_f64 rf;                                          /* general fidelity */
    alwan_f64 rg;                                          /* gamut index */
    alwan_f64 rfs[ALWAN_TM30_HUE_BINS];                    /* local fidelity */
    alwan_f64 rcs[ALWAN_TM30_HUE_BINS];                    /* local chroma shift, percent */
    alwan_f64 rhs[ALWAN_TM30_HUE_BINS];                    /* local hue shift */
    alwan_f64 average_norms[ALWAN_TM30_HUE_BINS];          /* length of each reference average */
    alwan_f64 averages_test[ALWAN_TM30_HUE_BINS][2];       /* (a', b') under the test source */
    alwan_f64 averages_reference[ALWAN_TM30_HUE_BINS][2];  /* (a', b') under the reference */
    int bins[ALWAN_TM30_SAMPLES];                          /* the bin each sample fell in */
    alwan_f64 cct, duv;                                    /* Ohno 2013, the table to 25000 K */
    alwan_f64 rs[ALWAN_TM30_SAMPLES];                      /* each sample's fidelity, R_s */
    alwan_f64 delta_e[ALWAN_TM30_SAMPLES];                 /* each sample's CAM02-UCS difference */
    alwan_f64 jab_test[ALWAN_TM30_SAMPLES][3];             /* J', a', b' under the test source */
    alwan_f64 jab_reference[ALWAN_TM30_SAMPLES][3];        /* and under the reference */
} alwan_tm30_f64;                                          /* alwan_tm30_f32 is the f32 twin */

alwan_status alwan_tm30_specification_f32(alwan_tm30_f32 *spec_out,
                                          alwan_spd_f32 const *test_spd, alwan_ctx *ctx);
alwan_status alwan_tm30_specification_f64(alwan_tm30_f64 *spec_out,
                                          alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
```

The rest of ANSI/IES TM-30-18, where `alwan_tm30_rf` gives only the headline number. The
99 colour evaluation samples are sorted into 16 hue bins by where each one lands under the
reference illuminant, and the rendition is then reported bin by bin.

`rg` is 100 times the ratio of the areas the test and reference polygons enclose: above
100 the source saturates on average, below 100 it dulls. `rcs` is signed, positive where
the bin gains chroma. `rhs` is the component of the shift along the bin's bisector, and is
not an angle. `averages_test` and `averages_reference` are the two polygons' vertices,
which is what a colour vector graphic draws; `average_norms` is the length of each
reference vertex, so a caller plotting the graphic can normalise the test vertex against
the circle without recomputing it.

A bin with no samples in it leaves its entries at zero, where colour-science has NaN. No
real source empties one; the 99 samples were chosen so that none can.

`rs`, `delta_e`, `jab_test`, `jab_reference`, `cct` and `duv` are colour-science's `R_s`,
`delta_E_s`, the two `Jpapbp` sets, `CCT` and `D_uv`: each sample's own fidelity and where it
lands in CAM02-UCS under the two sources.

Both entry points cost the same as `alwan_tm30_rf`, which runs the same pipeline and
discards everything but `rf`.

### alwan_cie224_rf

```c
alwan_f32 alwan_cie224_rf_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);
alwan_f64 alwan_cie224_rf_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
```

CIE 224:2017 Colour Fidelity Index: the same computation as `alwan_tm30_rf`, which it
calls. Returns [0, 100], or -1 on error.

---

## Spectral Similarity Index (SSI)

### alwan_ssi_calculate

```c
alwan_f32 alwan_ssi_calculate_f32(alwan_spd_f32 const *test_spd,
                                  alwan_spd_f32 const *reference_spd,
                                  alwan_ctx *ctx);
alwan_f64 alwan_ssi_calculate_f64(alwan_spd_f64 const *test_spd,
                                  alwan_spd_f64 const *reference_spd,
                                  alwan_ctx *ctx);
```

Academy Spectral Similarity Index (Holm and Maier 2016). Measures spectral similarity
between two light sources. Returns [0, 100] where 100 = perfect match, or negative on
error.

Both SPDs are resampled to 1 nm over 375-675 nm, integrated into thirty 10 nm bins
centred on 380 to 670 nm (half weight at each bin's edges) and normalised; the weighted
relative difference is smoothed with [0.22, 0.56, 0.22], zero beyond the ends. This is
colour-science's `spectral_similarity_index`, which alwan matches unrounded (colour rounds
to an integer by default).

---

## Metamerism Index

### alwan_metamerism_index

```c
alwan_f32 alwan_metamerism_index_f32(alwan_spd_f32 const *sample_reflectance,
                                     alwan_spd_f32 const *reference_reflectance,
                                     alwan_spd_f32 const *reference_illuminant,
                                     alwan_spd_f32 const *test_illuminant,
                                     alwan_observer_type observer,
                                     alwan_ctx *ctx);
alwan_f64 alwan_metamerism_index_f64(alwan_spd_f64 const *sample_reflectance,
                                     alwan_spd_f64 const *reference_reflectance,
                                     alwan_spd_f64 const *reference_illuminant,
                                     alwan_spd_f64 const *test_illuminant,
                                     alwan_observer_type observer,
                                     alwan_ctx *ctx);
```

CIE special metamerism index, change in illuminant (CIE 015): the colour difference under
a test illuminant of two specimens that match under a reference illuminant. A pair that
does not match exactly under the reference gets CIE 015's multiplicative correction: the
sample's X, Y, Z under the test illuminant are each multiplied by reference / sample under
the reference illuminant, so the residual reference mismatch is not counted. The index is
DeltaE*ab between the corrected sample and the reference under the test illuminant, against
its white; negative on error, including a sample with a zero tristimulus value under the
reference illuminant. Two greys of different lightness therefore give 0.

---

## Whiteness & Yellowness Indices

### ASTM E313 Illuminant/Observer Pairs

```c
typedef enum {
    ALWAN_ASTM_E313_C_2DEG = 0,    /* Illuminant C, 2-degree observer */
    ALWAN_ASTM_E313_D65_2DEG = 1,  /* Illuminant D65, 2-degree observer */
    ALWAN_ASTM_E313_C_10DEG = 2,   /* Illuminant C, 10-degree observer */
    ALWAN_ASTM_E313_D65_10DEG = 3  /* Illuminant D65, 10-degree observer */
} alwan_astm_e313_illuminant;
```

### alwan_yellowness_astm_e313

```c
alwan_status alwan_yellowness_astm_e313_{T}(alwan_{T} *yi_out, alwan_xyz_{T} const *xyz,
                                            alwan_astm_e313_illuminant illuminant);
```

ASTM E313 Yellowness Index, YI = 100 (Cx X - Cz Z) / Y. Input XYZ must be normalized to
Y=100 for perfect white. `ALWAN_E_INVALID` for a NULL or an illuminant outside the enum,
`ALWAN_E_RANGE` for Y at zero. Until 3.0.0 the three functions of this group returned the
value and signalled failure as -1, which a yellowness or whiteness index can legitimately
be; they now take the status-and-out form the rest of the family (Berger, Taube, Stensby,
Ganz, ASTM D1925) already had, and suite 31 pins that -1 comes back as -1 with `ALWAN_OK`.

### alwan_whiteness_astm_e313

```c
alwan_status alwan_whiteness_astm_e313_{T}(alwan_{T} *wi_out, alwan_xyz_{T} const *xyz,
                                           alwan_astm_e313_illuminant illuminant);
```

ASTM E313 Whiteness Index, WI = 3.388 Z - 3 Y. Input XYZ must be normalized to Y=100 for
perfect white. The formula has no illuminant term; the parameter sits beside the
yellowness one and must still be inside the enum (`ALWAN_E_INVALID` otherwise, or for a
NULL).

### alwan_whiteness_cie2004

```c
alwan_status alwan_whiteness_cie2004_{T}(alwan_{T} *w_out, alwan_vec2_{T} const *xy, alwan_{T} Y,
                                         alwan_vec2_{T} const *xy_n);
```

CIE 2004 Whiteness Index, W = Y + 800 (xn - x) + 1700 (yn - y). `w_out` receives W; the
tint is not returned here (`alwan_whiteness_ganz1979` returns a tint with its whiteness).
`ALWAN_E_INVALID` for a NULL.

**Example:**
```c
alwan_xyz_f64 paper = {93.0, 95.0, 101.0};  /* Slightly blue-white */
alwan_f64 yi, wi;

if (alwan_yellowness_astm_e313_f64(&yi, &paper, ALWAN_ASTM_E313_D65_2DEG) == ALWAN_OK &&
    alwan_whiteness_astm_e313_f64(&wi, &paper, ALWAN_ASTM_E313_D65_2DEG) == ALWAN_OK) {
    printf("YI = %.1f, WI = %.1f\n", yi, wi);
}
```

---

## Error Codes

Every function on this page that can fail returns `alwan_status` with an out parameter;
the CRI, CQS and TM-30 scores that return a value directly cannot fail on a valid
descriptor and say so in their own entries.

- `ALWAN_OK` (0): Success
- `ALWAN_E_INVALID` (-1): NULL pointer or unsupported parameter
- `ALWAN_E_NODATA` (-2): Required spectral data not loaded

---

## See Also

- [Spectral Operations](spectral.md): SPD creation and integration
- [Chromatic Adaptation](chromatic-adaptation.md): White point transforms
- [Color Difference](color-difference.md): DeltaE metrics
