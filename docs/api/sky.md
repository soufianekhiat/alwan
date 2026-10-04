# Physical Skies

Spectral sky and sun radiance for a sun position, from three models, ready for spectral
rendering and for suwar's environment samplers (its `importance-sampling.md`): Preetham, Shirley and
Smits 1999; Hosek and Wilkie 2012 with their 2013 solar radiance function; and Bruneton 2017's
precomputed atmospheric scattering. Plus the ASTM G173 reference solar spectra. Suite 284.

> **Precision variants:** every function and type shown as `name_{T}` exists as `name_f32` and
> `name_f64`. The parameter structs (`alwan_sky_params`, `alwan_sky_atmosphere`,
> `alwan_sky_bake_params`) are double in both.

## The models

| Model | What it is | Range | Reference it is held to |
|---|---|---|---|
| `ALWAN_SKY_PREETHAM` | The Perez distribution of luminance Y and chromaticity x, y for a turbidity (Appendix A.2), made spectral through the CIE daylight basis (A.5); the sun is Table 2's extraterrestrial spectrum through the A.1 transmittances (Rayleigh, aerosol, ozone 0.0035 m, mixed gases, water vapour 0.02 m). | sky 360-830 nm, sun 380-750 nm, turbidity 1-10 (fitted on 2-6), sun at or above the horizon | the paper's equations, transcribed a second time in numpy: Y to 2.5e-15, sun to 1e-15 |
| `ALWAN_SKY_HOSEK_WILKIE` | The authors' reference implementation 1.4a, ported: eleven bands from 320 nm every 40 nm, read between bands as the reference does (fading to 0 from 720 to 760 nm); the solar disc with limb darkening. | 320-720 nm, turbidity 1-10, ground albedo 0-1, sun 0-90 degrees | ArHosekSkyModel.c compiled and run, 4050 points: 5.4e-16 (det 4.3e-12) |
| `ALWAN_SKY_BRUNETON` | Transmittance, single and multiple scattering and irradiance precomputed for an atmosphere of Rayleigh, Mie and ozone layers over a spherical ground, at several wavelengths; then read for any view, sun and altitude, below the horizon too. | 360-830 nm, any sun elevation (to the precomputed limit), observer from the ground to the top of the atmosphere | the reference CPU model (atmosphere/reference) compiled and run on the same atmosphere at small table sizes, 47 wavelengths: sky radiance to 4.5e-13 of each spectrum's peak (det 6.8e-10), transmittance 9.7e-15, irradiance 4.3e-7 |

Units: spectral radiance in W m^-2 sr^-1 nm^-1. XYZ is that radiance integrated against the
observer's CMFs with no scale, so 683 Y is the luminance in cd/m^2 (as Hosek-Wilkie's own XYZ
data is defined). Preetham's sky is converted from its luminance in kcd/m^2: the daylight-basis
spectrum is scaled so its luminance against V(lambda) is exactly Preetham's Y, whatever the
observer chosen for XYZ.

Directions are the environment maps': +Y is the zenith; the sun sits at elevation e above the
horizon and map azimuth phi, `(cos e cos phi, sin e, cos e sin phi)`. Preetham and Hosek-Wilkie
have no ground: below the horizon their sky radiance is 0. Bruneton has a ground; a camera on
it looking down sees the ground at distance 0, so its in-scattered radiance there is 0 too; lift
the observer to see the light scattered above the ground.

Preetham's chromaticity goes through the CIE daylight basis, whose M1, M2 are an approximate
inverse: the spectrum's x, y come back within 1.5e-4 of the model's. Preetham noted the model's
limits; it is fitted for turbidity 2-6 and is known to misbehave at a low sun with high
turbidity (Zotti, Wilkie and Purgathofer 2007), where it can return negative values: alwan
returns what the equations give.

### Why the three disagree in level

