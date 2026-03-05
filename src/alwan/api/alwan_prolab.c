/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ProLab Color Space (Perceptually Uniform Projective)
 * See alwan_prolab_core.h
 *
 * Reference: Konovalenko et al. (2021)
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_prolab_core.h"

void alwan_xyz_to_prolab(alwan_prolab *prolab, alwan_xyz const *xyz) {
    *prolab = alwan_xyz_to_prolab_v(*xyz);
}

void alwan_prolab_to_xyz(alwan_xyz *xyz, alwan_prolab const *prolab) {
    *xyz = alwan_prolab_to_xyz_v(*prolab);
}

void alwan_xyz_to_prolab_custom(alwan_prolab *prolab,
                                 alwan_xyz const *xyz,
                                 alwan_xyz const *xyz_n) {
    *prolab = alwan_xyz_to_prolab_custom_v(*xyz, *xyz_n);
}

void alwan_prolab_to_xyz_custom(alwan_xyz *xyz,
                                 alwan_prolab const *prolab,
                                 alwan_xyz const *xyz_n) {
    *xyz = alwan_prolab_to_xyz_custom_v(*prolab, *xyz_n);
}
