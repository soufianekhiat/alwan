# Color Space Conversions API

Functions for converting between different color spaces and models.

---

## Overview

Alwan supports conversions between:
- **CIE spaces:** XYZ, xyY, Lab, Luv, LCh(ab), LCh(uv), UVW
- **Perceptual models:** IPT, ICtCp, JzAzBz, Oklab, Oklch, Hunter Lab, ProLab, OSA-UCS, DIN99, hdr-CIELAB, hdr-IPT, IgPgTg, ICaCb
- **RGB families:** sRGB, Adobe RGB, BT.709, BT.2020, Display P3, ACES, and 100+ more
- **Encoding spaces:** HSV, HSL, HSP, HSPLog, HSY, HWB, YCbCr, YCoCg, YcCbcCrc, CMY, CMYK, HCL, HLC, IHLS, Prismatic, CubeHelix, HSLuv, HPLuv, OkHSL, OkHSV
- **Relative luminance:** Multi-standard Y calculation from linear RGB

**API Pattern:** Single-element functions use semantic types. Bulk functions use `alwan_scalar*` with strides.

---

## CIE XYZ Conversions

### alwan_xyz_to_lab / alwan_xyz_to_lab_bulk

```c
// Single element
void alwan_xyz_to_lab(alwan_lab *lab, alwan_xyz const *xyz, alwan_xyz const *white_xyz);

// Bulk with strides (in bytes)
int alwan_xyz_to_lab_bulk(alwan_scalar *lab_out, alwan_scalar const *xyz_in,
                          alwan_xyz const *white_xyz, size_t count,
                          size_t in_stride, size_t out_stride);
```

**Example:**
```c
alwan_xyz xyz = {0.5, 0.6, 0.4};
alwan_xyz d65 = {0.95047, 1.0, 1.08883};
alwan_lab lab;
alwan_xyz_to_lab(&lab, &xyz, &d65);
printf("Lab: L=%.2f, a=%.2f, b=%.2f\n", lab.L, lab.a, lab.b);
```

---

### alwan_lab_to_xyz / alwan_lab_to_xyz_bulk

```c
void alwan_lab_to_xyz(alwan_xyz *xyz, alwan_lab const *lab, alwan_xyz const *white_xyz);
int alwan_lab_to_xyz_bulk(alwan_scalar *xyz_out, alwan_scalar const *lab_in,
                          alwan_xyz const *white_xyz, size_t count,
                          size_t in_stride, size_t out_stride);
```

---

### alwan_xyz_to_luv / alwan_xyz_to_luv_bulk

```c
void alwan_xyz_to_luv(alwan_luv *luv, alwan_xyz const *xyz, alwan_xyz const *white_xyz);
int alwan_xyz_to_luv_bulk(alwan_scalar *luv_out, alwan_scalar const *xyz_in,
                          alwan_xyz const *white_xyz, size_t count,
                          size_t in_stride, size_t out_stride);
```

---

### alwan_luv_to_xyz / alwan_luv_to_xyz_bulk

```c
void alwan_luv_to_xyz(alwan_xyz *xyz, alwan_luv const *luv, alwan_xyz const *white_xyz);
int alwan_luv_to_xyz_bulk(alwan_scalar *xyz_out, alwan_scalar const *luv_in,
                          alwan_xyz const *white_xyz, size_t count,
                          size_t in_stride, size_t out_stride);
```

---

### alwan_xyz_to_xyy / alwan_xyy_to_xyz

```c
void alwan_xyz_to_xyy(alwan_xyy *xyy, alwan_xyz const *xyz);
void alwan_xyy_to_xyz(alwan_xyz *xyz, alwan_xyy const *xyy);
```

---

## Cylindrical Representations

### alwan_lab_to_lch / alwan_lch_to_lab

```c
void alwan_lab_to_lch(alwan_lch *lch, alwan_lab const *lab);
void alwan_lch_to_lab(alwan_lab *lab, alwan_lch const *lch);

// Bulk versions
int alwan_lab_to_lch_bulk(alwan_scalar *lch_out, alwan_scalar const *lab_in,
                          size_t count, size_t in_stride, size_t out_stride);
int alwan_lch_to_lab_bulk(alwan_scalar *lab_out, alwan_scalar const *lch_in,
                          size_t count, size_t in_stride, size_t out_stride);
```

