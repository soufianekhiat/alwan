/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Spectral rendering: wavelength samplers (uniform, the visible-importance density of
 * Radziszewski, Boryczko and Alda 2009, tabulated weights through the 2D sampler), hero
 * wavelength rotation with balance-heuristic weights (Wilkie et al. 2014), CMF-based
 * weights for a tabulated sampler, and a spectral film that sums radiance samples into
 * XYZ. The per-sample math of the closed-form densities is in
 * core/alwan_wavelength_sampling_core.inc. Suite 287.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>
#include "../core/alwan_wavelength_sampling_core.h"

#if ALWAN_WITH_F32
#include "alwan_api_f32_setup.h"
#include "alwan_wavelength_sampling_impl.inc"
#include "alwan_api_teardown.h"
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_wavelength_sampling_impl.inc"
#include "alwan_api_teardown.h"
#endif

/* ----------------------------------------------------------------
 * The CMF table, 360 to 830 nm at 1 nm, read between its samples
 * ---------------------------------------------------------------- */

#define ALWAN__WL_CMF_MIN 360.0
#define ALWAN__WL_CMF_N   471   /* 360-830 at 1 nm */
#define ALWAN__WL_WINDOW  8     /* samples each side for a method other than LINEAR */

typedef struct {
    double v[3][ALWAN__WL_CMF_N];
    double grid[ALWAN__WL_CMF_N];
    alwan_interp_method interp;
} alwan__wl_cmf;

static alwan_status alwan__wl_cmf_load(alwan__wl_cmf *c, alwan_observer_type observer, alwan_interp_method interp,
                                       alwan_ctx *ctx) {
    alwan_spd_f64 xb, yb, zb;
    alwan_status st;
    int i;
    memset(&xb, 0, sizeof(xb));
    memset(&yb, 0, sizeof(yb));
    memset(&zb, 0, sizeof(zb));
    st = alwan_spd_observer_f64(&xb, &yb, &zb, observer, ctx);
    if (st != ALWAN_OK) return st;
    if (xb.count != ALWAN__WL_CMF_N || yb.count != ALWAN__WL_CMF_N || zb.count != ALWAN__WL_CMF_N) {
        st = ALWAN_E_INVALID;
    } else {
        for (i = 0; i < ALWAN__WL_CMF_N; i++) {
            c->v[0][i] = xb.values[i];
            c->v[1][i] = yb.values[i];
            c->v[2][i] = zb.values[i];
            c->grid[i] = ALWAN__WL_CMF_MIN + (double)i;
        }
        c->interp = interp;
    }
    alwan_spd_destroy_f64(&xb, ctx);
    alwan_spd_destroy_f64(&yb, ctx);
    alwan_spd_destroy_f64(&zb, ctx);
    return st;
}

/* The integer part of a non-negative position below n, counted rather than cast, so no
 * subscript comes from a float. */
static int alwan__wl_floor_index(double t, int n) {
    int lo = 0, hi = n - 1;
    if (!(t > 0.0)) return 0;
    if (t >= (double)(n - 1)) return n - 1;
    while (hi - lo > 1) {
        int const mid = lo + (hi - lo) / 2;
        if ((double)mid <= t) lo = mid; else hi = mid;
    }
    return lo;
}

/* The three CMFs at lambda; 0 outside 360 to 830 nm. */
static alwan_status alwan__wl_cmf_at(double out[3], alwan__wl_cmf const *c, double lambda) {
    double const t = lambda - ALWAN__WL_CMF_MIN;
    int i, k;
    if (!(t >= 0.0 && t <= (double)(ALWAN__WL_CMF_N - 1))) {
        out[0] = out[1] = out[2] = 0.0;
        return ALWAN_OK;
    }
    i = alwan__wl_floor_index(t, ALWAN__WL_CMF_N);
    if (c->interp == ALWAN_INTERP_LINEAR || i == ALWAN__WL_CMF_N - 1) {
        int const j = i < ALWAN__WL_CMF_N - 1 ? i + 1 : i;
        double const f = t - (double)i;
        for (k = 0; k < 3; k++) out[k] = c->v[k][i] + f * (c->v[k][j] - c->v[k][i]);
        return ALWAN_OK;
    }
    {
        int a = i - ALWAN__WL_WINDOW + 1, b = i + ALWAN__WL_WINDOW;
        if (a < 0) a = 0;
        if (b > ALWAN__WL_CMF_N - 1) b = ALWAN__WL_CMF_N - 1;
        for (k = 0; k < 3; k++) {
            double y = 0.0;
            alwan_status const st = alwan_interpolate_f64(c->grid + a, c->v[k] + a, (size_t)(b - a + 1), &lambda, &y, 1,
                                                          c->interp);
            if (st != ALWAN_OK) return ALWAN_E_INVALID;
            out[k] = y;
        }
    }
    return ALWAN_OK;
}

