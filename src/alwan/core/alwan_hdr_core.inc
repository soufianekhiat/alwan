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

    /* |Y_S|, as BT.2100 via colour-science's ootf_BT2100_HLG: a negative luminance (out of
     * gamut) scales by its magnitude. Until 2026-09-25 it was clamped to 1e-12. */
    ALWAN_CORE_T Ys_abs = ALWAN_CORE_ABS(Ys);
    ALWAN_CORE_T Ys_safe = ALWAN_CORE_SELECT(Ys_abs < ALWAN_CORE_LITERAL(1e-12),
                                         ALWAN_CORE_LITERAL(1e-12), Ys_abs);
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

    ALWAN_CORE_T Yd_abs = ALWAN_CORE_ABS(Yd);
    ALWAN_CORE_T Yd_safe = ALWAN_CORE_SELECT(Yd_abs < ALWAN_CORE_LITERAL(1e-12),
                                         ALWAN_CORE_LITERAL(1e-12), Yd_abs);

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
 * PU21: perceptually uniform encoding of absolute luminance
 * Mantiuk and Azimi 2021. The parameters are the published ones, from
 * gfxdisp/pu21 matlab/pu21_encoder.m (BSD-3-Clause) at 78340c0. Variant 0 is
 * banding_glare, which the authors recommend; 1 banding, 2 peaks, 3 peaks_glare.
 * The encoder there limits Y to [0.005, 10000] cd/m2 first; this is the curve alone.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_pu21_pick)(int variant, ALWAN_CORE_T banding_glare,
                                                            ALWAN_CORE_T banding, ALWAN_CORE_T peaks,
                                                            ALWAN_CORE_T peaks_glare) {
    return ALWAN_CORE_SELECT(variant == 1, banding,
           ALWAN_CORE_SELECT(variant == 2, peaks,
           ALWAN_CORE_SELECT(variant == 3, peaks_glare, banding_glare)));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_pu21_encode)(ALWAN_CORE_T Y, int variant) {
    ALWAN_CORE_T p1 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(0.353487901),
        ALWAN_CORE_LITERAL(1.070275272), ALWAN_CORE_LITERAL(1.043882782), ALWAN_CORE_LITERAL(816.885024));
    ALWAN_CORE_T p2 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(0.3734658629),
        ALWAN_CORE_LITERAL(0.4088273932), ALWAN_CORE_LITERAL(0.6459495343), ALWAN_CORE_LITERAL(1479.463946));
    ALWAN_CORE_T p3 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(8.277049286e-05),
        ALWAN_CORE_LITERAL(0.153224308), ALWAN_CORE_LITERAL(0.3194584211), ALWAN_CORE_LITERAL(0.001253215609));
    ALWAN_CORE_T p4 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(0.9062562627),
        ALWAN_CORE_LITERAL(0.2520326168), ALWAN_CORE_LITERAL(0.374025247), ALWAN_CORE_LITERAL(0.9329636822));
    ALWAN_CORE_T p5 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(0.09150303166),
        ALWAN_CORE_LITERAL(1.063512885), ALWAN_CORE_LITERAL(1.114783422), ALWAN_CORE_LITERAL(0.06746643971));
    ALWAN_CORE_T p6 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(0.9099517204),
        ALWAN_CORE_LITERAL(1.14115047), ALWAN_CORE_LITERAL(1.095360363), ALWAN_CORE_LITERAL(1.573435413));
    ALWAN_CORE_T p7 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(596.3148142),
        ALWAN_CORE_LITERAL(521.4527484), ALWAN_CORE_LITERAL(384.9217577), ALWAN_CORE_LITERAL(419.6006374));
    ALWAN_CORE_T yp = ALWAN_CORE_POW(Y, p4);
    ALWAN_CORE_T v = p7 * (ALWAN_CORE_POW((p1 + p2 * yp) / (ALWAN_CORE_ONE + p3 * yp), p5) - p6);
    return ALWAN_CORE_MAX(v, ALWAN_CORE_ZERO);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_pu21_decode)(ALWAN_CORE_T V, int variant) {
    ALWAN_CORE_T p1 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(0.353487901),
        ALWAN_CORE_LITERAL(1.070275272), ALWAN_CORE_LITERAL(1.043882782), ALWAN_CORE_LITERAL(816.885024));
    ALWAN_CORE_T p2 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(0.3734658629),
        ALWAN_CORE_LITERAL(0.4088273932), ALWAN_CORE_LITERAL(0.6459495343), ALWAN_CORE_LITERAL(1479.463946));
    ALWAN_CORE_T p3 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(8.277049286e-05),
        ALWAN_CORE_LITERAL(0.153224308), ALWAN_CORE_LITERAL(0.3194584211), ALWAN_CORE_LITERAL(0.001253215609));
    ALWAN_CORE_T p4 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(0.9062562627),
        ALWAN_CORE_LITERAL(0.2520326168), ALWAN_CORE_LITERAL(0.374025247), ALWAN_CORE_LITERAL(0.9329636822));
    ALWAN_CORE_T p5 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(0.09150303166),
        ALWAN_CORE_LITERAL(1.063512885), ALWAN_CORE_LITERAL(1.114783422), ALWAN_CORE_LITERAL(0.06746643971));
    ALWAN_CORE_T p6 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(0.9099517204),
        ALWAN_CORE_LITERAL(1.14115047), ALWAN_CORE_LITERAL(1.095360363), ALWAN_CORE_LITERAL(1.573435413));
    ALWAN_CORE_T p7 = ALWAN_CORE_FNVLIT(alwan_pu21_pick)(variant, ALWAN_CORE_LITERAL(596.3148142),
        ALWAN_CORE_LITERAL(521.4527484), ALWAN_CORE_LITERAL(384.9217577), ALWAN_CORE_LITERAL(419.6006374));
    ALWAN_CORE_T vp = ALWAN_CORE_POW(ALWAN_CORE_MAX(V / p7 + p6, ALWAN_CORE_ZERO), ALWAN_CORE_ONE / p5);
    return ALWAN_CORE_POW(ALWAN_CORE_MAX(vp - p1, ALWAN_CORE_ZERO) / (p2 - p3 * vp), ALWAN_CORE_ONE / p4);
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
 *
 * Report ITU-R BT.2446-1 (2021), section 4.1, Tables 2 and 3, on BT.2020 RGB. Input: linear
 * display-light HDR RGB normalised to the mastering peak L_hdr (1 = L_hdr); output: the SDR
 * R'G'B' (BT.2020 primaries, gamma-encoded) that the Report's Y'Cb'Cr' converts to through
 * Recommendation BT.2020's matrix, not clipped. Negative input is taken as 0.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_bt2446a_forward)(ALWAN_CORE_VEC3 rgb,
                                                    ALWAN_CORE_T L_hdr,
                                                    ALWAN_CORE_T L_sdr) {
    ALWAN_CORE_T inv24 = ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(2.4);
    ALWAN_CORE_T Rp = ALWAN_CORE_POW(ALWAN_CORE_MAX(rgb.v[0], ALWAN_CORE_ZERO), inv24);
    ALWAN_CORE_T Gp = ALWAN_CORE_POW(ALWAN_CORE_MAX(rgb.v[1], ALWAN_CORE_ZERO), inv24);
    ALWAN_CORE_T Bp = ALWAN_CORE_POW(ALWAN_CORE_MAX(rgb.v[2], ALWAN_CORE_ZERO), inv24);
    ALWAN_CORE_T Yp = ALWAN_CORE_LITERAL(0.2627) * Rp + ALWAN_CORE_LITERAL(0.6780) * Gp + ALWAN_CORE_LITERAL(0.0593) * Bp;

    ALWAN_CORE_T rho_hdr = ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(32.0) *
        ALWAN_CORE_POW(L_hdr / ALWAN_CORE_LITERAL(10000.0), inv24);
    ALWAN_CORE_T rho_sdr = ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(32.0) *
        ALWAN_CORE_POW(L_sdr / ALWAN_CORE_LITERAL(10000.0), inv24);

    /* Step 1: to the perceptual domain */
    ALWAN_CORE_T Ypp = ALWAN_CORE_LN(ALWAN_CORE_ONE + (rho_hdr - ALWAN_CORE_ONE) * Yp) / ALWAN_CORE_LN(rho_hdr);
    /* Step 2: the knee */
    ALWAN_CORE_T c1 = ALWAN_CORE_LITERAL(1.0770) * Ypp;
    ALWAN_CORE_T c2 = ALWAN_CORE_LITERAL(-1.1510) * Ypp * Ypp + ALWAN_CORE_LITERAL(2.7811) * Ypp - ALWAN_CORE_LITERAL(0.6302);
    ALWAN_CORE_T c3 = ALWAN_CORE_LITERAL(0.5000) * Ypp + ALWAN_CORE_LITERAL(0.5000);
    ALWAN_CORE_T Yc = ALWAN_CORE_SELECT(Ypp <= ALWAN_CORE_LITERAL(0.7399), c1,
                      ALWAN_CORE_SELECT(Ypp < ALWAN_CORE_LITERAL(0.9909), c2, c3));
    /* Step 3: back to the gamma domain */
    ALWAN_CORE_T Ysdr = (ALWAN_CORE_POW(rho_sdr, Yc) - ALWAN_CORE_ONE) / (rho_sdr - ALWAN_CORE_ONE);

    /* Table 3: colour correction */
    ALWAN_CORE_T f = ALWAN_CORE_SELECT(Yp > ALWAN_CORE_ZERO, Ysdr / (ALWAN_CORE_LITERAL(1.1) * Yp), ALWAN_CORE_ZERO);
    ALWAN_CORE_T Cb = f * (Bp - Yp) / ALWAN_CORE_LITERAL(1.8814);
    ALWAN_CORE_T Cr = f * (Rp - Yp) / ALWAN_CORE_LITERAL(1.4746);
    ALWAN_CORE_T Ytmo = Ysdr - ALWAN_CORE_MAX(ALWAN_CORE_LITERAL(0.1) * Cr, ALWAN_CORE_ZERO);

    ALWAN_CORE_VEC3 res;
    res.v[0] = Ytmo + ALWAN_CORE_LITERAL(1.4746) * Cr;
    res.v[1] = Ytmo - ALWAN_CORE_LITERAL(0.16455) * Cb - ALWAN_CORE_LITERAL(0.57135) * Cr;
    res.v[2] = Ytmo + ALWAN_CORE_LITERAL(1.8814) * Cb;
    return res;
}

