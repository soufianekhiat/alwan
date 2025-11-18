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

static void calculate_hunter_coefficients(alwan_vec3 const *xyz_n,
                                          alwan_scalar *ka,
                                          alwan_scalar *kb) {
    /* Ka = 175 × √(Xn / 98.043) */
    *ka = ALWAN_LITERAL(175.0) * ALWAN_SQRT(xyz_n->v[0] / HUNTER_KA_XN_REF);

    /* Kb = 70 × √(Zn / 118.115) */
    *kb = ALWAN_LITERAL(70.0) * ALWAN_SQRT(xyz_n->v[2] / HUNTER_KB_ZN_REF);
}

/* ----------------------------------------------------------------
 * XYZ <-> Hunter Lab (D65 Illuminant)
 * ---------------------------------------------------------------- */

void alwan_xyz_to_hunter_lab(alwan_vec3 const *xyz, alwan_vec3 *hunter_lab) {
    /* D65 reference white (Y = 100) - from colour-science TVS_ILLUMINANTS_HUNTERLAB */
    alwan_scalar const xn = ALWAN_LITERAL(95.02);
    alwan_scalar const yn = ALWAN_LITERAL(100.0);
    alwan_scalar const zn = ALWAN_LITERAL(108.82);

    /* D65 coefficients */
    alwan_scalar ka = HUNTER_KA_D65;
    alwan_scalar kb = HUNTER_KB_D65;

    /* Convert XYZ from Y=100 scale to Y=1 scale to match colour-science */
    alwan_scalar x_norm = xyz->v[0] / ALWAN_LITERAL(100.0);
    alwan_scalar y_norm = xyz->v[1] / ALWAN_LITERAL(100.0);
    alwan_scalar z_norm = xyz->v[2] / ALWAN_LITERAL(100.0);

    /* Calculate ratios (XYZ is now on Y=1 scale, XYZ_n is on Y=100 scale) */
    alwan_scalar y_ratio = y_norm / yn;
    alwan_scalar sqrt_y_ratio = ALWAN_SQRT(y_ratio);

    /* Guard against division by zero */
    if (sqrt_y_ratio < ALWAN_LITERAL(1e-10)) {
        hunter_lab->v[0] = ALWAN_LITERAL(0.0);
        hunter_lab->v[1] = ALWAN_LITERAL(0.0);
        hunter_lab->v[2] = ALWAN_LITERAL(0.0);
        return;
    }

    /* L = 100 × √(Y/Yn) - colour-science formula */
    hunter_lab->v[0] = ALWAN_LITERAL(100.0) * sqrt_y_ratio;

    /* a = Ka × ((X/Xn - Y/Yn) / √(Y/Yn)) */
    alwan_scalar x_ratio = x_norm / xn;
    hunter_lab->v[1] = ka * (x_ratio - y_ratio) / sqrt_y_ratio;

    /* b = Kb × ((Y/Yn - Z/Zn) / √(Y/Yn)) */
    alwan_scalar z_ratio = z_norm / zn;
    hunter_lab->v[2] = kb * (y_ratio - z_ratio) / sqrt_y_ratio;
}

