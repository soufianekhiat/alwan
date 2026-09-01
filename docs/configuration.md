# Configuration and build

Everything you can set at build time, what it does, and which combinations are
compiled and tested.

Sources of truth, in the order the preprocessor reaches them:

- `src/alwan/alwan_build_config.h` (precision resolution)
- `src/alwan/alwan_config.h` (data embedding, allocator and memcpy hooks)
- `src/alwan/alwan_platform.h` (backend detection, range normalisation)
- `CMakeLists.txt` and `buildsystem/sharpmake/src/` (the build systems)

This page explains that surface. It does not replace reading those files.

---

## The build matrix

Four independent axes. Nothing couples them except the precision guard on
`ALWAN_SCALAR_IS_FLOAT` described below.

| Axis | Values | CMake | Sharpmake |
|---|---|---|---|
| Optimisation | Debug, Release | `-DCMAKE_BUILD_TYPE=` | in the configuration name |
| Linkage | static, shared | `-DBUILD_SHARED_LIBS=ON` | `_Dll` suffix |
| Math | fast, deterministic | `-DALWAN_DETERMINISTIC=ON` | `_Det` suffix |
| Precision | both, f32, f64 | `-DALWAN_BUILD_PRECISION=` | project `Defines` |

Sharpmake, the reference build, emits the eight configurations that the first
three axes produce, all dual precision:

```
Debug              Release
Debug_Dll          Release_Dll
Debug_Det          Release_Det
Debug_Det_Dll      Release_Det_Dll
```

The full suite runs in all eight. CMake covers the same eight in the
permutation CI job (`build_type` x `deterministic` x `shared`), and the
precision job covers each single-precision selection in **both linkages**, with
`-Wl,--no-undefined` on the shared rows.

That last detail is not decoration. A static archive never resolves a symbol
nothing references, and an ELF shared object permits undefined symbols by
default, so a single-precision build that reaches into the excluded precision
links quietly on Linux and fails only when someone builds a DLL. The whole
matrix was verified by hand across both build systems, MSVC and gcc, before
2.0.0, and the linkage axis is in CI so it stays verified.

---

## Precision

Every numeric entry point exists in two precisions: a `float` variant (`_f32`,
`alwan_f32`) and a `double` variant (`_f64`, `alwan_f64`), each with its own
embedded data tables. A default build compiles **both**. Compiling one shrinks
the code and the embedded data. The alias `alwan_scalar` names whichever
precision the build treats as default.

Define **at most one** of these before including any alwan header, or on the
command line:

| User macro | Effect |
|---|---|
| *(none)* | build **both** precisions (the default) |
| `ALWAN_BUILD_ONLY_F32` | build only the `float` API and data |
| `ALWAN_BUILD_ONLY_F64` | build only the `double` API and data |

`alwan_build_config.h` resolves those into the gates the library tests:

| Gate | Meaning |
|---|---|
| `ALWAN_WITH_F32` | `1` when the f32 API and data are compiled |
| `ALWAN_WITH_F64` | `1` when the f64 API and data are compiled |
| `ALWAN_WITH_BOTH` | `1` only when both are |
| `ALWAN_WITH_F64_FACADE` | always `1`; see the facade section |

Defining both `ALWAN_BUILD_ONLY_*` is a hard `#error`.

### Declarations always exist, definitions are gated

A single-precision build gates **definitions and data**, never
**declarations**. The public declarations and the `alwan_*_f32` / `alwan_*_f64`
struct typedefs are present in every build, because they cost nothing; only the
function definitions and the embedded-data twins are wrapped in
`#if ALWAN_WITH_F32` / `#if ALWAN_WITH_F64`.

So calling an excluded precision **compiles** and fails at **link** time. Build
`ALWAN_BUILD_ONLY_F32`, call `alwan_xyz_to_lab_f64`, and the linker reports it.

### `ALWAN_WITH_F64_FACADE`, always 1

A few `_f32` entry points run their algorithm in `double` internally and narrow
the result. They stay available in an f32-only build, so their machinery is
gated by `ALWAN_WITH_F64_FACADE` (always `1`) rather than by `ALWAN_WITH_F64`.

This is a design choice, not a gap. Each one is an iterative solver, a
wavelength integration or a least-squares fit whose accuracy and run-to-run
repeatability depend on a single f64 core. Reusing that core from the f32 entry
point keeps the result stable instead of maintaining a second, less accurate
copy of the same algorithm. They are not to be "fixed" to native f32.

