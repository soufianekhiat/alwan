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

**Objects.** Every object a `_create`, `_load` or `_import` function returns lives in the
memory of the context it was given, and keeps that context: the CMYK model and inverse, CLF,
luminaire, chart, sky, iridescence, fluorescent material, spectral film, wavelength sampler,
Planckian table, colour solid and the RGB fit state. Its `_destroy` frees with that stored
context, whatever context it is handed, so a destroy given NULL (or another context) still
frees each block with the allocator that made it. The context must outlive every object made
with it. Suite 309 checks this for two pairs, and fails each of their allocations in turn.

One exception: an SPD. `alwan_spd` is a public struct (values, range, count) with no room for
a context, so `alwan_spd_create`, `alwan_spd_destroy` and every function that fills an SPD use
the default allocator on both sides. The Bruneton sky precompute also keeps its scratch on the
default allocator; it frees it before `alwan_sky_create` returns.

**Scratch.** `docs/capability_manifest.json` marks every operation that may allocate
(`"allocates": "yes"`, from a scan of the implementation and everything it calls). Scratch is
always freed before the function returns, so the allocator it comes from matters only for a
budget. It comes from the context in the flip metric, the gamut volume, the splines, the data
loader, the fluorescence and iridescence solvers, the sky bake, the wavelength weights, the
object loaders above and the experimental formation, grain and halation code. Elsewhere it
comes from the default allocator, with or without a context argument: the SPD integrations,
the camera and IDT solves, the CCM fit, leave-one-out and selection, `alwan_tps3d_fit`, the
display model and GOG fits, the skin model, thin film, `alwan_select`, the image metrics and
SSIM, the pattern renderer and the CLF writer.

**Ownership of results.** Outputs are the caller's buffers. No function returns a block for
the caller to free, except the objects above (freed by their `_destroy`) and the SPDs a
function fills (freed by `alwan_spd_destroy`). The pointers `alwan_data_get_*` hands out point
at alwan's tables: never free them.

**Failure.** An allocation that fails returns `ALWAN_E_NOMEM`; a create function frees what it
had built and returns NULL or the status. Outputs may be partly written. A size whose product
overflows is refused before allocating (`ALWAN_E_NOMEM`, or `ALWAN_E_RANGE` where a documented
limit applies). One exception: `alwan_cri_ra_f32` / `_f64` return the index itself, -1 on any
failure; `alwan_cri_specification_*` returns the status. A host `free_cb` may be handed NULL,
as `free` may.

**One memory manager for alwan and Suwar.** Suwar's `suwar_allocator` carries a user pointer
and the block size; alwan's callbacks carry neither. Suwar bridges the two, so a host passes
its manager once:

```c
/* the host's manager: a budget, an arena, a tracking allocator */
static void *host_alloc(void *user, size_t size, size_t align) { return my_alloc(user, size, align); }
static void  host_free(void *user, void *p, size_t size)       { my_free(user, p, size); }

suwar_allocator mine = { host_alloc, host_free, &my_heap };
suwar_allocator prev = suwar_set_thread_allocator(&mine);   /* this thread's suwar calls */

/* an alwan context whose callbacks reach the same manager */
alwan_config acfg = { suwar_thread_alloc, suwar_thread_free, NULL, 0 };
alwan_ctx *actx = alwan_create(&acfg);

/* ... alwan calls with actx, suwar calls on this thread ... */

alwan_destroy(actx);              /* after every object made with actx */
suwar_set_thread_allocator(&prev);
```

Each block goes back to the allocator that made it. `suwar_create` with no callbacks does the
same internally, and `suwar_ctx_alwan` returns the alwan context it owns, for alwan calls that
should share Suwar's allocator. A host that prefers one allocator per context passes the same
pair of functions to `alwan_config` and `suwar_config` instead.

