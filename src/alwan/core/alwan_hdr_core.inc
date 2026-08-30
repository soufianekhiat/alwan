/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only HDR Pipeline Utilities (parameterized)
 * No include guard - included twice via setup/teardown.
 */

/* ================================================================
 * HLG OOTF (Scene-to-Display) per BT.2100-2
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_hlg_ootf)(ALWAN_CORE_RGB E,
                                          ALWAN_CORE_T Lw,
                                          ALWAN_CORE_T gamma_sys) {
    ALWAN_CORE_RGB result;
    ALWAN_CORE_T alpha = Lw;

    /* BT.2100 luma coefficients for BT.2020 */
    ALWAN_CORE_T Ys = ALWAN_CORE_LITERAL(0.2627) * E.r
                    + ALWAN_CORE_LITERAL(0.6780) * E.g
                    + ALWAN_CORE_LITERAL(0.0593) * E.b;

    ALWAN_CORE_T Ys_safe = ALWAN_CORE_SELECT(Ys < ALWAN_CORE_LITERAL(1e-12),
                                         ALWAN_CORE_LITERAL(1e-12), Ys);
    ALWAN_CORE_T factor = alpha * ALWAN_CORE_POW(Ys_safe, gamma_sys - ALWAN_CORE_ONE);

    result.r = factor * E.r;
    result.g = factor * E.g;
    result.b = factor * E.b;
    return result;
}

/* ================================================================
 * HLG Inverse OOTF (Display-to-Scene)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_hlg_ootf_inv)(ALWAN_CORE_RGB Fd,
                                              ALWAN_CORE_T Lw,
                                              ALWAN_CORE_T gamma_sys) {
    ALWAN_CORE_RGB result;
    ALWAN_CORE_T alpha = Lw;

    /* Display-linear luma */
    ALWAN_CORE_T Yd = ALWAN_CORE_LITERAL(0.2627) * Fd.r
                    + ALWAN_CORE_LITERAL(0.6780) * Fd.g
                    + ALWAN_CORE_LITERAL(0.0593) * Fd.b;

    ALWAN_CORE_T Yd_safe = ALWAN_CORE_SELECT(Yd < ALWAN_CORE_LITERAL(1e-12),
                                         ALWAN_CORE_LITERAL(1e-12), Yd);

    ALWAN_CORE_T inv_gamma = ALWAN_CORE_ONE / gamma_sys;
    ALWAN_CORE_T factor = ALWAN_CORE_POW(alpha, -inv_gamma)
                        * ALWAN_CORE_POW(Yd_safe, (ALWAN_CORE_ONE - gamma_sys) / gamma_sys);

    result.r = factor * Fd.r;
    result.g = factor * Fd.g;
    result.b = factor * Fd.b;
    return result;
}

