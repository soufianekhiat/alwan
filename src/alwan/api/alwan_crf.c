/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Camera response recovery: Debevec and Malik 1997, sampled by Grossberg and Nayar's
 * 2003 histogram method, after colour-hdri (colour_hdri.calibration.debevec1997,
 * colour_hdri.sampling.grossberg2003), which the tests hold this to.
 *
 * Per channel, the response g (log exposure as a function of the pixel value z) is
 * the least-squares solution of
 *
 *     w(z_ij) (g(z_ij) - ln E_i) = w(z_ij) B_j        every sample i, exposure j
 *     lambda w(z) (g(z - 1) - 2 g(z) + g(z + 1)) = 0  smoothness, 0 < z < n - 1
 *     g(n / 2) = 0                                    the offset
 *
 * with B_j = ln(1 / L_j), L_j the ISO 2720 average luminance of exposure j.
 * colour-hdri gives the whole system to numpy's lstsq, 1250 unknowns for 1000
 * samples. Here each sample's ln E_i is eliminated first, by a Householder
 * reflection within that sample's rows, which leaves a system in g alone and the
 * same least-squares solution. A g column that only one smoothness row mentions is
 * set by that row. The responses are then exp(g), extrapolated by a polynomial
 * where the weight is zero, and scaled so each channel peaks at 1.
 *
 * Robertson, Borman and Stevenson 2003 is the other recovery here, after OpenCV's
 * CalibrateRobertson: it alternates between merging the bracket with the current
 * response and re-estimating the response from that merge, over every pixel.
 *
 * Everything computes in f64; the f32 entry points read f32 images.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_table_core.h"
#include <float.h>
#include <string.h>

#define ALWAN__CRF_MAX_BINS 65536

static double alwan__crf_px(void const *img, int f32, size_t stride, size_t p, int c) {
    char const *row = (char const *)img + p * stride;
    return f32 ? (double)((float const *)row)[c] : ((double const *)row)[c];
}

/* numpy.histogram over (0, 1): bin k holds [k/n, (k+1)/n) and the last one also 1.
 * Values outside [0, 1], and NaN, are not counted. */
static int alwan__crf_bin(double v, size_t bins) {
    alwan_table_cell_f64 cell;
    if (!(v >= 0.0 && v <= 1.0)) {
        return -1;
    }
    cell = alwan_table_cell_f64_v(v, (int)bins + 1);
    return cell.i0 < (int)bins ? cell.i0 : (int)bins - 1;
}

/* numpy.linspace(0, 1, n)[i]: i * (1 / (n - 1)), the last one exactly 1. */
static double alwan__crf_linspace(size_t i, size_t n) {
    if (n < 2) return 0.0;
    if (i == n - 1) return 1.0;
    return (double)i * (1.0 / (double)(n - 1));
}

static alwan_status alwan__grossberg(size_t *out, void const *const *images, int f32, size_t stride, size_t count,
                                     size_t image_count, size_t samples, size_t bins) {
    size_t *hist = (size_t *)ALWAN_ALLOC(bins * sizeof(size_t), sizeof(size_t));
    double *cdf = (double *)ALWAN_ALLOC(bins * sizeof(double), sizeof(double));
    alwan_status st = ALWAN_OK;
    size_t k, p, b, i;
    int c;
    if (!hist || !cdf) {
        if (hist) ALWAN_FREE(hist);
        if (cdf) ALWAN_FREE(cdf);
        return ALWAN_E_NOMEM;
    }
    for (k = 0; k < image_count && st == ALWAN_OK; k++) {
        for (c = 0; c < 3 && st == ALWAN_OK; c++) {
            size_t cum = 0;
            double total;
            memset(hist, 0, bins * sizeof(size_t));
            for (p = 0; p < count; p++) {
                int const bin = alwan__crf_bin(alwan__crf_px(images[k], f32, stride, p, c), bins);
                if (bin >= 0) hist[bin]++;
            }
            for (b = 0; b < bins; b++) {
                cum += hist[b];
                cdf[b] = (double)cum;
            }
            if (cum == 0) {
                st = ALWAN_E_RANGE; /* no value of this channel lies in [0, 1] */
                break;
            }
            total = cdf[bins - 1];
            for (b = 0; b < bins; b++) cdf[b] /= total;
            for (i = 0; i < samples; i++) {
                double const u = alwan__crf_linspace(i, samples);
                double best_d = ALWAN_ABS(cdf[0] - u);
                size_t best = 0;
                for (b = 1; b < bins; b++) {
                    double const d = ALWAN_ABS(cdf[b] - u);
                    if (d < best_d) { best_d = d; best = b; }
                }
                out[(i * image_count + k) * 3 + (size_t)c] = best;
            }
        }
    }
    ALWAN_FREE(hist);
    ALWAN_FREE(cdf);
    return st;
}

