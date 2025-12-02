/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ProLab Color Space (Perceptually Uniform Projective)
 *
 * Reference: Konovalenko et al. (2021)
 * "ProLab: A Perceptually Uniform Projective Color Coordinate System"
 * arXiv:2012.07653
 * https://github.com/konovalenko-iitp/proLab
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ----------------------------------------------------------------
 * ProLab Constants (Konovalenko 2021)
 * Generated from colour-science: colour.models.prolab
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* Projective transformation matrix Q (4x4 homogeneous coordinates)
 * Normalised cone responses to CIE XYZ tristimulus values */
static alwan_scalar const MATRIX_Q[16] = {
#include "data/prolab_matrix_q.csv"
};

/* Inverse projective transformation matrix Q^-1 (precomputed for efficiency)
 * From colour-science: colour.models.prolab.MATRIX_INVERSE_Q */
static alwan_scalar const MATRIX_INVERSE_Q[16] = {
#include "data/prolab_matrix_q_inv.csv"
};

/* D65 reference white XYZ (Y=1 normalized)
 * From colour-science: CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65'] */
static alwan_scalar const D65_WHITE_XYZ[3] = {
#include "data/white_d65_xyz_y1.csv"
};

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Helper: Projective Transformation (4x4 matrix × 3D point)
 * ---------------------------------------------------------------- */

static void apply_projective_transform(alwan_scalar const *matrix,
                                        alwan_vec3 const *input,
                                        alwan_vec3 *output) {
    /* Convert to homogeneous coordinates [x, y, z, 1] */
    alwan_scalar h[4];
    h[0] = input->v[0];
    h[1] = input->v[1];
    h[2] = input->v[2];
    h[3] = ALWAN_LITERAL(1.0);

    /* Apply 4x4 matrix transformation */
    alwan_scalar result[4];
    for (int i = 0; i < 4; i++) {
        result[i] = matrix[i * 4 + 0] * h[0] +
                    matrix[i * 4 + 1] * h[1] +
                    matrix[i * 4 + 2] * h[2] +
                    matrix[i * 4 + 3] * h[3];
    }

    /* Normalize by homogeneous coordinate w (result[3]) */
    alwan_scalar w = result[3];
    if (ALWAN_FABS(w) < ALWAN_LITERAL(1e-10)) {
        /* Avoid division by zero - return input as fallback */
        output->v[0] = input->v[0];
        output->v[1] = input->v[1];
        output->v[2] = input->v[2];
        return;
    }

    output->v[0] = result[0] / w;
    output->v[1] = result[1] / w;
    output->v[2] = result[2] / w;
}

/* ----------------------------------------------------------------
 * XYZ <-> ProLab (D65 Illuminant)
 * ---------------------------------------------------------------- */

void alwan_xyz_to_prolab(alwan_xyz const *xyz, alwan_prolab *prolab) {
    /* Step 1: Normalize XYZ by D65 reference white (relative XYZ) */
    alwan_vec3 xyz_relative;
    xyz_relative.v[0] = xyz->x / D65_WHITE_XYZ[0];
    xyz_relative.v[1] = xyz->y / D65_WHITE_XYZ[1];
    xyz_relative.v[2] = xyz->z / D65_WHITE_XYZ[2];

    /* Step 2: Apply projective transformation with MATRIX_Q */
    alwan_vec3 prolab_vec;
    apply_projective_transform(MATRIX_Q, &xyz_relative, &prolab_vec);
    prolab->L = prolab_vec.v[0];
    prolab->a = prolab_vec.v[1];
    prolab->b = prolab_vec.v[2];
}

void alwan_prolab_to_xyz(alwan_prolab const *prolab, alwan_xyz *xyz) {
    /* Step 1: Apply inverse projective transformation with MATRIX_INVERSE_Q */
    alwan_vec3 prolab_vec;
    prolab_vec.v[0] = prolab->L;
    prolab_vec.v[1] = prolab->a;
    prolab_vec.v[2] = prolab->b;

    alwan_vec3 xyz_relative;
    apply_projective_transform(MATRIX_INVERSE_Q, &prolab_vec, &xyz_relative);

    /* Step 2: Denormalize by D65 reference white */
    xyz->x = xyz_relative.v[0] * D65_WHITE_XYZ[0];
    xyz->y = xyz_relative.v[1] * D65_WHITE_XYZ[1];
    xyz->z = xyz_relative.v[2] * D65_WHITE_XYZ[2];
}

/* ----------------------------------------------------------------
 * XYZ <-> ProLab (Custom Illuminant)
 * ---------------------------------------------------------------- */

void alwan_xyz_to_prolab_custom(alwan_xyz const *xyz,
                                 alwan_prolab *prolab,
                                 alwan_xyz const *xyz_n) {
    /* Step 1: Normalize XYZ by custom reference white */
    alwan_vec3 xyz_relative;
    xyz_relative.v[0] = xyz->x / xyz_n->x;
    xyz_relative.v[1] = xyz->y / xyz_n->y;
    xyz_relative.v[2] = xyz->z / xyz_n->z;

    /* Step 2: Apply projective transformation */
    alwan_vec3 prolab_vec;
    apply_projective_transform(MATRIX_Q, &xyz_relative, &prolab_vec);
    prolab->L = prolab_vec.v[0];
    prolab->a = prolab_vec.v[1];
    prolab->b = prolab_vec.v[2];
}

void alwan_prolab_to_xyz_custom(alwan_prolab const *prolab,
                                 alwan_xyz *xyz,
                                 alwan_xyz const *xyz_n) {
    /* Step 1: Apply inverse projective transformation */
    alwan_vec3 prolab_vec;
    prolab_vec.v[0] = prolab->L;
    prolab_vec.v[1] = prolab->a;
    prolab_vec.v[2] = prolab->b;

    alwan_vec3 xyz_relative;
    apply_projective_transform(MATRIX_INVERSE_Q, &prolab_vec, &xyz_relative);

    /* Step 2: Denormalize by custom reference white */
    xyz->x = xyz_relative.v[0] * xyz_n->x;
    xyz->y = xyz_relative.v[1] * xyz_n->y;
    xyz->z = xyz_relative.v[2] * xyz_n->z;
}
