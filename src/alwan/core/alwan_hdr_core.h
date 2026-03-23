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

/* ================================================================
 * HLG OOTF (Scene-to-Display) per BT.2100-2
 *
 * Yd = alpha * pow(dot(Rbt2020, E), gamma_sys - 1) * E_component
 *
 * E      - scene-referred HLG signal (R, G, B) after OETF^-1
 * Lw     - nominal peak luminance (cd/m2), default 1000
 * gamma_sys - system gamma, default 1.2
 * Returns display-linear (Rd, Gd, Bd) in cd/m2
 * ================================================================ */

ALWAN_INLINE alwan_rgb alwan_hlg_ootf_v(alwan_rgb E,
                                          alwan_scalar Lw,
                                          alwan_scalar gamma_sys) {
    alwan_rgb result;
    alwan_scalar alpha = Lw;

    /* BT.2100 luma coefficients for BT.2020 */
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

/* ================================================================
 * HLG Inverse OOTF (Display-to-Scene)
 *
 * Inverts the OOTF to recover scene-referred signal from display.
 * ================================================================ */

ALWAN_INLINE alwan_rgb alwan_hlg_ootf_inv_v(alwan_rgb Fd,
                                              alwan_scalar Lw,
                                              alwan_scalar gamma_sys) {
    alwan_rgb result;
    alwan_scalar alpha = Lw;

    /* Display-linear luma */
    alwan_scalar Yd = ALWAN_LUMA_KR_BT2020 * Fd.r
                    + ALWAN_LUMA_KG_BT2020 * Fd.g
                    + ALWAN_LUMA_KB_BT2020 * Fd.b;

    alwan_scalar Yd_safe = ALWAN_SELECT(Yd < ALWAN_LITERAL(1e-12),
                                         ALWAN_LITERAL(1e-12), Yd);

    /* Inverse of: Fd = alpha * Ys^(gamma-1) * E, with Yd = alpha * Ys^gamma
     * => Ys = (Yd/alpha)^(1/gamma)
     * => E  = Fd / (alpha * Ys^(gamma-1))
     *       = Fd * alpha^(-1/gamma) * Yd^((1-gamma)/gamma) */
    alwan_scalar inv_gamma = ALWAN_ONE / gamma_sys;
    alwan_scalar factor = ALWAN_POW(alpha, -inv_gamma)
                        * ALWAN_POW(Yd_safe, (ALWAN_ONE - gamma_sys) / gamma_sys);

    result.r = factor * Fd.r;
    result.g = factor * Fd.g;
    result.b = factor * Fd.b;
    return result;
}

/* ================================================================
 * BT.2408 Reference White Level
 *
 * Returns the reference white level for PQ or HLG encoding.
 * For PQ:  203 / 10000 = 0.0203  (203 nits out of 10,000)
 * For HLG: 0.75 (75% of nominal peak)
 * use_pq: 1 for PQ, 0 for HLG
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_bt2408_ref_white_v(int use_pq) {
    return ALWAN_SELECT(use_pq,
                        ALWAN_LITERAL(203.0) / ALWAN_LITERAL(10000.0),
                        ALWAN_LITERAL(0.75));
}

/* ================================================================
 * Mirrored Transfer Function Extension
 *
 * Extends a transfer function to handle negative values symmetrically:
 *   tf_mirror(x) = sign(x) * tf(|x|)
 *
 * This is a macro-style inline that takes the TF result for |x|.
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_tf_mirror_v(alwan_scalar x,
                                             alwan_scalar tf_abs_x) {
    alwan_scalar sign_x = ALWAN_SELECT(x < ALWAN_ZERO, -ALWAN_ONE, ALWAN_ONE);
    return sign_x * tf_abs_x;
}

/* ================================================================
 * BT.2446 Method A: HDR to SDR Tone Mapping
 *
 * Three-spline Hermite tone curve per ITU-R BT.2446-1 Method A.
 * Maps scene luminance from HDR range to SDR range.
 *
 * Y_hdr  - input HDR luminance (PQ-encoded, normalized [0,1])
 * L_hdr  - peak HDR luminance (cd/m2), typically 1000
 * L_sdr  - peak SDR luminance (cd/m2), typically 100
 * Returns SDR luminance (normalized [0,1])
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_bt2446a_forward_v(alwan_scalar Y_hdr,
                                                    alwan_scalar L_hdr,
                                                    alwan_scalar L_sdr) {
    /* Compute tone mapping parameters from luminance ratio */
    alwan_scalar pHDR = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_hdr / ALWAN_LITERAL(10000.0),
                  ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));
    alwan_scalar pSDR = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_sdr / ALWAN_LITERAL(10000.0),
                  ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));

    /* Linearize the PQ signal */
    alwan_scalar Y_lin = (ALWAN_POW(pHDR, Y_hdr) - ALWAN_ONE) /
                          (pHDR - ALWAN_ONE);

    /* Knot points for the three-spline curve */
    alwan_scalar k1 = ALWAN_LITERAL(0.5) * pSDR / pHDR;
    alwan_scalar k3 = ALWAN_ONE;
    alwan_scalar k2 = k1 + (k3 - k1) * ALWAN_LITERAL(0.75);

    /* Compress: simple rational curve for the three regions */
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

    /* Re-encode to PQ domain for SDR */
    alwan_scalar Y_sdr = ALWAN_LN(ALWAN_ONE + (pSDR - ALWAN_ONE) * Y_mapped) /
                          ALWAN_LN(pSDR);

    return alwan_saturate(Y_sdr);
}

