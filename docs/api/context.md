# Context Management API

Context objects (`alwan_ctx`) manage library state and memory allocation.

Functions that take a `ctx` (e.g. `alwan_rgb_get_space_descriptor_{T}`, `alwan_rgb_convert_{T}`) accept it as the handle through which the **allocator** and (in a future runtime mode) disk-loaded data are reached. In the current **embedded** build the RGB color-space registry (primaries/whitepoint/transfer-function descriptors) is compiled into the binary from `src/alwan/data/**`, so descriptor lookups index static tables and ignore `ctx` (it may even be `NULL`). The context does **not** load anything from disk at runtime. Where `ctx` matters today: it supplies the allocator for functions that allocate (e.g. SPD/LUT routines) and gates optional work such as the chromatic-adaptation step inside `alwan_rgb_convert_{T}` (passing `NULL` skips adaptation rather than erroring). Per the v2.0 parameter convention, `ctx` is always the **last** argument (or absent on `_v` value-typed math).

> **Note:** Runtime data loading (`runtime_data_root`) is NOT implemented. Only embedded mode (`ALWAN_EMBED_DATA=1`, the default) is supported. Runtime mode is planned for alwan 3.0.0.

---

## Functions

### alwan_create

```c
alwan_ctx* alwan_create(const alwan_config *config);
```

Creates and initializes a new Alwan context.

**Parameters:**
- `config`: Configuration structure, or `NULL` for defaults

**Returns:**
- Pointer to new context on success
- `NULL` on allocation failure or initialization error
- `NULL` for a configuration that cannot be meant: a non-zero `flags`, or one
  of `alloc_cb` / `free_cb` without the other

**Default behavior (config = NULL):**
- Uses system `malloc`/`free` for allocation
- Embedded data mode (no runtime I/O)

**Example:**
```c
// Simple initialization
alwan_ctx *ctx = alwan_create(NULL);
if (!ctx) {
    // Handle error
}
```

**Thread Safety:** Safe to call from multiple threads (creates independent contexts)

---

### alwan_destroy

```c
void alwan_destroy(alwan_ctx *ctx);
```

Destroys a context and frees all associated resources.

**Parameters:**
- `ctx`: Context to destroy (can be `NULL`, in which case this is a no-op)

**Example:**
```c
alwan_destroy(ctx);
ctx = NULL;  // Good practice
```

**Thread Safety:** Must not be called while other threads are using the context

---

## Types

### alwan_ctx

```c
typedef struct alwan_ctx alwan_ctx;
```

Opaque context structure. Internal details are not exposed.

**Lifetime:**
- Created by `alwan_create()`
- Destroyed by `alwan_destroy()`

**Usage:**
- Pass to functions that require context (e.g., RGB space operations)
- Can be shared across threads for read-only operations
- One context per thread recommended for write operations

---

### alwan_config

```c
typedef struct {
    alwan_alloc_fn alloc_cb;          // Optional custom allocator (NULL = default); set both or neither
    alwan_free_fn  free_cb;           // Optional custom deallocator (NULL = default)
    char const *runtime_data_root;    // Reserved (runtime data loading is planned); currently ignored
    uint32_t flags;                   // Reserved for future use (must be 0)
} alwan_config;
```

Configuration for context creation.

**Fields:**

#### `alloc_cb`
Custom allocation callback for **context-lifetime** allocations: the context
object itself and any data it owns (e.g. a copied `runtime_data_root`).

> **Scope:** `alloc_cb`/`free_cb` govern context-lifetime allocations. The
> transient scratch buffers used inside colour-math routines (SPD integration,
> matrix solves, LUT baking, file I/O, etc.) are routed through the
> compile-time `ALWAN_ALLOC`/`ALWAN_FREE` hooks instead, which are the canonical
> library-wide allocation override. Define them at build time to replace the
> allocator everywhere:
> ```c
> #define ALWAN_ALLOC(sz, align) my_aligned_alloc((sz), (align))
> #define ALWAN_FREE(p)          my_free((p))
> ```

**Signature:**
```c
void* alloc_cb(size_t size, size_t align);
```

**Parameters:**
- `size`: Number of bytes to allocate
- `align`: Required alignment (power of 2)

**Returns:**
- Pointer to aligned memory, or `NULL` on failure

**Default:** `alwan_default_alloc` (aligned allocation; falls back to `malloc`)

