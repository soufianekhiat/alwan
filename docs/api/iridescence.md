# Thin-Film Iridescence

The colour of a thin transparent film over a base, as the Fresnel term of a microfacet
BRDF: soap on water, oil on asphalt, the oxide that colours heated titanium or steel, the
anti-reflection coating of a lens. After Belcour and Barla, "A Practical Extension to
Microfacet Theory for the Modeling of Varying Iridescence" (ACM TOG 36(4), SIGGRAPH 2017).

Light reflected at the film's top and light that crossed the film and came back from the
base interfere, and how depends on the wavelength: the reflectance spectrum oscillates,
faster the thicker the film. Sampling that spectrum every few nanometres aliases once the
film is a micrometre or so thick and the fringes are finer than the samples. Belcour and
Barla's step is to write the reflectance as a Fourier series in the film's round-trip
phase and to integrate each term against the colour matching functions in closed form: no
spectral sampling, and a thick film averages to its true colour.

alwan's own implementation, from the paper's equations, with two differences from the
paper:

- **The exact transform, not a Gaussian fit.** The paper fits the CMFs with Gaussians to
  integrate them in closed form. alwan takes the CMFs times the illuminant as piecewise
  linear in wavenumber between their 1 nm samples and integrates that exactly, segment by
  segment, into a table of its Fourier transform, once per colour space, illuminant and
  observer.
- **Spectral bands.** The paper holds the indices constant over the spectrum. alwan can
  split the spectrum into bands of equal colour weight, each with its own table and its
  own indices, so a dispersive film keeps its colour (below).

```c
#include "core/alwan_iridescence_core.h"      /* the per-direction math */
#include "core/alwan_iridescence_reader.inc"  /* the series and the table lookup, for a shader */
```

Suite 288.

## The film

```c
typedef struct {
    double ambient_n;      /* 0 for 1 (vacuum or air) */
    double film_n;         /* the film's real index */
    double thickness_nm;
    double base_n;
    double base_k;         /* 0 for a dielectric base */
} alwan_iridescent_film;
```

The ambient and the film are lossless; the base may absorb (a metal). With
`s = n1 sin(theta1)`, the normal components are `q1 = n1 cos(theta1)`,
`q2 = sqrt(n2^2 - s^2)` and `q3 = sqrt(n3^2 - s^2)` (complex for an absorbing base, the
forward root), Fresnel's coefficients `r12` and `r23` follow in the sign convention of
`alwan_fresnel` and `alwan_multilayer_tmm`, and the reflected amplitude is Airy's

```
r = (r12 + r23 e^(i delta)) / (1 + r12 r23 e^(i delta)),   delta = 4 pi d q2 / lambda
```

## The series

With `a = r12` (real) and `r23 = rho e^(i phi)`,

```
R = C0 + sum_{m >= 1} c_m rho^m cos(m (delta + phi))
C0  = (a^2 + rho^2 - 2 a^2 rho^2) / (1 - a^2 rho^2)
c_m = -2 K (-a)^m,   K = (1 - a^2)(1 - rho^2) / (1 - a^2 rho^2)
```

the expansion of `R` in its phase, which is Belcour and Barla's equation 9. Each term's
colour is `Re(r23^m G(m OPD))`, where `OPD = 2 d q2` is the optical path difference and
`G(f) = integral of w(lambda) e^(i 2 pi f / lambda) d lambda` is the Fourier transform in
wavenumber of a colour weight `w` (a CMF times the illuminant). Terms are added until one
falls below the table's tolerance (`1e-7`, at most 64 terms by default): the series
converges like `(a rho)^m`, a few terms for most films.

Past total internal reflection at the film's top (an ambient denser than the film, at a
grazing angle) the wave in the film is evanescent and nothing oscillates, so the series
does not apply. A thin film still lets light tunnel to the base (frustrated total internal
reflection). Each band's share is then the exact reflectance at the band's wavelength
times the band's white. With one band that is a rough approximation; with several it
follows the spectrum (measured below).

## Creating the table