/* ================================================================
 * BT.2408 Reference White Level
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_bt2408_ref_white)(int use_pq) {
    return ALWAN_CORE_SELECT(use_pq,
                        ALWAN_CORE_LITERAL(203.0) / ALWAN_CORE_LITERAL(10000.0),
                        ALWAN_CORE_LITERAL(0.75));
}

/* ================================================================
 * Mirrored Transfer Function Extension
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_tf_mirror)(ALWAN_CORE_T x,
                                             ALWAN_CORE_T tf_abs_x) {
    ALWAN_CORE_T sign_x = ALWAN_CORE_SELECT(x < ALWAN_CORE_ZERO, -ALWAN_CORE_ONE, ALWAN_CORE_ONE);
    return sign_x * tf_abs_x;
}

/* ================================================================
 * BT.2446 Method A: HDR to SDR Tone Mapping
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_bt2446a_forward)(ALWAN_CORE_T Y_hdr,
                                                    ALWAN_CORE_T L_hdr,
                                                    ALWAN_CORE_T L_sdr) {
    ALWAN_CORE_T pHDR = ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(32.0) *
        ALWAN_CORE_POW(L_hdr / ALWAN_CORE_LITERAL(10000.0),
                  ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.4));
    ALWAN_CORE_T pSDR = ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(32.0) *
        ALWAN_CORE_POW(L_sdr / ALWAN_CORE_LITERAL(10000.0),
                  ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.4));

    ALWAN_CORE_T Y_lin = (ALWAN_CORE_POW(pHDR, Y_hdr) - ALWAN_CORE_ONE) /
                          (pHDR - ALWAN_CORE_ONE);

    ALWAN_CORE_T k1 = ALWAN_CORE_LITERAL(0.5) * pSDR / pHDR;
    ALWAN_CORE_T k3 = ALWAN_CORE_ONE;
    ALWAN_CORE_T k2 = k1 + (k3 - k1) * ALWAN_CORE_LITERAL(0.75);

    ALWAN_CORE_T t1 = Y_lin / k1;
    ALWAN_CORE_T region1 = k1 * ALWAN_CORE_LITERAL(0.5) * t1;

    ALWAN_CORE_T t2 = (Y_lin - k1) / (k2 - k1);
    ALWAN_CORE_T region2 = k1 * ALWAN_CORE_LITERAL(0.5) + (k2 - k1) *
        (t2 - ALWAN_CORE_LITERAL(0.25) * t2 * t2);

    ALWAN_CORE_T mapped_mid = k1 * ALWAN_CORE_LITERAL(0.5) + (k2 - k1) * ALWAN_CORE_LITERAL(0.75);
    ALWAN_CORE_T t3 = (Y_lin - k2) / (k3 - k2);
    ALWAN_CORE_T region3 = mapped_mid + (ALWAN_CORE_ONE - mapped_mid) *
        t3 / (ALWAN_CORE_ONE + (ALWAN_CORE_ONE - t3) * ALWAN_CORE_LITERAL(2.0));

    ALWAN_CORE_T Y_mapped = ALWAN_CORE_SELECT(Y_lin <= k1, region1,
                            ALWAN_CORE_SELECT(Y_lin <= k2, region2, region3));

    ALWAN_CORE_T Y_sdr = ALWAN_CORE_LN(ALWAN_CORE_ONE + (pSDR - ALWAN_CORE_ONE) * Y_mapped) /
                          ALWAN_CORE_LN(pSDR);

    return ALWAN_CORE_SATURATE(Y_sdr);
}

/* ================================================================
 * BT.2446 Method A: SDR to HDR Inverse Mapping
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_bt2446a_inverse)(ALWAN_CORE_T Y_sdr,
                                                    ALWAN_CORE_T L_hdr,
                                                    ALWAN_CORE_T L_sdr) {
    ALWAN_CORE_T pHDR = ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(32.0) *
        ALWAN_CORE_POW(L_hdr / ALWAN_CORE_LITERAL(10000.0),
                  ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.4));
    ALWAN_CORE_T pSDR = ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(32.0) *
        ALWAN_CORE_POW(L_sdr / ALWAN_CORE_LITERAL(10000.0),
                  ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.4));

    ALWAN_CORE_T Y_mapped = (ALWAN_CORE_POW(pSDR, Y_sdr) - ALWAN_CORE_ONE) /
                             (pSDR - ALWAN_CORE_ONE);

    ALWAN_CORE_T k1 = ALWAN_CORE_LITERAL(0.5) * pSDR / pHDR;
    ALWAN_CORE_T k2_minus_k1 = (ALWAN_CORE_ONE - k1) * ALWAN_CORE_LITERAL(0.75);
    ALWAN_CORE_T k2 = k1 + k2_minus_k1;
    ALWAN_CORE_T mapped_k1 = k1 * ALWAN_CORE_LITERAL(0.5);
    ALWAN_CORE_T mapped_k2 = mapped_k1 + k2_minus_k1 * ALWAN_CORE_LITERAL(0.75);

    ALWAN_CORE_T inv_region1 = Y_mapped / ALWAN_CORE_LITERAL(0.5);

    ALWAN_CORE_T b_coeff = k2_minus_k1;
    ALWAN_CORE_T c_coeff = -ALWAN_CORE_LITERAL(0.25) * k2_minus_k1;
    ALWAN_CORE_T disc = b_coeff * b_coeff + ALWAN_CORE_LITERAL(4.0) * c_coeff * (Y_mapped - mapped_k1);
    ALWAN_CORE_T disc_safe = ALWAN_CORE_SELECT(disc < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, disc);
    ALWAN_CORE_T t2 = (-b_coeff + ALWAN_CORE_SQRT(disc_safe)) / (ALWAN_CORE_LITERAL(2.0) * c_coeff);
    ALWAN_CORE_T inv_region2 = k1 + t2 * k2_minus_k1;

    ALWAN_CORE_T rest = ALWAN_CORE_ONE - mapped_k2;
    ALWAN_CORE_T y_norm = (Y_mapped - mapped_k2) / ALWAN_CORE_SELECT(rest < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(1e-10), rest);
    ALWAN_CORE_T t3 = y_norm / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(2.0) * (ALWAN_CORE_ONE - y_norm));
    ALWAN_CORE_T k3_minus_k2 = ALWAN_CORE_ONE - k2;
    ALWAN_CORE_T inv_region3 = k2 + t3 * k3_minus_k2;

    ALWAN_CORE_T Y_lin = ALWAN_CORE_SELECT(Y_mapped <= mapped_k1, inv_region1,
                         ALWAN_CORE_SELECT(Y_mapped <= mapped_k2, inv_region2, inv_region3));

    ALWAN_CORE_T Y_hdr = ALWAN_CORE_LN(ALWAN_CORE_ONE + (pHDR - ALWAN_CORE_ONE) * Y_lin) /
                          ALWAN_CORE_LN(pHDR);

    return ALWAN_CORE_SATURATE(Y_hdr);
}

/* ================================================================
 * BT.2446 Method B: SDR to HDR Up-Conversion
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_bt2446b_forward)(ALWAN_CORE_T Y_sdr,
                                                    ALWAN_CORE_T L_hdr,
                                                    ALWAN_CORE_T L_sdr) {
    ALWAN_CORE_T Y_lin = ALWAN_CORE_POW(ALWAN_CORE_SATURATE(Y_sdr), ALWAN_CORE_LITERAL(2.4));

    ALWAN_CORE_T pHDR = ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(32.0) *
        ALWAN_CORE_POW(L_hdr / ALWAN_CORE_LITERAL(10000.0),
                  ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.4));
    ALWAN_CORE_T pSDR = ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(32.0) *
        ALWAN_CORE_POW(L_sdr / ALWAN_CORE_LITERAL(10000.0),
                  ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.4));

    ALWAN_CORE_T alpha = ALWAN_CORE_LN(pSDR) / ALWAN_CORE_LN(pHDR);

    ALWAN_CORE_T Y_expanded = ALWAN_CORE_POW(Y_lin, alpha);

    ALWAN_CORE_T Y_hdr = ALWAN_CORE_LN(ALWAN_CORE_ONE + (pHDR - ALWAN_CORE_ONE) * Y_expanded) /
                          ALWAN_CORE_LN(pHDR);

    return ALWAN_CORE_SATURATE(Y_hdr);
}

/* ================================================================
 * BT.2446 Method C: HDR to SDR Tone Mapping (Quantization-Aware)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_bt2446c_forward)(ALWAN_CORE_T Y_hdr,
                                                    ALWAN_CORE_T L_hdr,
                                                    ALWAN_CORE_T L_sdr) {
    ALWAN_CORE_T rho = ALWAN_CORE_LITERAL(1.0) +
        ALWAN_CORE_LITERAL(32.0) * ALWAN_CORE_POW(L_hdr / ALWAN_CORE_LITERAL(10000.0),
                                         ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.4));
    ALWAN_CORE_T rho_sdr = ALWAN_CORE_LITERAL(1.0) +
        ALWAN_CORE_LITERAL(32.0) * ALWAN_CORE_POW(L_sdr / ALWAN_CORE_LITERAL(10000.0),
                                         ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.4));

    ALWAN_CORE_T Y_lin = (ALWAN_CORE_POW(rho, Y_hdr) - ALWAN_CORE_ONE) / (rho - ALWAN_CORE_ONE);

    ALWAN_CORE_T alpha = ALWAN_CORE_LN(rho_sdr) / ALWAN_CORE_LN(rho);

    ALWAN_CORE_T Y_compressed = ALWAN_CORE_POW(Y_lin, alpha);

    ALWAN_CORE_T shoulder = ALWAN_CORE_LITERAL(0.95);
    ALWAN_CORE_T t = (Y_compressed - shoulder) / (ALWAN_CORE_ONE - shoulder);
    t = ALWAN_CORE_SELECT(t < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, t);
    ALWAN_CORE_T soft = shoulder + (ALWAN_CORE_ONE - shoulder) *
                        t / (ALWAN_CORE_ONE + t);
    Y_compressed = ALWAN_CORE_SELECT(Y_compressed > shoulder, soft, Y_compressed);

    ALWAN_CORE_T Y_sdr = ALWAN_CORE_LN(ALWAN_CORE_ONE + (rho_sdr - ALWAN_CORE_ONE) * Y_compressed) /
                          ALWAN_CORE_LN(rho_sdr);

    return ALWAN_CORE_SATURATE(Y_sdr);
}

/* ================================================================
 * BT.2390 EETF (Electro-Electro Transfer Function)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_bt2390_eetf)(ALWAN_CORE_T E_pq,
                                                ALWAN_CORE_T LB,
                                                ALWAN_CORE_T LW,
                                                ALWAN_CORE_T LB_target,
                                                ALWAN_CORE_T LW_target) {
    ALWAN_CORE_T range = LW - LB;
    range = ALWAN_CORE_SELECT(range < ALWAN_CORE_LITERAL(1e-10),
                         ALWAN_CORE_LITERAL(1e-10), range);
    ALWAN_CORE_T E_norm = (E_pq - LB) / range;
    E_norm = ALWAN_CORE_CLAMP(E_norm, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);

    ALWAN_CORE_T target_range = LW_target - LB_target;
    ALWAN_CORE_T KS = ALWAN_CORE_LITERAL(1.5) * target_range / range - ALWAN_CORE_LITERAL(0.5);
    KS = ALWAN_CORE_CLAMP(KS, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);

    ALWAN_CORE_T t = (E_norm - KS) / (ALWAN_CORE_ONE - KS + ALWAN_CORE_LITERAL(1e-10));
    t = ALWAN_CORE_CLAMP(t, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);

    ALWAN_CORE_T t2 = t * t;
    ALWAN_CORE_T t3 = t2 * t;

    ALWAN_CORE_T max_mapped = target_range / range;
    max_mapped = ALWAN_CORE_SELECT(max_mapped > ALWAN_CORE_ONE, ALWAN_CORE_ONE, max_mapped);

    ALWAN_CORE_T P = (ALWAN_CORE_LITERAL(2.0) * t3 - ALWAN_CORE_LITERAL(3.0) * t2 + ALWAN_CORE_ONE) * KS
                   + (t3 - ALWAN_CORE_LITERAL(2.0) * t2 + t) * (ALWAN_CORE_ONE - KS)
                   + (-ALWAN_CORE_LITERAL(2.0) * t3 + ALWAN_CORE_LITERAL(3.0) * t2) * max_mapped;

    ALWAN_CORE_T E_mapped = ALWAN_CORE_SELECT(E_norm <= KS, E_norm, P);

    return LB_target + E_mapped * range;
}

/* Convenience: BT.2390 EETF with luminance (cd/m2) parameters. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_bt2390_eetf_luminance)(ALWAN_CORE_T E_pq,
                                                          ALWAN_CORE_T L_source_peak,
                                                          ALWAN_CORE_T L_target_peak) {
    ALWAN_CORE_T LW_src = ALWAN_CORE_FNLIT(alwan_pq_oetf)(L_source_peak);
    ALWAN_CORE_T LW_tgt = ALWAN_CORE_FNLIT(alwan_pq_oetf)(L_target_peak);
    return ALWAN_CORE_FNVLIT(alwan_bt2390_eetf)(E_pq, ALWAN_CORE_ZERO, LW_src, ALWAN_CORE_ZERO, LW_tgt);
}

/* ================================================================
 * Exposure-Based Tone Mapping with Shoulder Compression
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_exposure_tonemap)(ALWAN_CORE_T L,
                                                     ALWAN_CORE_T exposure) {
    ALWAN_CORE_T gain = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), exposure);
    return ALWAN_CORE_ONE - ALWAN_CORE_EXP(-gain * L);
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_exposure_tonemap_rgb)(ALWAN_CORE_VEC3 rgb,
                                                       ALWAN_CORE_T exposure) {
    ALWAN_CORE_VEC3 result;
    ALWAN_CORE_T gain = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), exposure);
    result.v[0] = ALWAN_CORE_ONE - ALWAN_CORE_EXP(-gain * rgb.v[0]);
    result.v[1] = ALWAN_CORE_ONE - ALWAN_CORE_EXP(-gain * rgb.v[1]);
    result.v[2] = ALWAN_CORE_ONE - ALWAN_CORE_EXP(-gain * rgb.v[2]);
    return result;
}

/* ================================================================
 * Reinhard Calibrated Tone Mapping
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_reinhard_calibrated)(ALWAN_CORE_T L,
                                                        ALWAN_CORE_T key,
                                                        ALWAN_CORE_T L_avg,
                                                        ALWAN_CORE_T L_white) {
    ALWAN_CORE_T L_avg_safe = ALWAN_CORE_SELECT(L_avg < ALWAN_CORE_LITERAL(1e-10),
                                            ALWAN_CORE_LITERAL(1e-10), L_avg);
    ALWAN_CORE_T L_scaled = (key / L_avg_safe) * L;
    ALWAN_CORE_T Lw2 = L_white * L_white;
    return L_scaled * (ALWAN_CORE_ONE + L_scaled / Lw2) / (ALWAN_CORE_ONE + L_scaled);
}

/* ================================================================
 * HDR Gamut Mapping: Chroma Compression in JzCzhz
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_gamut_compress_chroma)(ALWAN_CORE_T Cz,
                                                         ALWAN_CORE_T Cz_max) {
    ALWAN_CORE_T ratio = Cz / ALWAN_CORE_SELECT(Cz_max < ALWAN_CORE_LITERAL(1e-10),
                                             ALWAN_CORE_LITERAL(1e-10), Cz_max);

    ALWAN_CORE_T excess = ratio - ALWAN_CORE_ONE;
    ALWAN_CORE_T compressed = ALWAN_CORE_ONE + ALWAN_CORE_TANH(excess) *
                               ALWAN_CORE_LITERAL(0.1);
    ALWAN_CORE_T Cz_out = ALWAN_CORE_SELECT(ratio <= ALWAN_CORE_ONE,
                                        Cz,
                                        compressed * Cz_max);
    return Cz_out;
}

ALWAN_INLINE ALWAN_CORE_JZCZHZ ALWAN_CORE_FNVLIT(alwan_hdr_gamut_map_jzczhz)(ALWAN_CORE_JZCZHZ src,
                                                          ALWAN_CORE_T Cz_max) {
    ALWAN_CORE_JZCZHZ result;
    result.Jz = src.Jz;
    result.hz = src.hz;
    result.Cz = ALWAN_CORE_FNVLIT(alwan_gamut_compress_chroma)(src.Cz, Cz_max);
    return result;
}

/* ================================================================
 * Peak Luminance Normalization for PQ
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_pq_normalize_peak)(ALWAN_CORE_T pq_value,
                                                      ALWAN_CORE_T display_peak) {
    ALWAN_CORE_T L_abs = ALWAN_CORE_FNLIT(alwan_pq_eotf)(pq_value);

    ALWAN_CORE_T L_scaled = ALWAN_CORE_SELECT(L_abs > display_peak,
                                          display_peak, L_abs);

    return ALWAN_CORE_FNLIT(alwan_pq_oetf)(L_scaled);
}

/* ================================================================
 * ST.2086 Metadata Helpers
 * ================================================================ */

