/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Skin reflectance from chromophores: a two-layer Kubelka-Munk model (after Doi and
 * Tominaga 2003, "Spectral estimation of human skin color using the Kubelka-Munk theory",
 * Proc. SPIE 5008), alwan's own code from the published coefficients:
 *
 *   epidermis  mu_a = Cm (bm mu_eu + (1 - bm) mu_pheo) + (1 - Cm) mu_base
 *   dermis     mu_a = Ch (g mu_oxy + (1 - g) mu_deoxy) + (1 - Ch) mu_base
 *   mu_eu   = 6.6e10 lambda^-3.33 mm^-1, mu_pheo = 2.9e14 lambda^-4.75 mm^-1
 *             (Donner and Jensen 2006, "A spectral BSSRDF for shading human skin")
 *   mu_base = 0.0244 + 8.53 exp(-(lambda - 154) / 66.2) mm^-1 (Jacques 2013, "Optical
 *             properties of biological tissues: a review", Phys. Med. Biol. 58)
 *   mu_blood = ln(10) eps(lambda) c, c = haemoglobin g/L / 64458 g/mol, eps from
 *             data/skin/haemoglobin.csv (Prahl's compilation, via the Virtual Tissue
 *             Simulator, MIT)
 *   mu_s'   = a (lambda / 500)^-b, a = 4.6 mm^-1, b = 1.421 (Jacques 2013, skin)
 *   K = 2 mu_a, S = 3/4 mu_s' - 1/4 mu_a (Star, Marijnissen and van Gemert 1988)
 *
 * The epidermis is a Kubelka-Munk layer of thickness d with a = (S + K) / S and
 * b = sqrt(a^2 - 1): R = sinh(bSd) / (a sinh(bSd) + b cosh(bSd)),
 * T = b / (a sinh(bSd) + b cosh(bSd)). The dermis is semi-infinite: R_inf = a - b. The two
 * combine as R = R1 + T1^2 R2 / (1 - R1 R2).
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
/* wavelength nm, HbO2, Hb: decadic molar extinction, cm^-1 / M */
static double const skin_hb[] = {
#include "../data/skin/haemoglobin.csv"
};
ALWAN_DIAG_POP

#define SKIN_HB_ROWS (sizeof(skin_hb) / sizeof(skin_hb[0]) / 3)
#define SKIN_LN10 2.302585092994045684
#define SKIN_HB_MOLAR_MASS 64458.0       /* g/mol, the value Prahl's conversion uses */
#define SKIN_WMIN 350.0
#define SKIN_WMAX 850.0

void alwan_skin_params_default(alwan_skin_params *params) {
    if (!params) return;
    memset(params, 0, sizeof *params);
    params->model = ALWAN_SKIN_KUBELKA_MUNK_2LAYER;
    params->melanin_fraction = 0.03;
    params->eumelanin_ratio = 0.7;
    params->blood_fraction = 0.02;
    params->oxygenation = 0.75;
}

typedef struct {
    double cm, bm, ch, g, d_mm, hb_molar, a, b;
    alwan_interp_method interp;
} skin_resolved;

static int skin_fraction_ok(double v) {
    return v >= 0.0 && v <= 1.0;
}

static int skin_nonneg_ok(double v) {
    return v >= 0.0 && v <= 1e300;
}

static alwan_status skin_resolve(skin_resolved *r, alwan_skin_params const *params) {
    alwan_skin_params def;
    if (!params) {
        alwan_skin_params_default(&def);
        params = &def;
    }
    if (params->model != ALWAN_SKIN_KUBELKA_MUNK_2LAYER) return ALWAN_E_INVALID;
    if (!skin_fraction_ok(params->melanin_fraction) || !skin_fraction_ok(params->eumelanin_ratio) ||
        !skin_fraction_ok(params->blood_fraction) || !skin_fraction_ok(params->oxygenation) ||
        !skin_nonneg_ok(params->epidermis_thickness_mm) || !skin_nonneg_ok(params->haemoglobin_g_per_l) ||
        !skin_nonneg_ok(params->scattering_per_mm) || !skin_nonneg_ok(params->scattering_power)) {
        return ALWAN_E_INVALID;
    }
    r->cm = params->melanin_fraction;
    r->bm = params->eumelanin_ratio;
    r->ch = params->blood_fraction;
    r->g = params->oxygenation;
    r->d_mm = params->epidermis_thickness_mm > 0.0 ? params->epidermis_thickness_mm : 0.1;
    r->hb_molar = (params->haemoglobin_g_per_l > 0.0 ? params->haemoglobin_g_per_l : 150.0) / SKIN_HB_MOLAR_MASS;
    r->a = params->scattering_per_mm > 0.0 ? params->scattering_per_mm : 4.6;
    r->b = params->scattering_power > 0.0 ? params->scattering_power : 1.421;
    r->interp = params->interpolation;
    return ALWAN_OK;
}

/* Haemoglobin's two extinction spectra at the wavelengths, read with the interpolation. */
static alwan_status skin_hb_at(double *oxy, double *deoxy, double const *w, size_t count, alwan_interp_method interp) {
    double x[SKIN_HB_ROWS], yo[SKIN_HB_ROWS], yd[SKIN_HB_ROWS];
    size_t i;
    alwan_status st;
    for (i = 0; i < SKIN_HB_ROWS; i++) {
        x[i] = skin_hb[3 * i];
        yo[i] = skin_hb[3 * i + 1];
        yd[i] = skin_hb[3 * i + 2];
    }
    st = alwan_interpolate_f64(x, yo, SKIN_HB_ROWS, w, oxy, count, interp);
    if (st == ALWAN_OK) st = alwan_interpolate_f64(x, yd, SKIN_HB_ROWS, w, deoxy, count, interp);
    if (st != ALWAN_OK) return ALWAN_E_INVALID;
    /* an overshooting interpolant must not make an absorber emit */
    for (i = 0; i < count; i++) {
        if (oxy[i] < 0.0) oxy[i] = 0.0;
        if (deoxy[i] < 0.0) deoxy[i] = 0.0;
    }
    return ALWAN_OK;
}

static double skin_baseline(double w) {
    return 0.0244 + 8.53 * ALWAN_EXP_F64(-(w - 154.0) / 66.2);
}

/* the layer coefficients at one wavelength, given haemoglobin's extinctions there */
static void skin_coefficients(double *mua_epi, double *mua_derm, double *musp, double w, double eps_oxy, double eps_deoxy,
                              skin_resolved const *r) {
    double const base = skin_baseline(w);
    double const eu = 6.6e10 * ALWAN_POW_F64(w, -3.33);
    double const pheo = 2.9e14 * ALWAN_POW_F64(w, -4.75);
    /* ln(10) eps [cm^-1/M] c [M] is per cm; a tenth of it per mm */
    double const blood = SKIN_LN10 * (r->g * eps_oxy + (1.0 - r->g) * eps_deoxy) * r->hb_molar * 0.1;
    *mua_epi = r->cm * (r->bm * eu + (1.0 - r->bm) * pheo) + (1.0 - r->cm) * base;
    *mua_derm = r->ch * blood + (1.0 - r->ch) * base;
    *musp = r->a * ALWAN_POW_F64(w / 500.0, -r->b);
}

static void skin_km(double *R, double *T, double K, double S, double d) {
    double a, b, x, t, denom;
    if (S <= 1e-300) {               /* a pure absorber: nothing scatters back */
        *R = 0.0;
        *T = (d > 0.0) ? ALWAN_EXP_F64(-K * d) : 1.0;
        return;
    }
    a = (S + K) / S;
    b = ALWAN_SQRT_F64(a * a - 1.0);
    if (b <= 0.0) {                  /* no absorption: R = Sd / (1 + Sd) */
        *R = S * d / (1.0 + S * d);
        *T = 1.0 / (1.0 + S * d);
        return;
    }
    x = b * S * d;
    t = ALWAN_TANH_F64(x);
    denom = a * t + b;
    *R = t / denom;
    /* T = b / (a sinh x + b cosh x) = b / (denom cosh x); cosh overflows to inf, T to 0 */
    *T = (x > 700.0) ? 0.0 : b / (denom * ALWAN_COSH_F64(x));
}

static double skin_r_inf(double K, double S) {
    double a;
    if (S <= 1e-300) return 0.0;
    a = (S + K) / S;
    return a - ALWAN_SQRT_F64(a * a - 1.0);
}

static double skin_reflectance_one(double mua_epi, double mua_derm, double musp, double d_mm) {
    double r1, t1, r2;
    double const s1 = 0.75 * musp - 0.25 * mua_epi, s2 = 0.75 * musp - 0.25 * mua_derm;
    skin_km(&r1, &t1, 2.0 * mua_epi, s1 > 0.0 ? s1 : 0.0, d_mm);
    r2 = skin_r_inf(2.0 * mua_derm, s2 > 0.0 ? s2 : 0.0);
    return r1 + t1 * t1 * r2 / (1.0 - r1 * r2);
}

static alwan_status skin_check_wavelengths(double const *w, size_t count) {
    size_t i;
    for (i = 0; i < count; i++) {
        if (!(w[i] >= SKIN_WMIN && w[i] <= SKIN_WMAX)) return ALWAN_E_RANGE;
    }
    return ALWAN_OK;
}

/* the work of both entry points, on a block small enough for the stack */
#define SKIN_BLOCK 256

static alwan_status skin_eval(double *R, double *mua_e, double *mua_d, double *musp, double const *w, size_t count,
                              skin_resolved const *r) {
    double oxy[SKIN_BLOCK], deoxy[SKIN_BLOCK];
    size_t start, i;
    for (start = 0; start < count; start += SKIN_BLOCK) {
        size_t const n = (count - start < SKIN_BLOCK) ? count - start : SKIN_BLOCK;
        alwan_status const st = skin_hb_at(oxy, deoxy, w + start, n, r->interp);
        if (st != ALWAN_OK) return st;
        for (i = 0; i < n; i++) {
            double e, dm, s;
            skin_coefficients(&e, &dm, &s, w[start + i], oxy[i], deoxy[i], r);
            if (mua_e) mua_e[start + i] = e;
            if (mua_d) mua_d[start + i] = dm;
            if (musp) musp[start + i] = s;
            if (R) R[start + i] = skin_reflectance_one(e, dm, s, r->d_mm);
        }
    }
    return ALWAN_OK;
}

alwan_status alwan_skin_absorption_f64(double *mua_epidermis, double *mua_dermis, double *musp, double const *wavelengths_nm,
                                       size_t count, alwan_skin_params const *params) {
    skin_resolved r;
    alwan_status st;
    if (!wavelengths_nm || count == 0) return ALWAN_E_INVALID;
    st = skin_resolve(&r, params);
    if (st == ALWAN_OK) st = skin_check_wavelengths(wavelengths_nm, count);
    if (st != ALWAN_OK) return st;
    return skin_eval(NULL, mua_epidermis, mua_dermis, musp, wavelengths_nm, count, &r);
}

alwan_status alwan_skin_reflectance_f64(double *reflectance_out, double const *wavelengths_nm, size_t count,
                                        alwan_skin_params const *params) {
    skin_resolved r;
    alwan_status st;
    if (!reflectance_out || !wavelengths_nm || count == 0) return ALWAN_E_INVALID;
    st = skin_resolve(&r, params);
    if (st == ALWAN_OK) st = skin_check_wavelengths(wavelengths_nm, count);
    if (st != ALWAN_OK) return st;
    return skin_eval(reflectance_out, NULL, NULL, NULL, wavelengths_nm, count, &r);
}

#define SKIN_COLOUR_MIN 360.0
#define SKIN_COLOUR_COUNT 471   /* 360-830 nm at 1 nm */

alwan_status alwan_skin_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_xyz_f64 *xyz_out, alwan_skin_params const *params,
                                alwan_rgb_space space, alwan_illuminant illuminant, alwan_observer_type observer, alwan_ctx *ctx) {
    double w[SKIN_COLOUR_COUNT], R[SKIN_COLOUR_COUNT];
    alwan_spd_f64 spd;
    alwan_status st;
    size_t i;
    if (!rgb_out) return ALWAN_E_INVALID;
    for (i = 0; i < SKIN_COLOUR_COUNT; i++) w[i] = SKIN_COLOUR_MIN + (double)i;
    st = alwan_skin_reflectance_f64(R, w, SKIN_COLOUR_COUNT, params);
    if (st != ALWAN_OK) return st;
    spd.values = R;
    spd.wavelength_min = SKIN_COLOUR_MIN;
    spd.wavelength_max = SKIN_COLOUR_MIN + (SKIN_COLOUR_COUNT - 1);
    spd.count = SKIN_COLOUR_COUNT;
    return alwan_reflectance_to_rgb_f64(rgb_out, xyz_out, &spd, ALWAN_INTERP_LINEAR, space, illuminant, observer, ctx);
}

