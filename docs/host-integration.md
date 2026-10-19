# Host integration

What a host (a node graph, a compositor, a library such as Suwar) can rely on when it calls
alwan: who allocates, which calls may run on several threads, what a pixel buffer may look
like, which backend runs which core, and which results are bitwise reproducible. Where a rule
is not yet met in full, the gap is listed with the rule.

`foundation.md` covers the headers a library built on alwan includes. `map.md` covers the
map families one by one.

## Allocation

**Contexts.** `alwan_create` takes an optional allocator pair. `alwan_ctx_alloc` and
`alwan_ctx_free` route through it; a NULL context means the default allocator.

**Mapping functions allocate nothing.** Every `_map_interleave`, `_map_interleave_ex`,
`_map_planar` and `_map_planar_ex` works in stack tiles (96 to 192 KB at most) and writes into
the caller's buffer. The OCIO grading maps with `params == NULL` used to allocate their
defaults on every call; they now build them on the stack.

One exception remains, in a build with `ALWAN_EMBED_DATA=0` only: the first call that reads a
table (the AgX and Tony McMapface view maps, spectral upsampling, any SPD lookup) loads it from
the data pack with the default allocator. Call `alwan_data_preload` (or `alwan_data_load`) at
initialisation, and no later call allocates.

**Objects.** The `_create` / `_destroy` pairs (SPD, CMYK model and inverse, CLF, luminaire,
sky, iridescence, fluorescence, spectral film, Planckian table, colour solid, chart,
wavelength tables) take a context, but today allocate with the default allocator on both
sides. The pairing is consistent, so nothing is freed with the wrong allocator; a host with a
budget allocator does not yet see these allocations. Moving each pair onto its context, with
the object keeping the context it was created with, is open work.

These public functions allocate scratch memory and take no context at all: the CCM fit,
leave-one-out and selection, `alwan_tps3d_fit`, the display model and GOG fits, the skin
model, thin film, `alwan_select`, the image metrics and SSIM, two camera functions and one
pattern function. They free what they allocate before returning.

**Failure.** An allocation that fails returns `ALWAN_E_NOMEM`; a create function frees what it
had built and returns NULL or the status. Outputs may be partly written. Known exceptions:
`alwan_cri_ra_f32` returns -1 on failure, and the CLF writer returns `ALWAN_E_INVALID`.

**Data tables.** Tables loaded by `alwan_data_load` live in the memory of the context passed
to it. `alwan_destroy` does not free them: call `alwan_data_free` before destroying that
context. A table that a later read loads on demand uses the default allocator, and
`alwan_data_free` releases each table with the allocator that made it.

## Threads

- A context is read-only once set up. Its setters (`alwan_ctx_set_aces_interp`) run before it
  is shared.
- Every map and scalar function is safe to call from any number of threads at once, on
  disjoint outputs, with shared read-only inputs and parameters.
- The ACES 2.0 gamut-compression cache is per thread. (It was one shared cache, and two
  threads with different output presets could read torn tables.)
- On-demand table loads are safe from many threads at once. `alwan_data_load`,
  `alwan_data_free`, `alwan_data_set_root` and `alwan_data_preload` are not synchronised with
  readers: run them when no other thread is inside the library.
- The CLF, `.cube` / LUT format and import/export functions switch `LC_NUMERIC` to "C" around
  their parsing and printing, for the calling thread only (`_configthreadlocale` on Windows,
  `uselocale` on POSIX). Other threads, and the host's own locale, never see the switch. (It
  was a process-wide `setlocale`, so two imports on two threads could leave each other, or the
  host, in the wrong locale.)

## Pixel buffers

**Formats.** `alwan_pixel_format` is U8, U16, F16, F32 or F64. An `_ex` map given any other
value returns `ALWAN_E_INVALID`. Integer formats always mean [0, 1]: U8 is v / 255 and U16 is
v / 65535 on load. On store, values are scaled, rounded half up, and saturated; NaN stores
as 0. F16 stores round to nearest even. A store from an f64 kernel to F16 goes through f32
first, so it can round twice; that is at most one F16 ULP.

**Strides.** Every stride is in bytes. An interleaved pixel stride must be at least the size
of its channels (3 or 4 times the channel size); the `_ex` maps check this and return
`ALWAN_E_INVALID`. Rows (`alwan_image_convert`) have their own byte stride and may carry
padding. Pass an explicit stride: a stride of 0 does not mean "tightly packed" everywhere
(see `alwan_decisions.md` on stride 0).

**Count.** A count of 0 is a valid call that does nothing; most maps return `ALWAN_OK`, a few
older ones `ALWAN_E_INVALID`. Do not rely on either.

