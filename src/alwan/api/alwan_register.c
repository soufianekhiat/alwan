/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Registration: the translation that lines one image up with another.
 *
 * ALWAN_REGISTER_PHASE_CORRELATION is scikit-image 0.26's phase_cross_correlation (space
 * 'real', no masks, disambiguate off), which is Guizar-Sicairos, Thurman and Fienup, "Efficient
 * subpixel image registration algorithms", Opt. Lett. 33(2), 2008:
 *
 *   the cross-power spectrum P = F(reference) conj(F(moving)), each term divided by its
 *   modulus (at least 100 epsilon) unless normalization is NONE; its inverse transform, scaled
 *   by 1/N; the first sample of largest modulus, in row-major order, as the whole-pixel shift,
 *   wrapped to (-n/2, n/2]. With an upsampling factor u above 1, the shift is rounded to 1/u
 *   (half to even, as numpy rounds), and the cross-correlation is evaluated again on a
 *   ceil(1.5 u) square of points 1/u apart about it by a matrix-multiply DFT of conj(P); its
 *   largest sample moves the shift by a fraction of a pixel.
 *
 * error = sqrt(|1 - |CC_max|^2 / (A_ref A_mov)|), with A the sum of |F|^2, divided by N when
 * u is 1 (scikit-image's two branches normalise differently and alwan keeps both); phasediff
 * is the angle of CC_max. The full-size transforms are alwan__fft's (radix 2 or Bluestein),
 * computed in double whatever the pixel type.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

static double const ALWAN_RG_PI = 3.14159265358979323846;

/* fftfreq(n, d)[j]: j / (n d) for j < ceil(n / 2), (j - n) / (n d) after, as numpy. */
static double alwan_rg_fftfreq(size_t j, size_t n, double d) {
    size_t const half = (n + 1) / 2;
    double const k = j < half ? (double)j : (double)j - (double)n;
    return k / ((double)n * d);
}

/* One 2-D transform of re/im (H x W, row-major) in place, rows then columns. */
static void alwan_rg_fft2(double *re, double *im, size_t W, size_t H, alwan__fft const *fw, alwan__fft const *fh,
                          double *cre, double *cim, int inverse) {
    size_t x, y;
    for (y = 0; y < H; y++) alwan__fft_run(fw, re + y * W, im + y * W, inverse);
    for (x = 0; x < W; x++) {
        for (y = 0; y < H; y++) cre[y] = re[y * W + x], cim[y] = im[y * W + x];
        alwan__fft_run(fh, cre, cim, inverse);
        for (y = 0; y < H; y++) re[y * W + x] = cre[y], im[y * W + x] = cim[y];
    }
}

static double alwan_rg_read(void const *p, size_t row_stride, size_t x, size_t y, int kind) {
    char const *row = (char const *)p + y * row_stride;
    if (kind == 0) return ((double const *)row)[x];
    if (kind == 1) return (double)((float const *)row)[x];
    return (double)((unsigned char const *)row)[x];
}

static alwan_status alwan_rg_run(alwan_register_result *out, void const *ref, size_t ref_rs, void const *mov, size_t mov_rs,
                                 size_t W, size_t H, alwan_register_method method, alwan_register_params const *params, int kind) {
    size_t const elem = kind == 0 ? sizeof(double) : kind == 1 ? sizeof(float) : 1;
    size_t const N = W * H;
    size_t u = 1;
    int phase = 1;
    double *buf = NULL, *sr, *si, *tr, *ti, *cre, *cim;
    alwan__fft *fw = NULL, *fh = NULL;
    alwan_status st = ALWAN_OK;
    double src_amp = 0.0, tgt_amp = 0.0, cc_re, cc_im, shift[2];
    size_t x, y, best = 0;

    if (!out || !ref || !mov || W == 0 || H == 0) return ALWAN_E_INVALID;
    if (method != ALWAN_REGISTER_PHASE_CORRELATION) return ALWAN_E_INVALID;
    if (ref_rs < W * elem || mov_rs < W * elem) return ALWAN_E_INVALID;
    if (params) {
        if (params->normalization != ALWAN_REGISTER_NORMALIZE_PHASE && params->normalization != ALWAN_REGISTER_NORMALIZE_NONE) {
            return ALWAN_E_INVALID;
        }
        phase = params->normalization == ALWAN_REGISTER_NORMALIZE_PHASE;
        if (params->upsample_factor > 0) u = params->upsample_factor;
    }
    if (N / W != H) return ALWAN_E_RANGE;

    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(4 * N + 2 * H, sizeof(double)), 64);
    fw = alwan__fft_create(W);
    fh = alwan__fft_create(H);
    if (!buf || !fw || !fh) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    sr = buf;
    si = sr + N;
    tr = si + N;
    ti = tr + N;
    cre = ti + N;
    cim = cre + H;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            sr[y * W + x] = alwan_rg_read(ref, ref_rs, x, y, kind);
            tr[y * W + x] = alwan_rg_read(mov, mov_rs, x, y, kind);
            si[y * W + x] = ti[y * W + x] = 0.0;
        }
    }
    alwan_rg_fft2(sr, si, W, H, fw, fh, cre, cim, 0);
    alwan_rg_fft2(tr, ti, W, H, fw, fh, cre, cim, 0);
    for (x = 0; x < N; x++) {
        src_amp += sr[x] * sr[x] + si[x] * si[x];
        tgt_amp += tr[x] * tr[x] + ti[x] * ti[x];
    }
    /* the cross-power spectrum, kept in (sr, si); the moving spectrum is not needed after */
    for (x = 0; x < N; x++) {
        double pr = sr[x] * tr[x] + si[x] * ti[x];
        double pi = si[x] * tr[x] - sr[x] * ti[x];
        if (phase) {
            double m = ALWAN_SQRT_F64(pr * pr + pi * pi);
            if (m < 100.0 * DBL_EPSILON) m = 100.0 * DBL_EPSILON;
            pr /= m;
            pi /= m;
        }
        sr[x] = pr;
        si[x] = pi;
    }
    /* the whole-pixel peak of ifft2(P) */
    memcpy(tr, sr, N * sizeof(double));
    memcpy(ti, si, N * sizeof(double));
    alwan_rg_fft2(tr, ti, W, H, fw, fh, cre, cim, 1);
    {
        double bm = -1.0;
        for (x = 0; x < N; x++) {
            double const m = ALWAN_HYPOT_F64(tr[x], ti[x]);
            if (m > bm) bm = m, best = x;
        }
    }
    cc_re = tr[best] / (double)N;
    cc_im = ti[best] / (double)N;
    shift[0] = (double)(best / W);
    shift[1] = (double)(best % W);
    if (shift[0] > ALWAN_TRUNC_F64((double)H / 2.0)) shift[0] -= (double)H;
    if (shift[1] > ALWAN_TRUNC_F64((double)W / 2.0)) shift[1] -= (double)W;

    if (u == 1) {
        src_amp /= (double)N;
        tgt_amp /= (double)N;
    } else {
        double const uf = (double)u;
        double const region_d = ALWAN_CEIL_F64(uf * 1.5);
        size_t const R = (size_t)region_d;
        double const dftshift = ALWAN_TRUNC_F64(region_d / 2.0);
        double off[2];
        double *kr = NULL, *ki = NULL, *mr = NULL, *mi = NULL;
        size_t k, m, r, j;
        double bm = -1.0;
        size_t bk = 0, bmm = 0;
        shift[0] = rint(shift[0] * uf) / uf;
        shift[1] = rint(shift[1] * uf) / uf;
        off[0] = dftshift - shift[0] * uf;
        off[1] = dftshift - shift[1] * uf;
        /* data = conj(P); first along the columns: M[k][r] = sum_j Kc[k][j] data[r][j], R x H */
        kr = (double *)ALWAN_ALLOC(alwan_safe_array_size(2 * R * (W > H ? W : H) + 2 * R * H, sizeof(double)), 64);
        if (!kr) {
            st = ALWAN_E_NOMEM;
            goto done;
        }
        ki = kr + R * (W > H ? W : H);
        mr = ki + R * (W > H ? W : H);
        mi = mr + R * H;
        for (k = 0; k < R; k++) {
            for (j = 0; j < W; j++) {
                double const a = -2.0 * ALWAN_RG_PI * ((double)k - off[1]) * alwan_rg_fftfreq(j, W, uf);
                kr[k * W + j] = ALWAN_COS_F64(a);
                ki[k * W + j] = ALWAN_SIN_F64(a);
            }
        }
        for (k = 0; k < R; k++) {
            for (r = 0; r < H; r++) {
                double ar = 0.0, ai = 0.0;
                for (j = 0; j < W; j++) {
                    double const dr = sr[r * W + j], di = -si[r * W + j];
                    double const qr = kr[k * W + j], qi = ki[k * W + j];
                    ar += qr * dr - qi * di;
                    ai += qr * di + qi * dr;
                }
                mr[k * H + r] = ar;
                mi[k * H + r] = ai;
            }
        }
        /* then along the rows: out[m][k] = sum_r Kr[m][r] M[k][r], R x R; the result is conjugated */
        for (m = 0; m < R; m++) {
            for (r = 0; r < H; r++) {
                double const a = -2.0 * ALWAN_RG_PI * ((double)m - off[0]) * alwan_rg_fftfreq(r, H, uf);
                kr[m * H + r] = ALWAN_COS_F64(a);
                ki[m * H + r] = ALWAN_SIN_F64(a);
            }
        }
        for (m = 0; m < R; m++) {
            for (k = 0; k < R; k++) {
                double ar = 0.0, ai = 0.0, mag;
                for (r = 0; r < H; r++) {
                    double const qr = kr[m * H + r], qi = ki[m * H + r];
                    double const dr = mr[k * H + r], di = mi[k * H + r];
                    ar += qr * dr - qi * di;
                    ai += qr * di + qi * dr;
                }
                ai = -ai;
                mag = ALWAN_HYPOT_F64(ar, ai);
                if (mag > bm) bm = mag, bk = k, bmm = m, cc_re = ar, cc_im = ai;
            }
        }
        ALWAN_FREE(kr);
        shift[0] += ((double)bmm - dftshift) / uf;
        shift[1] += ((double)bk - dftshift) / uf;
    }
    if (H == 1) shift[0] = 0.0;
    if (W == 1) shift[1] = 0.0;
    if (!(src_amp > 0.0) || !(tgt_amp > 0.0) || !isfinite(cc_re) || !isfinite(cc_im)) {
        st = ALWAN_E_RANGE;
        goto done;
    }
    out->shift[0] = shift[0];
    out->shift[1] = shift[1];
    out->error = ALWAN_SQRT_F64(ALWAN_ABS_F64(1.0 - (cc_re * cc_re + cc_im * cc_im) / (src_amp * tgt_amp)));
    out->phasediff = ALWAN_ATAN2_F64(cc_im, cc_re);
