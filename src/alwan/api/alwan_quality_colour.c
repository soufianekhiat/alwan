/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The NIST Colour Quality Scale and the CIE 2017 / ANSI IES TM-30-18 colour fidelity index,
 * computed the way colour-science 0.4.7 computes them (colour.quality.cqs and
 * colour.quality.cfi2017 / tm3018), so that the two agree to rounding (suite 257).
 *
 * What that means in practice, and what alwan did before 2026-09-25:
 *   - the spectra live on colour's grids: 360-780 nm at 1 nm for CQS, 380-780 nm at the
 *     test SPD's own interval for CIE 2017, never 360-830 nm at 5 nm;
 *   - the test SPD is read linearly between its samples, held at its end values (CQS) or
 *     zero (CIE 2017) outside them, as colour's LinearInterpolator and extrapolators do;
 *   - tristimulus values are colour's "Integration" sums, not the trapezoid rule;
 *   - the CCT is Ohno 2013 on a table built from the CIE 1931 CMFs over 360-780 nm (to
 *     100000 K for CQS, 25000 K for CIE 2017), not Robertson;
 *   - CIE daylight takes M1 and M2 rounded to three decimals (CIE 15), and for CIE 2017 at
 *     1 nm its basis comes Sprague-interpolated, which is how colour reshapes it;
 *   - the sample reflectances are colour's own tables on those grids (data/quality/).
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../data/alwan_data_tables.h"   /* ALWAN_TABLE_VS_REFLECTANCE, ALWAN_TABLE_CES_REFLECTANCE */
#include <math.h>
#include <string.h>

#define Q_CQS_START 360
#define Q_CQS_N 421           /* 360-780 nm at 1 nm */
#define Q_CFI_START 380
#define Q_CFI_N1 401          /* 380-780 nm at 1 nm */
#define Q_CFI_N5 81           /* 380-780 nm at 5 nm */
#define Q_CFI_SAMPLES 99
#define Q_BASIS5_START 300
#define Q_BASIS5_N 107        /* 300-830 nm at 5 nm */
#define Q_MAX_N 421

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#if ALWAN_TABLE_VS_REFLECTANCE
static double const q_vs_9_0[ALWAN_CQS_SAMPLES * Q_CQS_N] = {
#include "../data/quality/cqs_vs_9_0_360_780_1nm.csv"
};
static double const q_vs_7_4[ALWAN_CQS_SAMPLES * Q_CQS_N] = {
#include "../data/quality/cqs_vs_7_4_360_780_1nm.csv"
};
#endif
#if ALWAN_TABLE_CES_REFLECTANCE
static double const q_tcs_1nm[Q_CFI_SAMPLES * Q_CFI_N1] = {
#include "../data/quality/tcs_cfi2017_380_780_1nm.csv"
};
static double const q_tcs_5nm[Q_CFI_SAMPLES * Q_CFI_N5] = {
#include "../data/quality/tcs_cfi2017_380_780_5nm.csv"
};
static double const q_basis_1nm[3 * Q_CFI_N1] = {
#include "../data/quality/daylight_basis_380_780_1nm.csv"
};
#endif
#if ALWAN_TABLE_VS_REFLECTANCE || ALWAN_TABLE_CES_REFLECTANCE
static double const q_basis_5nm[3 * Q_BASIS5_N] = {
#include "../data/quality/daylight_basis_300_830_5nm.csv"
};
#endif
ALWAN_DIAG_POP

#if ALWAN_TABLE_VS_REFLECTANCE || ALWAN_TABLE_CES_REFLECTANCE

/* The CMFs of one observer on a grid: n wavelengths from start, every step nm, read off
 * alwan's 1 nm tables (360-830 nm), which equal colour's. */
typedef struct {
    double x[Q_MAX_N], y[Q_MAX_N], z[Q_MAX_N];
} q_cmfs;

static alwan_status q_load_cmfs(q_cmfs *c, alwan_observer_type observer, int start, int step, int n, alwan_ctx *ctx) {
    alwan_spd_f64 xb, yb, zb;
    int i;
    alwan_status const st = alwan_spd_observer_f64(&xb, &yb, &zb, observer, ctx);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < n; i++) {
        size_t const at = (size_t)(start - 360 + i * step);
        if (at >= xb.count) {
            alwan_spd_destroy_f64(&xb, ctx);
            alwan_spd_destroy_f64(&yb, ctx);
            alwan_spd_destroy_f64(&zb, ctx);
            return ALWAN_E_RANGE;
        }
        c->x[i] = xb.values[at];
        c->y[i] = yb.values[at];
        c->z[i] = zb.values[at];
    }
    alwan_spd_destroy_f64(&xb, ctx);
    alwan_spd_destroy_f64(&yb, ctx);
    alwan_spd_destroy_f64(&zb, ctx);
    return ALWAN_OK;
}

