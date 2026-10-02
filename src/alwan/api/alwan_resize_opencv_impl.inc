/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * One pixel type's cv::resize, INTER_CUBIC and INTER_AREA (see alwan_resize_opencv.c),
 * ported from OpenCV 5.0.0's modules/imgproc/src/resize.cpp under its 3-clause BSD notice
 * (Intel, Willow Garage, Itseez), reproduced in alwan_resize_opencv.c and in
 * licenses/OpenCV-imgproc-resize-BSD-3-Clause.txt.
 * No include guard: included once per type with
 *   CVR_T    the pixel type        CVR_WT  the row buffer type (int, float, double)
 *   CVR_AT   the coefficient type  CVR_AWT the INTER_AREA accumulator (float, float, double)
 *   CVR_AWF  the fast INTER_AREA accumulator (int, float, double)
 *   CVR_U8   8-bit (fixed point)   CVR_F32 float32 (OpenCV's SSE paths)
 *   CVR_FN(name) the function name for this type
 */

/* resizeGeneric_ with HResizeCubic / VResizeCubic (cubic) or, for INTER_AREA that
 * enlarges along an axis, HResizeLinear / VResizeLinear on the area-mode coefficients. */
static alwan_status CVR_FN(generic)(char *dst, size_t drs, int dw, int dh, char const *src, size_t srs, int sw, int sh, int cn,
                                    int cubic) {
    double const inv_scale_x = (double)dw / sw, inv_scale_y = (double)dh / sh;
    double const scale_x = 1. / inv_scale_x, scale_y = 1. / inv_scale_y;
    int const ksize = cubic ? 4 : 2, ksize2 = ksize / 2;
    int const width = dw * cn, swidth = sw * cn;
    int xmin = 0, xmax = dw, dx, dy, k, j;
    int *xofs, *yofs;
    CVR_AT *alpha, *beta;
    CVR_WT *rows;
    float cbuf[4];
    xofs = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)width + (size_t)dh, sizeof(int)), sizeof(int));
    alpha = (CVR_AT *)ALWAN_ALLOC(alwan_safe_array_size(((size_t)width + (size_t)dh) * (size_t)ksize, sizeof(CVR_AT)), sizeof(double));
    rows = (CVR_WT *)ALWAN_ALLOC(alwan_safe_array_size((size_t)width * (size_t)ksize, sizeof(CVR_WT)), sizeof(double));
    if (!xofs || !alpha || !rows) {
        ALWAN_FREE(xofs);
        ALWAN_FREE(alpha);
        ALWAN_FREE(rows);
        return ALWAN_E_NOMEM;
    }
    yofs = xofs + width;
    beta = alpha + (size_t)width * (size_t)ksize;
    for (dx = 0; dx < dw; dx++) {
        int sx;
        float fx;
        if (cubic) {
            fx = (float)((dx + 0.5) * scale_x - 0.5);
            sx = cvr_floor((double)fx);
            fx -= (float)sx;
        } else {
            sx = cvr_floor(dx * scale_x);
            fx = (float)((dx + 1) - (sx + 1) * inv_scale_x);
            fx = fx <= 0 ? 0.f : fx - (float)cvr_floor((double)fx);
        }
        if (sx < ksize2 - 1) {
            xmin = dx + 1;
            if (sx < 0 && !cubic) fx = 0, sx = 0;
        }
        if (sx + ksize2 >= sw) {
            if (dx < xmax) xmax = dx;
            if (sx >= sw - 1 && !cubic) fx = 0, sx = sw - 1;
        }
        for (k = 0; k < cn; k++) xofs[dx * cn + k] = sx * cn + k;
        if (cubic) cvr_cubic_coeffs(fx, cbuf);
        else cbuf[0] = 1.f - fx, cbuf[1] = fx;
        for (k = 0; k < cn; k++)
            for (j = 0; j < ksize; j++) {
#if CVR_U8
                alpha[(dx * cn + k) * ksize + j] = cvr_sat_short(cbuf[j] * 2048);
#else
                alpha[(dx * cn + k) * ksize + j] = cbuf[j];
#endif
            }
    }
    for (dy = 0; dy < dh; dy++) {
        int sy;
        float fy;
        if (cubic) {
            fy = (float)((dy + 0.5) * scale_y - 0.5);
            sy = cvr_floor((double)fy);
            fy -= (float)sy;
        } else {
            sy = cvr_floor(dy * scale_y);
            fy = (float)((dy + 1) - (sy + 1) * inv_scale_y);
            fy = fy <= 0 ? 0.f : fy - (float)cvr_floor((double)fy);
        }
        yofs[dy] = sy;
        if (cubic) cvr_cubic_coeffs(fy, cbuf);
        else cbuf[0] = 1.f - fy, cbuf[1] = fy;
        for (j = 0; j < ksize; j++) {
#if CVR_U8
            beta[dy * ksize + j] = cvr_sat_short(cbuf[j] * 2048);
#else
            beta[dy * ksize + j] = cbuf[j];
#endif
        }
    }
    xmin *= cn;
    xmax *= cn;
    for (dy = 0; dy < dh; dy++) {
        CVR_AT const *b = beta + (size_t)dy * (size_t)ksize;
        CVR_T *D = (CVR_T *)(dst + (size_t)dy * drs);
        int x;
        for (k = 0; k < ksize; k++) {
            int sy = yofs[dy] - ksize2 + 1 + k;
            CVR_T const *S;
            CVR_WT *R = rows + (size_t)k * (size_t)width;
            sy = sy >= 0 ? (sy < sh ? sy : sh - 1) : 0;
            S = (CVR_T const *)(src + (size_t)sy * srs);
            if (cubic) {
                int limit = xmin;
                CVR_AT const *a = alpha;
                dx = 0;
                for (;;) {
                    for (; dx < limit; dx++) {
                        int sxb = xofs[dx] - cn;
                        CVR_WT v = 0;
                        for (j = 0; j < 4; j++) {
                            int sxj = sxb + j * cn;
                            if ((unsigned)sxj >= (unsigned)swidth) {
                                while (sxj < 0) sxj += cn;
                                while (sxj >= swidth) sxj -= cn;
                            }
                            v += (CVR_WT)(S[sxj] * a[dx * 4 + j]);
                        }
                        R[dx] = v;
                    }
                    if (limit == width) break;
                    for (; dx < xmax; dx++) {
                        int const sx = xofs[dx];
                        CVR_AT const *c = a + dx * 4;
                        R[dx] = (CVR_WT)(S[sx - cn] * c[0]) + (CVR_WT)(S[sx] * c[1]) + (CVR_WT)(S[sx + cn] * c[2]) +
                                (CVR_WT)(S[sx + 2 * cn] * c[3]);
                    }
                    limit = width;
                }
            } else {
                for (dx = 0; dx < width; dx++) {
                    int const sx = xofs[dx];
#if CVR_U8
                    R[dx] = dx < xmax ? (int)S[sx] * alpha[dx * 2] + (int)S[sx + cn] * alpha[dx * 2 + 1] : (int)S[sx] * 2048;
#else
                    R[dx] = dx < xmax ? (CVR_WT)(S[sx] * alpha[dx * 2]) + (CVR_WT)(S[sx + cn] * alpha[dx * 2 + 1]) : (CVR_WT)S[sx];
#endif
                }
            }
        }
        if (cubic) {
            CVR_WT const *S0 = rows, *S1 = rows + width, *S2 = rows + 2 * (size_t)width, *S3 = rows + 3 * (size_t)width;
#if CVR_U8
            /* VResizeCubicVec_32s8u at OpenCV's SSE baseline: eight lanes in float, the
             * coefficients scaled by 2^-22, rounded to even; then the fixed-point tail */
            float const sc = 1.f / (2048 * 2048);
            float const b0 = b[0] * sc, b1 = b[1] * sc, b2 = b[2] * sc, b3 = b[3] * sc;
            int const xv = width >= 8 ? width - width % 8 : 0;
            for (x = 0; x < xv; x++) {
                float t = (float)S3[x] * b3;
                long r;
                t = (float)S2[x] * b2 + t;
                t = (float)S1[x] * b1 + t;
                t = (float)S0[x] * b0 + t;
                r = cvr_round_even((double)t);
                r = r < -32768 ? -32768 : r > 32767 ? 32767 : r;
                D[x] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r);
            }
            for (; x < width; x++) {
                int const v = (S0[x] * b[0] + S1[x] * b[1] + S2[x] * b[2] + S3[x] * b[3] + (1 << 21)) >> 22;
                D[x] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
            }
#elif CVR_F32
            /* VResizeCubicVec_32f: S0 b0 + (S1 b1 + (S2 b2 + S3 b3)), unfused, four lanes */
            int const xv = width - width % 4;
            for (x = 0; x < xv; x++) {
                float t = S3[x] * b[3];
                t = S2[x] * b[2] + t;
                t = S1[x] * b[1] + t;
                t = S0[x] * b[0] + t;
                D[x] = t;
            }
            for (; x < width; x++) D[x] = S0[x] * b[0] + S1[x] * b[1] + S2[x] * b[2] + S3[x] * b[3];
#else
            for (x = 0; x < width; x++)
                D[x] = S0[x] * (double)b[0] + S1[x] * (double)b[1] + S2[x] * (double)b[2] + S3[x] * (double)b[3];
#endif
        } else {
            CVR_WT const *S0 = rows, *S1 = rows + width;
#if CVR_U8
            /* VResizeLinear's 8-bit specialisation (its SSE form saturates; the values stay
             * in range for these coefficients) */
            for (x = 0; x < width; x++) {
                int const v = (((b[0] * (S0[x] >> 4)) >> 16) + ((b[1] * (S1[x] >> 4)) >> 16) + 2) >> 2;
                D[x] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
            }
#elif CVR_F32
            for (x = 0; x < width; x++) D[x] = S0[x] * b[0] + S1[x] * b[1];
#else
            for (x = 0; x < width; x++) D[x] = S0[x] * (double)b[0] + S1[x] * (double)b[1];
#endif
        }
    }
    ALWAN_FREE(xofs);
    ALWAN_FREE(alpha);
    ALWAN_FREE(rows);
    return ALWAN_OK;
}

