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

/* ---------------------------------------------------------------- the inverse */

/* The colour difference between the target and what c, m, y at this black would print. */
static double alwan__cmyk_miss(alwan_cmyk_model const *m, alwan_lab_f64 const *target, double c, double mg, double y,
                               double k) {
    alwan_lab_f64 got;
    double lab[3];
    if (alwan__cmyk_to_lab(lab, m, c, mg, y, k) != ALWAN_OK) return 1e30;
    got.L = lab[0];
    got.a = lab[1];
    got.b = lab[2];
    return alwan_delta_e_2000_f64(&got, target);
}

/* Walk downhill from v on a step that halves, trying the whole neighbourhood rather than
 * one colorant at a time, and return the difference left. Sixteen halvings take the step
 * from an eighth to under two parts in a million, past any ink a press can hold. */
static double alwan__cmyk_walk_from(double *v, alwan_cmyk_model const *m, alwan_lab_f64 const *target,
                                   double k, double miss, double step0, int halvings) {
    double step;
    int it, i;
    for (step = step0, it = 0; it < halvings; it++, step *= 0.5) {
        int moved = 1;
        while (moved) {
            int dc, dm, dy;
            moved = 0;
            for (dc = -1; dc <= 1; dc++) {
                for (dm = -1; dm <= 1; dm++) {
                    for (dy = -1; dy <= 1; dy++) {
                        double trial[3];
                        double d;
                        int same = 1;
                        if (dc == 0 && dm == 0 && dy == 0) continue;
                        trial[0] = v[0] + (double)dc * step;
                        trial[1] = v[1] + (double)dm * step;
                        trial[2] = v[2] + (double)dy * step;
                        for (i = 0; i < 3; i++) {
                            if (trial[i] < 0.0) trial[i] = 0.0;
                            if (trial[i] > 1.0) trial[i] = 1.0;
                            if (trial[i] != v[i]) same = 0;
                        }
                        if (same) continue;
                        d = alwan__cmyk_miss(m, target, trial[0], trial[1], trial[2], k);
                        if (d < miss) {
                            miss = d;
                            v[0] = trial[0];
                            v[1] = trial[1];
                            v[2] = trial[2];
                            moved = 1;
                        }
                    }
                }
            }
        }
    }
    return miss;
}

static double alwan__cmyk_walk(double *v, alwan_cmyk_model const *m, alwan_lab_f64 const *target,
                               double k, double miss) {
    return alwan__cmyk_walk_from(v, m, target, k, miss, 0.125, 16);
}

/* The CMY nearest a Lab at a fixed black. The scan covers the target's densest cube, and
 * the walk starts from the best several of its nodes rather than only the best one: well
 * outside the gamut the difference has more than one dip, and the deepest is not always
 * the one under the best node. Every candidate is scored by the forward model itself, so
 * the two directions cannot drift apart. */
enum { ALWAN__CMYK_STARTS = 8 };

