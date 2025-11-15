/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * 3x3 Matrix Operations
 * Matrix layout (row-major): [m00 m01 m02 m10 m11 m12 m20 m21 m22]
 * Index: m->m[row*3 + col]
 * ---------------------------------------------------------------- */

void alwan_mat3_identity(alwan_mat3x3 *out) {
    memset(out->m, 0, sizeof(out->m));
    out->m[0] = out->m[4] = out->m[8] = ALWAN_LITERAL(1.0);
}

void alwan_mat3_mul(alwan_mat3x3 const *a, alwan_mat3x3 const *b, alwan_mat3x3 *out) {
    alwan_mat3x3 tmp;

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            alwan_scalar sum = 0;
            for (int k = 0; k < 3; k++) {
                sum += a->m[row * 3 + k] * b->m[k * 3 + col];
            }
            tmp.m[row * 3 + col] = sum;
        }
    }

    memcpy(out, &tmp, sizeof(alwan_mat3x3));
}

void alwan_mat3_mulv(alwan_mat3x3 const *m, alwan_vec3 const *v, alwan_vec3 *out) {
    alwan_vec3 tmp;

    for (int row = 0; row < 3; row++) {
        tmp.v[row] = m->m[row * 3 + 0] * v->v[0] +
                     m->m[row * 3 + 1] * v->v[1] +
                     m->m[row * 3 + 2] * v->v[2];
    }

    memcpy(out, &tmp, sizeof(alwan_vec3));
}

/* ----------------------------------------------------------------
 * 3x3 Matrix Inversion using Partial-Pivot Gaussian Elimination
 * ---------------------------------------------------------------- */

int alwan_mat3_inv(alwan_mat3x3 const *m, alwan_mat3x3 *out) {
    /* Create augmented matrix [M | I] */
    alwan_scalar aug[3][6];

    /* Initialize with input matrix on left, identity on right */
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            aug[row][col] = m->m[row * 3 + col];
        }
        aug[row][3] = (row == 0) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(0.0);
        aug[row][4] = (row == 1) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(0.0);
        aug[row][5] = (row == 2) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(0.0);
    }

    /* Forward elimination with partial pivoting */
    for (int col = 0; col < 3; col++) {
        /* Find pivot row */
        int pivot_row = col;
        alwan_scalar max_val = ALWAN_FABS(aug[col][col]);

        for (int row = col + 1; row < 3; row++) {
            alwan_scalar val = ALWAN_FABS(aug[row][col]);
            if (val > max_val) {
                max_val = val;
                pivot_row = row;
            }
        }

        /* Check for singularity */
        if (max_val < ALWAN_EPSILON) {
            return ALWAN_E_RANGE;  /* Matrix is singular or near-singular */
        }

        /* Swap rows if needed */
        if (pivot_row != col) {
            for (int k = 0; k < 6; k++) {
                alwan_scalar tmp = aug[col][k];
                aug[col][k] = aug[pivot_row][k];
                aug[pivot_row][k] = tmp;
            }
        }

        /* Scale pivot row */
        alwan_scalar pivot = aug[col][col];
        for (int k = 0; k < 6; k++) {
            aug[col][k] /= pivot;
        }

        /* Eliminate column in rows below */
        for (int row = col + 1; row < 3; row++) {
            alwan_scalar factor = aug[row][col];
            for (int k = 0; k < 6; k++) {
                aug[row][k] -= factor * aug[col][k];
            }
        }
    }

    /* Back substitution */
    for (int col = 2; col >= 0; col--) {
        for (int row = col - 1; row >= 0; row--) {
            alwan_scalar factor = aug[row][col];
            for (int k = 0; k < 6; k++) {
                aug[row][k] -= factor * aug[col][k];
            }
        }
    }

    /* Extract inverse from right side of augmented matrix */
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            out->m[row * 3 + col] = aug[row][col + 3];
        }
    }

    return ALWAN_OK;
}
