# Refractive Index Database

The refractiveindex.info database (M. N. Polyanskiy, CC0 1.0) built into alwan: n and k of
every page of its n,k catalogue, 3,576 measurements and fits of metals, semiconductors,
dielectrics, glasses with the makers' catalogues, liquids, gases and organic materials. On
top of it: the reflectance and transmittance of a bare surface, of a stack of thin layers and
of a thick slab, and their colour in any RGB space, under any illuminant, for any observer.

The data is about 35 MB of CSV source and about 14 MB in the binary. It is built in by
default; define `ALWAN_WITH_REFRACTIVE_DATA` to 0 (CMake: `-DALWAN_WITH_REFRACTIVE_DATA=OFF`)
to leave it out. Without it `alwan_refractive_index_count` is 0 and every function that names
a database entry returns `ALWAN_E_NODATA`; `alwan_reflectance_to_rgb` and
`alwan_multilayer_tmm` (thin-films.md) take the caller's own spectra and indices and work the
same.

## Pages

```c
size_t alwan_refractive_index_count(void);
alwan_status alwan_refractive_index_find(size_t *index_out, char const *id);
alwan_status alwan_refractive_index_get_info(alwan_refractive_index_info *out, size_t index);
```

A page is one source's data for one material, named `"shelf/book/page"` as the site names
it: `"main/Au/Johnson"` is gold from Johnson and Christy (1972). `"Au"` or `"main/Au"` names
the book's first page, the one the database lists first for the material (Johnson and Christy
for gold, silver and copper, Rakic for aluminium); a bare book is looked for on the main
shelf first. Indices run in the database's catalogue order.

`alwan_refractive_index_info` gives the id, shelf, book and page, the display names, the
reference as plain text, whether the page has n and k, whether n is tabulated or which of the
database's nine dispersion formulas gives it, and the wavelength range of each, in nm. A page
with no k has k = 0 at every wavelength, the database's convention for a transparent
material; a page with no n (a few absorption-only pages) samples n as NaN and cannot be a
medium.

## Sampling n and k

```c
alwan_status alwan_refractive_index_sample_{T}(T *n_out, T *k_out, size_t index, T wavelength_nm,
                                               alwan_interp_method interpolation,
                                               alwan_extrapolate_mode extrapolate);
alwan_status alwan_refractive_index_spectrum_{T}(T *n_out, T *k_out, T const *wavelengths_nm, size_t count,
                                                 size_t index, alwan_interp_method interpolation,
                                                 alwan_extrapolate_mode extrapolate);
```

A tabulated page keeps its samples as the database spells them, read as float32, and is read
between them with any `alwan_interp_method`, the enum alwan's spectral functions already use:

| Method | On a tabulated page |
|---|---|
| `ALWAN_INTERP_LINEAR` (0, the default) | the chord; never overshoots, any grid |
| `ALWAN_INTERP_PCHIP` | smooth, monotone between samples: never overshoots, any grid |
| `ALWAN_INTERP_AKIMA` | smooth, any grid; Akima's vanishing-weight test reads the 16 samples around |
| `ALWAN_INTERP_CUBIC` | Catmull-Rom on the sample index, any grid; can overshoot |
| `ALWAN_INTERP_LAGRANGE` | the cubic through four samples, any grid; can overshoot |
| `ALWAN_INTERP_SPRAGUE`, `ALWAN_INTERP_LANCZOS` | uniform grids only: `ALWAN_E_INVALID` where the samples around the wavelength are not evenly spaced (to 1e-3) |

LINEAR is the default because it needs nothing of the grid and cannot overshoot: most of the
database's tables are not uniform in wavelength (Johnson and Christy is uniform in photon
energy), and a cubic can put k above or below its neighbours at an absorption edge. Every
method reads the 8 samples on either side of the wavelength, which is all any of them uses, so
a 100,000-sample table costs no more to read than a short one. At a sample, every method
returns the sample.

A formula page is evaluated exactly, whatever the interpolation, as the database defines it,
with the wavelength in micrometres: Sellmeier (1), Sellmeier-2 (2), polynomial (3), the
RefractiveIndex.INFO form (4), Cauchy (5), gases (6), Herzberger (7), retro (8) and exotic
(9). A formula whose n^2 comes out negative is `ALWAN_E_RANGE`.

