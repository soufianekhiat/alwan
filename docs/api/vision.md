# Vision Science API

Functions for modelling human visual perception, including contrast sensitivity, pupil response, and atmospheric optical effects.

---

## Overview

Vision science functions model how the human visual system perceives contrast, brightness, and color under different conditions. These are essential for:

- **Display optimization** — Matching content to human perception limits
- **Image quality metrics** — Predicting visible differences
- **HDR tone mapping** — Perceptually-aware luminance compression
- **Rendering optimization** — Adaptive detail based on visibility

**Implemented models:**
- **Barten 1999 CSF** — Contrast Sensitivity Function with full parameterization
- **Rayleigh scattering** — Atmospheric optical effects (Bodhaine 1999)

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
    alwan_scalar L,      // Luminance (cd/m²)
    alwan_scalar X_0,    // Angular size X (degrees)
    alwan_scalar Y_0     // Angular size Y (degrees), -1 to use X_0
);
```

Calculate pupil diameter based on luminance and stimulus angular size.

**Formula:** `d = 5 - 3 * tanh(0.4 * log10(L * X_0 * Y_0 / 1600))`

**Parameters:**
- `L` — Adapting luminance in cd/m² (typical: 0.01 to 10000)
- `X_0` — Horizontal angular size in degrees (default: 60)
- `Y_0` — Vertical angular size in degrees (-1 means use X_0)

**Returns:** Pupil diameter in mm (typically 2-8 mm)

**Example:**
```c
// Pupil size for 100 cd/m² display, 60° field of view
alwan_scalar d = alwan_pupil_diameter_barten1999(100.0, 60.0, 60.0);
// d ≈ 3.2 mm
```

---

### Retinal Illuminance

```c
alwan_scalar alwan_retinal_illuminance_barten1999(
    alwan_scalar L,                    // Luminance (cd/m²)
    alwan_scalar d,                    // Pupil diameter (mm)
    int apply_stiles_crawford          // 1 = apply correction, 0 = skip
);
```

Calculate retinal illuminance in Trolands, optionally with Stiles-Crawford effect correction.

**Formula (without Stiles-Crawford):** `E = (π * d² / 4) * L`

**Formula (with Stiles-Crawford):** `E = (π * d² / 4) * L * [1 - (d/9.7)² + (d/12.4)⁴]`

**Parameters:**
- `L` — Luminance in cd/m²
- `d` — Pupil diameter in mm
- `apply_stiles_crawford` — Apply directional sensitivity correction (1 = yes)

**Returns:** Retinal illuminance in Trolands (Td)

**Example:**
```c
// Retinal illuminance with Stiles-Crawford correction
alwan_scalar E = alwan_retinal_illuminance_barten1999(100.0, 3.2, 1);
// E ≈ 750 Td
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

**Formula:** `M_opt = exp(-2 * π² * σ² * u²)`

**Parameters:**
- `u` — Spatial frequency in cycles per degree
- `sigma` — Standard deviation of line-spread function in degrees

**Returns:** Optical MTF value (0 to 1)

**Example:**
```c
// MTF at 10 cycles/degree with typical optical blur
alwan_scalar mtf = alwan_optical_mtf_barten1999(10.0, 0.0133);
// mtf ≈ 0.65
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

**Formula:** `σ = sqrt(σ₀² + (C_ab * d)²)`

**Parameters:**
- `sigma_0` — Base optical sigma in degrees (default: 0.5/60 = 0.00833)
- `C_ab` — Spherical aberration coefficient (default: 0.08/60 = 0.00133)
- `d` — Pupil diameter in mm

**Returns:** Combined sigma in degrees

**Example:**
```c
alwan_scalar sigma_0 = 0.5 / 60.0;   // 0.5 arcminutes
alwan_scalar C_ab = 0.08 / 60.0;     // 0.08 arcminutes/mm
alwan_scalar sigma = alwan_sigma_barten1999(sigma_0, C_ab, 4.0);
// sigma ≈ 0.0099 degrees
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

**Formula:** `X = (1/X₀² + 1/X_max² + u²/N_max²)^(-0.5)`

**Parameters:**
- `u` — Spatial frequency in cycles per degree
- `X_0` — Object angular size in degrees (default: 60)
- `X_max` — Maximum integration area in degrees (default: 12)
- `N_max` — Maximum integration cycles (default: 15)

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
- `u` — Spatial frequency in cycles per degree (typical: 0.1 to 100)
- `params` — Model parameters (use `alwan_csf_barten1999_params_default()` for defaults)

**Returns:** Contrast sensitivity (dimensionless, higher = more sensitive)

**Example:**
```c
// Full workflow: luminance → pupil → sigma, E → CSF
alwan_scalar L = 100.0;  // 100 cd/m² display

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

Get luminous efficiency for a wavelength [360, 830] nm. Returns value [0, 1]. Data: CIE photopic V(lambda) 1924/1988, CIE scotopic V'(lambda) 1951.

### alwan_photopic_luminance / alwan_scotopic_luminance

```c
alwan_scalar alwan_photopic_luminance(alwan_ctx *ctx, alwan_spd const *spd);
alwan_scalar alwan_scotopic_luminance(alwan_ctx *ctx, alwan_spd const *spd);
```

Calculate photopic or scotopic luminance from an SPD. Returns luminance in cd/m^2.

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

## Rayleigh Scattering

See [Atmospheric Optics](atmosphere.md) for Rayleigh scattering functions:
- `alwan_rayleigh_cross_section()` — Molecular cross section
- `alwan_rayleigh_optical_depth()` — Atmospheric optical depth
- `alwan_rayleigh_spd()` — Scattered light spectrum

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

- [Atmospheric Optics](atmosphere.md) — Rayleigh scattering
- [Spectral Operations](spectral.md) — SPD operations
- [Color Appearance](color-appearance.md) — Perceptual color models
- [Color Difference](color-difference.md) — Visibility of color differences
