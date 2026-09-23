/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Total-variation denoising: Chambolle, "An Algorithm for Total Variation Minimization
 * and Applications", J. Math. Imaging and Vision 20, 2004, for the Rudin, Osher and Fatemi
 * model
 *
 *     minimise  sum (u - f)^2 / 2 + weight TV(u),   TV(u) = sum |grad u|,
 *
 * by fixed-point iterations on the dual field p (one vector a pixel):
 *
 *     u = f - div p,    g = grad u (forward differences, 0 past the last row / column),
 *     p <- (p - tau g) / (1 + tau |g| / weight),   tau = 1/4,
 *
 * stopping when the energy E = (sum |div p|^2 + weight sum |g|) / pixels changes by less
 * than eps times its first value, or after max_iterations. Flat regions go flat and edges
 * stay sharp; a larger weight removes more.
 *
 * The reference is scikit-image's denoise_tv_chambolle (restoration/_denoise.py, BSD-3),
 * and this follows it step for step, channel by channel (channel_axis set): its u is the
 * one formed at the top of the last pass, before that pass updates p, so a run that ends
 * on max_iterations returns the u of the previous p. scikit-image sums the energy with
 * numpy's pairwise summation and this with a plain loop, so a run whose stopping test is
 * within rounding of eps can stop one iteration apart.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int alwan_dn_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

/* One channel, w x h, in place in `u` (holding f on entry); scratch 5 w h doubles. */
static void alwan_tv_plane(double *u, size_t w, size_t h, double weight, double eps, size_t max_iter, double *scratch) {
    size_t const n = w * h;
    double *f = scratch, *py = f + n, *px = py + n, *gy = px + n, *gx = gy + n;
    double e_init = 0.0, e_prev = 0.0;
    double const tau = 0.25;
    size_t i, x, y, p;
    memcpy(f, u, n * sizeof(double));
    memset(py, 0, 4 * n * sizeof(double));
    for (i = 0; i < max_iter; i++) {
        double e = 0.0, tv = 0.0;
        if (i > 0) {
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    size_t const q = y * w + x;
                    double d = -(py[q] + px[q]);
                    if (y > 0) d += py[q - w];
                    if (x > 0) d += px[q - 1];
                    u[q] = f[q] + d;
                    e += d * d;
                }
            }
        }
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                size_t const q = y * w + x;
                double nrm;
                gy[q] = y + 1 < h ? u[q + w] - u[q] : 0.0;
                gx[q] = x + 1 < w ? u[q + 1] - u[q] : 0.0;
                nrm = sqrt(gy[q] * gy[q] + gx[q] * gx[q]);
                tv += nrm;
            }
        }
        e = (e + weight * tv) / (double)n;
        for (p = 0; p < n; p++) {
            double const nrm = sqrt(gy[p] * gy[p] + gx[p] * gx[p]) * (tau / weight) + 1.0;
            py[p] = (py[p] - tau * gy[p]) / nrm;
            px[p] = (px[p] - tau * gx[p]) / nrm;
        }
        if (i == 0) {
            e_init = e;
            e_prev = e;
        } else {
            if (fabs(e_prev - e) < eps * e_init) break;
            e_prev = e;
        }
    }
}

static alwan_status alwan_tv_run(void *out, size_t out_row_stride, void const *src, size_t src_row_stride, size_t ch,
                                 size_t w, size_t h, double weight, double eps, size_t max_iter, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    size_t const n = w * h;
    double *data, *scratch;
    size_t x, y, c;
    if (!out || !src) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_row_stride / elem / ch < w || out_row_stride / elem / ch < w) return ALWAN_E_INVALID;
    if (!alwan_dn_finite(weight) || !alwan_dn_finite(eps)) return ALWAN_E_INVALID;
    if (!(weight > 0.0) || !(eps >= 0.0) || max_iter == 0 || max_iter > 100000 || n / w != h) return ALWAN_E_RANGE;

    data = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, ch * sizeof(double)), sizeof(double));
    scratch = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, 5 * sizeof(double)), sizeof(double));
    if (!data || !scratch) {
        if (data) ALWAN_FREE(data);
        if (scratch) ALWAN_FREE(scratch);
        return ALWAN_E_NOMEM;
    }
    for (y = 0; y < h; y++) {
        char const *srow = (char const *)src + y * src_row_stride;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double const v = is_f32 ? (double)((alwan_f32 const *)srow)[x * ch + c] : ((alwan_f64 const *)srow)[x * ch + c];
                if (!alwan_dn_finite(v)) {
                    ALWAN_FREE(data);
                    ALWAN_FREE(scratch);
                    return ALWAN_E_INVALID;
                }
                data[c * n + y * w + x] = v;
            }
        }
    }
    for (c = 0; c < ch; c++) alwan_tv_plane(data + c * n, w, h, weight, eps, max_iter, scratch);
    for (y = 0; y < h; y++) {
        char *orow = (char *)out + y * out_row_stride;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double const v = data[c * n + y * w + x];
                if (is_f32) ((alwan_f32 *)orow)[x * ch + c] = (alwan_f32)v;
                else ((alwan_f64 *)orow)[x * ch + c] = v;
            }
        }
    }
    ALWAN_FREE(data);
    ALWAN_FREE(scratch);
    return ALWAN_OK;
}