`turbidity` drives Preetham and Hosek-Wilkie only. Bruneton reads its aerosol from
`alwan_sky_atmosphere` (`mie_angstrom_beta`, `mie_angstrom_alpha`), and the default is his demo's,
an aerosol optical depth of 0.0053 at every wavelength: air almost free of haze. Turbidity 3 is an
optical depth near 0.2 at 550 nm. So with the defaults Bruneton's sky is dark and its sun strong.
Horizontal illuminance at a sun 60 degrees up, ground albedo 0.1, in lux, from `683 Y` of
`alwan_sky_irradiance_xyz` (plate-size Bruneton tables, 16 wavelengths):

| Model and aerosol | Sky | Sun |
|---|---|---|
| Preetham, turbidity 2 / 3 | 24,400 / 31,500 | 77,900 / 69,500 |
| Hosek-Wilkie, turbidity 2 / 3 | 14,000 / 16,300 | 70,500 / 69,200 |
| Bruneton, default (optical depth 0.0053) | 7,800 | 98,000 |
| Bruneton, Angstrom beta 0.046 / 0.092, alpha 1.3 (Preetham's turbidity 2 / 3: beta = 0.04608 T - 0.04586) | 18,600 / 31,200 | 87,900 / 78,400 |

Bruneton's default sun, about 128,000 lx outside the atmosphere times sin 60 degrees times a
transmittance of 0.88, is what near-clean air gives, which also confirms the luminance
conversion. For the IESNA clear-sky fit, 0.8 + 15.5 (sin a)^0.5 klx, a 60-degree sun gives
15,200 lx: Hosek-Wilkie lands on it at turbidity 3; Preetham's sky is brighter than measured,
its known weakness; Bruneton with turbidity-3 aerosol is brighter too, since its Mie layer
(1.2 km scale height, single-scattering albedo 0.9, g 0.8) is the demo's, not fitted to sky
measurements. To compare the three on one haze, give Bruneton the Angstrom pair from the
formula above.

## Bruneton's precomputation

```c
typedef struct {
    alwan_f64 bottom_radius;        /* m [6360e3] */
    alwan_f64 top_radius;           /* m [6420e3] */
    alwan_f64 rayleigh_scattering;  /* per m at 1 um, times lambda_um^-4 [1.24062e-6] */
    alwan_f64 rayleigh_scale_height;/* m [8000] */
    alwan_f64 mie_scale_height;     /* m [1200] */
    alwan_f64 mie_angstrom_alpha;   /* as given (Bruneton 0) */
    alwan_f64 mie_angstrom_beta;    /* [5.328e-3] */
    alwan_f64 mie_single_scattering_albedo; /* [0.9] */
    alwan_f64 mie_phase_g;          /* [0.8] */
    alwan_f64 ozone_dobson;         /* [300]; negative for no ozone */
    alwan_f64 max_sun_zenith_angle; /* rad [120 degrees] */
    alwan_f64 observer_altitude;    /* m, as given */
    int scattering_orders;          /* [4] */
    int transmittance_width, transmittance_height;               /* [256, 64] */
    int scattering_r, scattering_mu, scattering_mu_s, scattering_nu; /* [32, 128, 32, 8] */
    int irradiance_width, irradiance_height;                     /* [64, 16] */
    alwan_sky_tables tables;        /* [ALWAN_SKY_TABLES_XYZ] or ALWAN_SKY_TABLES_SPECTRAL */
    size_t wavelength_count;        /* [15] */
    alwan_f64 const *wavelengths;   /* nm, rising; NULL [spread evenly over 360-830] */
} alwan_sky_atmosphere;
```

Zero picks the bracketed value: Bruneton's demo Earth and his table sizes. The solar spectrum is
ASTM G173's extraterrestrial column averaged over 10 nm bins (his demo's table, reproduced from
the G173 data) and the ozone cross sections are his demo's.

