/*
 * Alwan - Pure C colour science library
 * GENERATED FILE - DO NOT EDIT BY HAND.
 * Source: alwan_dev/gendata/gen_tf_polynomials.py
 *
 * IEC 61966-2-1 sRGB OETF/EOTF polynomial coefficients.
 * Used when ALWAN_DETERMINISTIC=1 to avoid libm pow().
 *
 * Coefficients are stored in the NORMALIZED power basis: each piece's
 * domain [lo, hi] is mapped to u in [-1, 1] before Horner evaluation.
 * Indexed [c0, c1, ..., cN] for Horner:
 *     y = ((cN * u + c(N-1)) * u + c(N-2)) * u + ... + c0
 *
 * OETF (pow(x, 0.416667)): piecewise -- split at 0.05.
 *   lo segment [0.0031308, 0.05]: degree 12, max abs 2.025e-05.
 *   hi segment [0.05, 1.0]:        degree 14, max abs 5.224e-05.
 * EOTF (pow(z, 2.4) with z=(x+0.05499999999999994)/1.055):
 *   z domain [0.090474, 1.0], degree 12, max abs 4.812e-08.
 */

#ifndef ALWAN_DET_SRGB_COEFFS_H
#define ALWAN_DET_SRGB_COEFFS_H

#include "../alwan_types.h"

#define ALWAN_DET_SRGB_OETF_LO_DEGREE   12
#define ALWAN_DET_SRGB_OETF_HI_DEGREE   14
#define ALWAN_DET_SRGB_OETF_BREAK       +3.13079999999999999e-03
#define ALWAN_DET_SRGB_OETF_SPLIT       +5.00000000000000028e-02
#define ALWAN_DET_SRGB_OETF_LINEAR      +1.29199999999999999e+01
#define ALWAN_DET_SRGB_OETF_ALPHA       +1.05499999999999994e+00
#define ALWAN_DET_SRGB_OETF_BETA        +5.49999999999999378e-02  /* alpha - 1 */
#define ALWAN_DET_SRGB_EOTF_DEGREE      12
#define ALWAN_DET_SRGB_EOTF_BREAK       +4.04499359999999988e-02  /* linear*break_x */
#define ALWAN_DET_SRGB_EOTF_POLY_LO     +9.04738729857819252e-02

/* OETF non-linear: pow(x, 0.416667) -- coefficients in u in [-1, 1]. */
static const alwan_f64 alwan_det_srgb_oetf_lo_coeffs_f64[13] = {
    +2.20527949175683663e-01,  /* c0 */
    +8.10391311634654499e-02,  /* c1 */
    -2.07802735218341125e-02,  /* c2 */
    +1.02279461196055389e-02,  /* c3 */
    -6.70856384082179139e-03,  /* c4 */
    -4.59683587653800414e-04,  /* c5 */
    +4.30539181354693164e-03,  /* c6 */
    +1.39812704234137039e-02,  /* c7 */
    -1.82824434318762247e-02,  /* c8 */
    -1.61757830951646323e-02,  /* c9 */
    +1.98377669972332228e-02,  /* c10 */
    +9.64522495415788376e-03,  /* c11 */
    -1.01486566231188072e-02,  /* c12 */
};

static const alwan_f32 alwan_det_srgb_oetf_lo_coeffs_f32[13] = {
    +2.20527949175683663e-01f,  /* c0 */
    +8.10391311634654499e-02f,  /* c1 */
    -2.07802735218341125e-02f,  /* c2 */
    +1.02279461196055389e-02f,  /* c3 */
    -6.70856384082179139e-03f,  /* c4 */
    -4.59683587653800414e-04f,  /* c5 */
    +4.30539181354693164e-03f,  /* c6 */
    +1.39812704234137039e-02f,  /* c7 */
    -1.82824434318762247e-02f,  /* c8 */
    -1.61757830951646323e-02f,  /* c9 */
    +1.98377669972332228e-02f,  /* c10 */
    +9.64522495415788376e-03f,  /* c11 */
    -1.01486566231188072e-02f,  /* c12 */
};