/* Least squares by Householder QR: A is m x n row-major, m >= n, destroyed; b too. */
static int alwan__crf_lsq(double *A, size_t m, size_t n, double *b, double *x) {
    size_t k, i, j;
    for (k = 0; k < n; k++) {
        double norm2 = 0.0, norm, alpha, vk, vtv;
        for (i = k; i < m; i++) norm2 += A[i * n + k] * A[i * n + k];
        norm = ALWAN_SQRT(norm2);
        if (!(norm > 0.0)) return 0;
        alpha = A[k * n + k] > 0.0 ? -norm : norm;
        vk = A[k * n + k] - alpha;
        vtv = vk * vk + (norm2 - A[k * n + k] * A[k * n + k]);
        if (!(vtv > 0.0)) return 0;
        for (j = k + 1; j < n; j++) {
            double s = vk * A[k * n + j];
            double f;
            for (i = k + 1; i < m; i++) s += A[i * n + k] * A[i * n + j];
            f = 2.0 * s / vtv;
            A[k * n + j] -= f * vk;
            for (i = k + 1; i < m; i++) A[i * n + j] -= f * A[i * n + k];
        }
        {
            double s = vk * b[k];
            double f;
            for (i = k + 1; i < m; i++) s += A[i * n + k] * b[i];
            f = 2.0 * s / vtv;
            b[k] -= f * vk;
            for (i = k + 1; i < m; i++) b[i] -= f * A[i * n + k];
        }
        A[k * n + k] = alpha;
    }
    for (k = n; k-- > 0;) {
        double s = b[k];
        for (j = k + 1; j < n; j++) s -= A[k * n + j] * x[j];
        if (!(ALWAN_ABS(A[k * n + k]) > 1e-300)) return 0;
        x[k] = s / A[k * n + k];
    }
    return 1;
}

typedef struct {
    int g[3];      /* g columns, -1 when unused */
    double a[3];
    int le;        /* the sample whose ln E this row holds, -1 for none */
    double le_a;
    double rhs;
    int alive;
} alwan__crf_row;

