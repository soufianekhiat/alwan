/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Three more colour quantisers behind alwan_quantize_ex_u8, each alwan's own code from its
 * paper:
 *
 *   KMEANS          Lloyd's algorithm over the pixels, as scikit-learn's KMeans with
 *                   algorithm="lloyd", n_init=1 and an explicit init runs it: the
 *                   distance ||c||^2 - 2 x.c, the first nearest centre on a tie, centres
 *                   as the sum times 1 / weight, empty clusters moved to the pixels
 *                   farthest from their centres, a stop when the labels repeat or the
 *                   summed squared centre shift falls to tol times the mean channel
 *                   variance, and a last E-step when the stop was not on repeated labels.
 *                   The pixels are handled as their distinct colours weighted by count,
 *                   which sums the same integers. Suite 297 holds it to scikit-learn.
 *   WU              Wu 1991, "Efficient statistical computations for optimal color
 *                   quantization" (Graphics Gems II): moments over a 33^3 cube of the top
 *                   five bits, boxes split where the summed squared means of the two
 *                   halves are largest, the box of largest variance split next. The
 *                   paper's own listing keeps the second moment and the variances in
 *                   single precision; alwan keeps them in double, which can choose a
 *                   different cut where two are within float rounding of each other.
 *   OCTREE_CLASSIC  Gervautz and Purgathofer 1988: an octree of depth 8 over the
 *                   distinct colours, reduced from the deepest level, the node with the
 *                   fewest pixels first (ties to the lower position in the tree), until
 *                   at most max_colors leaves are left. The paper reduces while it
 *                   inserts, so its result depends on the pixel order; reducing the full
 *                   tree does not.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- distinct colours */

typedef struct {
    size_t n;                 /* distinct colours */
    double *col;              /* n x 3 */
    double *w;                /* pixel count of each */
    size_t *first;            /* first pixel index of each */
    unsigned int *of_pixel;   /* each pixel's distinct colour */
} alwan_qx_colours;

static int alwan_qx_cmp_u64(void const *a, void const *b) {
    unsigned long long const x = *(unsigned long long const *)a, y = *(unsigned long long const *)b;
    return (x > y) - (x < y);
}

static void alwan_qx_colours_free(alwan_qx_colours *c) {
    if (c->col) ALWAN_FREE(c->col);
    if (c->w) ALWAN_FREE(c->w);
    if (c->first) ALWAN_FREE(c->first);
    if (c->of_pixel) ALWAN_FREE(c->of_pixel);
    memset(c, 0, sizeof *c);
}

static alwan_status alwan_qx_colours_build(alwan_qx_colours *c, unsigned char const *rgb, size_t stride,
                                           size_t count) {
    unsigned long long *keys;
    size_t i, n = 0;
    memset(c, 0, sizeof *c);
    if (count > 0xFFFFFFFFu) return ALWAN_E_RANGE;
    keys = (unsigned long long *)ALWAN_ALLOC(sizeof(unsigned long long) * count, sizeof(unsigned long long));
    c->of_pixel = (unsigned int *)ALWAN_ALLOC(sizeof(unsigned int) * count, sizeof(unsigned int));
    if (!keys || !c->of_pixel) {
        if (keys) ALWAN_FREE(keys);
        alwan_qx_colours_free(c);
        return ALWAN_E_NOMEM;
    }
    for (i = 0; i < count; i++) {
        unsigned char const *p = rgb + i * stride;
        unsigned long long const key = ((unsigned long long)p[0] << 16) | ((unsigned long long)p[1] << 8) | p[2];
        keys[i] = (key << 32) | (unsigned long long)i;
    }
    qsort(keys, count, sizeof keys[0], alwan_qx_cmp_u64);
    for (i = 0; i < count; i++) {
        if (i == 0 || (keys[i] >> 32) != (keys[i - 1] >> 32)) n++;
    }
    c->n = n;
    c->col = (double *)ALWAN_ALLOC(sizeof(double) * 3 * n, sizeof(double));
    c->w = (double *)ALWAN_ALLOC(sizeof(double) * n, sizeof(double));
    c->first = (size_t *)ALWAN_ALLOC(sizeof(size_t) * n, sizeof(size_t));
    if (!c->col || !c->w || !c->first) {
        ALWAN_FREE(keys);
        alwan_qx_colours_free(c);
        return ALWAN_E_NOMEM;
    }
    n = 0;
    for (i = 0; i < count; i++) {
        unsigned long long const key = keys[i] >> 32;
        size_t const pix = keys[i] & 0xFFFFFFFFull;
        if (i == 0 || key != (keys[i - 1] >> 32)) {
            c->col[3 * n + 0] = (double)((key >> 16) & 0xFF);
            c->col[3 * n + 1] = (double)((key >> 8) & 0xFF);
            c->col[3 * n + 2] = (double)(key & 0xFF);
            c->w[n] = 0.0;
            c->first[n] = pix;             /* sorted by index within a colour */
            n++;
        }
        c->w[n - 1] += 1.0;
        c->of_pixel[pix] = (unsigned int)(n - 1);
    }
    ALWAN_FREE(keys);
    return ALWAN_OK;
}

