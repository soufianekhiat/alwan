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
 *
 * MKL: Pitie and Kokaram, "The linear Monge-Kantorovitch linear colour mapping for
 * example-based colour transfer", IET CVMP 2007. The source's colours are moved by the
 * affine map that carries a Gaussian with the source's mean and covariance onto one with
 * the reference's while moving colours least on average (the Monge-Kantorovich optimum
 * between Gaussians):
 *
 *   T = Sr^-1/2 (Sr^1/2 Sz Sr^1/2)^1/2 Sr^-1/2,   out = T (x - mean_src) + mean_ref
 *
 * with Sr, Sz the sample covariances (divided by n - 1). The square roots come from a
 * cyclic Jacobi eigen decomposition; as in the authors' MATLAB code and color-matcher's
 * port, negative eigenvalues are zeroed and the inverse square root is 1 / (sqrt(l) +
 * DBL_EPSILON). T is unique for a source whose covariance is not singular. Where it is (a
 * grey image, a flat channel) the authors' code divides by DBL_EPSILON and the result
 * carries rounding noise amplified by 1e31; this takes the pseudo-inverse instead, an
 * eigenvalue under 1e-12 of the largest counting as zero, so the source is moved only
 * within the directions it spans.
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
        lg[i] = ALWAN_LOG10_F64(lms);
    }
    lab[0] = (lg[0] + lg[1] + lg[2]) / ALWAN_SQRT_F64(3.0);
    lab[1] = (lg[0] + lg[1] - 2.0 * lg[2]) / ALWAN_SQRT_F64(6.0);
    lab[2] = (lg[0] - lg[1]) / ALWAN_SQRT_F64(2.0);
}

static void alwan_ct_from_lab(double rgb[3], double const lab[3], double const minv[9]) {
    double const a = lab[0] * ALWAN_SQRT_F64(3.0) / 3.0, b = lab[1] * ALWAN_SQRT_F64(6.0) / 6.0, c = lab[2] * ALWAN_SQRT_F64(2.0) / 2.0;
    double lms[3];
    int i;
    lms[0] = ALWAN_POW_F64(10.0, a + b + c);
    lms[1] = ALWAN_POW_F64(10.0, a + b - c);
    lms[2] = ALWAN_POW_F64(10.0, a - 2.0 * b);
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
    for (c = 0; c < 3; c++) sd[c] = ALWAN_SQRT_F64(q[c] / (double)count);
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

/* ---- MKL ---- */

/* Eigen decomposition of a symmetric n x n matrix (n <= 4) by cyclic Jacobi: a becomes
 * diagonal (the eigenvalues), v the eigenvectors in its columns. */
static void alwan_mkl_jacobi(double a[16], double v[16], size_t n) {
    size_t i, j, k, sweep;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) v[i * n + j] = i == j ? 1.0 : 0.0;
    }
    for (sweep = 0; sweep < 100; sweep++) {
        double off = 0.0, diag = 0.0;
        for (i = 0; i < n; i++) {
            diag += a[i * n + i] * a[i * n + i];
            for (j = i + 1; j < n; j++) off += a[i * n + j] * a[i * n + j];
        }
        if (off <= 1e-30 * diag || off == 0.0) break;
        for (i = 0; i < n; i++) {
            for (j = i + 1; j < n; j++) {
                double const apq = a[i * n + j];
                double theta, t, c, s;
                if (apq == 0.0) continue;
                theta = (a[j * n + j] - a[i * n + i]) / (2.0 * apq);
                t = (theta >= 0.0 ? 1.0 : -1.0) / (ALWAN_ABS_F64(theta) + ALWAN_SQRT_F64(theta * theta + 1.0));
                c = 1.0 / ALWAN_SQRT_F64(t * t + 1.0);
                s = t * c;
                for (k = 0; k < n; k++) { /* columns i and j */
                    double const aki = a[k * n + i], akj = a[k * n + j];
                    a[k * n + i] = c * aki - s * akj;
                    a[k * n + j] = s * aki + c * akj;
                }
                for (k = 0; k < n; k++) { /* rows i and j */
                    double const aik = a[i * n + k], ajk = a[j * n + k];
                    a[i * n + k] = c * aik - s * ajk;
                    a[j * n + k] = s * aik + c * ajk;
                }
                for (k = 0; k < n; k++) {
                    double const vki = v[k * n + i], vkj = v[k * n + j];
                    v[k * n + i] = c * vki - s * vkj;
                    v[k * n + j] = s * vki + c * vkj;
                }
            }
        }
    }
}