**Aliasing.** A map may run in place, with `out == in`, when the input and output have the same
format and the same stride. Any other overlap is undefined: tiles are loaded, converted and
stored in blocks, so a shifted overlap reads values it already wrote.

**Alpha.** Three-channel maps read and write three channels. With a 4-channel stride they leave
the fourth untouched; they do not copy it from the input. `alwan_image_convert_rgba` handles
alpha explicitly through `alwan_alpha_mode`.

**Loads and stores.** The tile loaders read exactly the bytes of the pixels they convert. (The
U8 3-channel SIMD path used to read 4 bytes past the last pixel of a block.)

**Adaptation.** `alwan_image_convert`, the 3D LUT bakes and the CLF export apply a Bradford
adaptation whenever the source and destination white points differ by more than 1e-6. A NULL
context used to skip it silently.

## Backends

Atelier owns the GPU device, its queues and its memory; alwan supplies cores that compile in
each shading language and reads its tables through accessors the shader binds
(`ALWAN_TABLE_READ(i)` and the family-specific readers). What is compiled and what is run
differ:

| Backend | Compiled | Run and compared with C |
|---|---|---|
| C scalar | every core | every test suite |
| SIMD (SSE2, AVX, AVX2, NEON) | the map kernels of colorspace, oklab, ipt, jzazbz, ictcp, rgb, convenience, extended, gamut, colour correction, vision, CAM16, mat3 | suite 88 and suites 69/70, within the tolerances below |
| HLSL (dxc and fxc) | 50 of 52 cores, gated in CI | tables, vision, nn: bitwise or within 2 to 4 ULP; AgX and the deterministic transfer functions within 1e-5 / 1e-6, by hand |
| CUDA | every core header parses; manual | oklab, Lab, ICtCp PQ, Jzazbz, sRGB OETF and EOTF, f32 and f64 |
| OpenCL | every core header; manual | the same six kernels, f32 only |
| GLSL | **not compiled**: only a keyword lint runs | none |
| Halide | **not compiled**: `alwan_halide.h` is in no build | none |

Treat GLSL and Halide as unsupported until a compile gate exists. For the other GPU backends, a
core with no run is expected to work (it compiles, and its arithmetic is the C code), but only
the cores in the right-hand column have been compared with C. The full per-core matrix, with
the evidence for each cell, is `backend_matrix.csv` in the integration audit.

## Pointwise kernel contract

An alwan pointwise operation comes in up to four layers:

1. **Core** (`*_core.inc`, `_v` suffix): a pure function of one pixel and a parameter block,
   no pointers on the GPU backends, no allocation, no global state. This is what a shader or
   another library's kernel calls.
2. **Parameter preparation** (`*_prepare`, `*_init`, `*_create`): everything that depends on
   the settings and not the pixel (matrices, adaptation, tables, the ACES 2.0 gamut tables).
   Run once per graph node, its result is shared read-only by every tile.
3. **Map wrapper** (`_map_interleave`, `_map_planar`, `_ex`): strides, formats, tiles, SIMD.
   No allocation; it calls the prepared parameters and the core.
4. **Global statistics**: operations that need the whole image first (illuminant estimation,
   histogram matching, auto exposure) split into a reduction that returns a small parameter
   block and a pointwise apply. The spatial image algorithms live in Suwar, not here.

A node graph schedules layer 2 once, layer 3 per tile, and layer 4's reduction as its own node.

## Determinism

Two different claims, kept apart:

- **Bitwise.** The deterministic build (`Release_Det`, `ALWAN_DETERMINISTIC=1`) replaces libm
  with alwan's polynomial math and disables FMA contraction. It is held bitwise against itself
  across the six CI runners for the sections listed in `determinism.md`
  (`empirical_sections.grep`). On the GPU, CUDA (`--fmad=false`, f32 and f64) and OpenCL (f32)
  match the C deterministic build to 0 ULP for the six kernels above. The HLSL table readers
  match bitwise in a fast build.
- **Within a tolerance.** Everything else: the SIMD map kernels against the scalar cores (fast
  build: 5e-5 absolute in f32; the f64 colour conversions in the deterministic build allow up
  to 4 ULP), HLSL AgX (1e-5), the HLSL deterministic transfer functions (1e-6), nn exp rows
  (4 ULP).

In the deterministic build the map kernels run at SIMD width 1, so suite 88 compares scalar
with scalar there; only the transfer-function apply paths keep vector code.
`determinism.md` and `backends_limits.md` used to claim more (AgX bitwise on HLSL, "25
conversions at 0 ULP" in suite 88, all twelve primitives bitwise on OpenCL). No gating test
backed those claims, and the text now says what was measured and how.
