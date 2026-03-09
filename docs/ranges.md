# Color Space Channel Ranges

This document describes the valid ranges for each channel of every color space supported by Alwan.

**Legend:**
- `[a, b]` — Bounded range from a to b (inclusive)
- `[0, +inf[` — Unbounded above
- `]-inf, +inf[` — Fully unbounded
- **Typical** — Common working range (actual values may exceed)

---

## Table of Contents

1. [Device-Dependent Spaces](#device-dependent-spaces)
2. [CIE Colorimetric Spaces](#cie-colorimetric-spaces)
3. [Modern Perceptual Spaces](#modern-perceptual-spaces)
4. [Color Appearance Models](#color-appearance-models)
5. [Specialized Spaces](#specialized-spaces)

---

## Device-Dependent Spaces

### RGB (`alwan_rgb`)

| Channel | Range | Description |
|---------|-------|-------------|
| `r` | [0, 1] | Red (may exceed for HDR/wide gamut) |
| `g` | [0, 1] | Green (may exceed for HDR/wide gamut) |
| `b` | [0, 1] | Blue (may exceed for HDR/wide gamut) |

**Notes:** Linear RGB values are typically [0, 1] for in-gamut colors. Scene-referred HDR content may have values > 1. Out-of-gamut colors may have negative values.

### HSV (`alwan_hsv`)

| Channel | Range | Description |
|---------|-------|-------------|
| `h` | [0, 1] | Hue (normalized, 0=red, 0.33=green, 0.67=blue) |
| `s` | [0, 1] | Saturation |
| `v` | [0, 1] | Value (brightness) |

**Notes:** Hue is normalized to [0, 1] instead of degrees. Multiply by 360 for degrees.

### HSL (`alwan_hsl`)

| Channel | Range | Description |
|---------|-------|-------------|
| `h` | [0, 1] | Hue (normalized, 0=red, 0.33=green, 0.67=blue) |
| `s` | [0, 1] | Saturation |
| `l` | [0, 1] | Lightness |

**Notes:** Hue is normalized to [0, 1] instead of degrees. Multiply by 360 for degrees.

### HWB (CSS Color Level 4)

| Channel | Range | Description |
|---------|-------|-------------|
| `H` | [0, 1] | Hue (normalized, same as HSV/HSL) |
| `W` | [0, 1] | Whiteness |
| `B` | [0, 1] | Blackness |

**Notes:** W + B can exceed 1.0 (colors are normalized). Derived from HSV. Part of CSS Color Level 4 specification.

### CMY (`alwan_cmy`)

| Channel | Range | Description |
|---------|-------|-------------|
| `c` | [0, 1] | Cyan |
| `m` | [0, 1] | Magenta |
| `y` | [0, 1] | Yellow |

### CMYK (`alwan_cmyk`)

| Channel | Range | Description |
|---------|-------|-------------|
| `c` | [0, 1] | Cyan |
| `m` | [0, 1] | Magenta |
| `y` | [0, 1] | Yellow |
| `k` | [0, 1] | Key (black) |

### YCbCr (`alwan_ycbcr`)

| Channel | Range | Description |
|---------|-------|-------------|
| `Y` | [0, 1] | Luma |
| `Cb` | [-0.5, 0.5] | Blue-difference chroma |
| `Cr` | [-0.5, 0.5] | Red-difference chroma |

**Notes:** Full-range YCbCr. For studio/limited range (16-235), scale accordingly.

### YCoCg (`alwan_ycocg`)

| Channel | Range | Description |
|---------|-------|-------------|
| `Y` | [0, 1] | Luma |
| `Co` | [-0.5, 0.5] | Orange-cyan chrominance |
| `Cg` | [-0.5, 0.5] | Green-magenta chrominance |

### Y'Cb'Cr'c' (`alwan_yccbccrc`)

| Channel | Range | Description |
|---------|-------|-------------|
| `Yc` | [0, 1] | Constant luminance luma |
| `Cbc` | [-0.5, 0.5] | Blue-difference chroma |
| `Crc` | [-0.5, 0.5] | Red-difference chroma |

---

## CIE Colorimetric Spaces

### XYZ (`alwan_xyz`)

| Channel | Range | Description |
|---------|-------|-------------|
| `x` | [0, +inf[ | X tristimulus (typical ~0.95 for D65 white) |
| `y` | [0, +inf[ | Y tristimulus (luminance, 1.0 = reference white) |
| `z` | [0, +inf[ | Z tristimulus (typical ~1.09 for D65 white) |

**Notes:** Y is often normalized to 1.0 or 100.0 depending on convention. For absolute luminance, Y is in cd/m^2.

### xyY (`alwan_xyy`)

| Channel | Range | Description |
|---------|-------|-------------|
| `x` | [0, 1] | x chromaticity coordinate |
| `y` | [0, 1] | y chromaticity coordinate |
| `Y` | [0, +inf[ | Luminance |

**Notes:** x + y + z = 1 (where z is implicit). Valid chromaticities lie within the spectral locus.

### Lab (`alwan_lab`) — CIE L\*a\*b\*

| Channel | Range | Description |
|---------|-------|-------------|
| `L` | [0, 100] | Lightness |
| `a` | ~[-128, 127] | Green-red axis (unbounded) |
| `b` | ~[-128, 127] | Blue-yellow axis (unbounded) |

**Notes:** L\* is bounded [0, 100]. a\* and b\* are theoretically unbounded but typically within ±128 for real colors. Wide-gamut colors may exceed these values.

### Luv (`alwan_luv`) — CIE L\*u\*v\*

| Channel | Range | Description |
|---------|-------|-------------|
| `L` | [0, 100] | Lightness |
| `u` | ~[-134, 220] | u\* chromaticity (unbounded) |
| `v` | ~[-140, 122] | v\* chromaticity (unbounded) |

**Notes:** L\* is bounded [0, 100]. u\* and v\* are theoretically unbounded.

### LCh(ab) (`alwan_lch`) — Cylindrical Lab

| Channel | Range | Description |
|---------|-------|-------------|
| `L` | [0, 100] | Lightness |
| `C` | [0, +inf[ | Chroma |
| `h` | [0, 360[ | Hue angle in degrees |

### LCh(uv) (`alwan_lchuv`) — Cylindrical Luv

| Channel | Range | Description |
|---------|-------|-------------|
| `L` | [0, 100] | Lightness |
| `C` | [0, +inf[ | Chroma |
| `h` | [0, 360[ | Hue angle in degrees |

### UVW (`alwan_uvw`) — CIE 1964 U\*V\*W\*

| Channel | Range | Description |
|---------|-------|-------------|
| `U` | ]-inf, +inf[ | U\* coordinate |
| `V` | ]-inf, +inf[ | V\* coordinate |
| `W` | [0, +inf[ | W\* (lightness-like) |

**Notes:** Historical space, primarily used for CRI calculations.

### UCS (`alwan_ucs`) — CIE 1960 UCS

| Channel | Range | Description |
|---------|-------|-------------|
| `U` | [0, 1] | u chromaticity |
| `V` | [0, 1] | v chromaticity |
| `W` | [0, +inf[ | Y luminance |

**Notes:** Precursor to CIE 1976 u'v'. Used for CCT calculations.

### Hunter Lab (`alwan_hunter_lab`)

| Channel | Range | Description |
|---------|-------|-------------|
| `L` | [0, 100] | Lightness |
| `a` | ~[-100, 100] | Red-green (unbounded) |
| `b` | ~[-100, 100] | Yellow-blue (unbounded) |

**Notes:** Uses square roots instead of cube roots (unlike CIE Lab).

### DIN99 (`alwan_din99`)

| Channel | Range | Description |
|---------|-------|-------------|
| `L99` | [0, 100] | Lightness |
| `a99` | ]-inf, +inf[ | Red-green axis |
| `b99` | ]-inf, +inf[ | Yellow-blue axis |

**Notes:** German standard for improved perceptual uniformity. Variants: DIN99, DIN99b, DIN99c, DIN99d.

---

## Modern Perceptual Spaces

### Oklab (`alwan_oklab`)

| Channel | Range | Description |
|---------|-------|-------------|
| `L` | [0, 1] | Lightness |
| `a` | ~[-0.4, 0.4] | Green-red axis (unbounded) |
| `b` | ~[-0.4, 0.4] | Blue-yellow axis (unbounded) |

**Notes:** Designed for perceptual uniformity. L is [0, 1] for SDR; a and b are unbounded but typically small.

### Oklch (`alwan_oklch`) — Cylindrical Oklab

| Channel | Range | Description |
|---------|-------|-------------|
| `L` | [0, 1] | Lightness |
| `C` | [0, +inf[ | Chroma |
| `h` | [-pi, pi] | Hue angle in radians |

**Notes:** Hue is in radians (output of `atan2`). Convert to degrees with `h * 180 / pi`.

### Jzazbz (`alwan_jzazbz`)

| Channel | Range | Description |
|---------|-------|-------------|
| `Jz` | [0, 1] | Lightness (PQ-based, HDR) |
| `az` | ~[-0.5, 0.5] | Red-green axis |
| `bz` | ~[-0.5, 0.5] | Yellow-blue axis |

**Notes:** HDR perceptual space. Jz uses PQ transfer and can represent 0-10,000 cd/m^2.

### JzCzhz (`alwan_jzczhz`) — Cylindrical Jzazbz

| Channel | Range | Description |
|---------|-------|-------------|
| `Jz` | [0, 1] | Lightness |
| `Cz` | [0, +inf[ | Chroma |
| `hz` | [-pi, pi] | Hue angle in radians |

**Notes:** Hue is in radians (output of `atan2`). Convert to degrees with `hz * 180 / pi`.

### ICtCp (`alwan_ictcp`)

| Channel | Range | Description |
|---------|-------|-------------|
| `I` | [0, 1] | Intensity (PQ or HLG encoded) |
| `Ct` | ~[-0.5, 0.5] | Tritan (blue-yellow) axis |
| `Cp` | ~[-0.5, 0.5] | Protan (red-green) axis |

**Notes:** ITU-R BT.2100 HDR space. Requires linear BT.2020 RGB input.

### IPT (`alwan_ipt`)

| Channel | Range | Description |
|---------|-------|-------------|
| `I` | [0, 1] | Intensity/lightness |
| `P` | ~[-1, 1] | Protan (red-green) |
| `T` | ~[-1, 1] | Tritan (yellow-blue) |

**Notes:** Improved hue uniformity over CIELAB.

### IPTch (`alwan_iptch`) — Cylindrical IPT

| Channel | Range | Description |
|---------|-------|-------------|
| `I` | [0, 1] | Intensity |
| `C` | [0, +inf[ | Chroma |
| `h` | [-pi, pi] | Hue angle in radians |

**Notes:** Hue is in radians (output of `atan2`). Convert to degrees with `h * 180 / pi`.

### IgPgTg (`alwan_igpgtg`)

| Channel | Range | Description |
|---------|-------|-------------|
| `Ig` | [0, 1] | Intensity |
| `Pg` | ]-inf, +inf[ | Red-green |
| `Tg` | ]-inf, +inf[ | Yellow-blue |

**Notes:** Ebner & Fairchild (1998) improved IPT variant.

### ICaCb (`alwan_icacb`)

| Channel | Range | Description |
|---------|-------|-------------|
| `I` | [0, +inf[ | Intensity |
| `Ca` | ]-inf, +inf[ | Chromatic axis a |
| `Cb` | ]-inf, +inf[ | Chromatic axis b |

**Notes:** Zhang & Wandell (1996, 1997) image difference space.

### ProLab (`alwan_prolab`)

| Channel | Range | Description |
|---------|-------|-------------|
| `L` | [0, 100] | Lightness |
| `a` | ]-inf, +inf[ | Red-green |
| `b` | ]-inf, +inf[ | Yellow-blue |

**Notes:** Konovalenko et al. (2021) projective uniform space.

### OSA-UCS (`alwan_osa_ucs`)

| Channel | Range | Description |
|---------|-------|-------------|
| `L` | ~[-10, 8] | Lightness (centered near 0) |
| `j` | ~[-15, 15] | Yellowness (j > 0) / Blueness (j < 0) |
| `g` | ~[-15, 15] | Greenness (g > 0) / Redness (g < 0) |

**Notes:** OSA Uniform Color Scales. L=0 is mid-gray, not black.

---

## Color Appearance Models

All CAMs output perceptual correlates that depend on viewing conditions.

### CIECAM02 (`alwan_ciecam02_correlates`)

| Channel | Range | Description |
|---------|-------|-------------|
| `J` | [0, 100] | Lightness |
| `C` | [0, +inf[ | Chroma |
| `h` | [0, 360[ | Hue angle in degrees |
| `s` | [0, +inf[ | Saturation |
| `Q` | [0, +inf[ | Brightness |
| `M` | [0, +inf[ | Colorfulness |
| `H` | [0, 400] | Hue quadrature |

**Notes:** H cycles through Red(0)→Yellow(100)→Green(200)→Blue(300)→Red(400).

### CAM16 (`alwan_cam16_correlates`)

| Channel | Range | Description |
|---------|-------|-------------|
| `J` | [0, 100] | Lightness |
| `C` | [0, +inf[ | Chroma |
| `h` | [0, 360[ | Hue angle in degrees |
| `s` | [0, +inf[ | Saturation |
| `Q` | [0, +inf[ | Brightness |
| `M` | [0, +inf[ | Colorfulness |
| `H` | [0, 400] | Hue quadrature |

**Notes:** Improved version of CIECAM02 with CAT16 chromatic adaptation.

### CAM Jab (`alwan_cam_jab`) — UCS Coordinates

| Channel | Range | Description |
|---------|-------|-------------|
| `J` | [0, 100] | J' (transformed lightness) |
| `a` | ]-inf, +inf[ | a' (red-green) |
| `b` | ]-inf, +inf[ | b' (yellow-blue) |

**Notes:** Used for CAM02-UCS/LCD/SCD and CAM16-UCS/LCD/SCD color differences.

### ZCAM (`alwan_zcam_correlates`)

| Channel | Range | Description |
|---------|-------|-------------|
| `Jz` | [0, 100] | Lightness |
| `Cz` | [0, +inf[ | Chroma |
| `hz` | [0, 360[ | Hue angle in degrees |
| `Qz` | [0, +inf[ | Brightness |
| `Mz` | [0, +inf[ | Colorfulness |
| `Sz` | [0, +inf[ | Saturation |
| `Vz` | [0, +inf[ | Vividness |
| `Kz` | [0, 100] | Blackness |
| `Wz` | [0, 100] | Whiteness |

**Notes:** HDR color appearance model supporting 0.001-10,000 cd/m^2.

### Hellwig2022 (`alwan_hellwig2022_correlates`)

| Channel | Range | Description |
|---------|-------|-------------|
| `J` | [0, 100] | Lightness |
| `C` | [0, +inf[ | Chroma |
| `h` | [0, 360[ | Hue angle in degrees |
| `s` | [0, +inf[ | Saturation |
| `Q` | [0, +inf[ | Brightness |
| `M` | [0, +inf[ | Colorfulness |

**Notes:** Improved CAM16 with Helmholtz-Kohlrausch effect support.

### Hunt (`alwan_hunt_correlates`)

| Channel | Range | Description |
|---------|-------|-------------|
| `J` | [0, 100] | Lightness |
| `C` | [0, +inf[ | Chroma |
| `h` | [0, 360[ | Hue angle |
| `s` | [0, +inf[ | Saturation |
| `Q` | [0, +inf[ | Brightness |
| `M` | [0, +inf[ | Colorfulness |

**Notes:** Comprehensive historical CAM (Hunt 1991, 1995). Forward-only.

### Kim2009 (`alwan_kim2009_correlates`)

| Channel | Range | Description |
|---------|-------|-------------|
| `J` | [0, 100] | Lightness |
| `C` | [0, +inf[ | Chroma |
| `h` | [0, 360[ | Hue angle |

### LLAB (`alwan_llab_correlates`)

| Channel | Range | Description |
|---------|-------|-------------|
| `L` | [0, 100] | Lightness |
| `Ch` | [0, +inf[ | Chroma |
| `h` | [0, 360[ | Hue angle |
| `s` | [0, +inf[ | Saturation |

**Notes:** Luo, Lo and Kuo (1996) appearance model.

### ATD95 (`alwan_atd95_correlates`)

| Channel | Range | Description |
|---------|-------|-------------|
| `H` | [0, 360[ | Hue |
| `C` | [0, +inf[ | Chroma |
| `Br` | [0, +inf[ | Brightness |
| `A_1` | ]-inf, +inf[ | Achromatic response 1 |
| `T_1` | ]-inf, +inf[ | Tritanopic response 1 |
| `D_1` | ]-inf, +inf[ | Deuteranopic response 1 |
| `A_2` | ]-inf, +inf[ | Achromatic response 2 |
| `T_2` | ]-inf, +inf[ | Tritanopic response 2 |
| `D_2` | ]-inf, +inf[ | Deuteranopic response 2 |

**Notes:** Guth's ATD (1995) advanced temporal dynamics model.

### RLAB (`alwan_rlab_correlates`)

| Channel | Range | Description |
|---------|-------|-------------|
| `L` | [0, 100] | Lightness |
| `C` | [0, +inf[ | Chroma |
| `h` | [0, 360[ | Hue angle in degrees |
| `s` | [0, +inf[ | Saturation |
| `a` | ]-inf, +inf[ | Red-green opponent |
| `b` | ]-inf, +inf[ | Yellow-blue opponent |

**Notes:** Fairchild (1993, 1996) cross-media color reproduction model.

### Nayatani95 (`alwan_nayatani95_correlates`)

| Channel | Range | Description |
|---------|-------|-------------|
| `L_star_N` | [0, 100] | Perceived lightness |
| `C` | [0, +inf[ | Chroma |
| `theta` | [0, 2pi[ | Hue angle in radians |
| `S` | [0, +inf[ | Saturation |
| `B_r` | [0, +inf[ | Brightness |
| `L_star_P` | [0, +inf[ | Brightness-to-lightness ratio |

---

## Specialized Spaces

### Prismatic (`alwan_prismatic`)

| Channel | Range | Description |
|---------|-------|-------------|
| `L` | [0, 1] | Lightness (max of R, G, B) |
| `s` | [0, 1] | Normalized R component (R / (R+G+B)) |
| `h` | [0, 1] | Normalized G component (G / (R+G+B)) |

**Notes:** Pridmore (2021). Note: `s` and `h` are NOT saturation/hue angles but normalized RGB ratios. The implicit third component is `1 - s - h` (normalized B).

### HCL (`alwan_hcl`)

| Channel | Range | Description |
|---------|-------|-------------|
| `H` | [-pi, pi] | Hue angle in radians |
| `C` | [0, +inf[ | Chroma |
| `L` | [0, 1] | Luminance |

**Notes:** Sarifuddin (2005) Hue-Chroma-Luminance space. Hue is in radians.

### IHLS (`alwan_ihls`)

| Channel | Range | Description |
|---------|-------|-------------|
| `H` | [0, 2pi[ | Hue angle in radians |
| `L` | [0, 1] | Lightness (Y luminance) |
| `S` | [0, 1] | Saturation (delta = max - min) |

**Notes:** Hanbury (2003) Improved HLS. Hue is in radians [0, 2pi[.

---

## Summary Table

| Space | Type | L/Y Range | Chroma/Sat Range | Hue Range | Hue Unit |
|-------|------|-----------|------------------|-----------|----------|
| **Lab** | CIE | [0, 100] | unbounded | — | — |
| **LCh(ab)** | CIE | [0, 100] | [0, +inf[ | [0, 360[ | degrees |
| **Luv** | CIE | [0, 100] | unbounded | — | — |
| **LCh(uv)** | CIE | [0, 100] | [0, +inf[ | [0, 360[ | degrees |
| **Oklab** | Modern | [0, 1] | ~±0.4 | — | — |
| **Oklch** | Modern | [0, 1] | [0, +inf[ | [-pi, pi] | radians |
| **Jzazbz** | HDR | [0, 1] | ~±0.5 | — | — |
| **JzCzhz** | HDR | [0, 1] | [0, +inf[ | [-pi, pi] | radians |
| **ICtCp** | HDR | [0, 1] | ~±0.5 | — | — |
| **IPTch** | Modern | [0, 1] | [0, +inf[ | [-pi, pi] | radians |
| **CIECAM02** | CAM | [0, 100] | [0, +inf[ | [0, 360[ | degrees |
| **CAM16** | CAM | [0, 100] | [0, +inf[ | [0, 360[ | degrees |
| **ZCAM** | HDR CAM | [0, 100] | [0, +inf[ | [0, 360[ | degrees |
| **HSV/HSL** | Device | [0, 1] | [0, 1] | [0, 1] | normalized |
| **HCL** | Specialized | [0, 1] | [0, +inf[ | [-pi, pi] | radians |
| **IHLS** | Specialized | [0, 1] | [0, 1] | [0, 2pi[ | radians |

---

## Notes on Out-of-Range Values

1. **Scene-referred HDR:** RGB values may exceed [0, 1] for highlights
2. **Wide-gamut colors:** Lab a\*/b\* may exceed ±128
3. **Negative RGB:** Indicates out-of-gamut for destination space
4. **Chroma unbounded:** CAM chroma values depend on viewing conditions
5. **Hue discontinuity:** Hue wraps at its maximum (360°, 2pi, or 1.0 depending on space)

## Hue Convention Summary

Alwan uses different hue conventions depending on the color space:

| Convention | Spaces | Conversion |
|------------|--------|------------|
| **Degrees [0, 360[** | LCh(ab), LCh(uv), CIECAM02, CAM16, ZCAM, Hellwig2022, Hunt, Kim2009, LLAB, ATD95, RLAB | — |
| **Radians [-pi, pi]** | Oklch, JzCzhz, IPTch, HCL | `degrees = h * 180 / pi` |
| **Radians [0, 2pi[** | IHLS, Nayatani95 | `degrees = h * 180 / pi` |
| **Normalized [0, 1]** | HSV, HSL | `degrees = h * 360` |
| **Not a hue** | Prismatic | `s` and `h` are normalized RGB ratios |

## References

- CIE 015:2018 Colorimetry
- CIE 159:2004 A Colour Appearance Model for Colour Management Systems: CIECAM02
- Li et al. (2017) CAM16
- Safdar et al. (2017) Jzazbz
- Bjorn Ottosson (2020) Oklab
- ITU-R BT.2100 HDR
