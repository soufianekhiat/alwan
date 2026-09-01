/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Hellwig2022 Color Appearance Model
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Hellwig & Fairchild (2022)
 * "Predicting lightness, chroma, and hue using IAM and CAM frameworks"
 *
 * The _v() functions take pre-resolved scalar parameters (F, c, Nc, etc.)
 * instead of enum-based viewing conditions, making them branchless
 * and cross-platform compatible.
 */

#ifndef ALWAN_HELLWIG2022_CORE_H
#define ALWAN_HELLWIG2022_CORE_H

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
#include "alwan_hellwig2022_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_hellwig2022_core.inc"
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
    alwan_scalar H;
} alwan_hellwig2022_v_correlates;

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 HW22_V_M_CAT16 = {{
#include "../data/matrices/cat_cat16.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 HW22_V_M_CAT16_INV = {{
#include "../data/matrices/cat_cat16_inv.csv"
}};
ALWAN_DIAG_POP

static const alwan_scalar HW22_V_H_I[5] = {
    ALWAN_LITERAL(20.14), ALWAN_LITERAL(90.00),
    ALWAN_LITERAL(164.25), ALWAN_LITERAL(237.53),
    ALWAN_LITERAL(380.14)
};
static const alwan_scalar HW22_V_E_I[4] = {
    ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.7),
    ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.2)
};
static const alwan_scalar HW22_V_HQ_I[4] = {
    ALWAN_LITERAL(0.0), ALWAN_LITERAL(100.0),
    ALWAN_LITERAL(200.0), ALWAN_LITERAL(300.0)
};

ALWAN_INLINE alwan_scalar hw22_compute_D_v(alwan_scalar F, alwan_scalar La, alwan_scalar discount) {
    alwan_scalar D_computed = F * (ALWAN_ONE - ALWAN_ONE / ALWAN_LITERAL(3.6) *
                     ALWAN_EXP((-La - ALWAN_LITERAL(42.0)) / ALWAN_LITERAL(92.0)));
    D_computed = alwan_clamp(D_computed, ALWAN_ZERO, ALWAN_ONE);
    return ALWAN_SELECT(discount > ALWAN_LITERAL(0.5), ALWAN_ONE, D_computed);
}

ALWAN_INLINE alwan_scalar hw22_compute_FL_v(alwan_scalar La) {
    alwan_scalar k = ALWAN_ONE / (ALWAN_LITERAL(5.0) * La + ALWAN_ONE);
    alwan_scalar k4 = k * k * k * k;
    return ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * La) +
           ALWAN_LITERAL(0.1) * (ALWAN_ONE - k4) * (ALWAN_ONE - k4) *
           ALWAN_POW(ALWAN_LITERAL(5.0) * La, ALWAN_ONE / ALWAN_LITERAL(3.0));
}

ALWAN_INLINE alwan_scalar hw22_post_adaptation_nonlinear_v(alwan_scalar x, alwan_scalar FL) {
    alwan_scalar x_FL = (FL * x) / ALWAN_LITERAL(100.0);
    alwan_scalar x_abs = ALWAN_ABS(x_FL);
    alwan_scalar x_pow = ALWAN_POW(x_abs, ALWAN_LITERAL(0.42));
    alwan_scalar result = ALWAN_LITERAL(400.0) * x_pow / (ALWAN_LITERAL(27.13) + x_pow) + ALWAN_LITERAL(0.1);
    return ALWAN_SELECT(x_FL >= ALWAN_ZERO, result, -result);
}

ALWAN_INLINE alwan_scalar hw22_post_adaptation_nonlinear_inv_v(alwan_scalar x, alwan_scalar FL) {
    alwan_scalar y = x - ALWAN_LITERAL(0.1);
    alwan_scalar y_abs = ALWAN_ABS(y);
    alwan_scalar result = ALWAN_POW((ALWAN_LITERAL(27.13) * y_abs) /
                     (ALWAN_LITERAL(400.0) - y_abs), ALWAN_ONE / ALWAN_LITERAL(0.42)) *
                     ALWAN_LITERAL(100.0) / FL;
    return ALWAN_SELECT(x >= ALWAN_ZERO, result, -result);
}

ALWAN_INLINE alwan_scalar hw22_eccentricity_v(alwan_scalar h_rad) {
    alwan_scalar h2 = ALWAN_LITERAL(2.0) * h_rad;
    alwan_scalar h3 = ALWAN_LITERAL(3.0) * h_rad;
    alwan_scalar h4 = ALWAN_LITERAL(4.0) * h_rad;
    return ALWAN_ONE +
           ALWAN_LITERAL(-0.0582) * ALWAN_COS(h_rad) +
           ALWAN_LITERAL(-0.0258) * ALWAN_COS(h2) +
           ALWAN_LITERAL(-0.1347) * ALWAN_COS(h3) +
           ALWAN_LITERAL( 0.0289) * ALWAN_COS(h4) +
           ALWAN_LITERAL(-0.1475) * ALWAN_SIN(h_rad) +
           ALWAN_LITERAL(-0.0308) * ALWAN_SIN(h2) +
           ALWAN_LITERAL( 0.0385) * ALWAN_SIN(h3) +
           ALWAN_LITERAL( 0.0096) * ALWAN_SIN(h4);
}

