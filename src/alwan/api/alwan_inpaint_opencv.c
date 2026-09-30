/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ALWAN_INPAINT_TELEA_OPENCV and ALWAN_INPAINT_NS_OPENCV: cv::inpaint's two methods, ported
 * from OpenCV 5.0.0 modules/photo/src/inpaint.cpp, which carries this notice:
 *
 *   Intel License Agreement, For Open Source Computer Vision Library
 *   Copyright (C) 2000, Intel Corporation, all rights reserved.
 *   Third party copyrights are property of their respective icvers.
 *
 *   Redistribution and use in source and binary forms, with or without modification, are
 *   permitted provided that the following conditions are met:
 *   * Redistribution's of source code must retain the above copyright notice, this list of
 *     conditions and the following disclaimer.
 *   * Redistribution's in binary form must reproduce the above copyright notice, this list
 *     of conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *   * The name of Intel Corporation may not be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *   This software is provided by the copyright holders and contributors "as is" and any
 *   express or implied warranties, including, but not limited to, the implied warranties
 *   of merchantability and fitness for a particular purpose are disclaimed. In no event
 *   shall the Intel Corporation or contributors be liable for any direct, indirect,
 *   incidental, special, exemplary, or consequential damages (including, but not limited
 *   to, procurement of substitute goods or services; loss of use, data, or profits; or
 *   business interruption) however caused and on any theory of liability, whether in
 *   contract, strict liability, or tort (including negligence or otherwise) arising in any
 *   way out of the use of this software, even if advised of the possibility of such damage.
 *
 * TELEA is Telea 2004's fast marching fill; NS the Navier-Stokes flavoured variant after
 * Bertalmio, Bertozzi and Sapiro 2001, as OpenCV writes it. Both march the mask's boundary
 * inwards in order of distance (a heap keyed by arrival time, ties in insertion order) and
 * set each pixel from a weighted sum over the known pixels within the radius.
 *
 * The image is held as float planes. OpenCV computes its 8- and 16-bit gradients as int
 * differences and then floats them; those are exact in float, so one float path serves
 * every type and only the final rounding differs: TELEA writes an integer type as
 * saturate(cvRound(sat + 0.5)) (OpenCV's round_cast adds the half before cvRound rounds
 * half to even, so a result lands one level up from the nearest more often than not), NS
 * as saturate(cvRound((double)Ia / s)). cvRound of NaN or of anything past the int range
 * is INT_MIN, which saturates to 0.
 *
 * Kept as OpenCV has them, and documented in alwan.h:
 *   - TELEA's central image difference is scaled by 2, where the forward and backward ones
 *     are not; its three-channel path takes the distance weight's square root in double,
 *     the one-channel path in float.
 *   - Next to the image's first row or column the gradient reads are shifted one pixel
 *     inwards (km = k - 1 + (k == 1)), so they can read a masked pixel's src value before
 *     it is filled.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define IPCV_KNOWN 0
#define IPCV_BAND 1
#define IPCV_INSIDE 2
#define IPCV_CHANGE 3

typedef struct {
    float T;
    int i, j;
    int order;
} ipcv_elem;

typedef struct {
    ipcv_elem *a;
    size_t n, cap;
    int next_order;
} ipcv_heap;

/* std::priority_queue with std::greater: the smallest T first, equal T in insertion order. */
static int ipcv_less(ipcv_elem const *x, ipcv_elem const *y) {
    if (x->T < y->T) return 1;
    if (x->T > y->T) return 0;
    return x->order < y->order;
}

static void ipcv_push(ipcv_heap *h, int i, int j, float T) {
    size_t k = h->n++;
    ipcv_elem e;
    e.T = T; e.i = i; e.j = j; e.order = h->next_order++;
    while (k > 0) {
        size_t const p = (k - 1) / 2;
        if (!ipcv_less(&e, &h->a[p])) break;
        h->a[k] = h->a[p];
        k = p;
    }
    h->a[k] = e;
}

