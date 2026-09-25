# Vision Science API

Functions for modelling human visual perception, including contrast sensitivity, pupil response, and atmospheric optical effects.

---

## Overview

Vision science functions model how the human visual system perceives contrast, brightness, and color under different conditions. They are used for:

- **Display optimization**: Matching content to human perception limits
- **Image quality metrics**: Predicting visible differences
- **HDR tone mapping**: Perceptually-aware luminance compression
- **Rendering optimization**: Adaptive detail based on visibility

**Implemented models:**
- **Barten 1999 CSF**: Contrast Sensitivity Function with full parameterization
- **Rayleigh scattering**: Atmospheric optical effects (Bodhaine 1999)

---

## Barten 1999 Contrast Sensitivity Function

The Barten 1999 model predicts human contrast sensitivity as a function of spatial frequency, taking into account:

- Optical blur (pupil size, lens quality)
- Photon noise (retinal illuminance)
- Neural noise and lateral inhibition
- Spatial and temporal integration

### Pupil Diameter

```c
alwan_scalar alwan_pupil_diameter_barten1999(
    alwan_scalar L,      // Luminance (cd/m^2)
    alwan_scalar X_0,    // Angular size X (degrees)
    alwan_scalar Y_0     // Angular size Y (degrees), -1 to use X_0
);
```

Calculate pupil diameter based on luminance and stimulus angular size.

**Formula:** `d = 5 - 3 * tanh(0.4 * log10(L * X_0 * Y_0 / 1600))`

**Parameters:**
- `L`: Adapting luminance in cd/m^2 (typical: 0.01 to 10000)
- `X_0`: Horizontal angular size in degrees (default: 60)
- `Y_0`: Vertical angular size in degrees (-1 means use X_0)

**Returns:** Pupil diameter in mm (typically 2-8 mm)

**Example:**
```c
// Pupil size for 100 cd/m^2 display, 60 deg field of view
alwan_scalar d = alwan_pupil_diameter_barten1999(100.0, 60.0, 60.0);
// d ~= 3.2 mm
```

---

### Retinal Illuminance

```c
alwan_scalar alwan_retinal_illuminance_barten1999(
    alwan_scalar L,                    // Luminance (cd/m^2)
    alwan_scalar d,                    // Pupil diameter (mm)
    int apply_stiles_crawford          // 1 = apply correction, 0 = skip
);
```

Calculate retinal illuminance in Trolands, optionally with Stiles-Crawford effect correction.

**Formula (without Stiles-Crawford):** `E = (pi * d^2 / 4) * L`

**Formula (with Stiles-Crawford):** `E = (pi * d^2 / 4) * L * [1 - (d/9.7)^2 + (d/12.4)^4]`

**Parameters:**
- `L`: Luminance in cd/m^2
- `d`: Pupil diameter in mm
- `apply_stiles_crawford`: Apply directional sensitivity correction (1 = yes)

**Returns:** Retinal illuminance in Trolands (Td)

**Example:**
```c
// Retinal illuminance with Stiles-Crawford correction
alwan_scalar E = alwan_retinal_illuminance_barten1999(100.0, 3.2, 1);
// E ~= 750 Td
```

---

### Optical MTF (Modulation Transfer Function)

```c
alwan_scalar alwan_optical_mtf_barten1999(
    alwan_scalar u,       // Spatial frequency (cycles/degree)
    alwan_scalar sigma    // Line-spread function std dev (degrees)
);
```

Calculate the optical modulation transfer function representing blur from the eye's optics.

**Formula:** `M_opt = exp(-2 * pi^2 * sigma^2 * u^2)`

**Parameters:**
- `u`: Spatial frequency in cycles per degree
- `sigma`: Standard deviation of line-spread function in degrees

**Returns:** Optical MTF value (0 to 1)

**Example:**
```c
// MTF at 10 cycles/degree with typical optical blur
alwan_scalar mtf = alwan_optical_mtf_barten1999(10.0, 0.0133);
// mtf ~= 0.65
```

---

### Sigma (Line-Spread Function Standard Deviation)

```c
alwan_scalar alwan_sigma_barten1999(
    alwan_scalar sigma_0,    // Base optical sigma (degrees)
    alwan_scalar C_ab,       // Spherical aberration coefficient
    alwan_scalar d           // Pupil diameter (mm)
);
```

Calculate the standard deviation of the line-spread function combining base optical blur and pupil-dependent aberrations.