static alwan_status alwan__lab_to_cmyk(double *out, double *miss_out, alwan_cmyk_model const *m,
                                       alwan_lab_f64 const *target, double k) {
    int const n = k_plane_n[0];              /* the densest cube the target carries */
    double const *levels = k_plane_levels[0];
    double start[ALWAN__CMYK_STARTS][3], start_d[ALWAN__CMYK_STARTS];
    double best[3], miss = 1e30;
    int i, j, l, s, found = 0;
    if (!m) return ALWAN_E_INVALID;
    for (s = 0; s < ALWAN__CMYK_STARTS; s++) {
        start_d[s] = 1e30;
        start[s][0] = start[s][1] = start[s][2] = 0.0;
    }
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            for (l = 0; l < n; l++) {
                double const c = levels[i] * 0.01, mg = levels[j] * 0.01, y = levels[l] * 0.01;
                double const d = alwan__cmyk_miss(m, target, c, mg, y, k);
                if (d >= 1e30) continue;
                found = 1;
                for (s = 0; s < ALWAN__CMYK_STARTS; s++) {
                    if (d < start_d[s]) {                    /* keep the list sorted, best first */
                        int t;
                        for (t = ALWAN__CMYK_STARTS - 1; t > s; t--) {
                            start_d[t] = start_d[t - 1];
                            start[t][0] = start[t - 1][0];
                            start[t][1] = start[t - 1][1];
                            start[t][2] = start[t - 1][2];
                        }
                        start_d[s] = d;
                        start[s][0] = c;
                        start[s][1] = mg;
                        start[s][2] = y;
                        break;
                    }
                }
            }
        }
    }
    if (!found) return ALWAN_E_INVALID;
    for (s = 0; s < ALWAN__CMYK_STARTS; s++) {
        double v[3], d;
        if (start_d[s] >= 1e30) break;
        v[0] = start[s][0];
        v[1] = start[s][1];
        v[2] = start[s][2];
        d = alwan__cmyk_walk(v, m, target, k, start_d[s]);
        if (d < miss) {
            miss = d;
            best[0] = v[0];
            best[1] = v[1];
            best[2] = v[2];
        }
    }
    out[0] = best[0];
    out[1] = best[1];
    out[2] = best[2];
    if (miss_out) *miss_out = miss;
    return ALWAN_OK;
}

alwan_status alwan_lab_to_cmyk_f64(alwan_cmyk_f64 *cmyk_out, alwan_f64 *delta_e_out, alwan_lab_f64 const *lab,
                                   alwan_f64 k, alwan_cmyk_model const *model) {
    alwan_lab_f64 target;
    double cmy[3], miss = 0.0;
    alwan_status st;
    if (!cmyk_out || !lab || !model) return ALWAN_E_INVALID;
    if (!(k >= 0.0 && k <= 1.0)) return ALWAN_E_INVALID;
    target = *lab;
    ALWAN_DENORM_LAB(&target);
    st = alwan__lab_to_cmyk(cmy, &miss, model, &target, (double)k);
    if (st != ALWAN_OK) return st;
    cmyk_out->c = cmy[0];
    cmyk_out->m = cmy[1];
    cmyk_out->y = cmy[2];
    cmyk_out->k = k;
    if (delta_e_out) *delta_e_out = miss;
    return ALWAN_OK;
}

