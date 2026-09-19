/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Machado 2009 as a continuous model, derived rather than looked up.
 *
 * alwan already simulates colour vision deficiency by interpolating between
 * the eleven matrices Machado, Oliveira and Fernandes published. That is what
 * the paper tabulates and it is right for those eleven severities ON THE
 * DISPLAY THEY USED, a 1997 CRT. It cannot answer for a different display, and
 * a modern panel's primaries are not that CRT's. This derives the matrix
 * instead, so any shift and any display can be asked for.
 *
 * The model, from the paper and its published errata:
 *
 *   1. Take the Stockman and Sharpe 2 degree cone fundamentals at 1 nm.
 *   2. Shift them. L and M blend into each other by alpha(x) = (20 - x) / 20,
 *      weighted by the areas under the two curves, which is what keeps the
 *      blend energy-preserving:
 *
 *          L' = a(dL) L + 0.96 (areaL / areaM) (1 - a(dL)) M
 *          M' = a(dM) M + (1 / 0.96) (areaM / areaL) (1 - a(dM)) L
 *          S' = S(lambda - dS)
 *
 *      The 0.96 is the paper's, not a fudge; the errata page is what carries
 *      these forms rather than the printed article.
 *   3. Project normal and shifted onto the paper's opponent axes, white-black,
 *      yellow-blue and red-green, with a fixed 3x3.
 *   4. Integrate each opponent response against each display primary's
 *      spectrum, normalise each row of the resulting 3x3 so it sums to one,
 *      and the answer is inv(normal) * shifted.
 *
 * WHY THE PRIMARIES ARE EMBEDDED AT 1 nm. The published spectra are at 5 nm,
 * and colour-science resamples them with SPRAGUE interpolation. That is not a
 * detail: resampling linearly instead moves the resulting matrix by up to
 * 1.8e-02, a thousand times the agreement otherwise available. Reimplementing
 * Sprague here to produce a constant would be the wrong shape of work, so
 * gendata calls the reference and alwan embeds the 1 nm result.
 *
 * A caller supplying measured primaries of their own gets alwan's own
 * resampling, which is not guaranteed to agree with another library's to the
 * last digit. That is said in the header rather than hidden.
 *
 * Reference: Machado, Oliveira and Fernandes (2009), IEEE TVCG 15(6), with the
 * corrected equations from the authors' errata, and colour-science's
 * colour.matrix_anomalous_trichromacy_Machado2009, which suite 82 pins against.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>
#include <math.h>

enum {
    ALWAN_MACHADO_WMIN = 390,
    ALWAN_MACHADO_WMAX = 830,
    ALWAN_MACHADO_N = ALWAN_MACHADO_WMAX - ALWAN_MACHADO_WMIN + 1
};

/* The display primaries the model integrates against, on the model's own 1 nm
 * grid. Interleaved R, G, B per wavelength. */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const MACHADO_PRIMARIES_CRT[ALWAN_MACHADO_N * 3] = {
#include "../data/vision/display_primaries_crt_brainard1997_1nm.csv"
};
static alwan_f64 const MACHADO_PRIMARIES_APPLE[ALWAN_MACHADO_N * 3] = {
#include "../data/vision/display_primaries_apple_studio_1nm.csv"
};
ALWAN_DIAG_POP

/* LMS to the paper's opponent axes: white-black, yellow-blue, red-green. */
static double const MACHADO_LMS_TO_WSYBRG[9] = {
    0.600,  0.400,  0.000,
    0.240,  0.105, -0.700,
    1.200, -1.600,  0.400
};

/* Trapezoid over a 1 nm grid, which is what the reference integrates with. */
static double alwan_machado_trapz(double const *v, size_t n) {
    double sum = 0.0;
    size_t i;
    if (n < 2) return 0.0;
    for (i = 1; i + 1 < n; i++) sum += v[i];
    return sum + 0.5 * (v[0] + v[n - 1]);
}

/* The cone fundamentals on the model's grid. Recovered from the integration
 * weights, which are the only public way to reach them: with no illuminant,
 * the trapezoid rule and no normalisation, weight[c][i] is exactly
 * dlambda * t_i * response, and dlambda is 1 here with t a half at the ends. */
static alwan_status alwan_machado_cone_fundamentals(double *lms, alwan_ctx *ctx) {
    double *w = (double *)ALWAN_ALLOC(alwan_safe_array_size(ALWAN_MACHADO_N * 3,
                                                            sizeof(double)), sizeof(double));
    alwan_status st;
    size_t i, c;
    if (!w) return ALWAN_E_NOMEM;
    st = alwan_spectral_weights_observer_f64(w, ALWAN_MACHADO_N,
                                             (alwan_f64)ALWAN_MACHADO_WMIN,
                                             (alwan_f64)ALWAN_MACHADO_WMAX, NULL,
                                             ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG,
                                             ALWAN_INTEGRATE_TRAPEZOID, 0, ctx);
    if (st != ALWAN_OK) { ALWAN_FREE(w); return st; }
    for (c = 0; c < 3; c++) {
        for (i = 0; i < ALWAN_MACHADO_N; i++) {
            double const t = (i == 0 || i == ALWAN_MACHADO_N - 1) ? 0.5 : 1.0;
            lms[i * 3 + c] = w[c * ALWAN_MACHADO_N + i] / t;
        }
    }
    ALWAN_FREE(w);
    return ALWAN_OK;
}

