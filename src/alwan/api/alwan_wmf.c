/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The weighted median filter: Zhang, Xu and Jia, "100+ Times Faster Weighted Median Filter
 * (WMF)", CVPR 2014, as OpenCV's ximgproc::weightedMedianFilter computes it (opencv_contrib
 * 5.0.0, modules/ximgproc/src/weighted_median_filter.cpp). This file is a port of that code:
 *
 *   - each source channel is quantised to at most 256 levels by OpenCV's adaptive rule (a
 *     binary search on the error bound over the sorted values, each level the median of
 *     its run), filtered as indices, and mapped back to those medians;
 *   - the joint image is one 8-bit channel, 256 feature values, with OpenCV's weight between
 *     every pair (EXP, IV1, IV2, COS, JAC, OFF), in float;
 *   - the joint histogram and the balance counting box (BCB) with their necklace tables are
 *     walked in OpenCV's order, so the float sums of weights are OpenCV's to the bit.
 *
 * OpenCV's three-channel joint clusters the colours with cv::kmeans (k-means++ seeded from
 * its global random generator) and is not ported. alwan's guide is float: it is read as 8
 * bits, clamp(g, 0, 1) x 255 rounded half up, and sigma is in guide units (OpenCV's
 * 0..255 sigma over 255).
 *
 * The OpenCV code carries this notice:
 *
 *   License Agreement For Open Source Computer Vision Library (3 - clause BSD License)
 *
 *   Redistribution and use in source and binary forms, with or without modification,
 *   are permitted provided that the following conditions are met :
 *   * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and / or other materials provided with the distribution.
 *   * Neither the names of the copyright holders nor the names of the contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 *   This software is provided by the copyright holders and contributors "as is" and
 *   any express or implied warranties, including, but not limited to, the implied
 *   warranties of merchantability and fitness for a particular purpose are disclaimed.
 *   In no event shall copyright holders or contributors be liable for any direct,
 *   indirect, incidental, special, exemplary, or consequential damages
 *   (including, but not limited to, procurement of substitute goods or services;
 *   loss of use, data, or profits; or business interruption) however caused
 *   and on any theory of liability, whether in contract, strict liability,
 *   or tort(including negligence or otherwise) arising in any way out of
 *   the use of this software, even if advised of the possibility of such damage.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define WMF_NI 256
#define WMF_NF 256

typedef struct {
    float v;
    int i;
} wmf_pair;

static int wmf_cmp(void const *a, void const *b) {
    float const x = ((wmf_pair const *)a)->v, y = ((wmf_pair const *)b)->v;
    return x < y ? -1 : (x > y ? 1 : 0);
}

/* from32FTo32S: the adaptive quantisation of one float channel to indices, and the value each
 * index maps back to. */
static int wmf_quantise(int *idx, float *mapping, float const *img, size_t alls) {
    wmf_pair *data = (wmf_pair *)ALWAN_ALLOC(alwan_safe_array_size(alls, sizeof(wmf_pair)), sizeof(float));
    double max_val, min_val;
    float max_range, l, r, base;
    size_t i, base_i;
    int cnt;
    if (!data) return 0;
    for (i = 0; i < alls; i++) {
        data[i].i = (int)i;
        data[i].v = img[i];
    }
    qsort(data, alls, sizeof(wmf_pair), wmf_cmp);
    max_val = data[alls - 1].v;
    min_val = data[0].v;
    max_range = (float)(max_val - min_val);
    l = 0.0f;
    r = max_range * 2.0f / (float)WMF_NI;
    while (r > l) {
        float const m = (r + l) * 0.5f;
        int suc = 1;
        if (m == r || m == l) break;
        base = (float)min_val;
        cnt = 0;
        for (i = 0; i < alls; i++) {
            if (data[i].v > base + m) {
                cnt++;
                base = data[i].v;
                if (cnt == WMF_NI) {
                    suc = 0;
                    break;
                }
            }
        }
        if (suc) r = m;
        else l = m;
    }
    base = (float)min_val;
    base_i = 0;
    cnt = 0;
    for (i = 0; i < alls; i++) {
        if (data[i].v > base + r) {
            mapping[cnt] = data[(base_i + i - 1) >> 1].v;
            cnt++;
            base = data[i].v;
            base_i = i;
        }
        idx[data[i].i] = cnt;
    }
    mapping[cnt] = data[(base_i + alls - 1) >> 1].v;
    ALWAN_FREE(data);
    return 1;
}

