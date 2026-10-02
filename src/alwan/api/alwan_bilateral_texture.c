/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ALWAN_EDGE_FILTER_BILATERAL_TEXTURE: the bilateral texture filter (Cho, Lee, Kang and Lee,
 * "Bilateral Texture Filtering", SIGGRAPH 2014) as OpenCV's ximgproc::bilateralTextureFilter
 * computes it (opencv_contrib 5.0.0, modules/ximgproc/src/bilateral_texture_filter.cpp). This
 * file is a port of that code, kept to its float arithmetic and order of operations so the
 * result is OpenCV's value for value with IPP off:
 *
 *   - the box blur of cv::blur with BORDER_REFLECT: for windows up to 5 wide the row sums
 *     added afresh left to right (blockSum), wider ones a running sum (RowSum), both in
 *     double, then a running column sum in double scaled by 1 / (k k) and rounded to float;
 *   - the gradient magnitude of a forward difference each way (filter2D with [-1 1],
 *     BORDER_REFLECT), the window's maximum and minimum of the image (from 0 and from 1, as
 *     OpenCV starts them) and of the gradient, and the modified relative total variation
 *     maxG (2 fr + 1) / max(sumG, 1e-5) x (maxL - minL), averaged over three channels;
 *   - the guide: each pixel the blurred value at the window's least-mRTV pixel (replicated
 *     border, the first in scan order on a tie), blended with the blur by
 *     2 / (1 + exp(-sigma_alpha (mRTV - min mRTV))) - 1;
 *   - the joint bilateral filter on that guide over a 4 fr + 1 window, spatial weights
 *     exp(-(x^2 + y^2) / (2 (2 fr)^2)) and range weights exp(-d^2 / (2 sigma_avg^2)), d the
 *     guide difference (Euclidean over three channels), accumulated as OpenCV does: three
 *     channels through accumulateProduct, whose AVX2 blocks of 16 per row are fused
 *     multiply-adds;
 *
 * every exp the C runtime's expf, as cv::exp on float is in OpenCV's MSVC build. One or three
 * channels in 0..1 (OpenCV's 8-bit path divides by 255 first); a double call is computed in
 * float.
 *
 * The OpenCV code carries this notice:
 *
 *   License Agreement For Open Source Computer Vision Library
 *
 *   Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
 *   Copyright (C) 2009-2011, Willow Garage Inc., all rights reserved.
 *   Third party copyrights are property of their respective owners.
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
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

/* BORDER_REFLECT, fedcba | abcdef | fedcba */
static long btf_reflect(long p, long n) {
    if (n == 1) return 0;
    while (p < 0 || p >= n) {
        if (p < 0) p = -p - 1;
        if (p >= n) p = 2 * n - 1 - p;
    }
    return p;
}

static long btf_clamp(long p, long n) {
    return p < 0 ? 0 : p >= n ? n - 1 : p;
}

/* the element (y, x) of a w-wide plane, from the signed loop counters */
static size_t btf_at(long y, long x, size_t w) {
    return (size_t)y * w + (size_t)x;
}

static float btf_maxf(float a, float b) {
    return a > b ? a : b;
}

static float btf_minf(float a, float b) {
    return a < b ? a : b;
}

/* cv::blur(src, dst, (k, k), BORDER_REFLECT) on one plane of floats. rs and cs hold w and
 * h + k - 1 doubles. */
static void btf_blur(float *dst, float const *src, size_t w, size_t h, long fr, double *rows, double *sum) {
    long const k = 2 * fr + 1;
    double const scale = 1.0 / (double)(k * k);
    long r, x;
    size_t i;
    /* the k - 1 + h row sums, one row at a time, kept in a ring of k rows */
    for (i = 0; i < w; i++) sum[i] = 0.0;
    for (r = 0; r < (long)h + k - 1; r++) {
        float const *row = src + (size_t)btf_reflect(r - fr, (long)h) * w;
        double *rsum = rows + (size_t)(r % k) * w;
        if (k <= 5) { /* blockSum: each window added afresh */
            for (x = 0; x < (long)w; x++) {
                double s = (double)row[btf_reflect(x - fr, (long)w)];
                long j;
                for (j = 1; j < k; j++) s += (double)row[btf_reflect(x - fr + j, (long)w)];
                rsum[x] = s;
            }
        } else { /* RowSum: a running sum */
            double s = 0.0;
            long j;
            for (j = 0; j < k; j++) s += (double)row[btf_reflect(j - fr, (long)w)];
            rsum[0] = s;
            for (x = 1; x < (long)w; x++) {
                s += (double)row[btf_reflect(x - 1 + k - fr, (long)w)] - (double)row[btf_reflect(x - 1 - fr, (long)w)];
                rsum[x] = s;
            }
        }
        if (r < k - 1) {
            for (x = 0; x < (long)w; x++) sum[x] += rsum[x];
        } else {
            double const *oldest = rows + (size_t)((r - (k - 1)) % k) * w;
            float *drow = dst + (size_t)(r - (k - 1)) * w;
            for (x = 0; x < (long)w; x++) {
                double const s0 = sum[x] + rsum[x];
                drow[x] = (float)(s0 * scale);
                sum[x] = s0 - oldest[x];
            }
        }
    }
}

alwan_status alwan__btf_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t cn, size_t w, size_t h,
                            size_t fr_in, size_t iterations, double sigma_alpha, double sigma_avg, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const np = w * h;
    long const fr = (long)fr_in, fr2 = 2 * fr, kk = 2 * fr + 1, sw_side = 2 * fr2 + 1;
    float *I = 0, *B = 0, *L = 0, *G = 0, *Gt = 0, *mrtv = 0, *amin = 0, *gsel = 0, *alpha = 0, *J = 0, *sumw = 0, *dw = 0;
    float *SW = 0;
    double *rows = 0, *csum = 0;
    float coef;
    size_t it, c, i;
    long x, y;
    alwan_status st = ALWAN_OK;
    if (!out || !src || w == 0 || h == 0 || (cn != 1 && cn != 3)) return ALWAN_E_INVALID;
    if (src_rs / elem / cn < w || out_rs / elem / cn < w || np / w != h) return ALWAN_E_INVALID;
    if (fr_in == 0 || fr_in > 64 || iterations == 0 || iterations > 1000) return ALWAN_E_RANGE;
    if (!(sigma_alpha == sigma_alpha) || sigma_alpha < 0.0 || sigma_alpha > 1e30 || !(sigma_avg > 0.0) || sigma_avg > 1e30)
        return ALWAN_E_RANGE;
    I = (float *)ALWAN_ALLOC(alwan_safe_array_size(np * cn, sizeof(float)), 16);
    B = (float *)ALWAN_ALLOC(alwan_safe_array_size(np * cn, sizeof(float)), 16);
    L = (float *)ALWAN_ALLOC(alwan_safe_array_size(np, sizeof(float)), 16);
    G = (float *)ALWAN_ALLOC(alwan_safe_array_size(np, sizeof(float)), 16);
    Gt = (float *)ALWAN_ALLOC(alwan_safe_array_size(np * cn, sizeof(float)), 16);
    mrtv = (float *)ALWAN_ALLOC(alwan_safe_array_size(np, sizeof(float)), 16);
    amin = (float *)ALWAN_ALLOC(alwan_safe_array_size(np, sizeof(float)), 16);
    gsel = (float *)ALWAN_ALLOC(alwan_safe_array_size(np * cn, sizeof(float)), 16);
    alpha = (float *)ALWAN_ALLOC(alwan_safe_array_size(np, sizeof(float)), 16);
    J = (float *)ALWAN_ALLOC(alwan_safe_array_size(np * cn, sizeof(float)), 16);
    sumw = (float *)ALWAN_ALLOC(alwan_safe_array_size(np, sizeof(float)), 16);
    dw = (float *)ALWAN_ALLOC(alwan_safe_array_size(np, sizeof(float)), 16);
    SW = (float *)ALWAN_ALLOC(alwan_safe_array_size((size_t)(sw_side * sw_side), sizeof(float)), 16);
    rows = (double *)ALWAN_ALLOC(alwan_safe_array_size(alwan_safe_array_size(w, (size_t)kk), sizeof(double)), 16);
    csum = (double *)ALWAN_ALLOC(alwan_safe_array_size(w, sizeof(double)), 16);
    if (!I || !B || !L || !G || !Gt || !mrtv || !amin || !gsel || !alpha || !J || !sumw || !dw || !SW || !rows || !csum) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    for (y = 0; y < (long)h; y++) {
        unsigned char const *row = (unsigned char const *)src + (size_t)y * src_rs;
        for (x = 0; x < (long)(w * cn); x++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)row)[x] : ((alwan_f64 const *)row)[x];
            if (!(v == v) || v > FLT_MAX || v < -FLT_MAX) {
                st = ALWAN_E_INVALID;
                goto done;
            }
            I[(size_t)y * w * cn + (size_t)x] = (float)v;
        }
    }
    /* SW(r, c) = exp(-(x^2 + y^2) / (2 fr2^2)) in float */
    {
        float const den = (float)(2 * fr2 * fr2);
        long r2, c2;
        for (r2 = 0; r2 < sw_side; r2++) {
            float const yy = (float)(r2 - fr2);
            for (c2 = 0; c2 < sw_side; c2++) {
                float const xx = (float)(c2 - fr2);
                SW[r2 * sw_side + c2] = ALWAN_EXP_F32(-(xx * xx + yy * yy) / den);
            }
        }
    }
    coef = (float)(-0.5 / (sigma_avg * sigma_avg));
    for (it = 0; it < iterations; it++) {
        /* B: the box blur of each channel */
        for (c = 0; c < cn; c++) {
            for (i = 0; i < np; i++) L[i] = I[i * cn + c];
            btf_blur(G, L, w, h, fr, rows, csum);
            for (i = 0; i < np; i++) B[i * cn + c] = G[i];
        }
        /* mRTV */
        for (i = 0; i < np; i++) mrtv[i] = 0.0f;
        for (c = 0; c < cn; c++) {
            float const kscale = (float)kk;
            for (i = 0; i < np; i++) L[i] = I[i * cn + c];
            for (y = 0; y < (long)h; y++) {
                long const yd = btf_reflect(y + 1, (long)h);
                for (x = 0; x < (long)w; x++) {
                    float const v = L[(size_t)y * w + x];
                    float gx = L[(size_t)y * w + btf_reflect(x + 1, (long)w)] - v;
                    float gy = L[(size_t)yd * w + x] - v;
                    gx = gx * gx;
                    gy = gy * gy;
                    G[(size_t)y * w + x] = ALWAN_SQRT_F32(gx + gy);
                }
            }
            for (y = 0; y < (long)h; y++) {
                for (x = 0; x < (long)w; x++) {
                    float maxl = 0.0f, minl = 1.0f, maxg = 0.0f, sumg = 0.0f, mi;
                    long yy, xx;
                    for (yy = -fr; yy <= fr; yy++) {
                        long const sy = btf_reflect(y + yy, (long)h);
                        for (xx = -fr; xx <= fr; xx++) {
                            long const sx = btf_reflect(x + xx, (long)w);
                            float const tl = L[(size_t)sy * w + sx], tg = G[(size_t)sy * w + sx];
                            maxl = btf_maxf(maxl, tl);
                            minl = btf_minf(minl, tl);
                            maxg = btf_maxf(maxg, tg);
                            sumg = sumg + tg;
                        }
                    }
                    sumg = btf_maxf(sumg, 0.00001f);
                    mi = (maxg * kscale) / sumg;
                    mrtv[(size_t)y * w + x] = mrtv[(size_t)y * w + x] + mi * (maxl - minl);
                }
            }
        }
        if (cn == 3) {
            float const third = (float)(1.0 / 3.0);
            for (i = 0; i < np; i++) mrtv[i] = mrtv[i] * third;
        }
        /* compute_G: the blurred value at the window's least mRTV */
        for (i = 0; i < np * cn; i++) gsel[i] = B[i];
        for (i = 0; i < np; i++) amin[i] = 1.0f;
        {
            long oy, ox;
            for (oy = -fr; oy <= fr; oy++) {
                for (ox = -fr; ox <= fr; ox++) {
                    for (y = 0; y < (long)h; y++) {
                        long const ty = btf_clamp(y + oy, (long)h);
                        for (x = 0; x < (long)w; x++) {
                            long const tx = btf_clamp(x + ox, (long)w);
                            size_t const pb = btf_at(y, x, w), pt = btf_at(ty, tx, w);
                            if (amin[pb] > mrtv[pt]) {
                                amin[pb] = mrtv[pt];
                                for (c = 0; c < cn; c++) gsel[pb * cn + c] = B[pt * cn + c];
                            }
                        }
                    }
                }
            }
        }
        /* the blend weight */
        {
            float const sa = (float)sigma_alpha;
            for (i = 0; i < np; i++) {
                float const diff = mrtv[i] - amin[i];
                float a = -(diff * sa);
                a = ALWAN_EXP_F32(a);
                a = a + 1.0f;
                a = 1.0f / a;
                a = a * 2.0f - 1.0f;
                alpha[i] = a;
            }
            for (i = 0; i < np; i++) {
                float const ai = 1.0f - alpha[i];
                for (c = 0; c < cn; c++) {
                    float const g = gsel[i * cn + c] * alpha[i];
                    float const b = B[i * cn + c] * ai;
                    Gt[i * cn + c] = g + b;
                }
            }
        }
        /* the joint bilateral filter on Gt, x outer and y inner as OpenCV loops */
        for (i = 0; i < np; i++) sumw[i] = 0.0f;
        for (i = 0; i < np * cn; i++) J[i] = 0.0f;
        {
            long const xv = (long)w - (long)w % 16; /* accumulateProduct's AVX2 blocks */
            long ox, oy;
            for (ox = -fr2; ox <= fr2; ox++) {
                for (oy = -fr2; oy <= fr2; oy++) {
                    float const swv = SW[(fr2 + oy) * sw_side + (fr2 + ox)];
                    for (y = 0; y < (long)h; y++) {
                        long const sy = btf_reflect(y + oy, (long)h);
                        for (x = 0; x < (long)w; x++) {
                            long const sx = btf_reflect(x + ox, (long)w);
                            size_t const p0 = btf_at(y, x, w), ps = btf_at(sy, sx, w);
                            float d;
                            if (cn == 1) {
                                d = Gt[ps] - Gt[p0];
                                d = d * d;
                            } else {
                                float t0 = Gt[ps * 3 + 0] - Gt[p0 * 3 + 0];
                                float t1 = Gt[ps * 3 + 1] - Gt[p0 * 3 + 1];
                                float t2 = Gt[ps * 3 + 2] - Gt[p0 * 3 + 2];
                                t0 = t0 * t0;
                                t1 = t1 * t1;
                                t2 = t2 * t2;
                                d = 0.0f + t0;
                                d = d + t1;
                                d = d + t2;
                            }
                            d = ALWAN_EXP_F32(d * coef);
                            d = d * swv;
                            sumw[p0] = sumw[p0] + d;
                            if (cn == 1) {
                                float const prod = d * I[ps];
                                J[p0] = J[p0] + prod;
                            } else {
                                for (c = 0; c < 3; c++) {
                                    if (x < xv) J[p0 * 3 + c] = ALWAN_FMA_CR_F32(d, I[ps * 3 + c], J[p0 * 3 + c]);
                                    else J[p0 * 3 + c] = d * I[ps * 3 + c] + J[p0 * 3 + c];
                                }
                            }
                        }
                    }
                }
            }
            for (i = 0; i < np; i++) {
                float const s = btf_maxf(0.00001f, sumw[i]);
                for (c = 0; c < cn; c++) J[i * cn + c] = J[i * cn + c] / s;
            }
        }
        for (i = 0; i < np * cn; i++) I[i] = J[i];
    }
    for (y = 0; y < (long)h; y++) {
        unsigned char *row = (unsigned char *)out + (size_t)y * out_rs;
        for (x = 0; x < (long)(w * cn); x++) {
            float const v = I[(size_t)y * w * cn + (size_t)x];
            if (is_f32) ((alwan_f32 *)row)[x] = v;
            else ((alwan_f64 *)row)[x] = (alwan_f64)v;
        }
    }
done:
    if (I) ALWAN_FREE(I);
    if (B) ALWAN_FREE(B);
    if (L) ALWAN_FREE(L);
    if (G) ALWAN_FREE(G);
    if (Gt) ALWAN_FREE(Gt);
    if (mrtv) ALWAN_FREE(mrtv);
    if (amin) ALWAN_FREE(amin);
    if (gsel) ALWAN_FREE(gsel);
    if (alpha) ALWAN_FREE(alpha);
    if (J) ALWAN_FREE(J);
    if (sumw) ALWAN_FREE(sumw);
    if (dw) ALWAN_FREE(dw);
    if (SW) ALWAN_FREE(SW);
    if (rows) ALWAN_FREE(rows);
    if (csum) ALWAN_FREE(csum);
    return st;
}
