/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * CMCCAT2000 as a model (Li, Luo, Rigg and Hunt, "CMC 2000 chromatic adaptation
 * transform: CMCCAT2000", Color Research and Application 27(1), 2002): the degree of
 * adaptation from the two adapting luminances and the surround,
 *
 *   D = F (0.08 log10((L_A1 + L_A2) / 2) + 0.76 - 0.45 (L_A1 - L_A2) / (L_A1 + L_A2)),
 *
 * clipped to [0, 1], and RGB_c = RGB (D (Y_w / Y_wr) (RGB_wr / RGB_w) + 1 - D) in the
 * CMCCAT2000 cone space; the inverse divides. As colour's chromatic_adaptation_CMCCAT2000.
 *
 * Corresponding chromaticities: Breneman's 1987 experiments, in which observers matched a
 * colour seen under one adapting field to one remembered under another, run through a
 * chromatic adaptation model to score it: for each sample, the u'v' it had under the test
 * field, the u'v' the observers chose under the reference field, and the u'v' the model
 * predicts. As colour's corresponding_chromaticities_prediction_*, including its scales:
 * the adapting whites at Y = 1, each sample at its luminance factor times the primaries'
 * luminance, CIE 1994 on the Y = 100 scale with Y_o = 30 and the two illuminances equal to
 * that luminance, CMCCAT2000 with L_A1 = L_A2 = it and the average surround, Zhai 2018 fully
 * adapted to an equal-energy baseline. The data are gendata's
 * data/corresponding/breneman1987.csv, exported from colour.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static double const alwan_breneman_data[] = {
#include "../data/corresponding/breneman1987.csv"
};
ALWAN_DIAG_POP

static char const *const alwan_breneman_names[12] = { "Gray", "Red", "Skin", "Orange", "Brown", "Yellow",
                                                       "Foliage", "Green", "Blue-green", "Blue", "Sky", "Purple" };

static void alwan_cc_inv3(double *out, double const *m) {
    double const det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
    out[0] = (m[4] * m[8] - m[5] * m[7]) / det, out[1] = (m[2] * m[7] - m[1] * m[8]) / det, out[2] = (m[1] * m[5] - m[2] * m[4]) / det;
    out[3] = (m[5] * m[6] - m[3] * m[8]) / det, out[4] = (m[0] * m[8] - m[2] * m[6]) / det, out[5] = (m[2] * m[3] - m[0] * m[5]) / det;
    out[6] = (m[3] * m[7] - m[4] * m[6]) / det, out[7] = (m[1] * m[6] - m[0] * m[7]) / det, out[8] = (m[0] * m[4] - m[1] * m[3]) / det;
}

static void alwan_cc_mul(double *o, double const *m, double const *v) {
    o[0] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2];
    o[1] = m[3] * v[0] + m[4] * v[1] + m[5] * v[2];
    o[2] = m[6] * v[0] + m[7] * v[1] + m[8] * v[2];
}

static alwan_status alwan_cc_cmccat2000(double *out, double const *in, double const *w, double const *wr, double L_A1, double L_A2, double F,
                                        int inverse) {
    double const *M = g_cat_cmccat2000_f64;
    double Mi[9], rgb[3], rgb_w[3], rgb_wr[3], D, a, c[3];
    int i;
    if (!(L_A1 > 0.0) || !(L_A2 > 0.0) || !(F >= 0.0) || !(w[1] != 0.0) || !(wr[1] != 0.0)) return ALWAN_E_RANGE;
    alwan_cc_inv3(Mi, M);
    alwan_cc_mul(rgb, M, in);
    alwan_cc_mul(rgb_w, M, w);
    alwan_cc_mul(rgb_wr, M, wr);
    D = F * (0.08 * ALWAN_LOG10_F64(0.5 * (L_A1 + L_A2)) + 0.76 - 0.45 * (L_A1 - L_A2) / (L_A1 + L_A2));
    D = D < 0.0 ? 0.0 : D > 1.0 ? 1.0 : D;
    a = D * w[1] / wr[1];
    for (i = 0; i < 3; i++) {
        double const g = a * (rgb_wr[i] / rgb_w[i]) + 1.0 - D;
        c[i] = inverse ? rgb[i] / g : rgb[i] * g;
    }
    alwan_cc_mul(out, Mi, c);
    return ALWAN_OK;
}