/*
 * Non-local means: Buades, Coll and Morel, "A non-local algorithm for image denoising",
 * CVPR 2005. Every pixel becomes the weighted mean of the pixels in a search window around
 * it, each weighted by how alike the patches (template windows) around the two are:
 *
 *     w = exp(-d / (h^2 channels)),  d = mean over the patch of sum_c (a_c - b_c)^2,
 *
 * so a pixel draws on others with the same neighbourhood, wherever they sit in the window,
 * and noise averages out while structure repeats.
 *
 * The reference is OpenCV's cv::fastNlMeansDenoising for 8-bit data and the L2 norm
 * (modules/photo/src/fast_nlmeans_denoising_invoker*.hpp, Apache-2.0), and this reproduces
 * its integer arithmetic bit for bit: the image extended by BORDER_REFLECT_101; the patch
 * sum of squared differences divided by the patch area rounded up to a power of two, as a
 * right shift, and looked up in a table of cvRound(m exp(-d' / (float(h h) channels)))
 * with d' the shifted sum times 2^shift / area, zeroed below 0.001 m, m = INT_MAX /
 * (search^2 255); the estimate (sum w p + sum w / 2) / sum w in unsigned integers. OpenCV
 * computes the patch sums incrementally along each row; this computes them with one
 * integral image per search offset, which gives the same integers. The window sizes are
 * forced odd, as OpenCV forces them.
 */
static alwan_status alwan_nlm_run(unsigned char *out, size_t out_row_stride, unsigned char const *src,
                                  size_t src_row_stride, size_t ch, size_t w, size_t h, double hparam, size_t tws,
                                  size_t sws) {
    long const th = (long)(tws / 2), sh = (long)(sws / 2);
    long const T = 2 * th + 1, S = 2 * sh + 1, bs = th + sh;
    long const ew = (long)w + 2 * bs, eh = (long)h + 2 * bs;
    long const n = (long)w * (long)h;
    int const fpm = (int)(2147483647L / ((long)S * S * 255L));
    int shift = 0;
    long tsq, maxd, amax, x, y, sy, sx, k;
    double mult;
    float const hh = (float)hparam * (float)hparam * (float)(int)ch;
    unsigned char *ext;
    int *lut;
    long long *integ;
    long long *est, *wsum;
    if (!out || !src) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_row_stride / ch < w || out_row_stride / ch < w) return ALWAN_E_INVALID;
    if (!(hparam == hparam) || !(hparam >= 0.0) || hparam > 1e6 || tws == 0 || sws == 0 || tws > 101 || sws > 201) {
        return ALWAN_E_RANGE;
    }
    if (w > 1000000 || h > 1000000 || n / (long)w != (long)h) return ALWAN_E_RANGE;

    tsq = T * T;
    while ((1L << shift) < tsq) shift++;
    mult = (double)(1L << shift) / (double)tsq;
    maxd = 255L * 255L * (long)ch;
    amax = (long)((double)maxd / mult + 1.0);

    ext = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size((size_t)ew * (size_t)eh, ch), 16);
    lut = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)amax, sizeof(int)), sizeof(int));
    integ = (long long *)ALWAN_ALLOC(alwan_safe_array_size((size_t)(w + 2 * th + 1) * (h + 2 * th + 1), sizeof(long long)), 16);
    est = (long long *)ALWAN_ALLOC(alwan_safe_array_size((size_t)n, (ch + 1) * sizeof(long long)), 16);
    if (!ext || !lut || !integ || !est) {
        if (ext) ALWAN_FREE(ext);
        if (lut) ALWAN_FREE(lut);
        if (integ) ALWAN_FREE(integ);
        if (est) ALWAN_FREE(est);
        return ALWAN_E_NOMEM;
    }
    wsum = est + (size_t)n * ch;
    memset(est, 0, (size_t)n * (ch + 1) * sizeof(long long));

    for (k = 0; k < amax; k++) {
        double const d = (double)k * mult;
        double wv = exp(-d / (double)hh);
        double r;
        long rv;
        if (wv != wv) wv = 1.0; /* h = 0: 0 / 0 at d = 0 */
        r = (double)fpm * wv;
        rv = (long)floor(r);
        if (r - (double)rv > 0.5 || (r - (double)rv == 0.5 && (rv & 1))) rv++; /* cvRound, ties to even */
        if ((double)rv < 0.001 * (double)fpm) rv = 0;
        lut[k] = (int)rv;
    }

    for (y = 0; y < eh; y++) {
        long yy = y - bs;
        while (yy < 0 || yy >= (long)h) {
            if (h == 1) { yy = 0; break; }
            if (yy < 0) yy = -yy;
            if (yy >= (long)h) yy = 2 * ((long)h - 1) - yy;
        }
        for (x = 0; x < ew; x++) {
            long xx = x - bs;
            size_t c;
            while (xx < 0 || xx >= (long)w) {
                if (w == 1) { xx = 0; break; }
                if (xx < 0) xx = -xx;
                if (xx >= (long)w) xx = 2 * ((long)w - 1) - xx;
            }
            for (c = 0; c < ch; c++) ext[((size_t)y * ew + x) * ch + c] = src[(size_t)yy * src_row_stride + (size_t)xx * ch + c];
        }
    }

    {
        long const iw = (long)w + 2 * th + 1, ih = (long)h + 2 * th + 1; /* integral over the patch-centre area plus a margin */
        for (sy = -sh; sy <= sh; sy++) {
            for (sx = -sh; sx <= sh; sx++) {
                /* integ[(y + 1) iw + (x + 1)]: sum of d over rows < y + 1, cols < x + 1 of the area
                 * starting at ext (sh, sh), which is image pixel (-th, -th) */
                for (x = 0; x < iw; x++) integ[x] = 0;
                for (y = 0; y + 1 < ih; y++) {
                    long long rowsum = 0;
                    unsigned char const *a = ext + ((size_t)(y + sh) * ew + sh) * ch;
                    unsigned char const *b = ext + ((size_t)(y + sh + sy) * ew + sh + sx) * ch;
                    integ[(y + 1) * iw] = 0;
                    for (x = 0; x + 1 < iw; x++) {
                        int d = 0;
                        size_t c;
                        for (c = 0; c < ch; c++) {
                            int const t = (int)a[x * ch + c] - (int)b[x * ch + c];
                            d += t * t;
                        }
                        rowsum += d;
                        integ[(y + 1) * iw + x + 1] = integ[y * iw + x + 1] + rowsum;
                    }
                }
                for (y = 0; y < (long)h; y++) {
                    for (x = 0; x < (long)w; x++) {
                        /* the patch around image pixel (x, y) covers area rows y..y + 2 th */
                        long long const ds = integ[(y + T) * iw + x + T] - integ[y * iw + x + T] - integ[(y + T) * iw + x] + integ[y * iw + x];
                        int const wt = lut[(long)(ds >> shift)];
                        size_t const q = (size_t)y * w + (size_t)x;
                        unsigned char const *p = ext + ((size_t)(y + bs + sy) * ew + (size_t)(x + bs + sx)) * ch;
                        size_t c;
                        if (wt == 0) continue;
                        for (c = 0; c < ch; c++) est[q * ch + c] += (long long)wt * p[c];
                        wsum[q] += wt;
                    }
                }
            }
        }
    }

    for (y = 0; y < (long)h; y++) {
        for (x = 0; x < (long)w; x++) {
            size_t const q = (size_t)y * w + (size_t)x;
            size_t c;
            for (c = 0; c < ch; c++) {
                unsigned int const e = (unsigned int)est[q * ch + c];
                int const ws = (int)wsum[q];
                unsigned int const v = (e + (unsigned int)(ws / 2)) / (unsigned int)ws;
                out[(size_t)y * out_row_stride + (size_t)x * ch + c] = (unsigned char)(v > 255u ? 255u : v);
            }
        }
    }
    ALWAN_FREE(ext);
    ALWAN_FREE(lut);
    ALWAN_FREE(integ);
    ALWAN_FREE(est);
    return ALWAN_OK;
}

