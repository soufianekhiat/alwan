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
#include "alwan_math.h"  /* ALWAN_POW / ALWAN_FMA / ... routing */

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

/* CAT matrices (3x3 = 9 elements). Dual f32/f64 twins so the templated
 * f32 path reads native float data; ALWAN_CORE_FNLIT(g_cat_NAME) selects. */
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_bradford_f32[9];
#endif
#if ALWAN_WITH_F64
extern alwan_f64 const g_cat_bradford_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_cat02_f32[9];
#endif
#if ALWAN_WITH_F64
extern alwan_f64 const g_cat_cat02_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_cat16_f32[9];
#endif
#if ALWAN_WITH_F64
extern alwan_f64 const g_cat_cat16_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_sharp_f32[9];
#endif
#if ALWAN_WITH_F64
extern alwan_f64 const g_cat_sharp_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_fairchild_f32[9];
#endif
#if ALWAN_WITH_F64
extern alwan_f64 const g_cat_fairchild_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_cmccat97_f32[9];
#endif
#if ALWAN_WITH_F64
extern alwan_f64 const g_cat_cmccat97_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_cmccat2000_f32[9];
#endif
#if ALWAN_WITH_F64
extern alwan_f64 const g_cat_cmccat2000_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_cat02_brill_2008_f32[9];
#endif
#if ALWAN_WITH_F64
extern alwan_f64 const g_cat_cat02_brill_2008_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_bianco_2010_f32[9];
#endif
#if ALWAN_WITH_F64
extern alwan_f64 const g_cat_bianco_2010_f64[9];
#endif
#if ALWAN_WITH_F32
extern alwan_f32 const g_cat_bianco_pc_2010_f32[9];
#endif
#if ALWAN_WITH_F64
extern alwan_f64 const g_cat_bianco_pc_2010_f64[9];
#endif

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

