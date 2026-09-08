/*
 * Alwan - Pure C colour science library
 * GENERATED FILE - DO NOT EDIT BY HAND.
 * Source: alwan_dev/gendata/gen_tf_polynomials.py
 *
 * IEC 61966-2-1 sRGB OETF/EOTF constants.
 * Used when ALWAN_DETERMINISTIC=1 to avoid libm pow().
 *
 * There are no polynomial coefficients here any more. The power branch
 * goes through alwan_det_pow_pos, which is exp2(e * log2(x)) over the
 * committed minimax tables in alwan_det_math_coeffs.h. Its argument
 * reduction removes the branch point at x = 0 that made the old
 * piecewise fits converge so slowly. Measured against libm in f64:
 *   OETF pow(x, 0.416667) on [0.0031308, 1]: max abs 3.331e-16.
 *   EOTF pow(z, 2.4) on [0.090474, 1]: max abs 4.441e-16.
 * The tables these replaced sat between 4.8e-08 and 2.5e-04.
 */

#ifndef ALWAN_DET_SRGB_COEFFS_H
#define ALWAN_DET_SRGB_COEFFS_H

#include "../alwan_types.h"

#define ALWAN_DET_SRGB_OETF_BREAK       +3.13079999999999999e-03
#define ALWAN_DET_SRGB_OETF_LINEAR      +1.29199999999999999e+01
#define ALWAN_DET_SRGB_OETF_ALPHA       +1.05499999999999994e+00
#define ALWAN_DET_SRGB_OETF_BETA        +5.49999999999999378e-02  /* alpha - 1 */
#define ALWAN_DET_SRGB_EOTF_BREAK       +4.04499359999999988e-02  /* linear*break_x */

/* Smallest z the EOTF power branch sees; recorded, not used. */
#define ALWAN_DET_SRGB_EOTF_DOMAIN_LO   +9.04738729857819252e-02

#endif /* ALWAN_DET_SRGB_COEFFS_H */
