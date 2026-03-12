# Alwan API Comparison

Comparison of **Alwan** (C colour science library) against three reference projects:
- **colour-science** (Python) -- comprehensive colour science research library
- **OpenColorIO** (C++) -- VFX/film color management pipeline framework
- **aces-dev** (CTL) -- Academy Color Encoding System reference transforms

> Legend: Y = supported, -- = not supported, P = partial

---

## 1. Color Space Conversions

### 1.1 CIE / Perceptual Spaces

| Color Space | Alwan | colour-science | OpenColorIO | aces-dev |
|-------------|:-----:|:--------------:|:-----------:|:--------:|
| CIE XYZ | Y | Y | Y | Y |
| CIE xyY | Y | Y | Y (FixedFunc) | -- |
| CIE Lab | Y | Y | -- | -- |
| CIE Luv | Y | Y | Y (FixedFunc) | -- |
| CIE LCh(ab) | Y | Y | -- | -- |
| CIE LCh(uv) | Y | Y | -- | -- |
| CIE 1960 UCS (uv) | Y | Y | Y (FixedFunc) | -- |
| CIE U\*V\*W\* (1964) | Y | Y | -- | -- |
| Oklab | Y | Y | -- | -- |
| Oklch | Y | Y | -- | -- |
| DIN99 (variants 0-3) | Y | Y | -- | -- |
| ICtCp (PQ + HLG) | Y | Y | -- | -- |
| JzAzBz | Y | Y | -- | -- |
| JzCzhz | Y | Y | -- | -- |
| Hunter Lab | Y | Y | -- | -- |
| IPT | Y | Y | -- | -- |
| IPTch (cylindrical) | Y | P (no cyl.) | -- | -- |
| ProLab | Y | -- | -- | -- |
| OSA-UCS | Y | Y | -- | -- |
| hdr-CIELAB (Fairchild) | Y | Y | -- | -- |
| hdr-IPT (Fairchild) | Y | Y | -- | -- |
| IgPgTg | Y | Y | -- | -- |
| ICaCb | Y | Y | -- | -- |
| Prismatic (Pridmore) | Y | Y | -- | -- |
| HCL (Sarifuddin) | Y | Y | -- | -- |
| IHLS (Hanbury) | Y | Y | -- | -- |
| CAM16-UCS (Jab) | Y | Y | -- | -- |

### 1.2 Convenience / Device Models

| Color Space | Alwan | colour-science | OpenColorIO | aces-dev |
|-------------|:-----:|:--------------:|:-----------:|:--------:|
| HSV | Y | Y | Y (FixedFunc) | -- |
| HSL | Y | Y | -- | -- |
| CMY | Y | Y | -- | -- |
| CMYK | Y | Y | -- | -- |
| YCbCr (BT.601/709/2020) | Y | Y | -- | -- |
| YcCbcCrc (const. luma) | Y | -- | -- | -- |
| YCoCg | Y | -- | -- | -- |
| HWB | Y | Y | -- | -- |
| HSP (perceived brightness) | Y | -- | -- | -- |
| HSPLog (log saturation HSP) | Y | -- | -- | -- |
| HSY (luma-weighted) | Y | -- | -- | -- |
| Relative Luminance (multi-standard) | Y | Y | -- | -- |

### 1.3 RGB Working Spaces