static int ipcv_pop(ipcv_heap *h, int *i, int *j) {
    ipcv_elem last;
    size_t k = 0;
    if (h->n == 0) return 0;
    *i = h->a[0].i;
    *j = h->a[0].j;
    last = h->a[--h->n];
    for (;;) {
        size_t c = 2 * k + 1;
        if (c >= h->n) break;
        if (c + 1 < h->n && ipcv_less(&h->a[c + 1], &h->a[c])) c++;
        if (!ipcv_less(&h->a[c], &last)) break;
        h->a[k] = h->a[c];
        k = c;
    }
    if (h->n > 0) h->a[k] = last;
    return 1;
}

/* CvPriorityQueueFloat::Add: every non-zero pixel, raster order, at T = 0. */
static void ipcv_add(ipcv_heap *h, unsigned char const *f, int rows, int cols) {
    int i, j;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            if (f[i * cols + j] != 0) ipcv_push(h, i, j, 0.0f);
        }
    }
}

static float ipcv_fabsf(float v) {
    return v < 0.0f ? -v : v;
}

/* cvRound (SSE2 cvtsd2si, ties to even; NaN and out of range give INT_MIN), saturated. */
static long long ipcv_cvround(double v) {
    double fl, fr;
    long long i;
    if (!(v >= -2147483648.5 && v < 2147483647.5)) return (long long)INT_MIN;
    fl = ALWAN_FLOOR_F64(v);
    fr = v - fl;
    i = (long long)fl;
    if (fr > 0.5 || (fr == 0.5 && (i & 1))) i++;
    return i;
}

static float ipcv_sat(double v, int kind) {
    long long i;
    if (kind == 2) return (float)v;
    i = ipcv_cvround(v);
    if (i < 0) i = 0;
    if (kind == 0 && i > 255) i = 255;
    if (kind == 1 && i > 65535) i = 65535;
    return (float)i;
}

/* FastMarching_solve, in double as OpenCV writes it. */
static float ipcv_solve(int i1, int j1, int i2, int j2, unsigned char const *f, float const *t, int cols) {
    double sol;
    double const a11 = t[i1 * cols + j1], a22 = t[i2 * cols + j2];
    double const m12 = a11 < a22 ? a11 : a22;
    if (f[i1 * cols + j1] != IPCV_INSIDE) {
        if (f[i2 * cols + j2] != IPCV_INSIDE) {
            if (ALWAN_ABS_F64(a11 - a22) >= 1.0) sol = 1 + m12;
            else sol = (a11 + a22 + ALWAN_SQRT_F64((double)(2 - (a11 - a22) * (a11 - a22)))) * 0.5;
        } else {
            sol = 1 + a11;
        }
    } else if (f[i2 * cols + j2] != IPCV_INSIDE) {
        sol = 1 + a22;
    } else {
        sol = 1 + m12;
    }
    return (float)sol;
}

static float ipcv_min4(float a, float b, float c, float d) {
    a = a < b ? a : b;
    c = c < d ? c : d;
    return a < c ? a : c;
}

static float ipcv_arrival(int i, int j, unsigned char const *f, float const *t, int cols) {
    return ipcv_min4(ipcv_solve(i - 1, j, i, j - 1, f, t, cols), ipcv_solve(i + 1, j, i, j - 1, f, t, cols),
                     ipcv_solve(i - 1, j, i, j + 1, f, t, cols), ipcv_solve(i + 1, j, i, j + 1, f, t, cols));
}

/* icvCalcFMM with negate: the arrival times outwards from the band, over the ring TELEA
 * weighs, stored negated. */
