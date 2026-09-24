/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Vignetting: the light a lens loses towards the corners, characterised from a flat-field
 * frame (a picture of an evenly lit, featureless surface) and divided out of other frames.
 * As colour-hdri's distortion.vignette (BSD-3-Clause), which suite 238 holds this to.
 *
 * The flat field's principal point is the centre of mass of its brightest pixels: those
 * whose median over the channels exceeds threshold (0.99) times the brightest median, the
 * centroid's column and row divided by the width and the height. Coordinates are fractions
 * of the frame, 0 at the first pixel and 1 at the last, so a characterisation corrects a
 * frame of any size.
 *
 * PARABOLIC and HYPERBOLIC_COSINE fit a surface per channel to every pixel, about the
 * principal point (x, y the column and row fractions minus the point's):
 *
 *   PARABOLIC          (a_x2 x^2 + a_x1 x + a_x0) / 2 + (a_y2 y^2 + a_y1 y + a_y0) / 2, by
 *                      least squares within colour-hdri's bounds, exactly (a_x0 and a_y0
 *                      only ever appear as their sum, bounded to [0.9, 1.1])
 *   HYPERBOLIC_COSINE  1 - cosh(r_x (x - 0.5 - x_0)) cosh(r_y (y - 0.5 - y_0)) + c, by
 *                      Levenberg-Marquardt within colour-hdri's bounds, from its start
 *
 * colour-hdri fits both with scipy's curve_fit inside those bounds, which expect a flat
 * field normalised to about 1 at its brightest; the suite compares the surfaces (the
 * parabola's coefficients are not unique). A parabola fits a lens's falloff poorly, and its
 * constant often rests on the 0.9 bound: that is colour-hdri's model, kept. colour-hdri subtracts the principal point's ROW fraction from the column
 * coordinate and its column fraction from the row; alwan pairs them the right way round,
 * which is the same on a square frame.
 *
 * BIVARIATE_SPLINE keeps the flat field itself at low resolution: each channel smoothed by
 * a Gaussian (sigma denoise_sigma, 6, truncated at 6 sigma, the edge repeated), resampled to
 * `samples` (50) points on its longer side through the interpolating bicubic spline (FITPACK
 * with s = 0: not-a-knot cubics, one axis then the other), smoothed again (sigma
 * post_denoise_sigma, 1, truncated at 6); a frame is corrected through the same spline over
 * that grid.
 *
 * RBF samples the flat field (a Gaussian of sigma denoise_sigma, the edge mirrored) at
 * colour-hdri's pattern: two points on each diagonal, ten along each edge and a polar grid
 * of 7 radii by 21 angles about the principal point, stretched by the aspect ratio and moved
 * by colour-hdri's fixed jitter; and interpolates them with scipy's RBFInterpolator as
 * colour-hdri calls it: the cubic kernel r^3, a linear tail on coordinates scaled to
 * [-1, 1], smoothing (0.001) on the diagonal, the system solved by LU with partial pivoting.
 *
 * A corrected frame is the frame divided by the surface, channel by channel. 8-bit data are
 * read as value / 255 and written back rounded and saturated.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static double const alwan_vg_jitter[21 * 7 * 2] = {
#include "../data/vignette/rbf_jitter.csv"
};
ALWAN_DIAG_POP

struct alwan_vignette {
    alwan_vignette_method method;
    size_t ch;
    double ppx, ppy;         /* the principal point, column and row fractions */
    double coef[4][5];       /* PARABOLIC: a_x2, a_x1, a_y2, a_y1, (a_x0 + a_y0) / 2; HYPERBOLIC_COSINE: r_x, x_0, r_y, y_0, c */
    size_t gw, gh;           /* BIVARIATE_SPLINE: the grid, gw x gh x ch in grid */
    double *grid;
    size_t n;                /* RBF: the samples, n x 2 (row, column fractions) in pts */
    double *pts;
    double *w;               /* RBF: n + 3 weights a channel */
    double shift[2], scale[2];
};

/* ------------------------------------------------------------------------------------ */
/* Pieces                                                                               */

static long alwan_vg_reflect(long q, long n);

/* scipy.ndimage.gaussian_filter on one plane, rows then columns (axis 0 then 1): weights
 * exp(-x^2 / (2 s^2)) normalised, radius int(truncate s + 0.5), the edge repeated (nearest)
 * or mirrored about the last pixel (reflect). */
static alwan_status alwan_vg_gauss(double *a, size_t w, size_t h, double sigma, double truncate, int reflect) {
    long const r = (long)(truncate * sigma + 0.5);
    size_t const m = w > h ? w : h;
    double *k = (double *)ALWAN_ALLOC(alwan_safe_array_size((size_t)r + 1 + m, sizeof(double)), sizeof(double));
    double *line, sum = 0.0;
    long i, j;
    size_t x, y;
    if (!k) return ALWAN_E_NOMEM;
    line = k + r + 1;
    for (i = 0; i <= r; i++) {
        k[i] = exp(-0.5 / (sigma * sigma) * (double)i * (double)i);
        sum += i ? 2.0 * k[i] : k[i];
    }
    for (i = 0; i <= r; i++) k[i] /= sum;
#define ALWAN_VG_AT(n, q) (reflect ? alwan_vg_reflect((q), (long)(n)) : ((q) < 0 ? 0 : (q) >= (long)(n) ? (long)(n) - 1 : (q)))
    /* down the columns */
    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) line[y] = a[y * w + x];
        for (y = 0; y < h; y++) {
            double s = line[y] * k[0];
            for (j = 1; j <= r; j++) s += k[j] * (line[ALWAN_VG_AT(h, (long)y - j)] + line[ALWAN_VG_AT(h, (long)y + j)]);
            a[y * w + x] = s;
        }
    }
    /* along the rows */
    for (y = 0; y < h; y++) {
        memcpy(line, a + y * w, w * sizeof(double));
        for (x = 0; x < w; x++) {
            double s = line[x] * k[0];
            for (j = 1; j <= r; j++) s += k[j] * (line[ALWAN_VG_AT(w, (long)x - j)] + line[ALWAN_VG_AT(w, (long)x + j)]);
            a[y * w + x] = s;
        }
    }
