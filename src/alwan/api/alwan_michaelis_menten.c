/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The Michaelis-Menten relation, and its inverse.
 *
 * It arrived in colour science through vision rather than through enzymes.
 * A photoreceptor's response saturates the same way a reaction rate does, so
 * the same two-parameter curve describes both, and it turns up across the
 * library already: ALWAN_LIGHTNESS_ABEBE2017_MICHAELIS_MENTEN is this curve,
 * and the JP2499 display transform's tonescale is a parameterisation of it.
 * Both reach it through their own code. This makes the relation itself
 * callable, so a caller fitting a receptor response or building a tonescale
 * does not have to write it again.
 *
 *   Michaelis 1913   v = V_max S / (K_m + S)         S = v K_m / (V_max - v)
 *   Abebe 2017       v = V_max S / (b_m S + K_m)     S = v K_m / (V_max - b_m v)
 *
 * Abebe's b_m scales the substrate term in the denominator, and setting it to
 * one gives Michaelis's form back exactly. K_m is the substrate concentration
 * at which the rate reaches half of V_max, which is what makes it the useful
 * parameter to fit: it is a position on the input axis rather than a shape.
 *
 * WHERE EACH ONE STOPS. The forward relation is undefined where its
 * denominator vanishes, and the inverse where V_max meets b_m v, which is the
 * rate the curve approaches but never reaches. Both return ALWAN_E_DIVZERO
 * there rather than an infinity: a saturating curve has no answer for "what
 * input gives more than the maximum", and saying so is more use than a value
 * that poisons whatever it is put into.
 *
 * Reference: Michaelis and Menten (1913); Abebe, Pouli, Larabi and Reinhard
 * (2017), "Perceptual Lightness Modeling for High-Dynamic-Range Imaging".
 * colour-science's colour.biochemistry.reaction_rate_MichaelisMenten and
 * substrate_concentration_MichaelisMenten are what suite 166 pins against.
 */

#include "../alwan.h"
#include "../alwan_internal.h"

static int alwan_mm_finite(double v) {
    return (v == v) && (v > -1e308) && (v < 1e308);
}

static alwan_status alwan_mm_rate(double *out, double s, double v_max, double k_m, double b_m) {
    double den;
    if (!out) return ALWAN_E_INVALID;
    if (!alwan_mm_finite(s) || !alwan_mm_finite(v_max) || !alwan_mm_finite(k_m)
        || !alwan_mm_finite(b_m)) {
        return ALWAN_E_INVALID;
    }
    den = b_m * s + k_m;
    if (den == 0.0) return ALWAN_E_DIVZERO;
    *out = (v_max * s) / den;
    return alwan_mm_finite(*out) ? ALWAN_OK : ALWAN_E_RANGE;
}

static alwan_status alwan_mm_substrate(double *out, double v, double v_max, double k_m,
                                       double b_m) {
    double den;
    if (!out) return ALWAN_E_INVALID;
    if (!alwan_mm_finite(v) || !alwan_mm_finite(v_max) || !alwan_mm_finite(k_m)
        || !alwan_mm_finite(b_m)) {
        return ALWAN_E_INVALID;
    }
    den = v_max - b_m * v;
    if (den == 0.0) return ALWAN_E_DIVZERO;
    *out = (v * k_m) / den;
    return alwan_mm_finite(*out) ? ALWAN_OK : ALWAN_E_RANGE;
}

alwan_status alwan_michaelis_menten_rate_f64(alwan_f64 *rate_out, alwan_f64 substrate,
                                             alwan_f64 v_max, alwan_f64 k_m) {
    double r;
    alwan_status st;
    if (!rate_out) return ALWAN_E_INVALID;
    st = alwan_mm_rate(&r, (double)substrate, (double)v_max, (double)k_m, 1.0);
    if (st == ALWAN_OK) *rate_out = (alwan_f64)r;
    return st;
}

alwan_status alwan_michaelis_menten_rate_f32(alwan_f32 *rate_out, alwan_f32 substrate,
                                             alwan_f32 v_max, alwan_f32 k_m) {
    double r;
    alwan_status st;
    if (!rate_out) return ALWAN_E_INVALID;
    st = alwan_mm_rate(&r, (double)substrate, (double)v_max, (double)k_m, 1.0);
    if (st == ALWAN_OK) *rate_out = (alwan_f32)r;
    return st;
}

alwan_status alwan_michaelis_menten_substrate_f64(alwan_f64 *substrate_out, alwan_f64 rate,
                                                  alwan_f64 v_max, alwan_f64 k_m) {
    double s;
    alwan_status st;
    if (!substrate_out) return ALWAN_E_INVALID;
    st = alwan_mm_substrate(&s, (double)rate, (double)v_max, (double)k_m, 1.0);
    if (st == ALWAN_OK) *substrate_out = (alwan_f64)s;
    return st;
}

alwan_status alwan_michaelis_menten_substrate_f32(alwan_f32 *substrate_out, alwan_f32 rate,
                                                  alwan_f32 v_max, alwan_f32 k_m) {
    double s;
    alwan_status st;
    if (!substrate_out) return ALWAN_E_INVALID;
    st = alwan_mm_substrate(&s, (double)rate, (double)v_max, (double)k_m, 1.0);
    if (st == ALWAN_OK) *substrate_out = (alwan_f32)s;
    return st;
}

alwan_status alwan_michaelis_menten_rate_abebe2017_f64(alwan_f64 *rate_out, alwan_f64 substrate,
                                                       alwan_f64 v_max, alwan_f64 k_m,
                                                       alwan_f64 b_m) {
    double r;
    alwan_status st;
    if (!rate_out) return ALWAN_E_INVALID;
    st = alwan_mm_rate(&r, (double)substrate, (double)v_max, (double)k_m,
                                    (double)b_m);
    if (st == ALWAN_OK) *rate_out = (alwan_f64)r;
    return st;
}

alwan_status alwan_michaelis_menten_rate_abebe2017_f32(alwan_f32 *rate_out, alwan_f32 substrate,
                                                       alwan_f32 v_max, alwan_f32 k_m,
                                                       alwan_f32 b_m) {
    double r;
    alwan_status st;
    if (!rate_out) return ALWAN_E_INVALID;
    st = alwan_mm_rate(&r, (double)substrate, (double)v_max, (double)k_m,
                                    (double)b_m);
    if (st == ALWAN_OK) *rate_out = (alwan_f32)r;
    return st;
}

alwan_status alwan_michaelis_menten_substrate_abebe2017_f64(alwan_f64 *substrate_out,
                                                            alwan_f64 rate, alwan_f64 v_max,
                                                            alwan_f64 k_m, alwan_f64 b_m) {
    double s;
    alwan_status st;
    if (!substrate_out) return ALWAN_E_INVALID;
    st = alwan_mm_substrate(&s, (double)rate, (double)v_max, (double)k_m,
                                         (double)b_m);
    if (st == ALWAN_OK) *substrate_out = (alwan_f64)s;
    return st;
}

alwan_status alwan_michaelis_menten_substrate_abebe2017_f32(alwan_f32 *substrate_out,
                                                            alwan_f32 rate, alwan_f32 v_max,
                                                            alwan_f32 k_m, alwan_f32 b_m) {
    double s;
    alwan_status st;
    if (!substrate_out) return ALWAN_E_INVALID;
    st = alwan_mm_substrate(&s, (double)rate, (double)v_max, (double)k_m,
                                         (double)b_m);
    if (st == ALWAN_OK) *substrate_out = (alwan_f32)s;
    return st;
}