| RGB Space | Alwan | colour-science | OpenColorIO | aces-dev |
|-----------|:-----:|:--------------:|:-----------:|:--------:|
| sRGB / Linear sRGB | Y | Y | Y | -- |
| BT.709 / Linear | Y | Y | Y | -- |
| Display P3 / Linear | Y | Y | Y | -- |
| BT.2020 / Linear | Y | Y | Y | Y |
| Adobe RGB 1998 / Linear | Y | Y | P (via config) | -- |
| ProPhoto RGB / Linear | Y | Y | P (via config) | -- |
| Adobe Wide Gamut RGB | Y | Y | -- | -- |
| DCI-P3 / P3-D65 / P3-D60 | Y | Y | Y | Y |
| DCI-P3+ | Y | -- | -- | -- |
| ROMM / RIMM / ERIMM RGB | Y | Y | -- | -- |
| CIE RGB (1931) | Y | Y | -- | -- |
| ColorMatch RGB | Y | Y | -- | -- |
| Apple RGB | Y | Y | -- | -- |
| Best / Beta / DON RGB 4 | Y | Y | -- | -- |
| Ekta Space PS5 | Y | Y | -- | -- |
| Max RGB / Russell RGB | Y | -- | -- | -- |
| Sharp RGB | Y | Y | -- | -- |
| ECI RGB v2 | Y | Y | -- | -- |
| Xtreme RGB | Y | -- | -- | -- |
| Rec.1886 Rec.709 | Y | -- | Y (via config) | -- |
| Rec.2100 PQ / HLG | Y | Y | Y | -- |
| Display P3 HDR (PQ) | Y | -- | Y (via config) | -- |
| DCDM XYZ | Y | Y | Y | Y |
| NTSC 1953 / 1987 | Y | Y | -- | -- |
| PAL/SECAM | Y | Y | -- | -- |
| SMPTE 240M / C | Y | Y | -- | -- |
| BT.470-525 / 625 | Y | Y | -- | -- |
| EBU Tech. 3213-E | Y | -- | -- | -- |
| Gamma-encoded variants (2.2/1.8) | Y | Y | Y (ExponentTransform) | -- |
| **Total ~90+ RGB spaces** | **Y** | **~80+** | **Config-dependent** | **~6** |

### 1.4 Camera / Cinema Spaces

| Space | Alwan | colour-science | OpenColorIO | aces-dev |
|-------|:-----:|:--------------:|:-----------:|:--------:|
| ACES2065-1 (AP0) | Y | Y | Y | Y |
| ACEScg (AP1) | Y | Y | Y | Y |
| ACEScc | Y | Y | Y | Y |
| ACEScct | Y | Y | Y | Y |
| ACESproxy | Y | Y | Y | Y |
| ARRI Wide Gamut 3 / 4 | Y | Y | Y | P (IDTs) |
| ARRI LogC3 / LogC4 | Y | Y | Y | P (IDTs) |
| RED Wide Gamut RGB | Y | Y | Y | P (IDTs) |
| REDColor 1-4, DragonColor 1-2 | Y | -- | P (via config) | -- |
| REDLog / REDLogFilm / Log3G10 | Y | Y | Y | P (IDTs) |
| Sony S-Gamut / S-Gamut3 / Cine | Y | Y | Y | P (IDTs) |
| Sony Venice S-Gamut3 / Cine | Y | -- | Y | -- |
| S-Log / S-Log2 / S-Log3 | Y | Y | Y | P (IDTs) |
| Canon Cinema Gamut | Y | Y | Y | P (IDTs) |
| Canon Log / C-Log2 / C-Log3 | Y | Y | Y | P (IDTs) |
| Panasonic V-Gamut / V-Log | Y | Y | Y | P (IDTs) |
| DaVinci Wide Gamut | Y | Y | Y | -- |
| DaVinci Intermediate | Y | -- | Y | -- |
| Blackmagic Wide Gamut | Y | -- | Y | -- |
| Blackmagic Film (Gen 1-5) | Y | -- | Y | -- |
| FilmLight E-Gamut / T-Log | Y | Y | Y | -- |
| Fujifilm F-Gamut / F-Log | Y | -- | Y | -- |
| Nikon N-Gamut / N-Log | Y | -- | Y | -- |
| DJI D-Gamut | Y | -- | Y | -- |
| GoPro Protune Native | Y | -- | Y | -- |
| ALEXA Wide Gamut (legacy) | Y | Y | Y | P (IDTs) |

