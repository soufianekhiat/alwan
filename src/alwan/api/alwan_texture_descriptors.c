/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Texture descriptors with a shape of their own: grey-level co-occurrence matrices and
 * their properties (alwan_glcm_u8, _u16, alwan_glcm_props) and histograms of oriented
 * gradients (alwan_hog_{T}, _u8), as scikit-image 0.26 computes them (suite 294).
 *
 * The co-occurrence loop follows feature/_texture.pyx's _glcm_loop, the properties
 * feature/texture.py's graycoprops, the gradient and block normalisation feature/_hog.py,
 * the cell histograms feature/_hoghistogram.pyx (its float accumulator kept), and the
 * visualisation lines draw/_draw.pyx's _line. numpy's sums are reproduced where they decide
 * the last bit: a contiguous np.sum is numpy's pairwise sum (blocks of eight, halves past
 * 128). scikit-image is Copyright the scikit-image team, BSD-3-Clause
 * (https://github.com/scikit-image/scikit-image); see THIRD_PARTY_NOTICES.md and
 * licenses/scikit-image-BSD-3-Clause.txt, whose conditions apply to this port:
 *
 *   Redistribution and use in source and binary forms, with or without modification, are
 *   permitted provided that the following conditions are met: (1) redistributions of
 *   source code must retain the above copyright notice, this list of conditions and the
 *   following disclaimer; (2) redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution; (3) neither the
 *   name of the copyright holder nor the names of its contributors may be used to endorse
 *   or promote products derived from this software without specific prior written
 *   permission. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 *   NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* numpy's pairwise sum of a contiguous double array */
static double alwan_tdx_pairwise(double const *a, size_t n) {
    if (n < 8) {
        double res = 0.0;
        size_t i;
        for (i = 0; i < n; i++) res += a[i];
        return res;
    }
    if (n <= 128) {
        double r[8], res;
        size_t i, j;
        for (j = 0; j < 8; j++) r[j] = a[j];
        for (i = 8; i < n - (n % 8); i += 8) {
            for (j = 0; j < 8; j++) r[j] += a[i + j];
        }
        res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; i++) res += a[i];
        return res;
    } else {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan_tdx_pairwise(a, n2) + alwan_tdx_pairwise(a + n2, n - n2);
    }
}

/* ---------------------------------------------------------------------------------------
 * Grey-level co-occurrence matrices
 * --------------------------------------------------------------------------------------- */

/* _shared/interpolation.pxd's round: half away from zero, then truncated */
static long alwan_tdx_round(double r) {
    return (long)(r > 0.0 ? r + 0.5 : r - 0.5);
}