alwan_status alwan_lab_to_cmyk_f32(alwan_cmyk_f32 *cmyk_out, alwan_f32 *delta_e_out, alwan_lab_f32 const *lab,
                                   alwan_f32 k, alwan_cmyk_model const *model) {
    alwan_lab_f64 target;
    double cmy[3], miss = 0.0;
    alwan_status st;
    if (!cmyk_out || !lab || !model) return ALWAN_E_INVALID;
    if (!(k >= 0.0f && k <= 1.0f)) return ALWAN_E_INVALID;
    target.L = (double)lab->L;
    target.a = (double)lab->a;
    target.b = (double)lab->b;
    ALWAN_DENORM_LAB(&target);
    st = alwan__lab_to_cmyk(cmy, &miss, model, &target, (double)k);
    if (st != ALWAN_OK) return st;
    cmyk_out->c = (alwan_f32)cmy[0];
    cmyk_out->m = (alwan_f32)cmy[1];
    cmyk_out->y = (alwan_f32)cmy[2];
    cmyk_out->k = k;
    if (delta_e_out) *delta_e_out = (alwan_f32)miss;
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

/* Gated: this calls the alwan_chart_*_f32 accessors, which an f64-only build does not have.
 * It was compiled ungated, so ALWAN_BUILD_PRECISION=f64 built it and then
 * failed to link. */
#if ALWAN_WITH_F32
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
#endif /* ALWAN_WITH_F32 */

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


/* ----------------------------------------------------------------
 * A cached inverse, so CMYK can be a per-pixel target
 *
 * alwan_lab_to_cmyk_{T} scans the densest CMY cube, 729 nodes, and then walks downhill
 * from the best eight of them, which is some thousands of forward evaluations with a
 * CIEDE2000 on each. Measured on the embedded FOGRA39 it is 1.2 ms a call, so a
 * 1920 x 1080 frame is 42 minutes. That is the whole reason this exists. With the cache
 * an in-gamut query is 25 us, so the same frame is 53 seconds, and over 729 in-gamut
 * targets the worst it falls behind the exact search is 0.0415 dE.
 *
 * The cache is a regular grid over the Lab box the characterisation can reach at one
 * fixed black, holding at each node the CMY that the exact search returned for it. A
 * query is then a trilinear interpolation of C, M and Y, which is arithmetic and no
 * search.
 *
 * Two things about it are deliberate.
 *
 * The black is fixed when the cache is built, not when it is queried. That follows
 * alwan_lab_to_cmyk_{T}, where k is an input because a colour can be printed with more
 * ink and less black or the reverse. A cache over a fourth axis would interpolate
 * between two different ink-sharing decisions and produce neither.
 *
 * delta_e_out is MEASURED, not interpolated: the CMY that comes out of the grid is put
 * back through the forward model and compared to the target by CIEDE2000, at a cost of
 * one evaluation. So the number reports what this path actually did, including the grid
 * error, and cannot flatter itself. Interpolating the residual alongside the inks would
 * have been cheaper and would have hidden exactly the cases worth knowing about: where
 * the forward map folds, two very different ink mixes print nearly the same colour, and
 * a blend of them prints something else. size is the one knob, and the reported
 * difference is how you tell whether it is large enough.
 *
 * NOT TESTABLE AGAINST AN EXTERNAL REFERENCE. No library inverts a printing
 * characterisation this way for comparison; Argyll and littleCMS build a B2A table
 * inside an ICC profile, which is the same idea with a different container and no
 * published intermediate to check against. Suite 143 uses the exact search as the
 * reference instead, which is the stronger check here: the cache exists only to
 * approximate that function, so the function it approximates is the right oracle. The
 * characterisation itself is ISO 12642-2 (IT8.7/4) data, and FOGRA39L is
 * ISO 12647-2:2004/Amd 1 coated.
 * ---------------------------------------------------------------- */

enum { ALWAN__CMYK_INV_MIN = 4, ALWAN__CMYK_INV_MAX = 64, ALWAN__CMYK_BOX_STEPS = 17,
       ALWAN__CMYK_INV_HALVINGS = 6, ALWAN__CMYK_INV_CANDIDATES = 9 };

struct alwan_cmyk_inverse_s {
    int size;
    double k;
    double lo[3];
    double hi[3];
    double *cmy;      /* size^3 * 3, ((iL * size + ia) * size + ib) * 3 */
    /* A COPY of the characterisation, not a pointer to it. The residual is measured
     * through the forward model at query time, so the model has to be reachable then,
     * and a borrowed pointer would put a lifetime rule in the caller's hands for the
     * sake of the 105 KB this costs. */
    alwan_cmyk_model model;
};

/* The Lab box the characterisation reaches at this black. Swept rather than assumed: a
 * printing gamut is not symmetric about anything and its L range at k = 1 is a sliver. */
static alwan_status alwan__cmyk_box(double *lo, double *hi, alwan_cmyk_model const *m, double k) {
    int i, j, l, r;
    int found = 0;
    for (r = 0; r < 3; r++) { lo[r] = 1e30; hi[r] = -1e30; }
    for (i = 0; i < ALWAN__CMYK_BOX_STEPS; i++) {
        for (j = 0; j < ALWAN__CMYK_BOX_STEPS; j++) {
            for (l = 0; l < ALWAN__CMYK_BOX_STEPS; l++) {
                double lab[3];
                double const d = (double)(ALWAN__CMYK_BOX_STEPS - 1);
                if (alwan__cmyk_to_lab(lab, m, (double)i / d, (double)j / d, (double)l / d, k)
                    != ALWAN_OK) {
                    continue;
                }
                found = 1;
                for (r = 0; r < 3; r++) {
                    if (lab[r] < lo[r]) lo[r] = lab[r];
                    if (lab[r] > hi[r]) hi[r] = lab[r];
                }
            }
        }
    }
    if (!found) return ALWAN_E_INVALID;
    for (r = 0; r < 3; r++) {
        /* A degenerate axis would divide by zero on lookup. Widen it rather than
         * refuse: k = 1 is a legitimate query and its a and b barely move. */
        if (!(hi[r] - lo[r] > 1e-9)) { lo[r] -= 0.5; hi[r] += 0.5; }
    }
    return ALWAN_OK;
}

alwan_status alwan_cmyk_inverse_create(alwan_cmyk_inverse **out, alwan_cmyk_model const *model,
                                      alwan_f64 k, int size, alwan_ctx *ctx) {
    alwan_cmyk_inverse *inv;
    size_t nodes;
    int iL;
    alwan_status st;
    double lo[3], hi[3];
    (void)ctx;
    if (out) *out = NULL;
    if (!out || !model) return ALWAN_E_INVALID;
    if (!((double)k >= 0.0 && (double)k <= 1.0)) return ALWAN_E_INVALID;
    if (size < ALWAN__CMYK_INV_MIN || size > ALWAN__CMYK_INV_MAX) return ALWAN_E_RANGE;
    st = alwan__cmyk_box(lo, hi, model, (double)k);
    if (st != ALWAN_OK) return st;

    nodes = (size_t)size * (size_t)size * (size_t)size;
    inv = (alwan_cmyk_inverse *)ALWAN_ALLOC(sizeof(*inv), sizeof(double));
    if (!inv) return ALWAN_E_NOMEM;
    inv->cmy = (double *)ALWAN_ALLOC(alwan_safe_array_size(nodes * 3, sizeof(double)),
                                     sizeof(double));
    if (!inv->cmy) { ALWAN_FREE(inv); return ALWAN_E_NOMEM; }
    inv->size = size;
    inv->k = (double)k;
    inv->model = *model;
    for (iL = 0; iL < 3; iL++) { inv->lo[iL] = lo[iL]; inv->hi[iL] = hi[iL]; }

    for (iL = 0; iL < size; iL++) {
        int ia;
        for (ia = 0; ia < size; ia++) {
            int ib;
            for (ib = 0; ib < size; ib++) {
                alwan_lab_f64 target;
                double cmy[3];
                size_t const at = (size_t)((iL * size + ia) * size + ib) * 3;
                double const d = (double)(size - 1);
                target.L = lo[0] + (hi[0] - lo[0]) * (double)iL / d;
                target.a = lo[1] + (hi[1] - lo[1]) * (double)ia / d;
                target.b = lo[2] + (hi[2] - lo[2]) * (double)ib / d;
                st = alwan__lab_to_cmyk(cmy, NULL, model, &target, (double)k);
                if (st != ALWAN_OK) {
                    ALWAN_FREE(inv->cmy);
                    ALWAN_FREE(inv);
                    return st;
                }
                inv->cmy[at + 0] = cmy[0];
                inv->cmy[at + 1] = cmy[1];
                inv->cmy[at + 2] = cmy[2];
            }
        }
    }
    *out = inv;
    return ALWAN_OK;
}

void alwan_cmyk_inverse_destroy(alwan_cmyk_inverse *inv, alwan_ctx *ctx) {
    (void)ctx;
    if (!inv) return;
    ALWAN_FREE(inv->cmy);
    ALWAN_FREE(inv);
}

int alwan_cmyk_inverse_size(alwan_cmyk_inverse const *inv) {
    return inv ? inv->size : 0;
}


/* Trilinear over the grid, with the query clamped into the box. Clamping is the right
 * answer rather than a convenience: a Lab outside the box is outside what this
 * characterisation can print, the edge node already holds the closest ink it has, and
 * the measured difference that goes back to the caller says how far short it fell. */
static void alwan__cmyk_inv_lookup(double *cmy, double *corners,
                                   alwan_cmyk_inverse const *inv, double const *lab) {
    double t[3];
    int i0[3], i1[3];
    int r, a, b, c;
    int const n = inv->size;
    for (r = 0; r < 3; r++) {
        double u = (lab[r] - inv->lo[r]) / (inv->hi[r] - inv->lo[r]);
        double f;
        if (u < 0.0) u = 0.0;
        if (u > 1.0) u = 1.0;
        u *= (double)(n - 1);
        i0[r] = (int)u;
        if (i0[r] > n - 2) i0[r] = n - 2;
        if (i0[r] < 0) i0[r] = 0;
        i1[r] = i0[r] + 1;
        f = u - (double)i0[r];
        if (f < 0.0) f = 0.0;
        if (f > 1.0) f = 1.0;
        t[r] = f;
    }
    cmy[0] = cmy[1] = cmy[2] = 0.0;
    for (a = 0; a < 2; a++) {
        double const wa = a ? t[0] : 1.0 - t[0];
        int const la = a ? i1[0] : i0[0];
        for (b = 0; b < 2; b++) {
            double const wb = b ? t[1] : 1.0 - t[1];
            int const lb = b ? i1[1] : i0[1];
            for (c = 0; c < 2; c++) {
                double const wc = c ? t[2] : 1.0 - t[2];
                int const lc = c ? i1[2] : i0[2];
                size_t const at = (size_t)((la * n + lb) * n + lc) * 3;
                double const w = wa * wb * wc;
                cmy[0] += w * inv->cmy[at + 0];
                cmy[1] += w * inv->cmy[at + 1];
                cmy[2] += w * inv->cmy[at + 2];
                if (corners) {
                    int const slot = ((a * 2) + b) * 2 + c;
                    corners[slot * 3 + 0] = inv->cmy[at + 0];
                    corners[slot * 3 + 1] = inv->cmy[at + 1];
                    corners[slot * 3 + 2] = inv->cmy[at + 2];
                }
            }
        }
    }
    for (r = 0; r < 3; r++) {
        if (cmy[r] < 0.0) cmy[r] = 0.0;
        if (cmy[r] > 1.0) cmy[r] = 1.0;
    }
}

/* The grid gives the starting points; a short walk from the best gives the answer.
 *
 * Interpolation alone is not good enough to ship, and measuring said so. On the
 * embedded FOGRA39 at k = 0.2, against the exact search over 343 in-gamut targets and
 * 27 well outside it, the raw lookup left up to 3.58 dE at size 9 and still 0.91 at
 * size 33, and OUTSIDE the gamut about 10.8 dE at every size. That the out-of-gamut
 * error did not move with size is what identified it: a query beyond the box is clamped
 * onto a boundary face, and the ink stored at that face is the answer for the face, not
 * for the point. No grid density fixes that, because it is not a resolution problem.
 *
 * So the same downhill walk the exact search uses runs from the grid's answer, with the
 * step starting at one thirty-second of the ink range rather than an eighth and taking
 * six halvings rather than sixteen, since it begins near the answer instead of at a cube
 * node. Both paths therefore minimise the same quantity, CIEDE2000 through the forward
 * model, which is what lets suite 143 compare them directly: a difference between them
 * is grid error, not two operators disagreeing about the objective.
 *
 * That alone fixed the inside of the gamut, 3.58 dE down to 0.039, and left the outside
 * at 10.25, which is where the second measurement earned its keep. Adding the walk had
 * barely moved that number, so the cause was not the starting point's accuracy but which
 * minimum it was near. The exact search already says why, and the cache had thrown it
 * away: it walks from the best EIGHT cube nodes because well outside the gamut the
 * difference has more than one dip and the deepest is not under the best node. A single
 * start reproduces exactly the failure those eight exist to avoid.
 *
 * So the candidates are the interpolated inks plus the eight corner inks of the cell the
 * query landed in. Those corners are not guesses: each is what the exact search returned
 * for its own node, so they already sit in whichever dips the search found near here.
 * Nine forward evaluations and a CIEDE2000 each cost about a microsecond against the
 * walk's forty, so this is close to free.
 *
 * That fixed the inside and left the outside of the Lab box wrong by about 10 dE, and
 * three attempts to fix it in the cache all failed in the same informative way.
 *
 * The case that explains it: for L 50, a 90, b -90, a vivid purple 14.7 dE outside this
 * gamut, the exact search returns ink printing a near NEUTRAL grey, (50.0, -4.9, -7.1),
 * and the cache returned ink printing an actual purple, (44.9, 25.3, -20.8), at 21.5 dE.
 * The grey wins on the metric because CIEDE2000 divides chroma error by a term that grows
 * with chroma, so far out it prefers dropping the chroma to missing the hue. Both are
 * genuine minima of the same objective and they are 0.35 apart in ink. Adding the cell's
 * eight corner inks as starts changed the answer by nothing. Adding nine more spanning the
 * ink cube changed it by nothing. Widening the first step to the eighth the exact search
 * uses changed it by nothing either: all three runs returned the same ink to three
 * decimals. What actually finds that minimum is the exact search's scan of all 729 cube
 * nodes, one of which lands inside the far basin, and no small fixed set of starts stands
 * in for it.
 *
 * So the cache does not approximate outside its own domain. A query outside the Lab box
 * runs the exact search, which is not a slow path bolted on but the same rule the rest of
 * this file follows: the box is the region the cache holds data for, and beyond it the
 * honest answer is the one function that does. The cost is real and callers should know
 * it, so alwan.h says so: converting saturated source primaries, which sit well outside
 * any print gamut, pays the exact search per pixel, and the sane thing there is to convert
 * a palette rather than a frame.
 *
 * ALWAN_DENORM_LAB is already applied by the callers, so target is native Lab. */
static alwan_status alwan__cmyk_inv_eval(double *cmy, double *miss,
                                         alwan_cmyk_inverse const *inv,
                                         alwan_lab_f64 const *target) {
    double lab[3];
    double cand[ALWAN__CMYK_INV_CANDIDATES * 3];
    double best[3], best_d = 1e30;
    double left;
    int i;
    lab[0] = target->L;
    lab[1] = target->a;
    lab[2] = target->b;
    for (i = 0; i < 3; i++) {
        if (lab[i] < inv->lo[i] || lab[i] > inv->hi[i]) {
            return alwan__lab_to_cmyk(cmy, miss, &inv->model, target, inv->k);
        }
    }
    alwan__cmyk_inv_lookup(cand, cand + 3, inv, lab);
    for (i = 0; i < ALWAN__CMYK_INV_CANDIDATES; i++) {
        double const d = alwan__cmyk_miss(&inv->model, target, cand[i * 3 + 0],
                                          cand[i * 3 + 1], cand[i * 3 + 2], inv->k);
        if (d < best_d) {
            best_d = d;
            best[0] = cand[i * 3 + 0];
            best[1] = cand[i * 3 + 1];
            best[2] = cand[i * 3 + 2];
        }
    }
    if (best_d >= 1e30) return ALWAN_E_INVALID;
    left = alwan__cmyk_walk_from(best, &inv->model, target, inv->k, best_d,
                                 1.0 / 32.0, ALWAN__CMYK_INV_HALVINGS);
    cmy[0] = best[0];
    cmy[1] = best[1];
    cmy[2] = best[2];
    if (miss) *miss = left;
    return ALWAN_OK;
}

alwan_status alwan_cmyk_inverse_eval_f64(alwan_cmyk_f64 *cmyk_out, alwan_f64 *delta_e_out,
                                        alwan_lab_f64 const *lab, alwan_cmyk_inverse const *inv) {
    alwan_lab_f64 target;
    double cmy[3], miss = 0.0;
    alwan_status st;
    if (!cmyk_out || !lab || !inv) return ALWAN_E_INVALID;
    target = *lab;
    ALWAN_DENORM_LAB(&target);
    st = alwan__cmyk_inv_eval(cmy, &miss, inv, &target);
    if (st != ALWAN_OK) return st;
    cmyk_out->c = cmy[0];
    cmyk_out->m = cmy[1];
    cmyk_out->y = cmy[2];
    cmyk_out->k = (alwan_f64)inv->k;
    if (delta_e_out) *delta_e_out = miss;
    return ALWAN_OK;
}

alwan_status alwan_cmyk_inverse_eval_f32(alwan_cmyk_f32 *cmyk_out, alwan_f32 *delta_e_out,
                                        alwan_lab_f32 const *lab, alwan_cmyk_inverse const *inv) {
    alwan_lab_f64 target;
    double cmy[3], miss = 0.0;
    alwan_status st;
    if (!cmyk_out || !lab || !inv) return ALWAN_E_INVALID;
    target.L = (double)lab->L;
    target.a = (double)lab->a;
    target.b = (double)lab->b;
    ALWAN_DENORM_LAB(&target);
    st = alwan__cmyk_inv_eval(cmy, &miss, inv, &target);
    if (st != ALWAN_OK) return st;
    cmyk_out->c = (alwan_f32)cmy[0];
    cmyk_out->m = (alwan_f32)cmy[1];
    cmyk_out->y = (alwan_f32)cmy[2];
    cmyk_out->k = (alwan_f32)inv->k;
    if (delta_e_out) *delta_e_out = (alwan_f32)miss;
    return ALWAN_OK;
}

alwan_status alwan_cmyk_inverse_map_interleave_f64(alwan_f64 *cmyk_out, size_t out_stride,
                                                  alwan_f64 const *lab_in, size_t in_stride,
                                                  size_t count, alwan_f64 *worst_delta_e_out,
                                                  alwan_cmyk_inverse const *inv) {
    size_t i;
    double worst = 0.0;
    if (!cmyk_out || !lab_in || !inv) return ALWAN_E_INVALID;
    if (out_stride == 0 || in_stride == 0) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        alwan_f64 const *src = (alwan_f64 const *)((char const *)lab_in + i * in_stride);
        alwan_f64 *dst = (alwan_f64 *)((char *)cmyk_out + i * out_stride);
        alwan_lab_f64 target;
        double cmy[3], miss = 0.0;
        alwan_status st;
        target.L = src[0];
        target.a = src[1];
        target.b = src[2];
        ALWAN_DENORM_LAB(&target);
        st = alwan__cmyk_inv_eval(cmy, &miss, inv, &target);
        if (st != ALWAN_OK) return st;
        dst[0] = cmy[0];
        dst[1] = cmy[1];
        dst[2] = cmy[2];
        dst[3] = (alwan_f64)inv->k;
        if (miss > worst) worst = miss;
    }
    if (worst_delta_e_out) *worst_delta_e_out = (alwan_f64)worst;
    return ALWAN_OK;
}