/* numpy.interp over the SPD's own samples at n grid wavelengths start, start + step, ...:
 * inside its range linearly, as slope * (x - x_j) + y_j; outside it the end value (edge
 * non-zero, CQS's constant extrapolation) or 0 (CIE 2017's). The grid rises, so one cursor
 * walks the samples; no subscript comes from a float. */
static void q_read_spd(double *out, alwan_spd_f64 const *spd, double start, double step, int n, int edge) {
    double const w0 = (double)spd->wavelength_min, w1 = (double)spd->wavelength_max;
    double const ds = (w1 - w0) / (double)(spd->count - 1);
    size_t j = 0;
    int i;
    for (i = 0; i < n; i++) {
        double const x = start + step * (double)i;
        if (x < w0) {
            out[i] = edge ? (double)spd->values[0] : 0.0;
        } else if (x > w1) {
            out[i] = edge ? (double)spd->values[spd->count - 1] : 0.0;
        } else if (x == w1) {
            out[i] = (double)spd->values[spd->count - 1];
        } else {
            double xj, xk, slope;
            while (j + 2 < spd->count && w0 + ds * (double)(j + 1) <= x) j++;
            xj = w0 + ds * (double)j;
            xk = w0 + ds * (double)(j + 1);
            slope = ((double)spd->values[j + 1] - (double)spd->values[j]) / (xk - xj);
            out[i] = slope * (x - xj) + (double)spd->values[j];
        }
    }
}

/* colour's planck_law times 1e-9, as sd_blackbody takes it. */
static void q_planck(double *out, double start, double step, int n, double T) {
    int i;
    for (i = 0; i < n; i++) {
        double const l = (start + step * (double)i) * 1e-9;
        double const d = 1.0 / (ALWAN_EXP_F64(0.014388 / (l * T)) - 1.0);
        out[i] = ((3.741771e-16 / (l * l * l * l * l)) / ALWAN_PI) * d * 1e-9;
    }
}

/* CCT_to_xy_CIE_D, computed at any CCT as colour does (it warns outside 4000-25000 K). */
static void q_daylight_xy(double *x, double *y, double T) {
    double const T2 = T * T, T3 = T2 * T;
    if (T <= 7000.0) {
        *x = -4.607e9 / T3 + 2.9678e6 / T2 + 0.09911e3 / T + 0.244063;
    } else {
        *x = -2.0064e9 / T3 + 1.9018e6 / T2 + 0.24748e3 / T + 0.23704;
    }
    *y = -3.000 * *x * *x + 2.870 * *x - 0.275;
}

/* sd_CIE_illuminant_D_series's M1 and M2, rounded to three decimals as numpy.around does. */
static void q_daylight_m(double *m1, double *m2, double x, double y) {
    double const M = 0.0241 + 0.2562 * x - 0.7341 * y;
    *m1 = nearbyint(((-1.3515 - 1.7703 * x + 5.9114 * y) / M) * 1000.0) / 1000.0;
    *m2 = nearbyint(((0.0300 - 31.4424 * x + 30.0717 * y) / M) * 1000.0) / 1000.0;
}

/* colour's sd_to_XYZ_integration against an illuminant S (NULL: equal energy), d_w the
 * step: k = 100 / (sum ybar S d_w), XYZ = k sum(R S cmf) d_w. R NULL reads as 1. */
static void q_integrate(double xyz[3], double const *R, double const *S, q_cmfs const *c, int n, double dw) {
    double sy = 0.0, X = 0.0, Y = 0.0, Z = 0.0, k;
    int i;
    for (i = 0; i < n; i++) {
        double const s = S ? S[i] : 1.0;
        double const rs = (R ? R[i] : 1.0) * s;
        sy += c->y[i] * s;
        X += rs * c->x[i];
        Y += rs * c->y[i];
        Z += rs * c->z[i];
    }
    k = 100.0 / (sy * dw);
    xyz[0] = k * X * dw;
    xyz[1] = k * Y * dw;
    xyz[2] = k * Z * dw;
}

