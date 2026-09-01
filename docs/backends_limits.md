# Backend limitations: what is *not* available on every backend

Alwan targets four backends via `ALWAN_BACKEND` (`alwan_platform.h`):

| id | backend | scalar | how the core emits |
|----|---------|--------|--------------------|
| 0 | **C / C++** | `double` (or `float` via `ALWAN_SCALAR_IS_FLOAT`) | dual pass: the shared `.inc` is included twice -> native `_f32` **and** `_f64` |
| 1 | **HLSL** | `float` | single pass: `.inc` included once against `alwan_core_aliases.inc` |
| 2 | **GLSL** | `float` | single pass |
| 3 | **Halide** | `Halide::Expr` (real `double` available) | single pass |

The C backend is the full, compiled, dual-precision library. The GPU backends
compile the *same* header-only per-pixel core kernels a second time, at single
precision, by swapping a macro vocabulary. This document lists everything that
does **not** carry over unchanged.

> **Read this first: verification status.** Run-verified on a GPU (dxc + D3D12
> WARP, matching the C reference; see `alwan_dev/hlsl_regression/`): the **AgX
> analytic render** (fast ULP / det bit-exact) and the **deterministic sRGB +
> BT.2020 transfer set** (bit-exact 64/64).
>
> Compile-verified under **both** shader compilers, fast and
> `ALWAN_DETERMINISTIC=1`, one translation unit per core through
> `alwan_hlsl.h`: **40 of 43 `*_core.h`** under dxc (Shader Model 6, DXIL) and
> the **same 40 of 43** under fxc (Shader Model 5, DXBC). Measured 2026-09-01,
> dxc 1.8.2502 and fxc 10.1 from Windows SDK 10.0.26100. The lists are
> `alwan_dev/hlsl_regression/cores_dxc_clean.txt` and `cores_fxc_clean.txt`,
> both gated by `alwan_dev/tools/check_gpu_compile.py`.
>
> The three that do not compile are the same on both, and they share one cause:
> `table`, `lut` and `vision`. Their readers take the table by pointer
> (`const alwan_scalar *table`), and a shading language has no pointer type. The
> addressing gate itself is pure integer and float maths and would compile; the
> fetch is what does not. Serving these on GPU needs a resource abstraction
> (`Buffer` / `StructuredBuffer`, or a texture), which is a design change rather
> than a syntax fix, so they stay CPU-only for now.
>
> Eight cores were fixed in 2.0.1 and are new to the list: `atd95`, `hunt`,
> `quality` and `view` were missing a struct, an enum, a constant block or a
> whole function from their GPU branch; `extended`, `hellwig2022` and `hdr` used
> raw pointer out-parameters instead of the `ALWAN_PARAM_*` macros; `half`
> punned float bits through `memcpy`, which needs `asuint` / `asfloat` on a
> shader. `alwan_config.h` also reached `<stddef.h>`, `<stdint.h>` and
> `<string.h>` unconditionally, so every core that included it failed on the
> include line before any of its own code was parsed.
>
> **Both compilers are gated, because they disagree about what HLSL is.** dxc
> is Clang-based and accepts C-shaped syntax that the legacy grammar never
> allowed. fxc is what Shader Model 5 and D3D11 content compiles with, and it
> is stricter. Two constructs that compile under dxc and are hard errors under
> fxc shipped in 2.0.0 and were fixed in 2.0.1:
>
> - **East const.** `alwan_scalar const a = ...` is `error X3000: syntax error:
>   unexpected token 'const'` under fxc, followed by a spurious `X3080:
>   function must return a value` because the enclosing function failed to
>   parse. Qualifiers must precede the type. `alwan_dev/tools/check_east_const.py`
>   gates this without needing a compiler, over `src/alwan/core` and the
>   platform layer; outside that tier east const is legal C and is left alone.
> - **`(void)x;` casts.** Not valid HLSL. Use `ALWAN_UNUSED(x)`, which is empty
>   on the HLSL and GLSL branches.
>
> Keyword collisions are a closed class: `alwan_dev/tools/check_gpu_identifiers.py`
> scans every GPU-reachable line of the cores (and the `.inc` files fed to the
> GPU) for a reserved word used as a name, against a list of 101 words dxc
> rejects (probed) plus the GLSL 4.60 and Metal keyword lists. It runs in the
> Tooling CI job and as a post-build step of the Alwan library, and is clean
> (`out`, `linear`, `in`, `matrix`, `input` were renamed in 2.0.1). The same
> tool reports, without gating, the C types and headers in GPU-reachable code
> (47 sites: `size_t` in the table/lut addressing, `uint32_t` and `<stdint.h>`
> in `half`, one stride loop in `hdr`); `--strict` gates them once fixed.