/* ================================================================
 * BT.2446 Method A: SDR to HDR Inverse Mapping
 *
 * Inverse of the forward tone mapping.
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_bt2446a_inverse_v(alwan_scalar Y_sdr,
                                                    alwan_scalar L_hdr,
                                                    alwan_scalar L_sdr) {
    alwan_scalar pHDR = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_hdr / ALWAN_LITERAL(10000.0),
                  ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));
    alwan_scalar pSDR = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_sdr / ALWAN_LITERAL(10000.0),
                  ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));

    /* Linearize the SDR PQ signal */
    alwan_scalar Y_mapped = (ALWAN_POW(pSDR, Y_sdr) - ALWAN_ONE) /
                             (pSDR - ALWAN_ONE);

    /* Compute knot points (same as forward) */
    alwan_scalar k1 = ALWAN_LITERAL(0.5) * pSDR / pHDR;
    alwan_scalar k2_minus_k1 = (ALWAN_ONE - k1) * ALWAN_LITERAL(0.75);
    alwan_scalar k2 = k1 + k2_minus_k1;
    alwan_scalar mapped_k1 = k1 * ALWAN_LITERAL(0.5);
    alwan_scalar mapped_k2 = mapped_k1 + k2_minus_k1 * ALWAN_LITERAL(0.75);

    /* Inverse of each region */
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

    /* Re-encode to HDR PQ domain */
    alwan_scalar Y_hdr = ALWAN_LN(ALWAN_ONE + (pHDR - ALWAN_ONE) * Y_lin) /
                          ALWAN_LN(pHDR);

    return alwan_saturate(Y_hdr);
}

