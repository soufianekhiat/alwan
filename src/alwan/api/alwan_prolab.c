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
    ALWAN_NORM_PROLAB(prolab);
}

void alwan_prolab_to_xyz(alwan_xyz *xyz, alwan_prolab const *prolab) {
    alwan_prolab tmp = *prolab;
    ALWAN_DENORM_PROLAB(&tmp);
    *xyz = alwan_prolab_to_xyz_v(tmp);
}

void alwan_xyz_to_prolab_custom(alwan_prolab *prolab,
                                 alwan_xyz const *xyz,
                                 alwan_xyz const *xyz_n) {
    *prolab = alwan_xyz_to_prolab_custom_v(*xyz, *xyz_n);
    ALWAN_NORM_PROLAB(prolab);
}

void alwan_prolab_to_xyz_custom(alwan_xyz *xyz,
                                 alwan_prolab const *prolab,
                                 alwan_xyz const *xyz_n) {
    alwan_prolab tmp = *prolab;
    ALWAN_DENORM_PROLAB(&tmp);
    *xyz = alwan_prolab_to_xyz_custom_v(tmp, *xyz_n);
}
