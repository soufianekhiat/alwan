/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Fluorescence as bispectral reradiation: a Donaldson matrix on a wavelength grid, built
 * from a separable excitation x emission model or taken as given, its radiance and colour
 * under any illuminant, and the sampling a spectral path tracer needs. alwan's own code;
 * the model is set out in alwan.h above alwan_fluorescent_method.
 *
 *   D[o][i] = Q a(i) e(o) / sum_o' e(o') . lambda_i / lambda_o   (reradiation)
 *   D[i][i] += R_base(i) (1 - a(i))                                (reflection)
 *
 * with a and e Gaussians in wavenumber (nu = 1e7 / lambda, cm^-1) whose full width at
 * half maximum is given in nm at the peak, and the emission cut below the incident sample
 * unless ALWAN_FLUORESCENT_ANTI_STOKES. Summing e over the grid (rather than integrating
 * the continuous band) keeps the photon yield of each column exactly Q a(i).
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_fluorescence_core.h"
#include <string.h>

#define FLUO_DEFAULT_MIN 300.0
#define FLUO_DEFAULT_MAX 830.0
#define FLUO_NU 1e7                      /* nm to cm^-1 */
#define FLUO_FWHM_TO_SIGMA 0.42466090014400953   /* 1 / (2 sqrt(2 ln 2)) */

struct alwan_fluorescent_s {
    size_t n;
    double lmin, lmax, h;
    double *D;        /* n x n, row o = emitted, column i = incident */
    double *tab64;    /* 2 n (n + 3): OUTGOING rows, then INCOMING */
    float *tab32;
    double balance_max, fluo_max;
};

static int fluo_finite(double v) { return v == v && v < 1e300 && v > -1e300; }

alwan_status alwan_fluorescent_params_example(alwan_fluorescent_params *params, alwan_fluorescent_example example) {
    if (!params) return ALWAN_E_INVALID;
    memset(params, 0, sizeof *params);
    params->method = ALWAN_FLUORESCENT_PARAMETRIC;
    params->base_reflectance = 0.85;
    switch (example) {
    case ALWAN_FLUORESCENT_EXAMPLE_WHITENER:
        params->excitation_peak_nm = 350.0, params->excitation_fwhm_nm = 50.0, params->absorptance = 0.9;
        params->emission_peak_nm = 435.0, params->emission_fwhm_nm = 60.0, params->quantum_yield = 0.8;
        return ALWAN_OK;
    case ALWAN_FLUORESCENT_EXAMPLE_HIGHLIGHTER:
        params->excitation_peak_nm = 455.0, params->excitation_fwhm_nm = 70.0, params->absorptance = 0.95;
        params->emission_peak_nm = 515.0, params->emission_fwhm_nm = 40.0, params->quantum_yield = 0.9;
        return ALWAN_OK;
    default:
        return ALWAN_E_INVALID;
    }
}

/* A Gaussian band in wavenumber at each grid sample, 1 at the peak. */
static alwan_status fluo_band(double *out, double const *lam, size_t n, double peak, double fwhm) {
    double nu0, dnu, sigma;
    size_t k;
    if (!(fluo_finite(peak) && peak > 0.0 && fluo_finite(fwhm) && fwhm > 0.0 && fwhm < 2.0 * peak)) return ALWAN_E_INVALID;
    nu0 = FLUO_NU / peak;
    dnu = FLUO_NU / (peak - 0.5 * fwhm) - FLUO_NU / (peak + 0.5 * fwhm);
    sigma = dnu * FLUO_FWHM_TO_SIGMA;
    for (k = 0; k < n; k++) {
        double const z = (FLUO_NU / lam[k] - nu0) / sigma;
        out[k] = ALWAN_EXP_F64(-0.5 * z * z);
    }
    return ALWAN_OK;
}

/* The substrate's reflectance at the grid samples: the caller's table read with its
 * method (its ends held outside it), held to [0, 1]; or the constant. */
