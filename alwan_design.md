# Alwan — Design & Scope (Updated)
**License:** MIT  
**Language/ABI:** Pure C (C11), stable C ABI  
**Goal:** Alwan is a small, dependency‑free colour‑science math library with feature parity targets aligned to the Python **Colour** project (math & data only; no plotting/GUI).

> This document describes the scope, design principles, public API shape, **single-folder data strategy (`/data/*.csv` with C‑parsable numeric literals)**, numeric policies, repository layout (Sharpmake‑driven), and testing approach.

---

## 1) Scope

Alwan focuses on **deterministic, high‑performance colour math** and reference data:

- **Colour models & conversions**
  - XYZ, xyY, Lab, Luv, LCh(uv/ab), IPT, JzAzBz (conversions both ways as applicable).
  - RGB families: “derive from primaries + white” and convert RGB↔XYZ, RGB↔RGB.
  - YCbCr, YcCbcCrc (BT.601/709/2020).
  - Convenience models: HSV, HSL, **CMY, CMYK** (via RGB).
- **Chromatic adaptation**: Bradford, CAT02, CAT16, XYZ scaling.
- **Transfer functions (OETF/EOTF/OOTF)**: sRGB, BT.709, BT.1886, ST.2084 (PQ), HLG and common log curves (S‑Log, C‑Log, V‑Log).
- **ACES & AgX pipelines**
  - RGB colourspaces: **ACES2065‑1 (AP0), ACEScg (AP1), ACESproxy**.
  - **View/encoding transforms**: ACES RRT+ODT pack (where specification allows reference math), and **AgX** encode/decode & looks as a “view transform” module.
- **Colour differences**: ΔE76, ΔE94, CMC, **ΔE00**.
- **Colour appearance**: CIECAM02, CAM16 (forward/inverse, UCS).
- **Spectral computations**: Uniformly‑sampled SPD integration to XYZ; CMFs (CIE 1931/1964/2012), illuminants (A, D, E, F), resampling, band‑pass corrections.
- **Light quality & CCT**: CRI, CQS, SSI, TM‑30 summary (Rf/Rg), CCT estimators (e.g., McCamy, Robertson).
- **Gamut utilities**: Basic gamut mapping strategies; RGB gamut volume/coverage helpers.

**Out of scope (library core):** Any plotting/visualisation, device I/O, image codecs, camera/OCIO pipelines, GUI, threads created internally.

---

## 2) Design Principles

- **Pure C, zero external deps**: The library builds with any C11 compiler; the public ABI is plain C.  
- **Allocation control**: All heap usage (rare) goes through `ALWAN_ALLOC(size, align)` / `ALWAN_FREE(ptr)` macros that can be overridden at compile time. Default maps to `aligned_alloc`/`free` or `malloc`/`free` where alignment is unnecessary.
- **Deterministic & re‑entrant**: No global mutable state. All state & caches live in an explicit `alwan_ctx*`. Functions accept caller‑owned buffers and strides to avoid internal allocation.
- **Compile‑time scalar**: `typedef alwan_scalar` as `float` or `double` via `ALWAN_SCALAR_IS_FLOAT` (default: `double` for parity with Colour).
- **Numerical sanity**: Partial‑pivot Gaussian solve for 3×3, carefully‑clamped transfer functions, consistent white‑point handling, integration via Simpson with trapezoid fallback.
- **Bulk‑first API**: Transform functions operate over arrays with `count`, `in_stride`, `out_stride`, and optional user scratch to keep hot paths allocation‑free.
- **Portability**: No compiler extensions required; optional SIMD paths guarded by `#ifdef` (SSE2/NEON/etc.) live in separate translation units and are entirely optional.

---

## 3) Data Strategy — **Single `/data/` folder**

Many algorithms require reference datasets (CMFs, illuminants, RGB primaries). Alwan ships and consumes data from a **single folder**: `/data/`. The same files are used for both “embedded” and “runtime” modes.

### 3.1 File format: **C‑parsable CSV numeric literals**

- Each file contains only comma‑separated numeric literals at **maximum precision**, e.g.:  
  `2.457001234567890,1.466000000000000,0.960000000000000, ...`  
  No headers, no trailing comma on the last line, **no `f` suffix**.