ALWAN_INLINE void ALWAN_CORE_FNVLIT(alwan_st2086_init)(ALWAN_CORE_T display_primaries_xy[6],
                                       ALWAN_CORE_T white_point_xy[2],
                                       ALWAN_CORE_T max_luminance,
                                       ALWAN_CORE_T min_luminance,
                                       ALWAN_CORE_T *out_primaries_xy,
                                       ALWAN_CORE_T *out_white_xy,
                                       ALWAN_CORE_T *out_max_lum,
                                       ALWAN_CORE_T *out_min_lum) {
    out_primaries_xy[0] = display_primaries_xy[0];
    out_primaries_xy[1] = display_primaries_xy[1];
    out_primaries_xy[2] = display_primaries_xy[2];
    out_primaries_xy[3] = display_primaries_xy[3];
    out_primaries_xy[4] = display_primaries_xy[4];
    out_primaries_xy[5] = display_primaries_xy[5];
    out_white_xy[0] = white_point_xy[0];
    out_white_xy[1] = white_point_xy[1];
    *out_max_lum = max_luminance;
    *out_min_lum = min_luminance;
}

/* Compute MaxCLL from linear RGB pixel data (absolute luminance, cd/m2) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_content_light_level)(ALWAN_CORE_T const *rgb_data,
                                                        size_t count,
                                                        size_t stride_bytes) {
    ALWAN_CORE_T max_val = ALWAN_CORE_ZERO;
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T const *p = (ALWAN_CORE_T const *)((char const *)rgb_data + i * stride_bytes);
        ALWAN_CORE_T px_max = p[0];
        px_max = ALWAN_CORE_SELECT(p[1] > px_max, p[1], px_max);
        px_max = ALWAN_CORE_SELECT(p[2] > px_max, p[2], px_max);
        max_val = ALWAN_CORE_SELECT(px_max > max_val, px_max, max_val);
    }
    return max_val;
}
