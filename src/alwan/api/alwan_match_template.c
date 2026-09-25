/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_match_template_{T}: normalised cross-correlation of a template over an image, as
 * scikit-image's feature.match_template (Lewis, "Fast Normalized Cross-Correlation"), which
 * suite 251 holds this to.
 *
 * Step for step as scikit-image: the image is padded by the template's size on every side
 * (numpy.pad's modes, applied axis by axis); the window sums of the padded image and of its
 * square are running sums down the columns, differenced, then running sums along the rows,
 * differenced, in that order, so they are scikit-image's to the bit; the template's mean and
 * its sum of squared deviations are numpy's pairwise sums. The cross-correlation, which
 * scikit-image takes from scipy's fftconvolve, comes from alwan's FFT on power-of-two sizes,
 * or directly when the template is small enough for that to cost less; either way it
 * differs from scipy's by rounding. Then
 *
 *   numerator   = xcorr - window_sum * template_mean
 *   denominator = sqrt(max(0, (window_sum2 - window_sum^2 / n) * template_ssd))
 *   response    = numerator / denominator where denominator > epsilon, else 0
 *
 * and the output is the slice scikit-image returns.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <string.h>

/* numpy's pairwise summation of a contiguous run (the blocks of 8, then halves). */
static double alwan_mt_pairwise(double const *a, size_t n) {
    if (n < 8) {
        double r = 0.0;
        size_t i;
        for (i = 0; i < n; i++) r += a[i];
        return r;
    }
    if (n <= 128) {
        double r[8], res;
        size_t i, j;
        for (j = 0; j < 8; j++) r[j] = a[j];
        for (i = 8; i < n - (n % 8); i += 8)
            for (j = 0; j < 8; j++) r[j] += a[i + j];
        res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; i++) res += a[i];
        return res;
    }
    {
        size_t n2 = n / 2;
        n2 -= n2 % 8;
        return alwan_mt_pairwise(a, n2) + alwan_mt_pairwise(a + n2, n - n2);
    }
}

/* The source index numpy.pad reads for padded index i (in [-p, n + p)) of a line of n. */
static long alwan_mt_map(long i, long n, alwan_template_pad mode) {
    long t, period;
    if (i >= 0 && i < n) return i;
    switch (mode) {
    case ALWAN_TEMPLATE_PAD_EDGE:
        return i < 0 ? 0 : n - 1;
    case ALWAN_TEMPLATE_PAD_WRAP:
        t = i % n;
        return t < 0 ? t + n : t;
    case ALWAN_TEMPLATE_PAD_SYMMETRIC:
        period = 2 * n;
        t = i % period;
        if (t < 0) t += period;
        return t < n ? t : period - 1 - t;
    case ALWAN_TEMPLATE_PAD_REFLECT:
        if (n == 1) return 0;
        period = 2 * (n - 1);
        t = i % period;
        if (t < 0) t += period;
        return t < n ? t : period - t;
    default:
        return -1;   /* CONSTANT */
    }
}

static double alwan_mt_read(void const *p, size_t rs, size_t x, size_t y, int kind) {
    char const *row = (char const *)p + y * rs;
    return kind == 0 ? ((alwan_f64 const *)row)[x] : kind == 1 ? (double)((alwan_f32 const *)row)[x] : (double)((unsigned char const *)row)[x];
}

static void alwan_mt_fft2(double *re, double *im, size_t W, size_t H, alwan__fft const *fw, alwan__fft const *fh, double *cre, double *cim,
                          int inverse) {
    size_t x, y;
    for (y = 0; y < H; y++) alwan__fft_run(fw, re + y * W, im + y * W, inverse);
    for (x = 0; x < W; x++) {
        for (y = 0; y < H; y++) cre[y] = re[y * W + x], cim[y] = im[y * W + x];
        alwan__fft_run(fh, cre, cim, inverse);
        for (y = 0; y < H; y++) re[y * W + x] = cre[y], im[y * W + x] = cim[y];
    }
}