**Example:**
```c
void* my_alloc(size_t size, size_t align) {
    return _aligned_malloc(size, align);
}

alwan_ctx *ctx = alwan_create(&(alwan_config){
    .alloc_cb = my_alloc,
    .free_cb = _aligned_free
});
```

#### `free_cb`
Custom deallocation callback.

**Signature:**
```c
void free_cb(void *ptr);
```

**Parameters:**
- `ptr`: Pointer to free (can be `NULL`)

**Default:** `alwan_default_free` (matches `alwan_default_alloc`; uses `_aligned_free` on MSVC, `free` elsewhere)

#### `runtime_data_root`
Reserved. Runtime data loading is NOT implemented (planned for alwan 3.0.0).

**Type:** `const char*`

**Default:** `NULL`

This field is currently ignored. Building with `ALWAN_EMBED_DATA=0` produces a compile-time error.
Set to `NULL` until runtime loading is implemented in alwan 3.0.0.
---

## Library Version

### alwan_version_string

```c
char const *alwan_version_string(void);
```

Returns the version of the linked library binary as `"major.minor.patch"`.

**Parameters:** none.

**Returns:**
- Pointer to a static string literal. Never `NULL`. Current value `"2.0.0"`.

**Ownership and lifetime:** static storage duration. Do not free it, do not
write through it. Valid for the whole process lifetime.

**Thread Safety:** safe from any thread, concurrently. The body is one
`return ALWAN_VERSION_STRING;` (`alwan_context.c:148-150`) with no state.

`alwan_context.c` is compiled *into* the library, so the returned string is the
binary's own build-time version. That makes the header's dynamic-loading advice
usable: compare the returned string against the `ALWAN_VERSION_STRING` your
translation unit compiled with to detect a header/binary skew.

> **`ALWAN_VERSION_STRING` is a hand-maintained literal.** The numeric macros do
> not build it. `alwan_platform.h:20-24` defines `ALWAN_VERSION_MAJOR 2`,
> `ALWAN_VERSION_MINOR 0`, `ALWAN_VERSION_PATCH 0`, `ALWAN_VERSION` as
> `(MAJOR * 10000) + (MINOR * 100) + PATCH`, and then, separately,
> `ALWAN_VERSION_STRING "2.0.0"`. A third copy of the number lives in
> `CMakeLists.txt:12` as `VERSION 2.0.0`. Nothing cross-checks the three. The
> string can drift from `ALWAN_VERSION` with no build error and no test failure:
> the only test on it compares the string to the header macro of the *same*
> build, which cannot detect drift. Gate program logic on the integer
> `ALWAN_VERSION` and treat the string as display text.

The `major.minor.patch` shape (no pre-release suffix, no build metadata) is what
the literal happens to hold today. Nothing enforces it.

---

## Global Settings

One library *setting* lives outside the context: the ACES tone curve
interpolation method. It is process-wide, and `alwan_ctx` does not own it. It is
not the only process-wide mutable state in the library -- the ACES 2.0 output
transform also keeps a single-entry gamut-compression-parameter cache
(`g_aces2_gcp_cache_f64`, `alwan_aces_ff.c:3258`) outside the context, shared by
both precisions and documented in place as a "Single-threaded optimisation"
(`:3249-3251`).

> **Precision variants:** functions written below as `name_{T}` exist in two
> forms, `name_f32` (single precision) and `name_f64` (double precision).
> `T = f32 | f64`. There is no unsuffixed alias, no `_Generic` dispatch and no
> `alwan_scalar`-typed wrapper for any of them; pick the precision explicitly.

### alwan_set_aces_interp / alwan_get_aces_interp

```c
/* ACES tone curve interpolation method */
typedef enum {
    ALWAN_ACES_INTERP_BSPLINE = 0,  /* Quadratic B-spline (Academy CTL reference, default) */
    ALWAN_ACES_INTERP_HERMITE = 1,  /* Legacy piecewise Hermite approximation */
    ALWAN_ACES_INTERP_OCIO = 2      /* OCIO GradingRGBCurve (monotone cubic Hermite, pixel-exact OCIO match) */
} alwan_aces_interp;

void alwan_set_aces_interp(alwan_aces_interp method);
alwan_aces_interp alwan_get_aces_interp(void);
```

**Parameters:**
- `method`: one of the three enumerators. Not validated (see below).

