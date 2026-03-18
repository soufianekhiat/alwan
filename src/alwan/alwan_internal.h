/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Internal helpers and utilities
 */

#ifndef ALWAN_INTERNAL_H
#define ALWAN_INTERNAL_H

#include "alwan.h"  /* For alwan_alloc_fn, alwan_free_fn, alwan_f64 */

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
extern alwan_f64 const g_cat_bradford[9];
extern alwan_f64 const g_cat_cat02[9];
extern alwan_f64 const g_cat_cat16[9];
extern alwan_f64 const g_cat_sharp[9];
extern alwan_f64 const g_cat_fairchild[9];
extern alwan_f64 const g_cat_cmccat97[9];
extern alwan_f64 const g_cat_cmccat2000[9];
extern alwan_f64 const g_cat_cat02_brill_2008[9];
extern alwan_f64 const g_cat_bianco_2010[9];
extern alwan_f64 const g_cat_bianco_pc_2010[9];

/* CAM matrices (Hunt-Pointer-Estevez) */
extern alwan_f64 const g_hpe[9];
extern alwan_f64 const g_hpe_inv[9];

/* ICtCp matrices */
extern alwan_f64 const g_ictcp_rgb_to_lms[9];
extern alwan_f64 const g_ictcp_lms_to_rgb[9];
extern alwan_f64 const g_ictcp_lms_p_to_ictcp_pq[9];
extern alwan_f64 const g_ictcp_ictcp_to_lms_p_pq[9];
extern alwan_f64 const g_ictcp_lms_p_to_ictcp_hlg[9];
extern alwan_f64 const g_ictcp_ictcp_to_lms_p_hlg[9];
extern alwan_f64 const g_ictcp_xyz_to_bt2020[9];
extern alwan_f64 const g_ictcp_bt2020_to_xyz[9];

/* IPT matrices and constants */
extern alwan_f64 const g_ipt_exponent;
extern alwan_f64 const g_ipt_xyz_to_lms[9];
extern alwan_f64 const g_ipt_lms_to_xyz[9];
extern alwan_f64 const g_ipt_lms_p_to_ipt[9];
extern alwan_f64 const g_ipt_ipt_to_lms_p[9];

#endif /* ALWAN_EMBED_DATA */

/* ----------------------------------------------------------------
 * YCbCr coefficient resolution (shared between api and map)
 * ---------------------------------------------------------------- */

