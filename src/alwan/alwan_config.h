/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#ifndef ALWAN_CONFIG_H
#define ALWAN_CONFIG_H

#include "alwan_platform.h"
#include <stddef.h>
#include <stdint.h>

/* ----------------------------------------------------------------
 * Data embedding mode
 * ---------------------------------------------------------------- */
#ifndef ALWAN_EMBED_DATA
# define ALWAN_EMBED_DATA 1  /* 1=embed arrays, 0=runtime load from /data/ */
#endif

/* ----------------------------------------------------------------
 * Memory copy hook (overrideable at compile time)
 * Used for safe type punning between layout-compatible color types
 * ---------------------------------------------------------------- */
#ifndef ALWAN_MEMCPY
# include <string.h>
# define ALWAN_MEMCPY(dst, src, sz) memcpy((dst), (src), (sz))
#endif

/* ----------------------------------------------------------------
 * Allocation hooks (overrideable at compile time)
 * ---------------------------------------------------------------- */

/* Forward declarations for default allocators */
void *alwan_default_alloc(size_t size, size_t align);
void  alwan_default_free(void *ptr);
void *alwan_default_realloc(void *ptr, size_t old_size, size_t new_size, size_t align);

#ifndef ALWAN_ALLOC
# define ALWAN_ALLOC(sz, align) alwan_default_alloc((sz), (align))
#endif

#ifndef ALWAN_FREE
# define ALWAN_FREE(p) alwan_default_free((p))
#endif

#ifndef ALWAN_REALLOC
# define ALWAN_REALLOC(p, old_sz, new_sz, align) alwan_default_realloc((p), (old_sz), (new_sz), (align))
#endif

#endif /* ALWAN_CONFIG_H */