- Filenames encode identity & sampling, e.g.:  
  `cie_1931_2deg_xbar_360_830_1nm.csv`

### 3.2 Embedded mode (`#define ALWAN_EMBED_DATA 1`)

- Arrays are materialised by **including** the CSV directly into an initializer. Because bare literals are of type `double`, we locally disable the “implicit narrowing” warning when compiling as `float`.

  ```c
  /* alwan_data_embed.c */
  #include "alwan_config.h"
  #include "alwan.h"

  /* compiler‑portable diagnostic guards */
  #if defined(_MSC_VER)
    #define ALWAN_DIAG_PUSH __pragma(warning(push))
    #define ALWAN_DIAG_POP  __pragma(warning(pop))
    #define ALWAN_DIAG_DISABLE_FLOAT_CONV __pragma(warning(disable: 4244 4305))
  #elif defined(__clang__)
    #define ALWAN_DIAG_PUSH _Pragma("clang diagnostic push")
    #define ALWAN_DIAG_POP  _Pragma("clang diagnostic pop")
    #define ALWAN_DIAG_DISABLE_FLOAT_CONV _Pragma("clang diagnostic ignored \"-Wimplicit-float-conversion\"")
  #elif defined(__GNUC__)
    #define ALWAN_DIAG_PUSH _Pragma("GCC diagnostic push")
    #define ALWAN_DIAG_POP  _Pragma("GCC diagnostic pop")
    #define ALWAN_DIAG_DISABLE_FLOAT_CONV _Pragma("GCC diagnostic ignored \"-Wfloat-conversion\"")
  #else
    #define ALWAN_DIAG_PUSH
    #define ALWAN_DIAG_POP
    #define ALWAN_DIAG_DISABLE_FLOAT_CONV
  #endif

  ALWAN_DIAG_PUSH
  ALWAN_DIAG_DISABLE_FLOAT_CONV
  static alwan_scalar const g_cie_xbar[] = {
    #include "data/cie_1931_2deg_xbar_360_830_1nm.csv"
  };
  ALWAN_DIAG_POP
  ```

### 3.3 Runtime mode (`#define ALWAN_EMBED_DATA 0`)

- On first request, Alwan loads the **same `/data/*.csv`** from `alwan_config.runtime_data_root` or `ALWAN_DATA_PATH` env var, parses them with a tiny hand‑rolled CSV reader and **caches** arrays inside `alwan_ctx`.
- No difference in precision or semantics between embed/runtime modes.

### 3.4 Data identity & registry

- Datasets are referenced by ASCII keys, e.g. `"CIE 1931 2 Degree Standard Observer"`, `"D65"`, `"sRGB"`, `"ACEScg"`, `"ACES2065-1"`, `"AgX"` (view).  
- The context owns a registry mapping names → typed records (CMF, illuminant, RGB primaries/white/transfer names, view transforms).

---

## 4) Public API Shape (overview)

Header style: snake_case, east‑const, POD types only.  Error reporting via integer status codes; no `errno`.