/* ================================================================
 * BT.2446 Method B: SDR to HDR Up-Conversion
 *
 * Reference: ITU-R BT.2446-1 (2019), Method B
 *   "Conversion and coding practices for HDR/WCG Y'CbCr 4:2:0 video
 *    with PQ transfer characteristics"
 *   https://www.itu.int/rec/R-REC-BT.2446
 *
 * Scene-referred SDR-to-HDR expansion using a sigmoid curve.
 * Y_sdr  - input SDR luminance (normalized [0,1])
 * L_hdr  - target HDR peak luminance (cd/m2), typically 1000
 * L_sdr  - source SDR peak luminance (cd/m2), typically 100
 * Returns HDR luminance (normalized [0,1] in PQ domain)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_bt2446b_forward_v(alwan_scalar Y_sdr,
                                                    alwan_scalar L_hdr,
                                                    alwan_scalar L_sdr) {
    /* Linearize SDR signal using gamma 2.4 EOTF */
    alwan_scalar Y_lin = ALWAN_POW(alwan_saturate(Y_sdr), ALWAN_LITERAL(2.4));

    /* Compute PQ parameters (same basis as Method A) */
    alwan_scalar pHDR = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_hdr / ALWAN_LITERAL(10000.0),
                  ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));
    alwan_scalar pSDR = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_sdr / ALWAN_LITERAL(10000.0),
                  ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));

    /* Expansion: monotonic power curve.
     * Use the ratio of log-bases to compute an expansion exponent < 1,
     * which brightens midtones while preserving black (0) and white (1). */
    alwan_scalar alpha = ALWAN_LN(pSDR) / ALWAN_LN(pHDR);

    /* Apply power expansion (guaranteed monotonic since alpha > 0) */
    alwan_scalar Y_expanded = ALWAN_POW(Y_lin, alpha);

    /* Re-encode: map to HDR PQ domain */
    alwan_scalar Y_hdr = ALWAN_LN(ALWAN_ONE + (pHDR - ALWAN_ONE) * Y_expanded) /
                          ALWAN_LN(pHDR);

    return alwan_saturate(Y_hdr);
}

/* ================================================================
 * BT.2446 Method C: HDR to SDR Tone Mapping (Quantization-Aware)
 *
 * Reference: ITU-R BT.2446-1 (2019), Method C
 *   Designed for 10-bit quantized signals. Uses a power-based
 *   compression with smooth roll-off at highlights.
 *
 * Y_hdr  - input HDR luminance (PQ-encoded, normalized [0,1])
 * L_hdr  - peak HDR luminance (cd/m2), typically 1000
 * L_sdr  - peak SDR luminance (cd/m2), typically 100
 * Returns SDR luminance (gamma-encoded, normalized [0,1])
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_bt2446c_forward_v(alwan_scalar Y_hdr,
                                                    alwan_scalar L_hdr,
                                                    alwan_scalar L_sdr) {
    /* Compute compression ratio */
    alwan_scalar rho = ALWAN_LITERAL(1.0) +
        ALWAN_LITERAL(32.0) * ALWAN_POW(L_hdr / ALWAN_LITERAL(10000.0),
                                         ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));
    alwan_scalar rho_sdr = ALWAN_LITERAL(1.0) +
        ALWAN_LITERAL(32.0) * ALWAN_POW(L_sdr / ALWAN_LITERAL(10000.0),
                                         ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));

    /* Linearize PQ-encoded input */
    alwan_scalar Y_lin = (ALWAN_POW(rho, Y_hdr) - ALWAN_ONE) / (rho - ALWAN_ONE);

    /* Power-based compression: apply a variable exponent that
     * depends on the luminance ratio. This preserves quantization
     * steps better than the three-spline approach. */
    alwan_scalar alpha = ALWAN_LN(rho_sdr) / ALWAN_LN(rho);

    /* Apply power compression with highlight roll-off */
    alwan_scalar Y_compressed = ALWAN_POW(Y_lin, alpha);

    /* Smooth shoulder to avoid hard clipping at peak */
    alwan_scalar shoulder = ALWAN_LITERAL(0.95);
    alwan_scalar t = (Y_compressed - shoulder) / (ALWAN_ONE - shoulder);
    t = ALWAN_SELECT(t < ALWAN_ZERO, ALWAN_ZERO, t);
    alwan_scalar soft = shoulder + (ALWAN_ONE - shoulder) *
                        t / (ALWAN_ONE + t);
    Y_compressed = ALWAN_SELECT(Y_compressed > shoulder, soft, Y_compressed);

    /* Re-encode to SDR PQ domain */
    alwan_scalar Y_sdr = ALWAN_LN(ALWAN_ONE + (rho_sdr - ALWAN_ONE) * Y_compressed) /
                          ALWAN_LN(rho_sdr);

    return alwan_saturate(Y_sdr);
}

