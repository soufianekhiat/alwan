/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only CIECAM02 & CAM16 Color Appearance Models (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: CIE 159:2004 (CIECAM02), Li et al. (2017) (CAM16)
 */

/* ----------------------------------------------------------------
 * CIECAM02 / CAM16 Correlates (value-returning variants)
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T J, C, h, s, Q, M, H;
} ALWAN_CORE_FNLIT(alwan_ciecam02_v_correlates);

typedef struct {
    ALWAN_CORE_T J, C, h, s, Q, M, H;
} ALWAN_CORE_FNLIT(alwan_cam16_v_correlates);

/* ----------------------------------------------------------------
 * Transformation Matrices
 * ---------------------------------------------------------------- */

/* Hunt-Pointer-Estevez (HPE) matrix for CIECAM02 opponent signals */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CAM_M_HPE) = {{
#include "../data/matrices/hpe.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CAM_M_HPE_INV) = {{
#include "../data/matrices/hpe_inv.csv"
}};
/* CAT02 matrix for CIECAM02 chromatic adaptation */
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CAM_M_CAT02) = {{
#include "../data/matrices/cat_cat02.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CAM_M_CAT02_INV) = {{
#include "../data/matrices/cat_cat02_inv.csv"
}};
/* Precomputed M_HPE * M_CAT02^-1 for CIECAM02 (matches colour-science) */
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CAM_M_HPE_CAT02_INV) = {{
#include "../data/matrices/hpe_cat02_inv.csv"
}};
/* Inverse: M_CAT02 * M_HPE^-1 */
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CAM_M_CAT02_HPE_INV) = {{
#include "../data/matrices/hpe_cat02_inv_inv.csv"
}};
/* CAT16 matrix for CAM16 XYZ <-> LMS */
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CAM_M_CAT16) = {{
#include "../data/matrices/cat_cat16.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CAM_M_CAT16_INV) = {{
#include "../data/matrices/cat_cat16_inv.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Hue Quadrature Data (shared by CIECAM02 and CAM16)
 * ---------------------------------------------------------------- */

static const ALWAN_CORE_T ALWAN_CORE_FNLIT(CAM_HQ_h_i)[5] = {
    ALWAN_CORE_LITERAL(20.14), ALWAN_CORE_LITERAL(90.00),
    ALWAN_CORE_LITERAL(164.25), ALWAN_CORE_LITERAL(237.53),
    ALWAN_CORE_LITERAL(380.14)
};

static const ALWAN_CORE_T ALWAN_CORE_FNLIT(CAM_HQ_e_i)[5] = {
    ALWAN_CORE_LITERAL(0.8), ALWAN_CORE_LITERAL(0.7),
    ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.2),
    ALWAN_CORE_LITERAL(0.8)  /* wraparound: e_i[4] == e_i[0] */
};

static const ALWAN_CORE_T ALWAN_CORE_FNLIT(CAM_HQ_H_i)[5] = {
    ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(100.0),
    ALWAN_CORE_LITERAL(200.0), ALWAN_CORE_LITERAL(300.0),
    ALWAN_CORE_LITERAL(400.0)
};

/* ----------------------------------------------------------------
 * Compute degree of adaptation D
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(cam_compute_D)(
    ALWAN_CORE_T F,
    ALWAN_CORE_T La,
    ALWAN_CORE_T discount_illuminant
) {
    ALWAN_CORE_T D_computed = F * (ALWAN_CORE_ONE - ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.6) *
                     ALWAN_CORE_EXP((-La - ALWAN_CORE_LITERAL(42.0)) / ALWAN_CORE_LITERAL(92.0)));
    ALWAN_CORE_T D_clamped = ALWAN_CORE_CLAMP(D_computed, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    return ALWAN_CORE_SELECT(discount_illuminant > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_ONE, D_clamped);
}

/* ----------------------------------------------------------------
 * Post-adaptation nonlinear response compression
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear)(ALWAN_CORE_T x) {
    ALWAN_CORE_T x_abs = ALWAN_CORE_ABS(x);
    ALWAN_CORE_T x_pow = ALWAN_CORE_POW(x_abs, ALWAN_CORE_LITERAL(0.42));
    ALWAN_CORE_T magnitude = ALWAN_CORE_LITERAL(400.0) * x_pow / (ALWAN_CORE_LITERAL(27.13) + x_pow) + ALWAN_CORE_LITERAL(0.1);
    return ALWAN_CORE_SELECT(x < ALWAN_CORE_ZERO, -magnitude, magnitude);
}

/* ----------------------------------------------------------------
 * Inverse post-adaptation nonlinear response
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear_inv)(ALWAN_CORE_T x) {
    ALWAN_CORE_T y = x - ALWAN_CORE_LITERAL(0.1);
    ALWAN_CORE_T y_abs = ALWAN_CORE_ABS(y);
    ALWAN_CORE_T magnitude = ALWAN_CORE_POW(
        (ALWAN_CORE_LITERAL(27.13) * y_abs) / (ALWAN_CORE_LITERAL(400.0) - y_abs),
        ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(0.42));
    return ALWAN_CORE_SELECT(y < ALWAN_CORE_ZERO, -magnitude, magnitude);
}

/* ----------------------------------------------------------------
 * Derived viewing condition parameters (value-returning)
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T D, FL, n, Nbb, Ncb, z, A_w;
} ALWAN_CORE_FNLIT(alwan_cam_derived_params);

/* Compute all derived viewing condition parameters from scalars.
 * M_cat: chromatic adaptation matrix (CAT02 for CIECAM02, CAT16 for CAM16)
 * M_cat_inv: inverse of M_cat (only used for CIECAM02 two-step process)
 * M_hpe: HPE matrix (only used for CIECAM02; pass NULL-equivalent for CAM16)
 * use_hpe: if nonzero, apply M_hpe * M_cat_inv after adaptation (CIECAM02) */
ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_cam_derived_params) ALWAN_CORE_FNVLIT(cam_compute_derived_params)(
    ALWAN_CORE_T F,
    ALWAN_CORE_T La,
    ALWAN_CORE_T Yb,
    ALWAN_CORE_T Yw,
    ALWAN_CORE_T discount_illuminant,
    ALWAN_CORE_XYZ white_xyz,
    ALWAN_CORE_MAT3X3 M_cat,
    ALWAN_CORE_MAT3X3 M_cat_inv,
    ALWAN_CORE_MAT3X3 M_hpe,
    int use_hpe
) {
    ALWAN_CORE_FNLIT(alwan_cam_derived_params) p;

    /* Degree of adaptation D */
    p.D = ALWAN_CORE_FNVLIT(cam_compute_D)(F, La, discount_illuminant);

    /* Luminance level adaptation factor FL */
    ALWAN_CORE_T k = ALWAN_CORE_ONE / (ALWAN_CORE_LITERAL(5.0) * La + ALWAN_CORE_ONE);
    ALWAN_CORE_T k4 = k * k * k * k;
    p.FL = ALWAN_CORE_LITERAL(0.2) * k4 * (ALWAN_CORE_LITERAL(5.0) * La) +
           ALWAN_CORE_LITERAL(0.1) * (ALWAN_CORE_ONE - k4) * (ALWAN_CORE_ONE - k4) *
           ALWAN_CORE_POW(ALWAN_CORE_LITERAL(5.0) * La, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0));

    /* Background parameters */
    p.n   = Yb / Yw;
    p.Nbb = ALWAN_CORE_LITERAL(0.725) * ALWAN_CORE_POW(ALWAN_CORE_ONE / p.n, ALWAN_CORE_LITERAL(0.2));
    p.Ncb = p.Nbb;
    p.z   = ALWAN_CORE_LITERAL(1.48) + ALWAN_CORE_SQRT(p.n);

    /* Achromatic response of the white point A_w */
    ALWAN_CORE_VEC3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    ALWAN_CORE_VEC3 lms_w = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M_cat, white_v);

    ALWAN_CORE_T D_R_w = p.D * (Yw / lms_w.v[0]) + ALWAN_CORE_ONE - p.D;
    ALWAN_CORE_T D_G_w = p.D * (Yw / lms_w.v[1]) + ALWAN_CORE_ONE - p.D;
    ALWAN_CORE_T D_B_w = p.D * (Yw / lms_w.v[2]) + ALWAN_CORE_ONE - p.D;

    ALWAN_CORE_VEC3 rgb_cw = {{lms_w.v[0] * D_R_w, lms_w.v[1] * D_G_w, lms_w.v[2] * D_B_w}};

    /* For CIECAM02: convert adapted white from CAT02 space to HPE space */
    ALWAN_CORE_VEC3 hpe_w = rgb_cw;
    if (use_hpe) {
        ALWAN_CORE_VEC3 xyz_cw = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M_cat_inv, rgb_cw);
        hpe_w = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M_hpe, xyz_cw);
    }

    ALWAN_CORE_T R_aw = ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear)(p.FL * hpe_w.v[0] / ALWAN_CORE_LITERAL(100.0));
    ALWAN_CORE_T G_aw = ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear)(p.FL * hpe_w.v[1] / ALWAN_CORE_LITERAL(100.0));
    ALWAN_CORE_T B_aw = ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear)(p.FL * hpe_w.v[2] / ALWAN_CORE_LITERAL(100.0));

    p.A_w = (ALWAN_CORE_LITERAL(2.0) * R_aw + G_aw +
             ALWAN_CORE_LITERAL(0.05) * B_aw - ALWAN_CORE_LITERAL(0.305)) * p.Nbb;

    return p;
}

