/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Refractive indices from the refractiveindex.info database, and the colours of materials
 * and layer stacks made of them.
 *
 * alwan_refractive_index_*: n and k of any page of the database (data/alwan_data_refractive.h).
 * Tabulated pages are read between their samples with the caller's alwan_interp_method,
 * through alwan_interpolate on the 8 samples either side of the wavelength (every method
 * alwan has is local; Akima's vanishing-weight test then reads those 16 samples rather
 * than the whole table). SPRAGUE and LANCZOS need a uniform grid and refuse a non-uniform
 * window. Formula pages evaluate the database's dispersion formulas 1 to 9
 * ("Dispersion formulas", RefractiveIndex.INFO, 2014-06-29) with the wavelength in
 * micrometres, exactly, whatever the interpolation. Outside a quantity's range the caller's
 * alwan_extrapolate_mode applies.
 *
 * A caller's alwan_refractive_table goes through the same tabulated reader, in nm.
 *
 * alwan_refractive_stack_{T}: coherent layers on a substrate, by alwan_multilayer_tmm on the
 * media's indices sampled at the caller's wavelengths (edge values held outside the data).
 *
 * alwan_refractive_slab_{T}: a thick slab in the ambient, incoherent: the face reflectance R1
 * from Fresnel's amplitudes, the single-pass transmission A = exp(-4 pi Im(n cos t) d / lambda),
 * and the sum of the multiple reflections in power,
 *   R = R1 + (1 - R1)^2 R1 A^2 / (1 - R1^2 A^2),   T = (1 - R1)^2 A / (1 - R1^2 A^2).
 * The face reflects the same from inside as from outside (r' = -r), so one R1 serves both.
 *
 * alwan_reflectance_to_rgb_{T}: a spectrum's colour in a working space, normalised to a
 * perfect diffuser under the same light and adapted by Bradford to the space's white.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../data/alwan_data_refractive.h"
#include <string.h>

/* ---------------------------------------------------------------- *
 * Tabulated data: a database series (float32, um) or a caller's table (double, nm)
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_f32 const *lf, *vf;   /* the database's float32 samples, or NULL */
    double const *ld, *vd;      /* a caller's double samples, or NULL */
    size_t count;
} alwan__rx_tab;

static double alwan__rx_tl(alwan__rx_tab const *t, size_t i) { return t->lf ? (double)t->lf[i] : t->ld[i]; }
static double alwan__rx_tv(alwan__rx_tab const *t, size_t i) { return t->vf ? (double)t->vf[i] : t->vd[i]; }

#define ALWAN__RX_HALF 8   /* samples read on either side of the wavelength */

static int alwan__rx_interp_ok(alwan_interp_method m) {
    return m == ALWAN_INTERP_LINEAR || m == ALWAN_INTERP_CUBIC || m == ALWAN_INTERP_LANCZOS || m == ALWAN_INTERP_SPRAGUE ||
           m == ALWAN_INTERP_LAGRANGE || m == ALWAN_INTERP_AKIMA || m == ALWAN_INTERP_PCHIP;
}

/* The value at x inside [first, last] sample. */
static alwan_status alwan__rx_tab_in(double *out, alwan__rx_tab const *t, double x, alwan_interp_method m) {
    size_t lo = 0, hi = t->count - 1, a, b, i, n;
    double xs[2 * ALWAN__RX_HALF + 2], ys[2 * ALWAN__RX_HALF + 2];
    alwan_status st;
    if (t->count == 1) { *out = alwan__rx_tv(t, 0); return ALWAN_OK; }
    while (hi - lo > 1) {   /* the last sample at or below x */
        size_t const mid = lo + (hi - lo) / 2;
        if (alwan__rx_tl(t, mid) <= x) lo = mid; else hi = mid;
    }
    if (m == ALWAN_INTERP_LINEAR) {
        double const x0 = alwan__rx_tl(t, lo), x1 = alwan__rx_tl(t, hi);
        double const u = (x - x0) / (x1 - x0);
        *out = alwan__rx_tv(t, lo) + u * (alwan__rx_tv(t, hi) - alwan__rx_tv(t, lo));
        return ALWAN_OK;
    }
    a = lo >= ALWAN__RX_HALF ? lo - ALWAN__RX_HALF : 0;
    b = lo + ALWAN__RX_HALF + 2 < t->count ? lo + ALWAN__RX_HALF + 2 : t->count;
    n = b - a;
    for (i = 0; i < n; i++) xs[i] = alwan__rx_tl(t, a + i), ys[i] = alwan__rx_tv(t, a + i);
    if (m == ALWAN_INTERP_SPRAGUE || m == ALWAN_INTERP_LANCZOS) {
        /* both assume a uniform grid: refuse what is not one */
        double const step = (xs[n - 1] - xs[0]) / (double)(n - 1);
        for (i = 1; i < n; i++)
            if (ALWAN_ABS_F64((xs[i] - xs[i - 1]) - step) > 1e-3 * step) return ALWAN_E_INVALID;
    }
    if (x == xs[lo - a]) { *out = ys[lo - a]; return ALWAN_OK; }   /* a sample is a sample, for every method */
    st = alwan_interpolate_f64(xs, ys, n, &x, out, 1, m);
    return st == ALWAN_OK ? ALWAN_OK : ALWAN_E_INVALID;
}