/* Mean and sample covariance (n - 1) of count pixels of n channels. */
static int alwan_mkl_stats(double mean[4], double cov[16], void const *px, size_t stride, size_t count, size_t n,
                           int is_f32) {
    size_t i, a, b;
    for (a = 0; a < n; a++) mean[a] = 0.0;
    for (a = 0; a < n * n; a++) cov[a] = 0.0;
    for (i = 0; i < count; i++) {
        char const *p = (char const *)px + i * stride;
        for (a = 0; a < n; a++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)p)[a] : ((alwan_f64 const *)p)[a];
            if (!alwan_ct_finite(v)) return 0;
            mean[a] += v;
        }
    }
    for (a = 0; a < n; a++) mean[a] /= (double)count;
    for (i = 0; i < count; i++) {
        char const *p = (char const *)px + i * stride;
        double d[4];
        for (a = 0; a < n; a++) d[a] = (is_f32 ? (double)((alwan_f32 const *)p)[a] : ((alwan_f64 const *)p)[a]) - mean[a];
        for (a = 0; a < n; a++) {
            for (b = a; b < n; b++) cov[a * n + b] += d[a] * d[b];
        }
    }
    for (a = 0; a < n; a++) {
        for (b = a; b < n; b++) {
            cov[a * n + b] /= (double)(count - 1);
            cov[b * n + a] = cov[a * n + b];
        }
    }
    return 1;
}

static alwan_status alwan_mkl_run(void *out, size_t out_stride, void const *src, size_t src_stride, size_t src_count,
                                  void const *ref, size_t ref_stride, size_t ref_count, size_t n, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    double ms[4], mz[4], sr[16], sz[16], vr[16], vc[16], c[16], tmp[16], t[16];
    double val_r[4], inv_r[4], big = 0.0;
    size_t i, a, b, k;
    if (src_count < 2 || ref_count < 2) return ALWAN_E_INVALID;
    if (ref_stride / elem < n) return ALWAN_E_INVALID;
    if (!alwan_mkl_stats(ms, sr, src, src_stride, src_count, n, is_f32)) return ALWAN_E_INVALID;
    if (!alwan_mkl_stats(mz, sz, ref, ref_stride, ref_count, n, is_f32)) return ALWAN_E_INVALID;
    alwan_mkl_jacobi(sr, vr, n);
    for (a = 0; a < n; a++) {
        double const l = sr[a * n + a];
        if (l > big) big = l;
    }
    for (a = 0; a < n; a++) {
        double const l = sr[a * n + a] > 0.0 ? sr[a * n + a] : 0.0;
        val_r[a] = ALWAN_SQRT_F64(l);
        inv_r[a] = l > 1e-12 * big ? 1.0 / (val_r[a] + DBL_EPSILON) : 0.0;
    }
    /* C = D Vr' Sz Vr D, D = diag(sqrt l) */
    for (a = 0; a < n; a++) {
        for (b = 0; b < n; b++) {
            double s = 0.0;
            for (k = 0; k < n; k++) s += sz[a * n + k] * vr[k * n + b];
            tmp[a * n + b] = s; /* Sz Vr */
        }
    }
    for (a = 0; a < n; a++) {
        for (b = 0; b < n; b++) {
            double s = 0.0;
            for (k = 0; k < n; k++) s += vr[k * n + a] * tmp[k * n + b];
            c[a * n + b] = val_r[a] * s * val_r[b];
        }
    }
    alwan_mkl_jacobi(c, vc, n);
    /* C^1/2 = Vc sqrt(L) Vc', then T = Vr Dinv C^1/2 Dinv Vr' */
    for (a = 0; a < n; a++) {
        for (b = 0; b < n; b++) {
            double s = 0.0;
            for (k = 0; k < n; k++) {
                double const l = c[k * n + k] > 0.0 ? c[k * n + k] : 0.0;
                s += vc[a * n + k] * ALWAN_SQRT_F64(l) * vc[b * n + k];
            }
            tmp[a * n + b] = inv_r[a] * s * inv_r[b];
        }
    }
    for (a = 0; a < n; a++) {
        for (b = 0; b < n; b++) {
            double s = 0.0;
            for (k = 0; k < n; k++) s += vr[a * n + k] * tmp[k * n + b];
            c[a * n + b] = s; /* Vr (Dinv C^1/2 Dinv) */
        }
    }
    for (a = 0; a < n; a++) {
        for (b = 0; b < n; b++) {
            double s = 0.0;
            for (k = 0; k < n; k++) s += c[a * n + k] * vr[b * n + k];
            t[a * n + b] = s;
        }
    }
    for (i = 0; i < src_count; i++) {
        char const *p = (char const *)src + i * src_stride;
        char *d = (char *)out + i * out_stride;
        double x[4], y[4];
        for (a = 0; a < n; a++) x[a] = (is_f32 ? (double)((alwan_f32 const *)p)[a] : ((alwan_f64 const *)p)[a]) - ms[a];
        for (a = 0; a < n; a++) {
            double s = mz[a];
            for (b = 0; b < n; b++) s += t[a * n + b] * x[b];
            y[a] = s;
        }
        for (a = 0; a < n; a++) {
            if (is_f32) ((alwan_f32 *)d)[a] = (alwan_f32)y[a];
            else ((alwan_f64 *)d)[a] = y[a];
        }
    }
    return ALWAN_OK;
}