/* The necklace table of the BCB. */
static void wmf_update_bcb(int *num, int *f, int *b, int i, int v) {
    if (i) {
        if (!*num) {
            int const p2 = f[0];
            f[0] = i;
            f[i] = p2;
            b[p2] = i;
            b[i] = 0;
        } else if (!(*num + v)) {
            int const p1 = b[i], p2 = f[i];
            f[p1] = p2;
            b[p2] = p1;
        }
    }
    *num += v;
}

typedef struct {
    int *h, *hf, *hb;     /* WMF_NI x WMF_NF each */
    int bcb[WMF_NF], bcbf[WMF_NF], bcbb[WMF_NF];
} wmf_state;

/* filterCore: the joint-histogram scan, column by column. out may not be in. */
static void wmf_core(int *out, int const *in, int const *feat, float const *wmap, int rows, int cols, int r, wmf_state *s) {
    int x, y, i, j;
    memcpy(out, in, (size_t)rows * (size_t)cols * sizeof(int));
    for (x = 0; x < cols; x++) {
        int median = -1;
        int const down_x = x - r > 0 ? x - r : 0;
        int const up_x = x + r < cols - 1 ? x + r : cols - 1;
        int const up_y = r < rows - 1 ? r : rows - 1;
        memset(s->bcb, 0, sizeof s->bcb);
        memset(s->h, 0, (size_t)WMF_NI * WMF_NF * sizeof(int));
        for (i = 0; i < WMF_NI; i++) s->hf[i * WMF_NF] = s->hb[i * WMF_NF] = 0;
        s->bcbf[0] = s->bcbb[0] = 0;

        for (i = 0; i <= up_y; i++) {
            int const *ip = in + (size_t)i * (size_t)cols;
            int const *fp = feat + (size_t)i * (size_t)cols;
            for (j = down_x; j <= up_x; j++) {
                int const fval = ip[j], gval = fp[j];
                int *hist = s->h + (size_t)fval * WMF_NF;
                if (!hist[gval] && gval) {
                    int *hf = s->hf + (size_t)fval * WMF_NF;
                    int *hb = s->hb + (size_t)fval * WMF_NF;
                    int const p1 = 0, p2 = hf[0];
                    hf[p1] = gval;
                    hf[gval] = p2;
                    hb[p2] = gval;
                    hb[gval] = p1;
                }
                hist[gval]++;
                wmf_update_bcb(&s->bcb[gval], s->bcbf, s->bcbb, gval, -1);
            }
        }

        for (y = 0; y < rows; y++) {
            float balance = 0.0f;
            int const cur = feat[(size_t)y * (size_t)cols + (size_t)x];
            float const *fw = wmap + (size_t)cur * WMF_NF;
            int rownum;
            i = 0;
            do {
                balance += (float)s->bcb[i] * fw[i];
                i = s->bcbf[i];
            } while (i);

            if (balance >= 0.0f) {
                for (; balance >= 0.0f && median > 0; median--) {
                    float weight = 0.0f;
                    int const *next = s->h + (size_t)median * WMF_NF;
                    int const *nf = s->hf + (size_t)median * WMF_NF;
                    i = 0;
                    do {
                        weight += (float)(next[i] << 1) * fw[i];
                        wmf_update_bcb(&s->bcb[i], s->bcbf, s->bcbb, i, -(next[i] << 1));
                        i = nf[i];
                    } while (i);
                    balance -= weight;
                }
            } else if (balance < 0.0f) {
                for (; balance < 0.0f && median != WMF_NI - 1; median++) {
                    float weight = 0.0f;
                    int const *next = s->h + (size_t)(median + 1) * WMF_NF;
                    int const *nf = s->hf + (size_t)(median + 1) * WMF_NF;
                    i = 0;
                    do {
                        weight += (float)(next[i] << 1) * fw[i];
                        wmf_update_bcb(&s->bcb[i], s->bcbf, s->bcbb, i, next[i] << 1);
                        i = nf[i];
                    } while (i);
                    balance += weight;
                }
            }

            if (median != -1) out[(size_t)y * (size_t)cols + (size_t)x] = balance < 0.0f ? median + 1 : median;

            rownum = y + r + 1;
            if (rownum < rows) {
                int const *ip = in + (size_t)rownum * (size_t)cols;
                int const *fp = feat + (size_t)rownum * (size_t)cols;
                for (j = down_x; j <= up_x; j++) {
                    int const fval = ip[j], gval = fp[j];
                    int *hist = s->h + (size_t)fval * WMF_NF;
                    if (!hist[gval] && gval) {
                        int *hf = s->hf + (size_t)fval * WMF_NF;
                        int *hb = s->hb + (size_t)fval * WMF_NF;
                        int const p1 = 0, p2 = hf[0];
                        hf[gval] = p2;
                        hb[gval] = p1;
                        hf[p1] = hb[p2] = gval;
                    }
                    hist[gval]++;
                    wmf_update_bcb(&s->bcb[gval], s->bcbf, s->bcbb, gval, ((fval <= median) << 1) - 1);
                }
            }
            rownum = y - r;
            if (rownum >= 0) {
                int const *ip = in + (size_t)rownum * (size_t)cols;
                int const *fp = feat + (size_t)rownum * (size_t)cols;
                for (j = down_x; j <= up_x; j++) {
                    int const fval = ip[j], gval = fp[j];
                    int *hist = s->h + (size_t)fval * WMF_NF;
                    hist[gval]--;
                    if (!hist[gval] && gval) {
                        int *hf = s->hf + (size_t)fval * WMF_NF;
                        int *hb = s->hb + (size_t)fval * WMF_NF;
                        int const p1 = hb[gval], p2 = hf[gval];
                        hf[p1] = p2;
                        hb[p2] = p1;
                    }
                    wmf_update_bcb(&s->bcb[gval], s->bcbf, s->bcbb, gval, -((fval <= median) << 1) + 1);
                }
            }
        }
    }
}

