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

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_rlab_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_rlab_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

typedef struct {
    alwan_scalar L, a, b, h, C, s;
} alwan_rlab_v_correlates;

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 RLAB_M_HPE = {{
#include "../data/matrices/hpe.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 RLAB_M_HPE_INV = {{
#include "../data/matrices/hpe_inv.csv"
}};
ALWAN_DIAG_POP

ALWAN_CONSTEXPR alwan_mat3x3 RLAB_M_RLAB = {{
#include "../data/matrices/rlab_m.csv"
}};

ALWAN_CONSTEXPR alwan_mat3x3 RLAB_M_RLAB_INV = {{
#include "../data/matrices/rlab_m_inv.csv"
}};

ALWAN_INLINE alwan_rlab_v_correlates alwan_rlab_forward_v(
    alwan_xyz xyz, alwan_xyz xyz_w, alwan_xyz xyz_n,
    alwan_scalar sigma, alwan_scalar D) {
    alwan_rlab_v_correlates result;
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 lms_v = alwan_mat3_mulv_v(RLAB_M_HPE, xyz_v);
    alwan_scalar lms_0 = lms_v.v[0]; alwan_scalar lms_1 = lms_v.v[1]; alwan_scalar lms_2 = lms_v.v[2];
    alwan_vec3 xyz_w_v = {{xyz_w.x, xyz_w.y, xyz_w.z}};
    alwan_vec3 lms_w_v = alwan_mat3_mulv_v(RLAB_M_HPE, xyz_w_v);
    alwan_scalar lms_w0 = lms_w_v.v[0]; alwan_scalar lms_w1 = lms_w_v.v[1]; alwan_scalar lms_w2 = lms_w_v.v[2];
    /* RLAB adaptation: LMS_a = (p + D*(1-p)) / LMS_n, with the incomplete
     * adaptation term p from l_E and the absolute adapting luminance.
     * See the .inc twin for why Y_n is a fixed 318.31 here. */
    alwan_scalar const Y_n_default = ALWAN_LITERAL(318.31);
    alwan_scalar const y_n_cbrt = ALWAN_POW(Y_n_default, ALWAN_ONE / ALWAN_LITERAL(3.0));
    alwan_scalar const lms_w_sum = lms_w0 + lms_w1 + lms_w2;
    alwan_scalar const lms_w_sum_safe = ALWAN_SELECT(ALWAN_ABS(lms_w_sum) > ALWAN_LITERAL(1e-10), lms_w_sum, ALWAN_ONE);
    alwan_scalar const l_E0 = ALWAN_LITERAL(3.0) * lms_w0 / lms_w_sum_safe;
    alwan_scalar const l_E1 = ALWAN_LITERAL(3.0) * lms_w1 / lms_w_sum_safe;
    alwan_scalar const l_E2 = ALWAN_LITERAL(3.0) * lms_w2 / lms_w_sum_safe;
    alwan_scalar const p0 = (ALWAN_ONE + y_n_cbrt + l_E0) / (ALWAN_ONE + y_n_cbrt + ALWAN_ONE / l_E0);
    alwan_scalar const p1 = (ALWAN_ONE + y_n_cbrt + l_E1) / (ALWAN_ONE + y_n_cbrt + ALWAN_ONE / l_E1);
    alwan_scalar const p2 = (ALWAN_ONE + y_n_cbrt + l_E2) / (ALWAN_ONE + y_n_cbrt + ALWAN_ONE / l_E2);
    alwan_scalar adapt_0 = ALWAN_SELECT(lms_w0 > ALWAN_LITERAL(1e-10), (p0 + D * (ALWAN_ONE - p0)) / lms_w0, ALWAN_LITERAL(1.0));
    alwan_scalar lms_a0 = lms_0 * adapt_0;
    alwan_scalar adapt_1 = ALWAN_SELECT(lms_w1 > ALWAN_LITERAL(1e-10), (p1 + D * (ALWAN_ONE - p1)) / lms_w1, ALWAN_LITERAL(1.0));
    alwan_scalar lms_a1 = lms_1 * adapt_1;
    alwan_scalar adapt_2 = ALWAN_SELECT(lms_w2 > ALWAN_LITERAL(1e-10), (p2 + D * (ALWAN_ONE - p2)) / lms_w2, ALWAN_LITERAL(1.0));
    alwan_scalar lms_a2 = lms_2 * adapt_2;
    ALWAN_UNUSED(xyz_n);
    alwan_vec3 lms_a_v = {{lms_a0, lms_a1, lms_a2}};
    alwan_vec3 xyz_ref_v = alwan_mat3_mulv_v(RLAB_M_RLAB, lms_a_v);
    alwan_scalar xyz_ref_0 = xyz_ref_v.v[0]; alwan_scalar xyz_ref_1 = xyz_ref_v.v[1]; alwan_scalar xyz_ref_2 = xyz_ref_v.v[2];
    xyz_ref_0 = ALWAN_SELECT(xyz_ref_0 < ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), xyz_ref_0);
    xyz_ref_1 = ALWAN_SELECT(xyz_ref_1 < ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), xyz_ref_1);
    xyz_ref_2 = ALWAN_SELECT(xyz_ref_2 < ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), xyz_ref_2);
    alwan_scalar xyz_ref_sigma_0 = ALWAN_POW(xyz_ref_0, sigma);
    alwan_scalar xyz_ref_sigma_1 = ALWAN_POW(xyz_ref_1, sigma);
    alwan_scalar xyz_ref_sigma_2 = ALWAN_POW(xyz_ref_2, sigma);
    result.L = ALWAN_LITERAL(100.0) * xyz_ref_sigma_1;
    result.a = ALWAN_LITERAL(430.0) * (xyz_ref_sigma_0 - xyz_ref_sigma_1);
    result.b = ALWAN_LITERAL(170.0) * (xyz_ref_sigma_1 - xyz_ref_sigma_2);
    alwan_scalar h_deg = ALWAN_ATAN2(result.b, result.a) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    result.h = ALWAN_SELECT(h_deg < ALWAN_LITERAL(0.0), h_deg + ALWAN_LITERAL(360.0), h_deg);
    result.C = ALWAN_SQRT(result.a * result.a + result.b * result.b);
    result.s = ALWAN_SELECT(result.L > ALWAN_LITERAL(1e-10), result.C / result.L, ALWAN_LITERAL(0.0));
    return result;
}

