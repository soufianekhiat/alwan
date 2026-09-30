/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ALWAN_DENOISE_DCT_OPENCV: opencv_contrib 5.0.0's xphoto::dctDenoising
 * (modules/xphoto/src/dct_image_denoising.cpp), after G. Yu and G. Sapiro, "DCT Image
 * Denoising: a Simple and Effective Image Denoising Algorithm", IPOL 2011, ported so that
 * the result is OpenCV's to the bit (suite 279, cv2 5.0.0 with IPP off). That file carries
 * this notice, kept here as its conditions ask:
 *
 *                           License Agreement
 *                For Open Source Computer Vision Library
 *
 *   Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
 *   Copyright (C) 2009-2011, Willow Garage Inc., all rights reserved.
 *   Third party copyrights are property of their respective owners.
 *
 *   Redistribution and use in source and binary forms, with or without modification,
 *   are permitted provided that the following conditions are met:
 *
 *     * Redistribution's of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *     * Redistribution's in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 *
 *     * The name of Intel Corporation may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *   This software is provided by the copyright holders and contributors "as is" and
 *   any express or implied warranties, including, but not limited to, the implied
 *   warranties of merchantability and fitness for a particular purpose are disclaimed.
 *   In no event shall the Intel Corporation or contributors be liable for any direct,
 *   indirect, incidental, special, exemplary, or consequential damages
 *   (including, but not limited to, procurement of substitute goods or services;
 *   loss of use, data, or profits; or business interruption) however caused
 *   and on any theory of liability, whether in contract, strict liability,
 *   or tort (including negligence or otherwise) arising in any way out of
 *   the use of this software, even if advised of the possibility of such damage.
 *
 * What OpenCV does, step for step:
 *   - the image converted to float (convertTo: 8-bit exact, doubles rounded to nearest);
 *   - three channels (BGR) turned by cv::transform with the float matrix
 *     (1, 1, 1) / sqrt3, (1, 0, -1) / sqrt2, (1, -2, 1) / sqrt6, whose entries are
 *     powf(3, -0.5) and so on; the AVX2 dispatch of transform_32f takes pixels two at a
 *     time, fma(v0, m0, fma(v1, m1, v2 m2)), while it has eight floats ahead, and the
 *     scalar tail (the last one or two pixels of the image) sums m0 v0 + m1 v1 + m2 v2 + 0
 *     unfused;
 *   - each channel alone: every psize x psize patch at x < width - psize, y < height -
 *     psize (one short of the last position, so the last row and column are in no patch),
 *     through cv::dct (the port in api/alwan_cv_dxt.c), coefficients with |c| > 3 sigma
 *     kept and the rest multiplied by 0, cv::idct; the estimates summed in patch order into
 *     a float image and a float count, then divided: 0 / 0 is NaN where no patch reached;
 *   - three channels merged and turned back by the float inverse of the matrix (Matx33f::inv:
 *     the determinant in float, its reciprocal, the cofactors in float);
 *   - converted back: 8 bits round half to even with saturation, NaN to 0.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stdlib.h>
#include <string.h>

/* powf(3, -0.5), powf(2, -0.5), powf(6, -0.5): each the correctly rounded float */
#define DD_S3 ((float)0.57735026918962576450914878050196)
#define DD_S2 ((float)0.70710678118654752440084436210485)
#define DD_S6 ((float)0.40824829046386301636621401245098)

/* cv::transform on a continuous float BGR image of len pixels, in place (OpenCV clones the
 * source for an in-place call) */
