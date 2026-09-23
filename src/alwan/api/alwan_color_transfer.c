/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Colour transfer: Reinhard, Ashikhmin, Gooch and Shirley, "Color Transfer between
 * Images", IEEE CG&A 21(5), 2001. The look of one image carried onto another by matching
 * the mean and standard deviation of each channel in Ruderman's l alpha beta space, whose
 * channels are close to decorrelated for natural images, so a per-channel shift and
 * scale moves colour without the cross-talk it would cause in RGB.
 *
 *   LMS  = M RGB                      M as printed in the paper (RGB -> XYZ, BT.709 and
 *                                     D65, then XYZ -> LMS, combined, four decimals)
 *   lab  = diag(1/sqrt3, 1/sqrt6, 1/sqrt2) [1 1 1; 1 1 -2; 1 -1 0] log10 LMS
 *   lab' = (sigma_ref / sigma_src) (lab - mean_src) + mean_ref, per channel
 *   RGB' = M^-1 10^([1 1 1; 1 1 -1; 1 -2 0] diag(sqrt3/3, sqrt6/6, sqrt2/2) lab')
 *
 * The paper prints M^-1 rounded to four decimals; this uses the exact inverse of the
 * printed M, so a colour with nothing to transfer comes back as itself. An LMS value is
 * floored at 1e-6 before the logarithm (the paper is on 8-bit images and does not say
 * what it does with a zero). The standard deviations are population ones (divided by n);
 * a source channel with zero spread is shifted but not scaled. The output is not clamped.
 * No implementation of the paper's own space exists to compare with; suite 193 checks
 * the property that defines the method, that the result's lab statistics equal the
 * reference's.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>

static double const k_ct_m[9] = { 0.3811, 0.5783, 0.0402, 0.1967, 0.7244, 0.0782, 0.0241, 0.1288, 0.8444 };

static int alwan_ct_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

static void alwan_ct_to_lab(double lab[3], double const rgb[3]) {
    double lg[3];
    int i;
    for (i = 0; i < 3; i++) {
        double lms = k_ct_m[3 * i] * rgb[0] + k_ct_m[3 * i + 1] * rgb[1] + k_ct_m[3 * i + 2] * rgb[2];
        if (!(lms > 1e-6)) lms = 1e-6;
        lg[i] = log10(lms);
    }
    lab[0] = (lg[0] + lg[1] + lg[2]) / sqrt(3.0);
    lab[1] = (lg[0] + lg[1] - 2.0 * lg[2]) / sqrt(6.0);
    lab[2] = (lg[0] - lg[1]) / sqrt(2.0);
}

static void alwan_ct_from_lab(double rgb[3], double const lab[3], double const minv[9]) {
    double const a = lab[0] * sqrt(3.0) / 3.0, b = lab[1] * sqrt(6.0) / 6.0, c = lab[2] * sqrt(2.0) / 2.0;
    double lms[3];
    int i;
    lms[0] = pow(10.0, a + b + c);
    lms[1] = pow(10.0, a + b - c);
    lms[2] = pow(10.0, a - 2.0 * b);
    for (i = 0; i < 3; i++) rgb[i] = minv[3 * i] * lms[0] + minv[3 * i + 1] * lms[1] + minv[3 * i + 2] * lms[2];
}

static void alwan_ct_inverse(double inv[9]) {
    double const *m = k_ct_m;
    double const c00 = m[4] * m[8] - m[5] * m[7], c01 = m[5] * m[6] - m[3] * m[8], c02 = m[3] * m[7] - m[4] * m[6];
    double const det = m[0] * c00 + m[1] * c01 + m[2] * c02;
    inv[0] = c00 / det; inv[1] = (m[2] * m[7] - m[1] * m[8]) / det; inv[2] = (m[1] * m[5] - m[2] * m[4]) / det;
    inv[3] = c01 / det; inv[4] = (m[0] * m[8] - m[2] * m[6]) / det; inv[5] = (m[2] * m[3] - m[0] * m[5]) / det;
    inv[6] = c02 / det; inv[7] = (m[1] * m[6] - m[0] * m[7]) / det; inv[8] = (m[0] * m[4] - m[1] * m[3]) / det;
}

/* Mean and population standard deviation of the lab channels of count pixels. */
static int alwan_ct_stats(double mean[3], double sd[3], void const *rgb, size_t stride, size_t count, int is_f32) {
    double s[3] = { 0, 0, 0 }, q[3] = { 0, 0, 0 };
    size_t i;
    int c;
    for (i = 0; i < count; i++) {
        char const *p = (char const *)rgb + i * stride;
        double px[3], lab[3];
        for (c = 0; c < 3; c++) {
            px[c] = is_f32 ? (double)((alwan_f32 const *)p)[c] : (double)((alwan_f64 const *)p)[c];
            if (!alwan_ct_finite(px[c])) return 0;
        }
        alwan_ct_to_lab(lab, px);
        for (c = 0; c < 3; c++) s[c] += lab[c];
    }
    for (c = 0; c < 3; c++) mean[c] = s[c] / (double)count;
    for (i = 0; i < count; i++) {   /* a second pass, for the deviation about the mean */
        char const *p = (char const *)rgb + i * stride;
        double px[3], lab[3];
        for (c = 0; c < 3; c++) px[c] = is_f32 ? (double)((alwan_f32 const *)p)[c] : (double)((alwan_f64 const *)p)[c];
        alwan_ct_to_lab(lab, px);
        for (c = 0; c < 3; c++) q[c] += (lab[c] - mean[c]) * (lab[c] - mean[c]);
    }
    for (c = 0; c < 3; c++) sd[c] = sqrt(q[c] / (double)count);
    return 1;
}