static alwan_status fluo_base(double *out, double const *lam, size_t n, alwan_fluorescent_params const *p) {
    size_t k;
    if (!p->reflectance) {
        if (!(fluo_finite(p->base_reflectance) && p->base_reflectance >= 0.0 && p->base_reflectance <= 1.0)) return ALWAN_E_INVALID;
        for (k = 0; k < n; k++) out[k] = p->base_reflectance;
        return ALWAN_OK;
    } else {
        size_t const m = p->reflectance_count;
        size_t const stride = p->reflectance_stride ? p->reflectance_stride : sizeof(double);
        double *xin, *yin, *xout;
        alwan_status st;
        if (m < 2 || !(fluo_finite(p->reflectance_min) && fluo_finite(p->reflectance_max) && p->reflectance_max > p->reflectance_min))
            return ALWAN_E_INVALID;
        xin = (double *)ALWAN_ALLOC(alwan_safe_array_size(2 * m + n, sizeof(double)), sizeof(double));
        if (!xin) return ALWAN_E_NOMEM;
        yin = xin + m, xout = yin + m;
        for (k = 0; k < m; k++) {
            double const v = *(double const *)((char const *)p->reflectance + k * stride);
            if (!fluo_finite(v) || v < 0.0 || v > 1.0) { ALWAN_FREE(xin); return ALWAN_E_INVALID; }
            xin[k] = p->reflectance_min + (p->reflectance_max - p->reflectance_min) * (double)k / (double)(m - 1);
            yin[k] = v;
        }
        for (k = 0; k < n; k++) {
            double x = lam[k];
            if (x < p->reflectance_min) x = p->reflectance_min;
            if (x > p->reflectance_max) x = p->reflectance_max;
            xout[k] = x;
        }
        st = alwan_interpolate_f64(xin, yin, m, xout, out, n, p->interpolation);
        ALWAN_FREE(xin);
        if (st != ALWAN_OK) return ALWAN_E_INVALID;
        for (k = 0; k < n; k++) out[k] = out[k] < 0.0 ? 0.0 : (out[k] > 1.0 ? 1.0 : out[k]);
        return ALWAN_OK;
    }
}

static void fluo_tables(alwan_fluorescent *m, double const *lam) {
    size_t const n = m->n, rs = n + 3;
    size_t o, i, k;
    double bal = 0.0, fmax = 0.0;
    for (i = 0; i < n; i++) {   /* photons per column */
        double b = 0.0, off = 0.0;
        for (o = 0; o < n; o++) {
            b += m->D[o * n + i] * lam[o] / lam[i];
            if (o != i) off += m->D[o * n + i];
        }
        if (b > bal) bal = b;
        if (off > fmax) fmax = off;
    }
    m->balance_max = bal, m->fluo_max = fmax;
    for (k = 0; k < 2 * n; k++) {
        int const incoming = k >= n;
        size_t const r = incoming ? k - n : k;
        double *row = m->tab64 + k * rs;
        double total = 0.0, off = 0.0, run = 0.0;
        for (i = 0; i < n; i++) {
            double const d = incoming ? m->D[i * n + r] : m->D[r * n + i];
            total += d;
            if (i != r) off += d;
        }
        row[0] = total;
        row[1] = total > 0.0 ? off / total : 0.0;
        row[2] = 0.0;
        for (i = 0; i < n; i++) {
            double const d = (i == r) ? 0.0 : (incoming ? m->D[i * n + r] : m->D[r * n + i]);
            run += off > 0.0 ? d : 1.0;
            row[3 + i] = run;
        }
        for (i = 0; i < n; i++) row[3 + i] /= run;
        row[2 + n] = 1.0;
    }
    for (k = 0; k < 2 * n * rs; k++) m->tab32[k] = (float)m->tab64[k];
}

