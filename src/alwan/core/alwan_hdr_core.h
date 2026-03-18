/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only HDR Pipeline Utilities
 * HLG OOTF, BT.2408 reference white, mirrored TF extension.
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_HDR_CORE_H
#define ALWAN_HDR_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_core.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

/* Save and undef backward-compat aliases from alwan_core.h that would
 * interfere with ALWAN_CORE_FN token-pasting inside the .inc */
#ifdef alwan_pq_oetf
#undef alwan_pq_oetf
#endif
#ifdef alwan_pq_eotf
#undef alwan_pq_eotf
#endif

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_hdr_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_hdr_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_INLINE alwan_rgb alwan_hlg_ootf_v(alwan_rgb E,
                                          alwan_scalar Lw,
                                          alwan_scalar gamma_sys) {
    alwan_rgb result;
    alwan_scalar alpha = Lw;

    alwan_scalar Ys = ALWAN_LUMA_KR_BT2020 * E.r
                    + ALWAN_LUMA_KG_BT2020 * E.g
                    + ALWAN_LUMA_KB_BT2020 * E.b;

    alwan_scalar Ys_safe = ALWAN_SELECT(Ys < ALWAN_LITERAL(1e-12),
                                         ALWAN_LITERAL(1e-12), Ys);
    alwan_scalar factor = alpha * ALWAN_POW(Ys_safe, gamma_sys - ALWAN_ONE);

    result.r = factor * E.r;
    result.g = factor * E.g;
    result.b = factor * E.b;
    return result;
}

ALWAN_INLINE alwan_rgb alwan_hlg_ootf_inv_v(alwan_rgb Fd,
                                              alwan_scalar Lw,
                                              alwan_scalar gamma_sys) {
    alwan_rgb result;
    alwan_scalar alpha = Lw;

    alwan_scalar Yd = ALWAN_LUMA_KR_BT2020 * Fd.r
                    + ALWAN_LUMA_KG_BT2020 * Fd.g
                    + ALWAN_LUMA_KB_BT2020 * Fd.b;

    alwan_scalar Yd_safe = ALWAN_SELECT(Yd < ALWAN_LITERAL(1e-12),
                                         ALWAN_LITERAL(1e-12), Yd);

    alwan_scalar inv_gamma = ALWAN_ONE / gamma_sys;
    alwan_scalar factor = ALWAN_POW(alpha, -inv_gamma)
                        * ALWAN_POW(Yd_safe, (ALWAN_ONE - gamma_sys) / gamma_sys);

    result.r = factor * Fd.r;
    result.g = factor * Fd.g;
    result.b = factor * Fd.b;
    return result;
}

ALWAN_INLINE alwan_scalar alwan_bt2408_ref_white_v(int use_pq) {
    return ALWAN_SELECT(use_pq,
                        ALWAN_LITERAL(203.0) / ALWAN_LITERAL(10000.0),
                        ALWAN_LITERAL(0.75));
}

ALWAN_INLINE alwan_scalar alwan_tf_mirror_v(alwan_scalar x,
                                             alwan_scalar tf_abs_x) {
    alwan_scalar sign_x = ALWAN_SELECT(x < ALWAN_ZERO, -ALWAN_ONE, ALWAN_ONE);
    return sign_x * tf_abs_x;
}

ALWAN_INLINE alwan_scalar alwan_bt2446a_forward_v(alwan_scalar Y_hdr,
                                                    alwan_scalar L_hdr,
                                                    alwan_scalar L_sdr) {
    alwan_scalar pHDR = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_hdr / ALWAN_LITERAL(10000.0),
                  ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));
    alwan_scalar pSDR = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_sdr / ALWAN_LITERAL(10000.0),
                  ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));

    alwan_scalar Y_lin = (ALWAN_POW(pHDR, Y_hdr) - ALWAN_ONE) /
                          (pHDR - ALWAN_ONE);

    alwan_scalar k1 = ALWAN_LITERAL(0.5) * pSDR / pHDR;
    alwan_scalar k3 = ALWAN_ONE;
    alwan_scalar k2 = k1 + (k3 - k1) * ALWAN_LITERAL(0.75);

    alwan_scalar t1 = Y_lin / k1;
    alwan_scalar region1 = k1 * ALWAN_LITERAL(0.5) * t1;

    alwan_scalar t2 = (Y_lin - k1) / (k2 - k1);
    alwan_scalar region2 = k1 * ALWAN_LITERAL(0.5) + (k2 - k1) *
        (t2 - ALWAN_LITERAL(0.25) * t2 * t2);

    alwan_scalar mapped_mid = k1 * ALWAN_LITERAL(0.5) + (k2 - k1) * ALWAN_LITERAL(0.75);
    alwan_scalar t3 = (Y_lin - k2) / (k3 - k2);
    alwan_scalar region3 = mapped_mid + (ALWAN_ONE - mapped_mid) *
        t3 / (ALWAN_ONE + (ALWAN_ONE - t3) * ALWAN_LITERAL(2.0));

    alwan_scalar Y_mapped = ALWAN_SELECT(Y_lin <= k1, region1,
                            ALWAN_SELECT(Y_lin <= k2, region2, region3));

    alwan_scalar Y_sdr = ALWAN_LN(ALWAN_ONE + (pSDR - ALWAN_ONE) * Y_mapped) /
                          ALWAN_LN(pSDR);

    return alwan_saturate(Y_sdr);
}