/* Solve one channel: z holds samples x images bin indices, stride 3 between them. */
static alwan_status alwan__g_solve(double *g, size_t const *z, size_t samples, size_t image_count,
                                   double const *B, double const *w_n, size_t n, double l_s) {
    size_t const max_rows = samples * image_count + n + 1;
    alwan__crf_row *rows = (alwan__crf_row *)ALWAN_ALLOC(max_rows * sizeof(alwan__crf_row), sizeof(double));
    int *cnt_g = (int *)ALWAN_ALLOC(n * sizeof(int), sizeof(int));
    int *cnt_le = (int *)ALWAN_ALLOC((samples ? samples : 1) * sizeof(int), sizeof(int));
    int *col = (int *)ALWAN_ALLOC(n * sizeof(int), sizeof(int));        /* g column -> kept index */
    int *dropped_by = (int *)ALWAN_ALLOC(n * sizeof(int), sizeof(int)); /* row that set a dropped g */
    int *order = (int *)ALWAN_ALLOC(n * sizeof(int), sizeof(int));      /* drop order */
    double *M = NULL, *rhs = NULL, *x = NULL, *blk = NULL, *blk_rhs = NULL, *u = NULL, *s = NULL;
    size_t nrows = 0, r, i, j, K = 0, R = 0, max_block = image_count;
    int n_order = 0, progress;
    alwan_status st = ALWAN_OK;

    if (!rows || !cnt_g || !cnt_le || !col || !dropped_by || !order) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    memset(cnt_g, 0, n * sizeof(int));
    memset(cnt_le, 0, (samples ? samples : 1) * sizeof(int));

    /* The rows colour-hdri builds, zero rows left out. */
    for (i = 0; i < samples; i++) {
        for (j = 0; j < image_count; j++) {
            size_t const zc = z[(i * image_count + j) * 3];
            double const w = w_n[zc];
            alwan__crf_row *row;
            if (w == 0.0) continue;
            row = &rows[nrows++];
            row->g[0] = (int)zc; row->g[1] = -1; row->g[2] = -1;
            row->a[0] = w;
            row->le = (int)i;
            row->le_a = -w;
            row->rhs = w * B[j];
            row->alive = 1;
        }
    }
    {
        alwan__crf_row *row = &rows[nrows++];
        row->g[0] = (int)(n / 2); row->g[1] = -1; row->g[2] = -1;
        row->a[0] = 1.0;
        row->le = -1; row->le_a = 0.0; row->rhs = 0.0; row->alive = 1;
    }
    for (i = 1; i + 1 < n; i++) {
        double const c = l_s * w_n[i];
        alwan__crf_row *row;
        if (c == 0.0) continue;
        row = &rows[nrows++];
        row->g[0] = (int)i - 1; row->g[1] = (int)i; row->g[2] = (int)i + 1;
        row->a[0] = c; row->a[1] = c * -2.0; row->a[2] = c;
        row->le = -1; row->le_a = 0.0; row->rhs = 0.0; row->alive = 1;
    }
    for (r = 0; r < nrows; r++) {
        for (j = 0; j < 3; j++) if (rows[r].g[j] >= 0) cnt_g[rows[r].g[j]]++;
        if (rows[r].le >= 0) cnt_le[rows[r].le]++;
    }

    /* Prune what one row alone sets: a sample seen once (its ln E absorbs the row),
     * and a g column only one smoothness or offset row mentions. */
    for (i = 0; i < n; i++) dropped_by[i] = -1;
    do {
        progress = 0;
        for (r = 0; r < nrows; r++) {
            alwan__crf_row *row = &rows[r];
            if (!row->alive) continue;
            if (row->le >= 0 && cnt_le[row->le] == 1) {
                row->alive = 0;
                cnt_le[row->le] = 0;
                cnt_g[row->g[0]]--;
                progress = 1;
                continue;
            }
            if (row->le < 0) {
                for (j = 0; j < 3; j++) {
                    int const gc = row->g[j];
                    if (gc >= 0 && cnt_g[gc] == 1 && dropped_by[gc] < 0) {
                        size_t t;
                        row->alive = 0;
                        dropped_by[gc] = (int)r;
                        order[n_order++] = gc;
                        for (t = 0; t < 3; t++) if (row->g[t] >= 0) cnt_g[row->g[t]]--;
                        progress = 1;
                        break;
                    }
                }
            }
        }
    } while (progress);

    for (i = 0; i < n; i++) col[i] = (cnt_g[i] > 0 && dropped_by[i] < 0) ? (int)(K++) : -1;
    if (K == 0) {
        st = ALWAN_E_RANGE;
        goto done;
    }

    /* Rows of the reduced system: every alive smoothness and offset row, and for each
     * sample with m alive rows, m - 1 rows once its ln E is reflected out. */
    for (r = 0; r < nrows; r++) if (rows[r].alive) R++;
    M = (double *)ALWAN_ALLOC(R * K * sizeof(double), sizeof(double));
    rhs = (double *)ALWAN_ALLOC(R * sizeof(double), sizeof(double));
    x = (double *)ALWAN_ALLOC(K * sizeof(double), sizeof(double));
    blk = (double *)ALWAN_ALLOC(max_block * K * sizeof(double), sizeof(double));
    blk_rhs = (double *)ALWAN_ALLOC(max_block * sizeof(double), sizeof(double));
    u = (double *)ALWAN_ALLOC(max_block * sizeof(double), sizeof(double));
    s = (double *)ALWAN_ALLOC(K * sizeof(double), sizeof(double));
    if (!M || !rhs || !x || !blk || !blk_rhs || !u || !s) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    memset(M, 0, R * K * sizeof(double));
    R = 0;
    r = 0;
    while (r < nrows) {
        alwan__crf_row const *row = &rows[r];
        if (!row->alive) { r++; continue; }
        if (row->le < 0) {
            for (j = 0; j < 3; j++) if (row->g[j] >= 0) M[R * K + (size_t)col[row->g[j]]] += row->a[j];
            rhs[R++] = row->rhs;
            r++;
        } else {
            /* This sample's alive rows are contiguous. */
            int const le = row->le;
            size_t m = 0, t, q;
            double norm2 = 0.0, alpha, vtv, srhs;
            for (t = r; t < nrows && rows[t].le == le; t++) {
                if (!rows[t].alive) continue;
                memset(blk + m * K, 0, K * sizeof(double));
                blk[m * K + (size_t)col[rows[t].g[0]]] = rows[t].a[0];
                blk_rhs[m] = rows[t].rhs;
                u[m] = rows[t].le_a;
                norm2 += u[m] * u[m];
                m++;
            }
            r = t;
            if (m < 2) continue;
            alpha = u[0] > 0.0 ? -ALWAN_SQRT(norm2) : ALWAN_SQRT(norm2);
            vtv = (u[0] - alpha) * (u[0] - alpha) + (norm2 - u[0] * u[0]);
            u[0] -= alpha;
            memset(s, 0, K * sizeof(double));
            srhs = 0.0;
            for (t = 0; t < m; t++) {
                for (q = 0; q < K; q++) s[q] += u[t] * blk[t * K + q];
                srhs += u[t] * blk_rhs[t];
            }
            for (t = 1; t < m; t++) {
                double const f = 2.0 * u[t] / vtv;
                for (q = 0; q < K; q++) M[R * K + q] = blk[t * K + q] - f * s[q];
                rhs[R++] = blk_rhs[t] - f * srhs;
            }
        }
    }
    if (R < K || !alwan__crf_lsq(M, R, K, rhs, x)) {
        st = ALWAN_E_RANGE;
        goto done;
    }

    for (i = 0; i < n; i++) g[i] = col[i] >= 0 ? x[col[i]] : 0.0;
    /* Columns a single row set, in reverse order of setting. */
    while (n_order > 0) {
        int const gc = order[--n_order];
        alwan__crf_row const *row = &rows[dropped_by[gc]];
        double acc = row->rhs, a_self = 0.0;
        for (j = 0; j < 3; j++) {
            if (row->g[j] < 0) continue;
            if (row->g[j] == gc) a_self = row->a[j];
            else acc -= row->a[j] * g[row->g[j]];
        }
        g[gc] = a_self != 0.0 ? acc / a_self : 0.0;
    }

done:
    if (rows) ALWAN_FREE(rows);
    if (cnt_g) ALWAN_FREE(cnt_g);
    if (cnt_le) ALWAN_FREE(cnt_le);
    if (col) ALWAN_FREE(col);
    if (dropped_by) ALWAN_FREE(dropped_by);
    if (order) ALWAN_FREE(order);
    if (M) ALWAN_FREE(M);
    if (rhs) ALWAN_FREE(rhs);
    if (x) ALWAN_FREE(x);
    if (blk) ALWAN_FREE(blk);
    if (blk_rhs) ALWAN_FREE(blk_rhs);
    if (u) ALWAN_FREE(u);
    if (s) ALWAN_FREE(s);
    return st;
}