static alwan_status alwan_tdx_glcm(double *out, void const *src, size_t src_rs, size_t w, size_t h, double const *distances, size_t nd,
                                   double const *angles, size_t na, alwan_glcm_params const *params, int is16) {
    alwan_glcm_params const zero = { 0 };
    alwan_glcm_params const *p = params ? params : &zero;
    size_t const elem = is16 ? sizeof(unsigned short) : 1u;
    size_t levels, total, x, y, ai, di, i, j;
    if (!out || !src || !distances || !angles || w == 0 || h == 0 || nd == 0 || na == 0) return ALWAN_E_INVALID;
    if (src_rs / elem < w) return ALWAN_E_INVALID;
    if (is16 && p->levels == 0) return ALWAN_E_INVALID;
    levels = p->levels ? p->levels : 256u;
    if (levels > 65536u) return ALWAN_E_RANGE;
    for (di = 0; di < nd; di++) if (!(distances[di] - distances[di] == 0.0)) return ALWAN_E_INVALID;
    for (ai = 0; ai < na; ai++) if (!(angles[ai] - angles[ai] == 0.0)) return ALWAN_E_INVALID;
    total = alwan_safe_array_size(levels * levels, nd * na);
    if (total == 0 || total > ((size_t)1 << 28) || levels * levels / levels != levels || (nd * na) / na != nd) return ALWAN_E_RANGE;
    /* scikit-image refuses an image whose maximum is at or above levels */
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            size_t const v = is16 ? (size_t)((unsigned short const *)row)[x] : (size_t)((unsigned char const *)row)[x];
            if (v >= levels) return ALWAN_E_RANGE;
        }
    }
    memset(out, 0, total * sizeof(double));
    for (ai = 0; ai < na; ai++) {
        double const angle = angles[ai];
        for (di = 0; di < nd; di++) {
            double const distance = distances[di];
            long const off_r = alwan_tdx_round(ALWAN_SIN_F64(angle) * distance);
            long const off_c = alwan_tdx_round(ALWAN_COS_F64(angle) * distance);
            long const rows = (long)h, cols = (long)w;
            long const r0 = off_r < 0 ? -off_r : 0, r1 = rows - off_r < rows ? rows - off_r : rows;
            long const c0 = off_c < 0 ? -off_c : 0, c1 = cols - off_c < cols ? cols - off_c : cols;
            long r, c;
            for (r = r0; r < r1; r++) {
                char const *row = (char const *)src + (size_t)r * src_rs;
                char const *row2 = (char const *)src + (size_t)(r + off_r) * src_rs;
                for (c = c0; c < c1; c++) {
                    size_t const vi = is16 ? (size_t)((unsigned short const *)row)[c] : (size_t)((unsigned char const *)row)[c];
                    size_t const vj = is16 ? (size_t)((unsigned short const *)row2)[c + off_c]
                                           : (size_t)((unsigned char const *)row2)[c + off_c];
                    out[((vi * levels + vj) * nd + di) * na + ai] += 1.0;
                }
            }
        }
    }
    if (p->symmetric) {
        /* P + P^T, each (d, a) plane */
        for (i = 0; i < levels; i++) {
            for (j = i; j < levels; j++) {
                for (di = 0; di < nd * na; di++) {
                    size_t const a = (i * levels + j) * nd * na + di, b = (j * levels + i) * nd * na + di;
                    double const s = out[a] + out[b];
                    out[a] = s;
                    out[b] = s;
                }
            }
        }
    }
    if (p->normed) {
        for (di = 0; di < nd * na; di++) {
            double sum = 0.0;
            for (i = 0; i < levels * levels; i++) sum += out[i * nd * na + di];
            if (sum == 0.0) sum = 1.0;
            for (i = 0; i < levels * levels; i++) out[i * nd * na + di] /= sum;
        }
    }
    return ALWAN_OK;
}

alwan_status alwan_glcm_u8(double *out, unsigned char const *src, size_t src_row_stride, size_t width, size_t height, double const *distances,
                           size_t distance_count, double const *angles, size_t angle_count, alwan_glcm_params const *params) {
    return alwan_tdx_glcm(out, src, src_row_stride, width, height, distances, distance_count, angles, angle_count, params, 0);
}

alwan_status alwan_glcm_u16(double *out, unsigned short const *src, size_t src_row_stride, size_t width, size_t height, double const *distances,
                            size_t distance_count, double const *angles, size_t angle_count, alwan_glcm_params const *params) {
    return alwan_tdx_glcm(out, src, src_row_stride, width, height, distances, distance_count, angles, angle_count, params, 1);
}

/* np.sum(x, axis=(0, 1)) of a (levels, levels, D, A) product: with one (distance, angle) the
 * array is contiguous and numpy sums it pairwise; with more, numpy walks the reduced axes
 * outermost and adds in order */
static double alwan_tdx_sum(double const *v, size_t n, int pairwise) {
    if (pairwise) return alwan_tdx_pairwise(v, n);
    {
        double s = 0.0;
        size_t i;
        for (i = 0; i < n; i++) s += v[i];
        return s;
    }
}