The precomputation runs in `alwan_sky_create`, on the CPU, in double precision, on one core,
a batch of four wavelengths at a time (the geometry shared). At the default sizes with 15
wavelengths it was not timed to the end (eight times the texels of the plate's tables,
which take 91 to 128 s at 16 wavelengths, so expect minutes); the reference oracle's sizes (64 x 16 transmittance,
8 x 32 x 8 x 4 scattering, 16 x 4 irradiance) take about 17 s for 47 wavelengths. Time grows
with the scattering texel count, the wavelength count and the number of orders.

Tables are kept in the sky's precision, one of two ways:

- `ALWAN_SKY_TABLES_XYZ`: three channels, each wavelength weighted by the observer's CMF at it
  times the span it covers. Sky radiance, sun radiance and irradiance are linear in each
  wavelength's tables, so this is exact for them (suite 284 compares against SPECTRAL tables
  projected: 4.7e-16).
- `ALWAN_SKY_TABLES_SPECTRAL`: one channel per wavelength. Needed for spectral output,
  transmittance and aerial perspective. About 8.4 MB a wavelength at the default sizes in
  single precision.

`alwan_sky_get_layout_{T}` hands the tables to a shader, laid out as
`core/alwan_sky_atmosphere_reader.inc` reads them through `ALWAN_ATM_READ(i)`: sky radiance,
the solar disc, sun and sky irradiance, transmittance and the radiance scattered into a segment
(aerial perspective). `hlsl_regression/sky_atmosphere_compile.hlsl` in alwan_dev compiles the
reader under dxc (fast and deterministic) and fxc.

## Functions

```c
alwan_status alwan_sky_create_{T}(alwan_sky_{T} **out, alwan_sky_model model, alwan_sky_params const *params,
                                  alwan_ctx *ctx);
void alwan_sky_destroy_{T}(alwan_sky_{T} *sky, alwan_ctx *ctx);
alwan_status alwan_sky_set_sun_{T}(alwan_sky_{T} *sky, alwan_f64 elevation, alwan_f64 azimuth);
alwan_status alwan_sky_set_altitude_{T}(alwan_sky_{T} *sky, alwan_f64 altitude);
alwan_status alwan_sky_sun_direction_{T}(alwan_vec3_{T} *out, alwan_sky_{T} const *sky);
alwan_status alwan_sky_spectral_radiance_{T}(alwan_{T} *out, alwan_{T} const *wavelength_nm, size_t count,
                                             alwan_vec3_{T} const *dir, int include_sun, alwan_sky_{T} const *sky);
alwan_status alwan_sky_xyz_{T}(alwan_xyz_{T} *out, alwan_vec3_{T} const *dir, int include_sun,
                               alwan_sky_{T} const *sky);
alwan_status alwan_sky_irradiance_xyz_{T}(alwan_xyz_{T} *sun_out, alwan_xyz_{T} *sky_out,
                                          alwan_vec3_{T} const *normal, alwan_sky_{T} const *sky);
alwan_status alwan_sky_bake_equirect_{T}(alwan_{T} *out, size_t row_stride, size_t width, size_t height,
                                         alwan_sky_bake_params const *params, alwan_sky_{T} const *sky,
                                         alwan_ctx *ctx);
alwan_status alwan_sky_get_layout_{T}(alwan_sky_layout_{T} *out, alwan_sky_{T} const *sky);
alwan_status alwan_spd_astm_g173_{T}(alwan_spd_{T} *out, alwan_astm_g173_column column, alwan_{T} wavelength_min,
                                     alwan_{T} wavelength_max, size_t count, alwan_ctx *ctx);
```

- **create / destroy**: `params` NULL takes every default (sun on the horizon at azimuth 0,
  turbidity 3, albedo 0, CIE 1931 2 degree observer). `ALWAN_E_RANGE` for parameters outside a
  model's range; `ALWAN_E_INVALID` for a malformed atmosphere (odd `scattering_mu`, wavelengths
  outside 360-830 or not rising, more than 128 of them).