---

## 2. Transfer Functions

| Transfer Function | Alwan | colour-science | OpenColorIO | aces-dev |
|-------------------|:-----:|:--------------:|:-----------:|:--------:|
| sRGB OETF / EOTF | Y | Y | Y | -- |
| BT.709 | Y | Y | Y | -- |
| BT.2020 | Y | Y | Y | -- |
| PQ (ST 2084) | Y | Y | Y | Y |
| HLG (BT.2100) | Y | Y | Y | -- |
| BT.1886 | Y | Y | Y | Y |
| ACESproxy / ACEScc / ACEScct | Y | Y | Y | Y |
| S-Log / S-Log2 / S-Log3 | Y | Y | Y | P |
| Canon C-Log / C-Log2 / C-Log3 | Y | Y | Y | P |
| V-Log | Y | Y | Y | P |
| ARRI LogC3 / LogC4 | Y | Y | Y | P |
| REDLog / REDLogFilm / Log3G10 | Y | Y | Y | P |
| Blackmagic Film Gen 4 / 5 | Y | -- | Y | -- |
| FilmLight T-Log / E-Log | Y | -- | Y | -- |
| GoPro Protune | Y | -- | Y | -- |
| Nikon N-Log | Y | -- | Y | -- |
| Cineon / DPX | Y | Y | Y | -- |
| Apple Log | Y | -- | -- | -- |
| Fujifilm F-Log / F-Log2 | Y | -- | Y | -- |
| Leica L-Log | Y | -- | -- | -- |
| DJI D-Log | Y | -- | Y | -- |
| DCDM (ST 428-1) | Y | Y | Y | Y |
| ADX10 / ADX16 | Y | Y | Y | Y |
| Gamma 2.2 / 2.4 / 2.6 / 2.8 | Y | Y | Y | Y |
| **Arbitrary gamma** | -- | Y | Y | -- |

---

## 3. Chromatic Adaptation Transforms (CAT)

| CAT Method | Alwan | colour-science | OpenColorIO | aces-dev |
|------------|:-----:|:--------------:|:-----------:|:--------:|
| XYZ Scaling (Von Kries) | Y | Y | -- | -- |
| Bradford | Y | Y | P (baked in matrices) | Y (baked) |
| CAT02 | Y | Y | -- | -- |
| CAT16 | Y | Y | -- | -- |
| Sharp | Y | Y | -- | -- |
| Fairchild (1990) | Y | Y | -- | -- |
| CMCCAT97 | Y | Y | -- | -- |
| CMCCAT2000 | Y | Y | -- | -- |
| CAT02 Brill 2008 | Y | Y | -- | -- |
| Bianco 2010 | Y | Y | -- | -- |
| Bianco PC 2010 | Y | Y | -- | -- |
| Zhai & Luo 2018 (two-step) | Y | Y | -- | -- |
| **Arbitrary white point pairs** | **Y** | **Y** | **--** | **--** |
| **Bulk CAT application** | **Y** | **Y** (numpy) | **--** | **--** |

---

## 4. Color Difference (Delta E) Metrics

| Metric | Alwan | colour-science | OpenColorIO | aces-dev |
|--------|:-----:|:--------------:|:-----------:|:--------:|
| DE*76 (CIE 1976) | Y | Y | -- | -- |
| DE*94 (CIE 1994) | Y | Y | -- | -- |
| DE CMC(l:c) | Y | Y | -- | -- |
| CIEDE2000 | Y | Y | -- | -- |
| DE ITP (BT.2124) | Y | Y | -- | -- |
| DE HyAB | Y | Y | -- | -- |
| DE DIN99 | Y | Y | -- | -- |
| DE CAM02-LCD | Y | Y | -- | -- |
| DE CAM02-SCD | Y | Y | -- | -- |
| DE CAM02-UCS | Y | Y | -- | -- |
| DE CAM16-LCD | Y | Y | -- | -- |
| DE CAM16-SCD | Y | Y | -- | -- |
| DE CAM16-UCS | Y | Y | -- | -- |
| DE ZCAM | Y | -- | -- | -- |
| **Batch / bulk DE** | **Y** | **Y** (numpy) | **--** | **--** |