/* ================================================================
 * BT.2446 Method A: SDR to HDR Inverse Mapping
 *
 * Report ITU-R BT.2446-1, section 4.2, Table 4: SDR R'G'B' (BT.2020, full range) to linear
 * display-light HDR RGB normalised to its fixed peak of 1 000 cd/m2 (1 = 1 000 cd/m2).
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_bt2446a_inverse)(ALWAN_CORE_VEC3 rgb) {
    ALWAN_CORE_T Yp = ALWAN_CORE_LITERAL(0.2627) * rgb.v[0] + ALWAN_CORE_LITERAL(0.6780) * rgb.v[1]
                    + ALWAN_CORE_LITERAL(0.0593) * rgb.v[2];
    ALWAN_CORE_T Cb = (rgb.v[2] - Yp) / ALWAN_CORE_LITERAL(1.8814);
    ALWAN_CORE_T Cr = (rgb.v[0] - Yp) / ALWAN_CORE_LITERAL(1.4746);

    ALWAN_CORE_T Ypp = ALWAN_CORE_LITERAL(255.0) * Yp;
    ALWAN_CORE_T E1 = ALWAN_CORE_LITERAL(1.8712e-5) * Ypp * Ypp + ALWAN_CORE_LITERAL(-2.7334e-3) * Ypp + ALWAN_CORE_LITERAL(1.3141);
    ALWAN_CORE_T E2 = ALWAN_CORE_LITERAL(2.8305e-6) * Ypp * Ypp + ALWAN_CORE_LITERAL(-7.4622e-4) * Ypp + ALWAN_CORE_LITERAL(1.2528);
    ALWAN_CORE_T E = ALWAN_CORE_SELECT(Ypp <= ALWAN_CORE_LITERAL(70.0), E1, E2);
    ALWAN_CORE_T Yhdr = ALWAN_CORE_POW(ALWAN_CORE_MAX(Ypp, ALWAN_CORE_ZERO), E);

    ALWAN_CORE_T Sc = ALWAN_CORE_SELECT(Yp > ALWAN_CORE_ZERO, ALWAN_CORE_LITERAL(1.075) * Yhdr / Yp, ALWAN_CORE_ONE);
    ALWAN_CORE_T Cbh = Cb * Sc;
    ALWAN_CORE_T Crh = Cr * Sc;

    ALWAN_CORE_T r = ALWAN_CORE_CLAMP(Yhdr + ALWAN_CORE_LITERAL(1.4746) * Crh, ALWAN_CORE_ZERO, ALWAN_CORE_LITERAL(1000.0));
    ALWAN_CORE_T g = ALWAN_CORE_CLAMP(Yhdr - ALWAN_CORE_LITERAL(0.16455) * Cbh - ALWAN_CORE_LITERAL(0.57135) * Crh,
                                      ALWAN_CORE_ZERO, ALWAN_CORE_LITERAL(1000.0));
    ALWAN_CORE_T b = ALWAN_CORE_CLAMP(Yhdr + ALWAN_CORE_LITERAL(1.8814) * Cbh, ALWAN_CORE_ZERO, ALWAN_CORE_LITERAL(1000.0));

    ALWAN_CORE_VEC3 res;
    res.v[0] = ALWAN_CORE_POW(r / ALWAN_CORE_LITERAL(1000.0), ALWAN_CORE_LITERAL(2.4));
    res.v[1] = ALWAN_CORE_POW(g / ALWAN_CORE_LITERAL(1000.0), ALWAN_CORE_LITERAL(2.4));
    res.v[2] = ALWAN_CORE_POW(b / ALWAN_CORE_LITERAL(1000.0), ALWAN_CORE_LITERAL(2.4));
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

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_bt2446c_hlg_luminance)(ALWAN_CORE_T E, ALWAN_CORE_T L_hdr) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.17883277);
    ALWAN_CORE_T b = ALWAN_CORE_ONE - ALWAN_CORE_LITERAL(4.0) * a;
    ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.5) - a * ALWAN_CORE_LN(ALWAN_CORE_LITERAL(4.0) * a);
    ALWAN_CORE_T lo = E * E / ALWAN_CORE_LITERAL(3.0);
    ALWAN_CORE_T hi = (ALWAN_CORE_EXP((E - c) / a) + b) / ALWAN_CORE_LITERAL(12.0);
    ALWAN_CORE_T s = ALWAN_CORE_SELECT(E <= ALWAN_CORE_LITERAL(0.5), lo, hi);
    ALWAN_CORE_T gamma = ALWAN_CORE_LITERAL(1.2) + ALWAN_CORE_LITERAL(0.42) * ALWAN_CORE_LOG10(L_hdr / ALWAN_CORE_LITERAL(1000.0));
    return L_hdr * ALWAN_CORE_POW(s, gamma);
}

/* k1, k2, k3, k4 and the inflection point Y_ip (cd/m2) in v[0..4]. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_bt2446c_k3_residual)(ALWAN_CORE_T u, ALWAN_CORE_T c, ALWAN_CORE_T t) {
    /* u = 1 - k3: u ln(1 + c / u) - t, increasing in u from -t towards c - t */
    return u * ALWAN_CORE_LN(ALWAN_CORE_ONE + c / u) - t;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_bt2446c_curve)(ALWAN_CORE_T Y, ALWAN_CORE_T L_hdr, ALWAN_CORE_T L_sdr) {
    ALWAN_CORE_T k1 = L_sdr * ALWAN_CORE_POW(ALWAN_CORE_LITERAL(0.7), ALWAN_CORE_LITERAL(2.4))
                    / ALWAN_CORE_FNVLIT(alwan_bt2446c_hlg_luminance)(ALWAN_CORE_LITERAL(0.5), L_hdr);
    ALWAN_CORE_T Yip = L_sdr * ALWAN_CORE_POW(ALWAN_CORE_LITERAL(0.8), ALWAN_CORE_LITERAL(2.4)) / k1;
    ALWAN_CORE_T Yref = ALWAN_CORE_FNVLIT(alwan_bt2446c_hlg_luminance)(ALWAN_CORE_LITERAL(0.75), L_hdr);
    ALWAN_CORE_T Ywp = L_sdr * ALWAN_CORE_POW(ALWAN_CORE_LITERAL(0.96), ALWAN_CORE_LITERAL(2.4));
    /* equation (9) with k2 and k4 from (7) and (8): k1 Y_ip u ln((R - 1 + u) / u) = Ywp - k1 Y_ip */
    ALWAN_CORE_T c = Yref / Yip - ALWAN_CORE_ONE;
    ALWAN_CORE_T t = (Ywp - k1 * Yip) / (k1 * Yip);
    ALWAN_CORE_T lo = ALWAN_CORE_ZERO;
    ALWAN_CORE_T hi = t * c / (c - t);   /* u ln(1 + c/u) >= c u / (u + c) bounds the root */
    int i;
    for (i = 0; i < 80; i++) {
        ALWAN_CORE_T mid = (lo + hi) * ALWAN_CORE_LITERAL(0.5);
        ALWAN_CORE_T r = ALWAN_CORE_FNVLIT(alwan_bt2446c_k3_residual)(mid, c, t);
        lo = ALWAN_CORE_SELECT(r < ALWAN_CORE_ZERO, mid, lo);
        hi = ALWAN_CORE_SELECT(r < ALWAN_CORE_ZERO, hi, mid);
    }
    ALWAN_CORE_T u = (lo + hi) * ALWAN_CORE_LITERAL(0.5);
    ALWAN_CORE_T k3 = ALWAN_CORE_ONE - u;
    ALWAN_CORE_T k2 = k1 * u * Yip;
    ALWAN_CORE_T k4 = k1 * Yip - k2 * ALWAN_CORE_LN(u);
    ALWAN_CORE_T lin = k1 * Y;
    ALWAN_CORE_T arg = ALWAN_CORE_MAX(Y / Yip - k3, u);   /* equal to Y / Yip - k3 wherever it is used */
    ALWAN_CORE_T lg = k2 * ALWAN_CORE_LN(arg) + k4;
    return ALWAN_CORE_SELECT(Y < Yip, lin, lg);
}

