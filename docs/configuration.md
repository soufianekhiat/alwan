# Configuration

Compile-time and build-time knobs that affect the public Alwan library.

The main sources of truth are:

- `src/alwan/alwan_config.h`
- `src/alwan/alwan_build_config.h`
- `src/alwan/alwan_platform.h`
- the build-system options used by CMake or Sharpmake/MSVC

For the full treatment of single- vs dual-precision builds (binary-size
trade-offs, the f64-internal facade exceptions, and the Sharpmake/CMake flavor
mapping) see [build-and-precision.md](build-and-precision.md). This page gives
the macro-level summary.

---

## Current Configuration Surface

### Precision build selection

Defined in `alwan_build_config.h`. Alwan exposes every numeric entry point in
two native precisions — a float `_f32` variant and a double `_f64` variant, each
with its own embedded data tables. By default **both** precisions are compiled.
A caller who only needs one can shrink the compiled code and embedded-data
footprint by selecting a single-precision build.

User-facing switches (define **at most one**, before including any alwan
header):

| Macro | Effect |
|-------|--------|
| `ALWAN_BUILD_ONLY_F32` | Build only the single-precision (float) API + data |
| `ALWAN_BUILD_ONLY_F64` | Build only the double-precision (double) API + data |
| *(neither defined)* | Build BOTH precisions — the default |

Defining both is a hard `#error`. These resolve to the internal gates the rest
of the library tests:

| Resolved macro | Meaning |
|----------------|---------|
| `ALWAN_WITH_F32` | `1` when the f32 API/data is compiled, else `0` |
| `ALWAN_WITH_F64` | `1` when the f64 API/data is compiled, else `0` |
| `ALWAN_WITH_BOTH` | `1` only when both are compiled |
| `ALWAN_WITH_F64_FACADE` | Always `1`; gates f64-internal helpers that stay available even in an f32-only build |

Notes:

- Public *declarations* and the `alwan_*_f32` / `alwan_*_f64` struct typedefs
  remain present in every build (they cost nothing). Only the *definitions* and
  embedded *data* twins are gated, so calling an excluded-precision symbol fails
  at **link** time, not compile time.
- A handful of f32 public entry points are numerically f64-internal facades
  (ZCAM inverse, ACES 1.x inverse, Cheung 2004 / Finlayson 2015 CCM fits, gamut
  Monte-Carlo reductions). Their double-precision machinery is gated by
  `ALWAN_WITH_F64_FACADE` (always `1`), so it stays functional in an f32-only
  build.

### `ALWAN_SCALAR_IS_FLOAT`

Selects what the default-precision alias `alwan_scalar` names. On the C backend
`alwan_scalar` is `double` by default; define `ALWAN_SCALAR_IS_FLOAT=1` to make
it `float`. This interacts with the precision selection above:

- `ALWAN_BUILD_ONLY_F32` forces `ALWAN_SCALAR_IS_FLOAT=1` (the f64 scalar is not
  built), and combining it with `ALWAN_SCALAR_IS_FLOAT=0` is a `#error`.
- `ALWAN_BUILD_ONLY_F64` combined with `ALWAN_SCALAR_IS_FLOAT=1` is a `#error`
  (the f32 scalar is not built).

The explicit `_f32` / `_f64` entry points are unaffected — `alwan_scalar` only
drives the default-precision convenience value types and the GPU backends.

### CMake precision option

CMake exposes the same selection as a cache string that maps onto the macros
above:

```sh
cmake -S . -B build -DALWAN_BUILD_PRECISION=f32   # both | f32 | f64 (default both)
```

`f32` emits `ALWAN_BUILD_ONLY_F32=1`, `f64` emits `ALWAN_BUILD_ONLY_F64=1`, and
`both` defines neither (the default dual-precision build).

### `ALWAN_EMBED_DATA`

Defined in `alwan_config.h`.

```c
#define ALWAN_EMBED_DATA 1
```

- `1` is the supported mode today
- `0` is still planned only

Current behavior:

- embedded mode compiles reference CSV data into the library
- runtime / on-demand data loading is not implemented
- building with `ALWAN_EMBED_DATA=0` currently fails
- `alwan_config.runtime_data_root` is reserved and ignored in the current release

### `ALWAN_ALLOC`, `ALWAN_FREE`, `ALWAN_REALLOC`

Defined in `alwan_config.h`.

```c
#define ALWAN_ALLOC(sz, align) alwan_default_alloc((sz), (align))
#define ALWAN_FREE(p)          alwan_default_free((p))
#define ALWAN_REALLOC(p, old_sz, new_sz, align) \
    alwan_default_realloc((p), (old_sz), (new_sz), (align))
```

These hooks let callers integrate Alwan with custom allocators.

### `ALWAN_MEMCPY`

Defined in `alwan_config.h`.

```c
#define ALWAN_MEMCPY(dst, src, sz) memcpy((dst), (src), (sz))
```

