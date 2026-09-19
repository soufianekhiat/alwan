/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * GOG and GOGO: fitting a display's tone response from measured patches.
 *
 * A display put on a meter gives pairs of digital value and luminance, and the
 * question is what curve joins them. The two models that answer it are
 *
 *   GOG    L = (a d + b)^g                gain, offset, gamma
 *   GOGO   L = (a d + b)^g + c            and a flare term
 *
 * with d the normalised drive in [0, 1] and L the normalised luminance. The
 * flare c is what the room, the screen surface and the meter add to black; it
 * is why a measured display almost never reads zero at zero.
 *
 * NOT TESTABLE AGAINST A REFERENCE, and worth saying plainly: nothing in
 * colour-science fits either model, and searching it for gog, gain_offset and
 * the rest finds nothing. So the oracle here is the geometry rather than
 * another library. Suite 167 generates patches from a KNOWN model, fits them
 * back, and requires the parameters to return; that is a stronger check than
 * agreeing with a second implementation, because it would catch both being
 * wrong the same way. The published model is from Berns (1996), "Methods for
 * characterizing CRT displays", Displays 16(4), and the GOGO extension from
 * the same line of work.
 *
 * HOW THE FIT WORKS, because the obvious way does not.
 *
 * Throwing three or four parameters at a general optimiser from a cold start
 * is unreliable here: gamma and gain trade off against each other, so the
 * residual has a long curved valley and a simplex started anywhere wanders
 * along it. But the model linearises. For a FIXED gamma and flare,
 *
 *     (L - c)^(1/g) = a d + b
 *
 * is an ordinary straight line, and the best a and b follow in closed form.
 * So the search is only over gamma, or over gamma and c, with the other two
 * solved exactly inside it. That start is close enough that a short
 * fixed-iteration simplex on the real residual finishes the job.
 *
 * The two stages measure different things, which is the reason for having
 * both. The linearised stage minimises the residual of the straightened
 * curve, which is not the error a caller cares about; the refinement
 * minimises the luminance residual, which is. The value reported back is
 * always the second one.
 *
 * Deterministic by construction: a fixed initial simplex, a fixed iteration
 * count, no randomness and no convergence test that could stop at a different
 * place on a different machine.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>
#include <math.h>

enum {
    ALWAN_GOG_GAMMA_STEPS = 96,     /* the coarse sweep before the golden section */
    ALWAN_GOG_GOLDEN_ITER = 64,
    ALWAN_GOG_SIMPLEX_ITER = 400
};

#define ALWAN_GOG_GAMMA_MIN 0.4
#define ALWAN_GOG_GAMMA_MAX 6.0

typedef struct {
    double const *d;
    double const *l;
    size_t n;
} alwan_gog_data;

static int alwan_gog_finite(double v) {
    return (v == v) && (v > -1e308) && (v < 1e308);
}

static double alwan_gog_eval_raw(double a, double b, double g, double c, double d) {
    double const base = a * d + b;
    if (base <= 0.0) return c;                  /* the model floors at the flare */
    return pow(base, g) + c;
}

/* The luminance residual, which is the error a caller cares about. */
static double alwan_gog_rms(alwan_gog_data const *data, double a, double b, double g, double c) {
    double sum = 0.0;
    size_t i;
    for (i = 0; i < data->n; i++) {
        double const e = alwan_gog_eval_raw(a, b, g, c, data->d[i]) - data->l[i];
        if (!alwan_gog_finite(e)) return 1e308;
        sum += e * e;
    }
    return sqrt(sum / (double)data->n);
}

/* Best a and b for a fixed gamma and flare, in closed form: with the curve
 * straightened the problem is an ordinary line fit. */
static int alwan_gog_linear(alwan_gog_data const *data, double g, double c,
                            double *a_out, double *b_out) {
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0, den;
    size_t i, used = 0;
    for (i = 0; i < data->n; i++) {
        double const t = data->l[i] - c;
        double y;
        if (t < 0.0) continue;                  /* below the flare: no real root */
        y = pow(t, 1.0 / g);
        if (!alwan_gog_finite(y)) continue;
        sx += data->d[i];
        sy += y;
        sxx += data->d[i] * data->d[i];
        sxy += data->d[i] * y;
        used++;
    }
    if (used < 2) return 0;
    den = (double)used * sxx - sx * sx;
    if (den == 0.0) return 0;
    *a_out = ((double)used * sxy - sx * sy) / den;
    *b_out = (sy - *a_out * sx) / (double)used;
    return alwan_gog_finite(*a_out) && alwan_gog_finite(*b_out);
}