/*
 * Anisotropic diffusion: Perona and Malik, "Scale-space and edge detection using
 * anisotropic diffusion", IEEE PAMI 1990. Each iteration moves every pixel toward its eight
 * neighbours by
 *
 *     I <- I + alpha sum_n g(|I_n - I|) (I_n - I),   g(d) = exp(-(d / (K channels 255))^2),
 *
 * d the L1 distance over the channels in 0..255 units, so small differences (noise,
 * texture) diffuse and large ones (edges) stay.
 *
 * The reference is OpenCV's ximgproc::anisotropicDiffusion (opencv_contrib, BSD-3), 8-bit
 * three-channel, and this reproduces it bit for bit for three channels: a float table
 * exp(-(k k) / sigma^2), sigma = K 3 255 in float, the neighbours in the order left, right,
 * the three above, the three below, the sum accumulated in float, the result
 * saturate(round(I + alpha s)) rounding half to even, the border replicated before every
 * iteration, each iteration reading only the last. OpenCV's table has 765 entries and reads
 * one past its end when a pure black pixel meets a pure white one (d = 765); this table
 * has 766. For 1, 2 or 4 channels the same formula holds with their own count.
 *
 * OpenCV's loop is right for one iteration only: it refreshes the border with
 * copyMakeBorder from a view into the padded buffer itself, which (without
 * BORDER_ISOLATED) grows the view into the buffer instead of replicating, so later
 * iterations see a stale or never-written border and the result depends on that memory.
 * This replicates the border before every iteration, which equals n chained one-iteration
 * OpenCV calls, and that is what the test holds it to.
 */
static long alwan_ad_round(float v) {
    float f = floorf(v);
    float const d = v - f;
    if (d > 0.5f || (d == 0.5f && fmodf(f, 2.0f) != 0.0f)) f += 1.0f;
    return (long)f;
}