alwan_status alwan_cmyk_inverse_map_interleave_f32(alwan_f32 *cmyk_out, size_t out_stride,
                                                  alwan_f32 const *lab_in, size_t in_stride,
                                                  size_t count, alwan_f32 *worst_delta_e_out,
                                                  alwan_cmyk_inverse const *inv) {
    size_t i;
    double worst = 0.0;
    if (!cmyk_out || !lab_in || !inv) return ALWAN_E_INVALID;
    if (out_stride == 0 || in_stride == 0) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        alwan_f32 const *src = (alwan_f32 const *)((char const *)lab_in + i * in_stride);
        alwan_f32 *dst = (alwan_f32 *)((char *)cmyk_out + i * out_stride);
        alwan_lab_f64 target;
        double cmy[3], miss = 0.0;
        alwan_status st;
        target.L = (double)src[0];
        target.a = (double)src[1];
        target.b = (double)src[2];
        ALWAN_DENORM_LAB(&target);
        st = alwan__cmyk_inv_eval(cmy, &miss, inv, &target);
        if (st != ALWAN_OK) return st;
        dst[0] = (alwan_f32)cmy[0];
        dst[1] = (alwan_f32)cmy[1];
        dst[2] = (alwan_f32)cmy[2];
        dst[3] = (alwan_f32)inv->k;
        if (miss > worst) worst = miss;
    }
    if (worst_delta_e_out) *worst_delta_e_out = (alwan_f32)worst;
    return ALWAN_OK;
}
