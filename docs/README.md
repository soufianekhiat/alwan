# Alwan API Documentation

Complete reference documentation for the Alwan colour science library.

> **Note:** Runtime data loading mode (`ALWAN_EMBED_DATA=0`) is not yet implemented. Currently, only embedded mode (`ALWAN_EMBED_DATA=1`, default) is supported.

---

## Quick Navigation

### Getting Started
- **[Getting Started Guide](getting-started.md)** — First steps with Alwan
- **[Configuration](configuration.md)** — Compile-time options and build settings
- **[Examples](examples.md)** — Common use cases and code patterns

### API Reference
- **[Context Management](api/context.md)** — Library initialization and cleanup
- **[Color Spaces](api/color-spaces.md)** — 50+ color models: CIE, perceptual, encoding, HDR variants
- **[Chromatic Adaptation](api/chromatic-adaptation.md)** — White point adaptation transforms (12 CAT methods)
- **[Transfer Functions](api/transfer-functions.md)** — Encoding/decoding curves, 37+ camera logs, view transforms
- **[Matrix Operations](api/matrix-operations.md)** — 3×3 matrix math
- **[Spectral Operations](api/spectral.md)** — SPD lifecycle, integration, resampling, and upsampling
- **[Color Appearance Models](api/color-appearance.md)** — CIECAM02, CAM16, ZCAM, and 7 more CAMs
- **[Color Difference](api/color-difference.md)** — ΔE metrics (76, 94, 2000, CMC, ITP, HyAb, DIN99, CAM UCS)
- **[Gamut Operations](api/gamut.md)** — Gamut mapping, volume, coverage, and colorimetric analysis
- **[ACES Pipeline](api/aces.md)** — ACES 1.x and 2.0 output transforms, LMTs, fixed functions
- **[HDR Utilities](api/hdr.md)** — HLG OOTF, MaxCLL/MaxFALL, contrast metrics, arbitrary gamma
- **[Color Correction & Grading](api/color-correction.md)** — LGG, color matrix, printer lights, white balance, camera profiling
- **[Color Vision Deficiency](api/color-vision-deficiency.md)** — CVD simulation (Brettel 1997)
- **[CCT & Light Quality](api/cct-light-quality.md)** — CCT estimation, CRI, CQS, TM-30, SSI, whiteness/yellowness
- **[Vision Science](api/vision.md)** — Barten CSF, luminous efficiency, pupil response
- **[Atmospheric Optics](api/atmosphere.md)** — Rayleigh scattering (Bodhaine 1999)
- **[Reference Data](api/reference-data.md)** — Munsell, ColorChecker, NCS, interpolation, LUT
- **[Bulk Operations / Map](api/map.md)** — Batch pixel processing with stride and pixel format support
- **[SIMD](api/simd.md)** — SIMD-accelerated bulk operations
- **[GPU Backends](api/backends.md)** — HLSL, GLSL, and Halide backend usage

### Technical Details
- **[Precision & Limits](precision-and-limits.md)** — Numerical accuracy, tolerances, and constraints
- **[Data Management](data-management.md)** — Embedded vs runtime data loading

---

## API Conventions

### Function Naming
All public API functions use the `alwan_` prefix:
```c
alwan_xyz_to_lab()     // Color space conversion
alwan_mat3_inv()       // Matrix operation
alwan_create()         // Context management
```

### Parameter Order
Functions follow consistent parameter ordering (output-first convention):
1. **Output data pointer(s)** — ALL outputs come first
2. Context (if required)
3. Configuration/descriptor parameters
4. Input data pointer (const)
5. Count (for bulk operations)
6. Input stride
7. Output stride

Example:
```c
alwan_xyz_to_lab(
    alwan_vec3 *lab_out,         // Output (FIRST)
    const alwan_vec3 *xyz_in,    // Input
    const alwan_vec3 *white_pt,  // Config
    size_t count,                // Count
    size_t in_stride,            // Input stride
    size_t out_stride            // Output stride
);
```

### Stride Support
Bulk functions support stride parameters for processing interleaved data:
- **Stride in bytes**: Distance between consecutive elements
- Typically `3*sizeof(alwan_scalar)` for packed RGB/XYZ data

Example with stride:
```c
// RGB data interleaved with alpha: RGBARGBARGBA...
alwan_scalar rgba_data[100 * 4];
alwan_scalar encoded[100 * 4];

// Process only RGB channels, skip alpha (stride = 4 * sizeof(alwan_scalar))
alwan_oetf_apply(encoded, ALWAN_TF_SRGB, rgba_data, 100,
                 4 * sizeof(alwan_scalar),   // in_stride (skip alpha)
                 4 * sizeof(alwan_scalar));  // out_stride
```

### Error Handling
Most functions return status codes:
- `ALWAN_OK` (0) — Operation completed successfully
- `ALWAN_E_INVALID` (-1) — Invalid argument
- `ALWAN_E_NODATA` (-2) — Data not found
- `ALWAN_E_RANGE` (-3) — Value out of range
- `ALWAN_E_NOMEM` (-4) — Memory allocation failed
- `ALWAN_E_DIVZERO` (-5) — Division by zero

Check return values for operations that can fail:
```c
alwan_rgb_space_desc src_desc, dst_desc;
alwan_rgb_get_space_descriptor(&src_desc, ctx, ALWAN_RGB_SPACE_SRGB);
alwan_rgb_get_space_descriptor(&dst_desc, ctx, ALWAN_RGB_SPACE_BT2020);

alwan_rgb rgb_out;
int status = alwan_rgb_convert(&rgb_out, ctx, &src_desc, &dst_desc, &rgb_in);
if (status != ALWAN_OK) {
    // Handle error
}
```

### Memory Ownership
- **Input parameters**: Caller retains ownership, data is read-only
- **Output parameters**: Caller provides pre-allocated buffers
- **Context objects**: Managed via `alwan_create()` / `alwan_destroy()`

---

## Type Reference

### Core Types

```c
/* alwan_scalar is an internal type used in the GPU-compatible core layer.
 * All public API functions and types have explicit _f32 (float) and _f64 (double) variants. */
typedef ALWAN_SCALAR alwan_scalar;

// 2-component vector (for xy chromaticity coordinates)
typedef struct {
    alwan_scalar v[2];
} alwan_vec2;

// 3-component vector (generic)
typedef struct {
    alwan_scalar v[3];
} alwan_vec3;

// 3×3 matrix (row-major, flat array)
typedef struct {
    alwan_scalar m[9];  // [m00 m01 m02 m10 m11 m12 m20 m21 m22]
} alwan_mat3x3;

// Library context (opaque)
typedef struct alwan_ctx alwan_ctx;

// Status codes
typedef enum {
    ALWAN_OK       =  0,  // Success
    ALWAN_E_INVALID = -1, // Invalid argument
    ALWAN_E_NODATA = -2,  // Data not found or not loaded
    ALWAN_E_RANGE  = -3,  // Value out of valid range
    ALWAN_E_NOMEM  = -4,  // Memory allocation failed
    ALWAN_E_DIVZERO = -5  // Division by zero would occur
} alwan_status;
```

### Semantic Color Types

Alwan uses semantic types for type safety (all layout-compatible with `alwan_vec3`):

```c
typedef struct { alwan_scalar r, g, b; } alwan_rgb;
typedef struct { alwan_scalar x, y, z; } alwan_xyz;
typedef struct { alwan_scalar L, a, b; } alwan_lab;
typedef struct { alwan_scalar L, u, v; } alwan_luv;
typedef struct { alwan_scalar L, C, h; } alwan_lch;
typedef struct { alwan_scalar L, a, b; } alwan_oklab;
typedef struct { alwan_scalar L, C, h; } alwan_oklch;
typedef struct { alwan_scalar Jz, az, bz; } alwan_jzazbz;
typedef struct { alwan_scalar I, Ct, Cp; } alwan_ictcp;
typedef struct { alwan_scalar h, s, v; } alwan_hsv;
typedef struct { alwan_scalar h, s, l; } alwan_hsl;
typedef struct { alwan_scalar h, s, p; } alwan_hsp;   // Perceived brightness (Finley 2006)
typedef struct { alwan_scalar h, s, p; } alwan_hsplog; // HSP with log saturation stretching
typedef struct { alwan_scalar h, s, y; } alwan_hsy;   // Luma-weighted (chilliant HCY)
typedef struct { alwan_scalar Y, Cb, Cr; } alwan_ycbcr;
typedef struct { alwan_scalar Y, Co, Cg; } alwan_ycocg;
// ... and more
```

### Configuration Types

