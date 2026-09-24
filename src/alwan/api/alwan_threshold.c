/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_threshold_{T} and _u8: one global threshold per channel, as scikit-image's
 * filters.threshold_otsu, _li, _yen, _isodata, _triangle, _minimum and _mean compute it,
 * which suite 221 holds this to.
 *
 * The histogram is scikit-image's: on 8-bit data one bin per level from the channel's
 * minimum to its maximum; on float data `bins` equal bins over [min, max], edges
 * i * ((max - min) / bins) + min in the data's own precision, the last edge max, a value
 * placed as numpy.histogram places it. The bin centres are the midpoints of the edges.
 * Where scikit-image works in float32 (the counts, their running sums, Yen's whole
 * criterion) or in the data's precision (float32 data), this rounds each operation to it,
 * so the arithmetic, and the bin chosen, are the same:
 *
 *   OTSU      the bin centre that maximises the between-class variance
 *             w1 w2 (m1 - m2)^2 of the two classes it splits the histogram into
 *   LI        Li's iterative minimum cross entropy: t <- (mb - mf) / (ln mb - ln mf), the
 *             means of the values at and below t and above it, from the mean, until t
 *             moves by no more than the tolerance; on the values, or on the 8-bit histogram
 *   YEN       the bin centre maximising Yen's criterion
 *             ln(P1 (1 - P1))^2 / (sum_{i<=t} p_i^2 sum_{i>t} p_i^2)
 *   ISODATA   the lowest bin centre t with t <= (mean below t + mean above t) / 2 < t + width
 *   TRIANGLE  the level furthest below the line from the histogram's peak to the end of its
 *             longer tail
 *   MINIMUM   the histogram smoothed by a 3-tap mean (edges repeated) until it has two
 *             maxima, then the lowest bin between them
 *   MEAN      the mean
 *
 * Sums that numpy forms by reduction (the mean, Li's class means) are formed as numpy's
 * pairwise summation forms them. A constant channel returns its value for every method.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Round to the working precision: float when f32 is set. One +, -, * or / done in double
 * and rounded to float equals the float operation. */
static double alwan_th_r(double x, int f32) {
    return f32 ? (double)(float)x : x;
}

static double alwan_th_r32(double x) {
    return (double)(float)x;
}

/* numpy's pairwise summation, seeded with zero as a reduction is. */
static double alwan_th_pairwise(double const *a, size_t n, int f32) {
    if (n < 8) {
        double r = 0.0;
        size_t i;
        for (i = 0; i < n; i++) r = alwan_th_r(r + a[i], f32);
        return r;
    }
    if (n <= 128) {
        double r[8], res;
        size_t i, j;
        for (j = 0; j < 8; j++) r[j] = a[j];
        for (i = 8; i < n - (n % 8); i += 8)
            for (j = 0; j < 8; j++) r[j] = alwan_th_r(r[j] + a[i + j], f32);
        res = alwan_th_r(alwan_th_r(alwan_th_r(r[0] + r[1], f32) + alwan_th_r(r[2] + r[3], f32), f32) +
                             alwan_th_r(alwan_th_r(r[4] + r[5], f32) + alwan_th_r(r[6] + r[7], f32), f32),
                         f32);
        for (; i < n; i++) res = alwan_th_r(res + a[i], f32);
        return res;
    }
    {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan_th_r(alwan_th_pairwise(a, n2, f32) + alwan_th_pairwise(a + n2, n - n2, f32), f32);
    }
}

static double alwan_th_mean(double const *a, size_t n, int f32) {
    return alwan_th_r(alwan_th_r(0.0 + alwan_th_pairwise(a, n, f32), f32) / (double)n, f32);
}

/* The first index of the largest (smallest, with sign -1) value, a NaN first as numpy's. */
static size_t alwan_th_argmax(double const *a, size_t n, double sign) {
    size_t i, best = 0;
    if (a[0] != a[0]) return 0;
    for (i = 1; i < n; i++) {
        if (a[i] != a[i]) return i;
        if (sign * a[i] > sign * a[best]) best = i;
    }
    return best;
}

/* scikit-image's histogram of one channel's n values v (min vmin, max vmax): counts and
 * centres, nb bins. counts and centres have room for max(bins, 256) entries. */
static size_t alwan_th_histogram(double *counts, double *centres, double const *v, size_t n, double vmin, double vmax, size_t bins,
                                 int kind /* 0 f64, 1 f32, 2 u8 */) {
    size_t i, nb;
    if (kind == 2) {
        nb = (size_t)(vmax - vmin) + 1;
        memset(counts, 0, nb * sizeof(double));
        for (i = 0; i < nb; i++) centres[i] = vmin + (double)i;
        for (i = 0; i < n; i++) counts[(size_t)(v[i] - vmin)] += 1.0;
        return nb;
    }
    {
        int const f32 = kind == 1;
        double first = vmin, last = vmax, step, denom;
        double *edges = centres + bins;   /* bins + 1 scratch entries after the centres */
        nb = bins;
        if (first == last) {
            first = alwan_th_r(first - 0.5, f32);
            last = alwan_th_r(last + 0.5, f32);
        }
        denom = alwan_th_r(last - first, f32);
        step = alwan_th_r(denom / (double)nb, f32);
        for (i = 0; i < nb; i++) edges[i] = alwan_th_r(alwan_th_r((double)i * step, f32) + first, f32);
        edges[nb] = last;
        memset(counts, 0, nb * sizeof(double));
        for (i = 0; i < n; i++) {
            double const f = alwan_th_r(alwan_th_r(alwan_th_r(v[i] - first, f32) / denom, f32) * (double)nb, f32);
            size_t k = (size_t)f;
            if (k >= nb) k = nb - 1;
            if (v[i] < edges[k]) k--;
            else if (k != nb - 1 && v[i] >= edges[k + 1]) k++;
            counts[k] += 1.0;
        }
        for (i = 0; i < nb; i++) centres[i] = alwan_th_r(alwan_th_r(edges[i] + edges[i + 1], f32) / 2.0, f32);
        return nb;
    }
}

static int alwan_th_cmp(void const *a, void const *b) {
    double const x = *(double const *)a, y = *(double const *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

/* Li on one channel. work holds 3 n doubles. */
static double alwan_th_li(double const *v, size_t n, double vmin, double tolerance, int kind, double *work, double *counts,
                          double *centres) {
    int const f32 = kind == 1;
    double *img = work, *sel = work + n, *srt = work + 2 * n;
    double t_next, t_curr, tol;
    size_t i;
    for (i = 0; i < n; i++) img[i] = alwan_th_r(v[i] - vmin, f32);
    if (kind == 2) {
        double sum = 0.0, vmax = 0.0;
        size_t nb;
        for (i = 0; i < n; i++) {
            sum += img[i];
            if (img[i] > vmax) vmax = img[i];
        }
        tol = tolerance > 0.0 ? tolerance : 0.5;
        t_next = sum / (double)n;
        t_curr = -2.0 * tol;
        nb = alwan_th_histogram(counts, centres, img, n, 0.0, vmax, 0, 2);
        while (fabs(t_next - t_curr) > tol) {
            double sf = 0.0, wf = 0.0, sb = 0.0, wb = 0.0, mf, mb;
            t_curr = t_next;
            for (i = 0; i < nb; i++) {
                if (centres[i] > t_curr) {
                    sf += counts[i] * centres[i];
                    wf += counts[i];
                } else {
                    sb += counts[i] * centres[i];
                    wb += counts[i];
                }
            }
            mf = sf / wf;
            mb = sb / wb;
            if (mb == 0.0) break;
            t_next = (mb - mf) / (log(mb) - log(mf));
        }
        return t_next + vmin;
    }
    if (tolerance > 0.0) {
        tol = tolerance;
    } else {
        double gap = -1.0;
        memcpy(srt, img, n * sizeof(double));
        qsort(srt, n, sizeof(double), alwan_th_cmp);
        for (i = 1; i < n; i++) {
            double const d = alwan_th_r(srt[i] - srt[i - 1], f32);
            if (d > 0.0 && (gap < 0.0 || d < gap)) gap = d;
        }
        tol = alwan_th_r(gap / 2.0, f32);
    }
    t_next = alwan_th_mean(img, n, f32);
    t_curr = alwan_th_r(-2.0 * tol, f32);
    while (alwan_th_r(fabs(alwan_th_r(t_next - t_curr, f32)), f32) > tol) {
        size_t nf = 0, nbk = 0;
        double mf, mb;
        t_curr = t_next;
        for (i = 0; i < n; i++)
            if (img[i] > t_curr) sel[nf++] = img[i];
        /* t stays strictly between the class means, so neither class empties */
        if (nf == 0 || nf == n) break;
        mf = alwan_th_mean(sel, nf, f32);
        for (i = 0; i < n; i++)
            if (!(img[i] > t_curr)) sel[nbk++] = img[i];
        mb = alwan_th_mean(sel, nbk, f32);
        if (mb == 0.0) break;
        if (f32) {
            t_next = alwan_th_r32(alwan_th_r32(mb - mf) / alwan_th_r32((double)logf((float)mb) - (double)logf((float)mf)));
        } else {
            t_next = (mb - mf) / (log(mb) - log(mf));
        }
    }
    return alwan_th_r(t_next + vmin, f32);
}

/* One channel's threshold. counts, centres and cs hold max(bins, 256) + 1 doubles each
 * (centres twice that, for the edges); work 3 n. */
static alwan_status alwan_th_channel(double *out, double const *v, size_t n, alwan_threshold_method method, size_t bins,
                                     double tolerance, size_t max_iterations, int kind, double *counts, double *centres, double *a,
                                     double *b, double *c, double *work) {
    int const f32 = kind == 1, ct32 = kind == 1;   /* the centres' precision */
    double vmin = v[0], vmax = v[0];
    size_t i, nb;
    for (i = 1; i < n; i++) {
        if (v[i] < vmin) vmin = v[i];
        if (v[i] > vmax) vmax = v[i];
    }
    if (vmin == vmax) {
        *out = v[0];
        return ALWAN_OK;
    }
    if (method == ALWAN_THRESHOLD_MEAN) {
        memcpy(work, v, n * sizeof(double));
        *out = alwan_th_mean(work, n, f32);
        return ALWAN_OK;
    }
    if (method == ALWAN_THRESHOLD_LI) {
        *out = alwan_th_li(v, n, vmin, tolerance, kind, work, counts, centres);
        return ALWAN_OK;
    }
    nb = alwan_th_histogram(counts, centres, v, n, vmin, vmax, bins, kind);
    if (nb == 1) {
        *out = centres[0];
        return ALWAN_OK;
    }
    switch (method) {
    case ALWAN_THRESHOLD_OTSU: {
        /* a: w1, b: w2, c: variance; the class sums in the centres' precision */
        double s = 0.0, m1, m2;
        a[0] = counts[0];
        for (i = 1; i < nb; i++) a[i] = alwan_th_r32(a[i - 1] + counts[i]);
        b[nb - 1] = counts[nb - 1];
        for (i = nb - 1; i-- > 0;) b[i] = alwan_th_r32(b[i + 1] + counts[i]);
        /* work: the running top sums m2 needs, from the top */
        work[nb - 1] = alwan_th_r(counts[nb - 1] * centres[nb - 1], ct32);
        for (i = nb - 1; i-- > 0;) work[i] = alwan_th_r(work[i + 1] + alwan_th_r(counts[i] * centres[i], ct32), ct32);
        for (i = 0; i + 1 < nb; i++) {
            double d;
            s = alwan_th_r(s + alwan_th_r(counts[i] * centres[i], ct32), ct32);
            m1 = alwan_th_r(s / a[i], ct32);
            m2 = alwan_th_r(work[i + 1] / b[i + 1], ct32);
            d = alwan_th_r(m1 - m2, ct32);
            c[i] = alwan_th_r(alwan_th_r32(a[i] * b[i + 1]) * alwan_th_r(d * d, ct32), ct32);
        }
        *out = centres[alwan_th_argmax(c, nb - 1, 1.0)];
        return ALWAN_OK;
    }
    case ALWAN_THRESHOLD_YEN: {
        /* all float32: a: P1, b: P1_sq, c: P2_sq, then the criterion over a */
        double total = alwan_th_pairwise(counts, nb, 1), p;
        total = alwan_th_r32(0.0 + total);
        for (i = 0; i < nb; i++) {
            p = alwan_th_r32(counts[i] / total);
            a[i] = i ? alwan_th_r32(a[i - 1] + p) : p;
            b[i] = i ? alwan_th_r32(b[i - 1] + alwan_th_r32(p * p)) : alwan_th_r32(p * p);
        }
        for (i = nb; i-- > 0;) {
            p = alwan_th_r32(counts[i] / total);
            c[i] = i + 1 < nb ? alwan_th_r32(c[i + 1] + alwan_th_r32(p * p)) : alwan_th_r32(p * p);
        }
        for (i = 0; i + 1 < nb; i++) {
            double const inv = alwan_th_r32(1.0 / alwan_th_r32(b[i] * c[i + 1]));
            double const x = alwan_th_r32(a[i] * alwan_th_r32(1.0 - a[i]));
            work[i] = (double)logf((float)alwan_th_r32(inv * alwan_th_r32(x * x)));
        }
        *out = centres[alwan_th_argmax(work, nb - 1, 1.0)];
        return ALWAN_OK;
    }
    case ALWAN_THRESHOLD_ISODATA: {
        double const width = alwan_th_r(centres[1] - centres[0], ct32);
        double ci_last;
        a[0] = counts[0];
        for (i = 1; i < nb; i++) a[i] = alwan_th_r32(a[i - 1] + counts[i]);
        b[0] = alwan_th_r(counts[0] * centres[0], ct32);
        for (i = 1; i < nb; i++) b[i] = alwan_th_r(b[i - 1] + alwan_th_r(counts[i] * centres[i], ct32), ct32);
        ci_last = b[nb - 1];
        for (i = 0; i + 1 < nb; i++) {
            double const low = alwan_th_r(b[i] / a[i], ct32);
            double const high = alwan_th_r(alwan_th_r(ci_last - b[i], ct32) / alwan_th_r32(a[nb - 1] - a[i]), ct32);
            double const mean = alwan_th_r(alwan_th_r(low + high, ct32) / 2.0, ct32);
            double const dist = alwan_th_r(mean - centres[i], ct32);
            if (dist >= 0.0 && dist < width) {
                *out = centres[i];
                return ALWAN_OK;
            }
        }
        return ALWAN_E_RANGE;
    }
    case ALWAN_THRESHOLD_TRIANGLE: {
        size_t peak = alwan_th_argmax(counts, nb, 1.0), low = 0, high = nb - 1, width, level;
        double ph = counts[peak], norm, phn, wn;
        int flip;
        while (counts[low] == 0.0) low++;
        while (counts[high] == 0.0) high--;
        if (low == high) {
            *out = v[0];
            return ALWAN_OK;
        }
        flip = peak - low < high - peak;
        if (flip) {
            for (i = 0; i < nb / 2; i++) {
                double const t = counts[i];
                counts[i] = counts[nb - 1 - i];
                counts[nb - 1 - i] = t;
            }
            low = nb - high - 1;
            peak = nb - peak - 1;
        }
        width = peak - low;
        norm = sqrt(ph * ph + (double)width * (double)width);
        phn = ph / norm;
        wn = (double)width / norm;
        for (i = 0; i < width; i++) work[i] = phn * (double)i - wn * counts[i + low];
        level = alwan_th_argmax(work, width, 1.0) + low;
        if (flip) level = nb - level - 1;
        *out = centres[level];
        return ALWAN_OK;
    }
    case ALWAN_THRESHOLD_MINIMUM: {
        size_t it, maxima[3], nmax = 0, lo;
        for (i = 0; i < nb; i++) a[i] = alwan_th_r32(counts[i]);
        for (it = 0; it < max_iterations; it++) {
            int dir = 1;
            /* scipy's uniform_filter1d, size 3, mode reflect: a running sum in double */
            double t = a[0] + a[0] + a[1];
            b[0] = alwan_th_r32(t / 3.0);
            for (i = 1; i < nb; i++) {
                t += (i + 1 < nb ? a[i + 1] : a[nb - 1]) - a[i >= 2 ? i - 2 : 0];
                b[i] = alwan_th_r32(t / 3.0);
            }
            memcpy(a, b, nb * sizeof(double));
            nmax = 0;
            for (i = 0; i + 1 < nb; i++) {
                if (dir > 0) {
                    if (a[i + 1] < a[i]) {
                        dir = -1;
                        if (nmax < 3) maxima[nmax] = i;
                        nmax++;
                    }
                } else if (a[i + 1] > a[i]) {
                    dir = 1;
                }
            }
            if (nmax < 3) break;
        }
        if (nmax != 2 || it >= max_iterations - 1) return ALWAN_E_RANGE;
        lo = maxima[0] + alwan_th_argmax(a + maxima[0], maxima[1] - maxima[0] + 1, -1.0);
        *out = centres[lo];
        return ALWAN_OK;
    }
    default:
        return ALWAN_E_INVALID;
    }
}

static alwan_status alwan_th_run(double *threshold_out, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_threshold_method method, alwan_threshold_params const *params, int kind) {
    alwan_threshold_params const zero = { 0 };
    alwan_threshold_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const bins = kind == 2 ? 256 : (p->bins ? p->bins : 256);
    size_t const max_iterations = p->max_iterations ? p->max_iterations : 10000;
    size_t const n = w * h, hb = (bins > 256 ? bins : 256) + 1;
    double *buf, *v, *counts, *centres, *a, *b, *c, *work;
    size_t x, y, k;
    alwan_status st = ALWAN_OK;
    if (!threshold_out || !src || w == 0 || h == 0 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w) return ALWAN_E_INVALID;
    if ((unsigned)method > (unsigned)ALWAN_THRESHOLD_MEAN) return ALWAN_E_INVALID;
    if (bins < 2 || bins > 65536 || !(p->tolerance >= 0.0) || max_iterations > 1000000) return ALWAN_E_RANGE;
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(4 * n + 6 * hb, sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    v = buf;
    work = v + n;
    counts = work + 3 * n;
    centres = counts + hb;
    a = centres + 2 * hb;
    b = a + hb;
    c = b + hb;
    for (k = 0; k < ch && st == ALWAN_OK; k++) {
        for (y = 0; y < h; y++) {
            char const *row = (char const *)src + y * src_rs;
            for (x = 0; x < w; x++) {
                double const s = kind == 0 ? ((alwan_f64 const *)row)[x * ch + k]
                                 : kind == 1 ? (double)((alwan_f32 const *)row)[x * ch + k]
                                             : (double)((unsigned char const *)row)[x * ch + k];
                if (!(s - s == 0.0)) {   /* NaN or infinite */
                    ALWAN_FREE(buf);
                    return ALWAN_E_INVALID;
                }
                v[y * w + x] = s;
            }
        }
        st = alwan_th_channel(&threshold_out[k], v, n, method, bins, p->tolerance, max_iterations, kind, counts, centres, a, b, c,
                              work);
    }
    ALWAN_FREE(buf);
    return st;
}

alwan_status alwan_threshold_u8(double *threshold_out, unsigned char const *src, size_t src_row_stride, size_t channels, size_t width,
                                size_t height, alwan_threshold_method method, alwan_threshold_params const *params) {
    return alwan_th_run(threshold_out, src, src_row_stride, channels, width, height, method, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_threshold_f64(double *threshold_out, alwan_f64 const *src, size_t src_row_stride, size_t channels, size_t width,
                                 size_t height, alwan_threshold_method method, alwan_threshold_params const *params) {
    return alwan_th_run(threshold_out, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_threshold_f32(double *threshold_out, alwan_f32 const *src, size_t src_row_stride, size_t channels, size_t width,
                                 size_t height, alwan_threshold_method method, alwan_threshold_params const *params) {
    return alwan_th_run(threshold_out, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
