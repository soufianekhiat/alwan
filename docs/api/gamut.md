# Gamut Operations API

Functions for gamut mapping, volume calculation, and coverage analysis.

---

## Overview

Gamut operations handle colors that fall outside a color space's representable range:

- **Gamut Mapping:** Convert out-of-gamut colors to in-gamut equivalents
- **Gamut Volume:** Calculate the 3D volume of a color space
- **Gamut Coverage:** Measure how much of one gamut fits in another

---

## Gamut Mapping Methods

```c
typedef enum {
    ALWAN_GAMUT_MAP_CLIP = 0,           // Simple clipping to [0,1]
    ALWAN_GAMUT_MAP_HUE_PRESERVING = 1, // Project to gamut boundary preserving hue
    ALWAN_GAMUT_MAP_ADAPTIVE_L0,        // Adaptive toward L=0.5
    ALWAN_GAMUT_MAP_ADAPTIVE_CUSP,      // Adaptive toward cusp
    ALWAN_GAMUT_MAP_CHROMA_COMPRESS,    // Chroma compression
    ALWAN_GAMUT_MAP_SGCK,               // SGCK 2004
    ALWAN_GAMUT_MAP_HPMINDE,            // Hue-Preserving Minimum ΔE
    ALWAN_GAMUT_MAP_LIGHTNESS_PRESERVE  // Lightness Preserving
} alwan_gamut_map_method;
```

---

## Functions

### alwan_gamut_map

```c
int alwan_gamut_map(
    alwan_scalar *rgb_out,            // Output mapped RGB
    alwan_gamut_map_method method,
    alwan_scalar const *rgb_in,       // Input (may be out of gamut)
    size_t count,
    size_t in_stride,                 // Input stride in bytes
    size_t out_stride                 // Output stride in bytes
);
```

Maps RGB colors to [0,1] gamut using specified method.

**Example:**
```c
alwan_rgb rgb_in = {1.2, 0.5, -0.1};  // Out of gamut
alwan_rgb rgb_out;
alwan_gamut_map((alwan_scalar*)&rgb_out, ALWAN_GAMUT_MAP_CLIP,
                (alwan_scalar*)&rgb_in, 1,
                sizeof(alwan_rgb), sizeof(alwan_rgb));
// rgb_out = {1.0, 0.5, 0.0}
```

---

### alwan_gamut_map_xyz_to_rgb

```c
int alwan_gamut_map_xyz_to_rgb(
    alwan_rgb *rgb_out,
    alwan_ctx *ctx,
    alwan_rgb_space_desc const *space,
    alwan_xyz const *xyz_in
);
```

Maps XYZ color to RGB gamut with hue preservation (JCh-based).

---

### alwan_gamut_volume_mc

```c
int alwan_gamut_volume_mc(
    alwan_scalar *volume,             // Output volume estimate
    alwan_rgb_space_desc const *space,
    size_t num_samples,               // Number of Monte Carlo samples
    unsigned int seed                 // Random seed for reproducibility
);
```

Estimates RGB gamut volume using Monte Carlo sampling.

**Example:**
```c
alwan_rgb_space_desc srgb_desc;
alwan_rgb_get_space_descriptor(&srgb_desc, ctx, ALWAN_RGB_SPACE_SRGB);

alwan_scalar volume;
alwan_gamut_volume_mc(&volume, &srgb_desc, 100000, 42);
```

---

## Manual Gamut Checking

Check if RGB is within [0,1] bounds:

```c
static inline int is_in_gamut(alwan_rgb const *rgb) {
    return rgb->r >= 0.0 && rgb->r <= 1.0 &&
           rgb->g >= 0.0 && rgb->g <= 1.0 &&
           rgb->b >= 0.0 && rgb->b <= 1.0;
}
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — Invalid method or NULL pointer

---

## See Also

- [Color Spaces](color-spaces.md) — RGB conversions
- [Chromatic Adaptation](chromatic-adaptation.md) — White point transforms
