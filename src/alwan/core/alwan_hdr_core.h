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

    alwan_scalar Ys_abs = ALWAN_ABS(Ys);
    alwan_scalar Ys_safe = ALWAN_SELECT(Ys_abs < ALWAN_LITERAL(1e-12),
                                         ALWAN_LITERAL(1e-12), Ys_abs);
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

    alwan_scalar Yd_abs = ALWAN_ABS(Yd);
    alwan_scalar Yd_safe = ALWAN_SELECT(Yd_abs < ALWAN_LITERAL(1e-12),
                                         ALWAN_LITERAL(1e-12), Yd_abs);

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

/* ================================================================
 * PU21: perceptually uniform encoding of absolute luminance
 * Mantiuk and Azimi 2021. The parameters are the published ones, from
 * gfxdisp/pu21 matlab/pu21_encoder.m (BSD-3-Clause) at 78340c0. Variant 0 is
 * banding_glare, which the authors recommend; 1 banding, 2 peaks, 3 peaks_glare.
 * The encoder there limits Y to [0.005, 10000] cd/m2 first; this is the curve alone.
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_pu21_pick_v(int variant, alwan_scalar banding_glare,
                                                            alwan_scalar banding, alwan_scalar peaks,
                                                            alwan_scalar peaks_glare) {
    return ALWAN_SELECT(variant == 1, banding,
           ALWAN_SELECT(variant == 2, peaks,
           ALWAN_SELECT(variant == 3, peaks_glare, banding_glare)));
}

ALWAN_INLINE alwan_scalar alwan_pu21_encode_v(alwan_scalar Y, int variant) {
    alwan_scalar p1 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(0.353487901),
        ALWAN_LITERAL(1.070275272), ALWAN_LITERAL(1.043882782), ALWAN_LITERAL(816.885024));
    alwan_scalar p2 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(0.3734658629),
        ALWAN_LITERAL(0.4088273932), ALWAN_LITERAL(0.6459495343), ALWAN_LITERAL(1479.463946));
    alwan_scalar p3 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(8.277049286e-05),
        ALWAN_LITERAL(0.153224308), ALWAN_LITERAL(0.3194584211), ALWAN_LITERAL(0.001253215609));
    alwan_scalar p4 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(0.9062562627),
        ALWAN_LITERAL(0.2520326168), ALWAN_LITERAL(0.374025247), ALWAN_LITERAL(0.9329636822));
    alwan_scalar p5 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(0.09150303166),
        ALWAN_LITERAL(1.063512885), ALWAN_LITERAL(1.114783422), ALWAN_LITERAL(0.06746643971));
    alwan_scalar p6 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(0.9099517204),
        ALWAN_LITERAL(1.14115047), ALWAN_LITERAL(1.095360363), ALWAN_LITERAL(1.573435413));
    alwan_scalar p7 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(596.3148142),
        ALWAN_LITERAL(521.4527484), ALWAN_LITERAL(384.9217577), ALWAN_LITERAL(419.6006374));
    alwan_scalar yp = ALWAN_POW(Y, p4);
    alwan_scalar v = p7 * (ALWAN_POW((p1 + p2 * yp) / (ALWAN_ONE + p3 * yp), p5) - p6);
    return alwan_max(v, ALWAN_ZERO);
}

ALWAN_INLINE alwan_scalar alwan_pu21_decode_v(alwan_scalar V, int variant) {
    alwan_scalar p1 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(0.353487901),
        ALWAN_LITERAL(1.070275272), ALWAN_LITERAL(1.043882782), ALWAN_LITERAL(816.885024));
    alwan_scalar p2 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(0.3734658629),
        ALWAN_LITERAL(0.4088273932), ALWAN_LITERAL(0.6459495343), ALWAN_LITERAL(1479.463946));
    alwan_scalar p3 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(8.277049286e-05),
        ALWAN_LITERAL(0.153224308), ALWAN_LITERAL(0.3194584211), ALWAN_LITERAL(0.001253215609));
    alwan_scalar p4 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(0.9062562627),
        ALWAN_LITERAL(0.2520326168), ALWAN_LITERAL(0.374025247), ALWAN_LITERAL(0.9329636822));
    alwan_scalar p5 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(0.09150303166),
        ALWAN_LITERAL(1.063512885), ALWAN_LITERAL(1.114783422), ALWAN_LITERAL(0.06746643971));
    alwan_scalar p6 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(0.9099517204),
        ALWAN_LITERAL(1.14115047), ALWAN_LITERAL(1.095360363), ALWAN_LITERAL(1.573435413));
    alwan_scalar p7 = alwan_pu21_pick_v(variant, ALWAN_LITERAL(596.3148142),
        ALWAN_LITERAL(521.4527484), ALWAN_LITERAL(384.9217577), ALWAN_LITERAL(419.6006374));
    alwan_scalar vp = ALWAN_POW(alwan_max(V / p7 + p6, ALWAN_ZERO), ALWAN_ONE / p5);
    return ALWAN_POW(alwan_max(vp - p1, ALWAN_ZERO) / (p2 - p3 * vp), ALWAN_ONE / p4);
}

ALWAN_INLINE alwan_scalar alwan_tf_mirror_v(alwan_scalar x,
                                             alwan_scalar tf_abs_x) {
    alwan_scalar sign_x = ALWAN_SELECT(x < ALWAN_ZERO, -ALWAN_ONE, ALWAN_ONE);
    return sign_x * tf_abs_x;
}

/* ================================================================
 * BT.2446 Method A: HDR to SDR Tone Mapping
 *
 * Report ITU-R BT.2446-1 (2021), section 4.1, Tables 2 and 3, on BT.2020 RGB. Input: linear
 * display-light HDR RGB normalised to the mastering peak L_hdr (1 = L_hdr); output: the SDR
 * R'G'B' (BT.2020 primaries, gamma-encoded) that the Report's Y'Cb'Cr' converts to through
 * Recommendation BT.2020's matrix, not clipped. Negative input is taken as 0.
 * ================================================================ */

