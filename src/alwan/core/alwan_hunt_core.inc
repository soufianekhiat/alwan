/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Hunt Color Appearance Model (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Hunt (1991, 1995)
 * "Revised colour-appearance model for related and unrelated colours"
 */

/* ----------------------------------------------------------------
 * Hunt Appearance Correlates (local result struct)
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T J;    /* Lightness */
    ALWAN_CORE_T C;    /* Chroma */
    ALWAN_CORE_T h;    /* Hue angle (degrees) */
    ALWAN_CORE_T s;    /* Saturation */
    ALWAN_CORE_T Q;    /* Brightness */
    ALWAN_CORE_T M;    /* Colourfulness */
} ALWAN_CORE_FNLIT(alwan_hunt_v_correlates);

/* ----------------------------------------------------------------
 * Hunt-Pointer-Estevez Matrix
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(HUNT_V_M_HPE) = {{
#include "../data/matrices/hpe.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Nonlinear response function f_n (branchless)
 *
 * f_n(x) = sign(x) * 40 * |x|^0.73 / (|x|^0.73 + 2)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hunt_fn)(ALWAN_CORE_T x) {
    ALWAN_CORE_T ax = ALWAN_CORE_ABS(x);
    ALWAN_CORE_T ax_pow = ALWAN_CORE_POW(ax, ALWAN_CORE_LITERAL(0.73));
    ALWAN_CORE_T magnitude = ALWAN_CORE_LITERAL(40.0) * ax_pow / (ax_pow + ALWAN_CORE_LITERAL(2.0));
    ALWAN_CORE_T sign = ALWAN_CORE_SELECT(x >= ALWAN_CORE_ZERO, ALWAN_CORE_ONE, -ALWAN_CORE_ONE);
    return sign * magnitude;
}

/* ----------------------------------------------------------------
 * Hunt Colour Appearance Model (Hunt 1995)
 *
 * The full model, as published and as implemented by
 * colour.appearance.XYZ_to_Hunt.
 *
 * What this replaced, on 2026-08-27: chromatic adaptation written as
 * (D + 1 - D) * lms, which is the algebraic identity; the white point's LMS and
 * both surround induction factors computed and then discarded through
 * ALWAN_UNUSED; the eccentricity factor omitted; and ad-hoc expressions for
 * brightness, lightness and chroma, including a white-point brightness fixed at
 * Qw = N1 * 2.0. Every correlate disagreed with Fairchild's worked example by
 * one to two orders of magnitude.
 *
 * Steps, following the reference:
 *   F_L      luminance level adaptation factor
 *   rgb_a    chromatic adaptation, with cone bleaching and optional
 *            Helson-Judd and proximal-field terms
 *   A_a      achromatic post-adaptation signal
 *   C_1..3   colour difference signals
 *   h        hue angle, e_s eccentricity, F_t low-luminance tritanopia
 *   M        overall chromatic response from M_rg and M_yb
 *   s        saturation
 *   A        achromatic signal, including the scotopic rod term A_S
 *   Q, J     brightness and lightness against the white's own Q
 *   C_94     chroma, M_94 colourfulness
 * ---------------------------------------------------------------- */

/* Surround induction factors, per Hunt. The API layer resolved these before
 * 2026-08-27; they live here now so the core is self-contained on the GPU
 * paths, which have no API layer to call. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hunt_surround_Nc)(alwan_hunt_surround s) {
    return (s == ALWAN_HUNT_SURROUND_DARK) ? ALWAN_CORE_LITERAL(0.7) : ALWAN_CORE_LITERAL(1.0);
}
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hunt_surround_Nb)(alwan_hunt_surround s) {
    switch (s) {
        case ALWAN_HUNT_SURROUND_DIM:  return ALWAN_CORE_LITERAL(25.0);
        case ALWAN_HUNT_SURROUND_DARK: return ALWAN_CORE_LITERAL(25.0);
        case ALWAN_HUNT_SURROUND_NORMAL:
        default:                       return ALWAN_CORE_LITERAL(75.0);
    }
}
/* Hunt's nonlinearity: 40 * (x^0.73 / (x^0.73 + 2)). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hunt_f_n)(ALWAN_CORE_T x) {
    ALWAN_CORE_T const xp = ALWAN_CORE_POW(ALWAN_CORE_MAX(x, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.73));
    return ALWAN_CORE_LITERAL(40.0) * (xp / (xp + ALWAN_CORE_LITERAL(2.0)));
}

/* Eccentricity factor: piecewise-linear over Hunt's four hue anchors, with the
 * published extrapolations below 20.14 and above 237.53 degrees. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hunt_eccentricity)(ALWAN_CORE_T hue) {
    ALWAN_CORE_T const h_s0 = ALWAN_CORE_LITERAL(20.14), h_s1 = ALWAN_CORE_LITERAL(90.0), h_s2 = ALWAN_CORE_LITERAL(164.25), h_s3 = ALWAN_CORE_LITERAL(237.53);
    ALWAN_CORE_T const e_s0 = ALWAN_CORE_LITERAL(0.8),   e_s1 = ALWAN_CORE_LITERAL(0.7),  e_s2 = ALWAN_CORE_LITERAL(1.0),    e_s3 = ALWAN_CORE_LITERAL(1.2);
    ALWAN_CORE_T interp;
    if (hue <= h_s1) {
        interp = e_s0 + (e_s1 - e_s0) * (hue - h_s0) / (h_s1 - h_s0);
    } else if (hue <= h_s2) {
        interp = e_s1 + (e_s2 - e_s1) * (hue - h_s1) / (h_s2 - h_s1);
    } else {
        interp = e_s2 + (e_s3 - e_s2) * (hue - h_s2) / (h_s3 - h_s2);
    }
    if (hue < h_s0) {
        interp = ALWAN_CORE_LITERAL(0.856) - (hue / h_s0) * ALWAN_CORE_LITERAL(0.056);
    } else if (hue > h_s3) {
        interp = ALWAN_CORE_LITERAL(0.856) + ALWAN_CORE_LITERAL(0.344) * (ALWAN_CORE_LITERAL(360.0) - hue) / (ALWAN_CORE_LITERAL(360.0) - h_s3);
    }
    return interp;
}

/* Chromatic adaptation, shared by the stimulus and the reference white. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(hunt_adapt)(
    ALWAN_CORE_VEC3 rgb, ALWAN_CORE_VEC3 rgb_w, ALWAN_CORE_T Y_w, ALWAN_CORE_T Y_b, ALWAN_CORE_T La, ALWAN_CORE_T FL,
    ALWAN_CORE_VEC3 rgb_p, ALWAN_CORE_T p, int helson_judd, int discount) {

    ALWAN_CORE_T const rgb_w_sum = rgb_w.v[0] + rgb_w.v[1] + rgb_w.v[2];
    ALWAN_CORE_T const sum_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(rgb_w_sum) > ALWAN_CORE_LITERAL(1e-12), rgb_w_sum, ALWAN_CORE_ONE);
    ALWAN_CORE_T h_rgb[3], F_rgb[3], D_rgb[3], B_rgb[3], w[3];
    ALWAN_CORE_VEC3 result;
    int i;

    for (i = 0; i < 3; i++) {
        h_rgb[i] = ALWAN_CORE_LITERAL(3.0) * rgb_w.v[i] / sum_safe;
    }

    if (discount) {
        for (i = 0; i < 3; i++) F_rgb[i] = ALWAN_CORE_ONE;
    } else {
        ALWAN_CORE_T const La_p = ALWAN_CORE_POW(ALWAN_CORE_MAX(La, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));
        for (i = 0; i < 3; i++) {
            F_rgb[i] = (ALWAN_CORE_ONE + La_p + h_rgb[i]) / (ALWAN_CORE_ONE + La_p + ALWAN_CORE_ONE / h_rgb[i]);
        }
    }

    if (helson_judd) {
        ALWAN_CORE_T const Y_b_Y_w = Y_b / ALWAN_CORE_SELECT(ALWAN_CORE_ABS(Y_w) > ALWAN_CORE_LITERAL(1e-12), Y_w, ALWAN_CORE_ONE);
        ALWAN_CORE_T const ref = ALWAN_CORE_FNVLIT(hunt_f_n)(Y_b_Y_w * FL * F_rgb[1]);
        for (i = 0; i < 3; i++) {
            D_rgb[i] = ref - ALWAN_CORE_FNVLIT(hunt_f_n)(Y_b_Y_w * FL * F_rgb[i]);
        }
    } else {
        for (i = 0; i < 3; i++) D_rgb[i] = ALWAN_CORE_LITERAL(0.0);
    }

    /* Cone bleaching. */
    for (i = 0; i < 3; i++) {
        B_rgb[i] = ALWAN_CORE_LITERAL(1.0e7) / (ALWAN_CORE_LITERAL(1.0e7) + ALWAN_CORE_LITERAL(5.0) * La * (rgb_w.v[i] / ALWAN_CORE_LITERAL(100.0)));
        w[i] = rgb_w.v[i];
    }

    /* Proximal field adjustment of the reference white, when p is given. */
    if (p != ALWAN_CORE_LITERAL(0.0)) {
        ALWAN_CORE_T const p_rgb_sum = rgb_p.v[0] + rgb_p.v[1] + rgb_p.v[2];
        ALWAN_CORE_T const p_sum_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(p_rgb_sum) > ALWAN_CORE_LITERAL(1e-12), p_rgb_sum, ALWAN_CORE_ONE);
        for (i = 0; i < 3; i++) {
            ALWAN_CORE_T const p_rgb = ALWAN_CORE_LITERAL(3.0) * rgb_p.v[i] / p_sum_safe;
            w[i] = rgb_w.v[i] * ((ALWAN_CORE_LITERAL(1.0) - p) * p_rgb + (ALWAN_CORE_LITERAL(1.0) + p) / p_rgb) /
                   ((ALWAN_CORE_LITERAL(1.0) + p) * p_rgb + (ALWAN_CORE_LITERAL(1.0) - p) / p_rgb);
        }
    }

    for (i = 0; i < 3; i++) {
        ALWAN_CORE_T const denom = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(w[i]) > ALWAN_CORE_LITERAL(1e-12), w[i], ALWAN_CORE_ONE);
        result.v[i] = ALWAN_CORE_ONE + B_rgb[i] * (ALWAN_CORE_FNVLIT(hunt_f_n)(FL * F_rgb[i] * rgb.v[i] / denom) + D_rgb[i]);
    }
    return result;
}

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_hunt_v_correlates) ALWAN_CORE_FNVLIT(alwan_hunt_forward)(ALWAN_CORE_XYZ xyz, ALWAN_CORE_HUNT_VIEWING_CONDITIONS vc) {
    ALWAN_CORE_FNLIT(alwan_hunt_v_correlates) result;

    ALWAN_CORE_T const Y_w = vc.xyz_w.y;
    ALWAN_CORE_T const Y_b_in = (vc.xyz_b.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_b.y : vc.Yb;
    ALWAN_CORE_T const Y_b = (Y_b_in > ALWAN_CORE_LITERAL(0.0)) ? Y_b_in : ALWAN_CORE_LITERAL(20.0);
    ALWAN_CORE_T const La = vc.La;
    ALWAN_CORE_T const N_c = ALWAN_CORE_FNVLIT(hunt_surround_Nc)(vc.surround);
    ALWAN_CORE_T const N_b = ALWAN_CORE_FNVLIT(hunt_surround_Nb)(vc.surround);

    /* Induction factors: Hunt's approximation when the caller leaves them 0. */
    ALWAN_CORE_T const Yw_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(Y_w) > ALWAN_CORE_LITERAL(1e-12), Y_w, ALWAN_CORE_ONE);
    ALWAN_CORE_T const ind = ALWAN_CORE_LITERAL(0.725) * ALWAN_CORE_POW(ALWAN_CORE_MAX(Yw_safe / Y_b, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.2));
    ALWAN_CORE_T const N_cb = (vc.N_cb > ALWAN_CORE_LITERAL(0.0)) ? vc.N_cb : ind;
    ALWAN_CORE_T const N_bb = (vc.N_bb > ALWAN_CORE_LITERAL(0.0)) ? vc.N_bb : ind;

    /* Scotopic luminance of the illuminant, from CCT when not given. */
    ALWAN_CORE_T const cct = (vc.CCT_w > ALWAN_CORE_LITERAL(0.0)) ? vc.CCT_w : ALWAN_CORE_LITERAL(5000.0);
    ALWAN_CORE_T const L_AS = (vc.L_AS > ALWAN_CORE_LITERAL(0.0))
        ? vc.L_AS
        : ALWAN_CORE_LITERAL(2.26) * La * ALWAN_CORE_POW(ALWAN_CORE_MAX((cct / ALWAN_CORE_LITERAL(4000.0)) - ALWAN_CORE_LITERAL(0.4), ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));

    /* Scotopic responses default to the photopic luminances. */
    ALWAN_CORE_T const S_p   = (vc.S   > ALWAN_CORE_LITERAL(0.0)) ? vc.S   : xyz.y;
    ALWAN_CORE_T const S_w_p = (vc.S_w > ALWAN_CORE_LITERAL(0.0)) ? vc.S_w : Y_w;

    /* Backgrounds. The proximal field falls back to the background, which falls
     * back to a neutral at Y_b, matching the reference's own default. */
    ALWAN_CORE_VEC3 xyz_b_v, xyz_p_v;
    xyz_b_v.v[0] = (vc.xyz_b.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_b.x : Y_b;
    xyz_b_v.v[1] = Y_b;
    xyz_b_v.v[2] = (vc.xyz_b.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_b.z : Y_b;
    xyz_p_v.v[0] = (vc.xyz_p.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_p.x : xyz_b_v.v[0];
    xyz_p_v.v[1] = (vc.xyz_p.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_p.y : xyz_b_v.v[1];
    xyz_p_v.v[2] = (vc.xyz_p.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_p.z : xyz_b_v.v[2];

    /* Luminance level adaptation factor. */
    ALWAN_CORE_T const k = ALWAN_CORE_ONE / (ALWAN_CORE_LITERAL(5.0) * La + ALWAN_CORE_ONE);
    ALWAN_CORE_T const k4 = k * k * k * k;
    ALWAN_CORE_T const FL = ALWAN_CORE_LITERAL(0.2) * k4 * (ALWAN_CORE_LITERAL(5.0) * La) +
                  ALWAN_CORE_LITERAL(0.1) * (ALWAN_CORE_ONE - k4) * (ALWAN_CORE_ONE - k4) *
                  ALWAN_CORE_POW(ALWAN_CORE_LITERAL(5.0) * La, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));

    /* XYZ -> cone responses, for the stimulus, the white and the proximal field. */
    ALWAN_CORE_VEC3 xyz_v; xyz_v.v[0] = xyz.x; xyz_v.v[1] = xyz.y; xyz_v.v[2] = xyz.z;
    ALWAN_CORE_VEC3 xyz_w_v; xyz_w_v.v[0] = vc.xyz_w.x; xyz_w_v.v[1] = Y_w; xyz_w_v.v[2] = vc.xyz_w.z;
    ALWAN_CORE_VEC3 const rgb   = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HUNT_V_M_HPE), xyz_v);
    ALWAN_CORE_VEC3 const rgb_w = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HUNT_V_M_HPE), xyz_w_v);
    ALWAN_CORE_VEC3 const rgb_p = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HUNT_V_M_HPE), xyz_p_v);

    {
    ALWAN_CORE_VEC3 const rgb_a  = ALWAN_CORE_FNVLIT(hunt_adapt)(rgb,   rgb_w, Y_w, Y_b, La, FL, rgb_p, vc.p,
                                          vc.helson_judd_effect, vc.discount_illuminant);
    ALWAN_CORE_VEC3 const rgb_aw = ALWAN_CORE_FNVLIT(hunt_adapt)(rgb_w, rgb_w, Y_w, Y_b, La, FL, rgb_p, vc.p,
                                          vc.helson_judd_effect, vc.discount_illuminant);

    /* Achromatic post-adaptation signals. */
    ALWAN_CORE_T const A_a  = ALWAN_CORE_LITERAL(2.0) * rgb_a.v[0]  + rgb_a.v[1]  + rgb_a.v[2]  / ALWAN_CORE_LITERAL(20.0) - ALWAN_CORE_LITERAL(3.05) + ALWAN_CORE_ONE;
    ALWAN_CORE_T const A_aw = ALWAN_CORE_LITERAL(2.0) * rgb_aw.v[0] + rgb_aw.v[1] + rgb_aw.v[2] / ALWAN_CORE_LITERAL(20.0) - ALWAN_CORE_LITERAL(3.05) + ALWAN_CORE_ONE;

    /* Colour difference signals. */
    ALWAN_CORE_T const C1 = rgb_a.v[0] - rgb_a.v[1];
    ALWAN_CORE_T const C2 = rgb_a.v[1] - rgb_a.v[2];
    ALWAN_CORE_T const C3 = rgb_a.v[2] - rgb_a.v[0];
    ALWAN_CORE_T const C1w = rgb_aw.v[0] - rgb_aw.v[1];
    ALWAN_CORE_T const C2w = rgb_aw.v[1] - rgb_aw.v[2];
    ALWAN_CORE_T const C3w = rgb_aw.v[2] - rgb_aw.v[0];

    /* Hue angle. */
    ALWAN_CORE_T h_deg = ALWAN_CORE_LITERAL(180.0) * ALWAN_CORE_ATAN2(ALWAN_CORE_LITERAL(0.5) * (C2 - C3) / ALWAN_CORE_LITERAL(4.5), C1 - (C2 / ALWAN_CORE_LITERAL(11.0))) / ALWAN_CORE_PI;
    h_deg = h_deg - ALWAN_CORE_LITERAL(360.0) * ALWAN_CORE_FLOOR(h_deg / ALWAN_CORE_LITERAL(360.0));
    result.h = h_deg;

    {
    ALWAN_CORE_T const e_s = ALWAN_CORE_FNVLIT(hunt_eccentricity)(h_deg);
    ALWAN_CORE_T const F_t = La / (La + ALWAN_CORE_LITERAL(0.1));

    /* Chromatic responses. */
    ALWAN_CORE_T const M_yb   = ALWAN_CORE_LITERAL(100.0) * (ALWAN_CORE_LITERAL(0.5) * (C2 - C3) / ALWAN_CORE_LITERAL(4.5)) *
                      (e_s * (ALWAN_CORE_LITERAL(10.0) / ALWAN_CORE_LITERAL(13.0)) * N_c * N_cb * F_t);
    ALWAN_CORE_T const M_rg   = ALWAN_CORE_LITERAL(100.0) * (C1 - (C2 / ALWAN_CORE_LITERAL(11.0))) *
                      (e_s * (ALWAN_CORE_LITERAL(10.0) / ALWAN_CORE_LITERAL(13.0)) * N_c * N_cb);
    ALWAN_CORE_T const M_yb_w = ALWAN_CORE_LITERAL(100.0) * (ALWAN_CORE_LITERAL(0.5) * (C2w - C3w) / ALWAN_CORE_LITERAL(4.5)) *
                      (e_s * (ALWAN_CORE_LITERAL(10.0) / ALWAN_CORE_LITERAL(13.0)) * N_c * N_cb * F_t);
    ALWAN_CORE_T const M_rg_w = ALWAN_CORE_LITERAL(100.0) * (C1w - (C2w / ALWAN_CORE_LITERAL(11.0))) *
                      (e_s * (ALWAN_CORE_LITERAL(10.0) / ALWAN_CORE_LITERAL(13.0)) * N_c * N_cb);
    ALWAN_CORE_T const M   = ALWAN_CORE_SQRT(M_yb * M_yb + M_rg * M_rg);
    ALWAN_CORE_T const M_w = ALWAN_CORE_SQRT(M_yb_w * M_yb_w + M_rg_w * M_rg_w);

    /* Saturation. */
    ALWAN_CORE_T const rgb_a_sum = rgb_a.v[0] + rgb_a.v[1] + rgb_a.v[2];
    result.s = ALWAN_CORE_LITERAL(50.0) * M / ALWAN_CORE_SELECT(ALWAN_CORE_ABS(rgb_a_sum) > ALWAN_CORE_LITERAL(1e-12), rgb_a_sum, ALWAN_CORE_ONE);

    /* Achromatic signal, including the scotopic rod contribution. */
    {
    ALWAN_CORE_T const L_AS_226 = L_AS / ALWAN_CORE_LITERAL(2.26);
    ALWAN_CORE_T const five_LAS = ALWAN_CORE_LITERAL(5.0) * L_AS_226;
    ALWAN_CORE_T const j = ALWAN_CORE_LITERAL(0.00001) / (five_LAS + ALWAN_CORE_LITERAL(0.00001));
    ALWAN_CORE_T const F_LS = ALWAN_CORE_LITERAL(3800.0) * j * j * five_LAS +
                    ALWAN_CORE_LITERAL(0.2) * ALWAN_CORE_POW(ALWAN_CORE_MAX(ALWAN_CORE_ONE - j * j, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.4)) *
                    ALWAN_CORE_POW(ALWAN_CORE_MAX(five_LAS, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(6.0));
    ALWAN_CORE_T const S_S_w   = S_p   / ALWAN_CORE_SELECT(ALWAN_CORE_ABS(S_w_p) > ALWAN_CORE_LITERAL(1e-12), S_w_p, ALWAN_CORE_ONE);
    ALWAN_CORE_T const S_S_w_w = ALWAN_CORE_ONE;
    ALWAN_CORE_T const B_S   = ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(0.3) * ALWAN_CORE_POW(ALWAN_CORE_MAX(five_LAS * S_S_w, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.3))) +
                     ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(5.0) * five_LAS);
    ALWAN_CORE_T const B_S_w = ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(0.3) * ALWAN_CORE_POW(ALWAN_CORE_MAX(five_LAS * S_S_w_w, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.3))) +
                     ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(5.0) * five_LAS);
    ALWAN_CORE_T const A_S   = (ALWAN_CORE_FNVLIT(hunt_f_n)(F_LS * S_S_w)   * ALWAN_CORE_LITERAL(3.05) * B_S)   + ALWAN_CORE_LITERAL(0.3);
    ALWAN_CORE_T const A_S_w = (ALWAN_CORE_FNVLIT(hunt_f_n)(F_LS * S_S_w_w) * ALWAN_CORE_LITERAL(3.05) * B_S_w) + ALWAN_CORE_LITERAL(0.3);
    ALWAN_CORE_T const root  = ALWAN_CORE_SQRT(ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(0.3) * ALWAN_CORE_LITERAL(0.3));
    ALWAN_CORE_T const A     = N_bb * (A_a  - ALWAN_CORE_ONE + A_S   - ALWAN_CORE_LITERAL(0.3) + root);
    ALWAN_CORE_T const A_w   = N_bb * (A_aw - ALWAN_CORE_ONE + A_S_w - ALWAN_CORE_LITERAL(0.3) + root);

    /* Brightness. */
    {
    ALWAN_CORE_T const N_1 = ALWAN_CORE_POW(ALWAN_CORE_MAX(ALWAN_CORE_LITERAL(7.0) * A_w, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.5)) /
                   (ALWAN_CORE_LITERAL(5.33) * ALWAN_CORE_POW(ALWAN_CORE_MAX(N_b, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.13)));
    ALWAN_CORE_T const N_2 = (ALWAN_CORE_LITERAL(7.0) * A_w * ALWAN_CORE_POW(ALWAN_CORE_MAX(N_b, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.362))) / ALWAN_CORE_LITERAL(200.0);
    ALWAN_CORE_T const Q   = ALWAN_CORE_POW(ALWAN_CORE_MAX(ALWAN_CORE_LITERAL(7.0) * (A   + (M   / ALWAN_CORE_LITERAL(100.0))), ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.6)) * N_1 - N_2;
    ALWAN_CORE_T const Q_w = ALWAN_CORE_POW(ALWAN_CORE_MAX(ALWAN_CORE_LITERAL(7.0) * (A_w + (M_w / ALWAN_CORE_LITERAL(100.0))), ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.6)) * N_1 - N_2;
    ALWAN_CORE_T const Q_w_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(Q_w) > ALWAN_CORE_LITERAL(1e-12), Q_w, ALWAN_CORE_ONE);
    ALWAN_CORE_T const Y_b_Y_w = Y_b / Yw_safe;
    ALWAN_CORE_T const Z = ALWAN_CORE_ONE + ALWAN_CORE_POW(ALWAN_CORE_MAX(Y_b_Y_w, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.5));

    result.Q = Q;
    result.J = ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_POW(ALWAN_CORE_MAX(Q / Q_w_safe, ALWAN_CORE_LITERAL(0.0)), Z);
    result.C = ALWAN_CORE_LITERAL(2.44) * ALWAN_CORE_POW(ALWAN_CORE_MAX(result.s, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.69)) *
               ALWAN_CORE_POW(ALWAN_CORE_MAX(Q / Q_w_safe, ALWAN_CORE_LITERAL(0.0)), Y_b_Y_w) *
               (ALWAN_CORE_LITERAL(1.64) - ALWAN_CORE_POW(ALWAN_CORE_LITERAL(0.29), Y_b_Y_w));
    result.M = ALWAN_CORE_POW(ALWAN_CORE_MAX(FL, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.15)) * result.C;
    }
    }
    }
    }
    return result;
}