static unsigned char alwan_qx_round_u8(double v) {
    double const r = v + 0.5;
    if (!(r > 0.0)) return 0;
    if (r >= 255.0) return 255;
    return (unsigned char)(unsigned int)r;
}

/* ---------------------------------------------------------------- k-means */

/* E-step over the distinct colours: label each with the first centre of least
 * ||c||^2 - 2 x.c, as scikit-learn's dense chunk update computes it. */
static void alwan_qx_assign(unsigned int *label, alwan_qx_colours const *c, double const *cent, double const *cn,
                            size_t k) {
    size_t j, m;
    for (j = 0; j < c->n; j++) {
        double const *x = c->col + 3 * j;
        double best = 0.0;
        unsigned int bl = 0;
        for (m = 0; m < k; m++) {
            double const *cm = cent + 3 * m;
            double const dot = x[0] * cm[0] + x[1] * cm[1] + x[2] * cm[2];
            double const d = -2.0 * dot + cn[m];
            if (m == 0 || d < best) { best = d; bl = (unsigned int)m; }
        }
        label[j] = bl;
    }
}

static void alwan_qx_norms(double *cn, double const *cent, size_t k) {
    size_t m;
    for (m = 0; m < k; m++) {
        double const *cm = cent + 3 * m;
        cn[m] = cm[0] * cm[0] + cm[1] * cm[1] + cm[2] * cm[2];
    }
}

typedef struct {
    double dist;
    size_t colour;
} alwan_qx_far;

static int alwan_qx_cmp_far(void const *a, void const *b) {
    alwan_qx_far const *x = (alwan_qx_far const *)a, *y = (alwan_qx_far const *)b;
    if (x->dist != y->dist) return x->dist < y->dist ? 1 : -1;      /* farthest first */
    return 0;                                                       /* resolved below */
}