alwan_status alwan_glcm_props(double *out, double const *glcm, size_t levels, size_t distance_count, size_t angle_count, alwan_glcm_prop prop) {
    size_t const nda = distance_count * angle_count;
    size_t k, i, j, n, l2;
    int pw;
    double *P, *t;
    if (!out || !glcm || levels == 0 || distance_count == 0 || angle_count == 0) return ALWAN_E_INVALID;
    if ((unsigned)prop > (unsigned)ALWAN_GLCM_ENTROPY) return ALWAN_E_INVALID;
    if (levels > 65536u || nda / angle_count != distance_count) return ALWAN_E_RANGE;
    n = alwan_safe_array_size(levels * levels, nda);
    if (n == 0 || n > ((size_t)1 << 28)) return ALWAN_E_RANGE;
    for (k = 0; k < n; k++) if (!(glcm[k] >= 0.0) || !(glcm[k] < 1e300)) return ALWAN_E_INVALID;
    l2 = levels * levels;
    pw = nda == 1;
    P = (double *)ALWAN_ALLOC(alwan_safe_array_size(l2, sizeof(double)), sizeof(double));
    t = (double *)ALWAN_ALLOC(alwan_safe_array_size(l2, sizeof(double)), sizeof(double));
    if (!P || !t) {
        ALWAN_FREE(t);
        ALWAN_FREE(P);
        return ALWAN_E_NOMEM;
    }
    for (k = 0; k < nda; k++) {
        double sum, res;
        /* graycoprops divides each matrix by its sum, a zero sum read as 1 */
        for (i = 0; i < l2; i++) P[i] = glcm[i * nda + k];
        sum = alwan_tdx_sum(P, l2, pw);
        if (sum == 0.0) sum = 1.0;
        for (i = 0; i < l2; i++) P[i] /= sum;
        if (prop == ALWAN_GLCM_CORRELATION) {
            double mi, mj, vi, vj, cov;
            for (i = 0; i < levels; i++)
                for (j = 0; j < levels; j++) t[i * levels + j] = (double)i * P[i * levels + j];
            mi = alwan_tdx_sum(t, l2, pw);
            for (i = 0; i < levels; i++)
                for (j = 0; j < levels; j++) t[i * levels + j] = (double)j * P[i * levels + j];
            mj = alwan_tdx_sum(t, l2, pw);
            for (i = 0; i < levels; i++)
                for (j = 0; j < levels; j++) {
                    double const d = (double)i - mi;
                    t[i * levels + j] = P[i * levels + j] * (d * d);
                }
            vi = ALWAN_SQRT_F64(alwan_tdx_sum(t, l2, pw));
            for (i = 0; i < levels; i++)
                for (j = 0; j < levels; j++) {
                    double const d = (double)j - mj;
                    t[i * levels + j] = P[i * levels + j] * (d * d);
                }
            vj = ALWAN_SQRT_F64(alwan_tdx_sum(t, l2, pw));
            for (i = 0; i < levels; i++)
                for (j = 0; j < levels; j++) t[i * levels + j] = P[i * levels + j] * (((double)i - mi) * ((double)j - mj));
            cov = alwan_tdx_sum(t, l2, pw);
            res = (vi < 1e-15 || vj < 1e-15) ? 1.0 : cov / (vi * vj);
        } else if (prop == ALWAN_GLCM_MEAN || prop == ALWAN_GLCM_VARIANCE || prop == ALWAN_GLCM_STD) {
            double mean;
            for (i = 0; i < levels; i++)
                for (j = 0; j < levels; j++) t[i * levels + j] = (double)i * P[i * levels + j];
            mean = alwan_tdx_sum(t, l2, pw);
            if (prop == ALWAN_GLCM_MEAN) {
                res = mean;
            } else {
                for (i = 0; i < levels; i++)
                    for (j = 0; j < levels; j++) {
                        double const d = (double)i - mean;
                        t[i * levels + j] = P[i * levels + j] * (d * d);
                    }
                res = alwan_tdx_sum(t, l2, pw);
                if (prop == ALWAN_GLCM_STD) res = ALWAN_SQRT_F64(res);
            }
        } else {
            for (i = 0; i < levels; i++)
                for (j = 0; j < levels; j++) {
                    double const pv = P[i * levels + j];
                    double const d = (double)i - (double)j;
                    double v;
                    switch (prop) {
                    case ALWAN_GLCM_CONTRAST: v = pv * (d * d); break;
                    case ALWAN_GLCM_DISSIMILARITY: v = pv * ALWAN_ABS_F64(d); break;
                    case ALWAN_GLCM_HOMOGENEITY: v = pv * (1.0 / (1.0 + d * d)); break;
                    case ALWAN_GLCM_ASM:
                    case ALWAN_GLCM_ENERGY: v = pv * pv; break;
                    default: v = pv != 0.0 ? pv * (-ALWAN_LN_F64(pv)) : 0.0; break; /* ENTROPY */
                    }
                    t[i * levels + j] = v;
                }
            res = alwan_tdx_sum(t, l2, pw);
            if (prop == ALWAN_GLCM_ENERGY) res = ALWAN_SQRT_F64(res);
        }
        out[k] = res;
    }
    ALWAN_FREE(t);
    ALWAN_FREE(P);
    return ALWAN_OK;
}