ALWAN_INLINE alwan_scalar hw22_hue_to_quadrature_v(alwan_scalar h) {
    h = ALWAN_SELECT(h < ALWAN_LITERAL(20.14), h + ALWAN_LITERAL(360.0), h);
    h = ALWAN_SELECT(h > ALWAN_LITERAL(380.14), h - ALWAN_LITERAL(360.0), h);
    alwan_scalar hp0 = (h - HW22_V_H_I[0]) / HW22_V_E_I[0];
    alwan_scalar H0 = HW22_V_HQ_I[0] + (ALWAN_LITERAL(100.0) * hp0) / (hp0 + (HW22_V_H_I[1] - h) / HW22_V_E_I[0]);
    alwan_scalar hp1 = (h - HW22_V_H_I[1]) / HW22_V_E_I[1];
    alwan_scalar H1 = HW22_V_HQ_I[1] + (ALWAN_LITERAL(100.0) * hp1) / (hp1 + (HW22_V_H_I[2] - h) / HW22_V_E_I[1]);
    alwan_scalar hp2 = (h - HW22_V_H_I[2]) / HW22_V_E_I[2];
    alwan_scalar H2 = HW22_V_HQ_I[2] + (ALWAN_LITERAL(100.0) * hp2) / (hp2 + (HW22_V_H_I[3] - h) / HW22_V_E_I[2]);
    alwan_scalar hp3 = (h - HW22_V_H_I[3]) / HW22_V_E_I[3];
    alwan_scalar H3 = HW22_V_HQ_I[3] + (ALWAN_LITERAL(100.0) * hp3) / (hp3 + (HW22_V_H_I[4] - h) / HW22_V_E_I[3]);
    alwan_scalar H_result = ALWAN_SELECT(h < HW22_V_H_I[1], H0,
                            ALWAN_SELECT(h < HW22_V_H_I[2], H1,
                            ALWAN_SELECT(h < HW22_V_H_I[3], H2, H3)));
    return H_result;
}

ALWAN_INLINE alwan_scalar hw22_compute_Aw_v(
    alwan_scalar white_x, alwan_scalar white_y, alwan_scalar white_z,
    alwan_scalar D, alwan_scalar FL,
    ALWAN_PARAM_SCALAR_OUT RGB_w0_out, ALWAN_PARAM_SCALAR_OUT RGB_w1_out, ALWAN_PARAM_SCALAR_OUT RGB_w2_out) {
    /* White point XYZ -> RGB via CAT16 */
    alwan_vec3 white_v = {{white_x, white_y, white_z}};
    alwan_vec3 RGB_w_v = alwan_mat3_mulv_v(HW22_V_M_CAT16, white_v);
    alwan_scalar RGB_w0 = RGB_w_v.v[0];
    alwan_scalar RGB_w1 = RGB_w_v.v[1];
    alwan_scalar RGB_w2 = RGB_w_v.v[2];

    /* Store for caller */
    *RGB_w0_out = RGB_w0;
    *RGB_w1_out = RGB_w1;
    *RGB_w2_out = RGB_w2;

    /* Chromatic adaptation of the white onto itself. The per-channel factor
     * collapses to D + 1 - D = 1 by construction here; it is written out rather
     * than folded away so this stays visibly the same expression as the sample
     * path above it. */
    alwan_scalar RGB_wc0 = (white_y * D / RGB_w0 + ALWAN_ONE - D) * RGB_w0;
    alwan_scalar RGB_wc1 = (white_y * D / RGB_w1 + ALWAN_ONE - D) * RGB_w1;
    alwan_scalar RGB_wc2 = (white_y * D / RGB_w2 + ALWAN_ONE - D) * RGB_w2;

    /* Post-adaptation nonlinear compression of white */
    alwan_scalar R_aw = hw22_post_adaptation_nonlinear_v(RGB_wc0, FL);
    alwan_scalar G_aw = hw22_post_adaptation_nonlinear_v(RGB_wc1, FL);
    alwan_scalar B_aw = hw22_post_adaptation_nonlinear_v(RGB_wc2, FL);

    /* Hellwig2022 achromatic response for white */
    return ALWAN_LITERAL(2.0) * R_aw + G_aw + ALWAN_LITERAL(0.05) * B_aw - ALWAN_LITERAL(0.305);
}

