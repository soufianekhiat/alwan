/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * HDR Pipeline Utilities
 * Per-pixel math in alwan_hdr_core.h
 *
 * HLG OOTF, MaxCLL, MaxFALL, BT.2408 reference white.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_hdr_core.h"

/* ----------------------------------------------------------------
 * HLG OOTF
 * ---------------------------------------------------------------- */

int alwan_hlg_ootf(alwan_rgb *out, alwan_rgb const *in,
                    alwan_scalar Lw, alwan_scalar gamma_sys) {
    if (!out || !in) return ALWAN_E_INVALID;
    *out = alwan_hlg_ootf_v(*in, Lw, gamma_sys);
    return ALWAN_OK;
}

int alwan_hlg_ootf_inv(alwan_rgb *out, alwan_rgb const *in,
                        alwan_scalar Lw, alwan_scalar gamma_sys) {
    if (!out || !in) return ALWAN_E_INVALID;
    *out = alwan_hlg_ootf_inv_v(*in, Lw, gamma_sys);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * MaxCLL - Maximum Content Light Level
 * ---------------------------------------------------------------- */

int alwan_maxcll(alwan_scalar *maxcll_out,
                  alwan_scalar const *rgb_data,
                  size_t count,
                  size_t stride) {
    if (!maxcll_out || !rgb_data) return ALWAN_E_INVALID;
    if (count == 0) {
        *maxcll_out = ALWAN_LITERAL(0.0);
        return ALWAN_OK;
    }

    alwan_scalar max_val = ALWAN_LITERAL(0.0);
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *ptr = (alwan_scalar const *)((char const *)rgb_data + i * stride);
        /* MaxCLL is max of R, G, B across all pixels */
        alwan_scalar pixel_max = ptr[0];
        if (ptr[1] > pixel_max) pixel_max = ptr[1];
        if (ptr[2] > pixel_max) pixel_max = ptr[2];
        if (pixel_max > max_val) max_val = pixel_max;
    }
    *maxcll_out = max_val;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * MaxFALL - Maximum Frame Average Light Level
 * ---------------------------------------------------------------- */

int alwan_maxfall(alwan_scalar *maxfall_out,
                   alwan_scalar const *rgb_data,
                   size_t count,
                   size_t stride) {
    if (!maxfall_out || !rgb_data) return ALWAN_E_INVALID;
    if (count == 0) {
        *maxfall_out = ALWAN_LITERAL(0.0);
        return ALWAN_OK;
    }

    alwan_scalar sum = ALWAN_LITERAL(0.0);
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *ptr = (alwan_scalar const *)((char const *)rgb_data + i * stride);
        /* Frame average: mean of max(R,G,B) per pixel */
        alwan_scalar pixel_max = ptr[0];
        if (ptr[1] > pixel_max) pixel_max = ptr[1];
        if (ptr[2] > pixel_max) pixel_max = ptr[2];
        sum += pixel_max;
    }
    *maxfall_out = sum / (alwan_scalar)count;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * BT.2446 Method B: SDR to HDR
 * ---------------------------------------------------------------- */

int alwan_bt2446b_forward(alwan_scalar *Y_hdr_out, alwan_scalar Y_sdr,
                            alwan_scalar L_hdr, alwan_scalar L_sdr) {
    if (!Y_hdr_out) return ALWAN_E_INVALID;
    *Y_hdr_out = alwan_bt2446b_forward_v(Y_sdr, L_hdr, L_sdr);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * BT.2446 Method C: HDR to SDR
 * ---------------------------------------------------------------- */

int alwan_bt2446c_forward(alwan_scalar *Y_sdr_out, alwan_scalar Y_hdr,
                            alwan_scalar L_hdr, alwan_scalar L_sdr) {
    if (!Y_sdr_out) return ALWAN_E_INVALID;
    *Y_sdr_out = alwan_bt2446c_forward_v(Y_hdr, L_hdr, L_sdr);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * BT.2390 EETF
 * ---------------------------------------------------------------- */

int alwan_bt2390_eetf(alwan_scalar *E_out, alwan_scalar E_pq,
                       alwan_scalar LB, alwan_scalar LW,
                       alwan_scalar LB_target, alwan_scalar LW_target) {
    if (!E_out) return ALWAN_E_INVALID;
    *E_out = alwan_bt2390_eetf_v(E_pq, LB, LW, LB_target, LW_target);
    return ALWAN_OK;
}

int alwan_bt2390_eetf_luminance(alwan_scalar *E_out, alwan_scalar E_pq,
                                 alwan_scalar L_source_peak,
                                 alwan_scalar L_target_peak) {
    if (!E_out) return ALWAN_E_INVALID;
    *E_out = alwan_bt2390_eetf_luminance_v(E_pq, L_source_peak, L_target_peak);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Exposure-Based Tone Mapping
 * ---------------------------------------------------------------- */

int alwan_exposure_tonemap(alwan_scalar *out, alwan_scalar L,
                            alwan_scalar exposure) {
    if (!out) return ALWAN_E_INVALID;
    *out = alwan_exposure_tonemap_v(L, exposure);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Reinhard Calibrated
 * ---------------------------------------------------------------- */

int alwan_reinhard_calibrated(alwan_scalar *out, alwan_scalar L,
                               alwan_scalar key, alwan_scalar L_avg,
                               alwan_scalar L_white) {
    if (!out) return ALWAN_E_INVALID;
    *out = alwan_reinhard_calibrated_v(L, key, L_avg, L_white);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * HDR Gamut Mapping
 * ---------------------------------------------------------------- */

int alwan_hdr_gamut_map_jzczhz(alwan_jzczhz *out, alwan_jzczhz const *in,
                                alwan_scalar Cz_max) {
    if (!out || !in) return ALWAN_E_INVALID;
    *out = alwan_hdr_gamut_map_jzczhz_v(*in, Cz_max);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Display Characterization
 * ---------------------------------------------------------------- */

int alwan_pq_normalize_peak(alwan_scalar *pq_out, alwan_scalar pq_value,
                              alwan_scalar display_peak) {
    if (!pq_out) return ALWAN_E_INVALID;
    *pq_out = alwan_pq_normalize_peak_v(pq_value, display_peak);
    return ALWAN_OK;
}

int alwan_st2086_init(alwan_st2086_metadata *meta,
                       alwan_scalar const display_primaries_xy[6],
                       alwan_scalar const white_point_xy[2],
                       alwan_scalar max_luminance,
                       alwan_scalar min_luminance) {
    if (!meta || !display_primaries_xy || !white_point_xy) return ALWAN_E_INVALID;
    meta->display_primaries_xy[0] = display_primaries_xy[0];
    meta->display_primaries_xy[1] = display_primaries_xy[1];
    meta->display_primaries_xy[2] = display_primaries_xy[2];
    meta->display_primaries_xy[3] = display_primaries_xy[3];
    meta->display_primaries_xy[4] = display_primaries_xy[4];
    meta->display_primaries_xy[5] = display_primaries_xy[5];
    meta->white_point_xy[0] = white_point_xy[0];
    meta->white_point_xy[1] = white_point_xy[1];
    meta->max_luminance = max_luminance;
    meta->min_luminance = min_luminance;
    return ALWAN_OK;
}

int alwan_content_light_level_compute(alwan_content_light_level *cll_out,
                                       alwan_scalar const *rgb_data,
                                       size_t count,
                                       size_t stride) {
    if (!cll_out || !rgb_data) return ALWAN_E_INVALID;
    if (count == 0) {
        cll_out->max_cll = ALWAN_LITERAL(0.0);
        cll_out->max_fall = ALWAN_LITERAL(0.0);
        return ALWAN_OK;
    }

    alwan_scalar max_val = ALWAN_LITERAL(0.0);
    alwan_scalar sum = ALWAN_LITERAL(0.0);
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *ptr = (alwan_scalar const *)((char const *)rgb_data + i * stride);
        alwan_scalar pixel_max = ptr[0];
        if (ptr[1] > pixel_max) pixel_max = ptr[1];
        if (ptr[2] > pixel_max) pixel_max = ptr[2];
        if (pixel_max > max_val) max_val = pixel_max;
        sum += pixel_max;
    }
    cll_out->max_cll = max_val;
    cll_out->max_fall = sum / (alwan_scalar)count;
    return ALWAN_OK;
}
