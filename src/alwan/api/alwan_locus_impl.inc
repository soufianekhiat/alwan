/*
 * Mired and locus inverses, instantiated per precision by alwan_locus.c. Both
 * inverses run in double in both precisions; the forward formulas follow
 * colour-science's order of operations.
 */

#ifndef ALWAN__LOCUS_ONCE
#define ALWAN__LOCUS_ONCE

/* Krystek 1985, u and v as rational functions of T, with their derivatives. */
static void alwan__lc_krystek(double T, double *u, double *v, double *du, double *dv) {
    double const T2 = T * T;
    double const nu = 0.860117757 + 1.54118254 * 1e-4 * T + 1.28641212 * 1e-7 * T2;
    double const Du = 1.0 + 8.42420235 * 1e-4 * T + 7.08145163 * 1e-7 * T2;
    double const nv = 0.317398726 + 4.22806245 * 1e-5 * T + 4.20481691 * 1e-8 * T2;
    double const Dv = 1.0 - 2.89741816 * 1e-5 * T + 1.61456053 * 1e-7 * T2;
    *u = nu / Du;
    *v = nv / Dv;
    *du = ((1.54118254 * 1e-4 + 2.0 * 1.28641212 * 1e-7 * T) * Du - nu * (8.42420235 * 1e-4 + 2.0 * 7.08145163 * 1e-7 * T)) /
          (Du * Du);
    *dv = ((4.22806245 * 1e-5 + 2.0 * 4.20481691 * 1e-8 * T) * Dv - nv * (-2.89741816 * 1e-5 + 2.0 * 1.61456053 * 1e-7 * T)) /
          (Dv * Dv);
}

/* The CIE daylight locus, x_D and y_D of T on one side of 7000 K, with derivatives. */
static void alwan__lc_cie_d(double T, int high, double *x, double *y, double *dx, double *dy) {
    double const A = high ? -2.0064e9 : -4.6070e9, B = high ? 1.9018e6 : 2.9678e6;
    double const C = high ? 0.24748e3 : 0.09911e3, D = high ? 0.237040 : 0.244063;
    double const T2 = T * T, T3 = T2 * T;
    *x = A / T3 + B / T2 + C / T + D;
    *dx = -3.0 * A / (T3 * T) - 2.0 * B / T3 - C / T2;
    *y = -3.0 * *x * *x + 2.870 * *x - 0.275;
    *dy = (-6.0 * *x + 2.870) * *dx;
}

/* One curve point and the half derivative of its squared distance to (p, q). */
typedef void (*alwan__lc_curve)(double T, int branch, double *a, double *b, double *da, double *db);

static void alwan__lc_krystek_curve(double T, int branch, double *a, double *b, double *da, double *db) {
    (void)branch;
    alwan__lc_krystek(T, a, b, da, db);
}

static double alwan__lc_dist2(alwan__lc_curve f, int branch, double T, double p, double q, double *g) {
    double a, b, da, db;
    f(T, branch, &a, &b, &da, &db);
    *g = (a - p) * da + (b - q) * db;
    return (a - p) * (a - p) + (b - q) * (b - q);
}

/* Sample i of n, evenly spaced in mired from hi (i = 0) to lo (i = n), ends exact. */
static double alwan__lc_grid(int i, int n, double lo, double hi) {
    if (i <= 0) return hi;
    if (i >= n) return lo;
    return 1e6 / (1e6 / hi + (1e6 / lo - 1e6 / hi) * (double)i / (double)n);
}

/* The nearest point of one smooth piece of a curve to (p, q), T in [lo, hi]: a scan in
 * mired finds the nearest sample, and bisection on the derivative of the squared
 * distance finds the minimum between its neighbours to full precision. *at_end is set
 * when the minimum is an end of [lo, hi]. */
static double alwan__lc_nearest(alwan__lc_curve f, int branch, double lo, double hi, double p, double q, double *d2,
                                int *at_end) {
    enum { N = 512 };
    double best = 0.0, bd = 0.0, g, a, b, ga, gb;
    int i, bi = 0;
    for (i = 0; i <= N; i++) {
        double const T = alwan__lc_grid(i, N, lo, hi), dd = alwan__lc_dist2(f, branch, T, p, q, &g);
        if (i == 0 || dd < bd) {
            bd = dd;
            best = T;
            bi = i;
        }
    }
    a = alwan__lc_grid(bi + 1, N, lo, hi);
    b = alwan__lc_grid(bi - 1, N, lo, hi);
    alwan__lc_dist2(f, branch, a, p, q, &ga);
    alwan__lc_dist2(f, branch, b, p, q, &gb);
    if (ga < 0.0 && gb > 0.0) {
        for (i = 0; i < 200; i++) {
            double const mid = 0.5 * (a + b);
            double gm;
            if (mid <= a || mid >= b) break;
            alwan__lc_dist2(f, branch, mid, p, q, &gm);
            if (gm < 0.0) a = mid; else b = mid;
        }
        best = 0.5 * (a + b);
        bd = alwan__lc_dist2(f, branch, best, p, q, &g);
        *at_end = 0;
    } else {
        /* A point on the curve at the end itself is in range. */
        *at_end = (best <= lo || best >= hi) && bd > 1e-20;
    }
    *d2 = bd;
    return best;
}

