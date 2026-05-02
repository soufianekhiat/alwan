# GPU Backends

Alwan's core math is header-only and cross-platform. The same `*_core.inc` files that produce C f32/f64 functions are also compiled directly into HLSL, GLSL, and Halide pipelines.

---

## Overview

| Backend | File | Scalar type | Precision | Use case |
|---------|------|-------------|-----------|----------|
| **C** (default) | auto-detected | `double` or `float` | f32 or f64 | CPU, server, tools |
| **HLSL** | `alwan_hlsl.h` | `float` | f32 | DirectX shaders |
| **GLSL** | `alwan_glsl.h` | `float` | f32 | OpenGL / Vulkan shaders |
| **Halide** | `alwan_halide.h` | `Halide::Expr` | f32 or f64 | Pipeline generators |

GPU backends always use single precision. The C backend exposes both `_f32` (float) and `_f64` (double) variants — choose the appropriate suffix at each call site.

---

## Architecture

Each `*_core.h` module contains a `#if ALWAN_BACKEND == ALWAN_BACKEND_C` / `#else` split:

- **C path** — includes the `.inc` twice via `alwan_core_f32_setup.h` and `alwan_core_f64_setup.h`, emitting `alwan_foo_f32` and `alwan_foo_f64` variants.
- **GPU path** — includes the `.inc` once with `ALWAN_CORE_*` macros set by the bootstrap header (`alwan_hlsl.h` etc.), emitting unsuffixed `alwan_foo` variants that call GPU intrinsics.

The `ALWAN_CORE_*` macros bridge the `.inc` source to the active platform:

| Macro | C f32 | GPU |
|-------|-------|-----|
| `ALWAN_CORE_T` | `float` | `alwan_scalar` |
| `ALWAN_CORE_POW(x,y)` | `powf(x,y)` | `pow(x,y)` (intrinsic) |
| `ALWAN_CORE_SELECT(c,t,f)` | `(c)?(t):(f)` | `select(c,t,f)` (Halide) / `(c)?(t):(f)` |
| `ALWAN_CORE_FNLIT(name)` | `name_f32` | `name` |
| `ALWAN_CORE_FNVLIT(name)` | `name_f32_v` | `name_v` |

---

## HLSL Backend

### Setup

```hlsl
// At the top of your HLSL file, before any alwan headers
#include "alwan_hlsl.h"
#include "core/alwan_core.h"
#include "core/alwan_oklab_core.h"
#include "core/alwan_colorspace_core.h"
// ... include whichever modules you need
```

`alwan_hlsl.h` sets `ALWAN_BACKEND = ALWAN_BACKEND_HLSL` (also auto-detected from `__HLSL_VERSION`) and defines:
- `alwan_scalar` = `float`
- `ALWAN_LITERAL(x)` = `(x)` — no suffix needed (HLSL literals are float by default)
- `ALWAN_SELECT(c,t,f)` = `(c) ? (t) : (f)`
- Math macros routed to HLSL intrinsics: `pow`, `sqrt`, `exp`, `log`, `atan2`, `floor`, `ceil`
- `alwan_min/max/clamp/saturate/lerp` = HLSL built-ins
- All semantic types (`alwan_rgb`, `alwan_xyz`, etc.) = `struct { float r, g, b; }` etc.

### HLSL Type Mappings

| Alwan type | HLSL equivalent |
|-----------|----------------|
| `alwan_scalar` | `float` |
| `alwan_rgb` | `struct { float r, g, b; }` |
| `alwan_xyz` | `struct { float x, y, z; }` |
| `alwan_lab` | `struct { float L, a, b; }` |
| `alwan_mat3x3` | `struct { float m[9]; }` |

### Usage in a Pixel Shader

```hlsl
#include "alwan_hlsl.h"
#include "core/alwan_core.h"
#include "core/alwan_colorspace_core.h"
#include "core/alwan_oklab_core.h"

float4 PSMain(float2 uv : TEXCOORD) : SV_Target
{
    float4 encoded = InputTexture.Sample(Sampler, uv);

    // Decode sRGB gamma
    alwan_rgb linear;
    linear.r = alwan_srgb_eotf(encoded.r);
    linear.g = alwan_srgb_eotf(encoded.g);
    linear.b = alwan_srgb_eotf(encoded.b);

    // Convert to Oklab
    alwan_xyz xyz;
    alwan_rgb_to_xyz(&xyz, &linear); // assumes sRGB primaries pre-loaded

    alwan_oklab oklab;
    alwan_xyz_to_oklab(&oklab, &xyz);

    // Shift hue in Oklch
    alwan_oklch oklch;
    alwan_oklab_to_oklch(&oklch, &oklab);
    oklch.h += 0.05; // ~18° hue rotation

    // Back to sRGB
    alwan_oklab_to_oklch(&oklab, &oklch); // note: this is oklch_to_oklab
    alwan_oklab_to_xyz(&xyz, &oklab);
    alwan_xyz_to_rgb(&linear, &linear); // placeholder — use appropriate space

    return float4(alwan_srgb_oetf(linear.r),
                  alwan_srgb_oetf(linear.g),
                  alwan_srgb_oetf(linear.b),
                  encoded.a);
}
```