static alwan_status alwan_qx_kmeans(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                    unsigned char const *rgb, size_t stride, size_t count, size_t max_colors,
                                    alwan_quantize_params const *pr) {
    alwan_qx_colours c;
    double *cent = NULL, *cnew = NULL, *cn = NULL, *win = NULL, *shift = NULL, *init_buf = NULL;
    unsigned int *label = NULL, *label_old = NULL;
    alwan_qx_far *far = NULL;
    size_t k, j, m, it, max_iter, iters = 0;
    double tol, mean[3], var_sum;
    int strict = 0;
    alwan_status st;
    double const *init = pr ? pr->initial_centres : NULL;

    if (init) {
        if (pr->initial_count == 0 || pr->initial_count > max_colors) return ALWAN_E_INVALID;
        k = pr->initial_count;
        for (m = 0; m < 3 * k; m++) {
            if (!(init[m] == init[m]) || init[m] < -1e300 || init[m] > 1e300) return ALWAN_E_INVALID;
        }
    } else {
        unsigned char *pal = (unsigned char *)ALWAN_ALLOC(3 * max_colors, 1);
        size_t got = 0;
        if (!pal) return ALWAN_E_NOMEM;
        st = alwan_quantize_u8(pal, &got, NULL, rgb, stride, count, max_colors, ALWAN_QUANTIZE_MEDIAN_CUT);
        if (st != ALWAN_OK) { ALWAN_FREE(pal); return st; }
        init_buf = (double *)ALWAN_ALLOC(sizeof(double) * 3 * got, sizeof(double));
        if (!init_buf) { ALWAN_FREE(pal); return ALWAN_E_NOMEM; }
        for (m = 0; m < 3 * got; m++) init_buf[m] = (double)pal[m];
        ALWAN_FREE(pal);
        k = got;
        init = init_buf;
    }
    max_iter = (pr && pr->max_iterations) ? pr->max_iterations : 300;
    tol = (pr && pr->tolerance > 0.0) ? pr->tolerance : 1e-4;

    st = alwan_qx_colours_build(&c, rgb, stride, count);
    if (st != ALWAN_OK) { if (init_buf) ALWAN_FREE(init_buf); return st; }

    cent = (double *)ALWAN_ALLOC(sizeof(double) * 3 * k, sizeof(double));
    cnew = (double *)ALWAN_ALLOC(sizeof(double) * 3 * k, sizeof(double));
    cn = (double *)ALWAN_ALLOC(sizeof(double) * k, sizeof(double));
    win = (double *)ALWAN_ALLOC(sizeof(double) * k, sizeof(double));
    shift = (double *)ALWAN_ALLOC(sizeof(double) * k, sizeof(double));
    label = (unsigned int *)ALWAN_ALLOC(sizeof(unsigned int) * c.n, sizeof(unsigned int));
    label_old = (unsigned int *)ALWAN_ALLOC(sizeof(unsigned int) * c.n, sizeof(unsigned int));
    far = (alwan_qx_far *)ALWAN_ALLOC(sizeof(alwan_qx_far) * c.n, sizeof(double));
    if (!cent || !cnew || !cn || !win || !shift || !label || !label_old || !far) { st = ALWAN_E_NOMEM; goto done; }
    memcpy(cent, init, sizeof(double) * 3 * k);
    for (j = 0; j < c.n; j++) label_old[j] = 0xFFFFFFFFu;     /* no label yet */

    /* tol is relative to the mean of the per-channel variances over every pixel */
    var_sum = 0.0;
    {
        int ch;
        for (ch = 0; ch < 3; ch++) {
            double s = 0.0, v = 0.0;
            for (j = 0; j < c.n; j++) s += c.col[3 * j + ch] * c.w[j];
            mean[ch] = s / (double)count;
            for (j = 0; j < c.n; j++) {
                double const d = c.col[3 * j + ch] - mean[ch];
                v += d * d * c.w[j];
            }
            var_sum += v / (double)count;
        }
    }
    tol *= var_sum / 3.0;

    for (it = 0; it < max_iter; it++) {
        size_t n_empty = 0;
        alwan_qx_norms(cn, cent, k);
        alwan_qx_assign(label, &c, cent, cn, k);
        memset(cnew, 0, sizeof(double) * 3 * k);
        memset(win, 0, sizeof(double) * k);
        for (j = 0; j < c.n; j++) {
            size_t const l = label[j];
            win[l] += c.w[j];
            cnew[3 * l + 0] += c.col[3 * j + 0] * c.w[j];
            cnew[3 * l + 1] += c.col[3 * j + 1] * c.w[j];
            cnew[3 * l + 2] += c.col[3 * j + 2] * c.w[j];
        }
        for (m = 0; m < k; m++) if (win[m] == 0.0) n_empty++;
        if (n_empty) {
            /* scikit-learn moves each empty cluster onto one of the pixels farthest from
             * the centre they were labelled with, farthest first; pixels of one colour are
             * taken in order, and equal distances go to the colour seen first */
            size_t e, taken = 0, fi = 0, used_of_colour = 0;
            for (j = 0; j < c.n; j++) {
                double const *x = c.col + 3 * j, *cl = cent + 3 * label[j];
                double const d0 = x[0] - cl[0], d1 = x[1] - cl[1], d2 = x[2] - cl[2];
                far[j].dist = d0 * d0 + d1 * d1 + d2 * d2;
                far[j].colour = j;
            }
            qsort(far, c.n, sizeof far[0], alwan_qx_cmp_far);
            /* stable order within equal distances: by the colour's first pixel */
            {
                size_t a = 0;
                while (a < c.n) {
                    size_t b = a + 1, x, y;
                    while (b < c.n && far[b].dist == far[a].dist) b++;
                    for (x = a + 1; x < b; x++) {
                        alwan_qx_far const t = far[x];
                        y = x;
                        while (y > a && c.first[far[y - 1].colour] > c.first[t.colour]) { far[y] = far[y - 1]; y--; }
                        far[y] = t;
                    }
                    a = b;
                }
            }
            for (e = 0; e < k && taken < n_empty; e++) {
                size_t col_j, old;
                if (win[e] != 0.0) continue;
                while (fi < c.n && used_of_colour >= (size_t)c.w[far[fi].colour]) { fi++; used_of_colour = 0; }
                if (fi >= c.n) break;
                col_j = far[fi].colour;
                used_of_colour++;
                old = label[col_j];
                cnew[3 * old + 0] -= c.col[3 * col_j + 0];
                cnew[3 * old + 1] -= c.col[3 * col_j + 1];
                cnew[3 * old + 2] -= c.col[3 * col_j + 2];
                win[old] -= 1.0;
                cnew[3 * e + 0] = c.col[3 * col_j + 0];
                cnew[3 * e + 1] = c.col[3 * col_j + 1];
                cnew[3 * e + 2] = c.col[3 * col_j + 2];
                win[e] = 1.0;
                taken++;
            }
        }
        for (m = 0; m < k; m++) {
            if (win[m] > 0.0) {
                double const a = 1.0 / win[m];
                cnew[3 * m + 0] *= a;
                cnew[3 * m + 1] *= a;
                cnew[3 * m + 2] *= a;
            }
            {
                double const d0 = cnew[3 * m + 0] - cent[3 * m + 0];
                double const d1 = cnew[3 * m + 1] - cent[3 * m + 1];
                double const d2 = cnew[3 * m + 2] - cent[3 * m + 2];
                shift[m] = ALWAN_SQRT_F64(d0 * d0 + d1 * d1 + d2 * d2);
            }
        }
        memcpy(cent, cnew, sizeof(double) * 3 * k);
        iters = it + 1;
        if (memcmp(label, label_old, sizeof(unsigned int) * c.n) == 0) { strict = 1; break; }
        {
            double tot = 0.0;
            for (m = 0; m < k; m++) tot += shift[m] * shift[m];
            if (tot <= tol) break;
        }
        memcpy(label_old, label, sizeof(unsigned int) * c.n);
    }
    if (!strict) {
        alwan_qx_norms(cn, cent, k);
        alwan_qx_assign(label, &c, cent, cn, k);
    }

    for (m = 0; m < k; m++) {
        palette_out[3 * m + 0] = alwan_qx_round_u8(cent[3 * m + 0]);
        palette_out[3 * m + 1] = alwan_qx_round_u8(cent[3 * m + 1]);
        palette_out[3 * m + 2] = alwan_qx_round_u8(cent[3 * m + 2]);
    }
    if (pr && pr->centres_out) memcpy(pr->centres_out, cent, sizeof(double) * 3 * k);
    if (pr && pr->iterations_out) *pr->iterations_out = iters;
    if (index_out) {
        size_t i;
        for (i = 0; i < count; i++) index_out[i] = label[c.of_pixel[i]];
    }
    *count_out = k;
    st = ALWAN_OK;
done:
    if (cent) ALWAN_FREE(cent);
    if (cnew) ALWAN_FREE(cnew);
    if (cn) ALWAN_FREE(cn);
    if (win) ALWAN_FREE(win);
    if (shift) ALWAN_FREE(shift);
    if (label) ALWAN_FREE(label);
    if (label_old) ALWAN_FREE(label_old);
    if (far) ALWAN_FREE(far);
    if (init_buf) ALWAN_FREE(init_buf);
    alwan_qx_colours_free(&c);
    return st;
}