ALWAN_INLINE alwan_scalar alwan_bt2446a_inverse_v(alwan_scalar Y_sdr,
                                                    alwan_scalar L_hdr,
                                                    alwan_scalar L_sdr) {
    alwan_scalar pHDR = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_hdr / ALWAN_LITERAL(10000.0),
                  ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));
    alwan_scalar pSDR = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_sdr / ALWAN_LITERAL(10000.0),
                  ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));

    alwan_scalar Y_mapped = (ALWAN_POW(pSDR, Y_sdr) - ALWAN_ONE) /
                             (pSDR - ALWAN_ONE);

    alwan_scalar k1 = ALWAN_LITERAL(0.5) * pSDR / pHDR;
    alwan_scalar k2_minus_k1 = (ALWAN_ONE - k1) * ALWAN_LITERAL(0.75);
    alwan_scalar k2 = k1 + k2_minus_k1;
    alwan_scalar mapped_k1 = k1 * ALWAN_LITERAL(0.5);
    alwan_scalar mapped_k2 = mapped_k1 + k2_minus_k1 * ALWAN_LITERAL(0.75);

    alwan_scalar inv_region1 = Y_mapped / ALWAN_LITERAL(0.5);

    alwan_scalar b_coeff = k2_minus_k1;
    alwan_scalar c_coeff = -ALWAN_LITERAL(0.25) * k2_minus_k1;
    alwan_scalar disc = b_coeff * b_coeff + ALWAN_LITERAL(4.0) * c_coeff * (Y_mapped - mapped_k1);
    alwan_scalar disc_safe = ALWAN_SELECT(disc < ALWAN_ZERO, ALWAN_ZERO, disc);
    alwan_scalar t2 = (-b_coeff + ALWAN_SQRT(disc_safe)) / (ALWAN_LITERAL(2.0) * c_coeff);
    alwan_scalar inv_region2 = k1 + t2 * k2_minus_k1;

    alwan_scalar rest = ALWAN_ONE - mapped_k2;
    alwan_scalar y_norm = (Y_mapped - mapped_k2) / ALWAN_SELECT(rest < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), rest);
    alwan_scalar t3 = y_norm / (ALWAN_ONE + ALWAN_LITERAL(2.0) * (ALWAN_ONE - y_norm));
    alwan_scalar k3_minus_k2 = ALWAN_ONE - k2;
    alwan_scalar inv_region3 = k2 + t3 * k3_minus_k2;

    alwan_scalar Y_lin = ALWAN_SELECT(Y_mapped <= mapped_k1, inv_region1,
                         ALWAN_SELECT(Y_mapped <= mapped_k2, inv_region2, inv_region3));

    alwan_scalar Y_hdr = ALWAN_LN(ALWAN_ONE + (pHDR - ALWAN_ONE) * Y_lin) /
                          ALWAN_LN(pHDR);

    return alwan_saturate(Y_hdr);
}

ALWAN_INLINE alwan_scalar alwan_bt2446b_forward_v(alwan_scalar Y_sdr,
                                                    alwan_scalar L_hdr,
                                                    alwan_scalar L_sdr) {
    alwan_scalar Y_lin = ALWAN_POW(alwan_saturate(Y_sdr), ALWAN_LITERAL(2.4));

    alwan_scalar pHDR = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_hdr / ALWAN_LITERAL(10000.0),
                  ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));
    alwan_scalar pSDR = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_sdr / ALWAN_LITERAL(10000.0),
                  ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));

    alwan_scalar alpha = ALWAN_LN(pSDR) / ALWAN_LN(pHDR);

    alwan_scalar Y_expanded = ALWAN_POW(Y_lin, alpha);

    alwan_scalar Y_hdr = ALWAN_LN(ALWAN_ONE + (pHDR - ALWAN_ONE) * Y_expanded) /
                          ALWAN_LN(pHDR);

    return alwan_saturate(Y_hdr);
}

