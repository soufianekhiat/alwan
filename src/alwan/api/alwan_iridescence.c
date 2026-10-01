/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Thin-film iridescence for rendering, after Belcour and Barla, "A Practical Extension to
 * Microfacet Theory for the Modeling of Varying Iridescence", ACM TOG 36(4), SIGGRAPH 2017.
 * Their key step is to integrate the Airy reflectance of the film against the colour
 * matching functions term by term of its Fourier series, so the colour comes out without
 * sampling the spectrum and without the aliasing a coarse spectral sampling suffers on a
 * thick film. They fit the CMFs with Gaussians to integrate in closed form; alwan instead
 * takes the exact Fourier transform of the CMFs times the illuminant, a piecewise-linear
 * function of wavenumber integrated exactly segment by segment, and tabulates it once per
 * space, illuminant and observer (alwan_iridescence_create). The paper holds the indices
 * constant over the spectrum; alwan can split the spectrum into bands of equal colour
 * weight, each with its own table and its own indices, so a dispersive film (an oxide on
 * titanium) keeps its colour. The per-direction math is in core/alwan_iridescence_core.inc
 * and the series in core/alwan_iridescence_reader.inc, so a shader evaluates what
 * alwan_iridescence_fresnel_rgb does. alwan's own code, from the paper's equations.
 * Suite 288.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>
#include "../core/alwan_table_core.h"
#include "../core/alwan_ibl_core.h"
#include "../core/alwan_iridescence_core.h"

#define ALWAN__IR_PI 3.14159265358979323846
#define ALWAN__IR_DEFAULT_OPD_MAX 40000.0
#define ALWAN__IR_DEFAULT_OPD_STEP 2.0
#define ALWAN__IR_DEFAULT_TERMS 64
#define ALWAN__IR_DEFAULT_TOLERANCE 1e-7
#define ALWAN__IR_MAX_TERMS 256
#define ALWAN__IR_MAX_COUNT 2000001
#define ALWAN__IR_MAX_BANDS ALWAN_IRIDESCENCE_MAX_BANDS

struct alwan_iridescence_s {
    size_t count;          /* entries per band */
    size_t bands;
    double opd_step;
    int max_terms;
    double tolerance;
    double nu[ALWAN__IR_MAX_BANDS];   /* each band's mean wavenumber, its demodulation */
    double white_xyz[3];   /* the illuminant's white, Y = 1, unadapted */
    double white_rgb[3];   /* the same, adapted to the space and in RGB: (1, 1, 1) */
    double *rgb;           /* bands * count * 6, demodulated */
    double *xyz;           /* bands * count * 6, demodulated, unadapted */
    float *rgb32;
    float *xyz32;
};

/* ---------------------------------------------------------------- *
 * The Fourier transform of a piecewise-linear function of wavenumber
 * ---------------------------------------------------------------- */

/* E(t) = integral_0^1 e^(i t s) ds and B(t) = integral_0^1 s e^(i t s) ds, as (re, im). */
static void alwan__ir_eb(double *ere, double *eim, double *bre, double *bim, double t) {
    if (ALWAN_ABS_F64(t) < 1e-3) {
        double const t2 = t * t;
        *ere = 1.0 - t2 / 6.0 + t2 * t2 / 120.0;
        *eim = t / 2.0 - t * t2 / 24.0;
        *bre = 0.5 - t2 / 8.0 + t2 * t2 / 144.0;
        *bim = t / 3.0 - t * t2 / 30.0;
    } else {
        double const c = ALWAN_COS_F64(t), s = ALWAN_SIN_F64(t);
        *ere = s / t;
        *eim = (1.0 - c) / t;
        *bre = s / t + (c - 1.0) / (t * t);
        *bim = -c / t + s / (t * t);
    }
}

/* A uniform-grid SPD at lambda, linear, 0 outside it. */
static double alwan__ir_spd_at(alwan_spd_f64 const *s, double lambda) {
    double pos, fr;
    size_t lo = 0, hi;
    if (s->count < 2 || !(lambda >= s->wavelength_min && lambda <= s->wavelength_max)) return 0.0;
    pos = (lambda - s->wavelength_min) / (s->wavelength_max - s->wavelength_min) * (double)(s->count - 1);
    hi = s->count - 1;
    while (hi - lo > 1) {   /* the floor of pos, counted rather than cast */
        size_t const mid = lo + (hi - lo) / 2;
        if ((double)mid <= pos) lo = mid; else hi = mid;
    }
    fr = pos - (double)lo;
    if (fr > 1.0) fr = 1.0;
    return s->values[lo] + (s->values[lo + 1] - s->values[lo]) * fr;
}