/* S shifted in wavelength, linear between samples and zero off the ends, which
 * is what the reference's constant-zero extrapolation gives. */
static double alwan_machado_shifted_s(double const *lms, double at) {
    double const x = at - (double)ALWAN_MACHADO_WMIN;
    double f;
    long i;
    if (!(x >= 0.0) || x > (double)(ALWAN_MACHADO_N - 1)) return 0.0;
    i = (long)x;
    if (i >= ALWAN_MACHADO_N - 1) return lms[(size_t)(ALWAN_MACHADO_N - 1) * 3 + 2];
    f = x - (double)i;
    return lms[(size_t)i * 3 + 2] * (1.0 - f) + lms[(size_t)(i + 1) * 3 + 2] * f;
}

/* The 3x3 the paper builds: each opponent response integrated against each
 * primary, then every row scaled so it sums to one. */
static void alwan_machado_opponent_matrix(double *out, double const *lms,
                                          double const *primaries) {
    double ws[ALWAN_MACHADO_N], yb[ALWAN_MACHADO_N], rg[ALWAN_MACHADO_N];
    double prod[ALWAN_MACHADO_N];
    double const *axis[3];
    size_t i, r, c;

    for (i = 0; i < ALWAN_MACHADO_N; i++) {
        double const L = lms[i * 3 + 0], M = lms[i * 3 + 1], S = lms[i * 3 + 2];
        ws[i] = MACHADO_LMS_TO_WSYBRG[0] * L + MACHADO_LMS_TO_WSYBRG[1] * M
              + MACHADO_LMS_TO_WSYBRG[2] * S;
        yb[i] = MACHADO_LMS_TO_WSYBRG[3] * L + MACHADO_LMS_TO_WSYBRG[4] * M
              + MACHADO_LMS_TO_WSYBRG[5] * S;
        rg[i] = MACHADO_LMS_TO_WSYBRG[6] * L + MACHADO_LMS_TO_WSYBRG[7] * M
              + MACHADO_LMS_TO_WSYBRG[8] * S;
    }
    axis[0] = ws; axis[1] = yb; axis[2] = rg;

    for (r = 0; r < 3; r++) {
        double sum = 0.0;
        for (c = 0; c < 3; c++) {
            for (i = 0; i < ALWAN_MACHADO_N; i++) prod[i] = primaries[i * 3 + c] * axis[r][i];
            out[r * 3 + c] = alwan_machado_trapz(prod, ALWAN_MACHADO_N);
            sum += out[r * 3 + c];
        }
        if (sum != 0.0) {
            for (c = 0; c < 3; c++) out[r * 3 + c] /= sum;
        }
    }
}

