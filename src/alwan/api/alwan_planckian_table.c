/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * A sampled Planckian locus, and Ohno 2013's solve on top of it.
 *
 * alwan_cct_to_uv_planck1900 integrates the observer's CMFs on every call, and Ohno's
 * method wants a few thousand locus points per query, so the points are taken once and
 * kept: the table loads the CMFs one time and sums Planck's law across them for each
 * temperature. The solve then only reads the table.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stddef.h>
#include <string.h>

/* Ohno's own bounds and step, which colour-science defaults to as well. */
#define ALWAN__OHNO_START 1000.0
#define ALWAN__OHNO_END 100000.0
#define ALWAN__OHNO_SPACING 1.001

struct alwan_planckian_table_s {
    double *t;   /* temperature, kelvin */
    double *u;   /* CIE 1960 u */
    double *v;   /* CIE 1960 v */
    size_t count;
    alwan_observer_type observer;
};

/* The temperatures Ohno walks: geometric from start, the step easing towards the top so
 * the table stays dense where the locus turns, with start + 1 and end - 1 pinned so the
 * ends carry a neighbour. Returns the count, filling t when it is not NULL. */
static size_t alwan__ohno_temperatures(double *t, double start, double end, double spacing) {
    double next_t = start + 1.0, next_spacing = spacing;
    size_t n = 0;
    if (t) {
        t[0] = start;
        t[1] = start + 1.0;
    }
    n = 2;
    for (;;) {
        double d;
        next_t *= next_spacing;
        if (!(next_t < end)) break;
        if (t) t[n] = next_t;
        n++;
        /* Slightly decrease the step for higher CCT, as colour-science does. */
        d = (next_t - ALWAN__OHNO_START) / (ALWAN__OHNO_END - ALWAN__OHNO_START);
        d = d < 0.0 ? 0.0 : d > 1.0 ? 1.0 : d;
        next_spacing = spacing * (1.0 - d) + (1.0 + (spacing - 1.0) / 10.0) * d;
    }
    if (t) {
        t[n] = end - 1.0;
        t[n + 1] = end;
    }
    return n + 2;
}

/* Planck's law summed against the CMFs already loaded, then CIE 1960 uv. c1, pi and the
 * step multiply X, Y and Z alike and cancel, exactly as alwan_cct_to_uv_planck1900. */
static void alwan__planck_uv(double *u_out, double *v_out, double cct, alwan_spd_f64 const *xb,
                             alwan_spd_f64 const *yb, alwan_spd_f64 const *zb) {
    double X = 0.0, Y = 0.0, Z = 0.0, U, V, W, s;
    size_t i;
    for (i = 0; i < xb->count; i++) {
        double const l = ((double)xb->wavelength_min + (double)i) * 1e-9;
        double const p = (1.0 / (l * l * l * l * l)) / (ALWAN_EXP_F64(1.4388e-2 / (l * cct)) - 1.0);
        X += p * xb->values[i];
        Y += p * yb->values[i];
        Z += p * zb->values[i];
    }
    U = 2.0 / 3.0 * X;
    V = Y;
    W = 1.0 / 2.0 * (-X + 3.0 * Y + Z);
    s = U + V + W;
    *u_out = U / s;
    *v_out = V / s;
}

static alwan_status alwan__planckian_table_create(alwan_planckian_table **out, alwan_observer_type observer,
                                                  double start, double end, double spacing, alwan_ctx *ctx) {
    alwan_spd_f64 xb, yb, zb;
    alwan_planckian_table *table;
    size_t n, i;
    alwan_status st;
    if (!out) return ALWAN_E_INVALID;
    *out = NULL;
    if ((int)observer < 0 || (int)observer > (int)ALWAN_OBSERVER_WRIGHT_GUILD_1931) return ALWAN_E_INVALID;
    if (start == 0.0) start = ALWAN__OHNO_START;
    if (end == 0.0) end = ALWAN__OHNO_END;
    if (spacing == 0.0) spacing = ALWAN__OHNO_SPACING;
    if (!(start > 0.0) || !(end > start + 2.0) || !(spacing > 1.0)) return ALWAN_E_INVALID;

    n = alwan__ohno_temperatures(NULL, start, end, spacing);
    table = (alwan_planckian_table *)ALWAN_ALLOC(sizeof(*table), sizeof(double));
    if (!table) return ALWAN_E_NOMEM;
    memset(table, 0, sizeof(*table));
    table->t = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    table->u = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    table->v = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    if (!table->t || !table->u || !table->v) {
        alwan_planckian_table_destroy(table, ctx);
        return ALWAN_E_NOMEM;
    }
    table->count = n;
    table->observer = observer;
    alwan__ohno_temperatures(table->t, start, end, spacing);

    st = alwan_spd_observer_f64(&xb, &yb, &zb, observer, ctx);
    if (st != ALWAN_OK) {
        alwan_planckian_table_destroy(table, ctx);
        return st;
    }
    for (i = 0; i < n; i++) {
        alwan__planck_uv(&table->u[i], &table->v[i], table->t[i], &xb, &yb, &zb);
    }
    alwan_spd_destroy_f64(&xb, ctx);
    alwan_spd_destroy_f64(&yb, ctx);
    alwan_spd_destroy_f64(&zb, ctx);
    *out = table;
    return ALWAN_OK;
}

alwan_status alwan_planckian_table_create_f64(alwan_planckian_table **out, alwan_observer_type observer,
                                              alwan_f64 start, alwan_f64 end, alwan_f64 spacing, alwan_ctx *ctx) {
    return alwan__planckian_table_create(out, observer, (double)start, (double)end, (double)spacing, ctx);
}