---

## 5. Color Appearance Models (CAM)

| Model | Alwan | colour-science | OpenColorIO | aces-dev |
|-------|:-----:|:--------------:|:-----------:|:--------:|
| CIECAM02 (forward) | Y | Y | -- | -- |
| CIECAM02 (inverse) | Y | Y | -- | -- |
| CAM16 (forward) | Y | Y | -- | -- |
| CAM16 (inverse) | Y | Y | -- | -- |
| CAM16-UCS | Y | Y | -- | -- |
| ZCAM (forward) | Y | Y | -- | -- |
| ZCAM (inverse) | Y | Y | -- | -- |
| ZCAM-UCS | Y | -- | -- | -- |
| RLAB (forward + inverse) | Y | -- | -- | -- |
| Hunt (forward) | Y | Y | -- | -- |
| Hellwig2022 (forward + inverse) | Y | Y | -- | -- |
| Kim2009 (forward + inverse) | Y | Y | -- | -- |
| LLAB (forward) | Y | Y | -- | -- |
| ATD95 (forward) | Y | Y | -- | -- |
| Nayatani95 (forward) | Y | Y | -- | -- |
| **Bulk CAM processing** | **Y** | **Y** (numpy) | **--** | **--** |

---

## 6. Illuminants & Observers

### 6.1 Illuminants

| Illuminant | Alwan | colour-science | OpenColorIO | aces-dev |
|------------|:-----:|:--------------:|:-----------:|:--------:|
| A | Y | Y | -- | -- |
| B | Y | Y | -- | -- |
| C | Y | Y | -- | -- |
| D40, D45, D93 | Y | -- | -- | -- |
| D50, D55, D60, D65, D75 | Y | Y | -- | P (D60) |
| E (equal energy) | Y | Y | -- | -- |
| F1-F12 (fluorescent) | Y | Y | -- | -- |
| LED (B1-B5, BH1, RGB1, V1, V2) | Y | Y | -- | -- |
| HP1-HP5 (high pressure) | Y | Y | -- | -- |
| **SPD data for illuminants** | **Y** | **Y** | **--** | **--** |
| **Custom D-series from CCT** | -- | Y | -- | -- |

### 6.2 Standard Observers

| Observer | Alwan | colour-science | OpenColorIO | aces-dev |
|----------|:-----:|:--------------:|:-----------:|:--------:|
| CIE 1931 2-degree | Y | Y | -- | -- |
| CIE 1964 10-degree | Y | Y | -- | -- |
| CIE 2012 2-degree | Y | Y | -- | -- |
| CIE 2012 10-degree | Y | Y | -- | -- |
| Stockman & Sharpe 2000 | Y | Y | -- | -- |
| CIE 2015 2-degree | Y | Y | -- | -- |
| CIE 2015 10-degree | Y | Y | -- | -- |
| Wright & Guild 1931 | Y | Y | -- | -- |

---

## 7. Spectral Processing

