/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * ICtCp (ITU-R BT.2100 HDR Color Space)
 *
 * Reference: ITU-R Recommendation BT.2100-3 (02/2025)
 *
 * See alwan_ictcp_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_ictcp_core.h"

void alwan_rgb_to_ictcp(alwan_ictcp *ictcp, alwan_rgb const *rgb, int use_pq) {
    if (use_pq) {
        *ictcp = alwan_rgb_to_ictcp_pq_v(*rgb);
    } else {
        *ictcp = alwan_rgb_to_ictcp_hlg_v(*rgb);
    }
}

void alwan_ictcp_to_rgb(alwan_rgb *rgb, alwan_ictcp const *ictcp, int use_pq) {
    if (use_pq) {
        *rgb = alwan_ictcp_pq_to_rgb_v(*ictcp);
    } else {
        *rgb = alwan_ictcp_hlg_to_rgb_v(*ictcp);
    }
}

void alwan_xyz_to_ictcp(alwan_ictcp *ictcp, alwan_xyz const *xyz, int use_pq) {
    if (use_pq) {
        *ictcp = alwan_xyz_to_ictcp_pq_v(*xyz);
    } else {
        *ictcp = alwan_xyz_to_ictcp_hlg_v(*xyz);
    }
}

void alwan_ictcp_to_xyz(alwan_xyz *xyz, alwan_ictcp const *ictcp, int use_pq) {
    if (use_pq) {
        *xyz = alwan_ictcp_pq_to_xyz_v(*ictcp);
    } else {
        *xyz = alwan_ictcp_hlg_to_xyz_v(*ictcp);
    }
}
