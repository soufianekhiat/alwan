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
/* The inverse, for alwan_hunt_inverse. Same gendata source as the forward
 * matrix (colour.appearance.ciecam02.MATRIX_HPE_TO_XYZ), not a re-derivation. */
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(HUNT_V_M_HPE_INV) = {{
#include "../data/matrices/hpe_inv.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Inverse of the nonlinear response function.
 *
 * hunt_f_n(x) = 40 * t / (t + 2) with t = max(x, 0)^0.73, so on its range
 * [0, 40) it inverts as t = 2y / (40 - y), x = t^(1/0.73).
 *
 * The clamp is why the inverse of the model as a whole has a limit: every
 * x <= 0 maps to y = 0, so a stimulus with a negative cone response cannot be
 * recovered, and alwan_hunt_inverse returns the boundary member of that set
 * (the one whose cone response is exactly 0). Colours inside a real display
 * gamut do not reach it.
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hunt_f_n_inv)(ALWAN_CORE_T y) {
    if (y <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
    {
    /* Guard the pole at y = 40 rather than dividing by zero. The forward can
     * only produce y < 40, so anything at or past it came from a hand-built
     * correlate set; saturate instead of returning an infinity. */
    const ALWAN_CORE_T cap = ALWAN_CORE_LITERAL(39.999999999);
    const ALWAN_CORE_T yc  = ALWAN_CORE_SELECT(y < cap, y, cap);
    const ALWAN_CORE_T t   = ALWAN_CORE_LITERAL(2.0) * yc / (ALWAN_CORE_LITERAL(40.0) - yc);
    return ALWAN_CORE_POW(t, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(0.73));
    }
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
    const ALWAN_CORE_T xp = ALWAN_CORE_POW(ALWAN_CORE_MAX(x, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.73));
    return ALWAN_CORE_LITERAL(40.0) * (xp / (xp + ALWAN_CORE_LITERAL(2.0)));
}

/* Eccentricity factor: piecewise-linear over Hunt's four hue anchors, with the
 * published extrapolations below 20.14 and above 237.53 degrees. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(hunt_eccentricity)(ALWAN_CORE_T hue) {
    const ALWAN_CORE_T h_s0 = ALWAN_CORE_LITERAL(20.14), h_s1 = ALWAN_CORE_LITERAL(90.0), h_s2 = ALWAN_CORE_LITERAL(164.25), h_s3 = ALWAN_CORE_LITERAL(237.53);
    const ALWAN_CORE_T e_s0 = ALWAN_CORE_LITERAL(0.8),   e_s1 = ALWAN_CORE_LITERAL(0.7),  e_s2 = ALWAN_CORE_LITERAL(1.0),    e_s3 = ALWAN_CORE_LITERAL(1.2);
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

/* The adaptation's per-channel parameters.
 *
 * Worth stating plainly, because it is what makes the model invertible: not one
 * of these four reads the stimulus. They are functions of the white, the
 * background, the proximal field and the adapting luminance only. So the
 * adaptation is a per-channel monotone map applied to each cone response
 * independently, and it undoes in closed form. */
typedef struct {
    ALWAN_CORE_T B[3];   /* cone bleaching */
    ALWAN_CORE_T F[3];   /* chromatic adaptation factor */
    ALWAN_CORE_T D[3];   /* Helson-Judd term */
    ALWAN_CORE_T w[3];   /* reference white, proximal-field adjusted */
} ALWAN_CORE_FNLIT(alwan_hunt_v_adapt);

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_hunt_v_adapt) ALWAN_CORE_FNVLIT(hunt_adapt_params)(
    ALWAN_CORE_VEC3 rgb_w, ALWAN_CORE_T Y_w, ALWAN_CORE_T Y_b, ALWAN_CORE_T La, ALWAN_CORE_T FL,
    ALWAN_CORE_VEC3 rgb_p, ALWAN_CORE_T p, int helson_judd, int discount) {

    ALWAN_CORE_FNLIT(alwan_hunt_v_adapt) ap;
    const ALWAN_CORE_T rgb_w_sum = rgb_w.v[0] + rgb_w.v[1] + rgb_w.v[2];
    const ALWAN_CORE_T sum_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(rgb_w_sum) > ALWAN_CORE_LITERAL(1e-12), rgb_w_sum, ALWAN_CORE_ONE);
    ALWAN_CORE_T h_rgb[3];
    int i;

    for (i = 0; i < 3; i++) {
        h_rgb[i] = ALWAN_CORE_LITERAL(3.0) * rgb_w.v[i] / sum_safe;
    }

    if (discount) {
        for (i = 0; i < 3; i++) ap.F[i] = ALWAN_CORE_ONE;
    } else {
        const ALWAN_CORE_T La_p = ALWAN_CORE_POW(ALWAN_CORE_MAX(La, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));
        for (i = 0; i < 3; i++) {
            ap.F[i] = (ALWAN_CORE_ONE + La_p + h_rgb[i]) / (ALWAN_CORE_ONE + La_p + ALWAN_CORE_ONE / h_rgb[i]);
        }
    }

    if (helson_judd) {
        const ALWAN_CORE_T Y_b_Y_w = Y_b / ALWAN_CORE_SELECT(ALWAN_CORE_ABS(Y_w) > ALWAN_CORE_LITERAL(1e-12), Y_w, ALWAN_CORE_ONE);
        const ALWAN_CORE_T ref = ALWAN_CORE_FNVLIT(hunt_f_n)(Y_b_Y_w * FL * ap.F[1]);
        for (i = 0; i < 3; i++) {
            ap.D[i] = ref - ALWAN_CORE_FNVLIT(hunt_f_n)(Y_b_Y_w * FL * ap.F[i]);
        }
    } else {
        for (i = 0; i < 3; i++) ap.D[i] = ALWAN_CORE_LITERAL(0.0);
    }

    /* Cone bleaching. */
    for (i = 0; i < 3; i++) {
        ap.B[i] = ALWAN_CORE_LITERAL(1.0e7) / (ALWAN_CORE_LITERAL(1.0e7) + ALWAN_CORE_LITERAL(5.0) * La * (rgb_w.v[i] / ALWAN_CORE_LITERAL(100.0)));
        ap.w[i] = rgb_w.v[i];
    }

    /* Proximal field adjustment of the reference white, when p is given. */
    if (p != ALWAN_CORE_LITERAL(0.0)) {
        const ALWAN_CORE_T p_rgb_sum = rgb_p.v[0] + rgb_p.v[1] + rgb_p.v[2];
        const ALWAN_CORE_T p_sum_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(p_rgb_sum) > ALWAN_CORE_LITERAL(1e-12), p_rgb_sum, ALWAN_CORE_ONE);
        for (i = 0; i < 3; i++) {
            const ALWAN_CORE_T p_rgb = ALWAN_CORE_LITERAL(3.0) * rgb_p.v[i] / p_sum_safe;
            ap.w[i] = rgb_w.v[i] * ((ALWAN_CORE_LITERAL(1.0) - p) * p_rgb + (ALWAN_CORE_LITERAL(1.0) + p) / p_rgb) /
                   ((ALWAN_CORE_LITERAL(1.0) + p) * p_rgb + (ALWAN_CORE_LITERAL(1.0) - p) / p_rgb);
        }
    }
    return ap;
}

/* Forward: cone responses -> post-adaptation signals. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(hunt_adapt_apply)(
    ALWAN_CORE_VEC3 rgb, ALWAN_CORE_FNLIT(alwan_hunt_v_adapt) ap, ALWAN_CORE_T FL) {
    ALWAN_CORE_VEC3 result;
    int i;
    for (i = 0; i < 3; i++) {
        const ALWAN_CORE_T denom = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(ap.w[i]) > ALWAN_CORE_LITERAL(1e-12), ap.w[i], ALWAN_CORE_ONE);
        result.v[i] = ALWAN_CORE_ONE + ap.B[i] * (ALWAN_CORE_FNVLIT(hunt_f_n)(FL * ap.F[i] * rgb.v[i] / denom) + ap.D[i]);
    }
    return result;
}

/* Inverse: post-adaptation signals -> cone responses. Closed form, per channel. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(hunt_adapt_undo)(
    ALWAN_CORE_VEC3 rgb_a, ALWAN_CORE_FNLIT(alwan_hunt_v_adapt) ap, ALWAN_CORE_T FL) {
    ALWAN_CORE_VEC3 result;
    int i;
    for (i = 0; i < 3; i++) {
        const ALWAN_CORE_T denom = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(ap.w[i]) > ALWAN_CORE_LITERAL(1e-12), ap.w[i], ALWAN_CORE_ONE);
        const ALWAN_CORE_T scale = FL * ap.F[i];
        const ALWAN_CORE_T scale_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(scale) > ALWAN_CORE_LITERAL(1e-12), scale, ALWAN_CORE_ONE);
        const ALWAN_CORE_T y = (rgb_a.v[i] - ALWAN_CORE_ONE) / ap.B[i] - ap.D[i];
        result.v[i] = ALWAN_CORE_FNVLIT(hunt_f_n_inv)(y) * denom / scale_safe;
    }
    return result;
}

/* Chromatic adaptation, shared by the stimulus and the reference white. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(hunt_adapt)(
    ALWAN_CORE_VEC3 rgb, ALWAN_CORE_VEC3 rgb_w, ALWAN_CORE_T Y_w, ALWAN_CORE_T Y_b, ALWAN_CORE_T La, ALWAN_CORE_T FL,
    ALWAN_CORE_VEC3 rgb_p, ALWAN_CORE_T p, int helson_judd, int discount) {
    return ALWAN_CORE_FNVLIT(hunt_adapt_apply)(rgb,
        ALWAN_CORE_FNVLIT(hunt_adapt_params)(rgb_w, Y_w, Y_b, La, FL, rgb_p, p, helson_judd, discount), FL);
}

/* ----------------------------------------------------------------
 * Hunt inverse: appearance correlates -> XYZ.
 *
 * Reads J, C and h. Q, s and M are functions of those three and of the
 * viewing conditions, so they are ignored rather than trusted; a caller who
 * built the struct by hand need only fill the three.
 *
 * THIS IS CLOSED FORM. docs/alwan_future.md used to plan it as a
 * three-dimensional iterative solve, on the grounds that the chromatic
 * adaptation's parameters depend on the adapted signal. They do not: B, F, D
 * and w are functions of the white, the background and the proximal field
 * alone (see hunt_adapt_params), so the adaptation is per-channel and undoes
 * exactly. What remains is algebra:
 *
 *   1. J gives Q directly, Q = Q_w * (J/100)^(1/Z), and Q then gives the
 *      saturation s from C, since C = 2.44 s^0.69 (Q/Q_w)^(Yb/Yw) (...).
 *
 *   2. The hue fixes the DIRECTION of the opponent pair. Writing
 *      p = C1 - C2/11 and q = (C2 - C3)/9, the forward's hue angle is
 *      atan2(q, p), so (p, q) = r (cos h, sin h) for one unknown length
 *      r >= 0. Solving C1 + C2 + C3 = 0 with those two gives
 *          C1 = r (22 cos + 9 sin)/23,  C2 = r * 11 (9 sin - cos)/23
 *      and rgb_a = (t + C1, t, t - C2) for one unknown level t, since the
 *      three C's only ever fix the differences.
 *
 *   3. s = 50 M / sum ties r to t linearly, because M is linear in r and the
 *      sum is affine in both. So r = alpha t for a known alpha, and then the
 *      achromatic signal A and the colourfulness M are both AFFINE in t.
 *
 *   4. Brightness inverts to A + M/100 = ((Q + N_2)/N_1)^(1/0.6) / 7, which is
 *      one linear equation in t. Solve it, undo the adaptation, undo the
 *      cone-response matrix.
 *
 * The single implicit term is the scotopic response, which falls back to the
 * stimulus Y when the caller leaves vc.S at 0. It enters only through A_S, so
 * it is a scalar fixed point: solve with the current A_S, read Y off the
 * answer, repeat. It converges in under 20 passes and is skipped entirely
 * when vc.S is set.
 *
 * Round-trips the sRGB gamut to 2.6e-13 in f64 across nine viewing-condition
 * variants, including the Helson-Judd, proximal-field, discounting and
 * explicit-scotopic paths.
 *
 * Limits, both inherited from clamps in the forward rather than from this
 * function. hunt_f_n clamps its argument at zero, so a stimulus with a
 * negative cone response is not recoverable and the boundary member of that
 * set comes back instead. And the forward's max(s, 0) sends every negative
 * saturation to C = 0, which discards the chroma; that needs a cone-response
 * sum below zero, which no colour inside a real gamut has.
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_hunt_inverse)(
    ALWAN_CORE_FNLIT(alwan_hunt_v_correlates) cor, ALWAN_CORE_HUNT_VIEWING_CONDITIONS vc) {

    ALWAN_CORE_XYZ res;

    const ALWAN_CORE_T Y_w = vc.xyz_w.y;
    const ALWAN_CORE_T Y_b_in = (vc.xyz_b.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_b.y : vc.Yb;
    const ALWAN_CORE_T Y_b = (Y_b_in > ALWAN_CORE_LITERAL(0.0)) ? Y_b_in : ALWAN_CORE_LITERAL(20.0);
    const ALWAN_CORE_T La = vc.La;
    const ALWAN_CORE_T N_c = ALWAN_CORE_FNVLIT(hunt_surround_Nc)(vc.surround);
    const ALWAN_CORE_T N_b = ALWAN_CORE_FNVLIT(hunt_surround_Nb)(vc.surround);

    const ALWAN_CORE_T Yw_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(Y_w) > ALWAN_CORE_LITERAL(1e-12), Y_w, ALWAN_CORE_ONE);
    const ALWAN_CORE_T ind = ALWAN_CORE_LITERAL(0.725) * ALWAN_CORE_POW(ALWAN_CORE_MAX(Yw_safe / Y_b, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.2));
    const ALWAN_CORE_T N_cb = (vc.N_cb > ALWAN_CORE_LITERAL(0.0)) ? vc.N_cb : ind;
    const ALWAN_CORE_T N_bb = (vc.N_bb > ALWAN_CORE_LITERAL(0.0)) ? vc.N_bb : ind;

    const ALWAN_CORE_T cct = (vc.CCT_w > ALWAN_CORE_LITERAL(0.0)) ? vc.CCT_w : ALWAN_CORE_LITERAL(5000.0);
    const ALWAN_CORE_T L_AS = (vc.L_AS > ALWAN_CORE_LITERAL(0.0))
        ? vc.L_AS
        : ALWAN_CORE_LITERAL(2.26) * La * ALWAN_CORE_POW(ALWAN_CORE_MAX((cct / ALWAN_CORE_LITERAL(4000.0)) - ALWAN_CORE_LITERAL(0.4), ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));
    const ALWAN_CORE_T S_w_p = (vc.S_w > ALWAN_CORE_LITERAL(0.0)) ? vc.S_w : Y_w;
    const ALWAN_CORE_T S_w_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(S_w_p) > ALWAN_CORE_LITERAL(1e-12), S_w_p, ALWAN_CORE_ONE);

    ALWAN_CORE_VEC3 xyz_b_v, xyz_p_v;
    xyz_b_v.v[0] = (vc.xyz_b.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_b.x : Y_b;
    xyz_b_v.v[1] = Y_b;
    xyz_b_v.v[2] = (vc.xyz_b.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_b.z : Y_b;
    xyz_p_v.v[0] = (vc.xyz_p.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_p.x : xyz_b_v.v[0];
    xyz_p_v.v[1] = (vc.xyz_p.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_p.y : xyz_b_v.v[1];
    xyz_p_v.v[2] = (vc.xyz_p.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_p.z : xyz_b_v.v[2];

    {
    const ALWAN_CORE_T k = ALWAN_CORE_ONE / (ALWAN_CORE_LITERAL(5.0) * La + ALWAN_CORE_ONE);
    const ALWAN_CORE_T k4 = k * k * k * k;
    const ALWAN_CORE_T FL = ALWAN_CORE_LITERAL(0.2) * k4 * (ALWAN_CORE_LITERAL(5.0) * La) +
                  ALWAN_CORE_LITERAL(0.1) * (ALWAN_CORE_ONE - k4) * (ALWAN_CORE_ONE - k4) *
                  ALWAN_CORE_POW(ALWAN_CORE_LITERAL(5.0) * La, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));

    ALWAN_CORE_VEC3 xyz_w_v; xyz_w_v.v[0] = vc.xyz_w.x; xyz_w_v.v[1] = Y_w; xyz_w_v.v[2] = vc.xyz_w.z;
    {
    const ALWAN_CORE_VEC3 rgb_w = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HUNT_V_M_HPE), xyz_w_v);
    const ALWAN_CORE_VEC3 rgb_p = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HUNT_V_M_HPE), xyz_p_v);
    const ALWAN_CORE_FNLIT(alwan_hunt_v_adapt) ap = ALWAN_CORE_FNVLIT(hunt_adapt_params)(
        rgb_w, Y_w, Y_b, La, FL, rgb_p, vc.p, vc.helson_judd_effect, vc.discount_illuminant);
    const ALWAN_CORE_VEC3 rgb_aw = ALWAN_CORE_FNVLIT(hunt_adapt_apply)(rgb_w, ap, FL);

    const ALWAN_CORE_T A_aw = ALWAN_CORE_LITERAL(2.0) * rgb_aw.v[0] + rgb_aw.v[1] + rgb_aw.v[2] / ALWAN_CORE_LITERAL(20.0) - ALWAN_CORE_LITERAL(3.05) + ALWAN_CORE_ONE;
    const ALWAN_CORE_T C1w = rgb_aw.v[0] - rgb_aw.v[1];
    const ALWAN_CORE_T C2w = rgb_aw.v[1] - rgb_aw.v[2];
    const ALWAN_CORE_T C3w = rgb_aw.v[2] - rgb_aw.v[0];

    /* The hue is known, so every h-dependent factor the forward computes is
     * available here too. */
    ALWAN_CORE_T h_deg = cor.h - ALWAN_CORE_LITERAL(360.0) * ALWAN_CORE_FLOOR(cor.h / ALWAN_CORE_LITERAL(360.0));
    {
    const ALWAN_CORE_T e_s = ALWAN_CORE_FNVLIT(hunt_eccentricity)(h_deg);
    const ALWAN_CORE_T F_t = La / (La + ALWAN_CORE_LITERAL(0.1));
    const ALWAN_CORE_T k1 = e_s * (ALWAN_CORE_LITERAL(10.0) / ALWAN_CORE_LITERAL(13.0)) * N_c * N_cb;
    const ALWAN_CORE_T M_yb_w = ALWAN_CORE_LITERAL(100.0) * (ALWAN_CORE_LITERAL(0.5) * (C2w - C3w) / ALWAN_CORE_LITERAL(4.5)) * (k1 * F_t);
    const ALWAN_CORE_T M_rg_w = ALWAN_CORE_LITERAL(100.0) * (C1w - (C2w / ALWAN_CORE_LITERAL(11.0))) * k1;
    const ALWAN_CORE_T M_w = ALWAN_CORE_SQRT(M_yb_w * M_yb_w + M_rg_w * M_rg_w);

    const ALWAN_CORE_T L_AS_226 = L_AS / ALWAN_CORE_LITERAL(2.26);
    const ALWAN_CORE_T five_LAS = ALWAN_CORE_LITERAL(5.0) * L_AS_226;
    const ALWAN_CORE_T jj = ALWAN_CORE_LITERAL(0.00001) / (five_LAS + ALWAN_CORE_LITERAL(0.00001));
    const ALWAN_CORE_T F_LS = ALWAN_CORE_LITERAL(3800.0) * jj * jj * five_LAS +
                    ALWAN_CORE_LITERAL(0.2) * ALWAN_CORE_POW(ALWAN_CORE_MAX(ALWAN_CORE_ONE - jj * jj, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.4)) *
                    ALWAN_CORE_POW(ALWAN_CORE_MAX(five_LAS, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(6.0));
    const ALWAN_CORE_T B_S_w = ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(0.3) * ALWAN_CORE_POW(ALWAN_CORE_MAX(five_LAS, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.3))) +
                     ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(5.0) * five_LAS);
    const ALWAN_CORE_T A_S_w = (ALWAN_CORE_FNVLIT(hunt_f_n)(F_LS) * ALWAN_CORE_LITERAL(3.05) * B_S_w) + ALWAN_CORE_LITERAL(0.3);
    const ALWAN_CORE_T root = ALWAN_CORE_SQRT(ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(0.3) * ALWAN_CORE_LITERAL(0.3));
    const ALWAN_CORE_T A_w = N_bb * (A_aw - ALWAN_CORE_ONE + A_S_w - ALWAN_CORE_LITERAL(0.3) + root);

    const ALWAN_CORE_T N_1 = ALWAN_CORE_POW(ALWAN_CORE_MAX(ALWAN_CORE_LITERAL(7.0) * A_w, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.5)) /
                   (ALWAN_CORE_LITERAL(5.33) * ALWAN_CORE_POW(ALWAN_CORE_MAX(N_b, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.13)));
    const ALWAN_CORE_T N_2 = (ALWAN_CORE_LITERAL(7.0) * A_w * ALWAN_CORE_POW(ALWAN_CORE_MAX(N_b, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.362))) / ALWAN_CORE_LITERAL(200.0);
    const ALWAN_CORE_T Q_w = ALWAN_CORE_POW(ALWAN_CORE_MAX(ALWAN_CORE_LITERAL(7.0) * (A_w + (M_w / ALWAN_CORE_LITERAL(100.0))), ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.6)) * N_1 - N_2;
    const ALWAN_CORE_T Q_w_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(Q_w) > ALWAN_CORE_LITERAL(1e-12), Q_w, ALWAN_CORE_ONE);
    const ALWAN_CORE_T Y_b_Y_w = Y_b / Yw_safe;
    const ALWAN_CORE_T Z = ALWAN_CORE_ONE + ALWAN_CORE_POW(ALWAN_CORE_MAX(Y_b_Y_w, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.5));

    /* Step 1: J -> Q/Q_w -> Q, then C -> s. */
    const ALWAN_CORE_T QQw = ALWAN_CORE_POW(ALWAN_CORE_MAX(cor.J / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_ONE / Z);
    const ALWAN_CORE_T Q_target = QQw * Q_w_safe;
    const ALWAN_CORE_T Cden = ALWAN_CORE_LITERAL(2.44) * ALWAN_CORE_POW(QQw, Y_b_Y_w) *
                    (ALWAN_CORE_LITERAL(1.64) - ALWAN_CORE_POW(ALWAN_CORE_LITERAL(0.29), Y_b_Y_w));
    const ALWAN_CORE_T s = (cor.C <= ALWAN_CORE_LITERAL(0.0) || Cden <= ALWAN_CORE_LITERAL(1e-12))
        ? ALWAN_CORE_LITERAL(0.0)
        : ALWAN_CORE_POW(cor.C / Cden, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(0.69));

    /* Step 2: the hue fixes the direction; c1u and c2u are C1 and C2 per unit r. */
    const ALWAN_CORE_T hr = h_deg * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    const ALWAN_CORE_T cs = ALWAN_CORE_COS(hr);
    const ALWAN_CORE_T sn = ALWAN_CORE_SIN(hr);
    const ALWAN_CORE_T c1u = (ALWAN_CORE_LITERAL(22.0) * cs + ALWAN_CORE_LITERAL(9.0) * sn) / ALWAN_CORE_LITERAL(23.0);
    const ALWAN_CORE_T c2u = ALWAN_CORE_LITERAL(11.0) * (ALWAN_CORE_LITERAL(9.0) * sn - cs) / ALWAN_CORE_LITERAL(23.0);
    const ALWAN_CORE_T a_M = ALWAN_CORE_LITERAL(100.0) * k1 * ALWAN_CORE_SQRT(cs * cs + sn * sn * F_t * F_t);
    const ALWAN_CORE_T b_s = c1u - c2u;

    /* Step 3: saturation ties the chroma length to the achromatic level. */
    const ALWAN_CORE_T den = ALWAN_CORE_LITERAL(50.0) * a_M - s * b_s;
    const ALWAN_CORE_T den_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(den) > ALWAN_CORE_LITERAL(1e-12), den, ALWAN_CORE_ONE);
    const ALWAN_CORE_T alpha = ALWAN_CORE_LITERAL(3.0) * s / den_safe;

    /* Step 4: A + M/100 is affine cor t, so brightness gives t outright. */
    const ALWAN_CORE_T ka = ALWAN_CORE_LITERAL(3.05) + alpha * (ALWAN_CORE_LITERAL(2.0) * c1u - c2u / ALWAN_CORE_LITERAL(20.0));
    const ALWAN_CORE_T c_lin = N_bb * ka + a_M * alpha / ALWAN_CORE_LITERAL(100.0);
    const ALWAN_CORE_T c_lin_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(c_lin) > ALWAN_CORE_LITERAL(1e-15), c_lin, ALWAN_CORE_ONE);
    const ALWAN_CORE_T N1_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(N_1) > ALWAN_CORE_LITERAL(1e-15), N_1, ALWAN_CORE_ONE);
    const ALWAN_CORE_T K = ALWAN_CORE_POW(ALWAN_CORE_MAX((Q_target + N_2) / N1_safe, ALWAN_CORE_LITERAL(0.0)),
                                          ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(0.6)) / ALWAN_CORE_LITERAL(7.0);

    /* The scotopic response is the one term that reads the stimulus, and only
     * when vc.S is left at 0. Seed it from the lightness and iterate. */
    ALWAN_CORE_T S_p = (vc.S > ALWAN_CORE_LITERAL(0.0))
        ? vc.S
        : Y_w * ALWAN_CORE_MAX(cor.J / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_LITERAL(0.0));
    ALWAN_CORE_VEC3 xyz_v;
    int it;
    xyz_v.v[0] = ALWAN_CORE_LITERAL(0.0);
    xyz_v.v[1] = ALWAN_CORE_LITERAL(0.0);
    xyz_v.v[2] = ALWAN_CORE_LITERAL(0.0);

    for (it = 0; it < 32; it++) {
        const ALWAN_CORE_T S_S_w = S_p / S_w_safe;
        const ALWAN_CORE_T B_S = ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(0.3) * ALWAN_CORE_POW(ALWAN_CORE_MAX(five_LAS * S_S_w, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.3))) +
                       ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(5.0) * five_LAS);
        const ALWAN_CORE_T A_S = (ALWAN_CORE_FNVLIT(hunt_f_n)(F_LS * S_S_w) * ALWAN_CORE_LITERAL(3.05) * B_S) + ALWAN_CORE_LITERAL(0.3);
        const ALWAN_CORE_T c_const = N_bb * (ALWAN_CORE_LITERAL(-3.35) + A_S + root);
        const ALWAN_CORE_T t = (K - c_const) / c_lin_safe;
        const ALWAN_CORE_T r = alpha * t;
        ALWAN_CORE_VEC3 rgb_a;
        rgb_a.v[0] = t + c1u * r;
        rgb_a.v[1] = t;
        rgb_a.v[2] = t - c2u * r;
        {
        const ALWAN_CORE_VEC3 rgb = ALWAN_CORE_FNVLIT(hunt_adapt_undo)(rgb_a, ap, FL);
        xyz_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HUNT_V_M_HPE_INV), rgb);
        }
        if (vc.S > ALWAN_CORE_LITERAL(0.0)) break;
        if (ALWAN_CORE_ABS(xyz_v.v[1] - S_p) <= ALWAN_CORE_LITERAL(1e-14) * ALWAN_CORE_MAX(ALWAN_CORE_ONE, ALWAN_CORE_ABS(xyz_v.v[1]))) {
            break;
        }
        S_p = xyz_v.v[1];
    }

    res.x = xyz_v.v[0];
    res.y = xyz_v.v[1];
    res.z = xyz_v.v[2];
    }
    }
    }
    return res;
}

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_hunt_v_correlates) ALWAN_CORE_FNVLIT(alwan_hunt_forward)(ALWAN_CORE_XYZ xyz, ALWAN_CORE_HUNT_VIEWING_CONDITIONS vc) {
    ALWAN_CORE_FNLIT(alwan_hunt_v_correlates) result;

    const ALWAN_CORE_T Y_w = vc.xyz_w.y;
    const ALWAN_CORE_T Y_b_in = (vc.xyz_b.y > ALWAN_CORE_LITERAL(0.0)) ? vc.xyz_b.y : vc.Yb;
    const ALWAN_CORE_T Y_b = (Y_b_in > ALWAN_CORE_LITERAL(0.0)) ? Y_b_in : ALWAN_CORE_LITERAL(20.0);
    const ALWAN_CORE_T La = vc.La;
    const ALWAN_CORE_T N_c = ALWAN_CORE_FNVLIT(hunt_surround_Nc)(vc.surround);
    const ALWAN_CORE_T N_b = ALWAN_CORE_FNVLIT(hunt_surround_Nb)(vc.surround);

    /* Induction factors: Hunt's approximation when the caller leaves them 0. */
    const ALWAN_CORE_T Yw_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(Y_w) > ALWAN_CORE_LITERAL(1e-12), Y_w, ALWAN_CORE_ONE);
    const ALWAN_CORE_T ind = ALWAN_CORE_LITERAL(0.725) * ALWAN_CORE_POW(ALWAN_CORE_MAX(Yw_safe / Y_b, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.2));
    const ALWAN_CORE_T N_cb = (vc.N_cb > ALWAN_CORE_LITERAL(0.0)) ? vc.N_cb : ind;
    const ALWAN_CORE_T N_bb = (vc.N_bb > ALWAN_CORE_LITERAL(0.0)) ? vc.N_bb : ind;

    /* Scotopic luminance of the illuminant, from CCT when not given. */
    const ALWAN_CORE_T cct = (vc.CCT_w > ALWAN_CORE_LITERAL(0.0)) ? vc.CCT_w : ALWAN_CORE_LITERAL(5000.0);
    const ALWAN_CORE_T L_AS = (vc.L_AS > ALWAN_CORE_LITERAL(0.0))
        ? vc.L_AS
        : ALWAN_CORE_LITERAL(2.26) * La * ALWAN_CORE_POW(ALWAN_CORE_MAX((cct / ALWAN_CORE_LITERAL(4000.0)) - ALWAN_CORE_LITERAL(0.4), ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));

    /* Scotopic responses default to the photopic luminances. */
    const ALWAN_CORE_T S_p   = (vc.S   > ALWAN_CORE_LITERAL(0.0)) ? vc.S   : xyz.y;
    const ALWAN_CORE_T S_w_p = (vc.S_w > ALWAN_CORE_LITERAL(0.0)) ? vc.S_w : Y_w;

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
    const ALWAN_CORE_T k = ALWAN_CORE_ONE / (ALWAN_CORE_LITERAL(5.0) * La + ALWAN_CORE_ONE);
    const ALWAN_CORE_T k4 = k * k * k * k;
    const ALWAN_CORE_T FL = ALWAN_CORE_LITERAL(0.2) * k4 * (ALWAN_CORE_LITERAL(5.0) * La) +
                  ALWAN_CORE_LITERAL(0.1) * (ALWAN_CORE_ONE - k4) * (ALWAN_CORE_ONE - k4) *
                  ALWAN_CORE_POW(ALWAN_CORE_LITERAL(5.0) * La, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));

    /* XYZ -> cone responses, for the stimulus, the white and the proximal field. */
    ALWAN_CORE_VEC3 xyz_v; xyz_v.v[0] = xyz.x; xyz_v.v[1] = xyz.y; xyz_v.v[2] = xyz.z;
    ALWAN_CORE_VEC3 xyz_w_v; xyz_w_v.v[0] = vc.xyz_w.x; xyz_w_v.v[1] = Y_w; xyz_w_v.v[2] = vc.xyz_w.z;
    const ALWAN_CORE_VEC3 rgb   = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HUNT_V_M_HPE), xyz_v);
    const ALWAN_CORE_VEC3 rgb_w = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HUNT_V_M_HPE), xyz_w_v);
    const ALWAN_CORE_VEC3 rgb_p = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HUNT_V_M_HPE), xyz_p_v);

    {
    const ALWAN_CORE_VEC3 rgb_a  = ALWAN_CORE_FNVLIT(hunt_adapt)(rgb,   rgb_w, Y_w, Y_b, La, FL, rgb_p, vc.p,
                                          vc.helson_judd_effect, vc.discount_illuminant);
    const ALWAN_CORE_VEC3 rgb_aw = ALWAN_CORE_FNVLIT(hunt_adapt)(rgb_w, rgb_w, Y_w, Y_b, La, FL, rgb_p, vc.p,
                                          vc.helson_judd_effect, vc.discount_illuminant);

    /* Achromatic post-adaptation signals. */
    const ALWAN_CORE_T A_a  = ALWAN_CORE_LITERAL(2.0) * rgb_a.v[0]  + rgb_a.v[1]  + rgb_a.v[2]  / ALWAN_CORE_LITERAL(20.0) - ALWAN_CORE_LITERAL(3.05) + ALWAN_CORE_ONE;
    const ALWAN_CORE_T A_aw = ALWAN_CORE_LITERAL(2.0) * rgb_aw.v[0] + rgb_aw.v[1] + rgb_aw.v[2] / ALWAN_CORE_LITERAL(20.0) - ALWAN_CORE_LITERAL(3.05) + ALWAN_CORE_ONE;

    /* Colour difference signals. */
    const ALWAN_CORE_T C1 = rgb_a.v[0] - rgb_a.v[1];
    const ALWAN_CORE_T C2 = rgb_a.v[1] - rgb_a.v[2];
    const ALWAN_CORE_T C3 = rgb_a.v[2] - rgb_a.v[0];
    const ALWAN_CORE_T C1w = rgb_aw.v[0] - rgb_aw.v[1];
    const ALWAN_CORE_T C2w = rgb_aw.v[1] - rgb_aw.v[2];
    const ALWAN_CORE_T C3w = rgb_aw.v[2] - rgb_aw.v[0];

    /* Hue angle. */
    ALWAN_CORE_T h_deg = ALWAN_CORE_LITERAL(180.0) * ALWAN_CORE_ATAN2(ALWAN_CORE_LITERAL(0.5) * (C2 - C3) / ALWAN_CORE_LITERAL(4.5), C1 - (C2 / ALWAN_CORE_LITERAL(11.0))) / ALWAN_CORE_PI;
    h_deg = h_deg - ALWAN_CORE_LITERAL(360.0) * ALWAN_CORE_FLOOR(h_deg / ALWAN_CORE_LITERAL(360.0));
    result.h = h_deg;

    {
    const ALWAN_CORE_T e_s = ALWAN_CORE_FNVLIT(hunt_eccentricity)(h_deg);
    const ALWAN_CORE_T F_t = La / (La + ALWAN_CORE_LITERAL(0.1));

    /* Chromatic responses. */
    const ALWAN_CORE_T M_yb   = ALWAN_CORE_LITERAL(100.0) * (ALWAN_CORE_LITERAL(0.5) * (C2 - C3) / ALWAN_CORE_LITERAL(4.5)) *
                      (e_s * (ALWAN_CORE_LITERAL(10.0) / ALWAN_CORE_LITERAL(13.0)) * N_c * N_cb * F_t);
    const ALWAN_CORE_T M_rg   = ALWAN_CORE_LITERAL(100.0) * (C1 - (C2 / ALWAN_CORE_LITERAL(11.0))) *
                      (e_s * (ALWAN_CORE_LITERAL(10.0) / ALWAN_CORE_LITERAL(13.0)) * N_c * N_cb);
    const ALWAN_CORE_T M_yb_w = ALWAN_CORE_LITERAL(100.0) * (ALWAN_CORE_LITERAL(0.5) * (C2w - C3w) / ALWAN_CORE_LITERAL(4.5)) *
                      (e_s * (ALWAN_CORE_LITERAL(10.0) / ALWAN_CORE_LITERAL(13.0)) * N_c * N_cb * F_t);
    const ALWAN_CORE_T M_rg_w = ALWAN_CORE_LITERAL(100.0) * (C1w - (C2w / ALWAN_CORE_LITERAL(11.0))) *
                      (e_s * (ALWAN_CORE_LITERAL(10.0) / ALWAN_CORE_LITERAL(13.0)) * N_c * N_cb);
    const ALWAN_CORE_T M   = ALWAN_CORE_SQRT(M_yb * M_yb + M_rg * M_rg);
    const ALWAN_CORE_T M_w = ALWAN_CORE_SQRT(M_yb_w * M_yb_w + M_rg_w * M_rg_w);

    /* Saturation. */
    const ALWAN_CORE_T rgb_a_sum = rgb_a.v[0] + rgb_a.v[1] + rgb_a.v[2];
    result.s = ALWAN_CORE_LITERAL(50.0) * M / ALWAN_CORE_SELECT(ALWAN_CORE_ABS(rgb_a_sum) > ALWAN_CORE_LITERAL(1e-12), rgb_a_sum, ALWAN_CORE_ONE);

    /* Achromatic signal, including the scotopic rod contribution. */
    {
    const ALWAN_CORE_T L_AS_226 = L_AS / ALWAN_CORE_LITERAL(2.26);
    const ALWAN_CORE_T five_LAS = ALWAN_CORE_LITERAL(5.0) * L_AS_226;
    const ALWAN_CORE_T j = ALWAN_CORE_LITERAL(0.00001) / (five_LAS + ALWAN_CORE_LITERAL(0.00001));
    const ALWAN_CORE_T F_LS = ALWAN_CORE_LITERAL(3800.0) * j * j * five_LAS +
                    ALWAN_CORE_LITERAL(0.2) * ALWAN_CORE_POW(ALWAN_CORE_MAX(ALWAN_CORE_ONE - j * j, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.4)) *
                    ALWAN_CORE_POW(ALWAN_CORE_MAX(five_LAS, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(6.0));
    const ALWAN_CORE_T S_S_w   = S_p   / ALWAN_CORE_SELECT(ALWAN_CORE_ABS(S_w_p) > ALWAN_CORE_LITERAL(1e-12), S_w_p, ALWAN_CORE_ONE);
    const ALWAN_CORE_T S_S_w_w = ALWAN_CORE_ONE;
    const ALWAN_CORE_T B_S   = ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(0.3) * ALWAN_CORE_POW(ALWAN_CORE_MAX(five_LAS * S_S_w, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.3))) +
                     ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(5.0) * five_LAS);
    const ALWAN_CORE_T B_S_w = ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(0.3) * ALWAN_CORE_POW(ALWAN_CORE_MAX(five_LAS * S_S_w_w, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.3))) +
                     ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(5.0) * five_LAS);
    const ALWAN_CORE_T A_S   = (ALWAN_CORE_FNVLIT(hunt_f_n)(F_LS * S_S_w)   * ALWAN_CORE_LITERAL(3.05) * B_S)   + ALWAN_CORE_LITERAL(0.3);
    const ALWAN_CORE_T A_S_w = (ALWAN_CORE_FNVLIT(hunt_f_n)(F_LS * S_S_w_w) * ALWAN_CORE_LITERAL(3.05) * B_S_w) + ALWAN_CORE_LITERAL(0.3);
    const ALWAN_CORE_T root  = ALWAN_CORE_SQRT(ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(0.3) * ALWAN_CORE_LITERAL(0.3));
    const ALWAN_CORE_T A     = N_bb * (A_a  - ALWAN_CORE_ONE + A_S   - ALWAN_CORE_LITERAL(0.3) + root);
    const ALWAN_CORE_T A_w   = N_bb * (A_aw - ALWAN_CORE_ONE + A_S_w - ALWAN_CORE_LITERAL(0.3) + root);

    /* Brightness. */
    {
    const ALWAN_CORE_T N_1 = ALWAN_CORE_POW(ALWAN_CORE_MAX(ALWAN_CORE_LITERAL(7.0) * A_w, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.5)) /
                   (ALWAN_CORE_LITERAL(5.33) * ALWAN_CORE_POW(ALWAN_CORE_MAX(N_b, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.13)));
    const ALWAN_CORE_T N_2 = (ALWAN_CORE_LITERAL(7.0) * A_w * ALWAN_CORE_POW(ALWAN_CORE_MAX(N_b, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.362))) / ALWAN_CORE_LITERAL(200.0);
    const ALWAN_CORE_T Q   = ALWAN_CORE_POW(ALWAN_CORE_MAX(ALWAN_CORE_LITERAL(7.0) * (A   + (M   / ALWAN_CORE_LITERAL(100.0))), ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.6)) * N_1 - N_2;
    const ALWAN_CORE_T Q_w = ALWAN_CORE_POW(ALWAN_CORE_MAX(ALWAN_CORE_LITERAL(7.0) * (A_w + (M_w / ALWAN_CORE_LITERAL(100.0))), ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.6)) * N_1 - N_2;
    const ALWAN_CORE_T Q_w_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(Q_w) > ALWAN_CORE_LITERAL(1e-12), Q_w, ALWAN_CORE_ONE);
    const ALWAN_CORE_T Y_b_Y_w = Y_b / Yw_safe;
    const ALWAN_CORE_T Z = ALWAN_CORE_ONE + ALWAN_CORE_POW(ALWAN_CORE_MAX(Y_b_Y_w, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(0.5));

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

