/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Oklab & Oklch Color Spaces
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Björn Ottosson (2020)
 */

#ifndef ALWAN_OKLAB_CORE_H
#define ALWAN_OKLAB_CORE_H

#include "alwan_platform.h"
#include "alwan_types.h"

/* ----------------------------------------------------------------
 * Oklab Transformation Matrices
 * ---------------------------------------------------------------- */

/* XYZ (D65) to LMS cone response matrix (M1) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const OKLAB_M1[9] = {
#include "data/matrices/oklab_m1.csv"
};
static alwan_scalar const OKLAB_M2[9] = {
#include "data/matrices/oklab_m2.csv"
};
static alwan_scalar const OKLAB_M1_INV[9] = {
#include "data/matrices/oklab_m1_inv.csv"
};
static alwan_scalar const OKLAB_M2_INV[9] = {
#include "data/matrices/oklab_m2_inv.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * XYZ -> Oklab (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_oklab alwan_xyz_to_oklab_v(alwan_xyz xyz) {
    alwan_oklab result;

    /* Step 1: XYZ -> LMS via M1 matrix */
    alwan_scalar l = OKLAB_M1[0] * xyz.x + OKLAB_M1[1] * xyz.y + OKLAB_M1[2] * xyz.z;
    alwan_scalar m = OKLAB_M1[3] * xyz.x + OKLAB_M1[4] * xyz.y + OKLAB_M1[5] * xyz.z;
    alwan_scalar s = OKLAB_M1[6] * xyz.x + OKLAB_M1[7] * xyz.y + OKLAB_M1[8] * xyz.z;

    /* Step 2: LMS -> LMS' via cube root */
    alwan_scalar l_ = ALWAN_CBRT(l);
    alwan_scalar m_ = ALWAN_CBRT(m);
    alwan_scalar s_ = ALWAN_CBRT(s);

    /* Step 3: LMS' -> Lab via M2 matrix */
    result.L = OKLAB_M2[0] * l_ + OKLAB_M2[1] * m_ + OKLAB_M2[2] * s_;
    result.a = OKLAB_M2[3] * l_ + OKLAB_M2[4] * m_ + OKLAB_M2[5] * s_;
    result.b = OKLAB_M2[6] * l_ + OKLAB_M2[7] * m_ + OKLAB_M2[8] * s_;

    return result;
}

/* ----------------------------------------------------------------
 * Oklab -> XYZ (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_xyz alwan_oklab_to_xyz_v(alwan_oklab oklab) {
    alwan_xyz result;

    /* Step 1: Lab -> LMS' via M2_INV matrix */
    alwan_scalar l_ = OKLAB_M2_INV[0] * oklab.L + OKLAB_M2_INV[1] * oklab.a + OKLAB_M2_INV[2] * oklab.b;
    alwan_scalar m_ = OKLAB_M2_INV[3] * oklab.L + OKLAB_M2_INV[4] * oklab.a + OKLAB_M2_INV[5] * oklab.b;
    alwan_scalar s_ = OKLAB_M2_INV[6] * oklab.L + OKLAB_M2_INV[7] * oklab.a + OKLAB_M2_INV[8] * oklab.b;

    /* Step 2: LMS' -> LMS via cube */
    alwan_scalar l = l_ * l_ * l_;
    alwan_scalar m = m_ * m_ * m_;
    alwan_scalar s = s_ * s_ * s_;

    /* Step 3: LMS -> XYZ via M1_INV matrix */
    result.x = OKLAB_M1_INV[0] * l + OKLAB_M1_INV[1] * m + OKLAB_M1_INV[2] * s;
    result.y = OKLAB_M1_INV[3] * l + OKLAB_M1_INV[4] * m + OKLAB_M1_INV[5] * s;
    result.z = OKLAB_M1_INV[6] * l + OKLAB_M1_INV[7] * m + OKLAB_M1_INV[8] * s;

    return result;
}

/* ----------------------------------------------------------------
 * Oklab -> Oklch (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_oklch alwan_oklab_to_oklch_v(alwan_oklab oklab) {
    alwan_oklch result;

    /* L stays same */
    result.L = oklab.L;

    /* C = sqrt(a*a + b*b) */
    result.C = ALWAN_SQRT(oklab.a * oklab.a + oklab.b * oklab.b);

    /* h = atan2(b, a) */
    result.h = ALWAN_ATAN2(oklab.b, oklab.a);

    return result;
}

/* ----------------------------------------------------------------
 * Oklch -> Oklab (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_oklab alwan_oklch_to_oklab_v(alwan_oklch oklch) {
    alwan_oklab result;

    /* L stays same */
    result.L = oklch.L;

    /* a = C * cos(h) */
    result.a = oklch.C * ALWAN_COS(oklch.h);

    /* b = C * sin(h) */
    result.b = oklch.C * ALWAN_SIN(oklch.h);

    return result;
}

#endif /* ALWAN_OKLAB_CORE_H */
