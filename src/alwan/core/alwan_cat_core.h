/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Chromatic Adaptation Transform (CAT) core math
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * The _v() functions take pre-resolved CAT matrices (M, M_inv) as
 * parameters rather than an enum. The .c wrapper resolves the enum
 * to matrices and calls these functions.
 */

#ifndef ALWAN_CAT_CORE_H
#define ALWAN_CAT_CORE_H

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
#include "alwan_cat_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_cat_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_INLINE alwan_mat3x3 alwan_cat_xyz_scaling_v(alwan_xyz src_white, alwan_xyz dst_white) {
    alwan_mat3x3 result;
    alwan_scalar sx = dst_white.x / src_white.x;
    alwan_scalar sy = dst_white.y / src_white.y;
    alwan_scalar sz = dst_white.z / src_white.z;

    result.m[0] = sx;         result.m[1] = ALWAN_ZERO; result.m[2] = ALWAN_ZERO;
    result.m[3] = ALWAN_ZERO; result.m[4] = sy;         result.m[5] = ALWAN_ZERO;
    result.m[6] = ALWAN_ZERO; result.m[7] = ALWAN_ZERO; result.m[8] = sz;
    return result;
}

ALWAN_INLINE alwan_mat3x3 alwan_cat_matrix_v(
    alwan_mat3x3 M,
    alwan_mat3x3 M_inv,
    alwan_xyz src_white,
    alwan_xyz dst_white)
{
    alwan_vec3 vec_src;
    vec_src.v[0] = src_white.x;
    vec_src.v[1] = src_white.y;
    vec_src.v[2] = src_white.z;
    alwan_vec3 rgb_src = alwan_mat3_mulv_v(M, vec_src);

    alwan_vec3 vec_dst;
    vec_dst.v[0] = dst_white.x;
    vec_dst.v[1] = dst_white.y;
    vec_dst.v[2] = dst_white.z;
    alwan_vec3 rgb_dst = alwan_mat3_mulv_v(M, vec_dst);

    alwan_mat3x3 D;
    D.m[0] = rgb_dst.v[0] / rgb_src.v[0];
    D.m[1] = ALWAN_ZERO; D.m[2] = ALWAN_ZERO;
    D.m[3] = ALWAN_ZERO;
    D.m[4] = rgb_dst.v[1] / rgb_src.v[1];
    D.m[5] = ALWAN_ZERO;
    D.m[6] = ALWAN_ZERO; D.m[7] = ALWAN_ZERO;
    D.m[8] = rgb_dst.v[2] / rgb_src.v[2];

    alwan_mat3x3 DM = alwan_mat3_mul_v(D, M);
    return alwan_mat3_mul_v(M_inv, DM);
}

ALWAN_INLINE alwan_xyz alwan_cat_adapt_v(alwan_mat3x3 cat_mat, alwan_xyz xyz_in) {
    alwan_vec3 v;
    v.v[0] = xyz_in.x;
    v.v[1] = xyz_in.y;
    v.v[2] = xyz_in.z;
    alwan_vec3 r = alwan_mat3_mulv_v(cat_mat, v);
    alwan_xyz result;
    result.x = r.v[0];
    result.y = r.v[1];
    result.z = r.v[2];
    return result;
}

ALWAN_INLINE alwan_xyz alwan_cat_zhai2018_v(
    alwan_mat3x3 M,
    alwan_mat3x3 M_inv,
    alwan_xyz xyz_in,
    alwan_xyz xyz_src,
    alwan_xyz xyz_dst,
    alwan_scalar D_src,
    alwan_scalar D_dst,
    alwan_xyz xyz_baseline)
{
    alwan_vec3 v_in;
    v_in.v[0] = xyz_in.x; v_in.v[1] = xyz_in.y; v_in.v[2] = xyz_in.z;
    alwan_vec3 rgb_in = alwan_mat3_mulv_v(M, v_in);

    alwan_vec3 v_src;
    v_src.v[0] = xyz_src.x; v_src.v[1] = xyz_src.y; v_src.v[2] = xyz_src.z;
    alwan_vec3 rgb_src = alwan_mat3_mulv_v(M, v_src);

    alwan_vec3 v_dst;
    v_dst.v[0] = xyz_dst.x; v_dst.v[1] = xyz_dst.y; v_dst.v[2] = xyz_dst.z;
    alwan_vec3 rgb_dst = alwan_mat3_mulv_v(M, v_dst);

    alwan_vec3 v_o;
    v_o.v[0] = xyz_baseline.x; v_o.v[1] = xyz_baseline.y; v_o.v[2] = xyz_baseline.z;
    alwan_vec3 rgb_o = alwan_mat3_mulv_v(M, v_o);

    alwan_scalar D_rgb_src_0 = D_src * (rgb_o.v[0] / rgb_src.v[0]) + (ALWAN_ONE - D_src);
    alwan_scalar D_rgb_src_1 = D_src * (rgb_o.v[1] / rgb_src.v[1]) + (ALWAN_ONE - D_src);
    alwan_scalar D_rgb_src_2 = D_src * (rgb_o.v[2] / rgb_src.v[2]) + (ALWAN_ONE - D_src);

    alwan_scalar D_rgb_dst_0 = D_dst * (rgb_o.v[0] / rgb_dst.v[0]) + (ALWAN_ONE - D_dst);
    alwan_scalar D_rgb_dst_1 = D_dst * (rgb_o.v[1] / rgb_dst.v[1]) + (ALWAN_ONE - D_dst);
    alwan_scalar D_rgb_dst_2 = D_dst * (rgb_o.v[2] / rgb_dst.v[2]) + (ALWAN_ONE - D_dst);

    alwan_vec3 rgb_adapted;
    rgb_adapted.v[0] = (D_rgb_src_0 / D_rgb_dst_0) * rgb_in.v[0];
    rgb_adapted.v[1] = (D_rgb_src_1 / D_rgb_dst_1) * rgb_in.v[1];
    rgb_adapted.v[2] = (D_rgb_src_2 / D_rgb_dst_2) * rgb_in.v[2];

    alwan_vec3 vec_out = alwan_mat3_mulv_v(M_inv, rgb_adapted);
    alwan_xyz result;
    result.x = vec_out.v[0];
    result.y = vec_out.v[1];
    result.z = vec_out.v[2];
    return result;
}