static alwan_status alwan__rx_tab_eval(double *out, alwan__rx_tab const *t, double x, alwan_interp_method m, alwan_extrapolate_mode ex) {
    double const lo = alwan__rx_tl(t, 0), hi = alwan__rx_tl(t, t->count - 1);
    double edge, inner, at_edge, at_inner, v;
    if (x >= lo && x <= hi) return alwan__rx_tab_in(out, t, x, m);
    switch (ex) {
    case ALWAN_EXTRAPOLATE_ZERO: *out = 0.0; return ALWAN_OK;
    case ALWAN_EXTRAPOLATE_CONSTANT: *out = alwan__rx_tv(t, x < lo ? 0 : t->count - 1); return ALWAN_OK;
    case ALWAN_EXTRAPOLATE_LINEAR:
    case ALWAN_EXTRAPOLATE_LINEAR_CLAMP_ZERO: break;
    default: return ALWAN_E_INVALID;
    }
    if (t->count < 2) { *out = alwan__rx_tv(t, 0); return ALWAN_OK; }
    /* the slope of the last interval */
    edge = x < lo ? lo : hi;
    inner = alwan__rx_tl(t, x < lo ? 1 : t->count - 2);
    at_edge = alwan__rx_tv(t, x < lo ? 0 : t->count - 1);
    at_inner = alwan__rx_tv(t, x < lo ? 1 : t->count - 2);
    v = at_edge + (at_edge - at_inner) / (edge - inner) * (x - edge);
    if (ex == ALWAN_EXTRAPOLATE_LINEAR_CLAMP_ZERO && v < 0.0) v = 0.0;
    *out = v;
    return ALWAN_OK;
}