**Returns:** `alwan_set_aces_interp` returns nothing. `alwan_get_aces_interp`
returns the value last stored, verbatim.

**Default:** `ALWAN_ACES_INTERP_BSPLINE` (0), from the static initialiser of the
backing global (`alwan_aces_ff.c:23`).

Neither function takes a `ctx`, neither can fail, and neither returns an
`alwan_status`.

> **This is a single non-atomic process-wide global.** `alwan_aces_ff.c:23` holds
> it as `static alwan_aces_interp g_aces_interp = ALWAN_ACES_INTERP_BSPLINE;`
> under the comment "ACES interpolation mode (global, thread-unsafe for
> simplicity)". `struct alwan_ctx` has no interp field. Consequences: two threads
> that render with different settings race on an unsynchronised read/write; a
> `set` anywhere in the process changes results for every other user of the
> library in that process; there is no reset function, and the value survives
> every `alwan_create` / `alwan_destroy`. The repository tracks this as an open
> violation of the per-context model in [violations.md](../violations.md)
> ("Move it into `alwan_ctx`"). If you need two settings at once, you need two
> processes.

#### Scope

The header names it "the ACES tone curve interpolation method" with no scope.
The global is read in one function, the ACES 1.x forward output transform
`alwan_aces1_output_transform_{T}`: four branch sites in the f64 body
(`alwan_aces_ff.c:1332`, `:1341`, `:1350`, `:1368`) and four in the native f32
template (`alwan_aces1_impl.inc:258`, `:267`, `:272`, `:288`).

The declarations the rows below refer to:

```c
alwan_status alwan_aces1_output_transform_f64(alwan_rgb_f64 *rgb_out,
                                      alwan_rgb_f64 const *rgb_in,
                                      alwan_aces1_output output);

alwan_status alwan_view_transform_apply_f64(alwan_f64 *rgb_out, size_t out_stride,
                                      alwan_f64 const *rgb_in, size_t in_stride,
                                      size_t count, alwan_view_transform vt,
                                      alwan_ctx *ctx);
```

The ACES 1.x entry point is a single-triplet call; the view transform is a
strided bulk loop that takes a `ctx`.

| Entry point | Affected |
|---|---|
| `alwan_aces1_output_transform_{T}` | Yes, the only direct consumer |
| `alwan_view_transform_apply_{T}` / `alwan_view_transform_apply_unclamped_{T}` with `ALWAN_VIEW_ACES_REC709` | Yes, indirectly |
| `alwan_view_transform_{T}_map_interleave`, `alwan_view_transform_map_interleave_ex` with the same view | Yes, indirectly; they forward to `_apply_{T}` (`alwan_view_map.c:26`, `:33`, `:45`, `:49`, `:58`) |
| `alwan_aces1_output_transform_inv_{T}` | **No** |
| `alwan_aces2_output_transform_{T}` and its inverse | **No** |
| The 2D/3D LUT bakers with `ALWAN_VIEW_ACES_REC709` | **No**; they substitute the Narkowicz `alwan_aces_tonemap` fit (`alwan_lut_impl.inc:103-107`) |

> **The setting also steers the ACES view transform, and through it the bulk
> image paths.** `ALWAN_VIEW_ACES_REC709` runs
> `ALWAN_CORE_FNLIT(alwan_aces1_output_transform)(&out, &ap0, ALWAN_ACES1_OUT_SRGB_100NIT)`
> (`alwan_view_impl.inc:23`). That line is an internal template instantiation
> that resolves to `alwan_aces1_output_transform_{T}`; it is not a call a user
> can write. The result then has its sRGB OETF undone (`:26-28`). So
> `alwan_set_aces_interp` moves the output of `alwan_view_transform_apply_{T}`,
> `alwan_view_transform_apply_unclamped_{T}` and the three `map_interleave`
> entry points. A test suite or determinism hash that sets the interp globally
> also shifts every view-transform result taken afterwards in that process.

> **The enumerator's name and the ODT it runs disagree.** `alwan.h:587`
> documents `ALWAN_VIEW_ACES_REC709 = 0` as "ACES RRT + ODT Rec.709". The code
> passes `ALWAN_ACES1_OUT_SRGB_100NIT` and inverts its sRGB OETF
> (`alwan_view_impl.inc:23`, `:26-28`). The ODT is the sRGB 100-nit one;
> `ALWAN_ACES1_OUT_REC709_100NIT`, the BT.1886 output, is never reached from
> this view.