static void ipcv_calc_fmm(unsigned char *f, float *t, ipcv_heap *heap, int rows, int cols) {
    int ii, jj, q, i, j;
    while (ipcv_pop(heap, &ii, &jj)) {
        f[ii * cols + jj] = IPCV_CHANGE;
        for (q = 0; q < 4; q++) {
            if (q == 0) { i = ii - 1; j = jj; }
            else if (q == 1) { i = ii; j = jj - 1; }
            else if (q == 2) { i = ii + 1; j = jj; }
            else { i = ii; j = jj + 1; }
            /* OpenCV tests i > rows; a pixel on the zeroed border is never INSIDE, so the
             * row past it is never reached, and this bound keeps it that way */
            if (i <= 0 || j <= 0 || i >= rows - 1 || j >= cols - 1) continue;
            if (f[i * cols + j] == IPCV_INSIDE) {
                float const dist = ipcv_arrival(i, j, f, t, cols);
                t[i * cols + j] = dist;
                f[i * cols + j] = IPCV_BAND;
                ipcv_push(heap, i, j, dist);
            }
        }
    }
    for (i = 0; i < rows * cols; i++) {
        if (f[i] == IPCV_CHANGE) {
            f[i] = IPCV_KNOWN;
            t[i] = -t[i];
        }
    }
}

/* The border of an erows x ecols byte image set to 0 (SET_BORDER1_C1). */
static void ipcv_zero_border(unsigned char *m, int rows, int cols) {
    int i;
    for (i = 0; i < cols; i++) { m[i] = 0; m[(rows - 1) * cols + i] = 0; }
    for (i = 0; i < rows; i++) { m[i * cols] = 0; m[i * cols + cols - 1] = 0; }
}

/* cv::dilate with a (2 r + 1)-square (or, r = 0, the 3 x 3 cross), the outside ignored. */
static void ipcv_dilate(unsigned char *out, unsigned char const *in, int rows, int cols, int r, int cross) {
    int i, j, a, b;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            unsigned char m = 0;
            if (cross) {
                m = in[i * cols + j];
                if (i > 0 && in[(i - 1) * cols + j] > m) m = in[(i - 1) * cols + j];
                if (i + 1 < rows && in[(i + 1) * cols + j] > m) m = in[(i + 1) * cols + j];
                if (j > 0 && in[i * cols + j - 1] > m) m = in[i * cols + j - 1];
                if (j + 1 < cols && in[i * cols + j + 1] > m) m = in[i * cols + j + 1];
            } else {
                int const a0 = i - r < 0 ? 0 : i - r, a1 = i + r >= rows ? rows - 1 : i + r;
                int const b0 = j - r < 0 ? 0 : j - r, b1 = j + r >= cols ? cols - 1 : j + r;
                for (a = a0; a <= a1; a++) {
                    for (b = b0; b <= b1; b++) {
                        if (in[a * cols + b] > m) m = in[a * cols + b];
                    }
                }
            }
            out[i * cols + j] = m;
        }
    }
}

static void ipcv_subtract(unsigned char *a, unsigned char const *b, size_t n) {
    size_t k;
    for (k = 0; k < n; k++) a[k] = (unsigned char)(a[k] > b[k] ? a[k] - b[k] : 0);
}

/* One run of icvInpaint on nplanes float planes of w x h (nplanes 3 = OpenCV's CV_8UC3
 * path, 1 = its one-channel path), kind 0 u8, 1 u16, 2 f32 for the write rounding. */
