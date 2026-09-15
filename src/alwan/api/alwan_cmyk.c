/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * CMYK printing characterisations: CMYK to CIELAB through an ISO 12642-2 (IT8.7/4)
 * data set, built from any measured chart that carries one, or from the embedded
 * FOGRA39.
 *
 * An IT8.7/4 target lays out complete CMY cubes on six K planes. Lab is interpolated
 * multilinearly inside a plane's cube, as scipy's RegularGridInterpolator does, and
 * linearly in K between the two planes around the query. Lab rather than XYZ: on the
 * 321 FOGRA39 patches the model does not use, Lab gives dE2000 mean 0.13 and maximum
 * 1.21 against the file's own values, XYZ 0.24 and 1.52.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_colorspace_core.h"
#include <stddef.h>
#include <string.h>

/* FOGRA39.txt, byte for byte, one line per element; joined before it is parsed.
 * Source: Fogra (https://fogra.org), distributed unmodified under Fogra's terms. */
static char const *const k_fogra39_lines[] = {
#include "../data/cmyk/fogra39.inc"
};

/* The complete CMY cubes ISO 12642-2 lays out on six K planes, in percent. */
enum { ALWAN__CMYK_PLANES = 6, ALWAN__CMYK_LEVELS = 9, ALWAN__CMYK_NODES = 9 * 9 * 9 };
static double const k_plane_k[ALWAN__CMYK_PLANES] = { 0.0, 20.0, 40.0, 60.0, 80.0, 100.0 };
static int const k_plane_n[ALWAN__CMYK_PLANES] = { 9, 6, 5, 5, 4, 2 };
static double const k_plane_levels[ALWAN__CMYK_PLANES][ALWAN__CMYK_LEVELS] = {
    { 0.0, 10.0, 20.0, 30.0, 40.0, 55.0, 70.0, 85.0, 100.0 },
    { 0.0, 10.0, 20.0, 40.0, 70.0, 100.0 },
    { 0.0, 20.0, 40.0, 70.0, 100.0 },
    { 0.0, 20.0, 40.0, 70.0, 100.0 },
    { 0.0, 40.0, 70.0, 100.0 },
    { 0.0, 100.0 },
};

struct alwan_cmyk_model_s {
    /* Native Lab per node, [plane][(c * n + m) * n + y] with each plane's own n. */
    double lab[ALWAN__CMYK_PLANES][ALWAN__CMYK_NODES][3];
};

static int alwan__cmyk_plane_of(double k) {
    int p;
    for (p = 0; p < ALWAN__CMYK_PLANES; p++) {
        if (k >= k_plane_k[p] - 1e-9 && k <= k_plane_k[p] + 1e-9) return p;
    }
    return -1;
}

static int alwan__cmyk_level_of(int p, double v) {
    int i;
    for (i = 0; i < k_plane_n[p]; i++) {
        if (v >= k_plane_levels[p][i] - 1e-9 && v <= k_plane_levels[p][i] + 1e-9) return i;
    }
    return -1;
}

/* Every patch on a plane node is summed into it, so a node the target repeats is the
 * mean of its measurements. A node no patch reaches is ALWAN_E_NODATA: the chart is
 * not an IT8.7/4 characterisation. */