static alwan_status alwan_ad_run(unsigned char *out, size_t out_row_stride, unsigned char const *src,
                                 size_t src_row_stride, size_t channels, size_t width, size_t height, double alpha,
                                 double k, size_t iterations) {
    size_t const ch = channels, w = width, h = height;
    size_t const pw = w + 2, ph = h + 2;
    float const af = (float)alpha;
    float const sigma = (float)k * (float)(int)ch * 255.0f;
    float const isigma2 = 1.0f / (sigma * sigma);
    size_t const tabn = 255 * ch + 1;
    float *tab;
    unsigned char *a, *b;
    size_t it, x, y, c, i;
    if (!out || !src) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_row_stride / ch < w || out_row_stride / ch < w) return ALWAN_E_INVALID;
    if (!(alpha == alpha) || !(k == k) || !(alpha > 0.0) || k == 0.0 || alpha > 1e6 || k > 1e6 || k < -1e6 || iterations > 100000) {
        return ALWAN_E_RANGE;
    }
    if (iterations == 0) {
        for (y = 0; y < h; y++) memmove(out + y * out_row_stride, src + y * src_row_stride, w * ch);
        return ALWAN_OK;
    }
    tab = (float *)ALWAN_ALLOC(tabn * sizeof(float), sizeof(float));
    a = (unsigned char *)ALWAN_ALLOC(alwan_safe_array_size(pw * ph, 2 * ch), 16);
    if (!tab || !a) {
        if (tab) ALWAN_FREE(tab);
        if (a) ALWAN_FREE(a);
        return ALWAN_E_NOMEM;
    }
    b = a + pw * ph * ch;
    for (i = 0; i < tabn; i++) tab[i] = expf(-(float)(int)(i * i) * isigma2);
    for (y = 0; y < h; y++) memcpy(a + ((y + 1) * pw + 1) * ch, src + y * src_row_stride, w * ch);

    for (it = 0; it < iterations; it++) {
        /* replicate the border of a */
        for (y = 1; y <= h; y++) {
            memcpy(a + (y * pw) * ch, a + (y * pw + 1) * ch, ch);
            memcpy(a + (y * pw + w + 1) * ch, a + (y * pw + w) * ch, ch);
        }
        memcpy(a, a + pw * ch, pw * ch);
        memcpy(a + (h + 1) * pw * ch, a + h * pw * ch, pw * ch);
        for (y = 1; y <= h; y++) {
            for (x = 1; x <= w; x++) {
                unsigned char const *p0 = a + (y * pw + x) * ch;
                long const nb[8] = { -(long)ch, (long)ch, -(long)(pw * ch) - (long)ch, -(long)(pw * ch),
                                     -(long)(pw * ch) + (long)ch, (long)(pw * ch) - (long)ch, (long)(pw * ch),
                                     (long)(pw * ch) + (long)ch };
                float s[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                int kk;
                for (kk = 0; kk < 8; kk++) {
                    unsigned char const *p1 = p0 + nb[kk];
                    int dl[4], nabla = 0;
                    float wgt;
                    for (c = 0; c < ch; c++) {
                        dl[c] = (int)p1[c] - (int)p0[c];
                        nabla += dl[c] < 0 ? -dl[c] : dl[c];
                    }
                    wgt = tab[nabla];
                    for (c = 0; c < ch; c++) s[c] += (float)dl[c] * wgt;
                }
                for (c = 0; c < ch; c++) {
                    long const r = alwan_ad_round((float)p0[c] + af * s[c]);
                    b[(y * pw + x) * ch + c] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r);
                }
            }
        }
        { unsigned char *t = a; a = b; b = t; }
    }
    for (y = 0; y < h; y++) memcpy(out + y * out_row_stride, a + ((y + 1) * pw + 1) * ch, w * ch);
    ALWAN_FREE(tab);
    ALWAN_FREE(a < b ? a : b);
    return ALWAN_OK;
}

/*
 * DCT denoising: Yu and Sapiro, "DCT Image Denoising: a Simple and Effective Image
 * Denoising Algorithm", IPOL 2011. Every block_size x block_size patch of the image, at
 * every position, goes through an orthonormal 2D DCT-II; coefficients of magnitude at most
 * 3 sigma are zeroed (the DC one too, if it is that small); the inverse DCT gives the
 * patch back, and each pixel is the mean of every patch estimate covering it. Three
 * channels are first turned by the orthonormal opponent transform
 * (1, 1, 1) / sqrt3, (1, 0, -1) / sqrt2, (1, -2, 1) / sqrt6 and back after.
 *
 * The reference is OpenCV's xphoto::dctDenoising (opencv_contrib, BSD-3), and this follows
 * it except at the border: OpenCV places patches at x < width - block_size only, so the
 * last row and column are covered by no patch, and its output there is 0 for 8-bit data
 * and NaN for floats. Here patches run to x = width - block_size, as in the paper's own
 * code, so every pixel is covered. Rows and columns more than one block from the right
 * and bottom edges are unaffected by the difference.
 */