/* ---- IDT: Pitie, Kokaram and Dahyot, iterative distribution transfer ----
 *
 * F. Pitie, A. Kokaram, R. Dahyot, "N-dimensional probability density function transfer
 * and its application to colour transfer", ICCV 2005, and "Automated colour grading using
 * colour distribution transfer", CVIU 107(1-2), 2007. alwan's own code from the papers.
 *
 * Each iteration takes a rotation R (rows are the axes), projects the current source and
 * the reference on every axis, matches the source's 1D distribution to the reference's on
 * that axis, and moves each pixel by R' (matched - projected). The 1D transfer:
 *
 *   lo, hi        the smallest and largest projection of either image (an axis on which
 *                 hi <= lo moves nothing)
 *   e_k           lo + k step, step = (hi - lo) / (bins - 1), k = 0 .. bins - 1
 *   bin of v      the largest k with e_k <= v (a binary search over e: no float-to-index
 *                 cast), at most bins - 1
 *   C_k           the running sum of (count_k + 1e-6), divided by its last value: the eps
 *                 keeps C strictly increasing, so its inverse exists everywhere
 *   F_k           lo + f step, f where C_ref reaches C_src[k] (linear between the ref's
 *                 bins; 0 at or below C_ref[0], bins - 1 at or above C_ref[last])
 *   v'            F_k + ((v - e_k) / step) (F_{k+1} - F_k), k the bin of v at most bins - 2
 *
 * The rotations: iteration 0 is the identity; iteration i > 0 draws n x n values uniform in
 * [-1, 1) from splitmix64 (state = seed, value (x >> 11) * 2^-53 * 2 - 1, row major) and
 * orthonormalises the rows by modified Gram-Schmidt, redrawing a row whose norm falls under
 * 1e-6. Every sum runs in index order, so a numpy transcription reproduces it to the bit
 * (suite 299). */