/* ---------------------------------------------------------------- Wu 1991 */

#define ALWAN_WU_N 33u
#define ALWAN_WU_CELLS (ALWAN_WU_N * ALWAN_WU_N * ALWAN_WU_N)

typedef struct {
    unsigned int r0, r1, g0, g1, b0, b1;   /* r0 < r <= r1, as the listing */
    unsigned int vol;
} alwan_wu_box;

typedef struct {
    long long *wt, *mr, *mg, *mb;
    double *m2;
} alwan_wu_moments;

static size_t alwan_wu_ix(unsigned int r, unsigned int g, unsigned int b) {
    return ((size_t)r * ALWAN_WU_N + (size_t)g) * ALWAN_WU_N + (size_t)b;
}

static long long alwan_wu_vol(alwan_wu_box const *c, long long const *m) {
    return m[alwan_wu_ix(c->r1, c->g1, c->b1)] - m[alwan_wu_ix(c->r1, c->g1, c->b0)]
         - m[alwan_wu_ix(c->r1, c->g0, c->b1)] + m[alwan_wu_ix(c->r1, c->g0, c->b0)]
         - m[alwan_wu_ix(c->r0, c->g1, c->b1)] + m[alwan_wu_ix(c->r0, c->g1, c->b0)]
         + m[alwan_wu_ix(c->r0, c->g0, c->b1)] - m[alwan_wu_ix(c->r0, c->g0, c->b0)];
}

static double alwan_wu_vol_d(alwan_wu_box const *c, double const *m) {
    return m[alwan_wu_ix(c->r1, c->g1, c->b1)] - m[alwan_wu_ix(c->r1, c->g1, c->b0)]
         - m[alwan_wu_ix(c->r1, c->g0, c->b1)] + m[alwan_wu_ix(c->r1, c->g0, c->b0)]
         - m[alwan_wu_ix(c->r0, c->g1, c->b1)] + m[alwan_wu_ix(c->r0, c->g1, c->b0)]
         + m[alwan_wu_ix(c->r0, c->g0, c->b1)] - m[alwan_wu_ix(c->r0, c->g0, c->b0)];
}

/* the part of the box's sum below its low face on one axis (the listing's Bottom) */
static long long alwan_wu_bottom(alwan_wu_box const *c, int dir, long long const *m) {
    switch (dir) {
    case 0:
        return -m[alwan_wu_ix(c->r0, c->g1, c->b1)] + m[alwan_wu_ix(c->r0, c->g1, c->b0)]
               + m[alwan_wu_ix(c->r0, c->g0, c->b1)] - m[alwan_wu_ix(c->r0, c->g0, c->b0)];
    case 1:
        return -m[alwan_wu_ix(c->r1, c->g0, c->b1)] + m[alwan_wu_ix(c->r1, c->g0, c->b0)]
               + m[alwan_wu_ix(c->r0, c->g0, c->b1)] - m[alwan_wu_ix(c->r0, c->g0, c->b0)];
    default:
        return -m[alwan_wu_ix(c->r1, c->g1, c->b0)] + m[alwan_wu_ix(c->r1, c->g0, c->b0)]
               + m[alwan_wu_ix(c->r0, c->g1, c->b0)] - m[alwan_wu_ix(c->r0, c->g0, c->b0)];
    }
}

