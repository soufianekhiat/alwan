/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Kim, Weyrich and Kautz (2009) Color Appearance Model (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Kim, M. H., Weyrich, T., & Kautz, J. (2009). Modeling Human Color
 * Perception under Extended Luminance Levels. ACM Transactions on Graphics, 28(3).
 */

/* ----------------------------------------------------------------
 * Kim2009 Result Struct (suffixed per precision)
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T J;  /* Lightness */
    ALWAN_CORE_T C;  /* Chroma */
    ALWAN_CORE_T h;  /* Hue angle (degrees) */
} ALWAN_CORE_FNLIT(alwan_kim2009_v_correlates);

/* ----------------------------------------------------------------
 * Kim2009 Constants
 * ---------------------------------------------------------------- */

/* Cone response sigmoidal curve modulation factor */
#define KIM2009_V_N_C  ALWAN_CORE_LITERAL(0.57)

/* Brightness exponent */
#define KIM2009_V_N_Q  ALWAN_CORE_LITERAL(0.1308)

/* Chroma scaling and exponent */
#define KIM2009_V_A_K  ALWAN_CORE_LITERAL(456.5)
#define KIM2009_V_N_K  ALWAN_CORE_LITERAL(0.62)

/* Colourfulness coefficients */
#define KIM2009_V_A_M  ALWAN_CORE_LITERAL(0.11)
#define KIM2009_V_B_M  ALWAN_CORE_LITERAL(0.61)

/* Lightness function coefficients */
#define KIM2009_V_A_J  ALWAN_CORE_LITERAL(0.89)
#define KIM2009_V_B_J  ALWAN_CORE_LITERAL(0.24)
#define KIM2009_V_N_J  ALWAN_CORE_LITERAL(3.65)
#define KIM2009_V_O_J  ALWAN_CORE_LITERAL(0.65)

/* Achromatic signal weights [40, 20, 1] / 61 */
#define KIM2009_V_W_L    ALWAN_CORE_LITERAL(40.0)
#define KIM2009_V_W_M    ALWAN_CORE_LITERAL(20.0)
#define KIM2009_V_W_S    ALWAN_CORE_LITERAL(1.0)
#define KIM2009_V_W_SUM  ALWAN_CORE_LITERAL(61.0)

/* ----------------------------------------------------------------
 * Kim2009 Transformation Matrices (CSV includes)
 * ---------------------------------------------------------------- */