/* ----------------------------------------------------------------
 * Hue angle to hue quadrature H (branchless, unrolled)
 *
 * h must be in [0, 360). Uses nested ALWAN_CORE_SELECT for sector
 * selection across 5 sectors.
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(cam_hue_to_quadrature)(ALWAN_CORE_T h) {
    /* Normalize h to [0, 360) */
    ALWAN_CORE_T h_norm = ALWAN_CORE_FMOD(h, ALWAN_CORE_LITERAL(360.0));
    h_norm = ALWAN_CORE_SELECT(h_norm < ALWAN_CORE_ZERO, h_norm + ALWAN_CORE_LITERAL(360.0), h_norm);

    /* Wrap: if h' < 20.14, shift it up by 360 to match the table range */
    ALWAN_CORE_T hp = ALWAN_CORE_SELECT(h_norm < ALWAN_CORE_LITERAL(20.14),
                                   h_norm + ALWAN_CORE_LITERAL(360.0), h_norm);

    /* Compute H for each of the 4 possible sectors */
    /* Sector 0: h_i[0]=20.14 .. h_i[1]=90.00, e_i[0]=0.8, e_i[1]=0.7 */
    ALWAN_CORE_T h_diff_0 = hp - ALWAN_CORE_FNLIT(CAM_HQ_h_i)[0];
    ALWAN_CORE_T hi_diff_0 = ALWAN_CORE_FNLIT(CAM_HQ_h_i)[1] - ALWAN_CORE_FNLIT(CAM_HQ_h_i)[0];
    ALWAN_CORE_T H_0 = ALWAN_CORE_FNLIT(CAM_HQ_H_i)[0] + (ALWAN_CORE_LITERAL(100.0) * h_diff_0 * ALWAN_CORE_FNLIT(CAM_HQ_e_i)[0]) /
                        (h_diff_0 * ALWAN_CORE_FNLIT(CAM_HQ_e_i)[0] + (hi_diff_0 - h_diff_0) * ALWAN_CORE_FNLIT(CAM_HQ_e_i)[1]);

    /* Sector 1: h_i[1]=90.00 .. h_i[2]=164.25, e_i[1]=0.7, e_i[2]=1.0 */
    ALWAN_CORE_T h_diff_1 = hp - ALWAN_CORE_FNLIT(CAM_HQ_h_i)[1];
    ALWAN_CORE_T hi_diff_1 = ALWAN_CORE_FNLIT(CAM_HQ_h_i)[2] - ALWAN_CORE_FNLIT(CAM_HQ_h_i)[1];
    ALWAN_CORE_T H_1 = ALWAN_CORE_FNLIT(CAM_HQ_H_i)[1] + (ALWAN_CORE_LITERAL(100.0) * h_diff_1 * ALWAN_CORE_FNLIT(CAM_HQ_e_i)[1]) /
                        (h_diff_1 * ALWAN_CORE_FNLIT(CAM_HQ_e_i)[1] + (hi_diff_1 - h_diff_1) * ALWAN_CORE_FNLIT(CAM_HQ_e_i)[2]);

    /* Sector 2: h_i[2]=164.25 .. h_i[3]=237.53, e_i[2]=1.0, e_i[3]=1.2 */
    ALWAN_CORE_T h_diff_2 = hp - ALWAN_CORE_FNLIT(CAM_HQ_h_i)[2];
    ALWAN_CORE_T hi_diff_2 = ALWAN_CORE_FNLIT(CAM_HQ_h_i)[3] - ALWAN_CORE_FNLIT(CAM_HQ_h_i)[2];
    ALWAN_CORE_T H_2 = ALWAN_CORE_FNLIT(CAM_HQ_H_i)[2] + (ALWAN_CORE_LITERAL(100.0) * h_diff_2 * ALWAN_CORE_FNLIT(CAM_HQ_e_i)[2]) /
                        (h_diff_2 * ALWAN_CORE_FNLIT(CAM_HQ_e_i)[2] + (hi_diff_2 - h_diff_2) * ALWAN_CORE_FNLIT(CAM_HQ_e_i)[3]);

    /* Sector 3: h_i[3]=237.53 .. h_i[4]=380.14, e_i[3]=1.2, e_i[4]=0.8 */
    ALWAN_CORE_T h_diff_3 = hp - ALWAN_CORE_FNLIT(CAM_HQ_h_i)[3];
    ALWAN_CORE_T hi_diff_3 = ALWAN_CORE_FNLIT(CAM_HQ_h_i)[4] - ALWAN_CORE_FNLIT(CAM_HQ_h_i)[3];
    ALWAN_CORE_T H_3 = ALWAN_CORE_FNLIT(CAM_HQ_H_i)[3] + (ALWAN_CORE_LITERAL(100.0) * h_diff_3 * ALWAN_CORE_FNLIT(CAM_HQ_e_i)[3]) /
                        (h_diff_3 * ALWAN_CORE_FNLIT(CAM_HQ_e_i)[3] + (hi_diff_3 - h_diff_3) * ALWAN_CORE_FNLIT(CAM_HQ_e_i)[4]);

    /* Nested ALWAN_CORE_SELECT to choose the correct sector */
    ALWAN_CORE_T H_val = ALWAN_CORE_SELECT(hp < ALWAN_CORE_FNLIT(CAM_HQ_h_i)[1], H_0,
                         ALWAN_CORE_SELECT(hp < ALWAN_CORE_FNLIT(CAM_HQ_h_i)[2], H_1,
                         ALWAN_CORE_SELECT(hp < ALWAN_CORE_FNLIT(CAM_HQ_h_i)[3], H_2,
                                                           H_3)));

    return H_val;
}