**Output format:** L: [0, 100], C: [0, inf), h: [0, 360) degrees

---

### alwan_luv_to_lchuv / alwan_lchuv_to_luv

```c
void alwan_luv_to_lchuv(alwan_lchuv *lchuv, alwan_luv const *luv);
void alwan_lchuv_to_luv(alwan_luv *luv, alwan_lchuv const *lchuv);

// Bulk versions available
```

---

## Modern Perceptual Models

### alwan_xyz_to_oklab / alwan_oklab_to_xyz

```c
void alwan_xyz_to_oklab(alwan_oklab *oklab, alwan_xyz const *xyz);
void alwan_oklab_to_xyz(alwan_xyz *xyz, alwan_oklab const *oklab);

// Bulk versions
int alwan_xyz_to_oklab_bulk(alwan_scalar *oklab_out, alwan_scalar const *xyz_in,
                            size_t count, size_t in_stride, size_t out_stride);
```

**Advantages over Lab:** Better hue linearity, better chroma uniformity.

---

### alwan_oklab_to_oklch / alwan_oklch_to_oklab

```c
void alwan_oklab_to_oklch(alwan_oklch *oklch, alwan_oklab const *oklab);
void alwan_oklch_to_oklab(alwan_oklab *oklab, alwan_oklch const *oklch);
```

---

### alwan_xyz_to_jzazbz / alwan_jzazbz_to_xyz

```c
void alwan_xyz_to_jzazbz(alwan_jzazbz *jzazbz, alwan_xyz const *xyz);
void alwan_jzazbz_to_xyz(alwan_xyz *xyz, alwan_jzazbz const *jzazbz);
```

**Use case:** HDR color difference calculations, tone mapping.

---

### alwan_xyz_to_ipt / alwan_ipt_to_xyz

```c
void alwan_xyz_to_ipt(alwan_ipt *ipt, alwan_xyz const *xyz);
void alwan_ipt_to_xyz(alwan_xyz *xyz, alwan_ipt const *ipt);
```

---

### alwan_xyz_to_ictcp / alwan_ictcp_to_xyz

```c
void alwan_xyz_to_ictcp(alwan_ictcp *ictcp, alwan_xyz const *xyz,
                        alwan_transfer_function tf);  // ALWAN_TF_PQ or ALWAN_TF_HLG
void alwan_ictcp_to_xyz(alwan_xyz *xyz, alwan_ictcp const *ictcp,
                        alwan_transfer_function tf);
```

---

## RGB Conversions

### alwan_rgb_to_xyz / alwan_xyz_to_rgb

```c
int alwan_rgb_to_xyz(alwan_xyz *xyz, alwan_rgb_space_desc const *space, alwan_rgb const *rgb);
int alwan_xyz_to_rgb(alwan_rgb *rgb, alwan_rgb_space_desc const *space, alwan_xyz const *xyz);
```

---

### alwan_rgb_convert

```c
int alwan_rgb_convert(alwan_rgb *dst_rgb, alwan_ctx *ctx,
                      alwan_rgb_space_desc const *src_space,
                      alwan_rgb_space_desc const *dst_space,
                      alwan_rgb const *src_rgb);

// Bulk version
int alwan_rgb_convert_bulk(alwan_rgb *dst_rgb, alwan_ctx *ctx,
                           alwan_rgb_space_desc const *src_space,
                           alwan_rgb_space_desc const *dst_space,
                           alwan_rgb const *src_rgb, size_t count);
```

**Example:**
```c
alwan_rgb_space_desc srgb_desc, bt2020_desc;
alwan_rgb_get_space_descriptor(&srgb_desc, ctx, ALWAN_RGB_SPACE_SRGB);
alwan_rgb_get_space_descriptor(&bt2020_desc, ctx, ALWAN_RGB_SPACE_BT2020);

alwan_rgb rgb_in = {0.8, 0.3, 0.2};
alwan_rgb rgb_out;
alwan_rgb_convert(&rgb_out, ctx, &srgb_desc, &bt2020_desc, &rgb_in);
```

---

## sRGB Convenience Functions

