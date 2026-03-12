# Color Appearance Models API

Functions for color appearance models that predict color perception under different viewing conditions.

---

## Overview

Color appearance models (CAMs) predict how colors appear under varying viewing conditions:
- Illumination level (dark, dim, average, bright)
- Background luminance
- Surround conditions (dark, dim, average)
- Chromatic adaptation state

**Implemented models:**
- **CIECAM02** — CIE Color Appearance Model 2002
- **CAM16** — Color Appearance Model 2016 (improved CIECAM02)
- **ZCAM** — Latest CIE color appearance model
- **UCS variants** — Uniform Color Space versions (JMh → J'a'b')
- **Additional models** — RLAB, Hunt, Nayatani, LLAB, ATD95, Hellwig2022, Kim2009

---

## Functions

### Viewing Conditions

```c
typedef struct {
    alwan_vec3 white_point;       // Adapting white point (XYZ)
    alwan_scalar la;              // Adapting luminance (cd/m^2)
    alwan_scalar yb;              // Background relative luminance
    alwan_scalar surround;        // Surround: 0.8 (avg), 0.9 (dim), 1.0 (dark)
    alwan_scalar f;               // Luminance level: 0.8 (avg), 0.9 (dim), 1.0 (dark)
    alwan_scalar c;               // Impact of surround: 0.69 (avg), 0.59 (dim), 0.525 (dark)
    alwan_scalar nc;              // Chromatic induction: 1.0 (avg), 0.95 (dim), 0.8 (dark)
} alwan_cam_viewing_conditions;
```

---

### CIECAM02 Forward Transform

```c
alwan_result alwan_ciecam02_forward(
    alwan_scalar *J,   // Lightness (output first)
    alwan_scalar *C,   // Chroma
    alwan_scalar *h,   // Hue angle
    alwan_scalar *s,   // Saturation
    alwan_scalar *Q,   // Brightness
    alwan_scalar *M,   // Colorfulness
    alwan_scalar *H,   // Hue quadrature
    alwan_ctx *ctx,
    const alwan_vec3 *xyz,
    const alwan_cam_viewing_conditions *vc
);
```

Convert XYZ to CIECAM02 perceptual correlates.

---

### CIECAM02 Reverse Transform

```c
alwan_result alwan_ciecam02_reverse(
    alwan_vec3 *xyz,              // Output first
    alwan_ctx *ctx,
    alwan_scalar J, alwan_scalar C, alwan_scalar h,
    const alwan_cam_viewing_conditions *vc
);
```

Convert CIECAM02 JCh back to XYZ.

---

### CAM16 Forward Transform

```c
alwan_result alwan_cam16_forward(
    alwan_scalar *J,   // Lightness (output first)
    alwan_scalar *C,   // Chroma
    alwan_scalar *h,   // Hue angle
    alwan_scalar *s,   // Saturation
    alwan_scalar *Q,   // Brightness
    alwan_scalar *M,   // Colorfulness
    alwan_scalar *H,   // Hue quadrature
    alwan_ctx *ctx,
    const alwan_vec3 *xyz,
    const alwan_cam_viewing_conditions *vc
);
```

Convert XYZ to CAM16 perceptual correlates (improved CIECAM02).

---

### CAM16 UCS (Uniform Color Space)

```c
alwan_result alwan_cam16_to_ucs(
    alwan_scalar *J_prime,        // Output first
    alwan_scalar *a_prime,
    alwan_scalar *b_prime,
    alwan_scalar J, alwan_scalar M, alwan_scalar h
);
```

Convert CAM16 JMh to uniform color space J'a'b' for color difference calculations.

---

## Use Cases

### Perceptual Color Matching

Match colors across different viewing conditions:
```c
// Display viewed in dim office
alwan_cam_viewing_conditions dim_office = {
    .white_point = alwan_d65_xyz,
    .la = 64.0,   // cd/m^2
    .yb = 0.2,    // 20% gray background
    .surround = 0.9,
    .f = 0.9,
    .c = 0.59,
    .nc = 0.95
};

// Bright print viewed in bright gallery
alwan_cam_viewing_conditions bright_print = {
    .white_point = alwan_d50_xyz,
    .la = 318.0,  // cd/m^2
    .yb = 0.2,
    .surround = 0.8,
    .f = 0.8,
    .c = 0.69,
    .nc = 1.0
};

// Convert display XYZ → CAM16 → print XYZ (output first)
alwan_scalar J, C, h;
alwan_cam16_forward(&J, &C, &h, ..., ctx, &display_xyz, &dim_office);
alwan_cam16_reverse(&print_xyz, ctx, J, C, h, &bright_print);
```

---

### Color Difference in Perceptual Space

```c
// ΔE in CAM16-UCS space (output first)
alwan_scalar J1, M1, h1, J2, M2, h2;
alwan_cam16_forward(&J1, ..., &M1, &h1, ctx, &xyz1, &vc);
alwan_cam16_forward(&J2, ..., &M2, &h2, ctx, &xyz2, &vc);

alwan_scalar J1p, a1p, b1p, J2p, a2p, b2p;
alwan_cam16_to_ucs(&J1p, &a1p, &b1p, J1, M1, h1);
alwan_cam16_to_ucs(&J2p, &a2p, &b2p, J2, M2, h2);

alwan_scalar delta_e_cam16 = sqrt(
    pow(J1p - J2p, 2) +
    pow(a1p - a2p, 2) +
    pow(b1p - b2p, 2)
);
```

---

## ZCAM

### alwan_zcam_forward / alwan_zcam_inverse

```c
int alwan_zcam_forward(alwan_zcam_correlates *out,
                       alwan_xyz const *xyz,
                       alwan_zcam_viewing_conditions const *vc);

int alwan_zcam_inverse(alwan_xyz *xyz,
                       alwan_zcam_correlates const *correlates,
                       alwan_zcam_viewing_conditions const *vc);
```

Latest CIE color appearance model with improved HDR support. Built on Jzazbz color space.

### alwan_zcam_to_ucs

```c
int alwan_zcam_to_ucs(alwan_jzazbz *Jab_out,
                      alwan_zcam_correlates const *correlates);
```

Convert ZCAM correlates to uniform color space coordinates for color difference calculations.

---

## CAM18sl (Self-Luminous Colors)

### alwan_cam18sl_forward / alwan_cam18sl_inverse

```c
int alwan_cam18sl_forward(alwan_cam18sl_correlates *out,
                          alwan_xyz const *xyz, ...);

int alwan_cam18sl_inverse(alwan_xyz *xyz_out,
                          alwan_cam18sl_correlates const *correlates, ...);
```

Color appearance model for self-luminous stimuli (displays, LEDs). Unlike CIECAM02/CAM16, this model is designed specifically for emissive sources rather than reflective surfaces.

---

## CAM20u (Updated CAM)

### alwan_cam20u_forward / alwan_cam20u_inverse

```c
int alwan_cam20u_forward(alwan_cam20u_correlates *out,
                         alwan_xyz const *xyz, ...);

int alwan_cam20u_inverse(alwan_xyz *xyz_out,
                         alwan_cam20u_correlates const *correlates, ...);
```

Updated color appearance model with improved chromatic adaptation.

---

## Additional Color Appearance Models

Alwan also implements several historical and specialized CAMs. These are forward-only unless noted:

### RLAB (Revised Lab)

```c
int alwan_rlab_forward(alwan_rlab_correlates *out, alwan_xyz const *xyz, ...);
int alwan_rlab_inverse(alwan_xyz *xyz, alwan_rlab_correlates const *correlates, ...);
```

Fairchild 1996 revised Lab model. Invertible.

### Hunt Model

```c
int alwan_hunt_forward(alwan_hunt_correlates *out, alwan_xyz const *xyz, ...);
```

Hunt color appearance model. Complex viewing condition parameters.

### Hellwig & Fairchild 2022

```c
int alwan_hellwig2022_forward(alwan_hellwig2022_correlates *out,
                              alwan_xyz const *xyz, ...);
int alwan_hellwig2022_inverse(alwan_xyz *xyz_out,
                              alwan_hellwig2022_correlates const *correlates, ...);
```

Modern CAM with improved chromatic adaptation. Invertible.

### Kim 2009

```c
int alwan_kim2009_forward(alwan_kim2009_correlates *out,
                          alwan_xyz const *xyz, ...);
int alwan_kim2009_inverse(alwan_xyz *xyz_out,
                          alwan_kim2009_correlates const *correlates, ...);
```

Kim, Weymouth & Rossi 2009 model. Invertible.

### LLAB

```c
int alwan_llab_forward(alwan_llab_correlates *out, alwan_xyz const *xyz, ...);
```

Luo, Lo, Kuo 1996 color appearance model. Forward only.

### ATD95

```c
int alwan_atd95_forward(alwan_atd95_correlates *out, alwan_xyz const *xyz, ...);
```

Guth 1995 achromatic, tritanopic, and deuteranopic model. Forward only.

### Nayatani 1995

```c
int alwan_nayatani95_forward(alwan_nayatani95_correlates *out,
                             alwan_xyz const *xyz, ...);
```

Nayatani 1995 color appearance model. Forward only.

---

## Batch Processing

CIECAM02 and CAM16 support batch operations:

```c
int alwan_ciecam02_forward_map_interleave(alwan_ciecam02_correlates *correlates_out,
    alwan_scalar const *xyz_in, alwan_ciecam02_viewing_conditions const *vc,
    size_t count, size_t in_stride, size_t out_stride);

int alwan_cam16_forward_map_interleave(alwan_cam16_correlates *correlates_out,
    alwan_scalar const *xyz_in, alwan_cam16_viewing_conditions const *vc,
    size_t count, size_t in_stride, size_t out_stride);
```

Inverse batch and `_ex` (typed) variants also available.

---

## See Also

- [Color Difference](color-difference.md) — ΔE metrics (CAM02 UCS, CAM16 UCS, ZCAM)
- [Chromatic Adaptation](chromatic-adaptation.md) — White point adaptation
- [Color Spaces](color-spaces.md) — XYZ conversions
