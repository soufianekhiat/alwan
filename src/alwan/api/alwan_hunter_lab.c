/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Hunter Lab Color Space
 * Thin wrapper -- logic in alwan_hunter_lab_core.h
 *
 * Reference: Hunter (1948), ASTM D 1535
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_hunter_lab_core.h"

void alwan_xyz_to_hunter_lab(alwan_hunter_lab *hunter_lab, alwan_xyz const *xyz) {
    *hunter_lab = alwan_xyz_to_hunter_lab_v(*xyz);
}

void alwan_hunter_lab_to_xyz(alwan_xyz *xyz, alwan_hunter_lab const *hunter_lab) {
    *xyz = alwan_hunter_lab_to_xyz_v(*hunter_lab);
}

void alwan_xyz_to_hunter_lab_custom(alwan_hunter_lab *hunter_lab,
                                     alwan_xyz const *xyz,
                                     alwan_xyz const *xyz_n) {
    *hunter_lab = alwan_xyz_to_hunter_lab_custom_v(*xyz, *xyz_n);
}

void alwan_hunter_lab_to_xyz_custom(alwan_xyz *xyz,
                                     alwan_hunter_lab const *hunter_lab,
                                     alwan_xyz const *xyz_n) {
    *xyz = alwan_hunter_lab_to_xyz_custom_v(*hunter_lab, *xyz_n);
}