/* ---- the fit ------------------------------------------------------------------------- */

#define SKIN_FIT_MAX 4

static double *skin_fit_slot(alwan_skin_params *p, unsigned bit) {
    switch (bit) {
    case ALWAN_SKIN_FIT_MELANIN: return &p->melanin_fraction;
    case ALWAN_SKIN_FIT_BLOOD: return &p->blood_fraction;
    case ALWAN_SKIN_FIT_OXYGENATION: return &p->oxygenation;
    case ALWAN_SKIN_FIT_EUMELANIN: return &p->eumelanin_ratio;
    default: return NULL;
    }
}

/* sum of squared residuals of the model at p against the target */
static alwan_status skin_ssr(double *ssr, double *resid, alwan_skin_params const *p, double const *target, double const *w,
                             size_t count) {
    skin_resolved r;
    size_t i;
    double s = 0.0;
    alwan_status st = skin_resolve(&r, p);
    if (st != ALWAN_OK) return st;
    st = skin_eval(resid, NULL, NULL, NULL, w, count, &r);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < count; i++) {
        resid[i] -= target[i];
        s += resid[i] * resid[i];
    }
    *ssr = s;
    return ALWAN_OK;
}

static double skin_clamp01(double v) {
    return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

/* Solve the small normal equations (J^T J + lambda diag) x = -J^T r by Gaussian
 * elimination with partial pivoting; n <= 4. */
static int skin_solve(double A[SKIN_FIT_MAX][SKIN_FIT_MAX], double *bvec, int n) {
    int i, j, k;
    for (i = 0; i < n; i++) {
        int piv = i;
        for (k = i + 1; k < n; k++) {
            if (ALWAN_ABS_F64(A[k][i]) > ALWAN_ABS_F64(A[piv][i])) piv = k;
        }
        if (ALWAN_ABS_F64(A[piv][i]) < 1e-300) return 0;
        if (piv != i) {
            double t;
            for (j = 0; j < n; j++) { t = A[i][j]; A[i][j] = A[piv][j]; A[piv][j] = t; }
            t = bvec[i]; bvec[i] = bvec[piv]; bvec[piv] = t;
        }
        for (k = i + 1; k < n; k++) {
            double const f = A[k][i] / A[i][i];
            for (j = i; j < n; j++) A[k][j] -= f * A[i][j];
            bvec[k] -= f * bvec[i];
        }
    }
    for (i = n - 1; i >= 0; i--) {
        double s = bvec[i];
        for (j = i + 1; j < n; j++) s -= A[i][j] * bvec[j];
        bvec[i] = s / A[i][i];
    }
    return 1;
}

alwan_status alwan_skin_fit_f64(alwan_skin_params *params, double *rms_out, double const *reflectance,
                                double const *wavelengths_nm, size_t count, unsigned fit) {
    unsigned bits[SKIN_FIT_MAX];
    int n = 0, it, k;
    double *resid, *trial, *jac;
    double ssr = 0.0, lambda = 1e-3;
    alwan_status st;
    size_t i;
    if (!params || !reflectance || !wavelengths_nm || count == 0) return ALWAN_E_INVALID;
    if (fit == 0) fit = ALWAN_SKIN_FIT_MELANIN | ALWAN_SKIN_FIT_BLOOD | ALWAN_SKIN_FIT_OXYGENATION;
    if (fit & ~(unsigned)(ALWAN_SKIN_FIT_MELANIN | ALWAN_SKIN_FIT_BLOOD | ALWAN_SKIN_FIT_OXYGENATION |
                          ALWAN_SKIN_FIT_EUMELANIN)) {
        return ALWAN_E_INVALID;
    }
    for (k = 0; k < SKIN_FIT_MAX; k++) {
        unsigned const bit = 1u << k;
        if (fit & bit) bits[n++] = bit;
    }
    st = skin_check_wavelengths(wavelengths_nm, count);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < count; i++) {
        if (!(reflectance[i] >= -1e300 && reflectance[i] <= 1e300)) return ALWAN_E_INVALID;
    }
    resid = (double *)ALWAN_ALLOC(sizeof(double) * count * (size_t)(2 + SKIN_FIT_MAX), sizeof(double));
    if (!resid) return ALWAN_E_NOMEM;
    trial = resid + count;
    jac = trial + count;
    st = skin_ssr(&ssr, resid, params, reflectance, wavelengths_nm, count);
    /* Levenberg-Marquardt with forward differences in each fraction; the step is clamped to
     * [0, 1] and a step that does not lower the residual raises lambda. A fixed iteration
     * count keeps the deterministic build deterministic. */
    for (it = 0; st == ALWAN_OK && it < 200; it++) {
        double JtJ[SKIN_FIT_MAX][SKIN_FIT_MAX], g[SKIN_FIT_MAX];
        alwan_skin_params step;
        double ssr_new = 0.0;
        int a, b, improved = 0;
        for (a = 0; a < n; a++) {
            alwan_skin_params q = *params;
            double *slot = skin_fit_slot(&q, bits[a]);
            double const v = *slot;
            double h = 1e-6 + 1e-6 * ALWAN_ABS_F64(v);
            double ssr_q;
            if (v + h > 1.0) h = -h;
            *slot = v + h;
            st = skin_ssr(&ssr_q, jac + (size_t)a * count, &q, reflectance, wavelengths_nm, count);
            if (st != ALWAN_OK) break;
            for (i = 0; i < count; i++) jac[(size_t)a * count + i] = (jac[(size_t)a * count + i] - resid[i]) / h;
        }
        if (st != ALWAN_OK) break;
        for (a = 0; a < n; a++) {
            double s = 0.0;
            for (i = 0; i < count; i++) s += jac[(size_t)a * count + i] * resid[i];
            g[a] = -s;
            for (b = 0; b < n; b++) {
                double t = 0.0;
                for (i = 0; i < count; i++) t += jac[(size_t)a * count + i] * jac[(size_t)b * count + i];
                JtJ[a][b] = t;
            }
        }
        while (lambda < 1e12) {
            double A[SKIN_FIT_MAX][SKIN_FIT_MAX], x[SKIN_FIT_MAX];
            for (a = 0; a < n; a++) {
                for (b = 0; b < n; b++) A[a][b] = JtJ[a][b] + (a == b ? lambda * (JtJ[a][a] + 1e-12) : 0.0);
                x[a] = g[a];
            }
            if (!skin_solve(A, x, n)) { lambda *= 10.0; continue; }
            step = *params;
            for (a = 0; a < n; a++) {
                double *slot = skin_fit_slot(&step, bits[a]);
                *slot = skin_clamp01(*slot + x[a]);
            }
            st = skin_ssr(&ssr_new, trial, &step, reflectance, wavelengths_nm, count);
            if (st != ALWAN_OK) break;
            if (ssr_new < ssr) {
                *params = step;
                memcpy(resid, trial, sizeof(double) * count);
                improved = (ssr - ssr_new) > 1e-15 * (ssr + 1e-300);
                ssr = ssr_new;
                lambda = lambda * 0.3 > 1e-12 ? lambda * 0.3 : 1e-12;
                break;
            }
            lambda *= 10.0;
        }
        if (st != ALWAN_OK || !improved) break;
    }
    ALWAN_FREE(resid);
    if (st == ALWAN_OK && rms_out) *rms_out = ALWAN_SQRT_F64(ssr / (double)count);
    return st;
}

