/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Two things that are about colour differences without being one: the power function
 * Huang et al. 2015 fitted to each formula, and the STRESS index of Garcia et al. 2007,
 * which says how well a formula's numbers track what observers judged.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stddef.h>

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
/* a then b per formula, in the order alwan_huang2015_formula declares them. */
static double const k_huang2015[ALWAN_HUANG2015_COUNT * 2] = {
#include "../data/matrices/huang2015_coefficients.csv"
};
ALWAN_DIAG_POP

static alwan_status alwan__huang2015(double *out, double delta_e, alwan_huang2015_formula formula) {
    if (!out) return ALWAN_E_INVALID;
    if (formula < ALWAN_HUANG2015_CIE1976 || formula >= ALWAN_HUANG2015_COUNT) return ALWAN_E_INVALID;
    if (!(delta_e >= 0.0)) return ALWAN_E_INVALID;   /* a difference is never negative, and NaN is not one */
    *out = k_huang2015[2 * formula] * ALWAN_POW(delta_e, k_huang2015[2 * formula + 1]);
    return ALWAN_OK;
}

alwan_status alwan_power_function_huang2015_f64(alwan_f64 *out, alwan_f64 delta_e, alwan_huang2015_formula formula) {
    double v = 0.0;
    alwan_status st;
    if (!out) return ALWAN_E_INVALID;
    st = alwan__huang2015(&v, (double)delta_e, formula);
    if (st != ALWAN_OK) return st;
    *out = v;
    return ALWAN_OK;
}

alwan_status alwan_power_function_huang2015_f32(alwan_f32 *out, alwan_f32 delta_e, alwan_huang2015_formula formula) {
    double v = 0.0;
    alwan_status st;
    if (!out) return ALWAN_E_INVALID;
    st = alwan__huang2015(&v, (double)delta_e, formula);
    if (st != ALWAN_OK) return st;
    *out = (alwan_f32)v;
    return ALWAN_OK;
}

/* STRESS: the computed differences are first scaled by F1, the factor that best lines
 * them up with the visual ones, and what is left over is measured against the visual
 * differences themselves. Zero means the formula tracks the observers exactly. */
alwan_status alwan_index_stress_f64(alwan_f64 *stress_out, alwan_f64 const *delta_e, alwan_f64 const *delta_v,
                                    size_t count) {
    double sum_ee = 0.0, sum_ev = 0.0, sum_num = 0.0, sum_den = 0.0, f1;
    size_t i;
    if (!stress_out || !delta_e || !delta_v || count == 0) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        sum_ee += (double)delta_e[i] * (double)delta_e[i];
        sum_ev += (double)delta_e[i] * (double)delta_v[i];
    }
    if (sum_ev == 0.0) return ALWAN_E_DIVZERO;
    f1 = sum_ee / sum_ev;
    for (i = 0; i < count; i++) {
        double const r = (double)delta_e[i] - f1 * (double)delta_v[i];
        double const s = f1 * (double)delta_v[i];
        sum_num += r * r;
        sum_den += s * s;
    }
    if (sum_den == 0.0) return ALWAN_E_DIVZERO;
    *stress_out = ALWAN_SQRT(sum_num / sum_den);
    return ALWAN_OK;
}

alwan_status alwan_index_stress_f32(alwan_f32 *stress_out, alwan_f32 const *delta_e, alwan_f32 const *delta_v,
                                    size_t count) {
    double sum_ee = 0.0, sum_ev = 0.0, sum_num = 0.0, sum_den = 0.0, f1;
    size_t i;
    if (!stress_out || !delta_e || !delta_v || count == 0) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        sum_ee += (double)delta_e[i] * (double)delta_e[i];
        sum_ev += (double)delta_e[i] * (double)delta_v[i];
    }
    if (sum_ev == 0.0) return ALWAN_E_DIVZERO;
    f1 = sum_ee / sum_ev;
    for (i = 0; i < count; i++) {
        double const r = (double)delta_e[i] - f1 * (double)delta_v[i];
        double const s = f1 * (double)delta_v[i];
        sum_num += r * r;
        sum_den += s * s;
    }
    if (sum_den == 0.0) return ALWAN_E_DIVZERO;
    *stress_out = (alwan_f32)ALWAN_SQRT(sum_num / sum_den);
    return ALWAN_OK;
}
