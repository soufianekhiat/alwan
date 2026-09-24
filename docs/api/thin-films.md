# Thin Films and Multilayers

Interference colour: soap bubbles, oil on water, anti-reflection coatings, oxidised metal,
the scales of some beetles and butterflies. Light reflected from the two faces of a layer
thinner than a few wavelengths interferes with itself, so how much of each wavelength comes
back depends on the layer's thickness and index and on the angle, and a colourless material
shows colour.

## alwan_multilayer_tmm

```c
typedef struct {
    double const *n;         /* the real index of each medium */
    double const *k;         /* the extinction coefficient of each, or NULL for none */
    size_t row_stride;       /* 0: one value a medium; otherwise each medium's values over the wavelengths */
    size_t media_count;      /* 2 or more: the incident medium, the layers, the substrate */
    double const *thickness; /* media_count - 2 layer thicknesses, in the wavelengths' unit */
} alwan_multilayer;

alwan_status alwan_multilayer_tmm_f64(double *R, double *T, double const *wavelengths, size_t count,
                                      alwan_multilayer const *stack, double theta_degrees);
alwan_status alwan_multilayer_tmm_f32(float *R, float *T, float const *wavelengths, size_t count,
                                      alwan_multilayer const *stack, float theta_degrees);
```

The reflectance `R` and transmittance `T` of a stack of plane parallel layers between an
incident medium and a substrate, two values per wavelength, s then p polarised. Each medium's
index is `n + i k`, one value for every wavelength (`row_stride` 0) or tabulated per
wavelength to model dispersion. `theta_degrees` is the angle in the incident medium, which
must be lossless. Unpolarised light reflects the mean of the two.

It is the transfer-matrix method as Byrnes writes it ("Multilayer optical calculations",
arXiv:1603.02720, and his `tmm` package):

- the angle in every medium by Snell's law in complex numbers, per wavelength, with the
  forward-travelling branch chosen in the incident medium and the substrate;
- Fresnel's coefficients at every interface;
- a phase `delta = 2 pi n cos(theta) d / lambda` through each layer, its imaginary part held
  at 35 as Byrnes holds it, so an opaque layer does not overflow;
- the product of the layer matrices `M`, then `R = |M10 / M00|^2` and
  `T = |1 / M00|^2 Re(n_s cos theta_s) / Re(n_0 cos theta_0)`, the cosines conjugated for p.

Suite 240 holds it to Byrnes's `tmm` (MIT) on eight stacks: lossless stacks at an angle, a
soap film, a silver-like film at 45 degrees, glass to air past the critical angle, an opaque
absorber, absorbing layers at normal incidence, a bare interface and a dispersive stack, all
within 1.6e-14; lossless stacks conserve energy to 1.1e-15.

colour's `multilayer_tmm` agrees wherever its model is exact: lossless media, or normal
incidence. It takes every angle from the real part of the index at the first wavelength, so
it returns NaN for an absorbing layer at an angle and past the critical angle, and under
dispersion it is 2.3e-3 off on the suite's stack. alwan's angles are complex and per
wavelength, as Byrnes's are.

## alwan_fresnel

```c
alwan_status alwan_fresnel_f64(double *amplitudes, double n1, double k1, double n2, double k2,
                               double theta_degrees);
```

Fresnel's amplitude coefficients from a lossless medium `n1` into `n2 + i k2`:
`amplitudes` receives `r_s`, `r_p`, `t_s`, `t_p`, each as real then imaginary part. The
angle in the second medium follows Snell's law in complex numbers, on the forward branch. The
sign convention is Byrnes's,
`r_p = (n2 cos i - n1 cos t) / (n2 cos i + n1 cos t)`; colour's is its negative, which no
reflectance or transmittance sees. Suite 240 holds the four amplitudes to `tmm`'s
`interface_r` and `interface_t` within 4.0e-15, a metal at 60 degrees among them.

## alwan_water_refractive_index

```c
alwan_status alwan_water_refractive_index_f64(double *n_out, double wavelength_nm,
                                              double temperature_k, double density_kg_m3);
```

The refractive index of water from the wavelength, the temperature and the density, by the
molar refraction of Schiebener, Straub, Levelt Sengers and Gallagher (J. Phys. Chem. Ref.
Data 19(3), 1990): about 1.333 at 589 nm, 294 K and 1000 kg / m^3. As colour's
`light_water_refractive_index_Schiebener1990`, equal on 18 conditions (suite 240). A soap
film is mostly water, so this is its index.

## Colour from a spectrum

`R` over the visible wavelengths is a reflectance: integrate it with
`alwan_xyz_from_spd` under an illuminant for the colour a film shows.

## See Also

- [Spectral](spectral.md): SPDs and their integration
- [Atmospheric Optics](atmosphere.md): Rayleigh scattering