static alwan_status alwan__cmyk_build(alwan_cmyk_model **out, size_t n, double const *cmyk, double const *lab) {
    alwan_cmyk_model *m;
    int *count;
    size_t s;
    int p, q;
    m = (alwan_cmyk_model *)ALWAN_ALLOC(sizeof(*m), sizeof(double));
    count = (int *)ALWAN_ALLOC(sizeof(int) * ALWAN__CMYK_PLANES * ALWAN__CMYK_NODES, sizeof(int));
    if (!m || !count) {
        ALWAN_FREE(m);
        ALWAN_FREE(count);
        return ALWAN_E_NOMEM;
    }
    memset(m, 0, sizeof(*m));
    memset(count, 0, sizeof(int) * ALWAN__CMYK_PLANES * ALWAN__CMYK_NODES);
    for (s = 0; s < n; s++) {
        double const *d = cmyk + 4 * s;
        int const pl = alwan__cmyk_plane_of(d[3]);
        int i, j, k, node;
        if (pl < 0) continue;
        i = alwan__cmyk_level_of(pl, d[0]);
        j = alwan__cmyk_level_of(pl, d[1]);
        k = alwan__cmyk_level_of(pl, d[2]);
        if (i < 0 || j < 0 || k < 0) continue;
        node = (i * k_plane_n[pl] + j) * k_plane_n[pl] + k;
        m->lab[pl][node][0] += lab[3 * s + 0];
        m->lab[pl][node][1] += lab[3 * s + 1];
        m->lab[pl][node][2] += lab[3 * s + 2];
        count[pl * ALWAN__CMYK_NODES + node]++;
    }
    for (p = 0; p < ALWAN__CMYK_PLANES; p++) {
        int const nodes = k_plane_n[p] * k_plane_n[p] * k_plane_n[p];
        for (q = 0; q < nodes; q++) {
            int const c = count[p * ALWAN__CMYK_NODES + q];
            if (c == 0) {
                ALWAN_FREE(m);
                ALWAN_FREE(count);
                return ALWAN_E_NODATA;
            }
            m->lab[p][q][0] /= (double)c;
            m->lab[p][q][1] /= (double)c;
            m->lab[p][q][2] /= (double)c;
        }
    }
    ALWAN_FREE(count);
    *out = m;
    return ALWAN_OK;
}

/* The cell scipy's RegularGridInterpolator picks, searchsorted from the left: a value
 * on an interior level falls in the cell below it, at t = 1. */
static void alwan__cmyk_cell(int *i_out, double *t_out, double const *levels, int n, double x) {
    int i = 0;
    while (i < n - 2 && levels[i + 1] < x) i++;
    *i_out = i;
    *t_out = (x - levels[i]) / (levels[i + 1] - levels[i]);
}

static void alwan__cmyk_plane_lab(double *out, alwan_cmyk_model const *m, int p, double c, double mg, double y) {
    int const n = k_plane_n[p];
    double const *levels = k_plane_levels[p];
    int ic, im, iy, a, b, d;
    double tc, tm, ty, w[3][2];
    alwan__cmyk_cell(&ic, &tc, levels, n, c);
    alwan__cmyk_cell(&im, &tm, levels, n, mg);
    alwan__cmyk_cell(&iy, &ty, levels, n, y);
    w[0][0] = 1.0 - tc;
    w[0][1] = tc;
    w[1][0] = 1.0 - tm;
    w[1][1] = tm;
    w[2][0] = 1.0 - ty;
    w[2][1] = ty;
    out[0] = out[1] = out[2] = 0.0;
    for (a = 0; a < 2; a++) {
        for (b = 0; b < 2; b++) {
            for (d = 0; d < 2; d++) {
                double const wt = w[0][a] * w[1][b] * w[2][d];
                double const *v = m->lab[p][((ic + a) * n + (im + b)) * n + (iy + d)];
                out[0] += v[0] * wt;
                out[1] += v[1] * wt;
                out[2] += v[2] * wt;
            }
        }
    }
}

/* Native Lab of CMYK in [0, 1]; outside [0, 1] is ALWAN_E_INVALID. */
static alwan_status alwan__cmyk_to_lab(double *lab, alwan_cmyk_model const *m, double c, double mg, double y, double k) {
    double v0[3], v1[3];
    int p, p0 = 0, p1 = ALWAN__CMYK_PLANES - 1;
    if (!m) return ALWAN_E_INVALID;
    if (!(c >= 0.0 && c <= 1.0 && mg >= 0.0 && mg <= 1.0 && y >= 0.0 && y <= 1.0 && k >= 0.0 && k <= 1.0)) {
        return ALWAN_E_INVALID;
    }
    c *= 100.0;
    mg *= 100.0;
    y *= 100.0;
    k *= 100.0;
    for (p = 0; p < ALWAN__CMYK_PLANES; p++) {
        if (k_plane_k[p] <= k) p0 = p;
    }
    for (p = ALWAN__CMYK_PLANES - 1; p >= 0; p--) {
        if (k_plane_k[p] >= k) p1 = p;
    }
    alwan__cmyk_plane_lab(v0, m, p0, c, mg, y);
    if (p1 == p0) {
        lab[0] = v0[0];
        lab[1] = v0[1];
        lab[2] = v0[2];
        return ALWAN_OK;
    }
    alwan__cmyk_plane_lab(v1, m, p1, c, mg, y);
    {
        double const t = (k - k_plane_k[p0]) / (k_plane_k[p1] - k_plane_k[p0]);
        lab[0] = v0[0] + t * (v1[0] - v0[0]);
        lab[1] = v0[1] + t * (v1[1] - v0[1]);
        lab[2] = v0[2] + t * (v1[2] - v0[2]);
    }
    return ALWAN_OK;
}