/* the box's sum up to plane pos on one axis (the listing's Top) */
static long long alwan_wu_top(alwan_wu_box const *c, int dir, unsigned int pos, long long const *m) {
    switch (dir) {
    case 0:
        return m[alwan_wu_ix(pos, c->g1, c->b1)] - m[alwan_wu_ix(pos, c->g1, c->b0)]
               - m[alwan_wu_ix(pos, c->g0, c->b1)] + m[alwan_wu_ix(pos, c->g0, c->b0)];
    case 1:
        return m[alwan_wu_ix(c->r1, pos, c->b1)] - m[alwan_wu_ix(c->r1, pos, c->b0)]
               - m[alwan_wu_ix(c->r0, pos, c->b1)] + m[alwan_wu_ix(c->r0, pos, c->b0)];
    default:
        return m[alwan_wu_ix(c->r1, c->g1, pos)] - m[alwan_wu_ix(c->r1, c->g0, pos)]
               - m[alwan_wu_ix(c->r0, c->g1, pos)] + m[alwan_wu_ix(c->r0, c->g0, pos)];
    }
}

static double alwan_wu_var(alwan_wu_box const *c, alwan_wu_moments const *mo) {
    double const dr = (double)alwan_wu_vol(c, mo->mr);
    double const dg = (double)alwan_wu_vol(c, mo->mg);
    double const db = (double)alwan_wu_vol(c, mo->mb);
    double const xx = alwan_wu_vol_d(c, mo->m2);
    return xx - (dr * dr + dg * dg + db * db) / (double)alwan_wu_vol(c, mo->wt);
}

static double alwan_wu_maximize(alwan_wu_box const *c, int dir, unsigned int first, unsigned int last, int *cut,
                                long long wr, long long wg, long long wb, long long ww,
                                alwan_wu_moments const *mo) {
    long long const br = alwan_wu_bottom(c, dir, mo->mr), bg = alwan_wu_bottom(c, dir, mo->mg);
    long long const bb = alwan_wu_bottom(c, dir, mo->mb), bw = alwan_wu_bottom(c, dir, mo->wt);
    double max = 0.0;
    unsigned int i;
    *cut = -1;
    for (i = first; i < last; i++) {
        long long hr = br + alwan_wu_top(c, dir, i, mo->mr);
        long long hg = bg + alwan_wu_top(c, dir, i, mo->mg);
        long long hb = bb + alwan_wu_top(c, dir, i, mo->mb);
        long long hw = bw + alwan_wu_top(c, dir, i, mo->wt);
        double temp;
        if (hw == 0) continue;
        temp = ((double)hr * (double)hr + (double)hg * (double)hg + (double)hb * (double)hb) / (double)hw;
        hr = wr - hr; hg = wg - hg; hb = wb - hb; hw = ww - hw;
        if (hw == 0) continue;
        temp += ((double)hr * (double)hr + (double)hg * (double)hg + (double)hb * (double)hb) / (double)hw;
        if (temp > max) { max = temp; *cut = (int)i; }
    }
    return max;
}

static int alwan_wu_cut(alwan_wu_box *s1, alwan_wu_box *s2, alwan_wu_moments const *mo) {
    long long const wr = alwan_wu_vol(s1, mo->mr), wg = alwan_wu_vol(s1, mo->mg);
    long long const wb = alwan_wu_vol(s1, mo->mb), ww = alwan_wu_vol(s1, mo->wt);
    int cutr, cutg, cutb, dir;
    double const maxr = alwan_wu_maximize(s1, 0, s1->r0 + 1, s1->r1, &cutr, wr, wg, wb, ww, mo);
    double const maxg = alwan_wu_maximize(s1, 1, s1->g0 + 1, s1->g1, &cutg, wr, wg, wb, ww, mo);
    double const maxb = alwan_wu_maximize(s1, 2, s1->b0 + 1, s1->b1, &cutb, wr, wg, wb, ww, mo);
    if (maxr >= maxg && maxr >= maxb) {
        dir = 0;
        if (cutr < 0) return 0;            /* the box cannot be split */
    } else if (maxg >= maxr && maxg >= maxb) {
        dir = 1;
    } else {
        dir = 2;
    }
    s2->r1 = s1->r1; s2->g1 = s1->g1; s2->b1 = s1->b1;
    if (dir == 0) {
        s2->r0 = s1->r1 = (unsigned int)cutr; s2->g0 = s1->g0; s2->b0 = s1->b0;
    } else if (dir == 1) {
        s2->g0 = s1->g1 = (unsigned int)cutg; s2->r0 = s1->r0; s2->b0 = s1->b0;
    } else {
        s2->b0 = s1->b1 = (unsigned int)cutb; s2->r0 = s1->r0; s2->g0 = s1->g0;
    }
    s1->vol = (s1->r1 - s1->r0) * (s1->g1 - s1->g0) * (s1->b1 - s1->b0);
    s2->vol = (s2->r1 - s2->r0) * (s2->g1 - s2->g0) * (s2->b1 - s2->b0);
    return 1;
}

