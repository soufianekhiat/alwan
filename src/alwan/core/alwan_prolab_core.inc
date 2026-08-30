/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ProLab Color Space (Perceptually Uniform Projective) (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Konovalenko et al. (2021)
 * "ProLab: A Perceptually Uniform Projective Color Coordinate System"
 */

/* ----------------------------------------------------------------
 * ProLab Matrices & Constants
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* Projective transformation matrix Q (4x4 homogeneous coordinates) */
ALWAN_CONSTEXPR ALWAN_CORE_MAT4X4 ALWAN_CORE_FNLIT(ALWAN_PROLAB_MATRIX_Q) = {{
#include "../data/prolab_matrix_q.csv"
}};

/* Inverse projective transformation matrix Q^-1 */
ALWAN_CONSTEXPR ALWAN_CORE_MAT4X4 ALWAN_CORE_FNLIT(ALWAN_PROLAB_MATRIX_Q_INV) = {{
#include "../data/prolab_matrix_q_inv.csv"
}};

/* D65 reference white XYZ (Y=1 normalized) */
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(ALWAN_PROLAB_D65_WHITE)[3] = {
#include "../data/white_d65_xyz_y1.csv"
};

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * 4x4 projective transform (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_apply_projective)(ALWAN_CORE_MAT4X4 mat, ALWAN_CORE_VEC3 pt) {
    ALWAN_CORE_VEC3 result;

    /* Unrolled 4x4 matrix * [x, y, z, 1] */
    ALWAN_CORE_T r0 = mat.m[0]  * pt.v[0] + mat.m[1]  * pt.v[1] + mat.m[2]  * pt.v[2] + mat.m[3];
    ALWAN_CORE_T r1 = mat.m[4]  * pt.v[0] + mat.m[5]  * pt.v[1] + mat.m[6]  * pt.v[2] + mat.m[7];
    ALWAN_CORE_T r2 = mat.m[8]  * pt.v[0] + mat.m[9]  * pt.v[1] + mat.m[10] * pt.v[2] + mat.m[11];
    ALWAN_CORE_T w  = mat.m[12] * pt.v[0] + mat.m[13] * pt.v[1] + mat.m[14] * pt.v[2] + mat.m[15];

    /* Division guard: if |w| < 1e-10, return input as fallback */
    ALWAN_CORE_T abs_w = ALWAN_CORE_ABS(w);
    ALWAN_CORE_T safe = ALWAN_CORE_SELECT(abs_w < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T inv_w = ALWAN_CORE_SELECT(abs_w < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0) / w);

    result.v[0] = ALWAN_CORE_SELECT(abs_w < ALWAN_CORE_LITERAL(1e-10), pt.v[0], r0 * inv_w);
    result.v[1] = ALWAN_CORE_SELECT(abs_w < ALWAN_CORE_LITERAL(1e-10), pt.v[1], r1 * inv_w);
    result.v[2] = ALWAN_CORE_SELECT(abs_w < ALWAN_CORE_LITERAL(1e-10), pt.v[2], r2 * inv_w);

    ALWAN_UNUSED(safe);
    return result;
}

/* ----------------------------------------------------------------
 * XYZ -> ProLab, D65 (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_PROLAB ALWAN_CORE_FNVLIT(alwan_xyz_to_prolab)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_PROLAB result;

    /* Normalize XYZ by D65 reference white */
    ALWAN_CORE_VEC3 xyz_rel;
    xyz_rel.v[0] = xyz.x / ALWAN_CORE_FNLIT(ALWAN_PROLAB_D65_WHITE)[0];
    xyz_rel.v[1] = xyz.y / ALWAN_CORE_FNLIT(ALWAN_PROLAB_D65_WHITE)[1];
    xyz_rel.v[2] = xyz.z / ALWAN_CORE_FNLIT(ALWAN_PROLAB_D65_WHITE)[2];

    /* Apply projective transformation */
    ALWAN_CORE_VEC3 prolab_vec = ALWAN_CORE_FNVLIT(alwan_apply_projective)(ALWAN_CORE_FNLIT(ALWAN_PROLAB_MATRIX_Q), xyz_rel);
    result.L = prolab_vec.v[0];
    result.a = prolab_vec.v[1];
    result.b = prolab_vec.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * ProLab -> XYZ, D65 (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_prolab_to_xyz)(ALWAN_CORE_PROLAB prolab) {
    ALWAN_CORE_XYZ result;

    ALWAN_CORE_VEC3 prolab_vec;
    prolab_vec.v[0] = prolab.L;
    prolab_vec.v[1] = prolab.a;
    prolab_vec.v[2] = prolab.b;

    ALWAN_CORE_VEC3 xyz_rel = ALWAN_CORE_FNVLIT(alwan_apply_projective)(ALWAN_CORE_FNLIT(ALWAN_PROLAB_MATRIX_Q_INV), prolab_vec);

    result.x = xyz_rel.v[0] * ALWAN_CORE_FNLIT(ALWAN_PROLAB_D65_WHITE)[0];
    result.y = xyz_rel.v[1] * ALWAN_CORE_FNLIT(ALWAN_PROLAB_D65_WHITE)[1];
    result.z = xyz_rel.v[2] * ALWAN_CORE_FNLIT(ALWAN_PROLAB_D65_WHITE)[2];

    return result;
}

/* ----------------------------------------------------------------
 * XYZ -> ProLab, custom illuminant (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_PROLAB ALWAN_CORE_FNVLIT(alwan_xyz_to_prolab_custom)(ALWAN_CORE_XYZ xyz, ALWAN_CORE_XYZ xyz_n) {
    ALWAN_CORE_PROLAB result;

    ALWAN_CORE_T const eps = ALWAN_CORE_LITERAL(1e-10);
    ALWAN_CORE_T xn = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(xyz_n.x) > eps, xyz_n.x, eps);
    ALWAN_CORE_T yn = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(xyz_n.y) > eps, xyz_n.y, eps);
    ALWAN_CORE_T zn = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(xyz_n.z) > eps, xyz_n.z, eps);

    ALWAN_CORE_VEC3 xyz_rel;
    xyz_rel.v[0] = xyz.x / xn;
    xyz_rel.v[1] = xyz.y / yn;
    xyz_rel.v[2] = xyz.z / zn;

    ALWAN_CORE_VEC3 prolab_vec = ALWAN_CORE_FNVLIT(alwan_apply_projective)(ALWAN_CORE_FNLIT(ALWAN_PROLAB_MATRIX_Q), xyz_rel);
    result.L = prolab_vec.v[0];
    result.a = prolab_vec.v[1];
    result.b = prolab_vec.v[2];

    return result;
}

/* ----------------------------------------------------------------
 * ProLab -> XYZ, custom illuminant (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_prolab_to_xyz_custom)(ALWAN_CORE_PROLAB prolab, ALWAN_CORE_XYZ xyz_n) {
    ALWAN_CORE_XYZ result;

    ALWAN_CORE_VEC3 prolab_vec;
    prolab_vec.v[0] = prolab.L;
    prolab_vec.v[1] = prolab.a;
    prolab_vec.v[2] = prolab.b;

    ALWAN_CORE_VEC3 xyz_rel = ALWAN_CORE_FNVLIT(alwan_apply_projective)(ALWAN_CORE_FNLIT(ALWAN_PROLAB_MATRIX_Q_INV), prolab_vec);

    result.x = xyz_rel.v[0] * xyz_n.x;
    result.y = xyz_rel.v[1] * xyz_n.y;
    result.z = xyz_rel.v[2] * xyz_n.z;

    return result;
}