/* A chart's patches as percent CMYK and native Lab: the file's own Lab columns when it
 * has them, else its XYZ in Lab against its illuminant's white. */
static alwan_status alwan__cmyk_from_chart_f64(alwan_cmyk_model **out, alwan_chart_f64 const *chart) {
    size_t const n = alwan_chart_num_patches_f64(chart);
    double *cmyk, *lab;
    alwan_illuminant ill;
    alwan_observer_type obs;
    alwan_xyz_f64 white;
    size_t s;
    alwan_status st = ALWAN_OK;
    if (alwan_chart_device_model_f64(chart) != ALWAN_CHART_DEVICE_CMYK || n == 0) return ALWAN_E_NODATA;
    if (alwan_chart_native_illuminant_f64(&ill, &obs, chart) != ALWAN_OK ||
        alwan_illuminant_white_point_f64(&white, ill, obs) != ALWAN_OK) {
        return ALWAN_E_INVALID;
    }
    cmyk = (double *)ALWAN_ALLOC(alwan_safe_array_size(n * 4, sizeof(double)), sizeof(double));
    lab = (double *)ALWAN_ALLOC(alwan_safe_array_size(n * 3, sizeof(double)), sizeof(double));
    if (!cmyk || !lab) {
        ALWAN_FREE(cmyk);
        ALWAN_FREE(lab);
        return ALWAN_E_NOMEM;
    }
    for (s = 0; s < n && st == ALWAN_OK; s++) {
        alwan_lab_f64 l;
        st = alwan_chart_device_values_f64(cmyk + 4 * s, chart, s);
        if (st != ALWAN_OK) break;
        if (alwan_chart_lab_f64(&l, chart, s) == ALWAN_OK) {
            ALWAN_DENORM_LAB(&l);
        } else {
            alwan_xyz_f64 xyz;
            st = alwan_chart_xyz_f64(&xyz, chart, s);
            l = alwan_xyz_to_lab_f64_v(xyz, white);
        }
        lab[3 * s + 0] = l.L;
        lab[3 * s + 1] = l.a;
        lab[3 * s + 2] = l.b;
    }
    if (st == ALWAN_OK) st = alwan__cmyk_build(out, n, cmyk, lab);
    ALWAN_FREE(cmyk);
    ALWAN_FREE(lab);
    return st;
}

alwan_status alwan_cmyk_model_from_chart_f64(alwan_cmyk_model **out, alwan_chart_f64 const *chart, alwan_ctx *ctx) {
    (void)ctx;
    if (out) *out = NULL;
    if (!out || !chart) return ALWAN_E_INVALID;
    return alwan__cmyk_from_chart_f64(out, chart);
}