/* numpy.polyfit (scaled Vandermonde, least squares) through the points whose weight
 * is not zero, evaluated by Horner at the others. */
static alwan_status alwan__crf_extrapolate(double *crf, double const *w_n, size_t n, size_t d1) {
    size_t m = 0, i, k, r;
    double *A, *b, *c, *scale;
    alwan_status st = ALWAN_OK;
    for (i = 0; i < n; i++) if (w_n[i] != 0.0) m++;
    if (m == n) return ALWAN_OK;
    if (m < d1) return ALWAN_E_RANGE;
    A = (double *)ALWAN_ALLOC(m * d1 * sizeof(double), sizeof(double));
    b = (double *)ALWAN_ALLOC(m * sizeof(double), sizeof(double));
    c = (double *)ALWAN_ALLOC(d1 * sizeof(double), sizeof(double));
    scale = (double *)ALWAN_ALLOC(d1 * sizeof(double), sizeof(double));
    if (!A || !b || !c || !scale) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    for (i = 0, r = 0; i < n; i++) {
        double const xv = alwan__crf_linspace(i, n);
        double pw = 1.0;
        if (w_n[i] == 0.0) continue;
        /* numpy.vander: decreasing powers, built by repeated multiplication. */
        A[r * d1 + (d1 - 1)] = 1.0;
        for (k = 1; k < d1; k++) {
            pw *= xv;
            A[r * d1 + (d1 - 1 - k)] = pw;
        }
        b[r] = crf[i];
        r++;
    }
    for (k = 0; k < d1; k++) {
        double sum = 0.0;
        for (r = 0; r < m; r++) sum += A[r * d1 + k] * A[r * d1 + k];
        scale[k] = ALWAN_SQRT(sum);
        for (r = 0; r < m; r++) A[r * d1 + k] /= scale[k];
    }
    if (!alwan__crf_lsq(A, m, d1, b, c)) {
        st = ALWAN_E_RANGE;
        goto done;
    }
    for (k = 0; k < d1; k++) c[k] /= scale[k];
    for (i = 0; i < n; i++) {
        double y = 0.0;
        double const xv = alwan__crf_linspace(i, n);
        if (w_n[i] != 0.0) continue;
        for (k = 0; k < d1; k++) y = y * xv + c[k];
        crf[i] = y;
    }
done:
    if (A) ALWAN_FREE(A);
    if (b) ALWAN_FREE(b);
    if (c) ALWAN_FREE(c);
    if (scale) ALWAN_FREE(scale);
    return st;
}