ALWAN_INLINE alwan_vec3 alwan_bt2446a_forward_v(alwan_vec3 rgb,
                                                    alwan_scalar L_hdr,
                                                    alwan_scalar L_sdr) {
    alwan_scalar inv24 = ALWAN_ONE / ALWAN_LITERAL(2.4);
    alwan_scalar Rp = ALWAN_POW(alwan_max(rgb.v[0], ALWAN_ZERO), inv24);
    alwan_scalar Gp = ALWAN_POW(alwan_max(rgb.v[1], ALWAN_ZERO), inv24);
    alwan_scalar Bp = ALWAN_POW(alwan_max(rgb.v[2], ALWAN_ZERO), inv24);
    alwan_scalar Yp = ALWAN_LITERAL(0.2627) * Rp + ALWAN_LITERAL(0.6780) * Gp + ALWAN_LITERAL(0.0593) * Bp;

    alwan_scalar rho_hdr = ALWAN_ONE + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_hdr / ALWAN_LITERAL(10000.0), inv24);
    alwan_scalar rho_sdr = ALWAN_ONE + ALWAN_LITERAL(32.0) *
        ALWAN_POW(L_sdr / ALWAN_LITERAL(10000.0), inv24);

    /* Step 1: to the perceptual domain */
    alwan_scalar Ypp = ALWAN_LN(ALWAN_ONE + (rho_hdr - ALWAN_ONE) * Yp) / ALWAN_LN(rho_hdr);
    /* Step 2: the knee */
    alwan_scalar c1 = ALWAN_LITERAL(1.0770) * Ypp;
    alwan_scalar c2 = ALWAN_LITERAL(-1.1510) * Ypp * Ypp + ALWAN_LITERAL(2.7811) * Ypp - ALWAN_LITERAL(0.6302);
    alwan_scalar c3 = ALWAN_LITERAL(0.5000) * Ypp + ALWAN_LITERAL(0.5000);
    alwan_scalar Yc = ALWAN_SELECT(Ypp <= ALWAN_LITERAL(0.7399), c1,
                      ALWAN_SELECT(Ypp < ALWAN_LITERAL(0.9909), c2, c3));
    /* Step 3: back to the gamma domain */
    alwan_scalar Ysdr = (ALWAN_POW(rho_sdr, Yc) - ALWAN_ONE) / (rho_sdr - ALWAN_ONE);

    /* Table 3: colour correction */
    alwan_scalar f = ALWAN_SELECT(Yp > ALWAN_ZERO, Ysdr / (ALWAN_LITERAL(1.1) * Yp), ALWAN_ZERO);
    alwan_scalar Cb = f * (Bp - Yp) / ALWAN_LITERAL(1.8814);
    alwan_scalar Cr = f * (Rp - Yp) / ALWAN_LITERAL(1.4746);
    alwan_scalar Ytmo = Ysdr - alwan_max(ALWAN_LITERAL(0.1) * Cr, ALWAN_ZERO);

    alwan_vec3 res;
    res.v[0] = Ytmo + ALWAN_LITERAL(1.4746) * Cr;
    res.v[1] = Ytmo - ALWAN_LITERAL(0.16455) * Cb - ALWAN_LITERAL(0.57135) * Cr;
    res.v[2] = Ytmo + ALWAN_LITERAL(1.8814) * Cb;
    return res;
}