static alwan_status alwan__rx_user(double *n, double *k, alwan_refractive_table const *t, double wavelength_nm, alwan_interp_method m) {
    alwan__rx_tab tab;
    alwan_status st;
    size_t i;
    if (!t->wavelengths_nm || !t->n || t->count == 0) return ALWAN_E_INVALID;
    for (i = 1; i < t->count; i++)
        if (!(t->wavelengths_nm[i] > t->wavelengths_nm[i - 1])) return ALWAN_E_INVALID;
    tab.lf = tab.vf = NULL, tab.ld = t->wavelengths_nm, tab.vd = t->n, tab.count = t->count;
    st = alwan__rx_tab_eval(n, &tab, wavelength_nm, m, ALWAN_EXTRAPOLATE_CONSTANT);
    if (st != ALWAN_OK) return st;
    if (t->k) {
        tab.vd = t->k;
        return alwan__rx_tab_eval(k, &tab, wavelength_nm, m, ALWAN_EXTRAPOLATE_CONSTANT);
    }
    *k = 0.0;
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- *
 * The database
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_REFRACTIVE_DATA

/* a quiet NaN, from its bits: a page with no n */
static double alwan__rx_nan(void) {
    unsigned long long const bits = 0x7ff8000000000000ULL;
    double d;
    memcpy(&d, &bits, sizeof(d));
    return d;
}

/* n^2 of a formula series at lu um */
static double alwan__rx_formula(alwan__rx_series const *s, double lu) {
    double const *c = alwan__rx_coeffs + s->coeff_offset;
    int const nc = s->coeff_count;
    double const l2 = lu * lu;
    double v = 0.0;
    int i;
#define C(j) ((j) - 1 < nc ? c[(j) - 1] : 0.0)
    switch (s->formula) {
    case 1:   /* Sellmeier: n^2 - 1 = C1 + sum C(2i) l^2 / (l^2 - C(2i+1)^2) */
        v = C(1);
        for (i = 2; i + 1 <= nc; i += 2) v += C(i) * l2 / (l2 - C(i + 1) * C(i + 1));
        return v + 1.0;
    case 2:   /* Sellmeier-2: n^2 - 1 = C1 + sum C(2i) l^2 / (l^2 - C(2i+1)) */
        v = C(1);
        for (i = 2; i + 1 <= nc; i += 2) v += C(i) * l2 / (l2 - C(i + 1));
        return v + 1.0;
    case 3:   /* polynomial: n^2 = C1 + sum C(2i) l^C(2i+1) */
        v = C(1);
        for (i = 2; i + 1 <= nc; i += 2) v += C(i) * ALWAN_POW_F64(lu, C(i + 1));
        return v;
    case 4:   /* RefractiveIndex.INFO: n^2 = C1 + C2 l^C3 / (l^2 - C4^C5) + C6 l^C7 / (l^2 - C8^C9) + sum C(i) l^C(i+1) */
        v = C(1);
        if (nc >= 2) v += C(2) * ALWAN_POW_F64(lu, C(3)) / (l2 - ALWAN_POW_F64(C(4), C(5)));
        if (nc >= 6) v += C(6) * ALWAN_POW_F64(lu, C(7)) / (l2 - ALWAN_POW_F64(C(8), C(9)));
        for (i = 10; i + 1 <= nc; i += 2) v += C(i) * ALWAN_POW_F64(lu, C(i + 1));
        return v;
    case 5:   /* Cauchy: n = C1 + sum C(2i) l^C(2i+1) */
        v = C(1);
        for (i = 2; i + 1 <= nc; i += 2) v += C(i) * ALWAN_POW_F64(lu, C(i + 1));
        return v * v;
    case 6:   /* gases: n - 1 = C1 + sum C(2i) / (C(2i+1) - l^-2) */
        v = C(1);
        for (i = 2; i + 1 <= nc; i += 2) v += C(i) / (C(i + 1) - 1.0 / l2);
        return (v + 1.0) * (v + 1.0);
    case 7: { /* Herzberger: n = C1 + C2 / (l^2 - 0.028) + C3 (1 / (l^2 - 0.028))^2 + C4 l^2 + C5 l^4 + C6 l^6 */
        double const q = 1.0 / (l2 - 0.028);
        v = C(1) + C(2) * q + C(3) * q * q + C(4) * l2 + C(5) * l2 * l2 + C(6) * l2 * l2 * l2;
        return v * v;
    }
    case 8: { /* retro: (n^2 - 1) / (n^2 + 2) = C1 + C2 l^2 / (l^2 - C3) + C4 l^2 */
        double const a = C(1) + C(2) * l2 / (l2 - C(3)) + C(4) * l2;
        return (1.0 + 2.0 * a) / (1.0 - a);
    }
    case 9:   /* exotic: n^2 = C1 + C2 / (l^2 - C3) + C4 (l - C5) / ((l - C5)^2 + C6) */
        return C(1) + C(2) / (l2 - C(3)) + C(4) * (lu - C(5)) / ((lu - C(5)) * (lu - C(5)) + C(6));
    default:
        return -1.0;
    }
#undef C
}

static alwan_status alwan__rx_formula_n(double *out, alwan__rx_series const *s, double lu) {
    double const n2 = alwan__rx_formula(s, lu);
    if (!(n2 >= 0.0)) return ALWAN_E_RANGE;
    *out = ALWAN_SQRT_F64(n2);
    return ALWAN_OK;
}

static alwan_status alwan__rx_eval(double *out, alwan__rx_series const *s, double lu, alwan_interp_method m, alwan_extrapolate_mode ex) {
    double edge, inner, at_edge, at_inner, v, h;
    alwan_status st;
    if (!s->formula) {
        alwan__rx_tab tab;
        tab.lf = alwan__rx_lambda + s->lambda_offset, tab.vf = alwan__rx_values + s->value_offset;
        tab.ld = tab.vd = NULL, tab.count = (size_t)s->count;
        return alwan__rx_tab_eval(out, &tab, lu, m, ex);
    }
    if (lu >= s->lo_um && lu <= s->hi_um) return alwan__rx_formula_n(out, s, lu);
    switch (ex) {
    case ALWAN_EXTRAPOLATE_ZERO: *out = 0.0; return ALWAN_OK;
    case ALWAN_EXTRAPOLATE_CONSTANT: return alwan__rx_formula_n(out, s, lu < s->lo_um ? s->lo_um : s->hi_um);
    case ALWAN_EXTRAPOLATE_LINEAR:
    case ALWAN_EXTRAPOLATE_LINEAR_CLAMP_ZERO: break;
    default: return ALWAN_E_INVALID;
    }
    /* the slope over the last 1e-4 of the formula's range */
    h = (s->hi_um - s->lo_um) * 1e-4;
    edge = lu < s->lo_um ? s->lo_um : s->hi_um;
    inner = lu < s->lo_um ? s->lo_um + h : s->hi_um - h;
    st = alwan__rx_formula_n(&at_edge, s, edge);
    if (st != ALWAN_OK) return st;
    st = alwan__rx_formula_n(&at_inner, s, inner);
    if (st != ALWAN_OK) return st;
    v = at_edge + (at_edge - at_inner) / (edge - inner) * (lu - edge);
    if (ex == ALWAN_EXTRAPOLATE_LINEAR_CLAMP_ZERO && v < 0.0) v = 0.0;
    *out = v;
    return ALWAN_OK;
}

static alwan_status alwan__rx_sample(double *n, double *k, size_t index, double wavelength_nm, alwan_interp_method m, alwan_extrapolate_mode ex) {
    alwan__rx_page const *p;
    double const lu = wavelength_nm * 1e-3;
    alwan_status st;
    if (!alwan__rx_interp_ok(m)) return ALWAN_E_INVALID;
    if (index >= alwan__rx_page_count) return ALWAN_E_RANGE;
    if (!(wavelength_nm > 0.0) || wavelength_nm - wavelength_nm != 0.0) return ALWAN_E_RANGE;
    p = alwan__rx_pages + index;
    if (p->n_series >= 0) {
        st = alwan__rx_eval(n, alwan__rx_series_table + p->n_series, lu, m, ex);
        if (st != ALWAN_OK) return st;
    } else {
        *n = alwan__rx_nan();
    }
    if (p->k_series >= 0) {
        st = alwan__rx_eval(k, alwan__rx_series_table + p->k_series, lu, m, ex);
        if (st != ALWAN_OK) return st;
    } else {
        *k = 0.0;
    }
    return ALWAN_OK;
}

static void alwan__rx_range(double *lo_nm, double *hi_nm, int series) {
    if (series < 0) { *lo_nm = *hi_nm = 0.0; return; }
    {
        alwan__rx_series const *s = alwan__rx_series_table + series;
        double const lo = s->formula ? s->lo_um : (double)alwan__rx_lambda[s->lambda_offset];
        double const hi = s->formula ? s->hi_um : (double)alwan__rx_lambda[s->lambda_offset + s->count - 1];
        *lo_nm = lo * 1e3, *hi_nm = hi * 1e3;
    }
}

size_t alwan_refractive_index_count(void) { return alwan__rx_page_count; }

alwan_status alwan_refractive_index_find(size_t *index_out, char const *id) {
    size_t i, slashes = 0;
    char const *c;
    if (!index_out || !id || !*id) return ALWAN_E_INVALID;
    for (c = id; *c; c++) slashes += (*c == '/');
    if (slashes == 2) {
        for (i = 0; i < alwan__rx_page_count; i++)
            if (!strcmp(alwan__rx_pages[i].id, id)) { *index_out = i; return ALWAN_OK; }
        return ALWAN_E_RANGE;
    }
    if (slashes == 1) {
        size_t const sl = (size_t)(strchr(id, '/') - id);
        for (i = 0; i < alwan__rx_page_count; i++) {
            alwan__rx_page const *p = alwan__rx_pages + i;
            if (strlen(p->shelf) == sl && !strncmp(p->shelf, id, sl) && !strcmp(p->book, id + sl + 1)) {
                *index_out = i;
                return ALWAN_OK;
            }
        }
        return ALWAN_E_RANGE;
    }
    if (slashes == 0) {
        size_t first_other = (size_t)-1;
        for (i = 0; i < alwan__rx_page_count; i++) {
            alwan__rx_page const *p = alwan__rx_pages + i;
            if (strcmp(p->book, id)) continue;
            if (!strcmp(p->shelf, "main")) { *index_out = i; return ALWAN_OK; }
            if (first_other == (size_t)-1) first_other = i;
        }
        if (first_other != (size_t)-1) { *index_out = first_other; return ALWAN_OK; }
        return ALWAN_E_RANGE;
    }
    return ALWAN_E_RANGE;
}

alwan_status alwan_refractive_index_get_info(alwan_refractive_index_info *out, size_t index) {
    alwan__rx_page const *p;
    if (!out) return ALWAN_E_INVALID;
    if (index >= alwan__rx_page_count) return ALWAN_E_RANGE;
    p = alwan__rx_pages + index;
    memset(out, 0, sizeof(*out));
    out->id = p->id, out->shelf = p->shelf, out->book = p->book, out->page = p->page;
    out->material = p->material, out->name = p->name, out->reference = p->reference;
    out->has_n = p->n_series >= 0, out->has_k = p->k_series >= 0;
    out->n_formula = p->n_series >= 0 ? alwan__rx_series_table[p->n_series].formula : 0;
    alwan__rx_range(&out->n_min_nm, &out->n_max_nm, p->n_series);
    alwan__rx_range(&out->k_min_nm, &out->k_max_nm, p->k_series);
    return ALWAN_OK;
}

#else /* ALWAN_WITH_REFRACTIVE_DATA */

static alwan_status alwan__rx_sample(double *n, double *k, size_t index, double wavelength_nm, alwan_interp_method m, alwan_extrapolate_mode ex) {
    ALWAN_UNUSED(n); ALWAN_UNUSED(k); ALWAN_UNUSED(index); ALWAN_UNUSED(wavelength_nm); ALWAN_UNUSED(m); ALWAN_UNUSED(ex);
    return ALWAN_E_NODATA;
}

size_t alwan_refractive_index_count(void) { return 0; }

alwan_status alwan_refractive_index_find(size_t *index_out, char const *id) {
    if (!index_out || !id || !*id) return ALWAN_E_INVALID;
    return ALWAN_E_NODATA;
}

alwan_status alwan_refractive_index_get_info(alwan_refractive_index_info *out, size_t index) {
    ALWAN_UNUSED(index);
    if (!out) return ALWAN_E_INVALID;
    return ALWAN_E_NODATA;
}

#endif /* ALWAN_WITH_REFRACTIVE_DATA */

/* A medium's index at a wavelength: the caller's table, vacuum, or a page, edge values held. */
static alwan_status alwan__rx_medium(double *n, double *k, size_t material, alwan_refractive_table const *table, double wavelength_nm,
                                     alwan_interp_method m) {
    alwan_status st;
    if (!alwan__rx_interp_ok(m)) return ALWAN_E_INVALID;
    if (table) return alwan__rx_user(n, k, table, wavelength_nm, m);
    if (material == ALWAN_REFRACTIVE_VACUUM) { *n = 1.0, *k = 0.0; return ALWAN_OK; }
    st = alwan__rx_sample(n, k, material, wavelength_nm, m, ALWAN_EXTRAPOLATE_CONSTANT);
    if (st != ALWAN_OK) return st;
    if (*n - *n != 0.0) return ALWAN_E_RANGE;   /* a page with no n cannot be a medium */
    return ALWAN_OK;
}

alwan_status alwan_refractive_index_sample_f64(double *n_out, double *k_out, size_t index, double wavelength_nm, alwan_interp_method interpolation,
                                               alwan_extrapolate_mode extrapolate) {
    double k;
    alwan_status st;
    if (!n_out) return ALWAN_E_INVALID;
    st = alwan__rx_sample(n_out, &k, index, wavelength_nm, interpolation, extrapolate);
    if (st == ALWAN_OK && k_out) *k_out = k;
    return st;
}

alwan_status alwan_refractive_index_spectrum_f64(double *n_out, double *k_out, double const *wavelengths_nm, size_t count, size_t index,
                                                 alwan_interp_method interpolation, alwan_extrapolate_mode extrapolate) {
    size_t i;
    if (!n_out || !wavelengths_nm || count == 0) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        double n, k;
        alwan_status const st = alwan__rx_sample(&n, &k, index, wavelengths_nm[i], interpolation, extrapolate);
        if (st != ALWAN_OK) return st;
        n_out[i] = n;
        if (k_out) k_out[i] = k;
    }
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- *
 * Stacks and slabs
 * ---------------------------------------------------------------- */

static int alwan__rx_pol_ok(alwan_polarization p) {
    return p == ALWAN_POLARIZATION_UNPOLARIZED || p == ALWAN_POLARIZATION_S || p == ALWAN_POLARIZATION_P;
}

static double alwan__rx_pick(double s, double p, alwan_polarization pol) {
    return pol == ALWAN_POLARIZATION_S ? s : pol == ALWAN_POLARIZATION_P ? p : 0.5 * (s + p);
}

alwan_status alwan_refractive_stack_f64(double *R_out, double *T_out, double const *wavelengths_nm, size_t count,
                                        alwan_refractive_stack const *stack, alwan_ctx *ctx) {
    size_t media, m, w;
    double *buf, *n, *k, *thick, *R2, *T2;
    alwan_multilayer ml;
    alwan_status st = ALWAN_OK;
    ALWAN_UNUSED(ctx);
    if (!R_out || !T_out || !wavelengths_nm || !stack || count == 0) return ALWAN_E_INVALID;
    if (stack->layer_count && !stack->layers) return ALWAN_E_INVALID;
    if (!alwan__rx_pol_ok(stack->polarization) || !alwan__rx_interp_ok(stack->interpolation)) return ALWAN_E_INVALID;
    if (stack->layer_count > 4000) return ALWAN_E_RANGE;
    media = stack->layer_count + 2;
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(2 * media * count + media + 4 * count, sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    n = buf, k = n + media * count, thick = k + media * count, R2 = thick + media, T2 = R2 + 2 * count;
    for (m = 0; m < media && st == ALWAN_OK; m++) {
        size_t mat;
        alwan_refractive_table const *tab;
        if (m == 0) {
            mat = stack->ambient, tab = stack->ambient_table;
        } else if (m + 1 == media) {
            mat = stack->substrate, tab = stack->substrate_table;
        } else {
            mat = stack->layers[m - 1].material, tab = stack->layers[m - 1].table;
            thick[m - 1] = stack->layers[m - 1].thickness_nm;
        }
        for (w = 0; w < count && st == ALWAN_OK; w++)
            st = alwan__rx_medium(&n[m * count + w], &k[m * count + w], mat, tab, wavelengths_nm[w], stack->interpolation);
    }
    if (st == ALWAN_OK) {
        ml.n = n, ml.k = k, ml.row_stride = count, ml.media_count = media, ml.thickness = thick;
        st = alwan_multilayer_tmm_f64(R2, T2, wavelengths_nm, count, &ml, stack->theta_degrees);
    }
    if (st == ALWAN_OK)
        for (w = 0; w < count; w++) {
            R_out[w] = alwan__rx_pick(R2[2 * w], R2[2 * w + 1], stack->polarization);
            T_out[w] = alwan__rx_pick(T2[2 * w], T2[2 * w + 1], stack->polarization);
        }
    ALWAN_FREE(buf);
    return st;
}

/* complex square root on the branch with a non-negative imaginary part */
static void alwan__rx_csqrt(double *re, double *im, double a, double b) {
    double const r = ALWAN_SQRT_F64(a * a + b * b);
    double x = ALWAN_SQRT_F64(0.5 * (r + a)), y = ALWAN_SQRT_F64(0.5 * (r - a));
    if (b < 0.0) y = -y;
    if (y < 0.0) x = -x, y = -y;
    *re = x, *im = y;
}

alwan_status alwan_refractive_slab_f64(double *R_out, double *T_out, double const *wavelengths_nm, size_t count, size_t material,
                                       alwan_refractive_table const *table, double thickness_nm, size_t ambient, double theta_degrees,
                                       alwan_polarization polarization, alwan_interp_method interpolation) {
    size_t w;
    if (!R_out || !T_out || !wavelengths_nm || count == 0) return ALWAN_E_INVALID;
    if (!alwan__rx_pol_ok(polarization) || !alwan__rx_interp_ok(interpolation)) return ALWAN_E_INVALID;
    if (!(thickness_nm >= 0.0) || thickness_nm - thickness_nm != 0.0) return ALWAN_E_RANGE;
    if (!(theta_degrees >= 0.0 && theta_degrees < 90.0)) return ALWAN_E_RANGE;
    for (w = 0; w < count; w++) {
        double const lambda = wavelengths_nm[w];
        double n0, k0, n1, k1, amp[8], s, ncos_re, ncos_im, A, rr[2], tt[2];
        int pol;
        alwan_status st = alwan__rx_medium(&n0, &k0, ambient, NULL, lambda, interpolation);
        if (st != ALWAN_OK) return st;
        st = alwan__rx_medium(&n1, &k1, material, table, lambda, interpolation);
        if (st != ALWAN_OK) return st;
        if (k0 != 0.0) return ALWAN_E_RANGE;   /* the ambient is lossless */
        st = alwan_fresnel_f64(amp, n0, 0.0, n1, k1, theta_degrees);
        if (st != ALWAN_OK) return st;
        /* n1 cos(theta1) = sqrt(n1^2 - n0^2 sin^2 theta0), the forward branch */
        s = n0 * ALWAN_SIN_F64(theta_degrees * (3.14159265358979323846 / 180.0));
        alwan__rx_csqrt(&ncos_re, &ncos_im, n1 * n1 - k1 * k1 - s * s, 2.0 * n1 * k1);
        A = ALWAN_EXP_F64(-4.0 * 3.14159265358979323846 * ncos_im * thickness_nm / lambda);
        for (pol = 0; pol < 2; pol++) {
            double const R1 = amp[2 * pol] * amp[2 * pol] + amp[2 * pol + 1] * amp[2 * pol + 1];
            double const den = 1.0 - R1 * R1 * A * A;
            rr[pol] = R1 + (1.0 - R1) * (1.0 - R1) * R1 * A * A / den;
            tt[pol] = (1.0 - R1) * (1.0 - R1) * A / den;
        }
        R_out[w] = alwan__rx_pick(rr[0], rr[1], polarization);
        T_out[w] = alwan__rx_pick(tt[0], tt[1], polarization);
    }
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- *
 * Colour
 * ---------------------------------------------------------------- */

alwan_status alwan_reflectance_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_xyz_f64 *xyz_out, alwan_spd_f64 const *spectrum, alwan_interp_method interpolation,
                                          alwan_rgb_space space, alwan_illuminant illuminant, alwan_observer_type observer, alwan_ctx *ctx) {
    alwan_spd_f64 illum, diffuser, fine;
    alwan_spd_f64 const *use = spectrum;
    alwan_xyz_f64 white = {0, 0, 0}, xyz = {0, 0, 0}, dst_white, adapted;
    alwan_rgb_space_desc_f64 desc;
    alwan_mat3x3_f64 cat;
    alwan_status st;
    size_t i;
    if (!rgb_out || !spectrum || !spectrum->values || spectrum->count < 2) return ALWAN_E_INVALID;
    if (!alwan__rx_interp_ok(interpolation)) return ALWAN_E_INVALID;
    memset(&illum, 0, sizeof(illum));
    memset(&diffuser, 0, sizeof(diffuser));
    memset(&fine, 0, sizeof(fine));
    st = alwan_rgb_get_space_descriptor_f64(&desc, space, ctx);
    if (st != ALWAN_OK) return st;
    if (interpolation != ALWAN_INTERP_LINEAR) {
        /* the spectrum at steps of at most 1 nm over its range, read with the caller's method */
        double const span = spectrum->wavelength_max - spectrum->wavelength_min;
        size_t nf = 1;
        double reach = 0.0;
        double *xin;
        if (!(span > 0.0 && span < 1e6)) return ALWAN_E_RANGE;
        while (reach < span) reach += 1.0, nf++;   /* steps of 1 nm or just under, end to end */
        xin = (double *)ALWAN_ALLOC(alwan_safe_array_size(spectrum->count + nf, sizeof(double)), sizeof(double));
        if (!xin) return ALWAN_E_NOMEM;
        st = alwan_spd_create_f64(&fine, spectrum->wavelength_min, spectrum->wavelength_max, nf, ctx);
        if (st == ALWAN_OK) {
            double *xout = xin + spectrum->count;
            for (i = 0; i < spectrum->count; i++)
                xin[i] = spectrum->wavelength_min + span * (double)i / (double)(spectrum->count - 1);
            for (i = 0; i < nf; i++) xout[i] = spectrum->wavelength_min + span * (double)i / (double)(nf - 1);
            st = alwan_interpolate_f64(xin, spectrum->values, spectrum->count, xout, fine.values, nf, interpolation);
            if (st != ALWAN_OK) st = ALWAN_E_INVALID;
        }
        ALWAN_FREE(xin);
        if (st != ALWAN_OK) { alwan_spd_destroy_f64(&fine, ctx); return st; }
        use = &fine;
    }
    st = alwan_spd_illuminant_f64(&illum, illuminant, ctx);
    if (st == ALWAN_OK) st = alwan_spd_create_f64(&diffuser, use->wavelength_min, use->wavelength_max, use->count, ctx);
    if (st == ALWAN_OK) {
        for (i = 0; i < diffuser.count; i++) diffuser.values[i] = 1.0;
        st = alwan_xyz_from_spd_f64(&white, &diffuser, &illum, observer, ALWAN_INTEGRATE_TRAPEZOID, 0.0, ctx);
    }
    if (st == ALWAN_OK) st = alwan_xyz_from_spd_f64(&xyz, use, &illum, observer, ALWAN_INTEGRATE_TRAPEZOID, 0.0, ctx);
    alwan_spd_destroy_f64(&diffuser, ctx);
    alwan_spd_destroy_f64(&illum, ctx);
    alwan_spd_destroy_f64(&fine, ctx);
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

#define ALWAN__RX_COLOUR_MIN 360.0
#define ALWAN__RX_COLOUR_COUNT 471   /* 360-830 nm at 1 nm */

static alwan_status alwan__rx_colours(alwan_rgb_f64 *reflect_rgb, alwan_rgb_f64 *transmit_rgb, double const *R, double const *T,
                                      alwan_rgb_space space, alwan_illuminant illuminant, alwan_observer_type observer, alwan_ctx *ctx) {
    alwan_spd_f64 spd;
    alwan_status st = ALWAN_OK;
    spd.wavelength_min = ALWAN__RX_COLOUR_MIN;
    spd.wavelength_max = ALWAN__RX_COLOUR_MIN + (ALWAN__RX_COLOUR_COUNT - 1);
    spd.count = ALWAN__RX_COLOUR_COUNT;
    /* already at 1 nm: integrated as sampled */
    if (reflect_rgb) {
        spd.values = (double *)R;
        st = alwan_reflectance_to_rgb_f64(reflect_rgb, NULL, &spd, ALWAN_INTERP_LINEAR, space, illuminant, observer, ctx);
    }
    if (st == ALWAN_OK && transmit_rgb) {
        spd.values = (double *)T;
        st = alwan_reflectance_to_rgb_f64(transmit_rgb, NULL, &spd, ALWAN_INTERP_LINEAR, space, illuminant, observer, ctx);
    }
    return st;
}

alwan_status alwan_refractive_stack_rgb_f64(alwan_rgb_f64 *reflect_rgb, alwan_rgb_f64 *transmit_rgb, alwan_refractive_stack const *stack,
                                            alwan_rgb_space space, alwan_illuminant illuminant, alwan_observer_type observer, alwan_ctx *ctx) {
    double wl[ALWAN__RX_COLOUR_COUNT], R[ALWAN__RX_COLOUR_COUNT], T[ALWAN__RX_COLOUR_COUNT];
    alwan_status st;
    int i;
    if ((!reflect_rgb && !transmit_rgb) || !stack) return ALWAN_E_INVALID;
    for (i = 0; i < ALWAN__RX_COLOUR_COUNT; i++) wl[i] = ALWAN__RX_COLOUR_MIN + i;
    st = alwan_refractive_stack_f64(R, T, wl, ALWAN__RX_COLOUR_COUNT, stack, ctx);
    if (st != ALWAN_OK) return st;
    return alwan__rx_colours(reflect_rgb, transmit_rgb, R, T, space, illuminant, observer, ctx);
}

alwan_status alwan_refractive_slab_rgb_f64(alwan_rgb_f64 *reflect_rgb, alwan_rgb_f64 *transmit_rgb, size_t material, alwan_refractive_table const *table,
                                           double thickness_nm, alwan_interp_method interpolation, alwan_rgb_space space, alwan_illuminant illuminant,
                                           alwan_observer_type observer, alwan_ctx *ctx) {
    double wl[ALWAN__RX_COLOUR_COUNT], R[ALWAN__RX_COLOUR_COUNT], T[ALWAN__RX_COLOUR_COUNT];
    alwan_status st;
    int i;
    if (!reflect_rgb && !transmit_rgb) return ALWAN_E_INVALID;
    for (i = 0; i < ALWAN__RX_COLOUR_COUNT; i++) wl[i] = ALWAN__RX_COLOUR_MIN + i;
    st = alwan_refractive_slab_f64(R, T, wl, ALWAN__RX_COLOUR_COUNT, material, table, thickness_nm, ALWAN_REFRACTIVE_VACUUM, 0.0,
                                   ALWAN_POLARIZATION_UNPOLARIZED, interpolation);
    if (st != ALWAN_OK) return st;
    return alwan__rx_colours(reflect_rgb, transmit_rgb, R, T, space, illuminant, observer, ctx);
}

/* ---------------------------------------------------------------- *
 * f32: each widens to the f64 body and narrows the result
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F32
alwan_status alwan_refractive_index_sample_f32(float *n_out, float *k_out, size_t index, float wavelength_nm, alwan_interp_method interpolation,
                                               alwan_extrapolate_mode extrapolate) {
    double n, k;
    alwan_status st;
    if (!n_out) return ALWAN_E_INVALID;
    st = alwan_refractive_index_sample_f64(&n, &k, index, (double)wavelength_nm, interpolation, extrapolate);
    if (st == ALWAN_OK) {
        *n_out = (float)n;
        if (k_out) *k_out = (float)k;
    }
    return st;
}

alwan_status alwan_refractive_index_spectrum_f32(float *n_out, float *k_out, float const *wavelengths_nm, size_t count, size_t index,
                                                 alwan_interp_method interpolation, alwan_extrapolate_mode extrapolate) {
    size_t i;
    if (!n_out || !wavelengths_nm || count == 0) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        float n, k;
        alwan_status const st = alwan_refractive_index_sample_f32(&n, &k, index, wavelengths_nm[i], interpolation, extrapolate);
        if (st != ALWAN_OK) return st;
        n_out[i] = n;
        if (k_out) k_out[i] = k;
    }
    return ALWAN_OK;
}

static alwan_status alwan__rx_widen_run(float *R_out, float *T_out, float const *wavelengths_nm, size_t count, double **buf_out) {
    size_t i;
    double *buf;
    if (!R_out || !T_out || !wavelengths_nm || count == 0) return ALWAN_E_INVALID;
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(3 * count, sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    for (i = 0; i < count; i++) buf[2 * count + i] = (double)wavelengths_nm[i];
    *buf_out = buf;
    return ALWAN_OK;
}

alwan_status alwan_refractive_stack_f32(float *R_out, float *T_out, float const *wavelengths_nm, size_t count, alwan_refractive_stack const *stack, alwan_ctx *ctx) {
    double *buf = NULL;
    size_t i;
    alwan_status st = alwan__rx_widen_run(R_out, T_out, wavelengths_nm, count, &buf);
    if (st != ALWAN_OK) return st;
    st = alwan_refractive_stack_f64(buf, buf + count, buf + 2 * count, count, stack, ctx);
    if (st == ALWAN_OK)
        for (i = 0; i < count; i++) R_out[i] = (float)buf[i], T_out[i] = (float)buf[count + i];
    ALWAN_FREE(buf);
    return st;
}

alwan_status alwan_refractive_slab_f32(float *R_out, float *T_out, float const *wavelengths_nm, size_t count, size_t material,
                                       alwan_refractive_table const *table, float thickness_nm, size_t ambient, float theta_degrees,
                                       alwan_polarization polarization, alwan_interp_method interpolation) {
    double *buf = NULL;
    size_t i;
    alwan_status st = alwan__rx_widen_run(R_out, T_out, wavelengths_nm, count, &buf);
    if (st != ALWAN_OK) return st;
    st = alwan_refractive_slab_f64(buf, buf + count, buf + 2 * count, count, material, table, (double)thickness_nm, ambient,
                                   (double)theta_degrees, polarization, interpolation);
    if (st == ALWAN_OK)
        for (i = 0; i < count; i++) R_out[i] = (float)buf[i], T_out[i] = (float)buf[count + i];
    ALWAN_FREE(buf);
    return st;
}

static void alwan__rx_narrow_rgb(alwan_rgb_f32 *o, alwan_rgb_f64 const *i) {
    o->r = (float)i->r, o->g = (float)i->g, o->b = (float)i->b;
}

alwan_status alwan_reflectance_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_xyz_f32 *xyz_out, alwan_spd_f32 const *spectrum, alwan_interp_method interpolation,
                                          alwan_rgb_space space, alwan_illuminant illuminant, alwan_observer_type observer, alwan_ctx *ctx) {
    alwan_spd_f64 s;
    alwan_rgb_f64 rgb;
    alwan_xyz_f64 xyz;
    alwan_status st;
    size_t i;
    if (!rgb_out || !spectrum || !spectrum->values || spectrum->count < 2) return ALWAN_E_INVALID;
    s.values = (double *)ALWAN_ALLOC(alwan_safe_array_size(spectrum->count, sizeof(double)), sizeof(double));
    if (!s.values) return ALWAN_E_NOMEM;
    for (i = 0; i < spectrum->count; i++) s.values[i] = (double)spectrum->values[i];
    s.wavelength_min = (double)spectrum->wavelength_min, s.wavelength_max = (double)spectrum->wavelength_max, s.count = spectrum->count;
    st = alwan_reflectance_to_rgb_f64(&rgb, &xyz, &s, interpolation, space, illuminant, observer, ctx);
    ALWAN_FREE(s.values);
    if (st != ALWAN_OK) return st;
    alwan__rx_narrow_rgb(rgb_out, &rgb);
    if (xyz_out) xyz_out->x = (float)xyz.x, xyz_out->y = (float)xyz.y, xyz_out->z = (float)xyz.z;
    return ALWAN_OK;
}

alwan_status alwan_refractive_stack_rgb_f32(alwan_rgb_f32 *reflect_rgb, alwan_rgb_f32 *transmit_rgb, alwan_refractive_stack const *stack,
                                            alwan_rgb_space space, alwan_illuminant illuminant, alwan_observer_type observer, alwan_ctx *ctx) {
    alwan_rgb_f64 r, t;
    alwan_status const st = alwan_refractive_stack_rgb_f64(reflect_rgb ? &r : NULL, transmit_rgb ? &t : NULL, stack, space, illuminant,
                                                           observer, ctx);
    if (st != ALWAN_OK) return st;
    if (reflect_rgb) alwan__rx_narrow_rgb(reflect_rgb, &r);
    if (transmit_rgb) alwan__rx_narrow_rgb(transmit_rgb, &t);
    return ALWAN_OK;
}

alwan_status alwan_refractive_slab_rgb_f32(alwan_rgb_f32 *reflect_rgb, alwan_rgb_f32 *transmit_rgb, size_t material, alwan_refractive_table const *table,
                                           float thickness_nm, alwan_interp_method interpolation, alwan_rgb_space space, alwan_illuminant illuminant,
                                           alwan_observer_type observer, alwan_ctx *ctx) {
    alwan_rgb_f64 r, t;
    alwan_status const st = alwan_refractive_slab_rgb_f64(reflect_rgb ? &r : NULL, transmit_rgb ? &t : NULL, material, table, (double)thickness_nm,
                                                          interpolation, space, illuminant, observer, ctx);
    if (st != ALWAN_OK) return st;
    if (reflect_rgb) alwan__rx_narrow_rgb(reflect_rgb, &r);
    if (transmit_rgb) alwan__rx_narrow_rgb(transmit_rgb, &t);
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */
