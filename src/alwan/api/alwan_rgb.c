/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * RGB Color Spaces, Transfer Functions, and Matrix Derivation
 *
 * Transfer functions in alwan_rgb_core.h and alwan_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_rgb_core.h"
#include "../core/alwan_math_core.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* struct alwan_ctx is defined in alwan_internal.h */

/* ----------------------------------------------------------------
 * RGB Matrix Derivation
 * ---------------------------------------------------------------- */

int alwan_rgb_derive_matrices(alwan_mat3x3 *rgb_to_xyz,
                               alwan_mat3x3 *xyz_to_rgb,
                               alwan_rgb_space_desc const *desc) {
    if (!desc || !rgb_to_xyz || !xyz_to_rgb) {
        return ALWAN_E_INVALID;
    }

    alwan_rgb_matrices m = alwan_rgb_derive_matrices_v(
        desc->primaries_xy[0], desc->primaries_xy[1],
        desc->primaries_xy[2], desc->primaries_xy[3],
        desc->primaries_xy[4], desc->primaries_xy[5],
        desc->white_xy[0], desc->white_xy[1]);

    *rgb_to_xyz = m.rgb_to_xyz;
    *xyz_to_rgb = m.xyz_to_rgb;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> XYZ Direct Conversion
 * ---------------------------------------------------------------- */

int alwan_rgb_to_xyz(alwan_xyz *xyz,
                     alwan_rgb_space_desc const *space,
                     alwan_rgb const *rgb) {
    if (!space || !rgb || !xyz) {
        return ALWAN_E_INVALID;
    }

    alwan_mat3x3 const *mat;
    alwan_mat3x3 derived_rgb_to_xyz, derived_xyz_to_rgb;

    if (space->has_matrices) {
        mat = &space->rgb_to_xyz;
    } else {
        int status = alwan_rgb_derive_matrices(&derived_rgb_to_xyz, &derived_xyz_to_rgb, space);
        if (status != ALWAN_OK) return status;
        mat = &derived_rgb_to_xyz;
    }

    /* Apply RGB -> XYZ matrix */
    alwan_vec3 vec_in, vec_out;
    ALWAN_MEMCPY(&vec_in, rgb, sizeof(alwan_vec3));
    alwan_mat3_mulv(&vec_out, mat, &vec_in);
    ALWAN_MEMCPY(xyz, &vec_out, sizeof(alwan_vec3));

    return ALWAN_OK;
}

int alwan_xyz_to_rgb(alwan_rgb *rgb,
                     alwan_rgb_space_desc const *space,
                     alwan_xyz const *xyz) {
    if (!space || !xyz || !rgb) {
        return ALWAN_E_INVALID;
    }

    alwan_mat3x3 const *mat;
    alwan_mat3x3 derived_rgb_to_xyz, derived_xyz_to_rgb;

    if (space->has_matrices) {
        mat = &space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices(&derived_rgb_to_xyz, &derived_xyz_to_rgb, space);
        if (status != ALWAN_OK) return status;
        mat = &derived_xyz_to_rgb;
    }

    /* Apply XYZ -> RGB matrix */
    alwan_vec3 vec_in, vec_out;
    ALWAN_MEMCPY(&vec_in, xyz, sizeof(alwan_vec3));
    alwan_mat3_mulv(&vec_out, mat, &vec_in);
    ALWAN_MEMCPY(rgb, &vec_out, sizeof(alwan_vec3));

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Transfer Function API
 * ---------------------------------------------------------------- */

int alwan_oetf_apply(alwan_scalar *encoded,
                     alwan_transfer_function tf,
                     alwan_scalar const *linear, size_t count, size_t in_stride,
                     size_t out_stride) {
    if (!linear || !encoded) {
        return ALWAN_E_INVALID;
    }

    /* Select transfer function */
    alwan_scalar (*oetf_fn)(alwan_scalar) = NULL;

    switch (tf) {
        case ALWAN_TF_SRGB:
            oetf_fn = alwan_srgb_oetf;
            break;
        case ALWAN_TF_BT709:
        case ALWAN_TF_BT2020:
            oetf_fn = alwan_bt2020_oetf;
            break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084:
            oetf_fn = alwan_pq_oetf;
            break;
        case ALWAN_TF_HLG:
            oetf_fn = alwan_hlg_oetf;
            break;
        case ALWAN_TF_ACESPROXY:
            oetf_fn = alwan_acesproxy_oetf;
            break;
        case ALWAN_TF_BT1886:
            oetf_fn = alwan_gamma24_oetf;
            break;

        /* Extended Transfer Functions */
        case ALWAN_TF_SLOG:
            oetf_fn = alwan_slog_oetf;
            break;
        case ALWAN_TF_SLOG2:
            oetf_fn = alwan_slog2_oetf;
            break;
        case ALWAN_TF_SLOG3:
            oetf_fn = alwan_slog3_oetf;
            break;
        case ALWAN_TF_CLOG:
            oetf_fn = alwan_clog_oetf;
            break;
        case ALWAN_TF_CLOG2:
            oetf_fn = alwan_clog2_oetf;
            break;
        case ALWAN_TF_CLOG3:
            oetf_fn = alwan_clog3_oetf;
            break;
        case ALWAN_TF_VLOG:
            oetf_fn = alwan_vlog_oetf;
            break;
        case ALWAN_TF_LOGC3:
            oetf_fn = alwan_logc3_oetf;
            break;
        case ALWAN_TF_LOGC4:
            oetf_fn = alwan_logc4_oetf;
            break;
        case ALWAN_TF_REDLOG:
            oetf_fn = alwan_redlog_oetf;
            break;
        case ALWAN_TF_REDLOGFILM:
            oetf_fn = alwan_redlogfilm_oetf;
            break;
        case ALWAN_TF_LOG3G10:
            oetf_fn = alwan_log3g10_oetf;
            break;
        case ALWAN_TF_BMDFILM:
            oetf_fn = alwan_bmdfilm_oetf;
            break;
        case ALWAN_TF_BMDFILM4:
            oetf_fn = alwan_bmdfilm4_oetf;
            break;
        case ALWAN_TF_TLOG:
            oetf_fn = alwan_tlog_oetf;
            break;
        case ALWAN_TF_ELOG:
            oetf_fn = alwan_elog_oetf;
            break;
        case ALWAN_TF_PROTUNE:
            oetf_fn = alwan_protune_oetf;
            break;
        case ALWAN_TF_GAMMA22:
            oetf_fn = alwan_gamma22_oetf;
            break;
        case ALWAN_TF_GAMMA24:
            oetf_fn = alwan_gamma24_oetf;
            break;
        case ALWAN_TF_GAMMA26:
            oetf_fn = alwan_gamma26_oetf;
            break;
        case ALWAN_TF_GAMMA28:
            oetf_fn = alwan_gamma28_oetf;
            break;
        case ALWAN_TF_NLOG:
            oetf_fn = alwan_nlog_oetf;
            break;
        case ALWAN_TF_CINEON:
            oetf_fn = alwan_cineon_oetf;
            break;
        case ALWAN_TF_APPLE_LOG:
            oetf_fn = alwan_apple_log_oetf;
            break;
        case ALWAN_TF_FLOG:
            oetf_fn = alwan_flog_oetf;
            break;
        case ALWAN_TF_FLOG2:
            oetf_fn = alwan_flog2_oetf;
            break;
        case ALWAN_TF_LLOG:
            oetf_fn = alwan_llog_oetf;
            break;
        case ALWAN_TF_DLOG:
            oetf_fn = alwan_dlog_oetf;
            break;
        case ALWAN_TF_DCDM:
            oetf_fn = alwan_dcdm_oetf;
            break;
        case ALWAN_TF_LINEAR:
            oetf_fn = alwan_linear_identity;
            break;
        case ALWAN_TF_ACESCC:
            oetf_fn = alwan_acescc_oetf;
            break;
        case ALWAN_TF_ACESCCT:
            oetf_fn = alwan_acescct_oetf;
            break;
        case ALWAN_TF_ADX10:
            oetf_fn = alwan_adx10_oetf;
            break;
        case ALWAN_TF_ADX16:
            oetf_fn = alwan_adx16_oetf;
            break;

        default:
            return ALWAN_E_INVALID;
    }

    /* Apply transfer function to array */
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)linear + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)encoded + i * out_stride);
        *out_ptr = oetf_fn(*in_ptr);
    }

    return ALWAN_OK;
}