/* ---------------------------------------------------------------------------------------
 * Histograms of oriented gradients
 * --------------------------------------------------------------------------------------- */

/* numpy's remainder: the result takes the divisor's sign, a zero is +0 */
static double alwan_tdx_mod180(double a) {
    double m = ALWAN_FMOD_F64(a, 180.0);
    if (m != 0.0) {
        if (m < 0.0) m += 180.0;
    } else {
        m = 0.0;
    }
    return m;
}

/* hypot as numpy's np.hypot gives it. The det build's hypot is m sqrt(1 + (s/m)^2), not
 * correctly rounded, so there it is sqrt(x^2 + y^2): exact for the integer gradients of
 * 8-bit images, where channels tie (3, 4 and 4, 3 are both 5) and the first must win */
static double alwan_tdx_hypot(double x, double y) {
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
    return ALWAN_SQRT_F64(x * x + y * y);
#else
    return ALWAN_HYPOT_F64(x, y);
#endif
}

/* atan2 with its exact cases spelled out: on an axis or a diagonal (gradients of integer
 * images land there often, and on a bin edge) the correctly rounded value any libm returns,
 * so the det build's polynomial atan2 bins those pixels as the C runtime does */
static double alwan_tdx_atan2(double y, double x) {
    double const ay = ALWAN_ABS_F64(y), ax = ALWAN_ABS_F64(x);
    if (y == 0.0 && x == 0.0) return 0.0 * y;                      /* +0 or -0 */
    if (x == 0.0) return y > 0.0 ? 1.5707963267948966 : -1.5707963267948966;
    if (y == 0.0) {
        if (x > 0.0) return y;                                     /* +0 or -0 */
        return (1.0 / y > 0.0) ? 3.141592653589793 : -3.141592653589793;
    }
    if (ay == ax) {
        double const r = x > 0.0 ? 0.7853981633974483 : 2.356194490192345;
        return y > 0.0 ? r : -r;
    }
    return ALWAN_ATAN2_F64(y, x);
}

/* draw/_draw.pyx's _line, adding v to each pixel of the line (out float or double) */
static void alwan_tdx_line(double *img, size_t rs, size_t w, size_t h, long r0, long c0, long r1, long c1, double v, int as_float) {
    long r = r0, c = c0, dr = r1 - r0 < 0 ? r0 - r1 : r1 - r0, dc = c1 - c0 < 0 ? c0 - c1 : c1 - c0;
    long sr, sc, d, i, t;
    int steep = 0;
    sc = (c1 - c) > 0 ? 1 : -1;
    sr = (r1 - r) > 0 ? 1 : -1;
    if (dr > dc) {
        steep = 1;
        t = c; c = r; r = t;
        t = dc; dc = dr; dr = t;
        t = sc; sc = sr; sr = t;
    }
    d = (2 * dr) - dc;
    for (i = 0; i <= dc; i++) {
        long rr, cc;
        if (i == dc) {
            rr = r1;
            cc = c1;
        } else if (steep) {
            rr = c;
            cc = r;
        } else {
            rr = r;
            cc = c;
        }
        if (rr >= 0 && cc >= 0 && rr < (long)h && cc < (long)w) {
            double *px = (double *)((char *)img + (size_t)rr * rs) + cc;
            *px = as_float ? (double)(float)(*px + v) : *px + v;
        }
        if (i == dc) break;
        while (d >= 0) {
            r = r + sr;
            d = d - (2 * dc);
        }
        c = c + sc;
        d = d + (2 * dr);
    }
}