```c
typedef struct {
    double opd_max_nm;     /* 0: 40000 */
    double opd_step_nm;    /* 0: 2 */
    int max_terms;         /* 0: 64, at most 256 */
    double tolerance;      /* 0: 1e-7 */
    int bands;             /* 0: 1, at most ALWAN_IRIDESCENCE_MAX_BANDS (32) */
} alwan_iridescence_params;

alwan_status alwan_iridescence_create(alwan_iridescence **out, alwan_rgb_space space,
                                      alwan_illuminant illuminant, alwan_observer_type observer,
                                      alwan_iridescence_params const *params, alwan_ctx *ctx);
void alwan_iridescence_destroy(alwan_iridescence *s, alwan_ctx *ctx);
alwan_status alwan_iridescence_get_info(alwan_iridescence_info *info, alwan_iridescence const *s);
```

The colour weights are normalised so that a reflectance of 1 has `Y = 1`, adapted by
Bradford from the illuminant's white to the space's white, and expressed in the space's
linear RGB, as `alwan_reflectance_to_rgb` does: a perfect mirror comes out `(1, 1, 1)`.
The table covers optical path differences from 0 to `opd_max_nm` (40 um: the first term of
a 15 um film of index 1.33 at normal incidence) and is 0 past it, where the colour weights'
transform has decayed: on oil films from 2 to 8 um a 20 um table leaves 1.1e-4, a 40 um
one 1.7e-6, and 80 um gains nothing more. `info` gives the entry count, the step, each band's
demodulation wavenumber and wavelength (where its indices are sampled), and the white.

## The colour

```c
alwan_status alwan_iridescence_fresnel_rgb_{T}(alwan_rgb_{T} *rgb_out, alwan_xyz_{T} *xyz_out,
                                               {T} cos_theta, alwan_iridescent_film const *film,
                                               alwan_iridescence const *s);
```

The unpolarised reflected colour at the incidence cosine: in a microfacet BRDF,
`cos_theta` is `h . v`. `rgb_out` is in the table's space, `xyz_out` is XYZ before the
adaptation; either may be NULL. `film` holds one film per band, all of one thickness. A
film of zero thickness is the bare base's Fresnel reflectance.

```c
alwan_status alwan_iridescence_reflectance_{T}({T} *R_out, {T} const *wavelengths_nm, size_t count,
                                               {T} cos_theta, alwan_iridescent_film const *film,
                                               alwan_polarization polarization);
```

The exact Airy reflectance spectrum of one film (s, p or their mean), evanescent films
included: the spectrum the colour integrates.

```c
alwan_status alwan_iridescence_ggx_{T}(alwan_rgb_{T} *out, {T} nov, {T} nol, {T} noh, {T} voh,
                                       {T} alpha, alwan_iridescent_film const *film,
                                       alwan_iridescence const *s);
```

A GGX microfacet BRDF with the iridescent Fresnel term,
`F(h . v) D(n . h) G2(n . v, n . l) / (4 (n . v)(n . l))`, with the GGX distribution and
the height-correlated Smith term of [image-based lighting](ibl.md) (`alpha` the GGX width,
the square of perceptual roughness). 0 when `n . v`, `n . l` or `h . v` is not positive.

## Dispersive films: bands

The paper's model holds the indices constant over the spectrum. Titanium's absorption and
TiO2's index both change a lot across the visible range, and a constant-index film of
TiO2 on titanium comes out the wrong colour. With `bands > 1` the spectrum is split into
bands of equal colour weight (x + y + z under the illuminant). Each band gets its own
transform table and takes its own film, with the indices at the band's wavelength;
`alwan_iridescent_films_from_materials` fills them from the
[refractive database](refractive-index.md):

```c
alwan_status alwan_iridescent_film_from_materials(alwan_iridescent_film *out, size_t ambient,
    alwan_refractive_table const *ambient_table, size_t film_material,
    alwan_refractive_table const *film_table, double thickness_nm, size_t base,
    alwan_refractive_table const *base_table, double wavelength_nm, alwan_interp_method interpolation);
alwan_status alwan_iridescent_films_from_materials(alwan_iridescent_film *out, size_t capacity,
    alwan_iridescence const *s, size_t ambient, alwan_refractive_table const *ambient_table,
    size_t film_material, alwan_refractive_table const *film_table, double thickness_nm,
    size_t base, alwan_refractive_table const *base_table, alwan_interp_method interpolation);
```

