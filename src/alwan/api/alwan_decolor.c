/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Contrast-preserving decolorization.
 *
 * ALWAN_DECOLOR_LU2012 is a port of cv::decolor from OpenCV 5.0.0
 * (modules/photo/src/contrast_preserve.cpp and .hpp), after C. Lu, L. Xu and J. Jia,
 * "Contrast Preserving Decolorization", ICCP 2012. OpenCV is Copyright the OpenCV
 * authors, Apache License 2.0 (https://github.com/opencv/opencv); its notice is kept here
 * and the licence travels with the table this file reads, data/opencv/LICENSE-opencv.txt.
 * The OpenCV calls it makes are reproduced from the same sources: the float sRGB to Lab
 * conversion (color_lab.cpp: clip, 14-bit round, trilinear interpolation of an int16 table
 * on a 33 x 33 x 33 grid, which gendata/data/decolor_lab_lut.py reads back from cv2),
 * filter2D with the 2-tap difference kernels, the float INTER_LINEAR resize,
 * mulTransposed (MulTransposedL, double sums four products at a time), solve on a square
 * float system (LUImpl, partial pivoting; a pivot under 10 FLT_EPSILON fails it and
 * cv::solve then zeroes the result, so the weights go to 0 and the grey to 0 everywhere)
 * and the final min-max normalisation.
 *
 * The method, at OpenCV's defaults:
 *   - the image, at most 800 in height plus width (resized down by the float bilinear
 *     resize when larger), gives a gradient system: the horizontal and vertical forward
 *     differences of the nine monomials R^r G^g B^b with 1 <= r + g + b <= 2, and the
 *     colour contrast target, the CIELAB difference norm / 100 of the same neighbours;
 *   - a weak order sign per gradient: +1 when R, G and B all rise by more than 0.05, -1
 *     when all fall by more, 0 otherwise;
 *   - the weights start at 0.33 on R, G and B, and are refit by the paper's
 *     expectation-maximisation step, sigma 0.02, until the energy moves by under 1e-4 or
 *     after the sixteenth update;
 *   - the grey is the weighted polynomial on the full image, stretched to [0, 1].
 * cv::decolor then rounds to 8 bits (cvRound(grey * 255)); alwan returns the grey before
 * that, so the 8-bit image is that rounding of the single-precision result.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* OpenCV 5.0.0's RGB2LabLUT_s16 nodes: [blue][green][red][L, a, b]. */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static short const dcl_lab_lut[33 * 33 * 33 * 3] = {
#include "../data/opencv/rgb2lab_lut_s16.csv"
};
ALWAN_DIAG_POP

#define DCL_NCOMB 9