---

## 1. Double precision / the entire `_f64` API: **C only**

The shipped GPU backends are single precision (`alwan_scalar = float`). There is
no `f64` instantiation on any of them, so every `_f64` public function and any
result that relies on double accuracy is C-only. (Halide's scalar can be double,
but the shipped GPU path runs single precision.)

This follows from the backends, not from GPUs. HLSL and GLSL have no double.
A CUDA backend would, and would be the first GPU path where `_f64` is reachable;
see [alwan_future.md](alwan_future.md).

## 2. The compiled bulk / strided / typed API: **C only**

The GPU side is header-only, per-pixel, value-returning. It has **no** equivalent
of the compiled C library layer:

- `api/*.c` (48 files) and `map/*.c` (19 files): strided-buffer bulk transforms,
  `*_map_interleave`, the tiled SIMD map kernels;
- typed pixel formats (u8 / u16 / f16) and the `*_map_interleave_ex` typed
  delegates;
- null-checking / stride-loop wrappers.

On GPU the application owns buffers and dispatch; it calls the per-pixel core
functions directly. (Bulk, striding, pixel packing, and SIMD are all CPU-side.)

## 3. Core coverage: every core now has a GPU branch

All `*_core.h` shells now expose the GPU `#else` branch. The three former
holdouts were fixed:

| core | was | now |
|------|-----|-----|
| **`alwan_agx_jp2499_core`** (JP2499 DRT) | pointer out-params + pointer matrix geometry | **pointer-free** (struct-by-value `vec3`/`mat3x3`/small structs); the whole pipeline, *including* the 3x3-inversion geometry build, compiles for GPU (dxc-verified, fast + det); C output byte-identical |
| **`alwan_nayatani95_core`** | missing `#else` branch | branch added; compiles under dxc |
| **`alwan_normalize_core`** | missing `#else` branch; needed `alwan_uint16` on GPU | branch + GPU `uint` typedefs added; compiles under dxc |

`alwan_dev/hlsl_regression/cores_compile.hlsl` keeps these compiling as a
regression (fast and `-D ALWAN_DETERMINISTIC=1`).

For functions that do need out-parameters, the house idiom is
`ALWAN_PARAM_MAT3_OUT` / `ALWAN_PARAM_VEC3_OUT` / `ALWAN_REF` / `ALWAN_ADDR`
(`alwan_platform.h`): C/Halide expand to pointers, HLSL/GLSL to `out`
qualifiers. Per-pixel kernels prefer plain value returns (`_v` style, what
AgX, JP2499, `alwan_cdl_apply`, `nayatani95_forward` use); the `ALWAN_PARAM_*`
macros are the escape hatch when a signature must keep multiple outputs.

## 4. Data / LUT-backed features: embedding works; big 3D LUTs want textures

The CSV-embedding pattern the C build uses (`static const alwan_f32 t[] = {`
`#include "data/x.csv" };`) **compiles unchanged under dxc**, verified:

| table | size | result |
|-------|------|--------|
| AgX 1D contrast LUT | 4096 floats | [x] compiles in ~0.1 s, 40 KB DXIL; practical |
| Blender AgX 3D LUT | 57^3 (555 k floats) | [x] compiles (~1 s) but **4.8 MB DXIL**; works, yet a GPU **texture** (hardware filtering, no shader bloat) is the sane production route |

So embedded tables are **not fundamentally blocked** on GPU. What keeps
the LUT-backed features C-only today is *structure* rather than data: the LUT samplers
and view dispatch live in the compiled C layer (`api/alwan_view.c`), not in
header-only cores. Applies to: AgX views (SB2383 / Blender / original), the
ACES 2.0 cusp table, spectral SPD/CMF tables, gamut boundary data. (The LUT-free
**analytic** AgX engine remains the recommended GPU path.)

