/*
 * Alwan - Pure C colour science library
 * GENERATED FILE - DO NOT EDIT BY HAND.
 * Source: alwan_dev/gendata/gen_tf_polynomials.py
 *
 * ITU-R BT.2020 / BT.709 OETF/EOTF constants.
 * Used when ALWAN_DETERMINISTIC=1 to avoid libm pow().
 *
 * There are no polynomial coefficients here any more. The power branch
 * goes through alwan_det_pow_pos, which is exp2(e * log2(x)) over the
 * committed minimax tables in alwan_det_math_coeffs.h. Its argument
 * reduction removes the branch point at x = 0 that made the old
 * piecewise fits converge so slowly. Measured against libm in f64:
 *   OETF pow(x, 0.45) on [0.018, 1]: max abs 3.331e-16.
 *   EOTF pow(z, 2.22222) on [0.163785, 1]: max abs 4.441e-16.
 * The tables these replaced sat between 4.8e-08 and 2.5e-04.
 */

#ifndef ALWAN_DET_BT2020_COEFFS_H
#define ALWAN_DET_BT2020_COEFFS_H

#include "../alwan_types.h"

#define ALWAN_DET_BT2020_OETF_BREAK       +1.79999999999999986e-02
#define ALWAN_DET_BT2020_OETF_LINEAR      +4.50000000000000000e+00
#define ALWAN_DET_BT2020_OETF_ALPHA       +1.09899999999999998e+00
#define ALWAN_DET_BT2020_OETF_BETA        +9.89999999999999769e-02  /* alpha - 1 */
#define ALWAN_DET_BT2020_EOTF_BREAK       +8.09999999999999887e-02  /* linear*break_x */

/* Smallest z the EOTF power branch sees; recorded, not used. */
#define ALWAN_DET_BT2020_EOTF_DOMAIN_LO   +1.63785259326660576e-01

#endif /* ALWAN_DET_BT2020_COEFFS_H */
