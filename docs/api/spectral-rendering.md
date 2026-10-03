# Spectral Rendering: Wavelength Sampling and the Spectral Film

Two pieces a spectral renderer needs around its light transport: a way to pick the
wavelengths a path carries, with their pdfs, and a film that turns the radiance those
paths bring back into XYZ and RGB.

A path traced at wavelength λ with density p(λ) brings back radiance L(λ). The film
estimates the pixel's colour as the mean over paths of L(λ) cmf(λ) / p(λ), which
converges to the integral of L times the colour matching functions. Picking λ where the
CMFs are large lowers the variance; carrying several wavelengths a path (hero sampling)
lowers it further for the same number of paths.

Header: `alwan.h`. Per-sample math: `core/alwan_wavelength_sampling_core.h` (C, CUDA,
OpenCL, HLSL, GLSL). Suite 287.

## Wavelength samplers

```c
typedef enum {
    ALWAN_WAVELENGTH_PDF_UNIFORM = 0,
    ALWAN_WAVELENGTH_PDF_VISIBLE = 1,
    ALWAN_WAVELENGTH_PDF_TABULATED = 2
} alwan_wavelength_pdf;

alwan_status alwan_wavelength_sampler_create_{T}(alwan_wavelength_sampler_{T} **out, alwan_wavelength_pdf pdf,
                                                 alwan_wavelength_sampler_params_{T} const *params, alwan_ctx *ctx);
void alwan_wavelength_sampler_destroy_{T}(alwan_wavelength_sampler_{T} *sampler, alwan_ctx *ctx);
alwan_status alwan_wavelength_sample_{T}(T *lambda_out, T *pdf_out, T u, alwan_wavelength_sampler_{T} const *sampler);
alwan_status alwan_wavelength_sample_{T}_map_interleave(T *out, size_t out_stride, T const *in, size_t in_stride,
                                                        size_t count, alwan_wavelength_sampler_{T} const *sampler);
alwan_status alwan_wavelength_pdf_{T}(T *pdf_out, T lambda, alwan_wavelength_sampler_{T} const *sampler);
alwan_status alwan_wavelength_invert_{T}(T *u_out, T lambda, alwan_wavelength_sampler_{T} const *sampler);
```

The densities, over `[lambda_min, lambda_max]` (360 to 830 nm when both are 0):

| Density | p(λ) | Sampling |
|---|---|---|
| `UNIFORM` | 1 / (max - min) | linear |
| `VISIBLE` | a sech²(a (λ - b)) / (tanh(a (max - b)) - tanh(a (min - b))) | exact inverse CDF: λ = b + atanh(t0 + u (t1 - t0)) / a |
| `TABULATED` | the caller's weights, constant on equal bins | a piecewise-constant table (`alwan_wavelength_tabulated_mode`) |

`VISIBLE` is the density of Radziszewski, Boryczko and Alda 2009, "An improved technique
for full spectral rendering" (Journal of WSCG 17), a = 0.0072 per nm and b = 538 nm by
default (`visible_a`, `visible_b`). It is a smooth bump that covers the three CMFs, and
its CDF is a tanh, so the inverse is closed form.

`TABULATED` takes `weight_count` weights, one per bin of equal width, and draws with the
modes of `alwan_wavelength_tabulated_mode` (`tabulated_mode`: `ALWAN_WAVELENGTH_TABULATED_DIRECT`,
`_SEARCH`, `_ALIAS`, the same values the 2D importance sampler used): `SEARCH` and `ALIAS` exact and with the same pdf to
the bit, `DIRECT` a tabulated inverse whose pdf is the density it actually draws (so
estimates stay unbiased). The weights can come from an observer and a light:

```c
typedef enum { ALWAN_WAVELENGTH_WEIGHT_XYZ = 0, ALWAN_WAVELENGTH_WEIGHT_Y = 1 } alwan_wavelength_weight;

alwan_status alwan_wavelength_weights_{T}(T *weights_out, size_t count, T lambda_min, T lambda_max,
                                          alwan_wavelength_weight weight, alwan_observer_type observer,
                                          alwan_illuminant illuminant, alwan_ctx *ctx);
```

