/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * White balance: a port of opencv_contrib 5.0.0's xphoto white balance
 * (modules/xphoto/src/simple_color_balance.cpp, grayworld_white_balance.cpp,
 * learning_based_color_balance.cpp and learning_based_color_balance_model.hpp).
 * opencv_contrib is Copyright the OpenCV authors, Apache License 2.0
 * (https://github.com/opencv/opencv_contrib); its notice is kept here and the licence
 * travels with the model this file reads, data/opencv/LICENSE-opencv_contrib.txt.
 * LearningBasedWB is after D. Cheng, B. Price, S. Cohen and M. S. Brown, "Effective
 * learning-based illuminant estimation using simple features", CVPR 2015; the trees are
 * OpenCV's.
 *
 * The OpenCV calls these make are reproduced from OpenCV 5.0.0 (Apache-2.0,
 * https://github.com/opencv/opencv): calcHist's uniform binning (imgproc/src/histogram.cpp:
 * a lookup table for 8-bit, floor(v * t + b) in double otherwise, the upper bound
 * exclusive), the MatExpr that SimpleWB's last line builds (core/src/matrix_expressions.cpp,
 * collapsed into one convertTo with alpha and beta formed in double) and convertTo itself
 * (core/src/convert_scale.simd.hpp: alpha and beta cast to float, a fused multiply-add on
 * each whole AVX2 block of 16 values, the in-place remainder unfused, the result rounded to
 * nearest even and saturated). xphoto's vector loops are reproduced where their order shows:
 * LearningBasedWB's brightest pixel is kept per SIMD lane and the lanes compared at the end,
 * so which of several equally bright pixels wins follows the lanes, not the raster. Its
 * palette is a heap built by std::make_heap, pop_heap and push_heap, whose array order
 * decides the order the palette's densities are summed in; the heap here is the MSVC
 * standard library's, which the Windows cv2 is built with.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* opencv_contrib 5.0.0's LearningBasedWB model, verbatim (the literals are OpenCV's). */
static unsigned char const wb_feature_idx[20 * 4 * 2 * 15] = {
#include "../data/opencv/lbwb_feature_idx.csv"
};
static float const wb_thresh_vals[20 * 4 * 2 * 15] = {
#include "../data/opencv/lbwb_thresh_vals.csv"
};
static float const wb_leaf_vals[20 * 4 * 2 * 16] = {
#include "../data/opencv/lbwb_leaf_vals.csv"
};

#define WB_NUM_FEATURES 4
#define WB_NUM_TREES 20
#define WB_NUM_TREE_NODES 16
#define WB_TREE_DEPTH 4          /* cvRound(log(16.f) / log(2.f)) */
#define WB_PALETTE_SIZE 300
#define WB_EPS 0.00001f
#define WB_VECSZ 16              /* convertTo's AVX2 block: two 8-lane float vectors */

/* cvRound: to nearest, ties to even; out of the int range (and NaN) INT_MIN, as cvtsd2si. */
static int wb_round(double v) {
    double r;
    if (!(v > -2147483648.5 && v < 2147483647.5)) return INT_MIN;
    r = ALWAN_FLOOR_F64(v + 0.5);
    if (r - v == 0.5 && ALWAN_FMOD_F64(r, 2.0) != 0.0) r -= 1.0;
    return (int)r;
}

/* cvFloor, guarded: out of the int range INT_MIN. */
static int wb_floor(double v) {
    double const r = ALWAN_FLOOR_F64(v);
    if (!(r >= -2147483648.0 && r < 2147483648.0)) return INT_MIN;
    return (int)r;
}

static unsigned char wb_sat_u8(int v) {
    return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

/* ------------------------------------------------------------------------------------ */
/* SimpleWB                                                                             */
/* ------------------------------------------------------------------------------------ */

static float wb_read_f(void const *row, alwan_pixel_format fmt, size_t i) {
    return fmt == ALWAN_PIXEL_U8 ? (float)((unsigned char const *)row)[i] : ((float const *)row)[i];
}

static void wb_simple_channel(void *out, size_t out_rs, void const *src, size_t src_rs, alwan_pixel_format fmt,
                              size_t ch, size_t c, size_t w, size_t h,
                              float input_min, float input_max, float output_min, float output_max, float p) {
    int const n_elements = fmt == ALWAN_PIXEL_U8 ? 256 : 4096;
    int hist[4096];
    float min_value0 = input_min, max_value0 = input_max, interval, min_value, max_value;
    double t, b, lo, hi, alpha, beta;
    float fa, fb, s_lo, s_hi;
    int total = (int)(w * h), p1 = 0, p2 = n_elements - 1, n1 = 0, n2;
    size_t x, y, k, nfma;

    if (fmt == ALWAN_PIXEL_F32) {
        float step = (input_max - input_min) / (float)(n_elements - 1);
        max_value0 += step > 1.0f ? 1.0f : step;
        if (input_max == input_min) max_value0 += 1.0f;
    } else {
        max_value0 += 1.0f;
    }
    interval = (max_value0 - min_value0) / (float)n_elements;
    min_value = min_value0;
    max_value = max_value0;

    /* calcHist, uniform ranges {min_value, max_value}, upper bound exclusive */
    memset(hist, 0, sizeof hist);
    lo = (double)min_value;
    hi = (double)max_value;
    t = (double)n_elements / (hi - lo);
    b = -t * lo;
    if (fmt == ALWAN_PIXEL_U8) {
        int tab[256];
        int j;
        for (j = 0; j < 256; j++) {
            int idx = wb_floor((double)j * t + b);
            if ((double)j >= lo && (double)j < hi) {
                tab[j] = idx < 0 ? 0 : (idx > n_elements - 1 ? n_elements - 1 : idx);
            } else {
                tab[j] = -1;
            }
        }
        for (y = 0; y < h; y++) {
            unsigned char const *row = (unsigned char const *)src + y * src_rs;
            for (x = 0; x < w; x++) {
                int const bin = tab[row[x * ch + c]];
                if (bin >= 0) hist[bin]++;
            }
        }
    } else {
        for (y = 0; y < h; y++) {
            float const *row = (float const *)((unsigned char const *)src + y * src_rs);
            for (x = 0; x < w; x++) {
                double const v0 = (double)row[x * ch + c];
                int idx = wb_floor(v0 * t + b);
                if (v0 < lo || v0 >= hi) continue;
                idx = idx < 0 ? 0 : (idx > n_elements - 1 ? n_elements - 1 : idx);
                hist[idx]++;
            }
        }
    }

    /* the quantile search, in xphoto's float arithmetic; bounded where xphoto is not (a p
     * beyond the pixels in range would walk off the histogram) */
    n2 = total;
    s_lo = p * (float)total / 100.0f;
    s_hi = (100.0f - p) * (float)total / 100.0f;
    while (p1 < n_elements && (float)n1 + (float)hist[p1] < s_lo) {
        n1 += wb_round((double)(float)hist[p1++]);
        min_value += interval;
    }
    while (p2 >= 0 && (float)n2 - (float)hist[p2] > s_hi) {
        n2 -= wb_round((double)(float)hist[p2--]);
        max_value -= interval;
    }

    /* (output_max - output_min) * (src - min_value) / (max_value - min_value) + output_min,
     * as the MatExpr folds it: alpha and beta in double, then convertTo in float */
    {
        double const d_out = (double)(output_max - output_min);
        double const inv = 1. / (double)(max_value - min_value);
        alpha = (1.0 * d_out) * inv;
        beta = ((-(double)min_value) * d_out) * inv + (double)output_min;
    }
    fa = (float)alpha;
    fb = (float)beta;

    /* convertTo sees the continuous channel as one row of total values, in place */
    nfma = ((size_t)total / WB_VECSZ) * WB_VECSZ;
    k = 0;
    for (y = 0; y < h; y++) {
        void const *srow = (unsigned char const *)src + y * src_rs;
        unsigned char *orow = (unsigned char *)out + y * out_rs;
        for (x = 0; x < w; x++, k++) {
            float const v = wb_read_f(srow, fmt, x * ch + c);
            float r;
            if (k < nfma) {
                r = ALWAN_FMA_CR_F32(v, fa, fb);
            } else {
                float const m = v * fa;
                r = m + fb;
            }
            if (fmt == ALWAN_PIXEL_U8) {
                int const iv = wb_round((double)r);
                orow[x * ch + c] = wb_sat_u8(iv == INT_MIN ? 0 : iv);
            } else {
                ((float *)orow)[x * ch + c] = r;
            }
        }
    }
}

/* ------------------------------------------------------------------------------------ */
/* Pixel access for the 3-channel methods: alwan is R, G, B; xphoto reads B, G, R.       */
/* ------------------------------------------------------------------------------------ */

typedef struct {
    void const *src;
    size_t src_rs, ch, w;
    int u16;
} wb_img;

static void wb_px(wb_img const *im, size_t k, unsigned *r, unsigned *g, unsigned *b) {
    size_t const y = k / im->w, x = k % im->w;
    if (im->u16) {
        unsigned short const *p = (unsigned short const *)((unsigned char const *)im->src + y * im->src_rs) + x * im->ch;
        *r = p[0]; *g = p[1]; *b = p[2];
    } else {
        unsigned char const *p = (unsigned char const *)im->src + y * im->src_rs + x * im->ch;
        *r = p[0]; *g = p[1]; *b = p[2];
    }
}

static unsigned wb_max3(unsigned a, unsigned b, unsigned c) {
    unsigned m = a > b ? a : b;
    return m > c ? m : c;
}

static unsigned wb_min3(unsigned a, unsigned b, unsigned c) {
    unsigned m = a < b ? a : b;
    return m < c ? m : c;
}

/* applyChannelGains: gains scaled by their largest, then fixed point. gains_rgb are
 * normalised in place. out NULL applies nothing. */
static void wb_apply_gains(void *out, size_t out_rs, wb_img const *im, size_t h, float gains_rgb[3]) {
    /* xphoto: max(gainB, max(gainG, gainR)) */
    float const gr_ = gains_rgb[0] < gains_rgb[1] ? gains_rgb[1] : gains_rgb[0];
    float const gmax = gains_rgb[2] < gr_ ? gr_ : gains_rgb[2];
    int ig[3];
    size_t x, y, c;
    if (gmax > 0) {
        gains_rgb[0] /= gmax;
        gains_rgb[1] /= gmax;
        gains_rgb[2] /= gmax;
    }
    if (!out) return;
    for (c = 0; c < 3; c++) {
        ig[c] = wb_round((double)(gains_rgb[c] * (float)(im->u16 ? (1 << 16) : (1 << 8))));
    }
    for (y = 0; y < h; y++) {
        if (im->u16) {
            unsigned short const *s = (unsigned short const *)((unsigned char const *)im->src + y * im->src_rs);
            unsigned short *d = (unsigned short *)((unsigned char *)out + y * out_rs);
            for (x = 0; x < im->w; x++) {
                for (c = 0; c < 3; c++) {
                    unsigned const v = (unsigned)s[x * im->ch + c] * (unsigned)ig[c];
                    d[x * im->ch + c] = (unsigned short)(v >> 16);
                }
                if (im->ch == 4) d[x * 4 + 3] = s[x * 4 + 3];
            }
        } else {
            unsigned char const *s = (unsigned char const *)im->src + y * im->src_rs;
            unsigned char *d = (unsigned char *)out + y * out_rs;
            for (x = 0; x < im->w; x++) {
                for (c = 0; c < 3; c++) {
                    unsigned const v = ((unsigned)s[x * im->ch + c] * (unsigned)ig[c]) & 0xFFFFu;
                    d[x * im->ch + c] = (unsigned char)(v >> 8);
                }
                if (im->ch == 4) d[x * 4 + 3] = s[x * 4 + 3];
            }
        }
    }
}

/* ------------------------------------------------------------------------------------ */
/* GrayworldWB                                                                          */
/* ------------------------------------------------------------------------------------ */

static void wb_grayworld(float gains_rgb[3], wb_img const *im, size_t n, float thresh) {
    unsigned long long sr = 0, sg = 0, sb = 0;
    double dr, dg, db, max_sum;
    size_t k;
    unsigned const top = im->u16 ? 65535u : 255u;
    unsigned const t = (unsigned)wb_round((double)(thresh * (float)top));
    for (k = 0; k < n; k++) {
        unsigned r, g, b, mx, mn;
        wb_px(im, k, &r, &g, &b);
        mx = wb_max3(r, g, b);
        mn = wb_min3(r, g, b);
        if ((mx - mn) * top > t * mx) continue;
        sr += r; sg += g; sb += b;
    }
    if (!im->u16) {
        /* xphoto keeps 8-bit sums in 32 bits */
        sr &= 0xFFFFFFFFull; sg &= 0xFFFFFFFFull; sb &= 0xFFFFFFFFull;
    }
    dr = (double)sr; dg = (double)sg; db = (double)sb;
    max_sum = db > (dr > dg ? dr : dg) ? db : (dr > dg ? dr : dg);
    gains_rgb[0] = dr < 0.1 ? 0.f : (float)(max_sum / dr);
    gains_rgb[1] = dg < 0.1 ? 0.f : (float)(max_sum / dg);
    gains_rgb[2] = db < 0.1 ? 0.f : (float)(max_sum / db);
}

/* ------------------------------------------------------------------------------------ */
/* LearningBasedWB                                                                      */
/* ------------------------------------------------------------------------------------ */

typedef struct {
    float hist_val, r, g;
} wb_elem;

static void wb_chroma(float out[2], float r, float g, float b) {
    out[0] = r / (r + g + b + WB_EPS);
    out[1] = g / (r + g + b + WB_EPS);
}

/* the heap's order: a precedes b when a is the more frequent (xphoto's operator<) */
static int wb_less(wb_elem const *a, wb_elem const *b) {
    return a->hist_val > b->hist_val;
}

/* MSVC STL _Push_heap_by_index */
static void wb_push_by_index(wb_elem *f, long hole, long top, wb_elem val) {
    long idx;
    for (idx = (hole - 1) >> 1; top < hole && wb_less(&f[idx], &val); idx = (hole - 1) >> 1) {
        f[hole] = f[idx];
        hole = idx;
    }
    f[hole] = val;
}

/* MSVC STL _Pop_heap_hole_by_index */
static void wb_pop_hole_by_index(wb_elem *f, long hole, long bottom, wb_elem val) {
    long const top = hole;
    long idx = hole;
    long const max_non_leaf = (bottom - 1) >> 1;
    while (idx < max_non_leaf) {
        idx = 2 * idx + 2;
        if (wb_less(&f[idx], &f[idx - 1])) --idx;
        f[hole] = f[idx];
        hole = idx;
    }
    if (idx == max_non_leaf && bottom % 2 == 0) {
        f[hole] = f[bottom - 1];
        hole = bottom - 1;
    }
    wb_push_by_index(f, hole, top, val);
}

static void wb_make_heap(wb_elem *f, long n) {
    long hole;
    for (hole = n >> 1; hole > 0;) {
        wb_elem val;
        --hole;
        val = f[hole];
        wb_pop_hole_by_index(f, hole, n, val);
    }
}

static void wb_pop_heap(wb_elem *f, long n) {
    if (n >= 2) {
        wb_elem const val = f[n - 1];
        f[n - 1] = f[0];
        wb_pop_hole_by_index(f, 0, n - 1, val);
    }
}

static void wb_push_heap(wb_elem *f, long n) {
    if (n >= 2) wb_push_by_index(f, n - 1, 0, f[n - 1]);
}

static float wb_tree(float const feat[2], unsigned char const *fidx, float const *thr, float const *leaf) {
    int node = 0, d;
    for (d = 0; d < WB_TREE_DEPTH; d++) {
        node = feat[fidx[node]] <= thr[node] ? 2 * node + 1 : 2 * node + 2;
    }
    return leaf[node - WB_NUM_TREE_NODES + 1];
}

static int wb_cmp_float(void const *a, void const *b) {
    float const x = *(float const *)a, y = *(float const *)b;
    return (x > y) - (x < y);
}

/* std::nth_element(v, v + size / 2): the value at size / 2 of the sorted order */
static float wb_median(float *v, int n) {
    qsort(v, (size_t)n, sizeof *v, wb_cmp_float);
    return v[n / 2];
}

static alwan_status wb_learning(float gains_rgb[3], wb_img const *im, size_t n, float sat_thresh, int range_max,
                                int bins) {
    int const thresh = (int)(sat_thresh * (float)range_max);
    int const lanes = im->u16 ? 4 : 8, block = 2 * lanes;
    unsigned lane_sum[8] = {0}, lane_r[8] = {0}, lane_g[8] = {0}, lane_b[8] = {0};
    unsigned long long sr = 0, sg = 0, sb = 0;
    unsigned max_sum = 0, br = 0, bgc = 0, bb = 0, src_max = 0;
    size_t k, nblocks = n / (size_t)block;
    float feat[WB_NUM_FEATURES][2];
    int *hist;
    size_t nbins;
    double t, hi, max_hist_val = 0;
    int dom_b = 0, dom_g = 0, dom_r = 0, i, j, q;
    wb_elem palette[WB_PALETTE_SIZE];
    long psize = 0;
    float consensus_r[WB_NUM_TREES * WB_NUM_FEATURES], consensus_g[WB_NUM_TREES * WB_NUM_FEATURES];
    float all_r[WB_NUM_TREES * WB_NUM_FEATURES], all_g[WB_NUM_TREES * WB_NUM_FEATURES];
    int ncons = 0, nall = 0;
    float ill_r, ill_g, denom;

    /* preprocessing: the global maximum over every pixel; the mask is local max < thresh */
    for (k = 0; k < n; k++) {
        unsigned r, g, b, mx;
        wb_px(im, k, &r, &g, &b);
        mx = wb_max3(r, g, b);
        if (mx > src_max) src_max = mx;
    }

    /* the average and the brightest: vector blocks, the brightest kept per lane */
    for (k = 0; k < nblocks; k++) {
        int half, l;
        for (half = 0; half < 2; half++) {
            for (l = 0; l < lanes; l++) {
                unsigned r, g, b, sum;
                wb_px(im, k * (size_t)block + (size_t)(half * lanes + l), &r, &g, &b);
                if (!((int)wb_max3(r, g, b) < thresh)) r = g = b = 0;
                sum = r + g + b;
                sr += r; sg += g; sb += b;
                if (sum > lane_sum[l]) {
                    lane_sum[l] = sum;
                    lane_r[l] = r; lane_g[l] = g; lane_b[l] = b;
                }
            }
        }
    }
    for (i = 0; i < lanes; i++) {
        if (lane_sum[i] > max_sum) {
            max_sum = lane_sum[i];
            br = lane_r[i]; bgc = lane_g[i]; bb = lane_b[i];
        }
    }
    for (k = nblocks * (size_t)block; k < n; k++) {
        unsigned r, g, b, sum;
        wb_px(im, k, &r, &g, &b);
        if (!((int)wb_max3(r, g, b) < thresh)) continue;
        sum = r + g + b;
        sr += r; sg += g; sb += b;
        if (sum > max_sum) {
            max_sum = sum;
            br = r; bgc = g; bb = b;
        }
    }
    if (!im->u16) {
        sr &= 0xFFFFFFFFull; sg &= 0xFFFFFFFFull; sb &= 0xFFFFFFFFull;
    }
    {
        unsigned long long const m = sr > sg ? (sr > sb ? sr : sb) : (sg > sb ? sg : sb);
        double const max_rgb = (double)m;
        if (!(max_rgb > 0.0)) return ALWAN_E_RANGE;
        wb_chroma(feat[0], (float)((double)sr / max_rgb), (float)((double)sg / max_rgb),
                  (float)((double)sb / max_rgb));
        wb_chroma(feat[1], (float)br, (float)bgc, (float)bb);
    }

    /* the histogram, bins^3, B outermost, over the masked pixels from 0 up to max(bins, src_max),
     * that bound excluded */
    nbins = (size_t)bins * (size_t)bins * (size_t)bins;
    hist = (int *)calloc(nbins, sizeof *hist);
    if (!hist) return ALWAN_E_NOMEM;
    hi = (double)(float)((unsigned)bins > src_max ? (unsigned)bins : src_max);
    t = (double)bins / (hi - 0.0);
    for (k = 0; k < n; k++) {
        unsigned r, g, b;
        int ir, ig, ib;
        wb_px(im, k, &r, &g, &b);
        if (!((int)wb_max3(r, g, b) < thresh)) continue;
        if ((double)b >= hi || (double)g >= hi || (double)r >= hi) continue;
        ib = wb_floor((double)b * t + (-t * 0.0));
        ig = wb_floor((double)g * t + (-t * 0.0));
        ir = wb_floor((double)r * t + (-t * 0.0));
        ib = ib < 0 ? 0 : (ib > bins - 1 ? bins - 1 : ib);
        ig = ig < 0 ? 0 : (ig > bins - 1 ? bins - 1 : ig);
        ir = ir < 0 ? 0 : (ir > bins - 1 ? bins - 1 : ir);
        hist[((size_t)ib * (size_t)bins + (size_t)ig) * (size_t)bins + (size_t)ir]++;
    }

    /* the dominant bin, then the palette of the commonest bins */
    for (i = 0; i < bins; i++) {
        for (j = 0; j < bins; j++) {
            for (q = 0; q < bins; q++) {
                int const cnt = hist[((size_t)i * (size_t)bins + (size_t)j) * (size_t)bins + (size_t)q];
                if ((double)(float)cnt > max_hist_val) {
                    max_hist_val = (double)(float)cnt;
                    dom_b = i; dom_g = j; dom_r = q;
                }
            }
        }
    }
    wb_chroma(feat[2], (float)dom_r, (float)dom_g, (float)dom_b);

    for (i = 0; i < bins; i++) {
        for (j = 0; j < bins; j++) {
            for (q = 0; q < bins; q++) {
                float const bin_count = (float)hist[((size_t)i * (size_t)bins + (size_t)j) * (size_t)bins + (size_t)q];
                wb_elem el;
                float ch[2];
                if (bin_count < WB_EPS) continue;
                wb_chroma(ch, (float)q, (float)j, (float)i);
                el.hist_val = bin_count;
                el.r = ch[0];
                el.g = ch[1];
                if (psize < WB_PALETTE_SIZE) {
                    palette[psize++] = el;
                    if (psize == WB_PALETTE_SIZE) wb_make_heap(palette, psize);
                } else if (bin_count > palette[0].hist_val) {
                    wb_pop_heap(palette, psize);
                    palette[psize - 1] = el;
                    wb_push_heap(palette, psize);
                }
            }
        }
    }
    free(hist);

    /* the palette's mode: an Epanechnikov kernel density, bandwidth 0.1 */
    {
        float max_density = -1.0f;
        float const bw = 0.1f;
        float const den = bw * bw;
        long a, c;
        feat[3][0] = feat[3][1] = 0.0f;
        for (a = 0; a < psize; a++) {
            float cur = 0.0f;
            for (c = 0; c < psize; c++) {
                float const dr = palette[a].r - palette[c].r;
                float const dg = palette[a].g - palette[c].g;
                float const drr = dr * dr;
                float const dgg = dg * dg;
                float const d2 = drr + dgg;
                float const kv = 1.0f - (d2 / den);
                cur += kv < 0.0f ? 0.0f : kv;
            }
            if (cur > max_density) {
                max_density = cur;
                feat[3][0] = palette[a].r;
                feat[3][1] = palette[a].g;
            }
        }
    }

    /* the forest: each tree predicts r and g from one feature; a tree whose four
     * predictions agree (at least three pairs within 0.025) joins the consensus */
    {
        int const fms = 2 * (WB_NUM_TREE_NODES - 1), lms = WB_NUM_FEATURES * fms;
        int const fmsl = 2 * WB_NUM_TREE_NODES, lmsl = WB_NUM_FEATURES * fmsl;
        int tr, f, f2;
        for (tr = 0; tr < WB_NUM_TREES; tr++) {
            float lp[WB_NUM_FEATURES][2];
            int agree = 0;
            for (f = 0; f < WB_NUM_FEATURES; f++) {
                int const o = lms * tr + fms * f, ol = lmsl * tr + fmsl * f;
                lp[f][0] = wb_tree(feat[f], wb_feature_idx + o, wb_thresh_vals + o, wb_leaf_vals + ol);
                lp[f][1] = wb_tree(feat[f], wb_feature_idx + o + fms / 2, wb_thresh_vals + o + fms / 2,
                                   wb_leaf_vals + ol + fmsl / 2);
                all_r[nall] = lp[f][0];
                all_g[nall] = lp[f][1];
                nall++;
            }
            for (f = 0; f < WB_NUM_FEATURES - 1; f++) {
                for (f2 = f + 1; f2 < WB_NUM_FEATURES; f2++) {
                    float const dr = lp[f][0] - lp[f2][0];
                    float const dg = lp[f][1] - lp[f2][1];
                    double s = 0.0;
                    s += (double)dr * (double)dr;
                    s += (double)dg * (double)dg;
                    if (ALWAN_SQRT_F64(s) < (double)0.025f) agree++;
                }
            }
            if (agree >= 3) {
                for (f = 0; f < WB_NUM_FEATURES; f++) {
                    consensus_r[ncons] = lp[f][0];
                    consensus_g[ncons] = lp[f][1];
                    ncons++;
                }
            }
        }
    }
    if (ncons == 0) {
        ill_r = wb_median(all_r, nall);
        ill_g = wb_median(all_g, nall);
    } else {
        ill_r = wb_median(consensus_r, ncons);
        ill_g = wb_median(consensus_g, ncons);
    }

    denom = 1.0f - ill_r;
    denom = denom - ill_g;
    gains_rgb[0] = denom / ill_r;
    gains_rgb[1] = denom / ill_g;
    gains_rgb[2] = 1.0f;
    return ALWAN_OK;
}

/* ------------------------------------------------------------------------------------ */
/* The family                                                                           */
/* ------------------------------------------------------------------------------------ */

static int wb_finite(double v) {
    return v == v && v - v == 0.0;
}

alwan_status alwan_white_balance(void *out, size_t out_row_stride, void const *src, size_t src_row_stride,
                                 alwan_pixel_format format, size_t channels, size_t width, size_t height,
                                 alwan_white_balance_method method, alwan_white_balance_params const *params,
                                 alwan_f64 gains_out[3]) {
    alwan_white_balance_params pr;
    size_t bytes, row_bytes, n;

    if (!src || width == 0 || height == 0) return ALWAN_E_INVALID;
    if (params) {
        pr = *params;
    } else {
        memset(&pr, 0, sizeof pr);
    }
    if (!wb_finite(pr.p) || !wb_finite(pr.input_min) || !wb_finite(pr.input_max) || !wb_finite(pr.output_min) ||
        !wb_finite(pr.output_max) || !wb_finite(pr.saturation_threshold)) {
        return ALWAN_E_INVALID;
    }
    if (width > (size_t)INT_MAX / height) return ALWAN_E_RANGE;
    n = width * height;

    if (method == ALWAN_WHITE_BALANCE_SIMPLE) {
        float const top = format == ALWAN_PIXEL_U8 ? 255.0f : 1.0f;
        float in_min, in_max, out_min, out_max, p;
        size_t c, x, y;
        if (!out) return ALWAN_E_INVALID;
        if (format != ALWAN_PIXEL_U8 && format != ALWAN_PIXEL_F32) return ALWAN_E_INVALID;
        if (channels < 1 || channels > 4) return ALWAN_E_INVALID;
        bytes = format == ALWAN_PIXEL_U8 ? 1 : sizeof(float);
        row_bytes = width * channels * bytes;
        if (src_row_stride < row_bytes || out_row_stride < row_bytes) return ALWAN_E_INVALID;
        in_min = (float)pr.input_min;
        in_max = pr.input_max == 0.0 ? top : (float)pr.input_max;
        out_min = (float)pr.output_min;
        out_max = pr.output_max == 0.0 ? top : (float)pr.output_max;
        p = pr.p == 0.0 ? 2.0f : (float)pr.p;
        if (in_max < in_min) return ALWAN_E_INVALID;
        if (format == ALWAN_PIXEL_F32) {
            for (y = 0; y < height; y++) {
                float const *row = (float const *)((unsigned char const *)src + y * src_row_stride);
                for (x = 0; x < width * channels; x++) {
                    if (!wb_finite((double)row[x])) return ALWAN_E_INVALID;
                }
            }
        }
        for (c = 0; c < channels; c++) {
            wb_simple_channel(out, out_row_stride, src, src_row_stride, format, channels, c, width, height,
                              in_min, in_max, out_min, out_max, p);
        }
        return ALWAN_OK;
    }

    if (method == ALWAN_WHITE_BALANCE_GRAYWORLD || method == ALWAN_WHITE_BALANCE_LEARNING_BASED) {
        wb_img im;
        float gains[3];
        alwan_status st = ALWAN_OK;
        int const top = format == ALWAN_PIXEL_U16 ? 65535 : 255;
        if (!out && !gains_out) return ALWAN_E_INVALID;
        if (format != ALWAN_PIXEL_U8 && format != ALWAN_PIXEL_U16) return ALWAN_E_INVALID;
        if (channels != 3 && channels != 4) return ALWAN_E_INVALID;
        bytes = format == ALWAN_PIXEL_U16 ? 2 : 1;
        row_bytes = width * channels * bytes;
        if (src_row_stride < row_bytes || (out && out_row_stride < row_bytes)) return ALWAN_E_INVALID;
        if (pr.saturation_threshold < 0.0 || pr.saturation_threshold > 1.0) return ALWAN_E_INVALID;
        im.src = src;
        im.src_rs = src_row_stride;
        im.ch = channels;
        im.w = width;
        im.u16 = format == ALWAN_PIXEL_U16;
        if (method == ALWAN_WHITE_BALANCE_GRAYWORLD) {
            float const thr = pr.saturation_threshold == 0.0 ? 0.9f : (float)pr.saturation_threshold;
            wb_grayworld(gains, &im, n, thr);
        } else {
            float const thr = pr.saturation_threshold == 0.0 ? 0.98f : (float)pr.saturation_threshold;
            int const range = pr.range_max == 0 ? top : pr.range_max;
            int const bins = pr.hist_bin_num == 0 ? 64 : pr.hist_bin_num;
            if (range < 1 || range > top || bins < 1 || bins > 256) return ALWAN_E_INVALID;
            st = wb_learning(gains, &im, n, thr, range, bins);
            if (st != ALWAN_OK) return st;
        }
        wb_apply_gains(out, out_row_stride, &im, height, gains);
        if (gains_out) {
            gains_out[0] = (alwan_f64)gains[0];
            gains_out[1] = (alwan_f64)gains[1];
            gains_out[2] = (alwan_f64)gains[2];
        }
        return st;
    }

    return ALWAN_E_INVALID;
}
