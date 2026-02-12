/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only RLAB Color Appearance Model
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Fairchild (1993, 1996)
 * "RLAB: a color appearance space for color reproduction"
 * "Refinement of the RLAB color space"
 *
 * The _v() functions take pre-resolved scalar parameters (sigma, D)
 * instead of enum-based viewing conditions, making them branchless
 * and cross-platform compatible.
 */

#ifndef ALWAN_RLAB_CORE_H
#define ALWAN_RLAB_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

/* ----------------------------------------------------------------
 * RLAB Correlates (value-returning variant)
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_scalar L, a, b, h, C, s;
} alwan_rlab_v_correlates;

/* ----------------------------------------------------------------
 * RLAB Constants
 * ---------------------------------------------------------------- */

/* Hunt-Pointer-Estevez matrix for XYZ -> LMS conversion (via CSV) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const RLAB_M_HPE[9] = {
#include "../data/matrices/hpe.csv"
};
static alwan_scalar const RLAB_M_HPE_INV[9] = {
#include "../data/matrices/hpe_inv.csv"
};
ALWAN_DIAG_POP

/* RLAB reference space transformation matrix */
static alwan_scalar const RLAB_M_RLAB[9] = {
    ALWAN_LITERAL( 1.9569), ALWAN_LITERAL(-1.1882), ALWAN_LITERAL( 0.2313),
    ALWAN_LITERAL( 0.3612), ALWAN_LITERAL( 0.6388), ALWAN_LITERAL( 0.0000),
    ALWAN_LITERAL( 0.0000), ALWAN_LITERAL( 0.0000), ALWAN_LITERAL( 1.0000)
};

/* Inverse RLAB matrix (precomputed) */
static alwan_scalar const RLAB_M_RLAB_INV[9] = {
    ALWAN_LITERAL( 0.4002176356618033), ALWAN_LITERAL( 0.7075374888303094), ALWAN_LITERAL(-0.0807549572842153),
    ALWAN_LITERAL( 0.2264148854595820), ALWAN_LITERAL( 1.1653895504761134), ALWAN_LITERAL(-0.0528716718777167),
    ALWAN_LITERAL( 0.0000000000000000), ALWAN_LITERAL( 0.0000000000000000), ALWAN_LITERAL( 1.0000000000000000)
};

/* ================================================================
 * RLAB Forward: XYZ -> Correlates
 *
 * sigma: surround exponent (average=1/2.3, dim=1/2.9, dark=1/3.5)
 * D:     discounting factor (hard copy=1.0, soft copy=0.0, transparency=0.5)
 * xyz_w: white point under test illuminant (Y=100)
 * xyz_n: reference white (typically D65, Y=100)
 * ================================================================ */

