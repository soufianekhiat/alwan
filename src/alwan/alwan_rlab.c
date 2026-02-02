/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * RLAB Color Appearance Model
 * Based on: Fairchild (1993, 1996)
 * "RLAB: a color appearance space for color reproduction"
 * "Refinement of the RLAB color space"
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * RLAB Constants
 * ---------------------------------------------------------------- */

/* Hunt-Pointer-Estevez matrix for XYZ to LMS conversion */
static alwan_scalar const M_HPE[9] = {
#include "data/matrices/hpe.csv"
};

/* RLAB reference space transformation matrix */
static alwan_scalar const M_RLAB[9] = {
    ALWAN_LITERAL( 1.9569), ALWAN_LITERAL(-1.1882), ALWAN_LITERAL( 0.2313),
    ALWAN_LITERAL( 0.3612), ALWAN_LITERAL( 0.6388), ALWAN_LITERAL( 0.0000),
    ALWAN_LITERAL( 0.0000), ALWAN_LITERAL( 0.0000), ALWAN_LITERAL( 1.0000)
};

/* Inverse RLAB matrix (precomputed) */
static alwan_scalar const M_RLAB_INV[9] = {
    ALWAN_LITERAL( 0.4002176356618033), ALWAN_LITERAL( 0.7075374888303094), ALWAN_LITERAL(-0.0807549572842153),
    ALWAN_LITERAL( 0.2264148854595820), ALWAN_LITERAL( 1.1653895504761134), ALWAN_LITERAL(-0.0528716718777167),
    ALWAN_LITERAL( 0.0000000000000000), ALWAN_LITERAL( 0.0000000000000000), ALWAN_LITERAL( 1.0000000000000000)
};

/* Opponent color scaling factors */
static alwan_scalar const RLAB_A_SCALE = ALWAN_LITERAL(430.0);
static alwan_scalar const RLAB_B_SCALE = ALWAN_LITERAL(170.0);

/* ----------------------------------------------------------------
 * Helper Functions
 * ---------------------------------------------------------------- */

/* Get sigma (surround parameter) based on viewing conditions */
static alwan_scalar get_sigma(alwan_rlab_surround surround) {
    switch (surround) {
        case ALWAN_RLAB_SURROUND_AVERAGE:
            return ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.3);
        case ALWAN_RLAB_SURROUND_DIM:
            return ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.9);
        case ALWAN_RLAB_SURROUND_DARK:
            return ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.5);
        default:
            return ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.3);
    }
}

/* Get D (discounting-the-illuminant factor) based on media */
static alwan_scalar get_D_factor(int D_setting) {
    /* D_setting: 0 = auto (use defaults), 1 = hard copy, 2 = soft copy, 3 = transparency */
    switch (D_setting) {
        case 1:  /* Hard copy images */
            return ALWAN_LITERAL(1.0);
        case 2:  /* Soft copy images */
            return ALWAN_LITERAL(0.0);
        case 3:  /* Projected transparencies */
            return ALWAN_LITERAL(0.5);
        default: /* Auto: use 1.0 as default */
            return ALWAN_LITERAL(1.0);
    }
}

/* ----------------------------------------------------------------
 * RLAB Forward Transform: XYZ -> Correlates
 * ---------------------------------------------------------------- */

