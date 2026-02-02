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
- **[Color Spaces](api/color-spaces.md)** — Conversions between color models
- **[Chromatic Adaptation](api/chromatic-adaptation.md)** — White point adaptation transforms
- **[Transfer Functions](api/transfer-functions.md)** — Encoding/decoding curves and view transforms
- **[Matrix Operations](api/matrix-operations.md)** — 3×3 matrix math
- **[Spectral Operations](api/spectral.md)** — Spectral power distribution calculations
- **[Color Appearance Models](api/color-appearance.md)** — CIECAM02, CAM16
- **[Color Difference](api/color-difference.md)** — ΔE metrics
- **[Gamut Operations](api/gamut.md)** — Gamut mapping and analysis

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
Most transform functions support stride parameters for processing interleaved data:
- **Stride = 0**: Tightly packed array (equivalent to stride = sizeof(type))
- **Stride = N**: N bytes between consecutive elements

Example with stride:
```c
// RGB data interleaved with alpha: RGBARGBARGBA...
float rgba_data[100 * 4];
alwan_oetf_apply(rgba_data, ctx, "srgb", rgba_data, 100, 16, 16);
//               ^^^^^^^^                                ^^ ^^
//          output first                    strides at end (bytes)
```

### Error Handling
Most functions return status codes:
- `ALWAN_SUCCESS` (0) — Operation completed successfully
- `ALWAN_ERROR_*` — Specific error codes

Check return values for operations that can fail:
```c
alwan_result result = alwan_rgb_convert(ctx, "srgb", "bt2020", ...);
if (result != ALWAN_SUCCESS) {
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
// Scalar type (float or double based on ALWAN_SCALAR_IS_FLOAT)
typedef ALWAN_SCALAR alwan_scalar;

// 3D vector
typedef struct {
    alwan_scalar x, y, z;
} alwan_vec3;

// 3×3 matrix (row-major)
typedef struct {
    alwan_scalar m[3][3];
} alwan_mat3x3;

// Library context
typedef struct alwan_ctx alwan_ctx;

// Result codes
typedef enum {
    ALWAN_SUCCESS = 0,
    ALWAN_ERROR_INVALID_PARAMETER,
    ALWAN_ERROR_OUT_OF_MEMORY,
    ALWAN_ERROR_NOT_FOUND,
    ALWAN_ERROR_UNSUPPORTED
} alwan_result;
```

### Configuration Types

```c
// Context configuration
typedef struct {
    void* (*alloc_cb)(size_t size, size_t align);
    void (*free_cb)(void *ptr);
    const char *runtime_data_root;  // Only for ALWAN_EMBED_DATA=0
} alwan_config;

// RGB space descriptor
typedef struct {
    alwan_vec2 primaries[3];  // R, G, B chromaticity
    alwan_vec2 white_point;   // White point chromaticity
    const char *transfer;     // Transfer function name
} alwan_rgb_descriptor;

// Chromatic adaptation transform types
typedef enum {
    ALWAN_CAT_BRADFORD,
    ALWAN_CAT_CAT02,
    ALWAN_CAT_CAT16,
    ALWAN_CAT_XYZ_SCALING
} alwan_cat_type;
```

---

## Constants & Standard Values

### Standard Illuminants (D65, D50, etc.)
```c
extern const alwan_vec3 alwan_d65_xyz;  // D65 white point in XYZ
extern const alwan_vec3 alwan_d50_xyz;  // D50 white point in XYZ
extern const alwan_vec3 alwan_a_xyz;    // Illuminant A in XYZ
extern const alwan_vec3 alwan_e_xyz;    // Equal energy illuminant
```

### Common Chromaticities
```c
extern const alwan_vec2 alwan_d65_xy;   // D65 in xy
extern const alwan_vec2 alwan_d50_xy;   // D50 in xy
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

// Safe to call from multiple threads
#pragma omp parallel for
for (int i = 0; i < n; i++) {
    alwan_xyz_to_lab(&lab[i], &xyz[i], &d65, 1, 0, 0);
}

alwan_destroy(ctx);
```

---

## Performance Considerations

### Bulk Operations
Always prefer bulk operations over single-element loops:

**Slow:**
```c
for (int i = 0; i < 1000; i++) {
    alwan_xyz_to_lab(&lab[i], &xyz[i], &d65, 1, 0, 0);
}
```

**Fast:**
```c
alwan_xyz_to_lab(lab, xyz, &d65, 1000, sizeof(alwan_vec3), sizeof(alwan_vec3));
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