> **ACES 2.0 is unaffected.** `alwan_aces2_output_transform_{T}` contains no
> reference to the global. Setting `ALWAN_ACES_INTERP_OCIO` and expecting ACES
> 2.0 to match OCIO produces no change and no error.

> **The inverse ignores the setting, so a non-default setting breaks round
> trips.** `alwan_aces1_output_transform_inv_{T}` branches on
> `aces1_ssts_for_output_f64(output)` (`alwan_aces_ff.c:1893-1924`): outputs
> 8/9/10 run `aces1_ssts_inv` alone (`:1894-1902`, the comment there notes
> "There is no C5 stage"), and every other output runs `aces1_c9_inv` then
> `aces1_segmented_spline_c5_inv` (`:1904-1923`). Neither the Hermite tables nor
> the OCIO knots have an inverse in the tree. The f32 inverse widens to the f64
> one (`:1949-1963`). The header advertises the pair "for round-trip workflows"
> and states no precondition. Under the default, `docs/alwan_future.md:700-701`
> records all fifteen outputs round-tripping at 1.3e-11. Under `HERMITE` or
> `OCIO` the forward and the inverse are no longer inverses of each other;
> nothing in the tree measures how far apart they land.

#### Curve selected per setting

The three values do not select an interpolation of one curve. They select a
different curve chain, and which chain you get depends on the `output` argument
passed to the transform. `SDR/cinema` below means the nine outputs
`ALWAN_ACES1_OUT_REC709_100NIT` (0) through `ALWAN_ACES1_OUT_REC2020_100NIT` (7)
plus `ALWAN_ACES1_OUT_DCDM_48NIT` (11).

| `output` | BSPLINE (0) | HERMITE (1) | OCIO (2) |
|---|---|---|---|
| SDR/cinema (0-7, 11) | C5 + 48-nit C9 B-splines | Hermite RRT + 48-nit Hermite ODT | OCIO 7-knot C5 + 15-knot C9 |
| `..._1000NIT_PQ` (8) | SSTS | **SSTS** (setting dropped) | OCIO 7-knot HDR-1000 fit |
| `..._2000NIT_PQ` (9), `..._4000NIT_PQ` (10) | SSTS | **SSTS** (setting dropped) | **SSTS** (setting dropped) |
| `..._PQ_V103` (12, 13, 14) | C5 + 1000/2000/4000-nit C9 | **Hermite C5 + B-spline C9** (hybrid) | **C5 + C9 B-splines** (setting dropped) |

> **`ALWAN_ACES_INTERP_OCIO` is honoured for 10 of the 15 outputs.** The setting
> reaches the nine SDR/cinema outputs and `ALWAN_ACES1_OUT_REC2020_1000NIT_PQ`
> only (guards at `alwan_aces_ff.c:1332` and `:1341`). For `_2000NIT_PQ`,
> `_4000NIT_PQ` and all three `_V103` outputs the setting is discarded and the
> default curve runs. No status, no warning, no diagnostic: the caller asked for
> OCIO and got the B-spline or SSTS path.
>
> The enumerator's "pixel-exact OCIO match" is pinned for one output. Suite 56
> compares `REC2020_1000NIT_PQ` against OCIO 2.5.0's "ACES 1.1 - HDR Video (1000
> nits & Rec.2020 lim)" view and asserts a worst difference under 0.002, about
> two codes of 1023, with every channel below 0.05 excluded because near black
> the two implementations differ by tens of 10-bit codes. No test measures any
> of the nine SDR/cinema outputs against OCIO; their knots come from one OCIO
> builtin fit ("ACES-OUTPUT ... SDR-VIDEO_1.0", `alwan_aces_ff.c:1006-1008`,
> `:30-32`) applied unchanged to the P3-DCI, P3-D60 and DCDM cinema outputs.

> **"HERMITE" means three different things depending on `output`.** It is
> dropped entirely for the SSTS HDR outputs (8, 9, 10). For the three `_V103`
> outputs it yields an undocumented hybrid: the C5 stage becomes the Hermite fit
> while the C9 stage stays a B-spline, because the Hermite C9 exists only as the
> 48-nit table (guard at `alwan_aces_ff.c:1368`). Only on the nine SDR/cinema
> outputs is the whole chain Hermite.

#### Outside the knot range

