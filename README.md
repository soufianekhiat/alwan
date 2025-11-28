# Alwan

> **Alwan** (ألوان) — Arabic for "colours"

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)](https://github.com/yourusername/alwan)

A small, dependency-free colour science library in pure C11. Alwan provides production-ready colour math for applications that need precise, deterministic colour transformations without the overhead of external dependencies.

---

## Why Alwan?

**Built for Production**
- Zero external dependencies — pure C11 with a stable ABI
- Deterministic results across platforms and builds
- Full control over memory allocation

**Performance-First Design**
- Bulk-first API with stride support for efficient array processing
- Configurable precision (`float` or `double`) at compile time
- Optional SIMD paths without sacrificing portability

**Scientifically Rigorous**
- Targeting feature parity with Python's [Colour](https://github.com/colour-science/colour) library
- Comprehensive reference data (CMFs, illuminants, RGB spaces)
- Validated against authoritative test fixtures

---

## Quick Start

**Prerequisites:** Visual Studio 2022, .NET 6.0+ SDK, Git

1. **Clone and bootstrap:**
   ```batch
   git clone --recursive https://github.com/yourusername/alwan.git
   cd alwan
   bootstrap.bat
   ```

2. **Build and test:**
   ```batch
   msbuild Alwan_vs2022_win64.sln /p:Configuration=Debug_f64
   working_dir\AlwanTests.exe
   ```

3. **Start coding:**
   ```c
   #include "alwan.h"

   alwan_ctx *ctx = alwan_create(NULL);

   // Convert sRGB to CIE Lab
   alwan_vec3 rgb = {0.5, 0.3, 0.2};
   alwan_vec3 xyz, lab;
   alwan_rgb_to_xyz(&rgb, &xyz, 1, 0, 0);
   alwan_xyz_to_lab(&xyz, &d65_white, &lab, 1, 0, 0);

   alwan_destroy(ctx);
   ```

---

## Features

### Colour Models & Conversions
Transform between industry-standard colour spaces:
- **CIE spaces:** XYZ, xyY, Lab, Luv, LCh, LCh(uv)
- **Perceptual models:** IPT, ICtCp, JzAzBz, Oklab
- **RGB families:** sRGB, Adobe RGB, BT.709, BT.2020, Display P3, ACES, and more
- **Encoding spaces:** HSV, HSL, YCbCr, CMY, CMYK

### Chromatic Adaptation
Accurate white point adaptation using:
- Bradford (industry standard)
- CAT02, CAT16 (CIECAM models)
- XYZ scaling
- Custom transform matrices

### Transfer Functions & HDR
Support for modern display and camera encoding:
- **SDR:** sRGB, BT.709, BT.1886
- **HDR:** ST.2084 (PQ), HLG
- **Log curves:** S-Log, C-Log, V-Log, ARRI LogC
- **ACES & AgX:** ACES2065-1, ACEScg, ACESproxy, RRT+ODT, AgX view transforms

### Colour Appearance & Quality
Perceptual modelling and light quality metrics:
- **Appearance models:** CIECAM02, CAM16 (forward/inverse, UCS variants)
- **Colour differences:** ΔE76, ΔE94, CMC, ΔE00
- **Light quality:** CRI, CQS, SSI, TM-30
- **CCT estimation:** McCamy, Robertson

### Spectral & Gamut Tools
Low-level colour science operations:
- SPD integration to tristimulus values
- CMFs: CIE 1931/1964/2012, Stockman & Sharpe
- Standard illuminants: A, D-series, E, F-series
- Gamut mapping, volume calculation, coverage analysis

**Out of scope:** Plotting, device I/O, image codecs, GUI, threading

---

## Design Philosophy

### Pure C, Zero Dependencies
Built entirely in C11 with no external libraries. The only build dependency is Sharpmake (included as submodule). Deploy a single header and implementation — no package managers, no runtime surprises.

### Deterministic & Re-entrant
No global mutable state. All state lives in explicit context objects (`alwan_ctx`). Same inputs always produce identical outputs across platforms and builds.

### You Control Memory
Override allocation with custom `ALWAN_ALLOC`/`ALWAN_FREE` macros. Integrate seamlessly with game engines, embedded systems, or custom allocators.

### Precision Where You Need It
Choose `float` (7 digits, faster) or `double` (16 digits, default) at compile time. Test tolerances automatically adapt to your chosen precision.

### Built for Arrays
All transform functions process arrays with configurable stride. Transform entire images or interleaved vertex buffers efficiently without data copies.

---

## Configuration

### Scalar Type

Edit [alwan_config.h](src/alwan/alwan_config.h) or define at compile time:

```c
#define ALWAN_SCALAR_IS_FLOAT 0  // 0 = double (default), 1 = float
```

Build configurations:
- **Debug_f32** / **Release_f32** — 32-bit float precision
- **Debug_f64** / **Release_f64** — 64-bit double precision (default)

### Data Embedding

```c
#define ALWAN_EMBED_DATA 1  // 1 = embed (default), 0 = runtime load
```

**Embedded mode (default):** Reference data compiled directly into binary. Zero runtime I/O, instant startup.

**Runtime mode:** Load CSV data at initialization. Smaller binary, flexible data updates.

### Custom Allocators

```c
#define ALWAN_ALLOC(sz, align) my_alloc(sz, align)
#define ALWAN_FREE(p)          my_free(p)
```

---

## API Examples

### Context Management

```c
// Simple initialization
alwan_ctx *ctx = alwan_create(NULL);

// Custom configuration
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .alloc_cb = custom_alloc,
    .free_cb = custom_free,
    .runtime_data_root = "path/to/data"  // Only for ALWAN_EMBED_DATA=0
});

alwan_destroy(ctx);
```

### Colour Space Conversions

```c
// Single colour
alwan_vec3 rgb = {0.8, 0.2, 0.1};
alwan_vec3 xyz, lab;
alwan_rgb_to_xyz(&rgb, &xyz, 1, 0, 0);
alwan_xyz_to_lab(&xyz, &d65_white, &lab, 1, 0, 0);

// Bulk conversion with stride
float rgb_data[100 * 3];  // 100 RGB colours
float lab_data[100 * 3];
alwan_xyz_to_lab(rgb_data, &d65_white, lab_data, 100, 3, 3);
```

### Chromatic Adaptation

```c
alwan_vec3 xyz_d50 = {0.5, 0.6, 0.4};
alwan_vec3 xyz_d65;

alwan_xyz_adapt(ctx, ALWAN_CAT_BRADFORD,
                &d50_white, &d65_white,
                &xyz_d50, 1, 0, &xyz_d65, 0);
```

### RGB Space Operations

```c
// Derive matrices for custom RGB space
alwan_rgb_descriptor rgb_desc = {
    .primaries = {{0.64, 0.33}, {0.30, 0.60}, {0.15, 0.06}},
    .white_point = {0.3127, 0.3290},
    .transfer = "srgb"
};

alwan_mat3x3 rgb_to_xyz, xyz_to_rgb;
alwan_rgb_derive_matrices(ctx, &rgb_desc, &rgb_to_xyz, &xyz_to_rgb);

// Convert between RGB spaces
alwan_rgb_convert(ctx, "srgb", "bt2020", rgb_in, 100, 3, rgb_out, 3);
```

### Transfer Functions

```c
// Apply transfer function (OETF: linear → encoded)
alwan_oetf_apply(ctx, "srgb", linear_rgb, 100, 3, encoded_rgb, 3);

// Inverse transfer function (EOTF: encoded → linear)
alwan_eotf_apply(ctx, "srgb", encoded_rgb, 100, 3, linear_rgb, 3);

// View transforms for HDR
alwan_view_transform_apply(ctx, "agx", hdr_rgb, 100, 3, display_rgb, 3);
```

### Matrix Operations

```c
alwan_mat3x3 M, M_inv;
alwan_mat3_inv(&M, &M_inv);

alwan_vec3 v_in = {1.0, 0.5, 0.2};
alwan_vec3 v_out;
alwan_mat3_mul_vec3(&M, &v_in, &v_out);
```

---

## Data Strategy

Alwan ships reference datasets as **C-parsable CSV files** with maximum-precision literals:

```csv
0.950470000000000,1.000000000000000,1.088830000000000
```

These files serve dual purposes:
- **Embedded mode:** Included directly in C array initializers (zero runtime I/O)
- **Runtime mode:** Parsed and cached at initialization (smaller binary)

Filenames encode identity and sampling: `cie_1931_2deg_xbar_360_830_1nm.csv`

---

## Numerical Stability

Alwan prioritizes correctness and determinism:

- **Matrix operations:** 3×3 inversion via partial-pivot Gaussian elimination
- **Transfer functions:** Carefully clamped branches to avoid NaN/Inf propagation
- **Spectral integration:** Simpson's rule with trapezoid fallback
- **Explicit white points:** All conversions require explicit white point parameters
- **Precision-aware testing:** Tolerances adapt to `float` (1e-5) or `double` (1e-12)

---

## Testing

Self-contained test suite with no external framework:

```batch
working_dir\AlwanTests.exe
```

**Test Strategy:**
- **Authoritative fixtures:** Reference values from Python Colour library
- **Comprehensive coverage:** Canonical cases, edge cases, sweeps for each module
- **Precision-aware validation:** Error thresholds adapt to build configuration
- **Clear reporting:** Single executable, immediate pass/fail feedback

---

## Project Structure

```
alwan/
├── data/                   # Reference datasets (CMFs, illuminants, RGB descriptors)
├── src/alwan/              # Library source (pure C11)
│   ├── alwan.h            # Main public API
│   ├── alwan_config.h     # Compile-time configuration
│   └── ...
├── tests/unit/             # Self-contained test suite
├── sharpmake/              # Build system scripts
├── extern/Sharpmake/       # Build tool (submodule)
├── working_dir/            # Test executable output
└── bootstrap.bat           # One-step setup
```

---

## Build System

Alwan uses [Sharpmake](https://github.com/ubisoft/Sharpmake) for project generation:

- **No vcpkg/Conan/etc:** Single submodule dependency
- **Fast incremental builds:** Only regenerate when scripts change
- **Multi-configuration:** Debug/Release × float32/float64 in one solution

Regenerate projects after modifying `.cs` files:
```batch
generate_projects.bat
```

---

## Current Status

**Foundation Complete**

- ✅ Context management with custom allocators
- ✅ Matrix operations (multiply, inverse)
- ✅ Scalar abstraction (float/double)
- ✅ Data embedding with diagnostic guards
- ✅ Sharpmake build system
- ✅ Unified test suite
- ✅ 100+ RGB color spaces
- ✅ Comprehensive color appearance models (CIECAM02, CAM16, LLAB, Hellwig2022, Kim2009, ATD95)
- ✅ Modern color spaces (Oklab, JzAzBz, ICtCp)
- ✅ Advanced color difference metrics (ΔE76, ΔE94, ΔE00, CMC, CAM02-LCD/SCD, CAM16-LCD/SCD)
- ✅ Light quality metrics (CRI, CQS, SSI, TM-30)
- ✅ Spectral operations and gamut tools

---

## Documentation

Comprehensive documentation is available in the [docs/](docs/) folder. Always refer to [alwan.h](src/alwan/alwan.h) for the current API reference.

### 📚 Guides

- **[Getting Started](docs/getting-started.md)** — Installation, first program, basic concepts, and common workflows
  - Building and running tests
  - Your first color conversion
  - Understanding contexts and white points
  - Common pitfalls and debugging tips

- **[Configuration](docs/configuration.md)** — Compile-time options for precision, data embedding, and custom allocators
  - Scalar precision (float vs double)
  - Data embedding mode (embedded mode active; runtime loading available)
  - Custom memory allocators
  - Platform-specific settings

- **[Examples](docs/examples.md)** — 13 practical examples covering real-world use cases
  - Basic color conversions
  - Image processing workflows
  - HDR tone mapping
  - Color grading pipelines
  - Multi-threading patterns
  - Error handling

### 🔧 API Reference

Detailed function documentation with signatures, parameters, and usage patterns:

- **[Context Management](docs/api/context.md)** — Library initialization, memory management, lifecycle
- **[Color Spaces](docs/api/color-spaces.md)** — CIE XYZ/Lab/Luv, RGB conversions, perceptual models (Oklab, JzAzBz, ICtCp)
- **[Chromatic Adaptation](docs/api/chromatic-adaptation.md)** — White point transforms (Bradford, CAT02, CAT16)
- **[Transfer Functions](docs/api/transfer-functions.md)** — EOTFs/OETFs for SDR/HDR (sRGB, PQ, HLG, log curves)
- **[Matrix Operations](docs/api/matrix-operations.md)** — 3×3 matrix math for linear transforms
- **[Spectral Operations](docs/api/spectral.md)** — SPD integration, CMFs, illuminants
- **[Color Appearance](docs/api/color-appearance.md)** — CIECAM02, CAM16, LLAB, Hellwig2022, Kim2009, ATD95
- **[Color Difference](docs/api/color-difference.md)** — ΔE metrics (ΔE76, ΔE94, ΔE00, CMC, CAM02/16-LCD/SCD)
- **[Gamut Operations](docs/api/gamut.md)** — Gamut mapping and analysis

### 📖 Technical Details

- **[Precision & Limits](docs/precision-and-limits.md)** — Numerical accuracy, error bounds, and performance trade-offs
  - Float vs double accuracy comparison
  - Precision-aware test tolerances
  - Edge case handling (black, achromatic, out-of-gamut)
  - Accumulation error analysis

- **[Data Management](docs/data-management.md)** — How reference data (CMFs, illuminants, RGB spaces) is handled
  - Embedded mode (compiled-in data) — default configuration
  - Runtime mode (load from CSV) — available for flexible deployments
  - Data format and structure
  - Memory usage and performance

---

## Contributing

Development is incentivized through Patreon:

[![Patreon](https://img.shields.io/badge/Patreon-Become%20a%20Patron-f96854?style=for-the-badge&logo=patreon)](https://www.patreon.com/SoufianeKHIAT)

---

## License

MIT License — see [LICENSE](LICENSE) for details
