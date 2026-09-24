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
            double const half = (a + cq) / 2.0, root = sqrt(((a - cq) / 2.0) * ((a - cq) / 2.0) + b * b);
            double l1 = half + root, l2 = half - root;
            if (l1 < 0.0) l1 = 0.0;
            if (l2 < 0.0) l2 = 0.0;
            pr->axis_major_length = 4.0 * sqrt(l1);
            pr->axis_minor_length = 4.0 * sqrt(l2);
            pr->eccentricity = l1 == 0.0 ? 0.0 : sqrt(1.0 - l2 / l1);
            if (a - cq == 0.0) pr->orientation = b < 0.0 ? 3.14159265358979323846 / 4.0 : -3.14159265358979323846 / 4.0;
            else pr->orientation = 0.5 * atan2(-2.0 * b, cq - a);
        }
        pr->equivalent_diameter = pow(4.0 * (double)pr->area / 3.14159265358979323846, 0.5);
        /* perimeter: the 4-connected border inside the box, coded and weighted */
        {
            size_t const bw = c1 - c0 + 1, bh = r1 - r0 + 1;
            unsigned char *m = box, *bd = box + bw * bh;
            size_t hist[50], yy, xx;
            double const s2 = sqrt(2.0);
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
