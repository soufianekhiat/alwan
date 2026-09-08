/*
 * Alwan - Pure C colour science library
 * GENERATED FILE - DO NOT EDIT BY HAND.
 * Source: alwan_dev/gendata/gen_trig_polynomials.py
 * Fits:   alwan_dev/gendata/gen_trig_minimax.wls (Wolfram Remez, 60 digits)
 *
 * Deterministic trigonometry coefficients, used by core/alwan_deterministic.h
 * to build alwan_det_sin / _cos / _tan / _atan / _atan2 / _acos when
 * ALWAN_DETERMINISTIC=1. log10 and tanh need no table: they are exact
 * identities over the log2/exp2 pair in alwan_det_math_coeffs.h.
 *
 * Parity is factored out, so the polynomials are in y = r*r and the odd
 * functions carry their argument as an explicit factor:
 *
 *   sin(r)  = r * P(r*r)   P degree 7, minimax 1.880e-21 over [0, (pi/4)^2]
 *   cos(r)  =     Q(r*r)   Q degree 8, minimax 1.808e-23 over [0, (pi/4)^2]
 *   atan(s) = s * R(s*s)   R degree 13, minimax 1.629e-21 over [0, tan(pi/8)^2]
 *
 * That makes sin(0) = 0 and atan(0) = 0 exact rather than approximate, and
 * puts every fit three or more orders below f64 epsilon, so the realised
 * accuracy is set by Horner rounding and not by the approximation:
 * REALISED_LINE
 *
 * Unlike alwan_det_math_coeffs.h these are NOT in a [-1, 1] normalised
 * basis. Normalising would reintroduce a constant term and lose the exact
 * zero at the origin, which is the whole point of factoring out parity.
 */

#ifndef ALWAN_DET_TRIG_COEFFS_H
#define ALWAN_DET_TRIG_COEFFS_H

#include "../alwan_types.h"

#define ALWAN_DET_SIN_DEGREE 7
#define ALWAN_DET_COS_DEGREE 8
#define ALWAN_DET_ATAN_DEGREE 13

/* Cody-Waite pi/2, three parts. hi and mid carry 33 significant bits each,
 * so n * hi and n * mid are exact for the |n| an angle reduction produces,
 * and the rounding error lands in lo where it cannot cancel. */
#define ALWAN_DET_PIO2_HI  +1.57079505920410156e+00
#define ALWAN_DET_PIO2_MID +1.26759005070198327e-06
#define ALWAN_DET_PIO2_LO  +7.44354748048662325e-13

/* The f32 twin of the same split. Casting the f64 parts to float would
 * round away the extra precision the split exists to carry, so single
 * precision gets its own 12-bit-truncated parts. */
#define ALWAN_DET_PIO2_HI_F32  +1.57031250000000000e+00f
#define ALWAN_DET_PIO2_MID_F32 +4.83751296997070312e-04f
#define ALWAN_DET_PIO2_LO_F32  +7.54979012640433211e-08f

/* 2/pi, for the quadrant count. */
#define ALWAN_DET_TWO_OVER_PI +6.36619772367581382e-01

/* Angles for the atan and atan2 quadrant assembly. */
#define ALWAN_DET_PI      +3.14159265358979312e+00
#define ALWAN_DET_PIO2    +1.57079632679489656e+00
#define ALWAN_DET_PIO4    +7.85398163397448279e-01
#define ALWAN_DET_TAN_PI_8 +4.14213562373095034e-01

/* log10(2), the one constant log10 needs on top of log2. */
#define ALWAN_DET_LOG10_2 +3.01029995663981198e-01

/* sin(r) / r as a polynomial in y = r*r, r in [-pi/4, pi/4]. */
static const alwan_f64 alwan_det_sin_coeffs_f64[8] = {
    +1.00000000000000000e+00,  /* c0 */
    -1.66666666666666657e-01,  /* c1 */
    +8.33333333333332107e-03,  /* c2 */
    -1.98412698412531066e-04,  /* c3 */
    +2.75573192133904082e-06,  /* c4 */
    -2.50521047383288664e-08,  /* c5 */
    +1.60583476309273415e-10,  /* c6 */
    -7.57786825836233383e-13,  /* c7 */
};

