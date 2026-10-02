/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Low-discrepancy point sets in any number of dimensions (up to 64): Halton,
 * Halton with its digits scrambled, and Roberts' R_d. Suite 296.
 *
 * HALTON is the van der Corput radical inverse in the first d primes, computed as
 * scipy.stats.qmc.Halton(scramble=False) computes it (its _cy_van_der_corput): the
 * digits of the index read least significant first, each times 1/b^k with b^k
 * built by repeated division, so the points equal scipy's to the bit.
 *
 * HALTON_SCRAMBLED applies, per dimension and per digit position, a permutation of
 * the digits 0 .. b - 1, the leading zeros of the index included (so no point sits
 * at 0), out to the digits a double holds. This is the structure of scipy's
 * scrambled Halton (Owen 2017, "A randomized Halton algorithm in R"), but the
 * permutations come from a hash of seed, not from numpy's generator, so the points
 * are not scipy's. What the structure guarantees holds for both: the first b^k
 * points of a base-b dimension still put one point in each interval of width b^-k.
 *
 * R_D is Roberts' (2018) additive recurrence x_n = frac(1/2 + n alpha), alpha_j =
 * phi_d^-j with phi_d the positive root of x^(d+1) = x + 1: any count, even
 * coverage at every length, one multiply a coordinate.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>

#define ALWAN__LD_MAX_DIMS 64

static int const alwan__ld_primes[ALWAN__LD_MAX_DIMS] = {
    2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,  47,  53,
    59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107, 109, 113, 127, 131,
    137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223,
    227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311
};

/* scipy's _cy_van_der_corput, one index. */
static double alwan__ld_vdc(unsigned long long index, int base) {
    double sum = 0.0, b2r = 1.0 / (double)base;
    unsigned long long q = index;
    while (q > 0) {
        unsigned long long const rem = q % (unsigned long long)base;
        sum += (double)rem * b2r;
        b2r /= (double)base;
        q /= (unsigned long long)base;
    }
    return sum;
}

