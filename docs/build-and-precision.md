# Build And Precision

How Alwan's precision surface is selected at build time: the user macros, the
internal gates they resolve to, the f64-internal facade exception, and how the
CMake and Sharpmake builds map onto them.

The source of truth is `src/alwan/alwan_build_config.h` (precision resolution)
plus `CMakeLists.txt` (the `ALWAN_BUILD_PRECISION` option). This guide explains
that surface; it does not replace reading those files.

---

## The precision model in one paragraph

Every numeric entry point exists in two precisions: a `float` variant (`_f32`,
`alwan_f32`) and a `double` variant (`_f64`, `alwan_f64`), each with its own
embedded data tables. A default build compiles **both**. If you only need one
precision, you can compile a single-precision library to shrink both the
compiled code and the embedded-data footprint. The default-scalar alias
`alwan_scalar` names whichever precision the build treats as default.

---

## User switches → internal gates

Define **at most one** of these *before including any alwan header* (or, more
usually, on the compiler command line / build system):

| User macro | Effect |
|------------|--------|
| *(none)* | Build **both** precisions (the default) |
| `ALWAN_BUILD_ONLY_F32` | Build only the `float` (`_f32`) API + data |
| `ALWAN_BUILD_ONLY_F64` | Build only the `double` (`_f64`) API + data |

`alwan_build_config.h` resolves these into the gates the rest of the library
tests:

| Gate | Meaning |
|------|---------|
| `ALWAN_WITH_F32` | `1` when the f32 API/data is compiled, else `0` |
| `ALWAN_WITH_F64` | `1` when the f64 API/data is compiled, else `0` |
| `ALWAN_WITH_BOTH` | `1` only when both are compiled |

Defining both `ALWAN_BUILD_ONLY_*` macros is a hard error:

```c
#error "alwan: define at most one of ALWAN_BUILD_ONLY_F32 / ALWAN_BUILD_ONLY_F64 ..."
```

---

## Declarations always present, definitions gated → link-time, not compile-time

Single-precision builds gate **definitions and data**, never **declarations**.

- Public *declarations* and the `alwan_*_f32` / `alwan_*_f64` struct typedefs are
  present in **every** build (they cost nothing).
- Only the dual-precision function *definitions* (the `#include "*_impl.inc"`
  pairs) and the `NAME_f32` / `NAME_f64` embedded-data twins are wrapped in
  `#if ALWAN_WITH_F32` / `#if ALWAN_WITH_F64`.

Consequence: calling an excluded-precision symbol **compiles** fine (the
declaration exists) but fails at **link time** with an unresolved symbol rather
than at compile time. If you build `ALWAN_BUILD_ONLY_F32` and call
`alwan_xyz_to_lab_f64`, the linker is what reports the error.

---

## `ALWAN_WITH_F64_FACADE` (always `1`)

A handful of `_f32` public entry points are numerically f64-only and run their
algorithm in `double` internally, then narrow the result. They stay available in
an f32-only build, so the machinery they depend on is gated by
`ALWAN_WITH_F64_FACADE` (always `1`, because at least one precision is always
built) rather than by `ALWAN_WITH_F64`.

These f32 entry points are f64-internal facades. This is a **design choice
rather than a gap**: each one is an iterative solver, a wavelength integration,
or a least-squares fit whose precision and run-to-run repeatability depend on a
single f64 core. Reusing that core from the f32 entry point keeps the numerical
result stable and avoids maintaining a second, lower-precision copy of the same
algorithm. They are **not** to be "fixed" to native f32.

