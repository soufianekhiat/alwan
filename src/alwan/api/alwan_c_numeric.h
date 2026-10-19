/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2026 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * LC_NUMERIC "C" for the calling thread only, around the file readers and writers that print
 * and parse with the C library. setlocale alone is process-wide: two imports on two threads,
 * or one beside host code, left each other in the wrong locale.
 *
 *   void *tok = alwan__c_numeric_begin();   ... strtod / printf ...   alwan__c_numeric_end(tok);
 *
 * NULL from begin means the switch could not be made; end(NULL) does nothing. A POSIX
 * translation unit includes this before anything else (it needs uselocale under -std=c11).
 */

#ifndef ALWAN_C_NUMERIC_H
#define ALWAN_C_NUMERIC_H

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#if defined(__APPLE__)
#include <xlocale.h>
#endif

#if defined(_WIN32)

static char alwan__c_numeric_global_mode; /* token: the thread was on the global locale */

/* The common case, a thread on the global locale, allocates nothing: it goes per-thread,
 * sets "C" there, and on end returns to the untouched global locale. A thread already
 * per-thread keeps its numeric locale name to set back. */
static inline void *alwan__c_numeric_begin(void) {
    int const prev = _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
    if (prev == -1) return NULL;
    if (prev == _ENABLE_PER_THREAD_LOCALE) {
        char const *cur = setlocale(LC_NUMERIC, NULL);
        char *saved;
        size_t n;
        if (!cur) return NULL;
        n = strlen(cur);
        saved = (char *)malloc(n + 1);
        if (!saved) return NULL;
        memcpy(saved, cur, n + 1);
        setlocale(LC_NUMERIC, "C");
        return saved;
    }
    setlocale(LC_NUMERIC, "C");
    return &alwan__c_numeric_global_mode;
}

static inline void alwan__c_numeric_end(void *tok) {
    if (!tok) return;
    if (tok == &alwan__c_numeric_global_mode) {
        _configthreadlocale(_DISABLE_PER_THREAD_LOCALE);
        return;
    }
    setlocale(LC_NUMERIC, (char const *)tok);
    free(tok);
}

#else

/* The token is the thread's previous locale; the "C"-numeric copy made from it is freed on
 * end. Nothing else about the thread's locale changes. */
static inline void *alwan__c_numeric_begin(void) {
    locale_t const prev = uselocale((locale_t)0);
    locale_t base, c;
    if (prev == (locale_t)0) return NULL;
    base = duplocale(prev);
    if (base == (locale_t)0) return NULL;
    c = newlocale(LC_NUMERIC_MASK, "C", base);
    if (c == (locale_t)0) { freelocale(base); return NULL; }
    uselocale(c);
    return (void *)prev;
}

static inline void alwan__c_numeric_end(void *tok) {
    locale_t c;
    if (!tok) return;
    c = uselocale((locale_t)tok);
    if (c != (locale_t)0 && c != LC_GLOBAL_LOCALE) freelocale(c);
}

#endif

#endif /* ALWAN_C_NUMERIC_H */
