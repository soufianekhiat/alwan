/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * DIN99 Family (DIN99, DIN99b, DIN99c, DIN99d)
 * See alwan_din99_core.h
 *
 * Reference: DIN 6176:2001-03, ASTM D2244-07
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_din99_core.h"

void alwan_lab_to_din99(alwan_din99 *din99, alwan_lab const *lab, int variant) {
    if (variant < 0 || variant > 3) return;
    alwan_lab tmp = *lab;
    ALWAN_DENORM_LAB(&tmp);
    *din99 = alwan_lab_to_din99_v(tmp, variant);
    ALWAN_NORM_DIN99(din99);
}

void alwan_din99_to_lab(alwan_lab *lab, alwan_din99 const *din99, int variant) {
    if (variant < 0 || variant > 3) return;
    alwan_din99 tmp = *din99;
    ALWAN_DENORM_DIN99(&tmp);
    *lab = alwan_din99_to_lab_v(tmp, variant);
    ALWAN_NORM_LAB(lab);
}