/* CAT02 matrix for chromatic adaptation transform */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(KIM2009_V_M_CAT02) = {{
#include "../data/matrices/cat_cat02.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(KIM2009_V_M_CAT02_INV) = {{
#include "../data/matrices/cat_cat02_inv.csv"
}};
/* Hunt-Pointer-Estevez (HPE) matrix for XYZ to LMS conversion */
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(KIM2009_V_M_HPE) = {{
#include "../data/matrices/hpe.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(KIM2009_V_M_HPE_INV) = {{
#include "../data/matrices/hpe_inv.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Kim2009 Forward: XYZ -> Appearance Correlates (value-returning)
 * ---------------------------------------------------------------- */

/* Sign-preserving power, sign(x) * |x|^p.
 *
 * The cone responses below can be negative: the HPE matrix maps a saturated or
 * out-of-gamut XYZ to a negative L, M or S, and a plain pow() of that is NaN.
 * The forward transform then returned ALWAN_OK carrying NaN, which on an
 * ordinary frame was 79% of the pixels.
 *
 * colour-science uses spow at exactly these sites (LMS, LMS_w, L_A and J_p in
 * appearance/kim2009.py), so this follows the reference rather than inventing a
 * guard. Where the argument is provably non-negative, the o_j constant and the
 * hypot in the chroma step, plain pow is kept. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(kim2009_spow)(ALWAN_CORE_T x, ALWAN_CORE_T p) {
    ALWAN_CORE_T m = ALWAN_CORE_POW(ALWAN_CORE_ABS(x), p);
    return ALWAN_CORE_SELECT(x < ALWAN_CORE_ZERO, -m, m);
}
ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_kim2009_v_correlates) ALWAN_CORE_FNVLIT(alwan_kim2009_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_T white_x,
    ALWAN_CORE_T white_y,
    ALWAN_CORE_T white_z,
    ALWAN_CORE_T La,
    ALWAN_CORE_T D,
    ALWAN_CORE_T media_E
) {
    ALWAN_CORE_FNLIT(alwan_kim2009_v_correlates) result;

    /* Step 1: CAT02 transform - XYZ to RGB (sharpened cone responses) */
    ALWAN_CORE_VEC3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    ALWAN_CORE_VEC3 rgb_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(KIM2009_V_M_CAT02), xyz_v);
    ALWAN_CORE_T R   = rgb_v.v[0];
    ALWAN_CORE_T G   = rgb_v.v[1];
    ALWAN_CORE_T B   = rgb_v.v[2];

    /* Step 2: CAT02 transform - white point XYZ_w to RGB_w */
    ALWAN_CORE_VEC3 white_v = {{white_x, white_y, white_z}};
    ALWAN_CORE_VEC3 rgbw_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(KIM2009_V_M_CAT02), white_v);
    ALWAN_CORE_T R_w = rgbw_v.v[0];
    ALWAN_CORE_T G_w = rgbw_v.v[1];
    ALWAN_CORE_T B_w = rgbw_v.v[2];

    /* Step 3: Chromatic adaptation factors */
    ALWAN_CORE_T fac_R = D * (white_y / R_w) + (ALWAN_CORE_ONE - D);
    ALWAN_CORE_T fac_G = D * (white_y / G_w) + (ALWAN_CORE_ONE - D);
    ALWAN_CORE_T fac_B = D * (white_y / B_w) + (ALWAN_CORE_ONE - D);

    /* Step 4: Apply chromatic adaptation (test stimulus and white) */
    ALWAN_CORE_T R_c  = R   * fac_R;
    ALWAN_CORE_T G_c  = G   * fac_G;
    ALWAN_CORE_T B_c  = B   * fac_B;
    ALWAN_CORE_T R_wc = R_w * fac_R;
    ALWAN_CORE_T G_wc = G_w * fac_G;
    ALWAN_CORE_T B_wc = B_w * fac_B;

    /* Step 5: Inverse CAT02 to adapted XYZ_c */
    ALWAN_CORE_VEC3 rgbc_v = {{R_c, G_c, B_c}};
    ALWAN_CORE_VEC3 xyzc_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(KIM2009_V_M_CAT02_INV), rgbc_v);
    ALWAN_CORE_T Xc = xyzc_v.v[0];
    ALWAN_CORE_T Yc = xyzc_v.v[1];
    ALWAN_CORE_T Zc = xyzc_v.v[2];

    /* Step 6: Inverse CAT02 to adapted XYZ_wc */
    ALWAN_CORE_VEC3 rgbwc_v = {{R_wc, G_wc, B_wc}};
    ALWAN_CORE_VEC3 xyzwc_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(KIM2009_V_M_CAT02_INV), rgbwc_v);
    ALWAN_CORE_T Xwc = xyzwc_v.v[0];
    ALWAN_CORE_T Ywc = xyzwc_v.v[1];
    ALWAN_CORE_T Zwc = xyzwc_v.v[2];

    /* Step 7: HPE transform - convert to cone fundamentals LMS and LMS_w */
    ALWAN_CORE_VEC3 xyzc_in = {{Xc, Yc, Zc}};
    ALWAN_CORE_VEC3 lms_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(KIM2009_V_M_HPE), xyzc_in);
    ALWAN_CORE_T L   = lms_v.v[0];
    ALWAN_CORE_T M   = lms_v.v[1];
    ALWAN_CORE_T S   = lms_v.v[2];

    ALWAN_CORE_VEC3 xyzwc_in = {{Xwc, Ywc, Zwc}};
    ALWAN_CORE_VEC3 lmsw_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(KIM2009_V_M_HPE), xyzwc_in);
    ALWAN_CORE_T L_w = lmsw_v.v[0];
    ALWAN_CORE_T M_w = lmsw_v.v[1];
    ALWAN_CORE_T S_w = lmsw_v.v[2];

    /* Step 8: Cone response - power law with additive term (n_c = 0.57) */
    ALWAN_CORE_T La_nc = ALWAN_CORE_FNVLIT(kim2009_spow)(La, KIM2009_V_N_C);

    ALWAN_CORE_T L_nc   = ALWAN_CORE_FNVLIT(kim2009_spow)(L, KIM2009_V_N_C);
    ALWAN_CORE_T M_nc   = ALWAN_CORE_FNVLIT(kim2009_spow)(M, KIM2009_V_N_C);
    ALWAN_CORE_T S_nc   = ALWAN_CORE_FNVLIT(kim2009_spow)(S, KIM2009_V_N_C);

    ALWAN_CORE_T L_w_nc = ALWAN_CORE_FNVLIT(kim2009_spow)(L_w, KIM2009_V_N_C);
    ALWAN_CORE_T M_w_nc = ALWAN_CORE_FNVLIT(kim2009_spow)(M_w, KIM2009_V_N_C);
    ALWAN_CORE_T S_w_nc = ALWAN_CORE_FNVLIT(kim2009_spow)(S_w, KIM2009_V_N_C);

    ALWAN_CORE_T Lp   = L_nc   / (L_nc   + La_nc);
    ALWAN_CORE_T Mp   = M_nc   / (M_nc   + La_nc);
    ALWAN_CORE_T Sp   = S_nc   / (S_nc   + La_nc);

    ALWAN_CORE_T Lp_w = L_w_nc / (L_w_nc + La_nc);
    ALWAN_CORE_T Mp_w = M_w_nc / (M_w_nc + La_nc);
    ALWAN_CORE_T Sp_w = S_w_nc / (S_w_nc + La_nc);

    /* Step 9: Achromatic signals A and A_w */
    ALWAN_CORE_T A   = (KIM2009_V_W_L * Lp   + KIM2009_V_W_M * Mp   + KIM2009_V_W_S * Sp  ) / KIM2009_V_W_SUM;
    ALWAN_CORE_T A_w = (KIM2009_V_W_L * Lp_w + KIM2009_V_W_M * Mp_w + KIM2009_V_W_S * Sp_w) / KIM2009_V_W_SUM;

    /* Step 10: Perceived lightness J_p */
    ALWAN_CORE_T A_A_w      = A / A_w;
    ALWAN_CORE_T o_j_n_j    = ALWAN_CORE_POW(KIM2009_V_O_J, KIM2009_V_N_J);
    ALWAN_CORE_T num_jp     = -(A_A_w - KIM2009_V_B_J) * o_j_n_j;
    ALWAN_CORE_T den_jp     = A_A_w - KIM2009_V_B_J - KIM2009_V_A_J;
    ALWAN_CORE_T J_p        = ALWAN_CORE_FNVLIT(kim2009_spow)(num_jp / den_jp, ALWAN_CORE_ONE / KIM2009_V_N_J);

    /* Step 11: Media-dependent lightness J */
    ALWAN_CORE_T J_val = ALWAN_CORE_LITERAL(100.0) * (media_E * (J_p - ALWAN_CORE_ONE) + ALWAN_CORE_ONE);

    /* Step 12: Opponent signals a and b for chroma computation */
    ALWAN_CORE_T opp_a = (ALWAN_CORE_LITERAL(11.0) * Lp -
                          ALWAN_CORE_LITERAL(12.0) * Mp +
                          ALWAN_CORE_ONE * Sp) / ALWAN_CORE_LITERAL(11.0);

    ALWAN_CORE_T opp_b = (ALWAN_CORE_ONE * Lp +
                          ALWAN_CORE_ONE * Mp -
                          ALWAN_CORE_LITERAL(2.0) * Sp) / ALWAN_CORE_LITERAL(9.0);

    /* Step 13: Chroma C = a_k * sqrt(a^2 + b^2)^n_k */
    ALWAN_CORE_T C_val = KIM2009_V_A_K * ALWAN_CORE_POW(ALWAN_CORE_SQRT(opp_a * opp_a + opp_b * opp_b), KIM2009_V_N_K);

    /* Step 14: Hue angle h (degrees) */
    ALWAN_CORE_T h_rad = ALWAN_CORE_ATAN2(opp_b, opp_a);
    ALWAN_CORE_T h_deg = h_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    h_deg = ALWAN_CORE_SELECT(h_deg < ALWAN_CORE_ZERO, h_deg + ALWAN_CORE_LITERAL(360.0), h_deg);

    result.J = J_val;
    result.C = C_val;
    result.h = h_deg;

    return result;
}

