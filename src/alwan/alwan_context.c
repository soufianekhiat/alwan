/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Default allocators
 * ---------------------------------------------------------------- */

void *alwan_default_alloc(size_t size, size_t align) {
    /* Normalize alignment to be valid for aligned allocation functions.
     * Alignment must be power of 2 and >= sizeof(void*) for _aligned_malloc/aligned_alloc */
#if defined(_WIN32) && defined(_MSC_VER)
    /* On Windows, always use _aligned_malloc with normalized alignment
     * so that _aligned_free is always correct */
    if (align < sizeof(void *)) {
        align = sizeof(void *);
    }
    /* Ensure power of 2 */
    if ((align & (align - 1)) != 0) {
        /* Round up to next power of 2 */
        size_t p = sizeof(void *);
        while (p < align) p *= 2;
        align = p;
    }
    return _aligned_malloc(size, align);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    /* For small or invalid alignments, use plain malloc */
    if (align < sizeof(void *) || (align & (align - 1)) != 0) {
        return malloc(size);
    }
    /* C11 aligned_alloc requires size to be a multiple of align */
    if ((size % align) != 0) {
        size = ((size + align - 1) / align) * align;
    }
    return aligned_alloc(align, size);
#else
    /* Fallback to malloc - suitable for doubles/floats */
    (void)align;
    return malloc(size);
#endif
}

void alwan_default_free(void *ptr) {
    if (!ptr) return;
#if defined(_WIN32) && defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void *alwan_default_realloc(void *ptr, size_t old_size, size_t new_size, size_t align) {
    if (new_size == 0) {
        alwan_default_free(ptr);
        return NULL;
    }

    if (!ptr) {
        return alwan_default_alloc(new_size, align);
    }

    /* For aligned allocations, we need to allocate new, copy, and free old */
    void *new_ptr = alwan_default_alloc(new_size, align);
    if (!new_ptr) {
        return NULL;
    }

    /* Copy old data to new allocation */
    size_t copy_size = (old_size < new_size) ? old_size : new_size;
    if (copy_size > 0) {
        memcpy(new_ptr, ptr, copy_size);
    }

    alwan_default_free(ptr);
    return new_ptr;
}

/* struct alwan_ctx is defined in alwan_internal.h */

/* ----------------------------------------------------------------
 * Context API
 * ---------------------------------------------------------------- */

alwan_ctx *alwan_create(alwan_config const *cfg) {
    /* Use defaults for NULL config */
    alwan_alloc_fn alloc_fn = alwan_default_alloc;
    alwan_free_fn  free_fn  = alwan_default_free;
    char const *data_root   = NULL;
    uint32_t flags          = 0;

    if (cfg) {
        if (cfg->alloc_cb) alloc_fn = cfg->alloc_cb;
        if (cfg->free_cb)  free_fn  = cfg->free_cb;
        data_root = cfg->runtime_data_root;
        flags     = cfg->flags;
    }

    /* Allocate context */
    alwan_ctx *ctx = (alwan_ctx *)alloc_fn(sizeof(alwan_ctx), 16);
    if (!ctx) return NULL;

    /* Initialize */
    memset(ctx, 0, sizeof(alwan_ctx));
    ctx->alloc_fn = alloc_fn;
    ctx->free_fn  = free_fn;
    ctx->flags    = flags;

    /* Copy data root if provided */
    if (data_root) {
        size_t len = strlen(data_root);
        ctx->runtime_data_root = (char *)alloc_fn(len + 1, 1);
        if (!ctx->runtime_data_root) {
            free_fn(ctx);
            return NULL;
        }
        memcpy(ctx->runtime_data_root, data_root, len + 1);
    }

    return ctx;
}

void alwan_destroy(alwan_ctx *ctx) {
    if (!ctx) return;

    /* Free owned data root string */
    if (ctx->runtime_data_root) {
        ctx->free_fn(ctx->runtime_data_root);
    }

    /* Free context itself */
    ctx->free_fn(ctx);
}
