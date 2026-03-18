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

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_hdr_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_hdr_impl.inc"
#include "alwan_api_teardown.h"

/* ----------------------------------------------------------------
 * MaxCLL - Maximum Content Light Level
 * ---------------------------------------------------------------- */

int alwan_maxcll(alwan_f64 *maxcll_out,
                  alwan_f64 const *rgb_data,
                  size_t count,
                  size_t stride) {
    if (!maxcll_out || !rgb_data) return ALWAN_E_INVALID;
    if (count == 0) {
        *maxcll_out = ALWAN_LITERAL(0.0);
        return ALWAN_OK;
    }

    alwan_f64 max_val = ALWAN_LITERAL(0.0);
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *ptr = (alwan_f64 const *)((char const *)rgb_data + i * stride);
        /* MaxCLL is max of R, G, B across all pixels */
        alwan_f64 pixel_max = ptr[0];
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

int alwan_maxfall(alwan_f64 *maxfall_out,
                   alwan_f64 const *rgb_data,
                   size_t count,
                   size_t stride) {
    if (!maxfall_out || !rgb_data) return ALWAN_E_INVALID;
    if (count == 0) {
        *maxfall_out = ALWAN_LITERAL(0.0);
        return ALWAN_OK;
    }

    alwan_f64 sum = ALWAN_LITERAL(0.0);
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *ptr = (alwan_f64 const *)((char const *)rgb_data + i * stride);
        /* Frame average: mean of max(R,G,B) per pixel */
        alwan_f64 pixel_max = ptr[0];
        if (ptr[1] > pixel_max) pixel_max = ptr[1];
        if (ptr[2] > pixel_max) pixel_max = ptr[2];
        sum += pixel_max;
    }
    *maxfall_out = sum / (alwan_f64)count;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * BT.2446 Method B: SDR to HDR
 * ---------------------------------------------------------------- */

int alwan_bt2446b_forward(alwan_f64 *Y_hdr_out, alwan_f64 Y_sdr,
                            alwan_f64 L_hdr, alwan_f64 L_sdr) {
    if (!Y_hdr_out) return ALWAN_E_INVALID;
    *Y_hdr_out = alwan_bt2446b_forward_f64_v(Y_sdr, L_hdr, L_sdr);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * BT.2446 Method C: HDR to SDR
 * ---------------------------------------------------------------- */

int alwan_bt2446c_forward(alwan_f64 *Y_sdr_out, alwan_f64 Y_hdr,
                            alwan_f64 L_hdr, alwan_f64 L_sdr) {
    if (!Y_sdr_out) return ALWAN_E_INVALID;
    *Y_sdr_out = alwan_bt2446c_forward_f64_v(Y_hdr, L_hdr, L_sdr);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * BT.2390 EETF
 * ---------------------------------------------------------------- */

int alwan_bt2390_eetf(alwan_f64 *E_out, alwan_f64 E_pq,
                       alwan_f64 LB, alwan_f64 LW,
                       alwan_f64 LB_target, alwan_f64 LW_target) {
    if (!E_out) return ALWAN_E_INVALID;
    *E_out = alwan_bt2390_eetf_f64_v(E_pq, LB, LW, LB_target, LW_target);
    return ALWAN_OK;
}

int alwan_bt2390_eetf_luminance(alwan_f64 *E_out, alwan_f64 E_pq,
                                 alwan_f64 L_source_peak,
                                 alwan_f64 L_target_peak) {
    if (!E_out) return ALWAN_E_INVALID;
    *E_out = alwan_bt2390_eetf_luminance_f64_v(E_pq, L_source_peak, L_target_peak);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Exposure-Based Tone Mapping
 * ---------------------------------------------------------------- */

int alwan_exposure_tonemap(alwan_f64 *out, alwan_f64 L,
                            alwan_f64 exposure) {
    if (!out) return ALWAN_E_INVALID;
    *out = alwan_exposure_tonemap_f64_v(L, exposure);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Reinhard Calibrated
 * ---------------------------------------------------------------- */

int alwan_reinhard_calibrated(alwan_f64 *out, alwan_f64 L,
                               alwan_f64 key, alwan_f64 L_avg,
                               alwan_f64 L_white) {
    if (!out) return ALWAN_E_INVALID;
    *out = alwan_reinhard_calibrated_f64_v(L, key, L_avg, L_white);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Display Characterization
 * ---------------------------------------------------------------- */

int alwan_pq_normalize_peak(alwan_f64 *pq_out, alwan_f64 pq_value,
                              alwan_f64 display_peak) {
    if (!pq_out) return ALWAN_E_INVALID;
    *pq_out = alwan_pq_normalize_peak_f64_v(pq_value, display_peak);
    return ALWAN_OK;
}

int alwan_st2086_init(alwan_st2086_metadata *meta,
                       alwan_f64 const display_primaries_xy[6],
                       alwan_f64 const white_point_xy[2],
                       alwan_f64 max_luminance,
                       alwan_f64 min_luminance) {
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
                                       alwan_f64 const *rgb_data,
                                       size_t count,
                                       size_t stride) {
    if (!cll_out || !rgb_data) return ALWAN_E_INVALID;
    if (count == 0) {
        cll_out->max_cll = ALWAN_LITERAL(0.0);
        cll_out->max_fall = ALWAN_LITERAL(0.0);
        return ALWAN_OK;
    }

    alwan_f64 max_val = ALWAN_LITERAL(0.0);
    alwan_f64 sum = ALWAN_LITERAL(0.0);
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *ptr = (alwan_f64 const *)((char const *)rgb_data + i * stride);
        alwan_f64 pixel_max = ptr[0];
        if (ptr[1] > pixel_max) pixel_max = ptr[1];
        if (ptr[2] > pixel_max) pixel_max = ptr[2];
        if (pixel_max > max_val) max_val = pixel_max;
        sum += pixel_max;
    }
    cll_out->max_cll = max_val;
    cll_out->max_fall = sum / (alwan_f64)count;
    return ALWAN_OK;
}