**Formula:** `sigma = sqrt(sigma_0^2 + (C_ab * d)^2)`

**Parameters:**
- `sigma_0`: Base optical sigma in degrees (default: 0.5/60 = 0.00833)
- `C_ab`: Spherical aberration coefficient (default: 0.08/60 = 0.00133)
- `d`: Pupil diameter in mm

**Returns:** Combined sigma in degrees

**Example:**
```c
alwan_scalar sigma_0 = 0.5 / 60.0;   // 0.5 arcminutes
alwan_scalar C_ab = 0.08 / 60.0;     // 0.08 arcminutes/mm
alwan_scalar sigma = alwan_sigma_barten1999(sigma_0, C_ab, 4.0);
// sigma ~= 0.0099 degrees
```

---

### Maximum Angular Size

```c
alwan_scalar alwan_maximum_angular_size_barten1999(
    alwan_scalar u,        // Spatial frequency (cycles/degree)
    alwan_scalar X_0,      // Object angular size (degrees)
    alwan_scalar X_max,    // Maximum integration size (degrees)
    alwan_scalar N_max     // Maximum integration cycles
);
```

Calculate the effective angular size for spatial integration.

**Formula:** `X = (1/X_0^2 + 1/X_max^2 + u^2/N_max^2)^(-0.5)`

**Parameters:**
- `u`: Spatial frequency in cycles per degree
- `X_0`: Object angular size in degrees (default: 60)
- `X_max`: Maximum integration area in degrees (default: 12)
- `N_max`: Maximum integration cycles (default: 15)

**Returns:** Effective angular size in degrees

---

### Full CSF Model

```c
// Parameter structure
typedef struct {
    alwan_scalar sigma;    // Line-spread function std dev (degrees)
    alwan_scalar k;        // Signal-to-noise ratio (default: 3.0)
    alwan_scalar T;        // Integration time in seconds (default: 0.1)
    alwan_scalar X_0;      // Angular size x in degrees (default: 60)
    alwan_scalar Y_0;      // Angular size y in degrees (-1 = use X_0)
    alwan_scalar X_max;    // Max integration area x (default: 12)
    alwan_scalar Y_max;    // Max integration area y (-1 = use X_max)
    alwan_scalar N_max;    // Max integration cycles (default: 15)
    alwan_scalar n;        // Quantum efficiency (default: 0.03)
    alwan_scalar p;        // Photon conversion factor (default: 1.2274e6)
    alwan_scalar E;        // Retinal illuminance in Trolands
    alwan_scalar phi_0;    // Neural noise spectral density (default: 3e-8)
    alwan_scalar u_0;      // Lateral inhibition cutoff (default: 7)
} alwan_csf_barten1999_params;

// Initialize with defaults
void alwan_csf_barten1999_params_default(alwan_csf_barten1999_params *params);

// Compute contrast sensitivity
alwan_scalar alwan_csf_barten1999(
    alwan_scalar u,                              // Spatial frequency (cycles/degree)
    const alwan_csf_barten1999_params *params    // Model parameters
);
```

Compute contrast sensitivity at a given spatial frequency using the full Barten 1999 model.

**Parameters:**
- `u`: Spatial frequency in cycles per degree (typical: 0.1 to 100)
- `params`: Model parameters (use `alwan_csf_barten1999_params_default()` for defaults)

**Returns:** Contrast sensitivity (dimensionless, higher = more sensitive)

**Example:**
```c
// Full workflow: luminance -> pupil -> sigma, E -> CSF
alwan_scalar L = 100.0;  // 100 cd/m^2 display

// Calculate optical parameters
alwan_scalar d = alwan_pupil_diameter_barten1999(L, 60.0, 60.0);
alwan_scalar sigma = alwan_sigma_barten1999(0.5/60.0, 0.08/60.0, d);
alwan_scalar E = alwan_retinal_illuminance_barten1999(L, d, 1);

// Set up CSF parameters
alwan_csf_barten1999_params params;
alwan_csf_barten1999_params_default(&params);
params.sigma = sigma;
params.E = E;

// Compute contrast sensitivity at various frequencies
alwan_scalar csf_1cpd = alwan_csf_barten1999(1.0, &params);
alwan_scalar csf_5cpd = alwan_csf_barten1999(5.0, &params);
alwan_scalar csf_10cpd = alwan_csf_barten1999(10.0, &params);
alwan_scalar csf_30cpd = alwan_csf_barten1999(30.0, &params);

// Peak sensitivity typically around 3-8 cpd
// csf_5cpd > csf_1cpd > csf_30cpd (bandpass shape)
```

