/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The joint (cross) bilateral filter: Petschnigg et al., "Digital Photography with Flash
 * and No-Flash Image Pairs", and Eisemann and Durand, SIGGRAPH 2004, after Tomasi and
 * Manduchi's bilateral filter (ICCV 1998). Each output pixel is a weighted mean of the
 * source over a disc, weighted by distance and by how close a JOINT image's value there is
 * to its value at the centre, so edges come from the joint image: denoise a no-flash image
 * along the flash image's edges, or smooth a map along a photograph's. With the source as
 * its own joint it is the ordinary bilateral filter.
 *
 *   w(x, y) = exp(-|x - y|^2 / (2 sigma_space^2)) exp(-d(J(x), J(y))^2 / (2 sigma_color^2))
 *
 * over y in the disc |x - y| <= radius. It follows OpenCV's ximgproc::jointBilateralFilter
 * for float images: d is the L1 distance, the sum of absolute channel differences; the
 * window is the disc, not the square; and the border is reflected without repeating the
 * edge pixel (BORDER_REFLECT_101: ... c b | a b c ...). OpenCV evaluates the colour
 * Gaussian from a table of 4096 bins a channel with linear interpolation; this evaluates
 * it exactly, in double, which is the difference suite 194 measures. OpenCV falls back to
 * a square GaussianBlur when the joint image is exactly flat; this does not, and gives
 * the disc's spatial Gaussian there.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>

/* cv::borderInterpolate with BORDER_REFLECT_101 */
static size_t alwan_bf_reflect101(long p, size_t n) {
    if (n == 1) return 0;
    while (p < 0 || p >= (long)n) p = p < 0 ? -p : 2 * (long)n - p - 2;
    return (size_t)p;
}

static int alwan_bf_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

static double alwan_bf_get(void const *base, size_t row_stride, size_t ch_count, size_t y, size_t x, size_t c, int is_f32) {
    char const *row = (char const *)base + y * row_stride;
    return is_f32 ? (double)((alwan_f32 const *)row)[x * ch_count + c] : (double)((alwan_f64 const *)row)[x * ch_count + c];
}

static alwan_status alwan_bf_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t src_ch,
                                 void const *joint, size_t joint_row_stride, size_t joint_ch, size_t w, size_t h,
                                 size_t radius, double sigma_color, double sigma_space, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    long const r = (long)radius;
    double *buf, *jb, *sb;
    double const gc = -0.5 / (sigma_color * sigma_color), gs = -0.5 / (sigma_space * sigma_space);
    size_t x, y, c;
    if (!out || !src || !joint) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || src_ch == 0 || src_ch > 4 || joint_ch == 0 || joint_ch > 4) return ALWAN_E_INVALID;
    if (!(sigma_color > 0.0) || !(sigma_space > 0.0) || !alwan_bf_finite(sigma_color) || !alwan_bf_finite(sigma_space)) {
        return ALWAN_E_INVALID;
    }
    if (src_row_stride / elem / src_ch < w || joint_row_stride / elem / joint_ch < w || out_row_stride / elem / src_ch < w) {
        return ALWAN_E_INVALID;
    }
    if (radius == 0 || radius > 4096 || n / w != h) return ALWAN_E_RANGE;
    /* copies of both, so out may be either */
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, (src_ch + joint_ch) * sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    jb = buf;
    sb = buf + n * joint_ch;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            for (c = 0; c < joint_ch; c++) {
                double const v = alwan_bf_get(joint, joint_row_stride, joint_ch, y, x, c, is_f32);
                if (!alwan_bf_finite(v)) { ALWAN_FREE(buf); return ALWAN_E_INVALID; }
                jb[(y * w + x) * joint_ch + c] = v;
            }
            for (c = 0; c < src_ch; c++) {
                double const v = alwan_bf_get(src, src_row_stride, src_ch, y, x, c, is_f32);
                if (!alwan_bf_finite(v)) { ALWAN_FREE(buf); return ALWAN_E_INVALID; }
                sb[(y * w + x) * src_ch + c] = v;
            }
        }
    }
    for (y = 0; y < h; y++) {
        char *orow = (char *)out + y * out_row_stride;
        for (x = 0; x < w; x++) {
            double const *j0 = jb + (y * w + x) * joint_ch;
            double sum[4] = { 0, 0, 0, 0 }, wsum = 0.0;
            long i, j;
            for (i = -r; i <= r; i++) {
                size_t const yy = alwan_bf_reflect101((long)y + i, h);
                for (j = -r; j <= r; j++) {
                    double const r2 = (double)(i * i + j * j);
                    size_t const xx = alwan_bf_reflect101((long)x + j, w);
                    double const *jp = jb + (yy * w + xx) * joint_ch, *sp = sb + (yy * w + xx) * src_ch;
                    double dist = 0.0, wt;
                    if (r2 > (double)(r * r)) continue;
                    for (c = 0; c < joint_ch; c++) dist += fabs(j0[c] - jp[c]);
                    wt = exp(r2 * gs) * exp(dist * dist * gc);
                    for (c = 0; c < src_ch; c++) sum[c] += wt * sp[c];
                    wsum += wt;
                }
            }
            for (c = 0; c < src_ch; c++) {
                double const v = sum[c] / wsum;
                if (is_f32) ((alwan_f32 *)orow)[x * src_ch + c] = (alwan_f32)v;
                else ((alwan_f64 *)orow)[x * src_ch + c] = (alwan_f64)v;
            }
        }
    }
    ALWAN_FREE(buf);
    return ALWAN_OK;
}

