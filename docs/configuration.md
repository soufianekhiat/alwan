# Configuration

Compile-time configuration options for Alwan.

> **⚠️ Runtime Data Loading:** Runtime data loading mode (`ALWAN_EMBED_DATA=0`) is not yet fully implemented in v0.1-alpha. Currently, only embedded mode (`ALWAN_EMBED_DATA=1`, default) is supported.

---

## Overview

Alwan's behavior is controlled through compile-time macros defined in [alwan_config.h](../src/alwan/alwan_config.h) or via compiler flags. This approach enables:

- **Zero runtime overhead** for disabled features
- **Dead code elimination** by the compiler
- **Binary size optimization**
- **Platform-specific tuning**

---

## Scalar Precision

### ALWAN_SCALAR_IS_FLOAT

```c
#define ALWAN_SCALAR_IS_FLOAT 0  // 0 = double (default), 1 = float
```

Controls the underlying scalar type for all calculations.

**Options:**
- `0` — Use `double` (64-bit, ~16 decimal digits)
- `1` — Use `float` (32-bit, ~7 decimal digits)

**Impact:**
| Aspect | Float | Double |
|--------|-------|--------|
| Precision | ±1e-6 | ±1e-12 |
| Memory | 50% less | Baseline |
| Performance | Up to 2× faster | Baseline |
| Binary size | Slightly smaller | Baseline |

**Setting via compiler:**
```batch
cl /D ALWAN_SCALAR_IS_FLOAT=1 ...
```

**When to use float:**
- Real-time graphics (>30 fps)
- Memory-constrained systems
- 8-bit or 10-bit image processing
- Mobile platforms

**When to use double:**
- Scientific accuracy required
- High bit-depth images (>12-bit)
- Validation and testing
- Chained conversions (>5 steps)

**See also:** [Precision & Limits](precision-and-limits.md)

---

## Data Embedding

### ALWAN_EMBED_DATA

```c
#define ALWAN_EMBED_DATA 1  // 1 = embed (default), 0 = runtime load
```

Controls how reference data (CMFs, illuminants, RGB spaces) is included.

**Options:**
- `1` — Embed data in binary (default)
- `0` — Load data from CSV files at runtime

### Embedded Mode (ALWAN_EMBED_DATA=1)

**Advantages:**
- ✓ Zero I/O at runtime
- ✓ Instant initialization
- ✓ No file dependencies
- ✓ Works in sandboxed environments

**Disadvantages:**
- ✗ Larger binary size (~2-5 MB)
- ✗ Data updates require recompilation

**Binary size impact:**
| Component | Size |
|-----------|------|
| CMFs | ~500 KB |
| Illuminants | ~300 KB |
| RGB spaces | ~100 KB |
| Other data | ~100 KB |
| **Total** | **~1-2 MB** |

**Example:**
```c
// No configuration needed, data is in binary
alwan_ctx *ctx = alwan_create(NULL);
// Instant initialization
```

---

### Runtime Mode (ALWAN_EMBED_DATA=0)

**Advantages:**
- ✓ Smaller binary size
- ✓ Data can be updated without recompiling
- ✓ Faster linking during development

**Disadvantages:**
- ✗ Requires file I/O at initialization (~10-50 ms)
- ✗ CSV files must be deployed with binary
- ✗ Potential for missing/corrupt data errors

**Required:** Set `runtime_data_root` in config:
```c
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .runtime_data_root = "C:/Program Files/MyApp/data"
});
```

**Data directory structure:**
```
data/
├── cmf/
│   ├── cie_1931_2deg_x_360_830_1nm.csv
│   ├── cie_1931_2deg_y_360_830_1nm.csv
│   └── ...
├── illuminants/
│   ├── D65_360_830_1nm.csv
│   └── ...
└── rgb_spaces/
    ├── srgb.csv
    └── ...
```

**Error handling:**
```c
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .runtime_data_root = "./data"
});

if (!ctx) {
    fprintf(stderr, "Failed to load data from ./data\n");
    // Check:
    // - Path exists and is readable
    // - All required CSV files present
    // - CSV files not corrupted
}
```

---

## Memory Allocation

### ALWAN_ALLOC

```c
#define ALWAN_ALLOC(size, align) malloc(size)
```

Custom memory allocator for Alwan's internal allocations.

**Signature:**
```c
void* ALWAN_ALLOC(size_t size, size_t align);
```

**Parameters:**
- `size` — Bytes to allocate
- `align` — Required alignment (power of 2, e.g., 16)

**Returns:**
- Pointer to allocated memory, or `NULL` on failure

**Default:** Uses `malloc` (alignment is not guaranteed on all platforms)

---

### ALWAN_FREE

```c
#define ALWAN_FREE(ptr) free(ptr)
```

Custom memory deallocator.

**Signature:**
```c
void ALWAN_FREE(void *ptr);
```

**Parameters:**
- `ptr` — Pointer to free (may be `NULL`)

**Default:** Uses `free`

---

### Custom Allocator Example

**Game engine integration:**
```c
// my_config.h
#define ALWAN_ALLOC(sz, align) MyEngine::AllocAligned(sz, align)
#define ALWAN_FREE(ptr) MyEngine::FreeAligned(ptr)

#include "alwan.h"
```