/* ================================================================
 * BT.2446 Method A: SDR to HDR Inverse Mapping
 *
 * Report ITU-R BT.2446-1, section 4.2, Table 4: SDR R'G'B' (BT.2020, full range) to linear
 * display-light HDR RGB normalised to its fixed peak of 1 000 cd/m2 (1 = 1 000 cd/m2).
 * ================================================================ */

ALWAN_INLINE alwan_vec3 alwan_bt2446a_inverse_v(alwan_vec3 rgb) {
    alwan_scalar Yp = ALWAN_LITERAL(0.2627) * rgb.v[0] + ALWAN_LITERAL(0.6780) * rgb.v[1]
                    + ALWAN_LITERAL(0.0593) * rgb.v[2];
    alwan_scalar Cb = (rgb.v[2] - Yp) / ALWAN_LITERAL(1.8814);
    alwan_scalar Cr = (rgb.v[0] - Yp) / ALWAN_LITERAL(1.4746);

    alwan_scalar Ypp = ALWAN_LITERAL(255.0) * Yp;
    alwan_scalar E1 = ALWAN_LITERAL(1.8712e-5) * Ypp * Ypp + ALWAN_LITERAL(-2.7334e-3) * Ypp + ALWAN_LITERAL(1.3141);
    alwan_scalar E2 = ALWAN_LITERAL(2.8305e-6) * Ypp * Ypp + ALWAN_LITERAL(-7.4622e-4) * Ypp + ALWAN_LITERAL(1.2528);
    alwan_scalar E = ALWAN_SELECT(Ypp <= ALWAN_LITERAL(70.0), E1, E2);
    alwan_scalar Yhdr = ALWAN_POW(alwan_max(Ypp, ALWAN_ZERO), E);

    alwan_scalar Sc = ALWAN_SELECT(Yp > ALWAN_ZERO, ALWAN_LITERAL(1.075) * Yhdr / Yp, ALWAN_ONE);
    alwan_scalar Cbh = Cb * Sc;
    alwan_scalar Crh = Cr * Sc;

    alwan_scalar r = alwan_clamp(Yhdr + ALWAN_LITERAL(1.4746) * Crh, ALWAN_ZERO, ALWAN_LITERAL(1000.0));
    alwan_scalar g = alwan_clamp(Yhdr - ALWAN_LITERAL(0.16455) * Cbh - ALWAN_LITERAL(0.57135) * Crh,
                                      ALWAN_ZERO, ALWAN_LITERAL(1000.0));
    alwan_scalar b = alwan_clamp(Yhdr + ALWAN_LITERAL(1.8814) * Cbh, ALWAN_ZERO, ALWAN_LITERAL(1000.0));

    alwan_vec3 res;
    res.v[0] = ALWAN_POW(r / ALWAN_LITERAL(1000.0), ALWAN_LITERAL(2.4));
    res.v[1] = ALWAN_POW(g / ALWAN_LITERAL(1000.0), ALWAN_LITERAL(2.4));
    res.v[2] = ALWAN_POW(b / ALWAN_LITERAL(1000.0), ALWAN_LITERAL(2.4));
    return res;
}