This section covers the SDR/cinema and `_V103` outputs, which are the ones that
run a C5 + C9 (or OCIO) chain. Outputs 8-10 run the SSTS instead: it replaces
both splines (`alwan_aces_ff.c:1328-1339`, comment at `:1897`), floors its input
at `HALF_MIN` (`5.96046448e-08`, `:785`) and carries its own `min_x` / `max_x`.

- `BSPLINE` and `HERMITE` behave identically at the extremes. The Hermite tables
  are a fit of the same curve, end slopes included. Both flat-clamp the C5 stage
  to `1e-4` nits below `x = 0.18 * 2^-15` and to `10000` nits above
  `x = 0.18 * 2^18` (B-spline `min_pt_y` / `max_pt_y` at
  `alwan_aces_ff_core.inc:343-360`; Hermite `ps[0] == ps[6] == 0.0` at
  `alwan_aces_ff.c:71`, and `hermite_quad_eval` at `:37-59` reduces its
  above-range `end_slope` algebraically to `ps[n-1]`). Both let the C9 / ODT48
  stage extrapolate above its last knot with slope `0.04` in log-log
  (`aces1_c9_48nit_breakpoints.csv` `slope_high` `4.0e-2`, applied at
  `alwan_aces_ff.c:1107-1108`; Hermite `ps[14] = 0.04` at `:90`), which reaches
  linCV about 1.096 when the C5 stage sits at its 10000-nit ceiling.
- `OCIO` is the one that differs. `ocio_curve_eval` clamps to the first / last
  knot y in each of the two cascaded curves (`alwan_aces_ff.c:1052-1053`), so
  its ceiling is exactly linCV 1.0.

Both non-SSTS paths floor the linear input at `1e-10` before `log10`.

> **For outputs 0-7 none of this is observable.** The transform clamps
> display-linear RGB to `[0, 1]` before the OETF (`alwan_aces_ff.c:1513-1516`,
> "matches OCIO RangeTransform before EOTF"), so the linCV 1.096 the C9 / ODT
> extrapolation produces cannot reach the caller. `ALWAN_ACES1_OUT_DCDM_48NIT`
> returns from its own switch arm at `:1492-1499`, before that clamp is reached;
> it is the one SDR/cinema output where an out-of-range tone-curve value
> survives.

Fixed normalisation constants: the Hermite 48-nit ODT stage hard-codes black
`0.02` / white `48.0` nits (`alwan_aces_ff.c:91`), the OCIO SDR stage hard-codes
the same `0.02` / `48.0` (`aces1_ocio_sdr_eval`, `:1079-1085`, constants on
`:1084`), and the OCIO 1000-nit stage hard-codes `0.0001` / `1000.0`
(`aces1_ocio_hdr1000_eval`, `:1089-1095`, constants on `:1093-1094`).

#### Validation

> **An out-of-range value is stored, returned verbatim, and behaves as
> BSPLINE.** `alwan_set_aces_interp` is a bare store with no range check, no
> clamp and no way to report anything: it returns `void` (`alwan_aces_ff.c:25`).
> The forward transform dispatches on a chain of equality tests with the
> B-spline arm as the fall-through, so
> `alwan_set_aces_interp((alwan_aces_interp)99)` renders exactly like the
> default while `alwan_get_aces_interp()` returns `99`. The getter does not
> normalise. Do not assume its result is one of `{0, 1, 2}`.

**Error codes:** the setting itself can never produce one. The only status the
steered transform returns is `ALWAN_E_INVALID` for a `NULL` `rgb_out` / `rgb_in`
or an `output` outside `[0, ALWAN_ACES1_OUT_COUNT)`, which is 15
(`alwan_aces_ff.c:1255-1256`); `alwan_view_transform_apply_{T}` returns
`ALWAN_E_INVALID` for a `NULL` buffer or an unknown `alwan_view_transform`.
Neither is tied to the interp setting. Signatures and per-output detail are in
[ACES](aces.md) and [Transfer Functions](transfer-functions.md).

---

## Default Allocator

### alwan_default_realloc

```c
void *alwan_default_realloc(void *ptr, size_t old_size, size_t new_size, size_t align);
```

Reallocates a block obtained from `alwan_default_alloc`, preserving alignment.

**Parameters:**
- `ptr`: block to reallocate, or `NULL` to allocate a fresh one
- `old_size`: size in bytes of the block `ptr` currently owns. Trusted, never
  verified. Used directly as a `memcpy` length.