/* resizeAreaFast_: an integer factor each way, each output the mean of its block. */
static alwan_status CVR_FN(area_fast)(char *dst, size_t drs, int dw, int dh, char const *src, size_t srs, int sw, int sh, int cn, int isx,
                                      int isy) {
    int const area = isx * isy;
    float const scale = 1.f / area;
    int const dwidth1 = (sw / isx) * cn, width = dw * cn, swidth = sw * cn;
    int dy, dx, k, sy, sx;
    int *ofs = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)area * 2 + (size_t)width, sizeof(int)), sizeof(int));
    int *ofy, *xofs;
#if CVR_U8
    int const fast2 = isx == 2 && isy == 2 && (cn == 1 || cn == 3 || cn == 4);
#elif CVR_F32
    int const fast2 = isx == 2 && isy == 2 && (cn == 1 || cn == 4);
#else
    int const fast2 = 0;
#endif
    if (!ofs) return ALWAN_E_NOMEM;
    ofy = ofs + area;
    xofs = ofs + 2 * area;
    for (sy = 0, k = 0; sy < isy; sy++)
        for (sx = 0; sx < isx; sx++, k++) {
            ofy[k] = sy;
            ofs[k] = sx * cn;
        }
    for (dx = 0; dx < dw; dx++)
        for (k = 0; k < cn; k++) xofs[dx * cn + k] = isx * dx * cn + k;
    for (dy = 0; dy < dh; dy++) {
        CVR_T *D = (CVR_T *)(dst + (size_t)dy * drs);
        int const sy0 = dy * isy;
        int const w = sy0 + isy <= sh ? dwidth1 : 0;
        dx = 0;
        if (sy0 >= sh) {
            for (dx = 0; dx < width; dx++) D[dx] = 0;
            continue;
        }
        if (fast2) {
            CVR_T const *R0 = (CVR_T const *)(src + (size_t)sy0 * srs), *R1 = (CVR_T const *)(src + (size_t)(sy0 + 1) * srs);
#if CVR_U8
            /* ResizeAreaFastVec: (a + b + c + d + 2) >> 2, its SIMD and scalar forms alike */
            for (; dx < w; dx++) {
                int const i = (dx / cn) * 2 * cn + dx % cn;
                D[dx] = (unsigned char)((R0[i] + R0[i + cn] + R1[i] + R1[i + cn] + 2) >> 2);
            }
#else
            /* ResizeAreaFastVec_SIMD_32f: ((a + b) + (c + d)) 0.25, whole vectors of four */
            int const wv = w - w % 4;
            for (; dx < wv; dx++) {
                int const i = (dx / cn) * 2 * cn + dx % cn;
                D[dx] = ((R0[i] + R0[i + cn]) + (R1[i] + R1[i + cn])) * 0.25f;
            }
#endif
        }
        for (; dx < w; dx++) {
            CVR_T const *S = (CVR_T const *)(src + (size_t)sy0 * srs);
            CVR_AWF sum = 0;
            k = 0;
            for (; k <= area - 4; k += 4) {
                CVR_AWF const p0 = (CVR_AWF)((CVR_T const *)((char const *)S + (size_t)ofy[k] * srs))[xofs[dx] + ofs[k]];
                CVR_AWF const p1 = (CVR_AWF)((CVR_T const *)((char const *)S + (size_t)ofy[k + 1] * srs))[xofs[dx] + ofs[k + 1]];
                CVR_AWF const p2 = (CVR_AWF)((CVR_T const *)((char const *)S + (size_t)ofy[k + 2] * srs))[xofs[dx] + ofs[k + 2]];
                CVR_AWF const p3 = (CVR_AWF)((CVR_T const *)((char const *)S + (size_t)ofy[k + 3] * srs))[xofs[dx] + ofs[k + 3]];
                sum += p0 + p1 + p2 + p3;
            }
            for (; k < area; k++) sum += (CVR_AWF)((CVR_T const *)((char const *)S + (size_t)ofy[k] * srs))[xofs[dx] + ofs[k]];
#if CVR_U8
            D[dx] = cvr_sat_u8((float)sum * scale);
#elif CVR_F32
            D[dx] = sum * scale;
#else
            D[dx] = sum * (double)scale;
#endif
        }
        for (; dx < width; dx++) {
            CVR_AWF sum = 0;
            int count = 0, sx0 = xofs[dx], yy, xx;
            for (yy = 0; yy < isy; yy++) {
                CVR_T const *S;
                if (sy0 + yy >= sh) break;
                S = (CVR_T const *)(src + (size_t)(sy0 + yy) * srs) + sx0;
                for (xx = 0; xx < isx * cn; xx += cn) {
                    if (sx0 + xx >= swidth) break;
                    sum += (CVR_AWF)S[xx];
                    count++;
                }
            }
#if CVR_U8
            D[dx] = count ? cvr_sat_u8((float)sum / count) : 0;
#else
            D[dx] = count ? (CVR_T)((float)sum / count) : 0;
#endif
        }
    }
    ALWAN_FREE(ofs);
    return ALWAN_OK;
}

