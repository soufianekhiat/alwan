# Alwan

> **Alwan** (ألوان) — Arabic for "colours"

| Platform    | x64 | ARM |
|-------------|-----|-----|
| **Linux**   | [![Linux build](https://github.com/soufianekhiat/alwan/actions/workflows/linux-f64.yml/badge.svg?branch=main)](https://github.com/soufianekhiat/alwan/actions/workflows/linux-f64.yml)       | [![Linux ARM build](https://github.com/soufianekhiat/alwan/actions/workflows/linux-arm-f64.yml/badge.svg?branch=main)](https://github.com/soufianekhiat/alwan/actions/workflows/linux-arm-f64.yml)     |
| **Windows** | [![Windows build](https://github.com/soufianekhiat/alwan/actions/workflows/windows-f64.yml/badge.svg?branch=main)](https://github.com/soufianekhiat/alwan/actions/workflows/windows-f64.yml) | [![Windows ARM build](https://github.com/soufianekhiat/alwan/actions/workflows/windows-arm-f64.yml/badge.svg?branch=main)](https://github.com/soufianekhiat/alwan/actions/workflows/windows-arm-f64.yml) |
| **macOS**   | [![macOS build](https://github.com/soufianekhiat/alwan/actions/workflows/macos-f64.yml/badge.svg?branch=main)](https://github.com/soufianekhiat/alwan/actions/workflows/macos-f64.yml)       | [![macOS ARM build](https://github.com/soufianekhiat/alwan/actions/workflows/macos-arm-f64.yml/badge.svg?branch=main)](https://github.com/soufianekhiat/alwan/actions/workflows/macos-arm-f64.yml)     |

> Tests and benchmarks live in the sibling
> [alwan_dev](https://github.com/soufianekhiat/alwan_dev) repo — the lib build
> badges above only verify a clean compile across the six host targets.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))

A small, dependency-free colour science library in pure C11. Alwan provides production-ready colour math for applications that need precise, deterministic colour transformations without the overhead of external dependencies.

**Alwan is a colour science library, not a colour management library.** It provides the mathematical foundations — colour space conversions, chromatic adaptation, appearance models, spectral operations — but does not handle ICC profiles, device characterization, rendering intents, or profile connection spaces. For ICC workflow support, use Alwan for the underlying math and a dedicated library (e.g. LittleCMS) for profile I/O.

---

## Why Alwan?

**Built for Production**
- Zero external dependencies — pure C11, single library target
- Six host targets verified in CI (Linux/macOS/Windows × x64/ARM)

**Performance-First Design**
- Both `float` and `double` precision in one binary; pick at the
  call site
- SIMD bulk kernels with backends for SSE2 / AVX / AVX2 / NEON, plus
  a scalar fallback for any other ISA
- Map kernels operate on AoS/SoA buffers with `memcpy`-style
  argument convention

**Scientifically Rigorous**
- Validated against Python's [colour-science](https://github.com/colour-science/colour) library
- Comprehensive reference data (CMFs, illuminants, RGB spaces, ACES
  matrices) embedded as full-precision CSV literals
- Authoritative fixtures regenerated automatically from
  colour-science via the gendata pipeline

**Reproducible**
- Opt-in [`ALWAN_DETERMINISTIC=ON`](docs/determinism.md) makes the
  library produce **byte-identical output across every supported
  platform, compiler, and optimisation level** — useful for golden
  files, audit pipelines, and frame-accurate cross-platform replay

---

## Quick Start

**Prerequisites (Sharpmake/MSVC):** Visual Studio 2022, .NET 6.0+ SDK, Git

Or

**Prerequisites (CMake):** CMake 3.15+, any C11 compiler, Git

1. **Clone and bootstrap:**
   ```batch
   git clone --recursive https://github.com/soufianekhiat/alwan.git
   cd alwan
   buildsystem\bootstrap.bat
   ```

2. **Build the library (Sharpmake/MSVC):**
   ```batch
   msbuild Alwan_vs2022_Win64.sln /p:Configuration=Release_f64
   ```

   **Or with CMake:**
   ```sh
   cmake -S . -B build
   cmake --build build
   ```

   Tests, benchmarks, and image-gen tooling live in the sibling
   [alwan_dev](https://github.com/soufianekhiat/alwan_dev) repo.
   This repo only ships the library + headers; clone alwan_dev next
   to it for the test runner.

3. **Start coding:**
   ```c
   #include "alwan.h"

   /* Stateless conversions don't need a context — call directly. */
   alwan_xyz_f64 const d65 = {0.95047, 1.0, 1.08883};
   alwan_xyz_f64 xyz = {0.41246, 0.21267, 0.01933};   /* Pure red in XYZ */
   alwan_lab_f64 lab;

   alwan_xyz_to_lab_f64(&lab, &xyz, &d65);
   /* lab.L ≈ 53.24,  lab.a ≈ 80.09,  lab.b ≈ 67.20 */
   ```

   The optional `alwan_ctx` is only needed for APIs that allocate
   (LUT bake, CLF export, RGB-space lookup with caching). See the
   [Context Management](docs/api/context.md) doc for details.

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
- Sharp, XYZ scaling
- Zhai & Luo 2018 two-step CAT
- Custom transform matrices

### Transfer Functions & HDR
Support for modern display and camera encoding:
- **SDR:** sRGB, BT.709, BT.1886, gamma 2.2 / 2.4 / 2.6 / 2.8
- **HDR:** ST.2084 (PQ), HLG, BT.2390 EETF, BT.2446 A/B/C
- **Camera logs:** ARRI LogC3/LogC4, Sony S-Log/2/3, Canon C-Log/2/3,
  Panasonic V-Log, Nikon N-Log, RED REDLog/REDLogFilm/Log3G10,
  Fujifilm F-Log/F-Log2, DJI D-Log, Blackmagic Film Gen4/5, Leica
  L-Log, GoPro Protune
- **Film:** Cineon, ADX10/16
- **View transforms:** AgX (base/punchy/golden), Tony McMapface,
  Khronos PBR Neutral, Reinhard (extended + calibrated), Uchimura,
  Lottes, exposure-with-shoulder

### ACES Pipeline
Complete Academy Color Encoding System support:
- **ACES 1.x:** RRT+ODT output transforms for SDR/HDR displays (sRGB, Rec.709, P3, Rec.2020)
- **ACES 2.0:** JMh-based Output Transform with gamut compression and tone mapping
- **Encodings:** ACES2065-1 (AP0), ACEScg (AP1), ACEScc, ACEScct, ACESproxy
- **LMTs:** Look Modification Transforms (Glow, RedMod, Blue Light Artifact Fix)
- **Gamut tools:** ACES 1.3 GamutCompress, ACES 2.0 gamut compression

### Colour Appearance & Quality
Perceptual modelling and light quality metrics:
- **Appearance models:** CIECAM02, CAM16, Hellwig 2022, Kim 2009,
  ZCAM, Hunt, LLAB, ATD95, Nayatani 95, CAM18sl, CAM20u (most have
  forward + inverse + UCS variants)
- **Colour differences:** ΔE76, ΔE94, ΔE00, CMC, hyAB, ΔE_OK,
  ΔE_DIN99, ΔE_ITP, ΔE_CAM02 (LCD/SCD/UCS), ΔE_CAM16 (LCD/SCD/UCS)
- **Light quality:** CRI, CQS, SSI, TM-30 Rf
- **CCT estimation:** McCamy, Robertson, Hernandez-Andres, Kang,
  Ohno (with Δuv optimisation)
- **Vision:** Machado et al. CVD simulation (protan/deutan/tritan,
  full anomaly + dichromacy variants), CSF (Barten 1999)

### Spectral & Gamut Tools
Low-level colour science operations:
- SPD integration to tristimulus values (trapezoid + Simpson)
- CMFs: CIE 1931/1964/2012/2015, Stockman & Sharpe, Wright & Guild
- Standard illuminants: A, D-series, E, F-series, HP discharge
- RGB→spectrum upsampling: Smits 1999, Mallett 2019, Jakob & Hanika 2019
- Hero wavelength sampling for spectral renderers
- Gamut mapping (8 algorithms), Monte Carlo volume estimation,
  coverage analysis
- Rayleigh scattering model (Bodhaine 1999) for atmospheric work

**Out of scope:** plotting, GUI, image codec I/O (JPEG/PNG/TIFF
decoding), threading helpers. Use a dedicated library for those.

---

## Design Philosophy

### Pure C, Zero Dependencies
Built entirely in C11 with no external libraries. The only build dependency is Sharpmake (included as submodule). Deploy a single header and implementation — no package managers, no runtime surprises.

### Deterministic & Re-entrant
No global mutable state. All state lives in explicit context objects
(`alwan_ctx`). Same input → same output on the same machine, always.
For **byte-identical output across platforms** (compilers, optimisation
levels, ISA), opt in to [`ALWAN_DETERMINISTIC=ON`](docs/determinism.md).

### You Control Memory
Override allocation with custom `ALWAN_ALLOC`/`ALWAN_FREE` macros. Integrate seamlessly with game engines, embedded systems, or custom allocators.

### Precision Where You Need It
Both `float` (7 digits, faster) and `double` (16 digits) variants ship
in every build with explicit `_f32` / `_f64` suffixes — pick precision
at the call site, no recompile required. Test tolerances automatically
adapt to the variant under test. Favour `double` for reference and
agreement with colour-science.

### Output-First API
Bulk functions follow a consistent `memcpy`-style convention with the
output buffer first and stride next to its buffer:
```c
alwan_function(out, out_stride, in, in_stride, count, [extras]..., [ctx]);
```
Single-pixel functions take output first too:
```c
alwan_xyz_to_lab_f64(&lab_out, &xyz_in, &white);
```
The full set of rules (where `ctx` goes, error contracts, precision
suffixes) lives in [api-conventions.md](docs/api-conventions.md).

---

## Configuration

### Precision

The library always compiles both `_f32` and `_f64` function variants
in a single binary. Pick precision at the call site with the
appropriate suffix; there is no global "default" precision flag.

Sharpmake build configurations:
- **`Debug_f64`** / **`Release_f64`** — double precision (testing reference)
- **`Debug_f32`** / **`Release_f32`** — single precision (mobile/perf)

### Deterministic Mode

```sh
cmake -DALWAN_DETERMINISTIC=ON ...
```

Opt in to **byte-identical output across compilers, optimization
levels, and CPU architectures** (Linux/macOS/Windows × x86_64/aarch64).
The library swaps libm `pow / exp / log / cbrt` for in-house
polynomial implementations, disables FMA contraction, and routes
SIMD reductions through a canonical scalar fallback.

The cross-platform CI matrix in
[`alwan_dev/.github/workflows/determinism.yml`](https://github.com/soufianekhiat/alwan_dev/blob/main/.github/workflows/determinism.yml)
diffs a 165k-line dump across six runners every commit. Full design
notes in [docs/determinism.md](docs/determinism.md).

### Data Embedding

```c
#define ALWAN_EMBED_DATA 1  // 1 = embed (default)
```

**Embedded mode (default):** Reference data compiled directly into the
binary. Zero runtime I/O, instant startup. Runtime data loading
(`ALWAN_EMBED_DATA=0`) is reserved for a future release.

### Custom Allocators

```c
#define ALWAN_ALLOC(sz, align) my_alloc(sz, align)
#define ALWAN_FREE(p)          my_free(p)
```

---

## API Examples

All public functions have explicit `_f32` / `_f64` suffixes. Bulk
APIs follow the convention
`(out, out_stride, in, in_stride, count, [extras]…, [ctx])`
mirroring `memcpy`. See [api-conventions.md](docs/api-conventions.md)
for the complete rule set.

### Context Management

```c
/* Stateless math doesn't need a context — only allocate one for APIs
 * that cache or allocate (LUT bake, CLF export, RGB-space registry). */
alwan_ctx *ctx = alwan_create(NULL);

/* Custom allocators via alwan_config. */
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .alloc_cb = custom_alloc,
    .free_cb  = custom_free,
});

alwan_destroy(ctx);
```

### Colour Space Conversions

```c
/* Single pixel: output first. */
alwan_xyz_f64 xyz = {0.41246, 0.21267, 0.01933};
alwan_xyz_f64 d65 = {0.95047, 1.00000, 1.08883};
alwan_lab_f64 lab;
alwan_xyz_to_lab_f64(&lab, &xyz, &d65);

/* Bulk over a 100-pixel buffer (interleaved XYZ -> Lab). */
alwan_f64 xyz_buf[300];   /* 100 * 3 channels */
alwan_f64 lab_buf[300];
alwan_xyz_to_lab_f64_map_interleave(
    lab_buf, sizeof(alwan_f64) * 3,
    xyz_buf, sizeof(alwan_f64) * 3,
    100, &d65);
```

### Chromatic Adaptation (D65 → D50, Bradford)

```c
alwan_xyz_f64 d65 = {95.047, 100.000, 108.883};
alwan_xyz_f64 d50 = {96.422, 100.000,  82.521};
alwan_f64 in [3]  = {50.0, 60.0, 40.0};
alwan_f64 out[3];

alwan_xyz_adapt_f64(out, sizeof(alwan_f64) * 3,
                    in,  sizeof(alwan_f64) * 3,
                    1, &d65, &d50, ALWAN_CAT_BRADFORD);
```

### RGB Space Operations

```c
/* Derive RGB ↔ XYZ matrices for a custom space. */
alwan_rgb_space_desc_f64 desc = {
    .primaries_xy = {0.64, 0.33, 0.30, 0.60, 0.15, 0.06},
    .white_xy     = {0.3127, 0.3290},
    .oetf = ALWAN_TF_LINEAR,
    .eotf = ALWAN_TF_LINEAR,
};
alwan_mat3x3_f64 rgb_to_xyz, xyz_to_rgb;
alwan_rgb_derive_matrices_f64(&rgb_to_xyz, &xyz_to_rgb, &desc);

/* Convert between two named RGB spaces. */
alwan_rgb_space_desc_f64 src_desc, dst_desc;
alwan_rgb_get_space_descriptor_f64(&src_desc, ALWAN_RGB_SPACE_SRGB,   ctx);
alwan_rgb_get_space_descriptor_f64(&dst_desc, ALWAN_RGB_SPACE_BT2020, ctx);

alwan_rgb_f64 in_rgb = {0.8, 0.3, 0.2};
alwan_rgb_f64 out_rgb;
alwan_rgb_convert_f64(&out_rgb, &src_desc, &dst_desc, &in_rgb, ctx);
```

### Transfer Functions

```c
alwan_f64 linear[300], encoded[300];   /* 100 RGB triplets */

/* OETF: linear -> encoded */
alwan_oetf_apply_f64(encoded, sizeof(alwan_f64),
                     linear,  sizeof(alwan_f64),
                     300, ALWAN_TF_SRGB);

/* EOTF: encoded -> linear */
alwan_eotf_apply_f64(linear,  sizeof(alwan_f64),
                     encoded, sizeof(alwan_f64),
                     300, ALWAN_TF_SRGB);
```

### Matrix Operations

```c
alwan_mat3x3_f64 M = { /* ... */ };
alwan_mat3x3_f64 M_inv;
alwan_mat3_inv_f64(&M_inv, &M);

alwan_vec3_f64 v_in  = {1.0, 0.5, 0.2};
alwan_vec3_f64 v_out;
alwan_mat3_mulv_f64(&v_out, &M, &v_in);
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

Alwan prioritises correctness and determinism:

- **Matrix operations:** 3×3 inversion via partial-pivot Gaussian
  elimination; row order preserved for cross-platform stability
- **Transfer functions:** Carefully clamped branches to avoid
  NaN/Inf propagation; OETF/EOTF pairs verified bit-stable in
  deterministic mode
- **Spectral integration:** Simpson's rule with trapezoid fallback;
  bandpass correction via Stearns & Stearns
- **Explicit white points:** All conversions require explicit white
  point parameters — no hidden defaults
- **Precision-aware testing:** Tolerances adapt to build mode —
  ~1e-5 for f32, ~1e-12 for f64, ~1e-3 for the deterministic
  polynomial path (which trades libm last-bit agreement for
  cross-platform reproducibility)

---

## Testing

The test suite, benchmarks, and reference-data generators live in the
sibling [alwan_dev](https://github.com/soufianekhiat/alwan_dev) repo
to keep this repository's footprint focused on the production library.

```sh
git clone https://github.com/soufianekhiat/alwan_dev.git
cd alwan_dev
cmake -S . -B build && cmake --build build --config Release
./build/tests/alwan_tests           # 89 test suites, single binary
```

**Test Strategy:**
- **Authoritative fixtures:** Reference values computed from Python's
  [colour-science](https://github.com/colour-science/colour) library
- **Comprehensive coverage:** Canonical cases, edge cases, sweeps for
  each module — 89 suites, ~thousands of assertions
- **Precision-aware validation:** Error thresholds adapt to build
  configuration (1e-12 for f64, 1e-5 for f32; looser in deterministic
  mode where polynomial approximation replaces libm)
- **SIMD-vs-scalar parity:** 58 dedicated bit-exact assertions in
  `88_simd_parity` confirm every public conversion produces
  byte-identical output through both code paths in deterministic mode
- **Cross-platform regression:** `det_run_regression` dumps ~165k
  hex-encoded f64/f32 values per platform; the CI matrix diffs all
  six runners against each other

**Data Generation:**
Reference data and test fixtures are generated by Python scripts in
`alwan_dev/gendata/` and `alwan_dev/tools/`. Only test *inputs* are
hardcoded — all expected *outputs* come directly from colour-science.

---

## Project Structure

```
alwan/                       # this repo (library only)
├── src/alwan/               # Library source (pure C11)
│   ├── alwan.h              # Main public API
│   ├── alwan_config.h       # Compile-time configuration
│   ├── alwan_math.h         # Math routing (libm or det polynomials)
│   ├── api/                 # API wrapper implementations
│   ├── core/                # Header-only GPU-compatible core
│   ├── map/                 # SIMD-accelerated bulk kernels
│   ├── simd/                # SIMD backend (SSE2 / AVX / AVX2 / NEON / scalar)
│   └── data/                # Embedded reference data (CSV)
├── buildsystem/sharpmake/   # Sharpmake project definitions
├── docs/                    # User-facing documentation
├── road_to_determinism.md   # Determinism design history
└── CMakeLists.txt           # CMake build (alternative to Sharpmake)

alwan_dev/                   # sibling repo (tests, benches, tools)
├── tests/                   # 89 test suites + reference fixtures
├── bench/                   # micro-benchmarks
├── det_regression/          # cross-platform determinism regression tool
├── image_gen/               # validation visuals
├── gendata/                 # Python reference-data generators
└── tools/                   # API survey, lint, no-raw-libm checks
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
- ✅ Matrix operations (3×3 multiply, inverse, derived RGB↔XYZ)
- ✅ Dual precision (f32 + f64 in one binary)
- ✅ Data embedding with diagnostic guards
- ✅ Sharpmake + CMake build systems
- ✅ Unified test suite (89 suites, hosted in alwan_dev)
- ✅ 25+ named RGB spaces, easy to add more via space descriptors
- ✅ Comprehensive colour appearance models — CIECAM02, CAM16, ZCAM,
  Hellwig 2022, Kim 2009, Hunt, LLAB, ATD95, Nayatani 95, CAM18sl, CAM20u
- ✅ Modern colour spaces — Oklab/Oklch, JzAzBz/JzCzHz, ICtCp,
  IPT, IgPgTg, ICaCb, ProLab, OSA-UCS, Hunter Lab, DIN99
- ✅ ΔE metrics — ΔE76 / ΔE94 / ΔE00 / CMC / hyAB / OK / DIN99 /
  ITP / CAM02-LCD/SCD/UCS / CAM16-LCD/SCD/UCS
- ✅ Light-quality metrics — CRI, CQS, SSI, TM-30 Rf
- ✅ Spectral operations and gamut tools (8 mapping algorithms)
- ✅ ACES 1.x RRT+ODT pipeline — 7 outputs, validated against OCIO
- ✅ ACES 2.0 Output Transform — 9 outputs, JMh gamut mapping
- ✅ 25+ transfer functions including all major camera log formats
- ✅ 16 view transforms (AgX, BT.2446 A/B/C, BT.2390, Tony McMapface,
  Reinhard, Khronos PBR Neutral, Uchimura, Lottes, Exposure, ACES Rec.709)
- ✅ Spectrum upsampling — Smits 1999, Mallett 2019, Jakob 2019
- ✅ Vision — Machado CVD simulation (6 types), CSF (default + Barten)
- ✅ Cross-platform CI on six host targets (Linux/macOS/Windows × x64/ARM)
- ✅ Opt-in deterministic mode with cross-platform bit-exact CI matrix
  covering ~270 public functions ([docs/determinism.md](docs/determinism.md))

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
  - Data format and structure
  - Memory usage and performance

- **[Determinism](docs/determinism.md)** — Opt-in `ALWAN_DETERMINISTIC=ON`
  bit-exact reproducibility across compilers, optimisation levels,
  and CPU architectures
  - Where last-bit divergence comes from (libm, FMA, SIMD reductions)
  - ULP-distance testing
  - Performance trade-off (~5–20%)
  - Cross-platform CI matrix (~270 public functions)

---

## Contributing

Development is incentivized through Patreon:

[![Patreon](https://img.shields.io/badge/Patreon-Become%20a%20Patron-f96854?style=for-the-badge&logo=patreon)](https://www.patreon.com/SoufianeKHIAT)

---

## License

MIT License — see [LICENSE](LICENSE) for details