- `new_size`: requested size in bytes. `0` means "free `ptr`" (see below).
- `align`: requested alignment in bytes. Honoured with different rules per
  platform (see the table below).

**Returns:**
- Pointer to the new block on success. With a non-`NULL` `ptr` it is never equal
  to `ptr`: the new block is allocated at `alwan_context.c:70` while the old one
  is still live, and the old one is freed at `:81`, so the two addresses cannot
  coincide.
- `NULL` on allocation failure, with the **old block left allocated and valid**.
- `NULL` for the deliberate `new_size == 0` free.

**Declaration site:** `alwan_config.h:56`, not `alwan.h`. `alwan.h` includes
`alwan_config.h`, so it is reachable from `<alwan/alwan.h>`, but it is
undocumented there: the only text above it is the section header "Allocation
hooks (overrideable at compile time)" (`:45-47`), a three-line note on the
CPU-backend guard (`:49-51`), and the group comment "Forward declarations for
default allocators" (`:54`). Nothing describes `realloc` itself.

> **The declaration is conditional.** `alwan_default_alloc`,
> `alwan_default_free`, `alwan_default_realloc` and the
> `ALWAN_ALLOC` / `ALWAN_FREE` / `ALWAN_REALLOC` macros exist only when
> `ALWAN_BACKEND` is `ALWAN_BACKEND_C` or `ALWAN_BACKEND_HALIDE`
> (`alwan_config.h:53`, closed at `:71`). Under the HLSL and GLSL backends the
> symbols are not declared at all, so a header consumer that references them
> fails to compile there. See [Backends](backends.md).

#### Differences from C realloc

> **`alwan_default_realloc(p, n, 0, a)` is a free that returns `NULL`.**
> `new_size == 0` calls `alwan_default_free(ptr)` and returns `NULL`
> (`alwan_context.c:60-62`). The failure path returns the same `NULL`, so the
> two cases are indistinguishable from the return value alone. A caller that
> treats `NULL` as failure and keeps using or re-freeing `p` double-frees a
> block that is already gone. Check `new_size` yourself before the call.

> **It is never in place. Assume the pointer moved on every successful call.**
> Except for the two degenerate cases (`new_size == 0`, `ptr == NULL`), the
> implementation always allocates a fresh block via `alwan_default_alloc`,
> `memcpy`s `min(old_size, new_size)` bytes, frees the old block and returns the
> new pointer (`alwan_context.c:68-82`). This includes a pure shrink, which
> still pays a full copy. Any second pointer or offset into the old block
> dangles after the call. C's `realloc` may resize in place; this one cannot.

> **`old_size` is trusted with no bound check.** There is no stored header and no
> bookkeeping anywhere in the default allocator, so `old_size` is used raw as the
> `memcpy` length (`alwan_context.c:75-78`). Overstating it is an out-of-bounds
> **read** of the source block on every grow. Understating it silently truncates
> the surviving data with no error. C's `realloc` has no such parameter, so
> nothing in the signature hints that its accuracy matters.

> **On MSVC the result must be released through `alwan_default_free`.** The MSVC
> branch always returns `_aligned_malloc` memory (`alwan_context.c:32`), which
> `alwan_default_free` releases with `_aligned_free` (`:53`). Passing it to plain
> `free()` or to the CRT `realloc()` corrupts the heap. On the C11 branch the
> same call may return either `aligned_alloc` or plain `malloc` memory, both of
> which `free()` accepts. The ownership rule is strict on Windows and lax
> elsewhere, and the declaration states neither.

#### Alignment by build

All alignment behaviour comes from `alwan_default_alloc` (`alwan_context.c:16-48`).

| Build | Condition | Behaviour |
|---|---|---|
| MSVC (`_WIN32 && _MSC_VER`) | `align < sizeof(void*)` | raised to `sizeof(void*)`, then honoured |
| MSVC | `align` not a power of two | rounded **up** to the next power of two, then honoured |
| MSVC | otherwise | `_aligned_malloc(size, align)` |
| C11 (`__STDC_VERSION__ >= 201112L`) | `align < sizeof(void*)` or not a power of two | request **silently dropped**, plain `malloc(size)` |
| C11 | otherwise | `size` rounded up to a multiple of `align` when it is not already one, then `aligned_alloc(align, size)` |
| pre-C11, non-MSVC | always | `(void)align;` -- alignment **ignored unconditionally** |

