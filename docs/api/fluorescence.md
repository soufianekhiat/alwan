# Fluorescence

A fluorescent material absorbs light at one wavelength and sends part of it back at a longer
one. A paper whitener turns ultraviolet into blue, so the paper looks whiter than any
reflectance could make it; a highlighter turns blue into green and glows. Because the light
it emits comes from light it absorbed elsewhere, the material's colour depends on the
illuminant's whole spectrum, including the part outside what the eye sees. alwan describes
such a material by its Donaldson matrix, computes what it sends back and its colour under
any illuminant, and samples the reradiation for a spectral path tracer.

Suite 290. Header: `alwan.h`, section "Fluorescence".

## The matrix

On a grid of `count` wavelengths from `lambda_min` to `lambda_max` (step `h`; 300 to 830 nm
at 1 nm by default), `D[o][i]` is the radiance factor at emitted sample `o` per unit
irradiance in the bin of incident sample `i` (each bin `h` wide, centred on its sample). The
diagonal is the ordinary reflectance. Under an illuminant `E` the material sends back

    L(o) = sum_i D[o][i] E(i)

relative to a perfect white diffuser, and its total radiance factor is
`beta_T(o) = L(o) / E(o)`. Only a diagonal matrix makes that a reflectance independent of
the light.

## Building one

`alwan_fluorescent_create` takes `alwan_fluorescent_params`:

- **`ALWAN_FLUORESCENT_MATRIX`** takes `D` as given (row = emitted, column = incident), for
  measured bispectral data.
- **`ALWAN_FLUORESCENT_PARAMETRIC`** builds it from a separable model, the form diffuse
  fluorescent BRDFs for rendering use (e.g. Jung, Hanika, Marschner and Dachsbacher 2019,
  "A Simple Diffuse Fluorescent BBRRDF Model"):

  | Term | Meaning |
  |---|---|
  | `a(i)` | the fraction of incident photons the fluorophore absorbs, `absorptance` at the excitation peak |
  | `e(o)` | the emission band, normalised over the grid as photons |
  | `Q` | `quantum_yield`, photons emitted per photon absorbed |
  | `R_base` | the substrate's reflectance, a constant or a table read with `interpolation` |

      D[o][i] = Q a(i) e(o) / sum e . lambda_i / lambda_o     (o != i)
      D[i][i] = R_base(i) (1 - a(i)) + Q a(i) e(i) / sum e

  `lambda_i / lambda_o` turns photons into energy. Both bands are Gaussians in wavenumber
  (in energy, the shape dye bands take to first order), centred on a peak and as wide as a
  full width at half maximum given in nm. The band shapes are alwan's choice, not the
  paper's.
- **Stokes.** By default a photon is emitted only at its own wavelength or longer: the
  emission band is cut below the incident sample and renormalised, so each column's photon
  yield stays exactly `Q a(i)`; a band wholly below the incident sample emits nothing.
  `ALWAN_FLUORESCENT_ANTI_STOKES` keeps the uncut band.

`alwan_fluorescent_params_example` fills two example materials. They are parameters, not
measurements:

| Example | Excitation | Emission | Yield | Substrate |
|---|---|---|---|---|
| `WHITENER` (stilbene-like brightener) | 350 nm, FWHM 50, peak 0.9 | 435 nm, FWHM 60 | 0.8 | 0.85 |
| `HIGHLIGHTER` (pyranine-like ink) | 455 nm, FWHM 70, peak 0.95 | 515 nm, FWHM 40 | 0.9 | 0.85 |

No spectra ship with this feature, so there is nothing to license.

## Photons and energy

A column cannot give back more photons than it receives:

    sum_o D[o][i] lambda_o / lambda_i <= 1

The parametric model holds it by construction: each column gives back exactly
`R_base (1 - a) + Q a` (suite 290 measures it to 2e-15). With the Stokes cut, every emitted
photon is at a longer wavelength, so the energy given back is never more than the photons.
`alwan_fluorescent_get_info` reports the worst column of any matrix as `photon_balance_max`
(a measured matrix can come out slightly over 1); it does not refuse one.

## Radiance and colour

`alwan_fluorescent_radiance_{T}` gives `L`, `beta_T` and the fluorescent part of `beta_T`
at each grid sample, under a standard illuminant or a caller's SPD (read linearly, 0
outside its table).

`alwan_fluorescent_rgb_{T}` integrates `L` with the observer, divides by the Y of a
perfect diffuser under the same light, adapts by Bradford from the illuminant's white to
the space's, and returns linear RGB. A fluorescent material can pass Y = 1: the example
highlighter comes out at Y = 1.14 under D65 (linear sRGB 0.51, 1.43, 0.14).

The illuminant's ultraviolet matters, and the tables differ in how much they have:

| Illuminant | Table | The whitener's extra Z over the same paper without it |
|---|---|---|
| D65 | from 300 nm | +0.070 |
| A | from 300 nm, little UV | +0.012 |
| F11 | from 380 nm, no UV | +0.010 (only the excitation band's visible tail) |

The F-series tables stop at 380 nm, so they carry no ultraviolet into a whitener. That
follows the CIE tables, not the lamps: a real fluorescent tube emits some UV.

## Sampling for a path tracer

`alwan_fluorescent_sample_{T}` takes the wavelength a path carries and draws the other one:

- **`ALWAN_FLUORESCENT_OUTGOING`**: a path from the camera carries the emitted wavelength
  and draws the incident one from row `o` of `D`.
- **`ALWAN_FLUORESCENT_INCOMING`**: a path from a light carries the incident wavelength and
  draws the emitted one from column `i`.

It first picks the event: plain reflection (the same wavelength) with probability
`D`'s diagonal over the row's sum `T`, reradiation otherwise. For reradiation it picks a bin
in proportion to its entry and a wavelength uniform inside the bin. `weight_out` is `T` in
every case, so `weight` times the radiance at the sampled wavelength is an unbiased
estimate of `sum_j D L(j)`. `pdf_out` is the event's probability, times the density per nm
for reradiation; `alwan_fluorescent_pdf_{T}` gives the same density for multiple importance
sampling.

The tables are CDFs per row and per column, built once by `create`. A shader reads them
through `core/alwan_fluorescence_reader.inc` (accessor `ALWAN_FLUO_READ`, as the importance
sampling reader does) after `alwan_fluorescent_get_layout_f32` hands them over: row `r` of
the OUTGOING half starts at `r * (count + 3)` and holds `T`, the reradiation probability and
the `count + 1` CDF values; the INCOMING half follows at `incoming_base`. The core compiles
with dxc and fxc, fast and deterministic.

Memory: the matrix and the two tables are `n^2 + 2 n (n + 3)` doubles plus a float copy of the tables,
about 9 MB at the default 531 samples. A coarser grid (`count`) shrinks it quadratically.

## Measured (suite 290)

| Check | Result |
|---|---|
| A matrix with nothing off its diagonal, against `alwan_reflectance_to_rgb` | identical |
| Each column's photons against `R_base (1 - a) + Q a` | 2e-15 |
| The whitener's fluorescent radiance factor at 435 nm, linear in the UV level | 2e-16 |
| Reflection probability plus the integrated reradiation density | 1 to 7e-16 |
| Histogram of 2^18 stratified draws against the matrix column | 3.7e-6 |
| Path estimate of `sum_i D[515 nm][i] E(i)` against the product | 2.4e-6 relative |
| f32 radiance against f64 | 5e-8 |