/* ================================================================
 * Signed power, as the adaptation models below need it.
 *
 * Raises the magnitude and puts the sign back, so a negative base with a
 * fractional exponent stays real instead of turning into a NaN. The sign is
 * exactly zero at zero, which makes a zero base with a negative exponent come
 * out as zero rather than infinity.
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_cat_spow_v(alwan_scalar x, alwan_scalar p) {
    alwan_scalar mag = ALWAN_POW(ALWAN_ABS(x), p);
    alwan_scalar sign = ALWAN_SELECT(x < ALWAN_ZERO, -ALWAN_ONE,
                        ALWAN_SELECT(x > ALWAN_ZERO, ALWAN_ONE, ALWAN_ZERO));
    alwan_scalar r = sign * mag;
    return ALWAN_SELECT(r != r, ALWAN_ZERO, r);
}

/* ================================================================
 * CIE 1994 exponential factors (CIE 109-1994).
 * beta_1 drives the two cone channels, beta_2 the short-wave one.
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_cat_beta1_v(alwan_scalar x) {
    alwan_scalar t = alwan_cat_spow_v(x, ALWAN_LITERAL(0.4495));
    return (t * ALWAN_LITERAL(6.362) + ALWAN_LITERAL(6.469)) / (t + ALWAN_LITERAL(6.469));
}

ALWAN_INLINE alwan_scalar alwan_cat_beta2_v(alwan_scalar x) {
    alwan_scalar t = alwan_cat_spow_v(x, ALWAN_LITERAL(0.5128));
    return (t * ALWAN_LITERAL(8.091) + ALWAN_LITERAL(8.414)) * ALWAN_LITERAL(0.7844) / (t + ALWAN_LITERAL(8.414));
}

/* One channel of the CIE 1994 corresponding colour. */
ALWAN_INLINE alwan_scalar alwan_cat_cie1994_channel_v(
    alwan_scalar z,
    alwan_scalar x_1,
    alwan_scalar x_2,
    alwan_scalar y_1,
    alwan_scalar y_2,
    alwan_scalar Y_o,
    alwan_scalar K,
    alwan_scalar n)
{
    return (Y_o * x_2 + n)
         * alwan_cat_spow_v(K, ALWAN_ONE / y_2)
         * alwan_cat_spow_v((z + n) / (Y_o * x_1 + n), y_1 / y_2)
         - n;
}

/* ================================================================
 * CIE 1994 Chromatic Adaptation Model (CIE 109-1994)
 *
 * M, M_inv are the von Kries cone matrices, resolved by the caller.
 * ================================================================ */

