# GPU Backends

Alwan's core math is header-only and cross-platform. The same `*_core.inc` files that produce C f32/f64 functions are also compiled directly into HLSL, GLSL, OpenCL and Halide pipelines, and into CUDA kernels through the C branch itself.

---

## Overview

| Backend | File | Scalar type | Precision | Use case |
|---------|------|-------------|-----------|----------|
| **C** (default) | auto-detected | `double` or `float` | f32 or f64 | CPU, server, tools |
| **HLSL** | `alwan_hlsl.h` | `float` | f32 | DirectX shaders |
| **GLSL** | `alwan_glsl.h` | `float` | f32 | OpenGL / Vulkan shaders |
| **Halide** | `alwan_halide.h` | `Halide::Expr` | f32 or f64 | Pipeline generators |
| **OpenCL** | `alwan_opencl.h` | `float` (`double` with `ALWAN_OPENCL_FP64=1`) | f32, f64 opt-in | OpenCL C kernels |
| **CUDA** | none: `ALWAN_CUDA` under `__CUDACC__` | `float` and `double` | f32 and f64 | CUDA kernels, the C branch as-is |

The HLSL and GLSL backends are single-precision only; OpenCL is single precision unless the device reports `cl_khr_fp64` and the program is built with `ALWAN_OPENCL_FP64=1`; the Halide backend supports both (set `ALWAN_HALIDE_FLOAT_BITS` to 32 or 64). The C backend exposes both `_f32` (float) and `_f64` (double) variants, and CUDA compiles that same C branch, so a kernel can call either suffix.

---

## Architecture

Each `*_core.h` module contains a `#if ALWAN_BACKEND == ALWAN_BACKEND_C` / `#else` split:

- **C path**: includes the `.inc` twice via `alwan_core_f32_setup.h` and `alwan_core_f64_setup.h`, emitting `alwan_foo_f32` and `alwan_foo_f64` variants.
- **GPU path**: includes the `.inc` once with `ALWAN_CORE_*` macros set by the bootstrap header (`alwan_hlsl.h` etc.), emitting unsuffixed `alwan_foo` variants that call GPU intrinsics.

The `ALWAN_CORE_*` macros bridge the `.inc` source to the active platform:

| Macro | C f32 | GPU |
|-------|-------|-----|
| `ALWAN_CORE_T` | `float` | `alwan_scalar` |
| `ALWAN_CORE_POW(x,y)` | `powf(x,y)` | `pow(x,y)` (intrinsic) |
| `ALWAN_CORE_SELECT(c,t,f)` | `(c)?(t):(f)` | `select(c,t,f)` (Halide) / `(c)?(t):(f)` |
| `ALWAN_CORE_FNLIT(name)` | `name_f32` | `name` |
| `ALWAN_CORE_FNVLIT(name)` | `name_f32_v` | `name_v` |

---

## Ranges: the same `[0, 1]` convention as the C API

The core `_v` functions every backend compiles are **native**: Lab `L` on
`[0, 100]`, LCh hue in degrees, OkLCh / JzCzhz hue in radians. The C API
normalizes at its boundary (`ALWAN_NORMALIZE_RANGES`, default `1`); a GPU or
Halide caller does the same at its own boundary with the **value forms**,
`ALWAN_NORMV_<SPACE>(v)` / `ALWAN_DENORMV_<SPACE>(v)`, which rescale a colour
struct in place:

```c
alwan_lab lab = alwan_xyz_to_lab_v(xyz, white);   /* native: L on [0, 100] */
ALWAN_NORMV_LAB(lab);                             /* L on [0, 1], as the C API returns it */

alwan_oklch lch = alwan_oklab_to_oklch_v(ok);
ALWAN_NORMV_OKLCH(lch);                           /* h / pi on [-1, 1] */
```

