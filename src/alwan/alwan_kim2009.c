/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Kim, Weyrich and Kautz (2009) Color Appearance Model Implementation
 * Based on: Kim, M. H., Weyrich, T., & Kautz, J. (2009). Modeling Human Color Perception
 * under Extended Luminance Levels. ACM Transactions on Graphics, 28(3), 27:1-27:9.
 *
 * Kim2009 extends CIECAM02 to handle high dynamic range viewing conditions by
 * introducing media-specific parameters that modulate lightness prediction.
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Kim2009 Constants and Matrices
 * ---------------------------------------------------------------- */

/* CAT02 matrix for chromatic adaptation transform (from colour-science) */
static alwan_scalar const M_CAT02[9] = {
#include "data/matrices/cat_cat02.csv"
};

/* Inverse CAT02 matrix (from colour-science) */
static alwan_scalar const M_CAT02_inv[9] = {
#include "data/matrices/cat_cat02_inv.csv"
};

/* Hunt-Pointer-Estevez (HPE) matrix for XYZ to LMS conversion (from colour-science) */
static alwan_scalar const M_HPE[9] = {
#include "data/matrices/hpe.csv"
};

/* Inverse HPE matrix for LMS to XYZ conversion (from colour-science) */
static alwan_scalar const M_HPE_inv[9] = {
#include "data/matrices/hpe_inv.csv"
};

/* ----------------------------------------------------------------
 * Kim2009 Mathematical Constants
 * ---------------------------------------------------------------- */

/* Cone response sigmoidal curve modulation factor */
#define KIM2009_N_C ALWAN_LITERAL(0.57)

/* Brightness exponent */
#define KIM2009_N_Q ALWAN_LITERAL(0.1308)

/* Chroma scaling and exponent */
#define KIM2009_A_K ALWAN_LITERAL(456.5)
#define KIM2009_N_K ALWAN_LITERAL(0.62)

/* Colourfulness coefficients */
#define KIM2009_A_M ALWAN_LITERAL(0.11)
#define KIM2009_B_M ALWAN_LITERAL(0.61)

/* Lightness function coefficients */
#define KIM2009_A_J ALWAN_LITERAL(0.89)
#define KIM2009_B_J ALWAN_LITERAL(0.24)
#define KIM2009_N_J ALWAN_LITERAL(3.65)
#define KIM2009_O_J ALWAN_LITERAL(0.65)

/* Achromatic signal weights [40, 20, 1] / 61 */
#define KIM2009_V_A_L ALWAN_LITERAL(40.0)
#define KIM2009_V_A_M ALWAN_LITERAL(20.0)
#define KIM2009_V_A_S ALWAN_LITERAL(1.0)
#define KIM2009_V_A_SUM ALWAN_LITERAL(61.0)

/* ----------------------------------------------------------------
 * Helper Functions
 * ---------------------------------------------------------------- */

/* Get viewing condition induction factors based on background luminance Yb
 * Following CIECAM02 conventions, we derive surround from Yb relative to white */
static void get_induction_factors(
    alwan_scalar Yb,
    alwan_scalar Y_w,
    alwan_scalar *F,
    alwan_scalar *c,
    alwan_scalar *Nc
) {
    /* Compute relative background: n = Yb / Yw */
    alwan_scalar n = Yb / Y_w;

    /* Derive surround from background:
     * n >= 0.18 -> Average surround
     * 0.18 > n >= 0.01 -> Dim surround
     * n < 0.01 -> Dark surround
     */
    if (n >= ALWAN_LITERAL(0.18)) {
        /* Average surround */
        *F = ALWAN_LITERAL(1.0);
        *c = ALWAN_LITERAL(0.69);
        *Nc = ALWAN_LITERAL(1.0);
    } else if (n >= ALWAN_LITERAL(0.01)) {
        /* Dim surround */
        *F = ALWAN_LITERAL(0.9);
        *c = ALWAN_LITERAL(0.59);
        *Nc = ALWAN_LITERAL(0.95);
    } else {
        /* Dark surround */
        *F = ALWAN_LITERAL(0.8);
        *c = ALWAN_LITERAL(0.525);
        *Nc = ALWAN_LITERAL(0.8);
    }
}

/* Compute degree of adaptation D from F and adapting luminance */
static alwan_scalar compute_degree_of_adaptation(
    alwan_scalar F,
    alwan_scalar L_A,
    int discount_illuminant
) {
    if (discount_illuminant) {
        return ALWAN_LITERAL(1.0);
    }

    alwan_scalar D = F * (ALWAN_LITERAL(1.0) -
                          (ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.6)) *
                          ALWAN_EXP((-L_A - ALWAN_LITERAL(42.0)) / ALWAN_LITERAL(92.0)));

    /* Clamp D to [0, 1] */
    if (D < ALWAN_LITERAL(0.0)) D = ALWAN_LITERAL(0.0);
    if (D > ALWAN_LITERAL(1.0)) D = ALWAN_LITERAL(1.0);

    return D;
}