static alwan_status alwan__ir_build(alwan_iridescence *s, alwan_rgb_space space, alwan_illuminant illuminant,
                                    alwan_observer_type observer, alwan_ctx *ctx) {
    alwan_spd_f64 xb, yb, zb, illum;
    alwan_rgb_space_desc_f64 desc;
    alwan_mat3x3_f64 cat;
    double *nu = NULL, *g = NULL, *segw = NULL;   /* nodes ascending in wavenumber, three weights a node */
    size_t *seg_band = NULL;
    double total = 0.0, norm = 1.0;
    int use_cat;
    size_t n = 0, i, j, b;
    alwan_status st;
    memset(&xb, 0, sizeof(xb));
    memset(&yb, 0, sizeof(yb));
    memset(&zb, 0, sizeof(zb));
    memset(&illum, 0, sizeof(illum));
    st = alwan_rgb_get_space_descriptor_f64(&desc, space, ctx);
    if (st == ALWAN_OK) st = alwan_spd_observer_f64(&xb, &yb, &zb, observer, ctx);
    if (st == ALWAN_OK) st = alwan_spd_illuminant_f64(&illum, illuminant, ctx);
    if (st == ALWAN_OK && (xb.count < 2 || yb.count != xb.count || zb.count != xb.count)) st = ALWAN_E_INVALID;
    if (st == ALWAN_OK && xb.count - 1 < s->bands) st = ALWAN_E_INVALID;
    if (st == ALWAN_OK) {
        n = xb.count;
        nu = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
        g = (double *)ALWAN_ALLOC(alwan_safe_array_size(n * 3, sizeof(double)), sizeof(double));
        segw = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
        seg_band = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(size_t)), sizeof(size_t));
        if (!nu || !g || !segw || !seg_band) st = ALWAN_E_NOMEM;
    }
    if (st == ALWAN_OK) {
        /* node k ascends in wavenumber: the CMFs' sample n - 1 - k */
        for (i = 0; i < n; i++) {
            size_t const src = n - 1 - i;
            double const lambda = xb.wavelength_min + (xb.wavelength_max - xb.wavelength_min) * (double)src / (double)(n - 1);
            double const e = alwan__ir_spd_at(&illum, lambda);
            /* d lambda = -d nu / nu^2: the weight per unit wavenumber is w lambda^2 */
            double const jac = lambda * lambda;
            nu[i] = 1.0 / lambda;
            g[i * 3 + 0] = xb.values[src] * e * jac;
            g[i * 3 + 1] = yb.values[src] * e * jac;
            g[i * 3 + 2] = zb.values[src] * e * jac;
        }
        /* each segment's share of the colour weight x + y + z; bands of equal weight */
        for (i = 0; i + 1 < n; i++) {
            double const h = nu[i + 1] - nu[i];
            segw[i] = 0.5 * h * (g[i * 3] + g[i * 3 + 1] + g[i * 3 + 2] + g[(i + 1) * 3] + g[(i + 1) * 3 + 1] + g[(i + 1) * 3 + 2]);
            total += segw[i];
        }
        if (!(total > 0.0)) st = ALWAN_E_DIVZERO;
    }
    if (st == ALWAN_OK) {
        double run = 0.0;
        size_t band = 0;
        for (i = 0; i + 1 < n; i++) {
            /* the band of the segment's middle in the running weight, never decreasing, and
             * leaving every later band at least one segment */
            double const mid = run + 0.5 * segw[i];
            size_t const left = n - 1 - i;   /* segments from this one on */
            while (band + 1 < s->bands && mid > total * (double)(band + 1) / (double)s->bands) band++;
            if (s->bands - band > left) band = s->bands - left;
            seg_band[i] = band;
            run += segw[i];
        }
        for (b = 0; b < s->bands; b++) {   /* each band's weighted mean wavenumber */
            double num = 0.0, dnm = 0.0;
            for (i = 0; i + 1 < n; i++) {
                if (seg_band[i] != b) continue;
                num += segw[i] * 0.5 * (nu[i] + nu[i + 1]);
                dnm += segw[i];
            }
            s->nu[b] = dnm > 0.0 ? num / dnm : 0.0;
            if (!(s->nu[b] > 0.0)) {   /* a band with no weight: its middle */
                size_t first = n, last = 0;
                for (i = 0; i + 1 < n; i++) {
                    if (seg_band[i] != b) continue;
                    if (first == n) first = i;
                    last = i + 1;
                }
                s->nu[b] = first < n ? 0.5 * (nu[first] + nu[last]) : 1.0 / 550.0;
            }
        }
    }
    for (b = 0; st == ALWAN_OK && b < s->bands; b++) {
        double *xb_tab = s->xyz + b * s->count * 6;
        for (j = 0; j < s->count; j++) {
            double const f = (double)j * s->opd_step;
            double const k = 2.0 * ALWAN__IR_PI * f;
            double gr[3] = {0.0, 0.0, 0.0}, gi[3] = {0.0, 0.0, 0.0};
            double cr, ci;
            int c;
            for (i = 0; i + 1 < n; i++) {
                double h, ere, eim, bre, bim, are, aim, e0r, e0i;
                if (seg_band[i] != b) continue;
                h = nu[i + 1] - nu[i];
                e0r = ALWAN_COS_F64(k * nu[i]);
                e0i = ALWAN_SIN_F64(k * nu[i]);
                alwan__ir_eb(&ere, &eim, &bre, &bim, k * h);
                are = ere - bre;   /* A = integral_0^1 (1 - s) e^(i t s) ds */
                aim = eim - bim;
                for (c = 0; c < 3; c++) {
                    /* h e^(i k nu_i) (g_i A + g_(i+1) B) */
                    double const sr = g[i * 3 + c] * are + g[(i + 1) * 3 + c] * bre;
                    double const si = g[i * 3 + c] * aim + g[(i + 1) * 3 + c] * bim;
                    gr[c] += h * (e0r * sr - e0i * si);
                    gi[c] += h * (e0r * si + e0i * sr);
                }
            }
            /* demodulate: D = G e^(-i 2 pi f nu_b) */
            cr = ALWAN_COS_F64(k * s->nu[b]);
            ci = -ALWAN_SIN_F64(k * s->nu[b]);
            for (c = 0; c < 3; c++) {
                xb_tab[j * 6 + c * 2] = gr[c] * cr - gi[c] * ci;
                xb_tab[j * 6 + c * 2 + 1] = gr[c] * ci + gi[c] * cr;
            }
        }
    }
    if (st == ALWAN_OK) {
        norm = 0.0;
        for (b = 0; b < s->bands; b++) norm += s->xyz[b * s->count * 6 + 2];   /* Re G_Y(0) over the bands */
        if (!(norm > 0.0)) st = ALWAN_E_DIVZERO;
    }
    if (st == ALWAN_OK) {
        alwan_xyz_f64 white, dst;
        size_t const total_entries = s->bands * s->count;
        for (j = 0; j < total_entries * 6; j++) s->xyz[j] /= norm;
        s->white_xyz[0] = s->white_xyz[1] = s->white_xyz[2] = 0.0;
        for (b = 0; b < s->bands; b++) {
            s->white_xyz[0] += s->xyz[b * s->count * 6];
            s->white_xyz[1] += s->xyz[b * s->count * 6 + 2];
            s->white_xyz[2] += s->xyz[b * s->count * 6 + 4];
        }
        white.x = s->white_xyz[0];
        white.y = s->white_xyz[1];
        white.z = s->white_xyz[2];
        dst.x = desc.white_xy[0] / desc.white_xy[1];
        dst.y = 1.0;
        dst.z = (1.0 - desc.white_xy[0] - desc.white_xy[1]) / desc.white_xy[1];
        use_cat = !(ALWAN_ABS_F64(white.x - dst.x) < 1e-12 && ALWAN_ABS_F64(white.z - dst.z) < 1e-12);
        if (use_cat) st = alwan_cat_matrix_f64(&cat, &white, &dst, ALWAN_CAT_BRADFORD);
        for (j = 0; st == ALWAN_OK && j < total_entries; j++) {
            int part;
            for (part = 0; part < 2 && st == ALWAN_OK; part++) {
                alwan_xyz_f64 v, a;
                alwan_rgb_f64 rgb;
                v.x = s->xyz[j * 6 + part];
                v.y = s->xyz[j * 6 + 2 + part];
                v.z = s->xyz[j * 6 + 4 + part];
                if (use_cat) {
                    a.x = cat.m[0] * v.x + cat.m[1] * v.y + cat.m[2] * v.z;
                    a.y = cat.m[3] * v.x + cat.m[4] * v.y + cat.m[5] * v.z;
                    a.z = cat.m[6] * v.x + cat.m[7] * v.y + cat.m[8] * v.z;
                } else {
                    a = v;
                }
                st = alwan_xyz_to_rgb_f64(&rgb, &desc, &a);
                s->rgb[j * 6 + part] = rgb.r;
                s->rgb[j * 6 + 2 + part] = rgb.g;
                s->rgb[j * 6 + 4 + part] = rgb.b;
            }
        }
        if (st == ALWAN_OK) {
            s->white_rgb[0] = s->white_rgb[1] = s->white_rgb[2] = 0.0;
            for (b = 0; b < s->bands; b++) {
                s->white_rgb[0] += s->rgb[b * s->count * 6];
                s->white_rgb[1] += s->rgb[b * s->count * 6 + 2];
                s->white_rgb[2] += s->rgb[b * s->count * 6 + 4];
            }
            for (j = 0; j < total_entries * 6; j++) {
                s->rgb32[j] = (float)s->rgb[j];
                s->xyz32[j] = (float)s->xyz[j];
            }
        }
    }
    ALWAN_FREE(nu);
    ALWAN_FREE(g);
    ALWAN_FREE(segw);
    ALWAN_FREE(seg_band);
    alwan_spd_destroy_f64(&xb, ctx);
    alwan_spd_destroy_f64(&yb, ctx);
    alwan_spd_destroy_f64(&zb, ctx);
    alwan_spd_destroy_f64(&illum, ctx);
    return st;
}

alwan_status alwan_iridescence_create(alwan_iridescence **out, alwan_rgb_space space, alwan_illuminant illuminant,
                                      alwan_observer_type observer, alwan_iridescence_params const *params, alwan_ctx *ctx) {
    alwan_iridescence *s;
    double opd_max = ALWAN__IR_DEFAULT_OPD_MAX, opd_step = ALWAN__IR_DEFAULT_OPD_STEP, tol = ALWAN__IR_DEFAULT_TOLERANCE;
    int terms = ALWAN__IR_DEFAULT_TERMS, bands = 1;
    size_t count = 1, entries;
    double reach = 0.0;
    alwan_status st;
    if (!out) return ALWAN_E_INVALID;
    *out = NULL;
    if (params) {
        if (params->opd_max_nm != 0.0) opd_max = params->opd_max_nm;
        if (params->opd_step_nm != 0.0) opd_step = params->opd_step_nm;
        if (params->max_terms != 0) terms = params->max_terms;
        if (params->tolerance != 0.0) tol = params->tolerance;
        if (params->bands != 0) bands = params->bands;
    }
    if (!(opd_step > 0.0 && opd_max >= opd_step && opd_max < 1e7) || !(tol >= 0.0 && tol < 1.0)) return ALWAN_E_INVALID;
    if (terms < 1 || terms > ALWAN__IR_MAX_TERMS || bands < 1 || bands > ALWAN__IR_MAX_BANDS) return ALWAN_E_INVALID;
    while (reach + 0.5 * opd_step < opd_max) {   /* entries at 0, step, ... up to opd_max, counted */
        reach += opd_step;
        count++;
        if (count > ALWAN__IR_MAX_COUNT) return ALWAN_E_RANGE;
    }
    entries = count * (size_t)bands;
    s = (alwan_iridescence *)ALWAN_ALLOC(sizeof(*s), sizeof(double));
    if (!s) return ALWAN_E_NOMEM;
    memset(s, 0, sizeof(*s));
    s->count = count;
    s->bands = (size_t)bands;
    s->opd_step = opd_step;
    s->max_terms = terms;
    s->tolerance = tol;
    s->rgb = (double *)ALWAN_ALLOC(alwan_safe_array_size(entries * 6, sizeof(double)), sizeof(double));
    s->xyz = (double *)ALWAN_ALLOC(alwan_safe_array_size(entries * 6, sizeof(double)), sizeof(double));
    s->rgb32 = (float *)ALWAN_ALLOC(alwan_safe_array_size(entries * 6, sizeof(float)), sizeof(float));
    s->xyz32 = (float *)ALWAN_ALLOC(alwan_safe_array_size(entries * 6, sizeof(float)), sizeof(float));
    if (!s->rgb || !s->xyz || !s->rgb32 || !s->xyz32) {
        alwan_iridescence_destroy(s, ctx);
        return ALWAN_E_NOMEM;
    }
    st = alwan__ir_build(s, space, illuminant, observer, ctx);
    if (st != ALWAN_OK) {
        alwan_iridescence_destroy(s, ctx);
        return st;
    }
    *out = s;
    return ALWAN_OK;
}

