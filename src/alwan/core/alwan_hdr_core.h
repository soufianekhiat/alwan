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
    alwan_scalar Ys = ALWAN_LITERAL(0.2627) * E.r
                    + ALWAN_LITERAL(0.6780) * E.g
                    + ALWAN_LITERAL(0.0593) * E.b;

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
    alwan_scalar Yd = ALWAN_LITERAL(0.2627) * Fd.r
                    + ALWAN_LITERAL(0.6780) * Fd.g
                    + ALWAN_LITERAL(0.0593) * Fd.b;

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

#endif /* ALWAN_HDR_CORE_H */
