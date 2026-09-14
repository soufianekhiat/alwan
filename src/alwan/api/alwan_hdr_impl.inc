/* No include guard -- included twice (f32 and f64 passes) */

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

alwan_status ALWAN_CORE_FNLIT(alwan_maxcll)(ALWAN_CORE_T *maxcll_out,
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

alwan_status ALWAN_CORE_FNLIT(alwan_maxfall)(ALWAN_CORE_T *maxfall_out,
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

alwan_status ALWAN_CORE_FNLIT(alwan_bt2446b_forward)(ALWAN_CORE_T *Y_hdr_out, ALWAN_CORE_T Y_sdr,
                            ALWAN_CORE_T L_hdr, ALWAN_CORE_T L_sdr) {
    if (!Y_hdr_out) return ALWAN_E_INVALID;
    *Y_hdr_out = ALWAN_CORE_FNVLIT(alwan_bt2446b_forward)(Y_sdr, L_hdr, L_sdr);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * BT.2446 Method C: HDR to SDR
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_bt2446c_forward)(ALWAN_CORE_T *Y_sdr_out, ALWAN_CORE_T Y_hdr,
                            ALWAN_CORE_T L_hdr, ALWAN_CORE_T L_sdr) {
    if (!Y_sdr_out) return ALWAN_E_INVALID;
    *Y_sdr_out = ALWAN_CORE_FNVLIT(alwan_bt2446c_forward)(Y_hdr, L_hdr, L_sdr);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * BT.2390 EETF
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_bt2390_eetf)(ALWAN_CORE_T *E_out, ALWAN_CORE_T E_pq,
                       ALWAN_CORE_T LB, ALWAN_CORE_T LW,
                       ALWAN_CORE_T LB_target, ALWAN_CORE_T LW_target) {
    if (!E_out) return ALWAN_E_INVALID;
    *E_out = ALWAN_CORE_FNVLIT(alwan_bt2390_eetf)(E_pq, LB, LW, LB_target, LW_target);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_bt2390_eetf_luminance)(ALWAN_CORE_T *E_out, ALWAN_CORE_T E_pq,
                                 ALWAN_CORE_T L_source_peak,
                                 ALWAN_CORE_T L_target_peak) {
    if (!E_out) return ALWAN_E_INVALID;
    *E_out = ALWAN_CORE_FNVLIT(alwan_bt2390_eetf_luminance)(E_pq, L_source_peak, L_target_peak);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Exposure-Based Tone Mapping
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_exposure_tonemap)(ALWAN_CORE_T *out, ALWAN_CORE_T L,
                            ALWAN_CORE_T exposure) {
    if (!out) return ALWAN_E_INVALID;
    *out = ALWAN_CORE_FNVLIT(alwan_exposure_tonemap)(L, exposure);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Reinhard Calibrated
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_reinhard_calibrated)(ALWAN_CORE_T *out, ALWAN_CORE_T L,
                               ALWAN_CORE_T key, ALWAN_CORE_T L_avg,
                               ALWAN_CORE_T L_white) {
    if (!out) return ALWAN_E_INVALID;
    *out = ALWAN_CORE_FNVLIT(alwan_reinhard_calibrated)(L, key, L_avg, L_white);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Display Characterization
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_pq_normalize_peak)(ALWAN_CORE_T *pq_out, ALWAN_CORE_T pq_value,
                              ALWAN_CORE_T display_peak) {
    if (!pq_out) return ALWAN_E_INVALID;
    *pq_out = ALWAN_CORE_FNVLIT(alwan_pq_normalize_peak)(pq_value, display_peak);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_st2086_init)(ALWAN_CORE_ST2086_METADATA *meta,
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

alwan_status ALWAN_CORE_FNLIT(alwan_content_light_level_compute)(ALWAN_CORE_CONTENT_LIGHT_LEVEL *cll_out,
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

/* ----------------------------------------------------------------
 * BT.2408: HLG <-> PQ in display light, SDR at HDR reference white
 * ---------------------------------------------------------------- */

#ifndef ALWAN_HDR_MAPNAME
#  define ALWAN_HDR_MAPNAME_(base, sfx) base##sfx##_map_interleave
#  define ALWAN_HDR_MAPNAME(base, sfx)  ALWAN_HDR_MAPNAME_(base, sfx)
#endif

/* BT.2100-2's system gamma for an HLG display of nominal peak Lw: 1.2 at 1000 cd/m2. */
static ALWAN_CORE_T ALWAN_CORE_FN(bt2408_hlg_gamma)(ALWAN_CORE_T peak_nits) {
    return ALWAN_CORE_LITERAL(1.2)
         + ALWAN_CORE_LITERAL(0.42) * ALWAN_CORE_LOG10(peak_nits / ALWAN_CORE_LITERAL(1000.0));
}

/* A luminance argument: 0 takes the default, anything else must be positive. */
static int ALWAN_CORE_FN(bt2408_nits)(ALWAN_CORE_T *out, ALWAN_CORE_T value, ALWAN_CORE_T fallback) {
    if (value == ALWAN_CORE_ZERO) {
        *out = fallback;
        return 1;
    }
    *out = value;
    return value > ALWAN_CORE_ZERO;
}

alwan_status ALWAN_HDR_MAPNAME(alwan_bt2408_hlg_to_pq, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *out, size_t out_stride, ALWAN_CORE_T const *in, size_t in_stride, size_t count,
        ALWAN_CORE_T hlg_peak_nits) {
    ALWAN_CORE_T peak, gamma_sys;
    if (!out || !in || !ALWAN_CORE_FN(bt2408_nits)(&peak, hlg_peak_nits, ALWAN_CORE_LITERAL(1000.0))) {
        return ALWAN_E_INVALID;
    }
    gamma_sys = ALWAN_CORE_FN(bt2408_hlg_gamma)(peak);
    for (size_t p = 0; p < count; p++) {
        ALWAN_CORE_T const *s = (ALWAN_CORE_T const *)((char const *)in + p * in_stride);
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)out + p * out_stride);
        ALWAN_CORE_RGB scene, display;
        scene.r = ALWAN_CORE_FNVLIT(alwan_hlg_inverse_oetf)(s[0]);
        scene.g = ALWAN_CORE_FNVLIT(alwan_hlg_inverse_oetf)(s[1]);
        scene.b = ALWAN_CORE_FNVLIT(alwan_hlg_inverse_oetf)(s[2]);
        display = ALWAN_CORE_FNVLIT(alwan_hlg_ootf)(scene, peak, gamma_sys);
        o[0] = ALWAN_CORE_FNLIT(alwan_pq_oetf)(display.r);
        o[1] = ALWAN_CORE_FNLIT(alwan_pq_oetf)(display.g);
        o[2] = ALWAN_CORE_FNLIT(alwan_pq_oetf)(display.b);
    }
    return ALWAN_OK;
}

alwan_status ALWAN_HDR_MAPNAME(alwan_bt2408_pq_to_hlg, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *out, size_t out_stride, ALWAN_CORE_T const *in, size_t in_stride, size_t count,
        ALWAN_CORE_T hlg_peak_nits, int clip_to_peak) {
    ALWAN_CORE_T peak, gamma_sys;
    if (!out || !in || !ALWAN_CORE_FN(bt2408_nits)(&peak, hlg_peak_nits, ALWAN_CORE_LITERAL(1000.0))) {
        return ALWAN_E_INVALID;
    }
    gamma_sys = ALWAN_CORE_FN(bt2408_hlg_gamma)(peak);
    for (size_t p = 0; p < count; p++) {
        ALWAN_CORE_T const *s = (ALWAN_CORE_T const *)((char const *)in + p * in_stride);
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)out + p * out_stride);
        ALWAN_CORE_RGB scene, display;
        display.r = ALWAN_CORE_FNLIT(alwan_pq_eotf)(s[0]);
        display.g = ALWAN_CORE_FNLIT(alwan_pq_eotf)(s[1]);
        display.b = ALWAN_CORE_FNLIT(alwan_pq_eotf)(s[2]);
        if (clip_to_peak) {
            if (display.r > peak) display.r = peak;
            if (display.g > peak) display.g = peak;
            if (display.b > peak) display.b = peak;
        }
        scene = ALWAN_CORE_FNVLIT(alwan_hlg_ootf_inv)(display, peak, gamma_sys);
        o[0] = ALWAN_CORE_FNLIT(alwan_hlg_oetf)(scene.r);
        o[1] = ALWAN_CORE_FNLIT(alwan_hlg_oetf)(scene.g);
        o[2] = ALWAN_CORE_FNLIT(alwan_hlg_oetf)(scene.b);
    }
    return ALWAN_OK;
}

