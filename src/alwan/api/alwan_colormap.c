/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Scalar to colour maps. viridis, magma, inferno and plasma are Nathaniel Smith and
 * Stefan van der Walt's mpl-colormaps (CC0); turbo is Anton Mikhailov's (Copyright 2019
 * Google LLC, Apache-2.0). The 256-entry tables are the ones matplotlib ships
 * (gendata/data/colormaps.py); alwan reads them by linear interpolation or exactly as
 * matplotlib's ListedColormap does. Suite 302.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_table_core.h"
#include <string.h>

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static double const cm_viridis[256 * 3] = {
#include "../data/colormaps/viridis.csv"
};
static double const cm_magma[256 * 3] = {
#include "../data/colormaps/magma.csv"
};
static double const cm_inferno[256 * 3] = {
#include "../data/colormaps/inferno.csv"
};
static double const cm_plasma[256 * 3] = {
#include "../data/colormaps/plasma.csv"
};
static double const cm_turbo[256 * 3] = {
#include "../data/colormaps/turbo.csv"
};
ALWAN_DIAG_POP

#define CM_N 256

static double const *cm_table(alwan_colormap map) {
    switch (map) {
    case ALWAN_COLORMAP_VIRIDIS: return cm_viridis;
    case ALWAN_COLORMAP_MAGMA: return cm_magma;
    case ALWAN_COLORMAP_INFERNO: return cm_inferno;
    case ALWAN_COLORMAP_PLASMA: return cm_plasma;
    case ALWAN_COLORMAP_TURBO: return cm_turbo;
    default: return NULL;
    }
}

alwan_status alwan_colormap_table(alwan_f64 *rgb_out, size_t *count, alwan_colormap map) {
    double const *t = cm_table(map);
    size_t i;
    if (!t || !count) return ALWAN_E_INVALID;
    if (rgb_out) {
        if (*count < CM_N) return ALWAN_E_INVALID;
        for (i = 0; i < CM_N * 3; i++) rgb_out[i] = t[i];
    }
    *count = CM_N;
    return ALWAN_OK;
}

typedef struct {
    double const *t;
    alwan_colormap_params p;
    double under[4], over[4], bad[4];
} cm_setup;

static void cm_entry(double o[4], double const *t, size_t i) {
    o[0] = t[3 * i];
    o[1] = t[3 * i + 1];
    o[2] = t[3 * i + 2];
    o[3] = 1.0;
}

static alwan_status cm_prepare(cm_setup *s, alwan_colormap map, alwan_colormap_params const *params) {
    s->t = cm_table(map);
    if (!s->t) return ALWAN_E_INVALID;
    if (params) s->p = *params;
    else memset(&s->p, 0, sizeof s->p);
    if ((unsigned)s->p.lookup > (unsigned)ALWAN_COLORMAP_LOOKUP_MATPLOTLIB) return ALWAN_E_INVALID;
    if (s->p.vmin == 0.0 && s->p.vmax == 0.0) s->p.vmax = 1.0;
    if (!(s->p.vmax != s->p.vmin)) return ALWAN_E_INVALID;
    if (s->p.use_under) memcpy(s->under, s->p.under, sizeof s->under);
    else cm_entry(s->under, s->t, 0);
    if (s->p.use_over) memcpy(s->over, s->p.over, sizeof s->over);
    else cm_entry(s->over, s->t, CM_N - 1);
    if (s->p.use_bad) memcpy(s->bad, s->p.bad, sizeof s->bad);
    else memset(s->bad, 0, sizeof s->bad);   /* matplotlib's transparent black */
    return ALWAN_OK;
}

/* The colour at x already normalised to [0, 1]. scaled is x * 256 as the caller computed it
 * (in float for an f32 input, as numpy does for a float32 array), used by MATPLOTLIB; a
 * user colour or the bad colour is flagged so the encoding step leaves it alone. */
static int cm_lookup(cm_setup const *s, double x, double scaled, double o[4]) {
    if (x != x) {
        memcpy(o, s->bad, 4 * sizeof(double));
        return s->p.use_bad ? 1 : 2;
    }
    if (s->p.lookup == ALWAN_COLORMAP_LOOKUP_MATPLOTLIB) {
        double xa = scaled;
        if (xa == (double)CM_N) xa = CM_N - 1;
        if (xa < 0.0) {
            memcpy(o, s->under, 4 * sizeof(double));
            return s->p.use_under;
        }
        if (xa >= (double)CM_N) {
            memcpy(o, s->over, 4 * sizeof(double));
            return s->p.use_over;
        }
        /* xa is in [0, 256) here, so the cell's i0 is floor(xa), matplotlib's bin */
        cm_entry(o, s->t, (size_t)alwan_table_cell_unit_f64_v(xa, CM_N).i0);
        return 0;
    }
    if (x < 0.0) {
        memcpy(o, s->under, 4 * sizeof(double));
        return s->p.use_under;
    }
    if (x > 1.0) {
        memcpy(o, s->over, 4 * sizeof(double));
        return s->p.use_over;
    }
    {
        alwan_table_cell_f64 const cell = alwan_table_cell_f64_v(x, CM_N);   /* x * 255, i0, i1, frac */
        size_t const i0 = (size_t)cell.i0, i1 = (size_t)cell.i1;
        int c;
        for (c = 0; c < 3; c++) o[c] = s->t[3 * i0 + c] * (1.0 - cell.frac) + s->t[3 * i1 + c] * cell.frac;
        o[3] = 1.0;
    }
    return 0;
}