/* ----------------------------------------------------------------
 * Kim2009 Inverse: Appearance Correlates -> XYZ (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_kim2009_inverse)(
    ALWAN_CORE_FNLIT(alwan_kim2009_v_correlates) correlates,
    ALWAN_CORE_T white_x,
    ALWAN_CORE_T white_y,
    ALWAN_CORE_T white_z,
    ALWAN_CORE_T La,
    ALWAN_CORE_T D,
    ALWAN_CORE_T media_E
) {
    ALWAN_CORE_XYZ result;

    /* Step 1: CAT02 transform - white point to RGB_w */
    ALWAN_CORE_VEC3 white_v = {{white_x, white_y, white_z}};
    ALWAN_CORE_VEC3 rgbw_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(KIM2009_V_M_CAT02), white_v);
    ALWAN_CORE_T R_w = rgbw_v.v[0];
    ALWAN_CORE_T G_w = rgbw_v.v[1];
    ALWAN_CORE_T B_w = rgbw_v.v[2];

    /* Step 2: Chromatic adaptation factors for white */
    ALWAN_CORE_T fac_R = D * (white_y / R_w) + (ALWAN_CORE_ONE - D);
    ALWAN_CORE_T fac_G = D * (white_y / G_w) + (ALWAN_CORE_ONE - D);
    ALWAN_CORE_T fac_B = D * (white_y / B_w) + (ALWAN_CORE_ONE - D);

    /* Step 3: Apply chromatic adaptation to white */
    ALWAN_CORE_T R_wc = R_w * fac_R;
    ALWAN_CORE_T G_wc = G_w * fac_G;
    ALWAN_CORE_T B_wc = B_w * fac_B;

    /* Step 4: Inverse CAT02 to adapted XYZ_wc */
    ALWAN_CORE_VEC3 rgbwc_v = {{R_wc, G_wc, B_wc}};
    ALWAN_CORE_VEC3 xyzwc_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(KIM2009_V_M_CAT02_INV), rgbwc_v);
    ALWAN_CORE_T Xwc = xyzwc_v.v[0];
    ALWAN_CORE_T Ywc = xyzwc_v.v[1];
    ALWAN_CORE_T Zwc = xyzwc_v.v[2];

    /* Step 5: HPE transform - white to LMS_w */
    ALWAN_CORE_VEC3 xyzwc_in = {{Xwc, Ywc, Zwc}};
    ALWAN_CORE_VEC3 lmsw_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(KIM2009_V_M_HPE), xyzwc_in);
    ALWAN_CORE_T L_w = lmsw_v.v[0];
    ALWAN_CORE_T M_w = lmsw_v.v[1];
    ALWAN_CORE_T S_w = lmsw_v.v[2];

    /* Step 6: Cone response for white */
    ALWAN_CORE_T La_nc = ALWAN_CORE_FNVLIT(kim2009_spow)(La, KIM2009_V_N_C);

    ALWAN_CORE_T L_w_nc = ALWAN_CORE_FNVLIT(kim2009_spow)(L_w, KIM2009_V_N_C);
    ALWAN_CORE_T M_w_nc = ALWAN_CORE_FNVLIT(kim2009_spow)(M_w, KIM2009_V_N_C);
    ALWAN_CORE_T S_w_nc = ALWAN_CORE_FNVLIT(kim2009_spow)(S_w, KIM2009_V_N_C);

    ALWAN_CORE_T Lp_w = L_w_nc / (L_w_nc + La_nc);
    ALWAN_CORE_T Mp_w = M_w_nc / (M_w_nc + La_nc);
    ALWAN_CORE_T Sp_w = S_w_nc / (S_w_nc + La_nc);

    /* Step 7: Achromatic signal for white */
    ALWAN_CORE_T A_w = (KIM2009_V_W_L * Lp_w + KIM2009_V_W_M * Mp_w + KIM2009_V_W_S * Sp_w) / KIM2009_V_W_SUM;

    /* Step 8: Recover perceived lightness J_p from J */
    ALWAN_CORE_T J_p = (correlates.J / ALWAN_CORE_LITERAL(100.0) - ALWAN_CORE_ONE) / media_E + ALWAN_CORE_ONE;

    /* Step 9: Recover achromatic signal A from J_p */
    ALWAN_CORE_T J_p_nj  = ALWAN_CORE_FNVLIT(kim2009_spow)(J_p, KIM2009_V_N_J);
    ALWAN_CORE_T o_j_nj  = ALWAN_CORE_POW(KIM2009_V_O_J, KIM2009_V_N_J);
    ALWAN_CORE_T A_val   = A_w * ((KIM2009_V_A_J * J_p_nj) / (J_p_nj + o_j_nj) + KIM2009_V_B_J);

    /* Step 10: Recover opponent signals a and b from C and h */
    ALWAN_CORE_T hr      = correlates.h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_T C_a_k   = ALWAN_CORE_POW(correlates.C / KIM2009_V_A_K, ALWAN_CORE_ONE / KIM2009_V_N_K);
    ALWAN_CORE_T opp_a   = ALWAN_CORE_COS(hr) * C_a_k;
    ALWAN_CORE_T opp_b   = ALWAN_CORE_SIN(hr) * C_a_k;

    /* Step 11: Recover LMS_p from A, a, b */
    ALWAN_CORE_T Lp = A_val + (ALWAN_CORE_LITERAL(451.0) / ALWAN_CORE_LITERAL(1403.0)) * opp_a + (ALWAN_CORE_LITERAL(288.0) / ALWAN_CORE_LITERAL(1403.0)) * opp_b;
    ALWAN_CORE_T Mp = A_val - (ALWAN_CORE_LITERAL(891.0) / ALWAN_CORE_LITERAL(1403.0)) * opp_a - (ALWAN_CORE_LITERAL(261.0) / ALWAN_CORE_LITERAL(1403.0)) * opp_b;
    ALWAN_CORE_T Sp = A_val - (ALWAN_CORE_LITERAL(220.0) / ALWAN_CORE_LITERAL(1403.0)) * opp_a - (ALWAN_CORE_LITERAL(6300.0) / ALWAN_CORE_LITERAL(1403.0)) * opp_b;

    /* Step 12: Recover absolute cone responses LMS from LMS_p */
    ALWAN_CORE_T inv_nc = ALWAN_CORE_ONE / KIM2009_V_N_C;

    ALWAN_CORE_T den_L   = Lp - ALWAN_CORE_ONE;
    ALWAN_CORE_T ratio_L = -La_nc * Lp / den_L;
    ratio_L = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(den_L) < ALWAN_CORE_EPSILON, ALWAN_CORE_ZERO, ratio_L);
    ratio_L = ALWAN_CORE_SELECT(ratio_L < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ratio_L);
    ALWAN_CORE_T Lv = ALWAN_CORE_POW(ratio_L, inv_nc);
    Lv = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(den_L) < ALWAN_CORE_EPSILON, ALWAN_CORE_ZERO, Lv);

    ALWAN_CORE_T den_M   = Mp - ALWAN_CORE_ONE;
    ALWAN_CORE_T ratio_M = -La_nc * Mp / den_M;
    ratio_M = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(den_M) < ALWAN_CORE_EPSILON, ALWAN_CORE_ZERO, ratio_M);
    ratio_M = ALWAN_CORE_SELECT(ratio_M < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ratio_M);
    ALWAN_CORE_T Mv = ALWAN_CORE_POW(ratio_M, inv_nc);
    Mv = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(den_M) < ALWAN_CORE_EPSILON, ALWAN_CORE_ZERO, Mv);

    ALWAN_CORE_T den_S   = Sp - ALWAN_CORE_ONE;
    ALWAN_CORE_T ratio_S = -La_nc * Sp / den_S;
    ratio_S = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(den_S) < ALWAN_CORE_EPSILON, ALWAN_CORE_ZERO, ratio_S);
    ratio_S = ALWAN_CORE_SELECT(ratio_S < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ratio_S);
    ALWAN_CORE_T Sv = ALWAN_CORE_POW(ratio_S, inv_nc);
    Sv = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(den_S) < ALWAN_CORE_EPSILON, ALWAN_CORE_ZERO, Sv);

    /* Step 13: Inverse HPE transform - LMS to adapted XYZ_c */
    ALWAN_CORE_VEC3 lms_v = {{Lv, Mv, Sv}};
    ALWAN_CORE_VEC3 xyzc_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(KIM2009_V_M_HPE_INV), lms_v);
    ALWAN_CORE_T Xc = xyzc_v.v[0];
    ALWAN_CORE_T Yc = xyzc_v.v[1];
    ALWAN_CORE_T Zc = xyzc_v.v[2];

    /* Step 14: CAT02 transform - XYZ_c to RGB_c */
    ALWAN_CORE_VEC3 xyzc_in = {{Xc, Yc, Zc}};
    ALWAN_CORE_VEC3 rgbc_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(KIM2009_V_M_CAT02), xyzc_in);
    ALWAN_CORE_T R_c = rgbc_v.v[0];
    ALWAN_CORE_T G_c = rgbc_v.v[1];
    ALWAN_CORE_T B_c = rgbc_v.v[2];

    /* Step 15: Inverse chromatic adaptation - RGB_c to RGB */
    ALWAN_CORE_T R_out = R_c / fac_R;
    ALWAN_CORE_T G_out = G_c / fac_G;
    ALWAN_CORE_T B_out = B_c / fac_B;

    /* Step 16: Inverse CAT02 - RGB to XYZ */
    ALWAN_CORE_VEC3 rgb_out_v = {{R_out, G_out, B_out}};
    ALWAN_CORE_VEC3 xyz_out_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(KIM2009_V_M_CAT02_INV), rgb_out_v);
    result.x = xyz_out_v.v[0];
    result.y = xyz_out_v.v[1];
    result.z = xyz_out_v.v[2];

    return result;
}

/* Undefine local constants to avoid redefinition on second pass */
#undef KIM2009_V_N_C
#undef KIM2009_V_N_Q
#undef KIM2009_V_A_K
#undef KIM2009_V_N_K
#undef KIM2009_V_A_M
#undef KIM2009_V_B_M
#undef KIM2009_V_A_J
#undef KIM2009_V_B_J
#undef KIM2009_V_N_J
#undef KIM2009_V_O_J
#undef KIM2009_V_W_L
#undef KIM2009_V_W_M
#undef KIM2009_V_W_S
#undef KIM2009_V_W_SUM