static int q_uv(double uv[2], double const XYZ[3]) {
    double const U = 2.0 / 3.0 * XYZ[0], V = XYZ[1], W = 0.5 * (-XYZ[0] + 3.0 * XYZ[1] + XYZ[2]);
    double const s = U + V + W;
    if (!(s != 0.0)) return 0;
    uv[0] = U / s;
    uv[1] = V / s;
    return 1;
}

static alwan_status q_cct(double *cct, double *duv, double const XYZ[3], double end, alwan_ctx *ctx) {
    alwan_planckian_table *table = NULL;
    alwan_vec2_f64 uv;
    alwan_f64 t = 0.0, d = 0.0;
    double u[2];
    alwan_status st;
    if (!q_uv(u, XYZ)) return ALWAN_E_RANGE;
    st = alwan__planckian_table_create_range(&table, ALWAN_OBSERVER_CIE_1931_2DEG, 1000.0, end, 1.001, 780.0, ctx);
    if (st != ALWAN_OK) return st;
    uv.v[0] = u[0];
    uv.v[1] = u[1];
    st = alwan_uv_to_cct_ohno2013_f64(&t, &d, &uv, table);
    alwan_planckian_table_destroy(table, ctx);
    *cct = t;
    *duv = d;
    return st;
}

/* colour's XYZ_to_Lab against a white given as xy (Y 1), and its f(t). */
static double q_lab_f(double t) {
    return t > (24.0 / 116.0) * (24.0 / 116.0) * (24.0 / 116.0) ? ALWAN_CBRT_F64(t) : (841.0 / 108.0) * t + 16.0 / 116.0;
}

static void q_lab(double lab[3], double const XYZ[3], double const white[3]) {
    double const s = white[0] + white[1] + white[2];
    double const x = white[0] / s, y = white[1] / s;
    double const Xn = x / y, Yn = 1.0, Zn = (1.0 - x - y) / y;
    double const fx = q_lab_f(XYZ[0] / Xn), fy = q_lab_f(XYZ[1] / Yn), fz = q_lab_f(XYZ[2] / Zn);
    lab[0] = 116.0 * fy - 16.0;
    lab[1] = 500.0 * (fx - fy);
    lab[2] = 200.0 * (fy - fz);
}

/* The von Kries CMCCAT2000 matrix from one white to another, applied. */
static alwan_status q_vonkries(alwan_mat3x3_f64 *m, double const src[3], double const dst[3]) {
    alwan_xyz_f64 a, b;
    a.x = src[0]; a.y = src[1]; a.z = src[2];
    b.x = dst[0]; b.y = dst[1]; b.z = dst[2];
    return alwan_cat_matrix_f64(m, &a, &b, ALWAN_CAT_CMCCAT2000);
}

static void q_apply(double out[3], alwan_mat3x3_f64 const *m, double const in[3]) {
    out[0] = m->m[0] * in[0] + m->m[1] * in[1] + m->m[2] * in[2];
    out[1] = m->m[3] * in[0] + m->m[4] * in[1] + m->m[5] * in[2];
    out[2] = m->m[6] * in[0] + m->m[7] * in[1] + m->m[8] * in[2];
}

/* 10 ln(1 + exp((100 - s dE) / 10)) x f: CQS's scale_conversion and CIE 2017's
 * delta_E_to_R_f (s 6.73, f 1). */
static double q_scale(double dE, double f, double s) {
    return 10.0 * ALWAN_LN_F64(1.0 + ALWAN_EXP_F64((100.0 - s * dE) / 10.0)) * f;
}

static int q_spd_ok(alwan_spd_f64 const *s) {
    size_t i;
    if (!s->values || s->count < 2 || !(s->wavelength_max > s->wavelength_min)) return 0;
    for (i = 0; i < s->count; i++) {
        if (!isfinite((double)s->values[i])) return 0;
    }
    return 1;
}

#endif /* either table */

/* ----------------------------------------------------------------
 * NIST CQS
 * ---------------------------------------------------------------- */

#if ALWAN_TABLE_VS_REFLECTANCE
/* colour's gamut_area: the triangles from the origin to consecutive samples' (a*, b*), each
 * by Heron's formula. */