static void cm_decode(double o[4]) {
#if ALWAN_WITH_F64
    alwan_f64 t[3];
    alwan_eotf_apply_f64(t, sizeof(alwan_f64), o, sizeof(alwan_f64), 3, ALWAN_TF_SRGB);
    o[0] = t[0]; o[1] = t[1]; o[2] = t[2];
#else
    alwan_f32 a[3], t[3];
    a[0] = (alwan_f32)o[0]; a[1] = (alwan_f32)o[1]; a[2] = (alwan_f32)o[2];
    alwan_eotf_apply_f32(t, sizeof(alwan_f32), a, sizeof(alwan_f32), 3, ALWAN_TF_SRGB);
    o[0] = t[0]; o[1] = t[1]; o[2] = t[2];
#endif
}

static void cm_store(void *row, alwan_pixel_format f, size_t i, double v, int truncate) {
    double q;
    switch (f) {
    case ALWAN_PIXEL_U8:
        q = truncate ? ALWAN_FLOOR(v * 255.0) : ALWAN_FLOOR(v * 255.0 + 0.5);
        ((uint8_t *)row)[i] = (uint8_t)(q < 0.0 ? 0.0 : (q > 255.0 ? 255.0 : q));
        break;
    case ALWAN_PIXEL_U16:
        q = truncate ? ALWAN_FLOOR(v * 65535.0) : ALWAN_FLOOR(v * 65535.0 + 0.5);
        ((uint16_t *)row)[i] = (uint16_t)(q < 0.0 ? 0.0 : (q > 65535.0 ? 65535.0 : q));
        break;
    case ALWAN_PIXEL_F32: ((alwan_f32 *)row)[i] = (alwan_f32)v; break;
    default: ((alwan_f64 *)row)[i] = v; break;
    }
}

static size_t cm_fmt_size(alwan_pixel_format f) {
    return f == ALWAN_PIXEL_U8 ? 1u : f == ALWAN_PIXEL_U16 ? 2u : f == ALWAN_PIXEL_F32 ? 4u : f == ALWAN_PIXEL_F64 ? 8u : 0u;
}

static alwan_status cm_apply(void *out, size_t out_stride, alwan_pixel_format out_fmt, size_t out_channels,
                             void const *in, int in_is_f32, size_t in_stride, size_t width, size_t height,
                             alwan_colormap map, alwan_colormap_params const *params) {
    cm_setup s;
    alwan_status st;
    size_t const es = cm_fmt_size(out_fmt);
    size_t x, y, c;
    int const in_identity_norm = !params || (params->vmin == 0.0 && (params->vmax == 0.0 || params->vmax == 1.0));
    if (!out || !in || width == 0 || height == 0 || es == 0 || (out_channels != 3 && out_channels != 4)) return ALWAN_E_INVALID;
    if (out_stride < width * out_channels * es || in_stride < width * (in_is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64)))
        return ALWAN_E_INVALID;
    st = cm_prepare(&s, map, params);
    if (st != ALWAN_OK) return st;
    for (y = 0; y < height; y++) {
        void const *ir = (char const *)in + y * in_stride;
        void *orow = (char *)out + y * out_stride;
        for (x = 0; x < width; x++) {
            double xn, scaled, o[4];
            int user;
            if (in_is_f32) {
                alwan_f32 const vf = ((alwan_f32 const *)ir)[x];
                alwan_f32 xf = in_identity_norm ? vf
                    : (alwan_f32)((vf - (alwan_f32)s.p.vmin) / ((alwan_f32)s.p.vmax - (alwan_f32)s.p.vmin));
                xn = (double)xf;
                scaled = (double)(xf * (alwan_f32)CM_N);   /* float32, as numpy scales a float32 array */
            } else {
                double const v = ((alwan_f64 const *)ir)[x];
                xn = in_identity_norm ? v : (v - s.p.vmin) / (s.p.vmax - s.p.vmin);
                scaled = xn * (double)CM_N;
            }
            user = cm_lookup(&s, xn, scaled, o);
            if (s.p.linear_output && user == 0) cm_decode(o);
            for (c = 0; c < out_channels; c++)
                cm_store(orow, out_fmt, x * out_channels + c, o[c], s.p.lookup == ALWAN_COLORMAP_LOOKUP_MATPLOTLIB);
        }
    }
    return ALWAN_OK;
}

#if ALWAN_WITH_F64
alwan_status alwan_colormap_apply_f64(void *out, size_t out_row_stride, alwan_pixel_format out_fmt, size_t out_channels,
                                      alwan_f64 const *in, size_t in_row_stride, size_t width, size_t height,
                                      alwan_colormap map, alwan_colormap_params const *params) {
    return cm_apply(out, out_row_stride, out_fmt, out_channels, in, 0, in_row_stride, width, height, map, params);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_colormap_apply_f32(void *out, size_t out_row_stride, alwan_pixel_format out_fmt, size_t out_channels,
                                      alwan_f32 const *in, size_t in_row_stride, size_t width, size_t height,
                                      alwan_colormap map, alwan_colormap_params const *params) {
    return cm_apply(out, out_row_stride, out_fmt, out_channels, in, 1, in_row_stride, width, height, map, params);
}
#endif
