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
    ALWAN_CORE_MAT3X3 out;
    ALWAN_CORE_T sx = dst_white.x / src_white.x;
    ALWAN_CORE_T sy = dst_white.y / src_white.y;
    ALWAN_CORE_T sz = dst_white.z / src_white.z;

    out.m[0] = sx;             out.m[1] = ALWAN_CORE_ZERO; out.m[2] = ALWAN_CORE_ZERO;
    out.m[3] = ALWAN_CORE_ZERO; out.m[4] = sy;             out.m[5] = ALWAN_CORE_ZERO;
    out.m[6] = ALWAN_CORE_ZERO; out.m[7] = ALWAN_CORE_ZERO; out.m[8] = sz;
    return out;
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
    ALWAN_CORE_XYZ out;
    out.x = r.v[0];
    out.y = r.v[1];
    out.z = r.v[2];
    return out;
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
    ALWAN_CORE_XYZ out;
    out.x = vec_out.v[0];
    out.y = vec_out.v[1];
    out.z = vec_out.v[2];
    return out;
}