/* kind: 0 f64, 1 f32, 2 u8 */
static alwan_status alwan_tdx_hog(void *out, size_t *count_out, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                  alwan_hog_params const *params, int kind) {
    alwan_hog_params const zero = { 0 };
    alwan_hog_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const nor = p->orientations ? p->orientations : 9u;
    size_t const c_row = p->cell_height ? p->cell_height : 8u, c_col = p->cell_width ? p->cell_width : 8u;
    size_t const b_row = p->block_height ? p->block_height : 3u, b_col = p->block_width ? p->block_width : 3u;
    int const as_float = kind == 1;
    size_t n = w * h, n_cells_row, n_cells_col, n_blocks_row, n_blocks_col, count, x, y, c, i, blen;
    double *img = NULL, *grow = NULL, *gcol = NULL, *mag = NULL, *ori = NULL, *hist = NULL, *blk = NULL, *tmp = NULL;
    alwan_status st = ALWAN_OK;
    if (!src || !count_out || w == 0 || h == 0 || ch == 0 || ch > 4 || n / w != h) return ALWAN_E_INVALID;
    if (src_rs / elem < w * ch) return ALWAN_E_INVALID;
    if ((unsigned)p->block_norm > (unsigned)ALWAN_HOG_BLOCK_NORM_L2) return ALWAN_E_INVALID;
    if (nor > 4096u || c_row > 65536u || c_col > 65536u || b_row > 65536u || b_col > 65536u) return ALWAN_E_RANGE;
    n_cells_row = h / c_row;
    n_cells_col = w / c_col;
    if (n_cells_row < b_row || n_cells_col < b_col) return ALWAN_E_RANGE;
    n_blocks_row = n_cells_row - b_row + 1;
    n_blocks_col = n_cells_col - b_col + 1;
    blen = b_row * b_col * nor;
    count = alwan_safe_array_size(n_blocks_row * n_blocks_col, blen);
    if (count == 0) return ALWAN_E_RANGE;
    *count_out = count;
    if (!out && !p->visualization) return ALWAN_OK;
    if (p->visualization && p->visualization_row_stride / sizeof(double) < w) return ALWAN_E_INVALID;
    img = (double *)ALWAN_ALLOC(alwan_safe_array_size(n * ch, sizeof(double)), sizeof(double));
    grow = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    gcol = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    mag = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    ori = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    hist = (double *)ALWAN_ALLOC(alwan_safe_array_size(n_cells_row * n_cells_col, nor * sizeof(double)), sizeof(double));
    blk = (double *)ALWAN_ALLOC(alwan_safe_array_size(blen, sizeof(double)), sizeof(double));
    tmp = (double *)ALWAN_ALLOC(alwan_safe_array_size(blen, sizeof(double)), sizeof(double));
    if (!img || !grow || !gcol || !mag || !ori || !hist || !blk || !tmp) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    /* the image as its float type (float for f32, double otherwise), sqrt if asked */
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w * ch; x++) {
            double v = kind == 0 ? ((alwan_f64 const *)row)[x] : kind == 1 ? (double)((alwan_f32 const *)row)[x]
                                                                          : (double)((unsigned char const *)row)[x];
            if (!(v - v == 0.0) || (p->transform_sqrt && v < 0.0)) {
                st = ALWAN_E_INVALID;
                goto done;
            }
            if (p->transform_sqrt) v = as_float ? (double)(float)ALWAN_SQRT_F64(v) : ALWAN_SQRT_F64(v);
            img[y * w * ch + x] = v;
        }
    }
    /* _hog_channel_gradient per channel, in the image's float type; the channel with the
     * largest hypot(g_row, g_col) wins, the first on a tie */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double best_m = 0.0, br = 0.0, bc = 0.0;
            for (c = 0; c < ch; c++) {
                double gr = 0.0, gc = 0.0, m;
                if (as_float) {
                    /* numpy subtracts float32 from float32 in float */
                    if (y > 0 && y + 1 < h) {
                        float const a = (float)img[((y + 1) * w + x) * ch + c], b = (float)img[((y - 1) * w + x) * ch + c];
                        gr = (double)(float)(a - b);
                    }
                    if (x > 0 && x + 1 < w) {
                        float const a = (float)img[(y * w + x + 1) * ch + c], b = (float)img[(y * w + x - 1) * ch + c];
                        gc = (double)(float)(a - b);
                    }
                } else {
                    if (y > 0 && y + 1 < h) gr = img[((y + 1) * w + x) * ch + c] - img[((y - 1) * w + x) * ch + c];
                    if (x > 0 && x + 1 < w) gc = img[(y * w + x + 1) * ch + c] - img[(y * w + x - 1) * ch + c];
                }
                if (ch == 1) {
                    br = gr;
                    bc = gc;
                    break;
                }
                m = alwan_tdx_hypot(gr, gc);
                if (as_float) m = (double)(float)m;
                if (c == 0 || m > best_m) {
                    best_m = m;
                    br = gr;
                    bc = gc;
                }
            }
            grow[y * w + x] = br;
            gcol[y * w + x] = bc;
            /* hog_histograms: np.hypot and rad2deg(arctan2) % 180, in double */
            mag[y * w + x] = alwan_tdx_hypot(bc, br);
            ori[y * w + x] = alwan_tdx_mod180(alwan_tdx_atan2(br, bc) * (180.0 / 3.14159265358979323846));
        }
    }
    /* cell_hog: the magnitudes of a cell's pixels in the bin [end, start), summed in float
     * in raster order, divided by the cell's pixel count in float */
    {
        double const per180 = 180.0 / (double)nor;
        size_t o, ri, ci;
        for (o = 0; o < nor; o++) {
            double const ostart = per180 * (double)(o + 1), oend = per180 * (double)o;
            for (ri = 0; ri < n_cells_row; ri++) {
                for (ci = 0; ci < n_cells_col; ci++) {
                    float total = 0.0f;
                    size_t yy, xx;
                    for (yy = ri * c_row; yy < (ri + 1) * c_row; yy++) {
                        for (xx = ci * c_col; xx < (ci + 1) * c_col; xx++) {
                            double const ov = ori[yy * w + xx];
                            if (ov >= ostart || ov < oend) continue;
                            total = (float)((double)total + mag[yy * w + xx]);
                        }
                    }
                    hist[(ri * n_cells_col + ci) * nor + o] = (double)(total / (float)(int)(c_row * c_col));
                }
            }
        }
    }
    if (p->visualization) {
        long const radius = (long)((c_row < c_col ? c_row : c_col) / 2) - 1;
        double const pi = 3.14159265358979323846;
        size_t ri, ci, o;
        for (y = 0; y < h; y++) {
            double *vr = (double *)((char *)p->visualization + y * p->visualization_row_stride);
            for (x = 0; x < w; x++) vr[x] = 0.0;
        }
        for (ri = 0; ri < n_cells_row; ri++) {
            for (ci = 0; ci < n_cells_col; ci++) {
                for (o = 0; o < nor; o++) {
                    double const mid = pi * ((double)o + 0.5) / (double)nor;
                    /* the middle bin of an odd count is pi/2, whose cosine the C runtime gives as
                     * 6.1e-17, not 0: it moves a truncated endpoint, so it is spelled out */
                    int const half_pi = mid == 1.5707963267948966;
                    double const sn = half_pi ? 1.0 : ALWAN_SIN_F64(mid);
                    double const cs = half_pi ? 6.123233995736766e-17 : ALWAN_COS_F64(mid);
                    double const dr = (double)radius * sn, dc = (double)radius * cs;
                    double const cr = (double)(ri * c_row + c_row / 2), cc = (double)(ci * c_col + c_col / 2);
                    alwan_tdx_line(p->visualization, p->visualization_row_stride, w, h, (long)(cr - dc), (long)(cc + dr), (long)(cr + dc),
                                   (long)(cc - dr), hist[(ri * n_cells_col + ci) * nor + o], as_float);
                }
            }
        }
    }
    if (out) {
        double const eps = 1e-5;
        size_t r, cb, br_, bc_, o, k = 0;
        for (r = 0; r < n_blocks_row; r++) {
            for (cb = 0; cb < n_blocks_col; cb++) {
                size_t q = 0;
                for (br_ = 0; br_ < b_row; br_++)
                    for (bc_ = 0; bc_ < b_col; bc_++)
                        for (o = 0; o < nor; o++) blk[q++] = hist[((r + br_) * n_cells_col + (cb + bc_)) * nor + o];
                switch (p->block_norm) {
                case ALWAN_HOG_BLOCK_NORM_L1:
                case ALWAN_HOG_BLOCK_NORM_L1_SQRT: {
                    double s;
                    for (i = 0; i < blen; i++) tmp[i] = ALWAN_ABS_F64(blk[i]);
                    s = alwan_tdx_pairwise(tmp, blen) + eps;
                    for (i = 0; i < blen; i++) {
                        tmp[i] = blk[i] / s;
                        if (p->block_norm == ALWAN_HOG_BLOCK_NORM_L1_SQRT) tmp[i] = ALWAN_SQRT_F64(tmp[i]);
                    }
                    break;
                }
                default: { /* L2, L2_HYS */
                    double s;
                    for (i = 0; i < blen; i++) tmp[i] = blk[i] * blk[i];
                    s = ALWAN_SQRT_F64(alwan_tdx_pairwise(tmp, blen) + eps * eps);
                    for (i = 0; i < blen; i++) blk[i] = blk[i] / s;
                    if (p->block_norm == ALWAN_HOG_BLOCK_NORM_L2_HYS) {
                        for (i = 0; i < blen; i++) {
                            if (blk[i] > 0.2) blk[i] = 0.2;
                            tmp[i] = blk[i] * blk[i];
                        }
                        s = ALWAN_SQRT_F64(alwan_tdx_pairwise(tmp, blen) + eps * eps);
                        for (i = 0; i < blen; i++) blk[i] = blk[i] / s;
                    }
                    for (i = 0; i < blen; i++) tmp[i] = blk[i];
                    break;
                }
                }
                for (i = 0; i < blen; i++, k++) {
                    if (kind == 1) ((alwan_f32 *)out)[k] = (alwan_f32)tmp[i];
                    else ((alwan_f64 *)out)[k] = tmp[i];
                }
            }
        }
    }
done:
    ALWAN_FREE(tmp);
    ALWAN_FREE(blk);
    ALWAN_FREE(hist);
    ALWAN_FREE(ori);
    ALWAN_FREE(mag);
    ALWAN_FREE(gcol);
    ALWAN_FREE(grow);
    ALWAN_FREE(img);
    return st;
}

alwan_status alwan_hog_u8(alwan_f64 *out, size_t *count_out, unsigned char const *src, size_t src_row_stride, size_t channels, size_t width,
                          size_t height, alwan_hog_params const *params) {
    return alwan_tdx_hog(out, count_out, src, src_row_stride, channels, width, height, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_hog_f64(alwan_f64 *out, size_t *count_out, alwan_f64 const *src, size_t src_row_stride, size_t channels, size_t width,
                           size_t height, alwan_hog_params const *params) {
    return alwan_tdx_hog(out, count_out, src, src_row_stride, channels, width, height, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_hog_f32(alwan_f32 *out, size_t *count_out, alwan_f32 const *src, size_t src_row_stride, size_t channels, size_t width,
                           size_t height, alwan_hog_params const *params) {
    return alwan_tdx_hog(out, count_out, src, src_row_stride, channels, width, height, params, 1);
}
#endif