/* ================================================================
 * BT.2446 Method B: SDR to HDR Up-Conversion
 *
 * alwan's own curve in Method B's direction. Report ITU-R BT.2446-1 section 5.1 gives
 * Method B's inverse tone mapping (SDR to HDR, with section 5.2 its complementary HDR to
 * SDR) only as a figure and constraints (a knee near 78 % SDR, unity gradient below it),
 * no equations, so this is not the Report's curve and nothing here checks it against one.
 * ================================================================ */

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

/* ================================================================
 * BT.2446 Method C: HDR to SDR Tone Mapping
 *
 * Report ITU-R BT.2446-1, section 6.1.4: linear below an inflection point, logarithmic above,
 * on display luminance. The Report states the conditions its parameters come from: HDR skin
 * (50 % HLG) to SDR skin (70 %), the inflection at 80 % SDR, and HDR Reference White (75 %
 * HLG) to 96 % SDR, with HLG displayed at L_hdr (system gamma 1.2 + 0.42 log10(L_hdr / 1000),
 * black 0) and SDR by BT.1886 at L_sdr (gamma 2.4, black 0). k1 to k4 follow from those for
 * any L_hdr and L_sdr; at 1 000 and 100 cd/m2 they are the Report's 0.83802, 15.09968,
 * 0.74204, 78.99439 to all five published decimals (its "58.5" is 100 x 0.8^2.4 = 58.535
 * rounded). k3 is the root of the Report's equation (9), found by bisection.
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_bt2446c_hlg_luminance_v(alwan_scalar E, alwan_scalar L_hdr) {
    alwan_scalar a = ALWAN_LITERAL(0.17883277);
    alwan_scalar b = ALWAN_ONE - ALWAN_LITERAL(4.0) * a;
    alwan_scalar c = ALWAN_LITERAL(0.5) - a * ALWAN_LN(ALWAN_LITERAL(4.0) * a);
    alwan_scalar lo = E * E / ALWAN_LITERAL(3.0);
    alwan_scalar hi = (ALWAN_EXP((E - c) / a) + b) / ALWAN_LITERAL(12.0);
    alwan_scalar s = ALWAN_SELECT(E <= ALWAN_LITERAL(0.5), lo, hi);
    alwan_scalar gamma = ALWAN_LITERAL(1.2) + ALWAN_LITERAL(0.42) * ALWAN_LOG10(L_hdr / ALWAN_LITERAL(1000.0));
    return L_hdr * ALWAN_POW(s, gamma);
}

/* k1, k2, k3, k4 and the inflection point Y_ip (cd/m2) in v[0..4]. */
ALWAN_INLINE alwan_scalar alwan_bt2446c_k3_residual_v(alwan_scalar u, alwan_scalar c, alwan_scalar t) {
    /* u = 1 - k3: u ln(1 + c / u) - t, increasing in u from -t towards c - t */
    return u * ALWAN_LN(ALWAN_ONE + c / u) - t;
}