Each bin is the mean of x̄ + ȳ + z̄ (or ȳ) times the illuminant's SPD over the bin;
`ALWAN_ILLUMINANT_E` gives the CMFs alone.

`invert` returns the u that `sample` maps to λ (exact for all but `TABULATED` with
`ALIAS`, whose map is not monotone: `ALWAN_E_INVALID`).

## Hero wavelengths

```c
alwan_status alwan_wavelength_sample_hero_{T}(T *lambda_out, T *pdf_out, T *mis_out, size_t count,
                                              T u, alwan_wavelength_sampler_{T} const *sampler);
```

Wilkie, Nawaz, Droske, Weidlich and Hanika 2014, "Hero wavelength spectral sampling": the
hero is drawn from u, and `count - 1` more wavelengths are the hero moved by j / count of
the range, wrapped. `pdf_out[j]` is the sampler's density at wavelength j and
`mis_out[j]` its balance-heuristic weight, `pdf_j / sum_k pdf_k`. Every rotation of the
set is the set itself, so one sum serves every wavelength and the weights sum to 1. A path
that carries `L_j` at the `count` wavelengths contributes

    sum_j L_j mis_j / pdf_j = sum_j L_j / (pdf_0 + ... + pdf_{count-1})

so give the film `weight_j = mis_j / pdf_j`.

The core has `alwan_wavelength_hero_rotate_{f32,f64}_v` and the closed-form densities for
a shader; a shader using a tabulated density reads its pdf through the 2D sampler's
reader.

## The spectral film

```c
typedef enum {
    ALWAN_SPECTRAL_FILM_NORMALIZE_NONE = 0,
    ALWAN_SPECTRAL_FILM_NORMALIZE_ILLUMINANT = 1
} alwan_spectral_film_normalize;

typedef struct {
    alwan_observer_type observer;          /* 0: CIE 1931 2 deg */
    alwan_interp_method interpolation;     /* 0: LINEAR, how the 1 nm CMF table is read */
    alwan_spectral_film_normalize normalize;
    alwan_illuminant illuminant;           /* NORMALIZE_ILLUMINANT */
    int track_variance;
} alwan_spectral_film_params;

alwan_status alwan_spectral_film_create(alwan_spectral_film **out, size_t width, size_t height,
                                        alwan_spectral_film_params const *params, alwan_ctx *ctx);
void alwan_spectral_film_destroy(alwan_spectral_film *film, alwan_ctx *ctx);
alwan_status alwan_spectral_film_clear(alwan_spectral_film *film);
alwan_status alwan_spectral_film_add_{T}(alwan_spectral_film *film, size_t x, size_t y, T const *lambda,
                                         T const *radiance, T const *weight, size_t count);
alwan_status alwan_spectral_film_add_image_{T}(alwan_spectral_film *film, T const *lambda, T const *radiance,
                                               T const *weight, size_t count);
alwan_status alwan_spectral_film_resolve_xyz_{T}(T *out, size_t row_stride, alwan_spectral_film const *film);
alwan_status alwan_spectral_film_resolve_rgb_{T}(T *out, size_t row_stride, alwan_spectral_film const *film,
                                                 alwan_rgb_space space, int adapt, alwan_ctx *ctx);
alwan_status alwan_spectral_film_resolve_variance_{T}(T *out, size_t row_stride, alwan_spectral_film const *film);
alwan_status alwan_spectral_film_path_count(size_t *count_out, alwan_spectral_film const *film, size_t x, size_t y);
```

One `add` is one path: its wavelengths, the radiance at each and each one's weight (1 / pdf
for a single wavelength, `mis / pdf` for hero sampling; `weight` NULL for all 1). The
pixel's XYZ is the mean of `sum_j radiance_j weight_j cmf(lambda_j)` over its paths. The
CMFs are the observer's 1 nm table, read with `interpolation` between samples (LINEAR by
default; SPRAGUE, LANCZOS and the cubic methods work on its uniform grid) and 0 outside
360 to 830 nm. Accumulation is in double whatever the inputs; a path with a non-finite
value is refused whole.

