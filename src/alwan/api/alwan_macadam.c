/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * MacAdam's (1942) colour discrimination ellipses for observer PGN, as
 * colour-science carries them (data/macadam/SOURCE.txt).
 */

#include "../alwan.h"
#include "../alwan_internal.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static double const g_macadam1942[] = {
#include "../data/macadam/macadam1942.csv"
};
ALWAN_DIAG_POP

#define ALWAN__MACADAM_COUNT 25

alwan_status alwan_macadam1942_ellipses(alwan_macadam_ellipse *out, size_t capacity, size_t *count) {
    size_t i;
    if (count) *count = ALWAN__MACADAM_COUNT;
    if (!out && capacity == 0) return count ? ALWAN_E_RANGE : ALWAN_E_INVALID;
    if (!out) return ALWAN_E_INVALID;
    if (capacity < ALWAN__MACADAM_COUNT) return ALWAN_E_RANGE;
    for (i = 0; i < ALWAN__MACADAM_COUNT; i++) {
        double const *r = g_macadam1942 + 8 * i;
        out[i].x = r[0];
        out[i].y = r[1];
        out[i].a_observed = r[2];
        out[i].b_observed = r[3];
        out[i].theta_observed = r[4];
        out[i].a = r[5];
        out[i].b = r[6];
        out[i].theta = r[7];
    }
    return ALWAN_OK;
}