static alwan_status alwan_dct_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w,
                                  size_t h, double sigma, size_t ps, int kind /* 0 f64, 1 f32, 2 u8 */) {
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const n = w * h;
    double const thresh = 3.0 * sigma;
    double *plane, *acc, *cnt, *cm, *pa, *pb;
    size_t x, y, c, i, j, k, x0, y0;
    double const s3 = 1.0 / sqrt(3.0), s2 = 1.0 / sqrt(2.0), s6 = 1.0 / sqrt(6.0);
    double const opp[3][3] = { { s3, s3, s3 }, { s2, 0.0, -s2 }, { s6, -2.0 * s6, s6 } };
    if (!out || !src) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if (!alwan_dn_finite(sigma) || !(sigma >= 0.0)) return ALWAN_E_RANGE;
    if (ps < 2 || ps > 64 || w < ps || h < ps || n / w != h) return ALWAN_E_RANGE;

    plane = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, (ch + 2) * sizeof(double)), sizeof(double));
    cm = (double *)ALWAN_ALLOC(alwan_safe_array_size(ps * ps, 3 * sizeof(double)), sizeof(double));
    if (!plane || !cm) {
        if (plane) ALWAN_FREE(plane);
        if (cm) ALWAN_FREE(cm);
        return ALWAN_E_NOMEM;
    }
    acc = plane + n * ch;
    cnt = acc + n;
    pa = cm + ps * ps;
    pb = pa + ps * ps;
    for (k = 0; k < ps; k++) {
        double const a = k == 0 ? sqrt(1.0 / (double)ps) : sqrt(2.0 / (double)ps);
        for (i = 0; i < ps; i++) cm[k * ps + i] = a * cos(3.14159265358979323846 * (double)((2 * i + 1) * k) / (double)(2 * ps));
    }

    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            double v[4];
            for (c = 0; c < ch; c++) {
                v[c] = kind == 0 ? ((alwan_f64 const *)row)[x * ch + c]
                     : kind == 1 ? (double)((alwan_f32 const *)row)[x * ch + c]
                                 : (double)((unsigned char const *)row)[x * ch + c];
                if (!alwan_dn_finite(v[c])) {
                    ALWAN_FREE(plane);
                    ALWAN_FREE(cm);
                    return ALWAN_E_INVALID;
                }
            }
            for (c = 0; c < ch; c++) {
                plane[c * n + y * w + x] = ch == 3 ? opp[c][0] * v[0] + opp[c][1] * v[1] + opp[c][2] * v[2] : v[c];
            }
        }
    }

    for (c = 0; c < ch; c++) {
        double *pl = plane + c * n;
        for (i = 0; i < n; i++) { acc[i] = 0.0; cnt[i] = 0.0; }
        for (y0 = 0; y0 + ps <= h; y0++) {
            for (x0 = 0; x0 + ps <= w; x0++) {
                /* pb = C P, then pa = (C P) C^T: the 2D DCT */
                for (k = 0; k < ps; k++) {
                    for (j = 0; j < ps; j++) {
                        double s = 0.0;
                        for (i = 0; i < ps; i++) s += cm[k * ps + i] * pl[(y0 + i) * w + x0 + j];
                        pb[k * ps + j] = s;
                    }
                }
                for (k = 0; k < ps; k++) {
                    for (j = 0; j < ps; j++) {
                        double s = 0.0;
                        for (i = 0; i < ps; i++) s += pb[k * ps + i] * cm[j * ps + i];
                        pa[k * ps + j] = fabs(s) > thresh ? s : 0.0;
                    }
                }
                /* the inverse: C^T D C */
                for (k = 0; k < ps; k++) {
                    for (j = 0; j < ps; j++) {
                        double s = 0.0;
                        for (i = 0; i < ps; i++) s += cm[i * ps + k] * pa[i * ps + j];
                        pb[k * ps + j] = s;
                    }
                }
                for (k = 0; k < ps; k++) {
                    for (j = 0; j < ps; j++) {
                        double s = 0.0;
                        for (i = 0; i < ps; i++) s += pb[k * ps + i] * cm[i * ps + j];
                        acc[(y0 + k) * w + x0 + j] += s;
                        cnt[(y0 + k) * w + x0 + j] += 1.0;
                    }
                }
            }
        }
        for (i = 0; i < n; i++) pl[i] = acc[i] / cnt[i];
    }

    for (y = 0; y < h; y++) {
        char *row = (char *)out + y * out_rs;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double v;
                if (ch == 3) { /* the opponent transform is orthonormal: its inverse is its transpose */
                    v = opp[0][c] * plane[y * w + x] + opp[1][c] * plane[n + y * w + x] + opp[2][c] * plane[2 * n + y * w + x];
                } else {
                    v = plane[c * n + y * w + x];
                }
                if (kind == 0) {
                    ((alwan_f64 *)row)[x * ch + c] = v;
                } else if (kind == 1) {
                    ((alwan_f32 *)row)[x * ch + c] = (alwan_f32)v;
                } else {
                    double f = floor(v);
                    double const d = v - f;
                    if (d > 0.5 || (d == 0.5 && fmod(f, 2.0) != 0.0)) f += 1.0;
                    ((unsigned char *)row)[x * ch + c] = (unsigned char)(f < 0.0 ? 0.0 : f > 255.0 ? 255.0 : f);
                }
            }
        }
    }
    ALWAN_FREE(plane);
    ALWAN_FREE(cm);
    return ALWAN_OK;
}

/*
 * Wavelet denoising: Donoho and Johnstone's wavelet shrinkage (VisuShrink, Biometrika
 * 1994) and Chang, Yu and Vetterli's BayesShrink (IEEE TIP 2000). Each channel goes through
 * a multilevel 2D orthogonal wavelet transform; every detail sub-band is soft- or
 * hard-thresholded; the transform is inverted. The noise level is sigma, or when sigma is 0
 * the robust estimate median(|d|) / 0.6745 over the non-zero coefficients of the finest
 * diagonal sub-band. BayesShrink thresholds each sub-band at sigma^2 / sqrt(max(mean(d^2) -
 * sigma^2, eps)); VisuShrink everywhere at sigma sqrt(2 ln n), n the channel's pixel count.
 *
 * The reference is scikit-image's restoration.denoise_wavelet (channel_axis set,
 * convert2ycbcr off) over PyWavelets, and this follows it: PyWavelets' 'symmetric' mode
 * (half-sample symmetric extension), a level of n samples giving (n + F - 1) / 2
 * coefficients from the odd positions of the full convolution, the inverse the middle
 * 2 n - F + 2 samples of the upsampled convolution, each level's approximation cropped to
 * its detail shape before the inverse, and levels = max(floor(log2(min(w, h) / (F - 1))) -
 * 3, 1) by default. Hard thresholding zeroes |d| < t, as pywt.threshold does. The filter
 * banks (data/wavelets/wavelet_filters.csv) are exported from PyWavelets by
 * gendata/data/wavelet_filters.py.
 */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const alwan_wv_table[] = {
#include "../data/wavelets/wavelet_filters.csv"
};
ALWAN_DIAG_POP