alwan_status ALWAN_HDR_MAPNAME(alwan_bt2408_sdr_to_pq, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *out, size_t out_stride, ALWAN_CORE_T const *in, size_t in_stride, size_t count,
        ALWAN_CORE_T sdr_white_nits) {
    ALWAN_CORE_T white;
    if (!out || !in || !ALWAN_CORE_FN(bt2408_nits)(&white, sdr_white_nits, ALWAN_CORE_LITERAL(203.0))) {
        return ALWAN_E_INVALID;
    }
    for (size_t p = 0; p < count; p++) {
        ALWAN_CORE_T const *s = (ALWAN_CORE_T const *)((char const *)in + p * in_stride);
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)out + p * out_stride);
        o[0] = ALWAN_CORE_FNLIT(alwan_pq_oetf)(s[0] * white);
        o[1] = ALWAN_CORE_FNLIT(alwan_pq_oetf)(s[1] * white);
        o[2] = ALWAN_CORE_FNLIT(alwan_pq_oetf)(s[2] * white);
    }
    return ALWAN_OK;
}

alwan_status ALWAN_HDR_MAPNAME(alwan_bt2408_sdr_to_hlg, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *out, size_t out_stride, ALWAN_CORE_T const *in, size_t in_stride, size_t count,
        ALWAN_CORE_T sdr_white_nits, ALWAN_CORE_T hlg_peak_nits) {
    ALWAN_CORE_T white, peak, gamma_sys;
    if (!out || !in || !ALWAN_CORE_FN(bt2408_nits)(&white, sdr_white_nits, ALWAN_CORE_LITERAL(203.0))
        || !ALWAN_CORE_FN(bt2408_nits)(&peak, hlg_peak_nits, ALWAN_CORE_LITERAL(1000.0))) {
        return ALWAN_E_INVALID;
    }
    gamma_sys = ALWAN_CORE_FN(bt2408_hlg_gamma)(peak);
    for (size_t p = 0; p < count; p++) {
        ALWAN_CORE_T const *s = (ALWAN_CORE_T const *)((char const *)in + p * in_stride);
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)out + p * out_stride);
        ALWAN_CORE_RGB scene, display;
        display.r = s[0] * white;
        display.g = s[1] * white;
        display.b = s[2] * white;
        scene = ALWAN_CORE_FNVLIT(alwan_hlg_ootf_inv)(display, peak, gamma_sys);
        o[0] = ALWAN_CORE_FNLIT(alwan_hlg_oetf)(scene.r);
        o[1] = ALWAN_CORE_FNLIT(alwan_hlg_oetf)(scene.g);
        o[2] = ALWAN_CORE_FNLIT(alwan_hlg_oetf)(scene.b);
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ISO 21496-1 gain maps
 * ---------------------------------------------------------------- */