done:
    if (fw) alwan__fft_destroy(fw);
    if (fh) alwan__fft_destroy(fh);
    if (buf) ALWAN_FREE(buf);
    return st;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_register_f64(alwan_register_result *out, alwan_f64 const *reference, size_t reference_row_stride,
                                alwan_f64 const *moving, size_t moving_row_stride, size_t width, size_t height,
                                alwan_register_method method, alwan_register_params const *params) {
    return alwan_rg_run(out, reference, reference_row_stride, moving, moving_row_stride, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_register_f32(alwan_register_result *out, alwan_f32 const *reference, size_t reference_row_stride,
                                alwan_f32 const *moving, size_t moving_row_stride, size_t width, size_t height,
                                alwan_register_method method, alwan_register_params const *params) {
    return alwan_rg_run(out, reference, reference_row_stride, moving, moving_row_stride, width, height, method, params, 1);
}
#endif

alwan_status alwan_register_u8(alwan_register_result *out, unsigned char const *reference, size_t reference_row_stride,
                               unsigned char const *moving, size_t moving_row_stride, size_t width, size_t height,
                               alwan_register_method method, alwan_register_params const *params) {
    return alwan_rg_run(out, reference, reference_row_stride, moving, moving_row_stride, width, height, method, params, 2);
}