alwan_status alwan_fluorescent_create(alwan_fluorescent **out, alwan_fluorescent_params const *params, alwan_ctx *ctx) {
    alwan_fluorescent *m;
    double lmin, lmax, *lam = NULL, *a = NULL, *e = NULL, *base = NULL;
    size_t n, k, o, i;
    alwan_status st = ALWAN_OK;
    ALWAN_UNUSED(ctx);
    if (!out || !params) return ALWAN_E_INVALID;
    *out = NULL;
    if (params->method != ALWAN_FLUORESCENT_PARAMETRIC && params->method != ALWAN_FLUORESCENT_MATRIX) return ALWAN_E_INVALID;
    if (params->flags & ~ALWAN_FLUORESCENT_ANTI_STOKES) return ALWAN_E_INVALID;
    lmin = params->lambda_min, lmax = params->lambda_max;
    if (lmin == 0.0 && lmax == 0.0) lmin = FLUO_DEFAULT_MIN, lmax = FLUO_DEFAULT_MAX;
    if (!(fluo_finite(lmin) && fluo_finite(lmax) && lmin > 0.0 && lmax > lmin)) return ALWAN_E_INVALID;
    n = params->count;
    if (n == 0) {
        double const span = lmax - lmin;
        if (span > 100000.0) return ALWAN_E_INVALID;
        n = (size_t)ALWAN_CEIL_F64(span - 1e-9) + 1;
    }
    if (n < 2 || n > 20000) return ALWAN_E_INVALID;
    if (params->method == ALWAN_FLUORESCENT_MATRIX && !params->matrix) return ALWAN_E_INVALID;

    m = (alwan_fluorescent *)ALWAN_ALLOC(sizeof *m, sizeof(double));   /* bytes, alignment */
    if (!m) return ALWAN_E_NOMEM;
    memset(m, 0, sizeof *m);
    m->n = n, m->lmin = lmin, m->lmax = lmax, m->h = (lmax - lmin) / (double)(n - 1);
    m->D = (double *)ALWAN_ALLOC(alwan_safe_array_size(n * n, sizeof(double)), sizeof(double));
    m->tab64 = (double *)ALWAN_ALLOC(alwan_safe_array_size(2 * n * (n + 3), sizeof(double)), sizeof(double));
    m->tab32 = (float *)ALWAN_ALLOC(alwan_safe_array_size(2 * n * (n + 3), sizeof(float)), sizeof(float));
    lam = (double *)ALWAN_ALLOC(alwan_safe_array_size(4 * n, sizeof(double)), sizeof(double));
    if (!m->D || !m->tab64 || !m->tab32 || !lam) { st = ALWAN_E_NOMEM; goto done; }
    a = lam + n, e = a + n, base = e + n;
    for (k = 0; k < n; k++) lam[k] = lmin + m->h * (double)k;
    memset(m->D, 0, n * n * sizeof(double));

    if (params->method == ALWAN_FLUORESCENT_MATRIX) {
        size_t const rs = params->matrix_row_stride ? params->matrix_row_stride : n * sizeof(double);
        for (o = 0; o < n; o++) {
            double const *row = (double const *)((char const *)params->matrix + o * rs);
            for (i = 0; i < n; i++) {
                if (!fluo_finite(row[i]) || row[i] < 0.0) { st = ALWAN_E_INVALID; goto done; }
                m->D[o * n + i] = row[i];
            }
        }
    } else {
        double const A = params->absorptance, Q = params->quantum_yield;
        int const stokes = !(params->flags & ALWAN_FLUORESCENT_ANTI_STOKES);
        if (!(fluo_finite(A) && A >= 0.0 && A <= 1.0 && fluo_finite(Q) && Q >= 0.0 && Q <= 1.0)) { st = ALWAN_E_INVALID; goto done; }
        st = fluo_base(base, lam, n, params);
        if (st != ALWAN_OK) goto done;
        if (A > 0.0 && Q > 0.0) {
            st = fluo_band(a, lam, n, params->excitation_peak_nm, params->excitation_fwhm_nm);
            if (st == ALWAN_OK) st = fluo_band(e, lam, n, params->emission_peak_nm, params->emission_fwhm_nm);
            if (st != ALWAN_OK) goto done;
            for (k = 0; k < n; k++) a[k] *= A;
        } else {
            for (k = 0; k < n; k++) a[k] = 0.0, e[k] = 0.0;
        }
        for (i = 0; i < n; i++) {
            double const qa = Q * a[i];
            m->D[i * n + i] += base[i] * (1.0 - a[i]);
            if (qa > 0.0) {
                double sum = 0.0;
                for (o = stokes ? i : 0; o < n; o++) sum += e[o];
                if (sum > 0.0)
                    for (o = stokes ? i : 0; o < n; o++) m->D[o * n + i] += qa * (e[o] / sum) * lam[i] / lam[o];
            }
        }
    }
    fluo_tables(m, lam);
done:
    ALWAN_FREE(lam);
    if (st != ALWAN_OK) { alwan_fluorescent_destroy(m, ctx); return st; }
    *out = m;
    return ALWAN_OK;
}