/* The params type of this pass: alwan_gain_map_params_f32 or _f64. */
#define ALWAN__GAIN_MAP_PARAMS ALWAN_CORE_FNLIT(alwan_gain_map_params)

/* The gamma a channel is stored with; the zero value reads as 1. */
static ALWAN_CORE_T ALWAN_CORE_FN(gain_map_gamma)(ALWAN_CORE_T g) {
    return g == ALWAN_CORE_ZERO ? ALWAN_CORE_ONE : g;
}

/* Metadata the standard allows: a positive gamma, max >= min, offsets that are numbers. */
static int ALWAN_CORE_FN(gain_map_params_ok)(ALWAN__GAIN_MAP_PARAMS const *p, size_t channels) {
    for (size_t c = 0; c < channels; c++) {
        if (!(ALWAN_CORE_FN(gain_map_gamma)(p->gamma[c]) > ALWAN_CORE_ZERO)) return 0;
        if (!(p->gain_map_max[c] >= p->gain_map_min[c])) return 0;
        if (!(p->base_offset[c] == p->base_offset[c]) || !(p->alternate_offset[c] == p->alternate_offset[c])) {
            return 0;
        }
    }
    return 1;
}

alwan_status ALWAN_CORE_FNLIT(alwan_gain_map_weight)(ALWAN_CORE_T *weight_out, ALWAN__GAIN_MAP_PARAMS const *params,
                                                     ALWAN_CORE_T display_hdr_headroom) {
    if (!weight_out || !params || !(display_hdr_headroom == display_hdr_headroom)) {
        return ALWAN_E_INVALID;
    }
    /* Equal headrooms (or a NaN) describe no range to interpolate over. */
    if (!(params->alternate_hdr_headroom > params->base_hdr_headroom)
        && !(params->alternate_hdr_headroom < params->base_hdr_headroom)) {
        return ALWAN_E_INVALID;
    }
    *weight_out = ALWAN_CORE_FNVLIT(alwan_gain_map_weight)(display_hdr_headroom, params->base_hdr_headroom,
                                                          params->alternate_hdr_headroom);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_gain_map_measure)(ALWAN__GAIN_MAP_PARAMS *params,
                                                      ALWAN_CORE_T const *base, size_t base_stride,
                                                      ALWAN_CORE_T const *alternate, size_t alternate_stride,
                                                      size_t count) {
    ALWAN_CORE_T lo[3] = { ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ZERO };
    ALWAN_CORE_T hi[3] = { ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ZERO };
    int seen[3] = { 0, 0, 0 };
    if (!params || !base || !alternate || count == 0) {
        return ALWAN_E_INVALID;
    }
    for (size_t p = 0; p < count; p++) {
        ALWAN_CORE_T const *b = (ALWAN_CORE_T const *)((char const *)base + p * base_stride);
        ALWAN_CORE_T const *a = (ALWAN_CORE_T const *)((char const *)alternate + p * alternate_stride);
        for (int c = 0; c < 3; c++) {
            ALWAN_CORE_T const num = a[c] + params->alternate_offset[c];
            ALWAN_CORE_T const den = b[c] + params->base_offset[c];
            ALWAN_CORE_T g;
            if (!(num > ALWAN_CORE_ZERO) || !(den > ALWAN_CORE_ZERO)) {
                continue; /* no finite ratio */
            }
            g = ALWAN_CORE_LOG2(num / den);
            if (!seen[c] || g < lo[c]) lo[c] = g;
            if (!seen[c] || g > hi[c]) hi[c] = g;
            seen[c] = 1;
        }
    }
    if (!seen[0] || !seen[1] || !seen[2]) {
        return ALWAN_E_RANGE;
    }
    for (int c = 0; c < 3; c++) {
        params->gain_map_min[c] = lo[c];
        params->gain_map_max[c] = hi[c];
    }
    return ALWAN_OK;
}