#undef ALWAN_VG_AT
    ALWAN_FREE(k);
    return ALWAN_OK;
}

/* scipy's 'reflect': d c b a | a b c d | d c b a, any distance. */
static long alwan_vg_reflect(long q, long n) {
    long const p = 2 * n;
    q %= p;
    if (q < 0) q += p;
    return q < n ? q : p - 1 - q;
}

/* The not-a-knot cubic spline through (x[i], f[i]), i < m (m >= 4), its second derivatives
 * into M (m values); scratch of 3 m. */
static void alwan_vg_spline(double *M, double const *x, double const *f, size_t m, double *s) {
    double *a = s, *b = s + m, *c = s + 2 * m;
    size_t i;
    double h0, h1, hm, hn;
    /* interior equations for M[1..m-2], with M[0] and M[m-1] eliminated by the not-a-knot
     * conditions (the third derivative continuous at x[1] and x[m-2]) */
    for (i = 1; i + 1 < m; i++) {
        double const hl = x[i] - x[i - 1], hr = x[i + 1] - x[i];
        a[i] = hl;
        b[i] = 2.0 * (hl + hr);
        c[i] = hr;
        M[i] = 6.0 * ((f[i + 1] - f[i]) / hr - (f[i] - f[i - 1]) / hl);
    }
    h0 = x[1] - x[0], h1 = x[2] - x[1];
    hm = x[m - 2] - x[m - 3], hn = x[m - 1] - x[m - 2];
    b[1] += h0 + h0 * h0 / h1;
    c[1] -= h0 * h0 / h1;
    b[m - 2] += hn + hn * hn / hm;
    a[m - 2] -= hn * hn / hm;
    /* the tridiagonal system, by elimination */
    for (i = 2; i + 1 < m; i++) {
        double const t = a[i] / b[i - 1];
        b[i] -= t * c[i - 1];
        M[i] -= t * M[i - 1];
    }
    M[m - 2] /= b[m - 2];
    for (i = m - 2; i-- > 1;) M[i] = (M[i] - c[i] * M[i + 1]) / b[i];
    M[0] = M[1] * (1.0 + h0 / h1) - M[2] * h0 / h1;
    M[m - 1] = M[m - 2] * (1.0 + hn / hm) - M[m - 3] * hn / hm;
}

static double alwan_vg_spline_at(double const *x, double const *f, double const *M, size_t m, double q) {
    size_t lo = 0, hi = m - 1;
    double hh, u, v;
    while (hi - lo > 1) {
        size_t const mid = (lo + hi) / 2;
        if (x[mid] <= q) lo = mid;
        else hi = mid;
    }
    hh = x[hi] - x[lo];
    u = x[hi] - q, v = q - x[lo];
    return M[lo] * u * u * u / (6.0 * hh) + M[hi] * v * v * v / (6.0 * hh) + (f[lo] / hh - M[lo] * hh / 6.0) * u +
           (f[hi] / hh - M[hi] * hh / 6.0) * v;
}

/* numpy.linspace(0, 1, n) */
static void alwan_vg_linspace(double *t, size_t n) {
    size_t i;
    double const step = (1.0 - 0.0) / (double)(n - 1);
    for (i = 0; i < n; i++) t[i] = (double)i * step + 0.0;
    t[n - 1] = 1.0;
}

/* The bicubic interpolating spline of a (gw x gh) plane, evaluated on a (ow x oh) grid of
 * linspace fractions: along the rows, then down the columns. */
