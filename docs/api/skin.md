# Skin

The diffuse spectral reflectance of skin from what colours it, melanin in the epidermis and
blood in the dermis, and that reflectance's colour in any RGB space, observer and
illuminant. Use it as a renderer's albedo, to see how a skin tone moves under a different
light, or to estimate the chromophores behind a measured spectrum.

## The model

Two layers by Kubelka-Munk theory, after Doi and Tominaga ("Spectral estimation of human
skin color using the Kubelka-Munk theory", Proc. SPIE 5008, 2003): an epidermis of thickness
d over a dermis thick enough to count as semi-infinite. Light enters diffuse and leaves
diffuse. alwan's own code from published coefficients:

| Quantity | Value | Source |
|---|---|---|
| Eumelanin absorption | 6.6e10 lambda^-3.33 mm^-1 (lambda in nm) | Donner and Jensen 2006 |
| Pheomelanin absorption | 2.9e14 lambda^-4.75 mm^-1 | Donner and Jensen 2006 |
| Bloodless tissue | 0.0244 + 8.53 exp(-(lambda - 154) / 66.2) mm^-1 | Jacques 2013 |
| Blood | ln(10) eps(lambda) c, c = haemoglobin / 64458 g/mol | the caller's eps spectra |
| Reduced scattering, both layers | 4.6 (lambda / 500)^-1.421 mm^-1 | Jacques 2013, skin |
| Kubelka-Munk K, S | K = 2 mu_a, S = 3/4 mu_s' - 1/4 mu_a | Star et al. 1988 |

The epidermis absorbs Cm (bm mu_eu + (1 - bm) mu_pheo) + (1 - Cm) mu_base, the dermis
Ch (g mu_oxy + (1 - g) mu_deoxy) + (1 - Ch) mu_base, with Cm the melanin fraction, bm the
eumelanin share, Ch the blood fraction and g the oxygenation. With a = (S + K) / S and
b = sqrt(a^2 - 1), the epidermis reflects R1 = sinh(bSd) / (a sinh(bSd) + b cosh(bSd)) and
transmits T1 = b / (a sinh(bSd) + b cosh(bSd)); the dermis reflects R2 = a - b; together
R = R1 + T1^2 R2 / (1 - R1 R2).

## Haemoglobin spectra: supplied by the caller

alwan ships no haemoglobin table. The usual source, Scott Prahl's compilation of the molar
extinction of oxy- and deoxyhaemoglobin (omlc.org, "Tabulated Molar Extinction Coefficient
for Hemoglobin in Water", from W. B. Gratzer and N. Kollias), carries a copyright notice and
no licence, so it is not something an MIT library can redistribute. Until 3.0.0 alwan shipped
it through the Virtual Tissue Simulator's MIT-licensed copy; that table was removed.

Give the model the two spectra in `alwan_skin_params`:

- `haemoglobin_wavelengths_nm`: one strictly increasing grid, in nm;
- `haemoglobin_oxy` and `haemoglobin_deoxy`: decadic molar extinction of HbO2 and Hb on that
  grid, in cm^-1 / M (Prahl's tables use these units; a table in cm^-1 / (mg/mL) or per
  millimolar needs converting);
- `haemoglobin_count`: the number of samples, at least 2;
- `interpolation` reads between samples (0 is LINEAR), `haemoglobin_extrapolation` beyond
  the grid (0 is ALWAN_EXTRAPOLATE_ZERO, no blood absorption there: give a grid covering
  the wavelengths you evaluate, 350 to 850 nm for the model and 360 to 830 for `rgb`, or
  choose CONSTANT).

The arrays are read during each call and not kept. With `haemoglobin_count` 0 the model has
no blood term: a `blood_fraction` of exactly 0 still evaluates, anything above it returns
ALWAN_E_NODATA, and so does fitting `_BLOOD` or `_OXYGENATION`. The model's own range stays
350 to 850 nm.

## Limits

- **A model, not a measurement.** The coefficients are published fits from different
  studies. The two melanin laws (Donner and Jensen) give 67.9 mm^-1 at 500 nm for pure
  eumelanin, 1.31 times Jacques 2013's independent melanosome law: published fits for the
  same pigment disagree by that much, and the melanin fraction that gives a particular
  skin is only as good as them.
- **Diffuse body reflectance only.** No surface reflection (about 2.8 % for an index near
  1.4: put it in the BRDF's specular lobe), no angular dependence, no subsurface transport:
  a renderer with subsurface scattering needs the absorption and scattering coefficients
  (`alwan_skin_absorption`), not this reflectance.
- **Kubelka-Munk's link from mu_a and mu_s'.** Star et al.'s S = 3/4 mu_s' - 1/4 mu_a
  comes from diffusion theory and goes negative when absorption is strong (a heavily
  pigmented epidermis at short wavelengths). alwan then takes S = 0, a pure absorber,
  which is the right limit but not a measured one.
- **No other chromophores.** Beta-carotene, bilirubin and water are left out; they matter
  little between 400 and 850 nm for most skin, but not for all.
- **Darker settings saturate.** With the defaults' 30 % pheomelanin, high melanin
  fractions give a yellower colour (larger b*) than lighter ones; the eumelanin share is
  the parameter that moves it. The model has not been fitted to a measured database here,
  and suite 289 does not claim it matches one.

## Parameters

```c
typedef enum { ALWAN_SKIN_KUBELKA_MUNK_2LAYER = 0 } alwan_skin_model;

typedef struct {
    alwan_skin_model model;
    alwan_f64 melanin_fraction;        /* epidermis volume fraction of melanosomes, 0..1 */
    alwan_f64 eumelanin_ratio;         /* eumelanin's share of the melanin, 0..1 */
    alwan_f64 blood_fraction;          /* dermis volume fraction of blood, 0..1 */
    alwan_f64 oxygenation;             /* oxyhaemoglobin's share, 0..1 */
    alwan_f64 epidermis_thickness_mm;  /* 0 = 0.1 mm */
    alwan_f64 haemoglobin_g_per_l;     /* 0 = 150 g/L */
    alwan_f64 scattering_per_mm;       /* reduced scattering at 500 nm; 0 = 4.6 */
    alwan_f64 scattering_power;        /* 0 = 1.421 */
    alwan_interp_method interpolation; /* the haemoglobin spectra; 0 = LINEAR */
    alwan_f64 const *haemoglobin_wavelengths_nm; /* grid, nm */
    alwan_f64 const *haemoglobin_oxy;            /* HbO2, cm^-1 / M */
    alwan_f64 const *haemoglobin_deoxy;          /* Hb, cm^-1 / M */
    size_t haemoglobin_count;                    /* 0 = none */
    alwan_extrapolate_mode haemoglobin_extrapolation; /* 0 = ZERO */
} alwan_skin_params;

void alwan_skin_params_default(alwan_skin_params *params);
```

The four fractions are taken as given: 0 means none, so a zeroed struct is unpigmented,
bloodless tissue. The other fields read 0 as the default in the comment.
`alwan_skin_params_default` gives melanin 0.03 (70 % eumelanin), blood 0.02 at 75 %
oxygenation, the remaining fields 0 and no haemoglobin spectra: set them before evaluating
(passing NULL for params means those defaults, so it returns ALWAN_E_NODATA).

For orientation, S. L. Jacques's "Skin Optics Summary" (omlc.org, 1998) gives epidermal
melanosome volume fractions of 1.3 to 6.3 % in lightly pigmented adults, 11 to 16 % in
moderately pigmented adults and 18 to 43 % in darkly pigmented adults. With this model's
melanin laws, fractions near the top of that range give very dark reflectances; see Limits.

## Functions

```c
alwan_status alwan_skin_absorption_{T}(T *mua_epidermis, T *mua_dermis, T *musp,
                                       T const *wavelengths_nm, size_t count,
                                       alwan_skin_params const *params);
alwan_status alwan_skin_reflectance_{T}(T *reflectance_out, T const *wavelengths_nm, size_t count,
                                        alwan_skin_params const *params);
alwan_status alwan_skin_rgb_{T}(alwan_rgb_{T} *rgb_out, alwan_xyz_{T} *xyz_out,
                                alwan_skin_params const *params, alwan_rgb_space space,
                                alwan_illuminant illuminant, alwan_observer_type observer,
                                alwan_ctx *ctx);
alwan_status alwan_skin_fit_{T}(alwan_skin_params *params, T *rms_out, T const *reflectance,
                                T const *wavelengths_nm, size_t count, unsigned fit);
```

- `absorption` gives each layer's absorption and the reduced scattering, mm^-1; any output
  may be NULL.
- `reflectance` gives the diffuse reflectance, in [0, 1].
- `rgb` takes the reflectance from 360 to 830 nm at 1 nm through `alwan_reflectance_to_rgb`
  (normalised to a perfect diffuser under the illuminant, adapted by Bradford to the
  space's white). `xyz_out` may be NULL.
- `fit` adjusts the fractions named in `fit` (`ALWAN_SKIN_FIT_MELANIN`, `_BLOOD`,
  `_OXYGENATION`, `_EUMELANIN`; 0 is the first three) by Levenberg-Marquardt from the
  values already in `params`, each held in [0, 1], and reports the RMS residual. Start
  from `alwan_skin_params_default`. A fit that ends with a large residual says the
  spectrum is not one this model can make.

Errors: ALWAN_E_RANGE for a wavelength outside 350-850 nm; ALWAN_E_INVALID for a fraction
outside [0, 1], a negative or non-finite field, a NULL buffer, a zero count, malformed
haemoglobin spectra (a NULL array, fewer than 2 samples, a grid not strictly increasing, a
negative or non-finite value, an unknown extrapolation mode) or an interpolation the grid
does not allow; ALWAN_E_NODATA for blood without spectra. The f32 forms compute in f64.

## Measured (suite 289)

The suite passes Prahl's spectra (fetched by alwan_dev's gendata at test-data time, not
shipped), 350 to 850 nm every 2 nm, so these are the model's numbers with that data.

- Coefficients at fixed points against the published formulas: exact to rounding;
  oxygenated blood at 576 nm 29.760 mm^-1 from Prahl's 55540 cm^-1/M.
- A 1 m epidermis reflects as its own R_inf to 3.3e-16, a vanishing one as the dermis's to
  4.2e-12.
- Melanin 0.02 to 0.10 lowers R at 450 nm by a factor 0.17 and at 700 nm by 0.59.
- Oxygenated blood: R(542) 0.339, R(560) 0.383, R(576) 0.328, the Q-band double dip;
  deoxygenated: one dip, R(555) 0.334 below R(542) 0.348 and R(576) 0.360.
- The defaults in linear sRGB under D65: 0.548, 0.344, 0.224 (XYZ 0.390, 0.379, 0.264).
- A fit from the defaults recovers melanin 0.08, blood 0.035 and oxygenation 0.6 to six
  digits, residual 9e-18.
- f32 against f64: 3.0e-8.