int alwan_eotf_apply(alwan_scalar *linear,
                     alwan_transfer_function tf,
                     alwan_scalar const *encoded, size_t count, size_t in_stride,
                     size_t out_stride) {
    if (!encoded || !linear) {
        return ALWAN_E_INVALID;
    }

    /* Select transfer function */
    alwan_scalar (*eotf_fn)(alwan_scalar) = NULL;

    switch (tf) {
        case ALWAN_TF_SRGB:
            eotf_fn = alwan_srgb_eotf;
            break;
        case ALWAN_TF_BT709:
        case ALWAN_TF_BT2020:
            eotf_fn = alwan_bt2020_eotf;
            break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084:
            eotf_fn = alwan_pq_eotf;
            break;
        case ALWAN_TF_HLG:
            eotf_fn = alwan_hlg_eotf;
            break;
        case ALWAN_TF_BT1886:
            eotf_fn = alwan_bt1886_eotf;
            break;
        case ALWAN_TF_ACESPROXY:
            eotf_fn = alwan_acesproxy_eotf;
            break;

        /* Extended Transfer Functions */
        case ALWAN_TF_SLOG:
            eotf_fn = alwan_slog_eotf;
            break;
        case ALWAN_TF_SLOG2:
            eotf_fn = alwan_slog2_eotf;
            break;
        case ALWAN_TF_SLOG3:
            eotf_fn = alwan_slog3_eotf;
            break;
        case ALWAN_TF_CLOG:
            eotf_fn = alwan_clog_eotf;
            break;
        case ALWAN_TF_CLOG2:
            eotf_fn = alwan_clog2_eotf;
            break;
        case ALWAN_TF_CLOG3:
            eotf_fn = alwan_clog3_eotf;
            break;
        case ALWAN_TF_VLOG:
            eotf_fn = alwan_vlog_eotf;
            break;
        case ALWAN_TF_LOGC3:
            eotf_fn = alwan_logc3_eotf;
            break;
        case ALWAN_TF_LOGC4:
            eotf_fn = alwan_logc4_eotf;
            break;
        case ALWAN_TF_REDLOG:
            eotf_fn = alwan_redlog_eotf;
            break;
        case ALWAN_TF_REDLOGFILM:
            eotf_fn = alwan_redlogfilm_eotf;
            break;
        case ALWAN_TF_LOG3G10:
            eotf_fn = alwan_log3g10_eotf;
            break;
        case ALWAN_TF_BMDFILM:
            eotf_fn = alwan_bmdfilm_eotf;
            break;
        case ALWAN_TF_BMDFILM4:
            eotf_fn = alwan_bmdfilm4_eotf;
            break;
        case ALWAN_TF_TLOG:
            eotf_fn = alwan_tlog_eotf;
            break;
        case ALWAN_TF_ELOG:
            eotf_fn = alwan_elog_eotf;
            break;
        case ALWAN_TF_PROTUNE:
            eotf_fn = alwan_protune_eotf;
            break;
        case ALWAN_TF_GAMMA22:
            eotf_fn = alwan_gamma22_eotf;
            break;
        case ALWAN_TF_GAMMA24:
            eotf_fn = alwan_gamma24_eotf;
            break;
        case ALWAN_TF_GAMMA26:
            eotf_fn = alwan_gamma26_eotf;
            break;
        case ALWAN_TF_GAMMA28:
            eotf_fn = alwan_gamma28_eotf;
            break;
        case ALWAN_TF_NLOG:
            eotf_fn = alwan_nlog_eotf;
            break;
        case ALWAN_TF_CINEON:
            eotf_fn = alwan_cineon_eotf;
            break;
        case ALWAN_TF_APPLE_LOG:
            eotf_fn = alwan_apple_log_eotf;
            break;
        case ALWAN_TF_FLOG:
            eotf_fn = alwan_flog_eotf;
            break;
        case ALWAN_TF_FLOG2:
            eotf_fn = alwan_flog2_eotf;
            break;
        case ALWAN_TF_LLOG:
            eotf_fn = alwan_llog_eotf;
            break;
        case ALWAN_TF_DLOG:
            eotf_fn = alwan_dlog_eotf;
            break;
        case ALWAN_TF_DCDM:
            eotf_fn = alwan_dcdm_eotf;
            break;
        case ALWAN_TF_LINEAR:
            eotf_fn = alwan_linear_identity;
            break;
        case ALWAN_TF_ACESCC:
            eotf_fn = alwan_acescc_eotf;
            break;
        case ALWAN_TF_ACESCCT:
            eotf_fn = alwan_acescct_eotf;
            break;
        case ALWAN_TF_ADX10:
            eotf_fn = alwan_adx10_eotf;
            break;
        case ALWAN_TF_ADX16:
            eotf_fn = alwan_adx16_eotf;
            break;

        default:
            return ALWAN_E_INVALID;
    }

    /* Apply transfer function to array */
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)encoded + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)linear + i * out_stride);
        *out_ptr = eotf_fn(*in_ptr);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * M11: RGB <-> RGB Conversion
 * ---------------------------------------------------------------- */

