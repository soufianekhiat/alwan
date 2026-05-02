/* No include guard — included twice (f32 and f64 passes) */

void ALWAN_CORE_FNLIT(alwan_hlg_ootf)(ALWAN_CORE_RGB *out, ALWAN_CORE_RGB const *in,
                                      ALWAN_CORE_T Lw, ALWAN_CORE_T gamma_sys) {
    if (!out || !in) return;
    *out = ALWAN_CORE_FNVLIT(alwan_hlg_ootf)(*in, Lw, gamma_sys);
}

void ALWAN_CORE_FNLIT(alwan_hlg_ootf_inv)(ALWAN_CORE_RGB *out, ALWAN_CORE_RGB const *in,
                                          ALWAN_CORE_T Lw, ALWAN_CORE_T gamma_sys) {
    if (!out || !in) return;
    *out = ALWAN_CORE_FNVLIT(alwan_hlg_ootf_inv)(*in, Lw, gamma_sys);
}

void ALWAN_CORE_FNLIT(alwan_hdr_gamut_map_jzczhz)(ALWAN_CORE_JZCZHZ *out, ALWAN_CORE_JZCZHZ const *in,
                                                   ALWAN_CORE_T Cz_max) {
    if (!out || !in) return;
    *out = ALWAN_CORE_FNVLIT(alwan_hdr_gamut_map_jzczhz)(*in, Cz_max);
}

/* ----------------------------------------------------------------
 * MaxCLL - Maximum Content Light Level
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_maxcll)(ALWAN_CORE_T *maxcll_out,
                  ALWAN_CORE_T const *rgb_in, size_t stride, size_t count) {
    if (!maxcll_out || !rgb_in) return ALWAN_E_INVALID;
    if (count == 0) {
        *maxcll_out = ALWAN_CORE_LITERAL(0.0);
        return ALWAN_OK;
    }

    ALWAN_CORE_T max_val = ALWAN_CORE_LITERAL(0.0);
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T const *ptr = (ALWAN_CORE_T const *)((char const *)rgb_in + i * stride);
        /* MaxCLL is max of R, G, B across all pixels */
        ALWAN_CORE_T pixel_max = ptr[0];
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

int ALWAN_CORE_FNLIT(alwan_maxfall)(ALWAN_CORE_T *maxfall_out,
                   ALWAN_CORE_T const *rgb_in, size_t stride, size_t count) {
    if (!maxfall_out || !rgb_in) return ALWAN_E_INVALID;
    if (count == 0) {
        *maxfall_out = ALWAN_CORE_LITERAL(0.0);
        return ALWAN_OK;
    }

    ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T const *ptr = (ALWAN_CORE_T const *)((char const *)rgb_in + i * stride);
        /* Frame average: mean of max(R,G,B) per pixel */
        ALWAN_CORE_T pixel_max = ptr[0];
        if (ptr[1] > pixel_max) pixel_max = ptr[1];
        if (ptr[2] > pixel_max) pixel_max = ptr[2];
        sum += pixel_max;
    }
    *maxfall_out = sum / (ALWAN_CORE_T)count;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * BT.2446 Method B: SDR to HDR
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_bt2446b_forward)(ALWAN_CORE_T *Y_hdr_out, ALWAN_CORE_T Y_sdr,
                            ALWAN_CORE_T L_hdr, ALWAN_CORE_T L_sdr) {
    if (!Y_hdr_out) return ALWAN_E_INVALID;
    *Y_hdr_out = ALWAN_CORE_FNVLIT(alwan_bt2446b_forward)(Y_sdr, L_hdr, L_sdr);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * BT.2446 Method C: HDR to SDR
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_bt2446c_forward)(ALWAN_CORE_T *Y_sdr_out, ALWAN_CORE_T Y_hdr,
                            ALWAN_CORE_T L_hdr, ALWAN_CORE_T L_sdr) {
    if (!Y_sdr_out) return ALWAN_E_INVALID;
    *Y_sdr_out = ALWAN_CORE_FNVLIT(alwan_bt2446c_forward)(Y_hdr, L_hdr, L_sdr);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * BT.2390 EETF
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_bt2390_eetf)(ALWAN_CORE_T *E_out, ALWAN_CORE_T E_pq,
                       ALWAN_CORE_T LB, ALWAN_CORE_T LW,
                       ALWAN_CORE_T LB_target, ALWAN_CORE_T LW_target) {
    if (!E_out) return ALWAN_E_INVALID;
    *E_out = ALWAN_CORE_FNVLIT(alwan_bt2390_eetf)(E_pq, LB, LW, LB_target, LW_target);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_bt2390_eetf_luminance)(ALWAN_CORE_T *E_out, ALWAN_CORE_T E_pq,
                                 ALWAN_CORE_T L_source_peak,
                                 ALWAN_CORE_T L_target_peak) {
    if (!E_out) return ALWAN_E_INVALID;
    *E_out = ALWAN_CORE_FNVLIT(alwan_bt2390_eetf_luminance)(E_pq, L_source_peak, L_target_peak);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Exposure-Based Tone Mapping
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_exposure_tonemap)(ALWAN_CORE_T *out, ALWAN_CORE_T L,
                            ALWAN_CORE_T exposure) {
    if (!out) return ALWAN_E_INVALID;
    *out = ALWAN_CORE_FNVLIT(alwan_exposure_tonemap)(L, exposure);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Reinhard Calibrated
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_reinhard_calibrated)(ALWAN_CORE_T *out, ALWAN_CORE_T L,
                               ALWAN_CORE_T key, ALWAN_CORE_T L_avg,
                               ALWAN_CORE_T L_white) {
    if (!out) return ALWAN_E_INVALID;
    *out = ALWAN_CORE_FNVLIT(alwan_reinhard_calibrated)(L, key, L_avg, L_white);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Display Characterization
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_pq_normalize_peak)(ALWAN_CORE_T *pq_out, ALWAN_CORE_T pq_value,
                              ALWAN_CORE_T display_peak) {
    if (!pq_out) return ALWAN_E_INVALID;
    *pq_out = ALWAN_CORE_FNVLIT(alwan_pq_normalize_peak)(pq_value, display_peak);
    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_st2086_init)(ALWAN_CORE_ST2086_METADATA *meta,
                       ALWAN_CORE_T const display_primaries_xy[6],
                       ALWAN_CORE_T const white_point_xy[2],
                       ALWAN_CORE_T max_luminance,
                       ALWAN_CORE_T min_luminance) {
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

int ALWAN_CORE_FNLIT(alwan_content_light_level_compute)(ALWAN_CORE_CONTENT_LIGHT_LEVEL *cll_out,
                                       ALWAN_CORE_T const *rgb_in,
                                       size_t stride,
                                       size_t count) {
    if (!cll_out || !rgb_in) return ALWAN_E_INVALID;
    if (count == 0) {
        cll_out->max_cll = ALWAN_CORE_LITERAL(0.0);
        cll_out->max_fall = ALWAN_CORE_LITERAL(0.0);
        return ALWAN_OK;
    }

    ALWAN_CORE_T max_val = ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T const *ptr = (ALWAN_CORE_T const *)((char const *)rgb_in + i * stride);
        ALWAN_CORE_T pixel_max = ptr[0];
        if (ptr[1] > pixel_max) pixel_max = ptr[1];
        if (ptr[2] > pixel_max) pixel_max = ptr[2];
        if (pixel_max > max_val) max_val = pixel_max;
        sum += pixel_max;
    }
    cll_out->max_cll = max_val;
    cll_out->max_fall = sum / (ALWAN_CORE_T)count;
    return ALWAN_OK;
}
