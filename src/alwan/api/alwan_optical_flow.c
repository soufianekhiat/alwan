/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Image alignment by enhanced correlation (ALWAN_REGISTER_ECC, behind alwan_register) and
 * dense optical flow (alwan_optical_flow): ports of three implementations, each kept to its
 * arithmetic so that suite 300 can hold it to the original.
 *
 * ECC, OpenCV 5.0.0's findTransformECCWithMask (video/src/ecc.cpp), with the OpenCV pieces it
 * calls on x86-64 with AVX2 (the cv2 wheels' dispatch on this hardware):
 *   - GaussianBlur on float: getGaussianKernel (the fixed 3/5/7/9 kernels when sigma is 0), a
 *     separable filter with BORDER_REFLECT_101 whose vector blocks (8 columns, the column
 *     filter 16 at a time for kernels of 5 and more) fuse their multiply-adds and whose
 *     scalar tails do not (filter.simd.hpp);
 *   - filter2D with [-0.5, 0, 0.5], where every order of the two halvings rounds alike;
 *   - the float bilinear and nearest warpAffine / warpPerspective of warp_kernels.simd.hpp:
 *     the columns below the width rounded down to 16 take the vector kernel (coordinates and
 *     interpolation through fused multiply-adds), the rest the scalar one;
 *   - Mat::dot on float (dotProd_32f: 8 float lanes of fused multiply-adds over blocks of
 *     8192, four accumulators unrolled, each block's lanes summed and added in double, a
 *     scalar tail in double), row by row where one side is a column range;
 *   - Mat::inv: the closed forms in double for 2 and 3 parameters, LUImpl in float for 6
 *     and 8; the 6 x 1 products of hessianInv in double;
 *   - addWeighted on float as a vector fma (the error image), meanStdDev in double.
 * The arcsine of the Euclidean update is atan2(s, sqrt((1 - s)(1 + s))), not the C runtime's
 * asin, an ulp of the angle at most.
 *
 * FARNEBACK, OpenCV 5.0.0's calcOpticalFlowFarneback (video/src/optflowgf.cpp): the polynomial
 * expansion with its Gaussian moments and the 6 x 6 Cholesky inverse in double, the matrices
 * and the box or Gaussian averaging of FarnebackUpdateFlow_*, the pyramid sizes, GaussianBlur
 * (above) and resize INTER_LINEAR (alwan__cv_resize_f32_plane, the fast area path at a factor
 * of exactly 2). That file builds at OpenCV's SSE3 baseline, so its vector multiply-adds are
 * not fused and its scalar form is its exact form.
 *
 * TVL1 and ILK, scikit-image 0.26's optical_flow_tvl1 and optical_flow_ilk with
 * _coarse_to_fine (registration/_optical_flow.py, _optical_flow_utils.py): the pyramid of
 * pyramid_reduce (scipy's gaussian_filter, sigma 2/3, mode reflect, then resize: scipy's zoom,
 * grid_mode, order 1, mode mirror, clipped to the range), the flow resized by scipy's
 * order-0 zoom and scaled, warp(mode='edge') as scipy's map_coordinates (order 1, nearest,
 * weights 1 - t and 1 - (1 - t)), numpy's gradient, the median prefilter (scipy, mode
 * reflect), and for ILK scipy's uniform_filter (its running sum) or gaussian_filter
 * (alwan_filter's port), numpy's det through the logarithm and a 2 x 2 solve with partial
 * pivoting. Each operation is computed in double and rounded to float when the dtype is
 * float32, which rounds as float arithmetic would. The 2 x 2 solve is LAPACK's in its
 * structure, not OpenBLAS's kernels to the bit.
 *
 * The ported code carries these notices.
 *
 * ecc.cpp:
 *
 *   Intel License Agreement For Open Source Computer Vision Library
 *
 *   Copyright (C) 2000, Intel Corporation, all rights reserved.
 *   Third party copyrights are property of their respective owners.
 *
 *   Redistribution and use in source and binary forms, with or without modification,
 *   are permitted provided that the following conditions are met:
 *     * Redistribution's of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistribution's in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
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
 * optflowgf.cpp:
 *
 *   License Agreement For Open Source Computer Vision Library
 *
 *   Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
 *   Copyright (C) 2009, Willow Garage Inc., all rights reserved.
 *   Third party copyrights are property of their respective owners.
 *
 *   Redistribution and use in source and binary forms, with or without modification,
 *   are permitted provided that the following conditions are met:
 *     * Redistribution's of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistribution's in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 *     * The name of the copyright holders may not be used to endorse or promote products
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
 * scikit-image (registration/_optical_flow.py, _optical_flow_utils.py):
 *
 *   Copyright (C) 2019, the scikit-image team. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without modification, are
 *   permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright notice, this list
 *       of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright notice, this
 *       list of conditions and the following disclaimer in the documentation and/or other
 *       materials provided with the distribution.
 *    3. Neither the name of skimage nor the names of its contributors may be used to
 *       endorse or promote products derived from this software without specific prior
 *       written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *   AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 *   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

/* ==================================================================================== */
/* Small helpers                                                                         */
/* ==================================================================================== */

static int of_floor(double v) {
    double const r = ALWAN_FLOOR_F64(v);
    if (!(r > (double)INT_MIN && r < (double)INT_MAX)) return INT_MIN;
    return (int)r;
}

/* cvRound: to nearest, ties to even */
static int of_round(double v) {
    double r = ALWAN_FLOOR_F64(v + 0.5);
    if (r - v == 0.5 && ALWAN_FMOD_F64(r, 2.0) != 0.0) r -= 1.0;
    if (!(r > (double)INT_MIN && r < (double)INT_MAX)) return INT_MIN;
    return (int)r;
}

/* borderInterpolate with BORDER_REFLECT_101 */
static int of_reflect101(int p, int len) {
    if (len == 1) return 0;
    while ((unsigned)p >= (unsigned)len) {
        if (p < 0) p = -p;
        else p = len - 1 - (p - len) - 1;
    }
    return p;
}

static void *of_alloc(size_t count, size_t size) {
    size_t const bytes = alwan_safe_array_size(count, size);
    if (bytes == 0) return NULL;
    return ALWAN_ALLOC(bytes, 64);
}

/* ==================================================================================== */
/* OpenCV 5.0.0: GaussianBlur on float, BORDER_REFLECT_101                                */
/* ==================================================================================== */

/* getGaussianKernel(n, sigma, CV_32F): the fixed kernels for sigma <= 0 and n <= 9, else
 * exp(-x^2 / (2 sigma^2)) normalised (sigma <= 0: 0.15 n + 0.35, fused as softfloat's mulAdd) */
static void of_cv_gauss_kernel(float *k, int n, double sigma) {
    static double const k3[3] = {0.25, 0.5, 0.25};
    static double const k5[5] = {0.0625, 0.25, 0.375, 0.25, 0.0625};
    static double const k7[7] = {0.03125, 0.109375, 0.21875, 0.28125, 0.21875, 0.109375, 0.03125};
    static double const k9[9] = {4.0 / 256, 13.0 / 256, 30.0 / 256, 51.0 / 256, 60.0 / 256,
                                 51.0 / 256, 30.0 / 256, 13.0 / 256, 4.0 / 256};
    int i;
    if (sigma <= 0.0 && n <= 9 && (n & 1)) {
        double const *t = n == 1 ? NULL : n == 3 ? k3 : n == 5 ? k5 : n == 7 ? k7 : k9;
        for (i = 0; i < n; i++) k[i] = t ? (float)t[i] : 1.0f;
        return;
    }
    {
        double const sx = sigma > 0.0 ? sigma : ALWAN_FMA_CR_F64((double)n, 0.15, 0.35);
        double const scale2 = -0.125 / (sx * sx);
        int const h = (n - 1) / 2;
        double sum = 0.0, mul;
        int x;
        for (i = 0, x = 1 - n; i < h; i++, x += 2) sum += ALWAN_EXP_F64((double)(x * x) * scale2);
        sum *= 2.0;
        sum += 1.0;
        if ((n & 1) == 0) sum += 1.0;
        mul = 1.0 / sum;
        for (i = 0, x = 1 - n; i < h; i++, x += 2) {
            double const t = ALWAN_EXP_F64((double)(x * x) * scale2) * mul;
            k[i] = (float)t;
            k[n - 1 - i] = (float)t;
        }
        k[h] = (float)(1.0 * mul);
        if ((n & 1) == 0) k[h + 1] = k[h];
    }
}

/* The row pass of a symmetric kernel on one bordered row: S points at the bordered row's
 * first value (n / 2 extra values each side), D receives w values. */
static void of_cv_row(float *D, float const *Sb, int w, float const *k, int n) {
    int i = 0;
    if (n <= 5) {
        float const *S = Sb + n / 2;
        float const *kc = k + n / 2;
        if (n == 1) {
            for (i = 0; i < w; i++) D[i] = kc[0] * S[i];
            return;
        }
        /* SymmRowSmallVec_32f: vector blocks of 8, fused */
        if (n == 3) {
            int const special = ALWAN_ABS_F32(kc[0]) == 2.0f && kc[1] == 1.0f;
            for (; i <= w - 8; i += 8) {
                int j;
                for (j = i; j < i + 8; j++) {
                    if (special) D[j] = ALWAN_FMA_CR_F32(S[j], kc[0], S[j - 1] + S[j + 1]);
                    else D[j] = ALWAN_FMA_CR_F32(S[j], kc[0], (S[j - 1] + S[j + 1]) * kc[1]);
                }
            }
            /* SymmRowSmallFilter's scalar pairs */
            for (; i <= w - 2; i += 2) {
                int j;
                for (j = i; j < i + 2; j++) {
                    if (kc[0] == 2.0f && kc[1] == 1.0f) D[j] = S[j - 1] + S[j] * 2.0f + S[j + 1];
                    else if (kc[0] == -2.0f && kc[1] == 1.0f) D[j] = S[j - 1] - S[j] * 2.0f + S[j + 1];
                    else D[j] = S[j] * kc[0] + (S[j - 1] + S[j + 1]) * kc[1];
                }
            }
        } else if (n == 5) {
            int const special = kc[0] == -2.0f && kc[1] == 0.0f && kc[2] == 1.0f;
            for (; i <= w - 8; i += 8) {
                int j;
                for (j = i; j < i + 8; j++) {
                    if (special) D[j] = ALWAN_FMA_CR_F32(S[j], -2.0f, S[j - 2] + S[j + 2]);
                    else
                        D[j] = ALWAN_FMA_CR_F32(S[j + 2] + S[j - 2], kc[2],
                                                ALWAN_FMA_CR_F32(S[j], kc[0], (S[j - 1] + S[j + 1]) * kc[1]));
                }
            }
            for (; i <= w - 2; i += 2) {
                int j;
                for (j = i; j < i + 2; j++) {
                    if (special) D[j] = -2.0f * S[j] + S[j - 2] + S[j + 2];
                    else D[j] = S[j] * kc[0] + (S[j - 1] + S[j + 1]) * kc[1] + (S[j - 2] + S[j + 2]) * kc[2];
                }
            }
        }
        /* the single tail */
        for (; i < w; i++) {
            int kk;
            float s0 = kc[0] * S[i];
            for (kk = 1; kk <= n / 2; kk++) s0 += kc[kk] * (S[i + kk] + S[i - kk]);
            D[i] = s0;
        }
        return;
    }
    /* RowFilter with RowVec_32f: the AVX block of 8 fused from 0, the rest unfused */
    for (; i <= w - 8; i += 8) {
        int j;
        for (j = i; j < i + 8; j++) {
            float s = 0.0f;
            int kk;
            for (kk = 0; kk < n; kk++) s = ALWAN_FMA_CR_F32(Sb[j + kk], k[kk], s);
            D[j] = s;
        }
    }
    for (; i < w; i++) {
        float s = k[0] * Sb[i];
        int kk;
        for (kk = 1; kk < n; kk++) s += k[kk] * Sb[i + kk];
        D[i] = s;
    }
}

/* The column pass of a symmetric kernel: rows[-n/2 .. n/2] around the centre row. */
static void of_cv_col(float *D, float const *const *rows, int w, float const *k, int n) {
    int const h = n / 2;
    float const *kc = k + h;
    int i = 0;
    if (n == 3) {
        float const *S0 = rows[-1], *S1 = rows[0], *S2 = rows[1];
        int const one_two_one = kc[0] == 2.0f && kc[1] == 1.0f;
        /* SymmColumnSmallVec_32f, blocks of 8 */
        for (; i <= w - 8; i += 8) {
            int j;
            for (j = i; j < i + 8; j++) {
                if (one_two_one) D[j] = ALWAN_FMA_CR_F32(S1[j], kc[0], (S0[j] + S2[j]) + 0.0f);
                else D[j] = ALWAN_FMA_CR_F32(S0[j] + S2[j], kc[1], S1[j] * kc[0]);
            }
        }
        for (; i < w; i++) {
            if (one_two_one) D[i] = S0[i] + S1[i] * 2.0f + S2[i] + 0.0f;
            else D[i] = (S0[i] + S2[i]) * kc[1] + S1[i] * kc[0] + 0.0f;
        }
        return;
    }
    /* SymmColumnFilter with SymmColumnVec_32f: AVX blocks of 16, then one block of 8, fused */
    for (; i <= w - 16; i += 16) {
        int j;
        for (j = i; j < i + 16; j++) {
            float s = rows[0][j] * kc[0];
            int kk;
            for (kk = 1; kk <= h; kk++) s = ALWAN_FMA_CR_F32(rows[kk][j] + rows[-kk][j], kc[kk], s);
            D[j] = s;
        }
    }
    if (i <= w - 8) {
        int j;
        for (j = i; j < i + 8; j++) {
            float s = rows[0][j] * kc[0];
            int kk;
            for (kk = 1; kk <= h; kk++) s = ALWAN_FMA_CR_F32(rows[kk][j] + rows[-kk][j], kc[kk], s);
            D[j] = s;
        }
        i += 8;
    }
    for (; i < w; i++) {
        float s = kc[0] * rows[0][i] + 0.0f;
        int kk;
        for (kk = 1; kk <= h; kk++) s += kc[kk] * (rows[kk][i] + rows[-kk][i]);
        D[i] = s;
    }
}

/* cv::GaussianBlur(src, dst, Size(ksize, ksize), sigma, sigma) on a packed float plane;
 * dst may be src. 0 on no memory. */
static int of_cv_gaussian_blur(float *dst, float const *src, int w, int h, int ksize, double sigma) {
    float *kx, *ky;
    int nx = ksize, ny = ksize, x, y, i;
    float *R, *ext;
    float const **rp;
    if (h == 1) ny = 1;
    if (w == 1) nx = 1;
    if (nx == 1 && ny == 1) {
        if (dst != src) memcpy(dst, src, (size_t)w * (size_t)h * sizeof(float));
        return 1;
    }
    R = (float *)of_alloc((size_t)w * (size_t)h + (size_t)w + (size_t)ksize * 3 + 8, sizeof(float));
    rp = (float const **)of_alloc((size_t)ksize, sizeof(float const *));
    if (!R || !rp) {
        if (R) ALWAN_FREE(R);
        if (rp) ALWAN_FREE((void *)rp);
        return 0;
    }
    kx = R + (size_t)w * (size_t)h;
    ky = kx + ksize;
    ext = ky + ksize;
    of_cv_gauss_kernel(kx, nx, sigma);
    of_cv_gauss_kernel(ky, ny, sigma);
    for (y = 0; y < h; y++) {
        float const *s = src + (size_t)y * (size_t)w;
        for (x = 0; x < w + nx - 1; x++) ext[x] = s[of_reflect101(x - nx / 2, w)];
        of_cv_row(R + (size_t)y * (size_t)w, ext, w, kx, nx);
    }
    for (y = 0; y < h; y++) {
        for (i = 0; i < ny; i++) rp[i] = R + (size_t)of_reflect101(y + i - ny / 2, h) * (size_t)w;
        of_cv_col(dst + (size_t)y * (size_t)w, rp + ny / 2, w, ky, ny);
    }
    ALWAN_FREE((void *)rp);
    ALWAN_FREE(R);
    return 1;
}

/* ==================================================================================== */
/* OpenCV 5.0.0: the float warps of warp_kernels.simd.hpp, BORDER_CONSTANT 0              */
/* ==================================================================================== */

/* the source point of output (x, y): vec is the AVX2 kernel's form (fused) */
static void of_cv_map(float *sx, float *sy, float const M[9], int persp, int x, int y, int vec) {
    float const fx = (float)x, fy = (float)y;
    if (vec) {
        float const mx = fy * M[1] + M[2], my = fy * M[4] + M[5];
        float X = ALWAN_FMA_CR_F32(M[0], fx, mx), Y = ALWAN_FMA_CR_F32(M[3], fx, my);
        if (persp) {
            float const mw = fy * M[7] + M[8];
            float const W = ALWAN_FMA_CR_F32(M[6], fx, mw);
            X = X / W;
            Y = Y / W;
        }
        *sx = X;
        *sy = Y;
    } else {
        float X = fx * M[0] + fy * M[1] + M[2], Y = fx * M[3] + fy * M[4] + M[5];
        if (persp) {
            float const W = fx * M[6] + fy * M[7] + M[8];
            X = X / W;
            Y = Y / W;
        }
        *sx = X;
        *sy = Y;
    }
}

static int of_in_int(float v) {
    return v > -1073741824.f && v < 1073741824.f;
}

/* bilinear, one float channel, src and dst w x h */
static void of_cv_warp_linear(float *dst, float const *src, int w, int h, float const M[9], int persp) {
    int const vcols = (w / 16) * 16;
    int x, y;
    for (y = 0; y < h; y++) {
        float *d = dst + (size_t)y * (size_t)w;
        for (x = 0; x < w; x++) {
            int const vec = x < vcols;
            float sx, sy, a, b, p00, p01, p10, p11, v0, v1;
            int ix, iy;
            of_cv_map(&sx, &sy, M, persp, x, y, vec);
            d[x] = 0.0f;
            if (!of_in_int(sx) || !of_in_int(sy)) continue;
            ix = of_floor((double)sx);
            iy = of_floor((double)sy);
            a = sx - (float)ix;
            b = sy - (float)iy;
            if ((unsigned)(ix + 1) >= (unsigned)(w + 1) || (unsigned)(iy + 1) >= (unsigned)(h + 1)) continue;
            p00 = ((unsigned)ix < (unsigned)w && (unsigned)iy < (unsigned)h) ? src[(size_t)iy * w + ix] : 0.0f;
            p01 = ((unsigned)(ix + 1) < (unsigned)w && (unsigned)iy < (unsigned)h) ? src[(size_t)iy * w + ix + 1] : 0.0f;
            p10 = ((unsigned)ix < (unsigned)w && (unsigned)(iy + 1) < (unsigned)h) ? src[(size_t)(iy + 1) * w + ix] : 0.0f;
            p11 = ((unsigned)(ix + 1) < (unsigned)w && (unsigned)(iy + 1) < (unsigned)h) ? src[(size_t)(iy + 1) * w + ix + 1] : 0.0f;
            if (vec) {
                v0 = ALWAN_FMA_CR_F32(a, p01 - p00, p00);
                v1 = ALWAN_FMA_CR_F32(a, p11 - p10, p10);
                d[x] = ALWAN_FMA_CR_F32(b, v1 - v0, v0);
            } else {
                v0 = p00 + a * (p01 - p00);
                v1 = p10 + a * (p11 - p10);
                d[x] = v0 + b * (v1 - v0);
            }
        }
    }
}

/* nearest, 8-bit */
static void of_cv_warp_nearest_u8(unsigned char *dst, unsigned char const *src, int w, int h, float const M[9], int persp) {
    int const vcols = (w / 16) * 16;
    int x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            float sx, sy;
            int ix, iy;
            of_cv_map(&sx, &sy, M, persp, x, y, x < vcols);
            dst[(size_t)y * w + x] = 0;
            if (!of_in_int(sx) || !of_in_int(sy)) continue;
            ix = of_round((double)sx);
            iy = of_round((double)sy);
            if ((unsigned)ix < (unsigned)w && (unsigned)iy < (unsigned)h) dst[(size_t)y * w + x] = src[(size_t)iy * w + ix];
        }
    }
}

