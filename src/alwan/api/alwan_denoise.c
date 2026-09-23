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

alwan_status alwan_anisotropic_diffusion_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src,
                                            size_t src_row_stride, size_t channels, size_t width, size_t height,
                                            double alpha, double k, size_t iterations) {
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

alwan_status alwan_denoise_nl_means_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src,
                                       size_t src_row_stride, size_t channels, size_t width, size_t height, double h,
                                       size_t template_window, size_t search_window) {
    return alwan_nlm_run(out, out_row_stride, src, src_row_stride, channels, width, height, h, template_window,
                         search_window);
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_denoise_tv_chambolle_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src,
                                            size_t src_row_stride, size_t channels, size_t width, size_t height,
                                            alwan_f64 weight, alwan_f64 eps, size_t max_iterations) {
    return alwan_tv_run(out, out_row_stride, src, src_row_stride, channels, width, height, (double)weight, (double)eps,
                        max_iterations, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_denoise_tv_chambolle_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src,
                                            size_t src_row_stride, size_t channels, size_t width, size_t height,
                                            alwan_f32 weight, alwan_f32 eps, size_t max_iterations) {
    return alwan_tv_run(out, out_row_stride, src, src_row_stride, channels, width, height, (double)weight, (double)eps,
                        max_iterations, 1);
}
#endif