alwan_status alwan_cmyk_model_from_chart_f32(alwan_cmyk_model **out, alwan_chart_f32 const *chart, alwan_ctx *ctx) {
    size_t const n = alwan_chart_num_patches_f32(chart);
    double *cmyk, *lab;
    size_t s;
    alwan_illuminant ill;
    alwan_observer_type obs;
    alwan_xyz_f64 white;
    alwan_status st = ALWAN_OK;
    (void)ctx;
    if (out) *out = NULL;
    if (!out || !chart) return ALWAN_E_INVALID;
    if (alwan_chart_device_model_f32(chart) != ALWAN_CHART_DEVICE_CMYK || n == 0) return ALWAN_E_NODATA;
    if (alwan_chart_native_illuminant_f32(&ill, &obs, chart) != ALWAN_OK ||
        alwan_illuminant_white_point_f64(&white, ill, obs) != ALWAN_OK) {
        return ALWAN_E_INVALID;
    }
    cmyk = (double *)ALWAN_ALLOC(alwan_safe_array_size(n * 4, sizeof(double)), sizeof(double));
    lab = (double *)ALWAN_ALLOC(alwan_safe_array_size(n * 3, sizeof(double)), sizeof(double));
    if (!cmyk || !lab) {
        ALWAN_FREE(cmyk);
        ALWAN_FREE(lab);
        return ALWAN_E_NOMEM;
    }
    for (s = 0; s < n && st == ALWAN_OK; s++) {
        alwan_f32 dv[4];
        alwan_lab_f32 l32;
        int i;
        st = alwan_chart_device_values_f32(dv, chart, s);
        if (st != ALWAN_OK) break;
        for (i = 0; i < 4; i++) cmyk[4 * s + i] = (double)dv[i];
        if (alwan_chart_lab_f32(&l32, chart, s) == ALWAN_OK) {
            alwan_lab_f64 l;
            l.L = (double)l32.L;
            l.a = (double)l32.a;
            l.b = (double)l32.b;
            ALWAN_DENORM_LAB(&l);
            lab[3 * s + 0] = l.L;
            lab[3 * s + 1] = l.a;
            lab[3 * s + 2] = l.b;
        } else {
            alwan_xyz_f32 x32;
            alwan_xyz_f64 xyz;
            alwan_lab_f64 l;
            st = alwan_chart_xyz_f32(&x32, chart, s);
            xyz.x = (double)x32.x;
            xyz.y = (double)x32.y;
            xyz.z = (double)x32.z;
            l = alwan_xyz_to_lab_f64_v(xyz, white);
            lab[3 * s + 0] = l.L;
            lab[3 * s + 1] = l.a;
            lab[3 * s + 2] = l.b;
        }
    }
    if (st == ALWAN_OK) st = alwan__cmyk_build(out, n, cmyk, lab);
    ALWAN_FREE(cmyk);
    ALWAN_FREE(lab);
    return st;
}

alwan_status alwan_cmyk_model_fogra39(alwan_cmyk_model **out, alwan_ctx *ctx) {
    size_t const lines = sizeof(k_fogra39_lines) / sizeof(k_fogra39_lines[0]);
    size_t len = 0, i, pos = 0;
    char *buf;
    alwan_chart_f64 *chart = NULL;
    alwan_status st;
    if (out) *out = NULL;
    if (!out) return ALWAN_E_INVALID;
    for (i = 0; i < lines; i++) len += strlen(k_fogra39_lines[i]);
    buf = (char *)ALWAN_ALLOC(len + 1, 1);
    if (!buf) return ALWAN_E_NOMEM;
    for (i = 0; i < lines; i++) {
        size_t const l = strlen(k_fogra39_lines[i]);
        memcpy(buf + pos, k_fogra39_lines[i], l);
        pos += l;
    }
    buf[len] = '\0';
    st = alwan_chart_load_buffer_f64(&chart, buf, len, ctx);
    ALWAN_FREE(buf);
    if (st != ALWAN_OK) return st;
    st = alwan__cmyk_from_chart_f64(out, chart);
    alwan_chart_destroy_f64(chart, ctx);
    return st;
}

void alwan_cmyk_model_destroy(alwan_cmyk_model *model, alwan_ctx *ctx) {
    (void)ctx;
    ALWAN_FREE(model);
}

alwan_status alwan_cmyk_to_lab_f64(alwan_lab_f64 *lab_out, alwan_cmyk_f64 const *cmyk, alwan_cmyk_model const *model) {
    double v[3];
    alwan_status st;
    if (!lab_out || !cmyk) return ALWAN_E_INVALID;
    st = alwan__cmyk_to_lab(v, model, cmyk->c, cmyk->m, cmyk->y, cmyk->k);
    if (st != ALWAN_OK) return st;
    lab_out->L = v[0];
    lab_out->a = v[1];
    lab_out->b = v[2];
    ALWAN_NORM_LAB(lab_out);
    return ALWAN_OK;
}

alwan_status alwan_cmyk_to_lab_f32(alwan_lab_f32 *lab_out, alwan_cmyk_f32 const *cmyk, alwan_cmyk_model const *model) {
    double v[3];
    alwan_lab_f64 l;
    alwan_status st;
    if (!lab_out || !cmyk) return ALWAN_E_INVALID;
    st = alwan__cmyk_to_lab(v, model, (double)cmyk->c, (double)cmyk->m, (double)cmyk->y, (double)cmyk->k);
    if (st != ALWAN_OK) return st;
    l.L = v[0];
    l.a = v[1];
    l.b = v[2];
    ALWAN_NORM_LAB(&l);
    lab_out->L = (alwan_f32)l.L;
    lab_out->a = (alwan_f32)l.a;
    lab_out->b = (alwan_f32)l.b;
    return ALWAN_OK;
}
