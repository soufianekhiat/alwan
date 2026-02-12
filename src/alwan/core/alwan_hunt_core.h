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

/* ----------------------------------------------------------------
 * Hunt Appearance Correlates (local result struct)
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_scalar J;    /* Lightness */
    alwan_scalar C;    /* Chroma */
    alwan_scalar h;    /* Hue angle (degrees) */
    alwan_scalar s;    /* Saturation */
    alwan_scalar Q;    /* Brightness */
    alwan_scalar M;    /* Colourfulness */
} alwan_hunt_v_correlates;

/* ----------------------------------------------------------------
 * Hunt-Pointer-Estevez Matrix
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const HUNT_V_M_HPE[9] = {
#include "../data/matrices/hpe.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Nonlinear response function f_n (branchless)
 *
 * f_n(x) = sign(x) * 40 * |x|^0.73 / (|x|^0.73 + 2)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_scalar hunt_fn_v(alwan_scalar x) {
    alwan_scalar ax = ALWAN_ABS(x);
    alwan_scalar ax_pow = ALWAN_POW(ax, ALWAN_LITERAL(0.73));
    alwan_scalar magnitude = ALWAN_LITERAL(40.0) * ax_pow / (ax_pow + ALWAN_LITERAL(2.0));
    alwan_scalar sign = ALWAN_SELECT(x >= ALWAN_ZERO, ALWAN_ONE, -ALWAN_ONE);
    return sign * magnitude;
}

/* ----------------------------------------------------------------
 * Hunt Forward Transform: XYZ -> Correlates (value-returning)
 *
 * Parameters (all scalars — the .c wrapper resolves enums):
 *   xyz       : Test stimulus in CIE XYZ
 *   xyz_w_x/y/z : White point XYZ (Y=100)
 *   La        : Adapting luminance (cd/m^2)
 *   Yb        : Background luminance factor
 *   Nc        : Chromatic surround induction factor
 *   Nb        : Brightness surround induction factor
 *   D         : Degree of adaptation (0.0 = no discount, 1.0 = full)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_hunt_v_correlates alwan_hunt_forward_v(
    alwan_xyz xyz,
    alwan_scalar xyz_w_x,
    alwan_scalar xyz_w_y,
    alwan_scalar xyz_w_z,
    alwan_scalar La,
    alwan_scalar Yb,
    alwan_scalar Nc,
    alwan_scalar Nb,
    alwan_scalar D) {

    alwan_hunt_v_correlates result;

    /* Nc, Nb reserved for full Hunt surround model */
    (void)Nc; (void)Nb;

    /* Step 1: Compute adaptation factor k */
    alwan_scalar k = ALWAN_ONE / (ALWAN_LITERAL(5.0) * La + ALWAN_ONE);
    alwan_scalar k4 = k * k * k * k;

    /* Step 2: Compute luminance adaptation factor FL */
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * La) +
                      ALWAN_LITERAL(0.1) * (ALWAN_ONE - k4) * (ALWAN_ONE - k4) *
                      ALWAN_POW(ALWAN_LITERAL(5.0) * La, ALWAN_ONE / ALWAN_LITERAL(3.0));

    /* Step 3: Convert XYZ to LMS using HPE matrix (test stimulus) */
    alwan_scalar lms_l = HUNT_V_M_HPE[0] * xyz.x + HUNT_V_M_HPE[1] * xyz.y + HUNT_V_M_HPE[2] * xyz.z;
    alwan_scalar lms_m = HUNT_V_M_HPE[3] * xyz.x + HUNT_V_M_HPE[4] * xyz.y + HUNT_V_M_HPE[5] * xyz.z;
    alwan_scalar lms_s = HUNT_V_M_HPE[6] * xyz.x + HUNT_V_M_HPE[7] * xyz.y + HUNT_V_M_HPE[8] * xyz.z;

    /* Convert XYZ to LMS using HPE matrix (white point) */
    alwan_scalar lms_w_l = HUNT_V_M_HPE[0] * xyz_w_x + HUNT_V_M_HPE[1] * xyz_w_y + HUNT_V_M_HPE[2] * xyz_w_z;
    alwan_scalar lms_w_m = HUNT_V_M_HPE[3] * xyz_w_x + HUNT_V_M_HPE[4] * xyz_w_y + HUNT_V_M_HPE[5] * xyz_w_z;
    alwan_scalar lms_w_s = HUNT_V_M_HPE[6] * xyz_w_x + HUNT_V_M_HPE[7] * xyz_w_y + HUNT_V_M_HPE[8] * xyz_w_z;
    /* White point LMS reserved for full chromatic adaptation model */
    (void)lms_w_l; (void)lms_w_m; (void)lms_w_s;

    /* Step 4: Apply chromatic adaptation with discounting
     * D=1 -> full discount, D=0 -> no discount
     * adapted = (D + 1 - D) * lms = lms (simplifies to identity in original) */
    alwan_scalar adapt_l = (D + ALWAN_ONE - D) * lms_l;
    alwan_scalar adapt_m = (D + ALWAN_ONE - D) * lms_m;
    alwan_scalar adapt_s = (D + ALWAN_ONE - D) * lms_s;

    /* Step 5: Apply nonlinear response */
    alwan_scalar lms_n_l = hunt_fn_v(FL * adapt_l);
    alwan_scalar lms_n_m = hunt_fn_v(FL * adapt_m);
    alwan_scalar lms_n_s = hunt_fn_v(FL * adapt_s);

    /* Step 6: Compute opponent color dimensions */
    alwan_scalar C1 = lms_n_l - lms_n_m;
    alwan_scalar C2 = lms_n_m - lms_n_s;
    alwan_scalar C3 = lms_n_s - lms_n_l;

    /* Step 7: Compute hue angle h (degrees) */
    alwan_scalar h_num = ALWAN_LITERAL(0.5) * (C2 - C3) / ALWAN_LITERAL(4.5);
    alwan_scalar h_den = C1 - C2 / ALWAN_LITERAL(11.0);
    alwan_scalar h_rad = ALWAN_ATAN2(h_num, h_den);
    alwan_scalar h_deg = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    result.h = ALWAN_SELECT(h_deg < ALWAN_ZERO, h_deg + ALWAN_LITERAL(360.0), h_deg);

    /* Step 8: Eccentricity factor omitted (simplified model) */

    /* Step 9: Compute achromatic response A */
    alwan_scalar A = ALWAN_LITERAL(2.0) * lms_n_l + lms_n_m +
                     ALWAN_LITERAL(0.05) * lms_n_s - ALWAN_LITERAL(3.05) + ALWAN_ONE;

    /* Step 10: Compute brightness Q (simplified) */
    alwan_scalar N1 = ALWAN_POW(ALWAN_LITERAL(7.0), ALWAN_LITERAL(0.6));
    alwan_scalar M_total = ALWAN_SQRT(C1 * C1 + C2 * C2);
    result.Q = N1 * (A + M_total / ALWAN_LITERAL(100.0));

    /* Step 11: Compute lightness J (simplified)
     * Qw = N1 * 2.0 (simplified white point brightness) */
    alwan_scalar Qw = N1 * ALWAN_LITERAL(2.0);
    alwan_scalar Qw_safe = ALWAN_SELECT(Qw > ALWAN_LITERAL(1e-10), Qw, ALWAN_ONE);
    result.J = ALWAN_SELECT(Qw > ALWAN_LITERAL(1e-10),
                            ALWAN_LITERAL(100.0) * ALWAN_POW(result.Q / Qw_safe, ALWAN_LITERAL(0.67)),
                            ALWAN_ZERO);

    /* Step 12: Compute saturation s */
    alwan_scalar A_safe = ALWAN_SELECT(A > ALWAN_LITERAL(1e-10), A, ALWAN_ONE);
    result.s = ALWAN_SELECT(A > ALWAN_LITERAL(1e-10),
                            ALWAN_LITERAL(50.0) * M_total / A_safe,
                            ALWAN_ZERO);

    /* Step 13: Compute chroma C (simplified) */
    alwan_scalar Q_safe = ALWAN_SELECT(result.Q > ALWAN_LITERAL(1e-10), result.Q, ALWAN_ONE);
    alwan_scalar Yw = xyz_w_y;
    alwan_scalar Yb_Yw = ALWAN_SELECT(Yw > ALWAN_LITERAL(1e-10), Yb / Yw, ALWAN_ZERO);
    result.C = ALWAN_SELECT(result.Q > ALWAN_LITERAL(1e-10),
                            ALWAN_LITERAL(2.44) * ALWAN_POW(result.s, ALWAN_LITERAL(0.69)) *
                            ALWAN_POW(Q_safe / Qw_safe, Yb_Yw),
                            ALWAN_ZERO);

    /* Step 14: Compute colourfulness M */
    result.M = ALWAN_POW(FL, ALWAN_LITERAL(0.15)) * result.C;

    return result;
}

#endif /* ALWAN_HUNT_CORE_H */
