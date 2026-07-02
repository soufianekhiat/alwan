/*
 * Alwan - Pure C colour science library
 * GENERATED FILE - DO NOT EDIT BY HAND.
 * Source: alwan_dev/gendata/gen_tf_polynomials.py
 *
 * ITU-R BT.2020 / BT.709 OETF/EOTF polynomial coefficients.
 * Used when ALWAN_DETERMINISTIC=1 to avoid libm pow().
 *
 * Coefficients are stored in the NORMALIZED power basis: each piece's
 * domain [lo, hi] is mapped to u in [-1, 1] before Horner evaluation.
 * Indexed [c0, c1, ..., cN] for Horner:
 *     y = ((cN * u + c(N-1)) * u + c(N-2)) * u + ... + c0
 *
 * OETF (pow(x, 0.45)): piecewise -- split at 0.1.
 *   lo segment [0.018, 0.1]: degree 12, max abs 2.481e-04.
 *   hi segment [0.1, 1.0]:        degree 14, max abs 2.379e-06.
 * EOTF (pow(z, 2.22222) with z=(x+0.09899999999999998)/1.099):
 *   z domain [0.163785, 1.0], degree 12, max abs 5.497e-05.
 */

#ifndef ALWAN_DET_BT2020_COEFFS_H
#define ALWAN_DET_BT2020_COEFFS_H

#include "../alwan_types.h"

#define ALWAN_DET_BT2020_OETF_LO_DEGREE   12
#define ALWAN_DET_BT2020_OETF_HI_DEGREE   14
#define ALWAN_DET_BT2020_OETF_BREAK       +1.79999999999999986e-02
#define ALWAN_DET_BT2020_OETF_SPLIT       +1.00000000000000006e-01
#define ALWAN_DET_BT2020_OETF_LINEAR      +4.50000000000000000e+00
#define ALWAN_DET_BT2020_OETF_ALPHA       +1.09899999999999998e+00
#define ALWAN_DET_BT2020_OETF_BETA        +9.89999999999999769e-02  /* alpha - 1 */
#define ALWAN_DET_BT2020_EOTF_DEGREE      12
#define ALWAN_DET_BT2020_EOTF_BREAK       +8.09999999999999887e-02  /* linear*break_x */
#define ALWAN_DET_BT2020_EOTF_POLY_LO     +1.63785259326660576e-01

/* OETF non-linear: pow(x, 0.45) -- coefficients in u in [-1, 1]. */
#if ALWAN_BACKEND == ALWAN_BACKEND_C  /* f64 table: C-only (GPU is single precision) */
static const alwan_f64 alwan_det_bt2020_oetf_lo_coeffs_f64[13] = {
    +2.79822992577028029e-01,  /* c0 */
    +8.75037730329550206e-02,  /* c1 */
    -1.67216096474919335e-02,  /* c2 */
    +6.00961884186303370e-03,  /* c3 */
    -2.66868243173122056e-03,  /* c4 */
    +1.26655967271125042e-03,  /* c5 */
    -6.38572898107834207e-04,  /* c6 */
    +5.33274482685516479e-04,  /* c7 */
    -3.62187797779849432e-04,  /* c8 */
    -1.06343111904023016e-04,  /* c9 */
    +1.18033967552799693e-04,  /* c10 */
    +1.94287399824958371e-04,  /* c11 */
    -1.37811215601202228e-04,  /* c12 */
};
#endif

static const alwan_f32 alwan_det_bt2020_oetf_lo_coeffs_f32[13] = {
    +2.79822992577028029e-01f,  /* c0 */
    +8.75037730329550206e-02f,  /* c1 */
    -1.67216096474919335e-02f,  /* c2 */
    +6.00961884186303370e-03f,  /* c3 */
    -2.66868243173122056e-03f,  /* c4 */
    +1.26655967271125042e-03f,  /* c5 */
    -6.38572898107834207e-04f,  /* c6 */
    +5.33274482685516479e-04f,  /* c7 */
    -3.62187797779849432e-04f,  /* c8 */
    -1.06343111904023016e-04f,  /* c9 */
    +1.18033967552799693e-04f,  /* c10 */
    +1.94287399824958371e-04f,  /* c11 */
    -1.37811215601202228e-04f,  /* c12 */
};

