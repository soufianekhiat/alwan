/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * LUT baking, 2D flatten/unflatten, and trilinear sampling
 * Per-pixel math in alwan_lut_core.h, TFs in alwan_rgb_core.h
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_lut_core.h"
#include "../core/alwan_rgb_core.h"
#include "../core/alwan_colorspace_core.h"
#include "../core/alwan_view_core.h"
#include "../core/alwan_hdr_core.h"
#include "../core/alwan_math_core.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Transfer function resolvers (same as alwan_rgb_map.c)
 * ---------------------------------------------------------------- */

static alwan_scalar (*lut__resolve_eotf(alwan_transfer_function tf))(alwan_scalar) {
    switch (tf) {
    case ALWAN_TF_LINEAR:     return alwan_linear_identity;
    case ALWAN_TF_SRGB:       return alwan_srgb_eotf;
    case ALWAN_TF_BT709:      return alwan_bt2020_eotf;
    case ALWAN_TF_BT2020:     return alwan_bt2020_eotf;
    case ALWAN_TF_PQ:         return alwan_pq_eotf;
    case ALWAN_TF_ST2084:     return alwan_pq_eotf;
    case ALWAN_TF_HLG:        return alwan_hlg_eotf;
    case ALWAN_TF_BT1886:     return alwan_bt1886_eotf;
    case ALWAN_TF_ACESPROXY:  return alwan_acesproxy_eotf;
    case ALWAN_TF_ACESCC:     return alwan_acescc_eotf;
    case ALWAN_TF_ACESCCT:    return alwan_acescct_eotf;
    case ALWAN_TF_SLOG:       return alwan_slog_eotf;
    case ALWAN_TF_SLOG2:      return alwan_slog2_eotf;
    case ALWAN_TF_SLOG3:      return alwan_slog3_eotf;
    case ALWAN_TF_CLOG:       return alwan_clog_eotf;
    case ALWAN_TF_CLOG2:      return alwan_clog2_eotf;
    case ALWAN_TF_CLOG3:      return alwan_clog3_eotf;
    case ALWAN_TF_VLOG:       return alwan_vlog_eotf;
    case ALWAN_TF_LOGC3:      return alwan_logc3_eotf;
    case ALWAN_TF_LOGC4:      return alwan_logc4_eotf;
    case ALWAN_TF_REDLOG:     return alwan_redlog_eotf;
    case ALWAN_TF_REDLOGFILM: return alwan_redlogfilm_eotf;
    case ALWAN_TF_LOG3G10:    return alwan_log3g10_eotf;
    case ALWAN_TF_BMDFILM:    return alwan_bmdfilm_eotf;
    case ALWAN_TF_BMDFILM4:   return alwan_bmdfilm4_eotf;
    case ALWAN_TF_TLOG:       return alwan_tlog_eotf;
    case ALWAN_TF_ELOG:       return alwan_elog_eotf;
    case ALWAN_TF_PROTUNE:    return alwan_protune_eotf;
    case ALWAN_TF_GAMMA22:    return alwan_gamma22_eotf;
    case ALWAN_TF_GAMMA24:    return alwan_gamma24_eotf;
    case ALWAN_TF_GAMMA26:    return alwan_gamma26_eotf;
    case ALWAN_TF_GAMMA28:    return alwan_gamma28_eotf;
    case ALWAN_TF_NLOG:       return alwan_nlog_eotf;
    case ALWAN_TF_CINEON:     return alwan_cineon_eotf;
    case ALWAN_TF_APPLE_LOG:  return alwan_apple_log_eotf;
    case ALWAN_TF_FLOG:       return alwan_flog_eotf;
    case ALWAN_TF_FLOG2:      return alwan_flog2_eotf;
    case ALWAN_TF_LLOG:       return alwan_llog_eotf;
    case ALWAN_TF_DLOG:       return alwan_dlog_eotf;
    case ALWAN_TF_DCDM:       return alwan_dcdm_eotf;
    case ALWAN_TF_ADX10:      return alwan_adx10_eotf;
    case ALWAN_TF_ADX16:      return alwan_adx16_eotf;
    default:                  return NULL;
    }
}