ALWAN_INLINE alwan_scalar alwan_bt2446c_forward_v(alwan_scalar Y_hdr,
                                                    alwan_scalar L_hdr,
                                                    alwan_scalar L_sdr) {
    alwan_scalar rho = ALWAN_LITERAL(1.0) +
        ALWAN_LITERAL(32.0) * ALWAN_POW(L_hdr / ALWAN_LITERAL(10000.0),
                                         ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));
    alwan_scalar rho_sdr = ALWAN_LITERAL(1.0) +
        ALWAN_LITERAL(32.0) * ALWAN_POW(L_sdr / ALWAN_LITERAL(10000.0),
                                         ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));

    alwan_scalar Y_lin = (ALWAN_POW(rho, Y_hdr) - ALWAN_ONE) / (rho - ALWAN_ONE);

    alwan_scalar alpha = ALWAN_LN(rho_sdr) / ALWAN_LN(rho);

    alwan_scalar Y_compressed = ALWAN_POW(Y_lin, alpha);

    alwan_scalar shoulder = ALWAN_LITERAL(0.95);
    alwan_scalar t = (Y_compressed - shoulder) / (ALWAN_ONE - shoulder);
    t = ALWAN_SELECT(t < ALWAN_ZERO, ALWAN_ZERO, t);
    alwan_scalar soft = shoulder + (ALWAN_ONE - shoulder) *
                        t / (ALWAN_ONE + t);
    Y_compressed = ALWAN_SELECT(Y_compressed > shoulder, soft, Y_compressed);

    alwan_scalar Y_sdr = ALWAN_LN(ALWAN_ONE + (rho_sdr - ALWAN_ONE) * Y_compressed) /
                          ALWAN_LN(rho_sdr);

    return alwan_saturate(Y_sdr);
}

ALWAN_INLINE alwan_scalar alwan_bt2390_eetf_v(alwan_scalar E_pq,
                                                alwan_scalar LB,
                                                alwan_scalar LW,
                                                alwan_scalar LB_target,
                                                alwan_scalar LW_target) {
    alwan_scalar range = LW - LB;
    range = ALWAN_SELECT(range < ALWAN_LITERAL(1e-10),
                         ALWAN_LITERAL(1e-10), range);
    alwan_scalar E_norm = (E_pq - LB) / range;
    E_norm = alwan_clamp(E_norm, ALWAN_ZERO, ALWAN_ONE);

    alwan_scalar target_range = LW_target - LB_target;
    alwan_scalar KS = ALWAN_LITERAL(1.5) * target_range / range - ALWAN_LITERAL(0.5);
    KS = alwan_clamp(KS, ALWAN_ZERO, ALWAN_ONE);

    alwan_scalar t = (E_norm - KS) / (ALWAN_ONE - KS + ALWAN_LITERAL(1e-10));
    t = alwan_clamp(t, ALWAN_ZERO, ALWAN_ONE);

    alwan_scalar t2 = t * t;
    alwan_scalar t3 = t2 * t;

    alwan_scalar max_mapped = target_range / range;
    max_mapped = ALWAN_SELECT(max_mapped > ALWAN_ONE, ALWAN_ONE, max_mapped);

    alwan_scalar P = (ALWAN_LITERAL(2.0) * t3 - ALWAN_LITERAL(3.0) * t2 + ALWAN_ONE) * KS
                   + (t3 - ALWAN_LITERAL(2.0) * t2 + t) * (ALWAN_ONE - KS)
                   + (-ALWAN_LITERAL(2.0) * t3 + ALWAN_LITERAL(3.0) * t2) * max_mapped;

    alwan_scalar E_mapped = ALWAN_SELECT(E_norm <= KS, E_norm, P);

    return LB_target + E_mapped * range;
}

ALWAN_INLINE alwan_scalar alwan_bt2390_eetf_luminance_v(alwan_scalar E_pq,
                                                          alwan_scalar L_source_peak,
                                                          alwan_scalar L_target_peak) {
    alwan_scalar LW_src = alwan_pq_oetf(L_source_peak);
    alwan_scalar LW_tgt = alwan_pq_oetf(L_target_peak);
    return alwan_bt2390_eetf_v(E_pq, ALWAN_ZERO, LW_src, ALWAN_ZERO, LW_tgt);
}