## 5. Deterministic math: full transfer coverage, bit-exact across backends

`ALWAN_DETERMINISTIC=1` replaces the transfer functions with polynomial
evaluations. The GPU now has the **complete determinized transfer set**, sharing
the C `_f32` coefficient tables (HLSL `frexp`/`ldexp` intrinsics, `precise`
Horner to block mad-fusion, mirroring the C `FP_CONTRACT`-off contract):

| transfer | C (det) | GPU (det) |
|----------|:------:|:---------:|
| sRGB EOTF | polynomial | polynomial; **bit-exact, WARP-verified 64/64** |
| sRGB OETF | polynomial | polynomial; **bit-exact, WARP-verified 64/64** |
| BT.2020 OETF | polynomial | polynomial; **bit-exact, WARP-verified 64/64** |
| BT.2020 EOTF | polynomial | polynomial; **bit-exact, WARP-verified 64/64** |
| general `pow` / `log2` | libm (not determinized in C either) | hardware (ULP vs libm) |

(`alwan_dev/hlsl_regression/det_tf_runner.cpp` is the WARP bit-exactness test.)
The f64 coefficient tables are guarded C-only. **Cross-backend parity:** det-C
and det-GPU are bit-exact on every determinized transfer; ULP-close elsewhere
(hardware vs libm `pow`/`log2`, the same non-guarantee C-det itself has).

## 6. Host-only setup inside GPU-enabled cores

Even in a GPU-capable core, per-call *setup* that does matrix inversion or table
building stays on the CPU (`#if ALWAN_BACKEND == ALWAN_BACKEND_C`) and feeds the
shader as **uniforms**; only the per-pixel kernel runs on GPU. Example: AgX
`agx_tip_scales` + `alwan_agx_build_geometry` (host) produce the sigmoid scales
and inset/outset matrices consumed by the GPU `agx_render`.

---

## Backend-specific differences (available everywhere, but *differ*)

- **GLSL**: no `double`, so `alwan_f64` aliases to `float`; `atan2(y,x)` = `atan(y,x)`;
  `ALWAN_INLINE` is empty (file-scope functions).
- **Halide**: real `double`; kernels build symbolic `Expr`s; branchless
  `ALWAN_SELECT` becomes `Halide::select` (raw scalar `if` is not allowed).
- **HLSL**: `in` / `out` are reserved keywords (kernels must avoid them as
  identifiers); `ALWAN_TYPE_DEF` is `typedef`; `precise` blocks mad-fusion (the C
  path relies on `FP_CONTRACT` off for the same reason).
- **`ALWAN_CORE_*` vocabulary**: the GPU alias set (`alwan_core_aliases.inc`) is
  complete for everything the cores currently use. Note there is **no
  `ALWAN_CORE_ASIN`** (kernels use `pi/2 - acos(x)`); natural log is `ALWAN_CORE_LN`.

---

## Quick reference: feature x backend

| feature | C | HLSL | GLSL | Halide |
|---------|:-:|:----:|:----:|:------:|
| f64 / double precision | [x] | [ ] | [ ] | [ ] |
| strided / bulk / typed / SIMD API | [x] | [ ] | [ ] | [ ] |
| per-pixel color-model kernels | [x] | [~] | [~] | [~] |
| AgX **analytic** render | [x] | [x] run-verified | [~] | [~] |
| **JP2499** (full pipeline) | [x] | [x] compile-verified | [~] | [~] |
| Nayatani95 / normalize | [x] | [x] compile-verified | [~] | [~] |
| AgX **views** (embedded LUTs) | [x] | [~] data compiles; sampler/dispatch live in `api/*.c` | [~] | [~] |
| deterministic sRGB OETF+EOTF | [x] | [x] bit-exact, run-verified | [~] | [~] |
| deterministic BT.2020 OETF+EOTF | [x] | [x] bit-exact, run-verified | [~] | [~] |

Legend: [x] available | [ ] not available | [~] has a GPU branch / compiles but **unverified at runtime**.

*f64 note:* the GPU backends run a single-precision path, so `_f64` is C-only in
practice. HLSL (dxc) and Halide *have* a real `double` type, but the shipped GPU
instantiation is single precision; GLSL ES has no `double` at all.
