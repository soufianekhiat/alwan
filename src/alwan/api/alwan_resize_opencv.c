/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ALWAN_RESIZE_OPENCV_CUBIC and ALWAN_RESIZE_OPENCV_AREA: cv::resize with INTER_CUBIC and
 * INTER_AREA, a port of OpenCV 5.0.0's modules/imgproc/src/resize.cpp (the non-IPP code a
 * pip build of cv2 runs with IPP off). Suite 291 holds this to cv2.resize value for value.
 * resize.cpp carries this notice, kept here as its conditions ask (the full text is in
 * licenses/OpenCV-imgproc-resize-BSD-3-Clause.txt; see THIRD_PARTY_NOTICES.md):
 *
 *   License Agreement For Open Source Computer Vision Library
 *   Copyright (C) 2000-2008, 2017, Intel Corporation, all rights reserved.
 *   Copyright (C) 2009, Willow Garage Inc., all rights reserved.
 *   Copyright (C) 2014-2015, Itseez Inc., all rights reserved.
 *   Third party copyrights are property of their respective owners.
 *   Redistribution and use in source and binary forms, with or without modification, are
 *   permitted provided that the following conditions are met: redistributions of source
 *   code must retain the above copyright notice, this list of conditions and the following
 *   disclaimer; redistributions in binary form must reproduce them in the documentation
 *   and/or other materials provided with the distribution; the name of the copyright
 *   holders may not be used to endorse or promote products derived from this software
 *   without specific prior written permission. This software is provided by the copyright
 *   holders and contributors "as is" and any express or implied warranties, including, but
 *   not limited to, the implied warranties of merchantability and fitness for a particular
 *   purpose are disclaimed. In no event shall the Intel Corporation or contributors be
 *   liable for any direct, indirect, incidental, special, exemplary, or consequential
 *   damages (including, but not limited to, procurement of substitute goods or services;
 *   loss of use, data, or profits; or business interruption) however caused and on any
 *   theory of liability, whether in contract, strict liability, or tort (including
 *   negligence or otherwise) arising in any way out of the use of this software, even if
 *   advised of the possibility of such damage.
 *
 * INTER_CUBIC: each output position (x + 0.5) scale - 0.5, Keys' cubic with a = -0.75
 * (interpolateCubic, in float) over the four pixels around it, the edge pixel repeated,
 * the kernel NOT widened when shrinking (so a strong reduction aliases). The horizontal
 * pass into rows of the buffer type, then the vertical: 8-bit through 11-bit fixed-point
 * coefficients, its vertical pass in OpenCV's SSE form (eight lanes in float, rounded to
 * even) and a fixed-point tail; float32 with the vertical sum nested as OpenCV's SSE
 * baseline evaluates it over whole vectors of four; double left to right.
 *
 * INTER_AREA: shrinking both ways by integer factors, each output the mean of its block
 * (8-bit at a factor of 2 with 1, 3 or 4 channels as (a + b + c + d + 2) >> 2, otherwise
 * the float mean rounded to even); by other factors, computeResizeAreaTab's per-axis
 * weights accumulated in float (double for double); enlarging along either axis,
 * OpenCV's bilinear on its area-mode coefficients.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

static int cvr_floor(double v) {
    return (int)ALWAN_FLOOR_F64(v);
}

/* cvRound: to nearest, ties to even (SSE2's cvtsd2si under the default rounding mode). */
static long cvr_round_even(double v) {
    double const f = ALWAN_FLOOR_F64(v), d = v - f;
    long r = (long)f;
    if (d > 0.5 || (d == 0.5 && (r & 1))) r++;
    return r;
}

static short cvr_sat_short(float v) {
    long const r = cvr_round_even((double)v);
    return (short)(r < -32768 ? -32768 : r > 32767 ? 32767 : r);
}

static unsigned char cvr_sat_u8(float v) {
    long const r = cvr_round_even((double)v);
    return (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r);
}