/* The rolling guidance filter (Zhang, Shen, Xu and Jia, "Rolling Guidance Filter", ECCV
 * 2014): the joint bilateral filter iterated with its own last output as the joint image.
 * The paper starts from a Gaussian of the source (the joint bilateral filter with a
 * constant guide), which removes structures smaller than sigma_space, and the iterations
 * then bring large edges back; OpenCV's ximgproc::rollingGuidanceFilter starts from the
 * source itself. from_gaussian picks: 1 the paper, 0 OpenCV. */
static alwan_status alwan_rgf_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t ch,
                                  size_t w, size_t h, size_t radius, double sigma_color, double sigma_space,
                                  size_t iterations, int from_gaussian, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const row = w * ch * elem;
    char *copy;
    size_t k, y;
    alwan_status st;
    if (!out || !src) return ALWAN_E_INVALID;
    if (iterations == 0 || iterations > 1000) return ALWAN_E_RANGE;
    if (w == 0 || h == 0 || ch == 0 || ch > 4 || out_row_stride / elem / ch < w || src_row_stride / elem / ch < w) {
        return ALWAN_E_INVALID;
    }
    /* the source, kept apart from out: every iteration filters the ORIGINAL source, and out
     * may be src. One extra plane of w * h serves as the constant guide. */
    copy = (char *)ALWAN_ALLOC(alwan_safe_array_size(h, row + w * elem), sizeof(double));
    if (!copy) return ALWAN_E_NOMEM;
    for (y = 0; y < h; y++) {
        char const *s = (char const *)src + y * src_row_stride;
        for (k = 0; k < row; k++) copy[y * row + k] = s[k];
    }
    if (from_gaussian) {
        /* a constant joint image: every colour weight is 1, which leaves the spatial Gaussian */
        char *flat = copy + h * row;
        for (k = 0; k < w * h * elem; k++) flat[k] = 0;   /* all-zero bits: 0.0 in either precision */
        st = alwan_bf_run(out, out_row_stride, copy, row, ch, flat, w * elem, 1, w, h, radius,
                          sigma_color, sigma_space, is_f32);
    } else {
        st = alwan_bf_run(out, out_row_stride, copy, row, ch, copy, row, ch, w, h, radius,
                          sigma_color, sigma_space, is_f32);
    }
    for (k = 1; k < iterations && st == ALWAN_OK; k++) {
        st = alwan_bf_run(out, out_row_stride, copy, row, ch, out, out_row_stride, ch, w, h, radius,
                          sigma_color, sigma_space, is_f32);
    }
    ALWAN_FREE(copy);
    return st;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_joint_bilateral_filter_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src,
                                              size_t src_row_stride, size_t src_channels, alwan_f64 const *joint,
                                              size_t joint_row_stride, size_t joint_channels, size_t width,
                                              size_t height, size_t radius, alwan_f64 sigma_color, alwan_f64 sigma_space) {
    return alwan_bf_run(out, out_row_stride, src, src_row_stride, src_channels, joint, joint_row_stride, joint_channels,
                        width, height, radius, (double)sigma_color, (double)sigma_space, 0);
}
alwan_status alwan_rolling_guidance_filter_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src,
                                               size_t src_row_stride, size_t channels, size_t width, size_t height,
                                               size_t radius, alwan_f64 sigma_color, alwan_f64 sigma_space,
                                               size_t iterations, int from_gaussian) {
    return alwan_rgf_run(out, out_row_stride, src, src_row_stride, channels, width, height, radius,
                         (double)sigma_color, (double)sigma_space, iterations, from_gaussian != 0, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_joint_bilateral_filter_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src,
                                              size_t src_row_stride, size_t src_channels, alwan_f32 const *joint,
                                              size_t joint_row_stride, size_t joint_channels, size_t width,
                                              size_t height, size_t radius, alwan_f32 sigma_color, alwan_f32 sigma_space) {
    return alwan_bf_run(out, out_row_stride, src, src_row_stride, src_channels, joint, joint_row_stride, joint_channels,
                        width, height, radius, (double)sigma_color, (double)sigma_space, 1);
}
alwan_status alwan_rolling_guidance_filter_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src,
                                               size_t src_row_stride, size_t channels, size_t width, size_t height,
                                               size_t radius, alwan_f32 sigma_color, alwan_f32 sigma_space,
                                               size_t iterations, int from_gaussian) {
    return alwan_rgf_run(out, out_row_stride, src, src_row_stride, channels, width, height, radius,
                         (double)sigma_color, (double)sigma_space, iterations, from_gaussian != 0, 1);
}
#endif