static alwan_status ipcv_run(float *const *P, int nplanes, int kind, int w, int h, unsigned char const *mask,
                             size_t mask_rs, int ns, int range) {
    int const rows = h + 2, cols = w + 2;
    size_t const en = (size_t)rows * (size_t)cols;
    unsigned char *M, *band, *ring = NULL;
    float *t;
    ipcv_heap heap, out_heap;
    int i, j, ii, jj, q, k, l, c;
    alwan_status st = ALWAN_OK;

    memset(&heap, 0, sizeof heap);
    memset(&out_heap, 0, sizeof out_heap);
    M = (unsigned char *)ALWAN_ALLOC(en, 1);
    band = (unsigned char *)ALWAN_ALLOC(en, 1);
    t = (float *)ALWAN_ALLOC(alwan_safe_array_size(en, sizeof(float)), sizeof(float));
    heap.a = (ipcv_elem *)ALWAN_ALLOC(alwan_safe_array_size(en, sizeof(ipcv_elem)), sizeof(float));
    heap.cap = en;
    if (!ns) {
        ring = (unsigned char *)ALWAN_ALLOC(en, 1);
        out_heap.a = (ipcv_elem *)ALWAN_ALLOC(alwan_safe_array_size(en, sizeof(ipcv_elem)), sizeof(float));
        out_heap.cap = en;
    }
    if (!M || !band || !t || !heap.a || (!ns && (!ring || !out_heap.a))) {
        st = ALWAN_E_NOMEM;
        goto done;
    }

    memset(M, IPCV_KNOWN, en);
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            if (mask[(size_t)i * mask_rs + (size_t)j] != 0) M[(size_t)(i + 1) * (size_t)cols + (size_t)(j + 1)] = IPCV_INSIDE;
        }
    }
    for (k = 0; k < (int)en; k++) t[k] = 1.0e6f;
    ipcv_dilate(band, M, rows, cols, 0, 1);
    ipcv_subtract(band, M, en);
    ipcv_zero_border(band, rows, cols);
    ipcv_add(&heap, band, rows, cols);
    for (k = 0; k < (int)en; k++) {
        if (band[k]) t[k] = 0.0f;
    }

    if (!ns) {
        ipcv_dilate(ring, M, rows, cols, range, 0);
        ipcv_subtract(ring, M, en);
        ipcv_add(&out_heap, band, rows, cols);
        ipcv_subtract(ring, band, en);
        ipcv_zero_border(ring, rows, cols);
        ipcv_calc_fmm(ring, t, &out_heap, rows, cols);
    }

    while (ipcv_pop(&heap, &ii, &jj)) {
        M[ii * cols + jj] = IPCV_KNOWN;
        for (q = 0; q < 4; q++) {
            float dist;
            if (q == 0) { i = ii - 1; j = jj; }
            else if (q == 1) { i = ii; j = jj - 1; }
            else if (q == 2) { i = ii + 1; j = jj; }
            else { i = ii; j = jj + 1; }
            if (i <= 0 || j <= 0 || i > rows - 1 || j > cols - 1) continue;
            if (M[i * cols + j] != IPCV_INSIDE) continue;

            dist = ipcv_arrival(i, j, M, t, cols);
            t[i * cols + j] = dist;

            if (!ns) {
                float gtx, gty;
                float Ia[3] = {0.0f, 0.0f, 0.0f}, Jx[3] = {0.0f, 0.0f, 0.0f}, Jy[3] = {0.0f, 0.0f, 0.0f};
                float s[3] = {1.0e-20f, 1.0e-20f, 1.0e-20f};
                float const tij = t[i * cols + j];
                if (M[i * cols + j + 1] != IPCV_INSIDE) {
                    if (M[i * cols + j - 1] != IPCV_INSIDE) gtx = (float)(t[i * cols + j + 1] - t[i * cols + j - 1]) * 0.5f;
                    else gtx = (float)(t[i * cols + j + 1] - tij);
                } else {
                    gtx = M[i * cols + j - 1] != IPCV_INSIDE ? (float)(tij - t[i * cols + j - 1]) : 0.0f;
                }
                if (M[(i + 1) * cols + j] != IPCV_INSIDE) {
                    if (M[(i - 1) * cols + j] != IPCV_INSIDE) gty = (float)(t[(i + 1) * cols + j] - t[(i - 1) * cols + j]) * 0.5f;
                    else gty = (float)(t[(i + 1) * cols + j] - tij);
                } else {
                    gty = M[(i - 1) * cols + j] != IPCV_INSIDE ? (float)(tij - t[(i - 1) * cols + j]) : 0.0f;
                }
                for (k = i - range; k <= i + range; k++) {
                    int const km = k - 1 + (k == 1), kp = k - 1 - (k == rows - 2);
                    for (l = j - range; l <= j + range; l++) {
                        int const lm = l - 1 + (l == 1), lp = l - 1 - (l == cols - 2);
                        float rx, ry, vl, dst, lev, dir, wgt;
                        if (!(k > 0 && l > 0 && k < rows - 1 && l < cols - 1)) continue;
                        if (M[k * cols + l] == IPCV_INSIDE) continue;
                        if ((l - j) * (l - j) + (k - i) * (k - i) > range * range) continue;
                        ry = (float)(i - k);
                        rx = (float)(j - l);
                        vl = rx * rx + ry * ry;
                        if (nplanes == 3) dst = (float)(1. / ((double)vl * ALWAN_SQRT_F64((double)vl)));
                        else dst = (float)(1. / (double)(vl * ALWAN_SQRT_F32(vl)));
                        lev = (float)(1. / (double)(1.0f + ipcv_fabsf(t[k * cols + l] - tij)));
                        dir = rx * gtx + ry * gty;
                        if (ipcv_fabsf(dir) <= 0.01) dir = 0.000001f;
                        wgt = ipcv_fabsf(dst * lev * dir);
                        for (c = 0; c < nplanes; c++) {
                            float const *p = P[c];
                            float gix, giy;
                            if (M[k * cols + l + 1] != IPCV_INSIDE) {
                                if (M[k * cols + l - 1] != IPCV_INSIDE) gix = (float)(p[km * w + lp + 1] - p[km * w + lm - 1]) * 2.0f;
                                else gix = (float)(p[km * w + lp + 1] - p[km * w + lm]);
                            } else {
                                gix = M[k * cols + l - 1] != IPCV_INSIDE ? (float)(p[km * w + lp] - p[km * w + lm - 1]) : 0.0f;
                            }
                            if (M[(k + 1) * cols + l] != IPCV_INSIDE) {
                                if (M[(k - 1) * cols + l] != IPCV_INSIDE) giy = (float)(p[(kp + 1) * w + lm] - p[(km - 1) * w + lm]) * 2.0f;
                                else giy = (float)(p[(kp + 1) * w + lm] - p[km * w + lm]);
                            } else {
                                giy = M[(k - 1) * cols + l] != IPCV_INSIDE ? (float)(p[kp * w + lm] - p[(km - 1) * w + lm]) : 0.0f;
                            }
                            Ia[c] += wgt * p[(k - 1) * w + (l - 1)];
                            Jx[c] -= wgt * (gix * rx);
                            Jy[c] -= wgt * (giy * ry);
                            s[c] += wgt;
                        }
                    }
                }
                for (c = 0; c < nplanes; c++) {
                    float const sat = Ia[c] / s[c] + (Jx[c] + Jy[c]) / (ALWAN_SQRT_F32(Jx[c] * Jx[c] + Jy[c] * Jy[c]) + 1.0e-20f);
                    P[c][(i - 1) * w + (j - 1)] = kind == 2 ? sat : ipcv_sat((double)sat + 0.5, kind);
                }
            } else {
                float Ia[3] = {0.0f, 0.0f, 0.0f};
                float s[3] = {1.0e-20f, 1.0e-20f, 1.0e-20f};
                for (k = i - range; k <= i + range; k++) {
                    int const km = k - 1 + (k == 1), kp = k - 1 - (k == rows - 2);
                    for (l = j - range; l <= j + range; l++) {
                        int const lm = l - 1 + (l == 1), lp = l - 1 - (l == cols - 2);
                        float rx, ry, vl, dst;
                        if (!(k > 0 && l > 0 && k < rows - 1 && l < cols - 1)) continue;
                        if (M[k * cols + l] == IPCV_INSIDE) continue;
                        if ((l - j) * (l - j) + (k - i) * (k - i) > range * range) continue;
                        /* the three-channel path takes r = (k - i, l - j), the one-channel
                         * path its negative; dir is taken absolute, so the two agree */
                        ry = (float)(i - k);
                        rx = (float)(j - l);
                        vl = rx * rx + ry * ry;
                        dst = 1.0f / (vl * vl + 1.0f);
                        for (c = 0; c < nplanes; c++) {
                            float const *p = P[c];
                            float gix, giy, dir, wgt;
                            if (M[(k + 1) * cols + l] != IPCV_INSIDE) {
                                if (M[(k - 1) * cols + l] != IPCV_INSIDE)
                                    gix = ipcv_fabsf(p[(kp + 1) * w + lm] - p[kp * w + lm]) + ipcv_fabsf(p[kp * w + lm] - p[(km - 1) * w + lm]);
                                else
                                    gix = ipcv_fabsf(p[(kp + 1) * w + lm] - p[kp * w + lm]) * 2.0f;
                            } else {
                                gix = M[(k - 1) * cols + l] != IPCV_INSIDE ? ipcv_fabsf(p[kp * w + lm] - p[(km - 1) * w + lm]) * 2.0f : 0.0f;
                            }
                            if (M[k * cols + l + 1] != IPCV_INSIDE) {
                                if (M[k * cols + l - 1] != IPCV_INSIDE)
                                    giy = ipcv_fabsf(p[km * w + lp + 1] - p[km * w + lm]) + ipcv_fabsf(p[km * w + lm] - p[km * w + lm - 1]);
                                else
                                    giy = ipcv_fabsf(p[km * w + lp + 1] - p[km * w + lm]) * 2.0f;
                            } else {
                                giy = M[k * cols + l - 1] != IPCV_INSIDE ? ipcv_fabsf(p[km * w + lm] - p[km * w + lm - 1]) * 2.0f : 0.0f;
                            }
                            gix = -gix;
                            dir = rx * gix + ry * giy;
                            if (ipcv_fabsf(dir) <= 0.01) {
                                dir = 0.000001f;
                            } else {
                                dir = ipcv_fabsf((rx * gix + ry * giy) / ALWAN_SQRT_F32(vl * (gix * gix + giy * giy)));
                            }
                            wgt = dst * dir;
                            Ia[c] += wgt * p[(k - 1) * w + (l - 1)];
                            s[c] += wgt;
                        }
                    }
                }
                for (c = 0; c < nplanes; c++) P[c][(i - 1) * w + (j - 1)] = ipcv_sat((double)Ia[c] / s[c], kind);
            }
            M[i * cols + j] = IPCV_BAND;
            ipcv_push(&heap, i, j, dist);
        }
    }