Two consequences worth stating outright. `align = 32` on a pre-C11 non-MSVC
toolchain gets `malloc`'s alignment and no indication that the request was
discarded. On the C11 branch the size is rounded up to a multiple of `align`
when `(size % align) != 0` (`alwan_context.c:39-41`), so the block can be larger
than `new_size`; on the other two branches it is exactly `new_size`.

> **A non-power-of-two `align` above `SIZE_MAX / 2` hangs the process on MSVC.**
> The round-up loop is `size_t p = sizeof(void*); while (p < align) p *= 2;`
> (`alwan_context.c:28-29`). Once `p` reaches the largest representable power of
> two (2^63 on a 64-bit build) the next doubling overflows to `0` and the
> condition stays true forever. The call never returns and never reports `NULL`.
> The C11 and fallback branches have no such loop.

#### Reachability

> **`ALWAN_REALLOC` is a dead hook.** [configuration.md](../configuration.md)
> lists `ALWAN_ALLOC` / `ALWAN_FREE` / `ALWAN_REALLOC` / `ALWAN_MEMCPY` together
> as "all four ... default to the obvious implementations", which reads as if
> overriding any of them redirects the library. `ALWAN_REALLOC` has **zero call
> sites in the whole repository** outside its own `#define`
> (`alwan_config.h:67-68`) and the example in the configuration page, so
> overriding it changes nothing. The other three are live: `ALWAN_ALLOC` has 20
> call sites, `ALWAN_FREE` 34, and `ALWAN_MEMCPY` 26. `alwan_default_realloc` is
> therefore reachable only if you call it yourself; it exists as an exported
> utility.

> **A context's custom allocator cannot intercept it.** `alwan_config` has
> `alloc_cb` and `free_cb` (`alwan.h:109-110`) and no `realloc_cb`; there is no
> such typedef. `alwan_default_realloc` does not take a `ctx` and hard-calls
> `alwan_default_alloc` / `alwan_default_free`. A caller who invokes
> `ALWAN_REALLOC` or `alwan_default_realloc` directly therefore gets the built-in
> allocator regardless of any `alloc_cb` / `free_cb` on their context. The
> library itself never calls either.

**Thread Safety:** as thread-safe as the underlying platform allocator
(`_aligned_malloc` / `aligned_alloc` / `malloc`). It holds no library state.


---

## Usage Patterns

### Pattern 1: Simple Context (Recommended)

```c
alwan_ctx *ctx = alwan_create(NULL);

// Use context for operations...

alwan_destroy(ctx);
```

**Use when:** Default behavior is sufficient (system allocator, embedded data)

---

### Pattern 2: Custom Allocator

```c
void* my_alloc(size_t size, size_t align) {
    return my_memory_pool_alloc(size, align);
}

void my_free(void *ptr) {
    my_memory_pool_free(ptr);
}

alwan_ctx *ctx = alwan_create(&(alwan_config){
    .alloc_cb = my_alloc,
    .free_cb = my_free
});

// Context now uses custom allocator

alwan_destroy(ctx);
```

**Use when:**
- Integrating with game engine memory systems
- Embedded systems with custom allocators
- Memory profiling/debugging

---

### Pattern 3: Runtime Data Loading (NOT IMPLEMENTED)

> **NOT IMPLEMENTED.** Runtime mode is planned for alwan 3.0.0.
> Building with `ALWAN_EMBED_DATA=0` produces a compile-time error.
> Always use `ALWAN_EMBED_DATA=1` (the default) and pass `runtime_data_root = NULL`.

---

### Pattern 4: Multi-threaded Context Sharing

```c
// Create one shared context
alwan_ctx *shared_ctx = alwan_create(NULL);

/* Get descriptors once (thread-safe read; ctx is LAST) */
alwan_rgb_space_desc_{T} srgb_desc, bt2020_desc;
alwan_rgb_get_space_descriptor_{T}(&srgb_desc, ALWAN_RGB_SPACE_SRGB, shared_ctx);
alwan_rgb_get_space_descriptor_{T}(&bt2020_desc, ALWAN_RGB_SPACE_BT2020, shared_ctx);

/* Use from multiple threads (read-only operations) */
#pragma omp parallel for
for (int i = 0; i < n; i++) {
    /* Safe: read-only color space conversion (ctx is LAST) */
    alwan_rgb_convert_{T}(&out[i], &srgb_desc, &bt2020_desc, &rgb[i], shared_ctx);
}

alwan_destroy(shared_ctx);
```