/* Apply CAT02 chromatic adaptation (forward: RGB -> RGB_adapted) */
static void apply_chromatic_adaptation_forward(
    alwan_scalar const RGB[3],
    alwan_scalar const RGB_w[3],
    alwan_scalar Y_w,
    alwan_scalar D,
    alwan_scalar RGB_adapted[3]
) {
    for (int i = 0; i < 3; i++) {
        alwan_scalar factor = D * (Y_w / RGB_w[i]) + (ALWAN_LITERAL(1.0) - D);
        RGB_adapted[i] = RGB[i] * factor;
    }
}

/* Apply CAT02 chromatic adaptation (inverse: RGB_adapted -> RGB) */
static void apply_chromatic_adaptation_inverse(
    alwan_scalar const RGB_adapted[3],
    alwan_scalar const RGB_w[3],
    alwan_scalar Y_w,
    alwan_scalar D,
    alwan_scalar RGB[3]
) {
    for (int i = 0; i < 3; i++) {
        alwan_scalar factor = D * (Y_w / RGB_w[i]) + (ALWAN_LITERAL(1.0) - D);
        RGB[i] = RGB_adapted[i] / factor;
    }
}

/* Matrix-vector multiply: out = M * v */
static void mat3x3_mul_vec3(
    alwan_scalar const M[9],
    alwan_scalar const v[3],
    alwan_scalar out[3]
) {
    out[0] = M[0] * v[0] + M[1] * v[1] + M[2] * v[2];
    out[1] = M[3] * v[0] + M[4] * v[1] + M[5] * v[2];
    out[2] = M[6] * v[0] + M[7] * v[1] + M[8] * v[2];
}

/* ----------------------------------------------------------------
 * Kim2009 Forward Transform: XYZ -> Appearance Correlates
 * ---------------------------------------------------------------- */

