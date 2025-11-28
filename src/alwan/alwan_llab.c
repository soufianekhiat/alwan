/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * LLAB Color Appearance Model
 * Based on Luo, Lo and Kuo (1996)
 * "The LLAB(l:c) colour model"
 *
 * References:
 * - Luo, M. R., Lo, M.-C., & Kuo, W.-G. (1996). The LLAB(l:c) colour model.
 *   Color Research & Application, 21(6), 412-429.
 * - Fairchild, M. D. (2013). Color Appearance Models (3rd ed.). Wiley.
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <math.h>

/* ----------------------------------------------------------------
 * LLAB Transformation Matrices
 * ---------------------------------------------------------------- */

/* LLAB XYZ to RGB matrix (cone response) */
static alwan_scalar const M_LLAB_XYZ_TO_RGB[] = {
#include "data/matrices/llab_xyz_to_rgb.csv"
};

/* LLAB RGB to XYZ inverse matrix */
static alwan_scalar const M_LLAB_RGB_TO_XYZ[] = {
#include "data/matrices/llab_rgb_to_xyz.csv"
};

/* ----------------------------------------------------------------
 * LLAB Constants and Parameters
 * ---------------------------------------------------------------- */

/* Reference white (D65) */
#define LLAB_D65_X ALWAN_LITERAL(95.05)
#define LLAB_D65_Y ALWAN_LITERAL(100.0)
#define LLAB_D65_Z ALWAN_LITERAL(108.88)

/* Lab transformation constants */
#define LLAB_EPSILON ALWAN_LITERAL(0.008856)  /* (6/29)^3 */
#define LLAB_KAPPA ALWAN_LITERAL(903.3)       /* (29/3)^3 */
#define LLAB_COEF_116 ALWAN_LITERAL(116.0)
#define LLAB_COEF_16 ALWAN_LITERAL(16.0)
#define LLAB_COEF_500 ALWAN_LITERAL(500.0)
#define LLAB_COEF_200 ALWAN_LITERAL(200.0)

/* Chroma scaling constants */
#define LLAB_CHROMA_SCALE ALWAN_LITERAL(25.0)
#define LLAB_CHROMA_CONST ALWAN_LITERAL(0.05)

/* ----------------------------------------------------------------
 * LLAB Viewing Conditions Presets
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_scalar D;   /* Discounting-the-Illuminant factor */
    alwan_scalar F_S; /* Surround induction factor */
    alwan_scalar F_L; /* Lightness induction factor */
    alwan_scalar F_C; /* Chroma induction factor */
} llab_induction_factors;

/* Standard viewing conditions based on surround */
static llab_induction_factors const LLAB_SURROUND_FACTORS[] = {
    /* AVERAGE: Reference samples > 4 degrees */
    {ALWAN_LITERAL(1.0), ALWAN_LITERAL(3.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)},
    /* DIM: Television/VDU displays */
    {ALWAN_LITERAL(0.7), ALWAN_LITERAL(3.5), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},
    /* DARK: 35mm projection transparencies */
    {ALWAN_LITERAL(0.7), ALWAN_LITERAL(4.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)}
};

/* ----------------------------------------------------------------
 * Helper Functions
 * ---------------------------------------------------------------- */

/* 3x3 matrix-vector multiplication */
static void mat3_mul_vec3(alwan_scalar const *mat, alwan_scalar const *vec, alwan_scalar *out) {
    out[0] = mat[0] * vec[0] + mat[1] * vec[1] + mat[2] * vec[2];
    out[1] = mat[3] * vec[0] + mat[4] * vec[1] + mat[5] * vec[2];
    out[2] = mat[6] * vec[0] + mat[7] * vec[1] + mat[8] * vec[2];
}

/* Lab f(t) function with F_S induction factor */
static alwan_scalar llab_f(alwan_scalar t, alwan_scalar F_S) {
    if (t > LLAB_EPSILON) {
        return ALWAN_POW(t, ALWAN_LITERAL(1.0) / F_S);
    } else {
        /* Linear approximation for small values */
        alwan_scalar slope = (ALWAN_POW(LLAB_EPSILON, ALWAN_LITERAL(1.0) / F_S) -
                             LLAB_COEF_16 / LLAB_COEF_116) / LLAB_EPSILON;
        return slope * t + LLAB_COEF_16 / LLAB_COEF_116;
    }
}

/* ----------------------------------------------------------------
 * LLAB Forward Transform
 * ---------------------------------------------------------------- */