void alwan_iridescence_destroy(alwan_iridescence *s, alwan_ctx *ctx) {
    ALWAN_UNUSED(ctx);
    if (!s) return;
    ALWAN_FREE(s->rgb);
    ALWAN_FREE(s->xyz);
    ALWAN_FREE(s->rgb32);
    ALWAN_FREE(s->xyz32);
    ALWAN_FREE(s);
}

alwan_status alwan_iridescence_get_info(alwan_iridescence_info *info, alwan_iridescence const *s) {
    size_t b;
    if (!info || !s) return ALWAN_E_INVALID;
    memset(info, 0, sizeof(*info));
    info->count = s->count;
    info->bands = s->bands;
    info->opd_step_nm = s->opd_step;
    info->max_terms = s->max_terms;
    info->tolerance = s->tolerance;
    for (b = 0; b < s->bands; b++) {
        info->nu[b] = s->nu[b];
        info->wavelength_nm[b] = 1.0 / s->nu[b];
    }
    memcpy(info->white_xyz, s->white_xyz, sizeof(info->white_xyz));
    memcpy(info->white_rgb, s->white_rgb, sizeof(info->white_rgb));
    return ALWAN_OK;
}

alwan_status alwan_iridescence_sensitivity_f64(alwan_f64 *out, size_t capacity, int xyz, alwan_iridescence const *s) {
    if (!out || !s) return ALWAN_E_INVALID;
    if (capacity < s->bands * s->count * 6) return ALWAN_E_RANGE;
    memcpy(out, xyz ? s->xyz : s->rgb, s->bands * s->count * 6 * sizeof(double));
    return ALWAN_OK;
}

