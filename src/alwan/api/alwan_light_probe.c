/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Panoramic light probes: the upper-hemisphere illuminance of an equirectangular panorama,
 * absolute luminance calibration from a measured illuminance (Lagarde, Lachambre and
 * Jover 2016), and lights found by variance minimisation (Viriyothai and Debevec 2009).
 * As colour-hdri 0.2.6 (BSD-3-Clause), which suite 244 holds this to.
 *
 * Illuminance. Row i of a panorama H rows high spans the zenith angles
 * [i pi / H, (i + 1) pi / H]. The illuminance on an upward horizontal surface is the
 * integral of L cos(theta) over the upper hemisphere, and for a panorama constant within
 * each pixel it is exactly
 *
 *   E = sum over the pixels of L (2 pi / W) (sin^2 th1 - sin^2 th0) / 2
 *
 * with each row's band clipped at the horizon (EXACT): a uniform sky of L = 1 gives pi.
 * colour-hdri (LAGARDE2016) samples row i at theta = i pi / (H - 1), poles included, and
 * weights it by cos sin 2 pi^2 / (W H), the weight of pixel-centre sampling: a uniform sky
 * gives 2.934 at H = 16 and 3.1385 at H = 1024, low by about pi / H. It is kept to
 * reproduce colour-hdri's numbers and the weight image Lagarde gives for Photoshop.
 *
 * Luminance is the Y row of the space's RGB to XYZ matrix (sRGB when space is NULL), on the
 * first three channels, (Y_R R + Y_G G) + Y_B B as colour's RGB_luminance sums it.
 *
 * Variance minimisation. Starting from the whole frame, each level splits every region in
 * two, at the column or row that minimises the larger of the two parts' luminance
 * "variance" sqrt(sum a ((x - cx)^2 + (y - cy)^2)), about each part's centroid rounded
 * DOWN to a whole pixel (colour's centroid: floor division). Columns x_min..x_max - 1 are
 * tried first, then rows, and a candidate replaces the best so far only when strictly
 * smaller, so the first minimum wins; the first column candidate leaves the left part
 * empty, which is how a region whose luminance sits on one pixel ends with an empty
 * child. Each light is a region: its luminance centroid (the region's origin added), that
 * centroid over (W, H), and the sum of its RGB.
 *
 * Every candidate cut is scored in O(1) from per-column and per-row moment sums of the
 * region (O(area) to build), so a level costs O(W H). Those sums round differently from
 * the pixel-by-pixel sums colour-hdri takes, so every candidate near the best, and every
 * candidate whose centroid quotient sits within rounding of a whole pixel, is scored
 * again pixel by pixel before the first minimum is taken. On panoramas without exact
 * ties the cuts equal colour-hdri's (suite 244). Where a tie is exact, as with one bright
 * pixel on black (floor(41 a / a) is 40 or 41 by the last bit of a) or a uniform field
 * (mirror-image cuts), colour-hdri's choice follows the last bit of its luminance, whose
 * matrix differs from alwan's by a few ulps, and numpy's summation order; alwan's choice
 * is then its own, as valid, and not necessarily colour-hdri's.
 *
 * colour-hdri takes a light count n and runs int(sqrt(n)) levels, 2^int(sqrt(n)) lights:
 * n = 4 and 16 give 4 and 16, n = 64 gives 256. alwan takes the level count.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

#define ALWAN_LP_PI 3.141592653589793

/* The luminance weights: the Y row of the space's RGB to XYZ matrix. */
static alwan_status alwan_lp_weights(double *yw, alwan_rgb_space_desc_f64 const *space) {
    alwan_rgb_space_desc_f64 srgb;
    alwan_mat3x3_f64 to_xyz, from_xyz;
    alwan_status st;
    if (!space) {
        st = alwan_rgb_get_space_descriptor_f64(&srgb, ALWAN_RGB_SPACE_SRGB, NULL);
        if (st != ALWAN_OK) return st;
        space = &srgb;
    }
    st = alwan_rgb_derive_matrices_f64(&to_xyz, &from_xyz, space);
    if (st != ALWAN_OK) return st;
    yw[0] = to_xyz.m[3];
    yw[1] = to_xyz.m[4];
    yw[2] = to_xyz.m[5];
    return ALWAN_OK;
}

#if ALWAN_WITH_F32
static void alwan_lp_widen(alwan_rgb_space_desc_f64 *d, alwan_rgb_space_desc_f32 const *s) {
    int k;
    memset(d, 0, sizeof(*d));
    for (k = 0; k < 6; k++) d->primaries_xy[k] = s->primaries_xy[k];
    d->white_xy[0] = s->white_xy[0];
    d->white_xy[1] = s->white_xy[1];
    d->oetf = s->oetf;
    d->eotf = s->eotf;
}
#endif

static double alwan_lp_read(void const *src, size_t rs, size_t i, size_t y, int kind) {
    char const *row = (char const *)src + y * rs;
    return kind == 0 ? ((alwan_f64 const *)row)[i] : kind == 1 ? (double)((alwan_f32 const *)row)[i]
                                                               : (double)((unsigned char const *)row)[i] / 255.0;
}

static alwan_status alwan_lp_check(void const *src, size_t rs, size_t ch, size_t w, size_t h, int kind) {
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    if (!src || w == 0 || h == 0 || ch < 3 || ch > 4 || (w * h) / w != h) return ALWAN_E_INVALID;
    if (rs / elem / ch < w) return ALWAN_E_INVALID;
    return ALWAN_OK;
}

/* Luminance of every pixel into Y (w * h doubles). */
static alwan_status alwan_lp_luminance(double *Y, void const *src, size_t rs, size_t ch, size_t w, size_t h, double const *yw, int kind) {
    size_t x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            double const r = alwan_lp_read(src, rs, x * ch, y, kind);
            double const g = alwan_lp_read(src, rs, x * ch + 1, y, kind);
            double const b = alwan_lp_read(src, rs, x * ch + 2, y, kind);
            if (!(r - r == 0.0) || !(g - g == 0.0) || !(b - b == 0.0)) return ALWAN_E_INVALID;
            Y[y * w + x] = (yw[0] * r + yw[1] * g) + yw[2] * b;
        }
    return ALWAN_OK;
}

/* ------------------------------------------------------------------------------------ */
/* Illuminance                                                                          */

/* The weight of row i, in colour-hdri's convention: E = sum L w / (W H). */
static double alwan_lp_row_weight(size_t i, size_t h, alwan_hemisphere_illuminance_method method) {
    if (method == ALWAN_HEMISPHERE_ILLUMINANCE_LAGARDE2016) {
        /* np.linspace(0, 1, h) * np.pi, the last sample set to 1 exactly */
        double const t = h == 1 ? 0.0 : (i == h - 1 ? 1.0 : (double)i * (1.0 / (double)(h - 1)));
        double const theta = t * ALWAN_LP_PI;
        double const c = cos(theta);
        return c > 0.0 ? c * sin(theta) * 2.0 * (ALWAN_LP_PI * ALWAN_LP_PI) : 0.0;
    } else {
        /* H pi (sin^2 th1 - sin^2 th0) over the band, clipped at the horizon, written as
         * sin(th1 + th0) sin(th1 - th0) to keep the difference accurate */
        double const half = ALWAN_LP_PI / 2.0;
        double const t0 = (double)i * ALWAN_LP_PI / (double)h;
        double t1 = (double)(i + 1) * ALWAN_LP_PI / (double)h;
        if (t0 >= half) return 0.0;
        if (t1 > half) t1 = half;
        return (double)h * ALWAN_LP_PI * sin(t1 + t0) * sin(t1 - t0);
    }
}

static alwan_status alwan_lp_illuminance(double *E, void const *src, size_t rs, size_t ch, size_t w, size_t h,
                                         double const *yw, alwan_hemisphere_illuminance_method method, int kind) {
    size_t x, y;
    double sum = 0.0;
    if ((unsigned)method > (unsigned)ALWAN_HEMISPHERE_ILLUMINANCE_LAGARDE2016) return ALWAN_E_INVALID;
    if (method == ALWAN_HEMISPHERE_ILLUMINANCE_LAGARDE2016) {
        /* colour-hdri: sum of L cos sin over the upper rows, then times 2 pi^2 / (W H) */
        for (y = 0; y < h; y++) {
            double const t = h == 1 ? 0.0 : (y == h - 1 ? 1.0 : (double)y * (1.0 / (double)(h - 1)));
            double const theta = t * ALWAN_LP_PI;
            double const c = cos(theta), s = sin(theta);
            if (!(c > 0.0)) continue;
            for (x = 0; x < w; x++) {
                double const r = alwan_lp_read(src, rs, x * ch, y, kind);
                double const g = alwan_lp_read(src, rs, x * ch + 1, y, kind);
                double const b = alwan_lp_read(src, rs, x * ch + 2, y, kind);
                double const L = (yw[0] * r + yw[1] * g) + yw[2] * b;
                sum += L * c * s;
            }
        }
        *E = sum * (2.0 * (ALWAN_LP_PI * ALWAN_LP_PI) / (double)(w * h));
    } else {
        for (y = 0; y < h; y++) {
            double const k = alwan_lp_row_weight(y, h, method);
            double row = 0.0;
            if (k == 0.0) continue;
            for (x = 0; x < w; x++) {
                double const r = alwan_lp_read(src, rs, x * ch, y, kind);
                double const g = alwan_lp_read(src, rs, x * ch + 1, y, kind);
                double const b = alwan_lp_read(src, rs, x * ch + 2, y, kind);
                row += (yw[0] * r + yw[1] * g) + yw[2] * b;
            }
            sum += row * k;
        }
        *E = sum / ((double)w * (double)h);
    }
    return *E - *E == 0.0 ? ALWAN_OK : ALWAN_E_INVALID;
}

alwan_status alwan_upper_hemisphere_illuminance_weights(double *out, size_t out_row_stride, size_t width, size_t height,
                                                        alwan_hemisphere_illuminance_method method) {
    size_t x, y;
    if (!out || width == 0 || height == 0 || out_row_stride / sizeof(double) < width) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_HEMISPHERE_ILLUMINANCE_LAGARDE2016) return ALWAN_E_INVALID;
    for (y = 0; y < height; y++) {
        double const k = alwan_lp_row_weight(y, height, method);
        double *row = (double *)((char *)out + y * out_row_stride);
        for (x = 0; x < width; x++) row[x] = k;
    }
    return ALWAN_OK;
}

