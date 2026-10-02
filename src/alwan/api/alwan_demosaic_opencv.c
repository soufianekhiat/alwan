/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Bayer demosaicing on 8- and 16-bit planes: OpenCV's VNG and edge-aware methods, and the
 * float methods of alwan_cfa_bayer_demosaic_{T} run on integer codes.
 *
 * ALWAN_DEMOSAIC_VNG_OPENCV and ALWAN_DEMOSAIC_EDGE_AWARE_OPENCV are ports of
 * Bayer2RGB_VNG_8u and Bayer2RGB_EdgeAware_T from OpenCV 5.0.0's
 * modules/imgproc/src/demosaicing.cpp (cv::demosaicing with COLOR_Bayer*2BGR_VNG and
 * COLOR_Bayer*2BGR_EA), kept to OpenCV's integer arithmetic so the codes are equal to
 * cv2's. That file carries two notices, kept here as their conditions ask (and in
 * licenses/OpenCV-imgproc-demosaicing-BSD.txt):
 *
 *   License Agreement For Open Source Computer Vision Library
 *   Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
 *   Copyright (C) 2009-2010, Willow Garage Inc., all rights reserved.
 *   Copyright (C) 2014, Itseez Inc., all rights reserved.
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
 *   Original code for Bayer->BGR/RGB conversion is provided by Dirk Schaefer
 *   from MD-Mathematische Dienste GmbH:
 *     Copyright (c) 2002, MD-Mathematische Dienste GmbH, Im Defdahl 5-10,
 *     44141 Dortmund, Germany, www.md-it.de
 *     Redistribution and use in source and binary forms, with or without modification,
 *     are permitted provided that the following conditions are met: Redistributions of
 *     source code must retain the above copyright notice, this list of conditions and the
 *     following disclaimer. Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. The name of
 *     Contributor may not be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 *     EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *     OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 *     SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *     EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *     SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *     INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *     STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 *     OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Two things the port keeps that a reading of the scalar code alone would miss:
 *   - OpenCV's x64 build runs VNG's colour step eight pixels at a time with SIMD, and
 *     divides there by 0.5f / ng in float, while its scalar columns read a table whose
 *     1/14 entry is 0.0714286f, a different float. Which columns take which path is fixed
 *     by the loop structure, so the port reproduces it (alwan__vng_simd_span).
 *   - Edge-aware fills column 0 by writing past the end of the previous row of a
 *     contiguous image; the port copies column 1 into column 0 directly, which is what
 *     that write does when the image is contiguous, as cv2's output always is.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stddef.h>
#include <string.h>

/* cvRound of a float on x64 (_mm_cvtss_si32 under the default rounding mode): to nearest,
 * ties to even. The values here are small, so the float subtraction is exact. */
static int alwan__dm_round_even(float v) {
    int t = (int)v;                       /* towards zero */
    float const frac = v - (float)t;
    if (frac > 0.5f) t += 1;
    else if (frac < -0.5f) t -= 1;
    else if (frac == 0.5f) { if (t & 1) t += 1; }
    else if (frac == -0.5f) { if (t & 1) t -= 1; }
    return t;
}

/* The image sizes as the int OpenCV's loops index with; the callers bound them first. */
static int alwan__dm_int(size_t v) {
    return (int)v;
}