---

## Use Cases

### Display Quality Assessment

Determine the minimum contrast needed for visibility at different spatial frequencies:

```c
alwan_csf_barten1999_params params;
alwan_csf_barten1999_params_default(&params);
params.E = 500.0;  // Typical indoor viewing

// Contrast threshold = 1 / sensitivity
for (int freq = 1; freq <= 60; freq++) {
    alwan_scalar S = alwan_csf_barten1999((alwan_scalar)freq, &params);
    alwan_scalar threshold = 1.0 / S;
    printf("%d cpd: threshold = %.4f\n", freq, threshold);
}
```

### Adaptive Image Compression

Skip encoding detail that falls below visibility threshold:

```c
// For each DCT coefficient at frequency 'u'
alwan_scalar S = alwan_csf_barten1999(u, &params);
alwan_scalar threshold = 1.0 / S;

if (fabs(coefficient) < threshold * base_quantization) {
    coefficient = 0;  // Below visibility, can be discarded
}
```

### HDR Tone Mapping

Adjust local contrast based on adaptation luminance:

```c
// Per-region adaptation
for (int region = 0; region < num_regions; region++) {
    alwan_scalar L_adapt = region_luminance[region];
    alwan_scalar d = alwan_pupil_diameter_barten1999(L_adapt, 10.0, 10.0);
    alwan_scalar E = alwan_retinal_illuminance_barten1999(L_adapt, d, 1);

    params.E = E;
    alwan_scalar peak_sensitivity = alwan_csf_barten1999(5.0, &params);

    // Scale local contrast inversely with sensitivity
    region_contrast_scale[region] = base_contrast / peak_sensitivity;
}
```

---

## Luminous Efficiency & Luminance

### Vision Types

```c
typedef enum {
    ALWAN_VISION_PHOTOPIC = 0,  /* Daytime, cone-based - V(lambda) */
    ALWAN_VISION_SCOTOPIC = 1,  /* Nighttime, rod-based - V'(lambda) */
    ALWAN_VISION_MESOPIC = 2    /* Twilight, mixed rod/cone */
} alwan_vision_type;
```

### alwan_luminous_efficiency

```c
alwan_scalar alwan_luminous_efficiency(alwan_scalar wavelength,
                                       alwan_vision_type vision_type);
```

Get luminous efficiency for a wavelength [360, 830] nm. Returns value [0, 1]. Data: CIE photopic V(lambda) 1924 at 1 nm over 360-830 nm, CIE scotopic V'(lambda) 1951 at 1 nm over 380-780 nm, as colour-science ships them, interpolated linearly between samples. Inside [360, 830] but outside the scotopic table's range it returns the table's end value.

### alwan_photopic_luminance / alwan_scotopic_luminance

```c
alwan_scalar alwan_photopic_luminance(alwan_ctx *ctx, alwan_spd const *spd);
alwan_scalar alwan_scotopic_luminance(alwan_ctx *ctx, alwan_spd const *spd);
```

Calculate photopic or scotopic luminance from an SPD. Returns luminance in cd/m^2.

### alwan_spd_luminous_flux / _efficiency / _efficacy

```c
alwan_status alwan_spd_luminous_flux_f64(alwan_f64 *flux_out, alwan_spd_f64 const *spd,
                                         alwan_vision_type vision, alwan_f64 K_m);
alwan_status alwan_spd_luminous_efficiency_f64(alwan_f64 *efficiency_out,
                                               alwan_spd_f64 const *spd, alwan_vision_type vision);
alwan_status alwan_spd_luminous_efficacy_f64(alwan_f64 *efficacy_out, alwan_spd_f64 const *spd,
                                             alwan_vision_type vision, alwan_f64 K_m);
```

colour-science's `luminous_flux`, `luminous_efficiency` and `luminous_efficacy`: the
trapezoid of V(lambda) S(lambda) over the SPD's own samples, V 0 outside its table,
times K_m for the flux; over the plain trapezoid of S for the efficiency; K_m times the
efficiency for the efficacy, in lm/W. `vision` is photopic or scotopic; `K_m` 0 means
683 or 1700, colour-science's constants (`alwan_photopic_luminance` uses 683.002). Mesopic
needs an adaptation level and is `ALWAN_E_INVALID` here, as is a negative `K_m` or an
SPD of fewer than two samples. Matches colour-science to 2e-15 (suite 133).