/* An SPD on its uniform grid read linearly at lambda; 0 outside it. */
static double alwan__wl_spd_at(alwan_spd_f64 const *spd, double lambda) {
    double const w0 = spd->wavelength_min, w1 = spd->wavelength_max;
    double t;
    int i;
    if (spd->count < 2 || !(lambda >= w0 && lambda <= w1)) return 0.0;
    t = (lambda - w0) / (w1 - w0) * (double)(spd->count - 1);
    i = alwan__wl_floor_index(t, (int)spd->count);
    if (i >= (int)spd->count - 1) return spd->values[spd->count - 1];
    return spd->values[i] + (t - (double)i) * (spd->values[i + 1] - spd->values[i]);
}

/* ----------------------------------------------------------------
 * Weights for a tabulated sampler
 * ---------------------------------------------------------------- */

static alwan_status alwan__wl_weights(double *w, size_t count, double lo, double hi, alwan_wavelength_weight kind,
                                      alwan_observer_type observer, alwan_illuminant illuminant, alwan_ctx *ctx) {
    alwan__wl_cmf *c;
    alwan_spd_f64 illum;
    alwan_status st;
    size_t i;
    int k;
    if (!w || count == 0 || !(hi > lo) || !(lo >= 0.0) || hi - hi != 0.0) return ALWAN_E_INVALID;
    if ((int)kind != (int)ALWAN_WAVELENGTH_WEIGHT_XYZ && (int)kind != (int)ALWAN_WAVELENGTH_WEIGHT_Y) return ALWAN_E_INVALID;
    c = (alwan__wl_cmf *)ALWAN_ALLOC(sizeof(*c), sizeof(double));
    if (!c) return ALWAN_E_NOMEM;
    memset(&illum, 0, sizeof(illum));
    st = alwan__wl_cmf_load(c, observer, ALWAN_INTERP_LINEAR, ctx);
    if (st == ALWAN_OK) st = alwan_spd_illuminant_f64(&illum, illuminant, ctx);
    if (st == ALWAN_OK) {
        double const width = (hi - lo) / (double)count;
        for (i = 0; i < count; i++) {
            double sum = 0.0;
            for (k = 0; k < 16; k++) {
                double const l = lo + width * ((double)i + ((double)k + 0.5) / 16.0);
                double cmf[3];
                double const e = alwan__wl_spd_at(&illum, l);
                alwan__wl_cmf_at(cmf, c, l);
                sum += e * (kind == ALWAN_WAVELENGTH_WEIGHT_Y ? cmf[1] : cmf[0] + cmf[1] + cmf[2]);
            }
            w[i] = sum / 16.0;
        }
    }
    alwan_spd_destroy_f64(&illum, ctx);
    ALWAN_FREE(c);
    return st;
}

alwan_status alwan_wavelength_weights_f64(alwan_f64 *weights_out, size_t count, alwan_f64 lambda_min, alwan_f64 lambda_max,
                                          alwan_wavelength_weight weight, alwan_observer_type observer,
                                          alwan_illuminant illuminant, alwan_ctx *ctx) {
    return alwan__wl_weights(weights_out, count, lambda_min, lambda_max, weight, observer, illuminant, ctx);
}