static inline void alwan__get_ycbcr_coeffs(alwan_ycbcr_standard standard,
                                            alwan_f64 *kr, alwan_f64 *kb) {
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

/* ----------------------------------------------------------------
 * Luma coefficient resolution (alwan_luma_standard -> kr, kg, kb)
 * ---------------------------------------------------------------- */

static inline void alwan__get_luma_coeffs(int standard,
                                           alwan_f64 *kr, alwan_f64 *kg,
                                           alwan_f64 *kb) {
    switch (standard) {
        case 0: /* ALWAN_LUMA_BT601 */
            *kr = ALWAN_LUMA_KR_BT601;
            *kg = ALWAN_LUMA_KG_BT601;
            *kb = ALWAN_LUMA_KB_BT601;
            break;
        case 1: /* ALWAN_LUMA_BT709 */
            *kr = ALWAN_LUMA_KR_BT709;
            *kg = ALWAN_LUMA_KG_BT709;
            *kb = ALWAN_LUMA_KB_BT709;
            break;
        case 2: /* ALWAN_LUMA_BT2020 */
            *kr = ALWAN_LUMA_KR_BT2020;
            *kg = ALWAN_LUMA_KG_BT2020;
            *kb = ALWAN_LUMA_KB_BT2020;
            break;
        case 3: /* ALWAN_LUMA_ACES_AP1 */
            *kr = ALWAN_LUMA_KR_AP1;
            *kg = ALWAN_LUMA_KG_AP1;
            *kb = ALWAN_LUMA_KB_AP1;
            break;
        case 4: /* ALWAN_LUMA_ACES_AP0 */
            *kr = ALWAN_LUMA_KR_AP0;
            *kg = ALWAN_LUMA_KG_AP0;
            *kb = ALWAN_LUMA_KB_AP0;
            break;
        case 5: /* ALWAN_LUMA_DISPLAY_P3 */
            *kr = ALWAN_LUMA_KR_P3;
            *kg = ALWAN_LUMA_KG_P3;
            *kb = ALWAN_LUMA_KB_P3;
            break;
        case 6: /* ALWAN_LUMA_DCI_P3 */
            *kr = ALWAN_LUMA_KR_DCIP3;
            *kg = ALWAN_LUMA_KG_DCIP3;
            *kb = ALWAN_LUMA_KB_DCIP3;
            break;
        case 7: /* ALWAN_LUMA_ADOBE_RGB */
            *kr = ALWAN_LUMA_KR_ADOBE;
            *kg = ALWAN_LUMA_KG_ADOBE;
            *kb = ALWAN_LUMA_KB_ADOBE;
            break;
        case 8: /* ALWAN_LUMA_PROPHOTO_RGB */
            *kr = ALWAN_LUMA_KR_PROPHOTO;
            *kg = ALWAN_LUMA_KG_PROPHOTO;
            *kb = ALWAN_LUMA_KB_PROPHOTO;
            break;
        default:
            *kr = ALWAN_LUMA_KR_BT709;
            *kg = ALWAN_LUMA_KG_BT709;
            *kb = ALWAN_LUMA_KB_BT709;
            break;
    }
}

/* ----------------------------------------------------------------
 * Transfer function resolution (enum -> function pointer)
 *
 * Shared resolver for all modules that need to dispatch on
 * alwan_transfer_function.  Avoids duplicating the 40-case switch
 * in alwan_rgb.c, alwan_rgb_map.c, alwan_video.c, alwan_clf.c, etc.
 * ---------------------------------------------------------------- */

#include "core/alwan_rgb_core.h"

typedef float  (*alwan_tf_fn_f32)(float);
typedef double (*alwan_tf_fn_f64)(double);

static inline alwan_tf_fn_f32 alwan__resolve_oetf_f32(alwan_transfer_function tf) {
    switch (tf) {
    case ALWAN_TF_LINEAR:     return alwan_linear_identity_f32;
    case ALWAN_TF_SRGB:       return alwan_srgb_oetf_f32;
    case ALWAN_TF_BT709:      return alwan_bt2020_oetf_f32;
    case ALWAN_TF_BT2020:     return alwan_bt2020_oetf_f32;
    case ALWAN_TF_PQ:         return alwan_pq_oetf_f32;
    case ALWAN_TF_ST2084:     return alwan_pq_oetf_f32;
    case ALWAN_TF_HLG:        return alwan_hlg_oetf_f32;
    case ALWAN_TF_BT1886:     return alwan_gamma24_oetf_f32;
    case ALWAN_TF_ACESPROXY:  return alwan_acesproxy_oetf_f32;
    case ALWAN_TF_ACESCC:     return alwan_acescc_oetf_f32;
    case ALWAN_TF_ACESCCT:    return alwan_acescct_oetf_f32;
    case ALWAN_TF_SLOG:       return alwan_slog_oetf_f32;
    case ALWAN_TF_SLOG2:      return alwan_slog2_oetf_f32;
    case ALWAN_TF_SLOG3:      return alwan_slog3_oetf_f32;
    case ALWAN_TF_CLOG:       return alwan_clog_oetf_f32;
    case ALWAN_TF_CLOG2:      return alwan_clog2_oetf_f32;
    case ALWAN_TF_CLOG3:      return alwan_clog3_oetf_f32;
    case ALWAN_TF_VLOG:       return alwan_vlog_oetf_f32;
    case ALWAN_TF_LOGC3:      return alwan_logc3_oetf_f32;
    case ALWAN_TF_LOGC4:      return alwan_logc4_oetf_f32;
    case ALWAN_TF_REDLOG:     return alwan_redlog_oetf_f32;
    case ALWAN_TF_REDLOGFILM: return alwan_redlogfilm_oetf_f32;
    case ALWAN_TF_LOG3G10:    return alwan_log3g10_oetf_f32;
    case ALWAN_TF_BMDFILM:    return alwan_bmdfilm_oetf_f32;
    case ALWAN_TF_BMDFILM4:   return alwan_bmdfilm4_oetf_f32;
    case ALWAN_TF_TLOG:       return alwan_tlog_oetf_f32;
    case ALWAN_TF_ELOG:       return alwan_elog_oetf_f32;
    case ALWAN_TF_PROTUNE:    return alwan_protune_oetf_f32;
    case ALWAN_TF_GAMMA22:    return alwan_gamma22_oetf_f32;
    case ALWAN_TF_GAMMA24:    return alwan_gamma24_oetf_f32;
    case ALWAN_TF_GAMMA26:    return alwan_gamma26_oetf_f32;
    case ALWAN_TF_GAMMA28:    return alwan_gamma28_oetf_f32;
    case ALWAN_TF_NLOG:       return alwan_nlog_oetf_f32;
    case ALWAN_TF_CINEON:     return alwan_cineon_oetf_f32;
    case ALWAN_TF_APPLE_LOG:  return alwan_apple_log_oetf_f32;
    case ALWAN_TF_FLOG:       return alwan_flog_oetf_f32;
    case ALWAN_TF_FLOG2:      return alwan_flog2_oetf_f32;
    case ALWAN_TF_LLOG:       return alwan_llog_oetf_f32;
    case ALWAN_TF_DLOG:       return alwan_dlog_oetf_f32;
    case ALWAN_TF_DCDM:       return alwan_dcdm_oetf_f32;
    case ALWAN_TF_ADX10:      return alwan_adx10_oetf_f32;
    case ALWAN_TF_ADX16:      return alwan_adx16_oetf_f32;
    }
    return NULL;
}

static inline alwan_tf_fn_f64 alwan__resolve_oetf_f64(alwan_transfer_function tf) {
    switch (tf) {
    case ALWAN_TF_LINEAR:     return alwan_linear_identity_f64;
    case ALWAN_TF_SRGB:       return alwan_srgb_oetf_f64;
    case ALWAN_TF_BT709:      return alwan_bt2020_oetf_f64;
    case ALWAN_TF_BT2020:     return alwan_bt2020_oetf_f64;
    case ALWAN_TF_PQ:         return alwan_pq_oetf_f64;
    case ALWAN_TF_ST2084:     return alwan_pq_oetf_f64;
    case ALWAN_TF_HLG:        return alwan_hlg_oetf_f64;
    case ALWAN_TF_BT1886:     return alwan_gamma24_oetf_f64;
    case ALWAN_TF_ACESPROXY:  return alwan_acesproxy_oetf_f64;
    case ALWAN_TF_ACESCC:     return alwan_acescc_oetf_f64;
    case ALWAN_TF_ACESCCT:    return alwan_acescct_oetf_f64;
    case ALWAN_TF_SLOG:       return alwan_slog_oetf_f64;
    case ALWAN_TF_SLOG2:      return alwan_slog2_oetf_f64;
    case ALWAN_TF_SLOG3:      return alwan_slog3_oetf_f64;
    case ALWAN_TF_CLOG:       return alwan_clog_oetf_f64;
    case ALWAN_TF_CLOG2:      return alwan_clog2_oetf_f64;
    case ALWAN_TF_CLOG3:      return alwan_clog3_oetf_f64;
    case ALWAN_TF_VLOG:       return alwan_vlog_oetf_f64;
    case ALWAN_TF_LOGC3:      return alwan_logc3_oetf_f64;
    case ALWAN_TF_LOGC4:      return alwan_logc4_oetf_f64;
    case ALWAN_TF_REDLOG:     return alwan_redlog_oetf_f64;
    case ALWAN_TF_REDLOGFILM: return alwan_redlogfilm_oetf_f64;
    case ALWAN_TF_LOG3G10:    return alwan_log3g10_oetf_f64;
    case ALWAN_TF_BMDFILM:    return alwan_bmdfilm_oetf_f64;
    case ALWAN_TF_BMDFILM4:   return alwan_bmdfilm4_oetf_f64;
    case ALWAN_TF_TLOG:       return alwan_tlog_oetf_f64;
    case ALWAN_TF_ELOG:       return alwan_elog_oetf_f64;
    case ALWAN_TF_PROTUNE:    return alwan_protune_oetf_f64;
    case ALWAN_TF_GAMMA22:    return alwan_gamma22_oetf_f64;
    case ALWAN_TF_GAMMA24:    return alwan_gamma24_oetf_f64;
    case ALWAN_TF_GAMMA26:    return alwan_gamma26_oetf_f64;
    case ALWAN_TF_GAMMA28:    return alwan_gamma28_oetf_f64;
    case ALWAN_TF_NLOG:       return alwan_nlog_oetf_f64;
    case ALWAN_TF_CINEON:     return alwan_cineon_oetf_f64;
    case ALWAN_TF_APPLE_LOG:  return alwan_apple_log_oetf_f64;
    case ALWAN_TF_FLOG:       return alwan_flog_oetf_f64;
    case ALWAN_TF_FLOG2:      return alwan_flog2_oetf_f64;
    case ALWAN_TF_LLOG:       return alwan_llog_oetf_f64;
    case ALWAN_TF_DLOG:       return alwan_dlog_oetf_f64;
    case ALWAN_TF_DCDM:       return alwan_dcdm_oetf_f64;
    case ALWAN_TF_ADX10:      return alwan_adx10_oetf_f64;
    case ALWAN_TF_ADX16:      return alwan_adx16_oetf_f64;
    }
    return NULL;
}

static inline alwan_tf_fn_f32 alwan__resolve_eotf_f32(alwan_transfer_function tf) {
    switch (tf) {
    case ALWAN_TF_LINEAR:     return alwan_linear_identity_f32;
    case ALWAN_TF_SRGB:       return alwan_srgb_eotf_f32;
    case ALWAN_TF_BT709:      return alwan_bt2020_eotf_f32;
    case ALWAN_TF_BT2020:     return alwan_bt2020_eotf_f32;
    case ALWAN_TF_PQ:         return alwan_pq_eotf_f32;
    case ALWAN_TF_ST2084:     return alwan_pq_eotf_f32;
    case ALWAN_TF_HLG:        return alwan_hlg_eotf_f32;
    case ALWAN_TF_BT1886:     return alwan_bt1886_eotf_f32;
    case ALWAN_TF_ACESPROXY:  return alwan_acesproxy_eotf_f32;
    case ALWAN_TF_ACESCC:     return alwan_acescc_eotf_f32;
    case ALWAN_TF_ACESCCT:    return alwan_acescct_eotf_f32;
    case ALWAN_TF_SLOG:       return alwan_slog_eotf_f32;
    case ALWAN_TF_SLOG2:      return alwan_slog2_eotf_f32;
    case ALWAN_TF_SLOG3:      return alwan_slog3_eotf_f32;
    case ALWAN_TF_CLOG:       return alwan_clog_eotf_f32;
    case ALWAN_TF_CLOG2:      return alwan_clog2_eotf_f32;
    case ALWAN_TF_CLOG3:      return alwan_clog3_eotf_f32;
    case ALWAN_TF_VLOG:       return alwan_vlog_eotf_f32;
    case ALWAN_TF_LOGC3:      return alwan_logc3_eotf_f32;
    case ALWAN_TF_LOGC4:      return alwan_logc4_eotf_f32;
    case ALWAN_TF_REDLOG:     return alwan_redlog_eotf_f32;
    case ALWAN_TF_REDLOGFILM: return alwan_redlogfilm_eotf_f32;
    case ALWAN_TF_LOG3G10:    return alwan_log3g10_eotf_f32;
    case ALWAN_TF_BMDFILM:    return alwan_bmdfilm_eotf_f32;
    case ALWAN_TF_BMDFILM4:   return alwan_bmdfilm4_eotf_f32;
    case ALWAN_TF_TLOG:       return alwan_tlog_eotf_f32;
    case ALWAN_TF_ELOG:       return alwan_elog_eotf_f32;
    case ALWAN_TF_PROTUNE:    return alwan_protune_eotf_f32;
    case ALWAN_TF_GAMMA22:    return alwan_gamma22_eotf_f32;
    case ALWAN_TF_GAMMA24:    return alwan_gamma24_eotf_f32;
    case ALWAN_TF_GAMMA26:    return alwan_gamma26_eotf_f32;
    case ALWAN_TF_GAMMA28:    return alwan_gamma28_eotf_f32;
    case ALWAN_TF_NLOG:       return alwan_nlog_eotf_f32;
    case ALWAN_TF_CINEON:     return alwan_cineon_eotf_f32;
    case ALWAN_TF_APPLE_LOG:  return alwan_apple_log_eotf_f32;
    case ALWAN_TF_FLOG:       return alwan_flog_eotf_f32;
    case ALWAN_TF_FLOG2:      return alwan_flog2_eotf_f32;
    case ALWAN_TF_LLOG:       return alwan_llog_eotf_f32;
    case ALWAN_TF_DLOG:       return alwan_dlog_eotf_f32;
    case ALWAN_TF_DCDM:       return alwan_dcdm_eotf_f32;
    case ALWAN_TF_ADX10:      return alwan_adx10_eotf_f32;
    case ALWAN_TF_ADX16:      return alwan_adx16_eotf_f32;
    }
    return NULL;
}

static inline alwan_tf_fn_f64 alwan__resolve_eotf_f64(alwan_transfer_function tf) {
    switch (tf) {
    case ALWAN_TF_LINEAR:     return alwan_linear_identity_f64;
    case ALWAN_TF_SRGB:       return alwan_srgb_eotf_f64;
    case ALWAN_TF_BT709:      return alwan_bt2020_eotf_f64;
    case ALWAN_TF_BT2020:     return alwan_bt2020_eotf_f64;
    case ALWAN_TF_PQ:         return alwan_pq_eotf_f64;
    case ALWAN_TF_ST2084:     return alwan_pq_eotf_f64;
    case ALWAN_TF_HLG:        return alwan_hlg_eotf_f64;
    case ALWAN_TF_BT1886:     return alwan_bt1886_eotf_f64;
    case ALWAN_TF_ACESPROXY:  return alwan_acesproxy_eotf_f64;
    case ALWAN_TF_ACESCC:     return alwan_acescc_eotf_f64;
    case ALWAN_TF_ACESCCT:    return alwan_acescct_eotf_f64;
    case ALWAN_TF_SLOG:       return alwan_slog_eotf_f64;
    case ALWAN_TF_SLOG2:      return alwan_slog2_eotf_f64;
    case ALWAN_TF_SLOG3:      return alwan_slog3_eotf_f64;
    case ALWAN_TF_CLOG:       return alwan_clog_eotf_f64;
    case ALWAN_TF_CLOG2:      return alwan_clog2_eotf_f64;
    case ALWAN_TF_CLOG3:      return alwan_clog3_eotf_f64;
    case ALWAN_TF_VLOG:       return alwan_vlog_eotf_f64;
    case ALWAN_TF_LOGC3:      return alwan_logc3_eotf_f64;
    case ALWAN_TF_LOGC4:      return alwan_logc4_eotf_f64;
    case ALWAN_TF_REDLOG:     return alwan_redlog_eotf_f64;
    case ALWAN_TF_REDLOGFILM: return alwan_redlogfilm_eotf_f64;
    case ALWAN_TF_LOG3G10:    return alwan_log3g10_eotf_f64;
    case ALWAN_TF_BMDFILM:    return alwan_bmdfilm_eotf_f64;
    case ALWAN_TF_BMDFILM4:   return alwan_bmdfilm4_eotf_f64;
    case ALWAN_TF_TLOG:       return alwan_tlog_eotf_f64;
    case ALWAN_TF_ELOG:       return alwan_elog_eotf_f64;
    case ALWAN_TF_PROTUNE:    return alwan_protune_eotf_f64;
    case ALWAN_TF_GAMMA22:    return alwan_gamma22_eotf_f64;
    case ALWAN_TF_GAMMA24:    return alwan_gamma24_eotf_f64;
    case ALWAN_TF_GAMMA26:    return alwan_gamma26_eotf_f64;
    case ALWAN_TF_GAMMA28:    return alwan_gamma28_eotf_f64;
    case ALWAN_TF_NLOG:       return alwan_nlog_eotf_f64;
    case ALWAN_TF_CINEON:     return alwan_cineon_eotf_f64;
    case ALWAN_TF_APPLE_LOG:  return alwan_apple_log_eotf_f64;
    case ALWAN_TF_FLOG:       return alwan_flog_eotf_f64;
    case ALWAN_TF_FLOG2:      return alwan_flog2_eotf_f64;
    case ALWAN_TF_LLOG:       return alwan_llog_eotf_f64;
    case ALWAN_TF_DLOG:       return alwan_dlog_eotf_f64;
    case ALWAN_TF_DCDM:       return alwan_dcdm_eotf_f64;
    case ALWAN_TF_ADX10:      return alwan_adx10_eotf_f64;
    case ALWAN_TF_ADX16:      return alwan_adx16_eotf_f64;
    }
    return NULL;
}

#endif /* ALWAN_INTERNAL_H */