They follow `ALWAN_NORMALIZE_RANGES` on every backend (no-ops at `0`), use
nothing but `.` and a `do / while` that stays one statement (its condition is
`false` on GLSL, `0` elsewhere) -- syntax C, C++ / Halide, HLSL, GLSL,
OpenCL C and CUDA share -- and they are the **same rescaling** the C API
applies: its pointer forms, `ALWAN_NORM_<SPACE>(p)`, are defined as the value
forms through `*(p)` and exist on the C backend only. Taking an input: call the
`DENORMV` form on a normalized struct before handing it to a core `_v`
function. Unbounded channels (Lab `a`, `b`, chroma, Oklab `a`, `b`) are never
rescaled. The per-space table is [ranges.md](../ranges.md).

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
- `ALWAN_LITERAL(x)` = `(x)`; no suffix needed (HLSL literals are float by default)
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

Color types are always plain structs (`struct { alwan_scalar v[N]; }`) on every backend; they are **never** aliased to GPU vector types such as `float3` or to swizzle types. This keeps field access and per-channel semantics identical across C, HLSL, GLSL, and Halide; pack/unpack to `float3`/`vec3` explicitly at the shader boundary.

### Usage in a Pixel Shader

```hlsl
#include "alwan_hlsl.h"
#include "core/alwan_core.h"
#include "core/alwan_colorspace_core.h"
#include "core/alwan_oklab_core.h"

float4 PSMain(float2 uv : TEXCOORD) : SV_Target
{
    float4 encoded = InputTexture.Sample(Sampler, uv);

    // GPU-backend core functions are single-precision and return by value.
    // Transfer functions are unsuffixed scalars (alwan_srgb_eotf/_oetf); the
    // colour-model conversions use the `_v` (value) form. There is no
    // rgb<->xyz helper in the core; supply the sRGB<->XYZ 3x3 matrices from
    // the host and use the generic alwan_mat3_mulv_v(matrix, vector).

    // Decode sRGB gamma
    alwan_vec3 linear;
    linear.v[0] = alwan_srgb_eotf(encoded.r);
    linear.v[1] = alwan_srgb_eotf(encoded.g);
    linear.v[2] = alwan_srgb_eotf(encoded.b);

    // RGB -> XYZ (host-supplied matrix) -> Oklab -> Oklch
    alwan_vec3  xyz   = alwan_mat3_mulv_v(rgb_to_xyz, linear);
    alwan_oklab oklab = alwan_xyz_to_oklab_v(xyz);
    alwan_oklch oklch = alwan_oklab_to_oklch_v(oklab);

    // Shift hue (~18 deg). Oklch components are { L, C, h } at v[0..2].
    oklch.v[2] += 0.05;

    // Oklch -> Oklab -> XYZ -> RGB (host-supplied inverse matrix)
    oklab  = alwan_oklch_to_oklab_v(oklch);
    xyz    = alwan_oklab_to_xyz_v(oklab);
    linear = alwan_mat3_mulv_v(xyz_to_rgb, xyz);

    return float4(alwan_srgb_oetf(linear.v[0]),
                  alwan_srgb_oetf(linear.v[1]),
                  alwan_srgb_oetf(linear.v[2]),
                  encoded.a);
}
```

### HLSL Limitations

- No context (`alwan_ctx`): on the C backend `alwan_ctx` exists for the **embedded RGB-space registry** lookup (resolving an `ALWAN_RGB_*` enum to its `alwan_rgb_space_desc` primaries/whitepoint/TFs) and for the custom allocator; it is *not* a runtime data loader (runtime loading is not implemented). GPU code passes the descriptor explicitly, so no context is needed. All GPU functions are standalone.
- No bulk/stride functions: GPU shaders process one pixel at a time or via texture samplers.
- No `alwan_spd` operations: spectral data structures are C-only.
- No camera profiling or Munsell/ColorChecker lookups: these need the C-side embedded data tables.
- `ALWAN_CBRT(x)` is a **signed** cube root, `sign(x) * pow(abs(x), 1.0f/3.0f)`, so it matches libm `cbrtf` on negatives, where a bare `pow(x, 1/3)` returns NaN. There is no native HLSL cube root.
- **All 43 cores compile as HLSL**, under dxc at `cs_6_0` and fxc at `cs_5_0`, in both fast and deterministic modes (`alwan_dev/tools/check_gpu_compile.py`). The last three were `alwan_table_core.h` and the two that include it, `alwan_lut_core.h` and `alwan_vision_core.h`, and they were one cause: the table readers took the table as a pointer, and a shading language has no pointer type.