The gate covers the whole chain, not just the entry point. A facade that widens
to f64 needs the f64 code and tables it calls, so the f64 halves of the
spectral, colorspace, CAT, CAM, LUT, Oklab and reference-data modules are
compiled in every build. See the size table below for what that costs.

| Area | `_f32` entry points | Why it stays f64 internally |
|---|---|---|
| ZCAM forward and inverse | `alwan_zcam_*_f32`, `alwan_delta_e_zcam_f32` | iterative inverse whose convergence threshold is below f32 epsilon |
| ACES 1.x iterative inverses | `alwan_aces1_output_transform_inv_f32` | same: convergence tighter than f32 can represent |
| CCM least-squares fits | `alwan_colour_correction_matrix_cheung2004_f32`, `..._finlayson2015_f32` | the normal-equations solve squares the condition number |
| Gamut volume, ratio, coverage | `alwan_gamut_volume_f32`, `alwan_gamut_volume_ratio_f32`, `alwan_gamut_coverage_f32` | computed in f64 for stability; volume is an exact `\|det(M)\|`, the others reduce over it |
| Spectral quality metrics | `alwan_cri_ra_f32`, `alwan_cqs_calculate_f32`, `alwan_tm30_rf_f32`, `alwan_cie224_rf_f32`, `alwan_ssi_calculate_f32`, `alwan_metamerism_index_f32` | integrate f64 CMF tables with an f64 integrator |
| Kang CCT solver | `alwan_cct_kang_xy_f32` | Newton-Raphson with sub-f32-epsilon tolerances |

Not facades, native f32: the other CCT estimators (McCamy, Robertson,
Hernandez-Andres) compute in float, and the SPD and spectral-upsampling layer is
instantiated per precision with native f32 tables.

### `ALWAN_SCALAR_IS_FLOAT`

Selects what `alwan_scalar` names: `double` by default, `float` when set to `1`.
It drives the convenience value types (`alwan_rgb`, `alwan_xyz`, `alwan_lab`)
and the GPU backends; the explicit `_f32` / `_f64` entry points ignore it.
`alwan_build_config.h` keeps it consistent with the precision selection so the
alias always names a precision that exists:

| Build | `ALWAN_SCALAR_IS_FLOAT` | Guard |
|---|---|---|
| both *(default)* | unchanged, `double` unless you set `1` | none |
| `ALWAN_BUILD_ONLY_F32` | forced to `1` if undefined | `=0` is a `#error` |
| `ALWAN_BUILD_ONLY_F64` | left at `double` | `=1` is a `#error` |

On the C backend `alwan_platform.h` always typedefs its internal math scalar to
`double` regardless; this macro governs the public default-precision aliases.
GPU backends typedef `alwan_scalar` to `float` or `Halide::Expr`, see
[api/backends.md](api/backends.md).

### Size trade-off

Measured, static library, MSVC Release:

| Build | Size | vs dual |
|---|---|---|
| both *(default)* | 69.3 MB | |
| f32 only | 67.1 MB | -3% |
| f64 only | 45.8 MB | -34% |

The asymmetry is the facades. A single-precision build drops the other
precision's function bodies and data twins, which is where a saving would come
from, but it cannot drop what the f64-internal facades need: an `_f32` entry
point that computes in double pulls in the f64 spectral, colorspace, CAT, CAM,
LUT and Oklab code together with the f64 reference tables it reads. Since those
facades are f32 entry points, the obligation runs one way only. An f64-only
build has no such debt and drops the whole f32 half.

So choose `f64` for size and `f32` for a float-only surface, not for footprint.
If the f32 saving matters to you, the lever is the facade list rather than the
build flag: each entry moved to a native f32 implementation returns its share.
The large tables (the Jakob 2019 spectral upsampling LUTs above all) dominate
what remains.

---

## Data embedding

`ALWAN_EMBED_DATA`, in `alwan_config.h`, default `1`.

Embedded mode compiles the reference CSV data into the library. It is the only
mode that works today: runtime loading is not implemented, building with
`ALWAN_EMBED_DATA=0` fails, and `alwan_config.runtime_data_root` is reserved and
ignored. See [data-management.md](data-management.md).

