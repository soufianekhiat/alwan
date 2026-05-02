# Contributing to Alwan

Thanks for your interest. This document covers the conventions enforced
across the public C API. Compliance is checked by
[`tools/api_convention_survey.py`](tools/api_convention_survey.py) and the
build expects 100%.

---

## Parameter convention (v2.0)

Every public declaration in `src/alwan/alwan.h` must satisfy four rules:

| # | Rule |
|---|------|
| **R1** | `alwan_ctx *ctx` is the last parameter when present. |
| **R2** | Each `*_stride` parameter immediately follows the buffer it strides. |
| **R3** | Output buffers appear before input buffers; no out-after-in inside the buffer-stride block. |
| **R4** | Sizing parameters (`count`, `width`, `height`) come after the buffer-stride block, never before any buffer. |

A fifth informal rule:

> **R5.** Pure value-typed math functions (suffix `_v`, return-by-value, no
> pointers) take no `ctx`. `ctx` is only for I/O, allocation, and
> transforms that may need state.

### Why this convention

- **Memcpy ordering** (`out, out_stride, in, in_stride, count`) is familiar
  from `memcpy_s`, BLAS, image libraries, and matches how callers think:
  *"I'm writing to X with this stride, reading from Y with that stride,
  this many elements."*
- **`ctx` last** keeps it visually a footnote — it's plumbing, not part of
  the operation.
- **Stride next to its buffer** removes any ambiguity over which stride
  applies to which pointer when there are multiple buffers (delta_e batch,
  planar 3-channel functions).
- **Out before in** mirrors the assignment direction (`dst = src`) and
  prevents accidental swap bugs.

---

## Canonical signatures

```c
// Single buffer pair (interleave, 1D batch)
fn(out, out_stride, in, in_stride, count, [extras]..., [ctx]);

// Three-channel planar
fn(o0, out_stride, o1, o2,
   i0, in_stride, i1, i2,
   count, [extras]..., [ctx]);

// 2D image
fn(dst, dst_row_stride,
   src, src_row_stride,
   width, height, [extras]..., [ctx]);

// Two-input batch (delta-E style)
fn(out, in1, in1_stride, in2, in2_stride, count, [extras]...);

// Typed _ex variant — pixel formats live in the extras tail
fn_ex(out, out_stride, in, in_stride, count,
      out_fmt, in_fmt, [extras]..., [ctx]);
```

### Real examples (from the codebase)

```c
int alwan_oetf_apply_f64(alwan_f64 *encoded_out, size_t out_stride,
                         alwan_f64 const *linear_in, size_t in_stride,
                         size_t count, alwan_transfer_function tf);

int alwan_xyz_to_lab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride,
                                     alwan_f64 *out_ch1, alwan_f64 *out_ch2,
                                     alwan_f64 const *in_ch0, size_t in_stride,
                                     alwan_f64 const *in_ch1, alwan_f64 const *in_ch2,
                                     size_t count, alwan_xyz_f64 const *white_xyz);

int alwan_image_convert_f64(void *dst, size_t dst_row_stride,
                            void const *src, size_t src_row_stride,
                            size_t width, size_t height,
                            alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt,
                            alwan_rgb_space_desc_f64 const *src_space,
                            alwan_rgb_space_desc_f64 const *dst_space,
                            alwan_ctx *ctx);

int alwan_delta_e_2000_f64_batch(alwan_f64 *delta_e_out,
                                 alwan_f64 const *lab1_in, size_t in1_stride,
                                 alwan_f64 const *lab2_in, size_t in2_stride,
                                 size_t count);

int alwan_simulate_cvd_f64_map_planar(alwan_f64 *o0, size_t out_stride,
                                       alwan_f64 *o1, alwan_f64 *o2,
                                       alwan_f64 const *i0, size_t in_stride,
                                       alwan_f64 const *i1, alwan_f64 const *i2,
                                       size_t count,
                                       alwan_cvd_type cvd_type, alwan_f64 severity);
```

---

## Buffer naming

The survey only recognises a parameter as a "buffer" when its name matches
one of these patterns:

- Exact: `out`, `in`, `src`, `dst`, `buf`
- Numbered prefix (short): `o0`, `o1`, `o2`, `i0`, `i1`, `i2`
- Numbered prefix (verbose): `out0`, `out1`, `out2`, `in0`, `in1`, `in2`
- Suffixed: `*_in`, `*_out`, `*_buf`
- Channel form: `out_ch0`, `out_ch1`, `out_ch2`, `in_ch0`, `in_ch1`, `in_ch2`

Bare descriptive names (`rgb_data`, `linear`, `encoded`, `lab1`, `in0`,
`in1`, `in2`) **fail** the pattern even when they're clearly buffers.
Either rename to the canonical form (`linear_in`, `encoded_out`, `lab1_in`)
or use the short numbered form (`i0`/`i1`/`i2`).

