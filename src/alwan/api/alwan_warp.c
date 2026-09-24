/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_warp_{T} and _u8: an image resampled through an affine or perspective map, as
 * Pillow's Image.transform computes it (libImaging Geometry.c), which suite 234 holds this
 * to.
 *
 * Every output pixel's centre (x + 0.5, y + 0.5) is carried into the source by the
 * coefficients a: affine, (a0 x + a1 y + a2, a3 x + a4 y + a5); perspective, both divided
 * by a6 x + a7 y + 1. The source is sampled there:
 *
 *   NEAREST   the pixel under the point; for affine maps Pillow's three routes are
 *             followed: a pure scale by its pretabulated running sums, a rotation in its
 *             16.16 fixed point while every corner maps inside +-32768, otherwise its
 *             running sums in double
 *   BILINEAR  the four pixels around it, the columns clamped at the edges and a missing
 *             lower row taken as the upper
 *   BICUBIC   the sixteen around it by the Catmull-Rom cubic (a = -0.5) in Pillow's
 *             Horner form, the same edge rules
 *
 * A point outside the image leaves the output at `fill`. 8-bit results are truncated
 * (bicubic clamped first), float32 results stored from double, as Pillow's are.
 *
 * Beyond Pillow: the map may also be a swirl, a sampled field of source points or a
 * caller's function, and each output pixel may be integrated rather than point-sampled
 * (output-space subpixel integration): the pixel's sub-positions are offset BEFORE the map,
 * each carried to the source and reconstructed there, and the results averaged, so a map
 * that is far from linear inside one pixel (a swirl's centre, a lens, a displacement) is
 * averaged through its actual shape rather than through a linear footprint.
 *
 *   GRID      an n x n grid of sub-positions over the pixel: the brute-force reference
 *   R2        n points of the R2 sequence on the disk of the pixel's area, in antithetic
 *             pairs (+d, -d, so the mean offset is exactly zero), turned by a hashed angle
 *             per pixel so neighbouring pixels do not share one pattern
 *   ADAPTIVE  the map's footprint (a finite-difference Jacobian at half a pixel) and its
 *             departure from linear (second differences), both in source pixels, choose
 *             one point where the map is locally linear and within a pixel, otherwise 4 to
 *             64 R2 points, enough to cover the footprint's area and its curvature
 *
 * An integrated result is the mean in the data's own values (8-bit rounded to nearest):
 * integrate in linear light, and with alpha_channel set the colour is integrated
 * premultiplied by the last channel.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#define ALWAN_WP_COORD(v) ((v) < 0.0 ? -1 : (long)(v))
#define ALWAN_WP_FLOOR(v) ((v) < 0.0 ? (long)floor(v) : (long)(v))

typedef struct {
    void const *src;
    size_t rs, ch, w, h;
    int kind;
} alwan_wp_img;

static double alwan_wp_px(alwan_wp_img const *im, long x, long y, size_t c) {
    char const *row = (char const *)im->src + (size_t)y * im->rs;
    size_t const i = (size_t)x * im->ch + c;
    return im->kind == 0 ? ((alwan_f64 const *)row)[i] : im->kind == 1 ? (double)((alwan_f32 const *)row)[i]
                                                                       : (double)((unsigned char const *)row)[i];
}

static long alwan_wp_xclip(alwan_wp_img const *im, long x) {
    return x < 0 ? 0 : x < (long)im->w ? x : (long)im->w - 1;
}

static void alwan_wp_store(void *out, int kind, size_t i, double v, int cubic) {
    if (kind == 0) ((alwan_f64 *)out)[i] = v;
    else if (kind == 1) ((alwan_f32 *)out)[i] = (alwan_f32)v;
    else if (cubic) ((unsigned char *)out)[i] = (unsigned char)(v <= 0.0 ? 0.0 : v >= 255.0 ? 255.0 : v);
    else ((unsigned char *)out)[i] = (unsigned char)v;
}

static double alwan_wp_r32(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

/* Pillow's BICUBIC macro. On float32 pixels (f32 set) its p2, p3 and p4 are C float
 * arithmetic, the operands being FLOAT32; the Horner step with the double d is double. */
static double alwan_wp_cubic(double v1, double v2, double v3, double v4, double d, int f32) {
    double const p1 = v2;
    double const p2 = alwan_wp_r32(-v1 + v3, f32);
    double const p3 = alwan_wp_r32(alwan_wp_r32(alwan_wp_r32(2 * alwan_wp_r32(v1 - v2, f32) + v3, f32), f32) - v4, f32);
    double const p4 = alwan_wp_r32(alwan_wp_r32(alwan_wp_r32(-v1 + v2, f32) - v3, f32) + v4, f32);
    return p1 + d * (p2 + d * (p3 + d * p4));
}

/* Pillow's bilinear and bicubic filters, evaluated into v[channel] (not yet stored): 0 when
 * (xin, yin) is outside, else 1. */
static int alwan_wp_eval(double *vout, alwan_wp_img const *im, double xin, double yin, int cubic) {
    long x, y;
    double dx, dy;
    size_t c;
    int const f32 = im->kind == 1;
    if (xin < 0.0 || xin >= (double)im->w || yin < 0.0 || yin >= (double)im->h) return 0;
    xin -= 0.5;
    yin -= 0.5;
    x = ALWAN_WP_FLOOR(xin);
    y = ALWAN_WP_FLOOR(yin);
    dx = xin - (double)x;
    dy = yin - (double)y;
    if (!cubic) {
        long const x0 = alwan_wp_xclip(im, x), x1 = alwan_wp_xclip(im, x + 1);
        long const yc = y < 0 ? 0 : y < (long)im->h ? y : (long)im->h - 1;
        for (c = 0; c < im->ch; c++) {
            double v1, v2;
            double a = alwan_wp_px(im, x0, yc, c), b = alwan_wp_px(im, x1, yc, c);
            /* on float32 pixels (b - a) is float arithmetic, as in Pillow's BILINEAR macro */
            v1 = a + alwan_wp_r32(b - a, f32) * dx;
            if (y + 1 >= 0 && y + 1 < (long)im->h) {
                a = alwan_wp_px(im, x0, y + 1, c);
                b = alwan_wp_px(im, x1, y + 1, c);
                v2 = a + alwan_wp_r32(b - a, f32) * dx;
            } else {
                v2 = v1;
            }
            vout[c] = v1 + (v2 - v1) * dy;
        }
    } else {
        long xs[4], k;
        long const yc = (y - 1) < 0 ? 0 : (y - 1) < (long)im->h ? (y - 1) : (long)im->h - 1;
        x--;
        y--;
        for (k = 0; k < 4; k++) xs[k] = alwan_wp_xclip(im, x + k);
        for (c = 0; c < im->ch; c++) {
            double v[4];
            v[0] = alwan_wp_cubic(alwan_wp_px(im, xs[0], yc, c), alwan_wp_px(im, xs[1], yc, c), alwan_wp_px(im, xs[2], yc, c),
                                  alwan_wp_px(im, xs[3], yc, c), dx, f32);
            for (k = 1; k < 4; k++) {
                if (y + k >= 0 && y + k < (long)im->h) {
                    v[k] = alwan_wp_cubic(alwan_wp_px(im, xs[0], y + k, c), alwan_wp_px(im, xs[1], y + k, c), alwan_wp_px(im, xs[2], y + k, c),
                                          alwan_wp_px(im, xs[3], y + k, c), dx, f32);
                } else {
                    v[k] = v[k - 1];
                }
            }
            vout[c] = alwan_wp_cubic(v[0], v[1], v[2], v[3], dy, 0);
        }
    }
    return 1;
}

/* The filter sampled and stored as Pillow stores it (8-bit truncated, bicubic clamped). */
static int alwan_wp_sample(void *orow, size_t ox, alwan_wp_img const *im, double xin, double yin, int cubic) {
    double v[4];
    size_t c;
    if (!alwan_wp_eval(v, im, xin, yin, cubic)) return 0;
    for (c = 0; c < im->ch; c++) alwan_wp_store(orow, im->kind, ox * im->ch + c, v[c], cubic);
    return 1;
}

static void alwan_wp_copy_px(void *orow, size_t ox, alwan_wp_img const *im, long x, long y) {
    size_t const elem = im->kind == 0 ? sizeof(alwan_f64) : im->kind == 1 ? sizeof(alwan_f32) : 1u;
    memcpy((char *)orow + ox * im->ch * elem, (char const *)im->src + (size_t)y * im->rs + (size_t)x * im->ch * elem, im->ch * elem);
}

static int alwan_wp_check_fixed(double const a[6], long x, long y) {
    return fabs((double)x * a[0] + (double)y * a[1] + a[2]) < 32768.0 && fabs((double)x * a[3] + (double)y * a[4] + a[5]) < 32768.0;
}

static long alwan_wp_fix(double v) {
    double const t = v * 65536.0 + 0.5;
    return ALWAN_WP_FLOOR(t);
}

/* ------------------------------------------------------------------------------------ */
/* Maps other than the matrix, and output-space integration                             */

typedef struct {
    alwan_warp_params const *p;
    double a[8];
    double cx, cy, radius, angle;
    size_t ow, oh, gw, gh;
} alwan_wp_map;

static double const *alwan_wp_texel(alwan_wp_map const *m, long i, long j) {
    return (double const *)((char const *)m->p->field + (size_t)j * m->p->field_row_stride) + 2 * (size_t)i;
}

/* Lattice point (i, j), continued beyond the lattice along the edge's slope, each way in
 * turn: a map that is affine near its edge stays affine past it. */
static void alwan_wp_tex(alwan_wp_map const *m, long i, long j, double *t) {
    long const gw = (long)m->gw, gh = (long)m->gh;
    long const ic = i < 0 ? 0 : i >= gw ? gw - 1 : i, jc = j < 0 ? 0 : j >= gh ? gh - 1 : j;
    long const in = gw < 2 ? ic : (i < 0 ? 1 : gw - 2), jn = gh < 2 ? jc : (j < 0 ? 1 : gh - 2);
    double const di = (double)(i - ic), dj = (double)(j - jc);   /* how far past the edge, signed */
    double const *a = alwan_wp_texel(m, ic, jc);
    int k;
    for (k = 0; k < 2; k++) {
        double v = a[k];
        if (di != 0.0 && gw > 1) v += di * (a[k] - alwan_wp_texel(m, in, jc)[k]) * (i < 0 ? -1.0 : 1.0);
        if (dj != 0.0 && gh > 1) {
            /* the neighbouring row, continued along x the same way */
            double const *b = alwan_wp_texel(m, ic, jn);
            double w = b[k];
            if (di != 0.0 && gw > 1) w += di * (b[k] - alwan_wp_texel(m, in, jn)[k]) * (i < 0 ? -1.0 : 1.0);
            v += dj * (v - w) * (j < 0 ? -1.0 : 1.0);
        }
        t[k] = v;
    }
}

/* Knot i of the clamped uniform knot vector for n points of degree d. */
static double alwan_wp_knot(long i, long n, long d) {
    return i <= d ? 0.0 : i >= n ? 1.0 : (double)(i - d) / (double)(n - d);
}

/* The span holding t and the d + 1 B-spline basis values there (Piegl and Tiller, The NURBS
 * Book, A2.1 and A2.2), on the clamped uniform knots. */
static long alwan_wp_basis(double t, long n, long d, double *N) {
    double left[8], right[8];
    long idx, span, j, r;
    t = t < 0.0 ? 0.0 : t > 1.0 ? 1.0 : t;
    idx = (long)floor(t * (double)(n - d));
    if (idx > n - d - 1) idx = n - d - 1;
    span = d + idx;
    N[0] = 1.0;
    for (j = 1; j <= d; j++) {
        double saved = 0.0;
        left[j] = t - alwan_wp_knot(span + 1 - j, n, d);
        right[j] = alwan_wp_knot(span + j, n, d) - t;
        for (r = 0; r < j; r++) {
            double const temp = N[r] / (right[r + 1] + left[j - r]);
            N[r] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        N[j] = saved;
    }
    return span;
}

/* The field's source point at output point (x, y). */
static int alwan_wp_field(alwan_wp_map const *m, double x, double y, double *sx, double *sy) {
    alwan_warp_params const *p = m->p;
    if (p->field_interpolation == ALWAN_WARP_FIELD_NURBS) {
        long const d = p->field_degree ? p->field_degree : 3, nu = (long)m->gw, nv = (long)m->gh;
        double Nu[8], Nv[8], ax = 0.0, ay = 0.0, aw = 0.0;
        long const su = alwan_wp_basis(x / (double)m->ow, nu, d, Nu), sv = alwan_wp_basis(y / (double)m->oh, nv, d, Nv);
        long a, b;
        for (b = 0; b <= d; b++)
            for (a = 0; a <= d; a++) {
                long const i = su - d + a, j = sv - d + b;
                double const *t = alwan_wp_texel(m, i, j);
                double const wt = p->field_weights ? ((double const *)((char const *)p->field_weights + (size_t)j * p->field_weights_row_stride))[i] : 1.0;
                double const k = Nu[a] * Nv[b] * wt;
                ax += k * t[0];
                ay += k * t[1];
                aw += k;
            }
        *sx = ax / aw;
        *sy = ay / aw;
    } else {
        /* the lattice coordinate: texel centres; exact when the lattice is the output's size */
        double const fx = m->gw == m->ow ? x - 0.5 : x * (double)m->gw / (double)m->ow - 0.5;
        double const fy = m->gh == m->oh ? y - 0.5 : y * (double)m->gh / (double)m->oh - 0.5;
        long const i0 = (long)floor(fx), j0 = (long)floor(fy);
        double const tx = fx - (double)i0, ty = fy - (double)j0;
        if (p->field_interpolation == ALWAN_WARP_FIELD_LINEAR) {
            double f00[2], f10[2], f01[2], f11[2];
            alwan_wp_tex(m, i0, j0, f00), alwan_wp_tex(m, i0 + 1, j0, f10);
            alwan_wp_tex(m, i0, j0 + 1, f01), alwan_wp_tex(m, i0 + 1, j0 + 1, f11);
            *sx = (1 - ty) * ((1 - tx) * f00[0] + tx * f10[0]) + ty * ((1 - tx) * f01[0] + tx * f11[0]);
            *sy = (1 - ty) * ((1 - tx) * f00[1] + tx * f10[1]) + ty * ((1 - tx) * f01[1] + tx * f11[1]);
        } else {
            double wx[4], wy[4], ax = 0.0, ay = 0.0;
            int a, b;
            if (p->field_interpolation == ALWAN_WARP_FIELD_CATMULL_ROM) {
                wx[0] = ((-0.5 * tx + 1.0) * tx - 0.5) * tx, wx[1] = (1.5 * tx - 2.5) * tx * tx + 1.0;
                wx[2] = ((-1.5 * tx + 2.0) * tx + 0.5) * tx, wx[3] = (0.5 * tx - 0.5) * tx * tx;
                wy[0] = ((-0.5 * ty + 1.0) * ty - 0.5) * ty, wy[1] = (1.5 * ty - 2.5) * ty * ty + 1.0;
                wy[2] = ((-1.5 * ty + 2.0) * ty + 0.5) * ty, wy[3] = (0.5 * ty - 0.5) * ty * ty;
            } else {
                double const ux = 1.0 - tx, uy = 1.0 - ty;
                wx[0] = ux * ux * ux / 6.0, wx[1] = ((3.0 * tx - 6.0) * tx * tx + 4.0) / 6.0;
                wx[2] = (((-3.0 * tx + 3.0) * tx + 3.0) * tx + 1.0) / 6.0, wx[3] = tx * tx * tx / 6.0;
                wy[0] = uy * uy * uy / 6.0, wy[1] = ((3.0 * ty - 6.0) * ty * ty + 4.0) / 6.0;
                wy[2] = (((-3.0 * ty + 3.0) * ty + 3.0) * ty + 1.0) / 6.0, wy[3] = ty * ty * ty / 6.0;
            }
            for (b = 0; b < 4; b++)
                for (a = 0; a < 4; a++) {
                    double t[2];
                    alwan_wp_tex(m, i0 - 1 + a, j0 - 1 + b, t);
                    ax += wx[a] * wy[b] * t[0];
                    ay += wx[a] * wy[b] * t[1];
                }
            *sx = ax, *sy = ay;
        }
    }
    return *sx == *sx && *sy == *sy;
}

/* The source point shown at output point (x, y): 0 when there is none. */
static int alwan_wp_eval_map(alwan_wp_map const *m, double x, double y, double *sx, double *sy) {
    switch (m->p->map) {
    case ALWAN_WARP_MAP_SWIRL: {
        /* c + R(phi(r)) (p - c), phi = angle f(1 - r / R), f the C2 quintic 6s^5 - 15s^4 + 10s^3:
         * a rotation of the displacement, no atan2 and no singularity at the centre */
        double const dx = x - m->cx, dy = y - m->cy, r = sqrt(dx * dx + dy * dy);
        double s = 1.0 - r / m->radius, phi, cs, sn;
        if (s <= 0.0) {
            *sx = x, *sy = y;
            return 1;
        }
        if (s > 1.0) s = 1.0;
        phi = m->angle * (s * s * s * (s * (s * 6.0 - 15.0) + 10.0));
        cs = cos(phi);
        sn = sin(phi);
        *sx = m->cx + cs * dx - sn * dy;
        *sy = m->cy + sn * dx + cs * dy;
        return 1;
    }
    case ALWAN_WARP_MAP_FIELD:
        return alwan_wp_field(m, x, y, sx, sy);
    case ALWAN_WARP_MAP_CALLBACK:
        return m->p->callback(x, y, sx, sy, m->p->callback_user) != 0 && *sx == *sx && *sy == *sy;
    default:
        if (m->p->perspective) {
            double const d = m->a[6] * x + m->a[7] * y + 1;
            *sx = (m->a[0] * x + m->a[1] * y + m->a[2]) / d;
            *sy = (m->a[3] * x + m->a[4] * y + m->a[5]) / d;
        } else {
            *sx = m->a[0] * x + m->a[1] * y + m->a[2];
            *sy = m->a[3] * x + m->a[4] * y + m->a[5];
        }
        return 1;
    }
}

/* One sub-sample: the map at (x, y), the source reconstructed there, or the fill. */
static void alwan_wp_subsample(double *v, alwan_wp_map const *m, alwan_wp_img const *im, alwan_warp_method method, double x, double y) {
    double sx, sy;
    size_t c;
    if (alwan_wp_eval_map(m, x, y, &sx, &sy)) {
        if (method == ALWAN_WARP_NEAREST) {
            long const xi = ALWAN_WP_COORD(sx), yi = ALWAN_WP_COORD(sy);
            if (xi >= 0 && xi < (long)im->w && yi >= 0 && yi < (long)im->h) {
                for (c = 0; c < im->ch; c++) v[c] = alwan_wp_px(im, xi, yi, c);
                return;
            }
        } else if (alwan_wp_eval(v, im, sx, sy, method == ALWAN_WARP_BICUBIC)) {
            return;
        }
    }
    for (c = 0; c < im->ch; c++) v[c] = m->p->fill[c];
}

/* A 32-bit integer hash (Wellons' lowbias32), for the per-pixel rotation of the R2 set. */
static uint32_t alwan_wp_hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

/* The first k points of the R2 sequence (Roberts 2018, the plastic constant) in the unit
 * square, or (disk) taken to the unit disk by Shirley and Chiu's concentric map. */
static void alwan_wp_r2_points(double *pts, size_t k, int disk) {
    double const g = 1.32471795724474602596, a1 = 1.0 / g, a2 = 1.0 / (g * g), pi = 3.14159265358979323846;
    size_t n;
    for (n = 0; n < k; n++) {
        double u = 0.5 + (double)(n + 1) * a1, v = 0.5 + (double)(n + 1) * a2, a, b, r, phi;
        u -= floor(u);
        v -= floor(v);
        if (!disk) {
            pts[2 * n] = u;
            pts[2 * n + 1] = v;
            continue;
        }
        a = 2.0 * u - 1.0;
        b = 2.0 * v - 1.0;
        if (a == 0.0 && b == 0.0) {
            pts[2 * n] = pts[2 * n + 1] = 0.0;
            continue;
        }
        if (fabs(a) > fabs(b)) {
            r = a;
            phi = (pi / 4.0) * (b / a);
        } else {
            r = b;
            phi = pi / 2.0 - (pi / 4.0) * (a / b);
        }
        pts[2 * n] = r * cos(phi);
        pts[2 * n + 1] = r * sin(phi);
    }
}

/* A point of the unit square carried to an offset drawn from the kernel (inverse CDFs), so
 * a set of points spread evenly over the square spreads by the kernel's weight around 0. */
static void alwan_wp_kernel_offset(alwan_pixel_kernel kernel, double u, double v, double *ox, double *oy) {
    if (kernel == ALWAN_PIXEL_KERNEL_TENT) {
        *ox = u < 0.5 ? -1.0 + sqrt(2.0 * u) : 1.0 - sqrt(2.0 * (1.0 - u));
        *oy = v < 0.5 ? -1.0 + sqrt(2.0 * v) : 1.0 - sqrt(2.0 * (1.0 - v));
    } else if (kernel == ALWAN_PIXEL_KERNEL_GAUSSIAN) {
        /* Box-Muller on the radius cut at 3 s: 1 - exp(-4.5) of the mass */
        double const r = 0.5 * sqrt(-2.0 * log(1.0 - u * 0.98889100346175773)), t = 6.28318530717958647692 * v;
        *ox = r * cos(t);
        *oy = r * sin(t);
    } else {
        *ox = u - 0.5;
        *oy = v - 0.5;
    }
}

/* The kernel's half support and its weight at (dx, dy), for the grid. */
static double alwan_wp_kernel_radius(alwan_pixel_kernel kernel) {
    return kernel == ALWAN_PIXEL_KERNEL_TENT ? 1.0 : kernel == ALWAN_PIXEL_KERNEL_GAUSSIAN ? 1.5 : 0.5;
}

static double alwan_wp_kernel_weight(alwan_pixel_kernel kernel, double dx, double dy) {
    if (kernel == ALWAN_PIXEL_KERNEL_TENT) return (1.0 - fabs(dx)) * (1.0 - fabs(dy));
    if (kernel == ALWAN_PIXEL_KERNEL_GAUSSIAN) {
        double const r2 = dx * dx + dy * dy;
        return r2 <= 2.25 ? exp(-2.0 * r2) : 0.0;
    }
    return 1.0;
}

static void alwan_wp_store_mean(void *orow, int kind, size_t i, double v) {
    if (kind == 0) ((alwan_f64 *)orow)[i] = v;
    else if (kind == 1) ((alwan_f32 *)orow)[i] = (alwan_f32)v;
    else {
        double const t = floor(v + 0.5);
        ((unsigned char *)orow)[i] = (unsigned char)(t < 0.0 ? 0.0 : t > 255.0 ? 255.0 : t);
    }
}

/* The general path: any map, any integration policy. */
static alwan_status alwan_wp_general(void *out, size_t out_rs, size_t ow, size_t oh, alwan_wp_img const *im, alwan_warp_method method,
                                     alwan_warp_params const *p, double const a[8]) {
    alwan_wp_map m;
    alwan_pixel_integration const pol = p->integration;
    size_t const grid_n = p->samples ? p->samples : 16;
    size_t const r2_n = p->samples ? p->samples : 16;
    size_t const max_n = p->samples ? p->samples : 64;
    double const tol = p->tolerance > 0.0 ? p->tolerance : 0.05;
    double const radius = 0.56418958354775628695;   /* 1 / sqrt(pi): the disk of a pixel's area */
    alwan_pixel_kernel const kernel = p->kernel;
    double const kr = alwan_wp_kernel_radius(kernel);
    /* a wider kernel covers more source, so the adaptive count grows with its support */
    double const kscale = kernel == ALWAN_PIXEL_KERNEL_BOX ? 1.0 : 4.0;
    int const disk = p->r2_disk && kernel == ALWAN_PIXEL_KERNEL_BOX;
    size_t const npts = pol == ALWAN_PIXEL_INTEGRATE_GRID ? 0 : (pol == ALWAN_PIXEL_INTEGRATE_R2 ? r2_n : (4 * max_n > 4096 ? 4096 : 4 * max_n));
    double *pts = NULL;
    size_t const ch = im->ch;
    int const alpha = p->alpha_channel && (ch == 2 || ch == 4);
    size_t x, y, c;
    memset(&m, 0, sizeof(m));
    m.p = p;
    memcpy(m.a, a, sizeof(m.a));
    m.ow = ow, m.oh = oh;
    m.gw = p->field_width ? p->field_width : ow;
    m.gh = p->field_height ? p->field_height : oh;
    m.cx = (p->swirl_center[0] != 0.0 || p->swirl_center[1] != 0.0) ? p->swirl_center[0] : (double)ow / 2.0;
    m.cy = (p->swirl_center[0] != 0.0 || p->swirl_center[1] != 0.0) ? p->swirl_center[1] : (double)oh / 2.0;
    m.radius = p->swirl_radius > 0.0 ? p->swirl_radius : (double)(ow < oh ? ow : oh) / 2.0;
    m.angle = p->swirl_angle;
    if (npts) {
        pts = (double *)ALWAN_ALLOC(alwan_safe_array_size(npts, 2 * sizeof(double)), sizeof(double));
        if (!pts) return ALWAN_E_NOMEM;
        alwan_wp_r2_points(pts, npts / 2 + 1, disk);
    }
    for (y = 0; y < oh; y++) {
        char *orow = (char *)out + y * out_rs;
        unsigned char *brow = p->samples_out ? p->samples_out + y * p->samples_out_row_stride : NULL;
        for (x = 0; x < ow; x++) {
            double const px = (double)x + 0.5, py = (double)y + 0.5;
            double acc[4] = { 0, 0, 0, 0 }, v[4], wsum;
            size_t n = 1, k;
            if (pol == ALWAN_PIXEL_INTEGRATE_POINT) {
                double sx, sy;
                if (alwan_wp_eval_map(&m, px, py, &sx, &sy)) {
                    if (method == ALWAN_WARP_NEAREST) {
                        long const xi = ALWAN_WP_COORD(sx), yi = ALWAN_WP_COORD(sy);
                        if (xi >= 0 && xi < (long)im->w && yi >= 0 && yi < (long)im->h) alwan_wp_copy_px(orow, x, im, xi, yi);
                    } else {
                        alwan_wp_sample(orow, x, im, sx, sy, method == ALWAN_WARP_BICUBIC);
                    }
                }
                if (brow) brow[x] = 1;
                continue;
            }
            if (pol == ALWAN_PIXEL_INTEGRATE_ADAPTIVE) {
                /* the map's footprint (a finite-difference Jacobian, h = half a pixel) and how
                 * far it is from linear across the pixel (second differences), in source pixels */
                double q0x, q0y, qpx, qpy, qmx, qmy, rpx, rpy, rmx, rmy, fp, nl, need;
                int edge = 0;
                int ok = alwan_wp_eval_map(&m, px, py, &q0x, &q0y);
                ok &= alwan_wp_eval_map(&m, px + 0.5, py, &qpx, &qpy);
                ok &= alwan_wp_eval_map(&m, px - 0.5, py, &qmx, &qmy);
                ok &= alwan_wp_eval_map(&m, px, py + 0.5, &rpx, &rpy);
                ok &= alwan_wp_eval_map(&m, px, py - 0.5, &rmx, &rmy);
                if (!ok) {
                    fp = 1e9, nl = 1e9;   /* the map loses the source somewhere in the pixel */
                } else {
                    double const jx = hypot(qpx - qmx, qpy - qmy), jy = hypot(rpx - rmx, rpy - rmy);
                    double const hx = hypot(qpx - 2 * q0x + qmx, qpy - 2 * q0y + qmy), hy = hypot(rpx - 2 * q0x + rmx, rpy - 2 * q0y + rmy);
                    double reach;
                    fp = jx > jy ? jx : jy;
                    nl = hx > hy ? hx : hy;
                    /* the kernel's footprint in the source straddles the image's edge: the fill
                     * meets the picture there in a step the map's smoothness does not show */
                    reach = fp * kr + nl;
                    if (q0x > -reach && q0y > -reach && q0x < (double)im->w + reach && q0y < (double)im->h + reach &&
                        !(q0x >= reach && q0y >= reach && q0x <= (double)im->w - reach && q0y <= (double)im->h - reach))
                        edge = 1;
                }
                if (edge) {
                    /* a hard step: its coverage is a fraction that 64 shifted points still leave
                     * grainy along the edge (RMS 0.016 against 4096 points where 256 leave
                     * 0.005), and edge pixels are few */
                    n = 4 * max_n;
                    if (n > 4096) n = 4096;
                } else if (fp <= 1.0 && nl <= tol) {
                    /* a box over a linear map is its centre; a wider kernel still has to be drawn,
                     * or the flat parts of the map come out sharper than the rest (16: at the
                     * identity, as close to the kernel as R2 with 16 points, where 8 left 30% more) */
                    n = kernel == ALWAN_PIXEL_KERNEL_BOX ? 1 : 16;
                    if (n > max_n) n = max_n;
                } else {
                    static size_t const budget[5] = { 4, 8, 16, 32, 64 };
                    int b;
                    need = (fp * fp > nl / tol ? fp * fp : nl / tol) * kscale;
                    n = 64;
                    for (b = 0; b < 5; b++)
                        if ((double)budget[b] >= need) {
                            n = budget[b];
                            break;
                        }
                    if (kernel != ALWAN_PIXEL_KERNEL_BOX && n < 16) n = 16;
                    if (n > max_n) n = max_n;
                }
            } else if (pol == ALWAN_PIXEL_INTEGRATE_GRID) {
                n = grid_n * grid_n;
            } else {
                n = r2_n;
            }
            wsum = (double)n;
            if (n == 1) {
                alwan_wp_subsample(acc, &m, im, method, px, py);
            } else if (pol == ALWAN_PIXEL_INTEGRATE_GRID) {
                /* n x n cell centres over the kernel's support, each weighted by the kernel */
                size_t i, j;
                double const step = 2.0 * kr / (double)grid_n;
                wsum = 0.0;
                for (j = 0; j < grid_n; j++)
                    for (i = 0; i < grid_n; i++) {
                        double const dx = -kr + ((double)i + 0.5) * step, dy = -kr + ((double)j + 0.5) * step;
                        double const wk = alwan_wp_kernel_weight(kernel, dx, dy);
                        if (wk <= 0.0) continue;
                        alwan_wp_subsample(v, &m, im, method, px + dx, py + dy);
                        if (alpha)
                            for (c = 0; c + 1 < ch; c++) v[c] *= v[ch - 1];
                        for (c = 0; c < ch; c++) acc[c] += wk * v[c];
                        wsum += wk;
                    }
            } else {
                /* antithetic R2 pairs, every pair's mean offset exactly zero: over the square
                 * pixel shifted toroidally by a per-pixel hash (Cranley-Patterson), or on the disk
                 * of the pixel's area turned by a per-pixel hashed angle */
                uint32_t const hs = alwan_wp_hash((uint32_t)x * 73856093U ^ (uint32_t)y * 19349663U ^ p->seed);
                double const h1 = (double)hs / 4294967296.0, h2 = (double)alwan_wp_hash(hs ^ 0x9e3779b9U) / 4294967296.0;
                double const phi = 6.28318530717958647692 * h1;
                double const cs = cos(phi), sn = sin(phi);
                size_t const pairs = n / 2;
                if (n % 2) {
                    alwan_wp_subsample(v, &m, im, method, px, py);
                    if (alpha)
                        for (c = 0; c + 1 < ch; c++) v[c] *= v[ch - 1];
                    for (c = 0; c < ch; c++) acc[c] += v[c];
                }
                for (k = 0; k < pairs; k++) {
                    double ox, oy;
                    if (disk) {
                        ox = radius * (cs * pts[2 * k] - sn * pts[2 * k + 1]);
                        oy = radius * (sn * pts[2 * k] + cs * pts[2 * k + 1]);
                    } else {
                        double const u = pts[2 * k] + h1, w2 = pts[2 * k + 1] + h2;
                        alwan_wp_kernel_offset(kernel, u - floor(u), w2 - floor(w2), &ox, &oy);
                    }
                    int s;
                    for (s = -1; s <= 1; s += 2) {
                        alwan_wp_subsample(v, &m, im, method, px + s * ox, py + s * oy);
                        if (alpha)
                            for (c = 0; c + 1 < ch; c++) v[c] *= v[ch - 1];
                        for (c = 0; c < ch; c++) acc[c] += v[c];
                    }
                }
            }
            if (n == 1 && alpha)
                for (c = 0; c + 1 < ch; c++) acc[c] *= acc[ch - 1];
            for (c = 0; c < ch; c++) acc[c] /= wsum;
            if (alpha) {
                double const al = acc[ch - 1];
                for (c = 0; c + 1 < ch; c++) acc[c] = al > 0.0 ? acc[c] / al : 0.0;
            }
            for (c = 0; c < ch; c++) alwan_wp_store_mean(orow, im->kind, x * ch + c, acc[c]);
            if (brow) brow[x] = (unsigned char)(n > 255 ? 255 : n);
        }
    }
    ALWAN_FREE(pts);
    return ALWAN_OK;
}

static alwan_status alwan_wp_run(void *out, size_t out_rs, size_t ow, size_t oh, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_warp_method method, alwan_warp_params const *params, int kind) {
    alwan_warp_params const zero = { { 0 } };
    alwan_warp_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    int identity = 1, i;
    double a[8];
    alwan_wp_img im;
    size_t x, y, c;
    if (!out || !src || w == 0 || h == 0 || ow == 0 || oh == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < ow) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_WARP_BICUBIC) return ALWAN_E_INVALID;
    if ((unsigned)p->map > (unsigned)ALWAN_WARP_MAP_CALLBACK || (unsigned)p->integration > (unsigned)ALWAN_PIXEL_INTEGRATE_ADAPTIVE ||
        (unsigned)p->kernel > (unsigned)ALWAN_PIXEL_KERNEL_GAUSSIAN)
        return ALWAN_E_INVALID;
    if (p->map == ALWAN_WARP_MAP_FIELD) {
        size_t const gw = p->field_width ? p->field_width : ow, gh = p->field_height ? p->field_height : oh;
        int const d = p->field_degree ? p->field_degree : 3;
        if (!p->field || p->field_row_stride / (2 * sizeof(double)) < gw || (unsigned)p->field_interpolation > (unsigned)ALWAN_WARP_FIELD_NURBS)
            return ALWAN_E_INVALID;
        if (gw > 1u << 24 || gh > 1u << 24) return ALWAN_E_RANGE;
        if (p->field_interpolation == ALWAN_WARP_FIELD_NURBS) {
            if (d < 1 || d > 7 || gw <= (size_t)d || gh <= (size_t)d) return ALWAN_E_RANGE;
            if (p->field_weights) {
                if (p->field_weights_row_stride / sizeof(double) < gw) return ALWAN_E_INVALID;
                for (y = 0; y < gh; y++)
                    for (x = 0; x < gw; x++) {
                        double const wt = ((double const *)((char const *)p->field_weights + y * p->field_weights_row_stride))[x];
                        if (!(wt > 0.0) || !(wt - wt == 0.0)) return ALWAN_E_RANGE;
                    }
            }
        }
    }
    if (p->map == ALWAN_WARP_MAP_CALLBACK && !p->callback) return ALWAN_E_INVALID;
    if (p->integration == ALWAN_PIXEL_INTEGRATE_GRID && p->samples > 64) return ALWAN_E_RANGE;
    if (p->integration >= ALWAN_PIXEL_INTEGRATE_R2 && p->samples > 4096) return ALWAN_E_RANGE;
    if (!(p->swirl_radius >= 0.0) || !(p->swirl_angle - p->swirl_angle == 0.0) || !(p->tolerance >= 0.0)) return ALWAN_E_RANGE;
    if (w > 1u << 24 || h > 1u << 24 || ow > 1u << 24 || oh > 1u << 24) return ALWAN_E_RANGE;
    for (i = 0; i < 8; i++) {
        if (!(p->matrix[i] - p->matrix[i] == 0.0)) return ALWAN_E_RANGE;
        if (p->matrix[i] != 0.0) identity = 0;
    }
    for (i = 0; i < 8; i++) a[i] = p->matrix[i];
    if (identity) a[0] = 1.0, a[4] = 1.0;
    if (!p->perspective) a[6] = a[7] = 0.0;
    im.src = src, im.rs = src_rs, im.ch = ch, im.w = w, im.h = h, im.kind = kind;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w * ch; x++) {
            double const v = alwan_wp_px(&im, (long)(x / ch), (long)y, x % ch);
            if (!(v - v == 0.0)) return ALWAN_E_INVALID;
        }
    }
    /* the fill first; every sample that lands inside overwrites it */
    for (y = 0; y < oh; y++) {
        char *orow = (char *)out + y * out_rs;
        for (x = 0; x < ow; x++)
            for (c = 0; c < ch; c++) {
                double const f = p->fill[c];
                if (kind == 0) ((alwan_f64 *)orow)[x * ch + c] = f;
                else if (kind == 1) ((alwan_f32 *)orow)[x * ch + c] = (alwan_f32)f;
                else ((unsigned char *)orow)[x * ch + c] = (unsigned char)(f < 0.0 ? 0.0 : f > 255.0 ? 255.0 : f);
            }
    }
    if (p->map != ALWAN_WARP_MAP_MATRIX || p->integration != ALWAN_PIXEL_INTEGRATE_POINT)
        return alwan_wp_general(out, out_rs, ow, oh, &im, method, p, a);
    if (p->samples_out)
        for (y = 0; y < oh; y++) memset(p->samples_out + y * p->samples_out_row_stride, 1, ow);
    if (method == ALWAN_WARP_NEAREST && !p->perspective) {
        if (a[1] == 0.0 && a[3] == 0.0) {
            /* ImagingScaleAffine */
            double xo = a[2] + a[0] * 0.5, yo = a[5] + a[4] * 0.5;
            long *xin = (long *)ALWAN_ALLOC(alwan_safe_array_size(ow, sizeof(long)), sizeof(long));
            if (!xin) return ALWAN_E_NOMEM;
            for (x = 0; x < ow; x++) {
                xin[x] = ALWAN_WP_COORD(xo);
                xo += a[0];
            }
            for (y = 0; y < oh; y++) {
                long const yi = ALWAN_WP_COORD(yo);
                if (yi >= 0 && yi < (long)h)
                    for (x = 0; x < ow; x++)
                        if (xin[x] >= 0 && xin[x] < (long)w) alwan_wp_copy_px((char *)out + y * out_rs, x, &im, xin[x], yi);
                yo += a[4];
            }
            ALWAN_FREE(xin);
        } else if (alwan_wp_check_fixed(a, 0, 0) && alwan_wp_check_fixed(a, (long)ow, (long)oh) && alwan_wp_check_fixed(a, 0, (long)oh) &&
                   alwan_wp_check_fixed(a, (long)ow, 0)) {
            /* affine_fixed: 16.16 fixed point */
            long const a0 = alwan_wp_fix(a[0]), a1 = alwan_wp_fix(a[1]), a3 = alwan_wp_fix(a[3]), a4 = alwan_wp_fix(a[4]);
            long a2 = alwan_wp_fix(a[2] + a[0] * 0.5 + a[1] * 0.5), a5 = alwan_wp_fix(a[5] + a[3] * 0.5 + a[4] * 0.5);
            for (y = 0; y < oh; y++) {
                long xx = a2, yy = a5;
                for (x = 0; x < ow; x++) {
                    long const xi = xx >> 16;   /* arithmetic, as Pillow's int shift */
                    if (xi >= 0 && xi < (long)w) {
                        long const yi = yy >> 16;
                        if (yi >= 0 && yi < (long)h) alwan_wp_copy_px((char *)out + y * out_rs, x, &im, xi, yi);
                    }
                    xx += a0;
                    yy += a3;
                }
                a2 += a1;
                a5 += a4;
            }
        } else {
            double xo = a[2] + a[1] * 0.5 + a[0] * 0.5, yo = a[5] + a[4] * 0.5 + a[3] * 0.5;
            for (y = 0; y < oh; y++) {
                double xx = xo, yy = yo;
                for (x = 0; x < ow; x++) {
                    long const xi = ALWAN_WP_COORD(xx);
                    if (xi >= 0 && xi < (long)w) {
                        long const yi = ALWAN_WP_COORD(yy);
                        if (yi >= 0 && yi < (long)h) alwan_wp_copy_px((char *)out + y * out_rs, x, &im, xi, yi);
                    }
                    xx += a[0];
                    yy += a[3];
                }
                xo += a[1];
                yo += a[4];
            }
        }
        return ALWAN_OK;
    }
    for (y = 0; y < oh; y++) {
        char *orow = (char *)out + y * out_rs;
        for (x = 0; x < ow; x++) {
            double const xin = (double)x + 0.5, yin = (double)y + 0.5;
            double xs, ys;
            if (p->perspective) {
                xs = (a[0] * xin + a[1] * yin + a[2]) / (a[6] * xin + a[7] * yin + 1);
                ys = (a[3] * xin + a[4] * yin + a[5]) / (a[6] * xin + a[7] * yin + 1);
            } else {
                xs = a[0] * xin + a[1] * yin + a[2];
                ys = a[3] * xin + a[4] * yin + a[5];
            }
            if (method == ALWAN_WARP_NEAREST) {
                long const xi = ALWAN_WP_COORD(xs), yi = ALWAN_WP_COORD(ys);
                if (xi >= 0 && xi < (long)w && yi >= 0 && yi < (long)h) alwan_wp_copy_px(orow, x, &im, xi, yi);
            } else {
                alwan_wp_sample(orow, x, &im, xs, ys, method == ALWAN_WARP_BICUBIC);
            }
        }
    }
    return ALWAN_OK;
}

alwan_status alwan__warp_run(void *out, size_t out_rs, size_t ow, size_t oh, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                            alwan_warp_method method, alwan_warp_params const *params, int kind) {
    return alwan_wp_run(out, out_rs, ow, oh, src, src_rs, ch, w, h, method, params, kind);
}

alwan_status alwan_warp_u8(unsigned char *out, size_t out_row_stride, size_t out_width, size_t out_height, unsigned char const *src,
                           size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_warp_method method,
                           alwan_warp_params const *params) {
    return alwan_wp_run(out, out_row_stride, out_width, out_height, src, src_row_stride, channels, width, height, method, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_warp_f64(alwan_f64 *out, size_t out_row_stride, size_t out_width, size_t out_height, alwan_f64 const *src,
                            size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_warp_method method,
                            alwan_warp_params const *params) {
    return alwan_wp_run(out, out_row_stride, out_width, out_height, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_warp_f32(alwan_f32 *out, size_t out_row_stride, size_t out_width, size_t out_height, alwan_f32 const *src,
                            size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_warp_method method,
                            alwan_warp_params const *params) {
    return alwan_wp_run(out, out_row_stride, out_width, out_height, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