static const alwan_f64 alwan_det_srgb_oetf_hi_coeffs_f64[15] = {
    +7.64540870571882802e-01,  /* c0 */
    +2.88264945459668631e-01,  /* c1 */
    -7.62815312593596861e-02,  /* c2 */
    +3.46338030716015141e-02,  /* c3 */
    -1.66403922703599744e-02,  /* c4 */
    +3.12120174241526140e-02,  /* c5 */
    -4.43501396768077871e-02,  /* c6 */
    -7.00929282487610106e-02,  /* c7 */
    +1.20513946281691306e-01,  /* c8 */
    +1.69013178473658959e-01,  /* c9 */
    -2.36008334611156667e-01,  /* c10 */
    -1.69679838884518708e-01,  /* c11 */
    +2.14746046751567721e-01,  /* c12 */
    +7.31104505534837801e-02,  /* c13 */
    -8.29946057367243750e-02,  /* c14 */
};

static const alwan_f32 alwan_det_srgb_oetf_hi_coeffs_f32[15] = {
    +7.64540870571882802e-01f,  /* c0 */
    +2.88264945459668631e-01f,  /* c1 */
    -7.62815312593596861e-02f,  /* c2 */
    +3.46338030716015141e-02f,  /* c3 */
    -1.66403922703599744e-02f,  /* c4 */
    +3.12120174241526140e-02f,  /* c5 */
    -4.43501396768077871e-02f,  /* c6 */
    -7.00929282487610106e-02f,  /* c7 */
    +1.20513946281691306e-01f,  /* c8 */
    +1.69013178473658959e-01f,  /* c9 */
    -2.36008334611156667e-01f,  /* c10 */
    -1.69679838884518708e-01f,  /* c11 */
    +2.14746046751567721e-01f,  /* c12 */
    +7.31104505534837801e-02f,  /* c13 */
    -8.29946057367243750e-02f,  /* c14 */
};

/* EOTF non-linear: pow(z, 2.4); u in [-1, 1]. */
static const alwan_f64 alwan_det_srgb_eotf_coeffs_f64[13] = {
    +2.33240864171007339e-01,  /* c0 */
    +4.66891255625145052e-01,  /* c1 */
    +2.72592608034838868e-01,  /* c2 */
    +3.03165282839551048e-02,  /* c3 */
    -3.79587576910970365e-03,  /* c4 */
    +9.97258182171722584e-04,  /* c5 */
    -3.46918899520051044e-04,  /* c6 */
    +2.06216147730976139e-04,  /* c7 */
    -1.26414730219062801e-04,  /* c8 */
    -3.48424519162059978e-05,  /* c9 */
    +4.48587055367016899e-05,  /* c10 */
    +5.81507202876894552e-05,  /* c11 */
    -4.37068839539687224e-05,  /* c12 */
};

static const alwan_f32 alwan_det_srgb_eotf_coeffs_f32[13] = {
    +2.33240864171007339e-01f,  /* c0 */
    +4.66891255625145052e-01f,  /* c1 */
    +2.72592608034838868e-01f,  /* c2 */
    +3.03165282839551048e-02f,  /* c3 */
    -3.79587576910970365e-03f,  /* c4 */
    +9.97258182171722584e-04f,  /* c5 */
    -3.46918899520051044e-04f,  /* c6 */
    +2.06216147730976139e-04f,  /* c7 */
    -1.26414730219062801e-04f,  /* c8 */
    -3.48424519162059978e-05f,  /* c9 */
    +4.48587055367016899e-05f,  /* c10 */
    +5.81507202876894552e-05f,  /* c11 */
    -4.37068839539687224e-05f,  /* c12 */
};

#endif /* ALWAN_DET_SRGB_COEFFS_H */