static double q_gamut_area(double const (*lab)[3]) {
    double sum = 0.0;
    int i;
    for (i = 0; i < ALWAN_CQS_SAMPLES; i++) {
        double const *p = lab[i], *q = lab[(i + 1) % ALWAN_CQS_SAMPLES];
        double const A = ALWAN_SQRT_F64(p[1] * p[1] + p[2] * p[2]);
        double const B = ALWAN_SQRT_F64(q[1] * q[1] + q[2] * q[2]);
        double const Cc = ALWAN_SQRT_F64((q[1] - p[1]) * (q[1] - p[1]) + (q[2] - p[2]) * (q[2] - p[2]));
        double const t = (A + B + Cc) / 2.0;
        sum += ALWAN_SQRT_F64(t * (t - A) * (t - B) * (t - Cc));
    }
    return sum;
}
#endif

alwan_status alwan__cqs_compute(alwan_cqs_f64 *spec, alwan_spd_f64 const *test_spd, alwan_cqs_version version, alwan_ctx *ctx) {
    if (!spec || !test_spd) return ALWAN_E_INVALID;
    if (version != ALWAN_CQS_9_0 && version != ALWAN_CQS_7_4) return ALWAN_E_INVALID;
    if (!q_spd_ok(test_spd)) return ALWAN_E_INVALID;
#if !ALWAN_TABLE_VS_REFLECTANCE
    (void)ctx;
    return ALWAN_E_NODATA;
#else
    {
        double S[Q_CQS_N], Sr[Q_CQS_N], X[3], Xt[3], Xr[3];
        double lab_t[ALWAN_CQS_SAMPLES][3], lab_r[ALWAN_CQS_SAMPLES][3], xyz_ref[ALWAN_CQS_SAMPLES][3];
        double dc[ALWAN_CQS_SAMPLES], de[ALWAN_CQS_SAMPLES], dep[ALWAN_CQS_SAMPLES];
        double const *vs = version == ALWAN_CQS_9_0 ? q_vs_9_0 : q_vs_7_4;
        double cct = 0.0, duv = 0.0, f, s, s_f, rmsp = 0.0, rms = 0.0, pdc = 0.0, gt, gr;
        alwan_mat3x3_f64 cat;
        q_cmfs cmf;
        alwan_status st;
        int i, k;

        st = q_load_cmfs(&cmf, ALWAN_OBSERVER_CIE_1931_2DEG, Q_CQS_START, 1, Q_CQS_N, ctx);
        if (st != ALWAN_OK) return st;
        q_read_spd(S, test_spd, (double)Q_CQS_START, 1.0, Q_CQS_N, 1);
        q_integrate(X, S, NULL, &cmf, Q_CQS_N, 1.0);
        if (!(X[1] > 0.0)) return ALWAN_E_RANGE;
        st = q_cct(&cct, &duv, X, 100000.0, ctx);
        if (st != ALWAN_OK) return st;

        if (cct < 5000.0) {
            q_planck(Sr, (double)Q_CQS_START, 1.0, Q_CQS_N, cct);
        } else {
            /* sd_CIE_illuminant_D_series on its 300-830 nm 5 nm basis, then aligned to the
             * 1 nm grid by its LinearInterpolator. */
            double x, y, m1, m2, s5[Q_BASIS5_N];
            alwan_spd_f64 day;
            q_daylight_xy(&x, &y, cct);
            q_daylight_m(&m1, &m2, x, y);
            for (i = 0; i < Q_BASIS5_N; i++) {
                s5[i] = q_basis_5nm[i] + m1 * q_basis_5nm[Q_BASIS5_N + i] + m2 * q_basis_5nm[2 * Q_BASIS5_N + i];
            }
            day.values = s5;
            day.wavelength_min = (alwan_f64)Q_BASIS5_START;
            day.wavelength_max = (alwan_f64)(Q_BASIS5_START + 5 * (Q_BASIS5_N - 1));
            day.count = Q_BASIS5_N;
            q_read_spd(Sr, &day, (double)Q_CQS_START, 1.0, Q_CQS_N, 1);
        }

        /* The whites at Y = 1. */
        q_integrate(Xt, S, NULL, &cmf, Q_CQS_N, 1.0);
        q_integrate(Xr, Sr, NULL, &cmf, Q_CQS_N, 1.0);
        if (!(Xt[1] > 0.0) || !(Xr[1] > 0.0)) return ALWAN_E_RANGE;
        {
            double const yt = Xt[1], yr = Xr[1];
            for (k = 0; k < 3; k++) {
                Xt[k] /= yt;
                Xr[k] /= yr;
            }
        }
        st = q_vonkries(&cat, Xt, Xr);
        if (st != ALWAN_OK) return st;

        for (i = 0; i < ALWAN_CQS_SAMPLES; i++) {
            double const *R = vs + (size_t)i * Q_CQS_N;
            double xt[3], xr[3], xa[3], ct, cr;
            q_integrate(xt, R, S, &cmf, Q_CQS_N, 1.0);
            q_integrate(xr, R, Sr, &cmf, Q_CQS_N, 1.0);
            for (k = 0; k < 3; k++) {
                xt[k] /= 100.0;   /* domain_range_scale("1") */
                xr[k] /= 100.0;
                xyz_ref[i][k] = xr[k];
            }
            q_apply(xa, &cat, xt);
            q_lab(lab_t[i], xa, Xr);
            q_lab(lab_r[i], xr, Xr);
            ct = ALWAN_SQRT_F64(lab_t[i][1] * lab_t[i][1] + lab_t[i][2] * lab_t[i][2]);
            cr = ALWAN_SQRT_F64(lab_r[i][1] * lab_r[i][1] + lab_r[i][2] * lab_r[i][2]);
            dc[i] = ct - cr;
            de[i] = ALWAN_SQRT_F64((lab_t[i][0] - lab_r[i][0]) * (lab_t[i][0] - lab_r[i][0]) +
                                   (lab_t[i][1] - lab_r[i][1]) * (lab_t[i][1] - lab_r[i][1]) +
                                   (lab_t[i][2] - lab_r[i][2]) * (lab_t[i][2] - lab_r[i][2]));
            dep[i] = dc[i] > 0.0 ? ALWAN_SQRT_F64(de[i] * de[i] - dc[i] * dc[i]) : de[i];
        }

        if (version == ALWAN_CQS_9_0) {
            f = 1.0;
            s = 3.2;
            s_f = 2.93 * 1.0343;
        } else {
            /* CCT_factor: the reference samples adapted to D65 (colour's CIE 1931 xy
             * 0.3127, 0.3290), their gamut area against 8210, at most 1. */
            double W[3], labw[ALWAN_CQS_SAMPLES][3], ratio;
            alwan_mat3x3_f64 cw;
            W[0] = 0.3127 / 0.3290;
            W[1] = 1.0;
            W[2] = (1.0 - 0.3127 - 0.3290) / 0.3290;
            st = q_vonkries(&cw, Xr, W);
            if (st != ALWAN_OK) return st;
            for (i = 0; i < ALWAN_CQS_SAMPLES; i++) {
                double xa[3];
                q_apply(xa, &cw, xyz_ref[i]);
                q_lab(labw[i], xa, W);
            }
            ratio = q_gamut_area((double const (*)[3])labw) / 8210.0;
            f = ratio < 1.0 ? ratio : 1.0;
            s = 3.104;
            s_f = 2.928;
        }

        for (i = 0; i < ALWAN_CQS_SAMPLES; i++) {
            rmsp += dep[i] * dep[i];
            rms += de[i] * de[i];
            pdc += dc[i] > 0.0 ? dc[i] : 0.0;
        }
        rmsp = ALWAN_SQRT_F64(1.0 / (double)ALWAN_CQS_SAMPLES * rmsp);
        rms = ALWAN_SQRT_F64(1.0 / (double)ALWAN_CQS_SAMPLES * rms);
        pdc /= (double)ALWAN_CQS_SAMPLES;
        gt = q_gamut_area((double const (*)[3])lab_t);
        gr = q_gamut_area((double const (*)[3])lab_r);

        memset(spec, 0, sizeof(*spec));
        spec->qa = q_scale(rmsp, f, s);
        spec->qf = q_scale(rms, f, s_f);
        spec->qg = gt / 8210.0 * 100.0;
        if (version == ALWAN_CQS_9_0) {
            spec->qp = (alwan_f64)NAN;
            spec->qd = (alwan_f64)NAN;
        } else {
            spec->qp = 100.0 - 3.6 * (rmsp - pdc);
            spec->qd = gt / gr * f * 100.0;
        }
        spec->cct = cct;
        spec->duv = duv;
        spec->cct_factor = f;
        spec->gamut_test = gt;
        spec->gamut_reference = gr;
        for (i = 0; i < ALWAN_CQS_SAMPLES; i++) {
            spec->qas[i] = q_scale(dep[i], f, s);
            spec->delta_c[i] = dc[i];
            spec->delta_e[i] = de[i];
            spec->delta_ep[i] = dep[i];
            for (k = 0; k < 3; k++) {
                spec->lab_test[i][k] = lab_t[i][k];
                spec->lab_reference[i][k] = lab_r[i][k];
            }
        }
        return ALWAN_OK;
    }
#endif
}

