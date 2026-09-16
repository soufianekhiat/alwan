/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Chromatic Adaptation Transform (CAT) core math (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Uses ALWAN_CORE_* macros (set by alwan_core_f32_setup.h or _f64_setup.h).
 */

/* ================================================================
 * XYZ Scaling Adaptation (simplest case)
 * Returns diagonal matrix of dst/src ratios.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FNVLIT(alwan_cat_xyz_scaling)(ALWAN_CORE_XYZ src_white, ALWAN_CORE_XYZ dst_white) {
    ALWAN_CORE_MAT3X3 result;
    ALWAN_CORE_T sx = dst_white.x / src_white.x;
    ALWAN_CORE_T sy = dst_white.y / src_white.y;
    ALWAN_CORE_T sz = dst_white.z / src_white.z;

    result.m[0] = sx;             result.m[1] = ALWAN_CORE_ZERO; result.m[2] = ALWAN_CORE_ZERO;
    result.m[3] = ALWAN_CORE_ZERO; result.m[4] = sy;             result.m[5] = ALWAN_CORE_ZERO;
    result.m[6] = ALWAN_CORE_ZERO; result.m[7] = ALWAN_CORE_ZERO; result.m[8] = sz;
    return result;
}

/* ================================================================
 * General CAT Matrix Computation
 * Given a cone response matrix M (and its inverse M_inv),
 * compute the full adaptation matrix: M_adapt = M^-1 * D * M
 * where D = diag(rgb_dst ./ rgb_src).
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FNVLIT(alwan_cat_matrix)(
    ALWAN_CORE_MAT3X3 M,
    ALWAN_CORE_MAT3X3 M_inv,
    ALWAN_CORE_XYZ src_white,
    ALWAN_CORE_XYZ dst_white)
{
    /* Transform white points to cone response space */
    ALWAN_CORE_VEC3 vec_src;
    vec_src.v[0] = src_white.x;
    vec_src.v[1] = src_white.y;
    vec_src.v[2] = src_white.z;
    ALWAN_CORE_VEC3 rgb_src = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, vec_src);

    ALWAN_CORE_VEC3 vec_dst;
    vec_dst.v[0] = dst_white.x;
    vec_dst.v[1] = dst_white.y;
    vec_dst.v[2] = dst_white.z;
    ALWAN_CORE_VEC3 rgb_dst = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, vec_dst);

    /* Diagonal scaling matrix D = diag(rgb_dst ./ rgb_src) */
    ALWAN_CORE_MAT3X3 D;
    D.m[0] = rgb_dst.v[0] / rgb_src.v[0];
    D.m[1] = ALWAN_CORE_ZERO; D.m[2] = ALWAN_CORE_ZERO;
    D.m[3] = ALWAN_CORE_ZERO;
    D.m[4] = rgb_dst.v[1] / rgb_src.v[1];
    D.m[5] = ALWAN_CORE_ZERO;
    D.m[6] = ALWAN_CORE_ZERO; D.m[7] = ALWAN_CORE_ZERO;
    D.m[8] = rgb_dst.v[2] / rgb_src.v[2];

    /* M_adapt = M_inv * D * M */
    ALWAN_CORE_MAT3X3 DM = ALWAN_CORE_FNVLIT(alwan_mat3_mul)(D, M);
    return ALWAN_CORE_FNVLIT(alwan_mat3_mul)(M_inv, DM);
}