/* ==================================================================================== */
/* OpenCV 5.0.0: reductions                                                              */
/* ==================================================================================== */

/* dotProd_ (the scalar tail): groups of four products in double */
static double of_cv_dot_tail(float const *a, float const *b, int len) {
    double r = 0.0;
    int i = 0;
    for (; i <= len - 4; i += 4)
        r += (double)a[i] * b[i] + (double)a[i + 1] * b[i + 1] + (double)a[i + 2] * b[i + 2] + (double)a[i + 3] * b[i + 3];
    for (; i < len; i++) r += (double)a[i] * b[i];
    return r;
}

/* v_reduce_sum of an AVX2 float vector: ((l0 + l1) + (l2 + l3)) + ((l4 + l5) + (l6 + l7)) */
static float of_cv_hsum8(float const v[8]) {
    float const lo = (v[0] + v[1]) + (v[2] + v[3]);
    float const hi = (v[4] + v[5]) + (v[6] + v[7]);
    return lo + hi;
}

/* dotProd_32f */
static double of_cv_dot(float const *a, float const *b, int len) {
    double r = 0.0;
    int i = 0;
    int const len0 = len & -8;
    while (i < len0) {
        int const bs = len0 - i < 8192 ? len0 - i : 8192;
        float s0[8] = {0}, s1[8] = {0}, s2[8] = {0}, s3[8] = {0};
        int j = 0, l;
        for (; j <= bs - 32; j += 32) {
            for (l = 0; l < 8; l++) {
                s0[l] = ALWAN_FMA_CR_F32(a[j + l], b[j + l], s0[l]);
                s1[l] = ALWAN_FMA_CR_F32(a[j + 8 + l], b[j + 8 + l], s1[l]);
                s2[l] = ALWAN_FMA_CR_F32(a[j + 16 + l], b[j + 16 + l], s2[l]);
                s3[l] = ALWAN_FMA_CR_F32(a[j + 24 + l], b[j + 24 + l], s3[l]);
            }
        }
        for (l = 0; l < 8; l++) s0[l] = s0[l] + ((s1[l] + s2[l]) + s3[l]);
        for (; j <= bs - 8; j += 8)
            for (l = 0; l < 8; l++) s0[l] = ALWAN_FMA_CR_F32(a[j + l], b[j + l], s0[l]);
        r += (double)of_cv_hsum8(s0);
        a += bs;
        b += bs;
        i += bs;
    }
    return r + of_cv_dot_tail(a, b, len - i);
}

/* Mat::dot where one side is a column range: row by row */
static double of_cv_dot_rows(float const *a, float const *b, int w, int h) {
    double r = 0.0;
    int y;
    for (y = 0; y < h; y++) r += of_cv_dot(a + (size_t)y * w, b + (size_t)y * w, w);
    return r;
}

/* the square of cv::norm(block, NORM_L2), row by row in double */
static double of_cv_norm2_rows(float const *a, int w, int h) {
    double r = 0.0;
    int y, x;
    for (y = 0; y < h; y++) {
        double s = 0.0;
        for (x = 0; x < w; x++) {
            double const v = a[(size_t)y * w + x];
            s += v * v;
        }
        r += s;
    }
    return r;
}

/* meanStdDev with a mask (non-zero counts) */
static void of_cv_mean_std(double *mean, double *std, int *count, float const *v, unsigned char const *mask, size_t n) {
    double s = 0.0, sq = 0.0, scale;
    size_t i;
    int c = 0;
    for (i = 0; i < n; i++) {
        if (mask[i]) {
            double const t = v[i];
            s += t;
            sq += t * t;
            c++;
        }
    }
    scale = c ? 1.0 / (double)c : 0.0;
    *mean = s * scale;
    sq = sq * scale - (*mean) * (*mean);
    *std = ALWAN_SQRT_F64(sq > 0.0 ? sq : 0.0);
    *count = c;
}

/* LUImpl<float> with b the identity: the inverse; 0 when a pivot is under 10 FLT_EPSILON */
static int of_cv_lu_inv(float *A, int m, float *b) {
    float const eps = FLT_EPSILON * 10.0f;
    int i, j, k, c;
    for (i = 0; i < m * m; i++) b[i] = 0.0f;
    for (i = 0; i < m; i++) b[i * m + i] = 1.0f;
    for (i = 0; i < m; i++) {
        float d;
        k = i;
        for (j = i + 1; j < m; j++)
            if (ALWAN_ABS_F32(A[j * m + i]) > ALWAN_ABS_F32(A[k * m + i])) k = j;
        if (ALWAN_ABS_F32(A[k * m + i]) < eps) return 0;
        if (k != i) {
            for (j = i; j < m; j++) { float t = A[i * m + j]; A[i * m + j] = A[k * m + j]; A[k * m + j] = t; }
            for (c = 0; c < m; c++) { float t = b[i * m + c]; b[i * m + c] = b[k * m + c]; b[k * m + c] = t; }
        }
        d = -1.0f / A[i * m + i];
        for (j = i + 1; j < m; j++) {
            float const alpha = A[j * m + i] * d;
            for (k = i + 1; k < m; k++) {
                float const t = alpha * A[i * m + k];
                A[j * m + k] += t;
            }
            for (c = 0; c < m; c++) {
                float const t = alpha * b[i * m + c];
                b[j * m + c] += t;
            }
        }
    }
    for (i = m - 1; i >= 0; i--) {
        for (c = 0; c < m; c++) {
            float s = b[i * m + c];
            for (k = i + 1; k < m; k++) {
                float const t = A[i * m + k] * b[k * m + c];
                s -= t;
            }
            b[i * m + c] = s / A[i * m + i];
        }
    }
    return 1;
}

/* Mat::inv (DECOMP_LU) of a float n x n: zeros when singular */
static void of_cv_inv(float *D, float const *S, int n) {
    int i;
    if (n == 2) {
        double d = (double)S[0] * S[3] - (double)S[1] * S[2];
        for (i = 0; i < 4; i++) D[i] = 0.0f;
        if (d != 0.0) {
            /* the CV_SIMD128 form: times the reciprocal rounded to float, in float */
            float const df = (float)(1.0 / d);
            D[0] = S[3] * df;
            D[1] = S[1] * -df;
            D[2] = S[2] * -df;
            D[3] = S[0] * df;
        }
    } else if (n == 3) {
        double d = S[0] * ((double)S[4] * S[8] - (double)S[5] * S[7]) - S[1] * ((double)S[3] * S[8] - (double)S[5] * S[6]) +
                   S[2] * ((double)S[3] * S[7] - (double)S[4] * S[6]);
        for (i = 0; i < 9; i++) D[i] = 0.0f;
        if (d != 0.0) {
            double t[9];
            d = 1.0 / d;
            t[0] = ((double)S[4] * S[8] - (double)S[5] * S[7]) * d;
            t[1] = ((double)S[2] * S[7] - (double)S[1] * S[8]) * d;
            t[2] = ((double)S[1] * S[5] - (double)S[2] * S[4]) * d;
            t[3] = ((double)S[5] * S[6] - (double)S[3] * S[8]) * d;
            t[4] = ((double)S[0] * S[8] - (double)S[2] * S[6]) * d;
            t[5] = ((double)S[2] * S[3] - (double)S[0] * S[5]) * d;
            t[6] = ((double)S[3] * S[7] - (double)S[4] * S[6]) * d;
            t[7] = ((double)S[1] * S[6] - (double)S[0] * S[7]) * d;
            t[8] = ((double)S[0] * S[4] - (double)S[1] * S[3]) * d;
            for (i = 0; i < 9; i++) D[i] = (float)t[i];
        }
    } else {
        float A[64];
        memcpy(A, S, (size_t)n * (size_t)n * sizeof(float));
        if (!of_cv_lu_inv(A, n, D))
            for (i = 0; i < n * n; i++) D[i] = 0.0f;
    }
}

