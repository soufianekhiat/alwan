/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Fluorescent reradiation sampling (parameterized).
 * No include guard - included once per precision pass via setup/teardown.
 *
 * A Donaldson matrix on a grid of n samples, step h, sampled per row: the grid sample a
 * wavelength falls on, then the table readers of alwan_fluorescence_reader.inc.
 */

/* The grid sample whose bin holds lambda, or -1 outside the grid's bins (the first bin
 * starts half a step below lambda_min, the last ends half a step above the last sample). */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_fluo_bin)(ALWAN_CORE_T lambda, ALWAN_CORE_T lambda_min, ALWAN_CORE_T step, int n) {
    const ALWAN_CORE_T s = (lambda - lambda_min) / step + ALWAN_CORE_LITERAL(0.5);
    int k;
    if (!(s >= ALWAN_CORE_LITERAL(0.0) && s <= (ALWAN_CORE_T)n)) return -1;
    k = (int)ALWAN_CORE_FLOOR(s);
    if (k > n - 1) k = n - 1;
    return k;
}

/* ----------------------------------------------------------------
 * The table readers, bound to pointers wherever pointers exist. A shading language binds
 * them itself: see the top of alwan_fluorescence_reader.inc.
 * ---------------------------------------------------------------- */
#if ALWAN_HAS_POINTERS
#define ALWAN_FLUO_READER_POINTER 1
#include "alwan_fluorescence_reader.inc"
#endif
