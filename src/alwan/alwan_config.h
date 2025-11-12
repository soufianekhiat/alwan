/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef ALWAN_CONFIG_H
#define ALWAN_CONFIG_H

#include <stddef.h>
#include <stdint.h>

/* ----------------------------------------------------------------
 * alwan_scalar type selection
 * ---------------------------------------------------------------- */
#ifndef ALWAN_SCALAR_IS_FLOAT
# define ALWAN_SCALAR_IS_FLOAT 0  /* default: double for parity with Colour */
#endif

#if ALWAN_SCALAR_IS_FLOAT
  typedef float  alwan_scalar;
# define ALWAN_EPSILON 1e-6f
#else
  typedef double alwan_scalar;
# define ALWAN_EPSILON 1e-12
#endif

/* alwan_scalar literal suffix helper (auto-adds 'f' for float, nothing for double) */
#if ALWAN_SCALAR_IS_FLOAT
# define ALWAN_LITERAL(x) x##f
#else
# define ALWAN_LITERAL(x) x
#endif

/* ----------------------------------------------------------------
 * Data embedding mode
 * ---------------------------------------------------------------- */
#ifndef ALWAN_EMBED_DATA
# define ALWAN_EMBED_DATA 1  /* 1=embed arrays, 0=runtime load from /data/ */
#endif

/* ----------------------------------------------------------------
 * Allocation hooks (overrideable at compile time)
 * ---------------------------------------------------------------- */

/* Forward declarations for default allocators */
void *alwan_default_alloc(size_t size, size_t align);
void  alwan_default_free(void *ptr);

#ifndef ALWAN_ALLOC
# define ALWAN_ALLOC(sz, align) alwan_default_alloc((sz), (align))
#endif

#ifndef ALWAN_FREE
# define ALWAN_FREE(p) alwan_default_free((p))
#endif

/* ----------------------------------------------------------------
 * Diagnostic pragma helpers for CSV embedding
 * ---------------------------------------------------------------- */
#if defined(_MSC_VER)
# define ALWAN_DIAG_PUSH __pragma(warning(push))
# define ALWAN_DIAG_POP  __pragma(warning(pop))
# define ALWAN_DIAG_DISABLE_FLOAT_CONV __pragma(warning(disable: 4244 4305))
#elif defined(__clang__)
# define ALWAN_DIAG_PUSH _Pragma("clang diagnostic push")
# define ALWAN_DIAG_POP  _Pragma("clang diagnostic pop")
# define ALWAN_DIAG_DISABLE_FLOAT_CONV \
    _Pragma("clang diagnostic ignored \"-Wimplicit-float-conversion\"") \
    _Pragma("clang diagnostic ignored \"-Wdouble-promotion\"")
#elif defined(__GNUC__)
# define ALWAN_DIAG_PUSH _Pragma("GCC diagnostic push")
# define ALWAN_DIAG_POP  _Pragma("GCC diagnostic pop")
# define ALWAN_DIAG_DISABLE_FLOAT_CONV \
    _Pragma("GCC diagnostic ignored \"-Wfloat-conversion\"") \
    _Pragma("GCC diagnostic ignored \"-Wdouble-promotion\"")
#else
# define ALWAN_DIAG_PUSH
# define ALWAN_DIAG_POP
# define ALWAN_DIAG_DISABLE_FLOAT_CONV
#endif

#endif /* ALWAN_CONFIG_H */