/* The best gamma for a fixed flare: a coarse sweep so a local minimum in the
 * valley cannot trap the search, then a golden section to sharpen it. */
static double alwan_gog_best_gamma(alwan_gog_data const *data, double c,
                                   double *a_out, double *b_out) {
    double best_g = 1.0, best_err = 1e308;
    double lo, hi, x1, x2, f1, f2;
    double const phi = 0.6180339887498949;
    int i;

    for (i = 0; i <= ALWAN_GOG_GAMMA_STEPS; i++) {
        double const g = ALWAN_GOG_GAMMA_MIN
            + (ALWAN_GOG_GAMMA_MAX - ALWAN_GOG_GAMMA_MIN) * (double)i
              / (double)ALWAN_GOG_GAMMA_STEPS;
        double a, b, err;
        if (!alwan_gog_linear(data, g, c, &a, &b)) continue;
        err = alwan_gog_rms(data, a, b, g, c);
        if (err < best_err) { best_err = err; best_g = g; }
    }

    {
        double const span = (ALWAN_GOG_GAMMA_MAX - ALWAN_GOG_GAMMA_MIN)
                          / (double)ALWAN_GOG_GAMMA_STEPS;
        lo = best_g - span;
        hi = best_g + span;
        if (lo < ALWAN_GOG_GAMMA_MIN) lo = ALWAN_GOG_GAMMA_MIN;
        if (hi > ALWAN_GOG_GAMMA_MAX) hi = ALWAN_GOG_GAMMA_MAX;
    }
    x1 = hi - phi * (hi - lo);
    x2 = lo + phi * (hi - lo);
    {
        double a, b;
        f1 = alwan_gog_linear(data, x1, c, &a, &b) ? alwan_gog_rms(data, a, b, x1, c) : 1e308;
        f2 = alwan_gog_linear(data, x2, c, &a, &b) ? alwan_gog_rms(data, a, b, x2, c) : 1e308;
    }
    for (i = 0; i < ALWAN_GOG_GOLDEN_ITER; i++) {
        double a, b;
        if (f1 < f2) {
            hi = x2; x2 = x1; f2 = f1;
            x1 = hi - phi * (hi - lo);
            f1 = alwan_gog_linear(data, x1, c, &a, &b) ? alwan_gog_rms(data, a, b, x1, c) : 1e308;
        } else {
            lo = x1; x1 = x2; f1 = f2;
            x2 = lo + phi * (hi - lo);
            f2 = alwan_gog_linear(data, x2, c, &a, &b) ? alwan_gog_rms(data, a, b, x2, c) : 1e308;
        }
    }
    best_g = 0.5 * (lo + hi);
    if (!alwan_gog_linear(data, best_g, c, a_out, b_out)) {
        *a_out = 1.0;
        *b_out = 0.0;
    }
    return best_g;
}

/* A fixed-iteration Nelder-Mead on the real luminance residual, started from
 * the linearised solution. No convergence test: a fixed count is what makes
 * the answer the same on every machine. */