int alwan_llab_forward(
    alwan_xyz const *xyz,
    alwan_llab_viewing_conditions const *vc,
    alwan_llab_correlates *out
) {
    if (!xyz || !vc || !out) {
        return ALWAN_E_INVALID;
    }

    /* Get induction factors based on surround */
    llab_induction_factors const *factors = &LLAB_SURROUND_FACTORS[vc->surround];
    alwan_scalar D = factors->D;
    alwan_scalar F_S = factors->F_S;
    alwan_scalar F_L = factors->F_L;

    /* Override D if user specified */
    if (vc->D_factor >= 0) {
        D = (alwan_scalar)vc->D_factor;
    }

    /* Step 1: Convert XYZ to RGB (cone responses) */
    alwan_scalar XYZ_in[3] = {xyz->x, xyz->y, xyz->z};
    alwan_scalar RGB[3];
    mat3_mul_vec3(M_LLAB_XYZ_TO_RGB, XYZ_in, RGB);

    /* Convert reference illuminants to RGB */
    alwan_scalar XYZ_0[3] = {vc->xyz_0.x, vc->xyz_0.y, vc->xyz_0.z};
    alwan_scalar XYZ_r[3] = {vc->xyz_r.x, vc->xyz_r.y, vc->xyz_r.z};
    alwan_scalar RGB_0[3], RGB_r[3];
    mat3_mul_vec3(M_LLAB_XYZ_TO_RGB, XYZ_0, RGB_0);
    mat3_mul_vec3(M_LLAB_XYZ_TO_RGB, XYZ_r, RGB_r);

    /* Step 2: Apply BFD chromatic adaptation */
    alwan_scalar RGB_adapted[3];

    /* R and G channels: linear adaptation */
    for (int i = 0; i < 2; i++) {
        alwan_scalar ratio = RGB_r[i] / RGB_0[i];
        RGB_adapted[i] = (D * ratio + (ALWAN_LITERAL(1.0) - D)) * RGB[i];
    }

    /* B channel: nonlinear adaptation with power function */
    alwan_scalar beta = ALWAN_POW(RGB_0[2] / RGB_r[2], ALWAN_LITERAL(0.0834));
    alwan_scalar B_adapted_factor = D * ALWAN_POW(RGB_r[2] / RGB_0[2], beta) +
                                    (ALWAN_LITERAL(1.0) - D);
    RGB_adapted[2] = B_adapted_factor * ALWAN_POW(RGB[2], beta);

    /* Step 3: Convert adapted RGB back to XYZ */
    alwan_scalar XYZ_adapted[3];
    mat3_mul_vec3(M_LLAB_RGB_TO_XYZ, RGB_adapted, XYZ_adapted);

    /* Step 4: Compute Lab coordinates with F_S induction factor */
    /* Reference white for Lab calculation (use reference condition illuminant) */
    alwan_scalar X_n = vc->xyz_r.x;
    alwan_scalar Y_n = vc->xyz_r.y;
    alwan_scalar Z_n = vc->xyz_r.z;

    alwan_scalar f_X = llab_f(XYZ_adapted[0] / X_n, F_S);
    alwan_scalar f_Y = llab_f(XYZ_adapted[1] / Y_n, F_S);
    alwan_scalar f_Z = llab_f(XYZ_adapted[2] / Z_n, F_S);

    /* Compute z factor for lightness modulation */
    alwan_scalar z = ALWAN_LITERAL(1.0) + F_L * ALWAN_SQRT(vc->Y_b / ALWAN_LITERAL(100.0));

    /* LLAB lightness with z modulation */
    alwan_scalar L_L = LLAB_COEF_116 * ALWAN_POW(f_Y, z) - LLAB_COEF_16;

    /* LLAB opponent dimensions */
    alwan_scalar a_L = LLAB_COEF_500 * (f_X - f_Y);
    alwan_scalar b_L = LLAB_COEF_200 * (f_Y - f_Z);

    /* Step 5: Compute appearance correlates */

    /* Chroma */
    alwan_scalar c = ALWAN_SQRT(a_L * a_L + b_L * b_L);
    alwan_scalar Ch_L = LLAB_CHROMA_SCALE *
                        ALWAN_LOG(ALWAN_LITERAL(1.0) + LLAB_CHROMA_CONST * c);

    /* Hue angle */
    alwan_scalar h_L = ALWAN_ATAN2(b_L, a_L) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    if (h_L < ALWAN_LITERAL(0.0)) {
        h_L += ALWAN_LITERAL(360.0);
    }

    /* Saturation */
    alwan_scalar s_L = ALWAN_LITERAL(0.0);
    if (L_L > ALWAN_LITERAL(0.0)) {
        s_L = Ch_L / L_L;
    }

    /* Output correlates */
    out->L = L_L;
    out->Ch = Ch_L;
    out->h = h_L;
    out->s = s_L;

    return ALWAN_OK;
}