ALWAN_INLINE alwan_scalar alwan_exposure_tonemap_v(alwan_scalar L,
                                                     alwan_scalar exposure) {
    alwan_scalar gain = ALWAN_POW(ALWAN_LITERAL(2.0), exposure);
    return ALWAN_ONE - ALWAN_EXP(-gain * L);
}

ALWAN_INLINE alwan_vec3 alwan_exposure_tonemap_rgb_v(alwan_vec3 rgb,
                                                       alwan_scalar exposure) {
    alwan_vec3 result;
    alwan_scalar gain = ALWAN_POW(ALWAN_LITERAL(2.0), exposure);
    result.v[0] = ALWAN_ONE - ALWAN_EXP(-gain * rgb.v[0]);
    result.v[1] = ALWAN_ONE - ALWAN_EXP(-gain * rgb.v[1]);
    result.v[2] = ALWAN_ONE - ALWAN_EXP(-gain * rgb.v[2]);
    return result;
}

ALWAN_INLINE alwan_scalar alwan_reinhard_calibrated_v(alwan_scalar L,
                                                        alwan_scalar key,
                                                        alwan_scalar L_avg,
                                                        alwan_scalar L_white) {
    alwan_scalar L_avg_safe = ALWAN_SELECT(L_avg < ALWAN_LITERAL(1e-10),
                                            ALWAN_LITERAL(1e-10), L_avg);
    alwan_scalar L_scaled = (key / L_avg_safe) * L;
    alwan_scalar Lw2 = L_white * L_white;
    return L_scaled * (ALWAN_ONE + L_scaled / Lw2) / (ALWAN_ONE + L_scaled);
}

ALWAN_INLINE alwan_scalar alwan_gamut_compress_chroma_v(alwan_scalar Cz,
                                                         alwan_scalar Cz_max) {
    alwan_scalar ratio = Cz / ALWAN_SELECT(Cz_max < ALWAN_LITERAL(1e-10),
                                             ALWAN_LITERAL(1e-10), Cz_max);

    alwan_scalar excess = ratio - ALWAN_ONE;
    alwan_scalar compressed = ALWAN_ONE + ALWAN_TANH(excess) *
                               ALWAN_LITERAL(0.1);
    alwan_scalar Cz_out = ALWAN_SELECT(ratio <= ALWAN_ONE,
                                        Cz,
                                        compressed * Cz_max);
    return Cz_out;
}

ALWAN_INLINE alwan_jzczhz alwan_hdr_gamut_map_jzczhz_v(alwan_jzczhz in,
                                                          alwan_scalar Cz_max) {
    alwan_jzczhz result;
    result.Jz = in.Jz;
    result.hz = in.hz;
    result.Cz = alwan_gamut_compress_chroma_v(in.Cz, Cz_max);
    return result;
}

ALWAN_INLINE alwan_scalar alwan_pq_normalize_peak_v(alwan_scalar pq_value,
                                                      alwan_scalar display_peak) {
    alwan_scalar L_abs = alwan_pq_eotf(pq_value);

    alwan_scalar L_scaled = ALWAN_SELECT(L_abs > display_peak,
                                          display_peak, L_abs);

    return alwan_pq_oetf(L_scaled);
}

ALWAN_INLINE void alwan_st2086_init_v(alwan_scalar display_primaries_xy[6],
                                       alwan_scalar white_point_xy[2],
                                       alwan_scalar max_luminance,
                                       alwan_scalar min_luminance,
                                       alwan_scalar *out_primaries_xy,
                                       alwan_scalar *out_white_xy,
                                       alwan_scalar *out_max_lum,
                                       alwan_scalar *out_min_lum) {
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

ALWAN_INLINE alwan_scalar alwan_content_light_level_v(alwan_scalar const *rgb_data,
                                                        size_t count,
                                                        size_t stride_bytes) {
    alwan_scalar max_val = ALWAN_ZERO;
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *p = (alwan_scalar const *)((char const *)rgb_data + i * stride_bytes);
        alwan_scalar px_max = p[0];
        px_max = ALWAN_SELECT(p[1] > px_max, p[1], px_max);
        px_max = ALWAN_SELECT(p[2] > px_max, p[2], px_max);
        max_val = ALWAN_SELECT(px_max > max_val, px_max, max_val);
    }
    return max_val;
}

#endif

#endif /* ALWAN_HDR_CORE_H */