---

## Allocator and memcpy hooks

```c
#define ALWAN_ALLOC(sz, align) my_alloc((sz), (align))
#define ALWAN_FREE(p)          my_free((p))
#define ALWAN_REALLOC(p, old_sz, new_sz, align) my_realloc((p), (old_sz), (new_sz), (align))
#define ALWAN_MEMCPY(dst, src, sz) my_memcpy((dst), (src), (sz))
```

All four are in `alwan_config.h` and default to the obvious implementations.
`ALWAN_MEMCPY` is used for type punning between layout-compatible colour types.
Allocation can also be routed per context through `alwan_config` at runtime,
below.

---

## `ALWAN_NORMALIZE_RANGES`

In `alwan_platform.h`, default `1`. Bounded API channels are normalised to
`[0, 1]`; unbounded opponent axes (Lab `a`,`b`, Luv `u`,`v`, chroma `C`, Oklab
`a`,`b`) keep their native signed range; the core `_v` math is unaffected.

Set it to `0` to work in raw mathematical ranges. The rescaling is compiled
**into the library**, so this is a whole-program switch: build the library and
your own code with the same value. The alwan_dev tests and benchmarks build with
`0` so results match the native ranges of the references they validate against.
See [ranges.md](ranges.md) for the per-space table.

---

## `ALWAN_DETERMINISTIC`

```sh
cmake -S . -B build -DALWAN_DETERMINISTIC=ON
```

Trades throughput for reproducibility: deterministic transcendentals instead of
platform libm, canonical reduction order, stricter SIMD map kernels. The
contract, and exactly which parts of the surface it covers, are in
[determinism.md](determinism.md).

---

## Runtime configuration

```c
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .alloc_cb = my_alloc,
    .free_cb = my_free,
    .runtime_data_root = NULL,
    .flags = 0
});
```

Allocation hooks are the useful part. `runtime_data_root` is reserved and
ignored; do not expect a build to load files from it. Check `ctx == NULL` as the
allocation-failure gate.

---

## Recipes

```sh
# Default: dual precision, static, fast math
cmake -S . -B build && cmake --build build --config Release

# Single precision, float only
cmake -S . -B build -DALWAN_BUILD_PRECISION=f32

# Deterministic, for golden files and cross-platform byte stability
cmake -S . -B build -DALWAN_DETERMINISTIC=ON

# Shared library
cmake -S . -B build -DBUILD_SHARED_LIBS=ON
```

```c
/* Pick exactly one precision model, before any alwan header. */
#include "alwan.h"                 /* dual precision, the default */

#define ALWAN_BUILD_ONLY_F32 1     /* float only; also forces ALWAN_SCALAR_IS_FLOAT=1 */
#include "alwan.h"

#define ALWAN_BUILD_ONLY_F64 1     /* double only */
#include "alwan.h"
```

For Sharpmake, single precision is a `Defines` entry in `AlwanLib.cs`, since the
configuration names do not carry a precision axis:

```csharp
conf.Defines.Add("ALWAN_BUILD_ONLY_F32=1");
```

---

## When it goes wrong

| Symptom | Cause |
|---|---|
| `#error ... define at most one of ALWAN_BUILD_ONLY_F32 / ...` | both `ONLY_*` macros defined |
| `#error ... conflicts with ALWAN_SCALAR_IS_FLOAT=0` | `ONLY_F32` plus an explicit `ALWAN_SCALAR_IS_FLOAT=0` |
| `#error ... conflicts with ALWAN_SCALAR_IS_FLOAT=1` | `ONLY_F64` plus an explicit `ALWAN_SCALAR_IS_FLOAT=1` |
| Unresolved external `alwan_*_f64` or `_f32` at link | calling an excluded precision in a single-precision build |
| Results differ from the reference libraries by a fixed scale | library and caller built with different `ALWAN_NORMALIZE_RANGES` |

---

## What this page does not control

- It does not choose `_f32` or `_f64` at call sites; the API is explicit. See
  [precision-and-limits.md](precision-and-limits.md).
- It does not enable runtime data loading; nothing does yet.
- It does not replace `alwan.h` for exact signatures.

See also [data-management.md](data-management.md),
[determinism.md](determinism.md), [ranges.md](ranges.md),
[api/backends.md](api/backends.md), [api/context.md](api/context.md).
