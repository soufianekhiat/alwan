/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Fitting an ellipse to scattered points, and moving between the two ways of
 * writing one down.
 *
 * Halir and Flusser (1998), "Numerically Stable Direct Least Squares Fitting
 * of Ellipses". The direct method of Fitzgibbon, Pilu and Fisher constrains
 * the fit to an ellipse rather than any conic, so the answer cannot come back
 * a hyperbola no matter how the points are spread; Halir and Flusser's
 * contribution is to split the scatter matrix so the eigenproblem is 3x3 and
 * well conditioned instead of 6x6 and nearly singular. That split is the whole
 * algorithm and it is what is implemented here.
 *
 * Two forms of the same ellipse:
 *
 *   general    a x^2 + b x y + c y^2 + d x + e y + f = 0, six coefficients,
 *              which is what the fit produces and what composes linearly
 *   canonical  centre, the two semi-axes, and the rotation in degrees, which
 *              is what a reader wants and what a plot needs
 *
 * THE SIX COEFFICIENTS HAVE NO NATURAL SCALE. Multiplying all of them by any
 * non-zero constant is the same ellipse, so a fit that returned whatever its
 * eigenvector solver happened to produce would be reproducible only against
 * that solver. They come back here with unit 2-norm and a > 0, which is
 * well defined for an ellipse: 4ac > b^2 forces a and c to share a sign, so
 * neither is ever zero. The canonical form is unaffected either way, and that
 * is what suite 165 pins against colour-science.
 *
 * Reference: colour-science's colour.geometry.ellipse_fitting_Halir1998,
 * ellipse_coefficients_canonical_form and ellipse_coefficients_general_form.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>
#include <math.h>

/* ---------------------------------------------------------------- cubic */

/* The real roots of x^3 + p2 x^2 + p1 x + p0, by the trigonometric method for
 * three real roots and Cardano's for one. Returns how many it found, and they
 * are not sorted. Written here rather than reached for: it is arithmetic with
 * a closed form, not a model anything could be a reference for. */
static int alwan__cubic_real_roots(double p2, double p1, double p0, double *roots) {
    double const shift = p2 / 3.0;
    /* Depressed cubic t^3 + p t + q, with x = t - p2 / 3. */
    double const p = p1 - p2 * p2 / 3.0;
    double const q = (2.0 * p2 * p2 * p2) / 27.0 - (p2 * p1) / 3.0 + p0;
    double const disc = (q * q) / 4.0 + (p * p * p) / 27.0;

    if (p > -1e-300 && p < 1e-300 && q > -1e-300 && q < 1e-300) {
        roots[0] = -shift;
        return 1;
    }
    if (disc > 0.0) {
        double const s = sqrt(disc);
        double const u = -q / 2.0 + s;
        double const v = -q / 2.0 - s;
        double const cu = (u >= 0.0) ? pow(u, 1.0 / 3.0) : -pow(-u, 1.0 / 3.0);
        double const cv = (v >= 0.0) ? pow(v, 1.0 / 3.0) : -pow(-v, 1.0 / 3.0);
        roots[0] = cu + cv - shift;
        return 1;
    }
    {
        /* Three real roots: p is negative here, so the arc cosine is defined. */
        double const m = 2.0 * sqrt(-p / 3.0);
        double arg = 3.0 * q / (p * m);
        double phi;
        int i;
        if (arg > 1.0) arg = 1.0;
        if (arg < -1.0) arg = -1.0;
        phi = acos(arg) / 3.0;
        for (i = 0; i < 3; i++) {
            roots[i] = m * cos(phi - 2.0 * 3.14159265358979323846 * (double)i / 3.0) - shift;
        }
        return 3;
    }
}

/* A null vector of a 3x3 matrix known to be singular: the cross product of two
 * of its rows, taking whichever pair gives the longest result, since a pair
 * that is nearly parallel carries no direction worth using. Returns 0 when
 * every pair is degenerate, which means the matrix has rank below two. */