/* resizeArea_ on computeResizeAreaTab's weights. */
static alwan_status CVR_FN(area)(char *dst, size_t drs, int dw, int dh, char const *src, size_t srs, int sw, int sh, int cn,
                                 double scale_x, double scale_y) {
    int const width = dw * cn;
    cvr_decimate *xtab = (cvr_decimate *)ALWAN_ALLOC(alwan_safe_array_size(((size_t)sw + (size_t)sh) * 2, sizeof(cvr_decimate)), sizeof(double));
    cvr_decimate *ytab;
    CVR_AWT *buf = (CVR_AWT *)ALWAN_ALLOC(alwan_safe_array_size((size_t)width * 2, sizeof(CVR_AWT)), sizeof(double));
    CVR_AWT *sum;
    int xn, yn, j, k, x, prev_dy;
    if (!xtab || !buf) {
        ALWAN_FREE(xtab);
        ALWAN_FREE(buf);
        return ALWAN_E_NOMEM;
    }
    ytab = xtab + (size_t)sw * 2;
    sum = buf + width;
    xn = cvr_area_tab(sw, dw, cn, scale_x, xtab);
    yn = cvr_area_tab(sh, dh, 1, scale_y, ytab);
    for (x = 0; x < width; x++) sum[x] = 0;
    prev_dy = ytab[0].di;
    for (j = 0; j < yn; j++) {
        CVR_AWT const bt = (CVR_AWT)ytab[j].alpha;
        int const dy = ytab[j].di, sy = ytab[j].si;
        CVR_T const *S = (CVR_T const *)(src + (size_t)sy * srs);
        for (x = 0; x < width; x++) buf[x] = 0;
        for (k = 0; k < xn; k++) {
            int const sxn = xtab[k].si, dxn = xtab[k].di, c0 = 0;
            CVR_AWT const a = (CVR_AWT)xtab[k].alpha;
            int c;
            for (c = c0; c < cn; c++) buf[dxn + c] = buf[dxn + c] + (CVR_AWT)S[sxn + c] * a;
        }
        if (dy != prev_dy) {
            CVR_T *D = (CVR_T *)(dst + (size_t)prev_dy * drs);
            for (x = 0; x < width; x++) {
#if CVR_U8
                D[x] = cvr_sat_u8(sum[x]);
#else
                D[x] = (CVR_T)sum[x];
#endif
                sum[x] = bt * buf[x];
            }
            prev_dy = dy;
        } else {
            for (x = 0; x < width; x++) sum[x] += bt * buf[x];
        }
    }
    {
        CVR_T *D = (CVR_T *)(dst + (size_t)prev_dy * drs);
        for (x = 0; x < width; x++) {
#if CVR_U8
            D[x] = cvr_sat_u8(sum[x]);
#else
            D[x] = (CVR_T)sum[x];
#endif
        }
    }
    ALWAN_FREE(xtab);
    ALWAN_FREE(buf);
    return ALWAN_OK;
}

static alwan_status CVR_FN(resize)(char *dst, size_t drs, int dw, int dh, char const *src, size_t srs, int sw, int sh, int cn, int area) {
    double const inv_scale_x = (double)dw / sw, inv_scale_y = (double)dh / sh;
    double const scale_x = 1. / inv_scale_x, scale_y = 1. / inv_scale_y;
    if (area && scale_x >= 1 && scale_y >= 1) {
        long const isx = cvr_round_even(scale_x), isy = cvr_round_even(scale_y);
        double const ex = scale_x - (double)isx, ey = scale_y - (double)isy;
        if ((ex < 0 ? -ex : ex) < DBL_EPSILON && (ey < 0 ? -ey : ey) < DBL_EPSILON)
            return CVR_FN(area_fast)(dst, drs, dw, dh, src, srs, sw, sh, cn, (int)isx, (int)isy);
        return CVR_FN(area)(dst, drs, dw, dh, src, srs, sw, sh, cn, scale_x, scale_y);
    }
    return CVR_FN(generic)(dst, drs, dw, dh, src, srs, sw, sh, cn, !area);
}