void alwan_fluorescent_destroy(alwan_fluorescent *material, alwan_ctx *ctx) {
    ALWAN_UNUSED(ctx);
    if (!material) return;
    ALWAN_FREE(material->D);
    ALWAN_FREE(material->tab64);
    ALWAN_FREE(material->tab32);
    ALWAN_FREE(material);
}

alwan_status alwan_fluorescent_get_info(alwan_fluorescent_info *info, alwan_fluorescent const *material) {
    if (!info || !material) return ALWAN_E_INVALID;
    info->lambda_min = material->lmin, info->lambda_max = material->lmax, info->step = material->h;
    info->count = material->n;
    info->photon_balance_max = material->balance_max;
    info->fluorescent_max = material->fluo_max;
    return ALWAN_OK;
}

alwan_status alwan_fluorescent_matrix_f64(double *out, size_t row_stride, alwan_fluorescent const *material) {
    size_t o, n;
    if (!out || !material) return ALWAN_E_INVALID;
    n = material->n;
    if (!row_stride) row_stride = n * sizeof(double);
    for (o = 0; o < n; o++) memcpy((char *)out + o * row_stride, material->D + o * n, n * sizeof(double));
    return ALWAN_OK;
}

alwan_status alwan_fluorescent_matrix_f32(float *out, size_t row_stride, alwan_fluorescent const *material) {
    size_t o, i, n;
    if (!out || !material) return ALWAN_E_INVALID;
    n = material->n;
    if (!row_stride) row_stride = n * sizeof(float);
    for (o = 0; o < n; o++) {
        float *row = (float *)((char *)out + o * row_stride);
        for (i = 0; i < n; i++) row[i] = (float)material->D[o * n + i];
    }
    return ALWAN_OK;
}

/* The illuminant at the grid samples: linear between its samples, 0 outside its table. */
static alwan_status fluo_illuminant(double *E, alwan_fluorescent const *m, alwan_spd_f64 const *spd, alwan_illuminant ill, alwan_ctx *ctx) {
    alwan_spd_f64 own;
    alwan_spd_f64 const *use = spd;
    alwan_status st = ALWAN_OK;
    size_t k;
    memset(&own, 0, sizeof own);
    if (!use) {
        st = alwan_spd_illuminant_f64(&own, ill, ctx);
        if (st != ALWAN_OK) return st;
        use = &own;
    }
    if (!use->values || use->count < 2 || !(use->wavelength_max > use->wavelength_min)) {
        st = ALWAN_E_INVALID;
    } else {
        double const step = (use->wavelength_max - use->wavelength_min) / (double)(use->count - 1);
        for (k = 0; k < m->n; k++) {
            double const x = m->lmin + m->h * (double)k;
            double s, f, v;
            size_t j;
            if (x < use->wavelength_min || x > use->wavelength_max) { E[k] = 0.0; continue; }
            s = (x - use->wavelength_min) / step;
            j = (size_t)s;
            if (j >= use->count - 1) j = use->count - 2;
            f = s - (double)j;
            v = use->values[j] + f * (use->values[j + 1] - use->values[j]);
            if (!fluo_finite(v)) { st = ALWAN_E_INVALID; break; }
            E[k] = v;
        }
    }
    if (use == &own) alwan_spd_destroy_f64(&own, ctx);
    return st;
}