```c
/* --- alwan.h (selected excerpts) --- */

typedef alwan_scalar Scalar; /* ABI alias */

typedef struct alwan_ctx alwan_ctx; /* opaque context */

typedef enum {
  ALWAN_OK = 0, ALWAN_E_INVALID = -1, ALWAN_E_NODATA = -2, ALWAN_E_RANGE = -3, ALWAN_E_NOMEM = -4
} alwan_status;

/* Alloc hooks (POD only). Overridable macros route here by default. */
typedef void *(*alwan_alloc_fn)(size_t size, size_t align);
typedef void  (*alwan_free_fn)(void *ptr);

typedef struct {
  alwan_alloc_fn alloc_cb; /* nullable -> defaults */
  alwan_free_fn  free_cb;  /* nullable -> defaults */
  char const *runtime_data_root; /* nullable; used when ALWAN_EMBED_DATA==0 */
  uint32_t flags; /* reserved */
} alwan_config;

alwan_ctx *alwan_create(alwan_config const *cfg);
void       alwan_destroy(alwan_ctx *ctx);

/* Small math types */
typedef struct { Scalar v[3]; } alwan_vec3;
typedef struct { Scalar m[9]; } alwan_mat3x3;

/* Uniformly sampled SPD */
typedef struct {
  Scalar lambda_start_nm, step_nm;
  size_t n;
  Scalar const *samples;
} alwan_spd;

/* Observers & illuminants */
int alwan_observer_xyz(alwan_ctx *ctx, char const *observer_name,
                       Scalar lambda_start_nm, Scalar step_nm, size_t n,
                       Scalar *x_bar, Scalar *y_bar, Scalar *z_bar);
int alwan_spd_illuminant(alwan_ctx *ctx, char const *name,
                         Scalar lambda_start_nm, Scalar step_nm, size_t n,
                         Scalar *spd_out);

/* Spectral → XYZ (absolute if illuminant is NULL) */
int alwan_xyz_from_spd(alwan_ctx *ctx, alwan_spd const *spd,
                       char const *observer_name, alwan_spd const *illuminant_or_null,
                       alwan_vec3 *xyz_out);

/* Chromatic adaptation */
typedef enum { ALWAN_CAT_BRADFORD, ALWAN_CAT_CAT02, ALWAN_CAT_CAT16, ALWAN_CAT_XYZ_SCALE } alwan_cat;
int alwan_cat_matrix(alwan_ctx *ctx, alwan_cat cat,
                     alwan_vec3 const *src_w_xyz, alwan_vec3 const *dst_w_xyz, alwan_mat3x3 *m_out);
int alwan_xyz_adapt(alwan_ctx *ctx, alwan_cat cat,
                    alwan_vec3 const *src_w_xyz, alwan_vec3 const *dst_w_xyz,
                    alwan_vec3 const *xyz_in, size_t count, size_t in_stride,
                    alwan_vec3 *xyz_out, size_t out_stride);

/* RGB spaces */
typedef struct {
  Scalar primaries_3x2[6]; /* rx ry gx gy bx by */
  Scalar white_xy[2];
  char const *oetf_name; /* e.g., "srgb", "pq", "acesproxy" */
  char const *eotf_name;
} alwan_rgb_space_desc;

int alwan_rgb_derive_matrices(alwan_ctx *ctx, alwan_rgb_space_desc const *desc,
                              alwan_mat3x3 *rgb_to_xyz, alwan_mat3x3 *xyz_to_rgb);

/* Transfer & view transforms */
int alwan_oetf_apply(alwan_ctx *ctx, char const *name,
                     Scalar const *lin, size_t count, size_t is, Scalar *nonlin, size_t os);
int alwan_eotf_apply(alwan_ctx *ctx, char const *name,
                     Scalar const *nonlin, size_t count, size_t is, Scalar *lin, size_t os);

/* ACES & AgX view transforms */
int alwan_view_transform_apply(alwan_ctx *ctx, char const *name /* "aces_rrt+odt_rec709", "agx", "agx_punchy" */,
                               alwan_vec3 const *rgb_in, size_t n, size_t is,
                               alwan_vec3 *rgb_out, size_t os);

/* Core models */
void alwan_xyz_to_lab(alwan_vec3 const *xyz, alwan_vec3 const *wp_xyz,
                      alwan_vec3 *lab, size_t n, size_t in_stride, size_t out_stride);
void alwan_lab_to_xyz(alwan_vec3 const *lab, alwan_vec3 const *wp_xyz,
                      alwan_vec3 *xyz, size_t n, size_t in_stride, size_t out_stride);

/* Colour differences */
Scalar alwan_delta_e_2000(alwan_vec3 const *lab1, alwan_vec3 const *lab2);

/* Convenience (HSV/HSL/CMY/CMYK) */
void alwan_rgb_to_cmy(Scalar const *rgb, Scalar *cmy, size_t count, size_t in_stride, size_t out_stride);
void alwan_cmy_to_rgb(Scalar const *cmy, Scalar *rgb, size_t count, size_t in_stride, size_t out_stride);
void alwan_cmy_to_cmyk(Scalar const *cmy, Scalar *cmyk, size_t count, size_t in_stride, size_t out_stride);
void alwan_cmyk_to_cmy(Scalar const *cmyk, Scalar *cmy, size_t count, size_t in_stride, size_t out_stride);
```

---

## 5) Algorithms & Numerics (high level)