static alwan_status alwan__crf_params(size_t *samples, size_t *bins, double *l_s, alwan_merge_weight *wf, int *degree,
                                      int *keep, alwan_crf_debevec1997_params const *p) {
    alwan_crf_debevec1997_params const zero = { 0, 0, 0.0, ALWAN_MERGE_WEIGHT_DEBEVEC1997, 0, 0 };
    if (!p) p = &zero;
    *samples = p->samples ? p->samples : 1000;
    *bins = p->bins ? p->bins : 256;
    *l_s = p->smoothing == 0.0 ? 30.0 : p->smoothing;
    *wf = p->weight;
    *degree = p->extrapolation_degree == 0 ? 7 : p->extrapolation_degree;
    *keep = p->keep_scale;
    if (*bins < 3 || *bins > ALWAN__CRF_MAX_BINS || !(*l_s > 0.0)
        || (int)*wf < 0 || (int)*wf > (int)ALWAN_MERGE_WEIGHT_DOUBLE_SIGMOID) {
        return ALWAN_E_INVALID;
    }
    return ALWAN_OK;
}

static alwan_status alwan__debevec(double *out, void const *const *images, int f32, size_t stride, size_t count,
                                   double const *B, size_t image_count, alwan_crf_debevec1997_params const *params) {
    size_t samples, bins, i;
    double l_s;
    alwan_merge_weight wf;
    int degree, keep, c;
    size_t *z = NULL;
    double *w_n = NULL, *g = NULL;
    alwan_status st = alwan__crf_params(&samples, &bins, &l_s, &wf, &degree, &keep, params);
    if (st != ALWAN_OK) return st;
    z = (size_t *)ALWAN_ALLOC(samples * image_count * 3 * sizeof(size_t), sizeof(size_t));
    w_n = (double *)ALWAN_ALLOC(bins * sizeof(double), sizeof(double));
    g = (double *)ALWAN_ALLOC(bins * sizeof(double), sizeof(double));
    if (!z || !w_n || !g) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    st = alwan__grossberg(z, images, f32, stride, count, image_count, samples, bins);
    if (st != ALWAN_OK) goto done;
    for (i = 0; i < bins; i++) {
        (void)alwan_hdr_merge_weight_f64(&w_n[i], alwan__crf_linspace(i, bins), wf);
    }
    for (c = 0; c < 3 && st == ALWAN_OK; c++) {
        double peak = 0.0;
        st = alwan__g_solve(g, z + c, samples, image_count, B, w_n, bins, l_s);
        if (st != ALWAN_OK) break;
        for (i = 0; i < bins; i++) out[(size_t)c * bins + i] = ALWAN_EXP(g[i]);
        if (degree > 0) {
            st = alwan__crf_extrapolate(out + (size_t)c * bins, w_n, bins, (size_t)degree + 1);
            if (st != ALWAN_OK) break;
        }
        if (!keep) {
            for (i = 0; i < bins; i++) if (out[(size_t)c * bins + i] > peak) peak = out[(size_t)c * bins + i];
            if (!(peak > 0.0)) { st = ALWAN_E_RANGE; break; }
            for (i = 0; i < bins; i++) out[(size_t)c * bins + i] /= peak;
        }
    }
done:
    if (z) ALWAN_FREE(z);
    if (w_n) ALWAN_FREE(w_n);
    if (g) ALWAN_FREE(g);
    return st;
}

static alwan_status alwan__crf_log_exposures(double *B, double const *N, double const *t, double const *S,
                                             size_t image_count) {
    size_t j;
    for (j = 0; j < image_count; j++) {
        alwan_f64 L;
        if (alwan_average_luminance_f64(&L, N[j], t[j], S[j], 0.0) != ALWAN_OK) return ALWAN_E_INVALID;
        B[j] = ALWAN_LN(1.0 / L);
    }
    return ALWAN_OK;
}

/* ================================================================
 * Robertson, Borman and Stevenson 2003
 *
 * Start from a linear response I(z) = z / (n / 2). Then repeat:
 *
 *     E_p   = sum_j t_j w(z_pj) I(z_pj) / (sum_j t_j^2 w(z_pj) + DBL_EPSILON)
 *     I(z)  = the mean of t_j E_p over every pixel p and exposure j with z_pj = z
 *     I    /= I(n / 2)
 *
 * until the summed absolute change, averaged over the channels, is below the
 * threshold. w is OpenCV's weight, a Gaussian over the bins scaled and shifted to 1
 * at the centre and 0 at both ends. The exposures t_j only matter by their ratios.
 * OpenCV leaves a bin that no pixel falls in at NaN, and the NaN then also keeps its
 * stopping test from ever passing. Here such bins are left out of the stopping test
 * and filled afterwards, linearly between their seen neighbours and held beyond the
 * first and last. Every pixel takes part, so no image buffer is kept: the merge and
 * the new response are accumulated in one pass.
 * ================================================================ */