/* Mat * Mat for an n x n matrix and an n-vector (gemm): n of 2 to 4 takes gemmImpl's
 * small-matrix path with the sums in float, larger n accumulates in double */
static void of_cv_gemv(float *d, float const *A, float const *b, int n) {
    int i, j;
    for (i = 0; i < n; i++) {
        if (n <= 4) {
            float s = A[i * n] * b[0];
            for (j = 1; j < n; j++) s = ALWAN_FMA_CR_F32(A[i * n + j], b[j], s);
            d[i] = s;
        } else {
            double s = 0.0;
            for (j = 0; j < n; j++) s += (double)A[i * n + j] * b[j];
            d[i] = (float)s;
        }
    }
}

/* ==================================================================================== */
/* ECC                                                                                   */
/* ==================================================================================== */

/* threshold > 0 to 1, blur, times 0.5 / 0.95, round to 8 bits (findTransformECCWithMask) */
static int of_ecc_mask(unsigned char *out, unsigned char const *m, size_t rs, int w, int h, int gk, float *tmp) {
    int x, y;
    float const a = (float)(0.5 / 0.95);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) tmp[(size_t)y * w + x] = m[(size_t)y * rs + x] > 0 ? 1.0f : 0.0f;
    if (!of_cv_gaussian_blur(tmp, tmp, w, h, gk, 0.0)) return 0;
    for (x = 0; x < w * h; x++) {
        int const r = of_round((double)(tmp[x] * a));
        out[x] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r);
    }
    return 1;
}

alwan_status alwan__register_ecc(alwan_register_result *out, float const *ref, float const *mov, size_t W, size_t H,
                                 alwan_register_params const *params) {
    alwan_register_motion const motion = params ? params->motion : ALWAN_REGISTER_MOTION_AFFINE;
    int const max_iter = params && params->max_iterations > 0 ? (int)(params->max_iterations > 100000 ? 100000 : params->max_iterations) : 50;
    double const eps = params && params->epsilon != 0.0 ? (params->epsilon < 0.0 ? -1.0 : params->epsilon) : 0.001;
    int const gk = params && params->gauss_filter_size > 0 ? (int)params->gauss_filter_size : 5;
    unsigned char const *tmask_in = params ? params->reference_mask : NULL;
    unsigned char const *imask_in = params ? params->moving_mask : NULL;
    int const w = (int)W, h = (int)H, persp = motion == ALWAN_REGISTER_MOTION_HOMOGRAPHY;
    size_t const N = W * H;
    int np, i, j, iter;
    float M[9] = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
    float *buf = NULL, *tmpl, *img, *gx, *gy, *iw, *gxw, *gyw, *tzm, *err, *jac, *X, *Y;
    unsigned char *mbuf = NULL, *pre, *tmsk, *imask;
    float hess[64], hinv[64], iproj[8], tproj[8], iph[8], eproj[8], dp[8];
    double rho = -1.0, last_rho = -eps;
    alwan_status st = ALWAN_OK;

    if (motion != ALWAN_REGISTER_MOTION_AFFINE && motion != ALWAN_REGISTER_MOTION_TRANSLATION &&
        motion != ALWAN_REGISTER_MOTION_EUCLIDEAN && motion != ALWAN_REGISTER_MOTION_HOMOGRAPHY)
        return ALWAN_E_INVALID;
    if ((gk & 1) == 0 || gk > 63) return ALWAN_E_INVALID;
    if (W > (size_t)INT_MAX / 16 || H > (size_t)INT_MAX / 16) return ALWAN_E_RANGE;
    if (tmask_in && params->reference_mask_row_stride < W) return ALWAN_E_INVALID;
    if (imask_in && params->moving_mask_row_stride < W) return ALWAN_E_INVALID;
    for (i = 0; i < (int)N; i++)
        if (!isfinite(ref[i]) || !isfinite(mov[i])) return ALWAN_E_RANGE;
    np = motion == ALWAN_REGISTER_MOTION_TRANSLATION ? 2 : motion == ALWAN_REGISTER_MOTION_EUCLIDEAN ? 3 :
         motion == ALWAN_REGISTER_MOTION_HOMOGRAPHY ? 8 : 6;
    if (params && params->initial_warp) {
        for (i = 0; i < (persp ? 9 : 6); i++) {
            if (!isfinite(params->initial_warp[i])) return ALWAN_E_RANGE;
            M[i] = (float)params->initial_warp[i];
        }
    }

    buf = (float *)of_alloc(N * (size_t)(12 + np), sizeof(float));
    mbuf = (unsigned char *)of_alloc(N * 3, 1);
    if (!buf || !mbuf) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    tmpl = buf;
    img = tmpl + N;
    gx = img + N;
    gy = gx + N;
    iw = gy + N;
    gxw = iw + N;
    gyw = gxw + N;
    tzm = gyw + N;
    err = tzm + N;
    X = err + N;
    Y = X + N;
    jac = Y + N + N; /* one spare plane for the mask blur */
    pre = mbuf;
    tmsk = pre + N;
    imask = tmsk + N;

    for (i = 0; i < (int)N; i++) {
        X[i] = (float)(i % w);
        Y[i] = (float)(i / w);
    }
    if (!of_cv_gaussian_blur(tmpl, ref, w, h, gk, 0.0) || !of_cv_gaussian_blur(img, mov, w, h, gk, 0.0)) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    /* filter2D with dx = (-0.5, 0, 0.5) and its transpose, BORDER_REFLECT_101 */
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            float const l = img[(size_t)j * w + of_reflect101(i - 1, w)], r = img[(size_t)j * w + of_reflect101(i + 1, w)];
            float const u = img[(size_t)of_reflect101(j - 1, h) * w + i], d = img[(size_t)of_reflect101(j + 1, h) * w + i];
            gx[(size_t)j * w + i] = ALWAN_FMA_CR_F32(r, 0.5f, -0.5f * l);
            gy[(size_t)j * w + i] = ALWAN_FMA_CR_F32(d, 0.5f, -0.5f * u);
        }
    }
    if (tmask_in) {
        if (!of_ecc_mask(tmsk, tmask_in, params->reference_mask_row_stride, w, h, gk, Y + N)) {
            st = ALWAN_E_NOMEM;
            goto done;
        }
    } else {
        memset(tmsk, 1, N);
    }
    if (imask_in) {
        if (!of_ecc_mask(pre, imask_in, params->moving_mask_row_stride, w, h, gk, Y + N)) {
            st = ALWAN_E_NOMEM;
            goto done;
        }
        if (!tmask_in)
            for (i = 0; i < (int)N; i++)
                if (pre[i] == 0) gx[i] = gy[i] = 0.0f;
    } else {
        memset(pre, 1, N);
    }

    out->iterations = 0;
    for (iter = 1; iter <= max_iter && ALWAN_ABS_F64(rho - last_rho) >= eps; iter++) {
        double imean, istd, tmean, tstd, tnorm, inorm, corr, lam_n, lam_d, lam;
        int valid, cnt;
        float const fl0 = 0.0f;
        of_cv_warp_linear(iw, img, w, h, M, persp);
        of_cv_warp_linear(gxw, gx, w, h, M, persp);
        of_cv_warp_linear(gyw, gy, w, h, M, persp);
        of_cv_warp_nearest_u8(imask, pre, w, h, M, persp);
        if (tmask_in) {
            for (i = 0; i < (int)N; i++) {
                imask[i] = (unsigned char)(imask[i] & tmsk[i]);
                if (imask[i] == 0) gxw[i] = gyw[i] = 0.0f;
            }
        }
        of_cv_mean_std(&imean, &istd, &valid, iw, imask, N);
        of_cv_mean_std(&tmean, &tstd, &cnt, tmpl, imask, N);
        {
            float const fim = (float)imean, ftm = (float)tmean;
            for (i = 0; i < (int)N; i++) {
                if (imask[i]) {
                    iw[i] = iw[i] - fim;
                    tzm[i] = tmpl[i] - ftm;
                } else {
                    tzm[i] = fl0;
                }
            }
        }
        tnorm = ALWAN_SQRT_F64((double)valid * (tstd * tstd));
        inorm = ALWAN_SQRT_F64((double)valid * (istd * istd));

        /* the Jacobian, np planes of w x h */
        if (motion == ALWAN_REGISTER_MOTION_TRANSLATION) {
            memcpy(jac, gxw, N * sizeof(float));
            memcpy(jac + N, gyw, N * sizeof(float));
        } else if (motion == ALWAN_REGISTER_MOTION_AFFINE) {
            for (i = 0; i < (int)N; i++) {
                jac[i] = gxw[i] * X[i];
                jac[N + i] = gyw[i] * X[i];
                jac[2 * N + i] = gxw[i] * Y[i];
                jac[3 * N + i] = gyw[i] * Y[i];
                jac[4 * N + i] = gxw[i];
                jac[5 * N + i] = gyw[i];
            }
        } else if (motion == ALWAN_REGISTER_MOTION_EUCLIDEAN) {
            float const h0 = M[0], h1 = M[3];
            for (i = 0; i < (int)N; i++) {
                float const hatX = ALWAN_FMA_CR_F32(X[i], -h1, -(Y[i] * h0));
                float const hatY = ALWAN_FMA_CR_F32(X[i], h0, -(Y[i] * h1));
                jac[i] = gxw[i] * hatX + gyw[i] * hatY;
                jac[N + i] = gxw[i];
                jac[2 * N + i] = gyw[i];
            }
        } else {
            float const h0 = M[0], h1 = M[3], h2 = M[6], h3 = M[1], h4 = M[4], h5 = M[7], h6 = M[2], h7 = M[5];
            for (i = 0; i < (int)N; i++) {
                float const den = ALWAN_FMA_CR_F32(X[i], h2, ALWAN_FMA_CR_F32(Y[i], h5, 1.0f));
                float hatX = ALWAN_FMA_CR_F32(X[i], h0, Y[i] * h3) + h6;
                float hatY = ALWAN_FMA_CR_F32(X[i], h1, Y[i] * h4) + h7;
                float const g1 = gxw[i] / den, g2 = gyw[i] / den;
                float t;
                hatY = -hatY / den;
                hatX = -hatX / den;
                t = hatX * g1 + hatY * g2;
                jac[i] = g1 * X[i];
                jac[N + i] = g2 * X[i];
                jac[2 * N + i] = t * X[i];
                jac[3 * N + i] = g1 * Y[i];
                jac[4 * N + i] = g2 * Y[i];
                jac[5 * N + i] = t * Y[i];
                jac[6 * N + i] = g1;
                jac[7 * N + i] = g2;
            }
        }
        for (i = 0; i < np; i++) {
            double const nn = ALWAN_SQRT_F64(of_cv_norm2_rows(jac + (size_t)i * N, w, h));
            hess[i * (np + 1)] = (float)(nn * nn);
            for (j = i + 1; j < np; j++) {
                hess[i * np + j] = (float)of_cv_dot_rows(jac + (size_t)i * N, jac + (size_t)j * N, w, h);
                hess[j * np + i] = hess[i * np + j];
            }
        }
        of_cv_inv(hinv, hess, np);
        corr = of_cv_dot(tzm, iw, (int)N);
        last_rho = rho;
        rho = corr / (inorm * tnorm);
        if (isnan(rho)) {
            st = ALWAN_E_RANGE;
            goto done;
        }
        for (i = 0; i < np; i++) {
            iproj[i] = (float)of_cv_dot_rows(iw, jac + (size_t)i * N, w, h);
            tproj[i] = (float)of_cv_dot_rows(tzm, jac + (size_t)i * N, w, h);
        }
        of_cv_gemv(iph, hinv, iproj, np);
        lam_n = inorm * inorm - of_cv_dot(iproj, iph, np);
        lam_d = corr - of_cv_dot(tproj, iph, np);
        if (lam_d <= 0.0) {
            st = ALWAN_E_RANGE;
            goto done;
        }
        lam = lam_n / lam_d;
        {
            float const fl = (float)lam;
            if (N >= 8) {
                for (i = 0; i < (int)N; i++) err[i] = ALWAN_FMA_CR_F32(tzm[i], fl, -iw[i]);
            } else {
                for (i = 0; i < (int)N; i++) err[i] = tzm[i] * fl + iw[i] * -1.0f + 0.0f;
            }
        }
        for (i = 0; i < np; i++) eproj[i] = (float)of_cv_dot_rows(err, jac + (size_t)i * N, w, h);
        of_cv_gemv(dp, hinv, eproj, np);
        if (motion == ALWAN_REGISTER_MOTION_TRANSLATION) {
            M[2] += dp[0];
            M[5] += dp[1];
        } else if (motion == ALWAN_REGISTER_MOTION_AFFINE) {
            M[0] += dp[0];
            M[3] += dp[1];
            M[1] += dp[2];
            M[4] += dp[3];
            M[2] += dp[4];
            M[5] += dp[5];
        } else if (motion == ALWAN_REGISTER_MOTION_HOMOGRAPHY) {
            M[0] += dp[0];
            M[3] += dp[1];
            M[6] += dp[2];
            M[1] += dp[3];
            M[4] += dp[4];
            M[7] += dp[5];
            M[2] += dp[6];
            M[5] += dp[7];
        } else {
            double const s = (double)M[3];
            double th = (double)dp[0];
            th += ALWAN_ATAN2_F64(s, ALWAN_SQRT_F64((1.0 - s) * (1.0 + s)));
            M[2] += dp[1];
            M[5] += dp[2];
            M[0] = M[4] = (float)ALWAN_COS_F64(th);
            M[3] = (float)ALWAN_SIN_F64(th);
            M[1] = -M[3];
        }
        out->iterations = (size_t)iter;
    }
    for (i = 0; i < 9; i++) out->warp[i] = (double)M[i];
    if (!persp) {
        out->warp[6] = out->warp[7] = 0.0;
        out->warp[8] = 1.0;
    }
    out->correlation = rho;
    out->error = 1.0 - rho;
    out->phasediff = 0.0;
    out->shift[0] = 0.0 - out->warp[5];
    out->shift[1] = 0.0 - out->warp[2];
