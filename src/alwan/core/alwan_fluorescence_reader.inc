/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Fluorescent reradiation sampling, written against an accessor instead of a pointer.
 * No include guard - included once per binding.
 */

/* ================================================================
 * THE TABLE
 *
 * One row per grid sample and direction, count + 3 values from base:
 *   base + 0      T, the row's (column's) sum of the Donaldson matrix
 *   base + 1      the probability of reradiation, the off-diagonal part of T over T
 *   base + 2 ...  the count + 1 values of the CDF over the off-diagonal bins (the row's
 *                 own bin has weight 0), rising from 0 to 1
 * alwan_fluorescent_get_layout_f32 hands the OUTGOING rows and then the INCOMING ones.
 *
 * ACCESSOR. Tables are read through ALWAN_FLUO_READ(i). POINTER BINDING:
 * alwan_fluorescence_core.inc defines ALWAN_FLUO_READER_POINTER and includes this file
 * where pointers exist; the table is then the leading parameter. NAMED BINDING: a shader
 * defines
 *
 *     StructuredBuffer<float> Fluo : register(t4);
 *     #define ALWAN_FLUO_NAME     ink
 *     #define ALWAN_FLUO_READ(i)  Fluo[i]
 *     #include "core/alwan_fluorescence_reader.inc"
 *
 *     alwan_vec2 s = alwan_fluo_sample_lambda_ink(base, n, lambda_min, step, u);
 * ================================================================ */

#if defined(ALWAN_FLUO_READER_POINTER)

# define ALWAN_FLUO_FN_(base)      ALWAN_CORE_FNVLIT(base)
# define ALWAN_FLUO_PARAMS_        const ALWAN_CORE_T *tab,
# define ALWAN_FLUO_ARGS_          tab,
# define ALWAN_FLUO_READ(i)        tab[i]

#else

# if !defined(ALWAN_FLUO_NAME) || !defined(ALWAN_FLUO_READ)
#  error "alwan_fluorescence_reader.inc: define ALWAN_FLUO_NAME and ALWAN_FLUO_READ before including it"
# endif
# define ALWAN_FLUO_PASTE2_(a, b)  a##_##b
# define ALWAN_FLUO_PASTE_(a, b)   ALWAN_FLUO_PASTE2_(a, b)
# define ALWAN_FLUO_FN_(base)      ALWAN_FLUO_PASTE_(base, ALWAN_FLUO_NAME)
# define ALWAN_FLUO_PARAMS_
# define ALWAN_FLUO_ARGS_

#endif

/* T, the row's sum: the weight every sample from this row carries. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_FLUO_FN_(alwan_fluo_total)(ALWAN_FLUO_PARAMS_ int base) {
    return ALWAN_FLUO_READ(base);
}

/* The probability that the event is reradiation rather than plain reflection. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_FLUO_FN_(alwan_fluo_reradiate)(ALWAN_FLUO_PARAMS_ int base) {
    return ALWAN_FLUO_READ(base + 1);
}

/* The bin of the CDF at base + 2 (n intervals) whose interval holds u, by bisection. An
 * entry at 1 counts as above u even for u = 1, so u = 1 lands in the last bin that
 * reaches 1, never in an empty one after it. */
ALWAN_INLINE int ALWAN_FLUO_FN_(alwan_fluo_search)(ALWAN_FLUO_PARAMS_ int base, int n, ALWAN_CORE_T u) {
    int lo = 1, hi = n;
    while (lo < hi) {
        const int mid = lo + (hi - lo) / 2;
        const ALWAN_CORE_T e = ALWAN_FLUO_READ(base + 2 + mid);
        if (e > u || e >= ALWAN_CORE_LITERAL(1.0)) hi = mid;
        else lo = mid + 1;
    }
    return lo - 1;
}

/* A reradiated wavelength from u in [0, 1], uniform inside the bin it picks, and its
 * density per nm given reradiation: (CDF[k + 1] - CDF[k]) / step. Bin k spans
 * lambda_min + (k - 1/2) step to lambda_min + (k + 1/2) step. */
ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_FLUO_FN_(alwan_fluo_sample_lambda)(ALWAN_FLUO_PARAMS_ int base, int n,
                                                                      ALWAN_CORE_T lambda_min, ALWAN_CORE_T step,
                                                                      ALWAN_CORE_T u) {
    ALWAN_CORE_VEC2 r;
    const int k = ALWAN_FLUO_FN_(alwan_fluo_search)(ALWAN_FLUO_ARGS_ base, n, u);
    const ALWAN_CORE_T a = ALWAN_FLUO_READ(base + 2 + k);
    const ALWAN_CORE_T span = ALWAN_FLUO_READ(base + 3 + k) - a;
    ALWAN_DET_PRECISE ALWAN_CORE_T f = span > ALWAN_CORE_LITERAL(0.0) ? (u - a) / span : ALWAN_CORE_LITERAL(0.5);
    if (f < ALWAN_CORE_LITERAL(0.0)) f = ALWAN_CORE_LITERAL(0.0);
    if (f > ALWAN_CORE_LITERAL(1.0)) f = ALWAN_CORE_LITERAL(1.0);
    r.v[0] = lambda_min + ((ALWAN_CORE_T)k + f - ALWAN_CORE_LITERAL(0.5)) * step;
    r.v[1] = span / step;
    return r;
}

/* The density per nm, given reradiation, of bin k of the CDF at base. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_FLUO_FN_(alwan_fluo_bin_pdf)(ALWAN_FLUO_PARAMS_ int base, int k, ALWAN_CORE_T step) {
    return (ALWAN_FLUO_READ(base + 3 + k) - ALWAN_FLUO_READ(base + 2 + k)) / step;
}

#undef ALWAN_FLUO_FN_
#undef ALWAN_FLUO_PARAMS_
#undef ALWAN_FLUO_ARGS_
#undef ALWAN_FLUO_READ
#if !defined(ALWAN_FLUO_READER_POINTER)
# undef ALWAN_FLUO_PASTE2_
# undef ALWAN_FLUO_PASTE_
# undef ALWAN_FLUO_NAME
#endif
#undef ALWAN_FLUO_READER_POINTER