alwan_status alwan_wavelength_weights_f32(alwan_f32 *weights_out, size_t count, alwan_f32 lambda_min, alwan_f32 lambda_max,
                                          alwan_wavelength_weight weight, alwan_observer_type observer,
                                          alwan_illuminant illuminant, alwan_ctx *ctx) {
    double *w;
    alwan_status st;
    size_t i;
    if (!weights_out || count == 0) return ALWAN_E_INVALID;
    w = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(double)), sizeof(double));
    if (!w) return ALWAN_E_NOMEM;
    st = alwan__wl_weights(w, count, (double)lambda_min, (double)lambda_max, weight, observer, illuminant, ctx);
    if (st == ALWAN_OK) for (i = 0; i < count; i++) weights_out[i] = (alwan_f32)w[i];
    ALWAN_FREE(w);
    return st;
}

/* ----------------------------------------------------------------
 * The spectral film
 * ---------------------------------------------------------------- */

struct alwan_spectral_film_s {
    size_t w, h;
    double *acc;      /* 3 a pixel: the running sum, or Welford's running mean when tracking */
    double *m2;       /* 3 a pixel when tracking, else NULL */
    double *n;        /* paths a pixel (a double, which counts exactly to 2^53) */
    double scale;     /* applied at resolve: 1, or 1 / the illuminant's Y integral */
    double white[3];  /* the film's white, Y = 1, for adaptation */
    int track;
    alwan__wl_cmf cmf;
};

/* The integral of e(lambda) times the CMFs over 360 to 830 nm by the trapezoid at
 * 0.05 nm, on the film's own reading of the CMFs; e NULL for 1. */
static alwan_status alwan__wl_integral(double out[3], alwan__wl_cmf const *c, alwan_spd_f64 const *e) {
    int const steps = 9400;   /* 470 nm / 0.05 nm */
    double const h = (830.0 - ALWAN__WL_CMF_MIN) / (double)steps;
    int i, k;
    out[0] = out[1] = out[2] = 0.0;
    for (i = 0; i <= steps; i++) {
        double const l = ALWAN__WL_CMF_MIN + h * (double)i;
        double cmf[3];
        double const v = e ? alwan__wl_spd_at(e, l) : 1.0;
        double const wt = (i == 0 || i == steps) ? 0.5 : 1.0;
        alwan_status const st = alwan__wl_cmf_at(cmf, c, l);
        if (st != ALWAN_OK) return st;
        for (k = 0; k < 3; k++) out[k] += wt * v * cmf[k];
    }
    for (k = 0; k < 3; k++) out[k] *= h;
    return ALWAN_OK;
}