alwan_status alwan_iridescence_sensitivity_f32(alwan_f32 *out, size_t capacity, int xyz, alwan_iridescence const *s) {
    if (!out || !s) return ALWAN_E_INVALID;
    if (capacity < s->bands * s->count * 6) return ALWAN_E_RANGE;
    memcpy(out, xyz ? s->xyz32 : s->rgb32, s->bands * s->count * 6 * sizeof(float));
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- *
 * The film
 * ---------------------------------------------------------------- */

static int alwan__ir_finite(double v) {
    return v >= -1.7976931348623157e308 && v <= 1.7976931348623157e308;
}

/* One film's indices with the defaults applied; ALWAN_E_INVALID for a NULL, a NaN or an
 * index that is not positive. */
static alwan_status alwan__ir_film(double *eta1, double *eta2, double *d, double *eta3, double *k3, alwan_iridescent_film const *film) {
    if (!film) return ALWAN_E_INVALID;
    *eta1 = film->ambient_n == 0.0 ? 1.0 : film->ambient_n;
    *eta2 = film->film_n;
    *d = film->thickness_nm;
    *eta3 = film->base_n;
    *k3 = film->base_k;
    if (!alwan__ir_finite(*eta1) || !alwan__ir_finite(*eta2) || !alwan__ir_finite(*d) || !alwan__ir_finite(*eta3) ||
        !alwan__ir_finite(*k3)) return ALWAN_E_INVALID;
    if (!(*eta1 > 0.0) || !(*eta2 > 0.0) || !(*d >= 0.0) || !(*eta3 >= 0.0) || !(*k3 >= 0.0) || !(*eta3 + *k3 > 0.0))
        return ALWAN_E_INVALID;
    return ALWAN_OK;
}

/* Every band's film valid, all of one thickness. */
static alwan_status alwan__ir_films(alwan_iridescent_film const *films, size_t bands) {
    size_t b;
    double e1, e2, d, e3, k3;
    if (!films) return ALWAN_E_INVALID;
    for (b = 0; b < bands; b++) {
        alwan_status const st = alwan__ir_film(&e1, &e2, &d, &e3, &k3, &films[b]);
        if (st != ALWAN_OK) return st;
        if (films[b].thickness_nm != films[0].thickness_nm) return ALWAN_E_INVALID;
    }
    return ALWAN_OK;
}

static int alwan__ir_pol_ok(alwan_polarization p) {
    return (int)p == 0 || (int)p == 1 || (int)p == 2;
}

alwan_status alwan_iridescence_reflectance_f64(alwan_f64 *R_out, alwan_f64 const *wavelengths_nm, size_t count, alwan_f64 cos_theta,
                                               alwan_iridescent_film const *film, alwan_polarization polarization) {
    double e1, e2, d, e3, k3;
    size_t i;
    alwan_status const st = alwan__ir_film(&e1, &e2, &d, &e3, &k3, film);
    if (st != ALWAN_OK) return st;
    if (!R_out || !wavelengths_nm || count == 0 || !alwan__ir_pol_ok(polarization)) return ALWAN_E_INVALID;
    if (!(cos_theta >= 0.0 && cos_theta <= 1.0)) return ALWAN_E_RANGE;
    for (i = 0; i < count; i++) {
        if (!(wavelengths_nm[i] > 0.0) || !alwan__ir_finite(wavelengths_nm[i])) return ALWAN_E_RANGE;
        R_out[i] = alwan_irid_reflectance_f64_v(e1, e2, d, e3, k3, cos_theta, wavelengths_nm[i], (int)polarization);
    }
    return ALWAN_OK;
}

alwan_status alwan_iridescence_reflectance_f32(alwan_f32 *R_out, alwan_f32 const *wavelengths_nm, size_t count, alwan_f32 cos_theta,
                                               alwan_iridescent_film const *film, alwan_polarization polarization) {
    double e1, e2, d, e3, k3;
    size_t i;
    alwan_status const st = alwan__ir_film(&e1, &e2, &d, &e3, &k3, film);
    if (st != ALWAN_OK) return st;
    if (!R_out || !wavelengths_nm || count == 0 || !alwan__ir_pol_ok(polarization)) return ALWAN_E_INVALID;
    if (!(cos_theta >= 0.0f && cos_theta <= 1.0f)) return ALWAN_E_RANGE;
    for (i = 0; i < count; i++) {
        if (!(wavelengths_nm[i] > 0.0f) || !alwan__ir_finite((double)wavelengths_nm[i])) return ALWAN_E_RANGE;
        R_out[i] = alwan_irid_reflectance_f32_v((float)e1, (float)e2, (float)d, (float)e3, (float)k3, cos_theta, wavelengths_nm[i],
                                               (int)polarization);
    }
    return ALWAN_OK;
}

/* The sum over the bands of the core's series, on the double or the float table. */
static void alwan__ir_eval64(double out[3], double const *tab, alwan_iridescence const *s, alwan_iridescent_film const *films,
                             double cos1) {
    size_t b;
    out[0] = out[1] = out[2] = 0.0;
    for (b = 0; b < s->bands; b++) {
        alwan_iridescent_film const *f = &films[b];
        alwan_vec3_f64 const v = alwan_iridescence_fresnel_band_f64_v(
            tab, (int)s->count, s->opd_step, s->nu[b], (int)b, s->max_terms, s->tolerance, f->ambient_n == 0.0 ? 1.0 : f->ambient_n,
            f->film_n, f->thickness_nm, f->base_n, f->base_k, cos1);
        out[0] += v.v[0];
        out[1] += v.v[1];
        out[2] += v.v[2];
    }
}

static void alwan__ir_eval32(float out[3], float const *tab, alwan_iridescence const *s, alwan_iridescent_film const *films, float cos1) {
    size_t b;
    out[0] = out[1] = out[2] = 0.0f;
    for (b = 0; b < s->bands; b++) {
        alwan_iridescent_film const *f = &films[b];
        alwan_vec3_f32 const v = alwan_iridescence_fresnel_band_f32_v(
            tab, (int)s->count, (float)s->opd_step, (float)s->nu[b], (int)b, s->max_terms, (float)s->tolerance,
            (float)(f->ambient_n == 0.0 ? 1.0 : f->ambient_n), (float)f->film_n, (float)f->thickness_nm, (float)f->base_n,
            (float)f->base_k, cos1);
        out[0] += v.v[0];
        out[1] += v.v[1];
        out[2] += v.v[2];
    }
}

alwan_status alwan_iridescence_fresnel_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_xyz_f64 *xyz_out, alwan_f64 cos_theta,
                                               alwan_iridescent_film const *film, alwan_iridescence const *s) {
    double v[3];
    alwan_status st;
    if ((!rgb_out && !xyz_out) || !s) return ALWAN_E_INVALID;
    st = alwan__ir_films(film, s->bands);
    if (st != ALWAN_OK) return st;
    if (!(cos_theta >= 0.0 && cos_theta <= 1.0)) return ALWAN_E_RANGE;
    if (rgb_out) {
        alwan__ir_eval64(v, s->rgb, s, film, cos_theta);
        rgb_out->r = v[0];
        rgb_out->g = v[1];
        rgb_out->b = v[2];
    }
    if (xyz_out) {
        alwan__ir_eval64(v, s->xyz, s, film, cos_theta);
        xyz_out->x = v[0];
        xyz_out->y = v[1];
        xyz_out->z = v[2];
    }
    return ALWAN_OK;
}