done:
    if (buf) ALWAN_FREE(buf);
    if (mbuf) ALWAN_FREE(mbuf);
    return st;
}

/* ==================================================================================== */
/* OpenCV 5.0.0: calcOpticalFlowFarneback (optflowgf.cpp)                                */
/* ==================================================================================== */

/* Mat::inv(DECOMP_CHOLESKY) on the 6 x 6 moment matrix: CholImpl<double> with the identity
 * as the right-hand side. 0 when not positive definite. */
static int of_chol_inv6(double *A, double *b) {
    int const m = 6;
    int i, j, k;
    double s;
    double *L = A;
    for (i = 0; i < m * m; i++) b[i] = 0.0;
    for (i = 0; i < m; i++) b[i * m + i] = 1.0;
    for (i = 0; i < m; i++) {
        for (j = 0; j < i; j++) {
            s = A[i * m + j];
            for (k = 0; k < j; k++) s -= L[i * m + k] * L[j * m + k];
            L[i * m + j] = s * L[j * m + j];
        }
        s = A[i * m + i];
        for (k = 0; k < j; k++) {
            double const t = L[i * m + k];
            s -= t * t;
        }
        if (s < DBL_EPSILON) return 0;
        L[i * m + i] = 1.0 / ALWAN_SQRT_F64(s);
    }
    for (i = 0; i < m; i++) {
        for (j = 0; j < m; j++) {
            s = b[i * m + j];
            for (k = 0; k < i; k++) s -= L[i * m + k] * b[k * m + j];
            b[i * m + j] = s * L[i * m + i];
        }
    }
    for (i = m - 1; i >= 0; i--) {
        for (j = 0; j < m; j++) {
            s = b[i * m + j];
            for (k = m - 1; k > i; k--) s -= L[k * m + i] * b[k * m + j];
            b[i * m + j] = s * L[i * m + i];
        }
    }
    return 1;
}

/* FarnebackPrepareGaussian: g, xg and xxg on -n .. n (pointers at the centre) and four
 * entries of the inverse moment matrix. 0 when the matrix does not invert. */
static int of_fb_prepare(float *g, float *xg, float *xxg, int n, double sigma, double *ig11, double *ig03, double *ig33,
                         double *ig55) {
    double s = 0.0, G[36], inv[36];
    int x, y;
    if (sigma < FLT_EPSILON) sigma = n * 0.3;
    for (x = -n; x <= n; x++) {
        g[x] = (float)ALWAN_EXP_F64(-x * x / (2 * sigma * sigma));
        s += g[x];
    }
    s = 1. / s;
    for (x = -n; x <= n; x++) {
        g[x] = (float)(g[x] * s);
        xg[x] = (float)x * g[x];
        xxg[x] = (float)(x * x) * g[x];
    }
    for (x = 0; x < 36; x++) G[x] = 0.0;
    for (y = -n; y <= n; y++) {
        for (x = -n; x <= n; x++) {
            float const fx = (float)x, fy = (float)y;
            G[0] += g[y] * g[x];
            G[7] += g[y] * g[x] * fx * fx;
            G[21] += g[y] * g[x] * fx * fx * fx * fx;
            G[35] += g[y] * g[x] * fx * fx * fy * fy;
        }
    }
    G[14] = G[3] = G[4] = G[18] = G[24] = G[7];
    G[28] = G[21];
    G[22] = G[27] = G[35];
    if (!of_chol_inv6(G, inv)) return 0;
    *ig11 = inv[7];
    *ig03 = inv[3];
    *ig33 = inv[21];
    *ig55 = inv[35];
    return 1;
}

/* FarnebackPolyExp: dst is w x h x 5. 0 on no memory. */
static int of_fb_polyexp(float *dst, float const *src, int w, int h, int n, double sigma) {
    float kbuf[6 * 7 + 3];
    float *g = kbuf + n, *xg = g + n * 2 + 1, *xxg = xg + n * 2 + 1;
    float *rowb = (float *)of_alloc(((size_t)w + (size_t)n * 2) * 3, sizeof(float));
    float *row;
    double ig11, ig03, ig33, ig55;
    int x, y, k;
    if (!rowb) return 0;
    row = rowb + n * 3;
    if (!of_fb_prepare(g, xg, xxg, n, sigma, &ig11, &ig03, &ig33, &ig55)) {
        ALWAN_FREE(rowb);
        return 0;
    }
    for (y = 0; y < h; y++) {
        float g0 = g[0], g1, g2;
        float const *srow0 = src + (size_t)y * (size_t)w, *srow1;
        float *drow = dst + (size_t)y * (size_t)w * 5;
        /* vertical part of the convolution, replicated rows */
        for (x = 0; x < w; x++) {
            row[x * 3] = srow0[x] * g0;
            row[x * 3 + 1] = row[x * 3 + 2] = 0.f;
        }
        for (k = 1; k <= n; k++) {
            g0 = g[k];
            g1 = xg[k];
            g2 = xxg[k];
            srow0 = src + (size_t)(y - k > 0 ? y - k : 0) * (size_t)w;
            srow1 = src + (size_t)(y + k < h - 1 ? y + k : h - 1) * (size_t)w;
            for (x = 0; x < w; x++) {
                float const p = srow0[x] + srow1[x];
                float const t0 = row[x * 3] + g0 * p;
                float const t1 = row[x * 3 + 1] + g1 * (srow1[x] - srow0[x]);
                float const t2 = row[x * 3 + 2] + g2 * p;
                row[x * 3] = t0;
                row[x * 3 + 1] = t1;
                row[x * 3 + 2] = t2;
            }
        }
        /* horizontal part */
        for (x = 0; x < n * 3; x++) {
            row[-1 - x] = row[2 - x];
            row[w * 3 + x] = row[w * 3 + x - 3];
        }
        for (x = 0; x < w; x++) {
            double b1, b2 = 0, b3, b4 = 0, b5, b6 = 0;
            g0 = g[0];
            b1 = row[x * 3] * g0;
            b3 = row[x * 3 + 1] * g0;
            b5 = row[x * 3 + 2] * g0;
            for (k = 1; k <= n; k++) {
                double const tg = (double)(row[(x + k) * 3] + row[(x - k) * 3]);
                g0 = g[k];
                b1 += tg * g0;
                b4 += tg * xxg[k];
                b2 += (row[(x + k) * 3] - row[(x - k) * 3]) * xg[k];
                b3 += (row[(x + k) * 3 + 1] + row[(x - k) * 3 + 1]) * g0;
                b6 += (row[(x + k) * 3 + 1] - row[(x - k) * 3 + 1]) * xg[k];
                b5 += (row[(x + k) * 3 + 2] + row[(x - k) * 3 + 2]) * g0;
            }
            drow[x * 5 + 1] = (float)(b2 * ig11);
            drow[x * 5] = (float)(b3 * ig11);
            drow[x * 5 + 3] = (float)(b1 * ig03 + b4 * ig33);
            drow[x * 5 + 2] = (float)(b1 * ig03 + b5 * ig33);
            drow[x * 5 + 4] = (float)(b6 * ig55);
        }
    }
    ALWAN_FREE(rowb);
    return 1;
}

/* FarnebackUpdateMatrices on rows y0 .. y1 - 1; R0, R1 and M are w x h x 5, flow w x h x 2 */
static void of_fb_update_matrices(float const *R0a, float const *R1, float const *flowa, float *Ma, int w, int h, int y0,
                                  int y1) {
    static float const border[5] = {0.14f, 0.14f, 0.4472f, 0.4472f, 0.4472f};
    int const B = 5;
    size_t const step1 = (size_t)w * 5;
    int x, y;
    for (y = y0; y < y1; y++) {
        float const *flow = flowa + (size_t)y * (size_t)w * 2;
        float const *R0 = R0a + (size_t)y * step1;
        float *M = Ma + (size_t)y * step1;
        for (x = 0; x < w; x++) {
            float const dx = flow[x * 2], dy = flow[x * 2 + 1];
            float fx = (float)x + dx, fy = (float)y + dy;
            int const x1 = of_floor((double)fx), yy = of_floor((double)fy);
            float r2, r3, r4, r5, r6;
            fx -= (float)x1;
            fy -= (float)yy;
            if ((unsigned)x1 < (unsigned)(w - 1) && (unsigned)yy < (unsigned)(h - 1)) {
                float const *ptr = R1 + (size_t)yy * step1 + (size_t)x1 * 5;
                float const a00 = (1.f - fx) * (1.f - fy), a01 = fx * (1.f - fy), a10 = (1.f - fx) * fy, a11 = fx * fy;
                r2 = a00 * ptr[0] + a01 * ptr[5] + a10 * ptr[step1] + a11 * ptr[step1 + 5];
                r3 = a00 * ptr[1] + a01 * ptr[6] + a10 * ptr[step1 + 1] + a11 * ptr[step1 + 6];
                r4 = a00 * ptr[2] + a01 * ptr[7] + a10 * ptr[step1 + 2] + a11 * ptr[step1 + 7];
                r5 = a00 * ptr[3] + a01 * ptr[8] + a10 * ptr[step1 + 3] + a11 * ptr[step1 + 8];
                r6 = a00 * ptr[4] + a01 * ptr[9] + a10 * ptr[step1 + 4] + a11 * ptr[step1 + 9];
                r4 = (R0[x * 5 + 2] + r4) * 0.5f;
                r5 = (R0[x * 5 + 3] + r5) * 0.5f;
                r6 = (R0[x * 5 + 4] + r6) * 0.25f;
            } else {
                r2 = r3 = 0.f;
                r4 = R0[x * 5 + 2];
                r5 = R0[x * 5 + 3];
                r6 = R0[x * 5 + 4] * 0.5f;
            }
            r2 = (R0[x * 5] - r2) * 0.5f;
            r3 = (R0[x * 5 + 1] - r3) * 0.5f;
            r2 += r4 * dy + r6 * dx;
            r3 += r6 * dy + r5 * dx;
            if ((unsigned)(x - B) >= (unsigned)(w - B * 2) || (unsigned)(y - B) >= (unsigned)(h - B * 2)) {
                float const scale = (x < B ? border[x] : 1.f) * (x >= w - B ? border[w - x - 1] : 1.f) *
                                    (y < B ? border[y] : 1.f) * (y >= h - B ? border[h - y - 1] : 1.f);
                r2 *= scale;
                r3 *= scale;
                r4 *= scale;
                r5 *= scale;
                r6 *= scale;
            }
            M[x * 5] = r4 * r4 + r6 * r6;
            M[x * 5 + 1] = (r4 + r5) * r6;
            M[x * 5 + 2] = r5 * r5 + r6 * r6;
            M[x * 5 + 3] = r4 * r2 + r6 * r3;
            M[x * 5 + 4] = r6 * r2 + r5 * r3;
        }
    }
}

/* FarnebackUpdateFlow_Blur: a box window of bsz, running sums in double. 0 on no memory. */
static int of_fb_flow_blur(float const *R0, float const *R1, float *flowa, float *Ma, int w, int h, int bsz, int upd) {
    int const m = bsz / 2;
    int const min_stripe = (1 << 10) / w > bsz ? (1 << 10) / w : bsz;
    double const scale = 1. / (bsz * bsz);
    double *vb = (double *)of_alloc(((size_t)w + (size_t)m * 2 + 2) * 5, sizeof(double));
    double *vsum;
    float const *srow0;
    int y0 = 0, y1, x, y;
    if (!vb) return 0;
    vsum = vb + (size_t)(m + 1) * 5;
    srow0 = Ma;
    for (x = 0; x < w * 5; x++) vsum[x] = srow0[x] * (m + 2);
    for (y = 1; y < m; y++) {
        srow0 = Ma + (size_t)(y < h - 1 ? y : h - 1) * (size_t)w * 5;
        for (x = 0; x < w * 5; x++) vsum[x] += srow0[x];
    }
    for (y = 0; y < h; y++) {
        double g11, g12, g22, h1, h2;
        float *flow = flowa + (size_t)y * (size_t)w * 2;
        float const *srow1;
        srow0 = Ma + (size_t)(y - m - 1 > 0 ? y - m - 1 : 0) * (size_t)w * 5;
        srow1 = Ma + (size_t)(y + m < h - 1 ? y + m : h - 1) * (size_t)w * 5;
        for (x = 0; x < w * 5; x++) vsum[x] += srow1[x] - srow0[x];
        for (x = 0; x < (m + 1) * 5; x++) {
            vsum[-1 - x] = vsum[4 - x];
            vsum[w * 5 + x] = vsum[w * 5 + x - 5];
        }
        g11 = vsum[0] * (m + 2);
        g12 = vsum[1] * (m + 2);
        g22 = vsum[2] * (m + 2);
        h1 = vsum[3] * (m + 2);
        h2 = vsum[4] * (m + 2);
        for (x = 1; x < m; x++) {
            g11 += vsum[x * 5];
            g12 += vsum[x * 5 + 1];
            g22 += vsum[x * 5 + 2];
            h1 += vsum[x * 5 + 3];
            h2 += vsum[x * 5 + 4];
        }
        for (x = 0; x < w; x++) {
            double g11_, g12_, g22_, h1_, h2_, idet;
            g11 += vsum[(x + m) * 5] - vsum[(x - m) * 5 - 5];
            g12 += vsum[(x + m) * 5 + 1] - vsum[(x - m) * 5 - 4];
            g22 += vsum[(x + m) * 5 + 2] - vsum[(x - m) * 5 - 3];
            h1 += vsum[(x + m) * 5 + 3] - vsum[(x - m) * 5 - 2];
            h2 += vsum[(x + m) * 5 + 4] - vsum[(x - m) * 5 - 1];
            g11_ = g11 * scale;
            g12_ = g12 * scale;
            g22_ = g22 * scale;
            h1_ = h1 * scale;
            h2_ = h2 * scale;
            idet = 1. / (g11_ * g22_ - g12_ * g12_ + 1e-3);
            flow[x * 2] = (float)((g11_ * h2_ - g12_ * h1_) * idet);
            flow[x * 2 + 1] = (float)((g22_ * h1_ - g12_ * h2_) * idet);
        }
        y1 = y == h - 1 ? h : y - bsz;
        if (upd && (y1 == h || y1 >= y0 + min_stripe)) {
            of_fb_update_matrices(R0, R1, flowa, Ma, w, h, y0, y1);
            y0 = y1;
        }
    }
    ALWAN_FREE(vb);
    return 1;
}

