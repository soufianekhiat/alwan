/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Hunter Lab Color Space
 *
 * Reference: Hunter (1948), ASTM D 1535
 * Earlier Lab-type color space using square roots instead of cube roots
 * Ka and Kb coefficients depend on the illuminant
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ----------------------------------------------------------------
 * Hunter Lab Illuminant Coefficients
 * ---------------------------------------------------------------- */

/* Ka and Kb calculation constants (for reference white normalization) */
static alwan_scalar const HUNTER_KA_XN_REF = ALWAN_LITERAL(98.043);   /* X for C illuminant */
static alwan_scalar const HUNTER_KB_ZN_REF = ALWAN_LITERAL(118.115);  /* Z for C illuminant */

/* D65 Illuminant coefficients (precomputed for efficiency) */
static alwan_scalar const HUNTER_KA_D65 = ALWAN_LITERAL(172.30);
static alwan_scalar const HUNTER_KB_D65 = ALWAN_LITERAL(67.20);

/* ----------------------------------------------------------------
 * Helper: Calculate Ka and Kb for custom illuminant
 * ---------------------------------------------------------------- */

static void calculate_hunter_coefficients(alwan_xyz const *xyz_n,
                                          alwan_scalar *ka,
                                          alwan_scalar *kb) {
    /* Ka = 175 × √(Xn / 98.043) */
    *ka = ALWAN_LITERAL(175.0) * ALWAN_SQRT(xyz_n->x / HUNTER_KA_XN_REF);

    /* Kb = 70 × √(Zn / 118.115) */
    *kb = ALWAN_LITERAL(70.0) * ALWAN_SQRT(xyz_n->z / HUNTER_KB_ZN_REF);
}

/* ----------------------------------------------------------------
 * XYZ <-> Hunter Lab (D65 Illuminant)
 * ---------------------------------------------------------------- */

void alwan_xyz_to_hunter_lab(alwan_hunter_lab *hunter_lab, alwan_xyz const *xyz) {
    /* D65 reference white (Y = 100 scale) - from colour-science TVS_ILLUMINANTS_HUNTERLAB */
    alwan_scalar const xn = ALWAN_LITERAL(95.02);
    alwan_scalar const yn = ALWAN_LITERAL(100.0);
    alwan_scalar const zn = ALWAN_LITERAL(108.82);

    /* D65 coefficients */
    alwan_scalar ka = HUNTER_KA_D65;
    alwan_scalar kb = HUNTER_KB_D65;

    /* Calculate ratios - XYZ and XYZ_n both in Y=100 scale */
    alwan_scalar y_ratio = xyz->y / yn;
    alwan_scalar sqrt_y_ratio = ALWAN_SQRT(y_ratio);

    /* Guard against division by zero */
    if (sqrt_y_ratio < ALWAN_LITERAL(1e-10)) {
        hunter_lab->L = ALWAN_LITERAL(0.0);
        hunter_lab->a = ALWAN_LITERAL(0.0);
        hunter_lab->b = ALWAN_LITERAL(0.0);
        return;
    }

    /* L = 100 × √(Y/Yn) - colour-science formula */
    hunter_lab->L = ALWAN_LITERAL(100.0) * sqrt_y_ratio;

    /* a = Ka × ((X/Xn - Y/Yn) / √(Y/Yn)) */
    alwan_scalar x_ratio = xyz->x / xn;
    hunter_lab->a = ka * (x_ratio - y_ratio) / sqrt_y_ratio;

    /* b = Kb × ((Y/Yn - Z/Zn) / √(Y/Yn)) */
    alwan_scalar z_ratio = xyz->z / zn;
    hunter_lab->b = kb * (y_ratio - z_ratio) / sqrt_y_ratio;
}