static unsigned char alwan__dm_sat_u8(int v) {
    return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

/* The OpenCV Bayer code a pattern corresponds to. OpenCV names a pattern by the second
 * row's second and third sites, so RGGB is "BG". Returned as the two flags each method
 * derives from its code. */
static void alwan__dm_flags(alwan_cfa_pattern pattern, int *bg_or_gb, int *starts_green) {
    /* RGGB = BG, BGGR = RG, GRBG = GB, GBRG = GR */
    *bg_or_gb = (pattern == ALWAN_CFA_RGGB || pattern == ALWAN_CFA_GRBG);
    *starts_green = (pattern == ALWAN_CFA_GRBG || pattern == ALWAN_CFA_GBRG);
}

/* BORDER_REFLECT_101: ... 2 1 | 0 1 2 ... n-1 | n-2 n-3 ... */
static size_t alwan__dm_reflect101(ptrdiff_t i, size_t n) {
    ptrdiff_t const m = (ptrdiff_t)n;
    if (m == 1) return 0;
    while (i < 0 || i >= m) {
        if (i < 0) i = -i;
        if (i >= m) i = 2 * (m - 1) - i;
    }
    return (size_t)i;
}

/* The columns OpenCV's VNG colour step hands to its eight-pixel SIMD loop on one row:
 * [*first, *first + *count). */
static void alwan__vng_simd_span(int green_cell, int N, int *first, int *count) {
    int const limit = green_cell ? (N - 2 < 3 ? N - 2 : 3) : 2;
    *first = limit;
    *count = (limit <= N - 10) ? (((N - 10 - limit) / 8) + 1) * 8 : 0;
}

static alwan_status alwan__vng_u8(unsigned char *dst, size_t dst_stride, unsigned char const *src,
                                  size_t src_stride, size_t width, size_t height, alwan_cfa_pattern pattern) {
    static float const scale[] = { 0.f, 0.5f, 0.25f, 0.1666666666667f, 0.125f, 0.1f, 0.08333333333f,
                                   0.0714286f, 0.0625f };
    int const N = alwan__dm_int(width) + 4, H = alwan__dm_int(height) + 4;
    int const bstep = N;
    int const N2 = N * 2, N3 = N * 3, N4 = N * 4, N5 = N * 5, N6 = N * 6, N7 = N * 7;
    int const brows = 3, bcn = 7, bufstep = N7 * bcn;
    int bg_or_gb, starts_green, blueIdx, y;
    int greenCell0;
    size_t bytes_b, bytes_buf;
    unsigned char *bayer;
    unsigned short *buf;

    bytes_b = alwan_safe_array_size((size_t)N, (size_t)H);
    bytes_buf = alwan_safe_array_size((size_t)bufstep * (size_t)brows, sizeof(unsigned short));
    if (bytes_b == 0 || bytes_buf == 0) return ALWAN_E_RANGE;
    bayer = (unsigned char *)ALWAN_ALLOC(bytes_b, 16);
    buf = (unsigned short *)ALWAN_ALLOC(bytes_buf, 16);
    if (!bayer || !buf) {
        ALWAN_FREE(bayer);
        ALWAN_FREE(buf);
        return ALWAN_E_NOMEM;
    }
    memset(buf, 0, bytes_buf);
    {   /* copyMakeBorder(_srcmat, srcmat, 2, 2, 2, 2, BORDER_REFLECT_101) */
        size_t r, c;
        for (r = 0; r < (size_t)H; r++) {
            size_t const sr = alwan__dm_reflect101((ptrdiff_t)r - 2, height);
            unsigned char const *s = src + sr * src_stride;
            for (c = 0; c < (size_t)N; c++) {
                bayer[r * (size_t)N + c] = s[alwan__dm_reflect101((ptrdiff_t)c - 2, width)];
            }
        }
    }

    alwan__dm_flags(pattern, &bg_or_gb, &starts_green);
    blueIdx = bg_or_gb ? 0 : 2;
    greenCell0 = starts_green;

    for (y = 2; y < H - 2; y++) {
        unsigned char *dstrow = dst + dst_stride * (size_t)(y - 2);
        unsigned char const *srow;
        unsigned short const *brow0, *brow1, *brow2;
        int dy, i, greenCell, simd_first, simd_count;

        for (dy = (y == 2 ? -1 : 1); dy <= 1; dy++) {
            unsigned short *brow = buf + ((y + dy - 1) % brows) * bufstep + 1;
            int k;
            srow = bayer + (size_t)(y + dy) * (size_t)bstep + 1;
            for (k = 0; k < bcn; k++) brow[N * k - 1] = brow[(N - 2) + N * k] = 0;
            for (i = 1; i < N - 1; i++, srow++, brow++) {
                int const a = srow[-1 - bstep], b = srow[-bstep], c = srow[1 - bstep];
                int const d = srow[-1], e = srow[1];
                int const f = srow[-1 + bstep], g = srow[bstep], h = srow[1 + bstep];
                int const v0 = (a > f ? a - f : f - a) + (b > g ? b - g : g - b) * 2 + (c > h ? c - h : h - c);
                int const v1 = (a > c ? a - c : c - a) + (d > e ? d - e : e - d) * 2 + (f > h ? f - h : h - f);
                int const v2 = (c > f ? c - f : f - c) * 2;
                int const v3 = (a > h ? a - h : h - a) * 2;
                brow[0] = (unsigned short)v0;
                brow[N] = (unsigned short)v1;
                brow[N2] = (unsigned short)v2;
                brow[N3] = (unsigned short)v3;
                brow[N4] = (unsigned short)(v2 + (b > d ? b - d : d - b) + (g > e ? g - e : e - g));
                brow[N5] = (unsigned short)(v3 + (b > e ? b - e : e - b) + (g > d ? g - d : d - g));
                brow[N6] = (unsigned short)((b + d + e + g) >> 1);
            }
        }

        brow0 = buf + ((y - 2) % brows) * bufstep + 2;
        brow1 = buf + ((y - 1) % brows) * bufstep + 2;
        brow2 = buf + (y % brows) * bufstep + 2;
        srow = bayer + (size_t)y * (size_t)bstep + 2;
        greenCell = greenCell0;
        alwan__vng_simd_span(greenCell0, N, &simd_first, &simd_count);

        for (i = 2; i < N - 2; i++, srow++, brow0++, brow1++, brow2++, dstrow += 3) {
            int const gradN = brow0[0] + brow1[0];
            int const gradS = brow1[0] + brow2[0];
            int const gradW = brow1[N - 1] + brow1[N];
            int const gradE = brow1[N] + brow1[N + 1];
            int minGrad = gradN < gradS ? gradN : gradS;
            int maxGrad = gradN > gradS ? gradN : gradS;
            int gradNE, gradSW, gradNW, gradSE, T, R, G, B;
            int Rs = 0, Gs = 0, Bs = 0, ng = 0;
            float sc;
            minGrad = minGrad < gradW ? minGrad : gradW;
            minGrad = minGrad < gradE ? minGrad : gradE;
            maxGrad = maxGrad > gradW ? maxGrad : gradW;
            maxGrad = maxGrad > gradE ? maxGrad : gradE;

            if (!greenCell) {
                gradNE = brow0[N4 + 1] + brow1[N4];
                gradSW = brow1[N4] + brow2[N4 - 1];
                gradNW = brow0[N5 - 1] + brow1[N5];
                gradSE = brow1[N5] + brow2[N5 + 1];
            } else {
                gradNE = brow0[N2] + brow0[N2 + 1] + brow1[N2] + brow1[N2 + 1];
                gradSW = brow1[N2] + brow1[N2 - 1] + brow2[N2] + brow2[N2 - 1];
                gradNW = brow0[N3] + brow0[N3 - 1] + brow1[N3] + brow1[N3 - 1];
                gradSE = brow1[N3] + brow1[N3 + 1] + brow2[N3] + brow2[N3 + 1];
            }
            if (gradNE < minGrad) minGrad = gradNE;
            if (gradSW < minGrad) minGrad = gradSW;
            if (gradNW < minGrad) minGrad = gradNW;
            if (gradSE < minGrad) minGrad = gradSE;
            if (gradNE > maxGrad) maxGrad = gradNE;
            if (gradSW > maxGrad) maxGrad = gradSW;
            if (gradNW > maxGrad) maxGrad = gradNW;
            if (gradSE > maxGrad) maxGrad = gradSE;
            T = minGrad + ((maxGrad / 2) > 1 ? (maxGrad / 2) : 1);

            if (!greenCell) {
                if (gradN < T) { Rs += srow[-bstep * 2] + srow[0]; Gs += srow[-bstep] * 2; Bs += srow[-bstep - 1] + srow[-bstep + 1]; ng++; }
                if (gradS < T) { Rs += srow[bstep * 2] + srow[0]; Gs += srow[bstep] * 2; Bs += srow[bstep - 1] + srow[bstep + 1]; ng++; }
                if (gradW < T) { Rs += srow[-2] + srow[0]; Gs += srow[-1] * 2; Bs += srow[-bstep - 1] + srow[bstep - 1]; ng++; }
                if (gradE < T) { Rs += srow[2] + srow[0]; Gs += srow[1] * 2; Bs += srow[-bstep + 1] + srow[bstep + 1]; ng++; }
                if (gradNE < T) { Rs += srow[-bstep * 2 + 2] + srow[0]; Gs += brow0[N6 + 1]; Bs += srow[-bstep + 1] * 2; ng++; }
                if (gradSW < T) { Rs += srow[bstep * 2 - 2] + srow[0]; Gs += brow2[N6 - 1]; Bs += srow[bstep - 1] * 2; ng++; }
                if (gradNW < T) { Rs += srow[-bstep * 2 - 2] + srow[0]; Gs += brow0[N6 - 1]; Bs += srow[-bstep - 1] * 2; ng++; }
                if (gradSE < T) { Rs += srow[bstep * 2 + 2] + srow[0]; Gs += brow2[N6 + 1]; Bs += srow[bstep + 1] * 2; ng++; }
            } else {
                if (gradN < T) { Rs += srow[-bstep * 2 - 1] + srow[-bstep * 2 + 1]; Gs += srow[-bstep * 2] + srow[0]; Bs += srow[-bstep] * 2; ng++; }
                if (gradS < T) { Rs += srow[bstep * 2 - 1] + srow[bstep * 2 + 1]; Gs += srow[bstep * 2] + srow[0]; Bs += srow[bstep] * 2; ng++; }
                if (gradW < T) { Rs += srow[-1] * 2; Gs += srow[-2] + srow[0]; Bs += srow[-bstep - 2] + srow[bstep - 2]; ng++; }
                if (gradE < T) { Rs += srow[1] * 2; Gs += srow[2] + srow[0]; Bs += srow[-bstep + 2] + srow[bstep + 2]; ng++; }
                if (gradNE < T) { Rs += srow[-bstep * 2 + 1] + srow[1]; Gs += srow[-bstep + 1] * 2; Bs += srow[-bstep] + srow[-bstep + 2]; ng++; }
                if (gradSW < T) { Rs += srow[bstep * 2 - 1] + srow[-1]; Gs += srow[bstep - 1] * 2; Bs += srow[bstep] + srow[bstep - 2]; ng++; }
                if (gradNW < T) { Rs += srow[-bstep * 2 - 1] + srow[-1]; Gs += srow[-bstep - 1] * 2; Bs += srow[-bstep - 2] + srow[-bstep]; ng++; }
                if (gradSE < T) { Rs += srow[bstep * 2 + 1] + srow[1]; Gs += srow[bstep + 1] * 2; Bs += srow[bstep + 2] + srow[bstep]; ng++; }
            }

            /* ng >= 1: the smallest gradient is always below T */
            if (i >= simd_first && i < simd_first + simd_count) {
                sc = 0.5f / (float)ng;            /* the SIMD loop's v_div(0.5, ng) */
            } else {
                sc = scale[ng];                   /* the scalar loop's table */
            }
            if (!greenCell) {
                R = srow[0];
                G = R + alwan__dm_round_even((float)(Gs - Rs) * sc);
                B = R + alwan__dm_round_even((float)(Bs - Rs) * sc);
            } else {
                G = srow[0];
                R = G + alwan__dm_round_even((float)(Rs - Gs) * sc);
                B = G + alwan__dm_round_even((float)(Bs - Gs) * sc);
            }
            /* stored as RGB; OpenCV's BGR with blueIdx naming B's slot */
            dstrow[2 - blueIdx] = alwan__dm_sat_u8(B);
            dstrow[1] = alwan__dm_sat_u8(G);
            dstrow[blueIdx] = alwan__dm_sat_u8(R);
            greenCell = !greenCell;
        }
        greenCell0 = !greenCell0;
        blueIdx ^= 2;
    }
    ALWAN_FREE(bayer);
    ALWAN_FREE(buf);
    return ALWAN_OK;
}

/* Bayer2RGB_EdgeAware_T on one sample type. Output RGB (OpenCV writes BGR: blue's slot is
 * mirrored here). */
#define ALWAN__DM_EA_BODY(T)                                                                                \
    int bg_or_gb, starts_green, start_with_green, blue, y;                                                  \
    int const W = alwan__dm_int(width), Hh = alwan__dm_int(height);                                                             \
    int const sw = W - 2, sh = Hh - 2;                                                                      \
    if (W <= 2 || Hh <= 2) {                                                                                \
        for (y = 0; y < Hh; y++) memset((char *)dst + (size_t)y * dst_stride, 0, (size_t)W * 3 * sizeof(T)); \
        return ALWAN_OK;                                                                                    \
    }                                                                                                       \
    alwan__dm_flags(pattern, &bg_or_gb, &starts_green);                                                     \
    start_with_green = starts_green;                                                                        \
    blue = bg_or_gb;                                                                                        \
    for (y = 0; y < sh; y++) {                                                                              \
        T const *S = (T const *)((char const *)src + (size_t)(y + 1) * src_stride) + 1;                     \
        T *D = (T *)((char *)dst + (size_t)(y + 1) * dst_stride) + 3;                                       \
        ptrdiff_t const ss = (ptrdiff_t)(src_stride / sizeof(T));                                           \
        int x = 1, k;                                                                                       \
        /* D[c] is BGR in OpenCV: slot c here is 2 - c */                                                  \
        if (start_with_green) {                                                                             \
            D[2 - (blue << 1)] = (T)((S[-ss] + S[ss] + 1) >> 1);                                            \
            D[1] = S[0];                                                                                    \
            D[2 - (2 - (blue << 1))] = (T)((S[-1] + S[1] + 1) >> 1);                                        \
            D += 3; ++S; ++x;                                                                               \
        }                                                                                                   \
        for (; x < sw; x += 2, S += 2, D += 6) {                                                            \
            int const dh = S[-1] > S[1] ? S[-1] - S[1] : S[1] - S[-1];                                      \
            int const dv = S[ss] > S[-ss] ? S[ss] - S[-ss] : S[-ss] - S[ss];                                \
            int const g = (dh > dv ? (S[ss] + S[-ss] + 1) : (S[-1] + S[1] + 1)) >> 1;                      \
            int const diag = (S[-ss - 1] + S[-ss + 1] + S[ss - 1] + S[ss + 1] + 2) >> 2;                   \
            if (blue) {                                                                                     \
                D[2] = S[0]; D[1] = (T)g; D[0] = (T)diag;                                                   \
                D[5] = (T)((S[0] + S[2] + 1) >> 1); D[4] = S[1]; D[3] = (T)((S[-ss + 1] + S[ss + 1] + 1) >> 1); \
            } else {                                                                                        \
                D[2] = (T)diag; D[1] = (T)g; D[0] = S[0];                                                   \
                D[5] = (T)((S[-ss + 1] + S[ss + 1] + 1) >> 1); D[4] = S[1]; D[3] = (T)((S[0] + S[2] + 1) >> 1); \
            }                                                                                               \
        }                                                                                                   \
        if (x <= sw) {                                                                                      \
            int const dh = S[-1] > S[1] ? S[-1] - S[1] : S[1] - S[-1];                                      \
            int const dv = S[ss] > S[-ss] ? S[ss] - S[-ss] : S[-ss] - S[ss];                                \
            D[2 - (blue << 1)] = (T)((S[-ss - 1] + S[-ss + 1] + S[ss - 1] + S[ss + 1] + 2) >> 2);          \
            D[1] = (T)((dh > dv ? (S[ss] + S[-ss] + 1) : (S[-1] + S[1] + 1)) >> 1);                        \
            D[2 - (2 - (blue << 1))] = S[0];                                                                \
            D += 3; ++S;                                                                                    \
        }                                                                                                   \
        /* last column from the one before it; column 0 from column 1 */                                   \
        {                                                                                                   \
            T *row = (T *)((char *)dst + (size_t)(y + 1) * dst_stride);                                     \
            for (k = 0; k < 3; k++) {                                                                       \
                row[(size_t)(W - 1) * 3 + (size_t)k] = row[(size_t)(W - 2) * 3 + (size_t)k];                \
                row[(size_t)k] = row[3 + (size_t)k];                                                        \
            }                                                                                               \
        }                                                                                                   \
        start_with_green ^= 1;                                                                              \
        blue ^= 1;                                                                                          \
    }                                                                                                       \
    {                                                                                                       \
        T *first = dst;                                                                                     \
        T *second = (T *)((char *)dst + dst_stride);                                                        \
        T *last = (T *)((char *)dst + (size_t)(Hh - 1) * dst_stride);                                       \
        T *before = (T *)((char *)dst + (size_t)(Hh - 2) * dst_stride);                                     \
        size_t const n3 = (size_t)W * 3, j_end = n3;                                                        \
        size_t j;                                                                                           \
        for (j = 0; j < j_end; j++) { first[j] = second[j]; last[j] = before[j]; }                         \
    }                                                                                                       \
    return ALWAN_OK;

static alwan_status alwan__ea_u8(unsigned char *dst, size_t dst_stride, unsigned char const *src,
                                 size_t src_stride, size_t width, size_t height, alwan_cfa_pattern pattern) {
    ALWAN__DM_EA_BODY(unsigned char)
}

static alwan_status alwan__ea_u16(alwan_uint16 *dst, size_t dst_stride, alwan_uint16 const *src,
                                  size_t src_stride, size_t width, size_t height, alwan_cfa_pattern pattern) {
    ALWAN__DM_EA_BODY(alwan_uint16)
}

#undef ALWAN__DM_EA_BODY

/* Methods 0 to 3 on integer codes: the codes as they are (no normalisation; every one of
 * them is linear or scale-free in its decisions), demosaicked in double (float in a
 * single-precision build), rounded to nearest with ties up and clamped to the type. */
static alwan_status alwan__dm_float_path(void *dst, size_t dst_stride, void const *src, size_t src_stride,
                                         size_t width, size_t height, alwan_cfa_pattern pattern,
                                         alwan_demosaic_method method, int is16) {
#if ALWAN_WITH_F64
    typedef alwan_f64 alwan__dm_t;
#else
    typedef alwan_f32 alwan__dm_t;
#endif
    size_t const n = width * height;
    size_t const in_bytes = alwan_safe_array_size(n, sizeof(alwan__dm_t));
    size_t const out_bytes = alwan_safe_array_size(n, 3 * sizeof(alwan__dm_t));
    double const top = is16 ? 65535.0 : 255.0;
    alwan__dm_t *in, *out;
    alwan_status st;
    size_t x, y;
    if (in_bytes == 0 || out_bytes == 0) return ALWAN_E_RANGE;
    in = (alwan__dm_t *)ALWAN_ALLOC(in_bytes, sizeof(alwan__dm_t));
    out = (alwan__dm_t *)ALWAN_ALLOC(out_bytes, sizeof(alwan__dm_t));
    if (!in || !out) {
        ALWAN_FREE(in);
        ALWAN_FREE(out);
        return ALWAN_E_NOMEM;
    }
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            in[y * width + x] = is16 ? (alwan__dm_t)((alwan_uint16 const *)((char const *)src + y * src_stride))[x]
                                     : (alwan__dm_t)((unsigned char const *)((char const *)src + y * src_stride))[x];
        }
    }