static void dd_transform(float *px, size_t len, float const m[9]) {
    size_t const total = len * 3;
    size_t x = 0;
    if (total >= 8) {
        for (; x <= total - 8; x += 6) {
            size_t k;
            for (k = 0; k < 6; k += 3) {
                float const v0 = px[x + k], v1 = px[x + k + 1], v2 = px[x + k + 2];
                float const t0 = ALWAN_FMA_CR_F32(v0, m[0], ALWAN_FMA_CR_F32(v1, m[1], ALWAN_FMA_CR_F32(v2, m[2], 0.0f)));
                float const t1 = ALWAN_FMA_CR_F32(v0, m[3], ALWAN_FMA_CR_F32(v1, m[4], ALWAN_FMA_CR_F32(v2, m[5], 0.0f)));
                float const t2 = ALWAN_FMA_CR_F32(v0, m[6], ALWAN_FMA_CR_F32(v1, m[7], ALWAN_FMA_CR_F32(v2, m[8], 0.0f)));
                px[x + k] = t0;
                px[x + k + 1] = t1;
                px[x + k + 2] = t2;
            }
        }
    }
    for (; x < total; x += 3) {
        float const v0 = px[x], v1 = px[x + 1], v2 = px[x + 2];
        float const t0 = m[0] * v0 + m[1] * v1 + m[2] * v2 + 0.0f;
        float const t1 = m[3] * v0 + m[4] * v1 + m[5] * v2 + 0.0f;
        float const t2 = m[6] * v0 + m[7] * v1 + m[8] * v2 + 0.0f;
        px[x] = t0;
        px[x + 1] = t1;
        px[x + 2] = t2;
    }
}

/* Matx33f::inv (Matx_FastInvOp<float, 3, 3>): row-major a to b */
static void dd_inv3(float const a[9], float b[9]) {
    float d = a[0] * (a[4] * a[8] - a[7] * a[5]) -
              a[1] * (a[3] * a[8] - a[6] * a[5]) +
              a[2] * (a[3] * a[7] - a[6] * a[4]);
    d = 1 / d;
    b[0] = (a[4] * a[8] - a[5] * a[7]) * d;
    b[1] = (a[2] * a[7] - a[1] * a[8]) * d;
    b[2] = (a[1] * a[5] - a[2] * a[4]) * d;

    b[3] = (a[5] * a[6] - a[3] * a[8]) * d;
    b[4] = (a[0] * a[8] - a[2] * a[6]) * d;
    b[5] = (a[2] * a[3] - a[0] * a[5]) * d;

    b[6] = (a[3] * a[7] - a[4] * a[6]) * d;
    b[7] = (a[1] * a[6] - a[0] * a[7]) * d;
    b[8] = (a[0] * a[4] - a[1] * a[3]) * d;
}

/* grayDctDenoising on one w x h plane, in place; res and num are w x h scratch, patch and
 * est psize x psize */
static void dd_gray(float *plane, size_t w, size_t h, double sigma, size_t ps, alwan__cv_dct_plan const *fwd,
                    alwan__cv_dct_plan const *inv, float *res, float *num, float *patch, float *est) {
    size_t const cw = w - ps;
    size_t const npixels = (h - ps) * cw;
    double const thresh = 3 * sigma;
    size_t k, i, j;

    memset(res, 0, w * h * sizeof(float));
    memset(num, 0, w * h * sizeof(float));
    for (k = 0; k < npixels; ++k) {
        size_t const y = k / cw, x = k % cw;
        for (i = 0; i < ps; i++) memcpy(patch + i * ps, plane + (y + i) * w + x, ps * sizeof(float));
        alwan__cv_dct2d(fwd, patch, ps, patch, ps);
        for (i = 0; i < ps * ps; ++i) {
            float const v = patch[i];
            float const a = v < 0.0f ? -v : v;
            patch[i] = v * ((double)a > thresh ? 1.0f : 0.0f);
        }
        alwan__cv_dct2d(inv, patch, ps, est, ps);
        for (i = 0; i < ps; i++) {
            float *r = res + (y + i) * w + x;
            float *c = num + (y + i) * w + x;
            for (j = 0; j < ps; j++) {
                r[j] = r[j] + est[i * ps + j];
                c[j] = c[j] + 1.0f;
            }
        }
    }
    for (i = 0; i < w * h; i++) plane[i] = res[i] / num[i];
}

/* convertTo(CV_8U) of a float: cvRound, ties to even, saturated; NaN, an infinity and
 * anything from 2^31 up is INT_MIN there, which saturates to 0 */