static alwan_status alwan_ct_run(void *out, size_t out_stride, void const *src, size_t src_stride, size_t src_count,
                                 void const *ref, size_t ref_stride, size_t ref_count, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    double ms[3], ss[3], mr[3], sr[3], k[3], inv[9];
    size_t i;
    int c;
    if (!out || !src || !ref || src_count == 0 || ref_count == 0) return ALWAN_E_INVALID;
    if (out_stride < 3 * elem || src_stride < 3 * elem || ref_stride < 3 * elem) return ALWAN_E_INVALID;
    if (!alwan_ct_stats(ms, ss, src, src_stride, src_count, is_f32)) return ALWAN_E_INVALID;
    if (!alwan_ct_stats(mr, sr, ref, ref_stride, ref_count, is_f32)) return ALWAN_E_INVALID;
    for (c = 0; c < 3; c++) k[c] = ss[c] > 0.0 ? sr[c] / ss[c] : 1.0;
    alwan_ct_inverse(inv);
    for (i = 0; i < src_count; i++) {
        char const *p = (char const *)src + i * src_stride;
        char *d = (char *)out + i * out_stride;
        double px[3], lab[3], rgb[3];
        for (c = 0; c < 3; c++) px[c] = is_f32 ? (double)((alwan_f32 const *)p)[c] : (double)((alwan_f64 const *)p)[c];
        alwan_ct_to_lab(lab, px);
        for (c = 0; c < 3; c++) lab[c] = k[c] * (lab[c] - ms[c]) + mr[c];
        alwan_ct_from_lab(rgb, lab, inv);
        for (c = 0; c < 3; c++) {
            if (is_f32) ((alwan_f32 *)d)[c] = (alwan_f32)rgb[c];
            else ((alwan_f64 *)d)[c] = (alwan_f64)rgb[c];
        }
    }
    return ALWAN_OK;
}

/* ---- the family: alwan_color_transfer_{T} ---- */

static alwan_status alwan_ctf_run(void *out, size_t out_stride, void const *src, size_t src_stride, size_t src_count,
                                  void const *ref, size_t ref_stride, size_t ref_count, size_t channels,
                                  alwan_color_transfer_method method, alwan_color_transfer_params const *params,
                                  int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    double const amount = params && params->amount != 0.0 ? params->amount : 1.0;
    double *keep = NULL;
    alwan_status st;
    size_t i, c;
    if (!out || !src || !ref || src_count == 0 || channels == 0 || channels > 4) return ALWAN_E_INVALID;
    if (!(amount == amount) || amount < 0.0 || amount > 1.0) return ALWAN_E_RANGE;
    if (method == ALWAN_COLOR_TRANSFER_REINHARD2001 && channels != 3) return ALWAN_E_INVALID;
    if (method != ALWAN_COLOR_TRANSFER_REINHARD2001 && method != ALWAN_COLOR_TRANSFER_HISTOGRAM_MATCH) {
        return ALWAN_E_INVALID;
    }
    if (src_stride / elem < channels || out_stride / elem < channels) return ALWAN_E_INVALID;
    if (amount != 1.0) { /* out may be src: keep the source for the blend */
        keep = (double *)ALWAN_ALLOC(alwan_safe_array_size(src_count, channels * sizeof(double)), sizeof(double));
        if (!keep) return ALWAN_E_NOMEM;
        for (i = 0; i < src_count; i++) {
            char const *px = (char const *)src + i * src_stride;
            for (c = 0; c < channels; c++) {
                keep[i * channels + c] = is_f32 ? (double)((alwan_f32 const *)px)[c] : ((alwan_f64 const *)px)[c];
            }
        }
    }
    if (method == ALWAN_COLOR_TRANSFER_REINHARD2001) {
        st = alwan_ct_run(out, out_stride, src, src_stride, src_count, ref, ref_stride, ref_count, is_f32);
    } else {
        st = alwan__hm_run(out, out_stride, src, src_stride, src_count, ref, ref_stride, ref_count, channels, is_f32);
    }
    if (st == ALWAN_OK && keep) {
        for (i = 0; i < src_count; i++) {
            char *px = (char *)out + i * out_stride;
            for (c = 0; c < channels; c++) {
                double const s0 = keep[i * channels + c];
                double const t = is_f32 ? (double)((alwan_f32 *)px)[c] : ((alwan_f64 *)px)[c];
                double const v = s0 + amount * (t - s0);
                if (is_f32) ((alwan_f32 *)px)[c] = (alwan_f32)v;
                else ((alwan_f64 *)px)[c] = v;
            }
        }
    }
    if (keep) ALWAN_FREE(keep);
    return st;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_color_transfer_f64(alwan_f64 *out, size_t out_stride, alwan_f64 const *src, size_t src_stride,
                                      size_t src_count, alwan_f64 const *ref, size_t ref_stride, size_t ref_count,
                                      size_t channels, alwan_color_transfer_method method,
                                      alwan_color_transfer_params const *params) {
    return alwan_ctf_run(out, out_stride, src, src_stride, src_count, ref, ref_stride, ref_count, channels, method,
                         params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_color_transfer_f32(alwan_f32 *out, size_t out_stride, alwan_f32 const *src, size_t src_stride,
                                      size_t src_count, alwan_f32 const *ref, size_t ref_stride, size_t ref_count,
                                      size_t channels, alwan_color_transfer_method method,
                                      alwan_color_transfer_params const *params) {
    return alwan_ctf_run(out, out_stride, src, src_stride, src_count, ref, ref_stride, ref_count, channels, method,
                         params, 1);
}
#endif