void alwan_hunter_lab_to_xyz(alwan_vec3 const *hunter_lab, alwan_vec3 *xyz) {
    /* D65 reference white (Y = 100) - from colour-science TVS_ILLUMINANTS_HUNTERLAB */
    alwan_scalar const xn = ALWAN_LITERAL(95.02);
    alwan_scalar const yn = ALWAN_LITERAL(100.0);
    alwan_scalar const zn = ALWAN_LITERAL(108.82);

    /* D65 coefficients */
    alwan_scalar ka = HUNTER_KA_D65;
    alwan_scalar kb = HUNTER_KB_D65;

    /* L/100 - from colour-science formula */
    alwan_scalar l_norm = hunter_lab->v[0] / ALWAN_LITERAL(100.0);

    /* Y = (L/100)² × Yn × 100 - converts from Y=1 scale back to Y=100 scale */
    xyz->v[1] = l_norm * l_norm * yn * ALWAN_LITERAL(100.0);

    /* X = ((a/Ka) × (L/100) + (L/100)²) × Xn × 100 */
    alwan_scalar a_term = (hunter_lab->v[1] / ka) * l_norm;
    xyz->v[0] = (a_term + l_norm * l_norm) * xn * ALWAN_LITERAL(100.0);

    /* Z = -((b/Kb) × (L/100) - (L/100)²) × Zn × 100 */
    alwan_scalar b_term = (hunter_lab->v[2] / kb) * l_norm;
    xyz->v[2] = -(b_term - l_norm * l_norm) * zn * ALWAN_LITERAL(100.0);
}

/* ----------------------------------------------------------------
 * XYZ <-> Hunter Lab (Custom Illuminant)
 * ---------------------------------------------------------------- */

void alwan_xyz_to_hunter_lab_custom(alwan_vec3 const *xyz,
                                     alwan_vec3 *hunter_lab,
                                     alwan_vec3 const *xyz_n) {
    /* Calculate Ka and Kb for the given illuminant */
    alwan_scalar ka, kb;
    calculate_hunter_coefficients(xyz_n, &ka, &kb);

    /* Calculate ratios */
    alwan_scalar y_ratio = xyz->v[1] / xyz_n->v[1];
    alwan_scalar sqrt_y_ratio = ALWAN_SQRT(y_ratio);

    /* Guard against division by zero */
    if (sqrt_y_ratio < ALWAN_LITERAL(1e-10)) {
        hunter_lab->v[0] = ALWAN_LITERAL(0.0);
        hunter_lab->v[1] = ALWAN_LITERAL(0.0);
        hunter_lab->v[2] = ALWAN_LITERAL(0.0);
        return;
    }

    /* L = 10 × √(Y/Yn) - Hunter Lab uses 0-10 scale for L */
    hunter_lab->v[0] = ALWAN_LITERAL(10.0) * sqrt_y_ratio;

    /* a = Ka × ((X/Xn - Y/Yn) / √(Y/Yn)) */
    alwan_scalar x_ratio = xyz->v[0] / xyz_n->v[0];
    hunter_lab->v[1] = ka * (x_ratio - y_ratio) / sqrt_y_ratio;

    /* b = Kb × ((Y/Yn - Z/Zn) / √(Y/Yn)) */
    alwan_scalar z_ratio = xyz->v[2] / xyz_n->v[2];
    hunter_lab->v[2] = kb * (y_ratio - z_ratio) / sqrt_y_ratio;
}

void alwan_hunter_lab_to_xyz_custom(alwan_vec3 const *hunter_lab,
                                     alwan_vec3 *xyz,
                                     alwan_vec3 const *xyz_n) {
    /* Calculate Ka and Kb for the given illuminant */
    alwan_scalar ka, kb;
    calculate_hunter_coefficients(xyz_n, &ka, &kb);

    /* L/10 - Hunter Lab uses 0-10 scale for L */
    alwan_scalar l_norm = hunter_lab->v[0] / ALWAN_LITERAL(10.0);

    /* Y = (L/10)² × Yn */
    xyz->v[1] = l_norm * l_norm * xyz_n->v[1];

    /* X = ((a/Ka) × (L/100) + (L/100)²) × Xn */
    alwan_scalar a_term = (hunter_lab->v[1] / ka) * l_norm;
    xyz->v[0] = (a_term + l_norm * l_norm) * xyz_n->v[0];

    /* Z = -((b/Kb) × (L/100) - (L/100)²) × Zn */
    alwan_scalar b_term = (hunter_lab->v[2] / kb) * l_norm;
    xyz->v[2] = -(b_term - l_norm * l_norm) * xyz_n->v[2];
}