With `NORMALIZE_ILLUMINANT` the result is divided by the integral of the illuminant's SPD
times ȳ, so a path whose radiance is that SPD (a perfect diffuser lit by it) resolves to
its white with Y = 1. `resolve_rgb` with `adapt` non-zero then adapts by Bradford from that
white to the space's, so a perfect diffuser is (1, 1, 1) in any space. With
`NORMALIZE_NONE` the film's white is the observer's equal-energy white. The normalising
integrals are taken on the film's own reading of the CMFs at 0.05 nm, the SPD read
linearly.

`track_variance` keeps Welford's running mean and M2 per pixel and channel;
`resolve_variance` writes the variance of each pixel's mean, M2 / (n - 1) / n (0 under two
paths), the square of the standard error a renderer can use to stop sampling.

## Accuracy (suite 287)

The ground truth is the exact integral of radiance times the CMFs as the film reads them:
radiance is a product of SPDs read linearly, the CMFs are linear between 1 nm samples, so
the integrand is a cubic between merged breakpoints and Simpson's rule is exact.

- Each density integrates to 1 (UNIFORM 2e-13, VISIBLE 1e-14, TABULATED exactly; DIRECT's
  own density 9e-16), `invert(sample(u))` returns u to 6e-16, and the pdf `sample` returns
  is the pdf at the sample.
- SEARCH and ALIAS give the same tabulated pdf to the bit.
- Hero weights sum to 1 within 3.3e-16; rotations are a quarter of the range apart
  within 1.1e-13 nm.
- 29 spectra (D65, A, F11, a 5000 K blackbody, a 2 nm laser line at 532 nm and the 24
  ColorChecker patches of Ohta 1997 under D65), each with four strategies at 16384 paths:
  every XYZ estimate is inside 3 standard errors of the truth (worst 2.98).
- A perfect diffuser under D65 or A resolves to (1, 1, 1) within 5e-8 in sRGB, BT.2020 and
  ACEScg; under A without adaptation it stays warm (1.85, 0.83, 0.23 in sRGB).

RMS relative error of Y over 64 runs at 4096 paths:

| Spectrum | uniform | visible | CMF x D65 table | hero x4 visible |
|---|---|---|---|---|
| D65 | 2.3e-2 | 1.5e-2 | 1.2e-2 | 2.2e-3 |
| F11 (fluorescent) | 4.3e-2 | 3.6e-2 | 3.6e-2 | 1.9e-2 |
| laser 532 nm | 2.5e-1 | 1.8e-1 | 2.1e-1 | 1.4e-1 |
| ColorChecker blue | 2.1e-2 | 1.2e-2 | 1.4e-2 | 1.1e-3 |

Hero sampling evaluates four wavelengths a path, so at 4096 paths it has done as much
work as 16384 single wavelengths; it still wins there (2.2e-3 against 6.3e-3 for VISIBLE
at 16384 on D65), because its four wavelengths are stratified across the range. A spiky
spectrum (F11, the laser line) gains least: no density shaped by the CMFs knows where the
spikes are, and only one shaped by the light itself would.

## Determinism

`VISIBLE` reads a tanh and a log. The deterministic build evaluates those with its own
polynomials, so its wavelengths differ from the ordinary build's in the last bits (every
check above holds to the same bars in both); each build repeats its own samples bit for
bit. `UNIFORM`, `TABULATED` and the film use only + - * / and comparisons.

## The older hero functions

`alwan_hero_wavelength_sample_{T}`, `alwan_hero_wavelength_batch_{T}` and
`alwan_hero_wavelength_to_xyz_{T}` remain: uniform over 380 to 780 nm and Wyman, Sloan and
Shirley's analytic CMF fit, with no pdf choice. The sampler above is the general form.