static uint64_t alwan_idt_next(uint64_t *state) {
    uint64_t z;
    *state += 0x9E3779B97F4A7C15ull;
    z = *state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static double alwan_idt_uniform(uint64_t *state) {
    return (double)(alwan_idt_next(state) >> 11) * (1.0 / 9007199254740992.0) * 2.0 - 1.0;
}

static void alwan_idt_rotation(double r[16], size_t n, size_t it, uint64_t *state) {
    size_t a, b, k;
    if (it == 0) {
        for (a = 0; a < n; a++) {
            for (b = 0; b < n; b++) r[a * n + b] = a == b ? 1.0 : 0.0;
        }
        return;
    }
    for (a = 0; a < n * n; a++) r[a] = alwan_idt_uniform(state);
    for (a = 0; a < n; a++) {
        for (;;) {
            double norm = 0.0;
            for (b = 0; b < a; b++) {
                double dot = 0.0;
                for (k = 0; k < n; k++) dot += r[a * n + k] * r[b * n + k];
                for (k = 0; k < n; k++) r[a * n + k] -= dot * r[b * n + k];
            }
            for (k = 0; k < n; k++) norm += r[a * n + k] * r[a * n + k];
            norm = ALWAN_SQRT_F64(norm);
            if (norm >= 1e-6) {
                for (k = 0; k < n; k++) r[a * n + k] /= norm;
                break;
            }
            for (k = 0; k < n; k++) r[a * n + k] = alwan_idt_uniform(state);
        }
    }
}

/* The largest k in [0, count) with e[k] <= v; 0 when v < e[0] (never, here). */
static size_t alwan_idt_find(double const *e, size_t count, double v) {
    size_t lo = 0, hi = count;   /* invariant: answer in [lo, hi) once e[0] <= v */
    while (hi - lo > 1) {
        size_t const mid = lo + (hi - lo) / 2;
        if (e[mid] <= v) lo = mid;
        else hi = mid;
    }
    return lo;
}

/* One axis: d[i] = matched - p[i] for the count source projections p against the ref's q. */
static void alwan_idt_axis(double *d, double const *p, size_t count, double const *q, size_t rcount, size_t bins,
                           double *e, double *cx, double *cy, double *f) {
    double lo = p[0], hi = p[0], step, s;
    size_t i, k;
    for (i = 0; i < count; i++) {
        if (p[i] < lo) lo = p[i];
        if (p[i] > hi) hi = p[i];
    }
    for (i = 0; i < rcount; i++) {
        if (q[i] < lo) lo = q[i];
        if (q[i] > hi) hi = q[i];
    }
    if (!(hi > lo)) {
        for (i = 0; i < count; i++) d[i] = 0.0;
        return;
    }
    step = (hi - lo) / (double)(bins - 1);
    for (k = 0; k < bins; k++) {
        e[k] = lo + (double)k * step;
        cx[k] = 0.0;
        cy[k] = 0.0;
    }
    for (i = 0; i < count; i++) cx[alwan_idt_find(e, bins, p[i])] += 1.0;
    for (i = 0; i < rcount; i++) cy[alwan_idt_find(e, bins, q[i])] += 1.0;
    s = 0.0;
    for (k = 0; k < bins; k++) { s += cx[k] + 1e-6; cx[k] = s; }
    for (k = 0; k < bins; k++) cx[k] /= s;
    s = 0.0;
    for (k = 0; k < bins; k++) { s += cy[k] + 1e-6; cy[k] = s; }
    for (k = 0; k < bins; k++) cy[k] /= s;
    for (k = 0; k < bins; k++) {
        double const t = cx[k];
        double pos;
        if (t <= cy[0]) {
            pos = 0.0;
        } else if (t >= cy[bins - 1]) {
            pos = (double)(bins - 1);
        } else {
            size_t const j = alwan_idt_find(cy, bins, t);
            pos = (double)j + (t - cy[j]) / (cy[j + 1] - cy[j]);
        }
        f[k] = lo + pos * step;
    }
    for (i = 0; i < count; i++) {
        size_t kk = alwan_idt_find(e, bins, p[i]);
        double frac;
        if (kk > bins - 2) kk = bins - 2;
        frac = (p[i] - e[kk]) / step;
        d[i] = (f[kk] + frac * (f[kk + 1] - f[kk])) - p[i];
    }
}

/* x (count x n, row major) and y (rcount x n) in double; x is moved in place. */
static alwan_status alwan_idt_core(double *x, size_t count, double const *y, size_t rcount, size_t n,
                                   size_t iterations, size_t bins, uint64_t seed) {
    double r[16];
    double *p, *q, *d, *e, *cx, *cy, *f;
    uint64_t state = seed;
    size_t it, a, i, k;
    size_t const nmax = count > rcount ? count : rcount;
    p = (double *)ALWAN_ALLOC(alwan_safe_array_size(nmax, sizeof(double)), sizeof(double));
    q = (double *)ALWAN_ALLOC(alwan_safe_array_size(rcount, sizeof(double)), sizeof(double));
    d = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, n * sizeof(double)), sizeof(double));
    e = (double *)ALWAN_ALLOC(alwan_safe_array_size(bins, 4 * sizeof(double)), sizeof(double));
    if (!p || !q || !d || !e) {
        if (p) ALWAN_FREE(p);
        if (q) ALWAN_FREE(q);
        if (d) ALWAN_FREE(d);
        if (e) ALWAN_FREE(e);
        return ALWAN_E_NOMEM;
    }
    cx = e + bins;
    cy = cx + bins;
    f = cy + bins;
    for (it = 0; it < iterations; it++) {
        alwan_idt_rotation(r, n, it, &state);
        for (a = 0; a < n; a++) {
            for (i = 0; i < count; i++) {
                double s = 0.0;
                for (k = 0; k < n; k++) s += r[a * n + k] * x[i * n + k];
                p[i] = s;
            }
            for (i = 0; i < rcount; i++) {
                double s = 0.0;
                for (k = 0; k < n; k++) s += r[a * n + k] * y[i * n + k];
                q[i] = s;
            }
            alwan_idt_axis(p, p, count, q, rcount, bins, e, cx, cy, f); /* p becomes the delta */
            for (i = 0; i < count; i++) d[i * n + a] = p[i];
        }
        for (i = 0; i < count; i++) {
            for (k = 0; k < n; k++) {
                double t = 0.0;
                for (a = 0; a < n; a++) t += r[a * n + k] * d[i * n + a];
                x[i * n + k] += t;
            }
        }
    }
    ALWAN_FREE(p);
    ALWAN_FREE(q);
    ALWAN_FREE(d);
    ALWAN_FREE(e);
    return ALWAN_OK;
}