- **Matrix derivation for RGB spaces**: Solve for `M_rgb→xyz` from primaries `(rx,ry,gx,gy,bx,by)` and white `(xw,yw)`; invert for `M_xyz→rgb`.  3×3 solves use partial‑pivot Gaussian elimination.  
- **Chromatic adaptation**: Bradford, CAT02, CAT16, and XYZ scaling matrices; cache by `(src_wp, dst_wp, cat)` key.  
- **Transfer & view transforms**: Exact formulas with numerically‑stable branches; includes sRGB/BT.709/PQ/HLG/BT.1886, **ACESproxy**, and **AgX** encode/decode. `alwan_view_transform_apply` hosts ACES RRT+ODT wrappers (math only; no LUT I/O).  
- **Spectral integration**: Resample inputs to requested uniform grid (linear or Catmull‑Rom). Integrate to XYZ via Simpson (even `n`) else trapezoid. Absolute vs relative depending on illuminant usage.  
- **ΔE metrics**: Reference equations with defensively‑clamped intermediate terms.  
- **CIECAM02/CAM16**: Forward/inverse predicting appearance correlates; UCS transforms for distance metrics.  
- **CMY/CMYK**: `CMY = 1-RGB`; `K = min(C,M,Y)`; edge‑case K=1 handled; reverse via `C = C'*(1-K)+K`, etc.

---

## 6) Repository Layout (Sharpmake‑driven; no external deps)

Matches your preferred structure and makes **Sharpmake mandatory** for project generation.

```
/extern/                 # (kept empty; no runtime deps)
/src/
  alwan/                 # library sources
  ui/                    # (unused for now; kept for consistency)
/tests/                  # self-contained C tests (assert-runner)
/examples/               # tiny examples (optional)
/sharpmake/              # *.sharpmake.cs (main, tests, examples)
/projects/               # generated projects (.vcxproj, .sln)
/tmp/                    # intermediates
/tools/sharpmake/        # bootstrap output
/working_dir/            # test assets working dir
/data/                   # **single folder** of C‑parsable CSV datasets (*.csv)
bootstrap.bat            # build sharpmake & generate projects
generate_projects.bat    # invoke sharpmake
```

*(No vcpkg.json: we have zero third‑party deps.)*

---

## 7) Compile‑time Configuration

In `alwan_config.h`:

```c
/* Scalar selection */
#ifndef ALWAN_SCALAR_IS_FLOAT
# define ALWAN_SCALAR_IS_FLOAT 0 /* default: double */
#endif
#if ALWAN_SCALAR_IS_FLOAT
  typedef float  alwan_scalar;
#else
  typedef double alwan_scalar;
#endif

/* Data mode */
#ifndef ALWAN_EMBED_DATA
# define ALWAN_EMBED_DATA 1 /* 1=embed, 0=runtime (same /data/*.csv files) */
#endif

/* Allocation hooks (overrideable) */
#ifndef ALWAN_ALLOC
# define ALWAN_ALLOC(sz, align) alwan_default_alloc((sz),(align))
#endif
#ifndef ALWAN_FREE
# define ALWAN_FREE(p)          alwan_default_free((p))
#endif
```

---

## 8) Testing & Parity Validation

- **Authoritative fixtures** generated offline by Python **Colour** (separate script, not part of build).  
- Fixtures are stored twice:
  - as **C‑parsable arrays** (CSV) compiled into the test binary, and
  - as CSV loaded at runtime to exercise the loader (when `ALWAN_EMBED_DATA=0`).  
- For each module, run canonical cases and sweeps; assert absolute/relative error thresholds (double: `1e‑12` to `1e‑9`; float: `1e‑6` to `1e‑5`).

---

## 9) Threading Model

- The library is **thread‑agnostic**.  It does not spawn threads.  
- Read‑only ops may share a context if caches are pre‑built; otherwise one context per worker thread is simplest.

---

## 10) Versioning & Roadmap (see **alwan_plan.md**)

- v0.1: foundations + RGB matrices + sRGB TFs + core models; CMY/CMYK.  
- v0.2–v0.6: CATs, HDR TFs, spectral, CCT.  
- v0.7–v0.9: CIECAM02/CAM16, **ACES/AgX view transforms**, YCbCr/YcCbcCrc.  
- v1.0: parity sweep against Colour, API freeze.
