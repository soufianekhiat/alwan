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
#include "alwan_math_core.h"

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
ALWAN_CONSTEXPR alwan_mat3x3 RLAB_M_HPE = {{
#include "../data/matrices/hpe.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 RLAB_M_HPE_INV = {{
#include "../data/matrices/hpe_inv.csv"
}};
ALWAN_DIAG_POP

/* RLAB reference space transformation matrix */
ALWAN_CONSTEXPR alwan_mat3x3 RLAB_M_RLAB = {{
    ALWAN_LITERAL( 1.9569), ALWAN_LITERAL(-1.1882), ALWAN_LITERAL( 0.2313),
    ALWAN_LITERAL( 0.3612), ALWAN_LITERAL( 0.6388), ALWAN_LITERAL( 0.0000),
    ALWAN_LITERAL( 0.0000), ALWAN_LITERAL( 0.0000), ALWAN_LITERAL( 1.0000)
}};

/* Inverse RLAB matrix (precomputed) */
ALWAN_CONSTEXPR alwan_mat3x3 RLAB_M_RLAB_INV = {{
    ALWAN_LITERAL( 0.4002176356618033), ALWAN_LITERAL( 0.7075374888303094), ALWAN_LITERAL(-0.0807549572842153),
    ALWAN_LITERAL( 0.2264148854595820), ALWAN_LITERAL( 1.1653895504761134), ALWAN_LITERAL(-0.0528716718777167),
    ALWAN_LITERAL( 0.0000000000000000), ALWAN_LITERAL( 0.0000000000000000), ALWAN_LITERAL( 1.0000000000000000)
}};

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

    /* Step 1: XYZ -> LMS using HPE matrix */
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 lms_v = alwan_mat3_mulv_v(RLAB_M_HPE, xyz_v);
    alwan_scalar lms_0 = lms_v.v[0]; alwan_scalar lms_1 = lms_v.v[1]; alwan_scalar lms_2 = lms_v.v[2];

    /* White point XYZ -> LMS_w */
    alwan_vec3 xyz_w_v = {{xyz_w.x, xyz_w.y, xyz_w.z}};
    alwan_vec3 lms_w_v = alwan_mat3_mulv_v(RLAB_M_HPE, xyz_w_v);
    alwan_scalar lms_w0 = lms_w_v.v[0]; alwan_scalar lms_w1 = lms_w_v.v[1]; alwan_scalar lms_w2 = lms_w_v.v[2];

    /* Step 2: Chromatic adaptation */
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

    /* Step 3: LMS_adapted -> RLAB reference space */
    alwan_vec3 lms_a_v = {{lms_a0, lms_a1, lms_a2}};
    alwan_vec3 xyz_ref_v = alwan_mat3_mulv_v(RLAB_M_RLAB, lms_a_v);
    alwan_scalar xyz_ref_0 = xyz_ref_v.v[0]; alwan_scalar xyz_ref_1 = xyz_ref_v.v[1]; alwan_scalar xyz_ref_2 = xyz_ref_v.v[2];

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

    /* Step 3: RLAB_INV matrix * xyz_ref -> lms_adapted */
    alwan_vec3 xyz_ref_v = {{xyz_ref_0, xyz_ref_1, xyz_ref_2}};
    alwan_vec3 lms_a_v = alwan_mat3_mulv_v(RLAB_M_RLAB_INV, xyz_ref_v);
    alwan_scalar lms_a0 = lms_a_v.v[0]; alwan_scalar lms_a1 = lms_a_v.v[1]; alwan_scalar lms_a2 = lms_a_v.v[2];

    /* Step 4: White point XYZ -> LMS_w */
    alwan_vec3 xyz_w_v = {{xyz_w.x, xyz_w.y, xyz_w.z}};
    alwan_vec3 lms_w_v = alwan_mat3_mulv_v(RLAB_M_HPE, xyz_w_v);
    alwan_scalar lms_w0 = lms_w_v.v[0]; alwan_scalar lms_w1 = lms_w_v.v[1]; alwan_scalar lms_w2 = lms_w_v.v[2];

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

    /* Step 6: LMS -> XYZ using HPE inverse */
    alwan_vec3 lms_v = {{lms_0, lms_1, lms_2}};
    alwan_vec3 xyz_out_v = alwan_mat3_mulv_v(RLAB_M_HPE_INV, lms_v);
    result.x = xyz_out_v.v[0]; result.y = xyz_out_v.v[1]; result.z = xyz_out_v.v[2];

    return result;
}

#endif /* ALWAN_RLAB_CORE_H */