### Reading a table from a shader

The readers are in `core/alwan_table_reader.inc`, written against an accessor, `ALWAN_TABLE_READ(i)`, instead of a pointer. C binds the accessor to `table[i]` and keeps every reader it had, with the same names and signatures. A shader says what its table is, then includes the file, once per table:

```hlsl
#include "alwan_hlsl.h"
#include "core/alwan_table_core.h"

StructuredBuffer<float> GradeLut : register(t1);

#define ALWAN_TABLE_NAME     grade
#define ALWAN_TABLE_READ(i)  GradeLut[i]
#include "core/alwan_table_reader.inc"

alwan_vec3 graded = alwan_table3d_sample_tetrahedral_grade(33, rgb);
```

The name is appended to each reader and the table parameter is gone, since the accessor already says which table. The file undefines everything it was given, so a second table is another three lines. The accessor is any expression in `i`: a `StructuredBuffer`, a `static const` array, a `Load` on a texture, or an offset into a buffer several tables share, which is the case no parameter type could have expressed. It is evaluated with an index the gate has already clamped, so it needs no bounds check, and it may be evaluated more than once.

Three accessors, each enabling its own family:

| Define | Element | Readers |
|---|---|---|
| `ALWAN_TABLE_READ(i)` | scalar | `alwan_table1d_*`, `alwan_table2d_grid_*`, `alwan_table3d_*`, `alwan_table2d_sample_*` (the cube as a strip) |
| `ALWAN_TABLE_READ_MAT3(i)` | `alwan_mat3x3` | `alwan_table1d_mat3_*` |
| `ALWAN_TABLE_READ_P0(i)`, `_P1`, `_P2` | scalar, three planes | `alwan_table3d_planar3_*` |

`alwan_lut1d_sample_v`, `alwan_lut2d_sample_v` and `alwan_lut3d_sample_v` take a pointer and are compiled out on HLSL and GLSL. They were always thin names over `alwan_table1d_sample_linear`, `alwan_table2d_sample_trilinear` and `alwan_table3d_sample_trilinear`, which is what a shader calls instead. A table whose length is a compile-time constant can also be passed by value, as `ALWAN_PARAM_ARRAY_IN(type, name, n)`; the Machado severity ramps in `alwan_vision_core.h` go that way, so `alwan_simulate_machado_protan_v` needs no binding at all.

A caller that fetches its own elements can skip the readers and use what they are built from, all plain value functions in `alwan_table_core.h`: `alwan_table_cell_v` for the two indices and the fraction, `alwan_table3d_index_v`, `alwan_table2d_strip_index_v` and `alwan_table3d_planar_index_v` for the flat address of a node in each layout, and `alwan_table_blend_v`, `_delta_v`, `_catmull_rom_v`, `_mat3_v`, `_trilinear_v` and `_tetrahedral_v` for the arithmetic.

**A table read gives the same bits on the GPU as in C, in an ordinary build and not only a deterministic one.** `alwan_dev/hlsl_regression/run_table_parity.py` binds a `StructuredBuffer`, a static array, three planes in one buffer and the embedded Machado ramp, runs the shader on the D3D12 WARP device under both compilers, and holds every reader to bit equality with the C f32 path, over a coordinate sweep that includes values outside [0, 1], both infinities and NaN. Its first run did not pass, and what it found is why the claim can be made:

- dxc was one ULP off on Catmull-Rom and on tetrahedral, on about a third of the samples. It reassociates a long sum unless the result is `precise`. fxc did not, and the shorter blends happened to agree under both. Every blend now computes into an `ALWAN_DET_PRECISE` local.
- Every blend that went through `alwan_lerp` was one ULP off under both compilers. On HLSL that is the `lerp` intrinsic, `a + t*(b-a)`, where C spells `(1-t)*a + t*b`. No table blend uses `alwan_lerp` now. In C the replacement is the same two products in the same order, so nothing moved: the determinism dump is byte-identical before and after, 413,041 lines.

One thing the run measures and does not fix. The Machado simulation samples its matrix exactly and then multiplies a colour by it in `alwan_mat3_mulv_v`. Both shader compilers lower that three-term sum to a dot product, which associates differently from C, so in an ordinary build about a fifth of the outputs are one ULP off. That is the matrix product, which every colour conversion goes through, and whether an ordinary build should pay for `precise` there is a wider decision than this one.

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
- `ALWAN_FMOD(x,y)` = `mod(x,y)`; GLSL's built-in `mod` is the floored modulo (`x - y*floor(x/y)`), which differs from C `fmod` (truncated toward zero) for negative operands (the Halide backend uses the explicit floored form `x - floor(x/y)*y`)
- `ALWAN_LOG10(x)` = `log(x)/log(10.0)` (no native GLSL log10)
- `ALWAN_CBRT(x)` = signed cube root, `sign(x) * pow(abs(x), 1.0/3.0)` (parity with libm `cbrt`; bare `pow` NaNs on negatives)

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
| `fmod(x,y)` | `fmod(x,y)` (truncated) | `mod(x,y)` (floored) |
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
- `ALWAN_SELECT(c,t,f)` = `Halide::select(c,t,f)` (lazy, symbolic)
- All math macros = `Halide::pow`, `Halide::sqrt`, `Halide::log`, etc.
- `alwan_min/max/clamp` = `Halide::min`, `Halide::max`, `Halide::clamp`

### Float Precision

Set `ALWAN_HALIDE_FLOAT_BITS` before including `alwan_halide.h`:

```cpp
#define ALWAN_HALIDE_FLOAT_BITS 32  // Float(32), the default
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

        // The Halide path uses symbolic computation; no actual values yet
        // alwan functions return Halide::Expr, which Halide schedules/compiles

        output(x, y, 0) = rgb_val.r; // simplified; see full Oklab example below
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

- Operations are **symbolic**: no immediate results until `realize()` or AOT compilation.
- Cannot use C `if`/`else` on `Halide::Expr` values; all conditionals must go through `Halide::select` (mapped by `ALWAN_SELECT`).
- No `alwan_ctx`: context is C-only.
- Loop-based spectral integration is not GPU-suitable; use precomputed LUT approaches for spectral work.

---

## CUDA

CUDA is not an `ALWAN_BACKEND` value. nvcc is a C++ compiler with a real
`double`, so it compiles the ordinary C branch of every core header unchanged,
`_f32` and `_f64` alike; a separate id would only make every
`ALWAN_BACKEND == ALWAN_BACKEND_C` guard in the tree wrong. What CUDA needs is
the qualifier, and `alwan_platform.h` supplies it when `__CUDACC__` is defined:
`ALWAN_CUDA` is 1, every header-only function is `__host__ __device__`, and the
constant tables they read carry `__device__` in the device pass and nothing in
the host pass, so one declaration serves both. Test `ALWAN_CUDA`, not
`ALWAN_BACKEND`, to ask "am I being compiled for CUDA".

### Setup

```
nvcc -std=c++14 --expt-relaxed-constexpr -arch=sm_86 --fmad=false \
     -I <alwan>/src/alwan -I <alwan>/src/alwan/core my_kernel.cu