### alwan_spd_lef / alwan_spd_luminous_flux_lef

```c
typedef enum {
    ALWAN_LEF_CIE_1924_PHOTOPIC, ALWAN_LEF_JUDD_1951_PHOTOPIC, ALWAN_LEF_JUDD_VOS_1978_PHOTOPIC,
    ALWAN_LEF_CIE_1964_PHOTOPIC_10DEG, ALWAN_LEF_CIE_2008_PHOTOPIC_2DEG,
    ALWAN_LEF_CIE_2008_PHOTOPIC_10DEG, ALWAN_LEF_CIE_1951_SCOTOPIC
} alwan_lef;

alwan_status alwan_spd_lef_f64(alwan_spd_f64 *out, alwan_lef lef, alwan_ctx *ctx);
alwan_status alwan_spd_luminous_flux_lef_f64(alwan_f64 *flux_out, alwan_spd_f64 const *spd,
                                             alwan_lef lef, alwan_f64 K_m, alwan_ctx *ctx);
```

The luminous efficiency functions as datasets. `alwan_vision_type` names a regime with
one canonical function behind it; `alwan_lef` names the function. Seven, as
colour-science ships them: CIE 1924 V(lambda); Judd 1951 and Judd-Vos 1978, which correct
it below 460 nm; CIE 1964 for the 10 degree field; the CIE 2008 2 and 10 degree
physiologically relevant functions on the Stockman and Sharpe cone fundamentals; and CIE
1951 V'(lambda).

`alwan_spd_lef` creates the function as an SPD on its own grid: 1 nm for six of them,
10 nm over 370-770 nm for Judd 1951, as published. Peaks are 1, or within 5e-3 of it as
published (Judd 1951 at 0.995). CIE 1924 and CIE 1951 are the same numbers
`alwan_luminous_efficiency` interpolates; the other five are registry tables and answer
`ALWAN_E_NODATA` when compiled out.

`alwan_spd_luminous_flux_lef` is `alwan_spd_luminous_flux` under any of the seven: the
same trapezoid over the SPD's samples, V read from the function's table at each sample,
linear between its nodes, 0 outside its data. `K_m` 0 reads as 683, or 1700 for CIE 1951.
Matches `colour.luminous_flux(sd, lef=...)` to 8e-16 in f64 and 5e-8 in f32 over A, D65
and a sodium lamp (suite 156); for the two canonical functions this and
`alwan_spd_luminous_flux` give one number.

### alwan_photometer_f1_prime_{T} / alwan_photometer_mismatch_correction_{T}

```c
alwan_status alwan_photometer_f1_prime_{T}(alwan_{T} *f1_prime, alwan_spd_{T} const *detector,
                                           alwan_spd_{T} const *calibration);
alwan_status alwan_photometer_mismatch_correction_{T}(alwan_{T} *factor, alwan_spd_{T} const *detector,
                                                      alwan_spd_{T} const *test_source,
                                                      alwan_spd_{T} const *calibration);
```

How well a photometer head's spectral responsivity follows V(lambda), and how to correct its
reading for a given source, per ISO/CIE 19476:2014 (formerly CIE S 023).

- `f1_prime` is the general V(lambda) mismatch index:
  `f1' = sum |s*(l) - V(l)| / sum V(l)`, where `s*(l) = s(l) sum C V / sum C s` is the
  detector's responsivity `s` scaled to agree with V(lambda) under the calibration source `C`.
  It is summed over the detector's own samples. A detector that is V(lambda) gives 0 to
  round-off.
- `factor` is the spectral mismatch correction factor
  `F = (sum T V)(sum C s) / ((sum T s)(sum C V))`, summed over the test source `T`'s samples.
  Multiply the reading of a photometer calibrated under `C` by `F` to get the test source's
  photometric value. It is 1 when `T` is `C`, or when the detector is V(lambda).

V(lambda) is CIE 1924, 0 outside 360-830 nm. Every other curve is read by linear
interpolation and is 0 outside its own range. `calibration` NULL is CIE illuminant A by its
defining formula (CIE 15:2018, c2 = 1.435e7 nm K). That is not `alwan_spd_illuminant`'s A,
which is tabulated to 780 nm and held flat above, where a detector still responds.

