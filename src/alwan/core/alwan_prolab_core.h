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

/* ----------------------------------------------------------------
 * ProLab Matrices & Constants
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* Projective transformation matrix Q (4x4 homogeneous coordinates) */
ALWAN_CONSTEXPR alwan_mat4x4 ALWAN_PROLAB_MATRIX_Q = {{
#include "../data/prolab_matrix_q.csv"
}};

/* Inverse projective transformation matrix Q^-1 */
ALWAN_CONSTEXPR alwan_mat4x4 ALWAN_PROLAB_MATRIX_Q_INV = {{
#include "../data/prolab_matrix_q_inv.csv"
}};

/* D65 reference white XYZ (Y=1 normalized) */
static alwan_scalar const ALWAN_PROLAB_D65_WHITE[3] = {
#include "../data/white_d65_xyz_y1.csv"
};

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Helper: 4x4 projective transform (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_vec3 alwan_apply_projective_v(alwan_mat4x4 matrix, alwan_vec3 input) {
    alwan_vec3 result;

    /* Unrolled 4x4 matrix * [x, y, z, 1] */
    alwan_scalar r0 = matrix.m[0]  * input.v[0] + matrix.m[1]  * input.v[1] + matrix.m[2]  * input.v[2] + matrix.m[3];
    alwan_scalar r1 = matrix.m[4]  * input.v[0] + matrix.m[5]  * input.v[1] + matrix.m[6]  * input.v[2] + matrix.m[7];
    alwan_scalar r2 = matrix.m[8]  * input.v[0] + matrix.m[9]  * input.v[1] + matrix.m[10] * input.v[2] + matrix.m[11];
    alwan_scalar w  = matrix.m[12] * input.v[0] + matrix.m[13] * input.v[1] + matrix.m[14] * input.v[2] + matrix.m[15];

    /* Division guard: if |w| < 1e-10, return input as fallback */
    alwan_scalar abs_w = ALWAN_ABS(w);
    alwan_scalar safe = ALWAN_SELECT(abs_w < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    alwan_scalar inv_w = ALWAN_SELECT(abs_w < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0) / w);

    result.v[0] = ALWAN_SELECT(abs_w < ALWAN_LITERAL(1e-10), input.v[0], r0 * inv_w);
    result.v[1] = ALWAN_SELECT(abs_w < ALWAN_LITERAL(1e-10), input.v[1], r1 * inv_w);
    result.v[2] = ALWAN_SELECT(abs_w < ALWAN_LITERAL(1e-10), input.v[2], r2 * inv_w);

    ALWAN_UNUSED(safe);
    return result;
}

/* ----------------------------------------------------------------
 * XYZ -> ProLab, D65 (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_prolab alwan_xyz_to_prolab_v(alwan_xyz xyz) {
    alwan_prolab result;

    /* Normalize XYZ by D65 reference white */
    alwan_vec3 xyz_rel;
    xyz_rel.v[0] = xyz.x / ALWAN_PROLAB_D65_WHITE[0];
    xyz_rel.v[1] = xyz.y / ALWAN_PROLAB_D65_WHITE[1];
    xyz_rel.v[2] = xyz.z / ALWAN_PROLAB_D65_WHITE[2];

    /* Apply projective transformation */
    alwan_vec3 prolab_vec = alwan_apply_projective_v(ALWAN_PROLAB_MATRIX_Q, xyz_rel);
    result.L = prolab_vec.v[0];
    result.a = prolab_vec.v[1];
    result.b = prolab_vec.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * ProLab -> XYZ, D65 (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_xyz alwan_prolab_to_xyz_v(alwan_prolab prolab) {
    alwan_xyz result;

    alwan_vec3 prolab_vec;
    prolab_vec.v[0] = prolab.L;
    prolab_vec.v[1] = prolab.a;
    prolab_vec.v[2] = prolab.b;

    alwan_vec3 xyz_rel = alwan_apply_projective_v(ALWAN_PROLAB_MATRIX_Q_INV, prolab_vec);

    result.x = xyz_rel.v[0] * ALWAN_PROLAB_D65_WHITE[0];
    result.y = xyz_rel.v[1] * ALWAN_PROLAB_D65_WHITE[1];
    result.z = xyz_rel.v[2] * ALWAN_PROLAB_D65_WHITE[2];

    return result;
}

/* ----------------------------------------------------------------
 * XYZ -> ProLab, custom illuminant (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_prolab alwan_xyz_to_prolab_custom_v(alwan_xyz xyz, alwan_xyz xyz_n) {
    alwan_prolab result;

    alwan_scalar const eps = ALWAN_LITERAL(1e-10);
    alwan_scalar xn = ALWAN_SELECT(ALWAN_ABS(xyz_n.x) > eps, xyz_n.x, eps);
    alwan_scalar yn = ALWAN_SELECT(ALWAN_ABS(xyz_n.y) > eps, xyz_n.y, eps);
    alwan_scalar zn = ALWAN_SELECT(ALWAN_ABS(xyz_n.z) > eps, xyz_n.z, eps);

    alwan_vec3 xyz_rel;
    xyz_rel.v[0] = xyz.x / xn;
    xyz_rel.v[1] = xyz.y / yn;
    xyz_rel.v[2] = xyz.z / zn;

    alwan_vec3 prolab_vec = alwan_apply_projective_v(ALWAN_PROLAB_MATRIX_Q, xyz_rel);
    result.L = prolab_vec.v[0];
    result.a = prolab_vec.v[1];
    result.b = prolab_vec.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * ProLab -> XYZ, custom illuminant (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_xyz alwan_prolab_to_xyz_custom_v(alwan_prolab prolab, alwan_xyz xyz_n) {
    alwan_xyz result;

    alwan_vec3 prolab_vec;
    prolab_vec.v[0] = prolab.L;
    prolab_vec.v[1] = prolab.a;
    prolab_vec.v[2] = prolab.b;

    alwan_vec3 xyz_rel = alwan_apply_projective_v(ALWAN_PROLAB_MATRIX_Q_INV, prolab_vec);

    result.x = xyz_rel.v[0] * xyz_n.x;
    result.y = xyz_rel.v[1] * xyz_n.y;
    result.z = xyz_rel.v[2] * xyz_n.z;

    return result;
}

#endif /* ALWAN_PROLAB_CORE_H */