ALWAN_INLINE alwan_rlab_v_correlates alwan_rlab_forward_v(
    alwan_xyz xyz,
    alwan_xyz xyz_w,
    alwan_xyz xyz_n,
    alwan_scalar sigma,
    alwan_scalar D)
{
    alwan_rlab_v_correlates result;

    /* Step 1: XYZ -> LMS using HPE matrix (unrolled 3x) */
    alwan_scalar lms_0 = RLAB_M_HPE[0] * xyz.x + RLAB_M_HPE[1] * xyz.y + RLAB_M_HPE[2] * xyz.z;
    alwan_scalar lms_1 = RLAB_M_HPE[3] * xyz.x + RLAB_M_HPE[4] * xyz.y + RLAB_M_HPE[5] * xyz.z;
    alwan_scalar lms_2 = RLAB_M_HPE[6] * xyz.x + RLAB_M_HPE[7] * xyz.y + RLAB_M_HPE[8] * xyz.z;

    /* White point XYZ -> LMS_w (unrolled 3x) */
    alwan_scalar lms_w0 = RLAB_M_HPE[0] * xyz_w.x + RLAB_M_HPE[1] * xyz_w.y + RLAB_M_HPE[2] * xyz_w.z;
    alwan_scalar lms_w1 = RLAB_M_HPE[3] * xyz_w.x + RLAB_M_HPE[4] * xyz_w.y + RLAB_M_HPE[5] * xyz_w.z;
    alwan_scalar lms_w2 = RLAB_M_HPE[6] * xyz_w.x + RLAB_M_HPE[7] * xyz_w.y + RLAB_M_HPE[8] * xyz_w.z;

    /* Step 2: Chromatic adaptation (unrolled 3 channels) */
    alwan_scalar Y_Yw = xyz_n.y / xyz_w.y;

    alwan_scalar adapt_0 = ALWAN_SELECT(lms_w0 > ALWAN_LITERAL(1e-10),
        (D * (xyz_n.y / lms_w0) + ALWAN_LITERAL(1.0) - D) / Y_Yw,
        ALWAN_LITERAL(1.0));
    alwan_scalar lms_a0 = lms_0 * adapt_0;

    alwan_scalar adapt_1 = ALWAN_SELECT(lms_w1 > ALWAN_LITERAL(1e-10),
        (D * (xyz_n.y / lms_w1) + ALWAN_LITERAL(1.0) - D) / Y_Yw,
        ALWAN_LITERAL(1.0));
    alwan_scalar lms_a1 = lms_1 * adapt_1;

    alwan_scalar adapt_2 = ALWAN_SELECT(lms_w2 > ALWAN_LITERAL(1e-10),
        (D * (xyz_n.y / lms_w2) + ALWAN_LITERAL(1.0) - D) / Y_Yw,
        ALWAN_LITERAL(1.0));
    alwan_scalar lms_a2 = lms_2 * adapt_2;

    /* Step 3: LMS_adapted -> RLAB reference space (M_RLAB matrix multiply, unrolled) */
    alwan_scalar xyz_ref_0 = RLAB_M_RLAB[0] * lms_a0 + RLAB_M_RLAB[1] * lms_a1 + RLAB_M_RLAB[2] * lms_a2;
    alwan_scalar xyz_ref_1 = RLAB_M_RLAB[3] * lms_a0 + RLAB_M_RLAB[4] * lms_a1 + RLAB_M_RLAB[5] * lms_a2;
    alwan_scalar xyz_ref_2 = RLAB_M_RLAB[6] * lms_a0 + RLAB_M_RLAB[7] * lms_a1 + RLAB_M_RLAB[8] * lms_a2;

    /* Clamp negatives (unrolled 3x) */
    xyz_ref_0 = ALWAN_SELECT(xyz_ref_0 < ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), xyz_ref_0);
    xyz_ref_1 = ALWAN_SELECT(xyz_ref_1 < ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), xyz_ref_1);
    xyz_ref_2 = ALWAN_SELECT(xyz_ref_2 < ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), xyz_ref_2);

    /* Step 4: Apply power nonlinearity */
    alwan_scalar xyz_ref_sigma_0 = ALWAN_POW(xyz_ref_0, sigma);
    alwan_scalar xyz_ref_sigma_1 = ALWAN_POW(xyz_ref_1, sigma);
    alwan_scalar xyz_ref_sigma_2 = ALWAN_POW(xyz_ref_2, sigma);

    /* Step 5: Compute L */
    result.L = ALWAN_LITERAL(100.0) * xyz_ref_sigma_1;

    /* Step 6: Compute opponent dimensions a, b */
    result.a = ALWAN_LITERAL(430.0) * (xyz_ref_sigma_0 - xyz_ref_sigma_1);
    result.b = ALWAN_LITERAL(170.0) * (xyz_ref_sigma_1 - xyz_ref_sigma_2);

    /* Step 7: Hue angle h (degrees, [0, 360)) */
    alwan_scalar h_deg = ALWAN_ATAN2(result.b, result.a) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    result.h = ALWAN_SELECT(h_deg < ALWAN_LITERAL(0.0), h_deg + ALWAN_LITERAL(360.0), h_deg);

    /* Step 8: Chroma C */
    result.C = ALWAN_SQRT(result.a * result.a + result.b * result.b);

    /* Step 9: Saturation s */
    result.s = ALWAN_SELECT(result.L > ALWAN_LITERAL(1e-10), result.C / result.L, ALWAN_LITERAL(0.0));

    return result;
}

/* ================================================================
 * RLAB Inverse: Correlates -> XYZ
 *
 * sigma: surround exponent (average=1/2.3, dim=1/2.9, dark=1/3.5)
 * D:     discounting factor (hard copy=1.0, soft copy=0.0, transparency=0.5)
 * xyz_w: white point under test illuminant (Y=100)
 * xyz_n: reference white (typically D65, Y=100)
 * ================================================================ */