alwan_status alwan_iridescence_fresnel_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_xyz_f32 *xyz_out, alwan_f32 cos_theta,
                                               alwan_iridescent_film const *film, alwan_iridescence const *s) {
    float v[3];
    alwan_status st;
    if ((!rgb_out && !xyz_out) || !s) return ALWAN_E_INVALID;
    st = alwan__ir_films(film, s->bands);
    if (st != ALWAN_OK) return st;
    if (!(cos_theta >= 0.0f && cos_theta <= 1.0f)) return ALWAN_E_RANGE;
    if (rgb_out) {
        alwan__ir_eval32(v, s->rgb32, s, film, cos_theta);
        rgb_out->r = v[0];
        rgb_out->g = v[1];
        rgb_out->b = v[2];
    }
    if (xyz_out) {
        alwan__ir_eval32(v, s->xyz32, s, film, cos_theta);
        xyz_out->x = v[0];
        xyz_out->y = v[1];
        xyz_out->z = v[2];
    }
    return ALWAN_OK;
}

alwan_status alwan_iridescence_ggx_f64(alwan_rgb_f64 *out, alwan_f64 nov, alwan_f64 nol, alwan_f64 noh, alwan_f64 voh, alwan_f64 alpha,
                                       alwan_iridescent_film const *film, alwan_iridescence const *s) {
    alwan_rgb_f64 f;
    double scale;
    alwan_status st;
    if (!out || !s || !film) return ALWAN_E_INVALID;
    if (!(alpha >= 0.0 && alpha <= 1.0) || !(nov >= -1.0 && nov <= 1.0) || !(nol >= -1.0 && nol <= 1.0) || !(noh >= -1.0 && noh <= 1.0) ||
        !(voh >= -1.0 && voh <= 1.0)) return ALWAN_E_RANGE;
    out->r = out->g = out->b = 0.0;
    if (!(nov > 0.0) || !(nol > 0.0) || !(voh > 0.0)) return alwan__ir_films(film, s->bands);
    st = alwan_iridescence_fresnel_rgb_f64(&f, NULL, voh, film, s);
    if (st != ALWAN_OK) return st;
    scale = alwan_ibl_ggx_d_f64_v(noh, alpha) * alwan_ibl_smith_g2_f64_v(nov, nol, alpha) / (4.0 * nov * nol);
    out->r = f.r * scale;
    out->g = f.g * scale;
    out->b = f.b * scale;
    return ALWAN_OK;
}

