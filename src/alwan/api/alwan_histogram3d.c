/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * A three-dimensional histogram of RGB values: how many pixels fall in each cell of a
 * bins x bins x bins lattice over a box, the data a colour cube or point cloud view is
 * drawn from. It bins as numpy.histogramdd does, so the counts match it cell for cell:
 *
 *   the edges of each axis are numpy.linspace(lo, hi, bins + 1), that is lo + i * step
 *   with step = (hi - lo) / bins and the last edge exactly hi;
 *   a value's bin is searchsorted(edges, v, side='right') - 1;
 *   a value equal to hi goes in the last bin, not past it;
 *   a pixel with any channel below lo, above hi or NaN is not counted.
 *
 * Suite 189 holds it to numpy.histogramdd.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>

#define ALWAN_H3_MAX_BINS 1024

/* searchsorted(edges, v, 'right') - 1, with hi folded into the last bin; -1 outside. */
static long alwan_h3_bin(double const *edges, size_t bins, double v) {
    size_t lo = 0, hi = bins + 1;   /* first edge > v lies in [lo, hi] */
    if (!(v >= edges[0] && v <= edges[bins])) return -1;
    if (v == edges[bins]) return (long)bins - 1;
    while (lo < hi) {
        size_t const mid = (lo + hi) / 2;
        if (edges[mid] <= v) lo = mid + 1; else hi = mid;
    }
    return (long)lo - 1;
}

static alwan_status alwan_h3_run(unsigned int *counts_out, size_t bins, void const *rgb, int is_f32,
                                 size_t stride, size_t count, double const *lo, double const *hi) {
    double edges[3][ALWAN_H3_MAX_BINS + 1];
    size_t i, a, k;
    if (!counts_out || !rgb) return ALWAN_E_INVALID;
    if (bins == 0 || bins > ALWAN_H3_MAX_BINS) return ALWAN_E_RANGE;
    if (stride < 3 * (is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64))) return ALWAN_E_INVALID;
    for (a = 0; a < 3; a++) {
        double const step = (hi[a] - lo[a]) / (double)bins;
        if (!(lo[a] < hi[a]) || lo[a] != lo[a] || hi[a] - lo[a] > 1e308) return ALWAN_E_INVALID;
        for (k = 0; k < bins; k++) edges[a][k] = (double)k * step + lo[a];
        edges[a][bins] = hi[a];
    }
    memset(counts_out, 0, bins * bins * bins * sizeof *counts_out);
    for (i = 0; i < count; i++) {
        char const *p = (char const *)rgb + i * stride;
        long b[3];
        for (a = 0; a < 3; a++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)p)[a] : (double)((alwan_f64 const *)p)[a];
            b[a] = alwan_h3_bin(edges[a], bins, v);
            if (b[a] < 0) break;
        }
        if (a == 3) counts_out[((size_t)b[0] * bins + (size_t)b[1]) * bins + (size_t)b[2]]++;
    }
    return ALWAN_OK;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_histogram3d_f64(unsigned int *counts_out, size_t bins, alwan_f64 const *rgb, size_t stride,
                                   size_t count, alwan_f64 const lo[3], alwan_f64 const hi[3]) {
    double l[3], h[3];
    int a;
    if (!lo || !hi) return ALWAN_E_INVALID;
    for (a = 0; a < 3; a++) { l[a] = (double)lo[a]; h[a] = (double)hi[a]; }
    return alwan_h3_run(counts_out, bins, rgb, 0, stride, count, l, h);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_histogram3d_f32(unsigned int *counts_out, size_t bins, alwan_f32 const *rgb, size_t stride,
                                   size_t count, alwan_f32 const lo[3], alwan_f32 const hi[3]) {
    double l[3], h[3];
    int a;
    if (!lo || !hi) return ALWAN_E_INVALID;
    for (a = 0; a < 3; a++) { l[a] = (double)lo[a]; h[a] = (double)hi[a]; }
    return alwan_h3_run(counts_out, bins, rgb, 1, stride, count, l, h);
}
#endif