static alwan_scalar (*lut__resolve_oetf(alwan_transfer_function tf))(alwan_scalar) {
    switch (tf) {
    case ALWAN_TF_LINEAR:     return alwan_linear_identity;
    case ALWAN_TF_SRGB:       return alwan_srgb_oetf;
    case ALWAN_TF_BT709:      return alwan_bt2020_oetf;
    case ALWAN_TF_BT2020:     return alwan_bt2020_oetf;
    case ALWAN_TF_PQ:         return alwan_pq_oetf;
    case ALWAN_TF_ST2084:     return alwan_pq_oetf;
    case ALWAN_TF_HLG:        return alwan_hlg_oetf;
    case ALWAN_TF_BT1886:     return alwan_gamma24_oetf;
    case ALWAN_TF_ACESPROXY:  return alwan_acesproxy_oetf;
    case ALWAN_TF_ACESCC:     return alwan_acescc_oetf;
    case ALWAN_TF_ACESCCT:    return alwan_acescct_oetf;
    case ALWAN_TF_SLOG:       return alwan_slog_oetf;
    case ALWAN_TF_SLOG2:      return alwan_slog2_oetf;
    case ALWAN_TF_SLOG3:      return alwan_slog3_oetf;
    case ALWAN_TF_CLOG:       return alwan_clog_oetf;
    case ALWAN_TF_CLOG2:      return alwan_clog2_oetf;
    case ALWAN_TF_CLOG3:      return alwan_clog3_oetf;
    case ALWAN_TF_VLOG:       return alwan_vlog_oetf;
    case ALWAN_TF_LOGC3:      return alwan_logc3_oetf;
    case ALWAN_TF_LOGC4:      return alwan_logc4_oetf;
    case ALWAN_TF_REDLOG:     return alwan_redlog_oetf;
    case ALWAN_TF_REDLOGFILM: return alwan_redlogfilm_oetf;
    case ALWAN_TF_LOG3G10:    return alwan_log3g10_oetf;
    case ALWAN_TF_BMDFILM:    return alwan_bmdfilm_oetf;
    case ALWAN_TF_BMDFILM4:   return alwan_bmdfilm4_oetf;
    case ALWAN_TF_TLOG:       return alwan_tlog_oetf;
    case ALWAN_TF_ELOG:       return alwan_elog_oetf;
    case ALWAN_TF_PROTUNE:    return alwan_protune_oetf;
    case ALWAN_TF_GAMMA22:    return alwan_gamma22_oetf;
    case ALWAN_TF_GAMMA24:    return alwan_gamma24_oetf;
    case ALWAN_TF_GAMMA26:    return alwan_gamma26_oetf;
    case ALWAN_TF_GAMMA28:    return alwan_gamma28_oetf;
    case ALWAN_TF_NLOG:       return alwan_nlog_oetf;
    case ALWAN_TF_CINEON:     return alwan_cineon_oetf;
    case ALWAN_TF_APPLE_LOG:  return alwan_apple_log_oetf;
    case ALWAN_TF_FLOG:       return alwan_flog_oetf;
    case ALWAN_TF_FLOG2:      return alwan_flog2_oetf;
    case ALWAN_TF_LLOG:       return alwan_llog_oetf;
    case ALWAN_TF_DLOG:       return alwan_dlog_oetf;
    case ALWAN_TF_DCDM:       return alwan_dcdm_oetf;
    case ALWAN_TF_ADX10:      return alwan_adx10_oetf;
    case ALWAN_TF_ADX16:      return alwan_adx16_oetf;
    default:                  return NULL;
    }
}