/* ----------------------------------------------------------------
 * CIE 2017 colour fidelity index and ANSI/IES TM-30-18
 * ---------------------------------------------------------------- */

alwan_status alwan__cie2017_compute(alwan_tm30_f64 *spec, alwan_spd_f64 const *test_spd, alwan_ctx *ctx) {
    if (!spec || !test_spd) return ALWAN_E_INVALID;
    if (!q_spd_ok(test_spd)) return ALWAN_E_INVALID;
#if !ALWAN_TABLE_CES_REFLECTANCE
    (void)ctx;
    return ALWAN_E_NODATA;
#else
    {
        /* The grid is the test's own interval when that is 1 or 5 nm, as colour works; any
         * other spacing is read at 1 nm, where colour refuses it. */
        double const ds = ((double)test_spd->wavelength_max - (double)test_spd->wavelength_min) / (double)(test_spd->count - 1);
        int const step = fabs(ds - 5.0) < 1e-9 ? 5 : 1;
        int const n = step == 5 ? Q_CFI_N5 : Q_CFI_N1;
        double const *tcs = step == 5 ? q_tcs_5nm : q_tcs_1nm;
        double S[Q_CFI_N1], Sr[Q_CFI_N1], X[3], cct = 0.0, duv = 0.0;
        double jab[2][Q_CFI_SAMPLES][3], h_ref[Q_CFI_SAMPLES], de[Q_CFI_SAMPLES], de_sum = 0.0;
        q_cmfs c2, c10;
        alwan_status st;
        int i, k, pass;

        st = q_load_cmfs(&c2, ALWAN_OBSERVER_CIE_1931_2DEG, Q_CFI_START, step, n, ctx);
        if (st != ALWAN_OK) return st;
        st = q_load_cmfs(&c10, ALWAN_OBSERVER_CIE_1964_10DEG, Q_CFI_START, step, n, ctx);
        if (st != ALWAN_OK) return st;
        q_read_spd(S, test_spd, (double)Q_CFI_START, (double)step, n, 0);
        q_integrate(X, S, NULL, &c2, n, (double)step);
        if (!(X[1] > 0.0)) return ALWAN_E_RANGE;
        st = q_cct(&cct, &duv, X, 25000.0, ctx);
        if (st != ALWAN_OK) return st;

        {
            double P[Q_CFI_N1], Dl[Q_CFI_N1];
            if (cct <= 5000.0) q_planck(P, (double)Q_CFI_START, (double)step, n, cct);
            if (cct >= 4000.0) {
                double x, y, m1, m2;
                q_daylight_xy(&x, &y, cct);
                q_daylight_m(&m1, &m2, x, y);
                for (i = 0; i < n; i++) {
                    double s0, s1, s2;
                    if (step == 1) {
                        s0 = q_basis_1nm[i];
                        s1 = q_basis_1nm[Q_CFI_N1 + i];
                        s2 = q_basis_1nm[2 * Q_CFI_N1 + i];
                    } else {
                        int const at = (Q_CFI_START - Q_BASIS5_START) / 5 + i;
                        s0 = q_basis_5nm[at];
                        s1 = q_basis_5nm[Q_BASIS5_N + at];
                        s2 = q_basis_5nm[2 * Q_BASIS5_N + at];
                    }
                    Dl[i] = s0 + m1 * s1 + m2 * s2;
                }
            }
            if (cct < 4000.0) {
                memcpy(Sr, P, sizeof(double) * (size_t)n);
            } else if (cct <= 5000.0) {
                double xp[3], xd[3], m;
                q_integrate(xp, P, NULL, &c2, n, (double)step);
                q_integrate(xd, Dl, NULL, &c2, n, (double)step);
                m = (cct - 4000.0) / 1000.0;
                for (i = 0; i < n; i++) {
                    Sr[i] = (1.0 - m) * (P[i] / xp[1]) + m * (Dl[i] / xd[1]);
                }
            } else {
                memcpy(Sr, Dl, sizeof(double) * (size_t)n);
            }
        }

        for (pass = 0; pass < 2; pass++) {
            double const *src = pass == 0 ? S : Sr;
            double Xw[3], scaled[Q_CFI_N1], kk;
            alwan_ciecam02_viewing_conditions_f64 vc;
            q_integrate(Xw, src, NULL, &c10, n, (double)step);
            if (!(Xw[1] > 0.0)) return ALWAN_E_RANGE;
            kk = 100.0 / Xw[1];
            for (k = 0; k < 3; k++) Xw[k] = kk * Xw[k];
            for (i = 0; i < n; i++) scaled[i] = src[i] * kk;
            memset(&vc, 0, sizeof(vc));
            vc.white_xyz.x = Xw[0];
            vc.white_xyz.y = Xw[1];
            vc.white_xyz.z = Xw[2];
            vc.adapting_luminance = 100.0;
            vc.background_luminance = 20.0;
            vc.surround = ALWAN_CIECAM02_SURROUND_AVERAGE;
            vc.discount_illuminant = 1;
            for (i = 0; i < Q_CFI_SAMPLES; i++) {
                double xs[3], R[Q_CFI_N1], Jp, Mp, hr;
                alwan_xyz_f64 xyz;
                alwan_ciecam02_correlates_f64 cam;
                int w;
                for (w = 0; w < n; w++) R[w] = tcs[(size_t)i * (size_t)n + (size_t)w] * scaled[w];
                q_integrate(xs, R, NULL, &c10, n, (double)step);
                xyz.x = xs[0];
                xyz.y = xs[1];
                xyz.z = xs[2];
                st = alwan_ciecam02_forward_f64(&cam, &xyz, &vc);
                if (st != ALWAN_OK) return st;
                ALWAN_DENORM_CIECAM02(&cam);
                /* JMh_CIECAM02_to_CAM02UCS */
                Jp = ((1.0 + 100.0 * 0.007) * cam.J) / (1.0 + 0.007 * cam.J);
                Mp = (1.0 / 0.0228) * ALWAN_LN_F64(1.0 + 0.0228 * cam.M);
                hr = cam.h * (ALWAN_PI / 180.0);
                jab[pass][i][0] = Jp;
                jab[pass][i][1] = Mp * ALWAN_COS_F64(hr);
                jab[pass][i][2] = Mp * ALWAN_SIN_F64(hr);
                if (pass == 1) h_ref[i] = cam.h;
            }
        }

        memset(spec, 0, sizeof(*spec));
        for (i = 0; i < Q_CFI_SAMPLES; i++) {
            double const a = jab[0][i][0] - jab[1][i][0];
            double const b = jab[0][i][1] - jab[1][i][1];
            double const c = jab[0][i][2] - jab[1][i][2];
            de[i] = ALWAN_SQRT_F64(a * a + b * b + c * c);
            de_sum += de[i];
            spec->delta_e[i] = de[i];
            spec->rs[i] = q_scale(de[i], 1.0, 6.73);
            for (k = 0; k < 3; k++) {
                spec->jab_test[i][k] = jab[0][i][k];
                spec->jab_reference[i][k] = jab[1][i][k];
            }
        }
        spec->rf = q_scale(de_sum / (double)Q_CFI_SAMPLES, 1.0, 6.73);
        spec->cct = cct;
        spec->duv = duv;

        /* TM-30-18: bins by the reference hue, floor(h / 22.5). */
        {
            int counts[ALWAN_TM30_HUE_BINS];
            double bin_de[ALWAN_TM30_HUE_BINS], area_t = 0.0, area_r = 0.0;
            int b;
            for (b = 0; b < ALWAN_TM30_HUE_BINS; b++) {
                counts[b] = 0;
                bin_de[b] = 0.0;
            }
            for (i = 0; i < Q_CFI_SAMPLES; i++) {
                double const h = h_ref[i];
                int bin = 0;
                while (bin < ALWAN_TM30_HUE_BINS - 1 && h >= 22.5 * (double)(bin + 1)) bin++;
                spec->bins[i] = bin;
                counts[bin]++;
                bin_de[bin] += de[i];
                spec->averages_test[bin][0] += jab[0][i][1];
                spec->averages_test[bin][1] += jab[0][i][2];
                spec->averages_reference[bin][0] += jab[1][i][1];
                spec->averages_reference[bin][1] += jab[1][i][2];
            }
            for (b = 0; b < ALWAN_TM30_HUE_BINS; b++) {
                double nb;
                if (counts[b] == 0) continue;
                nb = (double)counts[b];
                spec->averages_test[b][0] /= nb;
                spec->averages_test[b][1] /= nb;
                spec->averages_reference[b][0] /= nb;
                spec->averages_reference[b][1] /= nb;
                spec->rfs[b] = q_scale(bin_de[b] / nb, 1.0, 6.73);
                spec->average_norms[b] = ALWAN_SQRT_F64(spec->averages_reference[b][0] * spec->averages_reference[b][0] +
                                                        spec->averages_reference[b][1] * spec->averages_reference[b][1]);
            }
            for (b = 0; b < ALWAN_TM30_HUE_BINS; b++) {
                int const c = (b + 1) % ALWAN_TM30_HUE_BINS;
                area_t += (spec->averages_test[b][0] * spec->averages_test[c][1] - spec->averages_test[b][1] * spec->averages_test[c][0]) / 2.0;
                area_r += (spec->averages_reference[b][0] * spec->averages_reference[c][1] -
                           spec->averages_reference[b][1] * spec->averages_reference[c][0]) / 2.0;
            }
            if (area_r != 0.0) spec->rg = 100.0 * (area_t / area_r);
            for (b = 0; b < ALWAN_TM30_HUE_BINS; b++) {
                double const ang = (22.5 * (double)b + 11.25) / 180.0 * ALWAN_PI;
                double const cs = ALWAN_COS_F64(ang), sn = ALWAN_SIN_F64(ang);
                double const da = spec->averages_test[b][0] - spec->averages_reference[b][0];
                double const db = spec->averages_test[b][1] - spec->averages_reference[b][1];
                if (spec->average_norms[b] == 0.0) continue;
                spec->rcs[b] = 100.0 * (da * cs + db * sn) / spec->average_norms[b];
                spec->rhs[b] = (-da * sn + db * cs) / spec->average_norms[b];
            }
        }
        return ALWAN_OK;
    }
#endif
}