/* ----------------------------------------------------------------
 * Inverse opponent colour dimensions
 *
 * Given t, h (radians), Nc, Ncb, et, A, Nbb, P_3, compute a and b.
 * Uses ALWAN_CORE_SELECT instead of if/else for branchless path.
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(cam_opponent_colour_inverse)(
    ALWAN_CORE_T t,
    ALWAN_CORE_T h_rad,
    ALWAN_CORE_T Nc,
    ALWAN_CORE_T Ncb,
    ALWAN_CORE_T et,
    ALWAN_CORE_T A,
    ALWAN_CORE_T Nbb
) {
    ALWAN_CORE_VEC2 result;

    ALWAN_CORE_T sin_h = ALWAN_CORE_SIN(h_rad);
    ALWAN_CORE_T cos_h = ALWAN_CORE_COS(h_rad);

    ALWAN_CORE_T P_1 = (ALWAN_CORE_LITERAL(50000.0) / ALWAN_CORE_LITERAL(13.0)) * Nc * Ncb * et / t;
    ALWAN_CORE_T P_2 = A / Nbb + ALWAN_CORE_LITERAL(0.305);
    ALWAN_CORE_T P_3 = ALWAN_CORE_LITERAL(21.0) / ALWAN_CORE_LITERAL(20.0);

    ALWAN_CORE_T numerator = P_2 * (ALWAN_CORE_LITERAL(2.0) + P_3) * (ALWAN_CORE_LITERAL(460.0) / ALWAN_CORE_LITERAL(1403.0));

    /* Case 1: |sin(h)| >= |cos(h)| -- solve for b first */
    ALWAN_CORE_T P_4 = P_1 / sin_h;
    ALWAN_CORE_T denom_b = P_4 +
                          (ALWAN_CORE_LITERAL(2.0) + P_3) * (ALWAN_CORE_LITERAL(220.0) / ALWAN_CORE_LITERAL(1403.0)) * (cos_h / sin_h) -
                          (ALWAN_CORE_LITERAL(27.0) / ALWAN_CORE_LITERAL(1403.0)) +
                          P_3 * (ALWAN_CORE_LITERAL(6300.0) / ALWAN_CORE_LITERAL(1403.0));
    ALWAN_CORE_T b_case1 = numerator / denom_b;
    ALWAN_CORE_T a_case1 = b_case1 * (cos_h / sin_h);

    /* Case 2: |sin(h)| < |cos(h)| -- solve for a first */
    ALWAN_CORE_T P_5 = P_1 / cos_h;
    ALWAN_CORE_T denom_a = P_5 +
                          (ALWAN_CORE_LITERAL(2.0) + P_3) * (ALWAN_CORE_LITERAL(220.0) / ALWAN_CORE_LITERAL(1403.0)) -
                          ((ALWAN_CORE_LITERAL(27.0) / ALWAN_CORE_LITERAL(1403.0)) - P_3 * (ALWAN_CORE_LITERAL(6300.0) / ALWAN_CORE_LITERAL(1403.0))) * (sin_h / cos_h);
    ALWAN_CORE_T a_case2 = numerator / denom_a;
    ALWAN_CORE_T b_case2 = a_case2 * (sin_h / cos_h);

    int use_sin_path = ALWAN_CORE_ABS(sin_h) >= ALWAN_CORE_ABS(cos_h);
    result.v[0] = ALWAN_CORE_SELECT(use_sin_path, a_case1, a_case2);
    result.v[1] = ALWAN_CORE_SELECT(use_sin_path, b_case1, b_case2);

    return result;
}