static alwan_status alwan_machado_derive(double *out, double dl, double dm, double ds,
                                         double const *primaries, alwan_ctx *ctx) {
    double *lms = NULL, *shifted = NULL;
    double normal[9], anomalous[9];
    alwan_mat3x3_f64 mn, inv, ma, product;
    alwan_status st;
    double area_l = 0.0, area_m = 0.0, al, am;
    double col[ALWAN_MACHADO_N];
    size_t i;

    lms = (double *)ALWAN_ALLOC(alwan_safe_array_size(ALWAN_MACHADO_N * 3, sizeof(double)),
                                sizeof(double));
    shifted = (double *)ALWAN_ALLOC(alwan_safe_array_size(ALWAN_MACHADO_N * 3, sizeof(double)),
                                    sizeof(double));
    if (!lms || !shifted) { ALWAN_FREE(lms); ALWAN_FREE(shifted); return ALWAN_E_NOMEM; }

    st = alwan_machado_cone_fundamentals(lms, ctx);
    if (st != ALWAN_OK) { ALWAN_FREE(lms); ALWAN_FREE(shifted); return st; }

    for (i = 0; i < ALWAN_MACHADO_N; i++) col[i] = lms[i * 3 + 0];
    area_l = alwan_machado_trapz(col, ALWAN_MACHADO_N);
    for (i = 0; i < ALWAN_MACHADO_N; i++) col[i] = lms[i * 3 + 1];
    area_m = alwan_machado_trapz(col, ALWAN_MACHADO_N);
    if (!(area_l > 0.0) || !(area_m > 0.0)) {
        ALWAN_FREE(lms); ALWAN_FREE(shifted);
        return ALWAN_E_DIVZERO;
    }

    al = (20.0 - dl) / 20.0;
    am = (20.0 - dm) / 20.0;
    for (i = 0; i < ALWAN_MACHADO_N; i++) {
        double const L = lms[i * 3 + 0], M = lms[i * 3 + 1];
        shifted[i * 3 + 0] = al * L + 0.96 * (area_l / area_m) * (1.0 - al) * M;
        shifted[i * 3 + 1] = am * M + (1.0 / 0.96) * (area_m / area_l) * (1.0 - am) * L;
        shifted[i * 3 + 2] = alwan_machado_shifted_s(
            lms, (double)ALWAN_MACHADO_WMIN + (double)i - ds);
    }

    alwan_machado_opponent_matrix(normal, lms, primaries);
    alwan_machado_opponent_matrix(anomalous, shifted, primaries);
    ALWAN_FREE(lms);
    ALWAN_FREE(shifted);

    memcpy(mn.m, normal, sizeof normal);
    memcpy(ma.m, anomalous, sizeof anomalous);
    st = alwan_mat3_inv_f64(&inv, &mn);
    if (st != ALWAN_OK) return st;
    alwan_mat3_mul_f64(&product, &inv, &ma);
    memcpy(out, product.m, sizeof product.m);
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- primaries */

static double const *alwan_machado_builtin(alwan_display_primaries which) {
    switch (which) {
        case ALWAN_DISPLAY_PRIMARIES_CRT_BRAINARD1997: return MACHADO_PRIMARIES_CRT;
        case ALWAN_DISPLAY_PRIMARIES_APPLE_STUDIO:     return MACHADO_PRIMARIES_APPLE;
        default: return NULL;
    }
}

alwan_status alwan_display_primaries_spd_f64(alwan_spd_f64 *r, alwan_spd_f64 *g,
                                             alwan_spd_f64 *b,
                                             alwan_display_primaries which, alwan_ctx *ctx) {
    double const *p = alwan_machado_builtin(which);
    alwan_spd_f64 *out[3];
    alwan_status st;
    size_t i, c;
    if (!r || !g || !b) return ALWAN_E_INVALID;
    if (!p) return ALWAN_E_RANGE;
    out[0] = r; out[1] = g; out[2] = b;
    for (c = 0; c < 3; c++) {
        st = alwan_spd_create_f64(out[c], (alwan_f64)ALWAN_MACHADO_WMIN,
                                  (alwan_f64)ALWAN_MACHADO_WMAX, ALWAN_MACHADO_N, ctx);
        if (st != ALWAN_OK) {
            while (c-- > 0) alwan_spd_destroy_f64(out[c], ctx);
            return st;
        }
        for (i = 0; i < ALWAN_MACHADO_N; i++) out[c]->values[i] = p[i * 3 + c];
    }
    return ALWAN_OK;
}

alwan_status alwan_display_primaries_spd_f32(alwan_spd_f32 *r, alwan_spd_f32 *g,
                                             alwan_spd_f32 *b,
                                             alwan_display_primaries which, alwan_ctx *ctx) {
    double const *p = alwan_machado_builtin(which);
    alwan_spd_f32 *out[3];
    alwan_status st;
    size_t i, c;
    if (!r || !g || !b) return ALWAN_E_INVALID;
    if (!p) return ALWAN_E_RANGE;
    out[0] = r; out[1] = g; out[2] = b;
    for (c = 0; c < 3; c++) {
        st = alwan_spd_create_f32(out[c], (alwan_f32)ALWAN_MACHADO_WMIN,
                                  (alwan_f32)ALWAN_MACHADO_WMAX, ALWAN_MACHADO_N, ctx);
        if (st != ALWAN_OK) {
            while (c-- > 0) alwan_spd_destroy_f32(out[c], ctx);
            return st;
        }
        for (i = 0; i < ALWAN_MACHADO_N; i++) out[c]->values[i] = (alwan_f32)p[i * 3 + c];
    }
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- entry */

/* A caller's own primaries, resampled onto the model's grid. Linear, and zero
 * off the ends, which is what the reference's constant-zero extrapolation
 * gives at the edges but not what its Sprague gives between samples: a
 * measured set will therefore not agree with another library to the last
 * digit. The built-in sets avoid this by being embedded at 1 nm already. */
static void alwan_machado_sample_spd(double *dst, double const *values, double wmin,
                                     double wmax, size_t count, size_t channel) {
    size_t i;
    double const step = (count > 1) ? (wmax - wmin) / (double)(count - 1) : 0.0;
    for (i = 0; i < ALWAN_MACHADO_N; i++) {
        double const nm = (double)ALWAN_MACHADO_WMIN + (double)i;
        double t, f;
        size_t j;
        if (count == 0 || step <= 0.0 || nm < wmin || nm > wmax) {
            dst[i * 3 + channel] = 0.0;
            continue;
        }
        t = (nm - wmin) / step;
        j = (size_t)t;
        if (j >= count - 1) { dst[i * 3 + channel] = values[count - 1]; continue; }
        f = t - (double)j;
        dst[i * 3 + channel] = values[j] * (1.0 - f) + values[j + 1] * f;
    }
}

alwan_status alwan_cvd_matrix_machado2009_shift_f64(alwan_mat3x3_f64 *out,
                                                    alwan_f64 shift_l, alwan_f64 shift_m,
                                                    alwan_f64 shift_s,
                                                    alwan_spd_f64 const *primary_r,
                                                    alwan_spd_f64 const *primary_g,
                                                    alwan_spd_f64 const *primary_b,
                                                    alwan_ctx *ctx) {
    double result[9];
    alwan_status st;
    if (!out) return ALWAN_E_INVALID;
    if (!primary_r && !primary_g && !primary_b) {
        st = alwan_machado_derive(result, (double)shift_l, (double)shift_m, (double)shift_s,
                                  MACHADO_PRIMARIES_CRT, ctx);
    } else {
        double *p;
        if (!primary_r || !primary_g || !primary_b) return ALWAN_E_INVALID;
        p = (double *)ALWAN_ALLOC(alwan_safe_array_size(ALWAN_MACHADO_N * 3, sizeof(double)),
                                  sizeof(double));
        if (!p) return ALWAN_E_NOMEM;
        alwan_machado_sample_spd(p, primary_r->values, primary_r->wavelength_min,
                                 primary_r->wavelength_max, primary_r->count, 0);
        alwan_machado_sample_spd(p, primary_g->values, primary_g->wavelength_min,
                                 primary_g->wavelength_max, primary_g->count, 1);
        alwan_machado_sample_spd(p, primary_b->values, primary_b->wavelength_min,
                                 primary_b->wavelength_max, primary_b->count, 2);
        st = alwan_machado_derive(result, (double)shift_l, (double)shift_m, (double)shift_s,
                                  p, ctx);
        ALWAN_FREE(p);
    }
    if (st == ALWAN_OK) memcpy(out->m, result, sizeof result);
    return st;
}

alwan_status alwan_cvd_matrix_machado2009_shift_f32(alwan_mat3x3_f32 *out,
                                                    alwan_f32 shift_l, alwan_f32 shift_m,
                                                    alwan_f32 shift_s,
                                                    alwan_spd_f32 const *primary_r,
                                                    alwan_spd_f32 const *primary_g,
                                                    alwan_spd_f32 const *primary_b,
                                                    alwan_ctx *ctx) {
    double result[9];
    alwan_status st;
    int i;
    if (!out) return ALWAN_E_INVALID;
    if (!primary_r && !primary_g && !primary_b) {
        st = alwan_machado_derive(result, (double)shift_l, (double)shift_m, (double)shift_s,
                                  MACHADO_PRIMARIES_CRT, ctx);
    } else {
        double *p;
        size_t k;
        if (!primary_r || !primary_g || !primary_b) return ALWAN_E_INVALID;
        p = (double *)ALWAN_ALLOC(alwan_safe_array_size(ALWAN_MACHADO_N * 3, sizeof(double)),
                                  sizeof(double));
        if (!p) return ALWAN_E_NOMEM;
        for (k = 0; k < ALWAN_MACHADO_N * 3; k++) p[k] = 0.0;
        {
            alwan_spd_f32 const *src[3];
            double *tmp = (double *)ALWAN_ALLOC(
                alwan_safe_array_size(primary_r->count > primary_g->count
                                      ? (primary_r->count > primary_b->count
                                         ? primary_r->count : primary_b->count)
                                      : (primary_g->count > primary_b->count
                                         ? primary_g->count : primary_b->count),
                                      sizeof(double)), sizeof(double));
            if (!tmp) { ALWAN_FREE(p); return ALWAN_E_NOMEM; }
            src[0] = primary_r; src[1] = primary_g; src[2] = primary_b;
            for (i = 0; i < 3; i++) {
                for (k = 0; k < src[i]->count; k++) tmp[k] = (double)src[i]->values[k];
                alwan_machado_sample_spd(p, tmp, (double)src[i]->wavelength_min,
                                         (double)src[i]->wavelength_max, src[i]->count,
                                         (size_t)i);
            }
            ALWAN_FREE(tmp);
        }
        st = alwan_machado_derive(result, (double)shift_l, (double)shift_m, (double)shift_s,
                                  p, ctx);
        ALWAN_FREE(p);
    }
    if (st == ALWAN_OK) {
        for (i = 0; i < 9; i++) out->m[i] = (alwan_f32)result[i];
    }
    return st;
}