| Feature | Alwan | colour-science | OpenColorIO | aces-dev |
|---------|:-----:|:--------------:|:-----------:|:--------:|
| SPD creation / manipulation | Y | Y | -- | -- |
| SPD to XYZ integration | Y | Y | -- | -- |
| Integration (trapezoid) | Y | Y | -- | -- |
| Integration (Simpson) | Y | Y | -- | -- |
| Bandpass correction (Stearns) | Y | Y | -- | -- |
| Standard illuminant SPD loading | Y | Y | -- | -- |
| Blackbody (Planck) SPD | Y | Y | -- | -- |
| SPD resampling (linear) | Y | Y | -- | -- |
| SPD resampling (Catmull-Rom) | Y | Y | -- | -- |
| SPD shape analysis | Y | Y | -- | -- |
| Camera spectral sensitivities | Y | Y | -- | -- |
| Spectral upsampling (Smits 1999) | Y | Y | -- | -- |
| Spectral upsampling (Mallett 2019) | Y | Y | -- | -- |
| Spectral upsampling (Jakob 2019) | Y | Y | -- | -- |
| Spectral locus (xy from wavelength) | Y | Y | -- | -- |
| Dominant wavelength | Y | Y | -- | -- |
| Excitation purity | Y | Y | -- | -- |
| Complementary wavelength | Y | Y | -- | -- |
| Multi-spectral imaging | -- | Y | -- | -- |
| Spectral optimization | Y | Y | -- | -- |

---

## 8. CCT & Light Quality Metrics

| Feature | Alwan | colour-science | OpenColorIO | aces-dev |
|---------|:-----:|:--------------:|:-----------:|:--------:|
| CCT - McCamy approximation | Y | Y | -- | -- |
| CCT - Robertson method | Y | Y | -- | -- |
| CCT - Hernandez-Andres 1999 | Y | Y | -- | -- |
| CCT - Kang 2002 (fwd + inv) | Y | Y | -- | -- |
| CCT + Duv optimization | Y | Y | -- | -- |
| CCT to xy (forward) | Y | Y | -- | -- |
| CRI (Ra) | Y | Y | -- | -- |
| CQS (Color Quality Scale) | Y | Y | -- | -- |
| TM-30 Rf | Y | Y | -- | -- |
| CIE 224:2017 Rf | Y | Y | -- | -- |
| SSI (Spectral Similarity Index) | Y | -- | -- | -- |
| Metamerism Index | Y | Y | -- | -- |
| TM-30 Rg (gamut area) | -- | Y | -- | -- |
| TM-30 color vector graphics | -- | Y | -- | -- |

---

## 9. Gamut Mapping & Analysis

| Feature | Alwan | colour-science | OpenColorIO | aces-dev |
|---------|:-----:|:--------------:|:-----------:|:--------:|
| Simple RGB clipping | Y | Y | Y (RangeTransform) | -- |
| Hue-preserving projection | Y | Y | -- | -- |
| Adaptive L0 | Y | -- | -- | -- |
| Adaptive Cusp | Y | -- | -- | -- |
| Chroma Compress | Y | -- | -- | -- |
| SGCK 2004 | Y | -- | -- | -- |
| HPMINDE | Y | -- | -- | -- |
| Lightness Preserving | Y | -- | -- | -- |
| Gamut volume (Monte Carlo) | Y | Y | -- | -- |
| Gamut volume ratio | Y | Y | -- | -- |
| Gamut coverage (MC) | Y | Y | -- | -- |
| Pointer's Gamut boundary | Y | Y | -- | -- |
| Pointer's Gamut check | Y | Y | -- | -- |
| XYZ-to-RGB perceptual map | Y | -- | -- | -- |
| ACES GamutComp 1.3 | Y | -- | Y (FixedFunc) | Y |
| ACES GamutCompress 2.0 | Y | -- | Y (FixedFunc) | Y |
| ICC rendering intents | -- | -- | -- | -- |

---

## 10. ACES Pipeline