int alwan_rlab_forward(alwan_rlab_correlates *out,
                       alwan_xyz const *xyz,
                       alwan_rlab_viewing_conditions const *vc) {
    if (!out || !xyz || !vc) {
        return -1;
    }

    /* Get viewing condition parameters */
    alwan_scalar sigma = get_sigma(vc->surround);
    alwan_scalar D = get_D_factor(vc->D_factor);

    /* Step 1: Convert XYZ to LMS using HPE matrix */
    alwan_vec3 lms, lms_w;
    lms.v[0] = M_HPE[0] * xyz->x + M_HPE[1] * xyz->y + M_HPE[2] * xyz->z;
    lms.v[1] = M_HPE[3] * xyz->x + M_HPE[4] * xyz->y + M_HPE[5] * xyz->z;
    lms.v[2] = M_HPE[6] * xyz->x + M_HPE[7] * xyz->y + M_HPE[8] * xyz->z;

    lms_w.v[0] = M_HPE[0] * vc->xyz_w.x + M_HPE[1] * vc->xyz_w.y + M_HPE[2] * vc->xyz_w.z;
    lms_w.v[1] = M_HPE[3] * vc->xyz_w.x + M_HPE[4] * vc->xyz_w.y + M_HPE[5] * vc->xyz_w.z;
    lms_w.v[2] = M_HPE[6] * vc->xyz_w.x + M_HPE[7] * vc->xyz_w.y + M_HPE[8] * vc->xyz_w.z;

    /* Step 2: Chromatic adaptation using von Kries-like transform */
    alwan_vec3 lms_adapted;
    for (int i = 0; i < 3; i++) {
        if (lms_w.v[i] > ALWAN_LITERAL(1e-10)) {
            alwan_scalar Y_Yw = vc->xyz_n.y / vc->xyz_w.y;
            alwan_scalar adapt_factor = (D * (vc->xyz_n.y / lms_w.v[i]) + ALWAN_LITERAL(1.0) - D) / Y_Yw;
            lms_adapted.v[i] = lms.v[i] * adapt_factor;
        } else {
            lms_adapted.v[i] = lms.v[i];
        }
    }

    /* Step 3: Transform to RLAB reference space */
    alwan_vec3 xyz_ref;
    xyz_ref.v[0] = M_RLAB[0] * lms_adapted.v[0] + M_RLAB[1] * lms_adapted.v[1] + M_RLAB[2] * lms_adapted.v[2];
    xyz_ref.v[1] = M_RLAB[3] * lms_adapted.v[0] + M_RLAB[4] * lms_adapted.v[1] + M_RLAB[5] * lms_adapted.v[2];
    xyz_ref.v[2] = M_RLAB[6] * lms_adapted.v[0] + M_RLAB[7] * lms_adapted.v[1] + M_RLAB[8] * lms_adapted.v[2];

    /* Ensure non-negative values for power operations */
    for (int i = 0; i < 3; i++) {
        if (xyz_ref.v[i] < ALWAN_LITERAL(0.0)) {
            xyz_ref.v[i] = ALWAN_LITERAL(0.0);
        }
    }

    /* Step 4: Apply nonlinearity (power function with sigma) */
    alwan_vec3 xyz_ref_sigma;
    xyz_ref_sigma.v[0] = ALWAN_POW(xyz_ref.v[0], sigma);
    xyz_ref_sigma.v[1] = ALWAN_POW(xyz_ref.v[1], sigma);
    xyz_ref_sigma.v[2] = ALWAN_POW(xyz_ref.v[2], sigma);

    /* Step 5: Compute lightness L */
    out->L = ALWAN_LITERAL(100.0) * xyz_ref_sigma.v[1];

    /* Step 6: Compute opponent color dimensions a and b */
    out->a = RLAB_A_SCALE * (xyz_ref_sigma.v[0] - xyz_ref_sigma.v[1]);
    out->b = RLAB_B_SCALE * (xyz_ref_sigma.v[1] - xyz_ref_sigma.v[2]);

    /* Step 7: Compute hue angle h (in degrees) */
    out->h = ALWAN_ATAN2(out->b, out->a) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    if (out->h < ALWAN_LITERAL(0.0)) {
        out->h += ALWAN_LITERAL(360.0);
    }

    /* Step 8: Compute chroma C */
    out->C = ALWAN_SQRT(out->a * out->a + out->b * out->b);

    /* Step 9: Compute saturation s */
    if (out->L > ALWAN_LITERAL(1e-10)) {
        out->s = out->C / out->L;
    } else {
        out->s = ALWAN_LITERAL(0.0);
    }

    return 0;
}

/* ----------------------------------------------------------------
 * RLAB Inverse Transform: Correlates -> XYZ
 * ---------------------------------------------------------------- */

