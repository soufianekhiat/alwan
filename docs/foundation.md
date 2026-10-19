# The foundation: building a library on alwan

`alwan.h` is the API for applications. `alwan_foundation.h` adds the part of alwan's machinery
that a library built **on** alwan may use: how alwan allocates, how it templates its cores for
both precisions, how a core reads a table on the CPU and in a shader, and the typed pixel
layer. Suwar, the image-processing library released with alwan 3.0.0, is built on it.

The foundation is supported from 3.0.0 under the same compatibility rules as `alwan.h`.
`alwan_internal.h` is not: it stays alwan's own and changes freely. A library built on alwan
includes `alwan.h` and `alwan_foundation.h` and nothing else from alwan.

## Two headers

- `alwan_foundation_core.h` is shader-safe: it compiles as C, HLSL, GLSL, CUDA and OpenCL and
  holds the platform layer, the types and the paths of the templating headers. A core header
  (one that a shader may include) includes this one, never `alwan_platform.h` or
  `alwan_types.h` by name.
- `alwan_foundation.h` is the C side: `alwan.h`, allocation, the map and SIMD layers. It
  includes `alwan_foundation_core.h`.

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

### Names in detail

These are the names a library may use, spelled out. Suwar uses all of them.

| Group | Names |
|---|---|
| Backend selection | `ALWAN_BACKEND`, `ALWAN_BACKEND_C` (and the other `ALWAN_BACKEND_*` values), `ALWAN_HAS_POINTERS` (0 on HLSL and GLSL, 1 elsewhere) |
| Language helpers | `ALWAN_INLINE`, `ALWAN_CONSTEXPR`, `ALWAN_TYPE_DEF`, `ALWAN_UNUSED`, `ALWAN_LITERAL`, `ALWAN_SELECT` |
| Math macros | `ALWAN_<F>`, `ALWAN_<F>_F32` and `ALWAN_<F>_F64` for F in ABS, ACOS, ATAN, ATAN2, CBRT (signed), CEIL, COS, COSH, EXP, FLOOR, FMOD (truncating), HYPOT, LN, LOG10, LOG2, POW, POWI, ROUND, SIN, SINH, SQRT, TANH, TRUNC. Not every F has all three spellings; the precision-suffixed ones are complete for F32 and F64 where alwan uses them. In the deterministic build they are alwan's polynomial math. |
| Fused multiply-add | `ALWAN_FMA_CR_F32`, `ALWAN_FMA_CR_F64` (C99 `fmaf` / `fma`, correctly rounded, kept by the deterministic build: for results that must match a fused reference), `ALWAN_FMAF` (`fmaf` on C, `a * b + c` on the shader backends) |
| Evaluation order | `ALWAN_DET_PRECISE`: marks a local whose expression must not be reassociated or contracted (`precise` in HLSL, nothing in C, where the deterministic build turns contraction off) |
| Constants | `ALWAN_PI`, `ALWAN_PI_F64` |
| Diagnostics | `ALWAN_DIAG_PUSH`, `ALWAN_DIAG_POP`, `ALWAN_DIAG_DISABLE_FLOAT_CONV`, `ALWAN_DIAG_DISABLE_UNUSED_FUNCTION` |
| Memory | `ALWAN_ALLOC`, `ALWAN_FREE`, `ALWAN_REALLOC`, `ALWAN_MEMCPY` (the default allocator; prefer `alwan_ctx_alloc` where a context is at hand) |
| Precision switches | `ALWAN_WITH_F32`, `ALWAN_WITH_F64`, `ALWAN_WITH_F64_FACADE` (from `alwan_build_config.h`) |
| Core templating | inside a setup/teardown pair: `ALWAN_CORE_T`, `ALWAN_CORE_FN`, `ALWAN_CORE_FNLIT`, `ALWAN_CORE_FNVLIT`, `ALWAN_CORE_FNLIT_MAP`, `ALWAN_CORE_SUFFIX`, `ALWAN_CORE_LITERAL`, `ALWAN_CORE_TABLE_CELL` and the `ALWAN_CORE_*` math |
| Table, half and microfacet cores | `alwan_table_coord_*_v`, `alwan_table_coord_unit_*_v`, `alwan_table_cell_*`, `alwan_half_to_float_*_v`, `alwan_float_to_half_*_v`, `alwan_microfacet_*` |
| Types | `alwan_f32`, `alwan_f64`, `alwan_scalar`, the vector and matrix types of `alwan_types.h`, `alwan_sample_mode` (`ALWAN_SAMPLE_LINEAR`, ...) |

## What it does not hold

- `struct alwan_ctx` and its fields: use `alwan_ctx_alloc` / `alwan_ctx_free`.
- alwan's embedded data tables and their accessors (`g_*`), the registry, and the per-feature
  workers declared in `alwan_internal.h`.
- Anything named only in `alwan_internal.h`.

A name that is not listed here is not part of the foundation, whatever header it happens to
sit in.