| Feature | Alwan | colour-science | OpenColorIO | aces-dev |
|---------|:-----:|:--------------:|:-----------:|:--------:|
| **ACES 1.x** | | | | |
| RedMod03 / RedMod10 (+ inv) | Y | -- | Y | Y |
| Glow03 / Glow10 (+ inv) | Y | -- | Y | Y |
| DarkToDim10 | Y | -- | Y | Y |
| GamutComp 1.3 (+ inv) | Y | -- | Y | Y |
| Blue Light Artifact Fix (+ inv) | Y | -- | Y | Y |
| ACES Look 1.0 LMT (+ inv) | Y | -- | -- | Y |
| Parametric LMT (CDL-style) | Y | -- | Y (CDLTransform) | Y |
| Output Transforms (12 presets) | Y | -- | Y (BuiltinTransform) | Y |
| Inverse Output Transforms | Y | -- | Y | Y |
| Rec.2100 Surround | Y | -- | Y (FixedFunc) | Y |
| **ACES 2.0** | | | | |
| TonescaleCompress20 | Y | -- | Y (FixedFunc) | Y |
| RGB to JMh20 | Y | -- | Y (FixedFunc) | Y |
| GamutCompress20 (+ inv) | Y | -- | Y (FixedFunc) | Y |
| Output Transform (12 presets) | Y | -- | Y (FixedFunc) | Y |
| Inverse Output Transform | Y | -- | Y | Y |
| Custom Output Transform | Y | -- | P (via config) | -- |

---

## 11. Color Vision & Perception

| Feature | Alwan | colour-science | OpenColorIO | aces-dev |
|---------|:-----:|:--------------:|:-----------:|:--------:|
| CVD simulation (Brettel) | Y | Y | -- | -- |
| Protanopia / Deuteranopia / Tritanopia | Y | Y | -- | -- |
| Anomalous trichromacy (severity) | Y | Y | -- | -- |
| Luminous efficiency (V(l)) | Y | Y | -- | -- |
| Photopic luminance from SPD | Y | Y | -- | -- |
| Scotopic luminance from SPD | Y | Y | -- | -- |
| Mesopic luminance (CIE 191:2010) | Y | -- | -- | -- |
| CSF (simplified Barten) | Y | -- | -- | -- |
| CSF (full Barten 1999) | Y | Y | -- | -- |
| Pupil diameter (Barten) | Y | -- | -- | -- |
| Retinal illuminance | Y | -- | -- | -- |
| Optical MTF | Y | -- | -- | -- |
| Helmholtz-Kohlrausch effect | P (via Hellwig2022) | Y | -- | -- |
| Chromatic adaptation models | Y (12 methods) | Y (12+ methods) | -- | -- |

---

## 12. Color Grading & Correction

| Feature | Alwan | colour-science | OpenColorIO | aces-dev |
|---------|:-----:|:--------------:|:-----------:|:--------:|
| Lift / Gamma / Gain | Y | -- | Y (GradingPrimary) | -- |
| Color matrix presets (sepia, etc.) | Y | -- | -- | -- |
| CDL (Slope/Offset/Power/Sat) | Y | -- | Y (CDLTransform) | Y |
| Printer lights | Y | -- | -- | Y |
| RGB curves | -- | -- | Y (GradingRGBCurve) | -- |
| Zone-based tone (shadows/hilights) | -- | -- | Y (GradingTone) | -- |
| Exposure / Contrast (dynamic GPU) | -- | -- | Y | -- |
| White balance from gray | Y | -- | -- | -- |

---

## 13. Camera Profiling & Correction

| Feature | Alwan | colour-science | OpenColorIO | aces-dev |
|---------|:-----:|:--------------:|:-----------:|:--------:|
| Cheung 2004 polynomial (3-35 terms) | Y | Y | -- | -- |
| Finlayson 2015 root-polynomial | Y | Y | -- | -- |
| Vandermonde expansion | Y | Y | -- | -- |
| Color correction matrix computation | Y | Y | -- | -- |
| White balance multipliers | Y | -- | -- | -- |

---

## 14. Reference Data & Color Order Systems

| Feature | Alwan | colour-science | OpenColorIO | aces-dev |
|---------|:-----:|:--------------:|:-----------:|:--------:|
| Munsell Renotation (XYZ <-> Munsell) | Y | Y | -- | -- |
| ColorChecker Classic (24) | Y | Y | -- | -- |
| ColorChecker SG (140) | Y | Y | -- | -- |
| ColorChecker Digital SG | Y | -- | -- | -- |
| BabelColor Average / HCT | Y | -- | -- | -- |
| NCS (Natural Color System) | Y | -- | -- | -- |
| Pantone | -- | -- | -- | -- |
| RAL | -- | Y | -- | -- |

