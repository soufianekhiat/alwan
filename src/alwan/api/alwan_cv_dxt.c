/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * OpenCV's discrete Fourier and cosine transforms on floats, for the ports that must match
 * OpenCV to the bit: alwan_gradient_edit's sine transform (complex rows through cv::dft)
 * and ALWAN_DENOISE_DCT_OPENCV (cv::dct and cv::idct on square blocks).
 *
 * A port of OpenCV 5.0.0's modules/core/src/dxt.cpp: DFTFactorize, DFTInit (with its
 * inverse permutation table), the mixed-radix complex transform DFT<float> (the radix-2,
 * -3, -5 and odd-factor passes, and the SSE3 radix-4, radix-2 and radix-3 passes the x64
 * build takes, whose operation order this file follows in scalar code), RealDFT and
 * CCSIDFT on real data in OpenCV's packed CCS layout, DCTInit, DCT and IDCT (the DCT
 * through a real DFT of the even-odd reordered row, after Makhoul), and OcvDctImpl's two
 * stages, rows then columns. OpenCV is Copyright the OpenCV authors, Apache License 2.0
 * (https://github.com/opencv/opencv), its licence text in data/opencv/LICENSE-opencv.txt;
 * dxt.cpp itself carries the Intel License Agreement for Open Source Computer Vision
 * Library, Copyright (C) 2000, Intel Corporation, all rights reserved, whose conditions
 * (retain this notice in source; reproduce it in the documentation of a binary; do not use
 * Intel's name to endorse derived products; provided "as is", without warranty) apply to
 * this port as well.
 *
 * Only the float, non-IPP paths are ported: OpenCV's pip build hands dct to IPP when IPP
 * is enabled, so the references are generated with IPP off.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stdlib.h>
#include <string.h>

static unsigned char const cvx_bitrev[256] = {
    0x00,0x80,0x40,0xc0,0x20,0xa0,0x60,0xe0,0x10,0x90,0x50,0xd0,0x30,0xb0,0x70,0xf0,
    0x08,0x88,0x48,0xc8,0x28,0xa8,0x68,0xe8,0x18,0x98,0x58,0xd8,0x38,0xb8,0x78,0xf8,
    0x04,0x84,0x44,0xc4,0x24,0xa4,0x64,0xe4,0x14,0x94,0x54,0xd4,0x34,0xb4,0x74,0xf4,
    0x0c,0x8c,0x4c,0xcc,0x2c,0xac,0x6c,0xec,0x1c,0x9c,0x5c,0xdc,0x3c,0xbc,0x7c,0xfc,
    0x02,0x82,0x42,0xc2,0x22,0xa2,0x62,0xe2,0x12,0x92,0x52,0xd2,0x32,0xb2,0x72,0xf2,
    0x0a,0x8a,0x4a,0xca,0x2a,0xaa,0x6a,0xea,0x1a,0x9a,0x5a,0xda,0x3a,0xba,0x7a,0xfa,
    0x06,0x86,0x46,0xc6,0x26,0xa6,0x66,0xe6,0x16,0x96,0x56,0xd6,0x36,0xb6,0x76,0xf6,
    0x0e,0x8e,0x4e,0xce,0x2e,0xae,0x6e,0xee,0x1e,0x9e,0x5e,0xde,0x3e,0xbe,0x7e,0xfe,
    0x01,0x81,0x41,0xc1,0x21,0xa1,0x61,0xe1,0x11,0x91,0x51,0xd1,0x31,0xb1,0x71,0xf1,
    0x09,0x89,0x49,0xc9,0x29,0xa9,0x69,0xe9,0x19,0x99,0x59,0xd9,0x39,0xb9,0x79,0xf9,
    0x05,0x85,0x45,0xc5,0x25,0xa5,0x65,0xe5,0x15,0x95,0x55,0xd5,0x35,0xb5,0x75,0xf5,
    0x0d,0x8d,0x4d,0xcd,0x2d,0xad,0x6d,0xed,0x1d,0x9d,0x5d,0xdd,0x3d,0xbd,0x7d,0xfd,
    0x03,0x83,0x43,0xc3,0x23,0xa3,0x63,0xe3,0x13,0x93,0x53,0xd3,0x33,0xb3,0x73,0xf3,
    0x0b,0x8b,0x4b,0xcb,0x2b,0xab,0x6b,0xeb,0x1b,0x9b,0x5b,0xdb,0x3b,0xbb,0x7b,0xfb,
    0x07,0x87,0x47,0xc7,0x27,0xa7,0x67,0xe7,0x17,0x97,0x57,0xd7,0x37,0xb7,0x77,0xf7,
    0x0f,0x8f,0x4f,0xcf,0x2f,0xaf,0x6f,0xef,0x1f,0x9f,0x5f,0xdf,0x3f,0xbf,0x7f,0xff
};

/* cos and sin of 2 pi / 2^m, OpenCV's DFTTab */
static double const cvx_dfttab[32][2] = {
    { 1.00000000000000000, 0.00000000000000000 },
    {-1.00000000000000000, 0.00000000000000000 },
    { 0.00000000000000000, 1.00000000000000000 },
    { 0.70710678118654757, 0.70710678118654746 },
    { 0.92387953251128674, 0.38268343236508978 },
    { 0.98078528040323043, 0.19509032201612825 },
    { 0.99518472667219693, 0.09801714032956060 },
    { 0.99879545620517241, 0.04906767432741802 },
    { 0.99969881869620425, 0.02454122852291229 },
    { 0.99992470183914450, 0.01227153828571993 },
    { 0.99998117528260111, 0.00613588464915448 },
    { 0.99999529380957619, 0.00306795676296598 },
    { 0.99999882345170188, 0.00153398018628477 },
    { 0.99999970586288223, 0.00076699031874270 },
    { 0.99999992646571789, 0.00038349518757140 },
    { 0.99999998161642933, 0.00019174759731070 },
    { 0.99999999540410733, 0.00009587379909598 },
    { 0.99999999885102686, 0.00004793689960307 },
    { 0.99999999971275666, 0.00002396844980842 },
    { 0.99999999992818922, 0.00001198422490507 },
    { 0.99999999998204725, 0.00000599211245264 },
    { 0.99999999999551181, 0.00000299605622633 },
    { 0.99999999999887801, 0.00000149802811317 },
    { 0.99999999999971945, 0.00000074901405658 },
    { 0.99999999999992983, 0.00000037450702829 },
    { 0.99999999999998246, 0.00000018725351415 },
    { 0.99999999999999567, 0.00000009362675707 },
    { 0.99999999999999889, 0.00000004681337854 },
    { 0.99999999999999978, 0.00000002340668927 },
    { 0.99999999999999989, 0.00000001170334463 },
    { 1.00000000000000000, 0.00000000585167232 },
    { 1.00000000000000000, 0.00000000292583616 }
};

/* DCTInit's DctScale: sqrt(2^-m) / 2 */
static double const cvx_dctscale[30] = {
    0.707106781186547570, 0.500000000000000000, 0.353553390593273790,
    0.250000000000000000, 0.176776695296636890, 0.125000000000000000,
    0.088388347648318447, 0.062500000000000000, 0.044194173824159223,
    0.031250000000000000, 0.022097086912079612, 0.015625000000000000,
    0.011048543456039806, 0.007812500000000000, 0.005524271728019903,
    0.003906250000000000, 0.002762135864009952, 0.001953125000000000,
    0.001381067932004976, 0.000976562500000000, 0.000690533966002488,
    0.000488281250000000, 0.000345266983001244, 0.000244140625000000,
    0.000172633491500622, 0.000122070312500000, 0.000086316745750311,
    0.000061035156250000, 0.000043158372875155, 0.000030517578125000
};

#define CVX_PI 3.1415926535897932384626433832795

#define CVX_BITREV(i, shift) \
    ((int)((((unsigned)cvx_bitrev[(i) & 255] << 24) + \
            ((unsigned)cvx_bitrev[((i) >> 8) & 255] << 16) + \
            ((unsigned)cvx_bitrev[((i) >> 16) & 255] << 8) + \
            ((unsigned)cvx_bitrev[((i) >> 24)])) >> (shift)))

static float const cvx_sin_120 = (float)0.86602540378443864676372317075294;
static float const cvx_fft5_2 = (float)0.559016994374947424102293417182819;
static float const cvx_fft5_3 = (float)-0.951056516295153572116439333379382;
static float const cvx_fft5_4 = (float)-1.538841768587626701285145288018455;
static float const cvx_fft5_5 = (float)0.363271264002680442947733378740309;

typedef alwan__cv_cf cvx_cf;

/* OcvDftOptions, the fields the float path reads */
typedef struct {
    int n;
    int tab_size;
    int nf;
    int const *factors;
    int const *itab;
    cvx_cf const *wave;
    int inverse;
    int no_permute;
    double scale;
    cvx_cf *scratch; /* the odd-factor pass's a[] and b[], factor - 1 entries */
} cvx_opts;

static int cvx_factorize(int n, int *factors) {
    int nf = 0, f, i, j;

    if (n <= 5) {
        factors[0] = n;
        return 1;
    }

    f = (((n - 1) ^ n) + 1) >> 1;
    if (f > 1) {
        factors[nf++] = f;
        n = f == n ? 1 : n / f;
    }

    for (f = 3; n > 1;) {
        int d = n / f;
        if (d * f == n) {
            factors[nf++] = f;
            n = d;
        } else {
            f += 2;
            if (f * f > n) break;
        }
    }

    if (n > 1) factors[nf++] = n;

    f = (factors[0] & 1) == 0;
    for (i = f; i < (nf + f) / 2; i++) {
        j = factors[i];
        factors[i] = factors[nf - i - 1 + f];
        factors[nf - i - 1 + f] = j;
    }
    return nf;
}

/* DFTInit for complex float. With inv_itab and more than one kind of factor, OpenCV builds
 * the forward table in the wave buffer and stores its inverse in itab; tmp stands in for
 * that buffer here (n0 ints). */
static void cvx_dft_init(int n0, int nf, int const *factors, int *itab, cvx_cf *wave, int inv_itab, int *tmp) {
    int digits[34], radix[34];
    int n = factors[0], m = 0;
    int *itab0 = itab;
    int i, j, k;
    double wre, wim, w1re, w1im, t;

    memset(digits, 0, sizeof digits);
    memset(radix, 0, sizeof radix);

    if (n0 <= 5) {
        itab[0] = 0;
        itab[n0 - 1] = n0 - 1;
        if (n0 != 4) {
            for (i = 1; i < n0 - 1; i++) itab[i] = i;
        } else {
            itab[1] = 2;
            itab[2] = 1;
        }
        if (n0 == 5) {
            wave[0].re = 1.f;
            wave[0].im = 0.f;
        }
        if (n0 != 4) return;
        m = 2;
    } else {
        radix[nf] = 1;
        digits[nf] = 0;
        for (i = 0; i < nf; i++) {
            digits[i] = 0;
            radix[nf - i - 1] = radix[nf - i] * factors[nf - i - 1];
        }

        if (inv_itab && factors[0] != factors[nf - 1]) itab = tmp;

        if ((n & 1) == 0) {
            int a = radix[1], na2 = n * a >> 1, na4 = na2 >> 1;
            for (m = 0; (unsigned)(1 << m) < (unsigned)n; m++) {
            }
            if (n <= 2) {
                itab[0] = 0;
                itab[1] = na2;
            } else if (n <= 256) {
                int shift = 10 - m;
                for (i = 0; i <= n - 4; i += 4) {
                    j = (cvx_bitrev[i >> 2] >> shift) * a;
                    itab[i] = j;
                    itab[i + 1] = j + na2;
                    itab[i + 2] = j + na4;
                    itab[i + 3] = j + na2 + na4;
                }
            } else {
                int shift = 34 - m;
                for (i = 0; i < n; i += 4) {
                    int i4 = i >> 2;
                    j = CVX_BITREV(i4, shift) * a;
                    itab[i] = j;
                    itab[i + 1] = j + na2;
                    itab[i + 2] = j + na4;
                    itab[i + 3] = j + na2 + na4;
                }
            }

            digits[1]++;

            if (nf >= 2) {
                for (i = n, j = radix[2]; i < n0;) {
                    for (k = 0; k < n; k++) itab[i + k] = itab[k] + j;
                    if ((i += n) >= n0) break;
                    j += radix[2];
                    for (k = 1; ++digits[k] >= factors[k]; k++) {
                        digits[k] = 0;
                        j += radix[k + 2] - radix[k];
                    }
                }
            }
        } else {
            for (i = 0, j = 0;;) {
                itab[i] = j;
                if (++i >= n0) break;
                j += radix[1];
                for (k = 0; ++digits[k] >= factors[k]; k++) {
                    digits[k] = 0;
                    j += radix[k + 2] - radix[k];
                }
            }
        }

        if (itab != itab0) {
            itab0[0] = 0;
            for (i = n0 & 1; i < n0; i += 2) {
                int k0 = itab[i];
                int k1 = itab[i + 1];
                itab0[k0] = i;
                itab0[k1] = i + 1;
            }
        }
    }

    if ((n0 & (n0 - 1)) == 0) {
        wre = w1re = cvx_dfttab[m][0];
        wim = w1im = -cvx_dfttab[m][1];
    } else {
        t = -CVX_PI * 2 / n0;
        wim = w1im = ALWAN_SIN_F64(t);
        wre = w1re = ALWAN_SQRT_F64(1. - w1im * w1im);
    }
    n = (n0 + 1) / 2;

    wave[0].re = 1.f;
    wave[0].im = 0.f;
    if ((n0 & 1) == 0) {
        wave[n].re = -1.f;
        wave[n].im = 0.f;
    }
    for (i = 1; i < n; i++) {
        wave[i].re = (float)wre;
        wave[i].im = (float)wim;
        wave[n0 - i].re = (float)wre;
        wave[n0 - i].im = (float)-wim;

        t = wre * w1re - wim * w1im;
        wim = wre * w1im + wim * w1re;
        wre = t;
    }
}

static cvx_cf cvx_cmul(cvx_cf a, cvx_cf w) {
    cvx_cf r;
    r.re = a.re * w.re - a.im * w.im;
    r.im = a.re * w.im + a.im * w.re;
    return r;
}

/* The SSE3 radix-4 pass (DFT_VecR4<float>): per butterfly, the three rotated inputs, then
 * the sums in the order the packed adds take them. */
static void cvx_r4_butterfly(cvx_cf *v0, cvx_cf *v1, int nx, cvx_cf a0, cvx_cf a1, cvx_cf a2, cvx_cf a3) {
    float s0r = a0.re + a1.re, s0i = a0.im + a1.im;
    float s2r = a2.re + a3.re, s2i = a2.im + a3.im;
    float d0r = a0.re - a1.re, d0i = a0.im - a1.im;
    float d2r = a2.re - a3.re, d2i = a2.im - a3.im;
    v0[0].re = s0r + s2r;
    v0[0].im = s0i + s2i;
    v0[nx].re = d0r + d2i;
    v0[nx].im = d0i - d2r;
    v1[0].re = s0r - s2r;
    v1[0].im = s0i - s2i;
    v1[nx].re = d0r - d2i;
    v1[nx].im = d0i + d2r;
}

static int cvx_dft_r4(cvx_cf *dst, int big_n, int n0, int *pdw0, cvx_cf const *wave) {
    int n = 1, i, j, nx, dw, dw0 = *pdw0;

    for (; n * 4 <= big_n;) {
        nx = n;
        n *= 4;
        dw0 /= 4;

        for (i = 0; i < n0; i += n) {
            cvx_cf *v0 = dst + i;
            cvx_cf *v1 = v0 + nx * 2;

            cvx_r4_butterfly(v0, v1, nx, v0[0], v0[nx], v1[0], v1[nx]);

            for (j = 1, dw = dw0; j < nx; j++, dw += dw0) {
                cvx_cf c1, c2, c3;
                v0 = dst + i + j;
                v1 = v0 + nx * 2;
                c1 = cvx_cmul(v0[nx], wave[dw * 2]);
                c3 = cvx_cmul(v1[nx], wave[dw * 3]);
                c2 = cvx_cmul(v1[0], wave[dw]);
                cvx_r4_butterfly(v0, v1, nx, v0[0], c1, c2, c3);
            }
        }
    }

    *pdw0 = dw0;
    return n;
}

/* DFT_R2, and DFT_VecR2<float>, whose hadd of the negated product gives the same sums */
static void cvx_dft_r2(cvx_cf *dst, int c_n, int n, int dw0, cvx_cf const *wave) {
    int const nx = n / 2;
    int i, j, dw;
    for (i = 0; i < c_n; i += n) {
        cvx_cf *v = dst + i;
        float r0 = v[0].re + v[nx].re;
        float i0 = v[0].im + v[nx].im;
        float r1 = v[0].re - v[nx].re;
        float i1 = v[0].im - v[nx].im;
        v[0].re = r0;
        v[0].im = i0;
        v[nx].re = r1;
        v[nx].im = i1;

        for (j = 1, dw = dw0; j < nx; j++, dw += dw0) {
            v = dst + i + j;
            r1 = v[nx].re * wave[dw].re - v[nx].im * wave[dw].im;
            i1 = v[nx].im * wave[dw].re + v[nx].re * wave[dw].im;
            r0 = v[0].re;
            i0 = v[0].im;

            v[0].re = r0 + r1;
            v[0].im = i0 + i1;
            v[nx].re = r0 - r1;
            v[nx].im = i0 - i1;
        }
    }
}

/* DFT_R3, and DFT_VecR3<float>, which rounds the same */
static void cvx_dft_r3(cvx_cf *dst, int c_n, int n, int dw0, cvx_cf const *wave) {
    int const nx = n / 3;
    int i, j, dw;
    for (i = 0; i < c_n; i += n) {
        {
            cvx_cf *v = dst + i;
            float r1 = v[nx].re + v[nx * 2].re;
            float i1 = v[nx].im + v[nx * 2].im;
            float r0 = v[0].re;
            float i0 = v[0].im;
            float r2 = cvx_sin_120 * (v[nx].im - v[nx * 2].im);
            float i2 = cvx_sin_120 * (v[nx * 2].re - v[nx].re);
            v[0].re = r0 + r1;
            v[0].im = i0 + i1;
            r0 -= 0.5f * r1;
            i0 -= 0.5f * i1;
            v[nx].re = r0 + r2;
            v[nx].im = i0 + i2;
            v[nx * 2].re = r0 - r2;
            v[nx * 2].im = i0 - i2;
        }

        for (j = 1, dw = dw0; j < nx; j++, dw += dw0) {
            cvx_cf *v = dst + i + j;
            float r0 = v[nx].re * wave[dw].re - v[nx].im * wave[dw].im;
            float i0 = v[nx].re * wave[dw].im + v[nx].im * wave[dw].re;
            float i2 = v[nx * 2].re * wave[dw * 2].re - v[nx * 2].im * wave[dw * 2].im;
            float r2 = v[nx * 2].re * wave[dw * 2].im + v[nx * 2].im * wave[dw * 2].re;
            float r1 = r0 + i2;
            float i1 = i0 + r2;

            r2 = cvx_sin_120 * (i0 - r2);
            i2 = cvx_sin_120 * (i2 - r0);
            r0 = v[0].re;
            i0 = v[0].im;
            v[0].re = r0 + r1;
            v[0].im = i0 + i1;
            r0 -= 0.5f * r1;
            i0 -= 0.5f * i1;
            v[nx].re = r0 + r2;
            v[nx].im = i0 + i2;
            v[nx * 2].re = r0 - r2;
            v[nx * 2].im = i0 - i2;
        }
    }
}

static void cvx_dft_r5(cvx_cf *dst, int c_n, int n, int dw0, cvx_cf const *wave) {
    int const nx = n / 5;
    int i, j, dw;
    for (i = 0; i < c_n; i += n) {
        for (j = 0, dw = 0; j < nx; j++, dw += dw0) {
            cvx_cf *v0 = dst + i + j;
            cvx_cf *v1 = v0 + nx * 2;
            cvx_cf *v2 = v1 + nx * 2;
            float r0, i0, r1, i1, r2, i2, r3, i3, r4, i4, r5, i5;

            r3 = v0[nx].re * wave[dw].re - v0[nx].im * wave[dw].im;
            i3 = v0[nx].re * wave[dw].im + v0[nx].im * wave[dw].re;
            r2 = v2[0].re * wave[dw * 4].re - v2[0].im * wave[dw * 4].im;
            i2 = v2[0].re * wave[dw * 4].im + v2[0].im * wave[dw * 4].re;

            r1 = r3 + r2;
            i1 = i3 + i2;
            r3 -= r2;
            i3 -= i2;

            r4 = v1[nx].re * wave[dw * 3].re - v1[nx].im * wave[dw * 3].im;
            i4 = v1[nx].re * wave[dw * 3].im + v1[nx].im * wave[dw * 3].re;
            r0 = v1[0].re * wave[dw * 2].re - v1[0].im * wave[dw * 2].im;
            i0 = v1[0].re * wave[dw * 2].im + v1[0].im * wave[dw * 2].re;

            r2 = r4 + r0;
            i2 = i4 + i0;
            r4 -= r0;
            i4 -= i0;

            r0 = v0[0].re;
            i0 = v0[0].im;
            r5 = r1 + r2;
            i5 = i1 + i2;

            v0[0].re = r0 + r5;
            v0[0].im = i0 + i5;

            r0 -= 0.25f * r5;
            i0 -= 0.25f * i5;
            r1 = cvx_fft5_2 * (r1 - r2);
            i1 = cvx_fft5_2 * (i1 - i2);
            r2 = -cvx_fft5_3 * (i3 + i4);
            i2 = cvx_fft5_3 * (r3 + r4);

            i3 *= -cvx_fft5_5;
            r3 *= cvx_fft5_5;
            i4 *= -cvx_fft5_4;
            r4 *= cvx_fft5_4;

            r5 = r2 + i3;
            i5 = i2 + r3;
            r2 -= i4;
            i2 -= r4;

            r3 = r0 + r1;
            i3 = i0 + i1;
            r0 -= r1;
            i0 -= i1;

            v0[nx].re = r3 + r2;
            v0[nx].im = i3 + i2;
            v2[0].re = r3 - r2;
            v2[0].im = i3 - i2;

            v1[0].re = r0 + r5;
            v1[0].im = i0 + i5;
            v1[nx].re = r0 - r5;
            v1[nx].im = i0 - i5;
        }
    }
}

static void cvx_dft_odd(cvx_cf *dst, int c_n, int tab_size, int factor, int n, int nx, int dw0,
                        cvx_cf const *wave, cvx_cf *buf) {
    int p, q, i, j, k, d, dd, dw, factor2 = (factor - 1) / 2;
    int const dw_f = tab_size / factor;
    cvx_cf *a = buf;
    cvx_cf *b = a + factor2;

    for (i = 0; i < c_n; i += n) {
        for (j = 0, dw = 0; j < nx; j++, dw += dw0) {
            cvx_cf *v = dst + i + j;
            cvx_cf v_0 = v[0];
            cvx_cf vn_0 = v_0;

            if (j == 0) {
                for (p = 1, k = nx; p <= factor2; p++, k += nx) {
                    float r0 = v[k].re + v[n - k].re;
                    float i0 = v[k].im - v[n - k].im;
                    float r1 = v[k].re - v[n - k].re;
                    float i1 = v[k].im + v[n - k].im;

                    vn_0.re += r0;
                    vn_0.im += i1;
                    a[p - 1].re = r0;
                    a[p - 1].im = i0;
                    b[p - 1].re = r1;
                    b[p - 1].im = i1;
                }
            } else {
                int const base = dw * factor;
                d = dw;

                for (p = 1, k = nx; p <= factor2; p++, k += nx, d += dw) {
                    float r2 = v[k].re * wave[d].re - v[k].im * wave[d].im;
                    float i2 = v[k].re * wave[d].im + v[k].im * wave[d].re;

                    float r1 = v[n - k].re * wave[base - d].re - v[n - k].im * wave[base - d].im;
                    float i1 = v[n - k].re * wave[base - d].im + v[n - k].im * wave[base - d].re;

                    float r0 = r2 + r1;
                    float i0 = i2 - i1;
                    r1 = r2 - r1;
                    i1 = i2 + i1;

                    vn_0.re += r0;
                    vn_0.im += i1;
                    a[p - 1].re = r0;
                    a[p - 1].im = i0;
                    b[p - 1].re = r1;
                    b[p - 1].im = i1;
                }
            }

            v[0] = vn_0;

            for (p = 1, k = nx; p <= factor2; p++, k += nx) {
                cvx_cf s0 = v_0, s1 = v_0;
                d = dd = dw_f * p;

                for (q = 0; q < factor2; q++) {
                    float r0 = wave[d].re * a[q].re;
                    float i0 = wave[d].im * a[q].im;
                    float r1 = wave[d].re * b[q].im;
                    float i1 = wave[d].im * b[q].re;

                    s1.re += r0 + i0;
                    s0.re += r0 - i0;
                    s1.im += r1 - i1;
                    s0.im += r1 + i1;

                    d += dd;
                    if (d >= tab_size) d -= tab_size;
                }

                v[k] = s0;
                v[n - k] = s1;
            }
        }
    }
}

/* DFT<float>: the mixed-radix complex transform, src and dst distinct or the same */
static void cvx_dft(cvx_opts const *c, cvx_cf const *src, cvx_cf *dst) {
    int const *itab = c->itab;
    cvx_cf const *wave = c->wave;
    int n = c->n;
    int const inv = c->inverse;
    int dw0 = c->tab_size;
    int f_idx, nx, i, j;
    float const scale = (float)c->scale;
    int const tab_step = c->tab_size == n ? 1 : c->tab_size == n * 2 ? 2 : c->tab_size / n;
    cvx_cf t;

    /* 0. shuffle data */
    if (dst != src) {
        if (!inv) {
            for (i = 0; i <= n - 2; i += 2, itab += 2 * tab_step) {
                int const k0 = itab[0], k1 = itab[tab_step];
                dst[i] = src[k0];
                dst[i + 1] = src[k1];
            }
            if (i < n) dst[n - 1] = src[n - 1];
        } else {
            for (i = 0; i <= n - 2; i += 2, itab += 2 * tab_step) {
                int const k0 = itab[0], k1 = itab[tab_step];
                t.re = src[k0].re;
                t.im = -src[k0].im;
                dst[i] = t;
                t.re = src[k1].re;
                t.im = -src[k1].im;
                dst[i + 1] = t;
            }
            if (i < n) {
                t.re = src[n - 1].re;
                t.im = -src[n - 1].im;
                dst[i] = t;
            }
        }
    } else {
        if (!c->no_permute) {
            if (c->nf == 1) {
                if ((n & 3) == 0) {
                    int const n2 = n / 2;
                    cvx_cf *dsth = dst + n2;

                    for (i = 0; i < n2; i += 2, itab += tab_step * 2) {
                        j = itab[0];
                        t = dst[i + 1];
                        dst[i + 1] = dsth[j];
                        dsth[j] = t;
                        if (j > i) {
                            t = dst[i];
                            dst[i] = dst[j];
                            dst[j] = t;
                            t = dsth[i + 1];
                            dsth[i + 1] = dsth[j + 1];
                            dsth[j + 1] = t;
                        }
                    }
                }
                /* else do nothing */
            } else {
                for (i = 0; i < n; i++, itab += tab_step) {
                    j = itab[0];
                    if (j > i) {
                        t = dst[i];
                        dst[i] = dst[j];
                        dst[j] = t;
                    }
                }
            }
        }

        if (inv) {
            for (i = 0; i <= n - 2; i += 2) {
                float const t0 = -dst[i].im;
                float const t1 = -dst[i + 1].im;
                dst[i].im = t0;
                dst[i + 1].im = t1;
            }
            if (i < n) dst[n - 1].im = -dst[n - 1].im;
        }
    }

    n = 1;
    /* 1. power-2 transforms: the SSE3 radix-4 pass takes every radix-4 stage, so the scalar
     * radix-4 loop that follows it in dxt.cpp finds nothing left to do */
    if ((c->factors[0] & 1) == 0) {
        if (c->factors[0] >= 4) n = cvx_dft_r4(dst, c->factors[0], c->n, &dw0, wave);
        for (; n < c->factors[0];) {
            n *= 2;
            dw0 /= 2;
            cvx_dft_r2(dst, c->n, n, dw0, wave);
        }
    }

    /* 2. all the other transforms */
    for (f_idx = (c->factors[0] & 1) ? 0 : 1; f_idx < c->nf; f_idx++) {
        int const factor = c->factors[f_idx];
        nx = n;
        n *= factor;
        dw0 /= factor;
        if (factor == 3) {
            cvx_dft_r3(dst, c->n, n, dw0, wave);
        } else if (factor == 5) {
            cvx_dft_r5(dst, c->n, n, dw0, wave);
        } else {
            cvx_dft_odd(dst, c->n, c->tab_size, factor, n, nx, dw0, wave, c->scratch);
        }
    }

    if (scale != 1) {
        float const re_scale = scale, im_scale = inv ? -scale : scale;
        for (i = 0; i < c->n; i++) {
            float const t0 = dst[i].re * re_scale;
            float const t1 = dst[i].im * im_scale;
            dst[i].re = t0;
            dst[i].im = t1;
        }
    } else if (inv) {
        for (i = 0; i <= c->n - 2; i += 2) {
            float const t0 = -dst[i].im;
            float const t1 = -dst[i + 1].im;
            dst[i].im = t0;
            dst[i + 1].im = t1;
        }
        if (i < c->n) dst[c->n - 1].im = -dst[c->n - 1].im;
    }
}

/* ================================================================
 * Complex rows: cv::dft of one row, forward, or inverse with DFT_SCALE
 * ================================================================ */

int alwan__cv_dft_plan_create(alwan__cv_dft_plan *p, int n) {
    memset(p, 0, sizeof *p);
    p->n = n;
    p->nf = cvx_factorize(n, p->factors);
    p->itab = (int *)malloc((size_t)n * sizeof(int));
    p->wave = (alwan__cv_cf *)malloc((size_t)n * sizeof(alwan__cv_cf));
    p->scratch = (alwan__cv_cf *)malloc(((size_t)n + 2) * sizeof(alwan__cv_cf));
    if (!p->itab || !p->wave || !p->scratch) return 0;
    memset(p->wave, 0, (size_t)n * sizeof(alwan__cv_cf));
    cvx_dft_init(n, p->nf, p->factors, p->itab, p->wave, 0, NULL);
    return 1;
}

void alwan__cv_dft_plan_free(alwan__cv_dft_plan *p) {
    free(p->itab);
    free(p->wave);
    free(p->scratch);
    p->itab = NULL;
    p->wave = NULL;
    p->scratch = NULL;
}

void alwan__cv_dft_row(alwan__cv_dft_plan const *p, alwan__cv_cf const *src, alwan__cv_cf *dst, int inv) {
    cvx_opts c;
    c.n = p->n;
    c.tab_size = p->n;
    c.nf = p->nf;
    c.factors = p->factors;
    c.itab = p->itab;
    c.wave = p->wave;
    c.inverse = inv;
    c.no_permute = 0;
    c.scale = inv ? 1. / p->n : 1.;
    c.scratch = p->scratch;
    cvx_dft(&c, src, dst);
}

/* ================================================================
 * Real transforms (RealDFT, CCSIDFT) and the DCT
 * ================================================================ */

/* RealDFT<float>, real output in CCS order, for an even n (the DCT's only case) */
static void cvx_real_dft(cvx_opts const *c, float const *src, float *dst) {
    int const n = c->n;
    float const scale = (float)c->scale;
    int j;

    if (n == 1) {
        dst[0] = src[0] * scale;
    } else if (n == 2) {
        float const t = (src[0] + src[1]) * scale;
        dst[1] = (src[0] - src[1]) * scale;
        dst[0] = t;
    } else {
        float t0, t, h1_re, h1_im, h2_re, h2_im;
        float const scale2 = scale * 0.5f;
        int const n2 = n >> 1;
        int lf[34];
        cvx_opts sub = *c;
        cvx_cf const *wave;

        memcpy(lf, c->factors, (size_t)c->nf * sizeof(int));
        lf[0] >>= 1;
        sub.factors = lf + (lf[0] == 1);
        sub.nf = c->nf - (lf[0] == 1);
        sub.inverse = 0;
        sub.no_permute = 0;
        sub.scale = 1.;
        sub.n = n2;

        cvx_dft(&sub, (cvx_cf const *)src, (cvx_cf *)dst);

        t = dst[0] - dst[1];
        dst[0] = (dst[0] + dst[1]) * scale;
        dst[1] = t * scale;

        t0 = dst[n2];
        t = dst[n - 1];
        dst[n - 1] = dst[1];

        wave = c->wave;

        for (j = 2, wave++; j < n2; j += 2, wave++) {
            /* calc odd */
            h2_re = scale2 * (dst[j + 1] + t);
            h2_im = scale2 * (dst[n - j] - dst[j]);

            /* calc even */
            h1_re = scale2 * (dst[j] + dst[n - j]);
            h1_im = scale2 * (dst[j + 1] - t);

            /* rotate */
            t = h2_re * wave->re - h2_im * wave->im;
            h2_im = h2_re * wave->im + h2_im * wave->re;
            h2_re = t;
            t = dst[n - j - 1];

            dst[j - 1] = h1_re + h2_re;
            dst[n - j - 1] = h1_re - h2_re;
            dst[j] = h1_im + h2_im;
            dst[n - j] = h2_im - h1_im;
        }

        if (j <= n2) {
            dst[n2 - 1] = t0 * scale;
            dst[n2] = -t * scale;
        }
    }
}

/* CCSIDFT<float>, real input in CCS order, for an even n; src and dst may be the same */
static void cvx_ccs_idft(cvx_opts const *c, float const *src, float *dst) {
    int const n = c->n;
    float const scale = (float)c->scale;
    int j, k;
    float t0, t1, t2, t3, t;

    if (n == 1) {
        dst[0] = src[0] * scale;
    } else if (n == 2) {
        t = (src[0] + src[1]) * scale;
        dst[1] = (src[0] - src[1]) * scale;
        dst[0] = t;
    } else {
        int const inplace = src == dst;
        cvx_cf const *w = c->wave;
        int const n2 = (n + 1) >> 1;
        int lf[34];
        cvx_opts sub = *c;

        t = src[1];
        t0 = (src[0] + src[n - 1]);
        t1 = (src[n - 1] - src[0]);
        dst[0] = t0;
        dst[1] = t1;

        for (j = 2, w++; j < n2; j += 2, w++) {
            float h1_re, h1_im, h2_re, h2_im;

            h1_re = (t + src[n - j - 1]);
            h1_im = (src[j] - src[n - j]);

            h2_re = (t - src[n - j - 1]);
            h2_im = (src[j] + src[n - j]);

            t = h2_re * w->re + h2_im * w->im;
            h2_im = h2_im * w->re - h2_re * w->im;
            h2_re = t;

            t = src[j + 1];
            t0 = h1_re - h2_im;
            t1 = -h1_im - h2_re;
            t2 = h1_re + h2_im;
            t3 = h1_im - h2_re;

            if (inplace) {
                dst[j] = t0;
                dst[j + 1] = t1;
                dst[n - j] = t2;
                dst[n - j + 1] = t3;
            } else {
                int const j2 = j >> 1;
                k = c->itab[j2];
                dst[k] = t0;
                dst[k + 1] = t1;
                k = c->itab[n2 - j2];
                dst[k] = t2;
                dst[k + 1] = t3;
            }
        }

        if (j <= n2) {
            t0 = t * 2;
            t1 = src[n2] * 2;

            if (inplace) {
                dst[n2] = t0;
                dst[n2 + 1] = t1;
            } else {
                k = c->itab[n2];
                dst[k * 2] = t0;
                dst[k * 2 + 1] = t1;
            }
        }

        memcpy(lf, c->factors, (size_t)c->nf * sizeof(int));
        lf[0] >>= 1;
        sub.factors = lf + (lf[0] == 1);
        sub.nf = c->nf - (lf[0] == 1);
        sub.inverse = 0;
        sub.no_permute = !inplace;
        sub.scale = 1.;
        sub.n = n2;

        cvx_dft(&sub, (cvx_cf const *)dst, (cvx_cf *)dst);

        for (j = 0; j < n; j += 2) {
            t0 = dst[j] * scale;
            t1 = dst[j + 1] * (-scale);
            dst[j] = t0;
            dst[j + 1] = t1;
        }
    }
}

struct alwan__cv_dct_plan {
    int n;
    int inverse;
    int nf;
    int factors[34];
    int *itab;
    cvx_cf *wave;
    cvx_cf *dct_wave;
    cvx_cf *scratch;
    float *src_buf; /* the DFT's input */
    float *dst_buf; /* its output: src_buf itself when the transform runs in place */
};

/* DCTInit for complex float */
static void cvx_dct_init(int n, cvx_cf *wave, int inv) {
    double wre, wim, w1re, w1im, t, scale;
    int i;

    if (n == 1) return;

    if ((n & (n - 1)) == 0) {
        int m;
        for (m = 0; (unsigned)(1 << m) < (unsigned)n; m++) {
        }
        scale = (!inv ? 2 : 1) * cvx_dctscale[m];
        w1re = cvx_dfttab[m + 2][0];
        w1im = -cvx_dfttab[m + 2][1];
    } else {
        t = 1. / (2 * n);
        scale = (!inv ? 2 : 1) * ALWAN_SQRT_F64(t);
        w1im = ALWAN_SIN_F64(-CVX_PI * t);
        w1re = ALWAN_SQRT_F64(1. - w1im * w1im);
    }
    n >>= 1;

    wre = (float)scale;
    wim = 0.f;

    for (i = 0; i <= n; i++) {
        wave[i].re = (float)wre;
        wave[i].im = (float)wim;
        t = wre * w1re - wim * w1im;
        wim = wre * w1im + wim * w1re;
        wre = t;
    }
}

alwan_status alwan__cv_dct_plan_create(alwan__cv_dct_plan **out, int n, int inverse) {
    alwan__cv_dct_plan *p;
    int inplace;
    *out = NULL;
    if (n < 1 || (n > 1 && (n & 1))) return ALWAN_E_INVALID;
    p = (alwan__cv_dct_plan *)calloc(1, sizeof *p);
    if (!p) return ALWAN_E_NOMEM;
    p->n = n;
    p->inverse = inverse;
    p->nf = cvx_factorize(n, p->factors);
    inplace = p->factors[0] == p->factors[p->nf - 1];
    p->itab = (int *)calloc((size_t)n, sizeof(int));
    p->wave = (cvx_cf *)calloc((size_t)n, sizeof(cvx_cf));
    p->dct_wave = (cvx_cf *)calloc((size_t)n / 2 + 1, sizeof(cvx_cf));
    p->scratch = (cvx_cf *)calloc((size_t)n + 2, sizeof(cvx_cf));
    p->src_buf = (float *)calloc((size_t)n, sizeof(float));
    p->dst_buf = inplace ? p->src_buf : (float *)calloc((size_t)n, sizeof(float));
    if (!p->itab || !p->wave || !p->dct_wave || !p->scratch || !p->src_buf || !p->dst_buf) {
        alwan__cv_dct_plan_destroy(p);
        return ALWAN_E_NOMEM;
    }
    {
        int *tmp = (int *)calloc((size_t)n, sizeof(int));
        if (!tmp) {
            alwan__cv_dct_plan_destroy(p);
            return ALWAN_E_NOMEM;
        }
        cvx_dft_init(n, p->nf, p->factors, p->itab, p->wave, inverse, tmp);
        free(tmp);
    }
    cvx_dct_init(n, p->dct_wave, inverse);
    *out = p;
    return ALWAN_OK;
}

void alwan__cv_dct_plan_destroy(alwan__cv_dct_plan *p) {
    if (!p) return;
    free(p->itab);
    free(p->wave);
    free(p->dct_wave);
    free(p->scratch);
    if (p->dst_buf != p->src_buf) free(p->dst_buf);
    free(p->src_buf);
    free(p);
}

static void cvx_opts_for(alwan__cv_dct_plan const *p, cvx_opts *c) {
    c->n = p->n;
    c->tab_size = p->n;
    c->nf = p->nf;
    c->factors = p->factors;
    c->itab = p->itab;
    c->wave = p->wave;
    c->inverse = 0;
    c->no_permute = 0;
    c->scale = 1.;
    c->scratch = p->scratch;
}

static float const cvx_sin_45 = (float)0.70710678118654752440084436210485;

/* DCT<float> on one row or column; steps in floats */
static void cvx_dct_1d(alwan__cv_dct_plan const *p, float const *src, size_t src_step, float *dst, size_t dst_step) {
    int const n = p->n;
    int j, n2 = n >> 1;
    float *dft_src = p->src_buf, *dft_dst = p->dst_buf;
    float *dst1 = dst + (size_t)(n - 1) * dst_step;
    cvx_cf const *dct_wave = p->dct_wave;
    cvx_opts c;

    if (n == 1) {
        dst[0] = src[0];
        return;
    }

    for (j = 0; j < n2; j++, src += src_step * 2) {
        dft_src[j] = src[0];
        dft_src[n - j - 1] = src[src_step];
    }

    cvx_opts_for(p, &c);
    cvx_real_dft(&c, dft_src, dft_dst);
    src = dft_dst;

    dst[0] = (float)(src[0] * dct_wave->re * cvx_sin_45);
    dst += dst_step;
    for (j = 1, dct_wave++; j < n2; j++, dct_wave++, dst += dst_step, dst1 -= dst_step) {
        float const t0 = dct_wave->re * src[j * 2 - 1] - dct_wave->im * src[j * 2];
        float const t1 = -dct_wave->im * src[j * 2 - 1] - dct_wave->re * src[j * 2];
        dst[0] = t0;
        dst1[0] = t1;
    }

    dst[0] = src[n - 1] * dct_wave->re;
}

/* IDCT<float> */
static void cvx_idct_1d(alwan__cv_dct_plan const *p, float const *src, size_t src_step, float *dst, size_t dst_step) {
    int const n = p->n;
    int j, n2 = n >> 1;
    float *dft_src = p->src_buf, *dft_dst = p->dst_buf;
    float const *src1 = src + (size_t)(n - 1) * src_step;
    cvx_cf const *dct_wave = p->dct_wave;
    cvx_opts c;

    if (n == 1) {
        dst[0] = src[0];
        return;
    }

    dft_src[0] = (float)(src[0] * 2 * dct_wave->re * cvx_sin_45);
    src += src_step;
    for (j = 1, dct_wave++; j < n2; j++, dct_wave++, src += src_step, src1 -= src_step) {
        float const t0 = dct_wave->re * src[0] - dct_wave->im * src1[0];
        float const t1 = -dct_wave->im * src[0] - dct_wave->re * src1[0];
        dft_src[j * 2 - 1] = t0;
        dft_src[j * 2] = t1;
    }

    dft_src[n - 1] = (float)(src[0] * 2 * dct_wave->re);
    cvx_opts_for(p, &c);
    cvx_ccs_idft(&c, dft_src, dft_dst);

    for (j = 0; j < n2; j++, dst += dst_step * 2) {
        dst[0] = dft_dst[j];
        dst[dst_step] = dft_dst[n - j - 1];
    }
}

/* OcvDctImpl::apply on an n x n block: the rows, then the columns of the result. Row
 * strides in floats; dst may be src. */
void alwan__cv_dct2d(alwan__cv_dct_plan const *p, float const *src, size_t src_stride, float *dst, size_t dst_stride) {
    int const n = p->n;
    int i;
    for (i = 0; i < n; i++) {
        if (p->inverse)
            cvx_idct_1d(p, src + (size_t)i * src_stride, 1, dst + (size_t)i * dst_stride, 1);
        else
            cvx_dct_1d(p, src + (size_t)i * src_stride, 1, dst + (size_t)i * dst_stride, 1);
    }
    for (i = 0; i < n; i++) {
        if (p->inverse)
            cvx_idct_1d(p, dst + i, dst_stride, dst + i, dst_stride);
        else
            cvx_dct_1d(p, dst + i, dst_stride, dst + i, dst_stride);
    }
}