**Data tables.** Tables loaded by `alwan_data_load` live in the memory of the context passed
to it. `alwan_destroy` does not free them: call `alwan_data_free` before destroying that
context. A table that a later read loads on demand uses the default allocator, and
`alwan_data_free` releases each table with the allocator that made it. Nothing else is
cached: the only per-thread state is the ACES 2.0 gamut-compression table (below), which lives
in thread-local storage and needs no freeing.

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
context used to skip it silently. `alwan_rgb_convert` and `alwan_rgb_convert_map_interleave`
still adapt only when given a context, as in 2.0.0 (the migration manifest lists the
difference); pass one, or use `alwan_image_convert_prepare`.

**Ranges.** With `ALWAN_NORMALIZE_RANGES=1` (the CMake default) the C API reports every
bounded channel (L of Lab, hue, saturation, ...) on [0, 1]; the unbounded opponent axes (Lab
`a`, `b`, Oklab `a`, `b`, chroma) keep their native signed range. With 0 everything is native.
It is fixed when alwan is built, reaches consumers through the `Alwan::alwan` target, and
`alwan_get_build_info` reports it; a host that stores values in a document should record which
it was. The cores (`_v`) always work in native ranges whatever the switch: a fused kernel
that must match the C API applies `ALWAN_NORMV_<SPACE>` / `ALWAN_DENORMV_<SPACE>` at its own
boundary (`ranges.md`, `configuration.md`). Suwar's reference values assume native ranges.

### Which maps run on a register's own buffer

| Kind | Buffer it accepts | Use on a typed, strided register |
|---|---|---|
| `_map_interleave_ex`, `_map_planar_ex` | any `alwan_pixel_format`, byte strides per pixel (and per plane), in and out formats independent | directly |
| `alwan_image_convert`, `_rgba`, `_data` | any format, a byte stride per row, tightly packed pixels within a row | directly, for row-contiguous registers |
| `_f32_map_interleave`, `_f64_map_interleave`, `_map_planar` | the native float type only, byte strides | directly for F32 / F64 registers; other formats need a staging conversion (or the `_ex` form) |
| `alwan_oetf_apply` / `alwan_eotf_apply` and the other strided array functions | native float, byte strides, a count | directly for F32 / F64 |
| scalar functions (`alwan_xyz_to_lab_f64`, ...) | one value | per pixel, in the host's loop |
| image operations (`"scope": "image"` in the manifest: the picture formation, spatial gamut maps, metrics, global tone map) | tightly packed interleaved RGB, `width * height * 3`, no stride | a staging copy for a strided or planar register |

`docs/capability_manifest.json` records each operation's layouts, formats and stride unit.

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

**Fusing.** A fused Halide stage or compute shader calls layer 1 and receives layer 2's result
as constants; alwan never allocates on or dispatches to a device. The manifest's
`fusion_areas` gives, for colour-space conversion, chromatic adaptation, transfer functions,
gamut mapping, exposure, tone mapping, appearance models, spectral conversion and LUT
evaluation, the preparation functions, the cores and the wrappers, and what has no core yet.
For a colour-space conversion, `alwan_image_convert_prepare` returns the plan
`alwan_image_convert` itself runs (one matrix, with the adaptation folded in, and the two
transfer-function enums), as a struct with no pointers; the manifest's `transfer_functions`
maps each enum to its core. A kernel that runs `oetf(combined * eotf(rgb))` with those cores
computes what the converter computes.

What has no core is C only, and a fused graph must run it as a separate CPU node: the complete
view transforms, the ACES 1.x and 2.0 output transforms as a whole (their parts have cores),
gamut methods beyond clip and hue-preserving, the spectral upsamplers, Hellwig 2022's
parameter split, and every image operation.

## Experimental picture formation

`alwan_picture_form_*` and `alwan_gamut_map_spatial` sit under `ALWAN_WITH_FORMATION`, in
`src/alwan/experimental/`. They are **experimental**: not part of the stable 3.x API, not
deterministic (no det-dump section covers them), and their signatures may change. They are
image operations, never pointwise kernels.

- **Buffers.** Interleaved RGB, tightly packed, `width * height * 3`; fields are
  `width * height` scalars in stops. No strides. All buffers are the caller's.