alwan_status alwan_spectral_film_create(alwan_spectral_film **out, size_t width, size_t height,
                                        alwan_spectral_film_params const *params, alwan_ctx *ctx) {
    alwan_spectral_film_params p;
    alwan_spectral_film *f;
    alwan_status st;
    size_t px, bytes3, bytes1;
    double wint[3];
    if (!out) return ALWAN_E_INVALID;
    *out = NULL;
    if (width == 0 || height == 0) return ALWAN_E_INVALID;
    memset(&p, 0, sizeof(p));
    if (params) p = *params;
    if ((int)p.normalize != (int)ALWAN_SPECTRAL_FILM_NORMALIZE_NONE
        && (int)p.normalize != (int)ALWAN_SPECTRAL_FILM_NORMALIZE_ILLUMINANT) {
        return ALWAN_E_INVALID;
    }
    px = width * height;
    if (px / width != height) return ALWAN_E_RANGE;
    bytes3 = alwan_safe_array_size(px, 3 * sizeof(double));
    bytes1 = alwan_safe_array_size(px, sizeof(double));
    if (bytes3 == 0 || bytes1 == 0) return ALWAN_E_RANGE;
    f = (alwan_spectral_film *)ALWAN_ALLOC(sizeof(*f), sizeof(double));
    if (!f) return ALWAN_E_NOMEM;
    memset(f, 0, sizeof(*f));
    f->w = width;
    f->h = height;
    f->track = p.track_variance != 0;
    st = alwan__wl_cmf_load(&f->cmf, p.observer, p.interpolation, ctx);
    if (st == ALWAN_OK) {
        /* refuse an interpolation the CMF grid does not allow up front, not at the first sample */
        double probe[3];
        st = alwan__wl_cmf_at(probe, &f->cmf, 500.5);
    }
    if (st == ALWAN_OK) {
        if (p.normalize == ALWAN_SPECTRAL_FILM_NORMALIZE_ILLUMINANT) {
            alwan_spd_f64 illum;
            memset(&illum, 0, sizeof(illum));
            st = alwan_spd_illuminant_f64(&illum, p.illuminant, ctx);
            if (st == ALWAN_OK) st = alwan__wl_integral(wint, &f->cmf, &illum);
            alwan_spd_destroy_f64(&illum, ctx);
        } else {
            st = alwan__wl_integral(wint, &f->cmf, NULL);
        }
    }
    if (st == ALWAN_OK && !(wint[1] > 0.0)) st = ALWAN_E_DIVZERO;
    if (st == ALWAN_OK) {
        f->scale = p.normalize == ALWAN_SPECTRAL_FILM_NORMALIZE_ILLUMINANT ? 1.0 / wint[1] : 1.0;
        f->white[0] = wint[0] / wint[1];
        f->white[1] = 1.0;
        f->white[2] = wint[2] / wint[1];
        f->acc = (double *)ALWAN_ALLOC(bytes3, sizeof(double));
        f->n = (double *)ALWAN_ALLOC(bytes1, sizeof(double));
        if (f->track) f->m2 = (double *)ALWAN_ALLOC(bytes3, sizeof(double));
        if (!f->acc || !f->n || (f->track && !f->m2)) st = ALWAN_E_NOMEM;
    }
    if (st != ALWAN_OK) {
        alwan_spectral_film_destroy(f, ctx);
        return st;
    }
    alwan_spectral_film_clear(f);
    *out = f;
    return ALWAN_OK;
}

void alwan_spectral_film_destroy(alwan_spectral_film *film, alwan_ctx *ctx) {
    (void)ctx;
    if (!film) return;
    if (film->acc) ALWAN_FREE(film->acc);
    if (film->m2) ALWAN_FREE(film->m2);
    if (film->n) ALWAN_FREE(film->n);
    ALWAN_FREE(film);
}

alwan_status alwan_spectral_film_clear(alwan_spectral_film *film) {
    size_t const px = film ? film->w * film->h : 0;
    if (!film) return ALWAN_E_INVALID;
    memset(film->acc, 0, px * 3 * sizeof(double));
    memset(film->n, 0, px * sizeof(double));
    if (film->m2) memset(film->m2, 0, px * 3 * sizeof(double));
    return ALWAN_OK;
}

static int alwan__wl_finite(double v) {
    return v == v && v - v == 0.0;
}

/* One path's contribution into pixel index i; nothing is added if any value is not
 * finite. */
static alwan_status alwan__wl_add(alwan_spectral_film *f, size_t i, double const *lambda, double const *radiance,
                                  double const *weight, size_t count) {
    double c[3] = {0.0, 0.0, 0.0};
    size_t j;
    int k;
    for (j = 0; j < count; j++) {
        double cmf[3];
        double const wt = weight ? weight[j] : 1.0;
        alwan_status st;
        if (!alwan__wl_finite(lambda[j]) || !alwan__wl_finite(radiance[j]) || !alwan__wl_finite(wt)) return ALWAN_E_INVALID;
        st = alwan__wl_cmf_at(cmf, &f->cmf, lambda[j]);
        if (st != ALWAN_OK) return st;
        for (k = 0; k < 3; k++) c[k] += radiance[j] * wt * cmf[k];
    }
    f->n[i] += 1.0;
    if (f->track) {
        /* Welford: mean += d / n, m2 += d (x - mean) */
        for (k = 0; k < 3; k++) {
            double const d = c[k] - f->acc[3 * i + k];
            f->acc[3 * i + k] += d / f->n[i];
            f->m2[3 * i + k] += d * (c[k] - f->acc[3 * i + k]);
        }
    } else {
        for (k = 0; k < 3; k++) f->acc[3 * i + k] += c[k];
    }
    return ALWAN_OK;
}