alwan_status alwan_planckian_table_create_f32(alwan_planckian_table **out, alwan_observer_type observer,
                                              alwan_f32 start, alwan_f32 end, alwan_f32 spacing, alwan_ctx *ctx) {
    return alwan__planckian_table_create(out, observer, (double)start, (double)end, (double)spacing, ctx);
}

void alwan_planckian_table_destroy(alwan_planckian_table *table, alwan_ctx *ctx) {
    (void)ctx;
    if (!table) return;
    ALWAN_FREE(table->t);
    ALWAN_FREE(table->u);
    ALWAN_FREE(table->v);
    ALWAN_FREE(table);
}

size_t alwan_planckian_table_size(alwan_planckian_table const *table) {
    return table ? table->count : 0;
}

/* Ohno 2013: the nearest entry and its two neighbours give a triangle whose apex is the
 * answer; past |Duv| = 0.002 the three distances are fitted with a parabola instead,
 * which is where colour-science changes over. */
static alwan_status alwan__uv_to_cct_ohno2013(double *cct_out, double *duv_out, double u, double v,
                                              alwan_planckian_table const *table) {
    size_t i, best = 0;
    double best_d = 0.0;
    double Tip, uip, vip, dip, Ti, di, Tin, uin, vin, din;
    double l, x, T_t, vtx, sign, duv_t, X, a, b, c, T_p, duv_p;
    if (!cct_out || !table || table->count < 3) return ALWAN_E_INVALID;
    for (i = 0; i < table->count; i++) {
        double const du = table->u[i] - u, dv = table->v[i] - v;
        double const d = ALWAN_SQRT(du * du + dv * dv);
        if (i == 0 || d < best_d) {
            best_d = d;
            best = i;
        }
    }
    /* Neither solution has the neighbours it needs at an end of the table. */
    if (best == 0 || best == table->count - 1) return ALWAN_E_RANGE;

    Tip = table->t[best - 1];
    uip = table->u[best - 1];
    vip = table->v[best - 1];
    Ti = table->t[best];
    Tin = table->t[best + 1];
    uin = table->u[best + 1];
    vin = table->v[best + 1];
    {
        double const dup = uip - u, dvp = vip - v;
        double const dun = uin - u, dvn = vin - v;
        dip = ALWAN_SQRT(dup * dup + dvp * dvp);
        di = best_d;
        din = ALWAN_SQRT(dun * dun + dvn * dvn);
    }

    /* Triangular solution. */
    l = ALWAN_SQRT((uin - uip) * (uin - uip) + (vin - vip) * (vin - vip));
    if (!(l > 0.0)) return ALWAN_E_DIVZERO;
    x = (dip * dip - din * din + l * l) / (2.0 * l);
    T_t = Tip + (Tin - Tip) * (x / l);
    vtx = vip + (vin - vip) * (x / l);
    sign = v - vtx < 0.0 ? -1.0 : (v - vtx > 0.0 ? 1.0 : 0.0);
    {
        double const r = dip * dip - x * x;
        duv_t = (r > 0.0 ? ALWAN_SQRT(r) : 0.0) * sign;
    }

    if (ALWAN_ABS(duv_t) < 0.002) {
        *cct_out = T_t;
        if (duv_out) *duv_out = duv_t;
        return ALWAN_OK;
    }

    /* Parabolic solution. */
    X = (Tin - Ti) * (Tip - Tin) * (Ti - Tip);
    if (X == 0.0) return ALWAN_E_DIVZERO;
    a = (Tip * (din - di) + Ti * (dip - din) + Tin * (di - dip)) / X;
    b = -(Tip * Tip * (din - di) + Ti * Ti * (dip - din) + Tin * Tin * (di - dip)) / X;
    c = -(dip * (Tin - Ti) * Ti * Tin + di * (Tip - Tin) * Tip * Tin + din * (Ti - Tip) * Tip * Ti) / X;
    if (a == 0.0) return ALWAN_E_DIVZERO;
    T_p = -b / (2.0 * a);
    duv_p = (a * T_p * T_p + b * T_p + c) * sign;
    *cct_out = T_p;
    if (duv_out) *duv_out = duv_p;
    return ALWAN_OK;
}

alwan_status alwan_uv_to_cct_ohno2013_f64(alwan_f64 *cct_out, alwan_f64 *duv_out, alwan_vec2_f64 const *uv,
                                          alwan_planckian_table const *table) {
    double cct = 0.0, duv = 0.0;
    alwan_status st;
    if (!cct_out || !uv) return ALWAN_E_INVALID;
    st = alwan__uv_to_cct_ohno2013(&cct, &duv, (double)uv->v[0], (double)uv->v[1], table);
    if (st != ALWAN_OK) return st;
    *cct_out = cct;
    if (duv_out) *duv_out = duv;
    return ALWAN_OK;
}

alwan_status alwan_uv_to_cct_ohno2013_f32(alwan_f32 *cct_out, alwan_f32 *duv_out, alwan_vec2_f32 const *uv,
                                          alwan_planckian_table const *table) {
    double cct = 0.0, duv = 0.0;
    alwan_status st;
    if (!cct_out || !uv) return ALWAN_E_INVALID;
    st = alwan__uv_to_cct_ohno2013(&cct, &duv, (double)uv->v[0], (double)uv->v[1], table);
    if (st != ALWAN_OK) return st;
    *cct_out = (alwan_f32)cct;
    if (duv_out) *duv_out = (alwan_f32)duv;
    return ALWAN_OK;
}