/* ================================================================
 * CIECAM02 Forward Transform (value-returning)
 *
 * All viewing condition parameters are pre-computed scalars:
 *   F, c, Nc  - surround parameters
 *   D         - degree of adaptation
 *   FL        - luminance level adaptation factor
 *   n         - Y_b / Y_w
 *   Nbb, Ncb  - background/chromatic induction factors
 *   z         - base exponential nonlinearity
 *   A_w       - achromatic response of the white point
 *   La        - adapting luminance (cd/m^2)
 *   Y_b       - background luminance factor
 *   Y_w       - reference white Y
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_ciecam02_v_correlates) ALWAN_CORE_FNVLIT(alwan_ciecam02_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_XYZ white_xyz,
    ALWAN_CORE_T F,
    ALWAN_CORE_T c,
    ALWAN_CORE_T Nc,
    ALWAN_CORE_T D,
    ALWAN_CORE_T FL,
    ALWAN_CORE_T n,
    ALWAN_CORE_T Nbb,
    ALWAN_CORE_T Ncb,
    ALWAN_CORE_T z,
    ALWAN_CORE_T A_w
) {
    ALWAN_CORE_FNLIT(alwan_ciecam02_v_correlates) result;
    ALWAN_UNUSED(F); /* F already folded into pre-computed D */

    /* Step 1: XYZ -> cone responses via CAT02 matrix */
    ALWAN_CORE_VEC3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    ALWAN_CORE_VEC3 rgb_cat = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CAM_M_CAT02), xyz_v);

    /* Step 2: White point cone responses via CAT02 */
    ALWAN_CORE_VEC3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    ALWAN_CORE_VEC3 rgb_cat_w = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CAM_M_CAT02), white_v);

    /* Step 3: Chromatic adaptation in CAT02 space */
    ALWAN_CORE_T D_R = D * (white_xyz.y / rgb_cat_w.v[0]) + ALWAN_CORE_ONE - D;
    ALWAN_CORE_T D_G = D * (white_xyz.y / rgb_cat_w.v[1]) + ALWAN_CORE_ONE - D;
    ALWAN_CORE_T D_B = D * (white_xyz.y / rgb_cat_w.v[2]) + ALWAN_CORE_ONE - D;

    ALWAN_CORE_VEC3 rgb_c = {{rgb_cat.v[0] * D_R, rgb_cat.v[1] * D_G, rgb_cat.v[2] * D_B}};

    /* Step 4: Convert adapted signals to HPE space using precomputed M_HPE * M_CAT02^-1 */
    ALWAN_CORE_VEC3 hpe = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CAM_M_HPE_CAT02_INV), rgb_c);

    /* Step 5: Post-adaptation nonlinear response compression */
    ALWAN_CORE_T R_a = ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear)(FL * hpe.v[0] / ALWAN_CORE_LITERAL(100.0));
    ALWAN_CORE_T G_a = ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear)(FL * hpe.v[1] / ALWAN_CORE_LITERAL(100.0));
    ALWAN_CORE_T B_a = ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear)(FL * hpe.v[2] / ALWAN_CORE_LITERAL(100.0));

    /* Step 5: Achromatic response A */
    ALWAN_CORE_T A = (ALWAN_CORE_LITERAL(2.0) * R_a + G_a +
                ALWAN_CORE_LITERAL(0.05) * B_a - ALWAN_CORE_LITERAL(0.305)) * Nbb;

    /* Step 6: Opponent colour dimensions */
    ALWAN_CORE_T a = R_a - ALWAN_CORE_LITERAL(12.0) * G_a / ALWAN_CORE_LITERAL(11.0) +
               B_a / ALWAN_CORE_LITERAL(11.0);
    ALWAN_CORE_T b = (R_a + G_a - ALWAN_CORE_LITERAL(2.0) * B_a) / ALWAN_CORE_LITERAL(9.0);

    /* Step 7: Hue angle h (degrees) */
    ALWAN_CORE_T h_rad = ALWAN_CORE_ATAN2(b, a);
    ALWAN_CORE_T h = h_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    h = ALWAN_CORE_SELECT(h < ALWAN_CORE_ZERO, h + ALWAN_CORE_LITERAL(360.0), h);

    /* Step 8: Eccentricity et (match colour-science: radians(h) + 2) */
    ALWAN_CORE_T et = ALWAN_CORE_LITERAL(0.25) * (ALWAN_CORE_COS(h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0) + ALWAN_CORE_LITERAL(2.0)) + ALWAN_CORE_LITERAL(3.8));

    /* Step 9: Hue quadrature H */
    ALWAN_CORE_T H = ALWAN_CORE_FNVLIT(cam_hue_to_quadrature)(h);

    /* Step 10: Lightness J */
    ALWAN_CORE_T J = ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_POW(A / A_w, c * z);

    /* Step 11: Brightness Q */
    ALWAN_CORE_T Q = (ALWAN_CORE_LITERAL(4.0) / c) * ALWAN_CORE_SQRT(J / ALWAN_CORE_LITERAL(100.0)) *
               (A_w + ALWAN_CORE_LITERAL(4.0)) * ALWAN_CORE_POW(FL, ALWAN_CORE_LITERAL(0.25));

    /* Step 12: Chroma C */
    ALWAN_CORE_T t_val = (ALWAN_CORE_LITERAL(50000.0) / ALWAN_CORE_LITERAL(13.0) * Nc * Ncb * et *
                ALWAN_CORE_SQRT(a * a + b * b)) /
               (R_a + G_a + ALWAN_CORE_LITERAL(21.0) / ALWAN_CORE_LITERAL(20.0) * B_a);
    ALWAN_CORE_T C_val = ALWAN_CORE_POW(t_val, ALWAN_CORE_LITERAL(0.9)) *
               ALWAN_CORE_SQRT(J / ALWAN_CORE_LITERAL(100.0)) *
               ALWAN_CORE_POW(ALWAN_CORE_LITERAL(1.64) - ALWAN_CORE_POW(ALWAN_CORE_LITERAL(0.29), n), ALWAN_CORE_LITERAL(0.73));

    /* Step 13: Colorfulness M and saturation s */
    ALWAN_CORE_T M = C_val * ALWAN_CORE_POW(FL, ALWAN_CORE_LITERAL(0.25));
    ALWAN_CORE_T s = ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_SQRT(M / Q);

    result.J = J;
    result.C = C_val;
    result.h = h;
    result.s = s;
    result.Q = Q;
    result.M = M;
    result.H = H;

    return result;
}