Direct conversions for sRGB (D65 white point):

```c
int alwan_srgb_to_xyz(alwan_xyz *xyz, alwan_rgb const *rgb);
int alwan_xyz_to_srgb(alwan_rgb *rgb, alwan_xyz const *xyz);
int alwan_srgb_to_lab(alwan_lab *lab, alwan_rgb const *rgb);
int alwan_lab_to_srgb(alwan_rgb *rgb, alwan_lab const *lab);
int alwan_srgb_to_oklab(alwan_oklab *oklab, alwan_rgb const *rgb);
int alwan_oklab_to_srgb(alwan_rgb *rgb, alwan_oklab const *oklab);

// Bulk versions with strides
int alwan_srgb_to_xyz_bulk(alwan_scalar *xyz_out, alwan_scalar const *rgb_in,
                           size_t count, size_t in_stride, size_t out_stride);
```

---

## Encoding Spaces

### alwan_rgb_to_hsv / alwan_hsv_to_rgb

```c
int alwan_rgb_to_hsv(alwan_hsv *hsv_out, alwan_rgb const *rgb);
int alwan_hsv_to_rgb(alwan_rgb *rgb_out, alwan_hsv const *hsv);
```

Operates on encoded (display-referred) sRGB values in [0, 1].

**Output:** H: [0, 1] (normalized, multiply by 360 for degrees), S: [0, 1], V: [0, 1]

---

### alwan_rgb_to_hsl / alwan_hsl_to_rgb

```c
int alwan_rgb_to_hsl(alwan_hsl *hsl_out, alwan_rgb const *rgb);
int alwan_hsl_to_rgb(alwan_rgb *rgb_out, alwan_hsl const *hsl);
```

Operates on encoded (display-referred) sRGB values in [0, 1].

**Output:** H: [0, 1] (normalized), S: [0, 1], L: [0, 1]

---

### alwan_linear_srgb_to_hsv / alwan_hsv_to_linear_srgb

```c
int alwan_linear_srgb_to_hsv(alwan_hsv *hsv_out, alwan_rgb const *rgb);
int alwan_hsv_to_linear_srgb(alwan_rgb *rgb_out, alwan_hsv const *hsv);
```

Applies sRGB OETF/EOTF internally so the caller works in linear light.

---

### alwan_linear_srgb_to_hsl / alwan_hsl_to_linear_srgb

```c
int alwan_linear_srgb_to_hsl(alwan_hsl *hsl_out, alwan_rgb const *rgb);
int alwan_hsl_to_linear_srgb(alwan_rgb *rgb_out, alwan_hsl const *hsl);
```

Applies sRGB OETF/EOTF internally so the caller works in linear light.

---

### alwan_rgb_to_hsp / alwan_hsp_to_rgb

```c
int alwan_rgb_to_hsp(alwan_hsp *hsp_out, alwan_rgb const *rgb);
int alwan_hsp_to_rgb(alwan_rgb *rgb_out, alwan_hsp const *hsp);
```

HSP: Hue, Saturation, Perceived brightness. P = sqrt(Pr*R^2 + Pg*G^2 + Pb*B^2) with BT.601 weights (0.299, 0.587, 0.114). H and S are identical to HSV. Used by DaVinci Resolve.

**Reference:** Darel Rex Finley (2006), http://alienryderflex.com/hsp.html

**Output:** H: [0, 1] (normalized), S: [0, 1], P: [0, 1]

---

### alwan_rgb_to_hsplog / alwan_hsplog_to_rgb

```c
int alwan_rgb_to_hsplog(alwan_hsplog *hsplog_out, alwan_rgb const *rgb);
int alwan_hsplog_to_rgb(alwan_rgb *rgb_out, alwan_hsplog const *hsplog);
```

HSPLog: HSP with logarithmic saturation stretching. S_log = log10(1 + 9*S), mapping [0,1] to [0,1]. Expands low saturation values — designed for log/flat-encoded footage. H and P are identical to HSP.

**Inspired by:** Nobe Color Remap (Time in Pixels) / DaVinci Resolve "HSP Log"

**Output:** H: [0, 1] (normalized), S: [0, 1] (log-stretched), P: [0, 1]

