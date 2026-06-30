# Context Management API

Context objects (`alwan_ctx`) manage library state and memory allocation.

Functions that take a `ctx` (e.g. `alwan_rgb_get_space_descriptor_{T}`, `alwan_rgb_convert_{T}`) accept it as the handle through which the **allocator** and — in a future runtime mode — disk-loaded data are reached. In the current **embedded** build the RGB color-space registry (primaries/whitepoint/transfer-function descriptors) is compiled into the binary from `src/alwan/data/**`, so descriptor lookups index static tables and ignore `ctx` (it may even be `NULL`). The context does **not** load anything from disk at runtime. Where `ctx` matters today: it supplies the allocator for functions that allocate (e.g. SPD/LUT routines) and gates optional work such as the chromatic-adaptation step inside `alwan_rgb_convert_{T}` (passing `NULL` skips adaptation rather than erroring). Per the v2.0 parameter convention, `ctx` is always the **last** argument (or absent on `_v` value-typed math).

> **Note:** Runtime data loading (`runtime_data_root`) is NOT implemented. Only embedded mode (`ALWAN_EMBED_DATA=1`, the default) is supported. Runtime mode is planned for alwan 3.0.0.

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
    alwan_alloc_fn alloc_cb;          // Optional custom allocator (NULL = default)
    alwan_free_fn  free_cb;           // Optional custom deallocator (NULL = default)
    char const *runtime_data_root;    // Optional data path for ALWAN_EMBED_DATA=0
    uint32_t flags;                   // Reserved for future use (must be 0)
} alwan_config;
```

Configuration for context creation.

**Fields:**

#### `alloc_cb`
Custom allocation callback for **context-lifetime** allocations — the context
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
- `size` — Number of bytes to allocate
- `align` — Required alignment (power of 2)

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
- `ptr` — Pointer to free (can be `NULL`)

**Default:** `alwan_default_free` (matches `alwan_default_alloc`; uses `_aligned_free` on MSVC, `free` elsewhere)

#### `runtime_data_root`
Reserved. Runtime data loading is NOT implemented (planned for alwan 3.0.0).

**Type:** `const char*`

**Default:** `NULL`

This field is currently ignored. Building with `ALWAN_EMBED_DATA=0` produces a compile-time error.
Set to `NULL` until runtime loading is implemented in alwan 3.0.0.

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

### Runtime Mode (ALWAN_EMBED_DATA=0) — NOT IMPLEMENTED

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
> (your custom function is paired with the default for the other) — make sure
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

- [Configuration](../configuration.md) — Compile-time options
- [Data Management](../data-management.md) — Embedded vs runtime data
- [Examples](../examples.md) — Complete usage examples
