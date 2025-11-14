/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * P1.1: Oklab & Oklch Color Spaces
 *
 * Reference: Björn Ottosson (2020)
 * "A perceptual color space for image processing"
 * https://bottosson.github.io/posts/oklab/
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ----------------------------------------------------------------
 * Oklab Transformation Matrices
 * ---------------------------------------------------------------- */

/* XYZ (D65) to LMS cone response matrix (M1) */
static alwan_scalar const M1[9] = {
    ALWAN_LITERAL(+0.8189330101), ALWAN_LITERAL(+0.3618667424), ALWAN_LITERAL(-0.1288597137),
    ALWAN_LITERAL(+0.0329845436), ALWAN_LITERAL(+0.9293118715), ALWAN_LITERAL(+0.0361456387),
    ALWAN_LITERAL(+0.0482003018), ALWAN_LITERAL(+0.2643662691), ALWAN_LITERAL(+0.6338517070)
};

/* LMS' (perceptual) to Lab matrix (M2) */
static alwan_scalar const M2[9] = {
    ALWAN_LITERAL(+0.2104542553), ALWAN_LITERAL(+0.7936177850), ALWAN_LITERAL(-0.0040720468),
    ALWAN_LITERAL(+1.9779984951), ALWAN_LITERAL(-2.4285922050), ALWAN_LITERAL(+0.4505937099),
    ALWAN_LITERAL(+0.0259040371), ALWAN_LITERAL(+0.7827717662), ALWAN_LITERAL(-0.8086757660)
};

/* LMS to XYZ inverse matrix (M1_inv) */
static alwan_scalar const M1_inv[9] = {
    ALWAN_LITERAL(+1.2270138511), ALWAN_LITERAL(-0.5577999807), ALWAN_LITERAL(+0.2812561490),
    ALWAN_LITERAL(-0.0405801784), ALWAN_LITERAL(+1.1122568696), ALWAN_LITERAL(-0.0716766787),
    ALWAN_LITERAL(-0.0763812845), ALWAN_LITERAL(-0.4214819784), ALWAN_LITERAL(+1.5861632204)
};

/* Lab to LMS' inverse matrix (M2_inv) */
static alwan_scalar const M2_inv[9] = {
    ALWAN_LITERAL(+1.0000000000), ALWAN_LITERAL(+0.3963377774), ALWAN_LITERAL(+0.2158037573),
    ALWAN_LITERAL(+1.0000000000), ALWAN_LITERAL(-0.1055613458), ALWAN_LITERAL(-0.0638541728),
    ALWAN_LITERAL(+1.0000000000), ALWAN_LITERAL(-0.0894841775), ALWAN_LITERAL(-1.2914855480)
};

/* ----------------------------------------------------------------
 * XYZ <-> Oklab
 * ---------------------------------------------------------------- */

void alwan_xyz_to_oklab(alwan_vec3 const *xyz, alwan_vec3 *oklab) {
    /* Step 1: XYZ (D65) → LMS (cone response) */
    alwan_vec3 lms;
    lms.v[0] = M1[0] * xyz->v[0] + M1[1] * xyz->v[1] + M1[2] * xyz->v[2];
    lms.v[1] = M1[3] * xyz->v[0] + M1[4] * xyz->v[1] + M1[5] * xyz->v[2];
    lms.v[2] = M1[6] * xyz->v[0] + M1[7] * xyz->v[1] + M1[8] * xyz->v[2];

    /* Step 2: LMS → LMS' (perceptual, cube root) */
    alwan_vec3 lms_p;
    lms_p.v[0] = ALWAN_CBRT(lms.v[0]);
    lms_p.v[1] = ALWAN_CBRT(lms.v[1]);
    lms_p.v[2] = ALWAN_CBRT(lms.v[2]);

    /* Step 3: LMS' → Lab */
    oklab->v[0] = M2[0] * lms_p.v[0] + M2[1] * lms_p.v[1] + M2[2] * lms_p.v[2];
    oklab->v[1] = M2[3] * lms_p.v[0] + M2[4] * lms_p.v[1] + M2[5] * lms_p.v[2];
    oklab->v[2] = M2[6] * lms_p.v[0] + M2[7] * lms_p.v[1] + M2[8] * lms_p.v[2];
}

void alwan_oklab_to_xyz(alwan_vec3 const *oklab, alwan_vec3 *xyz) {
    /* Step 1: Lab → LMS' (perceptual) */
    alwan_vec3 lms_p;
    lms_p.v[0] = M2_inv[0] * oklab->v[0] + M2_inv[1] * oklab->v[1] + M2_inv[2] * oklab->v[2];
    lms_p.v[1] = M2_inv[3] * oklab->v[0] + M2_inv[4] * oklab->v[1] + M2_inv[5] * oklab->v[2];
    lms_p.v[2] = M2_inv[6] * oklab->v[0] + M2_inv[7] * oklab->v[1] + M2_inv[8] * oklab->v[2];

    /* Step 2: LMS' → LMS (cube) */
    alwan_vec3 lms;
    lms.v[0] = lms_p.v[0] * lms_p.v[0] * lms_p.v[0];
    lms.v[1] = lms_p.v[1] * lms_p.v[1] * lms_p.v[1];
    lms.v[2] = lms_p.v[2] * lms_p.v[2] * lms_p.v[2];

    /* Step 3: LMS → XYZ (D65) */
    xyz->v[0] = M1_inv[0] * lms.v[0] + M1_inv[1] * lms.v[1] + M1_inv[2] * lms.v[2];
    xyz->v[1] = M1_inv[3] * lms.v[0] + M1_inv[4] * lms.v[1] + M1_inv[5] * lms.v[2];
    xyz->v[2] = M1_inv[6] * lms.v[0] + M1_inv[7] * lms.v[1] + M1_inv[8] * lms.v[2];
}

/* ----------------------------------------------------------------
 * Oklab <-> Oklch (Cylindrical)
 * ---------------------------------------------------------------- */

void alwan_oklab_to_oklch(alwan_vec3 const *oklab, alwan_vec3 *oklch) {
    /* L stays the same */
    oklch->v[0] = oklab->v[0];

    /* Chroma = sqrt(a^2 + b^2) */
    oklch->v[1] = ALWAN_SQRT(oklab->v[1] * oklab->v[1] + oklab->v[2] * oklab->v[2]);

    /* Hue = atan2(b, a) in radians */
    oklch->v[2] = ALWAN_ATAN2(oklab->v[2], oklab->v[1]);
}

void alwan_oklch_to_oklab(alwan_vec3 const *oklch, alwan_vec3 *oklab) {
    /* L stays the same */
    oklab->v[0] = oklch->v[0];

    /* a = C * cos(h) */
    oklab->v[1] = oklch->v[1] * ALWAN_COS(oklch->v[2]);

    /* b = C * sin(h) */
    oklab->v[2] = oklch->v[1] * ALWAN_SIN(oklch->v[2]);
}