int alwan_kim2009_forward(
    alwan_vec3 const *xyz,
    alwan_kim2009_viewing_conditions const *vc,
    alwan_kim2009_correlates *out
) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Step 1: Get induction factors based on background luminance */
    alwan_scalar F, c, Nc;
    alwan_scalar Y_w = vc->white_xyz.v[1];
    get_induction_factors(vc->Yb, Y_w, &F, &c, &Nc);

    /* Step 2: Compute degree of adaptation D */
    alwan_scalar D = compute_degree_of_adaptation(F, vc->La, vc->discount_illuminant);

    /* Step 3: CAT02 transform - XYZ to RGB (sharpened cone responses) */
    alwan_scalar RGB[3];
    alwan_scalar XYZ_in[3] = {xyz->v[0], xyz->v[1], xyz->v[2]};
    mat3x3_mul_vec3(M_CAT02, XYZ_in, RGB);

    /* Step 4: CAT02 transform - white point XYZ_w to RGB_w */
    alwan_scalar RGB_w[3];
    alwan_scalar XYZ_w[3] = {vc->white_xyz.v[0], vc->white_xyz.v[1], vc->white_xyz.v[2]};
    mat3x3_mul_vec3(M_CAT02, XYZ_w, RGB_w);

    /* Step 5: Apply chromatic adaptation to get RGB_c and RGB_wc */
    alwan_scalar RGB_c[3], RGB_wc[3];
    apply_chromatic_adaptation_forward(RGB, RGB_w, Y_w, D, RGB_c);
    apply_chromatic_adaptation_forward(RGB_w, RGB_w, Y_w, D, RGB_wc);

    /* Step 6: Inverse CAT02 to get adapted XYZ_c and XYZ_wc */
    alwan_scalar XYZ_c[3], XYZ_wc[3];
    mat3x3_mul_vec3(M_CAT02_inv, RGB_c, XYZ_c);
    mat3x3_mul_vec3(M_CAT02_inv, RGB_wc, XYZ_wc);

    /* Step 7: HPE transform - convert to cone fundamentals LMS and LMS_w */
    alwan_scalar LMS[3], LMS_w[3];
    mat3x3_mul_vec3(M_HPE, XYZ_c, LMS);
    mat3x3_mul_vec3(M_HPE, XYZ_wc, LMS_w);

    /* Step 8: Cone response - power law with additive term (n_c = 0.57) */
    alwan_scalar n_c = KIM2009_N_C;
    alwan_scalar L_A_n_c = ALWAN_POW(vc->La, n_c);

    alwan_scalar LMS_p[3], LMS_wp[3];
    for (int i = 0; i < 3; i++) {
        alwan_scalar LMS_n_c = ALWAN_POW(LMS[i], n_c);
        alwan_scalar LMS_w_n_c = ALWAN_POW(LMS_w[i], n_c);

        LMS_p[i] = LMS_n_c / (LMS_n_c + L_A_n_c);
        LMS_wp[i] = LMS_w_n_c / (LMS_w_n_c + L_A_n_c);
    }

    /* Step 9: Achromatic signals A and A_w */
    alwan_scalar A = (KIM2009_V_A_L * LMS_p[0] +
                      KIM2009_V_A_M * LMS_p[1] +
                      KIM2009_V_A_S * LMS_p[2]) / KIM2009_V_A_SUM;

    alwan_scalar A_w = (KIM2009_V_A_L * LMS_wp[0] +
                        KIM2009_V_A_M * LMS_wp[1] +
                        KIM2009_V_A_S * LMS_wp[2]) / KIM2009_V_A_SUM;

    /* Step 10: Perceived lightness J_p */
    alwan_scalar A_A_w = A / A_w;
    alwan_scalar o_j_n_j = ALWAN_POW(KIM2009_O_J, KIM2009_N_J);
    alwan_scalar numerator = -(A_A_w - KIM2009_B_J) * o_j_n_j;
    alwan_scalar denominator = A_A_w - KIM2009_B_J - KIM2009_A_J;
    alwan_scalar J_p = ALWAN_POW(numerator / denominator, ALWAN_LITERAL(1.0) / KIM2009_N_J);

    /* Step 11: Media-dependent lightness J (KEY DIFFERENCE from CIECAM02)
     * Use default media parameter E = 1.0 (CRT Displays) */
    alwan_scalar media_E = ALWAN_LITERAL(1.0);
    alwan_scalar J = ALWAN_LITERAL(100.0) * (media_E * (J_p - ALWAN_LITERAL(1.0)) + ALWAN_LITERAL(1.0));

    /* Step 12: Opponent signals a and b for chroma computation */
    alwan_scalar a = (ALWAN_LITERAL(11.0) * LMS_p[0] -
                      ALWAN_LITERAL(12.0) * LMS_p[1] +
                      ALWAN_LITERAL(1.0) * LMS_p[2]) / ALWAN_LITERAL(11.0);

    alwan_scalar b = (ALWAN_LITERAL(1.0) * LMS_p[0] +
                      ALWAN_LITERAL(1.0) * LMS_p[1] -
                      ALWAN_LITERAL(2.0) * LMS_p[2]) / ALWAN_LITERAL(9.0);

    /* Step 13: Chroma C */
    alwan_scalar C = KIM2009_A_K * ALWAN_POW(a * a + b * b, KIM2009_N_K);

    /* Step 14: Hue angle h */
    alwan_scalar h = ALWAN_ATAN2(b, a) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    if (h < ALWAN_LITERAL(0.0)) {
        h += ALWAN_LITERAL(360.0);
    }

    /* Fill output correlates (only J, C, h as per API) */
    out->J = J;
    out->C = C;
    out->h = h;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Kim2009 Inverse Transform: Appearance Correlates -> XYZ
 * ---------------------------------------------------------------- */