**Note:** No published specification exists. The formula used here is a reasonable interpretation of "logarithmic saturation stretching of HSP." The actual DaVinci Resolve implementation is proprietary (Blackmagic Design).

---

### alwan_rgb_to_hsy / alwan_hsy_to_rgb

```c
int alwan_rgb_to_hsy(alwan_hsy *hsy_out, alwan_rgb const *rgb);
int alwan_hsy_to_rgb(alwan_rgb *rgb_out, alwan_hsy const *hsy);
```

HSY: Hue, Saturation, Luma. Y = BT.601 weighted luma. S uses luma-aware max_sat remapping per hue sector. Used by DaVinci Resolve.

**Reference:** Kuzma Shapran "HCY" (chilliant.com); Krita KoColorConversions.cpp

**Output:** H: [0, 1] (normalized), S: [0, 1], Y: [0, 1]

---

### alwan_rgb_to_hwb / alwan_hwb_to_rgb

```c
int alwan_rgb_to_hwb(alwan_scalar *hwb_out, alwan_rgb const *rgb);
int alwan_hwb_to_rgb(alwan_rgb *rgb_out, alwan_scalar const *hwb_in);
```

HWB: Hue, Whiteness, Blackness (CSS Color Level 4). Derived from HSV.

**Output:** H: [0, 1] (normalized), W: [0, 1], B: [0, 1]

---

### alwan_rgb_to_ycbcr / alwan_ycbcr_to_rgb

```c
int alwan_rgb_to_ycbcr(alwan_ycbcr *ycbcr_out, alwan_rgb const *rgb, alwan_ycbcr_standard standard);
int alwan_ycbcr_to_rgb(alwan_rgb *rgb_out, alwan_ycbcr const *ycbcr, alwan_ycbcr_standard standard);
```

**Standards:** `ALWAN_YCBCR_BT601`, `ALWAN_YCBCR_BT709`, `ALWAN_YCBCR_BT2020`

---

### alwan_rgb_to_ycocg / alwan_ycocg_to_rgb

```c
int alwan_rgb_to_ycocg(alwan_ycocg *ycocg_out, alwan_rgb const *rgb);
int alwan_ycocg_to_rgb(alwan_rgb *rgb_out, alwan_ycocg const *ycocg);
```

Reversible integer transform used in H.264/AVC and video codecs.

---

### alwan_rgb_to_cmy / alwan_cmy_to_rgb

```c
int alwan_rgb_to_cmy(alwan_cmy *cmy_out, alwan_rgb const *rgb);
int alwan_cmy_to_rgb(alwan_rgb *rgb_out, alwan_cmy const *cmy);
```

---

## Relative Luminance

Computes Y = kr*R + kg*G + kb*B for a given standard or color space. Input RGB must be linear (scene-referred).

### alwan_relative_luminance

```c
int alwan_relative_luminance(alwan_scalar *Y_out, alwan_rgb const *rgb, alwan_luma_standard standard);
```

**Standards:**
```c
typedef enum {
    ALWAN_LUMA_BT601,       /* ITU-R BT.601 (SD) */
    ALWAN_LUMA_BT709,       /* ITU-R BT.709 / sRGB (HD) */
    ALWAN_LUMA_BT2020,      /* ITU-R BT.2020 (UHD) */
    ALWAN_LUMA_ACES_AP1,    /* ACES AP1 / ACEScg */
    ALWAN_LUMA_ACES_AP0,    /* ACES AP0 / ACES2065-1 */
    ALWAN_LUMA_DISPLAY_P3,  /* Display P3 / P3-D65 */
    ALWAN_LUMA_DCI_P3,      /* DCI-P3 (theater) */
    ALWAN_LUMA_ADOBE_RGB,   /* Adobe RGB (1998) */
    ALWAN_LUMA_PROPHOTO_RGB /* ProPhoto RGB / ROMM RGB */
} alwan_luma_standard;
```

### alwan_relative_luminance_kr_kb

```c
int alwan_relative_luminance_kr_kb(alwan_scalar *Y_out, alwan_rgb const *rgb,
                                   alwan_scalar kr, alwan_scalar kb);
```

Uses explicit kr/kb coefficients (kg = 1 - kr - kb).

