/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * P1.5: OSA-UCS Color Space (Optical Society of America Uniform Color Scales)
 *
 * Reference: OSA Uniform Color Scales Committee (1977)
 * "Optical Society of America Uniform Color Scales"
 * https://en.wikipedia.org/wiki/OSA-UCS
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ----------------------------------------------------------------
 * OSA-UCS Constants
 * ---------------------------------------------------------------- */

/* XYZ to RGB transformation matrix */
static alwan_scalar const XYZ_TO_RGB_OSA[9] = {
    ALWAN_LITERAL( 0.7990),   ALWAN_LITERAL( 0.4194),  ALWAN_LITERAL(-0.1648),
    ALWAN_LITERAL(-0.4493),   ALWAN_LITERAL( 1.3265),  ALWAN_LITERAL( 0.0927),
    ALWAN_LITERAL(-0.1149),   ALWAN_LITERAL( 0.3394),  ALWAN_LITERAL( 0.7170)
};

/* RGB to XYZ inverse matrix (precomputed) */
static alwan_scalar const RGB_TO_XYZ_OSA[9] = {
    ALWAN_LITERAL( 1.2671437106783715),   ALWAN_LITERAL(-0.3952805953175470),  ALWAN_LITERAL( 0.2076207506990453),
    ALWAN_LITERAL( 0.4270169237547476),   ALWAN_LITERAL( 0.7698457624019271),  ALWAN_LITERAL(-0.0557814606555453),
    ALWAN_LITERAL( 0.0000000000000000),   ALWAN_LITERAL(-0.1414598390909057),  ALWAN_LITERAL( 1.4194042205296260)
};

/* LMS to OSA-UCS transformation coefficients */
static alwan_scalar const LMS_TO_OSA_L[3] = {
    ALWAN_LITERAL(1.7),  ALWAN_LITERAL(8.0),  ALWAN_LITERAL(-9.7)
};

static alwan_scalar const LMS_TO_OSA_J[3] = {
    ALWAN_LITERAL(-13.7), ALWAN_LITERAL(17.7), ALWAN_LITERAL(-4.0)
};

/* ----------------------------------------------------------------
 * Helper Functions
 * ---------------------------------------------------------------- */

/* Calculate Y0 from xyY */
static alwan_scalar calculate_Y0(alwan_scalar x, alwan_scalar y, alwan_scalar Y) {
    alwan_scalar k = ALWAN_LITERAL(4.4934) * x * x +
                     ALWAN_LITERAL(4.3034) * y * y -
                     ALWAN_LITERAL(4.276) * x * y -
                     ALWAN_LITERAL(1.3744) * x -
                     ALWAN_LITERAL(2.5643) * y +
                     ALWAN_LITERAL(1.8103);
    return Y * k;
}