static alwan_status alwan_vg_resample(double *out, size_t ow, size_t oh, double const *g, size_t gw, size_t gh) {
    size_t const m = (gw > gh ? gw : gh), q = (ow > oh ? ow : oh);
    double *buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(gh * ow + 7 * m + 2 * q, sizeof(double)), sizeof(double));
    double *tmp, *xs, *ys, *M, *s, *col, *tq;
    size_t x, y;
    if (!buf) return ALWAN_E_NOMEM;
    tmp = buf;              /* gh x ow */
    xs = tmp + gh * ow;
    M = xs + m;
    s = M + m;
    col = s + 3 * m;
    ys = col + m;
    tq = ys + m;
    (void)ys;
    alwan_vg_linspace(xs, gw);
    alwan_vg_linspace(tq, ow);
    for (y = 0; y < gh; y++) {
        alwan_vg_spline(M, xs, g + y * gw, gw, s);
        for (x = 0; x < ow; x++) tmp[y * ow + x] = alwan_vg_spline_at(xs, g + y * gw, M, gw, tq[x]);
    }
    alwan_vg_linspace(xs, gh);
    alwan_vg_linspace(tq, oh);
    for (x = 0; x < ow; x++) {
        for (y = 0; y < gh; y++) col[y] = tmp[y * ow + x];
        alwan_vg_spline(M, xs, col, gh, s);
        for (y = 0; y < oh; y++) out[y * ow + x] = alwan_vg_spline_at(xs, col, M, gh, tq[y]);
    }
    ALWAN_FREE(buf);
    return ALWAN_OK;
}

/* The principal point: the centroid of the pixels whose median over the channels exceeds
 * threshold times the largest median. */
static void alwan_vg_principal_point(double *ppx, double *ppy, double const *a, size_t w, size_t h, size_t ch, double threshold) {
    double mx = -HUGE_VAL, sx = 0.0, sy = 0.0, cnt = 0.0;
    size_t i, pass;
    for (pass = 0; pass < 2; pass++)
        for (i = 0; i < w * h; i++) {
            double v[4], med;
            size_t c, d;
            for (c = 0; c < ch; c++) v[c] = a[i * ch + c];
            for (c = 1; c < ch; c++)   /* insertion sort, at most four */
                for (d = c; d > 0 && v[d - 1] > v[d]; d--) {
                    double const t = v[d];
                    v[d] = v[d - 1], v[d - 1] = t;
                }
            med = ch % 2 ? v[ch / 2] : 0.5 * (v[ch / 2 - 1] + v[ch / 2]);
            if (pass == 0) {
                if (med > mx) mx = med;
            } else if (mx * threshold < med) {
                sx += (double)(i % w), sy += (double)(i / w), cnt += 1.0;
            }
        }
    *ppx = cnt > 0.0 ? sx / cnt / (double)w : 0.5;
    *ppy = cnt > 0.0 ? sy / cnt / (double)h : 0.5;
}

/* colour-hdri's vignette_sampling_coordinates with its defaults, (row, column) fractions,
 * about the principal point (pr, pc) with aspect ratio w / h; returns the count kept. */
static size_t alwan_vg_rbf_coordinates(double *out, double pr, double pc, double aspect) {
    double c[40 + 147][2], lin[10];
    size_t n = 0, i, k;
    double const pi = 3.14159265358979323846, radius = 1.0 + (((pr > pc ? pr : pc) - 0.5) * 2.0);
    for (i = 0; i < 10; i++) lin[i] = (double)i / 9.0;
    lin[9] = 1.0;
    /* numpy.linspace(0, 1, 10)[1:2] and [-2:-1]: 1/9 and 8/9 as numpy computes them */
    for (i = 0; i < 2; i++) c[n][0] = lin[i ? 8 : 1], c[n][1] = lin[i ? 8 : 1], n++;
    for (i = 0; i < 2; i++) c[n][0] = lin[i ? 8 : 1], c[n][1] = 1.0 - lin[i ? 8 : 1], n++;
    for (i = 0; i < 10; i++) c[n][0] = lin[i], c[n][1] = 0.0, n++;
    for (i = 0; i < 10; i++) c[n][0] = lin[i], c[n][1] = 1.0, n++;
    for (i = 1; i < 9; i++) c[n][0] = 0.0, c[n][1] = lin[i], n++;
    for (i = 1; i < 9; i++) c[n][0] = 1.0, c[n][1] = lin[i], n++;
    /* LinearInterpolator([0, 0.5, 1], [0, p, 1]) on each coordinate */
    for (i = 0; i < n; i++)
        for (k = 0; k < 2; k++) {
            double const p = k ? pc : pr, t = c[i][k];
            /* numpy.interp: slope times the offset in the segment, plus its start */
            c[i][k] = t < 0.5 ? (p - 0.0) / (0.5 - 0.0) * (t - 0.0) + 0.0 : (1.0 - p) / (1.0 - 0.5) * (t - 0.5) + p;
        }
    /* the polar grid: rho = linspace(0, radius, 7), phi = linspace(-pi, pi, 21), jittered,
     * scaled by radius 0.9 / 2, the column stretched by the aspect ratio, about the point */
    for (i = 0; i < 21; i++)
        for (k = 0; k < 7; k++) {
            /* numpy.linspace: the start plus i steps, the last point the stop itself */
            double const rho = k == 6 ? radius : (double)k * (radius / 6.0);
            double const phi = i == 20 ? pi : -pi + (double)i * ((pi - -pi) / 20.0);
            size_t const at = (i * 7 + k) * 2;
            double x = rho * cos(phi), y = rho * sin(phi);
            x += (alwan_vg_jitter[at] - 0.5) / 1000.0;
            y += (alwan_vg_jitter[at + 1] - 0.5) / 1000.0;
            x /= 2.0 * 1.0 / 0.9;
            y /= 2.0 * 1.0 / 0.9;
            y *= aspect;
            c[n][0] = x + pr, c[n][1] = y + pc, n++;
        }
    for (i = 0, k = 0; i < n; i++)
        if (c[i][0] >= 0.0 && c[i][1] >= 0.0 && c[i][0] <= 1.0 && c[i][1] <= 1.0) out[2 * k] = c[i][0], out[2 * k + 1] = c[i][1], k++;
    return k;
}