static void alwan__robertson_weights(double *w, size_t bins) {
    double const q = (double)(bins - 1) / 4.0;
    double const e4 = ALWAN_EXP(4.0);
    double const scale = e4 / (e4 - 1.0), shift = 1.0 / (1.0 - e4);
    size_t i;
    for (i = 0; i < bins; i++) {
        double const v = (double)i / q - 2.0;
        w[i] = scale * ALWAN_EXP(-v * v) + shift;
    }
    /* Exactly 0, which the formula is; in f64 rounding leaves -3.5e-18 there, and in
     * OpenCV's f32 it is 0. */
    w[0] = 0.0;
    w[bins - 1] = 0.0;
}

static alwan_status alwan__robertson(double *out, void const *const *images, int f32, size_t stride, size_t count,
                                     double const *e, size_t image_count,
                                     alwan_crf_robertson2003_params const *params) {
    alwan_crf_robertson2003_params const zero = { 0, 0, 0.0 };
    alwan_crf_robertson2003_params const *pr = params ? params : &zero;
    size_t const bins = pr->bins ? pr->bins : 256;
    size_t const iterations = pr->iterations ? pr->iterations : 30;
    double const threshold = pr->threshold == 0.0 ? 0.01 : pr->threshold;
    size_t const mid = bins / 2;
    double *w, *card, *resp, *next;
    int *zk;
    size_t it, p, k, b;
    int c;
    alwan_status st = ALWAN_OK;

    if (bins < 3 || bins > ALWAN__CRF_MAX_BINS || !(threshold >= 0.0) || threshold - threshold != 0.0) {
        return ALWAN_E_INVALID;
    }
    w = (double *)ALWAN_ALLOC(10 * bins * sizeof(double), sizeof(double));
    zk = (int *)ALWAN_ALLOC(image_count * sizeof(int), sizeof(int));
    if (!w || !zk) {
        if (w) ALWAN_FREE(w);
        if (zk) ALWAN_FREE(zk);
        return ALWAN_E_NOMEM;
    }
    card = w + bins;
    resp = card + 3 * bins;
    next = resp + 3 * bins;
    alwan__robertson_weights(w, bins);
    memset(card, 0, 3 * bins * sizeof(double));
    for (k = 0; k < image_count; k++) {
        for (p = 0; p < count; p++) {
            for (c = 0; c < 3; c++) {
                int const bin = alwan__crf_bin(alwan__crf_px(images[k], f32, stride, p, c), bins);
                if (bin >= 0) card[(size_t)c * bins + (size_t)bin] += 1.0;
            }
        }
    }
    for (c = 0; c < 3; c++) {
        if (card[(size_t)c * bins + mid] == 0.0) {
            st = ALWAN_E_RANGE; /* the response is pinned at the middle value, which no pixel holds */
            goto done;
        }
        for (b = 0; b < bins; b++) resp[(size_t)c * bins + b] = (double)b / ((double)bins / 2.0);
    }

    for (it = 0; it < iterations; it++) {
        double diff = 0.0;
        memset(next, 0, 3 * bins * sizeof(double));
        for (p = 0; p < count; p++) {
            for (c = 0; c < 3; c++) {
                double const *rc = resp + (size_t)c * bins;
                double *nc = next + (size_t)c * bins;
                double num = 0.0, den = 0.0, E;
                for (k = 0; k < image_count; k++) {
                    double wz;
                    zk[k] = alwan__crf_bin(alwan__crf_px(images[k], f32, stride, p, c), bins);
                    if (zk[k] < 0) continue;
                    wz = w[zk[k]];
                    num += e[k] * (wz * rc[zk[k]]);
                    den += e[k] * e[k] * wz;
                }
                E = num * (1.0 / (den + DBL_EPSILON));
                for (k = 0; k < image_count; k++) {
                    if (zk[k] >= 0) nc[zk[k]] += e[k] * E;
                }
            }
        }
        for (c = 0; c < 3; c++) {
            double *nc = next + (size_t)c * bins;
            double const *cc = card + (size_t)c * bins;
            double middle;
            for (b = 0; b < bins; b++) nc[b] = cc[b] > 0.0 ? nc[b] / cc[b] : 0.0;
            middle = nc[mid];
            if (!(middle > 0.0)) {
                st = ALWAN_E_RANGE;
                goto done;
            }
            for (b = 0; b < bins; b++) {
                nc[b] /= middle;
                if (cc[b] > 0.0) diff += ALWAN_ABS(nc[b] - resp[(size_t)c * bins + b]);
            }
        }
        memcpy(resp, next, 3 * bins * sizeof(double));
        if (diff / 3.0 < threshold) break;
    }

    /* Bins no pixel fell in: linear between seen neighbours, held at the ends. */
    for (c = 0; c < 3; c++) {
        double *rc = resp + (size_t)c * bins;
        double const *cc = card + (size_t)c * bins;
        size_t prev = bins;
        for (b = 0; b < bins; b++) {
            if (cc[b] == 0.0) continue;
            if (prev == bins) {
                size_t q;
                for (q = 0; q < b; q++) rc[q] = rc[b];
            } else if (b - prev > 1) {
                size_t q;
                for (q = prev + 1; q < b; q++) {
                    double const f = (double)(q - prev) / (double)(b - prev);
                    rc[q] = rc[prev] + (rc[b] - rc[prev]) * f;
                }
            }
            prev = b;
        }
        for (b = prev + 1; b < bins; b++) rc[b] = rc[prev];
    }
    memcpy(out, resp, 3 * bins * sizeof(double));
done:
    ALWAN_FREE(w);
    ALWAN_FREE(zk);
    return st;
}