**Safe operations:**
- Color space conversions
- RGB space lookups
- Transfer function lookups

**Unsafe operations:**
- Modifying context state (currently none in API)

---

### Pattern 5: Per-Thread Context

```c
#pragma omp parallel
{
    // Each thread gets its own context
    alwan_ctx *ctx = alwan_create(NULL);

    #pragma omp for
    for (int i = 0; i < n; i++) {
        // Use thread-local context
        process_color(ctx, &data[i]);
    }

    alwan_destroy(ctx);
}
```

**Use when:**
- Maximum thread isolation needed
- Context creation overhead is acceptable

---

## Memory Usage

### Embedded Mode (ALWAN_EMBED_DATA=1)

**Allocation during `alwan_create()`:**
- Context structure: ~few KB
- No data loading (compiled into binary)

**Total runtime memory:** < 10 KB per context

---

### Runtime Mode (ALWAN_EMBED_DATA=0): NOT IMPLEMENTED

> Runtime data loading is not implemented. Planned for alwan 3.0.0.

---

## Error Handling

### Context Creation Failure

```c
alwan_ctx *ctx = alwan_create(config);
if (!ctx) {
    /* The only failure mode: the allocator returned NULL for the
     * context struct (or for the copied runtime_data_root). */
}
```

> **Note:** `alwan_create` does not cross-validate the config. `alloc_cb` and
> `free_cb` default independently, so setting one without the other is accepted
> (your custom function is paired with the default for the other). Make sure
> the two are compatible, since a custom allocation may otherwise be released
> with `alwan_default_free`.

**Debugging:**
1. Ensure the allocator callback actually returns non-`NULL`
2. Check available memory

---

### Null Context Handling

API functions validate their **required** pointer arguments (outputs, descriptors,
input colors) and return `ALWAN_E_INVALID` when those are `NULL`. The `ctx`
argument itself is treated as optional in the embedded build:

```c
alwan_destroy(NULL);  // Safe, does nothing

// ctx == NULL is tolerated: descriptor lookups ignore it, and
// alwan_rgb_convert_f64 simply skips the chromatic-adaptation step.
int status = alwan_rgb_convert_f64(&rgb_out, &src_desc, &dst_desc, &rgb_in, NULL);
// status == ALWAN_OK (conversion runs, white-point adaptation is skipped)

// A NULL *required* argument is what returns the error:
status = alwan_rgb_convert_f64(NULL, &src_desc, &dst_desc, &rgb_in, ctx);
// status == ALWAN_E_INVALID (dst_rgb is NULL)
```

---

## Limits

- **Maximum contexts:** Limited only by available memory
- **Context size:** < 10 KB (embedded mode)
- **Thread safety:** Read-only operations safe, write operations require synchronization
- **Lifetime:** No maximum, can persist for application lifetime

---

## Best Practices

1. **Create once, use many times**
   ```c
   // Good
   alwan_ctx *ctx = alwan_create(NULL);
   for (int i = 0; i < 1000; i++) {
       process_frame(ctx, &frames[i]);
   }
   alwan_destroy(ctx);

   // Bad (unnecessary overhead)
   for (int i = 0; i < 1000; i++) {
       alwan_ctx *ctx = alwan_create(NULL);
       process_frame(ctx, &frames[i]);
       alwan_destroy(ctx);
   }
   ```

2. **Always destroy contexts**
   ```c
   alwan_ctx *ctx = alwan_create(NULL);
   // ... use context ...
   alwan_destroy(ctx);  // Don't forget!
   ```

3. **Check for creation failure**
   ```c
   alwan_ctx *ctx = alwan_create(config);
   if (!ctx) {
       // Handle error appropriately
       return ERROR_CODE;
   }
   ```

4. **Use NULL config for defaults**
   ```c
   // Preferred: clear and simple
   alwan_ctx *ctx = alwan_create(NULL);

   // Unnecessary
   alwan_config cfg = {0};
   alwan_ctx *ctx = alwan_create(&cfg);
   ```

---

## See Also

- [Configuration](../configuration.md): Compile-time options
- [Data Management](../data-management.md): Embedded vs runtime data
- [Examples](../examples.md): Complete usage examples