static size_t alwan_mt_pow2(size_t n) {
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

static alwan_status alwan_mt_run(void *out, size_t out_rs, void const *img, size_t img_rs, size_t w, size_t h, void const *tpl, size_t tpl_rs,
                                 size_t tw, size_t th, alwan_template_params const *params, int kind) {
    alwan_template_params const zero = {0, ALWAN_TEMPLATE_PAD_CONSTANT, 0.0};
    alwan_template_params const *p = params ? params : &zero;
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const oelem = kind == 1 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t Hp, Wp, nr, nc, ow, oh, d0r, d0c, x, y, i, j, a, b, vol;
    double *P = NULL, *cs = NULL, *ws = NULL, *ws2 = NULL, *xc = NULL, *tv = NULL, *re = NULL, *im = NULL, *tre = NULL, *tim = NULL, *col = NULL;
    alwan__fft *fw = NULL, *fh = NULL;
    double tmean, tssd, eps;
    alwan_status st = ALWAN_OK;

    if (!out || !img || !tpl || w == 0 || h == 0 || tw == 0 || th == 0) return ALWAN_E_INVALID;
    if (img_rs / elem < w || tpl_rs / elem < tw) return ALWAN_E_INVALID;
    if ((unsigned)p->mode > (unsigned)ALWAN_TEMPLATE_PAD_WRAP) return ALWAN_E_INVALID;
    if (tw > w || th > h) return ALWAN_E_RANGE;
    ow = p->pad_input ? w : w - tw + 1;
    oh = p->pad_input ? h : h - th + 1;
    if (out_rs / oelem < ow) return ALWAN_E_INVALID;
    Hp = h + 2 * th;
    Wp = w + 2 * tw;
    if (Hp / 2 < th || Wp / 2 < tw || Wp > ((size_t)-1) / 8 / Hp) return ALWAN_E_RANGE;
    nr = Hp - th - 1;   /* rows of scikit-image's response before the slice */
    nc = Wp - tw - 1;
    d0r = p->pad_input ? (th - 1) / 2 : th - 1;
    d0c = p->pad_input ? (tw - 1) / 2 : tw - 1;
    vol = tw * th;

    P = (double *)ALWAN_ALLOC(alwan_safe_array_size(Hp * Wp, sizeof(double)), 64);
    cs = (double *)ALWAN_ALLOC(alwan_safe_array_size(Hp * Wp, sizeof(double)), 64);
    ws = (double *)ALWAN_ALLOC(alwan_safe_array_size(nr * nc, sizeof(double)), 64);
    ws2 = (double *)ALWAN_ALLOC(alwan_safe_array_size(nr * nc, sizeof(double)), 64);
    tv = (double *)ALWAN_ALLOC(alwan_safe_array_size(vol, sizeof(double)), 64);
    if (!P || !cs || !ws || !ws2 || !tv) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    /* the padded image, numpy.pad axis by axis (separable for every mode) */
    for (y = 0; y < Hp; y++) {
        long const sy = alwan_mt_map((long)y - (long)th, (long)h, p->mode);
        for (x = 0; x < Wp; x++) {
            long const sx = alwan_mt_map((long)x - (long)tw, (long)w, p->mode);
            P[y * Wp + x] = (sy < 0 || sx < 0) ? p->constant_value : alwan_mt_read(img, img_rs, (size_t)sx, (size_t)sy, kind);
        }
    }
    /* window sums of P, then of P squared: cumsum down, difference, cumsum along, difference */
    {
        int pass;
        for (pass = 0; pass < 2; pass++) {
            double *dst = pass == 0 ? ws : ws2;
            for (x = 0; x < Wp; x++) {
                double acc = 0.0;
                for (y = 0; y < Hp; y++) {
                    double const v = P[y * Wp + x];
                    acc += pass == 0 ? v : v * v;
                    cs[y * Wp + x] = acc;
                }
            }
            /* rows th .. Hp-2 minus rows 0 .. Hp-th-2, into cs rows 0..nr-1 (in place, ascending is safe) */
            for (i = 0; i < nr; i++)
                for (x = 0; x < Wp; x++) cs[i * Wp + x] = cs[(i + th) * Wp + x] - cs[i * Wp + x];
            for (i = 0; i < nr; i++) {
                double *row = cs + i * Wp;
                for (x = 1; x < Wp; x++) row[x] = row[x - 1] + row[x];
                for (j = 0; j < nc; j++) dst[i * nc + j] = row[j + tw] - row[j];
            }
        }
    }
    /* the template's mean and sum of squared deviations, numpy's way */
    for (a = 0; a < th; a++)
        for (b = 0; b < tw; b++) tv[a * tw + b] = alwan_mt_read(tpl, tpl_rs, b, a, kind);
    tmean = alwan_mt_pairwise(tv, vol) / (double)vol;
    {
        double *dev = cs;   /* scratch, Hp * Wp >= vol */
        for (i = 0; i < vol; i++) {
            double const d = tv[i] - tmean;
            dev[i] = d * d;
        }
        tssd = alwan_mt_pairwise(dev, vol);
    }
    /* cross-correlation over the output rows and columns only: xc[o_r][o_c] =
     * sum_ab P[i + 1 + a][j + 1 + b] T[a][b], i = d0r + o_r, j = d0c + o_c */
    xc = (double *)ALWAN_ALLOC(alwan_safe_array_size(ow * oh, sizeof(double)), 64);
    if (!xc) {
        st = ALWAN_E_NOMEM;
        goto done;
    }
    {
        size_t const Lr = alwan_mt_pow2(Hp), Lc = alwan_mt_pow2(Wp), L = Lr * Lc;
        double const direct = (double)ow * (double)oh * (double)vol;
        double const viafft = 3.0 * (double)L * (log2((double)L) + 2.0) * 2.5;
        if (direct <= viafft || L / Lr != Lc) {
            size_t or_, oc;
            for (or_ = 0; or_ < oh; or_++)
                for (oc = 0; oc < ow; oc++) {
                    size_t const i0 = d0r + or_ + 1, j0 = d0c + oc + 1;
                    double s = 0.0;
                    for (a = 0; a < th; a++) {
                        double const *pr = P + (i0 + a) * Wp + j0;
                        double const *tr = tv + a * tw;
                        for (b = 0; b < tw; b++) s += pr[b] * tr[b];
                    }
                    xc[or_ * ow + oc] = s;
                }
        } else {
            size_t or_, oc;
            double const scale = 1.0 / (double)L;
            re = (double *)ALWAN_ALLOC(alwan_safe_array_size(L, sizeof(double)), 64);
            im = (double *)ALWAN_ALLOC(alwan_safe_array_size(L, sizeof(double)), 64);
            tre = (double *)ALWAN_ALLOC(alwan_safe_array_size(L, sizeof(double)), 64);
            tim = (double *)ALWAN_ALLOC(alwan_safe_array_size(L, sizeof(double)), 64);
            col = (double *)ALWAN_ALLOC(alwan_safe_array_size(2 * Lr, sizeof(double)), 64);
            fw = alwan__fft_create(Lc);
            fh = alwan__fft_create(Lr);
            if (!re || !im || !tre || !tim || !col || !fw || !fh) {
                st = ALWAN_E_NOMEM;
                goto done;
            }
            memset(re, 0, L * sizeof(double));
            memset(im, 0, L * sizeof(double));
            memset(tre, 0, L * sizeof(double));
            memset(tim, 0, L * sizeof(double));
            for (y = 0; y < Hp; y++) memcpy(re + y * Lc, P + y * Wp, Wp * sizeof(double));
            for (a = 0; a < th; a++) memcpy(tre + a * Lc, tv + a * tw, tw * sizeof(double));
            alwan_mt_fft2(re, im, Lc, Lr, fw, fh, col, col + Lr, 0);
            alwan_mt_fft2(tre, tim, Lc, Lr, fw, fh, col, col + Lr, 0);
            for (i = 0; i < L; i++) {   /* A * conj(B): circular correlation */
                double const ar = re[i], ai = im[i], br = tre[i], bi = tim[i];
                re[i] = ar * br + ai * bi;
                im[i] = ai * br - ar * bi;
            }
            alwan_mt_fft2(re, im, Lc, Lr, fw, fh, col, col + Lr, 1);
            for (or_ = 0; or_ < oh; or_++)
                for (oc = 0; oc < ow; oc++) xc[or_ * ow + oc] = re[(d0r + or_ + 1) * Lc + (d0c + oc + 1)] * scale;
        }
    }
    eps = kind == 1 ? (double)FLT_EPSILON : DBL_EPSILON;
    {
        size_t or_, oc;
        for (or_ = 0; or_ < oh; or_++) {
            char *orow = (char *)out + or_ * out_rs;
            for (oc = 0; oc < ow; oc++) {
                size_t const k = (d0r + or_) * nc + (d0c + oc);
                double const s1 = ws[k], s2 = ws2[k];
                double const num = xc[or_ * ow + oc] - s1 * tmean;
                double den = (s2 - (s1 * s1) / (double)vol) * tssd;
                double r;
                if (den < 0.0) den = 0.0;
                den = sqrt(den);
                r = den > eps ? num / den : 0.0;
                if (kind == 1) ((alwan_f32 *)orow)[oc] = (alwan_f32)r;
                else ((alwan_f64 *)orow)[oc] = r;
            }
        }
    }
done:
    alwan__fft_destroy(fw);
    alwan__fft_destroy(fh);
    ALWAN_FREE(col);
    ALWAN_FREE(tim);
    ALWAN_FREE(tre);
    ALWAN_FREE(im);
    ALWAN_FREE(re);
    ALWAN_FREE(xc);
    ALWAN_FREE(tv);
    ALWAN_FREE(ws2);
    ALWAN_FREE(ws);
    ALWAN_FREE(cs);
    ALWAN_FREE(P);
    return st;
}

alwan_status alwan_match_template_u8(alwan_f64 *out, size_t out_row_stride, unsigned char const *image, size_t image_row_stride, size_t width,
                                     size_t height, unsigned char const *templ, size_t templ_row_stride, size_t templ_width, size_t templ_height,
                                     alwan_template_params const *params) {
    return alwan_mt_run(out, out_row_stride, image, image_row_stride, width, height, templ, templ_row_stride, templ_width, templ_height, params, 2);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_match_template_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *image, size_t image_row_stride, size_t width,
                                      size_t height, alwan_f64 const *templ, size_t templ_row_stride, size_t templ_width, size_t templ_height,
                                      alwan_template_params const *params) {
    return alwan_mt_run(out, out_row_stride, image, image_row_stride, width, height, templ, templ_row_stride, templ_width, templ_height, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_match_template_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *image, size_t image_row_stride, size_t width,
                                      size_t height, alwan_f32 const *templ, size_t templ_row_stride, size_t templ_width, size_t templ_height,
                                      alwan_template_params const *params) {
    return alwan_mt_run(out, out_row_stride, image, image_row_stride, width, height, templ, templ_row_stride, templ_width, templ_height, params, 1);
}
#endif