Overrideable memory-copy hook used for safe type punning between
layout-compatible color types. Override it before including `alwan.h` if you
need to route copies through a custom implementation.

### `ALWAN_NORMALIZE_RANGES`

Defined in `alwan_platform.h`.

```c
#define ALWAN_NORMALIZE_RANGES 1
```

Default behavior on the C backend:

- bounded API-channel ranges are normalized to `[0, 1]` (the default convention)
- unbounded opponent / chroma axes (Lab `a`,`b`, Luv `u`,`v`, chroma `C`,
  Oklab `a`,`b`) keep their native signed range
- core `_v` style math is not affected by this normalization layer

Set `ALWAN_NORMALIZE_RANGES=0` to work in raw mathematical ranges instead. The
rescaling is compiled **into the library**, so this is a whole-program switch:
build the `alwan` library and your own code with the same value (defining it in
a single consumer file does not change a prebuilt library). The `alwan_dev`
tests and benchmarks build with `=0` so results match the native ranges of the
reference libraries they validate against (colour-science, OCIO, ACES). See
[ranges.md](ranges.md) for the per-space table and the full rationale.

### `ALWAN_DETERMINISTIC`

Usually enabled from the build system, for example:

```sh
cmake -DALWAN_DETERMINISTIC=ON ...
```

This mode trades some performance for cross-platform reproducibility:

- deterministic transcendentals instead of platform `libm` variation
- canonicalized reduction order
- stricter behavior in SIMD-backed map kernels

See [determinism.md](determinism.md) for the exact consequences.

---

## Typical Configurations

### Default shipping build

Recommended when you want the current supported feature set with no external
data deployment:

```c
#define ALWAN_EMBED_DATA 1
```

Use default allocators unless you need engine integration.

### Custom allocator integration

```c
#define ALWAN_ALLOC(sz, align) my_alloc((sz), (align))
#define ALWAN_FREE(p)          my_free((p))
#define ALWAN_REALLOC(p, old_sz, new_sz, align) \
    my_realloc((p), (old_sz), (new_sz), (align))

#include "alwan.h"
```

### Raw-range workflow

If you want public API channels in their mathematical ranges rather than the
normalized public defaults:

```c
#define ALWAN_NORMALIZE_RANGES 0
#include "alwan.h"
```

### Deterministic regression build

```sh
cmake -S . -B build -DALWAN_DETERMINISTIC=ON
cmake --build build
```

Use this when golden files or cross-platform byte stability matter more than
throughput.

---

## `alwan_config`

Runtime configuration currently focuses on allocation hooks. The
`runtime_data_root` field is intentionally reserved for the future runtime-data
mode.

```c
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .alloc_cb = my_alloc,
    .free_cb = my_free,
    .runtime_data_root = NULL,
    .flags = 0
});
```

Notes:

- set `runtime_data_root` only as documentation of intent today
- do not expect current builds to load files from that path
- use `ctx == NULL` checks as your allocation-failure gate

---

## Build Notes

> Selecting **which precisions** the library compiles (`ALWAN_BUILD_ONLY_F32` /
> `ALWAN_BUILD_ONLY_F64` → `ALWAN_WITH_F32` / `ALWAN_WITH_F64`, the CMake
> `ALWAN_BUILD_PRECISION` option, `ALWAN_SCALAR_IS_FLOAT` coupling, and the
> f64-facade exception) is covered in
> [build-and-precision.md](build-and-precision.md).

### Sharpmake / MSVC

The generated solution exposes build flavors such as:

- `Debug_f32`
- `Release_f32`
- `Debug_f64`
- `Release_f64`

These flavors are presets that map onto the source-level precision switches
described above: the `_f32` flavors compile with `ALWAN_BUILD_ONLY_F32`, the
`_f64` flavors with `ALWAN_BUILD_ONLY_F64`. The dual-precision build (neither
macro) remains the default outside these flavors. The library always exposes the
explicit `_f32` and `_f64` entry points in the header regardless of flavor; an
excluded precision simply fails to link.

### CMake

CMake is the simplest path for deterministic builds and cross-platform CI:

```sh
cmake -S . -B build
cmake --build build
```

Add `-DALWAN_DETERMINISTIC=ON` for a deterministic build, or
`-DALWAN_BUILD_PRECISION=f32|f64` to compile a single precision.

---

## What This File Does Not Control

- It does not choose between `_f32` and `_f64` at *call sites*; the public API is
  explicit. (The build *can* exclude one precision via `ALWAN_BUILD_ONLY_*`, but
  the surviving entry points keep their explicit suffixes.)
- It does not enable runtime data loading yet.
- It does not replace the need to read `alwan.h` for exact signatures.

See also:

- [build-and-precision.md](build-and-precision.md)
- [data-management.md](data-management.md)
- [precision-and-limits.md](precision-and-limits.md)
- [docs/api/context.md](api/context.md)
