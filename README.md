# Alwan

**Alwan** is a small, dependency-free colour science math library in pure C (C11), targeting feature parity with the Python [Colour](https://github.com/colour-science/colour) project (math and data only, no visualization).

## Design Goals

- **Pure C11** with stable C ABI - no external dependencies
- **Deterministic & re-entrant** - no global mutable state, all state in explicit context
- **Allocation control** - all heap usage through overridable `ALWAN_ALLOC`/`ALWAN_FREE` macros
- **Compile-time scalar** - configurable as `float` or `double` (default: `double`)
- **Bulk-first API** - transform functions operate over arrays with count/stride/scratch buffers
- **Portable** - no compiler extensions required, optional SIMD paths

## Scope

Alwan focuses on deterministic, high-performance colour math and reference data:

- **Colour models & conversions**: XYZ, xyY, Lab, Luv, LCh, IPT, JzAzBz, RGB families, YCbCr, HSV, HSL, CMY, CMYK
- **Chromatic adaptation**: Bradford, CAT02, CAT16, XYZ scaling
- **Transfer functions**: sRGB, BT.709, BT.1886, ST.2084 (PQ), HLG, log curves (S-Log, C-Log, V-Log)
- **ACES & AgX pipelines**: ACES2065-1 (AP0), ACEScg (AP1), ACESproxy, RRT+ODT, AgX view transforms
- **Colour differences**: ΔE76, ΔE94, CMC, ΔE00
- **Colour appearance**: CIECAM02, CAM16 (forward/inverse, UCS)
- **Spectral computations**: SPD integration to XYZ, CMFs (CIE 1931/1964/2012), illuminants (A, D, E, F)
- **Light quality & CCT**: CRI, CQS, SSI, TM-30, CCT estimators (McCamy, Robertson)
- **Gamut utilities**: Basic gamut mapping, RGB gamut volume/coverage

**Out of scope**: Plotting, visualization, device I/O, image codecs, camera/OCIO pipelines, GUI, internal threading

## Building

### Prerequisites

- Visual Studio 2022 (for Windows)
- .NET 6.0 SDK or later (for building Sharpmake)
- Git

### Quick Start

1. Clone the repository with submodules:
   ```batch
   git clone --recursive https://github.com/yourusername/alwan.git
   cd alwan
   ```

2. Run the bootstrap script:
   ```batch
   bootstrap.bat
   ```

   This will:
   - Build Sharpmake from source
   - Copy Sharpmake artifacts to `tools/sharpmake/`
   - Generate Visual Studio solution files

3. Open the generated solution:
   ```batch
   Alwan_vs2022_win64.sln
   ```

4. Build and run tests from Visual Studio, or use MSBuild:
   ```batch
   msbuild Alwan_vs2022_win64.sln /p:Configuration=Debug_f64
   ```

### Build Configurations

The library supports multiple build configurations combining optimization and scalar precision:

- **Debug_f32** / **Release_f32**: 32-bit float precision (~7 decimal digits, faster)
- **Debug_f64** / **Release_f64**: 64-bit double precision (~16 decimal digits, default)

All configurations are built with:
- C11 standard (`/std:c11`)
- Warning level 4 (`/W4`)
- Warnings as errors (`/WX`)
- Force C compilation (`/TC`)

### Compile-Time Configuration

Configure the library by editing [alwan_config.h](src/alwan/alwan_config.h) or defining macros:

```c
/* Scalar type selection */
#define ALWAN_SCALAR_IS_FLOAT 0  // 0 = double (default), 1 = float

/* Data embedding mode */
#define ALWAN_EMBED_DATA 1       // 1 = embed (default), 0 = runtime load

/* Custom allocation hooks (optional) */
#define ALWAN_ALLOC(sz, align) my_alloc(sz, align)
#define ALWAN_FREE(p)          my_free(p)
```

### Regenerating Projects

If you modify the Sharpmake scripts (`.cs` files in `sharpmake/`), regenerate projects:

```batch
generate_projects.bat
```

## Data Strategy

Alwan ships reference datasets (CMFs, illuminants, RGB primaries) in a single `/data/` folder as **C-parsable CSV files** containing maximum-precision numeric literals:

```csv
2.457001234567890,1.466000000000000,0.960000000000000
```

These files work in both modes:

- **Embedded mode** (`ALWAN_EMBED_DATA=1`, default): Arrays materialized by including CSV directly into initializers, with compiler-specific diagnostic guards to suppress float conversion warnings
- **Runtime mode** (`ALWAN_EMBED_DATA=0`): Same CSV files loaded at runtime, parsed, and cached in `alwan_ctx`

Files are named to encode identity and sampling, e.g., `cie_1931_2deg_xbar_360_830_1nm.csv`.

## API Overview

```c
/* Context creation with optional configuration */
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .alloc_cb = custom_alloc,
    .free_cb = custom_free,
    .runtime_data_root = "path/to/data"
});

/* Math types: alwan_vec3, alwan_mat3x3 */
alwan_mat3x3 M, M_inv;
alwan_mat3_inv(&M, &M_inv);

/* Bulk transforms with stride support */
alwan_xyz_to_lab(xyz_in, wp, lab_out, count, in_stride, out_stride);

/* Chromatic adaptation */
alwan_xyz_adapt(ctx, ALWAN_CAT_BRADFORD, src_wp, dst_wp,
                xyz_in, count, stride, xyz_out, stride);

/* RGB space derivation */
alwan_rgb_derive_matrices(ctx, &rgb_desc, &rgb_to_xyz, &xyz_to_rgb);

/* Transfer functions & view transforms */
alwan_oetf_apply(ctx, "srgb", linear, count, stride, encoded, stride);
alwan_view_transform_apply(ctx, "agx", rgb_in, count, stride, rgb_out, stride);

alwan_destroy(ctx);
```

## Project Structure

```
/data/                   # C-parsable CSV datasets (CMFs, illuminants, RGB primaries)
/src/alwan/              # Library source code (pure C11)
/tests/unit/             # Unit tests (no external test framework)
/sharpmake/              # Sharpmake build scripts (.cs files)
/extern/Sharpmake/       # Sharpmake submodule (only build dependency)
/projects/               # Generated .vcxproj files (git-ignored)
/tmp/                    # Build intermediates (git-ignored)
/tools/sharpmake/        # Sharpmake binaries (generated by bootstrap)
/working_dir/            # Runtime working directory for executables
```

## Numerical Policies

Alwan prioritizes deterministic, numerically stable algorithms:

- **Matrix operations**: 3×3 solve via partial-pivot Gaussian elimination
- **Transfer functions**: Carefully clamped branches for numerical stability
- **Spectral integration**: Simpson's rule (even n) with trapezoid fallback
- **Consistent white-point handling**: All color space conversions use explicit white points
- **Scalar-aware tolerances**: Test assertions adapt to float (`1e-5`) or double (`1e-12`) precision

## Testing

Tests are self-contained C programs with no external test framework dependencies:

- **Fixtures**: Authoritative reference data generated offline by Python Colour, versioned as CSV
- **Coverage**: Canonical cases, edge cases, and sweep tests for each module
- **Validation**: Absolute/relative error thresholds appropriate for scalar type
- **Test runner**: Single executable runs all test suites consecutively with clear reporting

Run tests from Visual Studio or command line:
```batch
working_dir\AlwanTests.exe
```

## Current Status

**Foundation (v0.1-alpha)** - Core infrastructure complete:

- Context management with custom allocators
- 3×3 matrix operations (multiply, inverse with partial-pivot Gaussian elimination)
- Scalar-aware math API (float/double abstraction with `ALWAN_*` macros)
- Data embedding with compiler-portable diagnostic guards
- Sharpmake-driven build system (submodule, no vcpkg)
- Unified test suite with scalar-adaptive tolerances

See [alwan_plan.md](alwan_plan.md) for the complete development roadmap.

## License

MIT License - see [LICENSE](LICENSE) for details

## Documentation

- [alwan_design.md](alwan_design.md) - Detailed design documentation
- [alwan_plan.md](alwan_plan.md) - Development roadmap and milestones