static alwan_status alwan_qx_wu(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                unsigned char const *rgb, size_t stride, size_t count, size_t max_colors) {
    alwan_wu_moments mo;
    alwan_wu_box *cube = NULL;
    double *vv = NULL;
    unsigned short *tag = NULL;
    size_t i, K = max_colors, next = 0;
    alwan_status st = ALWAN_E_NOMEM;
    memset(&mo, 0, sizeof mo);
    if (K > 65535) K = 65535;                       /* the tag table holds 16-bit entries */
    mo.wt = (long long *)ALWAN_ALLOC(sizeof(long long) * ALWAN_WU_CELLS, sizeof(long long));
    mo.mr = (long long *)ALWAN_ALLOC(sizeof(long long) * ALWAN_WU_CELLS, sizeof(long long));
    mo.mg = (long long *)ALWAN_ALLOC(sizeof(long long) * ALWAN_WU_CELLS, sizeof(long long));
    mo.mb = (long long *)ALWAN_ALLOC(sizeof(long long) * ALWAN_WU_CELLS, sizeof(long long));
    mo.m2 = (double *)ALWAN_ALLOC(sizeof(double) * ALWAN_WU_CELLS, sizeof(double));
    cube = (alwan_wu_box *)ALWAN_ALLOC(sizeof(alwan_wu_box) * K, sizeof(unsigned int));
    vv = (double *)ALWAN_ALLOC(sizeof(double) * K, sizeof(double));
    tag = (unsigned short *)ALWAN_ALLOC(sizeof(unsigned short) * ALWAN_WU_CELLS, sizeof(unsigned short));
    if (!mo.wt || !mo.mr || !mo.mg || !mo.mb || !mo.m2 || !cube || !vv || !tag) goto done;
    memset(mo.wt, 0, sizeof(long long) * ALWAN_WU_CELLS);
    memset(mo.mr, 0, sizeof(long long) * ALWAN_WU_CELLS);
    memset(mo.mg, 0, sizeof(long long) * ALWAN_WU_CELLS);
    memset(mo.mb, 0, sizeof(long long) * ALWAN_WU_CELLS);
    memset(mo.m2, 0, sizeof(double) * ALWAN_WU_CELLS);
    memset(tag, 0, sizeof(unsigned short) * ALWAN_WU_CELLS);

    /* the histogram: cell (top five bits) + 1 on each axis */
    for (i = 0; i < count; i++) {
        unsigned char const *p = rgb + i * stride;
        size_t const ind = alwan_wu_ix((unsigned int)(p[0] >> 3) + 1u, (unsigned int)(p[1] >> 3) + 1u,
                                       (unsigned int)(p[2] >> 3) + 1u);
        mo.wt[ind] += 1;
        mo.mr[ind] += p[0];
        mo.mg[ind] += p[1];
        mo.mb[ind] += p[2];
        mo.m2[ind] += (double)((unsigned int)p[0] * p[0] + (unsigned int)p[1] * p[1] + (unsigned int)p[2] * p[2]);
    }
    /* cumulative moments, the listing's M3d */
    {
        unsigned int r, g, b;
        long long area[ALWAN_WU_N], area_r[ALWAN_WU_N], area_g[ALWAN_WU_N], area_b[ALWAN_WU_N];
        double area2[ALWAN_WU_N];
        for (r = 1; r < ALWAN_WU_N; r++) {
            memset(area, 0, sizeof area); memset(area_r, 0, sizeof area_r);
            memset(area_g, 0, sizeof area_g); memset(area_b, 0, sizeof area_b);
            memset(area2, 0, sizeof area2);
            for (g = 1; g < ALWAN_WU_N; g++) {
                long long line = 0, line_r = 0, line_g = 0, line_b = 0;
                double line2 = 0.0;
                for (b = 1; b < ALWAN_WU_N; b++) {
                    size_t const ind1 = alwan_wu_ix(r, g, b), ind2 = alwan_wu_ix(r - 1, g, b);
                    line += mo.wt[ind1]; line_r += mo.mr[ind1]; line_g += mo.mg[ind1];
                    line_b += mo.mb[ind1]; line2 += mo.m2[ind1];
                    area[b] += line; area_r[b] += line_r; area_g[b] += line_g;
                    area_b[b] += line_b; area2[b] += line2;
                    mo.wt[ind1] = mo.wt[ind2] + area[b];
                    mo.mr[ind1] = mo.mr[ind2] + area_r[b];
                    mo.mg[ind1] = mo.mg[ind2] + area_g[b];
                    mo.mb[ind1] = mo.mb[ind2] + area_b[b];
                    mo.m2[ind1] = mo.m2[ind2] + area2[b];
                }
            }
        }
    }

    cube[0].r0 = cube[0].g0 = cube[0].b0 = 0;
    cube[0].r1 = cube[0].g1 = cube[0].b1 = 32;
    cube[0].vol = 32u * 32u * 32u;
    vv[0] = 0.0;
    for (i = 1; i < K; i++) {
        size_t kk;
        double temp;
        if (alwan_wu_cut(&cube[next], &cube[i], &mo)) {
            vv[next] = cube[next].vol > 1 ? alwan_wu_var(&cube[next], &mo) : 0.0;
            vv[i] = cube[i].vol > 1 ? alwan_wu_var(&cube[i], &mo) : 0.0;
        } else {
            vv[next] = 0.0;             /* do not try this box again */
            i--;                        /* box i was not made */
        }
        next = 0;
        temp = vv[0];
        for (kk = 1; kk <= i; kk++) {
            if (vv[kk] > temp) { temp = vv[kk]; next = kk; }
        }
        if (temp <= 0.0) { K = i + 1; break; }
    }

    {
        size_t kk;
        for (kk = 0; kk < K; kk++) {
            unsigned int r, g, b;
            long long const weight = alwan_wu_vol(&cube[kk], mo.wt);
            for (r = cube[kk].r0 + 1; r <= cube[kk].r1; r++)
                for (g = cube[kk].g0 + 1; g <= cube[kk].g1; g++)
                    for (b = cube[kk].b0 + 1; b <= cube[kk].b1; b++)
                        tag[alwan_wu_ix(r, g, b)] = (unsigned short)kk;
            if (weight) {
                palette_out[3 * kk + 0] = (unsigned char)(alwan_wu_vol(&cube[kk], mo.mr) / weight);
                palette_out[3 * kk + 1] = (unsigned char)(alwan_wu_vol(&cube[kk], mo.mg) / weight);
                palette_out[3 * kk + 2] = (unsigned char)(alwan_wu_vol(&cube[kk], mo.mb) / weight);
            } else {
                palette_out[3 * kk + 0] = palette_out[3 * kk + 1] = palette_out[3 * kk + 2] = 0;
            }
        }
    }
    if (index_out) {
        for (i = 0; i < count; i++) {
            unsigned char const *p = rgb + i * stride;
            index_out[i] = tag[alwan_wu_ix((unsigned int)(p[0] >> 3) + 1u, (unsigned int)(p[1] >> 3) + 1u,
                                           (unsigned int)(p[2] >> 3) + 1u)];
        }
    }
    *count_out = K;
    st = ALWAN_OK;
done:
    if (mo.wt) ALWAN_FREE(mo.wt);
    if (mo.mr) ALWAN_FREE(mo.mr);
    if (mo.mg) ALWAN_FREE(mo.mg);
    if (mo.mb) ALWAN_FREE(mo.mb);
    if (mo.m2) ALWAN_FREE(mo.m2);
    if (cube) ALWAN_FREE(cube);
    if (vv) ALWAN_FREE(vv);
    if (tag) ALWAN_FREE(tag);
    return st;
}