Suite 247 holds both to luxpy's `f1prime` and `get_spectral_mismatch_correction_factors`
(within 3.1e-16 and 2.2e-15) and to the definitions written out in numpy, over four detectors
and five sources.

**Returns:** `ALWAN_OK`; `ALWAN_E_INVALID` for a NULL output or an SPD of fewer than two
samples; `ALWAN_E_RANGE` when a normalising sum is 0.

### Luminaire photometric files: alwan_luminaire_load / alwan_luminaire_load_buffer

```c
alwan_status alwan_luminaire_load(alwan_luminaire **out, char const *path, alwan_ctx *ctx);
alwan_status alwan_luminaire_load_buffer(alwan_luminaire **out, char const *buf, size_t len, alwan_ctx *ctx);
void alwan_luminaire_destroy(alwan_luminaire *lum, alwan_ctx *ctx);
```

Reads a luminaire's intensity distribution from an IES LM-63 file (the 1986 form without a
version line, 1991, 1995, 2002, 2019) or a EULUMDAT file (`.ldt`). The format is found by
the IES `TILT=` line, or an `IESNA` / `IES:` first line; anything else is read as
EULUMDAT. `TILT=INCLUDE` tables are read and skipped; `TILT=<file>` is recorded (keyword
`TILT`) and not followed. The object is allocated; free it with `alwan_luminaire_destroy`.
The buffer form does no file I/O and takes no ownership; the path form reads the file once
into memory and parses that.

Both formats store intensities over C-planes (the horizontal angle round the luminaire's
axis) and gamma angles (from the nadir), and store only what the luminaire's symmetry
needs. For photometric type C, alwan completes the map from C0 to C360: each plane is the
stored plane its symmetry maps it onto.

| Symmetry (`alwan_luminaire_symmetry`, EULUMDAT Isym) | IES horizontal angles | Map |
|---|---|---|
| `NONE` (0) | 0 to 360, or 0 to past 180 | as stored; C360 is C0 |
| `ROTATIONAL` (1) | one angle | every plane the stored one |
| `C0_C180` (2) | 0 to 180 | C -> -C |
| `C90_C270` (3) | 90 to 270 | C -> 180 - C (EULUMDAT stores C270 through C0 to C90) |
| `QUADRANT` (4) | 0 to 90 | C -> -C, 180 - C, C + 180 |

The map is in the file's units, scaled by its multiplier: an IES file's candela multiplier,
so candela (the ballast factor is reported in the info, not applied), or a EULUMDAT file's
conversion factor, so candela per 1000 lumens of lamp flux.

These read files the program did not write, and are built on the guards of the chart and
`.cube` readers (see the untrusted-input section of `docs/alwan_future.md`):

- Numbers are parsed without the locale or `sscanf`, the whole token must be a number, and
  a magnitude past 1e308 (a long digit string, an exponent, `inf`, `nan`) is refused.
- Counts are bounded before anything is allocated: 100,000 angles, 1,000,000 stored values,
  20 EULUMDAT lamp sets, 4096 IES keywords, a 64 MB file. Products go through
  `alwan_safe_array_size`.
- The data a count promises must be there, angle lists must rise strictly, the stored
  planes must match the symmetry, intensities must be finite and not negative, and nothing
  but blanks may follow the last value.
- LF, CR and CRLF line ends and a UTF-8 byte order mark all read, and give the same object.

Suite 253 reads 22 files written from six analytic luminaires, covering every symmetry.
The stored grids equal what was written. The intensity equals luxpy's `read_lamp_data` at
8000 of its grid points (a fixed sample of up to 400 a file), and every completed plane
matches the analytic luminaire. It
then damages every file: every truncation, oversized and overflowing counts and numbers,
NaN and infinity, negative intensities, angles out of order, a missing TILT, trailing
garbage and seeded random bytes. Each comes back as an error and no object. The same
battery also runs clean under an AddressSanitizer build.

**Returns:** `ALWAN_OK`; `ALWAN_E_INVALID` for a NULL argument or a malformed, truncated or
unreadable file; `ALWAN_E_RANGE` for a value outside what the format allows (a count past
its bound, a zero multiplier, angles out of order, a negative intensity, an IES horizontal
range that is none of the five, EULUMDAT Mc that its symmetry cannot divide);
`ALWAN_E_NOMEM`.