/* ---- single precision: the f64 work at the edges --------------------------------------- */

#if ALWAN_WITH_F32
static alwan_status skin_widen(double **out, float const *in, size_t count) {
    size_t i;
    *out = (double *)ALWAN_ALLOC(sizeof(double) * count, sizeof(double));
    if (!*out) return ALWAN_E_NOMEM;
    for (i = 0; i < count; i++) (*out)[i] = (double)in[i];
    return ALWAN_OK;
}

alwan_status alwan_skin_absorption_f32(float *mua_epidermis, float *mua_dermis, float *musp, float const *wavelengths_nm,
                                       size_t count, alwan_skin_params const *params) {
    double *w = NULL, *buf;
    alwan_status st;
    size_t i;
    if (!wavelengths_nm || count == 0) return ALWAN_E_INVALID;
    st = skin_widen(&w, wavelengths_nm, count);
    if (st != ALWAN_OK) return st;
    buf = (double *)ALWAN_ALLOC(sizeof(double) * count * 3, sizeof(double));
    if (!buf) { ALWAN_FREE(w); return ALWAN_E_NOMEM; }
    st = alwan_skin_absorption_f64(buf, buf + count, buf + 2 * count, w, count, params);
    if (st == ALWAN_OK) {
        for (i = 0; i < count; i++) {
            if (mua_epidermis) mua_epidermis[i] = (float)buf[i];
            if (mua_dermis) mua_dermis[i] = (float)buf[count + i];
            if (musp) musp[i] = (float)buf[2 * count + i];
        }
    }
    ALWAN_FREE(buf);
    ALWAN_FREE(w);
    return st;
}