The first samples one wavelength (0 for 550 nm), the second the table's band wavelengths.
An ambient or film that absorbs (k above 0.01) is `ALWAN_E_RANGE`; a smaller k is dropped.

120 nm of TiO2 (`main/TiO2`) on titanium (`main/Ti`) at normal incidence in sRGB under D65,
against the exact dispersive colour (`alwan_refractive_stack_rgb`, 0.7797 0.2429 0.3120):

| Bands | Colour | Worst error |
|---|---|---|
| constant indices at 550 nm | 0.6805 0.2865 0.0920 | 0.22 |
| 1 (indices at the band's 515 nm) | 0.6558 0.2087 0.1516 | 0.16 |
| 4 | 0.7869 0.2333 0.3031 | 0.0095 |
| 8 | 0.7836 0.2370 0.3143 | 0.0059 |
| 16 | 0.7800 0.2424 0.3115 | 0.0005 |

Each band costs one more table (about 1 MB in double at the defaults) and one more
series evaluation per call.

## Real time

```c
alwan_status alwan_iridescence_table_{T}({T} *out, size_t row_stride, size_t cos_count,
                                         size_t thickness_count, {T} thickness_min_nm,
                                         {T} thickness_max_nm, alwan_iridescent_film const *film,
                                         alwan_iridescence const *s);
```

A 2D table of the Fresnel term, the incidence cosine by the film thickness, three values a
texel, for a renderer that varies the thickness across a surface (a soap bubble). A shader
reads it with `alwan_iridescence_table_lookup` (bilinear, clamp to edge, `t` the position
in the thickness range). A 32 x 64 table over 0 to 1000 nm of oil matches the direct
evaluation exactly at its texel centres and to 5.6e-3 between them.

A shader can also evaluate the series itself from the transform table,
`alwan_iridescence_sensitivity_{T}` (`bands * count * 6` values) with
`alwan_iridescence_fresnel` (one band) or `alwan_iridescence_fresnel_band` (summed over
the bands) in `core/alwan_iridescence_reader.inc`, behind the `ALWAN_IRIDESCENCE_READ`
accessor:

```hlsl
StructuredBuffer<float> Sens : register(t7);
#define ALWAN_IRIDESCENCE_NAME     film
#define ALWAN_IRIDESCENCE_READ(i)  Sens[i]
#include "core/alwan_iridescence_reader.inc"
/* alwan_iridescence_fresnel_film(count, opd_step, nu0, terms, tol, n1, n2, d, n3, k3, cos) */
```

## Accuracy

Suite 288, all against ground truth rather than another implementation:

| Check | Result |
|---|---|
| Airy against `alwan_multilayer_tmm`, 5 films (one evanescent at grazing), 5 thicknesses, 5 angles, 31 wavelengths | 3.7e-14 |
| Zero thickness against `alwan_fresnel` | spectrum 8.5e-15, colour 6.5e-8 |
| Colour against the spectral integral at 0.1 nm, films 0 to 800 nm, 4 bases, 3 angles | 1.3e-5 in XYZ |
| Evanescent film, 1 / 8 / 16 bands, against the integral at 0.5 nm | 7.3e-2 / 1.5e-3 / 2.8e-4 |
| Oil films 2 to 8 um, normal incidence, against the integral at 0.02 nm | 1.5e-6 |
| f32 against f64 | 6.9e-7 |

On a 6 um oil film, the integral of the spectrum sampled every 0.02 nm is the truth. The
series lands within 8.4e-7 of it, and within 1.5e-6 on every oil film from 2 to 8 um. A 1 nm sampling is off by 1.1e-6, 5 nm by 3.3e-4, and
10 nm by 1.2e-2: the spectral sampling aliases where the series does not.

## See Also

- [Thin films and multilayers](thin-films.md): the transfer-matrix method
- [Refractive index](refractive-index.md): the database the bands sample
- [Image-based lighting](ibl.md): GGX and the Smith term