static void alwan_gog_refine(alwan_gog_data const *data, double *p, int np) {
    double simplex[5][4];
    double err[5];
    int i, j, k, it;

    for (i = 0; i <= np; i++) {
        for (j = 0; j < np; j++) simplex[i][j] = p[j];
        if (i > 0) {
            double const step = (fabs(p[i - 1]) > 1e-6) ? 0.05 * fabs(p[i - 1]) : 0.01;
            simplex[i][i - 1] += step;
        }
        err[i] = alwan_gog_rms(data, simplex[i][0], simplex[i][1], simplex[i][2],
                               (np > 3) ? simplex[i][3] : 0.0);
    }

    for (it = 0; it < ALWAN_GOG_SIMPLEX_ITER; it++) {
        int best = 0, worst = 0, second = 0;
        double centroid[4], trial[4], f;
        for (i = 1; i <= np; i++) {
            if (err[i] < err[best]) best = i;
            if (err[i] > err[worst]) worst = i;
        }
        second = (worst == 0) ? 1 : 0;
        for (i = 0; i <= np; i++) {
            if (i != worst && err[i] > err[second]) second = i;
        }
        for (j = 0; j < np; j++) {
            centroid[j] = 0.0;
            for (i = 0; i <= np; i++) if (i != worst) centroid[j] += simplex[i][j];
            centroid[j] /= (double)np;
        }
        /* reflect */
        for (j = 0; j < np; j++) trial[j] = centroid[j] + (centroid[j] - simplex[worst][j]);
        f = alwan_gog_rms(data, trial[0], trial[1], trial[2], (np > 3) ? trial[3] : 0.0);
        if (f < err[best]) {
            double expanded[4], fe;
            for (j = 0; j < np; j++) {
                expanded[j] = centroid[j] + 2.0 * (centroid[j] - simplex[worst][j]);
            }
            fe = alwan_gog_rms(data, expanded[0], expanded[1], expanded[2],
                               (np > 3) ? expanded[3] : 0.0);
            if (fe < f) {
                for (j = 0; j < np; j++) trial[j] = expanded[j];
                f = fe;
            }
        } else if (f >= err[second]) {
            for (j = 0; j < np; j++) {
                trial[j] = centroid[j] + 0.5 * (simplex[worst][j] - centroid[j]);
            }
            f = alwan_gog_rms(data, trial[0], trial[1], trial[2], (np > 3) ? trial[3] : 0.0);
            if (f >= err[worst]) {
                for (i = 0; i <= np; i++) {
                    if (i == best) continue;
                    for (j = 0; j < np; j++) {
                        simplex[i][j] = simplex[best][j]
                                      + 0.5 * (simplex[i][j] - simplex[best][j]);
                    }
                    err[i] = alwan_gog_rms(data, simplex[i][0], simplex[i][1], simplex[i][2],
                                           (np > 3) ? simplex[i][3] : 0.0);
                }
                continue;
            }
        }
        for (j = 0; j < np; j++) simplex[worst][j] = trial[j];
        err[worst] = f;
    }

    {
        int best = 0;
        for (i = 1; i <= np; i++) if (err[i] < err[best]) best = i;
        for (k = 0; k < np; k++) p[k] = simplex[best][k];
    }
}

static alwan_status alwan_gog_fit_core(double *params, double *rms_out,
                                       double const *d, double const *l, size_t n,
                                       int with_flare) {
    alwan_gog_data data;
    double a = 1.0, b = 0.0, g;
    size_t i;
    double lo_l = 1e308;

    if (!params || !d || !l) return ALWAN_E_INVALID;
    /* Three parameters need three points, four need four, and a fit to
     * exactly as many points as it has parameters says nothing about the
     * display. Five is the smallest set worth calling a measurement. */
    if (n < (with_flare ? 5u : 4u)) return ALWAN_E_RANGE;
    for (i = 0; i < n; i++) {
        if (!alwan_gog_finite(d[i]) || !alwan_gog_finite(l[i])) return ALWAN_E_INVALID;
        if (l[i] < lo_l) lo_l = l[i];
    }

    data.d = d;
    data.l = l;
    data.n = n;

    if (!with_flare) {
        g = alwan_gog_best_gamma(&data, 0.0, &a, &b);
        params[0] = a; params[1] = b; params[2] = g; params[3] = 0.0;
        alwan_gog_refine(&data, params, 3);
        params[3] = 0.0;
    } else {
        /* The flare is what the display reads at black, so the search starts
         * there and works down: c cannot exceed the darkest measurement. */
        double best_err = 1e308, best[4];
        int step;
        best[0] = 1.0; best[1] = 0.0; best[2] = 2.2; best[3] = 0.0;
        for (step = 0; step <= 32; step++) {
            double const trial_c = (lo_l > 0.0) ? lo_l * (double)step / 32.0 : 0.0;
            double ta, tb, tg, err;
            tg = alwan_gog_best_gamma(&data, trial_c, &ta, &tb);
            err = alwan_gog_rms(&data, ta, tb, tg, trial_c);
            if (err < best_err) {
                best_err = err;
                best[0] = ta; best[1] = tb; best[2] = tg; best[3] = trial_c;
            }
            if (lo_l <= 0.0) break;
        }
        params[0] = best[0]; params[1] = best[1]; params[2] = best[2]; params[3] = best[3];
        alwan_gog_refine(&data, params, 4);
    }

    if (!alwan_gog_finite(params[0]) || !alwan_gog_finite(params[1])
        || !alwan_gog_finite(params[2]) || !alwan_gog_finite(params[3])) {
        return ALWAN_E_RANGE;
    }
    if (rms_out) {
        *rms_out = alwan_gog_rms(&data, params[0], params[1], params[2], params[3]);
    }
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- entry */

alwan_status alwan_display_gog_fit_f64(alwan_display_gog_f64 *out, alwan_f64 *rms_out,
                                       alwan_f64 const *digital, alwan_f64 const *luminance,
                                       size_t count, int with_flare) {
    double params[4], rms = 0.0;
    alwan_status st;
    if (!out) return ALWAN_E_INVALID;
    st = alwan_gog_fit_core(params, &rms, digital, luminance, count, with_flare);
    if (st != ALWAN_OK) return st;
    out->gain = (alwan_f64)params[0];
    out->offset = (alwan_f64)params[1];
    out->gamma = (alwan_f64)params[2];
    out->flare = (alwan_f64)params[3];
    if (rms_out) *rms_out = (alwan_f64)rms;
    return ALWAN_OK;
}

alwan_status alwan_display_gog_fit_f32(alwan_display_gog_f32 *out, alwan_f32 *rms_out,
                                       alwan_f32 const *digital, alwan_f32 const *luminance,
                                       size_t count, int with_flare) {
    double params[4], rms = 0.0;
    double *d = NULL, *l = NULL;
    alwan_status st;
    size_t i;
    if (!out) return ALWAN_E_INVALID;
    if (!digital || !luminance) return ALWAN_E_INVALID;
    if (count < (with_flare ? 5u : 4u)) return ALWAN_E_RANGE;
    d = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(double)), sizeof(double));
    l = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(double)), sizeof(double));
    if (!d || !l) { ALWAN_FREE(d); ALWAN_FREE(l); return ALWAN_E_NOMEM; }
    for (i = 0; i < count; i++) {
        d[i] = (double)digital[i];
        l[i] = (double)luminance[i];
    }
    st = alwan_gog_fit_core(params, &rms, d, l, count, with_flare);
    ALWAN_FREE(d);
    ALWAN_FREE(l);
    if (st != ALWAN_OK) return st;
    out->gain = (alwan_f32)params[0];
    out->offset = (alwan_f32)params[1];
    out->gamma = (alwan_f32)params[2];
    out->flare = (alwan_f32)params[3];
    if (rms_out) *rms_out = (alwan_f32)rms;
    return ALWAN_OK;
}