done:
    if (M) ALWAN_FREE(M);
    if (band) ALWAN_FREE(band);
    if (ring) ALWAN_FREE(ring);
    if (t) ALWAN_FREE(t);
    if (heap.a) ALWAN_FREE(heap.a);
    if (out_heap.a) ALWAN_FREE(out_heap.a);
    return st;
}

/* cvRound(radius) clamped to 1..100; 0 is the default 3. */
static int ipcv_range(alwan_inpaint_params const *params, int *range) {
    double r = params ? params->radius : 0.0;
    long long v;
    if (r == 0.0) r = 3.0;
    if (!(r > 0.0 && r <= DBL_MAX)) return 0;
    v = ipcv_cvround(r);
    if (v < 1) v = 1;
    if (v > 100) v = 100;
    *range = (int)v;
    return 1;
}

/* The heap holds every pixel at most once per push, so erows x ecols bounds it; keep that
 * and the int indices OpenCV uses in range. */
static int ipcv_size_ok(size_t w, size_t h) {
    return w <= (size_t)INT_MAX / 4 && h <= (size_t)INT_MAX / 4 && (w + 2) <= (size_t)INT_MAX / (h + 2);
}

/* The TELEA and NS entry, formats U8 (1, 3 or 4 channels), U16, F32 and F64 (1 to 4). Every
 * channel of U16, F32 and F64, and a one-channel U8, runs OpenCV's one-channel path on its
 * own; U8 with 3 or 4 channels runs the CV_8UC3 path on the first three and copies the
 * fourth. F64 is computed in float, as OpenCV's CV_32F, and only the masked pixels are
 * written back from it. */
