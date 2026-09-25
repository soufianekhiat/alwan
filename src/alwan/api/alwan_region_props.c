/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_region_props_{T} and _u8: measurements of every labelled region of a uint32_t
 * label image, as scikit-image's measure.regionprops computes them, which suite 228 holds
 * this to.
 *
 * The pixels of each label are gathered in raster order (a sort of label and index).
 * area, bbox, centroid and the intensity minimum and maximum are exact. The intensity
 * mean is numpy's mean of the region's values: a pairwise sum for one channel, a running
 * sum per channel for several (numpy's reduction along axis 0), in the data's precision.
 * The shape measures come from the central moments: the inertia tensor
 * [[mu02, -mu11], [-mu11, mu20]] / mu00, its eigenvalues in closed form (scikit-image uses
 * LAPACK), the axis lengths 4 sqrt(eigenvalue), the eccentricity sqrt(1 - l2 / l1) and
 * the orientation 0.5 atan2(-2 b, c - a). The perimeter is scikit-image's: the region's
 * 4-connected border within its box, each border pixel coded by a 3 x 3 kernel
 * [[10, 2, 10], [2, 1, 2], [10, 2, 10]] and weighted 1, sqrt(2) or (1 + sqrt(2)) / 2 by its
 * code.
 *
 * The rest follow scikit-image's regionprops on the region's box as a uint8 mask: the raw,
 * central (about the box's centroid), normalised and Hu moments through alwan_moments and its
 * companions, the inertia tensor and its eigenvalues; the convex hull as scikit-image's
 * convex_hull_image builds it (every pixel as the four midpoints of its edges, their hull,
 * and every pixel centre inside it or on its edges), here exactly, in doubled integer
 * coordinates; the Euler number and Crofton's perimeter from the counts of 2 x 2
 * configurations over the mask padded by one, with scikit-image's coefficient tables (8- and
 * 4-connectivity, four directions); and the largest Feret diameter as scikit-image finds it:
 * the greatest distance between the points where the hull mask's outline crosses between
 * pixels, which are the midpoints of every inside-outside pair of 4-neighbours.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double alwan_rp_r(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

static int alwan_rp_cmp(void const *a, void const *b) {
    uint64_t const x = *(uint64_t const *)a, y = *(uint64_t const *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

static double alwan_rp_pairwise(double const *a, size_t n, int f32) {
    if (n < 8) {
        double r = 0.0;
        size_t i;
        for (i = 0; i < n; i++) r = alwan_rp_r(r + a[i], f32);
        return r;
    }
    if (n <= 128) {
        double r[8], res;
        size_t i, j;
        for (j = 0; j < 8; j++) r[j] = a[j];
        for (i = 8; i < n - (n % 8); i += 8)
            for (j = 0; j < 8; j++) r[j] = alwan_rp_r(r[j] + a[i + j], f32);
        res = alwan_rp_r(alwan_rp_r(alwan_rp_r(r[0] + r[1], f32) + alwan_rp_r(r[2] + r[3], f32), f32) +
                             alwan_rp_r(alwan_rp_r(r[4] + r[5], f32) + alwan_rp_r(r[6] + r[7], f32), f32),
                         f32);
        for (; i < n; i++) res = alwan_rp_r(res + a[i], f32);
        return res;
    }
    {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan_rp_r(alwan_rp_pairwise(a, n2, f32) + alwan_rp_pairwise(a + n2, n - n2, f32), f32);
    }
}

typedef struct {
    long long x, y;   /* doubled coordinates: column, row */
} alwan_rp_pt;

static int alwan_rp_pt_cmp(void const *a, void const *b) {
    alwan_rp_pt const *p = (alwan_rp_pt const *)a, *q = (alwan_rp_pt const *)b;
    if (p->x != q->x) return p->x < q->x ? -1 : 1;
    return p->y < q->y ? -1 : p->y > q->y ? 1 : 0;
}

static long long alwan_rp_cross(alwan_rp_pt o, alwan_rp_pt a, alwan_rp_pt b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

/* Andrew's monotone chain; pts sorted and deduplicated in place; hull (room n + 1) receives
 * the strictly convex vertices counter-clockwise (as the cross product counts), returns
 * their number. */
static size_t alwan_rp_hull(alwan_rp_pt *pts, size_t n, alwan_rp_pt *hull) {
    size_t i, k = 0, m = 0, t;
    qsort(pts, n, sizeof(*pts), alwan_rp_pt_cmp);
    for (i = 0; i < n; i++)
        if (m == 0 || pts[i].x != pts[m - 1].x || pts[i].y != pts[m - 1].y) pts[m++] = pts[i];
    n = m;
    if (n < 3) {
        for (i = 0; i < n; i++) hull[i] = pts[i];
        return n;
    }
    for (i = 0; i < n; i++) {
        while (k >= 2 && alwan_rp_cross(hull[k - 2], hull[k - 1], pts[i]) <= 0) k--;
        hull[k++] = pts[i];
    }
    for (i = n - 1, t = k + 1; i-- > 0;) {
        while (k >= t && alwan_rp_cross(hull[k - 2], hull[k - 1], pts[i]) <= 0) k--;
        hull[k++] = pts[i];
    }
    return k - 1;
}

static long long alwan_rp_floordiv(long long a, long long b) {
    long long q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}

/* pixel centres (doubled: (2c, 2r)) of a bw x bh box inside or on the hull; each edge A -> B
 * keeps cross(B - A, P - A) >= 0, which on one row is a bound on the column */
static size_t alwan_rp_count_in_hull(alwan_rp_pt const *hv, size_t nh, size_t bw, size_t bh, unsigned char *mark) {
    size_t r, total = 0;
    for (r = 0; r < bh; r++) {
        long long lo = 0, hi = (long long)bw - 1, Y = 2 * (long long)r;
        size_t e;
        for (e = 0; e < nh && lo <= hi; e++) {
            alwan_rp_pt const A = hv[e], B = hv[(e + 1) % nh];
            long long const dx = B.x - A.x, dy = B.y - A.y;
            /* dx (Y - Ay) - dy (2c - Ax) >= 0  <=>  2 dy c <= dx (Y - Ay) + dy Ax */
            long long const rhs = dx * (Y - A.y) + dy * A.x;
            if (dy == 0) {
                if (rhs < 0) hi = lo - 1;
            } else if (dy > 0) {
                long long const cmax = alwan_rp_floordiv(rhs, 2 * dy);
                if (cmax < hi) hi = cmax;
            } else {
                long long const cmin = -alwan_rp_floordiv(-rhs, 2 * dy);   /* ceil(rhs / (2 dy)), dy < 0 */
                if (cmin > lo) lo = cmin;
            }
        }
        if (lo <= hi) {
            total += (size_t)(hi - lo + 1);
            if (mark) memset(mark + r * bw + (size_t)lo, 1, (size_t)(hi - lo + 1));
        }
    }
    return total;
}

/* The convex hull, Euler number, Crofton perimeter and Feret diameter of a bw x bh mask. */
static alwan_status alwan_rp_shape(alwan_region_props *pr, unsigned char const *m, size_t bw, size_t bh) {
    size_t const n = bw * bh, pw = bw + 4, ph = bh + 4;
    alwan_rp_pt *pts = NULL, *hull = NULL;
    unsigned char *hm = NULL;
    size_t np_ = 0, nh, r, c, i, j;
    long long hist[16];
    alwan_status st = ALWAN_OK;
    /* the candidate pixels: the first and last of each row and each column */
    pts = (alwan_rp_pt *)ALWAN_ALLOC(alwan_safe_array_size(8 * (bw + bh) + 2 * (pw + ph) * 2 + 8, sizeof(alwan_rp_pt)), sizeof(long long));
    hull = (alwan_rp_pt *)ALWAN_ALLOC(alwan_safe_array_size(8 * (bw + bh) + 2 * (pw + ph) * 2 + 9, sizeof(alwan_rp_pt)), sizeof(long long));
    hm = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size(pw * ph + n, 1), 1);
    if (!pts || !hull || !hm) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    for (r = 0; r < bh; r++) {
        size_t first = bw, last = 0;
        for (c = 0; c < bw; c++)
            if (m[r * bw + c]) {
                if (first == bw) first = c;
                last = c;
            }
        if (first == bw) continue;
        {
            size_t const cs2[2] = {first, last};
            for (i = 0; i < 2; i++) {
                long long const X = 2 * (long long)cs2[i], Y = 2 * (long long)r;
                pts[np_].x = X - 1, pts[np_++].y = Y;
                pts[np_].x = X + 1, pts[np_++].y = Y;
                pts[np_].x = X, pts[np_++].y = Y - 1;
                pts[np_].x = X, pts[np_++].y = Y + 1;
            }
        }
    }
    for (c = 0; c < bw; c++) {
        size_t first = bh, last = 0;
        for (r = 0; r < bh; r++)
            if (m[r * bw + c]) {
                if (first == bh) first = r;
                last = r;
            }
        if (first == bh) continue;
        {
            size_t const rs2[2] = {first, last};
            for (i = 0; i < 2; i++) {
                long long const X = 2 * (long long)c, Y = 2 * (long long)rs2[i];
                pts[np_].x = X - 1, pts[np_++].y = Y;
                pts[np_].x = X + 1, pts[np_++].y = Y;
                pts[np_].x = X, pts[np_++].y = Y - 1;
                pts[np_].x = X, pts[np_++].y = Y + 1;
            }
        }
    }
    nh = alwan_rp_hull(pts, np_, hull);
    {
        unsigned char *cm = hm + pw * ph;   /* the hull mask in the box */
        memset(cm, 0, n);
        pr->area_convex = alwan_rp_count_in_hull(hull, nh, bw, bh, cm);
        pr->solidity = (double)pr->area / (double)pr->area_convex;
        /* Feret: the hull mask padded by 2, the midpoints of inside-outside 4-neighbour pairs */
        memset(hm, 0, pw * ph);
        for (r = 0; r < bh; r++)
            for (c = 0; c < bw; c++) hm[(r + 2) * pw + (c + 2)] = cm[r * bw + c];
        np_ = 0;
        for (r = 0; r < ph; r++)
            for (c = 0; c < pw; c++) {
                unsigned char const v = hm[r * pw + c];
                if (c + 1 < pw && hm[r * pw + c + 1] != v) {
                    pts[np_].x = 2 * (long long)c + 1, pts[np_++].y = 2 * (long long)r;
                }
                if (r + 1 < ph && hm[(r + 1) * pw + c] != v) {
                    pts[np_].x = 2 * (long long)c, pts[np_++].y = 2 * (long long)r + 1;
                }
            }
        {
            size_t const k = alwan_rp_hull(pts, np_, hull);
            long long best = 0;
            for (i = 0; i < k; i++)
                for (j = i + 1; j < k; j++) {
                    long long const dx = hull[i].x - hull[j].x, dy = hull[i].y - hull[j].y, d2 = dx * dx + dy * dy;
                    if (d2 > best) best = d2;
                }
            pr->feret_diameter_max = ALWAN_SQRT_F64((double)best / 4.0);
        }
    }
    /* Euler number and Crofton perimeter: 2 x 2 configurations of the mask padded by 1 */
    memset(hist, 0, sizeof(hist));
    for (r = 0; r < bh + 2; r++)
        for (c = 0; c < bw + 2; c++) {
#define ALWAN_RP_Q(yy, xx) (((yy) >= 1 && (xx) >= 1 && (yy) <= bh && (xx) <= bw) ? (m[((yy) - 1) * bw + ((xx) - 1)] ? 1 : 0) : 0)
            int const code = ALWAN_RP_Q(r, c) * 1 + (c >= 1 ? ALWAN_RP_Q(r, c - 1) : 0) * 4 + (r >= 1 ? ALWAN_RP_Q(r - 1, c) : 0) * 2 +
                             (r >= 1 && c >= 1 ? ALWAN_RP_Q(r - 1, c - 1) : 0) * 8;
#undef ALWAN_RP_Q
            hist[code]++;
        }
    {
        static int const e8[16] = {0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 0, 0, 0, -1, 0};
        double const pi = 3.14159265358979323846, s2 = ALWAN_SQRT_F64(2.0);
        double const c1 = pi / 4 * (1 + 1 / s2), c2 = pi / (4 * s2), c3 = pi / (2 * s2), c8 = pi / 4, c9 = pi / 2;
        double const cr[16] = {0, c1, c2, c3, 0, c1, 0, c2, c8, c9, c2, c2, c8, c9, 0, 0};
        long e = 0;
        double t = 0.0;
        int q;
        for (q = 0; q < 16; q++) {
            e += (long)e8[q] * (long)hist[q];
            t += cr[q] * (double)hist[q];
        }
        pr->euler_number = e;
        pr->perimeter_crofton = t;
    }
done:
    ALWAN_FREE(hm);
    ALWAN_FREE(hull);
    ALWAN_FREE(pts);
    return st;
}

/* The moments of the region's box mask, as scikit-image's regionprops takes them. */
static alwan_status alwan_rp_moments(alwan_region_props *pr, unsigned char const *m, size_t bw, size_t bh) {
    alwan_moments_params mp;
    double raw[16], cen[16], nu[16], t[4];
    alwan_status st;
    size_t i;
    memset(&mp, 0, sizeof(mp));
    st = alwan_moments_u8(raw, 3, m, bw, bw, bh, ALWAN_MOMENTS_RAW, NULL);
    if (st != ALWAN_OK) return st;
    mp.center_given = 1;
    mp.center[0] = raw[1 * 4 + 0] / raw[0];
    mp.center[1] = raw[0 * 4 + 1] / raw[0];
    st = alwan_moments_u8(cen, 3, m, bw, bw, bh, ALWAN_MOMENTS_CENTRAL, &mp);
    if (st != ALWAN_OK) return st;
    alwan_moments_normalized(nu, cen, 3, NULL);
    alwan_moments_hu(pr->moments_hu, nu, 3);
    alwan_inertia_tensor(t, pr->inertia_tensor_eigvals, cen, 3);
    for (i = 0; i < 16; i++) {
        pr->moments[i / 4][i % 4] = raw[i];
        pr->moments_central[i / 4][i % 4] = cen[i];
        pr->moments_normalized[i / 4][i % 4] = nu[i];
    }
    pr->inertia_tensor[0][0] = t[0], pr->inertia_tensor[0][1] = t[1];
    pr->inertia_tensor[1][0] = t[2], pr->inertia_tensor[1][1] = t[3];
    return ALWAN_OK;
}

static alwan_status alwan_rp_run(alwan_region_props *props, size_t capacity, size_t *count_out, uint32_t const *labels, size_t labels_rs,
                                 void const *src, size_t src_rs, size_t ch, size_t w, size_t h, int kind) {
    int const f32 = kind == 1;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const n = w * h;
    uint64_t *keys;
    double *vals = NULL;
    unsigned char *box = NULL;
    size_t nk = 0, i, x, y, count = 0, start;
    alwan_status st = ALWAN_OK;
    if (!labels || w == 0 || h == 0 || n / w != h || labels_rs / sizeof(uint32_t) < w) return ALWAN_E_INVALID;
    if (src && (ch == 0 || ch > 4 || src_rs / elem / ch < w)) return ALWAN_E_INVALID;
    if (!props && capacity) return ALWAN_E_INVALID;
    if (n >= (size_t)0xFFFFFFFFu) return ALWAN_E_RANGE;
    if (!src) ch = 0;
    keys = (uint64_t *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(uint64_t)), sizeof(uint64_t));
    if (!keys) return ALWAN_E_NOMEM;
    for (y = 0; y < h; y++) {
        uint32_t const *row = (uint32_t const *)((char const *)labels + y * labels_rs);
        for (x = 0; x < w; x++)
            if (row[x]) keys[nk++] = ((uint64_t)row[x] << 32) | (uint64_t)(y * w + x);
    }
    qsort(keys, nk, sizeof(uint64_t), alwan_rp_cmp);
    for (i = 0; i < nk; i++)
        if (i == 0 || (keys[i] >> 32) != (keys[i - 1] >> 32)) count++;
    if (count_out) *count_out = count;
    if (count > capacity) {
        ALWAN_FREE(keys);
        return props ? ALWAN_E_RANGE : ALWAN_OK;
    }
    vals = (double *)ALWAN_ALLOC(alwan_safe_array_size(nk + 1, sizeof(double)), sizeof(double));
    box = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size(n + 1, 3), sizeof(double));
    if (!vals || !box) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    count = 0;
    for (start = 0; start < nk;) {
        uint32_t const lab = (uint32_t)(keys[start] >> 32);
        size_t end = start, k, c, r0 = h, c0 = w, r1 = 0, c1 = 0;
        alwan_region_props *pr = &props[count++];
        double sr = 0.0, sc = 0.0, rc, cc, mu11 = 0.0, mu20 = 0.0, mu02 = 0.0;
        while (end < nk && (uint32_t)(keys[end] >> 32) == lab) end++;
        memset(pr, 0, sizeof(*pr));
        pr->label = lab;
        pr->area = end - start;
        for (k = start; k < end; k++) {
            size_t const idx = (size_t)(keys[k] & 0xFFFFFFFFu), ry = idx / w, rx = idx % w;
            if (ry < r0) r0 = ry;
            if (ry > r1) r1 = ry;
            if (rx < c0) c0 = rx;
            if (rx > c1) c1 = rx;
            sr += (double)ry;
            sc += (double)rx;
        }
        pr->bbox[0] = r0, pr->bbox[1] = c0, pr->bbox[2] = r1 + 1, pr->bbox[3] = c1 + 1;
        pr->centroid[0] = sr / (double)pr->area;
        pr->centroid[1] = sc / (double)pr->area;
        /* central moments about the centroid, in the box's coordinates */
        rc = pr->centroid[0] - (double)r0;
        cc = pr->centroid[1] - (double)c0;
        for (k = start; k < end; k++) {
            size_t const idx = (size_t)(keys[k] & 0xFFFFFFFFu);
            double const dr = (double)(idx / w - r0) - rc, dc = (double)(idx % w - c0) - cc;
            mu11 += dr * dc;
            mu20 += dr * dr;
            mu02 += dc * dc;
        }
        {
            double const mu0 = (double)pr->area, a = mu02 / mu0, b = -mu11 / mu0, cq = mu20 / mu0;
            double const half = (a + cq) / 2.0, root = ALWAN_SQRT_F64(((a - cq) / 2.0) * ((a - cq) / 2.0) + b * b);
            double l1 = half + root, l2 = half - root;
            if (l1 < 0.0) l1 = 0.0;
            if (l2 < 0.0) l2 = 0.0;
            pr->axis_major_length = 4.0 * ALWAN_SQRT_F64(l1);
            pr->axis_minor_length = 4.0 * ALWAN_SQRT_F64(l2);
            pr->eccentricity = l1 == 0.0 ? 0.0 : ALWAN_SQRT_F64(1.0 - l2 / l1);
            if (a - cq == 0.0) pr->orientation = b < 0.0 ? 3.14159265358979323846 / 4.0 : -3.14159265358979323846 / 4.0;
            else pr->orientation = 0.5 * ALWAN_ATAN2_F64(-2.0 * b, cq - a);
        }
        pr->equivalent_diameter = ALWAN_POW_F64(4.0 * (double)pr->area / 3.14159265358979323846, 0.5);
        /* perimeter: the 4-connected border inside the box, coded and weighted */
        {
            size_t const bw = c1 - c0 + 1, bh = r1 - r0 + 1;
            unsigned char *m = box, *bd = box + bw * bh;
            size_t hist[50], yy, xx;
            double const s2 = ALWAN_SQRT_F64(2.0);
            memset(m, 0, bw * bh);
            memset(hist, 0, sizeof(hist));
            for (k = start; k < end; k++) {
                size_t const idx = (size_t)(keys[k] & 0xFFFFFFFFu);
                m[(idx / w - r0) * bw + (idx % w - c0)] = 1;
            }
            for (yy = 0; yy < bh; yy++)
                for (xx = 0; xx < bw; xx++) {
                    size_t const o = yy * bw + xx;
                    int const inner = m[o] && yy > 0 && xx > 0 && yy + 1 < bh && xx + 1 < bw && m[o - bw] && m[o + bw] && m[o - 1] && m[o + 1];
                    bd[o] = (unsigned char)(m[o] && !inner);
                }
            for (yy = 0; yy < bh; yy++)
                for (xx = 0; xx < bw; xx++) {
                    int dy, dx, code = 0;
                    for (dy = -1; dy <= 1; dy++)
                        for (dx = -1; dx <= 1; dx++) {
                            long const sy = (long)yy + dy, sx = (long)xx + dx;
                            if (sy < 0 || sx < 0 || sy >= (long)bh || sx >= (long)bw) continue;
                            code += bd[(size_t)sy * bw + (size_t)sx] * (dy == 0 && dx == 0 ? 1 : (dy == 0 || dx == 0) ? 2 : 10);
                        }
                    hist[code]++;
                }
            {
                double wts[50], t = 0.0;
                int q;
                memset(wts, 0, sizeof(wts));
                wts[5] = wts[7] = wts[15] = wts[17] = wts[25] = wts[27] = 1.0;
                wts[21] = wts[33] = s2;
                wts[13] = wts[23] = (1.0 + s2) / 2.0;
                for (q = 0; q < 50; q++) t += (double)hist[q] * wts[q];
                pr->perimeter = t;
            }
            st = alwan_rp_moments(pr, m, bw, bh);
            if (st == ALWAN_OK) st = alwan_rp_shape(pr, m, bw, bh);
            if (st != ALWAN_OK) goto done;
        }
        /* intensity */
        for (c = 0; c < ch; c++) {
            size_t m = 0;
            double mn = 0.0, mx = 0.0, s;
            for (k = start; k < end; k++) {
                size_t const idx = (size_t)(keys[k] & 0xFFFFFFFFu), ry = idx / w, rx = idx % w;
                char const *row = (char const *)src + ry * src_rs;
                double const v = kind == 0 ? ((alwan_f64 const *)row)[rx * ch + c]
                                 : kind == 1 ? (double)((alwan_f32 const *)row)[rx * ch + c]
                                             : (double)((unsigned char const *)row)[rx * ch + c];
                vals[m++] = v;
                if (m == 1 || v < mn) mn = v;
                if (m == 1 || v > mx) mx = v;
            }
            if (ch == 1) {
                s = alwan_rp_r(0.0 + alwan_rp_pairwise(vals, m, f32), f32);
            } else {
                size_t q;
                s = 0.0;
                for (q = 0; q < m; q++) s = alwan_rp_r(s + vals[q], f32);
            }
            pr->intensity_mean[c] = alwan_rp_r(s / (double)m, f32);
            pr->intensity_min[c] = mn;
            pr->intensity_max[c] = mx;
        }
        start = end;
    }
done:
    ALWAN_FREE(box);
    ALWAN_FREE(vals);
    ALWAN_FREE(keys);
    return st;
}

alwan_status alwan_region_props_u8(alwan_region_props *props, size_t capacity, size_t *count_out, uint32_t const *labels, size_t labels_row_stride,
                                   unsigned char const *intensity, size_t intensity_row_stride, size_t channels, size_t width, size_t height) {
    return alwan_rp_run(props, capacity, count_out, labels, labels_row_stride, intensity, intensity_row_stride, channels, width, height, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_region_props_f64(alwan_region_props *props, size_t capacity, size_t *count_out, uint32_t const *labels, size_t labels_row_stride,
                                    alwan_f64 const *intensity, size_t intensity_row_stride, size_t channels, size_t width, size_t height) {
    return alwan_rp_run(props, capacity, count_out, labels, labels_row_stride, intensity, intensity_row_stride, channels, width, height, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_region_props_f32(alwan_region_props *props, size_t capacity, size_t *count_out, uint32_t const *labels, size_t labels_row_stride,
                                    alwan_f32 const *intensity, size_t intensity_row_stride, size_t channels, size_t width, size_t height) {
    return alwan_rp_run(props, capacity, count_out, labels, labels_row_stride, intensity, intensity_row_stride, channels, width, height, 1);
}
#endif