Outside a quantity's range the extrapolation mode applies to n and to k separately:
`ALWAN_EXTRAPOLATE_CONSTANT` holds the edge value, `LINEAR` continues the slope of the last
interval (a formula's last 1e-4 of its range), `LINEAR_CLAMP_ZERO` continues it and stops at
0, and `ZERO` gives 0, which for n is not an index but a "no data" mark.

## Your own optical constants

```c
typedef struct {
    double const *wavelengths_nm;   /* strictly ascending */
    double const *n;
    double const *k;                /* NULL: k = 0 */
    size_t count;
} alwan_refractive_table;
```

A table stands in for a database page in a stack or a slab, read with the same
interpolation and its edge values held outside. It needs no refractive data, so a build with
`ALWAN_WITH_REFRACTIVE_DATA` 0 still computes stacks and colours from measured or modelled
constants.

## Stacks

```c
#define ALWAN_REFRACTIVE_VACUUM ((size_t)-1)

typedef struct {
    size_t material;
    double thickness_nm;
    alwan_refractive_table const *table;   /* not NULL: used in place of the page */
} alwan_refractive_layer;

typedef struct {
    size_t ambient;                        /* lossless: vacuum, air, water, glass */
    alwan_refractive_layer const *layers;  /* from the ambient side down */
    size_t layer_count;
    size_t substrate;                      /* semi-infinite */
    double theta_degrees;                  /* in the ambient */
    alwan_polarization polarization;       /* UNPOLARIZED (0), S or P */
    alwan_interp_method interpolation;     /* 0: LINEAR */
    alwan_refractive_table const *ambient_table;     /* not NULL: in place of the ambient page */
    alwan_refractive_table const *substrate_table;   /* not NULL: in place of the substrate page */
} alwan_refractive_stack;
```

Zero a stack and its layers (`memset` or `= {0}`) before filling them in, so that the fields
you do not set mean their defaults.

```c
alwan_status alwan_refractive_stack_{T}(T *R_out, T *T_out, T const *wavelengths_nm, size_t count,
                                        alwan_refractive_stack const *stack, alwan_ctx *ctx);
```

The reflectance and transmittance of the stack, one value each per wavelength, by
`alwan_multilayer_tmm` on the pages' indices, held at their edge values outside a page's data.
Every layer is coherent: it interferes with itself, which is what makes a few nanometres of
gold on copper, or a soap film, show colour. `T` is the power that enters the substrate. A
bare surface has no layers; a free-standing film has `ALWAN_REFRACTIVE_VACUUM` as its
substrate.

## Thick slabs

```c
alwan_status alwan_refractive_slab_{T}(T *R_out, T *T_out, T const *wavelengths_nm, size_t count,
                                       size_t material, alwan_refractive_table const *table, T thickness_nm,
                                       size_t ambient, T theta_degrees, alwan_polarization polarization,
                                       alwan_interp_method interpolation);
```

A slab of one material with the ambient on both sides, incoherent: inside a pane of glass or a
bulk crystal the reflections add in power, not in amplitude, because the slab is far thicker
than the light's coherence length. With `R1` the reflectance of a face (the same from inside
as from outside) and `A = exp(-4 pi Im(n cos theta_t) d / lambda)` the single-pass
transmission,

    R = R1 + (1 - R1)^2 R1 A^2 / (1 - R1^2 A^2)
    T = (1 - R1)^2 A / (1 - R1^2 A^2)

per polarisation. A lossless slab reflects `2 R1 / (1 + R1)`, about 6.8 % for fused silica.

## Colour

```c
alwan_status alwan_reflectance_to_rgb_{T}(alwan_rgb_{T} *rgb_out, alwan_xyz_{T} *xyz_out,
                                          alwan_spd_{T} const *spectrum, alwan_interp_method interpolation,
                                          alwan_rgb_space space, alwan_illuminant illuminant,
                                          alwan_observer_type observer, alwan_ctx *ctx);
alwan_status alwan_refractive_stack_rgb_{T}(alwan_rgb_{T} *reflect_rgb, alwan_rgb_{T} *transmit_rgb,
                                            alwan_refractive_stack const *stack, alwan_rgb_space space,
                                            alwan_illuminant illuminant, alwan_observer_type observer,
                                            alwan_ctx *ctx);
alwan_status alwan_refractive_slab_rgb_{T}(alwan_rgb_{T} *reflect_rgb, alwan_rgb_{T} *transmit_rgb,
                                           size_t material, alwan_refractive_table const *table,
                                           T thickness_nm, alwan_interp_method interpolation,
                                           alwan_rgb_space space, alwan_illuminant illuminant,
                                           alwan_observer_type observer, alwan_ctx *ctx);
```