alwan_status alwan_cat_cmccat2000_f64(alwan_xyz_f64 *xyz_out, alwan_xyz_f64 const *xyz_in, alwan_xyz_f64 const *xyz_w, alwan_xyz_f64 const *xyz_wr,
                                      alwan_f64 L_A1, alwan_f64 L_A2, alwan_f64 F, int inverse) {
    double in[3], w[3], wr[3], out[3];
    alwan_status st;
    if (!xyz_out || !xyz_in || !xyz_w || !xyz_wr) return ALWAN_E_INVALID;
    in[0] = xyz_in->x, in[1] = xyz_in->y, in[2] = xyz_in->z;
    w[0] = xyz_w->x, w[1] = xyz_w->y, w[2] = xyz_w->z;
    wr[0] = xyz_wr->x, wr[1] = xyz_wr->y, wr[2] = xyz_wr->z;
    st = alwan_cc_cmccat2000(out, in, w, wr, L_A1, L_A2, F, inverse);
    if (st != ALWAN_OK) return st;
    xyz_out->x = out[0], xyz_out->y = out[1], xyz_out->z = out[2];
    return ALWAN_OK;
}

#if ALWAN_WITH_F32
alwan_status alwan_cat_cmccat2000_f32(alwan_xyz_f32 *xyz_out, alwan_xyz_f32 const *xyz_in, alwan_xyz_f32 const *xyz_w, alwan_xyz_f32 const *xyz_wr,
                                      alwan_f32 L_A1, alwan_f32 L_A2, alwan_f32 F, int inverse) {
    double in[3], w[3], wr[3], out[3];
    alwan_status st;
    if (!xyz_out || !xyz_in || !xyz_w || !xyz_wr) return ALWAN_E_INVALID;
    in[0] = xyz_in->x, in[1] = xyz_in->y, in[2] = xyz_in->z;
    w[0] = xyz_w->x, w[1] = xyz_w->y, w[2] = xyz_w->z;
    wr[0] = xyz_wr->x, wr[1] = xyz_wr->y, wr[2] = xyz_wr->z;
    st = alwan_cc_cmccat2000(out, in, w, wr, L_A1, L_A2, F, inverse);
    if (st != ALWAN_OK) return st;
    xyz_out->x = (alwan_f32)out[0], xyz_out->y = (alwan_f32)out[1], xyz_out->z = (alwan_f32)out[2];
    return ALWAN_OK;
}
#endif

/* CIE 1976 u'v' */
static void alwan_cc_uv(double *uv, double const *xyz) {
    double const d = xyz[0] + 15.0 * xyz[1] + 3.0 * xyz[2];
    uv[0] = 4.0 * xyz[0] / d;
    uv[1] = 9.0 * xyz[1] / d;
}

/* xy from u'v', then XYZ at luminance Y */
static void alwan_cc_uv_to_xyz(double *xyz, double u, double v, double Y) {
    double const d = 6.0 * u - 16.0 * v + 12.0, x = 9.0 * u / d, y = 4.0 * v / d;
    xyz[0] = x * Y / y;
    xyz[1] = Y;
    xyz[2] = (1.0 - x - y) * Y / y;
}