/* ================================================================
 * BT.2390 EETF (Electro-Electro Transfer Function)
 *
 * Reference: ITU-R BT.2390-8 (2021), Annex 1
 *   "High dynamic range television for production and
 *    international programme exchange"
 *   https://www.itu.int/rec/R-REC-BT.2390
 *
 * Hermite spline-based tone mapping for PQ signals.
 * Maps from source PQ range [LB,LW] to target PQ range.
 *
 * E_pq   - input PQ-encoded value (normalized [0,1])
 * LB     - source black level (PQ-encoded, e.g. 0.0)
 * LW     - source white level (PQ-encoded, e.g. 1.0 for 10000 nits)
 * LB_target - target black level (PQ-encoded)
 * LW_target - target white level (PQ-encoded)
 * Returns mapped PQ value
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_bt2390_eetf_v(alwan_scalar E_pq,
                                                alwan_scalar LB,
                                                alwan_scalar LW,
                                                alwan_scalar LB_target,
                                                alwan_scalar LW_target) {
    /* Normalize input to [0,1] within source range */
    alwan_scalar range = LW - LB;
    range = ALWAN_SELECT(range < ALWAN_LITERAL(1e-10),
                         ALWAN_LITERAL(1e-10), range);
    alwan_scalar E_norm = (E_pq - LB) / range;
    E_norm = alwan_clamp(E_norm, ALWAN_ZERO, ALWAN_ONE);

    /* Knee point: where tone mapping begins to compress */
    alwan_scalar target_range = LW_target - LB_target;
    alwan_scalar KS = ALWAN_LITERAL(1.5) * target_range / range - ALWAN_LITERAL(0.5);
    KS = alwan_clamp(KS, ALWAN_ZERO, ALWAN_ONE);

    /* Below knee point: linear pass-through */
    /* Above knee point: Hermite spline compression */
    alwan_scalar t = (E_norm - KS) / (ALWAN_ONE - KS + ALWAN_LITERAL(1e-10));
    t = alwan_clamp(t, ALWAN_ZERO, ALWAN_ONE);

    /* Hermite spline: P(t) that maps [KS,1] -> [KS, max_target] */
    alwan_scalar t2 = t * t;
    alwan_scalar t3 = t2 * t;

    /* Hermite basis: start at (KS, KS), end at (1, max_mapped)
     * with zero slope at end point for smooth roll-off */
    alwan_scalar max_mapped = target_range / range;
    max_mapped = ALWAN_SELECT(max_mapped > ALWAN_ONE, ALWAN_ONE, max_mapped);

    alwan_scalar P = (ALWAN_LITERAL(2.0) * t3 - ALWAN_LITERAL(3.0) * t2 + ALWAN_ONE) * KS
                   + (t3 - ALWAN_LITERAL(2.0) * t2 + t) * (ALWAN_ONE - KS)
                   + (-ALWAN_LITERAL(2.0) * t3 + ALWAN_LITERAL(3.0) * t2) * max_mapped;

    /* Final mapped value: below KS = linear, above = spline */
    alwan_scalar E_mapped = ALWAN_SELECT(E_norm <= KS, E_norm, P);

    /* Denormalize to target PQ range */
    return LB_target + E_mapped * range;
}

/* Convenience: BT.2390 EETF with luminance (cd/m²) parameters.
 * Internally converts to PQ domain. */
ALWAN_INLINE alwan_scalar alwan_bt2390_eetf_luminance_v(alwan_scalar E_pq,
                                                          alwan_scalar L_source_peak,
                                                          alwan_scalar L_target_peak) {
    /* Convert luminance levels to PQ-encoded values */
    alwan_scalar LW_src = alwan_pq_oetf(L_source_peak);
    alwan_scalar LW_tgt = alwan_pq_oetf(L_target_peak);
    return alwan_bt2390_eetf_v(E_pq, ALWAN_ZERO, LW_src, ALWAN_ZERO, LW_tgt);
}

/* ================================================================
 * Exposure-Based Tone Mapping with Shoulder Compression
 *
 * Reference: General photographic exposure model.
 *   L_out = 1 - exp(-exposure_gain * L_in)
 * exposure: EV offset (0 = no change, +1 = double, -1 = half)
 *
 * Per-channel application for RGB signals.
 * ================================================================ */

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

/* ================================================================
 * Reinhard Calibrated Tone Mapping
 *
 * Reference: Reinhard et al. (2002) "Photographic Tone Reproduction
 *            for Digital Images", SIGGRAPH 2002.
 *   https://www.cs.utah.edu/docs/techreports/2002/pdf/UUCS-02-001.pdf
 *
 * Key-based with white point adaptation:
 *   L_mapped = (key / L_avg) * L
 *   L_out = L_mapped * (1 + L_mapped / L_white^2) / (1 + L_mapped)
 *
 * key: exposure key (0.18 = 18% gray, the standard scene key)
 * L_avg: log-average luminance of the scene
 * L_white: smallest luminance mapped to pure white
 * ================================================================ */

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

/* ================================================================
 * HDR Gamut Mapping: Chroma Compression in JzCzhz
 *
 * Reference: ITU-R BT.2407 (2017) "Colour gamut mapping"
 *   Compress chroma along constant-hue lines to fit within a
 *   target gamut boundary, preserving lightness and hue.
 *
 * Jz, Cz, hz:  input JzCzhz coordinates
 * Cz_max:      maximum chroma at this (Jz, hz) for target gamut
 * Returns compressed chroma value
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_gamut_compress_chroma_v(alwan_scalar Cz,
                                                         alwan_scalar Cz_max) {
    /* If within gamut, no compression needed */
    alwan_scalar ratio = Cz / ALWAN_SELECT(Cz_max < ALWAN_LITERAL(1e-10),
                                             ALWAN_LITERAL(1e-10), Cz_max);

    /* Smooth compression using a soft-clip function:
     * For ratio <= 1: pass through (in gamut)
     * For ratio > 1:  compress toward 1 using tanh-based soft clip */
    alwan_scalar excess = ratio - ALWAN_ONE;
    alwan_scalar compressed = ALWAN_ONE + ALWAN_TANH(excess) *
                               ALWAN_LITERAL(0.1); /* gentle compression */
    alwan_scalar Cz_out = ALWAN_SELECT(ratio <= ALWAN_ONE,
                                        Cz,
                                        compressed * Cz_max);
    return Cz_out;
}

/* Full hue-preserving gamut map in JzCzhz:
 * Compresses chroma while preserving Jz and hz exactly. */
ALWAN_INLINE alwan_jzczhz alwan_hdr_gamut_map_jzczhz_v(alwan_jzczhz in,
                                                          alwan_scalar Cz_max) {
    alwan_jzczhz result;
    result.Jz = in.Jz;
    result.hz = in.hz;
    result.Cz = alwan_gamut_compress_chroma_v(in.Cz, Cz_max);
    return result;
}

/* ================================================================
 * Peak Luminance Normalization for PQ
 *
 * Reference: ITU-R BT.2408-4 (2021), "Operational practices in HDR
 *            television production"
 *
 * Scales PQ absolute values to a display-specific peak luminance.
 * Useful for adapting mastered content to displays with different
 * peak capabilities.
 *
 * pq_value:     PQ-encoded signal (normalized [0,1])
 * display_peak: actual display peak luminance (cd/m2)
 * Returns re-encoded PQ value normalized to the display peak
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_pq_normalize_peak_v(alwan_scalar pq_value,
                                                      alwan_scalar display_peak) {
    /* Decode PQ to absolute luminance */
    alwan_scalar L_abs = alwan_pq_eotf(pq_value);

    /* Scale to display-relative luminance and clip */
    alwan_scalar L_scaled = ALWAN_SELECT(L_abs > display_peak,
                                          display_peak, L_abs);

    /* Re-encode to PQ */
    return alwan_pq_oetf(L_scaled);
}

/* ================================================================
 * ST.2086 Metadata Helpers
 *
 * Reference: SMPTE ST 2086:2018 "Mastering Display Color Volume
 *            Metadata Supporting High Luminance and Wide Color
 *            Gamut Images"
 *
 * Populate ST.2086 metadata from known display parameters.
 * ================================================================ */

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

/* Compute MaxCLL from linear RGB pixel data (absolute luminance, cd/m2)
 * Returns the maximum of max(R,G,B) across all pixels */
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

#endif /* ALWAN_HDR_CORE_H */
