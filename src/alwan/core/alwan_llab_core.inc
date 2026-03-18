/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only LLAB Color Appearance Model (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Luo, Lo & Kuo (1996)
 * "The LLAB(l:c) colour model"
 */

/* ----------------------------------------------------------------
 * LLAB Output Type (suffixed per precision)
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T L, Ch, h, s;
} ALWAN_CORE_FNLIT(alwan_llab_v_correlates);

/* ----------------------------------------------------------------
 * LLAB Transformation Matrices
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(LLAB_XYZ_TO_RGB) = {{
#include "../data/matrices/llab_xyz_to_rgb.csv"
}};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(LLAB_RGB_TO_XYZ) = {{
#include "../data/matrices/llab_rgb_to_xyz.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * LLAB Constants
 * ---------------------------------------------------------------- */

#define LLAB_V_EPSILON       ALWAN_CORE_LITERAL(0.008856)
#define LLAB_V_COEF_116      ALWAN_CORE_LITERAL(116.0)
#define LLAB_V_COEF_16       ALWAN_CORE_LITERAL(16.0)
#define LLAB_V_COEF_500      ALWAN_CORE_LITERAL(500.0)
#define LLAB_V_COEF_200      ALWAN_CORE_LITERAL(200.0)
#define LLAB_V_CHROMA_SCALE  ALWAN_CORE_LITERAL(25.0)
#define LLAB_V_CHROMA_CONST  ALWAN_CORE_LITERAL(0.05)
#define LLAB_V_BETA_EXP      ALWAN_CORE_LITERAL(0.0834)

/* ----------------------------------------------------------------
 * LLAB f(t) with F_S induction factor
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(llab_f)(ALWAN_CORE_T t, ALWAN_CORE_T F_S) {
    ALWAN_CORE_T power_result = ALWAN_CORE_POW(t, ALWAN_CORE_LITERAL(1.0) / F_S);
    ALWAN_CORE_T slope = (ALWAN_CORE_POW(LLAB_V_EPSILON, ALWAN_CORE_LITERAL(1.0) / F_S) -
                          LLAB_V_COEF_16 / LLAB_V_COEF_116) / LLAB_V_EPSILON;
    ALWAN_CORE_T linear_result = slope * t + LLAB_V_COEF_16 / LLAB_V_COEF_116;
    return ALWAN_CORE_SELECT(t > LLAB_V_EPSILON, power_result, linear_result);
}

/* ----------------------------------------------------------------
 * LLAB Forward Transform (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_llab_v_correlates) ALWAN_CORE_FNVLIT(alwan_llab_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_XYZ xyz_0,
    ALWAN_CORE_XYZ xyz_r,
    ALWAN_CORE_T Y_b,
    ALWAN_CORE_T D,
    ALWAN_CORE_T F_S,
    ALWAN_CORE_T F_L
) {
    ALWAN_CORE_FNLIT(alwan_llab_v_correlates) result;

    /* Store original sample Y for re-scaling after adaptation */
    ALWAN_CORE_T Y_sample = xyz.y;

    /* Step 1: Normalize XYZ by Y, then convert to RGB (cone responses) */
    ALWAN_CORE_T Y_s_inv = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(Y_sample) > ALWAN_CORE_EPSILON,
                                     ALWAN_CORE_LITERAL(1.0) / Y_sample, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_VEC3 xyz_norm = {{xyz.x * Y_s_inv, ALWAN_CORE_LITERAL(1.0), xyz.z * Y_s_inv}};
    ALWAN_CORE_VEC3 rgb_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(LLAB_XYZ_TO_RGB), xyz_norm);
    ALWAN_CORE_T R = rgb_v.v[0]; ALWAN_CORE_T G = rgb_v.v[1]; ALWAN_CORE_T B = rgb_v.v[2];

    /* Normalize test illuminant by its Y, then convert to RGB */
    ALWAN_CORE_T Y0_inv = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(xyz_0.y) > ALWAN_CORE_EPSILON,
                                    ALWAN_CORE_LITERAL(1.0) / xyz_0.y, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_VEC3 xyz_0_norm = {{xyz_0.x * Y0_inv, ALWAN_CORE_LITERAL(1.0), xyz_0.z * Y0_inv}};
    ALWAN_CORE_VEC3 rgb_0_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(LLAB_XYZ_TO_RGB), xyz_0_norm);
    ALWAN_CORE_T R_0 = rgb_0_v.v[0]; ALWAN_CORE_T G_0 = rgb_0_v.v[1]; ALWAN_CORE_T B_0 = rgb_0_v.v[2];

    /* Normalize reference illuminant by its Y, then convert to RGB */
    ALWAN_CORE_T Yr_inv = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(xyz_r.y) > ALWAN_CORE_EPSILON,
                                    ALWAN_CORE_LITERAL(1.0) / xyz_r.y, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_VEC3 xyz_r_norm = {{xyz_r.x * Yr_inv, ALWAN_CORE_LITERAL(1.0), xyz_r.z * Yr_inv}};
    ALWAN_CORE_VEC3 rgb_r_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(LLAB_XYZ_TO_RGB), xyz_r_norm);
    ALWAN_CORE_T R_r = rgb_r_v.v[0]; ALWAN_CORE_T G_r = rgb_r_v.v[1]; ALWAN_CORE_T B_r = rgb_r_v.v[2];

    /* Step 2: BFD chromatic adaptation */
    ALWAN_CORE_T R_adapted = (D * (R_r / R_0) + (ALWAN_CORE_LITERAL(1.0) - D)) * R;
    ALWAN_CORE_T G_adapted = (D * (G_r / G_0) + (ALWAN_CORE_LITERAL(1.0) - D)) * G;

    ALWAN_CORE_T beta = ALWAN_CORE_POW(B_0 / B_r, LLAB_V_BETA_EXP);
    ALWAN_CORE_T B_adapted = (D * B_r / ALWAN_CORE_POW(B_0, beta) + (ALWAN_CORE_LITERAL(1.0) - D))
                             * ALWAN_CORE_POW(B, beta);

    /* Step 3: Re-scale adapted RGB by sample Y, then convert back to XYZ */
    ALWAN_CORE_VEC3 adapted_v = {{R_adapted * Y_sample, G_adapted * Y_sample, B_adapted * Y_sample}};
    ALWAN_CORE_VEC3 xyz_adapted_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(LLAB_RGB_TO_XYZ), adapted_v);
    ALWAN_CORE_T X_adapted = xyz_adapted_v.v[0];
    ALWAN_CORE_T Y_adapted = xyz_adapted_v.v[1];
    ALWAN_CORE_T Z_adapted = xyz_adapted_v.v[2];

    /* Step 4: Compute Lab coordinates with F_S induction factor */
    ALWAN_CORE_T f_X = ALWAN_CORE_FNVLIT(llab_f)(X_adapted / ALWAN_CORE_LITERAL(95.05), F_S);
    ALWAN_CORE_T f_Y = ALWAN_CORE_FNVLIT(llab_f)(Y_adapted / ALWAN_CORE_LITERAL(100.0), F_S);
    ALWAN_CORE_T f_Z = ALWAN_CORE_FNVLIT(llab_f)(Z_adapted / ALWAN_CORE_LITERAL(108.88), F_S);

    /* Compute z factor for lightness modulation */
    ALWAN_CORE_T z = ALWAN_CORE_LITERAL(1.0) + F_L * ALWAN_CORE_SQRT(Y_b / ALWAN_CORE_LITERAL(100.0));

    /* LLAB lightness with z modulation */
    ALWAN_CORE_T L_L = LLAB_V_COEF_116 * ALWAN_CORE_POW(f_Y, z) - LLAB_V_COEF_16;

    /* LLAB opponent dimensions */
    ALWAN_CORE_T a_L = LLAB_V_COEF_500 * (f_X - f_Y);
    ALWAN_CORE_T b_L = LLAB_V_COEF_200 * (f_Y - f_Z);

    /* Step 5: Compute appearance correlates */
    ALWAN_CORE_T c = ALWAN_CORE_SQRT(a_L * a_L + b_L * b_L);
    ALWAN_CORE_T Ch_L = LLAB_V_CHROMA_SCALE *
                        ALWAN_CORE_LN(ALWAN_CORE_LITERAL(1.0) + LLAB_V_CHROMA_CONST * c);

    ALWAN_CORE_T h_rad = ALWAN_CORE_ATAN2(b_L, a_L);
    ALWAN_CORE_T h_deg = h_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    ALWAN_CORE_T h_L = ALWAN_CORE_SELECT(h_deg < ALWAN_CORE_LITERAL(0.0), h_deg + ALWAN_CORE_LITERAL(360.0), h_deg);

    ALWAN_CORE_T s_L = ALWAN_CORE_SELECT(L_L > ALWAN_CORE_LITERAL(0.0), Ch_L / L_L, ALWAN_CORE_LITERAL(0.0));

    result.L = L_L;
    result.Ch = Ch_L;
    result.h = h_L;
    result.s = s_L;

    return result;
}

/* Undefine local constants to avoid redefinition on second pass */
#undef LLAB_V_EPSILON
#undef LLAB_V_COEF_116
#undef LLAB_V_COEF_16
#undef LLAB_V_COEF_500
#undef LLAB_V_COEF_200
#undef LLAB_V_CHROMA_SCALE
#undef LLAB_V_CHROMA_CONST
#undef LLAB_V_BETA_EXP