ALWAN_INLINE alwan_xyz alwan_cat_cie1994_v(
    alwan_mat3x3 M,
    alwan_mat3x3 M_inv,
    alwan_xyz xyz_in,
    alwan_scalar x_o1,
    alwan_scalar y_o1,
    alwan_scalar x_o2,
    alwan_scalar y_o2,
    alwan_scalar Y_o,
    alwan_scalar E_o1,
    alwan_scalar E_o2,
    alwan_scalar n)
{
    alwan_vec3 v_in;
    v_in.v[0] = xyz_in.x; v_in.v[1] = xyz_in.y; v_in.v[2] = xyz_in.z;
    alwan_vec3 rgb_1 = alwan_mat3_mulv_v(M, v_in);

    /* xi, eta, zeta of each adapting field */
    alwan_scalar xi_1 = (ALWAN_LITERAL(0.48105) * x_o1 + ALWAN_LITERAL(0.78841) * y_o1 - ALWAN_LITERAL(0.08081)) / y_o1;
    alwan_scalar eta_1 = (ALWAN_LITERAL(-0.27200) * x_o1 + ALWAN_LITERAL(1.11962) * y_o1 + ALWAN_LITERAL(0.04570)) / y_o1;
    alwan_scalar zeta_1 = (ALWAN_LITERAL(0.91822) * (ALWAN_ONE - x_o1 - y_o1)) / y_o1;
    alwan_scalar xi_2 = (ALWAN_LITERAL(0.48105) * x_o2 + ALWAN_LITERAL(0.78841) * y_o2 - ALWAN_LITERAL(0.08081)) / y_o2;
    alwan_scalar eta_2 = (ALWAN_LITERAL(-0.27200) * x_o2 + ALWAN_LITERAL(1.11962) * y_o2 + ALWAN_LITERAL(0.04570)) / y_o2;
    alwan_scalar zeta_2 = (ALWAN_LITERAL(0.91822) * (ALWAN_ONE - x_o2 - y_o2)) / y_o2;

    /* Effective adapting responses: (Y_o E_o / 100 pi) scales each field */
    alwan_scalar k_1 = (Y_o * E_o1) / (ALWAN_LITERAL(100.0) * ALWAN_LITERAL(3.14159265358979323846));
    alwan_scalar k_2 = (Y_o * E_o2) / (ALWAN_LITERAL(100.0) * ALWAN_LITERAL(3.14159265358979323846));

    alwan_scalar bR_o1 = alwan_cat_beta1_v(k_1 * xi_1);
    alwan_scalar bG_o1 = alwan_cat_beta1_v(k_1 * eta_1);
    alwan_scalar bB_o1 = alwan_cat_beta2_v(k_1 * zeta_1);
    alwan_scalar bR_o2 = alwan_cat_beta1_v(k_2 * xi_2);
    alwan_scalar bG_o2 = alwan_cat_beta1_v(k_2 * eta_2);
    alwan_scalar bB_o2 = alwan_cat_beta2_v(k_2 * zeta_2);

    /* K, the coefficient that keeps a neutral stimulus neutral */
    alwan_scalar K = alwan_cat_spow_v((Y_o * xi_1 + n) / (ALWAN_LITERAL(20.0) * xi_1 + n),
                                                       (ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) * bR_o1)
                   / alwan_cat_spow_v((Y_o * xi_2 + n) / (ALWAN_LITERAL(20.0) * xi_2 + n),
                                                       (ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) * bR_o2);
    K = K * (alwan_cat_spow_v((Y_o * eta_1 + n) / (ALWAN_LITERAL(20.0) * eta_1 + n),
                                               (ALWAN_ONE / ALWAN_LITERAL(3.0)) * bG_o1)
           / alwan_cat_spow_v((Y_o * eta_2 + n) / (ALWAN_LITERAL(20.0) * eta_2 + n),
                                               (ALWAN_ONE / ALWAN_LITERAL(3.0)) * bG_o2));

    alwan_vec3 rgb_2;
    rgb_2.v[0] = alwan_cat_cie1994_channel_v(rgb_1.v[0], xi_1, xi_2, bR_o1, bR_o2, Y_o, K, n);
    rgb_2.v[1] = alwan_cat_cie1994_channel_v(rgb_1.v[1], eta_1, eta_2, bG_o1, bG_o2, Y_o, K, n);
    rgb_2.v[2] = alwan_cat_cie1994_channel_v(rgb_1.v[2], zeta_1, zeta_2, bB_o1, bB_o2, Y_o, K, n);

    alwan_vec3 vec_out = alwan_mat3_mulv_v(M_inv, rgb_2);
    alwan_xyz result;
    result.x = vec_out.v[0];
    result.y = vec_out.v[1];
    result.z = vec_out.v[2];
    return result;
}

/* ================================================================
 * vK20 Chromatic Adaptation Model (Fairchild 2020)
 *
 * Adapts towards a weighted mixture of three whites: the previous white,
 * the current white and a fixed reference.
 * ================================================================ */

