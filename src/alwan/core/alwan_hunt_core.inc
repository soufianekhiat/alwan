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
 * Hunt Forward Transform: XYZ -> Correlates (value-returning)
 *
 * Parameters (all scalars -- the .c wrapper resolves enums):
 *   xyz       : Test stimulus in CIE XYZ
 *   xyz_w_x/y/z : White point XYZ (Y=100)
 *   La        : Adapting luminance (cd/m^2)
 *   Yb        : Background luminance factor
 *   Nc        : Chromatic surround induction factor
 *   Nb        : Brightness surround induction factor
 *   D         : Degree of adaptation (0.0 = no discount, 1.0 = full)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_hunt_v_correlates) ALWAN_CORE_FNVLIT(alwan_hunt_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_T xyz_w_x,
    ALWAN_CORE_T xyz_w_y,
    ALWAN_CORE_T xyz_w_z,
    ALWAN_CORE_T La,
    ALWAN_CORE_T Yb,
    ALWAN_CORE_T Nc,
    ALWAN_CORE_T Nb,
    ALWAN_CORE_T D) {

    ALWAN_CORE_FNLIT(alwan_hunt_v_correlates) result;

    /* Nc, Nb reserved for full Hunt surround model */
    ALWAN_UNUSED(Nc); ALWAN_UNUSED(Nb);

    /* Step 1: Compute adaptation factor k */
    ALWAN_CORE_T k = ALWAN_CORE_ONE / (ALWAN_CORE_LITERAL(5.0) * La + ALWAN_CORE_ONE);
    ALWAN_CORE_T k4 = k * k * k * k;

    /* Step 2: Compute luminance adaptation factor FL */
    ALWAN_CORE_T FL = ALWAN_CORE_LITERAL(0.2) * k4 * (ALWAN_CORE_LITERAL(5.0) * La) +
                      ALWAN_CORE_LITERAL(0.1) * (ALWAN_CORE_ONE - k4) * (ALWAN_CORE_ONE - k4) *
                      ALWAN_CORE_POW(ALWAN_CORE_LITERAL(5.0) * La, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));

    /* Step 3: Convert XYZ to LMS using HPE matrix (test stimulus) */
    ALWAN_CORE_VEC3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HUNT_V_M_HPE), xyz_v);
    ALWAN_CORE_T lms_l = lms.v[0];
    ALWAN_CORE_T lms_m = lms.v[1];
    ALWAN_CORE_T lms_s = lms.v[2];

    /* Convert XYZ to LMS using HPE matrix (white point) */
    ALWAN_CORE_VEC3 xyz_w_v = {{xyz_w_x, xyz_w_y, xyz_w_z}};
    ALWAN_CORE_VEC3 lms_w = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(HUNT_V_M_HPE), xyz_w_v);
    ALWAN_CORE_T lms_w_l = lms_w.v[0];
    ALWAN_CORE_T lms_w_m = lms_w.v[1];
    ALWAN_CORE_T lms_w_s = lms_w.v[2];
    /* White point LMS reserved for full chromatic adaptation model */
    ALWAN_UNUSED(lms_w_l); ALWAN_UNUSED(lms_w_m); ALWAN_UNUSED(lms_w_s);

    /* Step 4: Apply chromatic adaptation with discounting
     * D=1 -> full discount, D=0 -> no discount
     * adapted = (D + 1 - D) * lms = lms (simplifies to identity in original) */
    ALWAN_CORE_T adapt_l = (D + ALWAN_CORE_ONE - D) * lms_l;
    ALWAN_CORE_T adapt_m = (D + ALWAN_CORE_ONE - D) * lms_m;
    ALWAN_CORE_T adapt_s = (D + ALWAN_CORE_ONE - D) * lms_s;

    /* Step 5: Apply nonlinear response */
    ALWAN_CORE_T lms_n_l = ALWAN_CORE_FNVLIT(hunt_fn)(FL * adapt_l);
    ALWAN_CORE_T lms_n_m = ALWAN_CORE_FNVLIT(hunt_fn)(FL * adapt_m);
    ALWAN_CORE_T lms_n_s = ALWAN_CORE_FNVLIT(hunt_fn)(FL * adapt_s);

    /* Step 6: Compute opponent color dimensions */
    ALWAN_CORE_T C1 = lms_n_l - lms_n_m;
    ALWAN_CORE_T C2 = lms_n_m - lms_n_s;
    ALWAN_CORE_T C3 = lms_n_s - lms_n_l;

    /* Step 7: Compute hue angle h (degrees) */
    ALWAN_CORE_T h_num = ALWAN_CORE_LITERAL(0.5) * (C2 - C3) / ALWAN_CORE_LITERAL(4.5);
    ALWAN_CORE_T h_den = C1 - C2 / ALWAN_CORE_LITERAL(11.0);
    ALWAN_CORE_T h_rad = ALWAN_CORE_ATAN2(h_num, h_den);
    ALWAN_CORE_T h_deg = h_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    result.h = ALWAN_CORE_SELECT(h_deg < ALWAN_CORE_ZERO, h_deg + ALWAN_CORE_LITERAL(360.0), h_deg);

    /* Step 8: Eccentricity factor omitted (simplified model) */

    /* Step 9: Compute achromatic response A */
    ALWAN_CORE_T A = ALWAN_CORE_LITERAL(2.0) * lms_n_l + lms_n_m +
                     ALWAN_CORE_LITERAL(0.05) * lms_n_s - ALWAN_CORE_LITERAL(3.05) + ALWAN_CORE_ONE;

    /* Step 10: Compute brightness Q (simplified) */
    ALWAN_CORE_T N1 = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(7.0), ALWAN_CORE_LITERAL(0.6));
    ALWAN_CORE_T M_total = ALWAN_CORE_SQRT(C1 * C1 + C2 * C2);
    result.Q = N1 * (A + M_total / ALWAN_CORE_LITERAL(100.0));

    /* Step 11: Compute lightness J (simplified)
     * Qw = N1 * 2.0 (simplified white point brightness) */
    ALWAN_CORE_T Qw = N1 * ALWAN_CORE_LITERAL(2.0);
    ALWAN_CORE_T Qw_safe = ALWAN_CORE_SELECT(Qw > ALWAN_CORE_LITERAL(1e-10), Qw, ALWAN_CORE_ONE);
    result.J = ALWAN_CORE_SELECT(Qw > ALWAN_CORE_LITERAL(1e-10),
                            ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_POW(result.Q / Qw_safe, ALWAN_CORE_LITERAL(0.67)),
                            ALWAN_CORE_ZERO);

    /* Step 12: Compute saturation s */
    ALWAN_CORE_T A_safe = ALWAN_CORE_SELECT(A > ALWAN_CORE_LITERAL(1e-10), A, ALWAN_CORE_ONE);
    result.s = ALWAN_CORE_SELECT(A > ALWAN_CORE_LITERAL(1e-10),
                            ALWAN_CORE_LITERAL(50.0) * M_total / A_safe,
                            ALWAN_CORE_ZERO);

    /* Step 13: Compute chroma C (simplified) */
    ALWAN_CORE_T Q_safe = ALWAN_CORE_SELECT(result.Q > ALWAN_CORE_LITERAL(1e-10), result.Q, ALWAN_CORE_ONE);
    ALWAN_CORE_T Yw = xyz_w_y;
    ALWAN_CORE_T Yb_Yw = ALWAN_CORE_SELECT(Yw > ALWAN_CORE_LITERAL(1e-10), Yb / Yw, ALWAN_CORE_ZERO);
    result.C = ALWAN_CORE_SELECT(result.Q > ALWAN_CORE_LITERAL(1e-10),
                            ALWAN_CORE_LITERAL(2.44) * ALWAN_CORE_POW(result.s, ALWAN_CORE_LITERAL(0.69)) *
                            ALWAN_CORE_POW(Q_safe / Qw_safe, Yb_Yw),
                            ALWAN_CORE_ZERO);

    /* Step 14: Compute colourfulness M */
    result.M = ALWAN_CORE_POW(FL, ALWAN_CORE_LITERAL(0.15)) * result.C;

    return result;
}