static const alwan_f32 alwan_det_sin_coeffs_f32[8] = {
    +1.00000000000000000e+00f,  /* c0 */
    -1.66666666666666657e-01f,  /* c1 */
    +8.33333333333332107e-03f,  /* c2 */
    -1.98412698412531066e-04f,  /* c3 */
    +2.75573192133904082e-06f,  /* c4 */
    -2.50521047383288664e-08f,  /* c5 */
    +1.60583476309273415e-10f,  /* c6 */
    -7.57786825836233383e-13f,  /* c7 */
};

/* cos(r) as a polynomial in y = r*r, r in [-pi/4, pi/4]. */
static const alwan_f64 alwan_det_cos_coeffs_f64[9] = {
    +1.00000000000000000e+00,  /* c0 */
    -5.00000000000000000e-01,  /* c1 */
    +4.16666666666666644e-02,  /* c2 */
    -1.38888888888888569e-03,  /* c3 */
    +2.48015873015616331e-05,  /* c4 */
    -2.75573192121557126e-07,  /* c5 */
    +2.08767537775228854e-09,  /* c6 */
    -1.14702367856243792e-11,  /* c7 */
    +4.73589391497267162e-14,  /* c8 */
};

static const alwan_f32 alwan_det_cos_coeffs_f32[9] = {
    +1.00000000000000000e+00f,  /* c0 */
    -5.00000000000000000e-01f,  /* c1 */
    +4.16666666666666644e-02f,  /* c2 */
    -1.38888888888888569e-03f,  /* c3 */
    +2.48015873015616331e-05f,  /* c4 */
    -2.75573192121557126e-07f,  /* c5 */
    +2.08767537775228854e-09f,  /* c6 */
    -1.14702367856243792e-11f,  /* c7 */
    +4.73589391497267162e-14f,  /* c8 */
};

/* atan(s) / s as a polynomial in y = s*s, |s| <= tan(pi/8). */
static const alwan_f64 alwan_det_atan_coeffs_f64[14] = {
    +1.00000000000000000e+00,  /* c0 */
    -3.33333333333333315e-01,  /* c1 */
    +1.99999999999998568e-01,  /* c2 */
    -1.42857142856927355e-01,  /* c3 */
    +1.11111111094163106e-01,  /* c4 */
    -9.09090901093237702e-02,  /* c5 */
    +7.69230524166633345e-02,  /* c6 */
    -6.66661552879408148e-02,  /* c7 */
    +5.88160567086728828e-02,  /* c8 */
    -5.25540729500353632e-02,  /* c9 */
    +4.70479307804786001e-02,  /* c10 */
    -4.05239072112779306e-02,  /* c11 */
    +2.95583168774332383e-02,  /* c12 */
    -1.29338319364224689e-02,  /* c13 */
};

static const alwan_f32 alwan_det_atan_coeffs_f32[14] = {
    +1.00000000000000000e+00f,  /* c0 */
    -3.33333333333333315e-01f,  /* c1 */
    +1.99999999999998568e-01f,  /* c2 */
    -1.42857142856927355e-01f,  /* c3 */
    +1.11111111094163106e-01f,  /* c4 */
    -9.09090901093237702e-02f,  /* c5 */
    +7.69230524166633345e-02f,  /* c6 */
    -6.66661552879408148e-02f,  /* c7 */
    +5.88160567086728828e-02f,  /* c8 */
    -5.25540729500353632e-02f,  /* c9 */
    +4.70479307804786001e-02f,  /* c10 */
    -4.05239072112779306e-02f,  /* c11 */
    +2.95583168774332383e-02f,  /* c12 */
    -1.29338319364224689e-02f,  /* c13 */
};

#endif /* ALWAN_DET_TRIG_COEFFS_H */