/* The monomials in OpenCV's order: red outermost, then green, then blue. */
static int const dcl_comb[DCL_NCOMB][3] = {
    {0, 0, 1}, {0, 0, 2}, {0, 1, 0}, {0, 1, 1}, {0, 2, 0},
    {1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {2, 0, 0}
};

/* cvRound: to nearest, ties to even. */
static int dcl_round(double v) {
    double r = ALWAN_FLOOR_F64(v + 0.5);
    if (r - v == 0.5 && ALWAN_FMOD_F64(r, 2.0) != 0.0) r -= 1.0;
    if (!(r > (double)INT_MIN && r < (double)INT_MAX)) return 0;
    return (int)r;
}

/* cvFloor, guarded: out of the int range (not reachable from a resize coordinate) is 0. */
static int dcl_floor(double v) {
    double const r = ALWAN_FLOOR_F64(v);
    if (!(r > (double)INT_MIN && r < (double)INT_MAX)) return 0;
    return (int)r;
}

/* std::pow(float, int) for the exponents 0, 1 and 2: exact in double. */
static double dcl_pow(float x, int e) {
    double d = (double)x;
    return e == 0 ? 1.0 : (e == 1 ? d : d * d);
}

/* ------------------------------------------------------------------------------------ */
/* cv::resize, INTER_LINEAR, float, non-IPP: separable, unfused multiply-adds.          */
/* ------------------------------------------------------------------------------------ */

static int dcl_resize_linear(float *dst, int dw, int dh, float const *src, int sw, int sh) {
    double const scale_x = 1.0 / ((double)dw / (double)sw);
    double const scale_y = 1.0 / ((double)dh / (double)sh);
    int *xofs = (int *)malloc((size_t)dw * sizeof(int));
    float *alpha = (float *)malloc((size_t)dw * 2 * sizeof(float));
    float *hbuf = (float *)malloc((size_t)sh * (size_t)dw * 3 * sizeof(float));
    int xmax = dw, dx, dy, y, c;
    if (!xofs || !alpha || !hbuf) {
        free(xofs); free(alpha); free(hbuf);
        return 0;
    }
    for (dx = 0; dx < dw; dx++) {
        float fx = (float)(((double)dx + 0.5) * scale_x - 0.5);
        int sx = dcl_floor((double)fx);
        fx -= (float)sx;
        if (sx < 0) { fx = 0.0f; sx = 0; }
        if (sx + 1 >= sw) {
            if (dx < xmax) xmax = dx;
            if (sx >= sw - 1) { fx = 0.0f; sx = sw - 1; }
        }
        xofs[dx] = sx;
        alpha[2 * dx] = 1.0f - fx;
        alpha[2 * dx + 1] = fx;
    }
    for (y = 0; y < sh; y++) {
        float const *s = src + (size_t)y * (size_t)sw * 3;
        float *d = hbuf + (size_t)y * (size_t)dw * 3;
        for (dx = 0; dx < dw; dx++) {
            int sx = xofs[dx];
            for (c = 0; c < 3; c++) {
                if (dx < xmax) {
                    float t0 = s[sx * 3 + c] * alpha[2 * dx];
                    float t1 = s[(sx + 1) * 3 + c] * alpha[2 * dx + 1];
                    d[dx * 3 + c] = t0 + t1;
                } else {
                    d[dx * 3 + c] = s[sx * 3 + c];
                }
            }
        }
    }
    for (dy = 0; dy < dh; dy++) {
        float fy = (float)(((double)dy + 0.5) * scale_y - 0.5);
        int sy = dcl_floor((double)fy);
        int r0, r1;
        float b0, b1;
        size_t i;
        fy -= (float)sy;
        b0 = 1.0f - fy;
        b1 = fy;
        r0 = sy < 0 ? 0 : (sy > sh - 1 ? sh - 1 : sy);
        r1 = sy + 1 < 0 ? 0 : (sy + 1 > sh - 1 ? sh - 1 : sy + 1);
        for (i = 0; i < (size_t)dw * 3; i++) {
            float t0 = hbuf[(size_t)r0 * (size_t)dw * 3 + i] * b0;
            float t1 = hbuf[(size_t)r1 * (size_t)dw * 3 + i] * b1;
            dst[(size_t)dy * (size_t)dw * 3 + i] = t0 + t1;
        }
    }
    free(xofs); free(alpha); free(hbuf);
    return 1;
}

/* ------------------------------------------------------------------------------------ */
/* cvtColor(COLOR_BGR2Lab) on float: clip, 14-bit round, trilinear on the int16 table.   */
/* ------------------------------------------------------------------------------------ */

static void dcl_rgb_to_lab(float *lab, float r, float g, float b) {
    float const cr = r < 0.0f ? 0.0f : (r <= 1.0f ? r : 1.0f);
    float const cg = g < 0.0f ? 0.0f : (g <= 1.0f ? g : 1.0f);
    float const cb = b < 0.0f ? 0.0f : (b <= 1.0f ? b : 1.0f);
    int const cx = dcl_round((double)(cr * 16384.0f));
    int const cy = dcl_round((double)(cg * 16384.0f));
    int const cz = dcl_round((double)(cb * 16384.0f));
    int const tx = cx >> 9, ty = cy >> 9, tz = cz >> 9;
    int const x = (cx >> 5) & 15, y = (cy >> 5) & 15, z = (cz >> 5) & 15;
    int acc[3] = {0, 0, 0};
    int p, q, s, k;
    for (p = 0; p < 2; p++) {
        int const ix = tx + p > 32 ? 32 : tx + p;
        int const wx = p ? x : 16 - x;
        for (q = 0; q < 2; q++) {
            int const iy = ty + q > 32 ? 32 : ty + q;
            int const wy = q ? y : 16 - y;
            for (s = 0; s < 2; s++) {
                int const iz = tz + s > 32 ? 32 : tz + s;
                int const w = wx * wy * (s ? z : 16 - z);
                short const *node = dcl_lab_lut + (((size_t)iz * 33 + (size_t)iy) * 33 + (size_t)ix) * 3;
                for (k = 0; k < 3; k++) acc[k] += (int)node[k] * w;
            }
        }
    }
    {
        float const inv = 1.0f / 16384.0f;
        float const l = (float)((acc[0] + 2048) >> 12) * inv;
        float const a = (float)((acc[1] + 2048) >> 12) * inv;
        float const bb = (float)((acc[2] + 2048) >> 12) * inv;
        float const a256 = a * 256.0f;
        float const b256 = bb * 256.0f;
        lab[0] = l * 100.0f;
        lab[1] = a256 - 128.0f;
        lab[2] = b256 - 128.0f;
    }
}

void alwan__cv_rgb_to_lab_f32(float *lab, float r, float g, float b) {
    dcl_rgb_to_lab(lab, r, g, b);
}

/* ------------------------------------------------------------------------------------ */
/* Decolor::gradvector: forward differences, the last column / row 0, stored with x      */
/* outermost (OpenCV reads the transposed matrices): grad[x*h + y], then the vertical.   */
/* ------------------------------------------------------------------------------------ */

static void dcl_gradvector(double *grad, float const *ch, size_t stride, size_t w, size_t h) {
    size_t x, y;
    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            float const v = ch[(y * w + x) * stride];
            float gx = 0.0f, gy = 0.0f;
            if (x + 1 < w) gx = v - ch[(y * w + x + 1) * stride];
            if (y + 1 < h) gy = v - ch[((y + 1) * w + x) * stride];
            grad[x * h + y] = (double)gx;
            grad[w * h + x * h + y] = (double)gy;
        }
    }
}