```c
// Context configuration
typedef struct {
    alwan_alloc_fn alloc_cb;          // Optional custom allocator (NULL = default)
    alwan_free_fn  free_cb;           // Optional custom deallocator (NULL = default)
    char const *runtime_data_root;    // Optional data path for ALWAN_EMBED_DATA=0
    uint32_t flags;                   // Reserved for future use (must be 0)
} alwan_config;

// RGB space descriptor
typedef struct {
    alwan_scalar primaries_xy[6];  // rx, ry, gx, gy, bx, by in CIE xy chromaticity
    alwan_scalar white_xy[2];       // wx, wy in CIE xy chromaticity
    alwan_transfer_function oetf;   // OETF (use ALWAN_TF_LINEAR for none)
    alwan_transfer_function eotf;   // EOTF (use ALWAN_TF_LINEAR for none)
} alwan_rgb_space_desc;

// Chromatic adaptation transform methods
typedef enum {
    ALWAN_CAT_XYZ_SCALING = 0,
    ALWAN_CAT_BRADFORD    = 1,
    ALWAN_CAT_VON_KRIES   = 2,
    ALWAN_CAT_CAT02       = 3,
    ALWAN_CAT_CAT16       = 4,
    ALWAN_CAT_CMCCAT97    = 5,
    ALWAN_CAT_CMCCAT2000  = 6,
    ALWAN_CAT_SHARP       = 7,
    ALWAN_CAT_BIANCO_SCHETTINI_2010 = 8,
    ALWAN_CAT_FAIRCHILD   = 9,
    ALWAN_CAT_HUNT_POINTER_ESTEVEZ = 10,
    ALWAN_CAT_ZHAI_2018   = 11
} alwan_cat_method;
```

---

## Standard Illuminants

Illuminant data is accessed via data getter functions:

```c
alwan_scalar *data;
size_t count;

// Get D65 illuminant xy chromaticity
alwan_data_get_illuminant_d65(&data, &count, ctx);
// data[0] = x, data[1] = y

// Get illuminant by enum
alwan_data_get_illuminant_xy(&data, &count, ctx, ALWAN_ILLUMINANT_D50);
```

### Common Illuminant Values (Y=1.0 normalized XYZ)

| Illuminant | X | Y | Z |
|-----------|-----|-----|-----|
| D65 | 0.95047 | 1.00000 | 1.08883 |
| D50 | 0.96422 | 1.00000 | 0.82521 |
| A | 1.09850 | 1.00000 | 0.35585 |
| E | 1.00000 | 1.00000 | 1.00000 |

**Note:** Create white point XYZ values directly:
```c
alwan_xyz d65_white = {0.95047, 1.0, 1.08883};
alwan_xyz d50_white = {0.96422, 1.0, 0.82521};
```

---

## Thread Safety

Alwan is designed for multi-threaded use with the following guarantees:

- **Context objects (`alwan_ctx`)**: Thread-safe for concurrent reads if not modified
- **Transform functions**: Re-entrant, safe to call from multiple threads
- **No global state**: All state is explicit in context or parameters

**Recommended pattern for multi-threading:**
```c
// Create one context per thread, or use read-only context shared across threads
alwan_ctx *ctx = alwan_create(NULL);
alwan_xyz d65_white = {0.95047, 1.0, 1.08883};

// Safe to call from multiple threads (single-element functions are re-entrant)
#pragma omp parallel for
for (int i = 0; i < n; i++) {
    alwan_xyz_to_lab(&lab[i], &xyz[i], &d65_white);
}

// Or use bulk functions for better performance
alwan_xyz_to_lab_bulk((alwan_scalar*)lab, (alwan_scalar*)xyz, &d65_white,
                      n, sizeof(alwan_lab), sizeof(alwan_xyz));

alwan_destroy(ctx);
```

---

## Performance Considerations

### Bulk Operations
Always prefer bulk operations over single-element loops:

**Slow:**
```c
alwan_xyz d65_white = {0.95047, 1.0, 1.08883};
for (int i = 0; i < 1000; i++) {
    alwan_xyz_to_lab(&lab[i], &xyz[i], &d65_white);
}
```

**Fast:**
```c
alwan_xyz d65_white = {0.95047, 1.0, 1.08883};
alwan_xyz_to_lab_bulk((alwan_scalar*)lab, (alwan_scalar*)xyz, &d65_white,
                      1000, sizeof(alwan_xyz), sizeof(alwan_lab));
```

### Precision Selection
Choose precision based on your needs:
- **Float (f32)**: ~7 decimal digits, 2× faster, sufficient for most graphics
- **Double (f64)**: ~16 decimal digits, required for scientific accuracy

### Data Embedding
- **Embedded mode** (default): Zero I/O overhead, larger binary (~few MB)
- **Runtime mode**: Smaller binary, one-time load cost at initialization

---

## Next Steps

- **New to Alwan?** Start with the [Getting Started Guide](getting-started.md)
- **Need examples?** See [Examples](examples.md)
- **Looking for specific API?** Browse the [API Reference](api/) folder
- **Performance questions?** Read [Precision & Limits](precision-and-limits.md)