/* interpolateCubic, in float as OpenCV computes it. */
static void cvr_cubic_coeffs(float x, float *c) {
    float const A = -0.75f;
    c[0] = ((A * (x + 1) - 5 * A) * (x + 1) + 8 * A) * (x + 1) - 4 * A;
    c[1] = ((A + 2) * x - (A + 3)) * x * x + 1;
    c[2] = ((A + 2) * (1 - x) - (A + 3)) * (1 - x) * (1 - x) + 1;
    c[3] = 1.f - c[0] - c[1] - c[2];
}

typedef struct {
    int si, di;
    float alpha;
} cvr_decimate;

/* computeResizeAreaTab. */
static int cvr_area_tab(int ssize, int dsize, int cn, double scale, cvr_decimate *tab) {
    int k = 0, dx;
    for (dx = 0; dx < dsize; dx++) {
        double const fsx1 = dx * scale, fsx2 = fsx1 + scale;
        double const cellWidth = scale < ssize - fsx1 ? scale : ssize - fsx1;
        int sx1 = (int)ALWAN_CEIL_F64(fsx1), sx2 = cvr_floor(fsx2), sx;
        sx2 = sx2 < ssize - 1 ? sx2 : ssize - 1;
        sx1 = sx1 < sx2 ? sx1 : sx2;
        if (sx1 - fsx1 > 1e-3) {
            tab[k].di = dx * cn;
            tab[k].si = (sx1 - 1) * cn;
            tab[k++].alpha = (float)((sx1 - fsx1) / cellWidth);
        }
        for (sx = sx1; sx < sx2; sx++) {
            tab[k].di = dx * cn;
            tab[k].si = sx * cn;
            tab[k++].alpha = (float)(1.0 / cellWidth);
        }
        if (fsx2 - sx2 > 1e-3) {
            double m = fsx2 - sx2 < 1. ? fsx2 - sx2 : 1.;
            m = m < cellWidth ? m : cellWidth;
            tab[k].di = dx * cn;
            tab[k].si = sx2 * cn;
            tab[k++].alpha = (float)(m / cellWidth);
        }
    }
    return k;
}

/* 8-bit */
#define CVR_T unsigned char
#define CVR_WT int
#define CVR_AT short
#define CVR_AWT float
#define CVR_AWF int
#define CVR_U8 1
#define CVR_F32 0
#define CVR_FN(n) cvr_##n##_u8
#include "alwan_resize_opencv_impl.inc"
#undef CVR_T
#undef CVR_WT
#undef CVR_AT
#undef CVR_AWT
#undef CVR_AWF
#undef CVR_U8
#undef CVR_F32
#undef CVR_FN

/* float32 */
#define CVR_T float
#define CVR_WT float
#define CVR_AT float
#define CVR_AWT float
#define CVR_AWF float
#define CVR_U8 0
#define CVR_F32 1
#define CVR_FN(n) cvr_##n##_f32
#include "alwan_resize_opencv_impl.inc"
#undef CVR_T
#undef CVR_WT
#undef CVR_AT
#undef CVR_AWT
#undef CVR_AWF
#undef CVR_U8
#undef CVR_F32
#undef CVR_FN

/* double */
#define CVR_T double
#define CVR_WT double
#define CVR_AT float
#define CVR_AWT double
#define CVR_AWF double
#define CVR_U8 0
#define CVR_F32 0
#define CVR_FN(n) cvr_##n##_f64
#include "alwan_resize_opencv_impl.inc"
#undef CVR_T
#undef CVR_WT
#undef CVR_AT
#undef CVR_AWT
#undef CVR_AWF
#undef CVR_U8
#undef CVR_F32
#undef CVR_FN

alwan_status alwan__cv_resize(void *out, size_t out_rs, size_t ow, size_t oh, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                              int area, int kind) {
    char *d = (char *)out;
    char const *s = (char const *)src;
    if (kind == 0) return cvr_resize_f64(d, out_rs, (int)ow, (int)oh, s, src_rs, (int)w, (int)h, (int)ch, area);
    if (kind == 1) return cvr_resize_f32(d, out_rs, (int)ow, (int)oh, s, src_rs, (int)w, (int)h, (int)ch, area);
    return cvr_resize_u8(d, out_rs, (int)ow, (int)oh, s, src_rs, (int)w, (int)h, (int)ch, area);
}