#if ALWAN_WITH_F64
    st = alwan_cfa_bayer_demosaic_f64(out, width * 3 * sizeof(alwan__dm_t), in, width * sizeof(alwan__dm_t),
                                      width, height, pattern, method);
#else
    st = alwan_cfa_bayer_demosaic_f32(out, width * 3 * sizeof(alwan__dm_t), in, width * sizeof(alwan__dm_t),
                                      width, height, pattern, method);
#endif
    if (st == ALWAN_OK) {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width * 3; x++) {
                double v = (double)out[y * width * 3 + x];
                if (!(v > 0.0)) v = 0.0;          /* NaN to 0 too */
                if (v > top) v = top;
                v = ALWAN_FLOOR(v + 0.5);
                if (is16) ((alwan_uint16 *)((char *)dst + y * dst_stride))[x] = (alwan_uint16)v;
                else ((unsigned char *)((char *)dst + y * dst_stride))[x] = (unsigned char)v;
            }
        }
    }
    ALWAN_FREE(in);
    ALWAN_FREE(out);
    return st;
}

static alwan_status alwan__dm_check(void const *dst, void const *src, size_t dst_stride, size_t src_stride,
                                    size_t width, size_t height, alwan_cfa_pattern pattern,
                                    alwan_demosaic_method method, size_t sample) {
    if (!dst || !src || width < 2 || height < 2 || (int)pattern < 0 || (int)pattern > 3
        || (int)method < 0 || (int)method > (int)ALWAN_DEMOSAIC_EDGE_AWARE_OPENCV) {
        return ALWAN_E_INVALID;
    }
    if ((width * height) / width != height || width > 0x3fffffff / 8 || height > 0x3fffffff / 8) {
        return ALWAN_E_RANGE;
    }
    if (src_stride < width * sample || dst_stride < width * 3 * sample || (src_stride % sample) != 0) {
        return ALWAN_E_INVALID;
    }
    return ALWAN_OK;
}