ALWAN_INLINE alwan_xyz alwan_rlab_inverse_v(
    alwan_rlab_v_correlates correlates, alwan_xyz xyz_w, alwan_xyz xyz_n,
    alwan_scalar sigma, alwan_scalar D) {
    alwan_xyz result;
    alwan_scalar xyz_ref_sigma_1 = correlates.L / ALWAN_LITERAL(100.0);
    alwan_scalar xyz_ref_sigma_0 = xyz_ref_sigma_1 + correlates.a / ALWAN_LITERAL(430.0);
    alwan_scalar xyz_ref_sigma_2 = xyz_ref_sigma_1 - correlates.b / ALWAN_LITERAL(170.0);
    alwan_scalar inv_sigma = ALWAN_LITERAL(1.0) / sigma;
    alwan_scalar xyz_ref_0 = ALWAN_POW(ALWAN_ABS(xyz_ref_sigma_0), inv_sigma);
    alwan_scalar xyz_ref_1 = ALWAN_POW(ALWAN_ABS(xyz_ref_sigma_1), inv_sigma);
    alwan_scalar xyz_ref_2 = ALWAN_POW(ALWAN_ABS(xyz_ref_sigma_2), inv_sigma);
    alwan_vec3 xyz_ref_v = {{xyz_ref_0, xyz_ref_1, xyz_ref_2}};
    alwan_vec3 lms_a_v = alwan_mat3_mulv_v(RLAB_M_RLAB_INV, xyz_ref_v);
    alwan_scalar lms_a0 = lms_a_v.v[0]; alwan_scalar lms_a1 = lms_a_v.v[1]; alwan_scalar lms_a2 = lms_a_v.v[2];
    alwan_vec3 xyz_w_v = {{xyz_w.x, xyz_w.y, xyz_w.z}};
    alwan_vec3 lms_w_v = alwan_mat3_mulv_v(RLAB_M_HPE, xyz_w_v);
    alwan_scalar lms_w0 = lms_w_v.v[0]; alwan_scalar lms_w1 = lms_w_v.v[1]; alwan_scalar lms_w2 = lms_w_v.v[2];
    /* Exact inverse of the forward's adaptation; see the .inc twin. */
    alwan_scalar const Y_n_default = ALWAN_LITERAL(318.31);
    alwan_scalar const y_n_cbrt = ALWAN_POW(Y_n_default, ALWAN_ONE / ALWAN_LITERAL(3.0));
    alwan_scalar const lms_w_sum = lms_w0 + lms_w1 + lms_w2;
    alwan_scalar const lms_w_sum_safe = ALWAN_SELECT(ALWAN_ABS(lms_w_sum) > ALWAN_LITERAL(1e-10), lms_w_sum, ALWAN_ONE);
    alwan_scalar const l_E0 = ALWAN_LITERAL(3.0) * lms_w0 / lms_w_sum_safe;
    alwan_scalar const l_E1 = ALWAN_LITERAL(3.0) * lms_w1 / lms_w_sum_safe;
    alwan_scalar const l_E2 = ALWAN_LITERAL(3.0) * lms_w2 / lms_w_sum_safe;
    alwan_scalar const p0 = (ALWAN_ONE + y_n_cbrt + l_E0) / (ALWAN_ONE + y_n_cbrt + ALWAN_ONE / l_E0);
    alwan_scalar const p1 = (ALWAN_ONE + y_n_cbrt + l_E1) / (ALWAN_ONE + y_n_cbrt + ALWAN_ONE / l_E1);
    alwan_scalar const p2 = (ALWAN_ONE + y_n_cbrt + l_E2) / (ALWAN_ONE + y_n_cbrt + ALWAN_ONE / l_E2);
    alwan_scalar adapt_0 = ALWAN_SELECT(lms_w0 > ALWAN_LITERAL(1e-10), (p0 + D * (ALWAN_ONE - p0)) / lms_w0, ALWAN_LITERAL(1.0));
    alwan_scalar lms_0 = lms_a0 / adapt_0;
    alwan_scalar adapt_1 = ALWAN_SELECT(lms_w1 > ALWAN_LITERAL(1e-10), (p1 + D * (ALWAN_ONE - p1)) / lms_w1, ALWAN_LITERAL(1.0));
    alwan_scalar lms_1 = lms_a1 / adapt_1;
    alwan_scalar adapt_2 = ALWAN_SELECT(lms_w2 > ALWAN_LITERAL(1e-10), (p2 + D * (ALWAN_ONE - p2)) / lms_w2, ALWAN_LITERAL(1.0));
    alwan_scalar lms_2 = lms_a2 / adapt_2;
    ALWAN_UNUSED(xyz_n);
    alwan_vec3 lms_v = {{lms_0, lms_1, lms_2}};
    alwan_vec3 xyz_out_v = alwan_mat3_mulv_v(RLAB_M_HPE_INV, lms_v);
    result.x = xyz_out_v.v[0]; result.y = xyz_out_v.v[1]; result.z = xyz_out_v.v[2];
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_RLAB_CORE_H */