alwan_status alwan_iridescence_ggx_f32(alwan_rgb_f32 *out, alwan_f32 nov, alwan_f32 nol, alwan_f32 noh, alwan_f32 voh, alwan_f32 alpha,
                                       alwan_iridescent_film const *film, alwan_iridescence const *s) {
    alwan_rgb_f32 f;
    float scale;
    alwan_status st;
    if (!out || !s || !film) return ALWAN_E_INVALID;
    if (!(alpha >= 0.0f && alpha <= 1.0f) || !(nov >= -1.0f && nov <= 1.0f) || !(nol >= -1.0f && nol <= 1.0f) ||
        !(noh >= -1.0f && noh <= 1.0f) || !(voh >= -1.0f && voh <= 1.0f)) return ALWAN_E_RANGE;
    out->r = out->g = out->b = 0.0f;
    if (!(nov > 0.0f) || !(nol > 0.0f) || !(voh > 0.0f)) return alwan__ir_films(film, s->bands);
    st = alwan_iridescence_fresnel_rgb_f32(&f, NULL, voh, film, s);
    if (st != ALWAN_OK) return st;
    scale = alwan_ibl_ggx_d_f32_v(noh, alpha) * alwan_ibl_smith_g2_f32_v(nov, nol, alpha) / (4.0f * nov * nol);
    out->r = f.r * scale;
    out->g = f.g * scale;
    out->b = f.b * scale;
    return ALWAN_OK;
}

/* The table of the Fresnel term over (cosine, thickness): the films' thickness is replaced
 * texel row by texel row. */
static alwan_status alwan__ir_table(void *out, int is_f32, size_t row_stride, size_t cos_count, size_t thickness_count,
                                    double tmin, double tmax, alwan_iridescent_film const *film, alwan_iridescence const *s) {
    alwan_iridescent_film f[ALWAN__IR_MAX_BANDS];
    size_t i, j, b;
    size_t const elem = is_f32 ? sizeof(float) : sizeof(double);
    alwan_status st;
    if (!out || !s || cos_count == 0 || thickness_count == 0) return ALWAN_E_INVALID;
    st = alwan__ir_films(film, s->bands);
    if (st != ALWAN_OK) return st;
    if (cos_count > 65536 || thickness_count > 65536) return ALWAN_E_RANGE;
    if (row_stride < cos_count * 3 * elem) return ALWAN_E_INVALID;
    if (!(tmin >= 0.0) || !(tmax >= tmin) || !alwan__ir_finite(tmax)) return ALWAN_E_RANGE;
    for (b = 0; b < s->bands; b++) f[b] = film[b];
    for (j = 0; j < thickness_count; j++) {
        unsigned char *row = (unsigned char *)out + j * row_stride;
        double const d = tmin + (tmax - tmin) * ((double)j + 0.5) / (double)thickness_count;
        for (b = 0; b < s->bands; b++) f[b].thickness_nm = d;
        for (i = 0; i < cos_count; i++) {
            double const cs = ((double)i + 0.5) / (double)cos_count;
            if (is_f32) {
                float v[3];
                alwan__ir_eval32(v, s->rgb32, s, f, (float)cs);
                ((float *)row)[i * 3 + 0] = v[0];
                ((float *)row)[i * 3 + 1] = v[1];
                ((float *)row)[i * 3 + 2] = v[2];
            } else {
                double v[3];
                alwan__ir_eval64(v, s->rgb, s, f, cs);
                ((double *)row)[i * 3 + 0] = v[0];
                ((double *)row)[i * 3 + 1] = v[1];
                ((double *)row)[i * 3 + 2] = v[2];
            }
        }
    }
    return ALWAN_OK;
}

alwan_status alwan_iridescence_table_f64(alwan_f64 *out, size_t row_stride, size_t cos_count, size_t thickness_count,
                                         alwan_f64 thickness_min_nm, alwan_f64 thickness_max_nm, alwan_iridescent_film const *film,
                                         alwan_iridescence const *s) {
    return alwan__ir_table(out, 0, row_stride, cos_count, thickness_count, thickness_min_nm, thickness_max_nm, film, s);
}