/* Solve A x = b (n x n, row-major, destroyed) by LU with partial pivoting, as LAPACK's
 * dgesv: the pivot the largest magnitude in its column, the first on a tie. */
static int alwan_vg_solve(double *A, double *b, size_t n) {
    size_t i, j, k;
    for (k = 0; k < n; k++) {
        size_t p = k;
        double best = fabs(A[k * n + k]);
        for (i = k + 1; i < n; i++)
            if (fabs(A[i * n + k]) > best) best = fabs(A[i * n + k]), p = i;
        if (best == 0.0) return 0;
        if (p != k) {
            for (j = 0; j < n; j++) {
                double const t = A[k * n + j];
                A[k * n + j] = A[p * n + j], A[p * n + j] = t;
            }
            {
                double const t = b[k];
                b[k] = b[p], b[p] = t;
            }
        }
        for (i = k + 1; i < n; i++) {
            double const l = A[i * n + k] / A[k * n + k];
            A[i * n + k] = l;
            for (j = k + 1; j < n; j++) A[i * n + j] -= l * A[k * n + j];
            b[i] -= l * b[k];
        }
    }
    for (k = n; k-- > 0;) {
        double s = b[k];
        for (j = k + 1; j < n; j++) s -= A[k * n + j] * b[j];
        b[k] = s / A[k * n + k];
    }
    return 1;
}

/* The least-squares parabola within colour-hdri's bounds (a_x2, a_y2 in [-5, 0], a_x1, a_y1
 * in [-0.5, 0.5], the constant in [0.9, 1.1]), from its normal equations N p = b: every
 * assignment of each coefficient to free, its lower or its upper bound, the free ones solved,
 * the feasible solution of least cost kept. Exact, and 243 solves of at most 5 x 5. */
static int alwan_vg_parabola_bounded(double *best, double const *N, double const *b) {
    static double const lo[5] = { -5.0, -0.5, -5.0, -0.5, 0.9 }, hi[5] = { 0.0, 0.5, 0.0, 0.5, 1.1 };
    double best_cost = HUGE_VAL;
    int code, found = 0;
    for (code = 0; code < 243; code++) {
        int state[5], fr[5], nf = 0, i, j, t = code, ok = 1;
        double p[5], A[25], r[5], cost = 0.0;
        for (i = 0; i < 5; i++) {
            state[i] = t % 3, t /= 3;
            if (state[i] == 0) fr[nf++] = i;
            else p[i] = state[i] == 1 ? lo[i] : hi[i];
        }
        if (nf) {
            for (i = 0; i < nf; i++) {
                r[i] = b[fr[i]];
                for (j = 0; j < 5; j++)
                    if (state[j]) r[i] -= N[fr[i] * 5 + j] * p[j];
                for (j = 0; j < nf; j++) A[i * nf + j] = N[fr[i] * 5 + fr[j]];
            }
            if (!alwan_vg_solve(A, r, (size_t)nf)) continue;
            for (i = 0; i < nf; i++) {
                p[fr[i]] = r[i];
                if (r[i] < lo[fr[i]] || r[i] > hi[fr[i]]) ok = 0;
            }
        }
        if (!ok) continue;
        /* the cost less the constant z'z: p'Np - 2 b'p */
        for (i = 0; i < 5; i++) {
            double np = 0.0;
            for (j = 0; j < 5; j++) np += N[i * 5 + j] * p[j];
            cost += p[i] * np - 2.0 * b[i] * p[i];
        }
        if (cost < best_cost) {
            best_cost = cost, found = 1;
            for (i = 0; i < 5; i++) best[i] = p[i];
        }
    }
    return found;
}

static double alwan_vg_hcosh(double const *p, double x, double y) {
    return 1.0 - cosh(p[0] * (x - 0.5 - p[1])) * cosh(p[2] * (y - 0.5 - p[3])) + p[4];
}

/* Levenberg-Marquardt on the hyperbolic cosine, the parameters kept within colour-hdri's
 * bounds by projection; z the channel's pixels, xs and ys their coordinates. */
