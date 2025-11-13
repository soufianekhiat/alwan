/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 */

#include "alwan.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Default allocators
 * ---------------------------------------------------------------- */

void *alwan_default_alloc(size_t size, size_t align) {
    (void)align;  /* Most platforms don't need special alignment for our use */
#if defined(_WIN32) && defined(_MSC_VER)
    return _aligned_malloc(size, align);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    /* C11 aligned_alloc requires size to be a multiple of align */
    if (align > 0 && (size % align) != 0) {
        size = ((size + align - 1) / align) * align;
    }
    return aligned_alloc(align, size);
#else
    /* Fallback to malloc - suitable for doubles/floats */
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

/* ----------------------------------------------------------------
 * Internal context structure
 * ---------------------------------------------------------------- */

struct alwan_ctx {
    /* Allocation callbacks */
    alwan_alloc_fn alloc_fn;
    alwan_free_fn  free_fn;

    /* Configuration */
    char *runtime_data_root;  /* Owned copy (if non-NULL) */
    uint32_t flags;

    /* Future: data cache, registry, etc. */
};

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