### Buffers vs. value-input pointers

A `const *` to a small struct is a **knob**, not a bulk buffer:

```c
alwan_xyz_f64 const *white_xyz   // knob — goes in the extras tail
alwan_rgb_f64 const *gain         // knob
alwan_mat3x3_f64 const *matrix    // knob
alwan_rgb_space_desc_f64 const *space  // knob
```

vs.

```c
alwan_f64 const *in              // bulk buffer — goes in the buffer-stride block
alwan_f64 const *in_ch0
void const *src
```

The survey distinguishes them by element type — pointers to scalar types
(`alwan_f32`, `alwan_f64`, `float`, `double`, `void`, `uint8_t`,
`alwan_map_lane`, …) and matching names are buffers; pointers to known
struct types are knobs.

---

## Extras-tail ordering

Within the extras tail (everything after `count`/`width`/`height`):

1. **Pixel formats first**: `out_fmt` before `in_fmt`.
2. **Then descriptor structs**: matrices, white points, RGB-space
   descriptors, lift/gamma/gain triples.
3. **Then enums**: `alwan_cvd_type`, `alwan_ycbcr_standard`, etc.
4. **Then scalar tuning params**: `severity`, `red_lights`, `green_lights`,
   `blue_lights`, `bandpass_nm`.
5. **`alwan_ctx *ctx`** is always last.

This is a soft convention — the survey doesn't enforce ordering within the
tail — but it keeps signatures predictable.

---

## Naming patterns for function variants

Within a family (e.g. `alwan_xyz_to_lab`), the name suffix encodes shape:

| Suffix | Shape |
|---|---|
| (none) | Single value, takes pointers to small structs |
| `_v` | Value-typed math; return-by-value, GPU-compatible |
| `_f32` / `_f64` | Single-precision / double-precision |
| `_map_interleave` | Interleaved AoS buffer of 3-channel pixels |
| `_map_planar` | Three separate per-channel buffers |
| `_map_interleave_ex` / `_map_planar_ex` | Typed `void*` variant accepting any `alwan_pixel_format` |
| `_batch` | Per-pixel scalar output (e.g. delta-E) |

`_ex` functions dispatch internally to the matching `_f32` or `_f64`
kernel via the `ALWAN_EX_DELEGATE_*` and `ALWAN_PLANAR_EX_DELEGATE_*`
macros in `src/alwan/map/alwan_map_internal.h`.

---

## Error contract

Functions returning `int` use the `alwan_status` enum, never raw `0`/`-1`:

```c
ALWAN_OK         =  0
ALWAN_E_INVALID  = -1
ALWAN_E_NODATA   = -2
ALWAN_E_RANGE    = -3
ALWAN_E_NOMEM    = -4
ALWAN_E_DIVZERO  = -5
```

Functions that have no failure mode return `void`. Pure math functions
return their result by value (suffix `_v`).

---

## Build systems

- **Sharpmake** (in [`buildsystem/sharpmake/`](buildsystem/sharpmake/)) is
  the reference. Drive builds through Sharpmake unless otherwise asked.
- **CMake** (root `CMakeLists.txt`) replicates Sharpmake. Sharpmake
  auto-discovers source files via `SourceRootPath` glob; CMake requires
  explicit listing in `ALWAN_SOURCES` and `tests/CMakeLists.txt`.

```sh
# Sharpmake (reference)
# … see buildsystem/sharpmake/README

# CMake
cmake -S . -B build
cmake --build build
```

---

## Architecture in one paragraph

Two-tier: **core `_core.h`** is header-only, uses `ALWAN_INLINE`, is
GPU-compatible, and contains the actual math. **API `.c`** files are the
thin shell — null checks, stride loops, dispatch to core. Core functions
use the `_v` suffix and return by value. All literals are wrapped in
`ALWAN_LITERAL()`, math in `ALWAN_POW`/`ALWAN_EXP`/`ALWAN_LOG2`/etc.
Branching uses `ALWAN_SELECT()` (branchless) instead of `if/else` where
possible.

---

## Generated data

Magic numbers must come from Python scripts in [`./gendata/`](gendata/),
run by `./generate_data.ps1`. Published formula constants from
papers/standards are acceptable inline in core headers, but anything that
could be regenerated (CMFs, illuminants, basis spectra, matrices) must be
gendata output.

---

## Before submitting

```sh
# Check API compliance — must report 100%
python tools/api_convention_survey.py

# Check core .h / .inc parity — must report no drift
python tools/check_core_parity.py

# Build (Sharpmake or CMake)
cmake --build build

# Run tests
ctest --test-dir build
```

If you change anything in `gendata/`, regenerate before submitting:

```sh
./generate_data.ps1
```