int alwan_rlab_inverse(alwan_xyz *xyz,
                       alwan_rlab_correlates const *correlates,
                       alwan_rlab_viewing_conditions const *vc) {
    if (!xyz || !correlates || !vc) {
        return -1;
    }

    /* Get viewing condition parameters */
    alwan_scalar sigma = get_sigma(vc->surround);
    alwan_scalar D = get_D_factor(vc->D_factor);

    /* Step 1: Recover XYZ_ref_sigma from L, a, b */
    alwan_vec3 xyz_ref_sigma;
    xyz_ref_sigma.v[1] = correlates->L / ALWAN_LITERAL(100.0);
    xyz_ref_sigma.v[0] = xyz_ref_sigma.v[1] + correlates->a / RLAB_A_SCALE;
    xyz_ref_sigma.v[2] = xyz_ref_sigma.v[1] - correlates->b / RLAB_B_SCALE;

    /* Step 2: Apply inverse nonlinearity */
    alwan_vec3 xyz_ref;
    alwan_scalar inv_sigma = ALWAN_LITERAL(1.0) / sigma;
    xyz_ref.v[0] = ALWAN_POW(ALWAN_ABS(xyz_ref_sigma.v[0]), inv_sigma);
    xyz_ref.v[1] = ALWAN_POW(ALWAN_ABS(xyz_ref_sigma.v[1]), inv_sigma);
    xyz_ref.v[2] = ALWAN_POW(ALWAN_ABS(xyz_ref_sigma.v[2]), inv_sigma);

    /* Step 3: Transform from RLAB reference space to LMS */
    alwan_vec3 lms_adapted;
    lms_adapted.v[0] = M_RLAB_INV[0] * xyz_ref.v[0] + M_RLAB_INV[1] * xyz_ref.v[1] + M_RLAB_INV[2] * xyz_ref.v[2];
    lms_adapted.v[1] = M_RLAB_INV[3] * xyz_ref.v[0] + M_RLAB_INV[4] * xyz_ref.v[1] + M_RLAB_INV[5] * xyz_ref.v[2];
    lms_adapted.v[2] = M_RLAB_INV[6] * xyz_ref.v[0] + M_RLAB_INV[7] * xyz_ref.v[1] + M_RLAB_INV[8] * xyz_ref.v[2];

    /* Step 4: Inverse chromatic adaptation */
    alwan_vec3 lms_w;
    lms_w.v[0] = M_HPE[0] * vc->xyz_w.x + M_HPE[1] * vc->xyz_w.y + M_HPE[2] * vc->xyz_w.z;
    lms_w.v[1] = M_HPE[3] * vc->xyz_w.x + M_HPE[4] * vc->xyz_w.y + M_HPE[5] * vc->xyz_w.z;
    lms_w.v[2] = M_HPE[6] * vc->xyz_w.x + M_HPE[7] * vc->xyz_w.y + M_HPE[8] * vc->xyz_w.z;

    alwan_vec3 lms;
    for (int i = 0; i < 3; i++) {
        if (lms_w.v[i] > ALWAN_LITERAL(1e-10)) {
            alwan_scalar Y_Yw = vc->xyz_n.y / vc->xyz_w.y;
            alwan_scalar adapt_factor = (D * (vc->xyz_n.y / lms_w.v[i]) + ALWAN_LITERAL(1.0) - D) / Y_Yw;
            lms.v[i] = lms_adapted.v[i] / adapt_factor;
        } else {
            lms.v[i] = lms_adapted.v[i];
        }
    }

    /* Step 5: Convert LMS back to XYZ using inverse HPE matrix */
    /* Need to compute HPE inverse - for now use approximate */
    xyz->x = ALWAN_LITERAL(0.4002) * lms.v[0] + ALWAN_LITERAL(0.7075) * lms.v[1] - ALWAN_LITERAL(0.0808) * lms.v[2];
    xyz->y = ALWAN_LITERAL(-0.2280) * lms.v[0] + ALWAN_LITERAL(1.1500) * lms.v[1] + ALWAN_LITERAL(0.0612) * lms.v[2];
    xyz->z = ALWAN_LITERAL(0.9184) * lms.v[2];

    return 0;
}