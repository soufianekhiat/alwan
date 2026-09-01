# Alwan

> **Alwan** (ألوان): Arabic for "colours"

[![CI](https://github.com/soufianekhiat/alwan/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/soufianekhiat/alwan/actions/workflows/ci.yml)
[![tests](https://img.shields.io/badge/tests-107%20suites-brightgreen)](#validation)
[![checks](https://img.shields.io/badge/checks-75%2C034%20per%20run-brightgreen)](#validation)
[![reference data](https://img.shields.io/badge/reference%20data-341%20sets-blue)](#validation)
[![configurations](https://img.shields.io/badge/configurations-8-blue)](#validation)
[![platforms](https://img.shields.io/badge/platforms-6-blue)](#validation)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))

Every count above is measured, not claimed: see [Validation](#validation) for
the table and the script that reproduces it.

CI is on-demand, since this repo's minutes are billed. `CI` in the Actions tab
builds the whole matrix in one click (Linux, Windows and macOS on x86_64 and
ARM, plus the permutation and precision matrices: shared linkage,
`ALWAN_DETERMINISTIC=ON`, Debug, and the single-precision builds), and each
workflow can still be dispatched on its own. Only the `CI` badge tracks those
runs: a workflow reached through `workflow_call` reports as a job inside the
CI run rather than a run of its own. Those jobs verify a clean compile; the
test suite lives in the sibling
[alwan_dev](https://github.com/soufianekhiat/alwan_dev) repo.

**Version: 2.0.0**, tagged
[`v2.0.0`](https://github.com/soufianekhiat/alwan/releases/tag/v2.0.0).

A small, dependency-free colour science library in pure C11, for applications that need precise, deterministic colour transforms.

**Alwan is a colour science library, not a colour management library.** It provides the mathematical foundations (colour space conversions, chromatic adaptation, appearance models, spectral operations) and leaves ICC profiles, device characterization, rendering intents, and profile connection spaces to dedicated tools. For ICC workflows, use Alwan for the math and a library such as LittleCMS for profile I/O.

---

## Contents

- [Why Alwan?](#why-alwan)
- [Install & Vendoring](#install--vendoring)
- [Quick Start](#quick-start)
- [Features](#features)
- [Validation](#validation)
- [Design Philosophy](#design-philosophy)
- [Configuration](#configuration)
- [Performance](#performance)
- [API Examples](#api-examples)
- [Data Strategy](#data-strategy)
- [Numerical Stability](#numerical-stability)
- [Testing](#testing)
- [Project Structure](#project-structure)
- [Build System](#build-system)
- [Current Status](#current-status)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [Roadmap to 3.0.0](#roadmap-to-300)
- [License](#license)

---

## Why Alwan?

What makes Alwan different from other colour libraries:

**1. Reference-validated correctness**
- Every algorithm is validated against the authority for that algorithm:
  [OCIO](https://opencolorio.org/), Python's
  [colour-science](https://github.com/colour-science/colour), the official
  ACES CTL, and vendors' own code (Sobotka's AgX, Blender's AgX, Jp-DRT)
- The ACES 2.0 output transform matches OCIO to
  **DeltaE ITP 0.000 across the full hue sweep**; Blender AgX is an exact match
- Every embedded constant traces to a gendata script that calls the
  reference implementation, so there are no hand-copied matrices

**2. Deterministic across platforms and backends**
- Opt-in [`ALWAN_DETERMINISTIC=ON`](docs/determinism.md) produces
  **byte-identical output across every supported platform, compiler, and
  optimisation level**, for golden files, audit pipelines, and
  frame-accurate replay
- The deterministic transfer functions are **bit-exact between the C build
  and the GPU backends** (verified on D3D12)

**3. One source, CPU and GPU**
- The per-pixel core kernels compile unchanged as **C, HLSL and GLSL
  shaders, and Halide C++**. The same `.inc` source builds the library
  and the shaders (see [backend limits](docs/backends_limits.md) for
  per-feature status)

**4. No hidden behaviour**
- **Raw math by default**: conversions and simulations never silently clamp
  or destroy out-of-gamut information. Explicit `_gamut_safe(..., method)`
  and `_unclamped` variants put the clamping behaviour in the signature
  (see [gamut mapping](docs/gamut_mapping.md))
- **Normalized ranges by default** (`ALWAN_NORMALIZE_RANGES=1`): bounded
  channels arrive and leave in [0, 1] unless you opt out
- Errors through one `alwan_status` contract; strides are explicit bytes

**5. Native dual precision + byte-exact bulk paths**
- Every function exists as `_f32` **and** `_f64`; pick precision at the
  call site. Most families are compiled natively per precision (a short
  list of iterative solvers shares an f64 core, see [Precision](#precision))
- **Scalar == SIMD == planar == typed, byte-exact**: the bulk `map` kernels
  are guaranteed byte-identical to the scalar path, in both fast and
  deterministic builds
- SIMD backends for SSE2 / AVX / AVX2 / NEON, plus a scalar fallback;
  typed pipeline for u8 / u16 / f16 / f32 / f64 buffers

**6. Zero dependencies, zero runtime I/O, wide coverage**
- Pure C11, single library target; all data (CMFs, illuminants, LUTs,
  coefficient tables) embedded at compile time, so nothing is loaded from
  disk at runtime
- One coherent API for what usually takes several libraries: ACES 1.x *and*
  2.0, camera logs for 13 vendors, the AgX family plus a parameterized
  analytic engine and JP2499, 12 colour appearance models, spectral
  upsampling, CVD simulation, 11 gamut-mapping entry points (including an
  HDR ICtCp mapper and 18 spatial picture-formation methods), the DeltaE
  family, LUT baking and CLF interop
- Six host targets verified in CI (Linux/macOS/Windows x x64/ARM)

---

## Install & Vendoring

Alwan builds to a single library target (`alwan`, static by default) plus the public headers
under `src/alwan/`. There is no amalgamated single-file distribution; you
link the library and include `alwan.h`.

**CMake `add_subdirectory` (vendored / git submodule):**
```cmake
# git submodule add https://github.com/soufianekhiat/alwan.git extern/alwan
add_subdirectory(extern/alwan)
target_link_libraries(my_app PRIVATE Alwan::alwan)
```

**CMake `FetchContent`:**
```cmake
include(FetchContent)
FetchContent_Declare(
  alwan
  GIT_REPOSITORY https://github.com/soufianekhiat/alwan.git
  GIT_TAG        v2.0.0)
FetchContent_MakeAvailable(alwan)
target_link_libraries(my_app PRIVATE Alwan::alwan)
```

**Installed package (`find_package`):** the install rules export an
`Alwan::alwan` target and an `AlwanConfig.cmake`:
```cmake
find_package(Alwan REQUIRED)
target_link_libraries(my_app PRIVATE Alwan::alwan)
```

The `Alwan::alwan` target carries its include directory, so `#include "alwan.h"`
works without extra `target_include_directories`. CMake build options
(`ALWAN_BUILD_PRECISION`, `ALWAN_DETERMINISTIC`, `ALWAN_SIMD_ARCH`, ...) are
described under [Configuration](#configuration).

---

## Quick Start

**Prerequisites (Sharpmake/MSVC):** Visual Studio 2022, .NET 6.0+ SDK, Git

Or

**Prerequisites (CMake):** CMake 3.20+, any C11 compiler, Git

1. **Clone:**
   ```sh
   git clone --recursive https://github.com/soufianekhiat/alwan.git
   cd alwan
   ```

2. **Build with CMake (all platforms):**
   ```sh
   cmake -S . -B build
   cmake --build build
   ```

   **Or bootstrap the Sharpmake / Visual Studio workflow** (generates
   `Alwan_vs2022_Win64.sln`; needs the .NET 6+ SDK):
   ```sh
   buildsystem/bootstrap.sh      # POSIX shells (also Git Bash on Windows)
   ```
   ```batch
   buildsystem\bootstrap.bat     # Windows cmd
   ```
   ```batch
   msbuild Alwan_vs2022_Win64.sln /p:Configuration=Release /p:Platform=x64
   ```

   Tests, benchmarks, and image-gen tooling live in the sibling
   [alwan_dev](https://github.com/soufianekhiat/alwan_dev) repo.
   This repo only ships the library + headers; clone alwan_dev next
   to it for the test runner.

3. **Start coding:**
   ```c
   #include "alwan.h"

   /* Stateless conversions don't need a context; call directly. */
   alwan_xyz_f64 const d65 = {0.95047, 1.0, 1.08883};
   alwan_xyz_f64 xyz = {0.41246, 0.21267, 0.01933};   /* Pure red in XYZ */
   alwan_lab_f64 lab;

   alwan_xyz_to_lab_f64(&lab, &xyz, &d65);
   /* Default build (ALWAN_NORMALIZE_RANGES=1) rescales the bounded L channel
      to [0,1]:  lab.L ~= 0.5324,  lab.a ~= 80.09,  lab.b ~= 67.20
      (a and b are unbounded opponent axes and are not rescaled).
      With ALWAN_NORMALIZE_RANGES=0, L is native CIE [0,100] ~= 53.24.
      See docs/ranges.md. */
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
- CAT02, CAT16 (CIECAM models), CAT02 Brill 2008
- Sharp, XYZ scaling, Fairchild
- CMCCAT97, CMCCAT2000, Bianco 2010 (+ PC variant)
- Zhai & Luo 2018 two-step CAT
- Custom transform matrices

### Transfer Functions & HDR
Support for modern display and camera encoding:
- **SDR:** sRGB, BT.709, BT.1886, gamma 2.2 / 2.4 / 2.6 / 2.8
- **HDR:** ST.2084 (PQ), HLG, BT.2390 EETF, BT.2446 A/B/C
- **Camera logs:** ARRI LogC3/LogC4, Sony S-Log/2/3, Canon C-Log/2/3,
  Panasonic V-Log, Nikon N-Log, RED REDLog/REDLogFilm/Log3G10,
  Fujifilm F-Log/F-Log2, DJI D-Log, Blackmagic Film Gen4/5, Leica
  L-Log, FilmLight T-Log, Olympus OM-Log400 (E-Log), Apple Log,
  GoPro Protune
- **Film:** Cineon, ADX10/16
- **View transforms:** AgX (original/punchy/golden/SB2383/Blender),
  Tony McMapface, Khronos PBR Neutral, Reinhard (extended + calibrated),
  Uchimura, Lottes, exposure-with-shoulder
- **Parameterized DRT:** JP2499 (Juan Pablo Zambrano's "2499" analytic
  display transform; `alwan_jp2499_params_f32/f64` expose hue-flight,
  chroma-attenuation, purity and peak luminance)

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
  ZCAM, Hunt, LLAB, ATD95, RLAB, Nayatani 95, CAM18sl, CAM20u (most
  have forward + inverse + UCS variants)
- **Colour differences:** DeltaE76, DeltaE94, DeltaE00, CMC, hyAB, DeltaE_OK,
  DeltaE_DIN99, DeltaE_ITP, DeltaE_CAM02 (LCD/SCD/UCS), DeltaE_CAM16 (LCD/SCD/UCS),
  DeltaE_ZCAM
- **Light quality:** CRI, CQS, SSI, TM-30 Rf
- **CCT estimation:** McCamy, Robertson, Hernandez-Andres, Kang,
  plus Deltauv refinement (`alwan_cct_duv_optimize`, Ohno-style)
- **Vision:** Machado 2009 + Brettel 1997 CVD simulation
  (protan/deutan/tritan, full anomaly + dichromacy variants),
  CSF (Barten 1999)

### Spectral & Gamut Tools
Low-level colour science operations:
- SPD integration to tristimulus values (trapezoid + Simpson)
- CMFs: CIE 1931/1964/2012/2015, Stockman & Sharpe, Wright & Guild
- Standard illuminants: A, D-series, E, F-series, HP discharge
- RGB->spectrum upsampling: Smits 1999, Mallett 2019, Jakob & Hanika 2019
- Hero wavelength sampling for spectral renderers
- Gamut mapping (8 core algorithms + HDR ICtCp/JzCzHz mappers),
  matrix-determinant volume estimation, coverage analysis
- Rayleigh scattering model (Bodhaine 1999) for atmospheric work

### Picture Formation (spatial)
Spatial view transforms that turn open-domain scene light into a formed
picture, built for depth and form cognition. Developed in correspondence
with Troy Sobotka (creator of AgX):
- `alwan_gamut_map_spatial`: 18 formation methods, from the `GRADIENT`
  baseline to `CHANNEL` and the `COMPLETE` operators (the first to
  satisfy all 15 numerically testable formation constraints at once)
- The full constraint matrix ships as a regression test in alwan_dev
- `src/alwan/experimental/`: evidence-driven research operators
  (junction evidence, emission-gated purity, monotone global and
  evidence-gated local exposure) with structural tests; see the note
  under [Project Structure](#project-structure)

See [docs/picture_formation.md](docs/picture_formation.md).

**Out of scope:** plotting, GUI, image codec I/O (JPEG/PNG/TIFF
decoding), threading helpers. Use a dedicated library for those.

---

## Design Philosophy

### Pure C, Zero Dependencies
Built entirely in C11 with no external libraries. The two vendored submodules are optional tooling rather than library dependencies: Sharpmake (project generator) and Imath (used only by the sibling alwan_dev image tooling). Deploy a single library plus the public headers; no package manager is required.

### Deterministic & Re-entrant
No global mutable state. All state lives in explicit context objects
(`alwan_ctx`), so the same input gives the same output on the same
machine. For **byte-identical output across platforms** (compilers,
optimisation levels, ISA), opt in to
[`ALWAN_DETERMINISTIC=ON`](docs/determinism.md).

### You Control Memory
Override allocation with custom `ALWAN_ALLOC`/`ALWAN_FREE` macros, for game engines, embedded systems, or any custom allocator.

### Precision Where You Need It
`float` (7 digits, faster) and `double` (16 digits) variants ship in a
default build with explicit `_f32` / `_f64` suffixes; pick precision at
the call site. Native dual precision covers the allocator, the cube/LUT
layer, the image `view`, the CAM family, RLAB, SPD integration, and
spectral upsampling. A handful of iterative `_f32` solvers (ZCAM /
ACES 1.x inverses, the Cheung 2004 / Finlayson 2015 CCM fits, the gamut
volume/coverage helpers) run f64 internally (`ALWAN_WITH_F64_FACADE`,
always available even in an f32-only build). You can also compile a
single-precision-only library; see [Precision](#precision) below.
Test tolerances automatically adapt to the variant under test. Favour
`double` for reference and agreement with colour-science.

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

By default the library compiles both `_f32` and `_f64` function variants
into one binary; pick precision at the call site with the appropriate
suffix. To shrink code and embedded-data size you can build a single
precision instead: define **at most one** of these before any alwan
header (they resolve to the internal `ALWAN_WITH_F32` / `ALWAN_WITH_F64`
gates in `alwan_build_config.h`):

| User macro              | Result                                   |
|-------------------------|------------------------------------------|
| *(neither, the default)* | both precisions (`ALWAN_WITH_BOTH`)     |
| `ALWAN_BUILD_ONLY_F32`  | `_f32` API + data only                   |
| `ALWAN_BUILD_ONLY_F64`  | `_f64` API + data only                   |

Defining both is a compile `#error`. In a single-precision build the
declarations for the other precision still exist, so calling an
excluded-precision symbol fails at **link** time, not compile time. A few
`_f32` entry points (ZCAM/ACES 1.x iterative inverses, Cheung2004 /
Finlayson2015 CCM fits, the gamut volume/coverage helpers) run f64
internally via `ALWAN_WITH_F64_FACADE` and stay available even in an
f32-only build.

The CMake equivalent is `-DALWAN_BUILD_PRECISION=both|f32|f64` (default
`both`). The Sharpmake solution always builds both precisions; its
configuration axes are Debug/Release x deterministic (`_Det`) x
static/DLL (`_Dll`). For a single-precision build there, define
`ALWAN_BUILD_ONLY_F32` / `ALWAN_BUILD_ONLY_F64` project-wide. See
[docs/configuration.md](docs/configuration.md) for the full
matrix and binary-size trade-offs.

### Deterministic Mode

```sh
cmake -DALWAN_DETERMINISTIC=ON ...
```

Opt in to **byte-identical output across compilers, optimization
levels, and CPU architectures** (Linux/macOS/Windows x x86_64/aarch64).
The library swaps libm `pow / exp / log / cbrt` for in-house
polynomial implementations, disables FMA contraction, and routes
SIMD reductions through a canonical scalar fallback. Both build
systems also expose it as ready-made configurations: CMake
`Debug_Det` / `Release_Det`, Sharpmake `*_Det` / `*_Det_Dll`.

The cross-platform CI matrix in
[`alwan_dev/.github/workflows/determinism.yml`](https://github.com/soufianekhiat/alwan_dev/blob/main/.github/workflows/determinism.yml)
diffs a ~407k-line dump (the full public API surface) across six
runners on every commit. Full design notes in
[docs/determinism.md](docs/determinism.md).

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

## Performance

Bulk conversions are SIMD-vectorised and run in the hundreds of
megapixels per second. On a representative AVX2 host (`_f32`, interleaved,
8-wide), the CVD simulation and gamut-map pipelines reach **600-850
Mpix/s** and common pipelines such as sRGB<->XYZ and the Lab/Oklab family
land in the **50-600 Mpix/s** band; the `_f64` (4-wide) paths run at
roughly half that. The picture-formation transforms (AgX, JP2499, ACES
output, full CAM models) use scalar per-pixel workers by design and run
at 1-5 Mpix/s.

The full set (117 benchmarks across `_f32`/`_f64`, interleaved vs
planar, and per-pixel vs typed U8/U16/F16 dispatch) is in
[current_perf.md](current_perf.md). Regenerate it from the `alwan_bench`
target in the sibling [alwan_dev](https://github.com/soufianekhiat/alwan_dev)
repo. (Numbers are relative throughput for regression tracking, not a
cross-library benchmark.)

---

## API Examples

All public functions have explicit `_f32` / `_f64` suffixes. Bulk
APIs follow the convention
`(out, out_stride, in, in_stride, count, [extras]..., [ctx])`
mirroring `memcpy`. See [api-conventions.md](docs/api-conventions.md)
for the complete rule set.

### Context Management

```c
/* Stateless math doesn't need a context. Only allocate one for APIs
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

### Chromatic Adaptation (D65 -> D50, Bradford)

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
/* Derive RGB <-> XYZ matrices for a custom space. */
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

- **Matrix operations:** 3x3 inversion via partial-pivot Gaussian
  elimination; row order preserved for cross-platform stability
- **Transfer functions:** clamped branches to avoid NaN/Inf
  propagation; OETF/EOTF pairs verified bit-stable in
  deterministic mode
- **Spectral integration:** Simpson's rule with trapezoid fallback;
  bandpass correction via Stearns & Stearns
- **Explicit white points:** all conversions take explicit white
  point parameters; there are no hidden defaults
- **Precision-aware testing:** tolerances adapt to build mode:
  ~1e-5 for f32, ~1e-12 for f64, ~1e-3 for the deterministic
  polynomial path (which trades libm last-bit agreement for
  cross-platform reproducibility)

---

## Validation

Every number here is measured, not estimated, by
[`alwan_dev/tools/coverage_report.py`](https://github.com/soufianekhiat/alwan_dev/blob/main/tools/coverage_report.py).
Re-run it against any checkout to reproduce the table.

| What | Measured |
|---|---|
| Test suites | 107, all passing |
| Test cases | 782 |
| Checks executed per run | 75,034 |
| Assertion sites in the tests | 2,202 |
| Reference datasets (colour-science, OCIO, ACES-dev) | 341 |
| Embedded data tables | 721 |
| Exported symbols | 1,473 |
| Internal symbols reached by a test or a public entry point | 177 of 213 (83%) |
| Build configurations exercised | 8 |
| CI platforms | 6 |
| Cores that compile as HLSL under dxc | 32 of 43 |

Two of these deserve the emphasis:

**75,034 checks per run** is what actually executes, not what is written. A
single assertion inside a sweep over a reference grid runs thousands of times,
so counting source lines would undersell the suite by two orders of magnitude.
The count comes from a counter in the test framework and is printed by the
runner at the end of every run.

**The expected values are not ours.** Fixtures are generated by calling
colour-science, OpenColorIO and ACES-dev, never by re-implementing an
algorithm and recording what it happened to produce. A test that passes says
alwan agrees with the reference implementation, which is the only claim worth
making. Where a residual against a reference is irreducible, it is documented
with its cause rather than absorbed into a tolerance.

Beyond agreement, the suite pins behaviour that a value comparison misses: all
six CI platforms must produce byte-identical output over 289,225 of the
408,902 pinned values under `ALWAN_DETERMINISTIC=1` (the remainder are the
trig and log10 families that follow each platform's libm, listed explicitly),
SIMD and scalar paths must agree bit for bit, and the exported `.cube` and
`.clf` files must be byte-identical across every platform.

---

## Testing

The test suite, benchmarks, and reference-data generators live in the
sibling [alwan_dev](https://github.com/soufianekhiat/alwan_dev) repo
to keep this repository's footprint focused on the production library.

```sh
# alwan_dev expects the library repo checked out next to it as ../alwan
git clone --recursive https://github.com/soufianekhiat/alwan.git
git clone --recursive https://github.com/soufianekhiat/alwan_dev.git
cd alwan_dev
cmake -S . -B build     # -DALWAN_DEV_BUILD_IMAGE_GEN=OFF to skip the C++ image tooling
cmake --build build --config Release
./build/tests/Release/alwan_tests   # 107 test suites, single binary
```

(single-config generators put the binary at `build/tests/alwan_tests`)

**Test Strategy:**
- **Authoritative fixtures:** reference values computed from Python's
  [colour-science](https://github.com/colour-science/colour) library
- **Coverage:** canonical cases, edge cases, and sweeps for each
  module: 107 suites, 782 cases, 75,034 checks executed per run
  (see [Validation](#validation))
- **Precision-aware validation:** error thresholds adapt to build
  configuration (1e-12 for f64, 1e-5 for f32; looser in deterministic
  mode where polynomial approximation replaces libm)
- **SIMD-vs-scalar parity:** 53 bit-exact assertions in
  `88_simd_parity` confirm every public conversion produces
  byte-identical output through both code paths in deterministic mode
- **Cross-platform regression:** `det_run_regression` dumps ~407k
  hex-encoded f64/f32 values per platform (the full public API
  surface); the CI matrix diffs all six runners against each other

**Data Generation:**
Reference data and test fixtures are generated by Python scripts in
`alwan_dev/gendata/` and `alwan_dev/tools/`. Only test *inputs* are
hardcoded; expected *outputs* come from colour-science.

---

## Project Structure

```
alwan/                       # this repo (library only)
+-- src/alwan/               # Library source (pure C11)
|   +-- alwan.h              # Main public API
|   +-- alwan_config.h       # Compile-time configuration
|   +-- alwan_math.h         # Math routing (libm or det polynomials)
|   +-- api/                 # API wrapper implementations
|   +-- core/                # Header-only GPU-compatible core
|   +-- experimental/        # Research-tier picture-formation operators (see note)
|   +-- map/                 # SIMD-accelerated bulk kernels
|   +-- simd/                # SIMD backend (SSE2 / AVX / AVX2 / NEON / scalar)
|   \-- data/                # Embedded reference data (CSV)
+-- buildsystem/sharpmake/   # Sharpmake project definitions
+-- docs/                    # User-facing documentation
\-- CMakeLists.txt           # CMake build (alternative to Sharpmake)

alwan_dev/                   # sibling repo (tests, benches, tools)
+-- tests/                   # 107 test suites + reference fixtures
+-- bench/                   # micro-benchmarks
+-- det_regression/          # cross-platform determinism regression tool
+-- image_gen/               # validation visuals
+-- gendata/                 # Python reference-data generators
\-- tools/                   # API survey, lint, no-raw-libm checks
```

> **Note on `src/alwan/experimental/`**: this tier relaxes the
> library-wide guarantees. The operators there are **not covered by the
> deterministic mode** (no cross-platform byte-identity), use **inlined
> constants** rather than gendata-derived tables, and are outside the
> constraint-tested API surface. The declarations in `alwan.h` are stable;
> the internals are research code and may change. See
> [docs/picture_formation.md](docs/picture_formation.md).

---

## Build System

Two build routes, one source of truth:

- **CMake**: the portable route; what CI builds on all six host targets
  (Linux/macOS/Windows x x64/ARM). No extra tooling beyond a C11 compiler.
- **[Sharpmake](https://github.com/ubisoft/Sharpmake)**: the reference
  project generator (Visual Studio 2022 solutions), optional and mostly a
  Windows-development convenience. Sharpmake itself runs anywhere .NET 6+
  runs; both script flavours are provided.

Sharpmake perks:
- **No vcpkg/Conan/etc:** Single submodule dependency
- **Fast incremental builds:** Only regenerate when scripts change
- **Multi-configuration:** Debug/Release x deterministic x static/DLL in one solution

Regenerate projects after modifying `.cs` files:
```sh
buildsystem/generate_projects.sh    # POSIX shells (also Git Bash on Windows)
```
```batch
buildsystem\generate_projects.bat   # Windows cmd
```

---

## Current Status

**Foundation Complete**

- [x] Context management with custom allocators
- [x] Matrix operations (3x3 multiply, inverse, derived RGB<->XYZ)
- [x] Dual precision (f32 + f64 in one binary)
- [x] Data embedding with diagnostic guards
- [x] Sharpmake + CMake build systems
- [x] Unified test suite (107 suites, hosted in alwan_dev)
- [x] 104 named RGB spaces, easy to add more via space descriptors
- [x] Colour appearance models: CIECAM02, CAM16, ZCAM,
  Hellwig 2022, Kim 2009, Hunt, LLAB, ATD95, RLAB, Nayatani 95,
  CAM18sl, CAM20u
- [x] Modern colour spaces: Oklab/Oklch, JzAzBz/JzCzHz, ICtCp,
  IPT, IgPgTg, ICaCb, ProLab, OSA-UCS, Hunter Lab, DIN99
- [x] DeltaE metrics: DeltaE76 / DeltaE94 / DeltaE00 / CMC / hyAB / OK / DIN99 /
  ITP / CAM02-LCD/SCD/UCS / CAM16-LCD/SCD/UCS / ZCAM
- [x] Light-quality metrics: CRI, CQS, SSI, TM-30 Rf
- [x] Spectral operations and gamut tools (8 core mapping algorithms
  + HDR ICtCp/JzCzHz + spatial)
- [x] ACES 1.x RRT+ODT pipeline: 12 output presets, validated against OCIO
- [x] ACES 2.0 Output Transform: 12 output presets, JMh gamut mapping
- [x] 43 transfer functions including all major camera log formats
- [x] 18 view transforms (AgX original/punchy/golden/SB2383/Blender,
  BT.2446 A/B/C, BT.2390, Tony McMapface, Reinhard, Khronos PBR Neutral,
  Uchimura, Lottes, Exposure, ACES Rec.709) + JP2499 parameterized DRT
- [x] Spatial picture formation: 18 methods incl. the 15/15-constraint
  `COMPLETE` operators ([docs/picture_formation.md](docs/picture_formation.md))
- [x] Spectrum upsampling: Smits 1999, Mallett 2019, Jakob 2019
- [x] Vision: Machado CVD simulation (6 types), CSF (default + Barten)
- [x] Cross-platform CI on six host targets (Linux/macOS/Windows x x64/ARM)
- [x] Opt-in deterministic mode with cross-platform bit-exact CI matrix
  pinning the full public API surface (~407k-line dump per platform)
  ([docs/determinism.md](docs/determinism.md))

---

## Documentation

Documentation lives in the [docs/](docs/) folder. [alwan.h](src/alwan/alwan.h) is the authoritative API reference.

### Guides

- **[Getting Started](docs/getting-started.md)**: installation, first program, basic concepts, and common workflows
  - Building and running tests
  - Your first color conversion
  - Understanding contexts and white points
  - Common pitfalls and debugging tips

- **[Configuration](docs/configuration.md)**: compile-time options for precision, data embedding, and custom allocators
  - Scalar precision (float vs double)
  - Data embedding mode (embedded mode only; runtime loading planned)
  - Custom memory allocators
  - Platform-specific settings


- **[CPU Backends](docs/backends-cpu.md)**: SIMD ISA selection (SSE2/AVX/AVX2/NEON + scalar), SVML gating, and the fast-vs-deterministic math paths

- **[Examples](docs/examples.md)**: 12 practical examples
  - Typed single-pixel and planar conversions
  - Image conversion with strides and premultiplied alpha
  - LUT baking and `.cube` / CLF export
  - Half-float and integer-normalization / video workflows

### API Reference

Function documentation with signatures, parameters, and usage patterns:

- **[Context Management](docs/api/context.md)**: library initialization, memory management, lifecycle
- **[Color Spaces](docs/api/color-spaces.md)**: CIE XYZ/Lab/Luv, RGB conversions, perceptual models (Oklab, JzAzBz, ICtCp)
- **[Chromatic Adaptation](docs/api/chromatic-adaptation.md)**: white point transforms (Bradford, CAT02, CAT16)
- **[Transfer Functions](docs/api/transfer-functions.md)**: EOTFs/OETFs for SDR/HDR (sRGB, PQ, HLG, log curves)
- **[Matrix Operations](docs/api/matrix-operations.md)**: 3x3 matrix math for linear transforms
- **[Spectral Operations](docs/api/spectral.md)**: SPD integration, CMFs, illuminants
- **[Color Appearance](docs/api/color-appearance.md)**: CIECAM02, CAM16, LLAB, Hellwig2022, Kim2009, ATD95
- **[Color Difference](docs/api/color-difference.md)**: DeltaE metrics (DeltaE76, DeltaE94, DeltaE00, CMC, CAM02/16-LCD/SCD)
- **[Gamut Operations](docs/api/gamut.md)**: gamut mapping and analysis

### Technical Details

- **[Precision & Limits](docs/precision-and-limits.md)**: numerical accuracy, error bounds, and performance trade-offs
  - Float vs double accuracy comparison
  - Precision-aware test tolerances
  - Edge case handling (black, achromatic, out-of-gamut)
  - Accumulation error analysis

- **[Data Management](docs/data-management.md)**: how reference data (CMFs, illuminants, RGB spaces) is handled
  - Embedded mode (compiled-in data), the current implementation
  - Data format and structure
  - Memory usage and performance

- **[Determinism](docs/determinism.md)**: opt-in `ALWAN_DETERMINISTIC=ON`
  bit-exact reproducibility across compilers, optimisation levels,
  and CPU architectures
  - Where last-bit divergence comes from (libm, FMA, SIMD reductions)
  - ULP-distance testing
  - Performance trade-off (~5-20%)
  - Cross-platform CI matrix pinning the full public API surface (~407k-line dump)

---

## Contributing

Development is incentivized through Patreon:

[![Patreon](https://img.shields.io/badge/Patreon-Become%20a%20Patron-f96854?style=for-the-badge&logo=patreon)](https://www.patreon.com/SoufianeKHIAT)

---

## Roadmap to 3.0.0

Planned directions for the next major version. Open for PRs;
contributions and design discussions are welcome.

- **Smaller binaries**: opt in/out of data tables and feature families at
  compile time, so a build carries only the spaces, CMFs, and transforms it
  actually uses
- **On-demand data loading**: `ALWAN_EMBED_DATA=0`, loading reference data
  at runtime instead of embedding it (the `runtime_data_root` field in
  `alwan_config` is reserved for this)
- **Deterministic trig / log10**: extend `ALWAN_DETERMINISTIC` beyond
  `pow/exp/log/log2/cbrt` so hue-angle conversions, CAM hue correlates, and
  the log10 camera logs join the cross-platform byte-identity contract
- **Native-f32 iterative solvers**: retire the remaining
  `ALWAN_WITH_F64_FACADE` entry points where a native single-precision core
  makes sense
- **Picture-formation graduation**: promote the research operators from
  `experimental/` into the deterministic, gendata-backed main surface
- **Real SIMD work**: current vectorization is minimal (generic lane
  loops, and the view-transform / CAM / picture-formation kernels are
  scalar per-pixel workers). Hand-tuned AVX2/NEON paths for the hot
  pipelines are open ground; see [current_perf.md](current_perf.md) for
  where the time goes
- **Iteration cores belong in the cross-backend layer**: when an operation
  iterates (Newton solvers, gamut boundary search, chromatic adaptation
  fixed points), put the body of the loop in the backend-neutral core and
  leave only the loop itself to the caller. The step then compiles for
  HLSL, GLSL, C and Halide from one source, and each backend drives it with
  the control flow it can express: a bounded `for` on GPU, `while` with an
  early exit on CPU. A solver written as one monolithic loop inside a `.c`
  is effectively CPU-only, and porting it later means transcribing the maths
  a second time and keeping two copies in agreement

---

## License

MIT License; see [LICENSE](LICENSE) for details.

