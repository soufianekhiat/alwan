/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Hunt Color Appearance Model
 * Value-returning variant for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Hunt (1991, 1995)
 * "Revised colour-appearance model for related and unrelated colours"
 */

#ifndef ALWAN_HUNT_CORE_H
#define ALWAN_HUNT_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_math_core.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_hunt_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_hunt_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* GPU backends - original code */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

typedef struct {
    alwan_scalar J;
    alwan_scalar C;
    alwan_scalar h;
    alwan_scalar s;
    alwan_scalar Q;
    alwan_scalar M;
} alwan_hunt_v_correlates;

/* The viewing-conditions struct the C branch emits per precision from
 * alwan_types_gen.inc, in the single-precision GPU spelling. Field names,
 * order and derive-from-zero semantics match the C struct exactly; see the
 * comments there for what each field falls back to when left at 0. */
typedef struct {
    alwan_xyz    xyz_w;
    alwan_scalar La;
    alwan_scalar Yb;
    alwan_hunt_surround surround;
    int          discount_illuminant;

    alwan_xyz    xyz_b;
    alwan_xyz    xyz_p;
    alwan_scalar p;
    alwan_scalar L_AS;
    alwan_scalar CCT_w;
    alwan_scalar S;
    alwan_scalar S_w;
    alwan_scalar N_cb;
    alwan_scalar N_bb;
    int          helson_judd_effect;
} alwan_hunt_viewing_conditions;

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 HUNT_V_M_HPE = {{
#include "../data/matrices/hpe.csv"
}};
/* The inverse, for alwan_hunt_inverse_v. Same gendata source as the forward
 * matrix (colour.appearance.ciecam02.MATRIX_HPE_TO_XYZ), not a re-derivation. */
ALWAN_CONSTEXPR alwan_mat3x3 HUNT_V_M_HPE_INV = {{
#include "../data/matrices/hpe_inv.csv"
}};
ALWAN_DIAG_POP

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
ALWAN_INLINE alwan_scalar hunt_surround_Nc_v(alwan_hunt_surround s) {
    return (s == ALWAN_HUNT_SURROUND_DARK) ? ALWAN_LITERAL(0.7) : ALWAN_LITERAL(1.0);
}
ALWAN_INLINE alwan_scalar hunt_surround_Nb_v(alwan_hunt_surround s) {
    switch (s) {
        case ALWAN_HUNT_SURROUND_DIM:  return ALWAN_LITERAL(25.0);
        case ALWAN_HUNT_SURROUND_DARK: return ALWAN_LITERAL(25.0);
        case ALWAN_HUNT_SURROUND_NORMAL:
        default:                       return ALWAN_LITERAL(75.0);
    }
}
/* Hunt's nonlinearity: 40 * (x^0.73 / (x^0.73 + 2)). */
ALWAN_INLINE alwan_scalar hunt_f_n_v(alwan_scalar x) {
    const alwan_scalar xp = ALWAN_POW(alwan_max(x, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.73));
    return ALWAN_LITERAL(40.0) * (xp / (xp + ALWAN_LITERAL(2.0)));
}

/* Its inverse on the range [0, 40): t = 2y/(40-y), x = t^(1/0.73).
 *
 * The clamp in hunt_f_n_v is why the model's inverse has a limit: every x <= 0
 * maps to y = 0, so a stimulus with a negative cone response is not
 * recoverable, and alwan_hunt_inverse_v returns the boundary member of that
 * set. Colours inside a real display gamut do not reach it. */