ALWAN_INLINE alwan_scalar alwan_bt2446c_curve_v(alwan_scalar Y, alwan_scalar L_hdr, alwan_scalar L_sdr) {
    alwan_scalar k1 = L_sdr * ALWAN_POW(ALWAN_LITERAL(0.7), ALWAN_LITERAL(2.4))
                    / alwan_bt2446c_hlg_luminance_v(ALWAN_LITERAL(0.5), L_hdr);
    alwan_scalar Yip = L_sdr * ALWAN_POW(ALWAN_LITERAL(0.8), ALWAN_LITERAL(2.4)) / k1;
    alwan_scalar Yref = alwan_bt2446c_hlg_luminance_v(ALWAN_LITERAL(0.75), L_hdr);
    alwan_scalar Ywp = L_sdr * ALWAN_POW(ALWAN_LITERAL(0.96), ALWAN_LITERAL(2.4));
    /* equation (9) with k2 and k4 from (7) and (8): k1 Y_ip u ln((R - 1 + u) / u) = Ywp - k1 Y_ip */
    alwan_scalar c = Yref / Yip - ALWAN_ONE;
    alwan_scalar t = (Ywp - k1 * Yip) / (k1 * Yip);
    alwan_scalar lo = ALWAN_ZERO;
    alwan_scalar hi = t * c / (c - t);   /* u ln(1 + c/u) >= c u / (u + c) bounds the root */
    int i;
    for (i = 0; i < 80; i++) {
        alwan_scalar mid = (lo + hi) * ALWAN_LITERAL(0.5);
        alwan_scalar r = alwan_bt2446c_k3_residual_v(mid, c, t);
        lo = ALWAN_SELECT(r < ALWAN_ZERO, mid, lo);
        hi = ALWAN_SELECT(r < ALWAN_ZERO, hi, mid);
    }
    alwan_scalar u = (lo + hi) * ALWAN_LITERAL(0.5);
    alwan_scalar k3 = ALWAN_ONE - u;
    alwan_scalar k2 = k1 * u * Yip;
    alwan_scalar k4 = k1 * Yip - k2 * ALWAN_LN(u);
    alwan_scalar lin = k1 * Y;
    alwan_scalar arg = alwan_max(Y / Yip - k3, u);   /* equal to Y / Yip - k3 wherever it is used */
    alwan_scalar lg = k2 * ALWAN_LN(arg) + k4;
    return ALWAN_SELECT(Y < Yip, lin, lg);
}

/* The scalar: HDR display luminance over L_hdr in, SDR display luminance over L_sdr out, not
 * clipped (the brightest HDR input lands at 1.18 for 1 000 and 100 cd/m2). */
ALWAN_INLINE alwan_scalar alwan_bt2446c_forward_v(alwan_scalar Y_hdr,
                                                    alwan_scalar L_hdr,
                                                    alwan_scalar L_sdr) {
    return alwan_bt2446c_curve_v(Y_hdr * L_hdr, L_hdr, L_sdr) / L_sdr;
}

/* The whole of section 6.1 for BT.2020 signals: HLG R'G'B' in (displayed at 1 000 cd/m2,
 * gamma 1.2), crosstalk alpha, BT.2020 XYZ, the curve on Y with x and y kept, the Report's
 * XYZ to RGB, the inverse crosstalk, and BT.1886 at 100 cd/m2; SDR R'G'B' out, clipped to
 * [0, 1]. Black gives black. */
