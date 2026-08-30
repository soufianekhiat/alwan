/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ProLab Color Space (Perceptually Uniform Projective)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Konovalenko et al. (2021)
 * "ProLab: A Perceptually Uniform Projective Color Coordinate System"
 */

#ifndef ALWAN_PROLAB_CORE_H
#define ALWAN_PROLAB_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_prolab_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_prolab_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_CONSTEXPR alwan_mat4x4 ALWAN_PROLAB_MATRIX_Q = {{
#include "../data/prolab_matrix_q.csv"
}};

ALWAN_CONSTEXPR alwan_mat4x4 ALWAN_PROLAB_MATRIX_Q_INV = {{
#include "../data/prolab_matrix_q_inv.csv"
}};

static alwan_scalar const ALWAN_PROLAB_D65_WHITE[3] = {
#include "../data/white_d65_xyz_y1.csv"
};

ALWAN_DIAG_POP

ALWAN_INLINE alwan_vec3 alwan_apply_projective_v(alwan_mat4x4 mat, alwan_vec3 pt) {
    alwan_vec3 result;
    alwan_scalar r0 = mat.m[0]  * pt.v[0] + mat.m[1]  * pt.v[1] + mat.m[2]  * pt.v[2] + mat.m[3];
    alwan_scalar r1 = mat.m[4]  * pt.v[0] + mat.m[5]  * pt.v[1] + mat.m[6]  * pt.v[2] + mat.m[7];
    alwan_scalar r2 = mat.m[8]  * pt.v[0] + mat.m[9]  * pt.v[1] + mat.m[10] * pt.v[2] + mat.m[11];
    alwan_scalar w  = mat.m[12] * pt.v[0] + mat.m[13] * pt.v[1] + mat.m[14] * pt.v[2] + mat.m[15];
    alwan_scalar abs_w = ALWAN_ABS(w);
    alwan_scalar safe = ALWAN_SELECT(abs_w < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    alwan_scalar inv_w = ALWAN_SELECT(abs_w < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0) / w);
    result.v[0] = ALWAN_SELECT(abs_w < ALWAN_LITERAL(1e-10), pt.v[0], r0 * inv_w);
    result.v[1] = ALWAN_SELECT(abs_w < ALWAN_LITERAL(1e-10), pt.v[1], r1 * inv_w);
    result.v[2] = ALWAN_SELECT(abs_w < ALWAN_LITERAL(1e-10), pt.v[2], r2 * inv_w);
    ALWAN_UNUSED(safe);
    return result;
}

ALWAN_INLINE alwan_prolab alwan_xyz_to_prolab_v(alwan_xyz xyz) {
    alwan_prolab result;
    alwan_vec3 xyz_rel;
    xyz_rel.v[0] = xyz.x / ALWAN_PROLAB_D65_WHITE[0];
    xyz_rel.v[1] = xyz.y / ALWAN_PROLAB_D65_WHITE[1];
    xyz_rel.v[2] = xyz.z / ALWAN_PROLAB_D65_WHITE[2];
    alwan_vec3 prolab_vec = alwan_apply_projective_v(ALWAN_PROLAB_MATRIX_Q, xyz_rel);
    result.L = prolab_vec.v[0]; result.a = prolab_vec.v[1]; result.b = prolab_vec.v[2];
    return result;
}

ALWAN_INLINE alwan_xyz alwan_prolab_to_xyz_v(alwan_prolab prolab) {
    alwan_xyz result;
    alwan_vec3 prolab_vec;
    prolab_vec.v[0] = prolab.L; prolab_vec.v[1] = prolab.a; prolab_vec.v[2] = prolab.b;
    alwan_vec3 xyz_rel = alwan_apply_projective_v(ALWAN_PROLAB_MATRIX_Q_INV, prolab_vec);
    result.x = xyz_rel.v[0] * ALWAN_PROLAB_D65_WHITE[0];
    result.y = xyz_rel.v[1] * ALWAN_PROLAB_D65_WHITE[1];
    result.z = xyz_rel.v[2] * ALWAN_PROLAB_D65_WHITE[2];
    return result;
}

ALWAN_INLINE alwan_prolab alwan_xyz_to_prolab_custom_v(alwan_xyz xyz, alwan_xyz xyz_n) {
    alwan_prolab result;
    alwan_scalar const eps = ALWAN_LITERAL(1e-10);
    alwan_scalar xn = ALWAN_SELECT(ALWAN_ABS(xyz_n.x) > eps, xyz_n.x, eps);
    alwan_scalar yn = ALWAN_SELECT(ALWAN_ABS(xyz_n.y) > eps, xyz_n.y, eps);
    alwan_scalar zn = ALWAN_SELECT(ALWAN_ABS(xyz_n.z) > eps, xyz_n.z, eps);
    alwan_vec3 xyz_rel;
    xyz_rel.v[0] = xyz.x / xn; xyz_rel.v[1] = xyz.y / yn; xyz_rel.v[2] = xyz.z / zn;
    alwan_vec3 prolab_vec = alwan_apply_projective_v(ALWAN_PROLAB_MATRIX_Q, xyz_rel);
    result.L = prolab_vec.v[0]; result.a = prolab_vec.v[1]; result.b = prolab_vec.v[2];
    return result;
}

ALWAN_INLINE alwan_xyz alwan_prolab_to_xyz_custom_v(alwan_prolab prolab, alwan_xyz xyz_n) {
    alwan_xyz result;
    alwan_vec3 prolab_vec;
    prolab_vec.v[0] = prolab.L; prolab_vec.v[1] = prolab.a; prolab_vec.v[2] = prolab.b;
    alwan_vec3 xyz_rel = alwan_apply_projective_v(ALWAN_PROLAB_MATRIX_Q_INV, prolab_vec);
    result.x = xyz_rel.v[0] * xyz_n.x;
    result.y = xyz_rel.v[1] * xyz_n.y;
    result.z = xyz_rel.v[2] * xyz_n.z;
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_PROLAB_CORE_H */