alwan_status alwan_fluorescent_radiance_f64(double *radiance_out, double *total_out, double *fluorescent_out,
                                            alwan_fluorescent const *material, alwan_spd_f64 const *illuminant_spd,
                                            alwan_illuminant illuminant, alwan_ctx *ctx) {
    double *E;
    size_t o, i, n;
    alwan_status st;
    if (!material) return ALWAN_E_INVALID;
    n = material->n;
    E = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    if (!E) return ALWAN_E_NOMEM;
    st = fluo_illuminant(E, material, illuminant_spd, illuminant, ctx);
    if (st == ALWAN_OK) {
        for (o = 0; o < n; o++) {
            double L = 0.0;
            double const *row = material->D + o * n;
            for (i = 0; i < n; i++) L += row[i] * E[i];
            if (radiance_out) radiance_out[o] = L;
            if (total_out) total_out[o] = E[o] > 0.0 ? L / E[o] : 0.0;
            if (fluorescent_out) fluorescent_out[o] = E[o] > 0.0 ? (L - row[o] * E[o]) / E[o] : 0.0;
        }
    }
    ALWAN_FREE(E);
    return st;
}

alwan_status alwan_fluorescent_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_xyz_f64 *xyz_out, alwan_fluorescent const *material,
                                       alwan_spd_f64 const *illuminant_spd, alwan_illuminant illuminant,
                                       alwan_rgb_space space, alwan_observer_type observer, alwan_ctx *ctx) {
    alwan_spd_f64 Ls, Es, unit;
    alwan_xyz_f64 xyz = {0, 0, 0}, white = {0, 0, 0}, dst_white, adapted;
    alwan_rgb_space_desc_f64 desc;
    alwan_mat3x3_f64 cat;
    alwan_status st;
    size_t n, k;
    if (!rgb_out || !material) return ALWAN_E_INVALID;
    n = material->n;
    st = alwan_rgb_get_space_descriptor_f64(&desc, space, ctx);
    if (st != ALWAN_OK) return st;
    memset(&Ls, 0, sizeof Ls), memset(&Es, 0, sizeof Es), memset(&unit, 0, sizeof unit);
    st = alwan_spd_create_f64(&Ls, material->lmin, material->lmax, n, ctx);
    if (st == ALWAN_OK) st = alwan_spd_create_f64(&Es, material->lmin, material->lmax, n, ctx);
    if (st == ALWAN_OK) st = alwan_spd_create_f64(&unit, material->lmin, material->lmax, n, ctx);
    if (st == ALWAN_OK) st = fluo_illuminant(Es.values, material, illuminant_spd, illuminant, ctx);
    if (st == ALWAN_OK) st = alwan_fluorescent_radiance_f64(Ls.values, NULL, NULL, material, illuminant_spd, illuminant, ctx);
    if (st == ALWAN_OK) {
        for (k = 0; k < n; k++) unit.values[k] = 1.0;
        st = alwan_xyz_from_spd_f64(&xyz, &Ls, &unit, observer, ALWAN_INTEGRATE_TRAPEZOID, 0.0, ctx);
    }
    if (st == ALWAN_OK) st = alwan_xyz_from_spd_f64(&white, &Es, &unit, observer, ALWAN_INTEGRATE_TRAPEZOID, 0.0, ctx);
    alwan_spd_destroy_f64(&Ls, ctx), alwan_spd_destroy_f64(&Es, ctx), alwan_spd_destroy_f64(&unit, ctx);
    if (st != ALWAN_OK) return st;
    if (!(white.y > 0.0)) return ALWAN_E_DIVZERO;
    xyz.x /= white.y, xyz.y /= white.y, xyz.z /= white.y;
    white.x /= white.y, white.z /= white.y, white.y = 1.0;
    if (xyz_out) *xyz_out = xyz;
    dst_white.x = desc.white_xy[0] / desc.white_xy[1];
    dst_white.y = 1.0;
    dst_white.z = (1.0 - desc.white_xy[0] - desc.white_xy[1]) / desc.white_xy[1];
    if (ALWAN_ABS_F64(white.x - dst_white.x) < 1e-12 && ALWAN_ABS_F64(white.z - dst_white.z) < 1e-12) {
        adapted = xyz;
    } else {
        st = alwan_cat_matrix_f64(&cat, &white, &dst_white, ALWAN_CAT_BRADFORD);
        if (st != ALWAN_OK) return st;
        adapted.x = cat.m[0] * xyz.x + cat.m[1] * xyz.y + cat.m[2] * xyz.z;
        adapted.y = cat.m[3] * xyz.x + cat.m[4] * xyz.y + cat.m[5] * xyz.z;
        adapted.z = cat.m[6] * xyz.x + cat.m[7] * xyz.y + cat.m[8] * xyz.z;
    }
    return alwan_xyz_to_rgb_f64(rgb_out, &desc, &adapted);
}