### alwan_luminaire_get_info / alwan_luminaire_angles / alwan_luminaire_values / alwan_luminaire_map / alwan_luminaire_keyword

```c
alwan_status alwan_luminaire_get_info(alwan_luminaire_info *info, alwan_luminaire const *lum);
alwan_status alwan_luminaire_angles(double const **vertical, size_t *nv,
                                    double const **horizontal, size_t *nh, alwan_luminaire const *lum);
alwan_status alwan_luminaire_values(double const **values, size_t *count, alwan_luminaire const *lum);
alwan_status alwan_luminaire_map(double const **c_angles, size_t *nc, double const **gamma,
                                 size_t *ngamma, double const **values, alwan_luminaire const *lum);
char const *alwan_luminaire_keyword(alwan_luminaire const *lum, char const *key);
```

`alwan_luminaire_info` carries the header: format, IES version (1986 when there is no
version line, 0 for a version line alwan does not know), photometric type (1 C, 2 B, 3 A),
EULUMDAT luminaire type, symmetry, tilt, lamps and lamp sets, lumens per lamp (IES, -1 for
absolute photometry), the lamp flux, multiplier, ballast factor, input watts, units (IES 1
feet or 2 metres, EULUMDAT millimetres), dimensions, and EULUMDAT's light output ratio,
downward flux fraction and tilt angle.