ALWAN_INLINE alwan_vec3 alwan_bt2446c_rgb_v(alwan_vec3 hlg, alwan_scalar alpha) {
    alwan_scalar a = ALWAN_LITERAL(0.17883277);
    alwan_scalar bb = ALWAN_ONE - ALWAN_LITERAL(4.0) * a;
    alwan_scalar cc = ALWAN_LITERAL(0.5) - a * ALWAN_LN(ALWAN_LITERAL(4.0) * a);
    alwan_scalar e0 = alwan_max(hlg.v[0], ALWAN_ZERO);
    alwan_scalar e1 = alwan_max(hlg.v[1], ALWAN_ZERO);
    alwan_scalar e2 = alwan_max(hlg.v[2], ALWAN_ZERO);
    alwan_scalar s0 = ALWAN_SELECT(e0 <= ALWAN_LITERAL(0.5), e0 * e0 / ALWAN_LITERAL(3.0),
                                        (ALWAN_EXP((e0 - cc) / a) + bb) / ALWAN_LITERAL(12.0));
    alwan_scalar s1 = ALWAN_SELECT(e1 <= ALWAN_LITERAL(0.5), e1 * e1 / ALWAN_LITERAL(3.0),
                                        (ALWAN_EXP((e1 - cc) / a) + bb) / ALWAN_LITERAL(12.0));
    alwan_scalar s2 = ALWAN_SELECT(e2 <= ALWAN_LITERAL(0.5), e2 * e2 / ALWAN_LITERAL(3.0),
                                        (ALWAN_EXP((e2 - cc) / a) + bb) / ALWAN_LITERAL(12.0));
    alwan_scalar Ys = ALWAN_LITERAL(0.2627) * s0 + ALWAN_LITERAL(0.6780) * s1 + ALWAN_LITERAL(0.0593) * s2;
    alwan_scalar gain = ALWAN_SELECT(Ys > ALWAN_ZERO,
                                          ALWAN_LITERAL(1000.0) * ALWAN_POW(Ys, ALWAN_LITERAL(0.2)),
                                          ALWAN_ZERO);
    alwan_scalar R = gain * s0, G = gain * s1, B = gain * s2;

    alwan_scalar d = ALWAN_ONE - ALWAN_LITERAL(2.0) * alpha;
    alwan_scalar Rx = d * R + alpha * G + alpha * B;
    alwan_scalar Gx = alpha * R + d * G + alpha * B;
    alwan_scalar Bx = alpha * R + alpha * G + d * B;

    alwan_scalar X = ALWAN_LITERAL(0.6370) * Rx + ALWAN_LITERAL(0.1446) * Gx + ALWAN_LITERAL(0.1689) * Bx;
    alwan_scalar Y = ALWAN_LITERAL(0.2627) * Rx + ALWAN_LITERAL(0.6780) * Gx + ALWAN_LITERAL(0.0593) * Bx;
    alwan_scalar Z = ALWAN_LITERAL(0.0281) * Gx + ALWAN_LITERAL(1.0610) * Bx;
    alwan_scalar sum = X + Y + Z;
    alwan_scalar valid = ALWAN_SELECT(Y > ALWAN_ZERO, ALWAN_ONE, ALWAN_ZERO);
    alwan_scalar sum_s = ALWAN_SELECT(sum > ALWAN_ZERO, sum, ALWAN_ONE);
    alwan_scalar x = X / sum_s;
    alwan_scalar y = ALWAN_SELECT(Y > ALWAN_ZERO, Y / sum_s, ALWAN_ONE);

    alwan_scalar Ysdr = alwan_bt2446c_curve_v(Y, ALWAN_LITERAL(1000.0), ALWAN_LITERAL(100.0)) * valid;
    alwan_scalar Xs = x / y * Ysdr;
    alwan_scalar Zs = (ALWAN_ONE - x - y) / y * Ysdr;

    alwan_scalar Rxs = ALWAN_LITERAL(1.7167) * Xs + ALWAN_LITERAL(-0.3557) * Ysdr + ALWAN_LITERAL(-0.2534) * Zs;
    alwan_scalar Gxs = ALWAN_LITERAL(-0.6667) * Xs + ALWAN_LITERAL(1.6165) * Ysdr + ALWAN_LITERAL(0.0158) * Zs;
    alwan_scalar Bxs = ALWAN_LITERAL(0.0176) * Xs + ALWAN_LITERAL(-0.0428) * Ysdr + ALWAN_LITERAL(0.9421) * Zs;

    alwan_scalar inv = ALWAN_ONE / (ALWAN_ONE - ALWAN_LITERAL(3.0) * alpha);
    alwan_scalar Rs = inv * ((ALWAN_ONE - alpha) * Rxs - alpha * Gxs - alpha * Bxs);
    alwan_scalar Gs = inv * (-alpha * Rxs + (ALWAN_ONE - alpha) * Gxs - alpha * Bxs);
    alwan_scalar Bs = inv * (-alpha * Rxs - alpha * Gxs + (ALWAN_ONE - alpha) * Bxs);

    alwan_scalar inv24 = ALWAN_ONE / ALWAN_LITERAL(2.4);
    alwan_vec3 res;
    res.v[0] = alwan_saturate(ALWAN_POW(alwan_max(Rs / ALWAN_LITERAL(100.0), ALWAN_ZERO), inv24));
    res.v[1] = alwan_saturate(ALWAN_POW(alwan_max(Gs / ALWAN_LITERAL(100.0), ALWAN_ZERO), inv24));
    res.v[2] = alwan_saturate(ALWAN_POW(alwan_max(Bs / ALWAN_LITERAL(100.0), ALWAN_ZERO), inv24));
    return res;
}

