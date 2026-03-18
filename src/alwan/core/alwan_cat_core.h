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
    alwan_mat3x3 out;
    alwan_scalar sx = dst_white.x / src_white.x;
    alwan_scalar sy = dst_white.y / src_white.y;
    alwan_scalar sz = dst_white.z / src_white.z;

    out.m[0] = sx;         out.m[1] = ALWAN_ZERO; out.m[2] = ALWAN_ZERO;
    out.m[3] = ALWAN_ZERO; out.m[4] = sy;         out.m[5] = ALWAN_ZERO;
    out.m[6] = ALWAN_ZERO; out.m[7] = ALWAN_ZERO; out.m[8] = sz;
    return out;
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
    alwan_xyz out;
    out.x = r.v[0];
    out.y = r.v[1];
    out.z = r.v[2];
    return out;
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
    alwan_xyz out;
    out.x = vec_out.v[0];
    out.y = vec_out.v[1];
    out.z = vec_out.v[2];
    return out;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_CAT_CORE_H */