```

`--fmad=false` is the CUDA spelling of `-ffp-contract=off`. nvcc fuses
`a*b + c` into an fma by default, and a build that fuses is not bit-comparable
with a CPU that does not. The flag is required for parity and is what the
regression harness uses.

### Usage in a kernel

Alwan launches nothing, allocates no device memory and owns no image. You write
the `__global__` function, index the pixel and call the core on it. This is
`alwan_dev/cuda_regression/example_kernel.cu`, which the harness compiles and
runs so the snippet cannot rot:

```cuda
#include "alwan_types.h"
#include "core/alwan_oklab_core.h"
#include "core/alwan_core.h"

__global__ void srgb_to_oklab_kernel(float *out, float const *in, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;

    alwan_xyz_f32 lin;
    lin.x = alwan_srgb_eotf_f32(in[i * 3 + 0]);
    lin.y = alwan_srgb_eotf_f32(in[i * 3 + 1]);
    lin.z = alwan_srgb_eotf_f32(in[i * 3 + 2]);

    alwan_xyz_f32 xyz;   /* the caller's own matrix, next to alwan's maths */
    xyz.x = 0.4124564f * lin.x + 0.3575761f * lin.y + 0.1804375f * lin.z;
    xyz.y = 0.2126729f * lin.x + 0.7151522f * lin.y + 0.0721750f * lin.z;
    xyz.z = 0.0193339f * lin.x + 0.1191920f * lin.y + 0.9503041f * lin.z;

    alwan_oklab_f32 lab = alwan_xyz_to_oklab_f32_v(xyz);
    out[i * 3 + 0] = lab.L;
    out[i * 3 + 1] = lab.a;
    out[i * 3 + 2] = lab.b;
}
```

The `_f64` core is equally callable here, which is the one thing the
shading-language backends cannot offer.

### What agrees with the CPU, and when

Host and device compile one source, so any difference is the hardware and the
compiler, and there are two: contraction, closed by `--fmad=false`, and the
device math library, whose `pow`, `exp` and `log` are not bit-identical to the
host's and are not required to be. Under `ALWAN_DETERMINISTIC=1` those calls
are the committed polynomials, and the answer is bit-exact CPU against GPU on
every kernel the harness runs (`alwan_dev/cuda_regression/README.md` has the
table and the ULP caveat that goes with transfer functions). The fast build
agrees to the accuracy of the device library, and the harness says which of
the two it is measuring.

---

## OpenCL

OpenCL **is** an `ALWAN_BACKEND` value (4), because OpenCL C is not C:
program-scope constants live in an address space, `double` is an extension
rather than a given, and there is no C library. It takes the same single-pass
GPU branch of the core headers that HLSL and GLSL take, at single precision.

### Setup

```c
#include "alwan_opencl.h"
#include "core/alwan_oklab_core.h"
```

Build the program with `-I` pointing at `src/alwan` and `src/alwan/core`, and
with `-cl-std=CL2.0`. After the bootstrap header, `alwan_scalar` is `float`,
the math macros are OpenCL C builtins, and every colour type is a struct:
alwan never aliases to `float3`, whose size is 16 bytes rather than 12 and
whose components are swizzles.

`double` is opt-in: query the device for `cl_khr_fp64` first, then build with
`-DALWAN_OPENCL_FP64=1`. A program that enables the extension on a device
without it fails to build rather than silently demoting, which is the right
failure.

### Usage in a kernel

```c
#include "alwan_opencl.h"
#include "core/alwan_oklab_core.h"

