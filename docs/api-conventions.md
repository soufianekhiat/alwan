# Alwan API Conventions (v2.0)

This document is the single source of truth for parameter ordering and naming
across the public Alwan C API. The 2.0 release locks these conventions; every
public function in `alwan.h` must comply.

## 1. Parameter Order

The canonical signature is:

```c
return_type alwan_<verb>_<object>_<precision>[_<variant>](
    /* 1. Strided outputs — buffer immediately followed by its byte stride */
    out_buf_0, out_stride_0,
    out_buf_1, out_stride_1,
    /* …repeat for additional outputs */

    /* 2. Strided inputs — buffer immediately followed by its byte stride */
    in_buf_0,  in_stride_0,
    in_buf_1,  in_stride_1,
    /* …repeat for additional inputs */

    /* 3. Sizing — count first, then any width/height/depth */
    count [, width, height, depth, …],

    /* 4. Value-typed inputs — small structs, enums, and tuning parameters */
    value_in_0, value_in_1, …,

    /* 5. Context — last, optional (NULL allowed for stateless calls) */
    alwan_ctx *ctx
);
```

### Slots in detail

| Slot | What goes here | Examples |
|---|---|---|
| **1. Strided outputs** | Pointer-to-buffer parameters that the function writes to. Each is immediately followed by its **per-buffer layout descriptors** in this order: byte stride, then pixel format if applicable. | `alwan_f64 *xyz_out, size_t out_stride` or `void *dst, size_t dst_row_stride, alwan_pixel_format dst_fmt` |
| **2. Strided inputs** | Pointer-to-buffer parameters that the function reads from. Each is immediately followed by its per-buffer layout descriptors (stride, then format). | `alwan_f64 const *rgb_in, size_t in_stride` or `void const *src, size_t src_row_stride, alwan_pixel_format src_fmt` |
| **3. Sizing** | The count of elements processed and any auxiliary geometric sizes. | `size_t count`, `size_t width, size_t height` |
| **4. Value-typed inputs** | Small structs passed by const-pointer or by value, enums, and tuning knobs that aren't bulk data. | `alwan_xyz_f64 const *white_xyz`, `alwan_pixel_format src_fmt`, `alwan_transfer_function tf` |
| **5. Context** | The Alwan context handle. Always last. NULL is permitted for stateless calls; functions that require ctx must say so in their doc comment and return `ALWAN_E_INVALID` for NULL. | `alwan_ctx *ctx` |

### Why "ctx last"

`ctx` is plumbing — it carries allocators and configuration, not data. Putting
plumbing last keeps the data-flow ("X is produced from Y") at the front of the
signature where the reader cares. Making it last also lets call sites omit it
visually for stateless transforms (`f(out, in, count, NULL)`), which is most
of the API.

### Why "strides adjacent to buffers"

A reader scanning a long signature should not have to count argument positions
to learn which stride goes with which buffer. `(out_buf, out_stride)` and
`(in_buf, in_stride)` group as visual units. This matches BLAS/cblas (`lda`,
`ldb`, `ldc` adjacent to `A`, `B`, `C`).

## 2. Naming

- Function: `alwan_<verb>_<object>_<precision>[_<variant>]` — e.g. `alwan_xyz_to_lab_f64_map_interleave`.
- Output buffer: `<noun>_out` (e.g. `xyz_out`, `lab_out`).
- Input buffer: `<noun>_in` (e.g. `rgb_in`, `xyz_in`).
- Stride: `out_stride_N` / `in_stride_N` where `N` matches the buffer index, or
  drop the index when only one of each.
- Count: `count` for an opaque element count; `width`/`height`/`depth` for
  spatial dimensions.

## 3. Return value

- Functions that can fail return `int` with values from `alwan_status`. Never
  return raw `0` / `-1`; always use named error codes.
- Functions that cannot fail (pure math on validated inputs) may return
  `void` or the result by value.
- Out-parameters are filled only on `ALWAN_OK`; on error, callers must not
  read them.

## 4. Backwards compatibility

The 2.0 release is the convergence window. Older signatures (`ctx`-first,
trailing strides) are removed, not aliased. Downstream callers must be
migrated, not shimmed.

## 5. Lints / enforceable rules

A signature is non-compliant if any of:

- `alwan_ctx *ctx` is not the last parameter (when present).
- A `*_stride` parameter does not immediately follow the buffer it strides.
- An output buffer appears after an input buffer.
- A `count` / `width` parameter appears before a buffer or stride.

Keep this file in sync when conventions evolve. Reviewers should reject PRs
that introduce non-compliant signatures.