alwan_status ALWAN_HDR_MAPNAME(alwan_gain_map_encode, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *gain_out, size_t gain_stride,
        ALWAN_CORE_T const *base, size_t base_stride,
        ALWAN_CORE_T const *alternate, size_t alternate_stride,
        size_t count, ALWAN__GAIN_MAP_PARAMS const *params) {
    if (!gain_out || !base || !alternate || !params || !ALWAN_CORE_FN(gain_map_params_ok)(params, 3)) {
        return ALWAN_E_INVALID;
    }
    for (size_t p = 0; p < count; p++) {
        ALWAN_CORE_T const *b = (ALWAN_CORE_T const *)((char const *)base + p * base_stride);
        ALWAN_CORE_T const *a = (ALWAN_CORE_T const *)((char const *)alternate + p * alternate_stride);
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)gain_out + p * gain_stride);
        for (int c = 0; c < 3; c++) {
            o[c] = ALWAN_CORE_FNVLIT(alwan_gain_map_encode)(b[c], a[c], params->gain_map_min[c],
                                                           params->gain_map_max[c],
                                                           ALWAN_CORE_FN(gain_map_gamma)(params->gamma[c]),
                                                           params->base_offset[c], params->alternate_offset[c]);
        }
    }
    return ALWAN_OK;
}

alwan_status ALWAN_HDR_MAPNAME(alwan_gain_map_apply, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *out, size_t out_stride,
        ALWAN_CORE_T const *base, size_t base_stride,
        ALWAN_CORE_T const *gain, size_t gain_stride, size_t gain_channels,
        size_t count, ALWAN__GAIN_MAP_PARAMS const *params, ALWAN_CORE_T weight) {
    if (!out || !base || !gain || !params || (gain_channels != 1 && gain_channels != 3)
        || !(weight == weight) || !ALWAN_CORE_FN(gain_map_params_ok)(params, gain_channels)) {
        return ALWAN_E_INVALID;
    }
    for (size_t p = 0; p < count; p++) {
        ALWAN_CORE_T const *b = (ALWAN_CORE_T const *)((char const *)base + p * base_stride);
        ALWAN_CORE_T const *g = (ALWAN_CORE_T const *)((char const *)gain + p * gain_stride);
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)out + p * out_stride);
        ALWAN_CORE_T bv[3], gv[3];
        /* Read the pixel before writing it: out may be base, or even the gains. */
        for (int c = 0; c < 3; c++) {
            bv[c] = b[c];
            gv[c] = g[gain_channels == 3 ? c : 0];
        }
        for (int c = 0; c < 3; c++) {
            int const m = gain_channels == 3 ? c : 0;
            o[c] = ALWAN_CORE_FNVLIT(alwan_gain_map_apply)(bv[c], gv[c], params->gain_map_min[m],
                                                          params->gain_map_max[m],
                                                          ALWAN_CORE_FN(gain_map_gamma)(params->gamma[m]),
                                                          params->base_offset[m], params->alternate_offset[m],
                                                          weight);
        }
    }
    return ALWAN_OK;
}

#undef ALWAN__GAIN_MAP_PARAMS
