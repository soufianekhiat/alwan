# Context Management API

Context objects (`alwan_ctx`) manage library state, memory allocation, and reference data.

> **Note:** Runtime data loading (`runtime_data_root`) is not yet implemented. Only embedded mode works.

---

## Functions

### alwan_create

```c
alwan_ctx* alwan_create(const alwan_config *config);
```

Creates and initializes a new Alwan context.

**Parameters:**
- `config` — Configuration structure, or `NULL` for defaults

**Returns:**
- Pointer to new context on success
- `NULL` on allocation failure or initialization error

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
- `ctx` — Context to destroy (can be `NULL`, in which case this is a no-op)

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
    void* (*alloc_cb)(size_t size, size_t align);
    void (*free_cb)(void *ptr);
    const char *runtime_data_root;
} alwan_config;
```

Configuration for context creation.

**Fields:**

#### `alloc_cb`
Custom allocation callback.

**Signature:**
```c
void* alloc_cb(size_t size, size_t align);
```

**Parameters:**
- `size` — Number of bytes to allocate
- `align` — Required alignment (power of 2)

**Returns:**
- Pointer to aligned memory, or `NULL` on failure

**Default:** System `malloc` (ignores alignment on platforms without aligned allocation)

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
- `ptr` — Pointer to free (can be `NULL`)

**Default:** System `free`

#### `runtime_data_root`
Path to data directory for runtime loading.

**Type:** `const char*`

**Default:** `NULL` (not used in embedded mode)

**Only relevant when `ALWAN_EMBED_DATA=0`.**

**Example:**
```c
// Runtime data loading
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .runtime_data_root = "C:/alwan/data"
});
```

**Path format:**
- Absolute or relative paths supported
- Use forward slashes `/` or backslashes `\\`
- Must point to folder containing CSV files

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

### Pattern 3: Runtime Data Loading

**Requirements:**
- Build with `ALWAN_EMBED_DATA=0`
- CSV files accessible at runtime

```c
alwan_ctx *ctx = alwan_create(&(alwan_config){
    .runtime_data_root = "./data"
});

if (!ctx) {
    fprintf(stderr, "Failed to load data from ./data\n");
    return -1;
}

// Context loaded data from disk

alwan_destroy(ctx);
```

**Use when:**
- Binary size is critical
- Data needs to be updated without recompiling
- Lazy loading is acceptable

---

### Pattern 4: Multi-threaded Context Sharing

```c
// Create one shared context
alwan_ctx *shared_ctx = alwan_create(NULL);

// Use from multiple threads (read-only operations)
#pragma omp parallel for
for (int i = 0; i < n; i++) {
    // Safe: read-only color space conversion
    alwan_rgb_convert(&out[i], shared_ctx, "srgb", "bt2020", &rgb[i], 1, 0, 0);
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

### Runtime Mode (ALWAN_EMBED_DATA=0)

**Allocation during `alwan_create()`:**
- Context structure: ~few KB
- CMF data: ~50-200 KB (depending on observers)
- Illuminant data: ~20-100 KB
- RGB space data: ~10-50 KB

**Total runtime memory:** ~100-400 KB per context

**Load time:** 10-50 ms on modern systems (one-time cost)

---

## Error Handling

### Context Creation Failure

```c
alwan_ctx *ctx = alwan_create(config);
if (!ctx) {
    // Possible causes:
    // - Allocation failure
    // - Data files not found (runtime mode)
    // - Invalid data files (runtime mode)
    // - Invalid configuration
}
```

**Debugging:**
1. Check `config` pointers are valid
2. Verify `runtime_data_root` path exists (runtime mode)
3. Ensure allocator callbacks work correctly
4. Check available memory

---

### Null Context Handling

Most API functions handle `NULL` context gracefully:

```c
alwan_destroy(NULL);  // Safe, does nothing

// Functions requiring context will return error
alwan_result r = alwan_rgb_convert(NULL, ...);
// r == ALWAN_ERROR_INVALID_PARAMETER
```

---

## Limits

- **Maximum contexts:** Limited only by available memory
- **Context size:** < 10 KB (embedded), < 500 KB (runtime)
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

- [Configuration](../configuration.md) — Compile-time options
- [Data Management](../data-management.md) — Embedded vs runtime data
- [Examples](../examples.md) — Complete usage examples