static alwan_status alwan_lp_calibrate(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                       double measured, double const *yw, alwan_hemisphere_illuminance_method method, int kind) {
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : sizeof(alwan_f32);
    double E;
    size_t x, y, c;
    alwan_status st;
    if (!out || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if (!(measured - measured == 0.0)) return ALWAN_E_INVALID;
    st = alwan_lp_illuminance(&E, src, src_rs, ch, w, h, yw, method, kind);
    if (st != ALWAN_OK) return st;
    if (!(E > 0.0)) return ALWAN_E_RANGE;
    for (y = 0; y < h; y++) {
        char *orow = (char *)out + y * out_rs;
        for (x = 0; x < w; x++)
            for (c = 0; c < ch; c++) {
                double const v = alwan_lp_read(src, src_rs, x * ch + c, y, kind);
                /* colour-hdri: RGB / E_v * measured; a fourth channel is copied */
                double const r = c < 3 ? v / E * measured : v;
                if (kind == 0) ((alwan_f64 *)orow)[x * ch + c] = r;
                else ((alwan_f32 *)orow)[x * ch + c] = (alwan_f32)r;
            }
    }
    return ALWAN_OK;
}

/* ------------------------------------------------------------------------------------ */
/* Variance minimisation                                                                */

/* numpy's float floor division (npy_divmod): the floor of the exact quotient. */
static double alwan_lp_floordiv(double a, double b) {
    double mod, div, fl;
    if (b == 0.0) return a / b;
    mod = fmod(a, b);
    div = (a - mod) / b;
    if (mod != 0.0) {
        if ((b < 0.0) != (mod < 0.0)) div -= 1.0;
    }
    if (div != 0.0) {
        fl = floor(div);
        if (div - fl > 0.5) fl += 1.0;
    } else {
        fl = copysign(0.0, a / b);
    }
    return fl;
}

/* colour-hdri's luminance_variance on rows [ya, yb), columns [xa, xb) of Y, pixel by
 * pixel, coordinates local to the part. A part whose luminance sums to 0 (numpy's
 * centroid is then undefined) scores 0, which is colour-hdri's value when the part is all
 * zero. Returns the value BEFORE the square root; *cy, *cx receive the centroid. */
static double alwan_lp_q_direct(double const *Y, size_t W, size_t ya, size_t yb, size_t xa, size_t xb, double *cy, double *cx) {
    double s0 = 0.0, sr = 0.0, sc = 0.0, q = 0.0;
    size_t x, y;
    for (y = ya; y < yb; y++)
        for (x = xa; x < xb; x++) {
            double const a = Y[y * W + x];
            s0 += a;
            sr += (double)(y - ya) * a;
            sc += (double)(x - xa) * a;
        }
    if (s0 == 0.0 || !(s0 - s0 == 0.0)) {
        *cy = *cx = NAN;
        return 0.0;
    }
    *cy = alwan_lp_floordiv(sr, s0);
    *cx = alwan_lp_floordiv(sc, s0);
    for (y = ya; y < yb; y++)
        for (x = xa; x < xb; x++) {
            double const dr = (double)(y - ya) - *cy, dc = (double)(x - xa) - *cx;
            q += Y[y * W + x] * (dc * dc + dr * dr);
        }
    return q;
}

/* Whether a centroid quotient lies so near a whole pixel that its floor depends on rounding. */
static int alwan_lp_near_int(double num, double den) {
    double const t = num / den;
    return fabs(t - floor(t + 0.5)) <= 1e-9 * (1.0 + fabs(t));
}

/* The same from moment sums: s0 = sum a, sr = sum a r, sc = sum a c, qq = sum a (r^2 + c^2).
 * *amb is set when a floored centroid is within rounding of a whole pixel. */
static double alwan_lp_q_fast(double s0, double sr, double sc, double qq, int nonneg, int *amb) {
    double cy, cx, q;
    if (s0 == 0.0) return 0.0;
    if (alwan_lp_near_int(sr, s0) || alwan_lp_near_int(sc, s0)) *amb = 1;
    cy = alwan_lp_floordiv(sr, s0);
    cx = alwan_lp_floordiv(sc, s0);
    q = qq - 2.0 * cy * sr - 2.0 * cx * sc + (cy * cy + cx * cx) * s0;
    if (nonneg && q < 0.0) q = 0.0;
    return q;
}

/* Python's max(a, b) and the square root, both NaN-preserving as colour-hdri's are. */
static double alwan_lp_pymax(double a, double b) { return b > a ? b : a; }
static double alwan_lp_var(double q) { return q < 0.0 ? NAN : sqrt(q); }

typedef struct {
    size_t y0, y1, x0, x1;
} alwan_lp_region;

/* Candidate k of a region: 0..w-1 the columns x0 + k, then w..w+h-1 the rows y0 + k - w. */
static double alwan_lp_candidate_direct(double const *Y, size_t W, alwan_lp_region const *g, size_t k) {
    size_t const w = g->x1 - g->x0;
    double cy, cx, a, b;
    if (k < w) {
        size_t const j = g->x0 + k;
        a = alwan_lp_var(alwan_lp_q_direct(Y, W, g->y0, g->y1, g->x0, j, &cy, &cx));
        b = alwan_lp_var(alwan_lp_q_direct(Y, W, g->y0, g->y1, j, g->x1, &cy, &cx));
    } else {
        size_t const j = g->y0 + (k - w);
        a = alwan_lp_var(alwan_lp_q_direct(Y, W, g->y0, j, g->x0, g->x1, &cy, &cx));
        b = alwan_lp_var(alwan_lp_q_direct(Y, W, j, g->y1, g->x0, g->x1, &cy, &cx));
    }
    return alwan_lp_pymax(a, b);
}

/* Split one region. m holds 4 (w + h + 2) doubles of scratch, f and amb (w + h), idx (w + h). */
static alwan_status alwan_lp_split(alwan_lp_region *out, double const *Y, size_t W, alwan_lp_region const *g, int nonneg,
                                   double *m, double *f, unsigned char *amb, size_t *idx) {
    size_t const w = g->x1 - g->x0, h = g->y1 - g->y0, n = w + h;
    double *PA = m, *PR = PA + (w + 1), *PC1 = PR + (w + 1), *PQ = PC1 + (w + 1);
    double *QB = PQ + (w + 1), *QC = QB + (h + 1), *QR1 = QC + (h + 1), *QQ = QR1 + (h + 1);
    double AT, RT, C1T, QT, BT, CT, R1T, QQT, scale, best, tol;
    size_t k, x, y, nt = 0, winner = (size_t)-1;
    if (n == 0) return ALWAN_E_RANGE;   /* colour-hdri has no cut here (location -1) */
    /* per-column moments, prefix-summed: A = sum a, R = sum a r, C1 = sum a c, Q = sum a (r^2 + c^2) */
    PA[0] = PR[0] = PC1[0] = PQ[0] = 0.0;
    for (x = 0; x < w; x++) {
        double A = 0.0, R = 0.0, RR = 0.0;
        double const c = (double)x;
        for (y = 0; y < h; y++) {
            double const a = Y[(g->y0 + y) * W + g->x0 + x], r = (double)y;
            A += a;
            R += a * r;
            RR += a * r * r;
        }
        PA[x + 1] = PA[x] + A;
        PR[x + 1] = PR[x] + R;
        PC1[x + 1] = PC1[x] + A * c;
        PQ[x + 1] = PQ[x] + RR + A * c * c;
    }
    QB[0] = QC[0] = QR1[0] = QQ[0] = 0.0;
    for (y = 0; y < h; y++) {
        double B = 0.0, C = 0.0, CC = 0.0;
        double const r = (double)y;
        for (x = 0; x < w; x++) {
            double const a = Y[(g->y0 + y) * W + g->x0 + x], c = (double)x;
            B += a;
            C += a * c;
            CC += a * c * c;
        }
        QB[y + 1] = QB[y] + B;
        QC[y + 1] = QC[y] + C;
        QR1[y + 1] = QR1[y] + B * r;
        QQ[y + 1] = QQ[y] + CC + B * r * r;
    }
    AT = PA[w]; RT = PR[w]; C1T = PC1[w]; QT = PQ[w];
    BT = QB[h]; CT = QC[h]; R1T = QR1[h]; QQT = QQ[h];
    (void)BT;
    scale = fabs(QT) + ((double)w * (double)w + (double)h * (double)h) * fabs(AT);
    for (k = 0; k < w; k++) {
        double const mm = (double)k;
        double const s0l = PA[k], srl = PR[k], scl = PC1[k], ql = PQ[k];
        double const s0r = AT - PA[k], srr = RT - PR[k], c1r = C1T - PC1[k];
        double const scr = c1r - mm * s0r;
        double const qr = (QT - PQ[k]) - 2.0 * mm * c1r + mm * mm * s0r;
        int a = 0;
        f[k] = alwan_lp_pymax(alwan_lp_var(alwan_lp_q_fast(s0l, srl, scl, ql, nonneg, &a)),
                              alwan_lp_var(alwan_lp_q_fast(s0r, srr, scr, qr, nonneg, &a)));
        amb[k] = (unsigned char)a;
    }
    for (k = 0; k < h; k++) {
        double const mm = (double)k;
        double const s0t = QB[k], srt = QR1[k], sct = QC[k], qt = QQ[k];
        double const s0b = AT - QB[k], r1b = R1T - QR1[k], scb = CT - QC[k];
        double const srb = r1b - mm * s0b;
        double const qb = (QQT - QQ[k]) - 2.0 * mm * r1b + mm * mm * s0b;
        int a = 0;
        f[w + k] = alwan_lp_pymax(alwan_lp_var(alwan_lp_q_fast(s0t, srt, sct, qt, nonneg, &a)),
                                  alwan_lp_var(alwan_lp_q_fast(s0b, srb, scb, qb, nonneg, &a)));
        amb[w + k] = (unsigned char)a;
    }
    /* The fast scores' rounding is far below 1e-12 of the moment scale on the squared
     * scale; every candidate within twice that of the best, or with a centroid on the edge
     * of a whole pixel, is scored again directly, in candidate order. */
    best = INFINITY;
    for (k = 0; k < n; k++)
        if (f[k] < best && !amb[k]) best = f[k];
    tol = best < INFINITY ? sqrt(best * best + 2e-12 * scale) : INFINITY;
    for (k = 0; k < n; k++)
        if (amb[k] || f[k] <= tol) idx[nt++] = k;
    if (nt == 0) return ALWAN_E_RANGE;   /* every candidate NaN: colour-hdri finds no cut */
    if (nt == 1) {
        winner = idx[0];
    } else {
        double v = INFINITY;
        for (k = 0; k < nt; k++) {
            double const e = alwan_lp_candidate_direct(Y, W, g, idx[k]);
            if (e < v) {
                v = e;
                winner = idx[k];
                if (nonneg && v == 0.0) break;   /* nothing is strictly below 0 */
            }
        }
        if (winner == (size_t)-1) return ALWAN_E_RANGE;
    }
    if (winner < w) {
        size_t const j = g->x0 + winner;
        out[0].y0 = g->y0; out[0].y1 = g->y1; out[0].x0 = g->x0; out[0].x1 = j;
        out[1].y0 = g->y0; out[1].y1 = g->y1; out[1].x0 = j; out[1].x1 = g->x1;
    } else {
        size_t const j = g->y0 + (winner - w);
        out[0].y0 = g->y0; out[0].y1 = j; out[0].x0 = g->x0; out[0].x1 = g->x1;
        out[1].y0 = j; out[1].y1 = g->y1; out[1].x0 = g->x0; out[1].x1 = g->x1;
    }
    return ALWAN_OK;
}

static alwan_status alwan_lp_sample(alwan_light_probe_light *out, size_t capacity, size_t *count, void const *src, size_t rs, size_t ch,
                                    size_t w, size_t h, double const *yw, alwan_light_probe_method method,
                                    alwan_light_probe_params const *params, int kind) {
    alwan_light_probe_params const zero = { 0 };
    alwan_light_probe_params const *p = params ? params : &zero;
    size_t const levels = p->levels ? p->levels : 4;
    size_t const nl = (size_t)1 << (levels > 30 ? 30 : levels);
    alwan_lp_region *reg = NULL, *next = NULL;
    double *Y = NULL, *m = NULL, *f = NULL;
    unsigned char *amb = NULL;
    size_t *idx = NULL;
    size_t nr = 1, lv, i, x, y;
    int nonneg = 1;
    alwan_status st;
    if (count) *count = 0;
    if (!out || !count) return ALWAN_E_INVALID;
    st = alwan_lp_check(src, rs, ch, w, h, kind);
    if (st != ALWAN_OK) return st;
    if ((unsigned)method > (unsigned)ALWAN_LIGHT_PROBE_VARIANCE_MINIMIZATION) return ALWAN_E_INVALID;
    if (levels > 20) return ALWAN_E_RANGE;
    if (capacity < nl) return ALWAN_E_RANGE;
    Y = (double *)ALWAN_ALLOC(alwan_safe_array_size(w * h, sizeof(double)), sizeof(double));
    m = (double *)ALWAN_ALLOC(alwan_safe_array_size(4 * (w + h + 2), sizeof(double)), sizeof(double));
    f = (double *)ALWAN_ALLOC(alwan_safe_array_size(w + h + 1, sizeof(double)), sizeof(double));
    amb = (unsigned char *)ALWAN_ALLOC(w + h + 1, sizeof(size_t));
    idx = (size_t *)ALWAN_ALLOC(alwan_safe_array_size(w + h + 1, sizeof(size_t)), sizeof(size_t));
    reg = (alwan_lp_region *)ALWAN_ALLOC(alwan_safe_array_size(nl, sizeof(alwan_lp_region)), sizeof(size_t));
    next = (alwan_lp_region *)ALWAN_ALLOC(alwan_safe_array_size(nl, sizeof(alwan_lp_region)), sizeof(size_t));
    if (!Y || !m || !f || !amb || !idx || !reg || !next) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    st = alwan_lp_luminance(Y, src, rs, ch, w, h, yw, kind);
    if (st != ALWAN_OK) goto done;
    for (i = 0; i < w * h; i++)
        if (Y[i] < 0.0) nonneg = 0;
    reg[0].y0 = 0; reg[0].y1 = h; reg[0].x0 = 0; reg[0].x1 = w;
    for (lv = 0; lv < levels && st == ALWAN_OK; lv++) {
        alwan_lp_region *t;
        for (i = 0; i < nr && st == ALWAN_OK; i++) st = alwan_lp_split(next + 2 * i, Y, w, reg + i, nonneg, m, f, amb, idx);
        nr *= 2;
        t = reg; reg = next; next = t;
    }
    if (st != ALWAN_OK) goto done;
    for (i = 0; i < nr; i++) {
        alwan_lp_region const *g = reg + i;
        alwan_light_probe_light *L = out + i;
        double cy, cx;
        double rgb[3] = { 0.0, 0.0, 0.0 };
        size_t c;
        L->y0 = g->y0; L->y1 = g->y1; L->x0 = g->x0; L->x1 = g->x1;
        (void)alwan_lp_q_direct(Y, w, g->y0, g->y1, g->x0, g->x1, &cy, &cx);
        L->cy = cy + (double)g->y0;
        L->cx = cx + (double)g->x0;
        L->u = L->cx / (double)w;
        L->v = L->cy / (double)h;
        /* np.sum(np.sum(region, 0), 0): down the rows first, then across */
        for (x = g->x0; x < g->x1; x++) {
            double col[3] = { 0.0, 0.0, 0.0 };
            for (y = g->y0; y < g->y1; y++)
                for (c = 0; c < 3; c++) col[c] += alwan_lp_read(src, rs, x * ch + c, y, kind);
            for (c = 0; c < 3; c++) rgb[c] += col[c];
        }
        for (c = 0; c < 3; c++) L->rgb[c] = rgb[c];
    }
    *count = nr;
done:
    ALWAN_FREE(Y);
    ALWAN_FREE(m);
    ALWAN_FREE(f);
    ALWAN_FREE(amb);
    ALWAN_FREE(idx);
    ALWAN_FREE(reg);
    ALWAN_FREE(next);
    return st;
}

/* ------------------------------------------------------------------------------------ */
/* The API                                                                              */

alwan_status alwan_upper_hemisphere_illuminance_u8(double *illuminance, unsigned char const *src, size_t row_stride, size_t channels,
                                                   size_t width, size_t height, alwan_rgb_space_desc_f64 const *space,
                                                   alwan_hemisphere_illuminance_method method) {
    double yw[3];
    alwan_status st;
    if (!illuminance) return ALWAN_E_INVALID;
    st = alwan_lp_check(src, row_stride, channels, width, height, 2);
    if (st == ALWAN_OK) st = alwan_lp_weights(yw, space);
    return st == ALWAN_OK ? alwan_lp_illuminance(illuminance, src, row_stride, channels, width, height, yw, method, 2) : st;
}

alwan_status alwan_light_probe_sample_u8(alwan_light_probe_light *lights, size_t capacity, size_t *count, unsigned char const *src,
                                         size_t row_stride, size_t channels, size_t width, size_t height,
                                         alwan_rgb_space_desc_f64 const *space, alwan_light_probe_method method,
                                         alwan_light_probe_params const *params, alwan_ctx *ctx) {
    double yw[3];
    alwan_status st;
    (void)ctx;
    if (count) *count = 0;
    st = alwan_lp_weights(yw, space);
    return st == ALWAN_OK ? alwan_lp_sample(lights, capacity, count, src, row_stride, channels, width, height, yw, method, params, 2) : st;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_upper_hemisphere_illuminance_f64(double *illuminance, alwan_f64 const *src, size_t row_stride, size_t channels,
                                                    size_t width, size_t height, alwan_rgb_space_desc_f64 const *space,
                                                    alwan_hemisphere_illuminance_method method) {
    double yw[3];
    alwan_status st;
    if (!illuminance) return ALWAN_E_INVALID;
    st = alwan_lp_check(src, row_stride, channels, width, height, 0);
    if (st == ALWAN_OK) st = alwan_lp_weights(yw, space);
    return st == ALWAN_OK ? alwan_lp_illuminance(illuminance, src, row_stride, channels, width, height, yw, method, 0) : st;
}

alwan_status alwan_absolute_luminance_calibrate_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                                                    size_t channels, size_t width, size_t height, double measured_illuminance,
                                                    alwan_rgb_space_desc_f64 const *space, alwan_hemisphere_illuminance_method method) {
    double yw[3];
    alwan_status st = alwan_lp_check(src, src_row_stride, channels, width, height, 0);
    if (st == ALWAN_OK) st = alwan_lp_weights(yw, space);
    return st == ALWAN_OK ? alwan_lp_calibrate(out, out_row_stride, src, src_row_stride, channels, width, height, measured_illuminance, yw, method, 0)
                          : st;
}

alwan_status alwan_light_probe_sample_f64(alwan_light_probe_light *lights, size_t capacity, size_t *count, alwan_f64 const *src,
                                          size_t row_stride, size_t channels, size_t width, size_t height,
                                          alwan_rgb_space_desc_f64 const *space, alwan_light_probe_method method,
                                          alwan_light_probe_params const *params, alwan_ctx *ctx) {
    double yw[3];
    alwan_status st;
    (void)ctx;
    if (count) *count = 0;
    st = alwan_lp_weights(yw, space);
    return st == ALWAN_OK ? alwan_lp_sample(lights, capacity, count, src, row_stride, channels, width, height, yw, method, params, 0) : st;
}
#endif

#if ALWAN_WITH_F32
static alwan_status alwan_lp_weights_f32(double *yw, alwan_rgb_space_desc_f32 const *space) {
    alwan_rgb_space_desc_f64 d;
    if (!space) return alwan_lp_weights(yw, NULL);
    alwan_lp_widen(&d, space);
    return alwan_lp_weights(yw, &d);
}

alwan_status alwan_upper_hemisphere_illuminance_f32(double *illuminance, alwan_f32 const *src, size_t row_stride, size_t channels,
                                                    size_t width, size_t height, alwan_rgb_space_desc_f32 const *space,
                                                    alwan_hemisphere_illuminance_method method) {
    double yw[3];
    alwan_status st;
    if (!illuminance) return ALWAN_E_INVALID;
    st = alwan_lp_check(src, row_stride, channels, width, height, 1);
    if (st == ALWAN_OK) st = alwan_lp_weights_f32(yw, space);
    return st == ALWAN_OK ? alwan_lp_illuminance(illuminance, src, row_stride, channels, width, height, yw, method, 1) : st;
}

alwan_status alwan_absolute_luminance_calibrate_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                                                    size_t channels, size_t width, size_t height, double measured_illuminance,
                                                    alwan_rgb_space_desc_f32 const *space, alwan_hemisphere_illuminance_method method) {
    double yw[3];
    alwan_status st = alwan_lp_check(src, src_row_stride, channels, width, height, 1);
    if (st == ALWAN_OK) st = alwan_lp_weights_f32(yw, space);
    return st == ALWAN_OK ? alwan_lp_calibrate(out, out_row_stride, src, src_row_stride, channels, width, height, measured_illuminance, yw, method, 1)
                          : st;
}

alwan_status alwan_light_probe_sample_f32(alwan_light_probe_light *lights, size_t capacity, size_t *count, alwan_f32 const *src,
                                          size_t row_stride, size_t channels, size_t width, size_t height,
                                          alwan_rgb_space_desc_f32 const *space, alwan_light_probe_method method,
                                          alwan_light_probe_params const *params, alwan_ctx *ctx) {
    double yw[3];
    alwan_status st;
    (void)ctx;
    if (count) *count = 0;
    st = alwan_lp_weights_f32(yw, space);
    return st == ALWAN_OK ? alwan_lp_sample(lights, capacity, count, src, row_stride, channels, width, height, yw, method, params, 1) : st;
}
#endif