/* The exposure of each image, 1 / L with L its ISO 2720 average luminance. */
static alwan_status alwan__crf_exposures(double *e, double const *N, double const *t, double const *S,
                                         size_t image_count) {
    size_t j;
    for (j = 0; j < image_count; j++) {
        alwan_f64 L;
        if (alwan_average_luminance_f64(&L, N[j], t[j], S[j], 0.0) != ALWAN_OK || !(L > 0.0)) return ALWAN_E_INVALID;
        e[j] = 1.0 / L;
    }
    return ALWAN_OK;
}

/* ================================================================
 * Public entry points
 * ================================================================ */

#define ALWAN__CRF_CHECK(images, image_count)                                          \
    do {                                                                               \
        size_t q_;                                                                     \
        if (!(images) || (image_count) == 0) return ALWAN_E_INVALID;                   \
        for (q_ = 0; q_ < (image_count); q_++) if (!(images)[q_]) return ALWAN_E_INVALID; \
    } while (0)

#if ALWAN_WITH_F64
alwan_status alwan_crf_samples_grossberg2003_f64(size_t *bins_out, alwan_f64 const *const *images, size_t in_stride,
                                                 size_t count, size_t image_count, size_t samples, size_t bins) {
    ALWAN__CRF_CHECK(images, image_count);
    if (!bins_out || bins < 2 || bins > ALWAN__CRF_MAX_BINS) return ALWAN_E_INVALID;
    return alwan__grossberg(bins_out, (void const *const *)images, 0, in_stride, count, image_count, samples, bins);
}

alwan_status alwan_crf_debevec1997_f64(alwan_f64 *response_out, alwan_f64 const *const *images, size_t in_stride,
                                       size_t count, alwan_exposure_settings_f64 const *settings, size_t image_count,
                                       alwan_crf_debevec1997_params const *params) {
    double *B, *N, *t, *S;
    alwan_status st;
    size_t j;
    ALWAN__CRF_CHECK(images, image_count);
    if (!response_out || !settings) return ALWAN_E_INVALID;
    B = (double *)ALWAN_ALLOC(4 * image_count * sizeof(double), sizeof(double));
    if (!B) return ALWAN_E_NOMEM;
    N = B + image_count; t = N + image_count; S = t + image_count;
    for (j = 0; j < image_count; j++) {
        N[j] = settings[j].f_number; t[j] = settings[j].exposure_time; S[j] = settings[j].iso;
    }
    st = alwan__crf_log_exposures(B, N, t, S, image_count);
    if (st == ALWAN_OK) {
        st = alwan__debevec(response_out, (void const *const *)images, 0, in_stride, count, B, image_count, params);
    }
    ALWAN_FREE(B);
    return st;
}