static void alwan_vg_fit_hcosh(double *p, double const *z, double const *xs, double const *ys, size_t n) {
    static double const lo[5] = { 0.5, -1.0, 0.5, -1.0, 0.0 }, hi[5] = { 5.0, 0.0, 5.0, 0.0, 1.5 };
    double lambda = 1e-3, cost = 0.0;
    int it, i, j;
    size_t q;
    p[0] = 1.0, p[1] = 0.0, p[2] = 1.0, p[3] = 0.0, p[4] = 0.0;
    for (q = 0; q < n; q++) {
        double const r = alwan_vg_hcosh(p, xs[q], ys[q]) - z[q];
        cost += r * r;
    }
    for (it = 0; it < 200; it++) {
        double JtJ[5][5] = { { 0 } }, Jtr[5] = { 0 }, A[25], d[5], trial[5], tcost = 0.0;
        for (q = 0; q < n; q++) {
            double const u = xs[q] - 0.5 - p[1], v = ys[q] - 0.5 - p[3];
            double const cu = cosh(p[0] * u), cv = cosh(p[2] * v), su = sinh(p[0] * u), sv = sinh(p[2] * v);
            double const r = 1.0 - cu * cv + p[4] - z[q];
            double const g[5] = { -u * su * cv, p[0] * su * cv, -v * cu * sv, p[2] * cu * sv, 1.0 };
            for (i = 0; i < 5; i++) {
                Jtr[i] += g[i] * r;
                for (j = 0; j <= i; j++) JtJ[i][j] += g[i] * g[j];
            }
        }
        for (i = 0; i < 5; i++)
            for (j = 0; j < i; j++) JtJ[j][i] = JtJ[i][j];
        for (;;) {
            for (i = 0; i < 5; i++) {
                for (j = 0; j < 5; j++) A[i * 5 + j] = JtJ[i][j];
                A[i * 5 + i] += lambda * (JtJ[i][i] > 0.0 ? JtJ[i][i] : 1.0);
                d[i] = -Jtr[i];
            }
            if (!alwan_vg_solve(A, d, 5)) break;
            for (i = 0; i < 5; i++) {
                double t = p[i] + d[i];
                trial[i] = t < lo[i] ? lo[i] : t > hi[i] ? hi[i] : t;
            }
            tcost = 0.0;
            for (q = 0; q < n; q++) {
                double const r = alwan_vg_hcosh(trial, xs[q], ys[q]) - z[q];
                tcost += r * r;
            }
            if (tcost <= cost) break;
            lambda *= 10.0;
            if (lambda > 1e12) break;
        }
        if (!(tcost <= cost)) break;
        {
            double step = 0.0;
            for (i = 0; i < 5; i++) step += fabs(trial[i] - p[i]), p[i] = trial[i];
            if (cost - tcost <= 1e-15 * (cost > 1e-300 ? cost : 1e-300) && step < 1e-14) break;
            cost = tcost;
        }
        lambda = lambda / 10.0 > 1e-12 ? lambda / 10.0 : 1e-12;
    }
}

/* ------------------------------------------------------------------------------------ */
/* The surface                                                                          */

static alwan_status alwan_vg_surface(double *out, size_t w, size_t h, size_t c, alwan_vignette const *v) {
    size_t x, y, i;
    if (v->method == ALWAN_VIGNETTE_BIVARIATE_SPLINE) {
        double *g = (double *)ALWAN_ALLOC(alwan_safe_array_size(v->gw * v->gh, sizeof(double)), sizeof(double));
        alwan_status st;
        if (!g) return ALWAN_E_NOMEM;
        for (i = 0; i < v->gw * v->gh; i++) g[i] = v->grid[i * v->ch + c];
        st = alwan_vg_resample(out, w, h, g, v->gw, v->gh);
        ALWAN_FREE(g);
        return st;
    }
    if (v->method == ALWAN_VIGNETTE_RBF) {
        double const *wt = v->w + c * (v->n + 3);
        double *tx = (double *)ALWAN_ALLOC(alwan_safe_array_size(w + h, sizeof(double)), sizeof(double)), *ty;
        if (!tx) return ALWAN_E_NOMEM;
        ty = tx + w;
        alwan_vg_linspace(tx, w);
        alwan_vg_linspace(ty, h);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                double const r0 = ty[y], c0 = tx[x];
                double s = 0.0;
                for (i = 0; i < v->n; i++) {
                    double const dr = r0 - v->pts[2 * i], dc = c0 - v->pts[2 * i + 1], d = sqrt(dr * dr + dc * dc);
                    s += wt[i] * (d * d * d);
                }
                s += wt[v->n] + wt[v->n + 1] * ((r0 - v->shift[0]) / v->scale[0]) + wt[v->n + 2] * ((c0 - v->shift[1]) / v->scale[1]);
                out[y * w + x] = s;
            }
        ALWAN_FREE(tx);
        return ALWAN_OK;
    }
    {
        double const *p = v->coef[c];
        double *tx = (double *)ALWAN_ALLOC(alwan_safe_array_size(w + h, sizeof(double)), sizeof(double)), *ty;
        if (!tx) return ALWAN_E_NOMEM;
        ty = tx + w;
        alwan_vg_linspace(tx, w);
        alwan_vg_linspace(ty, h);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                double const xx = tx[x] - v->ppx, yy = ty[y] - v->ppy;
                out[y * w + x] = v->method == ALWAN_VIGNETTE_PARABOLIC
                                     ? (p[0] * xx * xx + p[1] * xx) / 2.0 + (p[2] * yy * yy + p[3] * yy) / 2.0 + p[4]
                                     : alwan_vg_hcosh(p, xx, yy);
            }
        ALWAN_FREE(tx);
        return ALWAN_OK;
    }
}

