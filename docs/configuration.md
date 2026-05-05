# Configuration

Compile-time and build-time knobs that affect the public Alwan library.

The main sources of truth are:

- `src/alwan/alwan_config.h`
- `src/alwan/alwan_platform.h`
- the build-system options used by CMake or Sharpmake/MSVC

---

## Current Configuration Surface

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

### `ALWAN_NORMALIZE_RANGES`

Defined in `alwan_platform.h`.

```c
#define ALWAN_NORMALIZE_RANGES 1
```

Default behavior on the C backend:

- bounded API-channel ranges are normalized to `[0, 1]`
- unbounded channels remain unbounded
- core `_v` style math is not affected by this normalization layer

Define `ALWAN_NORMALIZE_RANGES=0` before including `alwan.h` to work in the
raw mathematical ranges instead.

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

### Sharpmake / MSVC

The generated solution exposes build flavors such as:

- `Debug_f32`
- `Release_f32`
- `Debug_f64`
- `Release_f64`

Those names are build presets, not public API switches. The library still
exposes explicit `_f32` and `_f64` entry points in the header.

### CMake

CMake is the simplest path for deterministic builds and cross-platform CI:

```sh
cmake -S . -B build
cmake --build build
```

Add `-DALWAN_DETERMINISTIC=ON` when needed.

---

## What This File Does Not Control

- It does not choose between `_f32` and `_f64` at call sites; the public API is
  explicit.
- It does not enable runtime data loading yet.
- It does not replace the need to read `alwan.h` for exact signatures.

See also:

- [data-management.md](data-management.md)
- [precision-and-limits.md](precision-and-limits.md)
- [docs/api/context.md](api/context.md)
