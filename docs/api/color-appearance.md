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
    alwan_scalar la;              // Adapting luminance (cd/m²)
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
    .la = 64.0,   // cd/m²
    .yb = 0.2,    // 20% gray background
    .surround = 0.9,
    .f = 0.9,
    .c = 0.59,
    .nc = 0.95
};

// Bright print viewed in bright gallery
alwan_cam_viewing_conditions bright_print = {
    .white_point = alwan_d50_xyz,
    .la = 318.0,  // cd/m²
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

---

## See Also

- [Color Difference](color-difference.md) — ΔE metrics
- [Chromatic Adaptation](chromatic-adaptation.md) — White point adaptation
- [Color Spaces](color-spaces.md) — XYZ conversions