/* ------------------------------------------------------------------------------------ */
/* The API                                                                              */

static double alwan_vg_read(void const *src, size_t rs, size_t x, size_t y, int kind) {
    char const *row = (char const *)src + y * rs;
    return kind == 0 ? ((alwan_f64 const *)row)[x] : kind == 1 ? (double)((alwan_f32 const *)row)[x]
                                                               : (double)((unsigned char const *)row)[x] / 255.0;
}

static alwan_status alwan_vg_characterise(alwan_vignette **out, void const *src, size_t rs, size_t ch, size_t w, size_t h,
                                          alwan_vignette_method method, alwan_vignette_params const *params, int kind) {
    alwan_vignette_params const zero = { 0 };
    alwan_vignette_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    double const threshold = p->threshold == 0.0 ? 0.99 : p->threshold;
    double const sigma = p->denoise_sigma == 0.0 ? 6.0 : p->denoise_sigma;
    double const post = p->post_denoise_sigma == 0.0 ? 1.0 : p->post_denoise_sigma;
    double const smoothing = p->smoothing == 0.0 ? 0.001 : p->smoothing;
    size_t const samples = p->samples ? p->samples : 50;
    size_t const n = w * h;
    alwan_vignette *v;
    double *a, *plane;
    size_t x, y, c;
    alwan_status st = ALWAN_OK;
    if (out) *out = NULL;
    if (!out || !src || w < 2 || h < 2 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (rs / elem / ch < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_VIGNETTE_RBF) return ALWAN_E_INVALID;
    if (!(threshold > 0.0 && threshold <= 1.0) || !(sigma > 0.0 && sigma <= 1000.0) || !(post > 0.0 && post <= 1000.0) ||
        !(smoothing >= 0.0) || smoothing - smoothing != 0.0 || samples < 4 || samples > 4096)
        return ALWAN_E_RANGE;
    v = (alwan_vignette *)ALWAN_ALLOC(sizeof(*v), sizeof(double));
    a = (double *)ALWAN_ALLOC(alwan_safe_array_size(2 * n * ch, sizeof(double)), sizeof(double));
    if (!v || !a) {
        ALWAN_FREE(v);
        ALWAN_FREE(a);
        return ALWAN_E_NOMEM;
    }
    memset(v, 0, sizeof(*v));
    v->method = method;
    v->ch = ch;
    plane = a + n * ch;
    for (y = 0; y < h; y++)
        for (x = 0; x < w * ch; x++) {
            double const val = alwan_vg_read(src, rs, x, y, kind);
            if (!(val - val == 0.0)) {
                ALWAN_FREE(v);
                ALWAN_FREE(a);
                return ALWAN_E_INVALID;
            }
            a[y * w * ch + x] = val;
        }
    alwan_vg_principal_point(&v->ppx, &v->ppy, a, w, h, ch, threshold);
    if (method == ALWAN_VIGNETTE_PARABOLIC || method == ALWAN_VIGNETTE_HYPERBOLIC_COSINE) {
        double *xs = (double *)ALWAN_ALLOC(alwan_safe_array_size(2 * n + w + h, sizeof(double)), sizeof(double)), *ys, *tx, *ty;
        if (!xs) st = ALWAN_E_NOMEM;
        else {
            ys = xs + n, tx = ys + n, ty = tx + w;
            alwan_vg_linspace(tx, w);
            alwan_vg_linspace(ty, h);
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) xs[y * w + x] = tx[x] - v->ppx, ys[y * w + x] = ty[y] - v->ppy;
            for (c = 0; c < ch && st == ALWAN_OK; c++) {
                size_t i;
                for (i = 0; i < n; i++) plane[i] = a[i * ch + c];
                if (method == ALWAN_VIGNETTE_PARABOLIC) {
                    /* the normal equations of [x^2/2, x/2, y^2/2, y/2, 1] */
                    double N[25] = { 0 }, b[5] = { 0 };
                    int r, q;
                    for (i = 0; i < n; i++) {
                        double const g[5] = { xs[i] * xs[i] / 2.0, xs[i] / 2.0, ys[i] * ys[i] / 2.0, ys[i] / 2.0, 1.0 };
                        for (r = 0; r < 5; r++) {
                            b[r] += g[r] * plane[i];
                            for (q = 0; q < 5; q++) N[r * 5 + q] += g[r] * g[q];
                        }
                    }
                    if (!alwan_vg_parabola_bounded(v->coef[c], N, b)) st = ALWAN_E_RANGE;
                } else {
                    alwan_vg_fit_hcosh(v->coef[c], plane, xs, ys, n);
                }
            }
            ALWAN_FREE(xs);
        }
    } else if (method == ALWAN_VIGNETTE_BIVARIATE_SPLINE) {
        double const ratio = (double)samples / (double)(w > h ? w : h);
        v->gw = (size_t)((double)w * ratio);
        v->gh = (size_t)((double)h * ratio);
        if (w < 4 || h < 4 || v->gw < 4 || v->gh < 4) st = ALWAN_E_RANGE;
        else {
            double *g = (double *)ALWAN_ALLOC(alwan_safe_array_size(v->gw * v->gh, sizeof(double)), sizeof(double));
            v->grid = (double *)ALWAN_ALLOC(alwan_safe_array_size(v->gw * v->gh * ch, sizeof(double)), sizeof(double));
            if (!g || !v->grid) st = ALWAN_E_NOMEM;
            for (c = 0; c < ch && st == ALWAN_OK; c++) {
                size_t i;
                for (i = 0; i < n; i++) plane[i] = a[i * ch + c];
                st = alwan_vg_gauss(plane, w, h, sigma, sigma, 0);
                if (st == ALWAN_OK) st = alwan_vg_resample(g, v->gw, v->gh, plane, w, h);
                if (st == ALWAN_OK) st = alwan_vg_gauss(g, v->gw, v->gh, post, sigma, 0);   /* truncated at sigma, as colour-hdri passes it */
                for (i = 0; st == ALWAN_OK && i < v->gw * v->gh; i++) v->grid[i * ch + c] = g[i];
            }
            ALWAN_FREE(g);
        }
    } else {
        /* RBF */
        size_t const m = 40 + 147;
        double *pts = (double *)ALWAN_ALLOC(alwan_safe_array_size(2 * m, sizeof(double)), sizeof(double));
        if (!pts) st = ALWAN_E_NOMEM;
        else {
            size_t const k = alwan_vg_rbf_coordinates(pts, v->ppy, v->ppx, (double)w / (double)h);
            size_t const P = k + 3;
            double *A = (double *)ALWAN_ALLOC(alwan_safe_array_size(P * P + P, sizeof(double)), sizeof(double));
            double mn[2] = { HUGE_VAL, HUGE_VAL }, mx[2] = { -HUGE_VAL, -HUGE_VAL };
            size_t i, j;
            v->n = k;
            v->pts = pts;
            v->w = (double *)ALWAN_ALLOC(alwan_safe_array_size(P * ch, sizeof(double)), sizeof(double));
            if (!A || !v->w) st = ALWAN_E_NOMEM;
            else {
                double *b = A + P * P;
                for (i = 0; i < k; i++)
                    for (j = 0; j < 2; j++) {
                        if (pts[2 * i + j] < mn[j]) mn[j] = pts[2 * i + j];
                        if (pts[2 * i + j] > mx[j]) mx[j] = pts[2 * i + j];
                    }
                for (j = 0; j < 2; j++) {
                    v->shift[j] = (mx[j] + mn[j]) / 2.0;
                    v->scale[j] = (mx[j] - mn[j]) / 2.0;
                    if (v->scale[j] == 0.0) v->scale[j] = 1.0;
                }
                for (c = 0; c < ch && st == ALWAN_OK; c++) {
                    for (i = 0; i < n; i++) plane[i] = a[i * ch + c];
                    st = alwan_vg_gauss(plane, w, h, sigma, sigma, 1);
                    if (st != ALWAN_OK) break;
                    for (i = 0; i < P * P; i++) A[i] = 0.0;
                    for (i = 0; i < k; i++) {
                        for (j = 0; j < k; j++) {
                            double const dr = pts[2 * i] - pts[2 * j], dc = pts[2 * i + 1] - pts[2 * j + 1], d = sqrt(dr * dr + dc * dc);
                            A[i * P + j] = d * d * d;
                        }
                        A[i * P + i] += smoothing;
                        A[i * P + k] = A[k * P + i] = 1.0;
                        A[i * P + k + 1] = A[(k + 1) * P + i] = (pts[2 * i] - v->shift[0]) / v->scale[0];
                        A[i * P + k + 2] = A[(k + 2) * P + i] = (pts[2 * i + 1] - v->shift[1]) / v->scale[1];
                        /* the sample: as_int_array truncates the fraction times (size - 1) */
                        b[i] = plane[(size_t)(pts[2 * i] * (double)(h - 1)) * w + (size_t)(pts[2 * i + 1] * (double)(w - 1))];
                    }
                    b[k] = b[k + 1] = b[k + 2] = 0.0;
                    if (!alwan_vg_solve(A, b, P)) st = ALWAN_E_RANGE;
                    else memcpy(v->w + c * P, b, P * sizeof(double));
                }
            }
            ALWAN_FREE(A);
        }
    }
    ALWAN_FREE(a);
    if (st != ALWAN_OK) {
        alwan_vignette_destroy(v, NULL);
        return st;
    }
    *out = v;
    return ALWAN_OK;
}