---

## 15. Whiteness / Yellowness Indices

| Feature | Alwan | colour-science | OpenColorIO | aces-dev |
|---------|:-----:|:--------------:|:-----------:|:--------:|
| ASTM E313 Yellowness Index | Y | Y | -- | -- |
| ASTM E313 Whiteness Index | Y | Y | -- | -- |
| CIE 2004 Whiteness (W, Tw) | Y | Y | -- | -- |

---

## 16. Optical Phenomena

| Feature | Alwan | colour-science | OpenColorIO | aces-dev |
|---------|:-----:|:--------------:|:-----------:|:--------:|
| Rayleigh scattering cross section | Y | Y | -- | -- |
| Rayleigh optical depth | Y | Y | -- | -- |
| Rayleigh scattering SPD | Y | Y | -- | -- |

---

## 17. Interpolation & Math Utilities

| Feature | Alwan | colour-science | OpenColorIO | aces-dev |
|---------|:-----:|:--------------:|:-----------:|:--------:|
| 3x3 matrix multiply / inverse / det | Y | Y (numpy) | Y (MatrixTransform) | -- |
| Bulk matrix-vector transform | Y | Y (numpy) | Y (CPUProcessor) | -- |
| Linear interpolation | Y | Y | Y | -- |
| Cubic interpolation | Y | Y | -- | -- |
| Lanczos interpolation | Y | -- | -- | -- |
| Sprague 5th order | Y | Y | -- | -- |
| Lagrange polynomial | Y | Y | -- | -- |
| Akima spline | Y | -- | -- | -- |
| 1D LUT interpolation | Y | Y | Y (Lut1D) | -- |
| 3D LUT trilinear | Y | -- | Y (Lut3D) | -- |
| 3D LUT tetrahedral | Y | -- | Y (Lut3D) | -- |
| Int-to-float normalization (8/10/12/16-bit) | Y | Y | Y | -- |
| Extrapolation (const/linear/poly/exp) | Y | Y | -- | -- |

---

## 18. Architecture & Pipeline Features

| Feature | Alwan | colour-science | OpenColorIO | aces-dev |
|---------|:-----:|:--------------:|:-----------:|:--------:|
| **Language** | **C (pure)** | **Python** | **C++** | **CTL** |
| Config-driven pipeline | -- | -- | Y | -- |
| GPU shader generation (GLSL/HLSL/Metal) | -- | -- | Y | -- |
| SIMD-optimized CPU processor | -- | -- | Y (SSE2) | -- |
| Display/View management | -- | -- | Y | -- |
| LUT file I/O (.cube, .clf, .ctf, .csp, ...) | -- | -- | Y (15+ formats) | -- |
| ICC profile support | -- | -- | P (matrix/TRC) | -- |
| CDL file I/O (.cdl, .ccc, .cc) | -- | -- | Y | -- |
| Dynamic GPU parameters | -- | -- | Y | -- |
| OCIOZ config archives | -- | -- | Y | -- |
| Custom allocator support | Y | -- | -- | -- |
| Bulk/stride processing | Y | Y (numpy) | Y (scanline) | -- |
| Cross-platform (C/HLSL/Halide) | Y (planned) | -- | -- | -- |
| View transforms (AgX, ACES) | Y | -- | Y (via config) | Y |
| Opaque context handle | Y | -- | Y (Config) | -- |
| Zero-dependency (no stdlib) | Y (optional) | -- | -- | -- |
| Embeddable (no file I/O mode) | Y | -- | -- | -- |

---

## Summary: What Alwan Has That Others Don't