/* The filters of wavelet k: F, then dec_lo, dec_hi, rec_lo, rec_hi, each F long. */
static int alwan_wv_find(int k, size_t *flen, alwan_f64 const **filters) {
    int const count = (int)alwan_wv_table[0];
    alwan_f64 const *p = alwan_wv_table + 1;
    int i;
    if (k < 0 || k >= count) return 0;
    for (i = 0; i < k; i++) p += 1 + 4 * (size_t)p[0];
    *flen = (size_t)p[0];
    *filters = p + 1;
    return 1;
}

static size_t alwan_wv_sym(long i, long n) {
    while (i < 0 || i >= n) {
        if (i < 0) i = -i - 1;
        if (i >= n) i = 2 * n - 1 - i;
    }
    return (size_t)i;
}

/* One 1D step: n samples `step` apart into m = (n + F - 1) / 2 approximation and detail
 * coefficients, `ostep` apart. */
static void alwan_wv_dwt1(double *a, double *d, size_t ostep, double const *x, size_t step, size_t n,
                          double const *lo, double const *hi, size_t F) {
    size_t const m = (n + F - 1) / 2;
    size_t o, j;
    for (o = 0; o < m; o++) {
        long const i = (long)(2 * o + 1);
        double sa = 0.0, sd = 0.0;
        for (j = 0; j < F; j++) {
            double const v = x[alwan_wv_sym(i - (long)j, (long)n) * step];
            sa += lo[j] * v;
            sd += hi[j] * v;
        }
        a[o * ostep] = sa;
        d[o * ostep] = sd;
    }
}

/* One 1D inverse: n approximation and detail coefficients into 2 n - F + 2 samples. */
static void alwan_wv_idwt1(double *out, size_t ostep, double const *a, double const *d, size_t step, size_t n,
                           double const *rlo, double const *rhi, size_t F) {
    size_t const L = 2 * n - F + 2;
    size_t k, j;
    for (k = 0; k < L; k++) {
        long const t = (long)(k + F) - 2;
        double s = 0.0;
        long const j0 = t - (long)F + 1 > 0 ? (t - (long)F + 2) / 2 : 0;
        for (j = (size_t)j0; j < n; j++) {
            long const f = t - 2 * (long)j;
            if (f < 0) break;
            if (f < (long)F) s += rlo[f] * a[j * step] + rhi[f] * d[j * step];
        }
        out[k * ostep] = s;
    }
}

static int alwan_wv_cmp(void const *x, void const *y) {
    double const a = *(double const *)x, b = *(double const *)y;
    return a < b ? -1 : a > b ? 1 : 0;
}

/* Denoise one channel of w x h doubles in place. */
static alwan_status alwan_wv_plane(double *img, size_t w, size_t h, alwan_f64 const *filt, size_t F, size_t levels,
                                   double sigma, int visu, int hard) {
    double const *dlo = filt, *dhi = filt + F, *rlo = filt + 2 * F, *rhi = filt + 3 * F;
    size_t lw[40], lh[40], off[40]; /* level l: its four sub-bands of lw x lh at off */
    size_t total = 0, l, i, x, y, k;
    double *bands, *tmp, *cur;
    size_t cw = w, ch = h;
    for (l = 0; l < levels; l++) {
        lw[l] = (cw + F - 1) / 2;
        lh[l] = (ch + F - 1) / 2;
        off[l] = total;
        total += 4 * lw[l] * lh[l];
        cw = lw[l];
        ch = lh[l];
    }
    bands = (double *)ALWAN_ALLOC(alwan_safe_array_size(total + 2 * (2 * w + 2 * F) * (2 * h + 2 * F), sizeof(double)), sizeof(double));
    if (!bands) return ALWAN_E_NOMEM;
    tmp = bands + total;                              /* a column-pass buffer */
    cur = tmp + (2 * w + 2 * F) * (2 * h + 2 * F); /* the current approximation / reconstruction */

    /* forward: a x b image `src` (row stride sw) into the level's aa, ad, da, dd */
    {
        double const *src = img;
        size_t sw = w, sh = h, sstride = w;
        for (l = 0; l < levels; l++) {
            size_t const bw = lw[l], bh = lh[l];
            double *aa = bands + off[l], *ad = aa + bw * bh, *da = ad + bw * bh, *dd = da + bw * bh;
            double *ta = tmp, *td = tmp + bh * sw; /* after the column pass: bh x sw each */
            for (x = 0; x < sw; x++) alwan_wv_dwt1(ta + x, td + x, sw, src + x, sstride, sh, dlo, dhi, F);
            for (y = 0; y < bh; y++) {
                alwan_wv_dwt1(aa + y * bw, ad + y * bw, 1, ta + y * sw, 1, sw, dlo, dhi, F);
                alwan_wv_dwt1(da + y * bw, dd + y * bw, 1, td + y * sw, 1, sw, dlo, dhi, F);
            }
            src = aa;
            sw = bw;
            sh = bh;
            sstride = bw;
        }
    }

    /* the noise level and the thresholds */
    if (sigma == 0.0) {
        double *dd = bands + off[0] + 3 * lw[0] * lh[0];
        size_t const nd = lw[0] * lh[0];
        size_t nz = 0;
        double *v = tmp;
        for (i = 0; i < nd; i++) if (dd[i] != 0.0) v[nz++] = fabs(dd[i]);
        if (nz == 0) {
            sigma = 0.0;
        } else {
            qsort(v, nz, sizeof(double), alwan_wv_cmp);
            sigma = (nz & 1) ? v[nz / 2] : 0.5 * (v[nz / 2 - 1] + v[nz / 2]);
            sigma /= 0.6744897501960817; /* the normal's 75th percentile */
        }
    }
    for (l = 0; l < levels; l++) {
        size_t const nb = lw[l] * lh[l];
        for (k = 1; k < 4; k++) {
            double *b = bands + off[l] + k * nb;
            double t;
            if (visu) {
                t = sigma * sqrt(2.0 * log((double)(w * h)));
            } else {
                double m2 = 0.0, dv;
                for (i = 0; i < nb; i++) m2 += b[i] * b[i];
                m2 /= (double)nb;
                dv = m2 - sigma * sigma;
                t = sigma * sigma / sqrt(dv > DBL_EPSILON ? dv : DBL_EPSILON);
            }
            for (i = 0; i < nb; i++) {
                double const a = fabs(b[i]);
                if (hard) b[i] = a < t ? 0.0 : b[i];
                else b[i] = a > t ? (b[i] > 0.0 ? a - t : t - a) : 0.0;
            }
        }
    }

    /* inverse: from the coarsest, the approximation cropped to each level's detail shape */
    {
        size_t aw = lw[levels - 1], ah = lh[levels - 1];
        memcpy(cur, bands + off[levels - 1], aw * ah * sizeof(double));
        for (l = levels; l-- > 0;) {
            size_t const bw = lw[l], bh = lh[l];
            double const *ad = bands + off[l] + bw * bh, *da = ad + bw * bh, *dd = da + bw * bh;
            size_t const rw = 2 * bw - F + 2, rh = 2 * bh - F + 2;
            double *ra = tmp, *rd2 = tmp + bh * rw;
            /* crop the approximation to bw x bh (its row stride is aw) */
            for (y = 0; y < bh; y++) {
                alwan_wv_idwt1(ra + y * rw, 1, cur + y * aw, ad + y * bw, 1, bw, rlo, rhi, F);
                alwan_wv_idwt1(rd2 + y * rw, 1, da + y * bw, dd + y * bw, 1, bw, rlo, rhi, F);
            }
            for (x = 0; x < rw; x++) alwan_wv_idwt1(cur + x, rw, ra + x, rd2 + x, rw, bh, rlo, rhi, F);
            aw = rw;
            ah = rh;
        }
        (void)ah;
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) img[y * w + x] = cur[y * aw + x];
    }
    ALWAN_FREE(bands);
    return ALWAN_OK;
}