ALWAN_INLINE alwan_hellwig2022_v_correlates alwan_hellwig2022_forward_v(
    alwan_xyz xyz,
    alwan_scalar white_x, alwan_scalar white_y, alwan_scalar white_z,
    alwan_scalar F, alwan_scalar c, alwan_scalar Nc,
    alwan_scalar La, alwan_scalar Y_b, alwan_scalar Y_w,
    alwan_scalar discount_illuminant_as_scalar) {
    alwan_hellwig2022_v_correlates result;

    /* Step 1: Degree of adaptation D */
    alwan_scalar D = hw22_compute_D_v(F, La, discount_illuminant_as_scalar);

    /* Step 2: Luminance level adaptation factor FL */
    alwan_scalar FL = hw22_compute_FL_v(La);

    /* Step 3: White-point achromatic response A_w and white RGB_w */
    alwan_scalar RGB_w0, RGB_w1, RGB_w2;
    alwan_scalar A_w = hw22_compute_Aw_v(white_x, white_y, white_z, D, FL,
                                         &RGB_w0, &RGB_w1, &RGB_w2);

    /* Step 4: Transform test color XYZ -> RGB via CAT16 */
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 RGB_v = alwan_mat3_mulv_v(HW22_V_M_CAT16, xyz_v);
    alwan_scalar RGB0 = RGB_v.v[0];
    alwan_scalar RGB1 = RGB_v.v[1];
    alwan_scalar RGB2 = RGB_v.v[2];

    /* Step 5: Chromatic adaptation */
    alwan_scalar RGB_c0 = (white_y * D / RGB_w0 + ALWAN_ONE - D) * RGB0;
    alwan_scalar RGB_c1 = (white_y * D / RGB_w1 + ALWAN_ONE - D) * RGB1;
    alwan_scalar RGB_c2 = (white_y * D / RGB_w2 + ALWAN_ONE - D) * RGB2;

    /* Step 6: Post-adaptation nonlinear compression (directly on RGB_c, no post-adaptation matrix) */
    alwan_scalar R_a = hw22_post_adaptation_nonlinear_v(RGB_c0, FL);
    alwan_scalar G_a = hw22_post_adaptation_nonlinear_v(RGB_c1, FL);
    alwan_scalar B_a = hw22_post_adaptation_nonlinear_v(RGB_c2, FL);

    /* Step 7: Opponent dimensions */
    alwan_scalar a = R_a - ALWAN_LITERAL(12.0) * G_a / ALWAN_LITERAL(11.0) + B_a / ALWAN_LITERAL(11.0);
    alwan_scalar b = (R_a + G_a - ALWAN_LITERAL(2.0) * B_a) / ALWAN_LITERAL(9.0);

    /* Step 8: Hue angle h */
    alwan_scalar h_rad = ALWAN_ATAN2(b, a);
    alwan_scalar h_deg = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    h_deg = ALWAN_SELECT(h_deg < ALWAN_ZERO, h_deg + ALWAN_LITERAL(360.0), h_deg);

    /* Step 9: Eccentricity factor et (Hellwig2022 Fourier series) */
    alwan_scalar et = hw22_eccentricity_v(h_rad);

    /* Step 10: Achromatic response A.
     * A = 2R + G + 0.05B - 0.305, with no N_bb scaling and no N_c. That is
     * Hellwig2022's own departure from CAM16, not a shortcut taken here:
     * dropping the induction factors is the paper's contribution. Matches
     * colour.appearance.XYZ_to_Hellwig2022 to 3.9e-10. */
    alwan_scalar A = ALWAN_LITERAL(2.0) * R_a + G_a + ALWAN_LITERAL(0.05) * B_a - ALWAN_LITERAL(0.305);

    /* Step 11: Base exponential nonlinearity z */
    alwan_scalar n = Y_b / Y_w;
    alwan_scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);

    /* Step 12: Lightness J */
    result.J = ALWAN_LITERAL(100.0) * ALWAN_POW(A / A_w, c * z);

    /* Step 13: Colourfulness M = 43 * N_c * e_t * hypot(a, b), as published. */
    result.M = ALWAN_LITERAL(43.0) * Nc * et * ALWAN_SQRT(a * a + b * b);

    /* Step 14: Chroma C = 35 * M / A_w, as published. */
    result.C = ALWAN_LITERAL(35.0) * result.M / A_w;

    /* Step 15: Brightness Q = (2/c) * (J/100) * A_w, as published. */
    result.Q = (ALWAN_LITERAL(2.0) / c) * (result.J / ALWAN_LITERAL(100.0)) * A_w;

    /* Step 16: Saturation s (no sqrt in Hellwig2022) */
    result.s = ALWAN_SELECT(result.Q > ALWAN_EPSILON,
                            ALWAN_LITERAL(100.0) * result.M / result.Q,
                            ALWAN_ZERO);

    /* Step 17: Hue composition H */
    result.h = h_deg;
    result.H = hw22_hue_to_quadrature_v(h_deg);

    return result;
}