/* ----------------------------------------------------------------
 * Public CQS specification
 * ---------------------------------------------------------------- */

alwan_status alwan_cqs_specification_f64(alwan_cqs_f64 *spec_out, alwan_spd_f64 const *test_spd,
                                         alwan_cqs_version version, alwan_ctx *ctx) {
    alwan_cqs_f64 tmp;
    alwan_status st;
    if (!spec_out || !test_spd || !ctx) return ALWAN_E_INVALID;
    st = alwan__cqs_compute(&tmp, test_spd, version, ctx);
    if (st == ALWAN_OK) *spec_out = tmp;
    return st;
}

alwan_status alwan_cqs_specification_f32(alwan_cqs_f32 *spec_out, alwan_spd_f32 const *test_spd,
                                         alwan_cqs_version version, alwan_ctx *ctx) {
    alwan_cqs_f64 wide;
    alwan_spd_f64 tmp;
    alwan_status st;
    size_t i;
    int k, j;
    if (!spec_out || !test_spd || !ctx || !test_spd->values) return ALWAN_E_INVALID;
    st = alwan_spd_create_f64(&tmp, (alwan_f64)test_spd->wavelength_min, (alwan_f64)test_spd->wavelength_max, test_spd->count, ctx);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < test_spd->count; i++) tmp.values[i] = (alwan_f64)test_spd->values[i];
    st = alwan__cqs_compute(&wide, &tmp, version, ctx);
    alwan_spd_destroy_f64(&tmp, ctx);
    if (st != ALWAN_OK) return st;
    spec_out->qa = (alwan_f32)wide.qa;
    spec_out->qf = (alwan_f32)wide.qf;
    spec_out->qp = (alwan_f32)wide.qp;
    spec_out->qg = (alwan_f32)wide.qg;
    spec_out->qd = (alwan_f32)wide.qd;
    spec_out->cct = (alwan_f32)wide.cct;
    spec_out->duv = (alwan_f32)wide.duv;
    spec_out->cct_factor = (alwan_f32)wide.cct_factor;
    spec_out->gamut_test = (alwan_f32)wide.gamut_test;
    spec_out->gamut_reference = (alwan_f32)wide.gamut_reference;
    for (k = 0; k < ALWAN_CQS_SAMPLES; k++) {
        spec_out->qas[k] = (alwan_f32)wide.qas[k];
        spec_out->delta_c[k] = (alwan_f32)wide.delta_c[k];
        spec_out->delta_e[k] = (alwan_f32)wide.delta_e[k];
        spec_out->delta_ep[k] = (alwan_f32)wide.delta_ep[k];
        for (j = 0; j < 3; j++) {
            spec_out->lab_test[k][j] = (alwan_f32)wide.lab_test[k][j];
            spec_out->lab_reference[k][j] = (alwan_f32)wide.lab_reference[k][j];
        }
    }
    return ALWAN_OK;
}