ALWAN_INLINE alwan_xyz alwan_cat_vk20_v(
    alwan_mat3x3 M,
    alwan_mat3x3 M_inv,
    alwan_xyz xyz_in,
    alwan_xyz xyz_p,
    alwan_xyz xyz_n,
    alwan_xyz xyz_r,
    alwan_scalar D_n,
    alwan_scalar D_r,
    alwan_scalar D_p)
{
    alwan_vec3 v_in;
    v_in.v[0] = xyz_in.x; v_in.v[1] = xyz_in.y; v_in.v[2] = xyz_in.z;
    alwan_vec3 lms_in = alwan_mat3_mulv_v(M, v_in);

    alwan_vec3 v_p;
    v_p.v[0] = xyz_p.x; v_p.v[1] = xyz_p.y; v_p.v[2] = xyz_p.z;
    alwan_vec3 lms_p = alwan_mat3_mulv_v(M, v_p);

    alwan_vec3 v_n;
    v_n.v[0] = xyz_n.x; v_n.v[1] = xyz_n.y; v_n.v[2] = xyz_n.z;
    alwan_vec3 lms_n = alwan_mat3_mulv_v(M, v_n);

    alwan_vec3 v_r;
    v_r.v[0] = xyz_r.x; v_r.v[1] = xyz_r.y; v_r.v[2] = xyz_r.z;
    alwan_vec3 lms_r = alwan_mat3_mulv_v(M, v_r);

    /* One gain per cone, the reciprocal of the weighted white mixture */
    alwan_vec3 lms_adapted;
    lms_adapted.v[0] = lms_in.v[0] / (D_n * lms_n.v[0] + D_r * lms_r.v[0] + D_p * lms_p.v[0]);
    lms_adapted.v[1] = lms_in.v[1] / (D_n * lms_n.v[1] + D_r * lms_r.v[1] + D_p * lms_p.v[1]);
    lms_adapted.v[2] = lms_in.v[2] / (D_n * lms_n.v[2] + D_r * lms_r.v[2] + D_p * lms_p.v[2]);

    alwan_vec3 vec_out = alwan_mat3_mulv_v(M_inv, lms_adapted);
    alwan_xyz result;
    result.x = vec_out.v[0];
    result.y = vec_out.v[1];
    result.z = vec_out.v[2];
    return result;
}

/* ================================================================
 * Li 2025 Chromatic Adaptation Model
 *
 * A von Kries step in CAT16 cone space with a CIECAM-style degree of
 * adaptation, carrying the two whites' luminances through.
 * ================================================================ */

ALWAN_INLINE alwan_xyz alwan_cat_li2025_v(
    alwan_mat3x3 M,
    alwan_mat3x3 M_inv,
    alwan_xyz xyz_in,
    alwan_xyz xyz_ws,
    alwan_xyz xyz_wd,
    alwan_scalar L_A,
    alwan_scalar F_surround,
    int discount_illuminant)
{
    alwan_vec3 v_in;
    v_in.v[0] = xyz_in.x; v_in.v[1] = xyz_in.y; v_in.v[2] = xyz_in.z;
    alwan_vec3 lms_s = alwan_mat3_mulv_v(M, v_in);

    alwan_vec3 v_ws;
    v_ws.v[0] = xyz_ws.x; v_ws.v[1] = xyz_ws.y; v_ws.v[2] = xyz_ws.z;
    alwan_vec3 lms_ws = alwan_mat3_mulv_v(M, v_ws);

    alwan_vec3 v_wd;
    v_wd.v[0] = xyz_wd.x; v_wd.v[1] = xyz_wd.y; v_wd.v[2] = xyz_wd.z;
    alwan_vec3 lms_wd = alwan_mat3_mulv_v(M, v_wd);

    /* Degree of adaptation, clamped to [0, 1] */
    alwan_scalar D = F_surround * (ALWAN_ONE - (ALWAN_ONE / ALWAN_LITERAL(3.6))
                   * ALWAN_EXP((-L_A - ALWAN_LITERAL(42.0)) / ALWAN_LITERAL(92.0)));
    D = ALWAN_SELECT(D < ALWAN_ZERO, ALWAN_ZERO,
        ALWAN_SELECT(D > ALWAN_ONE, ALWAN_ONE, D));
    D = ALWAN_SELECT(discount_illuminant != 0, ALWAN_ONE, D);

    alwan_scalar Y_ratio = xyz_ws.y / xyz_wd.y;

    alwan_vec3 lms_a;
    lms_a.v[0] = lms_s.v[0] * (D * Y_ratio * (lms_wd.v[0] / lms_ws.v[0]) + (ALWAN_ONE - D));
    lms_a.v[1] = lms_s.v[1] * (D * Y_ratio * (lms_wd.v[1] / lms_ws.v[1]) + (ALWAN_ONE - D));
    lms_a.v[2] = lms_s.v[2] * (D * Y_ratio * (lms_wd.v[2] / lms_ws.v[2]) + (ALWAN_ONE - D));

    alwan_vec3 vec_out = alwan_mat3_mulv_v(M_inv, lms_a);
    alwan_xyz result;
    result.x = vec_out.v[0];
    result.y = vec_out.v[1];
    result.z = vec_out.v[2];
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_CAT_CORE_H */