/* ================================================================
 * Apply Adaptation Matrix to a single XYZ color
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_cat_adapt)(ALWAN_CORE_MAT3X3 cat_mat, ALWAN_CORE_XYZ xyz_in) {
    ALWAN_CORE_VEC3 v;
    v.v[0] = xyz_in.x;
    v.v[1] = xyz_in.y;
    v.v[2] = xyz_in.z;
    ALWAN_CORE_VEC3 r = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(cat_mat, v);
    ALWAN_CORE_XYZ result;
    result.x = r.v[0];
    result.y = r.v[1];
    result.z = r.v[2];
    return result;
}

/* ================================================================
 * Zhai & Luo 2018 Two-Step Chromatic Adaptation
 *
 * Takes M and M_inv directly (caller resolves CAT02 or CAT16).
 * D_src, D_dst: degree of adaptation [0,1] for source and dest.
 * xyz_baseline: baseline illuminant (typically equal-energy E).
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_cat_zhai2018)(
    ALWAN_CORE_MAT3X3 M,
    ALWAN_CORE_MAT3X3 M_inv,
    ALWAN_CORE_XYZ xyz_in,
    ALWAN_CORE_XYZ xyz_src,
    ALWAN_CORE_XYZ xyz_dst,
    ALWAN_CORE_T D_src,
    ALWAN_CORE_T D_dst,
    ALWAN_CORE_XYZ xyz_baseline)
{
    /* Transform all to cone response space */
    ALWAN_CORE_VEC3 v_in;
    v_in.v[0] = xyz_in.x; v_in.v[1] = xyz_in.y; v_in.v[2] = xyz_in.z;
    ALWAN_CORE_VEC3 rgb_in = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, v_in);

    ALWAN_CORE_VEC3 v_src;
    v_src.v[0] = xyz_src.x; v_src.v[1] = xyz_src.y; v_src.v[2] = xyz_src.z;
    ALWAN_CORE_VEC3 rgb_src = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, v_src);

    ALWAN_CORE_VEC3 v_dst;
    v_dst.v[0] = xyz_dst.x; v_dst.v[1] = xyz_dst.y; v_dst.v[2] = xyz_dst.z;
    ALWAN_CORE_VEC3 rgb_dst = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, v_dst);

    ALWAN_CORE_VEC3 v_o;
    v_o.v[0] = xyz_baseline.x; v_o.v[1] = xyz_baseline.y; v_o.v[2] = xyz_baseline.z;
    ALWAN_CORE_VEC3 rgb_o = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, v_o);

    /* D_RGB factors for source and destination (unrolled 3 channels)
     * D_RGB = D * (RGB_o / RGB_w) + 1 - D */
    ALWAN_CORE_T D_rgb_src_0 = D_src * (rgb_o.v[0] / rgb_src.v[0]) + (ALWAN_CORE_ONE - D_src);
    ALWAN_CORE_T D_rgb_src_1 = D_src * (rgb_o.v[1] / rgb_src.v[1]) + (ALWAN_CORE_ONE - D_src);
    ALWAN_CORE_T D_rgb_src_2 = D_src * (rgb_o.v[2] / rgb_src.v[2]) + (ALWAN_CORE_ONE - D_src);

    ALWAN_CORE_T D_rgb_dst_0 = D_dst * (rgb_o.v[0] / rgb_dst.v[0]) + (ALWAN_CORE_ONE - D_dst);
    ALWAN_CORE_T D_rgb_dst_1 = D_dst * (rgb_o.v[1] / rgb_dst.v[1]) + (ALWAN_CORE_ONE - D_dst);
    ALWAN_CORE_T D_rgb_dst_2 = D_dst * (rgb_o.v[2] / rgb_dst.v[2]) + (ALWAN_CORE_ONE - D_dst);

    /* Two-step adaptation: RGB_d = (D_RGB_src / D_RGB_dst) * RGB_in */
    ALWAN_CORE_VEC3 rgb_adapted;
    rgb_adapted.v[0] = (D_rgb_src_0 / D_rgb_dst_0) * rgb_in.v[0];
    rgb_adapted.v[1] = (D_rgb_src_1 / D_rgb_dst_1) * rgb_in.v[1];
    rgb_adapted.v[2] = (D_rgb_src_2 / D_rgb_dst_2) * rgb_in.v[2];

    /* Transform back to XYZ */
    ALWAN_CORE_VEC3 vec_out = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M_inv, rgb_adapted);
    ALWAN_CORE_XYZ result;
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

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_cat_spow)(ALWAN_CORE_T x, ALWAN_CORE_T p) {
    ALWAN_CORE_T mag = ALWAN_CORE_POW(ALWAN_CORE_ABS(x), p);
    ALWAN_CORE_T sign = ALWAN_CORE_SELECT(x < ALWAN_CORE_ZERO, -ALWAN_CORE_ONE,
                        ALWAN_CORE_SELECT(x > ALWAN_CORE_ZERO, ALWAN_CORE_ONE, ALWAN_CORE_ZERO));
    ALWAN_CORE_T r = sign * mag;
    return ALWAN_CORE_SELECT(r != r, ALWAN_CORE_ZERO, r);
}