ALWAN_INLINE alwan_xyz alwan_hellwig2022_inverse_v(
    alwan_hellwig2022_v_correlates correlates,
    alwan_scalar white_x, alwan_scalar white_y, alwan_scalar white_z,
    alwan_scalar F, alwan_scalar c, alwan_scalar Nc,
    alwan_scalar La, alwan_scalar Y_b, alwan_scalar Y_w,
    alwan_scalar discount_illuminant_as_scalar) {
    alwan_xyz result;

    /* Step 1: Degree of adaptation D */
    alwan_scalar D = hw22_compute_D_v(F, La, discount_illuminant_as_scalar);

    /* Step 2: Luminance level adaptation factor FL */
    alwan_scalar FL = hw22_compute_FL_v(La);

    /* Step 3: White-point achromatic response A_w and white RGB_w */
    alwan_scalar RGB_w0, RGB_w1, RGB_w2;
    alwan_scalar A_w = hw22_compute_Aw_v(white_x, white_y, white_z, D, FL,
                                         &RGB_w0, &RGB_w1, &RGB_w2);

    /* Step 4: Base exponential nonlinearity z */
    alwan_scalar n = Y_b / Y_w;
    alwan_scalar z = ALWAN_LITERAL(1.48) + ALWAN_SQRT(n);

    /* Step 5: Recover achromatic response A from lightness J */
    alwan_scalar A = A_w * ALWAN_POW(correlates.J / ALWAN_LITERAL(100.0), ALWAN_ONE / (c * z));

    /* Step 6: Recover opponent dimensions a, b from hue and chroma */
    alwan_scalar h_rad = correlates.h * ALWAN_PI / ALWAN_LITERAL(180.0);

    /* Hellwig2022 Fourier series eccentricity */
    alwan_scalar et = hw22_eccentricity_v(h_rad);

    /* C = 35*M/A_w => M = C*A_w/35 */
    alwan_scalar M = correlates.C * A_w / ALWAN_LITERAL(35.0);

    /* M = 43*Nc*et*sqrt(a^2+b^2) => sqrt(a^2+b^2) = M / (43*Nc*et) */
    alwan_scalar ab_magnitude = M / (ALWAN_LITERAL(43.0) * Nc * et);

    alwan_scalar a = ALWAN_COS(h_rad) * ab_magnitude;
    alwan_scalar b = ALWAN_SIN(h_rad) * ab_magnitude;

    /* Step 7: Recover post-adaptation cone responses from opponent dimensions */
    alwan_scalar p2 = A + ALWAN_LITERAL(0.305);
    alwan_scalar R_a = (ALWAN_LITERAL(460.0) * p2 + ALWAN_LITERAL(451.0) * a + ALWAN_LITERAL(288.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar G_a = (ALWAN_LITERAL(460.0) * p2 - ALWAN_LITERAL(891.0) * a - ALWAN_LITERAL(261.0) * b) / ALWAN_LITERAL(1403.0);
    alwan_scalar B_a = (ALWAN_LITERAL(460.0) * p2 - ALWAN_LITERAL(220.0) * a - ALWAN_LITERAL(6300.0) * b) / ALWAN_LITERAL(1403.0);

    /* Step 8: Inverse nonlinear compression -> RGB_c (no post-adaptation matrix in Hellwig2022) */
    alwan_scalar RGB_c0 = hw22_post_adaptation_nonlinear_inv_v(R_a, FL);
    alwan_scalar RGB_c1 = hw22_post_adaptation_nonlinear_inv_v(G_a, FL);
    alwan_scalar RGB_c2 = hw22_post_adaptation_nonlinear_inv_v(B_a, FL);

    /* Step 9: Reverse chromatic adaptation */
    alwan_scalar RGB0 = RGB_c0 / (white_y * D / RGB_w0 + ALWAN_ONE - D);
    alwan_scalar RGB1 = RGB_c1 / (white_y * D / RGB_w1 + ALWAN_ONE - D);
    alwan_scalar RGB2 = RGB_c2 / (white_y * D / RGB_w2 + ALWAN_ONE - D);

    /* Step 10: RGB -> XYZ via inverse CAT16 */
    alwan_vec3 RGB_inv_v = {{RGB0, RGB1, RGB2}};
    alwan_vec3 xyz_out_v = alwan_mat3_mulv_v(HW22_V_M_CAT16_INV, RGB_inv_v);
    result.x = xyz_out_v.v[0];
    result.y = xyz_out_v.v[1];
    result.z = xyz_out_v.v[2];

    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_HELLWIG2022_CORE_H */
