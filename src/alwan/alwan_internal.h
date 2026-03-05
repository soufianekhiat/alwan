/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Internal helpers and utilities
 */

#ifndef ALWAN_INTERNAL_H
#define ALWAN_INTERNAL_H

#include "alwan.h"  /* For alwan_alloc_fn, alwan_free_fn, alwan_scalar */

/* ----------------------------------------------------------------
 * Internal context structure (shared across modules)
 * ---------------------------------------------------------------- */

struct alwan_ctx {
    /* Allocation callbacks */
    alwan_alloc_fn alloc_fn;
    alwan_free_fn  free_fn;

    /* Configuration */
    char *runtime_data_root;  /* Owned copy (if non-NULL) */
    uint32_t flags;

    /* Future: data cache, registry, etc. */
};

/* ----------------------------------------------------------------
 * Safe allocation helper (overflow protection)
 * ---------------------------------------------------------------- */

/* Check for multiplication overflow before allocation.
 * Returns 0 if overflow would occur, otherwise returns the safe size. */
static inline size_t alwan_safe_array_size(size_t count, size_t elem_size) {
    if (elem_size == 0) return 0;
    if (count > SIZE_MAX / elem_size) return 0;  /* Overflow */
    return count * elem_size;
}

/* ----------------------------------------------------------------
 * Embedded Data (extern declarations)
 * ---------------------------------------------------------------- */

#if ALWAN_EMBED_DATA

/* CAT matrices (3x3 = 9 elements) */
extern alwan_scalar const g_cat_bradford[9];
extern alwan_scalar const g_cat_cat02[9];
extern alwan_scalar const g_cat_cat16[9];
extern alwan_scalar const g_cat_sharp[9];
extern alwan_scalar const g_cat_fairchild[9];
extern alwan_scalar const g_cat_cmccat97[9];
extern alwan_scalar const g_cat_cmccat2000[9];
extern alwan_scalar const g_cat_cat02_brill_2008[9];
extern alwan_scalar const g_cat_bianco_2010[9];
extern alwan_scalar const g_cat_bianco_pc_2010[9];

/* CAM matrices (Hunt-Pointer-Estevez) */
extern alwan_scalar const g_hpe[9];
extern alwan_scalar const g_hpe_inv[9];

/* ICtCp matrices */
extern alwan_scalar const g_ictcp_rgb_to_lms[9];
extern alwan_scalar const g_ictcp_lms_to_rgb[9];
extern alwan_scalar const g_ictcp_lms_p_to_ictcp_pq[9];
extern alwan_scalar const g_ictcp_ictcp_to_lms_p_pq[9];
extern alwan_scalar const g_ictcp_lms_p_to_ictcp_hlg[9];
extern alwan_scalar const g_ictcp_ictcp_to_lms_p_hlg[9];
extern alwan_scalar const g_ictcp_xyz_to_bt2020[9];
extern alwan_scalar const g_ictcp_bt2020_to_xyz[9];

/* IPT matrices and constants */
extern alwan_scalar const g_ipt_exponent;
extern alwan_scalar const g_ipt_xyz_to_lms[9];
extern alwan_scalar const g_ipt_lms_to_xyz[9];
extern alwan_scalar const g_ipt_lms_p_to_ipt[9];
extern alwan_scalar const g_ipt_ipt_to_lms_p[9];

#endif /* ALWAN_EMBED_DATA */

/* ----------------------------------------------------------------
 * YCbCr coefficient resolution (shared between api and map)
 * ---------------------------------------------------------------- */

static inline void alwan__get_ycbcr_coeffs(alwan_ycbcr_standard standard,
                                            alwan_scalar *kr, alwan_scalar *kb) {
    switch (standard) {
        case ALWAN_YCBCR_BT601:
            *kr = ALWAN_LUMA_KR_BT601;
            *kb = ALWAN_LUMA_KB_BT601;
            break;
        case ALWAN_YCBCR_BT709:
            *kr = ALWAN_LUMA_KR_BT709;
            *kb = ALWAN_LUMA_KB_BT709;
            break;
        case ALWAN_YCBCR_BT2020:
            *kr = ALWAN_LUMA_KR_BT2020;
            *kb = ALWAN_LUMA_KB_BT2020;
            break;
        default:
            *kr = ALWAN_LUMA_KR_BT709;
            *kb = ALWAN_LUMA_KB_BT709;
            break;
    }
}

#endif /* ALWAN_INTERNAL_H */