/* The scalar: HDR display luminance over L_hdr in, SDR display luminance over L_sdr out, not
 * clipped (the brightest HDR input lands at 1.18 for 1 000 and 100 cd/m2). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_bt2446c_forward)(ALWAN_CORE_T Y_hdr,
                                                    ALWAN_CORE_T L_hdr,
                                                    ALWAN_CORE_T L_sdr) {
    return ALWAN_CORE_FNVLIT(alwan_bt2446c_curve)(Y_hdr * L_hdr, L_hdr, L_sdr) / L_sdr;
}

/* The whole of section 6.1 for BT.2020 signals: HLG R'G'B' in (displayed at 1 000 cd/m2,
 * gamma 1.2), crosstalk alpha, BT.2020 XYZ, the curve on Y with x and y kept, the Report's
 * XYZ to RGB, the inverse crosstalk, and BT.1886 at 100 cd/m2; SDR R'G'B' out, clipped to
 * [0, 1]. Black gives black. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_bt2446c_rgb)(ALWAN_CORE_VEC3 hlg, ALWAN_CORE_T alpha) {
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.17883277);
    ALWAN_CORE_T bb = ALWAN_CORE_ONE - ALWAN_CORE_LITERAL(4.0) * a;
    ALWAN_CORE_T cc = ALWAN_CORE_LITERAL(0.5) - a * ALWAN_CORE_LN(ALWAN_CORE_LITERAL(4.0) * a);
    ALWAN_CORE_T e0 = ALWAN_CORE_MAX(hlg.v[0], ALWAN_CORE_ZERO);
    ALWAN_CORE_T e1 = ALWAN_CORE_MAX(hlg.v[1], ALWAN_CORE_ZERO);
    ALWAN_CORE_T e2 = ALWAN_CORE_MAX(hlg.v[2], ALWAN_CORE_ZERO);
    ALWAN_CORE_T s0 = ALWAN_CORE_SELECT(e0 <= ALWAN_CORE_LITERAL(0.5), e0 * e0 / ALWAN_CORE_LITERAL(3.0),
                                        (ALWAN_CORE_EXP((e0 - cc) / a) + bb) / ALWAN_CORE_LITERAL(12.0));
    ALWAN_CORE_T s1 = ALWAN_CORE_SELECT(e1 <= ALWAN_CORE_LITERAL(0.5), e1 * e1 / ALWAN_CORE_LITERAL(3.0),
                                        (ALWAN_CORE_EXP((e1 - cc) / a) + bb) / ALWAN_CORE_LITERAL(12.0));
    ALWAN_CORE_T s2 = ALWAN_CORE_SELECT(e2 <= ALWAN_CORE_LITERAL(0.5), e2 * e2 / ALWAN_CORE_LITERAL(3.0),
                                        (ALWAN_CORE_EXP((e2 - cc) / a) + bb) / ALWAN_CORE_LITERAL(12.0));
    ALWAN_CORE_T Ys = ALWAN_CORE_LITERAL(0.2627) * s0 + ALWAN_CORE_LITERAL(0.6780) * s1 + ALWAN_CORE_LITERAL(0.0593) * s2;
    ALWAN_CORE_T gain = ALWAN_CORE_SELECT(Ys > ALWAN_CORE_ZERO,
                                          ALWAN_CORE_LITERAL(1000.0) * ALWAN_CORE_POW(Ys, ALWAN_CORE_LITERAL(0.2)),
                                          ALWAN_CORE_ZERO);
    ALWAN_CORE_T R = gain * s0, G = gain * s1, B = gain * s2;

    ALWAN_CORE_T d = ALWAN_CORE_ONE - ALWAN_CORE_LITERAL(2.0) * alpha;
    ALWAN_CORE_T Rx = d * R + alpha * G + alpha * B;
    ALWAN_CORE_T Gx = alpha * R + d * G + alpha * B;
    ALWAN_CORE_T Bx = alpha * R + alpha * G + d * B;

    ALWAN_CORE_T X = ALWAN_CORE_LITERAL(0.6370) * Rx + ALWAN_CORE_LITERAL(0.1446) * Gx + ALWAN_CORE_LITERAL(0.1689) * Bx;
    ALWAN_CORE_T Y = ALWAN_CORE_LITERAL(0.2627) * Rx + ALWAN_CORE_LITERAL(0.6780) * Gx + ALWAN_CORE_LITERAL(0.0593) * Bx;
    ALWAN_CORE_T Z = ALWAN_CORE_LITERAL(0.0281) * Gx + ALWAN_CORE_LITERAL(1.0610) * Bx;
    ALWAN_CORE_T sum = X + Y + Z;
    ALWAN_CORE_T valid = ALWAN_CORE_SELECT(Y > ALWAN_CORE_ZERO, ALWAN_CORE_ONE, ALWAN_CORE_ZERO);
    ALWAN_CORE_T sum_s = ALWAN_CORE_SELECT(sum > ALWAN_CORE_ZERO, sum, ALWAN_CORE_ONE);
    ALWAN_CORE_T x = X / sum_s;
    ALWAN_CORE_T y = ALWAN_CORE_SELECT(Y > ALWAN_CORE_ZERO, Y / sum_s, ALWAN_CORE_ONE);

    ALWAN_CORE_T Ysdr = ALWAN_CORE_FNVLIT(alwan_bt2446c_curve)(Y, ALWAN_CORE_LITERAL(1000.0), ALWAN_CORE_LITERAL(100.0)) * valid;
    ALWAN_CORE_T Xs = x / y * Ysdr;
    ALWAN_CORE_T Zs = (ALWAN_CORE_ONE - x - y) / y * Ysdr;

    ALWAN_CORE_T Rxs = ALWAN_CORE_LITERAL(1.7167) * Xs + ALWAN_CORE_LITERAL(-0.3557) * Ysdr + ALWAN_CORE_LITERAL(-0.2534) * Zs;
    ALWAN_CORE_T Gxs = ALWAN_CORE_LITERAL(-0.6667) * Xs + ALWAN_CORE_LITERAL(1.6165) * Ysdr + ALWAN_CORE_LITERAL(0.0158) * Zs;
    ALWAN_CORE_T Bxs = ALWAN_CORE_LITERAL(0.0176) * Xs + ALWAN_CORE_LITERAL(-0.0428) * Ysdr + ALWAN_CORE_LITERAL(0.9421) * Zs;

    ALWAN_CORE_T inv = ALWAN_CORE_ONE / (ALWAN_CORE_ONE - ALWAN_CORE_LITERAL(3.0) * alpha);
    ALWAN_CORE_T Rs = inv * ((ALWAN_CORE_ONE - alpha) * Rxs - alpha * Gxs - alpha * Bxs);
    ALWAN_CORE_T Gs = inv * (-alpha * Rxs + (ALWAN_CORE_ONE - alpha) * Gxs - alpha * Bxs);
    ALWAN_CORE_T Bs = inv * (-alpha * Rxs - alpha * Gxs + (ALWAN_CORE_ONE - alpha) * Bxs);

    ALWAN_CORE_T inv24 = ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(2.4);
    ALWAN_CORE_VEC3 res;
    res.v[0] = ALWAN_CORE_SATURATE(ALWAN_CORE_POW(ALWAN_CORE_MAX(Rs / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_ZERO), inv24));
    res.v[1] = ALWAN_CORE_SATURATE(ALWAN_CORE_POW(ALWAN_CORE_MAX(Gs / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_ZERO), inv24));
    res.v[2] = ALWAN_CORE_SATURATE(ALWAN_CORE_POW(ALWAN_CORE_MAX(Bs / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_ZERO), inv24));
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

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_bt2390_eetf)(ALWAN_CORE_T E_pq,
                                                ALWAN_CORE_T LB,
                                                ALWAN_CORE_T LW,
                                                ALWAN_CORE_T LB_target,
                                                ALWAN_CORE_T LW_target) {
    ALWAN_CORE_T range = LW - LB;
    ALWAN_CORE_T E1 = ALWAN_CORE_CLAMP((E_pq - LB) / range, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    ALWAN_CORE_T minLum = (LB_target - LB) / range;
    ALWAN_CORE_T maxLum = (LW_target - LB) / range;
    ALWAN_CORE_T KS = ALWAN_CORE_LITERAL(1.5) * maxLum - ALWAN_CORE_LITERAL(0.5);

    /* a target at least as bright as the source leaves the signal alone (KS >= 1) */
    ALWAN_CORE_T den = ALWAN_CORE_SELECT(KS < ALWAN_CORE_ONE, ALWAN_CORE_ONE - KS, ALWAN_CORE_ONE);
    ALWAN_CORE_T T = (E1 - KS) / den;
    ALWAN_CORE_T T2 = T * T;
    ALWAN_CORE_T T3 = T2 * T;
    ALWAN_CORE_T P = (ALWAN_CORE_LITERAL(2.0) * T3 - ALWAN_CORE_LITERAL(3.0) * T2 + ALWAN_CORE_ONE) * KS
                   + (T3 - ALWAN_CORE_LITERAL(2.0) * T2 + T) * (ALWAN_CORE_ONE - KS)
                   + (-ALWAN_CORE_LITERAL(2.0) * T3 + ALWAN_CORE_LITERAL(3.0) * T2) * maxLum;
    ALWAN_CORE_T E2 = ALWAN_CORE_SELECT(E1 < KS || KS >= ALWAN_CORE_ONE, E1, P);
    ALWAN_CORE_T om = ALWAN_CORE_ONE - E2;
    ALWAN_CORE_T om2 = om * om;
    ALWAN_CORE_T E3 = E2 + minLum * om2 * om2;
    return E3 * range + LB;
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
                                       ALWAN_CORE_PARAM_ARRAY_OUT(ALWAN_CORE_T, out_primaries_xy, 6),
                                       ALWAN_CORE_PARAM_ARRAY_OUT(ALWAN_CORE_T, out_white_xy, 2),
                                       ALWAN_CORE_PARAM_OUT out_max_lum,
                                       ALWAN_CORE_PARAM_OUT out_min_lum) {
    out_primaries_xy[0] = display_primaries_xy[0];
    out_primaries_xy[1] = display_primaries_xy[1];
    out_primaries_xy[2] = display_primaries_xy[2];
    out_primaries_xy[3] = display_primaries_xy[3];
    out_primaries_xy[4] = display_primaries_xy[4];
    out_primaries_xy[5] = display_primaries_xy[5];
    out_white_xy[0] = white_point_xy[0];
    out_white_xy[1] = white_point_xy[1];
    ALWAN_CORE_REF(out_max_lum) = max_luminance;
    ALWAN_CORE_REF(out_min_lum) = min_luminance;
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

/* ================================================================
 * ISO 21496-1 gain maps
 *
 * A gain map holds, per pixel and channel, the log2 ratio of two renditions of one
 * picture, each offset so black keeps a finite ratio:
 *
 *     G = log2((alternate + k_alt) / (base + k_base))
 *
 * stored normalised to [0, 1] between gain_map_min and gain_map_max and raised to
 * gamma. A display with headroom H (log2 of its peak over SDR white) applies the
 * fraction W = (H - H_base) / (H_alt - H_base), limited to [0, 1]:
 *
 *     out = (base + k_base) * 2^(G W) - k_alt
 *
 * The limits are the standard's: a stored gain lies in [0, 1], and a display never
 * extrapolates past either rendition. A NaN ratio (a value below minus its offset)
 * stays NaN.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_gain_map_weight)(ALWAN_CORE_T display_headroom,
                                                                  ALWAN_CORE_T base_headroom,
                                                                  ALWAN_CORE_T alternate_headroom) {
    ALWAN_CORE_T w = (display_headroom - base_headroom) / (alternate_headroom - base_headroom);
    w = ALWAN_CORE_SELECT(w < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, w);
    return ALWAN_CORE_SELECT(w > ALWAN_CORE_ONE, ALWAN_CORE_ONE, w);
}

/* One channel's stored gain. gain_max == gain_min stores 0. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_gain_map_encode)(ALWAN_CORE_T base,
                                                                  ALWAN_CORE_T alternate,
                                                                  ALWAN_CORE_T gain_min,
                                                                  ALWAN_CORE_T gain_max,
                                                                  ALWAN_CORE_T gamma,
                                                                  ALWAN_CORE_T base_offset,
                                                                  ALWAN_CORE_T alternate_offset) {
    ALWAN_CORE_T range = gain_max - gain_min;
    ALWAN_CORE_T g = ALWAN_CORE_LOG2((alternate + alternate_offset) / (base + base_offset));
    ALWAN_CORE_T n = ALWAN_CORE_SELECT(range > ALWAN_CORE_ZERO,
                                       (g - gain_min) / ALWAN_CORE_SELECT(range > ALWAN_CORE_ZERO, range, ALWAN_CORE_ONE),
                                       g * ALWAN_CORE_ZERO);
    n = ALWAN_CORE_SELECT(n < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, n);
    n = ALWAN_CORE_SELECT(n > ALWAN_CORE_ONE, ALWAN_CORE_ONE, n);
    /* The stored range is [0, 1] by definition, and the deterministic pow gives
     * pow(1, 1) = 1 + 1 ulp, so the result is limited to 1 again. A NaN stays NaN. */
    n = ALWAN_CORE_SELECT(n > ALWAN_CORE_ZERO,
                          ALWAN_CORE_POW(ALWAN_CORE_SELECT(n > ALWAN_CORE_ZERO, n, ALWAN_CORE_ONE), gamma),
                          n * ALWAN_CORE_ZERO);
    return ALWAN_CORE_SELECT(n > ALWAN_CORE_ONE, ALWAN_CORE_ONE, n);
}

/* One channel of the rendition at weight W, from a stored gain. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_gain_map_apply)(ALWAN_CORE_T base,
                                                                 ALWAN_CORE_T stored_gain,
                                                                 ALWAN_CORE_T gain_min,
                                                                 ALWAN_CORE_T gain_max,
                                                                 ALWAN_CORE_T gamma,
                                                                 ALWAN_CORE_T base_offset,
                                                                 ALWAN_CORE_T alternate_offset,
                                                                 ALWAN_CORE_T weight) {
    ALWAN_CORE_T n = ALWAN_CORE_SELECT(stored_gain > ALWAN_CORE_ZERO,
                                       ALWAN_CORE_POW(ALWAN_CORE_SELECT(stored_gain > ALWAN_CORE_ZERO,
                                                                        stored_gain, ALWAN_CORE_ONE),
                                                      ALWAN_CORE_ONE / gamma),
                                       stored_gain * ALWAN_CORE_ZERO);
    ALWAN_CORE_T g = gain_min * (ALWAN_CORE_ONE - n) + gain_max * n;
    return (base + base_offset) * ALWAN_CORE_EXP(g * weight * ALWAN_CORE_LITERAL(0.69314718055994530942))
         - alternate_offset;
}