alwan_status alwan__inpaint_opencv(void *out, size_t out_rs, void const *src, size_t src_rs, alwan_pixel_format format,
                                   size_t ch, size_t w, size_t h, unsigned char const *mask, size_t mask_rs,
                                   alwan_inpaint_method method, alwan_inpaint_params const *params) {
    size_t elem, n, x, y, c, known = 0;
    float *planes[4] = {NULL, NULL, NULL, NULL};
    int range, kind, joint;
    alwan_status st = ALWAN_OK;

    if (!out || !src || !mask || w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (method != ALWAN_INPAINT_TELEA_OPENCV && method != ALWAN_INPAINT_NS_OPENCV) return ALWAN_E_INVALID;
    switch (format) {
    case ALWAN_PIXEL_U8: elem = 1; kind = 0; if (ch == 2) return ALWAN_E_INVALID; break;
    case ALWAN_PIXEL_U16: elem = 2; kind = 1; break;
    case ALWAN_PIXEL_F32: elem = 4; kind = 2; break;
    case ALWAN_PIXEL_F64: elem = 8; kind = 2; break;
    default: return ALWAN_E_INVALID;
    }
    if (!ipcv_size_ok(w, h)) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w || mask_rs < w) return ALWAN_E_INVALID;
    if (!ipcv_range(params, &range)) return ALWAN_E_INVALID;
    n = w * h;
    joint = format == ALWAN_PIXEL_U8 && ch >= 3;

    for (c = 0; c < ch; c++) {
        planes[c] = (float *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(float)), sizeof(float));
        if (!planes[c]) { st = ALWAN_E_NOMEM; goto done; }
    }
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            int const m = mask[y * mask_rs + x] != 0;
            known += !m;
            for (c = 0; c < ch; c++) {
                size_t const at = x * ch + c;
                float v;
                if (format == ALWAN_PIXEL_U8) v = (float)((unsigned char const *)row)[at];
                else if (format == ALWAN_PIXEL_U16) v = (float)((unsigned short const *)row)[at];
                else if (format == ALWAN_PIXEL_F32) v = ((float const *)row)[at];
                else v = (float)((double const *)row)[at];
                /* a known value must be finite; a masked one can be read (see above) but
                 * is not checked, as OpenCV does not check it */
                if (kind == 2 && !m && !(v == v && v <= FLT_MAX && v >= -FLT_MAX)) { st = ALWAN_E_INVALID; goto done; }
                planes[c][y * w + x] = v;
            }
        }
    }
    if (known == 0) { st = ALWAN_E_INVALID; goto done; }

    if (joint) {
        st = ipcv_run(planes, 3, kind, (int)w, (int)h, mask, mask_rs, method == ALWAN_INPAINT_NS_OPENCV, range);
    } else {
        for (c = 0; c < ch && st == ALWAN_OK; c++) {
            st = ipcv_run(&planes[c], 1, kind, (int)w, (int)h, mask, mask_rs, method == ALWAN_INPAINT_NS_OPENCV, range);
        }
    }
    if (st != ALWAN_OK) goto done;

    for (y = 0; y < h; y++) {
        char *orow = (char *)out + y * out_rs;
        char const *irow = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            int const m = mask[y * mask_rs + x] != 0;
            for (c = 0; c < ch; c++) {
                size_t const at = x * ch + c;
                float const v = planes[c][y * w + x];
                if (joint && c == 3) {
                    ((unsigned char *)orow)[at] = ((unsigned char const *)irow)[at];
                } else if (format == ALWAN_PIXEL_U8) {
                    ((unsigned char *)orow)[at] = (unsigned char)v;
                } else if (format == ALWAN_PIXEL_U16) {
                    ((unsigned short *)orow)[at] = (unsigned short)v;
                } else if (format == ALWAN_PIXEL_F32) {
                    ((float *)orow)[at] = v;
                } else {
                    double const d = ((double const *)irow)[at];
                    ((double *)orow)[at] = m ? (double)v : d;
                }
            }
        }
    }

done:
    for (c = 0; c < 4; c++) {
        if (planes[c]) ALWAN_FREE(planes[c]);
    }
    return st;
}
