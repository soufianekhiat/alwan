/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * P1.8: ProLab Color Space (Perceptually Uniform Projective)
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
 * ---------------------------------------------------------------- */

/* Projective transformation matrix Q (4x4 homogeneous coordinates)
 * Normalised cone responses to CIE XYZ tristimulus values */
static alwan_scalar const MATRIX_Q[16] = {
    ALWAN_LITERAL(75.54),    ALWAN_LITERAL(486.66),   ALWAN_LITERAL(167.39),   ALWAN_LITERAL(0.0),
    ALWAN_LITERAL(617.72),   ALWAN_LITERAL(-595.45),  ALWAN_LITERAL(-22.27),   ALWAN_LITERAL(0.0),
    ALWAN_LITERAL(48.34),    ALWAN_LITERAL(194.94),   ALWAN_LITERAL(-243.28),  ALWAN_LITERAL(0.0),
    ALWAN_LITERAL(0.7554),   ALWAN_LITERAL(3.8666),   ALWAN_LITERAL(1.6739),   ALWAN_LITERAL(1.0)
};

/* Inverse projective transformation matrix Q^-1 (precomputed for efficiency) */
static alwan_scalar const MATRIX_INVERSE_Q[16] = {
    ALWAN_LITERAL( 0.0013378602649006),   ALWAN_LITERAL( 0.0009782456140351),  ALWAN_LITERAL( 0.0007290529949745),  ALWAN_LITERAL(-0.0024451588739101),
    ALWAN_LITERAL( 0.0010597944259478),   ALWAN_LITERAL(-0.0000179577699177),  ALWAN_LITERAL( 0.0002529424837956),  ALWAN_LITERAL(-0.0012947791398256),
    ALWAN_LITERAL( 0.0020485225089390),   ALWAN_LITERAL( 0.0018639467779425),  ALWAN_LITERAL(-0.0018862050941859),  ALWAN_LITERAL(-0.0020262642926956),
    ALWAN_LITERAL(-0.0010107253464270),  ALWAN_LITERAL(-0.0015164185350877),  ALWAN_LITERAL(-0.0006203389830509),  ALWAN_LITERAL( 0.0031474828645656)
};

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
    if (ALWAN_ABS(w) < ALWAN_LITERAL(1e-10)) {
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

void alwan_xyz_to_prolab(alwan_vec3 const *xyz, alwan_vec3 *prolab) {
    /* D65 reference white (Y = 100) */
    alwan_scalar const xn = ALWAN_LITERAL(95.047);
    alwan_scalar const yn = ALWAN_LITERAL(100.0);
    alwan_scalar const zn = ALWAN_LITERAL(108.883);

    /* Step 1: Normalize XYZ by reference white (relative XYZ) */
    alwan_vec3 xyz_relative;
    xyz_relative.v[0] = xyz->v[0] / xn;
    xyz_relative.v[1] = xyz->v[1] / yn;
    xyz_relative.v[2] = xyz->v[2] / zn;

    /* Step 2: Apply projective transformation with MATRIX_Q */
    apply_projective_transform(MATRIX_Q, &xyz_relative, prolab);
}

void alwan_prolab_to_xyz(alwan_vec3 const *prolab, alwan_vec3 *xyz) {
    /* D65 reference white (Y = 100) */
    alwan_scalar const xn = ALWAN_LITERAL(95.047);
    alwan_scalar const yn = ALWAN_LITERAL(100.0);
    alwan_scalar const zn = ALWAN_LITERAL(108.883);

    /* Step 1: Apply inverse projective transformation with MATRIX_INVERSE_Q */
    alwan_vec3 xyz_relative;
    apply_projective_transform(MATRIX_INVERSE_Q, prolab, &xyz_relative);

    /* Step 2: Denormalize by reference white */
    xyz->v[0] = xyz_relative.v[0] * xn;
    xyz->v[1] = xyz_relative.v[1] * yn;
    xyz->v[2] = xyz_relative.v[2] * zn;
}

/* ----------------------------------------------------------------
 * XYZ <-> ProLab (Custom Illuminant)
 * ---------------------------------------------------------------- */

void alwan_xyz_to_prolab_custom(alwan_vec3 const *xyz,
                                 alwan_vec3 *prolab,
                                 alwan_vec3 const *xyz_n) {
    /* Step 1: Normalize XYZ by custom reference white */
    alwan_vec3 xyz_relative;
    xyz_relative.v[0] = xyz->v[0] / xyz_n->v[0];
    xyz_relative.v[1] = xyz->v[1] / xyz_n->v[1];
    xyz_relative.v[2] = xyz->v[2] / xyz_n->v[2];

    /* Step 2: Apply projective transformation */
    apply_projective_transform(MATRIX_Q, &xyz_relative, prolab);
}

void alwan_prolab_to_xyz_custom(alwan_vec3 const *prolab,
                                 alwan_vec3 *xyz,
                                 alwan_vec3 const *xyz_n) {
    /* Step 1: Apply inverse projective transformation */
    alwan_vec3 xyz_relative;
    apply_projective_transform(MATRIX_INVERSE_Q, prolab, &xyz_relative);

    /* Step 2: Denormalize by custom reference white */
    xyz->v[0] = xyz_relative.v[0] * xyz_n->v[0];
    xyz->v[1] = xyz_relative.v[1] * xyz_n->v[1];
    xyz->v[2] = xyz_relative.v[2] * xyz_n->v[2];
}