ALWAN_INLINE alwan_xyz alwan_rlab_inverse_v(
    alwan_rlab_v_correlates correlates,
    alwan_xyz xyz_w,
    alwan_xyz xyz_n,
    alwan_scalar sigma,
    alwan_scalar D)
{
    alwan_xyz result;

    /* Step 1: Recover xyz_ref_sigma from L, a, b */
    alwan_scalar xyz_ref_sigma_1 = correlates.L / ALWAN_LITERAL(100.0);
    alwan_scalar xyz_ref_sigma_0 = xyz_ref_sigma_1 + correlates.a / ALWAN_LITERAL(430.0);
    alwan_scalar xyz_ref_sigma_2 = xyz_ref_sigma_1 - correlates.b / ALWAN_LITERAL(170.0);

    /* Step 2: Apply inverse power nonlinearity */
    alwan_scalar inv_sigma = ALWAN_LITERAL(1.0) / sigma;
    alwan_scalar xyz_ref_0 = ALWAN_POW(ALWAN_ABS(xyz_ref_sigma_0), inv_sigma);
    alwan_scalar xyz_ref_1 = ALWAN_POW(ALWAN_ABS(xyz_ref_sigma_1), inv_sigma);
    alwan_scalar xyz_ref_2 = ALWAN_POW(ALWAN_ABS(xyz_ref_sigma_2), inv_sigma);

    /* Step 3: RLAB_INV matrix * xyz_ref -> lms_adapted (unrolled) */
    alwan_scalar lms_a0 = RLAB_M_RLAB_INV[0] * xyz_ref_0 + RLAB_M_RLAB_INV[1] * xyz_ref_1 + RLAB_M_RLAB_INV[2] * xyz_ref_2;
    alwan_scalar lms_a1 = RLAB_M_RLAB_INV[3] * xyz_ref_0 + RLAB_M_RLAB_INV[4] * xyz_ref_1 + RLAB_M_RLAB_INV[5] * xyz_ref_2;
    alwan_scalar lms_a2 = RLAB_M_RLAB_INV[6] * xyz_ref_0 + RLAB_M_RLAB_INV[7] * xyz_ref_1 + RLAB_M_RLAB_INV[8] * xyz_ref_2;

    /* Step 4: White point XYZ -> LMS_w (unrolled 3x) */
    alwan_scalar lms_w0 = RLAB_M_HPE[0] * xyz_w.x + RLAB_M_HPE[1] * xyz_w.y + RLAB_M_HPE[2] * xyz_w.z;
    alwan_scalar lms_w1 = RLAB_M_HPE[3] * xyz_w.x + RLAB_M_HPE[4] * xyz_w.y + RLAB_M_HPE[5] * xyz_w.z;
    alwan_scalar lms_w2 = RLAB_M_HPE[6] * xyz_w.x + RLAB_M_HPE[7] * xyz_w.y + RLAB_M_HPE[8] * xyz_w.z;

    /* Step 5: Inverse chromatic adaptation (unrolled 3 channels) */
    alwan_scalar Y_Yw = xyz_n.y / xyz_w.y;

    alwan_scalar adapt_0 = ALWAN_SELECT(lms_w0 > ALWAN_LITERAL(1e-10),
        (D * (xyz_n.y / lms_w0) + ALWAN_LITERAL(1.0) - D) / Y_Yw,
        ALWAN_LITERAL(1.0));
    alwan_scalar lms_0 = lms_a0 / adapt_0;

    alwan_scalar adapt_1 = ALWAN_SELECT(lms_w1 > ALWAN_LITERAL(1e-10),
        (D * (xyz_n.y / lms_w1) + ALWAN_LITERAL(1.0) - D) / Y_Yw,
        ALWAN_LITERAL(1.0));
    alwan_scalar lms_1 = lms_a1 / adapt_1;

    alwan_scalar adapt_2 = ALWAN_SELECT(lms_w2 > ALWAN_LITERAL(1e-10),
        (D * (xyz_n.y / lms_w2) + ALWAN_LITERAL(1.0) - D) / Y_Yw,
        ALWAN_LITERAL(1.0));
    alwan_scalar lms_2 = lms_a2 / adapt_2;

    /* Step 6: LMS -> XYZ using HPE inverse (via CSV) */
    result.x = RLAB_M_HPE_INV[0] * lms_0 + RLAB_M_HPE_INV[1] * lms_1 + RLAB_M_HPE_INV[2] * lms_2;
    result.y = RLAB_M_HPE_INV[3] * lms_0 + RLAB_M_HPE_INV[4] * lms_1 + RLAB_M_HPE_INV[5] * lms_2;
    result.z = RLAB_M_HPE_INV[6] * lms_0 + RLAB_M_HPE_INV[7] * lms_1 + RLAB_M_HPE_INV[8] * lms_2;

    return result;
}

#endif /* ALWAN_RLAB_CORE_H */
