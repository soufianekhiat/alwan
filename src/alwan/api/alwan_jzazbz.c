/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Jzazbz & JzCzhz Color Spaces (HDR Perceptual)
 *
 * Reference: Safdar et al. (2017)
 * "Perceptually uniform color space for image signals including
 *  high dynamic range and wide gamut"
 * https://opg.optica.org/oe/fulltext.cfm?uri=oe-25-13-15131
 *
 * See alwan_jzazbz_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_jzazbz_core.h"

void alwan_xyz_to_jzazbz(alwan_jzazbz *jzazbz, alwan_xyz const *xyz) {
    *jzazbz = alwan_xyz_to_jzazbz_v(*xyz);
}

void alwan_jzazbz_to_xyz(alwan_xyz *xyz, alwan_jzazbz const *jzazbz) {
    *xyz = alwan_jzazbz_to_xyz_v(*jzazbz);
}

void alwan_jzazbz_to_jzczhz(alwan_jzczhz *jzczhz, alwan_jzazbz const *jzazbz) {
    *jzczhz = alwan_jzazbz_to_jzczhz_v(*jzazbz);
}

void alwan_jzczhz_to_jzazbz(alwan_jzazbz *jzazbz, alwan_jzczhz const *jzczhz) {
    *jzazbz = alwan_jzczhz_to_jzazbz_v(*jzczhz);
}