### alwan_relative_luminance_space

```c
int alwan_relative_luminance_space(alwan_scalar *Y_out, alwan_rgb const *rgb,
                                   alwan_rgb_space_desc const *space);
```

Extracts the Y row from the RGB-to-XYZ normalized primary matrix, enabling luminance calculation for any of the 100+ supported RGB spaces.

---

## CIE 1964 UVW (Wyszecki)

Uniform color space predating CIE 1976 Lab/Luv. Requires a reference white.

```c
void alwan_xyz_to_uvw(alwan_uvw *uvw, alwan_xyz const *xyz, alwan_xyz const *white_xyz);
void alwan_uvw_to_xyz(alwan_xyz *xyz, alwan_uvw const *uvw, alwan_xyz const *white_xyz);
```

**Reference:** Wyszecki (1963). Superseded by CIE Lab/Luv for most purposes; retained for legacy pipeline compatibility.

---

## DIN99 Color Space

German standard DIN 6176 perceptual color space. Four variants with different rotation/scaling trade-offs.

```c
void alwan_lab_to_din99(alwan_din99 *din99, alwan_lab const *lab, int variant);
void alwan_din99_to_lab(alwan_lab *lab, alwan_din99 const *din99, int variant);
```

**Variants:** `0` (original DIN99), `1` (DIN99b), `2` (DIN99c), `3` (DIN99d — most uniform)

**Input:** CIE Lab values. Convert XYZ → Lab first.

**Use case:** Color difference calculation. `alwan_delta_e_din99` operates directly on `alwan_din99` values.

---

## Hunter Lab

Earlier perceptual space (1958) predating CIE 1976 Lab. Computed directly from XYZ with a different nonlinearity.

```c
void alwan_xyz_to_hunter_lab(alwan_hunter_lab *hunter_lab, alwan_xyz const *xyz);
void alwan_hunter_lab_to_xyz(alwan_xyz *xyz, alwan_hunter_lab const *hunter_lab);

// Custom reference white
void alwan_xyz_to_hunter_lab_custom(alwan_hunter_lab *hunter_lab,
                                    alwan_xyz const *xyz,
                                    alwan_xyz const *xyz_n);
void alwan_hunter_lab_to_xyz_custom(alwan_xyz *xyz,
                                    alwan_hunter_lab const *hunter_lab,
                                    alwan_xyz const *xyz_n);
```

The default form uses D65. The `_custom` form accepts an explicit reference white `xyz_n`.

**Reference:** Hunter (1958), "Photoelectric Color-Difference Meter."

---

## ProLab

Perceptually uniform space designed for smooth interpolation in 3D color pickers.

```c
void alwan_xyz_to_prolab(alwan_prolab *prolab, alwan_xyz const *xyz);
void alwan_prolab_to_xyz(alwan_xyz *xyz, alwan_prolab const *prolab);

// Custom reference white
void alwan_xyz_to_prolab_custom(alwan_prolab *prolab,
                                alwan_xyz const *xyz,
                                alwan_xyz const *xyz_n);
void alwan_prolab_to_xyz_custom(alwan_xyz *xyz,
                                alwan_prolab const *prolab,
                                alwan_xyz const *xyz_n);
```

**Output:** L: [0, 100], a: unconstrained, b: unconstrained

**Reference:** Brill & Süsstrunk (2008), "Renotating the Munsell Book of Color."

---

## OSA-UCS

Optical Society of America Uniform Color Scales. Non-standard but notably uniform for large color differences.

```c
void alwan_xyz_to_osa_ucs(alwan_osa_ucs *osa_ucs, alwan_xyz const *xyz);
void alwan_osa_ucs_to_xyz(alwan_xyz *xyz, alwan_osa_ucs const *osa_ucs);
```

**Output:** L: unconstrained, j: unconstrained, g: unconstrained

**Reference:** MacAdam (1974).

---

## Extended Perceptual Spaces

### hdr-CIELAB (Fairchild 2011)

CIELAB adapted for HDR content using a Michaelis-Menten power function instead of the cube-root nonlinearity. Uses a D65 white point at Y=1 scale (not Y=100).