/* LUImpl<float>: A is m x m, b is m x n; 0 when a pivot is under eps (cv::solve then
 * zeroes b). */
static int dcl_lu(float *A, int m, float *b, size_t n) {
    float const eps = FLT_EPSILON * 10.0f;
    int i, j, k;
    size_t c;
    for (i = 0; i < m; i++) {
        float d;
        k = i;
        for (j = i + 1; j < m; j++)
            if (ALWAN_ABS_F32(A[j * m + i]) > ALWAN_ABS_F32(A[k * m + i])) k = j;
        if (ALWAN_ABS_F32(A[k * m + i]) < eps) return 0;
        if (k != i) {
            for (j = i; j < m; j++) { float t = A[i * m + j]; A[i * m + j] = A[k * m + j]; A[k * m + j] = t; }
            for (c = 0; c < n; c++) { float t = b[(size_t)i * n + c]; b[(size_t)i * n + c] = b[(size_t)k * n + c]; b[(size_t)k * n + c] = t; }
        }
        d = -1.0f / A[i * m + i];
        for (j = i + 1; j < m; j++) {
            float const alpha = A[j * m + i] * d;
            for (k = i + 1; k < m; k++) {
                float const t = alpha * A[i * m + k];
                A[j * m + k] += t;
            }
            for (c = 0; c < n; c++) {
                float const t = alpha * b[(size_t)i * n + c];
                b[(size_t)j * n + c] += t;
            }
        }
    }
    for (i = m - 1; i >= 0; i--) {
        for (c = 0; c < n; c++) {
            float s = b[(size_t)i * n + c];
            for (k = i + 1; k < m; k++) {
                float const t = A[i * m + k] * b[(size_t)k * n + c];
                s -= t;
            }
            b[(size_t)i * n + c] = s / A[i * m + i];
        }
    }
    return 1;
}