static alwan_status alwan_wv_run(void *out, size_t out_rs, void const *src, size_t src_rs, size_t ch, size_t w, size_t h,
                                 alwan_denoise_params const *p, int kind /* 0 f64, 1 f32, 2 u8 */) {
    size_t const elem = kind == 0 ? sizeof(alwan_f64) : kind == 1 ? sizeof(alwan_f32) : 1u;
    size_t const n = w * h;
    double const scale = kind == 2 ? 255.0 : 1.0;
    double const sigma = p->sigma / scale;
    alwan_f64 const *filt;
    size_t F, levels, maxl, m, x, y, c;
    double *plane;
    alwan_status st = ALWAN_OK;
    if (!out || !src) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_rs / elem / ch < w || out_rs / elem / ch < w) return ALWAN_E_INVALID;
    if (!alwan_wv_find(p->wavelet, &F, &filt)) return ALWAN_E_INVALID;
    if (!alwan_dn_finite(p->sigma) || p->sigma < 0.0 || n / w != h) return ALWAN_E_RANGE;
    m = w < h ? w : h;
    maxl = 0;
    if (m >= F - 1) {
        while (((size_t)1 << (maxl + 1)) * (F - 1) <= m) maxl++;
    }
    levels = p->wavelet_levels != 0 ? p->wavelet_levels : (maxl > 4 ? maxl - 3 : 1);
    if (levels > 30 || (p->wavelet_levels != 0 && levels > (maxl > 0 ? maxl : 1))) return ALWAN_E_RANGE;

    plane = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, ch * sizeof(double)), sizeof(double));
    if (!plane) return ALWAN_E_NOMEM;
    for (y = 0; y < h; y++) {
        char const *row = (char const *)src + y * src_rs;
        for (x = 0; x < w; x++) {
            for (c = 0; c < ch; c++) {
                double const v = kind == 0 ? ((alwan_f64 const *)row)[x * ch + c]
                               : kind == 1 ? (double)((alwan_f32 const *)row)[x * ch + c]
                                           : (double)((unsigned char const *)row)[x * ch + c] / 255.0;
                if (!alwan_dn_finite(v)) {
                    ALWAN_FREE(plane);
                    return ALWAN_E_INVALID;
                }
                plane[c * n + y * w + x] = v;
            }
        }
    }
    for (c = 0; c < ch && st == ALWAN_OK; c++) {
        st = alwan_wv_plane(plane + c * n, w, h, filt, F, levels, sigma, p->wavelet_visushrink != 0, p->wavelet_hard != 0);
    }
    if (st == ALWAN_OK) {
        for (y = 0; y < h; y++) {
            char *row = (char *)out + y * out_rs;
            for (x = 0; x < w; x++) {
                for (c = 0; c < ch; c++) {
                    double const v = plane[c * n + y * w + x];
                    if (kind == 0) {
                        ((alwan_f64 *)row)[x * ch + c] = v;
                    } else if (kind == 1) {
                        ((alwan_f32 *)row)[x * ch + c] = (alwan_f32)v;
                    } else {
                        double f = floor(v * 255.0 + 0.5);
                        ((unsigned char *)row)[x * ch + c] = (unsigned char)(f < 0.0 ? 0.0 : f > 255.0 ? 255.0 : f);
                    }
                }
            }
        }
    }
    ALWAN_FREE(plane);
    return st;
}