/* ================================================================
 * CIECAM02 Inverse Transform (value-returning)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_ciecam02_inverse)(
    ALWAN_CORE_T J,
    ALWAN_CORE_T C,
    ALWAN_CORE_T h,
    ALWAN_CORE_XYZ white_xyz,
    ALWAN_CORE_T F,
    ALWAN_CORE_T c,
    ALWAN_CORE_T Nc,
    ALWAN_CORE_T D,
    ALWAN_CORE_T FL,
    ALWAN_CORE_T n,
    ALWAN_CORE_T Nbb,
    ALWAN_CORE_T Ncb,
    ALWAN_CORE_T z,
    ALWAN_CORE_T A_w
) {
    ALWAN_CORE_XYZ result;
    ALWAN_UNUSED(F); /* F already folded into pre-computed D */

    /* Step 1: Compute achromatic response A from J */
    ALWAN_CORE_T A = A_w * ALWAN_CORE_POW(J / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_ONE / (c * z));

    /* Step 2: Compute t from C */
    ALWAN_CORE_T t_val = ALWAN_CORE_POW(
        C / (ALWAN_CORE_SQRT(J / ALWAN_CORE_LITERAL(100.0)) *
             ALWAN_CORE_POW(ALWAN_CORE_LITERAL(1.64) - ALWAN_CORE_POW(ALWAN_CORE_LITERAL(0.29), n), ALWAN_CORE_LITERAL(0.73))),
        ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(0.9));

    /* Step 3: Eccentricity et */
    ALWAN_CORE_T et = ALWAN_CORE_LITERAL(0.25) * (ALWAN_CORE_COS(h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0) + ALWAN_CORE_LITERAL(2.0)) + ALWAN_CORE_LITERAL(3.8));

    /* Step 4: Compute opponent dimensions a, b via helper */
    ALWAN_CORE_T h_rad = h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_VEC2 ab = ALWAN_CORE_FNVLIT(cam_opponent_colour_inverse)(t_val, h_rad, Nc, Ncb, et, A, Nbb);
    ALWAN_CORE_T a = ab.v[0];
    ALWAN_CORE_T b = ab.v[1];

    /* Step 5: Compute adapted signals RGB_a from a, b, A */
    ALWAN_CORE_T P_2 = A / Nbb + ALWAN_CORE_LITERAL(0.305);
    ALWAN_CORE_T R_a = (ALWAN_CORE_LITERAL(460.0) * P_2 + ALWAN_CORE_LITERAL(451.0) * a + ALWAN_CORE_LITERAL(288.0) * b) / ALWAN_CORE_LITERAL(1403.0);
    ALWAN_CORE_T G_a = (ALWAN_CORE_LITERAL(460.0) * P_2 - ALWAN_CORE_LITERAL(891.0) * a - ALWAN_CORE_LITERAL(261.0) * b) / ALWAN_CORE_LITERAL(1403.0);
    ALWAN_CORE_T B_a = (ALWAN_CORE_LITERAL(460.0) * P_2 - ALWAN_CORE_LITERAL(220.0) * a - ALWAN_CORE_LITERAL(6300.0) * b) / ALWAN_CORE_LITERAL(1403.0);

    /* Step 6: Inverse post-adaptation nonlinearity (HPE signals) */
    ALWAN_CORE_T R_hpe = ALWAN_CORE_LITERAL(100.0) / FL * ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear_inv)(R_a);
    ALWAN_CORE_T G_hpe = ALWAN_CORE_LITERAL(100.0) / FL * ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear_inv)(G_a);
    ALWAN_CORE_T B_hpe = ALWAN_CORE_LITERAL(100.0) / FL * ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear_inv)(B_a);

    /* Step 7: HPE -> CAT02 space using precomputed M_CAT02 * M_HPE^-1 */
    ALWAN_CORE_VEC3 hpe_v = {{R_hpe, G_hpe, B_hpe}};
    ALWAN_CORE_VEC3 rgb_c = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CAM_M_CAT02_HPE_INV), hpe_v);

    /* Step 8: Inverse chromatic adaptation in CAT02 space */
    ALWAN_CORE_VEC3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    ALWAN_CORE_VEC3 rgb_cat_w = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CAM_M_CAT02), white_v);

    ALWAN_CORE_T D_R = D * (white_xyz.y / rgb_cat_w.v[0]) + ALWAN_CORE_ONE - D;
    ALWAN_CORE_T D_G = D * (white_xyz.y / rgb_cat_w.v[1]) + ALWAN_CORE_ONE - D;
    ALWAN_CORE_T D_B = D * (white_xyz.y / rgb_cat_w.v[2]) + ALWAN_CORE_ONE - D;

    ALWAN_CORE_VEC3 rgb_cat = {{rgb_c.v[0] / D_R, rgb_c.v[1] / D_G, rgb_c.v[2] / D_B}};

    /* Step 9: CAT02 -> XYZ via inverse CAT02 matrix */
    ALWAN_CORE_VEC3 xyz_out = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CAM_M_CAT02_INV), rgb_cat);
    result.x = xyz_out.v[0];
    result.y = xyz_out.v[1];
    result.z = xyz_out.v[2];

    return result;
}