/* ---------------------------------------------------------------- classic octree */

typedef struct {
    int child[8];             /* -1 for none */
    unsigned int level;       /* 0 root ... 8 a colour */
    unsigned int path;        /* the top 3 * level bits of the colour, interleaved */
    int leaf;                 /* 1 when it is a leaf now */
    long long n, sr, sg, sb;  /* pixels and sums below it */
    unsigned int index;       /* palette entry of a leaf */
} alwan_oc_node;

typedef struct {
    long long n;              /* pixels below the node */
    unsigned int path;
    int node;
} alwan_oc_key;

/* fewest pixels first, then the lower position in the tree */
static int alwan_oc_cmp(void const *a, void const *b) {
    alwan_oc_key const *x = (alwan_oc_key const *)a, *y = (alwan_oc_key const *)b;
    if (x->n != y->n) return x->n < y->n ? -1 : 1;
    return (x->path > y->path) - (x->path < y->path);
}

static void alwan_oc_number(alwan_oc_node *nodes, int node, unsigned char *palette, unsigned int *next) {
    alwan_oc_node *nd = nodes + node;
    int c;
    if (nd->leaf) {
        long long const n2 = 2 * nd->n;
        nd->index = *next;
        palette[3 * *next + 0] = (unsigned char)((2 * nd->sr + nd->n) / n2);
        palette[3 * *next + 1] = (unsigned char)((2 * nd->sg + nd->n) / n2);
        palette[3 * *next + 2] = (unsigned char)((2 * nd->sb + nd->n) / n2);
        (*next)++;
        return;
    }
    for (c = 0; c < 8; c++) if (nd->child[c] >= 0) alwan_oc_number(nodes, nd->child[c], palette, next);
}