/* ---- the family ---- */

static double alwan_dn_or(double v, double def) {
    return v == 0.0 ? def : v;
}

/* TV on 8-bit data: through double in 0..1, rounded back half to even (as the rest of the
 * 8-bit paths here round). */
static alwan_status alwan_tv_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src,
                                size_t src_row_stride, size_t ch, size_t w, size_t h, double weight, double eps,
                                size_t max_iter) {
    size_t const n = w * h * ch;
    double *buf;
    size_t x, y;
    alwan_status st;
    if (!out || !src) return ALWAN_E_INVALID;
    if (w == 0 || h == 0 || ch == 0 || ch > 4) return ALWAN_E_INVALID;
    if (src_row_stride / ch < w || out_row_stride / ch < w || n / w / ch != h) return ALWAN_E_INVALID;
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(n, sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w * ch; x++) buf[y * w * ch + x] = (double)src[y * src_row_stride + x] / 255.0;
    }
    st = alwan_tv_run(buf, w * ch * sizeof(double), buf, w * ch * sizeof(double), ch, w, h, weight, eps, max_iter, 0);
    if (st == ALWAN_OK) {
        for (y = 0; y < h; y++) {
            for (x = 0; x < w * ch; x++) {
                double v = buf[y * w * ch + x] * 255.0, f = floor(v);
                double const d = v - f;
                if (d > 0.5 || (d == 0.5 && fmod(f, 2.0) != 0.0)) f += 1.0;
                out[y * out_row_stride + x] = (unsigned char)(f < 0.0 ? 0.0 : f > 255.0 ? 255.0 : f);
            }
        }
    }
    ALWAN_FREE(buf);
    return st;
}

static alwan_status alwan_dn_float(void *out, size_t out_row_stride, void const *src, size_t src_row_stride,
                                   size_t channels, size_t width, size_t height, alwan_denoise_method method,
                                   alwan_denoise_params const *params, int is_f32) {
    alwan_denoise_params const zero = { 0 };
    alwan_denoise_params const *p = params ? params : &zero;
    switch (method) {
    case ALWAN_DENOISE_TV_CHAMBOLLE:
        return alwan_tv_run(out, out_row_stride, src, src_row_stride, channels, width, height, alwan_dn_or(p->weight, 0.1),
                            alwan_dn_or(p->tolerance, 2e-4), p->iterations == 0 ? 200 : p->iterations, is_f32);
    case ALWAN_DENOISE_DCT:
        return alwan_dct_run(out, out_row_stride, src, src_row_stride, channels, width, height,
                             alwan_dn_or(p->sigma, 10.0 / 255.0), p->block_size == 0 ? 16 : p->block_size, is_f32 ? 1 : 0);
    case ALWAN_DENOISE_WAVELET:
        return alwan_wv_run(out, out_row_stride, src, src_row_stride, channels, width, height, p, is_f32 ? 1 : 0);
    case ALWAN_DENOISE_NL_MEANS:
    case ALWAN_DENOISE_ANISOTROPIC_DIFFUSION: /* 8-bit only, as their references are */
    default:
        return ALWAN_E_INVALID;
    }
}

alwan_status alwan_denoise_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src, size_t src_row_stride,
                              size_t channels, size_t width, size_t height, alwan_denoise_method method,
                              alwan_denoise_params const *params) {
    alwan_denoise_params const zero = { 0 };
    alwan_denoise_params const *p = params ? params : &zero;
    switch (method) {
    case ALWAN_DENOISE_TV_CHAMBOLLE:
        return alwan_tv_u8(out, out_row_stride, src, src_row_stride, channels, width, height, alwan_dn_or(p->weight, 0.1),
                           alwan_dn_or(p->tolerance, 2e-4), p->iterations == 0 ? 200 : p->iterations);
    case ALWAN_DENOISE_NL_MEANS:
        return alwan_nlm_run(out, out_row_stride, src, src_row_stride, channels, width, height, alwan_dn_or(p->h, 10.0),
                             p->template_window == 0 ? 7 : p->template_window,
                             p->search_window == 0 ? 21 : p->search_window);
    case ALWAN_DENOISE_DCT:
        return alwan_dct_run(out, out_row_stride, src, src_row_stride, channels, width, height, alwan_dn_or(p->sigma, 10.0),
                             p->block_size == 0 ? 16 : p->block_size, 2);
    case ALWAN_DENOISE_WAVELET:
        return alwan_wv_run(out, out_row_stride, src, src_row_stride, channels, width, height, p, 2);
    case ALWAN_DENOISE_ANISOTROPIC_DIFFUSION:
        return alwan_ad_run(out, out_row_stride, src, src_row_stride, channels, width, height, alwan_dn_or(p->alpha, 0.15),
                            alwan_dn_or(p->k, 0.05), p->iterations == 0 ? 10 : p->iterations);
    default:
        return ALWAN_E_INVALID;
    }
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_denoise_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                               size_t channels, size_t width, size_t height, alwan_denoise_method method,
                               alwan_denoise_params const *params) {
    return alwan_dn_float(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_denoise_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                               size_t channels, size_t width, size_t height, alwan_denoise_method method,
                               alwan_denoise_params const *params) {
    return alwan_dn_float(out, out_row_stride, src, src_row_stride, channels, width, height, method, params, 1);
}
#endif