void alwan_hunter_lab_to_xyz(alwan_xyz *xyz, alwan_hunter_lab const *hunter_lab) {
    /* D65 reference white (Y = 100 scale) - from colour-science TVS_ILLUMINANTS_HUNTERLAB */
    alwan_scalar const xn = ALWAN_LITERAL(95.02);
    alwan_scalar const yn = ALWAN_LITERAL(100.0);
    alwan_scalar const zn = ALWAN_LITERAL(108.82);

    /* D65 coefficients */
    alwan_scalar ka = HUNTER_KA_D65;
    alwan_scalar kb = HUNTER_KB_D65;

    /* L/100 - from colour-science formula */
    alwan_scalar l_norm = hunter_lab->L / ALWAN_LITERAL(100.0);

    /* Y = (L/100)² × Yn - XYZ in Y=100 scale */
    xyz->y = l_norm * l_norm * yn;

    /* X = ((a/Ka) × (L/100) + (L/100)²) × Xn */
    alwan_scalar a_term = (hunter_lab->a / ka) * l_norm;
    xyz->x = (a_term + l_norm * l_norm) * xn;

    /* Z = -((b/Kb) × (L/100) - (L/100)²) × Zn */
    alwan_scalar b_term = (hunter_lab->b / kb) * l_norm;
    xyz->z = -(b_term - l_norm * l_norm) * zn;
}

/* ----------------------------------------------------------------
 * XYZ <-> Hunter Lab (Custom Illuminant)
 * ---------------------------------------------------------------- */

void alwan_xyz_to_hunter_lab_custom(alwan_hunter_lab *hunter_lab,
                                     alwan_xyz const *xyz,
                                     alwan_xyz const *xyz_n) {
    /* Calculate Ka and Kb for the given illuminant */
    alwan_scalar ka, kb;
    calculate_hunter_coefficients(xyz_n, &ka, &kb);

    /* Calculate ratios */
    alwan_scalar y_ratio = xyz->y / xyz_n->y;
    alwan_scalar sqrt_y_ratio = ALWAN_SQRT(y_ratio);

    /* Guard against division by zero */
    if (sqrt_y_ratio < ALWAN_LITERAL(1e-10)) {
        hunter_lab->L = ALWAN_LITERAL(0.0);
        hunter_lab->a = ALWAN_LITERAL(0.0);
        hunter_lab->b = ALWAN_LITERAL(0.0);
        return;
    }

    /* L = 10 × √(Y/Yn) - Hunter Lab uses 0-10 scale for L */
    hunter_lab->L = ALWAN_LITERAL(10.0) * sqrt_y_ratio;

    /* a = Ka × ((X/Xn - Y/Yn) / √(Y/Yn)) */
    alwan_scalar x_ratio = xyz->x / xyz_n->x;
    hunter_lab->a = ka * (x_ratio - y_ratio) / sqrt_y_ratio;

    /* b = Kb × ((Y/Yn - Z/Zn) / √(Y/Yn)) */
    alwan_scalar z_ratio = xyz->z / xyz_n->z;
    hunter_lab->b = kb * (y_ratio - z_ratio) / sqrt_y_ratio;
}

void alwan_hunter_lab_to_xyz_custom(alwan_xyz *xyz,
                                     alwan_hunter_lab const *hunter_lab,
                                     alwan_xyz const *xyz_n) {
    /* Calculate Ka and Kb for the given illuminant */
    alwan_scalar ka, kb;
    calculate_hunter_coefficients(xyz_n, &ka, &kb);

    /* L/10 - Hunter Lab uses 0-10 scale for L */
    alwan_scalar l_norm = hunter_lab->L / ALWAN_LITERAL(10.0);

    /* Y = (L/10)² × Yn */
    xyz->y = l_norm * l_norm * xyz_n->y;

    /* X = ((a/Ka) × (L/100) + (L/100)²) × Xn */
    alwan_scalar a_term = (hunter_lab->a / ka) * l_norm;
    xyz->x = (a_term + l_norm * l_norm) * xyz_n->x;

    /* Z = -((b/Kb) × (L/100) - (L/100)²) × Zn */
    alwan_scalar b_term = (hunter_lab->b / kb) * l_norm;
    xyz->z = -(b_term - l_norm * l_norm) * xyz_n->z;
}