alwan_status alwan_spectral_film_add_f64(alwan_spectral_film *film, size_t x, size_t y, alwan_f64 const *lambda,
                                         alwan_f64 const *radiance, alwan_f64 const *weight, size_t count) {
    if (!film || !lambda || !radiance) return ALWAN_E_INVALID;
    if (x >= film->w || y >= film->h) return ALWAN_E_RANGE;
    return alwan__wl_add(film, y * film->w + x, lambda, radiance, weight, count);
}

/* An f32 path widened in blocks on the stack. */
static alwan_status alwan__wl_add_f32(alwan_spectral_film *f, size_t i, alwan_f32 const *lambda, alwan_f32 const *radiance,
                                      alwan_f32 const *weight, size_t count) {
    enum { BLOCK = 64 };
    double l[BLOCK], r[BLOCK], w[BLOCK];
    double c[3] = {0.0, 0.0, 0.0};
    size_t j, done = 0;
    int k;
    /* the contribution is summed over the whole path before it reaches the pixel, so the
     * blocks only widen; the pixel's statistics see one path */
    while (done < count) {
        size_t const m = count - done < BLOCK ? count - done : BLOCK;
        for (j = 0; j < m; j++) {
            double cmf[3];
            alwan_status st;
            l[j] = (double)lambda[done + j];
            r[j] = (double)radiance[done + j];
            w[j] = weight ? (double)weight[done + j] : 1.0;
            if (!alwan__wl_finite(l[j]) || !alwan__wl_finite(r[j]) || !alwan__wl_finite(w[j])) return ALWAN_E_INVALID;
            st = alwan__wl_cmf_at(cmf, &f->cmf, l[j]);
            if (st != ALWAN_OK) return st;
            for (k = 0; k < 3; k++) c[k] += r[j] * w[j] * cmf[k];
        }
        done += m;
    }
    {
        /* the summed contribution as a one-wavelength path of weight 1 would not be a path;
         * accumulate it directly */
        f->n[i] += 1.0;
        if (f->track) {
            for (k = 0; k < 3; k++) {
                double const d = c[k] - f->acc[3 * i + k];
                f->acc[3 * i + k] += d / f->n[i];
                f->m2[3 * i + k] += d * (c[k] - f->acc[3 * i + k]);
            }
        } else {
            for (k = 0; k < 3; k++) f->acc[3 * i + k] += c[k];
        }
    }
    return ALWAN_OK;
}

alwan_status alwan_spectral_film_add_f32(alwan_spectral_film *film, size_t x, size_t y, alwan_f32 const *lambda,
                                         alwan_f32 const *radiance, alwan_f32 const *weight, size_t count) {
    if (!film || !lambda || !radiance) return ALWAN_E_INVALID;
    if (x >= film->w || y >= film->h) return ALWAN_E_RANGE;
    return alwan__wl_add_f32(film, y * film->w + x, lambda, radiance, weight, count);
}