ALWAN_INLINE alwan_scalar hunt_f_n_inv_v(alwan_scalar y) {
    if (y <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    {
    const alwan_scalar cap = ALWAN_LITERAL(39.999999999);
    const alwan_scalar yc  = ALWAN_SELECT(y < cap, y, cap);
    const alwan_scalar t   = ALWAN_LITERAL(2.0) * yc / (ALWAN_LITERAL(40.0) - yc);
    return ALWAN_POW(t, ALWAN_ONE / ALWAN_LITERAL(0.73));
    }
}

/* Eccentricity factor: piecewise-linear over Hunt's four hue anchors, with the
 * published extrapolations below 20.14 and above 237.53 degrees. */
ALWAN_INLINE alwan_scalar hunt_eccentricity_v(alwan_scalar hue) {
    const alwan_scalar h_s0 = ALWAN_LITERAL(20.14), h_s1 = ALWAN_LITERAL(90.0), h_s2 = ALWAN_LITERAL(164.25), h_s3 = ALWAN_LITERAL(237.53);
    const alwan_scalar e_s0 = ALWAN_LITERAL(0.8),   e_s1 = ALWAN_LITERAL(0.7),  e_s2 = ALWAN_LITERAL(1.0),    e_s3 = ALWAN_LITERAL(1.2);
    alwan_scalar interp;
    if (hue <= h_s1) {
        interp = e_s0 + (e_s1 - e_s0) * (hue - h_s0) / (h_s1 - h_s0);
    } else if (hue <= h_s2) {
        interp = e_s1 + (e_s2 - e_s1) * (hue - h_s1) / (h_s2 - h_s1);
    } else {
        interp = e_s2 + (e_s3 - e_s2) * (hue - h_s2) / (h_s3 - h_s2);
    }
    if (hue < h_s0) {
        interp = ALWAN_LITERAL(0.856) - (hue / h_s0) * ALWAN_LITERAL(0.056);
    } else if (hue > h_s3) {
        interp = ALWAN_LITERAL(0.856) + ALWAN_LITERAL(0.344) * (ALWAN_LITERAL(360.0) - hue) / (ALWAN_LITERAL(360.0) - h_s3);
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
    alwan_scalar B[3];   /* cone bleaching */
    alwan_scalar F[3];   /* chromatic adaptation factor */
    alwan_scalar D[3];   /* Helson-Judd term */
    alwan_scalar w[3];   /* reference white, proximal-field adjusted */
} alwan_hunt_v_adapt;

ALWAN_INLINE alwan_hunt_v_adapt hunt_adapt_params_v(
    alwan_vec3 rgb_w, alwan_scalar Y_w, alwan_scalar Y_b, alwan_scalar La, alwan_scalar FL,
    alwan_vec3 rgb_p, alwan_scalar p, int helson_judd, int discount) {

    alwan_hunt_v_adapt ap;
    const alwan_scalar rgb_w_sum = rgb_w.v[0] + rgb_w.v[1] + rgb_w.v[2];
    const alwan_scalar sum_safe = ALWAN_SELECT(ALWAN_ABS(rgb_w_sum) > ALWAN_LITERAL(1e-12), rgb_w_sum, ALWAN_ONE);
    alwan_scalar h_rgb[3];
    int i;

    for (i = 0; i < 3; i++) {
        h_rgb[i] = ALWAN_LITERAL(3.0) * rgb_w.v[i] / sum_safe;
    }

    if (discount) {
        for (i = 0; i < 3; i++) ap.F[i] = ALWAN_ONE;
    } else {
        const alwan_scalar La_p = ALWAN_POW(alwan_max(La, ALWAN_LITERAL(0.0)), ALWAN_ONE / ALWAN_LITERAL(3.0));
        for (i = 0; i < 3; i++) {
            ap.F[i] = (ALWAN_ONE + La_p + h_rgb[i]) / (ALWAN_ONE + La_p + ALWAN_ONE / h_rgb[i]);
        }
    }

    if (helson_judd) {
        const alwan_scalar Y_b_Y_w = Y_b / ALWAN_SELECT(ALWAN_ABS(Y_w) > ALWAN_LITERAL(1e-12), Y_w, ALWAN_ONE);
        const alwan_scalar ref = hunt_f_n_v(Y_b_Y_w * FL * ap.F[1]);
        for (i = 0; i < 3; i++) {
            ap.D[i] = ref - hunt_f_n_v(Y_b_Y_w * FL * ap.F[i]);
        }
    } else {
        for (i = 0; i < 3; i++) ap.D[i] = ALWAN_LITERAL(0.0);
    }

    /* Cone bleaching. */
    for (i = 0; i < 3; i++) {
        ap.B[i] = ALWAN_LITERAL(1.0e7) / (ALWAN_LITERAL(1.0e7) + ALWAN_LITERAL(5.0) * La * (rgb_w.v[i] / ALWAN_LITERAL(100.0)));
        ap.w[i] = rgb_w.v[i];
    }

    /* Proximal field adjustment of the reference white, when p is given. */
    if (p != ALWAN_LITERAL(0.0)) {
        const alwan_scalar p_rgb_sum = rgb_p.v[0] + rgb_p.v[1] + rgb_p.v[2];
        const alwan_scalar p_sum_safe = ALWAN_SELECT(ALWAN_ABS(p_rgb_sum) > ALWAN_LITERAL(1e-12), p_rgb_sum, ALWAN_ONE);
        for (i = 0; i < 3; i++) {
            const alwan_scalar p_rgb = ALWAN_LITERAL(3.0) * rgb_p.v[i] / p_sum_safe;
            ap.w[i] = rgb_w.v[i] * ((ALWAN_LITERAL(1.0) - p) * p_rgb + (ALWAN_LITERAL(1.0) + p) / p_rgb) /
                   ((ALWAN_LITERAL(1.0) + p) * p_rgb + (ALWAN_LITERAL(1.0) - p) / p_rgb);
        }
    }
    return ap;
}

/* Forward: cone responses -> post-adaptation signals. */
ALWAN_INLINE alwan_vec3 hunt_adapt_apply_v(
    alwan_vec3 rgb, alwan_hunt_v_adapt ap, alwan_scalar FL) {
    alwan_vec3 result;
    int i;
    for (i = 0; i < 3; i++) {
        const alwan_scalar denom = ALWAN_SELECT(ALWAN_ABS(ap.w[i]) > ALWAN_LITERAL(1e-12), ap.w[i], ALWAN_ONE);
        result.v[i] = ALWAN_ONE + ap.B[i] * (hunt_f_n_v(FL * ap.F[i] * rgb.v[i] / denom) + ap.D[i]);
    }
    return result;
}

/* Inverse: post-adaptation signals -> cone responses. Closed form, per channel. */
ALWAN_INLINE alwan_vec3 hunt_adapt_undo_v(
    alwan_vec3 rgb_a, alwan_hunt_v_adapt ap, alwan_scalar FL) {
    alwan_vec3 result;
    int i;
    for (i = 0; i < 3; i++) {
        const alwan_scalar denom = ALWAN_SELECT(ALWAN_ABS(ap.w[i]) > ALWAN_LITERAL(1e-12), ap.w[i], ALWAN_ONE);
        const alwan_scalar scale = FL * ap.F[i];
        const alwan_scalar scale_safe = ALWAN_SELECT(ALWAN_ABS(scale) > ALWAN_LITERAL(1e-12), scale, ALWAN_ONE);
        const alwan_scalar y = (rgb_a.v[i] - ALWAN_ONE) / ap.B[i] - ap.D[i];
        result.v[i] = hunt_f_n_inv_v(y) * denom / scale_safe;
    }
    return result;
}

/* Chromatic adaptation, shared by the stimulus and the reference white. */
ALWAN_INLINE alwan_vec3 hunt_adapt_v(
    alwan_vec3 rgb, alwan_vec3 rgb_w, alwan_scalar Y_w, alwan_scalar Y_b, alwan_scalar La, alwan_scalar FL,
    alwan_vec3 rgb_p, alwan_scalar p, int helson_judd, int discount) {
    return hunt_adapt_apply_v(rgb,
        hunt_adapt_params_v(rgb_w, Y_w, Y_b, La, FL, rgb_p, p, helson_judd, discount), FL);
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

ALWAN_INLINE alwan_xyz alwan_hunt_inverse_v(
    alwan_hunt_v_correlates cor, alwan_hunt_viewing_conditions vc) {

    alwan_xyz res;

    const alwan_scalar Y_w = vc.xyz_w.y;
    const alwan_scalar Y_b_in = (vc.xyz_b.y > ALWAN_LITERAL(0.0)) ? vc.xyz_b.y : vc.Yb;
    const alwan_scalar Y_b = (Y_b_in > ALWAN_LITERAL(0.0)) ? Y_b_in : ALWAN_LITERAL(20.0);
    const alwan_scalar La = vc.La;
    const alwan_scalar N_c = hunt_surround_Nc_v(vc.surround);
    const alwan_scalar N_b = hunt_surround_Nb_v(vc.surround);

    const alwan_scalar Yw_safe = ALWAN_SELECT(ALWAN_ABS(Y_w) > ALWAN_LITERAL(1e-12), Y_w, ALWAN_ONE);
    const alwan_scalar ind = ALWAN_LITERAL(0.725) * ALWAN_POW(alwan_max(Yw_safe / Y_b, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.2));
    const alwan_scalar N_cb = (vc.N_cb > ALWAN_LITERAL(0.0)) ? vc.N_cb : ind;
    const alwan_scalar N_bb = (vc.N_bb > ALWAN_LITERAL(0.0)) ? vc.N_bb : ind;

    const alwan_scalar cct = (vc.CCT_w > ALWAN_LITERAL(0.0)) ? vc.CCT_w : ALWAN_LITERAL(5000.0);
    const alwan_scalar L_AS = (vc.L_AS > ALWAN_LITERAL(0.0))
        ? vc.L_AS
        : ALWAN_LITERAL(2.26) * La * ALWAN_POW(alwan_max((cct / ALWAN_LITERAL(4000.0)) - ALWAN_LITERAL(0.4), ALWAN_LITERAL(0.0)), ALWAN_ONE / ALWAN_LITERAL(3.0));
    const alwan_scalar S_w_p = (vc.S_w > ALWAN_LITERAL(0.0)) ? vc.S_w : Y_w;
    const alwan_scalar S_w_safe = ALWAN_SELECT(ALWAN_ABS(S_w_p) > ALWAN_LITERAL(1e-12), S_w_p, ALWAN_ONE);

    alwan_vec3 xyz_b_v, xyz_p_v;
    xyz_b_v.v[0] = (vc.xyz_b.y > ALWAN_LITERAL(0.0)) ? vc.xyz_b.x : Y_b;
    xyz_b_v.v[1] = Y_b;
    xyz_b_v.v[2] = (vc.xyz_b.y > ALWAN_LITERAL(0.0)) ? vc.xyz_b.z : Y_b;
    xyz_p_v.v[0] = (vc.xyz_p.y > ALWAN_LITERAL(0.0)) ? vc.xyz_p.x : xyz_b_v.v[0];
    xyz_p_v.v[1] = (vc.xyz_p.y > ALWAN_LITERAL(0.0)) ? vc.xyz_p.y : xyz_b_v.v[1];
    xyz_p_v.v[2] = (vc.xyz_p.y > ALWAN_LITERAL(0.0)) ? vc.xyz_p.z : xyz_b_v.v[2];

    {
    const alwan_scalar k = ALWAN_ONE / (ALWAN_LITERAL(5.0) * La + ALWAN_ONE);
    const alwan_scalar k4 = k * k * k * k;
    const alwan_scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * La) +
                  ALWAN_LITERAL(0.1) * (ALWAN_ONE - k4) * (ALWAN_ONE - k4) *
                  ALWAN_POW(ALWAN_LITERAL(5.0) * La, ALWAN_ONE / ALWAN_LITERAL(3.0));

    alwan_vec3 xyz_w_v; xyz_w_v.v[0] = vc.xyz_w.x; xyz_w_v.v[1] = Y_w; xyz_w_v.v[2] = vc.xyz_w.z;
    {
    const alwan_vec3 rgb_w = alwan_mat3_mulv_v(HUNT_V_M_HPE, xyz_w_v);
    const alwan_vec3 rgb_p = alwan_mat3_mulv_v(HUNT_V_M_HPE, xyz_p_v);
    const alwan_hunt_v_adapt ap = hunt_adapt_params_v(
        rgb_w, Y_w, Y_b, La, FL, rgb_p, vc.p, vc.helson_judd_effect, vc.discount_illuminant);
    const alwan_vec3 rgb_aw = hunt_adapt_apply_v(rgb_w, ap, FL);

    const alwan_scalar A_aw = ALWAN_LITERAL(2.0) * rgb_aw.v[0] + rgb_aw.v[1] + rgb_aw.v[2] / ALWAN_LITERAL(20.0) - ALWAN_LITERAL(3.05) + ALWAN_ONE;
    const alwan_scalar C1w = rgb_aw.v[0] - rgb_aw.v[1];
    const alwan_scalar C2w = rgb_aw.v[1] - rgb_aw.v[2];
    const alwan_scalar C3w = rgb_aw.v[2] - rgb_aw.v[0];

    /* The hue is known, so every h-dependent factor the forward computes is
     * available here too. */
    alwan_scalar h_deg = cor.h - ALWAN_LITERAL(360.0) * ALWAN_FLOOR(cor.h / ALWAN_LITERAL(360.0));
    {
    const alwan_scalar e_s = hunt_eccentricity_v(h_deg);
    const alwan_scalar F_t = La / (La + ALWAN_LITERAL(0.1));
    const alwan_scalar k1 = e_s * (ALWAN_LITERAL(10.0) / ALWAN_LITERAL(13.0)) * N_c * N_cb;
    const alwan_scalar M_yb_w = ALWAN_LITERAL(100.0) * (ALWAN_LITERAL(0.5) * (C2w - C3w) / ALWAN_LITERAL(4.5)) * (k1 * F_t);
    const alwan_scalar M_rg_w = ALWAN_LITERAL(100.0) * (C1w - (C2w / ALWAN_LITERAL(11.0))) * k1;
    const alwan_scalar M_w = ALWAN_SQRT(M_yb_w * M_yb_w + M_rg_w * M_rg_w);

    const alwan_scalar L_AS_226 = L_AS / ALWAN_LITERAL(2.26);
    const alwan_scalar five_LAS = ALWAN_LITERAL(5.0) * L_AS_226;
    const alwan_scalar jj = ALWAN_LITERAL(0.00001) / (five_LAS + ALWAN_LITERAL(0.00001));
    const alwan_scalar F_LS = ALWAN_LITERAL(3800.0) * jj * jj * five_LAS +
                    ALWAN_LITERAL(0.2) * ALWAN_POW(alwan_max(ALWAN_ONE - jj * jj, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.4)) *
                    ALWAN_POW(alwan_max(five_LAS, ALWAN_LITERAL(0.0)), ALWAN_ONE / ALWAN_LITERAL(6.0));
    const alwan_scalar B_S_w = ALWAN_LITERAL(0.5) / (ALWAN_ONE + ALWAN_LITERAL(0.3) * ALWAN_POW(alwan_max(five_LAS, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.3))) +
                     ALWAN_LITERAL(0.5) / (ALWAN_ONE + ALWAN_LITERAL(5.0) * five_LAS);
    const alwan_scalar A_S_w = (hunt_f_n_v(F_LS) * ALWAN_LITERAL(3.05) * B_S_w) + ALWAN_LITERAL(0.3);
    const alwan_scalar root = ALWAN_SQRT(ALWAN_ONE + ALWAN_LITERAL(0.3) * ALWAN_LITERAL(0.3));
    const alwan_scalar A_w = N_bb * (A_aw - ALWAN_ONE + A_S_w - ALWAN_LITERAL(0.3) + root);

    const alwan_scalar N_1 = ALWAN_POW(alwan_max(ALWAN_LITERAL(7.0) * A_w, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.5)) /
                   (ALWAN_LITERAL(5.33) * ALWAN_POW(alwan_max(N_b, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.13)));
    const alwan_scalar N_2 = (ALWAN_LITERAL(7.0) * A_w * ALWAN_POW(alwan_max(N_b, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.362))) / ALWAN_LITERAL(200.0);
    const alwan_scalar Q_w = ALWAN_POW(alwan_max(ALWAN_LITERAL(7.0) * (A_w + (M_w / ALWAN_LITERAL(100.0))), ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.6)) * N_1 - N_2;
    const alwan_scalar Q_w_safe = ALWAN_SELECT(ALWAN_ABS(Q_w) > ALWAN_LITERAL(1e-12), Q_w, ALWAN_ONE);
    const alwan_scalar Y_b_Y_w = Y_b / Yw_safe;
    const alwan_scalar Z = ALWAN_ONE + ALWAN_POW(alwan_max(Y_b_Y_w, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.5));

    /* Step 1: J -> Q/Q_w -> Q, then C -> s. */
    const alwan_scalar QQw = ALWAN_POW(alwan_max(cor.J / ALWAN_LITERAL(100.0), ALWAN_LITERAL(0.0)), ALWAN_ONE / Z);
    const alwan_scalar Q_target = QQw * Q_w_safe;
    const alwan_scalar Cden = ALWAN_LITERAL(2.44) * ALWAN_POW(QQw, Y_b_Y_w) *
                    (ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), Y_b_Y_w));
    const alwan_scalar s = (cor.C <= ALWAN_LITERAL(0.0) || Cden <= ALWAN_LITERAL(1e-12))
        ? ALWAN_LITERAL(0.0)
        : ALWAN_POW(cor.C / Cden, ALWAN_ONE / ALWAN_LITERAL(0.69));

    /* Step 2: the hue fixes the direction; c1u and c2u are C1 and C2 per unit r. */
    const alwan_scalar hr = h_deg * ALWAN_PI / ALWAN_LITERAL(180.0);
    const alwan_scalar cs = ALWAN_COS(hr);
    const alwan_scalar sn = ALWAN_SIN(hr);
    const alwan_scalar c1u = (ALWAN_LITERAL(22.0) * cs + ALWAN_LITERAL(9.0) * sn) / ALWAN_LITERAL(23.0);
    const alwan_scalar c2u = ALWAN_LITERAL(11.0) * (ALWAN_LITERAL(9.0) * sn - cs) / ALWAN_LITERAL(23.0);
    const alwan_scalar a_M = ALWAN_LITERAL(100.0) * k1 * ALWAN_SQRT(cs * cs + sn * sn * F_t * F_t);
    const alwan_scalar b_s = c1u - c2u;

    /* Step 3: saturation ties the chroma length to the achromatic level. */
    const alwan_scalar den = ALWAN_LITERAL(50.0) * a_M - s * b_s;
    const alwan_scalar den_safe = ALWAN_SELECT(ALWAN_ABS(den) > ALWAN_LITERAL(1e-12), den, ALWAN_ONE);
    const alwan_scalar alpha = ALWAN_LITERAL(3.0) * s / den_safe;

    /* Step 4: A + M/100 is affine cor t, so brightness gives t outright. */
    const alwan_scalar ka = ALWAN_LITERAL(3.05) + alpha * (ALWAN_LITERAL(2.0) * c1u - c2u / ALWAN_LITERAL(20.0));
    const alwan_scalar c_lin = N_bb * ka + a_M * alpha / ALWAN_LITERAL(100.0);
    const alwan_scalar c_lin_safe = ALWAN_SELECT(ALWAN_ABS(c_lin) > ALWAN_LITERAL(1e-15), c_lin, ALWAN_ONE);
    const alwan_scalar N1_safe = ALWAN_SELECT(ALWAN_ABS(N_1) > ALWAN_LITERAL(1e-15), N_1, ALWAN_ONE);
    const alwan_scalar K = ALWAN_POW(alwan_max((Q_target + N_2) / N1_safe, ALWAN_LITERAL(0.0)),
                                          ALWAN_ONE / ALWAN_LITERAL(0.6)) / ALWAN_LITERAL(7.0);

    /* The scotopic response is the one term that reads the stimulus, and only
     * when vc.S is left at 0. Seed it from the lightness and iterate. */
    alwan_scalar S_p = (vc.S > ALWAN_LITERAL(0.0))
        ? vc.S
        : Y_w * alwan_max(cor.J / ALWAN_LITERAL(100.0), ALWAN_LITERAL(0.0));
    alwan_vec3 xyz_v;
    int it;
    xyz_v.v[0] = ALWAN_LITERAL(0.0);
    xyz_v.v[1] = ALWAN_LITERAL(0.0);
    xyz_v.v[2] = ALWAN_LITERAL(0.0);

    for (it = 0; it < 32; it++) {
        const alwan_scalar S_S_w = S_p / S_w_safe;
        const alwan_scalar B_S = ALWAN_LITERAL(0.5) / (ALWAN_ONE + ALWAN_LITERAL(0.3) * ALWAN_POW(alwan_max(five_LAS * S_S_w, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.3))) +
                       ALWAN_LITERAL(0.5) / (ALWAN_ONE + ALWAN_LITERAL(5.0) * five_LAS);
        const alwan_scalar A_S = (hunt_f_n_v(F_LS * S_S_w) * ALWAN_LITERAL(3.05) * B_S) + ALWAN_LITERAL(0.3);
        const alwan_scalar c_const = N_bb * (ALWAN_LITERAL(-3.35) + A_S + root);
        const alwan_scalar t = (K - c_const) / c_lin_safe;
        const alwan_scalar r = alpha * t;
        alwan_vec3 rgb_a;
        rgb_a.v[0] = t + c1u * r;
        rgb_a.v[1] = t;
        rgb_a.v[2] = t - c2u * r;
        {
        const alwan_vec3 rgb = hunt_adapt_undo_v(rgb_a, ap, FL);
        xyz_v = alwan_mat3_mulv_v(HUNT_V_M_HPE_INV, rgb);
        }
        if (vc.S > ALWAN_LITERAL(0.0)) break;
        if (ALWAN_ABS(xyz_v.v[1] - S_p) <= ALWAN_LITERAL(1e-14) * alwan_max(ALWAN_ONE, ALWAN_ABS(xyz_v.v[1]))) {
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

ALWAN_INLINE alwan_hunt_v_correlates alwan_hunt_forward_v(alwan_xyz xyz, alwan_hunt_viewing_conditions vc) {
    alwan_hunt_v_correlates result;

    const alwan_scalar Y_w = vc.xyz_w.y;
    const alwan_scalar Y_b_in = (vc.xyz_b.y > ALWAN_LITERAL(0.0)) ? vc.xyz_b.y : vc.Yb;
    const alwan_scalar Y_b = (Y_b_in > ALWAN_LITERAL(0.0)) ? Y_b_in : ALWAN_LITERAL(20.0);
    const alwan_scalar La = vc.La;
    const alwan_scalar N_c = hunt_surround_Nc_v(vc.surround);
    const alwan_scalar N_b = hunt_surround_Nb_v(vc.surround);

    /* Induction factors: Hunt's approximation when the caller leaves them 0. */
    const alwan_scalar Yw_safe = ALWAN_SELECT(ALWAN_ABS(Y_w) > ALWAN_LITERAL(1e-12), Y_w, ALWAN_ONE);
    const alwan_scalar ind = ALWAN_LITERAL(0.725) * ALWAN_POW(alwan_max(Yw_safe / Y_b, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.2));
    const alwan_scalar N_cb = (vc.N_cb > ALWAN_LITERAL(0.0)) ? vc.N_cb : ind;
    const alwan_scalar N_bb = (vc.N_bb > ALWAN_LITERAL(0.0)) ? vc.N_bb : ind;

    /* Scotopic luminance of the illuminant, from CCT when not given. */
    const alwan_scalar cct = (vc.CCT_w > ALWAN_LITERAL(0.0)) ? vc.CCT_w : ALWAN_LITERAL(5000.0);
    const alwan_scalar L_AS = (vc.L_AS > ALWAN_LITERAL(0.0))
        ? vc.L_AS
        : ALWAN_LITERAL(2.26) * La * ALWAN_POW(alwan_max((cct / ALWAN_LITERAL(4000.0)) - ALWAN_LITERAL(0.4), ALWAN_LITERAL(0.0)), ALWAN_ONE / ALWAN_LITERAL(3.0));

    /* Scotopic responses default to the photopic luminances. */
    const alwan_scalar S_p   = (vc.S   > ALWAN_LITERAL(0.0)) ? vc.S   : xyz.y;
    const alwan_scalar S_w_p = (vc.S_w > ALWAN_LITERAL(0.0)) ? vc.S_w : Y_w;

    /* Backgrounds. The proximal field falls back to the background, which falls
     * back to a neutral at Y_b, matching the reference's own default. */
    alwan_vec3 xyz_b_v, xyz_p_v;
    xyz_b_v.v[0] = (vc.xyz_b.y > ALWAN_LITERAL(0.0)) ? vc.xyz_b.x : Y_b;
    xyz_b_v.v[1] = Y_b;
    xyz_b_v.v[2] = (vc.xyz_b.y > ALWAN_LITERAL(0.0)) ? vc.xyz_b.z : Y_b;
    xyz_p_v.v[0] = (vc.xyz_p.y > ALWAN_LITERAL(0.0)) ? vc.xyz_p.x : xyz_b_v.v[0];
    xyz_p_v.v[1] = (vc.xyz_p.y > ALWAN_LITERAL(0.0)) ? vc.xyz_p.y : xyz_b_v.v[1];
    xyz_p_v.v[2] = (vc.xyz_p.y > ALWAN_LITERAL(0.0)) ? vc.xyz_p.z : xyz_b_v.v[2];

    /* Luminance level adaptation factor. */
    const alwan_scalar k = ALWAN_ONE / (ALWAN_LITERAL(5.0) * La + ALWAN_ONE);
    const alwan_scalar k4 = k * k * k * k;
    const alwan_scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * La) +
                  ALWAN_LITERAL(0.1) * (ALWAN_ONE - k4) * (ALWAN_ONE - k4) *
                  ALWAN_POW(ALWAN_LITERAL(5.0) * La, ALWAN_ONE / ALWAN_LITERAL(3.0));

    /* XYZ -> cone responses, for the stimulus, the white and the proximal field. */
    alwan_vec3 xyz_v; xyz_v.v[0] = xyz.x; xyz_v.v[1] = xyz.y; xyz_v.v[2] = xyz.z;
    alwan_vec3 xyz_w_v; xyz_w_v.v[0] = vc.xyz_w.x; xyz_w_v.v[1] = Y_w; xyz_w_v.v[2] = vc.xyz_w.z;
    const alwan_vec3 rgb   = alwan_mat3_mulv_v(HUNT_V_M_HPE, xyz_v);
    const alwan_vec3 rgb_w = alwan_mat3_mulv_v(HUNT_V_M_HPE, xyz_w_v);
    const alwan_vec3 rgb_p = alwan_mat3_mulv_v(HUNT_V_M_HPE, xyz_p_v);

    {
    const alwan_vec3 rgb_a  = hunt_adapt_v(rgb,   rgb_w, Y_w, Y_b, La, FL, rgb_p, vc.p,
                                          vc.helson_judd_effect, vc.discount_illuminant);
    const alwan_vec3 rgb_aw = hunt_adapt_v(rgb_w, rgb_w, Y_w, Y_b, La, FL, rgb_p, vc.p,
                                          vc.helson_judd_effect, vc.discount_illuminant);

    /* Achromatic post-adaptation signals. */
    const alwan_scalar A_a  = ALWAN_LITERAL(2.0) * rgb_a.v[0]  + rgb_a.v[1]  + rgb_a.v[2]  / ALWAN_LITERAL(20.0) - ALWAN_LITERAL(3.05) + ALWAN_ONE;
    const alwan_scalar A_aw = ALWAN_LITERAL(2.0) * rgb_aw.v[0] + rgb_aw.v[1] + rgb_aw.v[2] / ALWAN_LITERAL(20.0) - ALWAN_LITERAL(3.05) + ALWAN_ONE;

    /* Colour difference signals. */
    const alwan_scalar C1 = rgb_a.v[0] - rgb_a.v[1];
    const alwan_scalar C2 = rgb_a.v[1] - rgb_a.v[2];
    const alwan_scalar C3 = rgb_a.v[2] - rgb_a.v[0];
    const alwan_scalar C1w = rgb_aw.v[0] - rgb_aw.v[1];
    const alwan_scalar C2w = rgb_aw.v[1] - rgb_aw.v[2];
    const alwan_scalar C3w = rgb_aw.v[2] - rgb_aw.v[0];

    /* Hue angle. */
    alwan_scalar h_deg = ALWAN_LITERAL(180.0) * ALWAN_ATAN2(ALWAN_LITERAL(0.5) * (C2 - C3) / ALWAN_LITERAL(4.5), C1 - (C2 / ALWAN_LITERAL(11.0))) / ALWAN_PI;
    h_deg = h_deg - ALWAN_LITERAL(360.0) * ALWAN_FLOOR(h_deg / ALWAN_LITERAL(360.0));
    result.h = h_deg;

    {
    const alwan_scalar e_s = hunt_eccentricity_v(h_deg);
    const alwan_scalar F_t = La / (La + ALWAN_LITERAL(0.1));

    /* Chromatic responses. */
    const alwan_scalar M_yb   = ALWAN_LITERAL(100.0) * (ALWAN_LITERAL(0.5) * (C2 - C3) / ALWAN_LITERAL(4.5)) *
                      (e_s * (ALWAN_LITERAL(10.0) / ALWAN_LITERAL(13.0)) * N_c * N_cb * F_t);
    const alwan_scalar M_rg   = ALWAN_LITERAL(100.0) * (C1 - (C2 / ALWAN_LITERAL(11.0))) *
                      (e_s * (ALWAN_LITERAL(10.0) / ALWAN_LITERAL(13.0)) * N_c * N_cb);
    const alwan_scalar M_yb_w = ALWAN_LITERAL(100.0) * (ALWAN_LITERAL(0.5) * (C2w - C3w) / ALWAN_LITERAL(4.5)) *
                      (e_s * (ALWAN_LITERAL(10.0) / ALWAN_LITERAL(13.0)) * N_c * N_cb * F_t);
    const alwan_scalar M_rg_w = ALWAN_LITERAL(100.0) * (C1w - (C2w / ALWAN_LITERAL(11.0))) *
                      (e_s * (ALWAN_LITERAL(10.0) / ALWAN_LITERAL(13.0)) * N_c * N_cb);
    const alwan_scalar M   = ALWAN_SQRT(M_yb * M_yb + M_rg * M_rg);
    const alwan_scalar M_w = ALWAN_SQRT(M_yb_w * M_yb_w + M_rg_w * M_rg_w);

    /* Saturation. */
    const alwan_scalar rgb_a_sum = rgb_a.v[0] + rgb_a.v[1] + rgb_a.v[2];
    result.s = ALWAN_LITERAL(50.0) * M / ALWAN_SELECT(ALWAN_ABS(rgb_a_sum) > ALWAN_LITERAL(1e-12), rgb_a_sum, ALWAN_ONE);

    /* Achromatic signal, including the scotopic rod contribution. */
    {
    const alwan_scalar L_AS_226 = L_AS / ALWAN_LITERAL(2.26);
    const alwan_scalar five_LAS = ALWAN_LITERAL(5.0) * L_AS_226;
    const alwan_scalar j = ALWAN_LITERAL(0.00001) / (five_LAS + ALWAN_LITERAL(0.00001));
    const alwan_scalar F_LS = ALWAN_LITERAL(3800.0) * j * j * five_LAS +
                    ALWAN_LITERAL(0.2) * ALWAN_POW(alwan_max(ALWAN_ONE - j * j, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.4)) *
                    ALWAN_POW(alwan_max(five_LAS, ALWAN_LITERAL(0.0)), ALWAN_ONE / ALWAN_LITERAL(6.0));
    const alwan_scalar S_S_w   = S_p   / ALWAN_SELECT(ALWAN_ABS(S_w_p) > ALWAN_LITERAL(1e-12), S_w_p, ALWAN_ONE);
    const alwan_scalar S_S_w_w = ALWAN_ONE;
    const alwan_scalar B_S   = ALWAN_LITERAL(0.5) / (ALWAN_ONE + ALWAN_LITERAL(0.3) * ALWAN_POW(alwan_max(five_LAS * S_S_w, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.3))) +
                     ALWAN_LITERAL(0.5) / (ALWAN_ONE + ALWAN_LITERAL(5.0) * five_LAS);
    const alwan_scalar B_S_w = ALWAN_LITERAL(0.5) / (ALWAN_ONE + ALWAN_LITERAL(0.3) * ALWAN_POW(alwan_max(five_LAS * S_S_w_w, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.3))) +
                     ALWAN_LITERAL(0.5) / (ALWAN_ONE + ALWAN_LITERAL(5.0) * five_LAS);
    const alwan_scalar A_S   = (hunt_f_n_v(F_LS * S_S_w)   * ALWAN_LITERAL(3.05) * B_S)   + ALWAN_LITERAL(0.3);
    const alwan_scalar A_S_w = (hunt_f_n_v(F_LS * S_S_w_w) * ALWAN_LITERAL(3.05) * B_S_w) + ALWAN_LITERAL(0.3);
    const alwan_scalar root  = ALWAN_SQRT(ALWAN_ONE + ALWAN_LITERAL(0.3) * ALWAN_LITERAL(0.3));
    const alwan_scalar A     = N_bb * (A_a  - ALWAN_ONE + A_S   - ALWAN_LITERAL(0.3) + root);
    const alwan_scalar A_w   = N_bb * (A_aw - ALWAN_ONE + A_S_w - ALWAN_LITERAL(0.3) + root);

    /* Brightness. */
    {
    const alwan_scalar N_1 = ALWAN_POW(alwan_max(ALWAN_LITERAL(7.0) * A_w, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.5)) /
                   (ALWAN_LITERAL(5.33) * ALWAN_POW(alwan_max(N_b, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.13)));
    const alwan_scalar N_2 = (ALWAN_LITERAL(7.0) * A_w * ALWAN_POW(alwan_max(N_b, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.362))) / ALWAN_LITERAL(200.0);
    const alwan_scalar Q   = ALWAN_POW(alwan_max(ALWAN_LITERAL(7.0) * (A   + (M   / ALWAN_LITERAL(100.0))), ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.6)) * N_1 - N_2;
    const alwan_scalar Q_w = ALWAN_POW(alwan_max(ALWAN_LITERAL(7.0) * (A_w + (M_w / ALWAN_LITERAL(100.0))), ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.6)) * N_1 - N_2;
    const alwan_scalar Q_w_safe = ALWAN_SELECT(ALWAN_ABS(Q_w) > ALWAN_LITERAL(1e-12), Q_w, ALWAN_ONE);
    const alwan_scalar Y_b_Y_w = Y_b / Yw_safe;
    const alwan_scalar Z = ALWAN_ONE + ALWAN_POW(alwan_max(Y_b_Y_w, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.5));

    result.Q = Q;
    result.J = ALWAN_LITERAL(100.0) * ALWAN_POW(alwan_max(Q / Q_w_safe, ALWAN_LITERAL(0.0)), Z);
    result.C = ALWAN_LITERAL(2.44) * ALWAN_POW(alwan_max(result.s, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.69)) *
               ALWAN_POW(alwan_max(Q / Q_w_safe, ALWAN_LITERAL(0.0)), Y_b_Y_w) *
               (ALWAN_LITERAL(1.64) - ALWAN_POW(ALWAN_LITERAL(0.29), Y_b_Y_w));
    result.M = ALWAN_POW(alwan_max(FL, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(0.15)) * result.C;
    }
    }
    }
    }
    return result;
}


#endif /* ALWAN_BACKEND */

#endif /* ALWAN_HUNT_CORE_H */