Each cell shows whether the **other** library lacks this feature (Y = they also have it, **--** = they lack it).

| Alwan Feature | colour-science | OpenColorIO | aces-dev |
|---------------|:--------------:|:-----------:|:--------:|
| Pure C, zero dependencies | -- | -- | -- |
| Embeddable (no file I/O mode) | -- | -- | -- |
| Cross-platform C/HLSL/Halide target | -- | P (GPU shaders) | -- |
| ProLab color space | -- | -- | -- |
| ZCAM-UCS + DE ZCAM | -- | -- | -- |
| SSI (Spectral Similarity Index) | -- | -- | -- |
| Mesopic luminance (CIE 191:2010) | -- | -- | -- |
| Pupil diameter / Retinal illuminance | -- | -- | -- |
| Optical MTF (full Barten 1999) | -- | -- | -- |
| 8 gamut mapping algorithms | P (~2) | P (~2) | P (~2) |
| NCS color system | -- | -- | -- |
| BabelColor checker data | -- | -- | -- |
| Apple Log / Leica L-Log TFs | -- | -- | -- |
| DCI-P3+ space | -- | -- | -- |
| YcCbcCrc (const. luminance BT.2020) | -- | -- | -- |
| YCoCg | Y | -- | -- |
| Combined ACES 1.x + 2.0 as C API | -- | P (similar) | Y (CTL ref) |

---

## Summary: What Others Have That Alwan Doesn't

### From colour-science (Python)

| Missing Feature | Notes |
|-----------------|-------|
| HWB color space | Simple to add |
| Custom D-series illuminant from CCT | Generate SPD for arbitrary Kelvin |
| CIECAM97s (predecessor to CIECAM02) | Historical CAM |
| TM-30 Rg (gamut area index) | Only Rf is implemented |
| TM-30 color vector graphics | Visualization tooling |
| TLCI (Television Lighting Consistency Index) | Broadcast-specific quality metric |
| CSS Color Level 4 gamut mapping (OKLCh binary search) | Web-standard gamut mapping |
| Ohno 2013 CCT method | Modern CCT computation |
| Krystek 1985 CCT method | Additional CCT algorithm |
| Spectral upsampling (Meng 2015) | Additional spectral recovery method |
| Spectral upsampling (Otsu 2018) | Cluster-based spectral reconstruction |
| DICOM GSDF transfer function | Medical imaging |
| Weber / Michelson / Whittle contrast | Basic contrast metrics |
| RAL color system | Niche |
| Multi-spectral imaging | Research use case |
| Arbitrary gamma transfer functions | Only fixed gammas (2.2, 2.4, 2.6, 2.8) |
| Plotting / visualization | Not applicable (Python-specific) |
| Automatic color space graph traversal | Not applicable (library vs framework) |

### From OpenColorIO (C++)

| Missing Feature | Notes |
|-----------------|-------|
| Config-driven color pipeline | Architectural difference (library vs framework) |
| GPU shader code generation | Planned via HLSL cross-platform support |
| SIMD-optimized CPU path (SSE2) | Potential future optimization |
| LUT file I/O (.cube, .clf, .ctf, etc.) | Not a goal (pure color science library) |
| ICC profile reading | Not a goal |
| CDL file I/O | CDL math is supported (alwan_aces_lmt_apply) |
| Display/View management system | Not a goal (application-level concern) |
| GradingRGBCurve / GradingTone | Specialized grading tools |
| Dynamic GPU parameter updates | Planned via HLSL backend |
| OCIOZ config archives | Not applicable |
| Look management | Not applicable (framework feature) |

### From aces-dev (CTL)

| Missing Feature | Notes |
|-----------------|-------|
| Full IDT library (all cameras) | Alwan has camera spaces; IDTs are config-level |
| CTL scripting language | Not applicable |
| AMF (ACES Metadata File) | Not a goal |
| ACES system-level orchestration | Alwan provides ACES components, not the framework |