| Area | `_f32` entry points | Why it stays f64 internally |
|------|---------------------|-----------------------------|
| ZCAM forward + inverse | `alwan_zcam_*_f32`, `alwan_delta_e_zcam_f32` | Iterative inverse whose convergence threshold is below f32 epsilon |
| ACES 1.x iterative inverses | `alwan_aces1_output_transform_inv_f32` | Same: convergence tighter than f32 can represent |
| CCM polynomial / least-squares fits | `alwan_colour_correction_matrix_cheung2004_f32`, `..._finlayson2015_f32` | Least-squares normal-equations solve squares the condition number |
| Gamut volume / ratio / coverage | `alwan_gamut_volume_f32`, `alwan_gamut_volume_ratio_f32`, `alwan_gamut_coverage_f32` | Computed in f64 for stability; `volume` is an exact `\|det(M)\|`, ratio/coverage are reductions over it |
| Spectral quality metrics (wavelength integration) | `alwan_cri_ra_f32`, `alwan_cqs_calculate_f32`, `alwan_tm30_rf_f32`, `alwan_cie224_rf_f32`, `alwan_ssi_calculate_f32`, `alwan_metamerism_index_f32` | Integrate over f64 CMF tables with an f64 integrator; an f32 SPD mirror is widened, then narrowed back |
| Kang CCT solver | `alwan_cct_kang_xy_f32` | Newton-Raphson with sub-f32-epsilon tolerances; reuses the f64 solver by design |

> Counterexamples, native f32 rather than facades: the other CCT estimators
> (`alwan_cct_mccamy_xy_f32`, Robertson, Hernandez-Andres) compute natively in
> float, and the SPD / spectral-upsampling layer is templated and instantiated per
> precision with native f32 data tables. Only the entries listed above widen to f64.

In an `ALWAN_BUILD_ONLY_F32` build these still work: their private `double`
helpers use the `double` C type directly and are **not** gated by
`ALWAN_WITH_F64`. (`double` and `float` as C types are, of course, always
available regardless of the precision selection.)

---

## `ALWAN_SCALAR_IS_FLOAT` coupling

`ALWAN_SCALAR_IS_FLOAT` selects the default-precision alias `alwan_scalar`
(`double` by default; `float` when set to `1`). It is mainly relevant to the GPU
backends and the convenience value types (`alwan_rgb`, `alwan_xyz`, `alwan_lab`,
…). `alwan_build_config.h` keeps it consistent with the precision selection so
that `alwan_scalar` always names a precision that is actually compiled in:

| Build | `ALWAN_SCALAR_IS_FLOAT` | Conflict guard |
|-------|------------------------|----------------|
| both *(default)* | unchanged (`double` unless you set `1`) |, |
| `ALWAN_BUILD_ONLY_F32` | forced to `1` if undefined | `ALWAN_SCALAR_IS_FLOAT=0` is a `#error` (f64 scalar not built) |
| `ALWAN_BUILD_ONLY_F64` | left as default `double` | `ALWAN_SCALAR_IS_FLOAT=1` is a `#error` (f32 scalar not built) |

On the **C backend**, `alwan_platform.h` always typedefs `alwan_scalar = double`
for its internal math regardless of these settings; `ALWAN_SCALAR_IS_FLOAT`
governs the default-precision *public* value-type aliases. (GPU backends typedef
`alwan_scalar` to `float` / `Halide::Expr`; see
[`api/backends.md`](api/backends.md).)

---

## Build-system mapping

### CMake (the live precision switch)

CMake exposes the cache option `ALWAN_BUILD_PRECISION` (`both` | `f32` | `f64`,
default `both`), which emits the matching `ALWAN_BUILD_ONLY_*` macro **PUBLIC**
so consumers of the `alwan` target inherit it:

```sh
# Default: both precisions
cmake -S . -B build
cmake --build build

# Single-precision (float-only) library
cmake -S . -B build -DALWAN_BUILD_PRECISION=f32
cmake --build build

# Single-precision (double-only) library
cmake -S . -B build -DALWAN_BUILD_PRECISION=f64
cmake --build build
```

Mapping (`CMakeLists.txt`):

| `ALWAN_BUILD_PRECISION` | Compile definition added (PUBLIC) |
|-------------------------|-----------------------------------|
| `both` | *(none; dual precision)* |
| `f32` | `ALWAN_BUILD_ONLY_F32=1` |
| `f64` | `ALWAN_BUILD_ONLY_F64=1` |