int alwan_kim2009_inverse(
    alwan_kim2009_correlates const *correlates,
    alwan_kim2009_viewing_conditions const *vc,
    alwan_vec3 *out
) {
    if (!correlates || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Validate required correlates: J, C, and h */
    /* C must be provided for inverse transform */

    /* Step 1: Get induction factors based on background luminance */
    alwan_scalar F, c, Nc;
    alwan_scalar Y_w = vc->white_xyz.v[1];
    get_induction_factors(vc->Yb, Y_w, &F, &c, &Nc);

    /* Step 2: Compute degree of adaptation D */
    alwan_scalar D = compute_degree_of_adaptation(F, vc->La, vc->discount_illuminant);

    /* Step 3: CAT02 transform - white point to RGB_w */
    alwan_scalar RGB_w[3];
    alwan_scalar XYZ_w[3] = {vc->white_xyz.v[0], vc->white_xyz.v[1], vc->white_xyz.v[2]};
    mat3x3_mul_vec3(M_CAT02, XYZ_w, RGB_w);

    /* Step 4: Apply chromatic adaptation to white point */
    alwan_scalar RGB_wc[3];
    apply_chromatic_adaptation_forward(RGB_w, RGB_w, Y_w, D, RGB_wc);

    /* Step 5: Inverse CAT02 to get adapted XYZ_wc */
    alwan_scalar XYZ_wc[3];
    mat3x3_mul_vec3(M_CAT02_inv, RGB_wc, XYZ_wc);

    /* Step 6: HPE transform - white point to LMS_w */
    alwan_scalar LMS_w[3];
    mat3x3_mul_vec3(M_HPE, XYZ_wc, LMS_w);

    /* Step 7: Cone response for white point */
    alwan_scalar n_c = KIM2009_N_C;
    alwan_scalar L_A_n_c = ALWAN_POW(vc->La, n_c);

    alwan_scalar LMS_wp[3];
    for (int i = 0; i < 3; i++) {
        alwan_scalar LMS_w_n_c = ALWAN_POW(LMS_w[i], n_c);
        LMS_wp[i] = LMS_w_n_c / (LMS_w_n_c + L_A_n_c);
    }

    /* Step 8: Achromatic signal for white */
    alwan_scalar A_w = (KIM2009_V_A_L * LMS_wp[0] +
                        KIM2009_V_A_M * LMS_wp[1] +
                        KIM2009_V_A_S * LMS_wp[2]) / KIM2009_V_A_SUM;

    /* Step 9: Recover perceived lightness J_p from J
     * Use default media parameter E = 1.0 (CRT Displays) */
    alwan_scalar J = correlates->J;
    alwan_scalar media_E = ALWAN_LITERAL(1.0);
    alwan_scalar J_p = (J / ALWAN_LITERAL(100.0) - ALWAN_LITERAL(1.0)) / media_E + ALWAN_LITERAL(1.0);

    /* Step 10: Recover achromatic signal A from J_p */
    alwan_scalar J_p_n_j = ALWAN_POW(J_p, KIM2009_N_J);
    alwan_scalar o_j_n_j = ALWAN_POW(KIM2009_O_J, KIM2009_N_J);
    alwan_scalar A = A_w * ((KIM2009_A_J * J_p_n_j) / (J_p_n_j + o_j_n_j) + KIM2009_B_J);

    /* Step 11: Get C from correlates */
    alwan_scalar C = correlates->C;

    /* Step 12: Recover opponent signals a and b from C and h */
    alwan_scalar h = correlates->h;
    alwan_scalar hr = h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar C_a_k_n_k = ALWAN_POW(C / KIM2009_A_K, ALWAN_LITERAL(1.0) / KIM2009_N_K);
    alwan_scalar a = ALWAN_COS(hr) * C_a_k_n_k;
    alwan_scalar b = ALWAN_SIN(hr) * C_a_k_n_k;

    /* Step 13: Recover LMS_p from A, a, b using inverse matrix
     * LMS_p = M_inv * [A, a, b]
     * Where M = [[1.0000,  0.3215,  0.2053],
     *            [1.0000, -0.6351, -0.1860],
     *            [1.0000, -0.1568, -4.4904]]
     */
    alwan_scalar LMS_p[3];
    LMS_p[0] = A + ALWAN_LITERAL(0.3215) * a + ALWAN_LITERAL(0.2053) * b;
    LMS_p[1] = A - ALWAN_LITERAL(0.6351) * a - ALWAN_LITERAL(0.1860) * b;
    LMS_p[2] = A - ALWAN_LITERAL(0.1568) * a - ALWAN_LITERAL(4.4904) * b;

    /* Step 14: Recover absolute cone responses LMS from LMS_p
     * LMS = [(-L_A^n_c * LMS_p) / (LMS_p - 1)]^(1/n_c)
     */
    alwan_scalar LMS[3];
    for (int i = 0; i < 3; i++) {
        alwan_scalar numerator = -L_A_n_c * LMS_p[i];
        alwan_scalar denominator = LMS_p[i] - ALWAN_LITERAL(1.0);

        /* Handle division carefully */
        if (ALWAN_FABS(denominator) < ALWAN_LITERAL(1e-10)) {
            LMS[i] = ALWAN_LITERAL(0.0);
        } else {
            alwan_scalar ratio = numerator / denominator;
            if (ratio < ALWAN_LITERAL(0.0)) {
                ratio = ALWAN_LITERAL(0.0);  /* Clamp to valid range */
            }
            LMS[i] = ALWAN_POW(ratio, ALWAN_LITERAL(1.0) / n_c);
        }
    }

    /* Step 15: Inverse HPE transform - LMS to adapted XYZ_c */
    alwan_scalar XYZ_c[3];
    mat3x3_mul_vec3(M_HPE_inv, LMS, XYZ_c);

    /* Step 16: CAT02 transform - XYZ_c to RGB_c */
    alwan_scalar RGB_c[3];
    mat3x3_mul_vec3(M_CAT02, XYZ_c, RGB_c);

    /* Step 17: Inverse chromatic adaptation - RGB_c to RGB */
    alwan_scalar RGB[3];
    apply_chromatic_adaptation_inverse(RGB_c, RGB_w, Y_w, D, RGB);

    /* Step 18: Inverse CAT02 - RGB to XYZ */
    alwan_scalar XYZ_out[3];
    mat3x3_mul_vec3(M_CAT02_inv, RGB, XYZ_out);

    /* Fill output XYZ */
    out->v[0] = XYZ_out[0];
    out->v[1] = XYZ_out[1];
    out->v[2] = XYZ_out[2];

    return ALWAN_OK;
}
