/* No include guard -- included twice (f32 and f64 passes) */

void ALWAN_CORE_FNLIT(alwan_rgb_to_ictcp)(ALWAN_CORE_ICTCP *ictcp, ALWAN_CORE_RGB const *rgb, int use_pq) {
    if (use_pq) {
        *ictcp = ALWAN_CORE_FNVLIT(alwan_rgb_to_ictcp_pq)(*rgb);
    } else {
        *ictcp = ALWAN_CORE_FNVLIT(alwan_rgb_to_ictcp_hlg)(*rgb);
    }
}

void ALWAN_CORE_FNLIT(alwan_ictcp_to_rgb)(ALWAN_CORE_RGB *rgb, ALWAN_CORE_ICTCP const *ictcp, int use_pq) {
    if (use_pq) {
        *rgb = ALWAN_CORE_FNVLIT(alwan_ictcp_pq_to_rgb)(*ictcp);
    } else {
        *rgb = ALWAN_CORE_FNVLIT(alwan_ictcp_hlg_to_rgb)(*ictcp);
    }
}

void ALWAN_CORE_FNLIT(alwan_xyz_to_ictcp)(ALWAN_CORE_ICTCP *ictcp, ALWAN_CORE_XYZ const *xyz, int use_pq) {
    if (use_pq) {
        *ictcp = ALWAN_CORE_FNVLIT(alwan_xyz_to_ictcp_pq)(*xyz);
    } else {
        *ictcp = ALWAN_CORE_FNVLIT(alwan_xyz_to_ictcp_hlg)(*xyz);
    }
}

void ALWAN_CORE_FNLIT(alwan_ictcp_to_xyz)(ALWAN_CORE_XYZ *xyz, ALWAN_CORE_ICTCP const *ictcp, int use_pq) {
    if (use_pq) {
        *xyz = ALWAN_CORE_FNVLIT(alwan_ictcp_pq_to_xyz)(*ictcp);
    } else {
        *xyz = ALWAN_CORE_FNVLIT(alwan_ictcp_hlg_to_xyz)(*ictcp);
    }
}