static inline void alwan__get_luma_coeffs(alwan_luma_standard standard,
                                           alwan_f64 *kr, alwan_f64 *kg,
                                           alwan_f64 *kb) {
    switch (standard) {
        case ALWAN_LUMA_BT601:
            *kr = ALWAN_LUMA_KR_BT601;
            *kg = ALWAN_LUMA_KG_BT601;
            *kb = ALWAN_LUMA_KB_BT601;
            break;
        case ALWAN_LUMA_BT709:
            *kr = ALWAN_LUMA_KR_BT709;
            *kg = ALWAN_LUMA_KG_BT709;
            *kb = ALWAN_LUMA_KB_BT709;
            break;
        case ALWAN_LUMA_BT2020:
            *kr = ALWAN_LUMA_KR_BT2020;
            *kg = ALWAN_LUMA_KG_BT2020;
            *kb = ALWAN_LUMA_KB_BT2020;
            break;
        case ALWAN_LUMA_ACES_AP1:
            *kr = ALWAN_LUMA_KR_AP1;
            *kg = ALWAN_LUMA_KG_AP1;
            *kb = ALWAN_LUMA_KB_AP1;
            break;
        case ALWAN_LUMA_ACES_AP0:
            *kr = ALWAN_LUMA_KR_AP0;
            *kg = ALWAN_LUMA_KG_AP0;
            *kb = ALWAN_LUMA_KB_AP0;
            break;
        case ALWAN_LUMA_DISPLAY_P3:
            *kr = ALWAN_LUMA_KR_P3;
            *kg = ALWAN_LUMA_KG_P3;
            *kb = ALWAN_LUMA_KB_P3;
            break;
        case ALWAN_LUMA_DCI_P3:
            *kr = ALWAN_LUMA_KR_DCIP3;
            *kg = ALWAN_LUMA_KG_DCIP3;
            *kb = ALWAN_LUMA_KB_DCIP3;
            break;
        case ALWAN_LUMA_ADOBE_RGB:
            *kr = ALWAN_LUMA_KR_ADOBE;
            *kg = ALWAN_LUMA_KG_ADOBE;
            *kb = ALWAN_LUMA_KB_ADOBE;
            break;
        case ALWAN_LUMA_PROPHOTO_RGB:
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

/* TF inline definitions are exposed transitively to consumers that call
 * alwan_*_oetf/eotf_* directly (alwan_view.c, alwan_convenience_core.h, …). */
#include "core/alwan_rgb_core.h"

/* ----------------------------------------------------------------
 * Transfer function resolution (enum -> function pointer)
 *
 * Single-source-of-truth X-macro tables drive every TF dispatch in the
 * library. Adding a TF means adding one row — the four scalar resolvers
 * (defined in alwan_tf_resolve.c) and the SIMD dispatch sites are all
 * regenerated from these tables.
 *
 *   TF_TABLE_BODY(X)      X(ENUM_SUFFIX, OETF_BASE, EOTF_BASE)
 *   TF_SIMD_TABLE_BODY(X) X(ENUM_SUFFIX, OETF_SIMD_FN, EOTF_SIMD_FN)
 *
 * Function names are alwan_<base>_oetf_<prec> / alwan_<base>_eotf_<prec>.
 * LINEAR is intentionally excluded from TF_TABLE_BODY — its identity is
 * not named *_oetf or *_eotf so the resolvers handle it as a separate case.
 * SIMD entries omit LINEAR (identity = NULL) and asymmetric pairs are
 * encoded explicitly (e.g. HLG uses the "_full" EOTF variant).
 * ---------------------------------------------------------------- */

#define TF_TABLE_BODY(X)                                  \
    X(SRGB,       srgb,         srgb)                     \
    X(BT709,      bt2020,       bt2020)                   \
    X(BT2020,     bt2020,       bt2020)                   \
    X(PQ,         pq,           pq)                       \
    X(HLG,        hlg,          hlg)                      \
    X(BT1886,     gamma24,      bt1886)                   \
    X(ACESPROXY,  acesproxy,    acesproxy)                \
    X(ACESCC,     acescc,       acescc)                   \
    X(ACESCCT,    acescct,      acescct)                  \
    X(SLOG,       slog,         slog)                     \
    X(SLOG2,      slog2,        slog2)                    \
    X(SLOG3,      slog3,        slog3)                    \
    X(CLOG,       clog,         clog)                     \
    X(CLOG2,      clog2,        clog2)                    \
    X(CLOG3,      clog3,        clog3)                    \
    X(VLOG,       vlog,         vlog)                     \
    X(LOGC3,      logc3,        logc3)                    \
    X(LOGC4,      logc4,        logc4)                    \
    X(REDLOG,     redlog,       redlog)                   \
    X(REDLOGFILM, redlogfilm,   redlogfilm)               \
    X(LOG3G10,    log3g10,      log3g10)                  \
    X(BMDFILM,    bmdfilm,      bmdfilm)                  \
    X(BMDFILM4,   bmdfilm4,     bmdfilm4)                 \
    X(TLOG,       tlog,         tlog)                     \
    X(ELOG,       elog,         elog)                     \
    X(PROTUNE,    protune,      protune)                  \
    X(GAMMA22,    gamma22,      gamma22)                  \
    X(GAMMA24,    gamma24,      gamma24)                  \
    X(GAMMA26,    gamma26,      gamma26)                  \
    X(GAMMA28,    gamma28,      gamma28)                  \
    X(NLOG,       nlog,         nlog)                     \
    X(CINEON,     cineon,       cineon)                   \
    X(APPLE_LOG,  apple_log,    apple_log)                \
    X(FLOG,       flog,         flog)                     \
    X(FLOG2,      flog2,        flog2)                    \
    X(LLOG,       llog,         llog)                     \
    X(DLOG,       dlog,         dlog)                     \
    X(DCDM,       dcdm,         dcdm)                     \
    X(ADX10,      adx10,        adx10)                    \
    X(ADX16,      adx16,        adx16)

#define TF_SIMD_TABLE_BODY(X)                             \
    X(SRGB, srgb_oetf_simd, srgb_eotf_simd)               \
    X(PQ,   pq_oetf_simd,   pq_eotf_simd)                 \
    X(HLG,  hlg_oetf_simd,  hlg_eotf_full_simd)

typedef float  (*alwan_tf_fn_f32)(float);
typedef double (*alwan_tf_fn_f64)(double);

alwan_tf_fn_f32 alwan__resolve_oetf_f32(alwan_transfer_function tf);
alwan_tf_fn_f32 alwan__resolve_eotf_f32(alwan_transfer_function tf);
alwan_tf_fn_f64 alwan__resolve_oetf_f64(alwan_transfer_function tf);
alwan_tf_fn_f64 alwan__resolve_eotf_f64(alwan_transfer_function tf);

#endif /* ALWAN_INTERNAL_H */