`alwan_luminaire_angles` and `alwan_luminaire_values` return the stored grid exactly as
written: `nv` gamma angles, `nh` C-planes, and `nh x nv` intensities before the multiplier,
plane by plane. EULUMDAT Isym 3's planes come in the file's order, C270 through C0 to C90.
`alwan_luminaire_map` returns the completed map: `nc` planes from 0 to 360 and `nc x ngamma`
values scaled by the multiplier; `ALWAN_E_NODATA` for types A and B, which are read but not
completed. `alwan_luminaire_keyword` returns an IES keyword's value (brackets removed,
`[MORE]` lines joined to the one before by a space) or, for EULUMDAT, one of `COMPANY`,
`REPORT`, `LUMINAIRE`, `NUMBER`, `FILENAME`, `DATE`, `LAMP_TYPE`, `CCT`, `CRI` (the first
lamp set's). Keys are case-insensitive; NULL when absent. Every pointer is owned by the
object.

### alwan_luminaire_intensity / alwan_luminaire_flux

```c
alwan_status alwan_luminaire_intensity(double *out, alwan_luminaire const *lum,
                                       double c_deg, double gamma_deg);
alwan_status alwan_luminaire_flux(double *flux, alwan_luminaire const *lum,
                                  alwan_luminaire_flux_method method);
```

`alwan_luminaire_intensity` reads the completed map bilinearly at any C angle (wrapped
into [0, 360)) and gamma angle, in the map's units. Outside the gamma range the file
measured it is 0. Off the grid it is within 1.9e-15 of scipy's `RegularGridInterpolator`
on the same map.

`alwan_luminaire_flux` integrates the map over the gamma range it covers, in the map's
units times steradians: lumens for an IES file, lumens per 1000 lamp lumens for EULUMDAT.

- `ALWAN_LUMINAIRE_FLUX_LINEAR` integrates the bilinear map `alwan_luminaire_intensity` reads
  exactly: each plane's straight lines times sin(gamma) in closed form, then the trapezoid
  rule over C, which is exact for a map linear in C. Within 2.9e-16 of `scipy.integrate.quad`
  on the same map. A uniform sphere of 500 cd gives 4 pi 500 to the last digit; a Lambertian
  downlight sampled every 5 degrees is 6.3e-4 under pi I0.
- `ALWAN_LUMINAIRE_FLUX_TRAPEZOID` applies the trapezoid rule to I sin(gamma) at the samples
  and then over C, as luxpy's `luminous_intensity_to_luminous_flux` does: within 1.8e-16 of
  it on the 20 files luxpy reads.

luxpy's completion for the C90-C270 symmetry labels its last plane 0 where it means 360, so
its own flux for those files runs backwards over the last interval (194.6 lm for a luminaire
of 2507). Suite 253 relabels that plane before comparing; luxpy's intensities there equal
alwan's.

**Returns:** `ALWAN_OK`; `ALWAN_E_INVALID` for a NULL, a NaN angle or an unknown method;
`ALWAN_E_NODATA` for types A and B.

### alwan_mesopic_luminance

```c
alwan_scalar alwan_mesopic_luminance(alwan_ctx *ctx, alwan_spd const *spd,
                                     alwan_scalar adaptation_level);
```

Calculate mesopic luminance using CIE 191:2010 model. `adaptation_level` is the adaptation luminance in cd/m^2 [0.001, 10].

### alwan_csf (simplified)

```c
alwan_scalar alwan_csf(alwan_scalar spatial_frequency, alwan_scalar luminance);
```

Simplified contrast sensitivity function. Quick estimate without full Barten parameterization.

---

## Helmholtz-Kohlrausch Effect (Nayatani 1997)

### alwan_hke_object_nayatani1997_{T} / alwan_hke_luminous_nayatani1997_{T}

```c
alwan_{T} alwan_hke_object_nayatani1997_{T}(alwan_vec2_{T} const *uv,
                                            alwan_vec2_{T} const *uv_c,
                                            alwan_{T} L_a,
                                            alwan_hke_nayatani1997_method method);

alwan_{T} alwan_hke_luminous_nayatani1997_{T}(alwan_vec2_{T} const *uv,
                                              alwan_vec2_{T} const *uv_c,
                                              alwan_{T} L_a,
                                              alwan_hke_nayatani1997_method method);
```

A saturated colour looks brighter than a grey of the same luminance. These predict by how
much.

- `uv` — the stimulus as a **CIE 1960 UCS chromaticity pair**
- `uv_c` — the adapting field, same coordinates
- `L_a` — adapting luminance in cd/m²
- `method` — `ALWAN_HKE_NAYATANI1997_VCC` or `ALWAN_HKE_NAYATANI1997_VAC`

The two methods differ in exactly one coefficient, the weight on the hue term: `-0.866` for
VCC, `-0.134` for VAC. VCC corresponds to a colour matched for equal **brightness**, VAC to
one matched for equal **lightness**.

The object variant returns a multiplier on luminance and is **exactly 1** when the stimulus
sits on the adapting field, which is the reading the number exists to give: no chromatic
content, no effect. The luminous variant is `0.4462 (object + 0.3086)³`.

> **These take `u, v` chromaticities, not `U*V*W*`.** `alwan_xyz_to_ucs_{T}` returns the
> CIE 1964 `U*V*W*` triple, which is a different quantity. The `u, v` pair here is
> `u = 4X / (X + 15Y + 3Z)`, `v = 6Y / (X + 15Y + 3Z)`.

**Ranges:** both are native and have no normalization macro, per rule 2 of
[ranges.md](../ranges.md): they are viewing-condition-dependent multipliers with no fixed
bound. Across a sweep of the full hue period at four saturations and five adapting
luminances, the object variant spans about `[0.77, 1.40]` and the luminous one about
`[0.56, 2.24]` — the extents of that sweep, not limits of the model.

**Returns:** the effect, or a negative value on a NULL argument. The effect itself is a
positive multiplier, so a negative return is unambiguous.

---

## Rayleigh Scattering

See [Atmospheric Optics](atmosphere.md) for Rayleigh scattering functions:
- `alwan_rayleigh_cross_section()`: Molecular cross section
- `alwan_rayleigh_optical_depth()`: Atmospheric optical depth
- `alwan_rayleigh_spd()`: Scattered light spectrum

---

## Implementation Notes

### Numerical Stability

- All functions handle edge cases (zero luminance, zero frequency)
- Exponential terms are clamped to avoid underflow
- Division by zero is protected in ratio calculations

### Default Parameter Values

The default parameters match colour-science's implementation:
- `sigma_0 = 0.5/60` degrees (0.5 arcminutes base blur)
- `C_ab = 0.08/60` degrees/mm (spherical aberration)
- `k = 3.0` (signal-to-noise ratio)
- `T = 0.1` seconds (integration time)
- `n = 0.03` (quantum efficiency)
- `phi_0 = 3e-8` (neural noise)
- `u_0 = 7` cycles/degree (lateral inhibition cutoff)

### References

- Barten, P. G. J. (1999). *Contrast sensitivity of the human eye and its effects on image quality*. SPIE Press.
- colour-science implementation: `colour.contrast_sensitivity_function_Barten1999()`

---

## See Also

- [Atmospheric Optics](atmosphere.md): Rayleigh scattering
- [Spectral Operations](spectral.md): SPD operations
- [Color Appearance](color-appearance.md): Perceptual color models
- [Color Difference](color-difference.md): Visibility of color differences