/* featureIndexing for one 8-bit channel: the weight between every pair of values. */
static void wmf_weights(float *wmap, float sigma, alwan_wmf_weight type) {
    float const divider = 1.0f / (2.0f * sigma * sigma);
    int i, j;
    for (i = 0; i < WMF_NF; i++) {
#if defined(_MSC_VER)
#pragma loop(no_vector)
#endif
        for (j = i; j < WMF_NF; j++) {
            float const diff = ALWAN_ABS_F32((float)(i - j));
            float val;
            switch (type) {
            case ALWAN_WMF_IV1: val = 1.0f / (diff + sigma); break;
            case ALWAN_WMF_IV2: val = 1.0f / (diff * diff + sigma * sigma); break;
            case ALWAN_WMF_COS: val = 1.0f; break;
            case ALWAN_WMF_JAC: val = (float)((double)(i < j ? i : j) * 1.0 / (double)(i > j ? i : j)); break;
            case ALWAN_WMF_OFF: val = 1.0f; break;
            case ALWAN_WMF_EXP:
            default: val = ALWAN_EXP_F32(-(diff * diff) * divider); break;
            }
            wmap[i * WMF_NF + j] = wmap[j * WMF_NF + i] = val;
        }
    }
}

alwan_status alwan__wmf_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t sch,
                            void const *guide, size_t guide_row_stride, size_t gch, size_t w, size_t h,
                            size_t radius, double sigma, alwan_wmf_weight type, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    float *plane = NULL, *wmap = NULL, *mapping = NULL;
    int *feat = NULL, *idx = NULL, *res = NULL;
    wmf_state st;
    size_t x, y, c;
    alwan_status status = ALWAN_OK;

    memset(&st, 0, sizeof st);
    if (!out || !src || !guide) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || sch == 0 || sch > 4) return ALWAN_E_INVALID;
    if (gch != 1) return ALWAN_E_INVALID; /* OpenCV's 3-channel joint (k-means) is not ported */
    if (src_row_stride / elem / sch < w || guide_row_stride / elem / gch < w || out_row_stride / elem / sch < w) {
        return ALWAN_E_INVALID;
    }
    if ((int)type < (int)ALWAN_WMF_EXP || (int)type > (int)ALWAN_WMF_OFF) return ALWAN_E_INVALID;
    if (!(sigma > 0.0 && sigma <= 1e6) || radius == 0 || radius > 10000) return ALWAN_E_RANGE;
    if (w > (size_t)INT_MAX || h > (size_t)INT_MAX || n / w != h || n >= (size_t)INT_MAX) return ALWAN_E_RANGE;

    plane = (float *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(float)), sizeof(float));
    wmap = (float *)ALWAN_ALLOC((size_t)WMF_NF * WMF_NF * sizeof(float), sizeof(float));
    mapping = (float *)ALWAN_ALLOC((size_t)WMF_NI * sizeof(float), sizeof(float));
    feat = (int *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(int)), sizeof(int));
    idx = (int *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(int)), sizeof(int));
    res = (int *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(int)), sizeof(int));
    st.h = (int *)ALWAN_ALLOC((size_t)WMF_NI * WMF_NF * sizeof(int), sizeof(int));
    st.hf = (int *)ALWAN_ALLOC((size_t)WMF_NI * WMF_NF * sizeof(int), sizeof(int));
    st.hb = (int *)ALWAN_ALLOC((size_t)WMF_NI * WMF_NF * sizeof(int), sizeof(int));
    if (!plane || !wmap || !mapping || !feat || !idx || !res || !st.h || !st.hf || !st.hb) {
        status = ALWAN_E_NOMEM;
        goto done;
    }

    /* the guide as 8 bits */
    for (y = 0; y < h; y++) {
        char const *grow = (char const *)guide + y * guide_row_stride;
        for (x = 0; x < w; x++) {
            double v = is_f32 ? (double)((alwan_f32 const *)grow)[x] : ((alwan_f64 const *)grow)[x];
            double q;
            if (!(v == v && v <= DBL_MAX && v >= -DBL_MAX)) { status = ALWAN_E_INVALID; goto done; }
            v = v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
            q = ALWAN_FLOOR_F64(v * 255.0 + 0.5);
            feat[y * w + x] = (int)q;
        }
    }
    wmf_weights(wmap, (float)(sigma * 255.0), type);

    for (c = 0; c < sch; c++) {
        for (y = 0; y < h; y++) {
            char const *srow = (char const *)src + y * src_row_stride;
            for (x = 0; x < w; x++) {
                double const v = is_f32 ? (double)((alwan_f32 const *)srow)[x * sch + c] : ((alwan_f64 const *)srow)[x * sch + c];
                if (!(v == v && v <= DBL_MAX && v >= -DBL_MAX)) { status = ALWAN_E_INVALID; goto done; }
                plane[y * w + x] = (float)v;
            }
        }
        if (!wmf_quantise(idx, mapping, plane, n)) { status = ALWAN_E_NOMEM; goto done; }
        wmf_core(res, idx, feat, wmap, (int)h, (int)w, radius > (size_t)INT_MAX / 4 ? INT_MAX / 4 : (int)radius, &st);
        for (y = 0; y < h; y++) {
            char *orow = (char *)out + y * out_row_stride;
            for (x = 0; x < w; x++) {
                float const v = mapping[res[y * w + x]];
                if (is_f32) ((alwan_f32 *)orow)[x * sch + c] = v;
                else ((alwan_f64 *)orow)[x * sch + c] = (alwan_f64)v;
            }
        }
    }

done:
    if (plane) ALWAN_FREE(plane);
    if (wmap) ALWAN_FREE(wmap);
    if (mapping) ALWAN_FREE(mapping);
    if (feat) ALWAN_FREE(feat);
    if (idx) ALWAN_FREE(idx);
    if (res) ALWAN_FREE(res);
    if (st.h) ALWAN_FREE(st.h);
    if (st.hf) ALWAN_FREE(st.hf);
    if (st.hb) ALWAN_FREE(st.hb);
    return status;
}