```c
void alwan_xyz_to_hdr_cielab(alwan_lab *hdr_lab, alwan_xyz const *xyz);
void alwan_hdr_cielab_to_xyz(alwan_xyz *xyz, alwan_lab const *hdr_lab);
```

**Use case:** HDR tone mapping evaluation, HDR color difference.

**Reference:** Fairchild & Wyble (2010), "hdr-CIELAB and hdr-IPT."

---

### hdr-IPT (Fairchild 2011)

IPT adapted for HDR content, same Michaelis-Menten nonlinearity as hdr-CIELAB applied in the LMS domain.

```c
void alwan_xyz_to_hdr_ipt(alwan_ipt *hdr_ipt, alwan_xyz const *xyz);
void alwan_hdr_ipt_to_xyz(alwan_xyz *xyz, alwan_ipt const *hdr_ipt);
```

---

### IgPgTg

Improved IPT variant with better hue constancy for chromatic adaptation applications.

```c
void alwan_xyz_to_igpgtg(alwan_igpgtg *igpgtg, alwan_xyz const *xyz);
void alwan_igpgtg_to_xyz(alwan_xyz *xyz, alwan_igpgtg const *igpgtg);
```

**Reference:** Safdar et al. (2018), "Perceptually Uniform Color Space for Image Signals."

---

### ICaCb

Designed for HDR and wide-gamut content, using a PQ-based nonlinearity in the LMS domain.

```c
void alwan_xyz_to_icacb(alwan_icacb *icacb, alwan_xyz const *xyz);
void alwan_icacb_to_xyz(alwan_xyz *xyz, alwan_icacb const *icacb);
```

---

### Prismatic

Divides each RGB channel by their sum, producing a luminance-independent hue/saturation plane. Useful for color constancy and hue analysis.

```c
void alwan_rgb_to_prismatic(alwan_prismatic *prismatic, alwan_rgb const *rgb);
void alwan_prismatic_to_rgb(alwan_rgb *rgb, alwan_prismatic const *prismatic);
```

**Output:** L: luminance (sum of channels), s: saturation, h: hue angle

---

### HCL (Sarifuddin & Picard)

Perceptual cylindrical space derived from CIE Lab via a non-standard cylindrical mapping for improved hue uniformity.

```c
void alwan_rgb_to_hcl(alwan_hcl *hcl, alwan_rgb const *rgb);
void alwan_hcl_to_rgb(alwan_rgb *rgb, alwan_hcl const *hcl);
```

**Output:** H: [-π, π] radians, C: [0, inf), L: [0, 1]

**Reference:** Sarifuddin & Picard (2005).

---

### IHLS (Hanbury & Serra)

Improved HLS designed for image segmentation and analysis, with a hue angle defined on [0, 2π].

```c
void alwan_rgb_to_ihls(alwan_ihls *ihls, alwan_rgb const *rgb);
void alwan_ihls_to_rgb(alwan_rgb *rgb, alwan_ihls const *ihls);
```

**Output:** H: [0, 2π] radians, L: [0, 1], S: [0, 1]

**Reference:** Hanbury & Serra (2002).

---

### HLC (DIN 5033 / ISO 11664)

A reordering of CIE LCh(ab) placing hue first (H, L, C). Used in colorimetry standards and some European color specification systems.

```c
void alwan_lch_to_hlc(alwan_hlc *hlc, alwan_lch const *lch);
void alwan_hlc_to_lch(alwan_lch *lch, alwan_hlc const *hlc);
```

This is a pure component reordering — no numeric conversion. Convert XYZ → Lab → LCh first.

---

### CubeHelix

A colour scheme / encoding designed so that greyscale conversions are perceptually monotone. The hue cycles through the colour cube as lightness increases.

```c
void alwan_rgb_to_cubehelix(alwan_cubehelix *ch, alwan_rgb const *rgb);
void alwan_cubehelix_to_rgb(alwan_rgb *rgb, alwan_cubehelix const *ch);
```

**Output:** h: hue angle (radians), s: saturation, l: lightness [0, 1]

**Reference:** Green (2011), "A colour scheme for the display of astronomical intensity images."

---

## Perceptually Uniform sRGB Spaces

These spaces are tied specifically to sRGB and perform their chromatic computation in sRGB's gamut boundary.