/* ----------------------------------------------------------------
 * Combined matrix builder (src linear RGB -> dst linear RGB)
 * Same logic as alwan_image_convert
 * ---------------------------------------------------------------- */

static int lut__build_combined_matrix(alwan_mat3x3 *combined,
                                       alwan_ctx *ctx,
                                       alwan_rgb_space_desc const *src_space,
                                       alwan_rgb_space_desc const *dst_space) {
    alwan_mat3x3 src_to_xyz, xyz_to_src;
    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
    } else {
        int status = alwan_rgb_derive_matrices(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    alwan_mat3x3 dst_to_xyz, xyz_to_dst;
    if (dst_space->has_matrices) {
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Chromatic adaptation if white points differ */
    alwan_scalar const tol = ALWAN_LITERAL(1e-6);
    alwan_scalar dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_scalar dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_cat = (ALWAN_ABS(dx) > tol || ALWAN_ABS(dy) > tol);

    if (need_cat && ctx) {
        alwan_xyy src_xyy, dst_xyy;
        alwan_xyz src_wp, dst_wp;
        src_xyy.x = src_space->white_xy[0];
        src_xyy.y = src_space->white_xy[1];
        src_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&src_wp, &src_xyy);

        dst_xyy.x = dst_space->white_xy[0];
        dst_xyy.y = dst_space->white_xy[1];
        dst_xyy.Y = ALWAN_LITERAL(1.0);
        alwan_xyy_to_xyz(&dst_wp, &dst_xyy);

        alwan_mat3x3 cat;
        int status = alwan_cat_matrix(&cat, &src_wp, &dst_wp, ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        alwan_mat3x3 tmp = alwan_mat3_mul_v(cat, src_to_xyz);
        *combined = alwan_mat3_mul_v(xyz_to_dst, tmp);
    } else {
        *combined = alwan_mat3_mul_v(xyz_to_dst, src_to_xyz);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * View transform applicator (same dispatch as alwan_view.c)
 * ---------------------------------------------------------------- */

static alwan_vec3 lut__apply_view(alwan_view_transform vt, alwan_vec3 rgb) {
    alwan_vec3 in = {{
        alwan_max(rgb.v[0], ALWAN_ZERO),
        alwan_max(rgb.v[1], ALWAN_ZERO),
        alwan_max(rgb.v[2], ALWAN_ZERO)
    }};
    alwan_vec3 r;

    switch (vt) {
    case ALWAN_VIEW_ACES_REC709:
        r.v[0] = alwan_saturate(alwan_aces_tonemap_v(in.v[0]));
        r.v[1] = alwan_saturate(alwan_aces_tonemap_v(in.v[1]));
        r.v[2] = alwan_saturate(alwan_aces_tonemap_v(in.v[2]));
        return r;
    case ALWAN_VIEW_REINHARD_EXT:
        r = alwan_reinhard_extended_luma_v(in, ALWAN_LITERAL(4.0));
        r.v[0] = alwan_saturate(r.v[0]);
        r.v[1] = alwan_saturate(r.v[1]);
        r.v[2] = alwan_saturate(r.v[2]);
        return r;
    case ALWAN_VIEW_UCHIMURA:
        r.v[0] = alwan_saturate(alwan_uchimura_default_v(in.v[0]));
        r.v[1] = alwan_saturate(alwan_uchimura_default_v(in.v[1]));
        r.v[2] = alwan_saturate(alwan_uchimura_default_v(in.v[2]));
        return r;
    case ALWAN_VIEW_LOTTES:
        r = alwan_lottes_default_v(in);
        r.v[0] = alwan_saturate(r.v[0]);
        r.v[1] = alwan_saturate(r.v[1]);
        r.v[2] = alwan_saturate(r.v[2]);
        return r;
    case ALWAN_VIEW_EXPOSURE:
        r = alwan_exposure_tonemap_rgb_v(in, ALWAN_ZERO);
        r.v[0] = alwan_saturate(r.v[0]);
        r.v[1] = alwan_saturate(r.v[1]);
        r.v[2] = alwan_saturate(r.v[2]);
        return r;
    case ALWAN_VIEW_REINHARD_CALIBRATED:
        r.v[0] = alwan_saturate(alwan_reinhard_calibrated_v(
                     in.v[0], ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(4.0)));
        r.v[1] = alwan_saturate(alwan_reinhard_calibrated_v(
                     in.v[1], ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(4.0)));
        r.v[2] = alwan_saturate(alwan_reinhard_calibrated_v(
                     in.v[2], ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(4.0)));
        return r;
    case ALWAN_VIEW_KHRONOS_PBR_NEUTRAL:
        return alwan_khronos_pbr_neutral_v(in);
    default:
        /* For transforms not easily dispatched here (AgX full pipeline, etc.),
         * return input unchanged */
        return rgb;
    }
}

/* ----------------------------------------------------------------
 * 3D LUT Baking
 * ---------------------------------------------------------------- */

int alwan_bake_3dlut(alwan_scalar *out, int size,
                     alwan_ctx *ctx,
                     alwan_rgb_space_desc const *src_space,
                     alwan_rgb_space_desc const *dst_space) {
    if (!out || !src_space || !dst_space || size < 2 || size > 256) {
        return ALWAN_E_INVALID;
    }

    /* Build combined conversion matrix */
    alwan_mat3x3 combined;
    int status = lut__build_combined_matrix(&combined, ctx, src_space, dst_space);
    if (status != ALWAN_OK) return status;

    /* Resolve transfer functions */
    alwan_scalar (*eotf_fn)(alwan_scalar) = lut__resolve_eotf(src_space->eotf);
    alwan_scalar (*oetf_fn)(alwan_scalar) = lut__resolve_oetf(dst_space->oetf);
    if (!eotf_fn || !oetf_fn) return ALWAN_E_INVALID;

    /* Sample every point in the cube */
    size_t const total = (size_t)size * (size_t)size * (size_t)size;
    for (size_t i = 0; i < total; i++) {
        alwan_vec3 coord = alwan_lut3d_index_to_rgb_v(i, size);

        /* Source EOTF: encoded -> linear */
        alwan_vec3 lin = {{eotf_fn(coord.v[0]), eotf_fn(coord.v[1]), eotf_fn(coord.v[2])}};

        /* Combined matrix: src linear -> dst linear */
        alwan_vec3 dst_lin = alwan_mat3_mulv_v(combined, lin);

        /* Destination OETF: linear -> encoded */
        out[i * 3 + 0] = oetf_fn(dst_lin.v[0]);
        out[i * 3 + 1] = oetf_fn(dst_lin.v[1]);
        out[i * 3 + 2] = oetf_fn(dst_lin.v[2]);
    }

    return ALWAN_OK;
}

int alwan_bake_3dlut_view(alwan_scalar *out, int size,
                          alwan_ctx *ctx,
                          alwan_rgb_space_desc const *src_space,
                          alwan_rgb_space_desc const *dst_space,
                          alwan_view_transform vt) {
    if (!out || !src_space || !dst_space || size < 2 || size > 256) {
        return ALWAN_E_INVALID;
    }

    /* Build combined conversion matrix */
    alwan_mat3x3 combined;
    int status = lut__build_combined_matrix(&combined, ctx, src_space, dst_space);
    if (status != ALWAN_OK) return status;

    /* Resolve transfer functions */
    alwan_scalar (*eotf_fn)(alwan_scalar) = lut__resolve_eotf(src_space->eotf);
    alwan_scalar (*oetf_fn)(alwan_scalar) = lut__resolve_oetf(dst_space->oetf);
    if (!eotf_fn || !oetf_fn) return ALWAN_E_INVALID;

    /* Sample every point in the cube */
    size_t const total = (size_t)size * (size_t)size * (size_t)size;
    for (size_t i = 0; i < total; i++) {
        alwan_vec3 coord = alwan_lut3d_index_to_rgb_v(i, size);

        /* Source EOTF */
        alwan_vec3 lin = {{eotf_fn(coord.v[0]), eotf_fn(coord.v[1]), eotf_fn(coord.v[2])}};

        /* Combined matrix */
        alwan_vec3 dst_lin = alwan_mat3_mulv_v(combined, lin);

        /* View transform */
        dst_lin = lut__apply_view(vt, dst_lin);

        /* Destination OETF */
        out[i * 3 + 0] = oetf_fn(dst_lin.v[0]);
        out[i * 3 + 1] = oetf_fn(dst_lin.v[1]);
        out[i * 3 + 2] = oetf_fn(dst_lin.v[2]);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 1D LUT Baking
 * ---------------------------------------------------------------- */

int alwan_bake_1dlut(alwan_scalar *out, int size,
                     alwan_transfer_function tf,
                     int encode) {
    if (!out || size < 2 || size > 65536) return ALWAN_E_INVALID;

    alwan_scalar (*fn)(alwan_scalar) = encode
        ? lut__resolve_oetf(tf)
        : lut__resolve_eotf(tf);
    if (!fn) return ALWAN_E_INVALID;

    for (int i = 0; i < size; i++) {
        alwan_scalar t = alwan_lut1d_index_to_val_v((size_t)i, size);
        out[i] = fn(t);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 2D LUT Dimensions
 * ---------------------------------------------------------------- */

void alwan_lut2d_dimensions(int size, int *width, int *height) {
    if (width)  *width  = size * size;
    if (height) *height = size;
}

/* ----------------------------------------------------------------
 * 3D -> 2D flatten (horizontal strip of blue slices)
 *
 * 3D layout: index = B * size * size + G * size + R (R-fastest)
 * 2D layout: pixel(px, py) where px = B * size + R, py = G
 * ---------------------------------------------------------------- */

int alwan_lut3d_to_2d(alwan_scalar *out,
                       alwan_scalar const *lut3d,
                       int size) {
    if (!out || !lut3d || size < 2 || size > 256) return ALWAN_E_INVALID;

    int const w = size * size;

    for (int b = 0; b < size; b++) {
        for (int g = 0; g < size; g++) {
            for (int r = 0; r < size; r++) {
                /* 3D index (R-fastest) */
                size_t idx3d = ((size_t)b * (size_t)size * (size_t)size +
                                (size_t)g * (size_t)size + (size_t)r) * 3;

                /* 2D pixel coordinate */
                int px = b * size + r;
                int py = g;
                size_t idx2d = ((size_t)py * (size_t)w + (size_t)px) * 3;

                out[idx2d + 0] = lut3d[idx3d + 0];
                out[idx2d + 1] = lut3d[idx3d + 1];
                out[idx2d + 2] = lut3d[idx3d + 2];
            }
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 2D -> 3D unflatten
 * ---------------------------------------------------------------- */

int alwan_lut2d_to_3d(alwan_scalar *out,
                       alwan_scalar const *lut2d,
                       int size) {
    if (!out || !lut2d || size < 2 || size > 256) return ALWAN_E_INVALID;

    int const w = size * size;

    for (int b = 0; b < size; b++) {
        for (int g = 0; g < size; g++) {
            for (int r = 0; r < size; r++) {
                int px = b * size + r;
                int py = g;
                size_t idx2d = ((size_t)py * (size_t)w + (size_t)px) * 3;

                size_t idx3d = ((size_t)b * (size_t)size * (size_t)size +
                                (size_t)g * (size_t)size + (size_t)r) * 3;

                out[idx3d + 0] = lut2d[idx2d + 0];
                out[idx3d + 1] = lut2d[idx2d + 1];
                out[idx3d + 2] = lut2d[idx2d + 2];
            }
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Convenience: bake directly into 2D layout
 * ---------------------------------------------------------------- */

int alwan_bake_2dlut(alwan_scalar *out, int size,
                     alwan_ctx *ctx,
                     alwan_rgb_space_desc const *src_space,
                     alwan_rgb_space_desc const *dst_space) {
    if (!out || !src_space || !dst_space || size < 2 || size > 256) {
        return ALWAN_E_INVALID;
    }

    /* Build combined conversion matrix */
    alwan_mat3x3 combined;
    int status = lut__build_combined_matrix(&combined, ctx, src_space, dst_space);
    if (status != ALWAN_OK) return status;

    /* Resolve transfer functions */
    alwan_scalar (*eotf_fn)(alwan_scalar) = lut__resolve_eotf(src_space->eotf);
    alwan_scalar (*oetf_fn)(alwan_scalar) = lut__resolve_oetf(dst_space->oetf);
    if (!eotf_fn || !oetf_fn) return ALWAN_E_INVALID;

    int const w = size * size;
    alwan_scalar const inv = ALWAN_LITERAL(1.0) / (alwan_scalar)(size - 1);

    /* Iterate in 2D output order for cache coherence */
    for (int py = 0; py < size; py++) {           /* G axis */
        for (int px = 0; px < w; px++) {            /* B*size + R */
            int b = px / size;
            int r = px % size;
            int g = py;

            alwan_scalar rv = (alwan_scalar)r * inv;
            alwan_scalar gv = (alwan_scalar)g * inv;
            alwan_scalar bv = (alwan_scalar)b * inv;

            /* Source EOTF */
            alwan_vec3 lin = {{eotf_fn(rv), eotf_fn(gv), eotf_fn(bv)}};

            /* Combined matrix */
            alwan_vec3 dst_lin = alwan_mat3_mulv_v(combined, lin);

            /* Destination OETF */
            size_t idx = ((size_t)py * (size_t)w + (size_t)px) * 3;
            out[idx + 0] = oetf_fn(dst_lin.v[0]);
            out[idx + 1] = oetf_fn(dst_lin.v[1]);
            out[idx + 2] = oetf_fn(dst_lin.v[2]);
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 1D LUT linear sampling (API wrapper)
 * ---------------------------------------------------------------- */

int alwan_lut1d_sample(alwan_scalar *result,
                        alwan_scalar const *lut,
                        alwan_scalar t,
                        int size) {
    if (!result || !lut || size < 2) return ALWAN_E_INVALID;

    *result = alwan_lut1d_sample_v(lut, t, size);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 2D LUT bilinear sampling (flattened 3D strip, API wrapper)
 * ---------------------------------------------------------------- */

int alwan_lut2d_sample(alwan_rgb *result,
                        alwan_scalar const *lut2d,
                        alwan_rgb const *rgb,
                        int size) {
    if (!result || !lut2d || !rgb || size < 2) return ALWAN_E_INVALID;

    alwan_vec3 in = {{rgb->r, rgb->g, rgb->b}};
    alwan_vec3 out = alwan_lut2d_sample_v(lut2d, in, size);
    result->r = out.v[0];
    result->g = out.v[1];
    result->b = out.v[2];
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 3D LUT trilinear sampling (API wrapper)
 * ---------------------------------------------------------------- */

int alwan_lut3d_sample(alwan_rgb *result,
                        alwan_scalar const *lut,
                        alwan_rgb const *rgb,
                        int size) {
    if (!result || !lut || !rgb || size < 2) return ALWAN_E_INVALID;

    alwan_vec3 in = {{rgb->r, rgb->g, rgb->b}};
    alwan_vec3 out = alwan_lut3d_sample_v(lut, in, size);
    result->r = out.v[0];
    result->g = out.v[1];
    result->b = out.v[2];
    return ALWAN_OK;
}