**Memory tracking:**
```c
static size_t total_allocated = 0;

void* tracked_alloc(size_t size, size_t align) {
    void *ptr = _aligned_malloc(size, align);
    if (ptr) total_allocated += size;
    return ptr;
}

void tracked_free(void *ptr) {
    if (ptr) {
        // Note: Size tracking requires wrapper
        _aligned_free(ptr);
    }
}

#define ALWAN_ALLOC(sz, align) tracked_alloc(sz, align)
#define ALWAN_FREE(ptr) tracked_free(ptr)
```

**Stack allocator (small allocations only):**
```c
#define ALWAN_ALLOC(sz, align) alloca(sz)
#define ALWAN_FREE(ptr) ((void)0)  // No-op for stack
```

⚠️ **Warning:** Stack allocators are risky for runtime data loading mode.

---

## Compiler Warnings

### Diagnostic Control

Alwan includes compiler-specific pragmas to suppress warnings from embedded data:

```c
// Suppress float conversion warnings for CSV data
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4305 4244)
#elif defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wconversion"
#endif

// ... embedded data ...

#if defined(_MSC_VER)
    #pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#endif
```

These are internal to Alwan and require no user configuration.

---

## Build Configurations

Alwan's Sharpmake build system provides pre-configured combinations:

### Debug_f32

```
ALWAN_SCALAR_IS_FLOAT = 1
Optimization = None
Debug symbols = Yes
```

**Use for:** Fast iteration, graphics debugging

---

### Release_f32

```
ALWAN_SCALAR_IS_FLOAT = 1
Optimization = Full
Debug symbols = No
```

**Use for:** Production graphics applications

---

### Debug_f64

```
ALWAN_SCALAR_IS_FLOAT = 0
Optimization = None
Debug symbols = Yes
```

**Use for:** Scientific validation, algorithm development

---

### Release_f64

```
ALWAN_SCALAR_IS_FLOAT = 0
Optimization = Full
Debug symbols = No
```

**Use for:** High-precision production, offline rendering

---

## Platform-Specific Options

### Windows (MSVC)

**Recommended flags:**
```
/std:c11         # C11 standard
/W4              # Warning level 4
/WX              # Warnings as errors
/TC              # Force C compilation
/O2              # Optimize for speed
/fp:precise      # Precise floating-point
```

**Fast math (optional):**
```
/fp:fast         # Allow FP optimizations (may reduce accuracy)
```

⚠️ Use `/fp:fast` only if accuracy loss is acceptable.

---

### GCC / Clang

**Recommended flags:**
```
-std=c11
-Wall -Wextra
-Werror
-O2
-fno-fast-math   # Disable aggressive FP optimizations
```

**Architecture-specific:**
```
-march=native    # Use CPU-specific instructions (including SIMD)
-mtune=native    # Optimize for local CPU
```

---

### ARM / Embedded

**Considerations:**
- Use `float` (f32) for best performance on low-power ARM
- Disable runtime data loading (`ALWAN_EMBED_DATA=1`)
- Enable hardware FPU if available
- Consider reduced illuminant/CMF sets for memory savings

**Example flags:**
```
-mfpu=neon       # Enable NEON SIMD
-mfloat-abi=hard # Hardware floating-point ABI
```

---

## Advanced Configuration

### Custom Data Subset

For embedded systems with limited memory, create a custom build with only required data:

**1. Edit data inclusion lists** (in Alwan source)
**2. Remove unused illuminants**
**3. Remove unused CMFs**
**4. Keep only required RGB spaces**

**Example:** Remove all but D65 and sRGB reduces data size by ~80%.

---

## Validation

### Verify Configuration

```c
#include "alwan.h"
#include <stdio.h>

int main(void) {
    printf("Scalar size: %zu bytes\n", sizeof(alwan_scalar));
    printf("Embed data: %d\n", ALWAN_EMBED_DATA);

    alwan_ctx *ctx = alwan_create(NULL);
    if (ctx) {
        printf("Context created successfully\n");
        alwan_destroy(ctx);
    } else {
        printf("Context creation failed\n");
    }

    return 0;
}
```

**Expected output:**
```
Scalar size: 8 bytes          (or 4 for float)
Embed data: 1                 (or 0 for runtime)
Context created successfully
```

---

## Configuration Templates

### Template: Game Engine

```c
// alwan_game_config.h
#define ALWAN_SCALAR_IS_FLOAT 1              // Float precision
#define ALWAN_EMBED_DATA 1                    // Embedded data
#define ALWAN_ALLOC(sz, align) GameAlloc(sz, align)
#define ALWAN_FREE(ptr) GameFree(ptr)
```

---

### Template: Scientific Tool

```c
// alwan_science_config.h
#define ALWAN_SCALAR_IS_FLOAT 0              // Double precision
#define ALWAN_EMBED_DATA 0                    // Runtime loading
// Use default system allocator
```

---

### Template: Mobile App

```c
// alwan_mobile_config.h
#define ALWAN_SCALAR_IS_FLOAT 1              // Float for performance
#define ALWAN_EMBED_DATA 1                    // No file I/O on mobile
#define ALWAN_ALLOC(sz, align) malloc(sz)    // System allocator OK
#define ALWAN_FREE(ptr) free(ptr)
```

---

## Regenerating Projects

After changing configuration in `.cs` Sharpmake files:

```batch
generate_projects.bat
```

This regenerates all Visual Studio solution and project files with new settings.

---

## See Also

- [Precision & Limits](precision-and-limits.md) — Impact of scalar choice
- [Context Management](api/context.md) — Runtime configuration
- [Data Management](data-management.md) — Embedded vs runtime data
- [Getting Started](getting-started.md) — Build instructions