alwan_status alwan_skin_reflectance_f32(float *reflectance_out, float const *wavelengths_nm, size_t count,
                                        alwan_skin_params const *params) {
    double *w = NULL, *R;
    alwan_status st;
    size_t i;
    if (!reflectance_out || !wavelengths_nm || count == 0) return ALWAN_E_INVALID;
    st = skin_widen(&w, wavelengths_nm, count);
    if (st != ALWAN_OK) return st;
    R = (double *)ALWAN_ALLOC(sizeof(double) * count, sizeof(double));
    if (!R) { ALWAN_FREE(w); return ALWAN_E_NOMEM; }
    st = alwan_skin_reflectance_f64(R, w, count, params);
    if (st == ALWAN_OK) {
        for (i = 0; i < count; i++) reflectance_out[i] = (float)R[i];
    }
    ALWAN_FREE(R);
    ALWAN_FREE(w);
    return st;
}

alwan_status alwan_skin_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_xyz_f32 *xyz_out, alwan_skin_params const *params,
                                alwan_rgb_space space, alwan_illuminant illuminant, alwan_observer_type observer, alwan_ctx *ctx) {
    alwan_rgb_f64 rgb;
    alwan_xyz_f64 xyz;
    alwan_status st;
    if (!rgb_out) return ALWAN_E_INVALID;
    st = alwan_skin_rgb_f64(&rgb, &xyz, params, space, illuminant, observer, ctx);
    if (st == ALWAN_OK) {
        rgb_out->r = (float)rgb.r; rgb_out->g = (float)rgb.g; rgb_out->b = (float)rgb.b;
        if (xyz_out) { xyz_out->x = (float)xyz.x; xyz_out->y = (float)xyz.y; xyz_out->z = (float)xyz.z; }
    }
    return st;
}

alwan_status alwan_skin_fit_f32(alwan_skin_params *params, float *rms_out, float const *reflectance,
                                float const *wavelengths_nm, size_t count, unsigned fit) {
    double *w = NULL, *R = NULL, rms = 0.0;
    alwan_status st;
    if (!params || !reflectance || !wavelengths_nm || count == 0) return ALWAN_E_INVALID;
    st = skin_widen(&w, wavelengths_nm, count);
    if (st == ALWAN_OK) st = skin_widen(&R, reflectance, count);
    if (st == ALWAN_OK) st = alwan_skin_fit_f64(params, &rms, R, w, count, fit);
    if (st == ALWAN_OK && rms_out) *rms_out = (float)rms;
    ALWAN_FREE(R);
    ALWAN_FREE(w);
    return st;
}
#endif /* ALWAN_WITH_F32 */