/* ================================================================
 * CIE 1994 exponential factors (CIE 109-1994).
 * beta_1 drives the two cone channels, beta_2 the short-wave one.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_cat_beta1)(ALWAN_CORE_T x) {
    ALWAN_CORE_T t = ALWAN_CORE_FNVLIT(alwan_cat_spow)(x, ALWAN_CORE_LITERAL(0.4495));
    return (t * ALWAN_CORE_LITERAL(6.362) + ALWAN_CORE_LITERAL(6.469)) / (t + ALWAN_CORE_LITERAL(6.469));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_cat_beta2)(ALWAN_CORE_T x) {
    ALWAN_CORE_T t = ALWAN_CORE_FNVLIT(alwan_cat_spow)(x, ALWAN_CORE_LITERAL(0.5128));
    return (t * ALWAN_CORE_LITERAL(8.091) + ALWAN_CORE_LITERAL(8.414)) * ALWAN_CORE_LITERAL(0.7844) / (t + ALWAN_CORE_LITERAL(8.414));
}

/* One channel of the CIE 1994 corresponding colour. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_cat_cie1994_channel)(
    ALWAN_CORE_T z,
    ALWAN_CORE_T x_1,
    ALWAN_CORE_T x_2,
    ALWAN_CORE_T y_1,
    ALWAN_CORE_T y_2,
    ALWAN_CORE_T Y_o,
    ALWAN_CORE_T K,
    ALWAN_CORE_T n)
{
    return (Y_o * x_2 + n)
         * ALWAN_CORE_FNVLIT(alwan_cat_spow)(K, ALWAN_CORE_ONE / y_2)
         * ALWAN_CORE_FNVLIT(alwan_cat_spow)((z + n) / (Y_o * x_1 + n), y_1 / y_2)
         - n;
}

/* ================================================================
 * CIE 1994 Chromatic Adaptation Model (CIE 109-1994)
 *
 * M, M_inv are the von Kries cone matrices, resolved by the caller.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_cat_cie1994)(
    ALWAN_CORE_MAT3X3 M,
    ALWAN_CORE_MAT3X3 M_inv,
    ALWAN_CORE_XYZ xyz_in,
    ALWAN_CORE_T x_o1,
    ALWAN_CORE_T y_o1,
    ALWAN_CORE_T x_o2,
    ALWAN_CORE_T y_o2,
    ALWAN_CORE_T Y_o,
    ALWAN_CORE_T E_o1,
    ALWAN_CORE_T E_o2,
    ALWAN_CORE_T n)
{
    ALWAN_CORE_VEC3 v_in;
    v_in.v[0] = xyz_in.x; v_in.v[1] = xyz_in.y; v_in.v[2] = xyz_in.z;
    ALWAN_CORE_VEC3 rgb_1 = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, v_in);

    /* xi, eta, zeta of each adapting field */
    ALWAN_CORE_T xi_1 = (ALWAN_CORE_LITERAL(0.48105) * x_o1 + ALWAN_CORE_LITERAL(0.78841) * y_o1 - ALWAN_CORE_LITERAL(0.08081)) / y_o1;
    ALWAN_CORE_T eta_1 = (ALWAN_CORE_LITERAL(-0.27200) * x_o1 + ALWAN_CORE_LITERAL(1.11962) * y_o1 + ALWAN_CORE_LITERAL(0.04570)) / y_o1;
    ALWAN_CORE_T zeta_1 = (ALWAN_CORE_LITERAL(0.91822) * (ALWAN_CORE_ONE - x_o1 - y_o1)) / y_o1;
    ALWAN_CORE_T xi_2 = (ALWAN_CORE_LITERAL(0.48105) * x_o2 + ALWAN_CORE_LITERAL(0.78841) * y_o2 - ALWAN_CORE_LITERAL(0.08081)) / y_o2;
    ALWAN_CORE_T eta_2 = (ALWAN_CORE_LITERAL(-0.27200) * x_o2 + ALWAN_CORE_LITERAL(1.11962) * y_o2 + ALWAN_CORE_LITERAL(0.04570)) / y_o2;
    ALWAN_CORE_T zeta_2 = (ALWAN_CORE_LITERAL(0.91822) * (ALWAN_CORE_ONE - x_o2 - y_o2)) / y_o2;

    /* Effective adapting responses: (Y_o E_o / 100 pi) scales each field */
    ALWAN_CORE_T k_1 = (Y_o * E_o1) / (ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_LITERAL(3.14159265358979323846));
    ALWAN_CORE_T k_2 = (Y_o * E_o2) / (ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_LITERAL(3.14159265358979323846));

    ALWAN_CORE_T bR_o1 = ALWAN_CORE_FNVLIT(alwan_cat_beta1)(k_1 * xi_1);
    ALWAN_CORE_T bG_o1 = ALWAN_CORE_FNVLIT(alwan_cat_beta1)(k_1 * eta_1);
    ALWAN_CORE_T bB_o1 = ALWAN_CORE_FNVLIT(alwan_cat_beta2)(k_1 * zeta_1);
    ALWAN_CORE_T bR_o2 = ALWAN_CORE_FNVLIT(alwan_cat_beta1)(k_2 * xi_2);
    ALWAN_CORE_T bG_o2 = ALWAN_CORE_FNVLIT(alwan_cat_beta1)(k_2 * eta_2);
    ALWAN_CORE_T bB_o2 = ALWAN_CORE_FNVLIT(alwan_cat_beta2)(k_2 * zeta_2);

    /* K, the coefficient that keeps a neutral stimulus neutral */
    ALWAN_CORE_T K = ALWAN_CORE_FNVLIT(alwan_cat_spow)((Y_o * xi_1 + n) / (ALWAN_CORE_LITERAL(20.0) * xi_1 + n),
                                                       (ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0)) * bR_o1)
                   / ALWAN_CORE_FNVLIT(alwan_cat_spow)((Y_o * xi_2 + n) / (ALWAN_CORE_LITERAL(20.0) * xi_2 + n),
                                                       (ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0)) * bR_o2);
    K = K * (ALWAN_CORE_FNVLIT(alwan_cat_spow)((Y_o * eta_1 + n) / (ALWAN_CORE_LITERAL(20.0) * eta_1 + n),
                                               (ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0)) * bG_o1)
           / ALWAN_CORE_FNVLIT(alwan_cat_spow)((Y_o * eta_2 + n) / (ALWAN_CORE_LITERAL(20.0) * eta_2 + n),
                                               (ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0)) * bG_o2));

    ALWAN_CORE_VEC3 rgb_2;
    rgb_2.v[0] = ALWAN_CORE_FNVLIT(alwan_cat_cie1994_channel)(rgb_1.v[0], xi_1, xi_2, bR_o1, bR_o2, Y_o, K, n);
    rgb_2.v[1] = ALWAN_CORE_FNVLIT(alwan_cat_cie1994_channel)(rgb_1.v[1], eta_1, eta_2, bG_o1, bG_o2, Y_o, K, n);
    rgb_2.v[2] = ALWAN_CORE_FNVLIT(alwan_cat_cie1994_channel)(rgb_1.v[2], zeta_1, zeta_2, bB_o1, bB_o2, Y_o, K, n);

    ALWAN_CORE_VEC3 vec_out = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M_inv, rgb_2);
    ALWAN_CORE_XYZ result;
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

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_cat_vk20)(
    ALWAN_CORE_MAT3X3 M,
    ALWAN_CORE_MAT3X3 M_inv,
    ALWAN_CORE_XYZ xyz_in,
    ALWAN_CORE_XYZ xyz_p,
    ALWAN_CORE_XYZ xyz_n,
    ALWAN_CORE_XYZ xyz_r,
    ALWAN_CORE_T D_n,
    ALWAN_CORE_T D_r,
    ALWAN_CORE_T D_p)
{
    ALWAN_CORE_VEC3 v_in;
    v_in.v[0] = xyz_in.x; v_in.v[1] = xyz_in.y; v_in.v[2] = xyz_in.z;
    ALWAN_CORE_VEC3 lms_in = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, v_in);

    ALWAN_CORE_VEC3 v_p;
    v_p.v[0] = xyz_p.x; v_p.v[1] = xyz_p.y; v_p.v[2] = xyz_p.z;
    ALWAN_CORE_VEC3 lms_p = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, v_p);

    ALWAN_CORE_VEC3 v_n;
    v_n.v[0] = xyz_n.x; v_n.v[1] = xyz_n.y; v_n.v[2] = xyz_n.z;
    ALWAN_CORE_VEC3 lms_n = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, v_n);

    ALWAN_CORE_VEC3 v_r;
    v_r.v[0] = xyz_r.x; v_r.v[1] = xyz_r.y; v_r.v[2] = xyz_r.z;
    ALWAN_CORE_VEC3 lms_r = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, v_r);

    /* One gain per cone, the reciprocal of the weighted white mixture */
    ALWAN_CORE_VEC3 lms_adapted;
    lms_adapted.v[0] = lms_in.v[0] / (D_n * lms_n.v[0] + D_r * lms_r.v[0] + D_p * lms_p.v[0]);
    lms_adapted.v[1] = lms_in.v[1] / (D_n * lms_n.v[1] + D_r * lms_r.v[1] + D_p * lms_p.v[1]);
    lms_adapted.v[2] = lms_in.v[2] / (D_n * lms_n.v[2] + D_r * lms_r.v[2] + D_p * lms_p.v[2]);

    ALWAN_CORE_VEC3 vec_out = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M_inv, lms_adapted);
    ALWAN_CORE_XYZ result;
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

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_cat_li2025)(
    ALWAN_CORE_MAT3X3 M,
    ALWAN_CORE_MAT3X3 M_inv,
    ALWAN_CORE_XYZ xyz_in,
    ALWAN_CORE_XYZ xyz_ws,
    ALWAN_CORE_XYZ xyz_wd,
    ALWAN_CORE_T L_A,
    ALWAN_CORE_T F_surround,
    int discount_illuminant)
{
    ALWAN_CORE_VEC3 v_in;
    v_in.v[0] = xyz_in.x; v_in.v[1] = xyz_in.y; v_in.v[2] = xyz_in.z;
    ALWAN_CORE_VEC3 lms_s = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, v_in);

    ALWAN_CORE_VEC3 v_ws;
    v_ws.v[0] = xyz_ws.x; v_ws.v[1] = xyz_ws.y; v_ws.v[2] = xyz_ws.z;
    ALWAN_CORE_VEC3 lms_ws = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, v_ws);

    ALWAN_CORE_VEC3 v_wd;
    v_wd.v[0] = xyz_wd.x; v_wd.v[1] = xyz_wd.y; v_wd.v[2] = xyz_wd.z;
    ALWAN_CORE_VEC3 lms_wd = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, v_wd);

    /* Degree of adaptation, clamped to [0, 1] */
    ALWAN_CORE_T D = F_surround * (ALWAN_CORE_ONE - (ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.6))
                   * ALWAN_CORE_EXP((-L_A - ALWAN_CORE_LITERAL(42.0)) / ALWAN_CORE_LITERAL(92.0)));
    D = ALWAN_CORE_SELECT(D < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO,
        ALWAN_CORE_SELECT(D > ALWAN_CORE_ONE, ALWAN_CORE_ONE, D));
    D = ALWAN_CORE_SELECT(discount_illuminant != 0, ALWAN_CORE_ONE, D);

    ALWAN_CORE_T Y_ratio = xyz_ws.y / xyz_wd.y;

    ALWAN_CORE_VEC3 lms_a;
    lms_a.v[0] = lms_s.v[0] * (D * Y_ratio * (lms_wd.v[0] / lms_ws.v[0]) + (ALWAN_CORE_ONE - D));
    lms_a.v[1] = lms_s.v[1] * (D * Y_ratio * (lms_wd.v[1] / lms_ws.v[1]) + (ALWAN_CORE_ONE - D));
    lms_a.v[2] = lms_s.v[2] * (D * Y_ratio * (lms_wd.v[2] / lms_ws.v[2]) + (ALWAN_CORE_ONE - D));

    ALWAN_CORE_VEC3 vec_out = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M_inv, lms_a);
    ALWAN_CORE_XYZ result;
    result.x = vec_out.v[0];
    result.y = vec_out.v[1];
    result.z = vec_out.v[2];
    return result;
}
