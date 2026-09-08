/*
 * Alwan - Pure C colour science library
 * GENERATED FILE - DO NOT EDIT BY HAND.
 * Source: alwan_dev/gendata/gen_math_polynomials.py
 *
 * Deterministic-math polynomial coefficients. Used by
 * core/alwan_deterministic.h to build alwan_det_log2, alwan_det_exp2,
 * and alwan_det_pow_pos when ALWAN_DETERMINISTIC=1.
 *
 * Fits: alwan_dev/gendata/gen_math_minimax.wls (Wolfram Remez, 60 digits).
 *
 * Coefficients are stored in NORMALIZED basis: each domain [lo, hi]
 * is mapped to u in [-1, 1] before Horner evaluation.
 *
 * log2(m) on [0.5, 1.0], AS (m-1)*P(u): degree 18, max abs 3.331e-16.
 * exp2(t) on [0.0, 1.0]: degree 12, max abs 4.441e-16.
 */

#ifndef ALWAN_DET_MATH_COEFFS_H
#define ALWAN_DET_MATH_COEFFS_H

#include "../alwan_types.h"

#define ALWAN_DET_LOG2_DEGREE 18
#define ALWAN_DET_EXP2_DEGREE 12

/* log2(m) / (m-1), m in [0.5, 1.0]; u = 4*m - 3 in [-1, 1].
 * The caller multiplies by (m - 1). Fitting the quotient is what allows a
 * relative-error Remez here: log2 itself has a zero at m = 1. */
#if ALWAN_BACKEND == ALWAN_BACKEND_C  /* f64 table: C-only (GPU is single precision) */
static const alwan_f64 alwan_det_log2_coeffs_f64[19] = {
    +1.66014999711537525e+00,  /* c0 */
    -2.63443390736583627e-01,  /* c1 */
    +5.71555072387625318e-02,  /* c2 */
    -1.40886923108638484e-02,  /* c3 */
    +3.72235757576061749e-03,  /* c4 */
    -1.02725573515803268e-03,  /* c5 */
    +2.92081307071894575e-04,  /* c6 */
    -8.48720717535879582e-05,  /* c7 */
    +2.50725817739141994e-05,  /* c8 */
    -7.50387700603590157e-06,  /* c9 */
    +2.26939137377925017e-06,  /* c10 */
    -6.91536105985178584e-07,  /* c11 */
    +2.12391313305677313e-07,  /* c12 */
    -6.67615523089693646e-08,  /* c13 */
    +2.08287215527153400e-08,  /* c14 */
    -5.49407965059596627e-09,  /* c15 */
    +1.67890738622649872e-09,  /* c16 */
    -1.03241477943650673e-09,  /* c17 */
    +3.35421444341547214e-10,  /* c18 */
};
#endif

static const alwan_f32 alwan_det_log2_coeffs_f32[19] = {
    +1.66014999711537525e+00f,  /* c0 */
    -2.63443390736583627e-01f,  /* c1 */
    +5.71555072387625318e-02f,  /* c2 */
    -1.40886923108638484e-02f,  /* c3 */
    +3.72235757576061749e-03f,  /* c4 */
    -1.02725573515803268e-03f,  /* c5 */
    +2.92081307071894575e-04f,  /* c6 */
    -8.48720717535879582e-05f,  /* c7 */
    +2.50725817739141994e-05f,  /* c8 */
    -7.50387700603590157e-06f,  /* c9 */
    +2.26939137377925017e-06f,  /* c10 */
    -6.91536105985178584e-07f,  /* c11 */
    +2.12391313305677313e-07f,  /* c12 */
    -6.67615523089693646e-08f,  /* c13 */
    +2.08287215527153400e-08f,  /* c14 */
    -5.49407965059596627e-09f,  /* c15 */
    +1.67890738622649872e-09f,  /* c16 */
    -1.03241477943650673e-09f,  /* c17 */
    +3.35421444341547214e-10f,  /* c18 */
};

/* exp2(t), t in [0.0, 1.0]; u = 2*t - 1 in [-1, 1]. */
#if ALWAN_BACKEND == ALWAN_BACKEND_C  /* f64 table: C-only (GPU is single precision) */
static const alwan_f64 alwan_det_exp2_coeffs_f64[13] = {
    +1.41421356237309515e+00,  /* c0 */
    +4.90129071734273614e-01,  /* c1 */
    +8.49328960457687299e-02,  /* c2 */
    +9.81183290515260688e-03,  /* c3 */
    +8.50130539291461687e-04,  /* c4 */
    +5.89265586416148852e-05,  /* c5 */
    +3.40373149861612354e-06,  /* c6 */
    +1.68520492825650356e-07,  /* c7 */
    +7.30059389106225218e-09,  /* c8 */
    +2.81131611285169193e-10,  /* c9 */
    +9.74340231518465263e-12,  /* c10 */
    +3.07745284490446404e-13,  /* c11 */
    +8.84691901795342263e-15,  /* c12 */
};
#endif

static const alwan_f32 alwan_det_exp2_coeffs_f32[13] = {
    +1.41421356237309515e+00f,  /* c0 */
    +4.90129071734273614e-01f,  /* c1 */
    +8.49328960457687299e-02f,  /* c2 */
    +9.81183290515260688e-03f,  /* c3 */
    +8.50130539291461687e-04f,  /* c4 */
    +5.89265586416148852e-05f,  /* c5 */
    +3.40373149861612354e-06f,  /* c6 */
    +1.68520492825650356e-07f,  /* c7 */
    +7.30059389106225218e-09f,  /* c8 */
    +2.81131611285169193e-10f,  /* c9 */
    +9.74340231518465263e-12f,  /* c10 */
    +3.07745284490446404e-13f,  /* c11 */
    +8.84691901795342263e-15f,  /* c12 */
};

#endif /* ALWAN_DET_MATH_COEFFS_H */