/* Pixels to a packed double buffer, refusing a non-finite value. */
static double *alwan_ct_pack(void const *px, size_t stride, size_t count, size_t n, int is_f32, int *bad) {
    double *b = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, n * sizeof(double)), sizeof(double));
    size_t i, c;
    *bad = 0;
    if (!b) return NULL;
    for (i = 0; i < count; i++) {
        char const *p = (char const *)px + i * stride;
        for (c = 0; c < n; c++) {
            double const v = is_f32 ? (double)((alwan_f32 const *)p)[c] : ((alwan_f64 const *)p)[c];
            if (!alwan_ct_finite(v)) *bad = 1;
            b[i * n + c] = v;
        }
    }
    return b;
}

static void alwan_ct_unpack(void *out, size_t stride, double const *b, size_t count, size_t n, int is_f32) {
    size_t i, c;
    for (i = 0; i < count; i++) {
        char *p = (char *)out + i * stride;
        for (c = 0; c < n; c++) {
            if (is_f32) ((alwan_f32 *)p)[c] = (alwan_f32)b[i * n + c];
            else ((alwan_f64 *)p)[c] = b[i * n + c];
        }
    }
}

static alwan_status alwan_idt_run(void *out, size_t out_stride, void const *src, size_t src_stride, size_t src_count,
                                  void const *ref, size_t ref_stride, size_t ref_count, size_t n, size_t iterations,
                                  size_t bins, uint64_t seed, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    double *x, *y;
    int bad_x, bad_y;
    alwan_status st;
    if (ref_count == 0 || ref_stride / elem < n) return ALWAN_E_INVALID;
    x = alwan_ct_pack(src, src_stride, src_count, n, is_f32, &bad_x);
    y = alwan_ct_pack(ref, ref_stride, ref_count, n, is_f32, &bad_y);
    if (!x || !y) {
        if (x) ALWAN_FREE(x);
        if (y) ALWAN_FREE(y);
        return ALWAN_E_NOMEM;
    }
    if (bad_x || bad_y) {
        st = ALWAN_E_INVALID;
    } else {
        st = alwan_idt_core(x, src_count, y, ref_count, n, iterations, bins, seed);
        if (st == ALWAN_OK) alwan_ct_unpack(out, out_stride, x, src_count, n, is_f32);
    }
    ALWAN_FREE(x);
    ALWAN_FREE(y);
    return st;
}