`alwan_reflectance_to_rgb` lights a reflectance or transmittance spectrum with an illuminant,
integrates it with an observer, divides by a perfect diffuser under the same light (so Y = 1
for a spectrum of 1), adapts by Bradford from the illuminant's white to the space's white and
returns linear RGB in the space. A spectrum of 1 is (1, 1, 1) in every space under every
light. Its `interpolation` reads the spectrum between its samples: 0 (LINEAR) integrates it as
given, any other method first resamples it at 1 nm over its range with that method (the
spectrum's grid is uniform, so SPRAGUE and LANCZOS apply). For a renderer that is F0, an albedo or a filter colour in its working space; values
past 1 or below 0 are colours outside the space (gold's F0 is past 1 in red in sRGB).

The stack and slab forms read the media with the stack's (or the given) interpolation at
360 to 830 nm in 1 nm steps and pass R and T through it. Bare
gold is `{ VACUUM, NULL, 0, gold }`; 2 nm of gold on copper is one layer `{ gold, 2 }` on a
copper substrate; seen through 10 nm of gold on glass is the transmitted colour of
`{ VACUUM, { gold, 10 }, 1, glass }`.

```c
size_t au, cu;
alwan_refractive_index_find(&au, "Au");
alwan_refractive_index_find(&cu, "Cu");
alwan_refractive_layer gold = { au, 2.0, NULL };
alwan_refractive_stack st = { ALWAN_REFRACTIVE_VACUUM, &gold, 1, cu, 0.0, ALWAN_POLARIZATION_UNPOLARIZED };
alwan_rgb_f64 f0;
alwan_refractive_stack_rgb_f64(&f0, NULL, &st, ALWAN_RGB_SPACE_SRGB, ALWAN_ILLUMINANT_D65,
                               ALWAN_OBSERVER_CIE_1931_2DEG, NULL);
```

## Validation (suite 283)

No reference implementation: the data's own values, closed forms and conservation laws.

- Tabulated pages return the database's samples at their wavelengths and their mean half way
  between two; formula pages give Malitson's fused silica 1.458464 and SCHOTT N-BK7 1.516800 at
  587.56 nm, as both publish them (1.4585, 1.51680).
- A bare surface reflects `((n-1)^2 + k^2) / ((n+1)^2 + k^2)` to 7.8e-16; a lossless TiO2 /
  SiO2 stack at 30 degrees conserves energy to 8.9e-16; a zero-thickness layer changes nothing
  (1.1e-15); 1000 nm of copper on gold reflects as bulk copper (7.8e-16); a lossless slab
  reflects `2 R1 / (1 + R1)` (2.2e-16).
- Every interpolation method on Johnson and Christy's gold (non-uniform) and McPeak's (10 nm
  steps): at a sample each returns it (to the float32 spelling of its wavelength, 1e-5);
  between 495.9 and 520.9 nm, k is 1.953 (LINEAR, the chord), 1.917 (PCHIP, inside the two
  samples) to 1.930 (LAGRANGE); SPRAGUE and LANCZOS refuse Johnson and Christy's grid and read
  McPeak's. A formula page gives the same n under every method. LINEAR against PCHIP moves gold's
  F0 by 2e-3; a caller's table of a page's own samples reproduces the page exactly.
- A spectrum of 1 is (1, 1, 1) to 1e-12 in sRGB under D65 and in ACEScg under A with the
  10 degree observer. Gold, copper, silver and aluminium F0 in linear sRGB fall within 0.08 of
  the values real-time rendering tables publish (gold 1.04, 0.73, 0.37 against 1.00, 0.77,
  0.34: the tables differ by source); 2 nm of gold moves copper toward gold; 10 nm of gold
  transmits more green than red.

The database is pinned in alwan_dev's `gendata/datasets.py` and converted by
`gendata/data/refractive_index.py` (17 tabulated series arrive out of order and are sorted;
57 repeated wavelengths keep their first row).