/* FarnebackUpdateFlow_GaussianBlur: a Gaussian window of bsz in float. The SSE3 baseline
 * has no fused multiply-add, so its vector blocks are the scalar loops. 0 on no memory. */
static int of_fb_flow_gauss(float const *R0, float const *R1, float *flowa, float *Ma, int w, int h, int bsz, int upd) {
    int const m = bsz / 2;
    int const min_stripe = (1 << 10) / w > bsz ? (1 << 10) / w : bsz;
    double const sigma = m * 0.3;
    double s = 1;
    size_t const nv = ((size_t)w + (size_t)m * 2 + 2) * 5;
    float *vb = (float *)of_alloc(nv + (size_t)w * 5 + (size_t)m + 1, sizeof(float));
    float const **srow = (float const **)of_alloc((size_t)m * 2 + 1, sizeof(float const *));
    float *vsum, *hsum, *kernel;
    int y0 = 0, y1, x, y, i;
    if (!vb || !srow) {
        if (vb) ALWAN_FREE(vb);
        if (srow) ALWAN_FREE((void *)srow);
        return 0;
    }
    vsum = vb + (size_t)(m + 1) * 5;
    hsum = vb + nv;
    kernel = hsum + (size_t)w * 5;
    kernel[0] = (float)s;
    for (i = 1; i <= m; i++) {
        float const t = (float)ALWAN_EXP_F64(-i * i / (2 * sigma * sigma));
        kernel[i] = t;
        s += t * 2;
    }
    s = 1. / s;
    for (i = 0; i <= m; i++) kernel[i] = (float)(kernel[i] * s);
    for (y = 0; y < h; y++) {
        double g11, g12, g22, h1, h2, idet;
        float *flow = flowa + (size_t)y * (size_t)w * 2;
        for (i = 0; i <= m; i++) {
            srow[m - i] = Ma + (size_t)(y - i > 0 ? y - i : 0) * (size_t)w * 5;
            srow[m + i] = Ma + (size_t)(y + i < h - 1 ? y + i : h - 1) * (size_t)w * 5;
        }
        for (x = 0; x < w * 5; x++) {
            float s0 = srow[m][x] * kernel[0];
            for (i = 1; i <= m; i++) s0 += (srow[m + i][x] + srow[m - i][x]) * kernel[i];
            vsum[x] = s0;
        }
        for (x = 0; x < m * 5; x++) {
            vsum[-1 - x] = vsum[4 - x];
            vsum[w * 5 + x] = vsum[w * 5 + x - 5];
        }
        for (x = 0; x < w * 5; x++) {
            float sum = vsum[x] * kernel[0];
            for (i = 1; i <= m; i++) sum += kernel[i] * (vsum[x - i * 5] + vsum[x + i * 5]);
            hsum[x] = sum;
        }
        for (x = 0; x < w; x++) {
            g11 = hsum[x * 5];
            g12 = hsum[x * 5 + 1];
            g22 = hsum[x * 5 + 2];
            h1 = hsum[x * 5 + 3];
            h2 = hsum[x * 5 + 4];
            idet = 1. / (g11 * g22 - g12 * g12 + 1e-3);
            flow[x * 2] = (float)((g11 * h2 - g12 * h1) * idet);
            flow[x * 2 + 1] = (float)((g22 * h1 - g12 * h2) * idet);
        }
        y1 = y == h - 1 ? h : y - bsz;
        if (upd && (y1 == h || y1 >= y0 + min_stripe)) {
            of_fb_update_matrices(R0, R1, flowa, Ma, w, h, y0, y1);
            y0 = y1;
        }
    }
    ALWAN_FREE((void *)srow);
    ALWAN_FREE(vb);
    return 1;
}

/* resize a w x h x 2 flow to ow x oh (INTER_LINEAR, or INTER_AREA when area) and scale it
 * as Mat *= double does (convertTo: fma with the float scale). 0 on no memory. */
static int of_fb_resize_flow(float *out, int ow, int oh, float const *in, int iw, int ih, float scale, int area) {
    size_t const no = (size_t)ow * (size_t)oh, ni = (size_t)iw * (size_t)ih;
    float *pi = (float *)of_alloc(ni * 2 + no * 2, sizeof(float)), *po;
    size_t i;
    int c, ok = 1;
    if (!pi) return 0;
    po = pi + ni * 2;
    if (area) {
        if (alwan__cv_resize(po, (size_t)ow * 2 * sizeof(float), (size_t)ow, (size_t)oh, in, (size_t)iw * 2 * sizeof(float), 2,
                             (size_t)iw, (size_t)ih, 1, 1) != ALWAN_OK)
            ok = 0;
        else
            for (i = 0; i < no * 2; i++) out[i] = ALWAN_FMA_CR_F32(po[i], scale, 0.0f);
        ALWAN_FREE(pi);
        return ok;
    }
    for (c = 0; c < 2; c++) {
        for (i = 0; i < ni; i++) pi[(size_t)c * ni + i] = in[i * 2 + (size_t)c];
        if (!alwan__cv_resize_f32_plane(po + (size_t)c * no, ow, oh, pi + (size_t)c * ni, iw, ih)) ok = 0;
    }
    if (ok) {
        for (i = 0; i < no; i++) {
            out[i * 2] = ALWAN_FMA_CR_F32(po[i], scale, 0.0f);
            out[i * 2 + 1] = ALWAN_FMA_CR_F32(po[no + i], scale, 0.0f);
        }
    }
    ALWAN_FREE(pi);
    return ok;
}

/* FarnebackOpticalFlowImpl::calc on two packed float images; flow0 is W0 x H0 x 2 (read
 * first with use_initial_flow) */
static alwan_status of_farneback(float *flow0, float const *prev0, float const *next0, int W0, int H0,
                                 alwan_optical_flow_params const *p) {
    double const pyr = p && p->pyr_scale != 0.0 ? p->pyr_scale : 0.5;
    int levels = p && p->levels != 0 ? (p->levels < 0 ? 0 : (int)p->levels) : 5;
    int const win = p && p->window_size != 0 ? (int)p->window_size : 13;
    int const iters = p && p->iterations != 0 ? (int)p->iterations : 10;
    int const polyn = p && p->poly_n != 0 ? (int)p->poly_n : 5;
    double const polys = p && p->poly_sigma != 0.0 ? p->poly_sigma : 1.1;
    int const gauss = p ? p->gaussian_window != 0 : 0, init = p ? p->use_initial_flow != 0 : 0;
    int const min_size = 32;
    size_t const N0 = (size_t)W0 * (size_t)H0;
    float *prevflow = NULL, *flow = NULL, *fimg = NULL, *I = NULL, *R0 = NULL, *R1 = NULL, *M = NULL;
    alwan_status st = ALWAN_OK;
    int k, i, have_prev = 0, pw = 0, ph = 0;
    double scale;

    if (!(pyr > 0.0 && pyr < 1.0)) return ALWAN_E_INVALID;
    if (win < 1 || win > 255 || iters < 1 || iters > 10000) return ALWAN_E_INVALID;
    if (polyn != 5 && polyn != 7) return ALWAN_E_INVALID;
    if (!(polys > 0.0 && polys < 1e6)) return ALWAN_E_INVALID;
    if (levels > 64) levels = 64;
    for (k = 0, scale = 1; k < levels; k++) {
        scale *= pyr;
        if (W0 * scale < min_size || H0 * scale < min_size) break;
    }
    levels = k;
    prevflow = (float *)of_alloc(N0 * 2, sizeof(float));
    flow = (float *)of_alloc(N0 * 2, sizeof(float));
    fimg = (float *)of_alloc(N0, sizeof(float));
    I = (float *)of_alloc(N0, sizeof(float));
    R0 = (float *)of_alloc(N0 * 5, sizeof(float));
    R1 = (float *)of_alloc(N0 * 5, sizeof(float));
    M = (float *)of_alloc(N0 * 5, sizeof(float));
    if (!prevflow || !flow || !fimg || !I || !R0 || !R1 || !M) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    for (k = levels; k >= 0; k--) {
        double sigma;
        int sz, width, height;
        float *fl;
        for (i = 0, scale = 1; i < k; i++) scale *= pyr;
        sigma = (1. / scale - 1) * 0.5;
        sz = of_round(sigma * 5) | 1;
        if (sz < 3) sz = 3;
        width = of_round(W0 * scale);
        height = of_round(H0 * scale);
        if (width < 1 || height < 1) {
            st = ALWAN_E_RANGE;
            goto done;
        }
        fl = k > 0 ? flow : flow0;
        if (!have_prev) {
            if (init) {
                if (k > 0 && !of_fb_resize_flow(fl, width, height, flow0, W0, H0, (float)scale, 1)) {
                    st = ALWAN_E_NOMEM;
                    goto done;
                }
                /* at the finest level resize is a copy and the scale is 1 */
            } else {
                memset(fl, 0, (size_t)width * (size_t)height * 2 * sizeof(float));
            }
        } else if (!of_fb_resize_flow(fl, width, height, prevflow, pw, ph, (float)(1. / pyr), 0)) {
            st = ALWAN_E_NOMEM;
            goto done;
        }
        for (i = 0; i < 2; i++) {
            if (!of_cv_gaussian_blur(fimg, i == 0 ? prev0 : next0, W0, H0, sz, sigma) ||
                !alwan__cv_resize_f32_plane(I, width, height, fimg, W0, H0) ||
                !of_fb_polyexp(i == 0 ? R0 : R1, I, width, height, polyn, polys)) {
                st = ALWAN_E_NOMEM;
                goto done;
            }
        }
        of_fb_update_matrices(R0, R1, fl, M, width, height, 0, height);
        for (i = 0; i < iters; i++) {
            int const ok = gauss ? of_fb_flow_gauss(R0, R1, fl, M, width, height, win, i < iters - 1)
                                 : of_fb_flow_blur(R0, R1, fl, M, width, height, win, i < iters - 1);
            if (!ok) {
                st = ALWAN_E_NOMEM;
                goto done;
            }
        }
        memcpy(prevflow, fl, (size_t)width * (size_t)height * 2 * sizeof(float));
        pw = width;
        ph = height;
        have_prev = 1;
    }
done:
    if (prevflow) ALWAN_FREE(prevflow);
    if (flow) ALWAN_FREE(flow);
    if (fimg) ALWAN_FREE(fimg);
    if (I) ALWAN_FREE(I);
    if (R0) ALWAN_FREE(R0);
    if (R1) ALWAN_FREE(R1);
    if (M) ALWAN_FREE(M);
    return st;
}

/* ==================================================================================== */
/* scikit-image 0.26: optical_flow_tvl1 and optical_flow_ilk                             */
/* ==================================================================================== */

/* Every plane is a packed w x h array of doubles. f32 runs numpy's float32 arithmetic: each
 * operation in double rounded to float, which is the float operation (a double holds the
 * exact sum, difference or product of two floats, and the quotient and square root round
 * correctly twice). */
typedef struct {
    int f32;
    size_t w, h;
} sk_ctx;

static double sk_r(sk_ctx const *c, double v) {
    return c->f32 ? (double)(float)v : v;
}

/* scipy's 'mirror' (d c b | a b c d | c b a) */
static size_t sk_mirror(ptrdiff_t i, size_t n) {
    ptrdiff_t const p = (ptrdiff_t)n * 2 - 2;
    if (n == 1) return 0;
    if (i < 0) i = -i;
    i %= p;
    if (i >= (ptrdiff_t)n) i = p - i;
    return (size_t)i;
}

/* scipy's 'reflect' (d c b a | a b c d | d c b a) */
static size_t sk_reflect(ptrdiff_t i, size_t n) {
    ptrdiff_t const p = (ptrdiff_t)n * 2;
    if (i < 0) i = -i - 1;
    i %= p;
    if (i >= (ptrdiff_t)n) i = p - 1 - i;
    return (size_t)i;
}

static size_t sk_clampi(ptrdiff_t i, size_t n) {
    if (i < 0) return 0;
    if (i >= (ptrdiff_t)n) return n - 1;
    return (size_t)i;
}

/* numpy's pairwise sum (blocks of 128 split in halves, eight accumulators), each addition
 * rounded in the precision */