/* f32 forms: the spectrum widened, computed in double, narrowed. */
static alwan_status fluo_widen(alwan_spd_f64 *out, double **buf, alwan_spd_f32 const *in) {
    size_t k;
    *buf = NULL;
    if (!in) return ALWAN_OK;
    if (!in->values || in->count < 2) return ALWAN_E_INVALID;
    *buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(in->count, sizeof(double)), sizeof(double));
    if (!*buf) return ALWAN_E_NOMEM;
    for (k = 0; k < in->count; k++) (*buf)[k] = (double)in->values[k];
    out->values = *buf;
    out->wavelength_min = (double)in->wavelength_min;
    out->wavelength_max = (double)in->wavelength_max;
    out->count = in->count;
    return ALWAN_OK;
}

alwan_status alwan_fluorescent_radiance_f32(float *radiance_out, float *total_out, float *fluorescent_out,
                                            alwan_fluorescent const *material, alwan_spd_f32 const *illuminant_spd,
                                            alwan_illuminant illuminant, alwan_ctx *ctx) {
    alwan_spd_f64 wide;
    double *buf, *tmp;
    size_t n, k;
    alwan_status st;
    if (!material) return ALWAN_E_INVALID;
    n = material->n;
    st = fluo_widen(&wide, &buf, illuminant_spd);
    if (st != ALWAN_OK) return st;
    tmp = (double *)ALWAN_ALLOC(alwan_safe_array_size(3 * n, sizeof(double)), sizeof(double));
    if (!tmp) { ALWAN_FREE(buf); return ALWAN_E_NOMEM; }
    st = alwan_fluorescent_radiance_f64(tmp, tmp + n, tmp + 2 * n, material, illuminant_spd ? &wide : NULL, illuminant, ctx);
    if (st == ALWAN_OK) {
        for (k = 0; k < n; k++) {
            if (radiance_out) radiance_out[k] = (float)tmp[k];
            if (total_out) total_out[k] = (float)tmp[n + k];
            if (fluorescent_out) fluorescent_out[k] = (float)tmp[2 * n + k];
        }
    }
    ALWAN_FREE(tmp);
    ALWAN_FREE(buf);
    return st;
}