static int alwan__null_vector3(double const *m, double *v) {
    static int const pair[3][2] = { { 0, 1 }, { 1, 2 }, { 0, 2 } };
    double best = 0.0;
    int k, found = 0;
    for (k = 0; k < 3; k++) {
        double const *a = m + pair[k][0] * 3;
        double const *b = m + pair[k][1] * 3;
        double const x = a[1] * b[2] - a[2] * b[1];
        double const y = a[2] * b[0] - a[0] * b[2];
        double const z = a[0] * b[1] - a[1] * b[0];
        double const len = sqrt(x * x + y * y + z * z);
        if (len > best) {
            best = len;
            v[0] = x / len;
            v[1] = y / len;
            v[2] = z / len;
            found = 1;
        }
    }
    return found;
}

/* ---------------------------------------------------------------- fit */

/* THE POINTS ARE CENTRED AND SCALED BEFORE THE FIT, and the conic is mapped
 * back afterwards. This is not a refinement, it is what makes the routine work
 * on the data it exists for. A MacAdam 1942 ellipse is about 1e-3 across at a
 * chromaticity near 0.19, so x^2, xy and y^2 vary by parts in ten thousand
 * across the whole point set: the design matrix columns are nearly constant,
 * the scatter matrix is nearly rank deficient, and the eigen step has nothing
 * left to resolve. Measured on MacAdam ellipse 1, which is 2.2e-3 by 5.5e-4 at
 * (0.187, 0.118): without this, no eigenvector satisfies the ellipse
 * constraint at all and the fit reports ALWAN_E_RANGE. colour-science, which
 * does not normalise, finds TWO there and returns twelve coefficients, which
 * its own canonical-form conversion then cannot unpack. With the points
 * centred and scaled, the ellipse comes back to 1e-9.
 *
 * The substitution is exact. Fitting in u = (x - cx) / s and v = (y - cy) / s
 * gives A' u^2 + B' u v + C' v^2 + D' u + E' v + F' = 0, and multiplying
 * through by s^2 after substituting back gives the coefficients in x and y. */

/* The scatter matrices, accumulated in double whatever the caller's precision.
 * S1 = D1' D1, S2 = D1' D2, S3 = D2' D2 with D1 = [x^2, xy, y^2] and
 * D2 = [x, y, 1], which is Halir and Flusser's split of the design matrix. */
static void alwan__ellipse_scatter(double *s1, double *s2, double *s3,
                                   double x, double y) {
    double const d1[3] = { x * x, x * y, y * y };
    double const d2[3] = { x, y, 1.0 };
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            s1[i * 3 + j] += d1[i] * d1[j];
            s2[i * 3 + j] += d1[i] * d2[j];
            s3[i * 3 + j] += d2[i] * d2[j];
        }
    }
}