static unsigned char dd_to_u8(float v) {
    int i;
    float fr;
    if (!(v > -2147483648.0f) || !(v < 2147483648.0f)) return 0;
    if (v <= 0.5f) return 0;
    if (v >= 255.0f) return 255;
    i = (int)v;
    fr = v - (float)i;
    if (fr > 0.5f || (fr == 0.5f && (i & 1))) i++;
    return (unsigned char)i;
}

alwan_status alwan__dct_denoise_opencv(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch,
                                       size_t w, size_t h, double sigma, size_t ps, int kind) {
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t n, x, y, c;
    float *img = NULL, *planes = NULL, *res = NULL, *num = NULL, *patch = NULL, *est = NULL;
    alwan__cv_dct_plan *fwd = NULL, *inv = NULL;
    alwan_status st = ALWAN_E_NOMEM;
    float const mt[9] = { DD_S3, DD_S3, DD_S3, DD_S2, 0.0f, -DD_S2, DD_S6, -2.0f * DD_S6, DD_S6 };
    float mi[9];

    if (!out || !src) return ALWAN_E_INVALID;
    if (ch != 1 && ch != 3) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || w > 46340 || h > 46340) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if (!(sigma == sigma) || sigma < 0.0 || sigma > 1e30) return ALWAN_E_RANGE;
    if (ps < 2 || ps > 64 || (ps & 1u) || ps >= w || ps >= h) return ALWAN_E_RANGE;

    n = w * h;
    img = (float *)malloc(n * ch * sizeof(float));
    planes = (float *)malloc(n * ch * sizeof(float));
    res = (float *)malloc(n * sizeof(float));
    num = (float *)malloc(n * sizeof(float));
    patch = (float *)malloc(ps * ps * sizeof(float));
    est = (float *)malloc(ps * ps * sizeof(float));
    if (!img || !planes || !res || !num || !patch || !est) goto done;
    if (alwan__cv_dct_plan_create(&fwd, (int)ps, 0) != ALWAN_OK) goto done;
    if (alwan__cv_dct_plan_create(&inv, (int)ps, 1) != ALWAN_OK) goto done;

    /* convertTo(CV_32F), channels in OpenCV's B, G, R order */
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        float *d = img + y * w * ch;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                size_t const sc = ch == 3 ? 2 - c : c;
                float v;
                if (kind == 0) v = (float)((alwan_f64 const *)row)[x * ch + sc];
                else if (kind == 1) v = ((alwan_f32 const *)row)[x * ch + sc];
                else v = (float)((unsigned char const *)row)[x * ch + sc];
                d[x * ch + c] = v;
            }
        }
    }

    if (ch == 3) dd_transform(img, n, mt);
    for (c = 0; c < ch; c++)
        for (x = 0; x < n; x++) planes[c * n + x] = img[x * ch + c];
    for (c = 0; c < ch; c++) dd_gray(planes + c * n, w, h, sigma, ps, fwd, inv, res, num, patch, est);
    for (c = 0; c < ch; c++)
        for (x = 0; x < n; x++) img[x * ch + c] = planes[c * n + x];
    if (ch == 3) {
        dd_inv3(mt, mi);
        dd_transform(img, n, mi);
    }

    for (y = 0; y < h; y++) {
        char *row = (char *)out + y * out_rs;
        float const *s = img + y * w * ch;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                size_t const dc = ch == 3 ? 2 - c : c;
                float const v = s[x * ch + c];
                if (kind == 0) ((alwan_f64 *)row)[x * ch + dc] = (alwan_f64)v;
                else if (kind == 1) ((alwan_f32 *)row)[x * ch + dc] = v;
                else ((unsigned char *)row)[x * ch + dc] = dd_to_u8(v);
            }
        }
    }
    st = ALWAN_OK;

done:
    free(img);
    free(planes);
    free(res);
    free(num);
    free(patch);
    free(est);
    alwan__cv_dct_plan_destroy(fwd);
    alwan__cv_dct_plan_destroy(inv);
    return st;
}