alwan_status alwan_fluorescent_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_xyz_f32 *xyz_out, alwan_fluorescent const *material,
                                       alwan_spd_f32 const *illuminant_spd, alwan_illuminant illuminant,
                                       alwan_rgb_space space, alwan_observer_type observer, alwan_ctx *ctx) {
    alwan_spd_f64 wide;
    double *buf;
    alwan_rgb_f64 rgb;
    alwan_xyz_f64 xyz;
    alwan_status st;
    if (!rgb_out || !material) return ALWAN_E_INVALID;
    st = fluo_widen(&wide, &buf, illuminant_spd);
    if (st != ALWAN_OK) return st;
    st = alwan_fluorescent_rgb_f64(&rgb, &xyz, material, illuminant_spd ? &wide : NULL, illuminant, space, observer, ctx);
    ALWAN_FREE(buf);
    if (st != ALWAN_OK) return st;
    rgb_out->r = (float)rgb.r, rgb_out->g = (float)rgb.g, rgb_out->b = (float)rgb.b;
    if (xyz_out) xyz_out->x = (float)xyz.x, xyz_out->y = (float)xyz.y, xyz_out->z = (float)xyz.z;
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- *
 * Sampling, through the core readers (the same arithmetic a shader runs)
 * ---------------------------------------------------------------- */

static int fluo_u_ok(double u) { return u >= 0.0 && u <= 1.0; }

alwan_status alwan_fluorescent_sample_f64(double *lambda_out, double *pdf_out, double *weight_out, int *reradiated_out,
                                          double lambda, double u_event, double u_lambda,
                                          alwan_fluorescent_direction direction, alwan_fluorescent const *material) {
    int k, n, base;
    double T, P;
    if (!lambda_out || !weight_out || !material) return ALWAN_E_INVALID;
    if (direction != ALWAN_FLUORESCENT_OUTGOING && direction != ALWAN_FLUORESCENT_INCOMING) return ALWAN_E_INVALID;
    if (!fluo_u_ok(u_event) || !fluo_u_ok(u_lambda)) return ALWAN_E_RANGE;
    n = (int)material->n;
    k = alwan_fluo_bin_f64_v(lambda, material->lmin, material->h, n);
    if (k < 0) return ALWAN_E_RANGE;
    base = (direction == ALWAN_FLUORESCENT_INCOMING ? n : 0) * (n + 3) + k * (n + 3);
    T = alwan_fluo_total_f64_v(material->tab64, base);
    P = alwan_fluo_reradiate_f64_v(material->tab64, base);
    *weight_out = T;
    if (P > 0.0 && (u_event < P || P >= 1.0)) {
        alwan_vec2_f64 const s = alwan_fluo_sample_lambda_f64_v(material->tab64, base, n, material->lmin, material->h, u_lambda);
        *lambda_out = s.v[0];
        if (pdf_out) *pdf_out = P * s.v[1];
        if (reradiated_out) *reradiated_out = 1;
    } else {
        *lambda_out = lambda;
        if (pdf_out) *pdf_out = T > 0.0 ? 1.0 - P : 0.0;
        if (reradiated_out) *reradiated_out = 0;
    }
    return ALWAN_OK;
}

alwan_status alwan_fluorescent_sample_f32(float *lambda_out, float *pdf_out, float *weight_out, int *reradiated_out,
                                          float lambda, float u_event, float u_lambda,
                                          alwan_fluorescent_direction direction, alwan_fluorescent const *material) {
    int k, n, base;
    float T, P;
    if (!lambda_out || !weight_out || !material) return ALWAN_E_INVALID;
    if (direction != ALWAN_FLUORESCENT_OUTGOING && direction != ALWAN_FLUORESCENT_INCOMING) return ALWAN_E_INVALID;
    if (!fluo_u_ok((double)u_event) || !fluo_u_ok((double)u_lambda)) return ALWAN_E_RANGE;
    n = (int)material->n;
    k = alwan_fluo_bin_f32_v(lambda, (float)material->lmin, (float)material->h, n);
    if (k < 0) return ALWAN_E_RANGE;
    base = (direction == ALWAN_FLUORESCENT_INCOMING ? n : 0) * (n + 3) + k * (n + 3);
    T = alwan_fluo_total_f32_v(material->tab32, base);
    P = alwan_fluo_reradiate_f32_v(material->tab32, base);
    *weight_out = T;
    if (P > 0.0f && (u_event < P || P >= 1.0f)) {
        alwan_vec2_f32 const s = alwan_fluo_sample_lambda_f32_v(material->tab32, base, n, (float)material->lmin, (float)material->h, u_lambda);
        *lambda_out = s.v[0];
        if (pdf_out) *pdf_out = P * s.v[1];
        if (reradiated_out) *reradiated_out = 1;
    } else {
        *lambda_out = lambda;
        if (pdf_out) *pdf_out = T > 0.0f ? 1.0f - P : 0.0f;
        if (reradiated_out) *reradiated_out = 0;
    }
    return ALWAN_OK;
}

alwan_status alwan_fluorescent_pdf_f64(double *pdf_out, double lambda, double lambda_sampled,
                                       alwan_fluorescent_direction direction, alwan_fluorescent const *material) {
    int k, j, n, base;
    if (!pdf_out || !material) return ALWAN_E_INVALID;
    if (direction != ALWAN_FLUORESCENT_OUTGOING && direction != ALWAN_FLUORESCENT_INCOMING) return ALWAN_E_INVALID;
    n = (int)material->n;
    k = alwan_fluo_bin_f64_v(lambda, material->lmin, material->h, n);
    j = alwan_fluo_bin_f64_v(lambda_sampled, material->lmin, material->h, n);
    *pdf_out = 0.0;
    if (k < 0 || j < 0 || j == k) return ALWAN_OK;
    base = (direction == ALWAN_FLUORESCENT_INCOMING ? n : 0) * (n + 3) + k * (n + 3);
    *pdf_out = alwan_fluo_reradiate_f64_v(material->tab64, base) * alwan_fluo_bin_pdf_f64_v(material->tab64, base, j, material->h);
    return ALWAN_OK;
}

alwan_status alwan_fluorescent_pdf_f32(float *pdf_out, float lambda, float lambda_sampled,
                                       alwan_fluorescent_direction direction, alwan_fluorescent const *material) {
    int k, j, n, base;
    if (!pdf_out || !material) return ALWAN_E_INVALID;
    if (direction != ALWAN_FLUORESCENT_OUTGOING && direction != ALWAN_FLUORESCENT_INCOMING) return ALWAN_E_INVALID;
    n = (int)material->n;
    k = alwan_fluo_bin_f32_v(lambda, (float)material->lmin, (float)material->h, n);
    j = alwan_fluo_bin_f32_v(lambda_sampled, (float)material->lmin, (float)material->h, n);
    *pdf_out = 0.0f;
    if (k < 0 || j < 0 || j == k) return ALWAN_OK;
    base = (direction == ALWAN_FLUORESCENT_INCOMING ? n : 0) * (n + 3) + k * (n + 3);
    *pdf_out = alwan_fluo_reradiate_f32_v(material->tab32, base) * alwan_fluo_bin_pdf_f32_v(material->tab32, base, j, (float)material->h);
    return ALWAN_OK;
}

alwan_status alwan_fluorescent_get_layout_f32(alwan_fluorescent_layout_f32 *layout, alwan_fluorescent const *material) {
    if (!layout || !material) return ALWAN_E_INVALID;
    layout->table = material->tab32;
    layout->row_stride = material->n + 3;
    layout->incoming_base = material->n * layout->row_stride;
    layout->table_count = 2 * layout->incoming_base;
    layout->count = material->n;
    layout->lambda_min = (float)material->lmin;
    layout->step = (float)material->h;
    return ALWAN_OK;
}