alwan_status alwan_display_gog_eval_f64(alwan_f64 *luminance_out,
                                        alwan_display_gog_f64 const *model, alwan_f64 digital) {
    if (!luminance_out || !model) return ALWAN_E_INVALID;
    *luminance_out = (alwan_f64)alwan_gog_eval_raw((double)model->gain, (double)model->offset,
                                                   (double)model->gamma, (double)model->flare,
                                                   (double)digital);
    return ALWAN_OK;
}

alwan_status alwan_display_gog_eval_f32(alwan_f32 *luminance_out,
                                        alwan_display_gog_f32 const *model, alwan_f32 digital) {
    if (!luminance_out || !model) return ALWAN_E_INVALID;
    *luminance_out = (alwan_f32)alwan_gog_eval_raw((double)model->gain, (double)model->offset,
                                                   (double)model->gamma, (double)model->flare,
                                                   (double)digital);
    return ALWAN_OK;
}

/* The drive that produces a luminance. Closed form, since the model inverts
 * exactly: nothing below the flare has one, and neither does a gain of zero. */
static alwan_status alwan_gog_invert(double *out, double a, double b, double g, double c,
                                     double l) {
    double base;
    if (!alwan_gog_finite(l)) return ALWAN_E_INVALID;
    if (l < c) return ALWAN_E_RANGE;            /* darker than the display's own black */
    if (a == 0.0) return ALWAN_E_DIVZERO;
    if (g == 0.0) return ALWAN_E_DIVZERO;
    base = pow(l - c, 1.0 / g);
    *out = (base - b) / a;
    return alwan_gog_finite(*out) ? ALWAN_OK : ALWAN_E_RANGE;
}

alwan_status alwan_display_gog_invert_f64(alwan_f64 *digital_out,
                                          alwan_display_gog_f64 const *model,
                                          alwan_f64 luminance) {
    double d;
    alwan_status st;
    if (!digital_out || !model) return ALWAN_E_INVALID;
    st = alwan_gog_invert(&d, (double)model->gain, (double)model->offset, (double)model->gamma,
                          (double)model->flare, (double)luminance);
    if (st == ALWAN_OK) *digital_out = (alwan_f64)d;
    return st;
}

alwan_status alwan_display_gog_invert_f32(alwan_f32 *digital_out,
                                          alwan_display_gog_f32 const *model,
                                          alwan_f32 luminance) {
    double d;
    alwan_status st;
    if (!digital_out || !model) return ALWAN_E_INVALID;
    st = alwan_gog_invert(&d, (double)model->gain, (double)model->offset, (double)model->gamma,
                          (double)model->flare, (double)luminance);
    if (st == ALWAN_OK) *digital_out = (alwan_f32)d;
    return st;
}
