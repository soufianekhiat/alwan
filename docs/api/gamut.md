# Gamut Operations API

Functions for gamut mapping, volume calculation, and coverage analysis.

> **Precision variants:** Every function and type shown as `name_{T}` exists in two forms:
> `name_f32` (single precision, `float`) and `name_f64` (double precision, `double`).
> `T = f32 | f64`.

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
    ALWAN_GAMUT_MAP_CLIP = 0,            /* Simple clipping to [0,1] */
    ALWAN_GAMUT_MAP_HUE_PRESERVING = 1,  /* Project to gamut boundary preserving hue */
    ALWAN_GAMUT_MAP_ADAPTIVE_L0,         /* Adaptive toward L=0.5 */
    ALWAN_GAMUT_MAP_ADAPTIVE_CUSP,       /* Adaptive toward cusp */
    ALWAN_GAMUT_MAP_CHROMA_COMPRESS,     /* Chroma compression */
    ALWAN_GAMUT_MAP_SGCK,                /* SGCK 2004 */
    ALWAN_GAMUT_MAP_HPMINDE,             /* Hue-Preserving Minimum dE */
    ALWAN_GAMUT_MAP_LIGHTNESS_PRESERVE   /* Lightness Preserving */
} alwan_gamut_map_method;
```

---

## Functions

### alwan_gamut_{T}_map_interleave

```c
int alwan_gamut_{T}_map_interleave(
    alwan_{T} *rgb_out,
    alwan_{T} const *rgb_in,
    alwan_gamut_map_method method,
    size_t count,
    size_t in_stride,
    size_t out_stride);
```

Maps RGB colors to [0,1] gamut using the specified method.

**Example:**
```c
alwan_rgb_{T} rgb_in  = {1.2, 0.5, -0.1};  /* out of gamut */
alwan_rgb_{T} rgb_out;
alwan_gamut_{T}_map_interleave((alwan_{T}*)&rgb_out, (alwan_{T} const*)&rgb_in,
                                ALWAN_GAMUT_MAP_CLIP, 1,
                                sizeof(alwan_rgb_{T}), sizeof(alwan_rgb_{T}));
/* rgb_out = {1.0, 0.5, 0.0} */
```

---

### alwan_gamut_map_advanced_{T}

```c
int alwan_gamut_map_advanced_{T}(alwan_rgb_{T} *rgb_out,
                                  alwan_gamut_map_method method,
                                  alwan_rgb_space_desc_{T} const *space,
                                  alwan_rgb_{T} const *rgb_linear);
```

Advanced gamut mapping with awareness of the target RGB space's gamut boundary.

---

### alwan_css_gamut_map_interleave (no precision suffix -- operates on alwan_f64)

CSS Color Level 4 gamut mapping algorithm. Maps out-of-gamut colors using the binary search
algorithm from the CSS Color Level 4 specification.

---

### alwan_gamut_volume_mc_{T}

```c
int alwan_gamut_volume_mc_{T}(alwan_{T} *volume,
                               alwan_rgb_space_desc_{T} const *space,
                               size_t num_samples,
                               unsigned int seed);
```

Estimates RGB gamut volume using Monte Carlo sampling.

**Example:**
```c
alwan_rgb_space_desc_{T} srgb_desc;
alwan_rgb_get_space_descriptor_{T}(&srgb_desc, ctx, ALWAN_RGB_SPACE_SRGB);

alwan_{T} volume;
alwan_gamut_volume_mc_{T}(&volume, &srgb_desc, 100000, 42);
```

---

### alwan_gamut_volume_ratio_{T}

```c
int alwan_gamut_volume_ratio_{T}(alwan_{T} *ratio_out,
                                  alwan_rgb_space_desc_{T} const *space1,
                                  alwan_rgb_space_desc_{T} const *space2);
```

Calculate the volume ratio between two RGB gamuts.

---

### alwan_gamut_coverage_{T}

```c
int alwan_gamut_coverage_{T}(alwan_{T} *coverage_out,
                              alwan_rgb_space_desc_{T} const *space1,
                              alwan_rgb_space_desc_{T} const *space2,
                              size_t num_samples,
                              unsigned int seed);
```

Estimate what percentage of `space1`'s gamut is covered by `space2` using Monte Carlo sampling.

---

### Pointer's Gamut

```c
int alwan_is_within_pointer_gamut_{T}(alwan_vec2_{T} const *xy);
```

Test whether an xy chromaticity falls within Pointer's gamut (the gamut of real surface colors).

---

### Dominant Wavelength and Excitation Purity

```c
int alwan_spectral_locus_xy_{T}(alwan_vec2_{T} *xy_out, alwan_{T} wavelength);

int alwan_dominant_wavelength_{T}(alwan_{T} *wavelength_out,
                                   alwan_vec2_{T} *xy_wl_out, alwan_vec2_{T} *xy_cw_out,
                                   alwan_vec2_{T} const *xy, alwan_vec2_{T} const *xy_white);

int alwan_excitation_purity_{T}(alwan_{T} *purity_out,
                                 alwan_vec2_{T} const *xy, alwan_vec2_{T} const *xy_white);

int alwan_complementary_wavelength_{T}(alwan_{T} *wavelength_out,
                                        alwan_vec2_{T} *xy_wl_out, alwan_vec2_{T} *xy_cw_out,
                                        alwan_vec2_{T} const *xy, alwan_vec2_{T} const *xy_white);
```

---

## Manual Gamut Checking

Check if RGB is within [0,1] bounds:

```c
static int is_in_gamut(alwan_rgb_{T} const *rgb) {
    return rgb->r >= 0.0 && rgb->r <= 1.0 &&
           rgb->g >= 0.0 && rgb->g <= 1.0 &&
           rgb->b >= 0.0 && rgb->b <= 1.0;
}
```

---

## Error Codes

- `ALWAN_OK` (0) -- Success
- `ALWAN_E_INVALID` (-1) -- Invalid method or NULL pointer

---

## See Also

- [Color Spaces](color-spaces.md) -- RGB conversions
- [Chromatic Adaptation](chromatic-adaptation.md) -- White point transforms