### HLSL Limitations

- No context (`alwan_ctx`) — context is only needed for the C API's runtime data loading. All GPU functions are standalone.
- No bulk/stride functions — GPU shaders process one pixel at a time or via texture samplers.
- No `alwan_spd` operations — spectral data structures are C-only.
- No camera profiling or Munsell/ColorChecker lookups — these require runtime data.
- `ALWAN_CBRT(x)` is implemented as `pow(x, 1.0f/3.0f)` — no native HLSL cube root.

---

## GLSL Backend

### Setup

```glsl
// At the top of your GLSL file
#include "alwan_glsl.h"
#include "core/alwan_core.h"
#include "core/alwan_colorspace_core.h"
```

`alwan_glsl.h` sets `ALWAN_BACKEND = ALWAN_BACKEND_GLSL` (also auto-detected from `GL_core_profile` / `GL_es_profile`) and defines:
- `alwan_scalar` = `float`
- `ALWAN_LITERAL(x)` = `(x)`
- `ALWAN_SELECT(c,t,f)` = `(c) ? (t) : (f)`
- Math macros routed to GLSL built-ins
- `alwan_min/max/clamp` = GLSL built-ins; `alwan_saturate(x)` = `clamp(x, 0.0, 1.0)`; `alwan_lerp(a,b,t)` = `mix(a,b,t)`
- `ALWAN_ATAN2(y,x)` = `atan(y,x)` (GLSL two-argument form)
- `ALWAN_FMOD(x,y)` = `x - floor(x/y)*y` (GLSL has no `mod` for signed-style fmod)
- `ALWAN_LOG10(x)` = `log(x)/log(10.0)` (no native GLSL log10)
- `ALWAN_CBRT(x)` = `pow(x, 1.0/3.0)`

### Usage in a Fragment Shader

```glsl
#version 460

#include "alwan_glsl.h"
#include "core/alwan_core.h"
#include "core/alwan_colorspace_core.h"

uniform sampler2D u_texture;
in vec2 v_uv;
out vec4 FragColor;

void main() {
    vec4 c = texture(u_texture, v_uv);

    alwan_rgb lin = { alwan_srgb_eotf(c.r),
                      alwan_srgb_eotf(c.g),
                      alwan_srgb_eotf(c.b) };

    alwan_lab lab;
    alwan_xyz xyz;
    // ... convert through XYZ to Lab ...

    FragColor = vec4(alwan_srgb_oetf(lin.r),
                     alwan_srgb_oetf(lin.g),
                     alwan_srgb_oetf(lin.b), c.a);
}
```

### GLSL vs HLSL Differences

| Feature | HLSL | GLSL |
|---------|------|------|
| `lerp` | `lerp(a,b,t)` | `mix(a,b,t)` |
| `saturate` | `saturate(x)` | `clamp(x,0.0,1.0)` |
| `atan2(y,x)` | `atan2(y,x)` | `atan(y,x)` |
| `fmod(x,y)` | `fmod(x,y)` | `x - floor(x/y)*y` |
| `log10(x)` | `log10(x)` | `log(x)/log(10.0)` |

Both backends have identical function names and identical Alwan type definitions.

---

## Halide Backend

### Setup

```cpp
#include "alwan_halide.h"
#include "core/alwan_core.h"
#include "core/alwan_colorspace_core.h"
```

`alwan_halide.h` sets `ALWAN_BACKEND = ALWAN_BACKEND_HALIDE` (also auto-detected from `HALIDE_HALIDERUNTIME_H`) and configures:
- `alwan_scalar` = `alwan_halide_scalar` (a `Halide::Expr` subclass with an implicit `double` constructor)
- `ALWAN_LITERAL(x)` = `Halide::Internal::make_const(type, x)` where `type` is `Float(32)` or `Float(64)` depending on `ALWAN_HALIDE_FLOAT_BITS`
- `ALWAN_SELECT(c,t,f)` = `Halide::select(c,t,f)` — lazy, symbolic
- All math macros = `Halide::pow`, `Halide::sqrt`, `Halide::log`, etc.
- `alwan_min/max/clamp` = `Halide::min`, `Halide::max`, `Halide::clamp`