/* The whole method on a packed float RGB image; grey is w x h. */
static alwan_status dcl_lu2012(float *grey, float const *img, int w, int h, alwan_decolor_params const *pr) {
    float const sigma = pr && pr->sigma > 0.0 ? (float)pr->sigma : 0.02f;  /* Decolor::sigma is a float */
    double const sigma_d = (double)sigma;
    int const max_iter = pr && pr->max_iterations > 0 ? pr->max_iterations : 15;
    double const tol = pr && pr->tolerance > 0.0 ? pr->tolerance : 0.0001;
    int const working = pr && pr->working_size > 0 ? pr->working_size : 800;
    float *small = NULL;
    float const *sm = img;
    int sw = w, sh = h;
    size_t n, npx;
    double *cg = NULL, *poly = NULL, *alf = NULL, *tmp = NULL, *expterm = NULL;
    float *lab = NULL, *cur = NULL, *mt = NULL;
    float A[DCL_NCOMB * DCL_NCOMB];
    double wei[DCL_NCOMB];
    alwan_status st = ALWAN_E_NOMEM;
    size_t i, j;
    int k;

    if (w + h > working) {
        double const f = (double)working / (double)(h + w);
        int const dw = dcl_round((double)w * f), dh = dcl_round((double)h * f);
        if (dw < 1 || dh < 1) return ALWAN_E_INVALID;
        if (dw != w || dh != h) {
            small = (float *)malloc((size_t)dw * (size_t)dh * 3 * sizeof(float));
            if (!small || !dcl_resize_linear(small, dw, dh, img, w, h)) goto done;
            sm = small;
            sw = dw;
            sh = dh;
        }
    }
    npx = (size_t)sw * (size_t)sh;
    n = 2 * npx;
    cg = (double *)malloc(n * sizeof(double));
    alf = (double *)malloc(n * sizeof(double));
    tmp = (double *)malloc(3 * n * sizeof(double));
    poly = (double *)malloc((size_t)DCL_NCOMB * n * sizeof(double));
    lab = (float *)malloc(npx * 3 * sizeof(float));
    cur = (float *)malloc(npx * sizeof(float));
    mt = (float *)malloc((size_t)DCL_NCOMB * n * sizeof(float));
    expterm = (double *)malloc(n * sizeof(double));
    if (!cg || !alf || !tmp || !poly || !lab || !cur || !mt || !expterm) goto done;

    /* colorGrad: the CIELAB difference norm / 100 */
    for (i = 0; i < npx; i++) dcl_rgb_to_lab(lab + 3 * i, sm[3 * i], sm[3 * i + 1], sm[3 * i + 2]);
    for (k = 0; k < 3; k++) dcl_gradvector(tmp + (size_t)k * n, lab + k, 3, (size_t)sw, (size_t)sh);
    for (i = 0; i < n; i++) {
        double const l = tmp[i], a = tmp[n + i], b = tmp[2 * n + i];
        cg[i] = ALWAN_SQRT_F64(l * l + a * a + b * b) / 100.0;
    }

    /* weak_order: all three channels rise (fall) by more than 0.05 */
    for (k = 0; k < 3; k++) dcl_gradvector(tmp + (size_t)k * n, sm + k, 3, (size_t)sw, (size_t)sh);
    for (i = 0; i < n; i++) {
        double const r = tmp[i], g = tmp[n + i], b = tmp[2 * n + i];
        double const up = (r > 0.05 ? 1.0 : 0.0) * (g > 0.05 ? 1.0 : 0.0) * (b > 0.05 ? 1.0 : 0.0);
        double const dn = (r < -0.05 ? 1.0 : 0.0) * (g < -0.05 ? 1.0 : 0.0) * (b < -0.05 ? 1.0 : 0.0);
        alf[i] = up - dn;
    }

    /* grad_system: the monomials' gradients */
    for (k = 0; k < DCL_NCOMB; k++) {
        for (i = 0; i < npx; i++) {
            double const v = dcl_pow(sm[3 * i], dcl_comb[k][0]) * dcl_pow(sm[3 * i + 1], dcl_comb[k][1]);
            cur[i] = (float)(v * dcl_pow(sm[3 * i + 2], dcl_comb[k][2]));
        }
        dcl_gradvector(poly + (size_t)k * n, cur, 1, (size_t)sw, (size_t)sh);
    }

    /* wei_update_matrix: A = P P^T (MulTransposedL), B = P .* Cg, Mt = A \ B */
    for (k = 0; k < DCL_NCOMB; k++) {
        int l;
        for (l = k; l < DCL_NCOMB; l++) {
            double s = 0.0;
            double const *p1 = poly + (size_t)k * n, *p2 = poly + (size_t)l * n;
            size_t q = 0;
            for (; q + 4 <= n; q += 4) {
                double const t = (double)(float)p1[q] * (double)(float)p2[q]
                               + (double)(float)p1[q + 1] * (double)(float)p2[q + 1]
                               + (double)(float)p1[q + 2] * (double)(float)p2[q + 2]
                               + (double)(float)p1[q + 3] * (double)(float)p2[q + 3];
                s += t;
            }
            for (; q < n; q++) s += (double)(float)p1[q] * (double)(float)p2[q];
            A[k * DCL_NCOMB + l] = (float)s;
            A[l * DCL_NCOMB + k] = (float)s;
        }
    }
    for (k = 0; k < DCL_NCOMB; k++)
        for (i = 0; i < n; i++) mt[(size_t)k * n + i] = (float)(poly[(size_t)k * n + i] * cg[i]);
    if (!dcl_lu(A, DCL_NCOMB, mt, n)) memset(mt, 0, (size_t)DCL_NCOMB * n * sizeof(float));

    /* wei_inti: 0.33 on the three linear terms */
    for (k = 0; k < DCL_NCOMB; k++) {
        int const deg = dcl_comb[k][0] + dcl_comb[k][1] + dcl_comb[k][2];
        wei[k] = deg == 1 ? 0.33 : 0.0;
    }

    /* the EM iterations */
    {
        double const sq_sigma = (double)(sigma * sigma);
        double e = 0.0, pre = DBL_MAX;   /* OpenCV starts it at infinity */
        int iter = 0;
        while (ALWAN_ABS_F64(e - pre) > tol) {
            double wei1[DCL_NCOMB];
            double sum;
            iter++;
            pre = e;
            for (i = 0; i < n; i++) {
                double val = 0.0, t, t1, pos, neg, es;
                for (k = 0; k < DCL_NCOMB; k++) val = val + poly[(size_t)k * n + i] * wei[k];
                t = val - cg[i];
                t1 = val + cg[i];
                pos = ((1.0 + alf[i]) / 2.0) * ALWAN_EXP_F64(-0.5 * (t * t) / sq_sigma);
                neg = ((1.0 - alf[i]) / 2.0) * ALWAN_EXP_F64(-0.5 * (t1 * t1) / sq_sigma);
                es = pos + neg;
                expterm[i] = (pos - neg) / (es + (es == 0.0 ? 1.0 : 0.0));
            }
            for (k = 0; k < DCL_NCOMB; k++) {
                double v = 0.0;
                for (i = 0; i < n; i++) v = v + (double)mt[(size_t)k * n + i] * expterm[i];
                wei1[k] = v;
            }
            for (k = 0; k < DCL_NCOMB; k++) wei[k] = wei1[k];
            /* energyCalcu */
            sum = 0.0;
            for (i = 0; i < n; i++) {
                double val = 0.0, t, t1;
                for (k = 0; k < DCL_NCOMB; k++) val = val + poly[(size_t)k * n + i] * wei[k];
                t = val - cg[i];
                t1 = val + cg[i];
                sum += -1.0 * ALWAN_LN_F64(ALWAN_EXP_F64(-1.0 * (t * t) / sigma_d)
                                           + ALWAN_EXP_F64(-1.0 * (t1 * t1) / sigma_d));
            }
            e = sum / (double)n;
            if (iter > max_iter) break;
        }
    }

    /* grayImContruct on the full image, then min-max to [0, 1] */
    {
        size_t const total = (size_t)w * (size_t)h;
        double mn = DBL_MAX, mx = -DBL_MAX;
        memset(grey, 0, total * sizeof(float));
        for (k = 0; k < DCL_NCOMB; k++) {
            double const wk = (double)(float)wei[k];
            for (i = 0; i < total; i++) {
                double const term = ((wk * dcl_pow(img[3 * i], dcl_comb[k][0])) * dcl_pow(img[3 * i + 1], dcl_comb[k][1]))
                                  * dcl_pow(img[3 * i + 2], dcl_comb[k][2]);
                grey[i] = (float)((double)grey[i] + term);
            }
        }
        for (i = 0; i < total; i++) {
            double const v = (double)grey[i];
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        if (!(mx > mn)) {
            /* a flat or undefined grey: OpenCV divides by zero and its 8-bit cast reads 0 */
            memset(grey, 0, total * sizeof(float));
        } else {
            float const scale = (float)(1.0 / (mx - mn));
            for (j = 0; j < total; j++) {
                float const g = (float)((double)grey[j] - mn);
                grey[j] = g * scale;
            }
        }
    }
    st = ALWAN_OK;

done:
    free(small); free(cg); free(alf); free(tmp); free(poly); free(lab); free(cur); free(mt); free(expterm);
    return st;
}

static alwan_status dcl_check(size_t channels, size_t width, size_t height, size_t in_stride, size_t in_elem,
                              size_t out_stride, size_t out_elem, alwan_decolor_method method,
                              alwan_decolor_params const *params) {
    if (method != ALWAN_DECOLOR_LU2012) return ALWAN_E_INVALID;
    if (channels != 3 && channels != 4) return ALWAN_E_INVALID;
    if (width == 0 || height == 0 || width > (size_t)INT_MAX / 4 || height > (size_t)INT_MAX / 4) return ALWAN_E_INVALID;
    if (in_stride < width * channels * in_elem || out_stride < width * out_elem) return ALWAN_E_INVALID;
    if (params && (params->sigma < 0.0 || params->tolerance < 0.0 || params->max_iterations < 0 ||
                   params->working_size < 0 || !(params->sigma == params->sigma) ||
                   !(params->tolerance == params->tolerance)))
        return ALWAN_E_INVALID;
    return ALWAN_OK;
}

alwan_status alwan_decolor_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                               size_t channels, size_t width, size_t height, alwan_decolor_method method,
                               alwan_decolor_params const *params) {
    float *img;
    float *grey;
    alwan_status st;
    size_t x, y;
    if (!out || !src) return ALWAN_E_INVALID;
    st = dcl_check(channels, width, height, src_row_stride, sizeof(alwan_f32), out_row_stride, sizeof(alwan_f32), method, params);
    if (st != ALWAN_OK) return st;
    img = (float *)malloc(width * height * 3 * sizeof(float));
    grey = (float *)malloc(width * height * sizeof(float));
    if (!img || !grey) { free(img); free(grey); return ALWAN_E_NOMEM; }
    for (y = 0; y < height; y++) {
        alwan_f32 const *row = (alwan_f32 const *)((char const *)src + y * src_row_stride);
        for (x = 0; x < width; x++) {
            size_t const o = (y * width + x) * 3;
            img[o] = row[x * channels];
            img[o + 1] = row[x * channels + 1];
            img[o + 2] = row[x * channels + 2];
        }
    }
    st = dcl_lu2012(grey, img, (int)width, (int)height, params);
    if (st == ALWAN_OK) {
        for (y = 0; y < height; y++) {
            alwan_f32 *row = (alwan_f32 *)((char *)out + y * out_row_stride);
            for (x = 0; x < width; x++) row[x] = grey[y * width + x];
        }
    }
    free(img); free(grey);
    return st;
}