static double sk_pw_sum(sk_ctx const *c, double const *a, size_t n) {
    double r[8], res = 0.0;
    size_t i, j;
    if (n < 8) {
        for (i = 0; i < n; i++) res = sk_r(c, res + a[i]);
        return res;
    }
    if (n > 128) {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return sk_r(c, sk_pw_sum(c, a, n2) + sk_pw_sum(c, a + n2, n - n2));
    }
    for (j = 0; j < 8; j++) r[j] = a[j];
    for (i = 8; i < n - (n % 8); i += 8)
        for (j = 0; j < 8; j++) r[j] = sk_r(c, r[j] + a[i + j]);
    res = sk_r(c, sk_r(c, sk_r(c, r[0] + r[1]) + sk_r(c, r[2] + r[3])) + sk_r(c, sk_r(c, r[4] + r[5]) + sk_r(c, r[6] + r[7])));
    for (; i < n; i++) res = sk_r(c, res + a[i]);
    return res;
}

static void sk_minmax(double const *a, size_t n, double *mn, double *mx) {
    size_t i;
    *mn = *mx = a[0];
    for (i = 1; i < n; i++) {
        if (a[i] < *mn) *mn = a[i];
        if (a[i] > *mx) *mx = a[i];
    }
}

/* scipy.ndimage.gaussian_filter through alwan_filter, which reproduces it pass for pass */
static int sk_gaussian(sk_ctx const *c, double *dst, double const *src, size_t w, size_t h, double sigma,
                       alwan_filter_border border) {
    alwan_filter_params fp;
    size_t const n = w * h;
    size_t i;
    alwan_status st;
    memset(&fp, 0, sizeof(fp));
    fp.sigma = sigma;
    fp.border = border;
    if (!c->f32) return alwan__filter_run(dst, w * sizeof(double), src, w * sizeof(double), 1, w, h, ALWAN_FILTER_GAUSSIAN, &fp, 0) == ALWAN_OK;
    {
        float *t = (float *)of_alloc(n, sizeof(float));
        if (!t) return 0;
        for (i = 0; i < n; i++) t[i] = (float)src[i];
        st = alwan__filter_run(t, w * sizeof(float), t, w * sizeof(float), 1, w, h, ALWAN_FILTER_GAUSSIAN, &fp, 1);
        for (i = 0; i < n; i++) dst[i] = (double)t[i];
        ALWAN_FREE(t);
        return st == ALWAN_OK;
    }
}

/* scipy.ndimage.uniform_filter(size, mode='mirror'): uniform_filter1d down the columns then
 * along the rows, each a running sum in double over the extended line divided by the size
 * at every pixel, stored in the precision. line holds n + size values. */
static void sk_uniform_line(sk_ctx const *c, double *out, size_t ostep, double const *in, size_t istep, size_t n,
                            size_t size, double *line) {
    size_t const r = size / 2;
    size_t ll;
    double tmp = 0.0;
    for (ll = 0; ll < n + size - 1; ll++) line[ll] = in[sk_mirror((ptrdiff_t)ll - (ptrdiff_t)r, n) * istep];
    for (ll = 0; ll < size; ll++) tmp += line[ll];
    out[0] = sk_r(c, tmp / (double)size);
    for (ll = 1; ll < n; ll++) {
        tmp += line[ll + size - 1] - line[ll - 1];
        out[ll * ostep] = sk_r(c, tmp / (double)size);
    }
}

static int sk_uniform(sk_ctx const *c, double *dst, double const *src, size_t w, size_t h, size_t size) {
    size_t const nl = (w > h ? w : h) + size;
    double *line = (double *)of_alloc(nl + w * h, sizeof(double)), *t;
    size_t x, y;
    if (!line) return 0;
    t = line + nl;
    for (x = 0; x < w; x++) sk_uniform_line(c, t + x, w, src + x, w, h, size, line);
    for (y = 0; y < h; y++) sk_uniform_line(c, dst + y * w, 1, t + y * w, 1, w, size, line);
    ALWAN_FREE(line);
    return 1;
}

/* scipy.ndimage.median_filter(size=3), mode 'reflect', on one plane */
static void sk_median3(double *dst, double const *src, size_t w, size_t h) {
    size_t x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            double v[9];
            int i, j, k = 0;
            for (j = -1; j <= 1; j++)
                for (i = -1; i <= 1; i++)
                    v[k++] = src[sk_reflect((ptrdiff_t)y + j, h) * w + sk_reflect((ptrdiff_t)x + i, w)];
            for (i = 1; i < 9; i++) {
                double const t = v[i];
                j = i - 1;
                while (j >= 0 && v[j] > t) {
                    v[j + 1] = v[j];
                    j--;
                }
                v[j + 1] = t;
            }
            dst[y * w + x] = v[4];
        }
    }
}

/* np.gradient: central differences halved inside, one-sided at the ends; gr down the
 * columns (axis 0), gc along the rows */
static void sk_gradient(sk_ctx const *c, double *gr, double *gc, double const *a, size_t w, size_t h) {
    size_t x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            size_t const i = y * w + x;
            if (h < 2) gr[i] = 0.0;
            else if (y == 0) gr[i] = sk_r(c, a[i + w] - a[i]);
            else if (y == h - 1) gr[i] = sk_r(c, a[i] - a[i - w]);
            else gr[i] = sk_r(c, sk_r(c, a[i + w] - a[i - w]) / 2.0);
            if (w < 2) gc[i] = 0.0;
            else if (x == 0) gc[i] = sk_r(c, a[i + 1] - a[i]);
            else if (x == w - 1) gc[i] = sk_r(c, a[i] - a[i - 1]);
            else gc[i] = sk_r(c, sk_r(c, a[i + 1] - a[i - 1]) / 2.0);
        }
    }
}

/* skimage.transform.warp(image, coords, mode='edge'): map_coordinates order 1 with
 * mode 'nearest' at R(flow + grid) (the edge pixel repeated past the image), then clipped
 * to the image's range */
static void sk_warp(sk_ctx const *c, double *out, double const *img, double const *fr, double const *fc, size_t w, size_t h) {
    double mn, mx;
    size_t x, y;
    sk_minmax(img, w * h, &mn, &mx);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            size_t const i = y * w + x;
            double cr = sk_r(c, fr[i] + (double)y), cc = sk_r(c, fc[i] + (double)x);
            double sr, sc, tr, tc, wr[2], wc[2], t = 0.0;
            ptrdiff_t r0, c0;
            int a, b;
            /* scipy weighs the raw coordinate and clamps only the indices; a point beyond
             * 1e9 pixels is pulled in so the index stays representable */
            if (!(cr > -1e9)) cr = -1e9;
            else if (cr > 1e9) cr = 1e9;
            if (!(cc > -1e9)) cc = -1e9;
            else if (cc > 1e9) cc = 1e9;
            sr = ALWAN_FLOOR_F64(cr);
            sc = ALWAN_FLOOR_F64(cc);
            tr = cr - sr;
            tc = cc - sc;
            wr[0] = 1.0 - tr;
            wr[1] = 1.0 - wr[0];
            wc[0] = 1.0 - tc;
            wc[1] = 1.0 - wc[0];
            r0 = (ptrdiff_t)sr;
            c0 = (ptrdiff_t)sc;
            for (a = 0; a < 2; a++)
                for (b = 0; b < 2; b++)
                    t += img[sk_clampi(r0 + a, h) * w + sk_clampi(c0 + b, w)] * wr[a] * wc[b];
            t = sk_r(c, t);
            out[i] = t < mn ? mn : t > mx ? mx : t;
        }
    }
}

/* pyramid_reduce(downscale 2): gaussian_filter(sigma 2/3, mode 'reflect'), then resize to
 * ceil(n / 2) with ndi.zoom(order 1, mode 'mirror', grid_mode) and clipped to the smoothed
 * image's range. dst is ceil(w / 2) x ceil(h / 2). */
static int sk_reduce(sk_ctx const *c, double *dst, double const *src, size_t w, size_t h) {
    size_t const ow = (w + 1) / 2, oh = (h + 1) / 2;
    double *s = (double *)of_alloc(w * h, sizeof(double));
    double const zy = (double)h / (double)oh, zx = (double)w / (double)ow;
    double mn, mx;
    size_t x, y;
    if (!s) return 0;
    if (!sk_gaussian(c, s, src, w, h, 2 * 2 / 6.0, ALWAN_FILTER_BORDER_REFLECT)) {
        ALWAN_FREE(s);
        return 0;
    }
    sk_minmax(s, w * h, &mn, &mx);
    for (y = 0; y < oh; y++) {
        double const cy = ((double)y + 0.5) * zy - 0.5;
        double const fy = ALWAN_FLOOR_F64(cy);
        double wy[2];
        ptrdiff_t const y0 = (ptrdiff_t)fy;
        wy[0] = 1.0 - (cy - fy);
        wy[1] = 1.0 - wy[0];
        for (x = 0; x < ow; x++) {
            double const cx = ((double)x + 0.5) * zx - 0.5;
            double const fx = ALWAN_FLOOR_F64(cx);
            double wx[2], t = 0.0;
            ptrdiff_t const x0 = (ptrdiff_t)fx;
            int a, b;
            wx[0] = 1.0 - (cx - fx);
            wx[1] = 1.0 - wx[0];
            for (a = 0; a < 2; a++)
                for (b = 0; b < 2; b++) t += s[sk_mirror(y0 + a, h) * w + sk_mirror(x0 + b, w)] * wy[a] * wx[b];
            t = sk_r(c, t);
            dst[y * ow + x] = t < mn ? mn : t > mx ? mx : t;
        }
    }
    ALWAN_FREE(s);
    return 1;
}

/* _resize_flow: ndi.zoom(order 0, mode 'nearest') to ow x oh, each component times its
 * axis' n / o in the precision */
static void sk_resize_flow(sk_ctx const *c, double *dr, double *dc, size_t ow, size_t oh, double const *sr, double const *sc,
                           size_t w, size_t h) {
    double const zy = oh > 1 ? (double)(h - 1) / (double)(oh - 1) : 1.0;
    double const zx = ow > 1 ? (double)(w - 1) / (double)(ow - 1) : 1.0;
    double const fy = sk_r(c, (double)oh / (double)h), fx = sk_r(c, (double)ow / (double)w);
    size_t x, y;
    for (y = 0; y < oh; y++) {
        double cy = (double)y * zy;
        size_t iy;
        if (cy > (double)(h - 1)) cy = (double)(h - 1);
        iy = sk_clampi((ptrdiff_t)ALWAN_FLOOR_F64(cy + 0.5), h);
        for (x = 0; x < ow; x++) {
            double cx = (double)x * zx;
            size_t ix;
            if (cx > (double)(w - 1)) cx = (double)(w - 1);
            ix = sk_clampi((ptrdiff_t)ALWAN_FLOOR_F64(cx + 0.5), w);
            dr[y * ow + x] = sk_r(c, fy * sr[iy * w + ix]);
            dc[y * ow + x] = sk_r(c, fx * sc[iy * w + ix]);
        }
    }
}

typedef struct {
    double attachment, tightness, tol;
    size_t num_iter, num_warp, radius;
    int prefilter, gaussian;
} sk_params;