alwan_status alwan_cfa_bayer_demosaic_u8(alwan_uint8 *rgb_out, size_t rgb_row_stride, alwan_uint8 const *cfa,
                                         size_t cfa_row_stride, size_t width, size_t height,
                                         alwan_cfa_pattern pattern, alwan_demosaic_method method) {
    alwan_status const st = alwan__dm_check(rgb_out, cfa, rgb_row_stride, cfa_row_stride, width, height, pattern,
                                            method, 1);
    if (st != ALWAN_OK) return st;
    if (method == ALWAN_DEMOSAIC_VNG_OPENCV) {
        /* OpenCV falls back to its bilinear interpolation below 8 pixels a side; that path
         * is not ported, so such an image is refused rather than answered differently */
        if (width < 8 || height < 8) return ALWAN_E_INVALID;
        return alwan__vng_u8(rgb_out, rgb_row_stride, cfa, cfa_row_stride, width, height, pattern);
    }
    if (method == ALWAN_DEMOSAIC_EDGE_AWARE_OPENCV) {
        return alwan__ea_u8(rgb_out, rgb_row_stride, cfa, cfa_row_stride, width, height, pattern);
    }
    return alwan__dm_float_path(rgb_out, rgb_row_stride, cfa, cfa_row_stride, width, height, pattern, method, 0);
}

alwan_status alwan_cfa_bayer_demosaic_u16(alwan_uint16 *rgb_out, size_t rgb_row_stride, alwan_uint16 const *cfa,
                                          size_t cfa_row_stride, size_t width, size_t height,
                                          alwan_cfa_pattern pattern, alwan_demosaic_method method) {
    alwan_status const st = alwan__dm_check(rgb_out, cfa, rgb_row_stride, cfa_row_stride, width, height, pattern,
                                            method, sizeof(alwan_uint16));
    if (st != ALWAN_OK) return st;
    if (method == ALWAN_DEMOSAIC_VNG_OPENCV) {
        return ALWAN_E_INVALID;                   /* OpenCV's VNG is 8-bit only */
    }
    if (method == ALWAN_DEMOSAIC_EDGE_AWARE_OPENCV) {
        return alwan__ea_u16(rgb_out, rgb_row_stride, cfa, cfa_row_stride, width, height, pattern);
    }
    return alwan__dm_float_path(rgb_out, rgb_row_stride, cfa, cfa_row_stride, width, height, pattern, method, 1);
}