/* ================================================================
 * BT.2390 EETF, as Report ITU-R BT.2408-8 Annex 5 now gives it
 *
 * All levels PQ-encoded: LB and LW the mastering display's black and white, LB_target and
 * LW_target the target display's (Lmin and Lmax). E1 is normalised to the mastering range
 * (clamped to [0, 1], where the Annex defines the curve); minLum and maxLum are the target
 * levels normalised to that range; KS = 1.5 maxLum - 0.5; below KS 1:1, above it the Hermite
 * spline; then the black lift minLum (1 - E2)^4; then back to the mastering range.
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_bt2390_eetf_v(alwan_scalar E_pq,
                                                alwan_scalar LB,
                                                alwan_scalar LW,
                                                alwan_scalar LB_target,
                                                alwan_scalar LW_target) {
    alwan_scalar range = LW - LB;
    alwan_scalar E1 = alwan_clamp((E_pq - LB) / range, ALWAN_ZERO, ALWAN_ONE);
    alwan_scalar minLum = (LB_target - LB) / range;
    alwan_scalar maxLum = (LW_target - LB) / range;
    alwan_scalar KS = ALWAN_LITERAL(1.5) * maxLum - ALWAN_LITERAL(0.5);

    /* a target at least as bright as the source leaves the signal alone (KS >= 1) */
    alwan_scalar den = ALWAN_SELECT(KS < ALWAN_ONE, ALWAN_ONE - KS, ALWAN_ONE);
    alwan_scalar T = (E1 - KS) / den;
    alwan_scalar T2 = T * T;
    alwan_scalar T3 = T2 * T;
    alwan_scalar P = (ALWAN_LITERAL(2.0) * T3 - ALWAN_LITERAL(3.0) * T2 + ALWAN_ONE) * KS
                   + (T3 - ALWAN_LITERAL(2.0) * T2 + T) * (ALWAN_ONE - KS)
                   + (-ALWAN_LITERAL(2.0) * T3 + ALWAN_LITERAL(3.0) * T2) * maxLum;
    alwan_scalar E2 = ALWAN_SELECT(E1 < KS || KS >= ALWAN_ONE, E1, P);
    alwan_scalar om = ALWAN_ONE - E2;
    alwan_scalar om2 = om * om;
    alwan_scalar E3 = E2 + minLum * om2 * om2;
    return E3 * range + LB;
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

ALWAN_INLINE alwan_jzczhz alwan_hdr_gamut_map_jzczhz_v(alwan_jzczhz src,
                                                          alwan_scalar Cz_max) {
    alwan_jzczhz result;
    result.Jz = src.Jz;
    result.hz = src.hz;
    result.Cz = alwan_gamut_compress_chroma_v(src.Cz, Cz_max);
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
                                       ALWAN_PARAM_ARRAY_OUT(alwan_scalar, out_primaries_xy, 6),
                                       ALWAN_PARAM_ARRAY_OUT(alwan_scalar, out_white_xy, 2),
                                       ALWAN_PARAM_SCALAR_OUT out_max_lum,
                                       ALWAN_PARAM_SCALAR_OUT out_min_lum) {
    out_primaries_xy[0] = display_primaries_xy[0];
    out_primaries_xy[1] = display_primaries_xy[1];
    out_primaries_xy[2] = display_primaries_xy[2];
    out_primaries_xy[3] = display_primaries_xy[3];
    out_primaries_xy[4] = display_primaries_xy[4];
    out_primaries_xy[5] = display_primaries_xy[5];
    out_white_xy[0] = white_point_xy[0];
    out_white_xy[1] = white_point_xy[1];
    ALWAN_REF(out_max_lum) = max_luminance;
    ALWAN_REF(out_min_lum) = min_luminance;
}

