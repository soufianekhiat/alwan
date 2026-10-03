# The foundation: building a library on alwan

`alwan.h` is the API for applications. `alwan_foundation.h` adds the part of alwan's machinery
that a library built **on** alwan may use: how alwan allocates, how it templates its cores for
both precisions, how a core reads a table on the CPU and in a shader, and the typed pixel
layer. Suwar, the image-processing library released with alwan 3.0.0, is built on it.

The foundation is supported from 3.0.0 under the same compatibility rules as `alwan.h`.
`alwan_internal.h` is not: it stays alwan's own and changes freely. A library built on alwan
includes `alwan.h` and `alwan_foundation.h` and nothing else from alwan.

## Build

Add alwan's `src/alwan` directory to the include path. The foundation names several headers
by computed include (`#include ALWAN_FOUNDATION_CORE_F32_SETUP`), and those resolve against it.
Link the alwan library (static or DLL) as an application would.

## What it holds

| Area | Names | Notes |
|---|---|---|
| Allocation | `alwan_ctx_alloc`, `alwan_ctx_free` (declared in `alwan.h`), `alwan_safe_array_size`, `ALWAN_ALLOC`, `ALWAN_FREE` | Allocate through a caller's context without reading its fields; `NULL` means the default allocator. `alwan_safe_array_size(count, size)` is 0 on overflow. |
| Platform | `ALWAN_INLINE`, `ALWAN_LITERAL`, `ALWAN_SELECT`, `ALWAN_BACKEND`, `ALWAN_DIAG_PUSH` / `_POP` / `_DISABLE_*` | From `alwan_platform.h`, part of `alwan.h`'s closure. |
| Math | the `ALWAN_*` math macros (`ALWAN_POW`, `ALWAN_EXP`, `ALWAN_LOG2`, `ALWAN_SQRT`, `ALWAN_SIN`, ...) | Routed through `alwan_math.h`: the deterministic build swaps in alwan's polynomial math, so a library's own code is deterministic exactly when alwan's is. |
| Core templating | `ALWAN_FOUNDATION_CORE_F32_SETUP`, `ALWAN_FOUNDATION_CORE_F64_SETUP`, `ALWAN_FOUNDATION_CORE_TEARDOWN` | Instantiate a core `.inc` once per precision: `ALWAN_CORE_T`, `ALWAN_CORE_FNVLIT`, `ALWAN_CORE_FNLIT`, `ALWAN_CORE_LITERAL` and the `ALWAN_CORE_*` math. |
| Table readers | `ALWAN_FOUNDATION_TABLE_CORE`, `ALWAN_FOUNDATION_TABLE_READER` | `alwan_table_coord` / `alwan_table_cell` and the accessor seam `ALWAN_TABLE_READ(i)`, so a core reads its table identically on the CPU, in CUDA, OpenCL and HLSL. |
| Half floats | `ALWAN_FOUNDATION_HALF_CORE` | IEEE binary16 conversions, rounded to nearest even. |
| Microfacet terms | `ALWAN_FOUNDATION_MICROFACET_CORE` | GGX D, Smith Lambda and G2 (height-correlated), GGX half-vector sampling; shared with alwan's iridescence. |
| Typed map layer | `#define ALWAN_FOUNDATION_WITH_MAP` before the include | `map/alwan_map_internal.h`: per-pixel typed load and store over `alwan_pixel_format` (`alwan__load3_typed`, `alwan__store3_typed`, `alwan__store1_typed`), `alwan__f16_to_f32` / `alwan__f32_to_f16`, and the tile helpers (`alwan__load_tile_typed_3`, `alwan__store_tile_typed_3`, ...). These keep their `alwan__` spelling. |
| SIMD | `#define ALWAN_FOUNDATION_WITH_SIMD` | `simd/alwan_simd.h`: the AVX, AVX2, SSE2, NEON and scalar lane wrappers. The map layer includes it by itself. |

## What it does not hold

- `struct alwan_ctx` and its fields: use `alwan_ctx_alloc` / `alwan_ctx_free`.
- alwan's embedded data tables and their accessors (`g_*`), the registry, and the per-feature
  workers declared in `alwan_internal.h`.
- Anything named only in `alwan_internal.h`.

A name that is not listed here is not part of the foundation, whatever header it happens to
sit in.