/* ================================================================
 * CAM16 Forward Transform (value-returning)
 *
 * Same parameter convention as CIECAM02 but uses CAT16 matrix.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_cam16_v_correlates) ALWAN_CORE_FNVLIT(alwan_cam16_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_XYZ white_xyz,
    ALWAN_CORE_T F,
    ALWAN_CORE_T c,
    ALWAN_CORE_T Nc,
    ALWAN_CORE_T D,
    ALWAN_CORE_T FL,
    ALWAN_CORE_T n,
    ALWAN_CORE_T Nbb,
    ALWAN_CORE_T Ncb,
    ALWAN_CORE_T z,
    ALWAN_CORE_T A_w
) {
    ALWAN_CORE_FNLIT(alwan_cam16_v_correlates) result;
    ALWAN_UNUSED(F); /* F already folded into pre-computed D */

    /* Step 1: XYZ -> LMS via CAT16 matrix (stimulus) */
    ALWAN_CORE_VEC3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CAM_M_CAT16), xyz_v);
    ALWAN_CORE_T R = lms.v[0];
    ALWAN_CORE_T G = lms.v[1];
    ALWAN_CORE_T B = lms.v[2];

    /* Step 2: XYZ -> LMS via CAT16 matrix (white point) */
    ALWAN_CORE_VEC3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    ALWAN_CORE_VEC3 lms_w = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CAM_M_CAT16), white_v);
    ALWAN_CORE_T R_w = lms_w.v[0];
    ALWAN_CORE_T G_w = lms_w.v[1];
    ALWAN_CORE_T B_w = lms_w.v[2];

    /* Step 3: Chromatic adaptation */
    ALWAN_CORE_T D_R = D * (white_xyz.y / R_w) + ALWAN_CORE_ONE - D;
    ALWAN_CORE_T D_G = D * (white_xyz.y / G_w) + ALWAN_CORE_ONE - D;
    ALWAN_CORE_T D_B = D * (white_xyz.y / B_w) + ALWAN_CORE_ONE - D;

    ALWAN_CORE_T R_c = R * D_R;
    ALWAN_CORE_T G_c = G * D_G;
    ALWAN_CORE_T B_c = B * D_B;

    /* Step 4: Post-adaptation nonlinear response compression */
    ALWAN_CORE_T R_a = ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear)(FL * R_c / ALWAN_CORE_LITERAL(100.0));
    ALWAN_CORE_T G_a = ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear)(FL * G_c / ALWAN_CORE_LITERAL(100.0));
    ALWAN_CORE_T B_a = ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear)(FL * B_c / ALWAN_CORE_LITERAL(100.0));

    /* Step 5: Achromatic response A */
    ALWAN_CORE_T A = (ALWAN_CORE_LITERAL(2.0) * R_a + G_a +
                ALWAN_CORE_LITERAL(0.05) * B_a - ALWAN_CORE_LITERAL(0.305)) * Nbb;

    /* Step 6: Opponent colour dimensions */
    ALWAN_CORE_T a = R_a - ALWAN_CORE_LITERAL(12.0) * G_a / ALWAN_CORE_LITERAL(11.0) +
               B_a / ALWAN_CORE_LITERAL(11.0);
    ALWAN_CORE_T b = (R_a + G_a - ALWAN_CORE_LITERAL(2.0) * B_a) / ALWAN_CORE_LITERAL(9.0);

    /* Step 7: Hue angle h (degrees) */
    ALWAN_CORE_T h_rad = ALWAN_CORE_ATAN2(b, a);
    ALWAN_CORE_T h = h_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    h = ALWAN_CORE_SELECT(h < ALWAN_CORE_ZERO, h + ALWAN_CORE_LITERAL(360.0), h);

    /* Step 8: Eccentricity et (match colour-science: radians(h) + 2) */
    ALWAN_CORE_T et = ALWAN_CORE_LITERAL(0.25) * (ALWAN_CORE_COS(h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0) + ALWAN_CORE_LITERAL(2.0)) + ALWAN_CORE_LITERAL(3.8));

    /* Step 9: Hue quadrature H */
    ALWAN_CORE_T H = ALWAN_CORE_FNVLIT(cam_hue_to_quadrature)(h);

    /* Step 10: Lightness J */
    ALWAN_CORE_T J = ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_POW(A / A_w, c * z);

    /* Step 11: Brightness Q */
    ALWAN_CORE_T Q = (ALWAN_CORE_LITERAL(4.0) / c) * ALWAN_CORE_SQRT(J / ALWAN_CORE_LITERAL(100.0)) *
               (A_w + ALWAN_CORE_LITERAL(4.0)) * ALWAN_CORE_POW(FL, ALWAN_CORE_LITERAL(0.25));

    /* Step 12: Chroma C */
    ALWAN_CORE_T t_val = (ALWAN_CORE_LITERAL(50000.0) / ALWAN_CORE_LITERAL(13.0) * Nc * Ncb * et *
                ALWAN_CORE_SQRT(a * a + b * b)) /
               (R_a + G_a + ALWAN_CORE_LITERAL(21.0) / ALWAN_CORE_LITERAL(20.0) * B_a);
    ALWAN_CORE_T C_val = ALWAN_CORE_POW(t_val, ALWAN_CORE_LITERAL(0.9)) *
               ALWAN_CORE_SQRT(J / ALWAN_CORE_LITERAL(100.0)) *
               ALWAN_CORE_POW(ALWAN_CORE_LITERAL(1.64) - ALWAN_CORE_POW(ALWAN_CORE_LITERAL(0.29), n), ALWAN_CORE_LITERAL(0.73));

    /* Step 13: Colorfulness M and saturation s */
    ALWAN_CORE_T M = C_val * ALWAN_CORE_POW(FL, ALWAN_CORE_LITERAL(0.25));
    ALWAN_CORE_T s = ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_SQRT(M / Q);

    result.J = J;
    result.C = C_val;
    result.h = h;
    result.s = s;
    result.Q = Q;
    result.M = M;
    result.H = H;

    return result;
}