alwan_status alwan_decolor_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                               size_t channels, size_t width, size_t height, alwan_decolor_method method,
                               alwan_decolor_params const *params) {
    float *img;
    float *grey;
    alwan_status st;
    size_t x, y;
    if (!out || !src) return ALWAN_E_INVALID;
    st = dcl_check(channels, width, height, src_row_stride, sizeof(alwan_f64), out_row_stride, sizeof(alwan_f64), method, params);
    if (st != ALWAN_OK) return st;
    img = (float *)malloc(width * height * 3 * sizeof(float));
    grey = (float *)malloc(width * height * sizeof(float));
    if (!img || !grey) { free(img); free(grey); return ALWAN_E_NOMEM; }
    for (y = 0; y < height; y++) {
        alwan_f64 const *row = (alwan_f64 const *)((char const *)src + y * src_row_stride);
        for (x = 0; x < width; x++) {
            size_t const o = (y * width + x) * 3;
            img[o] = (float)row[x * channels];
            img[o + 1] = (float)row[x * channels + 1];
            img[o + 2] = (float)row[x * channels + 2];
        }
    }
    st = dcl_lu2012(grey, img, (int)width, (int)height, params);
    if (st == ALWAN_OK) {
        for (y = 0; y < height; y++) {
            alwan_f64 *row = (alwan_f64 *)((char *)out + y * out_row_stride);
            for (x = 0; x < width; x++) row[x] = (alwan_f64)grey[y * width + x];
        }
    }
    free(img); free(grey);
    return st;
}