/* Convert RGB color from one color space to another */
int alwan_rgb_convert(alwan_rgb *dst_rgb,
                      alwan_ctx *ctx,
                      alwan_rgb_space_desc const *src_space,
                      alwan_rgb_space_desc const *dst_space,
                      alwan_rgb const *src_rgb) {
    if (!src_space || !dst_space || !src_rgb || !dst_rgb) {
        return ALWAN_E_INVALID;
    }

    /* Get conversion matrices for both spaces */
    alwan_mat3x3 src_to_xyz, xyz_to_src;
    alwan_mat3x3 dst_to_xyz, xyz_to_dst;

    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
        xyz_to_src = src_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    if (dst_space->has_matrices) {
        dst_to_xyz = dst_space->rgb_to_xyz;
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Convert source RGB to XYZ */
    alwan_vec3 vec_in;
    ALWAN_MEMCPY(&vec_in, src_rgb, sizeof(alwan_vec3));
    alwan_vec3 xyz;
    alwan_mat3_mulv(&xyz, &src_to_xyz, &vec_in);

    /* Check if chromatic adaptation is needed */
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
    alwan_scalar dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_scalar dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_adaptation = (ALWAN_ABS(dx) > tolerance || ALWAN_ABS(dy) > tolerance);

    int status;
    if (need_adaptation && ctx) {
        /* Perform chromatic adaptation using Bradford CAT */
        alwan_xyy src_white_xyy, dst_white_xyy;
        alwan_xyz src_white_xyz, dst_white_xyz;

        /* Convert xy to XYZ (using Y=1.0) */
        src_white_xyy.x = src_space->white_xy[0];
        src_white_xyy.y = src_space->white_xy[1];
        src_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&src_white_xyz, &src_white_xyy);

        dst_white_xyy.x = dst_space->white_xy[0];
        dst_white_xyy.y = dst_space->white_xy[1];
        dst_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&dst_white_xyz, &dst_white_xyy);

        /* Compute and apply CAT matrix */
        alwan_mat3x3 cat_matrix;
        status = alwan_cat_matrix(&cat_matrix, &src_white_xyz, &dst_white_xyz,
                                  ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        alwan_vec3 xyz_adapted;
        alwan_mat3_mulv(&xyz_adapted, &cat_matrix, &xyz);
        xyz = xyz_adapted;
    }

    /* Convert adapted XYZ to destination RGB */
    alwan_vec3 vec_out;
    alwan_mat3_mulv(&vec_out, &xyz_to_dst, &xyz);
    ALWAN_MEMCPY(dst_rgb, &vec_out, sizeof(alwan_vec3));

    return ALWAN_OK;
}