static unsigned alwan__ld_hash(unsigned x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

/* The digits a double resolves in base b: ceil(53 / log2 b), at most 53. */
static int alwan__ld_digits(int base) {
    int n = 0;
    double scale = 1.0;
    while (scale < 9007199254740992.0 && n < 53) {   /* 2^53 */
        scale *= (double)base;
        n++;
    }
    return n;
}

/* Digit permutation for one dimension and one digit position, Fisher-Yates driven by a
 * hash of (seed, dimension, position). perm holds base entries. */
static void alwan__ld_perm(int *perm, int base, unsigned seed, int dim, int pos) {
    int i;
    unsigned h = alwan__ld_hash(seed ^ alwan__ld_hash((unsigned)dim * 0x9e3779b9U ^ alwan__ld_hash((unsigned)pos + 0x85ebca6bU)));
    for (i = 0; i < base; i++) perm[i] = i;
    for (i = base - 1; i > 0; i--) {
        int j, t;
        h = alwan__ld_hash(h + (unsigned)i);
        j = (int)(h % (unsigned)(i + 1));
        t = perm[i];
        perm[i] = perm[j];
        perm[j] = t;
    }
}

static double alwan__ld_vdc_scrambled(unsigned long long index, int base, int const *perms, int ndig) {
    double sum = 0.0, b2r = 1.0 / (double)base;
    unsigned long long q = index;
    int k;
    for (k = 0; k < ndig; k++) {
        unsigned long long const digit = q % (unsigned long long)base;   /* an integer digit, no float */
        sum += (double)perms[(size_t)k * (size_t)base + (size_t)digit] * b2r;
        b2r /= (double)base;
        q /= (unsigned long long)base;
    }
    return sum;
}

/* phi_d, the positive root of x^(d+1) = x + 1 (2 for d = 0 is never asked). */
static double alwan__ld_phi(int d) {
    double x = 2.0;
    int it;
    for (it = 0; it < 60; it++) x = ALWAN_POW_F64(1.0 + x, 1.0 / (double)(d + 1));
    return x;
}

static alwan_status alwan__ld_run(double *out64, float *out32, size_t row_stride, size_t count, size_t dims,
                                  alwan_low_discrepancy_method method, alwan_low_discrepancy_params const *params) {
    alwan_low_discrepancy_params const zero = { 0, 0 };
    alwan_low_discrepancy_params const *p = params ? params : &zero;
    size_t const elem = out64 ? sizeof(double) : sizeof(float);
    size_t const rs = row_stride ? row_stride : dims * elem;
    size_t i, j;
    if ((!out64 && !out32) || count == 0 || dims == 0 || dims > ALWAN__LD_MAX_DIMS) return ALWAN_E_INVALID;
    if (rs < dims * elem) return ALWAN_E_INVALID;
    if (method != ALWAN_LOW_DISCREPANCY_HALTON && method != ALWAN_LOW_DISCREPANCY_HALTON_SCRAMBLED &&
        method != ALWAN_LOW_DISCREPANCY_R) return ALWAN_E_INVALID;
    if ((unsigned long long)p->start_index > 0xFFFFFFFFFFFFFFFFULL - (unsigned long long)count) return ALWAN_E_RANGE;

    if (method == ALWAN_LOW_DISCREPANCY_R) {
        double alpha[ALWAN__LD_MAX_DIMS];
        double const phi = alwan__ld_phi((int)dims);
        double a = 1.0;
        for (j = 0; j < dims; j++) {
            a /= phi;
            alpha[j] = a;
        }
        for (i = 0; i < count; i++) {
            double const n = (double)((unsigned long long)p->start_index + i);
            char *row = (char *)(out64 ? (void *)out64 : (void *)out32) + i * rs;
            for (j = 0; j < dims; j++) {
                double v = 0.5 + n * alpha[j];
                v -= ALWAN_FLOOR_F64(v);
                if (out64) ((double *)row)[j] = v;
                else ((float *)row)[j] = (float)v;
            }
        }
        return ALWAN_OK;
    }

    for (j = 0; j < dims; j++) {
        int const base = alwan__ld_primes[j];
        int *perms = NULL;
        int ndig = 0;
        if (method == ALWAN_LOW_DISCREPANCY_HALTON_SCRAMBLED) {
            int k;
            ndig = alwan__ld_digits(base);
            perms = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)ndig, (size_t)base * sizeof(int)), sizeof(int));
            if (!perms) return ALWAN_E_NOMEM;
            for (k = 0; k < ndig; k++) alwan__ld_perm(perms + (size_t)k * (size_t)base, base, p->seed, (int)j, k);
        }
        for (i = 0; i < count; i++) {
            unsigned long long const idx = (unsigned long long)p->start_index + i;
            double const v = perms ? alwan__ld_vdc_scrambled(idx, base, perms, ndig) : alwan__ld_vdc(idx, base);
            char *row = (char *)(out64 ? (void *)out64 : (void *)out32) + i * rs;
            if (out64) ((double *)row)[j] = v;
            else ((float *)row)[j] = (float)v;
        }
        ALWAN_FREE(perms);
    }
    return ALWAN_OK;
}

alwan_status alwan_low_discrepancy_points_f64(alwan_f64 *out, size_t row_stride, size_t count, size_t dimensions,
                                              alwan_low_discrepancy_method method,
                                              alwan_low_discrepancy_params const *params) {
    if (!out) return ALWAN_E_INVALID;
    return alwan__ld_run(out, NULL, row_stride, count, dimensions, method, params);
}

alwan_status alwan_low_discrepancy_points_f32(alwan_f32 *out, size_t row_stride, size_t count, size_t dimensions,
                                              alwan_low_discrepancy_method method,
                                              alwan_low_discrepancy_params const *params) {
    if (!out) return ALWAN_E_INVALID;
    return alwan__ld_run(NULL, out, row_stride, count, dimensions, method, params);
}
