/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * OSA-UCS Color Space (Optical Society of America Uniform Color Scales)
 *
 * Reference: OSA Uniform Color Scales Committee (1977)
 *
 * Implementation delegated to alwan_osa_ucs_core.h (single source of truth).
 */

#include "alwan.h"
#include "alwan_internal.h"
#include "alwan_osa_ucs_core.h"

void alwan_xyz_to_osa_ucs(alwan_osa_ucs *osa_ucs, alwan_xyz const *xyz) {
    *osa_ucs = alwan_xyz_to_osa_ucs_v(*xyz);
}

void alwan_osa_ucs_to_xyz(alwan_xyz *xyz, alwan_osa_ucs const *osa_ucs) {
    *xyz = alwan_osa_ucs_to_xyz_v(*osa_ucs);
}