static alwan_status alwan__ellipse_solve(double *out, double const *s1,
                                         double const *s2, double const *s3) {
    alwan_mat3x3_f64 s3m, s3i;
    double t[9], m[9], reduced[9];
    double roots[3], a1[3], a2[3];
    double norm = 0.0;
    int i, j, k, n_roots, picked = -1;

    memcpy(s3m.m, s3, sizeof(double) * 9);
    if (alwan_mat3_inv_f64(&s3i, &s3m) != ALWAN_OK) return ALWAN_E_RANGE;

    /* T = -S3^-1 S2', so that the linear coefficients follow from the
     * quadratic ones and the eigenproblem shrinks to 3x3. */
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            double sum = 0.0;
            for (k = 0; k < 3; k++) sum += s3i.m[i * 3 + k] * s2[j * 3 + k];
            t[i * 3 + j] = -sum;
        }
    }

    /* M = S1 + S2 T, then the rows permuted and halved as the paper's
     * constraint matrix requires. */
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            double sum = s1[i * 3 + j];
            for (k = 0; k < 3; k++) sum += s2[i * 3 + k] * t[k * 3 + j];
            m[i * 3 + j] = sum;
        }
    }
    for (j = 0; j < 3; j++) {
        reduced[0 * 3 + j] = m[2 * 3 + j] / 2.0;
        reduced[1 * 3 + j] = -m[1 * 3 + j];
        reduced[2 * 3 + j] = m[0 * 3 + j] / 2.0;
    }

    {
        double const tr = reduced[0] + reduced[4] + reduced[8];
        double const minors =
            reduced[0] * reduced[4] - reduced[1] * reduced[3] +
            reduced[0] * reduced[8] - reduced[2] * reduced[6] +
            reduced[4] * reduced[8] - reduced[5] * reduced[7];
        double const det =
            reduced[0] * (reduced[4] * reduced[8] - reduced[5] * reduced[7]) -
            reduced[1] * (reduced[3] * reduced[8] - reduced[5] * reduced[6]) +
            reduced[2] * (reduced[3] * reduced[7] - reduced[4] * reduced[6]);
        n_roots = alwan__cubic_real_roots(-tr, minors, -det, roots);
    }

    /* Exactly one eigenvector satisfies 4 a c > b^2, which is what makes the
     * fit an ellipse rather than any conic. */
    for (i = 0; i < n_roots; i++) {
        double shifted[9];
        double v[3];
        for (j = 0; j < 9; j++) shifted[j] = reduced[j];
        shifted[0] -= roots[i];
        shifted[4] -= roots[i];
        shifted[8] -= roots[i];
        if (!alwan__null_vector3(shifted, v)) continue;
        if (4.0 * v[0] * v[2] - v[1] * v[1] > 0.0) {
            a1[0] = v[0]; a1[1] = v[1]; a1[2] = v[2];
            picked = i;
            break;
        }
    }
    if (picked < 0) return ALWAN_E_RANGE;      /* the points do not fit an ellipse */

    for (i = 0; i < 3; i++) {
        a2[i] = t[i * 3 + 0] * a1[0] + t[i * 3 + 1] * a1[1] + t[i * 3 + 2] * a1[2];
    }
    out[0] = a1[0]; out[1] = a1[1]; out[2] = a1[2];
    out[3] = a2[0]; out[4] = a2[1]; out[5] = a2[2];

    /* Unit 2-norm with a > 0: the six have no natural scale, and a fit whose
     * sign depended on an eigen solver would only be reproducible against that
     * solver. 4ac > b^2 forces a and c to share a sign, so a is never zero. */
    for (i = 0; i < 6; i++) norm += out[i] * out[i];
    norm = sqrt(norm);
    if (!(norm > 0.0)) return ALWAN_E_RANGE;
    if (out[0] < 0.0) norm = -norm;
    for (i = 0; i < 6; i++) out[i] /= norm;
    return ALWAN_OK;
}

/* Undo the normalisation: the conic fitted in (u, v) written in (x, y). */
static void alwan__ellipse_denormalise(double *k, double cx, double cy, double s) {
    double const a = k[0], b = k[1], c = k[2], d = k[3], e = k[4], f = k[5];
    k[0] = a;
    k[1] = b;
    k[2] = c;
    k[3] = -2.0 * a * cx - b * cy + s * d;
    k[4] = -b * cx - 2.0 * c * cy + s * e;
    k[5] = a * cx * cx + b * cx * cy + c * cy * cy - s * d * cx - s * e * cy + s * s * f;
}

static void alwan__ellipse_renormalise(double *k) {
    double norm = 0.0;
    int i;
    for (i = 0; i < 6; i++) norm += k[i] * k[i];
    norm = sqrt(norm);
    if (!(norm > 0.0)) return;
    if (k[0] < 0.0) norm = -norm;
    for (i = 0; i < 6; i++) k[i] /= norm;
}

/* One fit, reading the points through a caller-supplied getter so neither
 * precision needs a scratch copy of them. */
typedef void (*alwan__point_reader)(void const *points, size_t i, double *xy);

static void alwan__read_point_f64(void const *points, size_t i, double *xy) {
    alwan_vec2_f64 const *p = (alwan_vec2_f64 const *)points;
    xy[0] = p[i].v[0];
    xy[1] = p[i].v[1];
}

static void alwan__read_point_f32(void const *points, size_t i, double *xy) {
    alwan_vec2_f32 const *p = (alwan_vec2_f32 const *)points;
    xy[0] = (double)p[i].v[0];
    xy[1] = (double)p[i].v[1];
}