alwan_status alwan_corresponding_chromaticities_breneman1987(alwan_corresponding_prediction *out, size_t capacity, size_t *count, int experiment,
                                                            alwan_corresponding_model model, alwan_cat_method transform) {
    double const *lum = alwan_breneman_data, *e = alwan_breneman_data + 13;
    double Y, xyz_t[3], xyz_r[3];
    size_t S, i, k;
    if (!out || !count) return ALWAN_E_INVALID;
    *count = 0;
    if (experiment < 1 || experiment > 12 || (unsigned)model > (unsigned)ALWAN_CORRESPONDING_ZHAI2018) return ALWAN_E_INVALID;
    if (model == ALWAN_CORRESPONDING_ZHAI2018 && transform != ALWAN_CAT_CAT02 && transform != ALWAN_CAT_CAT16) return ALWAN_E_INVALID;
    /* find the experiment */
    for (k = 1; k < (size_t)experiment; k++) e += 3 + 4 + 4 * (size_t)e[2];
    Y = e[1];
    S = (size_t)e[2];
    /* no luminance for its primaries (5, 7, 10), or more samples than luminance factors (9) */
    if (!(Y > 0.0) || S > 12) return ALWAN_E_NODATA;
    if (capacity < S) return ALWAN_E_RANGE;
    alwan_cc_uv_to_xyz(xyz_t, e[3], e[4], 1.0);
    alwan_cc_uv_to_xyz(xyz_r, e[5], e[6], 1.0);
    for (i = 0; i < S; i++) {
        double const *s = e + 7 + 4 * i;
        double ct[3], cr[3], p[3];
        alwan_status st = ALWAN_OK;
        alwan_cc_uv_to_xyz(ct, s[0], s[1], lum[i] * Y);
        alwan_cc_uv_to_xyz(cr, s[2], s[3], lum[i] * Y);
        if (model == ALWAN_CORRESPONDING_VON_KRIES) {
            alwan_xyz_f64 wt, wr;
            alwan_mat3x3_f64 m;
            wt.x = xyz_t[0], wt.y = xyz_t[1], wt.z = xyz_t[2];
            wr.x = xyz_r[0], wr.y = xyz_r[1], wr.z = xyz_r[2];
            st = alwan_cat_matrix_f64(&m, &wt, &wr, transform);
            if (st == ALWAN_OK) alwan_cc_mul(p, m.m, ct);
        } else if (model == ALWAN_CORRESPONDING_CIE1994) {
            alwan_xyz_f64 a, b;
            alwan_vec2_f64 o1, o2;
            double const s1 = xyz_t[0] + xyz_t[1] + xyz_t[2], s2 = xyz_r[0] + xyz_r[1] + xyz_r[2];
            o1.v[0] = xyz_t[0] / s1, o1.v[1] = xyz_t[1] / s1;
            o2.v[0] = xyz_r[0] / s2, o2.v[1] = xyz_r[1] / s2;
            a.x = ct[0] * 100.0, a.y = ct[1] * 100.0, a.z = ct[2] * 100.0;
            st = alwan_cat_cie1994_f64(&b, &a, &o1, &o2, 30.0, Y, Y, 1.0);
            p[0] = b.x, p[1] = b.y, p[2] = b.z;
        } else if (model == ALWAN_CORRESPONDING_CMCCAT2000) {
            st = alwan_cc_cmccat2000(p, ct, xyz_t, xyz_r, Y, Y, 1.0, 0);
        } else {
            alwan_xyz_f64 a, b, ws, wd;
            a.x = ct[0] * 100.0, a.y = ct[1] * 100.0, a.z = ct[2] * 100.0;
            ws.x = xyz_t[0] * 100.0, ws.y = xyz_t[1] * 100.0, ws.z = xyz_t[2] * 100.0;
            wd.x = xyz_r[0] * 100.0, wd.y = xyz_r[1] * 100.0, wd.z = xyz_r[2] * 100.0;
            st = alwan_cat_zhai2018_f64(&b, &a, &ws, &wd, 1.0, 1.0, NULL, transform);
            p[0] = b.x, p[1] = b.y, p[2] = b.z;
        }
        if (st != ALWAN_OK) return st;
        out[i].name = alwan_breneman_names[i];
        alwan_cc_uv(out[i].uv_t, ct);
        alwan_cc_uv(out[i].uv_m, cr);
        alwan_cc_uv(out[i].uv_p, p);
    }
    *count = S;
    return ALWAN_OK;
}