alwan_status alwan_iridescence_table_f32(alwan_f32 *out, size_t row_stride, size_t cos_count, size_t thickness_count,
                                         alwan_f32 thickness_min_nm, alwan_f32 thickness_max_nm, alwan_iridescent_film const *film,
                                         alwan_iridescence const *s) {
    return alwan__ir_table(out, 1, row_stride, cos_count, thickness_count, (double)thickness_min_nm, (double)thickness_max_nm, film, s);
}

/* ---------------------------------------------------------------- *
 * Indices from the refractive database or a caller's table
 * ---------------------------------------------------------------- */

static alwan_status alwan__ir_index(double *n, double *k, size_t material, alwan_refractive_table const *table, double lambda,
                                    alwan_interp_method interpolation) {
    if (table) {
        double nv = 0.0, kv = 0.0, x = lambda;
        alwan_status st;
        if (!table->wavelengths_nm || !table->n || table->count == 0) return ALWAN_E_INVALID;
        if (table->count == 1) {
            *n = table->n[0];
            *k = table->k ? table->k[0] : 0.0;
            return ALWAN_OK;
        }
        if (x < table->wavelengths_nm[0]) x = table->wavelengths_nm[0];   /* edge values held outside */
        if (x > table->wavelengths_nm[table->count - 1]) x = table->wavelengths_nm[table->count - 1];
        st = alwan_interpolate_f64(table->wavelengths_nm, table->n, table->count, &x, &nv, 1, interpolation);
        if (st == ALWAN_OK && table->k) st = alwan_interpolate_f64(table->wavelengths_nm, table->k, table->count, &x, &kv, 1, interpolation);
        if (st != ALWAN_OK) return ALWAN_E_INVALID;
        *n = nv;
        *k = kv;
        return ALWAN_OK;
    }
    if (material == ALWAN_REFRACTIVE_VACUUM) {
        *n = 1.0;
        *k = 0.0;
        return ALWAN_OK;
    }
    return alwan_refractive_index_sample_f64(n, k, material, lambda, interpolation, ALWAN_EXTRAPOLATE_CONSTANT);
}

alwan_status alwan_iridescent_film_from_materials(alwan_iridescent_film *out, size_t ambient, alwan_refractive_table const *ambient_table,
                                                  size_t film_material, alwan_refractive_table const *film_table, double thickness_nm,
                                                  size_t base, alwan_refractive_table const *base_table, double wavelength_nm,
                                                  alwan_interp_method interpolation) {
    double lambda = wavelength_nm == 0.0 ? 550.0 : wavelength_nm;
    double n1 = 0.0, k1 = 0.0, n2 = 0.0, k2 = 0.0, n3 = 0.0, k3 = 0.0;
    alwan_status st;
    if (!out) return ALWAN_E_INVALID;
    if (!(lambda > 0.0) || !alwan__ir_finite(lambda) || !(thickness_nm >= 0.0) || !alwan__ir_finite(thickness_nm)) return ALWAN_E_RANGE;
    st = alwan__ir_index(&n1, &k1, ambient, ambient_table, lambda, interpolation);
    if (st == ALWAN_OK) st = alwan__ir_index(&n2, &k2, film_material, film_table, lambda, interpolation);
    if (st == ALWAN_OK) st = alwan__ir_index(&n3, &k3, base, base_table, lambda, interpolation);
    if (st != ALWAN_OK) return st;
    if (!(n1 > 0.0) || !(n2 > 0.0) || !(n3 >= 0.0) || !(k3 >= 0.0)) return ALWAN_E_RANGE;
    /* the series needs a lossless ambient and film; a trace of absorption is dropped */
    if (k1 > 1e-2 || k2 > 1e-2) return ALWAN_E_RANGE;
    out->ambient_n = n1;
    out->film_n = n2;
    out->thickness_nm = thickness_nm;
    out->base_n = n3;
    out->base_k = k3;
    return ALWAN_OK;
}

alwan_status alwan_iridescent_films_from_materials(alwan_iridescent_film *out, size_t capacity, alwan_iridescence const *s, size_t ambient,
                                                   alwan_refractive_table const *ambient_table, size_t film_material,
                                                   alwan_refractive_table const *film_table, double thickness_nm, size_t base,
                                                   alwan_refractive_table const *base_table, alwan_interp_method interpolation) {
    size_t b;
    if (!out || !s) return ALWAN_E_INVALID;
    if (capacity < s->bands) return ALWAN_E_RANGE;
    for (b = 0; b < s->bands; b++) {
        alwan_status const st = alwan_iridescent_film_from_materials(&out[b], ambient, ambient_table, film_material, film_table, thickness_nm,
                                                                     base, base_table, 1.0 / s->nu[b], interpolation);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}
