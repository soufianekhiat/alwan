/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * 2D importance sampling (parameterized). No include guard: included once per
 * precision pass. The functions that read a table live in
 * alwan_importance_sampling_reader.inc; this file binds them to pointers.
 */

/* ----------------------------------------------------------------
 * The table, in elements of the precision, for an image of nu x nv cells.
 *
 * SEARCH mode:
 *     w        0                       nv * nu       the weights, row by row
 *     cdf      nv * nu                 nv * (nu+1)   each row's CDF, 0 to 1
 *     mcdf     + nv * (nu+1)           nv + 1        the CDF over the rows' sums
 *     norm     + nv + 1                1             density per unit weight:
 *                                                    1 / (sum of weights x cell area)
 * DIRECT mode, with inverse tables of ru intervals per row and rv for the rows:
 *     rowinv   0                       nv * (ru+1)   each row's inverse CDF
 *     minv     nv * (ru+1)             rv + 1        the inverse CDF over the rows
 * ALIAS mode, n = nu * nv cells in row order:
 *     w        0                       n             the weights, row by row
 *     aprob    n                       n             each slot's threshold, 0 to 1
 *     aidx     2n                      n             each slot's alias, a cell index
 *                                                    held as a value (exact below 2^24
 *                                                    in single precision; prepare checks)
 *     anorm    3n                      1             density per unit weight, as norm
 *
 * Every CDF and inverse table rises from 0 (or its first node) to 1 (or its last)
 * and never falls. Offsets are ints so a shader can take them; the table holds
 * fewer than 2^31 elements (prepare checks).
 * ---------------------------------------------------------------- */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_is2d_off_cdf)(int nu, int nv) { return nv * nu; }
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_is2d_off_mcdf)(int nu, int nv) { return nv * nu + nv * (nu + 1); }
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_is2d_off_norm)(int nu, int nv) {
    return ALWAN_CORE_FNVLIT(alwan_is2d_off_mcdf)(nu, nv) + nv + 1;
}
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_is2d_off_minv)(int nv, int ru) { return nv * (ru + 1); }
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_is2d_off_aprob)(int n) { return n; }
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_is2d_off_aidx)(int n) { return 2 * n; }
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_is2d_off_anorm)(int n) { return 3 * n; }

/* The cell of n that holds v, v in cell units: the floor, clamped to [0, n - 1],
 * NaN to 0. Every float that becomes an index goes through here. */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_is2d_cell)(ALWAN_CORE_T v, int n) {
    if (!(v > ALWAN_CORE_LITERAL(0.0))) return 0;
    if (v >= (ALWAN_CORE_T)n) return n - 1;
    return (int)v;
}

/* ----------------------------------------------------------------
 * The table readers, bound to pointers wherever pointers exist. A shading
 * language binds them itself: see the top of
 * alwan_importance_sampling_reader.inc.
 * ---------------------------------------------------------------- */
#if ALWAN_HAS_POINTERS
#define ALWAN_IS2D_READER_POINTER 1
#include "alwan_importance_sampling_reader.inc"
#endif