### Float Precision

Set `ALWAN_HALIDE_FLOAT_BITS` before including `alwan_halide.h`:

```cpp
#define ALWAN_HALIDE_FLOAT_BITS 32  // Float(32) — default
// or
#define ALWAN_HALIDE_FLOAT_BITS 64  // Float(64)
#include "alwan_halide.h"
```

### Usage in a Halide Generator

```cpp
#include "alwan_halide.h"
#include "core/alwan_core.h"
#include "core/alwan_colorspace_core.h"

class SRGBToOklab : public Halide::Generator<SRGBToOklab> {
public:
    Input<Buffer<float, 3>>  input{"input"};
    Output<Buffer<float, 3>> output{"output"};

    void generate() {
        Var x, y, c;

        // Decode sRGB
        Func linear;
        linear(x, y, c) = alwan_srgb_eotf(input(x, y, c));

        // Pack into struct and convert
        alwan_rgb rgb_val;
        rgb_val.r = linear(x, y, 0);
        rgb_val.g = linear(x, y, 1);
        rgb_val.b = linear(x, y, 2);

        // The Halide path uses symbolic computation — no actual values yet
        // alwan functions return Halide::Expr, which Halide schedules/compiles

        output(x, y, 0) = rgb_val.r; // simplified — see full Oklab example below
        output(x, y, 1) = rgb_val.g;
        output(x, y, 2) = rgb_val.b;
    }
};
```

### `alwan_halide_scalar` Implicit Constructor

CSV data files (e.g. `#include "data/matrices/oklab_m1.csv"`) contain bare double literals. The `alwan_halide_scalar` struct adds an implicit constructor from `double` so these literals can initialize `alwan_scalar` arrays without wrapping every value in `ALWAN_LITERAL()`:

```cpp
// This works because of the implicit double constructor:
static alwan_scalar const OKLAB_M1[9] = {
#include "data/matrices/oklab_m1.csv"
};
```

### Halide Limitations

- Operations are **symbolic** — no immediate results until `realize()` or AOT compilation.
- Cannot use C `if`/`else` on `Halide::Expr` values — all conditionals must go through `Halide::select` (mapped by `ALWAN_SELECT`).
- No `alwan_ctx` — context is C-only.
- Loop-based spectral integration is not GPU-suitable; use precomputed LUT approaches for spectral work.

---

## Backend Detection

`alwan_platform.h` auto-detects the backend from compiler macros:

```c
#if defined(__HLSL_VERSION)
#  define ALWAN_BACKEND ALWAN_BACKEND_HLSL
#elif defined(GL_core_profile) || defined(GL_es_profile)
#  define ALWAN_BACKEND ALWAN_BACKEND_GLSL
#elif defined(HALIDE_HALIDERUNTIME_H)
#  define ALWAN_BACKEND ALWAN_BACKEND_HALIDE
#else
#  define ALWAN_BACKEND ALWAN_BACKEND_C
#endif
```

To force a backend (e.g. cross-compilation tools or offline preprocessing):

```c
#define ALWAN_BACKEND ALWAN_BACKEND_HLSL  // before any alwan include
#include "alwan_platform.h"
```

The bootstrap headers (`alwan_hlsl.h`, `alwan_glsl.h`, `alwan_halide.h`) do this for you with `#ifndef ALWAN_BACKEND` guards.

---

## Which Modules Are GPU-Compatible

All `*_core.h` files compile on all backends. The following are **C-only** (require context, heap allocation, or C standard library):

| Feature | Reason |
|---------|--------|
| `alwan_create` / `alwan_destroy` | Dynamic allocation |
| `alwan_rgb_get_space_descriptor` | Runtime data lookup |
| `alwan_spd_*` | Dynamic SPD struct |
| `alwan_munsell_*`, `alwan_colorchecker_*` | Atlas data lookup |
| Bulk `_map_interleave` / `_map_interleave_ex` | Loop + stride logic |
| Camera profiling | Matrix fitting |
| CCT estimation | Requires illuminant tables |

All core conversion functions (XYZ↔Lab, Oklab, ICtCp, transfer functions, CAT, gamut mapping primitives, etc.) are GPU-compatible.

---

## See Also

- [Configuration](../configuration.md) — `ALWAN_EMBED_DATA`, custom allocators
- [Getting Started](../getting-started.md) — C usage introduction
- [Map / Bulk Operations](map.md) — CPU batch processing