/* _tvl1 on one level: fr, fc in and out */
static int sk_tvl1(sk_ctx const *c, double const *ref, double const *mov, double *fr, double *fc, sk_params const *p) {
    size_t const w = c->w, h = c->h, n = w * h;
    double const f0 = sk_r(c, p->attachment * p->tightness), f1 = sk_r(c, 0.25 / p->tightness);
    double const tol = sk_r(c, p->tol * (double)n);
    double *buf = (double *)of_alloc(n * 19, sizeof(double));
    double *iw, *gr, *gc, *NI, *rho0, *ar, *ac, *g0, *g1, *p00, *p01, *p10, *p11, *d, *nrm, *prev, *tr;
    size_t wi, it, i, x, y;
    int comp, ri;
    if (!buf) return 0;
    iw = buf;
    gr = iw + n;
    gc = gr + n;
    NI = gc + n;
    rho0 = NI + n;
    ar = rho0 + n;
    ac = ar + n;
    g0 = ac + n;
    g1 = g0 + n;
    p00 = g1 + n;
    p01 = p00 + n;
    p10 = p01 + n;
    p11 = p10 + n;
    d = p11 + n;
    nrm = d + n;
    prev = nrm + n; /* 2 n: the row then the column component */
    tr = prev + 2 * n;
    for (i = 0; i < n * 6; i++) g0[i] = 0.0; /* g and proj start at zero on each level */
    for (wi = 0; wi < p->num_warp; wi++) {
        if (p->prefilter) {
            /* flow_previous keeps the flow before the median */
            memcpy(prev, fr, n * sizeof(double));
            memcpy(prev + n, fc, n * sizeof(double));
            sk_median3(tr, fr, w, h);
            memcpy(fr, tr, n * sizeof(double));
            sk_median3(tr, fc, w, h);
            memcpy(fc, tr, n * sizeof(double));
        }
        sk_warp(c, iw, mov, fr, fc, w, h);
        sk_gradient(c, gr, gc, iw, w, h);
        for (i = 0; i < n; i++) {
            NI[i] = sk_r(c, sk_r(c, gr[i] * gr[i]) + sk_r(c, gc[i] * gc[i]));
            if (NI[i] == 0.0) NI[i] = 1.0;
            rho0[i] = sk_r(c, sk_r(c, iw[i] - ref[i]) - sk_r(c, sk_r(c, gr[i] * fr[i]) + sk_r(c, gc[i] * fc[i])));
        }
        if (!p->prefilter) {
            memcpy(prev, fr, n * sizeof(double));
            memcpy(prev + n, fc, n * sizeof(double));
        }
        for (it = 0; it < p->num_iter; it++) {
            /* data term, in place on the current flow */
            for (i = 0; i < n; i++) {
                double const rho = sk_r(c, rho0[i] + sk_r(c, sk_r(c, gr[i] * fr[i]) + sk_r(c, gc[i] * fc[i])));
                if (ALWAN_ABS_F64(rho) <= sk_r(c, f0 * NI[i])) {
                    fr[i] = sk_r(c, fr[i] - sk_r(c, sk_r(c, rho * gr[i]) / NI[i]));
                    fc[i] = sk_r(c, fc[i] - sk_r(c, sk_r(c, rho * gc[i]) / NI[i]));
                } else {
                    double const s = rho > 0.0 ? f0 : rho < 0.0 ? -f0 : 0.0;
                    fr[i] = sk_r(c, fr[i] - sk_r(c, s * gr[i]));
                    fc[i] = sk_r(c, fc[i] - sk_r(c, s * gc[i]));
                }
            }
            if (it == 0 && !p->prefilter) {
                /* flow_previous is the same array as the flow the first data step updated */
                memcpy(prev, fr, n * sizeof(double));
                memcpy(prev + n, fc, n * sizeof(double));
            }
            memcpy(ar, fr, n * sizeof(double));
            memcpy(ac, fc, n * sizeof(double));
            /* regularisation, component by component */
            for (comp = 0; comp < 2; comp++) {
                double *cur = comp == 0 ? fr : fc;
                double const *aux = comp == 0 ? ar : ac;
                double *pa = comp == 0 ? p00 : p10, *pb = comp == 0 ? p01 : p11;
                for (ri = 0; ri < 2; ri++) {
                    for (y = 0; y + 1 < h; y++)
                        for (x = 0; x < w; x++) g0[y * w + x] = sk_r(c, cur[(y + 1) * w + x] - cur[y * w + x]);
                    for (y = 0; y < h; y++)
                        for (x = 0; x + 1 < w; x++) g1[y * w + x] = sk_r(c, cur[y * w + x + 1] - cur[y * w + x]);
                    for (i = 0; i < n; i++) {
                        double t = sk_r(c, ALWAN_SQRT_F64(sk_r(c, sk_r(c, g0[i] * g0[i]) + sk_r(c, g1[i] * g1[i]))));
                        t = sk_r(c, t * f1);
                        nrm[i] = sk_r(c, t + 1.0);
                    }
                    for (i = 0; i < n; i++) {
                        pa[i] = sk_r(c, sk_r(c, pa[i] - sk_r(c, 0.25 * g0[i])) / nrm[i]);
                        pb[i] = sk_r(c, sk_r(c, pb[i] - sk_r(c, 0.25 * g1[i])) / nrm[i]);
                    }
                    for (i = 0; i < n; i++) d[i] = -sk_r(c, pa[i] + pb[i]);
                    for (y = 1; y < h; y++)
                        for (x = 0; x < w; x++) d[y * w + x] = sk_r(c, d[y * w + x] + pa[(y - 1) * w + x]);
                    for (y = 0; y < h; y++)
                        for (x = 1; x < w; x++) d[y * w + x] = sk_r(c, d[y * w + x] + pb[y * w + x - 1]);
                    for (i = 0; i < n; i++) cur[i] = sk_r(c, aux[i] + d[i]);
                }
            }
        }
        /* the stopping test: the squared change, summed as numpy sums the (2, h, w) array */
        for (i = 0; i < n; i++) {
            double const a = sk_r(c, prev[i] - fr[i]), b = sk_r(c, prev[n + i] - fc[i]);
            tr[i] = sk_r(c, a * a);
            nrm[i] = sk_r(c, b * b);
        }
        memcpy(tr + n, nrm, n * sizeof(double));
        if (sk_pw_sum(c, tr, 2 * n) < tol) break;
    }
    ALWAN_FREE(buf);
    return 1;
}

/* numpy.linalg.det and solve on one symmetric 2 x 2 system [[a, b], [b, d]] x = (e, f):
 * LAPACK's LU with partial pivoting (the multiplier by the pivot's reciprocal, as OpenBLAS's
 * getf2 scales), the determinant through its logarithm as numpy forms it. numpy 2.3 gives a
 * float32 system the float64 answer rounded to float (measured on 20000 random systems:
 * every solution and determinant), so the LU runs in double for both precisions. */
static void sk_solve2(sk_ctx const *c, double a, double b, double d, double e, double f, double *x0, double *x1) {
    double u00 = a, u01 = b, m10 = b, m11 = d, y0 = e, y1 = f, l, u11, det, sign = 1.0;
    if (ALWAN_ABS_F64(b) > ALWAN_ABS_F64(a)) {
        u00 = b;
        u01 = d;
        m10 = a;
        m11 = b;
        y0 = f;
        y1 = e;
        sign = -1.0;
    }
    if (u00 == 0.0) {
        *x0 = *x1 = 0.0;
        return;
    }
    l = m10 * (1.0 / u00);
    u11 = m11 - l * u01;
    if (u11 == 0.0) det = 0.0;
    else {
        if (u00 < 0.0) sign = -sign;
        if (u11 < 0.0) sign = -sign;
        det = sign * ALWAN_EXP_F64(ALWAN_LN_F64(ALWAN_ABS_F64(u00)) + ALWAN_LN_F64(ALWAN_ABS_F64(u11)));
    }
    if (ALWAN_ABS_F64(sk_r(c, det)) < sk_r(c, 1e-14)) {
        *x0 = *x1 = 0.0;
        return;
    }
    y1 = y1 - l * y0;
    *x1 = y1 / u11;
    *x0 = sk_r(c, (y0 - *x1 * u01) / u00);
    *x1 = sk_r(c, *x1);
}

/* _ilk on one level */
static int sk_ilk(sk_ctx const *c, double const *ref, double const *mov, double *fr, double *fc, sk_params const *p) {
    size_t const w = c->w, h = c->h, n = w * h, size = 2 * p->radius + 1;
    double *buf = (double *)of_alloc(n * 10, sizeof(double));
    double *mw, *gr, *gc, *err, *t, *A00, *A01, *A11, *b0, *b1;
    size_t wi, i;
    int ok = 1;
    if (!buf) return 0;
    mw = buf;
    gr = mw + n;
    gc = gr + n;
    err = gc + n;
    t = err + n;
    A00 = t + n;
    A01 = A00 + n;
    A11 = A01 + n;
    b0 = A11 + n;
    b1 = b0 + n;
    for (wi = 0; wi < p->num_warp && ok; wi++) {
        int k;
        if (p->prefilter) {
            sk_median3(t, fr, w, h);
            memcpy(fr, t, n * sizeof(double));
            sk_median3(t, fc, w, h);
            memcpy(fc, t, n * sizeof(double));
        }
        sk_warp(c, mw, mov, fr, fc, w, h);
        sk_gradient(c, gr, gc, mw, w, h);
        for (i = 0; i < n; i++)
            err[i] = sk_r(c, sk_r(c, sk_r(c, sk_r(c, gr[i] * fr[i]) + sk_r(c, gc[i] * fc[i])) + ref[i]) - mw[i]);
        for (k = 0; k < 5 && ok; k++) {
            double *dst = k == 0 ? A00 : k == 1 ? A01 : k == 2 ? A11 : k == 3 ? b0 : b1;
            double const *u = k == 0 || k == 1 || k == 3 ? gr : gc;
            double const *v = k == 0 ? gr : k == 1 || k == 2 ? gc : err;
            for (i = 0; i < n; i++) t[i] = sk_r(c, u[i] * v[i]);
            ok = p->gaussian ? sk_gaussian(c, dst, t, w, h, (double)size / 4, ALWAN_FILTER_BORDER_MIRROR)
                             : sk_uniform(c, dst, t, w, h, size);
        }
        if (!ok) break;
        for (i = 0; i < n; i++) sk_solve2(c, A00[i], A01[i], A11[i], b0[i], b1[i], &fr[i], &fc[i]);
    }
    ALWAN_FREE(buf);
    return ok;
}

/* _coarse_to_fine: pyramids of both images (at most 10 levels, reduced while the smaller
 * side is over 32), the solver from the coarsest level up, the flow resized between them */
static alwan_status sk_coarse_to_fine(int f32, double *fr, double *fc, double const *ref, double const *mov, size_t w, size_t h,
                                      int tvl1, sk_params const *p) {
    double *lv[2][10];
    size_t lw[10], lh[10];
    int nl = 1, k, j;
    alwan_status st = ALWAN_OK;
    double *cr = NULL, *cc = NULL;
    sk_ctx c;
    c.f32 = f32;
    lv[0][0] = (double *)ref;
    lv[1][0] = (double *)mov;
    lw[0] = w;
    lh[0] = h;
    while (nl < 10 && (lw[nl - 1] < lh[nl - 1] ? lw[nl - 1] : lh[nl - 1]) > 32) {
        lw[nl] = (lw[nl - 1] + 1) / 2;
        lh[nl] = (lh[nl - 1] + 1) / 2;
        for (j = 0; j < 2; j++) {
            c.w = lw[nl - 1];
            c.h = lh[nl - 1];
            lv[j][nl] = (double *)of_alloc(lw[nl] * lh[nl], sizeof(double));
            if (!lv[j][nl] || !sk_reduce(&c, lv[j][nl], lv[j][nl - 1], lw[nl - 1], lh[nl - 1])) {
                if (lv[j][nl]) ALWAN_FREE(lv[j][nl]);
                if (j == 1) ALWAN_FREE(lv[0][nl]);
                st = ALWAN_E_NOMEM;
                goto done;
            }
        }
        nl++;
    }
    cr = (double *)of_alloc(w * h * 2, sizeof(double));
    if (!cr) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    cc = cr + w * h;
    for (k = nl - 1; k >= 0; k--) {
        double *r = k == 0 ? fr : cr, *cl = k == 0 ? fc : cc;
        int ok;
        if (k == nl - 1) {
            memset(r, 0, lw[k] * lh[k] * sizeof(double));
            memset(cl, 0, lw[k] * lh[k] * sizeof(double));
        } else {
            double *tmp = (double *)of_alloc(lw[k] * lh[k] * 2, sizeof(double));
            if (!tmp) {
                st = ALWAN_E_NOMEM;
                goto done;
            }
            sk_resize_flow(&c, tmp, tmp + lw[k] * lh[k], lw[k], lh[k], cr, cc, lw[k + 1], lh[k + 1]);
            memcpy(r, tmp, lw[k] * lh[k] * sizeof(double));
            memcpy(cl, tmp + lw[k] * lh[k], lw[k] * lh[k] * sizeof(double));
            ALWAN_FREE(tmp);
        }
        c.w = lw[k];
        c.h = lh[k];
        ok = tvl1 ? sk_tvl1(&c, lv[0][k], lv[1][k], r, cl, p) : sk_ilk(&c, lv[0][k], lv[1][k], r, cl, p);
        if (!ok) {
            st = ALWAN_E_NOMEM;
            goto done;
        }
    }
done:
    for (k = 1; k < nl; k++) {
        ALWAN_FREE(lv[0][k]);
        ALWAN_FREE(lv[1][k]);
    }
    if (cr) ALWAN_FREE(cr);
    return st;
}

/* ==================================================================================== */
/* Public entry points                                                                   */
/* ==================================================================================== */

/* kind 0 f64, 1 f32, 2 u8 */
static double of_read(void const *img, size_t rs, size_t x, size_t y, int kind) {
    unsigned char const *row = (unsigned char const *)img + y * rs;
    if (kind == 0) return ((double const *)(void const *)row)[x];
    if (kind == 1) return (double)((float const *)(void const *)row)[x];
    return (double)row[x];
}