/* ================================================================
 * CAM16 Inverse Transform (value-returning)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_cam16_inverse)(
    ALWAN_CORE_T J,
    ALWAN_CORE_T C,
    ALWAN_CORE_T h,
    ALWAN_CORE_XYZ white_xyz,
    ALWAN_CORE_T F,
    ALWAN_CORE_T c,
    ALWAN_CORE_T Nc,
    ALWAN_CORE_T D,
    ALWAN_CORE_T FL,
    ALWAN_CORE_T n,
    ALWAN_CORE_T Nbb,
    ALWAN_CORE_T Ncb,
    ALWAN_CORE_T z,
    ALWAN_CORE_T A_w
) {
    ALWAN_CORE_XYZ result;
    ALWAN_UNUSED(F); /* F already folded into pre-computed D */

    /* Step 1: Compute achromatic response A from J */
    ALWAN_CORE_T A = A_w * ALWAN_CORE_POW(J / ALWAN_CORE_LITERAL(100.0), ALWAN_CORE_ONE / (c * z));

    /* Step 2: Compute t from C */
    ALWAN_CORE_T t_val = ALWAN_CORE_POW(
        C / (ALWAN_CORE_SQRT(J / ALWAN_CORE_LITERAL(100.0)) *
             ALWAN_CORE_POW(ALWAN_CORE_LITERAL(1.64) - ALWAN_CORE_POW(ALWAN_CORE_LITERAL(0.29), n), ALWAN_CORE_LITERAL(0.73))),
        ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(0.9));

    /* Step 3: Eccentricity et */
    ALWAN_CORE_T et = ALWAN_CORE_LITERAL(0.25) * (ALWAN_CORE_COS(h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0) + ALWAN_CORE_LITERAL(2.0)) + ALWAN_CORE_LITERAL(3.8));

    /* Step 4: Compute opponent dimensions a, b via helper */
    ALWAN_CORE_T h_rad = h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_VEC2 ab = ALWAN_CORE_FNVLIT(cam_opponent_colour_inverse)(t_val, h_rad, Nc, Ncb, et, A, Nbb);
    ALWAN_CORE_T a = ab.v[0];
    ALWAN_CORE_T b = ab.v[1];

    /* Step 5: Compute adapted signals RGB_a from a, b, A */
    ALWAN_CORE_T P_2 = A / Nbb + ALWAN_CORE_LITERAL(0.305);
    ALWAN_CORE_T R_a = (ALWAN_CORE_LITERAL(460.0) * P_2 + ALWAN_CORE_LITERAL(451.0) * a + ALWAN_CORE_LITERAL(288.0) * b) / ALWAN_CORE_LITERAL(1403.0);
    ALWAN_CORE_T G_a = (ALWAN_CORE_LITERAL(460.0) * P_2 - ALWAN_CORE_LITERAL(891.0) * a - ALWAN_CORE_LITERAL(261.0) * b) / ALWAN_CORE_LITERAL(1403.0);
    ALWAN_CORE_T B_a = (ALWAN_CORE_LITERAL(460.0) * P_2 - ALWAN_CORE_LITERAL(220.0) * a - ALWAN_CORE_LITERAL(6300.0) * b) / ALWAN_CORE_LITERAL(1403.0);

    /* Step 6: Inverse post-adaptation nonlinearity */
    ALWAN_CORE_T R_c = ALWAN_CORE_LITERAL(100.0) / FL * ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear_inv)(R_a);
    ALWAN_CORE_T G_c = ALWAN_CORE_LITERAL(100.0) / FL * ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear_inv)(G_a);
    ALWAN_CORE_T B_c = ALWAN_CORE_LITERAL(100.0) / FL * ALWAN_CORE_FNVLIT(cam_post_adaptation_nonlinear_inv)(B_a);

    /* Step 7: Inverse chromatic adaptation */
    ALWAN_CORE_VEC3 white_v = {{white_xyz.x, white_xyz.y, white_xyz.z}};
    ALWAN_CORE_VEC3 lms_w = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CAM_M_CAT16), white_v);
    ALWAN_CORE_T R_w = lms_w.v[0];
    ALWAN_CORE_T G_w = lms_w.v[1];
    ALWAN_CORE_T B_w = lms_w.v[2];

    ALWAN_CORE_T D_R = D * (white_xyz.y / R_w) + ALWAN_CORE_ONE - D;
    ALWAN_CORE_T D_G = D * (white_xyz.y / G_w) + ALWAN_CORE_ONE - D;
    ALWAN_CORE_T D_B = D * (white_xyz.y / B_w) + ALWAN_CORE_ONE - D;

    ALWAN_CORE_T R = R_c / D_R;
    ALWAN_CORE_T G = G_c / D_G;
    ALWAN_CORE_T B = B_c / D_B;

    /* Step 8: LMS -> XYZ via inverse CAT16 matrix */
    ALWAN_CORE_VEC3 lms_inv = {{R, G, B}};
    ALWAN_CORE_VEC3 xyz_out = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CAM_M_CAT16_INV), lms_inv);
    result.x = xyz_out.v[0];
    result.y = xyz_out.v[1];
    result.z = xyz_out.v[2];

    return result;
}