/* Map RGB color space conversion */
int alwan_rgb_convert_map_interleave(alwan_rgb *dst_rgb,
                            alwan_ctx *ctx,
                            alwan_rgb_space_desc const *src_space,
                            alwan_rgb_space_desc const *dst_space,
                            alwan_rgb const *src_rgb,
                            size_t count) {
    if (!src_space || !dst_space || !src_rgb || !dst_rgb || count == 0) {
        return ALWAN_E_INVALID;
    }

    /* Get conversion matrices once for all colors */
    alwan_mat3x3 src_to_xyz, xyz_to_src;
    alwan_mat3x3 dst_to_xyz, xyz_to_dst;

    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
        xyz_to_src = src_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    if (dst_space->has_matrices) {
        dst_to_xyz = dst_space->rgb_to_xyz;
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Check if chromatic adaptation is needed */
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
    alwan_scalar dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_scalar dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_adaptation = (ALWAN_ABS(dx) > tolerance || ALWAN_ABS(dy) > tolerance);

    /* Precompute adaptation matrix if needed */
    alwan_mat3x3 cat_matrix;
    if (need_adaptation && ctx) {
        alwan_xyy src_white_xyy, dst_white_xyy;
        alwan_xyz src_white_xyz, dst_white_xyz;

        /* Convert xy to XYZ (using Y=1.0) */
        src_white_xyy.x = src_space->white_xy[0];
        src_white_xyy.y = src_space->white_xy[1];
        src_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&src_white_xyz, &src_white_xyy);

        dst_white_xyy.x = dst_space->white_xy[0];
        dst_white_xyy.y = dst_space->white_xy[1];
        dst_white_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&dst_white_xyz, &dst_white_xyy);

        /* Compute CAT matrix once */
        int status = alwan_cat_matrix(&cat_matrix, &src_white_xyz, &dst_white_xyz,
                                      ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;
    }

    /* Convert all colors */
    for (size_t i = 0; i < count; i++) {
        /* Convert source RGB to XYZ */
        alwan_vec3 xyz, vec_in;
        ALWAN_MEMCPY(&vec_in, &src_rgb[i], sizeof(alwan_vec3));
        alwan_mat3_mulv(&xyz, &src_to_xyz, &vec_in);

        /* Apply chromatic adaptation if needed */
        if (need_adaptation && ctx) {
            alwan_vec3 xyz_adapted;
            alwan_mat3_mulv(&xyz_adapted, &cat_matrix, &xyz);
            xyz = xyz_adapted;
        }

        /* Convert adapted XYZ to destination RGB */
        alwan_vec3 vec_out;
        alwan_mat3_mulv(&vec_out, &xyz_to_dst, &xyz);
        ALWAN_MEMCPY(&dst_rgb[i], &vec_out, sizeof(alwan_vec3));
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB Space Descriptor Helper
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * Embedded RGB Space Data
 * ---------------------------------------------------------------- */
#include "../alwan_rgb_embedded.h"
#include "../alwan_rgb_matrices_embedded.h"

/* ----------------------------------------------------------------
 * Transfer Function Lookup Table
 * Maps each alwan_rgb_space enum index to its {oetf, eotf} pair.
 * Order MUST match the alwan_rgb_space enum.
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_transfer_function oetf;
    alwan_transfer_function eotf;
} alwan_tf_pair;

static alwan_tf_pair const g_rgb_space_tf[] = {
    /* [0]  SRGB                     */ { ALWAN_TF_SRGB,     ALWAN_TF_SRGB     },
    /* [1]  BT709                    */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [2]  DISPLAY_P3               */ { ALWAN_TF_SRGB,     ALWAN_TF_SRGB     },
    /* [3]  BT2020                   */ { ALWAN_TF_BT2020,   ALWAN_TF_BT2020   },
    /* [4]  ACES2065_1               */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [5]  ACESCG                   */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [6]  ACESPROXY                */ { ALWAN_TF_ACESPROXY,ALWAN_TF_ACESPROXY},
    /* [7]  ACESCC                   */ { ALWAN_TF_ACESCC,   ALWAN_TF_ACESCC   },
    /* [8]  ACESCCT                  */ { ALWAN_TF_ACESCCT,  ALWAN_TF_ACESCCT  },
    /* [9]  ARRI_WIDE_GAMUT_3        */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [10] ARRI_WIDE_GAMUT_4        */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [11] ARRI_LOGC3               */ { ALWAN_TF_LOGC3,    ALWAN_TF_LOGC3    },
    /* [12] ARRI_LOGC4               */ { ALWAN_TF_LOGC4,    ALWAN_TF_LOGC4    },
    /* [13] REDCOLOR                 */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [14] REDCOLOR2                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [15] REDCOLOR3                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [16] REDCOLOR4                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [17] DRAGONCOLOR              */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [18] DRAGONCOLOR2             */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [19] REDLOG                   */ { ALWAN_TF_REDLOG,   ALWAN_TF_REDLOG   },
    /* [20] VENICE_S_GAMUT3          */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [21] VENICE_S_GAMUT3_CINE     */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [22] S_LOG                    */ { ALWAN_TF_SLOG,     ALWAN_TF_SLOG     },
    /* [23] S_LOG2                   */ { ALWAN_TF_SLOG2,    ALWAN_TF_SLOG2    },
    /* [24] S_LOG3                   */ { ALWAN_TF_SLOG3,    ALWAN_TF_SLOG3    },
    /* [25] CIE_RGB                  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [26] ADOBE_WIDE_GAMUT_RGB     */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [27] ROMM_RGB                 */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [28] RIMM_RGB                 */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [29] ERIMM_RGB                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [30] FILMLIGHT_E_GAMUT        */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [31] FILMLIGHT_T_LOG          */ { ALWAN_TF_TLOG,     ALWAN_TF_TLOG     },
    /* [32] F_GAMUT                  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [33] FUJIFILM_F_LOG           */ { ALWAN_TF_FLOG,     ALWAN_TF_FLOG     },
    /* [34] N_GAMUT                  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [35] N_LOG                    */ { ALWAN_TF_NLOG,     ALWAN_TF_NLOG     },
    /* [36] DJI_D_GAMUT             */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [37] PROTUNE_NATIVE           */ { ALWAN_TF_PROTUNE,  ALWAN_TF_PROTUNE  },
    /* [38] ITU_R_BT470_525          */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [39] ITU_R_BT470_625          */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [40] SMPTE_240M               */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [41] SMPTE_C                  */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [42] DCDM_XYZ                 */ { ALWAN_TF_DCDM,     ALWAN_TF_DCDM     },
    /* [43] BEST_RGB                 */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [44] BETA_RGB                 */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [45] DON_RGB_4                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [46] EKTA_SPACE_PS5           */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [47] MAX_RGB                  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [48] RUSSELL_RGB              */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [49] SHARP_RGB                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [50] ECI_RGB_V2               */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [51] ADOBE_RGB_1998           */ { ALWAN_TF_GAMMA22,  ALWAN_TF_GAMMA22  },
    /* [52] PROPHOTO_RGB             */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [53] DAVINCI_WIDE_GAMUT       */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [54] DAVINCI_INTERMEDIATE     */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [55] BLACKMAGIC_WIDE_GAMUT    */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [56] BLACKMAGIC_FILM          */ { ALWAN_TF_BMDFILM4, ALWAN_TF_BMDFILM4 },
    /* [57] BLACKMAGIC_FILM_GEN5     */ { ALWAN_TF_BMDFILM,  ALWAN_TF_BMDFILM  },
    /* [58] V_GAMUT                  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [59] V_LOG                    */ { ALWAN_TF_VLOG,     ALWAN_TF_VLOG     },
    /* [60] S_GAMUT                  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [61] S_GAMUT3                 */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [62] S_GAMUT3_CINE            */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [63] CINEMA_GAMUT             */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [64] CANON_LOG                */ { ALWAN_TF_CLOG,     ALWAN_TF_CLOG     },
    /* [65] REDWIDEGAMUTRGB          */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [66] DCI_P3                   */ { ALWAN_TF_GAMMA26,  ALWAN_TF_GAMMA26  },
    /* [67] DCI_P3_P                 */ { ALWAN_TF_GAMMA26,  ALWAN_TF_GAMMA26  },
    /* [68] P3_D65                   */ { ALWAN_TF_SRGB,     ALWAN_TF_SRGB     },
    /* [69] NTSC_1953                */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [70] NTSC_1987                */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [71] PAL_SECAM                */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [72] EBU_TECH_3213_E          */ { ALWAN_TF_BT709,    ALWAN_TF_BT709    },
    /* [73] APPLE_RGB                */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [74] COLORMATCH_RGB           */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [75] ALEXA_WIDE_GAMUT         */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [76] P3_D60                   */ { ALWAN_TF_GAMMA26,  ALWAN_TF_GAMMA26  },
    /* [77] XTREME_RGB               */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [78] LINEAR_REC709            */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [79] LINEAR_REC2020           */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [80] LINEAR_ADOBE_RGB_1998    */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [81] LINEAR_P3_D65            */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [82] LINEAR_DISPLAY_P3        */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [83] LINEAR_PROPHOTO_RGB      */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [84] LINEAR_DCI_P3            */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [85] LINEAR_ADOBE_WIDE_GAMUT  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [86] LINEAR_APPLE_RGB         */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [87] LINEAR_COLORMATCH_RGB    */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [88] LINEAR_P3_D60            */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [89] LINEAR_BT470_525         */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [90] LINEAR_BT470_625         */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [91] LINEAR_SMPTE_240M        */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [92] ITU_T_H273_22_UNSPEC     */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [93] ITU_T_H273_GENERIC_FILM  */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [94] PLASA_ANSI_E154          */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* [95] GAMMA22_REC709           */ { ALWAN_TF_GAMMA22,  ALWAN_TF_GAMMA22  },
    /* [96] GAMMA22_ADOBE_RGB        */ { ALWAN_TF_GAMMA22,  ALWAN_TF_GAMMA22  },
    /* [97] GAMMA22_P3_D65           */ { ALWAN_TF_GAMMA22,  ALWAN_TF_GAMMA22  },
    /* [98] GAMMA22_AP1              */ { ALWAN_TF_GAMMA22,  ALWAN_TF_GAMMA22  },
    /* [99] GAMMA18_REC709           */ { ALWAN_TF_LINEAR,   ALWAN_TF_LINEAR   },
    /* ColorInterop Display Color Spaces */
    /* [100] REC1886_REC709          */ { ALWAN_TF_BT709,    ALWAN_TF_BT1886   },
    /* [101] REC2100_PQ              */ { ALWAN_TF_PQ,       ALWAN_TF_PQ       },
    /* [102] REC2100_HLG             */ { ALWAN_TF_HLG,      ALWAN_TF_HLG      },
    /* [103] DISPLAY_P3_HDR          */ { ALWAN_TF_PQ,       ALWAN_TF_PQ       },
};

/* Static assert: TF table must match embedded data array size */
#if ALWAN_EMBED_DATA
_Static_assert(
    sizeof(g_rgb_space_tf) / sizeof(g_rgb_space_tf[0]) ==
    sizeof(g_rgb_space_data) / sizeof(g_rgb_space_data[0]),
    "g_rgb_space_tf[] and g_rgb_space_data[] size mismatch"
);
#endif

/* ----------------------------------------------------------------
 * Get RGB space descriptor by enum
 * ---------------------------------------------------------------- */

int alwan_rgb_get_space_descriptor(alwan_rgb_space_desc *desc, alwan_ctx *ctx, alwan_rgb_space space) {
    if (!desc) {
        return ALWAN_E_INVALID;
    }

#if ALWAN_EMBED_DATA
    /* Embedded data mode - direct array indexing (enum values map to array indices) */
    (void)ctx;  /* Unused in embedded mode */

    /* Bounds check: ensure enum value is within valid array range */
    if (space < 0 || (size_t)space >= g_rgb_space_data_count) {
        return ALWAN_E_INVALID;
    }

    /* Direct lookup using enum as index - data format: [rx, ry, gx, gy, bx, by, wx, wy] */
    alwan_scalar const *rgb_data = g_rgb_space_data[space];

    /* Copy primaries (first 6 values) and white point (last 2 values) */
    for (int j = 0; j < 6; j++) {
        desc->primaries_xy[j] = rgb_data[j];
    }
    desc->white_xy[0] = rgb_data[6];
    desc->white_xy[1] = rgb_data[7];

    /* Look up transfer functions from the TF table */
    desc->oetf = g_rgb_space_tf[space].oetf;
    desc->eotf = g_rgb_space_tf[space].eotf;

    /* Load precomputed matrices */
    alwan_scalar const *mat_data = g_rgb_space_matrices[space];
    for (int j = 0; j < 9; j++) {
        desc->rgb_to_xyz.m[j] = mat_data[j];
        desc->xyz_to_rgb.m[j] = mat_data[j + 9];
    }
    desc->has_matrices = 1;

    return ALWAN_OK;

#else
    #error "Only ALWAN_EMBED_DATA mode is supported. Runtime CSV loading has been removed."
#endif /* ALWAN_EMBED_DATA */
}

/* ----------------------------------------------------------------
 * Arbitrary Gamma OETF/EOTF
 * ---------------------------------------------------------------- */

int alwan_gamma_oetf(alwan_scalar *out, alwan_scalar const *in,
                      alwan_scalar gamma, size_t count,
                      size_t in_stride, size_t out_stride) {
    if (!out || !in) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)out + i * out_stride);
        *out_ptr = alwan_gamma_oetf_v(*in_ptr, gamma);
    }
    return ALWAN_OK;
}

int alwan_gamma_eotf(alwan_scalar *out, alwan_scalar const *in,
                      alwan_scalar gamma, size_t count,
                      size_t in_stride, size_t out_stride) {
    if (!out || !in) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)out + i * out_stride);
        *out_ptr = alwan_gamma_eotf_v(*in_ptr, gamma);
    }
    return ALWAN_OK;
}