alwan_status alwan_spectral_film_add_image_f64(alwan_spectral_film *film, alwan_f64 const *lambda, alwan_f64 const *radiance,
                                               alwan_f64 const *weight, size_t count) {
    size_t i, px;
    if (!film || !lambda || !radiance) return ALWAN_E_INVALID;
    px = film->w * film->h;
    for (i = 0; i < px; i++) {
        alwan_status const st = alwan__wl_add(film, i, lambda + i * count, radiance + i * count,
                                              weight ? weight + i * count : NULL, count);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

alwan_status alwan_spectral_film_add_image_f32(alwan_spectral_film *film, alwan_f32 const *lambda, alwan_f32 const *radiance,
                                               alwan_f32 const *weight, size_t count) {
    size_t i, px;
    if (!film || !lambda || !radiance) return ALWAN_E_INVALID;
    px = film->w * film->h;
    for (i = 0; i < px; i++) {
        alwan_status const st = alwan__wl_add_f32(film, i, lambda + i * count, radiance + i * count,
                                                  weight ? weight + i * count : NULL, count);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

/* Pixel i's XYZ mean, scaled. */
static void alwan__wl_mean(double out[3], alwan_spectral_film const *f, size_t i) {
    int k;
    for (k = 0; k < 3; k++) {
        double m;
        if (f->n[i] == 0.0) m = 0.0;
        else m = f->track ? f->acc[3 * i + k] : f->acc[3 * i + k] / f->n[i];
        out[k] = m * f->scale;
    }
}

alwan_status alwan_spectral_film_resolve_xyz_f64(alwan_f64 *out, size_t row_stride, alwan_spectral_film const *film) {
    size_t x, y;
    if (!out || !film) return ALWAN_E_INVALID;
    if (row_stride == 0) row_stride = film->w * 3 * sizeof(alwan_f64);
    for (y = 0; y < film->h; y++) {
        alwan_f64 *row = (alwan_f64 *)((char *)out + y * row_stride);
        for (x = 0; x < film->w; x++) alwan__wl_mean(row + 3 * x, film, y * film->w + x);
    }
    return ALWAN_OK;
}

alwan_status alwan_spectral_film_resolve_xyz_f32(alwan_f32 *out, size_t row_stride, alwan_spectral_film const *film) {
    size_t x, y;
    if (!out || !film) return ALWAN_E_INVALID;
    if (row_stride == 0) row_stride = film->w * 3 * sizeof(alwan_f32);
    for (y = 0; y < film->h; y++) {
        alwan_f32 *row = (alwan_f32 *)((char *)out + y * row_stride);
        for (x = 0; x < film->w; x++) {
            double m[3];
            alwan__wl_mean(m, film, y * film->w + x);
            row[3 * x + 0] = (alwan_f32)m[0];
            row[3 * x + 1] = (alwan_f32)m[1];
            row[3 * x + 2] = (alwan_f32)m[2];
        }
    }
    return ALWAN_OK;
}

/* The XYZ-to-RGB matrix of the space, adapted from the film's white when asked. */
static alwan_status alwan__wl_rgb_matrix(double m[9], alwan_spectral_film const *f, alwan_rgb_space space, int adapt,
                                         alwan_ctx *ctx) {
    alwan_rgb_space_desc_f64 desc;
    alwan_mat3x3_f64 cat, to_xyz, to_rgb;
    alwan_xyz_f64 src, dst;
    alwan_status st = alwan_rgb_get_space_descriptor_f64(&desc, space, ctx);
    int i, j, k;
    if (st != ALWAN_OK) return st;
    if (desc.has_matrices) {
        to_rgb = desc.xyz_to_rgb;
    } else {
        st = alwan_rgb_derive_matrices_f64(&to_xyz, &to_rgb, &desc);
        if (st != ALWAN_OK) return st;
    }
    for (i = 0; i < 9; i++) cat.m[i] = (i % 4 == 0) ? 1.0 : 0.0;
    if (adapt) {
        src.x = f->white[0], src.y = f->white[1], src.z = f->white[2];
        dst.x = desc.white_xy[0] / desc.white_xy[1];
        dst.y = 1.0;
        dst.z = (1.0 - desc.white_xy[0] - desc.white_xy[1]) / desc.white_xy[1];
        if (!(ALWAN_ABS_F64(src.x - dst.x) < 1e-12 && ALWAN_ABS_F64(src.z - dst.z) < 1e-12)) {
            st = alwan_cat_matrix_f64(&cat, &src, &dst, ALWAN_CAT_BRADFORD);
            if (st != ALWAN_OK) return st;
        }
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            double s = 0.0;
            for (k = 0; k < 3; k++) s += to_rgb.m[3 * i + k] * cat.m[3 * k + j];
            m[3 * i + j] = s;
        }
    }
    return ALWAN_OK;
}

alwan_status alwan_spectral_film_resolve_rgb_f64(alwan_f64 *out, size_t row_stride, alwan_spectral_film const *film,
                                                 alwan_rgb_space space, int adapt, alwan_ctx *ctx) {
    double m[9];
    size_t x, y;
    alwan_status st;
    if (!out || !film) return ALWAN_E_INVALID;
    st = alwan__wl_rgb_matrix(m, film, space, adapt, ctx);
    if (st != ALWAN_OK) return st;
    if (row_stride == 0) row_stride = film->w * 3 * sizeof(alwan_f64);
    for (y = 0; y < film->h; y++) {
        alwan_f64 *row = (alwan_f64 *)((char *)out + y * row_stride);
        for (x = 0; x < film->w; x++) {
            double v[3];
            alwan__wl_mean(v, film, y * film->w + x);
            row[3 * x + 0] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2];
            row[3 * x + 1] = m[3] * v[0] + m[4] * v[1] + m[5] * v[2];
            row[3 * x + 2] = m[6] * v[0] + m[7] * v[1] + m[8] * v[2];
        }
    }
    return ALWAN_OK;
}

alwan_status alwan_spectral_film_resolve_rgb_f32(alwan_f32 *out, size_t row_stride, alwan_spectral_film const *film,
                                                 alwan_rgb_space space, int adapt, alwan_ctx *ctx) {
    double m[9];
    size_t x, y;
    alwan_status st;
    if (!out || !film) return ALWAN_E_INVALID;
    st = alwan__wl_rgb_matrix(m, film, space, adapt, ctx);
    if (st != ALWAN_OK) return st;
    if (row_stride == 0) row_stride = film->w * 3 * sizeof(alwan_f32);
    for (y = 0; y < film->h; y++) {
        alwan_f32 *row = (alwan_f32 *)((char *)out + y * row_stride);
        for (x = 0; x < film->w; x++) {
            double v[3];
            alwan__wl_mean(v, film, y * film->w + x);
            row[3 * x + 0] = (alwan_f32)(m[0] * v[0] + m[1] * v[1] + m[2] * v[2]);
            row[3 * x + 1] = (alwan_f32)(m[3] * v[0] + m[4] * v[1] + m[5] * v[2]);
            row[3 * x + 2] = (alwan_f32)(m[6] * v[0] + m[7] * v[1] + m[8] * v[2]);
        }
    }
    return ALWAN_OK;
}

/* Pixel i's variance of the mean, scaled: m2 / (n - 1) / n x scale^2. */
static void alwan__wl_var(double out[3], alwan_spectral_film const *f, size_t i) {
    int k;
    double const n = f->n[i];
    for (k = 0; k < 3; k++) out[k] = n < 2.0 ? 0.0 : f->m2[3 * i + k] / (n - 1.0) / n * f->scale * f->scale;
}

alwan_status alwan_spectral_film_resolve_variance_f64(alwan_f64 *out, size_t row_stride, alwan_spectral_film const *film) {
    size_t x, y;
    if (!out || !film || !film->track) return ALWAN_E_INVALID;
    if (row_stride == 0) row_stride = film->w * 3 * sizeof(alwan_f64);
    for (y = 0; y < film->h; y++) {
        alwan_f64 *row = (alwan_f64 *)((char *)out + y * row_stride);
        for (x = 0; x < film->w; x++) alwan__wl_var(row + 3 * x, film, y * film->w + x);
    }
    return ALWAN_OK;
}

alwan_status alwan_spectral_film_resolve_variance_f32(alwan_f32 *out, size_t row_stride, alwan_spectral_film const *film) {
    size_t x, y;
    if (!out || !film || !film->track) return ALWAN_E_INVALID;
    if (row_stride == 0) row_stride = film->w * 3 * sizeof(alwan_f32);
    for (y = 0; y < film->h; y++) {
        alwan_f32 *row = (alwan_f32 *)((char *)out + y * row_stride);
        for (x = 0; x < film->w; x++) {
            double v[3];
            alwan__wl_var(v, film, y * film->w + x);
            row[3 * x + 0] = (alwan_f32)v[0];
            row[3 * x + 1] = (alwan_f32)v[1];
            row[3 * x + 2] = (alwan_f32)v[2];
        }
    }
    return ALWAN_OK;
}

alwan_status alwan_spectral_film_path_count(size_t *count_out, alwan_spectral_film const *film, size_t x, size_t y) {
    if (!count_out || !film) return ALWAN_E_INVALID;
    if (x >= film->w || y >= film->h) return ALWAN_E_RANGE;
    *count_out = (size_t)film->n[y * film->w + x];
    return ALWAN_OK;
}