/* ISO 21496-1 gain maps; the dual-precision pass above documents them. */
ALWAN_INLINE alwan_scalar alwan_gain_map_weight_v(alwan_scalar display_headroom,
                                                   alwan_scalar base_headroom,
                                                   alwan_scalar alternate_headroom) {
    alwan_scalar w = (display_headroom - base_headroom) / (alternate_headroom - base_headroom);
    w = ALWAN_SELECT(w < ALWAN_ZERO, ALWAN_ZERO, w);
    return ALWAN_SELECT(w > ALWAN_ONE, ALWAN_ONE, w);
}

ALWAN_INLINE alwan_scalar alwan_gain_map_encode_v(alwan_scalar base,
                                                   alwan_scalar alternate,
                                                   alwan_scalar gain_min,
                                                   alwan_scalar gain_max,
                                                   alwan_scalar gamma,
                                                   alwan_scalar base_offset,
                                                   alwan_scalar alternate_offset) {
    alwan_scalar range = gain_max - gain_min;
    alwan_scalar g = ALWAN_LOG2((alternate + alternate_offset) / (base + base_offset));
    alwan_scalar n = ALWAN_SELECT(range > ALWAN_ZERO,
                                  (g - gain_min) / ALWAN_SELECT(range > ALWAN_ZERO, range, ALWAN_ONE),
                                  g * ALWAN_ZERO);
    n = ALWAN_SELECT(n < ALWAN_ZERO, ALWAN_ZERO, n);
    n = ALWAN_SELECT(n > ALWAN_ONE, ALWAN_ONE, n);
    /* Limited to 1 again: a pow approximation can land an ulp above it. */
    n = ALWAN_SELECT(n > ALWAN_ZERO,
                     ALWAN_POW(ALWAN_SELECT(n > ALWAN_ZERO, n, ALWAN_ONE), gamma),
                     n * ALWAN_ZERO);
    return ALWAN_SELECT(n > ALWAN_ONE, ALWAN_ONE, n);
}

ALWAN_INLINE alwan_scalar alwan_gain_map_apply_v(alwan_scalar base,
                                                  alwan_scalar stored_gain,
                                                  alwan_scalar gain_min,
                                                  alwan_scalar gain_max,
                                                  alwan_scalar gamma,
                                                  alwan_scalar base_offset,
                                                  alwan_scalar alternate_offset,
                                                  alwan_scalar weight) {
    alwan_scalar n = ALWAN_SELECT(stored_gain > ALWAN_ZERO,
                                  ALWAN_POW(ALWAN_SELECT(stored_gain > ALWAN_ZERO, stored_gain, ALWAN_ONE),
                                            ALWAN_ONE / gamma),
                                  stored_gain * ALWAN_ZERO);
    alwan_scalar g = gain_min * (ALWAN_ONE - n) + gain_max * n;
    return (base + base_offset) * ALWAN_EXP(g * weight * ALWAN_LITERAL(0.69314718055994530942))
         - alternate_offset;
}

/* Scans a strided host buffer, so it is a CPU function: a shader reads pixels
 * from a resource, not from a pointer walked with a byte stride. Kept out of
 * the shader translation unit rather than left to fail on the pointer. */
#if ALWAN_BACKEND == ALWAN_BACKEND_C || ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
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
#endif /* CPU backends */

#endif

#endif /* ALWAN_HDR_CORE_H */