static void alwan__lc_cie_d_curve(double T, int branch, double *a, double *b, double *da, double *db) {
    alwan__lc_cie_d(T, branch, a, b, da, db);
}

#endif /* ALWAN__LOCUS_ONCE */

alwan_status ALWAN_CORE_FNLIT(alwan_cct_to_mired)(ALWAN_CORE_T *mired_out, ALWAN_CORE_T cct) {
    if (!mired_out || cct == ALWAN_CORE_LITERAL(0.0)) return ALWAN_E_INVALID;
    *mired_out = ALWAN_CORE_LITERAL(1.0e6) / cct;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_mired_to_cct)(ALWAN_CORE_T *cct_out, ALWAN_CORE_T mired) {
    if (!cct_out || mired == ALWAN_CORE_LITERAL(0.0)) return ALWAN_E_INVALID;
    *cct_out = ALWAN_CORE_LITERAL(1.0e6) / mired;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_cct_to_uv_krystek1985)(ALWAN_CORE_VEC2 *uv_out, ALWAN_CORE_T cct) {
    ALWAN_CORE_T const T = cct, T2 = T * T;
    if (!uv_out) return ALWAN_E_INVALID;
    uv_out->v[0] = (ALWAN_CORE_LITERAL(0.860117757) + ALWAN_CORE_LITERAL(1.54118254) * ALWAN_CORE_LITERAL(1e-4) * T +
                    ALWAN_CORE_LITERAL(1.28641212) * ALWAN_CORE_LITERAL(1e-7) * T2) /
                   (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(8.42420235) * ALWAN_CORE_LITERAL(1e-4) * T +
                    ALWAN_CORE_LITERAL(7.08145163) * ALWAN_CORE_LITERAL(1e-7) * T2);
    uv_out->v[1] = (ALWAN_CORE_LITERAL(0.317398726) + ALWAN_CORE_LITERAL(4.22806245) * ALWAN_CORE_LITERAL(1e-5) * T +
                    ALWAN_CORE_LITERAL(4.20481691) * ALWAN_CORE_LITERAL(1e-8) * T2) /
                   (ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_LITERAL(2.89741816) * ALWAN_CORE_LITERAL(1e-5) * T +
                    ALWAN_CORE_LITERAL(1.61456053) * ALWAN_CORE_LITERAL(1e-7) * T2);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_uv_to_cct_krystek1985)(ALWAN_CORE_T *cct_out, ALWAN_CORE_VEC2 const *uv) {
    double d2;
    int at_end;
    double T;
    if (!cct_out || !uv) return ALWAN_E_INVALID;
    T = alwan__lc_nearest(alwan__lc_krystek_curve, 0, 1000.0, 15000.0, (double)uv->v[0], (double)uv->v[1], &d2, &at_end);
    if (at_end) return ALWAN_E_RANGE;
    *cct_out = (ALWAN_CORE_T)T;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_xy_to_cct_cie_d)(ALWAN_CORE_T *cct_out, ALWAN_CORE_VEC2 const *xy) {
    double d_lo, d_hi, T_lo, T_hi, x, y, dx, dy, d_kink;
    int e_lo, e_hi;
    if (!cct_out || !xy) return ALWAN_E_INVALID;
    T_lo = alwan__lc_nearest(alwan__lc_cie_d_curve, 0, 4000.0, 7000.0, (double)xy->v[0], (double)xy->v[1], &d_lo, &e_lo);
    T_hi = alwan__lc_nearest(alwan__lc_cie_d_curve, 1, 7000.0, 25000.0, (double)xy->v[0], (double)xy->v[1], &d_hi, &e_hi);
    /* The two formulas meet at 7000 K only approximately; the joint itself is the lower
     * formula's point, as the locus is defined. */
    alwan__lc_cie_d(7000.0, 0, &x, &y, &dx, &dy);
    d_kink = (x - (double)xy->v[0]) * (x - (double)xy->v[0]) + (y - (double)xy->v[1]) * (y - (double)xy->v[1]);
    if (d_lo <= d_hi) {
        if (e_lo && T_lo <= 4000.0) return ALWAN_E_RANGE;
        *cct_out = (ALWAN_CORE_T)(d_kink < d_lo ? 7000.0 : T_lo);
    } else {
        if (e_hi && T_hi >= 25000.0) return ALWAN_E_RANGE;
        *cct_out = (ALWAN_CORE_T)(d_kink < d_hi ? 7000.0 : T_hi);
    }
    return ALWAN_OK;
}