### HSLuv

Human-friendly HSL with a perceptually uniform saturation axis. H and L follow CIE LCh(uv); S is normalized to the sRGB gamut boundary at that hue/lightness.

```c
void alwan_srgb_to_hsluv(alwan_hsluv *hsluv, alwan_rgb const *srgb);
void alwan_hsluv_to_srgb(alwan_rgb *srgb, alwan_hsluv const *hsluv);
```

**Output:** h: [0, 360] degrees, s: [0, 100], l: [0, 100]

**Reference:** [hsluv.org](https://www.hsluv.org)

---

### HPLuv

Like HSLuv but limited to hues that have a full pastel range at all lightness levels. More restricted gamut than HSLuv; saturation 100 is always achievable.

```c
void alwan_srgb_to_hpluv(alwan_hpluv *hpluv, alwan_rgb const *srgb);
void alwan_hpluv_to_srgb(alwan_rgb *srgb, alwan_hpluv const *hpluv);
```

**Output:** h: [0, 360] degrees, s: [0, 100], l: [0, 100]

---

### OkHSL

HSL-like encoding derived from the Oklab gamut boundary for sRGB. Designed for color pickers where hue and lightness changes look smooth.

```c
void alwan_srgb_to_okhsl(alwan_okhsl *okhsl, alwan_rgb const *srgb);
void alwan_okhsl_to_srgb(alwan_rgb *srgb, alwan_okhsl const *okhsl);
```

**Output:** h: [0, 1] (normalized hue), s: [0, 1], l: [0, 1]

**Reference:** Ottosson (2021), "A perceptual color space for image processing."

---

### OkHSV

HSV-like encoding derived from the Oklab gamut boundary for sRGB. Designed for color pickers — the maximum chroma corner is always reachable at s=1, v=1.

```c
void alwan_srgb_to_okhsv(alwan_okhsv *okhsv, alwan_rgb const *srgb);
void alwan_okhsv_to_srgb(alwan_rgb *srgb, alwan_okhsv const *okhsv);
```

**Output:** h: [0, 1] (normalized hue), s: [0, 1], v: [0, 1]

---

## YcCbcCrc (BT.2020 Constant Luminance)

Constant luminance variant of YCbCr defined in ITU-R BT.2020 for UHD video. Unlike regular YCbCr, luma is computed from linear RGB before quantization.

```c
int alwan_rgb_to_yccbccrc(alwan_yccbccrc *yccbccrc_out, alwan_rgb const *rgb, int bit_depth);
int alwan_yccbccrc_to_rgb(alwan_rgb *rgb_out, alwan_yccbccrc const *yccbccrc, int bit_depth);
```

**Parameters:**
- `rgb` — linear BT.2020 RGB (not encoded)
- `bit_depth` — `10` or `12`

**Reference:** ITU-R BT.2020-2, Section 4.

---

## Bulk Operations

Always prefer bulk operations over single-element loops:

```c
alwan_xyz d65 = {0.95047, 1.0, 1.08883};

// Slow: single-element loop
for (int i = 0; i < 1000; i++) {
    alwan_xyz_to_lab(&lab[i], &xyz[i], &d65);
}

// Fast: bulk with strides (in bytes)
alwan_xyz_to_lab_bulk((alwan_scalar*)lab, (alwan_scalar*)xyz, &d65,
                      1000, sizeof(alwan_xyz), sizeof(alwan_lab));
```

---

## Error Codes

- `ALWAN_OK` (0) — Success
- `ALWAN_E_INVALID` (-1) — Invalid parameter
- `ALWAN_E_NODATA` (-2) — Data not found
- `ALWAN_E_RANGE` (-3) — Value out of range
- `ALWAN_E_NOMEM` (-4) — Allocation failed
- `ALWAN_E_DIVZERO` (-5) — Division by zero

---

## See Also

- [Chromatic Adaptation](chromatic-adaptation.md) — White point transforms
- [Transfer Functions](transfer-functions.md) — Encoding/decoding
- [Color Difference](color-difference.md) — ΔE metrics that operate on these spaces
- [GPU Backends](backends.md) — Using color space conversions in HLSL/GLSL/Halide
- [Examples](../examples.md) — Usage examples