alwan_status alwan_crf_robertson2003_f64(alwan_f64 *response_out, alwan_f64 const *const *images, size_t in_stride,
                                         size_t count, alwan_exposure_settings_f64 const *settings, size_t image_count,
                                         alwan_crf_robertson2003_params const *params) {
    double *e, *N, *t, *S;
    alwan_status st;
    size_t j;
    ALWAN__CRF_CHECK(images, image_count);
    if (!response_out || !settings) return ALWAN_E_INVALID;
    e = (double *)ALWAN_ALLOC(4 * image_count * sizeof(double), sizeof(double));
    if (!e) return ALWAN_E_NOMEM;
    N = e + image_count; t = N + image_count; S = t + image_count;
    for (j = 0; j < image_count; j++) {
        N[j] = settings[j].f_number; t[j] = settings[j].exposure_time; S[j] = settings[j].iso;
    }
    st = alwan__crf_exposures(e, N, t, S, image_count);
    if (st == ALWAN_OK) {
        st = alwan__robertson(response_out, (void const *const *)images, 0, in_stride, count, e, image_count, params);
    }
    ALWAN_FREE(e);
    return st;
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
alwan_status alwan_crf_samples_grossberg2003_f32(size_t *bins_out, alwan_f32 const *const *images, size_t in_stride,
                                                 size_t count, size_t image_count, size_t samples, size_t bins) {
    ALWAN__CRF_CHECK(images, image_count);
    if (!bins_out || bins < 2 || bins > ALWAN__CRF_MAX_BINS) return ALWAN_E_INVALID;
    return alwan__grossberg(bins_out, (void const *const *)images, 1, in_stride, count, image_count, samples, bins);
}

alwan_status alwan_crf_debevec1997_f32(alwan_f32 *response_out, alwan_f32 const *const *images, size_t in_stride,
                                       size_t count, alwan_exposure_settings_f32 const *settings, size_t image_count,
                                       alwan_crf_debevec1997_params const *params) {
    size_t samples, bins, j;
    double l_s;
    alwan_merge_weight wf;
    int degree, keep;
    double *B, *N, *t, *S, *wide;
    alwan_status st;
    ALWAN__CRF_CHECK(images, image_count);
    if (!response_out || !settings) return ALWAN_E_INVALID;
    st = alwan__crf_params(&samples, &bins, &l_s, &wf, &degree, &keep, params);
    if (st != ALWAN_OK) return st;
    B = (double *)ALWAN_ALLOC((4 * image_count + 3 * bins) * sizeof(double), sizeof(double));
    if (!B) return ALWAN_E_NOMEM;
    N = B + image_count; t = N + image_count; S = t + image_count; wide = S + image_count;
    for (j = 0; j < image_count; j++) {
        N[j] = (double)settings[j].f_number; t[j] = (double)settings[j].exposure_time; S[j] = (double)settings[j].iso;
    }
    st = alwan__crf_log_exposures(B, N, t, S, image_count);
    if (st == ALWAN_OK) {
        st = alwan__debevec(wide, (void const *const *)images, 1, in_stride, count, B, image_count, params);
    }
    if (st == ALWAN_OK) {
        for (j = 0; j < 3 * bins; j++) response_out[j] = (alwan_f32)wide[j];
    }
    ALWAN_FREE(B);
    return st;
}

alwan_status alwan_crf_robertson2003_f32(alwan_f32 *response_out, alwan_f32 const *const *images, size_t in_stride,
                                         size_t count, alwan_exposure_settings_f32 const *settings, size_t image_count,
                                         alwan_crf_robertson2003_params const *params) {
    size_t const bins = params && params->bins ? params->bins : 256;
    double *e, *N, *t, *S, *wide;
    alwan_status st;
    size_t j;
    ALWAN__CRF_CHECK(images, image_count);
    if (!response_out || !settings) return ALWAN_E_INVALID;
    if (bins < 3 || bins > ALWAN__CRF_MAX_BINS) return ALWAN_E_INVALID;
    e = (double *)ALWAN_ALLOC((4 * image_count + 3 * bins) * sizeof(double), sizeof(double));
    if (!e) return ALWAN_E_NOMEM;
    N = e + image_count; t = N + image_count; S = t + image_count; wide = S + image_count;
    for (j = 0; j < image_count; j++) {
        N[j] = (double)settings[j].f_number; t[j] = (double)settings[j].exposure_time; S[j] = (double)settings[j].iso;
    }
    st = alwan__crf_exposures(e, N, t, S, image_count);
    if (st == ALWAN_OK) {
        st = alwan__robertson(wide, (void const *const *)images, 1, in_stride, count, e, image_count, params);
    }
    if (st == ALWAN_OK) {
        for (j = 0; j < 3 * bins; j++) response_out[j] = (alwan_f32)wide[j];
    }
    ALWAN_FREE(e);
    return st;
}
#endif /* ALWAN_WITH_F32 */