__kernel void to_oklab(__global float *out, __global const float *xyz) {
    int i = get_global_id(0);
    alwan_xyz x;
    x.x = xyz[i*3+0]; x.y = xyz[i*3+1]; x.z = xyz[i*3+2];
    alwan_oklab lab = alwan_xyz_to_oklab_v(x);
    out[i*3+0] = lab.L; out[i*3+1] = lab.a; out[i*3+2] = lab.b;
}
```

The tables the core reads are `__global const`, not `__constant`, and that is
structural rather than a preference: `__constant` is not part of the generic
address space in 1.2 or 2.0, so a `__constant T *` cannot be passed where the
core's samplers take a plain pointer. `alwan_dev/opencl_regression/README.md`
walks through it.

### What agrees with the CPU, and when

Under `ALWAN_DETERMINISTIC=1` every kernel the harness runs is bit-exact
against the CPU, every sample, on two conditions. Contraction is closed by
`#pragma OPENCL FP_CONTRACT OFF` inside the deterministic header. Division is
the one that hides: OpenCL allows single-precision divide to be 2.5 ULP off,
and `-cl-fp32-correctly-rounded-divide-sqrt` is what asks for IEEE. Without it
four of six kernels differ, and the primitives do not show it, because the
only division in `log2`, `exp2`, `pow_pos` and `cbrt` is by 0.5, which every
implementation rounds correctly; the sRGB EOTF, which divides by 1.055, was
wrong on 2831 of 65536 samples. The flag is a device capability, and the
harness checks for it before claiming anything.

---

## Backend Detection

`alwan_platform.h` auto-detects the backend from compiler macros:

```c
#if defined(__HLSL_VERSION)
#  define ALWAN_BACKEND ALWAN_BACKEND_HLSL
#elif defined(__OPENCL_VERSION__)
#  define ALWAN_BACKEND ALWAN_BACKEND_OPENCL
#elif defined(GL_core_profile) || defined(GL_es_profile)
#  define ALWAN_BACKEND ALWAN_BACKEND_GLSL
#elif defined(HALIDE_HALIDERUNTIME_H)
#  define ALWAN_BACKEND ALWAN_BACKEND_HALIDE
#else
#  define ALWAN_BACKEND ALWAN_BACKEND_C
#endif
```

OpenCL is checked before GLSL on purpose: an OpenCL C compiler defines
`__OPENCL_VERSION__`, and some also predefine GL interop symbols. CUDA is not
in this chain: under `__CUDACC__` the backend stays `ALWAN_BACKEND_C` and
`ALWAN_CUDA` is set instead, as the CUDA section explains.

To force a backend (e.g. cross-compilation tools or offline preprocessing):

```c
#define ALWAN_BACKEND ALWAN_BACKEND_HLSL  // before any alwan include
#include "alwan_platform.h"
```

The bootstrap headers (`alwan_hlsl.h`, `alwan_glsl.h`, `alwan_halide.h`, `alwan_opencl.h`) do this for you with `#ifndef ALWAN_BACKEND` guards.

---

## Which Modules Are GPU-Compatible

All `*_core.h` files compile on all backends. The following are **C-only** (require context, heap allocation, or C standard library):

| Feature | Reason |
|---------|--------|
| `alwan_create` / `alwan_destroy` | Dynamic allocation |
| `alwan_rgb_get_space_descriptor` | Embedded RGB-space registry lookup (enum -> descriptor) |
| `alwan_spd_*` | Dynamic SPD struct |
| `alwan_munsell_*`, `alwan_colorchecker_*` | Atlas data lookup |
| Bulk `_map_interleave` / `_map_interleave_ex` | Loop + stride logic |
| Camera profiling | Matrix fitting |
| CCT estimation | Requires illuminant tables |

All core conversion functions (XYZ<->Lab, Oklab, ICtCp, transfer functions, CAT, gamut mapping primitives, etc.) are GPU-compatible.

---

## See Also

- [CPU / SIMD Backends](../backends-cpu.md): the C backend's SSE2/AVX/AVX2/NEON kernels, SVML gating, and fast-vs-deterministic math paths
- [Configuration](../configuration.md): `ALWAN_EMBED_DATA`, custom allocators
- [Getting Started](../getting-started.md): C usage introduction
- [Map / Bulk Operations](map.md): CPU batch processing