static alwan_status alwan__ellipse_fit(double *out, void const *points, size_t count,
                                       alwan__point_reader read) {
    double s1[9], s2[9], s3[9];
    double cx = 0.0, cy = 0.0, scale = 0.0;
    alwan_status st;
    size_t i;

    /* Five points determine a conic, and fewer leave the scatter matrix
     * singular rather than merely under-determined. */
    if (count < 5) return ALWAN_E_RANGE;

    for (i = 0; i < count; i++) {
        double xy[2];
        read(points, i, xy);
        if (!(xy[0] == xy[0]) || !(xy[1] == xy[1])) return ALWAN_E_INVALID;
        cx += xy[0];
        cy += xy[1];
    }
    cx /= (double)count;
    cy /= (double)count;
    for (i = 0; i < count; i++) {
        double xy[2], dx, dy;
        read(points, i, xy);
        dx = xy[0] - cx;
        dy = xy[1] - cy;
        scale += dx * dx + dy * dy;
    }
    scale = sqrt(scale / (double)count);
    if (!(scale > 0.0)) return ALWAN_E_RANGE;      /* every point the same */

    memset(s1, 0, sizeof s1);
    memset(s2, 0, sizeof s2);
    memset(s3, 0, sizeof s3);
    for (i = 0; i < count; i++) {
        double xy[2];
        read(points, i, xy);
        alwan__ellipse_scatter(s1, s2, s3, (xy[0] - cx) / scale, (xy[1] - cy) / scale);
    }
    st = alwan__ellipse_solve(out, s1, s2, s3);
    if (st != ALWAN_OK) return st;
    alwan__ellipse_denormalise(out, cx, cy, scale);
    alwan__ellipse_renormalise(out);
    return ALWAN_OK;
}

alwan_status alwan_ellipse_fit_halir1998_f64(alwan_f64 *coefficients_out,
                                             alwan_vec2_f64 const *points, size_t count) {
    if (!coefficients_out || !points) return ALWAN_E_INVALID;
    return alwan__ellipse_fit(coefficients_out, points, count, alwan__read_point_f64);
}

alwan_status alwan_ellipse_fit_halir1998_f32(alwan_f32 *coefficients_out,
                                             alwan_vec2_f32 const *points, size_t count) {
    double out[6];
    alwan_status st;
    int i;
    if (!coefficients_out || !points) return ALWAN_E_INVALID;
    st = alwan__ellipse_fit(out, points, count, alwan__read_point_f32);
    if (st == ALWAN_OK) {
        for (i = 0; i < 6; i++) coefficients_out[i] = (alwan_f32)out[i];
    }
    return st;
}

/* ---------------------------------------------------------------- forms */

static alwan_status alwan__ellipse_canonical(double *out, double const *k) {
    double const a = k[0], b = k[1], c = k[2], d = k[3], e = k[4], f = k[5];
    double const d1 = b * b - 4.0 * a * c;
    double n1, n2;
    if (!(d1 < 0.0)) return ALWAN_E_RANGE;          /* not an ellipse */
    n1 = 2.0 * (a * e * e + c * d * d - b * d * e + d1 * f);
    n2 = sqrt((a - c) * (a - c) + b * b);
    if (!(n1 * (a + c + n2) >= 0.0) || !(n1 * (a + c - n2) >= 0.0)) return ALWAN_E_RANGE;
    out[0] = (2.0 * c * d - b * e) / d1;
    out[1] = (2.0 * a * e - b * d) / d1;
    out[2] = -sqrt(n1 * (a + c + n2)) / d1;
    out[3] = -sqrt(n1 * (a + c - n2)) / d1;
    /* A CIRCLE HAS NO ROTATION, and asking for one gives a garbage answer
     * rather than an error. n2 is what separates the two axes: when it is
     * negligible against a + c the ellipse is a circle to within rounding, the
     * angle carries no information, and the arc tangent below would be a ratio
     * of two quantities that are both noise. Points generated on a circle leave
     * b at about 1e-16 instead of exactly 0, which is enough to miss a test for
     * zero and land in that branch: measured, it returned -9.43 degrees for a
     * circle. Zero is the conventional answer and the one colour-science's own
     * selection falls through to. */
    if (n2 <= 1e-12 * (fabs(a) + fabs(c))) {
        out[4] = 0.0;
    } else if (b == 0.0) {
        out[4] = (a < c) ? 0.0 : 90.0;
    } else {
        out[4] = atan((c - a - n2) / b) * (180.0 / 3.14159265358979323846);
    }
    return ALWAN_OK;
}