Any other value is a `FATAL_ERROR`. Because the define is PUBLIC, a downstream
target linking `Alwan::alwan` sees the same gate and gets the same link-time
behaviour for excluded-precision calls.

### Sharpmake (the reference build)

Sharpmake is the reference build system; CMake replicates it. The reference
project (`buildsystem/sharpmake/src/`) emits configurations along the axes
Debug/Release × deterministic (`_Det`) × static/DLL (`_Dll`), all
**dual-precision** (it adds `ALWAN_EMBED_DATA=1` and
`ALWAN_NORMALIZE_RANGES=0`, no `ALWAN_BUILD_ONLY_*`). To produce a
single-precision Sharpmake build, add the macro to the project `Defines` in
`AlwanLib.cs` / `common.cs`:

```csharp
conf.Defines.Add("ALWAN_BUILD_ONLY_F32=1"); // or ALWAN_BUILD_ONLY_F64=1
```

> The macro is always the actual precision selector; there is no separate
> precision axis in the configuration names.

---

## Binary-size / data trade-offs

| Build | Code | Embedded data | When to pick it |
|-------|------|---------------|-----------------|
| both *(default)* | f32 **and** f64 instantiations of every kernel | f32 **and** f64 twins of every CSV table (matrices, LUTs, splines, CMFs, …) | You want both precisions selectable at call sites with no recompile |
| `f32` only | f32 instantiations only (+ the f64-facade helpers) | f32 data twins only (+ the f64 data the facades read) | Graphics / real-time host that is `float` end-to-end |
| `f64` only | f64 instantiations only | f64 data twins only | Validation / scientific pipeline that is `double` end-to-end |

Notes:

- A single-precision build removes the *other* precision's function bodies and
  embedded-data twins; that is where the size win comes from.
- It does **not** remove the f64-facade helpers (`ALWAN_WITH_F64_FACADE` stays
  `1`), so an f32-only build still carries the f64 code and data those few entry
  points need.
- Some large embedded-data translation units (e.g. the Jakob 2019 spectral
  upsampling LUTs) dominate the data footprint; dropping a precision halves their
  twin, which is the main reason to choose a single-precision build for size.

---

## Quick reference

```c
/* Pick exactly one model. Define BEFORE any alwan header (or on the cmd line). */

/* 1. Dual precision (default): define nothing. */
#include "alwan.h"

/* 2. Float-only library. */
#define ALWAN_BUILD_ONLY_F32 1   /* also forces ALWAN_SCALAR_IS_FLOAT=1 */
#include "alwan.h"

/* 3. Double-only library. */
#define ALWAN_BUILD_ONLY_F64 1
#include "alwan.h"
```

| Symptom | Cause |
|---------|-------|
| `#error ... define at most one of ALWAN_BUILD_ONLY_F32 / ...` | Both `ONLY_*` macros defined |
| `#error ... conflicts with ALWAN_SCALAR_IS_FLOAT=0` | `ONLY_F32` plus an explicit `ALWAN_SCALAR_IS_FLOAT=0` |
| `#error ... conflicts with ALWAN_SCALAR_IS_FLOAT=1` | `ONLY_F64` plus an explicit `ALWAN_SCALAR_IS_FLOAT=1` |
| Unresolved external `alwan_*_f64` (or `_f32`) at link | Calling an excluded precision in a single-precision build |

---

## Related docs

- [configuration.md](configuration.md) - the full compile-time knob surface
- [precision-and-limits.md](precision-and-limits.md) - choosing `_f32` vs `_f64`
  at call sites
- [determinism.md](determinism.md) - `ALWAN_DETERMINISTIC` and reproducibility
- [data-management.md](data-management.md) - embedded data today, runtime plan
- [api/backends.md](api/backends.md) - GPU backends and `alwan_scalar` typedefs