/* ================================================================
 * CAM16-UCS Forward Transform (JMh -> Jab) (value-returning)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_CAM_JAB ALWAN_CORE_FNVLIT(alwan_cam16_to_ucs)(
    ALWAN_CORE_T J,
    ALWAN_CORE_T M,
    ALWAN_CORE_T h
) {
    ALWAN_CORE_CAM_JAB result;

    ALWAN_CORE_T J_prime = ALWAN_CORE_LITERAL(1.7) * J / (ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(0.007) * J);
    ALWAN_CORE_T M_prime = ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(0.0228) * ALWAN_CORE_LN(ALWAN_CORE_ONE + ALWAN_CORE_LITERAL(0.0228) * M);

    ALWAN_CORE_T h_rad = h * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    result.J = J_prime;
    result.a = M_prime * ALWAN_CORE_COS(h_rad);
    result.b = M_prime * ALWAN_CORE_SIN(h_rad);

    return result;
}

/* ================================================================
 * CAM16-UCS Inverse Transform (Jab -> JMh) (value-returning)
 * Returns a cam16_v_correlates with only J, M, h populated;
 * C, s, Q, H are set to zero.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_cam16_v_correlates) ALWAN_CORE_FNVLIT(alwan_cam16_from_ucs)(
    ALWAN_CORE_T J_prime,
    ALWAN_CORE_T a_prime,
    ALWAN_CORE_T b_prime
) {
    ALWAN_CORE_FNLIT(alwan_cam16_v_correlates) result;

    ALWAN_CORE_T J = J_prime / (ALWAN_CORE_LITERAL(1.7) - ALWAN_CORE_LITERAL(0.007) * J_prime);
    ALWAN_CORE_T M_prime = ALWAN_CORE_SQRT(a_prime * a_prime + b_prime * b_prime);
    ALWAN_CORE_T M = (ALWAN_CORE_EXP(M_prime * ALWAN_CORE_LITERAL(0.0228)) - ALWAN_CORE_ONE) / ALWAN_CORE_LITERAL(0.0228);
    ALWAN_CORE_T h_rad = ALWAN_CORE_ATAN2(b_prime, a_prime);
    ALWAN_CORE_T h = h_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    h = ALWAN_CORE_SELECT(h < ALWAN_CORE_ZERO, h + ALWAN_CORE_LITERAL(360.0), h);

    result.J = J;
    result.C = ALWAN_CORE_ZERO;
    result.h = h;
    result.s = ALWAN_CORE_ZERO;
    result.Q = ALWAN_CORE_ZERO;
    result.M = M;
    result.H = ALWAN_CORE_ZERO;

    return result;
}