#if ALWAN_BACKEND == ALWAN_BACKEND_C  /* f64 table: C-only (GPU is single precision) */
static const alwan_f64 alwan_det_bt2020_oetf_hi_coeffs_f64[15] = {
    +7.64122978325621927e-01,  /* c0 */
    +2.81338957667788048e-01,  /* c1 */
    -6.33119297185145563e-02,  /* c2 */
    +2.66538084260639629e-02,  /* c3 */
    -1.37211141148963420e-02,  /* c4 */
    +9.22309627690351234e-03,  /* c5 */
    -6.87041583173599989e-03,  /* c6 */
    -1.80427146019491134e-03,  /* c7 */
    +4.65194882078594045e-03,  /* c8 */
    +1.28057605223117696e-02,  /* c9 */
    -1.42372827476935099e-02,  /* c10 */
    -1.17005596691220539e-02,  /* c11 */
    +1.24152344421690191e-02,  /* c12 */
    +6.07505804161316554e-03,  /* c13 */
    -5.64201534409066244e-03,  /* c14 */
};
#endif

static const alwan_f32 alwan_det_bt2020_oetf_hi_coeffs_f32[15] = {
    +7.64122978325621927e-01f,  /* c0 */
    +2.81338957667788048e-01f,  /* c1 */
    -6.33119297185145563e-02f,  /* c2 */
    +2.66538084260639629e-02f,  /* c3 */
    -1.37211141148963420e-02f,  /* c4 */
    +9.22309627690351234e-03f,  /* c5 */
    -6.87041583173599989e-03f,  /* c6 */
    -1.80427146019491134e-03f,  /* c7 */
    +4.65194882078594045e-03f,  /* c8 */
    +1.28057605223117696e-02f,  /* c9 */
    -1.42372827476935099e-02f,  /* c10 */
    -1.17005596691220539e-02f,  /* c11 */
    +1.24152344421690191e-02f,  /* c12 */
    +6.07505804161316554e-03f,  /* c13 */
    -5.64201534409066244e-03f,  /* c14 */
};

/* EOTF non-linear: pow(z, 2.22222); u in [-1, 1]. */
#if ALWAN_BACKEND == ALWAN_BACKEND_C  /* f64 table: C-only (GPU is single precision) */
static const alwan_f64 alwan_det_bt2020_eotf_coeffs_f64[13] = {
    +3.00212360690548241e-01,  /* c0 */
    +4.79359145486302207e-01,  /* c1 */
    +2.10487446767833791e-01,  /* c2 */
    +1.12032504673651827e-02,  /* c3 */
    -1.56546896982081326e-03,  /* c4 */
    +3.98484612037992879e-04,  /* c5 */
    -1.31578198676441841e-04,  /* c6 */
    +5.62964826109193787e-05,  /* c7 */
    -2.61504490545093080e-05,  /* c8 */
    +2.83543487061643549e-06,  /* c9 */
    +4.78462155392178577e-07,  /* c10 */
    +7.47327694691492783e-06,  /* c11 */
    -4.57575971696491311e-06,  /* c12 */
};
#endif

static const alwan_f32 alwan_det_bt2020_eotf_coeffs_f32[13] = {
    +3.00212360690548241e-01f,  /* c0 */
    +4.79359145486302207e-01f,  /* c1 */
    +2.10487446767833791e-01f,  /* c2 */
    +1.12032504673651827e-02f,  /* c3 */
    -1.56546896982081326e-03f,  /* c4 */
    +3.98484612037992879e-04f,  /* c5 */
    -1.31578198676441841e-04f,  /* c6 */
    +5.62964826109193787e-05f,  /* c7 */
    -2.61504490545093080e-05f,  /* c8 */
    +2.83543487061643549e-06f,  /* c9 */
    +4.78462155392178577e-07f,  /* c10 */
    +7.47327694691492783e-06f,  /* c11 */
    -4.57575971696491311e-06f,  /* c12 */
};

#endif /* ALWAN_DET_BT2020_COEFFS_H */
