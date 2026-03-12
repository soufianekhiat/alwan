# Alwan

> **Alwan** (ألوان) — Arabic for "colours"

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Windows f64](https://github.com/soufianekhiat/alwan/actions/workflows/ci-windows.yml/badge.svg)](https://github.com/soufianekhiat/alwan/actions/workflows/ci-windows.yml)
[![Linux f64](https://github.com/soufianekhiat/alwan/actions/workflows/ci-linux.yml/badge.svg)](https://github.com/soufianekhiat/alwan/actions/workflows/ci-linux.yml)
[![macOS f64](https://github.com/soufianekhiat/alwan/actions/workflows/ci-macos.yml/badge.svg)](https://github.com/soufianekhiat/alwan/actions/workflows/ci-macos.yml)

A small, dependency-free colour science library in pure C11. Alwan provides production-ready colour math for applications that need precise, deterministic colour transformations without the overhead of external dependencies.

**Alwan is a colour science library, not a colour management library.** It provides the mathematical foundations — colour space conversions, chromatic adaptation, appearance models, spectral operations — but does not handle ICC profiles, device characterization, rendering intents, or profile connection spaces. For ICC workflow support, use Alwan for the underlying math and a dedicated library (e.g. LittleCMS) for profile I/O.

---

## Why Alwan?

**Built for Production**
- Zero external dependencies
- Pure C11

**Performance-First Design**
- Configurable precision (`float` or `double`) at compile time

**Scientifically Rigorous**
- Targeting feature parity with Python's [Colour](https://github.com/colour-science/colour) library (not yet reached)
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

   // Convert sRGB to CIE Lab (output parameters first)
   alwan_rgb rgb = {0.5, 0.3, 0.2};
   alwan_xyz xyz;
   alwan_lab lab;
   alwan_xyz d65 = {0.95047, 1.0, 1.08883};

   alwan_srgb_to_xyz(&xyz, &rgb);
   alwan_xyz_to_lab(&lab, &xyz, &d65);

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
- **Camera logs:** ARRI LogC3/4, Sony S-Log2/3, Canon C-Log2/3, Panasonic V-Log, RED Log3G10, Fujifilm F-Log/F-Log2, DJI D-Log, Blackmagic Film Gen4/5, Leica L-Log
- **Film:** Cineon, ADX10/16
- **AgX:** AgX view transforms

### ACES Pipeline
Complete Academy Color Encoding System support:
- **ACES 1.x:** RRT+ODT output transforms for SDR/HDR displays (sRGB, Rec.709, P3, Rec.2020)
- **ACES 2.0:** JMh-based Output Transform with gamut compression and tone mapping
- **Encodings:** ACES2065-1 (AP0), ACEScg (AP1), ACEScc, ACEScct, ACESproxy
- **LMTs:** Look Modification Transforms (Glow, RedMod, Blue Light Artifact Fix)
- **Gamut tools:** ACES 1.3 GamutCompress, ACES 2.0 gamut compression

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
No global mutable state. All state lives in explicit context objects (`alwan_ctx`). Same inputs always produce identical outputs.

### You Control Memory
Override allocation with custom `ALWAN_ALLOC`/`ALWAN_FREE` macros. Integrate seamlessly with game engines, embedded systems, or custom allocators.

### Precision Where You Need It
Choose `float` (7 digits, faster) or `double` (16 digits, default) at compile time. Test tolerances automatically adapt to your chosen precision. Favor `double` to test against colour-science expected results.

### Output-First API
All functions place output parameters first, following a consistent convention:
```c
alwan_function(output, ctx, input, count, in_stride, out_stride);
```
This matches common C patterns (like `memcpy`) and improves code readability.

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
#define ALWAN_EMBED_DATA 1  // 1 = embed (default)
```

**Embedded mode (default):** Reference data compiled directly into binary. Zero runtime I/O, instant startup.

> **Note:** Runtime data loading (`ALWAN_EMBED_DATA=0`) is not yet implemented. Only embedded mode is currently supported.

### Custom Allocators

```c
#define ALWAN_ALLOC(sz, align) my_alloc(sz, align)
#define ALWAN_FREE(p)          my_free(p)
```

---

## API Examples

### Context Management

```c
// Simple initialization (recommended)
alwan_ctx *ctx = alwan_create(NULL);

// Custom allocators
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .alloc_cb = custom_alloc,
    .free_cb = custom_free
});

alwan_destroy(ctx);
```

### Colour Space Conversions

```c
// Single colour (output first)
alwan_rgb rgb = {0.8, 0.2, 0.1};
alwan_xyz xyz;
alwan_lab lab;
alwan_xyz d65 = {0.95047, 1.0, 1.08883};

alwan_srgb_to_xyz(&xyz, &rgb);
alwan_xyz_to_lab(&lab, &xyz, &d65);

// Bulk conversion with stride (output first)
alwan_xyz xyz_data[100];
alwan_lab lab_data[100];
alwan_xyz_to_lab_bulk((alwan_scalar*)lab_data, (alwan_scalar*)xyz_data, &d65,
                      100, sizeof(alwan_xyz), sizeof(alwan_lab));
```

### Chromatic Adaptation

```c
alwan_xyz d50 = {0.96422, 1.0, 0.82521};
alwan_xyz d65 = {0.95047, 1.0, 1.08883};
alwan_xyz xyz_in = {0.5, 0.6, 0.4};
alwan_xyz xyz_out;

// Adapt from D50 to D65 using Bradford
alwan_cat_adapt((alwan_scalar*)&xyz_out, &d50, &d65, ALWAN_CAT_BRADFORD,
                (alwan_scalar*)&xyz_in, 1, sizeof(alwan_xyz), sizeof(alwan_xyz));
```

### RGB Space Operations

```c
// Derive matrices for custom RGB space (outputs first)
alwan_rgb_space_desc rgb_desc = {
    .primaries_xy = {0.64, 0.33, 0.30, 0.60, 0.15, 0.06},  // rx,ry, gx,gy, bx,by
    .white_xy = {0.3127, 0.3290},
    .oetf = ALWAN_TF_LINEAR,
    .eotf = ALWAN_TF_LINEAR
};

alwan_mat3x3 rgb_to_xyz, xyz_to_rgb;
alwan_rgb_derive_matrices(&rgb_to_xyz, &xyz_to_rgb, &rgb_desc);

// Convert between RGB spaces (output first)
alwan_rgb_space_desc srgb_desc, bt2020_desc;
alwan_rgb_get_space_descriptor(&srgb_desc, ctx, ALWAN_RGB_SPACE_SRGB);
alwan_rgb_get_space_descriptor(&bt2020_desc, ctx, ALWAN_RGB_SPACE_BT2020);

alwan_rgb rgb_in = {0.8, 0.3, 0.2};
alwan_rgb rgb_out;
alwan_rgb_convert(&rgb_out, ctx, &srgb_desc, &bt2020_desc, &rgb_in);
```

### Transfer Functions

```c
alwan_scalar linear_rgb[300], encoded_rgb[300];  // 100 RGB triplets

// Apply transfer function (OETF: linear → encoded) - output first
alwan_oetf_apply(encoded_rgb, ALWAN_TF_SRGB, linear_rgb, 300,
                 sizeof(alwan_scalar), sizeof(alwan_scalar));

// Inverse transfer function (EOTF: encoded → linear) - output first
alwan_eotf_apply(linear_rgb, ALWAN_TF_SRGB, encoded_rgb, 300,
                 sizeof(alwan_scalar), sizeof(alwan_scalar));
```

### Matrix Operations

```c
alwan_mat3x3 M, M_inv;
alwan_mat3_inv(&M_inv, &M);  // Output first

alwan_vec3 v_in = {1.0, 0.5, 0.2};
alwan_vec3 v_out;
alwan_mat3_mulv(&v_out, &M, &v_in);  // Output first
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
- **Authoritative fixtures:** Reference values computed from Python's [colour-science](https://github.com/colour-science/colour) library
- **Comprehensive coverage:** Canonical cases, edge cases, sweeps for each module
- **Precision-aware validation:** Error thresholds adapt to build configuration (1e-12 for double, 1e-5 for float)
- **Clear reporting:** Single executable, immediate pass/fail feedback

**Data Generation:**
Test reference values and data files are generated by Python scripts in `gendata/`:
```powershell
python gendata/generate_all_reference_values.py src/alwan/data
```
Only test *inputs* are hardcoded — all expected *outputs* come directly from colour-science.

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
│   └── reference_values/  # Test fixtures (generated from colour-science)
├── gendata/                # Python data generation scripts
│   ├── data/              # Data file generators (illuminants, CMFs, matrices)
│   └── tests/             # Test reference generators (CAM test cases)
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
- ✅ Unified test suite (58 test suites)
- ✅ 100+ RGB color spaces
- ✅ Comprehensive color appearance models (CIECAM02, CAM16, LLAB, Hellwig2022, Kim2009, ATD95)
- ✅ Modern color spaces (Oklab, JzAzBz, ICtCp)
- ✅ Advanced color difference metrics (ΔE76, ΔE94, ΔE00, CMC, CAM02-LCD/SCD, CAM16-LCD/SCD)
- ✅ Light quality metrics (CRI, CQS, SSI, TM-30)
- ✅ Spectral operations and gamut tools
- ✅ ACES 1.x RRT+ODT pipeline (validated against OpenColorIO)
- ✅ ACES 2.0 Output Transform (JMh gamut mapping, tone scale compression)
- ✅ 37+ camera log formats (F-Log, D-Log, LogC, S-Log, V-Log, RED, BMDFilm, etc.)

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
  - Data embedding mode (embedded mode only; runtime loading planned)
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
  - Embedded mode (compiled-in data) — current implementation
  - Runtime mode (load from CSV) — planned for future release
  - Data format and structure
  - Memory usage and performance

---

## Contributing

Development is incentivized through Patreon:

[![Patreon](https://img.shields.io/badge/Patreon-Become%20a%20Patron-f96854?style=for-the-badge&logo=patreon)](https://www.patreon.com/SoufianeKHIAT)

---

## License

MIT License — see [LICENSE](LICENSE) for details