- **Allocation.** Each call allocates its scratch through the context, which must not be NULL
  (`ALWAN_E_INVALID`). The local solver takes about 22 scalars per pixel; the global solver 3
  per pixel when it forms a picture. `alwan_picture_form_global_exp_field` with `out == NULL`
  allocates nothing: it computes the field alone, in `e_out`. `alwan_picture_form_local_exp_lag`
  allocates nothing.
- **Picture output.** Optional in the `_field`, `_field_m`, `_resume` and `_resume_m` calls
  (`out` may be NULL), required in the plain calls and in `_apply`. `e_out` is required.
- **The field on its own.** The global field is a per-pixel function of the input and one
  pivot: with `pivot > 0` it is pointwise; with `pivot <= 0` the pivot is the mean of the
  image's carrier, a reduction over the whole image. The local field needs the whole image (an
  evidence-gated smoothing, a Jacobi solve, an order-repair solve that forms the picture three
  times per iteration), so a NULL `out` saves the copy and not the work.
  `alwan_picture_form_local_exp_inputs` returns the solver's inputs without a picture.
- **The picture from a field.** `alwan_picture_form_local_exp_apply` forms the picture from a
  given field, as the solve's own last step does (suite 108 holds them equal). It is pointwise
  for `COMPLETE_HEMI_LOOK` and `COMPLETE_PEAK`; other formation methods read the whole image.
- **Regions.** No region-of-interest form: every call reads the whole image.
- **Backends.** C only, f32 and f64. No core, no GPU code.
- **Tests.** Suites 97 to 100 and 108 check structure (order, monotonicity, frame
  consistency) at 1e-15 to 1e-2; the `_field` and `_apply` checks run in f64; suite 309 checks
  that the global field alone matches the full solve's bit for bit and allocates nothing.

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

By backend and precision, in four kinds: **bit-exact** (run and compared at 0 ULP),
**tolerance** (run and compared within a stated bound), **compiled** (the code compiles; no
run compares it with C), **unsupported** (no such build).

| Backend | f32 | f64 |
|---|---|---|
| C scalar, deterministic build | bit-exact across the six CI runners, for the `determinism.md` sections | bit-exact, same sections |
| C scalar, fast build | tolerance (libm differs by platform) | tolerance |
| SIMD map kernels (fast) | tolerance: 5e-5 absolute against scalar | tolerance |
| SIMD map kernels (deterministic) | scalar code (width 1); the transfer-function apply paths bit-exact | colour conversions up to 4 ULP; transfer functions bit-exact |
| HLSL (D3D12) | bit-exact: table readers, Machado matrices, nn tensor kernels; tolerance: AgX 1e-5, deterministic sRGB / BT.2020 TFs 1e-6, vision 2 ULP, nn exp 4 ULP; compiled (dxc + fxc in CI): 50 of 52 cores | unsupported |
| CUDA (`--fmad=false`) | bit-exact (deterministic) and tolerance (fast): xyz_to_lab, xyz_to_oklab, xyz_to_ictcp_pq, xyz_to_jzazbz, sRGB OETF / EOTF; compiled by hand: every core | the same six kernels, bit-exact and tolerance; compiled by hand |
| OpenCL 2.0 (correctly rounded divide and sqrt) | the same six kernels, bit-exact and tolerance; compiled by hand: every core | unsupported (fp64 never tested) |
| GLSL | compiled: none (a keyword lint only) | unsupported |
| Halide | compiled: none (never built) | unsupported |

None of the GPU comparisons runs in CI except the HLSL table, vision and compile gates; the
rest are run by hand (`backends_limits.md` names the scripts). A core that compiles for a
backend is not thereby equal to C on it.
`determinism.md` and `backends_limits.md` used to claim more (AgX bitwise on HLSL, "25
conversions at 0 ULP" in suite 88, all twelve primitives bitwise on OpenCL). No gating test
backed those claims, and the text now says what was measured and how.