/* ---- Xiao and Ma 2006 ----
 *
 * X. Xiao, L. Ma, "Color transfer in correlated color space", VRCIA 2006. The source's
 * colour distribution, seen as an ellipsoid (mean and covariance), is translated, rotated
 * and scaled onto the reference's in RGB itself, with no decorrelating space:
 *
 *   out = m_r + U_r S_r S_s^-1 U_s' (x - m_s),   cov = U L U', S = sqrt(L)
 *
 * The paper takes U and L from an SVD, which leaves each eigenvector's sign and the order
 * of equal eigenvalues open; here the eigenvalues are sorted largest first and each
 * eigenvector is signed so its component of largest magnitude is positive (the first such
 * component on a tie). Sample covariances (n - 1); a source eigenvalue under 1e-12 of the
 * largest gives a scale of 0, as MKL's pseudo-inverse. */

static void alwan_xiao_sorted(double l[4], double u[16], double a[16], size_t n) {
    double v[16];
    size_t idx[4], i, j, k;
    alwan_mkl_jacobi(a, v, n);
    for (i = 0; i < n; i++) idx[i] = i;
    for (i = 1; i < n; i++) { /* insertion sort, largest first; stable */
        size_t const t = idx[i];
        j = i;
        while (j > 0 && a[idx[j - 1] * n + idx[j - 1]] < a[t * n + t]) {
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = t;
    }
    for (j = 0; j < n; j++) {
        size_t const c = idx[j];
        size_t big = 0;
        double sign;
        l[j] = a[c * n + c];
        for (k = 1; k < n; k++) {
            if (ALWAN_ABS_F64(v[k * n + c]) > ALWAN_ABS_F64(v[big * n + c])) big = k;
        }
        sign = v[big * n + c] < 0.0 ? -1.0 : 1.0;
        for (k = 0; k < n; k++) u[k * n + j] = sign * v[k * n + c];
    }
}

static alwan_status alwan_xiao_run(void *out, size_t out_stride, void const *src, size_t src_stride, size_t src_count,
                                   void const *ref, size_t ref_stride, size_t ref_count, size_t n, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    double ms[4], mr[4], cs[16], cr[16], us[16], ur[16], ls[4], lr[4], scale[4], m[16], big = 0.0;
    size_t i, a, b, k;
    if (src_count < 2 || ref_count < 2) return ALWAN_E_INVALID;
    if (ref_stride / elem < n) return ALWAN_E_INVALID;
    if (!alwan_mkl_stats(ms, cs, src, src_stride, src_count, n, is_f32)) return ALWAN_E_INVALID;
    if (!alwan_mkl_stats(mr, cr, ref, ref_stride, ref_count, n, is_f32)) return ALWAN_E_INVALID;
    alwan_xiao_sorted(ls, us, cs, n);
    alwan_xiao_sorted(lr, ur, cr, n);
    for (a = 0; a < n; a++) {
        if (ls[a] > big) big = ls[a];
    }
    for (a = 0; a < n; a++) {
        double const s = ls[a] > 1e-12 * big && ls[a] > 0.0 ? ALWAN_SQRT_F64(ls[a]) : 0.0;
        double const r = lr[a] > 0.0 ? ALWAN_SQRT_F64(lr[a]) : 0.0;
        scale[a] = s > 0.0 ? r / s : 0.0;
    }
    /* M = U_r diag(scale) U_s' */
    for (a = 0; a < n; a++) {
        for (b = 0; b < n; b++) {
            double s = 0.0;
            for (k = 0; k < n; k++) s += ur[a * n + k] * scale[k] * us[b * n + k];
            m[a * n + b] = s;
        }
    }
    for (i = 0; i < src_count; i++) {
        char const *p = (char const *)src + i * src_stride;
        char *dst = (char *)out + i * out_stride;
        double x[4], y[4];
        for (a = 0; a < n; a++) x[a] = (is_f32 ? (double)((alwan_f32 const *)p)[a] : ((alwan_f64 const *)p)[a]) - ms[a];
        for (a = 0; a < n; a++) {
            double s = mr[a];
            for (b = 0; b < n; b++) s += m[a * n + b] * x[b];
            y[a] = s;
        }
        for (a = 0; a < n; a++) {
            if (is_f32) ((alwan_f32 *)dst)[a] = (alwan_f32)y[a];
            else ((alwan_f64 *)dst)[a] = y[a];
        }
    }
    return ALWAN_OK;
}

/* ---- regrain: Pitie, Kokaram and Dahyot 2007 ----
 *
 * The transfer t can stretch the source I's small variations into grain. The regrain
 * looks for J close to t where the source has detail and with the source's gradients where
 * it is flat, minimising sum psi |J - t|^2 + phi |grad J - grad I|^2, by Jacobi sweeps of
 * its Euler-Lagrange equation from J = t:
 *
 *   J'(p) = (psi_p t(p) + sum_q phi_pq (J(q) - I(q) + I(p))) / (psi_p + sum_q phi_pq)
 *
 * over the four neighbours q (x-1, x+1, y-1, y+1, in that order; a neighbour past the edge
 * is p itself), phi_pq = (phi_p + phi_q) / 2. With g the source's gradient magnitude at p
 * (central differences over every channel, clamped at the edge, the sum of squares taken
 * channel by channel), psi_p = min(1, 51.2 g) and phi_p = 30 / (1 + 10 g / smoothness):
 * the weights Pitie et al. give for images in [0, 1]. One scale only. */
static alwan_status alwan_ct_regrain(double *j, double const *src0, size_t w, size_t h, size_t n, size_t sweeps,
                                     double smooth) {
    size_t const count = w * h;
    double *t = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, n * sizeof(double)), sizeof(double));
    double *nj = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, n * sizeof(double)), sizeof(double));
    double *psi = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, 2 * sizeof(double)), sizeof(double));
    double *phi;
    size_t x, y, c, it;
    if (!t || !nj || !psi) {
        if (t) ALWAN_FREE(t);
        if (nj) ALWAN_FREE(nj);
        if (psi) ALWAN_FREE(psi);
        return ALWAN_E_NOMEM;
    }
    phi = psi + count;
    for (x = 0; x < count * n; x++) t[x] = j[x];
    for (y = 0; y < h; y++) {
        size_t const ym = y > 0 ? y - 1 : 0, yp = y + 1 < h ? y + 1 : h - 1;
        for (x = 0; x < w; x++) {
            size_t const xm = x > 0 ? x - 1 : 0, xp = x + 1 < w ? x + 1 : w - 1;
            double g = 0.0;
            for (c = 0; c < n; c++) {
                double const gx = (src0[(y * w + xp) * n + c] - src0[(y * w + xm) * n + c]) / 2.0;
                double const gy = (src0[(yp * w + x) * n + c] - src0[(ym * w + x) * n + c]) / 2.0;
                g += gx * gx + gy * gy;
            }
            g = ALWAN_SQRT_F64(g);
            psi[y * w + x] = 51.2 * g < 1.0 ? 51.2 * g : 1.0;
            phi[y * w + x] = 30.0 / (1.0 + 10.0 * g / smooth);
        }
    }
    for (it = 0; it < sweeps; it++) {
        for (y = 0; y < h; y++) {
            size_t const ym = y > 0 ? y - 1 : 0, yp = y + 1 < h ? y + 1 : h - 1;
            for (x = 0; x < w; x++) {
                size_t const xm = x > 0 ? x - 1 : 0, xp = x + 1 < w ? x + 1 : w - 1;
                size_t const pi = y * w + x;
                size_t const nb[4] = { y * w + xm, y * w + xp, ym * w + x, yp * w + x };
                double ph[4], den;
                size_t q;
                for (q = 0; q < 4; q++) ph[q] = (phi[pi] + phi[nb[q]]) / 2.0;
                den = psi[pi] + ph[0] + ph[1] + ph[2] + ph[3];
                for (c = 0; c < n; c++) {
                    double num = psi[pi] * t[pi * n + c];
                    for (q = 0; q < 4; q++) num += ph[q] * (j[nb[q] * n + c] - src0[nb[q] * n + c] + src0[pi * n + c]);
                    nj[pi * n + c] = num / den;
                }
            }
        }
        for (x = 0; x < count * n; x++) j[x] = nj[x];
    }
    ALWAN_FREE(t);
    ALWAN_FREE(nj);
    ALWAN_FREE(psi);
    return ALWAN_OK;
}