static alwan_status of_run(void *flow, size_t flow_rs, void const *ref, size_t ref_rs, void const *mov, size_t mov_rs, size_t W,
                           size_t H, alwan_optical_flow_method method, alwan_optical_flow_params const *p, int kind) {
    size_t const elem = kind == 0 ? sizeof(double) : kind == 1 ? sizeof(float) : 1;
    size_t const oelem = kind == 0 ? sizeof(double) : sizeof(float);
    size_t const N = W * H;
    size_t x, y;
    alwan_status st = ALWAN_OK;
    if (!flow || !ref || !mov || W < 2 || H < 2) return ALWAN_E_INVALID;
    if (method != ALWAN_OPTICAL_FLOW_TVL1 && method != ALWAN_OPTICAL_FLOW_ILK && method != ALWAN_OPTICAL_FLOW_FARNEBACK)
        return ALWAN_E_INVALID;
    if (N / W != H || W > (size_t)1 << 24 || H > (size_t)1 << 24) return ALWAN_E_RANGE;
    if (flow_rs == 0) flow_rs = W * 2 * oelem;
    if (ref_rs < W * elem || mov_rs < W * elem || flow_rs < W * 2 * oelem) return ALWAN_E_INVALID;
    if (p) {
        if (!isfinite(p->attachment) || !isfinite(p->tightness) || !isfinite(p->tolerance) || !isfinite(p->pyr_scale) ||
            !isfinite(p->poly_sigma))
            return ALWAN_E_RANGE;
        if (p->attachment < 0.0 || p->tightness < 0.0 || p->tolerance < 0.0) return ALWAN_E_INVALID;
        if (p->radius > 4096 || p->num_iter > 100000 || p->num_warp > 100000) return ALWAN_E_RANGE;
    }
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            if (!isfinite(of_read(ref, ref_rs, x, y, kind)) || !isfinite(of_read(mov, mov_rs, x, y, kind))) return ALWAN_E_RANGE;
        }
    }
    if (method == ALWAN_OPTICAL_FLOW_FARNEBACK) {
        float *buf = (float *)of_alloc(N * 4, sizeof(float));
        float *fr, *fm, *fl;
        if (!buf) return ALWAN_E_NOMEM;
        fr = buf;
        fm = fr + N;
        fl = fm + N;
        if (W > INT_MAX / 8 || H > INT_MAX / 8) {
            ALWAN_FREE(buf);
            return ALWAN_E_RANGE;
        }
        if (p && p->window_size != 0 && (p->window_size % 2 == 0 || p->window_size > 255)) {
            ALWAN_FREE(buf);
            return ALWAN_E_INVALID;
        }
        if (p && (p->iterations > 10000 || (p->pyr_scale != 0.0 && !(p->pyr_scale > 0.0 && p->pyr_scale < 1.0)) ||
                  p->poly_sigma < 0.0)) {
            ALWAN_FREE(buf);
            return ALWAN_E_INVALID;
        }
        for (y = 0; y < H; y++) {
            for (x = 0; x < W; x++) {
                fr[y * W + x] = (float)of_read(ref, ref_rs, x, y, kind);
                fm[y * W + x] = (float)of_read(mov, mov_rs, x, y, kind);
                if (p && p->use_initial_flow) {
                    unsigned char const *row = (unsigned char const *)flow + y * flow_rs;
                    if (kind == 0) {
                        fl[(y * W + x) * 2] = (float)((double const *)(void const *)row)[x * 2];
                        fl[(y * W + x) * 2 + 1] = (float)((double const *)(void const *)row)[x * 2 + 1];
                    } else {
                        fl[(y * W + x) * 2] = ((float const *)(void const *)row)[x * 2];
                        fl[(y * W + x) * 2 + 1] = ((float const *)(void const *)row)[x * 2 + 1];
                    }
                }
            }
        }
        st = of_farneback(fl, fr, fm, (int)W, (int)H, p);
        if (st == ALWAN_OK) {
            for (y = 0; y < H; y++) {
                unsigned char *row = (unsigned char *)flow + y * flow_rs;
                for (x = 0; x < W * 2; x++) {
                    if (kind == 0) ((double *)(void *)row)[x] = (double)fl[y * W * 2 + x];
                    else ((float *)(void *)row)[x] = fl[y * W * 2 + x];
                }
            }
        }
        ALWAN_FREE(buf);
        return st;
    }
    {
        int const f32 = kind != 0;
        double *buf = (double *)of_alloc(N * 4, sizeof(double));
        double *r, *m, *fr, *fc;
        sk_params sp;
        float const inv255 = (float)(1.0 / 255.0);
        if (!buf) return ALWAN_E_NOMEM;
        r = buf;
        m = r + N;
        fr = m + N;
        fc = fr + N;
        for (y = 0; y < H; y++) {
            for (x = 0; x < W; x++) {
                double a = of_read(ref, ref_rs, x, y, kind), b = of_read(mov, mov_rs, x, y, kind);
                if (kind == 2) {
                    /* img_as_float32: the values times 1/255 rounded to float, in float */
                    a = (double)(float)(a * (double)inv255);
                    b = (double)(float)(b * (double)inv255);
                }
                r[y * W + x] = a;
                m[y * W + x] = b;
            }
        }
        memset(&sp, 0, sizeof(sp));
        sp.attachment = p && p->attachment != 0.0 ? p->attachment : 15.0;
        sp.tightness = p && p->tightness != 0.0 ? p->tightness : 0.3;
        sp.tol = p && p->tolerance != 0.0 ? p->tolerance : 1e-4;
        sp.num_iter = p && p->num_iter != 0 ? p->num_iter : 10;
        sp.num_warp = p && p->num_warp != 0 ? p->num_warp : (method == ALWAN_OPTICAL_FLOW_TVL1 ? 5 : 10);
        sp.radius = p && p->radius != 0 ? p->radius : 7;
        sp.prefilter = p ? p->prefilter != 0 : 0;
        sp.gaussian = p ? p->gaussian != 0 : 0;
        st = sk_coarse_to_fine(f32, fr, fc, r, m, W, H, method == ALWAN_OPTICAL_FLOW_TVL1, &sp);
        if (st == ALWAN_OK) {
            for (y = 0; y < H; y++) {
                unsigned char *row = (unsigned char *)flow + y * flow_rs;
                for (x = 0; x < W; x++) {
                    /* scikit-image's (row, column) as (dx, dy) */
                    if (kind == 0) {
                        ((double *)(void *)row)[x * 2] = fc[y * W + x];
                        ((double *)(void *)row)[x * 2 + 1] = fr[y * W + x];
                    } else {
                        ((float *)(void *)row)[x * 2] = (float)fc[y * W + x];
                        ((float *)(void *)row)[x * 2 + 1] = (float)fr[y * W + x];
                    }
                }
            }
        }
        ALWAN_FREE(buf);
        return st;
    }
}

#if ALWAN_WITH_F32
alwan_status alwan_optical_flow_f32(alwan_f32 *flow, size_t flow_row_stride, alwan_f32 const *reference, size_t reference_row_stride,
                                    alwan_f32 const *moving, size_t moving_row_stride, size_t width, size_t height,
                                    alwan_optical_flow_method method, alwan_optical_flow_params const *params) {
    return of_run(flow, flow_row_stride, reference, reference_row_stride, moving, moving_row_stride, width, height, method, params, 1);
}
#endif

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_optical_flow_f64(alwan_f64 *flow, size_t flow_row_stride, alwan_f64 const *reference, size_t reference_row_stride,
                                    alwan_f64 const *moving, size_t moving_row_stride, size_t width, size_t height,
                                    alwan_optical_flow_method method, alwan_optical_flow_params const *params) {
    return of_run(flow, flow_row_stride, reference, reference_row_stride, moving, moving_row_stride, width, height, method, params, 0);
}
#endif

alwan_status alwan_optical_flow_u8(alwan_f32 *flow, size_t flow_row_stride, unsigned char const *reference, size_t reference_row_stride,
                                   unsigned char const *moving, size_t moving_row_stride, size_t width, size_t height,
                                   alwan_optical_flow_method method, alwan_optical_flow_params const *params) {
    return of_run(flow, flow_row_stride, reference, reference_row_stride, moving, moving_row_stride, width, height, method, params, 2);
}

/* the warp by a flow: alwan_warp's FIELD map, one lattice point a pixel. alwan_warp places
 * a source pixel's centre at its index + 0.5. */
static alwan_status of_warp_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t W, size_t H,
                                void const *flow, size_t flow_rs, int f32) {
    size_t const elem = f32 ? sizeof(float) : sizeof(double);
    double *field;
    alwan_warp_params wp;
    alwan_status st;
    size_t x, y;
    if (!out || !src || !flow || W == 0 || H == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (W * H / W != H) return ALWAN_E_RANGE;
    if (flow_rs == 0) flow_rs = W * 2 * elem;
    if (flow_rs < W * 2 * elem || src_rs < W * ch * elem || out_rs < W * ch * elem) return ALWAN_E_INVALID;
    field = (double *)of_alloc(W * H * 2, sizeof(double));
    if (!field) return ALWAN_E_NOMEM;
    for (y = 0; y < H; y++) {
        unsigned char const *row = (unsigned char const *)flow + y * flow_rs;
        for (x = 0; x < W; x++) {
            double dx, dy, sx, sy;
            if (f32) {
                dx = (double)((float const *)(void const *)row)[x * 2];
                dy = (double)((float const *)(void const *)row)[x * 2 + 1];
            } else {
                dx = ((double const *)(void const *)row)[x * 2];
                dy = ((double const *)(void const *)row)[x * 2 + 1];
            }
            if (!isfinite(dx) || !isfinite(dy)) {
                ALWAN_FREE(field);
                return ALWAN_E_INVALID;
            }
            sx = (double)x + dx;
            sy = (double)y + dy;
            sx = sx < 0.0 ? 0.0 : sx > (double)(W - 1) ? (double)(W - 1) : sx;
            sy = sy < 0.0 ? 0.0 : sy > (double)(H - 1) ? (double)(H - 1) : sy;
            field[(y * W + x) * 2] = sx + 0.5;
            field[(y * W + x) * 2 + 1] = sy + 0.5;
        }
    }
    memset(&wp, 0, sizeof(wp));
    wp.map = ALWAN_WARP_MAP_FIELD;
    wp.field = field;
    wp.field_row_stride = W * 2 * sizeof(double);
    wp.field_width = W;
    wp.field_height = H;
    st = alwan__warp_run(out, out_rs, W, H, src, src_rs, ch, W, H, ALWAN_WARP_BILINEAR, &wp, f32 ? 1 : 0);
    ALWAN_FREE(field);
    return st;
}

#if ALWAN_WITH_F32
alwan_status alwan_optical_flow_warp_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels,
                                         size_t width, size_t height, alwan_f32 const *flow, size_t flow_row_stride) {
    return of_warp_run(out, out_row_stride, src, src_row_stride, channels, width, height, flow, flow_row_stride, 1);
}
#endif

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_optical_flow_warp_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels,
                                         size_t width, size_t height, alwan_f64 const *flow, size_t flow_row_stride) {
    return of_warp_run(out, out_row_stride, src, src_row_stride, channels, width, height, flow, flow_row_stride, 0);
}
#endif

/* The Middlebury colour wheel, from Baker et al. 2011 (section 3.3 and figure 7): six ramps
 * between the primaries and secondaries, the steps chosen to be perceptually even. */
static void of_wheel(double col[55][3]) {
    static int const steps[6] = {15, 6, 4, 11, 13, 6};
    int seg, i, k = 0;
    for (seg = 0; seg < 6; seg++) {
        for (i = 0; i < steps[seg]; i++, k++) {
            double const up = ALWAN_FLOOR_F64(255.0 * i / steps[seg]), down = 255.0 - up;
            double r = 0, g = 0, b = 0;
            switch (seg) {
            case 0: r = 255; g = up; break;   /* red to yellow */
            case 1: r = down; g = 255; break; /* yellow to green */
            case 2: g = 255; b = up; break;   /* green to cyan */
            case 3: g = down; b = 255; break; /* cyan to blue */
            case 4: b = 255; r = up; break;   /* blue to magenta */
            default: b = down; r = 255; break; /* magenta to red */
            }
            col[k][0] = r / 255.0;
            col[k][1] = g / 255.0;
            col[k][2] = b / 255.0;
        }
    }
}

static alwan_status of_rgb_run(void *out, size_t out_rs, void const *flow, size_t flow_rs, size_t W, size_t H, double maxr, int f32) {
    size_t const elem = f32 ? sizeof(float) : sizeof(double);
    double wheel[55][3];
    size_t x, y;
    if (!out || !flow || W == 0 || H == 0) return ALWAN_E_INVALID;
    if (!(maxr >= 0.0) || !isfinite(maxr)) return ALWAN_E_INVALID;
    if (flow_rs == 0) flow_rs = W * 2 * elem;
    if (out_rs == 0) out_rs = W * 3 * elem;
    if (flow_rs < W * 2 * elem || out_rs < W * 3 * elem) return ALWAN_E_INVALID;
    of_wheel(wheel);
    if (maxr == 0.0) {
        for (y = 0; y < H; y++) {
            unsigned char const *row = (unsigned char const *)flow + y * flow_rs;
            for (x = 0; x < W; x++) {
                double const u = f32 ? (double)((float const *)(void const *)row)[x * 2] : ((double const *)(void const *)row)[x * 2];
                double const v = f32 ? (double)((float const *)(void const *)row)[x * 2 + 1] : ((double const *)(void const *)row)[x * 2 + 1];
                double const rr = ALWAN_SQRT_F64(u * u + v * v);
                if (isfinite(rr) && rr > maxr) maxr = rr;
            }
        }
        if (maxr == 0.0) maxr = 1.0;
    }
    for (y = 0; y < H; y++) {
        unsigned char const *row = (unsigned char const *)flow + y * flow_rs;
        unsigned char *orow = (unsigned char *)out + y * out_rs;
        for (x = 0; x < W; x++) {
            double u = f32 ? (double)((float const *)(void const *)row)[x * 2] : ((double const *)(void const *)row)[x * 2];
            double v = f32 ? (double)((float const *)(void const *)row)[x * 2 + 1] : ((double const *)(void const *)row)[x * 2 + 1];
            double c3[3];
            int ch;
            if (!isfinite(u) || !isfinite(v)) {
                c3[0] = c3[1] = c3[2] = 0.0; /* unknown flow is black */
            } else {
                double rad, a, fk, f;
                int k0, k1;
                u /= maxr;
                v /= maxr;
                rad = ALWAN_SQRT_F64(u * u + v * v);
                a = ALWAN_ATAN2_F64(-v, -u) / 3.14159265358979323846;
                if (a >= 1.0) a = -1.0; /* +x is the wheel's first step (red) whatever the sign of a zero v */
                fk = (a + 1.0) / 2.0 * 54.0;
                k0 = (int)ALWAN_FLOOR_F64(fk);
                if (k0 < 0) k0 = 0;
                if (k0 > 54) k0 = 54;
                k1 = k0 + 1 == 55 ? 0 : k0 + 1;
                f = fk - k0;
                for (ch = 0; ch < 3; ch++) {
                    double col = (1.0 - f) * wheel[k0][ch] + f * wheel[k1][ch];
                    if (rad <= 1.0) col = 1.0 - rad * (1.0 - col); /* less saturated towards the centre */
                    else col *= 0.75;                              /* out of range */
                    c3[ch] = col;
                }
            }
            for (ch = 0; ch < 3; ch++) {
                if (f32) ((float *)(void *)orow)[x * 3 + (size_t)ch] = (float)c3[ch];
                else ((double *)(void *)orow)[x * 3 + (size_t)ch] = c3[ch];
            }
        }
    }
    return ALWAN_OK;
}

#if ALWAN_WITH_F32
alwan_status alwan_optical_flow_to_rgb_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *flow, size_t flow_row_stride, size_t width,
                                           size_t height, double max_radius) {
    return of_rgb_run(out, out_row_stride, flow, flow_row_stride, width, height, max_radius, 1);
}
#endif

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_optical_flow_to_rgb_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *flow, size_t flow_row_stride, size_t width,
                                           size_t height, double max_radius) {
    return of_rgb_run(out, out_row_stride, flow, flow_row_stride, width, height, max_radius, 0);
}
#endif
