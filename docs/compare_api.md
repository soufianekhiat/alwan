# Alwan API Comparison

Comparison of **Alwan** against three commonly adjacent projects:

- **colour-science** - Python research and validation library
- **OpenColorIO** - production colour-management and interchange framework
- **aces-dev** - ACES reference transforms and standards material

> Legend:
> `Y` = broadly supported
> `P` = partial / indirect / config-dependent
> `--` = not a primary surface area
>
> This is a coarse capability matrix, not a line-by-line audit of external
> projects. The **Alwan** column is based on the current `src/alwan/alwan.h`
> in this repository.

---

## 1. Core Colour-Space Coverage

### 1.1 CIE / perceptual spaces

| Space family | Alwan | colour-science | OpenColorIO | aces-dev |
|--------------|:-----:|:--------------:|:-----------:|:--------:|
| XYZ / xyY | Y | Y | P | -- |
| Lab / Luv | Y | Y | P | -- |
| LCh(ab) / LCh(uv) | Y | Y | -- | -- |
| Oklab / Oklch | Y | Y | -- | -- |
| JzAzBz / JzCzhz | Y | Y | -- | -- |
| ICtCp | Y | Y | -- | -- |
| IPT / IPTch | Y | Y | -- | -- |
| Hunter Lab / ProLab / OSA-UCS / UCS / UVW | Y | P | -- | -- |
| hdr-CIELAB / hdr-IPT / IgPgTg / ICaCb | Y | P | -- | -- |

### 1.2 Convenience / device-oriented spaces

| Space family | Alwan | colour-science | OpenColorIO | aces-dev |
|--------------|:-----:|:--------------:|:-----------:|:--------:|
| HSV / HSL / HWB | Y | Y | P | -- |
| CMY / CMYK | Y | Y | -- | -- |
| YCbCr / YCoCg / constant-luma variants | Y | Y | -- | -- |
| HSP / HSPLog / HSY / IHLS / HCL / Prismatic | Y | P | -- | -- |

---

## 2. RGB Spaces And Transfer Functions

### 2.1 RGB working / display spaces

| Area | Alwan | colour-science | OpenColorIO | aces-dev |
|------|:-----:|:--------------:|:-----------:|:--------:|
| common display RGB (sRGB / BT.709 / Display P3 / BT.2020) | Y | Y | Y | P |
| ACES family RGB spaces | Y | Y | Y | Y |
| camera / cinema RGB spaces | Y | Y | Y | P |
| legacy / print / wide-gamut RGB sets | Y | Y | P | -- |
| linear and gamma-encoded RGB variants in one public enum | Y | P | P | -- |

### 2.2 Transfer functions

| Area | Alwan | colour-science | OpenColorIO | aces-dev |
|------|:-----:|:--------------:|:-----------:|:--------:|
| SDR transfer functions (sRGB / BT.709 / BT.2020 / gamma) | Y | Y | Y | P |
| HDR transfer functions (PQ / HLG / BT.1886) | Y | Y | Y | P |
| ACES transfer functions | Y | Y | Y | Y |
| major camera log curves | Y | Y | Y | P |
| direct OETF / EOTF apply functions in public C API | Y | P | P | -- |

---

## 3. Colour Science Models

### 3.1 Chromatic adaptation and appearance

| Area | Alwan | colour-science | OpenColorIO | aces-dev |
|------|:-----:|:--------------:|:-----------:|:--------:|
| multiple CAT methods | Y | Y | P | P |
| arbitrary white-point adaptation API | Y | Y | -- | -- |
| CIECAM02 / CAM16 | Y | Y | -- | -- |
| ZCAM / Hellwig2022 / Kim2009 / LLAB / Hunt / ATD95 | Y | P | -- | -- |

### 3.2 Colour-difference metrics

| Area | Alwan | colour-science | OpenColorIO | aces-dev |
|------|:-----:|:--------------:|:-----------:|:--------:|
| DE76 / DE94 / CMC / DE2000 | Y | Y | -- | -- |
| DE ITP / HyAB / DIN99 | Y | Y | -- | -- |
| CAM-based Delta E variants | Y | Y | -- | -- |

### 3.3 Spectral / gamut / vision

| Area | Alwan | colour-science | OpenColorIO | aces-dev |
|------|:-----:|:--------------:|:-----------:|:--------:|
| illuminants / CMFs / spectral integration | Y | Y | -- | -- |
| spectrum upsampling / hero wavelength tools | Y | P | -- | -- |
| gamut mapping helpers | Y | P | P | -- |
| light quality / CCT / CRI-style metrics | Y | Y | -- | -- |
| CVD / vision helpers | Y | P | -- | -- |

---

## 4. Batch Processing And Integration Surface

| Capability | Alwan | colour-science | OpenColorIO | aces-dev |
|------------|:-----:|:--------------:|:-----------:|:--------:|
| explicit C ABI | Y | -- | P | -- |
| explicit `_f32` and `_f64` public API | Y | -- | P | -- |
| typed interleaved batch functions | Y | P | P | -- |
| planar batch functions | Y | P | P | -- |
| typed-pixel frontends (`U8` / `U16` / `F16` / `F32` / `F64`) | Y | -- | P | -- |
| row-strided image conversion helpers | Y | -- | P | -- |
| deterministic build mode | Y | -- | P | -- |
| custom allocator hooks | Y | -- | P | -- |

OpenColorIO is strong at graph/pipeline integration, but Alwan exposes a more
direct low-level batch-processing C surface for callers that already own their
buffers and just want the math.

---

## 5. Interchange And Tooling

| Capability | Alwan | colour-science | OpenColorIO | aces-dev |
|------------|:-----:|:--------------:|:-----------:|:--------:|
| 1D / 2D / 3D LUT baking | Y | P | Y | P |
| LUT sampling helpers | Y | P | P | -- |
| `.cube` import / export | Y | P | Y | -- |
| CLF export | Y | P | Y | P |
| interop ID parse / format / enumerate helpers | Y | -- | P | -- |
| half conversion helpers | Y | -- | P | -- |
| integer normalization helpers | Y | P | P | -- |
| video signal encode / decode helpers | Y | P | P | -- |

---

## 6. Positioning Summary

### Where Alwan is stronger

- embeddable C-first API
- explicit precision variants in the public header
- direct batch math over caller-owned buffers
- typed-pixel and planar entry points
- compact production-library footprint without a full pipeline runtime

### Where the comparison libraries are stronger

- `colour-science`: breadth of exploratory / research workflows and Python ergonomics
- `OpenColorIO`: config-driven colour-management orchestration and DCC ecosystem integration
- `aces-dev`: reference transform material and standards alignment

### Important distinction

Alwan is a colour-science library with interchange helpers, not a full
facility-grade colour-management framework. It is strongest when the caller
already knows the pipeline policy and wants the transforms, descriptors, and
buffer-processing surface in a native C library.

---

## Related Docs

- [README.md](README.md)
- [examples.md](examples.md)
- [api-conventions.md](api-conventions.md)
- [TODO_ColorInterop.md](TODO_ColorInterop.md)