/* ---- the family: alwan_color_transfer_{T} ---- */

static alwan_status alwan_ctf_run(void *out, size_t out_stride, void const *src, size_t src_stride, size_t src_count,
                                  void const *ref, size_t ref_stride, size_t ref_count, size_t channels,
                                  alwan_color_transfer_method method, alwan_color_transfer_params const *params,
                                  int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    double const amount = params && params->amount != 0.0 ? params->amount : 1.0;
    size_t const iterations = params && params->iterations ? params->iterations : 20;
    size_t const bins = params && params->bins ? params->bins : 300;
    uint64_t const seed = params ? params->seed : 0;
    size_t const sweeps = params ? params->regrain_iterations : 0;
    double const smooth = params && params->regrain_smoothness != 0.0 ? params->regrain_smoothness : 1.0;
    size_t const width = params ? params->width : 0;
    double *keep = NULL;
    alwan_status st;
    size_t i, c;
    if (!out || !src || !ref || src_count == 0 || channels == 0 || channels > 4) return ALWAN_E_INVALID;
    if (!(amount == amount) || amount < 0.0 || amount > 1.0) return ALWAN_E_RANGE;
    if (method == ALWAN_COLOR_TRANSFER_REINHARD2001 && channels != 3) return ALWAN_E_INVALID;
    if (method != ALWAN_COLOR_TRANSFER_REINHARD2001 && method != ALWAN_COLOR_TRANSFER_HISTOGRAM_MATCH &&
        method != ALWAN_COLOR_TRANSFER_MKL && method != ALWAN_COLOR_TRANSFER_IDT &&
        method != ALWAN_COLOR_TRANSFER_XIAO2006) {
        return ALWAN_E_INVALID;
    }
    if (bins < 2) return ALWAN_E_INVALID;
    if (iterations > 10000 || sweeps > 10000) return ALWAN_E_RANGE;
    if (!(smooth > 0.0) || !alwan_ct_finite(smooth)) return ALWAN_E_INVALID;
    if (sweeps > 0 && (width == 0 || src_count % width != 0)) return ALWAN_E_INVALID;
    if (src_stride / elem < channels || out_stride / elem < channels) return ALWAN_E_INVALID;
    if (amount != 1.0 || sweeps > 0) { /* out may be src: keep the source for the blend and the regrain */
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
    } else if (method == ALWAN_COLOR_TRANSFER_MKL) {
        st = alwan_mkl_run(out, out_stride, src, src_stride, src_count, ref, ref_stride, ref_count, channels, is_f32);
    } else if (method == ALWAN_COLOR_TRANSFER_IDT) {
        st = alwan_idt_run(out, out_stride, src, src_stride, src_count, ref, ref_stride, ref_count, channels,
                           iterations, bins, seed, is_f32);
    } else if (method == ALWAN_COLOR_TRANSFER_XIAO2006) {
        st = alwan_xiao_run(out, out_stride, src, src_stride, src_count, ref, ref_stride, ref_count, channels, is_f32);
    } else {
        st = alwan__hm_run(out, out_stride, src, src_stride, src_count, ref, ref_stride, ref_count, channels, is_f32);
    }
    if (st == ALWAN_OK && sweeps > 0) {
        int bad;
        double *jbuf = alwan_ct_pack(out, out_stride, src_count, channels, is_f32, &bad);
        if (!jbuf) {
            st = ALWAN_E_NOMEM;
        } else {
            st = alwan_ct_regrain(jbuf, keep, width, src_count / width, channels, sweeps, smooth);
            if (st == ALWAN_OK) alwan_ct_unpack(out, out_stride, jbuf, src_count, channels, is_f32);
            ALWAN_FREE(jbuf);
        }
    }
    if (amount == 1.0 && keep) {   /* kept only for the regrain */
        ALWAN_FREE(keep);
        keep = NULL;
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