static alwan_status alwan_qx_octree_classic(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                            unsigned char const *rgb, size_t stride, size_t count,
                                            size_t max_colors) {
    alwan_qx_colours c;
    alwan_oc_node *nodes = NULL;
    alwan_oc_key *list = NULL;
    size_t cap, nn = 1, leaves, j;
    alwan_status st;
    unsigned int lvl, next = 0;

    st = alwan_qx_colours_build(&c, rgb, stride, count);
    if (st != ALWAN_OK) return st;
    cap = 8 * c.n + 1;
    nodes = (alwan_oc_node *)ALWAN_ALLOC(sizeof(alwan_oc_node) * cap, sizeof(long long));
    list = (alwan_oc_key *)ALWAN_ALLOC(sizeof(alwan_oc_key) * cap, sizeof(long long));
    if (!nodes || !list) { st = ALWAN_E_NOMEM; goto done; }
    memset(&nodes[0], 0, sizeof nodes[0]);
    for (j = 0; j < 8; j++) nodes[0].child[j] = -1;
    for (j = 0; j < c.n; j++) {
        unsigned int const r = (unsigned int)c.col[3 * j], g = (unsigned int)c.col[3 * j + 1];
        unsigned int const b = (unsigned int)c.col[3 * j + 2];
        long long const w = (long long)c.w[j];
        int node = 0;
        unsigned int L;
        nodes[0].n += w; nodes[0].sr += w * r; nodes[0].sg += w * g; nodes[0].sb += w * b;
        for (L = 0; L < 8; L++) {
            unsigned int const ci = (((r >> (7 - L)) & 1u) << 2) | (((g >> (7 - L)) & 1u) << 1) | ((b >> (7 - L)) & 1u);
            int ch = nodes[node].child[ci];
            if (ch < 0) {
                size_t q;
                ch = (int)nn++;
                memset(&nodes[ch], 0, sizeof nodes[ch]);
                for (q = 0; q < 8; q++) nodes[ch].child[q] = -1;
                nodes[ch].level = L + 1;
                nodes[ch].path = (nodes[node].path << 3) | ci;
                nodes[node].child[ci] = ch;
            }
            node = ch;
            nodes[node].n += w; nodes[node].sr += w * r; nodes[node].sg += w * g; nodes[node].sb += w * b;
        }
        nodes[node].leaf = 1;
    }
    leaves = c.n;
    for (lvl = 8; lvl-- > 0 && leaves > max_colors;) {
        size_t m = 0, q;
        for (j = 0; j < nn; j++) {
            if (nodes[j].level == lvl && !nodes[j].leaf) {
                list[m].n = nodes[j].n; list[m].path = nodes[j].path; list[m].node = (int)j; m++;
            }
        }
        qsort(list, m, sizeof list[0], alwan_oc_cmp);
        for (q = 0; q < m && leaves > max_colors; q++) {
            alwan_oc_node *nd = nodes + list[q].node;
            size_t kids = 0, z;
            for (z = 0; z < 8; z++) if (nd->child[z] >= 0) { kids++; nd->child[z] = -1; }
            nd->leaf = 1;
            leaves -= kids - 1;
        }
    }
    alwan_oc_number(nodes, 0, palette_out, &next);
    if (index_out) {
        size_t i;
        for (i = 0; i < count; i++) {
            unsigned char const *p = rgb + i * stride;
            int node = 0;
            unsigned int L = 0;
            while (!nodes[node].leaf) {
                unsigned int const ci = (((unsigned int)(p[0] >> (7 - L)) & 1u) << 2)
                                      | (((unsigned int)(p[1] >> (7 - L)) & 1u) << 1)
                                      | ((unsigned int)(p[2] >> (7 - L)) & 1u);
                node = nodes[node].child[ci];
                L++;
            }
            index_out[i] = nodes[node].index;
        }
    }
    *count_out = next;
    st = ALWAN_OK;
done:
    if (nodes) ALWAN_FREE(nodes);
    if (list) ALWAN_FREE(list);
    alwan_qx_colours_free(&c);
    return st;
}

alwan_status alwan__quantize_ext(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                 unsigned char const *rgb, size_t pixel_stride, size_t count, size_t max_colors,
                                 alwan_quantize_method method, alwan_quantize_params const *params) {
    switch (method) {
    case ALWAN_QUANTIZE_KMEANS:
        return alwan_qx_kmeans(palette_out, count_out, index_out, rgb, pixel_stride, count, max_colors, params);
    case ALWAN_QUANTIZE_WU:
        return alwan_qx_wu(palette_out, count_out, index_out, rgb, pixel_stride, count, max_colors);
    case ALWAN_QUANTIZE_OCTREE_CLASSIC:
        return alwan_qx_octree_classic(palette_out, count_out, index_out, rgb, pixel_stride, count, max_colors);
    default:
        return ALWAN_E_INVALID;
    }
}