static alwan_status alwan_vg_correct(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                     alwan_vignette const *v, int kind) {
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    double *s;
    size_t x, y, c;
    alwan_status st = ALWAN_OK;
    if (!out || !src || !v || w < 2 || h < 2 || ch != v->ch || w * h / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if (v->method == ALWAN_VIGNETTE_BIVARIATE_SPLINE && (w < 4 || h < 4)) return ALWAN_E_RANGE;
    s = (double *)ALWAN_ALLOC(alwan_safe_array_size(w * h, sizeof(double)), sizeof(double));
    if (!s) return ALWAN_E_NOMEM;
    for (c = 0; c < ch && st == ALWAN_OK; c++) {
        st = alwan_vg_surface(s, w, h, c, v);
        for (y = 0; st == ALWAN_OK && y < h; y++) {
            char *orow = (char *)out + y * out_rs;
            for (x = 0; x < w; x++) {
                double const r = alwan_vg_read(src, src_rs, x * ch + c, y, kind) / s[y * w + x];
                if (kind == 0) ((alwan_f64 *)orow)[x * ch + c] = r;
                else if (kind == 1) ((alwan_f32 *)orow)[x * ch + c] = (alwan_f32)r;
                else {
                    double const t = floor(r * 255.0 + 0.5);
                    ((unsigned char *)orow)[x * ch + c] = (unsigned char)(t < 0.0 ? 0.0 : t > 255.0 ? 255.0 : t);
                }
            }
        }
    }
    ALWAN_FREE(s);
    return st;
}

alwan_status alwan_vignette_evaluate(double *out, size_t out_row_stride, size_t width, size_t height, size_t channel,
                                     alwan_vignette const *vignette) {
    double *s;
    size_t y;
    alwan_status st;
    if (!out || !vignette || width < 2 || height < 2 || channel >= vignette->ch || out_row_stride / sizeof(double) < width) return ALWAN_E_INVALID;
    if (vignette->method == ALWAN_VIGNETTE_BIVARIATE_SPLINE && (width < 4 || height < 4)) return ALWAN_E_RANGE;
    s = (double *)ALWAN_ALLOC(alwan_safe_array_size(width * height, sizeof(double)), sizeof(double));
    if (!s) return ALWAN_E_NOMEM;
    st = alwan_vg_surface(s, width, height, channel, vignette);
    for (y = 0; st == ALWAN_OK && y < height; y++) memcpy((char *)out + y * out_row_stride, s + y * width, width * sizeof(double));
    ALWAN_FREE(s);
    return st;
}

alwan_status alwan_vignette_principal_point(double *xy, alwan_vignette const *vignette) {
    if (!xy || !vignette) return ALWAN_E_INVALID;
    xy[0] = vignette->ppx;
    xy[1] = vignette->ppy;
    return ALWAN_OK;
}

void alwan_vignette_destroy(alwan_vignette *vignette, alwan_ctx *ctx) {
    (void)ctx;
    if (!vignette) return;
    ALWAN_FREE(vignette->grid);
    ALWAN_FREE(vignette->pts);
    ALWAN_FREE(vignette->w);
    ALWAN_FREE(vignette);
}

alwan_status alwan_vignette_characterise_u8(alwan_vignette **out, unsigned char const *flat, size_t row_stride, size_t channels, size_t width,
                                            size_t height, alwan_vignette_method method, alwan_vignette_params const *params, alwan_ctx *ctx) {
    (void)ctx;
    return alwan_vg_characterise(out, flat, row_stride, channels, width, height, method, params, 2);
}

alwan_status alwan_vignette_correct_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src, size_t src_row_stride,
                                       size_t channels, size_t width, size_t height, alwan_vignette const *vignette) {
    return alwan_vg_correct(out, out_row_stride, src, src_row_stride, channels, width, height, vignette, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_vignette_characterise_f64(alwan_vignette **out, alwan_f64 const *flat, size_t row_stride, size_t channels, size_t width,
                                             size_t height, alwan_vignette_method method, alwan_vignette_params const *params, alwan_ctx *ctx) {
    (void)ctx;
    return alwan_vg_characterise(out, flat, row_stride, channels, width, height, method, params, 0);
}

alwan_status alwan_vignette_correct_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels,
                                        size_t width, size_t height, alwan_vignette const *vignette) {
    return alwan_vg_correct(out, out_row_stride, src, src_row_stride, channels, width, height, vignette, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_vignette_characterise_f32(alwan_vignette **out, alwan_f32 const *flat, size_t row_stride, size_t channels, size_t width,
                                             size_t height, alwan_vignette_method method, alwan_vignette_params const *params, alwan_ctx *ctx) {
    (void)ctx;
    return alwan_vg_characterise(out, flat, row_stride, channels, width, height, method, params, 1);
}

alwan_status alwan_vignette_correct_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels,
                                        size_t width, size_t height, alwan_vignette const *vignette) {
    return alwan_vg_correct(out, out_row_stride, src, src_row_stride, channels, width, height, vignette, 1);
}
#endif