/* Solve cubic equation using Cardano's formula for Lambda calculation */
static alwan_scalar solve_cubic_for_lambda(alwan_scalar L_osa) {
    /* Lambda = L * sqrt(2) + 14.4 (simplified approach) */
    return L_osa * ALWAN_SQRT(ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(14.4);
}

/* ----------------------------------------------------------------
 * XYZ <-> OSA-UCS
 * ---------------------------------------------------------------- */

void alwan_xyz_to_osa_ucs(alwan_vec3 const *xyz, alwan_vec3 *osa_ucs) {
    /* Normalize XYZ from Y=100 scale to Y=1 scale */
    alwan_vec3 xyz_norm;
    xyz_norm.v[0] = xyz->v[0] / ALWAN_LITERAL(100.0);
    xyz_norm.v[1] = xyz->v[1] / ALWAN_LITERAL(100.0);
    xyz_norm.v[2] = xyz->v[2] / ALWAN_LITERAL(100.0);

    /* Step 1: Convert XYZ to xyY */
    alwan_scalar sum = xyz_norm.v[0] + xyz_norm.v[1] + xyz_norm.v[2];
    if (sum < ALWAN_LITERAL(1e-10)) {
        /* Black point */
        osa_ucs->v[0] = ALWAN_LITERAL(0.0);  /* L */
        osa_ucs->v[1] = ALWAN_LITERAL(0.0);  /* j */
        osa_ucs->v[2] = ALWAN_LITERAL(0.0);  /* g */
        return;
    }

    alwan_scalar x = xyz_norm.v[0] / sum;
    alwan_scalar y = xyz_norm.v[1] / sum;
    alwan_scalar Y = xyz_norm.v[1];

    /* Step 2: Calculate Y0 (luminance factor) */
    alwan_scalar Y0 = calculate_Y0(x, y, Y);
    if (Y0 < ALWAN_LITERAL(0.0)) Y0 = ALWAN_LITERAL(0.0);

    alwan_scalar Y0_cbrt = ALWAN_POW(Y0, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    /* Step 3: Calculate Lambda */
    alwan_scalar Y0_minus_30 = Y0 - ALWAN_LITERAL(30.0);
    alwan_scalar Y0_minus_30_cbrt;
    if (Y0_minus_30 >= ALWAN_LITERAL(0.0)) {
        Y0_minus_30_cbrt = ALWAN_POW(Y0_minus_30, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
    } else {
        Y0_minus_30_cbrt = -ALWAN_POW(-Y0_minus_30, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
    }
    alwan_scalar lambda = ALWAN_LITERAL(5.9) * (Y0_cbrt - ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) +
                          ALWAN_LITERAL(0.042) * Y0_minus_30_cbrt;

    /* Step 4: Transform XYZ to RGB */
    alwan_vec3 rgb;
    rgb.v[0] = XYZ_TO_RGB_OSA[0] * xyz_norm.v[0] + XYZ_TO_RGB_OSA[1] * xyz_norm.v[1] + XYZ_TO_RGB_OSA[2] * xyz_norm.v[2];
    rgb.v[1] = XYZ_TO_RGB_OSA[3] * xyz_norm.v[0] + XYZ_TO_RGB_OSA[4] * xyz_norm.v[1] + XYZ_TO_RGB_OSA[5] * xyz_norm.v[2];
    rgb.v[2] = XYZ_TO_RGB_OSA[6] * xyz_norm.v[0] + XYZ_TO_RGB_OSA[7] * xyz_norm.v[1] + XYZ_TO_RGB_OSA[8] * xyz_norm.v[2];

    /* Step 5: Calculate RGB cube roots */
    alwan_vec3 rgb_cbrt;
    for (int i = 0; i < 3; i++) {
        if (rgb.v[i] >= ALWAN_LITERAL(0.0)) {
            rgb_cbrt.v[i] = ALWAN_POW(rgb.v[i], ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
        } else {
            rgb_cbrt.v[i] = -ALWAN_POW(-rgb.v[i], ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
        }
    }

    /* Step 6: Calculate chroma coefficient C */
    alwan_scalar C;
    if (Y0_cbrt > ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) {
        C = lambda / (ALWAN_LITERAL(5.9) * (Y0_cbrt - ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)));
    } else {
        C = ALWAN_LITERAL(1.0);  /* Default for very dark colors */
    }

    /* Step 7: Calculate OSA-UCS coordinates */
    /* L = (lambda - 14.4) / sqrt(2) */
    osa_ucs->v[0] = (lambda - ALWAN_LITERAL(14.4)) / ALWAN_SQRT(ALWAN_LITERAL(2.0));

    /* j = C × (1.7×R' + 8.0×G' - 9.7×B') */
    osa_ucs->v[1] = C * (LMS_TO_OSA_L[0] * rgb_cbrt.v[0] +
                         LMS_TO_OSA_L[1] * rgb_cbrt.v[1] +
                         LMS_TO_OSA_L[2] * rgb_cbrt.v[2]);

    /* g = C × (-13.7×R' + 17.7×G' - 4.0×B') */
    osa_ucs->v[2] = C * (LMS_TO_OSA_J[0] * rgb_cbrt.v[0] +
                         LMS_TO_OSA_J[1] * rgb_cbrt.v[1] +
                         LMS_TO_OSA_J[2] * rgb_cbrt.v[2]);
}

void alwan_osa_ucs_to_xyz(alwan_vec3 const *osa_ucs, alwan_vec3 *xyz) {
    /* OSA-UCS to XYZ requires iterative solution (Newton-Raphson)
     * This is a simplified implementation that provides approximate inverse
     * For high precision, use numerical optimization */

    /* Step 1: Calculate Lambda from L */
    alwan_scalar lambda = solve_cubic_for_lambda(osa_ucs->v[0]);

    /* Step 2: Calculate Y0_cbrt from lightness */
    alwan_scalar Y0_cbrt = (lambda / ALWAN_LITERAL(5.9)) + ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0);

    /* Step 3: Calculate chroma coefficient C */
    alwan_scalar C;
    if (Y0_cbrt > ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) {
        C = lambda / (ALWAN_LITERAL(5.9) * (Y0_cbrt - ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)));
    } else {
        C = ALWAN_LITERAL(1.0);
    }

    /* Guard against division by zero */
    if (ALWAN_FABS(C) < ALWAN_LITERAL(1e-10)) {
        xyz->v[0] = ALWAN_LITERAL(0.0);
        xyz->v[1] = ALWAN_LITERAL(0.0);
        xyz->v[2] = ALWAN_LITERAL(0.0);
        return;
    }

    /* Step 4: Recover RGB' (cube roots) from j and g
     * This requires solving a linear system:
     * j = C × (1.7×R' + 8.0×G' - 9.7×B')
     * g = C × (-13.7×R' + 17.7×G' - 4.0×B')
     *
     * We need a third equation. Use approximation based on lightness:
     * R' + G' + B' ≈ 3 × Y0_cbrt
     */
    alwan_scalar j_norm = osa_ucs->v[1] / C;
    alwan_scalar g_norm = osa_ucs->v[2] / C;

    /* Simplified inverse (approximate solution) */
    /* This uses a pseudo-inverse approach */
    alwan_vec3 rgb_cbrt;
    rgb_cbrt.v[0] = Y0_cbrt + ALWAN_LITERAL(0.01) * j_norm - ALWAN_LITERAL(0.02) * g_norm;
    rgb_cbrt.v[1] = Y0_cbrt + ALWAN_LITERAL(0.12) * j_norm + ALWAN_LITERAL(0.06) * g_norm;
    rgb_cbrt.v[2] = Y0_cbrt - ALWAN_LITERAL(0.10) * j_norm - ALWAN_LITERAL(0.05) * g_norm;

    /* Step 5: Calculate RGB from RGB' */
    alwan_vec3 rgb;
    for (int i = 0; i < 3; i++) {
        if (rgb_cbrt.v[i] >= ALWAN_LITERAL(0.0)) {
            rgb.v[i] = rgb_cbrt.v[i] * rgb_cbrt.v[i] * rgb_cbrt.v[i];
        } else {
            alwan_scalar abs_val = -rgb_cbrt.v[i];
            rgb.v[i] = -(abs_val * abs_val * abs_val);
        }
    }

    /* Step 6: Transform RGB to XYZ */
    alwan_vec3 xyz_norm;
    xyz_norm.v[0] = RGB_TO_XYZ_OSA[0] * rgb.v[0] + RGB_TO_XYZ_OSA[1] * rgb.v[1] + RGB_TO_XYZ_OSA[2] * rgb.v[2];
    xyz_norm.v[1] = RGB_TO_XYZ_OSA[3] * rgb.v[0] + RGB_TO_XYZ_OSA[4] * rgb.v[1] + RGB_TO_XYZ_OSA[5] * rgb.v[2];
    xyz_norm.v[2] = RGB_TO_XYZ_OSA[6] * rgb.v[0] + RGB_TO_XYZ_OSA[7] * rgb.v[1] + RGB_TO_XYZ_OSA[8] * rgb.v[2];

    /* Clamp negative values to zero */
    for (int i = 0; i < 3; i++) {
        if (xyz_norm.v[i] < ALWAN_LITERAL(0.0)) {
            xyz_norm.v[i] = ALWAN_LITERAL(0.0);
        }
    }

    /* Scale back to Y=100 */
    xyz->v[0] = xyz_norm.v[0] * ALWAN_LITERAL(100.0);
    xyz->v[1] = xyz_norm.v[1] * ALWAN_LITERAL(100.0);
    xyz->v[2] = xyz_norm.v[2] * ALWAN_LITERAL(100.0);
}
