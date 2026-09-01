/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#ifndef ALWAN_CONFIG_H
#define ALWAN_CONFIG_H

#include "alwan_platform.h"

/* A shading language has no C standard library. HLSL and GLSL get the size and
 * fixed-width spellings from alwan_types.h instead; Halide is a C++ eDSL, so it
 * takes the real headers. Without this guard, every core that reaches
 * alwan_config.h (table, lut, vision) fails to compile as a shader on the
 * include line, before any of its own code is even parsed. */
#if ALWAN_BACKEND == ALWAN_BACKEND_C || ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
# include <stddef.h>
# include <stdint.h>
#else
# include "alwan_types.h"
#endif

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
# if ALWAN_BACKEND == ALWAN_BACKEND_C || ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
#  include <string.h>
#  define ALWAN_MEMCPY(dst, src, sz) memcpy((dst), (src), (sz))
# endif
/* No definition on a shading language: memcpy takes addresses, and there are
 * none. The one core that punned bits this way (half) uses the asuint/asfloat
 * intrinsics on its GPU branch instead. */
#endif

/* ----------------------------------------------------------------
 * Allocation hooks (overrideable at compile time)
 * ---------------------------------------------------------------- */

/* CPU backends only: these take addresses, and a shading language has none.
 * A core reaches this header for ALWAN_READ_DATA_NO_BOUND_CHECK below, which
 * is a plain #define, so the GPU branch needs nothing else from this section. */
#if ALWAN_BACKEND == ALWAN_BACKEND_C || ALWAN_BACKEND == ALWAN_BACKEND_HALIDE

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

#endif /* CPU backends */

/* Table addressing bounds check.
 *
 * Every embedded table is read through the addressing gate in
 * core/alwan_table_core, which clamps the coordinate into [0, size-1] and
 * resolves NaN to the low edge. That guard is what stops a non-finite
 * coordinate from becoming (int)NaN == INT_MIN and indexing far outside the
 * array. It costs two compares per coordinate.
 *
 * Define this to 1 to compile the clamp out. Only do that when every
 * coordinate reaching a reader is already known finite and in range: without
 * it a NaN coordinate is an out-of-bounds read, not a wrong colour. The
 * library never defines it for its own builds.
 */
#ifndef ALWAN_READ_DATA_NO_BOUND_CHECK
# define ALWAN_READ_DATA_NO_BOUND_CHECK 0
#endif

#endif /* ALWAN_CONFIG_H */