- **set_sun / set_altitude**: move the sun, and Bruneton's observer, without making the sky
  again. Bruneton's tables depend on neither, so a sunrise is many reads of one precomputation;
  the analytic models re-cook what depends on the sun (microseconds).
- **spectral_radiance**: at any wavelengths, 0 outside a model's range. Bruneton needs SPECTRAL
  tables (else `ALWAN_E_NODATA`) and reads linearly between its precomputed wavelengths,
  holding the first and last out to 360 and 830 nm (the bins the XYZ projection gives them).
- **xyz**: the radiance along a direction against the sky's observer.
- **irradiance_xyz**: on a surface with the given normal, from the sun (direct, through the
  atmosphere) and from the sky. Bruneton reads its irradiance table (the paper's
  approximation for a tilted surface: the horizontal value scaled by (1 + n.up)/2); the
  analytic models integrate their radiance over the sphere numerically (128 x 512 directions)
  and the solar disc (16 rings x 32 spokes). On a Rayleigh-only Bruneton sky the
  cosine-weighted integral of a 128 x 64 baked map meets the table's horizontal sky irradiance
  to 1.6 %.
- **bake_equirect**: an equirectangular map, row 0 the zenith, as XYZ, linear RGB in any
  space (XYZ through the space's matrix, no chromatic adaptation) or spectral channels, each
  pixel the mean of `samples x samples` points. The sun is drawn where it falls with
  `include_sun`; a 1024-wide map's pixel is wider than the disc, so add samples or place the sun
  separately for a faithful sun.
- **get_layout**: Bruneton's tables and wavelengths; `ALWAN_E_INVALID` for the analytic models.
- **spd_astm_g173**: ASTM G173-03 (NREL, SMARTS 2.9.2): extraterrestrial, global tilt (37
  degrees, AM1.5) or direct plus circumsolar, W m^-2 nm^-1, read linearly between the table's
  rows (every 0.5 nm to 400, 1 nm to 1700, 5 nm to 4000). `ALWAN_E_RANGE` outside 280-4000 nm.

## Example

```c
alwan_sky_params p = {0};
p.sun_elevation = 0.35;              /* 20 degrees */
p.turbidity = 2.5;
alwan_sky_f32 *sky = NULL;
alwan_sky_create_f32(&sky, ALWAN_SKY_HOSEK_WILKIE, &p, ctx);

alwan_sky_bake_params bp = {0};
bp.format = ALWAN_SKY_BAKE_RGB;
bp.space = ALWAN_RGB_SPACE_ACESCG;
bp.include_sun = 1;
bp.samples = 4;
float *map = malloc(sizeof(float) * 3 * 1024 * 512);
alwan_sky_bake_equirect_f32(map, sizeof(float) * 3 * 1024, 1024, 512, &bp, sky, ctx);
/* weights for the sampler: suwar_env_weight_equirect_f32 on the map's luminance (suwar) */
alwan_sky_destroy_f32(sky, ctx);
```

## Sources

- A. J. Preetham, P. Shirley, B. Smits, "A Practical Analytic Model for Daylight", SIGGRAPH 1999.
- L. Hosek, A. Wilkie, "An Analytic Model for Full Spectral Sky-Dome Radiance", ACM TOG 31(4),
  2012; "Adding a Solar-Radiance Function to the Hosek-Wilkie Skylight Model", IEEE CG&A 33(3),
  2013. Code: BSD-3-Clause (THIRD_PARTY_NOTICES.md).
- E. Bruneton, "Precomputed Atmospheric Scattering: a New Implementation", 2017,
  https://ebruneton.github.io/precomputed_atmospheric_scattering/; E. Bruneton, F. Neyret,
  "Precomputed Atmospheric Scattering", EGSR 2008. Code: BSD-3-Clause (THIRD_PARTY_NOTICES.md).
- ASTM G173-03, Standard Tables for Reference Solar Spectral Irradiances, data from NREL.
