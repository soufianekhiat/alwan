/*
 * Alwan - Pure C colour science library
 * GENERATED FILE - DO NOT EDIT BY HAND.
 * Source: alwan_dev/gendata/gen_math_polynomials.py
 *
 * Deterministic-math polynomial coefficients. Used by
 * core/alwan_deterministic.h to build alwan_det_log2, alwan_det_exp2,
 * and alwan_det_pow_pos when ALWAN_DETERMINISTIC=1.
 *
 * Coefficients are stored in NORMALIZED basis: each domain [lo, hi]
 * is mapped to u in [-1, 1] before Horner evaluation.
 *
 * log2(m) on [0.5, 1.0]: degree 14, max abs 2.567e-12.
 * exp2(t) on [0.0, 1.0]: degree 10, max abs 4.219e-15.
 */

#ifndef ALWAN_DET_MATH_COEFFS_H
#define ALWAN_DET_MATH_COEFFS_H

#include "../alwan_types.h"

#define ALWAN_DET_LOG2_DEGREE 14
#define ALWAN_DET_EXP2_DEGREE 10

/* log2(m), m in [0.5, 1.0]; u = 2*m - 1 in [-1, 1]. */
#if ALWAN_BACKEND == ALWAN_BACKEND_C  /* f64 table: C-only (GPU is single precision) */
static const alwan_f64 alwan_det_log2_coeffs_f64[15] = {
    -4.15037499278775868e-01,  /* c0 */
    +4.80898346969580948e-01,  /* c1 */
    -8.01497245031076305e-02,  /* c2 */
    +1.78110496280567479e-02,  /* c3 */
    -4.45276226790181258e-03,  /* c4 */
    +1.18740625250951526e-03,  /* c5 */
    -3.29835951132871142e-04,  /* c6 */
    +9.42239391167798472e-05,  /* c7 */
    -2.74793397091190119e-05,  /* c8 */
    +8.18011163749353180e-06,  /* c9 */
    -2.45808477204246181e-06,  /* c10 */
    +6.92944351503917917e-07,  /* c11 */
    -2.08664225899635272e-07,  /* c12 */
    +1.00152545440049605e-07,  /* c13 */
    -3.19100097610913222e-08,  /* c14 */
};
#endif

static const alwan_f32 alwan_det_log2_coeffs_f32[15] = {
    -4.15037499278775868e-01f,  /* c0 */
    +4.80898346969580948e-01f,  /* c1 */
    -8.01497245031076305e-02f,  /* c2 */
    +1.78110496280567479e-02f,  /* c3 */
    -4.45276226790181258e-03f,  /* c4 */
    +1.18740625250951526e-03f,  /* c5 */
    -3.29835951132871142e-04f,  /* c6 */
    +9.42239391167798472e-05f,  /* c7 */
    -2.74793397091190119e-05f,  /* c8 */
    +8.18011163749353180e-06f,  /* c9 */
    -2.45808477204246181e-06f,  /* c10 */
    +6.92944351503917917e-07f,  /* c11 */
    -2.08664225899635272e-07f,  /* c12 */
    +1.00152545440049605e-07f,  /* c13 */
    -3.19100097610913222e-08f,  /* c14 */
};

/* exp2(t), t in [0.0, 1.0]; u = 2*t - 1 in [-1, 1]. */
#if ALWAN_BACKEND == ALWAN_BACKEND_C  /* f64 table: C-only (GPU is single precision) */
static const alwan_f64 alwan_det_exp2_coeffs_f64[11] = {
    +1.41421356237309759e+00,  /* c0 */
    +4.90129071734276667e-01,  /* c1 */
    +8.49328960457299831e-02,  /* c2 */
    +9.81183290511380458e-03,  /* c3 */
    +8.50130539641997043e-04,  /* c4 */
    +5.89265588705293000e-05,  /* c5 */
    +3.40373051691460020e-06,  /* c6 */
    +1.68519875559771040e-07,  /* c7 */
    +7.30165370766921441e-09,  /* c8 */
    +2.81861472178929525e-10,  /* c9 */
    +9.35977786593401558e-12,  /* c10 */
};
#endif

static const alwan_f32 alwan_det_exp2_coeffs_f32[11] = {
    +1.41421356237309759e+00f,  /* c0 */
    +4.90129071734276667e-01f,  /* c1 */
    +8.49328960457299831e-02f,  /* c2 */
    +9.81183290511380458e-03f,  /* c3 */
    +8.50130539641997043e-04f,  /* c4 */
    +5.89265588705293000e-05f,  /* c5 */
    +3.40373051691460020e-06f,  /* c6 */
    +1.68519875559771040e-07f,  /* c7 */
    +7.30165370766921441e-09f,  /* c8 */
    +2.81861472178929525e-10f,  /* c9 */
    +9.35977786593401558e-12f,  /* c10 */
};

#endif /* ALWAN_DET_MATH_COEFFS_H */