static void alwan__ellipse_general(double *out, double const *k) {
    double const theta = k[4] * (3.14159265358979323846 / 180.0);
    double const ct = cos(theta), st = sin(theta);
    double const aa = k[2] * k[2], bb = k[3] * k[3];
    double const a = aa * st * st + bb * ct * ct;
    double const b = 2.0 * (bb - aa) * st * ct;
    double const c = aa * ct * ct + bb * st * st;
    out[0] = a;
    out[1] = b;
    out[2] = c;
    out[3] = -2.0 * a * k[0] - b * k[1];
    out[4] = -b * k[0] - 2.0 * c * k[1];
    out[5] = a * k[0] * k[0] + b * k[0] * k[1] + c * k[1] * k[1] - aa * bb;
}

alwan_status alwan_ellipse_canonical_f64(alwan_f64 *canonical_out, alwan_f64 const *coefficients) {
    if (!canonical_out || !coefficients) return ALWAN_E_INVALID;
    return alwan__ellipse_canonical(canonical_out, coefficients);
}

alwan_status alwan_ellipse_canonical_f32(alwan_f32 *canonical_out, alwan_f32 const *coefficients) {
    double k[6], out[5];
    alwan_status st;
    int i;
    if (!canonical_out || !coefficients) return ALWAN_E_INVALID;
    for (i = 0; i < 6; i++) k[i] = (double)coefficients[i];
    st = alwan__ellipse_canonical(out, k);
    if (st == ALWAN_OK) {
        for (i = 0; i < 5; i++) canonical_out[i] = (alwan_f32)out[i];
    }
    return st;
}

alwan_status alwan_ellipse_general_f64(alwan_f64 *coefficients_out, alwan_f64 const *canonical) {
    if (!coefficients_out || !canonical) return ALWAN_E_INVALID;
    alwan__ellipse_general(coefficients_out, canonical);
    return ALWAN_OK;
}

alwan_status alwan_ellipse_general_f32(alwan_f32 *coefficients_out, alwan_f32 const *canonical) {
    double k[5], out[6];
    int i;
    if (!coefficients_out || !canonical) return ALWAN_E_INVALID;
    for (i = 0; i < 5; i++) k[i] = (double)canonical[i];
    alwan__ellipse_general(out, k);
    for (i = 0; i < 6; i++) coefficients_out[i] = (alwan_f32)out[i];
    return ALWAN_OK;
}

/* Points on a canonical ellipse at angles in degrees, as colour-science's
 * point_at_angle_on_ellipse: x = xc + a cos(t) cos(p) - b sin(t) sin(p),
 * y = yc + a sin(t) cos(p) + b cos(t) sin(p), with t the rotation. */
static void alwan__ellipse_point(double xy[2], double const c[5], double phi_deg) {
    double const phi = phi_deg * (3.14159265358979323846 / 180.0);
    double const theta = c[4] * (3.14159265358979323846 / 180.0);
    double const cp = cos(phi), sp = sin(phi), ct = cos(theta), st = sin(theta);
    xy[0] = c[0] + c[2] * ct * cp - c[3] * st * sp;
    xy[1] = c[1] + c[2] * st * cp + c[3] * ct * sp;
}

alwan_status alwan_ellipse_points_f64(alwan_vec2_f64 *points_out, alwan_f64 const *canonical,
                                      alwan_f64 const *angles_deg, size_t count) {
    size_t i;
    if (!points_out || !canonical || !angles_deg || count == 0) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        double xy[2];
        alwan__ellipse_point(xy, canonical, angles_deg[i]);
        points_out[i].v[0] = xy[0];
        points_out[i].v[1] = xy[1];
    }
    return ALWAN_OK;
}

alwan_status alwan_ellipse_points_f32(alwan_vec2_f32 *points_out, alwan_f32 const *canonical,
                                      alwan_f32 const *angles_deg, size_t count) {
    double c[5];
    size_t i;
    int k;
    if (!points_out || !canonical || !angles_deg || count == 0) return ALWAN_E_INVALID;
    for (k = 0; k < 5; k++) c[k] = (double)canonical[k];
    for (i = 0; i < count; i++) {
        double xy[2];
        alwan__ellipse_point(xy, c, (double)angles_deg[i]);
        points_out[i].v[0] = (alwan_f32)xy[0];
        points_out[i].v[1] = (alwan_f32)xy[1];
    }
    return ALWAN_OK;
}
